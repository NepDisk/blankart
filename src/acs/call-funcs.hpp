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
/// \file  call-funcs.hpp
/// \brief Action Code Script: CallFunc instructions

#ifndef __SRB2_ACS_CALL_FUNCS_HPP__
#define __SRB2_ACS_CALL_FUNCS_HPP__

#include "acsvm.hpp"

/*--------------------------------------------------
	bool CallFunc_???(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

		These are the actual CallFuncs ran when ACS
		is executed. Which CallFuncs are executed
		is based on the indices from the compiled
		data. ACS_EnvConstruct is where the link
		between the byte code and the actual function
		is made.

	Input Arguments:-
		thread: The ACS execution thread this action
			is running on.
		argV: An array of the action's arguments.
		argC: The length of the argument array.

	Return:-
		Returns true if this function pauses the
		thread's execution. Otherwise returns false.
--------------------------------------------------*/

bool CallFunc_Random(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ThingCount(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_TagWait(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PolyWait(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_CameraWait(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ChangeFloor(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ChangeCeiling(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_LineSide(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ClearLineSpecial(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_EndPrint(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerCount(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GameType(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GameSpeed(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Timer(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_IsNetworkGame(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SectorSound(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_AmbientSound(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetLineTexture(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetLineBlocking(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetLineSpecial(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ThingSound(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_EndPrintBold(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerTeam(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerRings(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerScore(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_PlayerNumber(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ActivatorTID(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_EndLog(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_strcmp(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_strcasecmp(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_CountEnemies(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_CountPushables(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
//bool CallFunc_HaveUnlockableTrigger(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
//bool CallFunc_HaveUnlockable(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerSkin(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerBot(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerLosing(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerExiting(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC)
;
bool CallFunc_Teleport(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetViewpoint(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SpawnObject(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SpawnObjectForced(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectX(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectY(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectZ(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectVelX(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectVelY(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectVelZ(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectAngle(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectRoll(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectPitch(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectFloorZ(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectCeilingZ(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectFloorTexture(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
//bool CallFunc_GetObjectLightLevel(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_CheckObjectState(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_CheckObjectFlag(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetObjectClass(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectPosition(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectVelocity(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectAngle(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectRoll(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectPitch(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectState(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectFlag(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectClass(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectDye(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetObjectSpecial(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);


bool CallFunc_GetObjectDye(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerEmeralds(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PlayerLap(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_LowestLap(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_EncoreMode(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_BreakTheCapsules(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_TimeAttack(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_FreePlay(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GrandPrix(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_PositionStart(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_PodiumPosition(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_SetLineRenderStyle(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_MapWarp(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_AddBot(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_StopLevelExit(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_ExitLevel(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_MusicPlay(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_MusicStopAll(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_MusicRemap(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_Sin(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Cos(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Tan(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Arcsin(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Arccos(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Hypot(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Sqrt(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Floor(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Ceil(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_Round(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_InvAngle(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_OppositeColor(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_GetLineProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetLineProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetSideProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetSideProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetSectorProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetSectorProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetThingProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_SetThingProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

bool CallFunc_GetLineUserProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetSideUserProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetSectorUserProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);
bool CallFunc_GetThingUserProperty(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

// Use to prevent scripts from crashing if missing functions are called
bool CallFunc_PlaceHolder(ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC);

#endif // __SRB2_ACS_CALL_FUNCS_HPP__
