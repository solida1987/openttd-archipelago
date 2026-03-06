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

### Base graphics warning on first launch
If built without PNG/zlib via vcpkg the game may show "missing 140 sprites" on the
intro screen. This is a cosmetic issue with the baseset and does not affect gameplay.
Fix: build with vcpkg toolchain so PNG and zlib are included.
**Severity:** Low — cosmetic only.
