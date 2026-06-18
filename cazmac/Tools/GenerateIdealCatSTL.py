#!/usr/bin/env python3
"""Generate an ideal domestic-cat target STL for the CazMac asset pipeline."""

from __future__ import annotations

import math
import os
import struct
import zlib
from dataclasses import dataclass


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "cazmac", "Assets", "Generated")
STL_PATH = os.path.join(OUT_DIR, "ideal-domestic-cat-target.stl")
PREVIEW_PATH = os.path.join(OUT_DIR, "ideal-domestic-cat-target-preview.png")
NOTES_PATH = os.path.join(OUT_DIR, "ideal-domestic-cat-target.md")


@dataclass(frozen=True)
class Vec3:
    x: float
    y: float
    z: float

    def __add__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, value: float) -> "Vec3":
        return Vec3(self.x * value, self.y * value, self.z * value)

    def dot(self, other: "Vec3") -> float:
        return self.x * other.x + self.y * other.y + self.z * other.z

    def cross(self, other: "Vec3") -> "Vec3":
        return Vec3(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x,
        )

    def length(self) -> float:
        return math.sqrt(self.dot(self))

    def normalized(self) -> "Vec3":
        length = self.length()
        if length <= 1.0e-9:
            return Vec3(0.0, 0.0, 1.0)
        return self * (1.0 / length)


Triangle = tuple[Vec3, Vec3, Vec3]


class Mesh:
    def __init__(self) -> None:
        self.triangles: list[Triangle] = []

    def add(self, a: Vec3, b: Vec3, c: Vec3) -> None:
        self.triangles.append((a, b, c))

    def extend(self, triangles: list[Triangle]) -> None:
        self.triangles.extend(triangles)

    def bounds(self) -> tuple[Vec3, Vec3]:
        points = [point for tri in self.triangles for point in tri]
        return (
            Vec3(min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)),
            Vec3(max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)),
        )


def ellipsoid(mesh: Mesh, center: Vec3, radius: Vec3, lat_steps: int = 14, lon_steps: int = 28) -> None:
    rows: list[list[Vec3]] = []
    for lat in range(lat_steps + 1):
        phi = -math.pi / 2.0 + math.pi * lat / lat_steps
        row = []
        for lon in range(lon_steps):
            theta = 2.0 * math.pi * lon / lon_steps
            row.append(
                Vec3(
                    center.x + radius.x * math.cos(phi) * math.cos(theta),
                    center.y + radius.y * math.cos(phi) * math.sin(theta),
                    center.z + radius.z * math.sin(phi),
                )
            )
        rows.append(row)

    for lat in range(lat_steps):
        for lon in range(lon_steps):
            a = rows[lat][lon]
            b = rows[lat][(lon + 1) % lon_steps]
            c = rows[lat + 1][(lon + 1) % lon_steps]
            d = rows[lat + 1][lon]
            if lat != 0:
                mesh.add(a, b, d)
            if lat != lat_steps - 1:
                mesh.add(b, c, d)


def basis_for_axis(axis: Vec3) -> tuple[Vec3, Vec3, Vec3]:
    w = axis.normalized()
    helper = Vec3(0.0, 0.0, 1.0)
    if abs(w.dot(helper)) > 0.92:
        helper = Vec3(0.0, 1.0, 0.0)
    u = w.cross(helper).normalized()
    v = w.cross(u).normalized()
    return u, v, w


def cylinder(mesh: Mesh, start: Vec3, end: Vec3, radius: float, steps: int = 18) -> None:
    axis = end - start
    u, v, _ = basis_for_axis(axis)
    ring_a = []
    ring_b = []
    for i in range(steps):
        angle = 2.0 * math.pi * i / steps
        offset = u * (math.cos(angle) * radius) + v * (math.sin(angle) * radius)
        ring_a.append(start + offset)
        ring_b.append(end + offset)
    for i in range(steps):
        j = (i + 1) % steps
        mesh.add(ring_a[i], ring_b[i], ring_a[j])
        mesh.add(ring_a[j], ring_b[i], ring_b[j])
        mesh.add(start, ring_a[j], ring_a[i])
        mesh.add(end, ring_b[i], ring_b[j])


def capsule(mesh: Mesh, start: Vec3, end: Vec3, radius: float, steps: int = 18) -> None:
    cylinder(mesh, start, end, radius, steps)
    ellipsoid(mesh, start, Vec3(radius, radius, radius), 8, steps)
    ellipsoid(mesh, end, Vec3(radius, radius, radius), 8, steps)


def tapered_capsule(mesh: Mesh, points: list[Vec3], radii: list[float]) -> None:
    for index in range(len(points) - 1):
        radius = (radii[index] + radii[index + 1]) * 0.5
        capsule(mesh, points[index], points[index + 1], radius, 18)


