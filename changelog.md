# Changelog — OpenTTD Archipelago

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
