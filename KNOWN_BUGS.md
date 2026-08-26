# Known Bugs & Limitations — OpenTTD Archipelago

Last updated: 2026-08-26

Only things actually observed go in here. A bug nobody has reproduced is a
guess, and a guess in this file reads as a warning.

---

## 🔴 Critical bugs (game-breaking)

*None known.*

## 🟠 Serious bugs (incorrect behaviour)

*None known.*

## 🟡 Medium bugs (something is wrong but not game-breaking)

*None known.*

---

## Fixed in 2.1.0

### Ruins sharing a town never counted their cargo

Reported by Rafcor: a game started with six ruins, one was cleared, and the
seventh that replaced it accepted cargo without the total ever moving.

Root cause: OpenTTD's cargo monitors reset as they are read. Missions, tasks
and ruins all encode the same key for the same company+cargo+town, and the
pass guarded against reading twice by *skipping* the second reader instead of
sharing what the first one got. Because every ruin is guaranteed a basic cargo
(the soft-lock guard), two ruins near one town always collide — so this was
the normal case, not an edge case.

Fixed by caching the amount rather than the fact: one drain per monitor per
tick, and every reader gets the same number. One delivery satisfying a mission,
a task and a ruin at once is correct — they all asked for exactly that.

### The Colby event could stall depending on climate

Same root cause, one step worse: Colby read his monitor in a separate pass
*after* the shared one, so he was not protected at all. With his own CLBY
cargo nothing went wrong, but on Tropical and Toyland that cargo does not
exist and the fallback is an ordinary one — which missions, tasks and ruins
also use. Colby then read zero forever and the event never advanced.

Fixed by moving Colby into the shared pass.

### Missions credited the wrong tasks

The old hand-written "credit tasks sharing this monitor" loop matched on
entity id alone, so a mail delivery credited a passenger task in the same
town. Removed: each task now looks up its own monitor through the cache.

### The Wrath tree limit could not be changed

The game enforced four Wrath limits (houses, roads, terrain, trees) but the
apworld only sent three. `wrath_limit_trees` had no option at all, so it was
permanently stuck at its default of 10 — the player was punished for felling
more than ten trees a year with no way to adjust it. Now an option like the
other three.

---

## 🔵 Known limitations (by design or low priority)

### Multiplayer (multiple companies) not supported

### Windows-only TLS/WSS

### `£` character in item names is platform-dependent

### WebSocket compression not supported

### OpenTTD lists only one downloaded set at a time in the menu

Measured 26 Aug 2026 with two sets downloaded into content_download/newgrf/
(Iron Horse and FIRS). Each is read correctly on its own, but with both
present the game's own scan reports only the one that sorts first by filename
— renaming the other to sort first swaps which one appears. Waiting two
minutes does not change it, so it is not the scan still running.

This is the game's behaviour in the menu, not the launcher's: the launcher's
own scan reads both archives and the badges are correct either way. Enabling
a set through OpenTTD's NewGRF window is unaffected as far as we have tested.

### NewGRF sets cannot be loaded mid-session

OpenTTD reads NewGRFs at startup. When the launcher fetches a missing set for
you, the current session still ends — restart the game, enable the set, and
reconnect. This is the game's own behaviour, not something the bridge can work
around.
