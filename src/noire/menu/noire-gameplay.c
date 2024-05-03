// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../../k_menu.h"
#include "../../m_cond.h"
#include "../../command.h"
#include "../../console.h"
#include "../../g_state.h" //For the tripwire toggle

// Noire
#include "../n_cvar.h"

menuitem_t OPTIONS_NoireGameplay[] =
{
	///////////
	// RINGS
	///////////
	{IT_HEADER, "Rings...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Rings", "Use these for a burst of speed.",
		NULL, {.cvar = &cv_ng_rings}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Cap", "Adjust maximum ring count (minimum is maximum negated)",
		NULL, {.cvar = &cv_ng_ringcap}, 0, 0},

	{IT_STRING | IT_CVAR, "Spill Cap", "Adjust maximum ring loss upon taking damage.",
		NULL, {.cvar = &cv_ng_spillcap}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Debt", "Should Rings go under 0?",
		NULL, {.cvar = &cv_ng_ringdebt}, 0, 0},

	{IT_STRING | IT_CVAR, "Ringsting", "Should having no Rings hurt?",
		NULL, {.cvar = &cv_ng_ringsting}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Deathmark", "Mark player for death when rings are equal to or below this value.",
		NULL, {.cvar = &cv_ng_ringsmarkedfordeath}, 0, 0},

	{IT_STRING | IT_CVAR, "Map Rings", "Should maps have Rings?",
		NULL, {.cvar = &cv_ng_maprings}, 0, 0},

	{IT_STRING | IT_CVAR, "Map Ringboxes", "Should maps have Ringboxes?",
		NULL, {.cvar = &cv_ng_mapringboxes}, 0, 0},

	{IT_STRING | IT_CVAR, "Ringbox Transformation", "Should Itemboxes become Ringboxes?",
		NULL, {.cvar = &cv_ng_ringboxtransform}, 0, 0},

	///////////
	// COLLECTABLES
	///////////
	{IT_HEADER, "Collectables...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Capsules", "Should Capsules spawn in-game?",
		NULL, {.cvar = &cv_ng_capsules}, 0, 0},

	///////////
	// MECHANICS
	///////////
	{IT_HEADER, "Mechanics...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Fast Fall Bounce", "Should Fast Fall bounce?",
		NULL, {.cvar = &cv_ng_fastfallbounce}, 0, 0},

	{IT_STRING | IT_CVAR, "Draft/Tether", "Should players ahead pull?",
		NULL, {.cvar = &cv_ng_draft}, 0, 0},

	{IT_STRING | IT_CVAR, "Tumble", "Should you tumble?",
		NULL, {.cvar = &cv_ng_tumble}, 0, 0},

	{IT_STRING | IT_CVAR, "Stumble", "Should you stumble?",
		NULL, {.cvar = &cv_ng_stumble}, 0, 0},

	{IT_STRING | IT_CVAR, "Hitlag", "Should there be hitlag?",
		NULL, {.cvar = &cv_ng_hitlag}, 0, 0},

	{IT_STRING | IT_CVAR, "Map Anger", "Amount of times a map has to be ignored by everyone to vote itself.",
		NULL, {.cvar = &cv_ng_mapanger}, 0, 0},

	{IT_STRING | IT_CVAR, "Tripwires", "Whenever terrain recognized as tripwires should exist.",
		NULL, {.cvar = &cv_ng_tripwireactive}, 0, 0},

	///////////
	// INSTAWHIP
	///////////
	{IT_HEADER, "Instawhip...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Instawhip", "Should you be allowed to instawhip?",
		NULL, {.cvar = &cv_ng_instawhip}, 0, 0},

	{IT_STRING | IT_CVAR, "Charge Time", "Adjust how long instawhip charges for (in hundredths of a second)",
		NULL, {.cvar = &cv_ng_instawhipcharge}, 0, 0},

	{IT_STRING | IT_CVAR, "Lockout", "Adjust wait time before using instawhip again (in hundredths of a second)",
		NULL, {.cvar = &cv_ng_instawhiplockout}, 0, 0},

	{IT_STRING | IT_CVAR, "Drain Rings", "Should holding instawhip drain rings?",
		NULL, {.cvar = &cv_ng_instawhipdrain}, 0, 0},

	///////////
	// SPINDASH
	///////////
	{IT_HEADER, "Spindash...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Spindash", "Should you be allowed to spindash?",
		NULL, {.cvar = &cv_ng_spindash}, 0, 0},

	{IT_STRING | IT_CVAR, "Threshold", "Adjust how low your speed must get to begin charging a spindash.",
		NULL, {.cvar = &cv_ng_spindashthreshold}, 0, 0},

	{IT_STRING | IT_CVAR, "Charge Time", "Adjust time before maximum spindash thrust (in tics; 0 is default behavior)",
		NULL, {.cvar = &cv_ng_spindashcharge}, 0, 0},

	{IT_STRING | IT_CVAR, "Overheat", "Whenever charging a spindash too much hurts players.",
		NULL, {.cvar = &cv_ng_spindashoverheat}, 0, 0},

	///////////
	// DRIVING
	///////////
	{IT_HEADER, "Driving...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Slope Physics", "Should there be slope physics?",
		NULL, {.cvar = &cv_ng_butteredslopes}, 0, 0},

	{IT_STRING | IT_CVAR, "Slope Resistance", "Should slopes be hard to climb?",
		NULL, {.cvar = &cv_ng_slopeclimb}, 0, 0},

	{IT_STRING | IT_CVAR, "Stairjank", "Should karts be affected by steps & bumpy roads, only roads or nothing?",
		NULL, {.cvar = &cv_ng_stairjank}, 0, 0},

	{IT_STRING | IT_CVAR, "Underwater Handling Adjust", "Should the turning of karts be affected when underwater?",
		NULL, {.cvar = &cv_ng_underwaterhandling}, 0, 0},

	///////////
	// RIVALS
	///////////
	{IT_HEADER, "Rivals...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Rivals", "Should there be rivals?",
		NULL, {.cvar = &cv_ng_rivals}, 0, 0},

	{IT_STRING | IT_CVAR, "Top Speed", "Adjust rival's top speed (10 for non-rivals)",
		NULL, {.cvar = &cv_ng_rivaltopspeed}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Power", "Adjust rival's ring power (10 for non-rivals)",
		NULL, {.cvar = &cv_ng_rivalringpower}, 0, 0},

	{IT_STRING | IT_CVAR, "Frantic Items", "Should rival use frantic items?",
		NULL, {.cvar = &cv_ng_rivalfrantic}, 0, 0},

	{IT_STRING | IT_CVAR, "2x Draft Power", "Should rival pull ahead at double speed?",
		NULL, {.cvar = &cv_ng_rivaldraft}, 0, 0},
};

