# OpenTTD Archipelago

A full [Archipelago](https://archipelago.gg) multiworld randomizer integration for **OpenTTD 15.2**.

All vehicles are locked at game start and randomized into the multiworld item pool. Complete procedurally generated missions, clear cursed ruins, defeat demigod rivals, and purchase items from the in-game shop to send checks. Receive vehicles, infrastructure unlocks, speed boosts, cash injections — or suffer traps like Recession, Breakdown Wave, and Vehicle License Revoke sent by other players.

---

## Features

- **All vanilla vehicles randomized** — trains, wagons, road vehicles, aircraft, ships across all 4 climates
- **8 bundled NewGRF sets** — Iron Horse (~164 trains), Military Items (69 aircraft), SHARK Ships (70 ships), Hover Vehicles (6 road vehicles), HEQS (46 road vehicles + 1 train), Vactrain (18 trains), Aircraftpack 2025 (47 aircraft), FIRS Industries (5 economy types). All toggled via YAML, no manual install
- **100+ infrastructure unlock items** — track directions, road directions, signals, bridges, tunnels, airports, terraform, trees, town actions. Controlled by Sphere Progression or individual toggles
- **Randomized missions** — transport cargo, earn profit, build stations, connect cities, and more across 4 difficulty tiers (Easy/Medium/Hard/Extreme)
- **Ruins system** — cursed map locations requiring cargo delivery to clear. Each ruin needs 2–4 cargo types
- **Demigod system** — rival AI companies sent by the God of Wackens. Pay tribute to defeat them
- **Colby Event** — a 5-step smuggling storyline with a moral choice at the end
- **In-game item shop** — purchase location checks with company funds across 7 price tiers
- **8 traps** — Breakdown Wave, Recession, Maintenance Surge, Signal Failure, Fuel Shortage, Forced Bank Loan, Industry Closure, Vehicle License Revoke
- **8 utility items** — Cash Injections (£50K/£200K/£500K), Loan Reduction, Cargo Bonus 2×, Reliability Boost, Town Growth, Station Upgrade
- **20 Speed Boost items** — fast-forward speed starts at 100% and increases by 10% per item up to 300%
- **6 win conditions** — Company Value, Monthly Profit, Vehicle Count, Town Population, Cargo Delivered, Missions Completed. All must be met simultaneously
- **11 difficulty presets** — Casual through Madness, plus fully custom sliders
- **Death Link** — vehicle crashes send deaths to the multiworld
- **God of Wackens Wrath** — destructive actions (bulldozing, terraforming) anger the God through 5 escalating punishment levels
- **Multiplayer mode** — cooperative play via dedicated server. Multiple players share one company
- **Community Vehicle Names** — vehicles auto-named after community members
- **Redesigned status bar** — full-width bottom panel with AP message log, button bar, and live stats
- **Vehicle Index** — searchable catalogue of all available vehicles with lock/unlock status
- **In-game Guide** — built-in reference window with gameplay tips and system explanations

---

## Download

### Play (Windows, standalone)

1. Download `openttd-archipelago-v1.4.1-win64.zip` from [Releases](../../releases/latest)
2. Extract anywhere — OpenGFX, OpenSFX, and OpenMSX are included. No separate OpenTTD install needed
3. Copy `openttd.apworld` into your Archipelago `custom_worlds/` directory:
   - Default path: `C:\ProgramData\Archipelago\custom_worlds\`
4. If your seed uses extra vehicle sets, install them first — they are **not**
   included: see [NEWGRF_SETUP.md](./NEWGRF_SETUP.md)
5. Generate a multiworld using your YAML (see [YAML Setup](#yaml-setup))
6. Launch `openttd.exe`, click **Archipelago** in the main menu, enter your connection details

### Play (Linux, standalone)

1. Download `openttd-archipelago-v1.4.1-linux-amd64.tar.gz` from [Releases](../../releases/latest)
2. Extract anywhere — all assets are included
3. Copy `apworld/openttd/` to your Archipelago `custom_worlds/` directory
4. If your seed uses extra vehicle sets, install them first — they are **not**
   included: see [NEWGRF_SETUP.md](./NEWGRF_SETUP.md)
5. Run `./openttd` (or `./server.sh` for dedicated server)
6. Connect via the in-game Archipelago menu

### Multiplayer (cooperative)

1. Follow steps 1–4 above
2. Set `multiplayer_mode: true` in your YAML
3. Host starts the game and opens it to multiplayer via the in-game Archipelago menu
4. Other players connect to the host's IP using standard OpenTTD multiplayer join
5. All players share one company and work toward the same goal

---

## YAML Setup

```yaml
name: YourName
game: OpenTTD

OpenTTD:
  # Win condition
  win_difficulty: normal          # casual | easy | normal | medium | hard | very_hard | extreme | insane | nutcase | madness | custom

  # Starting setup
  starting_vehicle_type: any      # any | train | road_vehicle | aircraft | ship
  starting_vehicle_count: 2       # 1–5
  starting_cash_bonus: none       # none | small (£50K) | medium (£200K) | large (£500K) | very_large (£2M)

  # Progression
  enable_sphere_progression: true # Lock all infrastructure behind item finds
  mission_difficulty: normal      # very_easy (×0.25) | easy (×0.5) | normal (×1.0) | hard (×2.0) | very_hard (×4.0)
  mission_tier_unlock_count: 5    # Missions needed to unlock next difficulty tier (0–20)

  # World
  landscape: temperate            # temperate | arctic | tropical | toyland
  map_size_x: 512                 # 512 | 1024 | 2048
  map_size_y: 512                 # 512 | 1024 | 2048
  start_year: 1950

  # Events & systems
  colby_event: false
  enable_demigods: false
  enable_wrath: true

  # Items
  enable_traps: true
  trap_count: 10
  utility_count: 15
  shop_price_tier: 3              # 1 (cheapest) – 7 (most expensive)

  # Ruins
  ruin_pool_size: 25
  max_active_ruins: 6

  # NewGRFs
  enable_iron_horse: true
  enable_firs: false

  # Multiplayer
  multiplayer_mode: false

  # Other
  death_link: false
  community_vehicle_names: true
```

See [docs/yaml_options.md](docs/yaml_options.md) for all options with descriptions and valid ranges.

---

## Building from Source

### Requirements

- Windows 10/11 (MSVC) or Linux x64 (GCC/Clang)
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with C++ workload
- [vcpkg](https://vcpkg.io/) for dependencies
- CMake 3.21+

### Steps

```powershell
# 1. Clone this repo
git clone https://github.com/solida1987/openttd-archipelago
cd openttd-archipelago

# 2. Install dependencies via vcpkg
vcpkg install  # reads vcpkg.json automatically

# 3. Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release

# 4. Package a standalone ZIP
.\build_and_package.bat
# Output: dist\openttd-archipelago-v1.4.1-win64.zip
```

---

## Known Limitations

| Issue | Severity | Notes |
|-------|----------|-------|
| Mac not supported | Medium | macOS is not tested. Wine may work but is unsupported |
| Multiplayer requires dedicated server setup | Low | Only needed for cooperative multiplayer mode |
| FIRS cargo combinations | Low | Some mission templates may produce unexpected cargo targets with FIRS enabled |

---

## License

This project is a fork of [OpenTTD](https://github.com/OpenTTD/OpenTTD) and is
licensed under the **GNU General Public License v2** — the same licence as
OpenTTD. OpenTTD is copyright © the OpenTTD contributors.

The APWorld (`openttd.apworld`) is licensed under **MIT**.

`COPYING.md` says the licence "applies to OpenTTD with the exception of some 3rd
party modules — see our readme for details". The section below is that detail,
reproduced verbatim from upstream OpenTTD's own README so the reference leads
somewhere. It was missing from earlier releases of this fork, which left
`COPYING.md` pointing at a document that did not contain what it promised.

### OpenTTD's third-party modules
OpenTTD is licensed under the GNU General Public License version 2.0.
For the complete license text, see the file '[COPYING.md](./COPYING.md)'.
This license applies to all files in this distribution, except as noted below.

The squirrel implementation in `src/3rdparty/squirrel` is licensed under the Zlib license.
See `src/3rdparty/squirrel/COPYRIGHT` for the complete license text.

The md5 implementation in `src/3rdparty/md5` is licensed under the Zlib license.
See the comments in the source files in `src/3rdparty/md5` for the complete license text.

The fmt implementation in `src/3rdparty/fmt` is licensed under the MIT license.
See `src/3rdparty/fmt/LICENSE.rst` for the complete license text.

The nlohmann json implementation in `src/3rdparty/nlohmann` is licensed under the MIT license.
See `src/3rdparty/nlohmann/LICENSE.MIT` for the complete license text.

The OpenGL API in `src/3rdparty/opengl` is licensed under the MIT license.
See `src/3rdparty/opengl/khrplatform.h` for the complete license text.

The catch2 implementation in `src/3rdparty/catch2` is licensed under the Boost Software License, Version 1.0.
See `src/3rdparty/catch2/LICENSE.txt` for the complete license text.

The icu scriptrun implementation in `src/3rdparty/icu` is licensed under the Unicode license.
See `src/3rdparty/icu/LICENSE` for the complete license text.

The monocypher implementation in `src/3rdparty/monocypher` is licensed under the 2-clause BSD and CC-0 license.
See `src/3rdparty/monocypher/LICENSE.md` for the complete license text.

The OpenTTD Social Integration API in `src/3rdparty/openttd_social_integration_api` is licensed under the MIT license.
See `src/3rdparty/openttd_social_integration_api/LICENSE` for the complete license text.

The atomic datatype support detection in `cmake/3rdparty/llvm/CheckAtomic.cmake` is licensed under the Apache 2.0 license.
See `cmake/3rdparty/llvm/LICENSE.txt` for the complete license text.
### This fork's own files

The Archipelago integration — `src/archipelago*.{cpp,h}`, the apworld under
`apworld/`, and the two NewGRFs in `media/baseset/` (`archipelago_ruins.grf`
and `archipelago_stars.grf`) — is ours, under the same GPL v2 as the rest.

### Bundled free assets

OpenGFX, OpenSFX, OpenMSX and SimpleAI travel with the download. They are free
replacements for the original Transport Tycoon Deluxe data, and OpenTTD cannot
run without a graphics set. Each is credited with its licence in
[THIRD-PARTY-NOTICES.md](./THIRD-PARTY-NOTICES.md).

### What is NOT bundled

No third-party vehicle or industry sets. Earlier releases shipped eight of them;
they have been removed. See [NEWGRF_SETUP.md](./NEWGRF_SETUP.md) for how to
install the ones a seed needs.

---

## Credits

- **OpenTTD** — the base game, [openttd.org](https://www.openttd.org)
- **Archipelago** — the multiworld randomizer framework, [archipelago.gg](https://archipelago.gg)
- **OpenGFX, OpenSFX, OpenMSX** — the free graphics, sound and music sets that
  ship with this download, by the OpenTTD community
- **SimpleAI** by Brumi — the computer players, GPL v2

Supported but **not** included — install them yourself through Check Online
Content, see [NEWGRF_SETUP.md](./NEWGRF_SETUP.md):

- **Iron Horse**, **FIRS Industries**, **HEQS** — by andythenorth
- **Military Items** — by adpro
- **SHARK Ships**, **Vactrain Set**, **Aircraftpack 2025**, **Hover Vehicles**
- Archipelago integration developed by [solida1987](https://github.com/solida1987)

---

## AI Usage Disclosure

AI-assisted tools are used throughout parts of this project as productivity
tools.

This includes, but is not limited to:

- Artwork and other visual assets
- Translation between Danish and English
- Discord messages and community communication
- Patch notes, documentation and release notes
- Source-code comments and other explanatory text
- General text editing, rewriting and formatting

AI tools may also be used as part of the overall development workflow.
Regardless of what tools are used during development, I remain responsible for
the project, its implementation, testing, releases and any code that is
distributed.

My native language is Danish, so AI is particularly useful for quickly
converting what I want to say into readable English instead of spending a large
amount of development time translating and rewriting everything manually.

AI-generated or AI-assisted visual assets may also be used where appropriate. I
am not an artist, and these tools allow me to create artwork for areas of the
project that would otherwise have little or no custom artwork.

This disclosure is here so there is no ambiguity about the use of AI-assisted
tools in the project.
