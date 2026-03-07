/*
 * OpenTTD Archipelago — Savegame chunk (APST)
 * String helpers are defined BEFORE OpenTTD headers to avoid safeguards.h bans.
 */

/* ── String helpers — before any OpenTTD headers ───────────────────── */
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

static std::string PackCargo_AP(const uint64_t *arr, int n)
{
    std::string out;
    char buf[64];
    for (int i = 0; i < n; i++) {
        if (arr[i] == 0) continue;
        if (!out.empty()) out += ',';
        snprintf(buf, sizeof(buf), "%d:%llu", i, (unsigned long long)arr[i]);
        out += buf;
    }
    return out;
}

static void UnpackCargo_AP(const std::string &s, uint64_t *arr, int n)
{
    std::fill(arr, arr + n, (uint64_t)0);
    if (s.empty()) return;
    const char *p = s.c_str();
    while (*p) {
        int idx = 0;
        unsigned long long val = 0;
        if (sscanf(p, "%d:%llu", &idx, &val) == 2 && idx >= 0 && idx < n)
            arr[idx] = (uint64_t)val;
        const char *comma = strchr(p, ',');
        if (!comma) break;
        p = comma + 1;
    }
}

/* ── OpenTTD headers ────────────────────────────────────────────────── */
#include "../stdafx.h"
#include "saveload.h"
#include "../safeguards.h"

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
void         AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out);
void         AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in);
std::string  AP_GetMaintainCountersStr();
void         AP_SetMaintainCountersStr(const std::string &s);
void         AP_GetEffectTimers(int *fuel, int *cargo, int *reliability, int *station);
void         AP_SetEffectTimers(int fuel, int cargo, int reliability, int station);
std::string  AP_GetNamedEntityStr();
void         AP_SetNamedEntityStr(const std::string &s);

/* ── Scratch variables for Save/Load ────────────────────────────────── */
static std::string _ap_sl_host;
static uint16_t    _ap_sl_port        = 38281;
static std::string _ap_sl_slot;
static std::string _ap_sl_pass;
static std::string _ap_sl_completed;
static int32_t     _ap_sl_shop_offset = 0;
static int32_t     _ap_sl_shop_days   = 0;
static bool        _ap_sl_goal_sent   = false;
static int64_t     _ap_sl_profit      = 0;
static std::string _ap_sl_cargo_str;
static std::string _ap_sl_maintain_str;
static int32_t     _ap_sl_fuel_ticks      = 0;
static int32_t     _ap_sl_cargo_ticks     = 0;
static int32_t     _ap_sl_reliability_ticks = 0;
static int32_t     _ap_sl_station_ticks   = 0;
static std::string _ap_sl_named_str;         /* named entity data: "loc:id:cumul;..." */

/* ── SaveLoad table ─────────────────────────────────────────────────── */
static const SaveLoad _ap_desc[] = {
    SLEG_SSTR("host",        _ap_sl_host,         SLE_STR),
    SLEG_VAR ("port",        _ap_sl_port,         SLE_UINT16),
    SLEG_SSTR("slot",        _ap_sl_slot,         SLE_STR),
    SLEG_SSTR("pass",        _ap_sl_pass,         SLE_STR),
    SLEG_SSTR("completed",   _ap_sl_completed,    SLE_STR),
    SLEG_VAR ("shop_offset", _ap_sl_shop_offset,  SLE_INT32),
    SLEG_VAR ("shop_days",   _ap_sl_shop_days,    SLE_INT32),
    SLEG_VAR ("goal_sent",   _ap_sl_goal_sent,    SLE_BOOL),
    SLEG_VAR ("profit",      _ap_sl_profit,       SLE_INT64),
    SLEG_SSTR("cargo",       _ap_sl_cargo_str,    SLE_STR),
    SLEG_SSTR("maintain",    _ap_sl_maintain_str,      SLE_STR),
    SLEG_VAR ("fuel_ticks",  _ap_sl_fuel_ticks,        SLE_INT32),
    SLEG_VAR ("cargo_ticks", _ap_sl_cargo_ticks,       SLE_INT32),
    SLEG_VAR ("rel_ticks",   _ap_sl_reliability_ticks, SLE_INT32),
    SLEG_SSTR("named",       _ap_sl_named_str,         SLE_STR),
    SLEG_VAR ("sta_ticks",   _ap_sl_station_ticks,     SLE_INT32),
};

struct APSTChunkHandler : ChunkHandler {
    APSTChunkHandler() : ChunkHandler('APST', CH_TABLE) {}

    void Save() const override
    {
        _ap_sl_host        = _ap_last_host;
        _ap_sl_port        = _ap_last_port;
        _ap_sl_slot        = _ap_last_slot;
        _ap_sl_pass        = _ap_last_pass;
        _ap_sl_completed   = AP_GetCompletedMissionsStr();
        _ap_sl_shop_offset = AP_GetShopPageOffset();
        _ap_sl_shop_days   = AP_GetShopDayCounter();
        _ap_sl_goal_sent   = AP_GetGoalSent();

        constexpr int NC = 64;
        uint64_t cargo[NC] = {};
        int64_t  profit    = 0;
        AP_GetCumulStats(cargo, NC, &profit);
        _ap_sl_profit    = profit;
        _ap_sl_cargo_str = PackCargo_AP(cargo, NC);
        _ap_sl_maintain_str = AP_GetMaintainCountersStr();
        _ap_sl_named_str    = AP_GetNamedEntityStr();

        /* Save active timed effect counters so bonuses/traps survive save/load */
        AP_GetEffectTimers(&_ap_sl_fuel_ticks, &_ap_sl_cargo_ticks,
                           &_ap_sl_reliability_ticks, &_ap_sl_station_ticks);

        SlTableHeader(_ap_desc);
        SlSetArrayIndex(0);
        SlGlobList(_ap_desc);
    }

    void Load() const override
    {
        const std::vector<SaveLoad> slt = SlCompatTableHeader(_ap_desc, {});
        if (SlIterateArray() == -1) return;
        SlGlobList(slt);

        _ap_last_host = _ap_sl_host;
        _ap_last_port = _ap_sl_port;
        _ap_last_slot = _ap_sl_slot;
        _ap_last_pass = _ap_sl_pass;

        AP_SetCompletedMissionsStr(_ap_sl_completed);
        AP_SetShopPageOffset(_ap_sl_shop_offset);
        AP_SetShopDayCounter(_ap_sl_shop_days);
        AP_SetGoalSent(_ap_sl_goal_sent);

        constexpr int NC = 64;
        uint64_t cargo[NC] = {};
        UnpackCargo_AP(_ap_sl_cargo_str, cargo, NC);
        AP_SetCumulStats(cargo, NC, _ap_sl_profit);
        AP_SetMaintainCountersStr(_ap_sl_maintain_str);
        AP_SetNamedEntityStr(_ap_sl_named_str);
        AP_SetEffectTimers(_ap_sl_fuel_ticks, _ap_sl_cargo_ticks,
                           _ap_sl_reliability_ticks, _ap_sl_station_ticks);
    }
};

static const APSTChunkHandler APST;
static const ChunkHandlerRef ap_chunk_handlers[] = { APST };
extern const ChunkHandlerTable _ap_chunk_handlers(ap_chunk_handlers);
