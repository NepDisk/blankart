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
/// \file  p_saveg.c
/// \brief Archiving: SaveGame I/O

#include "doomdef.h"
#include "byteptr.h"
#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_random.h"
#include "m_misc.h"
#include "p_local.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "r_data.h"
#include "r_fps.h"
#include "r_textures.h"
#include "r_things.h"
#include "r_skins.h"
#include "r_state.h"
#include "w_wad.h"
#include "y_inter.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_sky.h"
#include "p_polyobj.h"
#include "lua_script.h"
#include "p_slopes.h"

// SRB2Kart
#include "k_battle.h"
#include "k_pwrlv.h"
#include "k_terrain.h"

savedata_t savedata;
UINT8 *save_p;

// Block UINT32s to attempt to ensure that the correct data is
// being sent and received
#define ARCHIVEBLOCK_MISC     0x7FEEDEED
#define ARCHIVEBLOCK_PLAYERS  0x7F448008
#define ARCHIVEBLOCK_WORLD    0x7F8C08C0
#define ARCHIVEBLOCK_POBJS    0x7F928546
#define ARCHIVEBLOCK_THINKERS 0x7F37037C
#define ARCHIVEBLOCK_SPECIALS 0x7F228378
#define ARCHIVEBLOCK_WAYPOINTS 0x7F46498F

// Note: This cannot be bigger
// than an UINT16
typedef enum
{
	AWAYVIEW   = 0x01,
	FOLLOWITEM = 0x02,
	FOLLOWER   = 0x04,
	SKYBOXVIEW = 0x08,
	SKYBOXCENTER = 0x10,
	//free = 0x20,
} player_saveflags;

static inline void P_ArchivePlayer(void)
{
	const player_t *player = &players[consoleplayer];
	INT16 skininfo = player->skin;
	SINT8 pllives = player->lives;
	if (pllives < startinglivesbalance[numgameovers]) // Bump up to 3 lives if the player
		pllives = startinglivesbalance[numgameovers]; // has less than that.

	WRITEUINT16(save_p, skininfo);
	WRITEUINT8(save_p, numgameovers);
	WRITESINT8(save_p, pllives);
	WRITEUINT32(save_p, player->score);
}

static inline void P_UnArchivePlayer(void)
{
	INT16 skininfo = READUINT16(save_p);
	savedata.skin = skininfo;

	savedata.numgameovers = READUINT8(save_p);
	savedata.lives = READSINT8(save_p);
	savedata.score = READUINT32(save_p);
}

static void P_NetArchivePlayers(void)
{
	INT32 i, j;
	UINT16 flags;
//	size_t q;

	WRITEUINT32(save_p, ARCHIVEBLOCK_PLAYERS);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		WRITESINT8(save_p, (SINT8)adminplayers[i]);

		for (j = 0; j < PWRLV_NUMTYPES; j++)
		{
			WRITEINT16(save_p, clientpowerlevels[i][j]);
		}
		WRITEINT16(save_p, clientPowerAdd[i]);

		if (!playeringame[i])
			continue;

		flags = 0;

		// no longer send ticcmds

		WRITESTRINGN(save_p, player_names[i], MAXPLAYERNAME);

		WRITEUINT8(save_p, playerconsole[i]);
		WRITEINT32(save_p, splitscreen_invitations[i]);
		WRITEINT32(save_p, splitscreen_party_size[i]);
		WRITEINT32(save_p, splitscreen_original_party_size[i]);

		for (j = 0; j < MAXSPLITSCREENPLAYERS; ++j)
		{
			WRITEINT32(save_p, splitscreen_party[i][j]);
			WRITEINT32(save_p, splitscreen_original_party[i][j]);
		}

		WRITEANGLE(save_p, players[i].angleturn);
		WRITEANGLE(save_p, players[i].aiming);
		WRITEANGLE(save_p, players[i].drawangle);
		WRITEANGLE(save_p, players[i].viewrollangle);
		WRITEANGLE(save_p, players[i].tilt);
		WRITEANGLE(save_p, players[i].awayviewaiming);
		WRITEINT32(save_p, players[i].awayviewtics);

		WRITEUINT8(save_p, players[i].playerstate);
		WRITEUINT32(save_p, players[i].pflags);
		WRITEUINT8(save_p, players[i].panim);
		WRITEUINT8(save_p, players[i].spectator);
		WRITEUINT32(save_p, players[i].spectatewait);

		WRITEUINT16(save_p, players[i].flashpal);
		WRITEUINT16(save_p, players[i].flashcount);

		WRITEUINT8(save_p, players[i].skincolor);
		WRITEINT32(save_p, players[i].skin);
		WRITEUINT32(save_p, players[i].availabilities);
		WRITEUINT32(save_p, players[i].score);
		WRITESINT8(save_p, players[i].lives);
		WRITESINT8(save_p, players[i].xtralife);
		WRITEFIXED(save_p, players[i].speed);
		WRITEFIXED(save_p, players[i].lastspeed);
		WRITEINT32(save_p, players[i].deadtimer);
		WRITEUINT32(save_p, players[i].exiting);

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		WRITEFIXED(save_p, players[i].cmomx); // Conveyor momx
		WRITEFIXED(save_p, players[i].cmomy); // Conveyor momy
		WRITEFIXED(save_p, players[i].rmomx); // "Real" momx (momx - cmomx)
		WRITEFIXED(save_p, players[i].rmomy); // "Real" momy (momy - cmomy)

		WRITEINT16(save_p, players[i].totalring);
		WRITEUINT32(save_p, players[i].realtime);
		WRITEUINT8(save_p, players[i].laps);
		WRITEUINT8(save_p, players[i].latestlap);
		
		WRITEUINT32(save_p, players[i].starposttime);
		WRITEINT16(save_p, players[i].starpostx);
		WRITEINT16(save_p, players[i].starposty);
		WRITEINT16(save_p, players[i].starpostz);
		WRITEINT32(save_p, players[i].starpostnum);
		WRITEANGLE(save_p, players[i].starpostangle);
		WRITEUINT8(save_p, (UINT8)players[i].starpostflip);
		WRITEINT32(save_p, players[i].prevcheck);
		WRITEINT32(save_p, players[i].nextcheck);

		WRITEUINT8(save_p, players[i].ctfteam);

		WRITEUINT8(save_p, players[i].checkskip);

		WRITEINT16(save_p, players[i].lastsidehit);
		WRITEINT16(save_p, players[i].lastlinehit);

		WRITEINT32(save_p, players[i].onconveyor);

		WRITEUINT32(save_p, players[i].jointime);
		WRITEUINT32(save_p, players[i].spectatorreentry);

		WRITEUINT32(save_p, players[i].grieftime);
		WRITEUINT8(save_p, players[i].griefstrikes);

		WRITEUINT8(save_p, players[i].splitscreenindex);

		if (players[i].awayviewmobj)
			flags |= AWAYVIEW;

		if (players[i].followmobj)
			flags |= FOLLOWITEM;

		if (players[i].follower)
			flags |= FOLLOWER;

		if (players[i].skybox.viewpoint)
			flags |= SKYBOXVIEW;

		if (players[i].skybox.centerpoint)
			flags |= SKYBOXCENTER;

		WRITEUINT16(save_p, flags);

		if (flags & SKYBOXVIEW)
			WRITEUINT32(save_p, players[i].skybox.viewpoint->mobjnum);

		if (flags & SKYBOXCENTER)
			WRITEUINT32(save_p, players[i].skybox.centerpoint->mobjnum);

		if (flags & AWAYVIEW)
			WRITEUINT32(save_p, players[i].awayviewmobj->mobjnum);

		if (flags & FOLLOWITEM)
			WRITEUINT32(save_p, players[i].followmobj->mobjnum);

		WRITEUINT32(save_p, (UINT32)players[i].followitem);

		WRITEUINT32(save_p, players[i].charflags);

		// SRB2kart
		WRITEUINT8(save_p, players[i].kartspeed);
		WRITEUINT8(save_p, players[i].kartweight);
		
		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			WRITEINT16(save_p, players[i].lturn_max[j]);
			WRITEINT16(save_p, players[i].rturn_max[j]);
		}

		WRITEUINT8(save_p, players[i].followerskin);
		WRITEUINT8(save_p, players[i].followerready);	// booleans are really just numbers eh??
		WRITEUINT16(save_p, players[i].followercolor);
		if (flags & FOLLOWER)
			WRITEUINT32(save_p, players[i].follower->mobjnum);

		WRITEUINT16(save_p, players[i].nocontrol);
		WRITEUINT8(save_p, players[i].carry);
		WRITEUINT16(save_p, players[i].dye);

		WRITEUINT8(save_p, players[i].position);
		WRITEUINT8(save_p, players[i].oldposition);
		WRITEUINT8(save_p, players[i].positiondelay);
		WRITEUINT32(save_p, players[i].distancetofinish);
		WRITEUINT32(save_p, K_GetWaypointHeapIndex(players[i].nextwaypoint));
		WRITEUINT32(save_p, players[i].airtime);
		WRITEUINT8(save_p, players[i].startboost);

		WRITEUINT16(save_p, players[i].flashing);
		WRITEUINT16(save_p, players[i].spinouttimer);
		WRITEUINT8(save_p, players[i].spinouttype);
		WRITEINT16(save_p, players[i].squishedtimer);
		WRITEUINT8(save_p, players[i].instashield);
		WRITEUINT8(save_p, players[i].wipeoutslow);
		WRITEUINT8(save_p, players[i].justbumped);

		WRITESINT8(save_p, players[i].drift);
		WRITEFIXED(save_p, players[i].driftcharge);
		WRITEUINT8(save_p, players[i].driftboost);

		WRITESINT8(save_p, players[i].aizdriftstrat);
		WRITEINT32(save_p, players[i].aizdrifttilt);
		WRITEINT32(save_p, players[i].aizdriftturn);

		WRITEINT32(save_p, players[i].underwatertilt);

		WRITEFIXED(save_p, players[i].offroad);
		WRITEUINT8(save_p, players[i].pogospring);
		WRITEUINT8(save_p, players[i].brakestop);
		WRITEUINT8(save_p, players[i].waterskip);

		WRITEUINT16(save_p, players[i].springstars);
		WRITEUINT16(save_p, players[i].springcolor);
		WRITEUINT8(save_p, players[i].dashpadcooldown);

		WRITEFIXED(save_p, players[i].boostpower);
		WRITEFIXED(save_p, players[i].speedboost);
		WRITEFIXED(save_p, players[i].accelboost);
		WRITEFIXED(save_p, players[i].handleboost);
		WRITEANGLE(save_p, players[i].boostangle);

		WRITEUINT8(save_p, players[i].tripwireState);
		WRITEUINT8(save_p, players[i].tripwirePass);
		WRITEUINT16(save_p, players[i].tripwireLeniency);

		WRITEUINT16(save_p, players[i].itemroulette);
		WRITEUINT8(save_p, players[i].roulettetype);

		WRITESINT8(save_p, players[i].itemtype);
		WRITEUINT8(save_p, players[i].itemamount);
		WRITESINT8(save_p, players[i].throwdir);

		WRITEUINT8(save_p, players[i].sadtimer);

		WRITESINT8(save_p, players[i].rings);
		WRITEUINT8(save_p, players[i].pickuprings);
		WRITEUINT8(save_p, players[i].ringdelay);
		WRITEUINT16(save_p, players[i].ringboost);
		WRITEUINT8(save_p, players[i].sparkleanim);
		WRITEUINT16(save_p, players[i].superring);

		WRITEUINT8(save_p, players[i].curshield);
		WRITEUINT8(save_p, players[i].bubblecool);
		WRITEUINT8(save_p, players[i].bubbleblowup);
		WRITEUINT16(save_p, players[i].flamedash);
		WRITEUINT16(save_p, players[i].flamemeter);
		WRITEUINT8(save_p, players[i].flamelength);

		WRITEUINT16(save_p, players[i].hyudorotimer);
		WRITESINT8(save_p, players[i].stealingtimer);

		WRITEUINT16(save_p, players[i].sneakertimer);
		WRITEUINT8(save_p, players[i].floorboost);
		WRITEUINT8(save_p, players[i].waterrun);
		
		WRITEUINT8(save_p, players[i].boostcharge);

		WRITEINT16(save_p, players[i].growshrinktimer);
		WRITEINT16(save_p, players[i].growcancel);
		WRITEUINT16(save_p, players[i].rocketsneakertimer);
		WRITEUINT16(save_p, players[i].invincibilitytimer);

		WRITEUINT8(save_p, players[i].eggmanexplode);
		WRITESINT8(save_p, players[i].eggmanblame);

		WRITEUINT8(save_p, players[i].bananadrag);

		WRITESINT8(save_p, players[i].lastjawztarget);
		WRITEUINT8(save_p, players[i].jawztargetdelay);

		WRITEUINT8(save_p, players[i].confirmVictim);
		WRITEUINT8(save_p, players[i].confirmVictimDelay);

		WRITEUINT32(save_p, players[i].roundscore);
		WRITEUINT8(save_p, players[i].emeralds);
		WRITEUINT8(save_p, players[i].bumpers);
		WRITEINT16(save_p, players[i].karmadelay);
		WRITEUINT32(save_p, players[i].overtimekarma);
		WRITEINT16(save_p, players[i].spheres);
		WRITEUINT32(save_p, players[i].spheredigestion);

		WRITESINT8(save_p, players[i].glanceDir);

		WRITEUINT8(save_p, players[i].typing_timer);
		WRITEUINT8(save_p, players[i].typing_duration);

		WRITEUINT8(save_p, players[i].kickstartaccel);

		// botvars_t
		WRITEUINT8(save_p, players[i].botvars.difficulty);
		WRITEUINT8(save_p, players[i].botvars.diffincrease);
		WRITEUINT8(save_p, players[i].botvars.rival);
		WRITEFIXED(save_p, players[i].botvars.rubberband);
		WRITEUINT16(save_p, players[i].botvars.controller);
		WRITEUINT32(save_p, players[i].botvars.itemdelay);
		WRITEUINT32(save_p, players[i].botvars.itemconfirm);
		WRITESINT8(save_p, players[i].botvars.turnconfirm);
	}
}

static void P_NetUnArchivePlayers(void)
{
	INT32 i, j;
	UINT16 flags;

	if (READUINT32(save_p) != ARCHIVEBLOCK_PLAYERS)
		I_Error("Bad $$$.sav at archive block Players");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		adminplayers[i] = (INT32)READSINT8(save_p);

		for (j = 0; j < PWRLV_NUMTYPES; j++)
		{
			clientpowerlevels[i][j] = READINT16(save_p);
		}
		clientPowerAdd[i] = READINT16(save_p);

		// Do NOT memset player struct to 0
		// other areas may initialize data elsewhere
		//memset(&players[i], 0, sizeof (player_t));
		if (!playeringame[i])
			continue;

		// NOTE: sending tics should (hopefully) no longer be necessary

		READSTRINGN(save_p, player_names[i], MAXPLAYERNAME);

		playerconsole[i] = READUINT8(save_p);
		splitscreen_invitations[i] = READINT32(save_p);
		splitscreen_party_size[i] = READINT32(save_p);
		splitscreen_original_party_size[i] = READINT32(save_p);

		for (j = 0; j < MAXSPLITSCREENPLAYERS; ++j)
		{
			splitscreen_party[i][j] = READINT32(save_p);
			splitscreen_original_party[i][j] = READINT32(save_p);
		}

		players[i].angleturn = READANGLE(save_p);
		players[i].aiming = READANGLE(save_p);
		players[i].drawangle = players[i].old_drawangle = READANGLE(save_p);
		players[i].viewrollangle = READANGLE(save_p);
		players[i].tilt = READANGLE(save_p);
		players[i].awayviewaiming = READANGLE(save_p);
		players[i].awayviewtics = READINT32(save_p);

		players[i].playerstate = READUINT8(save_p);
		players[i].pflags = READUINT32(save_p);
		players[i].panim = READUINT8(save_p);
		players[i].spectator = READUINT8(save_p);
		players[i].spectatewait = READUINT32(save_p);

		players[i].flashpal = READUINT16(save_p);
		players[i].flashcount = READUINT16(save_p);

		players[i].skincolor = READUINT8(save_p);
		players[i].skin = READINT32(save_p);
		players[i].availabilities = READUINT32(save_p);
		players[i].score = READUINT32(save_p);
		players[i].lives = READSINT8(save_p);
		players[i].xtralife = READSINT8(save_p); // Ring Extra Life counter
		players[i].speed = READFIXED(save_p); // Player's speed (distance formula of MOMX and MOMY values)
		players[i].lastspeed = READFIXED(save_p);
		players[i].deadtimer = READINT32(save_p); // End game if game over lasts too long
		players[i].exiting = READUINT32(save_p); // Exitlevel timer

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		players[i].cmomx = READFIXED(save_p); // Conveyor momx
		players[i].cmomy = READFIXED(save_p); // Conveyor momy
		players[i].rmomx = READFIXED(save_p); // "Real" momx (momx - cmomx)
		players[i].rmomy = READFIXED(save_p); // "Real" momy (momy - cmomy)

		players[i].totalring = READINT16(save_p); // Total number of rings obtained for GP
		players[i].realtime = READUINT32(save_p); // integer replacement for leveltime
		players[i].laps = READUINT8(save_p); // Number of laps (optional)
		players[i].latestlap = READUINT8(save_p);
		
		players[i].starposttime = READUINT32(save_p);
		players[i].starpostx = READINT16(save_p);
		players[i].starposty = READINT16(save_p);
		players[i].starpostz = READINT16(save_p);
		players[i].starpostnum = READINT32(save_p);
		players[i].starpostangle = READANGLE(save_p);
		players[i].starpostflip = (boolean)READUINT8(save_p);
		players[i].prevcheck = READINT32(save_p);
		players[i].nextcheck = READINT32(save_p);

		players[i].ctfteam = READUINT8(save_p); // 1 == Red, 2 == Blue

		players[i].checkskip = READUINT8(save_p);

		players[i].lastsidehit = READINT16(save_p);
		players[i].lastlinehit = READINT16(save_p);

		players[i].onconveyor = READINT32(save_p);

		players[i].jointime = READUINT32(save_p);
		players[i].spectatorreentry = READUINT32(save_p);

		players[i].grieftime = READUINT32(save_p);
		players[i].griefstrikes = READUINT8(save_p);

		players[i].splitscreenindex = READUINT8(save_p);

		flags = READUINT16(save_p);

		if (flags & SKYBOXVIEW)
			players[i].skybox.viewpoint = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & SKYBOXCENTER)
			players[i].skybox.centerpoint = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & AWAYVIEW)
			players[i].awayviewmobj = (mobj_t *)(size_t)READUINT32(save_p);

		if (flags & FOLLOWITEM)
			players[i].followmobj = (mobj_t *)(size_t)READUINT32(save_p);
		
		players[i].followitem = (mobjtype_t)READUINT32(save_p);

		//SetPlayerSkinByNum(i, players[i].skin);
		players[i].charflags = READUINT32(save_p);

		// SRB2kart
		players[i].kartspeed = READUINT8(save_p);
		players[i].kartweight = READUINT8(save_p);
		
		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			players[i].lturn_max[j] = READINT16(save_p);
			players[i].rturn_max[j] = READINT16(save_p);
		}

		players[i].followerskin = READUINT8(save_p);
		players[i].followerready = READUINT8(save_p);
		players[i].followercolor = READUINT16(save_p);
		if (flags & FOLLOWER)
			players[i].follower = (mobj_t *)(size_t)READUINT32(save_p);

		players[i].nocontrol = READUINT16(save_p);
		players[i].carry = READUINT8(save_p);
		players[i].dye = READUINT16(save_p);

		players[i].position = READUINT8(save_p);
		players[i].oldposition = READUINT8(save_p);
		players[i].positiondelay = READUINT8(save_p);
		players[i].distancetofinish = READUINT32(save_p);
		players[i].nextwaypoint = (waypoint_t *)(size_t)READUINT32(save_p);
		players[i].airtime = READUINT32(save_p);
		players[i].startboost = READUINT8(save_p);

		players[i].flashing = READUINT16(save_p);
		players[i].spinouttimer = READUINT16(save_p);
		players[i].spinouttype = READUINT8(save_p);
		players[i].squishedtimer = READINT16(save_p);
		players[i].instashield = READUINT8(save_p);
		players[i].wipeoutslow = READUINT8(save_p);
		players[i].justbumped = READUINT8(save_p);

		players[i].drift = READSINT8(save_p);
		players[i].driftcharge = READFIXED(save_p);
		players[i].driftboost = READUINT8(save_p);

		players[i].aizdriftstrat = READSINT8(save_p);
		players[i].aizdrifttilt = READINT32(save_p);
		players[i].aizdriftturn = READINT32(save_p);

		players[i].underwatertilt = READINT32(save_p);

		players[i].offroad = READFIXED(save_p);
		players[i].pogospring = READUINT8(save_p);
		players[i].brakestop = READUINT8(save_p);
		players[i].waterskip = READUINT8(save_p);

		players[i].springstars = READUINT16(save_p);
		players[i].springcolor = READUINT16(save_p);
		players[i].dashpadcooldown = READUINT8(save_p);

		players[i].boostpower = READFIXED(save_p);
		players[i].speedboost = READFIXED(save_p);
		players[i].accelboost = READFIXED(save_p);
		players[i].handleboost = READFIXED(save_p);
		players[i].boostangle = READANGLE(save_p);

		players[i].tripwireState = READUINT8(save_p);
		players[i].tripwirePass = READUINT8(save_p);
		players[i].tripwireLeniency = READUINT16(save_p);

		players[i].itemroulette = READUINT16(save_p);
		players[i].roulettetype = READUINT8(save_p);

		players[i].itemtype = READSINT8(save_p);
		players[i].itemamount = READUINT8(save_p);
		players[i].throwdir = READSINT8(save_p);

		players[i].sadtimer = READUINT8(save_p);

		players[i].rings = READSINT8(save_p);
		players[i].pickuprings = READUINT8(save_p);
		players[i].ringdelay = READUINT8(save_p);
		players[i].ringboost = READUINT16(save_p);
		players[i].sparkleanim = READUINT8(save_p);
		players[i].superring = READUINT16(save_p);

		players[i].curshield = READUINT8(save_p);
		players[i].bubblecool = READUINT8(save_p);
		players[i].bubbleblowup = READUINT8(save_p);
		players[i].flamedash = READUINT16(save_p);
		players[i].flamemeter = READUINT16(save_p);
		players[i].flamelength = READUINT8(save_p);

		players[i].hyudorotimer = READUINT16(save_p);
		players[i].stealingtimer = READSINT8(save_p);

		players[i].sneakertimer = READUINT16(save_p);
		players[i].floorboost = READUINT8(save_p);
		players[i].waterrun = READUINT8(save_p);
		
		players[i].boostcharge = READUINT8(save_p);

		players[i].growshrinktimer = READINT16(save_p);
		players[i].growcancel = READINT16(save_p);
		players[i].rocketsneakertimer = READUINT16(save_p);
		players[i].invincibilitytimer = READUINT16(save_p);

		players[i].eggmanexplode = READUINT8(save_p);
		players[i].eggmanblame = READSINT8(save_p);

		players[i].bananadrag = READUINT8(save_p);

		players[i].lastjawztarget = READSINT8(save_p);
		players[i].jawztargetdelay = READUINT8(save_p);

		players[i].confirmVictim = READUINT8(save_p);
		players[i].confirmVictimDelay = READUINT8(save_p);

		players[i].roundscore = READUINT32(save_p);
		players[i].emeralds = READUINT8(save_p);
		players[i].bumpers = READUINT8(save_p);
		players[i].karmadelay = READINT16(save_p);
		players[i].overtimekarma = READUINT32(save_p);
		players[i].spheres = READINT16(save_p);
		players[i].spheredigestion = READUINT32(save_p);

		players[i].glanceDir = READSINT8(save_p);

		players[i].typing_timer = READUINT8(save_p);
		players[i].typing_duration = READUINT8(save_p);

		players[i].kickstartaccel = READUINT8(save_p);

		// botvars_t
		players[i].botvars.difficulty = READUINT8(save_p);
		players[i].botvars.diffincrease = READUINT8(save_p);
		players[i].botvars.rival = (boolean)READUINT8(save_p);
		players[i].botvars.rubberband = READFIXED(save_p);
		players[i].botvars.controller = READUINT16(save_p);
		players[i].botvars.itemdelay = READUINT32(save_p);
		players[i].botvars.itemconfirm = READUINT32(save_p);
		players[i].botvars.turnconfirm = READSINT8(save_p);

		//players[i].viewheight = P_GetPlayerViewHeight(players[i]); // scale cannot be factored in at this point
	}
}

