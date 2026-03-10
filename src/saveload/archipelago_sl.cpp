/*
 * OpenTTD Archipelago — Savegame chunk (APST)
 *
 * Serialisation strategy: ALL state is packed into a single std::string
 * field ("apstate") as a key=value plain-text format.  This makes the
 * chunk immune to type-mismatch errors across beta versions.
 *
 * KVGet/KVSet/Parse* use only std::string and std::from_chars — no banned
 * functions.  IStr/PackCargo/UnpackCargo use fmt::format and from_chars,
 * placed AFTER safeguards.h where fmt is available.
 */

/* ── Standard headers only — no banned functions here ──────────────── */
#include <string>
#include <vector>
#include <charconv>
#include <algorithm>

/* Minimal key=value serialiser — values may not contain '|' or '=' */
static std::string KVGet(const std::string &blob, const std::string &key, const std::string &def = "")
{
    std::string search = key + "=";
    auto pos = blob.find(search);
    if (pos == std::string::npos) return def;
    pos += search.size();
    auto end = blob.find('|', pos);
    return (end == std::string::npos) ? blob.substr(pos) : blob.substr(pos, end - pos);
}

static void KVSet(std::string &blob, const std::string &key, const std::string &val)
{
    if (!blob.empty()) blob += '|';
    blob += key + '=' + val;
}

/* string -> int/int64/uint16 helpers using std::from_chars (not banned) */
static int ParseInt(const std::string &s, int def = 0)
{
    int r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}
static uint16_t ParseU16(const std::string &s, uint16_t def = 0)
{
    uint16_t r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}
static int64_t ParseI64(const std::string &s, int64_t def = 0)
{
    int64_t r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}

/* ==================================================================== */
/* OpenTTD headers — safeguards.h bans snprintf/sscanf/to_string etc.  */
/* ALL remaining helpers use fmt::format (allowed) and from_chars only. */
/* ==================================================================== */
#include "../stdafx.h"
#include "saveload.h"
#include "../safeguards.h"
#include "../core/format.hpp"

/* int -> string helpers using fmt (no snprintf allowed past safeguards) */
static std::string IStr(int v)      { return fmt::format("{}", v); }
static std::string IStr(uint16_t v) { return fmt::format("{}", v); }
static std::string IStr(int64_t v)  { return fmt::format("{}", v); }
static std::string IStr(bool v)     { return v ? "1" : "0"; }

/* Pack uint64 cargo array as "idx:val,..." (sparse, skip zeroes) */
static std::string PackCargo_AP(const uint64_t *arr, int n)
{
    std::string out;
    for (int i = 0; i < n; i++) {
        if (arr[i] == 0) continue;
        if (!out.empty()) out += ',';
        out += fmt::format("{}:{}", i, arr[i]);
    }
    return out;
}

/* Unpack "idx:val,..." back into cargo array */
static void UnpackCargo_AP(const std::string &s, uint64_t *arr, int n)
{
    std::fill(arr, arr + n, static_cast<uint64_t>(0));
    if (s.empty()) return;
    const char *p   = s.data();
    const char *end = p + s.size();
    while (p < end) {
        int idx = 0;
        auto [p2, ec1] = std::from_chars(p, end, idx);
        if (ec1 != std::errc() || p2 >= end || *p2 != ':') break;
        p = p2 + 1;
        uint64_t val = 0;
        auto [p3, ec2] = std::from_chars(p, end, val);
        if (ec2 == std::errc() && idx >= 0 && idx < n) arr[idx] = val;
        p = p3;
        if (p < end && *p == ',') p++;
    }
}

/* ── External state from archipelago_manager.cpp ───────────────────── */
extern std::string  _ap_last_host;
extern uint16_t     _ap_last_port;
extern std::string  _ap_last_slot;
extern std::string  _ap_last_pass;

