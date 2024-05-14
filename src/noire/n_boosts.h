// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by KartKrew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_BOOST__
#define __N_BOOST__

#include "../d_player.h"
#include "../doomdef.h"
#include "../doomtype.h"
#include "../g_game.h"
#include "../k_kart.h"
#include "../k_respawn.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../m_easing.h"
#include "../s_sound.h" //S_StartSound
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

fixed_t N_GetKartSpeed(player_t *player, boolean doboostpower);
fixed_t N_GetKartAccel(player_t *player);
void N_GetKartBoostPower(player_t *player);
fixed_t N_3dKartMovement(const player_t *player, fixed_t forwardmove);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
