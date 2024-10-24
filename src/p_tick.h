// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_tick.h
/// \brief Thinkers, Tickers

#ifndef __P_TICK__
#define __P_TICK__

#ifdef __GNUG__
#pragma interface
#endif

extern tic_t leveltime;

// Called by G_Ticker. Carries out all thinking of enemies and players.
void Command_Numthinkers_f(void);
void Command_CountMobjs_f(void);

void P_RunChaseCameras(void);
void P_Ticker(boolean run);
void P_DoTeamscrambling(void);
void P_RemoveThinkerDelayed(thinker_t *thinker); //killed
mobj_t *P_SetTarget(mobj_t **mo, mobj_t *target);   // killough 11/98

// Negate the value for tics
INT32 P_AltFlip(INT32 value, tic_t tics);
#define P_RandomFlip(value) P_AltFlip(value, 1)

#endif
