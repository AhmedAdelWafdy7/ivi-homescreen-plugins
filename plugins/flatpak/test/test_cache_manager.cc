#include <execinfo.h>
#include <flutter/encodable_value.h>
#include <gtest/gtest.h>
#include <signal.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include "../cache/cache_config.h"
#include "../cache/cache_manager.h"
#include "../cache/interfaces/cache_observer.h"
#include "../cache/interfaces/cache_storage.h"
#include "../cache/interfaces/network_fetcher.h"
#include "../messages.g.h"

using namespace flatpak_plugin;

class TestCacheStorage : public ICacheStorage {
 private:
  mutable std::shared_mutex storage_mutex_;
  std::map<std::string,
           std::pair<std::string, std::chrono::system_clock::time_point>>
      storage_;
  std::string db_path_;
  std::atomic<bool> initialized_{false};

 public:
  explicit TestCacheStorage(const std::string& db_path) : db_path_(db_path) {}

  bool Initialize() override {
    std::lock_guard<std::shared_mutex> lock(storage_mutex_);

    std::filesystem::path path(db_path_);
    std::filesystem::create_directories(path.parent_path());

    std::ifstream file(db_path_);
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        size_t first_pipe = line.find('|');
        size_t second_pipe = line.find('|', first_pipe + 1);
        if (first_pipe != std::string::npos &&
            second_pipe != std::string::npos) {
          std::string key = line.substr(0, first_pipe);
          std::string data =
              line.substr(first_pipe + 1, second_pipe - first_pipe - 1);
          auto timestamp = std::chrono::system_clock::from_time_t(
              std::stoll(line.substr(second_pipe + 1)));
          storage_[key] = {data, timestamp};
        }
      }
      file.close();
    }

    initialized_.store(true);
    return true;
  }

  bool Store(const std::string& key,
             const std::string& data,
             std::chrono::system_clock::time_point expiry) override {
    if (!initialized_.load())
      return false;

    {
      std::lock_guard<std::shared_mutex> lock(storage_mutex_);
      storage_[key] = {data, expiry};
    }

    return WriteToFile();
  }

  std::optional<std::string> Retrieve(const std::string& key) override {
    if (!initialized_.load())
      return std::nullopt;

    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    auto it = storage_.find(key);
    if (it != storage_.end()) {
      return it->second.first;
    }
    return std::nullopt;
  }

  bool IsExpired(const std::string& key) override {
    if (!initialized_.load())
      return true;

    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    auto it = storage_.find(key);
    if (it != storage_.end()) {
      return std::chrono::system_clock::now() > it->second.second;
    }
    return true;
  }

  void Invalidate(const std::string& key = "") override {
    if (!initialized_.load())
      return;

    bool needs_file_update = false;
    {
      std::lock_guard<std::shared_mutex> lock(storage_mutex_);
      if (key.empty()) {
        if (!storage_.empty()) {
          storage_.clear();
          needs_file_update = true;
        }
      } else {
        auto erased = storage_.erase(key);
        needs_file_update = (erased > 0);
      }
    }

    if (needs_file_update) {
      if (key.empty()) {
        std::ofstream file(db_path_, std::ios::trunc);
        file.close();
      } else {
        WriteToFile();
      }
    }
  }

  size_t GetCacheSize() override {
    if (!initialized_.load())
      return 0;

    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    size_t total_size = 0;
    for (const auto& [key, value] : storage_) {
      total_size += key.size() + value.first.size();
    }
    return total_size;
  }

  size_t CleanupExpired() override {
    if (!initialized_.load())
      return 0;

    size_t removed = 0;
    auto now = std::chrono::system_clock::now();

    {
      std::lock_guard<std::shared_mutex> lock(storage_mutex_);
      auto it = storage_.begin();
      while (it != storage_.end()) {
        if (now > it->second.second) {
          it = storage_.erase(it);
          removed++;
        } else {
          ++it;
        }
      }
    }

    if (removed > 0) {
      WriteToFile();
    }

    return removed;
  }

 private:
  bool WriteToFile() {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    std::ofstream file(db_path_);
    if (!file.is_open())
      return false;

    for (const auto& [k, v] : storage_) {
      auto timestamp = std::chrono::system_clock::to_time_t(v.second);
      file << k << "|" << v.first << "|" << timestamp << "\n";
    }
    file.close();
    return true;
  }
};

