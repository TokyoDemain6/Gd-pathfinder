#include "gdpf/codec.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace gdpf {

// ---------------------------------------------------------------- base64 ---

std::optional<std::string> base64_decode(std::string_view in) {
    std::string out;
    out.reserve(in.size() * 3 / 4);
    std::uint32_t acc = 0;
    int bits = 0;
    for (char ch : in) {
        const auto c = static_cast<unsigned char>(ch);
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == '=' || c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        else return std::nullopt;
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xFFu));
        }
    }
    return out;
}

// --------------------------------------------------------------- inflate ---
//
// A straightforward RFC 1951 decoder using canonical Huffman decoding one bit
// at a time. It is not fast, but level strings are small (a few MB at most).

namespace {

struct BitReader {
    const std::uint8_t* data;
    std::size_t size;
    std::size_t pos = 0;
    std::uint64_t buf = 0;
    int count = 0;
    bool failed = false;

    std::uint32_t bits(int need) {
        while (count < need) {
            if (pos >= size) {
                failed = true;
                return 0;
            }
            buf |= static_cast<std::uint64_t>(data[pos++]) << count;
            count += 8;
        }
        const auto value = static_cast<std::uint32_t>(buf & ((std::uint64_t{1} << need) - 1));
        buf >>= need;
        count -= need;
        return value;
    }

    void align_to_byte() {
        buf = 0;
        count = 0;
    }
};

constexpr int kMaxBits = 15;
constexpr int kMaxLitCodes = 286;
constexpr int kMaxDistCodes = 30;
constexpr int kFixedLitCodes = 288;

struct Huffman {
    std::array<std::uint16_t, kMaxBits + 1> count{};
    std::array<std::uint16_t, kFixedLitCodes> symbol{};
};

// Builds decoding tables from code lengths. Returns <0 for an over-subscribed
// set, 0 for a complete set and >0 for an incomplete one.
int construct(Huffman& h, const std::uint16_t* length, int n) {
    h.count.fill(0);
    for (int sym = 0; sym < n; ++sym) ++h.count[length[sym]];
    if (h.count[0] == n) return 0;

    int left = 1;
    for (int len = 1; len <= kMaxBits; ++len) {
        left <<= 1;
        left -= h.count[static_cast<std::size_t>(len)];
        if (left < 0) return left;
    }

    std::array<std::uint16_t, kMaxBits + 1> offs{};
    for (std::size_t len = 1; len < kMaxBits; ++len) {
        offs[len + 1] = static_cast<std::uint16_t>(offs[len] + h.count[len]);
    }
    for (int sym = 0; sym < n; ++sym) {
        if (length[sym] != 0) {
            h.symbol[offs[length[sym]]++] = static_cast<std::uint16_t>(sym);
        }
    }
    return left;
}

int decode(BitReader& in, const Huffman& h) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (std::size_t len = 1; len <= kMaxBits; ++len) {
        code |= static_cast<int>(in.bits(1));
        if (in.failed) return -1;
        const int count = h.count[len];
        if (code - count < first) return h.symbol[static_cast<std::size_t>(index + (code - first))];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

bool inflate_codes(BitReader& in, std::string& out, const Huffman& lencode, const Huffman& distcode) {
    static constexpr std::uint16_t lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10,  11,  13,
                                                15, 17, 19, 23, 27, 31, 35, 43,  51,  59,
                                                67, 83, 99, 115, 131, 163, 195, 227, 258};
    static constexpr std::uint16_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static constexpr std::uint16_t dbase[30] = {1,    2,    3,    4,    5,    7,     9,     13,
                                                17,   25,   33,   49,   65,   97,    129,   193,
                                                257,  385,  513,  769,  1025, 1537,  2049,  3073,
                                                4097, 6145, 8193, 12289, 16385, 24577};
    static constexpr std::uint16_t dext[30] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                               4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                               9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int symbol = decode(in, lencode);
        if (symbol < 0) return false;
        if (symbol < 256) {
            out.push_back(static_cast<char>(symbol));
        } else if (symbol == 256) {
            return true;
        } else {
            symbol -= 257;
            if (symbol >= 29) return false;
            const std::size_t len = lbase[symbol] + in.bits(lext[symbol]);
            const int dsym = decode(in, distcode);
            if (dsym < 0 || dsym >= 30) return false;
            const std::size_t dist = dbase[dsym] + in.bits(dext[dsym]);
            if (in.failed || dist > out.size()) return false;
            const std::size_t start = out.size() - dist;
            // Byte by byte on purpose: the source and destination may overlap.
            for (std::size_t i = 0; i < len; ++i) out.push_back(out[start + i]);
        }
    }
}

