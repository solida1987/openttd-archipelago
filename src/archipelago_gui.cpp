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
#include "table/control_codes.h"
#include "town.h"
#include "zoom_func.h"
#include "company_base.h"
#include "company_func.h"
#include "command_func.h"
#include "misc_cmd.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/core/config.h"
#include "string_func.h"
#include "error.h"
#include "debug.h"
#include "settings_type.h"
#include "genworld.h"
#include "fios.h"
#include "console_func.h"
#include "hotkeys.h"
#include <charconv>
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
	/* Town rename row */
	WAPGUI_LABEL_TOWNS,
	WAPGUI_BTN_TOWNS_AUTO,
	WAPGUI_BTN_TOWNS_CUSTOM,
	WAPGUI_BTN_TOWNS_OFF,
	WAPGUI_LABEL_CUSTOM_NAMES,
	WAPGUI_EDIT_CUSTOM_NAMES,
	WAPGUI_CUSTOM_HINT,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
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
			/* Town rename row */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_TOWNS), SetStringTip(STR_ARCHIPELAGO_LABEL_TOWNS), SetMinimalSize(80, 14),
				NWidget(WWT_TEXTBTN, COLOUR_DARK_GREEN, WAPGUI_BTN_TOWNS_AUTO),   SetStringTip(STR_ARCHIPELAGO_TOWNS_AUTO,   STR_ARCHIPELAGO_TOWNS_AUTO_TIP),   SetMinimalSize(62, 14),
				NWidget(WWT_TEXTBTN, COLOUR_GREY,       WAPGUI_BTN_TOWNS_CUSTOM), SetStringTip(STR_ARCHIPELAGO_TOWNS_CUSTOM, STR_ARCHIPELAGO_TOWNS_CUSTOM_TIP), SetMinimalSize(62, 14),
				NWidget(WWT_TEXTBTN, COLOUR_GREY,       WAPGUI_BTN_TOWNS_OFF),    SetStringTip(STR_ARCHIPELAGO_TOWNS_OFF,    STR_ARCHIPELAGO_TOWNS_OFF_TIP),    SetMinimalSize(62, 14),
			EndContainer(),
			/* Custom names editbox — only visible when mode == 1 */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT,    INVALID_COLOUR, WAPGUI_LABEL_CUSTOM_NAMES), SetStringTip(STR_ARCHIPELAGO_LABEL_CUSTOM_NAMES), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY,    WAPGUI_EDIT_CUSTOM_NAMES),  SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			/* Hint text — only shown when Custom mode is active */
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_CUSTOM_HINT), SetStringTip(STR_ARCHIPELAGO_CUSTOM_NAMES_HINT), SetMinimalSize(284, 20), SetFill(1, 0),
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

extern int         _ap_town_rename_mode;
extern std::string _ap_town_custom_names;

/** Format a number with locale-appropriate thousand separators */
static std::string AP_FmtNum(int64_t v) { return GetString(STR_JUST_COMMA, v); }