///
/// Colormaps
///

static extracolormap_t *net_colormaps = NULL;
static UINT32 num_net_colormaps = 0;
static UINT32 num_ffloors = 0; // for loading

// Copypasta from r_data.c AddColormapToList
// But also check for equality and return the matching index
static UINT32 CheckAddNetColormapToList(extracolormap_t *extra_colormap)
{
	extracolormap_t *exc, *exc_prev = NULL;
	UINT32 i = 0;

	if (!net_colormaps)
	{
		net_colormaps = R_CopyColormap(extra_colormap, false);
		net_colormaps->next = 0;
		net_colormaps->prev = 0;
		num_net_colormaps = i+1;
		return i;
	}

	for (exc = net_colormaps; exc; exc_prev = exc, exc = exc->next)
	{
		if (R_CheckEqualColormaps(exc, extra_colormap, true, true, true))
			return i;
		i++;
	}

	exc_prev->next = R_CopyColormap(extra_colormap, false);
	extra_colormap->prev = exc_prev;
	extra_colormap->next = 0;

	num_net_colormaps = i+1;
	return i;
}

static extracolormap_t *GetNetColormapFromList(UINT32 index)
{
	// For loading, we have to be tricky:
	// We load the sectors BEFORE knowing the colormap values
	// So if an index doesn't exist, fill our list with dummy colormaps
	// until we get the index we want
	// Then when we load the color data, we set up the dummy colormaps

	extracolormap_t *exc, *last_exc = NULL;
	UINT32 i = 0;

	if (!net_colormaps) // initialize our list
		net_colormaps = R_CreateDefaultColormap(false);

	for (exc = net_colormaps; exc; last_exc = exc, exc = exc->next)
	{
		if (i++ == index)
			return exc;
	}


	// LET'S HOPE that index is a sane value, because we create up to [index]
	// entries in net_colormaps. At this point, we don't know
	// what the total colormap count is
	if (index >= numsectors*3 + num_ffloors)
		// if every sector had a unique colormap change AND a fade color thinker which has two colormap entries
		// AND every ffloor had a fade FOF thinker with one colormap entry
		I_Error("Colormap %d from server is too high for sectors %d", index, (UINT32)numsectors);

	// our index doesn't exist, so just make the entry
	for (; i <= index; i++)
	{
		exc = R_CreateDefaultColormap(false);
		if (last_exc)
			last_exc->next = exc;
		exc->prev = last_exc;
		exc->next = NULL;
		last_exc = exc;
	}
	return exc;
}

