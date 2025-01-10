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
/// \file  st_stuff.c
/// \brief Status bar code
///        Does the face/direction indicator animatin.
///        Does palette indicators as well (red pain/berserk, bright pickup)

#include "doomdef.h"
#include "g_game.h"
#include "i_sound.h"
#include "r_local.h"
#include "p_local.h"
#include "f_finale.h"
#include "st_stuff.h"
#include "i_video.h"
#include "v_video.h"
#include "z_zone.h"
#include "hu_stuff.h"
#include "console.h"
#include "s_sound.h"
#include "i_system.h"
#include "m_menu.h"
#include "m_cheat.h"
#include "m_misc.h" // moviemode
#include "m_anigif.h" // cv_gif_downscale
#include "p_setup.h" // NiGHTS grading
#include "k_grandprix.h"	// we need to know grandprix status for titlecards
#include "k_boss.h"
#include "r_fps.h"

//random index
#include "m_random.h"

// item finder
#include "m_cond.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#include "lua_hudlib_drawlist.h"
#include "lua_hud.h"
#include "lua_hook.h"

// SRB2Kart
#include "k_hud.h" // SRB2kart
#include "v_video.h"
#include "r_skins.h" // NUMFACES

#include "r_fps.h"

// variable to stop mayonaka static from flickering
consvar_t cv_lessflicker = CVAR_INIT ("lessflicker", "Off", CV_SAVE, CV_OnOff, NULL);

consvar_t cv_stagetitle = CVAR_INIT ("maptitle", "On", CV_SAVE, CV_OnOff, NULL);

UINT16 objectsdrawn = 0;

//
// STATUS BAR DATA
//

patch_t *faceprefix[MAXSKINS][NUMFACES];

// ------------------------------------------
//             status bar overlay
// ------------------------------------------

// Midnight Channel:
static patch_t *hud_tv1;
static patch_t *hud_tv2;

#ifdef HAVE_DISCORDRPC
// Discord Rich Presence
static patch_t *envelope;
#endif

static huddrawlist_h luahuddrawlist_game;
static huddrawlist_h luahuddrawlist_titlecard;

//
// STATUS BAR CODE
//

boolean ST_SameTeam(player_t *a, player_t *b)
{
	// Spectator chat.
	if (a->spectator && b->spectator)
	{
		return true;
	}

	// Team chat.
	if (G_GametypeHasTeams() == true)
	{
		// You get team messages if you're on the same team.
		return (a->ctfteam == b->ctfteam);
	}
	else
	{
		// Not that everyone's not on the same team, but team messages go to normal chat if everyone's not in the same team.
		return true;
	}
}

static boolean st_stopped = true;

void ST_Ticker(boolean run)
{
	if (st_stopped)
		return;

	if (run)
		ST_runTitleCard();
}

// 0 is default, any others are special palettes.
INT32 st_palette = 0;
UINT32 st_translucency = 10;

void ST_doPaletteStuff(void)
{
	INT32 palette;

	if (stplyr && stplyr->flashcount)
		palette = stplyr->flashpal;
	else
		palette = 0;

#ifdef HWRENDER
	if (rendermode == render_opengl)
		palette = 0; // No flashpals here in OpenGL
#endif

	if (palette != st_palette)
	{
		st_palette = palette;

		if (rendermode == render_soft)
		{
			//V_SetPaletteLump(GetPalette()); // Reset the palette -- is this needed?
			if (!r_splitscreen)
				V_SetPalette(palette);
		}
	}
}

void ST_UnloadGraphics(void)
{
	Patch_FreeTag(PU_HUDGFX);
}

