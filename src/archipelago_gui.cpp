/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#include "stdafx.h"
#include "core/format.hpp"
#include "core/string_consumer.hpp"
#include "archipelago.h"
#include "archipelago_gui.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "querystring_gui.h"
#include "fontcache.h"
#include "currency.h"
#include "newgrf_config.h"
#include "timer/timer_game_calendar.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "town.h"
#include "safeguards.h"

/* Forward declaration — defined in newgrf_gui.cpp */
void ShowNewGRFSettings(bool editable, bool show_params, bool exec_changes, GRFConfigList &config);
/* _grfconfig_newgame is the GRF list used for new game generation (not the running game). */
extern GRFConfigList _grfconfig_newgame;

/* =========================================================================
 * CONNECT WINDOW
 * ========================================================================= */

enum APWidgets : WidgetID {
	WAPGUI_LABEL_SERVER,
	WAPGUI_EDIT_SERVER,
	WAPGUI_LABEL_SLOT,
	WAPGUI_EDIT_SLOT,
	WAPGUI_LABEL_PASS,
	WAPGUI_EDIT_PASS,
	WAPGUI_STATUS,
	WAPGUI_SLOT_INFO,
	WAPGUI_BTN_CONNECT,
	WAPGUI_BTN_DISCONNECT,
	WAPGUI_BTN_NEWGRF,
	WAPGUI_BTN_CLOSE,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_widgets = {
	NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4), SetPadding(6),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_SERVER), SetStringTip(STR_ARCHIPELAGO_LABEL_SERVER), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_SERVER), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_SLOT), SetStringTip(STR_ARCHIPELAGO_LABEL_SLOT), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_SLOT), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_PASS), SetStringTip(STR_ARCHIPELAGO_LABEL_PASS), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_PASS), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_STATUS),    SetMinimalSize(284, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_SLOT_INFO), SetMinimalSize(284, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPGUI_BTN_CONNECT),    SetStringTip(STR_ARCHIPELAGO_BTN_CONNECT),    SetMinimalSize(72, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_RED,    WAPGUI_BTN_DISCONNECT), SetStringTip(STR_ARCHIPELAGO_BTN_DISCONNECT), SetMinimalSize(72, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPGUI_BTN_NEWGRF),     SetStringTip(STR_ARCHIPELAGO_BTN_NEWGRF),     SetMinimalSize(72, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPGUI_BTN_CLOSE),      SetStringTip(STR_ARCHIPELAGO_BTN_CLOSE),      SetMinimalSize(72, 14),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoConnectWindow : public Window {
	QueryString server_buf;
	QueryString slot_buf;
	QueryString pass_buf;
	std::string server_str, slot_str, pass_str;
	APState  last_state  = APState::DISCONNECTED;
	bool     last_has_sd = false;

	static std::string WinDiffName(APWinDifficulty d) {
		switch (d) {
			case APWinDifficulty::CASUAL:    return "Casual";
			case APWinDifficulty::EASY:      return "Easy";
			case APWinDifficulty::NORMAL:    return "Normal";
			case APWinDifficulty::MEDIUM:    return "Medium";
			case APWinDifficulty::HARD:      return "Hard";
			case APWinDifficulty::VERY_HARD: return "Very Hard";
			case APWinDifficulty::EXTREME:   return "Extreme";
			case APWinDifficulty::INSANE:    return "Insane";
			case APWinDifficulty::NUTCASE:   return "Nutcase";
			case APWinDifficulty::MADNESS:   return "Madness";
			case APWinDifficulty::CUSTOM:    return "Custom";
			default:                         return "?";
		}
	}

	static std::string StatusStr(APState s, bool has_sd, const std::string &err) {
		switch (s) {
			case APState::DISCONNECTED:   return "Not connected.";
			case APState::CONNECTING:     return "Connecting...";
			case APState::CONNECTED:      return "Authenticating...";
			case APState::AUTHENTICATING: return "Authenticating...";
			case APState::AUTHENTICATED:  return has_sd ? "Connected! Slot data loaded." : "Connected. Waiting for data...";
			case APState::AP_ERROR:       return "Error: " + err;
			default:                      return "...";
		}
	}

	static std::string SlotInfoStr(bool has_sd) {
		if (!has_sd || _ap_client == nullptr) return "";
		APSlotData sd = _ap_client->GetSlotData();
		return "Difficulty: " + WinDiffName(sd.win_difficulty) +
		       "  Missions: " + fmt::format("{}", sd.missions.size()) +
		       "  Start: " + sd.starting_vehicle;
	}

	ArchipelagoConnectWindow(WindowDesc &desc, WindowNumber wnum)
		: Window(desc), server_buf(256), slot_buf(64), pass_buf(64)
	{
		this->CreateNestedTree();
		this->querystrings[WAPGUI_EDIT_SERVER] = &server_buf;
		this->querystrings[WAPGUI_EDIT_SLOT]   = &slot_buf;
		this->querystrings[WAPGUI_EDIT_PASS]   = &pass_buf;
		this->FinishInitNested(wnum);

		/* Restore last connection settings from ap_connection.cfg */
		AP_LoadConnectionConfig();

		std::string full = _ap_last_host.empty() ? "archipelago.gg:38281"
		                 : _ap_last_host + ":" + fmt::format("{}", _ap_last_port);
		server_buf.text.Assign(full.c_str());
		server_str = full;
		if (!_ap_last_slot.empty()) { slot_buf.text.Assign(_ap_last_slot.c_str()); slot_str = _ap_last_slot; }
		if (!_ap_last_pass.empty()) { pass_buf.text.Assign(_ap_last_pass.c_str()); pass_str = _ap_last_pass; }
	}

	void OnEditboxChanged(WidgetID wid) override {
		switch (wid) {
			case WAPGUI_EDIT_SERVER: server_str = server_buf.text.GetText().data(); break;
			case WAPGUI_EDIT_SLOT:   slot_str   = slot_buf.text.GetText().data();   break;
			case WAPGUI_EDIT_PASS:   pass_str   = pass_buf.text.GetText().data();   break;
		}
	}

	void OnGameTick() override {
		if (_ap_client == nullptr) return;
		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		if (s != last_state || h != last_has_sd) { last_state = s; last_has_sd = h; this->SetDirty(); }
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (_ap_client == nullptr) return;
		switch (widget) {
			case WAPGUI_STATUS:
				DrawString(r.left, r.right, r.top,
				    StatusStr(_ap_client->GetState(), _ap_client->HasSlotData(), _ap_client->GetLastError()),
				    TC_BLACK);
				break;
			case WAPGUI_SLOT_INFO:
				DrawString(r.left, r.right, r.top, SlotInfoStr(_ap_client->HasSlotData()), TC_DARK_GREEN);
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPGUI_BTN_CONNECT: {
				if (_ap_client == nullptr) break;
				/* Strip any scheme prefix the user may have typed — auto-detect handles it */
				std::string raw = server_str;
				if (raw.rfind("wss://", 0) == 0)    raw = raw.substr(6);
				else if (raw.rfind("ws://", 0) == 0) raw = raw.substr(5);
				std::string host = raw;
				uint16_t port = 38281;
				auto colon = raw.rfind(':');
				if (colon != std::string::npos) {
					host = raw.substr(0, colon);
					int p = ParseInteger<int>(raw.substr(colon + 1)).value_or(0);
					if (p > 0 && p < 65536) port = (uint16_t)p;
				}
				_ap_last_host = host; _ap_last_port = port;
				_ap_last_slot = slot_str; _ap_last_pass = pass_str;
				AP_SaveConnectionConfig(); /* persist for next session */
				_ap_last_ssl = false; /* unused — auto-detect in WorkerThread */
				_ap_client->Connect(host, port, slot_str, pass_str, "OpenTTD-Exp", false);
				this->SetDirty();
				break;
			}
			case WAPGUI_BTN_DISCONNECT:
				if (_ap_client) _ap_client->Disconnect();
				this->SetDirty();
				break;
			case WAPGUI_BTN_NEWGRF:
				/* Open NewGRF settings editing the NEW GAME config (_grfconfig_newgame),
				 * not the running game's config (_grfconfig).  This matches what the
				 * world-generation dialog does (genworld_gui.cpp:849). Changes made here
				 * will apply to the next AP-generated world. */
				ShowNewGRFSettings(true, true, false, _grfconfig_newgame);
				break;
			case WAPGUI_BTN_CLOSE:
				this->Close();
				break;
		}
	}
};

static WindowDesc _ap_connect_desc(
	WDP_CENTER, {}, 380, 196,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_widgets
);

void ShowArchipelagoConnectWindow()
{
	if (_ap_client == nullptr) InitArchipelago();
	AllocateWindowDescFront<ArchipelagoConnectWindow>(_ap_connect_desc, 0);
}

