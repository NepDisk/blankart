// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Kart Krew.
// Copyright (C) 2018 by ZarroTsu.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_kart.c
/// \brief SRB2kart general.
///        All of the SRB2kart-unique stuff.

// TODO: Break this up into more files.
// Files dedicated only for "general miscellanea"
// are straight-up bad coding practice.
// It's better to have niche files that are
// too short than one file that's too massive.

#include "k_kart.h"
#include "d_player.h"
#include "doomstat.h"
#include "info.h"
#include "k_battle.h"
#include "k_pwrlv.h"
#include "k_color.h"
#include "k_respawn.h"
#include "doomdef.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "g_input.h"    // for device rumble
#include "m_fixed.h"
#include "m_random.h"
#include "p_local.h"
#include "p_mobj.h"
#include "p_slopes.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_local.h"
#include "r_things.h"
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
#include "k_terrain.h"
#include "k_director.h"
#include "k_collide.h"
#include "k_follower.h"
#include "k_objects.h"
#include "k_grandprix.h"
#include "k_boss.h"
#include "k_specialstage.h"
#include "k_roulette.h"
#include "k_podium.h"
#include "k_tally.h"
#include "music.h"
#include "m_easing.h"
#include "k_endcam.h"

// Noire
#include "noire/n_control.h"
#include "noire/n_legacycheckpoint.h"
#include "noire/n_cvar.h"

// SOME IMPORTANT VARIABLES DEFINED IN DOOMDEF.H:
// gamespeed is cc (0 for easy, 1 for normal, 2 for hard)
// franticitems is Frantic Mode items, bool
// encoremode is Encore Mode (duh), bool
// comeback is Battle Mode's karma comeback, also bool
// mapreset is set when enough players fill an empty server

boolean K_ThunderDome(void)
{
	if (K_CanChangeRules(true))
	{
		return (boolean)cv_thunderdome.value;
	}

	return false;
}

// lat: used for when the player is in some weird state where it wouldn't be wise for it to be overwritten by another object that does similarly wacky shit.
boolean K_isPlayerInSpecialState(player_t *p)
{
	return (
		p->rideroid
		|| p->rdnodepull
		|| p->bungee
		|| p->dlzrocket
		|| p->seasaw
		|| p->turbine
	);
}

boolean K_IsDuelItem(mobjtype_t type)
{
	switch (type)
	{
		case MT_DUELBOMB:
		case MT_BANANA:
		case MT_EGGMANITEM:
		case MT_SSMINE:
		case MT_LANDMINE:
		case MT_POGOSPRING:
			return true;

		default:
			return false;
	}
}

boolean K_DuelItemAlwaysSpawns(mapthing_t *mt)
{
	return !!(mt->thing_args[0]);
}

static void K_SpawnDuelOnlyItems(void)
{
	mapthing_t *mt = NULL;
	size_t i;

	mt = mapthings;
	for (i = 0; i < nummapthings; i++, mt++)
	{
		mobjtype_t type = P_GetMobjtype(mt->type);

		if (K_IsDuelItem(type) == true
			&& K_DuelItemAlwaysSpawns(mt) == false)
		{
			P_SpawnMapThing(mt);
		}
	}
}

void K_TimerReset(void)
{
	starttime = introtime = 0;
	memset(&g_darkness, 0, sizeof g_darkness);
	memset(&g_musicfade, 0, sizeof g_musicfade);
	inDuel = false;
	linecrossed = 0;
	timelimitintics = extratimeintics = secretextratime = 0;
	g_pointlimit = 0;
}

static void K_SpawnItemCapsules(void)
{
	mapthing_t *mt = mapthings;
	size_t i = SIZE_MAX;

	for (i = 0; i < nummapthings; i++, mt++)
	{
		boolean isRingCapsule = false;
		INT32 modeFlags = 0;

		if (mt->type != mobjinfo[MT_ITEMCAPSULE].doomednum)
		{
			continue;
		}

		isRingCapsule = (mt->thing_args[0] < 1 || mt->thing_args[0] == KITEM_SUPERRING || mt->thing_args[0] >= NUMKARTITEMS);
		if (isRingCapsule == true && ((gametyperules & GTR_SPHERES) || (modeattacking & ATTACKING_SPB)))
		{
			// don't spawn ring capsules in ringless gametypes
			continue;
		}

		if (gametype != GT_TUTORIAL) // Don't prevent capsule spawn via modeflags in Tutorial
		{
			modeFlags = mt->thing_args[3];
			if (modeFlags == TMICM_DEFAULT)
			{
				if (isRingCapsule == true)
				{
					modeFlags = TMICM_MULTIPLAYER|TMICM_TIMEATTACK;
				}
				else
				{
					modeFlags = TMICM_MULTIPLAYER;
				}
			}

			if (K_CapsuleTimeAttackRules() == true)
			{
				if ((modeFlags & TMICM_TIMEATTACK) == 0)
				{
					continue;
				}
			}
			else
			{
				if ((modeFlags & TMICM_MULTIPLAYER) == 0)
				{
					continue;
				}
			}
		}


		P_SpawnMapThing(mt);
	}
}

void K_TimerInit(void)
{
	UINT8 i;
	UINT8 numPlayers = 0;
	boolean domodeattack = ((modeattacking != ATTACKING_NONE)
		|| (grandprixinfo.gp == true && grandprixinfo.eventmode != GPEVENT_NONE));

	if (K_PodiumSequence() == true)
	{
		// Leave it alone for podium
		return;
	}

	const boolean bossintro = K_CheckBossIntro();

	// Rooooooolllling staaaaaaart
	if ((gametyperules & (GTR_ROLLINGSTART|GTR_CIRCUIT)) == (GTR_ROLLINGSTART|GTR_CIRCUIT))
	{
		S_StartSound(NULL, sfx_s25f);
		// The actual push occours in P_InitPlayers
	}
	else if (skipstats != 0 && bossintro == false)
	{
		S_StartSound(NULL, sfx_s26c);
	}

	if ((gametyperules & (GTR_CATCHER|GTR_CIRCUIT)) == (GTR_CATCHER|GTR_CIRCUIT))
	{
		K_InitSpecialStage();
	}
	else if (bossintro == true)
		;
	else
	{
		if (!domodeattack)
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i] || players[i].spectator)
				{
					continue;
				}

				numPlayers++;
			}

			if (numPlayers < 2)
			{
				domodeattack = true;
			}

			// 1v1 activates DUEL rules!
			inDuel = (numPlayers == 2);

			introtime = (108) + 5; // 108 for rotation, + 5 for white fade
		}
		starttime = 6*TICRATE + (3*TICRATE/4);
	}

	K_SpawnItemCapsules();
	K_BattleInit(domodeattack);

	timelimitintics = K_TimeLimitForGametype();
	g_pointlimit = K_PointLimitForGametype();

	// K_TimerInit is called after all mapthings are spawned,
	// so they didn't know if it's supposed to be a duel
	// (inDuel is always false before K_TimerInit is called).
	if (inDuel)
	{
		K_SpawnDuelOnlyItems();
	}

	if (
		battleprisons == true
		&& grandprixinfo.gp == true
		&& netgame == false
		&& gamedata->thisprisoneggpickup_cached != NULL
		&& gamedata->prisoneggstothispickup == 0
		&& maptargets > 1
	)
	{
		// This calculation is like this so...
		// - You can't get a Prison Egg Drop on the last broken target
		// - If it were 0 at minimum there'd be a slight bias towards the start of the round
		//    - This is bad because it benefits CD farming like in Brawl :D
		gamedata->prisoneggstothispickup = 1 + M_RandomKey(maptargets - 1);
	}
}

UINT32 K_GetPlayerDontDrawFlag(player_t *player)
{
	UINT32 flag = 0;

	if (player == NULL)
		return flag;

	if (player == &players[displayplayers[0]])
		flag |= RF_DONTDRAWP1;
	if (r_splitscreen >= 1 && player == &players[displayplayers[1]])
		flag |= RF_DONTDRAWP2;
	if (r_splitscreen >= 2 && player == &players[displayplayers[2]])
		flag |= RF_DONTDRAWP3;
	if (r_splitscreen >= 3 && player == &players[displayplayers[3]])
		flag |= RF_DONTDRAWP4;

	return flag;
}

void K_ReduceVFXForEveryone(mobj_t *mo)
{
	if (cv_reducevfx.value == 0)
	{
		// Leave the visuals alone.
		return;
	}

	mo->renderflags |= RF_DONTDRAW;
}

// Angle reflection used by springs & speed pads
angle_t K_ReflectAngle(angle_t yourangle, angle_t theirangle, fixed_t yourspeed, fixed_t theirspeed)
{
	INT32 angoffset;
	boolean subtract = false;

	angoffset = yourangle - theirangle;

	if ((angle_t)angoffset > ANGLE_180)
	{
		// Flip on wrong side
		angoffset = InvAngle((angle_t)angoffset);
		subtract = !subtract;
	}

	// Fix going directly against the spring's angle sending you the wrong way
	if ((angle_t)angoffset > ANGLE_90)
	{
		angoffset = ANGLE_180 - angoffset;
	}

	// Offset is reduced to cap it (90 / 2 = max of 45 degrees)
	angoffset /= 2;

	// Reduce further based on how slow your speed is compared to the spring's speed
	// (set both to 0 to ignore this)
	if (theirspeed != 0 && yourspeed != 0)
	{
		if (theirspeed > yourspeed)
		{
			angoffset = FixedDiv(angoffset, FixedDiv(theirspeed, yourspeed));
		}
	}

	if (subtract)
		angoffset = (signed)(theirangle) - angoffset;
	else
		angoffset = (signed)(theirangle) + angoffset;

	return (angle_t)angoffset;
}

boolean K_IsPlayerLosing(player_t *player)
{
	INT32 winningpos = 1;
	UINT8 i, pcount = 0;

	if (K_PodiumSequence() == true)
	{
		// Need to be in top 3 to win.
		return (player->position > 3);
	}

	if (player->pflags & PF_NOCONTEST)
		return true;

	if (battleprisons && numtargets == 0)
		return true; // Didn't even TRY?

	if (player->position == 1)
		return false;

	if (specialstageinfo.valid == true)
		return false; // anything short of DNF is COOL

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		if (players[i].position > pcount)
			pcount = players[i].position;
	}

	if (pcount <= 1)
		return false;

	winningpos = pcount/2;
	if (pcount % 2) // any remainder?
		winningpos++;

	return (player->position > winningpos);
}

fixed_t K_GetKartGameSpeedScalar(SINT8 value)
{
	// Easy = 81.25%
	// Normal = 100%
	// Hard = 118.75%
	// Nightmare = 137.5% ?!?!

	// WARNING: This value is used instead of directly checking game speed in some
	// cases, where hard difficulty breakpoints are needed, but compatibility with
	// the "4th Gear" cheat seemed relevant. Sorry about the weird indirection!
	// At the time of writing:
	// K_UpdateOffroad (G3+ double offroad penalty speed)
	// P_ButteredSlope (G1- Slope Assist)

	if (cv_4thgear.value && !netgame && (!demo.playback || !demo.netgame) && !modeattacking)
		value = 3;

	return ((13 + (3*value)) << FRACBITS) / 16;
}

// Array of states to pick the starting point of the animation, based on the actual time left for invincibility.
static INT32 K_SparkleTrailStartStates[KART_NUMINVSPARKLESANIM][2] = {
	{S_KARTINVULN12, S_KARTINVULNB12},
	{S_KARTINVULN11, S_KARTINVULNB11},
	{S_KARTINVULN10, S_KARTINVULNB10},
	{S_KARTINVULN9, S_KARTINVULNB9},
	{S_KARTINVULN8, S_KARTINVULNB8},
	{S_KARTINVULN7, S_KARTINVULNB7},
	{S_KARTINVULN6, S_KARTINVULNB6},
	{S_KARTINVULN5, S_KARTINVULNB5},
	{S_KARTINVULN4, S_KARTINVULNB4},
	{S_KARTINVULN3, S_KARTINVULNB3},
	{S_KARTINVULN2, S_KARTINVULNB2},
	{S_KARTINVULN1, S_KARTINVULNB1}
};

INT32 K_GetShieldFromItem(INT32 item)
{
	switch (item)
	{
		case KITEM_LIGHTNINGSHIELD: return KSHIELD_LIGHTNING;
		case KITEM_BUBBLESHIELD: return KSHIELD_BUBBLE;
		case KITEM_FLAMESHIELD: return KSHIELD_FLAME;
		default: return KSHIELD_NONE;
	}
}

SINT8 K_ItemResultToType(SINT8 getitem)
{
	if (getitem <= 0 || getitem >= NUMKARTRESULTS) // Sad (Fallback)
	{
		if (getitem != 0)
		{
			CONS_Printf("ERROR: K_GetItemResultToItemType - Item roulette gave bad item (%d) :(\n", getitem);
		}

		return KITEM_SAD;
	}

	if (getitem >= NUMKARTITEMS)
	{
		switch (getitem)
		{
			case KRITEM_DUALSNEAKER:
			case KRITEM_TRIPLESNEAKER:
				return KITEM_SNEAKER;

			case KRITEM_TRIPLEBANANA:
			case KRITEM_TENFOLDBANANA:
				return KITEM_BANANA;

			case KRITEM_TRIPLEORBINAUT:
			case KRITEM_QUADORBINAUT:
				return KITEM_ORBINAUT;

			case KRITEM_DUALJAWZ:
				return KITEM_JAWZ;

			default:
				I_Error("Bad item cooldown redirect for result %d\n", getitem);
				break;
		}
	}

	return getitem;
}

UINT8 K_ItemResultToAmount(SINT8 getitem)
{
	switch (getitem)
	{
		case KRITEM_DUALSNEAKER:
		case KRITEM_DUALJAWZ:
			return 2;

		case KRITEM_TRIPLESNEAKER:
		case KRITEM_TRIPLEBANANA:
		case KRITEM_TRIPLEORBINAUT:
			return 3;

		case KRITEM_QUADORBINAUT:
			return 4;

		case KRITEM_TENFOLDBANANA:
			return 10;

		default:
			return 1;
	}
}

tic_t K_GetItemCooldown(SINT8 itemResult)
{
	SINT8 itemType = K_ItemResultToType(itemResult);

	if (itemType < 1 || itemType >= NUMKARTITEMS)
	{
		return 0;
	}

	return itemCooldowns[itemType - 1];
}

void K_SetItemCooldown(SINT8 itemResult, tic_t time)
{
	SINT8 itemType = K_ItemResultToType(itemResult);

	if (itemType < 1 || itemType >= NUMKARTITEMS)
	{
		return;
	}

	itemCooldowns[itemType - 1] = max(itemCooldowns[itemType - 1], time);
}

void K_RunItemCooldowns(void)
{
	size_t i;

	for (i = 0; i < NUMKARTITEMS-1; i++)
	{
		if (itemCooldowns[i] > 0)
		{
			itemCooldowns[i]--;
		}
	}
}

boolean K_TimeAttackRules(void)
{
	UINT8 playing = 0;
	UINT8 i;

	if ((gametyperules & (GTR_CATCHER|GTR_CIRCUIT)) == (GTR_CATCHER|GTR_CIRCUIT))
	{
		// Kind of a hack -- Special Stages
		// are expected to be 1-player, so
		// we won't use the Time Attack changes
		return false;
	}

	if (modeattacking != ATTACKING_NONE)
	{
		// Time Attack obviously uses Time Attack rules :p
		return true;
	}

	if (battleprisons == true)
	{
		// Break the Capsules always uses Time Attack
		// rules, since you can bring 2-4 players in
		// via Grand Prix.
		return true;
	}

	if (gametype == GT_TUTORIAL)
	{
		// Tutorials are special. By default only one
		// player will be playing... but sometimes bots
		// can be spawned! So we still guarantee the
		// changed behaviour for consistency.
		return true;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] == false || players[i].spectator == true)
		{
			continue;
		}

		playing++;
		if (playing > 1)
		{
			break;
		}
	}

	// Use Time Attack gameplay rules with only 1P.
	return (playing <= 1);
}

boolean K_CapsuleTimeAttackRules(void)
{
	switch (cv_capsuletest.value)
	{
		case CV_CAPSULETEST_MULTIPLAYER:
			return false;

		case CV_CAPSULETEST_TIMEATTACK:
			return true;

		default:
			return K_TimeAttackRules();
	}
}

//}

//{ SRB2kart p_user.c Stuff

static fixed_t K_PlayerWeight(mobj_t *mobj, mobj_t *against)
{
	fixed_t weight = 5*FRACUNIT;

	if (!mobj->player)
		return weight;

	if (against && !P_MobjWasRemoved(against) && against->player
		&& ((!P_PlayerInPain(against->player) && P_PlayerInPain(mobj->player)) // You're hurt
		|| (against->player->itemtype == KITEM_BUBBLESHIELD && mobj->player->itemtype != KITEM_BUBBLESHIELD))) // They have a Bubble Shield
	{
		weight = 0; // This player does not cause any bump action
	}
	else if (against && against->type == MT_SPECIAL_UFO)
	{
		weight = 0;
	}
	else
	{
		// Applies rubberbanding, to prevent rubberbanding bots
		// from causing super crazy bumps.
		fixed_t spd = K_GetKartSpeed(mobj->player, false, true);

		fixed_t speedfactor = 8 * mapobjectscale;

		weight = (mobj->player->kartweight) * FRACUNIT;

		if (against && against->type == MT_MONITOR)
		{
			speedfactor /= 5; // speed matters more
		}
		else
		{
			if (mobj->player->itemtype == KITEM_BUBBLESHIELD)
				weight += 9*FRACUNIT;
		}

		if (mobj->player->speed > spd)
			weight += FixedDiv((mobj->player->speed - spd), speedfactor);
	}

	return weight;
}

fixed_t K_GetMobjWeight(mobj_t *mobj, mobj_t *against)
{
	fixed_t weight = 5*FRACUNIT;

	switch (mobj->type)
	{
		case MT_PLAYER:
			if (!mobj->player)
				break;
			weight = K_PlayerWeight(mobj, against);
			break;
		case MT_BUBBLESHIELD:
			weight = K_PlayerWeight(mobj->target, against);
			break;
		case MT_FALLINGROCK:
			if (against->player)
			{
				if (against->player->invincibilitytimer || K_IsBigger(against, mobj) == true)
					weight = 0;
				else
					weight = K_PlayerWeight(against, NULL);
			}
			break;
		case MT_ORBINAUT:
		case MT_ORBINAUT_SHIELD:
		case MT_DUELBOMB:
			if (against->player)
				weight = K_PlayerWeight(against, NULL);
			break;
		case MT_JAWZ:
		case MT_JAWZ_DUD:
		case MT_JAWZ_SHIELD:
			if (against->player)
				weight = K_PlayerWeight(against, NULL) + (3*FRACUNIT);
			else
				weight += 3*FRACUNIT;
			break;
		default:
			break;
	}

	return FixedMul(weight, mobj->scale);
}

static void K_SpawnBumpForObjs(mobj_t *mobj1, mobj_t *mobj2)
{

	mobj_t *fx = P_SpawnMobj(
		mobj1->x/2 + mobj2->x/2,
		mobj1->y/2 + mobj2->y/2,
		mobj1->z/2 + mobj2->z/2,
		MT_BUMP
	);
	fixed_t avgScale = (mobj1->scale + mobj2->scale) / 2;

	if (mobj1->eflags & MFE_VERTICALFLIP)
	{
		fx->eflags |= MFE_VERTICALFLIP;
	}
	else
	{
		fx->eflags &= ~MFE_VERTICALFLIP;
	}

	P_SetScale(fx, (fx->destscale = avgScale));

	if ((mobj1->player && mobj1->player->itemtype == KITEM_BUBBLESHIELD)
	|| (mobj2->player && mobj2->player->itemtype == KITEM_BUBBLESHIELD))
	{
		S_StartSound(mobj1, sfx_s3k44);
	}
	else
	{
		S_StartSound(mobj1, sfx_s3k49);
	}
}

static void K_PlayerJustBumped(player_t *player)
{
	mobj_t *playerMobj = NULL;

	if (player == NULL)
	{
		return;
	}

	playerMobj = player->mo;

	if (playerMobj == NULL || P_MobjWasRemoved(playerMobj))
	{
		return;
	}

	if (abs(player->rmomx) < playerMobj->scale && abs(player->rmomy) < playerMobj->scale)
	{
		// Because this is done during collision now, rmomx and rmomy need to be recalculated
		// so that friction doesn't immediately decide to stop the player if they're at a standstill
		player->rmomx = playerMobj->momx - player->cmomx;
		player->rmomy = playerMobj->momy - player->cmomy;
	}

	player->justbumped = bumptime;

	// If spinouttimer is not set yet but could be set later,
	// this lets the bump still trigger wipeout friction. If
	// spinouttimer never gets set, then this has no effect on
	// friction and gets unset anyway.
	player->wipeoutslow = wipeoutslowtime+1;

	if (player->spinouttimer)
	{
		player->spinouttimer = max(wipeoutslowtime+1, player->spinouttimer);
		//player->spinouttype = KSPIN_WIPEOUT; // Enforce type
	}
}

static boolean K_JustBumpedException(mobj_t *mobj)
{
	switch (mobj->type)
	{
		case MT_SA2_CRATE:
			return Obj_SA2CrateIsMetal(mobj);
		case MT_WALLSPIKE:
			return true;
		case MT_BATTLECAPSULE:
		{
			if (gametype == GT_TUTORIAL // Remove gametype check whenever it's safe to break compatibility with ghosts in a post-release patch
			&& mobj->momx == 0
			&& mobj->momy == 0
			&& mobj->momz == 0)
			{
				return true;
			}
			break;
		}
		default:
			break;
	}

	if (mobj->flags & MF_PUSHABLE)
		return true;

	return false;
}

static fixed_t K_GetBounceForce(mobj_t *mobj1, mobj_t *mobj2, fixed_t distx, fixed_t disty)
{
	const fixed_t forceMul = (4 * FRACUNIT) / 10; // Multiply by this value to make it feel like old bumps.

	fixed_t momdifx, momdify;
	fixed_t dot;
	fixed_t force = 0;

	momdifx = mobj1->momx - mobj2->momx;
	momdify = mobj1->momy - mobj2->momy;

	if (distx == 0 && disty == 0)
	{
		// if there's no distance between the 2, they're directly on top of each other, don't run this
		return 0;
	}

	{ // Normalize distance to the sum of the two objects' radii, since in a perfect world that would be the distance at the point of collision...
		fixed_t dist = P_AproxDistance(distx, disty);
		fixed_t nx, ny;

		dist = dist ? dist : 1;

		nx = FixedDiv(distx, dist);
		ny = FixedDiv(disty, dist);

		distx = FixedMul(mobj1->radius + mobj2->radius, nx);
		disty = FixedMul(mobj1->radius + mobj2->radius, ny);

		if (momdifx == 0 && momdify == 0)
		{
			// If there's no momentum difference, they're moving at exactly the same rate. Pretend they moved into each other.
			momdifx = -nx;
			momdify = -ny;
		}
	}

	dot = FixedMul(momdifx, distx) + FixedMul(momdify, disty);

	if (dot >= 0)
	{
		// They're moving away from each other
		return 0;
	}

	// Return the push force!
	force = FixedDiv(dot, FixedMul(distx, distx) + FixedMul(disty, disty));

	return FixedMul(force, forceMul);
}

boolean K_KartBouncing(mobj_t *mobj1, mobj_t *mobj2)
{
	const fixed_t minBump = 25*mapobjectscale;
	mobj_t *goombaBounce = NULL;
	fixed_t distx, disty, dist;
	fixed_t force;
	fixed_t mass1, mass2;

	if ((!mobj1 || P_MobjWasRemoved(mobj1))
	|| (!mobj2 || P_MobjWasRemoved(mobj2)))
	{
		return false;
	}

	// Don't bump when you're being reborn
	if ((mobj1->player && mobj1->player->playerstate != PST_LIVE)
		|| (mobj2->player && mobj2->player->playerstate != PST_LIVE))
		return false;

	if ((mobj1->player && mobj1->player->respawn.state != RESPAWNST_NONE)
		|| (mobj2->player && mobj2->player->respawn.state != RESPAWNST_NONE))
		return false;


	// Don't bump if you're flashing
	INT32 flash;

	flash = K_GetKartFlashing(mobj1->player);
	if (mobj1->player && mobj1->player->flashing > 0 && mobj1->player->flashing < flash)
	{
		if (mobj1->player->flashing < flash-1)
			mobj1->player->flashing++;
		return false;
	}

	flash = K_GetKartFlashing(mobj2->player);
	if (mobj2->player && mobj2->player->flashing > 0 && mobj2->player->flashing < flash)
	{
		if (mobj2->player->flashing < flash-1)
			mobj2->player->flashing++;
		return false;
	}

	// Don't bump if you've recently bumped
	if (mobj1->player && mobj1->player->justbumped && !K_JustBumpedException(mobj2))
	{
		mobj1->player->justbumped = bumptime;
		return false;
	}

	if (mobj2->player && mobj2->player->justbumped && !K_JustBumpedException(mobj1))
	{
		mobj2->player->justbumped = bumptime;
		return false;
	}

	// Adds the OTHER object's momentum times a bunch, for the best chance of getting the correct direction
	distx = (mobj1->x + mobj2->momx) - (mobj2->x + mobj1->momx);
	disty = (mobj1->y + mobj2->momy) - (mobj2->y + mobj1->momy);

	force = K_GetBounceForce(mobj1, mobj2, distx, disty);

	if (force == 0)
	{
		return false;
	}

	mass1 = K_GetMobjWeight(mobj1, mobj2);
	mass2 = K_GetMobjWeight(mobj2, mobj1);

	if ((P_IsObjectOnGround(mobj1) && mobj2->momz < 0) // Grounded
		|| (mass2 == 0 && mass1 > 0)) // The other party is immovable
	{
		goombaBounce = mobj2;
	}
	else if ((P_IsObjectOnGround(mobj2) && mobj1->momz < 0) // Grounded
		|| (mass1 == 0 && mass2 > 0)) // The other party is immovable
	{
		goombaBounce = mobj1;
	}

	if (goombaBounce != NULL)
	{
		// Perform a Goomba Bounce by reversing your z momentum.
		goombaBounce->momz = -goombaBounce->momz;
	}
	else
	{
		// Trade z momentum values.
		fixed_t newz = mobj1->momz;
		mobj1->momz = mobj2->momz;
		mobj2->momz = newz;
	}

	// Multiply by force
	distx = FixedMul(force, distx);
	disty = FixedMul(force, disty);
	dist = FixedHypot(distx, disty);

	// if the speed difference is less than this let's assume they're going proportionately faster from each other
	if (dist < minBump)
	{
		fixed_t normalisedx = FixedDiv(distx, dist);
		fixed_t normalisedy = FixedDiv(disty, dist);

		distx = FixedMul(minBump, normalisedx);
		disty = FixedMul(minBump, normalisedy);
	}

	if (mass2 > 0)
	{
		mobj1->momx = mobj1->momx - FixedMul(FixedDiv(2*mass2, mass1 + mass2), distx);
		mobj1->momy = mobj1->momy - FixedMul(FixedDiv(2*mass2, mass1 + mass2), disty);
	}

	if (mass1 > 0)
	{
		mobj2->momx = mobj2->momx - FixedMul(FixedDiv(2*mass1, mass1 + mass2), -distx);
		mobj2->momy = mobj2->momy - FixedMul(FixedDiv(2*mass1, mass1 + mass2), -disty);
	}

	K_SpawnBumpForObjs(mobj1, mobj2);

	K_PlayerJustBumped(mobj1->player);
	K_PlayerJustBumped(mobj2->player);

	return true;
}

// K_KartBouncing, but simplified to act like P_BouncePlayerMove
boolean K_KartSolidBounce(mobj_t *bounceMobj, mobj_t *solidMobj)
{
	const fixed_t minBump = 25*mapobjectscale;
	fixed_t distx, disty;
	fixed_t force;

	if ((!bounceMobj || P_MobjWasRemoved(bounceMobj))
	|| (!solidMobj || P_MobjWasRemoved(solidMobj)))
	{
		return false;
	}

	// Don't bump when you're being reborn
	if (bounceMobj->player && bounceMobj->player->playerstate != PST_LIVE)
		return false;

	if (bounceMobj->player && bounceMobj->player->respawn.state != RESPAWNST_NONE)
		return false;

	// Don't bump if you've recently bumped
	if (bounceMobj->player && bounceMobj->player->justbumped && !K_JustBumpedException(solidMobj))
	{
		bounceMobj->player->justbumped = bumptime;
		return false;
	}

	// Adds the OTHER object's momentum times a bunch, for the best chance of getting the correct direction
	{
		distx = (bounceMobj->x + solidMobj->momx) - (solidMobj->x + bounceMobj->momx);
		disty = (bounceMobj->y + solidMobj->momy) - (solidMobj->y + bounceMobj->momy);
	}

	force = K_GetBounceForce(bounceMobj, solidMobj, distx, disty);

	if (force == 0)
	{
		return false;
	}

	{
		// Normalize to the desired push value.
		fixed_t normalisedx;
		fixed_t normalisedy;
		fixed_t bounceSpeed;

		// Multiply by force
		distx = FixedMul(force, distx);
		disty = FixedMul(force, disty);
		fixed_t dist = FixedHypot(distx, disty);

		normalisedx = FixedDiv(distx, dist);
		normalisedy = FixedDiv(disty, dist);

		if (solidMobj->type == MT_WALLSPIKE)
		{
			fixed_t co = FCOS(solidMobj->angle);
			fixed_t si = FSIN(solidMobj->angle);

			// Always thrust out toward the tip
			normalisedx = FixedMul(normalisedx, abs(si)) - co;
			normalisedy = FixedMul(normalisedy, abs(co)) - si;
		}

		bounceSpeed = FixedHypot(bounceMobj->momx, bounceMobj->momy);
		bounceSpeed = FixedMul(bounceSpeed, (FRACUNIT - (FRACUNIT>>2) - (FRACUNIT>>3)));
		bounceSpeed += minBump;

		distx = FixedMul(bounceSpeed, normalisedx);
		disty = FixedMul(bounceSpeed, normalisedy);
	}

	bounceMobj->momx = bounceMobj->momx - distx;
	bounceMobj->momy = bounceMobj->momy - disty;
	bounceMobj->momz = -bounceMobj->momz;

	K_SpawnBumpForObjs(bounceMobj, solidMobj);
	K_PlayerJustBumped(bounceMobj->player);

	return true;
}

/**	\brief	Checks that the player is on an offroad subsector for realsies. Also accounts for line riding to prevent cheese.

	\param	mo	player mobj object

	\return	boolean
*/
static fixed_t K_CheckOffroadCollide(mobj_t *mo)
{
	// Check for sectors in touching_sectorlist
	msecnode_t *node;	// touching_sectorlist iter
	sector_t *s;		// main sector shortcut
	sector_t *s2;		// FOF sector shortcut
	ffloor_t *rover;	// FOF

	fixed_t flr;
	fixed_t cel;	// floor & ceiling for height checks to make sure we're touching the offroad sector.

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (node = mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		if (!node->m_sector)
			break;	// shouldn't happen.

		s = node->m_sector;
		// 1: Check for the main sector, make sure we're on the floor of that sector and see if we can apply offroad.
		// Make arbitrary Z checks because we want to check for 1 sector in particular, we don't want to affect the player if the offroad sector is way below them and they're lineriding a normal sector above.

		flr = P_MobjFloorZ(mo, s, s, mo->x, mo->y, NULL, false, true);
		cel = P_MobjCeilingZ(mo, s, s, mo->x, mo->y, NULL, true, true);	// get Z coords of both floors and ceilings for this sector (this accounts for slopes properly.)
		// NOTE: we don't use P_GetZAt with our x/y directly because the mobj won't have the same height because of its hitbox on the slope. Complex garbage but tldr it doesn't work.

		if ( ((s->flags & MSF_FLIPSPECIAL_FLOOR) && mo->z == flr)	// floor check
			|| ((mo->eflags & MFE_VERTICALFLIP && (s->flags & MSF_FLIPSPECIAL_CEILING) && (mo->z + mo->height) == cel)) )	// ceiling check.
		{
			return s->offroad;
		}

		// 2: If we're here, we haven't found anything. So let's try looking for FOFs in the sectors using the same logic.
		for (rover = s->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))	// This FOF doesn't exist anymore.
				continue;

			s2 = &sectors[rover->secnum];	// makes things easier for us

			flr = P_GetFOFBottomZ(mo, s, rover, mo->x, mo->y, NULL);
			cel = P_GetFOFTopZ(mo, s, rover, mo->x, mo->y, NULL);	// Z coords for fof top/bottom.

			// we will do essentially the same checks as above instead of bothering with top/bottom height of the FOF.
			// Reminder that an FOF's floor is its bottom, silly!
			if ( ((s2->flags & MSF_FLIPSPECIAL_FLOOR) && mo->z == cel)	// "floor" check
				|| ((s2->flags & MSF_FLIPSPECIAL_CEILING) && (mo->z + mo->height) == flr) )	// "ceiling" check.
			{
				return s2->offroad;
			}
		}
	}

	return 0; // couldn't find any offroad
}

/**	\brief	Updates the Player's offroad value once per frame

	\param	player	player object passed from K_KartPlayerThink

	\return	void
*/
static void K_UpdateOffroad(player_t *player)
{
	fixed_t offroad;
	terrain_t *terrain = player->mo->terrain;
	fixed_t offroadstrength = 0;

	// If inside an ice cube, don't
	if (player->icecube.frozen == false)
	{
		// TODO: Make this use actual special touch code.
		if (terrain != NULL && terrain->offroad > 0)
		{
			offroadstrength = (terrain->offroad << FRACBITS);
		}
		else
		{
			offroadstrength = K_CheckOffroadCollide(player->mo);
		}
	}

	// If you are in offroad, a timer starts.
	if (offroadstrength)
	{
		if (player->offroad == 0)
			player->offroad = (TICRATE/2);

		if (player->offroad > 0)
		{
			offroad = (offroadstrength) / (TICRATE/2);

			player->offroad += offroad;
		}

		if (player->offroad > (offroadstrength))
			player->offroad = (offroadstrength);

		if (player->roundconditions.touched_offroad == false
			&& !(player->exiting || (player->pflags & PF_NOCONTEST))
			&& player->offroad > (2*offroadstrength) / TICRATE)
		{
			player->roundconditions.touched_offroad = true;
			player->roundconditions.checkthisframe = true;
		}
	}
	else
		player->offroad = 0;
}