/** Abbreviate large numbers: 1234567 -> "1.23M", 50000000000 -> "50B" */
static std::string AP_FmtShort(int64_t v) {
	if (v < 0) return "-" + AP_FmtShort(-v);
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

struct ArchipelagoConnectWindow : public Window {
	QueryString server_buf;
	QueryString slot_buf;
	QueryString pass_buf;
	QueryString custom_names_buf;
	std::string server_str, slot_str, pass_str, custom_str;
	APState  last_state  = APState::DISCONNECTED;
	bool     last_has_sd = false;

	void UpdateTownButtons()
	{
		this->SetWidgetLoweredState(WAPGUI_BTN_TOWNS_AUTO,   _ap_town_rename_mode == 0);
		this->SetWidgetLoweredState(WAPGUI_BTN_TOWNS_CUSTOM, _ap_town_rename_mode == 1);
		this->SetWidgetLoweredState(WAPGUI_BTN_TOWNS_OFF,    _ap_town_rename_mode == 2);
		/* Show/hide the custom names editbox row and hint */
		this->SetWidgetDisabledState(WAPGUI_EDIT_CUSTOM_NAMES,  _ap_town_rename_mode != 1);
		this->SetWidgetDisabledState(WAPGUI_LABEL_CUSTOM_NAMES, _ap_town_rename_mode != 1);
		this->SetWidgetDisabledState(WAPGUI_CUSTOM_HINT,        _ap_town_rename_mode != 1);
		this->SetDirty();
	}

	static std::string WinDifficultyName(APWinDifficulty d) {
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
			default:                         return "Unknown";
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
		return "Difficulty: " + WinDifficultyName(sd.win_difficulty) +
		       "  Missions: " + fmt::format("{}", sd.missions.size()) +
		       "  Start: " + sd.starting_vehicle;
	}

	ArchipelagoConnectWindow(WindowDesc &desc, WindowNumber wnum)
		: Window(desc), server_buf(256), slot_buf(64), pass_buf(64), custom_names_buf(512)
	{
		this->CreateNestedTree();
		this->querystrings[WAPGUI_EDIT_SERVER]       = &server_buf;
		this->querystrings[WAPGUI_EDIT_SLOT]         = &slot_buf;
		this->querystrings[WAPGUI_EDIT_PASS]         = &pass_buf;
		this->querystrings[WAPGUI_EDIT_CUSTOM_NAMES] = &custom_names_buf;
		this->FinishInitNested(wnum);

		/* Restore last connection settings from ap_connection.cfg */
		AP_LoadConnectionConfig();

		std::string full = _ap_last_host.empty() ? "archipelago.gg:38281"
		                 : _ap_last_host + ":" + fmt::format("{}", _ap_last_port);
		server_buf.text.Assign(full.c_str());
		server_str = full;
		if (!_ap_last_slot.empty()) { slot_buf.text.Assign(_ap_last_slot.c_str()); slot_str = _ap_last_slot; }
		if (!_ap_last_pass.empty()) { pass_buf.text.Assign(_ap_last_pass.c_str()); pass_str = _ap_last_pass; }

		/* Restore town rename custom names if any */
		if (!_ap_town_custom_names.empty()) {
			custom_names_buf.text.Assign(_ap_town_custom_names.c_str());
			custom_str = _ap_town_custom_names;
		}

		UpdateTownButtons();
	}

	void OnEditboxChanged(WidgetID wid) override {
		switch (wid) {
			case WAPGUI_EDIT_SERVER:       server_str = server_buf.text.GetText().data(); break;
			case WAPGUI_EDIT_SLOT:         slot_str   = slot_buf.text.GetText().data();   break;
			case WAPGUI_EDIT_PASS:         pass_str   = pass_buf.text.GetText().data();   break;
			case WAPGUI_EDIT_CUSTOM_NAMES:
				custom_str = custom_names_buf.text.GetText().data();
				_ap_town_custom_names = custom_str;
				break;
		}
	}

	void OnGameTick() override {
		if (_ap_bridge_mode) {
			bool d = _ap_status_dirty.exchange(false);
			if (d) this->SetDirty();
			return;
		}
		if (_ap_client == nullptr) return;
		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		if (s != last_state || h != last_has_sd) { last_state = s; last_has_sd = h; this->SetDirty(); }
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (_ap_bridge_mode) {
			/* In bridge mode, show simplified status */
			switch (widget) {
				case WAPGUI_STATUS:
					DrawString(r.left, r.right, r.top, "Bridge mode (multiplayer)", TC_BLACK);
					break;
				case WAPGUI_SLOT_INFO: {
					const APSlotData &sd = AP_GetSlotData();
					if (sd.start_year > 0) {
						DrawString(r.left, r.right, r.top, "Slot data loaded", TC_DARK_GREEN);
					} else {
						DrawString(r.left, r.right, r.top, "Waiting for slot data...", TC_DARK_GREEN);
					}
					break;
				}
			}
			return;
		}
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
			case WAPGUI_BTN_TOWNS_AUTO:
				_ap_town_rename_mode = 0;
				UpdateTownButtons();
				break;
			case WAPGUI_BTN_TOWNS_CUSTOM:
				_ap_town_rename_mode = 1;
				UpdateTownButtons();
				break;
			case WAPGUI_BTN_TOWNS_OFF:
				_ap_town_rename_mode = 2;
				UpdateTownButtons();
				break;
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
				_ap_client->Connect(host, port, slot_str, pass_str, "OpenTTD", false);
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
	WDP_CENTER, {}, 380, 240,
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
	WAPST_TIMER,     ///< Real-time play timer
	WAPST_BTN_RECONNECT,
	WAPST_BTN_MISSIONS,
	WAPST_BTN_SETTINGS,
	WAPST_BTN_SHOP,
	WAPST_BTN_GUIDE,
	WAPST_BTN_INDEX,
	WAPST_BTN_COLBY,
	WAPST_BTN_DEMIGOD,
	WAPST_BTN_RUINS,
	WAPST_NEWS_LABEL,
	WAPST_NEWS_OFF,
	WAPST_NEWS_SELF,
	WAPST_NEWS_ALL,
	WAPST_BTN_DAYNIGHT,
	WAPST_BTN_OPEN_MP,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_status_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_ARCHIPELAGO_STATUS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
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
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_TIMER),    SetMinimalSize(340, 11), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPST_BTN_RECONNECT), SetStringTip(STR_ARCHIPELAGO_BTN_RECONNECT), SetMinimalSize(80, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BLUE,   WAPST_BTN_MISSIONS),  SetStringTip(STR_ARCHIPELAGO_BTN_MISSIONS),  SetMinimalSize(80, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPST_BTN_SETTINGS),  SetStringTip(STR_ARCHIPELAGO_BTN_SETTINGS),  SetMinimalSize(80, 14),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_NEWS_LABEL), SetMinimalSize(50, 14), SetStringTip(STR_ARCHIPELAGO_NEWS_LABEL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WAPST_NEWS_OFF),  SetStringTip(STR_ARCHIPELAGO_NEWS_OFF),  SetMinimalSize(40, 14),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WAPST_NEWS_SELF), SetStringTip(STR_ARCHIPELAGO_NEWS_SELF), SetMinimalSize(40, 14),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WAPST_NEWS_ALL),  SetStringTip(STR_ARCHIPELAGO_NEWS_ALL),  SetMinimalSize(40, 14),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPST_BTN_OPEN_MP), SetStringTip(STR_AP_OPEN_MP_BTN), SetMinimalSize(200, 16), SetFill(1, 0),
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
		/* Bridge mode: AP is handled by the Bridge, not a local client */
		if (_ap_bridge_mode) return "AP: Bridge (multiplayer)";

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
		/* Bridge mode: slot_data comes from the Bridge */
		if (_ap_bridge_mode) {
			const APSlotData &sd = AP_GetSlotData();
			if (sd.start_year == 0 && sd.map_x == 0) return "Waiting for slot data...";
			std::string diff_names[] = {"Casual","Easy","Normal","Medium","Hard",
			    "Very Hard","Extreme","Insane","Nutcase","Madness","Custom"};
			int di = (int)sd.win_difficulty;
			std::string dn = (di >= 0 && di <= 10) ? diff_names[di] : "?";
			return "Goal: " + dn + "  (click Guide for details)";
		}
		if (_ap_client == nullptr || !_ap_client->HasSlotData()) return "No slot data";
		const APSlotData &sd = AP_GetSlotData();
		std::string diff_names[] = {"Casual","Easy","Normal","Medium","Hard",
		    "Very Hard","Extreme","Insane","Nutcase","Madness","Custom"};
		int di = (int)sd.win_difficulty;
		std::string dn = (di >= 0 && di <= 10) ? diff_names[di] : "?";
		return "Goal: " + dn + "  (click Guide for details)";
	}

	/** Draw one win-condition line: "Label  cur / tgt  (XX%)  [OK]" */
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
		    AP_FmtShort(cur),
		    AP_FmtShort(tgt),
		    pct,
		    done ? "  [OK]" : "");
		DrawString(r.left, r.right, r.top, line, col);
	}

	uint timer_redraw_acc = 0; ///< ms accumulator for once-per-second timer redraw

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		/* In bridge mode, just check the dirty flag */
		if (_ap_bridge_mode) {
			bool d = _ap_status_dirty.exchange(false);
			if (d) this->SetDirty();
			return;
		}

		/* Play timer: tick while game is not paused and goal not sent */
		if (_game_mode == GM_NORMAL && !_pause_mode.Any() && !AP_GetGoalSent()) {
			AP_AddPlayTimerMs(delta_ms);
		}

		/* Redraw once per second for the timer, or when state changes */
		bool need_redraw = false;
		timer_redraw_acc += delta_ms;
		if (timer_redraw_acc >= 1000) {
			timer_redraw_acc -= 1000;
			need_redraw = true;
		}

		if (_ap_client != nullptr) {
			APState s = _ap_client->GetState();
			bool    h = _ap_client->HasSlotData();
			if (s != last_state || h != last_has_sd) {
				last_state = s; last_has_sd = h;
				need_redraw = true;
			}
		}
		bool d = _ap_status_dirty.exchange(false);
		if (d) need_redraw = true;

		if (need_redraw) this->SetDirty();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		switch (widget) {
			case WAPST_STATUS_LINE: {
				bool ok = _ap_bridge_mode || (_ap_client && _ap_client->GetState() == APState::AUTHENTICATED);
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
				if (_ap_bridge_mode || (_ap_client != nullptr && _ap_client->HasSlotData())) {
					DrawWinLine(r, widget);
				}
				break;

			case WAPST_TIMER: {
				uint32_t secs = AP_GetPlayTimer();
				uint32_t h = secs / 3600;
				uint32_t m = (secs % 3600) / 60;
				uint32_t s = secs % 60;
				bool frozen = AP_GetGoalSent();
				std::string label = frozen ? "Time (FINAL)" : "Time";
				std::string line = fmt::format("{}  {:02d}:{:02d}:{:02d}", label, h, m, s);
				DrawString(r.left, r.right, r.top, line, frozen ? TC_LIGHT_BLUE : TC_GOLD);
				break;
			}
		}
	}

	void OnPaint() override {
		bool disconnected = !_ap_bridge_mode && (_ap_client == nullptr ||
		    _ap_client->GetState() == APState::DISCONNECTED ||
		    _ap_client->GetState() == APState::AP_ERROR);
		this->SetWidgetDisabledState(WAPST_BTN_RECONNECT, !disconnected || _ap_last_host.empty());
		this->SetWidgetDisabledState(WAPST_BTN_MISSIONS, !AP_IsConnected());
		this->SetWidgetDisabledState(WAPST_BTN_SHOP,     !AP_IsConnected());
		/* Colby button: enabled when event is configured (shows countdown in step 0,
		 * full event UI once active). Disabled when no event is configured at all. */
		/* Highlight active news filter button */
		this->SetWidgetLoweredState(WAPST_NEWS_OFF,  _ap_news_filter == 0);
		this->SetWidgetLoweredState(WAPST_NEWS_SELF, _ap_news_filter == 1);
		this->SetWidgetLoweredState(WAPST_NEWS_ALL,  _ap_news_filter == 2);

		/* Open to Multiplayer: hide when already a server, disable when not in normal game,
		 * or when multiplayer_mode is not enabled in AP slot_data */
		bool already_server = _networking && _network_server;
		bool mp_disabled = !AP_IsMultiplayerMode();
		this->SetWidgetDisabledState(WAPST_BTN_OPEN_MP, already_server || _game_mode != GM_NORMAL || mp_disabled);
		if (already_server) {
			this->GetWidget<NWidgetCore>(WAPST_BTN_OPEN_MP)->SetString(STR_AP_OPEN_MP_STATUS);
		} else {
			this->GetWidget<NWidgetCore>(WAPST_BTN_OPEN_MP)->SetString(STR_AP_OPEN_MP_BTN);
		}

		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPST_BTN_RECONNECT:
				if (_ap_client && !_ap_last_host.empty())
					_ap_client->Connect(_ap_last_host, _ap_last_port, _ap_last_slot, _ap_last_pass, "OpenTTD", _ap_last_ssl);
				break;
			case WAPST_BTN_MISSIONS:
				ShowArchipelagoMissionsWindow();
				break;
			case WAPST_BTN_SETTINGS:
				ShowArchipelagoConnectWindow();
				break;
			case WAPST_NEWS_OFF:  _ap_news_filter = 0; this->SetDirty(); break;
			case WAPST_NEWS_SELF: _ap_news_filter = 1; this->SetDirty(); break;
			case WAPST_NEWS_ALL:  _ap_news_filter = 2; this->SetDirty(); break;
			case WAPST_BTN_OPEN_MP:
				AP_OpenToMultiplayer();
				this->SetDirty();
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

/**
 * Encode a Unicode code point as UTF-8 and append to @p out.
 */
static void AppendUtf8(std::string &out, char32_t c)
{
	if (c < 0x80) {
		out += (char)c;
	} else if (c < 0x800) {
		out += (char)(0xC0 | (c >> 6));
		out += (char)(0x80 | (c & 0x3F));
	} else if (c < 0x10000) {
		out += (char)(0xE0 | (c >> 12));
		out += (char)(0x80 | ((c >> 6) & 0x3F));
		out += (char)(0x80 | (c & 0x3F));
	} else {
		out += (char)(0xF0 | (c >> 18));
		out += (char)(0x80 | ((c >> 12) & 0x3F));
		out += (char)(0x80 | ((c >> 6) & 0x3F));
		out += (char)(0x80 | (c & 0x3F));
	}
}

/**
 * Build a copy of @p text where numeric sequences (digits, commas, currency prefixes)
 * are wrapped in SCC_PUSH_COLOUR / SCC_BLACK / SCC_POP_COLOUR so they render in black
 * while the rest uses whatever colour DrawString is called with.
 *
 * This replaces the old DrawStringWithBlackNumbers which drew segments one by one.
 * Drawing piecemeal caused cumulative 1-pixel overlap (DrawString returns the
 * rightmost pixel, not the next free pixel), leading to garbled text.
 */
static std::string ColourizeNumbers(const std::string &text)
{
	std::string result;
	result.reserve(text.size() + 64);

	size_t i = 0;
	while (i < text.size()) {
		/* Find start of next numeric token. */
		size_t num_start = std::string::npos;
		for (size_t j = i; j < text.size(); j++) {
			unsigned char ch = (unsigned char)text[j];
			if (ch >= '0' && ch <= '9') { num_start = j; break; }
			/* UTF-8 £ (0xC2 0xA3) followed by digit */
			if (ch == 0xC2 && j + 2 < text.size() && (unsigned char)text[j+1] == 0xA3 &&
			    (unsigned char)text[j+2] >= '0' && (unsigned char)text[j+2] <= '9') {
				num_start = j; break;
			}
			/* $ followed by digit */
			if (ch == '$' && j + 1 < text.size() &&
			    (unsigned char)text[j+1] >= '0' && (unsigned char)text[j+1] <= '9') {
				num_start = j; break;
			}
		}

		if (num_start == std::string::npos) {
			result.append(text, i, text.size() - i);
			break;
		}

		/* Copy text before the number verbatim. */
		result.append(text, i, num_start - i);

		/* Find end of numeric token. */
		size_t num_end = num_start;
		if ((unsigned char)text[num_end] == 0xC2 && num_end + 1 < text.size() && (unsigned char)text[num_end+1] == 0xA3) {
			num_end += 2;
		} else if (text[num_end] == '$') {
			num_end += 1;
		}
		while (num_end < text.size()) {
			unsigned char ch = (unsigned char)text[num_end];
			if (ch >= '0' && ch <= '9') { num_end++; continue; }
			if ((ch == ',' || ch == '.') && num_end + 1 < text.size() &&
			    (unsigned char)text[num_end+1] >= '0' && (unsigned char)text[num_end+1] <= '9') {
				num_end++; continue;
			}
			break;
		}

		/* Wrap the number in inline colour codes:
		 * PUSH_COLOUR saves the current text colour,
		 * BLACK switches to black for the number,
		 * POP_COLOUR restores the original. */
		AppendUtf8(result, SCC_PUSH_COLOUR);
		AppendUtf8(result, SCC_BLACK);
		result.append(text, num_start, num_end - num_start);
		AppendUtf8(result, SCC_POP_COLOUR);

		i = num_end;
	}
	return result;
}

/* =========================================================================
 * MISSIONS WINDOW (Bug F fix)
 *
 * Scrollable list of all missions from slot_data.
 * Shows: difficulty colour, description, completed/pending status.
 * ========================================================================= */

enum APMissionsWidgets : WidgetID {
	WAPM_CAPTION,
	WAPM_FILTER_PANEL,
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
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_MISSIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		/* Single filter row — All / Easy / Medium / Hard / Extreme / Tasks */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 2, 2), SetPadding(2),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPM_FILTER_ALL),     SetStringTip(STR_ARCHIPELAGO_FILTER_ALL),     SetMinimalSize(44, 14), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPM_FILTER_EASY),    SetStringTip(STR_ARCHIPELAGO_FILTER_EASY),    SetMinimalSize(44, 14), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPM_FILTER_MEDIUM),  SetStringTip(STR_ARCHIPELAGO_FILTER_MEDIUM),  SetMinimalSize(44, 14), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPM_FILTER_HARD),    SetStringTip(STR_ARCHIPELAGO_FILTER_HARD),    SetMinimalSize(44, 14), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_RED,    WAPM_FILTER_EXTREME), SetStringTip(STR_ARCHIPELAGO_FILTER_EXTREME), SetMinimalSize(44, 14), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BLUE,   WAPM_FILTER_TASKS),   SetStringTip(STR_ARCHIPELAGO_TAB_TASKS),      SetMinimalSize(44, 14), SetFill(1, 0), SetResize(1, 0),
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
	std::string   filter        = "all";  /* "all","easy","medium","hard","extreme","tasks" */
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
		 * so this fires continuously for real-time mission progress display.
		 * Tasks tab: ALWAYS rebuild so live t.current_value is reflected. */
		if (_ap_status_dirty.exchange(false) || show_tasks) RebuildVisibleList();
		else this->SetDirty();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override {
		if (!gui_scope) return;
		RebuildVisibleList();
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
		/* Reset scrollbar to top */
		if (this->scrollbar) this->scrollbar->SetPosition(0);
		if (show_tasks) {
			visible_missions.clear();
			RebuildTaskList();
		} else {
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
					/* Tasks: 4 rows per task. Row 0 = header — never navigate. */
					int abs_row = this->scrollbar->GetPosition() + (pt.y - r.top - 2) / rh;
					if (abs_row <= 0) break;
					int task_idx = (abs_row - 1) / 4;
					int sub_row  = (abs_row - 1) % 4;
					if (task_idx < 0 || task_idx >= (int)cached_tasks.size()) break;
					if (sub_row >= 2) break; /* only action + location rows clickable */
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
			 *   Line B:    → Entity / location name
			 *   Line C:      Progress  •  Deadline  •  Reward
			 * Row order: header(1) + task0(3) + sep(1)+task1(3) + ... */
			int first_row = this->scrollbar->GetPosition();
			int row_idx   = 0;

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
					DrawString(r.left + 4 + x_off, r.right, y, hdr, TC_GOLD);
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
						GfxFillRect(r.left + 4, y + rh / 2, r.right - 4, y + rh / 2 + 1, PC_DARK_GREY);
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
						for (const char *tag : {" [Reward", " [REWARD"}) {
							size_t p = action.find(tag);
							if (p != std::string::npos) { action = action.substr(0, p); break; }
						}
						for (const char *sep : {" from ", " to "}) {
							size_t p = action.find(sep);
							if (p != std::string::npos) { action = action.substr(0, p); break; }
						}

						std::string line_a = badge + diff_tag + "  " + action;
						DrawString(r.left + 4 + x_off, r.right, y, line_a, tc, SA_LEFT | SA_FORCE);
					}
					advance();
				}

				/* ── Line B: entity / location ── */
				{
					int y = row_y();
					if (y >= 0) {
						std::string remainder;
						const char *used_sep = nullptr;
						for (const char *sep : {" from ", " to "}) {
							size_t p = t.description.find(sep);
							if (p != std::string::npos) {
								remainder = t.description.substr(p + strlen(sep));
								used_sep  = sep;
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
							/* Chain safely: DrawString returns 0 when clipped (overlapping windows). */
							int base_x = r.left + 4 + x_off;
							int ret2 = DrawString(base_x, r.right, y, prefix_str, tc, SA_LEFT | SA_FORCE);
							int x2 = (ret2 > 0) ? ret2 + 1 : base_x;
							size_t en_pos = remainder.find(t.entity_name);
							if (en_pos == 0) {
								int ret3 = DrawString(x2, r.right, y, t.entity_name, TC_WHITE, SA_LEFT | SA_FORCE);
								int x3 = (ret3 > 0) ? ret3 + 1 : x2;
								std::string tail = remainder.substr(t.entity_name.size());
								if (!tail.empty()) DrawString(x3, r.right, y, tail, tc, SA_LEFT | SA_FORCE);
							} else {
								DrawString(x2, r.right, y, remainder, TC_WHITE, SA_LEFT | SA_FORCE);
							}
						} else {
							std::string fallback = prefix_str + (t.entity_name.empty() ? "(any)" : t.entity_name);
							DrawString(r.left + 4 + x_off, r.right, y, fallback, tc, SA_LEFT | SA_FORCE);
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
							std::string unit_str;
							if (IsValidCargoType((CargoType)t.cargo)) {
								const CargoSpec *cs = CargoSpec::Get((CargoType)t.cargo);
								if (cs && cs->classes.Test(CargoClass::Passengers)) {
									unit_str = "";
								} else {
									unit_str = " t";
								}
							}
							line_c += AP_FmtNum(t.current_value) + " / " + AP_FmtNum(t.amount) + unit_str + "  (" + fmt::format("{}%", pct) + ")";
							line_c += fmt::format("   -   By {}", t.deadline_year);
							if (t.reward_type == APTaskRewardType::MISSION_CHECK) {
								line_c += "   -   +[Mission Check]";
							} else {
								line_c += fmt::format("   -   +{}", AP_FormatMoneyCompact(t.reward_cash));
							}
						}
						DrawString(r.left + 4 + x_off, r.right, y, line_c, TC_GREY, SA_LEFT | SA_FORCE);
					}
					advance();
				}
			}
			return;
		}

		/* ── Missions tab (original logic) ── */
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

			/* Build progress string for ALL incomplete missions.
			 * Always show status so the player knows the tracker is live.
			 * Format: " [Status: X]" — target is already in the mission description text. */
			bool is_maintain = (m->type == "maintain_75" ||
			                    m->type.find("maintain") != std::string::npos);
			std::string progress_str;
			if (!m->completed && m->amount > 0) {
				if (is_maintain) {
					/* Keep months format — target is meaningful here */
					progress_str = "  (" + AP_FmtNum(m->current_value) + "/" + AP_FmtNum(m->amount) + " months)";
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
						progress_str = "  [Status: " + AP_FmtShort(std::max<int64_t>(0, m->current_value)) + "]";
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
			/* Assemble the main line WITHOUT the status/progress suffix —
			 * we draw that separately in TC_BLACK for readability. */
			std::string main_line = prefix + cap_diff + mission_num + " \xe2\x80\x93 " + desc + nav_hint;

			int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;
			int x = r.left + 4 + x_off;
			int right_edge = r.right;

			/* Check if this is a named-destination mission with a clickable entity */
			bool has_entity_link = (m->named_entity.tile != UINT32_MAX &&
			                        !m->named_entity.name.empty() && !m->completed);

			/* Helper: chain DrawString calls safely.
			 * DrawString returns the rightmost pixel drawn (left + w - 1),
			 * so we add +1 for the next segment.  BUT it returns 0 when
			 * the draw region is clipped (overlapping windows, partial redraws).
			 * In that case, keep using the previous x to avoid jumping to pixel 0. */
			auto ChainDraw = [](int prev_x, int ret) -> int {
				return (ret > 0) ? ret + 1 : prev_x;
			};

			if (has_entity_link) {
				const std::string &ename = m->named_entity.name;
				size_t name_pos = main_line.find(ename);
				if (name_pos != std::string::npos) {
					std::string before = main_line.substr(0, name_pos);
					std::string after  = main_line.substr(name_pos + ename.size());
					int x2 = ChainDraw(x, DrawString(x, right_edge, y, ColourizeNumbers(before), tc, SA_LEFT | SA_FORCE));
					int x3 = ChainDraw(x2, DrawString(x2, right_edge, y, ename, TC_WHITE, SA_LEFT | SA_FORCE));
					x = ChainDraw(x3, DrawString(x3, right_edge, y, ColourizeNumbers(after), tc, SA_LEFT | SA_FORCE));
				} else {
					x = ChainDraw(x, DrawString(x, right_edge, y, ColourizeNumbers(main_line), tc, SA_LEFT | SA_FORCE));
				}
			} else {
				x = ChainDraw(x, DrawString(x, right_edge, y, ColourizeNumbers(main_line), tc, SA_LEFT | SA_FORCE));
			}

			/* Draw progress/status in TC_BLACK for readability */
			if (!progress_str.empty()) {
				DrawString(x, right_edge, y, ColourizeNumbers(progress_str), TC_BLACK, SA_LEFT | SA_FORCE);
			}

			y += rh;
			if (y > r.bottom) break;
		}
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
		NWidget(WWT_CAPTION, COLOUR_CREAM), SetStringTip(STR_ARCHIPELAGO_SHOP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_CREAM),
		NWidget(WWT_DEFSIZEBOX, COLOUR_CREAM),
		NWidget(WWT_STICKYBOX, COLOUR_CREAM),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_CREAM), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_CREAM, WAPSH_LIST), SetMinimalSize(380, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPSH_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_CREAM, WAPSH_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPIP(4, 4, 4), SetPadding(4),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPSH_BTN_BUY),   SetStringTip(STR_ARCHIPELAGO_SHOP_BUY,   STR_EMPTY), SetMinimalSize(120, 16), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,  WAPSH_BTN_CLOSE), SetStringTip(STR_ARCHIPELAGO_BTN_CLOSE, STR_EMPTY), SetMinimalSize(80,  16), SetResize(1, 0),
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
		bool        purchased;    ///< true when already bought
		int         display_num;  ///< permanent display number (1-based, from sorted position)
	};
	std::vector<ShopEntry> shop_items;

	void RebuildShopList()
	{
		shop_items.clear();
		int total = AP_GetShopSlots();

		/* Collect ALL items (including already purchased) with labels and prices. */
		struct RawEntry { std::string loc; std::string label; int64_t price; bool purchased; };
		std::vector<RawEntry> raw;
		for (int i = 1; i <= total; i++) {
			std::string loc = fmt::format("Shop_Purchase_{:04d}", i);
			bool purchased = AP_IsShopLocationSent(loc);
			std::string label = AP_GetShopLocationLabel(loc);
			if (label.empty()) label = fmt::format("Item #{} (loading...)", i);
			raw.push_back({loc, label, AP_GetShopPrice(loc), purchased});
		}
		/* Sort ascending by price — cheapest first. */
		std::sort(raw.begin(), raw.end(),
		    [](const RawEntry &a, const RawEntry &b) { return a.price < b.price; });

		/* Assign lock state based on sorted position (uses original index,
		 * NOT filtered index — so purchasing items never changes other items'
		 * lock tiers or display numbers). */
		for (int idx = 0; idx < (int)raw.size(); idx++) {
			bool locked  = !raw[idx].purchased && !AP_IsShopSlotUnlocked(idx);
			int  needed  = AP_GetShopSlotRequiredMissions(idx);
			shop_items.push_back({raw[idx].loc, raw[idx].label, raw[idx].price,
			    locked, needed, raw[idx].purchased, idx + 1});
		}

		if (this->scrollbar) this->scrollbar->SetCount((int)shop_items.size());
		max_line_px = 0;
		for (int idx = 0; idx < (int)shop_items.size(); idx++) {
			const auto &e = shop_items[idx];
			std::string full;
			if (e.purchased) {
				full = fmt::format("[{}] [SOLD] {}", e.display_num, e.label);
			} else if (e.locked) {
				full = fmt::format("[{}] [LOCKED] Complete {} missions to unlock", e.display_num, e.missions_needed);
			} else {
				full = fmt::format("[{}] {} \xe2\x80\x94 {}", e.display_num, e.label, AP_FormatMoneyCompact(e.price));
			}
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
			if (e.purchased) {
				tc = TC_GREEN;
			} else if (e.locked) {
				tc = TC_GREY;
			} else if (i == selected) {
				tc = TC_WHITE;
			} else if (AP_CanAffordShopItem(e.location_name)) {
				tc = TC_YELLOW;
			} else {
				tc = TC_GREY;
			}
			std::string line;
			if (e.purchased) {
				line = fmt::format("[{}] [SOLD] {}",
				    e.display_num, e.label);
			} else if (e.locked) {
				line = fmt::format("[{}] [LOCKED] Complete {} missions to unlock",
				    e.display_num, e.missions_needed);
			} else {
				line = fmt::format("[{}] {} — {}",
				    e.display_num, e.label, AP_FormatMoneyCompact(e.price));
			}
			int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;
			DrawString(r.left + 4 + x_off, r.right, y, line, tc, SA_LEFT | SA_FORCE);
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
					ShopEntry &entry = shop_items[selected];
					if (entry.purchased) {
						AP_ShowConsole("[AP] Shop: already purchased");
						break;
					}
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
					/* Mark as purchased — don't remove from list */
					entry.purchased = true;
					selected = -1;
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
		NWidget(WWT_SHADEBOX, COLOUR_DARK_BLUE),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_BLUE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_BLUE),
	EndContainer(),
	/* Tab buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPGD_TAB_COMMANDS), SetStringTip(STR_ARCHIPELAGO_GUIDE_TAB_COMMANDS), SetMinimalSize(110, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPGD_TAB_HOTKEYS),  SetStringTip(STR_ARCHIPELAGO_GUIDE_TAB_HOTKEYS),  SetMinimalSize(110, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPGD_TAB_TIPS),     SetStringTip(STR_ARCHIPELAGO_GUIDE_TAB_TIPS),     SetMinimalSize(110, 16), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	/* Content panel + scrollbar */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_BLUE, WAPGD_PANEL), SetMinimalSize(330, 300), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPGD_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_BLUE, WAPGD_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_BLUE), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_BLUE),
	EndContainer(),
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
		_L("Fast forward is capped at 2500% — this keeps");
		_L("the AP connection stable.");
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
 * RUINS TRACKER WINDOW
 * Shows all AP ruins (active + completed) with cargo progress and
 * click-to-scroll-to-tile.
 * ========================================================================= */

