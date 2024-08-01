// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_user.c
/// \brief New stuff?
///        Player related stuff.
///        Bobbing POV/weapon, movement.
///        Pending weapon.

#include "d_player.h"
#include "doomdef.h"
#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "g_game.h"
#include "info.h"
#include "p_local.h"
#include "r_fps.h"
#include "r_main.h"
#include "s_sound.h"
#include "r_skins.h"
#include "d_think.h"
#include "r_sky.h"
#include "p_setup.h"
#include "m_random.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_joy.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "r_splats.h"
#include "z_zone.h"
#include "w_wad.h"
#include "hu_stuff.h"
// We need to affect the NiGHTS hud
#include "st_stuff.h"
#include "lua_script.h"
#include "lua_hook.h"
// Objectplace
#include "m_cheat.h"
// Thok camera snap (ctrl-f "chalupa")
#include "g_input.h"

// SRB2kart
#include "m_cond.h" // M_UpdateUnlockablesAndExtraEmblems
#include "k_kart.h"
#include "console.h" // CON_LogMessage
#include "k_respawn.h"
#include "k_bot.h"
#include "k_grandprix.h"
#include "k_boss.h"
#include "k_terrain.h" // K_SpawnSplashForMobj
#include "k_color.h"
#include "k_follower.h"

#ifdef HW3SOUND
#include "hardware/hw3sound.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_light.h"
#include "hardware/hw_main.h"
#endif

#if 0
static void P_NukeAllPlayers(player_t *player);
#endif

//
// Jingle stuff.
//

jingle_t jingleinfo[NUMJINGLES] = {
	// {musname, looping, reset, nest}
	{""        , false}, // JT_NONE
	{""        , false}, // JT_OTHER
	{""        , false}, // JT_MASTER
	{"_1up"    , false},
	{"_shoes"  ,  true},
	{"_inv"    , false},
	{"_minv"   , false},
	{"_drown"  , false},
	{"_super"  ,  true},
	{"_gover"  , false},
	{"_ntime"  , false},  // JT_NIGHTSTIMEOUT
	{"_drown"  , false}   // JT_SSTIMEOUT
	// {"_clear"  , false},
	// {"_inter"  ,  true},
	// {"_conti"  ,  true}
};

//
// Movement.
//

// 16 pixels of bob
//#define MAXBOB (0x10 << FRACBITS)

static boolean onground;

//
// P_Thrust
// Moves the given origin along a given angle.
//
void P_Thrust(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	mo->momx += FixedMul(move, FINECOSINE(angle));
	mo->momy += FixedMul(move, FINESINE(angle));
}

#if 0
static inline void P_VectorInstaThrust(fixed_t xa, fixed_t xb, fixed_t xc, fixed_t ya, fixed_t yb, fixed_t yc,
	fixed_t za, fixed_t zb, fixed_t zc, fixed_t momentum, mobj_t *mo)
{
	fixed_t a1, b1, c1, a2, b2, c2, i, j, k;

	a1 = xb - xa;
	b1 = yb - ya;
	c1 = zb - za;
	a2 = xb - xc;
	b2 = yb - yc;
	c2 = zb - zc;
/*
	// Convert to unit vectors...
	a1 = FixedDiv(a1,FixedSqrt(FixedMul(a1,a1) + FixedMul(b1,b1) + FixedMul(c1,c1)));
	b1 = FixedDiv(b1,FixedSqrt(FixedMul(a1,a1) + FixedMul(b1,b1) + FixedMul(c1,c1)));
	c1 = FixedDiv(c1,FixedSqrt(FixedMul(c1,c1) + FixedMul(c1,c1) + FixedMul(c1,c1)));

	a2 = FixedDiv(a2,FixedSqrt(FixedMul(a2,a2) + FixedMul(c2,c2) + FixedMul(c2,c2)));
	b2 = FixedDiv(b2,FixedSqrt(FixedMul(a2,a2) + FixedMul(c2,c2) + FixedMul(c2,c2)));
	c2 = FixedDiv(c2,FixedSqrt(FixedMul(a2,a2) + FixedMul(c2,c2) + FixedMul(c2,c2)));
*/
	// Calculate the momx, momy, and momz
	i = FixedMul(momentum, FixedMul(b1, c2) - FixedMul(c1, b2));
	j = FixedMul(momentum, FixedMul(c1, a2) - FixedMul(a1, c2));
	k = FixedMul(momentum, FixedMul(a1, b2) - FixedMul(a1, c2));

	mo->momx = i;
	mo->momy = j;
	mo->momz = k;
}
#endif

//
// P_InstaThrust
// Moves the given origin along a given angle instantly.
//
// FIXTHIS: belongs in another file, not here
//
void P_InstaThrust(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	mo->momx = FixedMul(move, FINECOSINE(angle));
	mo->momy = FixedMul(move,FINESINE(angle));
}

// Returns a location (hard to explain - go see how it is used)
fixed_t P_ReturnThrustX(mobj_t *mo, angle_t angle, fixed_t move)
{
	(void)mo;
	angle >>= ANGLETOFINESHIFT;
	return FixedMul(move, FINECOSINE(angle));
}
fixed_t P_ReturnThrustY(mobj_t *mo, angle_t angle, fixed_t move)
{
	(void)mo;
	angle >>= ANGLETOFINESHIFT;
	return FixedMul(move, FINESINE(angle));
}

//
// P_AutoPause
// Returns true when gameplay should be halted even if the game isn't necessarily paused.
//
boolean P_AutoPause(void)
{
	// Don't pause even on menu-up or focus-lost in netgames or record attack
	if (netgame || modeattacking || gamestate == GS_TITLESCREEN)
		return false;

	return ((menuactive && !demo.playback) || ( window_notinfocus && cv_pauseifunfocused.value ));
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight(player_t *player)
{
	fixed_t bob = 0;
	fixed_t pviewheight;
	mobj_t *mo = player->mo;
	fixed_t bobmul = FRACUNIT - FixedDiv(FixedHypot(player->rmomx, player->rmomy), K_GetKartSpeed(player, false, false));

	// Regular movement bobbing.
	// Should not be calculated when not on ground (FIXTHIS?)
	// OPTIMIZE: tablify angle
	// Note: a LUT allows for effects
	//  like a ramp with low health.

	if (bobmul > FRACUNIT) bobmul = FRACUNIT;
	if (bobmul < FRACUNIT/8) bobmul = FRACUNIT/8;

	player->bob = FixedMul(cv_movebob.value, bobmul);

	if (!P_IsObjectOnGround(mo) || player->spectator)
	{
		if (mo->eflags & MFE_VERTICALFLIP)
		{
			player->viewz = mo->z + mo->height - player->viewheight;
			if (player->viewz < mo->floorz + FixedMul(FRACUNIT, mo->scale))
				player->viewz = mo->floorz + FixedMul(FRACUNIT, mo->scale);
		}
		else
		{
			player->viewz = mo->z + player->viewheight;
			if (player->viewz > mo->ceilingz - FixedMul(FRACUNIT, mo->scale))
				player->viewz = mo->ceilingz - FixedMul(FRACUNIT, mo->scale);
		}
		return;
	}

	// First person move bobbing in Kart is more of a "move jitter", to match how a go-kart would feel :p
	if (leveltime & 1)
	{
		bob = player->bob;
	}

	// move viewheight
	pviewheight = P_GetPlayerViewHeight(player); // default eye view height

	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > pviewheight)
		{
			player->viewheight = pviewheight;
			player->deltaviewheight = 0;
		}

		if (player->viewheight < pviewheight/2)
		{
			player->viewheight = pviewheight/2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}

		if (player->deltaviewheight)
		{
			player->deltaviewheight += FixedMul(FRACUNIT/4, mo->scale);
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	if (player->mo->eflags & MFE_VERTICALFLIP)
		player->viewz = mo->z + mo->height - player->viewheight - bob;
	else
		player->viewz = mo->z + player->viewheight + bob;

	if (player->viewz > mo->ceilingz-FixedMul(4*FRACUNIT, mo->scale))
		player->viewz = mo->ceilingz-FixedMul(4*FRACUNIT, mo->scale);
	if (player->viewz < mo->floorz+FixedMul(4*FRACUNIT, mo->scale))
		player->viewz = mo->floorz+FixedMul(4*FRACUNIT, mo->scale);
}

/** Decides if a player is moving.
  * \param pnum The player number to test.
  * \return True if the player is considered to be moving.
  * \author Graue <graue@oceanbase.org>
  */
boolean P_PlayerMoving(INT32 pnum)
{
	player_t *p = &players[pnum];

	if (!Playing())
		return false;

	if (p->jointime < 5*TICRATE || p->playerstate == PST_DEAD || p->playerstate == PST_REBORN || p->spectator)
		return false;

	return gamestate == GS_LEVEL && p->mo && p->mo->health > 0
		&& (abs(p->rmomx) >= FixedMul(FRACUNIT/2, p->mo->scale)
			|| abs(p->rmomy) >= FixedMul(FRACUNIT/2, p->mo->scale)
			|| abs(p->mo->momz) >= FixedMul(FRACUNIT/2, p->mo->scale));
}

// P_GetNextEmerald
//
// Gets the number (0 based) of the next emerald to obtain
//
UINT8 P_GetNextEmerald(void)
{
	if (gamemap >= sstage_start && gamemap <= sstage_end)
		return (UINT8)(gamemap - sstage_start);
	if (gamemap >= smpstage_start || gamemap <= smpstage_end)
		return (UINT8)(gamemap - smpstage_start);
	return 0;
}

//
// P_GiveEmerald
//
// Award an emerald upon completion
// of a special stage.
//
void P_GiveEmerald(boolean spawnObj)
{
	UINT8 em = P_GetNextEmerald();

	S_StartSound(NULL, sfx_cgot); // Got the emerald!
	emeralds |= (1 << em);
	stagefailed = false;

	if (spawnObj)
	{
		// The Chaos Emerald begins to orbit us!
		// Only visibly give it to ONE person!
		UINT8 i, pnum = ((playeringame[consoleplayer]) && (!players[consoleplayer].spectator) && (players[consoleplayer].mo)) ? consoleplayer : 255;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			mobj_t *emmo;
			if (!playeringame[i])
				continue;
			if (players[i].spectator)
				continue;
			if (!players[i].mo)
				continue;

			emmo = P_SpawnMobjFromMobj(players[i].mo, 0, 0, players[i].mo->height, MT_GOTEMERALD);
			if (!emmo)
				continue;
			P_SetTarget(&emmo->target, players[i].mo);
			P_SetMobjState(emmo, mobjinfo[MT_GOTEMERALD].meleestate + em);

			// Make sure we're not being carried before our tracer is changed
			players[i].carry = CR_NONE;

			P_SetTarget(&players[i].mo->tracer, emmo);

			if (pnum == 255)
			{
				pnum = i;
				continue;
			}

			if (i == pnum)
				continue;

			emmo->flags2 |= RF_DONTDRAW;
		}
	}
}

//
// P_GiveFinishFlags
//
// Give the player visual indicators
// that they've finished the map.
//
void P_GiveFinishFlags(player_t *player)
{
	angle_t angle = FixedAngle(player->mo->angle << FRACBITS);
	UINT8 i;

	if (!player->mo)
		return;

	if (!(netgame||multiplayer))
		return;

	for (i = 0; i < 3; i++)
	{
		angle_t fa = (angle >> ANGLETOFINESHIFT) & FINEMASK;
		fixed_t xoffs = FINECOSINE(fa);
		fixed_t yoffs = FINESINE(fa);
		mobj_t* flag = P_SpawnMobjFromMobj(player->mo, xoffs, yoffs, 0, MT_FINISHFLAG);
		P_InitAngle(flag, angle);
		angle += FixedAngle(120*FRACUNIT);

		P_SetTarget(&flag->target, player->mo);
	}
}

//
// P_FindLowestLap
//
// SRB2Kart, a similar function as above for finding the lowest lap
//
UINT8 P_FindLowestLap(void)
{
	INT32 i;
	UINT8 lowest = UINT8_MAX;

	if (!(gametyperules & GTR_CIRCUIT))
		return 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (lowest == UINT8_MAX || players[i].laps < lowest)
		{
			lowest = players[i].laps;
		}
	}

	CONS_Debug(DBG_GAMELOGIC, "Lowest laps found: %d\n", lowest);

	return lowest;
}

//
// P_FindHighestLap
//
UINT8 P_FindHighestLap(void)
{
	INT32 i;
	UINT8 highest = 0;

	if (!(gametyperules & GTR_CIRCUIT))
		return 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (players[i].laps > highest)
			highest = players[i].laps;
	}

	CONS_Debug(DBG_GAMELOGIC, "Highest laps found: %d\n", highest);

	return highest;
}

//
// P_PlayerInPain
//
// Is player in pain??
// Checks for painstate and flashing, if both found return true
//
boolean P_PlayerInPain(player_t *player)
{
	if (player->spinouttimer || player->squishedtimer)
		return true;

	return false;
}

//
// P_ResetPlayer
//
// Useful when you want to kill everything the player is doing.
void P_ResetPlayer(player_t *player)
{
	//player->pflags &= ~(PF_);

	player->carry = CR_NONE;
	player->onconveyor = 0;

	player->drift = player->driftcharge = 0;
	player->trickpanel = 0;
	player->glanceDir = 0;
	player->pogospring = 0;
}

//
// P_GivePlayerRings
//
// Gives rings to the player, and does any special things required.
// Call this function when you want to increment the player's health.
// Returns the number of rings successfully given (or taken).
//

INT32 P_GivePlayerRings(player_t *player, INT32 num_rings)
{
	INT32 test;

	if (!player->mo)
		return 0;

	if ((gametyperules & GTR_BUMPERS)) // No rings in Battle Mode
		return 0;

	test = player->rings + num_rings;
	if (test > 20) // Caps at 20 rings, sorry!
		num_rings -= (test-20);
	else if (test < -20) // Chaotix ring debt!
		num_rings -= (test+20);

	player->rings += num_rings;
	//player->totalring += num_rings; // Used for GP lives later -- maybe you might want to move this earlier to discourage ring debt...

	return num_rings;
}

INT32 P_GivePlayerSpheres(player_t *player, INT32 num_spheres)
{
	num_spheres += player->spheres;

	// Not alive
	if ((gametyperules & GTR_BUMPERS) && (player->bumpers <= 0))
		return 0;

	if (num_spheres > 40) // Reached the cap, don't waste 'em!
		num_spheres = 40;
	else if (num_spheres < 0)
		num_spheres = 0;

	num_spheres -= player->spheres;

	player->spheres += num_spheres;

	return num_spheres;
}

//
// P_GivePlayerLives
//
// Gives the player an extra life.
// Call this function when you want to add lives to the player.
//
void P_GivePlayerLives(player_t *player, INT32 numlives)
{
	player->lives += numlives;

	if (player->lives > 9)
		player->lives = 9;
	else if (player->lives < 1)
		player->lives = 1;
}

// Adds to the player's score
void P_AddPlayerScore(player_t *player, UINT32 amount)
{
	if (!((gametyperules & GTR_BUMPERS)))
		return;

	if (player->exiting) // srb2kart
		return;

	// Don't go above MAXSCORE.
	if (player->roundscore + amount < MAXSCORE)
		player->roundscore += amount;
	else
		player->roundscore = MAXSCORE;
}

//
// P_PlayLivesJingle
//
void P_PlayLivesJingle(player_t *player)
{
	if (player && !P_IsLocalPlayer(player))
		return;

	if (use1upSound)
		S_StartSound(NULL, sfx_oneup);
	else
	{
		P_PlayJingle(player, JT_1UP);
		strlcpy(S_sfx[sfx_None].caption, "One-up", 7);
		S_StartCaption(sfx_None, -1, extralifetics+1);
	}
}

void P_PlayJingle(player_t *player, jingletype_t jingletype)
{
	const char *musname = jingleinfo[jingletype].musname;
	UINT16 musflags = 0;
	boolean looping = jingleinfo[jingletype].looping;

	char newmusic[7];
	strncpy(newmusic, musname, 7);
#ifdef HAVE_LUA_MUSICPLUS
 	if(LUAh_MusicJingle(jingletype, newmusic, &musflags, &looping))
 		return;
#endif
	newmusic[6] = 0;

	P_PlayJingleMusic(player, newmusic, musflags, looping, jingletype);
}

//
// P_PlayJingleMusic
//
void P_PlayJingleMusic(player_t *player, const char *musname, UINT16 musflags, boolean looping, UINT16 status)
{
	// If gamestate != GS_LEVEL, always play the jingle (1-up intermission)
	if (gamestate == GS_LEVEL && player && !P_IsLocalPlayer(player))
		return;

	S_RetainMusic(musname, musflags, looping, 0, status);
	S_StopMusic();
	S_ChangeMusicInternal(musname, looping);
}

boolean P_EvaluateMusicStatus(UINT16 status, const char *musname)
{
	// \todo lua hook
	int i;
	boolean result = false;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!P_IsLocalPlayer(&players[i]))
			continue;

		switch(status)
		{
			case JT_1UP: // Extra life
				result = false; //(players[i].powers[pw_extralife] > 1);
				break;

			case JT_SHOES:  // Speed shoes
				/*if (players[i].powers[pw_sneakers] > 1 && !players[i].powers[pw_super])
				{
					//strlcpy(S_sfx[sfx_None].caption, "Speed shoes", 12);
					//S_StartCaption(sfx_None, -1, players[i].powers[pw_sneakers]);
					result = true;
				}
				else*/
					result = false;
				break;

			case JT_INV: // Invincibility
			case JT_MINV: // Mario Invincibility
				/*if (players[i].powers[pw_invulnerability] > 1)
				{
					//strlcpy(S_sfx[sfx_None].caption, "Invincibility", 14);
					//S_StartCaption(sfx_None, -1, players[i].powers[pw_invulnerability]);
					result = true;
				}
				else*/
					result = false;
				break;

			case JT_DROWN:  // Drowning
				result = false; //(players[i].powers[pw_underwater] && players[i].powers[pw_underwater] <= 11*TICRATE + 1);
				break;

			case JT_SUPER:  // Super Sonic
				result = false; //(players[i].powers[pw_super]);
				break;

			case JT_GOVER: // Game Over
				result = (players[i].lives <= 0);
				break;

			case JT_NIGHTSTIMEOUT: // NiGHTS Time Out (10 seconds)
			case JT_SSTIMEOUT:
				result = false; //(players[i].nightstime && players[i].nightstime <= 10*TICRATE);
				break;

			case JT_OTHER:  // Other state
				result = LUA_HookShouldJingleContinue(&players[i], musname);
				break;

			case JT_NONE:   // Null state
			case JT_MASTER: // Main level music
			default:
				result = true;
		}

		if (result)
			break;
 	}

	return result;
}