void K_KartPainEnergyFling(player_t *player)
{
	static const UINT8 numfling = 5;
	INT32 i;
	mobj_t *mo;
	angle_t fa;
	fixed_t ns;
	fixed_t z;

	// Better safe than sorry.
	if (!player)
		return;

	// P_PlayerRingBurst: "There's no ring spilling in kart, so I'm hijacking this for the same thing as TD"
	// :oh:

	for (i = 0; i < numfling; i++)
	{
		INT32 objType = mobjinfo[MT_FLINGENERGY].reactiontime;
		fixed_t momxy, momz; // base horizonal/vertical thrusts

		z = player->mo->z;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - mobjinfo[objType].height;

		mo = P_SpawnMobj(player->mo->x, player->mo->y, z, objType);

		mo->fuse = 8*TICRATE;
		P_SetTarget(&mo->target, player->mo);

		mo->destscale = player->mo->scale;
		P_SetScale(mo, player->mo->scale);

		// Angle offset by player angle, then slightly offset by amount of fling
		fa = ((i*FINEANGLES/16) + (player->mo->angle>>ANGLETOFINESHIFT) - ((numfling-1)*FINEANGLES/32)) & FINEMASK;

		if (i > 15)
		{
			momxy = 3*FRACUNIT;
			momz = 4*FRACUNIT;
		}
		else
		{
			momxy = 28*FRACUNIT;
			momz = 3*FRACUNIT;
		}

		ns = FixedMul(momxy, mo->scale);
		mo->momx = FixedMul(FINECOSINE(fa),ns);

		ns = momz;
		P_SetObjectMomZ(mo, ns, false);

		if (i & 1)
			P_SetObjectMomZ(mo, ns, true);

		if (player->mo->eflags & MFE_VERTICALFLIP)
			mo->momz *= -1;
	}
}

// Adds gravity flipping to an object relative to its master and shifts the z coordinate accordingly.
void K_FlipFromObject(mobj_t *mo, mobj_t *master)
{
	mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP)|(master->eflags & MFE_VERTICALFLIP);
	mo->flags2 = (mo->flags2 & ~MF2_OBJECTFLIP)|(master->flags2 & MF2_OBJECTFLIP);

	if (mo->eflags & MFE_VERTICALFLIP)
		mo->z += master->height - FixedMul(master->scale, mo->height);
}

void K_MatchGenericExtraFlags(mobj_t *mo, mobj_t *master)
{
	// flipping
	// handle z shifting from there too. This is here since there's no reason not to flip us if needed when we do this anyway;
	K_FlipFromObject(mo, master);

	// visibility (usually for hyudoro)
	mo->renderflags = (mo->renderflags & ~RF_DONTDRAW) | (master->renderflags & RF_DONTDRAW);
}

// same as above, but does not adjust Z height when flipping
void K_GenericExtraFlagsNoZAdjust(mobj_t *mo, mobj_t *master)
{
	// flipping
	mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP)|(master->eflags & MFE_VERTICALFLIP);
	mo->flags2 = (mo->flags2 & ~MF2_OBJECTFLIP)|(master->flags2 & MF2_OBJECTFLIP);

	// visibility (usually for hyudoro)
	mo->renderflags = (mo->renderflags & ~RF_DONTDRAW) | (master->renderflags & RF_DONTDRAW);
}


void K_SpawnDashDustRelease(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	mobj_t *dust;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (!P_IsObjectOnGround(player->mo))
		return;

	if (!player->speed && !player->startboost)
		return;

	travelangle = player->mo->angle;

	if (player->drift || (player->pflags & PF_DRIFTEND))
		travelangle -= (ANGLE_45/5)*player->drift;

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_90, FixedMul(48*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_90, FixedMul(48*FRACUNIT, player->mo->scale));
		dust = P_SpawnMobj(newx, newy, player->mo->z, MT_FASTDUST);

		P_SetTarget(&dust->target, player->mo);
		dust->angle = travelangle - (((i&1) ? -1 : 1) * ANGLE_45);
		dust->destscale = player->mo->scale;
		P_SetScale(dust, player->mo->scale);

		dust->momx = 3*player->mo->momx/5;
		dust->momy = 3*player->mo->momy/5;
		dust->momz = 3*P_GetMobjZMovement(player->mo)/5;

		K_MatchGenericExtraFlags(dust, player->mo);
	}
}

static fixed_t K_GetBrakeFXScale(player_t *player, fixed_t maxScale)
{
	fixed_t s = FixedDiv(player->speed,
			K_GetKartSpeed(player, false, false));

	s = max(s, FRACUNIT);
	s = min(s, maxScale);

	return s;
}

static void K_SpawnBrakeDriftSparks(player_t *player) // Be sure to update the mobj thinker case too!
{
	mobj_t *sparks;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	// Position & etc are handled in its thinker, and its spawned invisible.
	// This avoids needing to dupe code if we don't need it.
	sparks = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BRAKEDRIFT);
	P_SetTarget(&sparks->target, player->mo);
	P_SetScale(sparks, (sparks->destscale = FixedMul(K_GetBrakeFXScale(player, 3*FRACUNIT), player->mo->scale)));
	K_MatchGenericExtraFlags(sparks, player->mo);
	sparks->renderflags |= RF_DONTDRAW;
}

static void K_SpawnGenericSpeedLines(player_t *player)
{
	mobj_t *fast = P_SpawnMobj(player->mo->x + (P_RandomRange(PR_DECORATION,-36,36) * player->mo->scale),
		player->mo->y + (P_RandomRange(PR_DECORATION,-36,36) * player->mo->scale),
		player->mo->z + (player->mo->height/2) + (P_RandomRange(PR_DECORATION,-20,20) * player->mo->scale),
		MT_FASTLINE);

	P_SetTarget(&fast->target, player->mo);
	fast->momx = 3*player->mo->momx/4;
	fast->momy = 3*player->mo->momy/4;
	fast->momz = 3*P_GetMobjZMovement(player->mo)/4;

	fast->z += player->mo->sprzoff;
	fast->angle = K_MomentumAngle(player->mo);

	K_MatchGenericExtraFlags(fast, player->mo);
	P_SetTarget(&fast->owner, player->mo);
	fast->renderflags |= RF_REDUCEVFX;

	if (player->invincibilitytimer)
	{
		const tic_t defaultTime = itemtime+(2*TICRATE);
		if (player->invincibilitytimer > defaultTime)
		{
			fast->color = player->mo->color;
		}
		else
		{
			fast->color = SKINCOLOR_INVINCFLASH;
		}
		fast->colorized = true;
	}
	else if (player->tripwireLeniency)
	{
		// Make it pink+blue+big when you can go through tripwire
		fast->color = (leveltime & 1) ? SKINCOLOR_LILAC : SKINCOLOR_JAWZ;
		fast->colorized = true;
		fast->renderflags |= RF_ADD;
	}
	else if (player->startboost)
	{
		fast->color = SKINCOLOR_PINETREE;
		fast->colorized = true;
		fast->renderflags |= RF_ADD;
	}
	else if (player->driftboost)
	{
		if (player->driftboost <= 20)
			fast->color = SKINCOLOR_SAPPHIRE;
		else if (player->driftboost <= 50)
			fast->color = SKINCOLOR_KETCHUP;
		else
			fast->color = K_RainbowColor(leveltime);


		fast->colorized = true;
		if (player->driftboost > 20)
			fast->renderflags |= RF_ADD;
		else
			fast->renderflags &= ~RF_ADD;
	}
	else if (player->ringboost)
	{
		fast->color = SKINCOLOR_BANANA;
		fast->colorized = true;
		fast->renderflags |= RF_ADD;
	}
}

void K_SpawnNormalSpeedLines(player_t *player)
{
	K_SpawnGenericSpeedLines(player);
}

void K_SpawnBumpEffect(mobj_t *mo)
{

	mobj_t *fx = P_SpawnMobj(mo->x, mo->y, mo->z, MT_BUMP);

	if (mo->eflags & MFE_VERTICALFLIP)
		fx->eflags |= MFE_VERTICALFLIP;
	else
		fx->eflags &= ~MFE_VERTICALFLIP;

	fx->scale = mo->scale;

		S_StartSound(mo, sfx_s3k49);
}

void K_SpawnMagicianParticles(mobj_t *mo, int spread)
{
	INT32 i;
	mobj_t *target = mo->target;

	if (!target || P_MobjWasRemoved(target))
		target = mo;

	for (i = 0; i < 16; i++)
	{
		fixed_t hmomentum = P_RandomRange(PR_DECORATION, spread * -1, spread) * mo->scale;
		fixed_t vmomentum = P_RandomRange(PR_DECORATION, spread * -1, spread) * mo->scale;
		UINT16 color = P_RandomKey(PR_DECORATION, numskincolors);

		fixed_t ang = FixedAngle(P_RandomRange(PR_DECORATION, 0, 359)*FRACUNIT);
		SINT8 flip = 1;

		mobj_t *dust;

		if (i & 1)
			ang -= ANGLE_90;
		else
			ang += ANGLE_90;

		// sprzoff for Garden Top!!
		dust = P_SpawnMobjFromMobjUnscaled(mo,
			FixedMul(mo->radius / 4, FINECOSINE(ang >> ANGLETOFINESHIFT)),
			FixedMul(mo->radius / 4, FINESINE(ang >> ANGLETOFINESHIFT)),
			(target->height / 4) + target->sprzoff, (i%3 == 0) ? MT_SIGNSPARKLE : MT_DUST
		);
		flip = P_MobjFlip(dust);

		dust->momx = target->momx + FixedMul(hmomentum, FINECOSINE(ang >> ANGLETOFINESHIFT));
		dust->momy = target->momy + FixedMul(hmomentum, FINESINE(ang >> ANGLETOFINESHIFT));
		dust->momz = vmomentum * flip;
		dust->scale = dust->scale*4;
		dust->frame |= FF_SUBTRACT|FF_TRANS90;
		dust->color = color;
		dust->colorized = true;
	}
}

static SINT8 K_GlanceAtPlayers(player_t *glancePlayer, boolean horn)
{
	const fixed_t maxdistance = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	const angle_t blindSpotSize = ANG10; // ANG5
	SINT8 glanceDir = 0;
	SINT8 lastValidGlance = 0;
	const boolean podiumspecial = (K_PodiumSequence() == true && glancePlayer->nextwaypoint == NULL && glancePlayer->speed == 0);
	boolean mysticmelodyspecial = false;

	if (podiumspecial)
	{
		if (glancePlayer->position > 3)
		{
			// Loser valley, focused on the mountain.
			return 0;
		}

		if (glancePlayer->position == 1)
		{
			// Sitting on the stand, I ammm thebest!
			return 0;
		}
	}

	// See if there's any players coming up behind us.
	// If so, your character will glance at 'em.
	mobj_t *victim = NULL, *victimnext = NULL;

	for (victim = trackercap; victim; victim = victimnext)
	{
		player_t *p = victim->player;
		angle_t back;
		angle_t diff;
		fixed_t distance;
		SINT8 dir = -1;

		victimnext = victim->itnext;

		if (p != NULL)
		{
			if (p == glancePlayer)
			{
				// FOOL! Don't glance at yerself!
				continue;
			}

			if (p->spectator || p->hyudorotimer > 0)
			{
				// Not playing / invisible
				continue;
			}

			if (podiumspecial && p->position >= glancePlayer->position)
			{
				// On the podium, only look with envy, not condesencion
				continue;
			}
		}
		else if (victim->type != MT_ANCIENTSHRINE)
		{
			// Ancient Shrines are a special exception to glance logic.
			continue;
		}

		if (!podiumspecial)
		{
			distance = R_PointToDist2(glancePlayer->mo->x, glancePlayer->mo->y, victim->x, victim->y);
			distance = R_PointToDist2(0, glancePlayer->mo->z, distance, victim->z);

			if (distance > maxdistance)
			{
				// Too far away
				continue;
			}
		}

		back = glancePlayer->mo->angle + ANGLE_180;
		diff = R_PointToAngle2(glancePlayer->mo->x, glancePlayer->mo->y, victim->x, victim->y) - back;

		if (diff > ANGLE_180)
		{
			diff = InvAngle(diff);
			dir = -dir;
		}

		if (diff > (podiumspecial ? (ANGLE_180 - blindSpotSize) : ANGLE_90))
		{
			// Not behind the player
			continue;
		}

		if (diff < blindSpotSize)
		{
			// Small blindspot directly behind your back, gives the impression of smoothly turning.
			continue;
		}

		if (!podiumspecial && P_CheckSight(glancePlayer->mo, victim) == false)
		{
			// Blocked by a wall, we can't glance at 'em!
			continue;
		}

		// Adds, so that if there's more targets on one of your sides, it'll glance on that side.
		glanceDir += dir;

		// That poses a limitation if there's an equal number of targets on both sides...
		// In that case, we'll pick the last chosen glance direction.
		lastValidGlance = dir;

		if (horn == true)
		{
			if (p != NULL)
			{
				K_FollowerHornTaunt(glancePlayer, p, false);
			}
			else if (victim->type == MT_ANCIENTSHRINE)
			{
				mysticmelodyspecial = true;
			}
		}
	}

	if (horn == true && lastValidGlance != 0)
	{
		const boolean tasteful = (glancePlayer->karthud[khud_taunthorns] == 0);

		K_FollowerHornTaunt(glancePlayer, glancePlayer, mysticmelodyspecial);

		if (tasteful && glancePlayer->karthud[khud_taunthorns] < 2*TICRATE)
			glancePlayer->karthud[khud_taunthorns] = 2*TICRATE;
	}

	if (glanceDir > 0)
	{
		return 1;
	}
	else if (glanceDir < 0)
	{
		return -1;
	}

	return lastValidGlance;
}

/**	\brief Handles the state changing for moving players, moved here to eliminate duplicate code

	\param	player	player data

	\return	void
*/
void K_KartMoveAnimation(player_t *player)
{
	const INT16 minturn = KART_FULLTURN/8;

	const fixed_t fastspeed = (K_GetKartSpeed(player, false, true) * 17) / 20; // 85%
	const fixed_t speedthreshold = player->mo->scale / 8;

	const boolean onground = P_IsObjectOnGround(player->mo);

	UINT16 buttons = K_GetKartButtons(player);
	const boolean spinningwheels = (((buttons & BT_ACCELERATE) == BT_ACCELERATE) || (onground && player->speed > 0));
	const boolean lookback = ((buttons & BT_LOOKBACK) == BT_LOOKBACK);

	SINT8 turndir = 0;
	SINT8 destGlanceDir = 0;
	SINT8 drift = player->drift;

	if (!lookback)
	{
		player->pflags &= ~PF_GAINAX;

		if (player->cmd.driftturn < -minturn)
		{
			turndir = -1;
		}
		else if (player->cmd.driftturn > minturn)
		{
			turndir = 1;
		}
	}
	else if (drift == 0)
	{
		// Prioritize looking back frames over turning
		turndir = 0;
	}

	// Sliptides: drift -> lookback frames
	if (abs(player->aizdriftturn) >= ANGLE_90)
	{
		destGlanceDir = -(2*intsign(player->aizdriftturn));
		player->glanceDir = destGlanceDir;
		drift = turndir = 0;
		player->pflags &= ~PF_GAINAX;
	}
	else if (player->aizdriftturn)
	{
		drift = intsign(player->aizdriftturn);
		turndir = 0;
	}
	else if (turndir == 0 && drift == 0)
	{
		// Only try glancing if you're driving straight.
		// This avoids all-players loops when we don't need it.
		const boolean horn = lookback && !(player->pflags & PF_GAINAX);
		destGlanceDir = K_GlanceAtPlayers(player, horn);

		if (lookback == true)
		{
			statenum_t gainaxstate = S_GAINAX_TINY;
			if (destGlanceDir == 0)
			{
				if (player->glanceDir != 0 && player->cmd.driftturn != 0)
				{
					// Keep to the side you were already on.
					if (player->glanceDir < 0)
					{
						destGlanceDir = -1;
					}
					else
					{
						destGlanceDir = 1;
					}
				}
				else
				{
					// Nep: Improved this by allowing player turn input to influence head turn direction while looking backwards
					if (player->cmd.driftturn < -minturn)
					{
						destGlanceDir = -1;
					}
					else if (player->cmd.driftturn > minturn)
					{
						destGlanceDir = 1;
					}
					else if (player->cmd.forwardmove >= 0)
						destGlanceDir = 0; // Look to your foward by default
					else if (player->cmd.forwardmove < 0)
						destGlanceDir = -1; // You are revering look backwards
				}
			}
			else
			{
				// Looking back AND glancing? Amplify the look!
				destGlanceDir *= 2;
				if (player->itemamount && player->itemtype)
					gainaxstate = S_GAINAX_HUGE;
				else
					gainaxstate = S_GAINAX_MID1;
			}

			if (destGlanceDir && !(player->pflags & PF_GAINAX))
			{
				mobj_t *gainax = P_SpawnMobjFromMobj(player->mo, 0, 0, 0, MT_GAINAX);
				gainax->movedir = (destGlanceDir < 0) ? (ANGLE_270-ANG10) : (ANGLE_90+ANG10);
				P_SetTarget(&gainax->target, player->mo);
				P_SetMobjState(gainax, gainaxstate);
				gainax->flags2 |= MF2_AMBUSH;
				player->pflags |= PF_GAINAX;
			}
		}
		else if (K_GetForwardMove(player) < 0 && destGlanceDir == 0)
		{
			// Reversing -- like looking back, but doesn't stack on the other glances.
			if (player->glanceDir != 0)
			{
				// Keep to the side you were already on.
				if (player->glanceDir < 0)
				{
					destGlanceDir = -1;
				}
				else
				{
					destGlanceDir = 1;
				}
			}
			else
			{
				// Look to your right by default
				destGlanceDir = -1;
			}
		}
	}
	else
	{
		destGlanceDir = 0;
		player->glanceDir = 0;
	}

#define SetState(sn) \
	if (player->mo->state != &states[sn]) \
		P_SetPlayerMobjState(player->mo, sn)

	if (onground == false)
	{
		// Only use certain frames in the air, to make it look like your tires are spinning fruitlessly!

		if (drift > 0)
		{
			// Neutral drift
			SetState(S_KART_DRIFT_L);
		}
		else if (drift < 0)
		{
			// Neutral drift
			SetState(S_KART_DRIFT_R);
		}
		else
		{
			if (turndir == -1)
			{
				SetState(S_KART_FAST_R);
			}
			else if (turndir == 1)
			{
				SetState(S_KART_FAST_L);
			}
			else
			{
				switch (player->glanceDir)
				{
					case -2:
						SetState(S_KART_FAST_LOOK_R);
						break;
					case 2:
						SetState(S_KART_FAST_LOOK_L);
						break;
					case -1:
						SetState(S_KART_FAST_GLANCE_R);
						break;
					case 1:
						SetState(S_KART_FAST_GLANCE_L);
						break;
					default:
						SetState(S_KART_FAST);
						break;
				}
			}
		}

		if (!spinningwheels)
		{
			// TODO: The "tires still in the air" states should have it's own SPR2s.
			// This was a quick hack to get the same functionality with less work,
			// but it's really dunderheaded & isn't customizable at all.
			player->mo->frame = (player->mo->frame & ~FF_FRAMEMASK);
			player->mo->tics++; // Makes it properly use frame 0
		}
	}
	else
	{
		if (drift > 0)
		{
			// Drifting LEFT!

			if (turndir == -1)
			{
				// Right -- outwards drift
				SetState(S_KART_DRIFT_L_OUT);
			}
			else if (turndir == 1)
			{
				// Left -- inwards drift
				SetState(S_KART_DRIFT_L_IN);
			}
			else
			{
				// Neutral drift
				SetState(S_KART_DRIFT_L);
			}
		}
		else if (drift < 0)
		{
			// Drifting RIGHT!

			if (turndir == -1)
			{
				// Right -- inwards drift
				SetState(S_KART_DRIFT_R_IN);
			}
			else if (turndir == 1)
			{
				// Left -- outwards drift
				SetState(S_KART_DRIFT_R_OUT);
			}
			else
			{
				// Neutral drift
				SetState(S_KART_DRIFT_R);
			}
		}
		else
		{
			if (player->speed >= fastspeed && player->speed >= (player->lastspeed - speedthreshold))
			{
				// Going REAL fast!

				if (turndir == -1)
				{
					SetState(S_KART_FAST_R);
				}
				else if (turndir == 1)
				{
					SetState(S_KART_FAST_L);
				}
				else
				{
					switch (player->glanceDir)
					{
						case -2:
							SetState(S_KART_FAST_LOOK_R);
							break;
						case 2:
							SetState(S_KART_FAST_LOOK_L);
							break;
						case -1:
							SetState(S_KART_FAST_GLANCE_R);
							break;
						case 1:
							SetState(S_KART_FAST_GLANCE_L);
							break;
						default:
							SetState(S_KART_FAST);
							break;
					}
				}
			}
			else
			{
				if (spinningwheels)
				{
					// Drivin' slow.

					if (turndir == -1)
					{
						SetState(S_KART_SLOW_R);
					}
					else if (turndir == 1)
					{
						SetState(S_KART_SLOW_L);
					}
					else
					{
						switch (player->glanceDir)
						{
							case -2:
								SetState(S_KART_SLOW_LOOK_R);
								break;
							case 2:
								SetState(S_KART_SLOW_LOOK_L);
								break;
							case -1:
								SetState(S_KART_SLOW_GLANCE_R);
								break;
							case 1:
								SetState(S_KART_SLOW_GLANCE_L);
								break;
							default:
								SetState(S_KART_SLOW);
								break;
						}
					}
				}
				else
				{
					// Completely still.

					if (turndir == -1)
					{
						SetState(S_KART_STILL_R);
					}
					else if (turndir == 1)
					{
						SetState(S_KART_STILL_L);
					}
					else
					{
						switch (player->glanceDir)
						{
							case -2:
								SetState(S_KART_STILL_LOOK_R);
								break;
							case 2:
								SetState(S_KART_STILL_LOOK_L);
								break;
							case -1:
								SetState(S_KART_STILL_GLANCE_R);
								break;
							case 1:
								SetState(S_KART_STILL_GLANCE_L);
								break;
							default:
								SetState(S_KART_STILL);
								break;
						}
					}
				}
			}
		}
	}

#undef SetState

	// Update your glance value to smooth it out.
	if (player->glanceDir > destGlanceDir)
	{
		player->glanceDir--;
	}
	else if (player->glanceDir < destGlanceDir)
	{
		player->glanceDir++;
	}

	if (!player->glanceDir)
		player->pflags &= ~PF_GAINAX;

	// Update lastspeed value -- we use to display slow driving frames instead of fast driving when slowing down.
	player->lastspeed = player->speed;
}

static void K_TauntVoiceTimers(player_t *player)
{
	if (!player)
		return;

	player->karthud[khud_tauntvoices] = 6*TICRATE;
	player->karthud[khud_voices] = 4*TICRATE;
}

static void K_RegularVoiceTimers(player_t *player)
{
	if (!player)
		return;

	player->karthud[khud_voices] = 4*TICRATE;

	if (player->karthud[khud_tauntvoices] < 4*TICRATE)
		player->karthud[khud_tauntvoices] = 4*TICRATE;
}

static UINT8 K_ObjectToSkinIDForSounds(mobj_t *source)
{
	if (source->player)
		return source->player->skin;

	if (!source->skin)
		return MAXSKINS;

	return ((skin_t *)source->skin)-skins;
}

static void K_PlayGenericTastefulTaunt(mobj_t *source, sfxenum_t sfx_id)
{
	UINT8 skinid = K_ObjectToSkinIDForSounds(source);
	if (skinid >= numskins)
		return;

	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (
		(
			(cv_kartvoices.value && tasteful)
			|| cv_tastelesstaunts.value
		)
		&& R_CanShowSkinInDemo(skinid)
	)
	{
		S_StartSound(source, sfx_id);
	}

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayAttackTaunt(mobj_t *source)
{
	// Gotta roll the RNG every time this is called for sync reasons
	sfxenum_t pick = P_RandomKey(PR_VOICES, 2);
	K_PlayGenericTastefulTaunt(source, sfx_kattk1+pick);
}

void K_PlayBoostTaunt(mobj_t *source)
{
	// Gotta roll the RNG every time this is called for sync reasons
	sfxenum_t pick = P_RandomKey(PR_VOICES, 2);
	K_PlayGenericTastefulTaunt(source, sfx_kbost1+pick);
}

void K_PlayOvertakeSound(mobj_t *source)
{
	UINT8 skinid = K_ObjectToSkinIDForSounds(source);
	if (skinid >= numskins)
		return;

	boolean tasteful = (!source->player || !source->player->karthud[khud_voices]);

	if (
		(
			(cv_kartvoices.value && tasteful)
			|| cv_tastelesstaunts.value
		)
		&& R_CanShowSkinInDemo(skinid)
	)
	{
		S_StartSound(source, sfx_kslow);
	}

	if (!tasteful)
		return;

	K_RegularVoiceTimers(source->player);
}

static void K_PlayGenericCombatSound(mobj_t *source, mobj_t *other, sfxenum_t sfx_id)
{
	UINT8 skinid = K_ObjectToSkinIDForSounds(source);
	if (skinid >= numskins)
		return;

	if (
		(cv_kartvoices.value || cv_tastelesstaunts.value)
		&& R_CanShowSkinInDemo(skinid)
	)
	{
		S_StartSound(
			source,
			skins[skinid].soundsid[S_sfx[sfx_id].skinsound]
		);
	}

	K_RegularVoiceTimers(source->player);
}

void K_PlayPainSound(mobj_t *source, mobj_t *other)
{
	sfxenum_t pick = P_RandomKey(PR_VOICES, 2); // Gotta roll the RNG every time this is called for sync reasons
	K_PlayGenericCombatSound(source, other, sfx_khurt1 + pick);
}

void K_PlayHitEmSound(mobj_t *source, mobj_t *other)
{
	K_PlayGenericCombatSound(source, other, sfx_khitem);
}

void K_TryHurtSoundExchange(mobj_t *victim, mobj_t *attacker)
{
	if (victim == NULL || P_MobjWasRemoved(victim) == true || victim->player == NULL)
	{
		return;
	}

	// In a perfect world we could move this here, but there's
	// a few niche situations where we want a pain sound from
	// the victim, but no confirm sound from the attacker.
	// (ex: DMG_STING)

	//K_PlayPainSound(victim, attacker);

	if (attacker == NULL || P_MobjWasRemoved(attacker) == true || attacker->player == NULL)
	{
		return;
	}

	attacker->player->confirmVictim = (victim->player - players);
	attacker->player->confirmVictimDelay = TICRATE/2;

	const INT32 followerskin = K_GetEffectiveFollowerSkin(attacker->player);
	if (attacker->player->follower != NULL
		&& followerskin >= 0
		&& followerskin < numfollowers)
	{
		const follower_t *fl = &followers[followerskin];
		attacker->player->follower->movecount = fl->hitconfirmtime; // movecount is used to play the hitconfirm animation for followers.
	}
}

void K_PlayPowerGloatSound(mobj_t *source)
{
	UINT8 skinid = K_ObjectToSkinIDForSounds(source);
	if (skinid >= numskins)
		return;

	if (
		(cv_kartvoices.value || cv_tastelesstaunts.value)
		&& R_CanShowSkinInDemo(skinid)
	)
	{
		S_StartSound(source, sfx_kgloat);
	}

	K_RegularVoiceTimers(source->player);
}

// MOVED so we don't have to extern K_ObjectToSkinID
void P_PlayVictorySound(mobj_t *source)
{
	UINT8 skinid = K_ObjectToSkinIDForSounds(source);
	if (skinid >= numskins)
		return;

	if (
		(cv_kartvoices.value || cv_tastelesstaunts.value)
		&& R_CanShowSkinInDemo(skinid)
	)
	{
		S_StartSound(source, sfx_kwin);
	}
}

static void K_HandleDelayedHitByEm(player_t *player)
{
	if (player->confirmVictimDelay == 0)
	{
		return;
	}

	player->confirmVictimDelay--;

	if (player->confirmVictimDelay == 0)
	{
		mobj_t *victim = NULL;

		if (player->confirmVictim < MAXPLAYERS && playeringame[player->confirmVictim])
		{
			player_t *victimPlayer = &players[player->confirmVictim];

			if (victimPlayer != NULL && victimPlayer->spectator == false)
			{
				victim = victimPlayer->mo;
			}
		}

		K_PlayHitEmSound(player->mo, victim);
	}
}

void K_MomentumToFacing(player_t *player)
{
	angle_t dangle = player->mo->angle - K_MomentumAngleReal(player->mo);

	if (dangle > ANGLE_180)
		dangle = InvAngle(dangle);

	// If you aren't on the ground or are moving in too different of a direction don't do this
	if (player->mo->eflags & MFE_JUSTHITFLOOR)
		; // Just hit floor ALWAYS redirects
	else if (!P_IsObjectOnGround(player->mo) || dangle > ANGLE_90)
		return;

	P_Thrust(player->mo, player->mo->angle, player->speed - FixedMul(player->speed, player->mo->friction));
	player->mo->momx = FixedMul(player->mo->momx - player->cmomx, player->mo->friction) + player->cmomx;
	player->mo->momy = FixedMul(player->mo->momy - player->cmomy, player->mo->friction) + player->cmomy;
}

boolean K_ApplyOffroad(const player_t *player)
{
	if (player->invincibilitytimer || player->hyudorotimer || player->sneakertimer)
		return false;
	return true;
}

fixed_t K_PlayerTripwireSpeedThreshold(const player_t *player)
{
	fixed_t required_speed = 2 * K_GetKartSpeed(player, false, false); // 200%

	if (player->offroad && K_ApplyOffroad(player))
	{
		// Increase to 300% if you're lawnmowering.
		required_speed = (required_speed * 3) / 2;
	}

	if (player->botvars.rubberband > FRACUNIT && K_PlayerUsesBotMovement(player) == true)
	{
		// Make it harder for bots to do this when rubberbanding.

		// This is actually biased really hard against the bot,
		// because the bot rubberbanding speed increase is
		// decreased with other boosts.

		required_speed = FixedMul(required_speed, player->botvars.rubberband);
	}

	return required_speed;
}

tripwirepass_t K_TripwirePassConditions(const player_t *player)
{
	if (
			player->invincibilitytimer ||
			player->sneakertimer
		)
		return TRIPWIRE_BLASTER;

	if (
			player->flamedash ||
			((player->speed > K_PlayerTripwireSpeedThreshold(player)) && player->tripwireReboundDelay == 0) ||
			player->fakeBoost
	)
		return TRIPWIRE_BOOST;

	if (
			player->growshrinktimer > 0 ||
			player->hyudorotimer
		)
		return TRIPWIRE_IGNORE;

	return TRIPWIRE_NONE;
}

boolean K_TripwirePass(const player_t *player)
{
	return (player->tripwirePass != TRIPWIRE_NONE);
}

boolean K_MovingHorizontally(mobj_t *mobj)
{
	return (P_AproxDistance(mobj->momx, mobj->momy) / 4 > abs(mobj->momz));
}

boolean K_WaterRun(mobj_t *mobj)
{
	switch (mobj->type)
	{
		case MT_ORBINAUT:
		{
			if (Obj_OrbinautCanRunOnWater(mobj))
			{
				return true;
			}

			return false;
		}

		case MT_JAWZ:
		{
			if (mobj->tracer != NULL && P_MobjWasRemoved(mobj->tracer) == false)
			{
				fixed_t jawzFeet = P_GetMobjFeet(mobj);
				fixed_t chaseFeet = P_GetMobjFeet(mobj->tracer);
				fixed_t footDiff = (chaseFeet - jawzFeet) * P_MobjFlip(mobj);

				// Water run if the player we're chasing is above/equal to us.
				// Start water skipping if they're underneath the water.
				return (footDiff > -mobj->tracer->height);
			}

			return false;
		}

		case MT_PLAYER:
		{
			fixed_t minspeed = 0;

			if (mobj->player == NULL)
			{
				return false;
			}

			minspeed = K_PlayerTripwireSpeedThreshold(mobj->player);

			if (mobj->player->speed < minspeed / 5) // 40%
			{
				return false;
			}

			if (mobj->player->invincibilitytimer
				|| mobj->player->sneakertimer
				|| mobj->player->flamedash
				|| mobj->player->speed > minspeed)
			{
				return true;
			}

			return false;
		}

		default:
		{
			return false;
		}
	}
}

boolean K_WaterSkip(mobj_t *mobj)
{
	if (mobj->waterskip >= 2)
	{
		// Already finished waterskipping.
		return false;
	}

	switch (mobj->type)
	{
		case MT_PLAYER:
		{
			// Allow
			break;
		}
		case MT_ORBINAUT:
		case MT_JAWZ:
		case MT_BALLHOG:
		{
			// Allow
			break;
		}

		default:
		{
			// Don't allow
			return false;
		}
	}

	if (mobj->waterskip > 0)
	{
		// Already waterskipping.
		// Simply make sure you haven't slowed down drastically.
		return (P_AproxDistance(mobj->momx, mobj->momy) > 20 * mapobjectscale);
	}
	else
	{
		// Need to be moving horizontally and not vertically
		// to be able to start a water skip.
		return K_MovingHorizontally(mobj);
	}
}

void K_SpawnWaterRunParticles(mobj_t *mobj)
{
	fixed_t runSpeed = 14 * mobj->scale;
	fixed_t curSpeed = INT32_MAX;
	fixed_t topSpeed = INT32_MAX;
	fixed_t trailScale = FRACUNIT;

	if (mobj->momz != 0)
	{
		// Only while touching ground.
		return;
	}

	if (mobj->watertop == INT32_MAX || mobj->waterbottom == INT32_MIN)
	{
		// Invalid water plane.
		return;
	}

	if (mobj->player != NULL)
	{
		if (mobj->player->spectator)
		{
			// Not as spectator.
			return;
		}

		if (mobj->player->carry == CR_SLIDING)
		{
			// Not in water slides.
			return;
		}

		topSpeed = K_GetKartSpeed(mobj->player, false, false);
		runSpeed = FixedMul(runSpeed, mobj->movefactor);
	}
	else
	{
		topSpeed = FixedMul(mobj->scale, K_GetKartSpeedFromStat(5));
	}

	curSpeed = P_AproxDistance(mobj->momx, mobj->momy);

	if (curSpeed <= runSpeed)
	{
		// Not fast enough.
		return;
	}

	// Near the water plane.
	if ((!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z + mobj->height >= mobj->watertop && mobj->z <= mobj->watertop)
		|| (mobj->eflags & MFE_VERTICALFLIP && mobj->z + mobj->height >= mobj->waterbottom && mobj->z <= mobj->waterbottom))
	{
		if (topSpeed > runSpeed)
		{
			trailScale = FixedMul(FixedDiv(curSpeed - runSpeed, topSpeed - runSpeed), mapobjectscale);
		}
		else
		{
			trailScale = mapobjectscale; // Scaling is based off difference between runspeed and top speed
		}

		if (trailScale > 0)
		{
			const angle_t forwardangle = K_MomentumAngle(mobj);
			const fixed_t playerVisualRadius = mobj->radius + (8 * mobj->scale);
			const size_t numFrames = S_WATERTRAIL8 - S_WATERTRAIL1;
			const statenum_t curOverlayFrame = S_WATERTRAIL1 + (leveltime % numFrames);
			const statenum_t curUnderlayFrame = S_WATERTRAILUNDERLAY1 + (leveltime % numFrames);
			fixed_t x1, x2, y1, y2;
			mobj_t *water;

			x1 = mobj->x + mobj->momx + P_ReturnThrustX(mobj, forwardangle + ANGLE_90, playerVisualRadius);
			y1 = mobj->y + mobj->momy + P_ReturnThrustY(mobj, forwardangle + ANGLE_90, playerVisualRadius);
			x1 = x1 + P_ReturnThrustX(mobj, forwardangle, playerVisualRadius);
			y1 = y1 + P_ReturnThrustY(mobj, forwardangle, playerVisualRadius);

			x2 = mobj->x + mobj->momx + P_ReturnThrustX(mobj, forwardangle - ANGLE_90, playerVisualRadius);
			y2 = mobj->y + mobj->momy + P_ReturnThrustY(mobj, forwardangle - ANGLE_90, playerVisualRadius);
			x2 = x2 + P_ReturnThrustX(mobj, forwardangle, playerVisualRadius);
			y2 = y2 + P_ReturnThrustY(mobj, forwardangle, playerVisualRadius);

			// Left
			// underlay
			water = P_SpawnMobj(x1, y1,
				((mobj->eflags & MFE_VERTICALFLIP) ? mobj->waterbottom - FixedMul(mobjinfo[MT_WATERTRAILUNDERLAY].height, mobj->scale) : mobj->watertop), MT_WATERTRAILUNDERLAY);
			water->angle = forwardangle - ANGLE_180 - ANGLE_22h;
			water->destscale = trailScale;
			water->momx = mobj->momx;
			water->momy = mobj->momy;
			water->momz = mobj->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curUnderlayFrame);
			P_SetTarget(&water->owner, mobj);
			water->renderflags |= RF_REDUCEVFX;

			// overlay
			water = P_SpawnMobj(x1, y1,
				((mobj->eflags & MFE_VERTICALFLIP) ? mobj->waterbottom - FixedMul(mobjinfo[MT_WATERTRAIL].height, mobj->scale) : mobj->watertop), MT_WATERTRAIL);
			water->angle = forwardangle - ANGLE_180 - ANGLE_22h;
			water->destscale = trailScale;
			water->momx = mobj->momx;
			water->momy = mobj->momy;
			water->momz = mobj->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curOverlayFrame);
			P_SetTarget(&water->owner, mobj);
			water->renderflags |= RF_REDUCEVFX;

			// Right
			// Underlay
			water = P_SpawnMobj(x2, y2,
				((mobj->eflags & MFE_VERTICALFLIP) ? mobj->waterbottom - FixedMul(mobjinfo[MT_WATERTRAILUNDERLAY].height, mobj->scale) : mobj->watertop), MT_WATERTRAILUNDERLAY);
			water->angle = forwardangle - ANGLE_180 + ANGLE_22h;
			water->destscale = trailScale;
			water->momx = mobj->momx;
			water->momy = mobj->momy;
			water->momz = mobj->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curUnderlayFrame);
			P_SetTarget(&water->owner, mobj);
			water->renderflags |= RF_REDUCEVFX;

			// Overlay
			water = P_SpawnMobj(x2, y2,
				((mobj->eflags & MFE_VERTICALFLIP) ? mobj->waterbottom - FixedMul(mobjinfo[MT_WATERTRAIL].height, mobj->scale) : mobj->watertop), MT_WATERTRAIL);
			water->angle = forwardangle - ANGLE_180 + ANGLE_22h;
			water->destscale = trailScale;
			water->momx = mobj->momx;
			water->momy = mobj->momy;
			water->momz = mobj->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curOverlayFrame);
			P_SetTarget(&water->owner, mobj);
			water->renderflags |= RF_REDUCEVFX;

			if (!S_SoundPlaying(mobj, sfx_s3kdbs))
			{
				const INT32 volume = (min(trailScale, FRACUNIT) * 255) / FRACUNIT;
				S_ReducedVFXSoundAtVolume(mobj, sfx_s3kdbs, volume, mobj->player);
			}
		}

		// Little water sound while touching water - just a nicety.
		if ((mobj->eflags & MFE_TOUCHWATER) && !(mobj->eflags & MFE_UNDERWATER))
		{
			if (P_RandomChance(PR_BUBBLE, FRACUNIT/2) && leveltime % TICRATE == 0)
			{
				S_ReducedVFXSound(mobj, sfx_floush, mobj->player);
			}
		}
	}
}

