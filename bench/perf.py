#!/usr/bin/env python3
"""Performance checks: measure, compare with bench/baseline.json, report.

    cmake -S . -B build && cmake --build build -j
    python3 bench/perf.py                      # every check this machine can run
    python3 bench/perf.py --sections work      # just one
    python3 bench/perf.py --update             # accept the new numbers

Exit status: 0 nothing moved, 1 a number moved without a baseline update,
2 a measurement could not be taken. bench/README.md explains each check.
"""

import argparse
import glob
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASELINE = ROOT / "bench" / "baseline.json"
SECTIONS = ["scenarios", "sizes", "table", "work", "instructions", "speed", "coverage", "mutation"]
TITLES = {
    "scenarios": "Simulator behaviour (one small level per mechanic)",
    "sizes": "Memory per state",
    "table": "Object table",
    "work": "Collision work per tick",
    "instructions": "CPU instructions per tick",
    "speed": "Wall-clock speed (shown only, never fails a run)",
    "coverage": "Test coverage",
    "mutation": "Physics numbers the tests pin down",
}
DEFAULT_LIMITS = {
    # A count of instructions repeats exactly on one machine. The margin is
    # for the C library, which picks different routines on different CPUs.
    "instructions_percent": 2.0,
    "coverage_points": 0.5,
    # An improvement nobody recorded lets a later slowdown back to the old
    # number pass unnoticed, so by default it has to be recorded too.
    "fail_on_unrecorded_improvement": True,
}

OK, WORSE, BETTER, CHANGED, NOTE = "ok", "worse", "better", "changed", "note"
MARK = {OK: "", WORSE: "❌ worse", BETTER: "✅ better", CHANGED: "⚠️ changed", NOTE: "ℹ️"}


class MeasureError(Exception):
    pass


# ---------------------------------------------------------------- running ---

def find_exe(build_dir, name):
    for sub in ("", "Release", "RelWithDebInfo", "Debug", "MinSizeRel"):
        for suffix in ("", ".exe"):
            path = pathlib.Path(build_dir) / sub / (name + suffix)
            if path.is_file():
                return str(path)
    raise MeasureError(f"{name} not found under {build_dir}: build the project first "
                       f"(cmake -S . -B {build_dir} && cmake --build {build_dir})")


def run(cmd, **kwargs):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    except FileNotFoundError:
        raise MeasureError(f"{cmd[0]} is not installed")


def bench_json(build_dir, command, stats=False):
    exe = find_exe(build_dir, "gdpf_bench_stats" if stats else "gdpf_bench")
    done = run([exe] + command)
    if done.returncode != 0:
        raise MeasureError(f"{os.path.basename(exe)} {' '.join(command)} failed: {done.stderr.strip()}")
    return json.loads(done.stdout)


# -------------------------------------------------------------- measuring ---

def measure_scenarios(args):
    return {"outcomes": bench_json(args.build_dir, ["scenarios"])}


def measure_sizes(args):
    return bench_json(args.build_dir, ["info"])["sizes"]


def measure_table(args):
    return bench_json(args.build_dir, ["info"])["object_table"]


def measure_work(args):
    return bench_json(args.build_dir, ["work"], stats=True)


def measure_speed(args):
    return bench_json(args.build_dir, ["time", str(args.reps)])


def measure_instructions(args):
    """Instructions per iteration, counted by valgrind's callgrind.

    Each case is run for one batch and for two; the difference is exactly one
    batch, with level building and program start-up cancelled out.
    """
    if shutil.which("valgrind") is None:
        raise MeasureError("valgrind is not installed (Linux: apt-get install valgrind)")
    exe = find_exe(args.build_dir, "gdpf_bench")
    cases = bench_json(args.build_dir, ["cases"])
    out = {"compiler": bench_json(args.build_dir, ["info"])["compiler"], "cases": {}}
    with tempfile.TemporaryDirectory() as tmp:
        for name, case in cases.items():
            totals = []
            for batches in (1, 2):
                log = os.path.join(tmp, f"{name}.{batches}")
                done = run(["valgrind", "--tool=callgrind", f"--callgrind-out-file={log}", exe, "run", name, str(batches)])
                found = re.search(r"^(?:summary|totals):\s*(\d+)", pathlib.Path(log).read_text(), re.M) if os.path.exists(log) else None
                if done.returncode != 0 or not found:
                    raise MeasureError(f"callgrind failed on {name}: {done.stderr.strip()[-300:]}")
                totals.append(int(found.group(1)))
            out["cases"][name] = {"unit": case["unit"],
                                  "per_iteration": round((totals[1] - totals[0]) / case["counted_n"], 2)}
    return out


