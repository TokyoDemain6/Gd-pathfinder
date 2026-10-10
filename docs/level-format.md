# Level format

Only what the parser relies on. The format has many more keys.

## Encoding

A saved level string is URL-safe base64 around a gzip stream (older data uses
zlib). Decoded, it is plain text:

```
kS38,...,kA2,0,kA4,0,...;1,1,2,315,3,15;1,8,2,345,3,15,6,90;...
```

Segments are separated by `;`. The first is the header. Each of the others is
one object. Every segment is a flat list of `key,value` pairs.

`parse_level()` accepts the decoded text, the encoded text, or a `.gmd`
export (an XML file whose `k4` entry is the encoded string and `k2` the name).

## Header keys

| Key | Meaning |
| --- | --- |
| `kA2` | Starting gamemode: 0 cube, 1 ship, 2 ball, 3 UFO, 4 wave, 5 robot, 6 spider, 7 swing |
| `kA3` | Start mini |
| `kA4` | Starting speed: **0 = 1x, 1 = 0.5x**, 2 = 2x, 3 = 3x, 4 = 4x |
| `kA8` | Start in dual mode |
| `kA10` | Two-player mode |
| `kA11` | Start with gravity flipped |
| `kA22` | Platformer level |

A start position object (ID 31) carries the same keys inside the object.

## Object keys

| Key | Meaning |
| --- | --- |
| 1 | Object ID |
| 2, 3 | X and Y of the object's centre |
| 4, 5 | Flipped horizontally, vertically |
| 6 | Rotation in degrees, clockwise |
| 32 | Scale |
| 128, 129 | Scale X, scale Y |
| 57 | Group IDs, separated by `.` |

The parser multiplies key 32 into both 128 and 129. Whether the game ever
writes all three for one object has not been checked.

All other keys are kept in `LevelObject::raw` and can be read with `prop()`.

## Not verified

The `.gmd` layout and the header keys other than `kA2`, `kA3`, `kA4` and
`kA11` are from memory and have not been tested against a real export.
