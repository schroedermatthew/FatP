#!/usr/bin/env python3
"""Download remaining art-history images from non-Wikipedia sources."""

import time
import urllib.request
from pathlib import Path

OUT = Path(r"C:\Users\mtthw\Desktop\AI Projects\FatP\Teaching\GPU\artifacts\Art_History")
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"

DOWNLOADS = {
    # smn.it gallery (Masaccio Holy Trinity)
    "smn_00_masaccio_trinity_overview.jpg": "https://www.datocms-assets.com/43/1500632206-smn-00-masaccio-trinita_aq06311.jpg",
    "smn_01_masaccio_trinity_firenze.jpg": "https://www.datocms-assets.com/43/1500632277-smn-01-masaccio-la-trinita-firenze-01.jpg",
    "smn_02_masaccio_trinity.jpg": "https://www.datocms-assets.com/43/1500632320-smn-02-masaccio-la-trinita.jpg",
    "smn_03_masaccio_trinity_detail.jpg": "https://www.datocms-assets.com/43/1500632364-smn-03-masaccio-la-trinita.jpg",
    "smn_04_masaccio_trinity_christ_detail.jpg": "https://www.datocms-assets.com/43/1500632397-smn-04-masaccio-la-trinita-cristo-in-croce-dett-copia.jpg",
    "smn_05_masaccio_trinity_mary_john.jpg": "https://www.datocms-assets.com/43/1500632424-smn-05-masaccio-la-trinita-con-maria-giovanni.jpg",
    "smn_06_masaccio_trinity_john_detail.jpg": "https://www.datocms-assets.com/43/1500632490-smn-06-masaccio-la-trinita-san-giovanni-dett.jpg",
    "smn_07_masaccio_trinity_patron_detail.jpg": "https://www.datocms-assets.com/43/1500632526-smn-07-masaccio-la-trinita-berto-di-bartolomeo-dett-copia.jpg",
    # Masaccio Theophilus fresco (Brunelleschi at far right) — wga.hu
    "Masaccio_Theophilus_Brunelleschi_fresco.jpg": "https://www.wga.hu/art/m/masaccio/brancacc/st_peter/theo_pet.jpg",
    # School of Athens details — Wikimedia Commons
    "Raphael_School_of_Athens_Raphael_self_portrait.jpg": "https://upload.wikimedia.org/wikipedia/commons/7/71/Sanzio_01_Raphael.jpg",
    "Raphael_School_of_Athens_Euclid_Bramante.jpg": "https://upload.wikimedia.org/wikipedia/commons/8/81/Sanzio_01_Euclid.jpg",
    "Raphael_School_of_Athens_Plato_Aristotle_vanishing_point.jpg": "https://upload.wikimedia.org/wikipedia/commons/9/98/Sanzio_01_Plato_Aristotle.jpg",
    "Raphael_School_of_Athens_labeled.jpg": "https://upload.wikimedia.org/wikipedia/commons/thumb/5/59/01_School_of_Athens_with_Labels.jpg/1920px-01_School_of_Athens_with_Labels.jpg",
    # Piero Flagellation — WGA alternate
    "Piero_Flagellation_of_Christ_wga.jpg": "https://www.wga.hu/art/p/piero/3/04flage1.jpg",
    # Cimabue Maestà — WGA alternate (Google Arts fallback)
    "Cimabue_Maesta_wga.jpg": "https://www.wga.hu/art/c/cimabue/madonna/madonna0.jpg",
}


def download(name: str, url: str) -> None:
    dest = OUT / name
    if dest.exists() and dest.stat().st_size > 1000:
        print(f"SKIP  {name}")
        return
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    for attempt in range(5):
        try:
            with urllib.request.urlopen(req, timeout=120) as resp:
                dest.write_bytes(resp.read())
            print(f"OK    {name} ({dest.stat().st_size:,} bytes)")
            return
        except Exception as exc:
            if attempt < 4:
                wait = 5 * (2 ** attempt)
                print(f"RETRY {name} ({exc}); wait {wait}s")
                time.sleep(wait)
            else:
                print(f"FAIL  {name}: {exc}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, url in DOWNLOADS.items():
        download(name, url)
        time.sleep(2)


if __name__ == "__main__":
    main()