void P_PlayRinglossSound(mobj_t *source)
{
	if (source->player && K_GetShieldFromItem(source->player->itemtype) != KSHIELD_NONE)
		S_StartSound(source, sfx_s1a3); // Shield hit (no ring loss)
	else if (source->player && source->player->rings <= 0)
		S_StartSound(source, sfx_s1a6); // Ring debt (lessened ring loss)
	else
		S_StartSound(source, sfx_s1c6); // Normal ring loss sound
}

void P_PlayDeathSound(mobj_t *source)
{
	S_StartSound(source, sfx_s3k35);
}

void P_PlayVictorySound(mobj_t *source)
{
	if (cv_kartvoices.value)
		S_StartSound(source, sfx_kwin);
}

//
// P_EndingMusic
//
// Consistently sets ending music!
//
boolean P_EndingMusic(player_t *player)
{
	char buffer[9];
	boolean looping = true;
	INT32 bestlocalpos, test;
	player_t *bestlocalplayer;

	if (!P_IsLocalPlayer(player)) // Only applies to a local player
		return false;

	if (multiplayer && demo.playback) // Don't play this in multiplayer replays
		return false;

	// Event - Level Finish
	// Check for if this is valid or not
#define getplayerpos(p) \
	(((players[p].position < 1) || (players[p].pflags & PF_NOCONTEST)) \
		? MAXPLAYERS+1 \
		: players[p].position);

	if (r_splitscreen)
	{
		INT32 *localplayertable = (splitscreen_partied[consoleplayer] ? splitscreen_party[consoleplayer] : g_localplayers);

		if (!((players[localplayertable[0]].exiting || (players[localplayertable[0]].pflags & PF_NOCONTEST))
			|| (players[localplayertable[1]].exiting || (players[localplayertable[1]].pflags & PF_NOCONTEST))
			|| ((r_splitscreen > 1) && (players[localplayertable[2]].exiting || (players[localplayertable[2]].pflags & PF_NOCONTEST)))
			|| ((r_splitscreen > 2) && (players[localplayertable[3]].exiting || (players[localplayertable[3]].pflags & PF_NOCONTEST)))))
			return false;

		bestlocalplayer = &players[localplayertable[0]];
		bestlocalpos = getplayerpos(localplayertable[0]);
#define setbests(p) \
	test = getplayerpos(p); \
	if (test < bestlocalpos) \
	{ \
		bestlocalplayer = &players[p]; \
		bestlocalpos = test; \
	}
		setbests(localplayertable[1]);
		if (r_splitscreen > 1)
			setbests(localplayertable[2]);
		if (r_splitscreen > 2)
			setbests(localplayertable[3]);
#undef setbests
	}
	else
	{
		if (!(player->exiting || (player->pflags & PF_NOCONTEST)))
			return false;

		bestlocalplayer = player;
		bestlocalpos = getplayerpos((player-players));
	}

#undef getplayerpos

	if ((gametyperules & GTR_CIRCUIT) && bestlocalpos == MAXPLAYERS+1)
		sprintf(buffer, "k*fail"); // F-Zero death results theme
	else
	{
		if (K_IsPlayerLosing(bestlocalplayer))
			sprintf(buffer, "k*lose");
		else if (bestlocalpos == 1)
			sprintf(buffer, "k*win");
		else
			sprintf(buffer, "k*ok");
	}

	S_SpeedMusic(1.0f);

	if ((gametyperules & GTR_CIRCUIT))
		buffer[1] = 'r';
	else if ((gametyperules & GTR_BUMPERS))
	{
		buffer[1] = 'b';
		looping = false;
	}

	S_ChangeMusicInternal(buffer, looping);

	return true;
}

//
// P_RestoreMusic
//
// Restores music after some special music change
//
void P_RestoreMusic(player_t *player)
{
	UINT32 position;

	if (!P_IsLocalPlayer(player)) // Only applies to a local player
		return;

	S_SpeedMusic(1.0f);

	// TO-DO: Use jingle system for Kart's stuff

	// Event - HERE COMES A NEW CHALLENGER
	if (mapreset)
	{
		S_ChangeMusicInternal("chalng", false); //S_StopMusic();
		return;
	}

	// Event - Level Ending
	if (P_EndingMusic(player))
		return;

	// Event - Level Start
	if (bossinfo.boss == false && (leveltime < (starttime + (TICRATE/2)))) // see also where time overs are handled
		return;

	{
		INT32 wantedmus = 0; // 0 is level music, 1 is invincibility, 2 is grow

		if (r_splitscreen)
		{
			INT32 bestlocaltimer = 1;
			INT32 *localplayertable = (splitscreen_partied[consoleplayer] ? splitscreen_party[consoleplayer] : g_localplayers);

#define setbests(p) \
	if (players[p].playerstate == PST_LIVE) \
	{ \
		if (players[p].growshrinktimer > bestlocaltimer) \
		{ wantedmus = 1; bestlocaltimer = players[p].growshrinktimer; } \
		else if (players[p].invincibilitytimer > bestlocaltimer) \
		{ wantedmus = 2; bestlocaltimer = players[p].invincibilitytimer; } \
	}
			setbests(localplayertable[0]);
			setbests(localplayertable[1]);
			if (r_splitscreen > 1)
				setbests(localplayertable[2]);
			if (r_splitscreen > 2)
				setbests(localplayertable[3]);
#undef setbests
		}
		else
		{
			if (player->playerstate == PST_LIVE)
			{
				if (player->growshrinktimer > 1)
					wantedmus = 1;
				else if (player->invincibilitytimer > 1)
					wantedmus = 2;
				
			}
		}

		// Item - Grow
		if (wantedmus == 1)
		{
			S_ChangeMusicInternal("kgrow", true);
			S_SetRestoreMusicFadeInCvar(&cv_growmusicfade);
		}
		// Item - Invincibility
		else if (wantedmus == 2)
		{
			S_ChangeMusicInternal("kinvnc", true);
			S_SetRestoreMusicFadeInCvar(&cv_invincmusicfade);
		}
		else
		{
#if 0
			// Event - Final Lap
			// Still works for GME, but disabled for consistency
			if ((gametyperules & GTR_CIRCUIT) && player->laps >= numlaps)
				S_SpeedMusic(1.2f);
#endif
			if (mapmusresume && cv_resume.value)
				position = mapmusresume;
			else
				position = mapmusposition;

			S_ChangeMusicEx(mapmusname, mapmusflags, true, position, 0,
					S_GetRestoreMusicFadeIn());
			S_ClearRestoreMusicFadeInCvar();
			mapmusresume = 0;
		}
	}
}

//
// P_IsObjectInGoop
//
// Returns true if the object is inside goop water.
// (Spectators and objects otherwise without gravity cannot have goop gravity!)
//
boolean P_IsObjectInGoop(mobj_t *mo)
{
	if (mo->player && mo->player->spectator)
		return false;

	if (mo->flags & MF_NOGRAVITY)
		return false;

	return ((mo->eflags & (MFE_UNDERWATER|MFE_GOOWATER)) == (MFE_UNDERWATER|MFE_GOOWATER));
}

//
// P_IsObjectOnGround
//
// Returns true if the player is
// on the ground. Takes reverse
// gravity and FOFs into account.
//
boolean P_IsObjectOnGround(mobj_t *mo)
{
	if (P_IsObjectInGoop(mo))
	{
/*
		// It's a crazy hack that checking if you're on the ground
		// would actually CHANGE your position and momentum,
		if (mo->z < mo->floorz)
		{
			mo->z = mo->floorz;
			mo->momz = 0;
		}
		else if (mo->z + mo->height > mo->ceilingz)
		{
			mo->z = mo->ceilingz - mo->height;
			mo->momz = 0;
		}
*/
		// but I don't want you to ever 'stand' while submerged in goo.
		// You're in constant vertical momentum, even if you get stuck on something.
		// No exceptions.
		return false;
	}

	if (mo->eflags & MFE_VERTICALFLIP)
	{
		if (mo->z+mo->height >= mo->ceilingz)
			return true;
	}
	else
	{
		if (mo->z <= mo->floorz)
			return true;
	}

	return false;
}

//
// P_IsObjectOnGroundIn
//
// Returns true if the player is
// on the ground in a specific sector. Takes reverse
// gravity and FOFs into account.
//
boolean P_IsObjectOnGroundIn(mobj_t *mo, sector_t *sec)
{
	ffloor_t *rover;

	// Is the object in reverse gravity?
	if (mo->eflags & MFE_VERTICALFLIP)
	{
		// Detect if the player is on the ceiling.
		if (mo->z+mo->height >= P_GetSpecialTopZ(mo, sec, sec))
			return true;
		// Otherwise, detect if the player is on the bottom of a FOF.
		else
		{
			for (rover = sec->ffloors; rover; rover = rover->next)
			{
				// If the FOF doesn't exist, continue.
				if (!(rover->flags & FF_EXISTS))
					continue;

				// If the FOF is configured to let the object through, continue.
				if (!((rover->flags & FF_BLOCKPLAYER && mo->player)
					|| (rover->flags & FF_BLOCKOTHERS && !mo->player)))
					continue;

				// If the the platform is intangible from below, continue.
				if (rover->flags & FF_PLATFORM)
					continue;

				// If the FOF is a water block, continue. (Unnecessary check?)
				if (rover->flags & FF_SWIMMABLE)
					continue;

				// Actually check if the player is on the suitable FOF.
				if (mo->z+mo->height == P_GetSpecialBottomZ(mo, sectors + rover->secnum, sec))
					return true;
			}
		}
	}
	// Nope!
	else
	{
		// Detect if the player is on the floor.
		if (mo->z <= P_GetSpecialBottomZ(mo, sec, sec))
			return true;
		// Otherwise, detect if the player is on the top of a FOF.
		else
		{
			for (rover = sec->ffloors; rover; rover = rover->next)
			{
				// If the FOF doesn't exist, continue.
				if (!(rover->flags & FF_EXISTS))
					continue;

				// If the FOF is configured to let the object through, continue.
				if (!((rover->flags & FF_BLOCKPLAYER && mo->player)
					|| (rover->flags & FF_BLOCKOTHERS && !mo->player)))
					continue;

				// If the the platform is intangible from above, continue.
				if (rover->flags & FF_REVERSEPLATFORM)
					continue;

				// If the FOF is a water block, continue. (Unnecessary check?)
				if (rover->flags & FF_SWIMMABLE)
					continue;

				// Actually check if the player is on the suitable FOF.
				if (mo->z == P_GetSpecialTopZ(mo, sectors + rover->secnum, sec))
					return true;
			}
		}
	}

	return false;
}

//
// P_IsObjectOnRealGround
//
// Helper function for T_EachTimeThinker
// Like P_IsObjectOnGroundIn, except ONLY THE REAL GROUND IS CONSIDERED, NOT FOFS
// I'll consider whether to make this a more globally accessible function or whatever in future
// -- Monster Iestyn
//
// Really simple, but personally I think it's also incredibly helpful. I think this is fine in p_user.c
// -- Sal

boolean P_IsObjectOnRealGround(mobj_t *mo, sector_t *sec)
{
	// Is the object in reverse gravity?
	if (mo->eflags & MFE_VERTICALFLIP)
	{
		// Detect if the player is on the ceiling.
		if (mo->z+mo->height >= P_GetSpecialTopZ(mo, sec, sec))
			return true;
	}
	// Nope!
	else
	{
		// Detect if the player is on the floor.
		if (mo->z <= P_GetSpecialBottomZ(mo, sec, sec))
			return true;
	}
	return false;
}

//
// P_SetObjectMomZ
//
// Sets the player momz appropriately.
// Takes reverse gravity into account.
//
void P_SetObjectMomZ(mobj_t *mo, fixed_t value, boolean relative)
{
	if (mo->eflags & MFE_VERTICALFLIP)
		value = -value;

	if (mo->scale != FRACUNIT)
		value = FixedMul(value, mo->scale);

	if (relative)
		mo->momz += value;
	else
		mo->momz = value;
}

//
// P_IsMachineLocalPlayer
//
// Returns true if player is
// ACTUALLY on the local machine
//
boolean P_IsMachineLocalPlayer(player_t *player)
{
	UINT8 i;

	if (player == NULL)
	{
		return false;
	}

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[g_localplayers[i]])
			return true;
	}

	return false;
}

//
// P_IsLocalPlayer
//
// Returns true if player is
// on the local machine
// (or simulated party)
//
boolean P_IsLocalPlayer(player_t *player)
{
	UINT8 i;

	if (player == NULL)
	{
		return false;
	}

	// nobody is ever local when watching something back - you're a spectator there, even if your g_localplayers might say otherwise
	if (demo.playback)
		return false;

	// parties - treat everyone as if it's couch co-op
	if (splitscreen_partied[consoleplayer])
	{
		for (i = 0; i < splitscreen_party_size[consoleplayer]; i++)
		{
			if (splitscreen_party[consoleplayer][i] == (player-players))
				return true;
		}
		return false;
	}

	return P_IsMachineLocalPlayer(player);
}

//
// P_IsDisplayPlayer
//
// Returns true if player is
// currently being watched.
//
boolean P_IsDisplayPlayer(player_t *player)
{
	UINT8 i;

	if (player == NULL)
	{
		return false;
	}

	for (i = 0; i <= r_splitscreen; i++) // DON'T skip P1
	{
		if (player == &players[displayplayers[i]])
			return true;
	}

	return false;
}

//
// P_SpawnGhostMobj
//
// Spawns a ghost object on the player
//
mobj_t *P_SpawnGhostMobj(mobj_t *mobj)
{
	mobj_t *ghost = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_GHOST);

	P_SetTarget(&ghost->target, mobj);

	P_SetScale(ghost, mobj->scale);
	ghost->destscale = mobj->scale;

	if (mobj->eflags & MFE_VERTICALFLIP)
	{
		ghost->eflags |= MFE_VERTICALFLIP;
		ghost->z += mobj->height - ghost->height;
	}

	ghost->color = mobj->color;
	ghost->colorized = mobj->colorized; // Kart: they should also be colorized if their origin is

	ghost->angle = (mobj->player ? mobj->player->drawangle : mobj->angle);
	ghost->roll = mobj->roll;
	ghost->pitch = mobj->pitch;
	ghost->sprite = mobj->sprite;
	ghost->sprite2 = mobj->sprite2;
	ghost->frame = mobj->frame;
	ghost->tics = -1;
	ghost->renderflags = (mobj->renderflags & ~RF_TRANSMASK)|RF_TRANS50;
	ghost->fuse = ghost->info->damage;
	ghost->skin = mobj->skin;
	ghost->standingslope = mobj->standingslope;

	ghost->sprxoff = mobj->sprxoff;
	ghost->spryoff = mobj->spryoff;
	ghost->sprzoff = mobj->sprzoff;
	ghost->rollangle = mobj->rollangle;

	ghost->spritexscale = mobj->spritexscale;
	ghost->spriteyscale = mobj->spriteyscale;
	ghost->spritexoffset = mobj->spritexoffset;
	ghost->spriteyoffset = mobj->spriteyoffset;

	if (mobj->flags2 & MF2_OBJECTFLIP)
		ghost->flags |= MF2_OBJECTFLIP;

	if (!(mobj->flags & MF_DONTENCOREMAP))
		ghost->flags &= ~MF_DONTENCOREMAP;

	if (mobj->player && mobj->player->followmobj)
	{
		mobj_t *ghost2 = P_SpawnGhostMobj(mobj->player->followmobj);
		P_SetTarget(&ghost2->tracer, ghost);
		P_SetTarget(&ghost->tracer, ghost2);
		ghost2->flags2 |= (mobj->player->followmobj->flags2 & MF2_LINKDRAW);
	}

	// Copy interpolation data :)
	ghost->old_x = mobj->old_x2;
	ghost->old_y = mobj->old_y2;
	ghost->old_z = mobj->old_z2;
	ghost->old_angle = (mobj->player ? mobj->player->old_drawangle2 : mobj->old_angle2);
	ghost->old_pitch = mobj->old_pitch2;
	ghost->old_roll = mobj->old_roll2;

	return ghost;
}

//
// P_DoPlayerExit
//
// Player exits the map via sector trigger
void P_DoPlayerExit(player_t *player)
{
	const boolean losing = K_IsPlayerLosing(player);

	if (player->exiting || mapreset)
		return;

	if (P_IsLocalPlayer(player) && (!player->spectator && !demo.playback))
		legitimateexit = true;

	if (G_GametypeUsesLives() && losing)
	{
		// Remove a life from the losing player
		K_PlayerLoseLife(player);
	}

	if ((gametyperules & GTR_CIRCUIT)) // If in Race Mode, allow
	{
		player->exiting = raceexittime+2;
		K_KartUpdatePosition(player);

		if (cv_kartvoices.value)
		{
			if (P_IsDisplayPlayer(player))
			{
				sfxenum_t sfx_id;
				if (losing)
					sfx_id = ((skin_t *)player->mo->skin)->soundsid[S_sfx[sfx_klose].skinsound];
				else
					sfx_id = ((skin_t *)player->mo->skin)->soundsid[S_sfx[sfx_kwin].skinsound];
				S_StartSound(NULL, sfx_id);
			}
			else
			{
				if (losing)
					S_StartSound(player->mo, sfx_klose);
				else
					S_StartSound(player->mo, sfx_kwin);
			}
		}

		if (cv_inttime.value > 0)
			P_EndingMusic(player);

		if (P_CheckRacers())
			player->exiting = raceexittime+1;
	}
	else if ((gametyperules & GTR_BUMPERS)) // Battle Mode exiting
	{
		player->exiting = battleexittime+1;
		P_EndingMusic(player);
	}
	else
		player->exiting = raceexittime+2; // Accidental death safeguard???

	if (grandprixinfo.gp == true)
	{
		if (player->bot)
		{
			// Bots are going to get harder... :)
			K_IncreaseBotDifficulty(player);
		}
		else if (!losing)
		{
			const UINT8 lifethreshold = 20;
			UINT8 extra = 0;

			// YOU WIN
			grandprixinfo.wonround = true;

			// Increase your total rings
			if (RINGTOTAL(player) > 0)
			{
				player->totalring += RINGTOTAL(player);

				extra = player->totalring / lifethreshold;

				if (extra > player->xtralife)
				{
					P_GivePlayerLives(player, extra - player->xtralife);
					S_StartSound(NULL, sfx_cdfm73);
					player->xtralife = extra;
				}
			}
		}
	}

	player->karthud[khud_cardanimation] = 0; // srb2kart: reset battle animation

	if (player == &players[consoleplayer])
		demo.savebutton = leveltime;
}

