// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_OBJECT__
#define __N_OBJECT__

#include "../../p_local.h"
#include "../../k_kart.h"
#include "../../g_game.h"
#include "../../r_main.h"
#include "../../s_sound.h"


#ifdef __cplusplus
extern "C" {
#endif

player_t *K_FindOldJawzTarget(mobj_t *actor, player_t *source);
void OBJ_JawzOldThink(mobj_t *actor);
void Obj_JawzOldThrown(mobj_t *th, fixed_t finalSpeed, SINT8 dir);
void Obj_OrbinautOldThrown(mobj_t *th, fixed_t finalSpeed, SINT8 dir);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
