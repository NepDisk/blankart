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
/// \file  environment.cpp
/// \brief Action Code Script: Environment definition

extern "C" {
#include "../doomtype.h"
#include "../doomdef.h"
#include "../doomstat.h"

#include "../r_defs.h"
#include "../r_state.h"
#include "../g_game.h"
#include "../p_spec.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../p_local.h"
}

#include "ACSVM/ACSVM/Code.hpp"
#include "ACSVM/ACSVM/CodeData.hpp"
#include "ACSVM/ACSVM/Environment.hpp"
#include "ACSVM/ACSVM/Error.hpp"
#include "ACSVM/ACSVM/Module.hpp"
#include "ACSVM/ACSVM/Scope.hpp"
#include "ACSVM/ACSVM/Script.hpp"
#include "ACSVM/ACSVM/Serial.hpp"
#include "ACSVM/ACSVM/Thread.hpp"
#include "ACSVM/Util/Floats.hpp"

#include "environment.hpp"
#include "thread.hpp"
#include "call-funcs.hpp"
#include "../cxxutil.hpp"

using namespace srb2::acs;

Environment ACSEnv;

Environment::Environment()
{
	ACSVM::GlobalScope *global = getGlobalScope(0);

	// Activate global scope immediately, since we don't want it off.
	// Not that we're adding any modules to it, though. :p
	global->active = true;

	// Set a branch limit (same as ZDoom's instruction limit)
	branchLimit = 2000000;

	// Add the data & function pointers.

	// Starting with raw ACS0 codes. I'm using this classic-style
	// format here to have a blueprint for what needs implementing,
	// but it'd also be fine to move these to new style.

	// See also:
	// - https://doomwiki.org/wiki/ACS0_instruction_set
	// - https://github.com/DavidPH/ACSVM/blob/master/ACSVM/CodeData.hpp
	// - https://github.com/DavidPH/ACSVM/blob/master/ACSVM/CodeList.hpp

	//  0 to 56: Implemented by ACSVM
	addCodeDataACS0( 57, {"",        2, addCallFunc(CallFunc_Random)});
	addCodeDataACS0( 58, {"WW",      0, addCallFunc(CallFunc_Random)});
	addCodeDataACS0( 59, {"",        2, addCallFunc(CallFunc_ThingCount)});
	addCodeDataACS0( 60, {"WW",      0, addCallFunc(CallFunc_ThingCount)});
	addCodeDataACS0( 61, {"",        1, addCallFunc(CallFunc_TagWait)});
	addCodeDataACS0( 62, {"W",       0, addCallFunc(CallFunc_TagWait)});
	addCodeDataACS0( 63, {"",        1, addCallFunc(CallFunc_PolyWait)});
	addCodeDataACS0( 64, {"W",       0, addCallFunc(CallFunc_PolyWait)});
	addCodeDataACS0( 65, {"",        2, addCallFunc(CallFunc_ChangeFloor)});
	addCodeDataACS0( 66, {"WWS",     0, addCallFunc(CallFunc_ChangeFloor)});
	addCodeDataACS0( 67, {"",        2, addCallFunc(CallFunc_ChangeCeiling)});
	addCodeDataACS0( 68, {"WWS",     0, addCallFunc(CallFunc_ChangeCeiling)});
	// 69 to 79: Implemented by ACSVM
	addCodeDataACS0( 80, {"",        0, addCallFunc(CallFunc_LineSide)});
	// 81 to 82: Implemented by ACSVM
	addCodeDataACS0( 83, {"",        0, addCallFunc(CallFunc_ClearLineSpecial)});
	// 84 to 85: Implemented by ACSVM
	addCodeDataACS0( 86, {"",        0, addCallFunc(CallFunc_EndPrint)});
	// 87 to 89: Implemented by ACSVM
	addCodeDataACS0( 90, {"",        0, addCallFunc(CallFunc_PlayerCount)});
	addCodeDataACS0( 91, {"",        0, addCallFunc(CallFunc_GameType)});
	addCodeDataACS0( 92, {"",        0, addCallFunc(CallFunc_GameSpeed)});
	addCodeDataACS0( 93, {"",        0, addCallFunc(CallFunc_Timer)});
	addCodeDataACS0( 94, {"",        2, addCallFunc(CallFunc_SectorSound)});
	addCodeDataACS0( 95, {"",        2, addCallFunc(CallFunc_AmbientSound)});

	addCodeDataACS0( 97, {"",        4, addCallFunc(CallFunc_SetLineTexture)});

	addCodeDataACS0( 99, {"",        7, addCallFunc(CallFunc_SetLineSpecial)});
	addCodeDataACS0(100, {"",        3, addCallFunc(CallFunc_ThingSound)});
	addCodeDataACS0(101, {"",        0, addCallFunc(CallFunc_EndPrintBold)});

	addCodeDataACS0(118, {"",        0, addCallFunc(CallFunc_IsNetworkGame)});
	addCodeDataACS0(119, {"",        0, addCallFunc(CallFunc_PlayerTeam)});
	addCodeDataACS0(120, {"",        0, addCallFunc(CallFunc_PlayerRings)});

	addCodeDataACS0(122, {"",        0, addCallFunc(CallFunc_PlayerScore)});

	// 136 to 137: Implemented by ACSVM

	// 157: Implemented by ACSVM

	// 167 to 173: Implemented by ACSVM
	addCodeDataACS0(174, {"BB",      0, addCallFunc(CallFunc_Random)});
	// 175 to 179: Implemented by ACSVM

	// 181 to 189: Implemented by ACSVM

	// 203 to 217: Implemented by ACSVM

	// 225 to 243: Implemented by ACSVM

	// 253: Implemented by ACSVM

	// 256 to 257: Implemented by ACSVM

	// 263: Implemented by ACSVM
	addCodeDataACS0(270, {"",        0, addCallFunc(CallFunc_EndLog)});
	// 273 to 275: Implemented by ACSVM

	// 291 to 325: Implemented by ACSVM

	// 330: Implemented by ACSVM

	// 349 to 361: Implemented by ACSVM

	// 363 to 380: Implemented by ACSVM

	// Now for new style functions.
	// This style is preferred for added functions
	// that aren't mimicing one from Hexen's or ZDoom's
	// ACS implementations.
	addFuncDataACS0(   1, addCallFunc(CallFunc_GetLineProperty));
	addFuncDataACS0(   2, addCallFunc(CallFunc_SetLineProperty));
	addFuncDataACS0(   3, addCallFunc(CallFunc_GetLineUserProperty));
	addFuncDataACS0(   4, addCallFunc(CallFunc_GetSectorProperty));
	addFuncDataACS0(   5, addCallFunc(CallFunc_SetSectorProperty));
	addFuncDataACS0(   6, addCallFunc(CallFunc_GetSectorUserProperty));
	addFuncDataACS0(   7, addCallFunc(CallFunc_GetSideProperty));
	addFuncDataACS0(   8, addCallFunc(CallFunc_SetSideProperty));
	addFuncDataACS0(   9, addCallFunc(CallFunc_GetSideUserProperty));
	addFuncDataACS0(  10, addCallFunc(CallFunc_GetThingProperty));
	addFuncDataACS0(  11, addCallFunc(CallFunc_SetThingProperty));
	addFuncDataACS0(  12, addCallFunc(CallFunc_GetThingUserProperty));
	//addFuncDataACS0(  13, addCallFunc(CallFunc_GetPlayerProperty));
	//addFuncDataACS0(  14, addCallFunc(CallFunc_SetPlayerProperty));
	//addFuncDataACS0(  15, addCallFunc(CallFunc_GetPolyobjProperty));
	//addFuncDataACS0(  16, addCallFunc(CallFunc_SetPolyobjProperty));

	addFuncDataACS0( 100, addCallFunc(CallFunc_strcmp));
	addFuncDataACS0( 101, addCallFunc(CallFunc_strcasecmp));

	addFuncDataACS0( 300, addCallFunc(CallFunc_CountEnemies));
	addFuncDataACS0( 301, addCallFunc(CallFunc_CountPushables));
	//addFuncDataACS0( 302, addCallFunc(CallFunc_HaveUnlockableTrigger));
	//addFuncDataACS0( 303, addCallFunc(CallFunc_HaveUnlockable));
	addFuncDataACS0( 304, addCallFunc(CallFunc_PlayerSkin));
	addFuncDataACS0( 305, addCallFunc(CallFunc_GetObjectDye));
	addFuncDataACS0( 306, addCallFunc(CallFunc_PlayerEmeralds));
	addFuncDataACS0( 307, addCallFunc(CallFunc_PlayerLap));
	addFuncDataACS0( 308, addCallFunc(CallFunc_LowestLap));
	addFuncDataACS0( 309, addCallFunc(CallFunc_EncoreMode));
	addFuncDataACS0( 310, addCallFunc(CallFunc_BreakTheCapsules));
	addFuncDataACS0( 311, addCallFunc(CallFunc_TimeAttack));
	addFuncDataACS0( 312, addCallFunc(CallFunc_ThingCount));
	addFuncDataACS0( 313, addCallFunc(CallFunc_GrandPrix));
	addFuncDataACS0( 315, addCallFunc(CallFunc_PlayerBot));

	addFuncDataACS0( 500, addCallFunc(CallFunc_CameraWait));
	addFuncDataACS0( 501, addCallFunc(CallFunc_PodiumPosition));
	addFuncDataACS0( 503, addCallFunc(CallFunc_SetLineRenderStyle));
	addFuncDataACS0( 504, addCallFunc(CallFunc_MapWarp));
	addFuncDataACS0( 505, addCallFunc(CallFunc_AddBot));
	addFuncDataACS0( 506, addCallFunc(CallFunc_StopLevelExit));
	addFuncDataACS0( 507, addCallFunc(CallFunc_ExitLevel));
	addFuncDataACS0( 508, addCallFunc(CallFunc_MusicPlay));
	addFuncDataACS0( 509, addCallFunc(CallFunc_MusicStopAll));
	addFuncDataACS0( 510, addCallFunc(CallFunc_MusicRemap));
}

