# Installing OpenTTD Archipelago

This game is a **plugin** for the Multiworld Launcher. The launcher is a
separate download, and it ships with no games in it — every game arrives as a
file you fetch and add yourself.

## What you need

| | |
|---|---|
| **Multiworld Launcher 3.0.1 or newer** | The host program. → [Download it here](https://github.com/solida1987/Multiworld-Launcher/releases/latest) |
| **This plugin** | The `.londonplugin` file from [this project's releases](https://github.com/solida1987/openttd-archipelago/releases/latest) |
| **Windows 10 or 11** | No separate runtime needed. |

OpenTTD is free software (GPL-2.0), so there is no game to own first — the
launcher downloads the whole game for you in step 4.

## Step 1 — Get the launcher

Download `launcher_package.zip` from the
[latest launcher release](https://github.com/solida1987/Multiworld-Launcher/releases/latest),
extract it somewhere you have write access, and run `Multiworld Launcher.exe`.

**The library will be empty.** That is correct.

## Step 2 — Get this plugin

Download **`openttd_archipelago-*.londonplugin`** from
[this project's latest release](https://github.com/solida1987/openttd-archipelago/releases/latest).

## Step 3 — Add it to the launcher

1. Click **Add plugin…** in the launcher.
2. Pick the `.londonplugin` file.
3. **Read the dialog** — who published it, what it declares it will do, and
   the SHA-256 of the file — and approve it.

OpenTTD appears in the library on the left immediately.

## Step 4 — Install the game

Click OpenTTD in the library, then **Install**. The launcher downloads the
game package from this repository's releases and keeps it updated.

## Step 5 — Play

- **Play** joins an Archipelago multiworld. There is no in-game login: the
  launcher owns the session, and the game's *Join AP* connects straight away.
- **Launch Standalone** plays a solo randomized seed with no server. Pick a
  seed under Settings, or let it pick one for you — progress is kept per seed.
- **Create YAML** on the game's page builds your multiworld settings file
  from the apworld's own option list.

If a multiworld seed was generated against extra NewGRF vehicle sets (Iron
Horse, FIRS, …), install those yourself from OpenTTD's own content service
and enable them in the game's NewGRF settings. The launcher tells you exactly
what is missing before play starts.

## If something goes wrong

The plugin writes `ap_launcher.log` next to the game — what the game and the
launcher said to each other. That file answers most questions; attach it if
you report a problem.

---

## The old standalone version

The pre-launcher build with its own in-game server login still exists as the
release marked **`legacy`**. It is discontinued and will not be updated; see
the README's Legacy section for what that means.
