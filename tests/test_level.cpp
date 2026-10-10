#include "check.hpp"
#include "gdpf/analyze.hpp"
#include "gdpf/level.hpp"

using namespace gdpf;

namespace {

Level must_parse(const std::string& text) {
    ParseResult r = parse_level(text);
    CHECK(r.ok);
    if (!r.ok) std::printf("  parse error: %s\n", r.error.c_str());
    return r.level;
}

void check_simple(const Level& level) {
    CHECK(level.start.gamemode == Gamemode::Cube);
    CHECK(level.start.speed == Speed::Normal);
    CHECK(!level.start.mini);
    CHECK(level.objects.size() == 6);
    if (level.objects.size() != 6) return;

    const LevelObject& block = level.objects[0];
    CHECK(block.id == 1);
    CHECK_NEAR(block.x, 315, 0);
    CHECK_NEAR(block.y, 15, 0);
    CHECK_NEAR(block.rotation, 0, 0);
    CHECK_NEAR(block.scale_x, 1, 0);
    CHECK(block.groups.empty());

    const LevelObject& spike = level.objects[1];
    CHECK(spike.id == 8);
    CHECK_NEAR(spike.rotation, 90, 0);

    const LevelObject& scaled = level.objects[2];
    CHECK_NEAR(scaled.scale_x, 2, 0);
    CHECK_NEAR(scaled.scale_y, 2, 0);
    CHECK(scaled.groups == std::vector<int>({3, 7, 12}));

    const LevelObject& orb = level.objects[3];
    CHECK(orb.id == 36);
    CHECK(orb.flip_x);
    CHECK(orb.flip_y);

    const LevelObject& stretched = level.objects[4];
    CHECK_NEAR(stretched.scale_x, 1.5, 0);
    CHECK_NEAR(stretched.scale_y, 0.5, 0);

    // A start position carries its own settings, readable from the raw text.
    const LevelObject& start_pos = level.objects[5];
    CHECK(start_pos.id == 31);
    const StartSettings s = parse_start_settings(start_pos.raw);
    CHECK(s.gamemode == Gamemode::Ship);
    CHECK(s.speed == Speed::Double);
    CHECK(s.mini);

    CHECK(spike.prop(6) == std::string_view("90"));
    CHECK(!spike.prop(32).has_value());
    CHECK(start_pos.prop("kA4") == std::string_view("2"));
    // "2" is the x key; it must not match the value of another key.
    CHECK(block.prop(2) == std::string_view("315"));
}

}  // namespace