//
// P_PlayerHitFloor
//
// Handles player hitting floor surface.
// Returns whether to clip momz.
boolean P_PlayerHitFloor(player_t *player, boolean fromAir)
{
	boolean clipmomz;

	I_Assert(player->mo != NULL);

	clipmomz = !(P_CheckDeathPitCollide(player->mo));

	if (fromAir == true && clipmomz == true)
	{
		K_SpawnSplashForMobj(player->mo, abs(player->mo->momz));
	}
	
	if (fromAir == true && clipmomz == true)
	{
		// Cut momentum in half when you hit the ground and
		// aren't pressing any controls.
		if (!(player->cmd.forwardmove || player->cmd.sidemove) && !player->cmomx && !player->cmomy
			&& !(player->spinouttimer))
		{
			player->mo->momx = player->mo->momx/2;
			player->mo->momy = player->mo->momy/2;
							
			if (player->cmd.buttons & BT_BRAKE && !(player->cmd.forwardmove)) // FURTHER slowdown if you're braking.
			{
				player->mo->momx = player->mo->momx/2;
				player->mo->momy = player->mo->momy/2;
			}                    
		}
	}

	return clipmomz;
}

boolean P_InQuicksand(mobj_t *mo) // Returns true if you are in quicksand
{
	sector_t *sector = mo->subsector->sector;
	fixed_t topheight, bottomheight;

	fixed_t flipoffset = ((mo->eflags & MFE_VERTICALFLIP) ? (mo->height/2) : 0);

	if (sector->ffloors)
	{
		ffloor_t *rover;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS))
				continue;

			if (!(rover->flags & FF_QUICKSAND))
				continue;

			topheight    = P_GetFFloorTopZAt   (rover, mo->x, mo->y);
			bottomheight = P_GetFFloorBottomZAt(rover, mo->x, mo->y);

			if (mo->z + flipoffset > topheight)
				continue;

			if (mo->z + (mo->height/2) + flipoffset < bottomheight)
				continue;

			return true;
		}
	}

	return false; // No sand here, Captain!
}

static void P_CheckBustableBlocks(player_t *player)
{
	msecnode_t *node;
	fixed_t oldx;
	fixed_t oldy;

	if ((netgame || multiplayer) && player->spectator)
		return;

	oldx = player->mo->x;
	oldy = player->mo->y;

	// SRB2Kart TODO: make shatter blocks the default behavior, we don't need the hundreds of other types

	P_UnsetThingPosition(player->mo);
	player->mo->x += player->mo->momx;
	player->mo->y += player->mo->momy;
	P_SetThingPosition(player->mo);

	for (node = player->mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		if (!node->m_sector)
			break;

		if (node->m_sector->ffloors)
		{
			ffloor_t *rover;
			fixed_t topheight, bottomheight;

			for (rover = node->m_sector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS)) continue;

				if ((rover->flags & FF_BUSTUP)/* && rover->master->frontsector->crumblestate == CRUMBLE_NONE*/)
				{
					// If it's an FF_SHATTER, you can break it just by touching it.
					if (rover->flags & FF_SHATTER)
						goto bust;

					if (rover->flags & FF_STRONGBUST)
						continue;

				bust:
					topheight = P_GetFOFTopZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);
					bottomheight = P_GetFOFBottomZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);

					// Height checks
					if (rover->flags & FF_SHATTERBOTTOM)
					{
						if (player->mo->z+player->mo->momz + player->mo->height < bottomheight)
							continue;

						if (player->mo->z+player->mo->height > bottomheight)
							continue;
					}
					else if (rover->flags & FF_SPINBUST)
					{
						if (player->mo->z+player->mo->momz > topheight)
							continue;

						if (player->mo->z + player->mo->height < bottomheight)
							continue;
					}
					else if (rover->flags & FF_SHATTER)
					{
						if (player->mo->z + player->mo->momz > topheight)
							continue;

						if (player->mo->z+player->mo->momz + player->mo->height < bottomheight)
							continue;
					}
					else
					{
						if (player->mo->z >= topheight)
							continue;

						if (player->mo->z + player->mo->height < bottomheight)
							continue;
					}

					// Impede the player's fall a bit
					if (((rover->flags & FF_SPINBUST) || (rover->flags & FF_SHATTER)) && player->mo->z >= topheight)
						player->mo->momz >>= 1;
					else if (rover->flags & FF_SHATTER)
					{
						player->mo->momx >>= 1;
						player->mo->momy >>= 1;
					}

					//if (metalrecording)
					//	G_RecordBustup(rover);

					EV_CrumbleChain(NULL, rover); // node->m_sector

					// Run a linedef executor??
					if (rover->master->flags & ML_EFFECT5)
						P_LinedefExecute((INT16)(P_AproxDistance(rover->master->dx, rover->master->dy)>>FRACBITS), player->mo, node->m_sector);

					goto bustupdone;
				}
			}
		}
	}
bustupdone:

	P_UnsetThingPosition(player->mo);
	player->mo->x = oldx;
	player->mo->y = oldy;
	P_SetThingPosition(player->mo);
}

static void P_CheckBouncySectors(player_t *player)
{
	msecnode_t *node;
	fixed_t oldx;
	fixed_t oldy;
	fixed_t oldz;
	vector3_t momentum;

	oldx = player->mo->x;
	oldy = player->mo->y;
	oldz = player->mo->z;

	P_UnsetThingPosition(player->mo);
	player->mo->x += player->mo->momx;
	player->mo->y += player->mo->momy;
	player->mo->z += player->mo->momz;
	P_SetThingPosition(player->mo);

	for (node = player->mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		if (!node->m_sector)
			break;

		if (node->m_sector->ffloors)
		{
			ffloor_t *rover;
			boolean top = true;
			fixed_t topheight, bottomheight;

			for (rover = node->m_sector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS))
					continue; // FOFs should not be bouncy if they don't even "exist"

				if (GETSECSPECIAL(rover->master->frontsector->special, 1) != 15)
					continue; // this sector type is required for FOFs to be bouncy

				topheight = P_GetFOFTopZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);
				bottomheight = P_GetFOFBottomZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);

				if (player->mo->z > topheight)
					continue;

				if (player->mo->z + player->mo->height < bottomheight)
					continue;

				if (oldz < P_GetFOFTopZ(player->mo, node->m_sector, rover, oldx, oldy, NULL)
						&& oldz + player->mo->height > P_GetFOFBottomZ(player->mo, node->m_sector, rover, oldx, oldy, NULL))
					top = false;

				{
					fixed_t linedist;

					linedist = P_AproxDistance(rover->master->v1->x-rover->master->v2->x, rover->master->v1->y-rover->master->v2->y);

					linedist = FixedDiv(linedist,100*FRACUNIT);

					if (top)
					{
						fixed_t newmom;

						pslope_t *slope;
						if (abs(oldz - topheight) < abs(oldz + player->mo->height - bottomheight)) { // Hit top
							slope = *rover->t_slope;
						} else { // Hit bottom
							slope = *rover->b_slope;
						}

						momentum.x = player->mo->momx;
						momentum.y = player->mo->momy;
						momentum.z = player->mo->momz*2;

						if (slope)
							P_ReverseQuantizeMomentumToSlope(&momentum, slope);

						newmom = momentum.z = -FixedMul(momentum.z,linedist)/2;

						if (abs(newmom) < (linedist*2))
						{
							goto bouncydone;
						}

						if (!(rover->master->flags & ML_NOTBOUNCY))
						{
							if (newmom > 0)
							{
								if (newmom < 8*FRACUNIT)
									newmom = 8*FRACUNIT;
							}
							else if (newmom > -8*FRACUNIT && newmom != 0)
								newmom = -8*FRACUNIT;
						}

						if (newmom > player->mo->height/2)
							newmom = player->mo->height/2;
						else if (newmom < -player->mo->height/2)
							newmom = -player->mo->height/2;

						momentum.z = newmom*2;

						if (slope)
							P_QuantizeMomentumToSlope(&momentum, slope);

						player->mo->momx = momentum.x;
						player->mo->momy = momentum.y;
						player->mo->momz = momentum.z/2;
					}
					else
					{
						player->mo->momx = -FixedMul(player->mo->momx,linedist);
						player->mo->momy = -FixedMul(player->mo->momy,linedist);
					}

					goto bouncydone;
				}
			}
		}
	}
bouncydone:
	P_UnsetThingPosition(player->mo);
	player->mo->x = oldx;
	player->mo->y = oldy;
	player->mo->z = oldz;
	P_SetThingPosition(player->mo);
}

static void P_CheckQuicksand(player_t *player)
{
	ffloor_t *rover;
	fixed_t sinkspeed, friction;
	fixed_t topheight, bottomheight;

	if (!(player->mo->subsector->sector->ffloors && player->mo->momz <= 0))
		return;

	for (rover = player->mo->subsector->sector->ffloors; rover; rover = rover->next)
	{
		if (!(rover->flags & FF_EXISTS)) continue;

		if (!(rover->flags & FF_QUICKSAND))
			continue;

		topheight    = P_GetFFloorTopZAt   (rover, player->mo->x, player->mo->y);
		bottomheight = P_GetFFloorBottomZAt(rover, player->mo->x, player->mo->y);

		if (topheight >= player->mo->z && bottomheight < player->mo->z + player->mo->height)
		{
			sinkspeed = abs(rover->master->v1->x - rover->master->v2->x)>>1;

			sinkspeed = FixedDiv(sinkspeed,TICRATE*FRACUNIT);

			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				fixed_t ceilingheight = P_GetCeilingZ(player->mo, player->mo->subsector->sector, player->mo->x, player->mo->y, NULL);

				player->mo->z += sinkspeed;

				if (player->mo->z + player->mo->height >= ceilingheight)
					player->mo->z = ceilingheight - player->mo->height;

				if (player->mo->momz <= 0)
					P_PlayerHitFloor(player, false);
			}
			else
			{
				fixed_t floorheight = P_GetFloorZ(player->mo, player->mo->subsector->sector, player->mo->x, player->mo->y, NULL);

				player->mo->z -= sinkspeed;

				if (player->mo->z <= floorheight)
					player->mo->z = floorheight;

				if (player->mo->momz >= 0)
					P_PlayerHitFloor(player, false);
			}

			friction = abs(rover->master->v1->y - rover->master->v2->y)>>6;

			player->mo->momx = FixedMul(player->mo->momx, friction);
			player->mo->momy = FixedMul(player->mo->momy, friction);
		}
	}
}

//
// P_CheckInvincibilityTimer
//
// Restores music from invincibility, and handles invincibility sparkles
//
static void P_CheckInvincibilityTimer(player_t *player)
{
	if (!player->invincibilitytimer)
		return;

	// Resume normal music stuff.
	if (player->invincibilitytimer == 1)
	{
		//K_KartResetPlayerColor(player); -- this gets called every tic anyways
		G_GhostAddColor((INT32) (player - players), GHC_NORMAL);

		P_RestoreMusic(player);
		return;
	}
}

//
// P_DoBubbleBreath
//
// Handles bubbles spawned by the player
//
static void P_DoBubbleBreath(player_t *player)
{
	fixed_t x = player->mo->x;
	fixed_t y = player->mo->y;
	fixed_t z = player->mo->z;
	mobj_t *bubble = NULL;

	if (!(player->mo->eflags & MFE_UNDERWATER) || player->spectator)
		return;

	if (player->charflags & SF_MACHINE)
	{
		if (P_RandomChance(FRACUNIT/5))
		{
			fixed_t r = player->mo->radius>>FRACBITS;
			x += (P_RandomRange(r, -r)<<FRACBITS);
			y += (P_RandomRange(r, -r)<<FRACBITS);
			z += (P_RandomKey(player->mo->height>>FRACBITS)<<FRACBITS);
			bubble = P_SpawnMobj(x, y, z, MT_WATERZAP);
			S_StartSound(bubble, sfx_beelec);
		}
	}
	else
	{
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - FixedDiv(player->mo->height,5*(FRACUNIT/4));
		else
			z += FixedDiv(player->mo->height,5*(FRACUNIT/4));

		if (P_RandomChance(FRACUNIT/16))
			bubble = P_SpawnMobj(x, y, z, MT_SMALLBUBBLE);
		else if (P_RandomChance(3*FRACUNIT/256))
			bubble = P_SpawnMobj(x, y, z, MT_MEDIUMBUBBLE);
	}

	if (bubble)
	{
		bubble->threshold = 42;
		bubble->destscale = player->mo->scale;
		P_SetScale(bubble, bubble->destscale);
	}
}

//#define OLD_MOVEMENT_CODE 1
static void P_3dMovement(player_t *player)
{
	ticcmd_t *cmd;
	angle_t movepushangle, movepushsideangle; // Analog
	fixed_t movepushforward = 0, movepushside = 0;
	angle_t dangle; // replaces old quadrants bits
	fixed_t oldMagnitude, newMagnitude;
	vector3_t totalthrust;

	totalthrust.x = totalthrust.y = 0; // I forget if this is needed
	totalthrust.z = FRACUNIT*P_MobjFlip(player->mo)/3; // A bit of extra push-back on slopes
	
	cmd = &player->cmd;
	
	if (!player->pogospring)
		cmd->sidemove = 0;

	// Get the old momentum; this will be needed at the end of the function! -SH
	oldMagnitude = R_PointToDist2(player->mo->momx - player->cmomx, player->mo->momy - player->cmomy, 0, 0);

	if (player->drift != 0)
	{
		movepushangle = player->mo->angle - (ANGLE_45/5) * player->drift;
	}
	else if (player->spinouttimer || player->wipeoutslow) // if spun out, use the boost angle
	{
		movepushangle = (angle_t)player->boostangle;
	}
	else
	{
		movepushangle = player->mo->angle;
	}
	movepushsideangle = movepushangle-ANGLE_90;

	// cmomx/cmomy stands for the conveyor belt speed.
	if (player->onconveyor == 2) // Wind/Current
	{
		//if (player->mo->z > player->mo->watertop || player->mo->z + player->mo->height < player->mo->waterbottom)
		if (!(player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER)))
			player->cmomx = player->cmomy = 0;
	}
	else if (player->onconveyor == 4 && !P_IsObjectOnGround(player->mo)) // Actual conveyor belt
		player->cmomx = player->cmomy = 0;
	else if (player->onconveyor != 2 && player->onconveyor != 4
#ifdef POLYOBJECTS
				&& player->onconveyor != 1
#endif
	)
		player->cmomx = player->cmomy = 0;

	player->rmomx = player->mo->momx - player->cmomx;
	player->rmomy = player->mo->momy - player->cmomy;

	// Calculates player's speed based on distance-of-a-line formula
	player->speed = R_PointToDist2(0, 0, player->rmomx, player->rmomy);

	// Monster Iestyn - 04-11-13
	// Quadrants are stupid, excessive and broken, let's do this a much simpler way!
	// Get delta angle from rmom angle and player angle first
	dangle = R_PointToAngle2(0,0, player->rmomx, player->rmomy) - player->mo->angle;
	if (dangle > ANGLE_180) //flip to keep to one side
	{
		dangle = InvAngle(dangle);
	}

	// anything else will leave both at 0, so no need to do anything else

	//{ SRB2kart 220217 - Toaster Code for misplaced thrust
#if 0
	if (!player->drift) // Not Drifting
	{
		angle_t difference = dangle/2;
		boolean reverse = (dangle >= ANGLE_90);

		if (dangleflip)
			difference = InvAngle(difference);

		if (reverse)
			difference += ANGLE_180;

		P_InstaThrust(player->mo, player->mo->angle + difference, player->speed);
	}