/* =========================================================================
 * STATUS WINDOW — persistent top-right overlay
 * ========================================================================= */

enum APStatusWidgets : WidgetID {
	WAPST_STATUS_LINE,
	WAPST_GOAL_LINE,
	WAPST_WIN_CV,    ///< Company Value progress line
	WAPST_WIN_POP,   ///< Town Population progress line
	WAPST_WIN_VEH,   ///< Vehicle Count progress line
	WAPST_WIN_CARGO, ///< Cargo Delivered progress line
	WAPST_WIN_PROF,  ///< Monthly Profit progress line
	WAPST_WIN_MISS,  ///< Missions Completed progress line
	WAPST_BTN_RECONNECT,
	WAPST_BTN_MISSIONS,
	WAPST_BTN_SETTINGS,
	WAPST_BTN_SHOP,
	WAPST_BTN_GUIDE,
	WAPST_BTN_COLBY,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_status_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_ARCHIPELAGO_STATUS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_VERTICAL), SetPIP(2, 2, 2), SetPadding(4),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_STATUS_LINE), SetMinimalSize(340, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_GOAL_LINE),   SetMinimalSize(340, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			/* 6 win condition progress lines */
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_WIN_CV),    SetMinimalSize(240, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_WIN_POP),   SetMinimalSize(340, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_WIN_VEH),   SetMinimalSize(340, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_WIN_CARGO), SetMinimalSize(340, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_WIN_PROF),  SetMinimalSize(340, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_WIN_MISS),  SetMinimalSize(340, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPST_BTN_RECONNECT), SetStringTip(STR_ARCHIPELAGO_BTN_RECONNECT), SetMinimalSize(80, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPST_BTN_MISSIONS),  SetStringTip(STR_ARCHIPELAGO_BTN_MISSIONS),  SetMinimalSize(80, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPST_BTN_SETTINGS),  SetStringTip(STR_ARCHIPELAGO_BTN_SETTINGS),  SetMinimalSize(80, 14),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPST_BTN_SHOP),      SetStringTip(STR_ARCHIPELAGO_BTN_SHOP),      SetMinimalSize(170, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BLUE,   WAPST_BTN_GUIDE),     SetStringTip(STR_ARCHIPELAGO_BTN_GUIDE),     SetMinimalSize(80, 14),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPST_BTN_COLBY),     SetStringTip(STR_ARCHIPELAGO_BTN_COLBY),     SetMinimalSize(280, 14), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoStatusWindow : public Window {
	APState last_state  = APState::DISCONNECTED;
	bool    last_has_sd = false;

	ArchipelagoStatusWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->CreateNestedTree();
		this->FinishInitNested(wnum);
	}

	static std::string StatusLine() {
		if (_ap_client == nullptr) return "AP: No client";
		switch (_ap_client->GetState()) {
			case APState::AUTHENTICATED:  return "AP: Connected";
			case APState::CONNECTING:     return "AP: Connecting...";
			case APState::AUTHENTICATING: return "AP: Authenticating...";
			case APState::AP_ERROR:       return "AP: Error - " + _ap_client->GetLastError();
			case APState::DISCONNECTED:   return "AP: Disconnected";
			default:                      return "AP: ...";
		}
	}

	static std::string GoalLine() {
		if (_ap_client == nullptr || !_ap_client->HasSlotData()) return "No slot data";
		const APSlotData &sd = AP_GetSlotData();
		// brief one-liner: difficulty name
		std::string diff_names[] = {"Casual","Easy","Normal","Medium","Hard",
		    "Very Hard","Extreme","Insane","Nutcase","Madness","Custom"};
		int di = (int)sd.win_difficulty;
		std::string dn = (di >= 0 && di <= 10) ? diff_names[di] : "?";
		return "Goal: " + dn + "  (click Guide for details)";
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		if (_ap_client == nullptr) return;
		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		bool    d = _ap_status_dirty.exchange(false);
		if (s != last_state || h != last_has_sd || d) {
			last_state = s; last_has_sd = h;
			this->SetDirty();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		switch (widget) {
			case WAPST_STATUS_LINE: {
				bool ok = (_ap_client && _ap_client->GetState() == APState::AUTHENTICATED);
				DrawString(r.left, r.right, r.top, StatusLine(), ok ? TC_GREEN : TC_RED);
				break;
			}
			case WAPST_GOAL_LINE:
				DrawString(r.left, r.right, r.top, GoalLine(), TC_GOLD);
				break;

			case WAPST_WIN_CV:
			case WAPST_WIN_POP:
			case WAPST_WIN_VEH:
			case WAPST_WIN_CARGO:
			case WAPST_WIN_PROF:
			case WAPST_WIN_MISS:
				if (_ap_client != nullptr && _ap_client->HasSlotData()) {
					DrawWinLine(r, widget);
				}
				break;
		}
	}

	/** Format a number with dot separators: 1234567 -> "1.234.567" */
	static std::string FmtNum(int64_t v) {
		if (v < 0) return "-" + FmtNum(-v);
		std::string s = fmt::format("{}", v);
		int ins = (int)s.size() - 3;
		while (ins > 0) { s.insert(ins, "."); ins -= 3; }
		return s;
	}

	/** Abbreviate large numbers: 1234567 -> "1.23M", 50000000000 -> "50B" */
	static std::string FmtShort(int64_t v) {
		if (v < 0) return "-" + FmtShort(-v);
		if (v >= 1'000'000'000LL) {
			double d = v / 1'000'000'000.0;
			if (d >= 100.0) return fmt::format("{:.0f}B", d);
			if (d >= 10.0)  return fmt::format("{:.1f}B", d);
			return fmt::format("{:.2f}B", d);
		}
		if (v >= 1'000'000LL) {
			double d = v / 1'000'000.0;
			if (d >= 100.0) return fmt::format("{:.0f}M", d);
			if (d >= 10.0)  return fmt::format("{:.1f}M", d);
			return fmt::format("{:.2f}M", d);
		}
		if (v >= 1'000LL) {
			double d = v / 1'000.0;
			if (d >= 100.0) return fmt::format("{:.0f}k", d);
			if (d >= 10.0)  return fmt::format("{:.1f}k", d);
			return fmt::format("{:.2f}k", d);
		}
		return fmt::format("{}", v);
	}

	/** Draw one win-condition line as a single left-aligned string.
	 *  Format:  "Label        cur / tgt   (XX%)  [OK]"
	 *  Numbers are abbreviated (M/B/k) so everything fits on one line. */
	static void DrawWinLine(const Rect &r, WidgetID widget) {
		const APSlotData &sd   = AP_GetSlotData();
		APWinProgress     prog = AP_GetWinProgress();
		int64_t cur = 0, tgt = 1;
		const char *label = "";
		switch (widget) {
			case WAPST_WIN_CV:
				cur = prog.company_value;   tgt = sd.win_target_company_value;   label = "Company Val"; break;
			case WAPST_WIN_POP:
				cur = prog.town_population; tgt = sd.win_target_town_population; label = "World Pop  "; break;
			case WAPST_WIN_VEH:
				cur = prog.vehicle_count;   tgt = sd.win_target_vehicle_count;   label = "Vehicles   "; break;
			case WAPST_WIN_CARGO:
				cur = prog.cargo_delivered; tgt = sd.win_target_cargo_delivered; label = "Cargo (t)  "; break;
			case WAPST_WIN_PROF:
				cur = prog.monthly_profit;  tgt = sd.win_target_monthly_profit;  label = "Mo. Profit "; break;
			case WAPST_WIN_MISS:
				cur = prog.missions;        tgt = sd.win_target_missions;        label = "Missions   "; break;
			default: return;
		}
		bool done = (cur >= tgt);
		int64_t pct = (tgt > 0) ? std::min<int64_t>(100LL, cur * 100LL / tgt) : 100LL;
		TextColour col = done ? TC_LIGHT_BLUE : TC_WHITE;

		std::string line = fmt::format("{}  {} / {}   ({}%){}",
		    label,
		    FmtShort(cur),
		    FmtShort(tgt),
		    pct,
		    done ? "  [OK]" : "");
		DrawString(r.left, r.right, r.top, line, col);
	}

	void OnPaint() override {
		bool disconnected = (_ap_client == nullptr ||
		    _ap_client->GetState() == APState::DISCONNECTED ||
		    _ap_client->GetState() == APState::AP_ERROR);
		this->SetWidgetDisabledState(WAPST_BTN_RECONNECT, !disconnected || _ap_last_host.empty());
		this->SetWidgetDisabledState(WAPST_BTN_MISSIONS, !AP_IsConnected());
		this->SetWidgetDisabledState(WAPST_BTN_SHOP,     !AP_IsConnected());
		/* Colby button: enabled when event is configured (shows countdown in step 0,
		 * full event UI once active). Disabled when no event is configured at all. */
		bool colby_configured = AP_IsConnected() && AP_IsColbyConfigured();
		this->SetWidgetDisabledState(WAPST_BTN_COLBY, !colby_configured);
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPST_BTN_RECONNECT:
				if (_ap_client && !_ap_last_host.empty())
					_ap_client->Connect(_ap_last_host, _ap_last_port, _ap_last_slot, _ap_last_pass, "OpenTTD-Exp", _ap_last_ssl);
				break;
			case WAPST_BTN_MISSIONS:
				ShowArchipelagoMissionsWindow();
				break;
			case WAPST_BTN_SETTINGS:
				ShowArchipelagoConnectWindow();
				break;
			case WAPST_BTN_SHOP:
				ShowArchipelagoShopWindow();
				break;
			case WAPST_BTN_GUIDE:
				ShowArchipelagoGuideWindow();
				break;
			case WAPST_BTN_COLBY:
				ShowArchipelagoColbyWindow();
				break;
		}
	}
};