static fixed_t K_FlameShieldDashVar(INT32 val)
{
	// 1 second = 75% + 50% top speed
	return (3*FRACUNIT/4) + (((val * FRACUNIT) / TICRATE));
}

static fixed_t K_RingDurationBoost(const player_t *player)
{
	fixed_t ret = FRACUNIT;

	if (K_PlayerUsesBotMovement(player))
	{
		// x2.0 for Lv. 9
		const fixed_t modifier = K_BotMapModifier();
		fixed_t add = ((player->botvars.difficulty-1) * modifier) / (DIFFICULTBOT-1);

		ret += add;

		if (player->botvars.rival == true || cv_levelskull.value)
		{
			// x2.0 for Rival
			ret *= 2;
		}
	}

	return ret;
}

// sets k_boostpower, k_speedboost, and k_accelboost to whatever we need it to be
static void K_GetKartBoostPower(player_t *player)
{
	fixed_t boostpower = FRACUNIT;
	fixed_t speedboost = 0, accelboost = 0;

	if (player->spinouttimer && player->wipeoutslow == 1) // Slow down after you've been bumped
	{
		player->boostpower = player->speedboost = player->accelboost = 0;
		return;
	}

	// Offroad is separate, it's difficult to factor it in with a variable value anyway.
	if (!(player->invincibilitytimer || player->hyudorotimer || player->sneakertimer)
		&& player->offroad >= 0)
		boostpower = FixedDiv(boostpower, player->offroad + FRACUNIT);

	if (player->bananadrag > TICRATE)
		boostpower = (4*boostpower)/5;

	// Banana drag/offroad dust
	if (boostpower < FRACUNIT
		&& player->mo && P_IsObjectOnGround(player->mo)
		&& player->speed > 0
		&& !player->spectator)
	{
		K_SpawnWipeoutTrail(player->mo);
		if (leveltime % 6 == 0)
			S_StartSound(player->mo, sfx_cdfm70);
	}

	if (player->sneakertimer) // Sneaker
	{
		switch (gamespeed)
		{
			case 0:
				speedboost = max(speedboost, 53740+768);
				break;
			case 2:
				speedboost = max(speedboost, 17294+768);
				break;
			default:
				speedboost = max(speedboost, 32768);
				break;
		}
		accelboost = max(accelboost, 8*FRACUNIT); // + 800%
	}

	if (player->invincibilitytimer) // Invincibility
	{
		speedboost = max(speedboost, 3*FRACUNIT/8); // + 37.5%
		accelboost = max(accelboost, 3*FRACUNIT); // + 300%
	}

	if (player->growshrinktimer > 0) // Grow
	{
		speedboost = max(speedboost, FRACUNIT/5); // + 20%
	}

	if (player->flamedash) // Flame Shield dash
	{
		fixed_t dash = K_FlameShieldDashVar(player->flamedash);
		fixed_t intermediate = 0;
		fixed_t boost = 0;

		intermediate = FixedDiv(FixedMul(FLOAT_TO_FIXED(.60), FRACUNIT*-1/2) - FRACUNIT/4,-FLOAT_TO_FIXED(.65)+FRACUNIT/2);
		boost = FixedMul(FLOAT_TO_FIXED(.60),(FRACUNIT-FixedDiv(FRACUNIT,(dash+intermediate))));

		speedboost = max(speedboost, boost); // + diminished top speed
		accelboost = max(accelboost,3*FRACUNIT);   // + 300% acceleration
	}

	if (player->driftboost) // Drift Boost
	{
		speedboost = max(speedboost, FRACUNIT/4); // + 25%
		accelboost = max(accelboost, 4*FRACUNIT); // + 400%
	}

	if (player->ringboost) // Ring Boost
	{
		speedboost = max(speedboost, FRACUNIT/5); // + 20% top speed
		accelboost = max(accelboost, 4*FRACUNIT); // + 400% acceleration
	}

	if (player->startboost) // Startup Boost
	{
		speedboost = max(speedboost, FRACUNIT/4); // + 25%
		accelboost = max(accelboost, 6*FRACUNIT); // + 300%
	}

	// don't average them anymore, this would make a small boost and a high boost less useful
	// just take the highest we want instead

	player->boostpower = boostpower;

	// value smoothing
	if (speedboost > player->speedboost)
		player->speedboost = speedboost;
	else
		player->speedboost += (speedboost - player->speedboost)/(TICRATE/2);

	player->accelboost = accelboost;
}

// Returns kart speed from a stat. Boost power and scale are NOT taken into account, no player or object is necessary.
fixed_t K_GetKartSpeedFromStat(UINT8 kartspeed)
{
	const fixed_t xspd = 3072;
	fixed_t g_cc = FRACUNIT;
	fixed_t k_speed = 150;
	fixed_t finalspeed;

	switch (gamespeed)
	{
		case 0:
			g_cc = 53248 + xspd; //  50cc =  81.25 + 4.69 =  85.94%
			break;
		case 2:
			g_cc = 77824 + xspd; // 150cc = 118.75 + 4.69 = 123.44%
			break;
		default:
			g_cc = 65536 + xspd; // 100cc = 100.00 + 4.69 = 104.69%
			break;
	}

	k_speed += kartspeed*3; // 153 - 177

	finalspeed = FixedMul(k_speed<<14, g_cc);
	return finalspeed;
}

fixed_t K_GetKartSpeed(const player_t *player, boolean doboostpower, boolean notused)
{
	UINT8 kartspeed = player->kartspeed;
	fixed_t finalspeed = K_GetKartSpeedFromStat(kartspeed);

	if (doboostpower && !player->pogospring && !P_IsObjectOnGround(player->mo))
		return (75*mapobjectscale); // air speed cap

	if ((gametyperules & GTR_KARMA) && player->mo->health <= 0)
		kartspeed = 1;

	finalspeed = FixedMul(finalspeed, player->mo->scale);

	if (doboostpower)
		return FixedMul(finalspeed, player->boostpower+player->speedboost);
	return finalspeed;
}

fixed_t K_GetKartAccel(const player_t *player)
{
	fixed_t k_accel = 32; // 36;
	UINT8 kartspeed = player->kartspeed;

	if ((gametyperules & GTR_KARMA) && player->mo->health <= 0)
		kartspeed = 1;

	//k_accel += 3 * (9 - kartspeed); // 36 - 60
	k_accel += 4 * (9 - kartspeed); // 32 - 64

	return FixedMul(k_accel, FRACUNIT+player->accelboost);
}

UINT16 K_GetKartFlashing(const player_t *player)
{
	UINT16 tics = flashingtics;

	if (gametyperules & GTR_BUMPERS)
	{
		return 1;
	}

	if (player == NULL)
	{
		return tics;
	}

	tics += (tics/8) * (player->kartspeed);
	return tics;
}

boolean K_PlayerShrinkCheat(const player_t *player)
{
	return (
		(player->pflags & PF_SHRINKACTIVE)
		&& (player->bot == false)
		&& (modeattacking == ATTACKING_NONE) // Anyone want to make another record attack category?
	);
}

void K_UpdateShrinkCheat(player_t *player)
{
	const boolean mobjValid = (player->mo != NULL && P_MobjWasRemoved(player->mo) == false);

	if (player->pflags & PF_SHRINKME)
	{
		player->pflags |= PF_SHRINKACTIVE;
	}
	else
	{
		player->pflags &= ~PF_SHRINKACTIVE;
	}

	if (mobjValid == true && K_PlayerShrinkCheat(player) == true)
	{
		player->mo->destscale = FixedMul(mapobjectscale, SHRINK_SCALE);
	}
}

boolean K_KartKickstart(const player_t *player)
{
	return ((player->pflags & PF_KICKSTARTACCEL)
		&& (!K_PlayerUsesBotMovement(player))
		&& (player->kickstartaccel >= ACCEL_KICKSTART));
}

UINT16 K_GetKartButtons(const player_t *player)
{
	return (player->cmd.buttons |
		(K_KartKickstart(player) ? BT_ACCELERATE : 0));
}

SINT8 K_GetForwardMove(const player_t *player)
{
	SINT8 forwardmove = player->cmd.forwardmove;

	if ((player->pflags & PF_STASIS)
		|| (player->carry == CR_SLIDING)
		|| Obj_PlayerRingShooterFreeze(player) == true)
	{
		return 0;
	}

	if (player->sneakertimer
		|| (((gametyperules & (GTR_ROLLINGSTART|GTR_CIRCUIT)) == (GTR_ROLLINGSTART|GTR_CIRCUIT)) && (leveltime < TICRATE/2)))
	{
		return MAXPLMOVE;
	}

	if (player->spinouttimer != 0
		|| player->icecube.frozen)
	{
		return 0;
	}

	if (K_KartKickstart(player)) // unlike the brute forward of sneakers, allow for backwards easing here
	{
		forwardmove += MAXPLMOVE;
		if (forwardmove > MAXPLMOVE)
			forwardmove = MAXPLMOVE;
	}

	return forwardmove;
}

fixed_t K_3dKartMovement(player_t *player, boolean onground, SINT8 forwardmove)
{
	fixed_t accelmax = 4000;
	fixed_t newspeed, oldspeed, finalspeed;
	fixed_t p_speed = K_GetKartSpeed(player, true, true);
	fixed_t p_accel = K_GetKartAccel(player);

	if (!onground) return 0; // If the player isn't on the ground, there is no change in speed

	// ACCELCODE!!!1!11!
	oldspeed = R_PointToDist2(0, 0, player->rmomx, player->rmomy); // FixedMul(P_AproxDistance(player->rmomx, player->rmomy), player->mo->scale);
	newspeed = FixedDiv(FixedDiv(FixedMul(oldspeed, accelmax - p_accel) + FixedMul(p_speed, p_accel), accelmax), ORIG_FRICTION);

	if (player->pogospring) // Pogo Spring minimum/maximum thrust
	{
		const fixed_t hscale = mapobjectscale;
		const fixed_t minspeed = 24*hscale;
		const fixed_t maxspeed = 28*hscale;

		if (newspeed > maxspeed && player->pogospring == 2)
			newspeed = maxspeed;
		if (newspeed < minspeed)
			newspeed = minspeed;
	}

	finalspeed = newspeed - oldspeed;

	// forwardmove is:
	//  50 while accelerating,
	//  25 while clutching,
	//   0 with no gas, and
	// -25 when only braking.

	finalspeed *= forwardmove/25;
	finalspeed /= 2;

	if (forwardmove < 0 && finalspeed > mapobjectscale*2)
		return finalspeed/2;
	else if (forwardmove < 0)
		return -mapobjectscale/2;

	if (finalspeed < 0)
		finalspeed = 0;

	return finalspeed;
}

angle_t K_MomentumAngleEx(const mobj_t *mo, const fixed_t threshold)
{
	if (FixedHypot(mo->momx, mo->momy) > threshold)
	{
		return R_PointToAngle2(0, 0, mo->momx, mo->momy);
	}
	else
	{
		return mo->angle; // default to facing angle, rather than 0
	}
}

angle_t K_MomentumAngleReal(const mobj_t *mo)
{
	if (mo->momx || mo->momy)
	{
		return R_PointToAngle2(0, 0, mo->momx, mo->momy);
	}
	else
	{
		return mo->angle; // default to facing angle, rather than 0
	}
}

void K_AwardPlayerRings(player_t *player, UINT16 rings, boolean overload)
{
	UINT16 superring;

	if (!overload)
	{
		INT32 totalrings =
			RINGTOTAL(player) + (player->superring);

		/* capped at 20 rings */
		if ((totalrings + rings) > cv_ng_ringcap.value)
		{
			if (totalrings >= cv_ng_ringcap.value)
				return; // woah dont let that go negative buster
			rings = (cv_ng_ringcap.value - totalrings);
		}
	}

	superring = player->superring + rings;

	/* check if not overflow */
	if (superring > player->superring)
		player->superring = superring;
}

void K_DoInstashield(player_t *player)
{
	mobj_t *layera;
	mobj_t *layerb;

	if (player->instashield > 0)
		return;

	player->instashield = 15; // length of instashield animation
	S_StartSound(player->mo, sfx_cdpcm9);

	layera = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INSTASHIELDA);
	layera->old_x = player->mo->old_x;
	layera->old_y = player->mo->old_y;
	layera->old_z = player->mo->old_z;
	P_SetTarget(&layera->target, player->mo);

	layerb = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INSTASHIELDB);
	layerb->old_x = player->mo->old_x;
	layerb->old_y = player->mo->old_y;
	layerb->old_z = player->mo->old_z;
	P_SetTarget(&layerb->target, player->mo);
}

void K_DoPowerClash(mobj_t *t1, mobj_t *t2) {
	mobj_t *clash;
	UINT8 lag1 = 5;
	UINT8 lag2 = 5;

	// short-circuit instashield for vfx visibility
	if (t1->player)
	{
		t1->player->instashield = 1;
		t1->player->speedpunt += 20;
		lag1 -= min(lag1, t1->player->speedpunt/10);
	}

	if (t2->player)
	{
		t2->player->instashield = 1;
		t2->player->speedpunt += 20;
		lag2 -= min(lag1, t2->player->speedpunt/10);
	}

	S_StartSound(t1, sfx_parry);

	clash = P_SpawnMobj((t1->x/2) + (t2->x/2), (t1->y/2) + (t2->y/2), (t1->z/2) + (t2->z/2), MT_POWERCLASH);

	// needs to handle mixed scale collisions (t1 grow t2 invinc)...
	clash->z = clash->z + (t1->height/4) + (t2->height/4);
	clash->angle = R_PointToAngle2(clash->x, clash->y, t1->x, t1->y) + ANGLE_90;

	// Shrink over time (accidental behavior that looked good)
	clash->destscale = (t1->scale) + (t2->scale);
	P_SetScale(clash, 3*clash->destscale/2);
}

void K_BattleAwardHit(player_t *player, player_t *victim, mobj_t *inflictor, UINT8 damage)
{
	UINT8 points = 1;
	boolean trapItem = false;
	boolean finishOff = (victim->mo->health > 0) && (victim->mo->health <= damage);

	if (!(gametyperules & GTR_POINTLIMIT))
	{
		// No points in this gametype.
		return;
	}

	if (player == NULL || victim == NULL)
	{
		// Invalid player or victim
		return;
	}

	if (player == victim)
	{
		// You cannot give yourself points
		return;
	}

	if (player->exiting)
	{
		// The round has already ended, don't mess with points
		return;
	}

	if ((inflictor && !P_MobjWasRemoved(inflictor)) && (inflictor->type == MT_BANANA && inflictor->health > 1))
	{
		trapItem = true;
	}

	// Only apply score bonuses to non-bananas
	if (trapItem == false)
	{
		if (K_IsPlayerWanted(victim))
		{
			// +3 points for hitting a wanted player
			points = 3;
		}
		else if (gametyperules & GTR_BUMPERS)
		{
			if (finishOff)
			{
				// +2 points for finishing off a player
				points = 2;
			}
		}
	}

	// Check this before adding to player score
	if ((gametyperules & GTR_BUMPERS) && finishOff && g_pointlimit <= player->roundscore)
	{
		K_EndBattleRound(player);

		mobj_t *source = !P_MobjWasRemoved(inflictor) ? inflictor : player->mo;

		K_StartRoundWinCamera(
			victim->mo,
			R_PointToAngle2(source->x, source->y, victim->mo->x, victim->mo->y) + ANGLE_135,
			200*mapobjectscale,
			8*TICRATE,
			FRACUNIT/512
		);
	}

	K_GivePointsToPlayer(player, victim, points);
}

void K_SpinPlayer(player_t *player, mobj_t *inflictor, mobj_t *source, INT32 type)
{
	(void)inflictor;
	(void)source;

	K_DirectorFollowAttack(player, inflictor, source);

	player->spinouttype = type;

	if (( player->spinouttype & KSPIN_THRUST ))
	{
		// At spinout, player speed is increased to 1/4 their regular speed, moving them forward
		fixed_t spd = K_GetKartSpeed(player, true, true) / 4;

		if (player->speed < spd)
			P_InstaThrust(player->mo, player->mo->angle, FixedMul(spd, player->mo->scale));

		S_StartSound(player->mo, sfx_slip);
	}

	player->spinouttimer = (3*TICRATE/2)+2;
	player->pogospring = 0; //NOIRE: Reset pogo state as it did in Kart
	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
}

void K_RemoveGrowShrink(player_t *player)
{
	if (player->mo && !P_MobjWasRemoved(player->mo))
	{
		if (player->growshrinktimer > 0) // Play Shrink noise
			S_StartSound(player->mo, sfx_kc59);
		else if (player->growshrinktimer < 0) // Play Grow noise
			S_StartSound(player->mo, sfx_kc5a);

		K_KartResetPlayerColor(player);

		player->mo->scalespeed = mapobjectscale/TICRATE;
		player->mo->destscale = mapobjectscale;

		if (K_PlayerShrinkCheat(player) == true)
		{
			player->mo->destscale = FixedMul(player->mo->destscale, SHRINK_SCALE);
		}
	}

	player->growshrinktimer = 0;
	player->roundconditions.consecutive_grow_lasers = 0;
}

void K_SquishPlayer(player_t *player, mobj_t *inflictor, mobj_t *source)
{
	// PS: Inflictor is unused for all purposes here and is actually only ever relevant to Lua. It may be nil too.
	if (P_MobjWasRemoved(player->mo))
		return; // mobj was removed (in theory that shouldn't happen)


	player->squishedtimer = TICRATE;

	// Reduce Shrink timer
	if (player->growshrinktimer < 0)
	{
		player->growshrinktimer += TICRATE;
		if (player->growshrinktimer >= 0)
			K_RemoveGrowShrink(player);
	}

	player->flashing = K_GetKartFlashing(player);

	player->mo->flags |= MF_NOCLIP;

	if (player->mo->state != &states[S_KART_SPINOUT]) // Squash
		P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

	player->instashield = 15;
	if (cv_kartdebughuddrop.value && !modeattacking)
		K_DropItems(player);
	else
	{
		K_DropHnextList(player);
		K_PopPlayerShield(player);
	}
	return;
}

// Should this object actually scale check?
// Scale-to-scale comparisons only make sense for objects that expect to have dynamic scale.
static boolean K_IsScaledItem(mobj_t *mobj)
{
	return mobj && !P_MobjWasRemoved(mobj) &&
		(mobj->type == MT_ORBINAUT || mobj->type == MT_JAWZ || mobj->type == MT_JAWZ_DUD
		|| mobj->type == MT_BANANA || mobj->type == MT_EGGMANITEM || mobj->type == MT_BALLHOG
		|| mobj->type == MT_SSMINE || mobj->type == MT_LANDMINE || mobj->type == MT_SINK
		|| mobj->type == MT_PLAYER);
}


boolean K_IsBigger(mobj_t *compare, mobj_t *other)
{
	fixed_t compareScale, otherScale;
	const fixed_t requiredDifference = (mapobjectscale/4);

	if ((compare == NULL || P_MobjWasRemoved(compare) == true)
		|| (other == NULL || P_MobjWasRemoved(other) == true))
	{
		return false;
	}

	// If a player is colliding with a non-kartitem object, we don't care about what scale that object is:
	// mappers are liable to fuck with the scale for their own reasons, and we need to compare against the
	// player's base scale instead to match expectations.
	if (K_IsScaledItem(compare) != K_IsScaledItem(other))
	{
		if (compare->type == MT_PLAYER)
			return (compare->scale > requiredDifference + FixedMul(mapobjectscale, P_GetMobjDefaultScale(compare)));
		else if (other->type == MT_PLAYER)
			return false; // Haha what the fuck are you doing
		// fallthrough
	}

	if ((compareScale = P_GetMobjDefaultScale(compare)) != FRACUNIT)
	{
		compareScale = FixedDiv(compare->scale, compareScale);
	}
	else
	{
		compareScale = compare->scale;
	}

	if ((otherScale = P_GetMobjDefaultScale(other)) != FRACUNIT)
	{
		otherScale = FixedDiv(other->scale, otherScale);
	}
	else
	{
		otherScale = other->scale;
	}

	return (compareScale > otherScale + requiredDifference);
}

// Bumpers give you bonus launch height and speed, strengthening your DI to help evade combos.
// bumperinflate visuals are handled by MT_BATTLEBUMPER, but the effects are in K_KartPlayerThink.
void K_BumperInflate(player_t *player)
{
	if (!player || P_MobjWasRemoved(player->mo))
		return;

	if (!(gametyperules & GTR_BUMPERS))
		return;

	player->bumperinflate = 3;

	if (player->mo->health > 1)
		S_StartSound(player->mo, sfx_cdpcm9);
}

void K_ApplyTripWire(player_t *player, tripwirestate_t state)
{

	if (state == TRIPSTATE_PASSED)
	{
		S_StartSound(player->mo, sfx_cdfm63);

		if (player->roundconditions.tripwire_hyuu == false
			&& player->hyudorotimer > 0)
		{
			player->roundconditions.tripwire_hyuu = true;
			player->roundconditions.checkthisframe = true;
		}

		player->tripwireLeniency += TICRATE/2;
	}
	else if (state == TRIPSTATE_BLOCKED)
	{
		S_StartSound(player->mo, sfx_kc40);
		player->tripwireReboundDelay = 60;
		if (player->curshield == KSHIELD_BUBBLE)
			player->tripwireReboundDelay *= 2;
	}

	player->tripwireState = state;

	player->tripwireUnstuck += 10;
}

INT32 K_ExplodePlayer(player_t *player, mobj_t *inflictor, mobj_t *source) // A bit of a hack, we just throw the player up higher here and extend their spinout timer
{
	INT32 ringburst = 10;
	fixed_t spbMultiplier = FRACUNIT;

	(void)source;

	K_DirectorFollowAttack(player, inflictor, source);

	if (inflictor != NULL && P_MobjWasRemoved(inflictor) == false)
	{
		if (inflictor->type == MT_SPBEXPLOSION && inflictor->movefactor)
		{
			if (modeattacking & ATTACKING_SPB)
			{
				P_DamageMobj(player->mo, inflictor, source, 1, DMG_INSTAKILL);
				player->SPBdistance = 0;
				Music_StopAll();
			}

			spbMultiplier = inflictor->movefactor;


			if (spbMultiplier < FRACUNIT)
			{
				spbMultiplier = FRACUNIT;
			}
		}
	}

	player->mo->momz = 18 * mapobjectscale * P_MobjFlip(player->mo); // please stop forgetting mobjflip checks!!!!
	player->mo->momx = player->mo->momy = 0;

	player->spinouttype = KSPIN_EXPLOSION;
	player->spinouttimer = (3*TICRATE/2)+2;
	//NOIRE: Around here in original kart code it reset sneaker timers, drift, drift charge and pogo state, should we do that?
	if (spbMultiplier != FRACUNIT)
	{
		player->mo->momz = FixedMul(player->mo->momz, spbMultiplier);
		player->spinouttimer = FixedMul(player->spinouttimer, spbMultiplier + ((spbMultiplier - FRACUNIT) / 2));

		ringburst = FixedMul(ringburst * FRACUNIT, spbMultiplier) / FRACUNIT;
		if (ringburst > cv_ng_ringcap.value)
		{
			ringburst = cv_ng_ringcap.value;
		}
	}

	if (player->mo->eflags & MFE_UNDERWATER)
		player->mo->momz = (117 * player->mo->momz) / 200;

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

	return ringburst;
}

// This kind of wipeout happens with no rings -- doesn't remove a bumper, has no invulnerability, and is much shorter.
void K_DebtStingPlayer(player_t *player, mobj_t *source)
{
	INT32 length = TICRATE;

	if (source->player)
	{
		length += (4 * (source->player->kartweight - player->kartweight));
	}

	player->spinouttype = KSPIN_STUNG;
	player->spinouttimer = length;
	player->wipeoutslow = min(length-1, wipeoutslowtime+1);

	player->ringvisualwarning = TICRATE*2;
	player->stingfx = true;

	if (P_IsDisplayPlayer(player) && !player->exiting)
		S_StartSoundAtVolume(NULL, sfx_sting0, 200);

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
}

void K_GiveBumpersToPlayer(player_t *player, player_t *victim, UINT8 amount)
{
	const UINT8 oldPlayerBumpers = K_Bumpers(player);

	UINT8 tookBumpers = 0;

	while (tookBumpers < amount)
	{
		const UINT8 newbumper = (oldPlayerBumpers + tookBumpers);

		angle_t newangle, diff;
		fixed_t newx, newy;

		mobj_t *newmo;

		if (newbumper <= 1)
		{
			diff = 0;
		}
		else
		{
			diff = FixedAngle(360*FRACUNIT/newbumper);
		}

		newangle = player->mo->angle;
		newx = player->mo->x + P_ReturnThrustX(player->mo, newangle + ANGLE_180, 64*FRACUNIT);
		newy = player->mo->y + P_ReturnThrustY(player->mo, newangle + ANGLE_180, 64*FRACUNIT);

		newmo = P_SpawnMobj(newx, newy, player->mo->z, MT_BATTLEBUMPER);
		newmo->threshold = newbumper;

		if (victim)
		{
			P_SetTarget(&newmo->tracer, victim->mo);
		}

		P_SetTarget(&newmo->target, player->mo);

		newmo->angle = (diff * (newbumper-1));
		newmo->color = (victim ? victim : player)->skincolor;

		if (newbumper+1 < 2)
		{
			P_SetMobjState(newmo, S_BATTLEBUMPER3);
		}
		else if (newbumper+1 < 3)
		{
			P_SetMobjState(newmo, S_BATTLEBUMPER2);
		}
		else
		{
			P_SetMobjState(newmo, S_BATTLEBUMPER1);
		}

		tookBumpers++;
	}

	// :jartcookiedance:
	player->mo->health += tookBumpers;
}

void K_TakeBumpersFromPlayer(player_t *player, player_t *victim, UINT8 amount)
{
	amount = min(amount, K_Bumpers(victim));

	if (amount == 0)
	{
		return;
	}

	K_GiveBumpersToPlayer(player, victim, amount);

	// Play steal sound
	S_StartSound(player->mo, sfx_3db06);
}

void K_GivePointsToPlayer(player_t *player, player_t *victim, UINT8 amount)
{
	P_AddPlayerScore(player, amount);
	K_SpawnBattlePoints(player, victim, amount);
}

#define MINEQUAKEDIST 4096

// Does the proximity screen flash and quake for explosions
void K_MineFlashScreen(mobj_t *source)
{
	INT32 pnum;
	player_t *p;

	if (P_MobjWasRemoved(source))
	{
		return;
	}

	S_StartSound(source, sfx_s3k4e);
	P_StartQuakeFromMobj(18, 55 * source->scale, MINEQUAKEDIST * source->scale, source);

	// check for potential display players near the source so we can have a sick flashpal.
	for (pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		p = &players[pnum];

		if (!playeringame[pnum] || !P_IsDisplayPlayer(p))
		{
			continue;
		}

		if (R_PointToDist2(p->mo->x, p->mo->y, source->x, source->y) < source->scale * MINEQUAKEDIST)
		{
			if (!bombflashtimer && P_CheckSight(p->mo, source))
			{
				bombflashtimer = TICRATE*2;
				P_FlashPal(p, PAL_WHITE, 1);
			}
		}
	}
}

