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

### Wine incompatibility (Beta 3 — fixed in Beta 4)
Beta 3 introduced WSS/WS auto-detection using the Windows Schannel TLS API
(`Secur32.dll`). Wine's Schannel implementation does not support the
`UNISP_NAME_A` provider used by the WSS probe, causing the connection worker
thread to fail before a plain WS fallback can be attempted.

**Beta 3 workaround:** Use Beta 2, which has no TLS code and connects via
plain WS only.

**Beta 4 fix:** Wine is detected at runtime via its `HKLM\Software\Wine`
registry key. When running under Wine the WSS probe is skipped entirely and
the client connects via plain WS directly, matching Beta 2 behaviour.

### Base graphics warning on first launch (fixed in Beta 4)
If built without PNG/zlib via vcpkg the game may show "missing 140 sprites" on the
intro screen. This is a cosmetic issue with the baseset and does not affect gameplay.
Fix: build with vcpkg toolchain so PNG and zlib are included.
**Severity:** Low — cosmetic only.
**Beta 4 fix:** The baseset warning widget is now unconditionally hidden in the
Archipelago build. The bundled baseset is complete; the warning was a false positive
caused by the build not being tagged as a vanilla "released version".
