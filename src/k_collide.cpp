// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Kart Krew.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_collide.cpp
/// \brief SRB2Kart item collision hooks

#include <algorithm>

#include "k_collide.h"
#include "doomtype.h"
#include "p_mobj.h"
#include "k_kart.h"
#include "p_local.h"
#include "s_sound.h"
#include "r_main.h" // R_PointToAngle2, R_PointToDist2
#include "hu_stuff.h" // Sink snipe print
#include "doomdef.h" // Sink snipe print
#include "g_game.h" // Sink snipe print
#include "k_objects.h"
#include "k_roulette.h"
#include "k_podium.h"
#include "m_random.h"
#include "k_hud.h" // K_AddMessage
#include "m_easing.h"

// Noire
#include "noire/n_cvar.h"

angle_t K_GetCollideAngle(mobj_t *t1, mobj_t *t2)
{
	fixed_t momux, momuy;
	angle_t test;

	if (!(t1->flags & MF_PAPERCOLLISION))
	{
		return R_PointToAngle2(t1->x, t1->y, t2->x, t2->y)+ANGLE_90;
	}

	test = R_PointToAngle2(0, 0, t2->momx, t2->momy) + ANGLE_90 - t1->angle;
	if (test > ANGLE_180)
		test = t1->angle + ANGLE_180;
	else
		test = t1->angle;

	// intentional way around - sine...
	momuy = P_AproxDistance(t2->momx, t2->momy);
	momux = t2->momx - P_ReturnThrustY(t2, test, 2*momuy);
	momuy = t2->momy - P_ReturnThrustX(t2, test, 2*momuy);

	return R_PointToAngle2(0, 0, momux, momuy);
}

boolean K_BananaBallhogCollide(mobj_t *t1, mobj_t *t2)
{
	boolean damageitem = false;

	if (((t1->target == t2) || (!(t2->flags & (MF_ENEMY|MF_BOSS)) && (t1->target == t2->target))) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if (((t1->type == MT_BANANA_SHIELD) && (t2->type == MT_BANANA_SHIELD))
		&& (t1->target == t2->target)) // Don't hit each other if you have the same target
		return true;

	if (t1->type == MT_BALLHOG && t2->type == MT_BALLHOG)
		return true; // Ballhogs don't collide with eachother

	if (t2->player)
	{
		if (t2->player->flashing > 0)
			return true;

		// Banana snipe!
		if (t1->type == MT_BANANA && t1->health > 1)
			S_StartSound(t2, sfx_bsnipe);

		damageitem = true;

		if (t2->player->flamedash && t2->player->itemtype == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
		}
		else
		{
			P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		}
	}
	else if (t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_ORBINAUT || t2->type == MT_ORBINAUT_SHIELD
		|| t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD || t2->type == MT_JAWZ_SHIELD
		|| t2->type == MT_BALLHOG)
	{
		// Other Item Damage
		angle_t bounceangle = K_GetCollideAngle(t1, t2);

		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 24*FRACUNIT, false);
		P_InstaThrust(t2, bounceangle, 16*FRACUNIT);

		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);

		damageitem = true;
	}
	else if (t2->type == MT_SSMINE_SHIELD || t2->type == MT_SSMINE || t2->type == MT_LANDMINE)
	{
		damageitem = true;
		// Bomb death
		P_KillMobj(t2, t1, t1, DMG_NORMAL);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		damageitem = true;
	}

	if (damageitem && P_MobjWasRemoved(t1) == false)
	{
		angle_t bounceangle;

		if (P_MobjWasRemoved(t2) == false)
		{
			bounceangle = K_GetCollideAngle(t2, t1);
		}
		else
		{
			bounceangle = K_MomentumAngle(t1) + ANGLE_90;
			t2 = NULL; // handles the arguments to P_KillMobj
		}

		// This Item Damage
		S_StartSound(t1, t1->info->deathsound);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);

		P_SetObjectMomZ(t1, 24*FRACUNIT, false);

		P_InstaThrust(t1, bounceangle, 16*FRACUNIT);
	}

	return true;
}

