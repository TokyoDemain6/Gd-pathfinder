#include "check.hpp"
#include "gdpf/codec.hpp"

using gdpf::base64_decode;
using gdpf::inflate;

namespace {

void check_fixture(const std::string& encoded_name, const std::string& raw_name) {
    const std::string encoded = read_fixture(encoded_name);
    const std::string expected = read_fixture(raw_name);
    CHECK(!encoded.empty());
    CHECK(!expected.empty());
    const auto bytes = base64_decode(encoded);
    CHECK(bytes.has_value());
    if (!bytes) return;
    const auto out = inflate(*bytes);
    CHECK(out.has_value());
    if (!out) return;
    CHECK(*out == expected);
}

}  // namespace

int main() {
    // base64, both alphabets
    CHECK(base64_decode("aGVsbG8=") == std::string("hello"));
    CHECK(base64_decode("aGVsbG8") == std::string("hello"));
    CHECK(base64_decode("-_8=") == std::string("\xFB\xFF"));
    CHECK(base64_decode("+/8=") == std::string("\xFB\xFF"));
    CHECK(base64_decode("aGVs\nbG8=\n") == std::string("hello"));
    CHECK(base64_decode("") == std::string());
    CHECK(!base64_decode("aGV$bG8=").has_value());

    // every block type and wrapper
    check_fixture("simple.gzip.txt", "simple.raw.txt");    // fixed Huffman, gzip
    check_fixture("simple.zlib.txt", "simple.raw.txt");    // zlib wrapper
    check_fixture("simple.stored.txt", "simple.raw.txt");  // stored blocks
    check_fixture("big.gzip.txt", "big.raw.txt");          // dynamic Huffman
    check_fixture("big.fast.txt", "big.raw.txt");

    // damaged input is rejected, not silently accepted
    {
        const auto good = base64_decode(read_fixture("big.gzip.txt"));
        CHECK(good.has_value());
        if (good) {
            std::string flipped = *good;
            flipped[flipped.size() / 2] = static_cast<char>(flipped[flipped.size() / 2] ^ 0x55);
            CHECK(!inflate(flipped).has_value());

            const std::string truncated = good->substr(0, good->size() / 2);
            CHECK(!inflate(truncated).has_value());

            std::string bad_crc = *good;
            bad_crc[bad_crc.size() - 6] = static_cast<char>(bad_crc[bad_crc.size() - 6] ^ 0x01);
            CHECK(!inflate(bad_crc).has_value());
        }
    }
    CHECK(!inflate("").has_value());
    CHECK(!inflate("not compressed data at all").has_value());

    return finish("codec");
}
