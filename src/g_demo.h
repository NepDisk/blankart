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
/// \file  g_demo.h
/// \brief Demo recording and playback

#ifndef __G_DEMO__
#define __G_DEMO__

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"

extern UINT8 *demo_p;

// ======================================
// DEMO playback/recording related stuff.
// ======================================

extern consvar_t cv_recordmultiplayerdemos, cv_netdemosyncquality;

extern tic_t demostarttime;

// Publicly-accessible demo vars
struct demovars_s {
	char titlename[65];
	boolean recording, playback, timing;
	UINT16 version; // Current file format of the demo being played
	boolean title; // Title Screen demo can be cancelled by any key
	boolean rewinding; // Rewind in progress

	boolean loadfiles, ignorefiles; // Demo file loading options
	boolean fromtitle; // SRB2Kart: Don't stop the music
	boolean inreplayhut; // Go back to replayhut after demos
	boolean quitafterplaying; // quit after playing a demo from cmdline
	boolean deferstart; // don't start playing demo right away
	boolean netgame; // multiplayer netgame

	tic_t savebutton; // Used to determine when the local player can choose to save the replay while the race is still going
	enum {
		DSM_NOTSAVING,
		DSM_WILLAUTOSAVE,
		DSM_TITLEENTRY,
		DSM_WILLSAVE,
		DSM_SAVED
	} savemode;

	boolean freecam;

};

extern struct demovars_s demo;

typedef enum {
	MD_NOTLOADED,
	MD_LOADED,
	MD_SUBDIR,
	MD_OUTDATED,
	MD_INVALID
} menudemotype_e;

struct menudemo_t {
	char filepath[256];
	menudemotype_e type;

	char title[65]; // Null-terminated for string prints
	UINT16 map;
	UINT8 addonstatus; // What do we need to do addon-wise to play this demo?
	UINT8 gametype;
	SINT8 kartspeed; // Add OR DF_ENCORE for encore mode, idk
	UINT8 numlaps;

	struct {
		UINT8 ranking;
		char name[17];
		UINT8 skin, color;
		UINT32 timeorscore;
	} standings[MAXPLAYERS];
};


extern mobj_t *metalplayback;

// Only called by startup code.
void G_RecordDemo(const char *name);
void G_RecordMetal(void);
void G_BeginRecording(void);
void G_BeginMetal(void);

// Only called by shutdown code.
void G_WriteStanding(UINT8 ranking, char *name, INT32 skinnum, UINT16 color, UINT32 val);
void G_SetDemoTime(UINT32 ptime, UINT32 plap);
UINT8 G_CmpDemoTime(char *oldname, char *newname);

typedef enum
{
	GHC_NORMAL = 0,
	GHC_SUPER,
	GHC_FIREFLOWER,
	GHC_INVINCIBLE,
	GHC_RETURNSKIN // not actually a colour
} ghostcolor_t;

extern UINT8 demo_extradata[MAXPLAYERS];
extern UINT8 demo_writerng;

#define DXD_RESPAWN    0x01 // "respawn" command in console
#define DXD_SKIN       0x02 // skin changed
#define DXD_NAME       0x04 // name changed
#define DXD_COLOR      0x08 // color changed
#define DXD_PLAYSTATE  0x10 // state changed between playing, spectating, or not in-game
#define DXD_FOLLOWER   0x20 // follower was changed
#define DXD_WEAPONPREF 0x40 // netsynced playsim settings were changed

#define DXD_PST_PLAYING    0x01
#define DXD_PST_SPECTATING 0x02
#define DXD_PST_LEFT       0x03

// Record/playback tics
void G_ReadDemoExtraData(void);
void G_WriteDemoExtraData(void);
void G_ReadDemoTiccmd(ticcmd_t *cmd, INT32 playernum);
void G_WriteDemoTiccmd(ticcmd_t *cmd, INT32 playernum);
void G_GhostAddColor(INT32 playernum, ghostcolor_t color);
void G_GhostAddFlip(INT32 playernum);
void G_GhostAddScale(INT32 playernum, fixed_t scale);
void G_GhostAddHit(INT32 playernum, mobj_t *victim);
void G_WriteAllGhostTics(void);
void G_WriteGhostTic(mobj_t *ghost, INT32 playernum);
void G_ConsAllGhostTics(void);
void G_ConsGhostTic(INT32 playernum);
void G_GhostTicker(void);

void G_InitDemoRewind(void);
void G_StoreRewindInfo(void);
void G_PreviewRewind(tic_t previewtime);
void G_ConfirmRewind(tic_t rewindtime);

void G_ReadMetalTic(mobj_t *metal);
void G_WriteMetalTic(mobj_t *metal);
void G_SaveMetal(UINT8 **buffer);
void G_LoadMetal(UINT8 **buffer);

// Your naming conventions are stupid and useless.
// There is no conflict here.
struct demoghost {
	UINT8 checksum[16];
	UINT8 *buffer, *p, color;
	UINT8 fadein;
	UINT16 version;
	mobj_t oldmo, *mo;
	struct demoghost *next;
};
extern demoghost *ghosts;

// G_CheckDemoExtraFiles: checks if our loaded WAD list matches the demo's.
#define DFILE_ERROR_NOTLOADED            0x01 // Files are not loaded, but can be without a restart.
#define DFILE_ERROR_OUTOFORDER           0x02 // Files are loaded, but out of order.
#define DFILE_ERROR_INCOMPLETEOUTOFORDER 0x03 // Some files are loaded out of order, but others are not.
#define DFILE_ERROR_CANNOTLOAD           0x04 // Files are missing and cannot be loaded.
#define DFILE_ERROR_EXTRAFILES           0x05 // Extra files outside of the replay's file list are loaded.

void G_DeferedPlayDemo(const char *demo);
void G_DoPlayDemo(char *defdemoname);
void G_TimeDemo(const char *name);
void G_AddGhost(char *defdemoname);
void G_UpdateStaffGhostName(lumpnum_t l);
void G_FreeGhosts(void);
void G_DoneLevelLoad(void);

void G_DoPlayMetal(void);
void G_StopMetalDemo(void);
ATTRNORETURN void FUNCNORETURN G_StopMetalRecording(boolean kill);

void G_StopDemo(void);
boolean G_CheckDemoStatus(void);

void G_LoadDemoInfo(menudemo_t *pdemo);
void G_DeferedPlayDemo(const char *demo);

void G_SaveDemo(void);

boolean G_DemoTitleResponder(event_t *ev);

#endif // __G_DEMO__