def measure_coverage(args):
    """Line and branch coverage of the library by its own tests."""
    if shutil.which("gcovr") is None:
        raise MeasureError("gcovr is not installed (pip install gcovr)")
    build = pathlib.Path(str(args.build_dir) + "-coverage")
    steps = [
        ["cmake", "-S", str(ROOT), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_CXX_FLAGS=--coverage",
         "-DGDPF_BUILD_BENCH=OFF", "-DGDPF_BUILD_TOOLS=OFF"],
        ["cmake", "--build", str(build), "-j"],
    ]
    for step in steps:
        done = run(step)
        if done.returncode != 0:
            raise MeasureError(f"coverage build failed: {(done.stdout + done.stderr)[-600:]}")
    for stale in glob.glob(str(build / "**" / "*.gcda"), recursive=True):
        os.remove(stale)
    done = run(["ctest", "--test-dir", str(build), "--no-tests=error", "--output-on-failure"])
    if done.returncode != 0:
        raise MeasureError(f"tests failed in the coverage build: {done.stdout[-600:]}")
    with tempfile.TemporaryDirectory() as tmp:
        report = os.path.join(tmp, "coverage.json")
        done = run(["gcovr", "--root", str(ROOT), "--object-directory", str(build),
                    "--filter", re.escape(str(ROOT / "src")) + "/", "--filter", re.escape(str(ROOT / "include")) + "/",
                    "--filter", re.escape(str(ROOT / "mod" / "src" / "format.hpp")),
                    # CMake's own compiler probe is built with the coverage flag too.
                    "--gcov-exclude-directories", ".*CompilerId.*",
                    "--exclude-throw-branches", "--merge-mode-functions=merge-use-line-min",
                    "--json-summary", report])
        if done.returncode != 0 or not os.path.exists(report):
            raise MeasureError(f"gcovr failed: {done.stderr.strip()[-600:]}")
        summary = json.loads(pathlib.Path(report).read_text())
    return {
        "line": round(summary["line_percent"], 1),
        "branch": round(summary["branch_percent"], 1),
        "files": {f["filename"].replace("\\", "/"): round(f["line_percent"], 1) for f in summary["files"]},
    }


def measure_mutation(args):
    with tempfile.TemporaryDirectory() as tmp:
        report = os.path.join(tmp, "mutation.json")
        done = run([sys.executable, str(ROOT / "bench" / "mutation.py"), "--out", report])
        if done.returncode != 0:
            raise MeasureError(f"mutation.py failed: {(done.stdout + done.stderr).strip()[-600:]}")
        return json.loads(pathlib.Path(report).read_text())


MEASURE = {
    "scenarios": measure_scenarios, "sizes": measure_sizes, "table": measure_table, "work": measure_work,
    "instructions": measure_instructions, "speed": measure_speed, "coverage": measure_coverage,
    "mutation": measure_mutation,
}


# -------------------------------------------------------------- comparing ---
#
# Each compare_* returns (rows, lines): table rows of
# (name, baseline, now, change, verdict) and extra lines of text.

def pct(new, old):
    return "" if not old or new == old else f"{100.0 * (new - old) / old:+.1f}%"


def lower_is_better(name, old, new, change=None):
    verdict = OK if new == old else (WORSE if new > old else BETTER)
    return (name, old, new, change if change is not None else pct(new, old), verdict)


def ending(outcome):
    return outcome.split("@")[0]


def compare_scenarios(now, base, limits):
    now, base = now["outcomes"], (base or {}).get("outcomes", {})
    rows = []
    for name in list(base) + [n for n in now if n not in base]:
        old, new = base.get(name), now.get(name)
        if old == new:
            continue
        if old is None:
            rows.append((name, "", ending(new), "new scenario", CHANGED))
        elif new is None:
            rows.append((name, ending(old), "", "scenario removed", CHANGED))
        elif ending(old) == ending(new):
            rows.append((name, ending(old), ending(new), "same ending, different path", CHANGED))
        elif ending(new) == "ok":
            rows.append((name, ending(old), "ok", "now runs", BETTER))
        elif ending(old) == "ok":
            rows.append((name, "ok", ending(new), "no longer runs", WORSE))
        else:
            rows.append((name, ending(old), ending(new), "ends differently", CHANGED))
    count = lambda outcomes, test: sum(1 for o in outcomes.values() if test(ending(o)))
    lines = [f"{len(now)} scenarios: {count(now, lambda e: e in ('ok', 'finished'))} run to the end, "
             f"{count(now, lambda e: e.startswith('fault'))} stop at something not implemented, "
             f"{count(now, lambda e: e == 'dead')} end in a death."]
    if base and not rows:
        lines.append("Every scenario matches the baseline tick for tick, bit for bit.")
    return rows, lines


