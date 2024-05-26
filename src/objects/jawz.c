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

void Obj_JawzThink(mobj_t *mobj)
{
	sector_t *sec2;
	fixed_t topspeed = mobj->movefactor;
	fixed_t distbarrier = 512*mapobjectscale;
	fixed_t distaway;

	P_SpawnGhostMobj(mobj);

	if (mobj->threshold > 0)
		mobj->threshold--;
	if (leveltime % TICRATE == 0)
		S_StartSound(mobj, mobj->info->activesound);

	if (gamespeed == 0)
		distbarrier = FixedMul(distbarrier, FRACUNIT-FRACUNIT/4);
	else if (gamespeed == 2)
		distbarrier = FixedMul(distbarrier, FRACUNIT+FRACUNIT/4);

	if ((gametyperules & GTR_CIRCUIT) && mobj->tracer)
	{
		distaway = P_AproxDistance(mobj->tracer->x - mobj->x, mobj->tracer->y - mobj->y);
		if (distaway < distbarrier)
		{
			if (mobj->tracer->player)
			{
				fixed_t speeddifference = abs(topspeed - min(mobj->tracer->player->speed, K_GetKartSpeed(mobj->tracer->player, false, false)));
				topspeed = topspeed - FixedMul(speeddifference, FRACUNIT-FixedDiv(distaway, distbarrier));
			}
		}
	}

	if (gametyperules & GTR_BUMPERS)
	{
		mobj->friction -= 1228;
		if (mobj->friction > FRACUNIT)
			mobj->friction = FRACUNIT;
		if (mobj->friction < 0)
			mobj->friction = 0;
	}

	mobj->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy);
	P_InstaThrust(mobj, mobj->angle, topspeed);

	if (mobj->tracer)
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->tracer->x, mobj->tracer->y);
	else
		mobj->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy);

	K_DriftDustHandling(mobj);

	/*sec2 = P_ThingOnSpecial3DFloor(mobj);
	if ((sec2 && GETSECSPECIAL(sec2->special, 3) == 1)
		|| (P_IsObjectOnRealGround(mobj, mobj->subsector->sector)
		&& GETSECSPECIAL(mobj->subsector->sector->special, 3) == 1))
		K_DoPogoSpring(mobj, 0, 1);*/
}

void Obj_JawzDudThink(mobj_t *mobj)
{
	boolean grounded = P_IsObjectOnGround(mobj);
	if (mobj->flags2 & MF2_AMBUSH)
	{
		if (grounded && (mobj->flags & MF_NOCLIPTHING))
		{
			mobj->momx = 1;
			mobj->momy = 0;
			S_StartSound(mobj, mobj->info->deathsound);
			mobj->flags &= ~MF_NOCLIPTHING;
		}
	}
	else
	{
		P_SpawnGhostMobj(mobj);
		mobj->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy);
		P_InstaThrust(mobj, mobj->angle, mobj->movefactor);

		if (grounded)
		{
			/*sector_t *sec2 = P_ThingOnSpecial3DFloor(mobj);
			if ((sec2 && GETSECSPECIAL(sec2->special, 3) == 1)
				|| (P_IsObjectOnRealGround(mobj, mobj->subsector->sector)
				&& GETSECSPECIAL(mobj->subsector->sector->special, 3) == 1))
				K_DoPogoSpring(mobj, 0, 1);*/
		}

		if (mobj->threshold > 0)
			mobj->threshold--;

		if (leveltime % TICRATE == 0)
			S_StartSound(mobj, mobj->info->activesound);
	}

}

player_t *K_FindJawzTarget(mobj_t *actor, player_t *source)
{
	fixed_t best = -1;
	player_t *wtarg = NULL;
	INT32 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		angle_t thisang;
		player_t *player;

		if (!playeringame[i])
			continue;

		player = &players[i];

		if (player->spectator)
			continue; // spectator

		if (!player->mo)
			continue;

		if (player->mo->health <= 0)
			continue; // dead

		// Don't target yourself, stupid.
		if (player == source)
			continue;

		// Don't home in on teammates.
		if (G_GametypeHasTeams() && source->ctfteam == player->ctfteam)
			continue;

		// Invisible, don't bother
		if (player->hyudorotimer)
			continue;

		// Find the angle, see who's got the best.
		thisang = actor->angle - R_PointToAngle2(actor->x, actor->y, player->mo->x, player->mo->y);
		if (thisang > ANGLE_180)
			thisang = InvAngle(thisang);

		// Jawz only go after the person directly ahead of you in race... sort of literally now!
		if (gametyperules & GTR_CIRCUIT)
		{
			// Don't go for people who are behind you
			if (thisang > ANGLE_67h)
				continue;
			// Don't pay attention to people who aren't above your position
			if (player->position >= source->position)
				continue;
			if ((best == -1) || (player->position > best))
			{
				wtarg = player;
				best = player->position;
			}
		}
		else if (gametyperules & GTR_BUMPERS)
		{
			fixed_t thisdist;
			fixed_t thisavg;

			// Don't go for people who are behind you
			if (thisang > ANGLE_45)
				continue;

			// Don't pay attention to dead players
			/*if (player->bumpers <= 0)
				continue;*/

			// Z pos too high/low
			if (abs(player->mo->z - (actor->z + actor->momz)) > RING_DIST/8)
				continue;

			thisdist = P_AproxDistance(player->mo->x - (actor->x + actor->momx), player->mo->y - (actor->y + actor->momy));

			if (thisdist > 2*RING_DIST) // Don't go for people who are too far away
				continue;

			thisavg = (AngleFixed(thisang) + thisdist) / 2;

			//CONS_Printf("got avg %d from player # %d\n", thisavg>>FRACBITS, i);

			if ((best == -1) || (thisavg < best))
			{
				wtarg = player;
				best = thisavg;
			}
		}
	}

	return wtarg;
}
