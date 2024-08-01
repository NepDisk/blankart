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
/// \file  r_main.c
/// \brief Rendering main loop and setup functions,
///        utility functions (BSP, geometry, trigonometry).
///        See tables.c, too.

#include "doomdef.h"
#include "g_game.h"
#include "g_input.h"
#include "r_local.h"
#include "r_splats.h" // faB(21jan): testing
#include "r_sky.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "p_local.h"
#include "keys.h"
#include "i_video.h"
#include "m_menu.h"
#include "am_map.h"
#include "d_main.h"
#include "v_video.h"
//#include "p_spec.h"
#include "p_setup.h"
#include "z_zone.h"
#include "m_random.h" // quake camera shake
#include "r_portal.h"
#include "r_main.h"
#include "i_system.h" // I_GetPreciseTime
#include "doomstat.h" // MAXSPLITSCREENPLAYERS
#include "r_fps.h" // Frame interpolation/uncapped

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

//profile stuff ---------------------------------------------------------
//#define TIMING
#ifdef TIMING
#include "p5prof.h"
INT64 mycount;
INT64 mytotal = 0;
//unsigned long  nombre = 100000;
#endif
//profile stuff ---------------------------------------------------------

// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 2048

// increment every time a check is made
size_t validcount = 1;

INT32 centerx, centery;

fixed_t centerxfrac, centeryfrac;
fixed_t projection[MAXSPLITSCREENPLAYERS];
fixed_t projectiony[MAXSPLITSCREENPLAYERS]; // aspect ratio
fixed_t fovtan[MAXSPLITSCREENPLAYERS]; // field of view

// just for profiling purposes
size_t framecount;

size_t loopcount;

fixed_t viewx, viewy, viewz;
angle_t viewangle, aimingangle, viewroll;
UINT8 viewssnum;
fixed_t viewcos, viewsin;
sector_t *viewsector;
player_t *viewplayer;
mobj_t *r_viewmobj;

int r_splitscreen;

fixed_t rendertimefrac;
fixed_t renderdeltatics;
boolean renderisnewtic;

