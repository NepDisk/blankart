// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 2013-2016 by Matthew "Inuyasha" Walsh.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  f_wipe.c
/// \brief SRB2 2.1 custom fade mask "wipe" behavior.

#include "f_finale.h"
#include "i_video.h"
#include "v_video.h"

#include "r_data.h" // NearestColor
#include "r_draw.h" // transtable
#include "p_pspr.h" // tr_transxxx

#include "w_wad.h"
#include "z_zone.h"

#include "i_time.h"
#include "i_system.h"
#include "i_threads.h"
#include "m_menu.h"
#include "console.h"
#include "d_main.h"
#include "m_misc.h" // movie mode
#include "d_clisrv.h" // So the network state can be updated during the wipe

#include "g_game.h"
#include "st_stuff.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#if NUMSCREENS < 5
#define NOWIPE // do not enable wipe image post processing for ARM, SH and MIPS CPUs
#endif

typedef struct fademask_s {
	UINT8* mask;
	UINT16 width, height;
	size_t size;
	fixed_t xscale, yscale;
} fademask_t;

UINT8 wipedefs[NUMWIPEDEFS] = {
	99, // wipe_credits_intermediate (0)

	0,  // wipe_level_toblack
	0,  // wipe_intermission_toblack
	0,  // wipe_voting_toblack,
	0,  // wipe_continuing_toblack
	0,  // wipe_titlescreen_toblack
	0,  // wipe_timeattack_toblack
	99, // wipe_credits_toblack
	0,  // wipe_evaluation_toblack
	0,  // wipe_gameend_toblack
	UINT8_MAX, // wipe_intro_toblack (hardcoded)
	99, // wipe_ending_toblack (hardcoded)
	99, // wipe_cutscene_toblack (hardcoded)

	72, // wipe_encore_toinvert
	99, // wipe_encore_towhite

	UINT8_MAX, // wipe_level_final
	0,  // wipe_intermission_final
	0,  // wipe_voting_final
	0,  // wipe_continuing_final
	0,  // wipe_titlescreen_final
	0,  // wipe_timeattack_final
	99, // wipe_credits_final
	0,  // wipe_evaluation_final
	0,  // wipe_gameend_final
	99, // wipe_intro_final (hardcoded)
	99, // wipe_ending_final (hardcoded)
	99  // wipe_cutscene_final (hardcoded)
};

//--------------------------------------------------------------------------
//                        SCREEN WIPE PACKAGE
//--------------------------------------------------------------------------

boolean WipeInAction = false;
boolean WipeStageTitle = false;
INT32 lastwipetic = 0;

#ifndef NOWIPE

#define GENLEN 31

static UINT8 *wipe_scr_start; //screen 3
static UINT8 *wipe_scr_end; //screen 4
static UINT8 *wipe_scr; //screen 0 (main drawing)
static UINT8 pallen;
static fixed_t paldiv;

/** Create fademask_t from lump
  *
  * \param	lump	Lump name to get data from
  * \return	fademask_t for lump
  */
static fademask_t *F_GetFadeMask(UINT8 masknum, UINT8 scrnnum) {
	static char lumpname[9] = "FADEmmss";
	static fademask_t fm = {NULL,0,0,0,0,0};
	lumpnum_t lumpnum;
	UINT8 *lump, *mask;
	size_t lsize;
	RGBA_t *pcolor;

	if (masknum > 99 || scrnnum > 99)
		goto freemask;

	// SRB2Kart: This suddenly triggers ERRORMODE now
	//sprintf(&lumpname[4], "%.2hu%.2hu", (UINT16)masknum, (UINT16)scrnnum);

	lumpname[4] = '0'+(masknum/10);
	lumpname[5] = '0'+(masknum%10);

	lumpname[6] = '0'+(scrnnum/10);
	lumpname[7] = '0'+(scrnnum%10);

	lumpnum = W_CheckNumForName(lumpname);
	if (lumpnum == LUMPERROR)
		goto freemask;

	lump = W_CacheLumpNum(lumpnum, PU_CACHE);
	lsize = W_LumpLength(lumpnum);
	switch (lsize)
	{
		case 256000: // 640x400
			fm.width = 640;
			fm.height = 400;
			break;
		case 64000: // 320x200
			fm.width = 320;
			fm.height = 200;
			break;
		case 16000: // 160x100
			fm.width = 160;
			fm.height = 100;
			break;
		case 4000: // 80x50 (minimum)
			fm.width = 80;
			fm.height = 50;
			break;

		default: // bad lump
			CONS_Alert(CONS_WARNING, "Fade mask lump %s of incorrect size, ignored\n", lumpname);
		case 0: // end marker (not bad!, but still need clearing)
			goto freemask;
	}
	if (lsize != fm.size)
		fm.mask = Z_Realloc(fm.mask, lsize, PU_STATIC, NULL);
	fm.size = lsize;

	mask = fm.mask;

	while (lsize--)
	{
		// Determine pixel to use from fademask
		pcolor = &pMasterPalette[*lump++];
		*mask++ = FixedDiv((pcolor->s.red+1)<<FRACBITS, paldiv)>>FRACBITS;
	}

	fm.xscale = FixedDiv(vid.width<<FRACBITS, fm.width<<FRACBITS);
	fm.yscale = FixedDiv(vid.height<<FRACBITS, fm.height<<FRACBITS);
	return &fm;

	// Landing point for freeing data -- do this instead of just returning NULL
	// this ensures the fade data isn't remaining in memory, unused
	// (could be up to 256,000 bytes if it's a HQ fade!)
	freemask:
	if (fm.mask)
	{
		Z_Free(fm.mask);
		fm.mask = NULL;
		fm.size = 0;
	}

	return NULL;
}