std::string  AP_GetCompletedMissionsStr();
void         AP_SetCompletedMissionsStr(const std::string &s);
int          AP_GetShopPageOffset();
void         AP_SetShopPageOffset(int v);
int          AP_GetShopDayCounter();
void         AP_SetShopDayCounter(int v);
bool         AP_GetGoalSent();
void         AP_SetGoalSent(bool v);
int          AP_GetFfSpeed();
void         AP_SetFfSpeed(int v);
void         AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out);
void         AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in);
std::string  AP_GetMaintainCountersStr();
void         AP_SetMaintainCountersStr(const std::string &s);
void         AP_GetEffectTimers(int *fuel, int *cargo, int *reliability, int *station, int *license_ticks, int *license_type);
void         AP_SetEffectTimers(int fuel, int cargo, int reliability, int station, int license_ticks, int license_type);
std::string  AP_GetNamedEntityStr();
void         AP_SetNamedEntityStr(const std::string &s);
std::string  AP_GetSentShopStr();
void         AP_SetSentShopStr(const std::string &s);
void         AP_GetColbyState(int *step, int64_t *delivered, int *target_town, bool *escaped, int *escape_ticks, bool *done, bool *popup_shown);
void         AP_SetColbyState(int step, int64_t delivered, int target_town, bool escaped, int escape_ticks, bool done, bool popup_shown);
std::string  AP_GetTasksStr();
void         AP_SetTasksStr(const std::string &s);
int          AP_GetTaskChecksCompletedSaved();
void         AP_SetTaskChecksCompleted(int v);

/* ── Scratch variable — single string holds all AP state ────────────── */
static std::string _ap_sl_blob;

/* ── SaveLoad table: one field only ─────────────────────────────────── */
static const SaveLoad _ap_desc[] = {
    SLEG_SSTR("apstate", _ap_sl_blob, SLE_STR),
};

struct APSTChunkHandler : ChunkHandler {
    APSTChunkHandler() : ChunkHandler('APST', CH_TABLE) {}

    void Save() const override
    {
        _ap_sl_blob.clear();

        KVSet(_ap_sl_blob, "host",  _ap_last_host);
        KVSet(_ap_sl_blob, "port",  IStr(_ap_last_port));
        KVSet(_ap_sl_blob, "slot",  _ap_last_slot);
        KVSet(_ap_sl_blob, "pass",  _ap_last_pass);

        KVSet(_ap_sl_blob, "completed",   AP_GetCompletedMissionsStr());
        KVSet(_ap_sl_blob, "shop_offset", IStr(AP_GetShopPageOffset()));
        KVSet(_ap_sl_blob, "shop_days",   IStr(AP_GetShopDayCounter()));
        KVSet(_ap_sl_blob, "shop_sent",   AP_GetSentShopStr());
        KVSet(_ap_sl_blob, "goal_sent",   IStr(AP_GetGoalSent()));
        KVSet(_ap_sl_blob, "ff_speed",    IStr(AP_GetFfSpeed()));

        constexpr int NC = 64;
        uint64_t cargo[NC] = {};
        int64_t  profit    = 0;
        AP_GetCumulStats(cargo, NC, &profit);
        KVSet(_ap_sl_blob, "profit",   IStr(profit));
        KVSet(_ap_sl_blob, "cargo",    PackCargo_AP(cargo, NC));
        KVSet(_ap_sl_blob, "maintain", AP_GetMaintainCountersStr());
        KVSet(_ap_sl_blob, "named",    AP_GetNamedEntityStr());

        int fuel = 0, carg = 0, rel = 0, sta = 0, lic_t = 0, lic_v = -1;
        AP_GetEffectTimers(&fuel, &carg, &rel, &sta, &lic_t, &lic_v);
        KVSet(_ap_sl_blob, "fuel_ticks",  IStr(fuel));
        KVSet(_ap_sl_blob, "cargo_ticks", IStr(carg));
        KVSet(_ap_sl_blob, "rel_ticks",   IStr(rel));
        KVSet(_ap_sl_blob, "sta_ticks",   IStr(sta));
        KVSet(_ap_sl_blob, "lic_ticks",   IStr(lic_t));
        KVSet(_ap_sl_blob, "lic_type",    IStr(lic_v));

        /* Colby Event */
        int   co_step = 0; int64_t co_del = 0; int co_town = (int)UINT16_MAX;
        bool  co_esc = false; int co_eticks = 0; bool co_done = false; bool co_pop = false;
        AP_GetColbyState(&co_step, &co_del, &co_town, &co_esc, &co_eticks, &co_done, &co_pop);
        KVSet(_ap_sl_blob, "co_step",   IStr(co_step));
        KVSet(_ap_sl_blob, "co_del",    IStr(co_del));
        KVSet(_ap_sl_blob, "co_town",   IStr(co_town));
        KVSet(_ap_sl_blob, "co_esc",    IStr(co_esc));
        KVSet(_ap_sl_blob, "co_eticks", IStr(co_eticks));
        KVSet(_ap_sl_blob, "co_done",   IStr(co_done));
        KVSet(_ap_sl_blob, "co_pop",    IStr(co_pop));

        /* Local tasks */
        KVSet(_ap_sl_blob, "tasks",       AP_GetTasksStr());
        KVSet(_ap_sl_blob, "task_checks", IStr(AP_GetTaskChecksCompletedSaved()));

        SlTableHeader(_ap_desc);
        SlSetArrayIndex(0);
        SlGlobList(_ap_desc);
    }