//
// precalculated math tables
//
angle_t clipangle[MAXSPLITSCREENPLAYERS];
angle_t doubleclipangle[MAXSPLITSCREENPLAYERS];

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
INT32 viewangletox[MAXSPLITSCREENPLAYERS][FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t xtoviewangle[MAXSPLITSCREENPLAYERS][MAXVIDWIDTH+1];

lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t *scalelightfixed[MAXLIGHTSCALE];
lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

// Hack to support extra boom colormaps.
extracolormap_t *extra_colormaps = NULL;

// Render stats
precise_t ps_prevframetime = 0;
precise_t ps_rendercalltime = 0;
precise_t ps_uitime = 0;
precise_t ps_swaptime = 0;

precise_t ps_bsptime = 0;

precise_t ps_sw_spritecliptime = 0;
precise_t ps_sw_portaltime = 0;
precise_t ps_sw_planetime = 0;
precise_t ps_sw_maskedtime = 0;

int ps_numbspcalls = 0;
int ps_numsprites = 0;
int ps_numdrawnodes = 0;
int ps_numpolyobjects = 0;

static CV_PossibleValue_t drawdist_cons_t[] = {
	/*{256, "256"},*/	{512, "512"},	{768, "768"},
	{1024, "1024"},	{1536, "1536"},	{2048, "2048"},
	{3072, "3072"},	{4096, "4096"},	{6144, "6144"},
	{8192, "8192"},	{0, "Infinite"},	{0, NULL}};

static CV_PossibleValue_t drawdist_precip_cons_t[] = {
	{256, "256"},	{512, "512"},	{768, "768"},
	{1024, "1024"},	{1536, "1536"},	{2048, "2048"},
	{0, "None"},	{0, NULL}};

static CV_PossibleValue_t fov_cons_t[] = {{60*FRACUNIT, "MIN"}, {179*FRACUNIT, "MAX"}, {0, NULL}};
static CV_PossibleValue_t translucenthud_cons_t[] = {{0, "MIN"}, {10, "MAX"}, {0, NULL}};
static CV_PossibleValue_t maxportals_cons_t[] = {{0, "MIN"}, {12, "MAX"}, {0, NULL}}; // lmao rendering 32 portals, you're a card
static CV_PossibleValue_t homremoval_cons_t[] = {{0, "No"}, {1, "Yes"}, {2, "Flash"}, {0, NULL}};

static void Fov_OnChange(void);
static void ChaseCam_OnChange(void);
static void ChaseCam2_OnChange(void);
static void ChaseCam3_OnChange(void);
static void ChaseCam4_OnChange(void);
static void FlipCam_OnChange(void);
static void FlipCam2_OnChange(void);
static void FlipCam3_OnChange(void);
static void FlipCam4_OnChange(void);

consvar_t cv_tailspickup = CVAR_INIT ("tailspickup", "On", CV_NETVAR|CV_NOSHOWHELP, CV_OnOff, NULL);
consvar_t cv_chasecam[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("chasecam", "On", CV_CALL, CV_OnOff, ChaseCam_OnChange),
	CVAR_INIT ("chasecam2", "On", CV_CALL, CV_OnOff, ChaseCam2_OnChange),
	CVAR_INIT ("chasecam3", "On", CV_CALL, CV_OnOff, ChaseCam3_OnChange),
	CVAR_INIT ("chasecam4", "On", CV_CALL, CV_OnOff, ChaseCam4_OnChange)
};

consvar_t cv_flipcam[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("flipcam", "Off", CV_CALL|CV_SAVE, CV_OnOff, weaponPrefChange),
	CVAR_INIT ("flipcam2", "Off", CV_CALL|CV_SAVE, CV_OnOff, weaponPrefChange2),
	CVAR_INIT ("flipcam3", "Off", CV_CALL|CV_SAVE, CV_OnOff, weaponPrefChange3),
	CVAR_INIT ("flipcam4", "Off", CV_CALL|CV_SAVE, CV_OnOff, weaponPrefChange4)
};

consvar_t cv_shadow = CVAR_INIT ("shadow", "On", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_skybox = CVAR_INIT ("skybox", "On", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_ffloorclip = CVAR_INIT ("ffloorclip", "On", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_allowmlook = CVAR_INIT ("allowmlook", "Yes", CV_NETVAR, CV_YesNo, NULL);
consvar_t cv_showhud = CVAR_INIT ("showhud", "Yes", CV_CALL,  CV_YesNo, R_SetViewSize);
consvar_t cv_translucenthud = CVAR_INIT ("translucenthud", "10", CV_SAVE, translucenthud_cons_t, NULL);

consvar_t cv_drawdist = CVAR_INIT ("drawdist", "8192", CV_SAVE, drawdist_cons_t, NULL);
consvar_t cv_drawdist_precip = CVAR_INIT ("drawdist_precip", "1024", CV_SAVE, drawdist_precip_cons_t, NULL);

consvar_t cv_fov[MAXSPLITSCREENPLAYERS] = {
	CVAR_INIT ("fov", "90", CV_FLOAT|CV_CALL|CV_SAVE, fov_cons_t, Fov_OnChange),
	CVAR_INIT ("fov2", "90", CV_FLOAT|CV_CALL|CV_SAVE, fov_cons_t, Fov_OnChange),
	CVAR_INIT ("fov3", "90", CV_FLOAT|CV_CALL|CV_SAVE, fov_cons_t, Fov_OnChange),
	CVAR_INIT ("fov4", "90", CV_FLOAT|CV_CALL|CV_SAVE, fov_cons_t, Fov_OnChange)
};

// Okay, whoever said homremoval causes a performance hit should be shot.
consvar_t cv_homremoval = CVAR_INIT ("homremoval", "Yes", CV_SAVE, homremoval_cons_t, NULL);

consvar_t cv_maxportals = CVAR_INIT ("maxportals", "2", CV_SAVE, maxportals_cons_t, NULL);

consvar_t cv_renderstats = CVAR_INIT ("renderstats", "Off", 0, CV_OnOff, NULL);

void SplitScreen_OnChange(void)
{
	UINT8 i;

	/*
	local splitscreen is updated before you're in a game,
	so this is the first value for renderer splitscreen
	*/
	r_splitscreen = splitscreen;

	// recompute screen size
	R_ExecuteSetViewSize();

	if (!demo.playback)
	{
		for (i = 1; i < MAXSPLITSCREENPLAYERS; i++)
		{
			if (i > splitscreen)
				CL_RemoveSplitscreenPlayer(displayplayers[i]);
			else
				CL_AddSplitscreenPlayer();
		}

		if (server && !netgame)
			multiplayer = splitscreen;
	}
	else
	{
		for (i = 1; i < MAXSPLITSCREENPLAYERS; i++)
			displayplayers[i] = consoleplayer;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && i != consoleplayer)
			{
				UINT8 j;
				for (j = 1; j < MAXSPLITSCREENPLAYERS; j++)
				{
					if (displayplayers[j] == consoleplayer)
					{
						displayplayers[j] = i;
						break;
					}
				}

				if (j == MAXSPLITSCREENPLAYERS)
					break;
			}
		}
	}
}
static void Fov_OnChange(void)
{
	R_SetViewSize();
}

static void ChaseCam_OnChange(void)
{
	;
}

static void ChaseCam2_OnChange(void)
{
	;
}

static void ChaseCam3_OnChange(void)
{
	;
}

static void ChaseCam4_OnChange(void)
{
	;
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
// check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//
INT32 R_PointOnSide(fixed_t x, fixed_t y, node_t *node)
{
	if (!node->dx)
		return x <= node->x ? node->dy > 0 : node->dy < 0;

	if (!node->dy)
		return y <= node->y ? node->dx < 0 : node->dx > 0;

	fixed_t dx = (x >> 1) - (node->x >> 1);
	fixed_t dy = (y >> 1) - (node->y >> 1);

	// Try to quickly decide by looking at sign bits.
	if ((node->dy ^ node->dx ^ dx ^ dy) < 0)
		return (node->dy ^ dx) < 0;  // (left is negative)
	return FixedMul(dy, node->dx>>FRACBITS) >= FixedMul(node->dy>>FRACBITS, dx);
}

// killough 5/2/98: reformatted
INT32 R_PointOnSegSide(fixed_t x, fixed_t y, seg_t *line)
{
	fixed_t lx = line->v1->x;
	fixed_t ly = line->v1->y;
	fixed_t ldx = line->v2->x - lx;
	fixed_t ldy = line->v2->y - ly;

	if (!ldx)
		return x <= lx ? ldy > 0 : ldy < 0;

	if (!ldy)
		return y <= ly ? ldx < 0 : ldx > 0;

	fixed_t dx = (x >> 1) - (lx >> 1);
	fixed_t dy = (y >> 1) - (ly >> 1);

	// Try to quickly decide by looking at sign bits.
	if ((ldy ^ ldx ^ dx ^ dy) < 0)
		return (ldy ^ dx) < 0;          // (left is negative)
	return FixedMul(dy, ldx>>FRACBITS) >= FixedMul(ldy>>FRACBITS, dx);
}

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table. The +1 size of tantoangle[]
//  is to handle the case when x==y without additional
//  checking.
//
// killough 5/2/98: reformatted, cleaned up

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
	return R_PointToAngle2(viewx, viewy, x, y);
}

// Similar to R_PointToAngle, but requires an additional player_t argument.
// If this player is a local displayplayer, this will base off the calculations off of their camera instead, otherwise use viewx/viewy as usual.
// Yes this is kinda ghetto.
angle_t R_PointToAnglePlayer(player_t *player, fixed_t x, fixed_t y)
{
	fixed_t refx = viewx, refy = viewy;
	camera_t *cam = NULL;
	UINT8 i;

	for (i = 0; i < r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			cam = &camera[i];
			break;
		}
	}

	// use whatever cam we found's coordinates.
	if (cam != NULL)
	{
		refx = cam->x;
		refy = cam->y;
	}

	return R_PointToAngle2(refx, refy, x, y);
}

// This version uses 64-bit variables to avoid overflows with large values.
// Currently used only by OpenGL rendering.
angle_t R_PointToAngle64(INT64 x, INT64 y)
{
	return (y -= viewy, (x -= viewx) || y) ?
	x >= 0 ?
	y >= 0 ?
		(x > y) ? tantoangle[SlopeDivEx(y,x)] :                            // octant 0
		ANGLE_90-tantoangle[SlopeDivEx(x,y)] :                               // octant 1
		x > (y = -y) ? 0-tantoangle[SlopeDivEx(y,x)] :                    // octant 8
		ANGLE_270+tantoangle[SlopeDivEx(x,y)] :                              // octant 7
		y >= 0 ? (x = -x) > y ? ANGLE_180-tantoangle[SlopeDivEx(y,x)] :  // octant 3
		ANGLE_90 + tantoangle[SlopeDivEx(x,y)] :                             // octant 2
		(x = -x) > (y = -y) ? ANGLE_180+tantoangle[SlopeDivEx(y,x)] :    // octant 4
		ANGLE_270-tantoangle[SlopeDivEx(x,y)] :                              // octant 5
		0;
}

angle_t R_PointToAngle2(fixed_t pviewx, fixed_t pviewy, fixed_t x, fixed_t y)
{
	return (y -= pviewy, (x -= pviewx) || y) ?
	x >= 0 ?
	y >= 0 ?
		(x > y) ? tantoangle[SlopeDiv(y,x)] :                          // octant 0
		ANGLE_90-tantoangle[SlopeDiv(x,y)] :                           // octant 1
		x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :                   // octant 8
		ANGLE_270+tantoangle[SlopeDiv(x,y)] :                          // octant 7
		y >= 0 ? (x = -x) > y ? ANGLE_180-tantoangle[SlopeDiv(y,x)] :  // octant 3
		ANGLE_90 + tantoangle[SlopeDiv(x,y)] :                         // octant 2
		(x = -x) > (y = -y) ? ANGLE_180+tantoangle[SlopeDiv(y,x)] :    // octant 4
		ANGLE_270-tantoangle[SlopeDiv(x,y)] :                          // octant 5
		0;
}

fixed_t R_PointToDist2(fixed_t px2, fixed_t py2, fixed_t px1, fixed_t py1)
{
	return FixedHypot(px1 - px2, py1 - py2);
}

// Little extra utility. Works in the same way as R_PointToAngle2
fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
	return R_PointToDist2(viewx, viewy, x, y);
}

