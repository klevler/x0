/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _libxzero_UTIL_LOGGER_IMPL_H
#define _libxzero_UTIL_LOGGER_IMPL_H
#include <xzero/StringUtil.h>

namespace xzero {

template <typename... T>
void Logger::log(
    LogLevel log_level,
    const String& component,
    const String& message,
    T... args) {
  if (log_level >= min_level_) {
    log(log_level, component, StringUtil::format(message, args...));
  }
}

template <typename... T>
void Logger::logException(
    LogLevel log_level,
    const String& component,
    const std::exception& exception,
    const String& message,
    T... args) {
  if (log_level >= min_level_) {
    logException(
        log_level,
        component,
        exception,
        StringUtil::format(message, args...));
  }
}

} // namespace xzero

#endif
