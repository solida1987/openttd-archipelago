# Changelog — OpenTTD Archipelago

## [exp-1.2.3] — 2026-03-16

### Fixed
- **Graphical glitch when demigod company spawns** — Fix: added `MarkWholeScreenDirty()` after demigod company naming completes.
- **Paper Truck and other wagons appearing on wrong climate** — Fix: added wagon climate exclusion frozensets at all 3 filter points.
- **FillError crash during generation** — Fix: replaced PRIORITY/EXCLUDED system with manual placement.
- **Speed Boost items trimmed from pool** — Fix: added `speed_boost_count` to `reserved`.
- **Airplane license revocation permanent** — When the trap expired, only engines in `_ap_unlocked_engine_ids` were unhidden. If no aircraft items had been received yet, all aircraft stayed hidden forever. Fix: unconditionally unhide all engines of the revoked type when the timer expires.
- **Infrastructure unlocks lost on reconnect** — All infrastructure unlocks (tracks, roads, signals, bridges, tunnels, airports, trees, terraform, town actions) were below the replay guard. On reconnect, slot_data reset lock states and replayed items were skipped. Fix: split item handling into two chains — traps (skip on replay) and infrastructure (always re-apply).
- **Breakdown Wave permanent** — Breakdown Wave set reliability to 1 with no timer, leaving vehicles permanently broken. Fix: now lasts 60 seconds, then automatically restores normal reliability.

### Changed
- **Item distribution uses fixed percentages** — Removed 7 "Advanced Balancing" YAML options. Progression items now manually placed: 40% missions / 40% shop / 10% ruins / 10% demigods. Missions fill easy→hard so early progression lands in easy missions.
- **Service intervals default to percentage-based** — AP sessions now force percentage-based service intervals (default 30%). Vehicles auto-service when reliability drops below threshold, fixing the issue where vehicles ran indefinitely at 0% reliability with day-based intervals.

### Added
- **Colby Event: reopen dismissed popups** — "Reopen Colby Decision" button in status window + auto-reopen after ~30 seconds.
- **Mission-linked industry protection** — Industries tied to active (incomplete) missions can no longer close down. Production is also kept at a minimum playable level (25% of default). No more "closing down" notifications for protected industries.
- **Victory vehicle requirement option** — `victory_vehicle_requirement` (5-50, default 15).
- **Tier vehicle multipliers** — `hard_tier_vehicle_multiplier` (1-5x) and `extreme_tier_vehicle_multiplier` (1-10x).
- **Minimum 150 missions** — Distributed across all 4 difficulty tiers, per-tier cap raised to 50.

---

## [exp-1.1] — patch_exp_1_1_30 — 2026-03-10

### Fixed
- **Task progress line: missing cargo unit label** — Progress now shows the correct OpenTTD unit for the cargo type (`tons`, `bags`, `litres`, `items`, etc.) by looking up `CargoSpec->units_volume` via `GetString()`. Previously showed raw numbers with no unit (e.g. `0 / 500 (0%)`); now shows `0 / 500 tons  (0%)`.
- **Task progress line: bullet characters rendered as empty boxes** — The bullet separator `•` (Unicode U+2022) is not present in OpenTTD's pixel font and rendered as □. Replaced with ASCII `-` throughout the task stats line.

---

## [exp-1.1] — patch_exp_1_1_29 — 2026-03-10

### Fixed
- **Phantom navigation links when clicking the mission/task list (three root causes):**
  - **C++ integer division on header row** — Clicking the header row (abs_row = 0) computed `task_idx = (0-1)/4 = 0` due to C++ truncating toward zero rather than -1, causing header clicks to navigate to the first task's map location. Fixed by guarding `if (abs_row <= 0) break` before the division.
  - **Stale `visible_missions` when switching to the Tasks filter** — `visible_missions` was never cleared when switching to Tasks. Clicks on the list fell through to mission navigation even though no missions were displayed. Fixed by calling `visible_missions.clear()` on every switch to Tasks, and `cached_tasks.clear()` on every switch away from Tasks.
  - **Scrollbar position not reset on filter switch** — If the user had scrolled to position 10 on "All" then switched to "Easy" (5 missions), the scrollbar stayed at position 10, causing clicks to hit invisible rows. Fixed by calling `this->scrollbar->SetPosition(0)` on every `SetFilterButton()` call.