static WindowDesc _ap_status_desc(
	WDP_AUTO, {"ap_status"}, 228, 82,
	WC_ARCHIPELAGO_TICKER, WC_NONE, {},
	_nested_ap_status_widgets
);

void ShowArchipelagoStatusWindow()
{
	AllocateWindowDescFront<ArchipelagoStatusWindow>(_ap_status_desc, 0);
}

/* =========================================================================
 * MISSIONS WINDOW (Bug F fix)
 *
 * Scrollable list of all missions from slot_data.
 * Shows: difficulty colour, description, completed/pending status.
 * ========================================================================= */

enum APMissionsWidgets : WidgetID {
	WAPM_CAPTION,
	WAPM_FILTER_ALL,
	WAPM_FILTER_EASY,
	WAPM_FILTER_MEDIUM,
	WAPM_FILTER_HARD,
	WAPM_FILTER_EXTREME,
	WAPM_FILTER_TASKS,  ///< Filter: show local tasks
	WAPM_SCROLLBAR,
	WAPM_HSCROLLBAR,
	WAPM_LIST,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_missions_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_MISSIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		/* Single filter row — All / Easy / Medium / Hard / Extreme / Tasks */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 2, 2), SetPadding(2),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPM_FILTER_ALL),     SetStringTip(STR_ARCHIPELAGO_FILTER_ALL),     SetMinimalSize(44, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPM_FILTER_EASY),    SetStringTip(STR_ARCHIPELAGO_FILTER_EASY),    SetMinimalSize(44, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPM_FILTER_MEDIUM),  SetStringTip(STR_ARCHIPELAGO_FILTER_MEDIUM),  SetMinimalSize(44, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPM_FILTER_HARD),    SetStringTip(STR_ARCHIPELAGO_FILTER_HARD),    SetMinimalSize(44, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_RED,    WAPM_FILTER_EXTREME), SetStringTip(STR_ARCHIPELAGO_FILTER_EXTREME), SetMinimalSize(44, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN,  WAPM_FILTER_TASKS),   SetStringTip(STR_ARCHIPELAGO_TAB_TASKS),      SetMinimalSize(44, 14),
		EndContainer(),
		/* Mission list + vertical scrollbar */
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WAPM_LIST), SetMinimalSize(460, 300), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPM_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPM_SCROLLBAR),
		EndContainer(),
		/* Horizontal scrollbar + resize box */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HSCROLLBAR, COLOUR_BROWN, WAPM_HSCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/* ---------------------------------------------------------------------------
 * Shared currency formatting helpers (used by both mission and shop windows).
 * Uses GetCurrency() so the symbol and rate are always in sync with game settings.
 * --------------------------------------------------------------------------- */

/** Compact money string: "$50M", "£200k", "1,5M kr" etc. */
static std::string AP_FormatMoneyCompact(int64_t amount)
{
	const CurrencySpec &cs = GetCurrency();
	int64_t scaled = amount * cs.rate;

	std::string num;
	if (scaled >= 1000000) num = fmt::format("{:.1f}M", scaled / 1000000.0);
	else if (scaled >= 1000) num = fmt::format("{}k", scaled / 1000);
	else num = fmt::format("{}", scaled);

	if (cs.symbol_pos == 0) return cs.prefix + num + cs.suffix;
	return num + cs.suffix;
}

struct ArchipelagoMissionsWindow : public Window {
	int           row_height    = 0;      /* computed in constructor from font height */
	int           max_line_px   = 0;      /* width in pixels of the longest line */
	std::string   filter        = "all";  /* "all","easy","medium","hard","extreme" */
	bool          show_tasks    = false;  /* true = Tasks tab, false = Missions tab */
	std::vector<const APMission *> visible_missions;
	std::vector<APTask> cached_tasks;     /* snapshot for task tab rendering */
	Scrollbar *scrollbar  = nullptr;
	Scrollbar *hscrollbar = nullptr;

	int ComputeTaskTotalRows() const {
		if (cached_tasks.empty()) return 1;
		return 1 + 3 + (int)(cached_tasks.size() - 1) * 4;
	}

	void RebuildTaskList() {
		cached_tasks = AP_GetTaskSnapshot();
		if (this->scrollbar && show_tasks) {
			this->scrollbar->SetCount(ComputeTaskTotalRows());
		}
		/* Compute max line width */
		max_line_px = 0;
		for (const APTask &t : cached_tasks) {
			int w = GetStringBoundingBox(t.description).width + 60;
			if (w > max_line_px) max_line_px = w;
		}
		UpdateHScrollbar();
		this->SetDirty();
	}

	void RebuildVisibleList() {
		if (show_tasks) { RebuildTaskList(); return; }
		visible_missions.clear();
		const APSlotData &sd = AP_GetSlotData();
		for (const APMission &m : sd.missions) {
			if (filter == "all" || m.difficulty == filter)
				visible_missions.push_back(&m);
		}
		if (this->scrollbar) {
			this->scrollbar->SetCount((int)visible_missions.size());
		}
		/* Compute max line pixel width so the horizontal scrollbar range is correct */
		max_line_px = 0;
		for (const APMission *m : visible_missions) {
			std::string cap_diff = m->difficulty.empty() ? "" :
				std::string(1, (char)toupper((unsigned char)m->difficulty[0])) + m->difficulty.substr(1);
			std::string prefix = m->completed ? "[X] " : "[ ] ";
			{ auto lu = m->location.rfind('_'); if (lu != std::string::npos) cap_diff += " #" + m->location.substr(lu + 1); }
			std::string line = prefix + cap_diff + " - " + m->description;
			int w = GetStringBoundingBox(line).width;
			if (w > max_line_px) max_line_px = w;
		}
		UpdateHScrollbar();
		this->SetDirty();
	}

	void UpdateHScrollbar() {
		if (!this->hscrollbar) return;
		NWidgetBase *nw = this->GetWidget<NWidgetBase>(WAPM_LIST);
		int visible_w = (nw != nullptr) ? (int)nw->current_x - 8 : 460;
		int total     = std::max(max_line_px + 16, visible_w);
		this->hscrollbar->SetCount(total);
		this->hscrollbar->SetCapacity(visible_w);
	}

	ArchipelagoMissionsWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3; /* +3px vertical padding */
		this->CreateNestedTree();
		this->scrollbar  = this->GetScrollbar(WAPM_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->hscrollbar = this->GetScrollbar(WAPM_HSCROLLBAR);
		this->hscrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		this->resize.step_width  = 1;
		this->SetWidgetLoweredState(WAPM_FILTER_ALL, true);
		RebuildVisibleList();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		/* Always rebuild — manager calls SetWindowClassesDirty every 250 ms
		 * so this fires continuously for real-time mission progress display. */
		if (_ap_status_dirty.exchange(false)) RebuildVisibleList();
		else this->SetDirty(); /* redraw current values without full rebuild */
	}

