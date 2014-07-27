// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/FixedBufferSink.h>
#include <base/io/SinkVisitor.h>

namespace base {

void FixedBufferSink::accept(SinkVisitor& v) {
  ;  // TODO v.visit(*this);
}

ssize_t FixedBufferSink::write(const void* buffer, size_t size) {
  buffer_.push_back(buffer, size);
  return size;
}

}  // namespace base