void ST_LoadGraphics(void)
{
	// SRB2 border patch
	// st_borderpatchnum = W_GetNumForName("GFZFLR01");
	// scr_borderpatch = W_CacheLumpNum(st_borderpatchnum, PU_HUDGFX);

	// the original Doom uses 'STF' as base name for all face graphics
	// Graue 04-08-2004: face/name graphics are now indexed by skins
	//                   but load them in R_AddSkins, that gets called
	//                   first anyway
	// cache the status bar overlay icons (fullscreen mode)
	K_LoadKartHUDGraphics();

	// Midnight Channel:
	HU_UpdatePatch(&hud_tv1, "HUD_TV1");
	HU_UpdatePatch(&hud_tv2, "HUD_TV2");

#ifdef HAVE_DISCORDRPC
	// Discord Rich Presence
	HU_UpdatePatch(&envelope, "K_REQUES");
#endif
}

// made separate so that skins code can reload custom face graphics
void ST_LoadFaceGraphics(INT32 skinnum)
{
#define FACE_MAX (FACE_MINIMAP+1)
	spritedef_t *sprdef = &skins[skinnum].sprites[SPR2_XTRA];
	spriteframe_t *sprframe;
	UINT8 i = 0, maxer = min(sprdef->numframes, FACE_MAX);
	while (i < maxer)
	{
		sprframe = &sprdef->spriteframes[i];
		faceprefix[skinnum][i] = W_CachePatchNum(sprframe->lumppat[0], PU_HUDGFX);
		i++;
	}
	if (i < FACE_MAX)
	{
		patch_t *missing = W_CachePatchName("MISSING", PU_HUDGFX);
		while (i < FACE_MAX)
		{
			faceprefix[skinnum][i] = missing;
			i++;
		}
	}
#undef FACE_MAX
}

void ST_ReloadSkinFaceGraphics(void)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
		ST_LoadFaceGraphics(i);
}

static inline void ST_InitData(void)
{
	// 'link' the statusbar display to a player, which could be
	// another player than consoleplayer, for example, when you
	// change the view in a multiplayer demo with F12.
	stplyr = &players[displayplayers[0]];

	st_palette = -1;
}

static inline void ST_Stop(void)
{
	if (st_stopped)
		return;

#ifdef HWRENDER
	if (rendermode != render_opengl)
#endif
		V_SetPalette(0);

	st_stopped = true;
}

void ST_Start(void)
{
	if (!st_stopped)
		ST_Stop();

	ST_InitData();

	if (!dedicated)
		st_stopped = false;
}

//
// Initializes the status bar, sets the defaults border patch for the window borders.
//

// used by OpenGL mode, holds lumpnum of flat used to fill space around the viewwindow
lumpnum_t st_borderpatchnum;

void ST_Init(void)
{
	if (dedicated)
		return;

	ST_LoadGraphics();

	luahuddrawlist_game = LUA_HUD_CreateDrawList();
	luahuddrawlist_titlecard = LUA_HUD_CreateDrawList();
}

// change the status bar too, when pressing F12 while viewing a demo.
void ST_changeDemoView(void)
{
	// the same routine is called at multiplayer deathmatch spawn
	// so it can be called multiple times
	ST_Start();
}

// =========================================================================
//                         STATUS BAR OVERLAY
// =========================================================================

boolean st_overlay;

/*
static INT32 SCZ(INT32 z)
{
	return FixedInt(FixedMul(z<<FRACBITS, vid.fdupy));
}
*/

/*
static INT32 SCY(INT32 y)
{
	//31/10/99: fixed by Hurdler so it _works_ also in hardware mode
	// do not scale to resolution for hardware accelerated
	// because these modes always scale by default
	y = SCZ(y); // scale to resolution
	if (splitscreen)
	{
		y >>= 1;
		if (stplyr != &players[displayplayers[0]])
			y += vid.height / 2;
	}
	return y;
}
*/

/*
static INT32 STRINGY(INT32 y)
{
	//31/10/99: fixed by Hurdler so it _works_ also in hardware mode
	// do not scale to resolution for hardware accelerated
	// because these modes always scale by default
	if (splitscreen)
	{
		y >>= 1;
		if (stplyr != &players[displayplayers[0]])
			y += BASEVIDHEIGHT / 2;
	}
	return y;
}
*/