	void SetFilterButton(const std::string &f) {
		filter = f;
		show_tasks = (f == "tasks");
		/* Push/depress buttons visually */
		this->SetWidgetLoweredState(WAPM_FILTER_ALL,     f == "all");
		this->SetWidgetLoweredState(WAPM_FILTER_EASY,    f == "easy");
		this->SetWidgetLoweredState(WAPM_FILTER_MEDIUM,  f == "medium");
		this->SetWidgetLoweredState(WAPM_FILTER_HARD,    f == "hard");
		this->SetWidgetLoweredState(WAPM_FILTER_EXTREME, f == "extreme");
		this->SetWidgetLoweredState(WAPM_FILTER_TASKS,   f == "tasks");
		/* Reset scrollbar to top so stale position can't fire wrong links */
		if (this->scrollbar) this->scrollbar->SetPosition(0);
		if (show_tasks) {
			/* Clear mission list so stale visible_missions can never be used */
			visible_missions.clear();
			RebuildTaskList();
		} else {
			/* Clear task cache so stale cached_tasks can never be used */
			cached_tasks.clear();
			RebuildVisibleList();
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPM_FILTER_ALL:     SetFilterButton("all");     break;
			case WAPM_FILTER_EASY:    SetFilterButton("easy");    break;
			case WAPM_FILTER_MEDIUM:  SetFilterButton("medium");  break;
			case WAPM_FILTER_HARD:    SetFilterButton("hard");    break;
			case WAPM_FILTER_EXTREME: SetFilterButton("extreme"); break;
			case WAPM_FILTER_TASKS:   SetFilterButton("tasks");   break;

			case WAPM_LIST: {
				int rh = GetCharacterHeight(FS_NORMAL) + 3;
				const Rect &r = this->GetWidget<NWidgetBase>(WAPM_LIST)->GetCurrentRect();

				if (show_tasks) {
					/* Tasks: 4 rows per task. Row 0 = header — never navigate from there.
					 * C++ divides (-1)/4 = 0 (truncates toward zero), so we must explicitly
					 * guard abs_row <= 0 before computing task_idx to avoid the header row
					 * accidentally mapping to task[0]. */
					int abs_row = this->scrollbar->GetPosition() + (pt.y - r.top - 2) / rh;
					if (abs_row <= 0) break;  /* header row or above visible area */
					int task_idx = (abs_row - 1) / 4;
					int sub_row  = (abs_row - 1) % 4;
					if (task_idx < 0 || task_idx >= (int)cached_tasks.size()) break;
					/* Only rows 0 and 1 (action line + location line) are clickable.
					 * Rows 2 (stats) and 3 (separator) are not. */
					if (sub_row >= 2) break;
					const APTask &t = cached_tasks[task_idx];
					if (t.entity_tile != UINT32_MAX) {
						ScrollMainWindowToTile(TileIndex{t.entity_tile});
					}
				} else {
					/* Missions: 1 row per mission */
					int row = this->scrollbar->GetPosition() + (pt.y - r.top - 2) / rh;
					if (row < 0 || row >= (int)visible_missions.size()) break;
					const APMission *m = visible_missions[row];
					if (m->named_entity.tile != UINT32_MAX) {
						ScrollMainWindowToTile(TileIndex{m->named_entity.tile});
					}
				}
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (widget != WAPM_LIST) return;

		int rh = GetCharacterHeight(FS_NORMAL) + 3;
		int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;

		if (show_tasks) {
			/* ── Tasks tab — multi-line card layout ──
			 * Layout per task (3 content lines; separator between tasks):
			 *   Line A: [STATUS] DIFFICULTY  Action description
			 *   Line B:    → Entity / location name  (TC_WHITE entity)
			 *   Line C:      Progress  •  Deadline  •  Reward
			 * Between tasks: 1 separator row.
			 * Row order: header(1) + task0(3) + sep(1)+task1(3) + sep(1)+task2(3) ...
			 */
			int first_row = this->scrollbar->GetPosition();
			int row_idx   = 0;

			/* Helper: y-coordinate for current row; -1 = out of view */
			auto row_y = [&]() -> int {
				int screen_row = row_idx - first_row;
				if (screen_row < 0) return -1;
				int y = r.top + 2 + screen_row * rh;
				return (y <= r.bottom) ? y : -1;
			};
			auto advance = [&]() { row_idx++; };

			/* ── Header row ── */
			{
				int y = row_y();
				if (y >= 0) {
					int total  = AP_GetTotalMissionsCompleted();
					int checks = AP_GetTaskChecksCompleted();
					std::string hdr = fmt::format("Task Mission Checks: {} (Total shop counter: {})", checks, total);
					DrawString(r.left + 4 + x_off, r.right + max_line_px, y, hdr, TC_GOLD);
				}
				advance();
			}

			for (int i = 0; i < (int)cached_tasks.size(); i++) {
				const APTask &t = cached_tasks[i];

				/* Difficulty colour */
				TextColour tc;
				if      (t.completed)              tc = TC_DARK_GREEN;
				else if (t.expired)                tc = TC_GREY;
				else if (t.difficulty == "easy")   tc = TC_GREEN;
				else if (t.difficulty == "medium") tc = TC_YELLOW;
				else if (t.difficulty == "hard")   tc = TC_ORANGE;
				else                               tc = TC_WHITE;

				/* ── Separator before tasks 1..N ── */
				if (i > 0) {
					int y = row_y();
					if (y >= 0) {
						GfxFillRect(r.left + 4, r.right - 4, y + rh / 2, y + rh / 2 + 1, PC_DARK_GREY);
					}
					advance();
				}

				/* ── Line A: [STATUS] DIFFICULTY  Action ── */
				{
					int y = row_y();
					if (y >= 0) {
						std::string badge;
						if      (t.completed) badge = "[DONE] ";
						else if (t.expired)   badge = "[EXPIRED] ";
						else                  badge = "[ ] ";

						std::string diff_tag;
						if      (t.difficulty == "easy")   diff_tag = "EASY";
						else if (t.difficulty == "medium") diff_tag = "MEDIUM";
						else if (t.difficulty == "hard")   diff_tag = "HARD";

						/* Action = description up to " from " or " to " */
						std::string action = t.description;
						/* Strip "[Reward..." suffix first */
						for (const char *tag : {" [Reward", " [REWARD"}) {
							size_t p = action.find(tag);
							if (p != std::string::npos) { action = action.substr(0, p); break; }
						}
						/* Strip entity suffix */
						for (const char *sep : {" from ", " to "}) {
							size_t p = action.find(sep);
							if (p != std::string::npos) { action = action.substr(0, p); break; }
						}

						std::string line_a = badge + diff_tag + "  " + action;
						DrawString(r.left + 4 + x_off, r.right + max_line_px, y, line_a, tc, SA_LEFT | SA_FORCE);
					}
					advance();
				}

				/* ── Line B: entity / location ── */
				{
					int y = row_y();
					if (y >= 0) {
						/* Find the part of description after " from " or " to " */
						std::string remainder;
						const char *used_sep = nullptr;
						for (const char *sep : {" from ", " to "}) {
							size_t p = t.description.find(sep);
							if (p != std::string::npos) {
								remainder = t.description.substr(p + strlen(sep));
								used_sep  = sep;
								/* Strip any trailing "[Reward..." */
								for (const char *tag : {" [Reward", " [REWARD"}) {
									size_t rp = remainder.find(tag);
									if (rp != std::string::npos) { remainder = remainder.substr(0, rp); break; }
								}
								break;
							}
						}

						std::string prefix_str = (used_sep != nullptr && strstr(used_sep, "from"))
						                       ? "    \xe2\x86\x92 from " : "    \xe2\x86\x92 to ";

						if (!remainder.empty() && !t.entity_name.empty()) {
							/* Draw prefix, then entity in TC_WHITE, then rest (e.g. "near X") */
							int x2 = DrawString(r.left + 4 + x_off, r.right + max_line_px, y, prefix_str, tc, SA_LEFT | SA_FORCE);
							size_t en_pos = remainder.find(t.entity_name);
							if (en_pos == 0) {
								int x3 = DrawString(x2, r.right + max_line_px, y, t.entity_name, TC_WHITE, SA_LEFT | SA_FORCE);
								std::string tail = remainder.substr(t.entity_name.size());
								if (!tail.empty()) DrawString(x3, r.right + max_line_px, y, tail, tc, SA_LEFT | SA_FORCE);
							} else {
								DrawString(x2, r.right + max_line_px, y, remainder, TC_WHITE, SA_LEFT | SA_FORCE);
							}
						} else {
							std::string fallback = prefix_str + (t.entity_name.empty() ? "(any)" : t.entity_name);
							DrawString(r.left + 4 + x_off, r.right + max_line_px, y, fallback, tc, SA_LEFT | SA_FORCE);
						}
					}
					advance();
				}

				/* ── Line C: progress  •  deadline  •  reward ── */
				{
					int y = row_y();
					if (y >= 0) {
						std::string line_c = "      ";
						if (t.completed) {
							line_c += "Completed!";
						} else if (t.expired) {
							line_c += "Expired";
						} else {
							int64_t pct = (t.amount > 0)
							    ? std::min<int64_t>(100LL, t.current_value * 100LL / t.amount) : 0;
							/* Unit suffix: passengers have no unit, freight shows "t" */
							std::string unit_str;
							if (IsValidCargoType((CargoType)t.cargo)) {
								const CargoSpec *cs = CargoSpec::Get((CargoType)t.cargo);
								if (cs && cs->classes.Test(CargoClass::Passengers)) {
									unit_str = ""; /* passengers: no unit suffix */
								} else {
									unit_str = " t"; /* freight */
								}
							}
							line_c += fmt::format("{} / {}{}  ({}%)", t.current_value, t.amount, unit_str, pct);
							line_c += fmt::format("   -   By {}", t.deadline_year);
							if (t.reward_type == APTaskRewardType::MISSION_CHECK) {
								line_c += "   -   +[Mission Check]";
							} else {
								line_c += fmt::format("   -   +{}", AP_FormatMoneyCompact(t.reward_cash));
							}
						}
						DrawString(r.left + 4 + x_off, r.right + max_line_px, y, line_c, TC_GREY, SA_LEFT | SA_FORCE);
					}
					advance();
				}
			}
			return;
		}

		/* ── Missions tab (original logic) ── */
		{
		/* Recompute row height at draw time — GetCharacterHeight returns the
		 * correct scaled value for the current UI zoom level.  Using the cached
		 * constructor value causes rows to overlap at UI scale >=2. */
		int rh = GetCharacterHeight(FS_NORMAL) + 3;

		int y = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / rh) + 1;

		for (int i = first; i < last && i < (int)visible_missions.size(); i++) {
			const APMission *m = visible_missions[i];

			/* Tier lock check */
			bool tier_locked = !AP_IsTierUnlocked(m->difficulty);

			/* Difficulty colour */
			TextColour tc = TC_GREY;
			if (tier_locked) {
				tc = TC_GREY; /* locked tier always grey */
			} else if (m->difficulty == "easy")    tc = TC_GREEN;
			else if (m->difficulty == "medium")  tc = TC_YELLOW;
			else if (m->difficulty == "hard")    tc = TC_ORANGE;
			else if (m->difficulty == "extreme") tc = TC_WHITE;

			if (m->completed) {
				tc = TC_DARK_GREEN; /* completed missions always dark green */
			}
			/* Named missions keep their difficulty colour — the map-pin icon in
			 * the description is the interactivity hint, not the text colour.
			 * Previously all named missions were forced TC_WHITE, which made them
			 * look identical to extreme missions and confused players. */

			/* Format: [X] Easy #042 - Description  (current/target) */
			/* Locked tiers: show lock indicator and requirement instead of live progress */
			if (tier_locked) {
				std::string cap_diff2 = m->difficulty.empty() ? "" :
					std::string(1, (char)toupper((unsigned char)m->difficulty[0])) + m->difficulty.substr(1);
				/* Determine which previous tier is required */
				std::string prev_tier = (m->difficulty == "medium") ? "easy"
				                       : (m->difficulty == "hard")   ? "medium"
				                                                      : "hard";
				int needed    = AP_GetTierThreshold(m->difficulty);
				int done      = AP_GetTierCompleted(prev_tier);
				int remaining = std::max(0, needed - done);
				std::string lock_line = fmt::format("[LOCKED] {} - Complete {} more {} mission{} to unlock",
				    cap_diff2, remaining, prev_tier, remaining == 1 ? "" : "s");
				int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;
				DrawString(r.left + 4 + x_off, r.right, y, lock_line, TC_GREY);
				y += rh;
				if (y > r.bottom) break;
				continue;
			}
			std::string prefix = m->completed ? "[X] " : "[ ] ";
			std::string cap_diff = m->difficulty.empty() ? "" :
				std::string(1, (char)toupper((unsigned char)m->difficulty[0])) + m->difficulty.substr(1);
			/* Extract mission number from location string (e.g. "Mission_Easy_042" -> "#042") */
			std::string mission_num;
			{
				const std::string &loc = m->location;
				auto last_us = loc.rfind('_');
				if (last_us != std::string::npos && last_us + 1 < loc.size()) {
					mission_num = " #" + loc.substr(last_us + 1);
				}
			}

			/* Build progress string for incomplete missions.
			 * Always show for all incomplete missions so players see the tracker is live.
			 * Format: " [Status: X]" — target is already in the mission text. */
			bool is_named    = (m->type == "passengers_to_town" || m->type == "mail_to_town" ||
			                    m->type == "cargo_from_industry" || m->type == "cargo_to_industry");
			bool is_maintain = (m->type == "maintain_75" ||
			                    m->type.find("maintain") != std::string::npos);
			std::string progress_str;
			if (!m->completed && m->amount > 0) {
				if (is_maintain) {
					/* Keep months format — target is meaningful here */
					progress_str = fmt::format("  ({}/{} months)", m->current_value, m->amount);
				} else {
					bool is_money = (m->unit == "\xC2\xA3" || m->unit == "\xC2\xA3/month" ||
					                 m->unit == "£" || m->unit == "£/month" ||
					                 m->unit.find("/month") != std::string::npos);
					if (is_money) {
						/* Clamp displayed value to 0 for money missions */
						int64_t display_val = std::max<int64_t>(0, m->current_value);
						progress_str = fmt::format("  [Status: {}]",
						    AP_FormatMoneyCompact(display_val));
					} else {
						auto fmt_num = [](int64_t v) -> std::string {
							if (v < 0) v = 0;
							if (v >= 1000000) return fmt::format("{:.1f}M", v / 1000000.0);
							if (v >= 1000)    return fmt::format("{}k", v / 1000);
							return fmt::format("{}", v);
						};
						progress_str = fmt::format("  [Status: {}]", fmt_num(m->current_value));
					}
				}
			}

			/* Bug 2 fix: replace hardcoded £ in description with the current
			 * game currency prefix so the mission text matches the player's
			 * chosen currency (e.g. USD shows $ instead of £). */
			std::string desc = m->description;
			{
				const CurrencySpec &cs = GetCurrency();
				std::string pound_utf8 = "\xC2\xA3"; /* UTF-8 encoding of £ */
				std::string replacement = cs.prefix.empty() ? std::string(cs.suffix) : std::string(cs.prefix);
				/* Only replace if the game isn't using GBP (prefix "£") */
				if (replacement != pound_utf8 && replacement != "\xC2\xA3") {
					size_t pos = 0;
					while ((pos = desc.find(pound_utf8, pos)) != std::string::npos) {
						desc.replace(pos, pound_utf8.size(), replacement);
						pos += replacement.size();
					}
				}
			}

			/* For named missions (town/industry assigned), append a map-pin symbol
			 * to hint that clicking this row will scroll the viewport there. */
			std::string nav_hint;
			if (m->named_entity.tile != UINT32_MAX) {
				nav_hint = " \xe2\x86\x91"; /* ↑ unicode arrow — visual cue to scroll map */
			}
			std::string line = prefix + cap_diff + mission_num + " - " + desc + progress_str + nav_hint;

			int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;
			int x = r.left + 4 + x_off;
			int right_edge = r.right + max_line_px;

			/* Named missions: draw the entity name (town/industry) in TC_WHITE so
			 * players know it is a clickable map link, distinct from the green text. */
			if (is_named && !m->named_entity.name.empty() && !m->completed) {
				const std::string &ename = m->named_entity.name;
				const std::string full_line = line;
				size_t name_pos = full_line.find(ename);
				if (name_pos != std::string::npos) {
					std::string before = full_line.substr(0, name_pos);
					std::string after  = full_line.substr(name_pos + ename.size());
					/* Draw prefix segment */
					int x2 = DrawString(x, right_edge, y, before, tc, SA_LEFT | SA_FORCE);
					/* Draw entity name in white */
					int x3 = DrawString(x2, right_edge, y, ename, TC_WHITE, SA_LEFT | SA_FORCE);
					/* Draw remainder in normal mission color */
					DrawString(x3, right_edge, y, after, tc, SA_LEFT | SA_FORCE);
				} else {
					DrawString(x, right_edge, y, line, tc, SA_LEFT | SA_FORCE);
				}
			} else {
				DrawString(x, right_edge, y, line, tc, SA_LEFT | SA_FORCE);
			}
			y += rh;
			if (y > r.bottom) break;
		}
		} /* end missions block */
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override {
		UpdateHScrollbar();
		this->SetDirty();
	}

	void OnResize() override {
		if (this->scrollbar) {
			int rh = GetCharacterHeight(FS_NORMAL) + 3;
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPM_LIST)->current_y / rh);
		}
		UpdateHScrollbar();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPM_LIST) {
			resize.height = row_height;
			resize.width = 1;
			size.height = std::max(size.height, (uint)(row_height * 15));
		}
	}
};

