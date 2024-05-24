// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_CONTROL__
#define __N_CONTROL__

#include "../d_player.h"
#include "../doomdef.h"
#include "../g_game.h"
#include "../k_kart.h"
#include "../k_respawn.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h" //S_StartSound
#include "../k_objects.h"
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

boolean K_PlayerCanTurn(const player_t* player);
void KV1_UpdatePlayerAngle(player_t* player);
void P_UpdateBotAngle(player_t *player);
INT16 K_GetKartTurnValue(const player_t *player, INT16 turnvalue);
void N_DoPogoSpring(mobj_t* mo, fixed_t vertispeed, UINT8 sound);
void N_PogoSidemove(player_t *player);
void N_LegacyStart(player_t *player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
