# Known Bugs & Limitations — OpenTTD Archipelago

Last updated: v1.0.0 (2026-03-09)

---

## 🔴 Critical bugs (game-breaking)

*None known in v1.0.0.*

---

## 🟠 Serious bugs (incorrect behaviour)

*None known in v1.0.0.*

---

## 🟡 Medium bugs (something is wrong but not game-breaking)

*None known in v1.0.0.*

---

## 🔵 Known limitations (by design or low priority)

### Multiplayer (multiple companies) not supported
**Status:** Not planned
**Description:** Only single-company gameplay is supported. Co-op and competition mode are not supported.

### Windows-only TLS/WSS
**Status:** Low priority
**Description:** WSS uses Windows Schannel. Linux/macOS clients can only connect via plain WS (unencrypted).

### `£` character in item names is platform-dependent
**Status:** Low priority
**Description:** Utility items use `£` directly (UTF-8). Risk of name mismatch on non-UTF-8 Windows locales.

### WebSocket compression not supported
**Status:** Low priority
**Description:** Compressed WebSocket connections (`permessage-deflate`) are not supported. The Archipelago server logs a warning but the connection works normally.