ACSVM::Thread *Environment::allocThread()
{
	return new Thread(this);
}

ACSVM::ModuleName Environment::getModuleName(char const *str, size_t len)
{
	ACSVM::String *name = getString(str, len);
	lumpnum_t lump = W_CheckNumForNameInFolder(str, "ACS/");

	return { name, nullptr, static_cast<size_t>(lump) };
}

void Environment::loadModule(ACSVM::Module *module)
{
	ACSVM::ModuleName *const name = &module->name;

	size_t lumpLen = 0;
	std::vector<ACSVM::Byte> data;

	if (name->i == (size_t)LUMPERROR)
	{
		// No lump given for module.
		CONS_Alert(CONS_WARNING, "Could not find ACS module \"%s\"; scripts will not function properly!\n", name->s->str);
		return; //throw ACSVM::ReadError("file open failure");
	}

	lumpLen = W_LumpLength(name->i);

	if (W_IsLumpWad(name->i) == true || lumpLen == 0)
	{
		CONS_Debug(DBG_SETUP, "Attempting to load ACS module from the BEHAVIOR lump of map '%s'...\n", name->s->str);

		// The lump given is a virtual resource.
		// Try to grab a BEHAVIOR lump from inside of it.
		virtres_t *vRes = vres_GetMap(name->i);
		auto _ = srb2::finally([vRes]() { vres_Free(vRes); });

		virtlump_t *vLump = vres_Find(vRes, "BEHAVIOR");
		if (vLump != nullptr && vLump->size > 0)
		{
			data.insert(data.begin(), vLump->data, vLump->data + vLump->size);
			CONS_Debug(DBG_SETUP, "Successfully found BEHAVIOR lump.\n");
		}
		else
		{
			CONS_Debug(DBG_SETUP, "No BEHAVIOR lump found.\n");
		}
	}
	else
	{
		CONS_Debug(DBG_SETUP, "Loading ACS module directly from lump '%s'...\n", name->s->str);

		// It's a real lump.
		ACSVM::Byte *lump = static_cast<ACSVM::Byte *>(Z_Calloc(lumpLen, PU_STATIC, nullptr));
		auto _ = srb2::finally([lump]() { Z_Free(lump); });

		W_ReadLump(name->i, lump);
		data.insert(data.begin(), lump, lump + lumpLen);
	}

	if (data.empty() == false)
	{
		try
		{
			module->readBytecode(data.data(), data.size());
		}
		catch (const ACSVM::ReadError &e)
		{
			CONS_Alert(CONS_ERROR, "Failed to load ACS module '%s': %s\n", name->s->str, e.what());
			throw ACSVM::ReadError("failed import");
		}
	}
	else
	{
		// Unlike Hexen, a BEHAVIOR lump is not required.
		// Simply ignore in this instance.
		CONS_Debug(DBG_SETUP, "ACS module has no data, ignoring...\n");
	}
}