#endif
	//}

	// Do not let the player control movement if not onground.
	// SRB2Kart: pogo spring and speed bumps are supposed to control like you're on the ground
	onground = (P_IsObjectOnGround(player->mo) || (player->pogospring));

	// Forward movement
	if (!((player->exiting || mapreset) || (P_PlayerInPain(player) && !onground)))
	{
		movepushforward = K_3dKartMovement(player);

		// allow very small movement while in air for gameplay
		if (!onground)
			movepushforward >>= 2; // proper air movement

		if (player->mo->movefactor != FRACUNIT) // Friction-scaled acceleration...
			movepushforward = FixedMul(movepushforward, player->mo->movefactor);

		if (cmd->buttons & BT_BRAKE && (K_GetForwardMove(player) == 0)) // SRB2kart - braking isn't instant
			movepushforward /= 64;

		if (K_GetForwardMove(player) > 0)
			player->brakestop = 0;
		else if (player->brakestop < 6) // Don't start reversing with brakes until you've made a stop first
		{
			if (player->speed < 8*FRACUNIT)
				player->brakestop++;
			movepushforward = 0;
		}

		totalthrust.x += P_ReturnThrustX(player->mo, movepushangle, movepushforward);
		totalthrust.y += P_ReturnThrustY(player->mo, movepushangle, movepushforward);
	}
	else if (!(player->spinouttimer))
	{
		K_MomentumToFacing(player);
	}
	
	// Sideways movement
	if (cmd->sidemove != 0 && !((player->exiting || mapreset) || player->spinouttimer))
	{
		if (cmd->sidemove > 0)
			movepushside = (cmd->sidemove * FRACUNIT/128) + FixedDiv(player->speed, K_GetKartSpeed(player, true, true));
		else
			movepushside = (cmd->sidemove * FRACUNIT/128) - FixedDiv(player->speed, K_GetKartSpeed(player, true, true));

		totalthrust.x += P_ReturnThrustX(player->mo, movepushsideangle, movepushside);
		totalthrust.y += P_ReturnThrustY(player->mo, movepushsideangle, movepushside);
	}

	if ((totalthrust.x || totalthrust.y)
		&& player->mo->standingslope != NULL
		&& (!(player->mo->standingslope->flags & SL_NOPHYSICS))
		&& abs(player->mo->standingslope->zdelta) > FRACUNIT/2)
	{
		// Factor thrust to slope, but only for the part pushing up it!
		// The rest is unaffected.
		angle_t thrustangle = R_PointToAngle2(0, 0, totalthrust.x, totalthrust.y) - player->mo->standingslope->xydirection;

		if (player->mo->standingslope->zdelta < 0)
		{
			// Direction goes down, so thrustangle needs to face toward
			if (thrustangle < ANGLE_90 || thrustangle > ANGLE_270)
			{
				P_QuantizeMomentumToSlope(&totalthrust, player->mo->standingslope);
			}
		}
		else
		{
			// Direction goes up, so thrustangle needs to face away
			if (thrustangle > ANGLE_90 && thrustangle < ANGLE_270)
			{
				P_QuantizeMomentumToSlope(&totalthrust, player->mo->standingslope);
			}
		}
	}

	player->mo->momx += totalthrust.x;
	player->mo->momy += totalthrust.y;

	// Time to ask three questions:
	// 1) Are we over topspeed?
	// 2) If "yes" to 1, were we moving over topspeed to begin with?
	// 3) If "yes" to 2, are we now going faster?

	// Time to ask three questions:
	// 1) Are we over topspeed?
	// 2) If "yes" to 1, were we moving over topspeed to begin with?
	// 3) If "yes" to 2, are we now going faster?

	// If "yes" to 3, normalize to our initial momentum; this will allow thoks to stay as fast as they normally are.
	// If "no" to 3, ignore it; the player might be going too fast, but they're slowing down, so let them.
	// If "no" to 2, normalize to topspeed, so we can't suddenly run faster than it of our own accord.
	// If "no" to 1, we're not reaching any limits yet, so ignore this entirely!
	// -Shadow Hog
	newMagnitude = R_PointToDist2(player->mo->momx - player->cmomx, player->mo->momy - player->cmomy, 0, 0);
	if (newMagnitude > K_GetKartSpeed(player, true, true)) //topspeed)
	{
		fixed_t tempmomx, tempmomy;
		if (oldMagnitude > K_GetKartSpeed(player, true, true) && onground) // SRB2Kart: onground check for air speed cap
		{
			if (newMagnitude > oldMagnitude)
			{
				tempmomx = FixedMul(FixedDiv(player->mo->momx - player->cmomx, newMagnitude), oldMagnitude);
				tempmomy = FixedMul(FixedDiv(player->mo->momy - player->cmomy, newMagnitude), oldMagnitude);
				player->mo->momx = tempmomx + player->cmomx;
				player->mo->momy = tempmomy + player->cmomy;
			}
			// else do nothing
		}
		else
		{
			tempmomx = FixedMul(FixedDiv(player->mo->momx - player->cmomx, newMagnitude), K_GetKartSpeed(player, true, true)); //topspeed)
			tempmomy = FixedMul(FixedDiv(player->mo->momy - player->cmomy, newMagnitude), K_GetKartSpeed(player, true, true)); //topspeed)
			player->mo->momx = tempmomx + player->cmomx;
			player->mo->momy = tempmomy + player->cmomy;
		}
	}
}

//
// P_UpdatePlayerAngle
//
// Updates player angleturn with cmd->turning
//
static void P_UpdatePlayerAngle(player_t *player)
{
	INT16 angle_diff, max_left_turn, max_right_turn;
	boolean add_delta = true;
	fixed_t currentSpeed = 0;
	ticcmd_t *cmd = &player->cmd;
	angle_t anglechange = player->angleturn;
	int i;
	UINT8 p = UINT8_MAX;

	player->steering = cmd->turning; // Set this so functions that rely on steering still work.

	if (K_GetKartTurnValue(player, KART_FULLTURN) != 0)
	{
		player->lturn_max[leveltime%MAXPREDICTTICS] = K_GetKartTurnValue(player, KART_FULLTURN)+1;
		player->rturn_max[leveltime%MAXPREDICTTICS] = K_GetKartTurnValue(player, -KART_FULLTURN)-1;
	}
	else
	{
		player->rturn_max[leveltime%MAXPREDICTTICS] = player->lturn_max[leveltime%MAXPREDICTTICS] = 0;
	}

	// KART: Don't directly apply angleturn! It may have been either A) forged by a malicious client, or B) not be a smooth turn due to a player dropping frames.
	// Instead, turn the player only up to the amount they're supposed to turn accounting for latency. Allow exactly 1 extra turn unit to try to keep old replays synced.
	angle_diff = cmd->angle - (player->mo->angle>>16);
	max_left_turn = player->lturn_max[(leveltime + MAXPREDICTTICS - cmd->latency) % MAXPREDICTTICS];
	max_right_turn = player->rturn_max[(leveltime + MAXPREDICTTICS - cmd->latency) % MAXPREDICTTICS];

	//CONS_Printf("----------------\nangle diff: %d - turning options: %d to %d - ", angle_diff, max_left_turn, max_right_turn);

	if (angle_diff > max_left_turn)
		angle_diff = max_left_turn;
	else if (angle_diff < max_right_turn)
		angle_diff = max_right_turn;
	else
	{
		// Try to keep normal turning as accurate to 1.0.1 as possible to reduce replay desyncs.
		anglechange = cmd->angle<<TICCMD_REDUCE;
		add_delta = false;
	}
	//CONS_Printf("applied turn: %d\n", angle_diff);

	if (add_delta) {
		anglechange += angle_diff<<TICCMD_REDUCE;
		anglechange &= ~0xFFFF; // Try to keep the turning somewhat similar to how it was before?
		/*CONS_Printf("leftover turn (%s): %5d or %4d%%\n",
						player_names[player-players],
						(INT16) (cmd->angle - (player->mo->angle>>TICCMD_REDUCE)),
						(INT16) (cmd->angle - (player->mo->angle>>TICCMD_REDUCE)) * 100 / (angle_diff ? angle_diff : 1));*/
	}


	// CONS_Printf("Playerid:%d\n",p);
	player->angleturn = anglechange;
	player->mo->angle = player->angleturn;

	if (!cv_allowmlook.value || player->spectator == false)
	{
		player->aiming = 0;
	}
	else
	{
		player->aiming += (player->cmd.aiming << TICCMD_REDUCE);
		player->aiming = G_ClipAimingPitch((INT32*) &player->aiming);
	}

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			localaiming[i] = player->aiming;
			break;
		}
	}
}

//
// P_SpectatorMovement
//
// Control for spectators in multiplayer
//
static void P_SpectatorMovement(player_t *player)
{
	ticcmd_t *cmd = &player->cmd;

	P_UpdatePlayerAngle(player);

	ticruned++;
	if (!(cmd->flags & TICCMD_RECEIVED))
		ticmiss++;

	if (cmd->buttons & BT_ACCELERATE)
		player->mo->z += 32*mapobjectscale;
	else if (cmd->buttons & BT_BRAKE)
		player->mo->z -= 32*mapobjectscale;

	if (player->mo->z > player->mo->ceilingz - player->mo->height)
		player->mo->z = player->mo->ceilingz - player->mo->height;
	if (player->mo->z < player->mo->floorz)
		player->mo->z = player->mo->floorz;

	player->mo->momx = player->mo->momy = player->mo->momz = 0;
	if (cmd->forwardmove != 0)
	{
		P_Thrust(player->mo, player->mo->angle, cmd->forwardmove*mapobjectscale);

		// Quake-style flying spectators :D
		player->mo->momz += FixedMul(cmd->forwardmove*mapobjectscale, AIMINGTOSLOPE(player->aiming));
	}
}

//
// P_MovePlayer
void P_MovePlayer(player_t *player)
{
	ticcmd_t *cmd;
	//INT32 i;

	fixed_t runspd;

	if (countdowntimeup)
		return;

	cmd = &player->cmd;
	runspd = 14*player->mo->scale; //srb2kart

	// Let's have some movement speed fun on low-friction surfaces, JUST for players...
	// (high friction surfaces shouldn't have any adjustment, since the acceleration in
	// this game is super high and that ends up cheesing high-friction surfaces.)
	runspd = FixedMul(runspd, player->mo->movefactor);

	// Control relinquishing stuff!
	if (player->nocontrol)
		player->pflags |= PF_STASIS;

	// note: don't unset stasis here

	if (player->spectator)
	{
		player->mo->eflags &= ~MFE_VERTICALFLIP; // deflip...
		P_SpectatorMovement(player);
		return;
	}

	//////////////////////
	// MOVEMENT CODE	//
	//////////////////////

	P_UpdatePlayerAngle(player);

	ticruned++;
	if (!(cmd->flags & TICCMD_RECEIVED))
		ticmiss++;

	P_3dMovement(player);

	if (cmd->turning == 0)
	{
		player->justDI = 0;
	}

	if (player->squishedtimer > 0)
	{
		P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
		player->mo->spriteyscale = (FRACUNIT / 4);
		if (player->squishedtimer == 1)
			player->mo->spriteyscale = FRACUNIT;
	}
	else if (player->carry == CR_SLIDING)
	{
		P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
		player->drawangle -= ANGLE_22h;
		player->mo->rollangle = 0;
		player->glanceDir = 0;
		player->pflags &= ~PF_GAINAX;
	}
	else if ((player->spinouttimer > 0))
	{
		UINT16 speed = player->spinouttimer/8;
		if (speed > 8)
			speed = 8;
		else if (speed < 1)
			speed = 1;

		P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

		if (speed == 1 && abs((signed)(player->mo->angle - player->drawangle)) < ANGLE_22h)
			player->drawangle = player->mo->angle; // Face forward at the end of the animation
		else
			player->drawangle -= (ANGLE_11hh * speed);

		player->mo->rollangle = 0;
	}
	else if (player->nocontrol && player->pflags & PF_SKIDDOWN)
	{
		P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

		if (((player->nocontrol + 5) % 20) < 10)
			player->drawangle += ANGLE_11hh;
		else
			player->drawangle -= ANGLE_11hh;
	}
	else
	{
		K_KartMoveAnimation(player);

		if (player->pogospring)
			player->drawangle += ANGLE_22h;
		else
		{
			player->drawangle = player->mo->angle;

			if (player->aizdriftturn)
			{
				player->drawangle += player->aizdriftturn;
			}
			else if (player->drift != 0)
			{
				INT32 a = (ANGLE_45 / 5) * player->drift;

				if (player->mo->eflags & MFE_UNDERWATER)
					a /= 2;

				player->drawangle += a;
			}
		}

		player->mo->rollangle = 0;
	}
	player->mo->movefactor = FRACUNIT; // We're not going to do any more with this, so let's change it back for the next frame.

	//{ SRB2kart

	// Drifting sound
	// Start looping the sound now.
	if (leveltime % 50 == 0 && onground && player->drift != 0)
		S_StartSound(player->mo, sfx_drift);
	// Leveltime being 50 might take a while at times. We'll start it up once, isntantly.
	else if (!S_SoundPlaying(player->mo, sfx_drift) && onground && player->drift != 0)
		S_StartSound(player->mo, sfx_drift);
	// Ok, we'll stop now.
	else if (player->drift == 0)
		S_StopSoundByID(player->mo, sfx_drift);

	K_MoveKartPlayer(player, onground);
	//}

//////////////////
//GAMEPLAY STUFF//
//////////////////

	if (((!(player->mo->eflags & MFE_VERTICALFLIP) && player->mo->z + player->mo->height >= player->mo->watertop && player->mo->z <= player->mo->watertop)
		|| (player->mo->eflags & MFE_VERTICALFLIP && player->mo->z + player->mo->height >= player->mo->waterbottom && player->mo->z <= player->mo->waterbottom))
	&& (player->speed > runspd)
	&& player->mo->momz == 0 && player->carry != CR_SLIDING && !player->spectator)
	{
		fixed_t playerTopSpeed = K_GetKartSpeed(player, false, false);
		fixed_t trailScale = FixedMul(FixedDiv(player->speed - runspd, playerTopSpeed - runspd), mapobjectscale);

		if (playerTopSpeed > runspd)
			trailScale = FixedMul(FixedDiv(player->speed - runspd, playerTopSpeed - runspd), mapobjectscale);
		else
			trailScale = mapobjectscale; // Scaling is based off difference between runspeed and top speed

		if (trailScale > 0)
		{
			const angle_t forwardangle = K_MomentumAngle(player->mo);
			const fixed_t playerVisualRadius = player->mo->radius + (8 * player->mo->scale);
			const size_t numFrames = S_WATERTRAIL8 - S_WATERTRAIL1;
			const statenum_t curOverlayFrame = S_WATERTRAIL1 + (leveltime % numFrames);
			const statenum_t curUnderlayFrame = S_WATERTRAILUNDERLAY1 + (leveltime % numFrames);
			fixed_t x1, x2, y1, y2;
			mobj_t *water;

			x1 = player->mo->x + player->mo->momx + P_ReturnThrustX(player->mo, forwardangle + ANGLE_90, playerVisualRadius);
			y1 = player->mo->y + player->mo->momy + P_ReturnThrustY(player->mo, forwardangle + ANGLE_90, playerVisualRadius);
			x1 = x1 + P_ReturnThrustX(player->mo, forwardangle, playerVisualRadius);
			y1 = y1 + P_ReturnThrustY(player->mo, forwardangle, playerVisualRadius);

			x2 = player->mo->x + player->mo->momx + P_ReturnThrustX(player->mo, forwardangle - ANGLE_90, playerVisualRadius);
			y2 = player->mo->y + player->mo->momy + P_ReturnThrustY(player->mo, forwardangle - ANGLE_90, playerVisualRadius);
			x2 = x2 + P_ReturnThrustX(player->mo, forwardangle, playerVisualRadius);
			y2 = y2 + P_ReturnThrustY(player->mo, forwardangle, playerVisualRadius);

			// Left
			// underlay
			water = P_SpawnMobj(x1, y1,
				((player->mo->eflags & MFE_VERTICALFLIP) ? player->mo->waterbottom - FixedMul(mobjinfo[MT_WATERTRAILUNDERLAY].height, player->mo->scale) : player->mo->watertop), MT_WATERTRAILUNDERLAY);
			P_InitAngle(water, forwardangle - ANGLE_180 - ANGLE_22h);
			water->destscale = trailScale;
			water->momx = player->mo->momx;
			water->momy = player->mo->momy;
			water->momz = player->mo->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curUnderlayFrame);

			// overlay
			water = P_SpawnMobj(x1, y1,
				((player->mo->eflags & MFE_VERTICALFLIP) ? player->mo->waterbottom - FixedMul(mobjinfo[MT_WATERTRAIL].height, player->mo->scale) : player->mo->watertop), MT_WATERTRAIL);
			P_InitAngle(water, forwardangle - ANGLE_180 - ANGLE_22h);
			water->destscale = trailScale;
			water->momx = player->mo->momx;
			water->momy = player->mo->momy;
			water->momz = player->mo->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curOverlayFrame);

			// Right
			// Underlay
			water = P_SpawnMobj(x2, y2,
				((player->mo->eflags & MFE_VERTICALFLIP) ? player->mo->waterbottom - FixedMul(mobjinfo[MT_WATERTRAILUNDERLAY].height, player->mo->scale) : player->mo->watertop), MT_WATERTRAILUNDERLAY);
			P_InitAngle(water, forwardangle - ANGLE_180 + ANGLE_22h);
			water->destscale = trailScale;
			water->momx = player->mo->momx;
			water->momy = player->mo->momy;
			water->momz = player->mo->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curUnderlayFrame);

			// Overlay
			water = P_SpawnMobj(x2, y2,
				((player->mo->eflags & MFE_VERTICALFLIP) ? player->mo->waterbottom - FixedMul(mobjinfo[MT_WATERTRAIL].height, player->mo->scale) : player->mo->watertop), MT_WATERTRAIL);
			P_InitAngle(water, forwardangle - ANGLE_180 + ANGLE_22h);
			water->destscale = trailScale;
			water->momx = player->mo->momx;
			water->momy = player->mo->momy;
			water->momz = player->mo->momz;
			P_SetScale(water, trailScale);
			P_SetMobjState(water, curOverlayFrame);

			if (!S_SoundPlaying(player->mo, sfx_s3kdbs))
			{
				const INT32 volume = (min(trailScale, FRACUNIT) * 255) / FRACUNIT;
				S_StartSoundAtVolume(player->mo, sfx_s3kdbs, volume);
			}
		}
	}

	// Little water sound while touching water - just a nicety.
	if ((player->mo->eflags & MFE_TOUCHWATER) && !(player->mo->eflags & MFE_UNDERWATER) && !player->spectator)
	{
		if (P_RandomChance(FRACUNIT/2) && leveltime % TICRATE == 0)
			S_StartSound(player->mo, sfx_floush);
	}

	////////////////////////////
	//SPINNING AND SPINDASHING//
	////////////////////////////

	// SRB2kart - Drifting smoke and fire
	if ((player->sneakertimer || player->flamedash)
		&& onground && (leveltime & 1))
		K_SpawnBoostTrail(player);

	if (player->invincibilitytimer > 0)
		K_SpawnSparkleTrail(player->mo);

	if (player->wipeoutslow > 1 && (leveltime & 1))
		K_SpawnWipeoutTrail(player->mo, false);

	K_DriftDustHandling(player->mo);

	// Crush test...
	if ((player->mo->ceilingz - player->mo->floorz < player->mo->height) && !(player->mo->flags & MF_NOCLIP))
	{
		if (player->spectator)
			P_DamageMobj(player->mo, NULL, NULL, 1, DMG_SPECTATOR); // Respawn crushed spectators
		else
			P_DamageMobj(player->mo, NULL, NULL, 1, DMG_SQUISH); // SRB2kart - we don't kill when squished, we squish when squished.

		if (player->playerstate == PST_DEAD)
			return;
	}

