// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  random-item.c
/// \brief Random item boxes

#include "../doomdef.h"
#include "../doomstat.h"
#include "../k_objects.h"
#include "../g_game.h"
#include "../p_local.h"
#include "../r_defs.h"
#include "../k_battle.h"
#include "../m_random.h"
#include "../k_specialstage.h" // specialstageinfo
#include "../k_kart.h"

// Noire
#include "../noire/n_cvar.h"

#define FLOAT_HEIGHT ( 12 * FRACUNIT )
#define FLOAT_TIME ( 2 * TICRATE )
#define FLOAT_ANGLE ( ANGLE_MAX / FLOAT_TIME )

#define SCALE_LO ( FRACUNIT * 2 / 3 )
#define SCALE_HI ( FRACUNIT )
#define SCALE_TIME ( 5 * TICRATE / 2 )
#define SCALE_ANGLE ( ANGLE_MAX / SCALE_TIME )

#define item_vfxtimer(o) ((o)->cvmem)

#define PREVIEWFLAGS (RF_ADD|RF_TRANS30)

static player_t *GetItemBoxPlayer(mobj_t *mobj)
{
	fixed_t closest = INT32_MAX;
	player_t *player = NULL;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!(playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo) && !players[i].spectator))
		{
			continue;
		}

		// Always use normal item box rules -- could pass in "2" for fakes but they blend in better like this
		if (P_CanPickupItem(&players[i], 1))
		{
			// Check for players who can take this pickup, but won't be allowed to (antifarming)
			UINT8 mytype = (mobj->flags2 & MF2_BOSSDEAD) ? 2 : 1;
			if (P_IsPickupCheesy(&players[i], mytype))
				continue;

			fixed_t dist = P_AproxDistance(P_AproxDistance(
				players[i].mo->x - mobj->x,
				players[i].mo->y - mobj->y),
				players[i].mo->z - mobj->z
			);

			if (dist > 8192*mobj->scale)
			{
				continue;
			}

			if (dist < closest)
			{
				player = &players[i];
				closest = dist;
			}
		}
	}

	return player;
}

boolean Obj_RandomItemSpawnIn(mobj_t *mobj)
{
	// We don't want item spawnpoints to be visible during
	// POSITION in Battle.
	// battleprisons isn't set in time to do this on spawn. GROAN
	if ((mobj->flags2 & MF2_BOSSFLEE) && (gametyperules & (GTR_CIRCUIT|GTR_PAPERITEMS)) == GTR_PAPERITEMS && !battleprisons)
		mobj->renderflags |= RF_DONTDRAW;

	if ((leveltime == starttime) && !(gametyperules & GTR_CIRCUIT) && (mobj->flags2 & MF2_BOSSFLEE)) // here on map start?
	{
		if (gametyperules & GTR_PAPERITEMS)
		{
			if (battleprisons == true)
			{
				;
			}
			else
			{
				// Spawn a battle monitor in your place and Fucking Die
				mobj_t *paperspawner = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_PAPERITEMSPOT);
				paperspawner->spawnpoint = mobj->spawnpoint;
				mobj->spawnpoint->mobj = paperspawner;
				P_RemoveMobj(mobj);
				return false;
			}
		}

		// Clear "hologram" appearance if it was set in RandomItemSpawn.
		if ((gametyperules & GTR_CIRCUIT) != GTR_CIRCUIT)
			mobj->renderflags &= ~PREVIEWFLAGS;

		// poof into existance
		P_UnsetThingPosition(mobj);
		mobj->flags &= ~(MF_NOCLIPTHING|MF_NOBLOCKMAP);
		mobj->renderflags &= ~RF_DONTDRAW;
		P_SetThingPosition(mobj);
		P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_EXPLODE);
		nummapboxes++;
	}

	return true;
}

void Obj_RandomItemSpawn(mobj_t *mobj)
{
	item_vfxtimer(mobj) = P_RandomRange(PR_DECORATION, 0, SCALE_TIME - 1);

	if (leveltime == 0)
	{
		mobj->flags2 |= MF2_BOSSFLEE; // mark as here on map start
		if ((gametyperules & GTR_CIRCUIT) != GTR_CIRCUIT) // delayed
		{
			P_UnsetThingPosition(mobj);
			mobj->flags |= (MF_NOCLIPTHING|MF_NOBLOCKMAP);
			mobj->renderflags |= PREVIEWFLAGS;
			P_SetThingPosition(mobj);
		}
	}
}