bool Environment::checkTag(ACSVM::Word type, ACSVM::Word tag)
{
	switch (type)
	{
		case ACS_TAGTYPE_SECTOR:
		{
			INT32 secnum = -1;

			TAG_ITER_SECTORS(tag, secnum)
			{
				sector_t *sec = &sectors[secnum];

				if (sec->floordata != nullptr || sec->ceilingdata != nullptr)
				{
					return false;
				}
			}

			return true;
		}

		case ACS_TAGTYPE_POLYOBJ:
		{
			const polyobj_t *po = Polyobj_GetForNum(tag);
			return (po == nullptr || po->thinker == nullptr);
		}

		case ACS_TAGTYPE_CAMERA:
		{
			const mobj_t *camera = P_FindObjectTypeFromTag(MT_ALTVIEWMAN, tag);
			if (camera == nullptr || camera->spawnpoint == nullptr)
			{
				return true;
			}

			return (camera->tracer == nullptr || P_MobjWasRemoved(camera->tracer) == true);
		}
	}

	return true;
}

ACSVM::Word Environment::callSpecImpl
	(
		ACSVM::Thread *thread, ACSVM::Word spec,
		const ACSVM::Word *argV, ACSVM::Word argC
	)
{
	auto info = &static_cast<Thread *>(thread)->info;
	ACSVM::MapScope *const map = thread->scopeMap;

	int arg = 0;
	int numStringArgs = 0;

	INT32 args[NUMLINEARGS] = {0};

	char *stringargs[NUMLINESTRINGARGS] = {0};
	auto _ = srb2::finally(
		[stringargs]()
		{
			for (int i = 0; i < NUMLINESTRINGARGS; i++)
			{
				Z_Free(stringargs[i]);
			}
		}
	);

	activator_t *activator = static_cast<activator_t *>(Z_Calloc(sizeof(activator_t), PU_LEVEL, nullptr));
	auto __ = srb2::finally(
		[info, activator]()
		{
			if (info->thread_era == thinker_era)
			{
				P_SetTarget(&activator->mo, NULL);
				Z_Free(activator);
			}
		}
	);

	// This needs manually set, as ACS just uses indicies in the
	// compiled string table and not actual strings, and SRB2 has
	// separate args and stringargs, so there's no way to
	// properly distinguish them.
	switch (spec)
	{
		case 442:
			numStringArgs = 2;
			break;
		case 413:
		case 414:
		case 415:
		case 423:
		case 425:
		case 443:
		case 459:
		case 461:
		case 463:
		case 469:
			numStringArgs = 1;
			break;
		default:
			break;
	}

	for (; arg < numStringArgs; arg++)
	{
		ACSVM::String *strPtr = map->getString(argV[arg]);

		stringargs[arg] = static_cast<char *>(Z_Malloc(strPtr->len + 1, PU_STATIC, nullptr));
		M_Memcpy(stringargs[arg], strPtr->str, strPtr->len + 1);
	}

	for (; arg < std::min((signed)argC, NUMLINEARGS); arg++)
	{
		args[arg - numStringArgs] = argV[arg];
	}

	P_SetTarget(&activator->mo, info->mo);
	activator->line = info->line;
	activator->side = info->side;
	activator->sector = info->sector;
	activator->po = info->po;
	activator->fromLineSpecial = false;

	P_ProcessSpecial(activator, spec, args, stringargs);
	return 1;
}