static WindowDesc _ap_missions_desc(
	WDP_AUTO, {"ap_missions"}, 480, 340,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_missions_widgets
);

void ShowArchipelagoMissionsWindow()
{
	AllocateWindowDescFront<ArchipelagoMissionsWindow>(_ap_missions_desc, 2);
}
/* =========================================================================
 * SHOP WINDOW
 * Shows AP_GetShopSlots() random shop items. Player clicks to purchase —
 * this sends a LocationChecks packet for that shop slot.
 * ========================================================================= */

enum APShopWidgets : WidgetID {
	WAPSH_LIST,
	WAPSH_SCROLLBAR,
	WAPSH_HSCROLLBAR,
	WAPSH_BTN_BUY,
	WAPSH_BTN_CLOSE,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_shop_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_CREAM),
		NWidget(WWT_CAPTION, COLOUR_CREAM), SetStringTip(STR_ARCHIPELAGO_SHOP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_CREAM), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_CREAM, WAPSH_LIST), SetMinimalSize(380, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPSH_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_CREAM, WAPSH_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPIP(4, 4, 4), SetPadding(4),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPSH_BTN_BUY),   SetStringTip(STR_ARCHIPELAGO_SHOP_BUY,   STR_EMPTY), SetMinimalSize(120, 16), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,  WAPSH_BTN_CLOSE), SetStringTip(STR_ARCHIPELAGO_BTN_CLOSE, STR_EMPTY), SetMinimalSize(80,  16),
		EndContainer(),
		/* Horizontal scrollbar + resize box */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HSCROLLBAR, COLOUR_CREAM, WAPSH_HSCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_CREAM),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoShopWindow : public Window {
	int row_height  = 0;      /* computed in constructor from font height */
	int selected    = -1;
	int max_line_px = 0;      /* pixel width of longest shop label */
	bool hints_all_loaded = false; /* true once no entries contain "loading..." */
	Scrollbar *scrollbar  = nullptr;
	Scrollbar *hscrollbar = nullptr;

	/* Shop items: (location_name, label, price) */
	struct ShopEntry {
		std::string location_name;
		std::string label;        ///< item name (hidden when locked)
		int64_t     price;        ///< item price (hidden when locked)
		bool        locked;       ///< true when shop tier not yet unlocked
		int         missions_needed; ///< total missions required to unlock
	};
	std::vector<ShopEntry> shop_items;

	void RebuildShopList()
	{
		shop_items.clear();
		int total = AP_GetShopSlots();

		/* Collect all unsold items with their labels and prices. */
		struct RawEntry { std::string loc; std::string label; int64_t price; };
		std::vector<RawEntry> raw;
		for (int i = 1; i <= total; i++) {
			std::string loc = fmt::format("Shop_Purchase_{:04d}", i);
			if (AP_IsShopLocationSent(loc)) continue;
			std::string label = AP_GetShopLocationLabel(loc);
			if (label.empty()) label = fmt::format("Item #{} (loading...)", i);
			raw.push_back({loc, label, AP_GetShopPrice(loc)});
		}
		/* Sort ascending by price — cheapest first. */
		std::sort(raw.begin(), raw.end(),
		    [](const RawEntry &a, const RawEntry &b) { return a.price < b.price; });

		/* Assign lock state based on sorted position. */
		for (int idx = 0; idx < (int)raw.size(); idx++) {
			bool locked  = !AP_IsShopSlotUnlocked(idx);
			int  needed  = AP_GetShopSlotRequiredMissions(idx);
			shop_items.push_back({raw[idx].loc, raw[idx].label, raw[idx].price, locked, needed});
		}

		if (this->scrollbar) this->scrollbar->SetCount((int)shop_items.size());
		max_line_px = 0;
		for (int idx = 0; idx < (int)shop_items.size(); idx++) {
			const auto &e = shop_items[idx];
			std::string full = e.locked
			    ? fmt::format("[{}] [LOCKED] Complete {} missions to unlock", idx + 1, e.missions_needed)
			    : fmt::format("[{}] {} \xe2\x80\x94 {}", idx + 1, e.label, AP_FormatMoneyCompact(e.price));
			int w = GetStringBoundingBox(full).width;
			if (w > max_line_px) max_line_px = w;
		}
		UpdateHScrollbar();
		this->SetDirty();
	}

	void UpdateHScrollbar() {
		if (!this->hscrollbar) return;
		NWidgetBase *nw = this->GetWidget<NWidgetBase>(WAPSH_LIST);
		int visible_w = (nw != nullptr) ? (int)nw->current_x - 8 : 380;
		int total     = std::max(max_line_px + 16, visible_w);
		this->hscrollbar->SetCount(total);
		this->hscrollbar->SetCapacity(visible_w);
	}

	ArchipelagoShopWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3; /* +3px vertical padding */
		this->CreateNestedTree();
		this->scrollbar  = this->GetScrollbar(WAPSH_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->hscrollbar = this->GetScrollbar(WAPSH_HSCROLLBAR);
		this->hscrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		this->resize.step_width  = 1;
		RebuildShopList();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		/* Once all hints are resolved, stop polling */
		if (hints_all_loaded) return;

		/* Refresh labels while any entry is still pending */
		bool any_loading = false;
		for (auto &entry : shop_items) {
			if (entry.label.find("loading") != std::string::npos) {
				any_loading = true;
				break;
			}
		}
		if (any_loading) {
			RebuildShopList();
		} else {
			hints_all_loaded = true;
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		/* Called when the shop rotates (SetWindowClassesDirty) or any other
		 * invalidation — rebuild the list immediately so the new page is
		 * visible without waiting for the next OnRealtimeTick poll. */
		RebuildShopList();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPSH_LIST) return;
		int y     = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / row_height) + 1;
		for (int i = first; i < last && i < (int)shop_items.size(); i++) {
			const ShopEntry &e = shop_items[i];
			TextColour tc;
			if (e.locked) {
				tc = TC_GREY;
			} else if (i == selected) {
				tc = TC_WHITE;
			} else if (AP_CanAffordShopItem(e.location_name)) {
				tc = TC_YELLOW;
			} else {
				tc = TC_GREY;
			}
			std::string line;
			if (e.locked) {
				line = fmt::format("[{}] [LOCKED] Complete {} missions to unlock",
				    i + 1, e.missions_needed);
			} else {
				line = fmt::format("[{}] {} — {}",
				    i + 1, e.label, AP_FormatMoneyCompact(e.price));
			}
			int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;
			DrawString(r.left + 4 + x_off, r.right + max_line_px, y, line, tc, SA_LEFT | SA_FORCE);
			y += row_height;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		switch (widget) {
			case WAPSH_LIST: {
				NWidgetBase *nw = this->GetWidget<NWidgetBase>(WAPSH_LIST);
				int row = this->scrollbar->GetPosition() + (pt.y - nw->pos_y) / row_height;
				if (row >= 0 && row < (int)shop_items.size()) selected = row;
				this->SetDirty();
				break;
			}
			case WAPSH_BTN_BUY:
				if (selected >= 0 && selected < (int)shop_items.size()) {
					const ShopEntry &entry = shop_items[selected];
					if (entry.locked) {
						AP_ShowConsole(fmt::format("[AP] Shop: locked — complete {} missions first",
						    entry.missions_needed));
						break;
					}
					if (!AP_CanAffordShopItem(entry.location_name)) {
						AP_ShowConsole(fmt::format("[AP] Shop: cannot afford {} — need {}",
						    entry.label, AP_FormatMoneyCompact(entry.price)));
						break;
					}
					AP_DeductShopPrice(entry.location_name);
					AP_SendCheckByName(entry.location_name);
					AP_NotifyShopPurchased();
					AP_ShowConsole(fmt::format("[AP] Shop: bought item for {}", entry.label));
					shop_items.erase(shop_items.begin() + selected);
					selected = -1;
					if (this->scrollbar) this->scrollbar->SetCount((int)shop_items.size());
					this->SetDirty();
				}
				break;
			case WAPSH_BTN_CLOSE:
				this->Close();
				break;
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override {
		UpdateHScrollbar();
		this->SetDirty();
	}

	void OnResize() override
	{
		if (this->scrollbar) {
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPSH_LIST)->current_y / row_height);
		}
		UpdateHScrollbar();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPSH_LIST) {
			resize.height = row_height;
			resize.width = 1;
			size.height   = std::max(size.height, (uint)(row_height * 8));
		}
	}
};