// Spawns the purely visual explosion
void K_SpawnMineExplosion(mobj_t *source, skincolornum_t color)
{
	INT32 i, radius, height;
	mobj_t *smoldering = P_SpawnMobj(source->x, source->y, source->z, MT_SMOLDERING);
	mobj_t *dust;
	mobj_t *truc;
	INT32 speed, speed2;

	K_MatchGenericExtraFlags(smoldering, source);
	smoldering->tics = TICRATE*3;
	radius = source->radius>>FRACBITS;
	height = source->height>>FRACBITS;

	if (!color)
		color = SKINCOLOR_KETCHUP;

	for (i = 0; i < 32; i++)
	{
		dust = P_SpawnMobj(source->x, source->y, source->z, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		dust->angle = (ANGLE_180/16) * i;
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		P_InstaThrust(dust, dust->angle, FixedMul(20*FRACUNIT, source->scale));
		dust->renderflags |= RF_DONTDRAW;

		truc = P_SpawnMobj(source->x + P_RandomRange(PR_EXPLOSION, -radius, radius)*FRACUNIT,
			source->y + P_RandomRange(PR_EXPLOSION, -radius, radius)*FRACUNIT,
			source->z + P_RandomRange(PR_EXPLOSION, 0, height)*FRACUNIT, MT_BOOMEXPLODE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*6;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(10*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(PR_EXPLOSION, -speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(PR_EXPLOSION, -speed, speed)*FRACUNIT;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(PR_EXPLOSION, -speed, speed)*FRACUNIT*P_MobjFlip(truc);
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->color = color;
		truc->renderflags |= RF_DONTDRAW;
	}

	for (i = 0; i < 16; i++)
	{
		dust = P_SpawnMobj(source->x + P_RandomRange(PR_EXPLOSION, -radius, radius)*FRACUNIT,
			source->y + P_RandomRange(PR_EXPLOSION, -radius, radius)*FRACUNIT,
			source->z + P_RandomRange(PR_EXPLOSION, 0, height)*FRACUNIT, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		dust->tics = 30;
		dust->momz = P_RandomRange(PR_EXPLOSION, FixedMul(3*FRACUNIT, source->scale)>>FRACBITS, FixedMul(7*FRACUNIT, source->scale)>>FRACBITS)*FRACUNIT;
		dust->renderflags |= RF_DONTDRAW;

		truc = P_SpawnMobj(source->x + P_RandomRange(PR_EXPLOSION, -radius, radius)*FRACUNIT,
			source->y + P_RandomRange(PR_EXPLOSION, -radius, radius)*FRACUNIT,
			source->z + P_RandomRange(PR_EXPLOSION, 0, height)*FRACUNIT, MT_BOOMPARTICLE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*5;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(PR_EXPLOSION, -speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(PR_EXPLOSION, -speed, speed)*FRACUNIT;
		speed = FixedMul(15*FRACUNIT, source->scale)>>FRACBITS;
		speed2 = FixedMul(45*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(PR_EXPLOSION, speed, speed2)*FRACUNIT*P_MobjFlip(truc);
		if (P_RandomChance(PR_EXPLOSION, FRACUNIT/2))
			truc->momz = -truc->momz;
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->tics = TICRATE*2;
		truc->color = color;
		truc->renderflags |= RF_DONTDRAW;
	}
}

#undef MINEQUAKEDIST

void K_SpawnLandMineExplosion(mobj_t *source, skincolornum_t color)
{
	mobj_t *smoldering;
	mobj_t *expl;
	UINT8 i;

	// Spawn smoke remains:
	smoldering = P_SpawnMobj(source->x, source->y, source->z, MT_SMOLDERING);
	P_SetScale(smoldering, source->scale);
	smoldering->tics = TICRATE*3;

	// spawn a few physics explosions
	for (i = 0; i < 15; i++)
	{
		expl = P_SpawnMobj(source->x, source->y, source->z + source->scale, MT_BOOMEXPLODE);
		expl->color = color;
		expl->tics = (i+1);
		expl->renderflags |= RF_DONTDRAW;

		//K_MatchGenericExtraFlags(expl, actor);
		P_SetScale(expl, source->scale*4);

		expl->momx = P_RandomRange(PR_EXPLOSION, -3, 3)*source->scale/2;
		expl->momy = P_RandomRange(PR_EXPLOSION, -3, 3)*source->scale/2;

		// 100/45 = 2.22 fu/t
		expl->momz = ((i+1)*source->scale*5/2)*P_MobjFlip(expl);
	}
}

fixed_t K_DefaultPlayerRadius(player_t *player)
{
	return FixedMul(player->mo->scale,
			player->mo->info->radius);
}

static mobj_t *K_SpawnKartMissile(mobj_t *source, mobjtype_t type, angle_t an, INT32 flags2, fixed_t speed)
{
	mobj_t *th;
	fixed_t x, y, z;
	fixed_t finalspeed = speed;
	mobj_t *throwmo;

	if (source->player && source->player->speed > K_GetKartSpeed(source->player, false, false))
	{
		angle_t input = source->angle - an;
		boolean invert = (input > ANGLE_180);
		if (invert)
			input = InvAngle(input);

		finalspeed = max(speed, FixedMul(speed, FixedMul(
			FixedDiv(source->player->speed, K_GetKartSpeed(source->player, false, false)), // Multiply speed to be proportional to your own, boosted maxspeed.
			(((180<<FRACBITS) - AngleFixed(input)) / 180) // multiply speed based on angle diff... i.e: don't do this for firing backward :V
			)));
	}

	x = source->x + source->momx + FixedMul(finalspeed, FINECOSINE(an>>ANGLETOFINESHIFT));
	y = source->y + source->momy + FixedMul(finalspeed, FINESINE(an>>ANGLETOFINESHIFT));
	z = source->z; // spawn on the ground please

	if (P_MobjFlip(source) < 0)
	{
		z = source->z+source->height - mobjinfo[type].height;
	}

	th = P_SpawnMobj(x, y, z, type);

	th->flags2 |= flags2;

	th->threshold = 10;

	if (th->info->seesound)
		S_StartSound(source, th->info->seesound);

	P_SetTarget(&th->target, source);

	if (P_IsObjectOnGround(source))
	{
		// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
		// This should set it for FOFs
		P_SetOrigin(th, th->x, th->y, th->z);
		// spawn on the ground if the player is on the ground
		if (P_MobjFlip(source) < 0)
		{
			th->z = th->ceilingz - th->height;
			th->eflags |= MFE_VERTICALFLIP;
		}
		else
			th->z = th->floorz;
	}

	th->angle = an;
	th->momx = FixedMul(finalspeed, FINECOSINE(an>>ANGLETOFINESHIFT));
	th->momy = FixedMul(finalspeed, FINESINE(an>>ANGLETOFINESHIFT));

	switch (type)
	{
		case MT_ORBINAUT:
			if (source && source->player)
				th->color = source->player->skincolor;
			else
				th->color = SKINCOLOR_GREY;
			th->movefactor = finalspeed;
			break;
		case MT_JAWZ:
			if (source && source->player)
			{
				INT32 lasttarg = source->player->lastjawztarget;
				th->cvmem = source->player->skincolor;
				if ((lasttarg >= 0 && lasttarg < MAXPLAYERS)
					&& playeringame[lasttarg]
					&& !players[lasttarg].spectator
					&& players[lasttarg].mo)
				{
					P_SetTarget(&th->tracer, players[lasttarg].mo);
				}
			}
			else
				th->cvmem = SKINCOLOR_KETCHUP;
			/* FALLTHRU */
		case MT_JAWZ_DUD:
			S_StartSound(th, th->info->activesound);
			/* FALLTHRU */
		case MT_SPB:
			th->movefactor = finalspeed;
			break;
		case MT_BUBBLESHIELDTRAP:
			P_SetScale(th, ((5*th->destscale)>>2)*4);
			th->destscale = (5*th->destscale)>>2;
			S_StartSound(th, sfx_s3kbfl);
			S_StartSound(th, sfx_cdfm35);
			break;
		default:
			break;
	}

	if (type != MT_BUBBLESHIELDTRAP)
	{
		x = x + P_ReturnThrustX(source, an, source->radius + th->radius);
		y = y + P_ReturnThrustY(source, an, source->radius + th->radius);
		throwmo = P_SpawnMobj(x, y, z, MT_FIREDITEM);
		throwmo->movecount = 1;
		throwmo->movedir = source->angle - an;
		P_SetTarget(&throwmo->target, source);
	}
	return NULL;
}

UINT16 K_DriftSparkColor(player_t *player, INT32 charge)
{
	INT32 ds = K_GetKartDriftSparkValue(player);
	UINT16 color = SKINCOLOR_NONE;

	if (charge < 0)
	{
		// Stage 0: GREY
		color = SKINCOLOR_GREY;
	}
	else if (charge >= ds*4)
	{
		// Stage 3: Rainbow
		if (charge <= (ds*4)+(32*3))
		{
			// transition
			color = SKINCOLOR_SILVER;
		}
		else
		{
			color = K_RainbowColor(leveltime);
		}
	}
	else if (charge >= ds*2)
	{
		// Stage 2: Red
		if (charge <= (ds*2)+(32*3))
		{
			// transition
			color = SKINCOLOR_TANGERINE;
		}
		else
		{
			color = SKINCOLOR_KETCHUP;
		}
	}
	else if (charge >= ds)
	{
		// Stage 1: Blue
		if (charge <= (ds)+(32*3))
		{
			// transition
			color = SKINCOLOR_PURPLE;
		}
		else
		{
			color = SKINCOLOR_SAPPHIRE;
		}
	}

	return color;
}

static void K_SpawnDriftSparks(player_t *player)
{
	INT32 ds = K_GetKartDriftSparkValue(player);
	fixed_t newx;
	fixed_t newy;
	mobj_t *spark;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (leveltime % 2 == 1)
		return;

	if (!player->drift
		|| (player->driftcharge < ds && !(player->driftcharge < 0)))
		return;

	travelangle = player->mo->angle-(ANGLE_45/5)*player->drift;

	for (i = 0; i < 2; i++)
	{
		SINT8 size = 1;
		UINT8 trail = 0;

		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(32*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(32*FRACUNIT, player->mo->scale));
		spark = P_SpawnMobj(newx, newy, player->mo->z, MT_DRIFTSPARK);

		P_SetTarget(&spark->target, player->mo);
		spark->angle = travelangle-(ANGLE_45/5)*player->drift;
		spark->destscale = player->mo->scale;
		P_SetScale(spark, player->mo->scale);

		spark->momx = player->mo->momx/2;
		spark->momy = player->mo->momy/2;
		//spark->momz = player->mo->momz/2;

		spark->color = K_DriftSparkColor(player, player->driftcharge);

		if (player->driftcharge < 0)
		{
			// Stage 0: Grey
			size = 0;
		}
		else if (player->driftcharge >= ds*4)
		{
			// Stage 3: Rainbow
			size = 2;
			trail = 2;

			if (player->driftcharge <= (ds*4)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
			else
			{
				spark->colorized = true;
			}
		}
		else if (player->driftcharge >= ds*2)
		{
			// Stage 2: Blue
			size = 2;
			trail = 1;

			if (player->driftcharge <= (ds*2)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
		}
		else
		{
			// Stage 1: Red
			size = 1;

			if (player->driftcharge <= (ds)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*2));
			}
		}

		if ((player->drift > 0 && player->cmd.driftturn > 0) // Inward drifts
			|| (player->drift < 0 && player->cmd.driftturn < 0))
		{
			if ((player->drift < 0 && (i & 1))
				|| (player->drift > 0 && !(i & 1)))
			{
				size++;
			}
			else if ((player->drift < 0 && !(i & 1))
				|| (player->drift > 0 && (i & 1)))
			{
				size--;
			}
		}
		else if ((player->drift > 0 && player->cmd.driftturn < 0) // Outward drifts
			|| (player->drift < 0 && player->cmd.driftturn > 0))
		{
			if ((player->drift < 0 && (i & 1))
				|| (player->drift > 0 && !(i & 1)))
			{
				size--;
			}
			else if ((player->drift < 0 && !(i & 1))
				|| (player->drift > 0 && (i & 1)))
			{
				size++;
			}
		}

		if (size == 2)
			P_SetMobjState(spark, S_DRIFTSPARK_A1);
		else if (size < 1)
			P_SetMobjState(spark, S_DRIFTSPARK_C1);
		else if (size > 2)
			P_SetMobjState(spark, S_DRIFTSPARK_D1);

		if (trail > 0)
			spark->tics += trail;

		K_MatchGenericExtraFlags(spark, player->mo);
	}
}

static void K_SpawnAIZDust(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	mobj_t *spark;
	angle_t travelangle;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (leveltime % 2 == 1)
		return;

	if (!P_IsObjectOnGround(player->mo))
		return;

	if (player->speed <= K_GetKartSpeed(player, false, true))
		return;

	travelangle = K_MomentumAngle(player->mo);
	//S_StartSound(player->mo, sfx_s3k47);

	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle - (player->aizdriftstrat*ANGLE_45), FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle - (player->aizdriftstrat*ANGLE_45), FixedMul(24*FRACUNIT, player->mo->scale));
		spark = P_SpawnMobj(newx, newy, player->mo->z, MT_AIZDRIFTSTRAT);

		spark->angle = travelangle+(player->aizdriftstrat*ANGLE_90);
		P_SetScale(spark, (spark->destscale = (3*player->mo->scale)>>2));

		spark->momx = (6*player->mo->momx)/5;
		spark->momy = (6*player->mo->momy)/5;
		spark->momz = P_GetMobjZMovement(player->mo);

		K_MatchGenericExtraFlags(spark, player->mo);
	}
}

void K_SpawnBoostTrail(player_t *player)
{
	fixed_t newx, newy, newz;
	fixed_t ground;
	mobj_t *flame;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (!P_IsObjectOnGround(player->mo)
		|| player->hyudorotimer != 0)
		return;

	if (player->mo->eflags & MFE_VERTICALFLIP)
		ground = player->mo->ceilingz;
	else
		ground = player->mo->floorz;

	if (player->drift != 0)
		travelangle = player->mo->angle;
	else
		travelangle = K_MomentumAngle(player->mo);

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		newz = P_GetZAt(player->mo->standingslope, newx, newy, ground);

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			newz -= FixedMul(mobjinfo[MT_SNEAKERTRAIL].height, player->mo->scale);
		}

		flame = P_SpawnMobj(newx, newy, ground, MT_SNEAKERTRAIL);

		P_SetTarget(&flame->target, player->mo);
		flame->angle = travelangle;
		flame->fuse = TICRATE*2;
		flame->destscale = player->mo->scale;
		P_SetScale(flame, player->mo->scale);
		// not K_MatchGenericExtraFlags so that a stolen sneaker can be seen
		K_FlipFromObject(flame, player->mo);

		flame->momx = 8;
		P_XYMovement(flame);
		if (P_MobjWasRemoved(flame))
			continue;

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			if (flame->z + flame->height < flame->ceilingz)
				P_RemoveMobj(flame);
		}
		else if (flame->z > flame->floorz)
			P_RemoveMobj(flame);
	}
}

void K_SpawnSparkleTrail(mobj_t *mo)
{
	const INT32 rad = (mo->radius*3)/FRACUNIT;
	mobj_t *sparkle;
	UINT8 invanimnum; // Current sparkle animation number
	INT32 invtime;// Invincibility time left, in seconds
	UINT8 index = 0;
	fixed_t newx, newy, newz;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (leveltime & 2)
		index = 1;

	invtime = mo->player->invincibilitytimer/TICRATE+1;

	//CONS_Printf("%d\n", index);

	newx = mo->x + (P_RandomRange(PR_DECORATION, -rad, rad)*FRACUNIT);
	newy = mo->y + (P_RandomRange(PR_DECORATION, -rad, rad)*FRACUNIT);
	newz = mo->z + (P_RandomRange(PR_DECORATION, 0, mo->height>>FRACBITS)*FRACUNIT);

	sparkle = P_SpawnMobj(newx, newy, newz, MT_SPARKLETRAIL);

	sparkle->angle = R_PointToAngle2(mo->x, mo->y, sparkle->x, sparkle->y);

	sparkle->movefactor = R_PointToDist2(mo->x, mo->y, sparkle->x, sparkle->y);	// Save the distance we spawned away from the player.
	//CONS_Printf("movefactor: %d\n", sparkle->movefactor/FRACUNIT);

	sparkle->extravalue1 = (sparkle->z - mo->z);			// Keep track of our Z position relative to the player's, I suppose.
	sparkle->extravalue2 = P_RandomRange(PR_DECORATION, 0, 1) ? 1 : -1;	// Rotation direction?
	sparkle->cvmem = P_RandomRange(PR_DECORATION, -25, 25)*mo->scale;		// Vertical "angle"

	K_FlipFromObject(sparkle, mo);
	P_SetTarget(&sparkle->target, mo);

	sparkle->destscale = mo->destscale;
	P_SetScale(sparkle, mo->scale);

	invanimnum = (invtime >= 11) ? 11 : invtime;
	//CONS_Printf("%d\n", invanimnum);

	P_SetMobjState(sparkle, K_SparkleTrailStartStates[invanimnum][index]);

	if (mo->player->invincibilitytimer > itemtime+(2*TICRATE))
	{
		sparkle->color = mo->color;
		sparkle->colorized = true;
	}
}

void K_SpawnWipeoutTrail(mobj_t *mo)
{
	mobj_t *dust;
	angle_t aoff;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (mo->player)
		aoff = (mo->player->drawangle + ANGLE_180);
	else
		aoff = (mo->angle + ANGLE_180);

	if ((leveltime / 2) & 1)
		aoff -= ANGLE_45;
	else
		aoff += ANGLE_45;

	dust = P_SpawnMobj(mo->x + FixedMul(24*mo->scale, FINECOSINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(PR_DECORATION,-8,8) << FRACBITS),
		mo->y + FixedMul(24*mo->scale, FINESINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(PR_DECORATION,-8,8) << FRACBITS),
		mo->z, MT_WIPEOUTTRAIL);

	P_SetTarget(&dust->target, mo);
	dust->angle = K_MomentumAngle(mo);
	dust->destscale = mo->scale;
	P_SetScale(dust, mo->scale);
	K_FlipFromObject(dust, mo);
}

//	K_DriftDustHandling
//	Parameters:
//		spawner: The map object that is spawning the drift dust
//	Description: Spawns the drift dust for objects, players use rmomx/y, other objects use regular momx/y.
//		Also plays the drift sound.
//		Other objects should be angled towards where they're trying to go so they don't randomly spawn dust
//		Do note that most of the function won't run in odd intervals of frames
void K_DriftDustHandling(mobj_t *spawner)
{
	angle_t anglediff;
	const INT16 spawnrange = spawner->radius >> FRACBITS;

	if (!P_IsObjectOnGround(spawner) || leveltime % 2 != 0)
		return;

	if (spawner->player)
	{
		if (spawner->player->pflags & PF_SKIDDOWN)
		{
			anglediff = abs((signed)(spawner->angle - spawner->player->drawangle));
			if (leveltime % 6 == 0)
				S_StartSound(spawner, sfx_screec); // repeated here because it doesn't always happen to be within the range when this is the case
		}
		else
		{
			angle_t playerangle = spawner->angle;

			if (spawner->player->speed < 5*spawner->scale)
				return;

			if (K_GetForwardMove(spawner->player) < 0)
				playerangle += ANGLE_180;

			anglediff = abs((signed)(playerangle - R_PointToAngle2(0, 0, spawner->player->rmomx, spawner->player->rmomy)));
		}
	}
	else
	{
		if (P_AproxDistance(spawner->momx, spawner->momy) < 5*spawner->scale)
			return;

		anglediff = abs((signed)(spawner->angle - K_MomentumAngle(spawner)));
	}

	if (anglediff > ANGLE_180)
		anglediff = InvAngle(anglediff);

	if (anglediff > ANG10*4) // Trying to turn further than 40 degrees
	{
		fixed_t spawnx = P_RandomRange(PR_DECORATION, -spawnrange, spawnrange) << FRACBITS;
		fixed_t spawny = P_RandomRange(PR_DECORATION, -spawnrange, spawnrange) << FRACBITS;
		INT32 speedrange = 2;
		mobj_t *dust = P_SpawnMobj(spawner->x + spawnx, spawner->y + spawny, spawner->z, MT_DRIFTDUST);
		dust->momx = FixedMul(spawner->momx + (P_RandomRange(PR_DECORATION, -speedrange, speedrange) * spawner->scale), 3*FRACUNIT/4);
		dust->momy = FixedMul(spawner->momy + (P_RandomRange(PR_DECORATION, -speedrange, speedrange) * spawner->scale), 3*FRACUNIT/4);
		dust->momz = P_MobjFlip(spawner) * (P_RandomRange(PR_DECORATION, 1, 4) * (spawner->scale));
		P_SetScale(dust, spawner->scale/2);
		dust->destscale = spawner->scale * 3;
		dust->scalespeed = spawner->scale/12;

		if (!spawner->player)
		{
			if (leveltime % 6 == 0)
				S_StartSound(spawner, sfx_screec);
		}

		K_MatchGenericExtraFlags(dust, spawner);

		// Sparkle-y warning for when you're about to change drift sparks!
		if (spawner->player && spawner->player->drift)
		{
			INT32 driftval = K_GetKartDriftSparkValue(spawner->player);
			INT32 warntime = driftval/3;
			INT32 dc = spawner->player->driftcharge;
			UINT8 c = SKINCOLOR_NONE;
			boolean rainbow = false;

			if (dc >= 0)
			{
				dc += warntime;
			}

			c = K_DriftSparkColor(spawner->player, dc);

			if (dc > (4*driftval)+(32*3))
			{
				rainbow = true;
			}

			if (c != SKINCOLOR_NONE)
			{
				P_SetMobjState(dust, S_DRIFTWARNSPARK1);
				dust->color = c;
				dust->colorized = rainbow;
			}
		}
	}
}

static mobj_t *K_FindLastTrailMobj(player_t *player)
{
	mobj_t *trail;

	if (!player || !(trail = player->mo) || !player->mo->hnext || !player->mo->hnext->health)
		return NULL;

	while (trail->hnext && !P_MobjWasRemoved(trail->hnext) && trail->hnext->health)
	{
		trail = trail->hnext;
	}

	return trail;
}

mobj_t *K_ThrowKartItem(player_t *player, boolean missile, mobjtype_t mapthing, INT32 defaultDir, INT32 altthrow)
{
	mobj_t *mo;
	INT32 dir;
	fixed_t PROJSPEED;
	angle_t newangle;
	fixed_t newx, newy, newz;
	mobj_t *throwmo;

	if (!player)
		return NULL;

	// Figure out projectile speed by game speed
	if (missile && mapthing != MT_BALLHOG) // Trying to keep compatability...
	{
		PROJSPEED = mobjinfo[mapthing].speed;
		if (gamespeed == 0)
			PROJSPEED = FixedMul(PROJSPEED, FRACUNIT-FRACUNIT/4);
		else if (gamespeed == 2)
			PROJSPEED = FixedMul(PROJSPEED, FRACUNIT+FRACUNIT/4);
		PROJSPEED = FixedMul(PROJSPEED, mapobjectscale);
	}
	else
	{
		switch (gamespeed)
		{
			case 0:
				PROJSPEED = 68*mapobjectscale; // Avg Speed is 34
				break;
			case 2:
				PROJSPEED = 96*mapobjectscale; // Avg Speed is 48
				break;
			default:
				PROJSPEED = 82*mapobjectscale; // Avg Speed is 41
				break;
		}
	}

	if (altthrow)
	{
		if (altthrow == 2) // Kitchen sink throwing
		{
			if (player->throwdir == 1)
				dir = 2;
			else
				dir = 1;
		}
		else
		{
			if (player->throwdir == 1)
				dir = 2;
			else if (player->throwdir == -1)
				dir = -1;
			else
				dir = 1;
		}
	}
	else
	{
		if (player->throwdir != 0)
			dir = player->throwdir;
		else
			dir = defaultDir;
	}

	if (missile) // Shootables
	{
		if (mapthing == MT_BALLHOG) // Messy
		{
			if (dir == -1)
			{
				// Shoot backward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180 - 0x06000000, 0, PROJSPEED/16);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180 - 0x03000000, 0, PROJSPEED/16);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/16);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180 + 0x03000000, 0, PROJSPEED/16);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180 + 0x06000000, 0, PROJSPEED/16);
			}
			else
			{
				// Shoot forward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x06000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x03000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + 0x03000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + 0x06000000, 0, PROJSPEED);
			}
		}
		else
		{
			if (dir == -1 && mapthing != MT_SPB)
			{
				// Shoot backward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/8);
			}
			else
			{
				// Shoot forward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle, 0, PROJSPEED);
			}
		}
	}
	else
	{
		player->bananadrag = 0; // RESET timer, for multiple bananas

		if (dir > 0)
		{
			// Shoot forward
			mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, mapthing);
			//K_FlipFromObject(mo, player->mo);
			// These are really weird so let's make it a very specific case to make SURE it works...
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				mo->z -= player->mo->height;
				mo->flags2 |= MF2_OBJECTFLIP;
				mo->eflags |= MFE_VERTICALFLIP;
			}

			mo->threshold = 10;
			P_SetTarget(&mo->target, player->mo);

			S_StartSound(player->mo, mo->info->seesound);

			if (mo)
			{
				angle_t fa = player->mo->angle>>ANGLETOFINESHIFT;
				fixed_t HEIGHT = (20 + (dir*10))*mapobjectscale + (player->mo->momz*P_MobjFlip(player->mo));

				mo->momz = HEIGHT*P_MobjFlip(mo);
				mo->momx = player->mo->momx + FixedMul(FINECOSINE(fa), PROJSPEED*dir);
				mo->momy = player->mo->momy + FixedMul(FINESINE(fa), PROJSPEED*dir);

				mo->extravalue2 = dir;

				if (mo->eflags & MFE_UNDERWATER)
					mo->momz = (117 * mo->momz) / 200;
			}

			// this is the small graphic effect that plops in you when you throw an item:
			throwmo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_FIREDITEM);
			P_SetTarget(&throwmo->target, player->mo);
			// Ditto:
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				throwmo->z -= player->mo->height;
				throwmo->flags2 |= MF2_OBJECTFLIP;
				throwmo->eflags |= MFE_VERTICALFLIP;
			}

			throwmo->movecount = 0; // above player
		}
		else
		{
			mobj_t *lasttrail = K_FindLastTrailMobj(player);

			if (lasttrail)
			{
				newx = lasttrail->x;
				newy = lasttrail->y;
				newz = lasttrail->z;
			}
			else
			{
				// Drop it directly behind you.
				fixed_t dropradius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(mobjinfo[mapthing].radius, mobjinfo[mapthing].radius);

				newangle = player->mo->angle;

				newx = player->mo->x + P_ReturnThrustX(player->mo, newangle + ANGLE_180, dropradius);
				newy = player->mo->y + P_ReturnThrustY(player->mo, newangle + ANGLE_180, dropradius);
				newz = player->mo->z;
			}

			mo = P_SpawnMobj(newx, newy, newz, mapthing); // this will never return null because collision isn't processed here
			K_FlipFromObject(mo, player->mo);

			mo->threshold = 10;
			P_SetTarget(&mo->target, player->mo);

			if (P_IsObjectOnGround(player->mo))
			{
				// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
				// This should set it for FOFs
				P_SetOrigin(mo, mo->x, mo->y, mo->z); // however, THIS can fuck up your day. just absolutely ruin you.
				if (P_MobjWasRemoved(mo))
					return NULL;

				if (P_MobjFlip(mo) > 0)
				{
					if (mo->floorz > mo->target->z - mo->height)
					{
						mo->z = mo->floorz;
					}
				}
				else
				{
					if (mo->ceilingz < mo->target->z + mo->target->height + mo->height)
					{
						mo->z = mo->ceilingz - mo->height;
					}
				}
			}

			if (player->mo->eflags & MFE_VERTICALFLIP)
				mo->eflags |= MFE_VERTICALFLIP;

			if (mapthing == MT_SSMINE)
				mo->extravalue1 = 49; // Pads the start-up length from 21 frames to a full 2 seconds
		}
	}

	return mo;
}

void K_PuntMine(mobj_t *origMine, mobj_t *punter)
{
	angle_t fa = K_MomentumAngle(punter);
	fixed_t z = (punter->momz * P_MobjFlip(punter)) + (30 * FRACUNIT);
	fixed_t spd;
	mobj_t *mine;

	if (!origMine || P_MobjWasRemoved(origMine))
		return;

	// This guarantees you hit a mine being dragged
	if (origMine->type == MT_SSMINE_SHIELD) // Create a new mine, and clean up the old one
	{
		mobj_t *mineOwner = origMine->target;

		mine = P_SpawnMobj(origMine->x, origMine->y, origMine->z, MT_SSMINE);

		P_SetTarget(&mine->target, mineOwner);

		mine->angle = origMine->angle;
		mine->flags2 = origMine->flags2;
		mine->floorz = origMine->floorz;
		mine->ceilingz = origMine->ceilingz;

		P_SetScale(mine, origMine->scale);
		mine->destscale = origMine->destscale;
		mine->scalespeed = origMine->scalespeed;

		// Copy interp data
		mine->old_angle = origMine->old_angle;
		mine->old_x = origMine->old_x;
		mine->old_y = origMine->old_y;
		mine->old_z = origMine->old_z;

		// Since we aren't using P_KillMobj, we need to clean up the hnext reference
		P_SetTarget(&mineOwner->hnext, NULL);
		K_UnsetItemOut(mineOwner->player);

		if (mineOwner->player->itemamount)
		{
			mineOwner->player->itemamount--;
		}

		if (!mineOwner->player->itemamount)
		{
			mineOwner->player->itemtype = KITEM_NONE;
		}

		P_RemoveMobj(origMine);
	}
	else
	{
		mine = origMine;
	}

	if (!mine || P_MobjWasRemoved(mine))
		return;

	if (mine->threshold > 0)
		return;

	spd = FixedMul(82 * punter->scale, K_GetKartGameSpeedScalar(gamespeed)); // Avg Speed is 41 in Normal

	mine->flags |= (MF_NOCLIP|MF_NOCLIPTHING);

	P_SetMobjState(mine, S_SSMINE_AIR1);
	mine->threshold = 10;
	mine->reactiontime = mine->info->reactiontime;

	mine->momx = punter->momx + FixedMul(FINECOSINE(fa >> ANGLETOFINESHIFT), spd);
	mine->momy = punter->momy + FixedMul(FINESINE(fa >> ANGLETOFINESHIFT), spd);
	P_SetObjectMomZ(mine, z, false);

	mine->flags &= ~(MF_NOCLIP|MF_NOCLIPTHING);
}

#define THUNDERRADIUS 320

// Rough size of the outer-rim sprites, after scaling.
// (The hitbox is already pretty strict due to only 1 active frame,
// we don't need to have it disjointedly small too...)
#define THUNDERSPRITE 80

static void K_DoLightningShield(player_t *player)
{
	mobj_t *mo;
	int i = 0;
	fixed_t sx;
	fixed_t sy;
	angle_t an;

	S_StartSound(player->mo, sfx_zio3);
	K_LightningShieldAttack(player->mo, (THUNDERRADIUS + THUNDERSPRITE) * FRACUNIT);

	// spawn vertical bolt
	mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
	P_SetTarget(&mo->target, player->mo);
	P_SetMobjState(mo, S_LZIO11);
	mo->color = SKINCOLOR_TEAL;
	mo->scale = player->mo->scale*3 + (player->mo->scale/2);

	mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
	P_SetTarget(&mo->target, player->mo);
	P_SetMobjState(mo, S_LZIO21);
	mo->color = SKINCOLOR_CYAN;
	mo->scale = player->mo->scale*3 + (player->mo->scale/2);

	// spawn horizontal bolts;
	for (i=0; i<7; i++)
	{
		mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
		mo->angle = P_RandomRange(PR_DECORATION, 0, 359)*ANG1;
		mo->fuse = P_RandomRange(PR_DECORATION, 20, 50);
		P_SetTarget(&mo->target, player->mo);
		P_SetMobjState(mo, S_KLIT1);
	}

	// spawn the radius thing:
	an = ANGLE_22h;
	for (i=0; i<15; i++)
	{
		sx = player->mo->x + FixedMul((player->mo->scale*THUNDERRADIUS), FINECOSINE((an*i)>>ANGLETOFINESHIFT));
		sy = player->mo->y + FixedMul((player->mo->scale*THUNDERRADIUS), FINESINE((an*i)>>ANGLETOFINESHIFT));
		mo = P_SpawnMobj(sx, sy, player->mo->z, MT_THOK);
		mo->angle = an*i;
		mo->extravalue1 = THUNDERRADIUS;	// Used to know whether we should teleport by radius or something.
		mo->scale = player->mo->scale*3;
		P_SetTarget(&mo->target, player->mo);
		P_SetMobjState(mo, S_KSPARK1);
	}
}

#undef THUNDERRADIUS
#undef THUNDERSPRITE

static void K_FlameDashLeftoverSmoke(mobj_t *src)
{
	UINT8 i;

	for (i = 0; i < 2; i++)
	{
		mobj_t *smoke = P_SpawnMobj(src->x, src->y, src->z+(8<<FRACBITS), MT_BOOSTSMOKE);

		P_SetScale(smoke, src->scale);
		smoke->destscale = 3*src->scale/2;
		smoke->scalespeed = src->scale/12;

		smoke->momx = 3*src->momx/4;
		smoke->momy = 3*src->momy/4;
		smoke->momz = 3*P_GetMobjZMovement(src)/4;

		P_Thrust(smoke, src->angle + FixedAngle(P_RandomRange(PR_DECORATION, 135, 225)<<FRACBITS), P_RandomRange(PR_DECORATION, 0, 8) * src->scale);
		smoke->momz += P_RandomRange(PR_DECORATION, 0, 4) * src->scale;
	}
}

static void K_DoHyudoroSteal(player_t *player)
{
	INT32 i, numplayers = 0;
	INT32 playerswappable[MAXPLAYERS];
	INT32 stealplayer = -1; // The player that's getting stolen from
	INT32 prandom = 0;
	boolean sink = P_RandomChance(PR_HYUDORO,FRACUNIT/64);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && players[i].mo->health > 0 && players[i].playerstate == PST_LIVE
			&& player != &players[i] && !players[i].exiting && !players[i].spectator // Player in-game

			// Can steal from this player
			&& ((gametyperules & GTR_CIRCUIT) //&& players[i].kartstuff[k_position] < player->kartstuff[k_position])
			|| ((gametyperules & GTR_BUMPERS) && players[i].mo->health > 0))

			// Has an item
			&& players[i].itemtype
			&& players[i].itemamount
			&& (!(players[i].itemflags & IF_ITEMOUT)))
			//&& !players[i].karthud[khud_itemblink]))
		{
			playerswappable[numplayers] = i;
			numplayers++;
		}
	}

	prandom = P_RandomFixed(PR_HYUDORO);
	S_StartSound(player->mo, sfx_s3k92);

	if (sink && numplayers > 0 && cv_items[KITEM_KITCHENSINK].value) // BEHOLD THE KITCHEN SINK
	{
		player->hyudorotimer = hyudorotime;
		player->stealingtimer = stealtime;

		player->itemtype = KITEM_KITCHENSINK;
		player->itemamount = 1;
		player->itemflags &= ~IF_ITEMOUT;
		return;
	}
	else if (((gametyperules & GTR_CIRCUIT) && player->position == 1) || numplayers == 0) // No-one can be stolen from? Oh well...
	{
		player->hyudorotimer = hyudorotime;
		player->stealingtimer = stealtime;
		return;
	}
	else if (numplayers == 1) // With just 2 players, we just need to set the other player to be the one to steal from
	{
		stealplayer = playerswappable[numplayers-1];
	}
	else if (numplayers > 1) // We need to choose between the available candidates for the 2nd player
	{
		stealplayer = playerswappable[prandom%(numplayers-1)];
	}

	if (stealplayer > -1) // Now here's where we do the stealing, has to be done here because we still know the player we're stealing from
	{
		player->hyudorotimer = hyudorotime;
		player->stealingtimer = stealtime;
		players[stealplayer].stolentimer = stealtime;

		player->itemtype = players[stealplayer].itemtype;
		player->itemamount = players[stealplayer].itemamount;
		player->itemflags &= ~IF_ITEMOUT;

		players[stealplayer].itemtype = KITEM_NONE;
		players[stealplayer].itemamount = 0;
		players[stealplayer].itemflags &= ~IF_ITEMOUT;

		if (P_IsPartyPlayer(&players[stealplayer]) && !splitscreen)
			S_StartSound(NULL, sfx_s3k92);
	}
}

