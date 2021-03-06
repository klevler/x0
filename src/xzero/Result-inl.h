// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>

template<typename T>
inline Result<T>::Result(const T& value)
  : success_(true),
    failureMessage_() {
  new (storage_) T(value);
}

template<typename T>
inline Result<T>::Result(T&& value)
  : success_(true),
    failureMessage_() {
  new (storage_) T(std::move(value));
}

template<typename T>
inline Result<T>::Result(_FailureMessage&& failure)
  : success_(false),
    failureMessage_(std::move(failure.message)) {
}

template<typename T>
Result<T>::Result(Result&& other)
    : success_(other.success_),
      failureMessage_(std::move(other.failureMessage_)) {
  if (success_) {
    new (storage_) T(std::move(*other.get()));
    other.get()->~T();
  }
}


template<typename T>
inline Result<T>::~Result() {
  if (success_) {
    ((T*) &storage_)->~T();
  }
}

template<typename T>
inline Result<T>::operator bool () const noexcept {
  return success_;
}

template<typename T>
inline bool Result<T>::isSuccess() const noexcept {
  return success_;
}

template<typename T>
inline bool Result<T>::isFailure() const noexcept {
  return !success_;
}

template<typename T>
inline const std::string& Result<T>::failureMessage() const noexcept {
  return failureMessage_;
}

template<typename T>
inline T* Result<T>::get() {
  require();
  return ((T*) &storage_);
}

template<typename T>
inline const T* Result<T>::get() const {
  require();
  return ((T*) &storage_);
}

template<typename T>
inline T* Result<T>::operator->() {
  return get();
}

template<typename T>
inline const T* Result<T>::operator->() const {
  return get();
}

template<typename T>
inline T& Result<T>::operator*() {
  return *get();
}

template<typename T>
inline const T& Result<T>::operator*() const {
  return *get();
}

template<typename T>
inline void Result<T>::require() const {
  using xzero::Status;
  using xzero::RuntimeError;

  if (isFailure())
    RAISE(IllegalStateError);
}

inline _FailureMessage Failure(const std::string& message) {
  return _FailureMessage(std::move(message));
}

template<typename... Args>
inline _FailureMessage Failuref(const std::string& fmt, Args... args) {
  std::string message = ::xzero::StringUtil::format(fmt, args...);
  return _FailureMessage(std::move(message));
}

template<typename T>
inline Result<T> Success(const T& value) {
  return Result<T>(value);
}

template<typename T>
inline Result<T> Success(T&& value) {
  return Result<T>(std::move(value));
}
