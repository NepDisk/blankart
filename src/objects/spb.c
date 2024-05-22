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

#define SPB_SLIPTIDEDELTA (ANG1 * 3)
#define SPB_STEERDELTA (ANGLE_90 - ANG10)
#define SPB_DEFAULTSPEED (FixedMul(mapobjectscale, K_GetKartSpeedFromStat(9) * 2))
#define SPB_ACTIVEDIST (1024 * FRACUNIT)

#define SPB_HOTPOTATO (2*TICRATE)
#define SPB_MAXSWAPS (2)
#define SPB_FLASHING (TICRATE)

#define SPB_CHASETIMESCALE (60*TICRATE)
#define SPB_CHASETIMEMUL (3*FRACUNIT)

#define SPB_SEEKTURN (FRACUNIT/8)
#define SPB_CHASETURN (FRACUNIT/4)

#define SPB_MANTA_SPACING (2750 * FRACUNIT)

#define SPB_MANTA_VSTART (150)
#define SPB_MANTA_VRATE (60)
#define SPB_MANTA_VMAX (100)

enum
{
	SPB_MODE_SEEK,
	SPB_MODE_CHASE,
	SPB_MODE_WAIT,
};

#define spb_mode(o) ((o)->extravalue1)
#define spb_modetimer(o) ((o)->extravalue2)

#define spb_nothink(o) ((o)->threshold)
#define spb_intangible(o) ((o)->cvmem)

#define spb_lastplayer(o) ((o)->lastlook)
#define spb_speed(o) ((o)->movefactor)
#define spb_pitch(o) ((o)->movedir)

#define spb_chasetime(o) ((o)->watertop) // running out of variables here...
#define spb_swapcount(o) ((o)->health)

#define spb_curwaypoint(o) ((o)->cusval)

#define spb_manta_vscale(o) ((o)->movecount)
#define spb_manta_totaldist(o) ((o)->reactiontime)

#define spb_owner(o) ((o)->target)
#define spb_chase(o) ((o)->tracer)

