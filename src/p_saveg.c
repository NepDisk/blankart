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
#include "acs/interface.h"

savedata_t savedata;

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
} player_saveflags;

static inline void P_ArchivePlayer(savebuffer_t *save)
{
	const player_t *player = &players[consoleplayer];
	INT16 skininfo = player->skin;
	SINT8 pllives = player->lives;
	if (pllives < startinglivesbalance[numgameovers]) // Bump up to 3 lives if the player
		pllives = startinglivesbalance[numgameovers]; // has less than that.

	WRITEUINT16(save->p, skininfo);
	WRITEUINT8(save->p, numgameovers);
	WRITESINT8(save->p, pllives);
	WRITEUINT32(save->p, player->score);
}

static inline void P_UnArchivePlayer(savebuffer_t *save)
{
	INT16 skininfo = READUINT16(save->p);
	savedata.skin = skininfo;

	savedata.numgameovers = READUINT8(save->p);
	savedata.lives = READSINT8(save->p);
	savedata.score = READUINT32(save->p);
}

static void P_NetArchivePlayers(savebuffer_t *save)
{
	INT32 i, j;
	UINT16 flags;
//	size_t q;

	WRITEUINT32(save->p, ARCHIVEBLOCK_PLAYERS);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		WRITESINT8(save->p, (SINT8)adminplayers[i]);

		for (j = 0; j < PWRLV_NUMTYPES; j++)
		{
			WRITEINT16(save->p, clientpowerlevels[i][j]);
		}
		WRITEINT16(save->p, clientPowerAdd[i]);

		if (!playeringame[i])
			continue;

		flags = 0;

		// no longer send ticcmds

		WRITESTRINGN(save->p, player_names[i], MAXPLAYERNAME);

		WRITEUINT8(save->p, playerconsole[i]);
		WRITEINT32(save->p, splitscreen_invitations[i]);
		WRITEINT32(save->p, splitscreen_party_size[i]);
		WRITEINT32(save->p, splitscreen_original_party_size[i]);

		for (j = 0; j < MAXSPLITSCREENPLAYERS; ++j)
		{
			WRITEINT32(save->p, splitscreen_party[i][j]);
			WRITEINT32(save->p, splitscreen_original_party[i][j]);
		}

		WRITEANGLE(save->p, players[i].angleturn);
		WRITEANGLE(save->p, players[i].aiming);
		WRITEANGLE(save->p, players[i].drawangle);
		WRITEANGLE(save->p, players[i].viewrollangle);
		WRITEANGLE(save->p, players[i].tilt);
		WRITEANGLE(save->p, players[i].awayviewaiming);
		WRITEINT32(save->p, players[i].awayviewtics);

		WRITEUINT8(save->p, players[i].playerstate);
		WRITEUINT32(save->p, players[i].pflags);
		WRITEUINT8(save->p, players[i].panim);
		WRITEUINT8(save->p, players[i].spectator);
		WRITEUINT32(save->p, players[i].spectatewait);

		WRITEUINT16(save->p, players[i].flashpal);
		WRITEUINT16(save->p, players[i].flashcount);

		WRITEUINT8(save->p, players[i].skincolor);
		WRITEINT32(save->p, players[i].skin);
		WRITEUINT32(save->p, players[i].availabilities);
		WRITEUINT32(save->p, players[i].score);
		WRITESINT8(save->p, players[i].lives);
		WRITESINT8(save->p, players[i].xtralife);
		WRITEFIXED(save->p, players[i].speed);
		WRITEFIXED(save->p, players[i].lastspeed);
		WRITEINT32(save->p, players[i].deadtimer);
		WRITEUINT32(save->p, players[i].exiting);

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		WRITEFIXED(save->p, players[i].cmomx); // Conveyor momx
		WRITEFIXED(save->p, players[i].cmomy); // Conveyor momy
		WRITEFIXED(save->p, players[i].rmomx); // "Real" momx (momx - cmomx)
		WRITEFIXED(save->p, players[i].rmomy); // "Real" momy (momy - cmomy)

		WRITEINT16(save->p, players[i].totalring);
		WRITEUINT32(save->p, players[i].realtime);
		WRITEUINT8(save->p, players[i].laps);
		WRITEUINT8(save->p, players[i].latestlap);
		WRITEUINT8(save->p, players[i].lapvalidation);
		
		WRITEUINT32(save->p, players[i].starposttime);
		WRITEINT16(save->p, players[i].starpostx);
		WRITEINT16(save->p, players[i].starposty);
		WRITEINT16(save->p, players[i].starpostz);
		WRITEINT32(save->p, players[i].starpostnum);
		WRITEANGLE(save->p, players[i].starpostangle);
		WRITEUINT8(save->p, (UINT8)players[i].starpostflip);
		WRITEINT32(save->p, players[i].prevcheck);
		WRITEINT32(save->p, players[i].nextcheck);

		WRITEUINT8(save->p, players[i].ctfteam);

		WRITEUINT8(save->p, players[i].checkskip);

		WRITEINT16(save->p, players[i].lastsidehit);
		WRITEINT16(save->p, players[i].lastlinehit);

		WRITEINT32(save->p, players[i].onconveyor);

		WRITEUINT32(save->p, players[i].jointime);
		WRITEUINT32(save->p, players[i].spectatorreentry);

		WRITEUINT32(save->p, players[i].grieftime);
		WRITEUINT8(save->p, players[i].griefstrikes);

		WRITEUINT8(save->p, players[i].splitscreenindex);

		if (players[i].awayviewmobj)
			flags |= AWAYVIEW;

		if (players[i].followmobj)
			flags |= FOLLOWITEM;

		if (players[i].follower)
			flags |= FOLLOWER;

		WRITEUINT16(save->p, flags);

		if (flags & AWAYVIEW)
			WRITEUINT32(save->p, players[i].awayviewmobj->mobjnum);

		if (flags & FOLLOWITEM)
			WRITEUINT32(save->p, players[i].followmobj->mobjnum);

		WRITEUINT32(save->p, (UINT32)players[i].followitem);

		WRITEUINT32(save->p, players[i].charflags);

		// SRB2kart
		WRITEUINT8(save->p, players[i].kartspeed);
		WRITEUINT8(save->p, players[i].kartweight);
		
		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			WRITEINT16(save->p, players[i].lturn_max[j]);
			WRITEINT16(save->p, players[i].rturn_max[j]);
		}

		WRITEUINT8(save->p, players[i].followerskin);
		WRITEUINT8(save->p, players[i].followerready);	// booleans are really just numbers eh??
		WRITEUINT16(save->p, players[i].followercolor);
		if (flags & FOLLOWER)
			WRITEUINT32(save->p, players[i].follower->mobjnum);

		WRITEUINT16(save->p, players[i].nocontrol);
		WRITEUINT8(save->p, players[i].carry);
		WRITEUINT16(save->p, players[i].dye);

		WRITEUINT8(save->p, players[i].position);
		WRITEUINT8(save->p, players[i].oldposition);
		WRITEUINT8(save->p, players[i].positiondelay);
		WRITEUINT32(save->p, players[i].distancetofinish);
		WRITEUINT32(save->p, players[i].distancetofinishprev);
		WRITEUINT32(save->p, K_GetWaypointHeapIndex(players[i].currentwaypoint));
		WRITEUINT32(save->p, K_GetWaypointHeapIndex(players[i].nextwaypoint));
		WRITEUINT32(save->p, players[i].airtime);
		WRITEUINT8(save->p, players[i].startboost);

		WRITEUINT16(save->p, players[i].flashing);
		WRITEUINT16(save->p, players[i].spinouttimer);
		WRITEUINT8(save->p, players[i].spinouttype);
		WRITEINT16(save->p, players[i].squishedtimer);
		WRITEUINT8(save->p, players[i].instashield);
		WRITEUINT8(save->p, players[i].wipeoutslow);
		WRITEUINT8(save->p, players[i].justbumped);

		WRITESINT8(save->p, players[i].drift);
		WRITEFIXED(save->p, players[i].driftcharge);
		WRITEUINT8(save->p, players[i].driftboost);

		WRITESINT8(save->p, players[i].aizdriftstrat);
		WRITEINT32(save->p, players[i].aizdrifttilt);
		WRITEINT32(save->p, players[i].aizdriftturn);

		WRITEINT32(save->p, players[i].underwatertilt);

		WRITEFIXED(save->p, players[i].offroad);
		WRITEUINT8(save->p, players[i].tiregrease);
		WRITEUINT8(save->p, players[i].pogospring);
		WRITEUINT8(save->p, players[i].brakestop);
		WRITEUINT8(save->p, players[i].waterskip);
		
		WRITEUINT8(save->p, players[i].dashpadcooldown);

		WRITEFIXED(save->p, players[i].boostpower);
		WRITEFIXED(save->p, players[i].speedboost);
		WRITEFIXED(save->p, players[i].accelboost);
		WRITEANGLE(save->p, players[i].boostangle);

		WRITEUINT8(save->p, players[i].tripwireState);
		WRITEUINT8(save->p, players[i].tripwirePass);
		WRITEUINT16(save->p, players[i].tripwireLeniency);
		WRITEUINT8(save->p, players[i].tripwireReboundDelay);

		WRITEUINT16(save->p, players[i].itemroulette);
		WRITEUINT8(save->p, players[i].roulettetype);

		WRITESINT8(save->p, players[i].itemtype);
		WRITEUINT8(save->p, players[i].itemamount);
		WRITESINT8(save->p, players[i].throwdir);

		WRITEUINT8(save->p, players[i].sadtimer);

		WRITESINT8(save->p, players[i].rings);
		WRITEUINT8(save->p, players[i].pickuprings);
		WRITEUINT8(save->p, players[i].ringdelay);
		WRITEUINT16(save->p, players[i].ringboost);
		WRITEUINT16(save->p, players[i].superring);

		WRITEUINT8(save->p, players[i].curshield);
		WRITEUINT8(save->p, players[i].bubblecool);
		WRITEUINT8(save->p, players[i].bubbleblowup);
		WRITEUINT16(save->p, players[i].flamedash);
		WRITEUINT16(save->p, players[i].flamemeter);
		WRITEUINT8(save->p, players[i].flamelength);

		WRITEUINT16(save->p, players[i].hyudorotimer);
		WRITESINT8(save->p, players[i].stealingtimer);

		WRITEUINT16(save->p, players[i].sneakertimer);
		WRITEUINT8(save->p, players[i].floorboost);
		WRITEUINT8(save->p, players[i].waterrun);
		
		WRITEUINT8(save->p, players[i].boostcharge);

		WRITEINT16(save->p, players[i].growshrinktimer);
		WRITEINT16(save->p, players[i].growcancel);
		WRITEUINT16(save->p, players[i].rocketsneakertimer);
		WRITEUINT16(save->p, players[i].invincibilitytimer);

		WRITEUINT8(save->p, players[i].eggmanexplode);
		WRITESINT8(save->p, players[i].eggmanblame);

		WRITEUINT8(save->p, players[i].bananadrag);

		WRITESINT8(save->p, players[i].lastjawztarget);
		WRITEUINT8(save->p, players[i].jawztargetdelay);

		WRITEUINT8(save->p, players[i].confirmVictim);
		WRITEUINT8(save->p, players[i].confirmVictimDelay);

		WRITEUINT32(save->p, players[i].roundscore);
		WRITEUINT8(save->p, players[i].emeralds);
		WRITEUINT8(save->p, players[i].bumper);
		WRITEINT16(save->p, players[i].karmadelay);
		WRITEUINT32(save->p, players[i].overtimekarma);
		WRITEINT16(save->p, players[i].spheres);
		WRITEUINT32(save->p, players[i].spheredigestion);

		WRITESINT8(save->p, players[i].glanceDir);

		WRITEUINT8(save->p, players[i].typing_timer);
		WRITEUINT8(save->p, players[i].typing_duration);

		WRITEUINT8(save->p, players[i].kickstartaccel);

		// botvars_t
		WRITEUINT8(save->p, players[i].botvars.difficulty);
		WRITEUINT8(save->p, players[i].botvars.diffincrease);
		WRITEUINT8(save->p, players[i].botvars.rival);
		WRITEFIXED(save->p, players[i].botvars.rubberband);
		WRITEUINT16(save->p, players[i].botvars.controller);
		WRITEUINT32(save->p, players[i].botvars.itemdelay);
		WRITEUINT32(save->p, players[i].botvars.itemconfirm);
		WRITESINT8(save->p, players[i].botvars.turnconfirm);
		
		WRITEFIXED(save->p, players[i].outrun);
		WRITEUINT8(save->p, players[i].outruntime);
		
		// sonicloopsvars_t
		WRITEFIXED(save->p, players[i].loop.radius);
		WRITEFIXED(save->p, players[i].loop.revolution);
		WRITEFIXED(save->p, players[i].loop.min_revolution);
		WRITEFIXED(save->p, players[i].loop.max_revolution);
		WRITEANGLE(save->p, players[i].loop.yaw);
		WRITEFIXED(save->p, players[i].loop.origin.x);
		WRITEFIXED(save->p, players[i].loop.origin.y);
		WRITEFIXED(save->p, players[i].loop.origin.z);
		WRITEFIXED(save->p, players[i].loop.origin_shift.x);
		WRITEFIXED(save->p, players[i].loop.origin_shift.y);
		WRITEFIXED(save->p, players[i].loop.shift.x);
		WRITEFIXED(save->p, players[i].loop.shift.y);
		WRITEUINT8(save->p, players[i].loop.flip);

		// sonicloopcamvars_t
		WRITEUINT32(save->p, players[i].loop.camera.enter_tic);
		WRITEUINT32(save->p, players[i].loop.camera.exit_tic);
		WRITEUINT32(save->p, players[i].loop.camera.zoom_in_speed);
		WRITEUINT32(save->p, players[i].loop.camera.zoom_out_speed);
		WRITEFIXED(save->p, players[i].loop.camera.dist);
		WRITEANGLE(save->p, players[i].loop.camera.pan);
		WRITEFIXED(save->p, players[i].loop.camera.pan_speed);
		WRITEUINT32(save->p, players[i].loop.camera.pan_accel);
		WRITEUINT32(save->p, players[i].loop.camera.pan_back);
	}
}

