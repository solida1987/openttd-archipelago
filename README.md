# OpenTTD Archipelago

A full [Archipelago](https://archipelago.gg) multiworld randomizer integration for **OpenTTD 15.2**.

All 202 vanilla vehicles are locked at game start and randomized into the multiworld item pool. Complete procedurally generated missions to send checks. Receive vehicles, cash injections, cargo bonuses — or suffer traps like Recession, Breakdown Wave, and forced Bank Loans sent by other players.

> **Status: Beta** — Core gameplay is complete and stable. See [Known Limitations](#known-limitations).

---

## Features

- **202 vehicles randomized** — all climates (Temperate, Arctic, Tropical, Toyland), all types (trains, wagons, road vehicles, aircraft, ships)
- **11 mission types** — transport cargo, earn profit, build stations, connect cities, buy from shop, and more
- **7 traps** — Breakdown Wave, Recession, Maintenance Surge, Signal Failure, Fuel Shortage, Forced Bank Loan, Industry Closure
- **8 utility items** — Cash Injections (£50k/£200k/£500k), Loan Reduction, Cargo Bonus 2×, Reliability Boost, Station Upgrade, Town Growth
- **5 win conditions** — Company Value, Monthly Profit, Vehicle Count, Town Population, Cargo Delivered
- **Death Link** — train crashes, road vehicle hits, and aircraft crashes all send deaths to the multiworld
- **Dynamic pool scaling** — mission and shop counts scale automatically with player count (1→24 players)
- **56 YAML options** — configure map size, climate, economy, vehicle limits, win conditions, and individual trap toggles

---

## Download

### Play (Windows, standalone)

1. Download `OpenTTD-AP-<version>-windows-x64.zip` from [Releases](../../releases)
2. Extract anywhere — OpenGFX and OpenSFX are included, no separate OpenTTD install needed
3. Download `openttd.apworld` from the same release
4. Place `openttd.apworld` in your Archipelago `worlds/` folder
5. Generate a multiworld using your YAML (see [YAML Setup](#yaml-setup))
6. Launch `openttd.exe`, click **Archipelago** in the main menu, enter your connection details

### Install APWorld only (if you already have the client)

Download `openttd.apworld` and place it in your Archipelago installation's `worlds/` folder.

---

## YAML Setup

Add an `openttd` section to your Archipelago YAML:

```yaml
name: YourName
game: OpenTTD

OpenTTD:
  win_condition: company_value       # company_value | monthly_profit | vehicle_count | town_population | cargo_delivered
  win_condition_value: 5000000       # Target value for the chosen condition
  starting_vehicle_type: trains      # trains | road | aircraft | ships | random
  landscape: temperate               # temperate | arctic | tropical | toyland
  mission_count: 300                 # Relative scaler (300 = ×1.0 baseline)
  enable_traps: true
  death_link: false
  map_size_x: 256
  map_size_y: 256
  start_year: 1950
  max_trains: 500
  max_loan: 500000
```

See [docs/yaml_options.md](docs/yaml_options.md) for all 56 options with descriptions and valid ranges.

---

## Building from Source

### Requirements

- Windows 10/11 (MSVC build only — Linux/Mac builds not currently tested)
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with C++ workload
- [vcpkg](https://vcpkg.io/) (for dependencies)
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
.\scripts\Package-OpenTTD-AP.ps1
# Output: dist\OpenTTD-AP-<version>-windows-x64.zip
```

---

## Known Limitations

| Issue | Severity | Notes |
|-------|----------|-------|
| "Maintain X% rating for N months" mission | Low | Counts qualifying stations now instead of tracking sustained rating over time. Completable but slightly easier than described |
| WebSocket compression | Low | Compressed connections not supported. Archipelago server shows a warning but connection works normally |
| Multiplayer (multiple companies) | Future | All logic assumes `_local_company`. Co-op/competition mode not supported |

---

## License

This project is a fork of [OpenTTD](https://github.com/OpenTTD/OpenTTD) and is licensed under the **GNU General Public License v2** — the same license as OpenTTD.

The APWorld (`openttd.apworld`) is licensed under **MIT**.

OpenTTD is copyright © the OpenTTD contributors. See [COPYING.md](COPYING.md) for the full GPL v2 text.

---

## Credits

- **OpenTTD** — the base game, [openttd.org](https://www.openttd.org)
- **Archipelago** — the multiworld randomizer framework, [archipelago.gg](https://archipelago.gg)
- Archipelago integration developed by [solida1987](https://github.com/solida1987)