/*
static INT32 SPLITFLAGS(INT32 f)
{
	// Pass this V_SNAPTO(TOP|BOTTOM) and it'll trim them to account for splitscreen! -Red
	if (splitscreen)
	{
		if (stplyr != &players[displayplayers[0]])
			f &= ~V_SNAPTOTOP;
		else
			f &= ~V_SNAPTOBOTTOM;
	}
	return f;
}
*/

/*
static INT32 SCX(INT32 x)
{
	return FixedInt(FixedMul(x<<FRACBITS, vid.fdupx));
}
*/

#if 0
static INT32 SCR(INT32 r)
{
	fixed_t y;
		//31/10/99: fixed by Hurdler so it _works_ also in hardware mode
	// do not scale to resolution for hardware accelerated
	// because these modes always scale by default
	y = FixedMul(r*FRACUNIT, vid.fdupy); // scale to resolution
	if (splitscreen)
	{
		y >>= 1;
		if (stplyr != &players[displayplayers[0]])
			y += vid.height / 2;
	}
	return FixedInt(FixedDiv(y, vid.fdupy));
}
#endif

// =========================================================================
//                          INTERNAL DRAWING
// =========================================================================

// Devmode information

/*static void ST_pushDebugString(INT32 *height, const char *string)
{
	V_DrawRightAlignedString(320, *height, V_MONOSPACE, string);
	*height -= 8;
}

static void ST_pushDebugTimeMS(INT32 *height, const char *label, UINT32 ms)
{
	ST_pushDebugString(height, va("%s%02d:%05.2f", label,
				ms / 60000, ms % 60000 / 1000.f));
}

static void ST_drawMusicDebug(INT32 *height)
{
	char mname[7];
	UINT16 mflags; // unused
	boolean looping;
	UINT8 i = 0;

	const musicdef_t *def;
	musictype_t format;

	if (!S_MusicInfo(mname, &mflags, &looping))
	{
		ST_pushDebugString(height, "Song: <NOTHING>");
		return;
	}

	def = S_FindMusicDef(mname, &i);
	format = S_MusicType();

	ST_pushDebugTimeMS(height, " Elapsed: ", S_GetMusicPosition());
	ST_pushDebugTimeMS(height, looping
			? "  Loop B: "
			: "Duration: ", S_GetMusicLength());

	if (looping)
	{
		ST_pushDebugTimeMS(height, "  Loop A: ", S_GetMusicLoopPoint());
	}

	if (def)
	{
		ST_pushDebugString(height, va("  Volume: %4d/100", def->volume));
	}

	if (format)
	{
		ST_pushDebugString(height, va("  Format: %d", S_MusicType()));
	}

	ST_pushDebugString(height, va("    Song: %8s", mname));
}*/

