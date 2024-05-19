// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  jawz.c
/// \brief Jawz item code.

#include "../doomdef.h"
#include "../doomstat.h"
#include "../info.h"
#include "../k_kart.h"
#include "../k_objects.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h"
#include "../g_game.h"
#include "../z_zone.h"
#include "../k_waypoint.h"
#include "../k_respawn.h"
#include "../k_collide.h"
#include "../k_specialstage.h"

#define MAX_JAWZ_TURN (ANGLE_90 / 15) // We can turn a maximum of 6 degrees per frame at regular max speed

#define jawz_speed(o) ((o)->movefactor)
#define jawz_selfdelay(o) ((o)->threshold)
#define jawz_dropped(o) ((o)->flags2 & MF2_AMBUSH)
#define jawz_droptime(o) ((o)->movecount)

#define jawz_retcolor(o) ((o)->cvmem)
#define jawz_stillturn(o) ((o)->cusval)

#define jawz_owner(o) ((o)->target)
#define jawz_chase(o) ((o)->tracer)