bool inflate_stored(BitReader& in, std::string& out) {
    in.align_to_byte();
    if (in.pos + 4 > in.size) return false;
    const std::uint32_t len = in.data[in.pos] | (static_cast<std::uint32_t>(in.data[in.pos + 1]) << 8);
    const std::uint32_t nlen = in.data[in.pos + 2] | (static_cast<std::uint32_t>(in.data[in.pos + 3]) << 8);
    in.pos += 4;
    if ((len ^ 0xFFFFu) != nlen) return false;
    if (in.pos + len > in.size) return false;
    out.append(reinterpret_cast<const char*>(in.data + in.pos), len);
    in.pos += len;
    return true;
}

bool inflate_fixed(BitReader& in, std::string& out) {
    static const auto tables = [] {
        std::array<Huffman, 2> t{};
        std::array<std::uint16_t, kFixedLitCodes> lengths{};
        std::size_t sym = 0;
        for (; sym < 144; ++sym) lengths[sym] = 8;
        for (; sym < 256; ++sym) lengths[sym] = 9;
        for (; sym < 280; ++sym) lengths[sym] = 7;
        for (; sym < kFixedLitCodes; ++sym) lengths[sym] = 8;
        construct(t[0], lengths.data(), kFixedLitCodes);
        for (sym = 0; sym < kMaxDistCodes; ++sym) lengths[sym] = 5;
        construct(t[1], lengths.data(), kMaxDistCodes);
        return t;
    }();
    return inflate_codes(in, out, tables[0], tables[1]);
}

