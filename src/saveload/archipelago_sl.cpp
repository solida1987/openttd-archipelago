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

/* Minimal key=value serialiser with escaping for '|', '=', and '\'. */
static std::string KVEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '|':  out += "\\p";  break;
            case '=':  out += "\\e";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string KVUnescape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\\': out += '\\'; i++; break;
                case 'p':  out += '|';  i++; break;
                case 'e':  out += '=';  i++; break;
                default:   out += s[i];      break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::string KVGet(const std::string &blob, const std::string &key, const std::string &def = "")
{
    std::string search = key + "=";
    /* Search for key= at the start of the blob or right after a '|' separator */
    size_t pos = 0;
    while (pos < blob.size()) {
        auto found = blob.find(search, pos);
        if (found == std::string::npos) return def;
        /* Ensure it's at the start or right after '|' (not inside a value) */
        if (found == 0 || blob[found - 1] == '|') {
            size_t val_start = found + search.size();
            auto end = blob.find('|', val_start);
            std::string raw = (end == std::string::npos) ? blob.substr(val_start) : blob.substr(val_start, end - val_start);
            return KVUnescape(raw);
        }
        pos = found + 1;
    }
    return def;
}

static void KVSet(std::string &blob, const std::string &key, const std::string &val)
{
    if (!blob.empty()) blob += '|';
    blob += key + '=' + KVEscape(val);
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
extern bool         _ap_last_ssl;

std::string  AP_GetCompletedMissionsStr();
void         AP_SetCompletedMissionsStr(const std::string &s);
int          AP_GetShopPageOffset();
void         AP_SetShopPageOffset(int v);
int          AP_GetShopDayCounter();
void         AP_SetShopDayCounter(int v);
bool         AP_GetGoalSent();
void         AP_SetGoalSent(bool v);
int64_t      AP_GetItemsReceivedCount();
void         AP_SetItemsReceivedCount(int64_t v);
void         AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out);
void         AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in);
std::string  AP_GetMaintainCountersStr();
void         AP_SetMaintainCountersStr(const std::string &s);
void         AP_GetEffectTimers(int *fuel, int *cargo, int *reliability, int *station, int *license_ticks, int *license_type, int *breakdown);
void         AP_SetEffectTimers(int fuel, int cargo, int reliability, int station, int license_ticks, int license_type, int breakdown);
std::string  AP_GetNamedEntityStr();
void         AP_SetNamedEntityStr(const std::string &s);
std::string  AP_GetSentShopStr();
void         AP_SetSentShopStr(const std::string &s);
uint8_t      AP_GetLockedTrackDirs(uint8_t railtype);
uint8_t      AP_GetLockedTrackDirsRaw(uint8_t ap_index);
void         AP_SetLockedTrackDirs(uint8_t railtype, uint8_t mask);
uint8_t      AP_GetLockedTramDirs();
void         AP_SetLockedTramDirs(uint8_t mask);
// back-compat shims (deprecated)
uint8_t      AP_GetLockedRailDirs();
void         AP_SetLockedRailDirs(uint8_t mask);
void         AP_GetColbyState(int *step, int64_t *delivered, int *target_town, bool *escaped, int *escape_ticks, bool *done, bool *popup_shown, uint32_t *stash_tile);
void         AP_SetColbyState(int step, int64_t delivered, int target_town, bool escaped, int escape_ticks, bool done, bool popup_shown, uint32_t stash_tile);
bool         AP_GetTownsRenamed();
void         AP_SetTownsRenamed(bool v);
uint8_t      AP_GetLockedRoadDirs();
void         AP_SetLockedRoadDirs(uint8_t mask);
uint8_t      AP_GetLockedSignals();
void         AP_SetLockedSignals(uint8_t mask);
uint16_t     AP_GetLockedBridges();
void         AP_SetLockedBridges(uint16_t mask);
bool         AP_GetLockedTunnels();
void         AP_SetLockedTunnels(bool v);
uint16_t     AP_GetLockedAirports();
void         AP_SetLockedAirports(uint16_t mask);
uint16_t     AP_GetLockedTrees();
void         AP_SetLockedTrees(uint16_t mask);
uint8_t      AP_GetLockedTerraform();
void         AP_SetLockedTerraform(uint8_t mask);
uint8_t      AP_GetLockedTownActions();
void         AP_SetLockedTownActions(uint8_t mask);
int          AP_GetFfSpeed();
void         AP_SetFfSpeed(int v);
std::string  AP_GetTasksStr();
void         AP_SetTasksStr(const std::string &s);
int          AP_GetTaskChecksCompletedSaved();
void         AP_SetTaskChecksCompleted(int v);
std::string  AP_GetRuinsStr();
void         AP_SetRuinsStr(const std::string &s);
std::string  AP_GetStarsStr();
void         AP_SetStarsStr(const std::string &s);
std::string  AP_GetNamePoolStr();
void         AP_SetNamePoolStr(const std::string &s);
void         AP_GetDemigodState(std::string *defeated, int *active_idx, int *company,
                                int *next_year, bool *veh_saved, bool *sv_train,
                                bool *sv_road, bool *sv_air, bool *sv_ship);