def compare_sizes(now, base, limits):
    return ([lower_is_better(f"sizeof({k}), bytes", (base or {}).get(k, v), v) for k, v in now.items()],
            ["One search state is " + str(now.get("PlayerState", "?")) + " bytes."])


def compare_table(now, base, limits):
    base = base or now
    old, new = base.get("total", now["total"]), now["total"]
    rows = [("object IDs known", old, new, f"{new - old:+d}" if new != old else "",
             OK if new == old else (BETTER if new > old else WORSE))]
    kinds = ", ".join(f"{now[k]} {k}" for k in ("solid", "hazard", "saw", "slope", "pad", "orb") if k in now)
    return rows, [f"{new} object IDs known, of which: {kinds}."]


def compare_work(now, base, limits):
    rows = []
    for case, n in now.items():
        b = (base or {}).get(case, n)
        for key, label in (("collider_visits", "objects visited"), ("overlap_tests", "hitbox tests")):
            scale = lambda d: round(d[key] / d["ticks"], 2)
            row = lower_is_better(f"{case}: {label}", scale(b), scale(n))
            # Compare the raw totals, so nothing hides behind rounding.
            if n[key] * b["ticks"] != b[key] * n["ticks"] and row[4] == OK:
                row = row[:4] + (WORSE if n[key] * b["ticks"] > b[key] * n["ticks"] else BETTER,)
            rows.append(row)
    return rows, ["Objects visited and hitbox tests per tick. Counted, not timed, so the same on every machine."]


def compare_instructions(now, base, limits):
    rows, lines = [], []
    limit = limits["instructions_percent"]
    same_compiler = not base or base.get("compiler") == now["compiler"]
    largest = (0.0, "")
    for case, n in now["cases"].items():
        new = n["per_iteration"]
        old = (base or {}).get("cases", {}).get(case, {}).get("per_iteration", new)
        delta = 100.0 * (new - old) / old if old else 0.0
        if abs(delta) > largest[0]:
            largest = (abs(delta), f"{delta:+.2f}% on {case}")
        verdict = OK if abs(delta) <= limit else (WORSE if delta > 0 else BETTER)
        if not same_compiler and verdict != OK:
            verdict = NOTE
        rows.append((f"{case} (per {n['unit']})", f"{old:,.0f}", f"{new:,.0f}", f"{delta:+.1f}%", verdict))
    lines.append(f"Largest move from the baseline: {largest[1] or 'none'}; {limit:g}% is allowed either way. "
                 f"Compiler: {now['compiler']}.")
    if not same_compiler:
        lines.append(f"The baseline was recorded with a different compiler ({base.get('compiler')}), so these are "
                     "shown but cannot fail the run. Record a new baseline on this compiler to switch the check back on.")
    return rows, lines


def compare_speed(now, base, limits):
    lines = ["| case | what it runs | speed on this machine |", "| --- | --- | --- |"]
    for case, n in now.items():
        if n["unit"] == "tick":
            lines.append(f"| {case} | {n['what']} | {1e3 / n['ns']:.2f} million ticks/s ({n['ns']:.0f} ns each) |")
        else:
            lines.append(f"| {case} | {n['what']} | {n['ns'] / 1e6:.1f} ms |")
    lines += ["", "Shared CI machines vary too much from run to run to compare these with a baseline; the "
                  "instruction counts do that job."]
    return [], lines


def compare_coverage(now, base, limits):
    rows = []
    limit = limits["coverage_points"]
    for key, label in (("line", "lines run by the tests"), ("branch", "branches taken by the tests")):
        new, old = now[key], (base or {}).get(key, now[key])
        verdict = OK if abs(new - old) <= limit else (WORSE if new < old else BETTER)
        if key == "branch" and verdict != OK:
            verdict = NOTE  # reported, but only line coverage can fail a run
        rows.append((label, f"{old:.1f}%", f"{new:.1f}%", f"{new - old:+.1f} points" if new != old else "", verdict))
    return rows, [f"The tests run {now['line']:.1f}% of the library's lines and {now['branch']:.1f}% of its branches. "
                  f"Line coverage may move {limit:g} points either way; branch coverage is shown only."]


