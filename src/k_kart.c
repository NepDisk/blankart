// SONIC ROBO BLAST 2 KART ~ ZarroTsu
//-----------------------------------------------------------------------------
/// \file  k_kart.c
/// \brief SRB2kart general.
///        All of the SRB2kart-unique stuff.

#include "k_kart.h"
#include "d_netcmd.h"
#include "d_player.h"
#include "doomstat.h"
#include "info.h"
#include "k_battle.h"
#include "k_boss.h"
#include "k_pwrlv.h"
#include "k_color.h"
#include "doomdef.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_fixed.h"
#include "m_random.h"
#include "p_local.h"
#include "p_mobj.h"
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
#include "k_terrain.h"
#include "k_director.h"
#include "k_collide.h"
#include "k_follower.h"
#include "k_grandprix.h"

// SOME IMPORTANT VARIABLES DEFINED IN DOOMDEF.H:
// gamespeed is cc (0 for easy, 1 for normal, 2 for hard)
// franticitems is Frantic Mode items, bool
// encoremode is Encore Mode (duh), bool
// comeback is Battle Mode's karma comeback, also bool
// battlewanted is an array of the WANTED player nums, -1 for no player in that slot
// indirectitemcooldown is timer before anyone's allowed another Shrink/SPB
// mapreset is set when enough players fill an empty server

void K_TimerInit(void)
{
	UINT8 i;
	UINT8 numPlayers = 0;//, numspec = 0;
	
	starttime = introtime = 0;

	if (!bossinfo.boss)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
			{
				continue;
			}

			if (players[i].spectator == true)
			{
				//numspec++;
				continue;
			}

			numPlayers++;
		}

		introtime = (108) + 5; // 108 for rotation, + 5 for white fade
		starttime = 6*TICRATE + (3*TICRATE/4);
		
		if (gametyperules & GTR_FREEROAM)
		{
			introtime = 0;
			starttime = 0;
		}
	}

	// NOW you can try to spawn in the Battle capsules, if there's not enough players for a match
	K_BattleInit();

	timelimitintics = extratimeintics = secretextratime = 0;
	if ((gametyperules & GTR_TIMELIMIT) && !bossinfo.boss)
	{
		if (!K_CanChangeRules())
		{
			if (grandprixinfo.gp)
			{
				timelimitintics = (20*TICRATE);
			}
			else
			{
				timelimitintics = timelimits[gametype] * (60*TICRATE);
			}
		}
		else
#ifndef TESTOVERTIMEINFREEPLAY
			if (!battlecapsules)
#endif
		{
			timelimitintics = cv_timelimit.value * (60*TICRATE);
		}
	}
}

UINT32 K_GetPlayerDontDrawFlag(player_t *player)
{
	UINT32 flag = 0;

	if (player == &players[displayplayers[0]])
		flag = RF_DONTDRAWP1;
	else if (r_splitscreen >= 1 && player == &players[displayplayers[1]])
		flag = RF_DONTDRAWP2;
	else if (r_splitscreen >= 2 && player == &players[displayplayers[2]])
		flag = RF_DONTDRAWP3;
	else if (r_splitscreen >= 3 && player == &players[displayplayers[3]])
		flag = RF_DONTDRAWP4;

	return flag;
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

//{ SRB2kart Net Variables

void K_RegisterKartStuff(void)
{
	CV_RegisterVar(&cv_sneaker);
	CV_RegisterVar(&cv_rocketsneaker);
	CV_RegisterVar(&cv_invincibility);
	CV_RegisterVar(&cv_banana);
	CV_RegisterVar(&cv_eggmanmonitor);
	CV_RegisterVar(&cv_orbinaut);
	CV_RegisterVar(&cv_jawz);
	CV_RegisterVar(&cv_mine);
	CV_RegisterVar(&cv_landmine);
	CV_RegisterVar(&cv_droptarget);
	CV_RegisterVar(&cv_ballhog);
	CV_RegisterVar(&cv_selfpropelledbomb);
	CV_RegisterVar(&cv_grow);
	CV_RegisterVar(&cv_shrink);
	CV_RegisterVar(&cv_lightningshield);
	CV_RegisterVar(&cv_bubbleshield);
	CV_RegisterVar(&cv_flameshield);
	CV_RegisterVar(&cv_hyudoro);
	CV_RegisterVar(&cv_pogospring);
	CV_RegisterVar(&cv_superring);
	CV_RegisterVar(&cv_kitchensink);

	CV_RegisterVar(&cv_dualsneaker);
	CV_RegisterVar(&cv_triplesneaker);
	CV_RegisterVar(&cv_triplebanana);
	CV_RegisterVar(&cv_decabanana);
	CV_RegisterVar(&cv_tripleorbinaut);
	CV_RegisterVar(&cv_quadorbinaut);
	CV_RegisterVar(&cv_dualjawz);

	CV_RegisterVar(&cv_kartminimap);
	CV_RegisterVar(&cv_kartcheck);
	CV_RegisterVar(&cv_kartinvinsfx);
	CV_RegisterVar(&cv_kartspeed);
	CV_RegisterVar(&cv_kartbumpers);
	CV_RegisterVar(&cv_kartfrantic);
	CV_RegisterVar(&cv_kartcomeback);
	CV_RegisterVar(&cv_kartencore);
	CV_RegisterVar(&cv_kartvoterulechanges);
	CV_RegisterVar(&cv_kartspeedometer);
	CV_RegisterVar(&cv_kartvoices);
	CV_RegisterVar(&cv_kartbot);
	CV_RegisterVar(&cv_karteliminatelast);
	CV_RegisterVar(&cv_kartusepwrlv);
	CV_RegisterVar(&cv_votetime);

	CV_RegisterVar(&cv_kartdebugitem);
	CV_RegisterVar(&cv_kartdebugamount);
	CV_RegisterVar(&cv_kartallowgiveitem);
	CV_RegisterVar(&cv_kartdebugdistribution);
	CV_RegisterVar(&cv_kartdebughuddrop);
	CV_RegisterVar(&cv_kartdebugwaypoints);
	CV_RegisterVar(&cv_kartdebugbotpredict);

	CV_RegisterVar(&cv_kartdebugcheckpoint);
	CV_RegisterVar(&cv_kartdebugnodes);
	CV_RegisterVar(&cv_kartdebugcolorize);
	CV_RegisterVar(&cv_kartdebugdirector);
	
	CV_RegisterVar(&cv_stagetitle);	
	
	CV_RegisterVar(&cv_lessflicker);
	
	CV_RegisterVar(&cv_kartrings);
	
	CV_RegisterVar(&cv_newspeedometer);
	
	CV_RegisterVar(&cv_kartwalltransfer);
}

//}

boolean K_IsPlayerLosing(player_t *player)
{
	INT32 winningpos = 1;
	UINT8 i, pcount = 0;

	if (battlecapsules && numtargets == 0)
		return true; // Didn't even TRY?

	if (battlecapsules || bossinfo.boss)
		return (player->bumper <= 0); // anything short of DNF is COOL

	if (player->position == 1)
		return false;

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
	return ((13 + (3*value)) << FRACBITS) / 16;
}

//{ SRB2kart Roulette Code - Position Based

consvar_t *KartItemCVars[NUMKARTRESULTS-1] =
{
	&cv_sneaker,
	&cv_rocketsneaker,
	&cv_invincibility,
	&cv_banana,
	&cv_eggmanmonitor,
	&cv_orbinaut,
	&cv_jawz,
	&cv_mine,
	&cv_landmine,
	&cv_ballhog,
	&cv_selfpropelledbomb,
	&cv_grow,
	&cv_shrink,
	&cv_lightningshield,
	&cv_bubbleshield,
	&cv_flameshield,
	&cv_hyudoro,
	&cv_pogospring,
	&cv_superring,
	&cv_kitchensink,
	&cv_droptarget,
	&cv_dualsneaker,
	&cv_triplesneaker,
	&cv_triplebanana,
	&cv_decabanana,
	&cv_tripleorbinaut,
	&cv_quadorbinaut,
	&cv_dualjawz
};

#define NUMKARTODDS 	80

// Less ugly 2D arrays
static INT32 K_KartItemOddsRace[NUMKARTRESULTS-1][8] =
{
				//P-Odds	 0  1  2  3  4  5  6  7
			   /*Sneaker*/ { 0, 0, 2, 4, 6, 0, 0, 0 }, // Sneaker
		/*Rocket Sneaker*/ { 0, 0, 0, 0, 0, 2, 4, 6 }, // Rocket Sneaker
		 /*Invincibility*/ { 0, 0, 0, 0, 3, 4, 6, 9 }, // Invincibility
				/*Banana*/ { 2, 3, 1, 0, 0, 0, 0, 0 }, // Banana
		/*Eggman Monitor*/ { 1, 2, 0, 0, 0, 0, 0, 0 }, // Eggman Monitor
			  /*Orbinaut*/ { 5, 5, 2, 2, 0, 0, 0, 0 }, // Orbinaut
				  /*Jawz*/ { 0, 4, 2, 1, 0, 0, 0, 0 }, // Jawz
				  /*Mine*/ { 0, 3, 3, 1, 0, 0, 0, 0 }, // Mine
			 /*Land Mine*/ { 3, 0, 0, 0, 0, 0, 0, 0 }, // Land Mine
			   /*Ballhog*/ { 0, 0, 2, 2, 0, 0, 0, 0 }, // Ballhog
   /*Self-Propelled Bomb*/ { 0, 0, 0, 0, 0, 2, 4, 0 }, // Self-Propelled Bomb
				  /*Grow*/ { 0, 0, 0, 1, 2, 3, 0, 0 }, // Grow
				/*Shrink*/ { 0, 0, 0, 0, 0, 0, 2, 0 }, // Shrink
	  /*Lightning Shield*/ { 1, 0, 0, 0, 0, 0, 0, 0 }, // Lightning Shield
		 /*Bubble Shield*/ { 0, 1, 2, 1, 0, 0, 0, 0 }, // Bubble Shield
		  /*Flame Shield*/ { 0, 0, 0, 0, 0, 1, 3, 5 }, // Flame Shield
			   /*Hyudoro*/ { 3, 0, 0, 0, 0, 0, 0, 0 }, // Hyudoro
		   /*Pogo Spring*/ { 0, 0, 0, 0, 0, 0, 0, 0 }, // Pogo Spring
			/*Super Ring*/ { 2, 1, 1, 0, 0, 0, 0, 0 }, // Super Ring
		  /*Kitchen Sink*/ { 0, 0, 0, 0, 0, 0, 0, 0 }, // Kitchen Sink
		   /*Drop Target*/ { 3, 0, 0, 0, 0, 0, 0, 0 }, // Drop Target
			/*Sneaker x2*/ { 0, 0, 2, 2, 2, 0, 0, 0 }, // Sneaker x2
			/*Sneaker x3*/ { 0, 0, 0, 1, 6,10, 5, 0 }, // Sneaker x3
			 /*Banana x3*/ { 0, 1, 1, 0, 0, 0, 0, 0 }, // Banana x3
			/*Banana x10*/ { 0, 0, 0, 1, 0, 0, 0, 0 }, // Banana x10
		   /*Orbinaut x3*/ { 0, 0, 1, 0, 0, 0, 0, 0 }, // Orbinaut x3
		   /*Orbinaut x4*/ { 0, 0, 0, 2, 0, 0, 0, 0 }, // Orbinaut x4
			   /*Jawz x2*/ { 0, 0, 1, 2, 1, 0, 0, 0 }  // Jawz x2
};

static INT32 K_KartItemOddsBattle[NUMKARTRESULTS][2] =
{
				//P-Odds	 0  1
			   /*Sneaker*/ { 2, 1 }, // Sneaker
		/*Rocket Sneaker*/ { 0, 0 }, // Rocket Sneaker
		 /*Invincibility*/ { 4, 1 }, // Invincibility
				/*Banana*/ { 0, 0 }, // Banana
		/*Eggman Monitor*/ { 1, 0 }, // Eggman Monitor
			  /*Orbinaut*/ { 8, 0 }, // Orbinaut
				  /*Jawz*/ { 8, 1 }, // Jawz
				  /*Mine*/ { 6, 1 }, // Mine
			 /*Land Mine*/ { 2, 0 }, // Land Mine
			   /*Ballhog*/ { 2, 1 }, // Ballhog
   /*Self-Propelled Bomb*/ { 0, 0 }, // Self-Propelled Bomb
				  /*Grow*/ { 2, 1 }, // Grow
				/*Shrink*/ { 0, 0 }, // Shrink
	  /*Lightning Shield*/ { 4, 0 }, // Lightning Shield
		 /*Bubble Shield*/ { 1, 0 }, // Bubble Shield
		  /*Flame Shield*/ { 1, 0 }, // Flame Shield
			   /*Hyudoro*/ { 2, 0 }, // Hyudoro
		   /*Pogo Spring*/ { 3, 0 }, // Pogo Spring
			/*Super Ring*/ { 0, 0 }, // Super Ring
		  /*Kitchen Sink*/ { 0, 0 }, // Kitchen Sink
		   /*Drop Target*/ { 2, 0 }, // Drop Target
			/*Sneaker x2*/ { 0, 0 }, // Sneaker x2
			/*Sneaker x3*/ { 0, 1 }, // Sneaker x3
			 /*Banana x3*/ { 0, 0 }, // Banana x3
			/*Banana x10*/ { 1, 1 }, // Banana x10
		   /*Orbinaut x3*/ { 2, 0 }, // Orbinaut x3
		   /*Orbinaut x4*/ { 1, 1 }, // Orbinaut x4
			   /*Jawz x2*/ { 5, 1 }  // Jawz x2
};

#define DISTVAR (2048) // Magic number distance for use with item roulette tiers
#define SPBSTARTDIST (5*DISTVAR) // Distance when SPB is forced onto 2nd place
#define SPBFORCEDIST (15*DISTVAR) // Distance when SPB is forced onto 2nd place
#define ENDDIST (12*DISTVAR) // Distance when the game stops giving you bananas

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

/**	\brief	Item Roulette for Kart

	\param	player		player
	\param	getitem		what item we're looking for

	\return	void
*/
static void K_KartGetItemResult(player_t *player, SINT8 getitem)
{
	if (getitem == KITEM_SPB || getitem == KITEM_SHRINK) // Indirect items
		indirectitemcooldown = 20*TICRATE;

	player->botvars.itemdelay = TICRATE;
	player->botvars.itemconfirm = 0;

	switch (getitem)
	{
		// Special roulettes first, then the generic ones are handled by default
		case KRITEM_DUALSNEAKER: // Sneaker x2
			player->itemtype = KITEM_SNEAKER;
			player->itemamount = 2;
			break;
		case KRITEM_TRIPLESNEAKER: // Sneaker x3
			player->itemtype = KITEM_SNEAKER;
			player->itemamount = 3;
			break;
		case KRITEM_TRIPLEBANANA: // Banana x3
			player->itemtype = KITEM_BANANA;
			player->itemamount = 3;
			break;
		case KRITEM_TENFOLDBANANA: // Banana x10
			player->itemtype = KITEM_BANANA;
			player->itemamount = 10;
			break;
		case KRITEM_TRIPLEORBINAUT: // Orbinaut x3
			player->itemtype = KITEM_ORBINAUT;
			player->itemamount = 3;
			break;
		case KRITEM_QUADORBINAUT: // Orbinaut x4
			player->itemtype = KITEM_ORBINAUT;
			player->itemamount = 4;
			break;
		case KRITEM_DUALJAWZ: // Jawz x2
			player->itemtype = KITEM_JAWZ;
			player->itemamount = 2;
			break;
		default:
			if (getitem <= 0 || getitem >= NUMKARTRESULTS) // Sad (Fallback)
			{
				if (getitem != 0)
					CONS_Printf("ERROR: P_KartGetItemResult - Item roulette gave bad item (%d) :(\n", getitem);
				player->itemtype = KITEM_SAD;
			}
			else
				player->itemtype = getitem;
			player->itemamount = 1;
			break;
	}
}

fixed_t K_ItemOddsScale(UINT8 numPlayers, boolean spbrush)
{
	const UINT8 basePlayer = 8; // The player count we design most of the game around.
	UINT8 playerCount = (spbrush ? 2 : numPlayers);
	fixed_t playerScaling = 0;

	// Then, it multiplies it further if the player count isn't equal to basePlayer.
	// This is done to make low player count races more interesting and high player count rates more fair.
	// (If you're in SPB mode and in 2nd place, it acts like it's a 1v1, so the catch-up game is not weakened.)
	if (playerCount < basePlayer)
	{
		// Less than basePlayer: increase odds significantly.
		// 2P: x2.5
		playerScaling = (basePlayer - playerCount) * (FRACUNIT / 4);
	}
	else if (playerCount > basePlayer)
	{
		// More than basePlayer: reduce odds slightly.
		// 16P: x0.75
		playerScaling = (basePlayer - playerCount) * (FRACUNIT / 32);
	}

	return playerScaling;
}

UINT32 K_ScaleItemDistance(UINT32 distance, UINT8 numPlayers, boolean spbrush)
{
	if (mapobjectscale != FRACUNIT)
	{
		// Bring back to normal scale.
		distance = FixedDiv(distance * FRACUNIT, mapobjectscale) / FRACUNIT;
	}

	if (franticitems == true)
	{
		// Frantic items pretends everyone's farther apart, for crazier items.
		distance = (15 * distance) / 14;
	}

	if (numPlayers > 0)
	{
		// Items get crazier with the fewer players that you have.
		distance = FixedMul(
			distance * FRACUNIT,
			FRACUNIT + (K_ItemOddsScale(numPlayers, spbrush) / 2)
		) / FRACUNIT;
	}

	return distance;
}

/**	\brief	Item Roulette for Kart

	\param	player	player object passed from P_KartPlayerThink

	\return	void
*/

INT32 K_KartGetItemOdds(
	UINT8 pos, SINT8 item,
	UINT32 ourDist,
	fixed_t mashed,
	boolean spbrush, boolean bot, boolean rival)
{
	INT32 newodds;
	INT32 i;

	UINT8 pingame = 0, pexiting = 0;

	SINT8 first = -1, second = -1;
	UINT32 firstDist = UINT32_MAX;
	UINT32 secondToFirst = UINT32_MAX;

	boolean powerItem = false;
	boolean cooldownOnStart = false;
	boolean indirectItem = false;
	boolean notNearEnd = false;

	INT32 shieldtype = KSHIELD_NONE;

	I_Assert(item > KITEM_NONE); // too many off by one scenarioes.
	I_Assert(KartItemCVars[NUMKARTRESULTS-2] != NULL); // Make sure this exists

	if (!KartItemCVars[item-1]->value && !modeattacking)
		return 0;

	/*
	if (bot)
	{
		// TODO: Item use on bots should all be passed-in functions.
		// Instead of manually inserting these, it should return 0
		// for any items without an item use function supplied

		switch (item)
		{
			case KITEM_SNEAKER:
				break;
			default:
				return 0;
		}
	}
	*/
	(void)bot;

	if (gametype == GT_BATTLE)
	{
		I_Assert(pos < 2); // DO NOT allow positions past the bounds of the table
		newodds = K_KartItemOddsBattle[item-1][pos];
	}
	else
	{
		I_Assert(pos < 8); // Ditto
		newodds = K_KartItemOddsRace[item-1][pos];
	}

	// Base multiplication to ALL item odds to simulate fractional precision
	newodds *= 4;

	shieldtype = K_GetShieldFromItem(item);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (!(gametyperules & GTR_BUMPERS) || players[i].bumper)
			pingame++;

		if (players[i].exiting)
			pexiting++;

		if (shieldtype != KSHIELD_NONE && shieldtype == K_GetShieldFromItem(players[i].itemtype))
		{
			// Don't allow more than one of each shield type at a time
			return 0;
		}

		if (players[i].mo && gametype == GT_RACE)
		{
			if (players[i].position == 1 && first == -1)
				first = i;
			if (players[i].position == 2 && second == -1)
				second = i;
		}
	}

	if (first != -1 && second != -1) // calculate 2nd's distance from 1st, for SPB
	{
		firstDist = players[first].distancetofinish;

		if (mapobjectscale != FRACUNIT)
		{
			firstDist = FixedDiv(firstDist * FRACUNIT, mapobjectscale) / FRACUNIT;
		}

		secondToFirst = K_ScaleItemDistance(
			players[second].distancetofinish - players[first].distancetofinish,
			pingame, spbrush
		);
	}

	switch (item)
	{
		case KITEM_BANANA:
		case KITEM_EGGMAN:
			notNearEnd = true;
			break;
		case KITEM_SUPERRING:
			notNearEnd = true;
			
			if (ringsdisabled) // No rings rolled if rings are turned off.
			{
				newodds = 0;
			}
			
			break;
		case KITEM_ROCKETSNEAKER:
		case KITEM_JAWZ:
		case KITEM_LANDMINE:
		case KITEM_DROPTARGET:
		case KITEM_BALLHOG:
		case KITEM_HYUDORO:
		case KRITEM_TRIPLESNEAKER:
		case KRITEM_TRIPLEORBINAUT:
		case KRITEM_QUADORBINAUT:
		case KRITEM_DUALJAWZ:
			powerItem = true;
			break;
		case KRITEM_TRIPLEBANANA:
		case KRITEM_TENFOLDBANANA:
			powerItem = true;
			notNearEnd = true;
			break;
		case KITEM_INVINCIBILITY:
		case KITEM_MINE:
		case KITEM_GROW:
		case KITEM_BUBBLESHIELD:
		case KITEM_FLAMESHIELD:
			cooldownOnStart = true;
			powerItem = true;
			break;
		case KITEM_SPB:
			cooldownOnStart = true;
			indirectItem = true;
			notNearEnd = true;

			if (firstDist < ENDDIST) // No SPB near the end of the race
			{
				newodds = 0;
			}
			else
			{
				const INT32 distFromStart = max(0, (INT32)secondToFirst - SPBSTARTDIST);
				const INT32 distRange = SPBFORCEDIST - SPBSTARTDIST;
				const INT32 mulMax = 3;

				INT32 multiplier = (distFromStart * mulMax) / distRange;

				if (multiplier < 0)
					multiplier = 0;
				if (multiplier > mulMax)
					multiplier = mulMax;

				newodds *= multiplier;
			}
			break;
		case KITEM_SHRINK:
			cooldownOnStart = true;
			powerItem = true;
			indirectItem = true;
			notNearEnd = true;

			if (pingame-1 <= pexiting)
				newodds = 0;
			break;
		case KITEM_LIGHTNINGSHIELD:
			cooldownOnStart = true;
			powerItem = true;

			if (spbplace != -1)
				newodds = 0;
			break;
		default:
			break;
	}

	if (newodds == 0)
	{
		// Nothing else we want to do with odds matters at this point :p
		return newodds;
	}

	if ((indirectItem == true) && (indirectitemcooldown > 0))
	{
		// Too many items that act indirectly in a match can feel kind of bad.
		newodds = 0;
	}
	else if ((cooldownOnStart == true) && (leveltime < (30*TICRATE)+starttime))
	{
		// This item should not appear at the beginning of a race. (Usually really powerful crowd-breaking items)
		newodds = 0;
	}
	else if ((notNearEnd == true) && (ourDist < ENDDIST))
	{
		// This item should not appear at the end of a race. (Usually trap items that lose their effectiveness)
		newodds = 0;
	}
	else if (powerItem == true)
	{
		// This item is a "power item". This activates "frantic item" toggle related functionality.
		fixed_t fracOdds = newodds * FRACUNIT;

		if (franticitems == true)
		{
			// First, power items multiply their odds by 2 if frantic items are on; easy-peasy.
			fracOdds *= 2;
		}

		if (rival == true)
		{
			// The Rival bot gets frantic-like items, also :p
			fracOdds *= 2;
		}

		fracOdds = FixedMul(fracOdds, FRACUNIT + K_ItemOddsScale(pingame, spbrush));

		if (mashed > 0)
		{
			// Lastly, it *divides* it based on your mashed value, so that power items are less likely when you mash.
			fracOdds = FixedDiv(fracOdds, FRACUNIT + mashed);
		}

		newodds = fracOdds / FRACUNIT;
	}

	return newodds;
}

