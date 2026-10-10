// Decoding helpers for Geometry Dash level strings.
//
// A saved level string is base64 (URL-safe alphabet) wrapped around a gzip
// stream. Older data uses a zlib wrapper instead. Both are handled here with a
// small built-in inflate so the project has no third-party dependencies.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace gdpf {

// Accepts both the standard and URL-safe alphabets. Padding and whitespace are
// ignored. Returns nullopt if any other character is present.
std::optional<std::string> base64_decode(std::string_view in);

// Inflates a gzip, zlib or raw deflate stream (auto-detected). Returns nullopt
// on malformed input or a checksum mismatch.
std::optional<std::string> inflate(std::string_view in);

}  // namespace gdpf