def compare_mutation(now, base, limits):
    pinned, unpinned = set(now["pinned"]), set(now["unpinned"])
    total = len(pinned) + len(unpinned)
    rows = []
    if base:
        for name in base.get("pinned", []):
            if name in unpinned:
                rows.append((name, "pinned", "not pinned", "a 1% change now goes unnoticed", WORSE))
        for name in base.get("unpinned", []):
            if name in pinned:
                rows.append((name, "not pinned", "pinned", "a test now covers it", BETTER))
        known = set(base.get("pinned", [])) | set(base.get("unpinned", []))
        for name in sorted((pinned | unpinned) - known):
            rows.append((name, "", "pinned" if name in pinned else "not pinned", "new number", CHANGED))
        for name in sorted(known - pinned - unpinned - set(now.get("skipped", []))):
            rows.append((name, "listed", "", "number removed or renamed", CHANGED))
    lines = [f"{len(pinned)} of {total} physics numbers are pinned: changing one by 1% makes a test fail."]
    if now.get("skipped"):
        lines.append("Could not be tried: " + ", ".join(now["skipped"]) + ".")
    return rows, lines


COMPARE = {
    "scenarios": compare_scenarios, "sizes": compare_sizes, "table": compare_table, "work": compare_work,
    "instructions": compare_instructions, "speed": compare_speed, "coverage": compare_coverage,
    "mutation": compare_mutation,
}

# Sections whose tables are worth showing in full even when nothing moved.
ALWAYS_SHOW = {"sizes", "table", "instructions", "coverage"}


def to_baseline(section, measured):
    """What gets stored for a section. Per-file coverage and timings are not."""
    if section == "coverage":
        return {"line": measured["line"], "branch": measured["branch"]}
    if section == "mutation":
        return {"pinned": sorted(measured["pinned"]), "unpinned": sorted(measured["unpinned"])}
    return measured


# -------------------------------------------------------------- reporting ---

def table(rows, limit=40):
    out = ["| | baseline | now | change | |", "| --- | --- | --- | --- | --- |"]
    for name, old, new, change, verdict in rows[:limit]:
        out.append(f"| {name} | {old} | {new} | {change} | {MARK[verdict]} |")
    if len(rows) > limit:
        out.append(f"| ...and {len(rows) - limit} more | | | | |")
    return out


def report(results, baseline, limits):
    """Returns (markdown, number of problems, annotations).

    Annotations are (level, title, message) for GitHub to show on the run
    page: one per number that moved, and one line summing up the run.
    """
    body, worse, unrecorded, missing = [], 0, 0, []
    strict = limits["fail_on_unrecorded_improvement"]
    annotations, digest = [], []
    for section in SECTIONS:
        if section not in results:
            continue
        base = baseline.get(section)
        if base is None and section != "speed":
            missing.append(section)
        rows, lines = COMPARE[section](results[section], base, limits)
        moved = [r for r in rows if r[4] not in (OK, NOTE)]
        worse += sum(1 for r in moved if r[4] in (WORSE, CHANGED))
        unrecorded += sum(1 for r in moved if r[4] == BETTER)
        for name, old, new, change, verdict in moved:
            failing = verdict != BETTER or strict
            said = {WORSE: "got worse", BETTER: "improved but the baseline was not updated", CHANGED: "changed"}[verdict]
            was = f"{old} -> {new}" if str(old) != str(new) else str(new)
            annotations.append(("error" if failing else "notice", TITLES[section],
                                f"{name} {said}: {was}" + (f" ({change})" if change else "")))
        if lines and section in ("scenarios", "instructions", "coverage", "mutation"):
            digest.append(lines[0])
        body.append(f"### {TITLES[section]}")
        body.append("")
        shown = rows if section in ALWAYS_SHOW else moved
        if shown:
            body += table(shown) + [""]
        elif base is not None and section != "speed":
            body += ["Nothing moved.", ""]
        body += lines + [""]

    problems = worse + (unrecorded if strict else 0)
    head = ["## Performance checks", ""]
    if problems == 0:
        head.append("**Passed.** Every number matches `bench/baseline.json`.")
        if unrecorded:
            head.append(f"{unrecorded} improved; update the baseline to keep the gain.")
    else:
        parts = []
        if worse:
            parts.append(f"{worse} got worse or changed")
        if unrecorded and strict:
            parts.append(f"{unrecorded} improved without being recorded")
        head.append(f"**Failed:** {' and '.join(parts)}.")
        head.append("")
        head.append("If this is a mistake, fix the code. If it is what you meant to do, record the new numbers in "
                    "the same change: run `python3 bench/perf.py --update` and commit `bench/baseline.json`, or "
                    "take the ready-made file from this run's `baseline-candidate` artifact.")
    if missing:
        head.append("")
        head.append("No baseline yet for: " + ", ".join(missing) + ". These were measured but not checked.")
    # GitHub shows at most ten errors per step; keep the rest as a count.
    errors = [a for a in annotations if a[0] == "error"]
    if len(errors) > 9:
        annotations = [a for a in annotations if a[0] != "error"] + errors[:9] + [
            ("error", "Performance checks", f"...and {len(errors) - 9} more. The run summary lists them all.")]
    verdict = "Passed" if problems == 0 else "Failed"
    annotations.append(("notice", f"Performance checks: {verdict.lower()}", f"{verdict}. " + " ".join(digest)))
    return "\n".join(head + [""] + body), problems, annotations


