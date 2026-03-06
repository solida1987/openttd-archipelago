# Changelog — OpenTTD Archipelago

## [1.0.0-beta4] — 2026-03-06

### Fixed
- **Wine incompatibility** — Beta 3's WSS/WS auto-detection used Windows Schannel
  (`Secur32.dll`) which Wine does not implement correctly, preventing connection.
  Wine is now detected at runtime via `HKLM\Software\Wine`; the WSS probe is skipped
  entirely and the client connects via plain WS directly (matching Beta 2 behaviour).
- **"Missing 140 sprites" warning on main menu** — false positive caused by the build
  not being tagged as a vanilla released version. The warning widget is now
  unconditionally hidden; the bundled baseset is complete.
- **Starting vehicle locked on non-Toyland maps** — Toyland-only trains
  (`Ploddyphut Choo-Choo`, `Powernaut Choo-Choo`, `MightyMover Choo-Choo`) could be
  selected as starting vehicle on Temperate/Arctic/Tropical maps where they do not
  exist, leaving the player with nothing unlocked.
- **Unviable starting road vehicles** — cargo trucks (goods, coal, etc.) could be
  selected as starting vehicle despite requiring specific industries near a depot.
  Players could find themselves unable to earn any money at game start.

### Changed
- **Starting vehicle pools tightened** — each transport type now only draws from
  vehicles that are always viable from day one regardless of map industries:
  - **Trains:** steam + early diesel only (passengers/mail always available)
  - **Road vehicles:** buses and mail trucks only (no cargo trucks)
  - **Aircraft:** first 5 small props only (work on basic airport)
  - **Ships:** passenger ferries only (no cargo ships or oil tankers)

---

## [1.0.0-beta3] — 2026-03-06

### Fixed
- **"Maintain X% rating for N months" missions** — now correctly tracks consecutive months
  where ALL rated stations meet the threshold. Any station falling below threshold resets
  the counter to zero. Previously approximated by counting qualifying stations.
- **DeathLink notification** — inbound deaths now show a full newspaper popup
  (`NewsStyle::Normal`) instead of the small corner notification, making them impossible
  to miss. Error is also printed in red to the console.
- **Server field placeholder** — default no longer shows `wss://` prefix; auto-detection
  handles protocol selection transparently.

### Added
- **Savegame persistence (APST chunk)** — AP session state now survives save/load:
  - Connection credentials (host, port, slot, password)
  - Completed mission list
  - Shop page offset and day counter
  - Cumulative cargo and profit statistics
  - "Maintain rating" month counters
- **Maintain rating counter persistence** — month counters for rating missions are saved
  and restored, so long-running missions are not reset by a save/load cycle.

### Changed
- Dead code cleanup: removed unused `bool fin` warning (C4189), unused `bool all_pass`
  variable in maintain timer, and unreachable `WAPGUI_BTN_MISSIONS` click handler.

---

## [1.0.0-beta2] — 2026-03-05

### Fixed
- **WSS/WS auto-detection** — client now probes WSS first and falls back to plain WS
  automatically; users never need to type a scheme prefix
- **Build fix** — zlib dependency is now optional (`#ifdef WITH_ZLIB`); build succeeds
  without it and falls back to uncompressed WebSocket frames

### Changed
- Server field placeholder changed to `archipelago.gg:38281` — scheme is handled
  automatically and no longer shown in the input field
- Reconnect button uses the same auto-detection logic

---

## [1.0.0-beta1] — 2026-03-05

First public beta release.

### Added

**Game client (C++ / OpenTTD 15.2)**
- WebSocket connection to Archipelago server with auto-reconnect
- Engine lock system — all 202 vanilla vehicles locked at game start, unlocked via received items
- `AP Connect` button in main menu and in-game toolbar
- Missions window showing current checks with progress bars
- Shop window with page rotation (refreshes every N in-game days per YAML setting)

**Item system**
- 202 vehicles across all climates: 35 trains, 27 wagons, 88 road vehicles, 41 aircraft, 11 ships
- 7 trap items: Breakdown Wave, Recession, Maintenance Surge, Signal Failure, Fuel Shortage, Forced Bank Loan, Industry Closure
- 8 utility items: Cash Injection ×3 tiers, Loan Reduction, Cargo Bonus 2×, Reliability Boost 90d, Station Upgrade 30d, Town Growth Boost

**Mission system**
- 11 mission types with procedural generation (no duplicates, spacing rules enforced)
- Dynamic pool scaling: 347 locations (solo) → 1095 locations (16 players)

**Death Link**
- Train collision, road vehicle hit by train, aircraft crash — all send deaths outbound
- Inbound deaths: industry closure + 10% money penalty, with 30-second cooldown

**Win conditions**
- 5 configurable win conditions: Company Value, Monthly Profit, Vehicle Count, Town Population, Cargo Delivered

**APWorld**
- Full Archipelago APWorld (`openttd.apworld`) with 56 configurable YAML options
- Supports Archipelago 0.6.6+