static WindowDesc _ap_shop_desc(
	WDP_AUTO, {"ap_shop"}, 400, 280,
	WC_NONE, WC_NONE, {},
	_nested_ap_shop_widgets
);

void ShowArchipelagoShopWindow()
{
	AllocateWindowDescFront<ArchipelagoShopWindow>(_ap_shop_desc, 3);
}



/* =========================================================================
 * Archipelago Guide Window
 * Reference for AP server commands, hotkeys and gameplay tips.
 * ========================================================================= */

enum ArchipelagoGuideWidgets {
	WAPGD_CAPTION,
	WAPGD_TAB_COMMANDS,
	WAPGD_TAB_HOTKEYS,
	WAPGD_TAB_TIPS,
	WAPGD_PANEL,
	WAPGD_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_guide_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_BLUE),
		NWidget(WWT_CAPTION, COLOUR_DARK_BLUE, WAPGD_CAPTION), SetStringTip(STR_ARCHIPELAGO_GUIDE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	/* Tab buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPGD_TAB_COMMANDS), SetStringTip(STR_ARCHIPELAGO_GUIDE_TAB_COMMANDS), SetMinimalSize(110, 16), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPGD_TAB_HOTKEYS),  SetStringTip(STR_ARCHIPELAGO_GUIDE_TAB_HOTKEYS),  SetMinimalSize(110, 16), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPGD_TAB_TIPS),     SetStringTip(STR_ARCHIPELAGO_GUIDE_TAB_TIPS),     SetMinimalSize(110, 16), SetFill(1, 0),
	EndContainer(),
	/* Content panel + scrollbar */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_BLUE, WAPGD_PANEL), SetMinimalSize(330, 300), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPGD_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_BLUE, WAPGD_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_RESIZEBOX, COLOUR_DARK_BLUE),
};