static void P_NetUnArchivePlayers(savebuffer_t *save)
{
	INT32 i, j;
	UINT16 flags;

	if (READUINT32(save->p) != ARCHIVEBLOCK_PLAYERS)
		I_Error("Bad $$$.sav at archive block Players");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		adminplayers[i] = (INT32)READSINT8(save->p);

		for (j = 0; j < PWRLV_NUMTYPES; j++)
		{
			clientpowerlevels[i][j] = READINT16(save->p);
		}
		clientPowerAdd[i] = READINT16(save->p);

		// Do NOT memset player struct to 0
		// other areas may initialize data elsewhere
		//memset(&players[i], 0, sizeof (player_t));
		if (!playeringame[i])
			continue;

		// NOTE: sending tics should (hopefully) no longer be necessary

		READSTRINGN(save->p, player_names[i], MAXPLAYERNAME);

		playerconsole[i] = READUINT8(save->p);
		splitscreen_invitations[i] = READINT32(save->p);
		splitscreen_party_size[i] = READINT32(save->p);
		splitscreen_original_party_size[i] = READINT32(save->p);

		for (j = 0; j < MAXSPLITSCREENPLAYERS; ++j)
		{
			splitscreen_party[i][j] = READINT32(save->p);
			splitscreen_original_party[i][j] = READINT32(save->p);
		}

		players[i].angleturn = READANGLE(save->p);
		players[i].aiming = READANGLE(save->p);
		players[i].drawangle = players[i].old_drawangle = READANGLE(save->p);
		players[i].viewrollangle = READANGLE(save->p);
		players[i].tilt = READANGLE(save->p);
		players[i].awayviewaiming = READANGLE(save->p);
		players[i].awayviewtics = READINT32(save->p);

		players[i].playerstate = READUINT8(save->p);
		players[i].pflags = READUINT32(save->p);
		players[i].panim = READUINT8(save->p);
		players[i].spectator = READUINT8(save->p);
		players[i].spectatewait = READUINT32(save->p);

		players[i].flashpal = READUINT16(save->p);
		players[i].flashcount = READUINT16(save->p);

		players[i].skincolor = READUINT8(save->p);
		players[i].skin = READINT32(save->p);
		players[i].availabilities = READUINT32(save->p);
		players[i].score = READUINT32(save->p);
		players[i].lives = READSINT8(save->p);
		players[i].xtralife = READSINT8(save->p); // Ring Extra Life counter
		players[i].speed = READFIXED(save->p); // Player's speed (distance formula of MOMX and MOMY values)
		players[i].lastspeed = READFIXED(save->p);
		players[i].deadtimer = READINT32(save->p); // End game if game over lasts too long
		players[i].exiting = READUINT32(save->p); // Exitlevel timer

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		players[i].cmomx = READFIXED(save->p); // Conveyor momx
		players[i].cmomy = READFIXED(save->p); // Conveyor momy
		players[i].rmomx = READFIXED(save->p); // "Real" momx (momx - cmomx)
		players[i].rmomy = READFIXED(save->p); // "Real" momy (momy - cmomy)

		players[i].totalring = READINT16(save->p); // Total number of rings obtained for GP
		players[i].realtime = READUINT32(save->p); // integer replacement for leveltime
		players[i].laps = READUINT8(save->p); // Number of laps (optional)
		players[i].latestlap = READUINT8(save->p);
		players[i].lapvalidation = READUINT8(save->p);
		
		players[i].starposttime = READUINT32(save->p);
		players[i].starpostx = READINT16(save->p);
		players[i].starposty = READINT16(save->p);
		players[i].starpostz = READINT16(save->p);
		players[i].starpostnum = READINT32(save->p);
		players[i].starpostangle = READANGLE(save->p);
		players[i].starpostflip = (boolean)READUINT8(save->p);
		players[i].prevcheck = READINT32(save->p);
		players[i].nextcheck = READINT32(save->p);

		players[i].ctfteam = READUINT8(save->p); // 1 == Red, 2 == Blue

		players[i].checkskip = READUINT8(save->p);

		players[i].lastsidehit = READINT16(save->p);
		players[i].lastlinehit = READINT16(save->p);

		players[i].onconveyor = READINT32(save->p);

		players[i].jointime = READUINT32(save->p);
		players[i].spectatorreentry = READUINT32(save->p);

		players[i].grieftime = READUINT32(save->p);
		players[i].griefstrikes = READUINT8(save->p);

		players[i].splitscreenindex = READUINT8(save->p);

		flags = READUINT16(save->p);

		if (flags & AWAYVIEW)
			players[i].awayviewmobj = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & FOLLOWITEM)
			players[i].followmobj = (mobj_t *)(size_t)READUINT32(save->p);
		
		players[i].followitem = (mobjtype_t)READUINT32(save->p);

		//SetPlayerSkinByNum(i, players[i].skin);
		players[i].charflags = READUINT32(save->p);

		// SRB2kart
		players[i].kartspeed = READUINT8(save->p);
		players[i].kartweight = READUINT8(save->p);
		
		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			players[i].lturn_max[j] = READINT16(save->p);
			players[i].rturn_max[j] = READINT16(save->p);
		}

		players[i].followerskin = READUINT8(save->p);
		players[i].followerready = READUINT8(save->p);
		players[i].followercolor = READUINT16(save->p);
		if (flags & FOLLOWER)
			players[i].follower = (mobj_t *)(size_t)READUINT32(save->p);

		players[i].nocontrol = READUINT16(save->p);
		players[i].carry = READUINT8(save->p);
		players[i].dye = READUINT16(save->p);

		players[i].position = READUINT8(save->p);
		players[i].oldposition = READUINT8(save->p);
		players[i].positiondelay = READUINT8(save->p);
		players[i].distancetofinish = READUINT32(save->p);
		players[i].distancetofinishprev = READUINT32(save->p);
		players[i].currentwaypoint = (waypoint_t *)(size_t)READUINT32(save->p);
		players[i].nextwaypoint = (waypoint_t *)(size_t)READUINT32(save->p);
		players[i].airtime = READUINT32(save->p);
		players[i].startboost = READUINT8(save->p);

		players[i].flashing = READUINT16(save->p);
		players[i].spinouttimer = READUINT16(save->p);
		players[i].spinouttype = READUINT8(save->p);
		players[i].squishedtimer = READINT16(save->p);
		players[i].instashield = READUINT8(save->p);
		players[i].wipeoutslow = READUINT8(save->p);
		players[i].justbumped = READUINT8(save->p);

		players[i].drift = READSINT8(save->p);
		players[i].driftcharge = READFIXED(save->p);
		players[i].driftboost = READUINT8(save->p);

		players[i].aizdriftstrat = READSINT8(save->p);
		players[i].aizdrifttilt = READINT32(save->p);
		players[i].aizdriftturn = READINT32(save->p);

		players[i].underwatertilt = READINT32(save->p);

		players[i].offroad = READFIXED(save->p);
		players[i].tiregrease = READFIXED(save->p);
		players[i].pogospring = READUINT8(save->p);
		players[i].brakestop = READUINT8(save->p);
		players[i].waterskip = READUINT8(save->p);

		players[i].dashpadcooldown = READUINT8(save->p);

		players[i].boostpower = READFIXED(save->p);
		players[i].speedboost = READFIXED(save->p);
		players[i].accelboost = READFIXED(save->p);
		players[i].boostangle = READANGLE(save->p);

		players[i].tripwireState = READUINT8(save->p);
		players[i].tripwirePass = READUINT8(save->p);
		players[i].tripwireLeniency = READUINT16(save->p);
		players[i].tripwireReboundDelay = READUINT8(save->p);

		players[i].itemroulette = READUINT16(save->p);
		players[i].roulettetype = READUINT8(save->p);

		players[i].itemtype = READSINT8(save->p);
		players[i].itemamount = READUINT8(save->p);
		players[i].throwdir = READSINT8(save->p);

		players[i].sadtimer = READUINT8(save->p);

		players[i].rings = READSINT8(save->p);
		players[i].pickuprings = READUINT8(save->p);
		players[i].ringdelay = READUINT8(save->p);
		players[i].ringboost = READUINT16(save->p);;
		players[i].superring = READUINT16(save->p);

		players[i].curshield = READUINT8(save->p);
		players[i].bubblecool = READUINT8(save->p);
		players[i].bubbleblowup = READUINT8(save->p);
		players[i].flamedash = READUINT16(save->p);
		players[i].flamemeter = READUINT16(save->p);
		players[i].flamelength = READUINT8(save->p);

		players[i].hyudorotimer = READUINT16(save->p);
		players[i].stealingtimer = READSINT8(save->p);

		players[i].sneakertimer = READUINT16(save->p);
		players[i].floorboost = READUINT8(save->p);
		players[i].waterrun = READUINT8(save->p);
		
		players[i].boostcharge = READUINT8(save->p);

		players[i].growshrinktimer = READINT16(save->p);
		players[i].growcancel = READINT16(save->p);
		players[i].rocketsneakertimer = READUINT16(save->p);
		players[i].invincibilitytimer = READUINT16(save->p);

		players[i].eggmanexplode = READUINT8(save->p);
		players[i].eggmanblame = READSINT8(save->p);

		players[i].bananadrag = READUINT8(save->p);

		players[i].lastjawztarget = READSINT8(save->p);
		players[i].jawztargetdelay = READUINT8(save->p);

		players[i].confirmVictim = READUINT8(save->p);
		players[i].confirmVictimDelay = READUINT8(save->p);

		players[i].roundscore = READUINT32(save->p);
		players[i].emeralds = READUINT8(save->p);
		players[i].bumper = READUINT8(save->p);
		players[i].karmadelay = READINT16(save->p);
		players[i].overtimekarma = READUINT32(save->p);
		players[i].spheres = READINT16(save->p);
		players[i].spheredigestion = READUINT32(save->p);

		players[i].glanceDir = READSINT8(save->p);

		players[i].typing_timer = READUINT8(save->p);
		players[i].typing_duration = READUINT8(save->p);

		players[i].kickstartaccel = READUINT8(save->p);

		// botvars_t
		players[i].botvars.difficulty = READUINT8(save->p);
		players[i].botvars.diffincrease = READUINT8(save->p);
		players[i].botvars.rival = (boolean)READUINT8(save->p);
		players[i].botvars.rubberband = READFIXED(save->p);
		players[i].botvars.controller = READUINT16(save->p);
		players[i].botvars.itemdelay = READUINT32(save->p);
		players[i].botvars.itemconfirm = READUINT32(save->p);
		players[i].botvars.turnconfirm = READSINT8(save->p);
		
		players[i].outrun = READFIXED(save->p);
		players[i].outruntime = READUINT8(save->p);

		// sonicloopsvars_t
		players[i].loop.radius = READFIXED(save->p);
		players[i].loop.revolution = READFIXED(save->p);
		players[i].loop.min_revolution = READFIXED(save->p);
		players[i].loop.max_revolution = READFIXED(save->p);
		players[i].loop.yaw = READANGLE(save->p);
		players[i].loop.origin.x = READFIXED(save->p);
		players[i].loop.origin.y = READFIXED(save->p);
		players[i].loop.origin.z = READFIXED(save->p);
		players[i].loop.origin_shift.x = READFIXED(save->p);
		players[i].loop.origin_shift.y = READFIXED(save->p);
		players[i].loop.shift.x = READFIXED(save->p);
		players[i].loop.shift.y = READFIXED(save->p);
		players[i].loop.flip = READUINT8(save->p);

		// sonicloopcamvars_t
		players[i].loop.camera.enter_tic = READUINT32(save->p);
		players[i].loop.camera.exit_tic = READUINT32(save->p);
		players[i].loop.camera.zoom_in_speed = READUINT32(save->p);
		players[i].loop.camera.zoom_out_speed = READUINT32(save->p);
		players[i].loop.camera.dist = READFIXED(save->p);
		players[i].loop.camera.pan = READANGLE(save->p);
		players[i].loop.camera.pan_speed = READFIXED(save->p);
		players[i].loop.camera.pan_accel = READUINT32(save->p);
		players[i].loop.camera.pan_back = READUINT32(save->p);

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

static void P_NetArchiveColormaps(savebuffer_t *save)
{
	// We save and then we clean up our colormap mess
	extracolormap_t *exc, *exc_next;
	UINT32 i = 0;
	WRITEUINT32(save->p, num_net_colormaps); // save for safety

	for (exc = net_colormaps; i < num_net_colormaps; i++, exc = exc_next)
	{
		// We must save num_net_colormaps worth of data
		// So fill non-existent entries with default.
		if (!exc)
			exc = R_CreateDefaultColormap(false);

		WRITEUINT8(save->p, exc->fadestart);
		WRITEUINT8(save->p, exc->fadeend);
		WRITEUINT8(save->p, exc->flags);

		WRITEINT32(save->p, exc->rgba);
		WRITEINT32(save->p, exc->fadergba);

#ifdef EXTRACOLORMAPLUMPS
		WRITESTRINGN(save->p, exc->lumpname, 9);
#endif

		exc_next = exc->next;
		Z_Free(exc); // don't need anymore
	}

	num_net_colormaps = 0;
	num_ffloors = 0;
	net_colormaps = NULL;
}

static void P_NetUnArchiveColormaps(savebuffer_t *save)
{
	// When we reach this point, we already populated our list with
	// dummy colormaps. Now that we are loading the color data,
	// set up the dummies.
	extracolormap_t *exc, *existing_exc, *exc_next = NULL;
	UINT32 i = 0;

	num_net_colormaps = READUINT32(save->p);

	for (exc = net_colormaps; i < num_net_colormaps; i++, exc = exc_next)
	{
		UINT8 fadestart, fadeend, flags;
		INT32 rgba, fadergba;
#ifdef EXTRACOLORMAPLUMPS
		char lumpname[9];
#endif

		fadestart = READUINT8(save->p);
		fadeend = READUINT8(save->p);
		flags = READUINT8(save->p);

		rgba = READINT32(save->p);
		fadergba = READINT32(save->p);

#ifdef EXTRACOLORMAPLUMPS
		READSTRINGN(save->p, lumpname, 9);

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
#define SD_TRIGGERER  0x04
#define SD_GRAVITY    0x08
#define SD_ACTION     0x10
#define SD_ARGS       0x20
#define SD_STRINGARGS 0x40
#define SD_DIFF5      0x80

//diff5 flags
#define SD_ACTIVATION 0x01

static boolean P_SectorArgsEqual(const sector_t *sc, const sector_t *spawnsc)
{
	UINT8 i;
	for (i = 0; i < NUM_SCRIPT_ARGS; i++)
		if (sc->args[i] != spawnsc->args[i])
			return false;

	return true;
}

static boolean P_SectorStringArgsEqual(const sector_t *sc, const sector_t *spawnsc)
{
	UINT8 i;
	for (i = 0; i < NUM_SCRIPT_STRINGARGS; i++)
	{
		if (!sc->stringargs[i])
			return !spawnsc->stringargs[i];

		if (strcmp(sc->stringargs[i], spawnsc->stringargs[i]))
			return false;
	}

	return true;
}

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
#define LD_DIFF3         0x80

// diff3 flags
#define LD_ACTIVATION    0x01

static boolean P_LineArgsEqual(const line_t *li, const line_t *spawnli)
{
	UINT8 i;
	for (i = 0; i < NUM_SCRIPT_ARGS; i++)
		if (li->args[i] != spawnli->args[i])
			return false;

	return true;
}

static boolean P_LineStringArgsEqual(const line_t *li, const line_t *spawnli)
{
	UINT8 i;
	for (i = 0; i < NUM_SCRIPT_STRINGARGS; i++)
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
static void ArchiveFFloors(savebuffer_t *save, const sector_t *ss)
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
			WRITEUINT16(save->p, j); // save ffloor "number"
			WRITEUINT8(save->p, fflr_diff);
			if (fflr_diff & FD_FLAGS)
				WRITEUINT32(save->p, rover->fofflags);
			if (fflr_diff & FD_ALPHA)
				WRITEINT16(save->p, rover->alpha);
		}
		j++;
	}
	WRITEUINT16(save->p, 0xffff);
}

static void UnArchiveFFloors(savebuffer_t *save, const sector_t *ss)
{
	UINT16 j = 0; // number of current ffloor in loop
	UINT16 fflr_i; // saved ffloor "number" of next modified ffloor
	UINT16 fflr_diff; // saved ffloor diff
	ffloor_t *rover;

	rover = ss->ffloors;
	if (!rover) // it is assumed sectors[i].ffloors actually exists, but just in case...
		I_Error("Sector does not have any ffloors!");

	fflr_i = READUINT16(save->p); // get first modified ffloor's number ready
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

		fflr_diff = READUINT8(save->p);

		if (fflr_diff & FD_FLAGS)
			rover->fofflags = READUINT32(save->p);
		if (fflr_diff & FD_ALPHA)
			rover->alpha = READINT16(save->p);

		fflr_i = READUINT16(save->p); // get next ffloor "number" ready

		j++;
		rover = rover->next;
	}
}

static void ArchiveSectors(savebuffer_t *save)
{
	size_t i, j;
	const sector_t *ss = sectors;
	const sector_t *spawnss = spawnsectors;
	UINT8 diff, diff2, diff3, diff4, diff5;

	for (i = 0; i < numsectors; i++, ss++, spawnss++)
	{
		diff = diff2 = diff3 = diff4 = diff5 = 0;
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

		if (ss->action != spawnss->action)
			diff4 |= SD_ACTION;
		if (!P_SectorArgsEqual(ss, spawnss))
			diff4 |= SD_ARGS;
		if (!P_SectorStringArgsEqual(ss, spawnss))
			diff4 |= SD_STRINGARGS;
		if (ss->activation != spawnss->activation)
			diff5 |= SD_ACTIVATION;

		if (ss->ffloors && CheckFFloorDiff(ss))
			diff |= SD_FFLOORS;

		if (diff5)
			diff4 |= SD_DIFF5;

		if (diff4)
			diff3 |= SD_DIFF4;

		if (diff3)
			diff2 |= SD_DIFF3;

		if (diff2)
			diff |= SD_DIFF2;

		if (diff)
		{
			WRITEUINT16(save->p, i);
			WRITEUINT8(save->p, diff);
			if (diff & SD_DIFF2)
				WRITEUINT8(save->p, diff2);
			if (diff2 & SD_DIFF3)
				WRITEUINT8(save->p, diff3);
			if (diff3 & SD_DIFF4)
				WRITEUINT8(save->p, diff4);
			if (diff & SD_FLOORHT)
				WRITEFIXED(save->p, ss->floorheight);
			if (diff & SD_CEILHT)
				WRITEFIXED(save->p, ss->ceilingheight);
			if (diff & SD_FLOORPIC)
				WRITEMEM(save->p, levelflats[ss->floorpic].name, 8);
			if (diff & SD_CEILPIC)
				WRITEMEM(save->p, levelflats[ss->ceilingpic].name, 8);
			if (diff & SD_LIGHT)
				WRITEINT16(save->p, ss->lightlevel);
			if (diff & SD_SPECIAL)
				WRITEINT16(save->p, ss->special);
			if (diff2 & SD_FXOFFS)
				WRITEFIXED(save->p, ss->floor_xoffs);
			if (diff2 & SD_FYOFFS)
				WRITEFIXED(save->p, ss->floor_yoffs);
			if (diff2 & SD_CXOFFS)
				WRITEFIXED(save->p, ss->ceiling_xoffs);
			if (diff2 & SD_CYOFFS)
				WRITEFIXED(save->p, ss->ceiling_yoffs);
			if (diff2 & SD_FLOORANG)
				WRITEANGLE(save->p, ss->floorpic_angle);
			if (diff2 & SD_CEILANG)
				WRITEANGLE(save->p, ss->ceilingpic_angle);
			if (diff2 & SD_TAG)
			{
				WRITEUINT32(save->p, ss->tags.count);
				for (j = 0; j < ss->tags.count; j++)
					WRITEINT16(save->p, ss->tags.tags[j]);
			}

			if (diff3 & SD_COLORMAP)
				WRITEUINT32(save->p, CheckAddNetColormapToList(ss->extra_colormap));
					// returns existing index if already added, or appends to net_colormaps and returns new index
			if (diff3 & SD_CRUMBLESTATE)
				WRITEINT32(save->p, ss->crumblestate);
			if (diff3 & SD_FLOORLIGHT)
			{
				WRITEINT16(save->p, ss->floorlightlevel);
				WRITEUINT8(save->p, ss->floorlightabsolute);
			}
			if (diff3 & SD_CEILLIGHT)
			{
				WRITEINT16(save->p, ss->ceilinglightlevel);
				WRITEUINT8(save->p, ss->ceilinglightabsolute);
			}
			if (diff3 & SD_FLAG)
				WRITEUINT32(save->p, ss->flags);
			if (diff3 & SD_SPECIALFLAG)
				WRITEUINT32(save->p, ss->specialflags);
			if (diff4 & SD_DAMAGETYPE)
				WRITEUINT8(save->p, ss->damagetype);
			if (diff4 & SD_TRIGGERTAG)
				WRITEINT16(save->p, ss->triggertag);
			if (diff4 & SD_TRIGGERER)
				WRITEUINT8(save->p, ss->triggerer);
			if (diff4 & SD_GRAVITY)
				WRITEFIXED(save->p, ss->gravity);

			if (diff4 & SD_ACTION)
				WRITEINT16(save->p, ss->action);
			if (diff4 & SD_ARGS)
			{
				for (j = 0; j < NUM_SCRIPT_ARGS; j++)
					WRITEINT32(save->p, ss->args[j]);
			}
			if (diff4 & SD_STRINGARGS)
			{
				for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
				{
					size_t len, k;

					if (!ss->stringargs[j])
					{
						WRITEINT32(save->p, 0);
						continue;
					}

					len = strlen(ss->stringargs[j]);
					WRITEINT32(save->p, len);
					for (k = 0; k < len; k++)
						WRITECHAR(save->p, ss->stringargs[j][k]);
				}
			}
			if (diff5 & SD_ACTIVATION)
				WRITEUINT32(save->p, ss->activation);

			if (diff & SD_FFLOORS)
				ArchiveFFloors(save, ss);
		}
	}

	WRITEUINT16(save->p, 0xffff);
}

