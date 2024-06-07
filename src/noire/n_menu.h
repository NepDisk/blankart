// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_MENU__
#define __N_MENU__

#include "../m_menu.h"
#include "../m_cond.h"
#include "../command.h"
#include "../console.h"
#include "../g_state.h" //For the tripwire toggle

// Noire
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

extern menuitem_t OPTIONS_NoireGameplay[];
extern menuitem_t OPTIONS_NoireGameplayRings[];
extern menuitem_t OPTIONS_NoireGameplayItems[];
extern menuitem_t OPTIONS_NoireGameplayMechanics[];
extern menuitem_t OPTIONS_NoireGameplayLives[];
extern menuitem_t OPTIONS_NoireGameplayBots[];
extern menuitem_t OPTIONS_NoireGameplayRivals[];
extern menu_t OPTIONS_NoireGameplayDef;
extern menu_t OPTIONS_NoireGameplayRingsDef;
extern menu_t OPTIONS_NoireGameplayItemsDef;
extern menu_t OPTIONS_NoireGameplayMechanicsDef;
extern menu_t OPTIONS_NoireGameplayLivesDef;
extern menu_t OPTIONS_NoireGameplayBotsDef;
extern menu_t OPTIONS_NoireGameplayRivalsDef;

extern menuitem_t OPTIONS_Noire[];
extern menu_t OPTIONS_NoireDef;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
