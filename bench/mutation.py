#!/usr/bin/env python3
"""Which physics numbers do the tests actually pin down?

Changes one number at a time by 1% in a private copy of the source, rebuilds
the physics tests against it and runs them. If a test fails, the number is
"pinned": a wrong value could not slip in unnoticed. If every test still
passes, nothing is checking that number.

The numbers tried are every literal defined in include/gdpf/constants.hpp and
every cell of the pad and orb strength tables in src/sim.cpp. The repository
itself is never modified.

    python3 bench/mutation.py                 # print the result
    python3 bench/mutation.py --out file.json # and save it

Needs a POSIX shell environment with g++ or clang++ (set CXX to choose).
"""

import argparse
import concurrent.futures
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONSTANTS = ROOT / "include" / "gdpf" / "constants.hpp"
SIM = ROOT / "src" / "sim.cpp"
NUMBER = re.compile(r"(?<![\w.])(-?\d+\.\d+|-?\d+)(f?)(?![\w.])")
# Defined in constants.hpp but not physics: nothing in the simulator reads them.
NOT_PHYSICS = {"kTicksPerSecond", "kVelToUnitsPerSecond", "kNoCeiling"}
MODES = ["cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"]


def nudged(token, suffix, percent):
    return repr(float(token) * (1 + percent / 100.0)) + suffix


def constant_mutants(percent):
    """(name, mutated header text) for each literal in a constant's definition."""
    lines = CONSTANTS.read_text().split("\n")
    array = None  # (name, next index) while inside a multi-line array
    for i, line in enumerate(lines):
        code = line.split("//")[0]
        start = re.match(r"\s*inline constexpr \w+ (\w+)(?:\[\w+\])? = (.*)", code)
        targets = []
        if start:
            name = start.group(1)
            if name in NOT_PHYSICS:
                continue
            if code.rstrip().endswith("{"):
                array = (name, 0)
                continue
            found = list(NUMBER.finditer(code, start.start(2)))
            targets = [(f"{name}[{k}]" if len(found) > 1 else name, m) for k, m in enumerate(found)]
        elif array:
            if code.strip().startswith("}"):
                array = None
                continue
            name, index = array
            found = list(NUMBER.finditer(code))
            targets = [(f"{name}[{index + k}]", m) for k, m in enumerate(found)]
            array = (name, index + max(1, code.count(",")))
        for label, m in targets:
            changed = line[:m.start()] + nudged(m.group(1), m.group(2), percent) + line[m.end():]
            yield label, "\n".join(lines[:i] + [changed] + lines[i + 1:])


def table_mutants(percent):
    """(name, mutated sim.cpp text) for each number in the pad and orb tables.

    Cells that say kUnknown have no number to change, and the wave ignores
    pads and orbs, so its column is skipped.
    """
    lines = SIM.read_text().split("\n")
    table = None
    for i, line in enumerate(lines):
        opening = re.match(r"\s*constexpr double (k\w+Factor)\[", line)
        if opening:
            table = opening.group(1)
            continue
        if table and line.strip().startswith("};"):
            table = None
            continue
        row = re.match(r"\s*/\*\s*(\w+)\s*\*/\s*\{(.*)\},", line) if table else None
        if not row:
            continue
        offset = row.start(2)
        for column, cell in enumerate(re.finditer(r"[^,]+", row.group(2))):
            token = cell.group(0).strip()
            mode = MODES[column] if column < len(MODES) else str(column)
            if mode == "wave" or not NUMBER.fullmatch(token):
                continue
            at = offset + cell.start() + len(cell.group(0)) - len(cell.group(0).lstrip())
            changed = line[:at] + nudged(token, "", percent) + line[at + len(token):]
            yield f"{table}[{row.group(1)}][{mode}]", "\n".join(lines[:i] + [changed] + lines[i + 1:])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", help="write the result as JSON to this file")
    parser.add_argument("--percent", type=float, default=1.0, help="how far to move each number (default 1)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    args = parser.parse_args()

    compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        sys.exit("mutation.py: no C++ compiler found (set CXX)")

    # Physics tests: the ones that drive the simulator.
    tests = [t for t in sorted((ROOT / "tests").glob("test_*.cpp"))
             if re.search(r'#include "gdpf/(sim|constants)\.hpp"', t.read_text())]
    if not tests:
        sys.exit("mutation.py: no test includes gdpf/sim.hpp")
    sources = sorted((ROOT / "src").glob("*.cpp"))
    # Only files that see constants.hpp need rebuilding for each change.
    rebuilt = [s for s in sources if s == SIM or '"gdpf/constants.hpp"' in s.read_text()]
    flags = ["-std=c++20", "-O0", "-ffp-contract=off", f'-DGDPF_FIXTURES="{ROOT / "tests" / "fixtures"}"']

    mutants = [("header", name, text) for name, text in constant_mutants(args.percent)]
    mutants += [("sim", name, text) for name, text in table_mutants(args.percent)]

    work = pathlib.Path(tempfile.mkdtemp(prefix="gdpf-mutation-"))
    try:
        objects = []
        for source in sources:
            if source in rebuilt:
                continue
            objects.append(str(work / (source.stem + ".o")))
            subprocess.run([compiler, *flags, f"-I{ROOT / 'include'}", "-c", str(source), "-o", objects[-1]], check=True)

        def build_and_test(where, header_text=None, sim_text=None):
            """True if the tests pass, False if they fail, None if it does not compile."""
            include = where / "include"
            shutil.copytree(ROOT / "include", include)
            if header_text is not None:
                (include / "gdpf" / "constants.hpp").write_text(header_text)
            compiled = []
            for source in rebuilt:
                copy = where / source.name
                copy.write_text(sim_text if (sim_text is not None and source == SIM) else source.read_text())
                compiled.append(str(copy))
            for test in tests:
                exe = where / test.stem
                built = subprocess.run([compiler, *flags, f"-I{include}", str(test), *compiled, *objects, "-o", str(exe)],
                                       capture_output=True, text=True)
                if built.returncode != 0:
                    return None
                try:
                    if subprocess.run([str(exe)], capture_output=True, timeout=120).returncode != 0:
                        return False
                except subprocess.TimeoutExpired:
                    return False
            return True

        clean = work / "clean"
        clean.mkdir()
        if build_and_test(clean) is not True:
            sys.exit("mutation.py: the tests do not pass before changing anything")

        def try_one(index):
            kind, _, text = mutants[index]
            where = work / f"m{index}"
            where.mkdir()
            outcome = build_and_test(where, header_text=text if kind == "header" else None,
                                     sim_text=text if kind == "sim" else None)
            shutil.rmtree(where, ignore_errors=True)
            return outcome

        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            outcomes = list(pool.map(try_one, range(len(mutants))))
    finally:
        shutil.rmtree(work, ignore_errors=True)

    result = {
        "percent": args.percent,
        "pinned": [m[1] for m, o in zip(mutants, outcomes) if o is False],
        "unpinned": [m[1] for m, o in zip(mutants, outcomes) if o is True],
        "skipped": [m[1] for m, o in zip(mutants, outcomes) if o is None],
    }
    total = len(result["pinned"]) + len(result["unpinned"])
    print(f"{len(result['pinned'])} of {total} physics numbers are pinned by the tests "
          f"(a {args.percent:g}% change makes one fail).")
    if result["unpinned"]:
        print("Not pinned: " + ", ".join(result["unpinned"]))
    if result["skipped"]:
        print("Did not compile when changed: " + ", ".join(result["skipped"]))
    if args.out:
        pathlib.Path(args.out).write_text(json.dumps(result, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