#ifdef FLOORSPLATS
	if (cv_shadow.value && rendermode == render_soft)
		R_AddFloorSplat(player->mo->subsector, player->mo, "SHADOW", player->mo->x,
			player->mo->y, player->mo->floorz, SPLATDRAWMODE_OPAQUE);
#endif

	// Look for blocks to bust up
	// Because of FF_SHATTER, we should look for blocks constantly,
	// not just when spinning or playing as Knuckles
	if (CheckForBustableBlocks)
		P_CheckBustableBlocks(player);

	// Check for a BOUNCY sector!
	if (CheckForBouncySector)
		P_CheckBouncySectors(player);

	// Look for Quicksand!
	if (CheckForQuicksand)
		P_CheckQuicksand(player);

	if (P_IsObjectOnGround(player->mo))
		player->mo->pmomz = 0;
}

static void P_DoZoomTube(player_t *player)
{
	fixed_t speed;
	mobj_t *waypoint = NULL;
	fixed_t dist;
	boolean reverse;

	if (player->speed > 0)
		reverse = false;
	else
		reverse = true;

	player->flashing = 1;

	speed = abs(player->speed);

	// change slope
	dist = P_AproxDistance(P_AproxDistance(player->mo->tracer->x - player->mo->x, player->mo->tracer->y - player->mo->y), player->mo->tracer->z - player->mo->z);

	if (dist < 1)
		dist = 1;

	player->mo->momx = FixedMul(FixedDiv(player->mo->tracer->x - player->mo->x, dist), (speed));
	player->mo->momy = FixedMul(FixedDiv(player->mo->tracer->y - player->mo->y, dist), (speed));
	player->mo->momz = FixedMul(FixedDiv(player->mo->tracer->z - player->mo->z, dist), (speed));

	// Calculate the distance between the player and the waypoint
	// 'dist' already equals this.

	// Will the player go past the waypoint?
	if (speed > dist)
	{
		speed -= dist;
		// If further away, set XYZ of player to waypoint location
		P_UnsetThingPosition(player->mo);
		player->mo->x = player->mo->tracer->x;
		player->mo->y = player->mo->tracer->y;
		player->mo->z = player->mo->tracer->z;
		P_SetThingPosition(player->mo);

		// ugh, duh!!
		player->mo->floorz = player->mo->subsector->sector->floorheight;
		player->mo->ceilingz = player->mo->subsector->sector->ceilingheight;

		CONS_Debug(DBG_GAMELOGIC, "Looking for next waypoint...\n");

		// Find next waypoint
		waypoint = reverse ? P_GetPreviousTubeWaypoint(player->mo->tracer, false) : P_GetNextTubeWaypoint(player->mo->tracer, false);

		if (waypoint)
		{
			CONS_Debug(DBG_GAMELOGIC, "Found waypoint (sequence %d, number %d).\n", waypoint->threshold, waypoint->health);

			P_SetTarget(&player->mo->tracer, waypoint);

			// calculate MOMX/MOMY/MOMZ for next waypoint

			// change slope
			dist = P_AproxDistance(P_AproxDistance(player->mo->tracer->x - player->mo->x, player->mo->tracer->y - player->mo->y), player->mo->tracer->z - player->mo->z);

			if (dist < 1)
				dist = 1;

			player->mo->momx = FixedMul(FixedDiv(player->mo->tracer->x - player->mo->x, dist), (speed));
			player->mo->momy = FixedMul(FixedDiv(player->mo->tracer->y - player->mo->y, dist), (speed));
			player->mo->momz = FixedMul(FixedDiv(player->mo->tracer->z - player->mo->z, dist), (speed));
		}
		else
		{
			P_SetTarget(&player->mo->tracer, NULL); // Else, we just let them fly.
			player->carry = CR_NONE;

			CONS_Debug(DBG_GAMELOGIC, "Next waypoint not found, releasing from track...\n");
		}
	}

	// change angle
	if (player->mo->tracer)
	{
		player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y, player->mo->tracer->x, player->mo->tracer->y);
		P_SetPlayerAngle(player, player->mo->angle);
	}

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
	player->drawangle -= ANGLE_22h;
}

#if 0
//
// P_NukeAllPlayers
//
// Hurts all players
// source = guy who gets the credit
//
static void P_NukeAllPlayers(player_t *player)
{
	mobj_t *mo;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		if (players[i].spectator)
			continue;
		if (!players[i].mo)
			continue;
		if (players[i].mo == player->mo)
			continue;
		if (players[i].mo->health <= 0)
			continue;

		P_DamageMobj(players[i].mo, player->mo, player->mo, 1, DMG_NORMAL);
	}

	CONS_Printf(M_GetText("%s caused a world of pain.\n"), player_names[player-players]);

	return;
}
#endif