static void ClearNetColormaps(void)
{
	// We're actually Z_Freeing each entry here,
	// so don't call this in P_NetUnArchiveColormaps (where entries will be used in-game)
	extracolormap_t *exc, *exc_next;

	for (exc = net_colormaps; exc; exc = exc_next)
	{
		exc_next = exc->next;
		Z_Free(exc);
	}
	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetArchiveColormaps(void)
{
	// We save and then we clean up our colormap mess
	extracolormap_t *exc, *exc_next;
	UINT32 i = 0;
	WRITEUINT32(save_p, num_net_colormaps); // save for safety

	for (exc = net_colormaps; i < num_net_colormaps; i++, exc = exc_next)
	{
		// We must save num_net_colormaps worth of data
		// So fill non-existent entries with default.
		if (!exc)
			exc = R_CreateDefaultColormap(false);

		WRITEUINT8(save_p, exc->fadestart);
		WRITEUINT8(save_p, exc->fadeend);
		WRITEUINT8(save_p, exc->flags);

		WRITEINT32(save_p, exc->rgba);
		WRITEINT32(save_p, exc->fadergba);

#ifdef EXTRACOLORMAPLUMPS
		WRITESTRINGN(save_p, exc->lumpname, 9);
#endif

		exc_next = exc->next;
		Z_Free(exc); // don't need anymore
	}

	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetUnArchiveColormaps(void)
{
	// When we reach this point, we already populated our list with
	// dummy colormaps. Now that we are loading the color data,
	// set up the dummies.
	extracolormap_t *exc, *existing_exc, *exc_next = NULL;
	UINT32 i = 0;

	num_net_colormaps = READUINT32(save_p);

	for (exc = net_colormaps; i < num_net_colormaps; i++, exc = exc_next)
	{
		UINT8 fadestart, fadeend, flags;
		INT32 rgba, fadergba;
#ifdef EXTRACOLORMAPLUMPS
		char lumpname[9];
#endif

		fadestart = READUINT8(save_p);
		fadeend = READUINT8(save_p);
		flags = READUINT8(save_p);

		rgba = READINT32(save_p);
		fadergba = READINT32(save_p);

#ifdef EXTRACOLORMAPLUMPS
		READSTRINGN(save_p, lumpname, 9);

		if (lumpname[0])
		{
			if (!exc)
				// no point making a new entry since nothing points to it,
				// but we needed to read the data so now continue
				continue;

			exc_next = exc->next; // this gets overwritten during our operations here, so get it now
			existing_exc = R_ColormapForName(lumpname);
			*exc = *existing_exc;
			R_AddColormapToList(exc); // see HACK note below on why we're adding duplicates
			continue;
		}
#endif

		if (!exc)
			// no point making a new entry since nothing points to it,
			// but we needed to read the data so now continue
			continue;

		exc_next = exc->next; // this gets overwritten during our operations here, so get it now

		exc->fadestart = fadestart;
		exc->fadeend = fadeend;
		exc->flags = flags;

		exc->rgba = rgba;
		exc->fadergba = fadergba;

#ifdef EXTRACOLORMAPLUMPS
		exc->lump = LUMPERROR;
		exc->lumpname[0] = 0;
#endif

		existing_exc = R_GetColormapFromListByValues(rgba, fadergba, fadestart, fadeend, flags);

		if (existing_exc)
			exc->colormap = existing_exc->colormap;
		else
			// CONS_Debug(DBG_RENDER, "Creating Colormap: rgba(%d,%d,%d,%d) fadergba(%d,%d,%d,%d)\n",
			// 	R_GetRgbaR(rgba), R_GetRgbaG(rgba), R_GetRgbaB(rgba), R_GetRgbaA(rgba),
			//	R_GetRgbaR(fadergba), R_GetRgbaG(fadergba), R_GetRgbaB(fadergba), R_GetRgbaA(fadergba));
			exc->colormap = R_CreateLightTable(exc);

		// HACK: If this dummy is a duplicate, we're going to add it
		// to the extra_colormaps list anyway. I think this is faster
		// than going through every loaded sector and correcting their
		// colormap address to the pre-existing one, PER net_colormap entry
		R_AddColormapToList(exc);

		if (i < num_net_colormaps-1 && !exc_next)
			exc_next = R_CreateDefaultColormap(false);
	}

	// if we still have a valid net_colormap after iterating up to num_net_colormaps,
	// some sector had a colormap index higher than num_net_colormaps. We done goofed or $$$ was corrupted.
	// In any case, add them to the colormap list too so that at least the sectors' colormap
	// addresses are valid and accounted properly
	if (exc_next)
	{
		existing_exc = R_GetDefaultColormap();
		for (exc = exc_next; exc; exc = exc->next)
		{
			exc->colormap = existing_exc->colormap; // all our dummies are default values
			R_AddColormapToList(exc);
		}
	}

	// Don't need these anymore
	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetArchiveTubeWaypoints(void)
{
	INT32 i, j;

	for (i = 0; i < NUMTUBEWAYPOINTSEQUENCES; i++)
	{
		WRITEUINT16(save_p, numtubewaypoints[i]);
		for (j = 0; j < numtubewaypoints[i]; j++)
			WRITEUINT32(save_p, tubewaypoints[i][j] ? tubewaypoints[i][j]->mobjnum : 0);
	}
}

static void P_NetUnArchiveTubeWaypoints(void)
{
	INT32 i, j;
	UINT32 mobjnum;

	for (i = 0; i < NUMTUBEWAYPOINTSEQUENCES; i++)
	{
		numtubewaypoints[i] = READUINT16(save_p);
		for (j = 0; j < numtubewaypoints[i]; j++)
		{
			mobjnum = READUINT32(save_p);
			tubewaypoints[i][j] = (mobjnum == 0) ? NULL : P_FindNewPosition(mobjnum);
		}
	}
}

///
/// World Archiving
///

#define SD_FLOORHT  0x01
#define SD_CEILHT   0x02
#define SD_FLOORPIC 0x04
#define SD_CEILPIC  0x08
#define SD_LIGHT    0x10
#define SD_SPECIAL  0x20
#define SD_DIFF2    0x40
#define SD_FFLOORS  0x80

// diff2 flags
#define SD_FXOFFS    0x01
#define SD_FYOFFS    0x02
#define SD_CXOFFS    0x04
#define SD_CYOFFS    0x08
#define SD_FLOORANG  0x10
#define SD_CEILANG   0x20
#define SD_TAG       0x40
#define SD_DIFF3     0x80

// diff3 flags
#define SD_TAGLIST   0x01
#define SD_COLORMAP  0x02
#define SD_CRUMBLESTATE 0x04
#define SD_FLOORLIGHT 0x08
#define SD_CEILLIGHT 0x10
#define SD_FLAG      0x20
#define SD_SPECIALFLAG 0x40
#define SD_DIFF4     0x80

//diff4 flags
#define SD_DAMAGETYPE 0x01
#define SD_TRIGGERTAG 0x02
#define SD_TRIGGERER 0x04
#define SD_GRAVITY   0x08

#define LD_FLAG     0x01
#define LD_SPECIAL  0x02
#define LD_CLLCOUNT 0x04
#define LD_S1TEXOFF 0x08
#define LD_S1TOPTEX 0x10
#define LD_S1BOTTEX 0x20
#define LD_S1MIDTEX 0x40
#define LD_DIFF2    0x80

// diff2 flags
#define LD_S2TEXOFF      0x01
#define LD_S2TOPTEX      0x02
#define LD_S2BOTTEX      0x04
#define LD_S2MIDTEX      0x08
#define LD_ARGS          0x10
#define LD_STRINGARGS    0x20
#define LD_EXECUTORDELAY 0x40

static boolean P_AreArgsEqual(const line_t *li, const line_t *spawnli)
{
	UINT8 i;
	for (i = 0; i < NUMLINEARGS; i++)
		if (li->args[i] != spawnli->args[i])
			return false;

	return true;
}

static boolean P_AreStringArgsEqual(const line_t *li, const line_t *spawnli)
{
	UINT8 i;
	for (i = 0; i < NUMLINESTRINGARGS; i++)
	{
		if (!li->stringargs[i])
			return !spawnli->stringargs[i];

		if (strcmp(li->stringargs[i], spawnli->stringargs[i]))
			return false;
	}

	return true;
}

#define FD_FLAGS 0x01
#define FD_ALPHA 0x02

// Check if any of the sector's FOFs differ from how they spawned
static boolean CheckFFloorDiff(const sector_t *ss)
{
	ffloor_t *rover;

	for (rover = ss->ffloors; rover; rover = rover->next)
	{
		if (rover->fofflags != rover->spawnflags
		|| rover->alpha != rover->spawnalpha)
			{
				return true; // we found an FOF that changed!
				// don't bother checking for more, we do that later
			}
	}
	return false;
}

// Special case: save the stats of all modified ffloors along with their ffloor "number"s
// we don't bother with ffloors that haven't changed, that would just add to savegame even more than is really needed
static void ArchiveFFloors(const sector_t *ss)
{
	size_t j = 0; // ss->ffloors is saved as ffloor #0, ss->ffloors->next is #1, etc
	ffloor_t *rover;
	UINT8 fflr_diff;
	for (rover = ss->ffloors; rover; rover = rover->next)
	{
		fflr_diff = 0; // reset diff flags
		if (rover->fofflags != rover->spawnflags)
			fflr_diff |= FD_FLAGS;
		if (rover->alpha != rover->spawnalpha)
			fflr_diff |= FD_ALPHA;

		if (fflr_diff)
		{
			WRITEUINT16(save_p, j); // save ffloor "number"
			WRITEUINT8(save_p, fflr_diff);
			if (fflr_diff & FD_FLAGS)
				WRITEUINT32(save_p, rover->fofflags);
			if (fflr_diff & FD_ALPHA)
				WRITEINT16(save_p, rover->alpha);
		}
		j++;
	}
	WRITEUINT16(save_p, 0xffff);
}

static void UnArchiveFFloors(const sector_t *ss)
{
	UINT16 j = 0; // number of current ffloor in loop
	UINT16 fflr_i; // saved ffloor "number" of next modified ffloor
	UINT16 fflr_diff; // saved ffloor diff
	ffloor_t *rover;

	rover = ss->ffloors;
	if (!rover) // it is assumed sectors[i].ffloors actually exists, but just in case...
		I_Error("Sector does not have any ffloors!");

	fflr_i = READUINT16(save_p); // get first modified ffloor's number ready
	for (;;) // for some reason the usual for (rover = x; ...) thing doesn't work here?
	{
		if (fflr_i == 0xffff) // end of modified ffloors list, let's stop already
			break;
		// should NEVER need to be checked
		//if (rover == NULL)
			//break;
		if (j != fflr_i) // this ffloor was not modified
		{
			j++;
			rover = rover->next;
			continue;
		}

		fflr_diff = READUINT8(save_p);

		if (fflr_diff & FD_FLAGS)
			rover->fofflags = READUINT32(save_p);
		if (fflr_diff & FD_ALPHA)
			rover->alpha = READINT16(save_p);

		fflr_i = READUINT16(save_p); // get next ffloor "number" ready

		j++;
		rover = rover->next;
	}
}

static void ArchiveSectors(void)
{
	size_t i, j;
	const sector_t *ss = sectors;
	const sector_t *spawnss = spawnsectors;
	UINT8 diff, diff2, diff3, diff4;

	for (i = 0; i < numsectors; i++, ss++, spawnss++)
	{
		diff = diff2 = diff3 = diff4 = 0;
		if (ss->floorheight != spawnss->floorheight)
			diff |= SD_FLOORHT;
		if (ss->ceilingheight != spawnss->ceilingheight)
			diff |= SD_CEILHT;
		//
		// flats
		//
		if (ss->floorpic != spawnss->floorpic)
			diff |= SD_FLOORPIC;
		if (ss->ceilingpic != spawnss->ceilingpic)
			diff |= SD_CEILPIC;

		if (ss->lightlevel != spawnss->lightlevel)
			diff |= SD_LIGHT;
		if (ss->special != spawnss->special)
			diff |= SD_SPECIAL;

		if (ss->floor_xoffs != spawnss->floor_xoffs)
			diff2 |= SD_FXOFFS;
		if (ss->floor_yoffs != spawnss->floor_yoffs)
			diff2 |= SD_FYOFFS;
		if (ss->ceiling_xoffs != spawnss->ceiling_xoffs)
			diff2 |= SD_CXOFFS;
		if (ss->ceiling_yoffs != spawnss->ceiling_yoffs)
			diff2 |= SD_CYOFFS;
		if (ss->floorpic_angle != spawnss->floorpic_angle)
			diff2 |= SD_FLOORANG;
		if (ss->ceilingpic_angle != spawnss->ceilingpic_angle)
			diff2 |= SD_CEILANG;

		if (!Tag_Compare(&ss->tags, &spawnss->tags))
			diff2 |= SD_TAG;

		if (ss->extra_colormap != spawnss->extra_colormap)
			diff3 |= SD_COLORMAP;
		if (ss->crumblestate)
			diff3 |= SD_CRUMBLESTATE;

		if (ss->floorlightlevel != spawnss->floorlightlevel || ss->floorlightabsolute != spawnss->floorlightabsolute)
			diff3 |= SD_FLOORLIGHT;
		if (ss->ceilinglightlevel != spawnss->ceilinglightlevel || ss->ceilinglightabsolute != spawnss->ceilinglightabsolute)
			diff3 |= SD_CEILLIGHT;
		if (ss->flags != spawnss->flags)
			diff3 |= SD_FLAG;
		if (ss->specialflags != spawnss->specialflags)
			diff3 |= SD_SPECIALFLAG;
		if (ss->damagetype != spawnss->damagetype)
			diff4 |= SD_DAMAGETYPE;
		if (ss->triggertag != spawnss->triggertag)
			diff4 |= SD_TRIGGERTAG;
		if (ss->triggerer != spawnss->triggerer)
			diff4 |= SD_TRIGGERER;
		if (ss->gravity != spawnss->gravity)
			diff4 |= SD_GRAVITY;

		if (ss->ffloors && CheckFFloorDiff(ss))
			diff |= SD_FFLOORS;

		if (diff4)
			diff3 |= SD_DIFF4;

		if (diff3)
			diff2 |= SD_DIFF3;

		if (diff2)
			diff |= SD_DIFF2;

		if (diff)
		{
			WRITEUINT16(save_p, i);
			WRITEUINT8(save_p, diff);
			if (diff & SD_DIFF2)
				WRITEUINT8(save_p, diff2);
			if (diff2 & SD_DIFF3)
				WRITEUINT8(save_p, diff3);
			if (diff3 & SD_DIFF4)
				WRITEUINT8(save_p, diff4);
			if (diff & SD_FLOORHT)
				WRITEFIXED(save_p, ss->floorheight);
			if (diff & SD_CEILHT)
				WRITEFIXED(save_p, ss->ceilingheight);
			if (diff & SD_FLOORPIC)
				WRITEMEM(save_p, levelflats[ss->floorpic].name, 8);
			if (diff & SD_CEILPIC)
				WRITEMEM(save_p, levelflats[ss->ceilingpic].name, 8);
			if (diff & SD_LIGHT)
				WRITEINT16(save_p, ss->lightlevel);
			if (diff & SD_SPECIAL)
				WRITEINT16(save_p, ss->special);
			if (diff2 & SD_FXOFFS)
				WRITEFIXED(save_p, ss->floor_xoffs);
			if (diff2 & SD_FYOFFS)
				WRITEFIXED(save_p, ss->floor_yoffs);
			if (diff2 & SD_CXOFFS)
				WRITEFIXED(save_p, ss->ceiling_xoffs);
			if (diff2 & SD_CYOFFS)
				WRITEFIXED(save_p, ss->ceiling_yoffs);
			if (diff2 & SD_FLOORANG)
				WRITEANGLE(save_p, ss->floorpic_angle);
			if (diff2 & SD_CEILANG)
				WRITEANGLE(save_p, ss->ceilingpic_angle);
			if (diff2 & SD_TAG)
			{
				WRITEUINT32(save_p, ss->tags.count);
				for (j = 0; j < ss->tags.count; j++)
					WRITEINT16(save_p, ss->tags.tags[j]);
			}

			if (diff3 & SD_COLORMAP)
				WRITEUINT32(save_p, CheckAddNetColormapToList(ss->extra_colormap));
					// returns existing index if already added, or appends to net_colormaps and returns new index
			if (diff3 & SD_CRUMBLESTATE)
				WRITEINT32(save_p, ss->crumblestate);
			if (diff3 & SD_FLOORLIGHT)
			{
				WRITEINT16(save_p, ss->floorlightlevel);
				WRITEUINT8(save_p, ss->floorlightabsolute);
			}
			if (diff3 & SD_CEILLIGHT)
			{
				WRITEINT16(save_p, ss->ceilinglightlevel);
				WRITEUINT8(save_p, ss->ceilinglightabsolute);
			}
			if (diff3 & SD_FLAG)
				WRITEUINT32(save_p, ss->flags);
			if (diff3 & SD_SPECIALFLAG)
				WRITEUINT32(save_p, ss->specialflags);
			if (diff4 & SD_DAMAGETYPE)
				WRITEUINT8(save_p, ss->damagetype);
			if (diff4 & SD_TRIGGERTAG)
				WRITEINT16(save_p, ss->triggertag);
			if (diff4 & SD_TRIGGERER)
				WRITEUINT8(save_p, ss->triggerer);
			if (diff4 & SD_GRAVITY)
				WRITEFIXED(save_p, ss->gravity);
			if (diff & SD_FFLOORS)
				ArchiveFFloors(ss);
		}
	}

	WRITEUINT16(save_p, 0xffff);
}

static void UnArchiveSectors(void)
{
	UINT16 i, j;
	UINT8 diff, diff2, diff3, diff4;
	for (;;)
	{
		i = READUINT16(save_p);

		if (i == 0xffff)
			break;

		if (i > numsectors)
			I_Error("Invalid sector number %u from server (expected end at %s)", i, sizeu1(numsectors));

		diff = READUINT8(save_p);
		if (diff & SD_DIFF2)
			diff2 = READUINT8(save_p);
		else
			diff2 = 0;
		if (diff2 & SD_DIFF3)
			diff3 = READUINT8(save_p);
		else
			diff3 = 0;
		if (diff3 & SD_DIFF4)
			diff4 = READUINT8(save_p);
		else
			diff4 = 0;

		if (diff & SD_FLOORHT)
			sectors[i].floorheight = READFIXED(save_p);
		if (diff & SD_CEILHT)
			sectors[i].ceilingheight = READFIXED(save_p);
		if (diff & SD_FLOORPIC)
		{
			sectors[i].floorpic = P_AddLevelFlatRuntime((char *)save_p);
			save_p += 8;
		}
		if (diff & SD_CEILPIC)
		{
			sectors[i].ceilingpic = P_AddLevelFlatRuntime((char *)save_p);
			save_p += 8;
		}
		if (diff & SD_LIGHT)
			sectors[i].lightlevel = READINT16(save_p);
		if (diff & SD_SPECIAL)
			sectors[i].special = READINT16(save_p);

		if (diff2 & SD_FXOFFS)
			sectors[i].floor_xoffs = READFIXED(save_p);
		if (diff2 & SD_FYOFFS)
			sectors[i].floor_yoffs = READFIXED(save_p);
		if (diff2 & SD_CXOFFS)
			sectors[i].ceiling_xoffs = READFIXED(save_p);
		if (diff2 & SD_CYOFFS)
			sectors[i].ceiling_yoffs = READFIXED(save_p);
		if (diff2 & SD_FLOORANG)
			sectors[i].floorpic_angle  = READANGLE(save_p);
		if (diff2 & SD_CEILANG)
			sectors[i].ceilingpic_angle = READANGLE(save_p);
		if (diff2 & SD_TAG)
		{
			size_t ncount = READUINT32(save_p);

			// Remove entries from global lists.
			for (j = 0; j < sectors[i].tags.count; j++)
				Taggroup_Remove(tags_sectors, sectors[i].tags.tags[j], i);

			// Reallocate if size differs.
			if (ncount != sectors[i].tags.count)
			{
				sectors[i].tags.count = ncount;
				sectors[i].tags.tags = Z_Realloc(sectors[i].tags.tags, ncount*sizeof(mtag_t), PU_LEVEL, NULL);
			}

			for (j = 0; j < ncount; j++)
				sectors[i].tags.tags[j] = READINT16(save_p);

			// Add new entries.
			for (j = 0; j < sectors[i].tags.count; j++)
				Taggroup_Remove(tags_sectors, sectors[i].tags.tags[j], i);
		}


		if (diff3 & SD_COLORMAP)
			sectors[i].extra_colormap = GetNetColormapFromList(READUINT32(save_p));
		if (diff3 & SD_CRUMBLESTATE)
			sectors[i].crumblestate = READINT32(save_p);
		if (diff3 & SD_FLOORLIGHT)
		{
			sectors[i].floorlightlevel = READINT16(save_p);
			sectors[i].floorlightabsolute = READUINT8(save_p);
		}
		if (diff3 & SD_CEILLIGHT)
		{
			sectors[i].ceilinglightlevel = READINT16(save_p);
			sectors[i].ceilinglightabsolute = READUINT8(save_p);
		}
		if (diff3 & SD_FLAG)
		{
			sectors[i].flags = READUINT32(save_p);
			CheckForReverseGravity |= (sectors[i].flags & MSF_GRAVITYFLIP);
		}
		if (diff3 & SD_SPECIALFLAG)
			sectors[i].specialflags = READUINT32(save_p);
		if (diff4 & SD_DAMAGETYPE)
			sectors[i].damagetype = READUINT8(save_p);
		if (diff4 & SD_TRIGGERTAG)
			sectors[i].triggertag = READINT16(save_p);
		if (diff4 & SD_TRIGGERER)
			sectors[i].triggerer = READUINT8(save_p);
		if (diff4 & SD_GRAVITY)
			sectors[i].gravity = READFIXED(save_p);

		if (diff & SD_FFLOORS)
			UnArchiveFFloors(&sectors[i]);
	}
}

static void ArchiveLines(void)
{
	size_t i;
	const line_t *li = lines;
	const line_t *spawnli = spawnlines;
	const side_t *si;
	const side_t *spawnsi;
	UINT8 diff, diff2; // no diff3

	for (i = 0; i < numlines; i++, spawnli++, li++)
	{
		diff = diff2 = 0;

		if (li->special != spawnli->special)
			diff |= LD_SPECIAL;

		if (spawnli->special == 321 || spawnli->special == 322) // only reason li->callcount would be non-zero is if either of these are involved
			diff |= LD_CLLCOUNT;

		if (!P_AreArgsEqual(li, spawnli))
			diff2 |= LD_ARGS;

		if (!P_AreStringArgsEqual(li, spawnli))
			diff2 |= LD_STRINGARGS;

		if (li->executordelay != spawnli->executordelay)
			diff2 |= LD_EXECUTORDELAY;

		if (li->sidenum[0] != 0xffff)
		{
			si = &sides[li->sidenum[0]];
			spawnsi = &spawnsides[li->sidenum[0]];
			if (si->textureoffset != spawnsi->textureoffset)
				diff |= LD_S1TEXOFF;
			//SoM: 4/1/2000: Some textures are colormaps. Don't worry about invalid textures.
			if (si->toptexture != spawnsi->toptexture)
				diff |= LD_S1TOPTEX;
			if (si->bottomtexture != spawnsi->bottomtexture)
				diff |= LD_S1BOTTEX;
			if (si->midtexture != spawnsi->midtexture)
				diff |= LD_S1MIDTEX;
		}
		if (li->sidenum[1] != 0xffff)
		{
			si = &sides[li->sidenum[1]];
			spawnsi = &spawnsides[li->sidenum[1]];
			if (si->textureoffset != spawnsi->textureoffset)
				diff2 |= LD_S2TEXOFF;
			if (si->toptexture != spawnsi->toptexture)
				diff2 |= LD_S2TOPTEX;
			if (si->bottomtexture != spawnsi->bottomtexture)
				diff2 |= LD_S2BOTTEX;
			if (si->midtexture != spawnsi->midtexture)
				diff2 |= LD_S2MIDTEX;
		}

		if (diff2)
			diff |= LD_DIFF2;

		if (diff)
		{
			WRITEINT16(save_p, i);
			WRITEUINT8(save_p, diff);
			if (diff & LD_DIFF2)
				WRITEUINT8(save_p, diff2);
			if (diff & LD_FLAG)
				WRITEUINT32(save_p, li->flags);
			if (diff & LD_SPECIAL)
				WRITEINT16(save_p, li->special);
			if (diff & LD_CLLCOUNT)
				WRITEINT16(save_p, li->callcount);

			si = &sides[li->sidenum[0]];
			if (diff & LD_S1TEXOFF)
				WRITEFIXED(save_p, si->textureoffset);
			if (diff & LD_S1TOPTEX)
				WRITEINT32(save_p, si->toptexture);
			if (diff & LD_S1BOTTEX)
				WRITEINT32(save_p, si->bottomtexture);
			if (diff & LD_S1MIDTEX)
				WRITEINT32(save_p, si->midtexture);

			si = &sides[li->sidenum[1]];
			if (diff2 & LD_S2TEXOFF)
				WRITEFIXED(save_p, si->textureoffset);
			if (diff2 & LD_S2TOPTEX)
				WRITEINT32(save_p, si->toptexture);
			if (diff2 & LD_S2BOTTEX)
				WRITEINT32(save_p, si->bottomtexture);
			if (diff2 & LD_S2MIDTEX)
				WRITEINT32(save_p, si->midtexture);
			if (diff2 & LD_ARGS)
			{
				UINT8 j;
				for (j = 0; j < NUMLINEARGS; j++)
					WRITEINT32(save_p, li->args[j]);
			}
			if (diff2 & LD_STRINGARGS)
			{
				UINT8 j;
				for (j = 0; j < NUMLINESTRINGARGS; j++)
				{
					size_t len, k;

					if (!li->stringargs[j])
					{
						WRITEINT32(save_p, 0);
						continue;
					}

					len = strlen(li->stringargs[j]);
					WRITEINT32(save_p, len);
					for (k = 0; k < len; k++)
						WRITECHAR(save_p, li->stringargs[j][k]);
				}
			}
			if (diff2 & LD_EXECUTORDELAY)
				WRITEINT32(save_p, li->executordelay);
		}
	}
	WRITEUINT16(save_p, 0xffff);
}

static void UnArchiveLines(void)
{
	UINT16 i;
	line_t *li;
	side_t *si;
	UINT8 diff, diff2; // no diff3

	for (;;)
	{
		i = READUINT16(save_p);

		if (i == 0xffff)
			break;
		if (i > numlines)
			I_Error("Invalid line number %u from server", i);

		diff = READUINT8(save_p);
		li = &lines[i];

		if (diff & LD_DIFF2)
			diff2 = READUINT8(save_p);
		else
			diff2 = 0;

		if (diff & LD_FLAG)
			li->flags = READUINT32(save_p);
		if (diff & LD_SPECIAL)
			li->special = READINT16(save_p);
		if (diff & LD_CLLCOUNT)
			li->callcount = READINT16(save_p);

		si = &sides[li->sidenum[0]];
		if (diff & LD_S1TEXOFF)
			si->textureoffset = READFIXED(save_p);
		if (diff & LD_S1TOPTEX)
			si->toptexture = READINT32(save_p);
		if (diff & LD_S1BOTTEX)
			si->bottomtexture = READINT32(save_p);
		if (diff & LD_S1MIDTEX)
			si->midtexture = READINT32(save_p);

		si = &sides[li->sidenum[1]];
		if (diff2 & LD_S2TEXOFF)
			si->textureoffset = READFIXED(save_p);
		if (diff2 & LD_S2TOPTEX)
			si->toptexture = READINT32(save_p);
		if (diff2 & LD_S2BOTTEX)
			si->bottomtexture = READINT32(save_p);
		if (diff2 & LD_S2MIDTEX)
			si->midtexture = READINT32(save_p);
		if (diff2 & LD_ARGS)
		{
			UINT8 j;
			for (j = 0; j < NUMLINEARGS; j++)
				li->args[j] = READINT32(save_p);
		}
		if (diff2 & LD_STRINGARGS)
		{
			UINT8 j;
			for (j = 0; j < NUMLINESTRINGARGS; j++)
			{
				size_t len = READINT32(save_p);
				size_t k;

				if (!len)
				{
					Z_Free(li->stringargs[j]);
					li->stringargs[j] = NULL;
					continue;
				}

				li->stringargs[j] = Z_Realloc(li->stringargs[j], len + 1, PU_LEVEL, NULL);
				for (k = 0; k < len; k++)
					li->stringargs[j][k] = READCHAR(save_p);
				li->stringargs[j][len] = '\0';
			}
		}
		if (diff2 & LD_EXECUTORDELAY)
			li->executordelay = READINT32(save_p);

	}
}

static void P_NetArchiveWorld(void)
{
	// initialize colormap vars because paranoia
	ClearNetColormaps();

	WRITEUINT32(save_p, ARCHIVEBLOCK_WORLD);

	ArchiveSectors();
	ArchiveLines();
	R_ClearTextureNumCache(false);
}

static void P_NetUnArchiveWorld(void)
{
	UINT16 i;

	if (READUINT32(save_p) != ARCHIVEBLOCK_WORLD)
		I_Error("Bad $$$.sav at archive block World");

	// initialize colormap vars because paranoia
	ClearNetColormaps();

	// count the level's ffloors so that colormap loading can have an upper limit
	for (i = 0; i < numsectors; i++)
	{
		ffloor_t *rover;
		for (rover = sectors[i].ffloors; rover; rover = rover->next)
			num_ffloors++;
	}

	UnArchiveSectors();
	UnArchiveLines();
}

//
// Thinkers
//

typedef enum
{
	MD_SPAWNPOINT  = 1,
	MD_POS         = 1<<1,
	MD_TYPE        = 1<<2,
	MD_MOM         = 1<<3,
	MD_RADIUS      = 1<<4,
	MD_HEIGHT      = 1<<5,
	MD_FLAGS       = 1<<6,
	MD_HEALTH      = 1<<7,
	MD_RTIME       = 1<<8,
	MD_STATE       = 1<<9,
	MD_TICS        = 1<<10,
	MD_SPRITE      = 1<<11,
	MD_FRAME       = 1<<12,
	MD_EFLAGS      = 1<<13,
	MD_PLAYER      = 1<<14,
	MD_MOVEDIR     = 1<<15,
	MD_MOVECOUNT   = 1<<16,
	MD_THRESHOLD   = 1<<17,
	MD_LASTLOOK    = 1<<18,
	MD_TARGET      = 1<<19,
	MD_TRACER      = 1<<20,
	MD_FRICTION    = 1<<21,
	MD_MOVEFACTOR  = 1<<22,
	MD_FLAGS2      = 1<<23,
	MD_FUSE        = 1<<24,
	MD_WATERTOP    = 1<<25,
	MD_WATERBOTTOM = 1<<26,
	MD_SCALE       = 1<<27,
	MD_DSCALE      = 1<<28,
	MD_BLUEFLAG    = 1<<29,
	MD_REDFLAG     = 1<<30,
	MD_MORE        = (INT32)(1U<<31)
} mobj_diff_t;

typedef enum
{
	MD2_CUSVAL       = 1,
	MD2_CVMEM        = 1<<1,
	MD2_SKIN         = 1<<2,
	MD2_COLOR        = 1<<3,
	MD2_SCALESPEED   = 1<<4,
	MD2_EXTVAL1      = 1<<5,
	MD2_EXTVAL2      = 1<<6,
	MD2_HNEXT        = 1<<7,
	MD2_HPREV        = 1<<8,
	MD2_FLOORROVER   = 1<<9,
	MD2_CEILINGROVER = 1<<10,
	MD2_SLOPE        = 1<<11,
	MD2_COLORIZED    = 1<<12,
	MD2_MIRRORED     = 1<<13,
	MD2_ROLLANGLE    = 1<<14,
	MD2_SHADOWSCALE  = 1<<15,
	MD2_RENDERFLAGS  = 1<<16,
	// 1<<17 was taken out, maybe reuse later
	MD2_SPRITEXSCALE = 1<<18,
	MD2_SPRITEYSCALE = 1<<19,
	MD2_SPRITEXOFFSET = 1<<20,
	MD2_SPRITEYOFFSET = 1<<21,
	MD2_FLOORSPRITESLOPE = 1<<22,
	MD2_DISPOFFSET   = 1<<23,
	//free       = 1<<24,
	MD2_WAYPOINTCAP  = 1<<25,
	MD2_KITEMCAP     = 1<<26,
	MD2_ITNEXT       = 1<<27,
	MD2_LASTMOMZ     = 1<<28,
	MD2_TERRAIN      = 1<<29,
} mobj_diff2_t;

typedef enum
{
	tc_mobj,
	tc_ceiling,
	tc_floor,
	tc_flash,
	tc_strobe,
	tc_glow,
	tc_fireflicker,
	tc_thwomp,
	tc_camerascanner,
	tc_elevator,
	tc_continuousfalling,
	tc_bouncecheese,
	tc_startcrumble,
	tc_marioblock,
	tc_marioblockchecker,
	tc_floatsector,
	tc_crushceiling,
	tc_scroll,
	tc_friction,
	tc_pusher,
	tc_laserflash,
	tc_lightfade,
	tc_executor,
	tc_raisesector,
	tc_noenemies,
	tc_eachtime,
	tc_disappear,
	tc_fade,
	tc_fadecolormap,
	tc_planedisplace,
	tc_dynslopeline,
	tc_dynslopevert,
	tc_polyrotate, // haleyjd 03/26/06: polyobjects
	tc_polymove,
	tc_polywaypoint,
	tc_polyslidedoor,
	tc_polyswingdoor,
	tc_polyflag,
	tc_polydisplace,
	tc_polyrotdisplace,
	tc_polyfade,
	tc_end
} specials_e;

static inline UINT32 SaveMobjnum(const mobj_t *mobj)
{
	if (mobj) return mobj->mobjnum;
	return 0;
}

static UINT32 SaveSector(const sector_t *sector)
{
	if (sector) return (UINT32)(sector - sectors);
	return 0xFFFFFFFF;
}

static UINT32 SaveLine(const line_t *line)
{
	if (line) return (UINT32)(line - lines);
	return 0xFFFFFFFF;
}

static inline UINT32 SavePlayer(const player_t *player)
{
	if (player) return (UINT32)(player - players);
	return 0xFFFFFFFF;
}

static UINT32 SaveSlope(const pslope_t *slope)
{
	if (slope) return (UINT32)(slope->id);
	return 0xFFFFFFFF;
}

static void SaveMobjThinker(const thinker_t *th, const UINT8 type)
{
	const mobj_t *mobj = (const mobj_t *)th;
	UINT32 diff;
	UINT32 diff2;

	// Ignore stationary hoops - these will be respawned from mapthings.
	if (mobj->type == MT_HOOP)
		return;

	// These are NEVER saved.
	if (mobj->type == MT_HOOPCOLLIDE)
		return;

	// This hoop has already been collected.
	if (mobj->type == MT_HOOPCENTER && mobj->threshold == 4242)
		return;

	// MT_SPARK: used for debug stuff
	if (mobj->type == MT_SPARK)
		return;

	if (mobj->spawnpoint)
	{
		// spawnpoint is not modified but we must save it since it is an identifier
		diff = MD_SPAWNPOINT;

		if ((mobj->x != mobj->spawnpoint->x << FRACBITS) ||
			(mobj->y != mobj->spawnpoint->y << FRACBITS) ||
			(mobj->angle != FixedAngle(mobj->spawnpoint->angle*FRACUNIT)) ||
			(mobj->pitch != FixedAngle(mobj->spawnpoint->pitch*FRACUNIT)) ||
			(mobj->roll != FixedAngle(mobj->spawnpoint->roll*FRACUNIT)) )
			diff |= MD_POS;

		if (mobj->info->doomednum != mobj->spawnpoint->type)
			diff |= MD_TYPE;
	}
	else
		diff = MD_POS | MD_TYPE; // not a map spawned thing so make it from scratch

	diff2 = 0;

	// not the default but the most probable
	if (mobj->momx != 0 || mobj->momy != 0 || mobj->momz != 0 || mobj->pmomz != 0)
		diff |= MD_MOM;
	if (mobj->radius != mobj->info->radius)
		diff |= MD_RADIUS;
	if (mobj->height != mobj->info->height)
		diff |= MD_HEIGHT;
	if (mobj->flags != mobj->info->flags)
		diff |= MD_FLAGS;
	if (mobj->flags2)
		diff |= MD_FLAGS2;
	if (mobj->health != mobj->info->spawnhealth)
		diff |= MD_HEALTH;
	if (mobj->reactiontime != mobj->info->reactiontime)
		diff |= MD_RTIME;
	if ((statenum_t)(mobj->state-states) != mobj->info->spawnstate)
		diff |= MD_STATE;
	if (mobj->tics != mobj->state->tics)
		diff |= MD_TICS;
	if (mobj->sprite != mobj->state->sprite)
		diff |= MD_SPRITE;
	if (mobj->sprite == SPR_PLAY && mobj->sprite2 != (mobj->state->frame&FF_FRAMEMASK))
		diff |= MD_SPRITE;
	if (mobj->frame != mobj->state->frame)
		diff |= MD_FRAME;
	if (mobj->anim_duration != (UINT16)mobj->state->var2)
		diff |= MD_FRAME;
	if (mobj->eflags)
		diff |= MD_EFLAGS;
	if (mobj->player)
		diff |= MD_PLAYER;

	if (mobj->movedir)
		diff |= MD_MOVEDIR;
	if (mobj->movecount)
		diff |= MD_MOVECOUNT;
	if (mobj->threshold)
		diff |= MD_THRESHOLD;
	if (mobj->lastlook != -1)
		diff |= MD_LASTLOOK;
	if (mobj->target)
		diff |= MD_TARGET;
	if (mobj->tracer)
		diff |= MD_TRACER;
	if (mobj->friction != ORIG_FRICTION)
		diff |= MD_FRICTION;
	if (mobj->movefactor != FRACUNIT)
		diff |= MD_MOVEFACTOR;
	if (mobj->fuse)
		diff |= MD_FUSE;
	if (mobj->watertop)
		diff |= MD_WATERTOP;
	if (mobj->waterbottom)
		diff |= MD_WATERBOTTOM;
	if (mobj->scale != FRACUNIT)
		diff |= MD_SCALE;
	if (mobj->destscale != mobj->scale)
		diff |= MD_DSCALE;
	if (mobj->scalespeed != mapobjectscale/12)
		diff2 |= MD2_SCALESPEED;

	if (mobj == redflag)
		diff |= MD_REDFLAG;
	if (mobj == blueflag)
		diff |= MD_BLUEFLAG;

	if (mobj->cusval)
		diff2 |= MD2_CUSVAL;
	if (mobj->cvmem)
		diff2 |= MD2_CVMEM;
	if (mobj->color)
		diff2 |= MD2_COLOR;
	if (mobj->skin)
		diff2 |= MD2_SKIN;
	if (mobj->extravalue1)
		diff2 |= MD2_EXTVAL1;
	if (mobj->extravalue2)
		diff2 |= MD2_EXTVAL2;
	if (mobj->hnext)
		diff2 |= MD2_HNEXT;
	if (mobj->hprev)
		diff2 |= MD2_HPREV;
	if (mobj->standingslope)
		diff2 |= MD2_SLOPE;
	if (mobj->colorized)
		diff2 |= MD2_COLORIZED;
	if (mobj->floorrover)
		diff2 |= MD2_FLOORROVER;
	if (mobj->ceilingrover)
		diff2 |= MD2_CEILINGROVER;
	if (mobj->mirrored)
		diff2 |= MD2_MIRRORED;
	if (mobj->rollangle)
		diff2 |= MD2_ROLLANGLE;
	if (mobj->shadowscale)
		diff2 |= MD2_SHADOWSCALE;
	if (mobj->renderflags)
		diff2 |= MD2_RENDERFLAGS;
	if (mobj->spritexscale != FRACUNIT)
		diff2 |= MD2_SPRITEXSCALE;
	if (mobj->spriteyscale != FRACUNIT)
		diff2 |= MD2_SPRITEYSCALE;
	if (mobj->spritexoffset)
		diff2 |= MD2_SPRITEXOFFSET;
	if (mobj->spriteyoffset)
		diff2 |= MD2_SPRITEYOFFSET;
	if (mobj->floorspriteslope)
	{
		pslope_t *slope = mobj->floorspriteslope;
		if (slope->zangle || slope->zdelta || slope->xydirection
		|| slope->o.x || slope->o.y || slope->o.z
		|| slope->d.x || slope->d.y
		|| slope->normal.x || slope->normal.y
		|| (slope->normal.z != FRACUNIT))
			diff2 |= MD2_FLOORSPRITESLOPE;
	}
	if (mobj->dispoffset)
		diff2 |= MD2_DISPOFFSET;
	if (mobj == waypointcap)
		diff2 |= MD2_WAYPOINTCAP;
	if (mobj == kitemcap)
		diff2 |= MD2_KITEMCAP;
	if (mobj->itnext)
		diff2 |= MD2_ITNEXT;
	if (mobj->lastmomz)
		diff2 |= MD2_LASTMOMZ;
	if (mobj->terrain != NULL || mobj->terrainOverlay != NULL)
		diff2 |= MD2_TERRAIN;

	if (diff2 != 0)
		diff |= MD_MORE;

	// Scrap all of that. If we're a hoop center, this is ALL we're saving.
	if (mobj->type == MT_HOOPCENTER)
		diff = MD_SPAWNPOINT;

	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, diff);
	if (diff & MD_MORE)
		WRITEUINT32(save_p, diff2);

	// save pointer, at load time we will search this pointer to reinitilize pointers
	WRITEUINT32(save_p, (size_t)mobj);

	WRITEFIXED(save_p, mobj->z); // Force this so 3dfloor problems don't arise.
	WRITEFIXED(save_p, mobj->floorz);
	WRITEFIXED(save_p, mobj->ceilingz);

	if (diff2 & MD2_FLOORROVER)
	{
		WRITEUINT32(save_p, SaveSector(mobj->floorrover->target));
		WRITEUINT16(save_p, P_GetFFloorID(mobj->floorrover));
	}

	if (diff2 & MD2_CEILINGROVER)
	{
		WRITEUINT32(save_p, SaveSector(mobj->ceilingrover->target));
		WRITEUINT16(save_p, P_GetFFloorID(mobj->ceilingrover));
	}

	if (diff & MD_SPAWNPOINT)
	{
		size_t z;

		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] != mobj->spawnpoint)
				continue;
			WRITEUINT16(save_p, z);
			break;
		}
		if (mobj->type == MT_HOOPCENTER)
			return;
	}

	if (diff & MD_TYPE)
		WRITEUINT32(save_p, mobj->type);
	if (diff & MD_POS)
	{
		WRITEFIXED(save_p, mobj->x);
		WRITEFIXED(save_p, mobj->y);
		WRITEANGLE(save_p, mobj->angle);
		WRITEANGLE(save_p, mobj->pitch);
		WRITEANGLE(save_p, mobj->roll);
	}
	if (diff & MD_MOM)
	{
		WRITEFIXED(save_p, mobj->momx);
		WRITEFIXED(save_p, mobj->momy);
		WRITEFIXED(save_p, mobj->momz);
		WRITEFIXED(save_p, mobj->pmomz);
	}
	if (diff & MD_RADIUS)
		WRITEFIXED(save_p, mobj->radius);
	if (diff & MD_HEIGHT)
		WRITEFIXED(save_p, mobj->height);
	if (diff & MD_FLAGS)
		WRITEUINT32(save_p, mobj->flags);
	if (diff & MD_FLAGS2)
		WRITEUINT32(save_p, mobj->flags2);
	if (diff & MD_HEALTH)
		WRITEINT32(save_p, mobj->health);
	if (diff & MD_RTIME)
		WRITEINT32(save_p, mobj->reactiontime);
	if (diff & MD_STATE)
		WRITEUINT16(save_p, mobj->state-states);
	if (diff & MD_TICS)
		WRITEINT32(save_p, mobj->tics);
	if (diff & MD_SPRITE) {
		WRITEUINT16(save_p, mobj->sprite);
		if (mobj->sprite == SPR_PLAY)
			WRITEUINT8(save_p, mobj->sprite2);
	}
	if (diff & MD_FRAME)
	{
		WRITEUINT32(save_p, mobj->frame);
		WRITEUINT16(save_p, mobj->anim_duration);
	}
	if (diff & MD_EFLAGS)
		WRITEUINT16(save_p, mobj->eflags);
	if (diff & MD_PLAYER)
		WRITEUINT8(save_p, mobj->player-players);
	if (diff & MD_MOVEDIR)
		WRITEANGLE(save_p, mobj->movedir);
	if (diff & MD_MOVECOUNT)
		WRITEINT32(save_p, mobj->movecount);
	if (diff & MD_THRESHOLD)
		WRITEINT32(save_p, mobj->threshold);
	if (diff & MD_LASTLOOK)
		WRITEINT32(save_p, mobj->lastlook);
	if (diff & MD_TARGET)
		WRITEUINT32(save_p, mobj->target->mobjnum);
	if (diff & MD_TRACER)
		WRITEUINT32(save_p, mobj->tracer->mobjnum);
	if (diff & MD_FRICTION)
		WRITEFIXED(save_p, mobj->friction);
	if (diff & MD_MOVEFACTOR)
		WRITEFIXED(save_p, mobj->movefactor);
	if (diff & MD_FUSE)
		WRITEINT32(save_p, mobj->fuse);
	if (diff & MD_WATERTOP)
		WRITEFIXED(save_p, mobj->watertop);
	if (diff & MD_WATERBOTTOM)
		WRITEFIXED(save_p, mobj->waterbottom);
	if (diff & MD_SCALE)
		WRITEFIXED(save_p, mobj->scale);
	if (diff & MD_DSCALE)
		WRITEFIXED(save_p, mobj->destscale);
	if (diff2 & MD2_SCALESPEED)
		WRITEFIXED(save_p, mobj->scalespeed);
	if (diff2 & MD2_CUSVAL)
		WRITEINT32(save_p, mobj->cusval);
	if (diff2 & MD2_CVMEM)
		WRITEINT32(save_p, mobj->cvmem);
	if (diff2 & MD2_SKIN)
		WRITEUINT8(save_p, (UINT8)((skin_t *)mobj->skin - skins));
	if (diff2 & MD2_COLOR)
		WRITEUINT16(save_p, mobj->color);
	if (diff2 & MD2_EXTVAL1)
		WRITEINT32(save_p, mobj->extravalue1);
	if (diff2 & MD2_EXTVAL2)
		WRITEINT32(save_p, mobj->extravalue2);
	if (diff2 & MD2_HNEXT)
		WRITEUINT32(save_p, mobj->hnext->mobjnum);
	if (diff2 & MD2_HPREV)
		WRITEUINT32(save_p, mobj->hprev->mobjnum);
	if (diff2 & MD2_ITNEXT)
		WRITEUINT32(save_p, mobj->itnext->mobjnum);
	if (diff2 & MD2_SLOPE)
		WRITEUINT16(save_p, mobj->standingslope->id);
	if (diff2 & MD2_COLORIZED)
		WRITEUINT8(save_p, mobj->colorized);
	if (diff2 & MD2_MIRRORED)
		WRITEUINT8(save_p, mobj->mirrored);
	if (diff2 & MD2_ROLLANGLE)
		WRITEANGLE(save_p, mobj->rollangle);
	if (diff2 & MD2_SHADOWSCALE)
	{
		WRITEFIXED(save_p, mobj->shadowscale);
		WRITEUINT8(save_p, mobj->whiteshadow);
	}
	if (diff2 & MD2_RENDERFLAGS)
	{
		UINT32 rf = mobj->renderflags;
		UINT32 q = rf & RF_DONTDRAW;

		if (q != RF_DONTDRAW // visible for more than one local player
		&& q != (RF_DONTDRAWP1|RF_DONTDRAWP2|RF_DONTDRAWP3)
		&& q != (RF_DONTDRAWP4|RF_DONTDRAWP1|RF_DONTDRAWP2)
		&& q != (RF_DONTDRAWP4|RF_DONTDRAWP1|RF_DONTDRAWP3)
		&& q != (RF_DONTDRAWP4|RF_DONTDRAWP2|RF_DONTDRAWP3))
			rf &= ~q;

		WRITEUINT32(save_p, rf);
	}
	if (diff2 & MD2_SPRITEXSCALE)
		WRITEFIXED(save_p, mobj->spritexscale);
	if (diff2 & MD2_SPRITEYSCALE)
		WRITEFIXED(save_p, mobj->spriteyscale);
	if (diff2 & MD2_SPRITEXOFFSET)
		WRITEFIXED(save_p, mobj->spritexoffset);
	if (diff2 & MD2_SPRITEYOFFSET)
		WRITEFIXED(save_p, mobj->spriteyoffset);
	if (diff2 & MD2_FLOORSPRITESLOPE)
	{
		pslope_t *slope = mobj->floorspriteslope;

		WRITEFIXED(save_p, slope->zdelta);
		WRITEANGLE(save_p, slope->zangle);
		WRITEANGLE(save_p, slope->xydirection);

		WRITEFIXED(save_p, slope->o.x);
		WRITEFIXED(save_p, slope->o.y);
		WRITEFIXED(save_p, slope->o.z);

		WRITEFIXED(save_p, slope->d.x);
		WRITEFIXED(save_p, slope->d.y);

		WRITEFIXED(save_p, slope->normal.x);
		WRITEFIXED(save_p, slope->normal.y);
		WRITEFIXED(save_p, slope->normal.z);
	}
	if (diff2 & MD2_DISPOFFSET)
	{
		WRITEINT32(save_p, mobj->dispoffset);
	}
	if (diff2 & MD2_LASTMOMZ)
	{
		WRITEINT32(save_p, mobj->lastmomz);
	}
	if (diff2 & MD2_TERRAIN)
	{
		WRITEUINT32(save_p, K_GetTerrainHeapIndex(mobj->terrain));
	}

	WRITEUINT32(save_p, mobj->mobjnum);
}

