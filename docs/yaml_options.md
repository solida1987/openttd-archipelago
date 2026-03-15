# OpenTTD Archipelago — YAML Options Reference

All 56 configurable options for the OpenTTD Archipelago world.
Add any of these under an `OpenTTD:` section in your Archipelago YAML file.

**Quick start example:**
```yaml
name: YourName
game: OpenTTD

OpenTTD:
  win_condition: company_value
  win_condition_company_value: 50000000
  starting_vehicle_type: trains
  landscape: temperate
  mission_count: 100
  enable_traps: true
  death_link: false
  map_size_x: 256
  map_size_y: 256
  start_year: 1950
  max_loan: 300000
```

---

## Randomizer

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `mission_count` | Number | `100` | `50` – `300` | How many missions to generate as location checks. Acts as a relative scaler — 300 is the baseline (×1.0). Higher values create more checks but scale the total pool for multiplayer. |
| `starting_vehicle_type` | Choice | `random` | `random` `trains` `road_vehicle` `aircraft` `ships` `one_of_each` | Which transport type you start with. `one_of_each` gives you one safe starter per type (bus, train, small plane, ferry) immediately. |
| `win_condition` | Choice | `company_value` | `company_value` `monthly_profit` `vehicle_count` `town_population` `cargo_delivered` | What you need to achieve to send the Victory item. |
| `win_condition_company_value` | Number | `50000000` | `1,000,000` – `10,000,000,000` | Target company value in £ (only used when `win_condition: company_value`). |
| `win_condition_monthly_profit` | Number | `1000000` | `100,000` – `100,000,000` | Monthly profit target in £ (only used when `win_condition: monthly_profit`). |
| `win_condition_vehicle_count` | Number | `50` | `10` – `500` | Number of vehicles running simultaneously (only used when `win_condition: vehicle_count`). |
| `win_condition_town_population` | Number | `20000` | `10,000` – `500,000` | Target total world population across all towns combined (only used when `win_condition: town_population`). |
| `win_condition_cargo_delivered` | Number | `1000000` | `100,000` – `100,000,000` | Total tons of cargo to deliver (only used when `win_condition: cargo_delivered`). |

---

## Shop

The shop lets you spend in-game money to buy location checks directly.

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `shop_slots` | Number | `5` | `3` – `10` | How many items are visible in the shop at a time. |
| `shop_refresh_days` | Number | `90` | `30` – `365` | In-game days between shop rotations. After each rotation the displayed items change. |
| `shop_price_tier` | Choice | `normal` | `easy` `normal` `hard` `extreme` | Price range for shop purchases. Easy: £10K–£500K. Normal: £100K–£5M. Hard: £1M–£50M. Extreme: £10M–£300M. |

---

## Traps

Trap items can be sent to you by other players in the multiworld. Each trap causes a temporary negative effect.

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `enable_traps` | Toggle | `true` | `true` `false` | Master switch. Set to `false` to completely remove all traps from the item pool. |
| `trap_intensity` | Number | `30` | `0` – `100` | What percentage of the item pool consists of traps. 0 = very rare (~2%). 100 = ~25% of all items are traps. Has no effect if `enable_traps` is off. |
| `trap_breakdown_wave` | Toggle | `true` | `true` `false` | Enable **Breakdown Wave** — all your vehicles break down simultaneously. |
| `trap_recession` | Toggle | `false` | `true` `false` | Enable **Recession** — your company's money is halved instantly. Off by default (very punishing). |
| `trap_maintenance_surge` | Toggle | `true` | `true` `false` | Enable **Maintenance Surge** — a forced loan of 25% of your max loan is added. |
| `trap_signal_failure` | Toggle | `true` | `true` `false` | Enable **Signal Failure** — trains are disrupted network-wide for a short period. |
| `trap_fuel_shortage` | Toggle | `true` | `true` `false` | Enable **Fuel Shortage** — all vehicles run at half speed for 60 seconds. |
| `trap_bank_loan` | Toggle | `false` | `true` `false` | Enable **Bank Loan Forced** — you are forced to take out your maximum available loan. Off by default (very punishing early game). |
| `trap_industry_closure` | Toggle | `false` | `true` `false` | Enable **Industry Closure** — one of your served industries closes permanently. Off by default. |

---

## World Generation

These settings control how the OpenTTD map is generated at game start.

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `start_year` | Number | `1950` | `1` – `5,000,000` | Starting calendar year. Does not restrict vehicle unlocks — you can use any unlocked vehicle regardless of year. |
| `map_size_x` | Choice | `256` | `64` `128` `256` `512` `1024` `2048` | Map width in tiles. |
| `map_size_y` | Choice | `256` | `64` `128` `256` `512` `1024` `2048` | Map height in tiles. |
| `landscape` | Choice | `temperate` | `temperate` `arctic` `tropical` `toyland` | Climate type. Affects which cargoes, industries and vehicles appear. Toyland uses a completely different vehicle set. |
| `land_generator` | Choice | `terragenesis` | `original` `terragenesis` | Terrain generation algorithm. Terragenesis produces more natural-looking terrain. |
| `industry_density` | Choice | `normal` | `fund_only` `minimal` `very_low` `low` `normal` `high` | Number of industries at game start. `fund_only` means no industries are placed automatically — you must fund them yourself. |

---