enum ArchipelagoRuinsTrackerWidgets : WidgetID {
	WAPRT_CAPTION,
	WAPRT_PANEL,
	WAPRT_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_ruins_tracker_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WAPRT_CAPTION), SetStringTip(STR_ARCHIPELAGO_RUINS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WAPRT_PANEL), SetMinimalSize(380, 300), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPRT_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPRT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

struct ArchipelagoRuinsTrackerWindow : Window {
	std::vector<APRuinView> ruins;
	std::vector<int> entry_y_offsets; ///< Cumulative Y pixel offset per entry
	int total_list_height = 0;        ///< Total pixel height of all entries
	Scrollbar *vscroll = nullptr;
	int refresh_timer = 0;

	/** Height of a single ruin entry based on actual cargo count. */
	int _EntryH(int cargo_count) const {
		int line_h = GetCharacterHeight(FS_NORMAL) + 2;
		/* 1 header line + cargo lines + 1 blank separator */
		return line_h * (1 + cargo_count + 1);
	}

	/** Max entry height (for scrollbar capacity). */
	int _MaxEntryH() const {
		int line_h = GetCharacterHeight(FS_NORMAL) + 2;
		return line_h * 6; /* header + 4 cargo max + separator */
	}

	ArchipelagoRuinsTrackerWindow(WindowDesc &desc, WindowNumber wn) : Window(desc)
	{
		this->InitNested(wn);
		this->vscroll = this->GetScrollbar(WAPRT_SCROLLBAR);
		this->_RebuildList();
	}