angle_t R_PointToAngleEx(INT32 x2, INT32 y2, INT32 x1, INT32 y1)
{
	INT64 dx = x1-x2;
	INT64 dy = y1-y2;
	if (dx < INT32_MIN || dx > INT32_MAX || dy < INT32_MIN || dy > INT32_MAX)
	{
		x1 = (int)(dx / 2 + x2);
		y1 = (int)(dy / 2 + y2);
	}
	return (y1 -= y2, (x1 -= x2) || y1) ?
	x1 >= 0 ?
	y1 >= 0 ?
		(x1 > y1) ? tantoangle[SlopeDivEx(y1,x1)] :                            // octant 0
		ANGLE_90-tantoangle[SlopeDivEx(x1,y1)] :                               // octant 1
		x1 > (y1 = -y1) ? 0-tantoangle[SlopeDivEx(y1,x1)] :                    // octant 8
		ANGLE_270+tantoangle[SlopeDivEx(x1,y1)] :                              // octant 7
		y1 >= 0 ? (x1 = -x1) > y1 ? ANGLE_180-tantoangle[SlopeDivEx(y1,x1)] :  // octant 3
		ANGLE_90 + tantoangle[SlopeDivEx(x1,y1)] :                             // octant 2
		(x1 = -x1) > (y1 = -y1) ? ANGLE_180+tantoangle[SlopeDivEx(y1,x1)] :    // octant 4
		ANGLE_270-tantoangle[SlopeDivEx(x1,y1)] :                              // octant 5
		0;
}

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
// killough 5/2/98: reformatted, cleaned up
//
// note: THIS IS USED ONLY FOR WALLS!
fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
	angle_t anglea = ANGLE_90 + (visangle-viewangle);
	angle_t angleb = ANGLE_90 + (visangle-rw_normalangle);
	fixed_t den = FixedMul(rw_distance, FINESINE(anglea>>ANGLETOFINESHIFT));
	// proff 11/06/98: Changed for high-res
	fixed_t num = FixedMul(projectiony[viewssnum],
			FINESINE(angleb>>ANGLETOFINESHIFT));

	if (den > num>>16)
	{
		num = FixedDiv(num, den);
		if (num > 64*FRACUNIT)
			return 64*FRACUNIT;
		if (num < 256)
			return 256;
		return num;
	}
	return 64*FRACUNIT;
}

//
// R_DoCulling
// Checks viewz and top/bottom heights of an item against culling planes
// Returns true if the item is to be culled, i.e it shouldn't be drawn!
// if ML_NOCLIMB is set, the camera view is required to be in the same area for culling to occur
boolean R_DoCulling(line_t *cullheight, line_t *viewcullheight, fixed_t vz, fixed_t bottomh, fixed_t toph)
{
	fixed_t cullplane;

	if (!cullheight)
		return false;

	cullplane = cullheight->frontsector->floorheight;
	if (cullheight->flags & ML_NOCLIMB) // Group culling
	{
		if (!viewcullheight)
			return false;

		// Make sure this is part of the same group
		if (viewcullheight->frontsector == cullheight->frontsector)
		{
			// OK, we can cull
			if (vz > cullplane && toph < cullplane) // Cull if below plane
				return true;

			if (bottomh > cullplane && vz <= cullplane) // Cull if above plane
				return true;
		}
	}
	else // Quick culling
	{
		if (vz > cullplane && toph < cullplane) // Cull if below plane
			return true;

		if (bottomh > cullplane && vz <= cullplane) // Cull if above plane
			return true;
	}

	return false;
}

// Returns search dimensions within a blockmap, in the direction of viewangle and out to a certain distance.
void R_GetRenderBlockMapDimensions(fixed_t drawdist, INT32 *xl, INT32 *xh, INT32 *yl, INT32 *yh)
{
	const angle_t left = viewangle - clipangle[viewssnum];
	const angle_t right = viewangle + clipangle[viewssnum];

	const fixed_t vxleft = viewx + FixedMul(drawdist, FCOS(left));
	const fixed_t vyleft = viewy + FixedMul(drawdist, FSIN(left));

	const fixed_t vxright = viewx + FixedMul(drawdist, FCOS(right));
	const fixed_t vyright = viewy + FixedMul(drawdist, FSIN(right));

	// Try to narrow the search to within only the field of view
	*xl = (unsigned)(min(viewx, min(vxleft, vxright)) - bmaporgx)>>MAPBLOCKSHIFT;
	*xh = (unsigned)(max(viewx, max(vxleft, vxright)) - bmaporgx)>>MAPBLOCKSHIFT;
	*yl = (unsigned)(min(viewy, min(vyleft, vyright)) - bmaporgy)>>MAPBLOCKSHIFT;
	*yh = (unsigned)(max(viewy, max(vyleft, vyright)) - bmaporgy)>>MAPBLOCKSHIFT;

	if (*xh >= bmapwidth)
		*xh = bmapwidth - 1;

	if (*yh >= bmapheight)
		*yh = bmapheight - 1;

	BMBOUNDFIX(*xl, *xh, *yl, *yh);
}

