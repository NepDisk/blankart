// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_menu.h"

///////////
// MAIN MENU
///////////
menuitem_t OPTIONS_NoireGameplay[] =
{
	{IT_STRING | IT_SUBMENU, "Ring Options...", "Adjust how rings work, or disable them entirely.",
		NULL, {.submenu = &OPTIONS_NoireGameplayRingsDef}, 0, 0},

	{IT_STRING | IT_SUBMENU, "Item Options...", "Adjust the behavior of certain items.",
		NULL, {.submenu = &OPTIONS_NoireGameplayItemsDef}, 0, 0},

	{IT_NOTHING|IT_SPACE, NULL, NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_SUBMENU, "Mechanics Options...", "Adjust the overall racing experience.",
		NULL, {.submenu = &OPTIONS_NoireGameplayMechanicsDef}, 0, 0},

	{IT_NOTHING|IT_SPACE, NULL, NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_SUBMENU, "Bot Options...", "Adjust the aggressiveness of CPUs.",
		NULL, {.submenu = &OPTIONS_NoireGameplayBotsDef}, 0, 0},
};

///////////
// RINGS
///////////
menuitem_t OPTIONS_NoireGameplayRings[] =
{
	{IT_STRING | IT_CVAR, "Rings", "Enable or disable all ring functionality.",
		NULL, {.cvar = &cv_ng_rings}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Cap", "Maximum amount of rings that can be held. (Minimum is maximum negated)",
		NULL, {.cvar = &cv_ng_ringcap}, 0, 0},

	{IT_STRING | IT_CVAR, "Spill Cap", "Maximum amount of rings that can be lost upon taking damage.",
		NULL, {.cvar = &cv_ng_spillcap}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Debt", "Let ring count go into negatives.",
		NULL, {.cvar = &cv_ng_ringdebt}, 0, 0},

	{IT_STRING | IT_CVAR, "Ringsting", "Should getting bumped while having no rings hurt?",
		NULL, {.cvar = &cv_ng_ringsting}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Deathmark", "Racer dies upon taking damage if ring count is equal to or below this value.",
		NULL, {.cvar = &cv_ng_ringdeathmark}, 0, 0},

	{IT_STRING | IT_CVAR, "Spawn Rings", "Will rings appear in tracks.",
		NULL, {.cvar = &cv_ng_maprings}, 0, 0},

	{IT_STRING | IT_CVAR, "Spawn Ring Boxes", "Enable or disable Ring Boxes appearance in tracks.",
		NULL, {.cvar = &cv_ng_mapringboxes}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Box Transformation", "After picking up Item Boxes, can they turn into Ring Boxes?",
		NULL, {.cvar = &cv_ng_ringboxtransform}, 0, 0},
};

///////////
// ITEMS
///////////
menuitem_t OPTIONS_NoireGameplayItems[] =
{
	{IT_STRING | IT_CVAR, "Capsules", "Enable or disable Item Capsules appearance in tracks.",
		NULL, {.cvar = &cv_ng_capsules}, 0, 0},

	{IT_STRING | IT_CVAR, "Orbinaut Behavior", "Toggle between Ring Racers or Kart's Orbinaut behavior.",
		NULL, {.cvar = &cv_ng_oldorbinaut}, 0, 0},

	{IT_STRING | IT_CVAR, "Jawz Behavior", "Toggle between Ring Racers or Kart's Jawz behavior.",
		NULL, {.cvar = &cv_ng_oldjawz}, 0, 0},
};

