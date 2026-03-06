# Changelog — OpenTTD Archipelago

## [1.0.0-beta6] — 2026-03-07

### Added
- **Iron Horse 4.14.1 support** — 164 verified locomotives from the Iron Horse NewGRF
  are now part of the item pool when the NewGRF is active. Engine names sourced
  directly from the GRF NFO (Action 4 feature 0x00) — no invented names.
  Enable via the NewGRF button in the AP Connect window before generating a world.
- **`one_of_each` starting vehicle option** — start with one safe vehicle per transport
  type (bus, train, small plane, passenger ferry) instead of a single random pick.

### Fixed
- **Mission text crushed at UI scale ≥2** — row height is now recomputed at draw
  time using the current font/scale rather than cached at window construction.
  Mission list is now fully readable at all interface sizes.
- **Currency shows £ on non-GBP games** — mission descriptions now replace the
  hardcoded £ symbol with the game's active currency prefix (e.g. $ for USD) at
  render time, matching the rest of the UI.
- **`random` starting vehicle could give unusable cargo wagon** — added a multi-level
  fallback: (1) road cargo trucks → bus substitute, (2) engine map rebuild + retry,
  (3) emergency fallback to first available locomotive. A starting vehicle is now
  always unlocked regardless of edge cases.
- **Shop shows fewer items than configured `shop_slots`** — `_compute_pool_size()`
  previously computed its own slot count and ignored the player's YAML `shop_slots`
  setting entirely. It now reads `self.options.shop_slots.value` directly. Setting
  `shop_slots: 5` in your YAML now reliably shows 5 items in the shop.
- **AP status window could not be dragged / was jammed in UI** — window placement
  changed from `WDP_MANUAL` (hardcoded top-right) to `WDP_AUTO` with persistence key
  `"ap_status"`. Position is now remembered between sessions and the window can be
  freely dragged.
- **"Unknown item: not handled" for vanilla engines** — `BuildEngineMap()` was called
  before `never_expire_vehicles = true`, so expired/not-yet-introduced engines returned
  empty names and were never indexed. Fix: expire flags set before map build + direct
  `string_id` fallback for any engine still returning empty from purchase list context.
- **Oil Tanker only unlocked one of three wagon variants** — `std::map` stored one
  `EngineID` per name; the Rail/Monorail/Maglev Oil Tanker variants share a name.
  Extras are now tracked in a parallel `_ap_engine_extras` map and all variants are
  unlocked together.
- **IH engine prefix mismatch in lock check** — Iron Horse engine names in slot_data
  carry an `IH: ` prefix; the C++ lock lookup now strips it before map lookup.

### Changed
- Default trap settings toned down — Bank Loan and Recession traps are now **off** by
  default; enable in YAML if desired.
- DeathLink is now **off** by default.
- Shop items sorted by price ascending so cheapest options are always visible first.

---

## [1.0.0-beta5] — 2026-03-06

### Fixed
- **Toyland missions on non-Toyland maps** — mission generator now uses a
  climate-filtered cargo list. Temperate/Arctic/Tropical maps will never
  generate missions referencing Candyfloss, Cola, Toffee or other
  Toyland-exclusive cargos.
- **Toyland vehicles in item pool on non-Toyland maps** — Toyland-only
  vehicles (Choo-Choos, Ploddyphut buses, all Toyland trucks/aircraft) are
  now excluded from the randomised item pool when the map is not Toyland.
  Previously these would appear as received items that silently did nothing.
- **"Service X towns" impossible on small maps** — mission amounts for
  town-service missions are now capped based on map dimensions so the
  generated count cannot exceed a realistic number of towns on the map.
- **Bank Loan Forced trap — 500M hardcoded amount** — the forced loan is now
  scaled to the session's configured `max_loan` value rather than a fixed
  £500,000,000 that was impossible to repay in early game.
- **Shop refusing purchases / price display wrong** — shop slots that have
  already been purchased this session are now tracked and filtered out of the
  shop list immediately after purchase, preventing the UI from showing bought
  slots as still available.

### Added
- **"Start with one of each" option** — new `Starting Vehicle Type` choice
  `one_of_each` gives the player one safe starting vehicle from every transport
  type at game start (bus, train, small plane, passenger ferry), so all four
  types are immediately available regardless of what gets randomised into the pool.

### Changed
- **Trap intensity slider** — new `Trap Intensity (%)` YAML option (0–100,
  default 30) controls what share of the item pool consists of traps.
  Replaces the implicit 15% hardcoded rate. Higher = more traps.
- **DeathLink default changed to off** — DeathLink was on by default, which
  surprised players who hadn't explicitly opted in to shared deaths. Now off by
  default; enable in your YAML if you want it.
- **Shop sorted by price ascending** — cheapest items now always appear at the
  top of the shop list, making it readable early-game when money is limited.
- **Default trap settings toned down** — `Trap: Bank Loan` and
  `Trap: Recession` both default to **off**. These two traps are the most
  punishing and are now opt-in rather than opt-out.

---

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
