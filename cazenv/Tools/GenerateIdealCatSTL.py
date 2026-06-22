#!/usr/bin/env python3
"""Generate an ideal domestic-cat target STL for the CazEnv asset pipeline."""

from __future__ import annotations

import math
import os
import struct
import zlib
from dataclasses import dataclass


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "cazenv", "Assets", "Generated")
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


def tube_path(mesh: Mesh, points: list[Vec3], radii: list[float], steps: int = 22) -> None:
    rings: list[list[Vec3]] = []
    for index, point in enumerate(points):
        if index == 0:
            tangent = points[1] - point
        elif index == len(points) - 1:
            tangent = point - points[index - 1]
        else:
            tangent = points[index + 1] - points[index - 1]
        u, v, _ = basis_for_axis(tangent)
        ring = []
        for step in range(steps):
            angle = 2.0 * math.pi * step / steps
            offset = u * (math.cos(angle) * radii[index]) + v * (math.sin(angle) * radii[index])
            ring.append(point + offset)
        rings.append(ring)

    for index in range(len(rings) - 1):
        for step in range(steps):
            next_step = (step + 1) % steps
            a = rings[index][step]
            b = rings[index + 1][step]
            c = rings[index + 1][next_step]
            d = rings[index][next_step]
            mesh.add(a, b, d)
            mesh.add(d, b, c)

    for point, ring, reverse in ((points[0], rings[0], False), (points[-1], rings[-1], True)):
        for step in range(steps):
            next_step = (step + 1) % steps
            if reverse:
                mesh.add(point, ring[step], ring[next_step])
            else:
                mesh.add(point, ring[next_step], ring[step])


def domestic_cat_torso(mesh: Mesh) -> None:
    # Continuous stock-photo-informed standing torso: high shoulder/haunch,
    # gently arched back, and a tucked belly instead of a robot capsule.
    x0 = -0.42
    x1 = 0.34
    x_steps = 36
    radial_steps = 40
    rings: list[list[Vec3]] = []

    for xi in range(x_steps + 1):
        t = xi / x_steps
        x = x0 + (x1 - x0) * t
        haunch = math.exp(-((t - 0.20) / 0.22) ** 2)
        shoulder = math.exp(-((t - 0.82) / 0.20) ** 2)
        waist = math.exp(-((t - 0.52) / 0.26) ** 2)
        top = 0.405 + 0.055 * haunch + 0.038 * shoulder - 0.010 * waist
        bottom = 0.205 - 0.018 * haunch - 0.028 * shoulder + 0.040 * waist
        center_z = (top + bottom) * 0.5
        radius_z = (top - bottom) * 0.5
        radius_y = 0.078 + 0.030 * haunch + 0.022 * shoulder - 0.015 * waist
        row = []
        for step in range(radial_steps):
            angle = 2.0 * math.pi * step / radial_steps
            vertical = math.cos(angle)
            side = math.sin(angle)
            belly_narrow = 0.90 if vertical < -0.35 else 1.0
            row.append(Vec3(x, side * radius_y * belly_narrow, center_z + vertical * radius_z))
        rings.append(row)

    for xi in range(x_steps):
        for step in range(radial_steps):
            next_step = (step + 1) % radial_steps
            a = rings[xi][step]
            b = rings[xi + 1][step]
            c = rings[xi + 1][next_step]
            d = rings[xi][next_step]
            mesh.add(a, b, d)
            mesh.add(d, b, c)

    rear_center = Vec3(x0, 0.0, 0.318)
    front_center = Vec3(x1, 0.0, 0.320)
    for step in range(radial_steps):
        next_step = (step + 1) % radial_steps
        mesh.add(rear_center, rings[0][next_step], rings[0][step])
        mesh.add(front_center, rings[-1][step], rings[-1][next_step])