	void _RebuildList()
	{
		this->ruins = AP_GetAllRuinViews();

		/* Sort: incomplete first (by id), then completed (by id). */
		std::sort(this->ruins.begin(), this->ruins.end(), [](const APRuinView &a, const APRuinView &b) {
			if (a.completed != b.completed) return !a.completed;
			return a.id < b.id;
		});

		/* Pre-compute cumulative Y offsets for pixel-accurate click detection. */
		this->entry_y_offsets.clear();
		int cum = 0;
		for (const auto &rv : this->ruins) {
			this->entry_y_offsets.push_back(cum);
			cum += _EntryH((int)rv.cargo.size());
		}
		this->total_list_height = cum;

		if (this->vscroll) {
			/* Use pixel-based scrolling for variable-height entries. */
			this->vscroll->SetCount(this->total_list_height);
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPRT_PANEL);
			int panel_h = panel ? panel->current_y : 300;
			this->vscroll->SetCapacity(panel_h);
		}
	}

	void OnPaint() override { this->DrawWidgets(); }

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPRT_PANEL) return;

		int line_h = GetCharacterHeight(FS_NORMAL) + 2;
		int max_y = r.bottom - 2;
		int xl = r.left + 6;
		int xr = r.right - 6;

		int scroll_pos = this->vscroll ? this->vscroll->GetPosition() : 0;

		if (this->ruins.empty()) {
			DrawString(xl, xr, r.top + 3, "No ruins discovered yet.", TC_GREY);
			return;
		}

		for (int i = 0; i < (int)this->ruins.size(); i++) {
			int entry_top = (i < (int)this->entry_y_offsets.size())
				? this->entry_y_offsets[i] : 0;
			int y = r.top + 3 + entry_top - scroll_pos;

			/* Skip entries above visible area */
			int eh = _EntryH((int)this->ruins[i].cargo.size());
			if (y + eh < r.top) continue;
			if (y > max_y) break;

			const auto &rv = this->ruins[i];

			/* ── Header line: [status icon] Ruin name — Town ── */
			TextColour name_col = rv.completed ? TC_GREEN : TC_ORANGE;
			std::string status_icon = rv.completed ? "\xE2\x9C\x93 " : "\xE2\x97\x8F "; /* ✓ or ● */
			std::string header = status_icon + rv.location_name;
			if (!rv.town_name.empty()) {
				header += " — " + rv.town_name;
			}
			if (rv.completed) header += "  [CLEARED]";
			DrawString(xl, xr, y, header, name_col);
			y += line_h;

			/* ── Cargo progress lines ── */
			for (const auto &c : rv.cargo) {
				std::string cargo_line = "    " + c.name + ": ";
				cargo_line += fmt::format("{}/{}", c.delivered, c.required);

				TextColour cargo_col;
				if (c.delivered >= c.required) {
					cargo_col = TC_GREEN;
					cargo_line += "  \xE2\x9C\x93"; /* ✓ */
				} else if (c.delivered > 0) {
					cargo_col = TC_YELLOW;
				} else {
					cargo_col = TC_WHITE;
				}

				DrawString(xl, xr, y, cargo_line, cargo_col);
				y += line_h;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		if (widget != WAPRT_PANEL) return;

		const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPRT_PANEL);
		int scroll_pos = this->vscroll ? this->vscroll->GetPosition() : 0;
		int pixel_y = (pt.y - panel->pos_y - 3) + scroll_pos;

		/* Binary search through cumulative offsets to find clicked entry. */
		int idx = -1;
		for (int i = 0; i < (int)this->ruins.size(); i++) {
			int top = (i < (int)this->entry_y_offsets.size()) ? this->entry_y_offsets[i] : 0;
			int bot = top + _EntryH((int)this->ruins[i].cargo.size());
			if (pixel_y >= top && pixel_y < bot) { idx = i; break; }
		}

		if (idx < 0 || idx >= (int)this->ruins.size()) return;

		const auto &rv = this->ruins[idx];
		if (rv.tile != UINT32_MAX) {
			ScrollMainWindowToTile(TileIndex{rv.tile});
		}
	}

	void OnGameTick() override
	{
		/* Refresh every ~2 seconds (74 ticks). */
		if (++this->refresh_timer >= 74) {
			this->refresh_timer = 0;
			this->_RebuildList();
			this->SetDirty();
		}
	}

	void OnResize() override
	{
		if (this->vscroll) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPRT_PANEL);
			int panel_h = panel ? panel->current_y : 300;
			this->vscroll->SetCapacity(panel_h);
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }
};

static WindowDesc _ap_ruins_desc(
	WDP_AUTO, {"ap_ruins_tracker"}, 400, 350,
	WC_ARCHIPELAGO_RUINS_TRACKER, WC_NONE, {},
	_nested_ap_ruins_tracker_widgets
);

void ShowArchipelagoRuinsTrackerWindow()
{
	AllocateWindowDescFront<ArchipelagoRuinsTrackerWindow>(_ap_ruins_desc, 0);
}


/* =========================================================================
 * STAR TRACKER WINDOW — reveal and track star locations
 * ========================================================================= */

enum ArchipelagoStarTrackerWidgets : WidgetID {
	WAPST_ST_CAPTION,
	WAPST_ST_PANEL,
	WAPST_ST_SCROLLBAR,
	WAPST_ST_BTN_REVEAL,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_star_tracker_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WAPST_ST_CAPTION), SetStringTip(STR_ARCHIPELAGO_STAR_TRACKER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WAPST_ST_PANEL), SetMinimalSize(380, 200), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPST_ST_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WAPST_ST_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL), SetPIP(4, 4, 4),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPST_ST_BTN_REVEAL), SetStringTip(STR_ARCHIPELAGO_STAR_REVEAL_BTN), SetMinimalSize(360, 16), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
};

struct ArchipelagoStarTrackerWindow : Window {
	std::vector<APStarView> stars;
	Scrollbar *vscroll = nullptr;
	int refresh_timer = 0;

	ArchipelagoStarTrackerWindow(WindowDesc &desc, WindowNumber wn) : Window(desc)
	{
		this->InitNested(wn);
		this->vscroll = this->GetScrollbar(WAPST_ST_SCROLLBAR);
		this->_RebuildList();
	}

	void _RebuildList()
	{
		this->stars = AP_GetRevealedStarViews();
		if (this->vscroll) {
			int line_h = GetCharacterHeight(FS_NORMAL) + 2;
			this->vscroll->SetCount((int)this->stars.size() * line_h + line_h * 3);
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPST_ST_PANEL);
			int panel_h = panel ? panel->current_y : 200;
			this->vscroll->SetCapacity(panel_h);
		}
	}

	void OnPaint() override
	{
		int revealed = AP_GetStarsRevealed();
		int pool = AP_GetStarPoolSize();
		int collected = AP_GetStarsCollected();
		int remaining = pool - collected;
		bool can_reveal = (remaining > revealed);

		this->SetWidgetDisabledState(WAPST_ST_BTN_REVEAL, !can_reveal);

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPST_ST_PANEL) return;

		int line_h = GetCharacterHeight(FS_NORMAL) + 2;
		int xl = r.left + 6;
		int xr = r.right - 6;
		int scroll_pos = this->vscroll ? this->vscroll->GetPosition() : 0;

		/* Header info */
		int collected = AP_GetStarsCollected();
		int pool = AP_GetStarPoolSize();
		int revealed = AP_GetStarsRevealed();

		std::string header = fmt::format("Stars: {}/{} collected | {} locations revealed", collected, pool, revealed);
		DrawString(xl, xr, r.top + 3, header, TC_GOLD);

		/* Cost line */
		int remaining = pool - collected;
		bool can_reveal = (remaining > revealed);
		if (can_reveal) {
			int64_t cost = AP_GetStarRevealCost();
			std::string cost_line = fmt::format("Next reveal cost: {}", GetString(STR_JUST_CURRENCY_LONG, cost));
			DrawString(xl, xr, r.top + 3 + line_h, cost_line, TC_WHITE);
		} else {
			DrawString(xl, xr, r.top + 3 + line_h, "All stars revealed or collected.", TC_GREY);
		}

		if (this->stars.empty()) {
			DrawString(xl, xr, r.top + 3 + line_h * 2, "Pay to reveal star locations on the map.", TC_GREY);
			return;
		}

		/* Draw revealed star entries */
		int entry_y = r.top + 3 + line_h * 3 - scroll_pos;
		for (int i = 0; i < (int)this->stars.size(); i++) {
			if (entry_y > r.bottom) break;
			if (entry_y + line_h < r.top) { entry_y += line_h; continue; }

			const auto &sv = this->stars[i];
			std::string line = fmt::format("  {} {}", "\xE2\x98\x85", sv.location_name);
			DrawString(xl, xr, entry_y, line, TC_YELLOW);
			entry_y += line_h;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		if (widget == WAPST_ST_BTN_REVEAL) {
			if (AP_RevealNextStar()) {
				this->_RebuildList();
				this->SetDirty();
			}
			return;
		}

		if (widget != WAPST_ST_PANEL) return;

		/* Click on a revealed star -> scroll to it on the map */
		int line_h = GetCharacterHeight(FS_NORMAL) + 2;
		const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPST_ST_PANEL);
		int scroll_pos = this->vscroll ? this->vscroll->GetPosition() : 0;
		int pixel_y = (pt.y - panel->pos_y - 3) + scroll_pos;

		/* First 3 lines are header, entries start at line 3 */
		int entry_idx = (pixel_y - line_h * 3) / line_h;
		if (entry_idx < 0 || entry_idx >= (int)this->stars.size()) return;

		const auto &sv = this->stars[entry_idx];
		if (sv.tile != UINT32_MAX) {
			ScrollMainWindowToTile(TileIndex{sv.tile});
		}
	}

	void OnGameTick() override
	{
		if (++this->refresh_timer >= 74) {
			this->refresh_timer = 0;
			this->_RebuildList();
			this->SetDirty();
		}
	}

	void OnResize() override
	{
		if (this->vscroll) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPST_ST_PANEL);
			int panel_h = panel ? panel->current_y : 200;
			this->vscroll->SetCapacity(panel_h);
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (gui_scope) {
			this->_RebuildList();
			this->SetDirty();
		}
	}
};