def ear(mesh: Mesh, base_center: Vec3, side: float) -> None:
    base_front = base_center + Vec3(0.035, side * 0.035, -0.008)
    base_back = base_center + Vec3(-0.055, side * 0.040, -0.004)
    base_inner = base_center + Vec3(-0.010, side * -0.020, 0.000)
    tip = base_center + Vec3(0.000, side * 0.022, 0.145)
    mesh.add(base_front, base_inner, tip)
    mesh.add(base_inner, base_back, tip)
    mesh.add(base_back, base_front, tip)
    mesh.add(base_front, base_back, base_inner)


def build_cat_mesh() -> Mesh:
    mesh = Mesh()

    # A side-on domestic-cat silhouette from the stock set: long low trunk,
    # distinct chest and haunch masses, compact head, and planted paw line.
    ellipsoid(mesh, Vec3(0.00, 0.00, 0.32), Vec3(0.37, 0.105, 0.125), 18, 36)
    ellipsoid(mesh, Vec3(0.20, 0.00, 0.34), Vec3(0.17, 0.112, 0.145), 16, 32)
    ellipsoid(mesh, Vec3(-0.19, 0.00, 0.315), Vec3(0.18, 0.120, 0.135), 16, 32)
    ellipsoid(mesh, Vec3(0.00, 0.00, 0.405), Vec3(0.33, 0.090, 0.045), 10, 32)

    capsule(mesh, Vec3(0.27, 0.00, 0.39), Vec3(0.39, 0.00, 0.415), 0.060, 20)
    ellipsoid(mesh, Vec3(0.49, 0.00, 0.425), Vec3(0.135, 0.090, 0.100), 16, 32)
    ellipsoid(mesh, Vec3(0.600, 0.00, 0.400), Vec3(0.060, 0.058, 0.042), 12, 24)
    ellipsoid(mesh, Vec3(0.655, 0.00, 0.392), Vec3(0.018, 0.024, 0.016), 8, 16)
    ellipsoid(mesh, Vec3(0.592, 0.042, 0.438), Vec3(0.020, 0.012, 0.014), 8, 16)
    ellipsoid(mesh, Vec3(0.592, -0.042, 0.438), Vec3(0.020, 0.012, 0.014), 8, 16)
    ear(mesh, Vec3(0.450, 0.050, 0.500), 1.0)
    ear(mesh, Vec3(0.450, -0.050, 0.500), -1.0)

    leg_specs = [
        (Vec3(0.220, 0.070, 0.305), Vec3(0.205, 0.070, 0.155), Vec3(0.235, 0.075, 0.045), 0.030),
        (Vec3(0.220, -0.070, 0.305), Vec3(0.205, -0.070, 0.155), Vec3(0.235, -0.075, 0.045), 0.030),
        (Vec3(-0.210, 0.082, 0.300), Vec3(-0.255, 0.082, 0.165), Vec3(-0.225, 0.088, 0.045), 0.034),
        (Vec3(-0.210, -0.082, 0.300), Vec3(-0.255, -0.082, 0.165), Vec3(-0.225, -0.088, 0.045), 0.034),
    ]
    for upper, knee, paw, radius in leg_specs:
        capsule(mesh, upper, knee, radius, 18)
        capsule(mesh, knee, paw, radius * 0.82, 18)
        ellipsoid(mesh, paw + Vec3(0.022, 0.0, -0.020), Vec3(0.065, 0.038, 0.022), 10, 20)

    tail_points = [
        Vec3(-0.365, 0.000, 0.350),
        Vec3(-0.500, 0.000, 0.365),
        Vec3(-0.635, 0.000, 0.395),
        Vec3(-0.760, 0.000, 0.425),
    ]
    tapered_capsule(mesh, tail_points, [0.034, 0.030, 0.024, 0.017])

    # Subtle shoulder and cheek markers preserve the cat read in plain STL.
    ellipsoid(mesh, Vec3(0.185, 0.112, 0.305), Vec3(0.055, 0.026, 0.040), 8, 18)
    ellipsoid(mesh, Vec3(0.185, -0.112, 0.305), Vec3(0.055, 0.026, 0.040), 8, 18)
    ellipsoid(mesh, Vec3(0.555, 0.045, 0.382), Vec3(0.030, 0.018, 0.020), 8, 16)
    ellipsoid(mesh, Vec3(0.555, -0.045, 0.382), Vec3(0.030, 0.018, 0.020), 8, 16)

    return mesh


def normal_for(triangle: Triangle) -> Vec3:
    a, b, c = triangle
    return (b - a).cross(c - a).normalized()


def write_ascii_stl(mesh: Mesh, path: str) -> None:
    with open(path, "w", encoding="ascii") as out:
        out.write("solid caz_ideal_domestic_cat_target\n")
        for triangle in mesh.triangles:
            normal = normal_for(triangle)
            out.write(f"  facet normal {normal.x:.7g} {normal.y:.7g} {normal.z:.7g}\n")
            out.write("    outer loop\n")
            for point in triangle:
                out.write(f"      vertex {point.x:.7g} {point.y:.7g} {point.z:.7g}\n")
            out.write("    endloop\n")
            out.write("  endfacet\n")
        out.write("endsolid caz_ideal_domestic_cat_target\n")