static void ST_drawDebugInfo(void)
{
	INT32 height = 192;

	if (!stplyr->mo)
		return;

	if (cv_debug & DBG_BASIC)
	{
		const fixed_t d = AngleFixed(stplyr->mo->angle);
		V_DrawRightAlignedString(320, 168, V_MONOSPACE, va("X: %6d", stplyr->mo->x>>FRACBITS));
		V_DrawRightAlignedString(320, 176, V_MONOSPACE, va("Y: %6d", stplyr->mo->y>>FRACBITS));
		V_DrawRightAlignedString(320, 184, V_MONOSPACE, va("Z: %6d", stplyr->mo->z>>FRACBITS));
		V_DrawRightAlignedString(320, 192, V_MONOSPACE, va("A: %6d", FixedInt(d)));

		height = 152;
	}

	if (cv_debug & DBG_DETAILED)
	{
		//V_DrawRightAlignedString(320, height - 104, V_MONOSPACE, va("SHIELD: %5x", stplyr->powers[pw_shield]));
		V_DrawRightAlignedString(320, height - 96,  V_MONOSPACE, va("SCALE: %5d%%", (stplyr->mo->scale*100)/FRACUNIT));
		//V_DrawRightAlignedString(320, height - 88,  V_MONOSPACE, va("DASH: %3d/%3d", stplyr->dashspeed>>FRACBITS, FixedMul(stplyr->maxdash,stplyr->mo->scale)>>FRACBITS));
		//V_DrawRightAlignedString(320, height - 80,  V_MONOSPACE, va("AIR: %4d, %3d", stplyr->powers[pw_underwater], stplyr->powers[pw_spacetime]));

		// Flags
		//V_DrawRightAlignedString(304-64, height - 72, V_MONOSPACE, "Flags:");
		//V_DrawString(304-60,             height - 72, (stplyr->jumping) ? V_GREENMAP : V_REDMAP, "JM");
		//V_DrawString(304-40,             height - 72, (stplyr->pflags & PF_JUMPED) ? V_GREENMAP : V_REDMAP, "JD");
		//V_DrawString(304-20,             height - 72, (stplyr->pflags & PF_SPINNING) ? V_GREENMAP : V_REDMAP, "SP");
		//V_DrawString(304,                height - 72, (stplyr->pflags & PF_STARTDASH) ? V_GREENMAP : V_REDMAP, "ST");

		V_DrawRightAlignedString(320, height - 64, V_MONOSPACE, va("CEILZ: %6d", stplyr->mo->ceilingz>>FRACBITS));
		V_DrawRightAlignedString(320, height - 56, V_MONOSPACE, va("FLOORZ: %6d", stplyr->mo->floorz>>FRACBITS));

		V_DrawRightAlignedString(320, height - 48, V_MONOSPACE, va("CNVX: %6d", stplyr->cmomx>>FRACBITS));
		V_DrawRightAlignedString(320, height - 40, V_MONOSPACE, va("CNVY: %6d", stplyr->cmomy>>FRACBITS));
		V_DrawRightAlignedString(320, height - 32, V_MONOSPACE, va("PLTZ: %6d", stplyr->mo->pmomz>>FRACBITS));

		V_DrawRightAlignedString(320, height - 24, V_MONOSPACE, va("MOMX: %6d", stplyr->rmomx>>FRACBITS));
		V_DrawRightAlignedString(320, height - 16, V_MONOSPACE, va("MOMY: %6d", stplyr->rmomy>>FRACBITS));
		V_DrawRightAlignedString(320, height - 8,  V_MONOSPACE, va("MOMZ: %6d", stplyr->mo->momz>>FRACBITS));
		V_DrawRightAlignedString(320, height,      V_MONOSPACE, va("SPEED: %6d", stplyr->speed>>FRACBITS));

		height -= 120;
	}

	if (cv_debug & DBG_RANDOMIZER) // randomizer testing
	{
		fixed_t peekres = P_RandomPeek();
		peekres *= 10000;     // Change from fixed point
		peekres >>= FRACBITS; // to displayable decimal

		V_DrawRightAlignedString(320, height - 16, V_MONOSPACE, va("Init: %08x", P_GetInitSeed()));
		V_DrawRightAlignedString(320, height - 8,  V_MONOSPACE, va("Seed: %08x", P_GetRandSeed()));
		V_DrawRightAlignedString(320, height,      V_MONOSPACE, va("==  :    .%04d", peekres));

		height -= 32;
	}

	if (cv_debug & DBG_MEMORY)
		V_DrawRightAlignedString(320, height,     V_MONOSPACE, va("Heap used: %7sKB", sizeu1(Z_TagsUsage(0, INT32_MAX)>>10)));
}

tic_t lt_ticker = 0, lt_lasttic = 0;
tic_t lt_exitticker = 0, lt_endtime = 0;


//
// Load the graphics for the title card.
// Don't let LJ see this
//
static void ST_cacheLevelTitle(void)
{
	//UINT8 i;
	//char buf[9];

}

//
// Start the title card.
//
void ST_startTitleCard(void)
{
	// cache every HUD patch used
	ST_cacheLevelTitle(); // Nothing

	// initialize HUD variables
	lt_ticker = lt_exitticker = lt_lasttic = 0;
	lt_endtime = 4*TICRATE;	// + (10*NEWTICRATERATIO);
}