static void SaveNoEnemiesThinker(const thinker_t *th, const UINT8 type)
{
	const noenemies_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
}

static void SaveBounceCheeseThinker(const thinker_t *th, const UINT8 type)
{
	const bouncecheese_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->distance);
	WRITEFIXED(save_p, ht->floorwasheight);
	WRITEFIXED(save_p, ht->ceilingwasheight);
	WRITECHAR(save_p, ht->low);
}

static void SaveContinuousFallThinker(const thinker_t *th, const UINT8 type)
{
	const continuousfall_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->speed);
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floorstartheight);
	WRITEFIXED(save_p, ht->ceilingstartheight);
	WRITEFIXED(save_p, ht->destheight);
}

static void SaveMarioBlockThinker(const thinker_t *th, const UINT8 type)
{
	const mariothink_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->speed);
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floorstartheight);
	WRITEFIXED(save_p, ht->ceilingstartheight);
	WRITEINT16(save_p, ht->tag);
}

static void SaveMarioCheckThinker(const thinker_t *th, const UINT8 type)
{
	const mariocheck_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
}

static void SaveThwompThinker(const thinker_t *th, const UINT8 type)
{
	const thwomp_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->crushspeed);
	WRITEFIXED(save_p, ht->retractspeed);
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floorstartheight);
	WRITEFIXED(save_p, ht->ceilingstartheight);
	WRITEINT32(save_p, ht->delay);
	WRITEINT16(save_p, ht->tag);
	WRITEUINT16(save_p, ht->sound);
	WRITEINT32(save_p, ht->initDelay);
}

