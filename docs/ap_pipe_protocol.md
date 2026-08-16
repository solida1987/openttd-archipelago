# The launcher pipe protocol

OpenTTD used to talk to the Archipelago server itself: raw sockets, two TLS
implementations, WebSocket framing, all in `src/archipelago.cpp`. The launcher
already has an AP client, so that was two clients for one connection — and the
one thing neither could do was check, before the game started, that the player
actually has the NewGRFs the seed was built from.

So the game now talks to the launcher over a named pipe, and the launcher owns
the Archipelago connection. `ArchipelagoClient`'s public methods are unchanged;
only what happens inside them is different.

---

## Transport

- Windows named pipe, `\\.\pipe\<name>`, byte stream, UTF-8.
- The launcher creates the server and passes `<name>` to the game with
  `-ap-pipe <name>` on the command line.
- One message per line, terminated by `\n`. No length prefix.
- Fields are separated by `:`. The **last** field may contain `:`, so parsers
  split from the left a fixed number of times.
- A line the receiver does not recognise is ignored, not an error. That is what
  lets one side gain a message before the other learns it.

---

## Game to launcher

| Message | Meaning |
|---|---|
| `HELLO:<version>` | first line after connecting; `<version>` is the protocol revision, currently `1` |
| `CHECK:<id>` | location checked, numeric AP id |
| `CHECKNAME:<name>` | location checked by name, when the game has no id |
| `GOAL:` | goal completed |
| `DEATH:<cause>` | DeathLink out; `<cause>` is free text |
| `SAY:<text>` | chat or `!command` to the server |
| `SCOUT:` | request scout data for shop locations |
| `GRF:<grfid>:<version>` | one loaded NewGRF, sent after `HELLO` |
| `GRFEND:` | end of the GRF list |
| `LOG:<text>` | diagnostic line for the launcher log |

## Launcher to game

| Message | Meaning |
|---|---|
| `STATE:<n>` | connection state: 0 disconnected, 1 connecting, 2 connected, 3 error |
| `ERROR:<text>` | human-readable reason for state 3 |
| `SLOTDATA:<json>` | the raw `slot_data` object, one line, no newlines inside |
| `ITEM:<id>:<index>` | item received; `<index>` is the AP resume index |
| `MISSING:<id>,<id>,…` | locations still unchecked |
| `LOCCOUNT:<n>` | how many locations the seed has, for the "x of y" counter |
| `HINT:<location>:<label>` | scout result, `player (game)`; keyed by location **name** |
| `DEATHLINK:<cause>` | DeathLink in |
| `PLAYERS:<name>,<name>,…` | slot names in the room |
| `REJECT:<text>` | the launcher refuses to start play; the game shows this and stops |

---

## Order of events

```
game                          launcher
  |-- HELLO:1 ----------------->|
  |-- GRF:43411223:8948 ------->|
  |-- GRF:f1250009:7366 ------->|
  |-- GRFEND: ----------------->|
  |                             |  compares the list with slot_data
  |<------------- SLOTDATA:{…} -|  (or REJECT: and nothing else)
  |<------------- MISSING:1,2,3 |
  |<------------- STATE:2 ------|
  |-- CHECK:101 --------------->|
  |<------------- ITEM:55:0 ----|
```

The GRF list comes **before** `SLOTDATA`, so the launcher can refuse a seed the
player cannot finish instead of letting them discover it three hours in. A
`REJECT:` is final: the game shows the text and does not start play.

---

## Rules that keep it debuggable

- **Everything is text.** A protocol you can read in a log is a protocol you can
  fix from a bug report.
- **`SLOTDATA` is one line.** The JSON must be minified. OpenTTD's slot_data is
  large — missions, ruins, stars, demigods, the item id map — and splitting it
  across lines would mean a reassembly buffer for no benefit.
- **Unknown messages are ignored.** Either side can be older than the other.
- **The game never retries on its own.** If the pipe drops, the game reports
  state 0 and waits. Reconnection is the launcher's business.

---

## Standalone

The same pipe carries the tracker feed with no AP session behind it: the
launcher answers `SLOTDATA` from a local seed and never sends `ITEM`. The game
cannot tell the difference and does not need to.
