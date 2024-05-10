// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2024 by hayaUnderscore
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "n_wad.h"

UINT8 W_CheckMultipleLumps(const char* lump, ...)
{
	va_list lumps;
	va_start(lumps, lump);
	const char* lumpname = lump;

	while (lumpname != NULL)
	{
		if (!W_LumpExists(lumpname))
		{
			va_end(lumps);
			return false;
		}
		lumpname = va_arg(lumps, const char*);
	}

	va_end(lumps);
	return true;
}