static void UnArchiveSectors(savebuffer_t *save)
{
	UINT16 i, j;
	UINT8 diff, diff2, diff3, diff4, diff5;
	for (;;)
	{
		i = READUINT16(save->p);

		if (i == 0xffff)
			break;

		if (i > numsectors)
			I_Error("Invalid sector number %u from server (expected end at %s)", i, sizeu1(numsectors));

		diff = READUINT8(save->p);
		if (diff & SD_DIFF2)
			diff2 = READUINT8(save->p);
		else
			diff2 = 0;
		if (diff2 & SD_DIFF3)
			diff3 = READUINT8(save->p);
		else
			diff3 = 0;
		if (diff3 & SD_DIFF4)
			diff4 = READUINT8(save->p);
		else
			diff4 = 0;

		if (diff4 & SD_DIFF5)
			diff5 = READUINT8(save->p);
		else
			diff5 = 0;

		if (diff & SD_FLOORHT)
			sectors[i].floorheight = READFIXED(save->p);
		if (diff & SD_CEILHT)
			sectors[i].ceilingheight = READFIXED(save->p);
		if (diff & SD_FLOORPIC)
		{
			sectors[i].floorpic = P_AddLevelFlatRuntime((char *)save->p);
			save->p += 8;
		}
		if (diff & SD_CEILPIC)
		{
			sectors[i].ceilingpic = P_AddLevelFlatRuntime((char *)save->p);
			save->p += 8;
		}
		if (diff & SD_LIGHT)
			sectors[i].lightlevel = READINT16(save->p);
		if (diff & SD_SPECIAL)
			sectors[i].special = READINT16(save->p);

		if (diff2 & SD_FXOFFS)
			sectors[i].floor_xoffs = READFIXED(save->p);
		if (diff2 & SD_FYOFFS)
			sectors[i].floor_yoffs = READFIXED(save->p);
		if (diff2 & SD_CXOFFS)
			sectors[i].ceiling_xoffs = READFIXED(save->p);
		if (diff2 & SD_CYOFFS)
			sectors[i].ceiling_yoffs = READFIXED(save->p);
		if (diff2 & SD_FLOORANG)
			sectors[i].floorpic_angle  = READANGLE(save->p);
		if (diff2 & SD_CEILANG)
			sectors[i].ceilingpic_angle = READANGLE(save->p);
		if (diff2 & SD_TAG)
		{
			size_t ncount = READUINT32(save->p);

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
				sectors[i].tags.tags[j] = READINT16(save->p);

			// Add new entries.
			for (j = 0; j < sectors[i].tags.count; j++)
				Taggroup_Remove(tags_sectors, sectors[i].tags.tags[j], i);
		}


		if (diff3 & SD_COLORMAP)
			sectors[i].extra_colormap = GetNetColormapFromList(READUINT32(save->p));
		if (diff3 & SD_CRUMBLESTATE)
			sectors[i].crumblestate = READINT32(save->p);
		if (diff3 & SD_FLOORLIGHT)
		{
			sectors[i].floorlightlevel = READINT16(save->p);
			sectors[i].floorlightabsolute = READUINT8(save->p);
		}
		if (diff3 & SD_CEILLIGHT)
		{
			sectors[i].ceilinglightlevel = READINT16(save->p);
			sectors[i].ceilinglightabsolute = READUINT8(save->p);
		}
		if (diff3 & SD_FLAG)
		{
			sectors[i].flags = READUINT32(save->p);
			CheckForReverseGravity |= (sectors[i].flags & MSF_GRAVITYFLIP);
		}
		if (diff3 & SD_SPECIALFLAG)
			sectors[i].specialflags = READUINT32(save->p);
		if (diff4 & SD_DAMAGETYPE)
			sectors[i].damagetype = READUINT8(save->p);
		if (diff4 & SD_TRIGGERTAG)
			sectors[i].triggertag = READINT16(save->p);
		if (diff4 & SD_TRIGGERER)
			sectors[i].triggerer = READUINT8(save->p);
		if (diff4 & SD_GRAVITY)
			sectors[i].gravity = READFIXED(save->p);

		if (diff4 & SD_ACTION)
			sectors[i].action = READINT16(save->p);
		if (diff4 & SD_ARGS)
		{
			for (j = 0; j < NUM_SCRIPT_ARGS; j++)
				sectors[i].args[j] = READINT32(save->p);
		}
		if (diff4 & SD_STRINGARGS)
		{
			for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
			{
				size_t len = READINT32(save->p);
				size_t k;

				if (!len)
				{
					Z_Free(sectors[i].stringargs[j]);
					sectors[i].stringargs[j] = NULL;
					continue;
				}

				sectors[i].stringargs[j] = Z_Realloc(sectors[i].stringargs[j], len + 1, PU_LEVEL, NULL);
				for (k = 0; k < len; k++)
					sectors[i].stringargs[j][k] = READCHAR(save->p);
				sectors[i].stringargs[j][len] = '\0';
			}
		}
		if (diff5 & SD_ACTIVATION)
			sectors[i].activation = READUINT32(save->p);

		if (diff & SD_FFLOORS)
			UnArchiveFFloors(save, &sectors[i]);
	}
}

static void ArchiveLines(savebuffer_t *save)
{
	size_t i;
	const line_t *li = lines;
	const line_t *spawnli = spawnlines;
	const side_t *si;
	const side_t *spawnsi;
	UINT8 diff, diff2, diff3;

	for (i = 0; i < numlines; i++, spawnli++, li++)
	{
		diff = diff2 = diff3 = 0;

		if (li->flags != spawnli->flags)
			diff |= LD_FLAG;

		if (li->special != spawnli->special)
			diff |= LD_SPECIAL;

		if (spawnli->special == 321 || spawnli->special == 322) // only reason li->callcount would be non-zero is if either of these are involved
			diff |= LD_CLLCOUNT;

		if (!P_LineArgsEqual(li, spawnli))
			diff2 |= LD_ARGS;

		if (!P_LineStringArgsEqual(li, spawnli))
			diff2 |= LD_STRINGARGS;

		if (li->executordelay != spawnli->executordelay)
			diff2 |= LD_EXECUTORDELAY;

		if (li->activation != spawnli->activation)
			diff3 |= LD_ACTIVATION;

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

		if (diff3)
			diff2 |= LD_DIFF3;

		if (diff2)
			diff |= LD_DIFF2;

		if (diff)
		{
			WRITEINT16(save->p, i);
			WRITEUINT8(save->p, diff);
			if (diff & LD_DIFF2)
				WRITEUINT8(save->p, diff2);
			if (diff2 & LD_DIFF3)
				WRITEUINT8(save->p, diff3);
			if (diff & LD_FLAG)
				WRITEUINT32(save->p, li->flags);
			if (diff & LD_SPECIAL)
				WRITEINT16(save->p, li->special);
			if (diff & LD_CLLCOUNT)
				WRITEINT16(save->p, li->callcount);

			si = &sides[li->sidenum[0]];
			if (diff & LD_S1TEXOFF)
				WRITEFIXED(save->p, si->textureoffset);
			if (diff & LD_S1TOPTEX)
				WRITEINT32(save->p, si->toptexture);
			if (diff & LD_S1BOTTEX)
				WRITEINT32(save->p, si->bottomtexture);
			if (diff & LD_S1MIDTEX)
				WRITEINT32(save->p, si->midtexture);

			si = &sides[li->sidenum[1]];
			if (diff2 & LD_S2TEXOFF)
				WRITEFIXED(save->p, si->textureoffset);
			if (diff2 & LD_S2TOPTEX)
				WRITEINT32(save->p, si->toptexture);
			if (diff2 & LD_S2BOTTEX)
				WRITEINT32(save->p, si->bottomtexture);
			if (diff2 & LD_S2MIDTEX)
				WRITEINT32(save->p, si->midtexture);
			if (diff2 & LD_ARGS)
			{
				UINT8 j;
				for (j = 0; j < NUM_SCRIPT_ARGS; j++)
					WRITEINT32(save->p, li->args[j]);
			}
			if (diff2 & LD_STRINGARGS)
			{
				UINT8 j;
				for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
				{
					size_t len, k;

					if (!li->stringargs[j])
					{
						WRITEINT32(save->p, 0);
						continue;
					}

					len = strlen(li->stringargs[j]);
					WRITEINT32(save->p, len);
					for (k = 0; k < len; k++)
						WRITECHAR(save->p, li->stringargs[j][k]);
				}
			}
			if (diff2 & LD_EXECUTORDELAY)
				WRITEINT32(save->p, li->executordelay);
			if (diff3 & LD_ACTIVATION)
				WRITEUINT32(save->p, li->activation);
		}
	}
	WRITEUINT16(save->p, 0xffff);
}

static void UnArchiveLines(savebuffer_t *save)
{
	UINT16 i;
	line_t *li;
	side_t *si;
	UINT8 diff, diff2, diff3;

	for (;;)
	{
		i = READUINT16(save->p);

		if (i == 0xffff)
			break;
		if (i > numlines)
			I_Error("Invalid line number %u from server", i);

		diff = READUINT8(save->p);
		li = &lines[i];

		if (diff & LD_DIFF2)
			diff2 = READUINT8(save->p);
		else
			diff2 = 0;

		if (diff2 & LD_DIFF3)
			diff3 = READUINT8(save->p);
		else
			diff3 = 0;

		if (diff & LD_FLAG)
			li->flags = READUINT32(save->p);
		if (diff & LD_SPECIAL)
			li->special = READINT16(save->p);
		if (diff & LD_CLLCOUNT)
			li->callcount = READINT16(save->p);

		si = &sides[li->sidenum[0]];
		if (diff & LD_S1TEXOFF)
			si->textureoffset = READFIXED(save->p);
		if (diff & LD_S1TOPTEX)
			si->toptexture = READINT32(save->p);
		if (diff & LD_S1BOTTEX)
			si->bottomtexture = READINT32(save->p);
		if (diff & LD_S1MIDTEX)
			si->midtexture = READINT32(save->p);

		si = &sides[li->sidenum[1]];
		if (diff2 & LD_S2TEXOFF)
			si->textureoffset = READFIXED(save->p);
		if (diff2 & LD_S2TOPTEX)
			si->toptexture = READINT32(save->p);
		if (diff2 & LD_S2BOTTEX)
			si->bottomtexture = READINT32(save->p);
		if (diff2 & LD_S2MIDTEX)
			si->midtexture = READINT32(save->p);
		if (diff2 & LD_ARGS)
		{
			UINT8 j;
			for (j = 0; j < NUM_SCRIPT_ARGS; j++)
				li->args[j] = READINT32(save->p);
		}
		if (diff2 & LD_STRINGARGS)
		{
			UINT8 j;
			for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
			{
				size_t len = READINT32(save->p);
				size_t k;

				if (!len)
				{
					Z_Free(li->stringargs[j]);
					li->stringargs[j] = NULL;
					continue;
				}

				li->stringargs[j] = Z_Realloc(li->stringargs[j], len + 1, PU_LEVEL, NULL);
				for (k = 0; k < len; k++)
					li->stringargs[j][k] = READCHAR(save->p);
				li->stringargs[j][len] = '\0';
			}
		}
		if (diff2 & LD_EXECUTORDELAY)
			li->executordelay = READINT32(save->p);
		if (diff3 & LD_ACTIVATION)
			li->activation = READUINT32(save->p);

	}
}

static void P_NetArchiveWorld(savebuffer_t *save)
{
	// initialize colormap vars because paranoia
	ClearNetColormaps();

	WRITEUINT32(save->p, ARCHIVEBLOCK_WORLD);

	ArchiveSectors(save);
	ArchiveLines(save);
	R_ClearTextureNumCache(false);
}

static void P_NetUnArchiveWorld(savebuffer_t *save)
{
	UINT16 i;

	if (READUINT32(save->p) != ARCHIVEBLOCK_WORLD)
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

	UnArchiveSectors(save);
	UnArchiveLines(save);
}

//
// Thinkers
//

static boolean P_ThingArgsEqual(const mobj_t *mobj, const mapthing_t *mapthing)
{
	UINT8 i;
	for (i = 0; i < NUM_MAPTHING_ARGS; i++)
		if (mobj->args[i] != mapthing->args[i])
			return false;

	return true;
}

static boolean P_ThingStringArgsEqual(const mobj_t *mobj, const mapthing_t *mapthing)
{
	UINT8 i;
	for (i = 0; i < NUM_MAPTHING_STRINGARGS; i++)
	{
		if (!mobj->stringargs[i])
			return !mapthing->stringargs[i];

		if (strcmp(mobj->stringargs[i], mapthing->stringargs[i]))
			return false;
	}

	return true;
}

static boolean P_ThingScriptEqual(const mobj_t *mobj, const mapthing_t *mapthing)
{
	UINT8 i;
	if (mobj->special != mapthing->special)
		return false;

	for (i = 0; i < NUM_SCRIPT_ARGS; i++)
		if (mobj->script_args[i] != mapthing->script_args[i])
			return false;

	for (i = 0; i < NUM_SCRIPT_STRINGARGS; i++)
	{
		if (!mobj->script_stringargs[i])
			return !mapthing->script_stringargs[i];

		if (strcmp(mobj->script_stringargs[i], mapthing->script_stringargs[i]))
			return false;
	}

	return true;
}

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
	MD_ARGS        = 1<<29,
	MD_STRINGARGS  = 1<<30,
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
	MD2_TID          = 1<<17,
	MD2_SPRITESCALE  = 1<<18,
	MD2_SPRITEOFFSET = 1<<19,
	MD2_WORLDOFFSET  = 1<<20,
	MD2_SPECIAL      = 1<<21,
	MD2_FLOORSPRITESLOPE = 1<<22,
	MD2_DISPOFFSET   = 1<<23,
	MD2_BOSS3CAP  = 1<<24,
	MD2_WAYPOINTCAP  = 1<<25,
	MD2_KITEMCAP     = 1<<26,
	MD2_ITNEXT       = 1<<27,
	MD2_LASTMOMZ     = 1<<28,
	MD2_TERRAIN      = 1<<29,
	MD2_LIGHTLEVEL   = (INT32)(1U<<30),
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