    void Load() const override
    {
        _ap_sl_blob.clear();

        const std::vector<SaveLoad> slt = SlCompatTableHeader(_ap_desc, {});
        if (SlIterateArray() == -1) return;
        SlGlobList(slt);
        /* Consume end-of-array marker to reset _next_offs to 0.
         * Without this, SlLoadChunk() sees _next_offs != 0 and throws
         * "Invalid array length". See animated_tile_sl.cpp for reference. */
        if (SlIterateArray() != -1) SlErrorCorrupt("Too many APST entries");

        if (_ap_sl_blob.empty()) return;

        try {
            _ap_last_host = KVGet(_ap_sl_blob, "host");
            _ap_last_port = ParseU16(KVGet(_ap_sl_blob, "port", "38281"), 38281);
            _ap_last_slot = KVGet(_ap_sl_blob, "slot");
            _ap_last_pass = KVGet(_ap_sl_blob, "pass");

            AP_SetCompletedMissionsStr(KVGet(_ap_sl_blob, "completed"));

            auto getint = [&](const std::string &key) -> int {
                return ParseInt(KVGet(_ap_sl_blob, key, "0"));
            };
            auto getint64 = [&](const std::string &key) -> int64_t {
                return ParseI64(KVGet(_ap_sl_blob, key, "0"));
            };

            AP_SetShopPageOffset(getint("shop_offset"));
            AP_SetShopDayCounter(getint("shop_days"));
            AP_SetSentShopStr(KVGet(_ap_sl_blob, "shop_sent"));
            AP_SetGoalSent(KVGet(_ap_sl_blob, "goal_sent", "0") == "1");
            AP_SetFfSpeed(getint("ff_speed"));  /* 0 = not saved yet → AP_SetFfSpeed clamps to 100 */

            constexpr int NC = 64;
            uint64_t cargo[NC] = {};
            UnpackCargo_AP(KVGet(_ap_sl_blob, "cargo"), cargo, NC);
            AP_SetCumulStats(cargo, NC, getint64("profit"));

            AP_SetMaintainCountersStr(KVGet(_ap_sl_blob, "maintain"));
            AP_SetNamedEntityStr(KVGet(_ap_sl_blob, "named"));

            AP_SetEffectTimers(getint("fuel_ticks"), getint("cargo_ticks"),
                               getint("rel_ticks"),  getint("sta_ticks"),
                               getint("lic_ticks"),  getint("lic_type"));

            /* Colby Event */
            AP_SetColbyState(
                getint("co_step"),
                ParseI64(KVGet(_ap_sl_blob, "co_del", "0")),
                getint("co_town"),
                KVGet(_ap_sl_blob, "co_esc",  "0") == "1",
                getint("co_eticks"),
                KVGet(_ap_sl_blob, "co_done", "0") == "1",
                KVGet(_ap_sl_blob, "co_pop",  "0") == "1"
            );

            /* Local tasks */
            AP_SetTasksStr(KVGet(_ap_sl_blob, "tasks"));
            AP_SetTaskChecksCompleted(getint("task_checks"));
        } catch (...) {
            /* Parsing failed — AP progress lost but game loads. */
        }
    }
};

static const APSTChunkHandler APST;
static const ChunkHandlerRef ap_chunk_handlers[] = { APST };
extern const ChunkHandlerTable _ap_chunk_handlers(ap_chunk_handlers);
