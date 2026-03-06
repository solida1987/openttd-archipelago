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
#include "querystring_gui.h"
#include "fontcache.h"
#include "currency.h"
#include "newgrf_config.h"
#include "table/strings.h"
#include "table/sprites.h"
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

	static std::string WinCondName(APWinCondition wc) {
		switch (wc) {
			case APWinCondition::COMPANY_VALUE:   return "Company Value";
			case APWinCondition::TOWN_POPULATION: return "Town Population";
			case APWinCondition::VEHICLE_COUNT:   return "Vehicle Count";
			case APWinCondition::CARGO_DELIVERED: return "Cargo Delivered";
			case APWinCondition::MONTHLY_PROFIT:  return "Monthly Profit";
			default:                              return "Unknown";
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
		return "Win: " + WinCondName(sd.win_condition) +
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
	WAPST_BTN_RECONNECT,
	WAPST_BTN_MISSIONS,
	WAPST_BTN_SETTINGS,
	WAPST_BTN_SHOP,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_status_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_ARCHIPELAGO_STATUS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_VERTICAL), SetPIP(2, 2, 2), SetPadding(4),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_STATUS_LINE), SetMinimalSize(220, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_GOAL_LINE),   SetMinimalSize(220, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPST_BTN_RECONNECT), SetStringTip(STR_ARCHIPELAGO_BTN_RECONNECT), SetMinimalSize(70, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPST_BTN_MISSIONS),  SetStringTip(STR_ARCHIPELAGO_BTN_MISSIONS),  SetMinimalSize(70, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPST_BTN_SETTINGS),  SetStringTip(STR_ARCHIPELAGO_BTN_SETTINGS),  SetMinimalSize(70, 14),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPST_BTN_SHOP),      SetStringTip(STR_ARCHIPELAGO_BTN_SHOP),      SetMinimalSize(225, 14), SetFill(1, 0),
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
		const char *wc = "?";
		switch (sd.win_condition) {
			case APWinCondition::COMPANY_VALUE:   wc = "Company Value"; break;
			case APWinCondition::TOWN_POPULATION: wc = "Population";    break;
			case APWinCondition::VEHICLE_COUNT:   wc = "Vehicles";      break;
			case APWinCondition::CARGO_DELIVERED: wc = "Cargo";         break;
			case APWinCondition::MONTHLY_PROFIT:  wc = "Profit";        break;
		}
		return fmt::format("Goal: {} >= {}", wc, sd.win_condition_value);
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
		}
	}

	void OnPaint() override {
		bool disconnected = (_ap_client == nullptr ||
		    _ap_client->GetState() == APState::DISCONNECTED ||
		    _ap_client->GetState() == APState::AP_ERROR);
		this->SetWidgetDisabledState(WAPST_BTN_RECONNECT, !disconnected || _ap_last_host.empty());
		this->SetWidgetDisabledState(WAPST_BTN_MISSIONS, !AP_IsConnected());
		this->SetWidgetDisabledState(WAPST_BTN_SHOP,     !AP_IsConnected());
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
			case WAPST_BTN_SHOP:
				ShowArchipelagoShopWindow();
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
	WAPM_FILTER_PANEL,
	WAPM_FILTER_ALL,
	WAPM_FILTER_EASY,
	WAPM_FILTER_MEDIUM,
	WAPM_FILTER_HARD,
	WAPM_FILTER_EXTREME,
	WAPM_SCROLLBAR,
	WAPM_LIST,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_missions_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_MISSIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		/* Filter row */
		NWidget(NWID_HORIZONTAL), SetPIP(2, 2, 2), SetPadding(2),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPM_FILTER_ALL),     SetStringTip(STR_ARCHIPELAGO_FILTER_ALL),     SetMinimalSize(50, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPM_FILTER_EASY),    SetStringTip(STR_ARCHIPELAGO_FILTER_EASY),    SetMinimalSize(50, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPM_FILTER_MEDIUM),  SetStringTip(STR_ARCHIPELAGO_FILTER_MEDIUM),  SetMinimalSize(50, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPM_FILTER_HARD),    SetStringTip(STR_ARCHIPELAGO_FILTER_HARD),    SetMinimalSize(50, 14),
			NWidget(WWT_PUSHTXTBTN, COLOUR_RED,    WAPM_FILTER_EXTREME), SetStringTip(STR_ARCHIPELAGO_FILTER_EXTREME), SetMinimalSize(50, 14),
		EndContainer(),
		/* Mission list + scrollbar */
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WAPM_LIST), SetMinimalSize(460, 300), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPM_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPM_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 0), EndContainer(),
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
	std::string   filter        = "all";  /* "all","easy","medium","hard","extreme" */
	std::vector<const APMission *> visible_missions;
	Scrollbar *scrollbar = nullptr;

	void RebuildVisibleList() {
		visible_missions.clear();
		const APSlotData &sd = AP_GetSlotData();
		for (const APMission &m : sd.missions) {
			if (filter == "all" || m.difficulty == filter)
				visible_missions.push_back(&m);
		}
		if (this->scrollbar) {
			this->scrollbar->SetCount((int)visible_missions.size());
		}
		this->SetDirty();
	}

	ArchipelagoMissionsWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3; /* +3px vertical padding */
		this->CreateNestedTree();
		this->scrollbar = this->GetScrollbar(WAPM_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		RebuildVisibleList();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		if (_ap_status_dirty.load()) RebuildVisibleList();
	}

	void SetFilterButton(const std::string &f) {
		filter = f;
		/* Push/depress buttons visually */
		this->SetWidgetLoweredState(WAPM_FILTER_ALL,     f == "all");
		this->SetWidgetLoweredState(WAPM_FILTER_EASY,    f == "easy");
		this->SetWidgetLoweredState(WAPM_FILTER_MEDIUM,  f == "medium");
		this->SetWidgetLoweredState(WAPM_FILTER_HARD,    f == "hard");
		this->SetWidgetLoweredState(WAPM_FILTER_EXTREME, f == "extreme");
		RebuildVisibleList();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPM_FILTER_ALL:     SetFilterButton("all");     break;
			case WAPM_FILTER_EASY:    SetFilterButton("easy");    break;
			case WAPM_FILTER_MEDIUM:  SetFilterButton("medium");  break;
			case WAPM_FILTER_HARD:    SetFilterButton("hard");    break;
			case WAPM_FILTER_EXTREME: SetFilterButton("extreme"); break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (widget != WAPM_LIST) return;

		/* Recompute row height at draw time — GetCharacterHeight returns the
		 * correct scaled value for the current UI zoom level.  Using the cached
		 * constructor value causes rows to overlap at UI scale >=2. */
		int rh = GetCharacterHeight(FS_NORMAL) + 3;

		int y = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / rh) + 1;

		for (int i = first; i < last && i < (int)visible_missions.size(); i++) {
			const APMission *m = visible_missions[i];

			/* Difficulty colour */
			TextColour tc = TC_GREY;
			if      (m->difficulty == "easy")    tc = TC_GREEN;
			else if (m->difficulty == "medium")  tc = TC_YELLOW;
			else if (m->difficulty == "hard")    tc = TC_ORANGE;
			else if (m->difficulty == "extreme") tc = TC_RED;

			if (m->completed) tc = TC_DARK_GREEN;

			/* Format: [X] Easy - Description  (current/target) */
			std::string prefix = m->completed ? "[X] " : "[ ] ";
			std::string cap_diff = m->difficulty.empty() ? "" :
				std::string(1, (char)toupper((unsigned char)m->difficulty[0])) + m->difficulty.substr(1);

			/* Build progress string for incomplete missions */
			std::string progress_str;
			if (!m->completed && m->amount > 0 && m->current_value > 0) {
				/* Detect money missions by unit */
				bool is_money = (m->unit == "\xC2\xA3" || m->unit == "£" ||
				                 m->unit.find("/month") != std::string::npos ||
				                 m->unit.find("month") != std::string::npos);
				if (is_money) {
					progress_str = fmt::format("  ({}/{})",
					    AP_FormatMoneyCompact(m->current_value),
					    AP_FormatMoneyCompact(m->amount));
				} else {
					auto fmt_num = [](int64_t v) -> std::string {
						if (v >= 1000000) return fmt::format("{:.1f}M", v / 1000000.0);
						if (v >= 1000)    return fmt::format("{}k", v / 1000);
						return fmt::format("{}", v);
					};
					progress_str = fmt::format("  ({}/{})", fmt_num(m->current_value), fmt_num(m->amount));
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

			std::string line = prefix + cap_diff + " - " + desc + progress_str;

			DrawString(r.left + 4, r.right - 4, y, line, tc);
			y += rh;
			if (y > r.bottom) break;
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override {
		this->SetDirty();
	}

	void OnResize() override {
		if (this->scrollbar) {
			int rh = GetCharacterHeight(FS_NORMAL) + 3;
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPM_LIST)->current_y / rh);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPM_LIST) {
			resize.height = row_height;
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
	WAPSH_BTN_BUY,
	WAPSH_BTN_CLOSE,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_shop_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_CREAM),
		NWidget(WWT_CAPTION, COLOUR_CREAM), SetStringTip(STR_ARCHIPELAGO_SHOP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_CREAM),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_CREAM, WAPSH_LIST), SetMinimalSize(380, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPSH_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_CREAM, WAPSH_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPIP(4, 4, 4), SetPadding(4),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPSH_BTN_BUY),   SetStringTip(STR_ARCHIPELAGO_SHOP_BUY,   STR_EMPTY), SetMinimalSize(120, 16), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,  WAPSH_BTN_CLOSE), SetStringTip(STR_ARCHIPELAGO_BTN_CLOSE, STR_EMPTY), SetMinimalSize(80,  16),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_CREAM), SetFill(1, 0), EndContainer(),
			NWidget(WWT_RESIZEBOX, COLOUR_CREAM),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoShopWindow : public Window {
	int row_height = 0;      /* computed in constructor from font height */
	int selected   = -1;
	Scrollbar *scrollbar = nullptr;

	/* Shop items: (location_name, label, price) */
	struct ShopEntry {
		std::string location_name;
		std::string label;   /* "player (game)" or pending */
		int64_t     price;
	};
	std::vector<ShopEntry> shop_items;

	void RebuildShopList()
	{
		shop_items.clear();
		const APSlotData &sd = AP_GetSlotData();
		int slots       = std::max(1, sd.shop_slots);
		int total       = slots * 20;               /* full pool */
		int page_offset = AP_GetShopPageOffset();   /* rotates every refresh */

		/* Scan the full pool (wrapping around) until we have exactly shop_slots
		 * items, or the pool is exhausted. This ensures the shop always shows
		 * the configured number of slots even after some items are purchased. */
		int found = 0;
		for (int n = 0; n < total && found < slots; n++) {
			int idx = (page_offset + n) % total + 1; /* 1-based location index */
			std::string loc = fmt::format("Shop_Purchase_{:04d}", idx);
			/* Skip slots already sent to the AP server this session */
			if (AP_IsShopLocationSent(loc)) continue;
			std::string label = AP_GetShopLocationLabel(loc);
			if (label.empty()) label = fmt::format("Slot #{} (loading...)", idx);
			int64_t price = AP_GetShopPrice(loc);
			shop_items.push_back({loc, label, price});
			found++;
		}
		/* Sort ascending by price so cheapest items are always at the top. */
		std::sort(shop_items.begin(), shop_items.end(),
		    [](const ShopEntry &a, const ShopEntry &b) { return a.price < b.price; });

		if (this->scrollbar) this->scrollbar->SetCount((int)shop_items.size());
		this->SetDirty();
	}

	ArchipelagoShopWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3; /* +3px vertical padding */
		this->CreateNestedTree();
		this->scrollbar = this->GetScrollbar(WAPSH_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		RebuildShopList();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		/* Refresh labels once hints arrive from server */
		bool any_loading = false;
		for (auto &entry : shop_items) {
			if (entry.label.find("loading") != std::string::npos) {
				any_loading = true;
				break;
			}
		}
		if (any_loading) RebuildShopList();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPSH_LIST) return;
		int y     = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / row_height) + 1;
		for (int i = first; i < last && i < (int)shop_items.size(); i++) {
			bool can_afford = AP_CanAffordShopItem(shop_items[i].location_name);
			TextColour tc;
			if (i == selected)   tc = TC_WHITE;
			else if (can_afford) tc = TC_YELLOW;
			else                 tc = TC_GREY;

			std::string price_str = AP_FormatMoneyCompact(shop_items[i].price);
			std::string line = fmt::format("[{}] {} — {}", i + 1, shop_items[i].label, price_str);
			DrawString(r.left + 4, r.right - 4, y, line, tc);
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

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }

	void OnResize() override
	{
		if (this->scrollbar) {
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPSH_LIST)->current_y / row_height);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPSH_LIST) {
			resize.height = row_height;
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