## Economy & Finance

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `max_loan` | Number | `300000` | `100,000` – `500,000,000` | Maximum initial bank loan available in £. Also affects how Bank Loan traps scale. |
| `infinite_money` | Toggle | `false` | `true` `false` | Allow spending money even with a negative balance. Effectively removes financial pressure. |
| `inflation` | Toggle | `false` | `true` `false` | Enable cost/price inflation over time. |
| `infrastructure_maintenance` | Toggle | `false` | `true` `false` | Charge monthly fees for owned infrastructure (tracks, roads, signals, etc.). |
| `vehicle_costs` | Choice | `medium` | `low` `medium` `high` | Multiplier on vehicle running costs. |
| `construction_cost` | Choice | `medium` | `low` `medium` `high` | Multiplier on construction costs (tracks, stations, depots, etc.). |
| `economy_type` | Choice | `smooth` | `original` `smooth` `frozen` | Economy volatility. `original` = boom/bust cycles. `smooth` = gentle fluctuation. `frozen` = no change. |
| `bribe` | Toggle | `true` | `true` `false` | Allow bribing the local authority to improve town rating. |
| `exclusive_rights` | Toggle | `true` | `true` `false` | Allow purchasing exclusive transport rights in a town. |
| `fund_buildings` | Toggle | `true` | `true` `false` | Allow funding new building construction in towns. |
| `fund_roads` | Toggle | `true` | `true` `false` | Allow funding local road reconstruction. |
| `give_money` | Toggle | `true` | `true` `false` | Allow transferring money to other companies. |
| `town_cargo_scale` | Number | `100` | `15` – `500` | Cargo production multiplier for towns (percent). 200 = double the normal passenger/mail output. |
| `industry_cargo_scale` | Number | `100` | `15` – `500` | Cargo production multiplier for industries (percent). |

---

## Vehicles & Infrastructure

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `max_trains` | Number | `500` | `0` – `65535` | Maximum trains per company. 0 disables trains entirely. |
| `max_roadveh` | Number | `500` | `0` – `65535` | Maximum road vehicles per company. |
| `max_aircraft` | Number | `200` | `0` – `65535` | Maximum aircraft per company. |
| `max_ships` | Number | `300` | `0` – `65535` | Maximum ships per company. |
| `max_train_length` | Number | `7` | `1` – `1000` | Maximum train length in tiles. The vanilla limit is 64; this can exceed it. |
| `station_spread` | Number | `12` | `4` – `1024` | Maximum distance in tiles between station parts that can still belong to the same station. Set to `1024` for virtually unlimited spread. |
| `road_stop_on_town_road` | Toggle | `true` | `true` `false` | Allow building drive-through bus/lorry stops on roads owned by towns. |
| `road_stop_on_competitor_road` | Toggle | `true` | `true` `false` | Allow building drive-through stops on roads owned by competitors. |
| `crossing_with_competitor` | Toggle | `true` | `true` `false` | Allow level crossings with rails or roads owned by competitors. |
| `road_side` | Choice | `right` | `left` `right` | Which side of the road vehicles drive on. |

---

## Towns & Environment

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `town_growth_rate` | Choice | `normal` | `none` `slow` `normal` `fast` `very_fast` | How quickly towns grow over time. Larger towns produce more cargo. |
| `found_town` | Choice | `forbidden` | `forbidden` `allowed` `custom_layout` | Whether players can found new towns during the game. |
| `allow_town_roads` | Toggle | `true` | `true` `false` | Allow towns to automatically build and expand their own road network. |

---

## Disasters & Accidents

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `disasters` | Toggle | `false` | `true` `false` | Enable random disasters such as floods, UFO attacks and industry explosions. |
| `plane_crashes` | Choice | `normal` | `none` `reduced` `normal` | How often planes crash. `none` disables aircraft accidents entirely (recommended for DeathLink games). |
| `vehicle_breakdowns` | Choice | `reduced` | `none` `reduced` `normal` | How often vehicles break down. `none` removes breakdowns. Default is `reduced` to avoid excessive micromanagement. |

---

## NewGRFs

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `enable_iron_horse` | Toggle | `false` | `true` `false` | Add Iron Horse locomotives to the item pool. Iron Horse is a British-inspired train set with ~164 engines spanning multiple eras. The GRF is bundled with the client — no manual installation needed. Works on Temperate, Arctic and Tropical maps only (not Toyland). |

---

## Death Link

| Option | Type | Default | Valid values | Description |
|--------|------|---------|--------------|-------------|
| `death_link` | Toggle | `false` | `true` `false` | When enabled: if you die, everyone in the multiworld dies. If anyone else dies, you die too. Deaths are triggered by vehicle crashes (train collisions, aircraft crashes, road vehicles struck by trains). Off by default. |

---

## Tips

**Recommended beginner settings:**
```yaml
OpenTTD:
  win_condition: company_value
  win_condition_company_value: 10000000
  starting_vehicle_type: one_of_each
  landscape: temperate
  enable_traps: true
  trap_intensity: 20
  trap_bank_loan: false
  trap_recession: false
  trap_industry_closure: false
  vehicle_breakdowns: none
  shop_price_tier: easy
  death_link: false
```

**Harder / longer run:**
```yaml
OpenTTD:
  win_condition: cargo_delivered
  win_condition_cargo_delivered: 5000000
  starting_vehicle_type: random
  enable_traps: true
  trap_intensity: 60
  trap_bank_loan: true
  trap_recession: true
  shop_price_tier: hard
  infrastructure_maintenance: true
  vehicle_breakdowns: normal
  death_link: true
```
