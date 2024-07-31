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
/// \file  g_game.h
/// \brief Game loop, events handling.

#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "g_demo.h"
#include "m_cheat.h" // objectplacing

extern char gamedatafilename[64];
extern char timeattackfolder[64];
extern char customversionstring[32];
#define GAMEDATASIZE (4*8192)

extern char  player_names[MAXPLAYERS][MAXPLAYERNAME+1];
extern INT32 player_name_changes[MAXPLAYERS];

extern player_t players[MAXPLAYERS];
extern boolean playeringame[MAXPLAYERS];

// gametic at level start
extern tic_t levelstarttic;

// for modding?
extern INT16 prevmap, nextmap;
extern INT32 gameovertics;
extern UINT8 ammoremovaltics;
extern tic_t timeinmap; // Ticker for time spent in level (used for levelcard display)
extern INT32 pausedelay;
extern boolean pausebreakkey;

extern boolean promptactive;


extern consvar_t cv_tutorialprompt;

extern consvar_t cv_chatwidth, cv_chatnotifications, cv_chatheight, cv_chattime, cv_consolechat, cv_chatbacktint, cv_chatspamprotection;
extern consvar_t cv_shoutname, cv_shoutcolor, cv_autoshout;
extern consvar_t cv_songcredits;

extern consvar_t cv_pauseifunfocused;

extern consvar_t cv_invertmouse;

