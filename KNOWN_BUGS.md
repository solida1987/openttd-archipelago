# Known Bugs & Limitations — OpenTTD Archipelago

Sidst opdateret: Beta 8 (2026-03-08)

---

## 🔴 Kritiske bugs (game-breaking)

*Ingen kendte kritiske bugs i beta 9.*

---

## 🟠 Alvorlige bugs (forkert opførsel)

*Ingen kendte alvorlige bugs i beta 8.*

---

## 🟡 Medium bugs (noget er forkert men ikke game-breaking)

### Shop `OnRealtimeTick` poller unødigt efter hints er loaded
**Status:** Åben  
**Siden:** Beta 1  
**Beskrivelse:** `ArchipelagoShopWindow::OnRealtimeTick` kører hvert 250ms og tjekker
om der er "loading..."-labels. Selv efter alle hints er modtaget og alle labels er
resolved, fortsætter polling. Ubetydelig CPU-effekt, men unødigt.  
**Fix:** Tilføj et `_hints_loaded`-flag der stopper polling når alle labels er gyldige.

### WebSocket compression-advarsel i server-log
**Status:** Åben  
**Siden:** Beta 1  
**Beskrivelse:** Serveren logger en advarsel om WebSocket permessage-deflate
compression. Funktionaliteten virker korrekt — det er en serverside-advarsel der
ikke påvirker gameplay.  
**Fix:** Lav en korrekt compression-handshake i `archipelago.cpp`.

---

## 🔵 Kendte begrænsninger (by design eller lav prioritet)

### AP-knap viser standard-ikon i stedet for AP-logo
**Status:** Åben — lav prioritet  
**Siden:** Beta 8  
**Beskrivelse:** Archipelago-knappen (toolbar + intro-menu) viser et generisk OpenTTD-ikon i stedet for AP-logoet. C++ peger korrekt på sprite 714, og `archipelago_icons.grf` indeholder korrekt Action A der erstatter sprite 714 med AP-logoet. Problemet er i `gfxinit.cpp`'s GRF-loading mekanisme — OpenTTD's NewGRF-loader processerer ikke vores GRF's Action A korrekt i den kontekst vi loader den. Problemet har vist sig svært at fixe uden adgang til fuld kildekode og debugger.  
**Workaround:** Ingen — knappen er fuldt funktionel, kun ikonet er forkert.  
**Fix:** Gennemgå `LoadNewGRF` base-set sprite replacement mekanisme med adgang til debugger/fuld kildekode.

### Multiplayer (flere companies) ikke understøttet
**Status:** Ikke planlagt  
**Beskrivelse:** Kun single-company gameplay er understøttet. Med flere companies
ville engine lock-systemet og mission-tracking skulle have per-company logik.

### Windows-only TLS/WSS
**Status:** Lav prioritet  
**Beskrivelse:** WSS (WebSocket Secure) bruger Windows Schannel (`Secur32.dll`).
Linux/macOS builds kan kun forbinde via plain WS (port 38281 uden SSL).

### `£`-tegnet i item-navne er platform-afhængigt
**Status:** Lav prioritet  
**Beskrivelse:** `UTILITY_ITEMS` i `items.py` bruger `£` direkte (UTF-8 `\xC2\xA3`).
C++ sammenligner med hardkodede strenge der indeholder samme tegn. Risiko for
mismatch på Windows-systemer med ikke-UTF-8 system-locale (sjælden, men mulig).

---

## ✅ Løste bugs (arkiv)

| Version | Bug | Fix |
|---------|-----|-----|
| Beta 9 | **Named missioner viste hvide** — difficulty-farven overskrevet til TC_WHITE for alle named-destination missioner | Removed TC_WHITE override; named missions beholder difficulty-farve |
| Beta 9 | **`mission_count=300` gav 250 missioner** — pool-size brugte scale-faktor mod base 350, minus shop slots | mission_count bruges nu direkte som antal missioner |
| Beta 9 | **`maintain`-type brugte skrøbelig string-søgning** — `type.find("90")` for tærskeldetektering | Python emitter nu `maintain_75`/`maintain_90`; C++ matcher eksplicit |
| Beta 9 | **Shop label-fallback viste rå `Shop_Purchase_NNNN`** — ingen nyttig tekst ved manglende item-hints | Fallback viser nu `"Shop Slot #N"` |
| Beta 9 | **Sugar/Toys/Batteries manglede fra Toyland cargo-pool** | Tilføjet til `CARGO_BY_LANDSCAPE[3]` |


