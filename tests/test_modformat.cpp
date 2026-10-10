// Tests for the mod's text helpers (mod/src/format.hpp), which do not need the
// game or Geode to build.

#include "check.hpp"
#include "format.hpp"

using namespace pfmod;

namespace {

std::size_t count(const std::string& text, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++n;
    return n;
}

gdpf::Level must_parse(const std::string& text) {
    gdpf::ParseResult r = gdpf::parse_level(text);
    CHECK(r.ok);
    return r.level;
}

}  // namespace

int main() {
    // --- file names -----------------------------------------------------------
    CHECK(safe_file_name("Stereo Madness", 0) == "Stereo Madness");
    CHECK(safe_file_name("My Layout", 12345678) == "My Layout_12345678");
    CHECK(safe_file_name("a/b\\c:d*e?f\"g<h>i|j", 0) == "a_b_c_d_e_f_g_h_i_j");
    CHECK(safe_file_name("../../evil", 7) == "______evil_7");
    CHECK(safe_file_name("  padded  ", 0) == "padded");
    CHECK(safe_file_name("", 0) == "level");
    CHECK(safe_file_name("   ", 9) == "level_9");
    CHECK(safe_file_name("\xC3\xA9t\xC3\xA9", 0) == "__t__");  // non-ASCII bytes are replaced
    CHECK(safe_file_name(std::string(200, 'x'), 5) == std::string(60, 'x') + "_5");
    CHECK(safe_file_name("name", -3) == "name");

    // --- level report -----------------------------------------------------------
    {
        const gdpf::Level level = must_parse("kA2,0;1,1,2,15,3,15;1,8,2,45,3,15;");
        const std::string text = report_message(level, gdpf::analyze(level));
        CHECK(text.find("<cg>Supported</c>") == 0);
        CHECK(text.find("2 objects.") != std::string::npos);
        CHECK(count(text, "\n- ") == 0);
    }
    {
        const gdpf::Level level = must_parse("kA2,0;1,1,2,15,3,15;1,987654,2,45,3,15;");
        const std::string text = report_message(level, gdpf::analyze(level));
        CHECK(text.find("<cy>Unverified</c>") == 0);
        CHECK(count(text, "\n- ") == 1);
        CHECK(text.find("987654") != std::string::npos);
    }
    {
        // Platformer, dual start, a move trigger, a slope, a dash orb, a
        // teleport portal and a tilted block: seven issues.
        const gdpf::Level level = must_parse(
            "kA22,1,kA8,1;1,901,2,15,3,15;1,289,2,45,3,15;1,1704,2,75,3,15;1,747,2,105,3,15;1,1,2,135,3,15,6,30;");
        const gdpf::Analysis analysis = gdpf::analyze(level);
        CHECK(analysis.issues.size() == 7);
        const std::string text = report_message(level, analysis);
        CHECK(text.find("<cr>Unsupported</c>") == 0);
        CHECK(count(text, "\n- ") == 5);  // capped
        CHECK(text.find("...and 2 more.") != std::string::npos);
        // Colour tags are balanced, or the game shows the rest of the text tinted.
        CHECK(count(text, "<c") == count(text, "</c>"));

        const std::string all = report_message(level, analysis, 100);
        CHECK(count(all, "\n- ") == 7);
        CHECK(all.find("more.") == std::string::npos);
    }
    {
        // A very long issue line is shortened.
        gdpf::Level level;
        gdpf::Analysis analysis;
        analysis.verdict = gdpf::Verdict::Unsupported;
        analysis.issues.push_back({true, std::string(500, 'z')});
        const std::string text = report_message(level, analysis);
        CHECK(text.find(std::string(107, 'z') + "...") != std::string::npos);
        CHECK(text.find(std::string(108, 'z')) == std::string::npos);
    }

    // --- object dump rows -------------------------------------------------------
    CHECK(count(kObjectCsvHeader, ",") == 18);
    {
        ObjectRow row;
        row.id = 8;
        row.type = 2;
        row.x = 345;
        row.y = 105;
        row.start_x = 345;
        row.start_y = 105;
        row.rotation = 90;
        row.scale_x = 1.5;
        row.scale_y = 1;
        row.flip_y = true;
        row.rect_x = 342;
        row.rect_y = 99;
        row.rect_w = 6;
        row.rect_h = 12;
        row.trigger = true;
        std::string out;
        append_object_row(out, row);
        CHECK(out == "8,2,345,105,345,105,90,1.5,1,0,1,342,99,6,12,0,0,0,1\n");
        append_object_row(out, row);
        CHECK(count(out, "\n") == 2);
    }
    {
        // 32-bit float positions survive unchanged, and extreme values still
        // produce one complete row with the right number of fields.
        ObjectRow row;
        row.x = static_cast<double>(123456.789f);
        row.y = static_cast<double>(0.1f);
        row.rect_w = 1e300;
        row.rect_h = -1e-300;
        row.radius = 32.3f;
        std::string out;
        append_object_row(out, row);
        CHECK(!out.empty() && out.back() == '\n');
        CHECK(count(out, ",") == 18);
        CHECK(count(out, "\n") == 1);
        double x = 0;
        double y = 0;
        CHECK(std::sscanf(out.c_str(), "0,0,%lf,%lf,", &x, &y) == 2);
        CHECK(static_cast<float>(x) == 123456.789f);
        CHECK(static_cast<float>(y) == 0.1f);
    }

    return finish("modformat");
}