void Obj_SPBEradicateCapsules(void)
{
	thinker_t *think;
	mobj_t *mo;

	// Expensive operation :D?
	for (think = thlist[THINK_MOBJ].next; think != &thlist[THINK_MOBJ]; think = think->next)
	{
		if (think->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mo = (mobj_t *)think;

		if (mo->type != MT_ITEMCAPSULE)
			continue;

		if (!mo->health || mo->fuse || mo->threshold != KITEM_SPB)
			continue;

		P_KillMobj(mo, NULL, NULL, DMG_NORMAL);
	}
}

void Obj_SPBThrown(mobj_t *spb, fixed_t finalspeed)
{
	spb_speed(spb) = finalspeed;

	Obj_SPBEradicateCapsules();
}

// Used for seeking and when SPB is trailing its target from way too close!
static void SpawnSPBSpeedLines(mobj_t *spb)
{
	mobj_t *fast = P_SpawnMobjFromMobj(spb,
		P_RandomRange(PR_DECORATION, -24, 24) * FRACUNIT,
		P_RandomRange(PR_DECORATION, -24, 24) * FRACUNIT,
		(spb->info->height / 2) + (P_RandomRange(PR_DECORATION, -24, 24) * FRACUNIT),
		MT_FASTLINE
	);

	P_SetTarget(&fast->target, spb);
	fast->angle = K_MomentumAngle(spb);

	fast->color = SKINCOLOR_RED;
	fast->colorized = true;

	K_MatchGenericExtraFlags(fast, spb);
}

static void SPBTurn(
	fixed_t destSpeed, angle_t destAngle,
	fixed_t *editSpeed, angle_t *editAngle,
	fixed_t lerp, SINT8 *returnSliptide)
{
	INT32 delta = AngleDeltaSigned(destAngle, *editAngle);
	fixed_t dampen = FRACUNIT;

	// Slow down when turning; it looks better and makes U-turns not unfair
	dampen = FixedDiv((180 * FRACUNIT) - AngleFixed(abs(delta)), 180 * FRACUNIT);
	*editSpeed = FixedMul(destSpeed, dampen);

	delta = FixedMul(delta, lerp);

	// Calculate sliptide effect during seeking.
	if (returnSliptide != NULL)
	{
		const boolean isSliptiding =  (abs(delta) >= SPB_SLIPTIDEDELTA);
		SINT8 sliptide = 0;

		if (isSliptiding == true)
		{
			if (delta < 0)
			{
				sliptide = -1;
			}
			else
			{
				sliptide = 1;
			}
		}

		*returnSliptide = sliptide;
	}

	*editAngle += delta;
}

static void SetSPBSpeed(mobj_t *spb, fixed_t xySpeed, fixed_t zSpeed)
{
	spb->momx = FixedMul(FixedMul(
		xySpeed,
		FINECOSINE(spb->angle >> ANGLETOFINESHIFT)),
		FINECOSINE(spb_pitch(spb) >> ANGLETOFINESHIFT)
	);

	spb->momy = FixedMul(FixedMul(
		xySpeed,
		FINESINE(spb->angle >> ANGLETOFINESHIFT)),
		FINECOSINE(spb_pitch(spb) >> ANGLETOFINESHIFT)
	);

	spb->momz = FixedMul(
		zSpeed,
		FINESINE(spb_pitch(spb) >> ANGLETOFINESHIFT)
	);
}

static void SPBSeek(mobj_t *actor, mobj_t *bestMobj)
{
	const fixed_t desiredSpeed = SPB_DEFAULTSPEED;

	fixed_t dist = INT32_MAX;

	fixed_t xySpeed = desiredSpeed;
	fixed_t zSpeed = desiredSpeed;

	angle_t hang, vang;

	spb_lastplayer(actor) = -1; // Just make sure this is reset

		if (bestMobj == NULL
		|| P_MobjWasRemoved(bestMobj) == true
		|| bestMobj->health <= 0
		|| (bestMobj->player != NULL && bestMobj->player->respawn.state != RESPAWNST_NONE))
		{
			// No one there? Completely STOP.
			actor->momx = actor->momy = actor->momz = 0;
			if (bestMobj == NULL)
				spbplace = -1;
			return;
		}

		// Found someone, now get close enough to initiate the slaughter...

		// don't hurt players that have nothing to do with this:
		spb_intangible(actor) = 1;

		P_SetTarget(&actor->tracer, bestMobj);
		if (bestMobj->player != NULL)
			spbplace = bestMobj->player->position;
		else
			spbplace = 1;

		dist = P_AproxDistance(P_AproxDistance(actor->x-actor->tracer->x, actor->y-actor->tracer->y), actor->z-actor->tracer->z);

		hang = R_PointToAngle2(actor->x, actor->y, actor->tracer->x, actor->tracer->y);
		vang = R_PointToAngle2(0, actor->z, dist, actor->tracer->z);

		{
			// Smoothly rotate horz angle
			angle_t input = hang - actor->angle;
			boolean invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			// Slow down when turning; it looks better and makes U-turns not unfair
			xySpeed = FixedMul(spb_speed(actor), max(0, (((180<<FRACBITS) - AngleFixed(input)) / 90) - FRACUNIT));

			input = FixedAngle(AngleFixed(input)/4);
			if (invert)
				input = InvAngle(input);

			actor->angle += input;

			// Smoothly rotate vert angle
			input = vang - actor->movedir;
			invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			// Slow down when turning; might as well do it for momz, since we do it above too
			zSpeed = FixedMul(spb_speed(actor), max(0, (((180<<FRACBITS) - AngleFixed(input)) / 90) - FRACUNIT));

			input = FixedAngle(AngleFixed(input)/4);
			if (invert)
				input = InvAngle(input);

			actor->movedir += input;
		}

		actor->momx = FixedMul(FixedMul(xySpeed, FINECOSINE(actor->angle>>ANGLETOFINESHIFT)), FINECOSINE(actor->movedir>>ANGLETOFINESHIFT));
		actor->momy = FixedMul(FixedMul(xySpeed, FINESINE(actor->angle>>ANGLETOFINESHIFT)), FINECOSINE(actor->movedir>>ANGLETOFINESHIFT));
		actor->momz = FixedMul(zSpeed, FINESINE(actor->movedir>>ANGLETOFINESHIFT));

		if (dist <= (3072*actor->tracer->scale)) // Close enough to target?
		{
			S_StartSound(actor, actor->info->attacksound); // Siren sound; might not need this anymore, but I'm keeping it for now just for debugging.
			actor->flags &= ~MF_NOCLIPTHING;
			spb_mode(actor) = 1; // TARGET ACQUIRED
			spb_modetimer(actor) = 7*TICRATE;
			spb_intangible(actor) = 0;
		}
}

static void SPBChase(mobj_t *spb, mobj_t *bestMobj)
{
	fixed_t baseSpeed = 0;
	fixed_t maxSpeed = 0;
	fixed_t desiredSpeed = 0;

	fixed_t range = INT32_MAX;
	fixed_t cx = 0, cy = 0;

	fixed_t dist = INT32_MAX;
	angle_t destAngle = spb->angle;
	angle_t destPitch = 0U;
	fixed_t xySpeed = 0;
	fixed_t zSpeed = 0;

	mobj_t *chase = NULL;
	player_t *chasePlayer = NULL;

	chase = spb_chase(spb);

	if (chase == NULL || P_MobjWasRemoved(chase) == true || chase->health <= 0)
	{
		P_SetTarget(&spb_chase(spb), NULL);
		spb_mode(spb) = SPB_MODE_WAIT;
		spb_modetimer(spb) = 55; // Slightly over the respawn timer length
		return;
	}

	// Increment chase time
	spb_chasetime(spb)++;

	baseSpeed = SPB_DEFAULTSPEED;
	range = (160 * chase->scale);
	range = max(range, FixedMul(range, K_GetKartGameSpeedScalar(gamespeed)));

	// Play the intimidating gurgle
	if (S_SoundPlaying(spb, spb->info->activesound) == false)
	{
		S_StartSound(spb, spb->info->activesound);
	}

	dist = P_AproxDistance(P_AproxDistance(spb->x - chase->x, spb->y - chase->y), spb->z - chase->z);

	chasePlayer = chase->player;

	if (chasePlayer != NULL)
	{
		UINT8 fracmax = 32;
		UINT8 spark = ((10 - chasePlayer->kartspeed) + chasePlayer->kartweight) / 2;
		fixed_t easiness = ((chasePlayer->kartspeed + (10 - spark)) << FRACBITS) / 2;

		fixed_t scaleAdjust = FRACUNIT;
		if (chase->scale > mapobjectscale)
			scaleAdjust = GROW_PHYSICS_SCALE;
		if (chase->scale < mapobjectscale)
			scaleAdjust = SHRINK_PHYSICS_SCALE;

		spb_lastplayer(spb) = chasePlayer - players; // Save the player num for death scumming...
		spbplace = chasePlayer->position;

		chasePlayer->pflags |= PF_RINGLOCK; // set ring lock

		if (P_IsObjectOnGround(chase) == false)
		{
			// In the air you have no control; basically don't hit unless you make a near complete stop
			baseSpeed = (7 * chasePlayer->speed) / 8;
		}
		else
		{
			// 7/8ths max speed for Knuckles, 3/4ths max speed for min accel, exactly max speed for max accel
			baseSpeed = FixedMul(
				((fracmax+1) << FRACBITS) - easiness,
				FixedMul(K_GetKartSpeed(chasePlayer, false, false), scaleAdjust)
			) / fracmax;
		}

		if (chasePlayer->carry == CR_SLIDING)
		{
			baseSpeed = chasePlayer->speed/2;
		}

		// Be fairer on conveyors
		cx = chasePlayer->cmomx;
		cy = chasePlayer->cmomy;

		// Switch targets if you're no longer 1st for long enough
		if (bestMobj != NULL
			&& (bestMobj->player == NULL || chasePlayer->position <= bestMobj->player->position))
		{
			spb_modetimer(spb) = SPB_HOTPOTATO;
		}
		else
		{
			if (spb_modetimer(spb) > 0)
			{
				spb_modetimer(spb)--;
			}

			if (spb_modetimer(spb) <= 0)
			{
				spb_mode(spb) = SPB_MODE_SEEK; // back to SEEKING
				spb_intangible(spb) = SPB_FLASHING;
			}
		}

		chasePlayer->SPBdistance = dist;
	}
	else
	{
		spb_lastplayer(spb) = -1;
		spbplace = 1;
		spb_modetimer(spb) = SPB_HOTPOTATO;
	}

	desiredSpeed = FixedMul(baseSpeed, FRACUNIT + FixedDiv(dist - range, range));

	if (desiredSpeed < baseSpeed)
	{
		desiredSpeed = baseSpeed;
	}

	maxSpeed = (baseSpeed * 3) / 2;
	if (desiredSpeed > maxSpeed)
	{
		desiredSpeed = maxSpeed;
	}

	if (desiredSpeed < 20 * chase->scale)
	{
		desiredSpeed = 20 * chase->scale;
	}

	if (chasePlayer != NULL)
	{
		if (chasePlayer->carry == CR_SLIDING)
		{
			// Hack for current sections to make them fair.
			desiredSpeed = min(desiredSpeed, chasePlayer->speed / 2);
		}

		const mobj_t *waypoint = chasePlayer->currentwaypoint ? chasePlayer->currentwaypoint->mobj : NULL;
		// thing_args[3]: SPB speed (0-100)
		if (waypoint && waypoint->thing_args[3]) // 0 = default speed (unchanged)
		{
			desiredSpeed = desiredSpeed * waypoint->thing_args[3] / 100;
		}
	}

	destAngle = R_PointToAngle2(spb->x, spb->y, chase->x, chase->y);
	destPitch = R_PointToAngle2(0, spb->z, P_AproxDistance(spb->x - chase->x, spb->y - chase->y), chase->z);

	// Modify stored speed
	if (desiredSpeed > spb_speed(spb))
	{
		spb_speed(spb) += (desiredSpeed - spb_speed(spb)) / TICRATE;
	}
	else
	{
		spb_speed(spb) = desiredSpeed;
	}

	SPBTurn(spb_speed(spb), destAngle, &xySpeed, &spb->angle, SPB_CHASETURN, NULL);
	SPBTurn(spb_speed(spb), destPitch, &zSpeed, &spb_pitch(spb), SPB_CHASETURN, NULL);

	SetSPBSpeed(spb, xySpeed, zSpeed);
	spb->momx += cx;
	spb->momy += cy;

	// Red speed lines for when it's gaining on its target. A tell for when you're starting to lose too much speed!
	if (R_PointToDist2(0, 0, spb->momx, spb->momy) > (16 * R_PointToDist2(0, 0, chase->momx, chase->momy)) / 15 // Going faster than the target
		&& xySpeed > 20 * mapobjectscale) // Don't display speedup lines at pitifully low speeds
	{
		SpawnSPBSpeedLines(spb);
	}
}

static void SPBWait(mobj_t *spb)
{
	player_t *oldPlayer = NULL;

	spb->momx = spb->momy = spb->momz = 0; // Stoooop
	spb_curwaypoint(spb) = -1; // Reset waypoint

	if (spb_lastplayer(spb) != -1
		&& playeringame[spb_lastplayer(spb)] == true)
	{
		oldPlayer = &players[spb_lastplayer(spb)];
	}

	if (oldPlayer != NULL
		&& oldPlayer->spectator == false
		&& oldPlayer->exiting > 0)
	{
		spbplace = oldPlayer->position;
		oldPlayer->pflags |= PF_RINGLOCK;
	}

	if (spb_modetimer(spb) > 0)
	{
		spb_modetimer(spb)--;
	}

	if (spb_modetimer(spb) <= 0)
	{
		if (oldPlayer != NULL)
		{
			if (oldPlayer->mo != NULL && P_MobjWasRemoved(oldPlayer->mo) == false)
			{
				P_SetTarget(&spb_chase(spb), oldPlayer->mo);
				spb_mode(spb) = SPB_MODE_CHASE;
				spb_modetimer(spb) = SPB_HOTPOTATO;
				spb_intangible(spb) = SPB_FLASHING;
				spb_speed(spb) = SPB_DEFAULTSPEED;
			}
		}
		else
		{
			spb_mode(spb) = SPB_MODE_SEEK;
			spb_modetimer(spb) = 0;
			spb_intangible(spb) = SPB_FLASHING;
			spbplace = -1;
		}
	}
}

void Obj_SPBThink(mobj_t *spb)
{
	mobj_t *ghost = NULL;
	mobj_t *bestMobj = NULL;
	UINT8 bestRank = UINT8_MAX;
	size_t i;

	if (spb->health <= 0)
	{
		return;
	}

	K_SetItemCooldown(KITEM_SPB, 20*TICRATE);

	ghost = P_SpawnGhostMobj(spb);
	ghost->fuse = 3;

	if (spb_owner(spb) != NULL && P_MobjWasRemoved(spb_owner(spb)) == false && spb_owner(spb)->player != NULL)
	{
		ghost->color = spb_owner(spb)->player->skincolor;
		ghost->colorized = true;
	}

	if (spb_nothink(spb) <= 1)
	{
		if (specialstageinfo.valid == true)
		{
			bestRank = 0;

			if ((bestMobj = K_GetPossibleSpecialTarget()) == NULL)
			{
				// experimental - I think it's interesting IMO
				Obj_MantaRingCreate(
					spb,
					spb_owner(spb),
					NULL
				);

				spb->fuse = TICRATE/3;
				spb_nothink(spb) = spb->fuse + 2;
			}
		}
	}

	if (spb_nothink(spb) > 0)
	{
		// Init values, don't think yet.
		spb_lastplayer(spb) = -1;
		spb_curwaypoint(spb) = -1;
		spb_chasetime(spb) = 0;
		spbplace = -1;

		spb_manta_totaldist(spb) = 0; // 30000?
		spb_manta_vscale(spb) = SPB_MANTA_VSTART;

		P_InstaThrust(spb, spb->angle, SPB_DEFAULTSPEED);

		spb_nothink(spb)--;
	}
	else
	{
		// Find the player with the best rank
		for (i = 0; i < MAXPLAYERS; i++)
		{
			player_t *player = NULL;

			if (playeringame[i] == false)
			{
				// Not valid
				continue;
			}

			player = &players[i];

			if (player->spectator == true || player->exiting > 0)
			{
				// Not playing
				continue;
			}

			/*
			if (player->mo == NULL || P_MobjWasRemoved(player->mo) == true)
			{
				// No mobj
				continue;
			}

			if (player->mo <= 0)
			{
				// Dead
				continue;
			}

			if (player->respawn.state != RESPAWNST_NONE)
			{
				// Respawning
				continue;
			}
			*/

			if (player->position < bestRank)
			{
				bestRank = player->position;
				bestMobj = player->mo;
			}
		}

		switch (spb_mode(spb))
		{
			case SPB_MODE_SEEK:
			default:
				SPBSeek(spb, bestMobj);
				break;

			case SPB_MODE_CHASE:
				SPBChase(spb, bestMobj);
				break;

			case SPB_MODE_WAIT:
				SPBWait(spb);
				break;
		}
	}

	// Flash on/off when intangible.
	if (spb_intangible(spb) > 0)
	{
		spb_intangible(spb)--;

		if (spb_intangible(spb) & 1)
		{
			spb->renderflags |= RF_DONTDRAW;
		}
		else
		{
			spb->renderflags &= ~RF_DONTDRAW;
		}
	}

	// Flash white when about to explode!
	if (spb->fuse > 0)
	{
		if (spb->fuse & 1)
		{
			spb->color = SKINCOLOR_INVINCFLASH;
			spb->colorized = true;
		}
		else
		{
			spb->color = SKINCOLOR_NONE;
			spb->colorized = false;
		}
	}

	// Clamp within level boundaries.
	if (spb->z < spb->floorz)
	{
		spb->z = spb->floorz;
	}
	else if (spb->z > spb->ceilingz - spb->height)
	{
		spb->z = spb->ceilingz - spb->height;
	}
}

void Obj_SPBExplode(mobj_t *spb)
{
	mobj_t *spbExplode = NULL;

	// Don't continue playing the gurgle or the siren
	S_StopSound(spb);

	spbExplode = P_SpawnMobjFromMobj(spb, 0, 0, 0, MT_SPBEXPLOSION);

	if (spb_owner(spb) != NULL && P_MobjWasRemoved(spb_owner(spb)) == false)
	{
		P_SetTarget(&spbExplode->target, spb_owner(spb));
	}

	spbExplode->threshold = KITEM_SPB;

	// Tell the explosion to use alternate knockback.
	spbExplode->movefactor = ((SPB_CHASETIMESCALE - spb_chasetime(spb)) * SPB_CHASETIMEMUL) / SPB_CHASETIMESCALE;

	P_RemoveMobj(spb);
}

void Obj_SPBTouch(mobj_t *spb, mobj_t *toucher)
{
	player_t *const player = toucher->player;
	mobj_t *owner = NULL;
	mobj_t *chase = NULL;

	if (spb_intangible(spb) > 0)
	{
		return;
	}

	owner = spb_owner(spb);

	if ((owner == toucher || owner == toucher->target)
		&& (spb_nothink(spb) > 0))
	{
		return;
	}

	if (spb->health <= 0 || toucher->health <= 0)
	{
		return;
	}

	if (player != NULL)
	{
		if (player->spectator == true)
		{
			return;
		}

		if (player->bubbleblowup > 0)
		{
			// Stun the SPB, and remove the shield.
			K_PopPlayerShield(player);
			K_DropHnextList(player);
			spb_mode(spb) = SPB_MODE_WAIT;
			spb_modetimer(spb) = 55; // Slightly over the respawn timer length
			return;
		}
	}

	chase = spb_chase(spb);
	if (chase != NULL && P_MobjWasRemoved(chase) == false
		&& toucher == chase)
	{
		// Cause the explosion.
		Obj_SPBExplode(spb);
		return;
	}
	else if (toucher->flags & MF_SHOOTABLE)
	{
		// Regular spinout, please.
		P_DamageMobj(toucher, spb, owner, 1, DMG_NORMAL);
	}
}
