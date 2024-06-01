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
	{IT_HEADER, "Colorized HUD...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Hud", "HUD will match player color.",
		NULL, {.cvar = &cv_colorizedhud}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Itembox", "Itembox becomes colored as well.",
		NULL, {.cvar = &cv_colorizeditembox}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Hud Color", "Use a different color for the colorized hud.",
		NULL, {.cvar = &cv_colorizedhudcolor}, 0, 0},

	{IT_HEADER, "Miscellaneous HUD...", NULL,
		NULL, {NULL}, 0, 0},
		
	{IT_STRING | IT_CVAR, "HUD Translucency", "Determines how transparent the hud is.",
		NULL, {.cvar = &cv_translucenthud}, 0, 0},

	{IT_STRING | IT_CVAR, "Hold Scoreboard Button", "Reverts the scoreboard button behavior to how it was in SRB2Kart.",
		NULL, {.cvar = &cv_holdscorebutt}, 0, 0},

	{IT_STRING | IT_CVAR, "Hi-res HUD Scale", "Determines the scale of the hud at higher resolutions.",
		NULL, {.cvar = &cv_highreshudscale}, 0, 0},
		
	{IT_HEADER, "Sound/Music...", NULL,
		NULL, {NULL}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Power Music", "Toggles the power music during gameplay.",
		NULL, {.cvar = &cv_specialmusic}, 0, 0},
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
