// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/HeaderField.h>
#include <deque>
#include <cstdint>

namespace xzero {
namespace http {
namespace hpack {

/**
  * Compression Sensitive HeaderField Table.
  *
  * The dynamic table (see Section 2.3.2) is a table that associates stored
  * header fields with index values. This table is dynamic and specific to an
  * encoding or decoding context.
  */
class DynamicTable {
 public:
  explicit DynamicTable(size_t maxSize);

  /**
   * Represents the "not found" case when searching for elements in a table.
   */
  enum { npos = static_cast<size_t>(-1) };

  // XXX: The additional 32 octets account for an estimated overhead associated
  // with an entry. For example, an entry structure using two 64-bit pointers to
  // reference the name and the value of the entry and two 64-bit integers for
  // counting the number of references to the name and value would have 32
  // octets of overhead.
  enum { HeaderFieldOverheadSize = 32 };

  /**
   * Retrieves number of fields in the table.
   */
  size_t length() const noexcept { return entries_.size(); }

  /**
   * Retrieves the sum of the size of all table entries.
   */
  size_t size() const noexcept { return size_; }

  /**
   * Retrieves the maximum size the table may use.
   *
   * @see size() const noexcept
   */
  size_t maxSize() const noexcept { return maxSize_; }

  /**
   * Sets the maximum allowed total table size.
   *
   * @see size() const noexcept
   */
  void setMaxSize(size_t limit);

  /**
   * Adds given @p field to the dynamic table.
   */
  void add(const HeaderField& field);

  /**
   * Adds given @p name and @p value pair to the table.
   */
  void add(const std::string& name, const std::string& value);

  /**
   * Searches for given @p field in the dynamic table.
   *
   * @param name Header field name to match for.
   * @param value Header field value to match for (optionally).
   * @param nameValueMatch output parameter that will contain the match type
   *                       that is @c true if it was a full (name,value)-match
   *                       or @c false if just a name-match.
   *
   * @return @c 0 if not found or the index into the DynamicTable if found.
   */
  size_t find(const std::string& name,
              const std::string& value,
              bool* nameValueMatch) const;

  size_t find(const HeaderField& field, bool* nameValueMatch) const;

  const HeaderField& at(size_t index) const;

  void clear();

 protected:
  void evict();

 private:
  size_t maxSize_;
  size_t size_;

  std::deque<HeaderField> entries_;
};

} // namespace hpack
} // namespace http
} // namespace xzero