//{ SRB2kart Roulette Code - Distance Based, yes waypoints

UINT8 K_FindUseodds(player_t *player, fixed_t mashed, UINT32 pdis, UINT8 bestbumper, boolean spbrush)
{
	UINT8 i;
	UINT8 useodds = 0;
	UINT8 disttable[14];
	UINT8 distlen = 0;
	boolean oddsvalid[8];

	// Unused now, oops :V
	(void)bestbumper;

	for (i = 0; i < 8; i++)
	{
		UINT8 j;
		boolean available = false;

		if (gametype == GT_BATTLE && i > 1)
		{
			oddsvalid[i] = false;
			break;
		}

		for (j = 1; j < NUMKARTRESULTS; j++)
		{
			if (K_KartGetItemOdds(
					i, j,
					player->distancetofinish,
					mashed,
					spbrush, player->bot, (player->bot && player->botvars.rival)
				) > 0)
			{
				available = true;
				break;
			}
		}

		oddsvalid[i] = available;
	}

#define SETUPDISTTABLE(odds, num) \
	if (oddsvalid[odds]) \
		for (i = num; i; --i) \
			disttable[distlen++] = odds;

	if (gametype == GT_BATTLE) // Battle Mode
	{
		if (player->roulettetype == 1 && oddsvalid[1] == true)
		{
			// 1 is the extreme odds of player-controlled "Karma" items
			useodds = 1;
		}
		else
		{
			useodds = 0;

			if (oddsvalid[0] == false && oddsvalid[1] == true)
			{
				// try to use karma odds as a fallback
				useodds = 1;
			}
		}
	}
	else
	{
		SETUPDISTTABLE(0,1);
		SETUPDISTTABLE(1,1);
		SETUPDISTTABLE(2,1);
		SETUPDISTTABLE(3,2);
		SETUPDISTTABLE(4,2);
		SETUPDISTTABLE(5,3);
		SETUPDISTTABLE(6,3);
		SETUPDISTTABLE(7,1);

		if (pdis == 0)
			useodds = disttable[0];
		else if (pdis > DISTVAR * ((12 * distlen) / 14))
			useodds = disttable[distlen-1];
		else
		{
			for (i = 1; i < 13; i++)
			{
				if (pdis <= DISTVAR * ((i * distlen) / 14))
				{
					useodds = disttable[((i * distlen) / 14)];
					break;
				}
			}
		}
	}

#undef SETUPDISTTABLE

	return useodds;
}

