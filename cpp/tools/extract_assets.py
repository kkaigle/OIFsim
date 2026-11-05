#!/usr/bin/env python3
"""
Iraq Theater Command — Native Asset & 3D Model Extractor
========================================================
Extracts all embedded theater geography, OpenStreetMap buildings (145,000+),
street & highway networks, elevation heightmap, satellite imagery, and glTF 2.0
(.glb) 3D models from `docs/index.html` into native binary/GLB files inside
`cpp/assets/` for the C++20 Mac & Windows engine.

Also dequantizes `KHR_mesh_quantization` vertex attributes (`SHORT` / `USHORT`)
into standard IEEE-754 `FLOAT` (5126) GLB buffers so native C++ `cgltf` / Raylib
loaders can load every `.glb` model and embedded texture directly.
"""

import base64
import json
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
HTML_PATH = ROOT_DIR / "docs" / "index.html"
ASSETS_DIR = ROOT_DIR / "cpp" / "assets"
MAP_DIR = ASSETS_DIR / "map"
MODELS_DIR = ASSETS_DIR / "models"


def jpeg_bytes_to_png(jpg_bytes: bytes) -> bytes:
    """Convert JPEG bytes to PNG bytes so Raylib builds without JPG enabled load every texture."""
    try:
        from PIL import Image
        import io
        im = Image.open(io.BytesIO(jpg_bytes))
        out = io.BytesIO()
        im.save(out, format="PNG")
        return out.getvalue()
    except Exception:
        pass
    if sys.platform == "darwin":
        with tempfile.TemporaryDirectory() as td:
            in_p = Path(td) / "in.jpg"
            out_p = Path(td) / "out.png"
            in_p.write_bytes(jpg_bytes)
            subprocess.run(
                ["/usr/bin/sips", "-s", "format", "png", str(in_p), "--out", str(out_p)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
            return out_p.read_bytes()
    return jpg_bytes


def w_proj(lon: float, lat: float):
    """Project (lon, lat) -> Iraq Theater Command world coordinates (x, y)."""
    return ((lon - 38.6) * 102.5, (37.6 - lat) * 122.0)


def vrd(buf: bytes, idx_ref: list) -> float:
    """Decode variable-length zigzag delta integer from OSMB street/road streams."""
    v = 0
    s = 0
    while True:
        b = buf[idx_ref[0]]
        idx_ref[0] += 1
        v += (b & 127) * (1 << s)
        s += 7
        if not (b & 128):
            break
    return -(v + 1) / 2.0 if (v % 2) else v / 2.0


def bsz(v: int) -> float:
    return v / 2.0 if v <= 200 else 100.0 + (v - 200) * 4.0


def dequantize_glb(raw_glb: bytes) -> bytes:
    """
    Convert a glTF 2.0 binary (.glb) using KHR_mesh_quantization (int16/uint16
    attributes) into a standard float32 GLB so cgltf / Raylib LoadModel() works
    natively without requiring KHR_mesh_quantization support.
    """
    if len(raw_glb) < 20:
        return raw_glb
    magic, version, total_len = struct.unpack_from("<4sII", raw_glb, 0)
    if magic != b"glTF" or version != 2:
        return raw_glb

    offset = 12
    json_data = None
    bin_data = b""
    while offset < len(raw_glb):
        chunk_len, chunk_type = struct.unpack_from("<I4s", raw_glb, offset)
        offset += 8
        chunk_bytes = raw_glb[offset : offset + chunk_len]
        offset += chunk_len
        if chunk_type == b"JSON":
            json_data = json.loads(chunk_bytes.decode("utf-8"))
        elif chunk_type == b"BIN\x00":
            bin_data = chunk_bytes

    if not json_data or not bin_data:
        return raw_glb

    # Remove KHR_mesh_quantization requirement after converting accessors to float32
    for ext_key in ("extensionsUsed", "extensionsRequired"):
        if ext_key in json_data:
            json_data[ext_key] = [
                e for e in json_data[ext_key] if e != "KHR_mesh_quantization"
            ]
            if not json_data[ext_key]:
                del json_data[ext_key]

    new_bin = bytearray(bin_data)
    accessors = json_data.get("accessors", [])
    buffer_views = json_data.get("bufferViews", [])

    type_counts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    comp_formats = {
        5120: ("b", 1, 127.0),
        5121: ("B", 1, 255.0),
        5122: ("h", 2, 32767.0),
        5123: ("H", 2, 65535.0),
    }

    # Check which accessors are used as vertex attributes (not indices)
    attr_accessor_indices = set()
    for mesh in json_data.get("meshes", []):
        for prim in mesh.get("primitives", []):
            for attr_name, acc_idx in prim.get("attributes", {}).items():
                attr_accessor_indices.add(acc_idx)

    for acc_idx in sorted(attr_accessor_indices):
        acc = accessors[acc_idx]
        ctype = acc.get("componentType")
        if ctype not in comp_formats:
            continue
        fmt_char, comp_size, norm_div = comp_formats[ctype]
        num_comps = type_counts.get(acc.get("type", "SCALAR"), 1)
        count = acc.get("count", 0)
        normalized = acc.get("normalized", False)

        bv_idx = acc["bufferView"]
        bv = buffer_views[bv_idx]
        base_offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
        stride = bv.get("byteStride", num_comps * comp_size)

        # Pad new_bin to 4-byte alignment
        while len(new_bin) % 4 != 0:
            new_bin.append(0)
        new_bv_offset = len(new_bin)

        unpack_fmt = "<" + (fmt_char * num_comps)
        pack_fmt = "<" + ("f" * num_comps)
        mins = [float("inf")] * num_comps
        maxs = [float("-inf")] * num_comps

        for i in range(count):
            pos = base_offset + i * stride
            vals = struct.unpack_from(unpack_fmt, bin_data, pos)
            if normalized:
                if fmt_char in ("b", "h"):
                    fvals = [max(v / norm_div, -1.0) for v in vals]
                else:
                    fvals = [v / norm_div for v in vals]
            else:
                fvals = [float(v) for v in vals]
            for c_i, fv in enumerate(fvals):
                if fv < mins[c_i]:
                    mins[c_i] = fv
                if fv > maxs[c_i]:
                    maxs[c_i] = fv
            new_bin.extend(struct.pack(pack_fmt, *fvals))

        new_bv_len = count * num_comps * 4
        new_bv_idx = len(buffer_views)
        buffer_views.append(
            {
                "buffer": 0,
                "byteOffset": new_bv_offset,
                "byteLength": new_bv_len,
                "target": 34962,
            }
        )
        acc["bufferView"] = new_bv_idx
        acc["byteOffset"] = 0
        acc["componentType"] = 5126  # FLOAT
        if "normalized" in acc:
            del acc["normalized"]
        acc["min"] = mins
        acc["max"] = maxs

    # Convert any embedded image/jpeg bufferViews to image/png for universal Raylib support
    for img in json_data.get("images", []):
        if img.get("mimeType") == "image/jpeg" and "bufferView" in img:
            bv = buffer_views[img["bufferView"]]
            off = bv.get("byteOffset", 0)
            ln = bv.get("byteLength", 0)
            jpg_b = bytes(bin_data[off : off + ln])
            png_b = jpeg_bytes_to_png(jpg_b)
            while len(new_bin) % 4 != 0:
                new_bin.append(0)
            new_off = len(new_bin)
            new_bin.extend(png_b)
            bv["byteOffset"] = new_off
            bv["byteLength"] = len(png_b)
            img["mimeType"] = "image/png"

    while len(new_bin) % 4 != 0:
        new_bin.append(0)
    json_data["buffers"][0]["byteLength"] = len(new_bin)

    json_bytes = json.dumps(json_data, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4 != 0:
        json_bytes += b" "

    total_size = 12 + 8 + len(json_bytes) + 8 + len(new_bin)
    out = bytearray()
    out.extend(struct.pack("<4sII", b"glTF", 2, total_size))
    out.extend(struct.pack("<I4s", len(json_bytes), b"JSON"))
    out.extend(json_bytes)
    out.extend(struct.pack("<I4s", len(new_bin), b"BIN\x00"))
    out.extend(new_bin)
    return bytes(out)


def build_house_glb(house_b64: str) -> bytes:
    """Convert the quantised HOUSE3D mesh into a standard glTF 2.0 .glb file."""
    u = base64.b64decode(house_b64)
    nV = struct.unpack_from("<H", u, 0)[0]
    nI = struct.unpack_from("<I", u, 2)[0]
    pv = []
    for i in range(nV):
        vx = struct.unpack_from("<H", u, 6 + (i * 3 + 0) * 2)[0] / 65535.0 - 0.5
        vy = struct.unpack_from("<H", u, 6 + (i * 3 + 1) * 2)[0] / 65535.0
        vz = struct.unpack_from("<H", u, 6 + (i * 3 + 2) * 2)[0] / 65535.0 - 0.5
        pv.append((vx, vy, vz))

    idx_off = 6 + nV * 6
    verts = []
    norms = []
    for t in range(0, nI, 3):
        i0 = struct.unpack_from("<H", u, idx_off + (t + 0) * 2)[0]
        i1 = struct.unpack_from("<H", u, idx_off + (t + 1) * 2)[0]
        i2 = struct.unpack_from("<H", u, idx_off + (t + 2) * 2)[0]
        p0, p1, p2 = pv[i0], pv[i1], pv[i2]
        ux, uy, uz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
        vx, vy, vz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
        nx = uy * vz - uz * vy
        ny = uz * vx - ux * vz
        nz = ux * vy - uy * vx
        nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        n = (nx / nl, ny / nl, nz / nl)
        verts.extend([p0, p1, p2])
        norms.extend([n, n, n])

    bin_buf = bytearray()
    mins = [float("inf")] * 3
    maxs = [float("-inf")] * 3
    for vx, vy, vz in verts:
        mins[0] = min(mins[0], vx)
        mins[1] = min(mins[1], vy)
        mins[2] = min(mins[2], vz)
        maxs[0] = max(maxs[0], vx)
        maxs[1] = max(maxs[1], vy)
        maxs[2] = max(maxs[2], vz)
        bin_buf.extend(struct.pack("<3f", vx, vy, vz))
    norm_off = len(bin_buf)
    for nx, ny, nz in norms:
        bin_buf.extend(struct.pack("<3f", nx, ny, nz))

    gltf = {
        "asset": {"version": "2.0", "generator": "ITC Asset Extractor"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "IraqiHouse", "mesh": 0}],
        "meshes": [
            {
                "name": "IraqiHouseMesh",
                "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "mode": 4}],
            }
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": len(verts),
                "type": "VEC3",
                "min": mins,
                "max": maxs,
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": len(norms),
                "type": "VEC3",
            },
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": norm_off, "target": 34962},
            {
                "buffer": 0,
                "byteOffset": norm_off,
                "byteLength": len(bin_buf) - norm_off,
                "target": 34962,
            },
        ],
        "buffers": [{"byteLength": len(bin_buf)}],
    }
    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4 != 0:
        json_bytes += b" "
    total_size = 12 + 8 + len(json_bytes) + 8 + len(bin_buf)
    out = bytearray()
    out.extend(struct.pack("<4sII", b"glTF", 2, total_size))
    out.extend(struct.pack("<I4s", len(json_bytes), b"JSON"))
    out.extend(json_bytes)
    out.extend(struct.pack("<I4s", len(bin_buf), b"BIN\x00"))
    out.extend(bin_buf)
    return bytes(out)


def main():
    print(f"[ITC Extractor] Reading {HTML_PATH} ...")
    html = HTML_PATH.read_text(encoding="utf-8")
    lines = html.splitlines()

    MAP_DIR.mkdir(parents=True, exist_ok=True)
    MODELS_DIR.mkdir(parents=True, exist_ok=True)

    # 1. Extract SAT_JPG (and convert to satellite.png for universal texture loading)
    m_sat = re.search(r'const SAT_JPG="data:image/jpeg;base64,([^"]+)";', html)
    if m_sat:
        sat_bytes = base64.b64decode(m_sat.group(1))
        sat_path = MAP_DIR / "satellite.jpg"
        sat_path.write_bytes(sat_bytes)
        sat_png_bytes = jpeg_bytes_to_png(sat_bytes)
        sat_png_path = MAP_DIR / "satellite.png"
        sat_png_path.write_bytes(sat_png_bytes)
        print(f"  -> Wrote {sat_png_path.relative_to(ROOT_DIR)} ({len(sat_png_bytes):,} bytes)")

    # 2. Extract ELEV_B64 (435x306 int16 heightmap)
    m_elev = re.search(r'const ELEV_B64="([^"]+)";', html)
    if m_elev:
        elev_bytes = base64.b64decode(m_elev.group(1))
        elev_path = MAP_DIR / "elevation.bin"
        elev_path.write_bytes(elev_bytes)
        print(f"  -> Wrote {elev_path.relative_to(ROOT_DIR)} ({len(elev_bytes):,} bytes)")

    # 3. Extract 3D glTF models from MODELS3D & HOUSE3D
    for line in lines:
        if line.startswith("const MODELS3D="):
            for m in re.finditer(
                r"([A-Za-z0-9_]+):\{len:([0-9.]+)(?:,raw:1)?(?:,turret:(?:'([^']+)'|null))?,b64:'([^']+)'\}",
                line,
            ):
                key, length_s, turret, b64_str = m.groups()
                raw_glb = base64.b64decode(b64_str)
                std_glb = dequantize_glb(raw_glb)
                glb_path = MODELS_DIR / f"{key}.glb"
                glb_path.write_bytes(std_glb)
                print(
                    f"  -> Wrote 3D model {glb_path.relative_to(ROOT_DIR)} ({len(std_glb):,} bytes, dequantized float32)"
                )
        elif line.startswith("const HOUSE3D="):
            m_h = re.search(r"const HOUSE3D='([^']+)';", line)
            if m_h:
                h_glb = build_house_glb(m_h.group(1))
                h_path = MODELS_DIR / "house.glb"
                h_path.write_bytes(h_glb)
                print(
                    f"  -> Wrote 3D model {h_path.relative_to(ROOT_DIR)} ({len(h_glb):,} bytes)"
                )

    # 4. Extract OSM & OSMB & Theater Geography into JSON + Binary buildings bundle
    osm = None
    osmb = None
    for line in lines:
        if line.startswith("const OSM="):
            osm = json.loads(line[len("const OSM=") : -1])
        elif line.startswith("const OSMB="):
            osmb = json.loads(line[len("const OSMB=") : -1])

    city_defs = [
        ("mosul", "Mosul", 43.13, 36.34, 1.4, "mixed", "FOB Marez"),
        ("erbil", "Erbil", 44.01, 36.19, 0.8, "kurd", "Erbil Air Base"),
        ("kirkuk", "Kirkuk", 44.39, 35.47, 0.7, "mixed", "FOB Warrior"),
        ("tikrit", "Tikrit", 43.68, 34.61, 0.15, "sunni", "FOB Speicher"),
        ("samarra", "Samarra", 43.88, 34.20, 0.2, "sunni", "FOB Brassfield-Mora"),
        ("baqubah", "Baqubah", 44.66, 33.75, 0.3, "sunni", "FOB Warhorse"),
        ("baghdad", "Baghdad", 44.36, 33.31, 5.5, "mixed", "Camp Victory"),
        ("fallujah", "Fallujah", 43.78, 33.35, 0.3, "sunni", "Camp Fallujah"),
        ("ramadi", "Ramadi", 43.30, 33.42, 0.4, "sunni", "Camp Blue Diamond"),
        ("haditha", "Haditha", 42.35, 34.13, 0.08, "sunni", "Haditha Dam"),
        ("qaim", "Al Qaim", 41.03, 34.37, 0.1, "sunni", "Camp Al Qaim"),
        ("rutbah", "Rutbah", 40.29, 33.04, 0.05, "sunni", "FOB Korean Village"),
        ("karbala", "Karbala", 44.02, 32.61, 0.5, "shia", "FOB Lima"),
        ("hillah", "Hillah", 44.42, 32.48, 0.5, "shia", "Camp Charlie"),
        ("najaf", "Najaf", 44.33, 32.0, 0.6, "shia", "FOB Duke"),
        ("diwaniyah", "Diwaniyah", 44.93, 31.99, 0.4, "shia", "Camp Echo"),
        ("kut", "Al Kut", 45.82, 32.51, 0.35, "shia", "FOB Delta"),
        ("amarah", "Amarah", 47.15, 31.84, 0.35, "shia", "Camp Abu Naji"),
        ("samawah", "Samawah", 45.28, 31.31, 0.2, "shia", "Camp Smitty"),
        ("nasiriyah", "Nasiriyah", 46.26, 31.04, 0.4, "shia", "Camp Adder"),
        ("basra", "Basra", 47.78, 30.51, 1.2, "shia", "Basra Air Station"),
        ("ummqasr", "Umm Qasr", 47.95, 30.03, 0.05, "shia", "Camp Bucca"),
        ("talafar", "Tal Afar", 42.45, 36.37, 0.2, "sunni", "FOB Sykes"),
        ("dohuk", "Dohuk", 42.99, 36.87, 0.3, "kurd", "FOB Dohuk"),
        ("sulay", "Sulaymaniyah", 45.43, 35.56, 0.7, "kurd", "FOB Sulaymaniyah"),
        ("baiji", "Baiji", 43.49, 34.93, 0.1, "sunni", "FOB Summerall"),
        ("balad", "Balad", 44.15, 34.02, 0.1, "mixed", "LSA Anaconda"),
        ("hit", "Hit", 42.82, 33.64, 0.05, "sunni", "FOB Hit"),
        ("iskan", "Iskandariyah", 44.35, 32.89, 0.05, "mixed", "FOB Kalsu"),
        ("rawah", "Rawah", 41.92, 34.48, 0.03, "sunni", "FOB Rawah"),
    ]

    border_ll = [
        [42.36, 37.11], [42.8, 37.33], [43.4, 37.28], [44.1, 37.3], [44.79, 37.15],
        [44.95, 36.8], [45.3, 36.6], [45.5, 36.0], [46.1, 35.8], [46.2, 35.2],
        [45.7, 34.85], [45.5, 34.6], [45.7, 34.3], [45.5, 34.0], [45.9, 33.6],
        [46.2, 33.2], [46.6, 32.9], [47.4, 32.45], [47.7, 32.0], [47.85, 31.5],
        [47.7, 31.0], [48.0, 30.95], [48.0, 30.5], [48.55, 29.95], [47.95, 30.0],
        [47.7, 30.1], [47.15, 30.0], [46.55, 29.1], [44.7, 29.2], [42.1, 31.1],
        [40.4, 31.95], [39.2, 32.15], [38.8, 33.37], [40.9, 34.4], [41.2, 34.8],
        [41.3, 35.6], [41.35, 36.4], [41.8, 36.6],
    ]
    kuwait_ll = [
        [47.7, 30.1], [47.95, 30.0], [48.0, 29.95], [48.2, 29.6], [48.0, 29.35],
        [48.4, 28.55], [47.7, 28.53], [46.55, 29.1], [47.15, 30.0],
    ]
    gulf_ll = [
        [48.55, 29.95], [49.8, 29.9], [50.6, 28.0], [48.4, 28.0], [48.4, 28.55],
        [48.0, 29.35], [48.2, 29.6], [48.0, 29.95],
    ]

    cities_out = []
    blds_bin = bytearray()
    streets_bin = bytearray()
    total_blds = 0
    total_streets = 0

    for cid, name, lon, lat, pop, reg, fob in city_defs:
        if osm and cid in osm.get("cc", {}):
            lon, lat = osm["cc"][cid]
        wx, wy = w_proj(lon, lat)
        kx = 1.0 / (111.32 * math.cos(lat * math.pi / 180.0) / 102.5) / 1000.0
        ky = 1.0 / (111.0 / 122.0) / 1000.0

        # Decode OSM buildings for this city
        b_b64 = osmb["B"].get(cid, "") if osmb else ""
        b_raw = base64.b64decode(b_b64) if b_b64 else b""
        b_count = len(b_raw) // 8
        b_offset = total_blds

        for i in range(0, b_count * 8, 8):
            X, Y, bw_u8, bh_u8, ang_u8, kl_u8 = struct.unpack_from("<hhBBBB", b_raw, i)
            X *= 2
            Y *= 2
            w_m = bsz(bw_u8)
            h_m = bsz(bh_u8)
            ang = ang_u8 * math.pi / 180.0
            k = kl_u8 & 15
            lv = kl_u8 >> 4
            cx = wx + X * kx
            cy = wy - Y * ky
            ww = w_m * kx
            wh = h_m * ky
            h_norm = min(1.0, lv / 8.0) if lv else (((i * 7919) % 100) / 100.0)
            blds_bin.extend(struct.pack("<6fBBxx", cx, cy, ww, wh, ang, h_norm, k, lv))
        total_blds += b_count

        # Decode OSM streets for this city
        s_b64 = osmb["S"].get(cid, "") if osmb else ""
        s_raw = base64.b64decode(s_b64) if s_b64 else b""
        idx_ref = [0]
        s_offset = total_streets
        s_count = 0
        while idx_ref[0] < len(s_raw):
            cls = s_raw[idx_ref[0]]
            idx_ref[0] += 1
            n_pts = int(vrd(s_raw, idx_ref))
            X = 0.0
            Y = 0.0
            prev = None
            for _ in range(n_pts):
                X += vrd(s_raw, idx_ref)
                Y += vrd(s_raw, idx_ref)
                px = wx + X * 2.0 * kx
                py = wy - Y * 2.0 * ky
                if prev is not None:
                    streets_bin.extend(
                        struct.pack("<4fI", prev[0], prev[1], px, py, cls)
                    )
                    s_count += 1
                prev = (px, py)
        total_streets += s_count

        cities_out.append(
            {
                "id": cid,
                "name": name,
                "lon": lon,
                "lat": lat,
                "x": wx,
                "y": wy,
                "pop": pop,
                "sect": reg,
                "fob": fob,
                "bldOffset": b_offset,
                "bldCount": b_count,
                "streetOffset": s_offset,
                "streetCount": s_count,
            }
        )

    # Write binary buildings & streets
    blds_path = MAP_DIR / "osm_buildings.bin"
    blds_path.write_bytes(blds_bin)
    print(
        f"  -> Wrote {blds_path.relative_to(ROOT_DIR)} ({total_blds:,} buildings, {len(blds_bin):,} bytes)"
    )

    streets_path = MAP_DIR / "osm_streets.bin"
    streets_path.write_bytes(streets_bin)
    print(
        f"  -> Wrote {streets_path.relative_to(ROOT_DIR)} ({total_streets:,} street segments, {len(streets_bin):,} bytes)"
    )

    # Write theater metadata JSON
    highways_out = []
    if osm and "hw" in osm:
        for hw in osm["hw"]:
            pts_w = [list(w_proj(lon, lat)) for lon, lat in hw["pts"]]
            highways_out.append(
                {
                    "name": hw["n"],
                    "kind": "msr"
                    if hw["n"].startswith("MSR") or hw["n"] in ("Highway 6", "Highway 2")
                    else "minor",
                    "pts": pts_w,
                }
            )

    rivers_out = []
    if osm and "rivers" in osm:
        for rv in osm["rivers"]:
            rivers_out.append([list(w_proj(lon, lat)) for lon, lat in rv])

    lakes_out = []
    if osm and "lakes" in osm:
        for lk_name, lk_poly in osm["lakes"]:
            lakes_out.append(
                {"name": lk_name, "poly": [list(w_proj(lon, lat)) for lon, lat in lk_poly]}
            )

    theater_json = {
        "geo": {"lo0": 30.0, "lo1": 59.0, "la0": 20.6, "la1": 41.0, "cols": 435, "rows": 306, "ppd": 15.0},
        "border": [list(w_proj(lon, lat)) for lon, lat in border_ll],
        "kuwait": [list(w_proj(lon, lat)) for lon, lat in kuwait_ll],
        "gulf": [list(w_proj(lon, lat)) for lon, lat in gulf_ll],
        "cities": cities_out,
        "highways": highways_out,
        "rivers": rivers_out,
        "lakes": lakes_out,
    }
    theater_path = MAP_DIR / "theater.json"
    theater_path.write_text(json.dumps(theater_json, indent=2), encoding="utf-8")
    print(f"  -> Wrote {theater_path.relative_to(ROOT_DIR)} ({theater_path.stat().st_size:,} bytes)")

    # Also write compact binary theater.bin for zero-dependency C++ loading
    tbin = bytearray()
    def pack_str(s: str):
        b = s.encode("utf-8")
        tbin.extend(struct.pack("<I", len(b)))
        tbin.extend(b)

    def pack_pts(pts):
        tbin.extend(struct.pack("<I", len(pts)))
        for x, y in pts:
            tbin.extend(struct.pack("<2f", x, y))

    tbin.extend(struct.pack("<4sI", b"ITCT", 1))
    pack_pts(theater_json["border"])
    pack_pts(theater_json["kuwait"])
    pack_pts(theater_json["gulf"])

    tbin.extend(struct.pack("<I", len(cities_out)))
    for c in cities_out:
        pack_str(c["id"])
        pack_str(c["name"])
        pack_str(c["sect"])
        pack_str(c["fob"])
        tbin.extend(
            struct.pack(
                "<5f4I",
                c["lon"],
                c["lat"],
                c["x"],
                c["y"],
                c["pop"],
                c["bldOffset"],
                c["bldCount"],
                c["streetOffset"],
                c["streetCount"],
            )
        )

    tbin.extend(struct.pack("<I", len(highways_out)))
    for hw in highways_out:
        pack_str(hw["name"])
        pack_str(hw["kind"])
        pack_pts(hw["pts"])

    tbin.extend(struct.pack("<I", len(rivers_out)))
    for rv in rivers_out:
        pack_pts(rv)

    tbin.extend(struct.pack("<I", len(lakes_out)))
    for lk in lakes_out:
        pack_str(lk["name"])
        pack_pts(lk["poly"])

    tbin_path = MAP_DIR / "theater.bin"
    tbin_path.write_bytes(tbin)
    print(f"  -> Wrote {tbin_path.relative_to(ROOT_DIR)} ({len(tbin):,} bytes)")
    print("[ITC Extractor] Done!")


if __name__ == "__main__":
    main()