static void SaveMobjThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const mobj_t *mobj = (const mobj_t *)th;
	UINT32 diff;
	UINT32 diff2;
	size_t j;

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

	diff2 = 0;

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
		
		if (!P_ThingArgsEqual(mobj, mobj->spawnpoint))
			diff |= MD_ARGS;

		if (!P_ThingStringArgsEqual(mobj, mobj->spawnpoint))
			diff |= MD_STRINGARGS;

		if (!P_ThingScriptEqual(mobj, mobj->spawnpoint))
			diff2 |= MD2_SPECIAL;
	}
	else
	{
		// not a map spawned thing, so make it from scratch
		diff = MD_POS | MD_TYPE;

		for (j = 0; j < NUM_MAPTHING_ARGS; j++)
		{
			if (mobj->args[j] != 0)
			{
				diff |= MD_ARGS;
				break;
			}
		}

		for (j = 0; j < NUM_MAPTHING_STRINGARGS; j++)
		{
			if (mobj->stringargs[j] != NULL)
			{
				diff |= MD_STRINGARGS;
				break;
			}
		}

		if (mobj->special != 0)
		{
			diff2 |= MD2_SPECIAL;
		}

		for (j = 0; j < NUM_SCRIPT_ARGS; j++)
		{
			if (mobj->script_args[j] != 0)
			{
				diff2 |= MD2_SPECIAL;
				break;
			}
		}

		for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
		{
			if (mobj->stringargs[j] != NULL)
			{
				diff2 |= MD2_SPECIAL;
				break;
			}
		}
	}

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
	if (mobj->tid != 0)
		diff2 |= MD2_TID;
	if (mobj->spritexscale != FRACUNIT || mobj->spriteyscale != FRACUNIT)
		diff2 |= MD2_SPRITESCALE;
	if (mobj->spritexoffset || mobj->spriteyoffset)
		diff2 |= MD2_SPRITEOFFSET;
	if (mobj->sprxoff || mobj->spryoff || mobj->sprzoff)
		diff2 |= MD2_WORLDOFFSET;
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
	if (mobj->lightlevel)
		diff2 |= MD2_LIGHTLEVEL;
	if (mobj->dispoffset)
		diff2 |= MD2_DISPOFFSET;
	if (mobj == boss3cap)
		diff2 |= MD2_BOSS3CAP;
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

	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, diff);
	if (diff & MD_MORE)
		WRITEUINT32(save->p, diff2);

	// save pointer, at load time we will search this pointer to reinitilize pointers
	WRITEUINT32(save->p, (size_t)mobj);

	WRITEFIXED(save->p, mobj->z); // Force this so 3dfloor problems don't arise.
	WRITEFIXED(save->p, mobj->floorz);
	WRITEFIXED(save->p, mobj->ceilingz);

	if (diff2 & MD2_FLOORROVER)
	{
		WRITEUINT32(save->p, SaveSector(mobj->floorrover->target));
		WRITEUINT16(save->p, P_GetFFloorID(mobj->floorrover));
	}

	if (diff2 & MD2_CEILINGROVER)
	{
		WRITEUINT32(save->p, SaveSector(mobj->ceilingrover->target));
		WRITEUINT16(save->p, P_GetFFloorID(mobj->ceilingrover));
	}

	if (diff & MD_SPAWNPOINT)
	{
		size_t z;

		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] != mobj->spawnpoint)
				continue;
			WRITEUINT16(save->p, z);
			break;
		}
		if (mobj->type == MT_HOOPCENTER)
			return;
	}

	if (diff & MD_TYPE)
		WRITEUINT32(save->p, mobj->type);
	if (diff & MD_POS)
	{
		WRITEFIXED(save->p, mobj->x);
		WRITEFIXED(save->p, mobj->y);
		WRITEANGLE(save->p, mobj->angle);
		WRITEANGLE(save->p, mobj->pitch);
		WRITEANGLE(save->p, mobj->roll);
	}
	if (diff & MD_MOM)
	{
		WRITEFIXED(save->p, mobj->momx);
		WRITEFIXED(save->p, mobj->momy);
		WRITEFIXED(save->p, mobj->momz);
		WRITEFIXED(save->p, mobj->pmomz);
	}
	if (diff & MD_RADIUS)
		WRITEFIXED(save->p, mobj->radius);
	if (diff & MD_HEIGHT)
		WRITEFIXED(save->p, mobj->height);
	if (diff & MD_FLAGS)
		WRITEUINT32(save->p, mobj->flags);
	if (diff & MD_FLAGS2)
		WRITEUINT32(save->p, mobj->flags2);
	if (diff & MD_HEALTH)
		WRITEINT32(save->p, mobj->health);
	if (diff & MD_RTIME)
		WRITEINT32(save->p, mobj->reactiontime);
	if (diff & MD_STATE)
		WRITEUINT16(save->p, mobj->state-states);
	if (diff & MD_TICS)
		WRITEINT32(save->p, mobj->tics);
	if (diff & MD_SPRITE) {
		WRITEUINT16(save->p, mobj->sprite);
		if (mobj->sprite == SPR_PLAY)
			WRITEUINT8(save->p, mobj->sprite2);
	}
	if (diff & MD_FRAME)
	{
		WRITEUINT32(save->p, mobj->frame);
		WRITEUINT16(save->p, mobj->anim_duration);
	}
	if (diff & MD_EFLAGS)
		WRITEUINT16(save->p, mobj->eflags);
	if (diff & MD_PLAYER)
		WRITEUINT8(save->p, mobj->player-players);
	if (diff & MD_MOVEDIR)
		WRITEANGLE(save->p, mobj->movedir);
	if (diff & MD_MOVECOUNT)
		WRITEINT32(save->p, mobj->movecount);
	if (diff & MD_THRESHOLD)
		WRITEINT32(save->p, mobj->threshold);
	if (diff & MD_LASTLOOK)
		WRITEINT32(save->p, mobj->lastlook);
	if (diff & MD_TARGET)
		WRITEUINT32(save->p, mobj->target->mobjnum);
	if (diff & MD_TRACER)
		WRITEUINT32(save->p, mobj->tracer->mobjnum);
	if (diff & MD_FRICTION)
		WRITEFIXED(save->p, mobj->friction);
	if (diff & MD_MOVEFACTOR)
		WRITEFIXED(save->p, mobj->movefactor);
	if (diff & MD_FUSE)
		WRITEINT32(save->p, mobj->fuse);
	if (diff & MD_WATERTOP)
		WRITEFIXED(save->p, mobj->watertop);
	if (diff & MD_WATERBOTTOM)
		WRITEFIXED(save->p, mobj->waterbottom);
	if (diff & MD_SCALE)
		WRITEFIXED(save->p, mobj->scale);
	if (diff & MD_DSCALE)
		WRITEFIXED(save->p, mobj->destscale);
	if (diff2 & MD2_SCALESPEED)
		WRITEFIXED(save->p, mobj->scalespeed);
	if (diff & MD_ARGS)
	{
		for (j = 0; j < NUM_MAPTHING_ARGS; j++)
			WRITEINT32(save->p, mobj->args[j]);
	}
	if (diff & MD_STRINGARGS)
	{
		for (j = 0; j < NUM_MAPTHING_STRINGARGS; j++)
		{
			size_t len, k;

			if (!mobj->stringargs[j])
			{
				WRITEINT32(save->p, 0);
				continue;
			}

			len = strlen(mobj->stringargs[j]);
			WRITEINT32(save->p, len);
			for (k = 0; k < len; k++)
				WRITECHAR(save->p, mobj->stringargs[j][k]);
		}
	}
	if (diff2 & MD2_CUSVAL)
		WRITEINT32(save->p, mobj->cusval);
	if (diff2 & MD2_CVMEM)
		WRITEINT32(save->p, mobj->cvmem);
	if (diff2 & MD2_SKIN)
		WRITEUINT8(save->p, (UINT8)((skin_t *)mobj->skin - skins));
	if (diff2 & MD2_COLOR)
		WRITEUINT16(save->p, mobj->color);
	if (diff2 & MD2_EXTVAL1)
		WRITEINT32(save->p, mobj->extravalue1);
	if (diff2 & MD2_EXTVAL2)
		WRITEINT32(save->p, mobj->extravalue2);
	if (diff2 & MD2_HNEXT)
		WRITEUINT32(save->p, mobj->hnext->mobjnum);
	if (diff2 & MD2_HPREV)
		WRITEUINT32(save->p, mobj->hprev->mobjnum);
	if (diff2 & MD2_ITNEXT)
		WRITEUINT32(save->p, mobj->itnext->mobjnum);
	if (diff2 & MD2_SLOPE)
		WRITEUINT16(save->p, mobj->standingslope->id);
	if (diff2 & MD2_COLORIZED)
		WRITEUINT8(save->p, mobj->colorized);
	if (diff2 & MD2_MIRRORED)
		WRITEUINT8(save->p, mobj->mirrored);
	if (diff2 & MD2_ROLLANGLE)
		WRITEANGLE(save->p, mobj->rollangle);
	if (diff2 & MD2_SHADOWSCALE)
	{
		WRITEFIXED(save->p, mobj->shadowscale);
		WRITEUINT8(save->p, mobj->whiteshadow);
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

		WRITEUINT32(save->p, rf);
	}
	if (diff2 & MD2_TID)
		WRITEINT16(save->p, mobj->tid);
	if (diff2 & MD2_SPRITESCALE)
	{
		WRITEFIXED(save->p, mobj->spritexscale);
		WRITEFIXED(save->p, mobj->spriteyscale);
	}
	if (diff2 & MD2_SPRITEOFFSET)
	{
		WRITEFIXED(save->p, mobj->spritexoffset);
		WRITEFIXED(save->p, mobj->spriteyoffset);
	}
	if (diff2 & MD2_WORLDOFFSET)
	{
		WRITEFIXED(save->p, mobj->sprxoff);
		WRITEFIXED(save->p, mobj->spryoff);
		WRITEFIXED(save->p, mobj->sprzoff);
	}
	if (diff2 & MD2_SPECIAL)
	{
		WRITEINT16(save->p, mobj->special);

		for (j = 0; j < NUM_SCRIPT_ARGS; j++)
			WRITEINT32(save->p, mobj->script_args[j]);

		for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
		{
			size_t len, k;

			if (!mobj->script_stringargs[j])
			{
				WRITEINT32(save->p, 0);
				continue;
			}

			len = strlen(mobj->script_stringargs[j]);
			WRITEINT32(save->p, len);
			for (k = 0; k < len; k++)
				WRITECHAR(save->p, mobj->script_stringargs[j][k]);
		}
	}
	if (diff2 & MD2_FLOORSPRITESLOPE)
	{
		pslope_t *slope = mobj->floorspriteslope;

		WRITEFIXED(save->p, slope->zdelta);
		WRITEANGLE(save->p, slope->zangle);
		WRITEANGLE(save->p, slope->xydirection);

		WRITEFIXED(save->p, slope->o.x);
		WRITEFIXED(save->p, slope->o.y);
		WRITEFIXED(save->p, slope->o.z);

		WRITEFIXED(save->p, slope->d.x);
		WRITEFIXED(save->p, slope->d.y);

		WRITEFIXED(save->p, slope->normal.x);
		WRITEFIXED(save->p, slope->normal.y);
		WRITEFIXED(save->p, slope->normal.z);
		
	}
	if (diff2 & MD2_LIGHTLEVEL)
	{
		WRITEINT16(save->p, mobj->lightlevel);
	}
	if (diff2 & MD2_DISPOFFSET)
	{
		WRITEINT32(save->p, mobj->dispoffset);
	}
	if (diff2 & MD2_LASTMOMZ)
	{
		WRITEINT32(save->p, mobj->lastmomz);
	}
	if (diff2 & MD2_TERRAIN)
	{
		WRITEUINT32(save->p, K_GetTerrainHeapIndex(mobj->terrain));
		WRITEUINT32(save->p, SaveMobjnum(mobj->terrainOverlay));
	}

	WRITEUINT32(save->p, mobj->mobjnum);
}

static void SaveNoEnemiesThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const noenemies_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
}

static void SaveBounceCheeseThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const bouncecheese_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->distance);
	WRITEFIXED(save->p, ht->floorwasheight);
	WRITEFIXED(save->p, ht->ceilingwasheight);
	WRITECHAR(save->p, ht->low);
}

static void SaveContinuousFallThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const continuousfall_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->speed);
	WRITEINT32(save->p, ht->direction);
	WRITEFIXED(save->p, ht->floorstartheight);
	WRITEFIXED(save->p, ht->ceilingstartheight);
	WRITEFIXED(save->p, ht->destheight);
}

static void SaveMarioBlockThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const mariothink_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->speed);
	WRITEINT32(save->p, ht->direction);
	WRITEFIXED(save->p, ht->floorstartheight);
	WRITEFIXED(save->p, ht->ceilingstartheight);
	WRITEINT16(save->p, ht->tag);
}

static void SaveMarioCheckThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const mariocheck_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT32(save->p, SaveSector(ht->sector));
}

static void SaveThwompThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const thwomp_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->crushspeed);
	WRITEFIXED(save->p, ht->retractspeed);
	WRITEINT32(save->p, ht->direction);
	WRITEFIXED(save->p, ht->floorstartheight);
	WRITEFIXED(save->p, ht->ceilingstartheight);
	WRITEINT32(save->p, ht->delay);
	WRITEINT16(save->p, ht->tag);
	WRITEUINT16(save->p, ht->sound);
	WRITEINT32(save->p, ht->initDelay);
}

static void SaveFloatThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const floatthink_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT16(save->p, ht->tag);
}

static void SaveEachTimeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const eachtime_t *ht  = (const void *)th;
	size_t i;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		WRITECHAR(save->p, ht->playersInArea[i]);
	}
	WRITECHAR(save->p, ht->triggerOnExit);
}

static void SaveRaiseThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const raise_t *ht  = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT16(save->p, ht->tag);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->ceilingbottom);
	WRITEFIXED(save->p, ht->ceilingtop);
	WRITEFIXED(save->p, ht->basespeed);
	WRITEFIXED(save->p, ht->extraspeed);
	WRITEUINT8(save->p, ht->shaketimer);
	WRITEUINT8(save->p, ht->flags);
}

static void SaveCeilingThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const ceiling_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->bottomheight);
	WRITEFIXED(save->p, ht->topheight);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->delay);
	WRITEFIXED(save->p, ht->delaytimer);
	WRITEUINT8(save->p, ht->crush);
	WRITEINT32(save->p, ht->texture);
	WRITEINT32(save->p, ht->direction);
	WRITEINT16(save->p, ht->tag);
	WRITEFIXED(save->p, ht->sourceline);
	WRITEFIXED(save->p, ht->origspeed);
	WRITEFIXED(save->p, ht->crushHeight);
	WRITEFIXED(save->p, ht->crushSpeed);
	WRITEFIXED(save->p, ht->returnHeight);
	WRITEFIXED(save->p, ht->returnSpeed);
}

static void SaveFloormoveThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const floormove_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT8(save->p, ht->crush);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->direction);
	WRITEINT32(save->p, ht->texture);
	WRITEFIXED(save->p, ht->floordestheight);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->origspeed);
	WRITEFIXED(save->p, ht->delay);
	WRITEFIXED(save->p, ht->delaytimer);
	WRITEINT16(save->p, ht->tag);
	WRITEFIXED(save->p, ht->sourceline);
	WRITEFIXED(save->p, ht->crushHeight);
	WRITEFIXED(save->p, ht->crushSpeed);
	WRITEFIXED(save->p, ht->returnHeight);
	WRITEFIXED(save->p, ht->returnSpeed);
}

static void SaveLightflashThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const lightflash_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->maxlight);
	WRITEINT32(save->p, ht->minlight);
}

static void SaveStrobeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const strobe_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->count);
	WRITEINT16(save->p, ht->minlight);
	WRITEINT16(save->p, ht->maxlight);
	WRITEINT32(save->p, ht->darktime);
	WRITEINT32(save->p, ht->brighttime);
}

static void SaveGlowThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const glow_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT16(save->p, ht->minlight);
	WRITEINT16(save->p, ht->maxlight);
	WRITEINT16(save->p, ht->direction);
	WRITEINT16(save->p, ht->speed);
}

static inline void SaveFireflickerThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const fireflicker_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->count);
	WRITEINT32(save->p, ht->resetcount);
	WRITEINT16(save->p, ht->maxlight);
	WRITEINT16(save->p, ht->minlight);
}

static void SaveElevatorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const elevator_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEUINT32(save->p, SaveSector(ht->actionsector));
	WRITEINT32(save->p, ht->direction);
	WRITEFIXED(save->p, ht->floordestheight);
	WRITEFIXED(save->p, ht->ceilingdestheight);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->origspeed);
	WRITEFIXED(save->p, ht->low);
	WRITEFIXED(save->p, ht->high);
	WRITEFIXED(save->p, ht->distance);
	WRITEFIXED(save->p, ht->delay);
	WRITEFIXED(save->p, ht->delaytimer);
	WRITEFIXED(save->p, ht->floorwasheight);
	WRITEFIXED(save->p, ht->ceilingwasheight);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
}

static void SaveCrumbleThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const crumble_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEUINT32(save->p, SaveSector(ht->actionsector));
	WRITEUINT32(save->p, SavePlayer(ht->player)); // was dummy
	WRITEINT32(save->p, ht->direction);
	WRITEINT32(save->p, ht->origalpha);
	WRITEINT32(save->p, ht->timer);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->floorwasheight);
	WRITEFIXED(save->p, ht->ceilingwasheight);
	WRITEUINT8(save->p, ht->flags);
}

