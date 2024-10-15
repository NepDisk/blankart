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
/// \file  thread.cpp
/// \brief Action Code Script: Thread definition

#include "thread.hpp"

extern "C" {
#include "../doomtype.h"
#include "../doomdef.h"
#include "../doomstat.h"
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

using namespace srb2::acs;

void Thread::start(
	ACSVM::Script *script, ACSVM::MapScope *map,
	const ACSVM::ThreadInfo *infoPtr, const ACSVM::Word *argV, ACSVM::Word argC)
{
	ACSVM::Thread::start(script, map, infoPtr, argV, argC);

	if (infoPtr != nullptr)
	{
		info = *static_cast<const ThreadInfo *>(infoPtr);
	}
	else
	{
		info = {};
	}

	result = 1;
}

void Thread::stop()
{
	ACSVM::Thread::stop();
	info = {};
}
