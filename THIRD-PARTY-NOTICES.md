# Third-party notices

Everything in this download that somebody else wrote, what it is, who wrote it,
and under what terms it is here.

This project itself is **GPL-2.0**, being a fork of OpenTTD. The full licence
text is in [COPYING.md](COPYING.md); the exceptions inside OpenTTD's own source
tree are listed under *Third-party components inside OpenTTD* in
[README.md](README.md).

---

## The game

**OpenTTD** — https://www.openttd.org — GPL-2.0
Copyright © the OpenTTD contributors.

This build is a **modified** OpenTTD: `src/archipelago*.{cpp,h}`,
`src/archipelago_manager.cpp`, `src/archipelago_gui.cpp` and
`src/saveload/archipelago_sl.cpp` were added, and a number of existing files
were changed to call into them. The complete corresponding source for this
binary is the repository this file came from.

---

## Base graphics, sound and music

Free replacements for the original Transport Tycoon Deluxe data. **No file
belonging to Transport Tycoon Deluxe is included in this download**, and none is
required — these sets stand in for all of it.

| Component | Files | Licence |
|---|---|---|
| **OpenGFX 8.0** | `baseset/ogfx*.grf`, `opengfx.obg`, `opengfx-8.0.tar` | GPL-2.0 |
| **OpenSFX 1.0.3** | `baseset/opensfx.cat`, `opensfx.obs`, `opensfx-1.0.3.tar` | see `baseset/readme.txt` |
| **OpenMSX 0.4.2** | 30 `.mid` files, `openmsx.obm`, `openmsx-0.4.2.tar` | see `baseset/license.txt` |

OpenMSX is the work of named composers, credited individually in
`baseset/readme.txt`. Their licence text travels in `baseset/license.txt` and
must stay with the files.

The files `orig_dos.obg`, `orig_win.obg`, `orig_dos.obm`, `orig_tto.obm`,
`orig_win.obm`, `orig_dos.obs`, `orig_win.obs`, `orig_dos_de.obg` and
`orig_extra.grf` are OpenTTD's own **descriptions** of where the original
Transport Tycoon data would be found. They contain none of that data.

---

## Computer players

| Component | Author | Licence |
|---|---|---|
| **SimpleAI 14** | Brumi | GPL-2.0, full text in `data/ai/SimpleAI-14/license.txt` |
| **AyStar** (`data/ai/library/graph/aystar`) | OpenTTD NoAI Developers Team | GPL-2.0 |
| **Rail pathfinder** (`data/ai/library/pathfinder/rail`) | OpenTTD NoAI Developers Team | GPL-2.0 |
| **Road pathfinder** (`data/ai/library/pathfinder/road`) | OpenTTD NoAI Developers Team | GPL-2.0 |
| **Binary heap** (`data/ai/library/queue/binary_heap`) | OpenTTD NoAI Developers Team | GPL-2.0 |

These are shipped as `.nut` source, so the GPL's source requirement is met by
the delivery itself.

---

## Support libraries

Shipped as built by the OpenTTD project for its own Windows releases.

| File | Library | Licence |
|---|---|---|
| `zlib1.dll` | zlib | zlib licence |
| `libpng16.dll` | libpng | libpng licence |
| `liblzma.dll` | XZ Utils / liblzma | public domain / 0BSD |
| `lzo2.dll` | LZO, by Markus Oberhumer | GPL-2.0 |
| `ogg.dll` | libogg, Xiph.Org | BSD-3-Clause |
| `opus.dll` | libopus, Xiph.Org | BSD-3-Clause |

---

## Our own content

| File | Source |
|---|---|
| `newgrf/archipelago_ruins.grf` | `media/baseset/archipelago_ruins/` — `.nml`, sprites and build script |
| `newgrf/archipelago_stars.grf` | this project |
| `baseset/archipelago_icons.grf` | this project |
| `apworld/openttd/` | this project |

---

## Not included

The optional vehicle and industry sets — **Iron Horse, FIRS, HEQS, SHARK,
Military items, Vactrain, Hover Vehicles and Aircraftpack 2025** — are not
distributed here.

Earlier releases up to v1.4.1 did include them, which was a mistake. They are
other people's work; most are GPL-2.0, which would oblige us to offer their
`.nml` source as well, and two of them state no licence in the file at all.

Install them yourself through OpenTTD's **Check Online Content**, from their
authors, in the version your seed asks for. See *NewGRF setup* in
[README.md](README.md).

---

*If something in this download is missing from this file, that is a bug — please
report it.*
