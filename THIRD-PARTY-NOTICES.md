# Third-party notices

Everything in the download that is not ours, and where its licence comes from.

Two kinds of statement appear below, and they are kept apart on purpose:

- **Read from the file.** The licence text or statement travels with the
  download, at the path given. That is the authority.
- **Identified by name.** The file carries no licence text of its own. It is
  named here so nothing is passed on silently, with a pointer to the project it
  comes from. Their page is the authority, not this one.

The distinction matters. An earlier review in this project asserted a licence
for a DLL from memory and got it wrong, so nothing here is stated as fact
unless a file in the download says it.

---

## OpenTTD itself

Copyright © the OpenTTD contributors. GNU General Public License version 2.0.

- **Read from the file:** [`COPYING.md`](./COPYING.md) — the full GPL v2 text.
- OpenTTD's own third-party modules (squirrel, md5, fmt, nlohmann json, the
  OpenGL API, catch2, icu scriptrun, monocypher, the social integration API and
  `CheckAtomic.cmake`) each carry their licence beside the source. They are
  listed in the **License** section of [`README.md`](./README.md), reproduced
  verbatim from upstream OpenTTD.

This fork's changes — the Archipelago integration in `src/archipelago*.{cpp,h}`,
the apworld under `apworld/`, and `archipelago_ruins.grf` /
`archipelago_stars.grf` — are under the same GPL v2. The complete source for the
binary you have is the repository this file came from, at the tag matching your
version.

---

## Graphics, sound and music

OpenTTD cannot start without a graphics set. These are the free replacements for
the original Transport Tycoon Deluxe data, made by the OpenTTD community, and
they ship with the download.

| Set | Files | Where its licence is |
|---|---|---|
| **OpenGFX** | `baseset/ogfx*.grf`, `opengfx.obg`, `opengfx-*.tar` | **Read from the file:** GPL v2, stated in the set's own `.tar` |
| **OpenSFX** | `baseset/opensfx.cat`, `opensfx.obs`, `opensfx-*.tar` | **Read from the file:** `baseset/readme.txt` |
| **OpenMSX** | `baseset/*.mid`, `openmsx.obm`, `openmsx-*.tar` | **Read from the file:** `baseset/license.txt` |
| OpenTTD fonts | `baseset/OpenTTD-*.ttf` | Part of OpenTTD, GPL v2 |

`orig_dos.obg`, `orig_win.obg` and their siblings are **descriptions** of where
the original Transport Tycoon Deluxe files would go. The originals themselves
are not in this download and must not be — they are Chris Sawyer's and
Atari's.

---

## Computer players

| Component | Author | Where its licence is |
|---|---|---|
| **SimpleAI 14** (`data/ai/SimpleAI-14/`) | Brumi | **Read from the file:** `license.txt` in that folder — GPL v2 |
| `data/ai/library/graph/aystar` | OpenTTD NoAI Developers Team | Part of OpenTTD, GPL v2 |
| `data/ai/library/pathfinder/rail`, `/road` | OpenTTD NoAI Developers Team | Part of OpenTTD, GPL v2 |
| `data/ai/library/queue/binary_heap` | OpenTTD NoAI Developers Team | Part of OpenTTD, GPL v2 |

These ship as Squirrel source. There is no compiled form without source, so
GPL's source requirement is met by the delivery itself.

---

## Supporting libraries (the DLLs)

Six libraries sit beside `openttd.exe`. They are the same files OpenTTD ships
with its own official Windows build — they are its dependencies, not ours, and
we did not build them.

**Identified by name.** None of these files carries a licence statement in its
version resource, so the entries below name the library rather than assert its
terms. Each project publishes its own licence.

| File | Library | Project |
|---|---|---|
| `zlib1.dll` | zlib | zlib.net |
| `libpng16.dll` | libpng | libpng.org |
| `liblzma.dll` | XZ Utils / liblzma | tukaani.org/xz |
| `lzo2.dll` | LZO, by Markus Oberhumer | oberhumer.com/opensource/lzo |
| `ogg.dll` | libogg, by Xiph.Org | xiph.org |
| `opus.dll` | libopus, by Xiph.Org | opus-codec.org |

---

## NewGRF vehicle and industry sets — NOT included

Releases up to and including v1.4.1 bundled eight third-party sets: Iron Horse,
FIRS, HEQS, SHARK, Military Items, Vactrain, Aircraftpack 2025 and Hover
Vehicles. About 79 MB of other people's work.

They should not have been in there. Several are GPL v2, which obliges whoever
passes on the binary to offer the source — the `.nml` and `.pnml` files — and we
offered nothing. Three carry no licence statement at all, so we could not have
known what was permitted even if we had wanted to.

**They have been removed, from the download and from this repository's history.**

Install the ones a seed needs through OpenTTD's own **Check Online Content**,
which fetches them from the authors' distribution service and keeps them
updated. See [`NEWGRF_SETUP.md`](./NEWGRF_SETUP.md).

The authors are credited in [`README.md`](./README.md), and the game still
supports every one of those sets — it simply no longer hands you copies of them.

---

## AI Usage Disclosure

See the **AI Usage Disclosure** section of [`README.md`](./README.md). It is
kept in one place so the two cannot drift apart.