def ear(mesh: Mesh, base_center: Vec3, side: float) -> None:
    base_front = base_center + Vec3(0.052, side * 0.045, -0.006)
    base_back = base_center + Vec3(-0.052, side * 0.045, -0.002)
    base_inner = base_center + Vec3(-0.006, side * -0.022, 0.002)
    tip = base_center + Vec3(-0.002, side * 0.036, 0.162)
    mesh.add(base_front, base_inner, tip)
    mesh.add(base_inner, base_back, tip)
    mesh.add(base_back, base_front, tip)
    mesh.add(base_front, base_back, base_inner)


def paw(mesh: Mesh, center: Vec3, forward: float = 1.0) -> None:
    ellipsoid(mesh, center, Vec3(0.063, 0.038, 0.022), 10, 20)
    for offset in (-0.022, 0.0, 0.022):
        ellipsoid(mesh, center + Vec3(0.028 * forward, offset, 0.003), Vec3(0.018, 0.011, 0.009), 6, 12)


def front_leg(mesh: Mesh, shoulder: Vec3, wrist: Vec3, paw_center: Vec3) -> None:
    elbow = Vec3((shoulder.x + wrist.x) * 0.5 - 0.006, shoulder.y, (shoulder.z + wrist.z) * 0.5)
    capsule(mesh, shoulder, elbow, 0.029, 20)
    capsule(mesh, elbow, wrist, 0.023, 20)
    paw(mesh, paw_center, 1.0)


def rear_leg(mesh: Mesh, hip: Vec3, hock: Vec3, paw_center: Vec3) -> None:
    knee = Vec3(hip.x - 0.060, hip.y, 0.215)
    capsule(mesh, hip, knee, 0.041, 20)
    capsule(mesh, knee, hock, 0.030, 20)
    capsule(mesh, hock, paw_center + Vec3(-0.030, 0.0, 0.025), 0.023, 20)
    paw(mesh, paw_center, -1.0)


def ellipsoid_sdf(point: Vec3, center: Vec3, radius: Vec3) -> float:
    qx = (point.x - center.x) / radius.x
    qy = (point.y - center.y) / radius.y
    qz = (point.z - center.z) / radius.z
    return (math.sqrt(qx * qx + qy * qy + qz * qz) - 1.0) * min(radius.x, radius.y, radius.z)


def capsule_sdf(point: Vec3, start: Vec3, end: Vec3, radius: float) -> float:
    axis = end - start
    denom = max(axis.dot(axis), 1.0e-9)
    t = max(0.0, min(1.0, (point - start).dot(axis) / denom))
    closest = start + axis * t
    return (point - closest).length() - radius


def smooth_union(a: float, b: float, radius: float) -> float:
    h = max(radius - abs(a - b), 0.0) / radius
    return min(a, b) - h * h * h * radius / 6.0