int main() {
    // The same level through every input form.
    check_simple(must_parse(read_fixture("simple.raw.txt")));
    check_simple(must_parse(read_fixture("simple.gzip.txt")));
    check_simple(must_parse(read_fixture("simple.zlib.txt")));
    check_simple(must_parse(read_fixture("simple.stored.txt")));
    {
        const Level level = must_parse(read_fixture("simple.gmd"));
        check_simple(level);
        CHECK(level.name == "Fixture Level");
    }
    check_simple(must_parse("  \n" + read_fixture("simple.gzip.txt") + "\n\n"));

    {
        const Level big = must_parse(read_fixture("big.gzip.txt"));
        CHECK(big.objects.size() == 4000);
        CHECK(big.start.gamemode == Gamemode::Ship);
        CHECK(big.start.speed == Speed::Triple);
        CHECK(big.start.mini);
        CHECK(big.start.flip_gravity);
    }

    // Header keys.
    CHECK(parse_start_settings("kA4,0").speed == Speed::Normal);
    CHECK(parse_start_settings("kA4,1").speed == Speed::Half);
    CHECK(parse_start_settings("kA4,2").speed == Speed::Double);
    CHECK(parse_start_settings("kA4,3").speed == Speed::Triple);
    CHECK(parse_start_settings("kA4,4").speed == Speed::Quadruple);
    CHECK(parse_start_settings("kA2,7").gamemode == Gamemode::Swing);
    CHECK(parse_start_settings("kA2,99").gamemode == Gamemode::Cube);
    CHECK(parse_start_settings("kA22,1").platformer);
    CHECK(parse_start_settings("kA8,1").dual);
    CHECK(!parse_start_settings("kS38,1_40_2_125,kA13,0").mini);

    // Object-only strings and single objects have no header.
    {
        const Level level = must_parse("1,1,2,15,3,15;1,8,2,45,3,15;");
        CHECK(level.objects.size() == 2);
        CHECK(level.header.empty());
    }
    CHECK(must_parse("1,1,2,15,3,15").objects.size() == 1);
    CHECK(must_parse("kA2,3,kA4,1;").objects.empty());

    // Bad input is reported, not guessed at.
    CHECK(!parse_level("").ok);
    CHECK(!parse_level("   \n").ok);
    CHECK(!parse_level("!!! not a level !!!").ok);
    CHECK(!parse_level("aGVsbG8gd29ybGQ").ok);          // base64, but not compressed data
    CHECK(!parse_level("kA2,0;2,15,3,15;").ok);         // object with no id
    CHECK(!parse_level("<k>k4</k><i>3</i>").ok);        // .gmd without a level string

    // --- analysis ---------------------------------------------------------
    {
        const Analysis a = analyze(must_parse("kA2,0;1,1,2,15,3,15;1,8,2,45,3,15;1,36,2,75,3,75;"));
        CHECK(a.verdict == Verdict::Supported);
        CHECK(a.issues.empty());
        CHECK(a.kind_counts[static_cast<std::size_t>(ObjKind::Solid)] == 1);
        CHECK(a.kind_counts[static_cast<std::size_t>(ObjKind::Hazard)] == 1);
        CHECK(a.kind_counts[static_cast<std::size_t>(ObjKind::Orb)] == 1);
    }
    {
        // An ID the table does not know: not rejected, but flagged.
        const Analysis a = analyze(must_parse("kA2,0;1,1,2,15,3,15;1,987654,2,45,3,15;1,987654,2,75,3,15;"));
        CHECK(a.verdict == Verdict::Unverified);
        CHECK(a.unknown_objects == 2);
        CHECK(a.unknown_ids == 1);
        CHECK(a.issues.size() == 1);
    }
    {
        // A move trigger makes the level out of scope.
        const Analysis a = analyze(must_parse("kA2,0;1,1,2,15,3,15;1,901,2,45,3,15,51,2;"));
        CHECK(a.verdict == Verdict::Unsupported);
    }
    {
        // Colour and pulse triggers do not.
        const Analysis a = analyze(must_parse("kA2,0;1,1,2,15,3,15;1,899,2,45,3,15;1,1006,2,45,3,45;"));
        CHECK(a.verdict == Verdict::Supported);
    }
    CHECK(analyze(must_parse("kA22,1;1,1,2,15,3,15;")).verdict == Verdict::Unsupported);   // platformer
    CHECK(analyze(must_parse("kA8,1;1,1,2,15,3,15;")).verdict == Verdict::Unsupported);    // dual start
    CHECK(analyze(must_parse("kA2,0;1,289,2,15,3,15;")).verdict == Verdict::Unsupported);  // slope
    CHECK(analyze(must_parse("kA2,0;1,1704,2,15,3,15;")).verdict == Verdict::Unsupported); // dash orb
    CHECK(analyze(must_parse("kA2,0;1,286,2,15,3,15;")).verdict == Verdict::Unsupported);  // dual portal
    CHECK(analyze(must_parse("kA2,0;1,287,2,15,3,15;")).verdict == Verdict::Supported);    // leaving dual is fine
    CHECK(analyze(must_parse("kA2,0;1,1,2,15,3,15,6,45;")).verdict == Verdict::Unsupported);  // rotated solid
    CHECK(analyze(must_parse("kA2,0;1,1,2,15,3,15,6,270;")).verdict == Verdict::Supported);
    CHECK(analyze(must_parse("kA2,0;1,8,2,15,3,15,6,45;")).verdict == Verdict::Supported);    // rotated spike is fine

    return finish("level");
}
