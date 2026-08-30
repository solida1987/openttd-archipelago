# Known Bugs & Limitations — OpenTTD Archipelago

Last updated: 2026-08-30

Only things actually observed go in here. A bug nobody has reproduced is a
guess, and a guess in this file reads as a warning.

---

## 🔴 Critical bugs (game-breaking)

*None known.*

## 🟠 Serious bugs (incorrect behaviour)

### Multiplayer can desync

Several systems decide what to do from `_local_company`, which is a property of
the client looking at the game rather than of the game itself. Cargo Bonus
doubles freight income for that company inside `GetTransportedGoodsIncome`, the
Wrath counters tally that company's demolitions, and the DeathLink penalty is
charged to it. The demigod system also changes company money and names directly
instead of going through the command system.

In singleplayer none of this can diverge. In a networked or bridged game the
server and each client can compute different numbers, which is what a desync is.
Multiplayer is not a supported configuration today; this is why.

## 🟡 Medium bugs (something is wrong but not game-breaking)

### v2.2.0 has not been played through

The release fixes a long list of faults found by reading the code, and it builds
with no errors or warnings, but the fixes have not been exercised in a live
session. The settings repair, the reconnect behaviour and the town/road change
are the three worth watching first.

### The logic does not know about the shop's mission gate

In game, shop slot N needs a number of completed missions to open — five more
missions for every five more slots. The generator's logic only asks whether the
player can move cargo at all, so it considers every shop slot reachable from the
moment they can. The two do not disagree about whether a slot can *ever* open,
which is why generation is sound, but they disagree about when: on the largest
seeds the last slots need almost every mission in the seed finished.

This is why the shop's own ceiling matters. The measured worst case is 858
slots, whose top slot needs 175 missions against the 200 such a seed carries.
Comfortable at ordinary sizes, tight at the extreme.

### NewGRF sets cannot be enabled when Documents is an unavailable OneDrive path

If `Documents` redirects into OneDrive and OneDrive is not installed, OpenTTD's
personal directory cannot be read, `openttd.cfg` cannot be written, and no
NewGRF is ever enabled — so every seed that needs one is refused. This is an
operating-system condition, not something the game or launcher can work around:
`SP_PERSONAL_DIR` comes from the home directory unconditionally, and `-c` does
not move it. Reinstalling OneDrive or moving `Documents` out of the OneDrive
path fixes it.

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