extern consvar_t cv_kickstartaccel[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_shrinkme[MAXSPLITSCREENPLAYERS];

extern consvar_t cv_turnaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_moveaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_brakeaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_aimaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_lookaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_fireaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_driftaxis[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_deadzone[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_digitaldeadzone[MAXSPLITSCREENPLAYERS];

extern consvar_t cv_ghost_besttime, cv_ghost_bestlap, cv_ghost_last, cv_ghost_guest, cv_ghost_staff;

extern consvar_t cv_invincmusicfade;
extern consvar_t cv_growmusicfade;

extern consvar_t cv_resetspecialmusic;

extern consvar_t cv_resume;

// mouseaiming (looking up/down with the mouse or keyboard)
#define KB_LOOKSPEED (1<<25)
#define MAXPLMOVE (50)
#define SLOWTURNTICS (6)

// build an internal map name MAPxx from map number
const char *G_BuildMapName(INT32 map);

void G_ResetAnglePrediction(player_t *player);
void G_BuildTiccmd(ticcmd_t *cmd, INT32 realtics, UINT8 ssplayer);

// copy ticcmd_t to and fro the normal way
ticcmd_t *G_CopyTiccmd(ticcmd_t* dest, const ticcmd_t* src, const size_t n);
// copy ticcmd_t to and fro network packets
ticcmd_t *G_MoveTiccmd(ticcmd_t* dest, const ticcmd_t* src, const size_t n);

// clip the console player aiming to the view
INT32 G_ClipAimingPitch(INT32 *aiming);
INT16 G_SoftwareClipAimingPitch(INT32 *aiming);

typedef enum
{
	AXISNONE = 0,

	AXISTURN,
	AXISMOVE,
	AXISBRAKE,
	AXISLOOK,

	AXISDIGITAL, // axes below this use digital deadzone

	AXISFIRE = AXISDIGITAL,
	AXISDRIFT,
	AXISSPINDASH,
	AXISLOOKBACK,
	AXISAIM,
} axis_input_e;

INT32 PlayerJoyAxis(UINT8 player, axis_input_e axissel);

extern angle_t localangle[MAXSPLITSCREENPLAYERS];
extern INT32 localaiming[MAXSPLITSCREENPLAYERS]; // should be an angle_t but signed

//
// GAME
//
void G_ChangePlayerReferences(mobj_t *oldmo, mobj_t *newmo);
void G_DoReborn(INT32 playernum);
void G_PlayerReborn(INT32 player, boolean betweenmaps);
void G_InitNew(UINT8 pencoremode, const char *mapname, boolean resetplayer,
	boolean skipprecutscene, boolean FLS);
char *G_BuildMapTitle(INT32 mapnum);

struct searchdim
{
	UINT8 pos;
	UINT8 siz;
};

typedef struct
{
	INT16  mapnum;
	UINT8  matchc;
	struct searchdim *matchd;/* offset that a pattern was matched */
	UINT8  keywhc;
	struct searchdim *keywhd;/* ...in KEYWORD */
	UINT8  total;/* total hits */
}
mapsearchfreq_t;

INT32 G_FindMap(const char *query, char **foundmapnamep,
		mapsearchfreq_t **freqp, INT32 *freqc);
void G_FreeMapSearch(mapsearchfreq_t *freq, INT32 freqc);

/* Match map name by search + 2 digit map code or map number. */
INT32 G_FindMapByNameOrCode(const char *query, char **foundmapnamep);

// XMOD spawning
mapthing_t *G_FindTeamStart(INT32 playernum);
mapthing_t *G_FindBattleStart(INT32 playernum);
mapthing_t *G_FindRaceStart(INT32 playernum);
mapthing_t *G_FindMapStart(INT32 playernum);
void G_MovePlayerToSpawnOrStarpost(INT32 playernum);
void G_SpawnPlayer(INT32 playernum);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1, but a warp test can start elsewhere
void G_DeferedInitNew(boolean pencoremode, const char *mapname, INT32 pickedchar,
	UINT8 ssplayers, boolean FLS);
void G_DoLoadLevel(boolean resetplayer);

void G_StartTitleCard(void);
void G_PreLevelTitleCard(void);
boolean G_IsTitleCardAvailable(void);

// Can be called by the startup code or M_Responder, calls P_SetupLevel.
void G_LoadGame(UINT32 slot, INT16 mapoverride);

void G_SaveGameData(void);

void G_SaveGame(UINT32 slot, INT16 mapnum);

void G_SaveGameOver(UINT32 slot, boolean modifylives);

extern UINT32 gametypedefaultrules[NUMGAMETYPES];
extern UINT32 gametypetol[NUMGAMETYPES];
extern INT16 gametyperankings[NUMGAMETYPES];

void G_SetGametype(INT16 gametype);
INT16 G_AddGametype(UINT32 rules);
void G_AddGametypeConstant(INT16 gtype, const char *newgtconst);
void G_UpdateGametypeSelections(void);
void G_AddTOL(UINT32 newtol, const char *tolname);
void G_AddGametypeTOL(INT16 gtype, UINT32 newtol);
INT32 G_GetGametypeByName(const char *gametypestr);
boolean G_IsSpecialStage(INT32 mapnum);
boolean G_GametypeUsesLives(void);
boolean G_GametypeHasTeams(void);
boolean G_GametypeHasSpectators(void);
INT16 G_SometimesGetDifferentGametype(void);
UINT8 G_GetGametypeColor(INT16 gt);
void G_ExitLevel(void);
void G_NextLevel(void);
void G_Continue(void);
void G_UseContinue(void);
void G_AfterIntermission(void);
void G_EndGame(void); // moved from y_inter.c/h and renamed

void G_Ticker(boolean run);
boolean G_Responder(event_t *ev);

boolean G_CouldView(INT32 playernum);
boolean G_CanView(INT32 playernum, UINT8 viewnum, boolean onlyactive);

INT32 G_FindView(INT32 startview, UINT8 viewnum, boolean onlyactive, boolean reverse);
INT32 G_CountPlayersPotentiallyViewable(boolean active);

void G_ResetViews(void);
void G_ResetView(UINT8 viewnum, INT32 playernum, boolean onlyactive);
void G_AdjustView(UINT8 viewnum, INT32 offset, boolean onlyactive);

void G_AddPartyMember (INT32 party_member, INT32 new_party_member);
void G_RemovePartyMember (INT32 party_member);
void G_ResetSplitscreen (INT32 playernum);

void G_AddPlayer(INT32 playernum);

void G_SetExitGameFlag(void);
void G_ClearExitGameFlag(void);
boolean G_GetExitGameFlag(void);

void G_SetRetryFlag(void);
void G_ClearRetryFlag(void);
boolean G_GetRetryFlag(void);

void G_SetModeAttackRetryFlag(void);
void G_ClearModeAttackRetryFlag(void);
boolean G_GetModeAttackRetryFlag(void);

void G_LoadGameData(void);
void G_LoadGameSettings(void);

void G_SetGameModified(boolean silent, boolean major);

void G_SetGamestate(gamestate_t newstate);

// Gamedata record shit
void G_AllocMainRecordData(INT16 i);
void G_ClearRecords(void);

tic_t G_GetBestTime(INT16 map);

FUNCMATH INT32 G_TicsToHours(tic_t tics);
FUNCMATH INT32 G_TicsToMinutes(tic_t tics, boolean full);
FUNCMATH INT32 G_TicsToSeconds(tic_t tics);
FUNCMATH INT32 G_TicsToCentiseconds(tic_t tics);
FUNCMATH INT32 G_TicsToMilliseconds(tic_t tics);

// Don't split up TOL handling
UINT32 G_TOLFlag(INT32 pgametype);

INT16 G_RandMap(UINT32 tolflags, INT16 pprevmap, UINT8 ignorebuffer, UINT8 maphell, boolean callagainsoon, INT16 *extbuffer);
void G_AddMapToBuffer(INT16 map);

#endif
