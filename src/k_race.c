// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_race.c
/// \brief Race Mode specific code.

#include "k_race.h"

#include "k_kart.h"
#include "k_battle.h"
#include "k_pwrlv.h"
#include "k_color.h"
#include "k_respawn.h"
#include "doomdef.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_random.h"
#include "p_local.h"
#include "p_slopes.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_local.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "z_zone.h"
#include "m_misc.h"
#include "m_cond.h"
#include "f_finale.h"
#include "lua_hud.h"	// For Lua hud checks
#include "lua_hook.h"	// For MobjDamage and ShouldDamage
#include "m_cheat.h"	// objectplacing
#include "p_spec.h"

#include "k_waypoint.h"
#include "k_bot.h"
#include "k_hud.h"


/*--------------------------------------------------
	UINT8 K_RaceLapCount(INT16 mapNum);

		See header file for description.
--------------------------------------------------*/

UINT8 K_RaceLapCount(INT16 mapNum)
{
	if (!(gametyperules & GTR_CIRCUIT))
	{
		// Not in Race mode
		return 0;
	}

	if (cv_numlaps.value == -1)
	{
		// Use map default
		return mapheaderinfo[mapNum]->numlaps;
	}

	return cv_numlaps.value;
}