//
// R_InitTextureMapping
//
static void R_InitTextureMapping(int s)
{
	INT32 i;
	INT32 x;
	INT32 t;
	fixed_t focallength;

	// Use tangent table to generate viewangletox:
	//  viewangletox will give the next greatest x
	//  after the view angle.
	//
	// Calc focallength
	//  so FIELDOFVIEW angles covers SCREENWIDTH.
	focallength = FixedDiv(projection[s],
		FINETANGENT(FINEANGLES/4+FIELDOFVIEW/2));

	focallengthf[s] = FIXED_TO_FLOAT(focallength);

	for (i = 0; i < FINEANGLES/2; i++)
	{
		if (FINETANGENT(i) > fovtan[s]*2)
			t = -1;
		else if (FINETANGENT(i) < -fovtan[s]*2)
			t = viewwidth+1;
		else
		{
			t = FixedMul(FINETANGENT(i), focallength);
			t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

			if (t < -1)
				t = -1;
			else if (t > viewwidth+1)
				t = viewwidth+1;
		}
		viewangletox[s][i] = t;
	}

	// Scan viewangletox[] to generate xtoviewangle[]:
	//  xtoviewangle will give the smallest view angle
	//  that maps to x.
	for (x = 0; x <= viewwidth;x++)
	{
		i = 0;
		while (viewangletox[s][i] > x)
			i++;
		xtoviewangle[s][x] = (i<<ANGLETOFINESHIFT) - ANGLE_90;
	}

	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES/2; i++)
	{
		if (viewangletox[s][i] == -1)
			viewangletox[s][i] = 0;
		else if (viewangletox[s][i] == viewwidth+1)
			viewangletox[s][i]  = viewwidth;
	}

	clipangle[s] = xtoviewangle[s][0];
	doubleclipangle[s] = clipangle[s]*2;
}



//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP 2