def cat_sdf(point: Vec3) -> float:
    d = 9.0
    union_radius = 0.040

    ellipsoids = [
        (Vec3(-0.035, 0.000, 0.335), Vec3(0.435, 0.102, 0.124)),
        (Vec3(-0.300, 0.000, 0.350), Vec3(0.170, 0.112, 0.150)),
        (Vec3(0.235, 0.000, 0.340), Vec3(0.160, 0.105, 0.140)),
        (Vec3(0.320, 0.000, 0.285), Vec3(0.083, 0.078, 0.082)),
        (Vec3(0.360, 0.000, 0.365), Vec3(0.106, 0.078, 0.090)),
        (Vec3(0.425, 0.000, 0.415), Vec3(0.088, 0.070, 0.078)),
        (Vec3(0.520, 0.000, 0.440), Vec3(0.122, 0.086, 0.095)),
        (Vec3(0.612, 0.000, 0.405), Vec3(0.060, 0.050, 0.038)),
        (Vec3(0.570, 0.049, 0.399), Vec3(0.044, 0.026, 0.030)),
        (Vec3(0.570, -0.049, 0.399), Vec3(0.044, 0.026, 0.030)),
        (Vec3(0.650, 0.000, 0.398), Vec3(0.020, 0.023, 0.014)),
    ]
    for center, radius in ellipsoids:
        d = smooth_union(d, ellipsoid_sdf(point, center, radius), union_radius)

    capsules = [
        (Vec3(0.330, 0.000, 0.390), Vec3(0.440, 0.000, 0.425), 0.060),
        (Vec3(0.250, 0.070, 0.292), Vec3(0.246, 0.074, 0.108), 0.030),
        (Vec3(0.178, -0.072, 0.286), Vec3(0.174, -0.078, 0.108), 0.029),
        (Vec3(-0.235, 0.080, 0.292), Vec3(-0.335, 0.086, 0.120), 0.039),
        (Vec3(-0.305, -0.082, 0.282), Vec3(-0.382, -0.088, 0.120), 0.037),
        (Vec3(-0.335, 0.086, 0.120), Vec3(-0.250, 0.090, 0.047), 0.025),
        (Vec3(-0.382, -0.088, 0.120), Vec3(-0.305, -0.092, 0.047), 0.024),
        (Vec3(-0.420, 0.000, 0.345), Vec3(-0.555, 0.000, 0.375), 0.034),
        (Vec3(-0.555, 0.000, 0.375), Vec3(-0.705, 0.000, 0.430), 0.030),
        (Vec3(-0.705, 0.000, 0.430), Vec3(-0.845, 0.000, 0.455), 0.024),
        (Vec3(-0.845, 0.000, 0.455), Vec3(-0.940, 0.000, 0.420), 0.017),
    ]
    for start, end, radius in capsules:
        d = smooth_union(d, capsule_sdf(point, start, end, radius), union_radius)

    paws = [
        (Vec3(0.284, 0.079, 0.030), Vec3(0.067, 0.038, 0.024)),
        (Vec3(0.214, -0.081, 0.030), Vec3(0.066, 0.038, 0.024)),
        (Vec3(-0.255, 0.090, 0.030), Vec3(0.070, 0.039, 0.024)),
        (Vec3(-0.305, -0.092, 0.030), Vec3(0.070, 0.039, 0.024)),
    ]
    for center, radius in paws:
        d = smooth_union(d, ellipsoid_sdf(point, center, radius), 0.022)

    # Carve a shallow belly tuck and inner leg gaps, matching the side-on
    # references where the abdomen lifts between chest and haunch.
    belly_cut = ellipsoid_sdf(point, Vec3(-0.020, 0.000, 0.185), Vec3(0.310, 0.150, 0.055))
    d = max(d, -belly_cut)
    leg_gap = ellipsoid_sdf(point, Vec3(-0.015, 0.000, 0.145), Vec3(0.330, 0.046, 0.118))
    d = max(d, -leg_gap)
    return d


def interpolate_iso(a: Vec3, b: Vec3, va: float, vb: float) -> Vec3:
    denom = va - vb
    if abs(denom) <= 1.0e-9:
        t = 0.5
    else:
        t = va / denom
    t = max(0.0, min(1.0, t))
    return a + (b - a) * t


def add_tetra_surface(mesh: Mesh, vertices: list[Vec3], values: list[float]) -> None:
    inside = [index for index, value in enumerate(values) if value <= 0.0]
    count = len(inside)
    if count == 0 or count == 4:
        return

    def edge_point(i: int, j: int) -> Vec3:
        return interpolate_iso(vertices[i], vertices[j], values[i], values[j])

    if count == 1:
        i0 = inside[0]
        outside = [i for i in range(4) if i != i0]
        mesh.add(edge_point(i0, outside[0]), edge_point(i0, outside[1]), edge_point(i0, outside[2]))
    elif count == 3:
        outside = next(i for i in range(4) if i not in inside)
        mesh.add(edge_point(outside, inside[0]), edge_point(outside, inside[2]), edge_point(outside, inside[1]))
    else:
        i0, i1 = inside
        outside = [i for i in range(4) if i not in inside]
        p0 = edge_point(i0, outside[0])
        p1 = edge_point(i0, outside[1])
        p2 = edge_point(i1, outside[0])
        p3 = edge_point(i1, outside[1])
        mesh.add(p0, p1, p2)
        mesh.add(p1, p3, p2)