void NG_Rings_OnChange(void)
{
	if(cv_ng_rings.value)
	{
		if (!con_startup)
		{
			CV_SetValue(&cv_ng_ringcap, 20);
			CV_SetValue(&cv_ng_spillcap, 20);
			CV_Set(&cv_ng_ringdebt, "On");
			CV_Set(&cv_ng_ringsting, "On");
			CV_Set(&cv_ng_maprings, "On");
			CV_Set(&cv_ng_mapringboxes, "On");
			CV_Set(&cv_ng_ringboxtransform, "On");


			// 3 - 10
			for (int i = 2; i < 10; i++)
			{
				OPTIONS_NoireGameplay[i].status = IT_STRING | IT_CVAR;
			}
		}
	}
	else
	{
		if (!con_startup)
		{
			CV_SetValue(&cv_ng_ringcap, 0);
			CV_SetValue(&cv_ng_spillcap, 0);
			CV_Set(&cv_ng_ringdebt, "Off");
			CV_Set(&cv_ng_ringsting, "Off");
			CV_Set(&cv_ng_maprings, "Off");
			CV_Set(&cv_ng_mapringboxes, "Off");
			CV_Set(&cv_ng_ringboxtransform, "Off");


			// 3 - 10
			for (int i = 2; i < 10; i++)
			{
				OPTIONS_NoireGameplay[i].status = IT_GRAYEDOUT;

			}
		}
	}
}

void NG_MapRings_OnChange(void)
{
	static boolean messageHasBeenDelivered = false;
	if (gamestate == GS_LEVEL && !messageHasBeenDelivered && menuactive)
	{
		M_StartMessage(
			"Map Rings Toggling",
			M_GetText("Changing this value requires a map reset for the new option to apply."),
			NULL,
			MM_NOTHING,
			NULL,
			NULL
		);
		messageHasBeenDelivered = true; //Through patches of violet
	}
}

void NG_Capsules_OnChange(void)
{
	static boolean messageHasBeenDelivered = false;
	if (gamestate == GS_LEVEL && !messageHasBeenDelivered && menuactive)
	{
		M_StartMessage(
			"Capsule Toggling",
			M_GetText("Changing this value requires a map reset for the new option to apply."),
			NULL,
			MM_NOTHING,
			NULL,
			NULL
		);
		messageHasBeenDelivered = true; //Through patches of violet
	}
}

void NG_Tripwire_OnChange(void)
{
	static boolean messageHasBeenDelivered = false;
	if (gamestate == GS_LEVEL && !messageHasBeenDelivered && menuactive)
	{
		M_StartMessage(
			"Tripwire Toggling",
			M_GetText("Changing this value requires a map reset for the new option to apply."),
			NULL,
			MM_NOTHING,
			NULL,
			NULL
		);
		messageHasBeenDelivered = true; //Through patches of violet
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
