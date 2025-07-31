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

#ifndef PLUGINS_FLATPAK_CACHE_NETWORK_FETCHER_H
#define PLUGINS_FLATPAK_CACHE_NETWORK_FETCHER_H

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "../../../common/curl_client/curl_client.h"

/**
 * @brief Network Fetcher Strategy interface
 *
 * Abstracts network operations to allow different implementations
 * (Curl, system network calls, mock for testing, etc.)
 */
class INetworkFetcher {
 public:
  virtual ~INetworkFetcher() = default;

  /**
   * @brief Performs an HTTP GET request to fetch data from a URL
   *
   * @param url The target URL to fetch data from
   * @param headers Optional HTTP headers to include in the request
   * @return std::optional<std::string> The response body if successful,
   * std::nullopt if failed
   */
  virtual std::optional<std::string> Fetch(
      const std::string& url,
      const std::vector<std::string>& headers = {}) = 0;

  /**
   * @brief Performs an HTTP POST request with form data
   *
   * @param url The target URL to send the POST request to
   * @param form_data Key-value pairs representing form data to be sent
   * @param headers Optional HTTP headers to include in the request
   * @return std::optional<std::string> The response body if successful,
   * std::nullopt if failed
   */
  virtual std::optional<std::string> Post(
      const std::string& url,
      const std::vector<std::pair<std::string, std::string>>& form_data,
      const std::vector<std::string>& headers = {}) = 0;

  /**
   * @brief Checks if network connectivity is available
   *
   * @return bool True if network is available, false otherwise
   */
  virtual bool IsNetworkAvailable() = 0;

  /**
   * @brief Retrieves the HTTP response code from the last network operation
   *
   * @return long The HTTP status code (e.g., 200, 404, 500) from the most
   * recent request
   */
  virtual long GetLastResponseCode() = 0;

  /**
   * @brief Sets the bearer token for HTTP authentication.
   * @param token The bearer token string to be used for authentication.
   *              An empty string will clear any previously set token.
   */
  virtual void SetBearerToken(const std::string& token) = 0;

  /**
   * @brief Interface function that fetches remotes over the network.
   * @param installation_id id of the installation in remote
   * @return Encodablelist contains Remote data of installation
   */
  virtual std::optional<flutter::EncodableList> FetchRemotes(
      const std::string& installation_id) = 0;
};

#endif  // PLUGINS_FLATPAK_CACHE_NETWORK_FETCHER_H