#!/usr/bin/env python3
"""Build, encode and decode Geometry Dash level strings.

As a library:

    from gdlevel import level, obj, encode
    raw = level([obj(1, 315, 15), obj(8, 345, 15)], kA4=0)
    text = encode(raw)

As a tool:

    python3 gdlevel.py decode level.txt     # encoded -> key,value text
    python3 gdlevel.py encode level.txt     # key,value text -> encoded
"""

import base64
import gzip
import sys
import zlib


def encode(raw: str, wrapper: str = "gzip", level: int = 9) -> str:
    """Encodes a decoded level string the way the game stores it."""
    data = raw.encode()
    if wrapper == "gzip":
        packed = gzip.compress(data, compresslevel=level, mtime=0)
    elif wrapper == "zlib":
        packed = zlib.compress(data, level)
    else:
        raise ValueError(f"unknown wrapper {wrapper!r}")
    return base64.urlsafe_b64encode(packed).decode()


def decode(text: str) -> str:
    """Decodes an encoded level string. Accepts gzip and zlib wrappers."""
    packed = base64.urlsafe_b64decode(text.strip() + "=" * (-len(text.strip()) % 4))
    # wbits 47 = auto-detect gzip or zlib.
    return zlib.decompress(packed, 47).decode()


def obj(obj_id: int, x: float, y: float, **props) -> str:
    """One object. Extra properties are given by key number: obj(1, 0, 15, k6=90)."""
    parts = [1, obj_id, 2, x, 3, y]
    for key, value in props.items():
        parts += [key.lstrip("k") if key[1:].isdigit() else key, value]
    return ",".join(_fmt(p) for p in parts)


def level(objects, **header) -> str:
    """A decoded level string. Header keys are given by name: level(objs, kA2=1)."""
    head = {"kA13": 0, "kA15": 0, "kA16": 0}
    head.update(header)
    head_text = ",".join(f"{k},{_fmt(v)}" for k, v in head.items())
    return ";".join([head_text, *objects]) + ";"


def gmd(name: str, encoded: str) -> str:
    """Wraps an encoded level string the way a .gmd export does."""
    return (
        '<?xml version="1.0"?><plist version="1.0" gjver="2.0"><dict>'
        f"<k>kCEK</k><i>4</i><k>k2</k><s>{name}</s><k>k4</k><s>{encoded}</s>"
        "<k>k5</k><s>gdpf</s></dict></plist>"
    )


def _fmt(value) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def main(argv) -> int:
    if len(argv) != 3 or argv[1] not in ("encode", "decode"):
        print(__doc__, file=sys.stderr)
        return 1
    with open(argv[2], encoding="utf-8") as handle:
        text = handle.read()
    print(encode(text.strip()) if argv[1] == "encode" else decode(text))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
