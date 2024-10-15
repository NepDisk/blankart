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
	//addFuncDataACS0(   1, addCallFunc(CallFunc_GetLineUDMFInt));
	//addFuncDataACS0(   2, addCallFunc(CallFunc_GetLineUDMFFixed));
	//addFuncDataACS0(   3, addCallFunc(CallFunc_GetThingUDMFInt));
	//addFuncDataACS0(   4, addCallFunc(CallFunc_GetThingUDMFFixed));
	//addFuncDataACS0(   5, addCallFunc(CallFunc_GetSectorUDMFInt));
	//addFuncDataACS0(   6, addCallFunc(CallFunc_GetSectorUDMFFixed));
	//addFuncDataACS0(   7, addCallFunc(CallFunc_GetSideUDMFInt));
	//addFuncDataACS0(   8, addCallFunc(CallFunc_GetSideUDMFFixed));

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
}

ACSVM::Thread *Environment::allocThread()
{
	return new Thread(this);
}

void Environment::loadModule(ACSVM::Module *module)
{
	ACSVM::ModuleName *const name = &module->name;

	size_t lumpLen = 0;
	std::vector<ACSVM::Byte> data;

	if (name->i == (size_t)LUMPERROR)
	{
		// No lump given for module.
		CONS_Alert(CONS_ERROR, "Bad lump for ACS module '%s'\n", name->s->str);
		throw ACSVM::ReadError("file open failure");
	}

	lumpLen = W_LumpLength(name->i);

	if (W_IsLumpWad(name->i) == true || lumpLen == 0)
	{
		// The lump given is a virtual resource.
		// Try to grab a BEHAVIOR lump from inside of it.
		virtres_t *vRes = vres_GetMap(name->i);
		virtlump_t *vLump = vres_Find(vRes, "BEHAVIOR");

		CONS_Printf("Attempting to load ACS module from map's virtual resource...\n");

		if (vLump != nullptr && vLump->size > 0)
		{
			data.insert(data.begin(), vLump->data, vLump->data + vLump->size);
			CONS_Printf("Successfully found BEHAVIOR lump.\n");
		}
		else
		{
			CONS_Printf("No BEHAVIOR lump found.\n");
		}

		vres_Free(vRes);
	}
	else
	{
		// It's a real lump.
		ACSVM::Byte *lump = static_cast<ACSVM::Byte *>(Z_Calloc(lumpLen, PU_STATIC, nullptr));
		auto _ = srb2::finally([lump]() { Z_Free(lump); });

		W_ReadLump(name->i, lump);
		data.insert(data.begin(), lump, lump + lumpLen);

		CONS_Printf("Loading ACS module directly from lump.\n");
	}

	if (data.empty() == false)
	{
		try
		{
			module->readBytecode(data.data(), data.size());
		}
		catch (const ACSVM::ReadError &e)
		{
			CONS_Printf("Failed to load ACS module '%s': %s\n", name->s->str, e.what());
			throw ACSVM::ReadError("failed import");
		}
	}
	else
	{
		// Unlike Hexen, a BEHAVIOR lump is not required.
		// Simply ignore in this instance.
		CONS_Printf("No data received, ignoring...\n");
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
	auto __ = srb2::finally([activator]() { Z_Free(activator); });

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