---

## [exp-1.1] — patch_exp_1_1_28 — 2026-03-10

### Fixed
- **Negative expenses counted as positive profit (all 6 occurrences)** — OpenTTD stores `expenses` as a **negative number**. The correct formula is `income + expenses` (as used by OpenTTD's own `economy.cpp` line 189). All 6 occurrences in `archipelago_manager.cpp` used `income - expenses`, which converted a loss (e.g. income=£100, expenses=-£800) into a large positive number (£900) instead of the correct -£700. The `if (period_profit > 0)` guard was therefore bypassed. Fixed in: snapshot init (line 211), period-change detection (line 225), period accumulation (line 234), `AP_GetTotalProfit` current period (line 258), earn-monthly mission evaluation (line 417), win condition MONTHLY_PROFIT check (line 1758).

  > **Note:** `patch_exp_1_1_26` claimed to contain this fix but the zip was verified to still contain `income - expenses` at all 6 locations. This patch is the first to actually deliver the fix.

---

## [exp-1.1] — patch_exp_1_1_27 — 2026-03-10

### Changed
- **Tab layout replaced with a single filter row** — Removed the two-tab row (`[Missions] / [Tasks]`) that sat above the filter buttons. Tasks is now a sixth button in the single filter row: `[All] [Easy] [Medium] [Hard] [Extreme] [Tasks]`. Tasks behaves like any other filter with no disabled buttons and no confusing two-tier layout. Removed widget IDs: `WAPM_TAB_MISSIONS`, `WAPM_TAB_TASKS`, `WAPM_FILTER_PANEL`. Added: `WAPM_FILTER_TASKS`.

### Fixed
- **Clicking a link on one filter triggered navigation from a different filter** — `OnClick(WAPM_LIST)` always looked up `visible_missions[row]` regardless of the active filter. Clicking the same screen position after switching filters would fire navigation from the previous filter's list. Fixed by splitting list click logic into `if (show_tasks)` / `else` branches routing to the correct backing list (`cached_tasks` vs. `visible_missions`).

---

## [exp-1.1] — patch_exp_1_1_26 — 2026-03-10

### Added
- **Task card multi-line layout** — Each task in the Tasks view now renders as 4 rows:
  ```
  [ ] EASY  Pick up 500 t of Iron Ore
      -> from Breningbury Iron Ore Mine near Breningbury
      0 / 500  (0%)   -   By 1951   -   +£25k
      -------------------------------------------------
  ```
  Row 0: status badge + colour-coded difficulty tag + action description. Row 1: location with entity name highlighted in white. Row 2: progress + deadline + reward in grey. Row 3: separator line.

---

## [exp-1.1] — patch_exp_1_1_25 — 2026-03-10

### Fixed
- **Build error: `_ap_tasks` / `_ap_task_next_id` / `_ap_task_checks_completed` undeclared** — Static variables were declared after `AP_InitSessionStats()` which referenced them. Declarations moved before the function.
- **Build error: `GetCurrency` / `CurrencySpec` not found** — Added `#include "currency.h"` to `archipelago_manager.cpp`.
- **Build error: `int rh` redeclared in `DrawWidget`** — Two separate `int rh` declarations existed in the same function scope. Wrapped the missions section in an extra `{ }` block scope to isolate the variable.

---

## [exp-1.1] — patch_exp_1_1_22 — 2026-03-10

### Added
- **Speed Boost item (x20)** — Fast forward is now an Archipelago item. The FF button starts locked at 100% (normal speed — no speedup). Each "Speed Boost" item received adds +10% FF speed, up to a maximum of 300% (20 items). Items are placed in missions and shops like all other utility items and can land in other players' games in multiworld.
- **Settings lockdown during AP session** — The following settings categories are hidden while an AP session is active (players cannot change gameplay parameters mid-run): Accounting, Vehicles, Limitations, Disasters, World Generation, Environment, AI/Competitors. Graphics, sound, interface and localisation settings remain accessible.

### Changed
- `gfx.cpp ChangeGameSpeed()` now uses `_settings_client.gui.fast_forward_speed_limit` instead of a hardcoded 2500%.
- `_ap_ff_speed` resets to 100 on session start and is saved/loaded in the savegame (KV key `ff_speed`).

---

## [exp-1.1] — patch_exp_1_1_21 — 2026-03-10

### Changed
- **Real-time tracking (250 ms)** — `CheckMissions()`, `AP_UpdateSessionStats()`, `AP_UpdateNamedMissions()` and `AP_ColbyTick()` now run every 250 ms instead of ~5 seconds. The missions window updates continuously instead of waiting 5 sec per tick. Named-destination progress (town/industry deliveries) now accumulates in real time instead of monthly.
- Engine lock sweep still runs ~5 sec (too expensive to run every 250 ms — iterates all engines).
- Win condition check still runs ~10 sec (rarely relevant, cheap guard).

---

## [exp-1.1] — patch_exp_1_1_14 — 2026-03-10

### Changed
- **Vehicle missions split by category** — "Have X vehicles" is now split into separate missions per vehicle type: trains, road vehicles, ships and aircraft. Ships and aircraft are introduced from medium difficulty; they do not appear in easy. All types use +7 progression (10 -> 17 -> 24 -> 31 -> 38 on easy; 45/80/150 starting value on medium/hard/extreme).
- **Active vehicle requirement** — A vehicle only counts toward "Have X active trains/ships/etc." if it has been running for at least 30 calendar days AND has earned money (i.e. made at least one delivery). Vehicles bought and left in a depot do not count. Implemented in `AP_CountActiveVehicles()` via `v->age >= 30` and `profit_this_year / profit_last_year > 0`.
- **Station missions split by type** — "Build X stations" is now expanded into separate mission types: train stations, bus stops, truck stops, docks and airports. Docks and airports are introduced from medium difficulty. Adds ~16-20 extra entries per difficulty to the pool.
- **Active station requirement** — A station only counts toward a station mission if cargo has ever been delivered to it (`GoodsEntry::State::EverAccepted`). Players cannot just build stations and leave them empty. Implemented in `AP_CountStations(facility, require_active)`.
- **Pool sizes** — easy: 83, medium: 98, hard: 92, extreme: 77 entries.

---

## [exp-1.1] — patch_exp_1_1_13 — 2026-03-10

### Changed
- **Vehicle count missions: +7 progression, all types** — All "Have X vehicles/trains/road vehicles/ships/aircraft" missions now start at 10 (easy) and increase by 7 per step. Replaces the old uneven intervals (2->3->5->8->...). Applies to all difficulties and all vehicle types.

---

## [exp-1.1] — patch_exp_1_1_12 — 2026-03-10

### Changed
- **Predefined mission pools** — `_generate_missions()` now uses fixed, predefined missions instead of a random generator with min/max ranges. Eliminates duplicates and near-duplicates (e.g. "Have 2 trains" + "Have 3 trains" in the same session). The pool is shuffled and the first N missions are selected. If a session requires more missions than the pool contains, the pool is reshuffled and reused.
- **Shop tier locking** — The first 5 shop slots are always unlocked. Each additional group of 5 slots requires 5 more completed missions (any difficulty). Slots 6-10 require 5 total missions, slots 11-15 require 10, etc. Shown in the GUI as grey `[LOCKED] Complete X missions to unlock`. Purchasing locked items is blocked with a console message.

---

## [exp-1.1] — 2026-03-10

### Fixed
- **"Unknown item" on AP hints** — Items and locations shared the same base ID (`6_100_000`). When other players used `!hint` on an OpenTTD item, the AP server looked up the ID and found a *location* instead of an item name, displaying "Unknown item (ID:...)". Fix: item base ID moved to `6_200_000` (items.py). Locations remain at `6_100_000+`. No overlap is possible.
- **Console shows all missions as Easy / wrong mission numbers** — Location IDs were assigned sequentially from `6100000` based on the total mission count at runtime. The AP server's data package used the class-level table with the maximum count — both counted from the same base and ended up with different ID-to-name mappings. Result: `Mission_Medium_001` had a different ID in the data package vs. the active session, so the console and tracker showed everything as Easy. Fix: fixed per-difficulty ID blocks, independent of total mission count (Python + C++ synchronised):
  - Easy: `6100000-6101999`
  - Medium: `6102000-6103999`
  - Hard: `6104000-6105999`
  - Extreme: `6106000-6107999`
  - Shop: `6108000-6109999`
  - Victory: `6110000`
- **DeathLink ConnectUpdate error** — The `ConnectUpdate` packet contained `"items_handling": 7`, which is not a valid field in the ConnectUpdate protocol. AP 0.6.6 silently rejected the entire packet, meaning the DeathLink tag was never registered. Fix: `ConnectUpdate` now only sends `{"cmd": "ConnectUpdate", "tags": [...]}`.

### Added
- **Mission Tier Gating** — Players must complete N missions of the previous tier before the next tier unlocks. Default N=5; can be set to 0 for no gating. New YAML option `mission_tier_unlock_count` (range 0-20). Easy is always available. Medium requires N easy, Hard requires N medium, Extreme requires N hard. Locked tiers are shown in grey in the missions window as `[LOCKED] Medium - Complete 3 more easy missions to unlock`.
- **Starting Vehicle Count adjusted** — `starting_vehicle_count` range changed to 1-5 (was 1-20); default 2.

---

## [exp-1.1] — 2026-03-09

### Fixed
- **DeathLink not working** — The Connect packet always sent `["DeathLink"]` tag regardless of the setting. Fix: Connect now sends empty tags; a `ConnectUpdate` packet is sent immediately after slot_data is received with the correct tags based on the `death_link` value from the YAML. `AP_OnDeathReceived` and `AP_SendDeath` both now guard against `death_link == false`.
- **InvalidGame on connect** — C++ sent `game: "OpenTTD"` but the exp APWorld is named `OpenTTD-Exp`. Fix: default game name in `archipelago.h` corrected to `"OpenTTD-Exp"`.
- **CMake install error** — `known-bugs.md` not found during build because the file is actually named `KNOWN_BUGS.md`. Fix: `InstallAndPackage.cmake` corrected to match the actual filename.

### Added
- **Trap: Vehicle License Revoke** — New trap that suspends a random vehicle category (Trains / Road Vehicles / Aircraft / Ships) for 1-2 in-game years. All engines of that type are hidden via `company_hidden` and automatically restored when the timer expires. Saved/loaded in savegame via `lic_ticks` + `lic_type` in the APST chunk. Toggle option `trap_license_revoke` in the APWorld.
- **Wagon Pool Mode** — New YAML option `wagon_pool_mode` with three states: `all_wagons` (default — wagons in pool as normal), `no_wagons` (all wagons available from the start, none in pool), `start_with_one` (one random wagon per climate group given for free, rest removed from pool).

---

## [exp-1.0] — 2026-03-09

### Added
- First experimental release based on stable v1.0.0.
- Separate APWorld (`openttd_exp.apworld`) with game name `OpenTTD-Exp` — can sit side by side with stable in `custom_worlds/`.
- Separate GitHub repository: `github.com/solida1987/openttd-archipelago-exp`.

### Fixed (summary of issues resolved during beta)
- See stable v1.0.0 CHANGELOG for complete beta history.