/**	Wipe ticker
  *
  * \param	fademask	pixels to change
  */
static void F_DoWipe(fademask_t *fademask, lighttable_t *fadecolormap, boolean reverse)
{
	// Software mask wipe -- optimized; though it might not look like it!
	// Okay, to save you wondering *how* this is more optimized than the simpler
	// version that came before it...
	// ---
	// The previous code did two FixedMul calls for every single pixel on the
	// screen, of which there are hundreds of thousands -- if not millions -- of.
	// This worked fine for smaller screen sizes, but with excessively large
	// (1920x1200) screens that meant 4 million+ calls out to FixedMul, and that
	// would take /just/ long enough that fades would start to noticably lag.
	// ---
	// This code iterates over the fade mask's pixels instead of the screen's,
	// and deals with drawing over each rectangular area before it moves on to
	// the next pixel in the fade mask.  As a result, it's more complex (and might
	// look a little messy; sorry!) but it simultaneously runs at twice the speed.
	// In addition, we precalculate all the X and Y positions that we need to draw
	// from and to, so it uses a little extra memory, but again, helps it run faster.
	// ---
	// Sal: I kinda destroyed some of this code by introducing Genesis-style fades.
	// A colormap can be provided in F_RunWipe, which the white/black values will be
	// remapped to the appropriate entry in the fade colormap.
	{
		// wipe screen, start, end
		UINT8       *w = wipe_scr;
		const UINT8 *s = wipe_scr_start;
		const UINT8 *e = wipe_scr_end;

		// first pixel for each screen
		UINT8       *w_base = w;
		const UINT8 *s_base = s;
		const UINT8 *e_base = e;

		// mask data, end
		UINT8       *transtbl;
		const UINT8 *mask    = fademask->mask;
		const UINT8 *maskend = mask + fademask->size;

		// rectangle draw hints
		UINT32 draw_linestart, draw_rowstart;
		UINT32 draw_lineend,   draw_rowend;
		UINT32 draw_linestogo, draw_rowstogo;

		// rectangle coordinates, etc.
		UINT16* scrxpos = (UINT16*)malloc((fademask->width + 1)  * sizeof(UINT16));
		UINT16* scrypos = (UINT16*)malloc((fademask->height + 1) * sizeof(UINT16));
		UINT16 maskx, masky;
		UINT32 relativepos;

		// ---
		// Screw it, we do the fixed point math ourselves up front.
		scrxpos[0] = 0;
		for (relativepos = 0, maskx = 1; maskx < fademask->width; ++maskx)
			scrxpos[maskx] = (relativepos += fademask->xscale)>>FRACBITS;
		scrxpos[fademask->width] = vid.width;

		scrypos[0] = 0;
		for (relativepos = 0, masky = 1; masky < fademask->height; ++masky)
			scrypos[masky] = (relativepos += fademask->yscale)>>FRACBITS;
		scrypos[fademask->height] = vid.height;
		// ---

		maskx = masky = 0;
		do
		{
			UINT8 m = *mask;

			draw_rowstart = scrxpos[maskx];
			draw_rowend   = scrxpos[maskx + 1];
			draw_linestart = scrypos[masky];
			draw_lineend   = scrypos[masky + 1];

			relativepos = (draw_linestart * vid.width) + draw_rowstart;
			draw_linestogo = draw_lineend - draw_linestart;

			if (reverse)
				m = ((pallen-1) - m);

			if (m == 0)
			{
				// shortcut - memcpy source to work
				while (draw_linestogo--)
				{
					M_Memcpy(w_base+relativepos, (reverse ? e_base : s_base)+relativepos, draw_rowend-draw_rowstart);
					relativepos += vid.width;
				}
			}
			else if (m >= (pallen-1))
			{
				// shortcut - memcpy target to work
				while (draw_linestogo--)
				{
					M_Memcpy(w_base+relativepos, (reverse ? s_base : e_base)+relativepos, draw_rowend-draw_rowstart);
					relativepos += vid.width;
				}
			}
			else
			{
				// pointer to transtable that this mask would use
				transtbl = transtables + ((9 - m)<<FF_TRANSSHIFT);

				// DRAWING LOOP
				while (draw_linestogo--)
				{
					w = w_base + relativepos;
					s = s_base + relativepos;
					e = e_base + relativepos;
					draw_rowstogo = draw_rowend - draw_rowstart;

					if (fadecolormap)
					{
						if (reverse)
							s = e;
						while (draw_rowstogo--)
							*w++ = fadecolormap[ ( m << 8 ) + *s++ ];
					}
					else while (draw_rowstogo--)
					{
						/*if (fadecolormap != NULL)
						{
							if (reverse)
								*w++ = fadecolormap[ ( m << 8 ) + *e++ ];
							else
								*w++ = fadecolormap[ ( m << 8 ) + *s++ ];
						}
						else*/
							*w++ = transtbl[ ( *e++ << 8 ) + *s++ ];
					}

					relativepos += vid.width;
				}
				// END DRAWING LOOP
			}

			if (++maskx >= fademask->width)
				++masky, maskx = 0;
		} while (++mask < maskend);

		free(scrxpos);
		free(scrypos);
	}
}
#endif