static WindowDesc _ap_star_tracker_desc(
	WDP_AUTO, {"ap_star_tracker"}, 400, 280,
	WC_ARCHIPELAGO_STAR_TRACKER, WC_NONE, {},
	_nested_ap_star_tracker_widgets
);

void ShowArchipelagoStarTrackerWindow()
{
	AllocateWindowDescFront<ArchipelagoStarTrackerWindow>(_ap_star_tracker_desc, 0);
}


/* =========================================================================
 * VEHICLE INDEX WINDOW
 * Categorised vehicle encyclopedia with live stats from the Engine pool.
 * ========================================================================= */

#include "engine_base.h"
#include "cargotype.h"
#include "settings_type.h"
#include "rail_type.h"

/** Format a Money value as a currency string for the index window. */
static std::string _APIdxMoney(Money v) { return GetString(STR_JUST_CURRENCY_LONG, v); }

enum APIndexWidgets : WidgetID {
	WAPIX_CAPTION,
	WAPIX_TAB_TRAINS,
	WAPIX_TAB_ROAD,
	WAPIX_TAB_AIRCRAFT,
	WAPIX_TAB_SHIPS,
	WAPIX_TAB_WAGONS,
	WAPIX_PANEL,
	WAPIX_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_index_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_BLUE),
		NWidget(WWT_CAPTION, COLOUR_DARK_BLUE, WAPIX_CAPTION), SetStringTip(STR_ARCHIPELAGO_INDEX_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_BLUE),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_BLUE),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_BLUE),
	EndContainer(),
	/* Tab buttons */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPIX_TAB_TRAINS),   SetStringTip(STR_ARCHIPELAGO_INDEX_TAB_TRAINS),   SetMinimalSize(80, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPIX_TAB_ROAD),     SetStringTip(STR_ARCHIPELAGO_INDEX_TAB_ROAD),     SetMinimalSize(100, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPIX_TAB_AIRCRAFT),  SetStringTip(STR_ARCHIPELAGO_INDEX_TAB_AIRCRAFT), SetMinimalSize(80, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPIX_TAB_SHIPS),    SetStringTip(STR_ARCHIPELAGO_INDEX_TAB_SHIPS),    SetMinimalSize(60, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_DARK_BLUE, WAPIX_TAB_WAGONS),   SetStringTip(STR_ARCHIPELAGO_INDEX_TAB_WAGONS),   SetMinimalSize(80, 16), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	/* Content panel + scrollbar */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_BLUE, WAPIX_PANEL), SetMinimalSize(460, 360), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPIX_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_BLUE, WAPIX_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_BLUE), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_BLUE),
	EndContainer(),
};

struct IndexEntry {
	EngineID    engine_id;
	std::string name;
	uint        speed;       ///< Display max speed
	Money       cost;        ///< Purchase cost
	Money       running;     ///< Annual running cost
	uint        capacity;    ///< Primary cargo capacity
	uint16_t    mail_cap;    ///< Mail capacity (aircraft)
	CargoType   cargo;       ///< Default cargo type
	std::string cargo_name;
	std::string refit_text;  ///< Comma-separated refit list
	std::string extra;       ///< Type-specific line (power/weight/railtype or subtype)
	bool        unlocked;
};

struct ArchipelagoIndexWindow : Window {
	int active_tab = 0;  ///< 0=trains 1=road 2=aircraft 3=ships 4=wagons
	std::vector<IndexEntry> entries;
	Scrollbar *vscroll = nullptr;

	static constexpr int LINES_PER_ENTRY = 4;

	ArchipelagoIndexWindow(WindowDesc &desc, WindowNumber wn) : Window(desc)
	{
		this->InitNested(wn);
		this->vscroll = this->GetScrollbar(WAPIX_SCROLLBAR);
		this->_BuildEntries();
	}

	int _LineH() const { return GetCharacterHeight(FS_NORMAL) + 2; }
	int _EntryH() const { return _LineH() * LINES_PER_ENTRY + 4; }

	void _BuildEntries()
	{
		entries.clear();
		VehicleType vt;
		bool wagons_only = false;
		switch (active_tab) {
			case 0: vt = VEH_TRAIN;    break;
			case 1: vt = VEH_ROAD;     break;
			case 2: vt = VEH_AIRCRAFT;  break;
			case 3: vt = VEH_SHIP;     break;
			case 4: vt = VEH_TRAIN; wagons_only = true; break;
			default: vt = VEH_TRAIN;    break;
		}

		for (const Engine *e : Engine::Iterate()) {
			if (e->type != vt) continue;

			/* Skip engines with invalid string_id — prevents crash in SCC_ENGINE_NAME */
			if (e->info.string_id == INVALID_STRING_ID) continue;

			/* Climate filter */
			if (!e->info.climates.Test(_settings_game.game_creation.landscape)) continue;

			/* Wagon vs engine filter for trains */
			if (vt == VEH_TRAIN) {
				const auto &ri = e->VehInfo<RailVehicleInfo>();
				bool is_wagon = (ri.railveh_type == RAILVEH_WAGON);
				if (wagons_only && !is_wagon) continue;
				if (!wagons_only && is_wagon) continue;
			}

			IndexEntry ie;
			ie.engine_id = e->index;
			ie.name = GetString(STR_ENGINE_NAME, PackEngineNameDParam(e->index, EngineNameContext::PurchaseList));
			ie.speed = e->GetDisplayMaxSpeed();
			ie.cost = e->GetCost();
			ie.running = e->GetRunningCost();
			ie.mail_cap = 0;
			ie.capacity = e->GetDisplayDefaultCapacity(&ie.mail_cap);
			ie.cargo = e->GetDefaultCargoType();
			ie.unlocked = AP_IsEngineUnlocked(e->index.base());

			/* Cargo name */
			if (IsValidCargoType(ie.cargo)) {
				const CargoSpec *cs = CargoSpec::Get(ie.cargo);
				if (cs != nullptr) ie.cargo_name = GetString(cs->name);
			}
			if (ie.cargo_name.empty()) ie.cargo_name = (ie.capacity > 0) ? "Various" : "None (engine)";

			/* Refit list */
			ie.refit_text.clear();
			int refit_count = 0;
			for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
				if (!HasBit(e->info.refit_mask, ct)) continue;
				if (!IsValidCargoType(ct)) continue;
				const CargoSpec *cs = CargoSpec::Get(ct);
				if (cs == nullptr) continue;
				if (!ie.refit_text.empty()) ie.refit_text += ", ";
				ie.refit_text += GetString(cs->name);
				refit_count++;
				if (refit_count >= 8) { ie.refit_text += ", ..."; break; }
			}
			if (ie.refit_text.empty()) ie.refit_text = "None";

			/* Type-specific extra info */
			if (vt == VEH_TRAIN) {
				const char *rail_names[] = {"Normal Rail", "Electric Rail", "Monorail", "Maglev"};
				const char *class_names[] = {"Steam", "Diesel", "Electric", "Monorail", "Maglev"};
				const auto &ri = e->VehInfo<RailVehicleInfo>();
				/* Find the primary railtype */
				int rt = 0;
				if (ri.intended_railtypes.Test(RAILTYPE_MAGLEV))        rt = 3;
				else if (ri.intended_railtypes.Test(RAILTYPE_MONO))     rt = 2;
				else if (ri.intended_railtypes.Test(RAILTYPE_ELECTRIC)) rt = 1;
				else                                                     rt = 0;
				if (wagons_only) {
					ie.extra = fmt::format("Type: Wagon | Rail: {}",
						(rt >= 0 && rt <= 3) ? rail_names[rt] : "Unknown");
				} else {
					int ec = (int)ri.engclass;
					ie.extra = fmt::format("Power: {} hp | Weight: {}t | {} | {}",
						e->GetPower(), e->GetDisplayWeight(),
						(ec >= 0 && ec <= 4) ? class_names[ec] : "Unknown",
						(rt >= 0 && rt <= 3) ? rail_names[rt] : "Unknown");
				}
			} else if (vt == VEH_AIRCRAFT) {
				const auto &ai = e->VehInfo<AircraftVehicleInfo>();
				bool is_heli = (ai.subtype & AIR_CTOL) == 0;
				ie.extra = fmt::format("Type: {} | Range: {}",
					is_heli ? "Helicopter" : "Aeroplane",
					e->GetRange() > 0 ? fmt::format("{} tiles", e->GetRange()) : "Unlimited");
				if (ie.mail_cap > 0) {
					ie.extra += fmt::format(" | Mail: {}", ie.mail_cap);
				}
			} else if (vt == VEH_SHIP) {
				ie.extra = fmt::format("Type: Ship");
			} else if (vt == VEH_ROAD) {
				ie.extra = fmt::format("Type: Road Vehicle");
			}

			entries.push_back(std::move(ie));
		}

		/* Sort alphabetically by name */
		std::sort(entries.begin(), entries.end(), [](const IndexEntry &a, const IndexEntry &b) {
			return a.name < b.name;
		});

		if (vscroll) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPIX_PANEL);
			int visible = (panel ? (int)panel->current_y : 360) / _EntryH();
			vscroll->SetCount((int)entries.size());
			vscroll->SetCapacity(std::max(1, visible));
		}
	}

	void OnPaint() override
	{
		this->SetWidgetLoweredState(WAPIX_TAB_TRAINS,   active_tab == 0);
		this->SetWidgetLoweredState(WAPIX_TAB_ROAD,     active_tab == 1);
		this->SetWidgetLoweredState(WAPIX_TAB_AIRCRAFT,  active_tab == 2);
		this->SetWidgetLoweredState(WAPIX_TAB_SHIPS,    active_tab == 3);
		this->SetWidgetLoweredState(WAPIX_TAB_WAGONS,   active_tab == 4);
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPIX_PANEL) return;

		const int LH = _LineH();
		const int EH = _EntryH();
		int start = vscroll ? vscroll->GetPosition() : 0;
		int visible = (r.bottom - r.top) / EH + 2;
		int y = r.top + 2;
		int xl = r.left + 6;
		int xr = r.right - 6;

		for (int idx = start; idx < (int)entries.size() && idx < start + visible; idx++) {
			if (y > r.bottom) break;
			const auto &e = entries[idx];

			/* Line 1: Name + unlock status */
			TextColour name_col = e.unlocked ? TC_GREEN : TC_RED;
			std::string status_str = e.unlocked ? " [UNLOCKED]" : " [LOCKED]";
			DrawString(xl, xr, y, e.name + status_str, name_col);
			y += LH;

			/* Line 2: Speed, cost, running cost */
			std::string line2 = fmt::format("Speed: {} km/h | Cost: {} | Running: {}/yr",
				e.speed, _APIdxMoney(e.cost), _APIdxMoney(e.running));
			DrawString(xl + 10, xr, y, line2, TC_WHITE);
			y += LH;

			/* Line 3: Cargo + capacity + extra */
			std::string line3;
			if (e.capacity > 0) {
				line3 = fmt::format("Cargo: {} ({}) | {}", e.cargo_name, e.capacity, e.extra);
			} else {
				line3 = fmt::format("Cargo: {} | {}", e.cargo_name, e.extra);
			}
			DrawString(xl + 10, xr, y, line3, TC_LIGHT_BLUE);
			y += LH;

			/* Line 4: Refit options */
			DrawString(xl + 10, xr, y, "Refit: " + e.refit_text, TC_FROMSTRING);
			y += LH + 4; /* extra 4px gap between entries */
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		switch (widget) {
			case WAPIX_TAB_TRAINS:   active_tab = 0; _BuildEntries(); this->SetDirty(); break;
			case WAPIX_TAB_ROAD:     active_tab = 1; _BuildEntries(); this->SetDirty(); break;
			case WAPIX_TAB_AIRCRAFT:  active_tab = 2; _BuildEntries(); this->SetDirty(); break;
			case WAPIX_TAB_SHIPS:    active_tab = 3; _BuildEntries(); this->SetDirty(); break;
			case WAPIX_TAB_WAGONS:   active_tab = 4; _BuildEntries(); this->SetDirty(); break;
		}
	}

	void OnResize() override
	{
		if (vscroll) {
			const NWidgetBase *panel = this->GetWidget<NWidgetBase>(WAPIX_PANEL);
			int visible = (panel ? (int)panel->current_y : 360) / _EntryH();
			vscroll->SetCapacity(std::max(1, visible));
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }
};

