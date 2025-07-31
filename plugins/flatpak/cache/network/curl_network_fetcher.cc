#include "curl_network_fetcher.h"
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <thread>

CurlNetworkFetcher::CurlNetworkFetcher(std::chrono::seconds timeout,
                                       int max_retries)
    : timeout_(timeout), max_retries_(max_retries) {
  curl_client_ = std::make_unique<plugin_common_curl::CurlClient>();
}

CurlNetworkFetcher::~CurlNetworkFetcher() = default;

template <typename Operation>
std::optional<std::string> CurlNetworkFetcher::PerformWithRetry(
    Operation&& operation) {
  int attempts = 0;

  while (attempts <= max_retries_) {
    try {
      auto result = operation();
      if (result.has_value()) {
        return result;
      }

      long response_code = last_response_code_.load();
      if (response_code >= 200 && response_code < 300) {
        // success
        return std::nullopt;
      }

      if (response_code >= 500 || response_code == 408 ||
          response_code == 429) {
        attempts++;
        if (attempts <= max_retries_) {
          // exponential backoff
          std::this_thread::sleep_for(
              std::chrono::seconds(1 << (attempts - 1)));
          continue;
        } else {
          return std::nullopt;
        }
      }
    } catch (std::exception& e) {
      spdlog::error("Network operation failed: {}", e.what());
      attempts++;
      spdlog::warn("Retrying network operation (attempt {})", attempts);
      if (attempts <= max_retries_) {
        // exponential backoff
        std::this_thread::sleep_for(std::chrono::seconds(1 << (attempts - 1)));
        continue;
      }
      break;
    }
  }
  return std::nullopt;
}

std::optional<std::string> CurlNetworkFetcher::Fetch(
    const std::string& url,
    const std::vector<std::string>& headers) {
  auto operation = [this, &url, &headers]() -> std::optional<std::string> {
    try {
      std::vector<std::string> processed_headers;
      ProcessHeaders(headers, processed_headers);

      auto response = curl_client_->Get(url);
      last_response_code_.store(curl_client_->GetHttpCode());

      long response_code = last_response_code_.load();
      if (response_code >= 200 && response_code < 300) {
        // success
        return response;
      }

      return std::nullopt;
    } catch (const std::exception& e) {
      spdlog::error("Network operation failed: {}", e.what());
      return std::nullopt;
    }
  };

  return PerformWithRetry(std::move(operation));
}

std::optional<std::string> CurlNetworkFetcher::Post(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& form_data,
    const std::vector<std::string>& headers) {
  auto operation = [this, &url, &form_data,
                    &headers]() -> std::optional<std::string> {
    try {
      std::vector<std::string> processed_headers;
      ProcessHeaders(headers, processed_headers);

      auto response = curl_client_->Post(url, form_data, headers);
      last_response_code_.store(curl_client_->GetHttpCode());

      long response_code = last_response_code_.load();
      if (response_code >= 200 && response_code < 300) {
        // success
        return response;
      }

      return std::nullopt;
    } catch (const std::exception& e) {
      spdlog::error("Network operation failed: {}", e.what());
      return std::nullopt;
    }
  };

  return PerformWithRetry(std::move(operation));
}

bool CurlNetworkFetcher::IsNetworkAvailable() {
  try {
    auto result = curl_client_->Get("https://www.google.com");
    long response_code = curl_client_->GetHttpCode();
    last_response_code_.store(response_code);

    return response_code > 0;
  } catch (const std::exception& e) {
    spdlog::error("Network operation failed: {}", e.what());
    return false;
  }
}

long CurlNetworkFetcher::GetLastResponseCode() {
  return last_response_code_.load();
}

void CurlNetworkFetcher::SetBearerToken(const std::string& token) {
  curl_client_->SetBearerToken(token);
}

std::optional<flutter::EncodableList> CurlNetworkFetcher::FetchRemotes(
    const std::string& installation_id) {
  std::unique_ptr<flatpak_plugin::FlatpakPlugin> plugin_;
  try {
    plugin_ = std::make_unique<flatpak_plugin::FlatpakPlugin>();

    // Get installation
    auto installation_result =
        plugin_->GetRemotesByInstallationId(installation_id);
    if (installation_result.has_error()) {
      spdlog::error("[Network Fetcher] Error fetching remotes : {}",
                    installation_result.error().message());
      return std::nullopt;
    }

    return installation_result.value();
  } catch (const std::exception& e) {
    spdlog::error("Network operation failed: {}", e.what());
    return std::nullopt;
  }
}

void CurlNetworkFetcher::ProcessHeaders(
    const std::vector<std::string>& headers,
    std::vector<std::string>& non_auth_headers) {
  non_auth_headers.clear();

  for (const auto& header : headers) {
    if (header.find("Authorization: Bearer ") == 0) {
      std::string token = header.substr(21);
      curl_client_->SetBearerToken(token);
    } else if (header.find("Authorization: ") == 0) {
      non_auth_headers.push_back(header);
    } else {
      non_auth_headers.push_back(header);
    }
  }
}