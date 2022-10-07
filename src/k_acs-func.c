// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2016 by James Haley, David Hill, et al. (Team Eternity)
// Copyright (C) 2022 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2022 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_acs-func.c
/// \brief ACS CallFunc definitions

#include "k_acs.h"

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_think.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "w_wad.h"
#include "m_random.h"
#include "g_game.h"
#include "d_player.h"
#include "r_defs.h"
#include "r_state.h"
#include "p_polyobj.h"
#include "taglist.h"

/*--------------------------------------------------
	bool ACS_CF_Random(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		ACS wrapper for P_RandomRange.
--------------------------------------------------*/
bool ACS_CF_Random(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	INT32 low = 0;
	INT32 high = 0;

	(void)argC;

	low = (INT32)argV[0];
	high = (INT32)argV[1];

	ACSVM_Thread_DataStk_Push(thread, P_RandomRange(PR_ACS, low, high));
	return false;
}

/*--------------------------------------------------
	bool ACS_CF_TagWait(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Pauses the thread until the tagged
		sector stops moving.
--------------------------------------------------*/
bool ACS_CF_TagWait(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	ACSVM_ThreadState st = {0};

	(void)argC;

	st.state = ACSVM_ThreadState_WaitTag;
	st.data = argV[0];
	st.type = ACS_TAGTYPE_SECTOR;

	ACSVM_Thread_SetState(thread, st);
	return true; // Execution interrupted
}

/*--------------------------------------------------
	bool ACS_CF_PolyWait(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Pauses the thread until the tagged
		polyobject stops moving.
--------------------------------------------------*/
bool ACS_CF_PolyWait(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	ACSVM_ThreadState st = {0};

	(void)argC;

	st.state = ACSVM_ThreadState_WaitTag;
	st.data = argV[0];
	st.type = ACS_TAGTYPE_POLYOBJ;

	ACSVM_Thread_SetState(thread, st);
	return true; // Execution interrupted
}

/*--------------------------------------------------
	bool ACS_CF_ChangeFloor(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Changes a floor texture.
--------------------------------------------------*/
bool ACS_CF_ChangeFloor(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	ACSVM_MapScope *map = NULL;
	ACSVM_String *str = NULL;
	const char *texName = NULL;

	INT32 secnum = -1;
	INT32 tag = 0;

	(void)argC;

	tag = argV[0];

	map = ACSVM_Thread_GetScopeMap(thread);
	str = ACSVM_MapScope_GetString(map, argV[1]);
	texName = ACSVM_String_GetStr(str);

	TAG_ITER_SECTORS(tag, secnum)
	{
		sector_t *sec = &sectors[secnum];
		sec->floorpic = P_AddLevelFlatRuntime(texName);
	}

	return false;
}

/*--------------------------------------------------
	bool ACS_CF_ChangeCeiling(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Changes a ceiling texture.
--------------------------------------------------*/
bool ACS_CF_ChangeCeiling(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	ACSVM_MapScope *map = NULL;
	ACSVM_String *str = NULL;
	const char *texName = NULL;

	INT32 secnum = -1;
	INT32 tag = 0;

	(void)argC;

	tag = argV[0];

	map = ACSVM_Thread_GetScopeMap(thread);
	str = ACSVM_MapScope_GetString(map, argV[1]);
	texName = ACSVM_String_GetStr(str);

	TAG_ITER_SECTORS(tag, secnum)
	{
		sector_t *sec = &sectors[secnum];
		sec->ceilingpic = P_AddLevelFlatRuntime(texName);
	}

	return false;
}

/*--------------------------------------------------
	bool ACS_CF_EndPrint(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		ACS wrapper for CONS_Printf.
--------------------------------------------------*/
bool ACS_CF_EndPrint(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	ACSVM_PrintBuf *buf = NULL;

	(void)argV;
	(void)argC;

	buf = ACSVM_Thread_GetPrintBuf(thread);
	CONS_Printf("%s\n", ACSVM_PrintBuf_GetData(buf));
	ACSVM_PrintBuf_Drop(buf);

	return false;
}

/*--------------------------------------------------
	bool ACS_CF_PlayerCount(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Pushes the number of players to ACS.
--------------------------------------------------*/
bool ACS_CF_PlayerCount(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	UINT8 numPlayers = 0;
	UINT8 i;

	(void)argV;
	(void)argC;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *player = NULL;

		if (playeringame[i] == false)
		{
			continue;
		}

		player = &players[i];

		if (player->spectator == true)
		{
			continue;
		}

		numPlayers++;
	}

	ACSVM_Thread_DataStk_Push(thread, numPlayers);
	return false;
}

/*--------------------------------------------------
	bool ACS_CF_GameType(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Pushes the current gametype to ACS.
--------------------------------------------------*/
bool ACS_CF_GameType(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	(void)argV;
	(void)argC;

	ACSVM_Thread_DataStk_Push(thread, gametype);
	return false;
}

/*--------------------------------------------------
	bool ACS_CF_GameSpeed(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Pushes the current game speed to ACS.
--------------------------------------------------*/
bool ACS_CF_GameSpeed(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	(void)argV;
	(void)argC;

	ACSVM_Thread_DataStk_Push(thread, gamespeed);
	return false;
}

/*--------------------------------------------------
	bool ACS_CF_Timer(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)

		Pushes leveltime to ACS.
--------------------------------------------------*/
bool ACS_CF_Timer(ACSVM_Thread *thread, ACSVM_Word const *argV, ACSVM_Word argC)
{
	(void)argV;
	(void)argC;

	ACSVM_Thread_DataStk_Push(thread, leveltime);
	return false;
}