static WindowDesc _ap_index_desc(
	WDP_AUTO, {"ap_index"}, 480, 420,
	WC_ARCHIPELAGO_INDEX, WC_NONE, {},
	_nested_ap_index_widgets
);

void ShowArchipelagoIndexWindow()
{
	AllocateWindowDescFront<ArchipelagoIndexWindow>(_ap_index_desc, 0);
}

/* =========================================================================
 * COLBY EVENT WINDOW
 * Shows current step progress, delivered amount, and town name.
 * ========================================================================= */

enum APColbyWidgets : WidgetID {
	WAPCB_CAPTION,
	WAPCB_PANEL,
	WAPCB_SCROLLBAR,
	WAPCB_BTN_REOPEN,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_colby_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WAPCB_CAPTION), SetStringTip(STR_ARCHIPELAGO_COLBY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WAPCB_PANEL), SetMinimalSize(320, 100), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPCB_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPCB_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPCB_BTN_REOPEN), SetStringTip(STR_AP_COLBY_BTN_REOPEN), SetMinimalSize(320, 14), SetFill(1, 0), SetResize(1, 0),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
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
					int seconds_left = std::max(0, cs.escape_ticks / 4);
					int minutes_left = (seconds_left + 30) / 60;
					add("Colby has escaped!", TC_RED);
					if (minutes_left > 0)
						add(fmt::format("Awaiting capture in approximately {} minute{}...", minutes_left, minutes_left == 1 ? "" : "s"), TC_ORANGE);
					else
						add(fmt::format("Awaiting capture in {} seconds...", seconds_left), TC_ORANGE);
				} else {
					/* Cache link targets */
					_link_town_id    = cs.town_id;
					_link_stash_tile = cs.source_tile;

					add(fmt::format("Step {}/5", cs.step), TC_GOLD);

					/* Clickable destination town */
					if (cs.town_id != (TownID)UINT16_MAX) {
						_lines.push_back({
							fmt::format("\u25ba Destination: {} (click to locate)", cs.town_name),
							TC_LIGHT_BLUE, false, 0, LINK_TOWN});
					} else {
						add(fmt::format("Destination: {}", cs.town_name), TC_WHITE);
					}

					/* Clickable stash station */
					if (cs.source_tile != INVALID_TILE && !cs.source_name.empty()) {
						_lines.push_back({
							fmt::format("\u25ba Pick up cargo at: {} (click to locate)",
							            cs.source_name),
							TC_YELLOW, false, 0, LINK_STASH});
						_lines.push_back({
							fmt::format("  Refit vehicle to '{}'. Use 'Unload all' at {}.",
							            cs.cargo_name.empty() ? "Packages" :
							            (std::string(1, (char)std::toupper((unsigned char)cs.cargo_name[0])) + cs.cargo_name.substr(1)),
							            cs.town_name),
							TC_GREY, false, 0, LINK_NONE});
						_lines.push_back({
							"  Buses, mail trucks, and passenger vehicles can carry this cargo.",
							TC_GREY, false, 0, LINK_NONE});
					} else {
						add("Build a station near Colby's Stash to pick up cargo.", TC_GREY);
						add(fmt::format("Refit your vehicle to '{}' and deliver to {}.",
						    cs.cargo_name.empty() ? "Packages" :
						    (std::string(1, (char)std::toupper((unsigned char)cs.cargo_name[0])) + cs.cargo_name.substr(1)),
						    cs.town_name), TC_GREY);
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
		/* Show/hide Reopen button based on popup_pending state */
		ColbyStatus cs = AP_GetColbyStatus();
		NWidgetCore *btn = this->GetWidget<NWidgetCore>(WAPCB_BTN_REOPEN);
		if (btn != nullptr) {
			bool should_show = cs.popup_pending;
			if (btn->IsDisabled() != !should_show) {
				btn->SetDisabled(!should_show);
			}
		}
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
		if (widget == WAPCB_BTN_REOPEN) {
			AP_ColbyReopenPopup();
			return;
		}
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

/* =========================================================================
 * COLBY DECISION WINDOW — Custom popup replacing ShowQuery.
 * X button just dismisses (popup stays pending). Explicit buttons execute
 * the actual choice. This lets the player reopen the decision if they
 * accidentally close it.
 * ========================================================================= */

enum ColbyDecisionWidgets : WidgetID {
	WCBD_MESSAGE,
	WCBD_BTN_A,   /* Arrest / Imprison */
	WCBD_BTN_B,   /* Let Escape / Sacrifice */
};

static constexpr std::initializer_list<NWidgetPart> _nested_colby_decision_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_AP_COLBY_DECISION_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(10, 8, 10), SetPadding(0, 10, 0, 10),
			NWidget(WWT_TEXT, INVALID_COLOUR, WCBD_MESSAGE), SetMinimalSize(380, 40), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WCBD_BTN_A), SetMinimalSize(380, 14), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_RED,   WCBD_BTN_B), SetMinimalSize(380, 14), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _colby_decision_desc(
	WDP_CENTER, {}, 420, 140,
	WC_AP_COLBY_DECISION, WC_NONE, {},
	_nested_colby_decision_widgets
);

struct ColbyDecisionWindow : public Window {
	int popup_type; /* 1 = arrest/escape, 2 = imprison/sacrifice */

	ColbyDecisionWindow(WindowDesc &desc, int type) : Window(desc), popup_type(type)
	{
		this->InitNested(0);
		/* Set button strings based on popup type */
		if (popup_type == 1) {
			this->GetWidget<NWidgetCore>(WCBD_MESSAGE)->SetStringTip(STR_AP_COLBY_ARREST_QUERY, STR_NULL);
			this->GetWidget<NWidgetCore>(WCBD_BTN_A)->SetStringTip(STR_AP_COLBY_BTN_ARREST, STR_NULL);
			this->GetWidget<NWidgetCore>(WCBD_BTN_B)->SetStringTip(STR_AP_COLBY_BTN_ESCAPE, STR_NULL);
		} else {
			this->GetWidget<NWidgetCore>(WCBD_MESSAGE)->SetStringTip(STR_AP_COLBY_ESCAPE_QUERY, STR_NULL);
			this->GetWidget<NWidgetCore>(WCBD_BTN_A)->SetStringTip(STR_AP_COLBY_BTN_IMPRISON, STR_NULL);
			this->GetWidget<NWidgetCore>(WCBD_BTN_B)->SetStringTip(STR_AP_COLBY_BTN_SACRIFICE, STR_NULL);
		}
	}

	/* Close() does NOT execute any choice — the popup stays pending so
	 * the player can reopen it from the Colby status window. */
	void Close(int data = 0) override
	{
		/* Don't clear popup_pending — the decision can be reopened */
		this->Window::Close(data);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WCBD_BTN_A:
				if (popup_type == 1) {
					AP_ColbyDecisionArrest();
				} else {
					AP_ColbyDecisionImprison();
				}
				this->Close();
				break;
			case WCBD_BTN_B:
				if (popup_type == 1) {
					AP_ColbyDecisionEscape();
				} else {
					AP_ColbyDecisionSacrifice();
				}
				this->Close();
				break;
		}
	}
};

void ShowColbyDecisionWindow(int popup_type)
{
	CloseWindowByClass(WC_AP_COLBY_DECISION);
	new ColbyDecisionWindow(_colby_decision_desc, popup_type);
}

/* =========================================================================
 * DEMIGOD WINDOW — God of Wackens detail view
 * ========================================================================= */

enum APDemigodWidgets : WidgetID {
	WAPDG_CAPTION,
	WAPDG_PANEL,
	WAPDG_BTN_TRIBUTE,
	WAPDG_BTN_CLOSE,
	WAPDG_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_demigod_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WAPDG_CAPTION), SetStringTip(STR_ARCHIPELAGO_DEMIGOD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WAPDG_PANEL), SetMinimalSize(340, 120), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPDG_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WAPDG_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL), SetPIP(4, 4, 4),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPDG_BTN_TRIBUTE), SetStringTip(STR_ARCHIPELAGO_DEMIGOD_PAY_TRIBUTE), SetMinimalSize(200, 16), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,  WAPDG_BTN_CLOSE),   SetStringTip(STR_ARCHIPELAGO_DEMIGOD_CLOSE),      SetMinimalSize(80, 16), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
};

struct ArchipelagoDemigodWindow : public Window {
	Scrollbar *vscroll = nullptr;

	ArchipelagoDemigodWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WAPDG_SCROLLBAR);
		this->FinishInitNested(wnum);
	}

	void OnPaint() override {
		DemigodStatus ds = AP_GetDemigodStatus();
		/* Tribute button: enabled only when demigod is active and player can afford it.
		 * When friendly, dismiss is always free (tribute_cost=0), so always enabled. */
		bool can_pay = false;
		if (ds.active) {
			if (ds.friendly) {
				can_pay = true; /* free dismiss */
			} else {
				Company *c = Company::GetIfValid(_local_company);
				can_pay = (c != nullptr && c->money >= (Money)ds.tribute_cost);
			}
		}
		this->SetWidgetDisabledState(WAPDG_BTN_TRIBUTE, !ds.active || !can_pay);

		/* Swap button text depending on friendly state */
		if (ds.active && ds.friendly) {
			this->GetWidget<NWidgetCore>(WAPDG_BTN_TRIBUTE)->SetStringTip(STR_ARCHIPELAGO_DEMIGOD_DISMISS, {});
		} else {
			this->GetWidget<NWidgetCore>(WAPDG_BTN_TRIBUTE)->SetStringTip(STR_ARCHIPELAGO_DEMIGOD_PAY_TRIBUTE, {});
		}

		/* Scrollbar */
		int lines = 10;
		this->vscroll->SetCount(lines);
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (widget != WAPDG_PANEL) return;
		DemigodStatus ds = AP_GetDemigodStatus();
		int y = r.top + 4;
		int line_h = GetCharacterHeight(FS_NORMAL) + 2;

		/* Title line */
		DrawString(r.left + 4, r.right - 4, y, "The God of Wackens watches over this world.", TC_GOLD);
		y += line_h;
		y += line_h / 2; /* spacing */

		/* Defeated counter */
		std::string counter = fmt::format("Demigods defeated: {} / {}", ds.defeated_count, ds.total_count);
		DrawString(r.left + 4, r.right - 4, y, counter, TC_WHITE);
		y += line_h;
		y += line_h / 2;

		if (ds.active) {
			if (ds.friendly) {
				/* Friendly demigod info */
				DrawString(r.left + 4, r.right - 4, y, "ALLIED DEMIGOD:", TC_GREEN);
				y += line_h;

				DrawString(r.left + 8, r.right - 4, y, ds.active_name, TC_ORANGE);
				y += line_h;

				std::string theme_line = fmt::format("Theme: {}", ds.active_theme);
				DrawString(r.left + 8, r.right - 4, y, theme_line, TC_LIGHT_BLUE);
				y += line_h;

				DrawString(r.left + 8, r.right - 4, y, "This demigod is now your ally! Yearly dividends incoming.", TC_GREEN);
				y += line_h;
			} else {
				/* Hostile demigod info */
				DrawString(r.left + 4, r.right - 4, y, "ACTIVE DEMIGOD:", TC_RED);
				y += line_h;

				DrawString(r.left + 8, r.right - 4, y, ds.active_name, TC_ORANGE);
				y += line_h;

				std::string theme_line = fmt::format("Theme: {}", ds.active_theme);
				DrawString(r.left + 8, r.right - 4, y, theme_line, TC_LIGHT_BLUE);
				y += line_h;

				std::string cost_line = fmt::format("Tribute cost: {}", GetString(STR_JUST_CURRENCY_LONG, (int64_t)ds.tribute_cost));
				DrawString(r.left + 8, r.right - 4, y, cost_line, TC_YELLOW);
				y += line_h;
			}
		} else if (ds.next_spawn_year > 0 && ds.defeated_count < ds.total_count) {
			/* Next spawn countdown */
			std::string next = fmt::format("Next demigod arrives in year: {}", ds.next_spawn_year);
			DrawString(r.left + 4, r.right - 4, y, next, TC_GREY);
			y += line_h;
		} else if (ds.defeated_count >= ds.total_count && ds.total_count > 0) {
			DrawString(r.left + 4, r.right - 4, y, "All demigods have been vanquished!", TC_GREEN);
			y += line_h;
		} else {
			DrawString(r.left + 4, r.right - 4, y, "No demigods configured.", TC_GREY);
			y += line_h;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPDG_BTN_TRIBUTE: {
				DemigodStatus ds = AP_GetDemigodStatus();
				if (!ds.active) break;
				/* Call defeat directly — no confirmation dialog for now */
				AP_DemigodDefeat();
				this->SetDirty();
				break;
			}
			case WAPDG_BTN_CLOSE:
				this->Close();
				break;
		}
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		this->SetDirty(); /* refresh every tick to track money / active state */
	}
};

