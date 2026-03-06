/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#ifndef ARCHIPELAGO_GUI_H
#define ARCHIPELAGO_GUI_H

#include "archipelago.h"
#include <string>
#include <cstdint>
#include <atomic>

void ShowArchipelagoConnectWindow();
void ShowArchipelagoStatusWindow();
void ShowArchipelagoMissionsWindow();
void ShowArchipelagoShopWindow();
void AP_ShowConsole(const std::string &msg);

extern std::string _ap_last_host;
extern uint16_t    _ap_last_port;
extern std::string _ap_last_slot;
extern std::string _ap_last_pass;
extern bool        _ap_last_ssl;

extern std::atomic<bool> _ap_status_dirty;

/* Accessor functions from manager */
const APSlotData &AP_GetSlotData();
bool              AP_IsConnected();

/* World-start handshake — called from intro_gui.cpp ONLY.
 * StartNewGameWithoutGUI must never be called from inside a timer callback. */
void     EnsureHandlersRegistered();
void     AP_SendCheckByName(const std::string &location_name);
void     AP_SendDeath(const std::string &cause);  ///< Send Death Link event (train/road crash)
void     AP_NotifyShopPurchased();     ///< Call when player buys any shop item (triggers shop-purchase missions)
int      AP_GetShopSlots();
int      AP_GetShopRefreshDays();
int      AP_GetShopPageOffset();   ///< Current page offset for shop slot rotation
std::string AP_GetShopLocationLabel(const std::string &location_name);
int64_t  AP_GetShopPrice(const std::string &location_name);
bool     AP_CanAffordShopItem(const std::string &location_name);
bool     AP_IsShopLocationSent(const std::string &location_name);
void     AP_DeductShopPrice(const std::string &location_name);
bool     AP_ShouldStartWorld();
void     AP_ConsumeWorldStart();   /* applies settings, clears flag */
uint32_t AP_GetWorldSeed();        /* seed to pass to StartNewGameWithoutGUI */

#endif /* ARCHIPELAGO_GUI_H */

/// Returns true while the 2x cargo payment bonus is active (used by economy.cpp)
bool AP_GetCargoBonusActive();