//
// P_NukeEnemies
// Looks for something you can hit - Used for bomb shield
//
void P_NukeEnemies(mobj_t *inflictor, mobj_t *source, fixed_t radius)
{
	mobj_t *mo;
	thinker_t *think;

	radius = FixedMul(radius, mapobjectscale);

	for (think = thlist[THINK_MOBJ].next; think != &thlist[THINK_MOBJ]; think = think->next)
	{
		if (think->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mo = (mobj_t *)think;

		if (!(mo->flags & MF_SHOOTABLE) && (mo->type != MT_SPB)) // Don't want to give SPB MF_SHOOTABLE, to ensure it's undamagable through other means
			continue;

		if (mo->flags & MF_MONITOR)
			continue; // Monitors cannot be 'nuked'.

		if (abs(inflictor->x - mo->x) > radius || abs(inflictor->y - mo->y) > radius || abs(inflictor->z - mo->z) > radius)
			continue; // Workaround for possible integer overflow in the below -Red

		if (P_AproxDistance(P_AproxDistance(inflictor->x - mo->x, inflictor->y - mo->y), inflictor->z - mo->z) > radius)
			continue;

		if (mo->type == MT_SPB) // If you destroy a SPB, you don't get the luxury of a cooldown.
		{
			spbplace = -1;
			indirectitemcooldown = 0;
		}

		if (mo->flags & MF_BOSS) //don't OHKO bosses nor players!
			P_DamageMobj(mo, inflictor, source, 1, DMG_NORMAL|DMG_CANTHURTSELF);
		else if (mo->type == MT_PLAYER)	// Lightning shield: Combo players.
			P_DamageMobj(mo, inflictor, source, 1, DMG_NORMAL|DMG_CANTHURTSELF|DMG_WOMBO);
		else
			P_DamageMobj(mo, inflictor, source, 1000, DMG_NORMAL|DMG_CANTHURTSELF);
	}
}

//
// P_ConsiderAllGone
// Shamelessly lifted from TD. Thanks, Sryder!
//

// SRB2Kart: Use for GP?
/*
static void P_ConsiderAllGone(void)
{
	INT32 i, lastdeadplayer = -1, deadtimercheck = INT32_MAX;

	if (countdown2)
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		if (players[i].playerstate != PST_DEAD && !players[i].spectator && players[i].mo && players[i].mo->health)
			break;

		if (players[i].spectator)
		{
			if (lastdeadplayer == -1)
				lastdeadplayer = i;
		}
		else if (players[i].lives > 0)
		{
			lastdeadplayer = i;
			if (players[i].deadtimer < deadtimercheck)
				deadtimercheck = players[i].deadtimer;
		}
	}

	if (i == MAXPLAYERS && lastdeadplayer != -1 && deadtimercheck > 2*TICRATE) // the last killed player will reset the level in G_DoReborn
	{
		//players[lastdeadplayer].spectator = true;
		players[lastdeadplayer].outofcoop = true;
		players[lastdeadplayer].playerstate = PST_REBORN;
	}
}
*/

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
static void P_DeathThink(player_t *player)
{
	boolean playerGone = false;

	player->deltaviewheight = 0;

	if (player->deadtimer < INT32_MAX)
		player->deadtimer++;

	if ((player->pflags & PF_NOCONTEST) && (gametyperules & GTR_CIRCUIT))
	{
		player->karthud[khud_timeovercam]++;

		if (player->mo)
		{
			player->mo->flags |= (MF_NOGRAVITY|MF_NOCLIP);
			player->mo->renderflags |= RF_DONTDRAW;
		}
	}
	else
		player->karthud[khud_timeovercam] = 0;

	K_KartPlayerHUDUpdate(player);

	if (player->pflags & PF_NOCONTEST)
	{
		playerGone = true;
	}
	else if (player->bot == false)
	{
		if (G_GametypeUsesLives() == true && player->lives == 0)
		{
			playerGone = true;
		}
	}

	if (playerGone == false && player->deadtimer > TICRATE)
	{
		player->playerstate = PST_REBORN;
	}

	// Keep time rolling
	if (!(exitcountdown && !racecountdown) && !(player->exiting || mapreset) && !(player->pflags & PF_NOCONTEST) && !stoppedclock)
	{
		if (leveltime >= starttime)
		{
			player->realtime = leveltime - starttime;
			if (player == &players[consoleplayer])
			{
				if (player->spectator || !circuitmap)
					curlap = 0;
				else if (curlap != UINT32_MAX)
					curlap++; // This is too complicated to sync to realtime, just sorta hope for the best :V
			}
		}
		else
		{
			player->realtime = 0;
			if (player == &players[consoleplayer])
				curlap = 0;
		}
	}

	if (!player->mo)
		return;

	//K_KartResetPlayerColor(player); -- called at death, don't think we need to re-establish

	P_CalcHeight(player);
}

//
// P_MoveCamera: make sure the camera is not outside the world and looks at the player avatar
//

camera_t camera[MAXSPLITSCREENPLAYERS]; // Four cameras, three for splitscreen

static void CV_CamRotate_OnChange(void)
{
	if (cv_cam_rotate[0].value < 0)
		CV_SetValue(&cv_cam_rotate[0], cv_cam_rotate[0].value + 360);
	else if (cv_cam_rotate[0].value > 359)
		CV_SetValue(&cv_cam_rotate[0], cv_cam_rotate[0].value % 360);
}

static void CV_CamRotate2_OnChange(void)
{
	if (cv_cam_rotate[1].value < 0)
		CV_SetValue(&cv_cam_rotate[1], cv_cam_rotate[1].value + 360);
	else if (cv_cam_rotate[1].value > 359)
		CV_SetValue(&cv_cam_rotate[1], cv_cam_rotate[1].value % 360);
}

static void CV_CamRotate3_OnChange(void)
{
	if (cv_cam_rotate[2].value < 0)
		CV_SetValue(&cv_cam_rotate[2], cv_cam_rotate[2].value + 360);
	else if (cv_cam_rotate[2].value > 359)
		CV_SetValue(&cv_cam_rotate[2], cv_cam_rotate[2].value % 360);
}

static void CV_CamRotate4_OnChange(void)
{
	if (cv_cam_rotate[3].value < 0)
		CV_SetValue(&cv_cam_rotate[3], cv_cam_rotate[3].value + 360);
	else if (cv_cam_rotate[3].value > 359)
		CV_SetValue(&cv_cam_rotate[3], cv_cam_rotate[3].value % 360);
}

static CV_PossibleValue_t CV_CamSpeed[] = {{0, "MIN"}, {1*FRACUNIT, "MAX"}, {0, NULL}};
static CV_PossibleValue_t CV_CamRotate[] = {{-720, "MIN"}, {720, "MAX"}, {0, NULL}};

consvar_t cv_cam_dist[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("cam_dist", "190", CV_FLOAT|CV_SAVE, NULL, NULL),
	CVAR_INIT ("cam2_dist", "190", CV_FLOAT|CV_SAVE, NULL, NULL),
	CVAR_INIT ("cam3_dist", "190", CV_FLOAT|CV_SAVE, NULL, NULL),
	CVAR_INIT ("cam4_dist", "190", CV_FLOAT|CV_SAVE, NULL, NULL)
};

consvar_t cv_cam_height[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("cam_height", "75", CV_FLOAT|CV_SAVE, NULL, NULL),
	CVAR_INIT ("cam2_height", "75", CV_FLOAT|CV_SAVE, NULL, NULL),
	CVAR_INIT ("cam3_height", "75", CV_FLOAT|CV_SAVE, NULL, NULL),
	CVAR_INIT ("cam4_height", "75", CV_FLOAT|CV_SAVE, NULL, NULL)
};

consvar_t cv_cam_still[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("cam_still", "Off", 0, CV_OnOff, NULL),
	CVAR_INIT ("cam2_still", "Off", 0, CV_OnOff, NULL),
	CVAR_INIT ("cam3_still", "Off", 0, CV_OnOff, NULL),
	CVAR_INIT ("cam4_still", "Off", 0, CV_OnOff, NULL)
};

consvar_t cv_cam_speed[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("cam_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL),
	CVAR_INIT ("cam2_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL),
	CVAR_INIT ("cam3_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL),
	CVAR_INIT ("cam4_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL)
};

consvar_t cv_cam_rotate[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("cam_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate_OnChange),
	CVAR_INIT ("cam2_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate2_OnChange),
	CVAR_INIT ("cam3_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate3_OnChange),
	CVAR_INIT ("cam4_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate4_OnChange)
};

consvar_t cv_tilting = CVAR_INIT ("tilting", "On", CV_SAVE, CV_OnOff, NULL);

consvar_t cv_actionmovie = CVAR_INIT ("actionmovie", "On", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_windowquake = CVAR_INIT ("windowquake", "Off", CV_SAVE, CV_OnOff, NULL);

fixed_t t_cam_dist[MAXSPLITSCREENPLAYERS] = {-42,-42,-42,-42};
fixed_t t_cam_height[MAXSPLITSCREENPLAYERS] = {-42,-42,-42,-42};
fixed_t t_cam_rotate[MAXSPLITSCREENPLAYERS] = {-42,-42,-42,-42};

// Heavily simplified version of G_BuildTicCmd that only takes the local first player's control input and converts it to readable ticcmd_t
// we then throw that ticcmd garbage in the camera and make it move
// TODO: please just use the normal ticcmd function somehow

static ticcmd_t cameracmd;

struct demofreecam_s democam;

// called by m_menu to reinit cam input every time it's toggled
void P_InitCameraCmd(void)
{
	memset(&cameracmd, 0, sizeof(ticcmd_t));	// initialize cmd
}

static ticcmd_t *P_CameraCmd(camera_t *cam)
{
	INT32 forward, side, axis; //i
	// these ones used for multiple conditions
	boolean turnleft, turnright, mouseaiming;
	boolean invertmouse, lookaxis, usejoystick, kbl;
	INT32 player_invert;
	INT32 screen_invert;
	ticcmd_t *cmd = &cameracmd;

	(void)cam;

	if (!demo.playback)
		return cmd;	// empty cmd, no.

	kbl = democam.keyboardlook;

	G_CopyTiccmd(cmd, I_BaseTiccmd(), 1); // empty, or external driver

	mouseaiming = true;
	invertmouse = cv_invertmouse.value;
	lookaxis = cv_lookaxis[0].value;

	usejoystick = true;
	turnright = PlayerInputDown(1, gc_turnright);
	turnleft = PlayerInputDown(1, gc_turnleft);

	axis = PlayerJoyAxis(1, AXISTURN);

	if (encoremode)
	{
		turnright ^= turnleft; // swap these using three XORs
		turnleft ^= turnright;
		turnright ^= turnleft;
		axis = -axis;
	}

	if (axis != 0)
	{
		turnright = turnright || (axis > 0);
		turnleft = turnleft || (axis < 0);
	}
	forward = 0;

	cmd->turning = 0;

	// let movement keys cancel each other out
	if (turnright && !(turnleft))
	{
		cmd->turning -= KART_FULLTURN;
		side += 4;
	}
	else if (turnleft && !(turnright))
	{
		cmd->turning += KART_FULLTURN;
		side -= 4;
	}

	cmd->turning -= (mousex * 8) * (encoremode ? -1 : 1);

	axis = PlayerJoyAxis(1, AXISMOVE);
	if (PlayerInputDown(1, gc_accelerate) || (usejoystick && axis > 0))
		cmd->buttons |= BT_ACCELERATE;
	axis = PlayerJoyAxis(1, AXISBRAKE);
	if (PlayerInputDown(1, gc_brake) || (usejoystick && axis > 0))
		cmd->buttons |= BT_BRAKE;
	axis = PlayerJoyAxis(1, AXISAIM);
	if (PlayerInputDown(1, gc_aimforward) || (usejoystick && axis < 0))
		forward += MAXPLMOVE;
	if (PlayerInputDown(1, gc_aimbackward) || (usejoystick && axis > 0))
		forward -= MAXPLMOVE;

	// fire with any button/key
	axis = PlayerJoyAxis(1, AXISFIRE);
	if (PlayerInputDown(1, gc_fire) || (usejoystick && axis > 0))
		cmd->buttons |= BT_ATTACK;

	// spectator aiming shit, ahhhh...
	player_invert = invertmouse ? -1 : 1;
	screen_invert = 1;	// nope

	// mouse look stuff (mouse look is not the same as mouse aim)
	kbl = false;

	// looking up/down
	cmd->aiming += (mlooky<<19)*player_invert*screen_invert;

	axis = PlayerJoyAxis(1, AXISLOOK);

	// spring back if not using keyboard neither mouselookin'
	if (!kbl && !lookaxis && !mouseaiming)
		cmd->aiming = 0;

	if (PlayerInputDown(1, gc_lookup) || (axis < 0))
	{
		cmd->aiming += KB_LOOKSPEED * screen_invert;
		kbl = true;
	}
	else if (PlayerInputDown(1, gc_lookdown) || (axis > 0))
	{
		cmd->aiming -= KB_LOOKSPEED * screen_invert;
		kbl = true;
	}

	if (PlayerInputDown(1, gc_centerview)) // No need to put a spectator limit on this one though :V
		cmd->aiming = 0;

	mousex = mousey = mlooky = 0;

	cmd->forwardmove += (SINT8)forward;
	cmd->sidemove += (SINT8)side;

	if (cmd->forwardmove > MAXPLMOVE)
		cmd->forwardmove = MAXPLMOVE;
	else if (cmd->forwardmove < -MAXPLMOVE)
		cmd->forwardmove = -MAXPLMOVE;
	
	if (cmd->sidemove > MAXPLMOVE)
		cmd->sidemove = MAXPLMOVE;
	else if (cmd->sidemove < -MAXPLMOVE)
		cmd->sidemove = -MAXPLMOVE;

	if (cmd->turning > KART_FULLTURN)
		cmd->turning = KART_FULLTURN;
	else if (cmd->turning < -KART_FULLTURN)
		cmd->turning = -KART_FULLTURN;

	democam.keyboardlook = kbl;

	return cmd;
}

void P_DemoCameraMovement(camera_t *cam)
{
	ticcmd_t *cmd;
	angle_t thrustangle;
	mobj_t *awayviewmobj_hack;
	player_t *lastp;

	// update democam stuff with what we got here:
	democam.cam = cam;
	democam.localangle = cam->angle;
	democam.localaiming = cam->aiming;

	// first off we need to get button input
	cmd = P_CameraCmd(cam);

	cam->aiming += cmd->aiming << TICCMD_REDUCE;
	cam->angle += cmd->turning << TICCMD_REDUCE;

	democam.localangle += cmd->turning << TICCMD_REDUCE;
	democam.localaiming += cmd->aiming << TICCMD_REDUCE;

	cam->aiming = G_ClipAimingPitch((INT32 *)&cam->aiming);
	democam.localaiming = G_ClipAimingPitch((INT32 *)&democam.localaiming);

	// camera movement:
	if (cmd->buttons & BT_ACCELERATE)
		cam->z += 32*mapobjectscale;
	else if (cmd->buttons & BT_BRAKE)
		cam->z -= 32*mapobjectscale;

	// if you hold item, you will lock on to displayplayer. (The last player you were ""f12-ing"")
	if (cmd->buttons & BT_ATTACK)
	{
		lastp = &players[displayplayers[0]];	// Fun fact, I was trying displayplayers[0]->mo as if it was Lua like an absolute idiot.
		cam->angle = R_PointToAngle2(cam->x, cam->y, lastp->mo->x, lastp->mo->y);
		cam->aiming = R_PointToAngle2(0, cam->z, R_PointToDist2(cam->x, cam->y, lastp->mo->x, lastp->mo->y), lastp->mo->z + lastp->mo->scale*128*P_MobjFlip(lastp->mo));	// This is still unholy. Aim a bit above their heads.
	}

	cam->momx = cam->momy = cam->momz = 0;

	if (cmd->forwardmove != 0)
	{
		thrustangle = cam->angle >> ANGLETOFINESHIFT;

		cam->x += FixedMul(cmd->forwardmove*mapobjectscale, FINECOSINE(thrustangle));
		cam->y += FixedMul(cmd->forwardmove*mapobjectscale, FINESINE(thrustangle));
		cam->z += FixedMul(cmd->forwardmove*mapobjectscale, AIMINGTOSLOPE(cam->aiming));
		// momentums are useless here, directly add to the coordinates

		// this.......... doesn't actually check for floors and walls and whatnot but the function to do that is a pure mess so fuck that.
		// besides freecam going inside walls sounds pretty cool on paper.
	}

	// awayviewmobj hack; this is to prevent us from hearing sounds from the player's perspective

	awayviewmobj_hack = P_SpawnMobj(cam->x, cam->y, cam->z, MT_THOK);
	awayviewmobj_hack->tics = 2;
	awayviewmobj_hack->renderflags |= RF_DONTDRAW;

	democam.soundmobj = awayviewmobj_hack;

	// update subsector to avoid crashes;
	cam->subsector = R_PointInSubsector(cam->x, cam->y);
}

void P_ResetCamera(player_t *player, camera_t *thiscam)
{
	tic_t tries = 0;
	fixed_t x, y, z;

	if (demo.freecam)
		return;	// do not reset the camera there.

	if (!player->mo)
		return;

	if (thiscam->chase && player->mo->health <= 0)
		return;

	thiscam->chase = !player->spectator;
	x = player->mo->x - P_ReturnThrustX(player->mo, thiscam->angle, player->mo->radius);
	y = player->mo->y - P_ReturnThrustY(player->mo, thiscam->angle, player->mo->radius);
	if (player->mo->eflags & MFE_VERTICALFLIP)
		z = player->mo->z + player->mo->height - P_GetPlayerViewHeight(player) - 16*FRACUNIT;
	else
		z = player->mo->z + P_GetPlayerViewHeight(player);

	// set bits for the camera
	thiscam->x = x;
	thiscam->y = y;
	thiscam->z = z;

	thiscam->angle = player->mo->angle;
	thiscam->aiming = 0;
	thiscam->relativex = 0;

	thiscam->subsector = R_PointInSubsector(thiscam->x,thiscam->y);

	thiscam->radius = 20*FRACUNIT;
	thiscam->height = 16*FRACUNIT;

	while (!P_MoveChaseCamera(player,thiscam,true) && ++tries < 2*TICRATE);
}

boolean P_MoveChaseCamera(player_t *player, camera_t *thiscam, boolean resetcalled)
{
	static boolean lookbackactive[MAXSPLITSCREENPLAYERS];
	static UINT8 lookbackdelay[MAXSPLITSCREENPLAYERS];
	UINT8 num;
	angle_t angle = 0, focusangle = 0, focusaiming = 0, pitch = 0;
	fixed_t x, y, z, dist, distxy, distz, viewpointx, viewpointy, camspeed, camdist, camheight, pviewheight;
	fixed_t pan, xpan, ypan;
	INT32 camrotate;
	boolean camstill, lookback, lookbackdown;
	UINT8 timeover;
	mobj_t *mo;
	fixed_t f1, f2;
	fixed_t speed;
#ifndef NOCLIPCAM
	boolean cameranoclip;
	subsector_t *newsubsec;
#endif

	thiscam->old_x = thiscam->x;
	thiscam->old_y = thiscam->y;
	thiscam->old_z = thiscam->z;
	thiscam->old_angle = thiscam->angle;
	thiscam->old_aiming = thiscam->aiming;

	democam.soundmobj = NULL;	// reset this each frame, we don't want the game crashing for stupid reasons now do we

	// We probably shouldn't move the camera if there is no player or player mobj somehow
	if (!player || !player->mo)
		return true;

	// This can happen when joining
	if (thiscam->subsector == NULL || thiscam->subsector->sector == NULL)
		return true;

	if (demo.freecam)
	{
		P_DemoCameraMovement(thiscam);
		return true;
	}

	if (thiscam == &camera[1]) // Camera 2
	{
		num = 1;
	}
	else if (thiscam == &camera[2]) // Camera 3
	{
		num = 2;
	}
	else if (thiscam == &camera[3]) // Camera 4
	{
		num = 3;
	}
	else // Camera 1
	{
		num = 0;
	}

	mo = player->mo;

	if (player->playerstate == PST_DEAD)
	{
		// Do not move the camera while dead!
		if (thiscam->chase)
			P_CalcChasePostImg(player, thiscam);

		return true;
	}

#ifndef NOCLIPCAM
	cameranoclip = ((player->pflags & PF_NOCLIP)
		|| (mo->flags & (MF_NOCLIP|MF_NOCLIPHEIGHT)) // Noclipping player camera noclips too!!
		|| (leveltime < introtime)); // Kart intro cam
#endif

	if ((player->pflags & PF_NOCONTEST) && (gametyperules & GTR_CIRCUIT)) // 1 for momentum keep, 2 for turnaround
		timeover = (player->karthud[khud_timeovercam] > 2*TICRATE ? 2 : 1);
	else
		timeover = 0;

	if (!(player->playerstate == PST_DEAD || player->exiting || leveltime < introtime))
	{
		if (player->spectator) // force cam off for spectators
			return true;

		if (!cv_chasecam[num].value && thiscam == &camera[num])
			return true;
	}

	if (!thiscam->chase && !resetcalled)
	{
		focusangle = localangle[num];
		camrotate = cv_cam_rotate[num].value;

		if (leveltime < introtime) // Whoooshy camera!
		{
			const INT32 introcam = (introtime - leveltime);
			camrotate += introcam*5;
		}

		thiscam->angle = focusangle + FixedAngle(camrotate*FRACUNIT);
		P_ResetCamera(player, thiscam);
		return true;
	}

	thiscam->radius = 20*mapobjectscale;
	thiscam->height = 16*mapobjectscale;

	// Don't run while respawning from a starpost
	// Inu 4/8/13 Why not?!
//	if (leveltime > 0 && timeinmap <= 0)
//		return true;

	if (demo.playback)
	{
		focusangle = mo->angle;
		focusaiming = 0;
	}
	else
	{
		focusangle = localangle[num];
		focusaiming = localaiming[num];
	}

	if (P_CameraThinker(player, thiscam, resetcalled))
		return true;

	lookback = ( player->cmd.buttons & BT_LOOKBACK );

	camspeed = cv_cam_speed[num].value;
	camstill = cv_cam_still[num].value;
	camrotate = cv_cam_rotate[num].value;
	camdist = FixedMul(cv_cam_dist[num].value, mapobjectscale);
	camheight = FixedMul(cv_cam_height[num].value, mapobjectscale);

	if (timeover)
	{
		const INT32 timeovercam = max(0, min(180, (player->karthud[khud_timeovercam] - 2*TICRATE)*15));
		camrotate += timeovercam;
	}
	else if (leveltime < introtime && !(modeattacking && !demo.playback)) // Whoooshy camera! (don't do this in RA when we PLAY, still do it in replays however~)
	{
		const INT32 introcam = (introtime - leveltime);
		camrotate += introcam*5;
		camdist += (introcam * mapobjectscale)*3;
		camheight += (introcam * mapobjectscale)*2;
	}
	else if (player->exiting) // SRB2Kart: Leave the camera behind while exiting, for dramatic effect!
		camstill = true;
	else if (lookback || lookbackdelay[num]) // SRB2kart - Camera flipper
	{
#define MAXLOOKBACKDELAY 2
		camspeed = FRACUNIT;
		if (lookback)
		{
			camrotate += 180;
			lookbackdelay[num] = MAXLOOKBACKDELAY;
		}
		else
			lookbackdelay[num]--;
	}
	else if (player->respawn.state != RESPAWNST_NONE)
	{
		camspeed = 3*FRACUNIT/4;
	}
	lookbackdown = (lookbackdelay[num] == MAXLOOKBACKDELAY) != lookbackactive[num];
	lookbackactive[num] = (lookbackdelay[num] == MAXLOOKBACKDELAY);
#undef MAXLOOKBACKDELAY

	if (mo->eflags & MFE_VERTICALFLIP)
		camheight += thiscam->height;

	if (camspeed > FRACUNIT)
		camspeed = FRACUNIT;

	if (timeover)
		angle = mo->angle + FixedAngle(camrotate*FRACUNIT);
	else if (leveltime < introtime)
		angle = focusangle + FixedAngle(camrotate*FRACUNIT);
	else if (camstill || resetcalled || player->playerstate == PST_DEAD)
		angle = thiscam->angle;
	else
	{
		if (camspeed == FRACUNIT)
			angle = focusangle + FixedAngle(camrotate<<FRACBITS);
		else
		{
			angle_t input = focusangle + FixedAngle(camrotate<<FRACBITS) - thiscam->angle;
			boolean invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			input = FixedAngle(FixedMul(AngleFixed(input), camspeed));
			if (invert)
				input = InvAngle(input);

			angle = thiscam->angle + input;
		}
	}

	if (!resetcalled && (leveltime >= introtime && timeover != 2)
		&& (t_cam_rotate[num] != -42))
	{
		angle = FixedAngle(camrotate*FRACUNIT);
		thiscam->angle = angle;
	}

	// sets ideal cam pos
	{
		const fixed_t speedthreshold = 48*mapobjectscale;
		const fixed_t olddist = P_AproxDistance(mo->x - thiscam->x, mo->y - thiscam->y);

		fixed_t lag, distoffset;

		dist = camdist;

		if (player->karthud[khud_boostcam])
		{
			dist -= FixedMul(11*dist/16, player->karthud[khud_boostcam]);
		}

		speed = P_AproxDistance(P_AproxDistance(mo->momx, mo->momy), mo->momz / 16);
		lag = FRACUNIT - ((FixedDiv(speed, speedthreshold) - FRACUNIT) * 2);

		if (lag > FRACUNIT)
		{
			lag = FRACUNIT;
		}

		if (lag < camspeed)
		{
			lag = camspeed;
		}

		distoffset = dist - olddist;
		dist = olddist + FixedMul(distoffset, lag);

		if (dist < 0)
		{
			dist = 0;
		}
	}

	if (mo->standingslope)
	{
		pitch = (angle_t)FixedMul(P_ReturnThrustX(mo, thiscam->angle - mo->standingslope->xydirection, FRACUNIT), (fixed_t)mo->standingslope->zangle);
		if (mo->eflags & MFE_VERTICALFLIP)
		{
			if (pitch >= ANGLE_180)
				pitch = 0;
		}
		else
		{
			if (pitch < ANGLE_180)
				pitch = 0;
		}
	}
	pitch = thiscam->pitch + (angle_t)FixedMul(pitch - thiscam->pitch, camspeed/4);

	if (rendermode == render_opengl && !cv_glshearing.value)
		distxy = FixedMul(dist, FINECOSINE((pitch>>ANGLETOFINESHIFT) & FINEMASK));
	else
		distxy = dist;
	distz = -FixedMul(dist, FINESINE((pitch>>ANGLETOFINESHIFT) & FINEMASK));

	x = mo->x - FixedMul(FINECOSINE((angle>>ANGLETOFINESHIFT) & FINEMASK), distxy);
	y = mo->y - FixedMul(FINESINE((angle>>ANGLETOFINESHIFT) & FINEMASK), distxy);

	// SRB2Kart: set camera panning
	if (camstill || resetcalled || player->playerstate == PST_DEAD)
		pan = xpan = ypan = 0;
	else
	{
		if (player->drift != 0)
		{
			fixed_t panmax = (dist/5);
			INT32 driftval = K_GetKartDriftSparkValue(player);
			INT32 dc = player->driftcharge;

			if (dc > driftval || dc < 0)
				dc = driftval;

			pan = FixedDiv(FixedMul((fixed_t)dc, panmax), driftval);

			if (pan > panmax)
				pan = panmax;
			if (player->drift < 0)
				pan *= -1;
		}
		else
			pan = 0;

		pan = thiscam->pan + FixedMul(pan - thiscam->pan, camspeed/4);

		xpan = FixedMul(FINECOSINE(((angle+ANGLE_90)>>ANGLETOFINESHIFT) & FINEMASK), pan);
		ypan = FixedMul(FINESINE(((angle+ANGLE_90)>>ANGLETOFINESHIFT) & FINEMASK), pan);

		x += xpan;
		y += ypan;
	}

	pviewheight = FixedMul(32<<FRACBITS, mo->scale);

	if (mo->eflags & MFE_VERTICALFLIP)
	{
		distz = min(-camheight, distz);
		z = mo->z + mo->height - pviewheight + distz;
	}
	else
	{
		distz = max(camheight, distz);
		z = mo->z + pviewheight + distz;
	}

#ifndef NOCLIPCAM // Disable all z-clipping for noclip cam
	// move camera down to move under lower ceilings
	newsubsec = R_PointInSubsectorOrNull(((mo->x>>FRACBITS) + (thiscam->x>>FRACBITS))<<(FRACBITS-1), ((mo->y>>FRACBITS) + (thiscam->y>>FRACBITS))<<(FRACBITS-1));

	if (!newsubsec)
		newsubsec = thiscam->subsector;

	if (newsubsec)
	{
		fixed_t myfloorz, myceilingz;
		fixed_t midz = thiscam->z + (thiscam->z - mo->z)/2;
		fixed_t midx = ((mo->x>>FRACBITS) + (thiscam->x>>FRACBITS))<<(FRACBITS-1);
		fixed_t midy = ((mo->y>>FRACBITS) + (thiscam->y>>FRACBITS))<<(FRACBITS-1);

		// Cameras use the heightsec's heights rather then the actual sector heights.
		// If you can see through it, why not move the camera through it too?
		if (newsubsec->sector->camsec >= 0)
		{
			myfloorz = sectors[newsubsec->sector->camsec].floorheight;
			myceilingz = sectors[newsubsec->sector->camsec].ceilingheight;
		}
		else if (newsubsec->sector->heightsec >= 0)
		{
			myfloorz = sectors[newsubsec->sector->heightsec].floorheight;
			myceilingz = sectors[newsubsec->sector->heightsec].ceilingheight;
		}
		else
		{
			myfloorz = P_CameraGetFloorZ(thiscam, newsubsec->sector, midx, midy, NULL);
			myceilingz = P_CameraGetCeilingZ(thiscam, newsubsec->sector, midx, midy, NULL);
		}

		// Check list of fake floors and see if floorz/ceilingz need to be altered.
		if (newsubsec->sector->ffloors)
		{
			ffloor_t *rover;
			fixed_t delta1, delta2;
			INT32 thingtop = midz + thiscam->height;

			for (rover = newsubsec->sector->ffloors; rover; rover = rover->next)
			{
				fixed_t topheight, bottomheight;
				if (!(rover->flags & FF_BLOCKOTHERS) || !(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERALL) || GETSECSPECIAL(rover->master->frontsector->special, 4) == 12)
					continue;

				topheight = P_CameraGetFOFTopZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);
				bottomheight = P_CameraGetFOFBottomZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);

				delta1 = midz - (bottomheight
					+ ((topheight - bottomheight)/2));
				delta2 = thingtop - (bottomheight
					+ ((topheight - bottomheight)/2));
				if (topheight > myfloorz && abs(delta1) < abs(delta2))
					myfloorz = topheight;
				if (bottomheight < myceilingz && abs(delta1) >= abs(delta2))
					myceilingz = bottomheight;
			}
		}

	// Check polyobjects and see if floorz/ceilingz need to be altered
	{
		INT32 xl, xh, yl, yh, bx, by;
		validcount++;

		xl = (unsigned)(tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
		xh = (unsigned)(tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
		yl = (unsigned)(tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
		yh = (unsigned)(tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

		BMBOUNDFIX(xl, xh, yl, yh);

		for (by = yl; by <= yh; by++)
			for (bx = xl; bx <= xh; bx++)
			{
				INT32 offset;
				polymaplink_t *plink; // haleyjd 02/22/06

				if (bx < 0 || by < 0 || bx >= bmapwidth || by >= bmapheight)
					continue;

				offset = by*bmapwidth + bx;

				// haleyjd 02/22/06: consider polyobject lines
				plink = polyblocklinks[offset];

				while (plink)
				{
					polyobj_t *po = plink->po;

					if (po->validcount != validcount) // if polyobj hasn't been checked
					{
						sector_t *polysec;
						fixed_t delta1, delta2, thingtop;
						fixed_t polytop, polybottom;

						po->validcount = validcount;

						if (!P_PointInsidePolyobj(po, x, y) || !(po->flags & POF_SOLID))
						{
							plink = (polymaplink_t *)(plink->link.next);
							continue;
						}

						// We're inside it! Yess...
						polysec = po->lines[0]->backsector;

						if (GETSECSPECIAL(polysec->special, 4) == 12)
						{ // Camera noclip polyobj.
							plink = (polymaplink_t *)(plink->link.next);
							continue;
						}

						if (po->flags & POF_CLIPPLANES)
						{
							polytop = polysec->ceilingheight;
							polybottom = polysec->floorheight;
						}
						else
						{
							polytop = INT32_MAX;
							polybottom = INT32_MIN;
						}

						thingtop = midz + thiscam->height;
						delta1 = midz - (polybottom + ((polytop - polybottom)/2));
						delta2 = thingtop - (polybottom + ((polytop - polybottom)/2));

						if (polytop > myfloorz && abs(delta1) < abs(delta2))
							myfloorz = polytop;

						if (polybottom < myceilingz && abs(delta1) >= abs(delta2))
							myceilingz = polybottom;
					}
					plink = (polymaplink_t *)(plink->link.next);
				}
			}
	}

		// crushed camera
		if (myceilingz <= myfloorz + thiscam->height && !resetcalled && !cameranoclip)
		{
			P_ResetCamera(player, thiscam);
			return true;
		}

		// camera fit?
		if (myceilingz != myfloorz
			&& myceilingz - thiscam->height < z)
		{
/*			// no fit
			if (!resetcalled && !cameranoclip)
			{
				P_ResetCamera(player, thiscam);
				return true;
			}
*/
			z = myceilingz - thiscam->height-FixedMul(11*FRACUNIT, mo->scale);
			// is the camera fit is there own sector
		}

		// Make the camera a tad smarter with 3d floors
		if (newsubsec->sector->ffloors && !cameranoclip)
		{
			ffloor_t *rover;

			for (rover = newsubsec->sector->ffloors; rover; rover = rover->next)
			{
				fixed_t topheight, bottomheight;
				if ((rover->flags & FF_BLOCKOTHERS) && (rover->flags & FF_RENDERALL) && (rover->flags & FF_EXISTS) && GETSECSPECIAL(rover->master->frontsector->special, 4) == 12)
				{
					topheight = P_CameraGetFOFTopZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);
					bottomheight = P_CameraGetFOFBottomZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);

					if (bottomheight - thiscam->height < z
						&& midz < bottomheight)
						z = bottomheight - thiscam->height-FixedMul(11*FRACUNIT, mo->scale);

					else if (topheight + thiscam->height > z
						&& midz > topheight)
						z = topheight;

					if ((mo->z >= topheight && midz < bottomheight)
						|| ((mo->z < bottomheight && mo->z+mo->height < topheight) && midz >= topheight))
					{
						// Can't see
						if (!resetcalled)
							P_ResetCamera(player, thiscam);
						return true;
					}
				}
			}
		}
	}

	if (thiscam->z < thiscam->floorz && !cameranoclip)
		thiscam->z = thiscam->floorz;
#endif // NOCLIPCAM

	// point viewed by the camera
	// this point is just 64 unit forward the player
	dist = 64*mapobjectscale;
	viewpointx = mo->x + FixedMul(FINECOSINE((angle>>ANGLETOFINESHIFT) & FINEMASK), dist) + xpan;
	viewpointy = mo->y + FixedMul(FINESINE((angle>>ANGLETOFINESHIFT) & FINEMASK), dist) + ypan;

	if (timeover)
		thiscam->angle = angle;
	else if (!camstill && !resetcalled && !paused && timeover != 1)
		thiscam->angle = R_PointToAngle2(thiscam->x, thiscam->y, viewpointx, viewpointy);

	if (timeover == 1)
	{
		thiscam->momx = P_ReturnThrustX(NULL, mo->angle, 32*mo->scale); // Push forward
		thiscam->momy = P_ReturnThrustY(NULL, mo->angle, 32*mo->scale);
		thiscam->momz = 0;
	}
	else if (player->exiting || timeover == 2)
	{
		thiscam->momx = thiscam->momy = thiscam->momz = 0;
	}
	else if (leveltime < introtime)
	{
		thiscam->momx = FixedMul(x - thiscam->x, camspeed);
		thiscam->momy = FixedMul(y - thiscam->y, camspeed);
		thiscam->momz = FixedMul(z - thiscam->z, camspeed);
	}
	else
	{
		thiscam->momx = x - thiscam->x;
		thiscam->momy = y - thiscam->y;
		thiscam->momz = FixedMul(z - thiscam->z, camspeed/2);
	}

	thiscam->pan = pan;
	thiscam->pitch = pitch;

	// compute aming to look the viewed point
	f1 = viewpointx-thiscam->x;
	f2 = viewpointy-thiscam->y;
	dist = FixedHypot(f1, f2);

	if (mo->eflags & MFE_VERTICALFLIP)
	{
		angle = R_PointToAngle2(0, thiscam->z + thiscam->height, dist, mo->z + mo->height - player->mo->height);
		if (thiscam->pitch < ANGLE_180 && thiscam->pitch > angle)
			angle += (thiscam->pitch - angle)/2;
	}
	else
	{
		angle = R_PointToAngle2(0, thiscam->z, dist, mo->z + player->mo->height);
		if (thiscam->pitch >= ANGLE_180 && thiscam->pitch < angle)
			angle -= (angle - thiscam->pitch)/2;
	}

	if (player->playerstate != PST_DEAD)
		angle += (focusaiming < ANGLE_180 ? focusaiming/2 : InvAngle(InvAngle(focusaiming)/2)); // overcomplicated version of '((signed)focusaiming)/2;'

	if (!camstill && !timeover) // Keep the view still...
	{
		G_ClipAimingPitch((INT32 *)&angle);

		if (camspeed == FRACUNIT)
			thiscam->aiming = angle;
		else
		{
			angle_t input;
			boolean invert;

			input = thiscam->aiming - angle;
			invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			input = FixedAngle(FixedMul(AngleFixed(input), (5*camspeed)/16));
			if (invert)
				input = InvAngle(input);

			thiscam->aiming -= input;
		}
	}

	if (!resetcalled && (player->playerstate == PST_DEAD || player->playerstate == PST_REBORN))
	{
		// Don't let the camera match your movement.
		thiscam->momz = 0;
		if (player->spectator)
			thiscam->aiming = 0;
		// Only let the camera go a little bit downwards.
		else if (!(mo->eflags & MFE_VERTICALFLIP) && thiscam->aiming < ANGLE_337h && thiscam->aiming > ANGLE_180)
			thiscam->aiming = ANGLE_337h;
		else if (mo->eflags & MFE_VERTICALFLIP && thiscam->aiming > ANGLE_22h && thiscam->aiming < ANGLE_180)
			thiscam->aiming = ANGLE_22h;
	}

	if (lookbackdown)
	{
		P_MoveChaseCamera(player, thiscam, false);
		R_ResetViewInterpolation(num + 1);
		R_ResetViewInterpolation(num + 1);
	}

	return (x == thiscam->x && y == thiscam->y && z == thiscam->z && angle == thiscam->aiming);

}

boolean P_SpectatorJoinGame(player_t *player)
{
	INT32 changeto = 0;
	const char *text = NULL;

	// Team changing isn't allowed.
	if (!cv_allowteamchange.value)
		return false;

	// Team changing in Team Match and CTF
	// Pressing fire assigns you to a team that needs players if allowed.
	// Partial code reproduction from p_tick.c autobalance code.
	// a surprise tool that will help us later...
	if (G_GametypeHasTeams())
	{
		INT32 z, numplayersred = 0, numplayersblue = 0;

		//find a team by num players, score, or random if all else fails.
		for (z = 0; z < MAXPLAYERS; ++z)
			if (playeringame[z])
			{
				if (players[z].ctfteam == 1)
					++numplayersred;
				else if (players[z].ctfteam == 2)
					++numplayersblue;
			}
		// for z

		if (numplayersblue > numplayersred)
			changeto = 1;
		else if (numplayersred > numplayersblue)
			changeto = 2;
		else if (bluescore > redscore)
			changeto = 1;
		else if (redscore > bluescore)
			changeto = 2;
		else
			changeto = (P_RandomFixed() & 1) + 1;

		if (!LUA_HookTeamSwitch(player, changeto, true, false, false))
			return false;
	}

	// no conditions that could cause the gamejoin to fail below this line

	if (player->mo)
	{
		P_RemoveMobj(player->mo);
		player->mo = NULL;
	}
	player->spectator = false;
	player->pflags &= ~PF_WANTSTOJOIN;
	player->spectatewait = 0;
	player->ctfteam = changeto;
	player->playerstate = PST_REBORN;

	// Reset away view (some code referenced from Got_Teamchange)
	{
		UINT8 i = 0;
		INT32 *localplayertable = (splitscreen_partied[consoleplayer] ? splitscreen_party[consoleplayer] : g_localplayers);

		for (i = 0; i < r_splitscreen; i++)
		{
			if (localplayertable[i] == (player-players))
			{
				LUA_HookViewpointSwitch(player, player, true);
				displayplayers[i] = (player-players);
				break;
			}
		}
	}

	// a surprise tool that will help us later...
	if (changeto == 1)
		text = va("\x82*%s switched to the %c%s%c team.\n", player_names[player-players], '\x85', "RED", '\x82');
	else if (changeto == 2)
		text = va("\x82*%s switched to the %c%s%c team.\n", player_names[player-players], '\x85', "BLU", '\x82');
	else
		text = va("\x82*%s entered the game.", player_names[player-players]);

	HU_AddChatText(text, false);
	return true; // no more player->mo, cannot continue.
}

// the below is first person only, if you're curious. check out P_CalcChasePostImg in p_mobj.c for chasecam
static void P_CalcPostImg(player_t *player)
{
	sector_t *sector = player->mo->subsector->sector;
	postimg_t *type = NULL;
	INT32 *param;
	fixed_t pviewheight;
	size_t i;

	if (player->mo->eflags & MFE_VERTICALFLIP)
		pviewheight = player->mo->z + player->mo->height - player->viewheight;
	else
		pviewheight = player->mo->z + player->viewheight;

	if (player->awayviewtics && player->awayviewmobj && !P_MobjWasRemoved(player->awayviewmobj))
	{
		sector = player->awayviewmobj->subsector->sector;
		pviewheight = player->awayviewmobj->z + 20*FRACUNIT;
	}

	for (i = 0; i <= (unsigned)r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			type = &postimgtype[i];
			param = &postimgparam[i];
			break;
		}
	}

	// see if we are in heat (no, not THAT kind of heat...)
	for (i = 0; i < sector->tags.count; i++)
	{
		if (Tag_FindLineSpecial(13, sector->tags.tags[i]) != -1)
		{
			*type = postimg_heat;
			break;
		}
		else if (sector->ffloors)
		{
			ffloor_t *rover;
			fixed_t topheight;
			fixed_t bottomheight;
			boolean gotres = false;

			for (rover = sector->ffloors; rover; rover = rover->next)
			{
				size_t j;

				if (!(rover->flags & FF_EXISTS))
					continue;

				topheight    = P_GetFFloorTopZAt   (rover, player->mo->x, player->mo->y);
				bottomheight = P_GetFFloorBottomZAt(rover, player->mo->x, player->mo->y);

				if (pviewheight >= topheight || pviewheight <= bottomheight)
					continue;

				for (j = 0; j < rover->master->frontsector->tags.count; j++)
				{
					if (Tag_FindLineSpecial(13, rover->master->frontsector->tags.tags[j]) != -1)
					{
						*type = postimg_heat;
						gotres = true;
						break;
					}
				}
			}
			if (gotres)
				break;
		}
	}

	// see if we are in water (water trumps heat)
	if (sector->ffloors)
	{
		ffloor_t *rover;
		fixed_t topheight;
		fixed_t bottomheight;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_SWIMMABLE) || rover->flags & FF_BLOCKPLAYER)
				continue;

			topheight    = P_GetFFloorTopZAt   (rover, player->mo->x, player->mo->y);
			bottomheight = P_GetFFloorBottomZAt(rover, player->mo->x, player->mo->y);

			if (pviewheight >= topheight || pviewheight <= bottomheight)
				continue;

			*type = postimg_water;
		}
	}

	if (!encoremode) // srb2kart
	{
		if (player->mo->eflags & MFE_VERTICALFLIP)
			*type = postimg_flip;
	}
	else
	{
		if (player->mo->eflags & MFE_VERTICALFLIP)
			*type = postimg_mirrorflip;
		else
			*type = postimg_mirror;
	}

#if 1
	(void)param;
#else
	// Motion blur
	if (player->speed > (35<<FRACBITS))
	{
		*type = postimg_motion;
		*param = (player->speed - 32)/4;

		if (*param > 5)
			*param = 5;
	}
#endif
}

void P_DoTimeOver(player_t *player)
{
	if (player->pflags & PF_NOCONTEST)
	{
		// NO! Don't do this!
		return;
	}

	if (P_IsLocalPlayer(player) && !demo.playback)
	{
		legitimateexit = true; // SRB2kart: losing a race is still seeing it through to the end :p
	}

	if (netgame && !player->bot && !bossinfo.boss)
	{
		CON_LogMessage(va(M_GetText("%s ran out of time.\n"), player_names[player-players]));
	}

	player->pflags |= PF_NOCONTEST;
	K_UpdatePowerLevelsOnFailure(player);

	if (G_GametypeUsesLives())
	{
		K_PlayerLoseLife(player);
	}

	if (player->mo)
	{
		S_StopSound(player->mo);
		P_DamageMobj(player->mo, NULL, NULL, 1, DMG_TIMEOVER);
	}

	P_EndingMusic(player);

	if (!exitcountdown)
		exitcountdown = 5*TICRATE;
}

// SRB2Kart: These are useful functions, but we aren't using them yet.
#if 0

// Get an axis of a certain ID number
static mobj_t *P_GetAxis(INT32 num)
{
	thinker_t *th;
	mobj_t *mobj;

	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)th;

		// NiGHTS axes spawn before anything else. If this mobj doesn't have MF2_AXIS, it means we reached the axes' end.
		if (!(mobj->flags2 & MF2_AXIS))
			break;

		// Skip if this axis isn't the one we want.
		if (mobj->health != num)
			continue;

		return mobj;
	}

	CONS_Alert(CONS_WARNING, "P_GetAxis: Track segment %d is missing!\n", num);
	return NULL;
}

// Auxiliary function. For a given position and axis, it calculates the nearest "valid" snap-on position.
static void P_GetAxisPosition(fixed_t x, fixed_t y, mobj_t *amo, fixed_t *newx, fixed_t *newy, angle_t *targetangle, angle_t *grind)
{
	fixed_t ax = amo->x;
	fixed_t ay = amo->y;
	angle_t ang;
	angle_t gr = 0;

	if (amo->type == MT_AXISTRANSFERLINE)
	{
		ang = amo->angle;
		// Extra security for cardinal directions.
		if (ang == ANGLE_90 || ang == ANGLE_270) // Vertical lines
			x = ax;
		else if (ang == 0 || ang == ANGLE_180) // Horizontal lines
			y = ay;
		else // Diagonal lines
		{
			fixed_t distance = R_PointToDist2(ax, ay, x, y);
			angle_t fad = ((R_PointToAngle2(ax, ay, x, y) - ang) >> ANGLETOFINESHIFT) & FINEMASK;
			fixed_t cosine = FINECOSINE(fad);
			angle_t fa = (ang >> ANGLETOFINESHIFT) & FINEMASK;
			distance = FixedMul(distance, cosine);
			x = ax + FixedMul(distance, FINECOSINE(fa));
			y = ay + FixedMul(distance, FINESINE(fa));
		}
	}
	else // Keep minecart to circle
	{
		fixed_t rad = amo->radius;
		fixed_t distfactor = FixedDiv(rad, R_PointToDist2(ax, ay, x, y));

		gr = R_PointToAngle2(ax, ay, x, y);
		ang = gr + ANGLE_90;
		x = ax + FixedMul(x - ax, distfactor);
		y = ay + FixedMul(y - ay, distfactor);
	}

	*newx = x;
	*newy = y;
	*targetangle = ang;
	*grind = gr;
}

static void P_ParabolicMove(mobj_t *mo, fixed_t x, fixed_t y, fixed_t z, fixed_t g, fixed_t speed)
{
	fixed_t dx = x - mo->x;
	fixed_t dy = y - mo->y;
	fixed_t dz = z - mo->z;
	fixed_t dh = P_AproxDistance(dx, dy);
	fixed_t c = FixedDiv(dx, dh);
	fixed_t s = FixedDiv(dy, dh);
	fixed_t fixConst = FixedDiv(speed, g);

	mo->momx = FixedMul(c, speed);
	mo->momy = FixedMul(s, speed);
	mo->momz = FixedDiv(dh, 2*fixConst) + FixedDiv(dz, FixedDiv(dh, fixConst/2));
}

#endif

	/* gaysed script from me, based on Golden's sprite slope roll */

// holy SHIT
static INT32
Quaketilt (player_t *player)
{
	angle_t tilt;
	fixed_t lowb; // this threshold for speed
	angle_t moma = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
	INT32 delta = (INT32)( player->mo->angle - moma );
	fixed_t speed;

	boolean sliptiding = K_Sliptiding(player);

	if (delta == (INT32)ANGLE_180)/* FUCK YOU HAVE A HACK */
	{
		return 0;
	}

	// Hi! I'm "not a math guy"!
	if (abs(delta) > ANGLE_90)
		delta = (INT32)(( moma + ANGLE_180 ) - player->mo->angle );
	if (P_IsObjectOnGround(player->mo))
	{
		if (sliptiding)
		{
			tilt = ANGLE_45;
			lowb = 5*FRACUNIT;
		}
		else
		{
			tilt = ANGLE_11hh/2;
			lowb = 15*FRACUNIT;
		}
	}
	else
	{
		tilt = ANGLE_22h;
		lowb = 10*FRACUNIT;
	}
	moma = FixedMul(FixedDiv(delta, ANGLE_90), tilt);
	speed = abs( player->mo->momx + player->mo->momy );
	if (speed < lowb)
	{
		// ease out tilt as we slow...
		moma = FixedMul(moma, FixedDiv(speed, lowb));
	}
	return moma;
}

static void
DoABarrelRoll (player_t *player)
{
	angle_t slope;
	angle_t delta;

	fixed_t smoothing;

	if (player->respawn.state != RESPAWNST_NONE)
	{
		player->tilt = 0;
		return;
	}

	if (player->exiting)
	{
		return;
	}

	slope = InvAngle(R_GetPitchRollAngle(player->mo, player));

	if (AbsAngle(slope) < ANGLE_11hh)
	{
		slope = 0;
	}

	if (AbsAngle(slope) > ANGLE_45)
	{
		slope = slope & ANGLE_180 ? InvAngle(ANGLE_45) : ANGLE_45;
	}

	slope -= Quaketilt(player);

	delta = slope - player->tilt;
	smoothing = FixedDiv(AbsAngle(slope), ANGLE_45);

	delta = FixedDiv(delta, 33 *
			FixedDiv(FRACUNIT, FRACUNIT + smoothing));

	if (delta)
		player->tilt += delta;
	else
		player->tilt  = slope;
}

//
// P_PlayerThink
//

void P_PlayerThink(player_t *player)
{
	ticcmd_t *cmd;
	const size_t playeri = (size_t)(player - players);

#ifdef PARANOIA
	if (!player->mo)
		I_Error("p_playerthink: players[%s].mo == NULL", sizeu1(playeri));
#endif

	// todo: Figure out what is actually causing these problems in the first place...
	if (player->mo->health <= 0 && player->playerstate == PST_LIVE) //you should be DEAD!
	{
		CONS_Debug(DBG_GAMELOGIC, "P_PlayerThink: Player %s in PST_LIVE with 0 health. (\"Zombie bug\")\n", sizeu1(playeri));
		player->playerstate = PST_DEAD;
	}

	player->old_drawangle = player->drawangle;

	player->pflags &= ~PF_HITFINISHLINE;

	if (player->awayviewmobj && P_MobjWasRemoved(player->awayviewmobj))
	{
		P_SetTarget(&player->awayviewmobj, NULL); // remove awayviewmobj asap if invalid
		player->awayviewtics = 0; // reset to zero
	}

	if (player->flashcount)
		player->flashcount--;

	if (player->awayviewtics && player->awayviewtics != -1)
		player->awayviewtics--;

	// Track airtime
	if (P_IsObjectOnGround(player->mo)
		&& !P_PlayerInPain(player)) // This isn't airtime, but it's control loss all the same.
	{
		player->airtime = 0;
	}
	else
	{
		player->airtime++;
	}

	cmd = &player->cmd;

	// SRB2kart
	// Save the dir the player is holding
	//  to allow items to be thrown forward or backward.
	{
		const INT16 threshold = 0; //(KART_FULLTURN / 2);
		if (cmd->throwdir > threshold)
		{
			player->throwdir = 1;
		}
		else if (cmd->throwdir < -threshold)
		{
			player->throwdir = -1;
		}
		else
		{
			player->throwdir = 0;
		}
	}

	// Accessibility - kickstart your acceleration
	if (!(player->pflags & PF_KICKSTARTACCEL))
	{
		player->kickstartaccel = 0;
	}
	else if (cmd->buttons & BT_ACCELERATE)
	{
		if (!player->exiting && !(player->oldcmd.buttons & BT_ACCELERATE))
		{
			player->kickstartaccel = 0;
		}
		else if (player->kickstartaccel < ACCEL_KICKSTART)
		{
			player->kickstartaccel++;
			if ((player->kickstartaccel == ACCEL_KICKSTART) && P_IsLocalPlayer(player))
			{
				S_StartSound(NULL, sfx_ding);
			}
		}
		else // for HUD
		{
			player->kickstartaccel = ACCEL_KICKSTART+1;
		}
	}
	else if (player->kickstartaccel < ACCEL_KICKSTART)
	{
		player->kickstartaccel = 0;
	}
	else // for HUD
	{
		player->kickstartaccel = ACCEL_KICKSTART+1;
	}

#ifdef PARANOIA
	if (player->playerstate == PST_REBORN)
		I_Error("player %s is in PST_REBORN\n", sizeu1(playeri));
#endif

	if (!mapreset)
	{
		if (gametyperules & GTR_CIRCUIT)
		{
			INT32 i;

			// Check if all the players in the race have finished. If so, end the level.
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (playeringame[i] && !players[i].spectator)
				{
					if (!players[i].exiting && !(players[i].pflags & PF_NOCONTEST) && players[i].lives > 0)
						break;
				}
			}

			if (i == MAXPLAYERS && player->exiting == raceexittime+2) // finished
				player->exiting = raceexittime+1;

#if 0
			// If 10 seconds are left on the timer,
			// begin the drown music for countdown!
			// SRB2Kart: despite how perfect this is, it's disabled FOR A REASON
			if (racecountdown == 11*TICRATE - 1)
			{
				if (P_IsLocalPlayer(player))
					S_ChangeMusicInternal("drown", false);
			}
#endif

			// If you've hit the countdown and you haven't made
			//  it to the exit, you're a goner!
			if (racecountdown == 1 && !player->spectator && !player->exiting && !(player->pflags & PF_NOCONTEST) && player->lives > 0)
			{
				P_DoTimeOver(player);

				if (player->playerstate == PST_DEAD)
				{
					LUA_HookPlayer(player, HOOK(PlayerThink));
					return;
				}
			}
		}

		// If it is set, start subtracting
		// Don't allow it to go back to 0
		if (player->exiting > 1 && (player->exiting < raceexittime+2 || !(gametyperules & GTR_CIRCUIT))) // SRB2kart - "&& player->exiting > 1"
			player->exiting--;

		if (player->exiting && exitcountdown)
			player->exiting = 99; // SRB2kart

		if (player->exiting == 2 || exitcountdown == 2)
		{
			if (server)
			{
				SendNetXCmd(XD_EXITLEVEL, NULL, 0);
			}
		}
	}

	// check water content, set stuff in mobj
	P_MobjCheckWater(player->mo);

#ifndef SECTORSPECIALSAFTERTHINK
	if (player->onconveyor != 1 || !P_IsObjectOnGround(player->mo))
		player->onconveyor = 0;
	// check special sectors : damage & secrets

	if (!player->spectator)
		P_PlayerInSpecialSector(player);
	else if (
#else
	if (player->spectator &&
#endif
		(gametyperules & GTR_LIVES))
	{
		/*P_ConsiderAllGone()*/;
	}

	if (player->playerstate == PST_DEAD)
	{
		if (player->spectator)
			player->mo->renderflags |= RF_GHOSTLY;
		else
			player->mo->renderflags &= ~RF_GHOSTLYMASK;
		P_DeathThink(player);
		LUA_HookPlayer(player, HOOK(PlayerThink));
		return;
	}

	// Make sure spectators always have a score and ring count of 0.
	if (player->spectator)
	{
		//player->score = 0;
		player->rings = 0;
		player->mo->health = 1;
	}

	// SRB2kart 010217
	if (leveltime < introtime)
	{
		player->nocontrol = 2;
	}

	// Synchronizes the "real" amount of time spent in the level.
	if (!(exitcountdown && !racecountdown) && !(player->exiting || mapreset) && !(player->pflags & PF_NOCONTEST) && !stoppedclock)
	{
		if (leveltime >= starttime)
		{
			player->realtime = leveltime - starttime;
			if (player == &players[consoleplayer])
			{
				if (player->spectator || !circuitmap)
					curlap = 0;
				else if (curlap != UINT32_MAX)
					curlap++; // This is too complicated to sync to realtime, just sorta hope for the best :V
			}
		}
		else
		{
			player->realtime = 0;
			if (player == &players[consoleplayer])
				curlap = 0;
		}
	}

	if ((netgame || multiplayer) && player->spectator && cmd->buttons & BT_ATTACK && !player->flashing)
	{
		player->pflags ^= PF_WANTSTOJOIN;
		player->flashing = TICRATE/2 + 1;
		/*if (P_SpectatorJoinGame(player))
			return; // player->mo was removed.*/
		//CONS_Printf("player %s wants to join on tic %d\n", player_names[player-players], leveltime);
	}

	if (player->respawn.state != RESPAWNST_NONE)
	{
		K_RespawnChecker(player);
		player->rmomx = player->rmomy = 0;

		if (player->respawn.state == RESPAWNST_DROP)
		{
			// Allows some turning
			P_MovePlayer(player);
		}
	}
	else if (player->mo->reactiontime)
	{
		// Reactiontime is used to prevent movement
		// for a bit after a teleport.
		player->mo->reactiontime--;
	}
	else if (player->mo->tracer && player->mo->tracer->type == MT_TUBEWAYPOINT)
	{
		P_DoZoomTube(player);
		player->rmomx = player->rmomy = 0;
	}
	else
	{
		// Move around.
		P_MovePlayer(player);
	}

	// Unset statis flag after moving.
	// In other words, if you manually set stasis via code,
	// it lasts for one tic.
	player->pflags &= ~PF_STASIS;

	if (player->onconveyor == 1)
		player->onconveyor = 3;
	else if (player->onconveyor == 3)
		player->cmomy = player->cmomx = 0;

	P_DoBubbleBreath(player); // Spawn Sonic's bubbles
	P_CheckInvincibilityTimer(player); // Spawn Invincibility Sparkles
	
	// "Blur" a bit when you have speed shoes and are going fast enough
	if ((player->driftboost || player->sneakertimer || player->startboost || player->ringboost) && !player->invincibilitytimer // SRB2kart
		&& (player->speed + abs(player->mo->momz)) > FixedMul(20*FRACUNIT,player->mo->scale))
	{
		UINT8 i;
		mobj_t *gmobj = P_SpawnGhostMobj(player->mo);

		gmobj->fuse = 2;
		if (leveltime & 1)
		{
			gmobj->frame &= ~FF_TRANSMASK;
			gmobj->frame |= tr_trans70<<FF_TRANSSHIFT;
		}

		// Hide the mobj from our sights if we're the displayplayer and chasecam is off.
		// Why not just not spawn the mobj?  Well, I'd rather only flirt with
		// consistency so much...
		for (i = 0; i <= splitscreen; i++)
		{
			if (player == &players[displayplayers[i]] && !camera[i].chase)
			{
				gmobj->renderflags |= RF_DONTDRAW;
				break;
			}
		}
	}

	// Counters, time dependent power ups.
	// Time Bonus & Ring Bonus count settings

	// Strength counts up to diminish fade.
	if (player->flashing && player->flashing < UINT16_MAX &&
		(player->spectator || player->flashing < K_GetKartFlashing(player)))
		player->flashing--;

	if (player->nocontrol && player->nocontrol < UINT16_MAX)
	{
		if (!(--player->nocontrol))
			player->pflags &= ~PF_SKIDDOWN;
	}
	else
		player->nocontrol = 0;

	// Flash player after being hit.
	if (!(player->hyudorotimer // SRB2kart - fixes Hyudoro not flashing when it should.
		|| player->growshrinktimer > 0 // Grow doesn't flash either.
		|| (player->respawn.state != RESPAWNST_NONE) // Respawn timer (for drop dash effect)
		|| (player->pflags & PF_NOCONTEST) // NO CONTEST explosion
		|| ((gametyperules & GTR_BUMPERS) && player->bumpers <= 0 && player->karmadelay)))
	{
		if (player->flashing > 1 && player->flashing < K_GetKartFlashing(player)
			&& (leveltime & 1))
			player->mo->renderflags |= RF_DONTDRAW;
		else
			player->mo->renderflags &= ~RF_DONTDRAW;
	}

	if (cmd->flags & TICCMD_TYPING)
	{
		/*
		typing_duration is slow to start and slow to stop.

		typing_timer counts down a grace period before the player is not
		actually considered typing anymore.
		*/
		if (cmd->flags & TICCMD_KEYSTROKE)
		{
			/* speed up if we are typing quickly! */
			if (player->typing_duration > 0 && player->typing_timer > 12)
			{
				if (player->typing_duration < 16)
				{
					player->typing_duration = 24;
				}
				else
				{
					/* slows down a tiny bit as it approaches the next dot */
					const UINT8 step = (((player->typing_duration + 15) & ~15) -
							player->typing_duration) / 2;
					player->typing_duration += max(step, 4);
				}
			}

			player->typing_timer = 15;
		}
		else if (player->typing_timer > 0)
		{
			player->typing_timer--;
		}

		/* if we are in the grace period (including currently typing) */
		if (player->typing_timer + player->typing_duration > 0)
		{
			/* always end the cycle on two dots */
			if (player->typing_timer == 0 &&
					(player->typing_duration < 16 || player->typing_duration == 40))
			{
				player->typing_duration = 0;
			}
			else if (player->typing_duration < 63)
			{
				player->typing_duration++;
			}
			else
			{
				player->typing_duration = 16;
			}
		}
	}
	else
	{
		player->typing_timer = 0;
		player->typing_duration = 0;
	}

	K_KartPlayerThink(player, cmd); // SRB2kart

	DoABarrelRoll(player);

	if (player->carry == CR_SLIDING)
		player->carry = CR_NONE;

	LUA_HookPlayer(player, HOOK(PlayerThink));
}

//
// P_PlayerAfterThink
//
// Thinker for player after all other thinkers have run
//
void P_PlayerAfterThink(player_t *player)
{
	camera_t *thiscam = NULL; // if not one of the displayed players, just don't bother
	UINT8 i;

#ifdef PARANOIA
	if (!player->mo)
	{
		const size_t playeri = (size_t)(player - players);
		I_Error("P_PlayerAfterThink: players[%s].mo == NULL", sizeu1(playeri));
	}
#endif

#ifdef SECTORSPECIALSAFTERTHINK
	if (player->onconveyor != 1 || !P_IsObjectOnGround(player->mo))
		player->onconveyor = 0;
	// check special sectors : damage & secrets

	if (!player->spectator)
		P_PlayerInSpecialSector(player);
#endif

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			thiscam = &camera[i];
			break;
		}
	}

	if (player->playerstate == PST_DEAD)
	{
		// Followers need handled while dead.
		K_HandleFollower(player);

		if (player->followmobj)
		{
			P_RemoveMobj(player->followmobj);
			P_SetTarget(&player->followmobj, NULL);
		}

		return;
	}

	if (thiscam)
	{
		if (!thiscam->chase) // bob view only if looking through the player's eyes
		{
			P_CalcHeight(player);
			P_CalcPostImg(player);
		}
		else
		{
			// defaults to make sure 1st person cam doesn't do anything weird on startup
			player->deltaviewheight = 0;
			player->viewheight = P_GetPlayerViewHeight(player);

			if (player->mo->eflags & MFE_VERTICALFLIP)
				player->viewz = player->mo->z + player->mo->height - player->viewheight;
			else
				player->viewz = player->mo->z + player->viewheight;
		}
	}

	// spectator invisibility and nogravity.
	if ((netgame || multiplayer) && player->spectator)
	{
		player->mo->renderflags |= RF_DONTDRAW;
		player->mo->flags |= MF_NOGRAVITY;
	}

	if (P_IsObjectOnGround(player->mo))
		player->mo->pmomz = 0;

	K_KartPlayerAfterThink(player);

	if (player->followmobj && (player->spectator || player->mo->health <= 0 || player->followmobj->type != player->followitem))
	{
		P_RemoveMobj(player->followmobj);
		P_SetTarget(&player->followmobj, NULL);
	}

	if (!player->spectator && player->mo->health && player->followitem)
	{
		if (!player->followmobj || P_MobjWasRemoved(player->followmobj))
		{
			P_SetTarget(&player->followmobj, P_SpawnMobjFromMobj(player->mo, 0, 0, 0, player->followitem));
			P_SetTarget(&player->followmobj->tracer, player->mo);
			switch (player->followmobj->type)
			{
				default:
					player->followmobj->flags2 |= MF2_LINKDRAW;
					break;
			}
		}

		if (player->followmobj)
		{
			if (LUA_HookFollowMobj(player, player->followmobj) || P_MobjWasRemoved(player->followmobj))
				{;}
			else
			{
				switch (player->followmobj->type)
				{
					default:
						var1 = 1;
						var2 = 0;
						A_CapeChase(player->followmobj);
						break;
				}
			}
		}
	}

	// Run followers in AfterThink, after the players have moved,
	// so a lag value of 1 is exactly attached to the player.
	K_HandleFollower(player);
}

void P_SetPlayerAngle(player_t *player, angle_t angle)
{
	P_ForceLocalAngle(player, angle);
	player->angleturn = angle;
}

angle_t P_GetLocalAngle(player_t *player)
{
	// this function is from vanilla srb2. can you tell?
	// (hint: they have separate variables for all of this shit instead of arrays)
	UINT8 i;

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
			return localangle[i];
	}

	return 0;
}

void P_ForceLocalAngle(player_t *player, angle_t angle)
{
	UINT8 i;

	angle = angle & ~UINT16_MAX;

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			localangle[i] = angle;
			break;
		}
	}
}

boolean P_PlayerFullbright(player_t *player)
{
	return (player->invincibilitytimer > 0);
}