static inline void R_InitLightTables(void)
{
	INT32 i;
	INT32 j;
	INT32 level;
	INT32 startmapl;
	INT32 scale;

	// Calculate the light levels to use
	//  for each level / distance combination.
	for (i = 0; i < LIGHTLEVELS; i++)
	{
		startmapl = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j = 0; j < MAXLIGHTZ; j++)
		{
			//added : 02-02-98 : use BASEVIDWIDTH, vid.width is not set already,
			// and it seems it needs to be calculated only once.
			scale = FixedDiv((BASEVIDWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT;
			level = startmapl - scale/DISTMAP;

			if (level < 0)
				level = 0;

			if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			zlight[i][j] = colormaps + level*256;
		}
	}
}

//#define WOUGHMP_WOUGHMP // I got a fish-eye lens - I'll make a rap video with a couple of friends
// it's kinda laggy sometimes

#ifdef WOUGHMP_WOUGHMP
#define AHHHH_IM_SO_MAAAAD { 0U, 0, FRACUNIT, NULL, 0, 0, {0}, {0}, false }
#else
#define AHHHH_IM_SO_MAAAAD { 0U,    FRACUNIT, NULL, 0, 0, {0}, {0}, false }
#endif

static struct viewmorph {
	angle_t rollangle; // pre-shifted by fineshift
#ifdef WOUGHMP_WOUGHMP
	fixed_t fisheye;
#endif

	fixed_t zoomneeded;
	INT32 *scrmap;
	INT32 scrmapsize;

	INT32 x1; // clip rendering horizontally for efficiency
	INT16 ceilingclip[MAXVIDWIDTH], floorclip[MAXVIDWIDTH];

	boolean use;
} viewmorph[MAXSPLITSCREENPLAYERS] = {
	AHHHH_IM_SO_MAAAAD,
	AHHHH_IM_SO_MAAAAD,
	AHHHH_IM_SO_MAAAAD,
	AHHHH_IM_SO_MAAAAD,
};

#undef AHHHH_IM_SO_MAAAAD

void R_CheckViewMorph(int s)
{
	struct viewmorph * v = &viewmorph[s];

	float zoomfactor, rollcos, rollsin;
	float x1, y1, x2, y2;
	fixed_t temp;
	INT32 end, vx, vy, pos, usedpos;
	INT32 realend;
	INT32 usedx, usedy;

	INT32 width = vid.width;
	INT32 height = vid.height;

	INT32 halfwidth;
	INT32 halfheight;

#ifdef WOUGHMP_WOUGHMP
	float fisheyemap[MAXVIDWIDTH/2 + 1];
#endif

	angle_t rollangle = viewroll;
#ifdef WOUGHMP_WOUGHMP
	fixed_t fisheye = cv_cam2_turnmultiplier.value; // temporary test value
#endif

	rollangle >>= ANGLETOFINESHIFT;
	rollangle = ((rollangle+2) & ~3) & FINEMASK; // Limit the distinct number of angles to reduce recalcs from angles changing a lot.

#ifdef WOUGHMP_WOUGHMP
	fisheye &= ~0x7FF; // Same
#endif

	if (r_splitscreen > 0)
	{
		height /= 2;

		if (r_splitscreen > 1)
		{
			width /= 2;
		}
	}

	halfwidth = width / 2;
	halfheight = height / 2;

	if (rollangle == v->rollangle &&
#ifdef WOUGHMP_WOUGHMP
		fisheye == v->fisheye &&
#endif
		v->scrmapsize == width * height)
		return; // No change

	v->rollangle = rollangle;
#ifdef WOUGHMP_WOUGHMP
	v->fisheye = fisheye;
#endif

	if (v->rollangle == 0
#ifdef WOUGHMP_WOUGHMP
		 && v->fisheye == 0
#endif
	 )
	{
		v->use = false;
		v->x1 = 0;
		if (v->zoomneeded != FRACUNIT)
			R_SetViewSize();
		v->zoomneeded = FRACUNIT;

		return;
	}

	if (v->scrmapsize != width * height)
	{
		if (v->scrmap)
			free(v->scrmap);
		v->scrmap = malloc(width * height * sizeof(INT32));
		v->scrmapsize = width * height;
	}

	temp = FINECOSINE(rollangle);
	rollcos = FIXED_TO_FLOAT(temp);
	temp = FINESINE(rollangle);
	rollsin = FIXED_TO_FLOAT(temp);

	// Calculate maximum zoom needed
	x1 = (width  * fabsf(rollcos) + height * fabsf(rollsin)) / width;
	y1 = (height * fabsf(rollcos) + width  * fabsf(rollsin)) / height;

#ifdef WOUGHMP_WOUGHMP
	if (fisheye)
	{
		float f = FIXED_TO_FLOAT(fisheye);
		for (vx = 0; vx <= halfwidth; vx++)
			fisheyemap[vx] = 1.0f / cos(atan(vx * f / halfwidth));

		f = cos(atan(f));
		if (f < 1.0f)
		{
			x1 /= f;
			y1 /= f;
		}
	}
#endif

	temp = max(x1, y1)*FRACUNIT;
	if (temp < FRACUNIT)
		temp = FRACUNIT;
	else
		temp |= 0x3FFF; // Limit how many times the viewport needs to be recalculated

	//CONS_Printf("Setting zoom to %f\n", FIXED_TO_FLOAT(temp));

	if (temp != v->zoomneeded)
	{
		v->zoomneeded = temp;
		R_SetViewSize();
	}

	zoomfactor = FIXED_TO_FLOAT(v->zoomneeded);

	realend = end = width * height - 1;

	if (r_splitscreen > 1)
	{
		realend = ( realend << 1 ) - width;
	}

	pos = 0;

	// Pre-multiply rollcos and rollsin to use for positional stuff
	rollcos /= zoomfactor;
	rollsin /= zoomfactor;

	x1 = -(halfwidth * rollcos - halfheight * rollsin);
	y1 = -(halfheight * rollcos + halfwidth * rollsin);

#ifdef WOUGHMP_WOUGHMP
	if (fisheye)
		v->x1 = (INT32)(halfwidth - (halfwidth * fabsf(rollcos) + halfheight * fabsf(rollsin)) * fisheyemap[halfwidth]);
	else
#endif
	v->x1 = (INT32)(halfwidth - (halfwidth * fabsf(rollcos) + halfheight * fabsf(rollsin)));
	//CONS_Printf("saving %d cols\n", v->x1);

	// Set ceilingclip and floorclip
	for (vx = 0; vx < width; vx++)
	{
		v->ceilingclip[vx] = height;
		v->floorclip[vx] = -1;
	}
	x2 = x1;
	y2 = y1;
	for (vx = 0; vx < width; vx++)
	{
		INT16 xa, ya, xb, yb;
		xa = x2+halfwidth;
		ya = y2+halfheight-1;
		xb = width-1-xa;
		yb = height-1-ya;

		v->ceilingclip[xa] = min(v->ceilingclip[xa], ya);
		v->floorclip[xa] = max(v->floorclip[xa], ya);
		v->ceilingclip[xb] = min(v->ceilingclip[xb], yb);
		v->floorclip[xb] = max(v->floorclip[xb], yb);
		x2 += rollcos;
		y2 += rollsin;
	}
	x2 = x1;
	y2 = y1;
	for (vy = 0; vy < height; vy++)
	{
		INT16 xa, ya, xb, yb;
		xa = x2+halfwidth;
		ya = y2+halfheight;
		xb = width-1-xa;
		yb = height-1-ya;

		v->ceilingclip[xa] = min(v->ceilingclip[xa], ya);
		v->floorclip[xa] = max(v->floorclip[xa], ya);
		v->ceilingclip[xb] = min(v->ceilingclip[xb], yb);
		v->floorclip[xb] = max(v->floorclip[xb], yb);
		x2 -= rollsin;
		y2 += rollcos;
	}

	//CONS_Printf("Top left corner is %f %f\n", x1, y1);

#ifdef WOUGHMP_WOUGHMP
	if (fisheye)
	{
		for (vy = 0; vy < halfheight; vy++)
		{
			x2 = x1;
			y2 = y1;
			x1 -= rollsin;
			y1 += rollcos;

			for (vx = 0; vx < width; vx++)
			{
				usedx = halfwidth + x2*fisheyemap[(int) floorf(fabsf(y2*zoomfactor))];
				usedy = halfheight + y2*fisheyemap[(int) floorf(fabsf(x2*zoomfactor))];

				usedpos = usedx + usedy*width;

				v->scrmap[pos] = usedpos;
				v->scrmap[end-pos] = end-usedpos;

				x2 += rollcos;
				y2 += rollsin;
				pos++;
			}
		}
	}
	else
	{
#endif
	x1 += halfwidth;
	y1 += halfheight;

	for (vy = 0; vy < halfheight; vy++)
	{
		x2 = x1;
		y2 = y1;
		x1 -= rollsin;
		y1 += rollcos;

		for (vx = 0; vx < width; vx++)
		{
			usedx = x2;
			usedy = y2;

			usedpos = usedx + usedy * vid.width;

			v->scrmap[pos] = usedpos;
			v->scrmap[end-pos] = realend-usedpos;

			x2 += rollcos;
			y2 += rollsin;
			pos++;
		}
	}
#ifdef WOUGHMP_WOUGHMP
	}
#endif

	v->use = true;
}

void R_ApplyViewMorph(int s)
{
	UINT8 *tmpscr = screens[4];
	UINT8 *srcscr = screens[0];
	INT32 width = vid.width;
	INT32 height = vid.height;
	INT32 p;
	INT32 end;

	if (!viewmorph[s].use)
		return;

	if (r_splitscreen == 1)
	{
		height /= 2;

		if (s == 1)
		{
			srcscr += vid.width * height;
		}
	}
	else if (r_splitscreen > 1)
	{
		width /= 2;
		height /= 2;

		if (s % 2)
		{
			srcscr += width;
		}

		if (s > 1)
		{
			srcscr += vid.width * height;
		}
	}

	end = width * height;

#if 0
	if (cv_debug & DBG_VIEWMORPH)
	{
		UINT8 border = 32;
		UINT8 grid = 160;
		INT32 ws = vid.width / 4;
		INT32 hs = vid.width * (vid.height / 4);

		memcpy(tmpscr, srcscr, vid.width*vid.height);
		for (p = 0; p < vid.width; p++)
		{
			tmpscr[viewmorph.scrmap[p]] = border;
			tmpscr[viewmorph.scrmap[p + hs]] = grid;
			tmpscr[viewmorph.scrmap[p + hs*2]] = grid;
			tmpscr[viewmorph.scrmap[p + hs*3]] = grid;
			tmpscr[viewmorph.scrmap[end - 1 - p]] = border;
		}
		for (p = vid.width; p < end; p += vid.width)
		{
			tmpscr[viewmorph.scrmap[p]] = border;
			tmpscr[viewmorph.scrmap[p + ws]] = grid;
			tmpscr[viewmorph.scrmap[p + ws*2]] = grid;
			tmpscr[viewmorph.scrmap[p + ws*3]] = grid;
			tmpscr[viewmorph.scrmap[end - 1 - p]] = border;
		}
	}
	else
#endif
	{
		for (p = 0; p < end; p++)
		{
			tmpscr[p] = srcscr[viewmorph[s].scrmap[p]];
		}
	}

	VID_BlitLinearScreen(tmpscr, srcscr,
			width*vid.bpp, height, width*vid.bpp, vid.width);
}

angle_t R_ViewRollAngle(const player_t *player)
{
	angle_t roll = player->viewrollangle;

	if (cv_tilting.value)
	{
		if (!player->spectator)
		{
			roll += player->tilt;
		}

		if (cv_actionmovie.value)
		{
			int xs = intsign(quake.x),
				 ys = intsign(quake.y),
				 zs = intsign(quake.z);
			roll += (xs ^ ys ^ zs) * ANG1;
		}
	}

	return roll;
}


//
// R_SetViewSize
// Do not really change anything here,
// because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean setsizeneeded;

void R_SetViewSize(void)
{
	setsizeneeded = true;
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize(void)
{
	fixed_t dy;
	INT32 i;
	INT32 j;
	INT32 level;
	INT32 startmapl;
	angle_t fov;
	int s;

	setsizeneeded = false;

	if (rendermode == render_none)
		return;

	// status bar overlay
	st_overlay = cv_showhud.value;

	scaledviewwidth = vid.width;
	viewheight = vid.height;

	if (r_splitscreen)
		viewheight >>= 1;

	viewwidth = scaledviewwidth;

	if (r_splitscreen > 1)
	{
		viewwidth >>= 1;
		scaledviewwidth >>= 1;
	}

	centerx = viewwidth/2;
	centery = viewheight/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	for (s = 0; s <= r_splitscreen; ++s)
	{
		fov = FixedAngle(cv_fov[s].value/2) + ANGLE_90;
		fovtan[s] = FixedMul(FINETANGENT(fov >> ANGLETOFINESHIFT), viewmorph[s].zoomneeded);
		if (r_splitscreen == 1) // Splitscreen FOV should be adjusted to maintain expected vertical view
			fovtan[s] = 17*fovtan[s]/10;

		projection[s] = projectiony[s] = FixedDiv(centerxfrac, fovtan[s]);

		R_InitTextureMapping(s);

		// planes
		if (rendermode == render_soft)
		{
			// this is only used for planes rendering in software mode
			j = viewheight*16;
			for (i = 0; i < j; i++)
			{
				dy = ((i - viewheight*8)<<FRACBITS);
				dy = FixedMul(abs(dy), fovtan[s]);
				yslopetab[s][i] = FixedDiv(centerx*FRACUNIT, dy);
			}
		}
	}

	R_InitViewBuffer(scaledviewwidth, viewheight);

	// thing clipping
	for (i = 0; i < viewwidth; i++)
		screenheightarray[i] = (INT16)viewheight;

	// setup sky scaling
	R_SetSkyScale();

	// planes
	if (rendermode == render_soft)
	{
		if (ds_su)
			Z_Free(ds_su);
		if (ds_sv)
			Z_Free(ds_sv);
		if (ds_sz)
			Z_Free(ds_sz);

		ds_su = ds_sv = ds_sz = NULL;
		ds_sup = ds_svp = ds_szp = NULL;
	}

	memset(scalelight, 0xFF, sizeof(scalelight));

	// Calculate the light levels to use for each level/scale combination.
	for (i = 0; i< LIGHTLEVELS; i++)
	{
		startmapl = ((LIGHTLEVELS - 1 - i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j = 0; j < MAXLIGHTSCALE; j++)
		{
			level = startmapl - j*vid.width/(viewwidth)/DISTMAP;

			if (level < 0)
				level = 0;

			if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS - 1;

			scalelight[i][j] = colormaps + level*256;
		}
	}

	// continue to do the software setviewsize as long as we use the reference software view
#ifdef HWRENDER
	if (rendermode != render_soft)
		HWR_SetViewSize();
#endif

	am_recalc = true;
}

//
// R_Init
//

void R_Init(void)
{
	// screensize independent
	//I_OutputMsg("\nR_InitData");
	//R_InitData(); -- split to d_main for its own startup steps since it takes AGES
	CONS_Printf("R_InitColormaps()...\n");
	R_InitColormaps();

	//I_OutputMsg("\nR_InitViewBorder");
	R_InitViewBorder();
	R_SetViewSize(); // setsizeneeded is set true

	//I_OutputMsg("\nR_InitPlanes");
	R_InitPlanes();

	// this is now done by SCR_Recalc() at the first mode set
	//I_OutputMsg("\nR_InitLightTables");
	R_InitLightTables();

	//I_OutputMsg("\nR_InitTranslucencyTables\n");
	//R_InitTranslucencyTables();

	R_InitDrawNodes();

	framecount = 0;
}

//
// R_PointInSubsector
//
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
	size_t nodenum = numnodes-1;

	while (!(nodenum & NF_SUBSECTOR))
		nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];

	return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_PointInSubsectorOrNull, same as above but returns 0 if not in subsector
//
subsector_t *R_PointInSubsectorOrNull(fixed_t x, fixed_t y)
{
	node_t *node;
	INT32 side, i;
	size_t nodenum;
	subsector_t *ret;
	seg_t *seg;

	// single subsector is a special case
	if (numnodes == 0)
		return subsectors;

	nodenum = numnodes - 1;

	while (!(nodenum & NF_SUBSECTOR))
	{
		node = &nodes[nodenum];
		side = R_PointOnSide(x, y, node);
		nodenum = node->children[side];
	}

	ret = &subsectors[nodenum & ~NF_SUBSECTOR];
	for (i = 0, seg = &segs[ret->firstline]; i < ret->numlines; i++, seg++)
	{
		if (seg->glseg)
			continue;

		//if (R_PointOnSegSide(x, y, seg)) -- breaks in ogl because polyvertex_t cast over vertex pointers
		if (P_PointOnLineSide(x, y, seg->linedef) != seg->side)
			return 0;
	}

	return ret;
}

//
// R_SetupFrame
//

static void
R_SetupCommonFrame
(		player_t * player,
		subsector_t * subsector)
{
	newview->player = player;

	newview->x += quake.x;
	newview->y += quake.y;
	newview->z += quake.z;

	newview->roll = R_ViewRollAngle(player);

	if (subsector)
		newview->sector = subsector->sector;
	else
		newview->sector = R_PointInSubsector(newview->x, newview->y)->sector;

	R_InterpolateView(rendertimefrac);
}

static void R_SetupAimingFrame(int s)
{
	player_t *player = &players[displayplayers[s]];
	camera_t *thiscam = &camera[s];

	if (player->awayviewtics)
	{
		newview->aim = player->awayviewaiming;
		newview->angle = player->awayviewmobj->angle;
	}
	else if (thiscam && thiscam->chase)
	{
		newview->aim = thiscam->aiming;
		newview->angle = thiscam->angle;
	}
	else if (!demo.playback && player->playerstate != PST_DEAD)
	{
		newview->aim = localaiming[s];
		newview->angle = localangle[s];
	}
	else
	{
		newview->aim = player->aiming;
		newview->angle = player->mo->angle;
	}
}

void R_SetupFrame(int s)
{
	player_t *player = &players[displayplayers[s]];
	camera_t *thiscam = &camera[s];
	boolean chasecam = (cv_chasecam[s].value != 0);

	R_SetViewContext(VIEWCONTEXT_PLAYER1 + s);

	if (player->spectator) // no spectator chasecam
		chasecam = false; // force chasecam off

	if (chasecam && (thiscam && !thiscam->chase))
	{
		P_ResetCamera(player, thiscam);
		thiscam->chase = true;
	}
	else if (!chasecam)
		thiscam->chase = false;

	newview->sky = false;

	R_SetupAimingFrame(s);

	if (player->awayviewtics)
	{
		// cut-away view stuff
		r_viewmobj = player->awayviewmobj; // should be a MT_ALTVIEWMAN
		I_Assert(r_viewmobj != NULL);

		newview->x = r_viewmobj->x;
		newview->y = r_viewmobj->y;
		newview->z = r_viewmobj->z + 20*FRACUNIT;

		R_SetupCommonFrame(player, r_viewmobj->subsector);
	}
	else if (!player->spectator && chasecam)
	// use outside cam view
	{
		r_viewmobj = NULL;

		newview->x = thiscam->x;
		newview->y = thiscam->y;
		newview->z = thiscam->z + (thiscam->height>>1);

		R_SetupCommonFrame(player, thiscam->subsector);
	}
	else
	// use the player's eyes view
	{
		r_viewmobj = player->mo;
		I_Assert(r_viewmobj != NULL);

		newview->x = r_viewmobj->x;
		newview->y = r_viewmobj->y;
		newview->z = player->viewz;

		R_SetupCommonFrame(player, r_viewmobj->subsector);
	}
}

void R_SkyboxFrame(int s)
{
	player_t *player = &players[displayplayers[s]];
	camera_t *thiscam = &camera[s];

	R_SetViewContext(VIEWCONTEXT_SKY1 + s);

	// cut-away view stuff
	newview->sky = true;
	r_viewmobj = player->skybox.viewpoint;
#ifdef PARANOIA
	if (!r_viewmobj)
	{
		const size_t playeri = (size_t)(player - players);
		I_Error("R_SkyboxFrame: r_viewmobj null (player %s)", sizeu1(playeri));
	}
#endif

	R_SetupAimingFrame(s);

	newview->angle += r_viewmobj->angle;

	newview->x = r_viewmobj->x;
	newview->y = r_viewmobj->y;
	newview->z = r_viewmobj->z; // 26/04/17: use actual Z position instead of spawnpoint angle!

	if (mapheaderinfo[gamemap-1])
	{
		mapheader_t *mh = mapheaderinfo[gamemap-1];
		vector3_t campos = {0,0,0}; // Position of player's actual view point
		mobj_t *center = player->skybox.centerpoint;

		if (player->awayviewtics) {
			campos.x = player->awayviewmobj->x;
			campos.y = player->awayviewmobj->y;
			campos.z = player->awayviewmobj->z + 20*FRACUNIT;
		} else if (thiscam->chase) {
			campos.x = thiscam->x;
			campos.y = thiscam->y;
			campos.z = thiscam->z + (thiscam->height>>1);
		} else {
			campos.x = player->mo->x;
			campos.y = player->mo->y;
			campos.z = player->viewz;
		}

		// Earthquake effects should be scaled in the skybox
		// (if an axis isn't used, the skybox won't shake in that direction)
		campos.x += quake.x;
		campos.y += quake.y;
		campos.z += quake.z;

		if (center) // Is there a viewpoint?
		{
			fixed_t x = 0, y = 0;
			if (mh->skybox_scalex > 0)
				x = (campos.x - center->x) / mh->skybox_scalex;
			else if (mh->skybox_scalex < 0)
				x = (campos.x - center->x) * -mh->skybox_scalex;

			if (mh->skybox_scaley > 0)
				y = (campos.y - center->y) / mh->skybox_scaley;
			else if (mh->skybox_scaley < 0)
				y = (campos.y - center->y) * -mh->skybox_scaley;

			if (r_viewmobj->angle == 0)
			{
				newview->x += x;
				newview->y += y;
			}
			else if (r_viewmobj->angle == ANGLE_90)
			{
				newview->x -= y;
				newview->y += x;
			}
			else if (r_viewmobj->angle == ANGLE_180)
			{
				newview->x -= x;
				newview->y -= y;
			}
			else if (r_viewmobj->angle == ANGLE_270)
			{
				newview->x += y;
				newview->y -= x;
			}
			else
			{
				angle_t ang = r_viewmobj->angle>>ANGLETOFINESHIFT;
				newview->x += FixedMul(x,FINECOSINE(ang)) - FixedMul(y,  FINESINE(ang));
				newview->y += FixedMul(x,  FINESINE(ang)) + FixedMul(y,FINECOSINE(ang));
			}
		}
		if (mh->skybox_scalez > 0)
			newview->z += campos.z / mh->skybox_scalez;
		else if (mh->skybox_scalez < 0)
			newview->z += campos.z * -mh->skybox_scalez;
	}


	R_SetupCommonFrame(player, r_viewmobj->subsector);
}

boolean R_ViewpointHasChasecam(player_t *player)
{
	boolean chasecam = false;
	UINT8 i;

	for (i = 0; i <= splitscreen; i++)
	{
		if (player == &players[g_localplayers[i]])
		{
			chasecam = (cv_chasecam[i].value != 0);
			break;
		}
	}

	if (player->playerstate == PST_DEAD || gamestate == GS_TITLESCREEN || tutorialmode)
		chasecam = true; // force chasecam on
	else if (player->spectator) // no spectator chasecam
		chasecam = false; // force chasecam off

	return chasecam;
}

boolean R_IsViewpointThirdPerson(player_t *player, boolean skybox)
{
	boolean chasecam = R_ViewpointHasChasecam(player);

	// cut-away view stuff
	if (player->awayviewtics || skybox)
		return chasecam;
	// use outside cam view
	else if (!player->spectator && chasecam)
		return true;

	// use the player's eyes view
	return false;
}

static void R_PortalFrame(portal_t *portal)
{
	viewx = portal->viewx;
	viewy = portal->viewy;
	viewz = portal->viewz;

	viewangle = portal->viewangle;
	viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	portalclipstart = portal->start;
	portalclipend = portal->end;

	if (portal->clipline != -1)
	{
		portalclipline = &lines[portal->clipline];
		portalcullsector = portalclipline->frontsector;
		viewsector = portalclipline->frontsector;
	}
	else
	{
		portalclipline = NULL;
		portalcullsector = NULL;
		viewsector = R_PointInSubsector(viewx, viewy)->sector;
	}
}

static void Mask_Pre (maskcount_t* m)
{
	m->drawsegs[0] = ds_p - drawsegs;
	m->vissprites[0] = visspritecount;
	m->viewx = viewx;
	m->viewy = viewy;
	m->viewz = viewz;
	m->viewsector = viewsector;
}

static void Mask_Post (maskcount_t* m)
{
	m->drawsegs[1] = ds_p - drawsegs;
	m->vissprites[1] = visspritecount;
}

// ================
// R_RenderView
// ================

//                     FAB NOTE FOR WIN32 PORT !! I'm not finished already,
// but I suspect network may have problems with the video buffer being locked
// for all duration of rendering, and being released only once at the end..
// I mean, there is a win16lock() or something that lasts all the rendering,
// so maybe we should release screen lock before each netupdate below..?

void R_RenderPlayerView(void)
{
	player_t * player = &players[displayplayers[viewssnum]];
	INT32			nummasks	= 1;
	maskcount_t*	masks		= malloc(sizeof(maskcount_t));

	// if this is display player 1
	if (cv_homremoval.value && player == &players[displayplayers[0]])
	{
		if (cv_homremoval.value == 1)
			V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31); // No HOM effect!
		else //'development' HOM removal -- makes it blindingly obvious if HOM is spotted.
			V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 32+(timeinmap&15));
	}
	else if (r_splitscreen == 2 && player == &players[displayplayers[2]])
	{
		// Draw over the fourth screen so you don't have to stare at a HOM :V
		V_DrawFill(viewwidth, viewheight, viewwidth, viewheight, 31|V_NOSCALESTART);
	}

	R_SetupFrame(viewssnum);
	framecount++;
	validcount++;

	// Clear buffers.
	R_ClearPlanes();
	if (viewmorph[viewssnum].use)
	{
		portalclipstart = viewmorph[viewssnum].x1;
		portalclipend = viewwidth-viewmorph[viewssnum].x1-1;
		R_PortalClearClipSegs(portalclipstart, portalclipend);
		memcpy(ceilingclip, viewmorph[viewssnum].ceilingclip, sizeof(INT16)*vid.width);
		memcpy(floorclip, viewmorph[viewssnum].floorclip, sizeof(INT16)*vid.width);
	}
	else
	{
		portalclipstart = 0;
		portalclipend = viewwidth;
		R_ClearClipSegs();
	}
	R_ClearDrawSegs();
	R_ClearSprites();
	Portal_InitList();

	// check for new console commands.
	NetUpdate();

	// The head node is the last node output.

	Mask_Pre(&masks[nummasks - 1]);
	curdrawsegs = ds_p;
//profile stuff ---------------------------------------------------------
#ifdef TIMING
	mytotal = 0;
	ProfZeroTimer();
#endif
	ps_numbspcalls = ps_numpolyobjects = ps_numdrawnodes = 0;
	ps_bsptime = I_GetPreciseTime();
	R_RenderBSPNode((INT32)numnodes - 1);
	R_AddPrecipitationSprites();
	ps_bsptime = I_GetPreciseTime() - ps_bsptime;
#ifdef TIMING
	RDMSR(0x10, &mycount);
	mytotal += mycount; // 64bit add

	CONS_Debug(DBG_RENDER, "RenderBSPNode: 0x%d %d\n", *((INT32 *)&mytotal + 1), (INT32)mytotal);
#endif
//profile stuff ---------------------------------------------------------
	Mask_Post(&masks[nummasks - 1]);

	ps_sw_spritecliptime = I_GetPreciseTime();
	R_ClipSprites(drawsegs, NULL);
	ps_sw_spritecliptime = I_GetPreciseTime() - ps_sw_spritecliptime;


	// Add skybox portals caused by sky visplanes.
	if (cv_skybox.value && player->skybox.viewpoint)
		Portal_AddSkyboxPortals(player);

	// Portal rendering. Hijacks the BSP traversal.
	ps_sw_portaltime = I_GetPreciseTime();
	if (portal_base)
	{
		portal_t *portal;

		for(portal = portal_base; portal; portal = portal_base)
		{
			portalrender = portal->pass; // Recursiveness depth.

			R_ClearFFloorClips();

			// Apply the viewpoint stored for the portal.
			R_PortalFrame(portal);

			// Hack in the clipsegs to delimit the starting
			// clipping for sprites and possibly other similar
			// future items.
			R_PortalClearClipSegs(portal->start, portal->end);

			// Hack in the top/bottom clip values for the window
			// that were previously stored.
			Portal_ClipApply(portal);

			validcount++;

			masks = realloc(masks, (++nummasks)*sizeof(maskcount_t));

			Mask_Pre(&masks[nummasks - 1]);
			curdrawsegs = ds_p;

			// Render the BSP from the new viewpoint, and clip
			// any sprites with the new clipsegs and window.
			R_RenderBSPNode((INT32)numnodes - 1);
			Mask_Post(&masks[nummasks - 1]);

			R_ClipSprites(ds_p - (masks[nummasks - 1].drawsegs[1] - masks[nummasks - 1].drawsegs[0]), portal);

			Portal_Remove(portal);
		}
	}
	ps_sw_portaltime = I_GetPreciseTime() - ps_sw_portaltime;

	ps_sw_planetime = I_GetPreciseTime();
	R_DrawPlanes();
	ps_sw_planetime = I_GetPreciseTime() - ps_sw_planetime;

	// draw mid texture and sprite
	// And now 3D floors/sides!
	ps_sw_maskedtime = I_GetPreciseTime();
	R_DrawMasked(masks, nummasks);
	ps_sw_maskedtime = I_GetPreciseTime() - ps_sw_maskedtime;

	free(masks);
}

