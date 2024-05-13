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

	{IT_HEADER, "Misc..", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Special Music", "Enable / Disable special music",
		NULL, {.cvar = &cv_specialmusic}, 0, 0},

	{IT_STRING | IT_CVAR, "Score Menu Hold", "Require holding to keep score menu open",
		NULL, {.cvar = &cv_holdscorebutt}, 0, 0},



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