static void SaveFloatThinker(const thinker_t *th, const UINT8 type)
{
	const floatthink_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT16(save_p, ht->tag);
}

static void SaveEachTimeThinker(const thinker_t *th, const UINT8 type)
{
	const eachtime_t *ht  = (const void *)th;
	size_t i;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		WRITECHAR(save_p, ht->playersInArea[i]);
	}
	WRITECHAR(save_p, ht->triggerOnExit);
}

static void SaveRaiseThinker(const thinker_t *th, const UINT8 type)
{
	const raise_t *ht  = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT16(save_p, ht->tag);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->ceilingbottom);
	WRITEFIXED(save_p, ht->ceilingtop);
	WRITEFIXED(save_p, ht->basespeed);
	WRITEFIXED(save_p, ht->extraspeed);
	WRITEUINT8(save_p, ht->shaketimer);
	WRITEUINT8(save_p, ht->flags);
}

static void SaveCeilingThinker(const thinker_t *th, const UINT8 type)
{
	const ceiling_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEFIXED(save_p, ht->bottomheight);
	WRITEFIXED(save_p, ht->topheight);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->delay);
	WRITEFIXED(save_p, ht->delaytimer);
	WRITEUINT8(save_p, ht->crush);
	WRITEINT32(save_p, ht->texture);
	WRITEINT32(save_p, ht->direction);
	WRITEINT16(save_p, ht->tag);
	WRITEFIXED(save_p, ht->origspeed);
	WRITEFIXED(save_p, ht->sourceline);
}

static void SaveFloormoveThinker(const thinker_t *th, const UINT8 type)
{
	const floormove_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT8(save_p, ht->crush);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->direction);
	WRITEINT32(save_p, ht->texture);
	WRITEFIXED(save_p, ht->floordestheight);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->origspeed);
	WRITEFIXED(save_p, ht->delay);
	WRITEFIXED(save_p, ht->delaytimer);
	WRITEINT16(save_p, ht->tag);
	WRITEFIXED(save_p, ht->sourceline);
}

static void SaveLightflashThinker(const thinker_t *th, const UINT8 type)
{
	const lightflash_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->maxlight);
	WRITEINT32(save_p, ht->minlight);
}

static void SaveStrobeThinker(const thinker_t *th, const UINT8 type)
{
	const strobe_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->count);
	WRITEINT16(save_p, ht->minlight);
	WRITEINT16(save_p, ht->maxlight);
	WRITEINT32(save_p, ht->darktime);
	WRITEINT32(save_p, ht->brighttime);
}

static void SaveGlowThinker(const thinker_t *th, const UINT8 type)
{
	const glow_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT16(save_p, ht->minlight);
	WRITEINT16(save_p, ht->maxlight);
	WRITEINT16(save_p, ht->direction);
	WRITEINT16(save_p, ht->speed);
}

static inline void SaveFireflickerThinker(const thinker_t *th, const UINT8 type)
{
	const fireflicker_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->count);
	WRITEINT32(save_p, ht->resetcount);
	WRITEINT16(save_p, ht->maxlight);
	WRITEINT16(save_p, ht->minlight);
}

static void SaveElevatorThinker(const thinker_t *th, const UINT8 type)
{
	const elevator_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEUINT32(save_p, SaveSector(ht->actionsector));
	WRITEINT32(save_p, ht->direction);
	WRITEFIXED(save_p, ht->floordestheight);
	WRITEFIXED(save_p, ht->ceilingdestheight);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->origspeed);
	WRITEFIXED(save_p, ht->low);
	WRITEFIXED(save_p, ht->high);
	WRITEFIXED(save_p, ht->distance);
	WRITEFIXED(save_p, ht->delay);
	WRITEFIXED(save_p, ht->delaytimer);
	WRITEFIXED(save_p, ht->floorwasheight);
	WRITEFIXED(save_p, ht->ceilingwasheight);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
}

static void SaveCrumbleThinker(const thinker_t *th, const UINT8 type)
{
	const crumble_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEUINT32(save_p, SaveSector(ht->actionsector));
	WRITEUINT32(save_p, SavePlayer(ht->player)); // was dummy
	WRITEINT32(save_p, ht->direction);
	WRITEINT32(save_p, ht->origalpha);
	WRITEINT32(save_p, ht->timer);
	WRITEFIXED(save_p, ht->speed);
	WRITEFIXED(save_p, ht->floorwasheight);
	WRITEFIXED(save_p, ht->ceilingwasheight);
	WRITEUINT8(save_p, ht->flags);
}

static inline void SaveScrollThinker(const thinker_t *th, const UINT8 type)
{
	const scroll_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEFIXED(save_p, ht->dx);
	WRITEFIXED(save_p, ht->dy);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->control);
	WRITEFIXED(save_p, ht->last_height);
	WRITEFIXED(save_p, ht->vdx);
	WRITEFIXED(save_p, ht->vdy);
	WRITEINT32(save_p, ht->accel);
	WRITEINT32(save_p, ht->exclusive);
	WRITEUINT8(save_p, ht->type);
}

static inline void SaveFrictionThinker(const thinker_t *th, const UINT8 type)
{
	const friction_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->friction);
	WRITEINT32(save_p, ht->movefactor);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->referrer);
	WRITEUINT8(save_p, ht->roverfriction);
}

static inline void SavePusherThinker(const thinker_t *th, const UINT8 type)
{
	const pusher_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEFIXED(save_p, ht->x_mag);
	WRITEFIXED(save_p, ht->y_mag);
	WRITEFIXED(save_p, ht->z_mag);
	WRITEINT32(save_p, ht->affectee);
	WRITEUINT8(save_p, ht->roverpusher);
	WRITEINT32(save_p, ht->referrer);
	WRITEINT32(save_p, ht->exclusive);
	WRITEINT32(save_p, ht->slider);
}

static void SaveLaserThinker(const thinker_t *th, const UINT8 type)
{
	const laserthink_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT16(save_p, ht->tag);
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEUINT8(save_p, ht->nobosses);
}

static void SaveLightlevelThinker(const thinker_t *th, const UINT8 type)
{
	const lightlevel_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT16(save_p, ht->sourcelevel);
	WRITEINT16(save_p, ht->destlevel);
	WRITEFIXED(save_p, ht->fixedcurlevel);
	WRITEFIXED(save_p, ht->fixedpertic);
	WRITEINT32(save_p, ht->timer);
}

static void SaveExecutorThinker(const thinker_t *th, const UINT8 type)
{
	const executor_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveLine(ht->line));
	WRITEUINT32(save_p, SaveMobjnum(ht->caller));
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEINT32(save_p, ht->timer);
}

static void SaveDisappearThinker(const thinker_t *th, const UINT8 type)
{
	const disappear_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, ht->appeartime);
	WRITEUINT32(save_p, ht->disappeartime);
	WRITEUINT32(save_p, ht->offset);
	WRITEUINT32(save_p, ht->timer);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->sourceline);
	WRITEINT32(save_p, ht->exists);
}

static void SaveFadeThinker(const thinker_t *th, const UINT8 type)
{
	const fade_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, CheckAddNetColormapToList(ht->dest_exc));
	WRITEUINT32(save_p, ht->sectornum);
	WRITEUINT32(save_p, ht->ffloornum);
	WRITEINT32(save_p, ht->alpha);
	WRITEINT16(save_p, ht->sourcevalue);
	WRITEINT16(save_p, ht->destvalue);
	WRITEINT16(save_p, ht->destlightlevel);
	WRITEINT16(save_p, ht->speed);
	WRITEUINT8(save_p, (UINT8)ht->ticbased);
	WRITEINT32(save_p, ht->timer);
	WRITEUINT8(save_p, ht->doexists);
	WRITEUINT8(save_p, ht->dotranslucent);
	WRITEUINT8(save_p, ht->dolighting);
	WRITEUINT8(save_p, ht->docolormap);
	WRITEUINT8(save_p, ht->docollision);
	WRITEUINT8(save_p, ht->doghostfade);
	WRITEUINT8(save_p, ht->exactalpha);
}

static void SaveFadeColormapThinker(const thinker_t *th, const UINT8 type)
{
	const fadecolormap_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSector(ht->sector));
	WRITEUINT32(save_p, CheckAddNetColormapToList(ht->source_exc));
	WRITEUINT32(save_p, CheckAddNetColormapToList(ht->dest_exc));
	WRITEUINT8(save_p, (UINT8)ht->ticbased);
	WRITEINT32(save_p, ht->duration);
	WRITEINT32(save_p, ht->timer);
}

static void SavePlaneDisplaceThinker(const thinker_t *th, const UINT8 type)
{
	const planedisplace_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->affectee);
	WRITEINT32(save_p, ht->control);
	WRITEFIXED(save_p, ht->last_height);
	WRITEFIXED(save_p, ht->speed);
	WRITEUINT8(save_p, ht->type);
}

static inline void SaveDynamicLineSlopeThinker(const thinker_t *th, const UINT8 type)
{
	const dynlineplanethink_t* ht = (const void*)th;

	WRITEUINT8(save_p, type);
	WRITEUINT8(save_p, ht->type);
	WRITEUINT32(save_p, SaveSlope(ht->slope));
	WRITEUINT32(save_p, SaveLine(ht->sourceline));
	WRITEFIXED(save_p, ht->extent);
}

static inline void SaveDynamicVertexSlopeThinker(const thinker_t *th, const UINT8 type)
{
	size_t i;
	const dynvertexplanethink_t* ht = (const void*)th;

	WRITEUINT8(save_p, type);
	WRITEUINT32(save_p, SaveSlope(ht->slope));
	for (i = 0; i < 3; i++)
		WRITEUINT32(save_p, SaveSector(ht->secs[i]));
	WRITEMEM(save_p, ht->vex, sizeof(ht->vex));
	WRITEMEM(save_p, ht->origsecheights, sizeof(ht->origsecheights));
	WRITEMEM(save_p, ht->origvecheights, sizeof(ht->origvecheights));
	WRITEUINT8(save_p, ht->relative);
}

static inline void SavePolyrotatetThinker(const thinker_t *th, const UINT8 type)
{
	const polyrotate_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->distance);
	WRITEUINT8(save_p, ht->turnobjs);
}

static void SavePolymoveThinker(const thinker_t *th, const UINT8 type)
{
	const polymove_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->speed);
	WRITEFIXED(save_p, ht->momx);
	WRITEFIXED(save_p, ht->momy);
	WRITEINT32(save_p, ht->distance);
	WRITEANGLE(save_p, ht->angle);
}

static void SavePolywaypointThinker(const thinker_t *th, UINT8 type)
{
	const polywaypoint_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->sequence);
	WRITEINT32(save_p, ht->pointnum);
	WRITEINT32(save_p, ht->direction);
	WRITEUINT8(save_p, ht->returnbehavior);
	WRITEUINT8(save_p, ht->continuous);
	WRITEUINT8(save_p, ht->stophere);
}

static void SavePolyslidedoorThinker(const thinker_t *th, const UINT8 type)
{
	const polyslidedoor_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->delay);
	WRITEINT32(save_p, ht->delayCount);
	WRITEINT32(save_p, ht->initSpeed);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->initDistance);
	WRITEINT32(save_p, ht->distance);
	WRITEUINT32(save_p, ht->initAngle);
	WRITEUINT32(save_p, ht->angle);
	WRITEUINT32(save_p, ht->revAngle);
	WRITEFIXED(save_p, ht->momx);
	WRITEFIXED(save_p, ht->momy);
	WRITEUINT8(save_p, ht->closing);
}

static void SavePolyswingdoorThinker(const thinker_t *th, const UINT8 type)
{
	const polyswingdoor_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->delay);
	WRITEINT32(save_p, ht->delayCount);
	WRITEINT32(save_p, ht->initSpeed);
	WRITEINT32(save_p, ht->speed);
	WRITEINT32(save_p, ht->initDistance);
	WRITEINT32(save_p, ht->distance);
	WRITEUINT8(save_p, ht->closing);
}

static void SavePolydisplaceThinker(const thinker_t *th, const UINT8 type)
{
	const polydisplace_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEUINT32(save_p, SaveSector(ht->controlSector));
	WRITEFIXED(save_p, ht->dx);
	WRITEFIXED(save_p, ht->dy);
	WRITEFIXED(save_p, ht->oldHeights);
}

static void SavePolyrotdisplaceThinker(const thinker_t *th, const UINT8 type)
{
	const polyrotdisplace_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEUINT32(save_p, SaveSector(ht->controlSector));
	WRITEFIXED(save_p, ht->rotscale);
	WRITEUINT8(save_p, ht->turnobjs);
	WRITEFIXED(save_p, ht->oldHeights);
}

static void SavePolyfadeThinker(const thinker_t *th, const UINT8 type)
{
	const polyfade_t *ht = (const void *)th;
	WRITEUINT8(save_p, type);
	WRITEINT32(save_p, ht->polyObjNum);
	WRITEINT32(save_p, ht->sourcevalue);
	WRITEINT32(save_p, ht->destvalue);
	WRITEUINT8(save_p, (UINT8)ht->docollision);
	WRITEUINT8(save_p, (UINT8)ht->doghostfade);
	WRITEUINT8(save_p, (UINT8)ht->ticbased);
	WRITEINT32(save_p, ht->duration);
	WRITEINT32(save_p, ht->timer);
}