static WindowDesc _ap_demigod_desc(
	WDP_AUTO, {"ap_demigod"}, 360, 180,
	WC_ARCHIPELAGO_DEMIGOD, WC_NONE, {},
	_nested_ap_demigod_widgets
);

void ShowArchipelagoDemigodWindow()
{
	AllocateWindowDescFront<ArchipelagoDemigodWindow>(_ap_demigod_desc, 0);
}

/* ---------------------------------------------------------------------------
 * RUIN DETAIL WINDOW — industry-like view for AP ruins
 * Opened when the player clicks on a ruin tile.
 * -------------------------------------------------------------------------- */

enum APRuinWidgets : WidgetID {
	WAPRN_CAPTION,
	WAPRN_PANEL,
	WAPRN_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_ruin_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WAPRN_CAPTION), SetStringTip(STR_ARCHIPELAGO_RUIN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN, WAPRN_PANEL), SetMinimalSize(300, 140), SetResize(1, 1), SetFill(1, 1), SetScrollbar(WAPRN_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPRN_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 0), SetFill(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

struct ArchipelagoRuinWindow : public Window {
	Scrollbar *vscroll = nullptr;
	uint32_t ruin_tile;

	ArchipelagoRuinWindow(WindowDesc &desc, WindowNumber wnum, uint32_t tile) : Window(desc), ruin_tile(tile) {
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WAPRN_SCROLLBAR);
		this->FinishInitNested(wnum);
	}

	void OnPaint() override {
		this->vscroll->SetCount(12);
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (widget != WAPRN_PANEL) return;

		APRuinView rv;
		bool found = AP_GetRuinViewByTile(this->ruin_tile, rv);
		int y = r.top + 4;
		int line_h = GetCharacterHeight(FS_NORMAL) + 2;

		if (!found) {
			DrawString(r.left + 4, r.right - 4, y, "This ruin is no longer active.", TC_GREY);
			return;
		}

		/* Location name */
		DrawString(r.left + 4, r.right - 4, y, fmt::format("Location: {}", rv.location_name), TC_GOLD);
		y += line_h;

		/* Nearby town */
		DrawString(r.left + 4, r.right - 4, y, fmt::format("Near: {}", rv.town_name), TC_WHITE);
		y += line_h;
		y += line_h / 2;

		if (rv.completed) {
			DrawString(r.left + 4, r.right - 4, y, "This ruin has been cleansed!", TC_GREEN);
			y += line_h;
			DrawString(r.left + 4, r.right - 4, y, "The AP location check has been sent.", TC_GREEN);
			return;
		}

		/* Cargo requirements header */
		DrawString(r.left + 4, r.right - 4, y, "Cargo required to cleanse this ruin:", TC_ORANGE);
		y += line_h;
		y += line_h / 4;

		/* Cargo lines */
		for (const auto &cl : rv.cargo) {
			bool done = (cl.delivered >= cl.required);
			TextColour tc = done ? TC_GREEN : TC_YELLOW;
			std::string mark = done ? " (done!)" : "";
			std::string line = fmt::format("  {}: {} / {}{}", cl.name, cl.delivered, cl.required, mark);
			DrawString(r.left + 8, r.right - 4, y, line, tc);
			y += line_h;
		}

		y += line_h / 2;
		DrawString(r.left + 4, r.right - 4, y, "Deliver the required cargo to a nearby station.", TC_FROMSTRING);
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		this->SetDirty();
	}
};

static WindowDesc _ap_ruin_desc(
	WDP_AUTO, {"ap_ruin"}, 320, 180,
	WC_ARCHIPELAGO_RUIN, WC_NONE, {},
	_nested_ap_ruin_widgets
);

void ShowArchipelagoRuinWindow(uint32_t tile_index)
{
	/* Close any existing ruin window, then open for this tile */
	CloseWindowByClass(WC_ARCHIPELAGO_RUIN);
	new ArchipelagoRuinWindow(_ap_ruin_desc, 0, tile_index);
}

/* ---------------------------------------------------------------------------
 * AP Victory Screen — shown when AP goal is completed
 * Reuses the vanilla Tycoon end-screen sprites as background.
 * -------------------------------------------------------------------------- */

static constexpr std::initializer_list<NWidgetPart> _nested_ap_victory_widgets = {
	NWidget(WWT_PANEL, COLOUR_BROWN, 0), SetResize(1, 1), EndContainer(),
};

static WindowDesc _ap_victory_desc(
	WDP_MANUAL, {}, 0, 0,
	WC_ENDSCREEN, WC_NONE,
	{},
	_nested_ap_victory_widgets
);

struct APVictoryWindow : Window {

	APVictoryWindow() : Window(_ap_victory_desc)
	{
		this->InitNested();
		this->flags.Reset(WindowFlag::WhiteBorder);
		ResizeWindow(this, _screen.width - this->width, _screen.height - this->height);

		if (!_networking) Command<CMD_PAUSE>::Post(PauseMode::Normal, true);
		MarkWholeScreenDirty();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		if (!_networking) Command<CMD_PAUSE>::Post(PauseMode::Normal, false);
		this->Window::Close();
	}

	void OnClick([[maybe_unused]] Point pt, [[maybe_unused]] WidgetID widget, [[maybe_unused]] int click_count) override
	{
		this->Close();
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		if (IsQuitKey(keycode)) return ES_NOT_HANDLED;
		switch (keycode) {
			case WKC_RETURN: case WKC_ESC: case WKC_SPACE:
				this->Close();
				return ES_HANDLED;
			default:
				return ES_HANDLED;
		}
	}

	void OnPaint() override
	{
		/* Resize to full screen */
		if (this->width != _screen.width || this->height != _screen.height)
			ResizeWindow(this, _screen.width - this->width, _screen.height - this->height);

		this->DrawWidgets();

		/* Dark grey fill behind the background sprite */
		GfxFillRect(Rect{0, 0, this->width, this->height}, PixelColour{105}, FILLRECT_OPAQUE);

		/* The background sprite is 640px wide, split into 10 strips of 50px */
		const SpriteID bg = SPR_TYCOON_IMG2_BEGIN;
		Dimension dim = GetSpriteSize(bg);
		int total_h = dim.height * 96 / 10; /* 96% of 10×50px = 480px equivalent */

		int x0 = std::max(0, (_screen.width  / 2) - ((int)dim.width  / 2));
		int y0 = std::max(0, (_screen.height / 2) - (total_h / 2));

		GfxFillRect(x0, y0, x0 + (int)dim.width - 1, y0 + total_h - 1, PC_BLACK);
		for (uint i = 0; i < 10; i++) {
			DrawSprite(bg + i, PAL_NONE, x0, y0 + (int)(i * dim.height));
		}

		/* --- Build text lines --- */
		const int W  = ScaleSpriteTrad(640);
		const int tx = x0; /* left edge of sprite area */

		/* Line 1 — Title (large, gold) */
		DrawStringMultiLine(
			tx + ScaleSpriteTrad(20), tx + W - ScaleSpriteTrad(20),
			y0 + ScaleSpriteTrad(60), y0 + ScaleSpriteTrad(120),
			std::string("ARCHIPELAGO \xe2\x80\x94 GOAL COMPLETE!"),
			TC_GOLD, SA_CENTER);

		/* Line 2 — Player / company (white) */
		const std::string slot = _ap_last_slot.empty() ? "You" : _ap_last_slot;
		const std::string company_line = slot + "'s Transport has completed the Archipelago Multiworld!";
		DrawStringMultiLine(
			tx + ScaleSpriteTrad(30), tx + W - ScaleSpriteTrad(30),
			y0 + ScaleSpriteTrad(130), y0 + ScaleSpriteTrad(195),
			company_line,
			TC_WHITE, SA_CENTER);

		/* Line 3 — Stats */
		const int missions_done  = AP_GetTotalMissionsCompleted();
		const int missions_total = AP_GetSlotData().mission_count;
		const int64_t items_recv = AP_GetItemsReceivedCount();

		const std::string stats_line = fmt::format(
			"Missions completed: {} / {}     \xe2\x80\xa2     Items received: {}",
			missions_done, missions_total, items_recv);
		DrawStringMultiLine(
			tx + ScaleSpriteTrad(30), tx + W - ScaleSpriteTrad(30),
			y0 + ScaleSpriteTrad(210), y0 + ScaleSpriteTrad(265),
			stats_line,
			TC_LIGHT_BLUE, SA_CENTER);

		/* Line 4 — Year & server */
		const int start_year   = AP_GetSlotData().start_year;
		const int current_year = TimerGameCalendar::year.base();
		const int years_played = std::max(0, current_year - start_year);
		const std::string server_str = _ap_last_host.empty()
			? "local"
			: _ap_last_host + ":" + fmt::format("{}", _ap_last_port);

		const std::string footer_line = fmt::format(
			"Year {}  \xe2\x80\xa2  {} years in service  \xe2\x80\xa2  {}",
			current_year, years_played, server_str);
		DrawStringMultiLine(
			tx + ScaleSpriteTrad(30), tx + W - ScaleSpriteTrad(30),
			y0 + ScaleSpriteTrad(280), y0 + ScaleSpriteTrad(330),
			footer_line,
			TC_GREY, SA_CENTER);

		/* Dismiss hint */
		DrawString(
			tx + ScaleSpriteTrad(30), tx + W - ScaleSpriteTrad(30),
			y0 + ScaleSpriteTrad(380),
			std::string("Press Enter, Space or Escape to continue"),
			TC_GREY, SA_CENTER);
	}
};

