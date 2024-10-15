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
/// \file  thread.hpp
/// \brief Action Code Script: Thread definition

#ifndef __SRB2_ACS_THREAD_HPP__
#define __SRB2_ACS_THREAD_HPP__

extern "C" {
#include "../doomtype.h"
#include "../doomdef.h"
#include "../doomstat.h"
#include "../p_tick.h"
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


namespace srb2::acs {

//
// Special global script types.
//
enum acs_scriptType_e
{
	ACS_ST_OPEN			=  1, // OPEN: Runs once when the level starts.
	ACS_ST_RESPAWN		=  2, // RESPAWN: Runs when a player respawns.
	ACS_ST_DEATH		=  3, // DEATH: Runs when a player dies.
	ACS_ST_ENTER		=  4, // ENTER: Runs when a player enters the game; both on start of the level, and when un-spectating.
	ACS_ST_LAP			=  5, // LAP: Runs when a player's lap increases from crossing the finish line.
};

//
// Script "waiting on tag" types.
//
enum acs_tagType_e
{
	ACS_TAGTYPE_POLYOBJ,
	ACS_TAGTYPE_SECTOR,
};

class ThreadInfo : public ACSVM::ThreadInfo
{
public:
	mobj_t *mo;					// Object that activated this thread.
	line_t *line;				// Linedef that activated this thread.
	UINT8 side;					// Front / back side of said linedef.
	sector_t *sector;			// Sector that activated this thread.
	polyobj_t *po;				// Polyobject that activated this thread.
	bool fromLineSpecial;		// Called from P_ProcessLineSpecial.

	ThreadInfo() :
		mo{ nullptr },
		line{ nullptr },
		side{ 0 },
		sector{ nullptr },
		po{ nullptr },
		fromLineSpecial{ false }
	{
	}

	ThreadInfo(const ThreadInfo &info) :
		mo{ nullptr },
		line{ info.line },
		side{ info.side },
		sector{ info.sector },
		po{ info.po },
		fromLineSpecial{ info.fromLineSpecial }
	{
		P_SetTarget(&mo, info.mo);
	}

	~ThreadInfo()
	{
		P_SetTarget(&mo, nullptr);
	}

	ThreadInfo &operator = (const ThreadInfo &info)
	{
		P_SetTarget(&mo, info.mo);
		line = info.line;
		side = info.side;
		po = info.po;

		return *this;
	}
};

class Thread : public ACSVM::Thread
{
public:
	ThreadInfo info;

	explicit Thread(ACSVM::Environment *env_) : ACSVM::Thread{env_} {}

	virtual ACSVM::ThreadInfo const *getInfo() const { return &info; }

	virtual void start(
		ACSVM::Script *script, ACSVM::MapScope *map,
		const ACSVM::ThreadInfo *info, const ACSVM::Word *argV, ACSVM::Word argC
	);

	virtual void stop();
};

}

#endif // __SRB2_ACS_THREAD_HPP__