void K_DoSneaker(player_t *player, INT32 type)
{
	const fixed_t intendedboost = FRACUNIT/2;

	if (player->roundconditions.touched_sneakerpanel == false
		&& !(player->exiting || (player->pflags & PF_NOCONTEST))
		&& player->floorboost != 0)
	{
		player->roundconditions.touched_sneakerpanel = true;
		player->roundconditions.checkthisframe = true;
	}

	if (player->floorboost == 0 || player->floorboost == 3)
	{
		const sfxenum_t normalsfx = sfx_cdfm01;
		const sfxenum_t smallsfx = sfx_cdfm40;
		sfxenum_t sfx = normalsfx;

		if (player->numsneakers)
		{
			// Use a less annoying sound when stacking sneakers.
			sfx = smallsfx;
		}

		S_StopSoundByID(player->mo, normalsfx);
		S_StopSoundByID(player->mo, smallsfx);
		S_StartSound(player->mo, sfx);

		K_SpawnDashDustRelease(player);
		if (intendedboost > player->speedboost)
			player->karthud[khud_destboostcam] = FixedMul(FRACUNIT, FixedDiv((intendedboost - player->speedboost), intendedboost));

		player->numsneakers++;
	}

	if (player->sneakertimer == 0)
	{
		if (type == 2)
		{
			if (player->mo->hnext)
			{
				mobj_t *cur = player->mo->hnext;
				while (cur && !P_MobjWasRemoved(cur))
				{
					if (!cur->tracer)
					{
						mobj_t *overlay = P_SpawnMobj(cur->x, cur->y, cur->z, MT_BOOSTFLAME);
						P_SetTarget(&overlay->target, cur);
						P_SetTarget(&cur->tracer, overlay);
						P_SetScale(overlay, (overlay->destscale = 3*cur->scale/4));
						K_FlipFromObject(overlay, cur);
					}
					cur = cur->hnext;
				}
			}
		}
		else
		{
			mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BOOSTFLAME);
			P_SetTarget(&overlay->target, player->mo);
			P_SetScale(overlay, (overlay->destscale = player->mo->scale));
			K_FlipFromObject(overlay, player->mo);
		}
	}

	player->sneakertimer = sneakertime;

	// set angle for spun out players:
	player->boostangle = player->mo->angle;
}

static void K_DoShrink(player_t *user)
{
	INT32 i;

	S_StartSound(user->mo, sfx_kc46); // Sound the BANG!

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;
		if (&players[i] == user)
			continue;
		if (players[i].position < user->position)
		{
			//P_FlashPal(&players[i], PAL_NUKE, 10);

			// Grow should get taken away.
			if (players[i].growshrinktimer > 0)
				K_RemoveGrowShrink(&players[i]);
			// Don't hit while invulnerable!
			else if (!players[i].invincibilitytimer
				&& players[i].growshrinktimer <= 0
				&& !players[i].hyudorotimer)
			{
				// Start shrinking!
				K_DropItems(&players[i]);
				players[i].growshrinktimer = -(15*TICRATE);

				if (players[i].mo && !P_MobjWasRemoved(players[i].mo))
				{
					players[i].mo->scalespeed = mapobjectscale/TICRATE;
					players[i].mo->destscale = (6*mapobjectscale)/8;
					if ((players[i].pflags & PF_SHRINKACTIVE) && !modeattacking && !players[i].bot)
						players[i].mo->destscale = (6*players[i].mo->destscale)/8;
					S_StartSound(players[i].mo, sfx_kc59);
				}
			}
		}
	}
}

void K_DoPogoSpring(mobj_t *mo, fixed_t vertispeed, UINT8 sound)
{
	const fixed_t vscale = mapobjectscale + (mo->scale - mapobjectscale);

	if (mo->player && mo->player->spectator)
		return;

	if (mo->eflags & MFE_SPRUNG)
		return;

	mo->standingslope = NULL;

	mo->eflags |= MFE_SPRUNG;

	if (mo->eflags & MFE_VERTICALFLIP)
		vertispeed *= -1;

	if (vertispeed == 0)
	{
		fixed_t thrust;

		if (mo->player)
		{
			thrust = 3*mo->player->speed/2;
			if (thrust < 48<<FRACBITS)
				thrust = 48<<FRACBITS;
			if (thrust > 72<<FRACBITS)
				thrust = 72<<FRACBITS;
			if (mo->player->pogospring != 2)
			{
				if (mo->player->sneakertimer)
					thrust = FixedMul(thrust, (5*FRACUNIT)/4);
				else if (mo->player->invincibilitytimer)
					thrust = FixedMul(thrust, (9*FRACUNIT)/8);
			}
		}
		else
		{
			thrust = FixedDiv(3*P_AproxDistance(mo->momx, mo->momy)/2, 5*FRACUNIT/2);
			if (thrust < 16<<FRACBITS)
				thrust = 16<<FRACBITS;
			if (thrust > 32<<FRACBITS)
				thrust = 32<<FRACBITS;
		}

		mo->momz = P_MobjFlip(mo)*FixedMul(FINESINE(ANGLE_22h>>ANGLETOFINESHIFT), FixedMul(thrust, vscale));
	}
	else
		mo->momz = FixedMul(vertispeed, vscale);

	if (mo->eflags & MFE_UNDERWATER)
		mo->momz = (117 * mo->momz) / 200;

	if (sound)
		S_StartSound(mo, (sound == 1 ? sfx_kc2f : sfx_kpogos));
}

static void K_ThrowLandMine(player_t *player)
{
	mobj_t *landMine;
	mobj_t *throwmo;

	landMine = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_LANDMINE);
	K_FlipFromObject(landMine, player->mo);
	landMine->threshold = 10;

	if (landMine->info->seesound)
		S_StartSound(player->mo, landMine->info->seesound);

	P_SetTarget(&landMine->target, player->mo);

	P_SetScale(landMine, player->mo->scale);
	landMine->destscale = player->mo->destscale;

	landMine->angle = player->mo->angle;

	landMine->momz = (30 * mapobjectscale * P_MobjFlip(player->mo)) + player->mo->momz;
	landMine->color = player->skincolor;

	throwmo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_FIREDITEM);
	P_SetTarget(&throwmo->target, player->mo);
	// Ditto:
	if (player->mo->eflags & MFE_VERTICALFLIP)
	{
		throwmo->z -= player->mo->height;
		throwmo->eflags |= MFE_VERTICALFLIP;
	}

	throwmo->movecount = 0; // above player
}

void K_DoInvincibility(player_t *player, tic_t time)
{
	if (!player->invincibilitytimer)
	{
		mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INVULNFLASH);
		P_SetTarget(&overlay->target, player->mo);
		overlay->destscale = player->mo->scale;
		P_SetScale(overlay, player->mo->scale);
	}

	if (P_IsPartyPlayer(player) == false)
	{
		S_StartSound(player->mo, sfx_alarmi);
	}

	player->invincibilitytimer = time;
}

void K_KillBananaChain(mobj_t *banana, mobj_t *inflictor, mobj_t *source)
{
	mobj_t *cachenext;

killnext:
	if (P_MobjWasRemoved(banana))
		return;

	cachenext = banana->hnext;

	if (banana->health)
	{
		if (banana->eflags & MFE_VERTICALFLIP)
			banana->z -= banana->height;
		else
			banana->z += banana->height;

		S_StartSound(banana, banana->info->deathsound);
		P_KillMobj(banana, inflictor, source, DMG_NORMAL);

		P_SetObjectMomZ(banana, 24*FRACUNIT, false);
		if (inflictor)
			P_InstaThrust(banana, R_PointToAngle2(inflictor->x, inflictor->y, banana->x, banana->y)+ANGLE_90, 16*FRACUNIT);
	}

	if ((banana = cachenext))
		goto killnext;
}

// Just for firing/dropping items.
void K_UpdateHnextList(player_t *player, boolean clean)
{
	mobj_t *work = player->mo, *nextwork;

	if (!work)
		return;

	nextwork = work->hnext;

	while ((work = nextwork) && !(work == NULL || P_MobjWasRemoved(work)))
	{
		nextwork = work->hnext;

		if (!clean && (!work->movedir || work->movedir <= (UINT16)player->itemamount))
		{
			continue;
		}

		P_RemoveMobj(work);
	}

	if (player->mo->hnext == NULL || P_MobjWasRemoved(player->mo->hnext))
	{
		// Like below, try to clean up the pointer if it's NULL.
		// Maybe this was a cause of the shrink/eggbox fails?
		P_SetTarget(&player->mo->hnext, NULL);
	}
}

// For getting hit!
void K_PopPlayerShield(player_t *player)
{
	INT32 shield = K_GetShieldFromItem(player->itemtype);

	// Doesn't apply if player is invalid.
	if (player->mo == NULL || P_MobjWasRemoved(player->mo))
	{
		return;
	}

	switch (shield)
	{
		case KSHIELD_NONE:
			// Doesn't apply to non-S3K shields.
			return;

		case KSHIELD_LIGHTNING:
			S_StartSound(player->mo, sfx_s3k7c);
			// K_DoLightningShield(player);
			break;
	}

	player->curshield = KSHIELD_NONE;
	player->itemtype = KITEM_NONE;
	player->itemamount = 0;
	K_UnsetItemOut(player);
}

// For getting hit!
void K_DropHnextList(player_t *player)
{
	mobj_t *work = player->mo, *nextwork, *dropwork;
	INT32 flip;
	mobjtype_t type;
	boolean orbit, ponground, dropall = true;

	if (!work || P_MobjWasRemoved(work))
		return;

	flip = P_MobjFlip(player->mo);
	ponground = P_IsObjectOnGround(player->mo);

	if (player->curshield != KSHIELD_NONE && player->itemamount)
	{
		if (player->curshield == KSHIELD_LIGHTNING)
			K_DoLightningShield(player);
		K_PopPlayerShield(player);
		player->itemamount = 0;
		player->itemtype = KITEM_NONE;
		player->curshield = 0;
	}

	nextwork = work->hnext;

	while ((work = nextwork) && !P_MobjWasRemoved(work))
	{
		nextwork = work->hnext;

		switch (work->type)
		{
			// Kart orbit items
			case MT_ORBINAUT_SHIELD:
				orbit = true;
				type = MT_ORBINAUT;
				break;
			case MT_JAWZ_SHIELD:
				orbit = true;
				type = MT_JAWZ_DUD;
				break;
			// Kart trailing items
			case MT_BANANA_SHIELD:
				orbit = false;
				type = MT_BANANA;
				break;
			case MT_SSMINE_SHIELD:
				orbit = false;
				dropall = false;
				type = MT_SSMINE;
				break;
			case MT_EGGMANITEM_SHIELD:
				orbit = false;
				type = MT_EGGMANITEM;
				break;
			// intentionally do nothing
			case MT_SINK_SHIELD:
			case MT_ROCKETSNEAKER:
				return;
			default:
				continue;
		}

		dropwork = P_SpawnMobj(work->x, work->y, work->z, type);
		P_SetTarget(&dropwork->target, player->mo);
		dropwork->angle = work->angle;
		dropwork->flags2 = work->flags2;
		dropwork->flags |= MF_NOCLIPTHING;
		dropwork->floorz = work->floorz;
		dropwork->ceilingz = work->ceilingz;

		if (ponground)
		{
			// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
			// This should set it for FOFs
			//P_SetOrigin(dropwork, dropwork->x, dropwork->y, dropwork->z); -- handled better by above floorz/ceilingz passing

			if (flip == 1)
			{
				if (dropwork->floorz > dropwork->target->z - dropwork->height)
				{
					dropwork->z = dropwork->floorz;
				}
			}
			else
			{
				if (dropwork->ceilingz < dropwork->target->z + dropwork->target->height + dropwork->height)
				{
					dropwork->z = dropwork->ceilingz - dropwork->height;
				}
			}
		}

		if (orbit) // splay out
		{
			dropwork->flags2 |= MF2_AMBUSH;
			dropwork->z += flip;
			dropwork->momx = player->mo->momx>>1;
			dropwork->momy = player->mo->momy>>1;
			dropwork->momz = 3*flip*mapobjectscale;
			if (dropwork->eflags & MFE_UNDERWATER)
				dropwork->momz = (117 * dropwork->momz) / 200;
			P_Thrust(dropwork, work->angle - ANGLE_90, 6*mapobjectscale);
			dropwork->movecount = 2;
			dropwork->movedir = work->angle - ANGLE_90;
			P_SetMobjState(dropwork, dropwork->info->deathstate);
			dropwork->tics = -1;
			if (type == MT_JAWZ_DUD)
				dropwork->z += 20*flip*dropwork->scale;
			else
			{
				dropwork->color = work->color;
				dropwork->angle -= ANGLE_90;
			}
		}
		else // plop on the ground
		{
			dropwork->flags &= ~MF_NOCLIPTHING;
			dropwork->threshold = 10;
		}

		P_RemoveMobj(work);
	}

	{
		// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
		P_SetTarget(&player->mo->hnext, NULL);
		player->bananadrag = 0;

		if (player->itemflags & IF_EGGMANOUT)
		{
			player->itemflags &= ~IF_EGGMANOUT;
		}
		else if ((player->itemflags & IF_ITEMOUT)
			&& (dropall || (--player->itemamount <= 0)))
		{
			player->itemamount = 0;
			K_UnsetItemOut(player);
			player->itemtype = KITEM_NONE;
		}
	}
}

SINT8 K_GetTotallyRandomResult(UINT8 useodds)
{
	INT32 spawnchance[NUMKARTRESULTS];
	INT32 totalspawnchance = 0;
	INT32 i;

	memset(spawnchance, 0, sizeof (spawnchance));

	for (i = 1; i < NUMKARTRESULTS; i++)
	{
		// Avoid calling K_FillItemRouletteData since that
		// function resets PR_ITEM_ROULETTE.
		spawnchance[i] = (
			totalspawnchance += K_KartGetItemOdds(NULL, NULL, useodds, i)
		);
	}

	if (totalspawnchance > 0)
	{
		totalspawnchance = P_RandomKey(PR_ITEM_ROULETTE, totalspawnchance);
		for (i = 0; i < NUMKARTRESULTS && spawnchance[i] <= totalspawnchance; i++);
	}
	else
	{
		i = KITEM_SAD;
	}

	return i;
}

mobj_t *K_CreatePaperItem(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT8 type, UINT16 amount)
{
	mobj_t *drop = P_SpawnMobj(x, y, z, MT_FLOATINGITEM);

	// FIXME: due to linkdraw sucking major ass, I was unable
	// to make a backdrop render behind dropped power-ups
	// (which use a smaller sprite than normal items). So
	// dropped power-ups have the backdrop baked into the
	// sprite for now.

	P_SetScale(drop, drop->scale>>4);
	drop->destscale = (3*drop->destscale)/2;

	drop->angle = angle;

	if (type == 0)
	{
		const SINT8 i = K_GetTotallyRandomResult(amount);

		// TODO: this is bad!
		// K_KartGetItemResult requires a player
		// but item roulette will need rewritten to change this

		const SINT8 newType = K_ItemResultToType(i);
		const UINT8 newAmount = K_ItemResultToAmount(i);

		if (newAmount > 1)
		{
			UINT8 j;

			for (j = 0; j < newAmount-1; j++)
			{
				K_CreatePaperItem(
					x, y, z,
					angle, flip,
					newType, 1
				);
			}
		}

		drop->threshold = newType;
		drop->movecount = 1;
	}
	else
	{
		drop->threshold = type;
		drop->movecount = amount;
	}

	if (gametyperules & GTR_CLOSERPLAYERS)
	{
		drop->fuse = BATTLE_DESPAWN_TIME;
	}

	return drop;
}

mobj_t *K_FlingPaperItem(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT8 type, UINT16 amount)
{
	mobj_t *drop = K_CreatePaperItem(x, y, z, angle, flip, type, amount);

	P_Thrust(drop,
		FixedAngle(P_RandomFixed(PR_ITEM_ROULETTE) * 180) + angle,
		16*mapobjectscale);

	drop->momz = flip * 3 * mapobjectscale;
	if (drop->eflags & MFE_UNDERWATER)
		drop->momz = (117 * drop->momz) / 200;

	return drop;
}

void K_DropPaperItem(player_t *player, UINT8 itemtype, UINT16 itemamount)
{
	if (!player->mo || P_MobjWasRemoved(player->mo))
	{
		return;
	}

	mobj_t *drop = K_FlingPaperItem(
		player->mo->x, player->mo->y, player->mo->z + player->mo->height/2,
		player->mo->angle + ANGLE_90, P_MobjFlip(player->mo),
		itemtype, itemamount
	);

	K_FlipFromObject(drop, player->mo);
}

// For getting EXTRA hit!
void K_DropItems(player_t *player)
{
	K_DropHnextList(player);

	if (player->itemamount > 0)
	{
		K_DropPaperItem(player, player->itemtype, player->itemamount);
	}

	K_StripItems(player);
}

void K_DropRocketSneaker(player_t *player)
{
	mobj_t *shoe = player->mo;
	fixed_t flingangle;
	boolean leftshoe = true; //left shoe is first

	if (!(player->mo && !P_MobjWasRemoved(player->mo) && player->mo->hnext && !P_MobjWasRemoved(player->mo->hnext)))
		return;

	while ((shoe = shoe->hnext) && !P_MobjWasRemoved(shoe))
	{
		if (shoe->type != MT_ROCKETSNEAKER)
			return; //woah, not a rocketsneaker, bail! safeguard in case this gets used when you're holding non-rocketsneakers

		shoe->renderflags &= ~RF_DONTDRAW;
		shoe->flags &= ~MF_NOGRAVITY;
		shoe->angle += ANGLE_45;

		if (shoe->eflags & MFE_VERTICALFLIP)
			shoe->z -= shoe->height;
		else
			shoe->z += shoe->height;

		//left shoe goes off tot eh left, right shoe off to the right
		if (leftshoe)
			flingangle = -(ANG60);
		else
			flingangle = ANG60;

		S_StartSound(shoe, shoe->info->deathsound);
		P_SetObjectMomZ(shoe, 24*FRACUNIT, false);
		P_InstaThrust(shoe, R_PointToAngle2(shoe->target->x, shoe->target->y, shoe->x, shoe->y)+flingangle, 16*FRACUNIT);
		shoe->momx += shoe->target->momx;
		shoe->momy += shoe->target->momy;
		shoe->momz += shoe->target->momz;
		shoe->extravalue2 = 1;

		leftshoe = false;
	}
	P_SetTarget(&player->mo->hnext, NULL);
	player->rocketsneakertimer = 0;
}

void K_DropKitchenSink(player_t *player)
{
	if (!(player->mo && !P_MobjWasRemoved(player->mo) && player->mo->hnext && !P_MobjWasRemoved(player->mo->hnext)))
		return;

	if (player->mo->hnext->type != MT_SINK_SHIELD)
		return; //so we can just call this function regardless of what is being held

	P_KillMobj(player->mo->hnext, NULL, NULL, DMG_NORMAL);

	P_SetTarget(&player->mo->hnext, NULL);
}

// When an item in the hnext chain dies.
void K_RepairOrbitChain(mobj_t *orbit)
{
	mobj_t *cachenext = orbit->hnext;

	// First, repair the chain
	if (orbit->hnext && orbit->hnext->health && !P_MobjWasRemoved(orbit->hnext))
	{
		P_SetTarget(&orbit->hnext->hprev, orbit->hprev);
		P_SetTarget(&orbit->hnext, NULL);
	}

	if (orbit->hprev && orbit->hprev->health && !P_MobjWasRemoved(orbit->hprev))
	{
		P_SetTarget(&orbit->hprev->hnext, cachenext);
		P_SetTarget(&orbit->hprev, NULL);
	}

	// Then recount to make sure item amount is correct
	if (orbit->target && orbit->target->player && !P_MobjWasRemoved(orbit->target))
	{
		INT32 num = 0;

		mobj_t *cur = orbit->target->hnext;
		mobj_t *prev = NULL;

		while (cur && !P_MobjWasRemoved(cur))
		{
			prev = cur;
			cur = cur->hnext;
			if (++num > orbit->target->player->itemamount)
				P_RemoveMobj(prev);
			else
				prev->movedir = num;
		}

		if (orbit->target && !P_MobjWasRemoved(orbit->target) && orbit->target->player->itemamount != num)
			orbit->target->player->itemamount = num;
	}
}

// Simplified version of a code bit in P_MobjFloorZ
static fixed_t K_BananaSlopeZ(pslope_t *slope, fixed_t x, fixed_t y, fixed_t z, fixed_t radius, boolean ceiling)
{
	fixed_t testx, testy;

	if (slope == NULL)
	{
		testx = x;
		testy = y;
	}
	else
	{
		if (slope->d.x < 0)
			testx = radius;
		else
			testx = -radius;

		if (slope->d.y < 0)
			testy = radius;
		else
			testy = -radius;

		if ((slope->zdelta > 0) ^ !!(ceiling))
		{
			testx = -testx;
			testy = -testy;
		}

		testx += x;
		testy += y;
	}

	return P_GetZAt(slope, testx, testy, z);
}

void K_CalculateBananaSlope(mobj_t *mobj, fixed_t x, fixed_t y, fixed_t z, fixed_t radius, fixed_t height, boolean flip, boolean player)
{
	fixed_t newz;
	sector_t *sec;
	pslope_t *slope = NULL;

	sec = R_PointInSubsector(x, y)->sector;

	if (flip)
	{
		slope = sec->c_slope;
		newz = K_BananaSlopeZ(slope, x, y, sec->ceilingheight, radius, true);
	}
	else
	{
		slope = sec->f_slope;
		newz = K_BananaSlopeZ(slope, x, y, sec->floorheight, radius, true);
	}

	// Check FOFs for a better suited slope
	if (sec->ffloors)
	{
		ffloor_t *rover;

		for (rover = sec->ffloors; rover; rover = rover->next)
		{
			fixed_t top, bottom;
			fixed_t d1, d2;

			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if ((!(((rover->fofflags & FOF_BLOCKPLAYER && player)
				|| (rover->fofflags & FOF_BLOCKOTHERS && !player))
				|| (rover->fofflags & FOF_QUICKSAND))
				|| (rover->fofflags & FOF_SWIMMABLE)))
				continue;

			top = K_BananaSlopeZ(*rover->t_slope, x, y, *rover->topheight, radius, false);
			bottom = K_BananaSlopeZ(*rover->b_slope, x, y, *rover->bottomheight, radius, true);

			if (flip)
			{
				if (rover->fofflags & FOF_QUICKSAND)
				{
					if (z < top && (z + height) > bottom)
					{
						if (newz > (z + height))
						{
							newz = (z + height);
							slope = NULL;
						}
					}
					continue;
				}

				d1 = (z + height) - (top + ((bottom - top)/2));
				d2 = z - (top + ((bottom - top)/2));

				if (bottom < newz && abs(d1) < abs(d2))
				{
					newz = bottom;
					slope = *rover->b_slope;
				}
			}
			else
			{
				if (rover->fofflags & FOF_QUICKSAND)
				{
					if (z < top && (z + height) > bottom)
					{
						if (newz < z)
						{
							newz = z;
							slope = NULL;
						}
					}
					continue;
				}

				d1 = z - (bottom + ((top - bottom)/2));
				d2 = (z + height) - (bottom + ((top - bottom)/2));

				if (top > newz && abs(d1) < abs(d2))
				{
					newz = top;
					slope = *rover->t_slope;
				}
			}
		}
	}

	//mobj->standingslope = slope;
	P_SetPitchRollFromSlope(mobj, slope);
}

// Move the hnext chain!
static void K_MoveHeldObjects(player_t *player)
{

	if (!player->mo)
		return;

	if (!player->mo->hnext)
	{
		player->bananadrag = 0;

		if (player->itemflags & IF_EGGMANOUT)
		{
			player->itemflags &= ~IF_EGGMANOUT;
		}
		else if (player->itemflags & IF_ITEMOUT)
		{
			player->itemamount = 0;
			K_UnsetItemOut(player);
			player->itemtype = KITEM_NONE;
		}
		return;
	}

	if (P_MobjWasRemoved(player->mo->hnext))
	{
		// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
		P_SetTarget(&player->mo->hnext, NULL);
		player->bananadrag = 0;

		if (player->itemflags & IF_EGGMANOUT)
		{
			player->itemflags &= ~IF_EGGMANOUT;
		}
		else if (player->itemflags & IF_ITEMOUT)
		{
			player->itemamount = 0;
			K_UnsetItemOut(player);
			player->itemtype = KITEM_NONE;
		}

		return;
	}

	switch (player->mo->hnext->type)
	{
		case MT_ORBINAUT_SHIELD: // Kart orbit items
		case MT_JAWZ_SHIELD:
			{
				Obj_OrbinautJawzMoveHeld(player);
				break;
			}
		case MT_BANANA_SHIELD: // Kart trailing items
		case MT_SSMINE_SHIELD:
		case MT_EGGMANITEM_SHIELD:
		case MT_SINK_SHIELD:
			{
				mobj_t *cur = player->mo->hnext;
				mobj_t *curnext;
				mobj_t *targ = player->mo;

				if (P_IsObjectOnGround(player->mo) && player->speed > 0)
					player->bananadrag++;

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(targ->radius, targ->radius) + FixedHypot(cur->radius, cur->radius);
					angle_t ang;
					fixed_t targx, targy, targz;
					fixed_t speed, dist;

					curnext = cur->hnext;

					if (cur->type == MT_EGGMANITEM_SHIELD)
					{

						// Decided that this should use their "canon" color.
						cur->color = SKINCOLOR_BLACK;
					}

					cur->flags &= ~MF_NOCLIPTHING;

					if ((player->mo->eflags & MFE_VERTICALFLIP) != (cur->eflags & MFE_VERTICALFLIP))
						K_FlipFromObject(cur, player->mo);

					if (!cur->health)
					{
						cur = curnext;
						continue;
					}

					if (cur->extravalue1 < radius)
						cur->extravalue1 += FixedMul(P_AproxDistance(cur->extravalue1, radius), FRACUNIT/12);
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					if (cur != player->mo->hnext)
					{
						targ = cur->hprev;
						dist = cur->extravalue1/4;
					}
					else
						dist = cur->extravalue1/2;

					if (!targ || P_MobjWasRemoved(targ))
					{
						cur = curnext;
						continue;
					}

					ang = targ->angle;
					targx = targ->x + P_ReturnThrustX(cur, ang + ANGLE_180, dist);
					targy = targ->y + P_ReturnThrustY(cur, ang + ANGLE_180, dist);
					targz = targ->z;

					speed = FixedMul(R_PointToDist2(cur->x, cur->y, targx, targy), 3*FRACUNIT/4);
					if (P_IsObjectOnGround(targ))
						targz = cur->floorz;

					cur->angle = R_PointToAngle2(cur->x, cur->y, targx, targy);

					/*
					if (P_IsObjectOnGround(player->mo) && player->speed > 0 && player->bananadrag > TICRATE
						&& P_RandomChance(PR_UNDEFINED, min(FRACUNIT/2, FixedDiv(player->speed, K_GetKartSpeed(player, false, false))/2)))
					{
						if (leveltime & 1)
							targz += 8*(2*FRACUNIT)/7;
						else
							targz -= 8*(2*FRACUNIT)/7;
					}
					*/

					if (speed > dist)
						P_InstaThrust(cur, cur->angle, speed-dist);

					P_SetObjectMomZ(cur, FixedMul(targz - cur->z, 7*FRACUNIT/8) - gravity, false);

					if (R_PointToDist2(cur->x, cur->y, targx, targy) > 768*FRACUNIT)
					{
						P_MoveOrigin(cur, targx, targy, cur->z);
						if (P_MobjWasRemoved(cur))
						{
							cur = curnext;
							continue;
						}
					}

					if (P_IsObjectOnGround(cur))
					{
						K_CalculateBananaSlope(cur, cur->x, cur->y, cur->z,
							cur->radius, cur->height, (cur->eflags & MFE_VERTICALFLIP), false);
					}

					cur = curnext;
				}
			}
			break;
		case MT_ROCKETSNEAKER: // Special rocket sneaker stuff
			{
				mobj_t *cur = player->mo->hnext;
				INT32 num = 0;

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius);
					boolean vibrate = ((leveltime & 1) && !cur->tracer);
					angle_t angoffset;
					fixed_t targx, targy, targz;

					cur->flags &= ~MF_NOCLIPTHING;

					if (player->rocketsneakertimer <= TICRATE && (leveltime & 1))
						cur->renderflags |= RF_DONTDRAW;
					else
						cur->renderflags &= ~RF_DONTDRAW;

					if (num & 1)
						P_SetMobjStateNF(cur, (vibrate ? S_ROCKETSNEAKER_LVIBRATE : S_ROCKETSNEAKER_L));
					else
						P_SetMobjStateNF(cur, (vibrate ? S_ROCKETSNEAKER_RVIBRATE : S_ROCKETSNEAKER_R));

					if (!player->rocketsneakertimer || cur->extravalue2 || !cur->health)
					{
						num = (num+1) % 2;
						cur = cur->hnext;
						continue;
					}

					if (cur->extravalue1 < radius)
						cur->extravalue1 += FixedMul(P_AproxDistance(cur->extravalue1, radius), FRACUNIT/12);
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

#if 1
					{
						angle_t input = player->drawangle - cur->angle;
						boolean invert = (input > ANGLE_180);
						if (invert)
							input = InvAngle(input);

						input = FixedAngle(AngleFixed(input)/4);
						if (invert)
							input = InvAngle(input);

						cur->angle = cur->angle + input;
					}
#else
					cur->angle = player->drawangle;
#endif

					angoffset = ANGLE_90 + (ANGLE_180 * num);

					targx = player->mo->x + P_ReturnThrustX(cur, cur->angle + angoffset, cur->extravalue1);
					targy = player->mo->y + P_ReturnThrustY(cur, cur->angle + angoffset, cur->extravalue1);

					{ // bobbing, copy pasted from my kimokawaiii entry
						fixed_t sine = FixedMul(player->mo->scale, 8 * FINESINE((((M_TAU_FIXED * (4*TICRATE)) * leveltime) >> ANGLETOFINESHIFT) & FINEMASK));
						targz = (player->mo->z + (player->mo->height/2)) + sine;
						if (player->mo->eflags & MFE_VERTICALFLIP)
							targz += (player->mo->height/2 - 32*player->mo->scale)*6;
					}

					if (cur->tracer && !P_MobjWasRemoved(cur->tracer))
					{
						fixed_t diffx, diffy, diffz;

						diffx = targx - cur->x;
						diffy = targy - cur->y;
						diffz = targz - cur->z;

						P_MoveOrigin(cur->tracer, cur->tracer->x + diffx + P_ReturnThrustX(cur, cur->angle + angoffset, 6*cur->scale),
							cur->tracer->y + diffy + P_ReturnThrustY(cur, cur->angle + angoffset, 6*cur->scale), cur->tracer->z + diffz);
						P_SetScale(cur->tracer, (cur->tracer->destscale = 3*cur->scale/4));
					}

					P_MoveOrigin(cur, targx, targy, targz);
					K_FlipFromObject(cur, player->mo);	// Update graviflip in real time thanks.

					cur->roll = player->mo->roll;
					cur->pitch = player->mo->pitch;

					num = (num+1) % 2;
					cur = cur->hnext;
				}
			}
			break;
		default:
			break;
	}
}

// Engine Sounds.
static void K_UpdateEngineSounds(player_t *player)
{
	const INT32 numsnds = 13;

	const fixed_t closedist = 160*FRACUNIT;
	const fixed_t fardist = 1536*FRACUNIT;

	const UINT8 dampenval = 48; // 255 * 48 = close enough to FRACUNIT/6

	const UINT16 buttons = K_GetKartButtons(player);

	INT32 class; // engine class number

	UINT8 volume = 255;
	fixed_t volumedampen = FRACUNIT;

	INT32 targetsnd = 0;
	INT32 i;

	if (leveltime < 8 || player->spectator || gamestate != GS_LEVEL || player->exiting)
	{
		// Silence the engines, and reset sound number while we're at it.
		player->karthud[khud_enginesnd] = 0;
		return;
	}

	class = R_GetEngineClass(player->kartspeed, player->kartweight, 0); // there are no unique sounds for ENGINECLASS_J

#if 0
	if ((leveltime % 8) != ((player-players) % 8)) // Per-player offset, to make engines sound distinct!
#else
	if (leveltime % 8)
#endif
	{
		// .25 seconds of wait time between each engine sound playback
		return;
	}

	if (player->respawn.state == RESPAWNST_DROP) // Dropdashing
	{
		// Dropdashing
		targetsnd = ((buttons & BT_ACCELERATE) ? 12 : 0);
	}
	else
	{
		// Average out the value of forwardmove and the speed that you're moving at.
		targetsnd = (((6 * K_GetForwardMove(player)) / 25) + ((player->speed / mapobjectscale) / 5)) / 2;
	}

	if (targetsnd < 0) { targetsnd = 0; }
	if (targetsnd > 12) { targetsnd = 12; }

	if (player->karthud[khud_enginesnd] < targetsnd) { player->karthud[khud_enginesnd]++; }
	if (player->karthud[khud_enginesnd] > targetsnd) { player->karthud[khud_enginesnd]--; }

	if (player->karthud[khud_enginesnd] < 0) { player->karthud[khud_enginesnd] = 0; }
	if (player->karthud[khud_enginesnd] > 12) { player->karthud[khud_enginesnd] = 12; }

	// This code calculates how many players (and thus, how many engine sounds) are within ear shot,
	// and rebalances the volume of your engine sound based on how far away they are.

	// This results in multiple things:
	// - When on your own, you will hear your own engine sound extremely clearly.
	// - When you were alone but someone is gaining on you, yours will go quiet, and you can hear theirs more clearly.
	// - When around tons of people, engine sounds will try to rebalance to not be as obnoxious.

	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT8 thisvol = 0;
		fixed_t dist;

		if (!playeringame[i] || !players[i].mo)
		{
			// This player doesn't exist.
			continue;
		}

		if (players[i].spectator)
		{
			// This player isn't playing an engine sound.
			continue;
		}

		if (P_IsDisplayPlayer(&players[i]))
		{
			// Don't dampen yourself!
			continue;
		}

		dist = P_AproxDistance(
			P_AproxDistance(
				player->mo->x - players[i].mo->x,
				player->mo->y - players[i].mo->y),
				player->mo->z - players[i].mo->z) / 2;

		dist = FixedDiv(dist, mapobjectscale);

		if (dist > fardist)
		{
			// ENEMY OUT OF RANGE !
			continue;
		}
		else if (dist < closedist)
		{
			// engine sounds' approx. range
			thisvol = 255;
		}
		else
		{
			thisvol = (15 * ((closedist - dist) / FRACUNIT)) / ((fardist - closedist) >> (FRACBITS+4));
		}

		volumedampen += (thisvol * dampenval);
	}

	if (volumedampen > FRACUNIT)
	{
		volume = FixedDiv(volume * FRACUNIT, volumedampen) / FRACUNIT;
	}

	if (volume <= 0)
	{
		// Don't need to play the sound at all.
		return;
	}

	S_StartSoundAtVolume(player->mo, (sfx_krta00 + player->karthud[khud_enginesnd]) + (class * numsnds), volume);
}