//
// What happens before drawing the title card.
// Which is just setting the HUD translucency.
//
void ST_preDrawTitleCard(void)
{
	if (!G_IsTitleCardAvailable())
		return;

	if (lt_ticker >= (lt_endtime + TICRATE))
		return;

	// Kart: nothing
}

//
// Run the title card.
// Called from ST_Ticker.
//
void ST_runTitleCard(void)
{
	boolean run = !(paused || P_AutoPause());
	//boolean gp = (marathonmode || (grandprixinfo.gp && grandprixinfo.roundnum));

	if (!G_IsTitleCardAvailable())
		return;

	if (lt_ticker >= (lt_endtime + TICRATE))
		return;

	if (run || (lt_ticker < PRELEVELTIME))
	{
		// tick
		lt_ticker++;

		// used for hud slidein
		if (lt_ticker >= lt_endtime)
			lt_exitticker++;
	}
}

//
// Draw the title card itself.
//

void ST_drawTitleCard(void)
{
	char *lvlttl = mapheaderinfo[gamemap-1]->lvlttl;
	char *subttl = mapheaderinfo[gamemap-1]->subttl;
	char *zonttl = mapheaderinfo[gamemap-1]->zonttl; // SRB2kart
	char *actnum = mapheaderinfo[gamemap-1]->actnum;
	INT32 lvlttlxpos;
	INT32 ttlnumxpos;
	INT32 zonexpos;
	INT32 dupcalc = (vid.width/vid.dupx);
	UINT8 gtc = G_GetGametypeColor(gametype);
	INT32 sub = 0;
	INT32 bary = (splitscreen)
		? BASEVIDHEIGHT/2
		: 163;
	INT32 lvlw;
	
	if (!cv_stagetitle.value)
		return;
	
	if (!LUA_HudEnabled(hud_stagetitle))
		goto luahook;

	if (timeinmap > 113 || timeinmap < 6)
		goto luahook;

	lvlw = V_LevelNameWidth(lvlttl);

	if (actnum[0])
		lvlttlxpos = ((BASEVIDWIDTH/2) - (lvlw/2)) - V_LevelNameWidth(actnum);
	else
		lvlttlxpos = ((BASEVIDWIDTH/2) - (lvlw/2));

	zonexpos = ttlnumxpos = lvlttlxpos + lvlw;
	if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOZONE))
	{
		if (zonttl[0])
			zonexpos -= V_LevelNameWidth(zonttl); // SRB2kart
		else
			zonexpos -= V_LevelNameWidth(M_GetText("Zone"));
	}

	if (lvlttlxpos < 0)
		lvlttlxpos = 0;

	if (timeinmap > 105)
	{
		INT32 count = (113 - (INT32)(timeinmap));
		sub = dupcalc;
		while (count-- > 0)
			sub >>= 1;
		sub = -sub;
	}

	{
		dupcalc = (dupcalc - BASEVIDWIDTH)>>1;
		V_DrawFill(sub - dupcalc, bary+9, ttlnumxpos+dupcalc + 1, 2, 31|V_SNAPTOBOTTOM);
		V_DrawDiag(sub + ttlnumxpos + 1, bary, 11, 31|V_SNAPTOBOTTOM);
		V_DrawFill(sub - dupcalc, bary, ttlnumxpos+dupcalc, 10, gtc|V_SNAPTOBOTTOM);
		V_DrawDiag(sub + ttlnumxpos, bary, 10, gtc|V_SNAPTOBOTTOM);

		if (subttl[0])
			V_DrawRightAlignedString(sub + zonexpos - 8, bary+1, V_ALLOWLOWERCASE|V_SNAPTOBOTTOM, subttl);
	}

	ttlnumxpos += sub;
	lvlttlxpos += sub;
	zonexpos += sub;

	V_DrawLevelTitle(lvlttlxpos, bary-18, V_SNAPTOBOTTOM, lvlttl);

	if (strlen(zonttl) > 0)
		V_DrawLevelTitle(zonexpos, bary+6, V_SNAPTOBOTTOM, zonttl);
	else if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOZONE))
		V_DrawLevelTitle(zonexpos, bary+6, V_SNAPTOBOTTOM, M_GetText("Zone"));

	if (actnum[0])
		V_DrawLevelTitle(ttlnumxpos+12, bary+6, V_SNAPTOBOTTOM, actnum);

