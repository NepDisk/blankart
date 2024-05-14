// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_CHECK__
#define __N_CHECK__

#include "../doomdef.h"
#include "../g_game.h"
#include "../p_tick.h"
#include "../p_maputl.h"
#include "../r_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void K_KartLegacyUpdatePosition(player_t *player);
mobj_t *P_GetObjectTypeInSectorNum(mobjtype_t type, size_t s);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