def github_command(level, title, message):
    """One annotation in the format GitHub Actions reads from a step's output."""
    escape = lambda text: text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    title = escape(title).replace(":", "%3A").replace(",", "%2C")
    return f"::{level} title={title}::{escape(message)}"


# ------------------------------------------------------------------- main ---

def load_baseline():
    return json.loads(BASELINE.read_text()) if BASELINE.exists() else {}


def write_baseline(path, baseline, results):
    merged = dict(baseline)
    merged.setdefault("limits", dict(DEFAULT_LIMITS))
    for section, measured in results.items():
        if section != "speed":
            merged[section] = to_baseline(section, measured)
    # Short sections first; the long scenario list goes last.
    order = ["limits", "sizes", "table", "work", "instructions", "coverage", "mutation", "scenarios"]
    ordered = {k: merged[k] for k in order if k in merged}
    pathlib.Path(path).write_text(json.dumps(ordered, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=str(ROOT / "build"), help="where the project was built (default: build)")
    parser.add_argument("--sections", default="all", help="comma-separated: " + ", ".join(SECTIONS))
    parser.add_argument("--reps", type=int, default=5, help="timing repetitions for the speed section")
    parser.add_argument("--update", action="store_true", help="write the measured numbers to bench/baseline.json")
    parser.add_argument("--results", help="also save the raw measurements to this file")
    parser.add_argument("--summary", help="append the report to this file (GitHub's step summary)")
    parser.add_argument("--merge", nargs="+", metavar="RESULTS", help="measure nothing; combine saved result files")
    parser.add_argument("--candidate", help="write the baseline these results would give to this file")
    args = parser.parse_args()

    # The report uses a few symbols a Windows console would refuse to print.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")

    baseline = load_baseline()
    limits = {**DEFAULT_LIMITS, **baseline.get("limits", {})}

    results = {}
    if args.merge:
        for path in args.merge:
            results.update(json.loads(pathlib.Path(path).read_text()))
    else:
        wanted = SECTIONS if args.sections == "all" else [s.strip() for s in args.sections.split(",") if s.strip()]
        unknown = [s for s in wanted if s not in SECTIONS]
        if unknown:
            parser.error("unknown section: " + ", ".join(unknown))
        failed = []
        for section in wanted:
            print(f"measuring {section}...", file=sys.stderr, flush=True)
            try:
                results[section] = MEASURE[section](args)
            except MeasureError as error:
                failed.append(f"{section}: {error}")
        if failed:
            # Asking for everything on a machine without the tools is fine;
            # asking for a section by name and not getting it is an error.
            for line in failed:
                print(("skipped " if args.sections == "all" else "error: ") + line, file=sys.stderr)
            if args.sections != "all" or not results:
                return 2

    if args.results:
        pathlib.Path(args.results).write_text(json.dumps(results, indent=2) + "\n")
    if args.candidate:
        write_baseline(args.candidate, baseline, results)
    if args.merge:
        print(f"combined {', '.join(results)} from {len(args.merge)} result file(s)")
        return 0

    if args.update:
        write_baseline(BASELINE, baseline, results)
        print(f"baseline updated: {', '.join(s for s in results if s != 'speed')}")
        return 0

    text, problems, annotations = report(results, baseline, limits)
    print(text)
    if os.environ.get("GITHUB_ACTIONS") == "true":
        for annotation in annotations:
            print(github_command(*annotation))
    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as handle:
            handle.write(text + "\n")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
