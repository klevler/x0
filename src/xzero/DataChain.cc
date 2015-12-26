// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/DataChain.h>
#include <xzero/io/FileView.h>
#include <xzero/Buffer.h>

namespace xzero {

// {{{ Chunk API overrides
class DataChain::BufferChunk : public Chunk {
 public:
  explicit BufferChunk(const BufferRef& buffer)
      : buffer_(buffer), offset_(0) {}

  explicit BufferChunk(Buffer&& buffer)
      : buffer_(std::forward<Buffer>(buffer)), offset_(0) {}

  std::unique_ptr<Chunk> get(size_t n) override;
  size_t transferTo(DataChainSink* sink, size_t n) override;
  size_t size() const override;

 private:
  Buffer buffer_;
  size_t offset_;
};

class DataChain::FileChunk : public Chunk {
 public:
  explicit FileChunk(FileView&& ref)
      : file_(std::forward<FileView>(ref)) {}

  std::unique_ptr<Chunk> get(size_t n) override;
  size_t transferTo(DataChainSink* sink, size_t n) override;
  size_t size() const override;

 private:
  FileView file_;
};
// }}}
// {{{ BufferChunk impl
std::unique_ptr<DataChain::Chunk> DataChain::BufferChunk::get(size_t n) {
  std::unique_ptr<Chunk> chunk(new BufferChunk(buffer_.ref(offset_, n)));
  offset_ += n;
  return chunk;
}

size_t DataChain::BufferChunk::transferTo(DataChainSink* sink, size_t n) {
  size_t out = sink->transfer(buffer_.ref(offset_, std::min(n, size())));
  offset_ += out;
  return out;
}

size_t DataChain::BufferChunk::size() const {
  return buffer_.size() - offset_;
}
// }}}
// {{{ FileChunk impl
std::unique_ptr<DataChain::Chunk> DataChain::FileChunk::get(size_t n) {
  std::unique_ptr<Chunk> chunk(new FileChunk(file_.view(0, n)));

  file_.setSize(file_.size() - n);
  file_.setOffset(file_.offset() + n);

  return chunk;
}

size_t DataChain::FileChunk::transferTo(DataChainSink* sink, size_t n) {
  size_t out = sink->transfer(file_.view(0, n));

  file_.setSize(file_.size() - out);
  file_.setOffset(file_.offset() + out);

  return out;
}

size_t DataChain::FileChunk::size() const {
  return file_.size();
}
// }}}

DataChain::DataChain()
    : chunks_(),
      buffer_(),
      size_(0) {
}

void DataChain::reset() {
  chunks_.clear();
  buffer_.clear();
  size_ = 0;
}

bool DataChain::empty() const noexcept {
  return chunks_.empty() && buffer_.empty();
}

void DataChain::write(const char* cstr) {
  size_t clen = strlen(cstr);
  buffer_.push_back(cstr, clen);
  size_ += clen;
}

void DataChain::write(const char* buf, size_t n) {
  buffer_.push_back(buf, n);
  size_ += n;
}

void DataChain::write(const BufferRef& buf) {
  buffer_.push_back(buf.data(), buf.size());
  size_ += buf.size();
}

void DataChain::write(Buffer&& buf) {
  if (buf.size() < 1024) { // buffer to small to justify a new object?
    buffer_.push_back(buf);
    size_ += buf.size();
  } else {
    flushBuffer();
    size_ += buf.size();
    buffer_ = std::move(buf);
  }
}

void DataChain::write(FileView&& file) {
  flushBuffer();
  if (!file.empty()) {
    chunks_.emplace_back(new FileChunk(std::move(file)));
    size_ += chunks_.back()->size();
  }
}

void DataChain::write(std::unique_ptr<Chunk>&& chunk) {
  flushBuffer();
  chunks_.emplace_back(std::move(chunk));
  size_ += chunks_.back()->size();
}

void DataChain::write8(uint8_t bin) {
  buffer_.push_back((char) bin);
  size_ += 1;
}

void DataChain::write16(uint16_t bin) {
  buffer_.push_back((char) ((bin >> 8) & 0xFF));
  buffer_.push_back((char) (bin & 0xFF));
  size_ += 2;
}

void DataChain::write24(uint32_t bin) {
  buffer_.push_back((char) ((bin >> 16) & 0xFF));
  buffer_.push_back((char) ((bin >> 8) & 0xFF));
  buffer_.push_back((char) (bin & 0xFF));
  size_ += 3;
}

void DataChain::write32(uint32_t bin) {
  buffer_.push_back((char) ((bin >> 24) & 0xFF));
  buffer_.push_back((char) ((bin >> 16) & 0xFF));
  buffer_.push_back((char) ((bin >> 8) & 0xFF));
  buffer_.push_back((char) (bin & 0xFF));
  size_ += 4;
}

void DataChain::flushBuffer() {
  if (!buffer_.empty()) {
    chunks_.emplace_back(new BufferChunk(std::move(buffer_)));
  }
}

std::unique_ptr<DataChain::Chunk> DataChain::get(size_t n) {
  if (chunks_.empty()) {
    flushBuffer();

    if (chunks_.empty()) {
      return nullptr;
    }
  }

  assert(chunks_.front() != nullptr);

  if (chunks_.front()->size() < n) {
    std::unique_ptr<Chunk> out = std::move(chunks_.front());
    size_ -= out->size();
    chunks_.pop_front();
    return out;
  }

  auto chunk = chunks_.front()->get(n);
  size_ -= chunk->size();
  return chunk;
}

bool DataChain::transferTo(DataChainSink* target) {
  return transferTo(target, size_);
}

bool DataChain::transferTo(DataChainSink* target, size_t n) {
  flushBuffer();

  while (n > 0 && size_ > 0) {
    Chunk* front = chunks_.front().get();

    size_t transferred = front->transferTo(target, n);

    size_ -= transferred;
    n -= transferred;

    if (transferred < front->size() && n > 0)
      return false; // incomplete transfer

    if (front->size() == 0)
      chunks_.pop_front();

    if (n == 0)
      return true;
  }

  return n == 0;
}

} // namespace xzero
