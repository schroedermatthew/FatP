#!/usr/bin/env python3
"""Download Fresnel explanation images from Wikimedia Commons into artifacts/."""

import urllib.request
from pathlib import Path

OUT = Path(__file__).resolve().parent
UA = "FatP-Teaching-Artifact/1.0 (educational; local)"

ITEMS = [
    (
        "Fresnel_reflection_curve.png",
        "https://upload.wikimedia.org/wikipedia/commons/thumb/a/a4/Fresnel_reflection.svg/1280px-Fresnel_reflection.svg.png",
        "Ulflund, Wikimedia Commons (CC BY-SA 3.0)",
    ),
    (
        "Fresnel_interface_rays.png",
        "https://upload.wikimedia.org/wikipedia/commons/thumb/f/fd/Refraction_internal_reflection_diagram.svg/1280px-Refraction_internal_reflection_diagram.svg.png",
        "Lasse Havelund / Pieter Kuiper, Wikimedia Commons (CC BY-SA 3.0)",
    ),
    (
        "Fresnel_equations_reflectance.png",
        "https://upload.wikimedia.org/wikipedia/commons/thumb/3/34/Fresnel_equations_-_reflectance.svg/960px-Fresnel_equations_-_reflectance.svg.png",
        "Cepheiden, Wikimedia Commons (CC BY-SA 3.0)",
    ),
]

for name, url, _ in ITEMS:
    dest = OUT / name
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as resp:
        dest.write_bytes(resp.read())
    print("wrote", dest, dest.stat().st_size)