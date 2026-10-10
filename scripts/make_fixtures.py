#!/usr/bin/env python3
"""Regenerates tests/fixtures. Run from anywhere: python3 scripts/make_fixtures.py

The fixtures are synthetic levels built here, not levels taken from the game.
Each encoded fixture has a matching .raw.txt holding the text it must decode to.
"""

import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from gdlevel import encode, gmd, level, obj  # noqa: E402

OUT = pathlib.Path(__file__).parent.parent / "tests" / "fixtures"


def write(name: str, text: str) -> None:
    (OUT / name).write_text(text, encoding="utf-8", newline="")


def simple() -> str:
    return level(
        [
            obj(1, 315, 15),
            obj(8, 345, 15, k6=90),
            obj(1, 375, 45, k32=2, k57="3.7.12"),
            obj(36, 405, 75, k4=1, k5=1),
            obj(1, 435, 15, k128=1.5, k129=0.5),
            obj(31, 600, 15, kA2=1, kA4=2, kA3=1),
        ],
        kA2=0,
        kA4=0,
    )


def big() -> str:
    """A few thousand objects, enough to force dynamic Huffman blocks."""
    rng = random.Random(22)
    objects = []
    for i in range(4000):
        objects.append(
            obj(
                rng.choice([1, 1, 1, 8, 39, 103, 36, 35, 12, 13, 200, 1332, 9999]),
                15 + 30 * i + rng.choice([0, 0, 7.5, 15]),
                15 + 30 * rng.randrange(0, 9),
                k6=rng.choice([0, 0, 90, 180, 270, 45]),
            )
        )
    return level(objects, kA2=1, kA4=3, kA3=1, kA11=1)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    raw = simple()
    write("simple.raw.txt", raw)
    write("simple.gzip.txt", encode(raw))
    write("simple.zlib.txt", encode(raw, wrapper="zlib"))
    write("simple.stored.txt", encode(raw, level=0))  # stored (uncompressed) blocks
    write("simple.gmd", gmd("Fixture Level", encode(raw)))

    raw = big()
    write("big.raw.txt", raw)
    write("big.gzip.txt", encode(raw))
    write("big.fast.txt", encode(raw, level=1))

    print(f"wrote fixtures to {OUT}")


if __name__ == "__main__":
    main()
