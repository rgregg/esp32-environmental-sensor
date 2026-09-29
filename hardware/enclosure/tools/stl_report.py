#!/usr/bin/env python3
"""Report the enclosed volume of an STL and the bounding boxes of its
non-degenerate connected parts. Used by check.sh to tell real clashes
(solid overlap) from faces that merely touch (zero volume).

Usage: stl_report.py FILE.stl [--max-volume MM3]
Exit 0 if volume <= max-volume (default 0.01 mm^3), else 1.
"""
import struct
import sys


def triangles(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:5] == b"solid" and b"facet" in data[:300]:
        verts = [tuple(map(float, line.split()[1:4]))
                 for line in data.decode().splitlines()
                 if line.strip().startswith("vertex")]
        return [verts[i:i + 3] for i in range(0, len(verts), 3)]
    count = struct.unpack("<I", data[80:84])[0]
    tris = []
    for i in range(count):
        f = struct.unpack("<12f", data[84 + i * 50:84 + i * 50 + 48])
        tris.append([f[3:6], f[6:9], f[9:12]])
    return tris


def signed_volume(tri):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = tri
    return (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6.0


def parts(tris):
    parent = {}

    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    key = lambda v: tuple(round(c, 3) for c in v)
    for t in tris:
        k = [key(v) for v in t]
        for v in k[1:]:
            parent[find(v)] = find(k[0])
    groups = {}
    for t in tris:
        groups.setdefault(find(key(t[0])), []).append(t)
    return groups.values()


def main():
    path = sys.argv[1]
    limit = float(sys.argv[sys.argv.index("--max-volume") + 1]) if "--max-volume" in sys.argv else 0.01
    tris = triangles(path)
    total = abs(sum(signed_volume(t) for t in tris))
    print(f"volume {total:.3f} mm^3")
    for group in parts(tris):
        vol = abs(sum(signed_volume(t) for t in group))
        if vol <= limit:
            continue
        pts = [v for t in group for v in t]
        lo = [min(p[i] for p in pts) for i in range(3)]
        hi = [max(p[i] for p in pts) for i in range(3)]
        print(f"  clash {vol:.2f} mm^3 at x {lo[0]:.2f}..{hi[0]:.2f}"
              f"  y {lo[1]:.2f}..{hi[1]:.2f}  z {lo[2]:.2f}..{hi[2]:.2f}")
    sys.exit(0 if total <= limit else 1)


if __name__ == "__main__":
    main()
