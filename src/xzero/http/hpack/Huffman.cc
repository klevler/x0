// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/Huffman.h>

namespace xzero {
namespace http {
namespace hpack {

struct HuffmanCode {
  unsigned nbits;
  uint32_t code;
};

static HuffmanCode huffmanCodes[] = {
  // TODO [sym] = { nbits, code },
};

size_t Huffman::encodeLength(const std::string& value) {
  size_t n = 0;

  for (uint8_t ch: value)
    n += huffmanCodes[ch].nbits;

  return (n + 7) / 8;
}

std::string Huffman::encode(const std::string& value) {
  return ""; //TODO
}

std::string Huffman::decode(const std::string& value) {
  return ""; //TODO
}

} // namespace hpack
} // namespace http
} // namespace xzero