static void K_UpdateInvincibilitySounds(player_t *player)
{
	INT32 sfxnum = sfx_None;

	if (player->mo->health > 0 && !P_IsPartyPlayer(player)) // used to be !P_IsDisplayPlayer(player)
	{
		if (player->invincibilitytimer > 0) // Prioritize invincibility
			sfxnum = sfx_alarmi;
		else if (player->growshrinktimer > 0)
			sfxnum = sfx_alarmg;
	}

	if (sfxnum != sfx_None && !S_SoundPlaying(player->mo, sfxnum))
		S_StartSound(player->mo, sfxnum);

#define STOPTHIS(this) \
	if (sfxnum != this && S_SoundPlaying(player->mo, this)) \
		S_StopSoundByID(player->mo, this);
	STOPTHIS(sfx_alarmi);
	STOPTHIS(sfx_alarmg);
#undef STOPTHIS
}

// This function is not strictly for non-netsynced properties.
void K_KartPlayerHUDUpdate(player_t *player)
{
	if (K_PlayerTallyActive(player) == true)
	{
		K_TickPlayerTally(player);
	}

	if (player->karthud[khud_lapanimation])
		player->karthud[khud_lapanimation]--;

	if (player->karthud[khud_yougotem])
		player->karthud[khud_yougotem]--;

	if (player->karthud[khud_voices])
		player->karthud[khud_voices]--;

	if (player->karthud[khud_tauntvoices])
		player->karthud[khud_tauntvoices]--;

	if (player->karthud[khud_taunthorns])
		player->karthud[khud_taunthorns]--;

	if (player->positiondelay)
		player->positiondelay--;

	if (player->karthud[khud_itemblink] && player->karthud[khud_itemblink]-- <= 0)
	{
		player->karthud[khud_itemblinkmode] = 0;
		player->karthud[khud_itemblink] = 0;
	}

	if (player->karthud[khud_rouletteoffset] != 0)
	{
		if (abs(player->karthud[khud_rouletteoffset]) < (FRACUNIT >> 1))
		{
			// Snap to 0, since the change is getting very small.
			player->karthud[khud_rouletteoffset] = 0;
		}
		else
		{
			// Lerp to 0.
			player->karthud[khud_rouletteoffset] = FixedMul(player->karthud[khud_rouletteoffset], FRACUNIT*3/4);
		}
	}

	if (!(gametyperules & GTR_SPHERES))
	{
		if (!player->exiting
		&& !(player->pflags & (PF_NOCONTEST|PF_ELIMINATED)))
		{
			player->hudrings = player->rings;
		}


		// 0 is the fast spin animation, set at 30 tics of ring boost or higher!
		if (player->ringboost >= 30)
			player->karthud[khud_ringdelay] = 0;
		else
			player->karthud[khud_ringdelay] = ((RINGANIM_DELAYMAX+1) * (30 - player->ringboost)) / 30;

		if (player->karthud[khud_ringframe] == 0 && player->karthud[khud_ringdelay] > RINGANIM_DELAYMAX)
		{
			player->karthud[khud_ringframe] = 0;
			player->karthud[khud_ringtics] = 0;
		}
		else if ((player->karthud[khud_ringtics]--) <= 0)
		{
			if (player->karthud[khud_ringdelay] == 0) // fast spin animation
			{
				player->karthud[khud_ringframe] = ((player->karthud[khud_ringframe]+2) % RINGANIM_NUMFRAMES);
				player->karthud[khud_ringtics] = 0;
			}
			else
			{
				player->karthud[khud_ringframe] = ((player->karthud[khud_ringframe]+1) % RINGANIM_NUMFRAMES);
				player->karthud[khud_ringtics] = min(RINGANIM_DELAYMAX, player->karthud[khud_ringdelay])-1;
			}
		}

		if (player->pflags & PF_RINGLOCK)
		{
			UINT8 normalanim = (leveltime % 14);
			UINT8 debtanim = 14 + (leveltime % 2);

			if (player->karthud[khud_ringspblock] >= 14) // debt animation
			{
				if ((player->hudrings > 0) // Get out of 0 ring animation
					&& (normalanim == 3 || normalanim == 10)) // on these transition frames.
					player->karthud[khud_ringspblock] = normalanim;
				else
					player->karthud[khud_ringspblock] = debtanim;
			}
			else // normal animation
			{
				if ((player->hudrings <= 0) // Go into 0 ring animation
					&& (player->karthud[khud_ringspblock] == 1 || player->karthud[khud_ringspblock] == 8)) // on these transition frames.
					player->karthud[khud_ringspblock] = debtanim;
				else
					player->karthud[khud_ringspblock] = normalanim;
			}
		}
		else
			player->karthud[khud_ringspblock] = (leveltime % 14); // reset to normal anim next time
	}

	if (player->exiting && !(player->pflags & PF_NOCONTEST))
	{
		if (player->karthud[khud_finish] <= 2*TICRATE)
			player->karthud[khud_finish]++;
	}
	else
		player->karthud[khud_finish] = 0;

}

#undef RINGANIM_DELAYMAX

// SRB2Kart: blockmap iterate for attraction shield users
static mobj_t *attractmo;
static fixed_t attractdist;
static fixed_t attractzdist;

static inline BlockItReturn_t PIT_AttractingRings(mobj_t *thing)
{
	if (attractmo == NULL || P_MobjWasRemoved(attractmo) || attractmo->player == NULL)
	{
		return BMIT_ABORT;
	}

	if (thing == NULL || P_MobjWasRemoved(thing))
	{
		return BMIT_CONTINUE; // invalid
	}

	if (thing == attractmo)
	{
		return BMIT_CONTINUE; // invalid
	}

	if (!(thing->type == MT_RING || thing->type == MT_FLINGRING || thing->type == MT_EMERALD))
	{
		return BMIT_CONTINUE; // not a ring
	}

	if (thing->health <= 0)
	{
		return BMIT_CONTINUE; // dead
	}

	if (thing->extravalue1 && thing->type != MT_EMERALD)
	{
		return BMIT_CONTINUE; // in special ring animation
	}

	if (thing->tracer != NULL && P_MobjWasRemoved(thing->tracer) == false)
	{
		return BMIT_CONTINUE; // already attracted
	}

	// see if it went over / under
	if (attractmo->z - attractzdist > thing->z + thing->height)
	{
		return BMIT_CONTINUE; // overhead
	}

	if (attractmo->z + attractmo->height + attractzdist < thing->z)
	{
		return BMIT_CONTINUE; // underneath
	}

	if (P_AproxDistance(attractmo->x - thing->x, attractmo->y - thing->y) > attractdist + thing->radius)
	{
		return BMIT_CONTINUE; // Too far away
	}

	if (RINGTOTAL(attractmo->player) >= cv_ng_ringcap.value || (attractmo->player->pflags & PF_RINGLOCK))
	{
		// Already reached max -- just joustle rings around.

		// Regular ring -> fling ring
		if (thing->info->reactiontime && thing->type != (mobjtype_t)thing->info->reactiontime)
		{
			thing->type = thing->info->reactiontime;
			thing->info = &mobjinfo[thing->type];
			thing->flags = thing->info->flags;

			P_InstaThrust(thing, P_RandomRange(PR_ITEM_RINGS, 0, 7) * ANGLE_45, 2 * thing->scale);
			P_SetObjectMomZ(thing, 8<<FRACBITS, false);
			thing->fuse = 120*TICRATE;

			thing->cusval = 0; // Reset attraction flag
		}
	}
	else
	{
		// set target
		P_SetTarget(&thing->tracer, attractmo);
	}

	return BMIT_CONTINUE; // find other rings
}

/** Looks for rings near a player in the blockmap.
  *
  * \param pmo Player object looking for rings to attract
  * \sa A_AttractChase
  */
static void K_LookForRings(mobj_t *pmo)
{
	INT32 bx, by, xl, xh, yl, yh;

	attractmo = pmo;
	attractdist = (400 * pmo->scale);
	attractzdist = attractdist >> 2;

	// Use blockmap to check for nearby rings
	yh = (unsigned)(pmo->y + (attractdist + MAXRADIUS) - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (unsigned)(pmo->y - (attractdist + MAXRADIUS) - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (unsigned)(pmo->x + (attractdist + MAXRADIUS) - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (unsigned)(pmo->x - (attractdist + MAXRADIUS) - bmaporgx)>>MAPBLOCKSHIFT;

	BMBOUNDFIX(xl, xh, yl, yh);

	for (by = yl; by <= yh; by++)
		for (bx = xl; bx <= xh; bx++)
			P_BlockThingsIterator(bx, by, PIT_AttractingRings);
}

static void K_UpdateTripwire(player_t *player)
{
	fixed_t speedThreshold = (3*K_GetKartSpeed(player, false, true))/4;
	boolean goodSpeed = (player->speed >= speedThreshold);
	boolean boostExists = (player->tripwireLeniency > 0); // can't be checked later because of subtractions...
	tripwirepass_t triplevel = K_TripwirePassConditions(player);

	if (triplevel != TRIPWIRE_NONE)
	{
		if (!boostExists)
		{
			mobj_t *front = P_SpawnMobjFromMobj(player->mo, 0, 0, 0, MT_TRIPWIREBOOST);
			mobj_t *back = P_SpawnMobjFromMobj(player->mo, 0, 0, 0, MT_TRIPWIREBOOST);

			P_SetTarget(&front->target, player->mo);
			P_SetTarget(&back->target, player->mo);

			P_SetTarget(&front->punt_ref, player->mo);
			P_SetTarget(&back->punt_ref, player->mo);

			front->dispoffset = 1;
			front->old_angle = back->old_angle = K_MomentumAngle(player->mo);
			back->dispoffset = -1;
			back->extravalue1 = 1;
			P_SetMobjState(back, S_TRIPWIREBOOST_BOTTOM);
		}

		player->tripwirePass = triplevel;

		if (triplevel != TRIPWIRE_CONSUME)
			player->tripwireLeniency = max(player->tripwireLeniency, TRIPWIRETIME);
	}

	// TRIPWIRE_CONSUME is only applied in very specific cases (currently, riding Garden Top)
	// and doesn't need leniency; however, it should track leniency from other pass conditions,
	// so that stripping Garden Top feels consistent.
	if (triplevel == TRIPWIRE_NONE || triplevel == TRIPWIRE_CONSUME)
	{
		if (boostExists)
		{
			player->tripwireLeniency--;
			if (goodSpeed == false && player->tripwireLeniency > 0)
			{
				// Decrease at double speed when your speed is bad.
				player->tripwireLeniency--;
			}
		}

		if (player->tripwireLeniency <= 0 && triplevel == TRIPWIRE_NONE)
		{
			player->tripwirePass = TRIPWIRE_NONE;
		}
	}
}

/**	\brief	Decreases various kart timers and powers per frame. Called in P_PlayerThink in p_user.c

	\param	player	player object passed from P_PlayerThink
	\param	cmd		control input from player

	\return	void
*/
void K_KartPlayerThink(player_t *player, ticcmd_t *cmd)
{
	const boolean onground = (P_IsObjectOnGround(player->mo));

	/* reset sprite offsets :) */
	player->mo->sprxoff = 0;
	player->mo->spryoff = 0;
	player->mo->sprzoff = 0;
	player->mo->spritexoffset = 0;
	player->mo->spriteyoffset = 0;

	player->cameraOffset = 0;

	player->pflags &= ~(PF_CASTSHADOW);

	if (player->loop.radius)
	{
		// Offset sprite Z position so wheels touch top of
		// hitbox when rotated 180 degrees.
		// TODO: this should be generalized for pitch/roll
		angle_t pitch = FixedAngle(player->loop.revolution * 360) / 2;
		player->mo->sprzoff += FixedMul(player->mo->height, FSIN(pitch));
	}

	K_UpdateOffroad(player);
	K_UpdateEngineSounds(player); // Thanks, VAda!

	Obj_DashRingPlayerThink(player);

	if (!cv_ng_ringdebt.value)
	{
			if (player->rings < 0)
				player->rings = 0;
	}

	// update boost angle if not spun out
	if (!player->spinouttimer && !player->wipeoutslow)
		player->boostangle = player->mo->angle;

	K_GetKartBoostPower(player);

	// Special effect objects!
	if (player->mo && !player->spectator)
	{
		if (player->dashpadcooldown) // Twinkle Circuit afterimages
		{
			mobj_t *ghost;
			ghost = P_SpawnGhostMobj(player->mo);
			ghost->fuse = player->dashpadcooldown+1;
			ghost->momx = player->mo->momx / (player->dashpadcooldown+1);
			ghost->momy = player->mo->momy / (player->dashpadcooldown+1);
			ghost->momz = player->mo->momz / (player->dashpadcooldown+1);
			player->dashpadcooldown--;
		}

		if (player->speed > 0)
		{
			// Speed lines
			if (player->sneakertimer || player->driftboost
			|| player->startboost || player->gateBoost
			|| player->ringboost)
			{
				K_SpawnNormalSpeedLines(player);
			}
		}

		// Race: spawn ring debt indicator
		// Battle: spawn zero-bumpers indicator
		if ((gametyperules & GTR_SPHERES) ? player->mo->health <= 1 : player->rings <= 0
			&&  (cv_ng_ringsting.value
			|| ((!cv_ng_ringsting.value) && cv_ng_ringdebt.value && (player->rings < 0))))
		{
			UINT8 doubler;

			// GROSS. In order to have a transparent version of this for a splitscreen local player, we actually need to spawn two!
			for (doubler = 0; doubler < 2; doubler++)
			{
				mobj_t *debtflag = P_SpawnMobj(player->mo->x + player->mo->momx, player->mo->y + player->mo->momy,
					player->mo->z + P_GetMobjZMovement(player->mo) + player->mo->height + (24*player->mo->scale), MT_THOK);

				debtflag->old_x = player->mo->old_x;
				debtflag->old_y = player->mo->old_y;
				debtflag->old_z = player->mo->old_z + P_GetMobjZMovement(player->mo) + player->mo->height + (24*player->mo->scale);

				P_SetMobjState(debtflag, S_RINGDEBT);
				P_SetScale(debtflag, (debtflag->destscale = player->mo->scale));

				K_MatchGenericExtraFlags(debtflag, player->mo);
				debtflag->frame += (leveltime % 4);

				if ((leveltime/12) & 1)
					debtflag->frame += 4;

				debtflag->color = player->skincolor;
				debtflag->fuse = 2;

				if (doubler == 0) // Real copy. Draw for everyone but us.
				{
					debtflag->renderflags |= K_GetPlayerDontDrawFlag(player);
				}
				else if (doubler == 1) // Fake copy. Draw for only us, and go transparent after a bit.
				{
					debtflag->renderflags |= (RF_DONTDRAW & ~K_GetPlayerDontDrawFlag(player));
					if (player->ringvisualwarning <= 1 || gametyperules & GTR_SPHERES)
						debtflag->renderflags |= RF_TRANS50;
				}
			}

		}

		if (player->springstars && (leveltime & 1))
		{
			fixed_t randx = P_RandomRange(PR_DECORATION, -40, 40) * player->mo->scale;
			fixed_t randy = P_RandomRange(PR_DECORATION, -40, 40) * player->mo->scale;
			fixed_t randz = P_RandomRange(PR_DECORATION, 0, player->mo->height >> FRACBITS) << FRACBITS;
			mobj_t *star = P_SpawnMobj(
				player->mo->x + randx,
				player->mo->y + randy,
				player->mo->z + randz,
				MT_KARMAFIREWORK);

			star->color = player->springcolor;
			star->flags |= MF_NOGRAVITY;
			star->momx = player->mo->momx / 2;
			star->momy = player->mo->momy / 2;
			star->momz = P_GetMobjZMovement(player->mo) / 2;
			star->fuse = 12;
			star->scale = player->mo->scale;
			star->destscale = star->scale / 2;

			player->springstars--;
		}
	}

	if (player->itemtype == KITEM_NONE)
		player->itemflags &= ~IF_HOLDREADY;

	// DKR style camera for boosting
	if (player->karthud[khud_boostcam] != 0 || player->karthud[khud_destboostcam] != 0)
	{
		if (player->karthud[khud_boostcam] < player->karthud[khud_destboostcam]
			&& player->karthud[khud_destboostcam] != 0)
		{
			player->karthud[khud_boostcam] += FRACUNIT/(TICRATE/4);
			if (player->karthud[khud_boostcam] >= player->karthud[khud_destboostcam])
				player->karthud[khud_destboostcam] = 0;
		}
		else
		{
			player->karthud[khud_boostcam] -= FRACUNIT/TICRATE;
			if (player->karthud[khud_boostcam] < player->karthud[khud_destboostcam])
				player->karthud[khud_boostcam] = player->karthud[khud_destboostcam] = 0;
		}
		//CONS_Printf("cam: %d, dest: %d\n", player->karthud[khud_boostcam], player->karthud[khud_destboostcam]);
	}

	if (onground)
	{
		if (player->karthud[khud_aircam] > 0)
		{
			player->karthud[khud_aircam] -= FRACUNIT / 5;

			if (player->karthud[khud_aircam] < 0)
			{
				player->karthud[khud_aircam] = 0;
			}

			//CONS_Printf("cam: %f\n", FixedToFloat(player->karthud[khud_aircam]));
		}
	}
	else
	{
		if (player->karthud[khud_aircam] < FRACUNIT)
		{
			player->karthud[khud_aircam] += FRACUNIT / TICRATE;

			if (player->karthud[khud_aircam] > FRACUNIT)
			{
				player->karthud[khud_aircam] = FRACUNIT;
			}

			//CONS_Printf("cam: %f\n", FixedToFloat(player->karthud[khud_aircam]));
		}
	}

	// Make ABSOLUTELY SURE that your flashing tics don't get set WHILE you're still in hit animations.
	if (player->spinouttimer != 0 || player->squishedtimer != 0)
	{
		if (( player->spinouttype & KSPIN_IFRAMES ) == 0)
			player->flashing = 0;
		else if (player->squishedtimer > 0)
			player->flashing = 0;
		else
			player->flashing = K_GetKartFlashing(player);
	}



	if (player->spinouttimer)
	{
		if (((P_IsObjectOnGround(player->mo)
			|| ( player->spinouttype & KSPIN_AIRTIMER ))
			&& (!player->sneakertimer))
		|| (player->respawn.state != RESPAWNST_NONE
			&& player->spinouttimer > 1
			&& (leveltime & 1)))
		{
			player->spinouttimer--;
			if (player->wipeoutslow > 1)
				player->wipeoutslow--;
		}
	}
	else
	{
		if (player->wipeoutslow >= 1)
			player->mo->friction = ORIG_FRICTION;
		player->wipeoutslow = 0;
	}

	if (player->rings > cv_ng_ringcap.value)
		player->rings = cv_ng_ringcap.value;
	else if (player->rings < -cv_ng_ringcap.value)
		player->rings = -cv_ng_ringcap.value;

	if (player->spheres > 40)
		player->spheres = 40;
	// where's the < 0 check? see below the following block!

	{
		tic_t spheredigestion = TICRATE*2; // Base rate of 1 every 2 seconds when playing.
		tic_t digestionpower = ((10 - player->kartspeed) + (10 - player->kartweight))-1; // 1 to 17

		// currently 0-34
		digestionpower = ((player->spheres*digestionpower)/20);

		if (digestionpower >= spheredigestion)
		{
			spheredigestion = 1;
		}
		else
		{
			spheredigestion -= digestionpower/2;
		}

		if ((player->spheres > 0) && (player->spheredigestion > 0))
		{
			// If you got a massive boost in spheres, catch up digestion as necessary.
			if (spheredigestion < player->spheredigestion)
			{
				player->spheredigestion = (spheredigestion + player->spheredigestion)/2;
			}

			player->spheredigestion--;

			if (player->spheredigestion == 0)
			{
				if (player->spheres > 5)
					player->spheres--;
				player->spheredigestion = spheredigestion;
			}

		}
		else
		{
			player->spheredigestion = spheredigestion;
		}
	}

	// where's the > 40 check? see above the previous block!
	if (player->spheres < 0)
		player->spheres = 0;

	if (!(gametyperules & GTR_KARMA) || (player->pflags & PF_ELIMINATED))
	{
		player->karmadelay = 0;
	}
	else if (player->karmadelay > 0 && !P_PlayerInPain(player))
	{
		player->karmadelay--;
		if (P_IsDisplayPlayer(player) && player->karmadelay <= 0)
			comebackshowninfo = true; // client has already seen the message
	}

	if (player->tripwireReboundDelay)
		player->tripwireReboundDelay--;

	if (player->ringdelay)
		player->ringdelay--;

	if (player->checkskip && numbosswaypoints> 0)
		player->checkskip--;

	if (P_PlayerInPain(player))
	{
		player->ringboost = 0;
	}
	else if (player->ringboost)
	{
		// If a Ring Box or Super Ring isn't paying out, aggressively reduce
		// extreme ringboost duration. Less aggressive for accel types, so they
		// retain more speed for small payouts.

		UINT8 roller = TICRATE*2;
		roller += 4*(8-player->kartspeed);

		if (player->superring == 0)
			player->ringboost -= max((player->ringboost / roller), 1);
		else
			player->ringboost--;
	}

	if (player->sneakertimer)
	{
		player->sneakertimer--;

		if (player->sneakertimer <= 0)
		{
			player->numsneakers = 0;
		}
	}

	if (player->flamedash)
	{
		player->flamedash--;

		if (player->flamedash == 0)
			S_StopSoundByID(player->mo, sfx_fshld1);
		else if (player->flamedash == 3 && player->curshield == KSHIELD_FLAME) // "Why 3?" We can't blend sounds so this is the best shit I've got
			S_StartSoundAtVolume(player->mo, sfx_fshld3, 255/3);
	}

	if (player->counterdash)
		player->counterdash--;

	if (player->sneakertimer && player->wipeoutslow > 0 && player->wipeoutslow < wipeoutslowtime+1)
		player->wipeoutslow = wipeoutslowtime+1;

	if (player->floorboost > 0)
		player->floorboost--;

	if (player->driftboost)
		player->driftboost--;

	if (player->strongdriftboost)
		player->strongdriftboost--;

	if (player->gateBoost)
		player->gateBoost--;

	if (player->startboost > 0)
		player->startboost--;

	if (player->speedpunt)
		player->speedpunt--;

	// This timer can get out of control fast, clamp to match player expectations about "new" hazards
	if (player->speedpunt > TICRATE*4)
		player->speedpunt = TICRATE*4;

	if (player->invincibilitytimer)
		player->invincibilitytimer--;

	if (!player->invincibilitytimer)
		player->invincibilityextensions = 0;

	if (player->preventfailsafe)
		player->preventfailsafe--;

	if (player->tripwireUnstuck > 150)
	{
		player->tripwireUnstuck = 0;
		K_DoIngameRespawn(player);
	}

	// Don't tick down while in damage state.
	// There may be some maps where the timer activates for
	// a moment during normal play, but would quickly correct
	// itself when the player drives forward.
	// If the player is in a damage state, they may not be
	// able to move in time.
	// Always let the respawn prompt appear.
	if (player->bigwaypointgap && (player->bigwaypointgap > AUTORESPAWN_THRESHOLD || !P_PlayerInPain(player)))
	{
		player->bigwaypointgap--;
		if (!player->bigwaypointgap)
			K_DoIngameRespawn(player);
		else if (player->bigwaypointgap == AUTORESPAWN_THRESHOLD)
			K_AddMessageForPlayer(player, "Press \xAE to respawn", true, false);
	}

	if (player->tripwireUnstuck)
		player->tripwireUnstuck--;

	if ((player->respawn.state == RESPAWNST_NONE) && player->growshrinktimer != 0)
	{
		if (player->growshrinktimer > 0)
			player->growshrinktimer--;
		if (player->growshrinktimer < 0)
			player->growshrinktimer++;

		// Back to normal
		if (player->growshrinktimer == 0)
			K_RemoveGrowShrink(player);
	}

	if (player->respawn.state != RESPAWNST_MOVE && (player->cmd.buttons & BT_RESPAWN) == BT_RESPAWN)
	{
		player->finalfailsafe++; // Decremented by ringshooter to "freeze" this timer
		// Part-way through the auto-respawn timer, you can tap Ring Shooter to respawn early
		if (player->finalfailsafe >= 4*TICRATE ||
			(player->bigwaypointgap && player->bigwaypointgap < AUTORESPAWN_THRESHOLD))
		{
			K_DoIngameRespawn(player);
			player->finalfailsafe = 0;
		}
	}
	else
	{
		player->finalfailsafe = 0;
	}

	if (player->freeRingShooterCooldown)
		player->freeRingShooterCooldown--;

	if (player->superring)
	{
		player->nextringaward++;
		UINT8 ringrate = 3 - min(2, player->superring / 20); // Used to consume fat stacks of cash faster.
		if (player->nextringaward >= ringrate)
		{
			mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
			ring->extravalue1 = 1; // Ring collect animation timer
			ring->angle = player->mo->angle; // animation angle
			P_SetTarget(&ring->target, player->mo); // toucher for thinker
			player->pickuprings++;
			if (player->superring == 1)
				ring->cvmem = 1; // play caching when collected
			player->nextringaward = 0;
			player->superring--;
		}
	}
	else
	{
		player->nextringaward = 99; // Next time we need to award superring, spawn the first one instantly.
	}

	if (player->pflags & PF_VOID) // Returning from FAULT VOID
	{
		player->pflags &= ~PF_VOID;
		player->mo->renderflags &= ~RF_DONTDRAW;
		player->mo->flags &= ~MF_NOCLIPTHING;
		player->mo->momx = 0;
		player->mo->momy = 0;
		player->mo->momz = 0;
		player->nocontrol = 0;
		player->driftboost = 0;
		player->strongdriftboost = 0;
		player->sneakertimer = 0;
		player->flashing = TICRATE/2;
		player->ringboost = 0;
		player->driftboost = player->strongdriftboost = 0;
		player->gateBoost = 0;
	}

	// Start at lap 1 on binary maps just to be safe.
	if ((!udmf) && player->laps == 0 && numlaps > 0)
		player->laps = 1;

	player->topAccel = min(player->topAccel + TOPACCELREGEN, MAXTOPACCEL);

	if (player->stealingtimer == 0
		&& player->rocketsneakertimer
		&& onground == true)
		player->rocketsneakertimer--;

	if (player->hyudorotimer)
		player->hyudorotimer--;

	K_AdjustPlayerFriction(player, onground);

	if (player->bumpUnstuck > 30*5)
	{
		player->bumpUnstuck = 0;
		K_DoIngameRespawn(player);
	}
	else if (player->bumpUnstuck)
	{
		player->bumpUnstuck--;
	}

	if (player->fakeBoost)
		player->fakeBoost--;

	if (player->bumperinflate)
	{
		fixed_t thrustdelta = MAXCOMBOTHRUST - MINCOMBOTHRUST;
		fixed_t floatdelta = MAXCOMBOFLOAT - MINCOMBOFLOAT;

		fixed_t thrustpertic = thrustdelta / MAXCOMBOTIME;
		fixed_t floatpertic = floatdelta / MAXCOMBOTIME;

		fixed_t totalthrust = thrustpertic * player->progressivethrust + MINCOMBOTHRUST;

		if (player->speed > K_GetKartSpeed(player, false, false))
			totalthrust = 0;

		P_Thrust(player->mo, K_MomentumAngle(player->mo), totalthrust);

		player->bumperinflate--;
	}

	if (player->ringvolume < MINRINGVOLUME)
		player->ringvolume = MINRINGVOLUME;
	else if (MAXRINGVOLUME - player->ringvolume < RINGVOLUMEREGEN)
		player->ringvolume = MAXRINGVOLUME;
	else
		player->ringvolume += RINGVOLUMEREGEN;

	// :D
	if (player->ringtransparency < MINRINGTRANSPARENCY)
		player->ringtransparency = MINRINGTRANSPARENCY;
	else if (MAXRINGTRANSPARENCY - player->ringtransparency < RINGTRANSPARENCYREGEN)
		player->ringtransparency = MAXRINGTRANSPARENCY;
	else
		player->ringtransparency += RINGTRANSPARENCYREGEN;

	if (player->sadtimer)
		player->sadtimer--;

	if (player->stealingtimer > 0)
		player->stealingtimer--;
	else if (player->stealingtimer < 0)
		player->stealingtimer++;

	if (player->squishedtimer > 0)
		player->squishedtimer--;

	if (player->justbumped > 0)
		player->justbumped--;

	UINT16 normalturn = abs(cmd->driftturn);
	UINT16 normalaim = abs(cmd->throwdir);

	if (normalturn != 0 || normalaim != 0)
	{
		if (normalturn != KART_FULLTURN && normalturn != KART_FULLTURN/2 && normalturn != 0)
			player->analoginput = true;
		if (normalaim != KART_FULLTURN && normalaim != KART_FULLTURN/2 && normalaim != 0)
			player->analoginput = true;
		if (normalturn == KART_FULLTURN/2 && normalaim == KART_FULLTURN)
			player->analoginput = false;
	}

	if (player->stingfx)
	{
		S_StartSound(player->mo, sfx_s226l);
		player->stingfx = false;
	}

	if (player->spinouttimer)
	{

		if (player->progressivethrust < MAXCOMBOTIME)
			player->progressivethrust++;
	}
	else
	{
		if (player->progressivethrust)
			player->progressivethrust--;
	}

	if (player->rings <= 0)
	{
		if (player->ringvisualwarning > 1)
			player->ringvisualwarning--;
	}
	else
	{
		player->ringvisualwarning = 0;
	}

	if (player->ringvisualwarning == 0 && player->rings <= 0)
	{
		player->ringvisualwarning = 6*TICRATE/2;
	}

	if (P_PlayerInPain(player) || player->respawn.state != RESPAWNST_NONE)
	{
		player->lastpickuptype = -1; // got your ass beat, go grab anything
	}

	K_UpdateTripwire(player);

	if (battleovertime.enabled)
	{
		fixed_t distanceToCenter = 0;

		if (battleovertime.radius > 0)
		{
			distanceToCenter = R_PointToDist2(player->mo->x, player->mo->y, battleovertime.x, battleovertime.y);
		}

		if (distanceToCenter + player->mo->radius > battleovertime.radius)
		{
			if (distanceToCenter - (player->mo->radius * 2) > battleovertime.radius &&
				(battleovertime.enabled >= 10*TICRATE) &&
				!(player->pflags & PF_ELIMINATED) &&
				!player->exiting)
			{
				P_DamageMobj(player->mo, NULL, NULL, 1, DMG_TIMEOVER);
			}

			if (!player->exiting && !(player->pflags & PF_ELIMINATED))
			{
				if (leveltime < player->darkness_end)
				{
					if (leveltime > player->darkness_end - DARKNESS_FADE_TIME)
					{
						player->darkness_start = leveltime - (player->darkness_end - leveltime);
					}
				}
				else
				{
					player->darkness_start = leveltime;
				}

				player->darkness_end = leveltime + (2 * DARKNESS_FADE_TIME);
			}
		}
	}

	extern consvar_t cv_fuzz;
	if (cv_fuzz.value && P_CanPickupItem(player, 1))
	{
		K_StartItemRoulette(player);
	}

	if (player->instashield)
		player->instashield--;

	if (player->eggmanexplode)
	{
		if (player->spectator)
			player->eggmanexplode = 0;
		else
		{
			player->eggmanexplode--;

			if (!S_SoundPlaying(player->mo, sfx_kc51))
				S_StartSound(player->mo, sfx_kc51);
			if (player->eggmanexplode == 5*TICRATE/2)
				S_StartSound(player->mo, sfx_s3k53);

			if (player->eggmanexplode <= 0)
			{
				mobj_t *eggsexplode;

				K_KartResetPlayerColor(player);

				//player->flashing = 0;
				eggsexplode = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SPBEXPLOSION);
				eggsexplode->height = 2 * player->mo->height;
				K_FlipFromObject(eggsexplode, player->mo);

				S_StopSoundByID(player->mo, sfx_s3k53);
				S_StopSoundByID(player->mo, sfx_kc51);

				eggsexplode->threshold = KITEM_EGGMAN;

				P_SetTarget(&eggsexplode->tracer, player->mo);

				if (player->eggmanblame >= 0
				&& player->eggmanblame < MAXPLAYERS
				&& playeringame[player->eggmanblame]
				&& !players[player->eggmanblame].spectator
				&& players[player->eggmanblame].mo)
					P_SetTarget(&eggsexplode->target, players[player->eggmanblame].mo);
			}
		}
	}

	//NOIRE Springs: Pogo stuff put in the same place as in the original code (after eggman stuff)
	if (P_IsObjectOnGround(player->mo) && player->pogospring)
	{
		if (P_MobjFlip(player->mo) * player->mo->momz <= 0) {
			player->pogospring = 0;
		}
	}

	if (player->itemtype == KITEM_BUBBLESHIELD)
	{
		if (player->bubblecool)
			player->bubblecool--;
	}
	else
	{
		player->bubbleblowup = 0;
		player->bubblecool = 0;
	}

	if (player->itemtype != KITEM_FLAMESHIELD)
	{
		if (player->flamedash)
			K_FlameDashLeftoverSmoke(player->mo);
	}

	if (cmd->buttons & BT_DRIFT)
	{
			player->pflags |= PF_DRIFTINPUT;
	}
	else
	{
		player->pflags &= ~PF_DRIFTINPUT;
	}

	// Roulette Code
	K_KartItemRoulette(player, cmd);

	// Handle invincibility sfx
	K_UpdateInvincibilitySounds(player); // Also thanks, VAda!

	if (player->tripwireState != TRIPSTATE_NONE)
	{
		if (player->tripwireState == TRIPSTATE_PASSED)
			S_StartSound(player->mo, sfx_cdfm63);
		else if (player->tripwireState == TRIPSTATE_BLOCKED)
			S_StartSound(player->mo, sfx_kc4c);

		player->tripwireState = TRIPSTATE_NONE;
	}

	// If the player is out of the game, these visuals may
	// look really strange.
	if (player->spectator == false && !(player->pflags & PF_NOCONTEST))
	{
		Obj_ServantHandSpawning(player);
	}

	K_HandleDelayedHitByEm(player);

	player->pflags &= ~PF_POINTME;

	if (player->icecube.frozen && player->icecube.shaketimer)
	{
		player->mo->sprxoff += P_RandomRange(PR_DECORATION, -4, 4) * player->mo->scale;
		player->mo->spryoff += P_RandomRange(PR_DECORATION, -4, 4) * player->mo->scale;
		player->mo->sprzoff += P_RandomRange(PR_DECORATION, -4, 4) * player->mo->scale;

		player->icecube.shaketimer--;
	}

	if ((player->mo->eflags & MFE_UNDERWATER) && player->curshield != KSHIELD_BUBBLE)
	{
		if (player->breathTimer < UINT16_MAX)
			player->breathTimer++;
	}

	if (spbplace == -1 && modeattacking & ATTACKING_SPB && player->mo->health && !P_PlayerInPain(player) && player->laps > 0 && !player->exiting && !(player->pflags & PF_NOCONTEST))
	{
		// I'd like this to play a sound to make the situation clearer, but this codepath seems
		// to run even when you make standard contact with the SPB. Not sure why. Oh well!
		// S_StartSound(NULL, sfx_s26d);
		P_DamageMobj(player->mo, NULL, NULL, 1, DMG_INSTAKILL);
	}

	N_LegacyStart(player);

}

void K_KartResetPlayerColor(player_t *player)
{
	boolean fullbright = false;

	if (!player->mo || P_MobjWasRemoved(player->mo)) // Can't do anything
		return;

	if (player->mo->health <= 0 || player->playerstate == PST_DEAD || (player->respawn.state == RESPAWNST_MOVE)) // Override everything
	{
		player->mo->colorized = (player->dye != 0);
		player->mo->color = player->dye ? player->dye : player->skincolor;
		goto finalise;
	}

	if (player->eggmanexplode) // You're gonna diiiiie
	{
		const INT32 flashtime = 4<<(player->eggmanexplode/TICRATE);
		if (player->eggmanexplode % (flashtime/2) != 0)
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
			goto finalise;
		}
		else if (player->eggmanexplode % flashtime == 0)
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_BLACK;
			fullbright = true;
			goto finalise;
		}
		else
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_CRIMSON;
			fullbright = true;
			goto finalise;
		}
	}

	if (player->invincibilitytimer) // You're gonna kiiiiill
	{
		fullbright = true;
		player->mo->color = K_RainbowColor(leveltime);
		player->mo->colorized = true;
		goto finalise;
	}

	if (player->growshrinktimer) // Ditto, for grow/shrink
	{
		if (player->growshrinktimer % 5 == 0)
		{
			player->mo->colorized = true;
			player->mo->color = (player->growshrinktimer < 0 ? SKINCOLOR_CREAMSICLE : SKINCOLOR_PERIWINKLE);
			fullbright = true;
			goto finalise;
		}
	}

	if (player->ringboost && (leveltime & 1)) // ring boosting
	{
		player->mo->colorized = true;
		fullbright = true;
		goto finalise;
	}

	if (player->icecube.frozen)
	{
		player->mo->colorized = true;
		player->mo->color = SKINCOLOR_CYAN;
		goto finalise;
	}

	player->mo->colorized = (player->dye != 0);
	player->mo->color = player->dye ? player->dye : player->skincolor;

