// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/EndPointWriter.h>
#include <xzero/net/EndPoint.h>
#include <xzero/logging.h>
#include <unistd.h>

#ifndef NDEBUG
#define TRACE(msg...) logTrace("net.EndPointWriter", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero {

EndPointWriter::EndPointWriter()
    : chain_(),
      sink_(nullptr) {
}

EndPointWriter::~EndPointWriter() {
}

void EndPointWriter::write(const BufferRef& data) {
  TRACE("write: enqueue $0 bytes", data.size());
  chain_.push_back(data);
}

void EndPointWriter::write(Buffer&& chunk) {
  TRACE("write: enqueue $0 bytes", chunk.size());
  chain_.push_back(std::move(chunk));
}

void EndPointWriter::write(FileView&& chunk) {
  TRACE("write: enqueue $0 bytes", chunk.size());
  chain_.push_back(std::move(chunk));
}

bool EndPointWriter::flush(EndPoint* sink) {
  TRACE("write: flushing $0 bytes", chain_.size());
  sink_ = sink;
  return chain_.transferTo(this);
}

bool EndPointWriter::empty() const {
  return chain_.empty();
}

size_t EndPointWriter::transfer(const BufferRef& chunk) {
  TRACE("transfer(buf): $0 bytes", chunk.size());
  return sink_->flush(chunk);
}

size_t EndPointWriter::transfer(const FileView& file) {
  TRACE("transfer(file): $0 bytes, fd $1", file.size(), file.handle());
  return sink_->flush(file.handle(), file.offset(), file.size());
}

} // namespace xzero
