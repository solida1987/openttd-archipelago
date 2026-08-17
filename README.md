# OpenTTD Archipelago

A full [Archipelago](https://archipelago.gg) multiworld randomizer integration
for **OpenTTD 15.2**, built on OpenTTD's own GPL-2.0 source.

All 202 vanilla vehicles are locked at game start and randomized into the
multiworld item pool. Complete procedurally generated missions to send checks.
Receive vehicles, cash injections and cargo bonuses — or suffer traps like
Recession, Breakdown Wave and forced Bank Loans sent by other players.

---

## The Multiworld Launcher is required

This game is a **plugin** for the
[**Multiworld Launcher**](https://github.com/solida1987/Multiworld-Launcher)
and does not work without it. The launcher *is* the randomizer's other half:
it holds the Archipelago connection, feeds the game its seed over a local
pipe, guards that your NewGRF sets match the seed, and tracks your checks and
items live while you play.

The launcher is a **separate download from its own project** (version
**3.0.1 or newer**), and it ships with no games in it: every game arrives as a
plugin file you fetch and add yourself.

There is no in-game server login. The launcher owns the session, so pressing
*Join AP* in the game connects straight away — nothing to type.

## Download & Install

1. Download **launcher_package.zip** from the
   [Multiworld Launcher releases](https://github.com/solida1987/Multiworld-Launcher/releases/latest),
   extract it anywhere you have write access, and run
   **`Multiworld Launcher.exe`**. The library will be empty — that is correct.
2. Download **`openttd_archipelago-*.londonplugin`** from
   [this project's latest release](https://github.com/solida1987/openttd-archipelago/releases/latest).
3. In the launcher, click **Add plugin…**, pick the file, **read the dialog**,
   and approve it. OpenTTD appears in the library.
4. Click OpenTTD in the library, then **Install** — the launcher downloads the
   game package from this repository's releases. OpenTTD is free software, so
   the whole game comes down; there is nothing to own first.
5. **Play** joins an Archipelago multiworld. **Launch Standalone** plays a
   solo randomized run with no server — pick a seed under Settings, or let it
   pick one for you.

Every release also carries the full game package as a plain zip, so you can
download and inspect it by hand — but installing through the launcher is what
keeps it updated.

## Features

- **202 vanilla vehicles randomized** — all climates, all vehicle types
- **Optional NewGRF sets** — seeds can be generated against Iron Horse, FIRS
  and more. You install those yourself from OpenTTD's own content service;
  the launcher checks that your sets match the seed before play starts
- **11 mission types** — transport cargo, earn profit, build stations,
  connect cities, buy from shop, and more
- **7 traps, 8 utility items, 20 speed boosts**
- **5 win conditions** — company value, monthly profit, vehicle count,
  town population, cargo delivered
- **Standalone mode** — pre-generated solo seeds, no server needed, progress
  kept per seed
- **Death Link** — vehicle crashes travel to and from the multiworld
- **Dynamic pool scaling** for 1–24 players, in-game guide, mission-linked
  industry protection

## YAML

The launcher builds your Archipelago YAML for you: **Create YAML** on the
game's page walks all 108 options with their real keys and defaults, taken
from the apworld itself.

## Building from source

Requirements: Visual Studio 2022, CMake, vcpkg.

```
git clone https://github.com/solida1987/openttd-archipelago
cd openttd-archipelago
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config RelWithDebInfo
```

The London plugin's source lives in `plugin/` in this repository. The apworld
is under `apworld/`; `tools/build_apworld.py` packages it, and the gates under
`tools/` verify a release before it ships.

## License

This project is a fork of [OpenTTD](https://github.com/OpenTTD/OpenTTD),
licensed **GPL-2.0** like OpenTTD itself. The apworld is MIT. OpenTTD is
copyright © the OpenTTD contributors; see [COPYING.md](COPYING.md).

No third-party NewGRF content is distributed with this project. The two
bundled `.grf` files (Archipelago Ruins, Archipelago Stars) are our own.

## Credits

- **OpenTTD** — the base game, [openttd.org](https://www.openttd.org)
- **Archipelago** — the multiworld framework, [archipelago.gg](https://archipelago.gg)
- Archipelago integration by [solida1987](https://github.com/solida1987)

---

## Legacy version (discontinued)

Before the launcher existed, this integration shipped as a **standalone build
with its own built-in Archipelago client** — you typed a server address, slot
and password into the game itself.

That line is **discontinued and will not be updated**, but it still works and
remains downloadable for anyone who prefers not to use the launcher: every
release carries it as **`openttd-archipelago-legacy-*-win64.zip`**, so it is
always one download away. It plays Archipelago the old way, on the multiworld
versions of its era. Its source is the `v1.4.1` tag in this repository.

Why it was replaced: the new line moves the whole network stack out of the
game and into the launcher. That gives one shared connection, a live tracker,
a NewGRF guard before play starts, standalone seeds, YAML generation and
auto-updates — none of which the old build can do. All new features and fixes
land only in the plugin line above.

---

## Archipelago Discord Notice

I have been permanently banned from the official Archipelago Discord server.
Because of this, please do not post or share links to this project on the
official Archipelago Discord, as this project is not permitted there.

For clarity, the ban was not related to malware, viruses, malicious code, or
any security issue with this project.

The moderation issues were related to:

* Copyright/distribution concerns involving game files in earlier versions of
  my projects. Those files were removed, the affected repositories and
  releases were cleaned up, and the distribution process was changed
  accordingly.
* Violations of the Discord server's own content rules, including
  links/content involving games that were restricted or considered 18+ under
  their server rules.

These issues relate to the official Archipelago Discord's moderation and
content policies.

Development and support for this project will continue independently outside
of the official Archipelago Discord.

---

## AI Usage Disclosure

AI-assisted tools are used throughout parts of this project as productivity tools.
This includes, but is not limited to:

* Artwork and other visual assets
* Translation between Danish and English
* Discord messages and community communication
* Patch notes, documentation and release notes
* Source-code comments and other explanatory text
* General text editing, rewriting and formatting

AI tools may also be used as part of the overall development workflow. Regardless of what tools are used during development, I remain responsible for the project, its implementation, testing, releases and any code that is distributed.

My native language is Danish, so AI is particularly useful for quickly converting what I want to say into readable English instead of spending a large amount of development time translating and rewriting everything manually.

AI-generated or AI-assisted visual assets may also be used where appropriate. I am not an artist, and these tools allow me to create artwork for areas of the project that would otherwise have little or no custom artwork.

This disclosure is here so there is no ambiguity about the use of AI-assisted tools in the project.
