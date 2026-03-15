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
- **Multiplayer mode** — cooperative play via bridge architecture. Multiple players share one company
- **Community Vehicle Names** — vehicles auto-named after community members
- **Redesigned status bar** — full-width bottom panel with AP message log, button bar, and live stats
- **Vehicle Index** — searchable catalogue of all available vehicles with lock/unlock status
- **In-game Guide** — built-in reference window with gameplay tips and system explanations

---

## Download

### Play (Windows, standalone)

1. Download `openttd-archipelago-v1.2.0-win64.zip` from [Releases](../../releases/latest)
2. Extract anywhere — OpenGFX, OpenSFX, and OpenMSX are included. No separate OpenTTD install needed
3. Copy the `apworld/openttd_exp/` folder into your Archipelago `custom_worlds/` directory:
   - Default path: `C:\ProgramData\Archipelago\custom_worlds\`
4. Generate a multiworld using your YAML (see [YAML Setup](#yaml-setup))
5. Launch `openttd.exe`, click **Archipelago** in the main menu, enter your connection details

### Multiplayer (cooperative)

1. Follow steps 1–4 above
2. Set `multiplayer_mode: true` in your YAML
3. Run `Server.bat` or use the bridge application in the `bridge/` folder
4. Other players connect to the host's IP on port 23969 using standard OpenTTD multiplayer join
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

- Windows 10/11 (MSVC build only — Linux/Mac builds not currently tested)
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
# Output: dist\openttd-archipelago-v1.2.0-win64.zip
```

---

## Known Limitations

| Issue | Severity | Notes |
|-------|----------|-------|
| Windows only | Medium | Linux/Mac not tested. Wine may work but is unsupported |
| Bridge multiplayer requires Python 3.10+ | Low | Only needed for cooperative multiplayer mode |
| FIRS cargo combinations | Low | Some mission templates may produce unexpected cargo targets with FIRS enabled |

---

## License

This project is a fork of [OpenTTD](https://github.com/OpenTTD/OpenTTD) and is licensed under the **GNU General Public License v2** — the same license as OpenTTD.

The APWorld (`openttd_exp.apworld`) is licensed under **MIT**.

OpenTTD is copyright © the OpenTTD contributors. See [COPYING.md](COPYING.md) for the full GPL v2 text.

---

## Credits

- **OpenTTD** — the base game, [openttd.org](https://www.openttd.org)
- **Archipelago** — the multiworld randomizer framework, [archipelago.gg](https://archipelago.gg)
- **Iron Horse** — train set by andythenorth, licensed GPL v2
- **FIRS Industries** — industry set by andythenorth, licensed GPL v2
- **HEQS** — heavy equipment set by andythenorth, licensed GPL v2
- **Military Items** — aircraft set, licensed GPL v2
- **SHARK Ships** — ship set, licensed GPL v2
- Archipelago integration developed by [solida1987](https://github.com/solida1987)