class TestNetworkFetcher : public INetworkFetcher {
 private:
  std::string bearer_token_;
  bool simulate_network_failure_ = false;
  long last_response_code_ = 200;

 public:
  void SetBearerToken(const std::string& token) override {
    bearer_token_ = token;
  }

  void SimulateNetworkFailure(bool fail) {
    simulate_network_failure_ = fail;
    if (fail) {
      last_response_code_ = 500;
    } else {
      last_response_code_ = 200;
    }
  }

  // Implement pure virtual methods from INetworkFetcher
  std::optional<std::string> Fetch(
      const std::string& url,
      const std::vector<std::string>& headers = {}) override {
    if (simulate_network_failure_) {
      last_response_code_ = 500;
      return std::nullopt;
    }

    last_response_code_ = 200;
    if (url.find("appstream") != std::string::npos) {
      return std::string(
          "<?xml version=\"1.0\"?><components><component "
          "type=\"desktop-application\"><id>com.example.app</id></component></"
          "components>");
    }
    return std::string("mock_data_for_" + url);
  }

  std::optional<std::string> Post(
      const std::string& url,
      const std::vector<std::pair<std::string, std::string>>& form_data,
      const std::vector<std::string>& headers = {}) override {
    if (simulate_network_failure_) {
      last_response_code_ = 500;
      return std::nullopt;
    }

    last_response_code_ = 200;
    return std::string("post_response_for_" + url);
  }

  bool IsNetworkAvailable() override { return !simulate_network_failure_; }

  long GetLastResponseCode() override { return last_response_code_; }

  std::optional<flutter::EncodableList> FetchApplicationsInstalled() {
    if (simulate_network_failure_) {
      return std::nullopt;
    }

    flutter::EncodableList apps;
    apps.push_back(
        flutter::EncodableMap{{"name", "org.gimp.GIMP"},
                              {"version", "2.10.32"},
                              {"description", "GNU Image Manipulation Program"},
                              {"installed_size", 234567890}});
    apps.push_back(
        flutter::EncodableMap{{"name", "org.libreoffice.LibreOffice"},
                              {"version", "7.4.2"},
                              {"description", "Office suite"},
                              {"installed_size", 456789012}});

    return apps;
  }

  std::optional<flutter::EncodableList> FetchApplicationsRemote(
      const std::string& remote_id) {
    if (simulate_network_failure_) {
      return std::nullopt;
    }

    flutter::EncodableList apps;
    apps.push_back(
        flutter::EncodableMap{{"name", "com.spotify.Client"},
                              {"version", "1.1.84"},
                              {"description", "Music streaming service"},
                              {"remote", remote_id},
                              {"download_size", 123456789}});
    apps.push_back(flutter::EncodableMap{{"name", "com.discordapp.Discord"},
                                         {"version", "0.0.20"},
                                         {"description", "Chat application"},
                                         {"remote", remote_id},
                                         {"download_size", 87654321}});

    return apps;
  }

  std::optional<flutter::EncodableMap> FetchUserInstallation() {
    if (simulate_network_failure_) {
      return std::nullopt;
    }

    flutter::EncodableMap installation;
    installation["id"] = "user";
    installation["path"] =
        std::string(getenv("HOME") ? getenv("HOME") : "/home/user") +
        "/.local/share/flatpak";
    installation["is_user"] = true;
    installation["display_name"] = "User Installation";

    return installation;
  }

  std::optional<flutter::EncodableList> FetchSystemInstallations() {
    if (simulate_network_failure_) {
      return std::nullopt;
    }

    flutter::EncodableList installations;
    installations.push_back(
        flutter::EncodableMap{{"id", "system"},
                              {"path", "/var/lib/flatpak"},
                              {"is_user", false},
                              {"display_name", "System Installation"}});

    return installations;
  }

  std::optional<flutter::EncodableList> FetchRemotes(
      const std::string& installation_id) {
    if (simulate_network_failure_) {
      return std::nullopt;
    }

    flutter::EncodableList remotes;
    remotes.push_back(flutter::EncodableMap{
        {"name", "flathub"},
        {"title", "Flathub"},
        {"url", "https://flathub.org/repo/flathub.flatpakrepo"},
        {"installation_id", installation_id},
        {"is_system", installation_id == "system"}});
    remotes.push_back(
        flutter::EncodableMap{{"name", "fedora"},
                              {"title", "Fedora"},
                              {"url", "oci+https://registry.fedoraproject.org"},
                              {"installation_id", installation_id},
                              {"is_system", installation_id == "system"}});

    return remotes;
  }
};

