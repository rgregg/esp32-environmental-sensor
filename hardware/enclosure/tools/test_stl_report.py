#!/usr/bin/env python3
"""Unit tests for stl_report.py. Run: python3 tools/test_stl_report.py"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import stl_report  # noqa: E402


def box_triangles(x0, y0, z0, x1, y1, z1):
    """12 outward-facing triangles of an axis-aligned box."""
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    faces = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
             (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    return [[v[a], v[b], v[c]] for a, b, c in faces]


def write_ascii(path, tris):
    with open(path, "w") as f:
        f.write("solid t\n")
        for t in tris:
            f.write(" facet normal 0 0 0\n  outer loop\n")
            for p in t:
                f.write(f"   vertex {p[0]} {p[1]} {p[2]}\n")
            f.write("  endloop\n endfacet\n")
        f.write("endsolid t\n")


def write_binary(path, tris):
    with open(path, "wb") as f:
        f.write(b"\0" * 80 + struct.pack("<I", len(tris)))
        for t in tris:
            f.write(struct.pack("<12fH", 0, 0, 0, *t[0], *t[1], *t[2], 0))


class StlReportTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def run_report(self, name):
        return subprocess.run([sys.executable, os.path.join(HERE, "stl_report.py"),
                               os.path.join(self.dir, name)], capture_output=True, text=True)

    def test_ascii_cube_volume_fails(self):
        write_ascii(os.path.join(self.dir, "c.stl"), box_triangles(0, 0, 0, 10, 10, 10))
        r = self.run_report("c.stl")
        self.assertEqual(r.returncode, 1)
        self.assertIn("volume 1000.000", r.stdout)
        self.assertIn("clash 1000.00 mm^3 at x 0.00..10.00", r.stdout)

    def test_binary_cube_volume(self):
        write_binary(os.path.join(self.dir, "b.stl"), box_triangles(1, 2, 3, 3, 4, 5))
        tris = stl_report.triangles(os.path.join(self.dir, "b.stl"))
        self.assertAlmostEqual(abs(sum(stl_report.signed_volume(t) for t in tris)), 8.0, places=4)

    def test_touching_faces_pass(self):
        # a zero-thickness box: what an intersection of two touching solids yields
        write_ascii(os.path.join(self.dir, "flat.stl"), box_triangles(0, 0, 5, 10, 10, 5))
        r = self.run_report("flat.stl")
        self.assertEqual(r.returncode, 0)
        self.assertNotIn("clash", r.stdout)


if __name__ == "__main__":
    unittest.main()
