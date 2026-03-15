/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file statusbar_widget.h Types related to the statusbar widgets. */

#ifndef WIDGETS_STATUSBAR_WIDGET_H
#define WIDGETS_STATUSBAR_WIDGET_H

/** Widgets of the #StatusBarWindow class. */
enum StatusbarWidgets : WidgetID {
	WID_S_LEFT,          ///< Left part of the statusbar; date is shown there.
	WID_S_MIDDLE,        ///< Middle part; current news or company name or *** SAVING *** or *** PAUSED ***.
	WID_S_RIGHT,         ///< Right part; bank balance.
	WID_S_AP_MESSAGES,   ///< AP message log (top-left panel).
	WID_S_AP_STATS,      ///< AP stats: checks, hints (top-right panel).
	/* AP button bar */
	WID_S_AP_NEWS_OFF,   ///< News filter: Off
	WID_S_AP_NEWS_SELF,  ///< News filter: Self only
	WID_S_AP_NEWS_ALL,   ///< News filter: All
	WID_S_AP_BTN_MISSIONS,
	WID_S_AP_BTN_DEMIGODS,
	WID_S_AP_BTN_RUINS,
	WID_S_AP_BTN_EVENTS,
	WID_S_AP_BTN_SHOP,
	WID_S_AP_BTN_GUIDE,
	WID_S_AP_BTN_INDEX,
};

#endif /* WIDGETS_STATUSBAR_WIDGET_H */
