#include "gdpf/level.hpp"

#include <cstdlib>
#include <cstring>

#include "gdpf/codec.hpp"

namespace gdpf {

namespace {

std::string_view trim(std::string_view s) {
    const auto is_space = [](char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; };
    while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
    return s;
}

// Calls fn(key, value) for each pair in "k,v,k,v". Stops early if fn returns
// false. A trailing key with no value is ignored.
template <typename Fn>
void for_each_pair(std::string_view text, Fn&& fn) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t key_end = text.find(',', pos);
        if (key_end == std::string_view::npos) return;
        std::size_t value_end = text.find(',', key_end + 1);
        if (value_end == std::string_view::npos) value_end = text.size();
        if (!fn(text.substr(pos, key_end - pos), text.substr(key_end + 1, value_end - key_end - 1))) {
            return;
        }
        pos = value_end + 1;
    }
}

bool is_number_key(std::string_view key) {
    if (key.empty()) return false;
    for (char c : key) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

int to_int(std::string_view s, int fallback = 0) {
    char buf[32];
    if (s.empty() || s.size() >= sizeof(buf)) return fallback;
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    char* end = nullptr;
    const long v = std::strtol(buf, &end, 10);
    return end == buf ? fallback : static_cast<int>(v);
}

double to_double(std::string_view s, double fallback = 0) {
    char buf[64];
    if (s.empty() || s.size() >= sizeof(buf)) return fallback;
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    char* end = nullptr;
    const double v = std::strtod(buf, &end);
    return end == buf ? fallback : v;
}

// Pulls <k>KEY</k><s>VALUE</s> out of a .gmd export.
std::optional<std::string_view> gmd_string(std::string_view text, std::string_view key) {
    std::string needle = "<k>";
    needle += key;
    needle += "</k>";
    const std::size_t at = text.find(needle);
    if (at == std::string_view::npos) return std::nullopt;
    const std::size_t open = text.find("<s>", at + needle.size());
    if (open == std::string_view::npos) return std::nullopt;
    const std::size_t close = text.find("</s>", open + 3);
    if (close == std::string_view::npos) return std::nullopt;
    return text.substr(open + 3, close - open - 3);
}

LevelObject parse_object(std::string_view segment) {
    LevelObject obj;
    obj.raw = std::string(segment);
    double scale = 1;
    double scale_x = 1;
    double scale_y = 1;
    for_each_pair(segment, [&](std::string_view k, std::string_view v) {
        if (!is_number_key(k)) return true;
        switch (to_int(k)) {
            case key::Id: obj.id = to_int(v); break;
            case key::X: obj.x = to_double(v); break;
            case key::Y: obj.y = to_double(v); break;
            case key::FlipX: obj.flip_x = to_int(v) != 0; break;
            case key::FlipY: obj.flip_y = to_int(v) != 0; break;
            case key::Rotation: obj.rotation = to_double(v); break;
            case key::Scale: scale = to_double(v, 1); break;
            case key::ScaleX: scale_x = to_double(v, 1); break;
            case key::ScaleY: scale_y = to_double(v, 1); break;
            case key::Groups: {
                std::size_t pos = 0;
                while (pos <= v.size()) {
                    std::size_t dot = v.find('.', pos);
                    if (dot == std::string_view::npos) dot = v.size();
                    if (dot > pos) obj.groups.push_back(to_int(v.substr(pos, dot - pos)));
                    pos = dot + 1;
                }
                break;
            }
            default: break;
        }
        return true;
    });
    obj.scale_x = scale * scale_x;
    obj.scale_y = scale * scale_y;
    return obj;
}

}  // namespace

std::optional<std::string_view> LevelObject::prop(std::string_view wanted) const {
    std::optional<std::string_view> found;
    for_each_pair(raw, [&](std::string_view k, std::string_view v) {
        if (k != wanted) return true;
        found = v;
        return false;
    });
    return found;
}

std::optional<std::string_view> LevelObject::prop(int wanted) const {
    return prop(std::string_view(std::to_string(wanted)));
}

StartSettings parse_start_settings(std::string_view text) {
    StartSettings s;
    for_each_pair(text, [&](std::string_view k, std::string_view v) {
        if (k == "kA2") s.gamemode = gamemode_from_setting(to_int(v));
        else if (k == "kA3") s.mini = to_int(v) != 0;
        else if (k == "kA4") s.speed = speed_from_setting(to_int(v));
        else if (k == "kA8") s.dual = to_int(v) != 0;
        else if (k == "kA10") s.two_player = to_int(v) != 0;
        else if (k == "kA11") s.flip_gravity = to_int(v) != 0;
        else if (k == "kA22") s.platformer = to_int(v) != 0;
        return true;
    });
    return s;
}

ParseResult parse_level(std::string_view input) {
    ParseResult result;
    std::string_view text = trim(input);
    if (text.empty()) {
        result.error = "input is empty";
        return result;
    }

    // .gmd export: the level string lives in the k4 entry.
    if (text.find("<k>k4</k>") != std::string_view::npos) {
        const auto inner = gmd_string(text, "k4");
        if (!inner) {
            result.error = "found a k4 key but no level string after it";
            return result;
        }
        if (const auto level_name = gmd_string(text, "k2")) result.level.name = std::string(*level_name);
        text = trim(*inner);
    }

    // An encoded string never contains ',' or ';', so their presence means the
    // text is already decoded.
    std::string decoded;
    if (text.find(';') == std::string_view::npos && text.find(',') == std::string_view::npos) {
        const auto bytes = base64_decode(text);
        if (!bytes) {
            result.error = "not a level string: expected key,value pairs or base64 text";
            return result;
        }
        auto inflated = inflate(*bytes);
        if (!inflated) {
            result.error = "base64 decoded, but the data inside is not valid gzip/zlib";
            return result;
        }
        decoded = std::move(*inflated);
        text = trim(decoded);
    }

    bool first = true;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find(';', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view segment = text.substr(pos, end - pos);
        pos = end + 1;
        if (segment.empty()) continue;

        // The header is the first segment and its keys start with "k". Some
        // tools emit object-only strings, so only treat it as a header if so.
        if (first) {
            first = false;
            if (segment.front() == 'k') {
                result.level.header = std::string(segment);
                result.level.start = parse_start_settings(segment);
                continue;
            }
        }

        LevelObject obj = parse_object(segment);
        if (obj.id <= 0) {
            result.error = "object without a valid id (key 1): \"" + std::string(segment.substr(0, 60)) + "\"";
            return result;
        }
        result.level.objects.push_back(std::move(obj));
    }

    result.ok = true;
    return result;
}

}  // namespace gdpf
