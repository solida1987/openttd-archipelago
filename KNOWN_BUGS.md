# Known Issues — OpenTTD Archipelago

This file covers known issues specific to the Archipelago integration.
For base OpenTTD known bugs, see the upstream repository.

---

## Active Known Issues

### WebSocket compression not supported
The client does not support permessage-deflate compression.
The Archipelago server logs a warning about this but the connection works normally.
Compressed frames are skipped gracefully.
**Severity:** Low — no gameplay impact.
**Note:** Will be resolved if zlib WebSocket support is added to the client.

### Multiplayer (multiple companies) not supported
All Archipelago logic assumes `_local_company`.
Running multiple human companies in the same OpenTTD game is not supported.
Co-op within a single company works fine.
**Severity:** Medium — do not start multiplayer OpenTTD games with this build.

### Windows-only TLS build
The TLS (WSS) implementation uses Windows Schannel.
Linux and macOS builds will connect via plain WS only (no wss://).
**Severity:** Low for current usage — Windows build is the primary release target.

### AP re-sends all items on reconnect — traps may fire twice
When the connection to the AP server drops and reconnects, the server re-sends
the full item list. Trap items that were already applied will be applied again
(Fuel Shortage re-engages, Bank Loan re-fires, etc.).
This is a consequence of the stateless item delivery model and affects all
Archipelago games that reconnect mid-session.
**Severity:** Medium — unexpected trap re-triggers. Workaround: avoid disconnecting
mid-session, or save before connecting if reconnects are expected.

### Named missions: industry may close before mission is complete
OpenTTD can close industries that are not being serviced. If the industry assigned
to a `cargo_from_industry` or `cargo_to_industry` mission closes, the mission
will remain in the list but can no longer be completed.
`AP_UpdateNamedMissions()` marks the mission as completed (auto-win) if the
industry is `nullptr`, but this may feel inconsistent to players who had started
working towards it.
**Severity:** Low — only affects sessions where industries close and no service
has been established. The `no_industry_close` option mitigates this.

### `locked_vehicles` YAML option not enforced in C++ lock logic
The `locked_vehicles` list from slot_data is parsed and stored in
`_ap_pending_sd.locked_vehicles` but the periodic engine lock sweep ignores it
— it locks all non-AP-unlocked engines regardless of this list.
In practice this is the intended behaviour (all engines are locked until AP
sends them), but the field is effectively unused and could cause confusion if
it was intended to allow pre-unlocked vehicles.
**Severity:** Low — no gameplay impact in current usage.

### Named mission `cargo_type` defaults to first produced/accepted slot only
`AP_AssignNamedEntities` always uses `ind->produced[0].cargo` or
`ind->accepted[0].cargo` for the cargomonitor lookup. Industries with multiple
produced or accepted cargos will only have their first cargo tracked.
**Severity:** Low — most industries have a single primary cargo. Edge cases with
multi-output industries (e.g. Oil Refinery producing both Petrol and Chemicals
in some NewGRFs) may track only one.

---

## Fixed in Beta 7

### Named missions never replaced [Town] / [Industry near Town] (fixed in Beta 7)
Python computed `type` from the template text before `{amount}`, so named missions
always had wrong types (`"deliver"`, `"supply"`, `"transport"`) instead of
`"passengers_to_town"` etc. C++ never matched these in `AP_AssignNamedEntities`
so no assignment ever happened and the placeholder text was never replaced.
**Fixed:** Python now sends `type = unit` directly for named mission templates.

### Named missions tracked all companies, not just the player's (fixed in Beta 7)
`AP_UpdateNamedMissions()` used global town/industry statistics that include
AI company deliveries. Any AI train servicing the target town would count toward
the player's mission progress.
**Fixed:** replaced with `cargomonitor.h` API (`GetDeliveryAmount` /
`GetPickupAmount`) which tracks per-company cargo flow.

### `cargo_to_industry` missions could never be completed (fixed in Beta 7)
Python generated `type="supply"` for these missions. `EvaluateMission()` had no
handler for `"supply"` so `current_value` was always 0.
**Fixed:** as part of the type-key fix above.

### `cargo_from_industry` missions counted all cargo session-wide (fixed in Beta 7)
Python generated `type="transport"` which matched the generic transport handler
and summed all cargo ever delivered — not just from the target industry.
**Fixed:** as part of the type-key fix above.

### [Town]/[Industry] still shown after save/load (fixed in Beta 7)
`AP_SetNamedEntityStr()` only restored `id` and `cumulative` from the save chunk.
`name` was always empty after a reload so `AP_StrReplace` never ran.
**Fixed:** `AP_SetNamedEntityStr()` now re-resolves `name`, `tile`, and
`cargo_type` from the live `Town`/`Industry` object after every load/reconnect.

### Named missions showed no progress bar at 0 (fixed in Beta 7)
The GUI only showed a progress string when `current_value > 0`. Named missions
starting at zero had no visible `(0/15,000)` indicator.
**Fixed:** named missions always show a progress string regardless of current value.

### TileIndex cast errors in named mission scroll (fixed in Beta 7)
`(uint32_t)t->xy` and similar casts did not compile (TileIndex is a strong typedef).
**Fixed:** `.base()` for reading, `TileIndex{value}` for construction.

### Shop labels showed player name instead of item name (fixed in Beta 7)
`GetLocationHint()` returned the AP player name for shop locations.
**Fixed:** Python sends `shop_item_names` dict in slot_data; C++ uses it as
primary source in `AP_GetShopLocationLabel()`.

### DeathLink never functioned (fixed in Beta 7)
`fill_slot_data()` never included the `"death_link"` key so C++ always parsed it
as `false`.
**Fixed:** `"death_link": bool(self.options.death_link.value)` added to
`fill_slot_data()`.

### Timed effects ran down in menu/pause (fixed in Beta 7)
Timer counters decremented on every realtime tick regardless of `_game_mode`.
**Fixed:** decrements now only happen when `_game_mode == GM_NORMAL`.

### Shop rotation did not update UI immediately (fixed in Beta 7)
`ArchipelagoShopWindow` was missing `OnInvalidateData()`.
**Fixed:** `OnInvalidateData()` override added, calls `RebuildShopList()` directly.

### C++ location IDs out of sync with actual generated missions (fixed in Beta 7)
Location IDs were computed from hardcoded distribution percentages, not from the
actual mission list generated by Python.
**Fixed:** `location_ids` built directly from `sd.missions`.

### Loan Reduction wasted if loan already paid off (fixed in Beta 7)
If loan was 0, the item silently did nothing.
**Fixed:** three cases handled: normal reduction, partial loan, no loan (cash).

### Shop repeated the same utility item (fixed in Beta 7)
Utility pool was built by repeating `UTILITY_ITEMS` in fixed order.
**Fixed:** pool is now shuffled in batches for even distribution.

### `location_name_to_id` too small for large multiworlds (fixed in Beta 7)
Class-level dict was hardcoded to `mission_count=300` — missions 301+ were
invisible to Archipelago in large multiworld games.
**Fixed:** built with worst-case values `mission_count=1140, shop_slots=3`.

### Timed effects lost on save/load (fixed in Beta 7)
Timer counters were not included in the APST save chunk.
**Fixed:** all four timers saved and restored via `AP_GetEffectTimers()` /
`AP_SetEffectTimers()`.

### `town_population` win condition checked world population (fixed in Beta 7)
Description said "any single town" but C++ used `GetWorldPopulation()`.
**Fixed:** documentation updated to reflect actual behaviour (total world population).

### Recession trap gave bonus to players in debt (fixed in Beta 7)
`c->money / 2` on a negative value halved the debt instead of punishing the player.
**Fixed:** players in debt receive an extra 25% of `max_loan` as additional debt.

### Town Growth Boost permanently mutated `growth_rate` (fixed in Beta 7)
Halved `growth_rate` for all towns without ever restoring it.
**Fixed:** `grow_counter` is reset instead — triggers an immediate growth pulse
with no lasting side-effects.

---

## Fixed in Beta 6

### Mission text crushed at UI scale ≥2 (fixed in Beta 6)
Row height was cached at window construction. **Fixed:** recomputed each draw call.

### Currency showed £ on non-GBP games (fixed in Beta 6)
Hardcoded £ in mission descriptions. **Fixed:** replaced at render-time with
active currency prefix.

### `random` starting vehicle could give cargo wagon only (fixed in Beta 6)
Engine map lookup could fail leaving player with no vehicle.
**Fixed:** three-level fallback added.

### Shop showed fewer items than configured `shop_slots` (fixed in Beta 6)
`_compute_pool_size()` ignored the YAML option.
**Fixed:** reads `self.options.shop_slots.value` directly.

### AP status window could not be dragged (fixed in Beta 6)
`WDP_MANUAL` snapped back to fixed position.
**Fixed:** changed to `WDP_AUTO` with persistence key.

### Oil Tanker only unlocked one of three wagon variants (fixed in Beta 6)
**Fixed:** `_ap_engine_extras` map added.

### IH engine prefix mismatch in lock check (fixed in Beta 6)
**Fixed:** `IH: ` prefix stripped before engine map lookup.

---

## Fixed in Beta 5

### Toyland missions/vehicles on wrong climate (fixed in Beta 5)
**Fixed:** filtered by landscape at world generation.

### "Service X towns" impossible on small maps (fixed in Beta 5)
**Fixed:** capped to realistic estimate based on map dimensions.

### Bank Loan Forced trap — 500M hardcoded (fixed in Beta 5)
**Fixed:** scales to `max_loan`.

### Shop re-purchase of already-bought slots (fixed in Beta 5)
**Fixed:** bought slots removed from list immediately.

---

## Fixed in Beta 4

### Wine incompatibility (fixed in Beta 4)
**Fixed:** WSS probe skipped under Wine (detected via `HKLM\Software\Wine`).

### "Missing 140 sprites" warning on main menu (fixed in Beta 4)
**Fixed:** warning widget unconditionally hidden.

### Starting vehicle locked on non-Toyland maps (fixed in Beta 4)
**Fixed:** Toyland-only vehicles excluded from starter pool.

---

## Fixed in Beta 3

### "Maintain X% rating" missions tracked incorrectly (fixed in Beta 3)
**Fixed:** consecutive month counter implemented correctly.

### DeathLink notification missing (fixed in Beta 3)
**Fixed:** full news item shown on inbound death.

---

## Fixed in Beta 2

### WSS/WS connection failed on plain WS servers (fixed in Beta 2)
**Fixed:** probes WSS first, falls back to WS automatically.