struct ArchipelagoGuideWindow : Window {
	int active_tab = 0;  ///< 0=commands 1=hotkeys 2=tips

	struct GuideLine {
		bool header;
		std::string text;
	};
	std::vector<GuideLine> lines;

	ArchipelagoGuideWindow(WindowDesc &desc, WindowNumber wn) : Window(desc)
	{
		this->InitNested(wn);
		this->vscroll = this->GetScrollbar(WAPGD_SCROLLBAR);
		this->_BuildLines();
	}

	int _LineH() const { return GetCharacterHeight(FS_NORMAL) + 3; }

	Scrollbar *vscroll = nullptr;

	void _BuildLines()
	{
		lines.clear();
		if (active_tab == 0) _BuildCommandLines();
		else if (active_tab == 1) _BuildHotkeyLines();
		else _BuildTipLines();
		if (vscroll) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPGD_PANEL);
			int visible = (panel ? panel->current_y : 300) / _LineH();
			vscroll->SetCount((int)lines.size());
			vscroll->SetCapacity(visible);
		}
	}

	void _H(const std::string &t) { lines.push_back({true,  t}); }
	void _L(const std::string &t) { lines.push_back({false, t}); }

	void _BuildCommandLines()
	{
		_H("=== Archipelago Server Commands ===");
		_L("");
		_L("Type these in the OpenTTD console (` key) using:");
		_L("  ap <command>    e.g.  ap !hint Wills 2-8-0");
		_L("");
		_H("--- Information ---");
		_L("!hint <item>      - Request a hint for an item");
		_L("!remaining        - Show how many checks remain");
		_L("!missing          - List unchecked locations");
		_L("!checked          - List completed locations");
		_L("!status           - Show connection & game status");
		_L("!players          - List connected players");
		_L("!countdown <sec>  - Start a countdown timer");
		_L("");
		_H("--- Cheats (require server permission) ---");
		_L("!getitem <name>   - Force-receive an item");
		_L("!collect          - Auto-collect all your items");
		_L("!release          - Release your remaining items");
		_L("                    to other players when you finish");
		_L("");
		_L("Note: Cheat commands must be enabled when the");
		_L("Archipelago server is generated (allow_cheats).");
		_L("");
		_H("--- Console shortcut ---");
		_L("ap !hint item     (sends '!hint item' to AP)");
		_L("ap !remaining     (sends '!remaining' to AP)");
	}

	void _BuildHotkeyLines()
	{
		_H("=== OpenTTD Essential Hotkeys ===");
		_L("");
		_H("--- General ---");
		_L("F1 / Pause        - Pause / unpause game");
		_L("Tab (hold)        - Fast forward (release to resume)");
		_L("Space             - Close error messages / news");
		_L("Delete            - Close all non-sticky windows");
		_L("~ / `             - Open console");
		_L("F3                - Save game");
		_L("Ctrl+S            - Take screenshot");
		_L("");
		_H("--- Map & View ---");
		_L("M                 - Toggle minimap");
		_L("Numpad +/-        - Zoom in / out");
		_L("Arrow keys        - Scroll map");
		_L("Shift+Arrow       - Scroll map faster");
		_L("Z                 - Zoom to mouse pointer");
		_L("C                 - Center on mouse pointer");
		_L("");
		_H("--- Building (Rail) ---");
		_L("A                 - Toggle autorail");
		_L("X                 - Toggle all transparency");
		_L("Ctrl+X            - Open transparency options");
		_L("R                 - Remove tool (while building)");
		_L("Ctrl (hold)       - Drag to build straight tracks");
		_L("Ctrl+Station      - Join/extend existing station");
		_L("");
		_H("--- Station Placement ---");
		_L("Ctrl (hold)       - Show catchment area");
		_L("Ctrl+drag         - Extend station coverage");
		_L("[ / ]             - Rotate station layout");
		_L("");
		_H("--- Vehicles ---");
		_L("Ctrl+Clone        - Clone with shared orders");
		_L("Ctrl+Start        - Start all vehicles in depot");
		_L("G (in depot)      - Go to nearest depot");
		_L("");
		_H("--- Orders ---");
		_L("Ctrl+Order        - Copy orders from vehicle");
		_L("D (in orders)     - Skip to next order now");
		_L("Non-stop (order)  - Hold Ctrl to toggle non-stop");
	}

	void _BuildTipLines()
	{
		_H("=== Gameplay Tips for Archipelago ===");
		_L("");
		_H("--- Airports & Aircraft ---");
		_L("NEVER send large aircraft (Darwin 300+, Dinger");
		_L("200+, FFP Hyperdart) to a small airport.");
		_L("They crash immediately on landing!");
		_L("Small aircraft: Sampson, Coleman, Bakewell");
		_L("Cotswald, Kelling K1, AirTaxi series.");
		_L("Large airports needed for: Darwin 300, 400+,");
		_L("Dinger 200, 1000, FFP Hyperdart, Juggerplane.");
		_L("");
		_H("--- Starting Strategy ---");
		_L("Start with road vehicles — they need no");
		_L("infrastructure and are fast to set up.");
		_L("Use buses between towns to earn early cash.");
		_L("Build rail when you have steady income.");
		_L("Keep a loan buffer — traps can drain money.");
		_L("");
		_H("--- Archipelago Items & Traps ---");
		_L("Breakdown Wave   - All vehicles break down");
		_L("Recession        - Income cut for a period");
		_L("Maintenance Surge- Running costs spike");
		_L("Signal Failure   - All signals go red");
		_L("Fuel Shortage    - Vehicles slow down");
		_L("Bank Loan Forced - Extra loan forced on you");
		_L("Industry Closure - A random industry closes");
		_L("");
		_H("--- Utility Items (positive) ---");
		_L("Cash Injection   - Free money!");
		_L("Loan Reduction   - Loan shrinks automatically");
		_L("Cargo Bonus      - 2x cargo payment, 60 days");
		_L("Reliability Boost- All vehicles more reliable");
		_L("Town Growth Boost- Towns grow faster");
		_L("Free Station     - Build a free station");
		_L("");
		_H("--- Missions ---");
		_L("Easy missions:   Good starting points.");
		_L("Hard/Extreme:    High rewards, worth doing");
		_L("                 before you run out of checks.");
		_L("Station Rating:  Keep a vehicle visiting the");
		_L("                 station regularly — rating drops");
		_L("                 if no vehicle visits for a month.");
		_L("");
		_H("--- Performance Tips ---");
		_L("Fast forward starts locked at 100% (normal speed).");
		_L("Each Speed Boost item you receive adds +10%,");
		_L("up to a max of 300%. Unlock FF by finding");
		_L("Speed Boosts in missions or shops.");
		_L("Large maps with many trains can slow the game.");
		_L("128x128 to 256x256 is ideal for AP games.");
	}

	void OnPaint() override
	{
		/* Highlight active tab */
		this->SetWidgetLoweredState(WAPGD_TAB_COMMANDS, active_tab == 0);
		this->SetWidgetLoweredState(WAPGD_TAB_HOTKEYS,  active_tab == 1);
		this->SetWidgetLoweredState(WAPGD_TAB_TIPS,     active_tab == 2);
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPGD_PANEL) return;

		const int LH = _LineH();
		int y = r.top + 3;
		int x = r.left + 4;
		int max_y = r.bottom - 2;
		int start = vscroll ? vscroll->GetPosition() : 0;
		int visible_lines = (r.bottom - r.top) / LH + 1;

		for (int i = start; i < (int)lines.size() && i < start + visible_lines; i++) {
			if (y + LH > max_y) break;
			const auto &line = lines[i];
			if (line.header) {
				DrawString(x, r.right - 4, y, line.text, TC_GOLD);
			} else {
				DrawString(x + 4, r.right - 4, y, line.text, TC_WHITE);
			}
			y += LH;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		switch (widget) {
			case WAPGD_TAB_COMMANDS: active_tab = 0; _BuildLines(); this->SetDirty(); break;
			case WAPGD_TAB_HOTKEYS:  active_tab = 1; _BuildLines(); this->SetDirty(); break;
			case WAPGD_TAB_TIPS:     active_tab = 2; _BuildLines(); this->SetDirty(); break;
		}
	}

	void OnResize() override
	{
		if (vscroll) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPGD_PANEL);
			int visible = (panel ? panel->current_y : 300) / _LineH();
			vscroll->SetCapacity(visible);
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }
};