void ShowAPVictoryScreen()
{
	CloseWindowByClass(WC_ENDSCREEN);
	new APVictoryWindow();
}

/* =========================================================================
 * AP START CHOICE WINDOW — 3-button dialog (Single Player / Host / Load)
 * ========================================================================= */

enum APStartChoiceWidgets : WidgetID {
	WAPSC_MESSAGE,
	WAPSC_BTN_SINGLE,
	WAPSC_BTN_HOST,
	WAPSC_BTN_LOAD,
	WAPSC_BTN_CANCEL,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_start_choice_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_ARCHIPELAGO_START_CHOICE_CAPTION_V2, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(8, 6, 8), SetPadding(8),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPSC_MESSAGE), SetStringTip(STR_ARCHIPELAGO_START_MESSAGE), SetMinimalSize(260, 14), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPSC_BTN_SINGLE), SetStringTip(STR_ARCHIPELAGO_START_SINGLE), SetMinimalSize(260, 14), SetFill(1, 0),
			/* Host Multiplayer button removed — use AP Bridge .exe instead */
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPSC_BTN_LOAD),   SetStringTip(STR_ARCHIPELAGO_START_LOAD),   SetMinimalSize(260, 14), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_RED,    WAPSC_BTN_CANCEL), SetStringTip(STR_ARCHIPELAGO_BTN_CLOSE),    SetMinimalSize(260, 14), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

struct APStartChoiceWindow : public Window {
	APStartChoiceWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested(0);
	}

	void Close(int data = 0) override
	{
		AP_SetWaitingForStartChoice(false);
		this->Window::Close(data);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WAPSC_BTN_SINGLE:
				/* Single Player — same as old "Yes" path */
				AP_SetWaitingForStartChoice(false);
				AP_ConsumeWorldStart();
				{
					uint32_t seed = AP_GetWorldSeed();
					IConsolePrint(CC_WHITE, fmt::format("[AP] Generating world (seed={})...", seed));
					_is_network_server = false;
					StartNewGameWithoutGUI(seed);
				}
				this->Close();
				break;

			/* WAPSC_BTN_HOST removed — multiplayer is now managed by the AP Bridge .exe */

			case WAPSC_BTN_LOAD:
				/* Load Save — same as old "No" path */
				AP_SetWaitingForStartChoice(false);
				AP_SetPendingLoadSave();
				ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD);
				this->Close();
				break;

			case WAPSC_BTN_CANCEL:
				this->Close();
				break;
		}
	}
};

static WindowDesc _ap_start_choice_desc(
	WDP_CENTER, {}, 300, 120,
	WC_AP_START_CHOICE, WC_NONE, {},
	_nested_ap_start_choice_widgets
);

void ShowAPStartChoiceWindow()
{
	CloseWindowByClass(WC_AP_START_CHOICE);
	new APStartChoiceWindow(_ap_start_choice_desc);
}

/* =========================================================================
 * AP JOIN MULTIPLAYER WINDOW — 4-field join: AP Server, Slot, Password, Game IP
 * ========================================================================= */

enum APJoinWidgets : WidgetID {
	WAPJOIN_LABEL_APSERVER,
	WAPJOIN_EDIT_APSERVER,
	WAPJOIN_LABEL_SLOT,
	WAPJOIN_EDIT_SLOT,
	WAPJOIN_LABEL_PASS,
	WAPJOIN_EDIT_PASS,
	WAPJOIN_LABEL_PLAYERNAME,
	WAPJOIN_EDIT_PLAYERNAME,
	WAPJOIN_LABEL_GAMESERVER,
	WAPJOIN_EDIT_GAMESERVER,
	WAPJOIN_BTN_JOIN,
	WAPJOIN_BTN_CANCEL,
	WAPJOIN_STATUS,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_join_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_ORANGE),
		NWidget(WWT_CAPTION, COLOUR_ORANGE), SetStringTip(STR_AP_JOIN_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_ORANGE),
		NWidget(WWT_STICKYBOX, COLOUR_ORANGE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_ORANGE),
		NWidget(NWID_VERTICAL), SetPIP(8, 4, 8), SetPadding(8),
			/* AP Server address */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPJOIN_LABEL_APSERVER), SetStringTip(STR_AP_JOIN_LABEL_APSERVER), SetMinimalSize(110, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPJOIN_EDIT_APSERVER), SetStringTip(STR_EMPTY), SetMinimalSize(220, 14), SetFill(1, 0),
			EndContainer(),
			/* Slot name */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPJOIN_LABEL_SLOT), SetStringTip(STR_AP_JOIN_LABEL_SLOT), SetMinimalSize(110, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPJOIN_EDIT_SLOT), SetStringTip(STR_EMPTY), SetMinimalSize(220, 14), SetFill(1, 0),
			EndContainer(),
			/* Password */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPJOIN_LABEL_PASS), SetStringTip(STR_AP_JOIN_LABEL_PASSWORD), SetMinimalSize(110, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPJOIN_EDIT_PASS), SetStringTip(STR_EMPTY), SetMinimalSize(220, 14), SetFill(1, 0),
			EndContainer(),
			/* Player name */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPJOIN_LABEL_PLAYERNAME), SetStringTip(STR_AP_JOIN_LABEL_PLAYERNAME), SetMinimalSize(110, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPJOIN_EDIT_PLAYERNAME), SetStringTip(STR_EMPTY), SetMinimalSize(220, 14), SetFill(1, 0),
			EndContainer(),
			/* Game server IP */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPJOIN_LABEL_GAMESERVER), SetStringTip(STR_AP_JOIN_LABEL_GAMESERVER), SetMinimalSize(110, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPJOIN_EDIT_GAMESERVER), SetStringTip(STR_EMPTY), SetMinimalSize(220, 14), SetFill(1, 0),
			EndContainer(),
			/* Status line */
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPJOIN_STATUS), SetMinimalSize(340, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			/* Join + Cancel buttons */
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPJOIN_BTN_JOIN),   SetStringTip(STR_AP_JOIN_BTN_CONNECT),    SetMinimalSize(120, 16), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_RED,   WAPJOIN_BTN_CANCEL), SetStringTip(STR_ARCHIPELAGO_BTN_CLOSE),  SetMinimalSize(120, 16), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct APJoinMultiplayerWindow : public Window {
	QueryString apserver_buf;
	QueryString slot_buf;
	QueryString pass_buf;
	QueryString playername_buf;
	QueryString gameserver_buf;
	std::string apserver_str, slot_str, pass_str, playername_str, gameserver_str;

	APJoinMultiplayerWindow(WindowDesc &desc) : Window(desc),
		apserver_buf(256), slot_buf(128), pass_buf(128), playername_buf(128), gameserver_buf(256)
	{
		this->InitNested(0);
		this->querystrings[WAPJOIN_EDIT_APSERVER]    = &this->apserver_buf;
		this->querystrings[WAPJOIN_EDIT_SLOT]         = &this->slot_buf;
		this->querystrings[WAPJOIN_EDIT_PASS]         = &this->pass_buf;
		this->querystrings[WAPJOIN_EDIT_PLAYERNAME]   = &this->playername_buf;
		this->querystrings[WAPJOIN_EDIT_GAMESERVER]   = &this->gameserver_buf;

		/* Pre-fill from last known AP connection if available */
		if (!_ap_last_host.empty()) {
			std::string full = _ap_last_host + ":" + fmt::format("{}", _ap_last_port);
			this->apserver_buf.text.Assign(full);
			this->apserver_str = full;
		} else {
			this->apserver_buf.text.Assign("archipelago.gg:38281");
			this->apserver_str = "archipelago.gg:38281";
		}
		if (!_ap_last_slot.empty()) {
			this->slot_buf.text.Assign(_ap_last_slot);
			this->slot_str = _ap_last_slot;
		}
		if (!_ap_last_pass.empty()) {
			this->pass_buf.text.Assign(_ap_last_pass);
			this->pass_str = _ap_last_pass;
		}
		/* Pre-fill player name from network settings or slot name */
		if (!_settings_client.network.client_name.empty() &&
		    _settings_client.network.client_name != "Player") {
			this->playername_buf.text.Assign(_settings_client.network.client_name);
			this->playername_str = _settings_client.network.client_name;
		} else if (!_ap_last_slot.empty()) {
			this->playername_buf.text.Assign(_ap_last_slot);
			this->playername_str = _ap_last_slot;
		} else {
			this->playername_buf.text.Assign("Player");
			this->playername_str = "Player";
		}
		this->gameserver_buf.text.Assign("127.0.0.1:3979");
		this->gameserver_str = "127.0.0.1:3979";

		this->SetFocusedWidget(WAPJOIN_EDIT_GAMESERVER);
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WAPJOIN_EDIT_APSERVER)    this->apserver_str    = this->apserver_buf.text.GetText();
		if (wid == WAPJOIN_EDIT_SLOT)        this->slot_str        = this->slot_buf.text.GetText();
		if (wid == WAPJOIN_EDIT_PASS)        this->pass_str        = this->pass_buf.text.GetText();
		if (wid == WAPJOIN_EDIT_PLAYERNAME)  this->playername_str  = this->playername_buf.text.GetText();
		if (wid == WAPJOIN_EDIT_GAMESERVER)  this->gameserver_str  = this->gameserver_buf.text.GetText();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget == WAPJOIN_BTN_CANCEL) {
			this->Close();
			return;
		}
		if (widget != WAPJOIN_BTN_JOIN) return;

		/* Validate fields */
		if (this->gameserver_str.empty() || this->slot_str.empty()) {
			ShowErrorMessage(GetEncodedString(STR_JUST_RAW_STRING,
				std::string("Please fill in at least Game Server and Slot name.")), {}, WL_ERROR);
			return;
		}

		/* ── Ensure AP client exists ── */
		if (_ap_client == nullptr) InitArchipelago();
		if (_ap_client == nullptr) return;

		/* ── Parse AP server address ── */
		std::string raw = this->apserver_str;
		if (raw.rfind("wss://", 0) == 0) raw = raw.substr(6);
		else if (raw.rfind("ws://", 0) == 0) raw = raw.substr(5);

		std::string host = raw;
		uint16_t port = 38281;
		auto colon = raw.rfind(':');
		if (colon != std::string::npos) {
			host = raw.substr(0, colon);
			int p = ParseInteger<int>(raw.substr(colon + 1)).value_or(0);
			if (p > 0 && p < 65536) port = (uint16_t)p;
		}

		_ap_last_host = host;
		_ap_last_port = port;
		_ap_last_slot = this->slot_str;
		_ap_last_pass = this->pass_str;
		AP_SaveConnectionConfig();

		/* ── Set player name ── */
		if (!this->playername_str.empty()) {
			_settings_client.network.client_name = this->playername_str;
		}

		/* ── Request deferred MP join ── */
		/* Connect to AP server first. When slot_data arrives, the AP
		 * manager will apply all NewGRFs + settings (just like for SP
		 * world generation), then auto-connect to the game server. */
		AP_RequestMPJoin(this->gameserver_str, this->slot_str);

		_ap_client->Connect(host, port, this->slot_str, this->pass_str, "OpenTTD", false);

		this->Close();
	}
};

static WindowDesc _ap_join_desc(
	WDP_CENTER, {}, 400, 160,
	WC_AP_JOIN_MULTIPLAYER, WC_NONE, {},
	_nested_ap_join_widgets
);

void ShowAPJoinMultiplayerWindow()
{
	CloseWindowByClass(WC_AP_JOIN_MULTIPLAYER);
	new APJoinMultiplayerWindow(_ap_join_desc);
}