///////////
// MECHANICS
///////////
menuitem_t OPTIONS_NoireGameplayMechanics[] =
{

	{IT_STRING | IT_SUBMENU, "Life Options...", "Adjust the behavior of the lives system.",
		NULL, {.submenu = &OPTIONS_NoireGameplayLivesDef}, 0, 0},

	{IT_NOTHING|IT_SPACE, NULL, NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Draft/Tether", "Enable or disable the 'tether' between racers, pulling racers behind.",
		NULL, {.cvar = &cv_ng_draft}, 0, 0},

	{IT_STRING | IT_CVAR, "Map Anger", "Amount of times a map has to be ignored by everyone to vote itself.",
		NULL, {.cvar = &cv_ng_mapanger}, 0, 0},

	{IT_STRING | IT_CVAR, "Tripwires", "Enable or disable terrain recognized as tripwire.",
		NULL, {.cvar = &cv_ng_tripwires}, 0, 0},

	{IT_STRING | IT_CVAR, "Force Disable Position", "Force disable Position on all maps regardless of SOC",
		NULL, {.cvar = &cv_ng_forcenoposition}, 0, 0},

	{IT_STRING | IT_CVAR, "Special Stages", "Enable or disable special stages in Intense or higher GPs.",
		NULL, {.cvar = &cv_ng_dospecialstage}, 0, 0},
};

///////////
// BOTS
///////////
menuitem_t OPTIONS_NoireGameplayBots[] =
{
	{IT_STRING | IT_SUBMENU, "Rival Options...", "Adjust the rival's cheats.",
		NULL, {.submenu = &OPTIONS_NoireGameplayRivalsDef}, 0, 0},

	{IT_NOTHING|IT_SPACE, NULL, NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Bot Rubberband Boost", "Should bots be allowed to use rubberband boost?",
		NULL, {.cvar = &cv_ng_botrubberbandboost}, 0, 0},

	{IT_STRING | IT_CVAR, "Use character rivals", "Should the preset 'rivals' of each character be spawned?",
		NULL, {.cvar = &cv_ng_charsetrivals}, 0, 0},
};

///////////
// RIVALS
///////////
menuitem_t OPTIONS_NoireGameplayRivals[] =
{
	{IT_STRING | IT_CVAR, "Rivals", "Enable or disable rivals.",
		NULL, {.cvar = &cv_ng_rivals}, 0, 0},

	{IT_STRING | IT_CVAR, "Top Speed", "The rival's top speed. (10 is equivalent to non-rivals)",
		NULL, {.cvar = &cv_ng_rivaltopspeed}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Power", "The rival's ring power. (10 is equivalent to non-rivals)",
		NULL, {.cvar = &cv_ng_rivalringpower}, 0, 0},

	{IT_STRING | IT_CVAR, "Frantic Items", "Enable or disable the rival's frantic items.",
		NULL, {.cvar = &cv_ng_rivalfrantic}, 0, 0},

	{IT_STRING | IT_CVAR, "2x Draft Power", "Should the rival pull ahead at double speed?",
		NULL, {.cvar = &cv_ng_rivaldraft}, 0, 0},
};

///////////
// LIVES
///////////
menuitem_t OPTIONS_NoireGameplayLives[] =
{
	{IT_STRING | IT_CVAR, "Lives", "Enable or disable the lives system in supported gamemodes.",
		NULL, {.cvar = &cv_ng_lives}, 0, 0},

	{IT_STRING | IT_CVAR, "Continues Affecting Rank", "Should lives/retries effect the end rank of a GP?",
		NULL, {.cvar = &cv_ng_continuesrank}, 0, 0},
};

void NG_Generic_OnChange(void)
{
	static boolean messageHasBeenDelivered = false;
	if (gamestate == GS_LEVEL && !messageHasBeenDelivered && menuactive)
	{
		M_StartMessage(
			"WARNING!",
			M_GetText("The map must be reset for your changes to apply."),
			NULL,
			MM_NOTHING,
			NULL,
			NULL
		);
		messageHasBeenDelivered = true; //Through patches of violet
	}
}

