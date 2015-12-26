// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/Buffer.h>
#include <xzero/io/FileView.h>
#include <memory>
#include <deque>

namespace xzero {

/**
 * Interface to implement the other endpoint to transfer data out of a DataChain.
 *
 * Implement this interface if you want to splice your data
 * efficiently into a socket or pipe for example.
 */
class DataChainListener {
 public:
  virtual ~DataChainListener() {}

  virtual size_t transfer(const BufferRef& chunk) = 0;
  virtual size_t transfer(const FileView& chunk) = 0;
};

/**
 * API to hold an ordered chain of different text data types.
 */
class DataChain {
 protected:
  class Chunk;
  class BufferChunk;
  class FileChunk;

 public:
  DataChain();

  /**
   * Removes any pending data from this chain, effectively emptying it out.
   */
  void reset();

  /**
   * Appends a C-string at the end of the chain.
   */
  void write(const char* cstr);

  /**
   * Appends an arbitrary buffer @p buf of @p n bytes at the end of the chain.
   */
  void write(const char* buf, size_t n);

  /**
   * Appends an arbitrary buffer @p buf at the endo f the chain.
   */
  void write(const BufferRef& buf);

  /**
   * Appends an arbitrary buffer @p buf at the end of the chain.
   */
  void write(Buffer&& buf);

  /**
   * Appends a @p file chunk at the end of the chain.
   */
  void write(FileView&& file);

  /**
   * Appends an opaque data @p chunk at the end of the chain.
   */
  void write(std::unique_ptr<Chunk>&& chunk);

  /**
   * Appends a byte at the end of the chain.
   */
  void write8(uint8_t bin);

  /**
   * Appends two bytes at the end of the chain.
   */
  void write16(uint16_t bin);

  /**
   * Appends a three bytes at the end of the chain.
   */
  void write24(uint32_t bin);

  /**
   * Appends 4 bytes at the end of the chain.
   */
  void write32(uint32_t bin);

  /**
   * Splits up to @p n bytes data from the front chunk of the data chain.
   *
   * The chunk is potentially cut to meet the byte requirements.
   *
   * @note This method only operates on the front chunk, never on many.
   *
   * @return the given chunk or @c nullptr if none available.
   */
  std::unique_ptr<Chunk> get(size_t n);

  /**
   * Transfers as much chained data chunks to @p target as possible.
   *
   * @retval true all data transferred
   * @retval false still data in chain pending
   */
  bool transferTo(DataChainListener* target);

  /**
   * Transfers up to @p n bytes of chained data chunks to @p target.
   *
   * @retval true all @p n bytes of requested data transferred
   * @retval false not all @p n bytes of requested data transferred
   */
  bool transferTo(DataChainListener* target, size_t n);

  /**
   * Tests if this data chain is empty.
   */
  bool empty() const noexcept;

  /**
   * Retrieves the total number of bytes this chain holds.
   */
  size_t size() const noexcept;

 protected:
  void flushBuffer();

 protected:
  std::deque<std::unique_ptr<Chunk>> chunks_;
  Buffer buffer_;
  size_t size_;
};

/**
 * The abstract interface for a Chunk (of data) within a DataChain.
 */
class DataChain::Chunk {
 public:
  virtual ~Chunk() {}

  virtual std::unique_ptr<Chunk> get(size_t n) = 0;
  virtual size_t transferTo(DataChainListener* sink, size_t n) = 0;
  virtual size_t size() const = 0;
};

// {{{ inlines
inline size_t DataChain::size() const noexcept {
  return size_;
}
// }}}

} // namespace xzero