// =========================================================================
//                    ENGINE COMMANDS & VARS
// =========================================================================

void R_RegisterEngineStuff(void)
{
	UINT8 i;

	CV_RegisterVar(&cv_gravity);
	CV_RegisterVar(&cv_tailspickup);
	CV_RegisterVar(&cv_allowmlook);
	CV_RegisterVar(&cv_homremoval);
	// Enough for dedicated server
	if (dedicated)
		return;

	CV_RegisterVar(&cv_drawdist);
	CV_RegisterVar(&cv_drawdist_precip);

	CV_RegisterVar(&cv_shadow);
	CV_RegisterVar(&cv_skybox);
	CV_RegisterVar(&cv_ffloorclip);

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		CV_RegisterVar(&cv_fov[i]);
		CV_RegisterVar(&cv_chasecam[i]);
		CV_RegisterVar(&cv_flipcam[i]);
		CV_RegisterVar(&cv_cam_dist[i]);
		CV_RegisterVar(&cv_cam_still[i]);
		CV_RegisterVar(&cv_cam_height[i]);
		CV_RegisterVar(&cv_cam_speed[i]);
		CV_RegisterVar(&cv_cam_rotate[i]);
	}

	CV_RegisterVar(&cv_tilting);
	CV_RegisterVar(&cv_actionmovie);
	CV_RegisterVar(&cv_windowquake);

	CV_RegisterVar(&cv_showhud);
	CV_RegisterVar(&cv_translucenthud);

	CV_RegisterVar(&cv_maxportals);

	CV_RegisterVar(&cv_movebob);

	// Frame interpolation/uncapped
	CV_RegisterVar(&cv_fpscap);
}