luahook:
	if (renderisnewtic)
	{
		LUA_HUD_ClearDrawList(luahuddrawlist_titlecard);
		LUA_HookHUD(luahuddrawlist_titlecard, HUD_HOOK(titlecard));
	}
	LUA_HUD_DrawList(luahuddrawlist_titlecard);
}


//
// Drawer for G_PreLevelTitleCard.
//
void ST_preLevelTitleCardDrawer(void)
{
	V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, levelfadecol);

	ST_drawTitleCard();
	I_OsPolling();
	I_UpdateNoBlit();
}

//
// Draw the status bar overlay, customisable: the user chooses which
// kind of information to overlay
//
static void ST_overlayDrawer(void)
{
	// hu_showscores = auto hide score/time/rings when tab rankings are shown
	if (!(hu_showscores && (netgame || multiplayer)))
	{
		K_drawKartHUD();

		if (renderisnewtic)
		{
			LUA_HUD_ClearDrawList(luahuddrawlist_game);
			LUA_HookHUD(luahuddrawlist_game, HUD_HOOK(game));
		}
		LUA_HUD_DrawList(luahuddrawlist_game);
	}

	if (!hu_showscores) // hide the following if TAB is held
	{
		if (cv_showviewpointtext.value)
		{
			if (!demo.title && !P_IsLocalPlayer(stplyr))
			{
				if (!r_splitscreen)
				{
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-40, V_HUDTRANSHALF, M_GetText("VIEWPOINT:"));
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-32, V_HUDTRANSHALF|V_ALLOWLOWERCASE, player_names[stplyr-players]);
				}
				else if (r_splitscreen == 1)
				{
					char name[MAXPLAYERNAME+12];

					INT32 y = (stplyr == &players[displayplayers[0]]) ? 4 : BASEVIDHEIGHT/2-12;
					sprintf(name, "VIEWPOINT: %s", player_names[stplyr-players]);
					V_DrawRightAlignedThinString(BASEVIDWIDTH-40, y, V_HUDTRANSHALF|V_ALLOWLOWERCASE|V_SNAPTOTOP|V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_SPLITSCREEN, name);
				}
				else if (r_splitscreen)
				{
					V_DrawCenteredThinString((vid.width/vid.dupx)/4, BASEVIDHEIGHT/2 - 12, V_HUDTRANSHALF|V_ALLOWLOWERCASE|V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_SPLITSCREEN, player_names[stplyr-players]);
				}
			}
		}
	}

	if (!hu_showscores && netgame && !mapreset)
	{
		if (stplyr->spectator && LUA_HudEnabled(hud_textspectator))
		{
			const char *itemtxt = M_GetText("Item - Join Game");

			if (stplyr->flashing)
				itemtxt = M_GetText("Item - . . .");
			else if (stplyr->pflags & PF_WANTSTOJOIN)
				itemtxt = M_GetText("Item - Cancel Join");
			else if (G_GametypeHasTeams())
				itemtxt = M_GetText("Item - Join Team");

			if (cv_ingamecap.value)
			{
				UINT8 numingame = 0;
				UINT8 i;

				for (i = 0; i < MAXPLAYERS; i++)
					if (playeringame[i] && !players[i].spectator)
						numingame++;

				itemtxt = va("%s (%s: %d)", itemtxt, M_GetText("Slots left"), max(0, cv_ingamecap.value - numingame));
			}

			// SRB2kart: changed positions & text
			if (r_splitscreen)
			{
				V_DrawThinString(2, (BASEVIDHEIGHT/2)-20, V_YELLOWMAP|V_HUDTRANSHALF|V_SPLITSCREEN|V_SNAPTOLEFT|V_SNAPTOBOTTOM, M_GetText("- SPECTATING -"));
				V_DrawThinString(2, (BASEVIDHEIGHT/2)-10, V_HUDTRANSHALF|V_SPLITSCREEN|V_SNAPTOLEFT|V_SNAPTOBOTTOM, itemtxt);
			}
			else
			{
				V_DrawString(2, BASEVIDHEIGHT-40, V_HUDTRANSHALF|V_SPLITSCREEN|V_YELLOWMAP|V_SNAPTOLEFT|V_SNAPTOBOTTOM, M_GetText("- SPECTATING -"));
				V_DrawString(2, BASEVIDHEIGHT-30, V_HUDTRANSHALF|V_SPLITSCREEN|V_SNAPTOLEFT|V_SNAPTOBOTTOM, itemtxt);
				V_DrawString(2, BASEVIDHEIGHT-20, V_HUDTRANSHALF|V_SPLITSCREEN|V_SNAPTOLEFT|V_SNAPTOBOTTOM, M_GetText("Accelerate - Float"));
				V_DrawString(2, BASEVIDHEIGHT-10, V_HUDTRANSHALF|V_SPLITSCREEN|V_SNAPTOLEFT|V_SNAPTOBOTTOM, M_GetText("Brake - Sink"));
			}
		}
	}
}