/** Save the "before" screen of a wipe.
  */
void F_WipeStartScreen(void)
{
#ifndef NOWIPE
#ifdef HWRENDER
	if(rendermode != render_soft)
	{
		HWR_StartScreenWipe();
		return;
	}
#endif
	wipe_scr_start = screens[3];
	I_ReadScreen(wipe_scr_start);
#endif
}

/** Save the "after" screen of a wipe.
  */
void F_WipeEndScreen(void)
{
#ifndef NOWIPE
#ifdef HWRENDER
	if(rendermode != render_soft)
	{
		HWR_EndScreenWipe();
		return;
	}
#endif
	wipe_scr_end = screens[4];
	I_ReadScreen(wipe_scr_end);
	V_DrawBlock(0, 0, 0, vid.width, vid.height, wipe_scr_start);
#endif
}

/**	Wiggle post processor for encore wipes
  */
static void F_DoEncoreWiggle(UINT8 time)
{
	UINT8 *tmpscr = wipe_scr_start;
	UINT8 *srcscr = wipe_scr;
	angle_t disStart = (time * 128) & FINEMASK;
	INT32 y, sine, newpix, scanline;

	for (y = 0; y < vid.height; y++)
	{
		sine = (FINESINE(disStart) * (time*12))>>FRACBITS;
		scanline = y / vid.dupy;
		if (scanline & 1)
			sine = -sine;
		newpix = abs(sine);

		if (sine < 0)
		{
			M_Memcpy(&tmpscr[(y*vid.width)+newpix], &srcscr[(y*vid.width)], vid.width-newpix);

			// Cleanup edge
			while (newpix)
			{
				tmpscr[(y*vid.width)+newpix] = srcscr[(y*vid.width)];
				newpix--;
			}
		}
		else
		{
			M_Memcpy(&tmpscr[(y*vid.width)], &srcscr[(y*vid.width) + sine], vid.width-newpix);

			// Cleanup edge
			while (newpix)
			{
				tmpscr[(y*vid.width) + vid.width - newpix] = srcscr[(y*vid.width) + (vid.width-1)];
				newpix--;
			}
		}

		disStart += (time*8); //the offset into the displacement map, increment each game loop
		disStart &= FINEMASK; //clip it to FINEMASK
	}
}

/** Draw the stage title.
  */
