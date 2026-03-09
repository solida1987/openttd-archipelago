# Installation Guide — OpenTTD Archipelago

## Requirements

- Windows 10 or 11 (64-bit)
- [Archipelago](https://archipelago.gg/downloads) installed (any recent version)
- An Archipelago server to connect to (hosted locally or via archipelago.gg)

---

## Step 1 — Install the APWorld

1. Download `openttd.apworld` from the [latest release](../../releases/latest)
2. Place it in your Archipelago `custom_worlds/` folder:
   - Default path: `C:\ProgramData\Archipelago\custom_worlds\`
   - If the folder doesn't exist, create it manually

The APWorld will be auto-detected next time you open the Archipelago Launcher.

---

## Step 2 — Prepare your YAML

Create a YAML file for your player slot. Minimum required:

```yaml
name: YourName
game: OpenTTD

OpenTTD:
  win_condition: company_value
  win_condition_company_value: 50000000
```

See [docs/yaml_options.md](docs/yaml_options.md) for all options.

---

## Step 3 — Generate and host the multiworld

1. Open Archipelago Launcher → **Generate**
2. Select your YAML (and other players' YAMLs if doing a multiworld)
3. Host the resulting `.zip` on archipelago.gg, or run the server locally

---

## Step 4 — Install the game client

1. Download `openttd-archipelago-v1.0.0-win64.zip` from the [latest release](../../releases/latest)
2. Extract to any folder — the game is fully portable, no installation needed
3. Run `openttd.exe`

---

## Step 5 — Connect

1. In the OpenTTD main menu, click **Archipelago**
2. Fill in:
   - **Server**: `archipelago.gg:PORT` or `localhost:PORT`
   - **Slot name**: your player name (must match the YAML)
   - **Password**: server password (if any)
3. Click **Connect** — the game will start with your randomizer settings applied

---

## Troubleshooting

**"Cannot connect to server"**
- Check that the Archipelago server is running and the port is correct
- The game uses WebSocket (port typically 38281). Make sure your firewall allows it.

**Vehicles are all locked**
- That's normal! You start with only your `starting_vehicle_type`. Unlock the rest by completing missions and receiving items from the multiworld.

**Missing baseset / game won't start**
- Make sure you extracted the full ZIP. The `baseset/` folder must be present next to `openttd.exe`.

**APWorld not showing in generator**
- Verify `openttd.apworld` is in `C:\ProgramData\Archipelago\custom_worlds\` and restart the Archipelago Launcher.

**AP Commands in-game**
- Open the console with the `` ` `` (backtick) key and type `ap !hint <item>` to send commands directly to the Archipelago server.
- Or click the **Guide** button on the AP status window for a full command reference.