finalise:

	if (player->curshield)
	{
		fullbright = true;
	}

	if (fullbright == true)
	{
		player->mo->frame |= FF_FULLBRIGHT;
	}
	else
	{
		if (!(player->mo->state->frame & FF_FULLBRIGHT))
			player->mo->frame &= ~FF_FULLBRIGHT;
	}
}

void K_KartPlayerAfterThink(player_t *player)
{
	K_KartResetPlayerColor(player);

	// Move held objects (Bananas, Orbinaut, etc)
	K_MoveHeldObjects(player);

	// Jawz reticule (seeking)
	if (player->itemtype == KITEM_JAWZ && (player->itemflags & IF_ITEMOUT))
	{
		INT32 lasttarg = player->lastjawztarget;
		player_t *targ;
		mobj_t *ret;

		if (player->jawztargetdelay && playeringame[lasttarg] && !players[lasttarg].spectator)
		{
			targ = &players[lasttarg];
			player->jawztargetdelay--;
		}
		else
			targ = K_FindJawzTarget(player->mo, player);

		if (!targ || !targ->mo || P_MobjWasRemoved(targ->mo))
		{
			player->lastjawztarget = -1;
			player->jawztargetdelay = 0;
			return;
		}

		ret = P_SpawnMobj(targ->mo->x, targ->mo->y, targ->mo->z, MT_PLAYERRETICULE);
		ret->old_x = targ->mo->old_x;
		ret->old_y = targ->mo->old_y;
		ret->old_z = targ->mo->old_z;
		P_SetTarget(&ret->target, targ->mo);
		ret->frame |= ((leveltime % 10) / 2);
		ret->tics = 1;
		ret->color = player->skincolor;

		if (targ-players != lasttarg)
		{
			if (P_IsPartyPlayer(player) || P_IsPartyPlayer(targ))
				S_StartSound(NULL, sfx_s3k89);
			else
				S_StartSound(targ->mo, sfx_s3k89);

			player->lastjawztarget = targ-players;
			player->jawztargetdelay = 5;
		}
	}
	else
	{
		player->lastjawztarget = -1;
		player->jawztargetdelay = 0;
	}

	if (player->itemtype == KITEM_LIGHTNINGSHIELD || ((gametyperules & GTR_POWERSTONES) && K_IsPlayerWanted(player)))
	{
		K_LookForRings(player->mo);
	}

}

/*--------------------------------------------------
	static boolean K_SetPlayerNextWaypoint(player_t *player)

		Sets the next waypoint of a player, by finding their closest waypoint, then checking which of itself and next or
		previous waypoints are infront of the player.
		Also sets the current waypoint.

	Input Arguments:-
		player - The player the next waypoint is being found for

	Return:-
		Whether it is safe to update the respawn waypoint
--------------------------------------------------*/
static boolean K_SetPlayerNextWaypoint(player_t *player)
{
	waypoint_t *finishline = K_GetFinishLineWaypoint();
	waypoint_t *bestwaypoint = NULL;
	boolean    updaterespawn = false;

	if ((player != NULL) && (player->mo != NULL) && (P_MobjWasRemoved(player->mo) == false))
	{
		waypoint_t *waypoint     = K_GetBestWaypointForMobj(player->mo, player->currentwaypoint);

		// Our current waypoint.
		bestwaypoint = waypoint;

		if (bestwaypoint != NULL)
		{
			player->currentwaypoint = bestwaypoint;
		}

		// check the waypoint's location in relation to the player
		// If it's generally in front, it's fine, otherwise, use the best next/previous waypoint.
		// EXCEPTION: If our best waypoint is the finishline AND we're facing towards it, don't do this.
		// Otherwise it breaks the distance calculations.
		if (waypoint != NULL)
		{
			angle_t playerangle     = player->mo->angle;
			angle_t momangle        = K_MomentumAngle(player->mo);
			angle_t angletowaypoint =
				R_PointToAngle2(player->mo->x, player->mo->y, waypoint->mobj->x, waypoint->mobj->y);
			angle_t angledelta      = ANGLE_180;
			angle_t momdelta        = ANGLE_180;

			angledelta = playerangle - angletowaypoint;
			if (angledelta > ANGLE_180)
			{
				angledelta = InvAngle(angledelta);
			}

			momdelta = momangle - angletowaypoint;
			if (momdelta > ANGLE_180)
			{
				momdelta = InvAngle(momdelta);
			}

			// We're using a lot of angle calculations here, because only using facing angle or only using momentum angle both have downsides.
			// nextwaypoints will be picked if you're facing OR moving forward.
			// prevwaypoints will be picked if you're facing AND moving backward.
#if 0
			if (angledelta > ANGLE_45 || momdelta > ANGLE_45)
#endif
			{
				angle_t nextbestdelta = ANGLE_90;
				angle_t nextbestmomdelta = ANGLE_90;
				angle_t nextbestanydelta = ANGLE_MAX;
				size_t i = 0U;

				if ((waypoint->nextwaypoints != NULL) && (waypoint->numnextwaypoints > 0U))
				{
					for (i = 0U; i < waypoint->numnextwaypoints; i++)
					{
						if (!K_GetWaypointIsEnabled(waypoint->nextwaypoints[i]))
						{
							continue;
						}

						if (K_PlayerUsesBotMovement(player) == true
						&& K_GetWaypointIsShortcut(waypoint->nextwaypoints[i]) == true
						&& K_BotCanTakeCut(player) == false)
						{
							// Bots that aren't able to take a shortcut will ignore shortcut waypoints.
							// (However, if they're already on a shortcut, then we want them to keep going.)

							if (player->nextwaypoint != NULL
							&& K_GetWaypointIsShortcut(player->nextwaypoint) == false)
							{
								continue;
							}
						}

						angletowaypoint = R_PointToAngle2(
							player->mo->x, player->mo->y,
							waypoint->nextwaypoints[i]->mobj->x, waypoint->nextwaypoints[i]->mobj->y);

						angledelta = playerangle - angletowaypoint;
						if (angledelta > ANGLE_180)
						{
							angledelta = InvAngle(angledelta);
						}

						momdelta = momangle - angletowaypoint;
						if (momdelta > ANGLE_180)
						{
							momdelta = InvAngle(momdelta);
						}

						if (angledelta < nextbestanydelta || momdelta < nextbestanydelta)
						{
							nextbestanydelta = min(angledelta, momdelta);
							player->besthanddirection = angletowaypoint;

							if (nextbestanydelta >= ANGLE_90)
								continue;

							// Wanted to use a next waypoint, so remove WRONG WAY flag.
							// Done here instead of when set, because of finish line
							// hacks meaning we might not actually use this one, but
							// we still want to acknowledge we're facing the right way.
							player->pflags &= ~PF_WRONGWAY;

							if (waypoint->nextwaypoints[i] != finishline) // Allow finish line.
							{
								if (P_TraceWaypointTraversal(player->mo, waypoint->nextwaypoints[i]->mobj) == false)
								{
									// Save sight checks when all of the other checks pass, so we only do it if we have to
									continue;
								}
							}

							bestwaypoint = waypoint->nextwaypoints[i];

							if (angledelta < nextbestdelta)
							{
								nextbestdelta = angledelta;
							}
							if (momdelta < nextbestmomdelta)
							{
								nextbestmomdelta = momdelta;
							}

							updaterespawn = true;
						}
					}
				}

				if ((waypoint->prevwaypoints != NULL) && (waypoint->numprevwaypoints > 0U)
				&& !(K_PlayerUsesBotMovement(player))) // Bots do not need prev waypoints
				{
					for (i = 0U; i < waypoint->numprevwaypoints; i++)
					{
						if (!K_GetWaypointIsEnabled(waypoint->prevwaypoints[i]))
						{
							continue;
						}

						angletowaypoint = R_PointToAngle2(
							player->mo->x, player->mo->y,
							waypoint->prevwaypoints[i]->mobj->x, waypoint->prevwaypoints[i]->mobj->y);

						angledelta = playerangle - angletowaypoint;
						if (angledelta > ANGLE_180)
						{
							angledelta = InvAngle(angledelta);
						}

						momdelta = momangle - angletowaypoint;
						if (momdelta > ANGLE_180)
						{
							momdelta = InvAngle(momdelta);
						}

						if (angledelta < nextbestdelta && momdelta < nextbestmomdelta)
						{
							if (waypoint->prevwaypoints[i] == finishline) // NEVER allow finish line.
							{
								continue;
							}

							if (P_TraceWaypointTraversal(player->mo, waypoint->prevwaypoints[i]->mobj) == false)
							{
								// Save sight checks when all of the other checks pass, so we only do it if we have to
								continue;
							}

							bestwaypoint = waypoint->prevwaypoints[i];

							nextbestdelta = angledelta;
							nextbestmomdelta = momdelta;

							// Set wrong way flag if we're using prevwaypoints
							player->pflags |= PF_WRONGWAY;
							updaterespawn = false;
						}
					}
				}
			}
		}

		if (!P_IsObjectOnGround(player->mo))
		{
			updaterespawn = false;
		}

		if (player->pflags & PF_UPDATEMYRESPAWN)
		{
			updaterespawn = true;
			player->pflags &= ~PF_UPDATEMYRESPAWN;
		}

		// If nextwaypoint is NULL, it means we don't want to update the waypoint until we touch another one.
		// player->nextwaypoint will keep its previous value in this case.
		if (bestwaypoint != NULL)
		{
			player->nextwaypoint = bestwaypoint;
		}
	}

	return updaterespawn;
}

/*--------------------------------------------------
	static void K_UpdateDistanceFromFinishLine(player_t *const player)

		Updates the distance a player has to the finish line.

	Input Arguments:-
		player - The player the distance is being updated for

	Return:-
		None
--------------------------------------------------*/
static void K_UpdateDistanceFromFinishLine(player_t *const player)
{
	if ((player != NULL) && (player->mo != NULL))
	{
		waypoint_t *finishline   = K_GetFinishLineWaypoint();

		// nextwaypoint is now the waypoint that is in front of us
		if ((player->exiting && !(player->pflags & PF_NOCONTEST)) || player->spectator)
		{
			// Player has finished, we don't need to calculate this
			player->distancetofinish = 0U;
		}
		else if (player->pflags & PF_NOCONTEST)
		{
			// We also don't need to calculate this, but there's also no need to destroy the data...
			;
		}
		else if ((player->currentwaypoint != NULL) && (player->nextwaypoint != NULL) && (finishline != NULL))
		{
			const boolean useshortcuts = false;
			const boolean huntbackwards = false;
			boolean pathfindsuccess = false;
			path_t pathtofinish = {0};

			pathfindsuccess =
				K_PathfindToWaypoint(player->nextwaypoint, finishline, &pathtofinish, useshortcuts, huntbackwards);

			// Update the player's distance to the finish line if a path was found.
			// Using shortcuts won't find a path, so distance won't be updated until the player gets back on track
			if (pathfindsuccess == true)
			{
				const boolean pathBackwardsReverse = ((player->pflags & PF_WRONGWAY) == 0);
				boolean pathBackwardsSuccess = false;
				path_t pathBackwards = {0};

				fixed_t disttonext = 0;
				UINT32 traveldist = 0;
				UINT32 adddist = 0;

				disttonext =
					P_AproxDistance(
						(player->mo->x >> FRACBITS) - (player->nextwaypoint->mobj->x >> FRACBITS),
						(player->mo->y >> FRACBITS) - (player->nextwaypoint->mobj->y >> FRACBITS));
				disttonext = P_AproxDistance(disttonext, (player->mo->z >> FRACBITS) - (player->nextwaypoint->mobj->z >> FRACBITS));

				traveldist = ((UINT32)disttonext) * 2;
				pathBackwardsSuccess =
					K_PathfindThruCircuit(player->nextwaypoint, traveldist, &pathBackwards, false, pathBackwardsReverse);

				if (pathBackwardsSuccess == true)
				{
					if (pathBackwards.numnodes > 1)
					{
						// Find the closest segment, and add the distance to reach it.
						vector3_t point;
						size_t i;

						vector3_t best;
						fixed_t bestPoint = INT32_MAX;
						fixed_t bestDist = INT32_MAX;
						UINT32 bestGScore = UINT32_MAX;

						point.x = player->mo->x;
						point.y = player->mo->y;
						point.z = player->mo->z;

						best.x = point.x;
						best.y = point.y;
						best.z = point.z;

						for (i = 1; i < pathBackwards.numnodes; i++)
						{
							vector3_t line[2];
							vector3_t result;

							waypoint_t *pwp = (waypoint_t *)pathBackwards.array[i - 1].nodedata;
							waypoint_t *wp = (waypoint_t *)pathBackwards.array[i].nodedata;

							fixed_t pDist = 0;
							UINT32 g = pathBackwards.array[i - 1].gscore;

							line[0].x = pwp->mobj->x;
							line[0].y = pwp->mobj->y;
							line[0].z = pwp->mobj->z;

							line[1].x = wp->mobj->x;
							line[1].y = wp->mobj->y;
							line[1].z = wp->mobj->z;

							P_ClosestPointOnLine3D(&point, line, &result);

							pDist = P_AproxDistance(point.x - result.x, point.y - result.y);
							pDist = P_AproxDistance(pDist, point.z - result.z);

							if (pDist < bestPoint)
							{
								FV3_Copy(&best, &result);

								bestPoint = pDist;

								bestDist =
									P_AproxDistance(
										(result.x >> FRACBITS) - (line[0].x >> FRACBITS),
										(result.y >> FRACBITS) - (line[0].y >> FRACBITS));
								bestDist = P_AproxDistance(bestDist, (result.z >> FRACBITS) - (line[0].z >> FRACBITS));

								bestGScore = g + ((UINT32)bestDist);
							}
						}

#if 0
						if (cv_kartdebugwaypoints.value)
						{
							mobj_t *debugmobj = P_SpawnMobj(best.x, best.y, best.z, MT_SPARK);
							P_SetMobjState(debugmobj, S_WAYPOINTORB);

							debugmobj->frame &= ~FF_TRANSMASK;
							debugmobj->frame |= FF_FULLBRIGHT; //FF_TRANS20

							debugmobj->tics = 1;
							debugmobj->color = SKINCOLOR_BANANA;
						}
#endif

						adddist = bestGScore;
					}
					/*
					else
					{
						// Only one point to work with, so just add your euclidean distance to that.
						waypoint_t *wp = (waypoint_t *)pathBackwards.array[0].nodedata;
						fixed_t disttowaypoint =
							P_AproxDistance(
								(player->mo->x >> FRACBITS) - (wp->mobj->x >> FRACBITS),
								(player->mo->y >> FRACBITS) - (wp->mobj->y >> FRACBITS));
						disttowaypoint = P_AproxDistance(disttowaypoint, (player->mo->z >> FRACBITS) - (wp->mobj->z >> FRACBITS));

						adddist = (UINT32)disttowaypoint;
					}
					*/
					Z_Free(pathBackwards.array);
				}
				/*
				else
				{
					// Fallback to adding euclidean distance to the next waypoint to the distancetofinish
					adddist = (UINT32)disttonext;
				}
				*/

				if (pathBackwardsReverse == false)
				{
					if (pathtofinish.totaldist > adddist)
					{
						player->distancetofinish = pathtofinish.totaldist - adddist;
					}
					else
					{
						player->distancetofinish = 0;
					}
				}
				else
				{
					player->distancetofinish = pathtofinish.totaldist + adddist;
				}
				Z_Free(pathtofinish.array);

				// distancetofinish is currently a flat distance to the finish line, but in order to be fully
				// correct we need to add to it the length of the entire circuit multiplied by the number of laps
				// left after this one. This will give us the total distance to the finish line, and allow item
				// distance calculation to work easily
				const mapheader_t *mapheader = mapheaderinfo[gamemap - 1];
				if ((mapheader->levelflags & LF_SECTIONRACE) == 0U)
				{
					const UINT8 numfulllapsleft = ((UINT8)numlaps - player->laps) / mapheader->lapspersection;
					player->distancetofinish += numfulllapsleft * K_GetCircuitLength();
				}
			}
		}
	}
}

static UINT32 u32_delta(UINT32 x, UINT32 y)
{
	return x > y ? x - y : y - x;
}

/*--------------------------------------------------
	static void K_UpdatePlayerWaypoints(player_t *const player)

		Updates the player's waypoints and finish line distance.

	Input Arguments:-
		player - The player to update

	Return:-
		None
--------------------------------------------------*/
static void K_UpdatePlayerWaypoints(player_t *const player)
{
	if (player->pflags & PF_FREEZEWAYPOINTS)
	{
		player->pflags &= ~PF_FREEZEWAYPOINTS;
		return;
	}

	const UINT32 distance_threshold = FixedMul(32768, mapobjectscale);

	waypoint_t *const old_currentwaypoint = player->currentwaypoint;
	waypoint_t *const old_nextwaypoint = player->nextwaypoint;

	boolean updaterespawn = K_SetPlayerNextWaypoint(player);

	// Update prev value (used for grief prevention code)
	player->distancetofinishprev = player->distancetofinish;
	K_UpdateDistanceFromFinishLine(player);

	UINT32 delta = u32_delta(player->distancetofinish, player->distancetofinishprev);
	if (delta > distance_threshold &&
		player->respawn.state == RESPAWNST_NONE && // Respawning should be a full reset.
		old_currentwaypoint != NULL && // So should touching the first waypoint ever.
		player->laps != 0 && // POSITION rooms may have unorthodox waypoints to guide bots.
		!(player->pflags & PF_TRUSTWAYPOINTS)) // Special exception.
	{
		extern consvar_t cv_debuglapcheat;
#define debug_args "Player %s: waypoint ID %d too far away (%u > %u)\n", \
		sizeu1(player - players), K_GetWaypointID(player->nextwaypoint), delta, distance_threshold
		if (cv_debuglapcheat.value)
			CONS_Printf(debug_args);
		else
			CONS_Debug(DBG_GAMELOGIC, debug_args);
#undef debug_args

		if (!cv_debuglapcheat.value)
		{
			// Distance jump is too great, keep the old waypoints and old distance.
			player->currentwaypoint = old_currentwaypoint;
			player->nextwaypoint = old_nextwaypoint;
			player->distancetofinish = player->distancetofinishprev;

			// Start the auto respawn timer when the distance jumps.
			if (!player->bigwaypointgap)
			{
				player->bigwaypointgap = AUTORESPAWN_TIME;
			}
		}
	}
	else
	{
		// Reset the auto respawn timer if distance changes are back to normal.
		if (player->bigwaypointgap && player->bigwaypointgap <= AUTORESPAWN_THRESHOLD + 1)
		{
			player->bigwaypointgap = 0;

			// While the player was in the "bigwaypointgap" state, laps did not change from crossing finish lines.
			// So reset the lap back to normal, in case they were able to get behind the line.
			player->laps = player->lastsafelap;
			player->cheatchecknum = player->lastsafecheatcheck;
		}
	}

	// Respawn point should only be updated when we're going to a nextwaypoint
	if ((updaterespawn) &&
	(player->bigwaypointgap == 0) &&
	(player->respawn.state == RESPAWNST_NONE) &&
	(player->nextwaypoint != old_nextwaypoint) &&
	(K_GetWaypointIsSpawnpoint(player->nextwaypoint)) &&
	(K_GetWaypointIsEnabled(player->nextwaypoint) == true))
	{
		player->respawn.wp = player->nextwaypoint;
		player->lastsafelap = player->laps;
		player->lastsafecheatcheck = player->cheatchecknum;
	}

	player->pflags &= ~PF_TRUSTWAYPOINTS; // clear special exception
}

INT32 K_GetKartRingPower(const player_t *player, boolean boosted)
{
	fixed_t ringPower = ((9 - player->kartspeed) + (9 - player->kartweight)) * (FRACUNIT/2);

	if (boosted == true)
	{
		ringPower = FixedMul(ringPower, K_RingDurationBoost(player));
	}

	return max(ringPower / FRACUNIT, 1);
}

// Returns false if this player being placed here causes them to collide with any other player
// Used in g_game.c for match etc. respawning
// This does not check along the z because the z is not correctly set for the spawnee at this point
boolean K_CheckPlayersRespawnColliding(INT32 playernum, fixed_t x, fixed_t y)
{
	INT32 i;
	fixed_t p1radius = players[playernum].mo->radius;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playernum == i || !playeringame[i] || players[i].spectator || !players[i].mo || players[i].mo->health <= 0
			|| players[i].playerstate != PST_LIVE || (players[i].mo->flags & MF_NOCLIP) || (players[i].mo->flags & MF_NOCLIPTHING))
			continue;

		if (abs(x - players[i].mo->x) < (p1radius + players[i].mo->radius)
			&& abs(y - players[i].mo->y) < (p1radius + players[i].mo->radius))
		{
			return false;
		}
	}
	return true;
}

INT32 K_GetKartDriftSparkValue(const player_t *player)
{
	return (26*4 + player->kartspeed*2 + (9 - player->kartweight))*8;
}


static void K_KartDrift(player_t *player, boolean onground)
{
	fixed_t minspeed = (10 * player->mo->scale); //NOIRE: No longer a const due to the pogo spring grow check we do below.

	const INT32 dsone = K_GetKartDriftSparkValue(player);
	const INT32 dstwo = K_GetKartDriftSparkValue(player) * 2;
	const INT32 dsthree = K_GetKartDriftSparkValue(player) * 4;

	const UINT16 buttons = K_GetKartButtons(player);



	// NOIRE:
	// Grown players taking yellow spring panels will go below minspeed for one tic,
	// and will then wrongdrift or have their sparks removed because of this.
	// This fixes this problem.
	if (player->pogospring && player->mo->scale > mapobjectscale)
		minspeed = FixedMul(10 << FRACBITS, mapobjectscale);

	// Drifting is actually straffing + automatic turning.
	// Holding the Jump button will enable drifting.
	// (This comment is extremely funny)

	// Drift Release (Moved here so you can't "chain" drifts)
	if (player->drift != -5 && player->drift != 5)
	{
		if (player->driftcharge < 0 || player->driftcharge >= dsone)
		{
			S_StartSound(player->mo, sfx_s23c);
			K_SpawnDashDustRelease(player);

			if (player->driftcharge < 0)
			{
				// Stage 0: Grey sparks
				if (player->driftboost < 15)
					player->driftboost = 15;
			}
			else if (player->driftcharge >= dsone && player->driftcharge < dstwo)
			{
				// Stage 1: Red sparks
				if (player->driftboost < 20)
					player->driftboost = 20;

			}
			else if (player->driftcharge < dsthree)
			{
				// Stage 2: Blue sparks
				if (player->driftboost < 50)
					player->driftboost = 50;

			}
			else if (player->driftcharge >= dsthree)
			{
				// Stage 3: Rainbow sparks
				if (player->driftboost < 125)
					player->driftboost = 125;
			}
		}
		// Remove charge
		player->driftcharge = 0;
	}

	// Drifting: left or right?
	if (!(player->pflags & PF_DRIFTINPUT))
	{
		// drift is not being performed so if we're just finishing set driftend and decrement counters
		if (player->drift > 0)
		{
			player->drift--;
			player->pflags |= PF_DRIFTEND;
		}
		else if (player->drift < 0)
		{
			player->drift++;
			player->pflags |= PF_DRIFTEND;
		}
		else
			player->pflags &= ~PF_DRIFTEND;
	}
	else if (player->speed > minspeed
		&& (player->drift == 0 || (player->pflags & PF_DRIFTEND)))
	{
		if (player->cmd.driftturn > 0)
		{
			// Starting left drift
			player->drift = 1;
			player->driftcharge = 0;
			player->pflags &= ~PF_DRIFTEND;
		}
		else if (player->cmd.driftturn < 0)
		{
			// Starting right drift
			player->drift = -1;
			player->driftcharge = 0;
			player->pflags &= ~PF_DRIFTEND;
		}
	}

	if (P_PlayerInPain(player) || player->speed < minspeed)
	{
		// Stop drifting
		player->drift = player->driftcharge = player->aizdriftstrat = 0;
		player->pflags &= ~(PF_BRAKEDRIFT|PF_GETSPARKS);
	}
	else if ((player->pflags & PF_DRIFTINPUT) && player->drift != 0)
	{
		// Incease/decrease the drift value to continue drifting in that direction
		fixed_t driftadditive = 24;
		boolean playsound = false;

		if (onground)
		{
			if (player->drift >= 1) // Drifting to the left
			{
				player->drift++;
				if (player->drift > 5)
					player->drift = 5;

				if (player->cmd.driftturn > 0) // Inward
					driftadditive += abs(player->cmd.driftturn)/100;
				if (player->cmd.driftturn < 0) // Outward
					driftadditive -= abs(player->cmd.driftturn)/75;
			}
			else if (player->drift <= -1) // Drifting to the right
			{
				player->drift--;
				if (player->drift < -5)
					player->drift = -5;

				if (player->cmd.driftturn < 0) // Inward
					driftadditive += abs(player->cmd.driftturn)/100;
				if (player->cmd.driftturn > 0) // Outward
					driftadditive -= abs(player->cmd.driftturn)/75;
			}

			// Disable drift-sparks until you're going fast enough
			if (!(player->pflags & PF_GETSPARKS)
				|| (player->offroad && K_ApplyOffroad(player)))
				driftadditive = 0;

			if (player->speed > minspeed*2)
				player->pflags |= PF_GETSPARKS;
		}
		else
		{
			driftadditive = 0;
		}

		// This spawns the drift sparks
		if ((player->driftcharge + driftadditive >= dsone)
			|| (player->driftcharge < 0))
		{
			K_SpawnDriftSparks(player);
		}

		if ((player->driftcharge < dsone && player->driftcharge+driftadditive >= dsone)
			|| (player->driftcharge < dstwo && player->driftcharge+driftadditive >= dstwo)
			|| (player->driftcharge < dsthree && player->driftcharge+driftadditive >= dsthree))
		{
			playsound = true;
		}

		// Sound whenever you get a different tier of sparks
		if (playsound && P_IsDisplayPlayer(player))
		{
			if (player->driftcharge == -1)
				S_StartSoundAtVolume(player->mo, sfx_sploss, 192); // Yellow spark sound
			else
				S_StartSoundAtVolume(player->mo, sfx_s3ka2, 192);
		}

		player->driftcharge += driftadditive;
		player->pflags &= ~PF_DRIFTEND;
	}

	if ((!player->sneakertimer)
	|| (!player->cmd.driftturn)
	|| (!player->aizdriftstrat)
	|| (player->cmd.driftturn > 0) != (player->aizdriftstrat > 0))
	{
		if (!player->drift)
			player->aizdriftstrat = 0;
		else
			player->aizdriftstrat = ((player->drift > 0) ? 1 : -1);
	}
	else if (player->aizdriftstrat && !player->drift)
		K_SpawnAIZDust(player);

	if (player->drift
		&& ((buttons & BT_BRAKE)
		|| !(buttons & BT_ACCELERATE))
		&& P_IsObjectOnGround(player->mo))
	{
		if (!(player->pflags & PF_BRAKEDRIFT))
			K_SpawnBrakeDriftSparks(player);
		player->pflags |= PF_BRAKEDRIFT;
	}
	else
		player->pflags &= ~PF_BRAKEDRIFT;
}
//
// K_KartUpdatePosition
//
void K_KartUpdatePosition(player_t *player)
{
	fixed_t position = 1;
	fixed_t oldposition = player->position;
	fixed_t i;
	INT32 realplayers = 0;

	if (player->spectator || !player->mo)
	{
		// Ensure these are reset for spectators
		player->position = 0;
		player->positiondelay = 0;
		return;
	}

	if (K_PodiumSequence() == true)
	{
		position = K_GetPodiumPosition(player);

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

			realplayers++;
		}
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

			realplayers++;

			if (gametyperules & GTR_CIRCUIT)
			{
				if (player->exiting) // End of match standings
				{
					// Only time matters
					if (players[i].realtime < player->realtime)
						position++;
				}
				else
				{
					// I'm a lap behind this player OR
					// My distance to the finish line is higher, so I'm behind
					if ((players[i].laps > player->laps)
						|| (players[i].distancetofinish < player->distancetofinish))
					{
						position++;
					}
				}
			}
			else
			{
				if (player->exiting) // End of match standings
				{
					// Only score matters
					if (players[i].roundscore > player->roundscore)
						position++;
				}
				else
				{
					UINT8 myEmeralds = K_NumEmeralds(player);
					UINT8 yourEmeralds = K_NumEmeralds(&players[i]);

					// First compare all points
					if (players[i].roundscore > player->roundscore)
					{
						position++;
					}
					else if (players[i].roundscore == player->roundscore)
					{
						// Emeralds are a tie breaker
						if (yourEmeralds > myEmeralds)
						{
							position++;
						}
						else if (yourEmeralds == myEmeralds)
						{
							// Bumpers are the second tier tie breaker
							if (K_Bumpers(&players[i]) > K_Bumpers(player))
							{
								position++;
							}
						}
					}
				}
			}
		}
	}

	if (leveltime < starttime || oldposition == 0)
		oldposition = position;

	if (position != oldposition) // Changed places?
	{
		if (!K_Cooperative() && player->positiondelay <= 0 && position < oldposition && P_IsDisplayPlayer(player) == true)
		{
			// Play sound when getting closer to 1st. MAXPLAYERS
			UINT32 soundpos = (max(0, position - 1) * 3)/realplayers; // always 1-15 despite there being 16 players at max...
#if MAXPLAYERS > 16
			if (soundpos < 15)
			{
				soundpos = 15;
			}
#endif
			S_ReducedVFXSound(player->mo, sfx_hoop3 - soundpos, NULL); // ...which is why we can start at index 2 for a lower general pitch
		}

		player->positiondelay = POS_DELAY_TIME + 4; // Position number growth
	}

	player->position = position;
}

void K_UpdateAllPlayerPositions(void)
{
	INT32 i;
	if (numbosswaypoints == 0)
	{
		// First loop: Ensure all players' distance to the finish line are all accurate
		for (i = 0; i < MAXPLAYERS; i++)
		{
			player_t *player = &players[i];
			if (!playeringame[i] || player->spectator || !player->mo || P_MobjWasRemoved(player->mo))
			{
				continue;
			}

			if (K_PodiumSequence() == true)
			{
				K_UpdatePodiumWaypoints(player);
				continue;
			}

			if (player->respawn.state == RESPAWNST_MOVE &&
				player->respawn.init == true &&
				player->lastsafelap != player->laps)
			{
				player->laps = player->lastsafelap;
				player->cheatchecknum = player->lastsafecheatcheck;
			}

			K_UpdatePlayerWaypoints(player);
		}

		// Second loop: Ensure all player positions reflect everyone's distances
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo))
			{
				K_KartUpdatePosition(&players[i]);
			}
		}
	}
	else
	{
		// Use legacy postion update code from v1
		for (i = 0; i < MAXPLAYERS; i++)
		{
			K_KartLegacyUpdatePosition(&players[i]);
		}
	}
}

//
// K_StripItems
//
void K_StripItems(player_t *player)
{
	K_DropRocketSneaker(player);
	K_DropKitchenSink(player);
	player->itemtype = KITEM_NONE;
	player->itemamount = 0;
	player->itemflags &= ~(IF_ITEMOUT|IF_EGGMANOUT);

	if (player->itemRoulette.eggman == false)
	{
		K_StopRoulette(&player->itemRoulette);
	}

	player->hyudorotimer = 0;
	player->stealingtimer = 0;

	player->curshield = KSHIELD_NONE;
	player->bananadrag = 0;
	player->sadtimer = 0;

	K_UpdateHnextList(player, true);
}

void K_StripOther(player_t *player)
{
	K_StopRoulette(&player->itemRoulette);

	player->invincibilitytimer = 0;
	if (player->growshrinktimer)
	{
		K_RemoveGrowShrink(player);
	}

	if (player->eggmanexplode)
	{
		player->eggmanexplode = 0;
		player->eggmanblame = -1;

		K_KartResetPlayerColor(player);
	}
}

