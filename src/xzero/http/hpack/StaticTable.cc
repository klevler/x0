// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/StaticTable.h>
#include <assert.h>

namespace xzero {
namespace http {
namespace hpack {

TableEntry StaticTable::entries_[61] = { // {{{
    /*  0 */ {":authority", ""},
    /*  1 */ {":method", "GET"},
    /*  2 */ {":method", "POST"},
    /*  3 */ {":path", "/"},
    /*  4 */ {":path", "/index.html"},
    /*  5 */ {":scheme", "http"},
    /*  6 */ {":scheme", "https"},
    /*  7 */ {":status", "200"},
    /*  8 */ {":status", "204"},
    /*  9 */ {":status", "206"},

    /* 10 */ {":status", "304"},
    /* 11 */ {":status", "400"},
    /* 12 */ {":status", "404"},
    /* 13 */ {":status", "500"},
    /* 14 */ {"accept-charset", ""},
    /* 15 */ {"accept-encoding", "gzip, deflate"},
    /* 16 */ {"accept-language", ""},
    /* 17 */ {"accept-ranges", ""},
    /* 18 */ {"accept", ""},
    /* 19 */ {"access-control-allow-origin", ""},

    /* 20 */ {"age", ""},
    /* 21 */ {"allow", ""},
    /* 22 */ {"authorization", ""},
    /* 23 */ {"cache-control", ""},
    /* 24 */ {"content-disposition", ""},
    /* 25 */ {"content-encoding", ""},
    /* 26 */ {"content-language", ""},
    /* 27 */ {"content-length", ""},
    /* 28 */ {"content-location", ""},
    /* 29 */ {"content-range", ""},

    /* 30 */ {"content-type", ""},
    /* 31 */ {"cookie", ""},
    /* 32 */ {"date", ""},
    /* 33 */ {"etag", ""},
    /* 34 */ {"expect", ""},
    /* 35 */ {"expires", ""},
    /* 36 */ {"from", ""},
    /* 37 */ {"host", ""},
    /* 38 */ {"if-match", ""},
    /* 39 */ {"if-modified-since", ""},

    /* 40 */ {"if-none-match", ""},
    /* 41 */ {"if-range", ""},
    /* 42 */ {"if-unmodified-since", ""},
    /* 43 */ {"last-modified", ""},
    /* 44 */ {"link", ""},
    /* 45 */ {"location", ""},
    /* 46 */ {"max-forwards", ""},
    /* 47 */ {"proxy-authenticate", ""},
    /* 48 */ {"proxy-authorization", ""},
    /* 49 */ {"range", ""},

    /* 50 */ {"referer", ""},
    /* 51 */ {"refresh", ""},
    /* 52 */ {"retry-after", ""},
    /* 53 */ {"server", ""},
    /* 54 */ {"set-cookie", ""},
    /* 55 */ {"strict-transport-security", ""},
    /* 56 */ {"transfer-encoding", ""},
    /* 57 */ {"user-agent", ""},
    /* 58 */ {"vary", ""},
    /* 59 */ {"via", ""},

    /* 60 */ {"www-authenticate", ""}
};
// }}}
StaticTable::SortedEntry StaticTable::sortedEntries_[61] = { // {{{
    /* XXX Yes, this is not ideal, even though the list looks sorted, it is not.
     * In order to still have quick search over it, I'll duplicate the
     * array of header fields and put the *ONLY* misguided field
     * into a new position, so that binary search works.
     */
    { 0, ":authority", ""},
    { 1, ":method", "GET"},
    { 2, ":method", "POST"},
    { 3, ":path", "/"},
    { 4, ":path", "/index.html"},
    { 5, ":scheme", "http"},
    { 6, ":scheme", "https"},
    { 7, ":status", "200"},
    { 8, ":status", "204"},
    { 9, ":status", "206"},

    {10, ":status", "304"},
    {11, ":status", "400"},
    {12, ":status", "404"},
    {13, ":status", "500"},
    {18, "accept", ""}, // XXX *** this is the change (moved up) *** XXX
    {14, "accept-charset", ""},
    {15, "accept-encoding", "gzip, deflate"},
    {16, "accept-language", ""},
    {17, "accept-ranges", ""},
    {19, "access-control-allow-origin", ""},

    {20, "age", ""},
    {21, "allow", ""},
    {22, "authorization", ""},
    {23, "cache-control", ""},
    {24, "content-disposition", ""},
    {25, "content-encoding", ""},
    {26, "content-language", ""},
    {27, "content-length", ""},
    {28, "content-location", ""},
    {29, "content-range", ""},

    {30, "content-type", ""},
    {31, "cookie", ""},
    {32, "date", ""},
    {33, "etag", ""},
    {34, "expect", ""},
    {35, "expires", ""},
    {36, "from", ""},
    {37, "host", ""},
    {38, "if-match", ""},
    {39, "if-modified-since", ""},

    {40, "if-none-match", ""},
    {41, "if-range", ""},
    {42, "if-unmodified-since", ""},
    {43, "last-modified", ""},
    {44, "link", ""},
    {45, "location", ""},
    {46, "max-forwards", ""},
    {47, "proxy-authenticate", ""},
    {48, "proxy-authorization", ""},
    {49, "range", ""},

    {50, "referer", ""},
    {51, "refresh", ""},
    {52, "retry-after", ""},
    {53, "server", ""},
    {54, "set-cookie", ""},
    {55, "strict-transport-security", ""},
    {56, "transfer-encoding", ""},
    {57, "user-agent", ""},
    {58, "vary", ""},
    {59, "via", ""},

    {60, "www-authenticate", ""}
};
// }}}

size_t StaticTable::length() {
  return sizeof(entries_) / sizeof(entries_[0]);
}

bool StaticTable::find(const TableEntry& entry,
                       size_t* index,
                       bool* nameValueMatch) {
  return find(entry.first, entry.second, index, nameValueMatch);
}

bool StaticTable::find(const std::string& name,
                       const std::string& value,
                       size_t* index,
                       bool* nameValueMatch) {
  size_t r = sizeof(sortedEntries_) / sizeof(*sortedEntries_) - 1;
  size_t l = 0;

  auto compare = [](const std::string& a, const std::string& b) -> int {
    int result = a.compare(b);
    return result;
  };

  while (r - l >= 2) {
    size_t m = (l + r) / 2;
    int cmp = compare(name, sortedEntries_[m].name);

    if (cmp < 0) {
      r = m - 1;
    } else if (cmp > 0) {
      l = m + 1;
    } else {
      cmp = compare(value, sortedEntries_[m].value);
      if (cmp < 0) {
        r = m - 1;
      } else if (cmp > 0) {
        l = m + 1;
      } else {
        *index = sortedEntries_[m].index;
        *nameValueMatch = true;
        return true;
      }
    }
  }

  int foundLeftName = compare(name, sortedEntries_[l].name) == 0;
  if (foundLeftName) {
    *index = sortedEntries_[l].index;
    if (compare(value, sortedEntries_[l].value) == 0) {
      *nameValueMatch = true;
      return true;
    }
  }

  int foundRightName = compare(name, sortedEntries_[r].name) == 0;
  if (foundRightName) {
    *index = sortedEntries_[r].index;
    if (compare(value, sortedEntries_[r].value) == 0) {
      *nameValueMatch = true;
      return true;
    }
  }

  return foundLeftName || foundRightName;
}

const TableEntry& StaticTable::at(size_t index) {
  assert(index < length());
  return entries_[index];
}

}  // namespace hpack
}  // namespace http
}  // namespace xzero