void NG_Rings_OnChange(void)
{
	if (con_startup) return;

	if(cv_ng_rings.value)
	{
		CV_SetValue(&cv_ng_ringcap, 20);
		CV_SetValue(&cv_ng_spillcap, 20);
		CV_Set(&cv_ng_ringdebt, "On");
		CV_Set(&cv_ng_ringsting, "On");
		CV_Set(&cv_ng_ringdeathmark, "-20");
		CV_Set(&cv_ng_maprings, "On");
		CV_Set(&cv_ng_mapringboxes, "On");
		CV_Set(&cv_ng_ringboxtransform, "On");

		for (int i = 1; i < OPTIONS_NoireGameplayRingsDef.numitems; i++)
		{
			OPTIONS_NoireGameplayRings[i].status = IT_STRING | IT_CVAR;
		}
	}
	else
	{
		CV_SetValue(&cv_ng_ringcap, 0);
		CV_SetValue(&cv_ng_spillcap, 0);
		CV_Set(&cv_ng_ringdebt, "Off");
		CV_Set(&cv_ng_ringsting, "Off");
		CV_Set(&cv_ng_ringdeathmark, "-20");
		CV_Set(&cv_ng_maprings, "Off");
		CV_Set(&cv_ng_mapringboxes, "Off");
		CV_Set(&cv_ng_ringboxtransform, "Off");

		for (int i = 1; i < OPTIONS_NoireGameplayRingsDef.numitems; i++)
		{
			OPTIONS_NoireGameplayRings[i].status = IT_GRAYEDOUT;
		}
	}
}

void NG_Lives_OnChange(void)
{
	if (con_startup) return;

	for (int i = 1; i < OPTIONS_NoireGameplayLivesDef.numitems; i++)
	{
		OPTIONS_NoireGameplayLives[i].status = cv_ng_lives.value
			? (IT_STRING | IT_CVAR)
			: IT_GRAYEDOUT;
	}
}

void NG_OldPogoOverride_OnChange(void)
{
	static boolean displayedWarning = false;
	if (!displayedWarning && menuactive)
	{
		M_StartMessage(
			"WARNING!",
			M_GetText("This toggle is just for fun. Some maps may break and diagonal/sloped spring panels won\'t be affected.\n\nEnable at your own risk!"),
			NULL,
			MM_NOTHING,
			NULL,
			NULL
		);
		displayedWarning = true;
	}
}

void NG_ForceNoPosition_OnChange(void)
{
	static boolean displayedWarning = false;
	if (!displayedWarning && menuactive)
	{
		M_StartMessage(
			"WARNING!",
			M_GetText("It is recommended to use SOC value\n\"LegacyStart = True\" instead of this convar.\nSome maps may break.\n\nEnable at your own risk!"),
			NULL,
			MM_NOTHING,
			NULL,
			NULL
		);
		displayedWarning = true;
	}
}

void NG_Rivals_OnChange(void)
{
	if (con_startup) return;

	for (int i = 1; i < OPTIONS_NoireGameplayRivalsDef.numitems; i++)
	{
		OPTIONS_NoireGameplayRivals[i].status = cv_ng_rivals.value
			? (IT_STRING | IT_CVAR)
			: IT_GRAYEDOUT;
	}
}

menu_t OPTIONS_NoireGameplayDef = {
	sizeof (OPTIONS_NoireGameplay) / sizeof (menuitem_t),
	&OPTIONS_GameplayDef,
	0,
	OPTIONS_NoireGameplay,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};

menu_t OPTIONS_NoireGameplayRingsDef = {
	sizeof (OPTIONS_NoireGameplayRings) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayDef,
	0,
	OPTIONS_NoireGameplayRings,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};

menu_t OPTIONS_NoireGameplayItemsDef = {
	sizeof (OPTIONS_NoireGameplayItems) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayDef,
	0,
	OPTIONS_NoireGameplayItems,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};

menu_t OPTIONS_NoireGameplayMechanicsDef = {
	sizeof (OPTIONS_NoireGameplayMechanics) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayDef,
	0,
	OPTIONS_NoireGameplayMechanics,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};

menu_t OPTIONS_NoireGameplayLivesDef = {
	sizeof (OPTIONS_NoireGameplayLives) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayMechanicsDef,
	0,
	OPTIONS_NoireGameplayLives,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};

menu_t OPTIONS_NoireGameplayBotsDef = {
	sizeof (OPTIONS_NoireGameplayBots) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayDef,
	0,
	OPTIONS_NoireGameplayBots,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};

menu_t OPTIONS_NoireGameplayRivalsDef = {
	sizeof (OPTIONS_NoireGameplayRivals) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayBotsDef,
	0,
	OPTIONS_NoireGameplayRivals,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};