INT32 K_FlameShieldMax(player_t *player)
{
	UINT32 disttofinish = 0;
	UINT32 distv = 1024; // Pre no-scams: 2048
	distv = distv * 16 / FLAMESHIELD_MAX; // Old distv was based on a 16-segment bar
	UINT8 numplayers = 0;
	UINT32 scamradius = 1500; // How close is close enough that we shouldn't be allowed to scam 1st?
	UINT8 i;

	if (gametyperules & GTR_CIRCUIT)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && !players[i].spectator)
				numplayers++;
			if (players[i].position == 1)
				disttofinish = players[i].distancetofinish;
		}
	}

	disttofinish = player->distancetofinish - disttofinish;
	distv = FixedMul(distv, mapobjectscale);

	if (numplayers <= 1)
	{
		return FLAMESHIELD_MAX; // max when alone, for testing
		// and when in battle, for chaos
	}
	else if (player->position == 1 || disttofinish < scamradius)
	{
		return 0; // minimum for first
	}

	disttofinish = disttofinish - scamradius;

	return min(FLAMESHIELD_MAX, (FLAMESHIELD_MAX / 16) + (disttofinish / distv)); // Ditto for this minimum, old value was 1/16
}

SINT8 K_Sliptiding(const player_t *player)
{
	/*
	if (player->mo->eflags & MFE_UNDERWATER)
		return 0;
	*/
	return player->drift ? 0 : player->aizdriftstrat;
}

static void K_AirFailsafe(player_t *player)
{
	const fixed_t maxSpeed = 6*player->mo->scale;
	const fixed_t thrustSpeed = 6*player->mo->scale; // 10*player->mo->scale

	if (player->speed > maxSpeed // Above the max speed that you're allowed to use this technique.
		|| player->respawn.state != RESPAWNST_NONE // Respawning, you don't need this AND drop dash :V
		|| player->preventfailsafe) // You just got hit or interacted with something committal, no mashing for distance
	{
		player->pflags &= ~PF_AIRFAILSAFE;
		return;
	}

	// Maps with "drop-in" POSITION areas (Dragonspire, Hydrocity) cause problems if we allow failsafe.
	if (leveltime < introtime)
		return;

	UINT8 buttons = K_GetKartButtons(player);

	// Accel inputs queue air-failsafe for when they're released,
	if ((buttons & (BT_ACCELERATE|BT_BRAKE)) == BT_ACCELERATE || K_GetForwardMove(player) != 0)
	{
		player->pflags |= PF_AIRFAILSAFE;
		return;
	}

	if (player->pflags & PF_AIRFAILSAFE)
	{
		// Push the player forward
		P_Thrust(player->mo, K_MomentumAngle(player->mo), thrustSpeed);

		S_StartSound(player->mo, sfx_s23c);

		player->pflags &= ~PF_AIRFAILSAFE;
	}
}

//
// K_AdjustPlayerFriction
//
void K_AdjustPlayerFriction(player_t *player, boolean onground)
{
	// JugadorXEI: Do *not* calculate friction when a player is pogo'd
	// because they'll be in the air and friction will not reset!
	if (onground && !player->pogospring)
	{
		// Friction
		if (!player->offroad)
		{
			if (player->speed > 0 && player->cmd.forwardmove == 0 && player->mo->friction == 59392)
				player->mo->friction += 4608;
		}

		if (player->speed > 0 && player->cmd.forwardmove < 0)	// change friction while braking no matter what, otherwise it's not any more effective than just letting go off accel
			player->mo->friction -= 2048;

		// Karma ice physics
		if ((gametyperules & GTR_KARMA) && player->mo->health <= 0)
		{
			player->mo->friction += 1228;

			if (player->mo->friction > FRACUNIT)
				player->mo->friction = FRACUNIT;
			if (player->mo->friction < 0)
				player->mo->friction = 0;

			player->mo->movefactor = FixedDiv(ORIG_FRICTION, player->mo->friction);

			if (player->mo->movefactor < FRACUNIT)
				player->mo->movefactor = 19*player->mo->movefactor - 18*FRACUNIT;
			else
				player->mo->movefactor = FRACUNIT; //player->mo->movefactor = ((player->mo->friction - 0xDB34)*(0xA))/0x80;

			if (player->mo->movefactor < 32)
				player->mo->movefactor = 32;
		}

		// Wipeout slowdown
		if (player->speed > 0 && player->spinouttimer && player->wipeoutslow)
		{
			if (player->offroad)
				player->mo->friction -= 4912;
			if (player->wipeoutslow == 1)
				player->mo->friction -= 9824;
		}
	}
}

void K_SetItemOut(player_t *player)
{
	player->itemflags |= IF_ITEMOUT;
}

void K_UnsetItemOut(player_t *player)
{
	player->itemflags &= ~IF_ITEMOUT;
	player->bananadrag = 0;
}

//
// K_MoveKartPlayer
//
void K_MoveKartPlayer(player_t *player, boolean onground)
{
	ticcmd_t *cmd = &player->cmd;
	boolean ATTACK_IS_DOWN = ((cmd->buttons & BT_ATTACK) && !(player->oldcmd.buttons & BT_ATTACK) && (player->respawn.state == RESPAWNST_NONE));
	boolean HOLDING_ITEM = (player->itemflags & (IF_ITEMOUT|IF_EGGMANOUT));
	boolean NO_HYUDORO = (player->stealingtimer == 0);

	// Play overtake sounds, but only if
	// - your place changed
	// - not exiting
	// - in relevant gametype
	// - more than 10 seconds after initial scramble
	if (player->oldposition != player->position
	&& !player->exiting
	&& (gametyperules & GTR_CIRCUIT)
	&& !K_Cooperative()
	&& leveltime >= starttime+(10*TICRATE))
	{
		if (player->oldposition < player->position) // But first, if you lost a place,
		{
			// then the other player taunts.
			K_RegularVoiceTimers(player); // and you can't for a bit
		}
		else // Otherwise,
		{
			K_PlayOvertakeSound(player->mo); // Say "YOU'RE TOO SLOW!"
		}
	}
	player->oldposition = player->position;

	// Prevent ring misfire
	if (!(cmd->buttons & BT_ATTACK))
	{
		if (player->itemtype == KITEM_NONE
			&& NO_HYUDORO && !(HOLDING_ITEM
			|| player->itemamount
			|| player->itemRoulette.active == true
			|| player->rocketsneakertimer
			|| player->eggmanexplode))
			player->itemflags |= IF_USERINGS;
		else
			player->itemflags &= ~IF_USERINGS;
	}

	if (player && player->mo && K_PlayerCanUseItem(player))
	{
		// First, the really specific, finicky items that function without the item being directly in your item slot.
		{
			// Ring boosting
			if (player->itemflags & IF_USERINGS)
			{
				// Auto-Ring
				UINT8 tiereddelay = 5;
				player->autoring = false;
				if (
					player->pflags & PF_AUTORING
					&& leveltime > starttime
					&& K_GetForwardMove(player) > 0
					&& P_IsObjectOnGround(player->mo)
				)
				{
					fixed_t pspeed = FixedDiv(player->speed * 100, K_GetKartSpeed(player, false, true));

					if (player->rings >= 18 && pspeed < 100*FRACUNIT)
					{
						player->autoring = true;
						tiereddelay = 3;
					}
					else if (player->rings >= 10 && pspeed < 85*FRACUNIT)
					{
						player->autoring = true;
						tiereddelay = 4;
					}
					else if (player->rings >= 4 && pspeed < 35*FRACUNIT)
					{
						player->autoring = true;
						tiereddelay = 5;
					}
					else
						player->autoring = false;
				}

				if (((cmd->buttons & BT_ATTACK) || player->autoring) && !player->ringdelay && player->rings > 0)
				{
					mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
					P_SetMobjState(ring, S_FASTRING1);

					if (P_IsDisplayPlayer(player))
					{
						UINT8 startfade = 220;
						UINT8 transfactor = 10 * (min(startfade, player->ringtransparency)) / startfade;
						if (transfactor < 10)
						{
							transfactor = max(transfactor, 4);
							ring->renderflags |= ((10-transfactor) << RF_TRANSSHIFT);
							ring->renderflags |= RF_ADD;
						}
					}
					player->ringtransparency -= RINGTRANSPARENCYUSEPENALTY;

					ring->extravalue1 = 1; // Ring use animation timer
					ring->extravalue2 = 1; // Ring use animation flag
					ring->shadowscale = 0;
					P_SetTarget(&ring->target, player->mo); // user

					const INT32 followerskin = K_GetEffectiveFollowerSkin(player);
					if (player->autoring
						&& player->follower != NULL
						&& P_MobjWasRemoved(player->follower) == false
						&& followerskin >= 0
						&& followerskin < numfollowers)
					{
						const follower_t *fl = &followers[followerskin];

						ring->cusval = player->follower->x - player->mo->x;
						ring->cvmem = player->follower->y - player->mo->y;
						ring->movefactor = P_GetMobjHead(player->follower) - P_GetMobjHead(player->mo);

						// cvmem is used to play the ring animation for followers
						player->follower->cvmem = fl->ringtime;
					}
					else
					{
						ring->cusval = ring->cvmem = ring->movefactor = 0;
					}

					// really silly stupid dumb HACK to fix interp
					// without needing to duplicate any code
					A_AttractChase(ring);
					P_SetOrigin(ring, ring->x, ring->y, ring->z);
					ring->extravalue1 = 1;

					player->rings--;
					if (player->autoring && !(cmd->buttons & BT_ATTACK))
						player->ringdelay = tiereddelay;
					else
						player->ringdelay = 3;
				}

			}
			// Other items
			else
			{
				// Eggman Monitor exploding
				if (player->eggmanexplode)
				{
					if (ATTACK_IS_DOWN && player->eggmanexplode <= 3*TICRATE && player->eggmanexplode > 1)
					{
						player->eggmanexplode = 1;
						player->botvars.itemconfirm = 0;
					}
				}
				// Eggman Monitor throwing
				else if (player->itemflags & IF_EGGMANOUT)
				{
					if (ATTACK_IS_DOWN)
					{
						K_ThrowKartItem(player, false, MT_EGGMANITEM, -1, 0);
						K_PlayAttackTaunt(player->mo);
						player->itemflags &= ~IF_EGGMANOUT;
						K_UpdateHnextList(player, true);
						player->botvars.itemconfirm = 0;
					}
				}
				// Rocket Sneaker usage
				else if (player->rocketsneakertimer > 1)
				{
					if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
					{
						K_DoSneaker(player, 2);
						K_PlayBoostTaunt(player->mo);
						if (player->rocketsneakertimer <= 3*TICRATE)
							player->rocketsneakertimer = 1;
						else
							player->rocketsneakertimer -= 3*TICRATE;
						player->botvars.itemconfirm = 2*TICRATE;
					}
				}
				else if (player->itemamount == 0)
				{
					K_UnsetItemOut(player);
				}
				else
				{
					switch (player->itemtype)
					{
						case KITEM_SNEAKER:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
							{
								K_DoSneaker(player, 1);
								K_PlayBoostTaunt(player->mo);
								player->itemamount--;
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_ROCKETSNEAKER:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO
								&& player->rocketsneakertimer == 0)
							{
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								K_PlayBoostTaunt(player->mo);
								S_StartSound(player->mo, sfx_s3k3a);

								//K_DoSneaker(player, 2);

								player->rocketsneakertimer = (itemtime*3);
								player->itemamount--;
								K_UpdateHnextList(player, true);

								for (moloop = 0; moloop < 2; moloop++)
								{
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_ROCKETSNEAKER);
									K_MatchGenericExtraFlags(mo, player->mo);
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = player->mo->angle;
									mo->threshold = 10;
									mo->movecount = moloop%2;
									mo->movedir = mo->lastlook = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_INVINCIBILITY:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO) // Doesn't hold your item slot hostage normally, so you're free to waste it if you have multiple
							{
								K_DoInvincibility(player,itemtime+(2*TICRATE));// 10 seconds
								K_PlayPowerGloatSound(player->mo);

								player->itemamount--;
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_BANANA:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								INT32 moloop;
								mobj_t *mo;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								K_SetItemOut(player);
								S_StartSound(player->mo, sfx_s254);

								for (moloop = 0; moloop < player->itemamount; moloop++)
								{
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BANANA_SHIELD);
									if (!mo)
									{
										player->itemamount = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = player->itemamount;
									mo->movedir = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
								player->botvars.itemconfirm = 0;
							}
							else if (ATTACK_IS_DOWN && (player->itemflags & IF_ITEMOUT)) // Banana x3 thrown
							{
								K_ThrowKartItem(player, false, MT_BANANA, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_EGGMAN:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->itemamount--;
								player->itemflags |= IF_EGGMANOUT;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EGGMANITEM_SHIELD);
								if (mo)
								{
									K_FlipFromObject(mo, player->mo);
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_ORBINAUT:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								angle_t newangle;
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								K_SetItemOut(player);
								S_StartSound(player->mo, sfx_s3k3a);

								for (moloop = 0; moloop < player->itemamount; moloop++)
								{
									newangle = (player->mo->angle + ANGLE_157h) + FixedAngle(((360 / player->itemamount) * moloop) << FRACBITS) + ANGLE_90;
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_ORBINAUT_SHIELD);
									if (!mo)
									{
										player->itemamount = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = newangle;
									mo->threshold = 10;
									mo->movecount = player->itemamount;
									mo->movedir = mo->lastlook = moloop+1;
									mo->color = player->skincolor;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
								player->botvars.itemconfirm = 0;
							}
							else if (ATTACK_IS_DOWN && (player->itemflags & IF_ITEMOUT)) // Orbinaut x3 thrown
							{
								K_ThrowKartItem(player, true, MT_ORBINAUT, 1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_JAWZ:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								angle_t newangle;
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								K_SetItemOut(player);
								S_StartSound(player->mo, sfx_s3k3a);

								for (moloop = 0; moloop < player->itemamount; moloop++)
								{
									newangle = (player->mo->angle + ANGLE_157h) + FixedAngle(((360 / player->itemamount) * moloop) << FRACBITS) + ANGLE_90;
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_JAWZ_SHIELD);
									if (!mo)
									{
										player->itemamount = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = newangle;
									mo->threshold = 10;
									mo->movecount = player->itemamount;
									mo->movedir = mo->lastlook = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
								player->botvars.itemconfirm = 0;
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && (player->itemflags & IF_ITEMOUT)) // Jawz thrown
							{
								if (player->throwdir == 1 || player->throwdir == 0)
									K_ThrowKartItem(player, true, MT_JAWZ, 1, 0);
								else if (player->throwdir == -1) // Throwing backward gives you a dud that doesn't home in
									K_ThrowKartItem(player, true, MT_JAWZ_DUD, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_MINE:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								K_SetItemOut(player);
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SSMINE_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
								player->botvars.itemconfirm = 0;
							}
							else if (ATTACK_IS_DOWN && (player->itemflags & IF_ITEMOUT))
							{
								K_ThrowKartItem(player, false, MT_SSMINE, 1, 1);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								player->itemflags &= ~IF_ITEMOUT;
								K_UpdateHnextList(player, true);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_LANDMINE:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->itemamount--;
								K_ThrowLandMine(player);
								K_PlayAttackTaunt(player->mo);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_BALLHOG:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_ThrowKartItem(player, true, MT_BALLHOG, 1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_SPB:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->itemamount--;
								K_SetItemOut(player);
								K_ThrowKartItem(player, true, MT_SPB, 1, 0);
								K_UnsetItemOut(player);
								K_PlayAttackTaunt(player->mo);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_GROW:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								if (player->growshrinktimer < 0)
								{
									// Old v1 behavior was to have Grow counter Shrink,
									// but Shrink is now so ephemeral that it should just work.
									K_RemoveGrowShrink(player);
									// So we fall through here.
								}

								K_PlayPowerGloatSound(player->mo);

								player->mo->scalespeed = mapobjectscale/TICRATE;
								player->mo->destscale = FixedMul(mapobjectscale, GROW_SCALE);

								if (K_PlayerShrinkCheat(player) == true)
								{
									player->mo->destscale = FixedMul(player->mo->destscale, SHRINK_SCALE);
								}

								if (P_IsPartyPlayer(player) == false && player->invincibilitytimer == 0)
								{
									// don't play this if the player has invincibility -- that takes priority
									S_StartSound(player->mo, sfx_alarmg);
								}

								player->growshrinktimer = itemtime+(4*TICRATE); // 12 seconds

								S_StartSound(player->mo, sfx_kc5a);

								player->itemamount--;
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_SHRINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoShrink(player);
								player->itemamount--;
								K_PlayPowerGloatSound(player->mo);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_LIGHTNINGSHIELD:
							if (player->curshield != KSHIELD_LIGHTNING)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_LIGHTNINGSHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k41);
								player->curshield = KSHIELD_LIGHTNING;
							}

							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoLightningShield(player);
								if (player->itemamount > 0)
								{
									// Why is this a conditional?
									// Lightning shield: the only item that allows you to
									// activate a mine while you're out of its radius,
									// the SAME tic it sets your itemamount to 0
									// ...:dumbestass:
									player->itemamount--;
									K_PlayAttackTaunt(player->mo);
									player->botvars.itemconfirm = 0;
								}
							}
							break;
						case KITEM_BUBBLESHIELD:
							if (player->curshield != KSHIELD_BUBBLE)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BUBBLESHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k3f);
								player->curshield = KSHIELD_BUBBLE;
							}

							if (!HOLDING_ITEM && NO_HYUDORO)
							{
								if ((cmd->buttons & BT_ATTACK) && (player->itemflags & IF_HOLDREADY))
								{
									if (player->bubbleblowup == 0)
										S_StartSound(player->mo, sfx_s3k75);

									player->bubbleblowup++;
									player->bubblecool = player->bubbleblowup*4;

									if (player->bubbleblowup > bubbletime*2)
									{
										K_ThrowKartItem(player, (player->throwdir > 0), MT_BUBBLESHIELDTRAP, -1, 0);
										if (player->throwdir == -1)
										{
											P_InstaThrust(player->mo, player->mo->angle, player->speed + (80 * mapobjectscale));
											player->fakeBoost += TICRATE/2;
										}
										K_PlayAttackTaunt(player->mo);
										player->bubbleblowup = 0;
										player->bubblecool = 0;
										player->itemflags &= ~IF_HOLDREADY;
										player->itemamount--;
										player->botvars.itemconfirm = 0;
									}
								}
								else
								{
									if (player->bubbleblowup > bubbletime)
										player->bubbleblowup = bubbletime;

									if (player->bubbleblowup)
										player->bubbleblowup--;

									if (player->bubblecool)
										player->itemflags &= ~IF_HOLDREADY;
									else
										player->itemflags |= IF_HOLDREADY;
								}
							}
							break;
						case KITEM_FLAMESHIELD:
							if (player->curshield != KSHIELD_FLAME)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_FLAMESHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k3e);
								player->curshield = KSHIELD_FLAME;
							}

							if (!HOLDING_ITEM && NO_HYUDORO)
							{
								INT32 destlen = K_FlameShieldMax(player);
								INT32 flamemax = 0;

								if (player->flamelength < destlen)
									player->flamelength = min(destlen, player->flamelength + 7); // Allows gauge to grow quickly when first acquired. 120/16 = ~7

								flamemax = player->flamelength + TICRATE; // TICRATE leniency period, but we block most effects at flamelength 0 down below

								if ((cmd->buttons & BT_ATTACK) && (player->itemflags & IF_HOLDREADY))
								{
									const INT32 incr = (gametyperules & GTR_CLOSERPLAYERS) ? 5 : 4;
									player->flamemeter += incr;

									if (player->flamelength)
									{

										if (player->flamedash == 0)
										{
											//S_StartSound(player->mo, sfx_s3k43);
											K_PlayBoostTaunt(player->mo);
											S_StartSoundAtVolume(player->mo, sfx_fshld0, 255/3);
											S_StartSoundAtVolume(player->mo, sfx_fshld1, 255/3);
										}

										S_StopSoundByID(player->mo, sfx_fshld3);

										player->flamedash += (incr/2);

										if (!onground)
										{
											P_Thrust(
												player->mo, K_MomentumAngle(player->mo),
												FixedMul(player->mo->scale, K_GetKartGameSpeedScalar(gamespeed))
											);
										}
									}

									if (player->flamemeter > flamemax)
									{
										P_Thrust(
											player->mo, player->mo->angle,
											FixedMul((50*player->mo->scale), K_GetKartGameSpeedScalar(gamespeed))
										);

										player->fakeBoost = TICRATE/3;

										S_StopSoundByID(player->mo, sfx_fshld1);
										S_StopSoundByID(player->mo, sfx_fshld0);
										S_StartSoundAtVolume(player->mo, sfx_fshld2, 255/3);

										player->flamemeter = 0;
										player->flamelength = 0;
										player->itemflags &= ~IF_HOLDREADY;
										player->itemamount--;
									}
								}
								else
								{
									player->itemflags |= IF_HOLDREADY;

									if (!(gametyperules & GTR_CLOSERPLAYERS) || leveltime % 6 == 0)
									{
										if (player->flamemeter > 0)
										{
											player->flamemeter--;
											if (!player->flamemeter)
												S_StopSoundByID(player->mo, sfx_fshld3);
										}
									}

									if (player->flamelength > destlen)
									{
										player->flamelength--; // Can ONLY go down if you're not using it

										flamemax = player->flamelength;
										if (flamemax > 0)
											flamemax += TICRATE; // leniency period
									}

									if (player->flamemeter > flamemax)
										player->flamemeter = flamemax;
								}
							}
							break;
						case KITEM_HYUDORO:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->itemamount--;
								K_DoHyudoroSteal(player); // yes. yes they do.
								//Obj_HyudoroDeploy(player->mo);
								K_PlayAttackTaunt(player->mo);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_POGOSPRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
							{
								K_PlayBoostTaunt(player->mo);
								K_DoPogoSpring(player->mo, 32<<FRACBITS, 2);
								P_SpawnMobjFromMobj(player->mo, 0, 0, 0, MT_POGOSPRING);
								player->itemamount--;
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_SUPERRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_AwardPlayerRings(player, 20, true);
								player->itemamount--;
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_KITCHENSINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								K_SetItemOut(player);
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SINK_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
								player->botvars.itemconfirm = 0;
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && (player->itemflags & IF_ITEMOUT)) // Sink thrown
							{
								K_ThrowKartItem(player, false, MT_SINK, 1, 2);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								player->itemflags &= ~IF_ITEMOUT;
								K_UpdateHnextList(player, true);
								player->botvars.itemconfirm = 0;
							}
							break;
						case KITEM_SAD:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO
								&& !player->sadtimer)
							{
								player->sadtimer = stealtime;
								player->itemamount--;
								player->botvars.itemconfirm = 0;
							}
							break;
						default:
							break;
					}
				}
			}
		}

		// No more!
		if (!player->itemamount)
		{
			player->itemflags &= ~IF_ITEMOUT;
			player->itemtype = KITEM_NONE;
		}

		if (K_GetShieldFromItem(player->itemtype) == KSHIELD_NONE)
		{
			player->curshield = KSHIELD_NONE; // RESET shield type
			player->bubbleblowup = 0;
			player->bubblecool = 0;
			player->flamelength = 0;
			player->flamemeter = 0;
		}

		if (spbplace == -1 || player->position != spbplace)
		{
			player->pflags &= ~PF_RINGLOCK; // reset ring lock
		}


		if (K_ItemSingularity(player->itemtype) == true)
		{
			K_SetItemCooldown(player->itemtype, 20*TICRATE);
		}

		if (player->hyudorotimer > 0)
		{
			player->mo->renderflags |= RF_DONTDRAW | RF_MODULATE;
			player->mo->renderflags &= ~K_GetPlayerDontDrawFlag(player);

			if (!(leveltime & 1) && (player->hyudorotimer < (TICRATE/2) || player->hyudorotimer > hyudorotime-(TICRATE/2)))
			{
				player->mo->renderflags &= ~(RF_DONTDRAW | RF_BLENDMASK);
			}

			player->flashing = player->hyudorotimer; // We'll do this for now, let's people know about the invisible people through subtle hints
		}
		else if (player->hyudorotimer == 0)
		{
			player->mo->renderflags &= ~RF_BLENDMASK;
		}
	}

	K_KartDrift(player, (onground || player->pogospring));


	// Quick Turning
	// You can't turn your kart when you're not moving.
	// So now it's time to burn some rubber!
	if (player->speed < 2 && leveltime > starttime && player->cmd.buttons & BT_ACCELERATE && player->cmd.buttons & BT_BRAKE && player->cmd.driftturn != 0)
	{
		if (leveltime % 8 == 0)
			S_StartSound(player->mo, sfx_s224);
	}

	// Squishing
	// If a Grow player or a sector crushes you, get flattened instead of being killed.
	if (player->squishedtimer <= 0)
	{
		player->mo->flags &= ~MF_NOCLIP;
	}
	else
	{
		player->mo->flags |= MF_NOCLIP;
		player->mo->momx = 0;
		player->mo->momy = 0;
	}

	if (onground == false
	&& !player->bungee		// if this list of condition ever gets bigger, maybe this should become a function.
	)
	{
		K_AirFailsafe(player);
	}
	else
	{
		player->pflags &= ~PF_AIRFAILSAFE;
	}

	Obj_RingShooterInput(player);

	if (player->bungee)
		Obj_playerBungeeThink(player);

	if (player->dlzrocket)
		Obj_playerDLZRocket(player);

	if (player->seasawcooldown && !player->seasaw)
		player->seasawcooldown--;

	if (player->turbine)
	{
		if (player->mo->tracer && !P_MobjWasRemoved(player->mo->tracer))
			Obj_playerWPZTurbine(player);
		else
			player->turbine--;	// acts as a cooldown
	}

	if (player->icecube.frozen)
	{
		Obj_IceCubeInput(player);
	}

	Obj_PlayerCloudThink(player);

	Obj_PlayerBulbThink(player);

}

void K_CheckSpectateStatus(boolean considermapreset)
{
	UINT8 respawnlist[MAXPLAYERS];
	UINT8 i, j, numingame = 0, numjoiners = 0;
	UINT8 numhumans = 0, numbots = 0;

	// Maintain spectate wait timer
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
		{
			continue;
		}

		if (!players[i].spectator)
		{
			numingame++;

			if (players[i].bot)
			{
				numbots++;
			}
			else
			{
				numhumans++;
			}

			players[i].spectatewait = 0;
			players[i].spectatorReentry = 0;
			continue;
		}

		if ((players[i].pflags & PF_WANTSTOJOIN))
		{
			players[i].spectatewait++;
		}
		else
		{
			players[i].spectatewait = 0;
		}

		if (gamestate != GS_LEVEL || considermapreset == false)
		{
			players[i].spectatorReentry = 0;
		}
		else if (players[i].spectatorReentry > 0)
		{
			players[i].spectatorReentry--;
		}
	}

	// No one's allowed to join
	if (!cv_allowteamchange.value)
		return;

	// DON'T allow if you've hit the in-game player cap
	if (cv_maxplayers.value && numhumans >= cv_maxplayers.value)
		return;

	// Get the number of players in game, and the players to be de-spectated.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		if (!players[i].spectator)
		{
			// Allow if you're not in a level
			if (gamestate != GS_LEVEL)
				continue;

			// DON'T allow if anyone's exiting
			if (players[i].exiting)
				return;

			// Allow if the match hasn't started yet
			if (numingame < 2 || leveltime < starttime || mapreset)
				continue;

			// DON'T allow if the match is 20 seconds in
			if (leveltime > (starttime + 20*TICRATE))
				return;

			// DON'T allow if the race is at 2 laps
			if ((gametyperules & GTR_CIRCUIT) && players[i].laps >= 2)
				return;

			continue;
		}

		if (players[i].bot)
		{
			// Spectating bots are controlled by other mechanisms.
			continue;
		}

		if (!(players[i].pflags & PF_WANTSTOJOIN))
		{
			// This spectator does not want to join.
			continue;
		}

		if (netgame && numingame > 0 && players[i].spectatorReentry > 0)
		{
			// This person has their reentry cooldown active.
			continue;
		}

		respawnlist[numjoiners++] = i;
	}

	// Literally zero point in going any further if nobody is joining.
	if (!numjoiners)
		return;

	// Organize by spectate wait timer (if there's more than one to sort)
	if (cv_maxplayers.value && numjoiners > 1)
	{
		UINT8 oldrespawnlist[MAXPLAYERS];
		memcpy(oldrespawnlist, respawnlist, numjoiners);
		for (i = 0; i < numjoiners; i++)
		{
			UINT8 pos = 0;
			INT32 ispecwait = players[oldrespawnlist[i]].spectatewait;

			for (j = 0; j < numjoiners; j++)
			{
				INT32 jspecwait = players[oldrespawnlist[j]].spectatewait;
				if (j == i)
					continue;
				if (jspecwait > ispecwait)
					pos++;
				else if (jspecwait == ispecwait && j < i)
					pos++;
			}

			respawnlist[pos] = oldrespawnlist[i];
		}
	}

	const UINT8 previngame = numingame;
	INT16 removeBotID = MAXPLAYERS - 1;

	// Finally, we can de-spectate everyone!
	for (i = 0; i < numjoiners; i++)
	{
		// Hit the in-game player cap while adding people?
		if (cv_maxplayers.value && numingame >= cv_maxplayers.value)
		{
			if (numbots > 0)
			{
				// Find a bot to kill to make room
				while (removeBotID >= 0)
				{
					if (playeringame[removeBotID] && players[removeBotID].bot)
					{
						//CONS_Printf("bot %s kicked to make room on tic %d\n", player_names[removeBotID], leveltime);
						CL_RemovePlayer(removeBotID, KR_LEAVE);
						numbots--;
						numingame--;
						break;
					}

					removeBotID--;
				}

				if (removeBotID < 0)
				{
					break;
				}
			}
			else
			{
				break;
			}
		}

		//CONS_Printf("player %s is joining on tic %d\n", player_names[respawnlist[i]], leveltime);
		P_SpectatorJoinGame(&players[respawnlist[i]]);
		numhumans++;
		numingame++;
	}

	if (considermapreset == false)
		return;

	// Reset the match when 2P joins 1P, DUEL mode
	// Reset the match when 3P joins 1P and 2P, DUEL mode must be disabled
	extern consvar_t cv_debugnewchallenger;
	if (i > 0 && !mapreset && gamestate == GS_LEVEL && (previngame < 3 && numingame >= 2) && !cv_debugnewchallenger.value)
	{
		Music_Play("comeon"); // COME ON
		mapreset = 3*TICRATE; // Even though only the server uses this for game logic, set for everyone for HUD
	}
}

UINT8 K_GetInvincibilityItemFrame(void)
{
	return ((leveltime % (7*3)) / 3);
}

UINT8 K_GetOrbinautItemFrame(UINT8 count)
{
	return min(count - 1, 3);
}

boolean K_IsSPBInGame(void)
{
	// is there an SPB chasing anyone?
	if (spbplace != -1)
		return true;

	// do any players have an SPB in their item slot?
	UINT8 i;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (players[i].itemtype == KITEM_SPB)
			return true;
	}


	// spbplace is still -1 until a fired SPB finds a target, so look for an in-map SPB just in case
	mobj_t *mobj;
	for (mobj = trackercap; mobj; mobj = mobj->itnext)
	{
		if (mobj->type != MT_SPB)
			continue;

		return true;
	}

	return false;
}

void K_UpdateMobjItemOverlay(mobj_t *part, SINT8 itemType, UINT8 itemCount)
{
	switch (itemType)
	{
		case KITEM_ORBINAUT:
			part->sprite = SPR_ITMO;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetOrbinautItemFrame(itemCount);
			break;
		case KITEM_INVINCIBILITY:
			part->sprite = SPR_ITMI;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetInvincibilityItemFrame();
			break;
		case KITEM_SAD:
			part->sprite = SPR_ITEM;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE;
			break;
		default:
			part->sprite = SPR_ITEM;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|(itemType);
			break;
	}
}

tic_t K_TimeLimitForGametype(void)
{
	const tic_t gametypeDefault = gametypes[gametype]->timelimit * (60*TICRATE);

	if (!(gametyperules & GTR_TIMELIMIT))
	{
		return 0;
	}

	if (modeattacking)
	{
		return 0;
	}

	// Grand Prix
	if (!K_CanChangeRules(true))
	{
		if (battleprisons)
		{
			if (grandprixinfo.gp)
			{
				if (grandprixinfo.gamespeed == KARTSPEED_EASY)
					return 30*TICRATE;
			}
			return 20*TICRATE;
		}

		return gametypeDefault;
	}

	if (cv_timelimit.value != -1)
	{
		return cv_timelimit.value * TICRATE;
	}

	// No time limit for Break the Capsules FREE PLAY
	if (battleprisons)
	{
		return 0;
	}

	return gametypeDefault;
}

UINT32 K_PointLimitForGametype(void)
{
	const UINT32 gametypeDefault = gametypes[gametype]->pointlimit;
	const UINT32 battleRules = GTR_BUMPERS|GTR_CLOSERPLAYERS|GTR_PAPERITEMS;

	UINT32 ptsCap = gametypeDefault;

	if (!(gametyperules & GTR_POINTLIMIT))
	{
		return 0;
	}

	if (K_Cooperative())
	{
		return 0;
	}

	if (K_CanChangeRules(true) == true && cv_pointlimit.value != -1)
	{
		return cv_pointlimit.value;
	}

	if ((gametyperules & battleRules) == battleRules) // why isn't this just another GTR_??
	{
		INT32 i;

		// It's frustrating that this shitty for-loop needs to
		// be duplicated every time the players need to be
		// counted.
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (D_IsPlayerHumanAndGaming(i))
			{
				ptsCap += 3;
			}
		}

		if (ptsCap > 16)
		{
			ptsCap = 16;
		}
	}

	return ptsCap;
}

boolean K_Cooperative(void)
{
	if (battleprisons)
	{
		return true;
	}

	if (bossinfo.valid)
	{
		return true;
	}

	if (specialstageinfo.valid)
	{
		return true;
	}

	if (gametype == GT_TUTORIAL)
	{
		// Maybe this should be a rule. Eventually?
		return true;
	}

	return false;
}

// somewhat sensible check for HUD sounds in a post-bot-takeover world
boolean K_IsPlayingDisplayPlayer(player_t *player)
{
	return P_IsDisplayPlayer(player) && (!player->exiting);
}

boolean K_PlayerCanPunt(player_t *player)
{

	if (player->invincibilitytimer > 0)
	{
		return true;
	}

	if (player->flamedash > 0 && player->itemtype == KITEM_FLAMESHIELD)
	{
		return true;
	}

	if (player->growshrinktimer > 0)
	{
		return true;
	}

	if (player->tripwirePass >= TRIPWIRE_BLASTER && player->speed >= K_PlayerTripwireSpeedThreshold(player))
	{
		return true;
	}

	return false;
}

void K_MakeObjectReappear(mobj_t *mo)
{
	(!P_MobjWasRemoved(mo->punt_ref) ? mo->punt_ref : mo)->reappear = leveltime + (30*TICRATE);
}

boolean K_PlayerCanUseItem(player_t *player)
{
	return (player->mo->health > 0 && !player->spectator && !P_PlayerInPain(player) && !mapreset && leveltime > introtime);
}

//}
