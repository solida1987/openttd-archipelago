# OpenTTD Archipelago — Experimental

> ⚠️ **EXPERIMENTAL BRANCH** — This repository is unstable by design.
> New features, events, and ideas are tested here before being promoted to the stable release.
> Expect bugs, incomplete features, and breaking changes between versions.
> For the stable release, see [openttd-archipelago](https://github.com/solida1987/openttd-archipelago).

---

A full [Archipelago](https://archipelago.gg) multiworld randomizer integration for **OpenTTD 15.2**.

All 202 vanilla vehicles are locked at game start and randomized into the multiworld item pool. Complete procedurally generated missions to send checks. Receive vehicles, cash injections, cargo bonuses — or suffer traps like Recession, Breakdown Wave, and forced Bank Loans sent by other players.

---

## Features

- **202 vanilla vehicles randomized** — all climates (Temperate, Arctic, Tropical, Toyland), all types (trains, wagons, road vehicles, aircraft, ships)
- **Iron Horse support** — enable the Iron Horse NewGRF to add 163 additional locomotives to the item pool (bundled, no manual install needed)
- **11 mission types** — transport cargo, earn profit, build stations, connect cities, buy from shop, and more
- **7 traps** — Breakdown Wave, Recession, Maintenance Surge, Signal Failure, Fuel Shortage, Forced Bank Loan, Industry Closure
- **8 utility items** — Cash Injections (£50k/£200k/£500k), Loan Reduction, Cargo Bonus 2×, Reliability Boost, Station Upgrade, Town Growth
- **5 win conditions** — Company Value, Monthly Profit, Vehicle Count, Town Population, Cargo Delivered
- **Death Link** — train crashes, road vehicle hits, and aircraft crashes all send deaths to the multiworld
- **Money Quests GameScript** — optional standalone GameScript that spawns cargo delivery quests with cash rewards
- **Dynamic pool scaling** — mission and shop counts scale automatically with player count (1–24 players)
- **56 YAML options** — configure map size, climate, economy, vehicle limits, win conditions, and individual trap toggles
- **In-game Guide** — built-in Guide & Tips window with AP commands, hotkeys, and gameplay tips

---

## Download

> ⚠️ Experimental builds may be unstable. Use the [stable release](https://github.com/solida1987/openttd-archipelago) for a reliable experience.

### Play (Windows, standalone)

1. Download `openttd-archipelago-exp-1.0-win64.zip` from [Releases](../../releases/latest)
2. Extract anywhere — OpenGFX is included, no separate OpenTTD install needed
3. Download `openttd.apworld` from the same release
4. Place `openttd.apworld` in your Archipelago `custom_worlds/` folder:
   - Default path: `C:\ProgramData\Archipelago\custom_worlds\`
5. Generate a multiworld using your YAML (see [YAML Setup](#yaml-setup))
6. Launch `openttd.exe`, click **Archipelago** in the main menu, enter your connection details

### Install APWorld only (if you already have the client)

Download `openttd.apworld` and place it in `C:\ProgramData\Archipelago\custom_worlds\`.

---

## YAML Setup

```yaml
name: YourName
game: OpenTTD

OpenTTD:
  win_condition: company_value            # company_value | monthly_profit | vehicle_count | town_population | cargo_delivered
  win_condition_company_value: 50000000   # Target value — use the key matching your win_condition
  starting_vehicle_type: train            # train | road_vehicle | aircraft | ship | random | one_of_each
  landscape: temperate                    # temperate | arctic | tropical | toyland
  mission_count: 100                      # Relative scaler (100 = default baseline)
  enable_traps: true
  death_link: false
  map_size_x: 256
  map_size_y: 256
  start_year: 1950
  max_trains: 500
  max_loan: 300000
```

> **Note:** The win condition target uses a separate key per type:
> `win_condition_company_value`, `win_condition_monthly_profit`, `win_condition_vehicle_count`,
> `win_condition_town_population`, or `win_condition_cargo_delivered`.
> Only the key matching your chosen `win_condition` is used.

See [docs/yaml_options.md](docs/yaml_options.md) for all 56 options with descriptions and valid ranges.

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
# Output: dist\openttd-archipelago-v1.0.0-win64.zip
```

---

## Known Limitations

| Issue | Severity | Notes |
|-------|----------|-------|
| Multiplayer (multiple companies) | Not planned | All logic assumes a single local company. Co-op / competition mode not supported |
| WebSocket compression | Low | Compressed WebSocket connections not supported. Server logs a warning but connection works normally |

---

## License

This project is a fork of [OpenTTD](https://github.com/OpenTTD/OpenTTD) and is licensed under the **GNU General Public License v2** — the same license as OpenTTD.

The APWorld (`openttd.apworld`) is licensed under **MIT**.

OpenTTD is copyright © the OpenTTD contributors. See [COPYING.md](COPYING.md) for the full GPL v2 text.

---

## Credits

- **OpenTTD** — the base game, [openttd.org](https://www.openttd.org)
- **Archipelago** — the multiworld randomizer framework, [archipelago.gg](https://archipelago.gg)
- **Iron Horse** — train set by andythenorth, licensed GPL v2
- Archipelago integration developed by [solida1987](https://github.com/solida1987)