void F_WipeStageTitle(void)
{
	// draw level title
	if ((WipeStageTitle) && G_IsTitleCardAvailable())
	{
		//ST_runTitleCard();
		//ST_drawTitleCard();
	}
}

/** After setting up the screens you want to wipe,
  * calling this will do a 'typical' wipe.
  */
void F_RunWipe(UINT8 wipetype, boolean drawMenu, const char *colormap, boolean reverse, boolean encorewiggle)
{
#ifdef NOWIPE
	(void)wipetype;
	(void)drawMenu;
	(void)colormap;
	(void)reverse;
	(void)encorewiggle;
#else
	tic_t nowtime;
	UINT8 wipeframe = 0;
	fademask_t *fmask;

	lumpnum_t clump = LUMPERROR;
	lighttable_t *fcolor = NULL;

	if (colormap != NULL)
		clump = W_GetNumForName(colormap);

	if (clump != LUMPERROR && wipetype != UINT8_MAX)
	{
		pallen = 32;
		fcolor = Z_MallocAlign((256 * pallen), PU_STATIC, NULL, 8);
		W_ReadLump(clump, fcolor);
	}
	else
	{
		pallen = 11;
		reverse = false;
	}

	paldiv = FixedDiv(257<<FRACBITS, pallen<<FRACBITS);

	// Init the wipe
	WipeInAction = true;
	wipe_scr = screens[0];

	// lastwipetic should either be 0 or the tic we last wiped
	// on for fade-to-black
	for (;;)
	{
		// get fademask first so we can tell if it exists or not
		fmask = F_GetFadeMask(wipetype, wipeframe++);
		if (!fmask)
			break;

		// wait loop
		while (!((nowtime = I_GetTime()) - lastwipetic))
		{
			I_Sleep(cv_sleep.value);
			I_UpdateTime(cv_timescale.value);
		}
		lastwipetic = nowtime;

#ifdef HWRENDER
		if (rendermode == render_opengl)
			HWR_DoWipe(wipetype, wipeframe-1); // send in the wipe type and wipeframe because we need to cache the graphic
		else
#endif

		if (rendermode != render_none) //this allows F_RunWipe to be called in dedicated servers
		{
			F_DoWipe(fmask, fcolor, reverse);

			if (encorewiggle)
			{
#ifdef HWRENDER
				if (rendermode != render_opengl)
#endif
					F_DoEncoreWiggle(wipeframe);
			}
		}

		I_OsPolling();
		I_UpdateNoBlit();

		if (drawMenu && rendermode != render_none)
		{
#ifdef HAVE_THREADS
			I_lock_mutex(&m_menu_mutex);
#endif
			M_Drawer(); // menu is drawn even on top of wipes
#ifdef HAVE_THREADS
			I_unlock_mutex(m_menu_mutex);
#endif
		}

		I_FinishUpdate(); // page flip or blit buffer

		if (moviemode)
			M_SaveFrame();

		NetKeepAlive(); // Update the network so we don't cause timeouts
	}

	WipeInAction = false;

	if (fcolor)
	{
		Z_Free(fcolor);
		fcolor = NULL;
	}
#endif
}

/** Returns tic length of wipe
  * One lump equals one tic
  */
tic_t F_GetWipeLength(UINT8 wipetype)
{
#ifdef NOWIPE
	(void)wipetype;
	return 0;
#else
	static char lumpname[10] = "FADEmmss";
	lumpnum_t lumpnum;
	UINT8 wipeframe;

	if (wipetype > 99)
		return 0;

	for (wipeframe = 0; wipeframe < 100; wipeframe++)
	{
		sprintf(&lumpname[4], "%.2hu%.2hu", (UINT16)wipetype, (UINT16)wipeframe);

		lumpnum = W_CheckNumForName(lumpname);
		if (lumpnum == LUMPERROR)
			return --wipeframe;
	}
	return --wipeframe;
#endif
}

/** Does the specified wipe exist?
  */
boolean F_WipeExists(UINT8 wipetype)
{
#ifdef NOWIPE
	(void)wipetype;
	return false;
#else
	static char lumpname[10] = "FADEmm00";
	lumpnum_t lumpnum;

	if (wipetype > 99)
		return false;

	sprintf(&lumpname[4], "%.2hu00", (UINT16)wipetype);

	lumpnum = W_CheckNumForName(lumpname);
	return !(lumpnum == LUMPERROR);
#endif
}