void Environment::printKill(ACSVM::Thread *thread, ACSVM::Word type, ACSVM::Word data)
{
	std::string scriptName;

	ACSVM::String *scriptNamePtr = (thread->script != nullptr) ? (thread->script->name.s) : nullptr;
	if (scriptNamePtr && scriptNamePtr->len)
		scriptName = std::string(scriptNamePtr->str);
	else
		scriptName = std::to_string((int)thread->script->name.i);

	ACSVM::KillType killType = static_cast<ACSVM::KillType>(type);

	if (killType == ACSVM::KillType::BranchLimit)
	{
		CONS_Alert(CONS_ERROR, "Terminated runaway script %s\n", scriptName.c_str());
		return;
	}
	else if (killType == ACSVM::KillType::UnknownCode)
	{
		CONS_Alert(CONS_ERROR, "ACSVM ERROR: Unknown opcode %d in script %s\n", data, scriptName.c_str());
	}
	else if (killType == ACSVM::KillType::UnknownFunc)
	{
		CONS_Alert(CONS_ERROR, "ACSVM ERROR: Unknown function %d in script %s\n", data, scriptName.c_str());
	}
	else if (killType == ACSVM::KillType::OutOfBounds)
	{
		CONS_Alert(CONS_ERROR, "ACSVM ERROR: Jumped to out of bounds location %lu in script %s\n",
			(thread->codePtr - thread->module->codeV.data() - 1), scriptName.c_str());
	}
	else
	{
		CONS_Alert(CONS_ERROR, "ACSVM ERROR: Kill %u:%d at %lu in script %s\n",
			type, data, (thread->codePtr - thread->module->codeV.data() - 1), scriptName.c_str());
	}

	CONS_Printf("Script terminated.\n");
}