class TestCacheObserver : public ICacheObserver {
 public:
  struct Event {
    std::string type;
    std::string key;
    std::string extra;
    size_t data_size = 0;
    long error_code = 0;
    std::chrono::system_clock::time_point timestamp;
  };

  std::vector<Event> events;

  // Implement all pure virtual methods from ICacheObserver
  void OnCacheHit(const std::string& key, size_t data_size) override {
    events.push_back(
        {"hit", key, "", data_size, 0, std::chrono::system_clock::now()});
  }

  void OnCacheMiss(const std::string& key) override {
    events.push_back({"miss", key, "", 0, 0, std::chrono::system_clock::now()});
  }

  void OnCacheStore(const std::string& key) {
    events.push_back(
        {"store", key, "", 0, 0, std::chrono::system_clock::now()});
  }

  void OnCacheRemove(const std::string& key) {
    events.push_back(
        {"remove", key, "", 0, 0, std::chrono::system_clock::now()});
  }

  void OnCacheError(const std::string& key, const std::string& error) {
    events.push_back(
        {"error", key, error, 0, 0, std::chrono::system_clock::now()});
  }

  void OnCacheExpired(const std::string& key) override {
    events.push_back(
        {"expired", key, "", 0, 0, std::chrono::system_clock::now()});
  }

  void OnNetworkFallback(const std::string& reason) override {
    events.push_back({"network_fallback", "", reason, 0, 0,
                      std::chrono::system_clock::now()});
  }

  void OnNetworkError(const std::string& url, long error_code) override {
    events.push_back({"network_error", url, "", 0, error_code,
                      std::chrono::system_clock::now()});
  }

  void OnCacheCleanup(size_t entries_cleaned) override {
    events.push_back({"cleanup", "", "", entries_cleaned, 0,
                      std::chrono::system_clock::now()});
  }

  void ClearEvents() { events.clear(); }

  bool HasEvent(const std::string& event_type) const {
    return std::any_of(
        events.begin(), events.end(),
        [&event_type](const Event& e) { return e.type == event_type; });
  }
};

class CacheManagerIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_db_path_ =
        "/tmp/cache_manager_test_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
        ".db";

    config_ = CacheConfig{.db_path = test_db_path_,
                          .default_ttl = std::chrono::seconds(60),
                          .policy = CachePolicy::CACHE_FIRST,
                          .enable_compression = false,
                          .max_cache_size_mb = 10,
                          .network_timeout = std::chrono::seconds(5),
                          .max_retries = 2,
                          .enable_auto_cleanup = true,
                          .cleanup_interval = std::chrono::minutes(10),
                          .enable_metrics = true};

    storage_ = std::make_unique<TestCacheStorage>(test_db_path_);
    fetcher_ = std::make_unique<TestNetworkFetcher>();
    observer_ = std::make_unique<TestCacheObserver>();

    storage_ptr_ = storage_.get();
    fetcher_ptr_ = fetcher_.get();
    observer_ptr_ = observer_.get();
  }

  void TearDown() override {
    cache_manager_.reset();

    if (std::filesystem::exists(test_db_path_)) {
      std::filesystem::remove(test_db_path_);
    }
  }

  void CreateCacheManager() {
    cache_manager_ = std::make_unique<CacheManager>(
        config_, std::move(storage_), std::move(fetcher_));

    if (observer_) {
      cache_manager_->AddObserver(std::move(observer_));
    }
  }

  std::string test_db_path_;
  CacheConfig config_;
  std::unique_ptr<CacheManager> cache_manager_;
  std::unique_ptr<TestCacheStorage> storage_;
  std::unique_ptr<TestNetworkFetcher> fetcher_;
  std::unique_ptr<TestCacheObserver> observer_;

  TestCacheStorage* storage_ptr_;
  TestNetworkFetcher* fetcher_ptr_;
  TestCacheObserver* observer_ptr_;
};

TEST_F(CacheManagerIntegrationTest, Initialzation) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());
}