bool inflate_dynamic(BitReader& in, std::string& out) {
    static constexpr std::uint8_t order[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                               11, 4,  12, 3, 13, 2, 14, 1, 15};
    const int nlen = static_cast<int>(in.bits(5)) + 257;
    const int ndist = static_cast<int>(in.bits(5)) + 1;
    const int ncode = static_cast<int>(in.bits(4)) + 4;
    if (in.failed || nlen > kMaxLitCodes || ndist > kMaxDistCodes) return false;

    std::array<std::uint16_t, kMaxLitCodes + kMaxDistCodes> lengths{};
    for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<std::uint16_t>(in.bits(3));
    if (in.failed) return false;

    Huffman lencode;
    if (construct(lencode, lengths.data(), 19) != 0) return false;

    int index = 0;
    while (index < nlen + ndist) {
        const int symbol = decode(in, lencode);
        if (symbol < 0) return false;
        if (symbol < 16) {
            lengths[static_cast<std::size_t>(index++)] = static_cast<std::uint16_t>(symbol);
            continue;
        }
        std::uint16_t value = 0;
        int repeat;
        if (symbol == 16) {
            if (index == 0) return false;
            value = lengths[static_cast<std::size_t>(index - 1)];
            repeat = 3 + static_cast<int>(in.bits(2));
        } else if (symbol == 17) {
            repeat = 3 + static_cast<int>(in.bits(3));
        } else {
            repeat = 11 + static_cast<int>(in.bits(7));
        }
        if (in.failed || index + repeat > nlen + ndist) return false;
        while (repeat-- > 0) lengths[static_cast<std::size_t>(index++)] = value;
    }
    if (lengths[256] == 0) return false;  // no end-of-block code

    Huffman litcode;
    Huffman distcode;
    int err = construct(litcode, lengths.data(), nlen);
    if (err < 0 || (err > 0 && nlen - litcode.count[0] != 1)) return false;
    err = construct(distcode, lengths.data() + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return false;

    return inflate_codes(in, out, litcode, distcode);
}

// Inflates a raw deflate stream starting at `in.pos`. On success `in.pos` is
// left just past the last byte the stream used.
bool inflate_raw(BitReader& in, std::string& out) {
    for (;;) {
        const bool last = in.bits(1) != 0;
        const std::uint32_t type = in.bits(2);
        if (in.failed) return false;
        bool ok = false;
        if (type == 0) ok = inflate_stored(in, out);
        else if (type == 1) ok = inflate_fixed(in, out);
        else if (type == 2) ok = inflate_dynamic(in, out);
        if (!ok || in.failed) return false;
        if (last) break;
    }
    in.align_to_byte();
    return true;
}

std::uint32_t crc32(std::string_view data) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t crc = 0xFFFFFFFFu;
    for (char ch : data) crc = table[(crc ^ static_cast<unsigned char>(ch)) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t adler32(std::string_view data) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (char ch : data) {
        a = (a + static_cast<unsigned char>(ch)) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

std::uint32_t read_le32(const std::uint8_t* p) {
    return p[0] | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t read_be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

// Returns the offset of the deflate data inside a gzip member, or 0 on error.
std::size_t gzip_header_size(const std::uint8_t* p, std::size_t n) {
    if (n < 10 || p[2] != 8) return 0;
    const std::uint8_t flags = p[3];
    std::size_t pos = 10;
    if (flags & 0x04u) {  // FEXTRA
        if (pos + 2 > n) return 0;
        pos += 2u + (p[pos] | (static_cast<std::size_t>(p[pos + 1]) << 8));
    }
    for (std::uint8_t bit : {std::uint8_t{0x08}, std::uint8_t{0x10}}) {  // FNAME, FCOMMENT
        if (flags & bit) {
            while (pos < n && p[pos] != 0) ++pos;
            ++pos;
        }
    }
    if (flags & 0x02u) pos += 2;  // FHCRC
    return pos < n ? pos : 0;
}

}  // namespace

std::optional<std::string> inflate(std::string_view input) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(input.data());
    const std::size_t n = input.size();
    std::string out;

    enum class Wrapper { Raw, Gzip, Zlib } wrapper = Wrapper::Raw;
    std::size_t start = 0;
    if (n >= 2 && p[0] == 0x1F && p[1] == 0x8B) {
        wrapper = Wrapper::Gzip;
        start = gzip_header_size(p, n);
        if (start == 0) return std::nullopt;
    } else if (n >= 2 && (p[0] & 0x0Fu) == 8 && ((static_cast<unsigned>(p[0]) << 8) | p[1]) % 31 == 0) {
        if (p[1] & 0x20u) return std::nullopt;  // preset dictionaries are not supported
        wrapper = Wrapper::Zlib;
        start = 2;
    }

    BitReader in{p + start, n - start};
    if (!inflate_raw(in, out)) return std::nullopt;

    // Verify the trailer when it is present. Some tools truncate it.
    const std::uint8_t* trailer = p + start + in.pos;
    const std::size_t remaining = n - start - in.pos;
    if (wrapper == Wrapper::Gzip && remaining >= 8) {
        if (read_le32(trailer) != crc32(out)) return std::nullopt;
        if (read_le32(trailer + 4) != static_cast<std::uint32_t>(out.size())) return std::nullopt;
    } else if (wrapper == Wrapper::Zlib && remaining >= 4) {
        if (read_be32(trailer) != adler32(out)) return std::nullopt;
    }
    return out;
}

}  // namespace gdpf
