# Known Issues — OpenTTD Archipelago

This file covers known issues specific to the Archipelago integration.
For base OpenTTD known bugs, see the upstream repository.

## Archipelago Integration

### WebSocket compression not supported
The client does not support permessage-deflate compression.
The Archipelago server logs a warning about this but the connection works normally.
Compressed frames are skipped gracefully.
**Severity:** Low — no gameplay impact.
**Note:** Will be resolved if zlib is included in the build via vcpkg.

### Multiplayer (multiple companies) not supported
All Archipelago logic assumes `_local_company`.
Running multiple human companies in the same OpenTTD game is not supported.
Co-op within a single company works fine.
**Severity:** Medium — do not start multiplayer OpenTTD games with this build.

### Windows-only build
The TLS (WSS) implementation uses Windows Schannel.
Linux and macOS builds will connect via plain WS only (no wss://).
**Severity:** Low for current usage — Windows build is the primary release target.

### Wine incompatibility (Beta 3 — fixed in Beta 4)
Beta 3 introduced WSS/WS auto-detection using the Windows Schannel TLS API
(`Secur32.dll`). Wine's Schannel implementation does not support the
`UNISP_NAME_A` provider used by the WSS probe, causing the connection worker
thread to fail before a plain WS fallback can be attempted.

**Beta 3 workaround:** Use Beta 2, which has no TLS code and connects via
plain WS only.

**Beta 4 fix:** Wine is detected at runtime via its `HKLM\Software\Wine`
registry key. When running under Wine the WSS probe is skipped entirely and
the client connects via plain WS directly, matching Beta 2 behaviour.

### Base graphics warning on first launch (fixed in Beta 4)
If built without PNG/zlib via vcpkg the game may show "missing 140 sprites" on the
intro screen. This is a cosmetic issue with the baseset and does not affect gameplay.
Fix: build with vcpkg toolchain so PNG and zlib are included.
**Severity:** Low — cosmetic only.
**Beta 4 fix:** The baseset warning widget is now unconditionally hidden in the
Archipelago build. The bundled baseset is complete; the warning was a false positive
caused by the build not being tagged as a vanilla "released version".
### Mission menu text crushed at UI scale ≥2 (fixed in Beta 6)
Row height was cached at window construction time. At UI scales ≥2 the font
height changes, causing rows to overlap and text to become unreadable.
Row height is now recomputed each draw call.
**Severity:** Medium — cosmetic but made missions unreadable at higher scales.

### Currency shows £ on non-GBP games (fixed in Beta 6)
Mission descriptions were generated with a hardcoded £ symbol and never
updated to match the player's chosen currency. The £ is now replaced at
render time with the active currency prefix from `GetCurrency()`.
**Severity:** Low — cosmetic, caused confusion but no gameplay impact.

### `random` starting vehicle type could give cargo wagon only (fixed in Beta 6)
In edge cases the selected starting vehicle could not be found in the engine
map (name mismatch, map built before NewGRF loaded, etc.), leaving the player
with nothing unlocked. A three-level fallback now ensures a vehicle is always
provided: cargo truck → bus, engine map rebuild + retry, emergency loco list.
**Severity:** High — player could start with no usable vehicle.

### Shop shows fewer items than configured `shop_slots` (fixed in Beta 6)
`_compute_pool_size()` computed its own slot count internally and completely
ignored the player's YAML `shop_slots` option. Setting `shop_slots: 5` would
often result in only 3–4 visible shop items. Now reads the YAML value directly.
**Severity:** Medium — YAML option had no effect.

### AP status window could not be dragged (fixed in Beta 6)
Status window used `WDP_MANUAL` placement with a hardcoded screen position.
This caused it to snap back or appear jammed in the toolbar area with no way
to move it. Changed to `WDP_AUTO` with a persistence key so the position is
remembered and the window is freely draggable.
**Severity:** Low — cosmetic annoyance, no gameplay impact.


Mission generator could produce Toyland cargo missions (Candyfloss, Cola etc.)
and the item pool could contain Toyland-only vehicles even when the map was
Temperate, Arctic or Tropical. Both are now filtered by landscape at world
generation time.
**Severity:** Medium — missions were impossible; received vehicles did nothing.

### "Service X towns" impossible on small maps (fixed in Beta 5)
Mission generator did not know how many towns a given map size would produce,
leading to "Service 60 different towns" on a 64×64 map. Amounts are now capped
to a realistic estimate based on `map_x`/`map_y`.
**Severity:** Medium — some missions could not be completed.

### Bank Loan Forced trap — 500M hardcoded (fixed in Beta 5)
The Bank Loan Forced trap set `current_loan` to £500,000,000 regardless of
the session's `max_loan` setting. On default settings this made the trap
nearly unrecoverable. Now scales to `max_loan`.
**Severity:** High — trap was disproportionately punishing.

### Shop allowing re-purchase of already-bought slots (fixed in Beta 5)
After buying a shop slot, it remained visible and selectable (though clicking
buy would silently fail after deducting money). Bought slots are now removed
from the shop list immediately.
**Severity:** Medium — confusing UX, potential money loss.