boolean K_EggItemCollide(mobj_t *t1, mobj_t *t2)
{
	// Push fakes out of other item boxes
	if (t2->type == MT_RANDOMITEM || t2->type == MT_EGGMANITEM)
	{
		P_InstaThrust(t1, R_PointToAngle2(t2->x, t2->y, t1->x, t1->y), t2->radius/4);
		return true;
	}

	if (t2->player)
	{
		if ((t1->target == t2 || t1->target == t2->target) && (t1->threshold > 0))
			return true;

		if (t1->health <= 0 || t2->health <= 0)
			return true;

		if (!P_CanPickupItem(t2->player, 2))
			return true;

		K_DropItems(t2->player);
		K_StartEggmanRoulette(t2->player);

		if (t2->player->flamedash && t2->player->itemtype == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
			P_KillMobj(t1, t2, t2, DMG_NORMAL);
			return true;
		}
		else
		{
			//Popitem
			//Obj_SpawnItemDebrisEffects(t1, t2);
			if (t1->info->deathsound)
				S_StartSound(t2, sfx_kc2e);

#if 0
			// Eggbox snipe!
			if (t1->type == MT_EGGMANITEM && t1->health > 1)
				S_StartSound(t2, sfx_bsnipe);
#endif

			if (t1->target && t1->target->player)
			{
				t2->player->eggmanblame = t1->target->player - players;

				if (t1->target->hnext == t1)
				{
					P_SetTarget(&t1->target->hnext, NULL);
					t1->target->player->itemflags &= ~IF_EGGMANOUT;
				}
			}

			P_RemoveMobj(t1);
			return true;
		}
	}

	return true;
}

static mobj_t *grenade;
static fixed_t explodedist;
static boolean explodespin;

static inline boolean PIT_SSMineChecks(mobj_t *thing)
{
	if (thing == grenade) // Don't explode yourself! Endless loop!
		return true;

	if (thing->health <= 0)
		return true;

	if (!(thing->flags & MF_SHOOTABLE) || (thing->flags & MF_SCENERY))
		return true;

	if (thing->player && (thing->player->spectator || thing->player->hyudorotimer > 0))
		return true;

	if (P_AproxDistance(P_AproxDistance(thing->x - grenade->x, thing->y - grenade->y), thing->z - grenade->z) > explodedist)
		return true; // Too far away

	if (P_CheckSight(grenade, thing) == false)
		return true; // Not in sight

	return false;
}

static inline BlockItReturn_t PIT_SSMineSearch(mobj_t *thing)
{
	if (grenade == NULL || P_MobjWasRemoved(grenade))
		return BMIT_ABORT; // There's the possibility these can chain react onto themselves after they've already died if there are enough all in one spot

	if (grenade->flags2 & MF2_DEBRIS) // don't explode twice
		return BMIT_ABORT;

	switch (thing->type)
	{
	case MT_PLAYER: // Don't explode for anything but an actual player.
	case MT_SPECIAL_UFO: // Also UFO catcher
		break;

	default:
		return BMIT_CONTINUE;
	}

	if (thing == grenade->target && grenade->threshold != 0) // Don't blow up at your owner instantly.
		return BMIT_CONTINUE;

	if (PIT_SSMineChecks(thing) == true)
		return BMIT_CONTINUE;

	// Explode!
	P_SetMobjState(grenade, grenade->info->deathstate);
	return BMIT_ABORT;
}