void         AP_SetDemigodState(const std::string &defeated, int active_idx, int company,
                                int next_year, bool veh_saved, bool sv_train,
                                bool sv_road, bool sv_air, bool sv_ship);
int64_t      AP_GetDemigodPlayerSpawnValue();
void         AP_SetDemigodPlayerSpawnValue(int64_t v);
bool         AP_GetDemigodFriendly();
void         AP_SetDemigodFriendly(bool v);
int64_t      AP_GetDemigodMoneyGiven();
void         AP_SetDemigodMoneyGiven(int64_t v);
int          AP_GetDemigodLastTransferYear();
void         AP_SetDemigodLastTransferYear(int v);
void         AP_GetWrathState(int *anger, int *houses, int *roads, int *terrain, int *trees, int *last_eval_year);
void         AP_SetWrathState(int anger, int houses, int roads, int terrain, int trees, int last_eval_year);

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
        KVSet(_ap_sl_blob, "ssl",   IStr(_ap_last_ssl));

        KVSet(_ap_sl_blob, "completed",   AP_GetCompletedMissionsStr());
        KVSet(_ap_sl_blob, "shop_offset", IStr(AP_GetShopPageOffset()));
        KVSet(_ap_sl_blob, "shop_days",   IStr(AP_GetShopDayCounter()));
        KVSet(_ap_sl_blob, "shop_sent",   AP_GetSentShopStr());
        KVSet(_ap_sl_blob, "goal_sent",   IStr(AP_GetGoalSent()));
        KVSet(_ap_sl_blob, "items_recv",  IStr(AP_GetItemsReceivedCount()));
        KVSet(_ap_sl_blob, "rail_dir_locks_0", IStr((int)AP_GetLockedTrackDirsRaw(0)));
        KVSet(_ap_sl_blob, "rail_dir_locks_1", IStr((int)AP_GetLockedTrackDirsRaw(1)));
        KVSet(_ap_sl_blob, "rail_dir_locks_2", IStr((int)AP_GetLockedTrackDirsRaw(2)));
        KVSet(_ap_sl_blob, "rail_dir_locks_3", IStr((int)AP_GetLockedTrackDirsRaw(3)));
        KVSet(_ap_sl_blob, "rail_dir_locks_4", IStr((int)AP_GetLockedTrackDirsRaw(4)));  /* Narrow Gauge */
        KVSet(_ap_sl_blob, "rail_dir_locks_5", IStr((int)AP_GetLockedTrackDirsRaw(5)));  /* Metro */
        KVSet(_ap_sl_blob, "rail_dir_locks_6", IStr((int)AP_GetLockedTrackDirsRaw(6)));  /* Vacuum Tube */

        constexpr int NC = 64;
        uint64_t cargo[NC] = {};
        int64_t  profit    = 0;
        AP_GetCumulStats(cargo, NC, &profit);
        KVSet(_ap_sl_blob, "profit",   IStr(profit));
        KVSet(_ap_sl_blob, "cargo",    PackCargo_AP(cargo, NC));
        KVSet(_ap_sl_blob, "maintain", AP_GetMaintainCountersStr());
        KVSet(_ap_sl_blob, "named",    AP_GetNamedEntityStr());

        int fuel = 0, carg = 0, rel = 0, sta = 0, lic_t = 0, lic_v = -1, brk = 0;
        AP_GetEffectTimers(&fuel, &carg, &rel, &sta, &lic_t, &lic_v, &brk);
        KVSet(_ap_sl_blob, "fuel_ticks",  IStr(fuel));
        KVSet(_ap_sl_blob, "cargo_ticks", IStr(carg));
        KVSet(_ap_sl_blob, "rel_ticks",   IStr(rel));
        KVSet(_ap_sl_blob, "sta_ticks",   IStr(sta));
        KVSet(_ap_sl_blob, "lic_ticks",   IStr(lic_t));
        KVSet(_ap_sl_blob, "lic_type",    IStr(lic_v));
        KVSet(_ap_sl_blob, "brk_ticks",   IStr(brk));

        /* Colby Event */
        int   co_step = 0; int64_t co_del = 0; int co_town = (int)UINT16_MAX;
        bool  co_esc = false; int co_eticks = 0; bool co_done = false; bool co_pop = false;
        uint32_t co_stash = UINT32_MAX;
        AP_GetColbyState(&co_step, &co_del, &co_town, &co_esc, &co_eticks, &co_done, &co_pop, &co_stash);
        KVSet(_ap_sl_blob, "co_step",   IStr(co_step));
        KVSet(_ap_sl_blob, "co_del",    IStr(co_del));
        KVSet(_ap_sl_blob, "co_town",   IStr(co_town));
        KVSet(_ap_sl_blob, "co_esc",    IStr(co_esc));
        KVSet(_ap_sl_blob, "co_eticks", IStr(co_eticks));
        KVSet(_ap_sl_blob, "co_done",   IStr(co_done));
        KVSet(_ap_sl_blob, "co_pop",    IStr(co_pop));
        KVSet(_ap_sl_blob, "co_stash",  IStr((int64_t)co_stash));

        KVSet(_ap_sl_blob, "towns_renamed", IStr(AP_GetTownsRenamed()));

        /* Infrastructure lock states */
        KVSet(_ap_sl_blob, "road_dir_locks",  IStr((int)AP_GetLockedRoadDirs()));
        KVSet(_ap_sl_blob, "tram_dir_locks",  IStr((int)AP_GetLockedTramDirs()));
        KVSet(_ap_sl_blob, "signal_locks",    IStr((int)AP_GetLockedSignals()));
        KVSet(_ap_sl_blob, "bridge_locks",    IStr((int)AP_GetLockedBridges()));
        KVSet(_ap_sl_blob, "tunnel_locks",    IStr(AP_GetLockedTunnels()));
        KVSet(_ap_sl_blob, "airport_locks",   IStr((int)AP_GetLockedAirports()));
        KVSet(_ap_sl_blob, "tree_locks",      IStr((int)AP_GetLockedTrees()));
        KVSet(_ap_sl_blob, "terraform_locks", IStr((int)AP_GetLockedTerraform()));
        KVSet(_ap_sl_blob, "town_action_locks", IStr((int)AP_GetLockedTownActions()));

        /* Speed Boost & Task System */
        KVSet(_ap_sl_blob, "ff_speed",    IStr(AP_GetFfSpeed()));
        KVSet(_ap_sl_blob, "tasks",       AP_GetTasksStr());
        KVSet(_ap_sl_blob, "task_checks", IStr(AP_GetTaskChecksCompletedSaved()));

        /* Ruins */
        KVSet(_ap_sl_blob, "ruins",       AP_GetRuinsStr());

        /* Stars */
        KVSet(_ap_sl_blob, "stars",       AP_GetStarsStr());

        /* Vehicle name pool */
        KVSet(_ap_sl_blob, "name_pool",   AP_GetNamePoolStr());

        /* Demigods (God of Wackens) */
        {
            std::string defeated; int aidx, acomp, nyear;
            bool vs, st, sr, sa, ss;
            AP_GetDemigodState(&defeated, &aidx, &acomp, &nyear, &vs, &st, &sr, &sa, &ss);
            KVSet(_ap_sl_blob, "dg_defeated",  defeated);
            KVSet(_ap_sl_blob, "dg_active",    IStr(aidx));
            KVSet(_ap_sl_blob, "dg_company",   IStr(acomp));
            KVSet(_ap_sl_blob, "dg_next_yr",   IStr(nyear));
            KVSet(_ap_sl_blob, "dg_veh_saved", IStr(vs));
            KVSet(_ap_sl_blob, "dg_sv_train",  IStr(st));
            KVSet(_ap_sl_blob, "dg_sv_road",   IStr(sr));
            KVSet(_ap_sl_blob, "dg_sv_air",    IStr(sa));
            KVSet(_ap_sl_blob, "dg_sv_ship",   IStr(ss));
            KVSet(_ap_sl_blob, "dg_pval",      IStr(AP_GetDemigodPlayerSpawnValue()));
            KVSet(_ap_sl_blob, "dg_friendly",  IStr(AP_GetDemigodFriendly() ? 1 : 0));
            KVSet(_ap_sl_blob, "dg_mgiven",    IStr(AP_GetDemigodMoneyGiven()));
            KVSet(_ap_sl_blob, "dg_xfer_yr",   IStr(AP_GetDemigodLastTransferYear()));
        }

        /* Wrath of the God of Wackens */
        {
            int anger, houses, roads, terrain, trees, eval_yr;
            AP_GetWrathState(&anger, &houses, &roads, &terrain, &trees, &eval_yr);
            KVSet(_ap_sl_blob, "wr_anger",   IStr(anger));
            KVSet(_ap_sl_blob, "wr_houses",  IStr(houses));
            KVSet(_ap_sl_blob, "wr_roads",   IStr(roads));
            KVSet(_ap_sl_blob, "wr_terrain", IStr(terrain));
            KVSet(_ap_sl_blob, "wr_trees",   IStr(trees));
            KVSet(_ap_sl_blob, "wr_eval_yr", IStr(eval_yr));
        }

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
            _ap_last_ssl  = KVGet(_ap_sl_blob, "ssl", "0") == "1";

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
            AP_SetItemsReceivedCount(ParseI64(KVGet(_ap_sl_blob, "items_recv", "0")));
            /* Track direction locks: read per-railtype bytes (new format).
             * Fallback: if new keys absent but old 'rail_dir_locks' present,
             * apply old global value to Normal Rail only (safe migration). */
            {
                std::string old_key = KVGet(_ap_sl_blob, "rail_dir_locks", "");
                for (int rt = 0; rt < 7; rt++) {
                    std::string key = fmt::format("rail_dir_locks_{}", rt);
                    std::string val = KVGet(_ap_sl_blob, key, "");
                    if (!val.empty()) {
                        AP_SetLockedTrackDirs((uint8_t)rt, (uint8_t)ParseInt(val));
                    } else if (rt == 0 && !old_key.empty()) {
                        /* Migration from patch_exp_2_0_6: apply old single mask to Normal Rail */
                        AP_SetLockedTrackDirs(0, (uint8_t)ParseInt(old_key));
                    }
                }
            }

            /* Infrastructure lock states */
            AP_SetLockedRoadDirs((uint8_t)getint("road_dir_locks"));
            AP_SetLockedTramDirs((uint8_t)getint("tram_dir_locks"));
            AP_SetLockedSignals((uint8_t)getint("signal_locks"));
            AP_SetLockedBridges((uint16_t)ParseU16(KVGet(_ap_sl_blob, "bridge_locks", "0")));
            AP_SetLockedTunnels(KVGet(_ap_sl_blob, "tunnel_locks", "0") == "1");
            AP_SetLockedAirports((uint16_t)ParseU16(KVGet(_ap_sl_blob, "airport_locks", "0")));
            AP_SetLockedTrees((uint16_t)ParseU16(KVGet(_ap_sl_blob, "tree_locks", "0")));
            AP_SetLockedTerraform((uint8_t)getint("terraform_locks"));
            AP_SetLockedTownActions((uint8_t)getint("town_action_locks"));

            constexpr int NC = 64;
            uint64_t cargo[NC] = {};
            UnpackCargo_AP(KVGet(_ap_sl_blob, "cargo"), cargo, NC);
            AP_SetCumulStats(cargo, NC, getint64("profit"));

            AP_SetMaintainCountersStr(KVGet(_ap_sl_blob, "maintain"));
            AP_SetNamedEntityStr(KVGet(_ap_sl_blob, "named"));

            AP_SetEffectTimers(getint("fuel_ticks"), getint("cargo_ticks"),
                               getint("rel_ticks"),  getint("sta_ticks"),
                               getint("lic_ticks"),  getint("lic_type"),
                               getint("brk_ticks"));

            /* Colby Event */
            AP_SetColbyState(
                getint("co_step"),
                ParseI64(KVGet(_ap_sl_blob, "co_del", "0")),
                getint("co_town"),
                KVGet(_ap_sl_blob, "co_esc",  "0") == "1",
                getint("co_eticks"),
                KVGet(_ap_sl_blob, "co_done", "0") == "1",
                KVGet(_ap_sl_blob, "co_pop",  "0") == "1",
                (uint32_t)ParseI64(KVGet(_ap_sl_blob, "co_stash", "4294967295"))
            );

            AP_SetTownsRenamed(KVGet(_ap_sl_blob, "towns_renamed", "0") == "1");

            /* Speed Boost & Task System */
            AP_SetFfSpeed(getint("ff_speed"));
            AP_SetTasksStr(KVGet(_ap_sl_blob, "tasks"));
            AP_SetTaskChecksCompleted(getint("task_checks"));

            /* Ruins */
            AP_SetRuinsStr(KVGet(_ap_sl_blob, "ruins"));

            /* Stars */
            AP_SetStarsStr(KVGet(_ap_sl_blob, "stars"));

            /* Vehicle name pool */
            AP_SetNamePoolStr(KVGet(_ap_sl_blob, "name_pool"));

            /* Demigods (God of Wackens) */
            AP_SetDemigodState(
                KVGet(_ap_sl_blob, "dg_defeated"),
                getint("dg_active"),
                getint("dg_company"),
                getint("dg_next_yr"),
                KVGet(_ap_sl_blob, "dg_veh_saved", "0") == "1",
                KVGet(_ap_sl_blob, "dg_sv_train", "0") == "1",
                KVGet(_ap_sl_blob, "dg_sv_road", "0") == "1",
                KVGet(_ap_sl_blob, "dg_sv_air", "0") == "1",
                KVGet(_ap_sl_blob, "dg_sv_ship", "0") == "1"
            );
            AP_SetDemigodPlayerSpawnValue(getint64("dg_pval"));
            AP_SetDemigodFriendly(KVGet(_ap_sl_blob, "dg_friendly", "0") == "1");
            AP_SetDemigodMoneyGiven(getint64("dg_mgiven"));
            AP_SetDemigodLastTransferYear(getint("dg_xfer_yr"));

            /* Wrath of the God of Wackens */
            AP_SetWrathState(
                getint("wr_anger"),
                getint("wr_houses"),
                getint("wr_roads"),
                getint("wr_terrain"),
                getint("wr_trees"),
                getint("wr_eval_yr")
            );
        } catch (const std::exception &) {
            /* KV escaping should prevent parse errors; if we still get here
             * the AP state string was corrupt — silently start fresh. */
        } catch (...) {
            /* Unknown exception — start fresh. */
        }
    }
};

static const APSTChunkHandler APST;
static const ChunkHandlerRef ap_chunk_handlers[] = { APST };
extern const ChunkHandlerTable _ap_chunk_handlers(ap_chunk_handlers);