void ST_DrawDemoTitleEntry(void)
{
	static UINT8 skullAnimCounter = 0;
	char *nametodraw;

	skullAnimCounter++;
	skullAnimCounter %= 8;

	nametodraw = demo.titlename;
	while (V_StringWidth(nametodraw, 0) > MAXSTRINGLENGTH*8 - 8)
		nametodraw++;

#define x (BASEVIDWIDTH/2 - 139)
#define y (BASEVIDHEIGHT/2)
	M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);
	V_DrawString(x + 8, y + 12, V_ALLOWLOWERCASE, nametodraw);
	if (skullAnimCounter < 4)
		V_DrawCharacter(x + 8 + V_StringWidth(nametodraw, 0), y + 12,
			'_' | 0x80, false);

	M_DrawTextBox(x + 30, y - 24, 26, 1);
	V_DrawString(x + 38, y - 16, V_ALLOWLOWERCASE, "Enter the name of the replay.");

	M_DrawTextBox(x + 50, y + 20, 20, 1);
	V_DrawThinString(x + 58, y + 28, V_ALLOWLOWERCASE, "Escape - Cancel");
	V_DrawRightAlignedThinString(x + 220, y + 28, V_ALLOWLOWERCASE, "Enter - Confirm");
#undef x
#undef y
}

// MayonakaStatic: draw Midnight Channel's TV-like borders
static void ST_MayonakaStatic(void)
{
	INT32 flag;
	if (cv_lessflicker.value)
		flag = V_70TRANS;
	else
		flag = (leveltime%2) ? V_90TRANS : V_70TRANS;

	V_DrawFixedPatch(0, 0, FRACUNIT, V_SNAPTOTOP|V_SNAPTOLEFT|flag, hud_tv1, NULL);
	V_DrawFixedPatch(320<<FRACBITS, 0, FRACUNIT, V_SNAPTOTOP|V_SNAPTORIGHT|V_FLIP|flag, hud_tv1, NULL);
	V_DrawFixedPatch(0, 142<<FRACBITS, FRACUNIT, V_SNAPTOBOTTOM|V_SNAPTOLEFT|flag, hud_tv2, NULL);
	V_DrawFixedPatch(320<<FRACBITS, 142<<FRACBITS, FRACUNIT, V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_FLIP|flag, hud_tv2, NULL);
}

#ifdef HAVE_DISCORDRPC
void ST_AskToJoinEnvelope(void)
{
	const tic_t freq = TICRATE/2;

	if (menuactive)
		return;

	if ((leveltime % freq) < freq/2)
		return;

	V_DrawFixedPatch(296*FRACUNIT, 2*FRACUNIT, FRACUNIT, V_SNAPTOTOP|V_SNAPTORIGHT, envelope, NULL);
	// maybe draw number of requests with V_DrawPingNum ?
}
#endif