static inline void SaveScrollThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const scroll_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEFIXED(save->p, ht->dx);
	WRITEFIXED(save->p, ht->dy);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->control);
	WRITEFIXED(save->p, ht->last_height);
	WRITEFIXED(save->p, ht->vdx);
	WRITEFIXED(save->p, ht->vdy);
	WRITEINT32(save->p, ht->accel);
	WRITEINT32(save->p, ht->exclusive);
	WRITEUINT8(save->p, ht->type);
}

static inline void SaveFrictionThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const friction_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->friction);
	WRITEINT32(save->p, ht->movefactor);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->referrer);
	WRITEUINT8(save->p, ht->roverfriction);
}

static inline void SavePusherThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const pusher_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEFIXED(save->p, ht->x_mag);
	WRITEFIXED(save->p, ht->y_mag);
	WRITEFIXED(save->p, ht->z_mag);
	WRITEINT32(save->p, ht->affectee);
	WRITEUINT8(save->p, ht->roverpusher);
	WRITEINT32(save->p, ht->referrer);
	WRITEINT32(save->p, ht->exclusive);
	WRITEINT32(save->p, ht->slider);
}

static void SaveLaserThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const laserthink_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT16(save->p, ht->tag);
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT8(save->p, ht->nobosses);
}

static void SaveLightlevelThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const lightlevel_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT16(save->p, ht->sourcelevel);
	WRITEINT16(save->p, ht->destlevel);
	WRITEFIXED(save->p, ht->fixedcurlevel);
	WRITEFIXED(save->p, ht->fixedpertic);
	WRITEINT32(save->p, ht->timer);
}

static void SaveExecutorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const executor_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->line));
	WRITEUINT32(save->p, SaveMobjnum(ht->caller));
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->timer);
}

static void SaveDisappearThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const disappear_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, ht->appeartime);
	WRITEUINT32(save->p, ht->disappeartime);
	WRITEUINT32(save->p, ht->offset);
	WRITEUINT32(save->p, ht->timer);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->sourceline);
	WRITEINT32(save->p, ht->exists);
}

static void SaveFadeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const fade_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, CheckAddNetColormapToList(ht->dest_exc));
	WRITEUINT32(save->p, ht->sectornum);
	WRITEUINT32(save->p, ht->ffloornum);
	WRITEINT32(save->p, ht->alpha);
	WRITEINT16(save->p, ht->sourcevalue);
	WRITEINT16(save->p, ht->destvalue);
	WRITEINT16(save->p, ht->destlightlevel);
	WRITEINT16(save->p, ht->speed);
	WRITEUINT8(save->p, (UINT8)ht->ticbased);
	WRITEINT32(save->p, ht->timer);
	WRITEUINT8(save->p, ht->doexists);
	WRITEUINT8(save->p, ht->dotranslucent);
	WRITEUINT8(save->p, ht->dolighting);
	WRITEUINT8(save->p, ht->docolormap);
	WRITEUINT8(save->p, ht->docollision);
	WRITEUINT8(save->p, ht->doghostfade);
	WRITEUINT8(save->p, ht->exactalpha);
}

static void SaveFadeColormapThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const fadecolormap_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEUINT32(save->p, CheckAddNetColormapToList(ht->source_exc));
	WRITEUINT32(save->p, CheckAddNetColormapToList(ht->dest_exc));
	WRITEUINT8(save->p, (UINT8)ht->ticbased);
	WRITEINT32(save->p, ht->duration);
	WRITEINT32(save->p, ht->timer);
}

static void SavePlaneDisplaceThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const planedisplace_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->control);
	WRITEFIXED(save->p, ht->last_height);
	WRITEFIXED(save->p, ht->speed);
	WRITEUINT8(save->p, ht->type);
}

static inline void SaveDynamicLineSlopeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const dynlineplanethink_t* ht = (const void*)th;

	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT32(save->p, SaveSlope(ht->slope));
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEFIXED(save->p, ht->extent);
}

static inline void SaveDynamicVertexSlopeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	size_t i;
	const dynvertexplanethink_t* ht = (const void*)th;

	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSlope(ht->slope));
	for (i = 0; i < 3; i++)
		WRITEUINT32(save->p, SaveSector(ht->secs[i]));
	WRITEMEM(save->p, ht->vex, sizeof(ht->vex));
	WRITEMEM(save->p, ht->origsecheights, sizeof(ht->origsecheights));
	WRITEMEM(save->p, ht->origvecheights, sizeof(ht->origvecheights));
	WRITEUINT8(save->p, ht->relative);
}

static inline void SavePolyrotatetThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyrotate_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->distance);
	WRITEUINT8(save->p, ht->turnobjs);
}

static void SavePolymoveThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polymove_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->speed);
	WRITEFIXED(save->p, ht->momx);
	WRITEFIXED(save->p, ht->momy);
	WRITEINT32(save->p, ht->distance);
	WRITEANGLE(save->p, ht->angle);
}

static void SavePolywaypointThinker(savebuffer_t *save, const thinker_t *th, UINT8 type)
{
	const polywaypoint_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->sequence);
	WRITEINT32(save->p, ht->pointnum);
	WRITEINT32(save->p, ht->direction);
	WRITEUINT8(save->p, ht->returnbehavior);
	WRITEUINT8(save->p, ht->continuous);
	WRITEUINT8(save->p, ht->stophere);
}

static void SavePolyslidedoorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyslidedoor_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->delay);
	WRITEINT32(save->p, ht->delayCount);
	WRITEINT32(save->p, ht->initSpeed);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->initDistance);
	WRITEINT32(save->p, ht->distance);
	WRITEUINT32(save->p, ht->initAngle);
	WRITEUINT32(save->p, ht->angle);
	WRITEUINT32(save->p, ht->revAngle);
	WRITEFIXED(save->p, ht->momx);
	WRITEFIXED(save->p, ht->momy);
	WRITEUINT8(save->p, ht->closing);
}

static void SavePolyswingdoorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyswingdoor_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->delay);
	WRITEINT32(save->p, ht->delayCount);
	WRITEINT32(save->p, ht->initSpeed);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->initDistance);
	WRITEINT32(save->p, ht->distance);
	WRITEUINT8(save->p, ht->closing);
}

static void SavePolydisplaceThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polydisplace_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEUINT32(save->p, SaveSector(ht->controlSector));
	WRITEFIXED(save->p, ht->dx);
	WRITEFIXED(save->p, ht->dy);
	WRITEFIXED(save->p, ht->oldHeights);
}

static void SavePolyrotdisplaceThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyrotdisplace_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEUINT32(save->p, SaveSector(ht->controlSector));
	WRITEFIXED(save->p, ht->rotscale);
	WRITEUINT8(save->p, ht->turnobjs);
	WRITEFIXED(save->p, ht->oldHeights);
}

static void SavePolyfadeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyfade_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->sourcevalue);
	WRITEINT32(save->p, ht->destvalue);
	WRITEUINT8(save->p, (UINT8)ht->docollision);
	WRITEUINT8(save->p, (UINT8)ht->doghostfade);
	WRITEUINT8(save->p, (UINT8)ht->ticbased);
	WRITEINT32(save->p, ht->duration);
	WRITEINT32(save->p, ht->timer);
}

