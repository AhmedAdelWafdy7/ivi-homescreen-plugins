/*
 * Copyright 2020-2024 Toyota Connected North America
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PLUGINS_FLATPAK_CACHE_CACHE_MANAGER_H
#define PLUGINS_FLATPAK_CACHE_CACHE_MANAGER_H

#include <flutter/encodable_value.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
#include "../messages.g.h"
#include "cache_config.h"
#include "interfaces/cache_observer.h"
#include "interfaces/cache_storage.h"
#include "interfaces/network_fetcher.h"
#include "operations/cache_operation_template.h"

namespace flatpak_plugin {

/**
 * @class CacheManager
 * @brief Manages caching operations for Flatpak-related data, including
 * storage, fetching, and cleanup.
 */
class CacheManager {
 private:
  std::unique_ptr<ICacheStorage> storage_;
  std::unique_ptr<INetworkFetcher> network_fetcher_;
  std::vector<std::unique_ptr<ICacheObserver>> observers_;
  CacheConfig config_;
  mutable std::shared_mutex cache_mutex_;
  bool is_initialized_ = false;
  std::thread cleanup_thread_;
  std::atomic<bool> stop_cleanup_{false};
  std::condition_variable cleanup_cv_;
  std::mutex cleanup_mutex_;

  mutable CacheMetrics metrics_;

  std::string GenerateKey(const std::string& base_key,
                          const std::vector<std::string>& params = {});

  void NotifyObservers(
      const std::function<void(ICacheObserver*)>& notification);

  void CleanupWorker();

  template <typename T>
  std::optional<T> PerformCacheOperation(
      const std::string& key,
      std::function<std::optional<T>()> network_operation,
      CacheOperationTemplate<T>* cache_operation);

  template <typename T>
  std::optional<T> TryNetworkOperation(
      const std::string& key,
      std::function<std::optional<T>()> network_operation);

  template <typename T>
  std::optional<T> TryNetworkAndCache(
      const std::string& key,
      std::function<std::optional<T>()> network_operation,
      CacheOperationTemplate<T>* cache_operation);

  static std::string SerializeInstallation(
      const flatpak_plugin::Installation& installation);

  static flatpak_plugin::Installation DeserializeInstallation(
      const std::string& data);

  static std::string SerializeEncodableList(flutter::EncodableValue list);

  static flutter::EncodableList DeserializeEncodableList(
      const std::string& data);

  flutter::EncodableList ConvertApplicationsToEncodableList(
      const flutter::EncodableList& apps);

  struct AppCacheOperation : CacheOperationTemplate<flutter::EncodableList> {
    CacheManager* manager;

    AppCacheOperation(CacheManager* manager) : manager(manager) {}

   protected:
    bool ValidateKey(const std::string& key) override { return !key.empty(); }
    std::string SerializeData(const flutter::EncodableList& data) override {
      try {
        flutter::EncodableValue encodable_value;
        encodable_value = flutter::EncodableList(data);
        return manager->SerializeEncodableList(encodable_value);
      } catch (const std::exception& e) {
        spdlog::error("Error serializing data: {}", e.what());
        return "";
      }
    }

    std::optional<flutter::EncodableList> DeserializeData(
        const std::string& serialized_data) override {
      if (serialized_data.empty()) {
        return std::nullopt;
      }
      try {
        return manager->DeserializeEncodableList(serialized_data);
      } catch (const std::exception& e) {
        spdlog::error("Error deserializing data: {}", e.what());
        return std::nullopt;
      }
    }

    std::chrono::system_clock::time_point GetExpiryTime() override {
      return std::chrono::system_clock::now() + manager->config_.default_ttl;
    }

    bool ValidateData(const flutter::EncodableList& data) override {
      return true;
    }
  };

  struct InstallationCacheOperation
      : CacheOperationTemplate<flatpak_plugin::Installation> {
    CacheManager* manager;
    InstallationCacheOperation(CacheManager* manager) : manager(manager) {}

   protected:
    bool ValidateKey(const std::string& key) override { return !key.empty(); }

    std::string SerializeData(
        const flatpak_plugin::Installation& data) override {
      try {
        return manager->SerializeInstallation(data);
      } catch (const std::exception& e) {
        spdlog::error("Error serializing data: {}", e.what());
        return "";
      }
    }

    std::optional<flatpak_plugin::Installation> DeserializeData(
        const std::string& data) override {
      if (data.empty()) {
        return std::nullopt;
      }
      try {
        return manager->DeserializeInstallation(data);
      } catch (const std::exception& e) {
        spdlog::error("Error deserializing data: {}", e.what());
        return std::nullopt;
      }
    }

    std::chrono::system_clock::time_point GetExpiryTime() override {
      return std::chrono::system_clock::now() + manager->config_.default_ttl;
    }

    bool ValidateData(const flatpak_plugin::Installation& data) override {
      return !data.id().empty();
    }
  };

 public:
  class Builder {
   private:
    CacheConfig config_;

   public:
    Builder& WithDatabasePath(const std::string& path) {
      config_.db_path = path;
      return *this;
    }

    Builder& WithDefaultTTL(std::chrono::seconds ttl) {
      config_.default_ttl = ttl;
      return *this;
    }

