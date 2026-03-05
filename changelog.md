# Changelog — OpenTTD Archipelago

## [1.0.0-beta2] — 2026-03-05

### Fixed
- **WSS support** — client now connects to the official archipelago.gg WebHost using encrypted WebSocket (wss://)
  - TLS implemented via Windows Schannel — no external dependencies required
  - `wss://` is now always prepended automatically; users only type the host and port
- **Build fix** — zlib dependency is now optional (`#ifdef WITH_ZLIB`); build succeeds without it and falls back to uncompressed WebSocket frames
- **GUI** — `wss://` displayed as a fixed label next to the server field so it cannot be accidentally deleted

### Changed
- Server field default placeholder changed from `archipelago.gg:38281` to `archipelago.gg:38281` (scheme handled automatically)
- Reconnect button also uses WSS automatically

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

### Known Issues

- "Maintain X% rating for N months" counts qualifying stations rather than tracking sustained rating over time
- WebSocket compression not supported (server shows a warning, connection works normally)
- Multiplayer (multiple companies in one game) not supported