static void P_NetArchiveThinkers(void)
{
	const thinker_t *th;
	UINT32 i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_THINKERS);

	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		UINT32 numsaved = 0;
		// save off the current thinkers
		for (th = thlist[i].next; th != &thlist[i]; th = th->next)
		{
			if (!(th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed
			 || th->function.acp1 == (actionf_p1)P_NullPrecipThinker))
				numsaved++;

			if (th->function.acp1 == (actionf_p1)P_MobjThinker)
			{
				SaveMobjThinker(th, tc_mobj);
				continue;
			}
	#ifdef PARANOIA
			else if (th->function.acp1 == (actionf_p1)P_NullPrecipThinker);
	#endif
			else if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
			{
				SaveCeilingThinker(th, tc_ceiling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_CrushCeiling)
			{
				SaveCeilingThinker(th, tc_crushceiling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MoveFloor)
			{
				SaveFloormoveThinker(th, tc_floor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LightningFlash)
			{
				SaveLightflashThinker(th, tc_flash);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
			{
				SaveStrobeThinker(th, tc_strobe);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Glow)
			{
				SaveGlowThinker(th, tc_glow);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FireFlicker)
			{
				SaveFireflickerThinker(th, tc_fireflicker);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MoveElevator)
			{
				SaveElevatorThinker(th, tc_elevator);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ContinuousFalling)
			{
				SaveContinuousFallThinker(th, tc_continuousfalling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ThwompSector)
			{
				SaveThwompThinker(th, tc_thwomp);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_NoEnemiesSector)
			{
				SaveNoEnemiesThinker(th, tc_noenemies);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_EachTimeThinker)
			{
				SaveEachTimeThinker(th, tc_eachtime);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_RaiseSector)
			{
				SaveRaiseThinker(th, tc_raisesector);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_CameraScanner)
			{
				SaveElevatorThinker(th, tc_camerascanner);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Scroll)
			{
				SaveScrollThinker(th, tc_scroll);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Friction)
			{
				SaveFrictionThinker(th, tc_friction);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Pusher)
			{
				SavePusherThinker(th, tc_pusher);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_BounceCheese)
			{
				SaveBounceCheeseThinker(th, tc_bouncecheese);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_StartCrumble)
			{
				SaveCrumbleThinker(th, tc_startcrumble);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MarioBlock)
			{
				SaveMarioBlockThinker(th, tc_marioblock);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MarioBlockChecker)
			{
				SaveMarioCheckThinker(th, tc_marioblockchecker);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FloatSector)
			{
				SaveFloatThinker(th, tc_floatsector);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LaserFlash)
			{
				SaveLaserThinker(th, tc_laserflash);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LightFade)
			{
				SaveLightlevelThinker(th, tc_lightfade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ExecutorDelay)
			{
				SaveExecutorThinker(th, tc_executor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Disappear)
			{
				SaveDisappearThinker(th, tc_disappear);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Fade)
			{
				SaveFadeThinker(th, tc_fade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FadeColormap)
			{
				SaveFadeColormapThinker(th, tc_fadecolormap);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PlaneDisplace)
			{
				SavePlaneDisplaceThinker(th, tc_planedisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjRotate)
			{
				SavePolyrotatetThinker(th, tc_polyrotate);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjMove)
			{
				SavePolymoveThinker(th, tc_polymove);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjWaypoint)
			{
				SavePolywaypointThinker(th, tc_polywaypoint);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyDoorSlide)
			{
				SavePolyslidedoorThinker(th, tc_polyslidedoor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyDoorSwing)
			{
				SavePolyswingdoorThinker(th, tc_polyswingdoor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjFlag)
			{
				SavePolymoveThinker(th, tc_polyflag);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjDisplace)
			{
				SavePolydisplaceThinker(th, tc_polydisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjRotDisplace)
			{
				SavePolyrotdisplaceThinker(th, tc_polyrotdisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjFade)
			{
				SavePolyfadeThinker(th, tc_polyfade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_DynamicSlopeLine)
			{
				SaveDynamicLineSlopeThinker(th, tc_dynslopeline);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_DynamicSlopeVert)
			{
				SaveDynamicVertexSlopeThinker(th, tc_dynslopevert);
				continue;
			}
#ifdef PARANOIA
			else if (th->function.acp1 != (actionf_p1)P_RemoveThinkerDelayed) // wait garbage collection
				I_Error("unknown thinker type %p", th->function.acp1);
#endif
		}

		CONS_Debug(DBG_NETPLAY, "%u thinkers saved in list %d\n", numsaved, i);

		WRITEUINT8(save_p, tc_end);
	}
}

static void P_NetArchiveWaypoints(void)
{
	waypoint_t *waypoint;
	size_t i;
	size_t numWaypoints = K_GetNumWaypoints();

	WRITEUINT32(save_p, ARCHIVEBLOCK_WAYPOINTS);
	WRITEUINT32(save_p, numWaypoints);

	for (i = 0U; i < numWaypoints; i++) {
		waypoint = K_GetWaypointFromIndex(i);

		// The only thing we save for each waypoint is the mobj.
		// Waypoints should NEVER be completely created or destroyed mid-race as a result of this
		WRITEUINT32(save_p, waypoint->mobj->mobjnum);
	}
}

static void P_NetUnArchiveWaypoints(void)
{
	if (READUINT32(save_p) != ARCHIVEBLOCK_WAYPOINTS)
		I_Error("Bad $$$.sav at archive block Waypoints!");
	else {
		UINT32 numArchiveWaypoints = READUINT32(save_p);
		size_t numSpawnedWaypoints = K_GetNumWaypoints();

		if (numArchiveWaypoints != numSpawnedWaypoints) {
			I_Error("Bad $$$.sav: More saved waypoints than created!");
		} else {
			waypoint_t *waypoint;
			UINT32 i;
			UINT32 temp;
			for (i = 0U; i < numArchiveWaypoints; i++) {
				waypoint = K_GetWaypointFromIndex(i);
				temp = READUINT32(save_p);
				if (!P_SetTarget(&waypoint->mobj, P_FindNewPosition(temp))) {
					CONS_Debug(DBG_GAMELOGIC, "waypoint mobj not found for %d\n", i);
				}
			}
		}
	}
}

// Now save the pointers, tracer and target, but at load time we must
// relink to this; the savegame contains the old position in the pointer
// field copyed in the info field temporarily, but finally we just search
// for the old position and relink to it.
mobj_t *P_FindNewPosition(UINT32 oldposition)
{
	thinker_t *th;
	mobj_t *mobj;

	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)th;
		if (mobj->mobjnum != oldposition)
			continue;

		return mobj;
	}
	CONS_Debug(DBG_GAMELOGIC, "mobj not found\n");
	return NULL;
}

static inline mobj_t *LoadMobj(UINT32 mobjnum)
{
	if (mobjnum == 0) return NULL;
	return (mobj_t *)(size_t)mobjnum;
}

static sector_t *LoadSector(UINT32 sector)
{
	if (sector >= numsectors) return NULL;
	return &sectors[sector];
}

static line_t *LoadLine(UINT32 line)
{
	if (line >= numlines) return NULL;
	return &lines[line];
}

static inline player_t *LoadPlayer(UINT32 player)
{
	if (player >= MAXPLAYERS) return NULL;
	return &players[player];
}

static inline pslope_t *LoadSlope(UINT32 slopeid)
{
	pslope_t *p = slopelist;
	if (slopeid > slopecount) return NULL;
	do
	{
		if (p->id == slopeid)
			return p;
	} while ((p = p->next));
	return NULL;
}

static thinker_t* LoadMobjThinker(actionf_p1 thinker)
{
	thinker_t *next;
	mobj_t *mobj;
	UINT32 diff;
	UINT32 diff2;
	INT32 i;
	fixed_t z, floorz, ceilingz;
	ffloor_t *floorrover = NULL, *ceilingrover = NULL;

	diff = READUINT32(save_p);
	if (diff & MD_MORE)
		diff2 = READUINT32(save_p);
	else
		diff2 = 0;

	next = (void *)(size_t)READUINT32(save_p);

	z = READFIXED(save_p); // Force this so 3dfloor problems don't arise.
	floorz = READFIXED(save_p);
	ceilingz = READFIXED(save_p);

	if (diff2 & MD2_FLOORROVER)
	{
		sector_t *sec = LoadSector(READUINT32(save_p));
		UINT16 id = READUINT16(save_p);
		floorrover = P_GetFFloorByID(sec, id);
	}

	if (diff2 & MD2_CEILINGROVER)
	{
		sector_t *sec = LoadSector(READUINT32(save_p));
		UINT16 id = READUINT16(save_p);
		ceilingrover = P_GetFFloorByID(sec, id);
	}

	if (diff & MD_SPAWNPOINT)
	{
		UINT16 spawnpointnum = READUINT16(save_p);

		if (mapthings[spawnpointnum].type == 1713) // NiGHTS Hoop special case
		{
			P_SpawnHoop(&mapthings[spawnpointnum]);
			return NULL;
		}

		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

		mobj->spawnpoint = &mapthings[spawnpointnum];
		mapthings[spawnpointnum].mobj = mobj;
	}
	else
		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

	// declare this as a valid mobj as soon as possible.
	mobj->thinker.function.acp1 = thinker;

	mobj->z = z;
	mobj->floorz = floorz;
	mobj->ceilingz = ceilingz;
	mobj->floorrover = floorrover;
	mobj->ceilingrover = ceilingrover;

	if (diff & MD_TYPE)
		mobj->type = READUINT32(save_p);
	else
	{
		for (i = 0; i < NUMMOBJTYPES; i++)
			if (mobj->spawnpoint && mobj->spawnpoint->type == mobjinfo[i].doomednum)
				break;
		if (i == NUMMOBJTYPES)
		{
			if (mobj->spawnpoint)
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing type %d\n", mobj->spawnpoint->type);
			else
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing type NULL\n");
			I_Error("Netsave corrupted");
		}
		mobj->type = i;
	}
	mobj->info = &mobjinfo[mobj->type];
	if (diff & MD_POS)
	{
		mobj->x = mobj->old_x = READFIXED(save_p);
		mobj->y = mobj->old_y = READFIXED(save_p);
		mobj->angle = mobj->old_angle = READANGLE(save_p);
		mobj->pitch = mobj->old_pitch = READANGLE(save_p);
		mobj->roll = mobj->old_roll = READANGLE(save_p);
	}
	else
	{
		mobj->x = mobj->old_x = mobj->spawnpoint->x << FRACBITS;
		mobj->y = mobj->old_y = mobj->spawnpoint->y << FRACBITS;
		mobj->angle = mobj->old_angle = FixedAngle(mobj->spawnpoint->angle*FRACUNIT);
		mobj->pitch = mobj->old_pitch = FixedAngle(mobj->spawnpoint->pitch*FRACUNIT);
		mobj->roll = mobj->old_roll = FixedAngle(mobj->spawnpoint->roll*FRACUNIT);
	}
	if (diff & MD_MOM)
	{
		mobj->momx = READFIXED(save_p);
		mobj->momy = READFIXED(save_p);
		mobj->momz = READFIXED(save_p);
		mobj->pmomz = READFIXED(save_p);
	} // otherwise they're zero, and the memset took care of it

	if (diff & MD_RADIUS)
		mobj->radius = READFIXED(save_p);
	else
		mobj->radius = mobj->info->radius;
	if (diff & MD_HEIGHT)
		mobj->height = READFIXED(save_p);
	else
		mobj->height = mobj->info->height;
	if (diff & MD_FLAGS)
		mobj->flags = READUINT32(save_p);
	else
		mobj->flags = mobj->info->flags;
	if (diff & MD_FLAGS2)
		mobj->flags2 = READUINT32(save_p);
	if (diff & MD_HEALTH)
		mobj->health = READINT32(save_p);
	else
		mobj->health = mobj->info->spawnhealth;
	if (diff & MD_RTIME)
		mobj->reactiontime = READINT32(save_p);
	else
		mobj->reactiontime = mobj->info->reactiontime;

	if (diff & MD_STATE)
		mobj->state = &states[READUINT16(save_p)];
	else
		mobj->state = &states[mobj->info->spawnstate];
	if (diff & MD_TICS)
		mobj->tics = READINT32(save_p);
	else
		mobj->tics = mobj->state->tics;
	if (diff & MD_SPRITE) {
		mobj->sprite = READUINT16(save_p);
		if (mobj->sprite == SPR_PLAY)
			mobj->sprite2 = READUINT8(save_p);
	}
	else {
		mobj->sprite = mobj->state->sprite;
		if (mobj->sprite == SPR_PLAY)
			mobj->sprite2 = mobj->state->frame&FF_FRAMEMASK;
	}
	if (diff & MD_FRAME)
	{
		mobj->frame = READUINT32(save_p);
		mobj->anim_duration = READUINT16(save_p);
	}
	else
	{
		mobj->frame = mobj->state->frame;
		mobj->anim_duration = (UINT16)mobj->state->var2;
	}
	if (diff & MD_EFLAGS)
		mobj->eflags = READUINT16(save_p);
	if (diff & MD_PLAYER)
	{
		i = READUINT8(save_p);
		mobj->player = &players[i];
		mobj->player->mo = mobj;
	}
	if (diff & MD_MOVEDIR)
		mobj->movedir = READANGLE(save_p);
	if (diff & MD_MOVECOUNT)
		mobj->movecount = READINT32(save_p);
	if (diff & MD_THRESHOLD)
		mobj->threshold = READINT32(save_p);
	if (diff & MD_LASTLOOK)
		mobj->lastlook = READINT32(save_p);
	else
		mobj->lastlook = -1;
	if (diff & MD_TARGET)
		mobj->target = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff & MD_TRACER)
		mobj->tracer = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff & MD_FRICTION)
		mobj->friction = READFIXED(save_p);
	else
		mobj->friction = ORIG_FRICTION;
	if (diff & MD_MOVEFACTOR)
		mobj->movefactor = READFIXED(save_p);
	else
		mobj->movefactor = FRACUNIT;
	if (diff & MD_FUSE)
		mobj->fuse = READINT32(save_p);
	if (diff & MD_WATERTOP)
		mobj->watertop = READFIXED(save_p);
	if (diff & MD_WATERBOTTOM)
		mobj->waterbottom = READFIXED(save_p);
	if (diff & MD_SCALE)
		mobj->scale = READFIXED(save_p);
	else
		mobj->scale = FRACUNIT;
	if (diff & MD_DSCALE)
		mobj->destscale = READFIXED(save_p);
	else
		mobj->destscale = mobj->scale;
	if (diff2 & MD2_SCALESPEED)
		mobj->scalespeed = READFIXED(save_p);
	else
		mobj->scalespeed = mapobjectscale/12;
	if (diff2 & MD2_CUSVAL)
		mobj->cusval = READINT32(save_p);
	if (diff2 & MD2_CVMEM)
		mobj->cvmem = READINT32(save_p);
	if (diff2 & MD2_SKIN)
		mobj->skin = &skins[READUINT8(save_p)];
	if (diff2 & MD2_COLOR)
		mobj->color = READUINT16(save_p);
	if (diff2 & MD2_EXTVAL1)
		mobj->extravalue1 = READINT32(save_p);
	if (diff2 & MD2_EXTVAL2)
		mobj->extravalue2 = READINT32(save_p);
	if (diff2 & MD2_HNEXT)
		mobj->hnext = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff2 & MD2_HPREV)
		mobj->hprev = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff2 & MD2_ITNEXT)
		mobj->itnext = (mobj_t *)(size_t)READUINT32(save_p);
	if (diff2 & MD2_SLOPE)
		mobj->standingslope = P_SlopeById(READUINT16(save_p));
	if (diff2 & MD2_COLORIZED)
		mobj->colorized = READUINT8(save_p);
	if (diff2 & MD2_MIRRORED)
		mobj->mirrored = READUINT8(save_p);
	if (diff2 & MD2_ROLLANGLE)
		mobj->rollangle = READANGLE(save_p);
	if (diff2 & MD2_SHADOWSCALE)
	{
		mobj->shadowscale = READFIXED(save_p);
		mobj->whiteshadow = READUINT8(save_p);
	}
	if (diff2 & MD2_RENDERFLAGS)
		mobj->renderflags = READUINT32(save_p);
	if (diff2 & MD2_SPRITEXSCALE)
		mobj->spritexscale = READFIXED(save_p);
	else
		mobj->spritexscale = FRACUNIT;
	if (diff2 & MD2_SPRITEYSCALE)
		mobj->spriteyscale = READFIXED(save_p);
	else
		mobj->spriteyscale = FRACUNIT;
	if (diff2 & MD2_SPRITEXOFFSET)
		mobj->spritexoffset = READFIXED(save_p);
	if (diff2 & MD2_SPRITEYOFFSET)
		mobj->spriteyoffset = READFIXED(save_p);
	if (diff2 & MD2_FLOORSPRITESLOPE)
	{
		pslope_t *slope = (pslope_t *)P_CreateFloorSpriteSlope(mobj);

		slope->zdelta = READFIXED(save_p);
		slope->zangle = READANGLE(save_p);
		slope->xydirection = READANGLE(save_p);

		slope->o.x = READFIXED(save_p);
		slope->o.y = READFIXED(save_p);
		slope->o.z = READFIXED(save_p);

		slope->d.x = READFIXED(save_p);
		slope->d.y = READFIXED(save_p);

		slope->normal.x = READFIXED(save_p);
		slope->normal.y = READFIXED(save_p);
		slope->normal.z = READFIXED(save_p);

		P_UpdateSlopeLightOffset(slope);
	}
	if (diff2 & MD2_DISPOFFSET)
	{
		mobj->dispoffset = READINT32(save_p);
	}
	if (diff2 & MD2_LASTMOMZ)
	{
		mobj->lastmomz = READINT32(save_p);
	}
	if (diff2 & MD2_TERRAIN)
	{
		mobj->terrain = (terrain_t *)(size_t)READUINT32(save_p);
		mobj->terrainOverlay = (mobj_t *)(size_t)READUINT32(save_p);
	}
	else
	{
		mobj->terrain = NULL;
	}

	if (diff & MD_REDFLAG)
	{
		redflag = mobj;
		rflagpoint = mobj->spawnpoint;
	}
	if (diff & MD_BLUEFLAG)
	{
		blueflag = mobj;
		bflagpoint = mobj->spawnpoint;
	}

	// set sprev, snext, bprev, bnext, subsector
	P_SetThingPosition(mobj);

	mobj->mobjnum = READUINT32(save_p);

	if (mobj->player)
	{
		if (mobj->eflags & MFE_VERTICALFLIP)
			mobj->player->viewz = mobj->z + mobj->height - mobj->player->viewheight;
		else
			mobj->player->viewz = mobj->player->mo->z + mobj->player->viewheight;
	}

	if (mobj->type == MT_SKYBOX)
	{
		mtag_t tag = mobj->movedir;
		if (tag < 0 || tag > 15)
		{
			CONS_Debug(DBG_GAMELOGIC, "LoadMobjThinker: Skybox ID %d of netloaded object is not between 0 and 15!\n", tag);
		}
		else if (mobj->flags2 & MF2_AMBUSH)
			skyboxcenterpnts[tag] = mobj;
		else
			skyboxviewpnts[tag] = mobj;
	}

	if (diff2 & MD2_WAYPOINTCAP)
		P_SetTarget(&waypointcap, mobj);

	if (diff2 & MD2_KITEMCAP)
		P_SetTarget(&kitemcap, mobj);

	mobj->info = (mobjinfo_t *)next; // temporarily, set when leave this function

	R_AddMobjInterpolator(mobj);

	return &mobj->thinker;
}

static thinker_t* LoadNoEnemiesThinker(actionf_p1 thinker)
{
	noenemies_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	return &ht->thinker;
}

static thinker_t* LoadBounceCheeseThinker(actionf_p1 thinker)
{
	bouncecheese_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->speed = READFIXED(save_p);
	ht->distance = READFIXED(save_p);
	ht->floorwasheight = READFIXED(save_p);
	ht->ceilingwasheight = READFIXED(save_p);
	ht->low = READCHAR(save_p);

	if (ht->sector)
		ht->sector->ceilingdata = ht;

	return &ht->thinker;
}

static thinker_t* LoadContinuousFallThinker(actionf_p1 thinker)
{
	continuousfall_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->speed = READFIXED(save_p);
	ht->direction = READINT32(save_p);
	ht->floorstartheight = READFIXED(save_p);
	ht->ceilingstartheight = READFIXED(save_p);
	ht->destheight = READFIXED(save_p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadMarioBlockThinker(actionf_p1 thinker)
{
	mariothink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->speed = READFIXED(save_p);
	ht->direction = READINT32(save_p);
	ht->floorstartheight = READFIXED(save_p);
	ht->ceilingstartheight = READFIXED(save_p);
	ht->tag = READINT16(save_p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadMarioCheckThinker(actionf_p1 thinker)
{
	mariocheck_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	return &ht->thinker;
}

static thinker_t* LoadThwompThinker(actionf_p1 thinker)
{
	thwomp_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->crushspeed = READFIXED(save_p);
	ht->retractspeed = READFIXED(save_p);
	ht->direction = READINT32(save_p);
	ht->floorstartheight = READFIXED(save_p);
	ht->ceilingstartheight = READFIXED(save_p);
	ht->delay = READINT32(save_p);
	ht->tag = READINT16(save_p);
	ht->sound = READUINT16(save_p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadFloatThinker(actionf_p1 thinker)
{
	floatthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->tag = READINT16(save_p);
	return &ht->thinker;
}

static thinker_t* LoadEachTimeThinker(actionf_p1 thinker)
{
	size_t i;
	eachtime_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		ht->playersInArea[i] = READCHAR(save_p);
	}
	ht->triggerOnExit = READCHAR(save_p);
	return &ht->thinker;
}

static thinker_t* LoadRaiseThinker(actionf_p1 thinker)
{
	raise_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->tag = READINT16(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->ceilingbottom = READFIXED(save_p);
	ht->ceilingtop = READFIXED(save_p);
	ht->basespeed = READFIXED(save_p);
	ht->extraspeed = READFIXED(save_p);
	ht->shaketimer = READUINT8(save_p);
	ht->flags = READUINT8(save_p);
	return &ht->thinker;
}

static thinker_t* LoadCeilingThinker(actionf_p1 thinker)
{
	ceiling_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->bottomheight = READFIXED(save_p);
	ht->topheight = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->delay = READFIXED(save_p);
	ht->delaytimer = READFIXED(save_p);
	ht->crush = READUINT8(save_p);
	ht->texture = READINT32(save_p);
	ht->direction = READINT32(save_p);
	ht->tag = READINT16(save_p);
	ht->origspeed = READFIXED(save_p);
	ht->sourceline = READFIXED(save_p);
	if (ht->sector)
		ht->sector->ceilingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadFloormoveThinker(actionf_p1 thinker)
{
	floormove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->crush = READUINT8(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->direction = READINT32(save_p);
	ht->texture = READINT32(save_p);
	ht->floordestheight = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->origspeed = READFIXED(save_p);
	ht->delay = READFIXED(save_p);
	ht->delaytimer = READFIXED(save_p);
	ht->tag = READINT16(save_p);
	ht->sourceline = READFIXED(save_p);
	if (ht->sector)
		ht->sector->floordata = ht;
	return &ht->thinker;
}

static thinker_t* LoadLightflashThinker(actionf_p1 thinker)
{
	lightflash_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->maxlight = READINT32(save_p);
	ht->minlight = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadStrobeThinker(actionf_p1 thinker)
{
	strobe_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->count = READINT32(save_p);
	ht->minlight = READINT16(save_p);
	ht->maxlight = READINT16(save_p);
	ht->darktime = READINT32(save_p);
	ht->brighttime = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadGlowThinker(actionf_p1 thinker)
{
	glow_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->minlight = READINT16(save_p);
	ht->maxlight = READINT16(save_p);
	ht->direction = READINT16(save_p);
	ht->speed = READINT16(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadFireflickerThinker(actionf_p1 thinker)
{
	fireflicker_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->count = READINT32(save_p);
	ht->resetcount = READINT32(save_p);
	ht->maxlight = READINT16(save_p);
	ht->minlight = READINT16(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadElevatorThinker(actionf_p1 thinker, boolean setplanedata)
{
	elevator_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->sector = LoadSector(READUINT32(save_p));
	ht->actionsector = LoadSector(READUINT32(save_p));
	ht->direction = READINT32(save_p);
	ht->floordestheight = READFIXED(save_p);
	ht->ceilingdestheight = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->origspeed = READFIXED(save_p);
	ht->low = READFIXED(save_p);
	ht->high = READFIXED(save_p);
	ht->distance = READFIXED(save_p);
	ht->delay = READFIXED(save_p);
	ht->delaytimer = READFIXED(save_p);
	ht->floorwasheight = READFIXED(save_p);
	ht->ceilingwasheight = READFIXED(save_p);
	ht->sourceline = LoadLine(READUINT32(save_p));

	if (ht->sector && setplanedata)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadCrumbleThinker(actionf_p1 thinker)
{
	crumble_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->actionsector = LoadSector(READUINT32(save_p));
	ht->player = LoadPlayer(READUINT32(save_p));
	ht->direction = READINT32(save_p);
	ht->origalpha = READINT32(save_p);
	ht->timer = READINT32(save_p);
	ht->speed = READFIXED(save_p);
	ht->floorwasheight = READFIXED(save_p);
	ht->ceilingwasheight = READFIXED(save_p);
	ht->flags = READUINT8(save_p);

	if (ht->sector)
		ht->sector->floordata = ht;

	return &ht->thinker;
}

static thinker_t* LoadScrollThinker(actionf_p1 thinker)
{
	scroll_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dx = READFIXED(save_p);
	ht->dy = READFIXED(save_p);
	ht->affectee = READINT32(save_p);
	ht->control = READINT32(save_p);
	ht->last_height = READFIXED(save_p);
	ht->vdx = READFIXED(save_p);
	ht->vdy = READFIXED(save_p);
	ht->accel = READINT32(save_p);
	ht->exclusive = READINT32(save_p);
	ht->type = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadFrictionThinker(actionf_p1 thinker)
{
	friction_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->friction = READINT32(save_p);
	ht->movefactor = READINT32(save_p);
	ht->affectee = READINT32(save_p);
	ht->referrer = READINT32(save_p);
	ht->roverfriction = READUINT8(save_p);
	return &ht->thinker;
}

static thinker_t* LoadPusherThinker(actionf_p1 thinker)
{
	pusher_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save_p);
	ht->x_mag = READFIXED(save_p);
	ht->y_mag = READFIXED(save_p);
	ht->z_mag = READFIXED(save_p);
	ht->affectee = READINT32(save_p);
	ht->roverpusher = READUINT8(save_p);
	ht->referrer = READINT32(save_p);
	ht->exclusive = READINT32(save_p);
	ht->slider = READINT32(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadLaserThinker(actionf_p1 thinker)
{
	laserthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->tag = READINT16(save_p);
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->nobosses = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadLightlevelThinker(actionf_p1 thinker)
{
	lightlevel_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->sourcelevel = READINT16(save_p);
	ht->destlevel = READINT16(save_p);
	ht->fixedcurlevel = READFIXED(save_p);
	ht->fixedpertic = READFIXED(save_p);
	ht->timer = READINT32(save_p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static inline thinker_t* LoadExecutorThinker(actionf_p1 thinker)
{
	executor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->line = LoadLine(READUINT32(save_p));
	ht->caller = LoadMobj(READUINT32(save_p));
	ht->sector = LoadSector(READUINT32(save_p));
	ht->timer = READINT32(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadDisappearThinker(actionf_p1 thinker)
{
	disappear_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->appeartime = READUINT32(save_p);
	ht->disappeartime = READUINT32(save_p);
	ht->offset = READUINT32(save_p);
	ht->timer = READUINT32(save_p);
	ht->affectee = READINT32(save_p);
	ht->sourceline = READINT32(save_p);
	ht->exists = READINT32(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadFadeThinker(actionf_p1 thinker)
{
	sector_t *ss;
	fade_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dest_exc = GetNetColormapFromList(READUINT32(save_p));
	ht->sectornum = READUINT32(save_p);
	ht->ffloornum = READUINT32(save_p);
	ht->alpha = READINT32(save_p);
	ht->sourcevalue = READINT16(save_p);
	ht->destvalue = READINT16(save_p);
	ht->destlightlevel = READINT16(save_p);
	ht->speed = READINT16(save_p);
	ht->ticbased = (boolean)READUINT8(save_p);
	ht->timer = READINT32(save_p);
	ht->doexists = READUINT8(save_p);
	ht->dotranslucent = READUINT8(save_p);
	ht->dolighting = READUINT8(save_p);
	ht->docolormap = READUINT8(save_p);
	ht->docollision = READUINT8(save_p);
	ht->doghostfade = READUINT8(save_p);
	ht->exactalpha = READUINT8(save_p);

	ss = LoadSector(ht->sectornum);
	if (ss)
	{
		size_t j = 0; // ss->ffloors is saved as ffloor #0, ss->ffloors->next is #1, etc
		ffloor_t *rover;
		for (rover = ss->ffloors; rover; rover = rover->next)
		{
			if (j == ht->ffloornum)
			{
				ht->rover = rover;
				rover->fadingdata = ht;
				break;
			}
			j++;
		}
	}
	return &ht->thinker;
}

static inline thinker_t* LoadFadeColormapThinker(actionf_p1 thinker)
{
	fadecolormap_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save_p));
	ht->source_exc = GetNetColormapFromList(READUINT32(save_p));
	ht->dest_exc = GetNetColormapFromList(READUINT32(save_p));
	ht->ticbased = (boolean)READUINT8(save_p);
	ht->duration = READINT32(save_p);
	ht->timer = READINT32(save_p);
	if (ht->sector)
		ht->sector->fadecolormapdata = ht;
	return &ht->thinker;
}

static inline thinker_t* LoadPlaneDisplaceThinker(actionf_p1 thinker)
{
	planedisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->affectee = READINT32(save_p);
	ht->control = READINT32(save_p);
	ht->last_height = READFIXED(save_p);
	ht->speed = READFIXED(save_p);
	ht->type = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadDynamicLineSlopeThinker(actionf_p1 thinker)
{
	dynlineplanethink_t* ht = Z_Malloc(sizeof(*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->type = READUINT8(save_p);
	ht->slope = LoadSlope(READUINT32(save_p));
	ht->sourceline = LoadLine(READUINT32(save_p));
	ht->extent = READFIXED(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadDynamicVertexSlopeThinker(actionf_p1 thinker)
{
	size_t i;
	dynvertexplanethink_t* ht = Z_Malloc(sizeof(*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->slope = LoadSlope(READUINT32(save_p));
	for (i = 0; i < 3; i++)
		ht->secs[i] = LoadSector(READUINT32(save_p));
	READMEM(save_p, ht->vex, sizeof(ht->vex));
	READMEM(save_p, ht->origsecheights, sizeof(ht->origsecheights));
	READMEM(save_p, ht->origvecheights, sizeof(ht->origvecheights));
	ht->relative = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyrotatetThinker(actionf_p1 thinker)
{
	polyrotate_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->distance = READINT32(save_p);
	ht->turnobjs = READUINT8(save_p);
	return &ht->thinker;
}

static thinker_t* LoadPolymoveThinker(actionf_p1 thinker)
{
	polymove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->momx = READFIXED(save_p);
	ht->momy = READFIXED(save_p);
	ht->distance = READINT32(save_p);
	ht->angle = READANGLE(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolywaypointThinker(actionf_p1 thinker)
{
	polywaypoint_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->sequence = READINT32(save_p);
	ht->pointnum = READINT32(save_p);
	ht->direction = READINT32(save_p);
	ht->returnbehavior = READUINT8(save_p);
	ht->continuous = READUINT8(save_p);
	ht->stophere = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyslidedoorThinker(actionf_p1 thinker)
{
	polyslidedoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->delay = READINT32(save_p);
	ht->delayCount = READINT32(save_p);
	ht->initSpeed = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->initDistance = READINT32(save_p);
	ht->distance = READINT32(save_p);
	ht->initAngle = READUINT32(save_p);
	ht->angle = READUINT32(save_p);
	ht->revAngle = READUINT32(save_p);
	ht->momx = READFIXED(save_p);
	ht->momy = READFIXED(save_p);
	ht->closing = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyswingdoorThinker(actionf_p1 thinker)
{
	polyswingdoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->delay = READINT32(save_p);
	ht->delayCount = READINT32(save_p);
	ht->initSpeed = READINT32(save_p);
	ht->speed = READINT32(save_p);
	ht->initDistance = READINT32(save_p);
	ht->distance = READINT32(save_p);
	ht->closing = READUINT8(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolydisplaceThinker(actionf_p1 thinker)
{
	polydisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->controlSector = LoadSector(READUINT32(save_p));
	ht->dx = READFIXED(save_p);
	ht->dy = READFIXED(save_p);
	ht->oldHeights = READFIXED(save_p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyrotdisplaceThinker(actionf_p1 thinker)
{
	polyrotdisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->controlSector = LoadSector(READUINT32(save_p));
	ht->rotscale = READFIXED(save_p);
	ht->turnobjs = READUINT8(save_p);
	ht->oldHeights = READFIXED(save_p);
	return &ht->thinker;
}

static thinker_t* LoadPolyfadeThinker(actionf_p1 thinker)
{
	polyfade_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save_p);
	ht->sourcevalue = READINT32(save_p);
	ht->destvalue = READINT32(save_p);
	ht->docollision = (boolean)READUINT8(save_p);
	ht->doghostfade = (boolean)READUINT8(save_p);
	ht->ticbased = (boolean)READUINT8(save_p);
	ht->duration = READINT32(save_p);
	ht->timer = READINT32(save_p);
	return &ht->thinker;
}

static void P_NetUnArchiveThinkers(void)
{
	thinker_t *currentthinker;
	thinker_t *next;
	UINT8 tclass;
	UINT8 restoreNum = false;
	UINT32 i;
	UINT32 numloaded = 0;

	if (READUINT32(save_p) != ARCHIVEBLOCK_THINKERS)
		I_Error("Bad $$$.sav at archive block Thinkers");

	// remove all the current thinkers
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		currentthinker = thlist[i].next;
		for (currentthinker = thlist[i].next; currentthinker != &thlist[i]; currentthinker = next)
		{
			next = currentthinker->next;

			if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker || currentthinker->function.acp1 == (actionf_p1)P_NullPrecipThinker)
				P_RemoveSavegameMobj((mobj_t *)currentthinker); // item isn't saved, don't remove it
			else
			{
				(next->prev = currentthinker->prev)->next = next;
				R_DestroyLevelInterpolators(currentthinker);
				Z_Free(currentthinker);
			}
		}
	}

	// we don't want the removed mobjs to come back
	iquetail = iquehead = 0;
	P_InitThinkers();

	// clear sector thinker pointers so they don't point to non-existant thinkers for all of eternity
	for (i = 0; i < numsectors; i++)
	{
		sectors[i].floordata = sectors[i].ceilingdata = sectors[i].lightingdata = sectors[i].fadecolormapdata = NULL;
	}

	// read in saved thinkers
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		for (;;)
		{
			thinker_t* th = NULL;
			tclass = READUINT8(save_p);

			if (tclass == tc_end)
				break; // leave the saved thinker reading loop
			numloaded++;

			switch (tclass)
			{
				case tc_mobj:
					th = LoadMobjThinker((actionf_p1)P_MobjThinker);
					break;

				case tc_ceiling:
					th = LoadCeilingThinker((actionf_p1)T_MoveCeiling);
					break;

				case tc_crushceiling:
					th = LoadCeilingThinker((actionf_p1)T_CrushCeiling);
					break;

				case tc_floor:
					th = LoadFloormoveThinker((actionf_p1)T_MoveFloor);
					break;

				case tc_flash:
					th = LoadLightflashThinker((actionf_p1)T_LightningFlash);
					break;

				case tc_strobe:
					th = LoadStrobeThinker((actionf_p1)T_StrobeFlash);
					break;

				case tc_glow:
					th = LoadGlowThinker((actionf_p1)T_Glow);
					break;

				case tc_fireflicker:
					th = LoadFireflickerThinker((actionf_p1)T_FireFlicker);
					break;

				case tc_elevator:
					th = LoadElevatorThinker((actionf_p1)T_MoveElevator, true);
					break;

				case tc_continuousfalling:
					th = LoadContinuousFallThinker((actionf_p1)T_ContinuousFalling);
					break;

				case tc_thwomp:
					th = LoadThwompThinker((actionf_p1)T_ThwompSector);
					break;

				case tc_noenemies:
					th = LoadNoEnemiesThinker((actionf_p1)T_NoEnemiesSector);
					break;

				case tc_eachtime:
					th = LoadEachTimeThinker((actionf_p1)T_EachTimeThinker);
					break;

				case tc_raisesector:
					th = LoadRaiseThinker((actionf_p1)T_RaiseSector);
					break;

				case tc_camerascanner:
					th = LoadElevatorThinker((actionf_p1)T_CameraScanner, false);
					break;

				case tc_bouncecheese:
					th = LoadBounceCheeseThinker((actionf_p1)T_BounceCheese);
					break;

				case tc_startcrumble:
					th = LoadCrumbleThinker((actionf_p1)T_StartCrumble);
					break;

				case tc_marioblock:
					th = LoadMarioBlockThinker((actionf_p1)T_MarioBlock);
					break;

				case tc_marioblockchecker:
					th = LoadMarioCheckThinker((actionf_p1)T_MarioBlockChecker);
					break;

				case tc_floatsector:
					th = LoadFloatThinker((actionf_p1)T_FloatSector);
					break;

				case tc_laserflash:
					th = LoadLaserThinker((actionf_p1)T_LaserFlash);
					break;

				case tc_lightfade:
					th = LoadLightlevelThinker((actionf_p1)T_LightFade);
					break;

				case tc_executor:
					th = LoadExecutorThinker((actionf_p1)T_ExecutorDelay);
					restoreNum = true;
					break;

				case tc_disappear:
					th = LoadDisappearThinker((actionf_p1)T_Disappear);
					break;

				case tc_fade:
					th = LoadFadeThinker((actionf_p1)T_Fade);
					break;

				case tc_fadecolormap:
					th = LoadFadeColormapThinker((actionf_p1)T_FadeColormap);
					break;

				case tc_planedisplace:
					th = LoadPlaneDisplaceThinker((actionf_p1)T_PlaneDisplace);
					break;
				case tc_polyrotate:
					th = LoadPolyrotatetThinker((actionf_p1)T_PolyObjRotate);
					break;

				case tc_polymove:
					th = LoadPolymoveThinker((actionf_p1)T_PolyObjMove);
					break;

				case tc_polywaypoint:
					th = LoadPolywaypointThinker((actionf_p1)T_PolyObjWaypoint);
					break;

				case tc_polyslidedoor:
					th = LoadPolyslidedoorThinker((actionf_p1)T_PolyDoorSlide);
					break;

				case tc_polyswingdoor:
					th = LoadPolyswingdoorThinker((actionf_p1)T_PolyDoorSwing);
					break;

				case tc_polyflag:
					th = LoadPolymoveThinker((actionf_p1)T_PolyObjFlag);
					break;

				case tc_polydisplace:
					th = LoadPolydisplaceThinker((actionf_p1)T_PolyObjDisplace);
					break;

				case tc_polyrotdisplace:
					th = LoadPolyrotdisplaceThinker((actionf_p1)T_PolyObjRotDisplace);
					break;

				case tc_polyfade:
					th = LoadPolyfadeThinker((actionf_p1)T_PolyObjFade);
					break;

				case tc_dynslopeline:
					th = LoadDynamicLineSlopeThinker((actionf_p1)T_DynamicSlopeLine);
					break;

				case tc_dynslopevert:
					th = LoadDynamicVertexSlopeThinker((actionf_p1)T_DynamicSlopeVert);
					break;

				case tc_scroll:
					th = LoadScrollThinker((actionf_p1)T_Scroll);
					break;

				case tc_friction:
					th = LoadFrictionThinker((actionf_p1)T_Friction);
					break;

				case tc_pusher:
					th = LoadPusherThinker((actionf_p1)T_Pusher);
					break;

				default:
					I_Error("P_UnarchiveSpecials: Unknown tclass %d in savegame", tclass);
			}
			if (th)
				P_AddThinker(i, th);
		}

		CONS_Debug(DBG_NETPLAY, "%u thinkers loaded in list %d\n", numloaded, i);
	}

	if (restoreNum)
	{
		executor_t *delay = NULL;
		UINT32 mobjnum;
		for (currentthinker = thlist[THINK_MAIN].next; currentthinker != &thlist[THINK_MAIN]; currentthinker = currentthinker->next)
		{
			if (currentthinker->function.acp1 != (actionf_p1)T_ExecutorDelay)
				continue;
			delay = (void *)currentthinker;
			if (!(mobjnum = (UINT32)(size_t)delay->caller))
				continue;
			delay->caller = P_FindNewPosition(mobjnum);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// haleyjd 03/26/06: PolyObject saving code
//
#define PD_FLAGS  0x01
#define PD_TRANS   0x02

static inline void P_ArchivePolyObj(polyobj_t *po)
{
	UINT8 diff = 0;
	WRITEINT32(save_p, po->id);
	WRITEANGLE(save_p, po->angle);

	WRITEFIXED(save_p, po->spawnSpot.x);
	WRITEFIXED(save_p, po->spawnSpot.y);

	if (po->flags != po->spawnflags)
		diff |= PD_FLAGS;
	if (po->translucency != po->spawntrans)
		diff |= PD_TRANS;

	WRITEUINT8(save_p, diff);

	if (diff & PD_FLAGS)
		WRITEINT32(save_p, po->flags);
	if (diff & PD_TRANS)
		WRITEINT32(save_p, po->translucency);
}

static inline void P_UnArchivePolyObj(polyobj_t *po)
{
	INT32 id;
	UINT32 angle;
	fixed_t x, y;
	UINT8 diff;

	// nullify all polyobject thinker pointers;
	// the thinkers themselves will fight over who gets the field
	// when they first start to run.
	po->thinker = NULL;

	id = READINT32(save_p);

	angle = READANGLE(save_p);

	x = READFIXED(save_p);
	y = READFIXED(save_p);

	diff = READUINT8(save_p);

	if (diff & PD_FLAGS)
		po->flags = READINT32(save_p);
	if (diff & PD_TRANS)
		po->translucency = READINT32(save_p);

	// if the object is bad or isn't in the id hash, we can do nothing more
	// with it, so return now
	if (po->isBad || po != Polyobj_GetForNum(id))
		return;

	// rotate and translate polyobject
	Polyobj_MoveOnLoad(po, angle, x, y);
}

static inline void P_ArchivePolyObjects(void)
{
	INT32 i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_POBJS);

	// save number of polyobjects
	WRITEINT32(save_p, numPolyObjects);

	for (i = 0; i < numPolyObjects; ++i)
		P_ArchivePolyObj(&PolyObjects[i]);
}

static inline void P_UnArchivePolyObjects(void)
{
	INT32 i, numSavedPolys;

	if (READUINT32(save_p) != ARCHIVEBLOCK_POBJS)
		I_Error("Bad $$$.sav at archive block Pobjs");

	numSavedPolys = READINT32(save_p);

	if (numSavedPolys != numPolyObjects)
		I_Error("P_UnArchivePolyObjects: polyobj count inconsistency\n");

	for (i = 0; i < numSavedPolys; ++i)
		P_UnArchivePolyObj(&PolyObjects[i]);
}

static inline void P_FinishMobjs(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;

	// put info field there real value
	for (currentthinker = thlist[THINK_MOBJ].next; currentthinker != &thlist[THINK_MOBJ];
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)currentthinker;
		mobj->info = &mobjinfo[mobj->type];
	}
}

static void P_RelinkPointers(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;
	UINT32 temp;

	// use info field (value = oldposition) to relink mobjs
	for (currentthinker = thlist[THINK_MOBJ].next; currentthinker != &thlist[THINK_MOBJ];
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mobj = (mobj_t *)currentthinker;

		if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER
			// MT_SPARK: used for debug stuff
			|| mobj->type == MT_SPARK)
			continue;

		if (mobj->tracer)
		{
			temp = (UINT32)(size_t)mobj->tracer;
			mobj->tracer = NULL;
			if (!P_SetTarget(&mobj->tracer, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "tracer not found on %d\n", mobj->type);
		}
		if (mobj->target)
		{
			temp = (UINT32)(size_t)mobj->target;
			mobj->target = NULL;
			if (!P_SetTarget(&mobj->target, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "target not found on %d\n", mobj->type);
		}
		if (mobj->hnext)
		{
			temp = (UINT32)(size_t)mobj->hnext;
			mobj->hnext = NULL;
			if (!P_SetTarget(&mobj->hnext, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "hnext not found on %d\n", mobj->type);
		}
		if (mobj->hprev)
		{
			temp = (UINT32)(size_t)mobj->hprev;
			mobj->hprev = NULL;
			if (!P_SetTarget(&mobj->hprev, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "hprev not found on %d\n", mobj->type);
		}
		if (mobj->itnext)
		{
			temp = (UINT32)(size_t)mobj->itnext;
			mobj->itnext = NULL;
			if (!P_SetTarget(&mobj->itnext, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "itnext not found on %d\n", mobj->type);
		}
		if (mobj->terrain)
		{
			temp = (UINT32)(size_t)mobj->terrain;
			mobj->terrain = K_GetTerrainByIndex(temp);
			if (mobj->terrain == NULL)
			{
				CONS_Debug(DBG_GAMELOGIC, "terrain not found on %d\n", mobj->type);
			}
		}
		if (mobj->terrainOverlay)
		{
			temp = (UINT32)(size_t)mobj->terrainOverlay;
			mobj->terrainOverlay = NULL;
			if (!P_SetTarget(&mobj->terrainOverlay, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "terrainOverlay not found on %d\n", mobj->type);
		}
		if (mobj->player)
		{
			if ( mobj->player->skybox.viewpoint)
			{
				temp = (UINT32)(size_t)mobj->player->skybox.viewpoint;
				mobj->player->skybox.viewpoint = NULL;
				if (!P_SetTarget(&mobj->player->skybox.viewpoint, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "skybox.viewpoint not found on %d\n", mobj->type);
			}
			if ( mobj->player->skybox.centerpoint)
			{
				temp = (UINT32)(size_t)mobj->player->skybox.centerpoint;
				mobj->player->skybox.centerpoint = NULL;
				if (!P_SetTarget(&mobj->player->skybox.centerpoint, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "skybox.centerpoint not found on %d\n", mobj->type);
			}
			if ( mobj->player->awayviewmobj)
			{
				temp = (UINT32)(size_t)mobj->player->awayviewmobj;
				mobj->player->awayviewmobj = NULL;
				if (!P_SetTarget(&mobj->player->awayviewmobj, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "awayviewmobj not found on %d\n", mobj->type);
			}
			if (mobj->player->followmobj)
			{
				temp = (UINT32)(size_t)mobj->player->followmobj;
				mobj->player->followmobj = NULL;
				if (!P_SetTarget(&mobj->player->followmobj, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "followmobj not found on %d\n", mobj->type);
			}
			if (mobj->player->follower)
			{
				temp = (UINT32)(size_t)mobj->player->follower;
				mobj->player->follower = NULL;
				if (!P_SetTarget(&mobj->player->follower, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "follower not found on %d\n", mobj->type);
			}
			if (mobj->player->nextwaypoint)
			{
				temp = (UINT32)(size_t)mobj->player->nextwaypoint;
				mobj->player->nextwaypoint = K_GetWaypointFromIndex(temp);
				if (mobj->player->nextwaypoint == NULL)
				{
					CONS_Debug(DBG_GAMELOGIC, "nextwaypoint not found on %d\n", mobj->type);
				}
			}
		}
	}
}

static inline void P_NetArchiveSpecials(void)
{
	size_t i, z;

	WRITEUINT32(save_p, ARCHIVEBLOCK_SPECIALS);

	// itemrespawn queue for deathmatch
	i = iquetail;
	while (iquehead != i)
	{
		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] == itemrespawnque[i])
			{
				WRITEUINT32(save_p, z);
				break;
			}
		}
		WRITEUINT32(save_p, itemrespawntime[i]);
		i = (i + 1) & (ITEMQUESIZE-1);
	}

	// end delimiter
	WRITEUINT32(save_p, 0xffffffff);

	// Sky number
	WRITESTRINGN(save_p, globallevelskytexture, 9);

	// Current global weather type
	WRITEUINT8(save_p, globalweather);

	if (metalplayback) // Is metal sonic running?
	{
		WRITEUINT8(save_p, 0x01);
		G_SaveMetal(&save_p);
	}
	else
		WRITEUINT8(save_p, 0x00);
}

static void P_NetUnArchiveSpecials(void)
{
	char skytex[9];
	size_t i;

	if (READUINT32(save_p) != ARCHIVEBLOCK_SPECIALS)
		I_Error("Bad $$$.sav at archive block Specials");

	// BP: added save itemrespawn queue for deathmatch
	iquetail = iquehead = 0;
	while ((i = READUINT32(save_p)) != 0xffffffff)
	{
		itemrespawnque[iquehead] = &mapthings[i];
		itemrespawntime[iquehead++] = READINT32(save_p);
	}

	READSTRINGN(save_p, skytex, sizeof(skytex));
	if (strcmp(skytex, globallevelskytexture))
		P_SetupLevelSky(skytex, true);

	globalweather = READUINT8(save_p);

	if (globalweather)
	{
		if (curWeather == globalweather)
			curWeather = PRECIP_NONE;

		P_SwitchWeather(globalweather);
	}
	else // PRECIP_NONE
	{
		if (curWeather != PRECIP_NONE)
			P_SwitchWeather(globalweather);
	}

	if (READUINT8(save_p) == 0x01) // metal sonic
		G_LoadMetal(&save_p);
}

// =======================================================================
//          Misc
// =======================================================================
static inline void P_ArchiveMisc(INT16 mapnum)
{
	//lastmapsaved = mapnum;
	lastmaploaded = mapnum;

	if (gamecomplete)
		mapnum |= 8192;

	WRITEINT16(save_p, mapnum);
	WRITEUINT16(save_p, emeralds+357);
	WRITESTRINGN(save_p, timeattackfolder, sizeof(timeattackfolder));
}

static inline void P_UnArchiveSPGame(INT16 mapoverride)
{
	char testname[sizeof(timeattackfolder)];

	gamemap = READINT16(save_p);

	if (mapoverride != 0)
	{
		gamemap = mapoverride;
		gamecomplete = 1;
	}
	else
		gamecomplete = 0;

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	//lastmapsaved = gamemap;
	lastmaploaded = gamemap;

	tokenlist = 0;
	token = 0;

	savedata.emeralds = READUINT16(save_p)-357;

	READSTRINGN(save_p, testname, sizeof(testname));

	if (strcmp(testname, timeattackfolder))
	{
		if (modifiedgame)
			I_Error("Save game not for this modification.");
		else
			I_Error("This save file is for a particular mod, it cannot be used with the regular game.");
	}

	memset(playeringame, 0, sizeof(*playeringame));
	playeringame[consoleplayer] = true;
}

static void P_NetArchiveMisc(boolean resending)
{
	size_t i;

	WRITEUINT32(save_p, ARCHIVEBLOCK_MISC);

	if (resending)
		WRITEUINT32(save_p, gametic);
	WRITEINT16(save_p, gamemap);

	if (gamestate != GS_LEVEL)
		WRITEINT16(save_p, GS_WAITINGPLAYERS); // nice hack to put people back into waitingplayers
	else
		WRITEINT16(save_p, gamestate);
	WRITEINT16(save_p, gametype);

	{
		UINT32 pig = 0;
		for (i = 0; i < MAXPLAYERS; i++)
			pig |= (playeringame[i] != 0)<<i;
		WRITEUINT32(save_p, pig);
	}

	WRITEUINT32(save_p, P_GetRandSeed());

	WRITEUINT32(save_p, tokenlist);

	WRITEUINT8(save_p, encoremode);

	WRITEUINT32(save_p, leveltime);
	WRITEUINT32(save_p, ssspheres);
	WRITEINT16(save_p, lastmap);
	WRITEUINT16(save_p, bossdisabled);
	WRITEUINT8(save_p, ringsdisabled);

	for (i = 0; i < 4; i++)
	{
		WRITEINT16(save_p, votelevels[i][0]);
		WRITEINT16(save_p, votelevels[i][1]);
	}

	for (i = 0; i < MAXPLAYERS; i++)
		WRITESINT8(save_p, votes[i]);

	WRITESINT8(save_p, pickedvote);

	WRITEUINT16(save_p, emeralds);
	{
		UINT8 globools = 0;
		if (stagefailed)
			globools |= 1;
		if (stoppedclock)
			globools |= (1<<1);
		WRITEUINT8(save_p, globools);
	}

	WRITEUINT32(save_p, token);
	WRITEINT32(save_p, sstimer);
	WRITEUINT32(save_p, bluescore);
	WRITEUINT32(save_p, redscore);

	WRITEUINT16(save_p, skincolor_redteam);
	WRITEUINT16(save_p, skincolor_blueteam);
	WRITEUINT16(save_p, skincolor_redring);
	WRITEUINT16(save_p, skincolor_bluering);

	WRITEINT32(save_p, modulothing);

	WRITEINT16(save_p, autobalance);
	WRITEINT16(save_p, teamscramble);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save_p, scrambleplayers[i]);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save_p, scrambleteams[i]);

	WRITEINT16(save_p, scrambletotal);
	WRITEINT16(save_p, scramblecount);

	WRITEUINT32(save_p, racecountdown);
	WRITEUINT32(save_p, exitcountdown);

	WRITEFIXED(save_p, gravity);
	WRITEFIXED(save_p, mapobjectscale);

	WRITEUINT32(save_p, countdowntimer);
	WRITEUINT8(save_p, countdowntimeup);

	// SRB2kart
	WRITEINT32(save_p, numgotboxes);
	WRITEUINT8(save_p, numtargets);
	WRITEUINT8(save_p, battlecapsules);

	WRITEUINT8(save_p, gamespeed);
	WRITEUINT8(save_p, numlaps);
	WRITEUINT8(save_p, franticitems);
	WRITEUINT8(save_p, comeback);

	WRITESINT8(save_p, speedscramble);
	WRITESINT8(save_p, encorescramble);

	for (i = 0; i < 4; i++)
		WRITESINT8(save_p, battlewanted[i]);

	// battleovertime_t
	WRITEUINT16(save_p, battleovertime.enabled);
	WRITEFIXED(save_p, battleovertime.radius);
	WRITEFIXED(save_p, battleovertime.x);
	WRITEFIXED(save_p, battleovertime.y);
	WRITEFIXED(save_p, battleovertime.z);

	WRITEUINT32(save_p, wantedcalcdelay);
	WRITEUINT32(save_p, indirectitemcooldown);
	WRITEUINT32(save_p, mapreset);

	WRITEUINT8(save_p, spectateGriefed);

	WRITEUINT8(save_p, thwompsactive);
	WRITEUINT8(save_p, lastLowestLap);
	WRITESINT8(save_p, spbplace);
	WRITEUINT8(save_p, startedInFreePlay);

	WRITEUINT32(save_p, introtime);
	WRITEUINT32(save_p, starttime);

	WRITEUINT32(save_p, timelimitintics);
	WRITEUINT32(save_p, extratimeintics);
	WRITEUINT32(save_p, secretextratime);

	// Is it paused?
	if (paused)
		WRITEUINT8(save_p, 0x2f);
	else
		WRITEUINT8(save_p, 0x2e);

	// Only the server uses this, but it
	// needs synched for remote admins anyway.
	WRITEUINT32(save_p, schedule_len);
	for (i = 0; i < schedule_len; i++)
	{
		scheduleTask_t *task = schedule[i];
		WRITEINT16(save_p, task->basetime);
		WRITEINT16(save_p, task->timer);
		WRITESTRING(save_p, task->command);
	}
}

static inline boolean P_NetUnArchiveMisc(boolean reloading)
{
	size_t i;
	size_t numTasks;

	if (READUINT32(save_p) != ARCHIVEBLOCK_MISC)
		I_Error("Bad $$$.sav at archive block Misc");

	if (reloading)
		gametic = READUINT32(save_p);

	gamemap = READINT16(save_p);

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	// tell the sound code to reset the music since we're skipping what
	// normally sets this flag
	if (!reloading)
		mapmusflags |= MUSIC_RELOADRESET;

	G_SetGamestate(READINT16(save_p));

	gametype = READINT16(save_p);

	{
		UINT32 pig = READUINT32(save_p);
		for (i = 0; i < MAXPLAYERS; i++)
		{
			playeringame[i] = (pig & (1<<i)) != 0;
			// playerstate is set in unarchiveplayers
		}
	}

	P_SetRandSeed(READUINT32(save_p));

	tokenlist = READUINT32(save_p);

	encoremode = (boolean)READUINT8(save_p);

	if (!P_LoadLevel(true, reloading))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Can't load the level!\n"));
		return false;
	}

	// get the time
	leveltime = READUINT32(save_p);
	ssspheres = READUINT32(save_p);
	lastmap = READINT16(save_p);
	bossdisabled = READUINT16(save_p);
	ringsdisabled = READUINT8(save_p);

	for (i = 0; i < 4; i++)
	{
		votelevels[i][0] = READINT16(save_p);
		votelevels[i][1] = READINT16(save_p);
	}

	for (i = 0; i < MAXPLAYERS; i++)
		votes[i] = READSINT8(save_p);

	pickedvote = READSINT8(save_p);

	emeralds = READUINT16(save_p);
	{
		UINT8 globools = READUINT8(save_p);
		stagefailed = !!(globools & 1);
		stoppedclock = !!(globools & (1<<1));
	}

	token = READUINT32(save_p);
	sstimer = READINT32(save_p);
	bluescore = READUINT32(save_p);
	redscore = READUINT32(save_p);

	skincolor_redteam = READUINT16(save_p);
	skincolor_blueteam = READUINT16(save_p);
	skincolor_redring = READUINT16(save_p);
	skincolor_bluering = READUINT16(save_p);

	modulothing = READINT32(save_p);

	autobalance = READINT16(save_p);
	teamscramble = READINT16(save_p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleplayers[i] = READINT16(save_p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleteams[i] = READINT16(save_p);

	scrambletotal = READINT16(save_p);
	scramblecount = READINT16(save_p);

	racecountdown = READUINT32(save_p);
	exitcountdown = READUINT32(save_p);

	gravity = READFIXED(save_p);
	mapobjectscale = READFIXED(save_p);

	countdowntimer = (tic_t)READUINT32(save_p);
	countdowntimeup = (boolean)READUINT8(save_p);

	// SRB2kart
	numgotboxes = READINT32(save_p);
	numtargets = READUINT8(save_p);
	battlecapsules = (boolean)READUINT8(save_p);

	gamespeed = READUINT8(save_p);
	numlaps = READUINT8(save_p);
	franticitems = (boolean)READUINT8(save_p);
	comeback = (boolean)READUINT8(save_p);

	speedscramble = READSINT8(save_p);
	encorescramble = READSINT8(save_p);

	for (i = 0; i < 4; i++)
		battlewanted[i] = READSINT8(save_p);

	// battleovertime_t
	battleovertime.enabled = READUINT16(save_p);
	battleovertime.radius = READFIXED(save_p);
	battleovertime.x = READFIXED(save_p);
	battleovertime.y = READFIXED(save_p);
	battleovertime.z = READFIXED(save_p);

	wantedcalcdelay = READUINT32(save_p);
	indirectitemcooldown = READUINT32(save_p);
	mapreset = READUINT32(save_p);

	spectateGriefed = READUINT8(save_p);

	thwompsactive = (boolean)READUINT8(save_p);
	lastLowestLap = READUINT8(save_p);
	spbplace = READSINT8(save_p);
	startedInFreePlay = READUINT8(save_p);

	introtime = READUINT32(save_p);
	starttime = READUINT32(save_p);
	
	timelimitintics = READUINT32(save_p);
	extratimeintics = READUINT32(save_p);
	secretextratime = READUINT32(save_p);

	// Is it paused?
	if (READUINT8(save_p) == 0x2f)
		paused = true;

	// Only the server uses this, but it
	// needs synched for remote admins anyway.
	Schedule_Clear();

	numTasks = READUINT32(save_p);
	for (i = 0; i < numTasks; i++)
	{
		INT16 basetime;
		INT16 timer;
		char command[MAXTEXTCMD];

		basetime = READINT16(save_p);
		timer = READINT16(save_p);
		READSTRING(save_p, command);

		Schedule_Add(basetime, timer, command);
	}

	return true;
}

static inline void P_ArchiveLuabanksAndConsistency(void)
{
	UINT8 i, banksinuse = NUM_LUABANKS;

	while (banksinuse && !luabanks[banksinuse-1])
		banksinuse--; // get the last used bank

	if (banksinuse)
	{
		WRITEUINT8(save_p, 0xb7); // luabanks marker
		WRITEUINT8(save_p, banksinuse);
		for (i = 0; i < banksinuse; i++)
			WRITEINT32(save_p, luabanks[i]);
	}

	WRITEUINT8(save_p, 0x1d); // consistency marker
}

static inline boolean P_UnArchiveLuabanksAndConsistency(void)
{
	switch (READUINT8(save_p))
	{
		case 0xb7: // luabanks marker
			{
				UINT8 i, banksinuse = READUINT8(save_p);
				if (banksinuse > NUM_LUABANKS)
				{
					CONS_Alert(CONS_ERROR, M_GetText("Corrupt Luabanks! (Too many banks in use)\n"));
					return false;
				}
				for (i = 0; i < banksinuse; i++)
					luabanks[i] = READINT32(save_p);
				if (READUINT8(save_p) != 0x1d) // consistency marker
				{
					CONS_Alert(CONS_ERROR, M_GetText("Corrupt Luabanks! (Failed consistency check)\n"));
					return false;
				}
			}
		case 0x1d: // consistency marker
			break;
		default: // anything else is nonsense
			CONS_Alert(CONS_ERROR, M_GetText("Failed consistency check (???)\n"));
			return false;
	}

	return true;
}

void P_SaveGame(INT16 mapnum)
{
	P_ArchiveMisc(mapnum);
	P_ArchivePlayer();
	P_ArchiveLuabanksAndConsistency();
}

void P_SaveNetGame(boolean resending)
{
	thinker_t *th;
	mobj_t *mobj;
	INT32 i = 1; // don't start from 0, it'd be confused with a blank pointer otherwise

	CV_SaveNetVars(&save_p);
	P_NetArchiveMisc(resending);

	// Assign the mobjnumber for pointer tracking
	if (gamestate == GS_LEVEL)
	{
		for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
		{
			if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
				continue;

			mobj = (mobj_t *)th;
			if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER
				// MT_SPARK: used for debug stuff
				|| mobj->type == MT_SPARK)
				continue;
			mobj->mobjnum = i++;
		}
	}

	P_NetArchivePlayers();
	if (gamestate == GS_LEVEL)
	{
		P_NetArchiveWorld();
		P_ArchivePolyObjects();
		P_NetArchiveThinkers();
		P_NetArchiveSpecials();
		P_NetArchiveColormaps();
		P_NetArchiveTubeWaypoints();
		P_NetArchiveWaypoints();
	}
	LUA_Archive(&save_p);

	P_ArchiveLuabanksAndConsistency();
}

boolean P_LoadGame(INT16 mapoverride)
{
	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();
	if (gamestate == GS_VOTING)
		Y_EndVote();
	G_SetGamestate(GS_NULL); // should be changed in P_UnArchiveMisc

	P_UnArchiveSPGame(mapoverride);
	P_UnArchivePlayer();

	if (!P_UnArchiveLuabanksAndConsistency())
		return false;

	// Only do this after confirming savegame is ok
	G_DeferedInitNew(false, G_BuildMapName(gamemap), savedata.skin, 0, true);
	COM_BufAddText("dummyconsvar 1\n"); // G_DeferedInitNew doesn't do this

	return true;
}

boolean P_LoadNetGame(boolean reloading)
{
	CV_LoadNetVars(&save_p);
	if (!P_NetUnArchiveMisc(reloading))
		return false;
	P_NetUnArchivePlayers();
	if (gamestate == GS_LEVEL)
	{
		P_NetUnArchiveWorld();
		P_UnArchivePolyObjects();
		P_NetUnArchiveThinkers();
		P_NetUnArchiveSpecials();
		P_NetUnArchiveColormaps();
		P_NetUnArchiveTubeWaypoints();
		P_NetUnArchiveWaypoints();
		P_RelinkPointers();
		P_FinishMobjs();
	}
	LUA_UnArchive(&save_p);

	// This is stupid and hacky, but maybe it'll work!
	P_SetRandSeed(P_GetInitSeed());

	// The precipitation would normally be spawned in P_SetupLevel, which is called by
	// P_NetUnArchiveMisc above. However, that would place it up before P_NetUnArchiveThinkers,
	// so the thinkers would be deleted later. Therefore, P_SetupLevel will *not* spawn
	// precipitation when loading a netgame save. Instead, precip has to be spawned here.
	// This is done in P_NetUnArchiveSpecials now.

	return P_UnArchiveLuabanksAndConsistency();
}
