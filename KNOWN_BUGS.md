# Known Bugs & Limitations — OpenTTD Archipelago

Sidst opdateret: Beta 9 (2026-03-08)

---

## 🔴 Kritiske bugs (game-breaking)

*Ingen kendte kritiske bugs i Beta 9.*

---

## 🟠 Alvorlige bugs (forkert opførsel)

*Ingen kendte alvorlige bugs i Beta 9.*

---

## 🟡 Medium bugs (noget er forkert men ikke game-breaking)

*Ingen kendte medium bugs i Beta 9.*

---

## 🔵 Kendte begrænsninger (by design eller lav prioritet)

### Multiplayer (flere companies) ikke understøttet
**Status:** Ikke planlagt
**Beskrivelse:** Kun single-company gameplay er understøttet.

### Windows-only TLS/WSS
**Status:** Lav prioritet
**Beskrivelse:** WSS bruger Windows Schannel. Linux/macOS kan kun forbinde via plain WS.

### `£`-tegnet i item-navne er platform-afhængigt
**Status:** Lav prioritet
**Beskrivelse:** `UTILITY_ITEMS` bruger `£` direkte (UTF-8). Risiko for mismatch på ikke-UTF-8 Windows-lokaler.

---

## ✅ Løste bugs (arkiv)

| Version | Bug | Fix |
|---------|-----|-----|
| Beta 9 | **Save/load crash "Field type is different than expected"** — `station_spread` og `max_train_length` ændret fra SLE_UINT8 til SLE_UINT16 i settings; PATS-chunk type-mismatch ved load af vanilla/Beta 8 saves | Begge felter reverteret til SLE_UINT8; max=255; manager clammer til uint8_t |
| Beta 9 | **Save/load crash "Invalid array length"** — `APSTChunkHandler::Load()` manglede andet `SlIterateArray()` kald til at reset `_next_offs` | Ekstra `SlIterateArray()` kald tilføjet efter `SlGlobList()` |
| Beta 9 | **AP-ikon brød remove-tracks-knappen** — sprite 714 = SPR_IMG_REMOVE bruges aktivt i rail/road GUI | Sprite 714 ikke længere brugt; ikon loades til slot 4883 via `LoadGrfFileIndexed` |
| Beta 9 | **Shop `OnRealtimeTick` stoppede ikke polling** | `hints_all_loaded`-flag tilføjet |
| Beta 9 | **Armored Trucks spawner altid ved custom_vehicle_count=1** | `locked_vehicles` indeholder nu alle klimaeligible køretøjer |
| Beta 9 | **Toyland-køretøjer på Temperate / non-Toyland på Toyland** | `_NON_TOYLAND_ENGINES` frozenset; klimafiltrering rettet |
| Beta 9 | **Server-adresse og slot-navn huskes ikke** | Gemmes/indlæses fra `ap_connection.cfg` |
| Beta 9 | **Mission-nummer ikke synligt** | `#042` prefix i mission-beskrivelse |
| Beta 9 | **Console location checks manglede** | `ap_check` og `ap_status` console commands |
| Beta 9 | **Named missioner viste hvide** — TC_WHITE override | Difficulty-farve bevaret |
| Beta 9 | **`mission_count=300` gav 250 missioner** — scale-faktor mod base 350 | Direkte antal |
| Beta 9 | **`maintain`-type brugte skrøbelig string-søgning** | `maintain_75`/`maintain_90` normaliseret |
| Beta 9 | **Shop label-fallback viste rå `Shop_Purchase_NNNN`** | Fallback: `"Shop Slot #N"` |
| Beta 9 | **Sugar/Toys/Batteries manglede fra Toyland cargo-pool** | Tilføjet til `CARGO_BY_LANDSCAPE[3]` |
| Beta 8 | **Iron Horse engines låstes ikke** — IH-prefix mismatch i `locked_vehicles` | Begge lock-steder tjekker nu `"IH: " + eng_name` |
| Beta 8 | **Server crash "No location for player 1"** — `shop_slots*20` vs direkte antal | Læser `shop_item_count` direkte |
| Beta 8 | **Startende tog forkert klima** | `ARCTIC_TROPIC_ONLY_TRAINS` + `TEMPERATE_ONLY_TRAINS` frozensets |
| Beta 8 | **Mission/shop-vinduer kun lodrette resize** | `resize.step_width = 1` |
| Beta 8 | **10 Toyland-cargos manglede i `BuildCargoMap()`** | Alle 10 labels tilføjet til `kNameLabel[]` |
| Beta 8 | **`cargo_to/from_industry` tæller kun slot 0** | Itererer alle `ind->accepted`/`ind->produced`-slots |
| Beta 8 | **`cargo_type` sentinel brugte slot[0]** | Bruger første gyldige slot |
| Beta 8 | **Guru Galaxy fejlagtigt ekskluderet** + 10 Toyland-only manglede | TOYLAND_ONLY_VEHICLES rettet |
| Beta 7 | **DeathLink virker aldrig** — `death_link` manglede i `fill_slot_data()` | Tilføjet til `fill_slot_data()` |
| Beta 7 | **Timed effects løber af i pause** | Wrappet i `if (_game_mode == GM_NORMAL)` |
| Beta 7 | **Shop-rotation vises ikke straks** | `OnInvalidateData()` tilføjet |
| Beta 7 | **C++ location IDs fra beregnet distribution** | Bygges fra `sd.missions` direkte |
| Beta 7 | **Loan Reduction spildt ved lån=0** | Konverteres til kontanter |
| Beta 7 | **Shop spammede samme utility item** | Utility-pool shuffles i batches |
| Beta 7 | **`location_name_to_id` for lille** | class-level dict: `mission_count=1140` |
| Beta 7 | **Timed effects nulstilles ved save/load** | Gemmes via `AP_GetEffectTimers` |
| Beta 7 | **Recession trap gav bonus ved gæld** | Gæld-path: lån-straf i stedet |
| Beta 7 | **Town Growth Boost muterede `growth_rate` permanent** | Nulstiller `grow_counter` |
| Beta 6 | Mission tekst knust ved UI scale ≥2 | Row height genberegnes ved draw-time |
| Beta 6 | Valuta viser £ på ikke-GBP spil | Currency-prefix ved render-tid |
| Beta 6 | `random` starting vehicle gav cargo wagon | Multi-niveau fallback |
| Beta 6 | Shop viste færre items end `shop_slots` | Læser YAML-option direkte |
| Beta 6 | AP status-vindue kunne ikke trækkes | `WDP_AUTO` med persistens-key |
| Beta 6 | "Unknown item: not handled" for vanilla engines | `BuildEngineMap()` efter expire-flag |
| Beta 6 | Oil Tanker kun én wagon-variant | `_ap_engine_extras`-map |
| Beta 6 | IH engine prefix mismatch | `IH: ` strippes før opslag |
| Beta 5 | Toyland missions på forkert map | Climate-filtreret cargo-liste |
| Beta 5 | Toyland køretøjer på forkert map | Ekskluderes fra pool |
| Beta 5 | "Service X towns" umulig på små maps | Cap baseret på map-dimensioner |
| Beta 5 | Bank Loan trap 500M hardkodet | Skaleres til `max_loan` |
| Beta 5 | Shop afviste køb / forkert pris | Købte slots filtreres straks |
| Beta 4 | Wine + WSS crash | WSS springes over under Wine |
| Beta 4 | "Missing 140 sprites" advarsel | Warning-widget skjules |
| Beta 4 | Starting vehicle låst forkert klima | Toyland-only ekskluderes |
| Beta 3 | "Maintain X% rating" tracker forkert | Korrekte måneder |
| Beta 3 | DeathLink notification usynlig | Fuld avisopslag |
| Beta 3 | AP-state tabt ved save/load | APST savegame-chunk |
| Beta 2 | WSS/WS auto-detection | Prober WSS, falder til WS |
| Beta 2 | Build fejlede uden zlib | `#ifdef WITH_ZLIB` guard |