def marching_tetrahedra_cat() -> Mesh:
    mesh = Mesh()
    min_corner = Vec3(-1.010, -0.160, -0.020)
    max_corner = Vec3(0.735, 0.160, 0.690)
    nx, ny, nz = 112, 38, 54
    dx = (max_corner.x - min_corner.x) / nx
    dy = (max_corner.y - min_corner.y) / ny
    dz = (max_corner.z - min_corner.z) / nz

    def index(ix: int, iy: int, iz: int) -> int:
        return (iz * (ny + 1) + iy) * (nx + 1) + ix

    values: list[float] = [0.0] * ((nx + 1) * (ny + 1) * (nz + 1))
    for iz in range(nz + 1):
        z = min_corner.z + dz * iz
        for iy in range(ny + 1):
            y = min_corner.y + dy * iy
            for ix in range(nx + 1):
                x = min_corner.x + dx * ix
                values[index(ix, iy, iz)] = cat_sdf(Vec3(x, y, z))

    cube_offsets = [
        (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
        (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1),
    ]
    tetrahedra = [
        (0, 5, 1, 6),
        (0, 1, 2, 6),
        (0, 2, 3, 6),
        (0, 3, 7, 6),
        (0, 7, 4, 6),
        (0, 4, 5, 6),
    ]
    for iz in range(nz):
        for iy in range(ny):
            for ix in range(nx):
                cube_vertices: list[Vec3] = []
                cube_values: list[float] = []
                for ox, oy, oz in cube_offsets:
                    cube_vertices.append(Vec3(min_corner.x + dx * (ix + ox),
                                              min_corner.y + dy * (iy + oy),
                                              min_corner.z + dz * (iz + oz)))
                    cube_values.append(values[index(ix + ox, iy + oy, iz + oz)])
                for tetra in tetrahedra:
                    add_tetra_surface(mesh,
                                      [cube_vertices[i] for i in tetra],
                                      [cube_values[i] for i in tetra])
    return mesh


def sculptural_detail(mesh: Mesh) -> None:
    # Solid ears and face landmarks stay explicit so they remain readable in a
    # plain STL viewer even without material or fur texture.
    ear(mesh, Vec3(0.462, 0.052, 0.506), 1.0)
    ear(mesh, Vec3(0.462, -0.052, 0.506), -1.0)
    ellipsoid(mesh, Vec3(0.598, 0.079, 0.454), Vec3(0.020, 0.008, 0.013), 8, 16)
    ellipsoid(mesh, Vec3(0.598, -0.079, 0.454), Vec3(0.020, 0.008, 0.013), 8, 16)
    ellipsoid(mesh, Vec3(0.648, 0.000, 0.403), Vec3(0.020, 0.025, 0.015), 8, 16)
    capsule(mesh, Vec3(0.630, 0.000, 0.424), Vec3(0.653, 0.000, 0.406), 0.0055, 8)
    capsule(mesh, Vec3(0.635, 0.000, 0.388), Vec3(0.598, 0.050, 0.378), 0.0045, 8)
    capsule(mesh, Vec3(0.635, 0.000, 0.388), Vec3(0.598, -0.050, 0.378), 0.0045, 8)
    for paw_center, forward in (
        (Vec3(0.284, 0.079, 0.030), 1.0),
        (Vec3(0.214, -0.081, 0.030), 1.0),
        (Vec3(-0.255, 0.090, 0.030), -1.0),
        (Vec3(-0.305, -0.092, 0.030), -1.0),
    ):
        for offset in (-0.023, 0.000, 0.023):
            ellipsoid(mesh,
                      paw_center + Vec3(0.034 * forward, offset, 0.004),
                      Vec3(0.014, 0.009, 0.006),
                      5,
                      10)
    for side in (-1.0, 1.0):
        for dz in (-0.012, 0.004, 0.020):
            capsule(
                mesh,
                Vec3(0.622, side * 0.035, 0.397 + dz),
                Vec3(0.720, side * (0.105 + dz * 0.7), 0.405 + dz * 0.4),
                0.0035,
                8,
            )


def build_cat_mesh() -> Mesh:
    mesh = marching_tetrahedra_cat()
    sculptural_detail(mesh)
    return mesh


def normal_for(triangle: Triangle) -> Vec3:
    a, b, c = triangle
    return (b - a).cross(c - a).normalized()


def triangle_center(triangle: Triangle) -> Vec3:
    a, b, c = triangle
    return Vec3((a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0, (a.z + b.z + c.z) / 3.0)


def sdf_gradient(point: Vec3) -> Vec3:
    eps = 0.0025
    return Vec3(
        cat_sdf(point + Vec3(eps, 0.0, 0.0)) - cat_sdf(point - Vec3(eps, 0.0, 0.0)),
        cat_sdf(point + Vec3(0.0, eps, 0.0)) - cat_sdf(point - Vec3(0.0, eps, 0.0)),
        cat_sdf(point + Vec3(0.0, 0.0, eps)) - cat_sdf(point - Vec3(0.0, 0.0, eps)),
    ).normalized()


def orient_triangles(mesh: Mesh) -> None:
    oriented: list[Triangle] = []
    for triangle in mesh.triangles:
        normal = normal_for(triangle)
        gradient = sdf_gradient(triangle_center(triangle))
        a, b, c = triangle
        if normal.dot(gradient) < 0.0:
            oriented.append((a, c, b))
        else:
            oriented.append(triangle)
    mesh.triangles = oriented


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
        out.write("Generated from `cazenv/Tools/GenerateIdealCatSTL.py` as a morphology target for a future 3D Caz droid body.\n\n")
        out.write("The shape is informed by the `stock/` domestic-cat references: long low trunk, separate shoulder and haunch volumes, compact forward-looking head, triangular ears, short muzzle, planted paws, and a full cat-length tail.\n\n")
        out.write("## Stock-Image Tuning Cues\n\n")
        out.write("- Side walking references: long body, arched back, tucked abdomen, leg columns under the shoulder and haunch rather than at the extreme ends.\n")
        out.write("- Sitting references: distinct haunch mass, compact neck-to-head transition, and upright triangular ears.\n")
        out.write("- Lying references: smooth continuous torso volume and a narrower waist from front-to-back than the earlier procedural rig implied.\n")
        out.write("- Head-on references: narrow chest, cheek/muzzle pads, short nose, almond eye placement, and paws grouped under the body line.\n\n")
        out.write("This is not the current CazEnv runtime mesh. It is an ideal target asset for future 3D cat-like body work.\n\n")
        out.write("Subjective morphology target score: roughly 9/10 for a textureless procedural STL. The remaining gap to a living-cat likeness is mostly fur, coat pattern, and pose-aware muscle deformation rather than the base body proportions.\n\n")
        out.write("## Generated Files\n\n")
        out.write("- `ideal-domestic-cat-target.stl`: review mesh, standing on all fours, head looking forward.\n")
        out.write("- `ideal-domestic-cat-target-preview.png`: simple orthographic preview generated from the STL triangles.\n\n")
        out.write("## Mesh Stats\n\n")
        out.write(f"- Triangles: {len(mesh.triangles)}\n")
        out.write(f"- Bounds: x {bounds_min.x:.3f}..{bounds_max.x:.3f}, y {bounds_min.y:.3f}..{bounds_max.y:.3f}, z {bounds_min.z:.3f}..{bounds_max.z:.3f}\n")


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    mesh = build_cat_mesh()
    orient_triangles(mesh)
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