void ST_Drawer(void)
{
	boolean stagetitle = false; // Decide whether to draw the stage title or not

	// Doom's status bar only updated if necessary.
	// However, ours updates every frame regardless, so the "refresh" param was removed
	//(void)refresh;

	// force a set of the palette by using doPaletteStuff()
	if (vid.recalc)
		st_palette = -1;

	// Do red-/gold-shifts from damage/items
#ifdef HWRENDER
	//25/08/99: Hurdler: palette changes is done for all players,
	//                   not only player1! That's why this part
	//                   of code is moved somewhere else.
	if (rendermode == render_soft)
#endif
		if (rendermode != render_none) ST_doPaletteStuff();

	{
		const tic_t length = TICRATE/2;

		if (lt_exitticker)
		{
			st_translucency = cv_translucenthud.value;
			if (lt_exitticker < length)
				st_translucency = (((INT32)(lt_ticker - lt_endtime))*st_translucency)/((INT32)length);
		}
		else
			st_translucency = 0;
	}

	// Check for a valid level title
	// If the HUD is enabled
	// And, if Lua is running, if the HUD library has the stage title enabled
	if ((stagetitle = (G_IsTitleCardAvailable() && *mapheaderinfo[gamemap-1]->lvlttl != '\0' && !(hu_showscores && (netgame || multiplayer)))))
		ST_preDrawTitleCard();

	if (st_overlay)
	{
		UINT8 i;
		// No deadview!
		for (i = 0; i <= r_splitscreen; i++)
		{
			stplyr = &players[displayplayers[i]];
			R_SetViewContext(VIEWCONTEXT_PLAYER1 + i);
			R_InterpolateView(rendertimefrac); // to assist with object tracking
			ST_overlayDrawer();
		}

		// draw Midnight Channel's overlay ontop
		if (mapheaderinfo[gamemap-1]->typeoflevel & TOL_TV)	// Very specific Midnight Channel stuff.
			ST_MayonakaStatic();
	}

	// Draw a white fade on level opening
	if (timeinmap < 15)
	{
		if (timeinmap <= 5)
			V_DrawFill(0,0,BASEVIDWIDTH,BASEVIDHEIGHT,120); // Pure white on first few frames, to hide SRB2's awful level load artifacts
		else
			V_DrawFadeScreen(120, 15-timeinmap); // Then gradually fade out from there
	}

	if (stagetitle)
		ST_drawTitleCard();

	// Replay manual-save stuff
	if (demo.recording && multiplayer && demo.savebutton && demo.savebutton + 3*TICRATE < leveltime)
	{
		switch (demo.savemode)
		{
		case DSM_NOTSAVING:
			V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, V_HUDTRANS|V_SNAPTOTOP|V_SNAPTORIGHT|V_ALLOWLOWERCASE|((gametyperules & GTR_BUMPERS) ? V_REDMAP : V_SKYMAP), "Look Backward: Save replay");
			break;

		case DSM_WILLAUTOSAVE:
			V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, V_HUDTRANS|V_SNAPTOTOP|V_SNAPTORIGHT|V_ALLOWLOWERCASE|((gametyperules & GTR_BUMPERS) ? V_REDMAP : V_SKYMAP), "Replay will be saved. (Look Backward: Change title)");
			break;

		case DSM_WILLSAVE:
			V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, V_HUDTRANS|V_SNAPTOTOP|V_SNAPTORIGHT|V_ALLOWLOWERCASE|((gametyperules & GTR_BUMPERS) ? V_REDMAP : V_SKYMAP), "Replay will be saved.");
			break;

		case DSM_TITLEENTRY:
			ST_DrawDemoTitleEntry();
			break;

		default: // Don't render anything
			break;
		}
	}

	ST_drawDebugInfo();
}
