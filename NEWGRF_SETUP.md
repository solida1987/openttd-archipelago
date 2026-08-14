# NewGRF vehicle sets — installing them yourself

This package does **not** include any third-party vehicle or industry sets. It
used to, and that was a mistake on our part: those sets are other people's work,
several of them are licensed GPL v2 — which means whoever hands you the binary
owes you the source as well — and a few carry no licence statement at all. We
were not in a position to pass any of them on, so they are gone from the
download.

Nothing is lost by this. OpenTTD has a built-in downloader that fetches these
sets from the authors' own distribution service, keeps them updated, and knows
exactly which version you have. That is a better way to get them than a copy
bundled by a third party, which is what we were.

---

## The short version

1. Start `openttd.exe`
2. Main menu → **Check Online Content**
3. Search for the set you want, tick it, click **Download**
4. Main menu → **NewGRF Settings** → move the set from the right-hand list to
   the left-hand list, so it is *active* and not merely *downloaded*
5. Start your Archipelago game

Step 4 is the one people miss. A set that sits in your download folder without
being switched on is not loaded, and as far as the game is concerned you do not
have it.

---

## Which sets a seed needs

That is decided when the multiworld is generated, not when you play. Whoever
generated the seed chose it in their YAML, and everyone in that seed needs the
same sets.

If your YAML has none of the options below turned on, you need nothing from
this page — the seed will use OpenTTD's own vehicles and industries.

| YAML option | Set | Author | Licence as stated in the file |
|---|---|---|---|
| `enable_iron_horse` | Iron Horse | andythenorth | GPL v2 |
| `enable_firs` | FIRS Industries | andythenorth | GPL v2 |
| `enable_heqs` | HEQS heavy equipment | andythenorth | *no statement in the file* |
| `enable_shark_ships` | SHARK ships | — | GPL v2 |
| `enable_military_items` | Military Items | adpro | GPL v2, © 2021 |
| `enable_vactrain` | Vactrain Set | — | GPL v2 or later |
| `enable_aircraftpack` | Aircraftpack 2025 | — | *no statement in the file* |
| `enable_hover_vehicles` | Hover Vehicles | — | *no statement in the file* |

The licence column is what we read out of the files themselves during the
review that led to this page. It is recorded here so nobody has to repeat that
work, not as legal advice — the authors' own pages are the authority.

---

## Versions matter

A seed is generated against a *specific build* of each set. Install a different
one and the item pool may name vehicles your copy does not have.

| Set | GRFID | Build the apworld expects |
|---|---|---|
| Iron Horse | `43411223` | 8948 or newer |
| FIRS | `f1250009` | 7366 or newer |
| SHARK | `4a44bbb1` | 1720 or newer |
| HEQS | `41501202` | 5199 or newer |
| Military Items | `41440101` | 12 or newer |
| Vactrain Set | `444a5901` | 80 or newer |
| Aircraftpack 2025 | `4c480101` | 6 or newer |
| Hover Vehicles | `485a0101` | any |

Check Online Content normally gives you the newest build, which is what you
want.

---

## There is no check, and that is on purpose

**This version of the game does not verify that you have the right sets.** It
will start whether you have them or not. If a seed needs Iron Horse and you do
not have it, the game will run and the items for those trains will simply never
be usable — possibly hours in, with no message telling you why.

So: read the YAML the seed was generated from, install what it asks for, and
switch it on before you start. If you are joining somebody else's multiworld,
ask them which sets they enabled.

---

## Troubleshooting

**"I downloaded it but the vehicles are not there."**
It is downloaded but not active. NewGRF Settings → move it to the left-hand
list. This catches almost everybody once.

**"Check Online Content finds nothing."**
It needs an internet connection and it talks to `bananas.openttd.org`. A
firewall blocking OpenTTD will make the list come up empty rather than show an
error.

**"I have the set but the game says the version is wrong."**
Download the newest build through Check Online Content. A set installed by hand
years ago can be several thousand builds behind.

**"Can I just copy the .grf file from somewhere?"**
It will work, but you will not get updates and you will not know which build
you have — which is precisely the situation that led to this page existing.