static void P_NetArchiveThinkers(savebuffer_t *save)
{
	const thinker_t *th;
	UINT32 i;

	WRITEUINT32(save->p, ARCHIVEBLOCK_THINKERS);

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
				SaveMobjThinker(save, th, tc_mobj);
				continue;
			}
	#ifdef PARANOIA
			else if (th->function.acp1 == (actionf_p1)P_NullPrecipThinker);
	#endif
			else if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
			{
				SaveCeilingThinker(save, th, tc_ceiling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_CrushCeiling)
			{
				SaveCeilingThinker(save, th, tc_crushceiling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MoveFloor)
			{
				SaveFloormoveThinker(save, th, tc_floor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LightningFlash)
			{
				SaveLightflashThinker(save, th, tc_flash);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
			{
				SaveStrobeThinker(save, th, tc_strobe);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Glow)
			{
				SaveGlowThinker(save, th, tc_glow);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FireFlicker)
			{
				SaveFireflickerThinker(save, th, tc_fireflicker);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MoveElevator)
			{
				SaveElevatorThinker(save, th, tc_elevator);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ContinuousFalling)
			{
				SaveContinuousFallThinker(save, th, tc_continuousfalling);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ThwompSector)
			{
				SaveThwompThinker(save, th, tc_thwomp);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_NoEnemiesSector)
			{
				SaveNoEnemiesThinker(save, th, tc_noenemies);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_EachTimeThinker)
			{
				SaveEachTimeThinker(save, th, tc_eachtime);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_RaiseSector)
			{
				SaveRaiseThinker(save, th, tc_raisesector);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_CameraScanner)
			{
				SaveElevatorThinker(save, th, tc_camerascanner);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Scroll)
			{
				SaveScrollThinker(save, th, tc_scroll);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Friction)
			{
				SaveFrictionThinker(save, th, tc_friction);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Pusher)
			{
				SavePusherThinker(save, th, tc_pusher);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_BounceCheese)
			{
				SaveBounceCheeseThinker(save, th, tc_bouncecheese);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_StartCrumble)
			{
				SaveCrumbleThinker(save, th, tc_startcrumble);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MarioBlock)
			{
				SaveMarioBlockThinker(save, th, tc_marioblock);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_MarioBlockChecker)
			{
				SaveMarioCheckThinker(save, th, tc_marioblockchecker);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FloatSector)
			{
				SaveFloatThinker(save, th, tc_floatsector);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LaserFlash)
			{
				SaveLaserThinker(save, th, tc_laserflash);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_LightFade)
			{
				SaveLightlevelThinker(save, th, tc_lightfade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_ExecutorDelay)
			{
				SaveExecutorThinker(save, th, tc_executor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Disappear)
			{
				SaveDisappearThinker(save, th, tc_disappear);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_Fade)
			{
				SaveFadeThinker(save, th, tc_fade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_FadeColormap)
			{
				SaveFadeColormapThinker(save, th, tc_fadecolormap);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PlaneDisplace)
			{
				SavePlaneDisplaceThinker(save, th, tc_planedisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjRotate)
			{
				SavePolyrotatetThinker(save, th, tc_polyrotate);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjMove)
			{
				SavePolymoveThinker(save, th, tc_polymove);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjWaypoint)
			{
				SavePolywaypointThinker(save, th, tc_polywaypoint);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyDoorSlide)
			{
				SavePolyslidedoorThinker(save, th, tc_polyslidedoor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyDoorSwing)
			{
				SavePolyswingdoorThinker(save, th, tc_polyswingdoor);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjFlag)
			{
				SavePolymoveThinker(save, th, tc_polyflag);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjDisplace)
			{
				SavePolydisplaceThinker(save, th, tc_polydisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjRotDisplace)
			{
				SavePolyrotdisplaceThinker(save, th, tc_polyrotdisplace);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_PolyObjFade)
			{
				SavePolyfadeThinker(save, th, tc_polyfade);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_DynamicSlopeLine)
			{
				SaveDynamicLineSlopeThinker(save, th, tc_dynslopeline);
				continue;
			}
			else if (th->function.acp1 == (actionf_p1)T_DynamicSlopeVert)
			{
				SaveDynamicVertexSlopeThinker(save, th, tc_dynslopevert);
				continue;
			}
#ifdef PARANOIA
			else if (th->function.acp1 != (actionf_p1)P_RemoveThinkerDelayed) // wait garbage collection
				I_Error("unknown thinker type %p", th->function.acp1);
#endif
		}

		CONS_Debug(DBG_NETPLAY, "%u thinkers saved in list %d\n", numsaved, i);

		WRITEUINT8(save->p, tc_end);
	}
}

static void P_NetArchiveWaypoints(savebuffer_t *save)
{
	waypoint_t *waypoint;
	size_t i;
	size_t numWaypoints = K_GetNumWaypoints();

	WRITEUINT32(save->p, ARCHIVEBLOCK_WAYPOINTS);
	WRITEUINT32(save->p, numWaypoints);

	for (i = 0U; i < numWaypoints; i++) {
		waypoint = K_GetWaypointFromIndex(i);

		// The only thing we save for each waypoint is the mobj.
		// Waypoints should NEVER be completely created or destroyed mid-race as a result of this
		WRITEUINT32(save->p, waypoint->mobj->mobjnum);
	}
}

static void P_NetUnArchiveWaypoints(savebuffer_t *save)
{
	if (READUINT32(save->p) != ARCHIVEBLOCK_WAYPOINTS)
		I_Error("Bad $$$.sav at archive block Waypoints!");
	else {
		UINT32 numArchiveWaypoints = READUINT32(save->p);
		size_t numSpawnedWaypoints = K_GetNumWaypoints();

		if (numArchiveWaypoints != numSpawnedWaypoints) {
			I_Error("Bad $$$.sav: More saved waypoints than created!");
		} else {
			waypoint_t *waypoint;
			UINT32 i;
			UINT32 temp;
			for (i = 0U; i < numArchiveWaypoints; i++) {
				waypoint = K_GetWaypointFromIndex(i);
				temp = READUINT32(save->p);
				waypoint->mobj = NULL;
				if (!P_SetTarget(&waypoint->mobj, P_FindNewPosition(temp))) {
					CONS_Debug(DBG_GAMELOGIC, "waypoint mobj not found for %d\n", i);
				}
			}
		}
	}
}

static void P_NetArchiveTubeWaypoints(savebuffer_t *save)
{
	INT32 i, j;

	for (i = 0; i < NUMTUBEWAYPOINTSEQUENCES; i++)
	{
		WRITEUINT16(save->p, numtubewaypoints[i]);
		for (j = 0; j < numtubewaypoints[i]; j++)
			WRITEUINT32(save->p, tubewaypoints[i][j] ? tubewaypoints[i][j]->mobjnum : 0);
	}
}

static void P_NetUnArchiveTubeWaypoints(savebuffer_t *save)
{
	INT32 i, j;
	UINT32 mobjnum;

	for (i = 0; i < NUMTUBEWAYPOINTSEQUENCES; i++)
	{
		numtubewaypoints[i] = READUINT16(save->p);
		for (j = 0; j < numtubewaypoints[i]; j++)
		{
			mobjnum = READUINT32(save->p);
			tubewaypoints[i][j] = (mobjnum == 0) ? NULL : P_FindNewPosition(mobjnum);
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
	CONS_Debug(DBG_GAMELOGIC, "mobj %d not found\n", oldposition);
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

static thinker_t* LoadMobjThinker(savebuffer_t *save, actionf_p1 thinker)
{
	thinker_t *next;
	mobj_t *mobj;
	UINT32 diff;
	UINT32 diff2;
	INT32 i;
	fixed_t z, floorz, ceilingz;
	ffloor_t *floorrover = NULL, *ceilingrover = NULL;
	size_t j;

	diff = READUINT32(save->p);
	if (diff & MD_MORE)
		diff2 = READUINT32(save->p);
	else
		diff2 = 0;

	next = (void *)(size_t)READUINT32(save->p);

	z = READFIXED(save->p); // Force this so 3dfloor problems don't arise.
	floorz = READFIXED(save->p);
	ceilingz = READFIXED(save->p);

	if (diff2 & MD2_FLOORROVER)
	{
		sector_t *sec = LoadSector(READUINT32(save->p));
		UINT16 id = READUINT16(save->p);
		floorrover = P_GetFFloorByID(sec, id);
	}

	if (diff2 & MD2_CEILINGROVER)
	{
		sector_t *sec = LoadSector(READUINT32(save->p));
		UINT16 id = READUINT16(save->p);
		ceilingrover = P_GetFFloorByID(sec, id);
	}

	if (diff & MD_SPAWNPOINT)
	{
		UINT16 spawnpointnum = READUINT16(save->p);

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
		mobj->type = READUINT32(save->p);
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
		mobj->x = mobj->old_x = READFIXED(save->p);
		mobj->y = mobj->old_y = READFIXED(save->p);
		mobj->angle = mobj->old_angle = READANGLE(save->p);
		mobj->pitch = mobj->old_pitch = READANGLE(save->p);
		mobj->roll = mobj->old_roll = READANGLE(save->p);
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
		mobj->momx = READFIXED(save->p);
		mobj->momy = READFIXED(save->p);
		mobj->momz = READFIXED(save->p);
		mobj->pmomz = READFIXED(save->p);
	} // otherwise they're zero, and the memset took care of it

	if (diff & MD_RADIUS)
		mobj->radius = READFIXED(save->p);
	else
		mobj->radius = mobj->info->radius;
	if (diff & MD_HEIGHT)
		mobj->height = READFIXED(save->p);
	else
		mobj->height = mobj->info->height;
	if (diff & MD_FLAGS)
		mobj->flags = READUINT32(save->p);
	else
		mobj->flags = mobj->info->flags;
	if (diff & MD_FLAGS2)
		mobj->flags2 = READUINT32(save->p);
	if (diff & MD_HEALTH)
		mobj->health = READINT32(save->p);
	else
		mobj->health = mobj->info->spawnhealth;
	if (diff & MD_RTIME)
		mobj->reactiontime = READINT32(save->p);
	else
		mobj->reactiontime = mobj->info->reactiontime;

	if (diff & MD_STATE)
		mobj->state = &states[READUINT16(save->p)];
	else
		mobj->state = &states[mobj->info->spawnstate];
	if (diff & MD_TICS)
		mobj->tics = READINT32(save->p);
	else
		mobj->tics = mobj->state->tics;
	if (diff & MD_SPRITE) {
		mobj->sprite = READUINT16(save->p);
		if (mobj->sprite == SPR_PLAY)
			mobj->sprite2 = READUINT8(save->p);
	}
	else {
		mobj->sprite = mobj->state->sprite;
		if (mobj->sprite == SPR_PLAY)
			mobj->sprite2 = mobj->state->frame&FF_FRAMEMASK;
	}
	if (diff & MD_FRAME)
	{
		mobj->frame = READUINT32(save->p);
		mobj->anim_duration = READUINT16(save->p);
	}
	else
	{
		mobj->frame = mobj->state->frame;
		mobj->anim_duration = (UINT16)mobj->state->var2;
	}
	if (diff & MD_EFLAGS)
		mobj->eflags = READUINT16(save->p);
	if (diff & MD_PLAYER)
	{
		i = READUINT8(save->p);
		mobj->player = &players[i];
		mobj->player->mo = mobj;
	}
	if (diff & MD_MOVEDIR)
		mobj->movedir = READANGLE(save->p);
	if (diff & MD_MOVECOUNT)
		mobj->movecount = READINT32(save->p);
	if (diff & MD_THRESHOLD)
		mobj->threshold = READINT32(save->p);
	if (diff & MD_LASTLOOK)
		mobj->lastlook = READINT32(save->p);
	else
		mobj->lastlook = -1;
	if (diff & MD_TARGET)
		mobj->target = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff & MD_TRACER)
		mobj->tracer = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff & MD_FRICTION)
		mobj->friction = READFIXED(save->p);
	else
		mobj->friction = ORIG_FRICTION;
	if (diff & MD_MOVEFACTOR)
		mobj->movefactor = READFIXED(save->p);
	else
		mobj->movefactor = FRACUNIT;
	if (diff & MD_FUSE)
		mobj->fuse = READINT32(save->p);
	if (diff & MD_WATERTOP)
		mobj->watertop = READFIXED(save->p);
	if (diff & MD_WATERBOTTOM)
		mobj->waterbottom = READFIXED(save->p);
	if (diff & MD_SCALE)
		mobj->scale = READFIXED(save->p);
	else
		mobj->scale = FRACUNIT;
	if (diff & MD_DSCALE)
		mobj->destscale = READFIXED(save->p);
	else
		mobj->destscale = mobj->scale;
	if (diff2 & MD2_SCALESPEED)
		mobj->scalespeed = READFIXED(save->p);
	else
		mobj->scalespeed = mapobjectscale/12;
	if (diff & MD_ARGS)
	{
		for (j = 0; j < NUM_MAPTHING_ARGS; j++)
			mobj->args[j] = READINT32(save->p);
	}
	if (diff & MD_STRINGARGS)
	{
		for (j = 0; j < NUM_MAPTHING_STRINGARGS; j++)
		{
			size_t len = READINT32(save->p);
			size_t k;

			if (!len)
			{
				Z_Free(mobj->stringargs[j]);
				mobj->stringargs[j] = NULL;
				continue;
			}

			mobj->stringargs[j] = Z_Realloc(mobj->stringargs[j], len + 1, PU_LEVEL, NULL);
			for (k = 0; k < len; k++)
				mobj->stringargs[j][k] = READCHAR(save->p);
			mobj->stringargs[j][len] = '\0';
		}
	}
	if (diff2 & MD2_CUSVAL)
		mobj->cusval = READINT32(save->p);
	if (diff2 & MD2_CVMEM)
		mobj->cvmem = READINT32(save->p);
	if (diff2 & MD2_SKIN)
		mobj->skin = &skins[READUINT8(save->p)];
	if (diff2 & MD2_COLOR)
		mobj->color = READUINT16(save->p);
	if (diff2 & MD2_EXTVAL1)
		mobj->extravalue1 = READINT32(save->p);
	if (diff2 & MD2_EXTVAL2)
		mobj->extravalue2 = READINT32(save->p);
	if (diff2 & MD2_HNEXT)
		mobj->hnext = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff2 & MD2_HPREV)
		mobj->hprev = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff2 & MD2_ITNEXT)
		mobj->itnext = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff2 & MD2_SLOPE)
		mobj->standingslope = P_SlopeById(READUINT16(save->p));
	if (diff2 & MD2_COLORIZED)
		mobj->colorized = READUINT8(save->p);
	if (diff2 & MD2_MIRRORED)
		mobj->mirrored = READUINT8(save->p);
	if (diff2 & MD2_ROLLANGLE)
		mobj->rollangle = READANGLE(save->p);
	if (diff2 & MD2_SHADOWSCALE)
	{
		mobj->shadowscale = READFIXED(save->p);
		mobj->whiteshadow = READUINT8(save->p);
	}
	if (diff2 & MD2_RENDERFLAGS)
		mobj->renderflags = READUINT32(save->p);
	if (diff2 & MD2_TID)
		P_SetThingTID(mobj, READINT16(save->p));
	if (diff2 & MD2_SPRITESCALE)
	{
		mobj->spritexscale = READFIXED(save->p);
		mobj->spriteyscale = READFIXED(save->p);
	}
	else
	{
		mobj->spritexoffset = mobj->spriteyoffset = 0;
	}
	if (diff2 & MD2_SPRITEOFFSET)
	{
		mobj->spritexoffset = READFIXED(save->p);
		mobj->spriteyoffset = READFIXED(save->p);
	}
	else
	{
		mobj->spritexoffset = mobj->spriteyoffset = 0;
	}
	if (diff2 & MD2_WORLDOFFSET)
	{
		mobj->sprxoff = READFIXED(save->p);
		mobj->spryoff = READFIXED(save->p);
		mobj->sprzoff = READFIXED(save->p);
	}
	else
	{
		mobj->sprxoff = mobj->spryoff = mobj->sprzoff = 0;
	}
	if (diff2 & MD2_SPECIAL)
	{
		mobj->special = READINT16(save->p);

		for (j = 0; j < NUM_SCRIPT_ARGS; j++)
			mobj->script_args[j] = READINT32(save->p);

		for (j = 0; j < NUM_SCRIPT_STRINGARGS; j++)
		{
			size_t len = READINT32(save->p);
			size_t k;

			if (!len)
			{
				Z_Free(mobj->script_stringargs[j]);
				mobj->script_stringargs[j] = NULL;
				continue;
			}

			mobj->script_stringargs[j] = Z_Realloc(mobj->script_stringargs[j], len + 1, PU_LEVEL, NULL);
			for (k = 0; k < len; k++)
				mobj->script_stringargs[j][k] = READCHAR(save->p);
			mobj->script_stringargs[j][len] = '\0';
		}
	}
	if (diff2 & MD2_FLOORSPRITESLOPE)
	{
		pslope_t *slope = (pslope_t *)P_CreateFloorSpriteSlope(mobj);

		slope->zdelta = READFIXED(save->p);
		slope->zangle = READANGLE(save->p);
		slope->xydirection = READANGLE(save->p);

		slope->o.x = READFIXED(save->p);
		slope->o.y = READFIXED(save->p);
		slope->o.z = READFIXED(save->p);

		slope->d.x = READFIXED(save->p);
		slope->d.y = READFIXED(save->p);

		slope->normal.x = READFIXED(save->p);
		slope->normal.y = READFIXED(save->p);
		slope->normal.z = READFIXED(save->p);

		P_UpdateSlopeLightOffset(slope);
	}
	if (diff2 & MD2_LIGHTLEVEL)
	{
		mobj->lightlevel = READINT16(save->p);
	}
	if (diff2 & MD2_DISPOFFSET)
	{
		mobj->dispoffset = READINT32(save->p);
	}
	if (diff2 & MD2_LASTMOMZ)
	{
		mobj->lastmomz = READINT32(save->p);
	}
	if (diff2 & MD2_TERRAIN)
	{
		mobj->terrain = (terrain_t *)(size_t)READUINT32(save->p);
		mobj->terrainOverlay = (mobj_t *)(size_t)READUINT32(save->p);
	}
	else
	{
		mobj->terrain = NULL;
	}

	// set sprev, snext, bprev, bnext, subsector
	P_SetThingPosition(mobj);

	mobj->mobjnum = READUINT32(save->p);

	if (mobj->player)
	{
		if (mobj->eflags & MFE_VERTICALFLIP)
			mobj->player->viewz = mobj->z + mobj->height - mobj->player->viewheight;
		else
			mobj->player->viewz = mobj->player->mo->z + mobj->player->viewheight;
	}

	if (mobj->type == MT_SKYBOX && mobj->spawnpoint)
	{
		P_InitSkyboxPoint(mobj, mobj->spawnpoint);
	}

	if (diff2 & MD2_BOSS3CAP)
		P_SetTarget(&boss3cap, mobj);
	
	if (diff2 & MD2_WAYPOINTCAP)
		P_SetTarget(&waypointcap, mobj);

	if (diff2 & MD2_KITEMCAP)
		P_SetTarget(&kitemcap, mobj);

	mobj->info = (mobjinfo_t *)next; // temporarily, set when leave this function

	R_AddMobjInterpolator(mobj);

	return &mobj->thinker;
}

static thinker_t* LoadNoEnemiesThinker(savebuffer_t *save, actionf_p1 thinker)
{
	noenemies_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	return &ht->thinker;
}

static thinker_t* LoadBounceCheeseThinker(savebuffer_t *save, actionf_p1 thinker)
{
	bouncecheese_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	ht->speed = READFIXED(save->p);
	ht->distance = READFIXED(save->p);
	ht->floorwasheight = READFIXED(save->p);
	ht->ceilingwasheight = READFIXED(save->p);
	ht->low = READCHAR(save->p);

	if (ht->sector)
		ht->sector->ceilingdata = ht;

	return &ht->thinker;
}

static thinker_t* LoadContinuousFallThinker(savebuffer_t *save, actionf_p1 thinker)
{
	continuousfall_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->speed = READFIXED(save->p);
	ht->direction = READINT32(save->p);
	ht->floorstartheight = READFIXED(save->p);
	ht->ceilingstartheight = READFIXED(save->p);
	ht->destheight = READFIXED(save->p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadMarioBlockThinker(savebuffer_t *save, actionf_p1 thinker)
{
	mariothink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->speed = READFIXED(save->p);
	ht->direction = READINT32(save->p);
	ht->floorstartheight = READFIXED(save->p);
	ht->ceilingstartheight = READFIXED(save->p);
	ht->tag = READINT16(save->p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadMarioCheckThinker(savebuffer_t *save, actionf_p1 thinker)
{
	mariocheck_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	return &ht->thinker;
}

static thinker_t* LoadThwompThinker(savebuffer_t *save, actionf_p1 thinker)
{
	thwomp_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	ht->crushspeed = READFIXED(save->p);
	ht->retractspeed = READFIXED(save->p);
	ht->direction = READINT32(save->p);
	ht->floorstartheight = READFIXED(save->p);
	ht->ceilingstartheight = READFIXED(save->p);
	ht->delay = READINT32(save->p);
	ht->tag = READINT16(save->p);
	ht->sound = READUINT16(save->p);

	if (ht->sector)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadFloatThinker(savebuffer_t *save, actionf_p1 thinker)
{
	floatthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	ht->tag = READINT16(save->p);
	return &ht->thinker;
}

static thinker_t* LoadEachTimeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	size_t i;
	eachtime_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	for (i = 0; i < MAXPLAYERS; i++)
	{
		ht->playersInArea[i] = READCHAR(save->p);
	}
	ht->triggerOnExit = READCHAR(save->p);
	return &ht->thinker;
}

static thinker_t* LoadRaiseThinker(savebuffer_t *save, actionf_p1 thinker)
{
	raise_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->tag = READINT16(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->ceilingbottom = READFIXED(save->p);
	ht->ceilingtop = READFIXED(save->p);
	ht->basespeed = READFIXED(save->p);
	ht->extraspeed = READFIXED(save->p);
	ht->shaketimer = READUINT8(save->p);
	ht->flags = READUINT8(save->p);
	return &ht->thinker;
}

static thinker_t* LoadCeilingThinker(savebuffer_t *save, actionf_p1 thinker)
{
	ceiling_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->bottomheight = READFIXED(save->p);
	ht->topheight = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->delay = READFIXED(save->p);
	ht->delaytimer = READFIXED(save->p);
	ht->crush = READUINT8(save->p);
	ht->texture = READINT32(save->p);
	ht->direction = READINT32(save->p);
	ht->tag = READINT16(save->p);
	ht->sourceline = READFIXED(save->p);
	ht->origspeed = READFIXED(save->p);
	ht->crushHeight = READFIXED(save->p);
	ht->crushSpeed = READFIXED(save->p);
	ht->returnHeight = READFIXED(save->p);
	ht->returnSpeed = READFIXED(save->p);
	if (ht->sector)
		ht->sector->ceilingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadFloormoveThinker(savebuffer_t *save, actionf_p1 thinker)
{
	floormove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->crush = READUINT8(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->direction = READINT32(save->p);
	ht->texture = READINT32(save->p);
	ht->floordestheight = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->origspeed = READFIXED(save->p);
	ht->delay = READFIXED(save->p);
	ht->delaytimer = READFIXED(save->p);
	ht->tag = READINT16(save->p);
	ht->sourceline = READFIXED(save->p);
	ht->crushHeight = READFIXED(save->p);
	ht->crushSpeed = READFIXED(save->p);
	ht->returnHeight = READFIXED(save->p);
	ht->returnSpeed = READFIXED(save->p);
	if (ht->sector)
		ht->sector->floordata = ht;
	return &ht->thinker;
}

static thinker_t* LoadLightflashThinker(savebuffer_t *save, actionf_p1 thinker)
{
	lightflash_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->maxlight = READINT32(save->p);
	ht->minlight = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadStrobeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	strobe_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->count = READINT32(save->p);
	ht->minlight = READINT16(save->p);
	ht->maxlight = READINT16(save->p);
	ht->darktime = READINT32(save->p);
	ht->brighttime = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadGlowThinker(savebuffer_t *save, actionf_p1 thinker)
{
	glow_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->minlight = READINT16(save->p);
	ht->maxlight = READINT16(save->p);
	ht->direction = READINT16(save->p);
	ht->speed = READINT16(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadFireflickerThinker(savebuffer_t *save, actionf_p1 thinker)
{
	fireflicker_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->count = READINT32(save->p);
	ht->resetcount = READINT32(save->p);
	ht->maxlight = READINT16(save->p);
	ht->minlight = READINT16(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static thinker_t* LoadElevatorThinker(savebuffer_t *save, actionf_p1 thinker, boolean setplanedata)
{
	elevator_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->actionsector = LoadSector(READUINT32(save->p));
	ht->direction = READINT32(save->p);
	ht->floordestheight = READFIXED(save->p);
	ht->ceilingdestheight = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->origspeed = READFIXED(save->p);
	ht->low = READFIXED(save->p);
	ht->high = READFIXED(save->p);
	ht->distance = READFIXED(save->p);
	ht->delay = READFIXED(save->p);
	ht->delaytimer = READFIXED(save->p);
	ht->floorwasheight = READFIXED(save->p);
	ht->ceilingwasheight = READFIXED(save->p);
	ht->sourceline = LoadLine(READUINT32(save->p));

	if (ht->sector && setplanedata)
	{
		ht->sector->ceilingdata = ht;
		ht->sector->floordata = ht;
	}

	return &ht->thinker;
}

static thinker_t* LoadCrumbleThinker(savebuffer_t *save, actionf_p1 thinker)
{
	crumble_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	ht->actionsector = LoadSector(READUINT32(save->p));
	ht->player = LoadPlayer(READUINT32(save->p));
	ht->direction = READINT32(save->p);
	ht->origalpha = READINT32(save->p);
	ht->timer = READINT32(save->p);
	ht->speed = READFIXED(save->p);
	ht->floorwasheight = READFIXED(save->p);
	ht->ceilingwasheight = READFIXED(save->p);
	ht->flags = READUINT8(save->p);

	if (ht->sector)
		ht->sector->floordata = ht;

	return &ht->thinker;
}

static thinker_t* LoadScrollThinker(savebuffer_t *save, actionf_p1 thinker)
{
	scroll_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dx = READFIXED(save->p);
	ht->dy = READFIXED(save->p);
	ht->affectee = READINT32(save->p);
	ht->control = READINT32(save->p);
	ht->last_height = READFIXED(save->p);
	ht->vdx = READFIXED(save->p);
	ht->vdy = READFIXED(save->p);
	ht->accel = READINT32(save->p);
	ht->exclusive = READINT32(save->p);
	ht->type = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadFrictionThinker(savebuffer_t *save, actionf_p1 thinker)
{
	friction_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->friction = READINT32(save->p);
	ht->movefactor = READINT32(save->p);
	ht->affectee = READINT32(save->p);
	ht->referrer = READINT32(save->p);
	ht->roverfriction = READUINT8(save->p);
	return &ht->thinker;
}

static thinker_t* LoadPusherThinker(savebuffer_t *save, actionf_p1 thinker)
{
	pusher_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->x_mag = READFIXED(save->p);
	ht->y_mag = READFIXED(save->p);
	ht->z_mag = READFIXED(save->p);
	ht->affectee = READINT32(save->p);
	ht->roverpusher = READUINT8(save->p);
	ht->referrer = READINT32(save->p);
	ht->exclusive = READINT32(save->p);
	ht->slider = READINT32(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadLaserThinker(savebuffer_t *save, actionf_p1 thinker)
{
	laserthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->tag = READINT16(save->p);
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->nobosses = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadLightlevelThinker(savebuffer_t *save, actionf_p1 thinker)
{
	lightlevel_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->sourcelevel = READINT16(save->p);
	ht->destlevel = READINT16(save->p);
	ht->fixedcurlevel = READFIXED(save->p);
	ht->fixedpertic = READFIXED(save->p);
	ht->timer = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	return &ht->thinker;
}

static inline thinker_t* LoadExecutorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	executor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->line = LoadLine(READUINT32(save->p));
	ht->caller = LoadMobj(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	ht->timer = READINT32(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadDisappearThinker(savebuffer_t *save, actionf_p1 thinker)
{
	disappear_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->appeartime = READUINT32(save->p);
	ht->disappeartime = READUINT32(save->p);
	ht->offset = READUINT32(save->p);
	ht->timer = READUINT32(save->p);
	ht->affectee = READINT32(save->p);
	ht->sourceline = READINT32(save->p);
	ht->exists = READINT32(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadFadeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	sector_t *ss;
	fade_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dest_exc = GetNetColormapFromList(READUINT32(save->p));
	ht->sectornum = READUINT32(save->p);
	ht->ffloornum = READUINT32(save->p);
	ht->alpha = READINT32(save->p);
	ht->sourcevalue = READINT16(save->p);
	ht->destvalue = READINT16(save->p);
	ht->destlightlevel = READINT16(save->p);
	ht->speed = READINT16(save->p);
	ht->ticbased = (boolean)READUINT8(save->p);
	ht->timer = READINT32(save->p);
	ht->doexists = READUINT8(save->p);
	ht->dotranslucent = READUINT8(save->p);
	ht->dolighting = READUINT8(save->p);
	ht->docolormap = READUINT8(save->p);
	ht->docollision = READUINT8(save->p);
	ht->doghostfade = READUINT8(save->p);
	ht->exactalpha = READUINT8(save->p);

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

static inline thinker_t* LoadFadeColormapThinker(savebuffer_t *save, actionf_p1 thinker)
{
	fadecolormap_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->source_exc = GetNetColormapFromList(READUINT32(save->p));
	ht->dest_exc = GetNetColormapFromList(READUINT32(save->p));
	ht->ticbased = (boolean)READUINT8(save->p);
	ht->duration = READINT32(save->p);
	ht->timer = READINT32(save->p);
	if (ht->sector)
		ht->sector->fadecolormapdata = ht;
	return &ht->thinker;
}

static inline thinker_t* LoadPlaneDisplaceThinker(savebuffer_t *save, actionf_p1 thinker)
{
	planedisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->affectee = READINT32(save->p);
	ht->control = READINT32(save->p);
	ht->last_height = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->type = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadDynamicLineSlopeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	dynlineplanethink_t* ht = Z_Malloc(sizeof(*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->type = READUINT8(save->p);
	ht->slope = LoadSlope(READUINT32(save->p));
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->extent = READFIXED(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadDynamicVertexSlopeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	size_t i;
	dynvertexplanethink_t* ht = Z_Malloc(sizeof(*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;

	ht->slope = LoadSlope(READUINT32(save->p));
	for (i = 0; i < 3; i++)
		ht->secs[i] = LoadSector(READUINT32(save->p));
	READMEM(save->p, ht->vex, sizeof(ht->vex));
	READMEM(save->p, ht->origsecheights, sizeof(ht->origsecheights));
	READMEM(save->p, ht->origvecheights, sizeof(ht->origvecheights));
	ht->relative = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyrotatetThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyrotate_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->turnobjs = READUINT8(save->p);
	return &ht->thinker;
}

static thinker_t* LoadPolymoveThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polymove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->momx = READFIXED(save->p);
	ht->momy = READFIXED(save->p);
	ht->distance = READINT32(save->p);
	ht->angle = READANGLE(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolywaypointThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polywaypoint_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->sequence = READINT32(save->p);
	ht->pointnum = READINT32(save->p);
	ht->direction = READINT32(save->p);
	ht->returnbehavior = READUINT8(save->p);
	ht->continuous = READUINT8(save->p);
	ht->stophere = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyslidedoorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyslidedoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->delay = READINT32(save->p);
	ht->delayCount = READINT32(save->p);
	ht->initSpeed = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->initDistance = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->initAngle = READUINT32(save->p);
	ht->angle = READUINT32(save->p);
	ht->revAngle = READUINT32(save->p);
	ht->momx = READFIXED(save->p);
	ht->momy = READFIXED(save->p);
	ht->closing = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyswingdoorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyswingdoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->delay = READINT32(save->p);
	ht->delayCount = READINT32(save->p);
	ht->initSpeed = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->initDistance = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->closing = READUINT8(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolydisplaceThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polydisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->controlSector = LoadSector(READUINT32(save->p));
	ht->dx = READFIXED(save->p);
	ht->dy = READFIXED(save->p);
	ht->oldHeights = READFIXED(save->p);
	return &ht->thinker;
}

static inline thinker_t* LoadPolyrotdisplaceThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyrotdisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->controlSector = LoadSector(READUINT32(save->p));
	ht->rotscale = READFIXED(save->p);
	ht->turnobjs = READUINT8(save->p);
	ht->oldHeights = READFIXED(save->p);
	return &ht->thinker;
}

static thinker_t* LoadPolyfadeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyfade_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->sourcevalue = READINT32(save->p);
	ht->destvalue = READINT32(save->p);
	ht->docollision = (boolean)READUINT8(save->p);
	ht->doghostfade = (boolean)READUINT8(save->p);
	ht->ticbased = (boolean)READUINT8(save->p);
	ht->duration = READINT32(save->p);
	ht->timer = READINT32(save->p);
	return &ht->thinker;
}

static void P_NetUnArchiveThinkers(savebuffer_t *save)
{
	thinker_t *currentthinker;
	thinker_t *next;
	UINT8 tclass;
	UINT8 restoreNum = false;
	UINT32 i;
	UINT32 numloaded = 0;

	if (READUINT32(save->p) != ARCHIVEBLOCK_THINKERS)
		I_Error("Bad $$$.sav at archive block Thinkers");

	// remove all the current thinkers
	for (i = 0; i < NUM_THINKERLISTS; i++)
	{
		for (currentthinker = thlist[i].next; currentthinker != &thlist[i]; currentthinker = next)
		{
			next = currentthinker->next;

			currentthinker->references = 0; // Heinous but this is the only place the assertion in P_UnlinkThinkers is wrong

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

	// Oh my god don't blast random memory with our reference counts.
	boss3cap = waypointcap = kitemcap = NULL;
	for (i = 0; i <= 15; i++)
	{
		skyboxcenterpnts[i] = skyboxviewpnts[i] = NULL;
	}

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
			tclass = READUINT8(save->p);

			if (tclass == tc_end)
				break; // leave the saved thinker reading loop
			numloaded++;

			switch (tclass)
			{
				case tc_mobj:
					th = LoadMobjThinker(save, (actionf_p1)P_MobjThinker);
					break;

				case tc_ceiling:
					th = LoadCeilingThinker(save, (actionf_p1)T_MoveCeiling);
					break;

				case tc_crushceiling:
					th = LoadCeilingThinker(save, (actionf_p1)T_CrushCeiling);
					break;

				case tc_floor:
					th = LoadFloormoveThinker(save, (actionf_p1)T_MoveFloor);
					break;

				case tc_flash:
					th = LoadLightflashThinker(save, (actionf_p1)T_LightningFlash);
					break;

				case tc_strobe:
					th = LoadStrobeThinker(save, (actionf_p1)T_StrobeFlash);
					break;

				case tc_glow:
					th = LoadGlowThinker(save, (actionf_p1)T_Glow);
					break;

				case tc_fireflicker:
					th = LoadFireflickerThinker(save, (actionf_p1)T_FireFlicker);
					break;

				case tc_elevator:
					th = LoadElevatorThinker(save, (actionf_p1)T_MoveElevator, true);
					break;

				case tc_continuousfalling:
					th = LoadContinuousFallThinker(save, (actionf_p1)T_ContinuousFalling);
					break;

				case tc_thwomp:
					th = LoadThwompThinker(save, (actionf_p1)T_ThwompSector);
					break;

				case tc_noenemies:
					th = LoadNoEnemiesThinker(save, (actionf_p1)T_NoEnemiesSector);
					break;

				case tc_eachtime:
					th = LoadEachTimeThinker(save, (actionf_p1)T_EachTimeThinker);
					break;

				case tc_raisesector:
					th = LoadRaiseThinker(save, (actionf_p1)T_RaiseSector);
					break;

				case tc_camerascanner:
					th = LoadElevatorThinker(save, (actionf_p1)T_CameraScanner, false);
					break;

				case tc_bouncecheese:
					th = LoadBounceCheeseThinker(save, (actionf_p1)T_BounceCheese);
					break;

				case tc_startcrumble:
					th = LoadCrumbleThinker(save, (actionf_p1)T_StartCrumble);
					break;

				case tc_marioblock:
					th = LoadMarioBlockThinker(save, (actionf_p1)T_MarioBlock);
					break;

				case tc_marioblockchecker:
					th = LoadMarioCheckThinker(save, (actionf_p1)T_MarioBlockChecker);
					break;

				case tc_floatsector:
					th = LoadFloatThinker(save, (actionf_p1)T_FloatSector);
					break;

				case tc_laserflash:
					th = LoadLaserThinker(save, (actionf_p1)T_LaserFlash);
					break;

				case tc_lightfade:
					th = LoadLightlevelThinker(save, (actionf_p1)T_LightFade);
					break;

				case tc_executor:
					th = LoadExecutorThinker(save, (actionf_p1)T_ExecutorDelay);
					restoreNum = true;
					break;

				case tc_disappear:
					th = LoadDisappearThinker(save, (actionf_p1)T_Disappear);
					break;

				case tc_fade:
					th = LoadFadeThinker(save, (actionf_p1)T_Fade);
					break;

				case tc_fadecolormap:
					th = LoadFadeColormapThinker(save, (actionf_p1)T_FadeColormap);
					break;

				case tc_planedisplace:
					th = LoadPlaneDisplaceThinker(save, (actionf_p1)T_PlaneDisplace);
					break;
				case tc_polyrotate:
					th = LoadPolyrotatetThinker(save, (actionf_p1)T_PolyObjRotate);
					break;

				case tc_polymove:
					th = LoadPolymoveThinker(save, (actionf_p1)T_PolyObjMove);
					break;

				case tc_polywaypoint:
					th = LoadPolywaypointThinker(save, (actionf_p1)T_PolyObjWaypoint);
					break;

				case tc_polyslidedoor:
					th = LoadPolyslidedoorThinker(save, (actionf_p1)T_PolyDoorSlide);
					break;

				case tc_polyswingdoor:
					th = LoadPolyswingdoorThinker(save, (actionf_p1)T_PolyDoorSwing);
					break;

				case tc_polyflag:
					th = LoadPolymoveThinker(save, (actionf_p1)T_PolyObjFlag);
					break;

				case tc_polydisplace:
					th = LoadPolydisplaceThinker(save, (actionf_p1)T_PolyObjDisplace);
					break;

				case tc_polyrotdisplace:
					th = LoadPolyrotdisplaceThinker(save, (actionf_p1)T_PolyObjRotDisplace);
					break;

				case tc_polyfade:
					th = LoadPolyfadeThinker(save, (actionf_p1)T_PolyObjFade);
					break;

				case tc_dynslopeline:
					th = LoadDynamicLineSlopeThinker(save, (actionf_p1)T_DynamicSlopeLine);
					break;

				case tc_dynslopevert:
					th = LoadDynamicVertexSlopeThinker(save, (actionf_p1)T_DynamicSlopeVert);
					break;

				case tc_scroll:
					th = LoadScrollThinker(save, (actionf_p1)T_Scroll);
					break;

				case tc_friction:
					th = LoadFrictionThinker(save, (actionf_p1)T_Friction);
					break;

				case tc_pusher:
					th = LoadPusherThinker(save, (actionf_p1)T_Pusher);
					break;

				default:
					I_Error("P_UnarchiveSpecials: Unknown tclass %d in savegame", tclass);
			}
			if (th)
				P_AddThinker(i, th);
		}

		CONS_Debug(DBG_NETPLAY, "%u thinkers loaded in list %d\n", numloaded, i);
	}
	
	// Set each skyboxmo to the first skybox (or NULL)
	skyboxmo[0] = skyboxviewpnts[0];
	skyboxmo[1] = skyboxcenterpnts[0];

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

static inline void P_ArchivePolyObj(savebuffer_t *save, polyobj_t *po)
{
	UINT8 diff = 0;
	WRITEINT32(save->p, po->id);
	WRITEANGLE(save->p, po->angle);

	WRITEFIXED(save->p, po->spawnSpot.x);
	WRITEFIXED(save->p, po->spawnSpot.y);

	if (po->flags != po->spawnflags)
		diff |= PD_FLAGS;
	if (po->translucency != po->spawntrans)
		diff |= PD_TRANS;

	WRITEUINT8(save->p, diff);

	if (diff & PD_FLAGS)
		WRITEINT32(save->p, po->flags);
	if (diff & PD_TRANS)
		WRITEINT32(save->p, po->translucency);
}

static inline void P_UnArchivePolyObj(savebuffer_t *save, polyobj_t *po)
{
	INT32 id;
	UINT32 angle;
	fixed_t x, y;
	UINT8 diff;

	// nullify all polyobject thinker pointers;
	// the thinkers themselves will fight over who gets the field
	// when they first start to run.
	po->thinker = NULL;

	id = READINT32(save->p);

	angle = READANGLE(save->p);

	x = READFIXED(save->p);
	y = READFIXED(save->p);

	diff = READUINT8(save->p);

	if (diff & PD_FLAGS)
		po->flags = READINT32(save->p);
	if (diff & PD_TRANS)
		po->translucency = READINT32(save->p);

	// if the object is bad or isn't in the id hash, we can do nothing more
	// with it, so return now
	if (po->isBad || po != Polyobj_GetForNum(id))
		return;

	// rotate and translate polyobject
	Polyobj_MoveOnLoad(po, angle, x, y);
}

static inline void P_ArchivePolyObjects(savebuffer_t *save)
{
	INT32 i;

	WRITEUINT32(save->p, ARCHIVEBLOCK_POBJS);

	// save number of polyobjects
	WRITEINT32(save->p, numPolyObjects);

	for (i = 0; i < numPolyObjects; ++i)
		P_ArchivePolyObj(save, &PolyObjects[i]);
}

static inline void P_UnArchivePolyObjects(savebuffer_t *save)
{
	INT32 i, numSavedPolys;

	if (READUINT32(save->p) != ARCHIVEBLOCK_POBJS)
		I_Error("Bad $$$.sav at archive block Pobjs");

	numSavedPolys = READINT32(save->p);

	if (numSavedPolys != numPolyObjects)
		I_Error("P_UnArchivePolyObjects: polyobj count inconsistency\n");

	for (i = 0; i < numSavedPolys; ++i)
		P_UnArchivePolyObj(save, &PolyObjects[i]);
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
	UINT32 temp, i;

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
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;
			if ( players[i].awayviewmobj)
			{
				temp = (UINT32)(size_t)players[i].awayviewmobj;
				players[i].awayviewmobj = NULL;
				if (!P_SetTarget(&players[i].awayviewmobj, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "awayviewmobj not found on player %d\n", i);
			}
			if (players[i].followmobj)
			{
				temp = (UINT32)(size_t)players[i].followmobj;
				players[i].followmobj = NULL;
				if (!P_SetTarget(&players[i].followmobj, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "followmobj not found on player %d\n", i);
			}
			if (players[i].follower)
			{
				temp = (UINT32)(size_t)players[i].follower;
				players[i].follower = NULL;
				if (!P_SetTarget(&players[i].follower, P_FindNewPosition(temp)))
					CONS_Debug(DBG_GAMELOGIC, "follower not found on player %d\n", i);
			}
			if (players[i].currentwaypoint)
			{
				temp = (UINT32)(size_t)players[i].currentwaypoint;
				players[i].currentwaypoint = K_GetWaypointFromIndex(temp);
				if (players[i].currentwaypoint == NULL)
				{
					CONS_Debug(DBG_GAMELOGIC, "currentwaypoint not found on player %d\n", i);
				}
			}
			if (players[i].nextwaypoint)
			{
				temp = (UINT32)(size_t)players[i].nextwaypoint;
				players[i].nextwaypoint = K_GetWaypointFromIndex(temp);
				if (players[i].nextwaypoint == NULL)
				{
					CONS_Debug(DBG_GAMELOGIC, "nextwaypoint not found on player %d\n", i);
				}
			}
		}
	}
}

static inline void P_NetArchiveSpecials(savebuffer_t *save)
{
	size_t i, z;

	WRITEUINT32(save->p, ARCHIVEBLOCK_SPECIALS);

	// itemrespawn queue for deathmatch
	i = iquetail;
	while (iquehead != i)
	{
		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] == itemrespawnque[i])
			{
				WRITEUINT32(save->p, z);
				break;
			}
		}
		WRITEUINT32(save->p, itemrespawntime[i]);
		i = (i + 1) & (ITEMQUESIZE-1);
	}

	// end delimiter
	WRITEUINT32(save->p, 0xffffffff);

	// Sky number
	WRITESTRINGN(save->p, globallevelskytexture, 9);

	// Current global weather type
	WRITEUINT8(save->p, globalweather);

	if (metalplayback) // Is metal sonic running?
	{
		WRITEUINT8(save->p, 0x01);
		G_SaveMetal(&save->p);
	}
	else
		WRITEUINT8(save->p, 0x00);
}

static void P_NetUnArchiveSpecials(savebuffer_t *save)
{
	char skytex[9];
	size_t i;

	if (READUINT32(save->p) != ARCHIVEBLOCK_SPECIALS)
		I_Error("Bad $$$.sav at archive block Specials");

	// BP: added save itemrespawn queue for deathmatch
	iquetail = iquehead = 0;
	while ((i = READUINT32(save->p)) != 0xffffffff)
	{
		itemrespawnque[iquehead] = &mapthings[i];
		itemrespawntime[iquehead++] = READINT32(save->p);
	}

	READSTRINGN(save->p, skytex, sizeof(skytex));
	if (strcmp(skytex, globallevelskytexture))
		P_SetupLevelSky(skytex, true);

	globalweather = READUINT8(save->p);

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

	if (READUINT8(save->p) == 0x01) // metal sonic
		G_LoadMetal(&save->p);
}

// =======================================================================
//          Misc
// =======================================================================
static inline void P_ArchiveMisc(savebuffer_t *save, INT16 mapnum)
{
	//lastmapsaved = mapnum;
	lastmaploaded = mapnum;

	if (gamecomplete)
		mapnum |= 8192;

	WRITEINT16(save->p, mapnum);
	WRITEUINT16(save->p, emeralds+357);
	WRITESTRINGN(save->p, timeattackfolder, sizeof(timeattackfolder));
}

static inline void P_UnArchiveSPGame(savebuffer_t *save, INT16 mapoverride)
{
	char testname[sizeof(timeattackfolder)];

	gamemap = READINT16(save->p);

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

	savedata.emeralds = READUINT16(save->p)-357;

	READSTRINGN(save->p, testname, sizeof(testname));

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

static void P_NetArchiveMisc(savebuffer_t *save, boolean resending)
{
	size_t i;

	WRITEUINT32(save->p, ARCHIVEBLOCK_MISC);

	if (resending)
		WRITEUINT32(save->p, gametic);
	WRITEINT16(save->p, gamemap);

	if (gamestate != GS_LEVEL)
		WRITEINT16(save->p, GS_WAITINGPLAYERS); // nice hack to put people back into waitingplayers
	else
		WRITEINT16(save->p, gamestate);
	WRITEINT16(save->p, gametype);

	{
		UINT32 pig = 0;
		for (i = 0; i < MAXPLAYERS; i++)
			pig |= (playeringame[i] != 0)<<i;
		WRITEUINT32(save->p, pig);
	}

	WRITEUINT32(save->p, P_GetRandSeed());

	WRITEUINT32(save->p, tokenlist);

	WRITEUINT8(save->p, encoremode);
	WRITEUINT8(save->p, mapmusrng);

	WRITEUINT32(save->p, leveltime);
	WRITEUINT32(save->p, ssspheres);
	WRITEINT16(save->p, lastmap);
	WRITEUINT16(save->p, bossdisabled);
	WRITEUINT8(save->p, ringsdisabled);

	for (i = 0; i < 4; i++)
	{
		WRITEINT16(save->p, votelevels[i][0]);
		WRITEINT16(save->p, votelevels[i][1]);
	}

	for (i = 0; i < MAXPLAYERS; i++)
		WRITESINT8(save->p, votes[i]);

	WRITESINT8(save->p, pickedvote);

	WRITEUINT16(save->p, emeralds);
	{
		UINT8 globools = 0;
		if (stagefailed)
			globools |= 1;
		if (stoppedclock)
			globools |= (1<<1);
		WRITEUINT8(save->p, globools);
	}

	WRITEUINT32(save->p, token);
	WRITEINT32(save->p, sstimer);
	WRITEUINT32(save->p, bluescore);
	WRITEUINT32(save->p, redscore);

	WRITEUINT16(save->p, skincolor_redteam);
	WRITEUINT16(save->p, skincolor_blueteam);
	WRITEUINT16(save->p, skincolor_redring);
	WRITEUINT16(save->p, skincolor_bluering);

	WRITEINT32(save->p, modulothing);

	WRITEINT16(save->p, autobalance);
	WRITEINT16(save->p, teamscramble);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save->p, scrambleplayers[i]);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save->p, scrambleteams[i]);

	WRITEINT16(save->p, scrambletotal);
	WRITEINT16(save->p, scramblecount);

	WRITEUINT32(save->p, racecountdown);
	WRITEUINT32(save->p, exitcountdown);
	
	// exitcondition_t
	WRITEUINT8(save->p, g_exit.losing);
	WRITEUINT8(save->p, g_exit.retry);

	WRITEFIXED(save->p, gravity);
	WRITEFIXED(save->p, mapobjectscale);

	WRITEUINT32(save->p, countdowntimer);
	WRITEUINT8(save->p, countdowntimeup);

	// SRB2kart
	WRITEINT32(save->p, numgotboxes);
	WRITEUINT8(save->p, numtargets);
	WRITEUINT8(save->p, battlecapsules);

	WRITEUINT8(save->p, gamespeed);
	WRITEUINT8(save->p, numlaps);
	WRITEUINT8(save->p, franticitems);
	WRITEUINT8(save->p, comeback);

	WRITESINT8(save->p, speedscramble);
	WRITESINT8(save->p, encorescramble);

	for (i = 0; i < 4; i++)
		WRITESINT8(save->p, battlewanted[i]);

	// battleovertime_t
	WRITEUINT16(save->p, battleovertime.enabled);
	WRITEFIXED(save->p, battleovertime.radius);
	WRITEFIXED(save->p, battleovertime.x);
	WRITEFIXED(save->p, battleovertime.y);
	WRITEFIXED(save->p, battleovertime.z);

	WRITEUINT32(save->p, wantedcalcdelay);
	WRITEUINT32(save->p, indirectitemcooldown);
	WRITEUINT32(save->p, mapreset);

	WRITEUINT8(save->p, spectateGriefed);

	WRITEUINT8(save->p, thwompsactive);
	WRITEUINT8(save->p, lastLowestLap);
	WRITESINT8(save->p, spbplace);
	WRITEUINT8(save->p, startedInFreePlay);

	WRITEUINT32(save->p, introtime);
	WRITEUINT32(save->p, starttime);

	WRITEUINT32(save->p, timelimitintics);
	WRITEUINT32(save->p, extratimeintics);
	WRITEUINT32(save->p, secretextratime);

	// Is it paused?
	if (paused)
		WRITEUINT8(save->p, 0x2f);
	else
		WRITEUINT8(save->p, 0x2e);

	// Only the server uses this, but it
	// needs synched for remote admins anyway.
	WRITEUINT32(save->p, schedule_len);
	for (i = 0; i < schedule_len; i++)
	{
		scheduleTask_t *task = schedule[i];
		WRITEINT16(save->p, task->basetime);
		WRITEINT16(save->p, task->timer);
		WRITESTRING(save->p, task->command);
	}
}

static inline boolean P_NetUnArchiveMisc(savebuffer_t *save, boolean reloading)
{
	size_t i;
	size_t numTasks;

	if (READUINT32(save->p) != ARCHIVEBLOCK_MISC)
		I_Error("Bad $$$.sav at archive block Misc");

	if (reloading)
		gametic = READUINT32(save->p);

	gamemap = READINT16(save->p);

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	// tell the sound code to reset the music since we're skipping what
	// normally sets this flag
	if (!reloading)
		mapmusflags |= MUSIC_RELOADRESET;

	G_SetGamestate(READINT16(save->p));

	gametype = READINT16(save->p);

	{
		UINT32 pig = READUINT32(save->p);
		for (i = 0; i < MAXPLAYERS; i++)
		{
			playeringame[i] = (pig & (1<<i)) != 0;
			// playerstate is set in unarchiveplayers
		}
	}

	P_SetRandSeed(READUINT32(save->p));

	tokenlist = READUINT32(save->p);

	encoremode = (boolean)READUINT8(save->p);
	
	mapmusrng = READUINT8(save->p);

	if (!P_LoadLevel(true, reloading))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Can't load the level!\n"));
		return false;
	}

	// get the time
	leveltime = READUINT32(save->p);
	ssspheres = READUINT32(save->p);
	lastmap = READINT16(save->p);
	bossdisabled = READUINT16(save->p);
	ringsdisabled = READUINT8(save->p);

	for (i = 0; i < 4; i++)
	{
		votelevels[i][0] = READINT16(save->p);
		votelevels[i][1] = READINT16(save->p);
	}

	for (i = 0; i < MAXPLAYERS; i++)
		votes[i] = READSINT8(save->p);

	pickedvote = READSINT8(save->p);

	emeralds = READUINT16(save->p);
	{
		UINT8 globools = READUINT8(save->p);
		stagefailed = !!(globools & 1);
		stoppedclock = !!(globools & (1<<1));
	}

	token = READUINT32(save->p);
	sstimer = READINT32(save->p);
	bluescore = READUINT32(save->p);
	redscore = READUINT32(save->p);

	skincolor_redteam = READUINT16(save->p);
	skincolor_blueteam = READUINT16(save->p);
	skincolor_redring = READUINT16(save->p);
	skincolor_bluering = READUINT16(save->p);

	modulothing = READINT32(save->p);

	autobalance = READINT16(save->p);
	teamscramble = READINT16(save->p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleplayers[i] = READINT16(save->p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleteams[i] = READINT16(save->p);

	scrambletotal = READINT16(save->p);
	scramblecount = READINT16(save->p);

	racecountdown = READUINT32(save->p);
	exitcountdown = READUINT32(save->p);

	// exitcondition_t
	g_exit.losing = READUINT8(save->p);
	g_exit.retry = READUINT8(save->p);
	
	gravity = READFIXED(save->p);
	mapobjectscale = READFIXED(save->p);

	countdowntimer = (tic_t)READUINT32(save->p);
	countdowntimeup = (boolean)READUINT8(save->p);

	// SRB2kart
	numgotboxes = READINT32(save->p);
	numtargets = READUINT8(save->p);
	battlecapsules = (boolean)READUINT8(save->p);

	gamespeed = READUINT8(save->p);
	numlaps = READUINT8(save->p);
	franticitems = (boolean)READUINT8(save->p);
	comeback = (boolean)READUINT8(save->p);

	speedscramble = READSINT8(save->p);
	encorescramble = READSINT8(save->p);

	for (i = 0; i < 4; i++)
		battlewanted[i] = READSINT8(save->p);

	// battleovertime_t
	battleovertime.enabled = READUINT16(save->p);
	battleovertime.radius = READFIXED(save->p);
	battleovertime.x = READFIXED(save->p);
	battleovertime.y = READFIXED(save->p);
	battleovertime.z = READFIXED(save->p);

	wantedcalcdelay = READUINT32(save->p);
	indirectitemcooldown = READUINT32(save->p);
	mapreset = READUINT32(save->p);

	spectateGriefed = READUINT8(save->p);

	thwompsactive = (boolean)READUINT8(save->p);
	lastLowestLap = READUINT8(save->p);
	spbplace = READSINT8(save->p);
	startedInFreePlay = READUINT8(save->p);

	introtime = READUINT32(save->p);
	starttime = READUINT32(save->p);
	
	timelimitintics = READUINT32(save->p);
	extratimeintics = READUINT32(save->p);
	secretextratime = READUINT32(save->p);

	// Is it paused?
	if (READUINT8(save->p) == 0x2f)
		paused = true;

	// Only the server uses this, but it
	// needs synched for remote admins anyway.
	Schedule_Clear();

	numTasks = READUINT32(save->p);
	for (i = 0; i < numTasks; i++)
	{
		INT16 basetime;
		INT16 timer;
		char command[MAXTEXTCMD];

		basetime = READINT16(save->p);
		timer = READINT16(save->p);
		READSTRING(save->p, command);

		Schedule_Add(basetime, timer, command);
	}

	return true;
}

static inline void P_ArchiveLuabanksAndConsistency(savebuffer_t *save)
{
	UINT8 i, banksinuse = NUM_LUABANKS;

	while (banksinuse && !luabanks[banksinuse-1])
		banksinuse--; // get the last used bank

	if (banksinuse)
	{
		WRITEUINT8(save->p, 0xb7); // luabanks marker
		WRITEUINT8(save->p, banksinuse);
		for (i = 0; i < banksinuse; i++)
			WRITEINT32(save->p, luabanks[i]);
	}

	WRITEUINT8(save->p, 0x1d); // consistency marker
}

static inline boolean P_UnArchiveLuabanksAndConsistency(savebuffer_t *save)
{
	switch (READUINT8(save->p))
	{
		case 0xb7: // luabanks marker
			{
				UINT8 i, banksinuse = READUINT8(save->p);
				if (banksinuse > NUM_LUABANKS)
				{
					CONS_Alert(CONS_ERROR, M_GetText("Corrupt Luabanks! (Too many banks in use)\n"));
					return false;
				}
				for (i = 0; i < banksinuse; i++)
					luabanks[i] = READINT32(save->p);
				if (READUINT8(save->p) != 0x1d) // consistency marker
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

void P_SaveGame(savebuffer_t *save, INT16 mapnum)
{
	P_ArchiveMisc(save, mapnum);
	P_ArchivePlayer(save);
	P_ArchiveLuabanksAndConsistency(save);
}

void P_SaveNetGame(savebuffer_t *save, boolean resending)
{
	thinker_t *th;
	mobj_t *mobj;
	UINT32 i = 1; // don't start from 0, it'd be confused with a blank pointer otherwise

	CV_SaveNetVars(&save->p);
	P_NetArchiveMisc(save, resending);

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

	P_NetArchivePlayers(save);
	if (gamestate == GS_LEVEL)
	{
		P_NetArchiveWorld(save);
		P_ArchivePolyObjects(save);
		P_NetArchiveThinkers(save);
		P_NetArchiveSpecials(save);
		P_NetArchiveColormaps(save);
		P_NetArchiveTubeWaypoints(save);
		P_NetArchiveWaypoints(save);
	}

	ACS_Archive(save);
	LUA_Archive(save, true);

	P_ArchiveLuabanksAndConsistency(save);
}

boolean P_LoadGame(savebuffer_t *save, INT16 mapoverride)
{
	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();
	if (gamestate == GS_VOTING)
		Y_EndVote();
	G_SetGamestate(GS_NULL); // should be changed in P_UnArchiveMisc

	P_UnArchiveSPGame(save, mapoverride);
	P_UnArchivePlayer(save);

	if (!P_UnArchiveLuabanksAndConsistency(save))
		return false;

	// Only do this after confirming savegame is ok
	G_DeferedInitNew(false, G_BuildMapName(gamemap), savedata.skin, 0, true);
	COM_BufAddText("dummyconsvar 1\n"); // G_DeferedInitNew doesn't do this

	return true;
}

boolean P_LoadNetGame(savebuffer_t *save, boolean reloading)
{
	CV_LoadNetVars(&save->p);
	if (!P_NetUnArchiveMisc(save,reloading))
		return false;
	P_NetUnArchivePlayers(save);
	if (gamestate == GS_LEVEL)
	{
		P_NetUnArchiveWorld(save);
		P_UnArchivePolyObjects(save);
		P_NetUnArchiveThinkers(save);
		P_NetUnArchiveSpecials(save);
		P_NetUnArchiveColormaps(save);
		P_NetUnArchiveTubeWaypoints(save);
		P_NetUnArchiveWaypoints(save);
		P_RelinkPointers();
		P_FinishMobjs();
	}

	ACS_UnArchive(save);
	LUA_UnArchive(save, true);

	// This is stupid and hacky, but maybe it'll work!
	P_SetRandSeed(P_GetInitSeed());

	// The precipitation would normally be spawned in P_SetupLevel, which is called by
	// P_NetUnArchiveMisc above. However, that would place it up before P_NetUnArchiveThinkers,
	// so the thinkers would be deleted later. Therefore, P_SetupLevel will *not* spawn
	// precipitation when loading a netgame save. Instead, precip has to be spawned here.
	// This is done in P_NetUnArchiveSpecials now.

	return P_UnArchiveLuabanksAndConsistency(save);
}