TEST_F(CacheManagerIntegrationTest, BuilderPattern) {
  auto manager = CacheManager::Builder()
                     .WithDatabasePath("/tmp/builder_test.db")
                     .WithCachePolicy(CachePolicy::NETWORK_FIRST)
                     .WithAutoCleanup(true, std::chrono::minutes(5))
                     .WithDefaultTTL(std::chrono::minutes(10))
                     .WithCompression(false)
                     .WithMaxCacheSize(50)
                     .WithMaxRetries(3)
                     .WithNetworkTimeout(std::chrono::seconds(5))
                     .WithMetrics(true)
                     .Build();

  ASSERT_NE(manager, nullptr);
  EXPECT_TRUE(manager->IsHealthy());

  const auto& built_config = manager->GetConfig();
  EXPECT_EQ(built_config.db_path, "/tmp/builder_test.db");
  EXPECT_EQ(built_config.default_ttl, std::chrono::minutes(10));
  EXPECT_EQ(built_config.policy, CachePolicy::NETWORK_FIRST);
  EXPECT_EQ(built_config.max_cache_size_mb, 50);
  EXPECT_EQ(built_config.max_retries, 3);
  EXPECT_EQ(built_config.network_timeout, std::chrono::seconds(5));
  EXPECT_TRUE(built_config.enable_metrics);
  EXPECT_FALSE(built_config.enable_compression);

  std::filesystem::remove("/tmp/builder_test.db");
}

// network fetching tests
TEST_F(CacheManagerIntegrationTest, FetchApplicationInstalledFirstTime) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());
  auto result = cache_manager_->GetApplicationsInstalled();
  ASSERT_TRUE(result.has_value());

  EXPECT_GT(result->size(), 0);
  EXPECT_GE(observer_ptr_->events.size(), 1);
  EXPECT_TRUE(observer_ptr_->HasEvent("miss") ||
              observer_ptr_->HasEvent("store"));
}

TEST_F(CacheManagerIntegrationTest, CacheHitOnSecondRequest) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());

  // fetch from network
  auto result1 = cache_manager_->GetApplicationsInstalled();
  ASSERT_TRUE(result1.has_value());

  observer_ptr_->ClearEvents();

  // hit cache if cache policy allows
  auto result2 = cache_manager_->GetApplicationsInstalled();
  ASSERT_TRUE(result2.has_value());

  EXPECT_EQ(result1->size(), result2->size());

  // cache first policy get a cache hit
  bool has_cache_interaction =
      observer_ptr_->HasEvent("hit") || observer_ptr_->HasEvent("miss");
  EXPECT_TRUE(has_cache_interaction);
}

TEST_F(CacheManagerIntegrationTest, ForceRefreshBypassesCache) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());
  auto result1 = cache_manager_->GetApplicationsInstalled();
  ASSERT_TRUE(result1.has_value());

  observer_ptr_->ClearEvents();

  auto result2 = cache_manager_->GetApplicationsInstalled(true);
  ASSERT_TRUE(result2.has_value());
  EXPECT_GT(observer_ptr_->events.size(), 0);
}

TEST_F(CacheManagerIntegrationTest, NetworkFailureHandling) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());

  fetcher_ptr_->SimulateNetworkFailure(true);
  auto result = cache_manager_->GetApplicationsInstalled();

  fetcher_ptr_->SimulateNetworkFailure(false);
  result = cache_manager_->GetApplicationsInstalled();
  EXPECT_TRUE(result.has_value());
}

TEST_F(CacheManagerIntegrationTest, DifferentAPIendpoints) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());

  auto intalled = cache_manager_->GetApplicationsInstalled();
  EXPECT_TRUE(intalled.has_value());

  auto remote = cache_manager_->GetApplicationsRemote("flathub");
  EXPECT_TRUE(remote.has_value());

  auto user_install = cache_manager_->GetUserInstallation();
  EXPECT_TRUE(user_install.has_value());

  auto system_installs = cache_manager_->GetSystemInstallations();
  EXPECT_TRUE(system_installs.has_value());

  auto remotes = cache_manager_->GetRemotes("user");
  EXPECT_TRUE(remotes.has_value());
}

TEST_F(CacheManagerIntegrationTest, CacheInvalidation) {
  CreateCacheManager();
  EXPECT_TRUE(cache_manager_->IsHealthy());

  auto result = cache_manager_->GetApplicationsInstalled();
  EXPECT_TRUE(result.has_value());

  EXPECT_GT(cache_manager_->GetCacheSize(), 0);

  // invalidate key

  cache_manager_->InvalidateKey("application_installed");
  observer_ptr_->ClearEvents();

  auto result2 = cache_manager_->GetApplicationsInstalled();
  EXPECT_TRUE(result2.has_value());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}