def shade_triangle(triangle: Triangle) -> int:
    light = Vec3(0.4, -0.6, 0.7).normalized()
    normal = normal_for(triangle)
    shade = 0.45 + max(0.0, normal.dot(light)) * 0.45
    return max(70, min(225, int(shade * 255)))


def project(point: Vec3) -> tuple[float, float, float]:
    u = point.x * 1.05 - point.y * 0.48
    v = point.z + point.y * 0.22
    depth = point.y * 1.4 - point.x * 0.08 - point.z * 0.02
    return u, v, depth


def write_preview(mesh: Mesh, path: str, width: int = 1100, height: int = 720) -> None:
    projected = []
    min_u = min_v = float("inf")
    max_u = max_v = float("-inf")
    for triangle in mesh.triangles:
        pts = [project(point) for point in triangle]
        projected.append((sum(p[2] for p in pts) / 3.0, pts, shade_triangle(triangle)))
        for u, v, _ in pts:
            min_u, max_u = min(min_u, u), max(max_u, u)
            min_v, max_v = min(min_v, v), max(max_v, v)

    margin = 58
    scale = min((width - margin * 2) / (max_u - min_u), (height - margin * 2) / (max_v - min_v))

    def to_screen(p: tuple[float, float, float]) -> tuple[float, float]:
        u, v, _ = p
        x = margin + (u - min_u) * scale
        y = height - margin - (v - min_v) * scale
        return x, y

    pixels = bytearray([238, 240, 235] * width * height)

    def put(x: int, y: int, value: int) -> None:
        if 0 <= x < width and 0 <= y < height:
            idx = (y * width + x) * 3
            pixels[idx] = value
            pixels[idx + 1] = value
            pixels[idx + 2] = value

    def edge(a: tuple[float, float], b: tuple[float, float], c: tuple[float, float]) -> float:
        return (c[0] - a[0]) * (b[1] - a[1]) - (c[1] - a[1]) * (b[0] - a[0])

    for _, pts, shade in sorted(projected, key=lambda item: item[0]):
        a, b, c = [to_screen(p) for p in pts]
        min_x = max(0, math.floor(min(a[0], b[0], c[0])))
        max_x = min(width - 1, math.ceil(max(a[0], b[0], c[0])))
        min_y = max(0, math.floor(min(a[1], b[1], c[1])))
        max_y = min(height - 1, math.ceil(max(a[1], b[1], c[1])))
        area = edge(a, b, c)
        if abs(area) < 1.0e-6:
            continue
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                p = (x + 0.5, y + 0.5)
                w0 = edge(b, c, p)
                w1 = edge(c, a, p)
                w2 = edge(a, b, p)
                if (w0 >= 0 and w1 >= 0 and w2 >= 0) or (w0 <= 0 and w1 <= 0 and w2 <= 0):
                    put(x, y, shade)

    raw = b"".join(b"\x00" + pixels[y * width * 3 : (y + 1) * width * 3] for y in range(height))
    png = b"\x89PNG\r\n\x1a\n"

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as out:
        out.write(png)


def write_notes(mesh: Mesh, path: str) -> None:
    bounds_min, bounds_max = mesh.bounds()
    with open(path, "w", encoding="utf-8") as out:
        out.write("# Ideal Domestic Cat Target STL\n\n")
        out.write("Generated from `cazmac/Tools/GenerateIdealCatSTL.py` as a morphology target for a future 3D Caz droid body.\n\n")
        out.write("The shape is informed by the `stock/` domestic-cat references: long low trunk, separate shoulder and haunch volumes, compact forward-looking head, triangular ears, short muzzle, planted paws, and a full cat-length tail.\n\n")
        out.write("This is not the current CazMac renderer mesh. It is an ideal target asset for replacing the existing 2D procedural body with a real 3D cat-like model.\n\n")
        out.write("## Generated Files\n\n")
        out.write("- `ideal-domestic-cat-target.stl`: review mesh, standing on all fours, head looking forward.\n")
        out.write("- `ideal-domestic-cat-target-preview.png`: simple orthographic preview generated from the STL triangles.\n\n")
        out.write("## Mesh Stats\n\n")
        out.write(f"- Triangles: {len(mesh.triangles)}\n")
        out.write(f"- Bounds: x {bounds_min.x:.3f}..{bounds_max.x:.3f}, y {bounds_min.y:.3f}..{bounds_max.y:.3f}, z {bounds_min.z:.3f}..{bounds_max.z:.3f}\n")


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    mesh = build_cat_mesh()
    write_ascii_stl(mesh, STL_PATH)
    write_preview(mesh, PREVIEW_PATH)
    write_notes(mesh, NOTES_PATH)
    bounds_min, bounds_max = mesh.bounds()
    print(f"wrote {STL_PATH}")
    print(f"wrote {PREVIEW_PATH}")
    print(f"triangles={len(mesh.triangles)}")
    print(
        "bounds="
        f"({bounds_min.x:.3f},{bounds_min.y:.3f},{bounds_min.z:.3f}).."
        f"({bounds_max.x:.3f},{bounds_max.y:.3f},{bounds_max.z:.3f})"
    )


if __name__ == "__main__":
    main()