| Beta | Bug | Fix |
|------|-----|-----|
| Beta 8 | **Iron Horse engines låstes ikke (locked_vehicles IH-prefix mismatch)** — `locked_vehicles` indeholdt `"IH: 4-4-2 Lark"` men C++ søgte på `"4-4-2 Lark"` → IH engines aldrig låst | Begge lock-steder tjekker nu også `"IH: " + eng_name` |
| Beta 8 | **Server crash "No location for player 1"** — shop_slots*20 vs direkte shop_item_count | Læser shop_item_count direkte; fallback til shop_slots*20 for gamle saves |
| Beta 8 | **Startende tog forkert på Temperate/Arctic/Tropic** — ingen klimafiltrering i STARTING_VEHICLES | ARCTIC_TROPIC_ONLY_TRAINS + TEMPERATE_ONLY_TRAINS frozensets tilføjet |
| Beta 8 | **Mission- og shop-vinduer kun lodrette resize** — step_width aldrig sat | step_width = 1 i begge vindues-konstruktører |
| Beta 8 | **10 Toyland-cargos manglede i BuildCargoMap()** — AP_FindCargoType() returnerede INVALID_CARGO for Sugar/Toys/Batteries/Sweets/Toffee/Cola/Candyfloss/Bubbles/Plastic/Fizzy Drinks — Toyland transport-missioner talte alt cargo i stedet for specifik type | Alle 10 Toyland-labels tilføjet til kNameLabel[] |
| Beta 8 | **`cargo_to_industry` tæller ikke alle cargo-typer** — kun slot 0, rapporteret af killeroid356 (Steel talte ikke i Cadton Factory) | Itererer nu over alle `ind->accepted`-slots |
| Beta 8 | **`cargo_from_industry` tæller ikke alle cargo-typer** — kun slot 0 (Oil Refinery: kun Oil, ikke Goods) | Itererer nu over alle `ind->produced`-slots |
| Beta 8 | **`cargo_type` sentinel brugte slot[0]** — 0xFF-guard skippede gyldige missioner lydløst | Alle fire assignments bruger nu første *gyldige* slot |
| Beta 8 | **Guru Galaxy (ID 232) fejlagtigt ekskluderet fra non-Toyland** | Fjernet fra `TOYLAND_ONLY_VEHICLES` |
| Beta 8 | **10 Toyland-only køretøjer manglede i filteret** — sendt som items til non-Toyland spillere | Tilføjet alle 10 til `TOYLAND_ONLY_VEHICLES` |
| Beta 7 | **DeathLink virker aldrig** — `death_link` manglede i `fill_slot_data()` | Tilføjet `"death_link": bool(self.options.death_link.value)` til `fill_slot_data()` |
| Beta 7 | **Timed effects løber af i menu/pause** — timer-tæller decrementerede udenfor `GM_NORMAL` | Alle 4 timer-decrements wrappet i `if (_game_mode == GM_NORMAL)` |
| Beta 7 | **Shop-rotation vises ikke straks** — manglede `OnInvalidateData()` override | `OnInvalidateData()` tilføjet til `ArchipelagoShopWindow` |
| Beta 7 | **C++ location IDs fra beregnet distribution, ikke faktiske missions** — mismatch ved `max_attempts` | `location_ids` bygges nu fra `sd.missions`-array direkte |
| Beta 7 | **Loan Reduction spildt ved lån=0** — ingen effekt, ingen besked | Konverteres nu til kontanter med besked |
| Beta 7 | **Shop spammede samme utility item** — fast rækkefølge i pool | Utility-pool shuffles nu i tilfældige batches |
| Beta 7 | **`location_name_to_id` for lille — missions 301+ manglede fra AP registry** | class-level dict bruger nu mission_count=1140 |
| Beta 7 | **Timed effects nulstilledes ved save/load** — timers ikke gemt i APST | Gemmes/gendannes nu via AP_GetEffectTimers/AP_SetEffectTimers |
| Beta 7 | **`town_population` win: "single town" vs faktisk world total** | Dokumentation og docstring rettet |
| Beta 7 | **Recession trap gav bonus til spillere i gæld** — halvering af negativ Money reducerede gæld | Gæld-path tilføjer nu lån-straf i stedet |
| Beta 7 | **Town Growth Boost muterede growth_rate permanent** — stackede til 1, byer stoppede med at vokse | Nulstiller nu kun grow_counter (puls) |
| Beta 6 | Mission tekst knust ved UI scale ≥2 | Row height genberegnes ved draw-time |
| Beta 6 | Valuta viser £ på ikke-GBP spil | Currency-prefix erstattes ved render-tid |
| Beta 6 | `random` starting vehicle gav ubrugelig cargo wagon | Multi-niveau fallback |
| Beta 6 | Shop viste færre items end konfigureret `shop_slots` | Læser nu YAML-option direkte |
| Beta 6 | AP status-vindue kunne ikke trækkes | `WDP_AUTO` med persistens-key |
| Beta 6 | "Unknown item: not handled" for vanilla engines | `BuildEngineMap()` efter expire-flag |
| Beta 6 | Oil Tanker kun unlockede én wagon-variant | Parallel `_ap_engine_extras`-map |
| Beta 6 | IH engine prefix mismatch i lock check | `IH: ` prefix strippes før opslag |
| Beta 5 | Toyland missions på ikke-Toyland maps | Climate-filtreret cargo-liste |
| Beta 5 | Toyland køretøjer i pool på forkert map | Ekskluderes fra randomiseret pool |
| Beta 5 | "Service X towns" umulig på små maps | Cap baseret på map-dimensioner |
| Beta 5 | Bank Loan trap hardkodet til 500M | Skaleres til `max_loan` |
| Beta 5 | Shop afviste køb / forkert prisvisning | Købte slots filtreres øjeblikkeligt |
| Beta 4 | Wine inkompatibilitet med WSS | WSS-probe springes over under Wine |
| Beta 4 | "Missing 140 sprites" advarsel | Warning-widget skjules |
| Beta 4 | Starting vehicle låst på forkert klima | Toyland-only ekskluderes fra starter-pool |
| Beta 3 | "Maintain X% rating" tracker forkert | Korrekte konsekutive måneder |
| Beta 3 | DeathLink notification usynlig | Fuld avisopslag ved inbound death |
| Beta 3 | AP-state tabt ved save/load | APST savegame-chunk tilføjet |
| Beta 2 | WSS/WS auto-detection | Prober WSS, falder tilbage til WS |
| Beta 2 | Build fejlede uden zlib | `#ifdef WITH_ZLIB` guard tilføjet |
