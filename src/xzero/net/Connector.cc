// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/Connector.h>
#include <xzero/net/ConnectionFactory.h>
#include <xzero/net/Server.h>
#include <xzero/StringUtil.h>
#include <algorithm>
#include <cassert>

namespace xzero {

Connector::Connector(const std::string& name, Executor* executor)
  : name_(name),
    executor_(executor),
    connectionFactories_(),
    defaultConnectionFactory_(),
    listeners_() {
}

Connector::~Connector() {
}

const std::string& Connector::name() const {
  return name_;
}

void Connector::setName(const std::string& name) {
  name_ = name;
}

std::shared_ptr<ConnectionFactory> Connector::addConnectionFactory(
    std::shared_ptr<ConnectionFactory> factory) {

  assert(factory.get() != nullptr);

  connectionFactories_[factory->protocolName()] = factory;

  if (connectionFactories_.size() == 1) {
    defaultConnectionFactory_ = factory;
  }

  return factory;
}

std::shared_ptr<ConnectionFactory> Connector::connectionFactory(const std::string& protocolName) const {
  auto i = connectionFactories_.find(protocolName);
  if (i != connectionFactories_.end()) {
    return i->second;
  }
  return nullptr;
}

std::list<std::shared_ptr<ConnectionFactory>> Connector::connectionFactories() const {
  std::list<std::shared_ptr<ConnectionFactory>> result;
  for (auto& entry: connectionFactories_) {
    result.push_back(entry.second);
  }
  return result;
}

void Connector::setDefaultConnectionFactory(std::shared_ptr<ConnectionFactory> factory) {
  auto i = connectionFactories_.find(factory->protocolName());
  if (i == connectionFactories_.end())
    throw std::runtime_error("Invalid argument.");

  if (i->second != factory)
    throw std::runtime_error("Invalid argument.");

  defaultConnectionFactory_ = factory;
}

std::shared_ptr<ConnectionFactory> Connector::defaultConnectionFactory() const {
  return defaultConnectionFactory_;
}

std::string Connector::toString() const {
  char buf[128];
  int n = snprintf(buf, sizeof(buf), "Connector/%s @ %p", name_.c_str(), this);
  return std::string(buf, n);
}

template <>
std::string StringUtil::toString(Connector* connector) {
  return connector->toString();
}

template <>
std::string StringUtil::toString(const Connector* connector) {
  return connector->toString();
}

} // namespace xzero