    Builder& WithCachePolicy(CachePolicy policy) {
      config_.policy = policy;
      return *this;
    }

    Builder& WithCompression(bool enable = true) {
      config_.enable_compression = enable;
      return *this;
    }

    Builder& WithMaxCacheSize(size_t size_mb) {
      config_.max_cache_size_mb = size_mb;
      return *this;
    }

    Builder& WithNetworkTimeout(std::chrono::seconds timeout) {
      config_.network_timeout = timeout;
      return *this;
    }

    Builder& WithMaxRetries(int retries) {
      config_.max_retries = retries;
      return *this;
    }

    Builder& WithAutoCleanup(
        bool enable,
        std::chrono::minutes interval = std::chrono::minutes(60)) {
      config_.enable_auto_cleanup = enable;
      config_.cleanup_interval = interval;
      return *this;
    }

    Builder& WithMetrics(bool enable = true) {
      config_.enable_metrics = enable;
      return *this;
    }

    std::unique_ptr<CacheManager> Build();
  };

  explicit CacheManager(const CacheConfig& config);

  CacheManager(const CacheConfig& config,
               std::unique_ptr<ICacheStorage> storage,
               std::unique_ptr<INetworkFetcher> fetcher);

  ~CacheManager();

  bool Initialize();

  void AddObserver(std::unique_ptr<ICacheObserver> observer);

  void SetBearerToken(const std::string& token);

  // Flatpak-specific cache operations

  /**
   * @brief Retrieves the list of installed applications.
   * @param force_refresh If true, forces a refresh of the installed
   * applications cache. Defaults to false.
   * @return std::optional<flutter::EncodableList> Optional list of installed
   * applications.
   */
  std::optional<flutter::EncodableList> GetApplicationsInstalled(
      bool force_refresh = false);

  /**
   * @brief Retrieves the list of applications from a specified remote source.
   * @param remote_id The identifier of the remote source to query for
   * applications.
   * @param force_refresh If true, forces a refresh from the remote source
   * instead of using cached data. Defaults to false.
   * @return An optional EncodableList containing the applications retrieved
   * from the remote. Returns std::nullopt if no applications are found or an
   * error occurs.
   */
  std::optional<flutter::EncodableList> GetApplicationsRemote(
      const std::string& remote_id,
      bool force_refresh = false);

  /**
   * @brief Retrieves the user installation information.
   * @param force_refresh If true, forces a refresh of the installation
   * information. Defaults to false.
   * @return std::optional<Installation> The user's installation information, or
   * std::nullopt if not available.
   */
  std::optional<Installation> GetUserInstallation(bool force_refresh = false);

  /**
   * @brief Retrieves the list of system installations.
   * @param force_refresh If true, forces a refresh of the cached installations.
   * @return std::optional<flutter::EncodableList> An optional list of system
   * installations. If no installations are found or an error occurs, the
   * optional will be empty.
   */
  std::optional<flutter::EncodableList> GetSystemInstallations(
      bool force_refresh = false);

  /**
   * @brief Retrieves the list of remotes for a given installation.
   * @param installation_id The identifier of the installation to query remotes
   * for.
   * @param force_refresh If true, forces a refresh of the remotes from the
   * source. Defaults to false.
   * @return std::optional<flutter::EncodableList> An optional list of remotes.
   * If no remotes are found or an error occurs, std::nullopt is returned.
   */
  std::optional<flutter::EncodableList> GetRemotes(
      const std::string& installation_id,
      bool force_refresh = false);

  // Cache management operations

  /**
   * @brief Invalidate all cache entries
   */
  void InvalidateAll();

  /**
   * @brief Invalidate specific cache key
   * @param key Cache key to invalidate
   */
  void InvalidateKey(const std::string& key);

  /**
   * @brief Check if cache is healthy
   * @return true if cache is functioning properly
   */
  bool IsHealthy() const;

  /**
   * @brief Get current cache size
   * @return Cache size in bytes
   */
  size_t GetCacheSize() const;

  /**
   * @brief Set cache policy
   * @param policy New cache policy
   */
  void SetCachePolicy(CachePolicy policy);

  /**
   * @brief Get current cache policy
   * @return Current cache policy
   */
  CachePolicy GetCachePolicy() const;

  /**
   * @brief Force cleanup of expired entries
   * @return Number of entries cleaned
   */
  size_t ForceCleanup();

  /**
   * @brief Get cache metrics
   * @return Current cache metrics
   */
  const CacheMetrics& GetMetrics() const { return metrics_; }

  /**
   * @brief Get cache configuration
   * @return Current cache configuration
   */
  const CacheConfig& GetConfig() const;

 public:
  /**
   * @brief Export cache to file
   * @param filepath Path to export file
   * @return true if export successful
   */
  bool ExportCache(const std::string& filepath);

  /**
   * @brief Import cache from file
   * @param filepath Path to import file
   * @return true if import successful
   */
  bool ImportCache(const std::string& filepath);

  CacheMetrics* GetMetricsPtr() const { return &metrics_; }
};

}  // namespace flatpak_plugin

#endif  // PLUGINS_FLATPAK_CACHE_CACHE_MANAGER_H