void K_DoMineSearch(mobj_t *actor, fixed_t size)
{
	INT32 bx, by, xl, xh, yl, yh;

	explodedist = FixedMul(size, actor->scale);
	grenade = actor;

	yh = (unsigned)(actor->y + (explodedist + MAXRADIUS) - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (unsigned)(actor->y - (explodedist + MAXRADIUS) - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (unsigned)(actor->x + (explodedist + MAXRADIUS) - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (unsigned)(actor->x - (explodedist + MAXRADIUS) - bmaporgx)>>MAPBLOCKSHIFT;

	BMBOUNDFIX (xl, xh, yl, yh);

	for (by = yl; by <= yh; by++)
		for (bx = xl; bx <= xh; bx++)
			P_BlockThingsIterator(bx, by, PIT_SSMineSearch);
}

static inline BlockItReturn_t PIT_SSMineExplode(mobj_t *thing)
{
	if (grenade == NULL || P_MobjWasRemoved(grenade))
		return BMIT_ABORT; // There's the possibility these can chain react onto themselves after they've already died if there are enough all in one spot

#if 0
	if (grenade->flags2 & MF2_DEBRIS) // don't explode twice
		return BMIT_ABORT;
#endif

	if (PIT_SSMineChecks(thing) == true)
		return BMIT_CONTINUE;

	// Don't do Big Boy Damage to the UFO Catcher with
	// lingering spinout damage
	if (thing->type == MT_SPECIAL_UFO && explodespin)
	{
		return BMIT_CONTINUE;
	}

	P_DamageMobj(thing, grenade,grenade->target, 1, (explodespin ? DMG_NORMAL : DMG_EXPLODE));

	return BMIT_CONTINUE;
}

tic_t K_MineExplodeAttack(mobj_t *actor, fixed_t size, boolean spin)
{
	INT32 bx, by, xl, xh, yl, yh;

	explodespin = spin;
	explodedist = FixedMul(size, actor->scale);
	grenade = actor;

	// Use blockmap to check for nearby shootables
	yh = (unsigned)(actor->y + explodedist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (unsigned)(actor->y - explodedist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (unsigned)(actor->x + explodedist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (unsigned)(actor->x - explodedist - bmaporgx)>>MAPBLOCKSHIFT;

	BMBOUNDFIX (xl, xh, yl, yh);

	for (by = yl; by <= yh; by++)
		for (bx = xl; bx <= xh; bx++)
			P_BlockThingsIterator(bx, by, PIT_SSMineExplode);

	// Set this flag to ensure that the inital action won't be triggered twice.
	actor->flags2 |= MF2_DEBRIS;

	return 0;
}

boolean K_MineCollide(mobj_t *t1, mobj_t *t2)
{
	if (((t1->target == t2) || (!(t2->flags & (MF_ENEMY|MF_BOSS)) && (t1->target == t2->target))) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if (t2->player)
	{
		if (t2->player->flashing > 0)
			return true;

		// Bomb punting
		if ((t1->state >= &states[S_SSMINE1] && t1->state <= &states[S_SSMINE4])
			|| (t1->state >= &states[S_SSMINE_DEPLOY8] && t1->state <= &states[S_SSMINE_EXPLODE2]))
		{
			P_KillMobj(t1, t2, t2, DMG_NORMAL);
		}
		else
		{
			K_PuntMine(t1, t2);
		}
	}
	else if (t2->type == MT_ORBINAUT || t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD
		|| t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD)
	{
		// Bomb death
		angle_t bounceangle = K_GetCollideAngle(t1, t2);

		P_KillMobj(t1, t2, t2, DMG_NORMAL);

		// Other Item Damage
		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 24*FRACUNIT, false);
		P_InstaThrust(t2, bounceangle, 16*FRACUNIT);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Bomb death
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
	}

	return true;
}

boolean K_LandMineCollide(mobj_t *t1, mobj_t *t2)
{
	if (((t1->target == t2) || (!(t2->flags & (MF_ENEMY|MF_BOSS)) && (t1->target == t2->target))) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if (t2->player)
	{

		if (t2->player->flashing)
			return true;

		// Banana snipe!
		if (t1->health > 1)
		{
			if (t1->target
			&& t1->target->player
			&& t2->player != t1->target->player)
			{
				t1->target->player->roundconditions.landmine_dunk = true;
				t1->target->player->roundconditions.checkthisframe = true;
			}

			S_StartSound(t2, sfx_bsnipe);
		}

		if (t2->player->flamedash && t2->player->itemtype == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
		}
		else
		{
			// Player Damage
			P_DamageMobj(t2, t1, t1->target, 1, DMG_EXPLODE);
		}

		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}
	else if (t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_ORBINAUT || t2->type == MT_ORBINAUT_SHIELD
		|| t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD || t2->type == MT_JAWZ_SHIELD
		|| t2->type == MT_BALLHOG)
	{
		// Other Item Damage
		angle_t bounceangle = K_GetCollideAngle(t1, t2);

		if (t2->eflags & MFE_VERTICALFLIP)
			t2->z -= t2->height;
		else
			t2->z += t2->height;

		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);

		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		if (P_MobjWasRemoved(t2))
		{
			t2 = NULL; // handles the arguments to P_KillMobj
		}
		else
		{
			P_SetObjectMomZ(t2, 24*FRACUNIT, false);
			P_InstaThrust(t2, bounceangle, 16*FRACUNIT);

		}
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}
	else if (t2->type == MT_SSMINE_SHIELD || t2->type == MT_SSMINE || t2->type == MT_LANDMINE)
	{
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
		// Bomb death
		P_KillMobj(t2, t1, t1, DMG_NORMAL);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);

		if (P_MobjWasRemoved(t2))
		{
			t2 = NULL; // handles the arguments to P_KillMobj
		}

		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}

	return true;
}

static mobj_t *lightningSource;
static fixed_t lightningDist;

static inline BlockItReturn_t PIT_LightningShieldAttack(mobj_t *thing)
{
	if (lightningSource == NULL || P_MobjWasRemoved(lightningSource))
	{
		// Invalid?
		return BMIT_ABORT;
	}

	if (thing == NULL || P_MobjWasRemoved(thing))
	{
		// Invalid?
		return BMIT_ABORT;
	}

	if (thing == lightningSource)
	{
		// Don't explode yourself!!
		return BMIT_CONTINUE;
	}

	if (thing->health <= 0)
	{
		// Dead
		return BMIT_CONTINUE;
	}

	if (thing->type != MT_SPB)
	{
		if (!(thing->flags & MF_SHOOTABLE) || (thing->flags & MF_SCENERY))
		{
			// Not shootable
			return BMIT_CONTINUE;
		}
	}

	if (thing->player && thing->player->spectator)
	{
		// Spectator
		return BMIT_CONTINUE;
	}

	if (P_AproxDistance(thing->x - lightningSource->x, thing->y - lightningSource->y) > lightningDist + thing->radius)
	{
		// Too far away
		return BMIT_CONTINUE;
	}

#if 0
	if (P_CheckSight(lightningSource, thing) == false)
	{
		// Not in sight
		return BMIT_CONTINUE;
	}
#endif

	P_DamageMobj(thing, lightningSource, lightningSource, 1, DMG_VOLTAGE|DMG_CANTHURTSELF);
	return BMIT_CONTINUE;
}

void K_LightningShieldAttack(mobj_t *actor, fixed_t size)
{
	INT32 bx, by, xl, xh, yl, yh;

	lightningDist = FixedMul(size, actor->scale);
	lightningSource = actor;

	// Use blockmap to check for nearby shootables
	yh = (unsigned)(actor->y + lightningDist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (unsigned)(actor->y - lightningDist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (unsigned)(actor->x + lightningDist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (unsigned)(actor->x - lightningDist - bmaporgx)>>MAPBLOCKSHIFT;

	BMBOUNDFIX (xl, xh, yl, yh);

	for (by = yl; by <= yh; by++)
		for (bx = xl; bx <= xh; bx++)
			P_BlockThingsIterator(bx, by, PIT_LightningShieldAttack);
}

boolean K_BubbleShieldCanReflect(mobj_t *t1, mobj_t *t2)
{
	return (t2->type == MT_ORBINAUT || t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD
		|| t2->type == MT_BANANA || t2->type == MT_EGGMANITEM || t2->type == MT_BALLHOG
		|| t2->type == MT_SSMINE || t2->type == MT_LANDMINE || t2->type == MT_SINK
		|| (t2->type == MT_PLAYER && t1->target != t2));
}

boolean K_BubbleShieldReflect(mobj_t *t1, mobj_t *t2)
{
	mobj_t *owner = t1->player ? t1 : t1->target;

	if (t2->target != owner || !t2->threshold)
	{
		if (!t2->momx && !t2->momy)
		{
			t2->momz += (24*t2->scale) * P_MobjFlip(t2);
		}
		else
		{
			t2->momx = -6*t2->momx;
			t2->momy = -6*t2->momy;
			t2->momz = -6*t2->momz;
			t2->angle += ANGLE_180;
		}
		if (t2->type == MT_JAWZ)
			P_SetTarget(&t2->tracer, t2->target); // Back to the source!
		P_SetTarget(&t2->target, owner); // Let the source reflect it back again!
		t2->threshold = 10;
		S_StartSound(t1, sfx_s3k44);
	}

	return true;
}

boolean K_BubbleShieldCollide(mobj_t *t1, mobj_t *t2)
{
	if (t2->type == MT_PLAYER)
	{
		// Counter desyncs
		/*mobj_t *oldthing = thing;
		mobj_t *oldg_tm.thing = g_tm.thing;

		P_Thrust(g_tm.thing, R_PointToAngle2(thing->x, thing->y, g_tm.thing->x, g_tm.thing->y), 4*thing->scale);

		thing = oldthing;
		P_SetTarget(&g_tm.thing, oldg_tm.thing);*/

		if (K_KartBouncing(t2, t1->target) == true)
		{
			if (t2->player && t1->target && t1->target->player)
			{
				K_PvPTouchDamage(t2, t1->target);
			}

			// Don't play from t1 else it gets cut out... for some reason.
			S_StartSound(t2, sfx_s3k44);
		}

		return true;
	}

	if (K_BubbleShieldCanReflect(t1, t2))
	{
		return K_BubbleShieldReflect(t1, t2);
	}

	if (t2->flags & MF_SHOOTABLE)
	{
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
	}

	return true;
}

boolean K_KitchenSinkCollide(mobj_t *t1, mobj_t *t2)
{
	if (((t1->target == t2) || (!(t2->flags & (MF_ENEMY|MF_BOSS)) && (t1->target == t2->target))) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t2->player)
	{

		S_StartSound(NULL, sfx_bsnipe); // let all players hear it.

		HU_SetCEchoFlags(0);
		HU_SetCEchoDuration(5);
		HU_DoCEcho(va("%s\\was hit by a kitchen sink.\\\\\\\\", player_names[t2->player-players]));
		I_OutputMsg("%s was hit by a kitchen sink.\n", player_names[t2->player-players]);

		P_DamageMobj(t2, t1, t1->target, 1, DMG_INSTAKILL);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_KillMobj(t2, t2, t1->target, DMG_NORMAL);
		if (P_MobjWasRemoved(t2))
		{
			t2 = NULL; // handles the arguments to P_KillMobj
		}
		// This item damage
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}

	return true;
}

boolean K_FallingRockCollide(mobj_t *t1, mobj_t *t2)
{
	if (t2->player || t2->type == MT_FALLINGROCK)
		K_KartBouncing(t2, t1);
	return true;
}

boolean K_PvPTouchDamage(mobj_t *t1, mobj_t *t2)
{
	if (K_PodiumSequence() == true)
	{
		// Always regular bumps, no ring toss.
		return false;
	}

	// What the fuck is calling this with stale refs? Whatever, validation's cheap.
	if (P_MobjWasRemoved(t1) || P_MobjWasRemoved(t2) || !t1->player || !t2->player)
		return false;

	// Clash instead of damage if both parties have any of these conditions
	auto canClash = [](mobj_t *t1, mobj_t *t2)
	{
		return (K_IsBigger(t1, t2) == true)
			|| (t1->player->invincibilitytimer > 0)
			|| (t1->player->flamedash > 0 && t1->player->itemtype == KITEM_FLAMESHIELD)
			|| (t1->player->bubbleblowup > 0);
	};

	if (canClash(t1, t2) && canClash(t2, t1))
	{
		K_DoPowerClash(t1, t2);
		return false;
	}

	auto forEither = [t1, t2](auto conditionCallable, auto damageCallable)
	{
		const bool t1Condition = conditionCallable(t1, t2);
		const bool t2Condition = conditionCallable(t2, t1);

		if (t1Condition == true && t2Condition == false)
		{
			damageCallable(t1, t2);
			return true;
		}
		else if (t1Condition == false && t2Condition == true)
		{
			damageCallable(t2, t1);
			return true;
		}

		return false;
	};

	auto doDamage = [](UINT8 damageType)
	{
		return [damageType](mobj_t *t1, mobj_t *t2)
		{
			P_DamageMobj(t2, t1, t1, 1, damageType);
		};
	};

	// Cause Spinout on invincibility
	auto shouldSpinout = [](mobj_t *t1, mobj_t *t2)
	{
		return (t1->player->invincibilitytimer > 0);
	};

	if (forEither(shouldSpinout, doDamage(DMG_NORMAL)))
	{
		return true;
	}

	// Flame Shield dash damage
	// Bubble Shield blowup damage
	auto shouldWipeout = [](mobj_t *t1, mobj_t *t2)
	{
		return (t1->player->flamedash > 0 && t1->player->itemtype == KITEM_FLAMESHIELD)
			|| (t1->player->bubbleblowup > 0);
	};

	if (forEither(shouldWipeout, doDamage(DMG_WIPEOUT)))
	{
		return true;
	}

	// Battle Mode Sneaker damage
	// (Pogo Spring damage is handled in head-stomping code)
	if (gametyperules & GTR_BUMPERS)
	{
		auto shouldSteal = [](mobj_t *t1, mobj_t *t2)
		{
			return ((t1->player->sneakertimer > 0)
				&& !P_PlayerInPain(t1->player)
				&& (t1->player->flashing == 0));
		};

		if (forEither(shouldSteal, doDamage(DMG_NORMAL | DMG_STEAL)))
		{
			return true;
		}
	}

	// Cause sqiush on scale difference
	auto shouldSquish= [](mobj_t *t1, mobj_t *t2)
	{
		return K_IsBigger(t1, t2);
	};

	if (forEither(shouldSquish, doDamage(DMG_SQUISH)))
	{
		return true;
	}

	if (cv_ng_ringsting.value)
	{
		// Ring sting, this is a bit more unique
		auto doSting = [](mobj_t *t1, mobj_t *t2)
		{
			if (K_GetShieldFromItem(t2->player->itemtype) != KSHIELD_NONE)
			{
				return false;
			}

			bool stung = false;

			if (t2->player->rings <= 0 && t2->health == 1) // no bumpers
			{
				P_DamageMobj(t2, t1, t1, 1, DMG_STING);
				stung = true;
			}

			P_PlayerRingBurst(t2->player, 1);

			return stung;



		};

		if (forEither(doSting, doSting))
		{
			return true;
		}
	}

	return false;
}

void K_PuntHazard(mobj_t *t1, mobj_t *t2)
{
	// TODO: spawn a unique mobjtype other than MT_GHOST
	mobj_t *img = P_SpawnGhostMobj(t1);

	K_MakeObjectReappear(t1);

	img->flags &= ~MF_NOGRAVITY;
	img->renderflags = t1->renderflags & ~RF_DONTDRAW;
	img->extravalue1 = 1;
	img->extravalue2 = 2;
	img->fuse = 2*TICRATE;

	struct Vector
	{
		fixed_t x_, y_, z_;
		fixed_t h_ = FixedHypot(x_, y_);
		fixed_t speed_ = std::max(60 * mapobjectscale, FixedHypot(h_, z_) * 2);

		explicit Vector(fixed_t x, fixed_t y, fixed_t z) : x_(x), y_(y), z_(z) {}
		explicit Vector(const mobj_t* mo) :
			Vector(std::max(
				Vector(mo->x - mo->old_x, mo->y - mo->old_y, mo->z - mo->old_z),
				Vector(mo->momx, mo->momy, mo->momz)
			))
		{
		}
		explicit Vector(const Vector&) = default;

		bool operator<(const Vector& b) const { return speed_ < b.speed_; }

		void invert()
		{
			x_ = -x_;
			y_ = -y_;
			z_ = -z_;
		}

		void thrust(mobj_t* mo) const
		{
			angle_t yaw = R_PointToAngle2(0, 0, h_, z_);
			yaw = std::max(AbsAngle(yaw), static_cast<angle_t>(ANGLE_11hh)) + (yaw & ANGLE_180);

			P_InstaThrust(mo, R_PointToAngle2(0, 0, x_, y_), FixedMul(speed_, FCOS(yaw)));
			mo->momz = FixedMul(speed_, FSIN(yaw));
		}
	};

	Vector h_vector(t1);
	Vector p_vector(t2);

	h_vector.invert();

	std::max(h_vector, p_vector).thrust(img);

	K_DoPowerClash(img, t2); // applies hitlag
	P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);
}

boolean K_PuntCollide(mobj_t *t1, mobj_t *t2)
{
	// MF_SHOOTABLE will get damaged directly, instead
	if (t1->flags & (MF_DONTPUNT | MF_SHOOTABLE))
	{
		return false;
	}

	if (!t2->player)
	{
		return false;
	}

	if (!K_PlayerCanPunt(t2->player))
	{
		return false;
	}

	if (t1->flags & MF_ELEMENTAL)
	{
		K_MakeObjectReappear(t1);

		// copied from MT_ITEMCAPSULE
		UINT8 i;
		INT16 spacing = (t1->radius >> 1) / t1->scale;
		// dust effects
		for (i = 0; i < 10; i++)
		{
			mobj_t *puff = P_SpawnMobjFromMobj(
				t1,
				P_RandomRange(PR_ITEM_DEBRIS, -spacing, spacing) * FRACUNIT,
				P_RandomRange(PR_ITEM_DEBRIS, -spacing, spacing) * FRACUNIT,
				P_RandomRange(PR_ITEM_DEBRIS, 0, 4*spacing) * FRACUNIT,
				MT_DUST
			);

			puff->momz = puff->scale * P_MobjFlip(puff);

			P_Thrust(puff, R_PointToAngle2(t2->x, t2->y, puff->x, puff->y), 3*puff->scale);

			puff->momx += t2->momx / 2;
			puff->momy += t2->momy / 2;
			puff->momz += t2->momz / 2;
		}
	}
	else
	{
		K_PuntHazard(t1, t2);
	}

	return true;
}
