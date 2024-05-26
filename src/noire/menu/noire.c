// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

// Noire
#include "../n_menu.h"
#include "../n_cvar.h"
#include "../../d_main.h"
#include "../../v_video.h"

menuitem_t OPTIONS_Noire[] =
{
	{IT_HEADER, "Colorized Hud..", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Hud", "HUD will be colorized.",
		NULL, {.cvar = &cv_colorizedhud}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Itembox", "Itembox coloring.",
		NULL, {.cvar = &cv_colorizeditembox}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Hud Color", "Color used for hud",
		NULL, {.cvar = &cv_colorizedhudcolor}, 0, 0},

	{IT_HEADER, "Other Shit", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Hud Translucency", "The thing that was removed for no reason lol",
		NULL, {.cvar = &cv_translucenthud}, 0, 0},

	{IT_STRING | IT_CVAR, "Hold Scoreboard Button", "Butts",
		NULL, {.cvar = &cv_holdscorebutt}, 0, 0},

	{IT_STRING | IT_CVAR, "Power Music", "Toggle the Power Music",
		NULL, {.cvar = &cv_specialmusic}, 0, 0},

	{IT_STRING | IT_CVAR, "Highreshudscale", "When the hud is high how much crap scales.",
		NULL, {.cvar = &cv_highreshudscale}, 0, 0},
};

void ColorHUD_OnChange(void)
{
	if (con_startup) return;

	if(cv_colorizedhud.value && clr_hud)
	{
		for (int i = 2; i < 4; i++)
		{
			OPTIONS_Noire[i].status = IT_STRING | IT_CVAR;
		}
	}
	else
	{
		for (int i = 2; i < 4; i++)
		{
			OPTIONS_Noire[i].status = IT_GRAYEDOUT;
		}
	}
}

menu_t OPTIONS_NoireDef = {
	sizeof (OPTIONS_Noire) / sizeof (menuitem_t),
	&OPTIONS_MainDef,
	0,
	OPTIONS_Noire,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	ColorHUD_OnChange,
	NULL,
	NULL,
};