static WindowDesc _ap_guide_desc(
	WDP_AUTO, {"ap_guide"}, 340, 340,
	WC_ARCHIPELAGO_GUIDE, WC_NONE, {},
	_nested_ap_guide_widgets
);

void ShowArchipelagoGuideWindow()
{
	AllocateWindowDescFront<ArchipelagoGuideWindow>(_ap_guide_desc, 0);
}

/* =========================================================================
 * COLBY EVENT WINDOW
 * Shows current step progress, delivered amount, and town name.
 * ========================================================================= */

enum APColbyWidgets : WidgetID {
	WAPCB_CAPTION,
	WAPCB_PANEL,
	WAPCB_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_colby_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WAPCB_CAPTION), SetStringTip(STR_ARCHIPELAGO_COLBY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WAPCB_PANEL), SetMinimalSize(320, 100), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPCB_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPCB_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
};

struct ArchipelagoColbyWindow : public Window {
	Scrollbar *vscroll = nullptr;

	enum LinkID { LINK_NONE = -1, LINK_TOWN = 0, LINK_STASH = 1 };

	struct EventLine {
		std::string text;
		TextColour   col;
		bool         is_bar   = false; ///< render as a progress bar
		int          bar_fill = 0;     ///< 0..1000 (per-mille fill)
		LinkID       link     = LINK_NONE; ///< clickable link type
	};
	std::vector<EventLine> _lines;

	/* Cached link targets so OnClick can use them */
	TownID    _link_town_id    = (TownID)UINT16_MAX;
	TileIndex _link_stash_tile = INVALID_TILE;

	ArchipelagoColbyWindow(WindowDesc &desc, WindowNumber wn) : Window(desc)
	{
		this->InitNested(wn);
		this->vscroll = this->GetScrollbar(WAPCB_SCROLLBAR);
		this->_RebuildLines();
	}

	int _LineH() const { return GetCharacterHeight(FS_NORMAL) + 3; }

	void _RebuildLines()
	{
		_lines.clear();

		auto add = [&](const std::string &t, TextColour c) {
			_lines.push_back({t, c, false, 0});
		};
		auto bar = [&](int fill_permille) {
			_lines.push_back({"", TC_WHITE, true, fill_permille});
		};

		if (!(_ap_client && _ap_client->HasSlotData()) || !AP_GetSlotData().colby_event) {
			add("No active events.", TC_GREY);
		} else {
			ColbyStatus cs = AP_GetColbyStatus();

			if (!cs.enabled) {
				add("No active events.", TC_GREY);
			} else if (cs.step == 0) {
				/* Event is configured but hasn't triggered yet — show countdown */
				add("[ The Colby Event ]", TC_GREY);
				add("", TC_GREY);
				int start_year = AP_GetSlotData().colby_start_year;
				int cur_year   = (int)TimerGameCalendar::year.base();
				if (start_year > cur_year) {
					add(fmt::format("Event begins in {} (year {}).", start_year - cur_year == 1
					    ? "1 year" : fmt::format("{} years", start_year - cur_year), start_year), TC_GREY);
				} else {
					add("Event is about to begin...", TC_GREY);
				}
			} else {
				add("[ The Colby Event ]", TC_WHITE);
				add("", TC_WHITE);

				if (cs.done) {
					add("Event completed.", TC_GREEN);
				} else if (cs.escaped) {
					int days_left = std::max(0, cs.escape_ticks / 74);
					add(fmt::format("Colby has escaped!", days_left), TC_RED);
					add(fmt::format("Awaiting capture in approximately {} days...", days_left), TC_ORANGE);
				} else {
					/* Cache link targets */
					_link_town_id    = (TownID)cs.town_id;
					_link_stash_tile = TileIndex{cs.source_tile};

					add(fmt::format("Step {}/5", cs.step), TC_GOLD);

					/* Clickable destination town */
					if (cs.town_id != 0xFFFFU) {
						_lines.push_back({
							fmt::format("\u25ba Destination: {} (click to locate)", cs.town_name),
							TC_LIGHT_BLUE, false, 0, LINK_TOWN});
					} else {
						add(fmt::format("Destination: {}", cs.town_name), TC_WHITE);
					}

					/* Clickable stash station */
					if (cs.source_tile != 0xFFFFFFFFU && !cs.source_name.empty()) {
						_lines.push_back({
							fmt::format("\u25ba Pick up {} at: {} (click to locate)",
							            cs.cargo_name, cs.source_name),
							TC_YELLOW, false, 0, LINK_STASH});
						_lines.push_back({
							"  Load any vehicle. Use 'Unload all' in orders.",
							TC_GREY, false, 0, LINK_NONE});
					} else {
						add("Pick up packages from Colby's Stash", TC_GREY);
						add("and deliver them to the destination town.", TC_GREY);
						add("Use 'Unload all' in vehicle orders.", TC_GREY);
					}

					add("", TC_WHITE);
					add(fmt::format("{} delivered: {} / {}",
					                cs.cargo_name.empty() ? "Packages" :
					                (std::string(1, (char)std::toupper((unsigned char)cs.cargo_name[0])) + cs.cargo_name.substr(1)),
					                cs.delivered, cs.step_amount), TC_LIGHT_BLUE);
					int fill = (cs.step_amount > 0)
						? (int)((double)cs.delivered / cs.step_amount * 1000)
						: 0;
					bar(std::clamp(fill, 0, 1000));
				}
			}
		}

		/* Update scrollbar */
		if (vscroll != nullptr) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPCB_PANEL);
			int vis = (panel ? panel->current_y : 100) / _LineH();
			vscroll->SetCount((int)_lines.size());
			vscroll->SetCapacity(vis);
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		this->_RebuildLines();
		this->SetDirty();
	}

	void OnResize() override
	{
		if (vscroll != nullptr) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPCB_PANEL);
			int vis = (panel ? panel->current_y : 100) / _LineH();
			vscroll->SetCapacity(vis);
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }

	/** Convert a panel-relative y position to a line index, or -1 if out of range. */
	int _YToLine(int y_abs, const Rect &r) const
	{
		const int LH  = _LineH();
		const int PAD = 6;
		int start = vscroll ? vscroll->GetPosition() : 0;
		int idx   = start + (y_abs - r.top - PAD) / LH;
		if (idx < 0 || idx >= (int)_lines.size()) return -1;
		return idx;
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget != WAPCB_PANEL) return;
		const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPCB_PANEL);
		if (panel == nullptr) return;
		Rect r{panel->pos_x, panel->pos_y,
		       panel->pos_x + (int)panel->current_x,
		       panel->pos_y + (int)panel->current_y};
		int idx = _YToLine(pt.y, r);
		if (idx < 0) return;
		const auto &line = _lines[idx];
		if (line.link == LINK_TOWN) {
			if (_link_town_id != (TownID)UINT16_MAX) {
				const Town *t = Town::GetIfValid(_link_town_id);
				if (t != nullptr) ScrollMainWindowToTile(t->xy);
			}
		} else if (line.link == LINK_STASH) {
			if (_link_stash_tile != INVALID_TILE) {
				ScrollMainWindowToTile(_link_stash_tile);
			}
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPCB_PANEL) return;

		const int LH  = _LineH();
		const int PAD = 6;
		int y    = r.top + PAD;
		int start = vscroll ? vscroll->GetPosition() : 0;
		int vis   = vscroll ? vscroll->GetCapacity() : (int)_lines.size();

		for (int i = start; i < start + vis && i < (int)_lines.size(); i++) {
			if (y + LH > r.bottom - 2) break;
			const auto &line = _lines[i];

			if (line.is_bar) {
				/* Progress bar */
				int bw     = r.right - r.left - PAD * 2;
				int filled = bw * line.bar_fill / 1000;
				int bh     = LH - 3;
				GfxFillRect(r.left + PAD,          y, r.left + PAD + filled, y + bh, PC_GREEN);
				GfxFillRect(r.left + PAD + filled, y, r.right - PAD,         y + bh, PC_DARK_GREY);
			} else if (!line.text.empty()) {
				TextColour col = line.col;
				DrawString(r.left + PAD, r.right - PAD, y, line.text, col);
			}
			y += LH;
		}
	}
};

static WindowDesc _ap_colby_desc(
	WDP_AUTO, {"ap_colby"}, 340, 160,
	WC_ARCHIPELAGO_COLBY, WC_NONE, {},
	_nested_ap_colby_widgets
);

void ShowArchipelagoColbyWindow()
{
	AllocateWindowDescFront<ArchipelagoColbyWindow>(_ap_colby_desc, 0);
}
