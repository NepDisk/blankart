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
/// \file  st_stuff.h
/// \brief Status bar header

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomtype.h"
#include "d_event.h"
#include "d_player.h"
#include "r_defs.h"

// SRB2Kart
#include "r_skins.h" // NUMFACES

#ifdef __cplusplus
extern "C" {
#endif

//
// STATUS BAR
//

// Called by main loop.
void ST_Ticker(boolean run);

// Called when naming a replay.
void ST_DrawDemoTitleEntry(void);

#ifdef HAVE_DISCORDRPC
// Called when you have Discord asks
void ST_AskToJoinEnvelope(void);
#endif

// Called by main loop.
void ST_Drawer(void);

// Called when the console player is spawned on each level.
void ST_Start(void);

// Called by startup code.
void ST_Init(void);

// Called by G_Responder() when pressing F12 while viewing a demo.
void ST_changeDemoView(void);

void ST_UnloadGraphics(void);
void ST_LoadGraphics(void);

// face load graphics, called when skin changes
void ST_LoadFaceGraphics(INT32 playernum);
void ST_ReloadSkinFaceGraphics(void);

void ST_doPaletteStuff(void);

// title card
void ST_startTitleCard(void);
void ST_runTitleCard(void);
void ST_drawTitleCard(void);
void ST_preDrawTitleCard(void);
void ST_preLevelTitleCardDrawer(void);

extern tic_t lt_ticker, lt_lasttic;
extern tic_t lt_exitticker, lt_endtime;

// return if player a is in the same team as player b
boolean ST_SameTeam(player_t *a, player_t *b);

//--------------------
// status bar overlay
//--------------------

extern boolean st_overlay; // sb overlay on or off when fullscreen
extern INT32 st_palette; // 0 is default, any others are special palettes.
extern UINT32 st_translucency;

extern lumpnum_t st_borderpatchnum;
// patches, also used in intermission
extern patch_t *faceprefix[MAXSKINS][NUMFACES];

extern UINT16 objectsdrawn;

// variable to stop mayonaka static from flickering
extern consvar_t cv_lessflicker;
extern consvar_t cv_stagetitle;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