static void K_KartItemRoulette(player_t *player, ticcmd_t *cmd)
{
	INT32 i;
	UINT8 pingame = 0;
	UINT8 roulettestop;
	UINT32 pdis = 0;
	UINT8 useodds = 0;
	INT32 spawnchance[NUMKARTRESULTS];
	INT32 totalspawnchance = 0;
	UINT8 bestbumper = 0;
	fixed_t mashed = 0;
	boolean dontforcespb = false;
	boolean spbrush = false;

	// This makes the roulette cycle through items - if this is 0, you shouldn't be here.
	if (!player->itemroulette)
		return;
	player->itemroulette++;

	// Gotta check how many players are active at this moment.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		pingame++;
		if (players[i].exiting)
			dontforcespb = true;
		if (players[i].bumper > bestbumper)
			bestbumper = players[i].bumper;
	}

	// No forced SPB in 1v1s, it has to be randomly rolled
	if (pingame <= 2)
		dontforcespb = true;

	// This makes the roulette produce the random noises.
	if ((player->itemroulette % 3) == 1 && P_IsDisplayPlayer(player) && !demo.freecam)
	{
#define PLAYROULETTESND S_StartSound(NULL, sfx_itrol1 + ((player->itemroulette / 3) % 8))
		for (i = 0; i <= r_splitscreen; i++)
		{
			if (player == &players[displayplayers[i]] && players[displayplayers[i]].itemroulette)
				PLAYROULETTESND;
		}
#undef PLAYROULETTESND
	}

	roulettestop = TICRATE + (3*(pingame - player->position));

	// If the roulette finishes or the player presses BT_ATTACK, stop the roulette and calculate the item.
	// I'm returning via the exact opposite, however, to forgo having another bracket embed. Same result either way, I think.
	// Finally, if you get past this check, now you can actually start calculating what item you get.
	if ((cmd->buttons & BT_ATTACK) && (player->itemroulette >= roulettestop)
		&& !(player->pflags & (PF_ITEMOUT|PF_EGGMANOUT|PF_USERINGS)))
	{
		// Mashing reduces your chances for the good items
		mashed = FixedDiv((player->itemroulette)*FRACUNIT, ((TICRATE*3)+roulettestop)*FRACUNIT) - FRACUNIT;
	}
	else if (!(player->itemroulette >= (TICRATE*3)))
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator
			&& players[i].position == 1)
		{
			// This player is first! Yay!

			if (player->distancetofinish <= players[i].distancetofinish)
			{
				// Guess you're in first / tied for first?
				pdis = 0;
			}
			else
			{
				// Subtract 1st's distance from your distance, to get your distance from 1st!
				pdis = player->distancetofinish - players[i].distancetofinish;
			}
			break;
		}
	}

	if (spbplace != -1 && player->position == spbplace+1)
	{
		// SPB Rush Mode: It's 2nd place's job to catch-up items and make 1st place's job hell
		pdis = (3 * pdis) / 2;
		spbrush = true;
	}

	pdis = K_ScaleItemDistance(pdis, pingame, spbrush);

	if (player->bot && player->botvars.rival)
	{
		// Rival has better odds :)
		pdis = (15 * pdis) / 14;
	}

	// SPECIAL CASE No. 1:
	// Fake Eggman items
	if (player->roulettetype == 2)
	{
		player->eggmanexplode = 4*TICRATE;
		//player->karthud[khud_itemblink] = TICRATE;
		//player->karthud[khud_itemblinkmode] = 1;
		player->itemroulette = 0;
		player->roulettetype = 0;
		if (P_IsDisplayPlayer(player) && !demo.freecam)
			S_StartSound(NULL, sfx_itrole);
		return;
	}

	// SPECIAL CASE No. 2:
	// Give a debug item instead if specified
	if (cv_kartdebugitem.value != 0 && !modeattacking)
	{
		K_KartGetItemResult(player, cv_kartdebugitem.value);
		player->itemamount = cv_kartdebugamount.value;
		player->karthud[khud_itemblink] = TICRATE;
		player->karthud[khud_itemblinkmode] = 2;
		player->itemroulette = 0;
		player->roulettetype = 0;
		if (P_IsDisplayPlayer(player) && !demo.freecam)
			S_StartSound(NULL, sfx_dbgsal);
		return;
	}

	// SPECIAL CASE No. 3:
	// Record Attack / alone mashing behavior
	if (modeattacking || pingame == 1)
	{
		if (gametype == GT_RACE)
		{
			if (mashed && (modeattacking || (cv_superring.value && !ringsdisabled))) // ANY mashed value? You get rings.
			{
				K_KartGetItemResult(player, KITEM_SUPERRING);
				player->karthud[khud_itemblinkmode] = 1;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
			}
			else
			{
				if (modeattacking || cv_sneaker.value) // Waited patiently? You get a sneaker!
					K_KartGetItemResult(player, KITEM_SNEAKER);
				else  // Default to sad if nothing's enabled...
					K_KartGetItemResult(player, KITEM_SAD);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
		}
		else if (gametype == GT_BATTLE)
		{
			if (mashed && (modeattacking || bossinfo.boss || cv_banana.value)) // ANY mashed value? You get a banana.
			{
				K_KartGetItemResult(player, KITEM_BANANA);
				player->karthud[khud_itemblinkmode] = 1;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
			}
			else if (bossinfo.boss)
			{
				K_KartGetItemResult(player, KITEM_ORBINAUT);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
			else
			{
				if (modeattacking || cv_tripleorbinaut.value) // Waited patiently? You get Orbinaut x3!
					K_KartGetItemResult(player, KRITEM_TRIPLEORBINAUT);
				else  // Default to sad if nothing's enabled...
					K_KartGetItemResult(player, KITEM_SAD);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
		}

		player->karthud[khud_itemblink] = TICRATE;
		player->itemroulette = 0;
		player->roulettetype = 0;
		return;
	}

	// SPECIAL CASE No. 4:
	// Being in ring debt occasionally forces Super Ring on you if you mashed
	if (!(gametyperules & GTR_SPHERES || ringsdisabled) && mashed && player->rings < 0 && cv_superring.value)
	{
		INT32 debtamount = min(20, abs(player->rings));
		if (P_RandomChance((debtamount*FRACUNIT)/20))
		{
			K_KartGetItemResult(player, KITEM_SUPERRING);
			player->karthud[khud_itemblink] = TICRATE;
			player->karthud[khud_itemblinkmode] = 1;
			player->itemroulette = 0;
			player->roulettetype = 0;
			if (P_IsDisplayPlayer(player))
				S_StartSound(NULL, sfx_itrolm);
			return;
		}
	}

	// SPECIAL CASE No. 5:
	// Force SPB onto 2nd if they get too far behind
	if ((gametyperules & GTR_CIRCUIT) && player->position == 2 && pdis > SPBFORCEDIST
		&& spbplace == -1 && !indirectitemcooldown && !dontforcespb
		&& cv_selfpropelledbomb.value)
	{
		K_KartGetItemResult(player, KITEM_SPB);
		player->karthud[khud_itemblink] = TICRATE;
		player->karthud[khud_itemblinkmode] = 2;
		player->itemroulette = 0;
		player->roulettetype = 0;
		if (P_IsDisplayPlayer(player))
			S_StartSound(NULL, sfx_itrolk);
		return;
	}

	// NOW that we're done with all of those specialized cases, we can move onto the REAL item roulette tables.
	// Initializes existing spawnchance values
	for (i = 0; i < NUMKARTRESULTS; i++)
		spawnchance[i] = 0;

	// Split into another function for a debug function below
	useodds = K_FindUseodds(player, mashed, pdis, bestbumper, spbrush);

	for (i = 1; i < NUMKARTRESULTS; i++)
	{
		spawnchance[i] = (totalspawnchance += K_KartGetItemOdds(
			useodds, i,
			player->distancetofinish,
			mashed,
			spbrush, player->bot, (player->bot && player->botvars.rival))
		);
	}

	// Award the player whatever power is rolled
	if (totalspawnchance > 0)
	{
		totalspawnchance = P_RandomKey(totalspawnchance);
		for (i = 0; i < NUMKARTRESULTS && spawnchance[i] <= totalspawnchance; i++);

		K_KartGetItemResult(player, i);
	}
	else
	{
		player->itemtype = KITEM_SAD;
		player->itemamount = 1;
	}

	if (P_IsDisplayPlayer(player) && !demo.freecam)
		S_StartSound(NULL, ((player->roulettetype == 1) ? sfx_itrolk : (mashed ? sfx_itrolm : sfx_itrolf)));

	player->karthud[khud_itemblink] = TICRATE;
	player->karthud[khud_itemblinkmode] = ((player->roulettetype == 1) ? 2 : (mashed ? 1 : 0));

	player->itemroulette = 0; // Since we're done, clear the roulette number
	player->roulettetype = 0; // This too
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
	else
	{
		// Applies rubberbanding, to prevent rubberbanding bots
		// from causing super crazy bumps.
		fixed_t spd = K_GetKartSpeed(mobj->player, false, true);

		weight = (mobj->player->kartweight) * FRACUNIT;

		if (mobj->player->speed > spd)
			weight += (mobj->player->speed - spd) / 8;

		if (mobj->player->itemtype == KITEM_BUBBLESHIELD)
			weight += 9*FRACUNIT;
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
		case MT_KART_LEFTOVER:
			weight = 5*FRACUNIT/2;

			if (mobj->extravalue1 > 0)
			{
				weight = mobj->extravalue1 * (FRACUNIT >> 1);
			}
			break;
		case MT_BUBBLESHIELD:
			weight = K_PlayerWeight(mobj->target, against);
			break;
		case MT_FALLINGROCK:
			if (against->player)
			{
				if (against->player->invincibilitytimer || against->player->growshrinktimer > 0)
					weight = 0;
				else
					weight = K_PlayerWeight(against, NULL);
			}
			break;
		case MT_ORBINAUT:
		case MT_ORBINAUT_SHIELD:
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
		case MT_DROPTARGET:
		case MT_DROPTARGET_SHIELD:
			if (against->player)
				weight = K_PlayerWeight(against, NULL);
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
	else if (mobj1->type == MT_DROPTARGET || mobj1->type == MT_DROPTARGET_SHIELD) // no need to check the other way around
	{
		// Sound handled in K_DropTargetCollide
		// S_StartSound(mobj2, sfx_s258);
		fx->colorized = true;
		fx->color = mobj1->color;
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

	if (player->spinouttimer)
	{
		player->wipeoutslow = wipeoutslowtime+1;
		player->spinouttimer = max(wipeoutslowtime+1, player->spinouttimer);
		//player->spinouttype = KSPIN_WIPEOUT; // Enforce type
	}
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

	if ((mobj1->player && mobj1->player->respawn)
		|| (mobj2->player && mobj2->player->respawn))
		return false;

	if (mobj1->type != MT_DROPTARGET && mobj1->type != MT_DROPTARGET_SHIELD)
	{ // Don't bump if you're flashing
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
	}
	
	if ((mobj1->player && mobj1->player->squishedtimer > 0)
		|| (mobj2->player && mobj2->player->squishedtimer > 0))
		return false;
	

	// Don't bump if you've recently bumped
	if (mobj1->player && mobj1->player->justbumped)
	{
		mobj1->player->justbumped = bumptime;
		return false;
	}

	if (mobj2->player && mobj2->player->justbumped)
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
	fixed_t distx, disty, dist;
	fixed_t force;

	if ((!bounceMobj || P_MobjWasRemoved(bounceMobj))
	|| (!solidMobj || P_MobjWasRemoved(solidMobj)))
	{
		return false;
	}

	// Don't bump when you're being reborn
	if (bounceMobj->player && bounceMobj->player->playerstate != PST_LIVE)
		return false;

	if (bounceMobj->player && bounceMobj->player->respawn)
		return false;

	// Don't bump if you've recently bumped
	if (bounceMobj->player && bounceMobj->player->justbumped)
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

	// Multiply by force
	distx = FixedMul(force, distx);
	disty = FixedMul(force, disty);
	dist = FixedHypot(distx, disty);

	{
		// Normalize to the desired push value.
		fixed_t normalisedx = FixedDiv(distx, dist);
		fixed_t normalisedy = FixedDiv(disty, dist);
		fixed_t bounceSpeed;

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
	terrain_t *terrain = player->mo->terrain;
	fixed_t offroadstrength = 0;

	// TODO: Make this use actual special touch code.
	if (terrain != NULL && terrain->offroad > 0)
	{
		offroadstrength = (terrain->offroad << FRACBITS);
	}
	else
	{
		offroadstrength = K_CheckOffroadCollide(player->mo);
	}

	// If you are in offroad, a timer starts.
	if (offroadstrength)
	{
		if (player->offroad < offroadstrength)
			player->offroad += offroadstrength / TICRATE;

		if (player->offroad > offroadstrength)
			player->offroad = offroadstrength;
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

void K_SpawnNormalSpeedLines(player_t *player)
{
	mobj_t *fast = P_SpawnMobj(player->mo->x + (P_RandomRange(-36,36) * player->mo->scale),
		player->mo->y + (P_RandomRange(-36,36) * player->mo->scale),
		player->mo->z + (player->mo->height/2) + (P_RandomRange(-20,20) * player->mo->scale),
		MT_FASTLINE);

	P_SetTarget(&fast->target, player->mo);
	fast->angle = K_MomentumAngle(player->mo);
	fast->momx = 3*player->mo->momx/4;
	fast->momy = 3*player->mo->momy/4;
	fast->momz = 3*P_GetMobjZMovement(player->mo)/4;

	K_MatchGenericExtraFlags(fast, player->mo);

	if (player->tripwireLeniency)
	{
		fast->destscale = fast->destscale * 2;
		P_SetScale(fast, 3*fast->scale/2);
	}

	if (player->tripwireLeniency)
	{
		// Make it pink+blue+big when you can go through tripwire
		fast->color = (leveltime & 1) ? SKINCOLOR_LILAC : SKINCOLOR_JAWZ;
		fast->colorized = true;
		fast->renderflags |= RF_ADD;
	}
	else if (player->pflags & PF_AIRFAILSAFE)
	{
		fast->color = SKINCOLOR_WHITE;
		fast->colorized = true;
	}
}

void K_SpawnInvincibilitySpeedLines(mobj_t *mo)
{
	mobj_t *fast = P_SpawnMobjFromMobj(mo,
		P_RandomRange(-48, 48) * FRACUNIT,
		P_RandomRange(-48, 48) * FRACUNIT,
		P_RandomRange(0, 64) * FRACUNIT,
		MT_FASTLINE);
	P_SetMobjState(fast, S_KARTINVLINES1);

	P_SetTarget(&fast->target, mo);
	fast->angle = K_MomentumAngle(mo);

	fast->momx = 3*mo->momx/4;
	fast->momy = 3*mo->momy/4;
	fast->momz = 3*P_GetMobjZMovement(mo)/4;

	K_MatchGenericExtraFlags(fast, mo);

	fast->color = mo->color;
	fast->colorized = true;

	if (mo->player->invincibilitytimer < 10*TICRATE)
		fast->destscale = 6*((mo->player->invincibilitytimer/TICRATE)*FRACUNIT)/8;
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

static SINT8 K_GlanceAtPlayers(player_t *glancePlayer)
{
	const fixed_t maxdistance = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	const angle_t blindSpotSize = ANG10; // ANG5
	UINT8 i;
	SINT8 glanceDir = 0;
	SINT8 lastValidGlance = 0;

	// See if there's any players coming up behind us.
	// If so, your character will glance at 'em.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *p;
		angle_t back;
		angle_t diff;
		fixed_t distance;
		SINT8 dir = -1;

		if (!playeringame[i])
		{
			// Invalid player
			continue;
		}

		p = &players[i];

		if (p == glancePlayer)
		{
			// FOOL! Don't glance at yerself!
			continue;
		}

		if (!p->mo || P_MobjWasRemoved(p->mo))
		{
			// Invalid mobj
			continue;
		}

		if (p->spectator || p->hyudorotimer > 0)
		{
			// Not playing / invisible
			continue;
		}

		distance = R_PointToDist2(glancePlayer->mo->x, glancePlayer->mo->y, p->mo->x, p->mo->y);

		if (distance > maxdistance)
		{
			continue;
		}

		back = glancePlayer->mo->angle + ANGLE_180;
		diff = R_PointToAngle2(glancePlayer->mo->x, glancePlayer->mo->y, p->mo->x, p->mo->y) - back;

		if (diff > ANGLE_180)
		{
			diff = InvAngle(diff);
			dir = -dir;
		}

		if (diff > ANGLE_90)
		{
			// Not behind the player
			continue;
		}

		if (diff < blindSpotSize)
		{
			// Small blindspot directly behind your back, gives the impression of smoothly turning.
			continue;
		}

		if (P_CheckSight(glancePlayer->mo, p->mo) == true)
		{
			// Not blocked by a wall, we can glance at 'em!
			// Adds, so that if there's more targets on one of your sides, it'll glance on that side.
			glanceDir += dir;

			// That poses a limitation if there's an equal number of targets on both sides...
			// In that case, we'll pick the last chosen glance direction.
			lastValidGlance = dir;
		}
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

/**	\brief	Calculates the respawn timer and drop-boosting

	\param	player	player object passed from K_KartPlayerThink

	\return	void
*/
void K_RespawnChecker(player_t *player)
{
	ticcmd_t *cmd = &player->cmd;

	if (player->spectator)
		return;

	if (player->respawn > 1)
	{
		player->respawn--;
		player->mo->momz = 0;
		player->flashing = 2;
		player->nocontrol = 2;
		if (leveltime % 8 == 0)
		{
			INT32 i;
			if (!mapreset)
				S_StartSound(player->mo, sfx_s3kcas);

			for (i = 0; i < 8; i++)
			{
				mobj_t *mo;
				angle_t newangle;
				fixed_t newx, newy, newz;

				newangle = FixedAngle(((360/8)*i)*FRACUNIT);
				newx = player->mo->x + P_ReturnThrustX(player->mo, newangle, 31<<FRACBITS); // does NOT use scale, since this effect doesn't scale properly
				newy = player->mo->y + P_ReturnThrustY(player->mo, newangle, 31<<FRACBITS);
				if (player->mo->eflags & MFE_VERTICALFLIP)
					newz = player->mo->z + player->mo->height;
				else
					newz = player->mo->z;

				mo = P_SpawnMobj(newx, newy, newz, MT_DEZLASER);
				if (mo)
				{
					if (player->mo->eflags & MFE_VERTICALFLIP)
						mo->eflags |= MFE_VERTICALFLIP;
					P_SetTarget(&mo->target, player->mo);
					mo->angle = newangle+ANGLE_90;
					mo->momz = (8<<FRACBITS) * P_MobjFlip(player->mo);
					P_SetScale(mo, (mo->destscale = FRACUNIT));
				}
			}
		}
	}
	else if (player->respawn == 1)
	{
		if (player->growshrinktimer < 0)
		{
			player->mo->scalespeed = mapobjectscale/TICRATE;
			player->mo->destscale = (6*mapobjectscale)/8;
			if (K_PlayerShrinkCheat(player) && !modeattacking && !player->bot)
				player->mo->destscale = (6*player->mo->destscale)/8;
		}

		if (!P_IsObjectOnGround(player->mo) && !mapreset)
		{
			player->flashing = K_GetKartFlashing(player);

			// Sal: The old behavior was stupid and prone to accidental usage.
			// Let's rip off Mania instead, and turn this into a Drop Dash!

			if (cmd->buttons & BT_ACCELERATE)
				player->dropdash++;
			else
				player->dropdash = 0;

			if (player->dropdash == TICRATE/4)
				S_StartSound(player->mo, sfx_ddash);

			if ((player->dropdash >= TICRATE/4)
				&& (player->dropdash & 1))
				player->mo->colorized = true;
			else
				player->mo->colorized = false;
		}
		else
		{
			if ((cmd->buttons & BT_ACCELERATE) && (player->dropdash >= TICRATE/4))
			{
				S_StartSound(player->mo, sfx_s23c);
				player->startboost = 50;
				K_SpawnDashDustRelease(player);
			}
			player->mo->colorized = false;
			player->dropdash = 0;
			player->respawn = 0;
		}
	}
}

/**	\brief Handles the state changing for moving players, moved here to eliminate duplicate code

	\param	player	player data

	\return	void
*/
void K_KartMoveAnimation(player_t *player)
{
	const fixed_t fastspeed = (K_GetKartSpeed(player, false, true) * 17) / 20; // 85%
	const fixed_t speedthreshold = player->mo->scale / 8;

	const boolean onground = P_IsObjectOnGround(player->mo);

	UINT16 buttons = K_GetKartButtons(player);
	const boolean spinningwheels = (player->speed > 0);
	const boolean lookback = ((buttons & BT_LOOKBACK) == BT_LOOKBACK);

	SINT8 turndir = 0;
	SINT8 destGlanceDir = 0;
	SINT8 drift = player->drift;

	if (!lookback)
	{
		player->pflags &= ~PF_GAINAX;

		if (player->cmd.turning < 0)
		{
			turndir = -1;
		}
		else if (player->cmd.turning > 0)
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
		destGlanceDir = K_GlanceAtPlayers(player);

		if (lookback == true)
		{
			statenum_t gainaxstate = S_GAINAX_TINY;
			if (destGlanceDir == 0)
			{
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
		else if (player->cmd.forwardmove < 0 && destGlanceDir == 0)
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
		// Not glancing
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

void K_PlayAttackTaunt(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons
	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kattk1+pick);

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayBoostTaunt(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons
	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kbost1+pick);

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayOvertakeSound(mobj_t *source)
{
	boolean tasteful = (!source->player || !source->player->karthud[khud_voices]);

	if (!(gametype == GT_RACE)) // Only in race
		return;

	// 4 seconds from before race begins, 10 seconds afterwards
	if (leveltime < starttime+(10*TICRATE))
		return;

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kslow);

	if (!tasteful)
		return;

	K_RegularVoiceTimers(source->player);
}

void K_PlayPainSound(mobj_t *source, mobj_t *other)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons

	sfxenum_t sfx_id = ((skin_t *)source->skin)->soundsid[S_sfx[sfx_khurt1 + pick].skinsound];
	boolean alwaysHear = false;

	if (other != NULL && P_MobjWasRemoved(other) == false && other->player != NULL)
	{
		alwaysHear = P_IsDisplayPlayer(other->player);
	}

	if (cv_kartvoices.value)
	{
		S_StartSound(alwaysHear ? NULL : source, sfx_id);
	}

	K_RegularVoiceTimers(source->player);
}

void K_PlayHitEmSound(mobj_t *source, mobj_t *other)
{
	sfxenum_t sfx_id = ((skin_t *)source->skin)->soundsid[S_sfx[sfx_khitem].skinsound];
	boolean alwaysHear = false;

	if (other != NULL && P_MobjWasRemoved(other) == false && other->player != NULL)
	{
		alwaysHear = P_IsDisplayPlayer(other->player);
	}

	if (cv_kartvoices.value)
	{
		S_StartSound(alwaysHear ? NULL : source, sfx_id);
	}

	K_RegularVoiceTimers(source->player);
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

	if (attacker->player->follower != NULL)
	{
		const follower_t *fl = &followers[attacker->player->followerskin];
		attacker->player->follower->movecount = fl->hitconfirmtime; // movecount is used to play the hitconfirm animation for followers.
	}
}

void K_PlayPowerGloatSound(mobj_t *source)
{
	if (cv_kartvoices.value)
	{
		S_StartSound(source, sfx_kgloat);
	}

	K_RegularVoiceTimers(source->player);
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
	angle_t dangle = player->mo->angle - K_MomentumAngle(player->mo);

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

boolean K_ApplyOffroad(player_t *player)
{
	if (player->invincibilitytimer || player->hyudorotimer || player->sneakertimer)
		return false;
	return true;
}

boolean K_SlopeResistance(player_t *player)
{
	if (player->invincibilitytimer || player->sneakertimer || player->flamedash)
		return true;
	return false;
}

fixed_t K_PlayerTripwireSpeedThreshold(player_t *player)
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

tripwirepass_t K_TripwirePassConditions(player_t *player)
{
	if (
			player->invincibilitytimer ||
			player->sneakertimer
		)
		return TRIPWIRE_BLASTER;

	if (
			player->flamedash ||
			((player->speed > K_PlayerTripwireSpeedThreshold(player)) && player->tripwireReboundDelay == 0)
	)
		return TRIPWIRE_BOOST;

	if (
			player->growshrinktimer > 0 ||
			player->hyudorotimer
		)
		return TRIPWIRE_IGNORE;

	return TRIPWIRE_NONE;
}

boolean K_TripwirePass(player_t *player)
{
	return (player->tripwirePass != TRIPWIRE_NONE);
}

boolean K_WaterRun(player_t *player)
{
	if (player->waterrun)
		return true;
	return false;
}

static fixed_t K_FlameShieldDashVar(INT32 val)
{
	// 1 second = 75% + 50% top speed
	return (3*FRACUNIT/4) + (((val * FRACUNIT) / TICRATE) / 2);
}

// sets boostpower, speedboost and accelboost to whatever we need it to be
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
	if (K_ApplyOffroad(player) && player->offroad >= 0)
		boostpower = FixedDiv(boostpower, player->offroad + FRACUNIT);

	if (player->bananadrag > TICRATE)
		boostpower = (4*boostpower)/5;
	
	// Banana drag/offroad dust
	if (boostpower < FRACUNIT
		&& player->mo && P_IsObjectOnGround(player->mo)
		&& player->speed > 0
		&& !player->spectator)
	{
		K_SpawnWipeoutTrail(player->mo, true);
		if (leveltime % 6 == 0)
			S_StartSound(player->mo, sfx_cdfm70);
	}

#define ADDBOOST(s,a) { \
	speedboost = max(speedboost,s); \
	accelboost = max(accelboost,a); \
}

	if (player->sneakertimer) // Sneaker
	{
		fixed_t sneakerspeedboost = 0;
		switch (gamespeed)
		{
			case 0:
				sneakerspeedboost = max(speedboost, 53740+768);
				break;
			case 2:
				sneakerspeedboost = max(speedboost, 17294+768);
				break;
			default:
				sneakerspeedboost = max(speedboost, 32768);
				break;
		}
		ADDBOOST(sneakerspeedboost, 8*FRACUNIT); // + ???% top speed, + 800% acceleration, +50% handlin
		
	}

	if (player->invincibilitytimer) // Invincibility
	{
		ADDBOOST(3*FRACUNIT/8, 3*FRACUNIT); // + 37.5% top speed, + 300% acceleration, +25% handling
	}

	if (player->growshrinktimer > 0) // Grow
	{
		ADDBOOST(FRACUNIT/5, 0); // + 20% top speed, + 0% acceleration, +25% handling
	}

	if (player->flamedash) // Flame Shield dash
	{
		fixed_t dash = K_FlameShieldDashVar(player->flamedash);
		ADDBOOST( dash,	3*FRACUNIT); // + infinite top speed // + 300% acceleration
	}

	if (player->startboost) // Startup Boost
	{
		ADDBOOST(FRACUNIT/4, 6*FRACUNIT); // + 25% top speed, + 300% acceleration, +25% handling
	}

	if (player->driftboost) // Drift Boost
	{
		ADDBOOST(FRACUNIT/4, 4*FRACUNIT); // + 25% top speed, + 400% acceleration, +0% handling
	}

	if (player->ringboost) // Ring Boost
	{
		// Make rings additive so they aren't useless with other boosts
		speedboost += FRACUNIT/6; // 15%
		accelboost = max(4*FRACUNIT, accelboost); // 400%
	}

	player->boostpower = boostpower;

	// value smoothing
	if (speedboost > player->speedboost)
	{
		player->speedboost = speedboost;
	}
	else
	{
		player->speedboost += (speedboost - player->speedboost) / (TICRATE/2);
	}

	player->accelboost = accelboost;
}

// Returns value based on being Grown or Shrunk otherwise returns FRACUNIT
static fixed_t K_GrowShrinkOrNormal(const player_t *player)
{
	fixed_t scaleDiff = player->mo->scale - mapobjectscale;
	fixed_t playerScale = FixedDiv(player->mo->scale, mapobjectscale);
	fixed_t speedMul = FRACUNIT;

	if (scaleDiff > 0 || scaleDiff < 0)
	{
		// Grown or Shrunk
		speedMul = playerScale;
	}

	return speedMul;
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

fixed_t K_GetKartSpeed(player_t *player, boolean doboostpower, boolean dorubberband)
{
	fixed_t k_speed = 150;
	fixed_t g_cc = FRACUNIT;
	fixed_t xspd = 3072;		// 4.6875 aka 3/64
	UINT8 kartspeed = player->kartspeed;
	fixed_t finalspeed;

	if (doboostpower && !player->pogospring && !P_IsObjectOnGround(player->mo))
		return (75*mapobjectscale); // air speed cap

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

	if ((gametyperules & GTR_KARMA) && (player->bumper <= 0))
		kartspeed = 1;

	k_speed += kartspeed*3; // 153 - 177

	finalspeed = FixedMul(FixedMul(k_speed<<14, g_cc), player->mo->scale);
	
	if (player->spheres > 0)
	{
		fixed_t sphereAdd = (FRACUNIT/60); // 66% at max
		finalspeed = FixedMul(finalspeed, FRACUNIT + (sphereAdd * player->spheres));
	}
	
	if (K_PlayerUsesBotMovement(player))
	{
		// Increase bot speed by 1-10% depending on difficulty
		fixed_t add = (player->botvars.difficulty * (FRACUNIT/10)) / DIFFICULTBOT;
		finalspeed = FixedMul(finalspeed, FRACUNIT + add);

		if (player->bot && player->botvars.rival)
		{
			// +10% top speed for the rival
			finalspeed = FixedMul(finalspeed, 11*FRACUNIT/10);
		}
	}
	
	if (dorubberband == true && player->botvars.rubberband < FRACUNIT && K_PlayerUsesBotMovement(player) == true)
	{
		finalspeed = FixedMul(finalspeed, player->botvars.rubberband);
	}

	if (doboostpower)
		finalspeed = FixedMul(finalspeed, player->boostpower+player->speedboost);
	
	if (player->outrun != 0)
	{
		// Milky Way's roads
		finalspeed += FixedMul(player->outrun, K_GrowShrinkOrNormal(player));
	}

	
	return finalspeed;
}

fixed_t K_GetKartAccel(player_t *player)
{
	fixed_t k_accel = 32; // 36;
	UINT8 kartspeed = player->kartspeed;

	if ((gametyperules & GTR_KARMA) && player->bumper <= 0)
		kartspeed = 1;

	k_accel += 4 * (9 - kartspeed); // 32 - 64

	return FixedMul(k_accel, FRACUNIT+player->accelboost);
}

UINT16 K_GetKartFlashing(player_t *player)
{
	UINT16 tics = flashingtics;

	if (gametype == GT_BATTLE)
	{
		// TODO: gametyperules
		return 1;
	}

	if (player == NULL)
	{
		return tics;
	}

	tics += (tics/8) * (player->kartspeed);
	return tics;
}

boolean K_PlayerShrinkCheat(player_t *player)
{
	return (
		(player->pflags & PF_SHRINKACTIVE)
		&& (player->bot == false)
		&& (modeattacking == false) // Anyone want to make another record attack category?
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

boolean K_KartKickstart(player_t *player)
{
	return ((player->pflags & PF_KICKSTARTACCEL)
		&& (!K_PlayerUsesBotMovement(player))
		&& (player->kickstartaccel >= ACCEL_KICKSTART));
}

UINT16 K_GetKartButtons(player_t *player)
{
	return (player->cmd.buttons |
		(K_KartKickstart(player) ? BT_ACCELERATE : 0));
}

SINT8 K_GetForwardMove(player_t *player)
{
	SINT8 forwardmove = player->cmd.forwardmove;
	
	if ((player->exiting || mapreset))
	{
		return 0;
	}
	
	if ((player->pflags & PF_STASIS) || (player->carry == CR_SLIDING))
	{
		return 0;
	}

	if (player->sneakertimer)
	{
		return MAXPLMOVE;
	}

	if (player->spinouttimer)
	{
		return 0;
	}
	
	if (leveltime < starttime && !(gametyperules & GTR_FREEROAM))
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

fixed_t K_GetNewSpeed(player_t *player)
{
	const fixed_t accelmax = 4000;
	const fixed_t p_speed = K_GetKartSpeed(player, true, true);
	fixed_t p_accel = K_GetKartAccel(player);
	fixed_t newspeed, oldspeed, finalspeed;
	boolean onground = (P_IsObjectOnGround(player->mo) || (player->pogospring));
	
	if (!onground) return 0; // If the player isn't on the ground, there is no change in speed

	if (K_PlayerUsesBotMovement(player) == true && player->botvars.rubberband > 0)
	{
		// Acceleration is tied to top speed...
		// so if we want JUST a top speed boost, we have to do this...
		p_accel = FixedDiv(p_accel, player->botvars.rubberband);
	}

	oldspeed = R_PointToDist2(0, 0, player->rmomx, player->rmomy);
	newspeed = FixedDiv(FixedDiv(FixedMul(oldspeed, accelmax - p_accel) + FixedMul(p_speed, p_accel), accelmax), ORIG_FRICTION);
	
	if (player->pogospring) // Pogo Spring minimum/maximum thrust
	{
		const fixed_t hscale = mapobjectscale;
		fixed_t minspeed = 24*hscale;
		fixed_t maxspeed = 28*hscale;
		
		if (player->mo->terrain)
		{
			minspeed = player->mo->terrain->pogoSpringMin*hscale;
			maxspeed = player->mo->terrain->pogoSpringMax*hscale;
		}

		if (newspeed > maxspeed && player->pogospring == 2)
			newspeed = maxspeed;
		if (newspeed < minspeed)
			newspeed = minspeed;
	}

	finalspeed = newspeed - oldspeed;

	return finalspeed;
}

fixed_t K_3dKartMovement(player_t *player)
{
	fixed_t finalspeed = K_GetNewSpeed(player);

	SINT8 forwardmove = K_GetForwardMove(player);

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

angle_t K_MomentumAngle(mobj_t *mo)
{
	if (FixedHypot(mo->momx, mo->momy) >= mo->scale)
	{
		return R_PointToAngle2(0, 0, mo->momx, mo->momy);
	}
	else
	{
		return mo->angle; // default to facing angle, rather than 0
	}
}

void K_AwardPlayerRings(player_t *player, INT32 rings, boolean overload)
{
	UINT16 superring;

	if (!overload)
	{
		INT32 totalrings =
			RINGTOTAL(player) + (player->superring / 3);

		/* capped at 20 rings */
		if ((totalrings + rings) > 20)
			rings = (20 - totalrings);
	}

	superring = player->superring + (rings * 3);

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

void K_BattleAwardHit(player_t *player, player_t *victim, mobj_t *inflictor, UINT8 bumpersRemoved)
{
	UINT8 points = 1;
	boolean trapItem = false;

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
			if ((victim->bumper > 0) && (victim->bumper <= bumpersRemoved))
			{
				// +2 points for finishing off a player
				points = 2;
			}
		}
	}

	if (gametyperules & GTR_POINTLIMIT)
	{
		P_AddPlayerScore(player, points);
		K_SpawnBattlePoints(player, victim, points);
	}
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
	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
}

static void K_RemoveGrowShrink(player_t *player)
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
	player->growcancel = -1;

	P_RestoreMusic(player);
}

void K_SquishPlayer(player_t *player, mobj_t *inflictor, mobj_t *source)
{
	(void)inflictor;
	(void)source;
	
	player->squishedtimer = TICRATE;

	// Reduce Shrink timer
	if (player->growshrinktimer < 0)
	{
		player->growshrinktimer += TICRATE;
		if (player->growshrinktimer >= 0)
			K_RemoveGrowShrink(player);
	}

	player->mo->flags |= MF_NOCLIP;

	player->instashield = 15;
}

void K_ApplyTripWire(player_t *player, tripwirestate_t state)
{
	// We are either softlocked or wildly misbehaving. Stop that!
	if (state == TRIPSTATE_BLOCKED && player->tripwireReboundDelay && (player->speed > 5 * K_GetKartSpeed(player, false, false)))
		P_DamageMobj(player->mo, NULL, NULL, 1, DMG_STING);

	if (state == TRIPSTATE_PASSED)
	{
		S_StartSound(player->mo, sfx_ssa015);
		player->tripwireLeniency += TICRATE/2;
	}
	else if (state == TRIPSTATE_BLOCKED)
	{
		S_StartSound(player->mo, sfx_kc40);
		player->tripwireReboundDelay = 60;
	}

	player->tripwireState = state;
}

INT32 K_ExplodePlayer(player_t *player, mobj_t *inflictor, mobj_t *source) // A bit of a hack, we just throw the player up higher here and extend their spinout timer
{
	INT32 ringburst = 10;

	(void)source;

	K_DirectorFollowAttack(player, inflictor, source);

	player->mo->momz = 18*mapobjectscale*P_MobjFlip(player->mo); // please stop forgetting mobjflip checks!!!!
	if (player->mo->eflags & MFE_UNDERWATER)
		player->mo->momz = (117 * player->mo->momz) / 200;
	player->mo->momx = player->mo->momy = 0;

	player->spinouttype = KSPIN_EXPLOSION;
	player->spinouttimer = (3*TICRATE/2)+2;

	if (inflictor && !P_MobjWasRemoved(inflictor))
	{
		if (inflictor->type == MT_SPBEXPLOSION && inflictor->extravalue1)
		{
			player->spinouttimer = ((5*player->spinouttimer)/2)+1;
			player->mo->momz *= 2;
			ringburst = 20;
		}
	}

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

	if (P_IsDisplayPlayer(player))
		P_StartQuake(64<<FRACBITS, 5);

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

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
}

void K_HandleBumperChanges(player_t *player, UINT8 prevBumpers)
{
	if (!(gametyperules & GTR_BUMPERS))
	{
		// Bumpers aren't being used
		return;
	}

	// TODO: replace all console text print-outs with a real visual

	if (player->bumper > 0 && prevBumpers == 0)
	{
		if (netgame)
		{
			CONS_Printf(M_GetText("%s is back in the game!\n"), player_names[player-players]);
		}
	}
	else if (player->bumper == 0 && prevBumpers > 0)
	{
		mobj_t *karmahitbox = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_KARMAHITBOX);
		P_SetTarget(&karmahitbox->target, player->mo);

		karmahitbox->destscale = player->mo->destscale;
		P_SetScale(karmahitbox, player->mo->scale);

		player->karmadelay = comebacktime;

		if (bossinfo.boss)
		{
			P_DoTimeOver(player);
		}
		else if (netgame)
		{
			CONS_Printf(M_GetText("%s lost all of their bumpers!\n"), player_names[player-players]);
		}
	}

	K_CalculateBattleWanted();
	K_CheckBumpers();
}

void K_DestroyBumpers(player_t *player, UINT8 amount)
{
	UINT8 oldBumpers = player->bumper;

	if (!(gametyperules & GTR_BUMPERS))
	{
		return;
	}

	amount = min(amount, player->bumper);

	if (amount == 0)
	{
		return;
	}

	player->bumper -= amount;
	K_HandleBumperChanges(player, oldBumpers);
}

void K_TakeBumpersFromPlayer(player_t *player, player_t *victim, UINT8 amount)
{
	UINT8 oldPlayerBumpers = player->bumper;
	UINT8 oldVictimBumpers = victim->bumper;

	UINT8 tookBumpers = 0;

	if (!(gametyperules & GTR_BUMPERS))
	{
		return;
	}

	amount = min(amount, victim->bumper);

	if (amount == 0)
	{
		return;
	}

	while ((tookBumpers < amount) && (victim->bumper > 0))
	{
		UINT8 newbumper = player->bumper;

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

		P_SetTarget(&newmo->tracer, victim->mo);
		P_SetTarget(&newmo->target, player->mo);

		newmo->angle = (diff * (newbumper-1));
		newmo->color = victim->skincolor;

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

		player->bumper++;
		victim->bumper--;
		tookBumpers++;
	}

	if (tookBumpers == 0)
	{
		// No change occured.
		return;
	}

	// Play steal sound
	S_StartSound(player->mo, sfx_3db06);

	K_HandleBumperChanges(player, oldPlayerBumpers);
	K_HandleBumperChanges(victim, oldVictimBumpers);
}

// Spawns the purely visual explosion
void K_SpawnMineExplosion(mobj_t *source, UINT8 color)
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

	S_StartSound(smoldering, sfx_s3k4e);

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

		truc = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_BOOMEXPLODE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*6;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(10*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(-speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(-speed, speed)*FRACUNIT;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(-speed, speed)*FRACUNIT*P_MobjFlip(truc);
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->color = color;
	}

	for (i = 0; i < 16; i++)
	{
		dust = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		dust->tics = 30;
		dust->momz = P_RandomRange(FixedMul(3*FRACUNIT, source->scale)>>FRACBITS, FixedMul(7*FRACUNIT, source->scale)>>FRACBITS)*FRACUNIT;

		truc = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_BOOMPARTICLE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*5;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(-speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(-speed, speed)*FRACUNIT;
		speed = FixedMul(15*FRACUNIT, source->scale)>>FRACBITS;
		speed2 = FixedMul(45*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(speed, speed2)*FRACUNIT*P_MobjFlip(truc);
		if (P_RandomChance(FRACUNIT/2))
			truc->momz = -truc->momz;
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->tics = TICRATE*2;
		truc->color = color;
	}
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

	if (!player->drift || player->driftcharge < K_GetKartDriftSparkValue(player))
		return;

	travelangle = player->mo->angle-(ANGLE_45/5)*player->drift;

	for (i = 0; i < 2; i++)
	{
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

		if (player->driftcharge >= K_GetKartDriftSparkValue(player)*4)
		{
			spark->color = (UINT8)(1 + (leveltime % (MAXSKINCOLORS-1)));
			
			
			if (player->driftcharge <= K_GetKartDriftSparkValue(player)*4+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
			
		}
		else if (player->driftcharge >= K_GetKartDriftSparkValue(player)*2)
		{
			if (player->driftcharge <= (K_GetKartDriftSparkValue(player)*2)+(24*3))
				spark->color = SKINCOLOR_RASPBERRY; // transition
			else
				spark->color = SKINCOLOR_KETCHUP;
			
			if (player->driftcharge <= K_GetKartDriftSparkValue(player)*2+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
			
		}
		else
		{
			spark->color = SKINCOLOR_SAPPHIRE;
			
			
			if (player->driftcharge <= K_GetKartDriftSparkValue(player)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*2));
			}
		}

		if ((player->drift > 0 && player->cmd.turning > 0) // Inward drifts
			|| (player->drift < 0 && player->cmd.turning < 0))
		{
			if ((player->drift < 0 && (i & 1))
				|| (player->drift > 0 && !(i & 1)))
				P_SetMobjState(spark, S_DRIFTSPARK_A1);
			else if ((player->drift < 0 && !(i & 1))
				|| (player->drift > 0 && (i & 1)))
				P_SetMobjState(spark, S_DRIFTSPARK_C1);
		}
		else if ((player->drift > 0 && player->cmd.turning < 0) // Outward drifts
			|| (player->drift < 0 && player->cmd.turning > 0))
		{
			if ((player->drift < 0 && (i & 1))
				|| (player->drift > 0 && !(i & 1)))
				P_SetMobjState(spark, S_DRIFTSPARK_C1);
			else if ((player->drift < 0 && !(i & 1))
				|| (player->drift > 0 && (i & 1)))
				P_SetMobjState(spark, S_DRIFTSPARK_A1);
		}

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
		|| player->hyudorotimer != 0
		|| ((gametyperules & GTR_BUMPERS) && player->bumper <= 0 && player->karmadelay))
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
	const INT32 rad = (mo->radius*2)>>FRACBITS;
	mobj_t *sparkle;
	INT32 i;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (i = 0; i < 3; i++)
	{
		fixed_t newx = mo->x + mo->momx + (P_RandomRange(-rad, rad)<<FRACBITS);
		fixed_t newy = mo->y + mo->momy + (P_RandomRange(-rad, rad)<<FRACBITS);
		fixed_t newz = mo->z + mo->momz + (P_RandomRange(0, mo->height>>FRACBITS)<<FRACBITS);

		sparkle = P_SpawnMobj(newx, newy, newz, MT_SPARKLETRAIL);
		K_FlipFromObject(sparkle, mo);

		P_SetTarget(&sparkle->target, mo);
		sparkle->destscale = mo->destscale;
		P_SetScale(sparkle, mo->scale);
		sparkle->color = mo->color;
	}

	P_SetMobjState(sparkle, S_KARTINVULN_LARGE1);
}

void K_SpawnWipeoutTrail(mobj_t *mo, boolean translucent)
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

	dust = P_SpawnMobj(mo->x + FixedMul(24*mo->scale, FINECOSINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(-8,8) << FRACBITS),
		mo->y + FixedMul(24*mo->scale, FINESINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(-8,8) << FRACBITS),
		mo->z, MT_WIPEOUTTRAIL);

	P_SetTarget(&dust->target, mo);
	dust->angle = K_MomentumAngle(mo);
	dust->destscale = mo->scale;
	P_SetScale(dust, mo->scale);
	K_FlipFromObject(dust, mo);

	if (translucent) // offroad effect
	{
		dust->momx = mo->momx/2;
		dust->momy = mo->momy/2;
		dust->momz = mo->momz/2;
	}

	if (translucent)
		dust->renderflags |= RF_TRANS50;
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
	const INT16 spawnrange = spawner->radius>>FRACBITS;

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

			if (spawner->player->speed < 5<<FRACBITS)
				return;

			if (spawner->player->cmd.forwardmove < 0)
				playerangle += ANGLE_180;

			anglediff = abs((signed)(playerangle - R_PointToAngle2(0, 0, spawner->player->rmomx, spawner->player->rmomy)));
		}
	}
	else
	{
		if (P_AproxDistance(spawner->momx, spawner->momy) < 5<<FRACBITS)
			return;

		anglediff = abs((signed)(spawner->angle - R_PointToAngle2(0, 0, spawner->momx, spawner->momy)));
	}

	if (anglediff > ANGLE_180)
		anglediff = InvAngle(anglediff);

	if (anglediff > ANG10*4) // Trying to turn further than 40 degrees
	{
		fixed_t spawnx = P_RandomRange(-spawnrange, spawnrange)<<FRACBITS;
		fixed_t spawny = P_RandomRange(-spawnrange, spawnrange)<<FRACBITS;
		INT32 speedrange = 2;
		mobj_t *dust = P_SpawnMobj(spawner->x + spawnx, spawner->y + spawny, spawner->z, MT_DRIFTDUST);
		dust->momx = FixedMul(spawner->momx + (P_RandomRange(-speedrange, speedrange)<<FRACBITS), 3*(spawner->scale)/4);
		dust->momy = FixedMul(spawner->momy + (P_RandomRange(-speedrange, speedrange)<<FRACBITS), 3*(spawner->scale)/4);
		dust->momz = P_MobjFlip(spawner) * (P_RandomRange(1, 4) * (spawner->scale));
		P_SetScale(dust, spawner->scale/2);
		dust->destscale = spawner->scale * 3;
		dust->scalespeed = spawner->scale/12;
		dust->target = spawner;

		if (leveltime % 6 == 0)
			S_StartSound(spawner, sfx_screec);

		K_MatchGenericExtraFlags(dust, spawner);
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
#if 0
			if (player->throwdir == 1)
				dir = 3;
			else if (player->throwdir == -1)
				dir = 1;
			else
				dir = 2;
#else
			if (player->throwdir == 1)
				dir = 2;
			else
				dir = 1;
#endif
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
			mo = NULL; // can't return multiple projectiles
			if (dir == -1)
			{
				// Shoot backward
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) - 0x06000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) - 0x03000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) + 0x03000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) + 0x06000000, 0, PROJSPEED/8);
			}
			else
			{
				// Shoot forward
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x06000000, 0, PROJSPEED);
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

			if (mapthing == MT_DROPTARGET && mo)
			{
				mo->reactiontime = TICRATE/2;
				P_SetMobjState(mo, mo->info->painstate);
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
			else if (mapthing == MT_BUBBLESHIELDTRAP)
			{
				P_SetScale(mo, ((5*mo->destscale)>>2)*4);
				mo->destscale = (5*mo->destscale)>>2;
				S_StartSound(mo, sfx_s3kbfl);
			}
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

	switch (gamespeed)
	{
		case 0:
			spd = 68*mapobjectscale; // Avg Speed is 34
			break;
		case 2:
			spd = 96*mapobjectscale; // Avg Speed is 48
			break;
		default:
			spd = 82*mapobjectscale; // Avg Speed is 41
			break;
	}

	mine->flags |= MF_NOCLIPTHING;

	P_SetMobjState(mine, S_SSMINE_AIR1);
	mine->threshold = 10;
	mine->reactiontime = mine->info->reactiontime;

	mine->momx = punter->momx + FixedMul(FINECOSINE(fa >> ANGLETOFINESHIFT), spd);
	mine->momy = punter->momy + FixedMul(FINESINE(fa >> ANGLETOFINESHIFT), spd);
	P_SetObjectMomZ(mine, z, false);

	mine->flags &= ~MF_NOCLIPTHING;
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
		mo->angle = P_RandomRange(0, 359)*ANG1;
		mo->fuse = P_RandomRange(20, 50);
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

		P_Thrust(smoke, src->angle + FixedAngle(P_RandomRange(135, 225)<<FRACBITS), P_RandomRange(0, 8) * src->scale);
		smoke->momz += P_RandomRange(0, 4) * src->scale;
	}
}

static void K_DoHyudoroSteal(player_t *player)
{
	INT32 i, numplayers = 0;
	INT32 playerswappable[MAXPLAYERS];
	INT32 stealplayer = -1; // The player that's getting stolen from
	INT32 prandom = 0;
	boolean sink = P_RandomChance(FRACUNIT/64);
	INT32 hyu = hyudorotime;

	if (gametype == GT_RACE)
		hyu *= 2; // double in race

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && players[i].mo->health > 0 && players[i].playerstate == PST_LIVE
			&& player != &players[i] && !players[i].exiting && !players[i].spectator // Player in-game

			// Can steal from this player
			&& (gametype == GT_RACE //&& players[i].position < player->position)
			|| ((gametyperules & GTR_BUMPERS) && players[i].bumper > 0))

			// Has an item
			&& (players[i].itemtype
			&& players[i].itemamount
			&& !(players[i].pflags & PF_ITEMOUT)))
		{
			playerswappable[numplayers] = i;
			numplayers++;
		}
	}

	prandom = P_RandomFixed();
	S_StartSound(player->mo, sfx_s3k92);

	if (sink && numplayers > 0 && cv_kitchensink.value) // BEHOLD THE KITCHEN SINK
	{
		player->hyudorotimer = hyu;
		player->stealingtimer = stealtime;

		player->itemtype = KITEM_KITCHENSINK;
		player->itemamount = 1;
		K_UnsetItemOut(player);
		return;
	}
	else if ((gametype == GT_RACE && player->position == 1) || numplayers == 0) // No-one can be stolen from? Oh well...
	{
		player->hyudorotimer = hyu;
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
		player->hyudorotimer = hyu;
		player->stealingtimer = stealtime;
		players[stealplayer].stolentimer = stealtime;

		player->itemtype = players[stealplayer].itemtype;
		player->itemamount = players[stealplayer].itemamount;
		K_UnsetItemOut(player);

		players[stealplayer].itemtype = KITEM_NONE;
		players[stealplayer].itemamount = 0;
		K_UnsetItemOut(&players[stealplayer]);

		if (P_IsDisplayPlayer(&players[stealplayer]) && !r_splitscreen)
			S_StartSound(NULL, sfx_s3k92);
	}
}

void K_DoSneaker(player_t *player, INT32 type)
{
	fixed_t intendedboost;

	switch (gamespeed)
	{
		case 0:
			intendedboost = 53740+768;
			break;
		case 2:
			intendedboost = 17294+768;
			break;
		//expert
		case 3:
			intendedboost = 14706;
			break;
		default:
			intendedboost = 32768;
			break;
	}

	if (player->floorboost == 0 || player->floorboost == 3)
	{
		S_StopSoundByID(player->mo, sfx_cdfm01);
		S_StartSound(player->mo, sfx_cdfm01);

		K_SpawnDashDustRelease(player);
		if (intendedboost > player->speedboost)
			player->karthud[khud_destboostcam] = FixedMul(FRACUNIT, FixedDiv((intendedboost - player->speedboost), intendedboost));

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

	if (type != 0)
	{
		K_PlayBoostTaunt(player->mo);
	}

	player->sneakertimer = sneakertime;

	// set angle for spun out players:
	player->boostangle = player->mo->angle;
}

void K_DoWaterRunPanel(player_t *player)
{
	fixed_t intendedboost;

	switch (gamespeed)
	{
		case 0:
			intendedboost = 53740+768;
			break;
		case 2:
			intendedboost = 17294+768;
			break;
		//expert
		case 3:
			intendedboost = 14706;
			break;
		default:
			intendedboost = 32768;
			break;
	}

	if (player->floorboost == 0 || player->floorboost == 3)
	{
		S_StopSoundByID(player->mo, sfx_cdfm01);
		S_StopSoundByID(player->mo, sfx_kc3c);
		S_StartSound(player->mo, sfx_cdfm01);
		S_StartSound(player->mo, sfx_kc3c);

		K_SpawnDashDustRelease(player);
		if (intendedboost > player->speedboost)
			player->karthud[khud_destboostcam] = FixedMul(FRACUNIT, FixedDiv((intendedboost - player->speedboost), intendedboost));
	}

	if (player->sneakertimer == 0)
	{
		mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BOOSTFLAME);
		P_SetTarget(&overlay->target, player->mo);
		P_SetScale(overlay, (overlay->destscale = player->mo->scale));
		K_FlipFromObject(overlay, player->mo);
	}
	
	player->sneakertimer = TICRATE*2;
	player->waterrun = true;

	// set angle for spun out players:
	player->boostangle = player->mo->angle;
}

static void K_DoShrink(player_t *user)
{
	INT32 i;
	mobj_t *mobj, *next;

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
			else
			{
				// Start shrinking!
				K_DropItems(&players[i]);
				players[i].growshrinktimer = -(15*TICRATE);

				if (players[i].mo && !P_MobjWasRemoved(players[i].mo))
				{
					players[i].mo->scalespeed = mapobjectscale/TICRATE;
					players[i].mo->destscale = FixedMul(mapobjectscale, SHRINK_SCALE);

					if (K_PlayerShrinkCheat(&players[i]) == true)
					{
						players[i].mo->destscale = FixedMul(players[i].mo->destscale, SHRINK_SCALE);
					}

					S_StartSound(players[i].mo, sfx_kc59);
				}
			}
		}
	}

	// kill everything in the kitem list while we're at it:
	for (mobj = kitemcap; mobj; mobj = next)
	{
		next = mobj->itnext;

		// check if the item is being held by a player behind us before removing it.
		// check if the item is a "shield" first, bc i'm p sure thrown items keep the player that threw em as target anyway

		if (mobj->type == MT_BANANA_SHIELD || mobj->type == MT_JAWZ_SHIELD ||
		mobj->type == MT_SSMINE_SHIELD || mobj->type == MT_EGGMANITEM_SHIELD ||
		mobj->type == MT_SINK_SHIELD || mobj->type == MT_ORBINAUT_SHIELD ||
		mobj->type == MT_DROPTARGET_SHIELD)
		{
			if (mobj->target && mobj->target->player)
			{
				if (mobj->target->player->position > user->position)
					continue; // this guy's behind us, don't take his stuff away!
			}
		}

		mobj->destscale = 0;
		mobj->flags &= ~(MF_SOLID|MF_SHOOTABLE|MF_SPECIAL);
		mobj->flags |= MF_NOCLIPTHING; // Just for safety

		if (mobj->type == MT_SPB)
			spbplace = -1;
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
					thrust = FixedMul(thrust, 5*FRACUNIT/4);
				else if (mo->player->invincibilitytimer)
					thrust = FixedMul(thrust, 9*FRACUNIT/8);
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

	player->invincibilitytimer += time;

	if (P_IsLocalPlayer(player) == true)
	{
		S_ChangeMusicSpecial("kinvnc");
	}
	else //used to be "if (P_IsDisplayPlayer(player) == false)"
	{
		S_StartSound(player->mo, (cv_kartinvinsfx.value ? sfx_alarmi : sfx_kinvnc));
	}

	P_RestoreMusic(player);
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

		P_SetObjectMomZ(banana, 8*FRACUNIT, false);
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
void K_DropHnextList(player_t *player, boolean keepshields)
{
	mobj_t *work = player->mo, *nextwork, *dropwork;
	INT32 flip;
	mobjtype_t type;
	boolean orbit, ponground, dropall = true;
	INT32 shield = K_GetShieldFromItem(player->itemtype);

	if (work == NULL || P_MobjWasRemoved(work))
	{
		return;
	}

	flip = P_MobjFlip(player->mo);
	ponground = P_IsObjectOnGround(player->mo);

	if (shield != KSHIELD_NONE && !keepshields)
	{
		if (shield == KSHIELD_LIGHTNING)
		{
			K_DoLightningShield(player);
		}

		player->curshield = KSHIELD_NONE;
		player->itemtype = KITEM_NONE;
		player->itemamount = 0;
		K_UnsetItemOut(player);
	}

	nextwork = work->hnext;

	while ((work = nextwork) && !(work == NULL || P_MobjWasRemoved(work)))
	{
		nextwork = work->hnext;

		if (!work->health)
			continue; // taking care of itself

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
			case MT_DROPTARGET_SHIELD:
				orbit = false;
				dropall = false;
				type = MT_DROPTARGET;
				break;
			case MT_EGGMANITEM_SHIELD:
				orbit = false;
				type = MT_EGGMANITEM;
				break;
			// intentionally do nothing
			case MT_ROCKETSNEAKER:
			case MT_SINK_SHIELD:
				return;
			default:
				continue;
		}

		dropwork = P_SpawnMobj(work->x, work->y, work->z, type);

		P_SetTarget(&dropwork->target, player->mo);
		P_AddKartItem(dropwork); // needs to be called here so shrink can bust items off players in front of the user.

		dropwork->angle = work->angle;

		dropwork->scalespeed = work->scalespeed;
		dropwork->spritexscale = work->spritexscale;
		dropwork->spriteyscale = work->spriteyscale;

		dropwork->flags |= MF_NOCLIPTHING;
		dropwork->flags2 = work->flags2;
		dropwork->eflags = work->eflags;

		dropwork->renderflags = work->renderflags;
		dropwork->color = work->color;
		dropwork->colorized = work->colorized;
		dropwork->whiteshadow = work->whiteshadow;

		dropwork->floorz = work->floorz;
		dropwork->ceilingz = work->ceilingz;

		dropwork->health = work->health; // will never be set to 0 as long as above guard exists

		// Copy interp data
		dropwork->old_angle = work->old_angle;
		dropwork->old_x = work->old_x;
		dropwork->old_y = work->old_y;
		dropwork->old_z = work->old_z;

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
			{
				dropwork->z += 20*flip*dropwork->scale;
			}
			else
			{
				dropwork->angle -= ANGLE_90;
			}
		}
		else // plop on the ground
		{
			dropwork->threshold = 10;
			dropwork->flags &= ~MF_NOCLIPTHING;
		}

		P_RemoveMobj(work);
	}

	// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
	P_SetTarget(&player->mo->hnext, NULL);

	player->bananadrag = 0;

	if (player->pflags & PF_EGGMANOUT)
	{
		player->pflags &= ~PF_EGGMANOUT;
	}
	else if ((player->pflags & PF_ITEMOUT)
		&& (dropall || (--player->itemamount <= 0)))
	{
		player->itemamount = 0;
		K_UnsetItemOut(player);
		player->itemtype = KITEM_NONE;
	}
}

mobj_t *K_CreatePaperItem(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT8 type, UINT8 amount)
{
	mobj_t *drop = P_SpawnMobj(x, y, z, MT_FLOATINGITEM);
	P_SetScale(drop, drop->scale>>4);
	drop->destscale = (3*drop->destscale)/2;

	drop->angle = angle;
	P_Thrust(drop,
		FixedAngle(P_RandomFixed() * 180) + angle,
		16*mapobjectscale);

	drop->momz = flip * 3 * mapobjectscale;
	if (drop->eflags & MFE_UNDERWATER)
		drop->momz = (117 * drop->momz) / 200;

	if (type == 0)
	{
		UINT8 useodds = 0;
		INT32 spawnchance[NUMKARTRESULTS];
		INT32 totalspawnchance = 0;
		INT32 i;

		memset(spawnchance, 0, sizeof (spawnchance));

		useodds = amount;

		for (i = 1; i < NUMKARTRESULTS; i++)
		{
			spawnchance[i] = (totalspawnchance += K_KartGetItemOdds(
				useodds, i,
				UINT32_MAX,
				0,
				false, false, false
				)
			);
		}

		if (totalspawnchance > 0)
		{
			UINT8 newType;
			UINT8 newAmount;

			totalspawnchance = P_RandomKey(totalspawnchance);
			for (i = 0; i < NUMKARTRESULTS && spawnchance[i] <= totalspawnchance; i++);

			// TODO: this is bad!
			// K_KartGetItemResult requires a player
			// but item roulette will need rewritten to change this

			switch (i)
			{
				// Special roulettes first, then the generic ones are handled by default
				case KRITEM_DUALSNEAKER: // Sneaker x2
					newType = KITEM_SNEAKER;
					newAmount = 2;
					break;
				case KRITEM_TRIPLESNEAKER: // Sneaker x3
					newType = KITEM_SNEAKER;
					newAmount = 3;
					break;
				case KRITEM_TRIPLEBANANA: // Banana x3
					newType = KITEM_BANANA;
					newAmount = 3;
					break;
				case KRITEM_TENFOLDBANANA: // Banana x10
					newType = KITEM_BANANA;
					newAmount = 10;
					break;
				case KRITEM_TRIPLEORBINAUT: // Orbinaut x3
					newType = KITEM_ORBINAUT;
					newAmount = 3;
					break;
				case KRITEM_QUADORBINAUT: // Orbinaut x4
					newType = KITEM_ORBINAUT;
					newAmount = 4;
					break;
				case KRITEM_DUALJAWZ: // Jawz x2
					newType = KITEM_JAWZ;
					newAmount = 2;
					break;
				default:
					newType = i;
					newAmount = 1;
					break;
			}

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
			drop->threshold = 1;
			drop->movecount = 1;
		}
	}
	else
	{
		drop->threshold = type;
		drop->movecount = amount;
	}

	drop->flags |= MF_NOCLIPTHING;

	return drop;
}

// For getting EXTRA hit!
void K_DropItems(player_t *player)
{
	K_DropHnextList(player, true);

	if (player->mo && !P_MobjWasRemoved(player->mo) && player->itemamount > 0)
	{
		mobj_t *drop = K_CreatePaperItem(
			player->mo->x, player->mo->y, player->mo->z + player->mo->height/2,
			player->mo->angle + ANGLE_90, P_MobjFlip(player->mo),
			player->itemtype, player->itemamount
		);

		K_FlipFromObject(drop, player->mo);
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
		P_SetObjectMomZ(shoe, 8*FRACUNIT, false);
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
	if (orbit->target && orbit->target->player)
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

		if (orbit->target->player->itemamount != num)
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
	TryMoveResult_t result = {0};
	if (!player->mo)
		return;

	if (!player->mo->hnext)
	{
		player->bananadrag = 0;
		if (player->pflags & PF_EGGMANOUT)
			player->pflags &= ~PF_EGGMANOUT;
		else if (player->pflags & PF_ITEMOUT)
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
		if (player->pflags & PF_EGGMANOUT)
			player->pflags &= ~PF_EGGMANOUT;
		else if (player->pflags & PF_ITEMOUT)
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
				mobj_t *cur = player->mo->hnext;
				fixed_t speed = ((8 - min(4, player->itemamount)) * cur->info->speed) / 7;

				player->bananadrag = 0; // Just to make sure

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius); // mobj's distance from its Target, or Radius.
					fixed_t z;

					if (!cur->health)
					{
						cur = cur->hnext;
						continue;
					}

					cur->color = player->skincolor;

					cur->angle -= ANGLE_90;
					cur->angle += FixedAngle(speed);

					if (cur->extravalue1 < radius)
						cur->extravalue1 += P_AproxDistance(cur->extravalue1, radius) / 12;
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					// If the player is on the ceiling, then flip your items as well.
					if (player && player->mo->eflags & MFE_VERTICALFLIP)
						cur->eflags |= MFE_VERTICALFLIP;
					else
						cur->eflags &= ~MFE_VERTICALFLIP;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

					if (P_MobjFlip(cur) > 0)
						z = player->mo->z;
					else
						z = player->mo->z + player->mo->height - cur->height;

					cur->flags |= MF_NOCLIPTHING; // temporarily make them noclip other objects so they can't hit anyone while in the player
					P_MoveOrigin(cur, player->mo->x, player->mo->y, z);
					cur->momx = FixedMul(FINECOSINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->momy = FixedMul(FINESINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->flags &= ~MF_NOCLIPTHING;

					if (!P_TryMove(cur, player->mo->x + cur->momx, player->mo->y + cur->momy, true, &result))
						P_SlideMove(cur,&result);

					if (P_IsObjectOnGround(player->mo))
					{
						if (P_MobjFlip(cur) > 0)
						{
							if (cur->floorz > player->mo->z - cur->height)
								z = cur->floorz;
						}
						else
						{
							if (cur->ceilingz < player->mo->z + player->mo->height + cur->height)
								z = cur->ceilingz - cur->height;
						}
					}

					// Center it during the scale up animation
					z += (FixedMul(mobjinfo[cur->type].height, player->mo->scale - cur->scale)>>1) * P_MobjFlip(cur);

					cur->z = z;
					cur->momx = cur->momy = 0;
					cur->angle += ANGLE_90;

					cur = cur->hnext;
				}
			}
			break;
		case MT_BANANA_SHIELD: // Kart trailing items
		case MT_SSMINE_SHIELD:
		case MT_DROPTARGET_SHIELD:
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
					else if (cur->type == MT_DROPTARGET_SHIELD)
					{
						cur->renderflags = (cur->renderflags|RF_FULLBRIGHT) ^ RF_FULLDARK; // the difference between semi and fullbright
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

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

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
						&& P_RandomChance(min(FRACUNIT/2, FixedDiv(player->speed, K_GetKartSpeed(player, false, false))/2)))
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

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

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
		else
		{
			fixed_t thisdist;
			fixed_t thisavg;

			// Don't go for people who are behind you
			if (thisang > ANGLE_45)
				continue;

			// Don't pay attention to dead players
			if (player->bumper <= 0)
				continue;

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

// Engine Sounds.
static void K_UpdateEngineSounds(player_t *player)
{
	const INT32 numsnds = 13;

	const fixed_t closedist = 160*FRACUNIT;
	const fixed_t fardist = 1536*FRACUNIT;

	const UINT8 dampenval = 48; // 255 * 48 = close enough to FRACUNIT/6

	const UINT16 buttons = K_GetKartButtons(player);

	INT32 class, s, w; // engine class number

	UINT8 volume = 255;
	fixed_t volumedampen = FRACUNIT;

	INT32 targetsnd = 0;
	INT32 i;

	s = (player->kartspeed - 1) / 3;
	w = (player->kartweight - 1) / 3;

#define LOCKSTAT(stat) \
	if (stat < 0) { stat = 0; } \
	if (stat > 2) { stat = 2; }
	LOCKSTAT(s);
	LOCKSTAT(w);
#undef LOCKSTAT

	class = s + (3*w);

	if (leveltime < 8 || player->spectator)
	{
		// Silence the engines, and reset sound number while we're at it.
		player->karthud[khud_enginesnd] = 0;
		return;
	}

#if 0
	if ((leveltime % 8) != ((player-players) % 8)) // Per-player offset, to make engines sound distinct!
#else
	if (leveltime % 8)
#endif
	{
		// .25 seconds of wait time between each engine sound playback
		return;
	}

	if ((leveltime >= starttime-(2*TICRATE) && leveltime <= starttime) || player->respawn) // Startup boost and dropdashing
	{
		// Startup boosts only want to check for BT_ACCELERATE being pressed.
		targetsnd = ((buttons & BT_ACCELERATE) ? 12 : 0);
	}
	else
	{
		// Average out the value of forwardmove and the speed that you're moving at.
		targetsnd = (((6 * player->cmd.forwardmove) / 25) + ((player->speed / mapobjectscale) / 5)) / 2;
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

	if (player->mo->health > 0 && !P_IsLocalPlayer(player)) // used to be !P_IsDisplayPlayer(player)
	{
		if (cv_kartinvinsfx.value)
		{
			if (player->growshrinktimer > 0) // Prioritize Grow
				sfxnum = sfx_alarmg;
			else if (player->invincibilitytimer > 0) 
				sfxnum = sfx_alarmi;

		}
		else
		{
			if (player->growshrinktimer > 0)
				sfxnum = sfx_kgrow;
			else if (player->invincibilitytimer > 0)
				sfxnum = sfx_kinvnc;
		}
	}

	if (sfxnum != sfx_None && !S_SoundPlaying(player->mo, sfxnum))
		S_StartSound(player->mo, sfxnum);

#define STOPTHIS(this) \
	if (sfxnum != this && S_SoundPlaying(player->mo, this)) \
		S_StopSoundByID(player->mo, this);
	STOPTHIS(sfx_alarmi);
	STOPTHIS(sfx_alarmg);
	STOPTHIS(sfx_kinvnc);
	STOPTHIS(sfx_kgrow);
#undef STOPTHIS
}

void K_KartPlayerHUDUpdate(player_t *player)
{
	if (player->karthud[khud_lapanimation])
		player->karthud[khud_lapanimation]--;

	if (player->karthud[khud_yougotem])
		player->karthud[khud_yougotem]--;

	if (player->karthud[khud_voices])
		player->karthud[khud_voices]--;

	if (player->karthud[khud_tauntvoices])
		player->karthud[khud_tauntvoices]--;

	if (player->karthud[khud_itemblink] && player->karthud[khud_itemblink]-- <= 0)
	{
		player->karthud[khud_itemblinkmode] = 0;
		player->karthud[khud_itemblink] = 0;
	}

	if (gametype == GT_RACE)
	{
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
		
		if (!ringsdisabled)
		{
			if (player->pflags & PF_RINGLOCK)
			{
				UINT8 normalanim = (leveltime % 14);
				UINT8 debtanim = 14 + (leveltime % 2);

				if (player->karthud[khud_ringspblock] >= 14) // debt animation
				{
					if ((player->rings > 0) // Get out of 0 ring animation
						&& (normalanim == 3 || normalanim == 10)) // on these transition frames.
						player->karthud[khud_ringspblock] = normalanim;
					else
						player->karthud[khud_ringspblock] = debtanim;
				}
				else // normal animation
				{
					if ((player->rings <= 0) // Go into 0 ring animation
						&& (player->karthud[khud_ringspblock] == 1 || player->karthud[khud_ringspblock] == 8)) // on these transition frames.
						player->karthud[khud_ringspblock] = debtanim;
					else
						player->karthud[khud_ringspblock] = normalanim;
				}
			}
			else
				player->karthud[khud_ringspblock] = (leveltime % 14); // reset to normal anim next time
		}
	}

	if ((gametyperules & GTR_BUMPERS) && (player->exiting || player->karmadelay))
	{
		if (player->exiting)
		{
			if (player->exiting < 6*TICRATE)
				player->karthud[khud_cardanimation] += ((164-player->karthud[khud_cardanimation])/8)+1;
			else if (player->exiting == 6*TICRATE)
				player->karthud[khud_cardanimation] = 0;
			else if (player->karthud[khud_cardanimation] < 2*TICRATE)
				player->karthud[khud_cardanimation]++;
		}
		else
		{
			if (player->karmadelay < 6*TICRATE)
				player->karthud[khud_cardanimation] -= ((164-player->karthud[khud_cardanimation])/8)+1;
			else if (player->karmadelay < 9*TICRATE)
				player->karthud[khud_cardanimation] += ((164-player->karthud[khud_cardanimation])/8)+1;
		}

		if (player->karthud[khud_cardanimation] > 164)
			player->karthud[khud_cardanimation] = 164;
		if (player->karthud[khud_cardanimation] < 0)
			player->karthud[khud_cardanimation] = 0;
	}
	else if (gametype == GT_RACE && player->exiting)
	{
		if (player->karthud[khud_cardanimation] < 2*TICRATE)
			player->karthud[khud_cardanimation]++;
	}
	else
		player->karthud[khud_cardanimation] = 0;
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

	if (!(thing->type == MT_RING || thing->type == MT_FLINGRING))
	{
		return BMIT_CONTINUE; // not a ring
	}

	if (thing->health <= 0)
	{
		return BMIT_CONTINUE; // dead
	}

	if (thing->extravalue1)
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

	if (RINGTOTAL(attractmo->player) >= 20 || (attractmo->player->pflags & PF_RINGLOCK))
	{
		// Already reached max -- just joustle rings around.

		// Regular ring -> fling ring
		if (thing->info->reactiontime && thing->type != (mobjtype_t)thing->info->reactiontime)
		{
			thing->type = thing->info->reactiontime;
			thing->info = &mobjinfo[thing->type];
			thing->flags = thing->info->flags;

			P_InstaThrust(thing, P_RandomRange(0,7) * ANGLE_45, 2 * thing->scale);
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
		player->tripwirePass = triplevel;
		player->tripwireLeniency = max(player->tripwireLeniency, TRIPWIRETIME);
	}
	else
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

		if (player->tripwireLeniency <= 0)
		{
			player->tripwirePass = TRIPWIRE_NONE;
		}
	}
}

static void K_RaceStart(player_t *player)
{
	// Start charging once you're given the opportunity.
	if (leveltime >= starttime-(2*TICRATE) && leveltime <= starttime)
	{
		if (player->cmd.buttons & BT_ACCELERATE)
		{
			if (player->boostcharge == 0)
				player->boostcharge = player->cmd.latency;

			player->boostcharge++;
		}
		else
			player->boostcharge = 0;
	}

	// Increase your size while charging your engine.
	if (leveltime < starttime+10)
	{
		player->mo->scalespeed = mapobjectscale/12;
		player->mo->destscale = mapobjectscale + (FixedMul(mapobjectscale, player->boostcharge*131));
		if (K_PlayerShrinkCheat(player) && !modeattacking && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	// Determine the outcome of your charge.
	if (leveltime > starttime && player->boostcharge)
	{
		// Not even trying?
		if (player->boostcharge < 35)
		{
			if (player->boostcharge > 17)
				S_StartSound(player->mo, sfx_cdfm00); // chosen instead of a conventional skid because it's more engine-like
		}
		// Get an instant boost!
		else if (player->boostcharge <= 50)
		{
			player->startboost = (50-player->boostcharge)+20;

			if (player->boostcharge <= 36)
			{
				player->startboost = 0;
				K_DoSneaker(player, 0);
				player->sneakertimer = 70; // PERFECT BOOST!!

				if (!player->floorboost || player->floorboost == 3) // Let everyone hear this one
					S_StartSound(player->mo, sfx_s25f);
			}
			else
			{
				K_SpawnDashDustRelease(player); // already handled for perfect boosts by K_DoSneaker
				if ((!player->floorboost || player->floorboost == 3) && P_IsLocalPlayer(player))
				{
					if (player->boostcharge <= 40)
						S_StartSound(player->mo, sfx_cdfm01); // You were almost there!
					else
						S_StartSound(player->mo, sfx_s23c); // Nope, better luck next time.
				}
			}
		}
		// You overcharged your engine? Those things are expensive!!!
		else if (player->boostcharge > 50)
		{
			player->nocontrol = 40;
			//S_StartSound(player->mo, sfx_kc34);
			S_StartSound(player->mo, sfx_s3k83);
			player->pflags |= PF_SKIDDOWN; // cheeky pflag reuse
		}

		player->boostcharge = 0;
	}

}

/**	\brief	Decreases various kart timers and powers per frame. Called in P_PlayerThink in p_user.c

	\param	player	player object passed from P_PlayerThink
	\param	cmd		control input from player

	\return	void
*/
void K_KartPlayerThink(player_t *player, ticcmd_t *cmd)
{
	const boolean onground = P_IsObjectOnGround(player->mo);

	/* reset sprite offsets :) */
	player->mo->sprxoff = 0;
	player->mo->spryoff = 0;
	player->mo->sprzoff = 0;
	player->mo->spritexoffset = 0;
	player->mo->spriteyoffset = 0;

	player->cameraOffset = 0;
	
	K_UpdateOffroad(player);
	K_UpdateEngineSounds(player); // Thanks, VAda!

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
			if (player->sneakertimer || player->ringboost
				|| player->driftboost || player->startboost)
			{
#if 0
				if (player->invincibilitytimer)
					K_SpawnInvincibilitySpeedLines(player->mo);
				else
#endif
					K_SpawnNormalSpeedLines(player);
			}
			
			// Could probably be moved somewhere else.
			K_HandleFootstepParticles(player->mo);
		}

		if (gametype == GT_RACE && player->rings <= 0 && !ringsdisabled) // spawn ring debt indicator
		{
			mobj_t *debtflag = P_SpawnMobj(player->mo->x + player->mo->momx, player->mo->y + player->mo->momy,
				player->mo->z + P_GetMobjZMovement(player->mo) + player->mo->height + (24*player->mo->scale), MT_THOK);

			P_SetMobjState(debtflag, S_RINGDEBT);
			P_SetScale(debtflag, (debtflag->destscale = player->mo->scale));

			K_MatchGenericExtraFlags(debtflag, player->mo);
			debtflag->frame += (leveltime % 4);

			if ((leveltime/12) & 1)
				debtflag->frame += 4;

			debtflag->color = player->skincolor;
			debtflag->fuse = 2;

			debtflag->renderflags = K_GetPlayerDontDrawFlag(player);
		}
	}

	if (player->itemtype == KITEM_NONE)
		player->pflags &= ~PF_HOLDREADY;

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

	player->karthud[khud_timeovercam] = 0;

	// Make ABSOLUTELY SURE that your flashing tics don't get set WHILE you're still in hit animations.
	if (player->spinouttimer != 0
		|| player->wipeoutslow != 0
		|| player->squishedtimer != 0)
	{
		player->flashing = K_GetKartFlashing(player);
	}
	else if (player->flashing >= K_GetKartFlashing(player))
	{
		player->flashing--;
	}

	if (player->spinouttimer)
	{
		if ((P_IsObjectOnGround(player->mo)
			|| ( player->spinouttype & KSPIN_AIRTIMER ))
			&& (!player->sneakertimer))
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

	
	if (ringsdisabled)
	{
		player->rings = 0;
	}
	else
	{
		if (player->rings > 20)
			player->rings = 20;
		else if (player->rings < -20)
			player->rings = -20;
	}

	if (player->spheres > 40)
		player->spheres = 40;
	// where's the < 0 check? see below the following block!

	if ((gametyperules & GTR_BUMPERS) && (player->bumper <= 0))
	{
		// Deplete 1 every tic when removed from the game.
		player->spheres--;
		player->spheredigestion = 0;
	}
	else
	{
		tic_t spheredigestion = TICRATE; // Base rate of 1 every second when playing.
		tic_t digestionpower = ((10 - player->kartspeed) + (10 - player->kartweight))-1; // 1 to 17

		// currently 0-34
		digestionpower = ((player->spheres*digestionpower)/20);

		if (digestionpower >= spheredigestion)
		{
			spheredigestion = 1;
		}
		else
		{
			spheredigestion -= digestionpower;
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

	if (comeback == false || !(gametyperules & GTR_KARMA) || (player->pflags & PF_ELIMINATED))
	{
		player->karmadelay = comebacktime;
	}
	else if (player->karmadelay > 0 && !P_PlayerInPain(player))
	{
		player->karmadelay--;
		if (P_IsDisplayPlayer(player) && player->bumper <= 0 && player->karmadelay <= 0)
			comebackshowninfo = true; // client has already seen the message
	}
	
	if (P_IsObjectOnGround(player->mo) && player->pogospring)
	{
		if (P_MobjFlip(player->mo)*player->mo->momz <= 0)
			player->pogospring = 0;
	}

	if (player->tripwireReboundDelay)
		player->tripwireReboundDelay--;

	if (player->ringdelay)
		player->ringdelay--;

	if (P_PlayerInPain(player))
		player->ringboost = 0;
	else if (player->ringboost)
		player->ringboost--;

	if (player->sneakertimer)
	{
		player->sneakertimer--;
	}

	if (player->flamedash)
		player->flamedash--;

	if (player->sneakertimer && player->wipeoutslow > 0 && player->wipeoutslow < wipeoutslowtime+1)
		player->wipeoutslow = wipeoutslowtime+1;

	if (player->floorboost > 0)
		player->floorboost--;
	
	if (player->sneakertimer == 0)
		player->waterrun = false;

	if (player->driftboost)
		player->driftboost--;

	if (player->startboost > 0 && onground == true)
	{
		player->startboost--;
	}

	if (player->invincibilitytimer)
		player->invincibilitytimer--;
	
	if (player->checkskip)
		player->checkskip--;

	if (player->growshrinktimer != 0)
	{
		if (player->growshrinktimer > 0)
			player->growshrinktimer--;
		if (player->growshrinktimer < 0)
			player->growshrinktimer++;

		// Back to normal
		if (player->growshrinktimer == 0)
			K_RemoveGrowShrink(player);
	}

	if (player->superring)
	{
		if (player->superring % 3 == 0)
		{
			mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
			ring->extravalue1 = 1; // Ring collect animation timer
			ring->angle = player->mo->angle; // animation angle
			P_SetTarget(&ring->target, player->mo); // toucher for thinker
			player->pickuprings++;
			if (player->superring <= 3)
				ring->cvmem = 1; // play caching when collected
		}
		player->superring--;
	}
	
	if (player->stealingtimer == 0 && player->stolentimer == 0
		&& player->rocketsneakertimer)
		player->rocketsneakertimer--;

	if (player->hyudorotimer)
		player->hyudorotimer--;

	if (player->sadtimer)
		player->sadtimer--;

	if (player->stealingtimer)
		player->stealingtimer--;

	if (player->stolentimer)
		player->stolentimer--;
	
	if (player->squishedtimer > 0)
		player->squishedtimer--;

	if (player->justbumped > 0)
		player->justbumped--;
	
	if (player->outruntime > 0)
		player->outruntime--;

	K_UpdateTripwire(player);

	K_KartPlayerHUDUpdate(player);

	if (battleovertime.enabled && !(player->pflags & PF_ELIMINATED) && player->bumper <= 0 && player->karmadelay <= 0)
	{
		if (player->overtimekarma)
			player->overtimekarma--;
		else
			P_DamageMobj(player->mo, NULL, NULL, 1, DMG_TIMEOVER);
	}

	if ((battleovertime.enabled >= 10*TICRATE) && !(player->pflags & PF_ELIMINATED) && !player->exiting)
	{
		fixed_t distanceToBarrier = 0;

		if (battleovertime.radius > 0)
		{
			distanceToBarrier = R_PointToDist2(player->mo->x, player->mo->y, battleovertime.x, battleovertime.y) - (player->mo->radius * 2);
		}

		if (distanceToBarrier > battleovertime.radius)
		{
			P_DamageMobj(player->mo, NULL, NULL, 1, DMG_TIMEOVER);
		}
	}

	if (P_IsObjectOnGround(player->mo))
		player->waterskip = 0;

	if (player->instashield)
		player->instashield--;

	if (player->eggmanexplode)
	{
		if (player->spectator || (gametype == GT_BATTLE && !player->bumper))
			player->eggmanexplode = 0;
		else
		{
			player->eggmanexplode--;
			if (player->eggmanexplode <= 0)
			{
				mobj_t *eggsexplode;

				K_KartResetPlayerColor(player);

				//player->flashing = 0;
				eggsexplode = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SPBEXPLOSION);
				if (player->eggmanblame >= 0
				&& player->eggmanblame < MAXPLAYERS
				&& playeringame[player->eggmanblame]
				&& !players[player->eggmanblame].spectator
				&& players[player->eggmanblame].mo)
					P_SetTarget(&eggsexplode->target, players[player->eggmanblame].mo);
			}
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
	
	// Respawn Checker
	if (player->respawn)
		K_RespawnChecker(player);

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

	K_HandleDelayedHitByEm(player);
	
	if (!(gametyperules & GTR_FREEROAM))
		K_RaceStart(player);
	
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
}

void K_KartResetPlayerColor(player_t *player)
{
	boolean fullbright = false;

	if (!player->mo || P_MobjWasRemoved(player->mo)) // Can't do anything
		return;

	if (player->mo->health <= 0 || player->playerstate == PST_DEAD) // Override everything
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
			;
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
		const tic_t defaultTime = itemtime+(2*TICRATE);
		tic_t flicker = 2;
		boolean skip = false;

		fullbright = true;

		player->mo->color = K_RainbowColor(leveltime / 2);
		player->mo->colorized = true;
		skip = true;

		if (skip)
		{
			goto finalise;
		}
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
	else
	{
		player->mo->colorized = (player->dye != 0);
		player->mo->color = player->dye ? player->dye : player->skincolor;
	}

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
	if (player->itemtype == KITEM_JAWZ && (player->pflags & PF_ITEMOUT))
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
			if (P_IsDisplayPlayer(player) || P_IsDisplayPlayer(targ))
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

	if (player->itemtype == KITEM_LIGHTNINGSHIELD)
	{
		K_LookForRings(player->mo);
	}
}

/*--------------------------------------------------
	size_t K_NextRespawnWaypointIndex(waypoint_t *waypoint)
		See header file for description.
--------------------------------------------------*/
size_t K_NextRespawnWaypointIndex(waypoint_t *waypoint)
{
	size_t           i = 0U;
	size_t newwaypoint = SIZE_MAX;

	// Set to the first valid nextwaypoint, for simplicity's sake.
	// If we reach the last waypoint and it's still not valid, just use it anyway. Someone needs to fix their map!
	for (i = 0U; i < waypoint->numnextwaypoints; i++)
	{
		newwaypoint = i;

		if ((i == waypoint->numnextwaypoints - 1U)
		|| ((K_GetWaypointIsEnabled(waypoint->nextwaypoints[newwaypoint]) == true)
		&& (K_GetWaypointIsSpawnpoint(waypoint->nextwaypoints[newwaypoint]) == true)))
		{
			break;
		}
	}

	return newwaypoint;
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
		player->currentwaypoint = bestwaypoint = waypoint;

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

			// Remove WRONG WAY flag.
			player->pflags &= ~PF_WRONGWAY;

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

						if (angledelta < nextbestdelta || momdelta < nextbestmomdelta)
						{
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

		if (!P_IsObjectOnGround(player->mo) || player->respawn)
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
static void K_UpdateDistanceFromFinishLine(player_t *player)
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
				if ((mapheaderinfo[gamemap - 1]->levelflags & LF_SECTIONRACE) == 0U)
				{
					const UINT8 numfulllapsleft = ((UINT8)numlaps - player->laps);
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
	const UINT32 distance_threshold = FixedMul(32768, mapobjectscale);

	waypoint_t *const old_currentwaypoint = player->currentwaypoint;
	waypoint_t *const old_nextwaypoint = player->nextwaypoint;

	boolean updaterespawn = K_SetPlayerNextWaypoint(player);

	// Update prev value (used for grief prevention code)
	K_UpdateDistanceFromFinishLine(player);
	player->distancetofinishprev = player->distancetofinish;

	// Respawning should be a full reset.
	UINT32 delta = u32_delta(player->distancetofinish, player->distancetofinishprev);
	if (!player->respawn && delta > distance_threshold)
	{
		CONS_Debug(DBG_GAMELOGIC, "Player %s: waypoint ID %d too far away (%u > %u)\n",
			sizeu1(player - players), K_GetWaypointID(player->nextwaypoint), delta, distance_threshold);

		// Distance jump is too great, keep the old waypoints and recalculate distance.
		player->currentwaypoint = old_currentwaypoint;
		player->nextwaypoint = old_nextwaypoint;
		player->distancetofinish = player->distancetofinishprev;
	}

	// Respawn point should only be updated when we're going to a nextwaypoint
	if ((updaterespawn) &&
	(!player->respawn) &&
	(player->nextwaypoint != old_nextwaypoint) &&
	(K_GetWaypointIsSpawnpoint(player->nextwaypoint)) &&
	(K_GetWaypointIsEnabled(player->nextwaypoint) == true))
	{
		if (!(player->pflags & PF_WRONGWAY))
			player->grieftime = 0;
		
		// Set time, z, flip and angle first.
		player->starposttime = player->realtime;
		player->starpostz = player->mo->z >> FRACBITS;
		player->starpostflip = (player->mo->eflags & MFE_VERTICALFLIP) ? true : false;
		player->starpostangle = player->mo->angle;
		
		// Then do x and y
		player->starpostx = player->mo->x >> FRACBITS;
		player->starposty = player->mo->y >> FRACBITS;
	}
}

INT32 K_GetKartRingPower(player_t *player, boolean boosted)
{
	INT32 ringPower = ((9 - player->kartspeed) + (9 - player->kartweight)) / 2;

	if (boosted == true && K_PlayerUsesBotMovement(player))
	{
		// Double for Lv. 9
		ringPower += (player->botvars.difficulty * ringPower) / DIFFICULTBOT;
	}

	return ringPower;
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

// countersteer is how strong the controls are telling us we are turning
// turndir is the direction the controls are telling us to turn, -1 if turning right and 1 if turning left
static INT16 K_GetKartDriftValue(player_t *player, fixed_t countersteer)
{
	INT16 basedrift, driftadjust;
	fixed_t driftweight = player->kartweight*14; // 12

	if (player->drift == 0 || !P_IsObjectOnGround(player->mo))
	{
		// If they aren't drifting or on the ground, this doesn't apply
		return 0;
	}

	if (player->pflags & PF_DRIFTEND)
	{
		// Drift has ended and we are tweaking their angle back a bit
		return -266*player->drift;
	}

	basedrift = (83 * player->drift) - (((driftweight - 14) * player->drift) / 5); // 415 - 303
	driftadjust = abs((252 - driftweight) * player->drift / 5);

	return basedrift + (FixedMul(driftadjust * FRACUNIT, countersteer) / FRACUNIT);
}

INT16 K_GetKartTurnValue(player_t *player, INT16 turnvalue)
{
	fixed_t p_topspeed = K_GetKartSpeed(player, false, false);
	fixed_t p_curspeed = min(player->speed, p_topspeed * 2);
	fixed_t p_maxspeed = p_topspeed * 3;
	fixed_t adjustangle = FixedDiv((p_maxspeed>>16) - (p_curspeed>>16), (p_maxspeed>>16) + player->kartweight);

	if (player->spectator)
		return turnvalue;

	if (player->drift != 0 && P_IsObjectOnGround(player->mo))
	{
		// If we're drifting we have a completely different turning value
		if (!(player->pflags & PF_DRIFTEND))
		{
			// 800 is the max set in g_game.c with angleturn
			fixed_t countersteer = FixedDiv(turnvalue*FRACUNIT, 800*FRACUNIT);
			turnvalue = K_GetKartDriftValue(player, countersteer);
		}
		else
			turnvalue = (INT16)(turnvalue + K_GetKartDriftValue(player, FRACUNIT));

		return turnvalue;
	}

	turnvalue = FixedMul(turnvalue, adjustangle); // Weight has a small effect on turning

	if (player->invincibilitytimer || player->sneakertimer || player->growshrinktimer > 0)
		turnvalue = FixedMul(turnvalue, FixedDiv(5*FRACUNIT, 4*FRACUNIT));

	return turnvalue;
}

INT32 K_GetKartDriftSparkValue(player_t *player)
{
	UINT8 kartspeed = ((gametyperules & GTR_KARMA) && player->bumper <= 0)
		? 1
		: player->kartspeed;
	return (26*4 + kartspeed*2 + (9 - player->kartweight))*8;
}

INT32 K_GetKartDriftSparkValueForStage(player_t *player, UINT8 stage)
{
	fixed_t mul = FRACUNIT;

	// This code is function is pretty much useless now that the timing changes are linear but bleh.
	switch (stage)
	{
		case 2:
			mul = 2*FRACUNIT; // x2
			break;
		case 3:
			mul = 3*FRACUNIT; // x3
			break;
		case 4:
			mul = 4*FRACUNIT; // x4
			break;
	}

	return (FixedMul(K_GetKartDriftSparkValue(player) * FRACUNIT, mul) / FRACUNIT);
}

static void K_KartDrift(player_t *player, boolean onground)
{
	fixed_t minspeed = (10 * player->mo->scale);
	fixed_t driftadditive = 24;
	INT32 dsone = K_GetKartDriftSparkValue(player);
	INT32 dstwo = dsone*2;
	INT32 dsthree = dstwo*2;

	// Grown players taking yellow spring panels will go below minspeed for one tic,
	// and will then wrongdrift or have their sparks removed because of this.
	// This fixes this problem.
	if (player->pogospring == 2 && player->mo->scale > mapobjectscale)
		minspeed = FixedMul(10<<FRACBITS, mapobjectscale);

	// Drifting is actually straffing + automatic turning.
	// Holding the Jump button will enable drifting.

	// Drift Release (Moved here so you can't "chain" drifts)
	if ((player->drift != -5 && player->drift != 5)
		&& player->driftcharge < dsone
		&& onground)
	{
		player->driftcharge = 0;
	}
	else if ((player->drift != -5 && player->drift != 5)
		&& (player->driftcharge >= dsone && player->driftcharge < dstwo)
		&& onground)
	{
		if (player->driftboost < 20)
			player->driftboost = 20;
		S_StartSound(player->mo, sfx_s23c);
		player->driftcharge = 0;
	}
	else if ((player->drift != -5 && player->drift != 5)
		&& player->driftcharge < dsthree
		&& onground)
	{
		if (player->driftboost < 50)
			player->driftboost = 50;
		S_StartSound(player->mo, sfx_s23c);
		player->driftcharge = 0;
	}
	else if ((player->drift != -5 && player->drift != 5)
		&& player->driftcharge >= dsthree
		&& onground)
	{
		if (player->driftboost < 125)
			player->driftboost = 125;
		S_StartSound(player->mo, sfx_s23c);
		player->driftcharge = 0;
	}

	// Drifting: left or right?
	if ((player->cmd.turning > 0) && player->speed > minspeed && (player->pflags & PF_DRIFTINPUT) && (player->drift == 0 || (player->pflags & PF_DRIFTEND)))
	{
		// Starting left drift
		player->drift = 1;
		player->pflags &= ~PF_DRIFTEND;
	}
	else if ((player->cmd.turning < 0) && player->speed > minspeed && (player->pflags & PF_DRIFTINPUT) && (player->drift == 0 || (player->pflags & PF_DRIFTEND)))
	{
		// Starting right drift
		player->drift = -1;
		player->pflags &= ~PF_DRIFTEND;
	}
	else if (!(player->pflags & PF_DRIFTINPUT))
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


	// Incease/decrease the drift value to continue drifting in that direction
	if (!P_PlayerInPain(player) && (player->pflags & PF_DRIFTINPUT) && onground && player->drift != 0)
	{
		if (player->drift >= 1) // Drifting to the left
		{
			player->drift++;
			if (player->drift > 5)
				player->drift = 5;

			if (player->cmd.turning > 0) // Inward
				driftadditive += abs(player->cmd.turning)/100;
			if (player->cmd.turning < 0) // Outward
				driftadditive -= abs(player->cmd.turning)/75;
		}
		else if (player->drift <= -1) // Drifting to the right
		{
			player->drift--;
			if (player->drift < -5)
				player->drift = -5;

			if (player->cmd.turning < 0) // Inward
				driftadditive += abs(player->cmd.turning)/100;
			if (player->cmd.turning > 0) // Outward
				driftadditive -= abs(player->cmd.turning)/75;
		}

		// Disable drift-sparks until you're going fast enough
		if (!(player->pflags & PF_GETSPARKS)
			|| (player->offroad && K_ApplyOffroad(player)))
			driftadditive = 0;
		
		if (player->speed > minspeed*2)
			player->pflags |= PF_GETSPARKS;

		// Sound whenever you get a different tier of sparks
		if ((player->driftcharge < dsone && player->driftcharge+driftadditive >= dsone)
			|| (player->driftcharge < dstwo && player->driftcharge+driftadditive >= dstwo)
			|| (player->driftcharge < dsthree && player->driftcharge+driftadditive >= dsthree))
		{
			if (P_IsDisplayPlayer(player)) // UGHGHGH...
				S_StartSoundAtVolume(player->mo, sfx_s3ka2, 192); // Ugh...
		}
		
		player->driftcharge += driftadditive;
		player->pflags &= ~PF_DRIFTEND;
	}
	
	// Spawn Sparks regardless of size
	if (!P_PlayerInPain(player) && player->drift != 0)
	{
		if (player->driftcharge >= dsone)
			K_SpawnDriftSparks(player);
	}

	// Stop drifting
	if (P_PlayerInPain(player) || player->speed < minspeed)
	{
		player->drift = player->driftcharge = player->aizdriftstrat = 0;
		player->pflags &= ~(PF_BRAKEDRIFT|PF_GETSPARKS);
	}

	if ((player->sneakertimer == 0)
	|| (!stplyr->cmd.turning)
	|| (!player->aizdriftstrat)
	|| (stplyr->cmd.turning > 0) != (player->aizdriftstrat > 0))
	{
		if (!player->drift)
			player->aizdriftstrat = 0;
		else
			player->aizdriftstrat = ((player->drift > 0) ? 1 : -1);
	}
	else if (player->aizdriftstrat && !player->drift)
	{
		K_SpawnAIZDust(player);

		if (abs(player->aizdrifttilt) < ANGLE_22h)
		{
			player->aizdrifttilt =
				(abs(player->aizdrifttilt) + ANGLE_11hh / 4) *
				player->aizdriftstrat;
		}

		if (abs(player->aizdriftturn) < ANGLE_112h)
		{
			player->aizdriftturn =
				(abs(player->aizdriftturn) + ANGLE_11hh) *
				player->aizdriftstrat;
		}
	}

	if (!K_Sliptiding(player))
	{
		player->aizdrifttilt -= player->aizdrifttilt / 4;
		player->aizdriftturn -= player->aizdriftturn / 4;

		if (abs(player->aizdrifttilt) < ANGLE_11hh / 4)
			player->aizdrifttilt = 0;
		if (abs(player->aizdriftturn) < ANGLE_11hh)
			player->aizdriftturn = 0;
	}

	if (player->drift
		&& ((player->cmd.buttons & BT_BRAKE)
		|| !(player->cmd.buttons & BT_ACCELERATE))
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

	if (player->spectator || !player->mo)
	{
		// Ensure these are reset for spectators
		player->position = 0;
		player->positiondelay = 0;
		return;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;

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

				if (yourEmeralds > myEmeralds)
				{
					// Emeralds matter above all
					position++;
				}
				else if (yourEmeralds == myEmeralds)
				{
					// Bumpers are a tie breaker
					if (players[i].bumper > player->bumper)
					{
						position++;
					}
					else if (players[i].bumper == player->bumper)
					{
						// Score is the second tier tie breaker
						if (players[i].roundscore > player->roundscore)
						{
							position++;
						}
					}
				}
			}
		}
	}

	if (leveltime < starttime || oldposition == 0)
		oldposition = position;

	if (oldposition != position) // Changed places?
		player->positiondelay = 10; // Position number growth

	player->position = position;
}

void K_KartLegacyUpdatePosition(player_t *player)
{
	fixed_t position = 1;
	fixed_t oldposition = player->position;
	fixed_t i, ppcd, pncd, ipcd, incd;
	fixed_t pmo, imo;
	mobj_t *mo;

	if (player->spectator || !player->mo)
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;

		if (gametyperules & GTR_CIRCUIT)
		{
			if ((((players[i].starpostnum) + (numstarposts + 1) * players[i].laps) >
				((player->starpostnum) + (numstarposts + 1) * player->laps)))
				position++;
			else if (((players[i].starpostnum) + (numstarposts+1)*players[i].laps) ==
				((player->starpostnum) + (numstarposts+1)*player->laps))
			{
				ppcd = pncd = ipcd = incd = 0;

				player->prevcheck = players[i].prevcheck = 0;
				player->nextcheck = players[i].nextcheck = 0;

				// This checks every thing on the map, and looks for MT_BOSS3WAYPOINT (the thing we're using for checkpoint wp's, for now)
				for (mo = boss3cap; mo != NULL; mo = mo->tracer)
				{
					pmo = P_AproxDistance(P_AproxDistance(	mo->x - player->mo->x,
															mo->y - player->mo->y),
															mo->z - player->mo->z) / FRACUNIT;
					imo = P_AproxDistance(P_AproxDistance(	mo->x - players[i].mo->x,
															mo->y - players[i].mo->y),
															mo->z - players[i].mo->z) / FRACUNIT;

					if (mo->health == player->starpostnum && (!mo->movecount || mo->movecount == player->laps+1))
					{
						player->prevcheck += pmo;
						ppcd++;
					}
					if (mo->health == (player->starpostnum + 1) && (!mo->movecount || mo->movecount == player->laps+1))
					{
						player->nextcheck += pmo;
						pncd++;
					}
					if (mo->health == players[i].starpostnum && (!mo->movecount || mo->movecount == players[i].laps+1))
					{
						players[i].prevcheck += imo;
						ipcd++;
					}
					if (mo->health == (players[i].starpostnum + 1) && (!mo->movecount || mo->movecount == players[i].laps+1))
					{
						players[i].nextcheck += imo;
						incd++;
					}
				}

				if (ppcd > 1) player->prevcheck /= ppcd;
				if (pncd > 1) player->nextcheck /= pncd;
				if (ipcd > 1) players[i].prevcheck /= ipcd;
				if (incd > 1) players[i].nextcheck /= incd;

				if ((players[i].nextcheck > 0 || player->nextcheck > 0) && !player->exiting)
				{
					if ((players[i].nextcheck - players[i].prevcheck) <
						(player->nextcheck - player->prevcheck))
						position++;
				}
				else if (!player->exiting)
				{
					if (players[i].prevcheck > player->prevcheck)
						position++;
				}
				else
				{
					if (players[i].starposttime < player->starposttime)
						position++;
				}
			}
		}
	}

	if (leveltime < starttime || oldposition == 0)
		oldposition = position;

	if (oldposition != position) // Changed places?
		player->positiondelay = 10; // Position number growth

	player->position = position;
}

void K_UpdateAllPlayerPositions(void)
{
	INT32 i;
	// First loop: Ensure all players' distance to the finish line are all accurate
	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *player = &players[i];
		if (!playeringame[i] || player->spectator || !player->mo || P_MobjWasRemoved(player->mo))
		{
			continue;
		}

		K_UpdatePlayerWaypoints(player);
	}

	// Second loop: Ensure all player positions reflect everyone's distances
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo))
		{
			if (numbosswaypoints > 0)
				K_KartLegacyUpdatePosition(&players[i]);
			else
				K_KartUpdatePosition(&players[i]);
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
	player->pflags &= ~(PF_ITEMOUT|PF_EGGMANOUT);

	if (!player->itemroulette || player->roulettetype != 2)
	{
		player->itemroulette = 0;
		player->roulettetype = 0;
	}

	player->hyudorotimer = 0;
	player->stealingtimer = 0;
	player->stolentimer = 0;

	player->curshield = KSHIELD_NONE;
	player->bananadrag = 0;

	player->sadtimer = 0;

	K_UpdateHnextList(player, true);
}

void K_StripOther(player_t *player)
{
	player->itemroulette = 0;
	player->roulettetype = 0;

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

static INT32 K_FlameShieldMax(player_t *player)
{
	UINT32 disttofinish = 0;
	UINT32 distv = DISTVAR;
	UINT8 numplayers = 0;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator)
			numplayers++;
		if (players[i].position == 1)
			disttofinish = players[i].distancetofinish;
	}

	if (numplayers <= 1 || gametype == GT_BATTLE)
	{
		return 16; // max when alone, for testing
		// and when in battle, for chaos
	}
	else if (player->position == 1)
	{
		return 0; // minimum for first
	}

	disttofinish = player->distancetofinish - disttofinish;
	distv = FixedMul(distv * FRACUNIT, mapobjectscale) / FRACUNIT;
	return min(16, 1 + (disttofinish / distv));
}

SINT8 K_Sliptiding(player_t *player)
{
	return player->drift ? 0 : player->aizdriftstrat;
}

static void K_AirFailsafe(player_t *player)
{
	const fixed_t maxSpeed = 6*player->mo->scale;
	const fixed_t thrustSpeed = 6*player->mo->scale; // 10*player->mo->scale

	if (player->speed > maxSpeed // Above the max speed that you're allowed to use this technique.
		|| player->respawn) // Respawning, you don't need this AND drop dash :V
	{
		player->pflags &= ~PF_AIRFAILSAFE;
		return;
	}

	if ((K_GetKartButtons(player) & BT_ACCELERATE) || player->cmd.forwardmove != 0)
	{
		// Queue up later
		player->pflags |= PF_AIRFAILSAFE;
		return;
	}

	if (player->pflags & PF_AIRFAILSAFE)
	{
		// Push the player forward
		P_Thrust(player->mo, K_MomentumAngle(player->mo), thrustSpeed);

		S_StartSound(player->mo, sfx_s23c);
		K_SpawnNormalSpeedLines(player);

		player->pflags &= ~PF_AIRFAILSAFE;
	}
}

//

//
static void K_AdjustPlayerFriction(player_t *player)
{
	boolean onground = P_IsObjectOnGround(player->mo);
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
		if ((gametyperules & GTR_KARMA) && player->bumper <= 0)
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
	player->pflags |= PF_ITEMOUT;
}

void K_UnsetItemOut(player_t *player)
{
	player->pflags &= ~PF_ITEMOUT;
	player->bananadrag = 0;
}

//
// K_MoveKartPlayer
//
void K_MoveKartPlayer(player_t *player, boolean onground)
{
	ticcmd_t *cmd = &player->cmd;
	boolean ATTACK_IS_DOWN = ((cmd->buttons & BT_ATTACK) && !(player->oldcmd.buttons & BT_ATTACK));
	boolean HOLDING_ITEM = (player->pflags & (PF_ITEMOUT|PF_EGGMANOUT));
	boolean NO_HYUDORO = (player->stealingtimer == 0 && player->stolentimer == 0);

	if (!player->exiting)
	{
		if (player->oldposition < player->position) // But first, if you lost a place,
		{
			player->oldposition = player->position; // then the other player taunts.
			K_RegularVoiceTimers(player); // and you can't for a bit
		}
		else if (player->oldposition > player->position) // Otherwise,
		{
			K_PlayOvertakeSound(player->mo); // Say "YOU'RE TOO SLOW!"
			player->oldposition = player->position; // Restore the old position,
		}
	}

	if (player->positiondelay)
		player->positiondelay--;

	// Prevent ring misfire
	if (!(cmd->buttons & BT_ATTACK))
	{
		if (player->itemtype == KITEM_NONE
			&& NO_HYUDORO && !ringsdisabled && !(HOLDING_ITEM
			|| player->itemamount
			|| player->itemroulette
			|| player->rocketsneakertimer
			|| player->eggmanexplode
			|| (player->growshrinktimer > 0)))
			player->pflags |= PF_USERINGS;
		else
			player->pflags &= ~PF_USERINGS;
	}

	if (player && player->mo && player->mo->health > 0 && !player->spectator && !P_PlayerInPain(player) && !(player->exiting || mapreset) && leveltime > introtime)
	{
		// First, the really specific, finicky items that function without the item being directly in your item slot.
		{
			// Ring boosting
			if (player->pflags & PF_USERINGS)
			{
				if ((cmd->buttons & BT_ATTACK) && !player->ringdelay && player->rings > 0)
				{
					mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
					P_SetMobjState(ring, S_FASTRING1);
					ring->extravalue1 = 1; // Ring use animation timer
					ring->extravalue2 = 1; // Ring use animation flag
					ring->shadowscale = 0;
					P_SetTarget(&ring->target, player->mo); // user
					player->rings--;
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
						player->eggmanexplode = 1;
				}
				// Eggman Monitor throwing
				else if (player->pflags & PF_EGGMANOUT)
				{
					if (ATTACK_IS_DOWN)
					{
						K_ThrowKartItem(player, false, MT_EGGMANITEM, -1, 0);
						K_PlayAttackTaunt(player->mo);
						player->pflags &= ~PF_EGGMANOUT;
						K_UpdateHnextList(player, true);
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
					}
				}
				// Grow Canceling
				else if (player->growshrinktimer > 0)
				{
					if (player->growcancel >= 0)
					{
						if (cmd->buttons & BT_ATTACK)
						{
							player->growcancel++;
							if (player->growcancel > 26)
								K_RemoveGrowShrink(player);
						}
						else
							player->growcancel = 0;
					}
					else
					{
						if ((cmd->buttons & BT_ATTACK) || (player->oldcmd.buttons & BT_ATTACK))
							player->growcancel = -1;
						else
							player->growcancel = 0;
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
							}
							break;
						case KITEM_INVINCIBILITY:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO) // Doesn't hold your item slot hostage normally, so you're free to waste it if you have multiple
							{
								K_DoInvincibility(player, 10 * TICRATE);
								K_PlayPowerGloatSound(player->mo);
								player->itemamount--;
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
							}
							else if (ATTACK_IS_DOWN && (player->pflags & PF_ITEMOUT)) // Banana x3 thrown
							{
								K_ThrowKartItem(player, false, MT_BANANA, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_EGGMAN:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->itemamount--;
								player->pflags |= PF_EGGMANOUT;
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
							}
							else if (ATTACK_IS_DOWN && (player->pflags & PF_ITEMOUT)) // Orbinaut x3 thrown
							{
								K_ThrowKartItem(player, true, MT_ORBINAUT, 1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
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
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && (player->pflags & PF_ITEMOUT)) // Jawz thrown
							{
								if (player->throwdir == 1 || player->throwdir == 0)
									K_ThrowKartItem(player, true, MT_JAWZ, 1, 0);
								else if (player->throwdir == -1) // Throwing backward gives you a dud that doesn't home in
									K_ThrowKartItem(player, true, MT_JAWZ_DUD, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								K_UpdateHnextList(player, false);
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
							}
							else if (ATTACK_IS_DOWN && (player->pflags & PF_ITEMOUT))
							{
								K_ThrowKartItem(player, false, MT_SSMINE, 1, 1);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								player->pflags &= ~PF_ITEMOUT;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_LANDMINE:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->itemamount--;
								K_ThrowLandMine(player);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_DROPTARGET:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								K_SetItemOut(player);
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_DROPTARGET_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							else if (ATTACK_IS_DOWN && (player->pflags & PF_ITEMOUT))
							{
								K_ThrowKartItem(player, (player->throwdir > 0), MT_DROPTARGET, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								player->pflags &= ~PF_ITEMOUT;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_BALLHOG:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->itemamount--;
								K_ThrowKartItem(player, true, MT_BALLHOG, 1, 0);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_SPB:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->itemamount--;
								K_ThrowKartItem(player, true, MT_SPB, 1, 0);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_GROW:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								if (player->growshrinktimer < 0)
								{
									// If you're shrunk, then "grow" will just make you normal again.
									K_RemoveGrowShrink(player);
								}
								else
								{
									K_PlayPowerGloatSound(player->mo);

									player->mo->scalespeed = mapobjectscale/TICRATE;
									player->mo->destscale = FixedMul(mapobjectscale, GROW_SCALE);

									if (K_PlayerShrinkCheat(player) == true)
									{
										player->mo->destscale = FixedMul(player->mo->destscale, SHRINK_SCALE);
									}

									// TODO: gametyperules
									player->growshrinktimer = (gametype == GT_BATTLE ? 8 : 12) * TICRATE;

									if (player->invincibilitytimer > 0)
									{
										; // invincibility has priority in P_RestoreMusic, no point in starting here
									}
									else if (P_IsLocalPlayer(player) == true)
									{
										S_ChangeMusicSpecial("kgrow");
									}
									else //used to be "if (P_IsDisplayPlayer(player) == false)"
									{
										S_StartSound(player->mo, (cv_kartinvinsfx.value ? sfx_alarmg : sfx_kgrow));
									}

									P_RestoreMusic(player);
									S_StartSound(player->mo, sfx_kc5a);
								}

								player->itemamount--;
							}
							break;
						case KITEM_SHRINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoShrink(player);
								player->itemamount--;
								K_PlayPowerGloatSound(player->mo);
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
								if ((cmd->buttons & BT_ATTACK) && (player->pflags & PF_HOLDREADY))
								{
									if (player->bubbleblowup == 0)
										S_StartSound(player->mo, sfx_s3k75);

									player->bubbleblowup++;
									player->bubblecool = player->bubbleblowup*4;

									if (player->bubbleblowup > bubbletime*2)
									{
										K_ThrowKartItem(player, (player->throwdir > 0), MT_BUBBLESHIELDTRAP, -1, 0);
										K_PlayAttackTaunt(player->mo);
										player->bubbleblowup = 0;
										player->bubblecool = 0;
										player->pflags &= ~PF_HOLDREADY;
										player->itemamount--;
									}
								}
								else
								{
									if (player->bubbleblowup > bubbletime)
										player->bubbleblowup = bubbletime;

									if (player->bubbleblowup)
										player->bubbleblowup--;

									if (player->bubblecool)
										player->pflags &= ~PF_HOLDREADY;
									else
										player->pflags |= PF_HOLDREADY;
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
									player->flamelength++; // Can always go up!

								flamemax = player->flamelength * flameseg;
								if (flamemax > 0)
									flamemax += TICRATE; // leniency period

								if ((cmd->buttons & BT_ATTACK) && (player->pflags & PF_HOLDREADY))
								{
									// TODO: gametyperules
									const INT32 incr = gametype == GT_BATTLE ? 4 : 2;

									if (player->flamedash == 0)
									{
										S_StartSound(player->mo, sfx_s3k43);
										K_PlayBoostTaunt(player->mo);
									}

									player->flamedash += incr;
									player->flamemeter += incr;

									if (!onground)
									{
										P_Thrust(
											player->mo, K_MomentumAngle(player->mo),
											FixedMul(player->mo->scale, K_GetKartGameSpeedScalar(gamespeed))
										);
									}

									if (player->flamemeter > flamemax)
									{
										P_Thrust(
											player->mo, player->mo->angle,
											FixedMul((50*player->mo->scale), K_GetKartGameSpeedScalar(gamespeed))
										);

										player->flamemeter = 0;
										player->flamelength = 0;
										player->pflags &= ~PF_HOLDREADY;
										player->itemamount--;
									}
								}
								else
								{
									player->pflags |= PF_HOLDREADY;

									// TODO: gametyperules
									if (gametype != GT_BATTLE || leveltime % 6 == 0)
									{
										if (player->flamemeter > 0)
											player->flamemeter--;
									}

									if (player->flamelength > destlen)
									{
										player->flamelength--; // Can ONLY go down if you're not using it

										flamemax = player->flamelength * flameseg;
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
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_POGOSPRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO && player->pogospring == 0)
							{
								K_PlayBoostTaunt(player->mo);
								K_DoPogoSpring(player->mo, 32<<FRACBITS, 2);
								P_SpawnMobjFromMobj(player->mo, 0, 0, 0, MT_POGOSPRING);
								player->pogospring = 1;
								player->itemamount--;
							}
							break;
						case KITEM_SUPERRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_AwardPlayerRings(player, 15, true);
								player->itemamount--;
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
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && (player->pflags & PF_ITEMOUT)) // Sink thrown
							{
								K_ThrowKartItem(player, false, MT_SINK, 1, 2);
								K_PlayAttackTaunt(player->mo);
								player->itemamount--;
								player->pflags &= ~PF_ITEMOUT;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_SAD:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO
								&& !player->sadtimer)
							{
								player->sadtimer = stealtime;
								player->itemamount--;
							}
							break;
						default:
							break;
					}
				}
			}
		}
	}

		// No more!
		if (!player->itemamount)
		{
			player->pflags &= ~PF_ITEMOUT;
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
			player->pflags &= ~PF_RINGLOCK; // reset ring lock
			
		if (player->growshrinktimer <= 0)
			player->growcancel = -1;

		if (player->itemtype == KITEM_SPB
			|| player->itemtype == KITEM_SHRINK
			|| player->growshrinktimer < 0)
			indirectitemcooldown = 20*TICRATE;

		if (player->hyudorotimer > 0)
		{
			INT32 hyu = hyudorotime;

			if (gametype == GT_RACE)
				hyu *= 2; // double in race

			if (leveltime & 1)
			{
				player->mo->renderflags |= RF_DONTDRAW;
			}
			else
			{
				if (player->hyudorotimer >= (TICRATE/2) && player->hyudorotimer <= hyu-(TICRATE/2))
					player->mo->renderflags &= ~K_GetPlayerDontDrawFlag(player);
				else
					player->mo->renderflags &= ~RF_DONTDRAW;
			}

			player->flashing = player->hyudorotimer; // We'll do this for now, let's people know about the invisible people through subtle hints
		}
		else if (player->hyudorotimer == 0)
		{
			player->mo->renderflags &= ~RF_DONTDRAW;
		}

		if (gametype == GT_BATTLE && player->bumper <= 0) // dead in match? you da bomb
		{
			K_DropItems(player); //K_StripItems(player);
			K_StripOther(player);
			player->mo->renderflags |= RF_GHOSTLY;
			player->flashing = player->karmadelay;
		}
		else if (gametype == GT_RACE || player->bumper > 0)
		{
			player->mo->renderflags &= ~(RF_TRANSMASK|RF_BRIGHTMASK);
		}
		
	K_AdjustPlayerFriction(player);

	K_KartDrift(player, onground);
	
	// Quick Turning
	// You can't turn your kart when you're not moving.
	// So now it's time to burn some rubber!
	if (player->speed < 2 && leveltime > starttime && player->cmd.buttons & BT_ACCELERATE && player->cmd.buttons & BT_BRAKE && player->cmd.turning != 0)
	{
		if (leveltime % 8 == 0)
			S_StartSound(player->mo, sfx_s224);
	}

	if (onground == false)
	{
		K_AirFailsafe(player);
	}
	else
	{
		player->pflags &= ~PF_AIRFAILSAFE;
	}
}

void K_CheckSpectateStatus(void)
{
	UINT8 respawnlist[MAXPLAYERS];
	UINT8 i, j, numingame = 0, numjoiners = 0;
	UINT8 previngame = 0;

	// Maintain spectate wait timer
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
			
		if (players[i].spectator && (players[i].pflags & PF_WANTSTOJOIN))
			players[i].spectatewait++;
		else
			players[i].spectatewait = 0;
		
		if (gamestate != GS_LEVEL)
			players[i].spectatorreentry = 0;
		else if (players[i].spectatorreentry > 0)
			players[i].spectatorreentry--;		
	}

	// No one's allowed to join
	if (!cv_allowteamchange.value)
		return;

	// Get the number of players in game, and the players to be de-spectated.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		if (!players[i].spectator)
		{
			numingame++;
			// DON'T allow if you've hit the in-game player cap
			if (cv_ingamecap.value && numingame >= cv_ingamecap.value)
				return;
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
			if (gametype == GT_RACE && players[i].laps >= 2) // DON'T allow if the race is at 2 laps
				return;
			continue;
		}
		else if (players[i].bot || !(players[i].pflags & PF_WANTSTOJOIN))
		{
			// This spectator does not want to join.
			continue;
		}

		respawnlist[numjoiners++] = i;
	}
	
	// The map started as a legitimate race, but there's still the one player.
	// Don't allow new joiners, as they're probably a ragespeccer.
	if ((gametyperules & GTR_CIRCUIT) && startedInFreePlay == false && numingame == 1)
	{
		return;
	}


	// literally zero point in going any further if nobody is joining
	if (!numjoiners)
		return;

	// Organize by spectate wait timer
#if 0
	if (cv_ingamecap.value)
#endif
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

	// Finally, we can de-spectate everyone in the list!
	previngame = numingame;

	for (i = 0; i < numjoiners; i++)
	{
		// Hit the in-game player cap while adding people?
		if (cv_ingamecap.value && numingame+i >= cv_ingamecap.value) 
			break;
		
		// This person has their reentry cooldown active.
		if (players[i].spectatorreentry > 0 && numingame > 0)
			continue;
		
		//CONS_Printf("player %s is joining on tic %d\n", player_names[respawnlist[i]], leveltime);
		P_SpectatorJoinGame(&players[respawnlist[i]]);
		numingame++;
	}

	// Reset the match if you're in an empty server
	if (!mapreset && gamestate == GS_LEVEL && (previngame < 2 && numingame >= 2))
	{
		S_ChangeMusicInternal("chalng", false); // COME ON
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
	UINT8 i;
	thinker_t *think;

	// is there an SPB chasing anyone?
	if (spbplace != -1)
		return true;

	// do any players have an SPB in their item slot?
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (players[i].itemtype == KITEM_SPB)
			return true;
	}

	// spbplace is still -1 until a fired SPB finds a target, so look for an in-map SPB just in case
	for (think = thlist[THINK_MOBJ].next; think != &thlist[THINK_MOBJ]; think = think->next)
	{
		if (think->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		if (((mobj_t *)think)->type == MT_SPB)
			return true;
	}

	return false;
}

//}
