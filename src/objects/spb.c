// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  spb.c
/// \brief Self Propelled Bomb item code.

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
#include "../k_specialstage.h"


// These are not the OG functions, these were ported from v1
void Obj_SPBThink(mobj_t *mobj)
{
	K_SetItemCooldown(KITEM_SPB,20*TICRATE);

	mobj_t *ghost = P_SpawnGhostMobj(mobj);
    ghost->fuse = 3;
    if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
    {
        ghost->color = mobj->target->player->skincolor;
        ghost->colorized = true;
    }
    if (mobj->threshold > 0)
        mobj->threshold--;
}


void Obj_SPBTouch(mobj_t *spb, mobj_t *toucher)
{

	if ((spb->target == toucher || spb->target == toucher->target) && (spb->threshold > 0))
		return;

	if (spb->health <= 0 || spb->health <= 0)
		return;

	if (toucher->player && toucher->player->spectator)
		return;

	if (spb->tracer && !P_MobjWasRemoved(spb->tracer) && toucher == spb->tracer)
	{
		mobj_t *spbexplode;

		if (toucher->player->invincibilitytimer > 0 || toucher->player->growshrinktimer > 0 || toucher->player->hyudorotimer > 0)
		{
			//player->powers[pw_flashing] = 0;
			K_DropHnextList(toucher->player);
			K_StripItems(toucher->player);
		}

		S_StopSound(spb); // Don't continue playing the gurgle or the siren

		spbexplode = P_SpawnMobj(toucher->x, toucher->y, toucher->z, MT_SPBEXPLOSION);
		spbexplode->extravalue1 = 1; // Tell K_ExplodePlayer to use extra knockback
		if (spb->target && !P_MobjWasRemoved(spb->target))
			P_SetTarget(&spbexplode->target, spb->target);

		P_RemoveMobj(spb);
	}
	else
		P_DamageMobj(toucher, spb->target, 0, 1, DMG_NORMAL);
}
