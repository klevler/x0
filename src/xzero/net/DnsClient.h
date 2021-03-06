// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/net/IPAddress.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

namespace xzero {

/**
 * DNS client API.
 */
class XZERO_BASE_API DnsClient {
 public:
  /** Retrieves all IPv4 addresses for given DNS name.
   *
   * @throw if none found or an error occurred.
   */
  const std::vector<IPAddress>& ipv4(const std::string& name);

  /** Retrieves all IPv6 addresses for given DNS name.
   *
   * @throw RuntimeError if none found or an error occurred.
   */
  const std::vector<IPAddress>& ipv6(const std::string& name);

  /** Retrieves all IPv4 and IPv6 addresses for given DNS name.
   *
   * @throw RuntimeError if none found or an error occurred.
   */
  std::vector<IPAddress> ip(const std::string& name);

  /** Retrieves all TXT records for given DNS name.
   *
   * @throw RuntimeError if none found or an error occurred.
   */
  std::vector<std::string> txt(const std::string& name);

  /** Retrieves all MX records for given DNS name.
   *
   * @throw RuntimeError if none found or an error occurred.
   */
  std::vector<std::pair<int, std::string>> mx(const std::string& name);

  /**
   * Retrieves the Resource Record (DNS name) of an IP address.
   */
  std::string rr(const IPAddress& ip);

  void clearIPv4();
  void clearIPv6();
  void clearIP();
  void clearTXT();
  void clearMX();
  void clearRR();

 private:
  template<typename InetType, const int AddressFamilty>
  static const std::vector<IPAddress>& lookupIP(
      const std::string& name,
      std::unordered_map<std::string, std::vector<IPAddress>>* cache,
      std::mutex* cacheMutex);

 private:
  std::unordered_map<std::string, std::vector<IPAddress>> ipv4_;
  std::mutex ipv4Mutex_;

  std::unordered_map<std::string, std::vector<IPAddress>> ipv6_;
  std::mutex ipv6Mutex_;
};

}  // namespace xzero
