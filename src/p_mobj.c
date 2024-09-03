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
/// \file  p_mobj.c
/// \brief Moving object handling. Spawn functions

#include "p_mobj.h"
#include "d_netcmd.h"
#include "dehacked.h"
#include "doomdef.h"
#include "doomtype.h"
#include "g_game.h"
#include "g_input.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "p_local.h"
#include "p_setup.h"
#include "r_fps.h"
#include "r_main.h"
#include "r_skins.h"
#include "r_sky.h"
#include "r_splats.h"
#include "s_sound.h"
#include "z_zone.h"
#include "m_random.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "info.h"
#include "i_video.h"
#include "lua_hook.h"
#include "p_slopes.h"
#include "f_finale.h"
#include "m_cond.h"

// SRB2kart
#include "k_kart.h"
#include "k_battle.h"
#include "k_boss.h"
#include "k_color.h"
#include "k_bot.h"
#include "k_terrain.h"
#include "k_collide.h"

static CV_PossibleValue_t CV_BobSpeed[] = {{0, "MIN"}, {4*FRACUNIT, "MAX"}, {0, NULL}};
consvar_t cv_movebob = CVAR_INIT ("movebob", "1.0", CV_FLOAT|CV_SAVE, CV_BobSpeed, NULL);

actioncache_t actioncachehead;

static mobj_t *overlaycap = NULL;
mobj_t *kitemcap = NULL;	// Used for Kart offensive items (the ones that can get removed by sizedown)
mobj_t *waypointcap = NULL;

mobj_t *mobjcache = NULL;

void P_InitCachedActions(void)
{
	actioncachehead.prev = actioncachehead.next = &actioncachehead;
}

void P_RunCachedActions(void)
{
	actioncache_t *ac;
	actioncache_t *next;

	for (ac = actioncachehead.next; ac != &actioncachehead; ac = next)
	{
		var1 = states[ac->statenum].var1;
		var2 = states[ac->statenum].var2;
		astate = &states[ac->statenum];
		if (ac->mobj && !P_MobjWasRemoved(ac->mobj)) // just in case...
			states[ac->statenum].action.acp1(ac->mobj);
		next = ac->next;
		Z_Free(ac);
	}
}

void P_AddCachedAction(mobj_t *mobj, INT32 statenum)
{
	actioncache_t *newaction = Z_Calloc(sizeof(actioncache_t), PU_LEVEL, NULL);
	newaction->mobj = mobj;
	newaction->statenum = statenum;
	actioncachehead.prev->next = newaction;
	newaction->next = &actioncachehead;
	newaction->prev = actioncachehead.prev;
	actioncachehead.prev = newaction;
}

static inline INT32 randomframe (mobj_t *mobj, INT32 n)
{
	// Only mobj thinkers should use synced RNG
	if (mobj->thinker.function.acp1 == (actionf_p1)P_MobjThinker)
		return P_RandomKey(n);
	else
		return M_RandomKey(n);
}

//
// P_SetupStateAnimation
//
static void P_SetupStateAnimation(mobj_t *mobj, state_t *st)
{
	INT32 animlength = (mobj->sprite == SPR_PLAY && mobj->skin)
		? (INT32)(((skin_t *)mobj->skin)->sprites[mobj->sprite2].numframes) - 1
		: st->var1;

	if (!(st->frame & FF_ANIMATE))
		return;

	if (animlength <= 0 || st->var2 == 0)
	{
		mobj->frame &= ~FF_ANIMATE;
		return; // Crash/stupidity prevention
	}

	mobj->anim_duration = (UINT16)st->var2;

	if (st->frame & FF_GLOBALANIM)
	{
		// Attempt to account for the pre-ticker for objects spawned on load
		if (!leveltime) return;

		mobj->anim_duration -= (leveltime + 2) % st->var2;            // Duration synced to timer
		mobj->frame += ((leveltime + 2) / st->var2) % (animlength + 1); // Frame synced to timer (duration taken into account)
	}
	else if (st->frame & FF_RANDOMANIM)
	{
		mobj->frame += randomframe(mobj, animlength + 1);     // Random starting frame
		mobj->anim_duration -= randomframe(mobj, st->var2); // Random duration for first frame
	}
}

//
// P_CycleStateAnimation
//
FUNCINLINE static ATTRINLINE void P_CycleStateAnimation(mobj_t *mobj)
{
	// var2 determines delay between animation frames
	if (!(mobj->frame & FF_ANIMATE) || --mobj->anim_duration != 0)
		return;

	mobj->anim_duration = (UINT16)mobj->state->var2;

	if (mobj->sprite != SPR_PLAY)
	{
		const UINT8 start = mobj->state->frame & FF_FRAMEMASK;

		UINT8 frame = mobj->frame & FF_FRAMEMASK;

		// compare the current sprite frame to the one we started from
		// if more than var1 away from it, swap back to the original
		// else just advance by one
		if ((mobj->frame & FF_REVERSEANIM ? (start - (--frame)) : ((++frame) - start)) > mobj->state->var1)
			frame = start;

		mobj->frame = frame | (mobj->frame & ~FF_FRAMEMASK);

		return;
	}

	// sprite2 version of above
	if (mobj->skin && (((++mobj->frame) & FF_FRAMEMASK) >= (UINT32)(((skin_t *)mobj->skin)->sprites[mobj->sprite2].numframes)))
		mobj->frame &= ~FF_FRAMEMASK;
}

//
// P_CycleMobjState
//
static void P_CycleMobjState(mobj_t *mobj)
{
	// state animations
	P_CycleStateAnimation(mobj);

	// cycle through states,
	// calling action functions at transitions
	if (mobj->tics != -1)
	{
		mobj->tics--;

		// you can cycle through multiple states in a tic
		if (!mobj->tics && mobj->state)
			if (!P_SetMobjState(mobj, mobj->state->nextstate))
				return; // freed itself
	}
}

//
// P_CycleMobjState for players.
//
static void P_CyclePlayerMobjState(mobj_t *mobj)
{
	// state animations
	P_CycleStateAnimation(mobj);

	// cycle through states,
	// calling action functions at transitions
	if (mobj->tics != -1)
	{
		mobj->tics--;

		// you can cycle through multiple states in a tic
		if (!mobj->tics && mobj->state)
			if (!P_SetPlayerMobjState(mobj, mobj->state->nextstate))
				return; // freed itself
	}
}

//
// P_SetPlayerMobjState
// Returns true if the mobj is still present.
//
// Separate from P_SetMobjState because of the flashing check and Super states
//
boolean P_SetPlayerMobjState(mobj_t *mobj, statenum_t state)
{
	state_t *st;
	player_t *player = mobj->player;

	// remember states seen, to detect cycles:
	static statenum_t seenstate_tab[NUMSTATES]; // fast transition table
	statenum_t *seenstate = seenstate_tab; // pointer to table
	static INT32 recursion; // detects recursion
	statenum_t i; // initial state
	statenum_t tempstate[NUMSTATES]; // for use with recursion

#ifdef PARANOIA
	if (player == NULL)
		I_Error("P_SetPlayerMobjState used for non-player mobj. Use P_SetMobjState instead!\n(Mobj type: %d, State: %d)", mobj->type, state);
#endif

	// Set animation state
	// The pflags version of this was just as convoluted.
	switch(state)
	{
		case S_KART_STILL:
		case S_KART_STILL_L:
		case S_KART_STILL_R:
		case S_KART_STILL_GLANCE_L:
		case S_KART_STILL_GLANCE_R:
		case S_KART_STILL_LOOK_L:
		case S_KART_STILL_LOOK_R:
			player->panim = PA_STILL;
			break;
		case S_KART_SLOW:
		case S_KART_SLOW_L:
		case S_KART_SLOW_R:
		case S_KART_SLOW_GLANCE_L:
		case S_KART_SLOW_GLANCE_R:
		case S_KART_SLOW_LOOK_L:
		case S_KART_SLOW_LOOK_R:
			player->panim = PA_SLOW;
			break;
		case S_KART_FAST:
		case S_KART_FAST_L:
		case S_KART_FAST_R:
		case S_KART_FAST_GLANCE_L:
		case S_KART_FAST_GLANCE_R:
		case S_KART_FAST_LOOK_L:
		case S_KART_FAST_LOOK_R:
			player->panim = PA_FAST;
			break;
		case S_KART_DRIFT_L:
		case S_KART_DRIFT_L_OUT:
		case S_KART_DRIFT_L_IN:
		case S_KART_DRIFT_R:
		case S_KART_DRIFT_R_OUT:
		case S_KART_DRIFT_R_IN:
			player->panim = PA_DRIFT;
			break;
		case S_KART_SPINOUT:
		case S_KART_DEAD:
			player->panim = PA_HURT;
			break;
		default:
			player->panim = PA_ETC;
			break;
	}

	if (recursion++) // if recursion detected,
		memset(seenstate = tempstate, 0, sizeof tempstate); // clear state table

	i = state;

	do
	{
		if (state == S_NULL)
		{ // Bad SOC!
			CONS_Alert(CONS_ERROR, "Cannot remove player mobj by setting its state to S_NULL.\n");
			//P_RemoveMobj(mobj);
			return false;
		}

		st = &states[state];
		mobj->state = st;
		mobj->tics = st->tics;

		// Player animations
		if (st->sprite == SPR_PLAY)
		{
			skin_t *skin = ((skin_t *)mobj->skin);
			UINT16 frame = (mobj->frame & FF_FRAMEMASK)+1;
			UINT8 numframes, spr2;

			if (skin)
			{
				spr2 = P_GetSkinSprite2(skin, st->frame & FF_FRAMEMASK, mobj->player);
				numframes = skin->sprites[spr2].numframes;
			}
			else
			{
				spr2 = 0;
				frame = 0;
				numframes = 0;
			}

			if (mobj->sprite != SPR_PLAY)
			{
				mobj->sprite = SPR_PLAY;
				frame = 0;
			}
			else if (mobj->sprite2 != spr2)
			{
				if ((st->frame & FF_SPR2MIDSTART) && numframes && P_RandomChance(FRACUNIT/2))
					frame = numframes/2;
				else
					frame = 0;
			}

			if (frame >= numframes)
			{
				if (st->frame & FF_SPR2ENDSTATE) // no frame advancement
				{
					if (st->var1 == mobj->state-states)
						frame--;
					else
					{
						if (mobj->frame & FF_FRAMEMASK)
							mobj->frame--;
						return P_SetPlayerMobjState(mobj, st->var1);
					}
				}
				else
					frame = 0;
			}

			mobj->sprite2 = spr2;
			mobj->frame = frame|(st->frame&~FF_FRAMEMASK);
			if (P_PlayerFullbright(player))
				mobj->frame |= FF_FULLBRIGHT;
		}
		// Regular sprites
		else
		{
			mobj->sprite = st->sprite;
			mobj->frame = st->frame;
		}

		P_SetupStateAnimation(mobj, st);

		// Modified handling.
		// Call action functions when the state is set

		if (st->action.acp1)
		{
			var1 = st->var1;
			var2 = st->var2;
			astate = st;
			st->action.acp1(mobj);

			// woah. a player was removed by an action.
			// this sounds like a VERY BAD THING, but there's nothing we can do now...
			if (P_MobjWasRemoved(mobj))
				return false;
		}

		seenstate[state] = 1 + st->nextstate;

		state = st->nextstate;
	} while (!mobj->tics && !seenstate[state]);

	if (!mobj->tics)
		CONS_Alert(CONS_WARNING, M_GetText("State cycle detected, exiting.\n"));

	if (!--recursion)
		for (;(state = seenstate[i]) > S_NULL; i = state - 1)
			seenstate[i] = S_NULL; // erase memory of states

	return true;
}


boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
	state_t *st;

	// remember states seen, to detect cycles:
	static statenum_t seenstate_tab[NUMSTATES]; // fast transition table
	statenum_t *seenstate = seenstate_tab; // pointer to table
	static INT32 recursion; // detects recursion
	statenum_t i = state; // initial state
	statenum_t tempstate[NUMSTATES]; // for use with recursion

#ifdef PARANOIA
	if (mobj->player != NULL)
		I_Error("P_SetMobjState used for player mobj. Use P_SetPlayerMobjState instead!\n(State called: %d)", state);
#endif

	if (recursion++) // if recursion detected,
		memset(seenstate = tempstate, 0, sizeof tempstate); // clear state table

	do
	{
		if (state == S_NULL)
		{
			P_RemoveMobj(mobj);
			return false;
		}

		st = &states[state];
		mobj->state = st;
		mobj->tics = st->tics;

		// Player animations
		if (st->sprite == SPR_PLAY)
		{
			skin_t *skin = ((skin_t *)mobj->skin);
			UINT16 frame = (mobj->frame & FF_FRAMEMASK)+1;
			UINT8 numframes, spr2;

			if (skin)
			{
				spr2 = P_GetSkinSprite2(skin, st->frame & FF_FRAMEMASK, mobj->player);
				numframes = skin->sprites[spr2].numframes;
			}
			else
			{
				spr2 = 0;
				frame = 0;
				numframes = 0;
			}

			if (mobj->sprite != SPR_PLAY)
			{
				mobj->sprite = SPR_PLAY;
				frame = 0;
			}
			else if (mobj->sprite2 != spr2)
			{
				if ((st->frame & FF_SPR2MIDSTART) && numframes && P_RandomChance(FRACUNIT/2))
					frame = numframes/2;
				else
					frame = 0;
			}

			if (frame >= numframes)
			{
				if (st->frame & FF_SPR2ENDSTATE) // no frame advancement
				{
					if (st->var1 == mobj->state-states)
						frame--;
					else
					{
						if (mobj->frame & FF_FRAMEMASK)
							mobj->frame--;
						return P_SetMobjState(mobj, st->var1);
					}
				}
				else
					frame = 0;
			}

			mobj->sprite2 = spr2;
			mobj->frame = frame|(st->frame&~FF_FRAMEMASK);
		}
		// Regular sprites
		else
		{
			mobj->sprite = st->sprite;
			mobj->frame = st->frame;
		}

		P_SetupStateAnimation(mobj, st);

		// Modified handling.
		// Call action functions when the state is set

		if (st->action.acp1)
		{
			var1 = st->var1;
			var2 = st->var2;
			astate = st;
			st->action.acp1(mobj);
			if (P_MobjWasRemoved(mobj))
				return false;
		}

		seenstate[state] = 1 + st->nextstate;

		state = st->nextstate;
	} while (!mobj->tics && !seenstate[state]);

	if (!mobj->tics)
		CONS_Alert(CONS_WARNING, M_GetText("State cycle detected, exiting.\n"));

	if (!--recursion)
		for (;(state = seenstate[i]) > S_NULL; i = state - 1)
			seenstate[i] = S_NULL; // erase memory of states

	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_SetMobjStateNF
//
// Same as P_SetMobjState, but does not call the state function.
//
//----------------------------------------------------------------------------

boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
	state_t *st;

	if (state == S_NULL)
	{ // Remove mobj
		P_RemoveMobj(mobj);
		return false;
	}
	st = &states[state];
	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;
	P_SetupStateAnimation(mobj, st);

	return true;
}

static boolean P_SetPrecipMobjState(precipmobj_t *mobj, statenum_t state)
{
	state_t *st;

	if (state == S_NULL)
	{ // Remove mobj
		P_FreePrecipMobj(mobj);
		return false;
	}
	st = &states[state];
	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;
	P_SetupStateAnimation((mobj_t*)mobj, st);

	return true;
}

//
// P_MobjFlip
//
// Special utility to return +1 or -1 depending on mobj's gravity
//
SINT8 P_MobjFlip(mobj_t *mobj)
{
	if (mobj && mobj->eflags & MFE_VERTICALFLIP)
		return -1;
	return 1;
}

//
// P_ExplodeMissile
//
void P_ExplodeMissile(mobj_t *mo)
{
	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	mo->momx = mo->momy = mo->momz = 0;

	if (mo->flags & MF_NOCLIPTHING)
		return;

	mo->flags &= ~MF_MISSILE;

	mo->flags |= MF_NOGRAVITY; // Dead missiles don't need to sink anymore.
	mo->flags |= MF_NOCLIPTHING; // Dummy flag to indicate that this was already called.

	if (mo->info->deathsound && !(mo->flags2 & MF2_DEBRIS))
		S_StartSound(mo, mo->info->deathsound);

	P_SetMobjState(mo, mo->info->deathstate);
}

// P_InsideANonSolidFFloor
//
// Returns TRUE if mobj is inside a non-solid 3d floor.
boolean P_InsideANonSolidFFloor(mobj_t *mobj, ffloor_t *rover)
{
	fixed_t topheight, bottomheight;
	if (!(rover->fofflags & FOF_EXISTS))
		return false;

	if ((((rover->fofflags & FOF_BLOCKPLAYER) && mobj->player)
		|| ((rover->fofflags & FOF_BLOCKOTHERS) && !mobj->player)))
		return false;

	topheight    = P_GetFFloorTopZAt   (rover, mobj->x, mobj->y);
	bottomheight = P_GetFFloorBottomZAt(rover, mobj->x, mobj->y);

	if (mobj->z > topheight)
		return false;

	if (mobj->z + mobj->height < bottomheight)
		return false;

	return true;
}

// P_GetFloorZ (and its ceiling counterpart)
// Gets the floor height (or ceiling height) of the mobj's contact point in sector, assuming object's center if moved to [x, y]
// If line is supplied, it's a divider line on the sector. Set it to NULL if you're not checking for collision with a line
// Supply boundsec ONLY when checking for specials! It should be the "in-level" sector, and sector the control sector (if separate).
// If set, then this function will iterate through boundsec's linedefs to find the highest contact point on the slope. Non-special-checking
// usage will handle that later.
static fixed_t HighestOnLine(fixed_t radius, fixed_t x, fixed_t y, line_t *line, pslope_t *slope, boolean actuallylowest)
{
	// Alright, so we're sitting on a line that contains our slope sector, and need to figure out the highest point we're touching...
	// The solution is simple! Get the line's vertices, and pull each one in along its line until it touches the object's bounding box
	// (assuming it isn't already inside), then test each point's slope Z and return the higher of the two.
	vertex_t v1, v2;
	v1.x = line->v1->x;
	v1.y = line->v1->y;
	v2.x = line->v2->x;
	v2.y = line->v2->y;

	/*CONS_Printf("BEFORE: v1 = %f %f %f\n",
				FIXED_TO_FLOAT(v1.x),
				FIXED_TO_FLOAT(v1.y),
				FIXED_TO_FLOAT(P_GetSlopeZAt(slope, v1.x, v1.y))
				);
	CONS_Printf("        v2 = %f %f %f\n",
				FIXED_TO_FLOAT(v2.x),
				FIXED_TO_FLOAT(v2.y),
				FIXED_TO_FLOAT(P_GetSlopeZAt(slope, v2.x, v2.y))
				);*/

	if (abs(v1.x-x) > radius) {
		// v1's x is out of range, so rein it in
		fixed_t diff = abs(v1.x-x) - radius;

		if (v1.x < x) { // Moving right
			v1.x += diff;
			v1.y += FixedMul(diff, FixedDiv(line->dy, line->dx));
		} else { // Moving left
			v1.x -= diff;
			v1.y -= FixedMul(diff, FixedDiv(line->dy, line->dx));
		}
	}

	if (abs(v1.y-y) > radius) {
		// v1's y is out of range, so rein it in
		fixed_t diff = abs(v1.y-y) - radius;

		if (v1.y < y) { // Moving up
			v1.y += diff;
			v1.x += FixedMul(diff, FixedDiv(line->dx, line->dy));
		} else { // Moving down
			v1.y -= diff;
			v1.x -= FixedMul(diff, FixedDiv(line->dx, line->dy));
		}
	}

	if (abs(v2.x-x) > radius) {
		// v1's x is out of range, so rein it in
		fixed_t diff = abs(v2.x-x) - radius;

		if (v2.x < x) { // Moving right
			v2.x += diff;
			v2.y += FixedMul(diff, FixedDiv(line->dy, line->dx));
		} else { // Moving left
			v2.x -= diff;
			v2.y -= FixedMul(diff, FixedDiv(line->dy, line->dx));
		}
	}

	if (abs(v2.y-y) > radius) {
		// v2's y is out of range, so rein it in
		fixed_t diff = abs(v2.y-y) - radius;

		if (v2.y < y) { // Moving up
			v2.y += diff;
			v2.x += FixedMul(diff, FixedDiv(line->dx, line->dy));
		} else { // Moving down
			v2.y -= diff;
			v2.x -= FixedMul(diff, FixedDiv(line->dx, line->dy));
		}
	}

	/*CONS_Printf("AFTER:  v1 = %f %f %f\n",
				FIXED_TO_FLOAT(v1.x),
				FIXED_TO_FLOAT(v1.y),
				FIXED_TO_FLOAT(P_GetSlopeZAt(slope, v1.x, v1.y))
				);
	CONS_Printf("        v2 = %f %f %f\n",
				FIXED_TO_FLOAT(v2.x),
				FIXED_TO_FLOAT(v2.y),
				FIXED_TO_FLOAT(P_GetSlopeZAt(slope, v2.x, v2.y))
				);*/

	// Return the higher of the two points
	if (actuallylowest)
		return min(
			P_GetSlopeZAt(slope, v1.x, v1.y),
			P_GetSlopeZAt(slope, v2.x, v2.y)
		);
	else
		return max(
			P_GetSlopeZAt(slope, v1.x, v1.y),
			P_GetSlopeZAt(slope, v2.x, v2.y)
		);
}

fixed_t P_MobjFloorZ(mobj_t *mobj, sector_t *sector, sector_t *boundsec, fixed_t x, fixed_t y, line_t *line, boolean lowest, boolean perfect)
{
	I_Assert(mobj != NULL);
	I_Assert(sector != NULL);

	if (sector->f_slope) {
		fixed_t testx, testy;
		pslope_t *slope = sector->f_slope;

		// Get the corner of the object that should be the highest on the slope
		if (slope->d.x < 0)
			testx = mobj->radius;
		else
			testx = -mobj->radius;

		if (slope->d.y < 0)
			testy = mobj->radius;
		else
			testy = -mobj->radius;

		if ((slope->zdelta > 0) ^ !!(lowest)) {
			testx = -testx;
			testy = -testy;
		}

		testx += x;
		testy += y;

		// If the highest point is in the sector, then we have it easy! Just get the Z at that point
		if (R_PointInSubsector(testx, testy)->sector == (boundsec ? boundsec : sector))
			return P_GetSlopeZAt(slope, testx, testy);

		// If boundsec is set, we're looking for specials. In that case, iterate over every line in this sector to find the TRUE highest/lowest point
		if (perfect) {
			size_t i;
			line_t *ld;
			fixed_t bbox[4];
			fixed_t finalheight;

			if (lowest)
				finalheight = INT32_MAX;
			else
				finalheight = INT32_MIN;

			bbox[BOXLEFT] = x-mobj->radius;
			bbox[BOXRIGHT] = x+mobj->radius;
			bbox[BOXTOP] = y+mobj->radius;
			bbox[BOXBOTTOM] = y-mobj->radius;
			for (i = 0; i < boundsec->linecount; i++) {
				ld = boundsec->lines[i];

				if (bbox[BOXRIGHT] <= ld->bbox[BOXLEFT] || bbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
				|| bbox[BOXTOP] <= ld->bbox[BOXBOTTOM] || bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
					continue;

				if (P_BoxOnLineSide(bbox, ld) != -1)
					continue;

				if (lowest)
					finalheight = min(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, true));
				else
					finalheight = max(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, false));
			}

			return finalheight;
		}

		// If we're just testing for base sector location (no collision line), just go for the center's spot...
		// It'll get fixed when we test for collision anyway, and the final result can't be lower than this
		if (line == NULL)
			return P_GetSlopeZAt(slope, x, y);

		return HighestOnLine(mobj->radius, x, y, line, slope, lowest);
	} else // Well, that makes it easy. Just get the floor height
		return sector->floorheight;
}

fixed_t P_MobjCeilingZ(mobj_t *mobj, sector_t *sector, sector_t *boundsec, fixed_t x, fixed_t y, line_t *line, boolean lowest, boolean perfect)
{
	I_Assert(mobj != NULL);
	I_Assert(sector != NULL);

	if (sector->c_slope) {
		fixed_t testx, testy;
		pslope_t *slope = sector->c_slope;

		// Get the corner of the object that should be the highest on the slope
		if (slope->d.x < 0)
			testx = mobj->radius;
		else
			testx = -mobj->radius;

		if (slope->d.y < 0)
			testy = mobj->radius;
		else
			testy = -mobj->radius;

		if ((slope->zdelta > 0) ^ !!(lowest)) {
			testx = -testx;
			testy = -testy;
		}

		testx += x;
		testy += y;

		// If the highest point is in the sector, then we have it easy! Just get the Z at that point
		if (R_PointInSubsector(testx, testy)->sector == (boundsec ? boundsec : sector))
			return P_GetSlopeZAt(slope, testx, testy);

		// If boundsec is set, we're looking for specials. In that case, iterate over every line in this sector to find the TRUE highest/lowest point
		if (perfect) {
			size_t i;
			line_t *ld;
			fixed_t bbox[4];
			fixed_t finalheight;

			if (lowest)
				finalheight = INT32_MAX;
			else
				finalheight = INT32_MIN;

			bbox[BOXLEFT] = x-mobj->radius;
			bbox[BOXRIGHT] = x+mobj->radius;
			bbox[BOXTOP] = y+mobj->radius;
			bbox[BOXBOTTOM] = y-mobj->radius;
			for (i = 0; i < boundsec->linecount; i++) {
				ld = boundsec->lines[i];

				if (bbox[BOXRIGHT] <= ld->bbox[BOXLEFT] || bbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
				|| bbox[BOXTOP] <= ld->bbox[BOXBOTTOM] || bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
					continue;

				if (P_BoxOnLineSide(bbox, ld) != -1)
					continue;

				if (lowest)
					finalheight = min(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, true));
				else
					finalheight = max(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, false));
			}

			return finalheight;
		}

		// If we're just testing for base sector location (no collision line), just go for the center's spot...
		// It'll get fixed when we test for collision anyway, and the final result can't be lower than this
		if (line == NULL)
			return P_GetSlopeZAt(slope, x, y);

		return HighestOnLine(mobj->radius, x, y, line, slope, lowest);
	} else // Well, that makes it easy. Just get the ceiling height
		return sector->ceilingheight;
}

// Now do the same as all above, but for cameras because apparently cameras are special?
fixed_t P_CameraFloorZ(camera_t *mobj, sector_t *sector, sector_t *boundsec, fixed_t x, fixed_t y, line_t *line, boolean lowest, boolean perfect)
{
	I_Assert(mobj != NULL);
	I_Assert(sector != NULL);

	if (sector->f_slope) {
		fixed_t testx, testy;
		pslope_t *slope = sector->f_slope;

		// Get the corner of the object that should be the highest on the slope
		if (slope->d.x < 0)
			testx = mobj->radius;
		else
			testx = -mobj->radius;

		if (slope->d.y < 0)
			testy = mobj->radius;
		else
			testy = -mobj->radius;

		if ((slope->zdelta > 0) ^ !!(lowest)) {
			testx = -testx;
			testy = -testy;
		}

		testx += x;
		testy += y;

		// If the highest point is in the sector, then we have it easy! Just get the Z at that point
		if (R_PointInSubsector(testx, testy)->sector == (boundsec ? boundsec : sector))
			return P_GetSlopeZAt(slope, testx, testy);

		// If boundsec is set, we're looking for specials. In that case, iterate over every line in this sector to find the TRUE highest/lowest point
		if (perfect) {
			size_t i;
			line_t *ld;
			fixed_t bbox[4];
			fixed_t finalheight;

			if (lowest)
				finalheight = INT32_MAX;
			else
				finalheight = INT32_MIN;

			bbox[BOXLEFT] = x-mobj->radius;
			bbox[BOXRIGHT] = x+mobj->radius;
			bbox[BOXTOP] = y+mobj->radius;
			bbox[BOXBOTTOM] = y-mobj->radius;
			for (i = 0; i < boundsec->linecount; i++) {
				ld = boundsec->lines[i];

				if (bbox[BOXRIGHT] <= ld->bbox[BOXLEFT] || bbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
				|| bbox[BOXTOP] <= ld->bbox[BOXBOTTOM] || bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
					continue;

				if (P_BoxOnLineSide(bbox, ld) != -1)
					continue;

				if (lowest)
					finalheight = min(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, true));
				else
					finalheight = max(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, false));
			}

			return finalheight;
		}

		// If we're just testing for base sector location (no collision line), just go for the center's spot...
		// It'll get fixed when we test for collision anyway, and the final result can't be lower than this
		if (line == NULL)
			return P_GetSlopeZAt(slope, x, y);

		return HighestOnLine(mobj->radius, x, y, line, slope, lowest);
	} else // Well, that makes it easy. Just get the floor height
		return sector->floorheight;
}

fixed_t P_CameraCeilingZ(camera_t *mobj, sector_t *sector, sector_t *boundsec, fixed_t x, fixed_t y, line_t *line, boolean lowest, boolean perfect)
{
	I_Assert(mobj != NULL);
	I_Assert(sector != NULL);

	if (sector->c_slope) {
		fixed_t testx, testy;
		pslope_t *slope = sector->c_slope;

		// Get the corner of the object that should be the highest on the slope
		if (slope->d.x < 0)
			testx = mobj->radius;
		else
			testx = -mobj->radius;

		if (slope->d.y < 0)
			testy = mobj->radius;
		else
			testy = -mobj->radius;

		if ((slope->zdelta > 0) ^ !!(lowest)) {
			testx = -testx;
			testy = -testy;
		}

		testx += x;
		testy += y;

		// If the highest point is in the sector, then we have it easy! Just get the Z at that point
		if (R_PointInSubsector(testx, testy)->sector == (boundsec ? boundsec : sector))
			return P_GetSlopeZAt(slope, testx, testy);

		// If boundsec is set, we're looking for specials. In that case, iterate over every line in this sector to find the TRUE highest/lowest point
		if (perfect) {
			size_t i;
			line_t *ld;
			fixed_t bbox[4];
			fixed_t finalheight;

			if (lowest)
				finalheight = INT32_MAX;
			else
				finalheight = INT32_MIN;

			bbox[BOXLEFT] = x-mobj->radius;
			bbox[BOXRIGHT] = x+mobj->radius;
			bbox[BOXTOP] = y+mobj->radius;
			bbox[BOXBOTTOM] = y-mobj->radius;
			for (i = 0; i < boundsec->linecount; i++) {
				ld = boundsec->lines[i];

				if (bbox[BOXRIGHT] <= ld->bbox[BOXLEFT] || bbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
				|| bbox[BOXTOP] <= ld->bbox[BOXBOTTOM] || bbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
					continue;

				if (P_BoxOnLineSide(bbox, ld) != -1)
					continue;

				if (lowest)
					finalheight = min(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, true));
				else
					finalheight = max(finalheight, HighestOnLine(mobj->radius, x, y, ld, slope, false));
			}

			return finalheight;
		}

		// If we're just testing for base sector location (no collision line), just go for the center's spot...
		// It'll get fixed when we test for collision anyway, and the final result can't be lower than this
		if (line == NULL)
			return P_GetSlopeZAt(slope, x, y);

		return HighestOnLine(mobj->radius, x, y, line, slope, lowest);
	} else // Well, that makes it easy. Just get the ceiling height
		return sector->ceilingheight;
}
static void P_PlayerFlip(mobj_t *mo)
{
	if (!mo->player)
		return;
	
	if (mo->player->pflags & PF_FLIPCAM)
	{
		UINT8 i;

		mo->player->aiming = InvAngle(mo->player->aiming);

		for (i = 0; i <= r_splitscreen; i++)
		{
			if (mo->player-players == displayplayers[i])
			{
				localaiming[i] = mo->player->aiming;
				if (camera[i].chase) {
					camera[i].aiming = InvAngle(camera[i].aiming);
					camera[i].z = mo->z - camera[i].z + mo->z;
					if (mo->eflags & MFE_VERTICALFLIP)
						camera[i].z += FixedMul(20*FRACUNIT, mo->scale);
				}
			}
		}
	}

	G_GhostAddFlip((INT32) (mo->player - players));
	// Flip aiming to match!
}

//
// P_GetMobjGravity
//
// Returns the current gravity
// value of the object.
//
fixed_t P_GetMobjGravity(mobj_t *mo)
{
	fixed_t gravityadd = 0;
	boolean no3dfloorgrav = true; // Custom gravity
	boolean goopgravity = false;
	boolean wasflip;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	wasflip = (mo->eflags & MFE_VERTICALFLIP) != 0;

	if (mo->type != MT_SPINFIRE)
		mo->eflags &= ~MFE_VERTICALFLIP;

	if (mo->subsector->sector->ffloors) // Check for 3D floor gravity too.
	{
		ffloor_t *rover;
		fixed_t gravfactor;

		for (rover = mo->subsector->sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS) || !P_InsideANonSolidFFloor(mo, rover)) // P_InsideANonSolidFFloor checks for FOF_EXISTS itself, but let's not always call this function
				continue;

			if ((rover->fofflags & (FOF_SWIMMABLE|FOF_GOOWATER)) == (FOF_SWIMMABLE|FOF_GOOWATER))
				goopgravity = true;

			gravfactor = P_GetSectorGravityFactor(rover->master->frontsector);

			if (gravfactor == FRACUNIT)
				continue;

			gravityadd = -FixedMul(gravity, gravfactor);

			if ((rover->master->frontsector->flags & MSF_GRAVITYFLIP) && gravityadd > 0)
				mo->eflags |= MFE_VERTICALFLIP;

			no3dfloorgrav = false;
			break;
		}
	}

	if (no3dfloorgrav)
	{
		gravityadd = -FixedMul(gravity, P_GetSectorGravityFactor(mo->subsector->sector));

		if ((mo->subsector->sector->flags & MSF_GRAVITYFLIP) && gravityadd > 0)
			mo->eflags |= MFE_VERTICALFLIP;
	}

	// Less gravity underwater.
	if ((mo->eflags & MFE_UNDERWATER) && !goopgravity)
	{
		if (mo->momz * P_MobjFlip(mo) <= 0)
		{
			gravityadd = 4*gravityadd/3;
		}
		else
		{
			gravityadd = gravityadd/3;
		}
	}

	if (mo->player)
	{
		if (mo->flags2 & MF2_OBJECTFLIP)
		{
			gravityadd = -gravityadd;
			mo->eflags ^= MFE_VERTICALFLIP;
		}

		if (wasflip == !(mo->eflags & MFE_VERTICALFLIP)) // note!! == ! is not equivalent to != here - turns numeric into bool this way
		{
			P_PlayerFlip(mo);
		}

		if (mo->player->waterskip)
		{
			gravityadd = (4*gravityadd)/3;
		}

		if (mo->player->pogospring)
		{
			gravityadd = (5*gravityadd)/2;
		}
	}
	else
	{
		// Objects with permanent reverse gravity (MF2_OBJECTFLIP)
		if (mo->flags2 & MF2_OBJECTFLIP)
		{
			mo->eflags |= MFE_VERTICALFLIP;

			if (mo->z + mo->height >= mo->ceilingz)
			{
				gravityadd = 0;
			}
			else if (gravityadd < 0) // Don't sink, only rise up
			{
				gravityadd = -gravityadd;
			}
		}
		else //Otherwise, sort through the other exceptions.
		{
			switch (mo->type)
			{
				case MT_FLINGRING:
				case MT_FLINGCOIN:
				case MT_FLINGBLUESPHERE:
				case MT_FLINGNIGHTSCHIP:
				case MT_BOUNCERING:
				case MT_RAILRING:
				case MT_INFINITYRING:
				case MT_AUTOMATICRING:
				case MT_EXPLOSIONRING:
				case MT_SCATTERRING:
				case MT_GRENADERING:
				case MT_BOUNCEPICKUP:
				case MT_RAILPICKUP:
				case MT_AUTOPICKUP:
				case MT_EXPLODEPICKUP:
				case MT_SCATTERPICKUP:
				case MT_GRENADEPICKUP:
				case MT_REDFLAG:
				case MT_BLUEFLAG:
					if (mo->target)
					{
						// Flung items copy the gravity of their tosser.
						if ((mo->target->eflags & MFE_VERTICALFLIP) && !(mo->eflags & MFE_VERTICALFLIP))
						{
							gravityadd = -gravityadd;
							mo->eflags |= MFE_VERTICALFLIP;
						}
					}
					break;
				case MT_WATERDROP:
				case MT_BATTLEBUMPER:
					gravityadd /= 2;
					break;
				case MT_BANANA:
				case MT_EGGMANITEM:
				case MT_SSMINE:
				case MT_LANDMINE:
				case MT_DROPTARGET:
				case MT_SINK:
				case MT_EMERALD:
					if (mo->extravalue2 > 0)
					{
						gravityadd *= mo->extravalue2;
					}

					gravityadd = (5*gravityadd)/2;
					break;
				case MT_ORBINAUT:
				case MT_JAWZ:
				case MT_JAWZ_DUD:
					gravityadd = (5*gravityadd)/2;
					break;
				case MT_SIGN:
					gravityadd /= 8;
					break;
				case MT_KARMAFIREWORK:
					gravityadd /= 3;
					break;
				default:
					break;
			}
		}
	}

	// Goop has slower, reversed gravity
	if (goopgravity)
	{
		gravityadd = -((gravityadd/5) + (gravityadd/8));
	}

	gravityadd = FixedMul(gravityadd, mapobjectscale);

	return gravityadd;
}

//
// P_CheckGravity
//
// Checks the current gravity state
// of the object. If affect is true,
// a gravity force will be applied.
//
void P_CheckGravity(mobj_t *mo, boolean affect)
{
	fixed_t gravityadd = P_GetMobjGravity(mo);

	if (!mo->momz) // mobj at stop, no floor, so feel the push of gravity!
		gravityadd <<= 1;

	if (affect)
		mo->momz += gravityadd;

	if (mo->type == MT_SKIM && mo->z + mo->momz <= mo->watertop && mo->z >= mo->watertop)
	{
		mo->momz = 0;
		mo->flags |= MF_NOGRAVITY;
	}
}

//
// P_SetPitchRollFromSlope
//
void P_SetPitchRollFromSlope(mobj_t *mo, pslope_t *slope)
{
	if (slope)
	{
		fixed_t tempz = slope->normal.z;
		fixed_t tempy = slope->normal.y;
		fixed_t tempx = slope->normal.x;

		mo->pitch = R_PointToAngle2(0, 0, FixedSqrt(FixedMul(tempy, tempy) + FixedMul(tempz, tempz)), tempx);
		mo->roll = R_PointToAngle2(0, 0, tempz, tempy);
	}
	else
	{
		mo->pitch = mo->roll = 0;
	}
}

//
// P_SetPitchRoll
//
void P_SetPitchRoll(mobj_t *mo, angle_t pitch, angle_t yaw)
{
	pitch = InvAngle(pitch);
	yaw >>= ANGLETOFINESHIFT;
	mo->roll  = FixedMul(pitch, FINESINE   (yaw));
	mo->pitch = FixedMul(pitch, FINECOSINE (yaw));
}

#define STOPSPEED (FRACUNIT)

//
// P_SceneryXYFriction
//
static void P_SceneryXYFriction(mobj_t *mo, fixed_t oldx, fixed_t oldy)
{
	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (abs(mo->momx) < FixedMul(STOPSPEED/32, mo->scale)
		&& abs(mo->momy) < FixedMul(STOPSPEED/32, mo->scale))
	{
		mo->momx = 0;
		mo->momy = 0;
	}
	else
	{
		if ((oldx == mo->x) && (oldy == mo->y)) // didn't go anywhere
		{
			mo->momx = FixedMul(mo->momx,ORIG_FRICTION);
			mo->momy = FixedMul(mo->momy,ORIG_FRICTION);
		}
		else
		{
			mo->momx = FixedMul(mo->momx,mo->friction);
			mo->momy = FixedMul(mo->momy,mo->friction);
		}

		if (mo->type == MT_CANNONBALLDECOR)
		{
			// Stolen from P_SpawnFriction
			mo->friction = FRACUNIT - 0x100;
			//mo->movefactor = ((0x10092 - mo->friction)*(0x70))/0x158;
		}
		else
			mo->friction = ORIG_FRICTION;
	}
}

//
// P_XYFriction
//
// adds friction on the xy plane
//
static void P_XYFriction(mobj_t *mo, fixed_t oldx, fixed_t oldy)
{
	player_t *player;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	player = mo->player;
	if (player) // valid only if player avatar
	{
		if (abs(player->rmomx) < FixedMul(STOPSPEED, mo->scale)
		    && abs(player->rmomy) < FixedMul(STOPSPEED, mo->scale)
		    && (!(player->cmd.forwardmove && !player->cmd.sidemove))
			&& !(player->mo->standingslope && (!(player->mo->standingslope->flags & SL_NOPHYSICS)) && (abs(player->mo->standingslope->zdelta) >= FRACUNIT/2))
				)
		{
			// if in a walking frame, stop moving
			if (player->panim == PA_SLOW)
			{
				P_SetPlayerMobjState(mo, S_KART_STILL);
			}
			mo->momx = player->cmomx;
			mo->momy = player->cmomy;
		}
		else
		{
			if (oldx == mo->x && oldy == mo->y) // didn't go anywhere
			{
				mo->momx = FixedMul(mo->momx, ORIG_FRICTION);
				mo->momy = FixedMul(mo->momy, ORIG_FRICTION);
			}
			else
			{
				mo->momx = FixedMul(mo->momx, mo->friction);
				mo->momy = FixedMul(mo->momy, mo->friction);
			}

			mo->friction = ORIG_FRICTION;
		}
	}
	else
		P_SceneryXYFriction(mo, oldx, oldy);
}

static void P_PushableCheckBustables(mobj_t *mo)
{
	msecnode_t *node;
	fixed_t oldx;
	fixed_t oldy;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (netgame && mo->player && mo->player->spectator)
		return;

	oldx = mo->x;
	oldy = mo->y;

	P_UnsetThingPosition(mo);
	mo->x += mo->momx;
	mo->y += mo->momy;
	P_SetThingPosition(mo);

	for (node = mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		ffloor_t *rover;
		fixed_t topheight, bottomheight;

		if (!node->m_sector)
			break;

		if (!node->m_sector->ffloors)
			continue;

		for (rover = node->m_sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (!(rover->fofflags & FOF_BUSTUP))
				continue;

			if (!(rover->bustflags & FB_PUSHABLES))
				continue;

			if (rover->master->frontsector->crumblestate != CRUMBLE_NONE)
				continue;

			topheight = P_GetFOFTopZ(mo, node->m_sector, rover, mo->x, mo->y, NULL);
			bottomheight = P_GetFOFBottomZ(mo, node->m_sector, rover, mo->x, mo->y, NULL);

			// Height checks
			if (rover->bustflags & FB_ONLYBOTTOM)
			{
				if (mo->z + mo->momz + mo->height < bottomheight)
					continue;

				if (mo->z + mo->height > bottomheight)
					continue;
			}
			else
			{
				switch (rover->busttype)
				{
				case BT_TOUCH:
					if (mo->z + mo->momz > topheight)
						continue;

					if (mo->z + mo->momz + mo->height < bottomheight)
						continue;

					break;
				case BT_SPINBUST:
					if (mo->z + mo->momz > topheight)
						continue;

					if (mo->z + mo->height < bottomheight)
						continue;

					break;
				default:
					if (mo->z >= topheight)
						continue;

					if (mo->z + mo->height < bottomheight)
						continue;

					break;
				}
			}

			EV_CrumbleChain(NULL, rover); // node->m_sector

			// Run a linedef executor??
			if (rover->bustflags & FB_EXECUTOR)
				P_LinedefExecute(rover->busttag, mo, node->m_sector);

			goto bustupdone;
		}
	}
bustupdone:
	P_UnsetThingPosition(mo);
	mo->x = oldx;
	mo->y = oldy;
	P_SetThingPosition(mo);
}

//
// P_CheckSkyHit
//
static boolean P_CheckSkyHit(mobj_t *mo)
{
	if (ceilingline && ceilingline->backsector
		&& ceilingline->backsector->ceilingpic == skyflatnum
		&& ceilingline->frontsector
		&& ceilingline->frontsector->ceilingpic == skyflatnum
		&& (mo->z >= ceilingline->frontsector->ceilingheight
		|| mo->z >= ceilingline->backsector->ceilingheight))
			return true;
	return false;
}

//
// P_XYMovement
//
void P_XYMovement(mobj_t *mo)
{
	player_t *player;
	fixed_t xmove, ymove;
	fixed_t oldx, oldy; // reducing bobbing/momentum on ice when up against walls
	boolean moved;
	pslope_t *oldslope = NULL;
	vector3_t slopemom = {0,0,0};
	fixed_t predictedz = 0;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	// if it's stopped
	if (!mo->momx && !mo->momy)
	{
		if (mo->flags2 & MF2_SKULLFLY)
		{
			// the skull slammed into something
			mo->flags2 &= ~MF2_SKULLFLY;
			mo->momx = mo->momy = mo->momz = 0;

			// set in 'search new direction' state?
			if (mo->type != MT_EGGMOBILE)
				P_SetMobjState(mo, mo->info->spawnstate);

			return;
		}
	}

	player = mo->player; //valid only if player avatar

	xmove = mo->momx;
	ymove = mo->momy;

	oldx = mo->x;
	oldy = mo->y;

	if (mo->flags & MF_NOCLIPHEIGHT)
	{
		mo->standingslope = NULL;
		mo->terrain = NULL;
	}

	// adjust various things based on slope
	if (mo->standingslope && abs(mo->standingslope->zdelta) > FRACUNIT>>8) {
		if (!P_IsObjectOnGround(mo)) { // We fell off at some point? Do the twisty thing!
			P_SlopeLaunch(mo);
			xmove = mo->momx;
			ymove = mo->momy;
		} else { // Still on the ground.
			slopemom.x = xmove;
			slopemom.y = ymove;
			slopemom.z = 0;
			P_QuantizeMomentumToSlope(&slopemom, mo->standingslope);

			xmove = slopemom.x;
			ymove = slopemom.y;

			predictedz = mo->z + slopemom.z; // We'll use this later...

			oldslope = mo->standingslope;
		}
	} else if (P_IsObjectOnGround(mo) && !mo->momz)
		predictedz = mo->z;

	// Pushables can break some blocks
	if (CheckForBustableBlocks && ((mo->flags & MF_PUSHABLE) || ((mo->info->flags & MF_PUSHABLE) && mo->fuse)))
		P_PushableCheckBustables(mo);

	//{ SRB2kart - Ballhogs
	if (mo->type == MT_BALLHOG)
	{
		if (mo->health)
		{
			mo->health--;
			if (mo->health == 0)
				mo->destscale = 0;
		}
	}
	//}

	if (!P_TryMove(mo, mo->x + xmove, mo->y + ymove, true)
		&& !(P_MobjWasRemoved(mo) || mo->eflags & MFE_SPRUNG))
	{
		// blocked move
		moved = false;

		if (LUA_HookMobjMoveBlocked(mo, tmhitthing, blockingline))
		{
			if (P_MobjWasRemoved(mo))
				return;
		}
		else if (P_MobjWasRemoved(mo))
			return;
		
		P_PushSpecialLine(blockingline, mo);

		//{ SRB2kart - Jawz
		if (mo->type == MT_JAWZ || mo->type == MT_JAWZ_DUD)
		{
			if (mo->health == 1)
			{
				// This Item Damage
				S_StartSound(mo, mo->info->deathsound);
				P_KillMobj(mo, NULL, NULL, DMG_NORMAL);

				P_SetObjectMomZ(mo, 8*FRACUNIT, false);
				P_InstaThrust(mo, R_PointToAngle2(mo->x, mo->y, mo->x + xmove, mo->y + ymove)+ANGLE_90, 16*FRACUNIT);
			}
		}
		//}
		else if (mo->flags & MF_BOUNCE)
		{
			P_BounceMove(mo);
			if (P_MobjWasRemoved(mo))
				return;
			xmove = ymove = 0;
			S_StartSound(mo, mo->info->activesound);

			//{ SRB2kart - Orbinaut, Ballhog
			// Ballhog dies on contact with walls
			if (mo->type == MT_ORBINAUT || mo->type == MT_BALLHOG)
			{
				mobj_t *fx;
				fx = P_SpawnMobj(mo->x, mo->y, mo->z, MT_BUMP);
				if (mo->eflags & MFE_VERTICALFLIP)
					fx->eflags |= MFE_VERTICALFLIP;
				else
					fx->eflags &= ~MFE_VERTICALFLIP;
				fx->scale = mo->scale;
			}

			switch (mo->type)
			{
				case MT_ORBINAUT: // Orbinaut speed decreasing
					if (mo->health > 1)
					{
						S_StartSound(mo, mo->info->attacksound);
						mo->health--;
						// This prevents an item thrown at a wall from
						// phasing through you on its return.
						mo->threshold = 0;
					}
					/*FALLTHRU*/

				case MT_JAWZ:
					if (mo->health == 1)
					{
						// This Item Damage
						S_StartSound(mo, mo->info->deathsound);
						P_KillMobj(mo, NULL, NULL, DMG_NORMAL);

						P_SetObjectMomZ(mo, 8*FRACUNIT, false);
						P_InstaThrust(mo, R_PointToAngle2(mo->x, mo->y, mo->x + xmove, mo->y + ymove)+ANGLE_90, 16*FRACUNIT);
					}
					break;

				case MT_BUBBLESHIELDTRAP:
					S_StartSound(mo, sfx_s3k44); // Bubble bounce
					break;

				case MT_DROPTARGET:
					// This prevents an item thrown at a wall from
					// phasing through you on its return.
					mo->threshold = 0;
					break;

				default:
					break;
			}
		}
		else if (mo->flags & MF_MISSILE)
		{
			// explode a missile
			if (P_CheckSkyHit(mo))
			{
				// Hack to prevent missiles exploding
				// against the sky.
				// Does not handle sky floors.
				// Check frontsector as well.

				P_RemoveMobj(mo);
				return;
			}

			// draw damage on wall
			//SPLAT TEST ----------------------------------------------------------
#ifdef WALLSPLATS
			if (blockingline && mo->type != MT_REDRING && mo->type != MT_FIREBALL
			&& !(mo->flags2 & (MF2_AUTOMATIC|MF2_RAILRING|MF2_BOUNCERING|MF2_EXPLOSION|MF2_SCATTER)))
				// set by last P_TryMove() that failed
			{
				divline_t divl;
				divline_t misl;
				fixed_t frac;

				P_MakeDivline(blockingline, &divl);
				misl.x = mo->x;
				misl.y = mo->y;
				misl.dx = mo->momx;
				misl.dy = mo->momy;
				frac = P_InterceptVector(&divl, &misl);
				R_AddWallSplat(blockingline, P_PointOnLineSide(mo->x,mo->y,blockingline),
					"A_DMG3", mo->z, frac, SPLATDRAWMODE_SHADE);
			}
#endif
			// --------------------------------------------------------- SPLAT TEST

			P_ExplodeMissile(mo);
			return;
		}
		else if (mo->flags & MF_STICKY)
		{
			S_StartSound(mo, mo->info->activesound);
			mo->momx = mo->momy = mo->momz = 0; //Full stop!
			mo->flags |= MF_NOGRAVITY; //Stay there!
			mo->flags &= ~MF_STICKY; //Don't check again!

			// Check for hit against sky here
			if (P_CheckSkyHit(mo))
			{
				// Hack to prevent missiles exploding
				// against the sky.
				// Does not handle sky floors.
				// Check frontsector as well.

				P_RemoveMobj(mo);
				return;
			}
		}
		if (mo->flags & MF_SLIDEME|MF_PUSHABLE)
		{
			P_SlideMove(mo);
			if (P_MobjWasRemoved(mo))
				return;
			xmove = ymove = 0;
		}
		//
	}
	else
		moved = true;

	if (P_MobjWasRemoved(mo)) // MF_SPECIAL touched a player! O_o;;
		return;

	if (moved == true)
	{
		// TERRAIN footstep effects.
		K_HandleFootstepParticles(mo);
	}

	if (moved && oldslope && !(mo->flags & MF_NOCLIPHEIGHT))
	{
		// Check to see if we ran off
		if (oldslope != mo->standingslope)
		{
			// First, compare different slopes
			angle_t oldangle, newangle;
			angle_t moveangle = K_MomentumAngle(mo);

			oldangle = FixedMul((signed)oldslope->zangle, FINECOSINE((moveangle - oldslope->xydirection) >> ANGLETOFINESHIFT));

			if (mo->standingslope)
				newangle = FixedMul((signed)mo->standingslope->zangle, FINECOSINE((moveangle - mo->standingslope->xydirection) >> ANGLETOFINESHIFT));
			else
				newangle = 0;

			// Now compare the Zs of the different quantizations
			if (oldangle-newangle > ANG30 && oldangle-newangle < ANGLE_180)
			{
				// Allow for a bit of sticking - this value can be adjusted later
				mo->standingslope = oldslope;
				P_SetPitchRollFromSlope(mo, mo->standingslope);
				P_SlopeLaunch(mo);

				//CONS_Printf("launched off of slope - ");
			}

			/*
			CONS_Printf("old angle %f - new angle %f = %f\n",
				FIXED_TO_FLOAT(AngleFixed(oldangle)),
				FIXED_TO_FLOAT(AngleFixed(newangle)),
				FIXED_TO_FLOAT(AngleFixed(oldangle-newangle))
			);
			*/
		
		}
		else if (predictedz - mo->z > abs(slopemom.z/2)
			&& P_CanApplySlopePhysics(mo, mo->standingslope) == true && (!(mo->eflags & MFE_VERTICALFLIP) && (mo->z <= mo->floorz))) // mo->z <= mo->floorz means 'only do this if we did a stairstep up'
			// Sryder 2018-11-26: Don't launch here if it's a slope without physics, we stick to those like glue anyway
		{
			// Now check if we were supposed to stick to this slope
			//CONS_Printf("%d-%d > %d\n", (predictedz), (mo->z), (slopemom.z/2));
			P_SlopeLaunch(mo);
		}
	}
	else if (moved && mo->standingslope && predictedz)
	{
		angle_t moveangle = K_MomentumAngle(mo);
		angle_t newangle = FixedMul((signed)mo->standingslope->zangle, FINECOSINE((moveangle - mo->standingslope->xydirection) >> ANGLETOFINESHIFT));

		/*
		CONS_Printf("flat to angle %f - predicted z of %f\n",
			FIXED_TO_FLOAT(AngleFixed(ANGLE_MAX-newangle)),
			FIXED_TO_FLOAT(predictedz)
		);
		*/

		if (ANGLE_MAX-newangle > ANG30 && newangle > ANGLE_180)
		{
			mo->momz = P_MobjFlip(mo)*FRACUNIT/2;
			mo->z = predictedz + P_MobjFlip(mo);
			mo->standingslope = NULL;
			mo->terrain = NULL;
			//CONS_Printf("Launched off of flat surface running into downward slope\n");
		}
	}

	// Check the gravity status.
	P_CheckGravity(mo, false);

	if (mo->flags & MF_NOCLIPHEIGHT)
		return; // no frictions for objects that can pass through floors

	if (mo->flags & MF_MISSILE || mo->flags2 & MF2_SKULLFLY || mo->type == MT_SHELL || mo->type == MT_VULTURE || mo->type == MT_PENGUINATOR)
		return; // no friction for missiles ever

	if ((mo->type == MT_BIGTUMBLEWEED || mo->type == MT_LITTLETUMBLEWEED)
			&& (mo->standingslope && abs(mo->standingslope->zdelta) > FRACUNIT>>8)) // Special exception for tumbleweeds on slopes
		return;

	//{ SRB2kart stuff
	if (mo->type == MT_FLINGRING || mo->type == MT_BALLHOG || mo->type == MT_BUBBLESHIELDTRAP)
		return;

	if (player && (player->spinouttimer && !player->wipeoutslow)
		&& player->speed <= FixedDiv(20*mapobjectscale, player->offroad + FRACUNIT))
		return;
	//}

	if (((!(mo->eflags & MFE_VERTICALFLIP) && mo->z > mo->floorz) || (mo->eflags & MFE_VERTICALFLIP && mo->z+mo->height < mo->ceilingz))
		&& !(player && player->carry == CR_SLIDING))
		return; // no friction when airborne

	P_XYFriction(mo, oldx, oldy);
}

void P_RingXYMovement(mobj_t *mo)
{
	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (!P_SceneryTryMove(mo, mo->x + mo->momx, mo->y + mo->momy))
		P_BounceMove(mo);
}

void P_SceneryXYMovement(mobj_t *mo)
{
	fixed_t oldx, oldy; // reducing bobbing/momentum on ice when up against walls

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	oldx = mo->x;
	oldy = mo->y;

	if (!P_SceneryTryMove(mo, mo->x + mo->momx, mo->y + mo->momy))
		P_BounceMove(mo);

	if (P_MobjWasRemoved(mo))
		return;

	if ((!(mo->eflags & MFE_VERTICALFLIP) && mo->z > mo->floorz) || (mo->eflags & MFE_VERTICALFLIP && mo->z+mo->height < mo->ceilingz))
		return; // no friction when airborne

	if (mo->flags & MF_NOCLIPHEIGHT)
		return; // no frictions for objects that can pass through floors

	P_SceneryXYFriction(mo, oldx, oldy);
}

//
// P_AdjustMobjFloorZ_FFloors
//
// Utility function for P_ZMovement and related
// Adjusts mo->floorz/mo->ceiling accordingly for FFloors
//
// "motype" determines what behaviour to use exactly
// This is to keep things consistent in case these various object types NEED to be different
//
// motype options:
// 0 - normal
// 1 - forces false check for water (rings)
// 2 - forces false check for water + different quicksand behaviour (scenery)
//
void P_AdjustMobjFloorZ_FFloors(mobj_t *mo, sector_t *sector, UINT8 motype)
{
	ffloor_t *rover;
	fixed_t delta1, delta2, thingtop;
	fixed_t topheight, bottomheight;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	thingtop = mo->z + mo->height;

	for (rover = sector->ffloors; rover; rover = rover->next)
	{
		if (!(rover->fofflags & FOF_EXISTS))
			continue;

		topheight = P_GetFOFTopZ(mo, sector, rover, mo->x, mo->y, NULL);
		bottomheight = P_GetFOFBottomZ(mo, sector, rover, mo->x, mo->y, NULL);

		if (mo->player && P_CheckSolidFFloorSurface(mo->player, rover)) // only the player should stand on lava or run on water
			;
		else if (motype != 0 && rover->fofflags & FOF_SWIMMABLE) // "scenery" only
			continue;
		else if (rover->fofflags & FOF_QUICKSAND) // quicksand
			;
		else if (!( // if it's not either of the following...
				(rover->fofflags & (FOF_BLOCKPLAYER|FOF_MARIO) && mo->player) // ...solid to players? (mario blocks are always solid from beneath to players)
			    || (rover->fofflags & FOF_BLOCKOTHERS && !mo->player) // ...solid to others?
				)) // ...don't take it into account.
			continue;
		if (rover->fofflags & FOF_QUICKSAND)
		{
			switch (motype)
			{
				case 2: // scenery does things differently for some reason
					if (mo->z < topheight && bottomheight < thingtop)
					{
						mo->floorz = mo->z;
						continue;
					}
					break;
				default:
					if (mo->z < topheight && bottomheight < thingtop)
					{
						if (mo->floorz < mo->z)
							mo->floorz = mo->z;
					}
					continue; // This is so you can jump/spring up through quicksand from below.
			}
		}

		delta1 = mo->z - (bottomheight + ((topheight - bottomheight)/2));
		delta2 = thingtop - (bottomheight + ((topheight - bottomheight)/2));

		if (topheight > mo->floorz && abs(delta1) < abs(delta2)
			&& (rover->fofflags & FOF_SOLID) // Non-FOF_SOLID Mario blocks are only solid from bottom
			&& !(rover->fofflags & FOF_REVERSEPLATFORM)
			&& ((P_MobjFlip(mo)*mo->momz >= 0) || (!(rover->fofflags & FOF_PLATFORM)))) // In reverse gravity, only clip for FOFs that are intangible from their bottom (the "top" you're falling through) if you're coming from above ("below" in your frame of reference)
		{
			mo->floorz = topheight;
		}
		if (bottomheight < mo->ceilingz && abs(delta1) >= abs(delta2)
			&& !(rover->fofflags & FOF_PLATFORM)
			&& ((P_MobjFlip(mo)*mo->momz >= 0) || ((rover->fofflags & FOF_SOLID) && !(rover->fofflags & FOF_REVERSEPLATFORM)))) // In normal gravity, only clip for FOFs that are intangible from the top if you're coming from below
		{
			mo->ceilingz = bottomheight;
		}
	}
}

//
// P_AdjustMobjFloorZ_PolyObjs
//
// Utility function for P_ZMovement and related
// Adjusts mo->floorz/mo->ceiling accordingly for PolyObjs
//
static void P_AdjustMobjFloorZ_PolyObjs(mobj_t *mo, subsector_t *subsec)
{
	polyobj_t *po = subsec->polyList;
	sector_t *polysec;
	fixed_t delta1, delta2, thingtop;
	fixed_t polytop, polybottom;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	thingtop = mo->z + mo->height;

	while(po)
	{
		if (!P_MobjInsidePolyobj(po, mo) || !(po->flags & POF_SOLID))
		{
			po = (polyobj_t *)(po->link.next);
			continue;
		}

		// We're inside it! Yess...
		polysec = po->lines[0]->backsector;

		if (po->flags & POF_CLIPPLANES)
		{
			polytop = polysec->ceilingheight;
			polybottom = polysec->floorheight;
		}
		else
		{
			polytop = INT32_MAX;
			polybottom = INT32_MIN;
		}

		delta1 = mo->z - (polybottom + ((polytop - polybottom)/2));
		delta2 = thingtop - (polybottom + ((polytop - polybottom)/2));

		if (polytop > mo->floorz && abs(delta1) < abs(delta2))
			mo->floorz = polytop;

		if (polybottom < mo->ceilingz && abs(delta1) >= abs(delta2))
			mo->ceilingz = polybottom;

		po = (polyobj_t *)(po->link.next);
	}
}

void P_RingZMovement(mobj_t *mo)
{
	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	// Intercept the stupid 'fall through 3dfloors' bug
	if (mo->subsector->sector->ffloors)
		P_AdjustMobjFloorZ_FFloors(mo, mo->subsector->sector, 1);
	if (mo->subsector->polyList)
		P_AdjustMobjFloorZ_PolyObjs(mo, mo->subsector);

	// adjust height
	if (mo->eflags & MFE_APPLYPMOMZ && !P_IsObjectOnGround(mo))
	{
		mo->momz += mo->pmomz;
		mo->pmomz = 0;
		mo->eflags &= ~MFE_APPLYPMOMZ;
	}
	mo->z += mo->momz;

	// clip movement
	if (mo->z <= mo->floorz && !(mo->flags & MF_NOCLIPHEIGHT))
	{
		mo->z = mo->floorz;

		mo->momz = 0;
	}
	else if (mo->z + mo->height > mo->ceilingz && !(mo->flags & MF_NOCLIPHEIGHT))
	{
		mo->z = mo->ceilingz - mo->height;

		mo->momz = 0;
	}
}

boolean P_CheckDeathPitCollide(mobj_t *mo)
{
	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (mo->player && mo->player->pflags & PF_GODMODE)
	{
		return false;
	}

	if (mo->subsector->sector->damagetype == SD_DEATHPIT)
	{
		const boolean flipped = (mo->eflags & MFE_VERTICALFLIP);
		const sectorflags_t flags = mo->subsector->sector->flags;

		return (
			   (mo->z <= mo->subsector->sector->floorheight
				&& ((flags & MSF_TRIGGERSPECIAL_HEADBUMP) || !flipped) && (flags & MSF_FLIPSPECIAL_FLOOR))
			|| (mo->z + mo->height >= mo->subsector->sector->ceilingheight
				&& ((flags & MSF_TRIGGERSPECIAL_HEADBUMP) || flipped) && (flags & MSF_FLIPSPECIAL_CEILING))
		);
	}

	return false;
}

boolean P_CheckSolidLava(ffloor_t *rover)
{
	if ((rover->fofflags & FOF_SWIMMABLE) && (rover->master->frontsector->damagetype == SD_LAVA))
	{
		return true;
	}

	return false;
}

//
// P_ZMovement
// Returns false if the mobj was killed/exploded/removed, true otherwise.
//
boolean P_ZMovement(mobj_t *mo)
{
	fixed_t dist, delta;
	boolean onground;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	// Intercept the stupid 'fall through 3dfloors' bug
	if (mo->subsector->sector->ffloors)
		P_AdjustMobjFloorZ_FFloors(mo, mo->subsector->sector, 0);
	if (mo->subsector->polyList)
		P_AdjustMobjFloorZ_PolyObjs(mo, mo->subsector);

	// adjust height
	if (mo->eflags & MFE_APPLYPMOMZ && !P_IsObjectOnGround(mo))
	{
		mo->momz += mo->pmomz;
		mo->pmomz = 0;
		mo->eflags &= ~MFE_APPLYPMOMZ;
	}
	mo->z += mo->momz;
	onground = P_IsObjectOnGround(mo);

	if (mo->standingslope)
	{
		if (mo->flags & MF_NOCLIPHEIGHT)
			mo->standingslope = NULL;
		else if (!onground)
			P_SlopeLaunch(mo);
	}

	switch (mo->type)
	{
		case MT_SKIM:
			// skims don't bounce
			if (mo->z > mo->watertop && mo->z - mo->momz <= mo->watertop)
			{
				mo->z = mo->watertop;
				mo->momz = 0;
				mo->flags |= MF_NOGRAVITY;
			}
			break;
		case MT_SPINFIRE:
			if (P_CheckDeathPitCollide(mo))
			{
				P_RemoveMobj(mo);
				return false;
			}
			break;
		case MT_GOOP:
			if (P_CheckDeathPitCollide(mo))
			{
				P_RemoveMobj(mo);
				return false;
			}
			if (mo->z <= mo->floorz && mo->momz)
			{
				P_SetMobjState(mo, mo->info->meleestate);
				mo->momx = mo->momy = mo->momz = 0;
				mo->z = mo->floorz;
				if (mo->info->painsound)
					S_StartSound(mo, mo->info->painsound);
			}
			break;
		case MT_FALLINGROCK:
		case MT_BIGTUMBLEWEED:
		case MT_LITTLETUMBLEWEED:
		case MT_SHELL:
			// Remove stuff from death pits.
			if (P_CheckDeathPitCollide(mo))
			{
				P_RemoveMobj(mo);
				return false;
			}
			break;

		case MT_REDFLAG:
		case MT_BLUEFLAG:
			// Remove from death pits.  DON'T FUCKING DESPAWN IT DAMMIT
			if (P_CheckDeathPitCollide(mo))
			{
				mo->fuse = 1;
				return false;
			}
			break;

		case MT_EMERALD:
			if (P_CheckDeathPitCollide(mo))
			{
				P_RemoveMobj(mo);
				return false;
			}

			if (mo->z <= mo->floorz || mo->z + mo->height >= mo->ceilingz)
			{
				// Stop when hitting the floor
				mo->momx = mo->momy = 0;
			}
			break;

		case MT_RING: // Ignore still rings
		case MT_BLUESPHERE:
		case MT_FLINGRING:
			// Remove flinged stuff from death pits.
			if (P_CheckDeathPitCollide(mo))
			{
				P_RemoveMobj(mo);
				return false;
			}
			if (!(mo->momx || mo->momy || mo->momz))
				return true;
			break;
		case MT_BOUNCERING:
		case MT_INFINITYRING:
		case MT_AUTOMATICRING:
		case MT_RAILRING:
		case MT_EXPLOSIONRING:
		case MT_SCATTERRING:
		case MT_GRENADERING:
		case MT_BOUNCEPICKUP:
		case MT_RAILPICKUP:
		case MT_AUTOPICKUP:
		case MT_EXPLODEPICKUP:
		case MT_SCATTERPICKUP:
		case MT_GRENADEPICKUP:
			// Remove flinged stuff from death pits.
			if (P_CheckDeathPitCollide(mo) && (mo->flags2 & MF2_DONTRESPAWN))
			{
				P_RemoveMobj(mo);
				return false;
			}
			if (!(mo->momx || mo->momy || mo->momz))
				return true;
			break;
		case MT_FLAMEJET:
		case MT_VERTICALFLAMEJET:
			if (mo->flags & MF_BOUNCE)
				return true;
			break;
		case MT_SPIKE:
		case MT_WALLSPIKE:
			// Dead spike particles disappear upon ground contact
			if (!mo->health && (mo->z <= mo->floorz || mo->z + mo->height >= mo->ceilingz))
			{
				P_RemoveMobj(mo);
				return false;
			}
			break;
		default:
			// SRB2kart stuff that should die in pits
			// Shouldn't stop moving along the Z if there's no speed though!
			if (P_CanDeleteKartItem(mo->type))
			{
				// Remove stuff from death pits.
				if (P_CheckDeathPitCollide(mo))
				{
					P_RemoveMobj(mo);
					return false;
				}
			}
			break;
	}

	if (!mo->player && P_CheckDeathPitCollide(mo))
	{
		if (mo->flags & MF_ENEMY || mo->flags & MF_BOSS || mo->type == MT_MINECART)
		{
			// Kill enemies, bosses and minecarts that fall into death pits.
			if (mo->health)
			{
				P_KillMobj(mo, NULL, NULL, DMG_NORMAL);
			}
			return false;
		}
	}

	if (P_MobjFlip(mo)*mo->momz < 0
	&& (mo->flags2 & MF2_CLASSICPUSH))
		mo->momx = mo->momy = 0;

	if (mo->flags & MF_FLOAT && mo->target && mo->health
	&& !(mo->type == MT_EGGMOBILE) && mo->target->health > 0)
	{
		// float down towards target if too close
		if (!(mo->flags2 & MF2_SKULLFLY) && !(mo->flags2 & MF2_INFLOAT))
		{
			dist = P_AproxDistance(mo->x - mo->target->x, mo->y - mo->target->y);

			delta = (mo->target->z + (mo->height>>1)) - mo->z;

			if (delta < 0 && dist < -(delta*3))
				mo->z -= FixedMul(FLOATSPEED, mo->scale);
			else if (delta > 0 && dist < (delta*3))
				mo->z += FixedMul(FLOATSPEED, mo->scale);

			if (mo->type == MT_JETJAW && mo->z + mo->height > mo->watertop)
				mo->z = mo->watertop - mo->height;
		}

	}

	// clip movement
	if (((mo->z <= mo->floorz && !(mo->eflags & MFE_VERTICALFLIP))
		|| (mo->z + mo->height >= mo->ceilingz && mo->eflags & MFE_VERTICALFLIP))
		&& !(mo->flags & MF_NOCLIPHEIGHT))
	{
		vector3_t mom;
		mom.x = mo->momx;
		mom.y = mo->momy;
		mom.z = mo->momz;

		if (mo->eflags & MFE_VERTICALFLIP)
			mo->z = mo->ceilingz - mo->height;
		else
			mo->z = mo->floorz;

		if (!(mo->flags & MF_MISSILE) && mo->standingslope) // You're still on the ground; why are we here?
		{
			mo->momz = 0;
			return true;
		}

		P_CheckPosition(mo, mo->x, mo->y); // Sets mo->standingslope correctly
		if (P_MobjWasRemoved(mo)) // mobjs can be removed by P_CheckPosition -- Monster Iestyn 31/07/21
			return false;

		K_UpdateMobjTerrain(mo, ((mo->eflags & MFE_VERTICALFLIP) ? tmceilingpic : tmfloorpic));

		if (((mo->eflags & MFE_VERTICALFLIP) ? tmceilingslope : tmfloorslope) && (mo->type != MT_STEAM))
		{
			mo->standingslope = (mo->eflags & MFE_VERTICALFLIP) ? tmceilingslope : tmfloorslope;
			P_SetPitchRollFromSlope(mo, mo->standingslope);
			P_ReverseQuantizeMomentumToSlope(&mom, mo->standingslope);
		}

		// hit the floor
		if (mo->type == MT_FIREBALL) // special case for the fireball
			mom.z = P_MobjFlip(mo)*FixedMul(5*FRACUNIT, mo->scale);
		else if (mo->type == MT_SPINFIRE) // elemental shield fire is another exception here
			;
		else if (mo->type == MT_DEBTSPIKE)
		{
			mom.x = mom.y = 0;
			mom.z = -mom.z/2;
		}
		else if (mo->flags & MF_MISSILE)
		{
			if (!(mo->flags & MF_NOCLIP))
			{
				// This is a really ugly hard-coded hack to prevent grenades
				// from exploding the instant they hit the ground, and then
				// another to prevent them from turning into hockey pucks.
				// I'm sorry in advance. -SH
				// PS: Oh, and Brak's napalm bombs too, now.
				if (mo->flags & MF_GRENADEBOUNCE)
				{
					// Going down? (Or up in reverse gravity?)
					if (P_MobjFlip(mo)*mom.z < 0)
					{
						// If going slower than a fracunit, just stop.
						if (abs(mom.z) < mo->scale)
						{
							mom.x = mom.y = mom.z = 0;
						}
						// Otherwise bounce up at half speed.
						else
							mom.z = -mom.z/2;
						S_StartSound(mo, mo->info->activesound);
					}
				}
				// Hack over. Back to your regularly scheduled detonation. -SH
				else
				{
					// Don't explode on the sky!
					if (!(mo->eflags & MFE_VERTICALFLIP)
					&& mo->subsector->sector->floorpic == skyflatnum
					&& mo->subsector->sector->floorheight == mo->floorz)
						P_RemoveMobj(mo);
					else if (mo->eflags & MFE_VERTICALFLIP
					&& mo->subsector->sector->ceilingpic == skyflatnum
					&& mo->subsector->sector->ceilingheight == mo->ceilingz)
						P_RemoveMobj(mo);
					else
						P_ExplodeMissile(mo);
					return false;
				}
			}
		}

		if (P_MobjFlip(mo)*mom.z < 0) // falling
		{
			mo->eflags |= MFE_JUSTHITFLOOR;
			K_SpawnSplashForMobj(mo, abs(mom.z));

			if (mo->flags2 & MF2_SKULLFLY) // the skull slammed into something
				mom.z = -mom.z;
			else if (mo->type == MT_BIGTUMBLEWEED
				|| mo->type == MT_LITTLETUMBLEWEED
				|| mo->type == MT_CANNONBALLDECOR
				|| mo->type == MT_FALLINGROCK)
			{
				mom.z = -FixedMul(mom.z, FixedDiv(17*FRACUNIT,20*FRACUNIT));

				if (mo->type == MT_BIGTUMBLEWEED || mo->type == MT_LITTLETUMBLEWEED)
				{
					if (abs(mom.x) < FixedMul(STOPSPEED, mo->scale)
						&& abs(mom.y) < FixedMul(STOPSPEED, mo->scale)
						&& abs(mom.z) < FixedMul(STOPSPEED*3, mo->scale))
					{
						if (mo->flags2 & MF2_AMBUSH)
						{
							// Give the tumbleweed another random kick if it runs out of steam.
							mom.z += P_MobjFlip(mo)*FixedMul(6*FRACUNIT, mo->scale);

							if (P_RandomChance(FRACUNIT/2))
								mom.x += FixedMul(6*FRACUNIT, mo->scale);
							else
								mom.x -= FixedMul(6*FRACUNIT, mo->scale);

							if (P_RandomChance(FRACUNIT/2))
								mom.y += FixedMul(6*FRACUNIT, mo->scale);
							else
								mom.y -= FixedMul(6*FRACUNIT, mo->scale);
						}
						else if (mo->standingslope && abs(mo->standingslope->zdelta) > FRACUNIT>>8)
						{
							// Pop the object up a bit to encourage bounciness
							//mom.z = P_MobjFlip(mo)*mo->scale;
						}
						else
						{
							mom.x = mom.y = mom.z = 0;
							P_SetMobjState(mo, mo->info->spawnstate);
						}
					}

					// Stolen from P_SpawnFriction
					mo->friction = FRACUNIT - 0x100;
				}
				else if (mo->type == MT_FALLINGROCK)
				{
					if (P_MobjFlip(mo)*mom.z > FixedMul(2*FRACUNIT, mo->scale))
						S_StartSound(mo, mo->info->activesound + P_RandomKey(mo->info->reactiontime));

					mom.z /= 2; // Rocks not so bouncy

					if (!mo->fuse
						&& abs(mom.x) < FixedMul(STOPSPEED*2, mo->scale)
						&& abs(mom.y) < FixedMul(STOPSPEED*2, mo->scale)
						&& abs(mom.z) < FixedMul(STOPSPEED*2*3, mo->scale))
					{
						//P_RemoveMobj(mo);
						//return false;
						mo->fuse = TICRATE;
					}
				}
				else if (mo->type == MT_CANNONBALLDECOR)
				{
					mom.z /= 2;
					if (abs(mom.z) < FixedMul(STOPSPEED*3, mo->scale))
						mom.z = 0;
				}
			}
			else
				mom.z = (tmfloorthing ? tmfloorthing->momz : 0);

		}
		else if (tmfloorthing)
			mom.z = tmfloorthing->momz;

		if (mo->standingslope) { // MT_STEAM will never have a standingslope, see above.
			P_QuantizeMomentumToSlope(&mom, mo->standingslope);
		}

		mo->momx = mom.x;
		mo->momy = mom.y;
		mo->momz = mom.z;

		if (mo->type == MT_STEAM)
			return true;
	}
	else
	{
		mo->terrain = NULL;

		if (!(mo->flags & MF_NOGRAVITY)) // Gravity here!
		{
			/// \todo may not be needed (done in P_MobjThinker normally)
			mo->eflags &= ~MFE_JUSTHITFLOOR;

			P_CheckGravity(mo, true);
		}
	}

	if (((mo->z + mo->height > mo->ceilingz && !(mo->eflags & MFE_VERTICALFLIP))
		|| (mo->z < mo->floorz && mo->eflags & MFE_VERTICALFLIP))
	&& !(mo->flags & MF_NOCLIPHEIGHT))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
			mo->z = mo->floorz;
		else
			mo->z = mo->ceilingz - mo->height;

		if (mo->type == MT_SPINFIRE)
			;
		else if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
		{
			// Hack 2: Electric Boogaloo -SH
			if (mo->flags & MF_GRENADEBOUNCE)
			{
				if (P_MobjFlip(mo)*mo->momz >= 0)
				{
					mo->momz = -mo->momz;
					S_StartSound(mo, mo->info->activesound);
				}
			}
			else
			{
				// Don't explode on the sky!
				if (!(mo->eflags & MFE_VERTICALFLIP)
				&& mo->subsector->sector->ceilingpic == skyflatnum
				&& mo->subsector->sector->ceilingheight == mo->ceilingz)
					P_RemoveMobj(mo);
				else if (mo->eflags & MFE_VERTICALFLIP
				&& mo->subsector->sector->floorpic == skyflatnum
				&& mo->subsector->sector->floorheight == mo->floorz)
					P_RemoveMobj(mo);
				else
					P_ExplodeMissile(mo);
				return false;
			}
		}

		if (P_MobjFlip(mo)*mo->momz > 0) // hit the ceiling
		{
			if (mo->flags2 & MF2_SKULLFLY) // the skull slammed into something
				mo->momz = -mo->momz;
			else
			// Flags bounce
			if (mo->type == MT_REDFLAG || mo->type == MT_BLUEFLAG)
			{
				mo->momz = -FixedMul(mo->momz, FixedDiv(17*FRACUNIT,20*FRACUNIT));
			}
			else
				mo->momz = 0;
		}
	}

	return true;
}

// Check for "Mario" blocks to hit and bounce them
static void P_CheckMarioBlocks(mobj_t *mo)
{
	msecnode_t *node;

	if (netgame && mo->player->spectator)
		return;

	for (node = mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		ffloor_t *rover;

		if (!node->m_sector->ffloors)
			continue;

		for (rover = node->m_sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (!(rover->fofflags & FOF_MARIO))
				continue;

			if (mo->eflags & MFE_VERTICALFLIP)
				continue; // if you were flipped, your head isn't actually hitting your ceilingz is it?

			if (*rover->bottomheight != mo->ceilingz)
				continue;

			if (rover->fofflags & FOF_GOOWATER) // Brick block!
				EV_CrumbleChain(node->m_sector, rover);
			else // Question block!
				EV_MarioBlock(rover, node->m_sector, mo);
		}
	}
}

// Check if we're on a polyobject that triggers a linedef executor.
static boolean P_PlayerPolyObjectZMovement(mobj_t *mo)
{
	msecnode_t *node;
	boolean stopmovecut = false;

	for (node = mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		sector_t *sec = node->m_sector;
		subsector_t *newsubsec;
		size_t i;

		for (i = 0; i < numsubsectors; i++)
		{
			polyobj_t *po;
			sector_t *polysec;
			newsubsec = &subsectors[i];

			if (newsubsec->sector != sec)
				continue;

			for (po = newsubsec->polyList; po; po = (polyobj_t *)(po->link.next))
			{
				if (!(po->flags & POF_SOLID))
					continue;

				if (!P_MobjInsidePolyobj(po, mo))
					continue;

				polysec = po->lines[0]->backsector;

				// Moving polyobjects should act like conveyors if the player lands on one. (I.E. none of the momentum cut thing below) -Red
				if ((mo->z == polysec->ceilingheight || mo->z + mo->height == polysec->floorheight) && po->thinker)
					stopmovecut = true;

				if (!(po->flags & POF_LDEXEC))
					continue;

				if (mo->z != polysec->ceilingheight)
					continue;

				// We're landing on a PO, so check for a linedef executor.
				P_LinedefExecute(po->triggertag, mo, NULL);
			}
		}
	}

	return stopmovecut;
}

void P_PlayerZMovement(mobj_t *mo)
{
	boolean onground;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (!mo->player)
		return;

	// Intercept the stupid 'fall through 3dfloors' bug
	if (mo->subsector->sector->ffloors)
		P_AdjustMobjFloorZ_FFloors(mo, mo->subsector->sector, 0);
	if (mo->subsector->polyList)
		P_AdjustMobjFloorZ_PolyObjs(mo, mo->subsector);

	// check for smooth step up
	if ((mo->eflags & MFE_VERTICALFLIP && mo->z + mo->height > mo->ceilingz)
		|| (!(mo->eflags & MFE_VERTICALFLIP) && mo->z < mo->floorz))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
			mo->player->viewheight -= (mo->z+mo->height) - mo->ceilingz;
		else
			mo->player->viewheight -= mo->floorz - mo->z;

		mo->player->deltaviewheight =
			(P_GetPlayerViewHeight(mo->player) - mo->player->viewheight)>>3;
	}

	// adjust height
	if (mo->eflags & MFE_APPLYPMOMZ && !P_IsObjectOnGround(mo))
	{
		mo->momz += mo->pmomz;
		mo->pmomz = 0;
		mo->eflags &= ~MFE_APPLYPMOMZ;
	}

	mo->z += mo->momz;
	onground = P_IsObjectOnGround(mo);

	// Have player fall through floor?
	if (mo->player->playerstate == PST_DEAD
	|| mo->player->playerstate == PST_REBORN)
		return;

	if (mo->standingslope)
	{
		if (mo->flags & MF_NOCLIPHEIGHT)
			mo->standingslope = NULL;
		else if (!onground)
			P_SlopeLaunch(mo);
	}

	// clip movement
	if (onground && !(mo->flags & MF_NOCLIPHEIGHT))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
		{
			mo->z = mo->ceilingz - mo->height;
		}
		else
		{
			mo->z = mo->floorz;
		}

		K_UpdateMobjTerrain(mo, (mo->eflags & MFE_VERTICALFLIP ? tmceilingpic : tmfloorpic));

		// Get up if you fell.
		if (mo->player->panim == PA_HURT && mo->player->spinouttimer == 0 && mo->player->squishedtimer == 0)
		{
			P_SetPlayerMobjState(mo, S_KART_STILL);
		}

		if (!mo->standingslope && (mo->eflags & MFE_VERTICALFLIP ? tmceilingslope : tmfloorslope))
		{
			// Handle landing on slope during Z movement
			P_HandleSlopeLanding(mo, (mo->eflags & MFE_VERTICALFLIP ? tmceilingslope : tmfloorslope));
		}

		if (P_MobjFlip(mo) * mo->momz < 0) // falling
		{
			boolean clipmomz = !(P_CheckDeathPitCollide(mo));

			mo->pmomz = 0; // We're on a new floor, don't keep doing platform movement.
			mo->eflags |= MFE_JUSTHITFLOOR; // Spin Attack

			clipmomz = P_PlayerHitFloor(mo->player, true);
			P_PlayerPolyObjectZMovement(mo);

			if (clipmomz)
			{
				mo->momz = (tmfloorthing ? tmfloorthing->momz : 0);
			}
		}
		else if (tmfloorthing)
		{
			mo->momz = tmfloorthing->momz;
		}
	}
	else
	{
		mo->terrain = NULL;

		if (!(mo->flags & MF_NOGRAVITY)) // Gravity here!
		{
			if (P_IsObjectInGoop(mo) && !(mo->flags & MF_NOCLIPHEIGHT))
			{
				if (mo->z < mo->floorz)
				{
					mo->z = mo->floorz;
					mo->momz = 0;
				}
				else if (mo->z + mo->height > mo->ceilingz)
				{
					mo->z = mo->ceilingz - mo->height;
					mo->momz = 0;
				}
			}
			/// \todo may not be needed (done in P_MobjThinker normally)
			mo->eflags &= ~MFE_JUSTHITFLOOR;
			P_CheckGravity(mo, true);
		}
	}

	if (((mo->eflags & MFE_VERTICALFLIP && mo->z < mo->floorz) || (!(mo->eflags & MFE_VERTICALFLIP) && mo->z + mo->height > mo->ceilingz))
		&& !(mo->flags & MF_NOCLIPHEIGHT))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
			mo->z = mo->floorz;
		else
			mo->z = mo->ceilingz - mo->height;

		if (P_MobjFlip(mo)*mo->momz > 0)
		{
			if (CheckForMarioBlocks)
				P_CheckMarioBlocks(mo);

			mo->momz = 0;
			P_CheckGravity(mo, true);

			if (abs(mo->momz) < 15 * mapobjectscale)
			{
				mo->momz = 15 * mapobjectscale
					* -(P_MobjFlip(mo));
			}

			K_SpawnBumpEffect(mo);
		}
	}
}

boolean P_SceneryZMovement(mobj_t *mo)
{
	// Intercept the stupid 'fall through 3dfloors' bug
	if (mo->subsector->sector->ffloors)
		P_AdjustMobjFloorZ_FFloors(mo, mo->subsector->sector, 2);
	if (mo->subsector->polyList)
		P_AdjustMobjFloorZ_PolyObjs(mo, mo->subsector);

	// adjust height
	if (mo->eflags & MFE_APPLYPMOMZ && !P_IsObjectOnGround(mo))
	{
		mo->momz += mo->pmomz;
		mo->pmomz = 0;
		mo->eflags &= ~MFE_APPLYPMOMZ;
	}
	mo->z += mo->momz;

	switch (mo->type)
	{
		case MT_BOOMEXPLODE:
		case MT_BOOMPARTICLE:
			if (!(mo->flags & MF_BOUNCE) && (mo->z <= mo->floorz || mo->z+mo->height >= mo->ceilingz))
			{
				// set standingslope
				P_TryMove(mo, mo->x, mo->y, true);
				mo->momz = -mo->momz;
				if (mo->standingslope)
				{
					if (mo->flags & MF_NOCLIPHEIGHT)
						mo->standingslope = NULL;
					else if (!P_IsObjectOnGround(mo))
						P_SlopeLaunch(mo);
				}
				S_StartSound(mo, mo->info->activesound);
			}
			break;
		case MT_SMALLBUBBLE:
			if (mo->z <= mo->floorz || mo->z+mo->height >= mo->ceilingz) // Hit the floor, so POP!
			{
				// don't sounds stop when you kill the mobj..?
				// yes, they do, making this entirely redundant
				P_RemoveMobj(mo);
				return false;
			}
			break;
		case MT_MEDIUMBUBBLE:
			if (P_CheckDeathPitCollide(mo)) // Don't split if you fell in a pit
			{
				P_RemoveMobj(mo);
				return false;
			}
			if ((!(mo->eflags & MFE_VERTICALFLIP) && mo->z <= mo->floorz)
			|| (mo->eflags & MFE_VERTICALFLIP && mo->z+mo->height >= mo->ceilingz)) // Hit the floor, so split!
			{
				// split
				mobj_t *explodemo = NULL;
				UINT8 prandom, i;

				for (i = 0; i < 4; ++i) // split into four
				{
					prandom = P_RandomByte();
					explodemo = P_SpawnMobj(mo->x, mo->y, mo->z, MT_SMALLBUBBLE);
					explodemo->momx += ((prandom & 0x0F) << (FRACBITS-2)) * (i & 2 ? -1 : 1);
					explodemo->momy += ((prandom & 0xF0) << (FRACBITS-6)) * (i & 1 ? -1 : 1);
					explodemo->destscale = mo->scale;
					P_SetScale(explodemo, mo->scale);
				}

				if (mo->threshold != 42) // Don't make pop sound if threshold is 42.
					S_StartSound(explodemo, sfx_bubbl1 + P_RandomKey(5));
				//note that we assign the bubble sound to one of the new bubbles.
				// in other words, IT ACTUALLY GETS USED YAAAAAAAY

				P_RemoveMobj(mo);
				return false;
			}
			else if (mo->z <= mo->floorz || mo->z+mo->height >= mo->ceilingz) // Hit the ceiling instead? Just disappear anyway
			{
				P_RemoveMobj(mo);
				return false;
			}
			break;
		case MT_SEED: // now scenery
			if (P_CheckDeathPitCollide(mo)) // No flowers for death pits
			{
				P_RemoveMobj(mo);
				return false;
			}
			// Soniccd seed turns into a flower!
			if ((!(mo->eflags & MFE_VERTICALFLIP) && mo->z <= mo->floorz)
			|| (mo->eflags & MFE_VERTICALFLIP && mo->z+mo->height >= mo->ceilingz))
			{
				mobjtype_t flowertype = ((P_RandomChance(FRACUNIT/2)) ? MT_GFZFLOWER1 : MT_GFZFLOWER3);
				mobj_t *flower = P_SpawnMobjFromMobj(mo, 0, 0, 0, flowertype);
				if (flower)
				{
					P_SetScale(flower, mo->scale/16);
					flower->destscale = mo->scale;
					flower->scalespeed = mo->scale/8;
				}

				P_RemoveMobj(mo);
				return false;
			}
		default:
			break;
	}

	// clip movement
	if (((mo->z <= mo->floorz && !(mo->eflags & MFE_VERTICALFLIP))
		|| (mo->z + mo->height >= mo->ceilingz && mo->eflags & MFE_VERTICALFLIP))
	&& !(mo->flags & MF_NOCLIPHEIGHT))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
			mo->z = mo->ceilingz - mo->height;
		else
			mo->z = mo->floorz;

		if (P_MobjFlip(mo)*mo->momz < 0) // falling
		{
			mo->eflags |= MFE_JUSTHITFLOOR; // Spin Attack

			if (tmfloorthing)
				mo->momz = tmfloorthing->momz;
			else if (!tmfloorthing)
				mo->momz = 0;
		}
	}
	else if (!(mo->flags & MF_NOGRAVITY)) // Gravity here!
	{
		/// \todo may not be needed (done in P_MobjThinker normally)
		mo->eflags &= ~MFE_JUSTHITFLOOR;

		P_CheckGravity(mo, true);
	}

	if (((mo->z + mo->height > mo->ceilingz && !(mo->eflags & MFE_VERTICALFLIP))
		|| (mo->z < mo->floorz && mo->eflags & MFE_VERTICALFLIP))
	&& !(mo->flags & MF_NOCLIPHEIGHT))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
			mo->z = mo->floorz;
		else
			mo->z = mo->ceilingz - mo->height;

		if (P_MobjFlip(mo)*mo->momz > 0) // hit the ceiling
			mo->momz = 0;
	}

	return true;
}

//
// P_CanRunOnWater
//
// Returns true if player can water run on a 3D floor
//
boolean P_CanRunOnWater(player_t *player, ffloor_t *rover)
{
	const boolean flip = (player->mo->eflags & MFE_VERTICALFLIP);
	fixed_t surfaceheight = INT32_MAX;
	fixed_t playerbottom = INT32_MAX;
	fixed_t surfDiff = INT32_MAX;
	fixed_t maxStep = INT32_MAX;
	boolean doifit = false;

	pslope_t *waterSlope = NULL;
	angle_t ourZAng = 0;
	angle_t waterZAng = 0;

	if (rover == NULL)
	{
		// No rover.
		return false;
	}

	if (!(rover->fofflags & FOF_SWIMMABLE))
	{
		// It's not even a water FOF.
		return false;
	}

	if (player->carry != CR_NONE) // Special carry state.
	{
		// No good player state.
		return false;
	}

	if (P_IsObjectOnGround(player->mo) == false)
	{
		// Don't allow jumping onto water to start a water run.
		// (Already water running still counts as being on the ground.)
		return false;
	}

	if (K_WaterRun(player) == false)
	{
		// Basic conditions for enabling water run.
		return false;
	}

	if (player->mo->standingslope != NULL)
	{
		ourZAng = player->mo->standingslope->zangle;
	}
 
	waterSlope = (flip ? *rover->b_slope : *rover->t_slope);
	if (waterSlope != NULL)
	{
		waterZAng = waterSlope->zangle;
	}
 
	if (ourZAng != waterZAng)
	{
		// The surface slopes are different.
		return false;
	}
 
	surfaceheight = flip ? P_GetFFloorBottomZAt(rover, player->mo->x, player->mo->y) : P_GetFFloorTopZAt(rover, player->mo->x, player->mo->y);
	playerbottom = flip ? (player->mo->z + player->mo->height) : player->mo->z;
 
	doifit = flip ? (surfaceheight - player->mo->floorz >= player->mo->height) : (player->mo->ceilingz - surfaceheight >= player->mo->height);
 
	if (!doifit)
	{
		// Player can't fit in this space.
		return false;
	}

	maxStep = P_GetThingStepUp(player->mo, player->mo->x, player->mo->y);

	surfDiff = flip ? (surfaceheight - playerbottom) : (playerbottom - surfaceheight);
	if (surfDiff <= maxStep && surfDiff >= 0)
	{
		// We start water run IF we can step-down!
		return true;
	}
 
	return false;
}

boolean P_CheckSolidFFloorSurface(player_t *player, ffloor_t *rover)
{
	return P_CheckSolidLava(rover) ||
		P_CanRunOnWater(player, rover);
}

//
// P_MobjCheckWater
//
// Check for water, set stuff in mobj_t struct for movement code later.
// This is called either by P_MobjThinker() or P_PlayerThink()
void P_MobjCheckWater(mobj_t *mobj)
{
	boolean waterwasnotset = (mobj->watertop == INT32_MAX);
	boolean wasinwater = (mobj->eflags & MFE_UNDERWATER) == MFE_UNDERWATER;
	boolean wasingoo = (mobj->eflags & MFE_GOOWATER) == MFE_GOOWATER;
	fixed_t thingtop = mobj->z + mobj->height;
	sector_t *sector = mobj->subsector->sector;
	ffloor_t *rover;
	player_t *p = mobj->player; // Will just be null if not a player.
	fixed_t height = mobj->height;
	fixed_t halfheight = height / 2;
	boolean wasgroundpounding = false;
	fixed_t top2 = P_GetSectorCeilingZAt(sector, mobj->x, mobj->y);
	fixed_t bot2 = P_GetSectorFloorZAt(sector, mobj->x, mobj->y);

	// Default if no water exists.
	mobj->watertop = mobj->waterbottom = mobj->z - 1000*FRACUNIT;

	// Reset water state.
	mobj->eflags &= ~(MFE_UNDERWATER|MFE_TOUCHWATER|MFE_GOOWATER|MFE_TOUCHLAVA);

	for (rover = sector->ffloors; rover; rover = rover->next)
	{
		fixed_t topheight, bottomheight;

		topheight = P_GetSpecialTopZ(mobj, sectors + rover->secnum, sector);
		bottomheight = P_GetSpecialBottomZ(mobj, sectors + rover->secnum, sector);

		if (!(rover->fofflags & FOF_EXISTS) || !(rover->fofflags & FOF_SWIMMABLE)
		 || (((rover->fofflags & FOF_BLOCKPLAYER) && mobj->player)
		 || ((rover->fofflags & FOF_BLOCKOTHERS) && !mobj->player)))
		{
			if (topheight < top2 && topheight > thingtop)
				top2 = topheight;
			if (bottomheight > bot2 && bottomheight < mobj->z)
				bot2 = bottomheight;
			continue;
		}

		if (mobj->eflags & MFE_VERTICALFLIP)
		{
			if (topheight < (thingtop - halfheight)
			 || bottomheight > (thingtop + halfheight))
				continue;
		}
		else
		{
			if (topheight < (mobj->z - halfheight)
			 || bottomheight > (mobj->z + halfheight))
				continue;
		}

		// Set the watertop and waterbottom
		mobj->watertop = topheight;
		mobj->waterbottom = bottomheight;

		// Just touching the water?
		if (((mobj->eflags & MFE_VERTICALFLIP) && thingtop - height < bottomheight)
		 || (!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z + height > topheight))
			mobj->eflags |= MFE_TOUCHWATER;

		// Actually in the water?
		if (((mobj->eflags & MFE_VERTICALFLIP) && thingtop - (height>>1) > bottomheight)
		 || (!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z + (height>>1) < topheight))
			mobj->eflags |= MFE_UNDERWATER;

		if (mobj->eflags & (MFE_TOUCHWATER|MFE_UNDERWATER))
		{
			if (rover->master->frontsector->damagetype == SD_LAVA)
				mobj->eflags |= MFE_TOUCHLAVA;

			if (rover->fofflags & FOF_GOOWATER && !(mobj->flags & MF_NOGRAVITY))
				mobj->eflags |= MFE_GOOWATER;
		}
	}

	if (mobj->terrain != NULL)
	{
		if (mobj->terrain->flags & TRF_LIQUID)
		{
			// This floor is water.
			mobj->eflags |= MFE_TOUCHWATER;

			if (mobj->eflags & MFE_VERTICALFLIP)
			{
				mobj->watertop = thingtop + height;
				mobj->waterbottom = thingtop;
			}
			else
			{
				mobj->watertop = mobj->z;
				mobj->waterbottom = mobj->z - height;
			}
		}
	}

	if (mobj->watertop > top2)
		mobj->watertop = top2;

	if (mobj->waterbottom < bot2)
		mobj->waterbottom = bot2;

	// Spectators and dead players don't get to do any of the things after this.
	if (p && (p->spectator || p->playerstate != PST_LIVE))
	{
		return;
	}

	// The rest of this code only executes on a water state change.
	if (!!(mobj->eflags & MFE_UNDERWATER) == wasinwater)
		return;

	if (p && !p->waterskip &&
			p->curshield != KSHIELD_BUBBLE && wasinwater)
	{
		S_StartSound(mobj, sfx_s3k38);
	}

	if ((p) // Players
	 || (mobj->flags & MF_PUSHABLE) // Pushables
	 || ((mobj->info->flags & MF_PUSHABLE) && mobj->fuse) // Previously pushable, might be moving still
	)
	{
		fixed_t waterZ = INT32_MAX;
		fixed_t solidZ = INT32_MAX;
		fixed_t diff = INT32_MAX;

		fixed_t thingZ = INT32_MAX;
		boolean splashValid = false;

		if (mobj->eflags & MFE_VERTICALFLIP)
		{
			waterZ = mobj->waterbottom;
			solidZ = mobj->ceilingz;
		}
		else
		{
			waterZ = mobj->watertop;
			solidZ = mobj->floorz;
		}

		diff = waterZ - solidZ;
		if (mobj->eflags & MFE_VERTICALFLIP)
		{
			diff = -diff;
		}

		// Time to spawn the bubbles!
		{
			INT32 i;
			INT32 bubblecount;
			UINT8 prandom[4];
			mobj_t *bubble;
			mobjtype_t bubbletype;

			if (mobj->eflags & MFE_GOOWATER || wasingoo)
				S_StartSound(mobj, sfx_ghit);
			else if (mobj->eflags & MFE_TOUCHLAVA)
				S_StartSound(mobj, sfx_splash);
			else
				S_StartSound(mobj, sfx_splish); // And make a sound!

			bubblecount = FixedDiv(abs(mobj->momz), mobj->scale)>>(FRACBITS-1);
			// Max bubble count
			if (bubblecount > 128)
				bubblecount = 128;

			// Create tons of bubbles
			for (i = 0; i < bubblecount; i++)
			{
				// P_RandomByte()s are called individually to allow consistency
				// across various compilers, since the order of function calls
				// in C is not part of the ANSI specification.
				prandom[0] = P_RandomByte();
				prandom[1] = P_RandomByte();
				prandom[2] = P_RandomByte();
				prandom[3] = P_RandomByte();

				bubbletype = MT_SMALLBUBBLE;
				if (!(prandom[0] & 0x3)) // medium bubble chance up to 64 from 32
					bubbletype = MT_MEDIUMBUBBLE;

				bubble = P_SpawnMobj(
					mobj->x + FixedMul((prandom[1]<<(FRACBITS-3)) * (prandom[0]&0x80 ? 1 : -1), mobj->scale),
					mobj->y + FixedMul((prandom[2]<<(FRACBITS-3)) * (prandom[0]&0x40 ? 1 : -1), mobj->scale),
					mobj->z + FixedMul((prandom[3]<<(FRACBITS-2)), mobj->scale), bubbletype);

				if (bubble)
				{
					if (P_MobjFlip(mobj)*mobj->momz < 0)
						bubble->momz = mobj->momz >> 4;
					else
						bubble->momz = 0;

					bubble->destscale = mobj->scale;
					P_SetScale(bubble, mobj->scale);
				}
			}
		}

		if (waterwasnotset)
			return;

		// Check to make sure you didn't just cross into a sector to jump out of
		// that has shallower water than the block you were originally in.
		if (diff <= (height >> 1))
		{
			return;
		}

		if (mobj->eflags & MFE_GOOWATER || wasingoo)
		{
			// Decide what happens to your momentum when you enter/leave goopy water.
			if (P_MobjFlip(mobj) * mobj->momz > 0)
			{
				mobj->momz -= (mobj->momz/8); // cut momentum a little bit to prevent multiple bobs
				//CONS_Printf("leaving\n");
			}
			else
			{
				if (!wasgroundpounding)
					mobj->momz >>= 1; // kill momentum significantly, to make the goo feel thick.
				//CONS_Printf("entering\n");
			}
		}
		else if (wasinwater && P_MobjFlip(mobj) * mobj->momz > 0)
		{
			// Give the mobj a little out-of-water boost.
			mobj->momz = FixedMul(mobj->momz, FixedDiv(780*FRACUNIT, 457*FRACUNIT));
		}

		if (mobj->eflags & MFE_VERTICALFLIP)
		{
			thingZ = thingtop - (height >> 1);
			splashValid = (thingZ - mobj->momz <= waterZ);
		}
		else
		{
			thingZ = mobj->z + (height >> 1);
			splashValid = (thingZ - mobj->momz >= waterZ);
		}

		if (P_MobjFlip(mobj) * mobj->momz <= 0)
		{
			if (splashValid == true)
			{
				// Spawn a splash
				mobj_t *splish;
				mobjtype_t splishtype = (mobj->eflags & MFE_TOUCHLAVA) ? MT_LAVASPLISH : MT_SPLISH;

				if (mobj->eflags & MFE_VERTICALFLIP)
				{
					splish = P_SpawnMobj(mobj->x, mobj->y, waterZ - FixedMul(mobjinfo[splishtype].height, mobj->scale), splishtype);
					splish->flags2 |= MF2_OBJECTFLIP;
					splish->eflags |= MFE_VERTICALFLIP;
				}
				else
				{
					splish = P_SpawnMobj(mobj->x, mobj->y, waterZ, splishtype);
				}

				splish->destscale = mobj->scale;
				P_SetScale(splish, mobj->scale);

				// skipping stone!
				if (K_WaterSkip(p) == true)
				{
					const fixed_t hop = 5 * mapobjectscale;

					mobj->momx = (4*mobj->momx)/5;
					mobj->momy = (4*mobj->momy)/5;
					mobj->momz = hop * P_MobjFlip(mobj);

					p->waterskip++;
				}
			}
		}
		else if (P_MobjFlip(mobj) * mobj->momz > 0)
		{
			if (splashValid == true && !(mobj->eflags & MFE_UNDERWATER)) // underwater check to prevent splashes on opposite side
			{
				// Spawn a splash
				mobj_t *splish;
				mobjtype_t splishtype = (mobj->eflags & MFE_TOUCHLAVA) ? MT_LAVASPLISH : MT_SPLISH;

				if (mobj->eflags & MFE_VERTICALFLIP)
				{
					splish = P_SpawnMobj(mobj->x, mobj->y, waterZ - FixedMul(mobjinfo[splishtype].height, mobj->scale), splishtype);
					splish->flags2 |= MF2_OBJECTFLIP;
					splish->eflags |= MFE_VERTICALFLIP;
				}
				else
				{
					splish = P_SpawnMobj(mobj->x, mobj->y, waterZ, splishtype);
				}

				splish->destscale = mobj->scale;
				P_SetScale(splish, mobj->scale);
			}
		}
	}
}

static void P_SceneryCheckWater(mobj_t *mobj)
{
	sector_t *sector;

	// Default if no water exists.
	mobj->watertop = mobj->waterbottom = mobj->z - 1000*FRACUNIT;

	// see if we are in water, and set some flags for later
	sector = mobj->subsector->sector;

	if (sector->ffloors)
	{
		ffloor_t *rover;
		fixed_t topheight, bottomheight;

		mobj->eflags &= ~(MFE_UNDERWATER|MFE_TOUCHWATER);

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS) || !(rover->fofflags & FOF_SWIMMABLE) || rover->fofflags & FOF_BLOCKOTHERS)
				continue;

			topheight    = P_GetFFloorTopZAt   (rover, mobj->x, mobj->y);
			bottomheight = P_GetFFloorBottomZAt(rover, mobj->x, mobj->y);

			if (topheight <= mobj->z
				|| bottomheight > (mobj->z + (mobj->height>>1)))
				continue;

			if (mobj->z + mobj->height > topheight)
				mobj->eflags |= MFE_TOUCHWATER;
			else
				mobj->eflags &= ~MFE_TOUCHWATER;

			// Set the watertop and waterbottom
			mobj->watertop = topheight;
			mobj->waterbottom = bottomheight;

			if (mobj->z + (mobj->height>>1) < topheight)
				mobj->eflags |= MFE_UNDERWATER;
			else
				mobj->eflags &= ~MFE_UNDERWATER;
		}
	}
	else
		mobj->eflags &= ~(MFE_UNDERWATER|MFE_TOUCHWATER);
}

static boolean P_CameraCheckHeat(camera_t *thiscam)
{
	sector_t *sector;
	fixed_t halfheight = thiscam->z + (thiscam->height >> 1);

	// see if we are in water
	sector = thiscam->subsector->sector;

	if (sector->flags & MSF_HEATWAVE)
		return true;

	if (sector->ffloors)
	{
		ffloor_t *rover;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (halfheight >= P_GetFFloorTopZAt(rover, thiscam->x, thiscam->y))
				continue;
			if (halfheight <= P_GetFFloorBottomZAt(rover, thiscam->x, thiscam->y))
				continue;

			if (rover->master->frontsector->flags & MSF_HEATWAVE)
				return true;
		}
	}

	return false;
}

static boolean P_CameraCheckWater(camera_t *thiscam)
{
	sector_t *sector;
	fixed_t halfheight = thiscam->z + (thiscam->height >> 1);

	// see if we are in water
	sector = thiscam->subsector->sector;

	if (sector->ffloors)
	{
		ffloor_t *rover;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS) || !(rover->fofflags & FOF_SWIMMABLE) || rover->fofflags & FOF_BLOCKOTHERS)
				continue;

			if (halfheight >= P_GetFFloorTopZAt(rover, thiscam->x, thiscam->y))
				continue;
			if (halfheight <= P_GetFFloorBottomZAt(rover, thiscam->x, thiscam->y))
				continue;

			return true;
		}
	}

	return false;
}

void P_DestroyRobots(void)
{
	// Search through all the thinkers for enemies.
	mobj_t *mo;
	thinker_t *think;

	for (think = thlist[THINK_MOBJ].next; think != &thlist[THINK_MOBJ]; think = think->next)
	{
		if (think->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mo = (mobj_t *)think;
		if (mo->health <= 0 || !(mo->flags & (MF_ENEMY|MF_BOSS)))
			continue; // not a valid enemy

		if (mo->type == MT_PLAYER) // Don't chase after other players!
			continue;

		// Found a target enemy
		P_KillMobj(mo, players[consoleplayer].mo, players[consoleplayer].mo, DMG_NORMAL);
	}
}

// the below is chasecam only, if you're curious. check out P_CalcPostImg in p_user.c for first person
void P_CalcChasePostImg(player_t *player, camera_t *thiscam)
{
	boolean flipcam = (player->pflags & PF_FLIPCAM && player->mo->eflags & MFE_VERTICALFLIP);
	postimg_t postimg = postimg_none;
	UINT8 i;

	// This can happen when joining
	if (thiscam->subsector == NULL || thiscam->subsector->sector == NULL)
		return;

	if (encoremode && !flipcam)
		postimg = postimg_mirror;
	else if (!encoremode && flipcam)
		postimg = postimg_flip;
	else if (encoremode && flipcam)
		postimg = postimg_mirrorflip;
	else if (player->awayviewtics && player->awayviewmobj && !P_MobjWasRemoved(player->awayviewmobj)) // Camera must obviously exist
	{
		camera_t dummycam;
		dummycam.subsector = player->awayviewmobj->subsector;
		dummycam.x = player->awayviewmobj->x;
		dummycam.y = player->awayviewmobj->y;
		dummycam.z = player->awayviewmobj->z;
		//dummycam.height = 40*FRACUNIT; // alt view height is 20*FRACUNIT
		dummycam.height = 0;			 // Why? Remote viewpoint cameras have no height.
		// Are we in water?
		if (P_CameraCheckWater(&dummycam))
			postimg = postimg_water;
		else if (P_CameraCheckHeat(&dummycam))
			postimg = postimg_heat;
	}
	else
	{
		// Are we in water?
		if (P_CameraCheckWater(thiscam))
			postimg = postimg_water;
		else if (P_CameraCheckHeat(thiscam))
			postimg = postimg_heat;
	}

	if (postimg != postimg_none)
	{
		for (i = 0; i <= r_splitscreen; i++)
		{
			if (player == &players[displayplayers[i]])
				postimgtype[i] = postimg;
		}
	}
}

// P_CameraThinker
//
// Process the mobj-ish required functions of the camera
boolean P_CameraThinker(player_t *player, camera_t *thiscam, boolean resetcalled)
{
	P_CalcChasePostImg(player, thiscam);

	if (thiscam->momx || thiscam->momy)
	{
		if (!P_TryCameraMove(thiscam->x + thiscam->momx, thiscam->y + thiscam->momy, thiscam)) // Thanks for the greatly improved camera, Lach -- Sev
		{ // Never fails for 2D mode.
			mobj_t dummy;
			dummy.thinker.function.acp1 = (actionf_p1)P_MobjThinker;
			dummy.subsector = thiscam->subsector;
			dummy.x = thiscam->x;
			dummy.y = thiscam->y;
			dummy.z = thiscam->z;
			dummy.height = thiscam->height;

			if ((player->pflags & PF_NOCONTEST) && (gametyperules & GTR_CIRCUIT))
			{
				player->karthud[khud_timeovercam] = (2*TICRATE)+1;
			}

			if (!resetcalled && !(player->pflags & PF_NOCLIP || leveltime < introtime) && !P_CheckSight(&dummy, player->mo)) // TODO: "P_CheckCameraSight" instead.
			{
				P_ResetCamera(player, thiscam);
			}
			else
			{
				fixed_t camspeed = P_AproxDistance(thiscam->momx, thiscam->momy);

				P_SlideCameraMove(thiscam);

				if (!resetcalled && P_AproxDistance(thiscam->momx, thiscam->momy) == camspeed)
				{
					P_ResetCamera(player, thiscam);
					resetcalled = true;
				}
			}

			if (resetcalled) // Okay this means the camera is fully reset.
				return true;
		}
	}

	thiscam->subsector = R_PointInSubsector(thiscam->x, thiscam->y);
	thiscam->floorz = tmfloorz;
	thiscam->ceilingz = tmceilingz;

	if (thiscam->momz || player->mo->pmomz)
	{
		// adjust height
		thiscam->z += thiscam->momz + player->mo->pmomz;
	}

	if (thiscam->ceilingz - thiscam->z < thiscam->height
		&& thiscam->ceilingz >= thiscam->z)
	{
		thiscam->ceilingz = thiscam->z + thiscam->height;
		thiscam->floorz = thiscam->z;
	}
	return false;
}

static void P_CheckCrumblingPlatforms(mobj_t *mobj)
{
	msecnode_t *node;

	if (netgame && mobj->player->spectator)
		return;

	for (node = mobj->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		ffloor_t *rover;

		for (rover = node->m_sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (!(rover->fofflags & FOF_CRUMBLE))
				continue;

			if (mobj->eflags & MFE_VERTICALFLIP)
			{
				if (P_GetSpecialBottomZ(mobj, sectors + rover->secnum, node->m_sector) != mobj->z + mobj->height)
					continue;
			}
			else
			{
				if (P_GetSpecialTopZ(mobj, sectors + rover->secnum, node->m_sector) != mobj->z)
					continue;
			}

			EV_StartCrumble(rover->master->frontsector, rover, (rover->fofflags & FOF_FLOATBOB), mobj->player, rover->alpha, !(rover->fofflags & FOF_NORETURN));
		}
	}
}

static boolean P_MobjTouchesSectorWithWater(mobj_t *mobj)
{
	msecnode_t *node;

	for (node = mobj->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		ffloor_t *rover;

		if (!node->m_sector->ffloors)
			continue;

		for (rover = node->m_sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (!(rover->fofflags & FOF_SWIMMABLE))
				continue;

			return true;
		}
	}

	return false;
}

// Check for floating water platforms and bounce them
static void P_CheckFloatbobPlatforms(mobj_t *mobj)
{
	msecnode_t *node;

	// Can't land on anything if you're not moving downwards
	if (P_MobjFlip(mobj)*mobj->momz >= 0)
		return;

	if (!P_MobjTouchesSectorWithWater(mobj))
		return;

	for (node = mobj->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		ffloor_t *rover;

		if (!node->m_sector->ffloors)
			continue;

		for (rover = node->m_sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (!(rover->fofflags & FOF_FLOATBOB))
				continue;


			if (mobj->eflags & MFE_VERTICALFLIP)
			{
				if (abs(*rover->bottomheight - (mobj->z + mobj->height)) > abs(mobj->momz))
					continue;
			}
			else
			{
				if (abs(*rover->topheight - mobj->z) > abs(mobj->momz))
					continue;
			}

			// Initiate a 'bouncy' elevator function which slowly diminishes.
			EV_BounceSector(rover->master->frontsector, -mobj->momz, rover->master);
		}
	}
}

static void P_PlayerMobjThinker(mobj_t *mobj)
{
	I_Assert(mobj != NULL);
	I_Assert(mobj->player != NULL);
	I_Assert(!P_MobjWasRemoved(mobj));

	P_MobjCheckWater(mobj);

	P_ButteredSlope(mobj);

	// momentum movement
	mobj->eflags &= ~MFE_JUSTSTEPPEDDOWN;

	// Zoom tube
	if ((mobj->player->carry == CR_ZOOMTUBE && mobj->tracer && !P_MobjWasRemoved(mobj->tracer)))
	{
		P_HitSpecialLines(mobj, mobj->x, mobj->y, mobj->momx, mobj->momy);
		P_UnsetThingPosition(mobj);
		mobj->x += mobj->momx;
		mobj->y += mobj->momy;
		mobj->z += mobj->momz;
		P_SetThingPosition(mobj);
		P_CheckPosition(mobj, mobj->x, mobj->y);
		mobj->floorz = tmfloorz;
		mobj->ceilingz = tmceilingz;
		mobj->terrain = NULL;
		goto animonly;
	}

	// Needed for gravity boots
	P_CheckGravity(mobj, false);

	if (mobj->momx || mobj->momy)
	{
		P_XYMovement(mobj);

		if (P_MobjWasRemoved(mobj))
			return;
	}
	else
		P_TryMove(mobj, mobj->x, mobj->y, true);

	P_CheckCrumblingPlatforms(mobj);

	if (CheckForFloatBob)
		P_CheckFloatbobPlatforms(mobj);

	// always do the gravity bit now, that's simpler
	// BUT CheckPosition only if wasn't done before.
	if (!(mobj->eflags & MFE_ONGROUND) || mobj->momz
		|| ((mobj->eflags & MFE_VERTICALFLIP) && mobj->z + mobj->height != mobj->ceilingz)
		|| (!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z != mobj->floorz)
		|| P_IsObjectInGoop(mobj))
	{
		P_PlayerZMovement(mobj);
		P_CheckPosition(mobj, mobj->x, mobj->y); // Need this to pick up objects!

		if (P_MobjWasRemoved(mobj))
			return;
	}
	else
	{
		mobj->eflags &= ~MFE_JUSTHITFLOOR;
	}

	K_UpdateTerrainOverlay(mobj);

animonly:
	P_CyclePlayerMobjState(mobj);
}

void P_CalculatePrecipFloor(precipmobj_t *mobj)
{
	// recalculate floorz each time
	const sector_t *mobjsecsubsec;
	boolean setWater = false;

	if (mobj && mobj->subsector && mobj->subsector->sector)
		mobjsecsubsec = mobj->subsector->sector;
	else
		return;

	mobj->precipflags &= ~PCF_INVISIBLE;
	mobj->floorz = P_GetSectorFloorZAt(mobjsecsubsec, mobj->x, mobj->y);
	mobj->ceilingz = P_GetSectorCeilingZAt(mobjsecsubsec, mobj->x, mobj->y);

	if (mobjsecsubsec->ffloors)
	{
		ffloor_t *rover;
		fixed_t height;

		for (rover = mobjsecsubsec->ffloors; rover; rover = rover->next)
		{
			// If it exists, it'll get rained on.
			if (!(rover->fofflags & FOF_EXISTS))
				continue;

			if (precipprops[curWeather].effects & PRECIPFX_WATERPARTICLES)
			{
				if (!(rover->fofflags & FOF_SWIMMABLE))
					continue;

				if (setWater == false)
				{
					mobj->ceilingz = P_GetFFloorTopZAt(rover, mobj->x, mobj->y);
					mobj->floorz = P_GetFFloorBottomZAt(rover, mobj->x, mobj->y);
					setWater = true;
				}
				else
				{
					height = P_GetFFloorTopZAt(rover, mobj->x, mobj->y);
					if (height > mobj->ceilingz)
						mobj->ceilingz = height;

					height = P_GetFFloorBottomZAt(rover, mobj->x, mobj->y);
					if (height < mobj->floorz)
						mobj->floorz = height;
				}
			}
			else
			{
				if (!(rover->fofflags & FOF_BLOCKOTHERS) && !(rover->fofflags & FOF_SWIMMABLE))
					continue;

				height = P_GetFFloorTopZAt(rover, mobj->x, mobj->y);
				if (height > mobj->floorz)
					mobj->floorz = height;
			}
		}
	}

	if ((precipprops[curWeather].effects & PRECIPFX_WATERPARTICLES) && setWater == false)
	{
		mobj->precipflags |= PCF_INVISIBLE;
	}
}

void P_RecalcPrecipInSector(sector_t *sector)
{
	mprecipsecnode_t *psecnode;

	if (!sector)
		return;

	sector->moved = true; // Recalc lighting and things too, maybe

	for (psecnode = sector->touching_preciplist; psecnode; psecnode = psecnode->m_thinglist_next)
		P_CalculatePrecipFloor(psecnode->m_thing);
}

//
// P_NullPrecipThinker
//
// Just the identification of a precip thinker. The thinker
// should never actually be called!

void P_NullPrecipThinker(precipmobj_t *mobj)
{
	(void)mobj;
	I_Assert("P_NullPrecipThinker should not be called" == 0);

}

boolean P_PrecipThinker(precipmobj_t *mobj)
{
	boolean flip = (mobj->precipflags & PCF_FLIP);
	
	if (mobj->lastThink == leveltime)
		return true; // already thinked this tick

	mobj->lastThink = leveltime;

	R_ResetPrecipitationMobjInterpolationState(mobj);

	P_CycleStateAnimation((mobj_t *)mobj);

	if (mobj->state == &states[S_RAINRETURN])
	{
		// Reset to ceiling!
		if (!P_SetPrecipMobjState(mobj, mobj->info->spawnstate))
			return false;
		mobj->z = (flip) ? (mobj->floorz) : (mobj->ceilingz);
		mobj->momz = FixedMul(-mobj->info->speed, mapobjectscale);
		mobj->precipflags &= ~PCF_SPLASH;
		R_ResetPrecipitationMobjInterpolationState(mobj);
	}

	if (mobj->tics != -1)
	{
		if (mobj->tics)
		{
			mobj->tics--;
		}

		if (mobj->tics == 0)
		{
			if ((mobj->precipflags & PCF_SPLASH) && (mobj->state->nextstate == S_NULL))
			{
				// HACK: sprite changes are 1 tic late, so you would see splashes on the ceiling if not for this state.
				// We need to use the settings from the previous state, since some of those are NOT 1 tic late.
				INT32 frame = (mobj->frame & ~FF_FRAMEMASK);
				if (!P_SetPrecipMobjState(mobj, S_RAINRETURN))
					return false;
				mobj->frame = frame;
				return true;
			}
			else
			{
				if (!P_SetPrecipMobjState(mobj, mobj->state->nextstate))
					return true;
			}
		}
	}

	if (mobj->precipflags & PCF_SPLASH)
		return true;

	mobj->z += mobj->momz;

	// adjust height
	if ((flip) ? (mobj->z >= mobj->ceilingz) : (mobj->z <= mobj->floorz))
	{
		if ((mobj->info->deathstate == S_NULL) || (mobj->precipflags & PCF_PIT)) // no splashes on sky or bottomless pits
		{
			mobj->z = (flip) ? (mobj->floorz) : (mobj->ceilingz);
			R_ResetPrecipitationMobjInterpolationState(mobj);
		}
		else
		{
			if (!P_SetPrecipMobjState(mobj, mobj->info->deathstate))
				return false;
			mobj->z = (flip) ? (mobj->ceilingz) : (mobj->floorz);
			mobj->precipflags |= PCF_SPLASH;
			R_ResetPrecipitationMobjInterpolationState(mobj);
		}
	}
	
	return true;
}

static void P_RingThinker(mobj_t *mobj)
{
	mobj_t *spark;	// Ring Fuse

	if (mobj->momx || mobj->momy)
	{
		P_RingXYMovement(mobj);

		if (P_MobjWasRemoved(mobj))
			return;
	}

	// always do the gravity bit now, that's simpler
	// BUT CheckPosition only if wasn't done before.
	if (mobj->momz)
	{
		P_RingZMovement(mobj);
		P_CheckPosition(mobj, mobj->x, mobj->y); // Need this to pick up objects!

		if (P_MobjWasRemoved(mobj))
			return;
	}

	// This thinker splits apart before the regular fuse handling so we need to handle it here instead.
	if (mobj->fuse)
	{
		mobj->fuse--;

		if (mobj->fuse < TICRATE*3)
		{
			if (leveltime & 1)
				mobj->renderflags |= RF_DONTDRAW;
			else
				mobj->renderflags &= ~RF_DONTDRAW;
		}

		if (!mobj->fuse)
		{
			if (!LUA_HookMobj(mobj, MOBJ_HOOK(MobjFuse)))
			{
				mobj->renderflags &= ~RF_DONTDRAW;
				spark = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_SIGNSPARKLE);	// Spawn a fancy sparkle
				K_MatchGenericExtraFlags(spark, mobj);
				spark->colorized = true;
				spark->color = mobj->color ? mobj->color : SKINCOLOR_YELLOW;	// Use yellow if the ring doesn't use a skin color. (It should be red for SPB rings, but let normal rings look fancy too!)
				P_RemoveMobj(mobj);	// Adieu, monde cruel!
				return;
			}

		}

	}

	P_CycleMobjState(mobj);
}

static void P_ItemCapsulePartThinker(mobj_t *mobj)
{
	if (mobj->fuse > 0) // dead
	{
		mobj->fuse--;
		if (mobj->fuse == 0)
		{
			P_RemoveMobj(mobj);
			return;
		}
		mobj->renderflags ^= RF_DONTDRAW;
	}
	else // alive
	{
		mobj_t *target = mobj->target;
		fixed_t targetScale, z;

		if (P_MobjWasRemoved(target))
		{
			P_RemoveMobj(mobj);
			return;
		}

		// match the capsule's scale
		if (mobj->extravalue1)
			targetScale = FixedMul(mobj->extravalue1, target->scale);
		else
			targetScale = target->scale;

		if (mobj->scale != targetScale)
			P_SetScale(mobj, mobj->destscale = targetScale);

		// find z position
		if (mobj->flags2 & MF2_CLASSICPUSH) // centered items should not be flipped
			mobj->renderflags = (mobj->renderflags & ~RF_DONTDRAW) | (target->renderflags & RF_DONTDRAW);
		else
			K_GenericExtraFlagsNoZAdjust(mobj, target);

		if (mobj->eflags & MFE_VERTICALFLIP)
			z = target->z + target->height - mobj->height - FixedMul(mobj->scale, mobj->movefactor);
		else
			z = target->z + FixedMul(mobj->scale, mobj->movefactor);

		// rotate & move to capsule
		mobj->angle += mobj->movedir;
		if (mobj->flags2 & MF2_CLASSICPUSH) // centered
			P_MoveOrigin(mobj, target->x, target->y, z);
		else
			P_MoveOrigin(mobj,
				target->x + P_ReturnThrustX(mobj, mobj->angle + ANGLE_90, mobj->radius),
				target->y + P_ReturnThrustY(mobj, mobj->angle + ANGLE_90, mobj->radius),
				z);
	}
}

static void P_RefreshItemCapsuleParts(mobj_t *mobj)
{
	UINT8 numNumbers = 0;
	INT32 count = 0;
	INT32 itemType = mobj->threshold;
	mobj_t *part;
	skincolornum_t color;
	UINT32 newRenderFlags = 0;
	boolean colorized;

	if (itemType < 1 || itemType >= NUMKARTITEMS)
		itemType = KITEM_SAD;

	// update invincibility properties
	if (itemType == KITEM_INVINCIBILITY)
	{
		mobj->renderflags = (mobj->renderflags & ~RF_BRIGHTMASK) | RF_FULLBRIGHT;
		mobj->colorized = true;
	}
	else
	{
		mobj->renderflags = (mobj->renderflags & ~RF_BRIGHTMASK) | RF_SEMIBRIGHT;
		mobj->color = SKINCOLOR_NONE;
		mobj->colorized = false;
	}

	// update cap colors
	if (itemType == KITEM_SUPERRING)
	{
		color = SKINCOLOR_GOLD;
		newRenderFlags |= RF_SEMIBRIGHT;
	}
	else if (mobj->spawnpoint && (mobj->spawnpoint->args[2] & TMICF_INVERTTIMEATTACK))
		color = SKINCOLOR_SAPPHIRE;
	else if (itemType == KITEM_SPB)
		color = SKINCOLOR_JET;
	else
		color = SKINCOLOR_NONE;

	colorized = (color != SKINCOLOR_NONE);
	part = mobj;
	while (!P_MobjWasRemoved(part->hnext))
	{
		part = part->hnext;
		part->color = color;
		part->colorized = colorized;
		part->renderflags = (part->renderflags & ~RF_BRIGHTMASK) | newRenderFlags;
	}

	// update inside item frame
	part = mobj->tracer;
	if (P_MobjWasRemoved(part))
		return;

	part->threshold = mobj->threshold;
	part->movecount = mobj->movecount;

	switch (itemType)
	{
		case KITEM_ORBINAUT:
			part->sprite = SPR_ITMO;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetOrbinautItemFrame(mobj->movecount);
			break;
		case KITEM_INVINCIBILITY:
			part->sprite = SPR_ITMI;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetInvincibilityItemFrame();
			break;
		case KITEM_SAD:
			part->sprite = SPR_ITEM;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE;
			break;
		default:
			part->sprite = SPR_ITEM;
			part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|(itemType);
			break;
	}

	// update number frame
	if (K_GetShieldFromItem(itemType) != KSHIELD_NONE) // shields don't stack, so don't show a number
		;
	else
	{
		switch (itemType)
		{
			case KITEM_ORBINAUT: // only display the number when the sprite no longer changes
				if (mobj->movecount - 1 > K_GetOrbinautItemFrame(mobj->movecount))
					count = mobj->movecount;
				break;
			case KITEM_SUPERRING: // always display the number, and multiply it by 5
				count = mobj->movecount * 5;
				break;
			case KITEM_SAD: // never display the number
			case KITEM_SPB:
				break;
			default:
				if (mobj->movecount > 1)
					count = mobj->movecount;
				break;
		}
	}

	while (count > 0)
	{
		if (P_MobjWasRemoved(part->tracer))
		{
			P_SetTarget(&part->tracer, P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_OVERLAY));
			P_SetTarget(&part->tracer->target, part);
			P_SetMobjState(part->tracer, S_INVISIBLE);
			part->tracer->spriteyoffset = 10*FRACUNIT;
			part->tracer->spritexoffset = 13*numNumbers*FRACUNIT;
		}
		part = part->tracer;
		part->sprite = SPR_ITMN;
		part->frame = FF_FULLBRIGHT|(count % 10);
		count /= 10;
		numNumbers++;
	}

	// delete any extra overlays (I guess in case the number changes?)
	if (part->tracer)
	{
		P_RemoveMobj(part->tracer);
		P_SetTarget(&part->tracer, NULL);
	}
}

#define CAPSULESIDES 5
#define ANG_CAPSULE (UINT32_MAX / CAPSULESIDES)
#define ROTATIONSPEED (2*ANG2)
static void P_SpawnItemCapsuleParts(mobj_t *mobj)
{
	UINT8 i;
	mobj_t *part;
	fixed_t buttScale = 0;
	statenum_t buttState = S_ITEMCAPSULE_BOTTOM_SIDE_AIR;
	angle_t spin = ANGLE_MAX - ROTATIONSPEED;

	if (P_IsObjectOnGround(mobj))
	{
		buttScale = 13*FRACUNIT/10;
		buttState = S_ITEMCAPSULE_BOTTOM_SIDE_GROUND;
		spin = 0;
	}

	// inside item
	part = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_ITEMCAPSULE_PART);
	P_SetTarget(&part->target, mobj);
	P_SetMobjState(part, S_ITEMICON);
	part->movedir = ROTATIONSPEED; // rotation speed
	part->extravalue1 = 175*FRACUNIT/100; // relative scale
	part->flags2 |= MF2_CLASSICPUSH; // classicpush = centered horizontally
	part->flags2 &= ~MF2_OBJECTFLIP; // centered item should not be flipped
	part->eflags &= ~MFE_VERTICALFLIP;
	P_SetTarget(&mobj->tracer, part); // pointer to this item, so we can modify its sprite/frame

	// capsule caps
	part = mobj;
	for (i = 0; i < CAPSULESIDES; i++)
	{
		// a bottom side
		P_SetTarget(&part->hnext, P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_ITEMCAPSULE_PART));
		P_SetTarget(&part->hnext->hprev, part);
		part = part->hnext;
		P_SetTarget(&part->target, mobj);
		P_SetMobjState(part, buttState);
		P_InitAngle(part, i * ANG_CAPSULE);
		part->movedir = spin; // rotation speed
		part->movefactor = 0; // z offset
		part->extravalue1 = buttScale; // relative scale

		// a top side
		P_SetTarget(&part->hnext, P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_ITEMCAPSULE_PART));
		P_SetTarget(&part->hnext->hprev, part);
		part = part->hnext;
		P_SetTarget(&part->target, mobj);
		P_SetMobjState(part, S_ITEMCAPSULE_TOP_SIDE);
		P_InitAngle(part, i * ANG_CAPSULE);
		part->movedir = spin; // rotation speed
		part->movefactor = mobj->info->height - part->info->height; // z offset
	}

	P_RefreshItemCapsuleParts(mobj);
}
#undef CAPSULESIDES
#undef ANG_CAPSULE
#undef ROTATIONSPEED

//
// P_BossTargetPlayer
// If closest is true, find the closest player.
// Returns true if a player is targeted.
//
boolean P_BossTargetPlayer(mobj_t *actor, boolean closest)
{
	INT32 stop = -1, c = 0;
	player_t *player;
	fixed_t dist, lastdist = 0;

	// first time init, this allow minimum lastlook changes
	if (actor->lastlook < 0)
		actor->lastlook = P_RandomByte();
	actor->lastlook &= PLAYERSMASK;

	for( ; ; actor->lastlook = (actor->lastlook+1) & PLAYERSMASK)
	{
		// save the first look so we stop next time.
		if (stop < 0)
			stop = actor->lastlook;
		// reached the beginning again, done looking.
		else if (actor->lastlook == stop)
			return (closest && lastdist > 0);

		if (!playeringame[actor->lastlook])
			continue;

		if (!closest && c++ == 2)
			return false;

		player = &players[actor->lastlook];

		if (player->spectator)
			continue; // ignore notarget

		if (!player->mo || P_MobjWasRemoved(player->mo))
			continue;

		if (player->mo->health <= 0)
			continue; //dead

		if (!P_CheckSight(actor, player->mo))
			continue; // out of sight

		if (closest)
		{
			dist = P_AproxDistance(actor->x - player->mo->x, actor->y - player->mo->y);
			if (!lastdist || dist < lastdist)
			{
				lastdist = dist+1;
				P_SetTarget(&actor->target, player->mo);
			}
			continue;
		}

		P_SetTarget(&actor->target, player->mo);
		return true;
	}
}

// Finds the player no matter what they're hiding behind (even lead!)
boolean P_SupermanLook4Players(mobj_t *actor)
{
	INT32 c, stop = 0;
	player_t *playersinthegame[MAXPLAYERS];

	for (c = 0; c < MAXPLAYERS; c++)
	{
		if (playeringame[c] && !players[c].spectator)
		{
			if (!players[c].mo || players[c].bot)
				continue;

			if (players[c].mo->health <= 0)
				continue; // dead

			if ((gametyperules & GTR_BUMPERS) && players[c].bumpers <= 0)
				continue; // other dead

			playersinthegame[stop] = &players[c];
			stop++;
		}
	}

	if (!stop)
		return false;

	P_SetTarget(&actor->target, playersinthegame[P_RandomKey(stop)]->mo);
	return true;
}

// AI for a generic boss.
static void P_GenericBossThinker(mobj_t *mobj)
{
	if (mobj->state->nextstate == mobj->info->spawnstate && mobj->tics == 1)
		mobj->flags2 &= ~MF2_FRET;

	if (!mobj->target || !(mobj->target->flags & MF_SHOOTABLE))
	{
		if (mobj->health <= 0)
			return;

		// look for a new target
		if (P_BossTargetPlayer(mobj, false) && mobj->info->seesound)
			S_StartSound(mobj, mobj->info->seesound);

		return;
	}

	// Don't call A_ functions here, let the SOC do the AI!

	if (mobj->state == &states[mobj->info->meleestate]
		|| (mobj->state == &states[mobj->info->missilestate]
		&& mobj->health > mobj->info->damage))
	{
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->target->x, mobj->target->y);
	}
}

//
// P_GetClosestAxis
//
// Finds the CLOSEST axis to the source mobj
mobj_t *P_GetClosestAxis(mobj_t *source)
{
	thinker_t *th;
	mobj_t *mo2;
	mobj_t *closestaxis = NULL;
	fixed_t dist1, dist2 = 0;

	// scan the thinkers to find the closest axis point
	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2->type == MT_AXIS)
		{
			if (closestaxis == NULL)
			{
				closestaxis = mo2;
				dist2 = R_PointToDist2(source->x, source->y, mo2->x, mo2->y)-mo2->radius;
			}
			else
			{
				dist1 = R_PointToDist2(source->x, source->y, mo2->x, mo2->y)-mo2->radius;

				if (dist1 < dist2)
				{
					closestaxis = mo2;
					dist2 = dist1;
				}
			}
		}
	}

	if (closestaxis == NULL)
		CONS_Debug(DBG_NIGHTS, "ERROR: No axis points found!\n");

	return closestaxis;
}

static void P_GimmeAxisXYPos(mobj_t *closestaxis, degenmobj_t *mobj)
{
	const angle_t fa = R_PointToAngle2(closestaxis->x, closestaxis->y, mobj->x, mobj->y)>>ANGLETOFINESHIFT;

	mobj->x = closestaxis->x + FixedMul(FINECOSINE(fa),closestaxis->radius);
	mobj->y = closestaxis->y + FixedMul(FINESINE(fa),closestaxis->radius);
}

static void P_MoveHoop(mobj_t *mobj)
{
	const fixed_t fuse = (mobj->fuse*mobj->extravalue2);
	const angle_t fa = mobj->movedir*(FINEANGLES/mobj->extravalue1);
	TVector v;
	TVector *res;
	fixed_t finalx, finaly, finalz;
	fixed_t x, y, z;

	//I_Assert(mobj->target != NULL);
	if (!mobj->target) /// \todo DEBUG ME! Target was P_RemoveMobj'd at some point, and therefore no longer valid!
		return;

	x = mobj->target->x;
	y = mobj->target->y;
	z = mobj->target->z+mobj->target->height/2;

	// Make the sprite travel towards the center of the hoop
	v[0] = FixedMul(FINECOSINE(fa),fuse);
	v[1] = 0;
	v[2] = FixedMul(FINESINE(fa),fuse);
	v[3] = FRACUNIT;

	res = VectorMatrixMultiply(v, *RotateXMatrix(FixedAngle(mobj->target->movedir*FRACUNIT)));
	M_Memcpy(&v, res, sizeof (v));
	res = VectorMatrixMultiply(v, *RotateZMatrix(FixedAngle(mobj->target->movecount*FRACUNIT)));
	M_Memcpy(&v, res, sizeof (v));

	finalx = x + v[0];
	finaly = y + v[1];
	finalz = z + v[2];

	P_UnsetThingPosition(mobj);
	mobj->x = finalx;
	mobj->y = finaly;
	P_SetThingPosition(mobj);
	mobj->z = finalz - mobj->height/2;
}

void P_SpawnHoopOfSomething(fixed_t x, fixed_t y, fixed_t z, fixed_t radius, INT32 number, mobjtype_t type, angle_t rotangle)
{
	mobj_t *mobj;
	INT32 i;
	TVector v;
	TVector *res;
	fixed_t finalx, finaly, finalz;
	mobj_t hoopcenter;
	mobj_t *axis;
	degenmobj_t xypos;
	angle_t degrees, fa, closestangle;

	hoopcenter.x = x;
	hoopcenter.y = y;
	hoopcenter.z = z;

	axis = P_GetClosestAxis(&hoopcenter);

	if (!axis)
	{
		CONS_Debug(DBG_NIGHTS, "You forgot to put axis points in the map!\n");
		return;
	}

	xypos.x = x;
	xypos.y = y;

	P_GimmeAxisXYPos(axis, &xypos);

	x = xypos.x;
	y = xypos.y;

	hoopcenter.z = z - mobjinfo[type].height/2;

	hoopcenter.x = x;
	hoopcenter.y = y;

	closestangle = R_PointToAngle2(x, y, axis->x, axis->y);

	degrees = FINEANGLES/number;

	radius >>= FRACBITS;

	// Create the hoop!
	for (i = 0; i < number; i++)
	{
		fa = (i*degrees);
		v[0] = FixedMul(FINECOSINE(fa),radius);
		v[1] = 0;
		v[2] = FixedMul(FINESINE(fa),radius);
		v[3] = FRACUNIT;

		res = VectorMatrixMultiply(v, *RotateXMatrix(rotangle));
		M_Memcpy(&v, res, sizeof (v));
		res = VectorMatrixMultiply(v, *RotateZMatrix(closestangle));
		M_Memcpy(&v, res, sizeof (v));

		finalx = x + v[0];
		finaly = y + v[1];
		finalz = z + v[2];

		mobj = P_SpawnMobj(finalx, finaly, finalz, type);
		mobj->z -= mobj->height/2;
	}
}

void P_SpawnParaloop(fixed_t x, fixed_t y, fixed_t z, fixed_t radius, INT32 number, mobjtype_t type, statenum_t nstate, angle_t rotangle, boolean spawncenter)
{
	mobj_t *mobj;
	INT32 i;
	TVector v;
	TVector *res;
	fixed_t finalx, finaly, finalz, dist;
	angle_t degrees, fa, closestangle;
	fixed_t mobjx, mobjy, mobjz;

	degrees = FINEANGLES/number;

	radius = FixedDiv(radius,5*(FRACUNIT/4));

	closestangle = 0;

	// Create the hoop!
	for (i = 0; i < number; i++)
	{
		fa = (i*degrees);
		v[0] = FixedMul(FINECOSINE(fa),radius);
		v[1] = 0;
		v[2] = FixedMul(FINESINE(fa),radius);
		v[3] = FRACUNIT;

		res = VectorMatrixMultiply(v, *RotateXMatrix(rotangle));
		M_Memcpy(&v, res, sizeof (v));
		res = VectorMatrixMultiply(v, *RotateZMatrix(closestangle));
		M_Memcpy(&v, res, sizeof (v));

		finalx = x + v[0];
		finaly = y + v[1];
		finalz = z + v[2];

		mobj = P_SpawnMobj(finalx, finaly, finalz, type);

		mobj->z -= mobj->height>>1;

		// change angle
		P_InitAngle(mobj, R_PointToAngle2(mobj->x, mobj->y, x, y));

		// change slope
		dist = P_AproxDistance(P_AproxDistance(x - mobj->x, y - mobj->y), z - mobj->z);

		if (dist < 1)
			dist = 1;

		mobjx = mobj->x;
		mobjy = mobj->y;
		mobjz = mobj->z;

		// set to special state
		if (nstate != S_NULL)
			P_SetMobjState(mobj, nstate);

		mobj->momx = FixedMul(FixedDiv(x - mobjx, dist), 5*FRACUNIT);
		mobj->momy = FixedMul(FixedDiv(y - mobjy, dist), 5*FRACUNIT);
		mobj->momz = FixedMul(FixedDiv(z - mobjz, dist), 5*FRACUNIT);
		mobj->fuse = (radius>>(FRACBITS+2)) + 1;

		if (spawncenter)
		{
			mobj->x = x;
			mobj->y = y;
			mobj->z = z;
		}

		if (mobj->fuse <= 1)
			mobj->fuse = 2;

		mobj->flags |= MF_NOCLIPTHING;
		mobj->flags &= ~MF_SPECIAL;

		if (mobj->fuse > 7)
			mobj->tics = mobj->fuse - 7;
		else
			mobj->tics = 1;
	}
}

//
// P_SetScale
//
// Sets the sprite scaling
//
void P_SetScale(mobj_t *mobj, fixed_t newscale)
{
	player_t *player;
	fixed_t oldscale;

	if (!mobj)
		return;

	oldscale = mobj->scale; //keep for adjusting stuff below

	mobj->scale = newscale;

	mobj->radius = FixedMul(FixedDiv(mobj->radius, oldscale), newscale);
	mobj->height = FixedMul(FixedDiv(mobj->height, oldscale), newscale);

	player = mobj->player;

	if (player)
	{
		G_GhostAddScale((INT32) (player - players), newscale);
		player->viewheight = FixedMul(FixedDiv(player->viewheight, oldscale), newscale); // Nonono don't calculate viewheight elsewhere, this is the best place for it!
	}
}

void P_Attract(mobj_t *source, mobj_t *dest, boolean nightsgrab) // Home in on your target
{
	fixed_t dist, ndist, speedmul;
	angle_t vangle;
	fixed_t tx = dest->x;
	fixed_t ty = dest->y;
	fixed_t tz = dest->z + (dest->height/2); // Aim for center
	fixed_t xydist = P_AproxDistance(tx - source->x, ty - source->y);

	if (!dest || dest->health <= 0 || !dest->player || !source->tracer)
		return;

	// change angle
	//source->angle = R_PointToAngle2(source->x, source->y, tx, ty);

	// change slope
	dist = P_AproxDistance(xydist, tz - source->z);

	if (dist < 1)
		dist = 1;

	if (nightsgrab && source->movefactor)
	{
		source->movefactor += FRACUNIT/2;

		if (dist < source->movefactor)
		{
			source->momx = source->momy = source->momz = 0;
			P_MoveOrigin(source, tx, ty, tz);
		}
		else
		{
			vangle = R_PointToAngle2(source->z, 0, tz, xydist);

			source->momx = FixedMul(FINESINE(vangle >> ANGLETOFINESHIFT), FixedMul(FINECOSINE(source->angle >> ANGLETOFINESHIFT), source->movefactor));
			source->momy = FixedMul(FINESINE(vangle >> ANGLETOFINESHIFT), FixedMul(FINESINE(source->angle >> ANGLETOFINESHIFT), source->movefactor));
			source->momz = FixedMul(FINECOSINE(vangle >> ANGLETOFINESHIFT), source->movefactor);
		}
	}
	else
	{
		if (nightsgrab)
			speedmul = P_AproxDistance(dest->momx, dest->momy) + FixedMul(8*FRACUNIT, source->scale);
		else
			speedmul = P_AproxDistance(dest->momx, dest->momy) + FixedMul(source->info->speed, source->scale);

		source->momx = FixedMul(FixedDiv(tx - source->x, dist), speedmul);
		source->momy = FixedMul(FixedDiv(ty - source->y, dist), speedmul);
		source->momz = FixedMul(FixedDiv(tz - source->z, dist), speedmul);
	}

	// Instead of just unsetting NOCLIP like an idiot, let's check the distance to our target.
	ndist = P_AproxDistance(P_AproxDistance(tx - (source->x+source->momx),
	                                        ty - (source->y+source->momy)),
	                                        tz - (source->z+source->momz));

	if (ndist > dist) // gone past our target
	{
		// place us on top of them then.
		source->momx = source->momy = source->momz = 0;
		P_UnsetThingPosition(source);
		source->x = tx;
		source->y = ty;
		source->z = tz;
		P_SetThingPosition(source);
	}
}

static void P_NightsItemChase(mobj_t *thing)
{
	if (!thing->tracer)
	{
		P_SetTarget(&thing->tracer, NULL);
		thing->flags2 &= ~MF2_NIGHTSPULL;
		thing->movefactor = 0;
		return;
	}

	if (!thing->tracer->player)
		return;

	P_Attract(thing, thing->tracer, true);
}

//
// P_MaceRotate
// Spins a hnext-chain of objects around its centerpoint, side to side or periodically.
//
void P_MaceRotate(mobj_t *center, INT32 baserot, INT32 baseprevrot)
{
	TVector unit_lengthways, unit_sideways, pos_lengthways, pos_sideways;
	TVector *res;
	fixed_t radius, dist, zstore;
	angle_t fa;
	boolean dosound = false;
	mobj_t *mobj = center->hnext, *hnext = NULL;

	INT32 lastthreshold = -1; // needs to never be equal at start of loop
	fixed_t lastfriction = INT32_MIN; // ditto; almost certainly never, but...

	INT32 rot;
	INT32 prevrot;

	dist = pos_sideways[0] = pos_sideways[1] = pos_sideways[2] = pos_sideways[3] = unit_sideways[3] =\
	 pos_lengthways[0] = pos_lengthways[1] = pos_lengthways[2] = pos_lengthways[3] = 0;

	while (mobj)
	{
		if (P_MobjWasRemoved(mobj) || !mobj->health)
		{
			mobj = mobj->hnext;
			continue;
		}

		mobj->momx = mobj->momy = mobj->momz = 0;

		if (mobj->threshold != lastthreshold
		|| mobj->friction != lastfriction)
		{
			rot = (baserot + mobj->threshold) & FINEMASK;
			prevrot = (baseprevrot + mobj->threshold) & FINEMASK;

			pos_lengthways[0] = pos_lengthways[1] = pos_lengthways[2] = pos_lengthways[3] = 0;

			dist = ((mobj->info->speed) ? mobj->info->speed : mobjinfo[MT_SMALLMACECHAIN].speed);
			dist = ((center->scale == FRACUNIT) ? dist : FixedMul(dist, center->scale));

			fa = (FixedAngle(center->movefactor*FRACUNIT) >> ANGLETOFINESHIFT);
			radius = FixedMul(dist, FINECOSINE(fa));
			unit_lengthways[1] = -FixedMul(dist, FINESINE(fa));
			unit_lengthways[3] = FRACUNIT;

			// Swinging Chain.
			if (center->flags2 & MF2_STRONGBOX)
			{
				fixed_t swingmag = FixedMul(FINECOSINE(rot), center->lastlook << FRACBITS);
				fixed_t prevswingmag = FINECOSINE(prevrot);

				if ((prevswingmag > 0) != (swingmag > 0)) // just passed its lowest point
					dosound = true;

				fa = ((FixedAngle(swingmag) >> ANGLETOFINESHIFT) + mobj->friction) & FINEMASK;

				unit_lengthways[0] = FixedMul(FINESINE(fa), -radius);
				unit_lengthways[2] = FixedMul(FINECOSINE(fa), -radius);
			}
			// Rotating Chain.
			else
			{
				angle_t prevfa = (prevrot + mobj->friction) & FINEMASK;
				fa = (rot + mobj->friction) & FINEMASK;

				// completed a half-spin
				dosound = ((prevfa > (FINEMASK/2)) != (fa > (FINEMASK/2)));

				unit_lengthways[0] = FixedMul(FINECOSINE(fa), radius);
				unit_lengthways[2] = FixedMul(FINESINE(fa), radius);
			}

			// Calculate the angle matrixes for the link.
			res = VectorMatrixMultiply(unit_lengthways, *RotateXMatrix(center->threshold << ANGLETOFINESHIFT));
			M_Memcpy(&unit_lengthways, res, sizeof(unit_lengthways));
			res = VectorMatrixMultiply(unit_lengthways, *RotateZMatrix(center->angle));
			M_Memcpy(&unit_lengthways, res, sizeof(unit_lengthways));

			lastthreshold = mobj->threshold;
			lastfriction = mobj->friction;
		}

		if (dosound && (mobj->flags2 & MF2_BOSSNOTRAP))
		{
			S_StartSound(mobj, mobj->info->activesound);
			dosound = false;
		}

		if (pos_sideways[3] != mobj->movefactor)
		{
			if (!unit_sideways[3])
			{
				unit_sideways[1] = dist;
				unit_sideways[0] = unit_sideways[2] = 0;
				unit_sideways[3] = FRACUNIT;

				res = VectorMatrixMultiply(unit_sideways, *RotateXMatrix(center->threshold << ANGLETOFINESHIFT));
				M_Memcpy(&unit_sideways, res, sizeof(unit_sideways));
				res = VectorMatrixMultiply(unit_sideways, *RotateZMatrix(center->angle));
				M_Memcpy(&unit_sideways, res, sizeof(unit_sideways));
			}

			if (pos_sideways[3] > mobj->movefactor)
			{
				do
				{
					pos_sideways[0] -= unit_sideways[0];
					pos_sideways[1] -= unit_sideways[1];
					pos_sideways[2] -= unit_sideways[2];
				}
				while ((--pos_sideways[3]) != mobj->movefactor);
			}
			else
			{
				do
				{
					pos_sideways[0] += unit_sideways[0];
					pos_sideways[1] += unit_sideways[1];
					pos_sideways[2] += unit_sideways[2];
				}
				while ((++pos_sideways[3]) != mobj->movefactor);
			}
		}

		hnext = mobj->hnext; // just in case the mobj is removed

		if (pos_lengthways[3] > mobj->movecount)
		{
			do
			{
				pos_lengthways[0] -= unit_lengthways[0];
				pos_lengthways[1] -= unit_lengthways[1];
				pos_lengthways[2] -= unit_lengthways[2];
			}
			while ((--pos_lengthways[3]) != mobj->movecount);
		}
		else if (pos_lengthways[3] < mobj->movecount)
		{
			do
			{
				pos_lengthways[0] += unit_lengthways[0];
				pos_lengthways[1] += unit_lengthways[1];
				pos_lengthways[2] += unit_lengthways[2];
			}
			while ((++pos_lengthways[3]) != mobj->movecount);
		}

		P_UnsetThingPosition(mobj);

		mobj->x = center->x;
		mobj->y = center->y;
		mobj->z = center->z;

		// Add on the appropriate distances to the center's co-ordinates.
		if (pos_lengthways[3])
		{
			mobj->x += pos_lengthways[0];
			mobj->y += pos_lengthways[1];
			zstore = pos_lengthways[2] + pos_sideways[2];
		}
		else
			zstore = pos_sideways[2];

		mobj->x += pos_sideways[0];
		mobj->y += pos_sideways[1];

		// Cut the height to align the link with the axis.
		if (mobj->type == MT_SMALLMACECHAIN || mobj->type == MT_BIGMACECHAIN || mobj->type == MT_SMALLGRABCHAIN || mobj->type == MT_BIGGRABCHAIN)
			zstore -= P_MobjFlip(mobj)*mobj->height/4;
		else
			zstore -= P_MobjFlip(mobj)*mobj->height/2;

		mobj->z += zstore;

#if 0 // toaster's testing flashie!
		if (!(mobj->movecount & 1) && !(leveltime & TICRATE)) // I had a brainfart and the flashing isn't exactly what I expected it to be, but it's actually much more useful.
			mobj->renderflags ^= RF_DONTDRAW;
#endif

		P_SetThingPosition(mobj);

#if 0 // toaster's height-clipping dealie!
		if (!pos_lengthways[3] || P_MobjWasRemoved(mobj) || (mobj->flags & MF_NOCLIPHEIGHT))
			goto cont;

		if ((fa = ((center->threshold & (FINEMASK/2)) << ANGLETOFINESHIFT)) > ANGLE_45 && fa < ANGLE_135) // only move towards center when the motion is towards/away from the ground, rather than alongside it
			goto cont;

		if (mobj->subsector->sector->ffloors)
			P_AdjustMobjFloorZ_FFloors(mobj, mobj->subsector->sector, 2);

		if (mobj->floorz > mobj->z)
			zstore = (mobj->floorz - zstore);
		else if (mobj->ceilingz < mobj->z)
			zstore = (mobj->ceilingz - mobj->height - zstore);
		else
			goto cont;

		zstore = FixedDiv(zstore, dist); // Still needs work... scaling factor is wrong!

		P_UnsetThingPosition(mobj);

		mobj->x -= FixedMul(unit_lengthways[0], zstore);
		mobj->y -= FixedMul(unit_lengthways[1], zstore);

		P_SetThingPosition(mobj);

cont:
#endif
		mobj = hnext;
	}
}

// Kartitem stuff.

// This item is never attached to a player -- it can DIE
// unconditionally in death sectors.
boolean P_IsKartFieldItem(INT32 type)
{
	switch (type)
	{
		case MT_BANANA:
		case MT_EGGMANITEM:
		case MT_ORBINAUT:
		case MT_JAWZ:
		case MT_JAWZ_DUD:
		case MT_SSMINE:
		case MT_LANDMINE:
		case MT_BALLHOG:
		case MT_BUBBLESHIELDTRAP:
		case MT_POGOSPRING:
		case MT_SINK:
		case MT_DROPTARGET:
			return true;

		default:
			return false;
	}
}

boolean P_IsKartItem(INT32 type)
{
	switch (type)
	{
		case MT_EGGMANITEM_SHIELD:
		case MT_BANANA_SHIELD:
		case MT_DROPTARGET_SHIELD:
		case MT_ORBINAUT_SHIELD:
		case MT_JAWZ_SHIELD:
		case MT_SSMINE_SHIELD:
		case MT_SINK_SHIELD:
			return true;

		// Primarily for minimap data, handle with care
		case MT_SPB:
		case MT_BATTLECAPSULE:
			return true;

		default:
			return P_IsKartFieldItem(type);
	}
}

boolean K_IsMissileOrKartItem(mobj_t *mo)
{
	if (mo->flags & MF_MISSILE)
	{
		// It's already a missile!
		return true;
	}

	if (mo->type == MT_SPB)
	{
		// Not considered a field item, so manually include.
		return true;
	}

	return P_IsKartFieldItem(mo->type);
}

// This item can die in death sectors. There may be some
// special conditions for items that don't switch types...
// TODO: just make a general function for things that should
// die like this?
boolean P_CanDeleteKartItem(INT32 type)
{
	return P_IsKartFieldItem(type);
}

// Called when a kart item "thinks"
void P_AddKartItem(mobj_t *thing)
{
	I_Assert(thing != NULL);

	if (kitemcap == NULL)
		P_SetTarget(&kitemcap, thing);
	else {
		mobj_t *mo;
		for (mo = kitemcap; mo && mo->itnext; mo = mo->itnext)
			;

		I_Assert(mo != NULL);
		I_Assert(mo->itnext == NULL);

		P_SetTarget(&mo->itnext, thing);
	}
	P_SetTarget(&thing->itnext, NULL);
}

// Called only when a kart item is removed
// Keeps the hnext list from corrupting.
static void P_RemoveKartItem(mobj_t *thing)
{
	mobj_t *mo;
	for (mo = kitemcap; mo; mo = mo->itnext)
	{
		if (mo->itnext != thing)
			continue;

		P_SetTarget(&mo->itnext, thing->itnext);
		P_SetTarget(&thing->itnext, NULL);
		return;
	}
}

// Doesn't actually do anything since items have their own thinkers,
// but this is necessary for the sole purpose of updating kitemcap
void P_RunKartItems(void)
{
	mobj_t *mobj, *next;

	for (mobj = kitemcap; mobj; mobj = next)
	{
		next = mobj->itnext;
		P_SetTarget(&mobj->itnext, NULL);
	}
	P_SetTarget(&kitemcap, NULL);
}

void P_RunOverlays(void)
{
	// run overlays
	mobj_t *mo, *next = NULL;
	fixed_t destx,desty,zoffs;

	for (mo = overlaycap; mo; mo = next)
	{
		I_Assert(!P_MobjWasRemoved(mo));

		// grab next in chain, then unset the chain target
		next = mo->hnext;
		P_SetTarget(&mo->hnext, NULL);

		if (!mo->target)
			continue;

		if (P_MobjWasRemoved(mo->target))
		{
			P_RemoveMobj(mo);
			continue;
		}

		if (!r_splitscreen /*&& rendermode != render_soft*/)
		{
			angle_t viewingangle;

			if (players[displayplayers[0]].awayviewtics)
				viewingangle = R_PointToAngle2(mo->target->x, mo->target->y, players[displayplayers[0]].awayviewmobj->x, players[displayplayers[0]].awayviewmobj->y);
			else if (!camera[0].chase && players[displayplayers[0]].mo)
				viewingangle = R_PointToAngle2(mo->target->x, mo->target->y, players[displayplayers[0]].mo->x, players[displayplayers[0]].mo->y);
			else
				viewingangle = R_PointToAngle2(mo->target->x, mo->target->y, camera[0].x, camera[0].y);

			if (!(mo->state->frame & FF_ANIMATE) && mo->state->var1)
				viewingangle += ANGLE_180;
			destx = mo->target->x + P_ReturnThrustX(mo->target, viewingangle, FixedMul(FRACUNIT/4, mo->scale));
			desty = mo->target->y + P_ReturnThrustY(mo->target, viewingangle, FixedMul(FRACUNIT/4, mo->scale));
		}
		else
		{
			destx = mo->target->x;
			desty = mo->target->y;
		}

		mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP) | (mo->target->eflags & MFE_VERTICALFLIP);
		mo->scale = mo->destscale = FixedMul(mo->target->scale, mo->movefactor);
		mo->angle = (mo->target->player ? mo->target->player->drawangle : mo->target->angle) + mo->movedir;
		mo->rollangle = mo->target->rollangle;
		mo->pitch = mo->target->pitch;
		mo->roll = mo->target->roll;

		if ((mo->flags & MF_DONTENCOREMAP) != (mo->target->flags & MF_DONTENCOREMAP))
			mo->flags ^= MF_DONTENCOREMAP;

		if (!(mo->state->frame & FF_ANIMATE))
			zoffs = FixedMul(((signed)mo->state->var2)*FRACUNIT, mo->scale);
		// if you're using FF_ANIMATE on an overlay,
		// then you're on your own.
		else
			zoffs = 0;

		P_UnsetThingPosition(mo);
		mo->x = destx;
		mo->y = desty;
		mo->radius = mo->target->radius;
		mo->height = mo->target->height;
		if (mo->eflags & MFE_VERTICALFLIP)
			mo->z = (mo->target->z + mo->target->height - mo->height) - zoffs;
		else
			mo->z = mo->target->z + zoffs;
		if (mo->state->var1)
			P_SetUnderlayPosition(mo);
		else
			P_SetThingPosition(mo);
		P_CheckPosition(mo, mo->x, mo->y);
	}
	P_SetTarget(&overlaycap, NULL);
}

// Called only when MT_OVERLAY thinks.
static void P_AddOverlay(mobj_t *thing)
{
	I_Assert(thing != NULL);

	if (overlaycap == NULL)
		P_SetTarget(&overlaycap, thing);
	else {
		mobj_t *mo;
		for (mo = overlaycap; mo && mo->hnext; mo = mo->hnext)
			;

		I_Assert(mo != NULL);
		I_Assert(mo->hnext == NULL);

		P_SetTarget(&mo->hnext, thing);
	}
	P_SetTarget(&thing->hnext, NULL);
}

// Called only when MT_OVERLAY (or anything else in the overlaycap list) is removed.
// Keeps the hnext list from corrupting.
static void P_RemoveOverlay(mobj_t *thing)
{
	mobj_t *mo;
	for (mo = overlaycap; mo; mo = mo->hnext)
	{
		if (mo->hnext != thing)
			continue;

		P_SetTarget(&mo->hnext, thing->hnext);
		P_SetTarget(&thing->hnext, NULL);
		return;
	}
}

static void P_MobjScaleThink(mobj_t *mobj)
{
	fixed_t oldheight = mobj->height;
	UINT8 correctionType = 0; // Don't correct Z position, just gain height

	if (mobj->flags & MF_NOCLIPHEIGHT || (mobj->z > mobj->floorz && mobj->z + mobj->height < mobj->ceilingz))
		correctionType = 1; // Correct Z position by centering
	else if (mobj->eflags & MFE_VERTICALFLIP)
		correctionType = 2; // Correct Z position by moving down

	if (abs(mobj->scale - mobj->destscale) < mobj->scalespeed)
		P_SetScale(mobj, mobj->destscale);
	else if (mobj->scale < mobj->destscale)
		P_SetScale(mobj, mobj->scale + mobj->scalespeed);
	else if (mobj->scale > mobj->destscale)
		P_SetScale(mobj, mobj->scale - mobj->scalespeed);

	if (correctionType == 1)
		mobj->z -= (mobj->height - oldheight)/2;
	else if (correctionType == 2)
		mobj->z -= mobj->height - oldheight;

	if (mobj->scale == mobj->destscale)
	{
		/// \todo Lua hook for "reached destscale"?
		switch (mobj->type)
		{
			default:
				if (mobj->player)
				{
					if (mobj->scale <= 1)
					{
						mobj->renderflags |= RF_DONTDRAW;
					}
				}
				else
				{
					if (!mobj->player && mobj->scale == 0)
					{
						P_RemoveMobj(mobj);
						return;
					}
				}
				break;
		}
	}
}

static void P_MaceSceneryThink(mobj_t *mobj)
{
	angle_t oldmovedir = mobj->movedir;

	// Always update movedir to prevent desyncing (in the traditional sense, not the netplay sense).
	mobj->movedir = (mobj->movedir + mobj->lastlook) & FINEMASK;

	// If too far away and not deliberately spitting in the face of optimisation, don't think!
	if (!(mobj->flags2 & MF2_BOSSNOTRAP))
	{
		UINT8 i;
		// Quick! Look through players! Don't move unless a player is relatively close by.
		// The below is selected based on CEZ2's first room. I promise you it is a coincidence that it looks like the weed number.
		for (i = 0; i < MAXPLAYERS; ++i)
			if (playeringame[i] && players[i].mo
				&& P_AproxDistance(P_AproxDistance(mobj->x - players[i].mo->x, mobj->y - players[i].mo->y), mobj->z - players[i].mo->z) < (4200 << FRACBITS))
				break; // Stop looking.
		if (i == MAXPLAYERS)
		{
			if (!(mobj->flags2 & MF2_BEYONDTHEGRAVE))
			{
				mobj_t *ref = mobj;

				// stop/hide all your babies
				while ((ref = ref->hnext))
				{
					ref->eflags = (((ref->flags & MF_NOTHINK) ? 0 : 1)
						| ((ref->flags & MF_NOCLIPTHING) ? 0 : 2)
						| ((ref->renderflags & RF_DONTDRAW) ? 0 : 4)); // oh my god this is nasty.
					ref->flags |= MF_NOTHINK|MF_NOCLIPTHING;
					ref->renderflags |= RF_DONTDRAW;
					ref->momx = ref->momy = ref->momz = 0;
				}

				mobj->flags2 |= MF2_BEYONDTHEGRAVE;
			}
			return; // don't make bubble!
		}
		else if (mobj->flags2 & MF2_BEYONDTHEGRAVE)
		{
			mobj_t *ref = mobj;

			// start/show all your babies
			while ((ref = ref->hnext))
			{
				if (ref->eflags & 1)
					ref->flags &= ~MF_NOTHINK;
				if (ref->eflags & 2)
					ref->flags &= ~MF_NOCLIPTHING;
				if (ref->eflags & 4)
					ref->renderflags &= ~RF_DONTDRAW;
				ref->eflags = 0; // le sign
			}

			mobj->flags2 &= ~MF2_BEYONDTHEGRAVE;
		}
	}

	// Okay, time to MOVE
	P_MaceRotate(mobj, mobj->movedir, oldmovedir);
}

static void P_FlameJetSceneryThink(mobj_t *mobj)
{
	mobj_t *flame;
	fixed_t strength;

	if (!(mobj->flags2 & MF2_FIRING))
		return;

	if ((leveltime & 3) != 0)
		return;

	// Wave the flames back and forth. Reactiontime determines which direction it's going.
	if (mobj->fuse <= -16)
		mobj->reactiontime = 1;
	else if (mobj->fuse >= 16)
		mobj->reactiontime = 0;

	if (mobj->reactiontime)
		mobj->fuse += 2;
	else
		mobj->fuse -= 2;

	flame = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_FLAMEJETFLAME);
	P_SetMobjState(flame, S_FLAMEJETFLAME4);

	P_InitAngle(flame, mobj->angle);

	if (mobj->flags2 & MF2_AMBUSH) // Wave up and down instead of side-to-side
		flame->momz = mobj->fuse << (FRACBITS - 2);
	else
		flame->angle += FixedAngle(mobj->fuse<<FRACBITS);

	strength = 20*FRACUNIT;
	strength -= ((20*FRACUNIT)/16)*mobj->movedir;

	P_InstaThrust(flame, flame->angle, strength);
	S_StartSound(flame, sfx_fire);
}

static void P_VerticalFlameJetSceneryThink(mobj_t *mobj)
{
	mobj_t *flame;
	fixed_t strength;

	if (!(mobj->flags2 & MF2_FIRING))
		return;

	if ((leveltime & 3) != 0)
		return;

	// Wave the flames back and forth. Reactiontime determines which direction it's going.
	if (mobj->fuse <= -16)
		mobj->reactiontime = 1;
	else if (mobj->fuse >= 16)
		mobj->reactiontime = 0;

	if (mobj->reactiontime)
		mobj->fuse++;
	else
		mobj->fuse--;

	flame = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_FLAMEJETFLAME);

	strength = 20*FRACUNIT;
	strength -= ((20*FRACUNIT)/16)*mobj->movedir;

	// If deaf'd, the object spawns on the ceiling.
	if (mobj->flags2 & MF2_AMBUSH)
	{
		mobj->z = mobj->ceilingz - mobj->height;
		flame->momz = -strength;
	}
	else
	{
		flame->momz = strength;
		P_SetMobjState(flame, S_FLAMEJETFLAME7);
	}

	P_InstaThrust(flame, mobj->angle, FixedDiv(mobj->fuse*FRACUNIT, 3*FRACUNIT));
	S_StartSound(flame, sfx_fire);
}

static boolean P_ParticleGenSceneryThink(mobj_t *mobj)
{
	if (!mobj->lastlook)
		return false;

	if (!mobj->threshold)
		return false;

	if (--mobj->fuse <= 0)
	{
		INT32 i = 0;
		mobj_t *spawn;
		fixed_t bottomheight, topheight;
		INT32 type = mobj->threshold, line = mobj->cvmem;

		mobj->fuse = (tic_t)mobj->reactiontime;

		bottomheight = lines[line].frontsector->floorheight;
		topheight = lines[line].frontsector->ceilingheight - mobjinfo[(mobjtype_t)type].height;

		if (mobj->waterbottom != bottomheight || mobj->watertop != topheight)
		{
			if (mobj->movefactor && (topheight > bottomheight))
				mobj->health = (tic_t)(FixedDiv((topheight - bottomheight), abs(mobj->movefactor)) >> FRACBITS);
			else
				mobj->health = 0;

			mobj->z = ((mobj->flags2 & MF2_OBJECTFLIP) ? topheight : bottomheight);
		}

		if (!mobj->health)
			return false;

		for (i = 0; i < mobj->lastlook; i++)
		{
			spawn = P_SpawnMobj(
				mobj->x + FixedMul(FixedMul(mobj->friction, mobj->scale), FINECOSINE(mobj->angle >> ANGLETOFINESHIFT)),
				mobj->y + FixedMul(FixedMul(mobj->friction, mobj->scale), FINESINE(mobj->angle >> ANGLETOFINESHIFT)),
				mobj->z,
				(mobjtype_t)mobj->threshold);
			P_SetScale(spawn, mobj->scale);
			spawn->momz = FixedMul(mobj->movefactor, spawn->scale);
			spawn->destscale = spawn->scale/100;
			spawn->scalespeed = spawn->scale/mobj->health;
			spawn->tics = (tic_t)mobj->health;
			spawn->flags2 |= (mobj->flags2 & MF2_OBJECTFLIP);
			spawn->angle += P_RandomKey(36)*ANG10; // irrelevant for default objects but might make sense for some custom ones

			mobj->angle += mobj->movedir;
		}
	}

	return true;
}

static void P_MobjSceneryThink(mobj_t *mobj)
{
	if (LUA_HookMobj(mobj, MOBJ_HOOK(MobjThinker)))
		return;
	if (P_MobjWasRemoved(mobj))
		return;

	switch (mobj->type)
	{
	case MT_SHADOW:
		if (mobj->tracer)
		{
			P_MoveOrigin(mobj,
					mobj->tracer->x,
					mobj->tracer->y,
					mobj->tracer->z);
		}
		else
		{
			P_RemoveMobj(mobj);
			return;
		}
		break;
	case MT_BOSSJUNK:
		mobj->renderflags ^= RF_DONTDRAW;
		break;
	case MT_MACEPOINT:
	case MT_CHAINMACEPOINT:
	case MT_SPRINGBALLPOINT:
	case MT_CHAINPOINT:
	case MT_FIREBARPOINT:
	case MT_CUSTOMMACEPOINT:
	case MT_HIDDEN_SLING:
		P_MaceSceneryThink(mobj);
		break;
	case MT_SMALLMACE:
	case MT_BIGMACE:
		P_SpawnGhostMobj(mobj)->tics = 8;
		break;
	case MT_HOOP:
		if (mobj->fuse > 1)
			P_MoveHoop(mobj);
		else if (mobj->fuse == 1)
			mobj->movecount = 1;

		if (mobj->movecount)
		{
			mobj->fuse++;

			if (mobj->fuse > 32)
			{
				// Don't kill the hoop center. For the sake of respawning.
				//if (mobj->target)
				//	P_RemoveMobj(mobj->target);

				P_RemoveMobj(mobj);
			}
		}
		else
			mobj->fuse--;
		return;
	case MT_NIGHTSPARKLE:
		if (mobj->tics != -1)
		{
			mobj->tics--;

			// you can cycle through multiple states in a tic
			if (!mobj->tics)
				if (!P_SetMobjState(mobj, mobj->state->nextstate))
					return; // freed itself
		}

		P_UnsetThingPosition(mobj);
		mobj->x += mobj->momx;
		mobj->y += mobj->momy;
		mobj->z += mobj->momz;
		P_SetThingPosition(mobj);
		return;
	case MT_NIGHTSLOOPHELPER:
		if (--mobj->tics <= 0)
			P_RemoveMobj(mobj);

		// Don't touch my fuse!
		return;
	case MT_OVERLAY:
		if (!mobj->target)
		{
			P_RemoveMobj(mobj);
			return;
		}
		else
			P_AddOverlay(mobj);
		break;
	case MT_WATERDROP:
		P_SceneryCheckWater(mobj);
		if ((mobj->z <= mobj->floorz || mobj->z <= mobj->watertop)
			&& mobj->health > 0)
		{
			mobj->health = 0;
			P_SetMobjState(mobj, mobj->info->deathstate);
			S_StartSound(mobj, mobj->info->deathsound + P_RandomKey(mobj->info->mass));
			return;
		}
		break;
	case MT_BUBBLES:
		P_SceneryCheckWater(mobj);
		break;
	case MT_SMALLBUBBLE:
	case MT_MEDIUMBUBBLE:
	case MT_EXTRALARGEBUBBLE:	// start bubble dissipate
		P_SceneryCheckWater(mobj);
		if (P_MobjWasRemoved(mobj)) // bubble was removed by not being in water
			return;
		if (!(mobj->eflags & MFE_UNDERWATER)
			|| (!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z + mobj->height >= mobj->ceilingz)
			|| (mobj->eflags & MFE_VERTICALFLIP && mobj->z <= mobj->floorz)
			|| (P_CheckDeathPitCollide(mobj))
			|| --mobj->fuse <= 0) // Bubbles eventually dissipate if they can't reach the surface.
		{
			// no playing sound: no point; the object is being removed
			P_RemoveMobj(mobj);
			return;
		}
		break;
	case MT_LOCKON:
		if (!mobj->target)
		{
			P_RemoveMobj(mobj);
			return;
		}

		mobj->renderflags &= ~RF_DONTDRAW;

		mobj->x = mobj->target->x;
		mobj->y = mobj->target->y;

		mobj->eflags |= (mobj->target->eflags & MFE_VERTICALFLIP);

		mobj->destscale = mobj->target->destscale;
		P_SetScale(mobj, mobj->target->scale);

		if (!(mobj->eflags & MFE_VERTICALFLIP))
			mobj->z = mobj->target->z + mobj->target->height + FixedMul((16 + abs((signed)(leveltime % TICRATE) - TICRATE/2))*FRACUNIT, mobj->target->scale);
		else
			mobj->z = mobj->target->z - FixedMul((16 + abs((signed)(leveltime % TICRATE) - TICRATE/2))*FRACUNIT, mobj->target->scale) - mobj->height;

		mobj->old_z = mobj->z;

		break;
	case MT_LOCKONINF:
		if (!(mobj->flags2 & MF2_STRONGBOX))
		{
			mobj->threshold = mobj->z;
			mobj->flags2 |= MF2_STRONGBOX;
		}
		if (!(mobj->eflags & MFE_VERTICALFLIP))
			mobj->z = mobj->threshold + FixedMul((16 + abs((signed)(leveltime % TICRATE) - TICRATE/2))*FRACUNIT, mobj->scale);
		else
			mobj->z = mobj->threshold - FixedMul((16 + abs((signed)(leveltime % TICRATE) - TICRATE/2))*FRACUNIT, mobj->scale);

		mobj->old_z = mobj->z;

		break;
	case MT_FLAMEJET:
		P_FlameJetSceneryThink(mobj);
		break;
	case MT_VERTICALFLAMEJET:
		P_VerticalFlameJetSceneryThink(mobj);
		break;
	case MT_FLICKY_01_CENTER:
	case MT_FLICKY_02_CENTER:
	case MT_FLICKY_03_CENTER:
	case MT_FLICKY_04_CENTER:
	case MT_FLICKY_05_CENTER:
	case MT_FLICKY_06_CENTER:
	case MT_FLICKY_07_CENTER:
	case MT_FLICKY_08_CENTER:
	case MT_FLICKY_09_CENTER:
	case MT_FLICKY_10_CENTER:
	case MT_FLICKY_11_CENTER:
	case MT_FLICKY_12_CENTER:
	case MT_FLICKY_13_CENTER:
	case MT_FLICKY_14_CENTER:
	case MT_FLICKY_15_CENTER:
	case MT_FLICKY_16_CENTER:
	case MT_SECRETFLICKY_01_CENTER:
	case MT_SECRETFLICKY_02_CENTER:
		if (mobj->tracer && (mobj->flags & MF_NOCLIPTHING)
			&& (mobj->flags & MF_GRENADEBOUNCE))
			// for now: only do this bounce routine if flicky is in-place. \todo allow in all movements
		{
			if (!(mobj->tracer->flags2 & MF2_OBJECTFLIP) && mobj->tracer->z <= mobj->tracer->floorz)
				mobj->tracer->momz = 7*FRACUNIT;
			else if ((mobj->tracer->flags2 & MF2_OBJECTFLIP) && mobj->tracer->z >= mobj->tracer->ceilingz - mobj->tracer->height)
				mobj->tracer->momz = -7*FRACUNIT;
		}
		break;
	case MT_SEED:
		if (P_MobjFlip(mobj)*mobj->momz < mobj->info->speed)
			mobj->momz = P_MobjFlip(mobj)*mobj->info->speed;
		break;
	case MT_ROCKCRUMBLE1:
	case MT_ROCKCRUMBLE2:
	case MT_ROCKCRUMBLE3:
	case MT_ROCKCRUMBLE4:
	case MT_ROCKCRUMBLE5:
	case MT_ROCKCRUMBLE6:
	case MT_ROCKCRUMBLE7:
	case MT_ROCKCRUMBLE8:
	case MT_ROCKCRUMBLE9:
	case MT_ROCKCRUMBLE10:
	case MT_ROCKCRUMBLE11:
	case MT_ROCKCRUMBLE12:
	case MT_ROCKCRUMBLE13:
	case MT_ROCKCRUMBLE14:
	case MT_ROCKCRUMBLE15:
	case MT_ROCKCRUMBLE16:
	case MT_WOODDEBRIS:
	case MT_BRICKDEBRIS:
	case MT_BROKENROBOT:
		if (mobj->z <= P_FloorzAtPos(mobj->x, mobj->y, mobj->z, mobj->height)
			&& mobj->state != &states[mobj->info->deathstate])
		{
			P_SetMobjState(mobj, mobj->info->deathstate);
			return;
		}
		break;
	case MT_PARTICLEGEN:
		if (!P_ParticleGenSceneryThink(mobj))
			return;
		break;
	case MT_FSGNA:
		if (mobj->movedir)
			mobj->angle += mobj->movedir;
		break;
	case MT_ORBINAUT_SHIELD: // Kart orbit/trail items
	case MT_JAWZ_SHIELD:
	case MT_BANANA_SHIELD:
	case MT_SSMINE_SHIELD:
	case MT_EGGMANITEM_SHIELD:
	case MT_SINK_SHIELD:
		if ((mobj->health > 0
			&& (!mobj->target || !mobj->target->player || mobj->target->health <= 0 || mobj->target->player->spectator))
			|| (mobj->health <= 0 && P_IsObjectOnGround(mobj))
			|| P_CheckDeathPitCollide(mobj)) // When in death state
		{
			P_RemoveMobj(mobj);
			return;
		}
		break;
	case MT_SMOLDERING:
		if (leveltime % 2 == 0)
		{
			fixed_t x = P_RandomRange(-35, 35)*mobj->scale;
			fixed_t y = P_RandomRange(-35, 35)*mobj->scale;
			fixed_t z = P_RandomRange(0, 70)*mobj->scale;
			mobj_t *smoke = P_SpawnMobj(mobj->x + x, mobj->y + y, mobj->z + z, MT_SMOKE);
			P_SetMobjState(smoke, S_OPAQUESMOKE1);
			K_MatchGenericExtraFlags(smoke, mobj);
			smoke->scale = mobj->scale * 2;
			smoke->destscale = mobj->scale * 6;
			smoke->momz = P_RandomRange(4, 9)*FRACUNIT*P_MobjFlip(smoke);
		}
		break;
	case MT_BOOMPARTICLE:
		{
			fixed_t x = P_RandomRange(-16, 16)*mobj->scale;
			fixed_t y = P_RandomRange(-16, 16)*mobj->scale;
			fixed_t z = P_RandomRange(0, 32)*mobj->scale*P_MobjFlip(mobj);
			if (leveltime % 2 == 0)
			{
				mobj_t *smoke = P_SpawnMobj(mobj->x + x, mobj->y + y, mobj->z + z, MT_BOSSEXPLODE);
				K_MatchGenericExtraFlags(smoke, mobj);
				P_SetMobjState(smoke, S_QUICKBOOM1);
				smoke->scale = mobj->scale/2;
				smoke->destscale = mobj->scale;
				smoke->color = mobj->color;
			}
			else
			{
				mobj_t *smoke = P_SpawnMobj(mobj->x + x, mobj->y + y, mobj->z + z, MT_SMOKE);
				P_SetMobjState(smoke, S_OPAQUESMOKE1);
				K_MatchGenericExtraFlags(smoke, mobj);
				smoke->scale = mobj->scale;
				smoke->destscale = mobj->scale*2;
			}
			if (mobj->tics <= TICRATE)
			{
				mobj->destscale = FixedDiv(mobj->scale, 100*FRACUNIT);
			}
		}
		break;
	case MT_BATTLEBUMPER:
		if (mobj->health <= 0)
		{
			mobj->fuse--;

			if (!S_SoundPlaying(mobj, sfx_cdfm71))
			{
				S_StartSound(mobj, sfx_cdfm71);
			}

			if (mobj->fuse <= 0)
			{
				statenum_t curState = (mobj->state - states);

				if (curState == S_BATTLEBUMPER1)
				{
					P_SetMobjState(mobj, S_BATTLEBUMPER_EXCRYSTALA1);

					if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer))
					{
						P_SetMobjState(mobj->tracer, S_BATTLEBUMPER_EXSHELLA1);
					}

					mobj->shadowscale *= 2;
					mobj->fuse = 12;
				}
				else if (curState >= S_BATTLEBUMPER_EXCRYSTALA1 && curState <= S_BATTLEBUMPER_EXCRYSTALA4)
				{
					P_SetMobjState(mobj, S_BATTLEBUMPER_EXCRYSTALB1);

					if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer))
					{
						P_SetMobjState(mobj->tracer, S_BATTLEBUMPER_EXSHELLB1);
					}

					mobj->shadowscale *= 2;
					mobj->fuse = 24;
					break;
				}
				else if (curState >= S_BATTLEBUMPER_EXCRYSTALB1 && curState <= S_BATTLEBUMPER_EXCRYSTALB4)
				{
					P_SetMobjState(mobj, S_BATTLEBUMPER_EXCRYSTALC1);

					if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer))
					{
						P_SetMobjState(mobj->tracer, S_BATTLEBUMPER_EXSHELLC1);
					}

					mobj->shadowscale *= 2;
					mobj->fuse = 32;
					break;
				}
				else
				{
					const INT16 spacing = 64;
					UINT8 i;

					for (i = 0; i < 10; i++)
					{
						mobj_t *debris = P_SpawnMobjFromMobj(
							mobj,
							P_RandomRange(-spacing, spacing) * FRACUNIT,
							P_RandomRange(-spacing, spacing) * FRACUNIT,
							P_RandomRange(-spacing, spacing) * FRACUNIT,
							MT_BATTLEBUMPER_DEBRIS
						);

						P_SetScale(debris, (debris->destscale *= 2));
						debris->color = mobj->color;

						debris->momz = -debris->scale * P_MobjFlip(debris);
					}

					for (i = 0; i < 2; i++)
					{
						mobj_t *blast = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_BATTLEBUMPER_BLAST);

						blast->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy) + ANGLE_45;
						if (i & 1)
						{
							blast->angle += ANGLE_90;
							S_StartSound(blast, sfx_cdfm64);
						}
						P_InitAngle(blast, blast->angle);

						blast->destscale *= 4;
					}

					for (i = 0; i < 10; i++)
					{
						mobj_t *puff = P_SpawnMobjFromMobj(
							mobj,
							P_RandomRange(-spacing, spacing) * FRACUNIT,
							P_RandomRange(-spacing, spacing) * FRACUNIT,
							P_RandomRange(-spacing, spacing) * FRACUNIT,
							MT_DUST
						);

						P_SetScale(puff, (puff->destscale *= 5));
						puff->momz = puff->scale * P_MobjFlip(puff);

						P_Thrust(puff, R_PointToAngle2(mobj->x, mobj->y, puff->x, puff->y), puff->scale);
					}

					P_RemoveMobj(mobj);
					return;
				}
			}

			break;
		}

		if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player
			&& mobj->target->health > 0 && !mobj->target->player->spectator)
		{
			fixed_t rad = 32*mobj->target->scale;
			fixed_t offz;
			angle_t ang, diff;

			if (!((mobj->target->player-players) & 1))
				ang = -FixedAngle(mobj->info->speed);
			else
				ang = FixedAngle(mobj->info->speed);

			if (mobj->target->player->bumpers <= 1)
				diff = 0;
			else
				diff = FixedAngle(360*FRACUNIT/mobj->target->player->bumpers);

			ang = (ang*leveltime) + (diff * (mobj->threshold-1));

			// If the player is on the ceiling, then flip your items as well.
			if (mobj->target->eflags & MFE_VERTICALFLIP)
			{
				mobj->eflags |= MFE_VERTICALFLIP;
				offz = mobj->target->height / 2;
			}
			else
			{
				mobj->eflags &= ~MFE_VERTICALFLIP;
				offz = mobj->target->height / 5;
			}

			mobj->renderflags = (mobj->target->renderflags & RF_DONTDRAW);

			if (mobj->target->eflags & MFE_VERTICALFLIP)
			{
				offz += 4*FRACUNIT;
			}
			else
			{
				offz -= 4*FRACUNIT;
			}

			if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer) && mobj->tracer->player
				&& mobj->tracer->health > 0 && !mobj->tracer->player->spectator) // STOLEN
			{
				mobj->color = mobj->tracer->color;
			}
			else
			{
				mobj->color = mobj->target->color;
			}

			if (mobj->target->player->bumpers < 2)
				P_SetMobjState(mobj, S_BATTLEBUMPER3);
			else if (mobj->target->player->bumpers < 3)
				P_SetMobjState(mobj, S_BATTLEBUMPER2);
			else
				P_SetMobjState(mobj, S_BATTLEBUMPER1);

			// Shrink your items if the player shrunk too.
			P_SetScale(mobj, mobj->target->scale);

			P_UnsetThingPosition(mobj);
			{
				const angle_t fa = ang >> ANGLETOFINESHIFT;
				mobj->x = mobj->target->x + FixedMul(FINECOSINE(fa), rad);
				mobj->y = mobj->target->y + FixedMul(FINESINE(fa), rad);
				mobj->z = mobj->target->z + offz;
				P_SetThingPosition(mobj);
			}

			if (mobj->target->player->bumpers <= mobj->threshold)
			{
				// Do bumper destruction
				P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
				break;
			}
		}
		else
		{
			// Sliently remove
			P_RemoveMobj(mobj);
			return;
		}

		break;

	case MT_BATTLEBUMPER_DEBRIS:
		if (mobj->state == states + S_BATTLEBUMPER_EXDEBRIS2)
		{
			mobj->renderflags ^= RF_DONTDRAW;
		}
		break;

	// see also K_drawKartItem in k_hud.c
	case MT_PLAYERARROW:
		if (mobj->target && mobj->target->health
			&& mobj->target->player && !mobj->target->player->spectator
			&& mobj->target->health && mobj->target->player->playerstate != PST_DEAD
			/*&& players[displayplayers[0]].mo && !players[displayplayers[0]].spectator*/)
		{
			fixed_t scale = 3*mobj->target->scale;
			mobj->color = mobj->target->color;
			K_MatchGenericExtraFlags(mobj, mobj->target);

			if ((gametype == GT_RACE || mobj->target->player->bumpers <= 0)
#if 1 // Set to 0 to test without needing to host
				|| (P_IsDisplayPlayer(mobj->target->player))
#endif
				)
				mobj->renderflags |= RF_DONTDRAW;

			P_UnsetThingPosition(mobj);
			mobj->x = mobj->target->x;
			mobj->y = mobj->target->y;

			P_InitAngle (mobj, R_PointToAngle(mobj->x, mobj->y) + ANGLE_90); // literally only happened because i wanted to ^L^R the SPR_ITEM's

			if (!r_splitscreen && players[displayplayers[0]].mo)
			{
				scale = mobj->target->scale + FixedMul(FixedDiv(abs(P_AproxDistance(players[displayplayers[0]].mo->x-mobj->target->x,
					players[displayplayers[0]].mo->y-mobj->target->y)), RING_DIST), mobj->target->scale);
				if (scale > 16*mobj->target->scale)
					scale = 16*mobj->target->scale;
			}
			mobj->destscale = scale;

			if (!(mobj->target->eflags & MFE_VERTICALFLIP))
			{
				mobj->z = mobj->target->z + mobj->target->height + (16*mobj->target->scale);
				mobj->eflags &= ~MFE_VERTICALFLIP;
			}
			else
			{
				mobj->z = mobj->target->z - mobj->target->height - (16*mobj->target->scale);
				mobj->eflags |= MFE_VERTICALFLIP;
			}
			P_SetThingPosition(mobj);

			if (!mobj->tracer)
			{
				mobj_t *overlay = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_OVERLAY);
				P_SetTarget(&mobj->tracer, overlay);
				P_SetTarget(&mobj->tracer->target, mobj);
				P_SetMobjState(mobj->tracer, S_PLAYERARROW_ITEM);
				P_SetScale(mobj->tracer, (mobj->tracer->destscale = mobj->scale));
			}

			// Do this in an easy way
			if (mobj->target->player->itemroulette)
			{
				mobj->tracer->color = mobj->target->player->skincolor;
				mobj->tracer->colorized = true;
			}
			else
			{
				mobj->tracer->color = SKINCOLOR_NONE;
				mobj->tracer->colorized = false;
			}

			if (!(mobj->renderflags & RF_DONTDRAW))
			{
				const INT32 numberdisplaymin = ((mobj->target->player->itemtype == KITEM_ORBINAUT) ? 5 : 2);

				// Set it to use the correct states for its condition
				if (mobj->target->player->itemroulette)
				{
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);
					mobj->tracer->sprite = SPR_ITEM;
					switch((mobj->target->player->itemroulette % (16*3)) / 3)
					{
						// Each case is handled in threes, to give three frames of in-game time to see the item on the roulette
						case 0: // Sneaker
							mobj->tracer->frame = KITEM_SNEAKER;
							//localcolor = SKINCOLOR_RASPBERRY;
							break;
						case 1: // Banana
							mobj->tracer->frame = KITEM_BANANA;
							//localcolor = SKINCOLOR_YELLOW;
							break;
						case 2: // Orbinaut
							mobj->tracer->frame = KITEM_ORBINAUT;
							//localcolor = SKINCOLOR_STEEL;
							break;
						case 3: // Mine
							mobj->tracer->frame = KITEM_MINE;
							//localcolor = SKINCOLOR_JET;
							break;
						case 4: // Grow
							mobj->tracer->frame = KITEM_GROW;
							//localcolor = SKINCOLOR_TEAL;
							break;
						case 5: // Hyudoro
							mobj->tracer->frame = KITEM_HYUDORO;
							//localcolor = SKINCOLOR_STEEL;
							break;
						case 6: // Rocket Sneaker
							mobj->tracer->frame = KITEM_ROCKETSNEAKER;
							//localcolor = SKINCOLOR_TANGERINE;
							break;
						case 7: // Jawz
							mobj->tracer->frame = KITEM_JAWZ;
							//localcolor = SKINCOLOR_JAWZ;
							break;
						case 8: // Self-Propelled Bomb
							mobj->tracer->frame = KITEM_SPB;
							//localcolor = SKINCOLOR_JET;
							break;
						case 9: // Shrink
							mobj->tracer->frame = KITEM_SHRINK;
							//localcolor = SKINCOLOR_ORANGE;
							break;
						case 10: // Invincibility
							mobj->tracer->frame = KITEM_INVINCIBILITY;
							//localcolor = SKINCOLOR_GREY;
							break;
						case 11: // Eggman Monitor
							mobj->tracer->frame = KITEM_EGGMAN;
							//localcolor = SKINCOLOR_ROSE;
							break;
						case 12: // Ballhog
							mobj->tracer->frame = KITEM_BALLHOG;
							//localcolor = SKINCOLOR_LILAC;
							break;
						case 13: // Lightning Shield
							mobj->tracer->frame = KITEM_LIGHTNINGSHIELD;
							//localcolor = SKINCOLOR_CYAN;
							break;
						case 14: // Super Ring
							mobj->tracer->frame = KITEM_SUPERRING;
							//localcolor = SKINCOLOR_GOLD;
							break;
						case 15: // Land Mine
							mobj->tracer->frame = KITEM_LANDMINE;
							//localcolor = SKINCOLOR_JET;
							break;
						case 16: // Drop Target
							mobj->tracer->frame = KITEM_DROPTARGET;
							//localcolor = SKINCOLOR_LIME;
							break;
					}
					mobj->tracer->frame |= FF_FULLBRIGHT;
					mobj->tracer->renderflags &= ~RF_DONTDRAW;
				}
				else if (mobj->target->player->stealingtimer < 0)
				{
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);
					mobj->tracer->sprite = SPR_ITEM;
					mobj->tracer->frame = FF_FULLBRIGHT|KITEM_HYUDORO;
					if (leveltime & 2)
						mobj->tracer->renderflags &= ~RF_DONTDRAW;
					else
						mobj->tracer->renderflags |= RF_DONTDRAW;
				}
				else if ((mobj->target->player->stealingtimer > 0) && (leveltime & 2))
				{
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);
					mobj->tracer->sprite = SPR_ITEM;
					mobj->tracer->frame = FF_FULLBRIGHT|KITEM_HYUDORO;
					mobj->tracer->renderflags &= ~RF_DONTDRAW;
				}
				else if (mobj->target->player->eggmanexplode > 1)
				{
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);
					mobj->tracer->sprite = SPR_ITEM;
					mobj->tracer->frame = FF_FULLBRIGHT|KITEM_EGGMAN;
					if (leveltime & 1)
						mobj->tracer->renderflags &= ~RF_DONTDRAW;
					else
						mobj->tracer->renderflags |= RF_DONTDRAW;
				}
				else if (mobj->target->player->rocketsneakertimer > 1)
				{
					//itembar = mobj->target->player->rocketsneakertimer; -- not today satan
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);
					mobj->tracer->sprite = SPR_ITEM;
					mobj->tracer->frame = FF_FULLBRIGHT|KITEM_ROCKETSNEAKER;
					if (leveltime & 1)
						mobj->tracer->renderflags &= ~RF_DONTDRAW;
					else
						mobj->tracer->renderflags |= RF_DONTDRAW;
				}
				else if (mobj->target->player->growshrinktimer > 0)
				{
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);
					mobj->tracer->sprite = SPR_ITEM;
					mobj->tracer->frame = FF_FULLBRIGHT|KITEM_GROW;

					if (leveltime & 1)
						mobj->tracer->renderflags &= ~RF_DONTDRAW;
					else
						mobj->tracer->renderflags |= RF_DONTDRAW;
				}
				else if (mobj->target->player->itemtype && mobj->target->player->itemamount > 0)
				{
					P_SetMobjState(mobj, S_PLAYERARROW_BOX);

					switch (mobj->target->player->itemtype)
					{
						case KITEM_ORBINAUT:
							mobj->tracer->sprite = SPR_ITMO;
							mobj->tracer->frame = FF_FULLBRIGHT|K_GetOrbinautItemFrame(mobj->target->player->itemamount);
							break;
						case KITEM_INVINCIBILITY:
							mobj->tracer->sprite = SPR_ITMI;
							mobj->tracer->frame = FF_FULLBRIGHT|K_GetInvincibilityItemFrame();
							break;
						case KITEM_SAD:
							mobj->tracer->sprite = SPR_ITEM;
							mobj->tracer->frame = FF_FULLBRIGHT;
							break;
						default:
							mobj->tracer->sprite = SPR_ITEM;
							mobj->tracer->frame = FF_FULLBRIGHT|(mobj->target->player->itemtype);
							break;
					}

					if (mobj->target->player->pflags & PF_ITEMOUT)
					{
						if (leveltime & 1)
							mobj->tracer->renderflags &= ~RF_DONTDRAW;
						else
							mobj->tracer->renderflags |= RF_DONTDRAW;
					}
					else
						mobj->tracer->renderflags &= ~RF_DONTDRAW;
				}
				else
				{
					P_SetMobjState(mobj, S_PLAYERARROW);
					P_SetMobjState(mobj->tracer, S_PLAYERARROW_ITEM);
				}

				mobj->tracer->destscale = scale;

				if (mobj->target->player->itemamount >= numberdisplaymin
					&& mobj->target->player->itemamount <= 10) // Meh, too difficult to support greater than this; convert this to a decent HUD object and then maybe :V
				{
					mobj_t *number = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_OVERLAY);
					mobj_t *numx = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_OVERLAY);

					P_SetTarget(&number->target, mobj);
					P_SetMobjState(number, S_PLAYERARROW_NUMBER);
					P_SetScale(number, mobj->scale);
					number->destscale = scale;
					number->frame = FF_FULLBRIGHT|(mobj->target->player->itemamount);

					P_SetTarget(&numx->target, mobj);
					P_SetMobjState(numx, S_PLAYERARROW_X);
					P_SetScale(numx, mobj->scale);
					numx->destscale = scale;
				}

				if (K_IsPlayerWanted(mobj->target->player) && mobj->movecount != 1)
				{
					mobj_t *wanted = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_PLAYERWANTED);
					P_SetTarget(&wanted->target, mobj->target);
					P_SetTarget(&wanted->tracer, mobj);
					P_SetScale(wanted, mobj->scale);
					wanted->destscale = scale;
					mobj->movecount = 1;
				}
				else if (!K_IsPlayerWanted(mobj->target->player))
					mobj->movecount = 0;
			}
			else
				mobj->tracer->renderflags |= RF_DONTDRAW;
		}
		else if (mobj->health > 0)
		{
			P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
			return;
		}
		break;
	case MT_PLAYERWANTED:
		if (mobj->target && mobj->target->health && mobj->tracer
			&& mobj->target->player && !mobj->target->player->spectator
			&& mobj->target->health && mobj->target->player->playerstate != PST_DEAD
			&& players[g_localplayers[0]].mo && !players[g_localplayers[0]].spectator)
		{
			fixed_t scale = 3*mobj->target->scale;

			if (!K_IsPlayerWanted(mobj->target->player))
			{
				mobj->tracer->movecount = 0;
				P_RemoveMobj(mobj);
				return;
			}

			if (mobj->tracer->renderflags & RF_DONTDRAW)
				mobj->renderflags |= RF_DONTDRAW;
			else
				mobj->renderflags &= ~RF_DONTDRAW;

			P_UnsetThingPosition(mobj);
			mobj->x = mobj->target->x;
			mobj->y = mobj->target->y;

			if (!r_splitscreen && players[displayplayers[0]].mo)
			{
				scale = mobj->target->scale + FixedMul(FixedDiv(abs(P_AproxDistance(players[displayplayers[0]].mo->x-mobj->target->x,
					players[displayplayers[0]].mo->y-mobj->target->y)), RING_DIST), mobj->target->scale);
				if (scale > 16*mobj->target->scale)
					scale = 16*mobj->target->scale;
			}
			mobj->destscale = scale;

			if (!(mobj->target->eflags & MFE_VERTICALFLIP))
			{
				mobj->z = mobj->target->z + mobj->target->height + (16*mobj->target->scale) + (64*scale);
				mobj->eflags &= ~MFE_VERTICALFLIP;
			}
			else
			{
				mobj->z = mobj->target->z - mobj->target->height - (16*mobj->target->scale) - (64*scale);
				mobj->eflags |= MFE_VERTICALFLIP;
			}
			P_SetThingPosition(mobj);
		}
		else if (mobj->health > 0)
		{
			P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
			return;
		}
		break;
	case MT_PETSMOKER:
		if (!(leveltime % 10))
		{
			mobj_t *smok = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_PETSMOKE);
			if (mobj->spawnpoint && mobj->spawnpoint->args[0])
				P_SetMobjStateNF(smok, smok->info->painstate); // same function, diff sprite
		}
		break;
	case MT_ITEMCAPSULE_PART:
		P_ItemCapsulePartThinker(mobj);
		break;
	case MT_BATTLECAPSULE_PIECE:
		if (mobj->extravalue2)
			mobj->frame |= FF_VERTICALFLIP;
		else
			mobj->frame &= ~FF_VERTICALFLIP;

		if (mobj->flags2 & MF2_OBJECTFLIP)
			mobj->eflags |= MFE_VERTICALFLIP;

		if (mobj->tics > 0)
			mobj->renderflags ^= RF_DONTDRAW;
		break;
	case MT_VWREF:
	case MT_VWREB:
	{
		INT32 strength;
		++mobj->movedir;
		mobj->frame &= ~FF_TRANSMASK;
		strength = min(mobj->fuse, (INT32)mobj->movedir)*3;
		if (strength < 10)
			mobj->frame |= ((10 - strength) << (FF_TRANSSHIFT));
	}
	/* FALLTHRU */
	default:
		if (mobj->fuse)
		{ // Scenery object fuse! Very basic!
			mobj->fuse--;
			if (!mobj->fuse)
			{
				if (!LUA_HookMobj(mobj, MOBJ_HOOK(MobjFuse)))
					P_RemoveMobj(mobj);
				return;
			}
		}
		break;
	}

	P_SceneryThinker(mobj);
}

static boolean P_MobjPushableThink(mobj_t *mobj)
{
	P_MobjCheckWater(mobj);
	P_PushableThinker(mobj);
	return true;
}

static boolean P_MobjBossThink(mobj_t *mobj)
{
	if (LUA_HookMobj(mobj, MOBJ_HOOK(BossThinker)))
	{
		if (P_MobjWasRemoved(mobj))
			return false;
	}
	else if (P_MobjWasRemoved(mobj))
		return false;
	else
		switch (mobj->type)
		{
		// No SRB2Kart bosses... yet :)
		default: // Generic SOC-made boss
			if (mobj->flags2 & MF2_SKULLFLY)
				P_SpawnGhostMobj(mobj);
			P_GenericBossThinker(mobj);
			break;
		}
	if (mobj->flags2 & MF2_BOSSFLEE)
	{
		if (mobj->extravalue1)
		{
			if (!(--mobj->extravalue1))
			{
				if (mobj->target)
				{
					mobj->momz = FixedMul(FixedDiv(mobj->target->z - mobj->z, P_AproxDistance(mobj->x - mobj->target->x, mobj->y - mobj->target->y)), mobj->scale << 1);
					mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->target->x, mobj->target->y);
				}
				else
					mobj->momz = 8*mobj->scale;
			}
			else
				mobj->angle += mobj->movedir;
		}
		else if (mobj->target)
			P_InstaThrust(mobj, mobj->angle, FixedMul(12*FRACUNIT, mobj->scale));
	}
	return true;
}

static boolean P_MobjDeadThink(mobj_t *mobj)
{
	switch (mobj->type)
	{
	case MT_PLAYER:
		/// \todo Have the player's dead body completely finish its animation even if they've already respawned.
		if (!mobj->fuse)
		{ // Go away.
		  /// \todo Actually go ahead and remove mobj completely, and fix any bugs and crashes doing this creates. Chasecam should stop moving, and F12 should never return to it.
			mobj->momz = 0;

			if (mobj->player)
			{
				mobj->renderflags |= RF_DONTDRAW;
			}
			else // safe to remove, nobody's going to complain!
			{
				P_RemoveMobj(mobj);
				return false;
			}
		}
		else // Apply gravity to fall downwards.
		{
			P_SetObjectMomZ(mobj, -2*FRACUNIT/3, true);
		}
		break;
	case MT_METALSONIC_RACE:
	{
		if (!(mobj->fuse % 8))
		{
			fixed_t r = mobj->radius >> FRACBITS;
			mobj_t *explosion = P_SpawnMobj(
				mobj->x + (P_RandomRange(r, -r) << FRACBITS),
				mobj->y + (P_RandomRange(r, -r) << FRACBITS),
				mobj->z + (P_RandomKey(mobj->height >> FRACBITS) << FRACBITS),
				MT_SONIC3KBOSSEXPLODE);
			S_StartSound(explosion, sfx_s3kb4);
		}
		P_SetObjectMomZ(mobj, -2*FRACUNIT/3, true);
	}
	break;
	case MT_ORBINAUT:
	case MT_BANANA:
	case MT_EGGMANITEM:
	case MT_LANDMINE:
	//case MT_DROPTARGET:
	case MT_SPB:
		if (P_IsObjectOnGround(mobj))
		{
			P_RemoveMobj(mobj);
			return false;
		}
		/* FALLTHRU */
	case MT_ORBINAUT_SHIELD:
	case MT_BANANA_SHIELD:
	case MT_EGGMANITEM_SHIELD:
		mobj->renderflags ^= RF_DONTDRAW;
		break;
	case MT_JAWZ:
	case MT_JAWZ_DUD:
		if (P_IsObjectOnGround(mobj))
			P_SetMobjState(mobj, mobj->info->xdeathstate);
		/* FALLTHRU */
	case MT_JAWZ_SHIELD:
		mobj->renderflags ^= RF_DONTDRAW;
		break;
	case MT_SSMINE:
	case MT_SPBEXPLOSION:
		if (mobj->extravalue2 != -100)
		{
			P_SetMobjState(mobj, mobj->info->deathstate);
			mobj->extravalue2 = -100;
		}
		else
		{
			P_RemoveMobj(mobj);
			return false;
		}
		break;
	case MT_CDUFO:
		if (mobj->fuse > TICRATE)
			mobj->renderflags ^= RF_DONTDRAW; // only by good fortune does this end with it having RF_DONTDRAW... don't touch!
		break;
	case MT_SMK_PIPE:
		if (mobj->flags2 & MF2_AMBUSH)
			P_SetMobjStateNF(mobj, mobj->info->seestate);
		else
			P_SetMobjStateNF(mobj, mobj->info->spawnstate);
		/* FALLTHRU */
	case MT_SMK_MOLE:
		mobj->renderflags ^= RF_DONTDRAW;
		if (P_IsObjectOnGround(mobj))
		{
			P_RemoveMobj(mobj);
			return false;
		}
		break;
	case MT_SMK_THWOMP:
		if (mobj->flags2 & MF2_AMBUSH)
		{
			mobj->colorized = true;
			mobj->color = K_RainbowColor(leveltime);
			mobj->frame |= FF_FULLBRIGHT;
		}
		else
		{
			mobj->colorized = false;
			mobj->color = SKINCOLOR_NONE;
			mobj->frame &= (~FF_FULLBRIGHT);
		}

		mobj->renderflags ^= RF_DONTDRAW;
		if (P_IsObjectOnGround(mobj))
		{
			P_RemoveMobj(mobj);
			return false;
		}
		break;
	case MT_BATTLECAPSULE:
		if (!(mobj->fuse & 1))
		{
			const SINT8 amt = 96;
			mobj_t *dust;
			UINT8 i;

			for (i = 0; i < 2; i++)
			{
				fixed_t xoffset = P_RandomRange(-amt, amt) * mobj->scale;
				fixed_t yoffset = P_RandomRange(-amt, amt) * mobj->scale;
				fixed_t zoffset = P_RandomRange(-(amt >> 1), (amt >> 1)) * mobj->scale;

				dust = P_SpawnMobj(mobj->x + xoffset, mobj->y + yoffset,
					mobj->z + (mobj->height >> 1) + zoffset, MT_EXPLODE);
			}

			if (dust && !P_MobjWasRemoved(dust)) // Only do for 1 explosion
				S_StartSound(dust, sfx_s3k3d);
		}
		break;
	default:
		break;
	}
	return true;
}

// Angle-to-tracer to trigger a linedef exec
// See Linedef Exec 457 (Track mobj angle to point)
static void P_TracerAngleThink(mobj_t *mobj)
{
	angle_t looking;
	angle_t ang;

	if (!mobj->tracer)
		return;

	if (!mobj->extravalue2)
		return;

	// mobj->lastlook - Don't disable behavior after first failure
	// mobj->extravalue1 - Angle tolerance
	// mobj->extravalue2 - Exec tag upon failure
	// mobj->cvval - Allowable failure delay
	// mobj->cvmem - Failure timer

	if (mobj->player)
		looking = mobj->player->angleturn;
	else
		looking = mobj->angle;

	ang = looking - R_PointToAngle2(mobj->x, mobj->y, mobj->tracer->x, mobj->tracer->y);

	// \todo account for distance between mobj and tracer
	// Because closer mobjs can be facing beyond the angle tolerance
	// yet tracer is still in the camera view

	// failure state: mobj is not facing tracer
	// Reasaonable defaults: ANGLE_67h, ANGLE_292h
	if (ang >= (angle_t)mobj->extravalue1 && ang <= ANGLE_MAX - (angle_t)mobj->extravalue1)
	{
		if (mobj->cvmem)
			mobj->cvmem--;
		else
		{
			INT32 exectag = mobj->extravalue2; // remember this before we erase the values

			if (mobj->lastlook)
				mobj->cvmem = mobj->cusval; // reset timer for next failure
			else
			{
				// disable after first failure
				mobj->eflags &= ~MFE_TRACERANGLE;
				mobj->lastlook = mobj->extravalue1 = mobj->extravalue2 = mobj->cvmem = mobj->cusval = 0;
			}

			P_LinedefExecute(exectag, mobj, NULL);
		}
	}
	else
		mobj->cvmem = mobj->cusval; // reset failure timer
}

static boolean P_MobjRegularThink(mobj_t *mobj)
{
	if ((mobj->flags & MF_ENEMY) && (mobj->state->nextstate == mobj->info->spawnstate && mobj->tics == 1))
		mobj->flags2 &= ~MF2_FRET;

	if (mobj->eflags & MFE_TRACERANGLE)
		P_TracerAngleThink(mobj);

	switch (mobj->type)
	{
	case MT_WALLSPIKEBASE:
		if (!mobj->target) {
			P_RemoveMobj(mobj);
			return false;
		}
		mobj->frame = (mobj->frame & ~FF_FRAMEMASK)|(mobj->target->frame & FF_FRAMEMASK);
#if 0
		if (mobj->angle != mobj->target->angle + ANGLE_90) // reposition if not the correct angle
		{
			mobj_t* target = mobj->target; // shortcut
			const fixed_t baseradius = target->radius - (target->scale/2); //FixedMul(FRACUNIT/2, target->scale);
			P_UnsetThingPosition(mobj);
			mobj->x = target->x - P_ReturnThrustX(target, target->angle, baseradius);
			mobj->y = target->y - P_ReturnThrustY(target, target->angle, baseradius);
			P_SetThingPosition(mobj);
			mobj->angle = target->angle + ANGLE_90;
		}
#endif
		break;
	case MT_FALLINGROCK:
		// Despawn rocks here in case zmovement code can't do so (blame slopes)
		if (!mobj->momx && !mobj->momy && !mobj->momz
			&& ((mobj->eflags & MFE_VERTICALFLIP) ?
				mobj->z + mobj->height >= mobj->ceilingz
				: mobj->z <= mobj->floorz))
		{
			mobj->fuse = TICRATE;
		}
		P_MobjCheckWater(mobj);
		break;
	case MT_FLAME:
		if (mobj->flags2 & MF2_BOSSNOTRAP)
		{
			if (!mobj->target || P_MobjWasRemoved(mobj->target))
			{
				if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer))
					P_RemoveMobj(mobj->tracer);
				P_RemoveMobj(mobj);
				return false;
			}
			mobj->z = mobj->target->z + mobj->target->momz;
			if (!(mobj->eflags & MFE_VERTICALFLIP))
				mobj->z += mobj->target->height;
		}
		if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer))
		{
			mobj->tracer->z = mobj->z + P_MobjFlip(mobj)*20*mobj->scale;
			if (mobj->eflags & MFE_VERTICALFLIP)
				mobj->tracer->z += mobj->height;
		}
		break;
	case MT_WAVINGFLAG1:
	case MT_WAVINGFLAG2:
	{
		fixed_t base = (leveltime << (FRACBITS + 1));
		mobj_t *seg = mobj->tracer, *prev = mobj;
		mobj->movedir = mobj->angle
			+ ((((FINESINE((FixedAngle(base << 1) >> ANGLETOFINESHIFT) & FINEMASK)
				+ FINESINE((FixedAngle(base << 4) >> ANGLETOFINESHIFT) & FINEMASK)) >> 1)
				+ FINESINE((FixedAngle(base*9) >> ANGLETOFINESHIFT) & FINEMASK)
				+ FINECOSINE(((FixedAngle(base*9)) >> ANGLETOFINESHIFT) & FINEMASK)) << 12); //*2^12
		while (seg)
		{
			seg->movedir = seg->angle;
			seg->angle = prev->movedir;
			P_UnsetThingPosition(seg);
			seg->x = prev->x + P_ReturnThrustX(prev, prev->angle, prev->radius);
			seg->y = prev->y + P_ReturnThrustY(prev, prev->angle, prev->radius);
			seg->z = prev->z + prev->height - (seg->scale >> 1);
			P_SetThingPosition(seg);
			prev = seg;
			seg = seg->tracer;
		}
	}
	break;
	case MT_SMASHINGSPIKEBALL:
		mobj->momx = mobj->momy = 0;
		if (mobj->state - states == S_SMASHSPIKE_FALL && P_IsObjectOnGround(mobj))
		{
			P_SetMobjState(mobj, S_SMASHSPIKE_STOMP1);
			S_StartSound(mobj, sfx_spsmsh);
		}
		else if (mobj->state - states == S_SMASHSPIKE_RISE2 && P_MobjFlip(mobj)*(mobj->z - mobj->movecount) >= 0)
		{
			mobj->momz = 0;
			P_SetMobjState(mobj, S_SMASHSPIKE_FLOAT);
		}
		break;
	case MT_PLAYER:
		if (mobj->player)
			P_PlayerMobjThinker(mobj);
		return false;
	case MT_RING:
		if (P_MobjWasRemoved(mobj))
			return false;

		// No need to check water. Who cares?
		P_RingThinker(mobj);

		if (mobj->flags2 & MF2_NIGHTSPULL)
			P_NightsItemChase(mobj);
		else
			A_AttractChase(mobj);
		return false;
		// Flung items
	case MT_FLINGRING:
		if (P_MobjWasRemoved(mobj))
			return false;

		if (mobj->flags2 & MF2_NIGHTSPULL)
			P_NightsItemChase(mobj);
		else
			A_AttractChase(mobj);
		break;
	case MT_DEBTSPIKE:
		if (mobj->fuse == 0 && P_GetMobjFeet(mobj) == P_GetMobjGround(mobj))
		{
			mobj->fuse = 90;
		}
		break;
	case MT_EMBLEM:
		if (mobj->flags2 & MF2_NIGHTSPULL)
			P_NightsItemChase(mobj);
		break;
	case MT_FLOATINGITEM:
	{
		if (mobj->flags & MF_NOCLIPTHING)
		{
			if (P_CheckDeathPitCollide(mobj))
			{
				P_RemoveMobj(mobj);
				return false;
			}
			else if (P_IsObjectOnGround(mobj))
			{
				mobj->momx = 1;
				mobj->momy = 0;
				mobj->flags &= ~MF_NOCLIPTHING;
				mobj->flags |= MF_NOGRAVITY;
			}
		}
		else
		{
			mobj->angle += 2*ANG2;
			if (mobj->flags2 & MF2_NIGHTSPULL)
			{
				if (!mobj->tracer || !mobj->tracer->health
				|| mobj->scale <= mapobjectscale>>4)
				{
					P_RemoveMobj(mobj);
					return false;
				}
				P_Attract(mobj, mobj->tracer, true);
			}
			else
			{
				fixed_t adj = FixedMul(FRACUNIT - FINECOSINE((mobj->movedir>>ANGLETOFINESHIFT) & FINEMASK), (mapobjectscale<<3));
				mobj->movedir += 2*ANG2;
				if (mobj->eflags & MFE_VERTICALFLIP)
					mobj->z = mobj->ceilingz - mobj->height - adj;
				else
					mobj->z = mobj->floorz + adj;
			}
		}

		switch (mobj->threshold)
		{
			case KITEM_ORBINAUT:
				mobj->sprite = SPR_ITMO;
				mobj->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetOrbinautItemFrame(mobj->movecount);
				break;
			case KITEM_INVINCIBILITY:
				mobj->sprite = SPR_ITMI;
				mobj->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetInvincibilityItemFrame();
				break;
			case KITEM_SAD:
				mobj->sprite = SPR_ITEM;
				mobj->frame = FF_FULLBRIGHT|FF_PAPERSPRITE;
				break;
			case KITEM_SPB:
			case KITEM_SHRINK:
				indirectitemcooldown = 20*TICRATE;
				/* FALLTHRU */
			default:
				mobj->sprite = SPR_ITEM;
				mobj->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|(mobj->threshold);
				break;
		}
		break;
	}
	case MT_ITEMCAPSULE:
		// scale the capsule
		if (mobj->scale < mobj->extravalue1)
		{
			fixed_t oldHeight = mobj->height;

			if ((mobj->extravalue1 - mobj->scale) < mobj->scalespeed)
				P_SetScale(mobj, mobj->destscale = mobj->extravalue1);
			else
				P_SetScale(mobj, mobj->destscale = mobj->scale + mobj->scalespeed);

			if (mobj->eflags & MFE_VERTICALFLIP)
				mobj->z -= (mobj->height - oldHeight);
		}

		// spawn parts if not done yet
		// (this SHOULD be done when the capsule is spawned, but gravflip isn't set up at that point)
		if (!(mobj->flags2 & MF2_JUSTATTACKED))
		{
			mobj->flags2 |= MF2_JUSTATTACKED;
			P_SpawnItemCapsuleParts(mobj);
		}

		// update & animate capsule
		if (!P_MobjWasRemoved(mobj->tracer))
		{
			mobj_t *part = mobj->tracer;

			if (mobj->threshold != part->threshold
			 || mobj->movecount != part->movecount) // change the capsule properties if the item type or amount is updated
				P_RefreshItemCapsuleParts(mobj);

			// animate invincibility capsules
			if (mobj->threshold == KITEM_INVINCIBILITY)
			{
				mobj->color = K_RainbowColor(leveltime);
				part->frame = FF_FULLBRIGHT|FF_PAPERSPRITE|K_GetInvincibilityItemFrame();
			}
		}
		break;
	case MT_ORBINAUT:
	{
		boolean grounded = P_IsObjectOnGround(mobj);

		if (mobj->flags2 & MF2_AMBUSH)
		{
			if (grounded && (mobj->flags & MF_NOCLIPTHING))
			{
				mobj->momx = 1;
				mobj->momy = 0;
				mobj->frame = 3;
				S_StartSound(mobj, mobj->info->activesound);
				mobj->flags &= ~MF_NOCLIPTHING;
			}
			else if (mobj->movecount)
				mobj->movecount--;
			else if (mobj->frame < 3)
			{
				mobj->movecount = 2;
				mobj->frame++;
			}
		}
		else
		{
			fixed_t finalspeed = mobj->movefactor;
			const fixed_t currentspeed = R_PointToDist2(0, 0, mobj->momx, mobj->momy);
			fixed_t thrustamount = 0;
			fixed_t frictionsafety = (mobj->friction == 0) ? 1 : mobj->friction;
			mobj_t *ghost = P_SpawnGhostMobj(mobj);
			ghost->colorized = true; // already has color!

			if (!grounded)
			{
				// No friction in the air
				frictionsafety = FRACUNIT;
			}

			mobj->angle = K_MomentumAngle(mobj);
			if (mobj->health <= 5)
			{
				INT32 i;
				for (i = 5; i >= mobj->health; i--)
					finalspeed = FixedMul(finalspeed, FRACUNIT-FRACUNIT/4);
			}

			if (currentspeed >= finalspeed)
			{
				// Thrust as if you were at top speed, slow down naturally
				thrustamount = FixedDiv(finalspeed, frictionsafety) - finalspeed;
			}
			else
			{
				const fixed_t beatfriction = FixedDiv(currentspeed, frictionsafety) - currentspeed;
				// Thrust to immediately get to top speed
				thrustamount = beatfriction + FixedDiv(finalspeed - currentspeed, frictionsafety);
			}

			P_Thrust(mobj, mobj->angle, thrustamount);

			if (P_MobjTouchingSectorSpecial(mobj, 3, 1))
				K_DoPogoSpring(mobj, 0, 1);

			if (mobj->threshold > 0)
				mobj->threshold--;

			if (leveltime % 6 == 0)
				S_StartSound(mobj, mobj->info->activesound);
		}
		break;
	}
	case MT_JAWZ:
	{
		mobj_t *ghost = P_SpawnGhostMobj(mobj);

		if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
		{
			ghost->color = mobj->target->player->skincolor;
			ghost->colorized = true;
		}

		if (mobj->threshold > 0)
			mobj->threshold--;
		if (leveltime % TICRATE == 0)
			S_StartSound(mobj, mobj->info->activesound);

		// Movement handling has ALL been moved to A_JawzChase

		K_DriftDustHandling(mobj);

		if (P_MobjTouchingSectorSpecial(mobj, 3, 1))
			K_DoPogoSpring(mobj, 0, 1);

		if (!(gametyperules & GTR_CIRCUIT))
			mobj->friction = max(0, 3 * mobj->friction / 4);

		break;
	}
	case MT_JAWZ_DUD:
	{
		boolean grounded = P_IsObjectOnGround(mobj);

		if (mobj->flags2 & MF2_AMBUSH)
		{
			if (grounded && (mobj->flags & MF_NOCLIPTHING))
			{
				mobj->momx = 1;
				mobj->momy = 0;
				S_StartSound(mobj, mobj->info->deathsound);
				mobj->flags &= ~MF_NOCLIPTHING;
			}
		}
		else
		{
			mobj_t *ghost = P_SpawnGhostMobj(mobj);
			const fixed_t currentspeed = R_PointToDist2(0, 0, mobj->momx, mobj->momy);
			fixed_t frictionsafety = (mobj->friction == 0) ? 1 : mobj->friction;
			fixed_t thrustamount = 0;

			if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
			{
				ghost->color = mobj->target->player->skincolor;
				ghost->colorized = true;
			}

			if (!grounded)
			{
				// No friction in the air
				frictionsafety = FRACUNIT;
			}

			if (currentspeed >= mobj->movefactor)
			{
				// Thrust as if you were at top speed, slow down naturally
				thrustamount = FixedDiv(mobj->movefactor, frictionsafety) - mobj->movefactor;
			}
			else
			{
				const fixed_t beatfriction = FixedDiv(currentspeed, frictionsafety) - currentspeed;
				// Thrust to immediately get to top speed
				thrustamount = beatfriction + FixedDiv(mobj->movefactor - currentspeed, frictionsafety);
			}

			mobj->angle = K_MomentumAngle(mobj);
			P_Thrust(mobj, mobj->angle, thrustamount);

			if (P_MobjTouchingSectorSpecial(mobj, 3, 1))
				K_DoPogoSpring(mobj, 0, 1);

			if (mobj->threshold > 0)
				mobj->threshold--;

			if (leveltime % TICRATE == 0)
				S_StartSound(mobj, mobj->info->activesound);
		}

		break;
	}
	case MT_EGGMANITEM:
		{
			player_t *player = K_GetItemBoxPlayer(mobj);
			UINT8 color = SKINCOLOR_BLACK;

			if (player != NULL)
			{
				color = player->skincolor;
			}

			mobj->color = color;
			mobj->colorized = false;
		}
		/* FALLTHRU */
	case MT_BANANA:
		mobj->friction = ORIG_FRICTION/4;

		if (mobj->momx || mobj->momy)
		{
			mobj_t *ghost = P_SpawnGhostMobj(mobj);

			if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
			{
				ghost->color = mobj->target->player->skincolor;
				ghost->colorized = true;
			}
		}

		if (P_IsObjectOnGround(mobj) && mobj->health > 1)
		{
			S_StartSound(mobj, mobj->info->activesound);
			mobj->momx = mobj->momy = 0;
			mobj->health = 1;
		}

		if (mobj->threshold > 0)
			mobj->threshold--;
		break;
	case MT_SPB:
		indirectitemcooldown = 20*TICRATE;
		/* FALLTHRU */
	case MT_BALLHOG:
		{
			mobj_t *ghost = P_SpawnGhostMobj(mobj);
			ghost->fuse = 3;

			if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
			{
				ghost->color = mobj->target->player->skincolor;
				ghost->colorized = true;
			}

			if (mobj->threshold > 0)
				mobj->threshold--;
		}
		break;
	case MT_SINK:
		if (mobj->momx || mobj->momy)
		{
			mobj_t *ghost = P_SpawnGhostMobj(mobj);

			if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
			{
				ghost->color = mobj->target->player->skincolor;
				ghost->colorized = true;
			}
		}

		if (P_IsObjectOnGround(mobj))
		{
			S_StartSound(mobj, mobj->info->deathsound);
			P_SetMobjState(mobj, S_NULL);
		}

		if (mobj->threshold > 0)
			mobj->threshold--;
		break;
	case MT_SSMINE:
		if (mobj->target && mobj->target->player)
			mobj->color = mobj->target->player->skincolor;
		else
			mobj->color = SKINCOLOR_KETCHUP;

		if (mobj->momx || mobj->momy)
		{
			mobj_t *ghost = P_SpawnGhostMobj(mobj);
			ghost->colorized = true; // already has color!
		}

		if (P_IsObjectOnGround(mobj) && (mobj->momz * P_MobjFlip(mobj)) <= 0
		&& (mobj->state == &states[S_SSMINE_AIR1] || mobj->state == &states[S_SSMINE_AIR2]))
		{
			if (mobj->extravalue1 > 0)
				mobj->extravalue1--;
			else
			{
				mobj->momx = mobj->momy = 0;
				S_StartSound(mobj, mobj->info->activesound);
				P_SetMobjState(mobj, S_SSMINE_DEPLOY1);
			}
		}

		if ((mobj->state >= &states[S_SSMINE1] && mobj->state <= &states[S_SSMINE4])
			|| (mobj->state >= &states[S_SSMINE_DEPLOY8] && mobj->state <= &states[S_SSMINE_DEPLOY13]))
			A_SSMineSearch(mobj);

		if (mobj->threshold > 0)
			mobj->threshold--;
		break;
	case MT_LANDMINE:
		mobj->friction = ORIG_FRICTION/4;

		if (mobj->momx || mobj->momy || mobj->momz)
		{
			mobj_t *ghost = P_SpawnGhostMobj(mobj);
			ghost->colorized = true; // already has color!
		}

		if (P_IsObjectOnGround(mobj) && mobj->health > 1)
		{
			S_StartSound(mobj, mobj->info->activesound);
			mobj->momx = mobj->momy = 0;
			mobj->health = 1;
		}

		if (mobj->threshold > 0)
			mobj->threshold--;
		break;
	case MT_DROPTARGET:
		if (mobj->reactiontime > 0)
		{
			// Slippery tipping mode.
			INT32 slippytip = 1 + (mobj->reactiontime/2);
			if (slippytip > 64)
				slippytip = 64;
			else if (slippytip < 8)
				slippytip = 8;
			if (!(mobj->health & 1) == !(mobj->flags2 & MF2_AMBUSH))
			{
				slippytip = -slippytip;
			}
			mobj->angle += slippytip*ANG2;

			mobj->friction = ((2*ORIG_FRICTION)+FRACUNIT)/3; // too low still?

			/*if (mobj->momx || mobj->momy || mobj->momz)
			{
				mobj_t *ghost = P_SpawnGhostMobj(mobj);
				ghost->colorized = true; // already has color!
			}*/

			if (!--mobj->reactiontime)
			{
				P_SetMobjState(mobj, mobj->info->spawnstate);
			}
		}
		else
		{
			// Time to stop, ramp up the friction...
			mobj->friction = ORIG_FRICTION/4; // too high still?
		}

		mobj->renderflags = (mobj->renderflags|RF_FULLBRIGHT) ^ RF_FULLDARK; // the difference between semi and fullbright

		if (mobj->threshold > 0)
			mobj->threshold--;
		break;
	case MT_SPBEXPLOSION:
		mobj->health--;
		break;
	case MT_EMERALD:
		{
			if (battleovertime.enabled >= 10*TICRATE)
			{
				fixed_t distance = R_PointToDist2(mobj->x, mobj->y, battleovertime.x, battleovertime.y);

				if (distance > battleovertime.radius)
				{
					// Delete emeralds to let them reappear
					P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
				}
			}

			if (leveltime % 3 == 0)
			{
				mobj_t *sparkle = P_SpawnMobjFromMobj(
					mobj,
					P_RandomRange(-48, 48) * FRACUNIT,
					P_RandomRange(-48, 48) * FRACUNIT,
					P_RandomRange(0, 64) * FRACUNIT,
					MT_EMERALDSPARK
				);

				sparkle->color = mobj->color;
				sparkle->momz += 8 * mobj->scale * P_MobjFlip(mobj);
			}

			if (mobj->threshold > 0)
				mobj->threshold--;
		}
		break;
	case MT_BOOSTFLAME:
		if (!mobj->target || !mobj->target->health)
		{
			P_RemoveMobj(mobj);
			return false;
		}

		mobj->angle = mobj->target->angle;
		P_MoveOrigin(mobj, mobj->target->x + P_ReturnThrustX(mobj, mobj->angle+ANGLE_180, mobj->target->radius),
			mobj->target->y + P_ReturnThrustY(mobj, mobj->angle+ANGLE_180, mobj->target->radius), mobj->target->z);
		P_SetScale(mobj, mobj->target->scale);

		mobj->roll = mobj->target->roll;
		mobj->pitch = mobj->target->pitch;

		{
			player_t *p = NULL;
			if (mobj->target->target && mobj->target->target->player)
				p = mobj->target->target->player;
			else if (mobj->target->player)
				p = mobj->target->player;

			if (p)
			{
				if (p->sneakertimer > mobj->movecount)
					P_SetMobjState(mobj, S_BOOSTFLAME);
				mobj->movecount = p->sneakertimer;
			}
		}

		if (mobj->state == &states[S_BOOSTSMOKESPAWNER])
		{
			mobj_t *smoke = P_SpawnMobj(mobj->x, mobj->y, mobj->z+(8<<FRACBITS), MT_BOOSTSMOKE);

			P_SetScale(smoke, mobj->target->scale/2);
			smoke->destscale = 3*mobj->target->scale/2;
			smoke->scalespeed = mobj->target->scale/12;

			smoke->momx = mobj->target->momx/2;
			smoke->momy = mobj->target->momy/2;
			smoke->momz = mobj->target->momz/2;

			P_Thrust(smoke, mobj->angle+FixedAngle(P_RandomRange(135, 225)<<FRACBITS), P_RandomRange(0, 8) * mobj->target->scale);
		}
		break;
	case MT_SPARKLETRAIL:
		if (!mobj->target)
		{
			P_RemoveMobj(mobj);
			return false;
		}
		mobj->color = mobj->target->color;
		mobj->colorized = mobj->target->colorized;
		break;
	case MT_INVULNFLASH:
		if (!mobj->target || !mobj->target->health || (mobj->target->player && !mobj->target->player->invincibilitytimer))
		{
			P_RemoveMobj(mobj);
			return false;
		}
		P_MoveOrigin(mobj, mobj->target->x, mobj->target->y, mobj->target->z);
		break;
	case MT_BRAKEDRIFT:
		if ((!mobj->target || !mobj->target->health || !mobj->target->player || !P_IsObjectOnGround(mobj->target))
			|| !mobj->target->player->drift || !(mobj->target->player->pflags & PF_BRAKEDRIFT)
			|| !((mobj->target->player->cmd.buttons & BT_BRAKE)
			|| (K_GetKartButtons(mobj->target->player) & BT_ACCELERATE))) // Letting go of accel functions about the same as brake-drifting
		{
			P_RemoveMobj(mobj);
			return false;
		}
		else
		{
			UINT8 driftcolor = K_DriftSparkColor(mobj->target->player, mobj->target->player->driftcharge);
			fixed_t newx, newy;
			angle_t travelangle;

			travelangle = mobj->target->angle - ((ANGLE_45/5)*mobj->target->player->drift);

			newx = mobj->target->x + P_ReturnThrustX(mobj->target, travelangle+ANGLE_180, 24*mobj->target->scale);
			newy = mobj->target->y + P_ReturnThrustY(mobj->target, travelangle+ANGLE_180, 24*mobj->target->scale);
			P_MoveOrigin(mobj, newx, newy, mobj->target->z);

			mobj->angle = travelangle - ((ANGLE_90/5)*mobj->target->player->drift);
			P_SetScale(mobj, (mobj->destscale = mobj->target->scale));

			if (driftcolor != SKINCOLOR_NONE)
				mobj->color = driftcolor;
			else
				mobj->color = SKINCOLOR_SILVER;

			if (!S_SoundPlaying(mobj, sfx_cdfm17))
				S_StartSound(mobj, sfx_cdfm17);

			K_MatchGenericExtraFlags(mobj, mobj->target);
			if (leveltime & 1)
				mobj->renderflags |= RF_DONTDRAW;
		}
		break;
	case MT_PLAYERRETICULE:
		if (!mobj->target || !mobj->target->health)
		{
			P_RemoveMobj(mobj);
			return false;
		}
		P_MoveOrigin(mobj, mobj->target->x, mobj->target->y, mobj->target->z);
		mobj->old_x = mobj->target->old_x;
		mobj->old_y = mobj->target->old_y;
		mobj->old_z = mobj->target->old_z;
		break;
	case MT_INSTASHIELDB:
		mobj->renderflags ^= RF_DONTDRAW;
		/* FALLTHRU */
	case MT_INSTASHIELDA:
		if (!mobj->target || P_MobjWasRemoved(mobj->target) || !mobj->target->health || (mobj->target->player && !mobj->target->player->instashield))
		{
			P_RemoveMobj(mobj);
			return false;
		}
		P_MoveOrigin(mobj, mobj->target->x, mobj->target->y, mobj->target->z);
		mobj->old_x = mobj->target->old_x;
		mobj->old_y = mobj->target->old_y;
		mobj->old_z = mobj->target->old_z;
		K_MatchGenericExtraFlags(mobj, mobj->target);
		break;
	case MT_BATTLEPOINT:
		if (!mobj->target || P_MobjWasRemoved(mobj->target))
		{
			P_RemoveMobj(mobj);
			return false;
		}

		if (mobj->movefactor < 48*mobj->target->scale)
		{
			mobj->movefactor += (48*mobj->target->scale)/6;
			if (mobj->movefactor > mobj->target->height)
				mobj->movefactor = mobj->target->height;
		}
		else if (mobj->movefactor > 48*mobj->target->scale)
		{
			mobj->movefactor -= (48*mobj->target->scale)/6;
			if (mobj->movefactor < mobj->target->height)
				mobj->movefactor = mobj->target->height;
		}
		K_MatchGenericExtraFlags(mobj, mobj->target);
		P_MoveOrigin(mobj, mobj->target->x, mobj->target->y, mobj->target->z + (mobj->target->height/2) + mobj->movefactor);
		break;
	case MT_RINGSPARKS:
		if (!mobj->target || P_MobjWasRemoved(mobj->target))
		{
			P_RemoveMobj(mobj);
			return false;
		}

		mobj->z = mobj->target->z;

		K_MatchGenericExtraFlags(mobj, mobj->target);

		P_MoveOrigin(mobj, mobj->target->x + FINECOSINE(mobj->angle >> ANGLETOFINESHIFT),
				mobj->target->y + FINESINE(mobj->angle >> ANGLETOFINESHIFT),
				mobj->z + (mobj->target->height * P_MobjFlip(mobj)));
		break;
	case MT_GAINAX:
		if (!mobj->target || P_MobjWasRemoved(mobj->target) // sanity
			|| !mobj->target->player // ditto
			|| !mobj->target->player->glanceDir // still glancing?
			|| mobj->target->player->aizdriftturn // only other circumstance where can glance
			|| ((K_GetKartButtons(mobj->target->player) & BT_LOOKBACK) != BT_LOOKBACK)) // it's a lookback indicator...
		{
			P_RemoveMobj(mobj);
			return false;
		}

		mobj->angle = mobj->target->player->drawangle;
		mobj->z = mobj->target->z;

		K_MatchGenericExtraFlags(mobj, mobj->target);
		mobj->renderflags = (mobj->renderflags & ~RF_DONTDRAW)|K_GetPlayerDontDrawFlag(mobj->target->player);

		P_MoveOrigin(mobj, mobj->target->x + FixedMul(34 * mapobjectscale, FINECOSINE((mobj->angle + mobj->movedir) >> ANGLETOFINESHIFT)),
				mobj->target->y + FixedMul(34 * mapobjectscale, FINESINE((mobj->angle + mobj->movedir) >> ANGLETOFINESHIFT)),
				mobj->z + (32 * mapobjectscale * P_MobjFlip(mobj)));

		{
			statenum_t gainaxstate = mobj->state-states;
			if (gainaxstate == S_GAINAX_TINY)
			{
				if (abs(mobj->target->player->glanceDir) > 1)
				{
					if (mobj->target->player->itemamount && mobj->target->player->itemtype)
						gainaxstate = S_GAINAX_HUGE;
					else
						gainaxstate = S_GAINAX_MID1;
					P_SetMobjState(mobj, gainaxstate);
				}
			}
			else if (abs(mobj->target->player->glanceDir) <= 1)
			{
				if (mobj->flags2 & MF2_AMBUSH)
					mobj->flags2 &= ~MF2_AMBUSH;
				else
					P_SetMobjState(mobj, S_GAINAX_TINY);
			}
		}

		break;
	case MT_FLAMESHIELDPAPER:
		if (!mobj->target || P_MobjWasRemoved(mobj->target))
		{
			P_RemoveMobj(mobj);
			return false;
		}

		mobj->z = mobj->target->z;

		K_MatchGenericExtraFlags(mobj, mobj->target);

		{
			INT32 perpendicular = ((mobj->extravalue1 & 1) ? -ANGLE_90 : ANGLE_90);
			fixed_t newx = mobj->target->x + P_ReturnThrustX(NULL, mobj->target->angle + perpendicular, 8*mobj->target->scale);
			fixed_t newy = mobj->target->y + P_ReturnThrustY(NULL, mobj->target->angle + perpendicular, 8*mobj->target->scale);

			P_MoveOrigin(mobj, newx, newy, mobj->target->z);

			if (mobj->extravalue1 & 1)
				mobj->angle = mobj->target->angle - ANGLE_45;
			else
				mobj->angle = mobj->target->angle + ANGLE_45;
		}
		break;
	case MT_LIGHTNINGSHIELD:
	{
		fixed_t destx, desty;
		if (!mobj->target || !mobj->target->health || !mobj->target->player
			|| mobj->target->player->curshield != KSHIELD_LIGHTNING)
		{
			P_RemoveMobj(mobj);
			return false;
		}
		P_SetScale(mobj, (mobj->destscale = (5*mobj->target->scale)>>2));

		if (!r_splitscreen /*&& rendermode != render_soft*/)
		{
			angle_t viewingangle;
			statenum_t curstate = ((mobj->tics == 1) ? (mobj->state->nextstate) : ((statenum_t)(mobj->state-states)));

			if (players[displayplayers[0]].awayviewtics)
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, players[displayplayers[0]].awayviewmobj->x, players[displayplayers[0]].awayviewmobj->y);
			else if (!camera[0].chase && players[displayplayers[0]].mo)
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, players[displayplayers[0]].mo->x, players[displayplayers[0]].mo->y);
			else
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, camera[0].x, camera[0].y);

			if (curstate > S_LIGHTNINGSHIELD15 && curstate <= S_LIGHTNINGSHIELD24)
				viewingangle += ANGLE_180;

			destx = mobj->target->x + P_ReturnThrustX(mobj->target, viewingangle, mobj->scale>>4);
			desty = mobj->target->y + P_ReturnThrustY(mobj->target, viewingangle, mobj->scale>>4);
		}
		else
		{
			destx = mobj->target->x;
			desty = mobj->target->y;
		}

		P_MoveOrigin(mobj, destx, desty, mobj->target->z);
		break;
	}
	case MT_BUBBLESHIELD:
	{
		fixed_t destx, desty;
		fixed_t scale;
		statenum_t curstate;

		if (!mobj->target || !mobj->target->health || !mobj->target->player
			|| mobj->target->player->curshield != KSHIELD_BUBBLE)
		{
			P_RemoveMobj(mobj);
			return false;
		}

		scale = (5*mobj->target->scale)>>2;
		curstate = ((mobj->tics == 1) ? (mobj->state->nextstate) : ((statenum_t)(mobj->state-states)));

		if (mobj->target->player->bubbleblowup)
		{
			INT32 blow = mobj->target->player->bubbleblowup;
			if (blow > bubbletime)
				blow = bubbletime;

			if (curstate != S_BUBBLESHIELDBLOWUP)
				P_SetMobjState(mobj, S_BUBBLESHIELDBLOWUP);

			mobj->angle += ANGLE_22h;
			mobj->renderflags &= ~RF_GHOSTLYMASK;
			scale += (blow * (3*scale)) / bubbletime;

			mobj->frame = (states[S_BUBBLESHIELDBLOWUP].frame + mobj->extravalue1);
			if ((mobj->target->player->bubbleblowup > bubbletime) && (leveltime & 1))
				mobj->frame = (states[S_BUBBLESHIELDBLOWUP].frame + 5);

			if (mobj->extravalue1 < 4 && mobj->extravalue2 < blow && !mobj->cvmem && (leveltime & 1)) // Growing
			{
				mobj->extravalue1++;
				if (mobj->extravalue1 >= 4)
					mobj->cvmem = 1; // shrink back down
			}
			else if ((mobj->extravalue1 > -4 && mobj->extravalue2 > blow)
				|| (mobj->cvmem && mobj->extravalue1 > 0)) // Shrinking
				mobj->extravalue1--;

			if (P_IsObjectOnGround(mobj->target))
			{
				UINT8 i;

				for (i = 0; i < 2; i++)
				{
					angle_t a = mobj->angle + ((i & 1) ? ANGLE_180 : 0);
					fixed_t ws = (mobj->target->scale>>1);
					mobj_t *wave;

					ws += (blow * ws) / bubbletime;

					wave = P_SpawnMobj(
						(mobj->target->x - mobj->target->momx) + P_ReturnThrustX(NULL, a, mobj->radius - (21*ws)),
						(mobj->target->y - mobj->target->momy) + P_ReturnThrustY(NULL, a, mobj->radius - (21*ws)),
						(mobj->target->z - mobj->target->momz), MT_THOK);

					wave->flags &= ~(MF_NOCLIPHEIGHT|MF_NOGRAVITY);
					P_SetScale(wave, (wave->destscale = ws));

					P_SetMobjState(wave, S_BUBBLESHIELDWAVE1);

					wave->momx = mobj->target->momx;
					wave->momy = mobj->target->momy;
					wave->momz = mobj->target->momz;
				}
			}
		}
		else
		{
			mobj->cvmem = 0;
			mobj->angle = mobj->target->angle;

			if (curstate == S_BUBBLESHIELDBLOWUP)
			{
				if (mobj->extravalue1 != 0)
				{
					mobj->frame = (states[S_BUBBLESHIELDBLOWUP].frame + mobj->extravalue1);

					if (mobj->extravalue1 < 0 && (leveltime & 1))
						mobj->extravalue1++;
					else if (mobj->extravalue1 > 0)
						mobj->extravalue1--;
				}
				else
				{
					P_SetMobjState(mobj, S_BUBBLESHIELD1);
					mobj->extravalue1 = 0;
				}
			}
			else
			{
				if (mobj->target->player->bubblecool && ((curstate-S_BUBBLESHIELD1) & 1))
					mobj->renderflags |= RF_GHOSTLY;
				else
					mobj->renderflags &= ~RF_GHOSTLYMASK;
			}
		}

		mobj->extravalue2 = mobj->target->player->bubbleblowup;
		P_SetScale(mobj, (mobj->destscale = scale));

		if (!splitscreen /*&& rendermode != render_soft*/)
		{
			angle_t viewingangle;

			if (players[displayplayers[0]].awayviewtics)
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, players[displayplayers[0]].awayviewmobj->x, players[displayplayers[0]].awayviewmobj->y);
			else if (!camera[0].chase && players[displayplayers[0]].mo)
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, players[displayplayers[0]].mo->x, players[displayplayers[0]].mo->y);
			else
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, camera[0].x, camera[0].y);

			destx = mobj->target->x + P_ReturnThrustX(mobj->target, viewingangle, mobj->scale>>4);
			desty = mobj->target->y + P_ReturnThrustY(mobj->target, viewingangle, mobj->scale>>4);
		}
		else
		{
			destx = mobj->target->x;
			desty = mobj->target->y;
		}

		P_MoveOrigin(mobj, destx, desty, mobj->target->z);
		break;
	}
	case MT_FLAMESHIELD:
	{
		fixed_t destx, desty;
		statenum_t curstate;
		statenum_t underlayst = S_NULL;
		INT32 flamemax = 0;

		if (!mobj->target || !mobj->target->health || !mobj->target->player
			|| mobj->target->player->curshield != KSHIELD_FLAME)
		{
			P_RemoveMobj(mobj);
			return false;
		}

		flamemax = mobj->target->player->flamelength * flameseg;

		P_SetScale(mobj, (mobj->destscale = (5*mobj->target->scale)>>2));

		curstate = ((mobj->tics == 1) ? (mobj->state->nextstate) : ((statenum_t)(mobj->state-states)));

		if (mobj->target->player->flamedash)
		{
			if (!(curstate >= S_FLAMESHIELDDASH1 && curstate <= S_FLAMESHIELDDASH12))
				P_SetMobjState(mobj, S_FLAMESHIELDDASH1);

			if (curstate == S_FLAMESHIELDDASH2)
				underlayst = S_FLAMESHIELDDASH2_UNDERLAY;
			else if (curstate == S_FLAMESHIELDDASH5)
				underlayst = S_FLAMESHIELDDASH5_UNDERLAY;
			else if (curstate == S_FLAMESHIELDDASH8)
				underlayst = S_FLAMESHIELDDASH8_UNDERLAY;
			else if (curstate == S_FLAMESHIELDDASH11)
				underlayst = S_FLAMESHIELDDASH11_UNDERLAY;

			if (leveltime & 1)
			{
				UINT8 i;
				UINT8 nl = 2;

				if (mobj->target->player->flamedash > mobj->extravalue1)
					nl = 3;

				for (i = 0; i < nl; i++)
				{
					mobj_t *fast = P_SpawnMobj(mobj->x + (P_RandomRange(-36,36) * mobj->scale),
						mobj->y + (P_RandomRange(-36,36) * mobj->scale),
						mobj->z + (mobj->height/2) + (P_RandomRange(-20,20) * mobj->scale),
						MT_FASTLINE);

					P_InitAngle(fast, mobj->angle);
					fast->momx = 3*mobj->target->momx/4;
					fast->momy = 3*mobj->target->momy/4;
					fast->momz = 3*P_GetMobjZMovement(mobj->target)/4;

					K_MatchGenericExtraFlags(fast, mobj);
					P_SetMobjState(fast, S_FLAMESHIELDLINE1 + i);
				}
			}
		}
		else
		{
			if (curstate >= S_FLAMESHIELDDASH1 && curstate <= S_FLAMESHIELDDASH12)
				P_SetMobjState(mobj, S_FLAMESHIELD1);
		}

		mobj->extravalue1 = mobj->target->player->flamedash;

		if (mobj->target->player->flamemeter > flamemax)
		{
			mobj_t *flash = P_SpawnMobj(mobj->x + mobj->target->momx, mobj->y + mobj->target->momy, mobj->z + mobj->target->momz, MT_THOK);
			P_SetMobjState(flash, S_FLAMESHIELDFLASH);

			if (leveltime & 1)
			{
				flash->frame |= 2 + ((leveltime / 2) % 4);
			}
			else
			{
				flash->frame |= ((leveltime / 2) % 2);
			}
		}

		if (!splitscreen /*&& rendermode != render_soft*/)
		{
			angle_t viewingangle;

			if (players[displayplayers[0]].awayviewtics)
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, players[displayplayers[0]].awayviewmobj->x, players[displayplayers[0]].awayviewmobj->y);
			else if (!camera[0].chase && players[displayplayers[0]].mo)
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, players[displayplayers[0]].mo->x, players[displayplayers[0]].mo->y);
			else
				viewingangle = R_PointToAngle2(mobj->target->x, mobj->target->y, camera[0].x, camera[0].y);

			if (curstate >= S_FLAMESHIELD1 && curstate < S_FLAMESHIELDDASH1 && ((curstate-S_FLAMESHIELD1) & 1))
				viewingangle += ANGLE_180;

			destx = mobj->target->x + P_ReturnThrustX(mobj->target, viewingangle, mobj->scale>>4);
			desty = mobj->target->y + P_ReturnThrustY(mobj->target, viewingangle, mobj->scale>>4);
		}
		else
		{
			destx = mobj->target->x;
			desty = mobj->target->y;
		}

		P_MoveOrigin(mobj, destx, desty, mobj->target->z);
		mobj->angle = K_MomentumAngle(mobj->target);

		if (underlayst != S_NULL)
		{
			mobj_t *underlay = P_SpawnMobj(mobj->target->x, mobj->target->y, mobj->target->z, MT_FLAMESHIELDUNDERLAY);
			P_InitAngle(underlay, mobj->angle);
			P_SetMobjState(underlay, underlayst);
		}
		break;
	}
	case MT_ROCKETSNEAKER:
		if (!mobj->target || !mobj->target->health)
		{
			P_RemoveMobj(mobj);
			return false;
		}
		if (mobj->target->player && !mobj->target->player->rocketsneakertimer)
		{
			mobj->flags &= ~MF_NOGRAVITY;
			mobj->angle += ANGLE_45;

			if (!mobj->extravalue2)
			{
				K_DropRocketSneaker(mobj->target->player);
			}
			else if (P_IsObjectOnGround(mobj))
			{
				P_RemoveMobj(mobj);
				return false;
			}
		}
		break;
	case MT_KARMAHITBOX:
		{
			statenum_t state = (mobj->state-states);

			if (!mobj->target || !mobj->target->health || !mobj->target->player || mobj->target->player->spectator
				|| (gametype == GT_RACE || mobj->target->player->bumpers))
			{
				P_RemoveMobj(mobj);
				return false;
			}

			P_MoveOrigin(mobj, mobj->target->x, mobj->target->y, mobj->target->z);
			mobj->angle = mobj->target->angle;
			mobj->scalespeed = mobj->target->scalespeed;
			mobj->destscale = mobj->target->destscale;
			P_SetScale(mobj, mobj->target->scale);
			mobj->color = mobj->target->color;
			mobj->colorized = true;

			mobj->radius = 24*mobj->target->scale;
			mobj->height = 2*mobj->radius;

			if (mobj->target->player->karmadelay > 0)
			{
				if (state < S_PLAYERBOMB1 || state > S_PLAYERBOMB20)
					P_SetMobjState(mobj, S_PLAYERBOMB1);
				if (mobj->target->player->karmadelay < TICRATE && (leveltime & 1))
					mobj->renderflags &= ~RF_DONTDRAW;
				else
					mobj->renderflags |= RF_DONTDRAW;
			}
			else
			{
				if (state < S_PLAYERBOMB1 || state > S_PLAYERBOMB20)
					P_SetMobjState(mobj, S_PLAYERBOMB1);

				if (mobj->target->player->flashing && (leveltime & 1))
					mobj->renderflags |= RF_DONTDRAW;
				else
					mobj->renderflags &= ~RF_DONTDRAW;
			}

			// Update mobj antigravity status:
			mobj->eflags = (mobj->eflags & ~MFE_VERTICALFLIP)|(mobj->target->eflags & MFE_VERTICALFLIP);
			mobj->flags2 = (mobj->flags2 & ~MF2_OBJECTFLIP)|(mobj->target->flags2 & MF2_OBJECTFLIP);

			// Now for the wheels
			{
				const fixed_t rad = FixedMul(mobjinfo[MT_PLAYER].radius, mobj->target->scale);
				mobj_t *cur = mobj->hnext;

				while (cur && !P_MobjWasRemoved(cur))
				{
					fixed_t offx = rad;
					fixed_t offy = rad;

					if (cur->lastlook == 1 || cur->lastlook == 3)
						offx *= -1;
					if (cur->lastlook == 2 || cur->lastlook == 3)
						offy *= -1;

					P_MoveOrigin(cur, mobj->x + offx, mobj->y + offy, mobj->z);
					cur->scalespeed = mobj->target->scalespeed;
					cur->destscale = mobj->target->destscale;
					P_SetScale(cur, mobj->target->scale);
					cur->color = mobj->target->color;
					cur->colorized = true;
					K_FlipFromObject(cur, mobj->target);

					if (mobj->renderflags & RF_DONTDRAW)
						cur->renderflags |= RF_DONTDRAW;
					else
						cur->renderflags &= ~RF_DONTDRAW;

					cur = cur->hnext;
				}
			}
		}
		break;
		case MT_SIGN: // Kart's unique sign behavior
			if (mobj->movecount)
			{
				if (mobj->z <= mobj->movefactor)
				{
					P_SetMobjState(mobj, S_SIGN_END);
					if (mobj->info->attacksound)
						S_StartSound(mobj, mobj->info->attacksound);
					mobj->flags |= MF_NOGRAVITY; // ?
					mobj->flags &= ~MF_NOCLIPHEIGHT;
					mobj->z = mobj->movefactor;
					mobj->movecount = 0;
				}
				else
				{
					P_SpawnMobj(mobj->x + (P_RandomRange(-48,48)*mobj->scale),
						mobj->y + (P_RandomRange(-48,48)*mobj->scale),
						mobj->z + (24*mobj->scale) + (P_RandomRange(-8,8)*mobj->scale),
						MT_SIGNSPARKLE);
					mobj->flags &= ~MF_NOGRAVITY;
					if (abs(mobj->z - mobj->movefactor) <= (512*mobj->scale) && !mobj->cvmem)
					{
						if (mobj->info->seesound)
							S_StartSound(mobj, mobj->info->seesound);
						mobj->cvmem = 1;
					}
				}
			}
			break;
	case MT_CDUFO:
		if (!mobj->spawnpoint || mobj->fuse)
			break;

		if (mobj->movecount)
		{
			mobj->movecount--;
			break;
		}
		else if (P_AproxDistance(mobj->x - (mobj->spawnpoint->x<<FRACBITS), mobj->y - (mobj->spawnpoint->y<<FRACBITS)) < (420<<FRACBITS))
			break;

		mobj->movecount = 3;

		{
			angle_t facing = P_RandomRange(0, 90);
			if (facing >= 45)
				facing = InvAngle((facing - 45)*ANG1);
			else
				facing *= ANG1;

			mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->spawnpoint->x<<FRACBITS, mobj->spawnpoint->y<<FRACBITS) + facing;
		}
		break;
	case MT_FZEROBOOM: // F-Zero explosion
		if (!mobj->extravalue1)
		{
			fixed_t mx = P_ReturnThrustX(NULL, mobj->angle, 32*mobj->scale);
			fixed_t my = P_ReturnThrustY(NULL, mobj->angle, 32*mobj->scale);
			mobj_t *explosion = P_SpawnMobj(mobj->x + (2*mx), mobj->y + (2*my), mobj->z+(mobj->height/2), MT_THOK);

			P_SetMobjState(explosion, S_FZEROBOOM1);
			explosion->scale = mobj->scale*2;
			explosion->momx = mx;
			explosion->momy = my;

			S_StartSound(mobj, mobj->info->seesound);
			mobj->extravalue1 = 1;
		}

		if (mobj->extravalue1 != 2 && !S_SoundPlaying(mobj, mobj->info->attacksound))
			S_StartSound(mobj, mobj->info->attacksound);

		if (mobj->extravalue2 <= 8) // Short delay
			mobj->extravalue2++; // flametimer
		else // fire + smoke pillar
		{
			UINT8 i;
			mobj_t *fire = P_SpawnMobj(mobj->x + (P_RandomRange(-32, 32)*mobj->scale), mobj->y + (P_RandomRange(-32, 32)*mobj->scale), mobj->z, MT_THOK);

			fire->sprite = SPR_FPRT;
			fire->frame = FF_FULLBRIGHT|FF_TRANS30;
			fire->scale = mobj->scale*4;
			fire->momz = P_RandomRange(2, 3)*mobj->scale;
			fire->scalespeed = mobj->scale/12;
			fire->destscale = 1;
			fire->tics = TICRATE;

			for (i = 0; i < 2; i++)
			{
				mobj_t *smoke = P_SpawnMobj(mobj->x + (P_RandomRange(-16, 16)*mobj->scale), mobj->y + (P_RandomRange(-16, 16)*mobj->scale), mobj->z, MT_SMOKE);

				P_SetMobjState(smoke, S_FZSLOWSMOKE1);
				smoke->scale = mobj->scale;
				smoke->momz = P_RandomRange(3, 10)*mobj->scale;
				smoke->destscale = mobj->scale*4;
				smoke->scalespeed = mobj->scale/24;
			}
		}
		break;
	case MT_EZZPROPELLER:
		if (mobj->hnext)
		{
			mobj_t *cur = mobj->hnext;

			while (cur && !P_MobjWasRemoved(cur))
			{
				cur->angle += FixedAngle(mobj->info->speed);
				P_MoveOrigin(cur, mobj->x + FINECOSINE((cur->angle*8)>>ANGLETOFINESHIFT),
					mobj->y + FINESINE((cur->angle*8)>>ANGLETOFINESHIFT), mobj->z);
				//P_SpawnGhostMobj(cur)->tics = 2;

				cur = cur->hnext;
			}
		}
		if (!S_SoundPlaying(mobj, mobj->info->seesound))
			S_StartSound(mobj, mobj->info->seesound);
		break;
	case MT_FROGGER:
		{
			statenum_t frogstate = (mobj->state-states);

			// FROG ATTACK VALUES:
			// threshold: distance
			// movecount: time
			// lastlook: direction
			// extravalue1: x step
			// extravalue2: y step
			// cusval: z step

			if (frogstate == S_FROGGER)
			{
				mobj->threshold = mobj->movecount = mobj->lastlook = 0; // clear tongue attack
				mobj->extravalue1 = mobj->extravalue2 = mobj->cusval = 0;
				if (mobj->hnext) // Clean hnext list
				{
					mobj_t *cur = mobj->hnext;
					while (cur && !P_MobjWasRemoved(cur))
					{
						mobj_t *next = cur->hnext;
						P_RemoveMobj(cur);
						cur = next;
					}
				}

				if (mobj->reactiontime)
					mobj->reactiontime--;
				else
				{
					if (mobj->flags2 & MF2_AMBUSH)
					{
						mobj->momz = P_RandomRange(12, 16)<<FRACBITS;
						S_StartSound(mobj, sfx_s3kb1);
						P_SetMobjState(mobj, S_FROGGER_JUMP);
					}
					else
					{
						mobj_t *tongue = P_SpawnMobj(mobj->x, mobj->y, mobj->z + (mobj->height/2), MT_FROGTONGUE);
						P_SetTarget(&mobj->tracer, tongue);
						P_SetMobjState(mobj, S_FROGGER_ATTACK);
					}
				}
			}
			else if (frogstate == S_FROGGER_ATTACK)
			{
				if (!mobj->tracer || P_MobjWasRemoved(mobj->tracer))
				{
					mobj->reactiontime = mobj->info->reactiontime;
					P_SetMobjState(mobj, S_FROGGER);
					break;
				}

				if (mobj->threshold == 0)
				{
					fixed_t targetz = mobj->tracer->z; //mobj->z + (mobj->height/2)

					mobj->threshold = 256;
					mobj->movecount = 1;
					mobj->lastlook = 1;

					mobj->tracer->angle = mobj->angle;

					mobj->extravalue1 = FixedMul(FixedMul((mobj->threshold/16)<<FRACBITS,
						FINECOSINE(((angle_t)targetz)>>ANGLETOFINESHIFT)),
						FINECOSINE(mobj->angle>>ANGLETOFINESHIFT)) >> FRACBITS;

					mobj->extravalue2 = FixedMul(FixedMul((mobj->threshold/16)<<FRACBITS,
						FINECOSINE(((angle_t)targetz)>>ANGLETOFINESHIFT)),
						FINESINE(mobj->angle>>ANGLETOFINESHIFT)) >> FRACBITS;

					mobj->cusval = FixedMul((mobj->threshold/16)<<FRACBITS,
						FINESINE(((angle_t)targetz)>>ANGLETOFINESHIFT)) >> FRACBITS;

					S_StartSound(mobj, sfx_s3k8c); // Play that tongue-y sound.
				}

				mobj->movecount += mobj->lastlook;

				if (!(P_TryMove(mobj->tracer, mobj->x + ((mobj->extravalue1<<FRACBITS) * mobj->movecount), mobj->y + ((mobj->extravalue2<<FRACBITS) * mobj->movecount), true))
					|| (mobj->movecount >= 16) // maximum travel time
					|| (mobj->tracer->z <= mobj->tracer->floorz) // Through the floor
					|| ((mobj->tracer->z + mobj->tracer->height) >= mobj->tracer->ceilingz)) // Through the ceiling
				{
					mobj->lastlook = -1; // Reverse direction.
				}

				if (mobj->movecount == 0) // It's back to its source, time to reset.
				{
					mobj->threshold = mobj->lastlook = 0;

					P_RemoveMobj(mobj->tracer);

					if (mobj->hnext) // Clean hnext list
					{
						mobj_t *cur = mobj->hnext;
						while (cur && !P_MobjWasRemoved(cur))
						{
							mobj_t *next = cur->hnext;
							P_RemoveMobj(cur);
							cur = next;
						}
					}

					mobj->reactiontime = mobj->info->reactiontime;
					P_SetMobjState(mobj, S_FROGGER);
				}
				else
				{
					const UINT8 numjoints = 11;
					UINT8 joint = numjoints;
					mobj_t *cur = mobj->hnext, *prev = mobj;

					mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->tracer->x, mobj->tracer->y);

					for (; joint > 0; joint--)
					{
						fixed_t wx = mobj->tracer->x + (joint * (mobj->x - mobj->tracer->x) / (numjoints+1));
						fixed_t wy = mobj->tracer->y + (joint * (mobj->y - mobj->tracer->y) / (numjoints+1));
						fixed_t wz = mobj->tracer->z + (joint * ((mobj->z + (mobj->height/2)) - mobj->tracer->z) / (numjoints+1));

						if (cur && !P_MobjWasRemoved(cur))
							P_MoveOrigin(cur, wx, wy, wz);
						else
							cur = P_SpawnMobj(wx, wy, wz, MT_FROGTONGUE_JOINT);

						P_SetTarget(&cur->target, mobj);

						P_SetTarget(&prev->hnext, cur);
						P_SetTarget(&cur->hprev, prev);

						prev = cur;
						cur = cur->hnext;
					}
				}
			}
			else if (frogstate == S_FROGGER_JUMP)
			{
				if (P_IsObjectOnGround(mobj))
				{
					mobj->reactiontime = mobj->info->reactiontime;
					P_SetMobjState(mobj, S_FROGGER);
				}
			}
		}
		break;
	case MT_ROBRA:
	case MT_BLUEROBRA:
		if (mobj->health)
		{
			boolean blue = (mobj->type == MT_BLUEROBRA);

			if (blue)
			{
				if (mobj->spawnpoint)
					mobj->extravalue2 = mobj->spawnpoint->angle;
				else
					mobj->extravalue2 = 128;
			}
			else
			{
				if (!mobj->extravalue2)
					mobj->extravalue2 = P_RandomRange(64, 192);
			}

			if (mobj->reactiontime)
				mobj->reactiontime--;
			else
			{
				if (!mobj->extravalue1)
				{
					mobj_t *head = P_SpawnMobj(mobj->x, mobj->y, mobj->z, (blue ? MT_BLUEROBRA_HEAD : MT_ROBRA_HEAD));
					P_SetTarget(&mobj->tracer, head);

					mobj->destscale = mapobjectscale;
					P_SetTarget(&mobj->tracer->target, mobj->target);
					P_SetTarget(&mobj->tracer->tracer, mobj);
					mobj->tracer->extravalue2 = mobj->extravalue2;

					if (!blue)
						mobj->tracer->angle = mobj->angle;

					mobj->extravalue1 = 1;
				}
			}

			if ((mobj->extravalue1) && !(mobj->tracer && !P_MobjWasRemoved(mobj->tracer)))
			{
				mobj->reactiontime = 20*mobj->info->reactiontime;
				P_SetTarget(&mobj->target, NULL);
				mobj->extravalue1 = 0;
			}

			if ((mobj->tracer && !P_MobjWasRemoved(mobj->tracer)) && !(leveltime % 10))
			{
				mobj_t *dust = P_SpawnMobj(mobj->x + (P_RandomRange(-4, 4)<<FRACBITS),
					mobj->y + (P_RandomRange(-4, 4)<<FRACBITS),
					mobj->z + (P_RandomRange(0, 2)<<FRACBITS), MT_BBZDUST);
				P_SetScale(dust, mobj->scale/2);
				P_InstaThrust(dust, FixedAngle(P_RandomRange(0,359)<<FRACBITS), abs(mobj->tracer->momz)/2);

				if (abs(mobj->tracer->momz) >= 2<<FRACBITS)
					S_StartSound(mobj, sfx_s3k7e);
			}
		}
		break;
	case MT_ROBRA_HEAD:
	case MT_BLUEROBRA_HEAD:
		if (mobj->health)
		{
			boolean blue = (mobj->type == MT_BLUEROBRA_HEAD);
			UINT8 locnumsegs = abs(mobj->z - mobj->floorz) / (32 * mobj->scale);
			UINT8 i;
			mobj_t *cur = mobj->hnext, *prev = mobj;

			if (blue)
				mobj->angle = (angle_t)mobj->extravalue1;
			mobj->extravalue1 += (FixedAngle(2*mobj->momz) * (blue ? -1 : 1));

			for (i = 0; i < locnumsegs*2; i++) // *2 to check for any extra segs still present
			{
				fixed_t segz = mobj->z - ((i+1) * (32 * mobj->scale));

				if (cur && !P_MobjWasRemoved(cur))
				{
					if (i >= locnumsegs) // Remove extras
					{
						mobj_t *next = cur->hnext;
						P_RemoveMobj(cur);
						cur = next;
						continue;
					}
					else // Move into place
						P_MoveOrigin(cur, mobj->x, mobj->y, segz);
				}
				else
				{
					if (i >= locnumsegs) // We're done with this list
						continue; //break;
					else // Need another here!
						cur = P_SpawnMobj(mobj->x, mobj->y, segz, (blue ? MT_BLUEROBRA_JOINT : MT_ROBRA_JOINT));
				}

				P_SetTarget(&cur->target, mobj);
				P_SetScale(cur, (7*mobj->scale)/8);

				cur->angle = mobj->extravalue1;
				mobj->extravalue1 += (FixedAngle(2*mobj->momz) * (blue ? -1 : 1));

				P_SetTarget(&prev->hnext, cur);
				P_SetTarget(&cur->hprev, prev);

				prev = cur;
				cur = cur->hnext;
			}

			{
				//fixed_t ceilingheight = mobj->ceilingz - (72<<FRACBITS);
				fixed_t floorheight = mobj->floorz + (72<<FRACBITS);
				fixed_t maxheight = mobj->floorz + (mobj->extravalue2<<FRACBITS);
				fixed_t targetheight = maxheight;

				if (mobj->z < targetheight)
				{
					mobj->momz += mobj->info->speed;
					if ((mobj->z < floorheight) && (mobj->momz < 0))
						mobj->momz /= 2;
				}
				else
				{
					mobj->momz -= mobj->info->speed;
					if ((mobj->z > (targetheight + (64<<FRACBITS))) && (mobj->momz > 0))
						mobj->momz /= 2;
				}
			}
		}
		break;
	case MT_ROBRA_JOINT:
	case MT_BLUEROBRA_JOINT:
		if (!mobj->target || P_MobjWasRemoved(mobj->target))
		{
			P_RemoveMobj(mobj);
			return false;
		}
		break;
	case MT_SMK_PIPE:
		if (mobj->flags2 & MF2_AMBUSH)
			P_SetMobjStateNF(mobj, mobj->info->seestate);
		else
			P_SetMobjStateNF(mobj, mobj->info->spawnstate);
		break;
	case MT_SMK_MOLESPAWNER:
		if (!mobj->target || P_MobjWasRemoved(mobj->target))
		{
			mobj_t *newmole = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_SMK_MOLE);
			P_SetTarget(&mobj->target, newmole);
			return false;
		}
		break;
	case MT_SMK_MOLE:
		if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
		{
			player_t *player = mobj->target->player;

			mobj->extravalue1 = 1;
			player->offroad += 2<<FRACBITS;

			P_MoveOrigin(mobj,
				player->mo->x + P_ReturnThrustX(NULL, player->mo->angle, player->mo->radius)
					+ P_ReturnThrustX(NULL, player->mo->angle+ANGLE_90, (mobj->threshold)<<FRACBITS),
				player->mo->y + P_ReturnThrustY(NULL, player->mo->angle, player->mo->radius)
					+ P_ReturnThrustY(NULL, player->mo->angle+ANGLE_90, (mobj->threshold)<<FRACBITS),
				player->mo->z + (player->mo->height/2 * P_MobjFlip(player->mo))
					+ (P_RandomRange(-abs(mobj->threshold), abs(mobj->threshold))<<FRACBITS));

			mobj->threshold /= 2;
			mobj->momz = 0;

			if (mobj->movecount > 8*TICRATE)
			{
				P_KillMobj(mobj, mobj->target, mobj->target, DMG_NORMAL);
				break;
			}

			if (abs(player->cmd.turning) > 100)
			{
				INT32 lastsign = 0;
				if (mobj->lastlook > 0)
					lastsign = 1;
				else if (mobj->lastlook < 0)
					lastsign = -1;

				if ((player->cmd.turning > 0 && lastsign < 0)
					|| (player->cmd.turning < 0 && lastsign > 0))
				{
					mobj->movecount += (TICRATE/2);
					mobj->threshold = 16*lastsign;
					S_StartSound(mobj, sfx_s1ab);
				}

				mobj->lastlook = player->cmd.turning;
			}

			mobj->movecount++;
		}
		else if (mobj->extravalue1) // lost your player somehow, DIE
		{
			P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
			break;
		}
		else
		{
			if (P_IsObjectOnGround(mobj))
			{
				if (mobj->reactiontime)
					mobj->reactiontime--;
				else
				{
					mobj->momz = (mobj->info->speed * P_MobjFlip(mobj));
					mobj->reactiontime = mobj->info->reactiontime;
				}
			}
		}
		break;
	case MT_SMK_THWOMP:
		if (mobj->flags2 & MF2_AMBUSH)
		{
			mobj->colorized = true;
			mobj->color = K_RainbowColor(leveltime);
			mobj->frame |= FF_FULLBRIGHT;
		}
		else
		{
			mobj->colorized = false;
			mobj->color = SKINCOLOR_NONE;
			mobj->frame &= (~FF_FULLBRIGHT);
		}

		if (!thwompsactive)
			break;

		if (mobj->reactiontime)
			mobj->reactiontime--;
		else
		{
			if (mobj->extravalue1)
			{
				P_SpawnGhostMobj(mobj)->tics = 3;

				if (mobj->z == mobj->floorz)
				{
					UINT8 i;

					mobj->extravalue1 = 0;
					mobj->reactiontime = mobj->info->reactiontime;
					S_StartSound(mobj, sfx_s1bd);

					for (i = 0; i < 8; i++)
					{
						mobj_t *dust = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_DRIFTDUST);
						P_InstaThrust(dust, FixedAngle(((360*FRACUNIT)/8) * i), mobj->info->speed/8);
						dust->momz = P_MobjFlip(mobj) * (P_RandomRange(1,4)<<FRACBITS);
						dust->scale = mobj->scale/2;
						dust->destscale = mobj->scale*3;
					}
				}
				else
					mobj->momz = (-mobj->info->speed) * P_MobjFlip(mobj);
			}
			else
			{
				if (mobj->z > mobj->movefactor)
					mobj->z = mobj->movefactor;

				if (mobj->z == mobj->movefactor)
				{
					mobj->extravalue1 = 1;
					//S_StartSound(mobj, sfx_s1bb);
				}
				else
					mobj->momz = (mobj->info->speed/16) * P_MobjFlip(mobj);
			}
		}
		break;
	case MT_BUBBLESHIELDTRAP:
		if (leveltime % 180 == 0)
			S_StartSound(mobj, sfx_s3kbfl);

		if (mobj->tracer && !P_MobjWasRemoved(mobj->tracer) && mobj->tracer->player)
		{
			player_t *player = mobj->tracer->player;
			fixed_t destx, desty, curfz, destfz;
			boolean blockmove = false;

			mobj->flags = MF_NOCLIP|MF_NOCLIPHEIGHT|MF_NOCLIPTHING|MF_NOGRAVITY|MF_DONTENCOREMAP;
			mobj->extravalue1 = 1;

			mobj->cvmem /= 2;
			mobj->momz = 0;
			mobj->destscale = ((5*mobj->tracer->scale)>>2) + (mobj->tracer->scale>>3);

			mobj->tracer->momz = (8*mobj->tracer->scale) * P_MobjFlip(mobj->tracer);

			mobj->tracer->momx = (31*mobj->tracer->momx)/32;
			mobj->tracer->momy = (31*mobj->tracer->momy)/32;

			destx = mobj->x + mobj->tracer->momx;
			desty = mobj->y + mobj->tracer->momy;

			if (mobj->tracer->eflags & MFE_VERTICALFLIP)
			{
				curfz = P_GetCeilingZ(mobj->tracer, mobj->tracer->subsector->sector, mobj->tracer->x, mobj->tracer->y, NULL);
				destfz = P_GetCeilingZ(mobj->tracer, R_PointInSubsector(destx, desty)->sector, destx, desty, NULL);
				blockmove = (curfz - destfz >= 24*mobj->scale);
			}
			else
			{
				curfz = P_GetFloorZ(mobj->tracer, mobj->tracer->subsector->sector, mobj->tracer->x, mobj->tracer->y, NULL);
				destfz = P_GetFloorZ(mobj->tracer, R_PointInSubsector(destx, desty)->sector, destx, desty, NULL);
				blockmove = (destfz - curfz >= 24*mobj->scale);
			}

			if (blockmove)
			{
				mobj->tracer->momx = mobj->tracer->momy = 0;
			}

			P_MoveOrigin(mobj,
				mobj->tracer->x + P_ReturnThrustX(NULL, mobj->tracer->angle+ANGLE_90, (mobj->cvmem)<<FRACBITS),
				mobj->tracer->y + P_ReturnThrustY(NULL, mobj->tracer->angle+ANGLE_90, (mobj->cvmem)<<FRACBITS),
				mobj->tracer->z - (4*mobj->tracer->scale) + (P_RandomRange(-abs(mobj->cvmem), abs(mobj->cvmem))<<FRACBITS));

			if (mobj->movecount > 4*TICRATE)
			{
				S_StartSound(mobj->tracer, sfx_s3k77);
				mobj->tracer->flags &= ~MF_NOGRAVITY;
				P_KillMobj(mobj, mobj->tracer, mobj->tracer, DMG_NORMAL);
				break;
			}

			if (abs(player->cmd.turning) > 100)
			{
				INT32 lastsign = 0;
				if (mobj->lastlook > 0)
					lastsign = 1;
				else if (mobj->lastlook < 0)
					lastsign = -1;

				if ((player->cmd.turning > 0 && lastsign < 0)
					|| (player->cmd.turning < 0 && lastsign > 0))
				{
					mobj->movecount += (TICRATE/2);
					mobj->cvmem = 8*lastsign;
					S_StartSound(mobj, sfx_s3k7a);
				}

				mobj->lastlook = player->cmd.turning;
			}

			mobj->movecount++;
		}
		else if (mobj->extravalue1) // lost your player somehow, DIE
		{
			P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
			break;
		}
		else
		{
			mobj->destscale = (5*mapobjectscale)>>2;

			if (mobj->threshold > 0)
				mobj->threshold--;

			if (abs(mobj->momx) < 8*mobj->destscale && abs(mobj->momy) < 8*mobj->destscale)
			{
				// Stop, give light gravity
				mobj->momx = mobj->momy = 0;
				mobj->momz = -(mobj->scale * P_MobjFlip(mobj));
			}
			else
			{
				UINT8 i;
				mobj_t *ghost = P_SpawnGhostMobj(mobj);

				if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
				{
					ghost->color = mobj->target->player->skincolor;
					ghost->colorized = true;
				}

				mobj->momx = (23*mobj->momx)/24;
				mobj->momy = (23*mobj->momy)/24;

				mobj->angle = K_MomentumAngle(mobj);

				if ((mobj->z - mobj->floorz) < (24*mobj->scale) && (leveltime % 3 != 0))
				{
					// Cool wave effects!
					for (i = 0; i < 2; i++)
					{
						angle_t aoff;
						SINT8 sign = 1;
						mobj_t *wave;

						if (i & 1)
							sign = -1;
						else
							sign = 1;

						aoff = (mobj->angle + ANGLE_180) + (ANGLE_45 * sign);

						wave = P_SpawnMobj(mobj->x + FixedMul(mobj->radius, FINECOSINE(aoff>>ANGLETOFINESHIFT)),
							mobj->y + FixedMul(mobj->radius, FINESINE(aoff>>ANGLETOFINESHIFT)),
							mobj->z, MT_THOK);

						wave->flags &= ~(MF_NOCLIPHEIGHT|MF_NOGRAVITY);
						P_SetScale(wave, (wave->destscale = mobj->scale/2));

						P_SetMobjState(wave, S_BUBBLESHIELDWAVE1);
						if (leveltime & 1)
							wave->tics++;

						P_SetTarget(&wave->target, mobj);
						wave->angle = mobj->angle - (ANGLE_90 * sign); // point completely perpendicular from the bubble
						K_FlipFromObject(wave, mobj);

						P_Thrust(wave, wave->angle, 4*mobj->scale);
					}
				}
			}
		}
		break;
	case MT_TUMBLEGEM:
	case MT_TUMBLECOIN:
		mobj->friction = 95*FRACUNIT/100;
		if ((((statenum_t)(mobj->state-states)) == mobj->info->seestate)
			&& abs(mobj->momx) < mobj->scale && abs(mobj->momy) < mobj->scale && P_IsObjectOnGround(mobj))
		{
			mobj->momx = mobj->momy = 0;
			P_SetMobjState(mobj, mobj->info->spawnstate);
		}
			break;
	case MT_KARMAFIREWORK:
		if (mobj->flags & MF_NOGRAVITY)
			break;

		if (mobj->momz == 0)
		{
			P_RemoveMobj(mobj);
			return false;
		}
		else
		{
			mobj_t *trail = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_THOK);
			P_SetMobjState(trail, S_KARMAFIREWORKTRAIL);
			P_SetScale(trail, mobj->scale);
			trail->destscale = 1;
			trail->scalespeed = mobj->scale/12;
			trail->color = mobj->color;
		}
		break;
	case MT_BATTLECAPSULE:
		{
			SINT8 realflip = P_MobjFlip(mobj);
			SINT8 flip = realflip; // Flying capsules needs flipped sprites, but not flipped gravity
			fixed_t bottom;
			mobj_t *cur;

			if (mobj->extravalue1)
			{
				const INT32 speed = 6*TICRATE; // longer is slower
				fixed_t sine = FINESINE((((M_TAU_FIXED * speed) * leveltime) >> ANGLETOFINESHIFT) & FINEMASK) * flip;

				// Flying capsules are flipped upside-down, like S3K
				flip = -flip;

				// ALL CAPSULE MOVEMENT NEEDS TO HAPPEN AFTER THIS & ADD TO MOMENTUM FOR BOBBING TO BE ACCURATE
				mobj->momz = sine/2;
			}

			// Moving capsules
			if (mobj->target && !P_MobjWasRemoved(mobj->target))
			{
				fixed_t speed = mobj->movefactor;
				UINT8 sequence = mobj->lastlook;
				boolean backandforth = (mobj->flags2 & MF2_AMBUSH);
				SINT8 direction = mobj->cvmem;
				mobj_t *next = NULL;
				fixed_t dist, momx, momy, momz;

				dist = P_AproxDistance(mobj->target->x - mobj->x, mobj->target->y - mobj->y);
				if (mobj->extravalue1)
					dist = P_AproxDistance(dist, mobj->target->z - mobj->z);
				if (dist < 1)
					dist = 1;

				if (speed <= dist)
				{
					momx = FixedMul(FixedDiv(mobj->target->x - mobj->x, dist), speed);
					momy = FixedMul(FixedDiv(mobj->target->y - mobj->y, dist), speed);
					if (mobj->extravalue1)
						momz = mobj->momz + FixedMul(FixedDiv(mobj->target->z - mobj->z, dist), speed);

					mobj->momx = momx;
					mobj->momy = momy;
					if (mobj->extravalue1)
						mobj->momz = momz;
				}
				else
				{
					speed -= dist;

					P_UnsetThingPosition(mobj);
					mobj->x = mobj->target->x;
					mobj->y = mobj->target->y;
					mobj->z = mobj->target->z;
					P_SetThingPosition(mobj);

					mobj->floorz = mobj->subsector->sector->floorheight;
					mobj->ceilingz = mobj->subsector->sector->ceilingheight;

					// Onto the next waypoint!
					next = (direction < 0) ? P_GetPreviousTubeWaypoint(mobj->target, false) : P_GetNextTubeWaypoint(mobj->target, false);

					// Are we at the end of the waypoint chain?
					// If so, search again for the first/previous waypoint (depending on settings)
					if (next == NULL)
					{
						if (backandforth)
						{
							// Back and forth movement.
							mobj->cvmem = -mobj->cvmem;
							direction = mobj->cvmem;

							next = (direction < 0) ? P_GetPreviousTubeWaypoint(mobj->target, false) : P_GetNextTubeWaypoint(mobj->target, false);
						}
						else
						{
							// Looping circular movement.
							next = (direction < 0) ? P_GetLastTubeWaypoint(sequence) : P_GetFirstTubeWaypoint(sequence);
						}
					}

					if (next && !P_MobjWasRemoved(next))
					{
						P_SetTarget(&mobj->target, next);
						mobj->movecount = next->health;

						dist = P_AproxDistance(mobj->target->x - mobj->x, mobj->target->y - mobj->y);
						if (mobj->extravalue1)
							dist = P_AproxDistance(dist, mobj->target->z - mobj->z);
						if (dist < 1)
							dist = 1;

						momx = FixedMul(FixedDiv(mobj->target->x - mobj->x, dist), speed);
						momy = FixedMul(FixedDiv(mobj->target->y - mobj->y, dist), speed);
						if (mobj->extravalue1)
							momz = mobj->momz + FixedMul(FixedDiv(mobj->target->z - mobj->z, dist), speed);

						mobj->momx = momx;
						mobj->momy = momy;
						if (mobj->extravalue1)
							mobj->momz = momz;
					}
					else
					{
						CONS_Alert(CONS_WARNING, "Moving capsule could not find next waypoint! (seq: %d)\n", sequence);
						P_SetTarget(&mobj->target, NULL);
					}
				}
			}

			if (flip == -1)
				bottom = mobj->z + mobj->height;
			else
				bottom = mobj->z;

			cur = mobj->hnext;

			// Move each piece to the proper position
			while (cur && !P_MobjWasRemoved(cur))
			{
				fixed_t newx = mobj->x;
				fixed_t newy = mobj->y;
				fixed_t newz = bottom;
				statenum_t state = (statenum_t)(cur->state-states);

				cur->scale = mobj->scale;
				cur->destscale = mobj->destscale;
				cur->scalespeed = mobj->scalespeed;

				cur->extravalue2 = mobj->extravalue1;

				cur->flags2 = (cur->flags2 & ~MF2_OBJECTFLIP)|(mobj->flags2 & MF2_OBJECTFLIP);

				if (state == S_BATTLECAPSULE_TOP)
					newz += (80 * mobj->scale * flip);
				else if (state == S_BATTLECAPSULE_BUTTON)
					newz += (108 * mobj->scale * flip);
				else if (state == S_BATTLECAPSULE_SUPPORT
					|| state == S_BATTLECAPSULE_SUPPORTFLY
					|| state == S_KARMAWHEEL)
				{
					fixed_t offx = mobj->radius;
					fixed_t offy = mobj->radius;

					if (cur->extravalue1 & 1)
						offx = -offx;

					if (cur->extravalue1 > 1)
						offy = -offy;

					newx += offx;
					newy += offy;
				}
				else if (state == S_BATTLECAPSULE_SIDE1
					|| state == S_BATTLECAPSULE_SIDE2)
				{
					fixed_t offset = 48 * mobj->scale;
					angle_t angle = (ANGLE_45 * cur->extravalue1);

					newx += FixedMul(offset, FINECOSINE(angle >> ANGLETOFINESHIFT));
					newy += FixedMul(offset, FINESINE(angle >> ANGLETOFINESHIFT));
					newz += (12 * mobj->scale * flip);

					cur->angle = angle + ANGLE_90;
				}

				P_MoveOrigin(cur, newx, newy, newz);

				cur = cur->hnext;
			}
		}
		break;
	case MT_RANDOMITEM:
		if ((leveltime == starttime) && !(gametyperules & GTR_CIRCUIT) && (mobj->flags2 & MF2_BOSSNOTRAP)) // here on map start?
		{
			if (gametyperules & GTR_PAPERITEMS)
			{
				if (battlecapsules == true || bossinfo.boss == true)
				{
					;
				}
				else
				{
					mobj_t *paperspawner = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_PAPERITEMSPOT);
					paperspawner->spawnpoint = mobj->spawnpoint;
					mobj->spawnpoint->mobj = paperspawner;
					P_RemoveMobj(mobj);
					return false;
				}
			}
			// poof into existance
			P_UnsetThingPosition(mobj);
			mobj->flags &= ~(MF_NOCLIPTHING|MF_NOBLOCKMAP);
			mobj->renderflags &= ~RF_DONTDRAW;
			P_SetThingPosition(mobj);
			P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_EXPLODE);
			nummapboxes++;
		}
		// FALLTHRU
	case MT_SPHEREBOX:
		if (gametype == GT_BATTLE && mobj->threshold == 70)
		{
			mobj->color = K_RainbowColor(leveltime);
			mobj->colorized = true;

			if (battleovertime.enabled)
			{
				angle_t ang = FixedAngle((leveltime % 360) << FRACBITS);
				fixed_t z = battleovertime.z;
				fixed_t dist;
				mobj_t *ghost;

				/*if (z < mobj->subsector->sector->floorheight)
					z = mobj->subsector->sector->floorheight;*/

				if (mobj->extravalue1 < 512)
					mobj->extravalue1++;
				dist = mobj->extravalue1 * mapobjectscale;

				P_MoveOrigin(mobj, battleovertime.x + P_ReturnThrustX(NULL, ang, dist),
					battleovertime.y + P_ReturnThrustY(NULL, ang, dist), z);

				ghost = P_SpawnGhostMobj(mobj);
				ghost->fuse = 4;
				ghost->frame |= FF_FULLBRIGHT;
			}
		}
		else
		{
			player_t *player = K_GetItemBoxPlayer(mobj);
			UINT8 color = SKINCOLOR_BLACK;

			if (player != NULL)
			{
				color = player->skincolor;
			}

			mobj->color = color;
			mobj->colorized = false;
		}
		break;
	default:
		// check mobj against possible water content, before movement code
		P_MobjCheckWater(mobj);
		break;
	}
	return true;
}

static void P_FiringThink(mobj_t *mobj)
{
	if (!mobj->target)
		return;

	if (mobj->health <= 0)
		return;

	if (mobj->state->action.acp1 == (actionf_p1)A_Boss1Laser)
	{
		if (mobj->state->tics > 1)
		{
			var1 = mobj->state->var1;
			var2 = mobj->state->var2 & 65535;
			mobj->state->action.acp1(mobj);
		}
	}
	else if (leveltime & 1) // Fire mode
	{
		mobj_t *missile;

		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->target->x, mobj->target->y);
		missile = P_SpawnMissile(mobj, mobj->target, mobj->extravalue1);

		if (missile)
		{
			if (mobj->flags2 & MF2_SUPERFIRE)
				missile->flags2 |= MF2_SUPERFIRE;

			if (mobj->info->attacksound)
				S_StartSound(missile, mobj->info->attacksound);
		}
	}
	else
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->target->x, mobj->target->y);
}

static void K_MineExplodeThink(mobj_t *mobj)
{
	if (mobj->state->action.acp1 == (actionf_p1)A_SSMineExplode)
	{
		if (mobj->state->tics > 1)
		{
			K_MineExplodeAttack(mobj, mobj->info->painchance, (boolean)mobj->state->var1);
		}
	}
}

static void P_MonitorFuseThink(mobj_t *mobj)
{
	mobj_t *newmobj;

	// Special case for ALL monitors.
	// If a box's speed is nonzero, it's allowed to respawn as a WRM/SRM.
	if (mobj->info->speed != 0 && (mobj->flags2 & (MF2_AMBUSH|MF2_STRONGBOX)))
	{
		mobjtype_t spawnchance[64];
		INT32 numchoices = 0, i = 0;

		// This define should make it a lot easier to organize and change monitor weights
#define SETMONITORCHANCES(type, strongboxamt, weakboxamt) \
for (i = ((mobj->flags2 & MF2_STRONGBOX) ? strongboxamt : weakboxamt); i; --i) spawnchance[numchoices++] = type

					//                Type             SRM WRM
		SETMONITORCHANCES(MT_SNEAKERS_BOX, 0, 10); // Super Sneakers
		SETMONITORCHANCES(MT_INVULN_BOX, 2, 0); // Invincibility
		SETMONITORCHANCES(MT_WHIRLWIND_BOX, 3, 8); // Whirlwind Shield
		SETMONITORCHANCES(MT_ELEMENTAL_BOX, 3, 8); // Elemental Shield
		SETMONITORCHANCES(MT_ATTRACT_BOX, 2, 0); // Attraction Shield
		SETMONITORCHANCES(MT_FORCE_BOX, 3, 3); // Force Shield
		SETMONITORCHANCES(MT_ARMAGEDDON_BOX, 2, 0); // Armageddon Shield
		SETMONITORCHANCES(MT_MIXUP_BOX, 0, 1); // Teleporters
		SETMONITORCHANCES(MT_RECYCLER_BOX, 0, 1); // Recycler
		SETMONITORCHANCES(MT_1UP_BOX, 1, 1); // 1-Up
		// =======================================
		//                Total             16  32

#undef SETMONITORCHANCES

		i = P_RandomKey(numchoices); // Gotta love those random numbers!
		newmobj = P_SpawnMobj(mobj->x, mobj->y, mobj->z, spawnchance[i]);
	}
	else
		newmobj = P_SpawnMobj(mobj->x, mobj->y, mobj->z, mobj->type);

	// Transfer flags2 (ambush, strongbox, objectflip)
	newmobj->flags2 = mobj->flags2;
	P_RemoveMobj(mobj); // make sure they disappear
}

static boolean P_FuseThink(mobj_t *mobj)
{
	if (mobj->type == MT_SNAPPER_HEAD || mobj->type == MT_SNAPPER_LEG || mobj->type == MT_MINECARTSEG)
		mobj->renderflags ^= RF_DONTDRAW;

	if (mobj->fuse <= TICRATE && (mobj->type == MT_RANDOMITEM || mobj->type == MT_EGGMANITEM || mobj->type == MT_FALLINGROCK))
		mobj->renderflags ^= RF_DONTDRAW;

	mobj->fuse--;

	if (mobj->fuse)
		return true;

	if (LUA_HookMobj(mobj, MOBJ_HOOK(MobjFuse)) || P_MobjWasRemoved(mobj))
		;
	else if (mobj->info->flags & MF_MONITOR)
	{
		P_MonitorFuseThink(mobj);
		return false;
	}
	else switch (mobj->type)
	{
		// gargoyle and snowman handled in P_PushableThinker, not here
	case MT_SPIKE:
	case MT_WALLSPIKE:
		P_SetMobjState(mobj, mobj->state->nextstate);
		mobj->fuse = mobj->spawnpoint ? mobj->spawnpoint->args[0] : mobj->info->speed;
		break;
	case MT_LAVAFALL:
		if (mobj->state - states == S_LAVAFALL_DORMANT)
		{
			mobj->fuse = 30;
			P_SetMobjState(mobj, S_LAVAFALL_TELL);
			S_StartSound(mobj, mobj->info->seesound);
		}
		else if (mobj->state - states == S_LAVAFALL_TELL)
		{
			mobj->fuse = 40;
			P_SetMobjState(mobj, S_LAVAFALL_SHOOT);
			S_StopSound(mobj);
			S_StartSound(mobj, mobj->info->attacksound);
		}
		else
		{
			mobj->fuse = 30;
			P_SetMobjState(mobj, S_LAVAFALL_DORMANT);
			S_StopSound(mobj);
		}
		return false;
	case MT_RANDOMITEM:
		if (mobj->flags2 & MF2_DONTRESPAWN)
		{
			;
		}
		else if ((gametyperules & GTR_BUMPERS) && (mobj->threshold != 70))
		{
			break;
		}
		else
		{
			mobj_t *newmobj;

			// Respawn from mapthing if you have one!
			if (mobj->spawnpoint)
			{
				P_SpawnMapThing(mobj->spawnpoint);
				newmobj = mobj->spawnpoint->mobj; // this is set to the new mobj in P_SpawnMapThing
			}
			else
				newmobj = P_SpawnMobj(mobj->x, mobj->y, mobj->z, mobj->type);

			// Transfer flags2 (strongbox, objectflip, bossnotrap)
			newmobj->flags2 = mobj->flags2;
			if (mobj->threshold == 70)
				newmobj->threshold = 70;
		}

		P_RemoveMobj(mobj); // make sure they disappear
		return false;
	case MT_ITEMCAPSULE:
		if (mobj->spawnpoint)
			P_SpawnMapThing(mobj->spawnpoint);
		else
		{
			mobj_t *newMobj = P_SpawnMobj(mobj->x, mobj->y, mobj->z, mobj->type);
			newMobj->threshold = mobj->threshold;
			newMobj->movecount = mobj->movecount;
		}
		P_RemoveMobj(mobj);
		return false;
	case MT_SMK_ICEBLOCK:
		{
			mobj_t *cur = mobj->hnext, *next;
			UINT8 i;

			for (i = 0; i < 5; i++)
			{
				mobj_t *debris = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_SMK_ICEBLOCK_DEBRIS);
				P_InitAngle(debris, FixedAngle(P_RandomRange(0,360)<<FRACBITS));
				P_InstaThrust(debris, debris->angle, P_RandomRange(3,18)*(FRACUNIT/4));
				debris->momz = P_RandomRange(4,8)<<FRACBITS;
				if (!i) // kinda hacky :V
					S_StartSound(debris, sfx_s3k82);
			}

			while (cur && !P_MobjWasRemoved(cur))
			{
				next = cur->hnext;
				P_RemoveMobj(cur);
				cur = next;
			}

			P_RemoveMobj(mobj);
			return false;
		}
	case MT_PLAYER:
		break; // don't remove
	default:
		P_SetMobjState(mobj, mobj->info->xdeathstate); // will remove the mobj if S_NULL.
		break;
		// Looking for monitors? They moved to a special condition above.
	}

	return !P_MobjWasRemoved(mobj);
}

//
// P_MobjThinker
//
void P_MobjThinker(mobj_t *mobj)
{
	I_Assert(mobj != NULL);
	I_Assert(!P_MobjWasRemoved(mobj));

	// Set old position (for interpolation)
	mobj->old_x = mobj->x;
	mobj->old_y = mobj->y;
	mobj->old_z = mobj->z;
	mobj->old_angle = mobj->angle;
	mobj->old_pitch = mobj->pitch;
	mobj->old_roll = mobj->roll;

	// Remove dead target/tracer.
	if (mobj->target && P_MobjWasRemoved(mobj->target))
		P_SetTarget(&mobj->target, NULL);
	if (mobj->tracer && P_MobjWasRemoved(mobj->tracer))
		P_SetTarget(&mobj->tracer, NULL);
	if (mobj->hnext && P_MobjWasRemoved(mobj->hnext))
		P_SetTarget(&mobj->hnext, NULL);
	if (mobj->hprev && P_MobjWasRemoved(mobj->hprev))
		P_SetTarget(&mobj->hprev, NULL);

	if (mobj->flags & MF_NOTHINK)
		return;

	if ((mobj->flags & MF_BOSS) && mobj->spawnpoint && (bossdisabled & (1<<mobj->spawnpoint->args[0])))
		return;

	mobj->eflags &= ~(MFE_PUSHED|MFE_SPRUNG|MFE_JUSTBOUNCEDWALL|MFE_SLOPELAUNCHED);

	tmfloorthing = tmhitthing = NULL;

	// Check for sector special actions
	P_CheckMobjTouchingSectorActions(mobj);

	// Sector flag MSF_TRIGGERLINE_MOBJ allows ANY mobj to trigger a linedef exec
	P_CheckMobjTrigger(mobj, false);

	if (mobj->scale != mobj->destscale)
		P_MobjScaleThink(mobj); // Slowly scale up/down to reach your destscale.

	if (mobj->type == MT_GHOST && mobj->fuse > 0) // Not guaranteed to be MF_SCENERY or not MF_SCENERY!
	{
		if (mobj->extravalue1 > 0) // Sonic Advance 2 mode
		{
			if (mobj->extravalue2 >= 2)
			{
				UINT32 dontdraw = RF_DONTDRAW;

				if (mobj->tracer)
					dontdraw &= ~(mobj->tracer->renderflags);

				if (mobj->extravalue2 == 2) // I don't know why the normal logic doesn't work for this.
					mobj->renderflags ^= dontdraw;
				else
				{
					if (mobj->fuse == mobj->extravalue2)
						mobj->renderflags &= ~(dontdraw);
					else
						mobj->renderflags |= dontdraw;
				}
			}
		}
		else
		{
			UINT32 dur = (mobj->flags2 & MF2_BOSSNOTRAP)
				? (2*mobj->fuse)/3
				: mobj->fuse/2;
			if (((mobj->renderflags & RF_TRANSMASK) >> RF_TRANSSHIFT) < ((NUMTRANSMAPS-1) - dur))
				// fade out when nearing the end of fuse...
				mobj->renderflags = (mobj->renderflags & ~RF_TRANSMASK) | (((NUMTRANSMAPS-1) - dur) << RF_TRANSSHIFT);
		}
	}

	// Special thinker for scenery objects
	if (mobj->flags & MF_SCENERY)
	{
		P_MobjSceneryThink(mobj);
		return;
	}

	// Check for a Lua thinker first
	if (!mobj->player)
	{
		if (LUA_HookMobj(mobj, MOBJ_HOOK(MobjThinker)) || P_MobjWasRemoved(mobj))
			return;
	}
	else if (!mobj->player->spectator)
	{
		// You cannot short-circuit the player thinker like you can other thinkers.
		LUA_HookMobj(mobj, MOBJ_HOOK(MobjThinker));
		if (P_MobjWasRemoved(mobj))
			return;
	}

	// if it's pushable, or if it would be pushable other than temporary disablement, use the
	// separate thinker
	if (mobj->flags & MF_PUSHABLE || (mobj->info->flags & MF_PUSHABLE && mobj->fuse))
	{
		if (!P_MobjPushableThink(mobj))
			return;
	}
	else if (mobj->flags & MF_BOSS)
	{
		if (!P_MobjBossThink(mobj))
			return;
	}
	else if (mobj->health <= 0) // Dead things think differently than the living.
	{
		if (!P_MobjDeadThink(mobj))
			return;
	}
	else
	{
		if (!P_MobjRegularThink(mobj))
			return;
	}
	if (P_MobjWasRemoved(mobj))
		return;

	// Destroy items sector special
	if (P_CanDeleteKartItem(mobj->type))
	{
		if (mobj->health > 0 && P_MobjTouchingSectorSpecialFlag(mobj, SSF_DELETEITEMS))
		{
			if (mobj->type == MT_SSMINE
			|| mobj->type == MT_BUBBLESHIELDTRAP
			|| mobj->type == MT_BALLHOG)
			{
				S_StartSound(mobj, mobj->info->deathsound);
				P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);
			}
			else
			{
				// This Item Damage
				if (mobj->eflags & MFE_VERTICALFLIP)
					mobj->z -= mobj->height;
				else
					mobj->z += mobj->height;

				S_StartSound(mobj, mobj->info->deathsound);
				P_KillMobj(mobj, NULL, NULL, DMG_NORMAL);

				P_SetObjectMomZ(mobj, 8*FRACUNIT, false);
				P_InstaThrust(mobj, R_PointToAngle2(0, 0, mobj->momx, mobj->momy) + ANGLE_90, 16*FRACUNIT);
			}

			return;
		}
	}

	if (mobj->flags2 & MF2_FIRING)
		P_FiringThink(mobj);

	if (mobj->flags2 & MF2_DEBRIS)
		K_MineExplodeThink(mobj);

	if (mobj->type == MT_AMBIENT)
	{
		if (leveltime % mobj->health)
			return;
		if (mobj->threshold)
			S_StartSound(mobj, mobj->threshold);
		return;
	}

	// Check fuse
	if (mobj->fuse && !P_FuseThink(mobj))
		return;

	I_Assert(mobj != NULL);
	I_Assert(!P_MobjWasRemoved(mobj));

	if (mobj->momx || mobj->momy || (mobj->flags2 & MF2_SKULLFLY))
	{
		P_XYMovement(mobj);
		if (P_MobjWasRemoved(mobj))
			return;
	}

	// always do the gravity bit now, that's simpler
	// BUT CheckPosition only if wasn't done before.
	if (!(mobj->eflags & MFE_ONGROUND) || mobj->momz
		|| ((mobj->eflags & MFE_VERTICALFLIP) && mobj->z + mobj->height != mobj->ceilingz)
		|| (!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z != mobj->floorz)
		|| P_IsObjectInGoop(mobj))
	{
		if (!P_ZMovement(mobj))
			return; // mobj was removed
		P_CheckPosition(mobj, mobj->x, mobj->y); // Need this to pick up objects!
		if (P_MobjWasRemoved(mobj))
			return;
	}
	else
	{
		mobj->pmomz = 0; // to prevent that weird rocketing gargoyle bug
		mobj->eflags &= ~MFE_JUSTHITFLOOR;
	}

	// Sliding physics for slidey mobjs!
	if (mobj->type == MT_FLINGRING
		|| mobj->type == MT_FLINGCOIN
		|| mobj->type == MT_FLINGBLUESPHERE
		|| mobj->type == MT_FLINGNIGHTSCHIP
		|| mobj->type == MT_EMERALD
		|| mobj->type == MT_BIGTUMBLEWEED
		|| mobj->type == MT_LITTLETUMBLEWEED
		|| mobj->type == MT_CANNONBALLDECOR
		|| mobj->type == MT_FALLINGROCK
		|| mobj->type == MT_ORBINAUT
		|| mobj->type == MT_JAWZ || mobj->type == MT_JAWZ_DUD
		|| (mobj->type == MT_DROPTARGET && mobj->reactiontime))
	{
		P_TryMove(mobj, mobj->x, mobj->y, true); // Sets mo->standingslope correctly

		if (P_MobjWasRemoved(mobj)) // anything that calls checkposition can be lethal
			return;

		//if (mobj->standingslope) CONS_Printf("slope physics on mobj\n");
		P_ButteredSlope(mobj);
	}

	K_UpdateTerrainOverlay(mobj);

	if (mobj->flags & (MF_ENEMY|MF_BOSS) && mobj->health
		&& P_CheckDeathPitCollide(mobj)) // extra pit check in case these didn't have momz
	{
		P_KillMobj(mobj, NULL, NULL, DMG_DEATHPIT);
		return;
	}

	// Crush enemies!
	if (mobj->ceilingz - mobj->floorz < mobj->height)
	{
		if ((
		(mobj->flags & (MF_ENEMY|MF_BOSS)
			&& mobj->flags & MF_SHOOTABLE)
		|| mobj->type == MT_EGGSHIELD)
		&& !(mobj->flags & MF_NOCLIPHEIGHT)
		&& mobj->health > 0)
		{
			P_KillMobj(mobj, NULL, NULL, DMG_CRUSHED);
			return;
		}
	}

	if (P_MobjWasRemoved(mobj))
		return; // obligatory paranoia check

	if (P_IsKartItem(mobj->type))	// mobj is a kart item we want on the list:
		P_AddKartItem(mobj);		// add to kitem list

	// Can end up here if a player dies.
	if (mobj->player)
		P_CyclePlayerMobjState(mobj);
	else
		P_CycleMobjState(mobj);

	if (P_MobjWasRemoved(mobj))
		return;

	switch (mobj->type)
	{
		case MT_BOUNCEPICKUP:
		case MT_RAILPICKUP:
		case MT_AUTOPICKUP:
		case MT_EXPLODEPICKUP:
		case MT_SCATTERPICKUP:
		case MT_GRENADEPICKUP:
			if (mobj->health == 0) // Fading tile
			{
				INT32 value = mobj->info->damage/10;
				value = mobj->fuse/value;
				value = 10-value;
				value--;

				if (value <= 0)
					value = 1;

				mobj->frame &= ~FF_TRANSMASK;
				mobj->frame |= value << FF_TRANSSHIFT;
			}
			break;
		default:
			break;
	}
}

// Quick, optimized function for the Rail Rings
// Returns true if move failed or mobj was removed by movement (death pit, missile hits wall, etc.)
boolean P_RailThinker(mobj_t *mobj)
{
	fixed_t x, y, z;

	I_Assert(mobj != NULL);
	I_Assert(!P_MobjWasRemoved(mobj));

	x = mobj->x, y = mobj->y, z = mobj->z;

	if (mobj->momx || mobj->momy)
	{
		P_XYMovement(mobj);
		if (P_MobjWasRemoved(mobj))
			return true;
	}

	if (mobj->momz)
	{
		if (!P_ZMovement(mobj))
			return true; // mobj was removed
		//P_CheckPosition(mobj, mobj->x, mobj->y);
	}

	return P_MobjWasRemoved(mobj) || (x == mobj->x && y == mobj->y && z == mobj->z);
}

// Unquick, unoptimized function for pushables
void P_PushableThinker(mobj_t *mobj)
{
	I_Assert(mobj != NULL);
	I_Assert(!P_MobjWasRemoved(mobj));

	P_CheckMobjTrigger(mobj, true);

	// it has to be pushable RIGHT NOW for this part to happen
	if (mobj->flags & MF_PUSHABLE && !(mobj->momx || mobj->momy))
		P_TryMove(mobj, mobj->x, mobj->y, true);

	if (mobj->fuse == 1) // it would explode in the MobjThinker code
	{
		mobj_t *spawnmo;
		fixed_t x, y, z;
		subsector_t *ss;

		// Left here just in case we'd
		// want to make pushable bombs
		// or something in the future.
		switch (mobj->type)
		{
			case MT_SNOWMAN:
			case MT_GARGOYLE:
				x = mobj->spawnpoint->x << FRACBITS;
				y = mobj->spawnpoint->y << FRACBITS;

				ss = R_PointInSubsector(x, y);

				if (mobj->spawnpoint->z != 0)
					z = mobj->spawnpoint->z << FRACBITS;
				else
					z = ss->sector->floorheight;

				spawnmo = P_SpawnMobj(x, y, z, mobj->type);
				spawnmo->spawnpoint = mobj->spawnpoint;
				P_UnsetThingPosition(spawnmo);
				spawnmo->flags = mobj->flags;
				P_SetThingPosition(spawnmo);
				spawnmo->flags2 = mobj->flags2;
				spawnmo->flags |= MF_PUSHABLE;
				P_RemoveMobj(mobj);
				break;
			default:
				break;
		}
	}
}

// Quick, optimized function for scenery
void P_SceneryThinker(mobj_t *mobj)
{
	if (mobj->flags & MF_BOXICON)
	{
		if (!(mobj->eflags & MFE_VERTICALFLIP))
		{
			if (mobj->z < mobj->floorz + FixedMul(mobj->info->damage, mobj->scale))
				mobj->momz = FixedMul(mobj->info->speed, mobj->scale);
			else
				mobj->momz = 0;
		}
		else
		{
			if (mobj->z + FixedMul(mobj->info->height, mobj->scale) > mobj->ceilingz - FixedMul(mobj->info->damage, mobj->scale))
				mobj->momz = -FixedMul(mobj->info->speed, mobj->scale);
			else
				mobj->momz = 0;
		}
	}

	// momentum movement
	if (mobj->momx || mobj->momy)
	{
		P_SceneryXYMovement(mobj);

		if (P_MobjWasRemoved(mobj))
			return;
	}

	// always do the gravity bit now, that's simpler
	// BUT CheckPosition only if wasn't done before.
	if (!(mobj->eflags & MFE_ONGROUND) || mobj->momz
		|| ((mobj->eflags & MFE_VERTICALFLIP) && mobj->z + mobj->height != mobj->ceilingz)
		|| (!(mobj->eflags & MFE_VERTICALFLIP) && mobj->z != mobj->floorz)
		|| P_IsObjectInGoop(mobj))
	{
		if (!P_SceneryZMovement(mobj))
			return; // mobj was removed
		P_CheckPosition(mobj, mobj->x, mobj->y); // Need this to pick up objects!
		if (P_MobjWasRemoved(mobj))
			return;
		mobj->floorz = tmfloorz;
		mobj->ceilingz = tmceilingz;
		mobj->floorrover = tmfloorrover;
		mobj->ceilingrover = tmceilingrover;
	}
	else
	{
		mobj->pmomz = 0; // to prevent that weird rocketing gargoyle bug
		mobj->eflags &= ~MFE_JUSTHITFLOOR;
	}

	P_CycleMobjState(mobj);

	if (mobj->type != MT_RANDOMAUDIENCE)
		return;

	{
		if (!mobj->colorized) // a fan of someone?
			return;

		if (mobj->threshold >= 0) // not already happy or sad?
		{
			if (!playeringame[mobj->threshold] || players[mobj->threshold].spectator) // focused on a valid player?
				return;

			if (!(players[mobj->threshold].exiting) && !(players[mobj->threshold].pflags & PF_NOCONTEST)) // not finished yet?
				return;

			if (K_IsPlayerLosing(&players[mobj->threshold]))
				mobj->threshold = -2;
			else
			{
				mobj->threshold = -1;
				S_StartSound(mobj, sfx_chaooo);
			}
		}

		if (mobj->threshold == -1)
			mobj->angle += ANGLE_22h;

		if (((statenum_t)(mobj->state-states) != S_AUDIENCE_CHAO_CHEER2) || (mobj->tics != states[S_AUDIENCE_CHAO_CHEER2].tics)) // not at the start of your cheer jump?
			return;

		mobj->momz = 0;

		P_SetMobjState(mobj, ((mobj->threshold == -1) ? S_AUDIENCE_CHAO_WIN2 : S_AUDIENCE_CHAO_LOSE));
	}
}

//
// GAME SPAWN FUNCTIONS
//

static void P_DefaultMobjShadowScale(mobj_t *thing)
{
	thing->shadowscale = 0;
	thing->whiteshadow = ((thing->frame & FF_BRIGHTMASK) == FF_FULLBRIGHT);

	switch (thing->type)
	{
		case MT_PLAYER:
			thing->shadowscale = FRACUNIT;
			break;
		case MT_SMALLMACE:
		case MT_BIGMACE:
		case MT_PUMA:
		case MT_BIGPUMA:
		case MT_FALLINGROCK:
		case MT_SMK_MOLE:
		case MT_SMK_THWOMP:
		case MT_BATTLEBUMPER:
		case MT_BANANA:
		case MT_ORBINAUT:
		case MT_ORBINAUT_SHIELD:
		case MT_JAWZ:
		case MT_JAWZ_DUD:
		case MT_JAWZ_SHIELD:
		case MT_SSMINE:
		case MT_SSMINE_SHIELD:
		case MT_LANDMINE:
		case MT_BALLHOG:
		case MT_SINK:
		case MT_ROCKETSNEAKER:
		case MT_SPB:
			thing->shadowscale = 3*FRACUNIT/2;
			break;
		case MT_BANANA_SHIELD:
			thing->shadowscale = 12*FRACUNIT/5;
			break;
		case MT_RANDOMITEM:
		case MT_SPHEREBOX:
			thing->shadowscale = FRACUNIT/2;
			thing->whiteshadow = false;
			break;
		case MT_EGGMANITEM:
			thing->shadowscale = FRACUNIT;
			thing->whiteshadow = false;
			break;
		case MT_EGGMANITEM_SHIELD:
			thing->shadowscale = 3*FRACUNIT/2;
			thing->whiteshadow = false;
			break;
		case MT_DROPTARGET:
		case MT_DROPTARGET_SHIELD:
			thing->shadowscale = 5*FRACUNIT/4;
			thing->whiteshadow = true;
			break;
		case MT_LIGHTNINGSHIELD:
		case MT_BUBBLESHIELD:
		case MT_BUBBLESHIELDTRAP:
		case MT_FLAMESHIELD:
			thing->shadowscale = FRACUNIT;
			break;
		case MT_RING:
		case MT_FLINGRING:
		case MT_DEBTSPIKE:
		case MT_FLOATINGITEM:
		case MT_BLUESPHERE:
		case MT_EMERALD:
		case MT_ITEMCAPSULE:
		case MT_POGOSPRING:
			thing->shadowscale = FRACUNIT/2;
			break;
		default:
			if (thing->flags & (MF_ENEMY|MF_BOSS))
				thing->shadowscale = FRACUNIT;
			break;
	}
}

//
// P_SpawnMobj
//
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	const mobjinfo_t *info = &mobjinfo[type];
	SINT8 sc = -1;
	state_t *st;
	mobj_t *mobj;

	if (type == MT_NULL)
	{
#if 0		
#ifdef PARANOIA
		I_Error("Tried to spawn MT_NULL\n");
#endif
		return NULL;
#endif
		// Hack: Some code assumes that P_SpawnMobj can never return NULL
		// So replace MT_NULL with MT_RAY in the meantime
		// Remove when dealt properly
		CONS_Debug(DBG_GAMELOGIC, "Tried to spawn MT_NULL, using MT_RAY\n");
		type = MT_RAY;
	}

	if (mobjcache != NULL)
	{
		mobj = mobjcache;
		mobjcache = mobjcache->hnext;
		memset(mobj, 0, sizeof(*mobj));
	}
	else
	{
		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);
	}

	// this is officially a mobj, declared as soon as possible.
	mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
	mobj->type = type;
	mobj->info = info;

	mobj->x = x;
	mobj->y = y;

	mobj->radius = info->radius;
	mobj->height = info->height;
	mobj->flags = info->flags;

	mobj->health = (info->spawnhealth ? info->spawnhealth : 1);

	mobj->reactiontime = info->reactiontime;

	mobj->lastlook = -1; // stuff moved in P_enemy.P_LookForPlayer

	// do not set the state with P_SetMobjState,
	// because action routines can not be called yet
	st = &states[info->spawnstate];

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame; // FF_FRAMEMASK for frame, and other bits..
	P_SetupStateAnimation(mobj, st);

	mobj->friction = ORIG_FRICTION;

	mobj->movefactor = FRACUNIT;

	// All mobjs are created at 100% scale.
	mobj->scale = FRACUNIT;
	mobj->destscale = mobj->scale;
	mobj->scalespeed = FRACUNIT/12;

	if (mapobjectscale != FRACUNIT) //&& !(mobj->type == MT_BLACKEGGMAN)
	{
		mobj->destscale = mapobjectscale;
		mobj->scalespeed = mapobjectscale/12;
	}

	// Sprite rendering
	mobj->spritexscale = mobj->spriteyscale = mobj->scale;
	mobj->spritexoffset = mobj->spriteyoffset = 0;
	mobj->dispoffset = info->dispoffset;
	mobj->floorspriteslope = NULL;

	// set subsector and/or block links
	P_SetThingPosition(mobj);
	I_Assert(mobj->subsector != NULL);

	// Make sure scale matches destscale immediately when spawned
	P_SetScale(mobj, mobj->destscale);

	mobj->floorz   = P_GetSectorFloorZAt  (mobj->subsector->sector, x, y);
	mobj->ceilingz = P_GetSectorCeilingZAt(mobj->subsector->sector, x, y);

	mobj->floorrover = NULL;
	mobj->ceilingrover = NULL;

	// Tells MobjCheckWater that the water height was not set.
	mobj->watertop = INT32_MAX;

	if (z == ONFLOORZ)
	{
		mobj->z = mobj->floorz;

		if (mobj->type == MT_UNIDUS)
			mobj->z += FixedMul(mobj->info->mass, mobj->scale);

		// defaults onground
		if (mobj->z == mobj->floorz)
			mobj->eflags |= MFE_ONGROUND;
	}
	else if (z == ONCEILINGZ)
	{
		mobj->z = mobj->ceilingz - mobj->height;

		if (mobj->type == MT_UNIDUS)
			mobj->z -= FixedMul(mobj->info->mass, mobj->scale);

		// defaults onground
		if (mobj->z + mobj->height == mobj->ceilingz)
			mobj->eflags |= MFE_ONGROUND;
	}
	else
		mobj->z = z;

	mobj->colorized = false;

	// Set shadowscale here, before spawn hook so that Lua can change it
	P_DefaultMobjShadowScale(mobj);

	// DANGER! This can cause P_SpawnMobj to return NULL!
	// Avoid using P_RemoveMobj on the newly created mobj in "MobjSpawn" Lua hooks!
	if (LUA_HookMobj(mobj, MOBJ_HOOK(MobjSpawn)))
	{
		if (P_MobjWasRemoved(mobj))
			return NULL;
	}
	else if (P_MobjWasRemoved(mobj))
		return NULL;
	else
	switch (mobj->type)
	{
		case MT_ALTVIEWMAN:
			if (titlemapinaction) mobj->flags &= ~MF_NOTHINK;
			break;
		case MT_LOCKONINF:
			P_SetScale(mobj, (mobj->destscale = 3*mobj->scale));
			break;
		case MT_FAKEMOBILE:
		case MT_EGGSHIELD:
			mobj->flags2 |= MF2_INVERTAIMABLE;
			break;
		case MT_DETON:
			mobj->movedir = 0;
			break;
		case MT_EGGGUARD:
			{
				mobj_t *spawn = P_SpawnMobj(x, y, z, MT_EGGSHIELD);
				spawn->destscale = mobj->scale;
				P_SetScale(spawn, mobj->scale);
				P_SetTarget(&mobj->tracer, spawn);
				P_SetTarget(&spawn->target, mobj);
			}
			break;
		case MT_UNIDUS:
			{
				INT32 i;
				mobj_t *ball;
				// Spawn "damage" number of "painchance" spikeball mobjs
				// threshold is the distance they should keep from the MT_UNIDUS (touching radius + ball painchance)
				for (i = 0; i < mobj->info->damage; i++)
				{
					ball = P_SpawnMobj(x, y, z, mobj->info->painchance);
					ball->destscale = mobj->scale;
					P_SetScale(ball, mobj->scale);
					P_SetTarget(&ball->target, mobj);
					ball->movedir = FixedAngle(FixedMul(FixedDiv(i<<FRACBITS, mobj->info->damage<<FRACBITS), 360<<FRACBITS));
					ball->threshold = ball->radius + mobj->radius + FixedMul(ball->info->painchance, ball->scale);

					var1 = ball->state->var1, var2 = ball->state->var2;
					ball->state->action.acp1(ball);
				}
			}
			break;
		case MT_POINTY:
			{
				INT32 q;
				mobj_t *ball, *lastball = mobj;

				for (q = 0; q < mobj->info->painchance; q++)
				{
					ball = P_SpawnMobj(x, y, z, mobj->info->mass);
					ball->destscale = mobj->scale;
					P_SetScale(ball, mobj->scale);
					P_SetTarget(&lastball->tracer, ball);
					P_SetTarget(&ball->target, mobj);
					lastball = ball;
				}
			}
			break;
		case MT_CRUSHSTACEAN:
			{
				mobj_t *bigmeatyclaw = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_CRUSHCLAW);
				P_InitAngle(bigmeatyclaw, mobj->angle + ((mobj->flags2 & MF2_AMBUSH) ? ANGLE_90 : ANGLE_270));
				P_SetTarget(&mobj->tracer, bigmeatyclaw);
				P_SetTarget(&bigmeatyclaw->tracer, mobj);
				mobj->reactiontime >>= 1;
			}
			break;
		case MT_BANPYURA:
			{
				mobj_t *bigmeatyclaw = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_BANPSPRING);
				P_InitAngle(bigmeatyclaw, mobj->angle + ((mobj->flags2 & MF2_AMBUSH) ? ANGLE_90 : ANGLE_270));
				P_SetTarget(&mobj->tracer, bigmeatyclaw);
				P_SetTarget(&bigmeatyclaw->tracer, mobj);
				mobj->reactiontime >>= 1;
			}
			break;
		case MT_BIGMINE:
			mobj->extravalue1 = FixedHypot(mobj->x, mobj->y)>>FRACBITS;
			break;
		case MT_WAVINGFLAG1:
		case MT_WAVINGFLAG2:
			{
				mobj_t *prev = mobj, *cur;
				UINT8 i;
				for (i = 0; i <= 16; i++) // probably should be < but staying authentic to the Lua version
				{
					cur = P_SpawnMobjFromMobj(mobj, 0, 0, 0, ((mobj->type == MT_WAVINGFLAG1) ? MT_WAVINGFLAGSEG1 : MT_WAVINGFLAGSEG2));;
					P_SetTarget(&prev->tracer, cur);
					cur->extravalue1 = i;
					prev = cur;
				}
			}
			break;
		case MT_EGGMOBILE2:
			// Special condition for the 2nd boss.
			mobj->watertop = mobj->info->speed;
			break;
		case MT_EGGMOBILE3:
			mobj->movefactor = -512*FRACUNIT;
			mobj->flags2 |= MF2_CLASSICPUSH;
			break;
		case MT_EGGMOBILE4:
			mobj->flags2 |= MF2_INVERTAIMABLE;
			break;
		case MT_FLICKY_08:
			mobj->color = (P_RandomChance(FRACUNIT/2) ? SKINCOLOR_RED : SKINCOLOR_AQUAMARINE);
			break;
		case MT_BALLOON:
			{
				static const UINT8 BALLOONCOLORS[] = {
					// Carnival Night balloon colors
					SKINCOLOR_KETCHUP,
					SKINCOLOR_SAPPHIRE,
					SKINCOLOR_TANGERINE,
					SKINCOLOR_JET
				};

				mobj->color = BALLOONCOLORS[P_RandomKey(sizeof(BALLOONCOLORS))];
			}
			break;
		case MT_POGOSPRING:
			P_SetScale(mobj, (mobj->destscale = 3 * mobj->destscale / 2));
			break;
		case MT_KART_LEFTOVER:
			mobj->color = SKINCOLOR_RED;
			break;
		case MT_EGGROBO1:
			mobj->movecount = P_RandomKey(13);
			mobj->color = FIRSTRAINBOWCOLOR + P_RandomKey(FIRSTSUPERCOLOR - FIRSTRAINBOWCOLOR);
			break;
		case MT_HIVEELEMENTAL:
			mobj->extravalue1 = 5;
			break;
		case MT_SMASHINGSPIKEBALL:
			mobj->movecount = mobj->z;
			break;
		case MT_SPINBOBERT:
			{
				mobj_t *fire;
				fire = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_SPINBOBERT_FIRE1);
				P_SetTarget(&fire->target, mobj);
				P_SetTarget(&mobj->hnext, fire);
				fire = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_SPINBOBERT_FIRE2);
				P_SetTarget(&fire->target, mobj);
				P_SetTarget(&mobj->hprev, fire);
			}
			break;
		case MT_REDRING: // Make MT_REDRING red by default
			mobj->color = skincolor_redring;
			break;
		case MT_SMALLBUBBLE: // Bubbles eventually dissipate, in case they get caught somewhere.
		case MT_MEDIUMBUBBLE:
		case MT_EXTRALARGEBUBBLE:
			mobj->fuse += 30 * TICRATE;
			break;
		case MT_NIGHTSDRONE:
			nummaprings = -1; // no perfect bonus, rings are free
			break;
		case MT_EGGCAPSULE:
			mobj->reactiontime = 0;
			mobj->extravalue1 = mobj->cvmem =\
			 mobj->cusval = mobj->movecount =\
			 mobj->lastlook = mobj->extravalue2 = -1;
			break;
		case MT_REDTEAMRING:
			mobj->color = skincolor_redteam;
			break;
		case MT_BLUETEAMRING:
			mobj->color = skincolor_blueteam;
			break;
		case MT_RING:
			if (nummaprings >= 0)
				nummaprings++;
			break;
		case MT_METALSONIC_RACE:
			mobj->skin = &skins[5];
			/* FALLTHRU */
		case MT_METALSONIC_BATTLE:
			mobj->color = skins[5].prefcolor;
			sc = 5;
			break;
		case MT_FANG:
			sc = 4;
			break;
		case MT_ROSY:
			sc = 3;
			break;
		case MT_CORK:
			mobj->flags2 |= MF2_SUPERFIRE;
			break;
		case MT_FBOMB:
			mobj->flags2 |= MF2_EXPLOSION;
			break;
		case MT_OILLAMP:
			{
				mobj_t* overlay = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_OVERLAY);
				P_SetTarget(&overlay->target, mobj);
				P_SetMobjState(overlay, S_OILLAMPFLARE);
				break;
			}
		case MT_TNTBARREL:
			mobj->momx = 1; //stack hack
			mobj->flags2 |= MF2_INVERTAIMABLE;
			break;
		case MT_MINECARTEND:
			P_SetTarget(&mobj->tracer, P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_MINECARTENDSOLID));
			P_InitAngle(mobj->tracer, mobj->angle + ANGLE_90);
			break;
		case MT_TORCHFLOWER:
			{
				mobj_t *fire = P_SpawnMobjFromMobj(mobj, 0, 0, 46*FRACUNIT, MT_FLAME);
				P_SetTarget(&mobj->target, fire);
				break;
			}
		case MT_PYREFLY:
			mobj->extravalue1 = (FixedHypot(mobj->x, mobj->y)/FRACUNIT) % 360;
			mobj->extravalue2 = 0;
			mobj->fuse = 100;
			break;
		// SRB2Kart
		case MT_ITEMCAPSULE:
		{
			// set default item & count
#if 0 // set to 1 to test capsules with random items, e.g. with objectplace
			if (P_RandomChance(FRACUNIT/3))
				mobj->threshold = KITEM_SUPERRING;
			else if (P_RandomChance(FRACUNIT/3))
				mobj->threshold = KITEM_SPB;
			else if (P_RandomChance(FRACUNIT/3))
				mobj->threshold = KITEM_ORBINAUT;
			else
				mobj->threshold = P_RandomRange(1, NUMKARTITEMS - 1);
			mobj->movecount = P_RandomChance(FRACUNIT/3) ? 1 : P_RandomKey(32) + 1;
#else
			mobj->threshold = KITEM_SUPERRING; // default item is super ring
			mobj->movecount = 1;
#endif

			// set starting scale
			mobj->extravalue1 = mobj->scale; // this acts as the capsule's destscale; we're avoiding P_MobjScaleThink because we want aerial capsules not to scale from their center
			mobj->scalespeed >>= 1;
			P_SetScale(mobj, mobj->destscale = mapobjectscale >> 4);

			break;
		}
		case MT_KARMAHITBOX:
			{
				const fixed_t rad = FixedMul(mobjinfo[MT_PLAYER].radius, mobj->scale);
				mobj_t *cur, *prev = mobj;
				INT32 i;

				for (i = 0; i < 4; i++)
				{
					fixed_t offx = rad;
					fixed_t offy = rad;

					if (i == 1 || i == 3)
						offx *= -1;
					if (i == 2 || i == 3)
						offy *= -1;

					cur = P_SpawnMobj(mobj->x + offx, mobj->y + offy, mobj->z, MT_KARMAWHEEL);
					cur->destscale = mobj->scale;
					P_SetScale(cur, mobj->scale);

					cur->lastlook = i;

					P_SetTarget(&cur->hprev, prev);
					P_SetTarget(&prev->hnext, cur);

					prev = cur;
				}
			}
			break;
		case MT_BIGRING:
			P_SetScale(mobj, (mobj->destscale = 3*FRACUNIT));
			break;
		case MT_RANDOMAUDIENCE:
		{
			fixed_t randu = P_RandomFixed();
			P_SetScale(mobj, (mobj->destscale <<= 1));
			if (randu < (FRACUNIT/9)) // a fan of someone?
			{
				UINT8 i, pcount = 0;
				UINT8 pnum[MAXPLAYERS];

				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (!playeringame[i])
						continue;
					pnum[pcount] = i;
					pcount++;
				}

				if (pcount)
				{
					mobj->threshold = pnum[P_RandomKey(pcount)];
					mobj->color = players[mobj->threshold].skincolor;
					mobj->colorized = true;
					break;
				}
			}

			if (randu > (FRACUNIT/2))
			{
				mobj->color = P_RandomKey(numskincolors-1)+1;
				break;
			}

			mobj->color = SKINCOLOR_CYAN;
			break;
		}
		case MT_MARBLETORCH:
			P_SpawnMobj(mobj->x, mobj->y, mobj->z + (29*mobj->scale), MT_MARBLELIGHT);
			break;
		case MT_RUSTYLAMP_ORANGE:
			P_SpawnMobj(mobj->x, mobj->y, mobj->z + (69*mobj->scale), MT_MARBLELIGHT);
			break;
		case MT_DAYTONAPINETREE:
		{
			angle_t diff = FixedAngle((360/mobj->info->mass)*FRACUNIT);
			UINT8 i;

			for (i = 0; i < mobj->info->mass; i++)
			{
				angle_t ang = i * diff;
				mobj_t *side = P_SpawnMobj(mobj->x + FINECOSINE((ang>>ANGLETOFINESHIFT) & FINEMASK),
					mobj->y + FINESINE((ang>>ANGLETOFINESHIFT) & FINEMASK), mobj->z, MT_DAYTONAPINETREE_SIDE);
				P_InitAngle(side, ang);
				P_SetTarget(&side->target, mobj);
				side->threshold = i;
			}
			break;
		}
		case MT_EZZPROPELLER:
		{
			mobj_t *cur, *prev = mobj;
			UINT8 i;

			for (i = 0; i < mobj->info->mass; i++)
			{
				mobj->angle = FixedAngle((i * (360/mobj->info->mass))<<FRACBITS);

				cur = P_SpawnMobj(mobj->x + FINECOSINE(((mobj->angle*8)>>ANGLETOFINESHIFT) & FINEMASK),
					mobj->y + FINESINE(((mobj->angle*8)>>ANGLETOFINESHIFT) & FINEMASK), mobj->z, MT_EZZPROPELLER_BLADE);
				P_InitAngle(cur, mobj->angle);

				P_SetTarget(&cur->hprev, prev);
				P_SetTarget(&prev->hnext, cur);

				prev = cur;
			}
			break;
		}
		case MT_ROBRA:
		case MT_BLUEROBRA:
			P_SetScale(mobj, (mobj->destscale = 1));
			break;
		case MT_ROBRA_HEAD:
			{
				mobj_t *shell;

				shell = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_OVERLAY);
				P_SetTarget(&shell->target, mobj);
				P_SetMobjState(shell, S_ROBRASHELL_INSIDE);

				shell = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_OVERLAY);
				P_SetTarget(&shell->target, mobj);
				P_SetMobjState(shell, S_ROBRASHELL_OUTSIDE);
			}
			break;
		case MT_EERIEFOGGEN:
			{
				UINT16 i;
				for (i = 0; i < mobj->info->mass; i++)
				{
					fixed_t newx = mobj->x + (P_RandomRange(-mobj->info->mass, mobj->info->mass)<<FRACBITS);
					fixed_t newy = mobj->y + (P_RandomRange(-mobj->info->mass, mobj->info->mass)<<FRACBITS);

					if (P_FloorzAtPos(newx, newy, mobj->z, 8<<FRACBITS) == mobj->z)
						P_SpawnMobj(newx, newy, mobj->z, MT_EERIEFOG);
				}
			}
			break;
		case MT_DROPTARGET:
		case MT_DROPTARGET_SHIELD:
			mobj->color = SKINCOLOR_LIME;
			mobj->colorized = true;
			mobj->renderflags |= RF_FULLBRIGHT;
			break;
		case MT_SMK_MOLE:
			mobj->reactiontime = P_RandomRange(0, 3*mobj->info->reactiontime/2); // Random delay on start of level
			break;
		case MT_SMK_THWOMP:
			mobj->reactiontime = P_RandomRange(0, 3*mobj->info->reactiontime); // Random delay on start of level
			if (mobj->z == mobj->floorz)
				mobj->z += (256<<FRACBITS);
			mobj->movefactor = mobj->z + (256<<FRACBITS);
			break;
		case MT_SMK_ICEBLOCK:
			{
				mobj_t *cur, *prev = mobj;
				UINT8 i;

				for (i = 0; i < 4; i++)
				{
					cur = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_SMK_ICEBLOCK_SIDE);
					P_SetTarget(&cur->target, mobj);
					cur->threshold = i;
					P_MoveOrigin(cur, cur->x + ((cur->radius>>FRACBITS) * FINECOSINE((FixedAngle((90*cur->threshold)<<FRACBITS)>>ANGLETOFINESHIFT) & FINEMASK)),
						cur->y + ((cur->radius>>FRACBITS) * FINESINE((FixedAngle((90*cur->threshold)<<FRACBITS)>>ANGLETOFINESHIFT) & FINEMASK)), cur->z);
					P_InitAngle(cur, ANGLE_90*(cur->threshold+1));

					P_SetTarget(&cur->hprev, prev);
					P_SetTarget(&prev->hnext, cur);

					prev = cur;
				}
			}
			break;
		case MT_TUMBLEGEM:
			mobj->color = P_RandomKey(numskincolors - 1) + 1;
		default:
			break;
	}

	if (sc != -1 && !(mobj->flags2 & MF2_SLIDEPUSH))
	{
		UINT8 i;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;

			if (players[i].skin == sc)
			{
				mobj->color = SKINCOLOR_SILVER;
				mobj->colorized = true;
				mobj->flags2 |= MF2_SLIDEPUSH;
				break;
			}
		}
	}

	if (!(mobj->flags & MF_NOTHINK))
		P_AddThinker(THINK_MOBJ, &mobj->thinker);

	if (mobj->skin) // correct inadequecies above.
	{
		mobj->sprite2 = P_GetSkinSprite2(mobj->skin, (mobj->frame & FF_FRAMEMASK), NULL);
		mobj->frame &= ~FF_FRAMEMASK;
	}

	// Call action functions when the state is set
	if (st->action.acp1 && (mobj->flags & MF_RUNSPAWNFUNC))
	{
		if (levelloading)
		{
			// Cache actions in a linked list
			// with function pointer, and
			// var1 & var2, which will be executed
			// when the level finishes loading.
			P_AddCachedAction(mobj, mobj->info->spawnstate);
		}
		else
		{
			var1 = st->var1;
			var2 = st->var2;
			astate = st;
			st->action.acp1(mobj);
			// DANGER! This can cause P_SpawnMobj to return NULL!
			// Avoid using MF_RUNSPAWNFUNC on mobjs whose spawn state expects target or tracer to already be set!
			if (P_MobjWasRemoved(mobj))
				return NULL;
		}
	}

	if (CheckForReverseGravity && !(mobj->flags & MF_NOBLOCKMAP))
		P_CheckGravity(mobj, false);

	R_AddMobjInterpolator(mobj);

	return mobj;
}

static precipmobj_t *P_SpawnPrecipMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	const mobjinfo_t *info = &mobjinfo[type];
	state_t *st;
	fixed_t start_z = INT32_MIN;
	precipmobj_t *mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

	mobj->type = type;
	mobj->info = info;

	mobj->x = x;
	mobj->y = y;
	mobj->flags = info->flags;

	// do not set the state with P_SetMobjState,
	// because action routines can not be called yet
	st = &states[info->spawnstate];

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame; // FF_FRAMEMASK for frame, and other bits..
	P_SetupStateAnimation((mobj_t*)mobj, st);

	// set subsector and/or block links
	P_SetPrecipitationThingPosition(mobj);

	mobj->floorz   = P_GetSectorFloorZAt  (mobj->subsector->sector, x, y);
	mobj->ceilingz = P_GetSectorCeilingZAt(mobj->subsector->sector, x, y);

	mobj->floorrover = NULL;
	mobj->ceilingrover = NULL;

	mobj->z = z;
	mobj->momz = FixedMul(-info->speed, mapobjectscale);

	if (info->speed < 0)
	{
		mobj->precipflags |= PCF_FLIP;
	}

	start_z = mobj->floorz;

	mobj->thinker.function.acp1 = (actionf_p1)P_NullPrecipThinker;
	P_AddThinker(THINK_PRECIP, &mobj->thinker);

	P_CalculatePrecipFloor(mobj);

	if (mobj->floorz != start_z)
	{
		; //mobj->precipflags |= PCF_FOF;
	}
	else
	{
		INT32 dmg = mobj->subsector->sector->damagetype;
		boolean sFlag = (mobj->precipflags & PCF_FLIP) ? (mobj->subsector->sector->flags & MSF_FLIPSPECIAL_CEILING) : (mobj->subsector->sector->flags & MSF_FLIPSPECIAL_FLOOR);
		boolean pitFloor = ((dmg == SD_DEATHPIT) && sFlag);
		boolean skyFloor = (mobj->precipflags & PCF_FLIP) ? (mobj->subsector->sector->ceilingpic == skyflatnum) : (mobj->subsector->sector->floorpic == skyflatnum);

		if (pitFloor || skyFloor)
		{
			mobj->precipflags |= PCF_PIT;
		}
	}

	R_ResetPrecipitationMobjInterpolationState(mobj);

	return mobj;
}

void *P_CreateFloorSpriteSlope(mobj_t *mobj)
{
	if (mobj->floorspriteslope)
		Z_Free(mobj->floorspriteslope);
	mobj->floorspriteslope = Z_Calloc(sizeof(pslope_t), PU_LEVEL, NULL);
	mobj->floorspriteslope->normal.z = FRACUNIT;
	return (void *)mobj->floorspriteslope;
}

void P_RemoveFloorSpriteSlope(mobj_t *mobj)
{
	if (mobj->floorspriteslope)
		Z_Free(mobj->floorspriteslope);
	mobj->floorspriteslope = NULL;
}

//
// P_RemoveMobj
//
mapthing_t *itemrespawnque[ITEMQUESIZE];
tic_t itemrespawntime[ITEMQUESIZE];
size_t iquehead, iquetail;

#ifdef PARANOIA
#define SCRAMBLE_REMOVED // Force debug build to crash when Removed mobj is accessed
#endif
void P_RemoveMobj(mobj_t *mobj)
{
	I_Assert(mobj != NULL);
	if (P_MobjWasRemoved(mobj))
		return; // something already removing this mobj.

	mobj->thinker.function.acp1 = (actionf_p1)P_RemoveThinkerDelayed; // shh. no recursing.
	LUA_HookMobj(mobj, MOBJ_HOOK(MobjRemoved));
	mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker; // needed for P_UnsetThingPosition, etc. to work.

	// Rings only, please!
	if (mobj->spawnpoint &&
		(mobj->type == MT_RING
		|| mobj->type == MT_BLUESPHERE)
		&& !(mobj->flags2 & MF2_DONTRESPAWN))
	{
		//CONS_Printf("added to queue at tic %d\n", leveltime);
		itemrespawnque[iquehead] = mobj->spawnpoint;
		itemrespawntime[iquehead] = leveltime;
		iquehead = (iquehead+1)&(ITEMQUESIZE-1);
		// lose one off the end?
		if (iquehead == iquetail)
			iquetail = (iquetail+1)&(ITEMQUESIZE-1);
	}

	if (mobj->type == MT_KARMAHITBOX) // Remove linked list objects for certain types
	{
		mobj_t *cur = mobj->hnext;

		while (cur && !P_MobjWasRemoved(cur))
		{
			mobj_t *prev = cur; // Kind of a dumb var, but we need to set cur before we remove the mobj
			cur = cur->hnext;
			P_RemoveMobj(prev);
		}
	}

	if (mobj->type == MT_OVERLAY)
		P_RemoveOverlay(mobj);

	if (mobj->type == MT_SPB)
		spbplace = -1;

	if (P_IsKartItem(mobj->type))
		P_RemoveKartItem(mobj);

	if (mobj->player && mobj->player->followmobj)
	{
		P_RemoveMobj(mobj->player->followmobj);
		P_SetTarget(&mobj->player->followmobj, NULL);
	}

	mobj->health = 0; // Just because

	// unlink from sector and block lists
	P_UnsetThingPosition(mobj);
	if (sector_list)
	{
		P_DelSeclist(sector_list);
		sector_list = NULL;
	}

	mobj->flags |= MF_NOSECTOR|MF_NOBLOCKMAP;
	mobj->subsector = NULL;
	mobj->state = NULL;
	mobj->player = NULL;

	P_RemoveFloorSpriteSlope(mobj);

	// stop any playing sound
	S_StopSound(mobj);

	// killough 11/98:
	//
	// Remove any references to other mobjs.
	P_SetTarget(&mobj->target, NULL);
	P_SetTarget(&mobj->tracer, NULL);

	// repair hnext chain
	{
		mobj_t *cachenext = mobj->hnext;

		if (mobj->hnext && !P_MobjWasRemoved(mobj->hnext))
		{
			P_SetTarget(&mobj->hnext->hprev, mobj->hprev);
			P_SetTarget(&mobj->hnext, NULL);
		}

		if (mobj->hprev && !P_MobjWasRemoved(mobj->hprev))
		{
			P_SetTarget(&mobj->hprev->hnext, cachenext);
			P_SetTarget(&mobj->hprev, NULL);
		}
	}

	// DBG: set everything in mobj_t to 0xFF instead of leaving it. debug memory error.
#ifdef SCRAMBLE_REMOVED
	// Invalidate mobj_t data to cause crashes if accessed!
	memset((UINT8 *)mobj + sizeof(thinker_t), 0xff, sizeof(mobj_t) - sizeof(thinker_t));
#endif

	P_RemoveThingTID(mobj);
	R_RemoveMobjInterpolator(mobj);

	// free block
	if (!mobj->thinker.next)
	{ // Uh-oh, the mobj doesn't think, P_RemoveThinker would never go through!
		INT32 prevreferences;
		if (!mobj->thinker.references)
		{
			// no references, dump it directly in the mobj cache
			mobj->hnext = mobjcache;
			mobjcache = mobj;
			return;
		}

		prevreferences = mobj->thinker.references;
		P_AddThinker(THINK_MOBJ, (thinker_t *)mobj);
		mobj->thinker.references = prevreferences;
	}

	P_RemoveThinker((thinker_t *)mobj);
}

// This does not need to be added to Lua.
// To test it in Lua, check mobj.valid
boolean P_MobjWasRemoved(mobj_t *mobj)
{
	if (mobj && mobj->thinker.function.acp1 == (actionf_p1)P_MobjThinker)
		return false;
	return true;
}

void P_FreePrecipMobj(precipmobj_t *mobj)
{
	// unlink from sector and block lists
	P_UnsetPrecipThingPosition(mobj);

	if (precipsector_list)
	{
		P_DelPrecipSeclist(precipsector_list);
		precipsector_list = NULL;
	}

	// free block
	// Precipmobjs don't actually think using their thinker,
	// so the free cannot be delayed.
	P_UnlinkThinker((thinker_t*)mobj);

}

// Clearing out stuff for savegames
void P_RemoveSavegameMobj(mobj_t *mobj)
{
	// unlink from sector and block lists
	if (((thinker_t *)mobj)->function.acp1 == (actionf_p1)P_NullPrecipThinker)
	{
		P_UnsetPrecipThingPosition((precipmobj_t *)mobj);

		if (precipsector_list)
		{
			P_DelPrecipSeclist(precipsector_list);
			precipsector_list = NULL;
		}
	}
	else
	{
		// unlink from sector and block lists
		P_UnsetThingPosition(mobj);

		// Remove touching_sectorlist from mobj.
		if (sector_list)
		{
			P_DelSeclist(sector_list);
			sector_list = NULL;
		}
	}

	// stop any playing sound
	S_StopSound(mobj);

	// free block
	P_UnlinkThinker((thinker_t*)mobj);
}

static CV_PossibleValue_t respawnitemtime_cons_t[] = {{1, "MIN"}, {300, "MAX"}, {0, NULL}};
consvar_t cv_itemrespawntime = CVAR_INIT ("respawnitemtime", "2", CV_NETVAR|CV_CHEAT, respawnitemtime_cons_t, NULL);
consvar_t cv_itemrespawn = CVAR_INIT ("respawnitem", "On", CV_NETVAR, CV_OnOff, NULL);

static void P_SpawnPrecipitationAt(fixed_t basex, fixed_t basey)
{
	INT32 j, k;

	const mobjtype_t type = precipprops[curWeather].type;
	const UINT8 randomstates = (UINT8)mobjinfo[type].damage;
	const boolean flip = (mobjinfo[type].speed < 0);

	fixed_t i, x, y, z, height;

	UINT16 numparticles = 0;
	boolean condition = false;

	subsector_t *precipsector = NULL;
	precipmobj_t *rainmo = NULL;

	// If mobjscale < FRACUNIT, each blockmap cell covers
	// more area so spawn more precipitation in that area.
	for (i = 0; i < FRACUNIT; i += mapobjectscale)
	{
		x = basex + ((M_RandomKey(MAPBLOCKUNITS << 3) << FRACBITS) >> 3);
		y = basey + ((M_RandomKey(MAPBLOCKUNITS << 3) << FRACBITS) >> 3);

		precipsector = R_PointInSubsectorOrNull(x, y);

		// No sector? Stop wasting time,
		// move on to the next entry in the blockmap
		if (!precipsector)
			continue;

		// Not in a sector with visible sky?
		if (precipprops[curWeather].effects & PRECIPFX_WATERPARTICLES)
		{
			condition = false;

			if (precipsector->sector->ffloors)
			{
				ffloor_t *rover;

				for (rover = precipsector->sector->ffloors; rover; rover = rover->next)
				{
					if (!(rover->fofflags & FOF_EXISTS))
						continue;

					if (!(rover->fofflags & FOF_SWIMMABLE))
						continue;

					condition = true;
					break;
				}
			}
		}
		else
		{
			condition = (precipsector->sector->ceilingpic == skyflatnum);
		}

		if (precipsector->sector->flags & MSF_INVERTPRECIP)
		{
			condition = !condition;
		}

		if (!condition)
		{
			continue;
		}

		height = precipsector->sector->ceilingheight - precipsector->sector->floorheight;
		height = FixedDiv(height, mapobjectscale);

		// Exists, but is too small for reasonable precipitation.
		if (height < 64<<FRACBITS)
			continue;

		// Hack around a quirk of this entire system, where taller sectors look like they get less precipitation.
		numparticles = 1 + (height / (MAPBLOCKUNITS<<4<<FRACBITS));

		// Don't set z properly yet...
		z = (flip) ? (precipsector->sector->floorheight) : (precipsector->sector->ceilingheight);

		for (j = 0; j < numparticles; j++)
		{
			INT32 floorz;
			INT32 ceilingz;

			rainmo = P_SpawnPrecipMobj(x, y, z, type);

			if (randomstates > 0)
			{
				UINT8 mrand = M_RandomByte();
				UINT8 threshold = UINT8_MAX / (randomstates + 1);
				statenum_t st = mobjinfo[type].spawnstate;

				for (k = 0; k < randomstates; k++)
				{
					if (mrand < (threshold * (k+1)))
					{
						P_SetPrecipMobjState(rainmo, st+k+1);
						break;
					}
				}
			}

			floorz = rainmo->floorz >> FRACBITS;
			ceilingz = rainmo->ceilingz >> FRACBITS;

			if (floorz < ceilingz)
			{
				// Randomly assign a height, now that floorz is set.
				rainmo->z = M_RandomRange(floorz, ceilingz) << FRACBITS;
			}
			else
			{
				// ...except if the floor is above the ceiling.
				rainmo->z = ceilingz << FRACBITS;
			}
		}
	}
}

void P_SpawnPrecipitation(void)
{
	INT32 i;

	const mobjtype_t type = precipprops[curWeather].type;

	fixed_t basex, basey;

	if (dedicated || !cv_drawdist_precip.value || type == MT_NULL)
		return;

	// Use the blockmap to narrow down our placing patterns
	for (i = 0; i < bmapwidth*bmapheight; ++i)
	{
		basex = bmaporgx + (i % bmapwidth) * MAPBLOCKSIZE;
		basey = bmaporgy + (i / bmapwidth) * MAPBLOCKSIZE;

		P_SpawnPrecipitationAt(basex, basey);
	}
}

//
// P_PrecipitationEffects
//
void P_PrecipitationEffects(void)
{
	INT16 thunderchance = INT16_MAX;
	INT32 volume;
	size_t i;

	INT32 rainsfx = mobjinfo[precipprops[curWeather].type].seesound;
	INT32 rainfreq = mobjinfo[precipprops[curWeather].type].mass;

	boolean sounds_thunder = (precipprops[curWeather].effects & PRECIPFX_THUNDER);
	boolean effects_lightning = (precipprops[curWeather].effects & PRECIPFX_LIGHTNING);
	boolean lightningStrike = false;

	boolean sounds_rain = (rainsfx != sfx_None && (!leveltime || leveltime % rainfreq == 1));

	// No thunder except every other tic.
	if (!(leveltime & 1))
	{
		if ((precipprops[globalweather].effects & PRECIPFX_THUNDER)
		|| (precipprops[globalweather].effects & PRECIPFX_LIGHTNING))
		{
			// Before, consistency failures were possible if a level started
			// with global rain and switched players to anything else ...
			// If the global weather has lightning strikes,
			// EVERYONE gets them at the SAME time!
			thunderchance = (P_RandomKey(8192));
		}
		else if (sounds_thunder || effects_lightning)
		{
			// But on the other hand, if the global weather is ANYTHING ELSE,
			// don't sync lightning strikes.
			// While we'll only use the variable if we care about it, it's
			// nice to save on RNG calls when we don't need it.
			thunderchance = (M_RandomKey(8192));
		}
	}

	if (thunderchance < 70)
		lightningStrike = true;

	// Currently thunderstorming with lightning, and we're sounding the thunder...
	// and where there's thunder, there's gotta be lightning!
	if (effects_lightning && lightningStrike)
	{
		sector_t *ss = sectors;

		for (i = 0; i < numsectors; i++, ss++)
			if (ss->ceilingpic == skyflatnum) // Only for the sky.
				P_SpawnLightningFlash(ss); // Spawn a quick flash thinker
	}

	// Local effects from here on out!
	// If we're not in game fully yet, we don't worry about them.
	if (!playeringame[g_localplayers[0]] || !players[g_localplayers[0]].mo)
		return;

	if (sound_disabled)
		return; // Sound off? D'aw, no fun.

	if (!sounds_rain && !sounds_thunder)
		return; // no need to calculate volume at ALL

	if (players[g_localplayers[0]].mo->subsector->sector->ceilingpic == skyflatnum)
		volume = 255; // Sky above? We get it full blast.
	else
	{
		INT64 x, y, yl, yh, xl, xh;
		fixed_t closedist, newdist;

		// Essentially check in a 1024 unit radius of the player for an outdoor area.
#define RADIUSSTEP (64*FRACUNIT)
#define SEARCHRADIUS (16*RADIUSSTEP)
		yl = yh = players[g_localplayers[0]].mo->y;
		yl -= SEARCHRADIUS;
		while (yl < INT32_MIN)
			yl += RADIUSSTEP;
		yh += SEARCHRADIUS;
		while (yh > INT32_MAX)
			yh -= RADIUSSTEP;

		xl = xh = players[g_localplayers[0]].mo->x;
		xl -= SEARCHRADIUS;
		while (xl < INT32_MIN)
			xl += RADIUSSTEP;
		xh += SEARCHRADIUS;
		while (xh > INT32_MAX)
			xh -= RADIUSSTEP;

		closedist = SEARCHRADIUS*2;
#undef SEARCHRADIUS
		for (y = yl; y <= yh; y += RADIUSSTEP)
			for (x = xl; x <= xh; x += RADIUSSTEP)
			{
				if (R_PointInSubsector((fixed_t)x, (fixed_t)y)->sector->ceilingpic == skyflatnum) // Found the outdoors!
				{
					newdist = S_CalculateSoundDistance(players[g_localplayers[0]].mo->x, players[g_localplayers[0]].mo->y, 0, (fixed_t)x, (fixed_t)y, 0);
					if (newdist < closedist)
						closedist = newdist;
				}
			}
#undef RADIUSSTEP

		volume = 255 - (closedist>>(FRACBITS+2));
	}

	if (volume < 0)
		volume = 0;
	else if (volume > 255)
		volume = 255;

	if (sounds_rain)
		S_StartSoundAtVolume(players[g_localplayers[0]].mo, rainsfx, volume);

	if (!sounds_thunder)
		return;

	if (effects_lightning && lightningStrike && volume)
	{
		// Large, close thunder sounds to go with our lightning.
		S_StartSoundAtVolume(players[g_localplayers[0]].mo, sfx_litng1 + M_RandomKey(4), volume);
	}
	else if (thunderchance < 20)
	{
		// You can always faintly hear the thunder...
		if (volume < 80)
			volume = 80;

		S_StartSoundAtVolume(players[g_localplayers[0]].mo, sfx_athun1 + M_RandomKey(2), volume);
	}
}

void P_RespawnBattleBoxes(void)
{
	thinker_t *th;

	if (!(gametyperules & GTR_BUMPERS))
		return;

	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		mobj_t *box;
		mobj_t *newmobj;

		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		box = (mobj_t *)th;

		if (box->type != MT_RANDOMITEM
			|| (box->flags2 & MF2_DONTRESPAWN)
			|| box->threshold != 68
			|| box->fuse
			|| ((tic_t)box->cvmem+1 >= leveltime))
			continue; // only popped items

		// Respawn from mapthing if you have one!
		if (box->spawnpoint)
		{
			P_SpawnMapThing(box->spawnpoint);
			newmobj = box->spawnpoint->mobj; // this is set to the new mobj in P_SpawnMapThing
		}
		else
		{
			newmobj = P_SpawnMobj(box->x, box->y, box->z, box->type);
		}

		// Transfer flags2 (strongbox, objectflip, bossnotrap)
		newmobj->flags2 = box->flags2;
		P_RemoveMobj(box); // make sure they disappear

		if (numgotboxes > 0)
			numgotboxes--; // you've restored a box, remove it from the count
	}
}

/** Returns corresponding mobj type from mapthing number.
 * \param mthingtype Mapthing number in question.
 * \return Mobj type; MT_UNKNOWN if nothing found.
 */
mobjtype_t P_GetMobjtype(UINT16 mthingtype)
{
	mobjtype_t i;
	for (i = 0; i < NUMMOBJTYPES; i++)
		if (mthingtype == mobjinfo[i].doomednum)
			return i;
	return MT_UNKNOWN;
}

//
// P_RespawnSpecials
//
void P_RespawnSpecials(void)
{
	UINT8 p, pcount = 0;
	INT32 time = 30*TICRATE; // Respawn things in empty dedicated servers
	mapthing_t *mthing = NULL;

	if (!(gametyperules & GTR_CIRCUIT) && nummapboxes && (numgotboxes >= (4*nummapboxes/5))) // Battle Mode respawns all boxes in a different way
		P_RespawnBattleBoxes();

	// wait time depends on player count
	for (p = 0; p < MAXPLAYERS; p++)
	{
		if (playeringame[p] && !players[p].spectator)
			pcount++;
	}

#if 0 // set to 1 to enable quick respawns for testing
	if (true)
		time = 5*TICRATE;
	else
#endif
	if (gametyperules & GTR_SPHERES)
	{
		if (pcount > 2)
			time -= (5*TICRATE) * (pcount-2);

		if (time < 5*TICRATE)
			time = 5*TICRATE;
	}
	else
	{
		if (pcount == 1) // No respawn when alone
		{
			return;
		}
		else if (pcount > 1)
		{
			time = (120 - ((pcount-2) * 20)) * TICRATE;

			// If the map is longer or shorter than 3 laps, then adjust ring respawn to account for this.
			// 5 lap courses would have more retreaded ground, while 2 lap courses would have less.
			if ((mapheaderinfo[gamemap-1]->numlaps != 3)
				&& !(mapheaderinfo[gamemap-1]->levelflags & LF_SECTIONRACE))
			{
				time = (time * 3) / max(1, mapheaderinfo[gamemap-1]->numlaps);
			}

			if (time < 10*TICRATE)
			{
				// Ensure it doesn't go into absurdly low values
				time = 10*TICRATE;
			}
		}
	}

	// nothing left to respawn?
	if (iquehead == iquetail)
		return;

	// the first item in the queue is the first to respawn
	if (leveltime - itemrespawntime[iquetail] < (tic_t)time)
		return;

	mthing = itemrespawnque[iquetail];

#ifdef PARANOIA
	if (!mthing)
		I_Error("itemrespawnque[iquetail] is NULL!");
#endif

	if (mthing)
		P_SpawnMapThing(mthing);

	//CONS_Printf("respawn happened on tic %d, irt %d, t %d\n", leveltime, itemrespawntime[iquetail], time);

	// pull it from the que
	iquetail = (iquetail+1)&(ITEMQUESIZE-1);
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged between levels.
//
void P_SpawnPlayer(INT32 playernum)
{
	UINT8 i, pcount = 0; // MAXPLAYERS if exiting
	player_t *p = &players[playernum];
	mobj_t *mobj;

	if (p->playerstate == PST_REBORN)
		G_PlayerReborn(playernum, false);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (i == playernum)
			continue;
		if (!playeringame[i] || players[i].spectator)
			continue;
		if (players[i].exiting)
		{
			pcount = MAXPLAYERS;
			break;
		}
		if (players[i].jointime <= 1) // Prevent splitscreen hosters/joiners from only adding 1 player at a time in empty servers
			continue;
		pcount++;
	}

	// spawn as spectator determination
	if (multiplayer && demo.playback)
	{
		; // Don't mess with spectator values since the demo setup handles them already.
	}
	else if (p->bot)
	{
		/*
		if (bonusgame || specialstage)
		{
			// Bots should avoid
			p->spectator = true;
		}
		*/
		{
			// No point in a spectating bot!
			p->spectator = false;
		}
	}
	else if (netgame && p->jointime <= 1 && pcount)
	{
		p->spectator = true;

#if 0
		if (pcount == 1 || leveltime < starttime)
			p->pflags |= PF_WANTSTOJOIN;
		p->jointime = 2;
#endif
		p->spectatorreentry = 0; //(cv_spectatorreentry.value * TICRATE);
	}
	else if (multiplayer && !netgame)
	{
		// If you're in a team game and you don't have a team assigned yet...
		if (G_GametypeHasTeams() && p->ctfteam == 0)
		{
			changeteam_union NetPacket;
			UINT16 usvalue;
			NetPacket.value.l = NetPacket.value.b = 0;

			// Spawn as a spectator,
			// yes even in splitscreen mode
			p->spectator = true;
			p->spectatorreentry = 0; //(cv_spectatorreentry.value * TICRATE);
			
			if (playernum&1) p->skincolor = skincolor_redteam;
			else             p->skincolor = skincolor_blueteam;

			// but immediately send a team change packet.
			NetPacket.packet.playernum = playernum;
			NetPacket.packet.verification = true;
			NetPacket.packet.newteam = !(playernum&1) + 1;

			usvalue = SHORT(NetPacket.value.l|NetPacket.value.b);
			SendNetXCmd(XD_TEAMCHANGE, &usvalue, sizeof(usvalue));
		}
		else // Otherwise, never spectator.
		{
			p->spectator = false;
		}
	}

	if (G_GametypeHasTeams())
	{
		// Fix stupid non spectator spectators.
		if (!p->spectator && !p->ctfteam)
		{
			p->spectator = true;
			p->spectatorreentry = 0; //(cv_spectatorreentry.value * TICRATE);
		}

		// Fix team colors.
		// This code isn't being done right somewhere else. Oh well.
		if (p->ctfteam == 1)
			p->skincolor = skincolor_redteam;
		else if (p->ctfteam == 2)
			p->skincolor = skincolor_blueteam;
	}

	if (leveltime > introtime)
		p->flashing = K_GetKartFlashing(p); // Babysitting deterrent

	mobj = P_SpawnMobj(0, 0, 0, MT_PLAYER);
	(mobj->player = p)->mo = mobj;

	mobj->angle = mobj->old_angle = 0;

	// set color translations for player sprites
	mobj->color = p->skincolor;

	// set 'spritedef' override in mobj for player skins.. (see ProjectSprite)
	// (usefulness: when body mobj is detached from player (who respawns),
	// the dead body mobj retains the skin through the 'spritedef' override).
	mobj->skin = &skins[p->skin];
	P_SetupStateAnimation(mobj, mobj->state);

	mobj->health = 1;
	p->playerstate = PST_LIVE;

	if (!p->exiting || !p->realtime)
	{
		p->realtime = leveltime;
	}

	p->followitem = skins[p->skin].followitem;

	//awayview stuff
	p->awayviewmobj = NULL;
	p->awayviewtics = 0;

	p->skybox.viewpoint = skyboxviewpnts[0];
	p->skybox.centerpoint = skyboxcenterpnts[0];

	P_SetTarget(&p->follower, NULL);	// cleanse follower from existence

	if (K_PlayerShrinkCheat(p) == true)
	{
		mobj->destscale = FixedMul(mobj->destscale, SHRINK_SCALE);
	}

	// set the scale to the mobj's destscale so settings get correctly set.  if we don't, they sometimes don't.
	P_SetScale(mobj, mobj->destscale);
	P_FlashPal(p, 0, 0); // Resets

	p->grieftime = 0;
	
	if (gametyperules & GTR_BUMPERS)
	{
		mobj_t *overheadarrow = P_SpawnMobj(mobj->x, mobj->y, mobj->z + mobj->height + 16*FRACUNIT, MT_PLAYERARROW);
		P_SetTarget(&overheadarrow->target, mobj);
		overheadarrow->renderflags |= RF_DONTDRAW;
		P_SetScale(overheadarrow, mobj->destscale);

		if (p->spectator)
		{
			// HEY! No being cheap...
			p->bumpers = 0;
		}
		else if ((p->bumpers > 0) || (leveltime < starttime) || (pcount <= 1))
		{
			if ((leveltime < starttime) || (pcount <= 1)) // Start of the map?
			{
				if (leveltime > 2) // Reset those bumpers!
				{
					p->bumpers = K_StartingBumperCount();
					K_SpawnPlayerBattleBumpers(p);
				}
				else // temp, will get overwritten in K_BattleInit
				{
					p->bumpers = 1;
				}
			}
		}
		else if (p->bumpers <= 0)
		{
			mobj_t *karmahitbox = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_KARMAHITBOX); // Player hitbox is too small!!
			P_SetTarget(&karmahitbox->target, mobj);
			karmahitbox->destscale = mobj->scale;
			P_SetScale(karmahitbox, mobj->scale);
		}
	}
}

void P_AfterPlayerSpawn(INT32 playernum)
{
	player_t *p = &players[playernum];
	mobj_t *mobj = p->mo;
	UINT8 i;

	// Update interpolation
	mobj->old_x = mobj->x;
	mobj->old_y = mobj->y;
	mobj->old_z = mobj->z;
	mobj->old_angle = mobj->angle;

	P_SetPlayerAngle(p, mobj->angle);

	p->viewheight = P_GetPlayerViewHeight(p);

	if (p->mo->eflags & MFE_VERTICALFLIP)
		p->viewz = p->mo->z + p->mo->height - p->viewheight;
	else
		p->viewz = p->mo->z + p->viewheight;

	if (playernum == consoleplayer)
	{
		// wake up the status bar
		ST_Start();
		// wake up the heads up text
		HU_Start();
	}

	p->drawangle = mobj->angle;

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (camera[i].chase)
		{
			if (displayplayers[i] == playernum)
				P_ResetCamera(p, &camera[i]);
		}
	}

	if (CheckForReverseGravity)
		P_CheckGravity(mobj, false);
}

// spawn it at a playerspawn mapthing
void P_MovePlayerToSpawn(INT32 playernum, mapthing_t *mthing)
{
	fixed_t x = 0, y = 0;
	angle_t angle = 0;

	fixed_t z;
	sector_t *sector;
	fixed_t floor, ceiling;

	player_t *p = &players[playernum];
	mobj_t *mobj = p->mo;
	I_Assert(mobj != NULL);

	if (mthing)
	{
		x = mthing->x << FRACBITS;
		y = mthing->y << FRACBITS;
		angle = FixedAngle(mthing->angle*FRACUNIT);
		
		p->starpostx = mthing->x >> FRACBITS;
		p->starposty = mthing->y >> FRACBITS;
		p->starpostangle = FixedAngle(mthing->angle*FRACUNIT);
	}
	//spawn at the origin as a desperation move if there is no mapthing

	// set Z height
	sector = R_PointInSubsector(x, y)->sector;

	floor =
	sector->f_slope ? P_GetSlopeZAt(sector->f_slope, x, y) :
	sector->floorheight;
	ceiling =
	sector->c_slope ? P_GetSlopeZAt(sector->c_slope, x, y) :
	sector->ceilingheight;

	if (mthing)
	{
		// Setting the spawnpoint's args[0] will make the player start on the ceiling
		// Objectflip inverts
		if (!!(mthing->args[0]) ^ !!(mthing->options & MTF_OBJECTFLIP))
		{
			z = ceiling - mobjinfo[MT_PLAYER].height;
			if (mthing->options >> ZSHIFT)
				z -= ((mthing->options >> ZSHIFT) << FRACBITS);
			if (p->respawn)
				z -= 128*mapobjectscale;
		}
		else
		{
			z = floor;
			if (mthing->options >> ZSHIFT)
				z += ((mthing->options >> ZSHIFT) << FRACBITS);
			if (p->respawn)
				z += 128*mapobjectscale;
		}

		if (mthing->options & MTF_OBJECTFLIP) // flip the player!
		{
			mobj->eflags |= MFE_VERTICALFLIP;
			mobj->flags2 |= MF2_OBJECTFLIP;
		}

		if (mthing->args[0])
			P_SetPlayerMobjState(mobj, S_KART_SPINOUT);
	}
	else
		z = floor;

	if (z < floor)
		z = floor;
	else if (z > ceiling - mobjinfo[MT_PLAYER].height)
		z = ceiling - mobjinfo[MT_PLAYER].height;

	mobj->floorz = floor;
	mobj->ceilingz = ceiling;

	P_UnsetThingPosition(mobj);
	mobj->x = x;
	mobj->y = y;
	P_SetThingPosition(mobj);

	mobj->z = z;
	if (mobj->z == mobj->floorz)
		mobj->eflags |= MFE_ONGROUND;

	mobj->angle = angle;

	P_AfterPlayerSpawn(playernum);
}

void P_MovePlayerToStarpost(INT32 playernum)
{
	fixed_t z;
	sector_t *sector;
	fixed_t floor, ceiling;

	player_t *p = &players[playernum];
	mobj_t *mobj = p->mo;
	I_Assert(mobj != NULL);

	P_UnsetThingPosition(mobj);
	mobj->x = p->starpostx << FRACBITS;
	mobj->y = p->starposty << FRACBITS;
	P_SetThingPosition(mobj);
	sector = R_PointInSubsector(mobj->x, mobj->y)->sector;

	floor =
	sector->f_slope ? P_GetSlopeZAt(sector->f_slope, mobj->x, mobj->y) :
	sector->floorheight;
	ceiling =
	sector->c_slope ? P_GetSlopeZAt(sector->c_slope, mobj->x, mobj->y) :
	sector->ceilingheight;

	if (mobj->player->starpostflip)
		z = (p->starpostz<<FRACBITS) - FixedMul(128<<FRACBITS, mapobjectscale) - mobj->height;
	else
		z = (p->starpostz<<FRACBITS) + FixedMul(128<<FRACBITS, mapobjectscale);

	//z = (p->starpostz + 128) << FRACBITS; // reverse gravity exists, pls
	mobj->player->starpostflip = 0;

	if (z < floor)
		z = floor;
	else if (z > ceiling - mobjinfo[MT_PLAYER].height)
		z = ceiling - mobjinfo[MT_PLAYER].height;

	mobj->floorz = floor;
	mobj->ceilingz = ceiling;

	mobj->z = z;
	if (mobj->z == mobj->floorz)
		mobj->eflags |= MFE_ONGROUND;

	mobj->angle = p->starpostangle;

	//p->kartstuff[k_waypoint] = p->kartstuff[k_starpostwp]; // SRB2kart

	P_AfterPlayerSpawn(playernum);

	//if (!(netgame || multiplayer))
	//	leveltime = p->starposttime;
}

fixed_t P_GetMobjSpawnHeight(const mobjtype_t mobjtype, const fixed_t x, const fixed_t y, const fixed_t dz, const fixed_t offset, const boolean flip, const fixed_t scale)
{
	const subsector_t *ss = R_PointInSubsector(x, y);

	// Axis objects snap to the floor.
	if (mobjtype == MT_AXIS || mobjtype == MT_AXISTRANSFER || mobjtype == MT_AXISTRANSFERLINE)
		return ONFLOORZ;

	// Establish height.
	if (flip)
		return P_GetSectorCeilingZAt(ss->sector, x, y) - dz - FixedMul(scale, offset + mobjinfo[mobjtype].height);
	else
		return P_GetSectorFloorZAt(ss->sector, x, y) + dz + FixedMul(scale, offset);
}

fixed_t P_GetMapThingSpawnHeight(const mobjtype_t mobjtype, const mapthing_t* mthing, const fixed_t x, const fixed_t y)
{
	fixed_t dz = mthing->z << FRACBITS; // Base offset from the floor.
	fixed_t offset = 0; // Specific scaling object offset.
	boolean flip = (!!(mobjinfo[mobjtype].flags & MF_SPAWNCEILING) ^ !!(mthing->options & MTF_OBJECTFLIP));

	switch (mobjtype)
	{
	// Objects with a non-zero default height.
	// (None yet)

	// Horizontal springs, float additional units unless args[0] is set.
	case MT_YELLOWHORIZ:
	case MT_REDHORIZ:
	case MT_BLUEHORIZ:
		offset += mthing->args[0] ? 0 : 16*FRACUNIT;
		break;

	// Ring-like items, float additional units unless args[0] is set.
	case MT_SPIKEBALL:
	case MT_EMBLEM:
	case MT_RING:
	case MT_BLUESPHERE:
		offset += mthing->args[0] ? 0 : 24*FRACUNIT;
		break;

	// This object does not have an offset
	default:
		break;
	}

	if (!(dz + offset)) // Snap to the surfaces when there's no offset set.
	{
		if (flip)
			return ONCEILINGZ;
		else
			return ONFLOORZ;
	}

	return P_GetMobjSpawnHeight(mobjtype, x, y, dz, offset, flip, mthing->scale);
}

static boolean P_SpawnNonMobjMapThing(mapthing_t *mthing)
{
#if MAXPLAYERS > 32
	You should think about modifying the deathmatch starts to take full advantage of this!
#endif
	if (mthing->type <= MAXPLAYERS) // Player starts
	{
		// save spots for respawning in network games
		if (!metalrecording)
			playerstarts[mthing->type - 1] = mthing;
		return true;
	}
	else if (mthing->type == 33) // Match starts
	{
		if (numdmstarts < MAX_DM_STARTS)
		{
			deathmatchstarts[numdmstarts] = mthing;
			mthing->type = 0;
			numdmstarts++;
		}
		return true;
	}
	else if (mthing->type == 34) // Red CTF starts
	{
		if (numredctfstarts < MAXPLAYERS)
		{
			redctfstarts[numredctfstarts] = mthing;
			mthing->type = 0;
			numredctfstarts++;
		}
		return true;
	}
	else if (mthing->type == 35) // Blue CTF starts
	{
		if (numbluectfstarts < MAXPLAYERS)
		{
			bluectfstarts[numbluectfstarts] = mthing;
			mthing->type = 0;
			numbluectfstarts++;
		}
		return true;
	}
	else if (metalrecording && mthing->type == mobjinfo[MT_METALSONIC_RACE].doomednum)
	{
		// If recording, you ARE Metal Sonic. Do not spawn it, do not save normal spawnpoints.
		playerstarts[0] = mthing;
		return true;
	}
	else if (mthing->type == 750 // Slope vertex point (formerly chaos spawn)
		     || (mthing->type == FLOOR_SLOPE_THING || mthing->type == CEILING_SLOPE_THING) // Slope anchors
		     || (mthing->type >= 600 && mthing->type <= 611) // Special placement patterns
		     || mthing->type == 1713) // Hoops
	{
		return true; // These are handled elsewhere.
	}

	return false;
}

static boolean P_AllowMobjSpawn(mapthing_t* mthing, mobjtype_t i)
{
	(void)mthing;

	switch (i)
	{
		case MT_EMBLEM:
			if (netgame || multiplayer)
				return false; // Single player only

			if (modifiedgame && !savemoddata)
				return false; // No cheating!!
			break;
		case MT_RING:
			if (ringsdisabled)
				return false;
		case MT_ITEMCAPSULE:
			{
				boolean isRingCapsule = (mthing->args[0] < 1 || mthing->args[0] == KITEM_SUPERRING || mthing->args[0] >= NUMKARTITEMS);

				// don't spawn ring capsules in GTR_SPHERES gametypes
				if (isRingCapsule && (gametyperules & GTR_SPHERES))
					return false;

				// in record attack, only spawn ring capsules
				// (behavior can be inverted with the Extra flag, i.e. item capsule spawns and ring capsule does not)
				if (modeattacking
				&& (!(mthing->args[2] & TMICF_INVERTTIMEATTACK) == !isRingCapsule))
					return false;
			}
			break;
		default:
			break;
	}

	// No bosses outside of a combat situation.
	// (just in case we want boss arenas to do double duty as battle maps)
	if (!bossinfo.boss && (mobjinfo[i].flags & MF_BOSS))
		return false;

	if (metalrecording) // Metal Sonic can't use these things.
	{
		if (mobjinfo[i].flags & (MF_ENEMY|MF_BOSS))
			return false;
	}

	return true;
}

static mobjtype_t P_GetMobjtypeSubstitute(mapthing_t *mthing, mobjtype_t i)
{
	(void)mthing;

	if ((i == MT_RING) && (gametyperules & GTR_SPHERES))
		return MT_BLUESPHERE;

	if ((i == MT_RANDOMITEM) && (gametyperules & (GTR_PAPERITEMS|GTR_CIRCUIT)) == (GTR_PAPERITEMS|GTR_CIRCUIT) && !bossinfo.boss)
		return MT_PAPERITEMSPOT;

	return i;
}

static boolean P_SetupEmblem(mapthing_t *mthing, mobj_t *mobj)
{
	INT32 j;
	emblem_t* emblem = M_GetLevelEmblems(gamemap);
	skincolornum_t emcolor;

	while (emblem)
	{
		if (emblem->type == ET_GLOBAL && emblem->tag == Tag_FGet(&mthing->tags))
			break;

		emblem = M_GetLevelEmblems(-1);
	}

	if (!emblem)
	{
		CONS_Debug(DBG_GAMELOGIC, "No map emblem for map %d with tag %d found!\n", gamemap, Tag_FGet(&mthing->tags));
		return false;
	}

	j = emblem - emblemlocations;

	I_Assert(emblemlocations[j].sprite >= 'A' && emblemlocations[j].sprite <= 'Z');
	P_SetMobjState(mobj, mobj->info->spawnstate + (emblemlocations[j].sprite - 'A'));

	mobj->health = j + 1;
	emcolor = M_GetEmblemColor(&emblemlocations[j]); // workaround for compiler complaint about bad function casting
	mobj->color = (UINT16)emcolor;

	if (emblemlocations[j].collected)
	{
		P_UnsetThingPosition(mobj);
		mobj->flags |= MF_NOCLIP;
		mobj->flags &= ~MF_SPECIAL;
		mobj->flags |= MF_NOBLOCKMAP;
		mobj->frame |= (tr_trans50 << FF_TRANSSHIFT);
		P_SetThingPosition(mobj);
	}
	else
	{
		mobj->frame &= ~FF_TRANSMASK;

		if (emblemlocations[j].type == ET_GLOBAL)
		{
			mobj->reactiontime = emblemlocations[j].var;
		}
	}
	return true;
}

static boolean P_SetupMace(mapthing_t *mthing, mobj_t *mobj, boolean *doangle)
{
	fixed_t mlength, mmaxlength, mlengthset, mspeed, mphase, myaw, mpitch, mminlength, mnumspokes, mpinch, mroll, mnumnospokes, mwidth, mwidthset, mmin, msound, radiusfactor, widthfactor;
	angle_t mspokeangle;
	mobjtype_t chainlink, macetype, firsttype, linktype;
	boolean mdosound, mdocenter, mchainlike = false;
	mobj_t *spawnee = NULL, *hprev = mobj;
	mobjflag_t mflagsapply;
	mobjflag2_t mflags2apply;
	mobjeflag_t meflagsapply;
	const size_t mthingi = (size_t)(mthing - mapthings);

	mlength = abs(mthing->args[0]);
	mnumspokes = mthing->args[1] + 1;
	mspokeangle = FixedAngle((360*FRACUNIT)/mnumspokes) >> ANGLETOFINESHIFT;
	mwidth = max(0, mthing->args[2]);
	mspeed = abs(mthing->args[3] << 4);
	mphase = mthing->args[4] % 360;
	mpinch = mthing->args[5] % 360;
	mnumnospokes = mthing->args[6];
	mminlength = max(0, min(mlength - 1, mthing->args[7]));
	mpitch = mthing->pitch % 360;
	myaw = mthing->angle % 360;
	mroll = mthing->roll % 360;

	CONS_Debug(DBG_GAMELOGIC, "Mace/Chain (mapthing #%s):\n"
		"Length is %d (minus %d)\n"
		"Speed is %d\n"
		"Phase is %d\n"
		"Yaw is %d\n"
		"Pitch is %d\n"
		"No. of spokes is %d (%d antispokes)\n"
		"Pinch is %d\n"
		"Roll is %d\n"
		"Width is %d\n",
		sizeu1(mthingi), mlength, mminlength, mspeed, mphase, myaw, mpitch, mnumspokes, mnumnospokes, mpinch, mroll, mwidth);

	if (mnumnospokes > 0 && (mnumnospokes < mnumspokes))
		mnumnospokes = mnumspokes/mnumnospokes;
	else
		mnumnospokes = ((mobj->type == MT_CHAINMACEPOINT) ? (mnumspokes) : 0);

	mobj->lastlook = mspeed;
	mobj->movecount = mobj->lastlook;
	mobj->angle = FixedAngle(myaw << FRACBITS);
	*doangle = false;
	mobj->threshold = (FixedAngle(mpitch << FRACBITS) >> ANGLETOFINESHIFT);
	mobj->movefactor = mpinch;
	mobj->movedir = 0;

	// Mobjtype selection
	switch (mobj->type)
	{
	case MT_SPRINGBALLPOINT:
		macetype = ((mthing->args[8] & TMM_DOUBLESIZE)
			? MT_REDSPRINGBALL
			: MT_YELLOWSPRINGBALL);
		chainlink = MT_SMALLMACECHAIN;
		break;
	case MT_FIREBARPOINT:
		macetype = ((mthing->args[8] & TMM_DOUBLESIZE)
			? MT_BIGFIREBAR
			: MT_SMALLFIREBAR);
		chainlink = MT_NULL;
		break;
	case MT_CUSTOMMACEPOINT:
		macetype = mthing->stringargs[0] ? get_number(mthing->stringargs[0]) : MT_NULL;
		chainlink = mthing->stringargs[1] ? get_number(mthing->stringargs[1]) : MT_NULL;
		break;
	case MT_CHAINPOINT:
		if (mthing->args[8] & TMM_DOUBLESIZE)
		{
			macetype = MT_BIGGRABCHAIN;
			chainlink = MT_BIGMACECHAIN;
		}
		else
		{
			macetype = MT_SMALLGRABCHAIN;
			chainlink = MT_SMALLMACECHAIN;
		}
		mchainlike = true;
		break;
	default:
		if (mthing->args[8] & TMM_DOUBLESIZE)
		{
			macetype = MT_BIGMACE;
			chainlink = MT_BIGMACECHAIN;
		}
		else
		{
			macetype = MT_SMALLMACE;
			chainlink = MT_SMALLMACECHAIN;
		}
		break;
	}

	if (!macetype && !chainlink)
		return true;

	if (mobj->type == MT_CHAINPOINT)
	{
		if (!mlength)
			return true;
	}
	else
		mlength++;

	firsttype = macetype;

	// Adjustable direction
	if (mthing->args[8] & TMM_ALLOWYAWCONTROL)
		mobj->flags |= MF_SLIDEME;

	// Swinging
	if (mthing->args[8] & TMM_SWING)
	{
		mobj->flags2 |= MF2_STRONGBOX;
		mmin = ((mnumnospokes > 1) ? 1 : 0);
	}
	else
		mmin = mnumspokes;

	// If over distance away, don't move UNLESS this flag is applied
	if (mthing->args[8] & TMM_ALWAYSTHINK)
		mobj->flags2 |= MF2_BOSSNOTRAP;

	// Make the links the same type as the end - repeated below
	if ((mobj->type != MT_CHAINPOINT) && (((mthing->args[8] & TMM_MACELINKS) == TMM_MACELINKS) != (mobj->type == MT_FIREBARPOINT))) // exclusive or
	{
		linktype = macetype;
		radiusfactor = 2; // Double the radius.
	}
	else
		radiusfactor = (((linktype = chainlink) == MT_NULL) ? 2 : 1);

	if (!mchainlike)
		mchainlike = (firsttype == chainlink);
	widthfactor = (mchainlike ? 1 : 2);

	mflagsapply = (mthing->args[8] & TMM_CLIP) ? 0 : (MF_NOCLIP|MF_NOCLIPHEIGHT);
	mflags2apply = ((mthing->options & MTF_OBJECTFLIP) ? MF2_OBJECTFLIP : 0);
	meflagsapply = ((mthing->options & MTF_OBJECTFLIP) ? MFE_VERTICALFLIP : 0);

	msound = (mchainlike ? 0 : (mwidth & 1));

	// Quick and easy preparatory variable setting
	mphase = (FixedAngle(mphase << FRACBITS) >> ANGLETOFINESHIFT);
	mroll = (FixedAngle(mroll << FRACBITS) >> ANGLETOFINESHIFT);

#define makemace(mobjtype, dist, moreflags2) {\
	spawnee = P_SpawnMobj(mobj->x, mobj->y, mobj->z, mobjtype);\
	P_SetTarget(&spawnee->tracer, mobj);\
	spawnee->threshold = mphase;\
	spawnee->friction = mroll;\
	spawnee->movefactor = mwidthset;\
	spawnee->movecount = dist;\
	P_InitAngle(spawnee, myaw);\
	spawnee->flags |= (MF_NOGRAVITY|mflagsapply);\
	spawnee->flags2 |= (mflags2apply|moreflags2);\
	spawnee->eflags |= meflagsapply;\
	P_SetTarget(&hprev->hnext, spawnee);\
	P_SetTarget(&spawnee->hprev, hprev);\
	hprev = spawnee;\
}

	mdosound = (mspeed && !(mthing->args[8] & TMM_SILENT));
	mdocenter = (macetype && (mthing->args[8] & TMM_CENTERLINK));

	// The actual spawning of spokes
	while (mnumspokes-- > 0)
	{
		// Offsets
		if (mthing->args[8] & TMM_SWING) // Swinging
			mroll = (mroll - mspokeangle) & FINEMASK;
		else // Spinning
			mphase = (mphase - mspokeangle) & FINEMASK;

		if (mnumnospokes && !(mnumspokes % mnumnospokes)) // Skipping a "missing" spoke
		{
			if (mobj->type != MT_CHAINMACEPOINT)
				continue;

			linktype = chainlink;
			firsttype = ((mthing->args[8] & TMM_DOUBLESIZE) ? MT_BIGGRABCHAIN : MT_SMALLGRABCHAIN);
			mmaxlength = 1 + (mlength - 1) * radiusfactor;
			radiusfactor = widthfactor = 1;
		}
		else
		{
			if (mobj->type == MT_CHAINMACEPOINT)
			{
				// Make the links the same type as the end - repeated above
				if (mthing->args[8] & TMM_MACELINKS)
				{
					linktype = macetype;
					radiusfactor = 2;
				}
				else
					radiusfactor = (((linktype = chainlink) == MT_NULL) ? 2 : 1);

				firsttype = macetype;
				widthfactor = 2;
			}

			mmaxlength = mlength;
		}

		mwidthset = mwidth;
		mlengthset = mminlength;

		if (mdocenter) // Innermost link
			makemace(linktype, 0, 0);

		// Out from the center...
		if (linktype)
		{
			while ((++mlengthset) < mmaxlength)
				makemace(linktype, radiusfactor*mlengthset, 0);
		}
		else
			mlengthset = mmaxlength;

		// Outermost mace/link
		if (firsttype)
			makemace(firsttype, radiusfactor*mlengthset, MF2_AMBUSH);

		if (!mwidth)
		{
			if (mdosound && mnumspokes <= mmin) // Can it make a sound?
				spawnee->flags2 |= MF2_BOSSNOTRAP;
		}
		else
		{
			// Across the bar!
			if (!firsttype)
				mwidthset = -mwidth;
			else if (mwidth > 0)
			{
				while ((mwidthset -= widthfactor) > -mwidth)
				{
					makemace(firsttype, radiusfactor*mlengthset, MF2_AMBUSH);
					if (mdosound && (mwidthset == msound) && mnumspokes <= mmin) // Can it make a sound?
						spawnee->flags2 |= MF2_BOSSNOTRAP;
				}
			}
			else
			{
				while ((mwidthset += widthfactor) < -mwidth)
				{
					makemace(firsttype, radiusfactor*mlengthset, MF2_AMBUSH);
					if (mdosound && (mwidthset == msound) && mnumspokes <= mmin) // Can it make a sound?
						spawnee->flags2 |= MF2_BOSSNOTRAP;
				}
			}
			mwidth = -mwidth;

			// Outermost mace/link again!
			if (firsttype)
				makemace(firsttype, radiusfactor*(mlengthset--), MF2_AMBUSH);

			// ...and then back into the center!
			if (linktype)
				while (mlengthset > mminlength)
					makemace(linktype, radiusfactor*(mlengthset--), 0);

			if (mdocenter) // Innermost link
				makemace(linktype, 0, 0);
		}
	}
#undef makemace
	return true;
}

static boolean P_SetupParticleGen(mapthing_t *mthing, mobj_t *mobj)
{
	fixed_t radius, speed, zdist;
	INT32 type, numdivisions, anglespeed, ticcount;
	angle_t angledivision;
	INT32 line;
	const size_t mthingi = (size_t)(mthing - mapthings);

	// Find the corresponding linedef special, using args[6] as tag
	line = mthing->args[6] ? Tag_FindLineSpecial(15, mthing->args[6]) : -1;

	type = mthing->stringargs[0] ? get_number(mthing->stringargs[0]) : MT_PARTICLE;

	ticcount = mthing->args[4];
	if (ticcount < 1)
		ticcount = 3;

	numdivisions = mthing->args[0];

	if (numdivisions)
	{
		radius = mthing->args[1] << FRACBITS;
		anglespeed = (mthing->args[3]) % 360;
		angledivision = 360/numdivisions;
	}
	else
	{
		numdivisions = 1; // Simple trick to make P_ParticleGenSceneryThink simpler.
		radius = 0;
		anglespeed = 0;
		angledivision = 0;
	}

	speed = abs(mthing->args[2]) << FRACBITS;
	if (mthing->options & MTF_OBJECTFLIP)
		speed *= -1;

	zdist = abs(mthing->args[5]) << FRACBITS;

	CONS_Debug(DBG_GAMELOGIC, "Particle Generator (mapthing #%s):\n"
		"Radius is %d\n"
		"Speed is %d\n"
		"Anglespeed is %d\n"
		"Numdivisions is %d\n"
		"Angledivision is %d\n"
		"Type is %d\n"
		"Tic separation is %d\n",
		sizeu1(mthingi), radius, speed, anglespeed, numdivisions, angledivision, type, ticcount);

	if (line == -1)
		CONS_Debug(DBG_GAMELOGIC, "Spawn Z is %d\nZ dist is %d\n", mobj->z, zdist);
	else
		CONS_Debug(DBG_GAMELOGIC, "Heights are taken from control sector\n");

	mobj->angle = 0;
	mobj->movefactor = speed;
	mobj->lastlook = numdivisions;
	mobj->movedir = angledivision*ANG1;
	mobj->movecount = anglespeed*ANG1;
	mobj->friction = radius;
	mobj->threshold = type;
	mobj->reactiontime = ticcount;
	mobj->extravalue1 = zdist;
	mobj->cvmem = line;
	mobj->watertop = mobj->waterbottom = 0;
	return true;
}

static boolean P_SetupBooster(mapthing_t* mthing, mobj_t* mobj, boolean strong)
{
	angle_t angle = FixedAngle(mthing->angle << FRACBITS);
	fixed_t x1 = FINECOSINE((angle >> ANGLETOFINESHIFT) & FINEMASK);
	fixed_t y1 = FINESINE((angle >> ANGLETOFINESHIFT) & FINEMASK);
	fixed_t x2 = FINECOSINE(((angle + ANGLE_90) >> ANGLETOFINESHIFT) & FINEMASK);
	fixed_t y2 = FINESINE(((angle + ANGLE_90) >> ANGLETOFINESHIFT) & FINEMASK);
	statenum_t facestate = strong ? S_REDBOOSTERSEG_FACE : S_YELLOWBOOSTERSEG_FACE;
	statenum_t leftstate = strong ? S_REDBOOSTERSEG_LEFT : S_YELLOWBOOSTERSEG_LEFT;
	statenum_t rightstate = strong ? S_REDBOOSTERSEG_RIGHT : S_YELLOWBOOSTERSEG_RIGHT;
	statenum_t rollerstate = strong ? S_REDBOOSTERROLLER : S_YELLOWBOOSTERROLLER;

	mobj_t *seg = P_SpawnMobjFromMobj(mobj, 26*x1, 26*y1, 0, MT_BOOSTERSEG);
	P_InitAngle(seg, angle - ANGLE_90);
	P_SetMobjState(seg, facestate);
	seg = P_SpawnMobjFromMobj(mobj, -26*x1, -26*y1, 0, MT_BOOSTERSEG);
	P_InitAngle(seg, angle + ANGLE_90);
	P_SetMobjState(seg, facestate);
	seg = P_SpawnMobjFromMobj(mobj, 21*x2, 21*y2, 0, MT_BOOSTERSEG);
	P_InitAngle(seg, angle);
	P_SetMobjState(seg, leftstate);
	seg = P_SpawnMobjFromMobj(mobj, -21*x2, -21*y2, 0, MT_BOOSTERSEG);
	P_InitAngle(seg, angle);
	P_SetMobjState(seg, rightstate);

	seg = P_SpawnMobjFromMobj(mobj, 13*(x1 + x2), 13*(y1 + y2), 0, MT_BOOSTERROLLER);
	P_InitAngle(seg, angle);
	P_SetMobjState(seg, rollerstate);
	seg = P_SpawnMobjFromMobj(mobj, 13*(x1 - x2), 13*(y1 - y2), 0, MT_BOOSTERROLLER);
	P_InitAngle(seg, angle);
	P_SetMobjState(seg, rollerstate);
	seg = P_SpawnMobjFromMobj(mobj, -13*(x1 + x2), -13*(y1 + y2), 0, MT_BOOSTERROLLER);
	P_InitAngle(seg, angle);
	P_SetMobjState(seg, rollerstate);
	seg = P_SpawnMobjFromMobj(mobj, -13*(x1 - x2), -13*(y1 - y2), 0, MT_BOOSTERROLLER);
	P_InitAngle(seg, angle);
	P_SetMobjState(seg, rollerstate);

	return true;
}

static void P_SnapToFinishLine(mobj_t *mobj)
{
	line_t *finishline = P_FindNearestLine(mobj->x, mobj->y,
			mobj->subsector->sector, 2001); // case 2001: Finish Line
	if (finishline != NULL)
	{
		P_UnsetThingPosition(mobj);
		P_ClosestPointOnLine(mobj->x, mobj->y, finishline, (vertex_t *)&mobj->x);
		P_SetThingPosition(mobj);
	}
}

static mobj_t *P_MakeSoftwareCorona(mobj_t *mo, INT32 height)
{
	mobj_t *corona = P_SpawnMobjFromMobj(mo, 0, 0, height<<FRACBITS, MT_PARTICLE);
	corona->sprite = SPR_FLAM;
	corona->frame = (FF_FULLBRIGHT|FF_TRANS90|12);
	corona->tics = -1;
	return corona;
}

void P_InitSkyboxPoint(mobj_t *mobj, mapthing_t *mthing)
{
	mtag_t tag = Tag_FGet(&mthing->tags);
	if (tag < 0 || tag > 15)
	{
		CONS_Debug(DBG_GAMELOGIC, "P_InitSkyboxPoint: Skybox ID %d of mapthing %s is not between 0 and 15!\n", tag, sizeu1((size_t)(mthing - mapthings)));
		return;
	}

	if (mthing->args[0])
		P_SetTarget(&skyboxcenterpnts[tag], mobj);
	else
		P_SetTarget(&skyboxviewpnts[tag], mobj);
}

static boolean P_MapAlreadyHasStarPost(mobj_t *mobj)
{
	thinker_t *th;
	mobj_t *mo2;

	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2 == mobj)
			continue;

		if (mo2->type == MT_STARPOST && mo2->health == mobj->health)
			return true;
	}

	return false;
}

static boolean P_SetupSpawnedMapThing(mapthing_t *mthing, mobj_t *mobj, boolean *doangle)
{
	boolean override = LUA_HookMapThingSpawn(mobj, mthing);

	if (P_MobjWasRemoved(mobj))
		return false;

	if (override)
		return true;

	switch (mobj->type)
	{
	case MT_EMBLEM:
	{
		if (!P_SetupEmblem(mthing, mobj))
			return false;
		break;
	}
	case MT_SKYBOX:
	{
		P_InitSkyboxPoint(mobj, mthing);
		break;
	}
	case MT_EGGSTATUE:
		if (mthing->args[1])
		{
			mobj->color = SKINCOLOR_GOLD;
			mobj->colorized = true;
		}
		break;
	case MT_FAN:
		if (mthing->args[1] & TMF_INVISIBLE)
		{
			P_UnsetThingPosition(mobj);
			if (sector_list)
			{
				P_DelSeclist(sector_list);
				sector_list = NULL;
			}
			mobj->flags |= MF_NOSECTOR; // this flag basically turns it invisible
			P_SetThingPosition(mobj);
		}
		if (mthing->args[1] & TMF_NODISTANCECHECK)
			mobj->flags2 |= MF2_AMBUSH;
		if (mthing->args[0])
			mobj->health = mthing->args[0];
		else
			mobj->health = FixedMul(mobj->subsector->sector->ceilingheight - mobj->subsector->sector->floorheight, 3*(FRACUNIT/4)) >> FRACBITS;
		break;
	case MT_BALLOON:
		if (mthing->stringargs[0])
			mobj->color = get_number(mthing->stringargs[0]);
		if (mthing->args[0])
			mobj->flags2 |= MF2_AMBUSH;
		break;
	case MT_FLAME:
		if (mthing->args[0])
		{
			mobj_t *corona = P_MakeSoftwareCorona(mobj, 20);
			P_SetScale(corona, (corona->destscale = mobj->scale*3));
			P_SetTarget(&mobj->tracer, corona);
		}
		break;
	case MT_FLAMEHOLDER:
		if (!(mthing->args[0] & TMFH_NOFLAME)) // Spawn the fire
		{
			mobj_t *flame = P_SpawnMobjFromMobj(mobj, 0, 0, mobj->height, MT_FLAME);
			P_SetTarget(&flame->target, mobj);
			flame->flags2 |= MF2_BOSSNOTRAP;
			if (mthing->args[0] & TMFH_CORONA)
			{
				mobj_t *corona = P_MakeSoftwareCorona(flame, 20);
				P_SetScale(corona, (corona->destscale = flame->scale*3));
				P_SetTarget(&flame->tracer, corona);
			}
		}
		break;
	case MT_CANDLE:
	case MT_CANDLEPRICKET:
		if (mthing->args[0])
			P_MakeSoftwareCorona(mobj, ((mobj->type == MT_CANDLE) ? 42 : 176));
		break;
	case MT_JACKO1:
	case MT_JACKO2:
	case MT_JACKO3:
		if (!(mthing->args[0])) // take the torch out of the crafting recipe
		{
			mobj_t *overlay = P_SpawnMobjFromMobj(mobj, 0, 0, 0, MT_OVERLAY);
			P_SetTarget(&overlay->target, mobj);
			P_SetMobjState(overlay, mobj->info->raisestate);
		}
		break;
	case MT_WATERDRIP:
		mobj->tics = 3*TICRATE + mthing->args[0];
		break;
	case MT_FLAMEJET:
	case MT_VERTICALFLAMEJET:
		mobj->movecount = mthing->args[0];
		mobj->threshold = mthing->args[1];
		mobj->movedir = mthing->args[2];
		if (mthing->args[3])
			mobj->flags2 |= MF2_AMBUSH;
		break;
	case MT_MACEPOINT:
	case MT_CHAINMACEPOINT:
	case MT_SPRINGBALLPOINT:
	case MT_CHAINPOINT:
	case MT_FIREBARPOINT:
	case MT_CUSTOMMACEPOINT:
		if (!P_SetupMace(mthing, mobj, doangle))
			return false;
		break;
	case MT_PARTICLEGEN:
		if (!P_SetupParticleGen(mthing, mobj))
			return false;
		break;
	case MT_ROCKSPAWNER:
		mobj->threshold = mthing->angle;
		mobj->movecount = mthing->extrainfo;
		break;
	case MT_TUBEWAYPOINT:
	{
		UINT8 sequence = mthing->args[0];
		UINT8 id = mthing->args[1];
		mobj->health = id;
		mobj->threshold = sequence;
		P_AddTubeWaypoint(sequence, id, mobj);
		break;
	}
	case MT_DSZSTALAGMITE:
	case MT_DSZ2STALAGMITE:
	case MT_KELP:
		if (mthing->args[0]) { // make mobj twice as big as normal
			P_SetScale(mobj, 2*mobj->scale); // not 2*FRACUNIT in case of something like the old ERZ3 mode
			mobj->destscale = mobj->scale;
		}
		break;
	case MT_THZTREE:
	{ // Spawn the branches
		angle_t mobjangle = FixedAngle((mthing->angle % 113) << FRACBITS);
		P_InitAngle(P_SpawnMobjFromMobj(mobj, FRACUNIT, 0, 0, MT_THZTREEBRANCH), mobjangle + ANGLE_22h);
		P_InitAngle(P_SpawnMobjFromMobj(mobj, 0, FRACUNIT, 0, MT_THZTREEBRANCH), mobjangle + ANGLE_157h);
		P_InitAngle(P_SpawnMobjFromMobj(mobj, -FRACUNIT, 0, 0, MT_THZTREEBRANCH), mobjangle + ANGLE_270);
	}
	break;
	case MT_CEZPOLE1:
	case MT_CEZPOLE2:
	{ // Spawn the banner
		angle_t mobjangle = FixedAngle(mthing->angle << FRACBITS);
		P_InitAngle(P_SpawnMobjFromMobj(mobj,
			P_ReturnThrustX(mobj, mobjangle, 4 << FRACBITS),
			P_ReturnThrustY(mobj, mobjangle, 4 << FRACBITS),
			0, ((mobj->type == MT_CEZPOLE1) ? MT_CEZBANNER1 : MT_CEZBANNER2)), mobjangle + ANGLE_90);
	}
	break;
	case MT_HHZTREE_TOP:
	{ // Spawn the branches
		angle_t mobjangle = FixedAngle(mthing->angle << FRACBITS) & (ANGLE_90 - 1);
		mobj_t* leaf;
#define doleaf(x, y) \
			leaf = P_SpawnMobjFromMobj(mobj, x, y, 0, MT_HHZTREE_PART);\
			P_InitAngle(leaf, mobjangle);\
			P_SetMobjState(leaf, leaf->info->seestate);\
			mobjangle += ANGLE_90
		doleaf(FRACUNIT, 0);
		doleaf(0, FRACUNIT);
		doleaf(-FRACUNIT, 0);
		doleaf(0, -FRACUNIT);
#undef doleaf
	}
	break;
	case MT_SMASHINGSPIKEBALL:
		if (mthing->args[0] > 0)
			mobj->tics += mthing->args[0];
		break;
	case MT_BIGFERN:
	{
		angle_t angle = FixedAngle(mthing->angle << FRACBITS);
		UINT8 j;
		for (j = 0; j < 8; j++)
		{
			angle_t fa = (angle >> ANGLETOFINESHIFT) & FINEMASK;
			fixed_t xoffs = FINECOSINE(fa);
			fixed_t yoffs = FINESINE(fa);
			mobj_t* leaf = P_SpawnMobjFromMobj(mobj, xoffs, yoffs, 0, MT_BIGFERNLEAF);
			P_InitAngle(leaf, angle);
			angle += ANGLE_45;
		}
		break;
	}
	case MT_REDBOOSTER:
	case MT_YELLOWBOOSTER:
		if (!P_SetupBooster(mthing, mobj, mobj->type == MT_REDBOOSTER))
			return false;
		break;
	case MT_AXIS:
		// Inverted if args[3] is set
		if (mthing->args[3])
			mobj->flags2 |= MF2_AMBUSH;

		mobj->radius = abs(mthing->args[2]) << FRACBITS;
		// FALLTHRU
	case MT_AXISTRANSFER:
	case MT_AXISTRANSFERLINE:
		// Mare it belongs to
		mobj->threshold = min(mthing->args[0], 7);

		// # in the mare
		mobj->health = mthing->args[1];

		mobj->flags2 |= MF2_AXIS;
		break;
	case MT_STARPOST:
		mobj->health = mthing->args[0] + 1;
		if (!P_MapAlreadyHasStarPost(mobj))
			numstarposts++;
		break;
	case MT_BOSS3WAYPOINT:
		mobj->health = mthing->angle;
		mobj->movecount = mthing->extrainfo;
		P_SetTarget(&mobj->tracer, waypointcap);
		P_SetTarget(&waypointcap, mobj);
		numbosswaypoints++;
		break;
	case MT_SPIKE:
		// Pop up spikes!
		if (mthing->args[0])
		{
			mobj->flags &= ~MF_SCENERY;
			mobj->fuse = mthing->args[1];
		}
		if (mthing->args[2] & TMSF_RETRACTED)
			P_SetMobjState(mobj, mobj->info->meleestate);
		// Use per-thing collision for spikes unless the intangible flag is checked.
		if (!(mthing->args[2] & TMSF_INTANGIBLE) && !metalrecording)
		{
			P_UnsetThingPosition(mobj);
			mobj->flags &= ~(MF_NOBLOCKMAP|MF_NOGRAVITY|MF_NOCLIPHEIGHT);
			mobj->flags |= MF_SOLID;
			P_SetThingPosition(mobj);
		}
		break;
	case MT_WALLSPIKE:
		// Pop up spikes!
		if (mthing->args[0])
		{
			mobj->flags &= ~MF_SCENERY;
			mobj->fuse = mthing->args[1];
		}
		if (mthing->args[2] & TMSF_RETRACTED)
			P_SetMobjState(mobj, mobj->info->meleestate);
		// Use per-thing collision for spikes unless the intangible flag is checked.
		if (!(mthing->args[2] & TMSF_INTANGIBLE) && !metalrecording)
		{
			P_UnsetThingPosition(mobj);
			mobj->flags &= ~(MF_NOBLOCKMAP | MF_NOCLIPHEIGHT);
			mobj->flags |= MF_SOLID;
			P_SetThingPosition(mobj);
		}
		// spawn base
		{
			const angle_t mobjangle = FixedAngle(mthing->angle << FRACBITS); // the mobj's own angle hasn't been set quite yet so...
			const fixed_t baseradius = mobj->radius - mobj->scale;
			mobj_t* base = P_SpawnMobj(
				mobj->x - P_ReturnThrustX(mobj, mobjangle, baseradius),
				mobj->y - P_ReturnThrustY(mobj, mobjangle, baseradius),
				mobj->z, MT_WALLSPIKEBASE);
			P_InitAngle(base, mobjangle + ANGLE_90);
			base->destscale = mobj->destscale;
			P_SetScale(base, mobj->scale);
			P_SetTarget(&base->target, mobj);
			P_SetTarget(&mobj->tracer, base);
		}
		break;
	case MT_BIGTUMBLEWEED:
	case MT_LITTLETUMBLEWEED:
		if (mthing->args[0])
		{
			fixed_t offset = FixedMul(16*FRACUNIT, mobj->scale);
			mobj->momx += P_RandomChance(FRACUNIT/2) ? offset : -offset;
			mobj->momy += P_RandomChance(FRACUNIT/2) ? offset : -offset;
			mobj->momz += offset;
			mobj->flags2 |= MF2_AMBUSH;
		}
		break;
	case MT_REDFLAG:
		redflag = mobj;
		rflagpoint = mobj->spawnpoint;
		break;
	case MT_BLUEFLAG:
		blueflag = mobj;
		bflagpoint = mobj->spawnpoint;
		break;
	// SRB2Kart
	case MT_WAYPOINT:
	{
		const fixed_t mobjscale =
			mapheaderinfo[gamemap-1]->default_waypoint_radius;
		mtag_t tag = Tag_FGet(&mthing->tags);

		if (mthing->args[1] > 0)
			mobj->radius = (mthing->args[1]) * FRACUNIT;
		else if (mobjscale > 0)
			mobj->radius = mobjscale;
		else
			mobj->radius = DEFAULT_WAYPOINT_RADIUS * mapobjectscale;

		// Use threshold to store the next waypoint ID
		// movecount is being used for the current waypoint ID
		// reactiontime lets us know if we can respawn at it
		// lastlook is used for indicating the waypoint is a shortcut
		// extravalue1 is used for indicating the waypoint is disabled
		// extravalue2 is used for indicating the waypoint is the finishline
		mobj->threshold = mthing->args[0];
		mobj->movecount = tag;
		if (mthing->args[2] & TMWPF_DISABLED)
		{
			mobj->extravalue1 = 0; // The waypoint is disabled if extra is on
		}
		else
		{
			mobj->extravalue1 = 1;
		}
		if (mthing->args[2] & TMWPF_SHORTCUT)
		{
			mobj->lastlook = 1; // the waypoint is a shortcut if objectspecial is on
		}
		else
		{
			mobj->lastlook = 0;
		}
		if (mthing->args[2] & TMWPF_NORESPAWN)
		{
			mobj->reactiontime = 0; // Can't respawn at if Ambush is on
		}
		else
		{
			mobj->reactiontime = 1;
		}
		if (mthing->args[2] & TMWPF_FINISHLINE)
		{
			mobj->extravalue2 = 1; // args[2] of 1 means the waypoint is at the finish line
			mobj->reactiontime = 0; // Also don't respawn at finish lines

			P_SnapToFinishLine(mobj);
		}
		else
		{
			mobj->extravalue2 = 0;
		}

		// Sryder 2018-12-7: Grabbed this from the old MT_BOSS3WAYPOINT section so they'll be in the waypointcap instead
		P_SetTarget(&mobj->tracer, waypointcap);
		P_SetTarget(&waypointcap, mobj);
		break;
	}
	case MT_BOTHINT:
	{
		// Change size
		if (mthing->args[0] > 0)
		{
			mobj->radius = mthing->args[0] * FRACUNIT;
		}
		else
		{
			mobj->radius = 32 * mapobjectscale;
		}

		// Steer away instead of towards
		if (mthing->args[2])
		{
			mobj->extravalue1 = 0;
		}
		else
		{
			mobj->extravalue1 = 1;
		}

		// Steering amount
		if (mthing->args[1] == 0)
		{
			mobj->extravalue2 = 4;
		}
		else
		{
			mobj->extravalue2 = mthing->args[1];
		}
		break;
	}
	case MT_RANDOMITEM:
	{
		boolean delayed = !(gametyperules & GTR_CIRCUIT);
		if (leveltime < (delayed ? starttime : 3))
		{
			mobj->flags2 |= MF2_BOSSNOTRAP; // mark as here on map start
			if (delayed)
			{
				P_UnsetThingPosition(mobj);
				mobj->flags |= (MF_NOCLIPTHING|MF_NOBLOCKMAP);
				mobj->renderflags |= RF_DONTDRAW;
				P_SetThingPosition(mobj);
			}
		}
		else
		{
			P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_EXPLODE);
		}
		break;
	}
	case MT_ITEMCAPSULE:
	{
		// we have to adjust for reverse gravity early so that the below grounded checks work
		if (mthing->options & MTF_OBJECTFLIP)
		{
			mobj->eflags |= MFE_VERTICALFLIP;
			mobj->flags2 |= MF2_OBJECTFLIP;
			mobj->z += FixedMul(mobj->extravalue1, mobj->info->height) - mobj->height;
		}

		// determine whether this capsule is grounded or aerial
		if (mobj->subsector->sector->ffloors)
			P_AdjustMobjFloorZ_FFloors(mobj, mobj->subsector->sector, 0);
		if (mobj->subsector->polyList)
			P_AdjustMobjFloorZ_PolyObjs(mobj, mobj->subsector);
		if (!P_IsObjectOnGround(mobj))
			mobj->flags |= MF_NOGRAVITY;

		// Angle = item type
		if (mthing->args[0] > 0 && mthing->args[0] < NUMKARTITEMS)
			mobj->threshold = mthing->args[0];

		// Parameter = extra items (x5 for rings)
		mobj->movecount += mthing->args[1];

		// Ambush = double size (grounded) / half size (aerial)
		if (!(mthing->args[2] & TMICF_INVERTSIZE) == !P_IsObjectOnGround(mobj))
		{
			mobj->extravalue1 <<= 1;
			mobj->scalespeed <<= 1;
		}

		const fixed_t blimit = FixedDiv(MAPBLOCKSIZE, mobj->info->radius);
		if (mobj->extravalue1 > blimit)
		{
			 // don't make them larger than the blockmap can handle
			mobj->extravalue1 = blimit;
		}

		break;
	}
	case MT_AAZTREE_HELPER:
	{
		fixed_t top = mobj->z;
		UINT8 i;
		UINT8 locnumsegs = (mthing->extrainfo)+2;
		UINT8 numleaves = max(3, (abs(mthing->angle+1) % 6) + 3);
		mobj_t *coconut;

		// Spawn tree segments
		for (i = 0; i < locnumsegs; i++)
		{
			P_SpawnMobj(mobj->x, mobj->y, top, MT_AAZTREE_SEG);
			top += FixedMul(mobjinfo[MT_AAZTREE_SEG].height, mobj->scale);
		}

		// Big coconut topper
		coconut = P_SpawnMobj(mobj->x, mobj->y, top - (8<<FRACBITS), MT_AAZTREE_COCONUT);
		P_SetScale(coconut, (coconut->destscale = (2*mobj->scale)));

		// Spawn all of the papersprite leaves
		for (i = 0; i < numleaves; i++)
		{
			mobj_t *leaf;

			mobj->angle = FixedAngle((i * (360/numleaves))<<FRACBITS);

			leaf = P_SpawnMobj(mobj->x + FINECOSINE((mobj->angle>>ANGLETOFINESHIFT) & FINEMASK),
				mobj->y + FINESINE((mobj->angle>>ANGLETOFINESHIFT) & FINEMASK), top, MT_AAZTREE_LEAF);
			P_InitAngle(leaf, mobj->angle);

			// Small coconut for each leaf
			P_SpawnMobj(mobj->x + (32 * FINECOSINE((mobj->angle>>ANGLETOFINESHIFT) & FINEMASK)),
				mobj->y + (32 * FINESINE((mobj->angle>>ANGLETOFINESHIFT) & FINEMASK)), top - (24<<FRACBITS), MT_AAZTREE_COCONUT);
		}

		P_RemoveMobj(mobj); // Don't need this helper obj anymore
		return false;
	}
	case MT_BATTLECAPSULE:
	{
		sector_t *sec = R_PointInSubsector(mobj->x, mobj->y)->sector;
		mobj_t *cur, *prev = mobj;
		fixed_t floorheights[MAXFFLOORS+1];
		UINT8 numfloors = 0;
		boolean fly = true;
		UINT8 i;

		// This floor height stuff is stupid but I couldn't get it to work any other way for whatever reason
		if (mthing->options & MTF_OBJECTFLIP)
		{
			floorheights[numfloors] = P_GetSectorCeilingZAt(sec, mobj->x, mobj->y) - mobj->height;
		}
		else
		{
			floorheights[numfloors] = P_GetSectorFloorZAt(sec, mobj->x, mobj->y);
		}

		numfloors++;

		if (sec->ffloors)
		{
			ffloor_t *rover;
			for (rover = sec->ffloors; rover; rover = rover->next)
			{
				if ((rover->fofflags & FOF_EXISTS) && (rover->fofflags & FOF_BLOCKOTHERS))
				{
					if (mthing->options & MTF_OBJECTFLIP)
					{
						floorheights[numfloors] = P_GetFFloorBottomZAt(rover, mobj->x, mobj->y) - mobj->height;
					}
					else
					{
						floorheights[numfloors] = P_GetFFloorBottomZAt(rover, mobj->x, mobj->y);
					}

					numfloors++;
				}
			}
		}

		for (i = 0; i < numfloors; i++)
		{
			if (mobj->z == floorheights[i])
			{
				fly = false;
				break;
			}
		}

		// Flying capsules
		if (fly)
		{
			mobj->flags |= MF_NOGRAVITY;
			mobj->extravalue1 = 1; // Set extravalue1 for later reference
		}

		// Moving capsules!
		if (mthing->args[0] && mthing->args[1])
		{
			K_SetupMovingCapsule(mthing, mobj);
		}

		// NOW FOR ALL OF THE PIECES!!
		// Init hnext list
		// Spherical top
		cur = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_BATTLECAPSULE_PIECE);
		P_SetMobjState(cur, S_BATTLECAPSULE_TOP);

		P_SetTarget(&cur->target, mobj);
		P_SetTarget(&cur->hprev, prev);
		P_SetTarget(&prev->hnext, cur);
		prev = cur;

		// Tippity-top decorational button
		cur = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_BATTLECAPSULE_PIECE);
		P_SetMobjState(cur, S_BATTLECAPSULE_BUTTON);

		P_SetTarget(&cur->target, mobj);
		P_SetTarget(&cur->hprev, prev);
		P_SetTarget(&prev->hnext, cur);
		prev = cur;

		// Supports on the bottom
		for (i = 0; i < 4; i++)
		{
			cur = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_BATTLECAPSULE_PIECE);
			cur->extravalue1 = i;

			if (mobj->extravalue1) // Flying capsule, moving or not
				P_SetMobjState(cur, S_BATTLECAPSULE_SUPPORTFLY);
			else if (mobj->target && !P_MobjWasRemoved(mobj->target)) // Grounded, moving capsule
				P_SetMobjState(cur, S_KARMAWHEEL);
			else
				P_SetMobjState(cur, S_BATTLECAPSULE_SUPPORT); // Grounded, stationary capsule

			P_SetTarget(&cur->target, mobj);
			P_SetTarget(&cur->hprev, prev);
			P_SetTarget(&prev->hnext, cur);
			prev = cur;
		}

		// Side paneling
		for (i = 0; i < 8; i++)
		{
			cur = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_BATTLECAPSULE_PIECE);
			cur->extravalue1 = i;

			if (i & 1)
				P_SetMobjState(cur, S_BATTLECAPSULE_SIDE2);
			else
				P_SetMobjState(cur, S_BATTLECAPSULE_SIDE1);

			P_SetTarget(&cur->target, mobj);
			P_SetTarget(&cur->hprev, prev);
			P_SetTarget(&prev->hnext, cur);
			prev = cur;
		}

		// Increment no. of capsules on the map counter
		maptargets++;
	}
	default:
		break;
	}

	if (mobj->flags & MF_BOSS)
	{
		if (mthing->args[1]) // No egg trap for this boss
			mobj->flags2 |= MF2_BOSSNOTRAP;
	}

	return true;
}

static mobj_t *P_SpawnMobjFromMapThing(mapthing_t *mthing, fixed_t x, fixed_t y, fixed_t z, mobjtype_t i)
{
	mobj_t *mobj = NULL;
	boolean doangle = true;

	mobj = P_SpawnMobj(x, y, z, i);
	mobj->spawnpoint = mthing;

	P_SetScale(mobj, FixedMul(mobj->scale, mthing->scale));
	mobj->destscale = FixedMul(mobj->destscale, mthing->scale);
	
	mobj->spritexscale = mthing->spritexscale;
	mobj->spriteyscale = mthing->spriteyscale;

	if (!P_SetupSpawnedMapThing(mthing, mobj, &doangle))
		return mobj;

	if (doangle)
	{
		P_InitAngle(mobj, FixedAngle(mthing->angle << FRACBITS));
	}

	if ((mobj->flags & MF_SPRING)
	&& mobj->info->damage != 0
	&& mobj->info->mass == 0)
	{
		// Offset sprite of horizontal springs
		angle_t a = mobj->angle + ANGLE_180;
		mobj->sprxoff = FixedMul(mobj->radius, FINECOSINE(a >> ANGLETOFINESHIFT));
		mobj->spryoff = FixedMul(mobj->radius, FINESINE(a >> ANGLETOFINESHIFT));
	}

	P_InitPitch(mobj, FixedAngle(mthing->pitch << FRACBITS));
	P_InitRoll(mobj, FixedAngle(mthing->roll << FRACBITS));

	P_SetThingTID(mobj, Tag_FGet(&mthing->tags));

	mthing->mobj = mobj;

	// Generic reverse gravity for individual objects flag.
	if (mthing->options & MTF_OBJECTFLIP)
	{
		mobj->eflags |= MFE_VERTICALFLIP;
		mobj->flags2 |= MF2_OBJECTFLIP;
	}

	return mobj;
}

//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
mobj_t *P_SpawnMapThing(mapthing_t *mthing)
{
	mobjtype_t i;
	mobj_t *mobj = NULL;
	fixed_t x, y, z;

	if (!mthing->type)
		return mobj; // Ignore type-0 things as NOPs

	if (mthing->type == 3328) // 3D Mode start Thing
		return mobj;

	if (!objectplacing && P_SpawnNonMobjMapThing(mthing))
		return mobj;

	i = P_GetMobjtype(mthing->type);
	if (i == MT_UNKNOWN)
		CONS_Alert(CONS_WARNING, M_GetText("Unknown thing type %d placed at (%d, %d)\n"), mthing->type, mthing->x, mthing->y);

	// Skip all returning/substitution code in objectplace.
	if (!objectplacing)
	{
		if (!P_AllowMobjSpawn(mthing, i))
			return mobj;

		i = P_GetMobjtypeSubstitute(mthing, i);
		if (i == MT_NULL) // Don't spawn mobj
			return mobj;
	}

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	z = P_GetMapThingSpawnHeight(i, mthing, x, y);
	return P_SpawnMobjFromMapThing(mthing, x, y, z, i);
}

void P_SpawnHoop(mapthing_t *mthing)
{
	if (metalrecording)
		return;

	mobj_t *mobj = NULL;
	mobj_t *nextmobj = NULL;
	mobj_t *hoopcenter;
	TMatrix *pitchmatrix, *yawmatrix;
	fixed_t radius = mthing->args[0] << FRACBITS;
	fixed_t sizefactor = 4*FRACUNIT;
	fixed_t hoopsize = radius/sizefactor;
	INT32 i;
	angle_t fa;
	TVector v, *res;
	fixed_t x = mthing->x << FRACBITS;
	fixed_t y = mthing->y << FRACBITS;
	fixed_t z = P_GetMobjSpawnHeight(MT_HOOP, x, y, mthing->z << FRACBITS, 0, false, mthing->scale);

	hoopcenter = P_SpawnMobj(x, y, z, MT_HOOPCENTER);
	hoopcenter->spawnpoint = mthing;
	hoopcenter->z -= hoopcenter->height/2;

	P_UnsetThingPosition(hoopcenter);
	hoopcenter->x = x;
	hoopcenter->y = y;
	P_SetThingPosition(hoopcenter);

	hoopcenter->movedir = mthing->pitch;
	pitchmatrix = RotateXMatrix(FixedAngle(hoopcenter->movedir << FRACBITS));
	hoopcenter->movecount = mthing->angle;
	yawmatrix = RotateZMatrix(FixedAngle(hoopcenter->movecount << FRACBITS));

	// For the hoop when it flies away
	hoopcenter->extravalue1 = hoopsize;
	hoopcenter->extravalue2 = radius/12;

	// Create the hoop!
	for (i = 0; i < hoopsize; i++)
	{
		fa = i*(FINEANGLES/hoopsize);
		v[0] = FixedMul(FINECOSINE(fa), radius);
		v[1] = 0;
		v[2] = FixedMul(FINESINE(fa), radius);
		v[3] = FRACUNIT;

		res = VectorMatrixMultiply(v, *pitchmatrix);
		M_Memcpy(&v, res, sizeof(v));
		res = VectorMatrixMultiply(v, *yawmatrix);
		M_Memcpy(&v, res, sizeof(v));

		mobj = P_SpawnMobj(x + v[0], y + v[1], z + v[2], MT_HOOP);
		mobj->z -= mobj->height/2;

		P_SetTarget(&mobj->target, hoopcenter); // Link the sprite to the center.
		mobj->fuse = 0;

		// Link all the sprites in the hoop together
		if (nextmobj)
		{
			P_SetTarget(&mobj->hprev, nextmobj);
			P_SetTarget(&mobj->hprev->hnext, mobj);
		}
		else
			P_SetTarget(&mobj->hprev, P_SetTarget(&mobj->hnext, NULL));

		nextmobj = mobj;
	}

	// Create the collision detectors!
	// Create them until the size is less than 8
	// But always create at least ONE set of collision detectors
	do
	{
		if (hoopsize >= 32)
			hoopsize -= 16;
		else
			hoopsize /= 2;

		radius = hoopsize*sizefactor;

		for (i = 0; i < hoopsize; i++)
		{
			fa = i*(FINEANGLES/hoopsize);
			v[0] = FixedMul(FINECOSINE(fa), radius);
			v[1] = 0;
			v[2] = FixedMul(FINESINE(fa), radius);
			v[3] = FRACUNIT;

			res = VectorMatrixMultiply(v, *pitchmatrix);
			M_Memcpy(&v, res, sizeof(v));
			res = VectorMatrixMultiply(v, *yawmatrix);
			M_Memcpy(&v, res, sizeof(v));

			mobj = P_SpawnMobj(x + v[0], y + v[1], z + v[2], MT_HOOPCOLLIDE);
			mobj->z -= mobj->height/2;

			// Link all the collision sprites together.
			P_SetTarget(&mobj->hnext, NULL);
			P_SetTarget(&mobj->hprev, nextmobj);
			P_SetTarget(&mobj->hprev->hnext, mobj);

			nextmobj = mobj;
		}
	} while (hoopsize >= 8);
}

static void P_SpawnItemRow(mapthing_t *mthing, mobjtype_t *itemtypes, UINT8 numitemtypes, INT32 numitems, fixed_t horizontalspacing, fixed_t verticalspacing, INT16 fixedangle)
{
	mapthing_t dummything;
	mobj_t *mobj = NULL;
	fixed_t x = mthing->x << FRACBITS;
	fixed_t y = mthing->y << FRACBITS;
	fixed_t z = mthing->z << FRACBITS;
	INT32 r;
	angle_t angle = FixedAngle(fixedangle << FRACBITS);
	angle_t fineangle = (angle >> ANGLETOFINESHIFT) & FINEMASK;

	for (r = 0; r < numitemtypes; r++)
	{
		dummything = *mthing;
		dummything.type = mobjinfo[itemtypes[r]].doomednum;
		// Skip all returning/substitution code in objectplace.
		if (!objectplacing)
		{
			if (!P_AllowMobjSpawn(&dummything, itemtypes[r]))
			{
				itemtypes[r] = MT_NULL;
				continue;
			}

			itemtypes[r] = P_GetMobjtypeSubstitute(&dummything, itemtypes[r]);
		}
	}
	z = P_GetMobjSpawnHeight(itemtypes[0], x, y, z, 0, mthing->options & MTF_OBJECTFLIP, mthing->scale);

	for (r = 0; r < numitems; r++)
	{
		mobjtype_t itemtype = itemtypes[r % numitemtypes];
		if (itemtype == MT_NULL)
			continue;
		dummything.type = mobjinfo[itemtype].doomednum;

		x += FixedMul(horizontalspacing, FINECOSINE(fineangle));
		y += FixedMul(horizontalspacing, FINESINE(fineangle));
		z += (mthing->options & MTF_OBJECTFLIP) ? -verticalspacing : verticalspacing;

		mobj = P_SpawnMobjFromMapThing(&dummything, x, y, z, itemtype);

		if (!mobj)
			continue;

		mobj->spawnpoint = NULL;
	}
}

static void P_SpawnSingularItemRow(mapthing_t *mthing, mobjtype_t itemtype, INT32 numitems, fixed_t horizontalspacing, fixed_t verticalspacing, INT16 fixedangle)
{
	mobjtype_t itemtypes[1] = { itemtype };
	P_SpawnItemRow(mthing, itemtypes, 1, numitems, horizontalspacing, verticalspacing, fixedangle);
}

static void P_SpawnItemCircle(mapthing_t *mthing, mobjtype_t *itemtypes, UINT8 numitemtypes, INT32 numitems, fixed_t size)
{
	mapthing_t dummything;
	mobj_t* mobj = NULL;
	fixed_t x = mthing->x << FRACBITS;
	fixed_t y = mthing->y << FRACBITS;
	fixed_t z = mthing->z << FRACBITS;
	angle_t angle = FixedAngle(mthing->angle << FRACBITS);
	angle_t fa;
	INT32 i;
	TVector v, *res;

	for (i = 0; i < numitemtypes; i++)
	{
		dummything = *mthing;
		dummything.type = mobjinfo[itemtypes[i]].doomednum;
		// Skip all returning/substitution code in objectplace.
		if (!objectplacing)
		{
			if (!P_AllowMobjSpawn(&dummything, itemtypes[i]))
			{
				itemtypes[i] = MT_NULL;
				continue;
			}

			itemtypes[i] = P_GetMobjtypeSubstitute(&dummything, itemtypes[i]);
		}
	}
	z = P_GetMobjSpawnHeight(itemtypes[0], x, y, z, 0, false, mthing->scale);

	for (i = 0; i < numitems; i++)
	{
		mobjtype_t itemtype = itemtypes[i % numitemtypes];
		if (itemtype == MT_NULL)
			continue;
		dummything.type = mobjinfo[itemtype].doomednum;

		fa = i*FINEANGLES/numitems;
		v[0] = FixedMul(FINECOSINE(fa), size);
		v[1] = 0;
		v[2] = FixedMul(FINESINE(fa), size);
		v[3] = FRACUNIT;

		res = VectorMatrixMultiply(v, *RotateZMatrix(angle));
		M_Memcpy(&v, res, sizeof(v));

		mobj = P_SpawnMobjFromMapThing(&dummything, x + v[0], y + v[1], z + v[2], itemtype);

		if (!mobj)
			continue;

		mobj->z -= mobj->height/2;
		mobj->spawnpoint = NULL;
	}
}

static void P_ParseItemTypes(char *itemstring, mobjtype_t *itemtypes, UINT8 *numitemtypes)
{
	char *tok;

	*numitemtypes = 0;
	if (itemstring)
	{
		char *stringcopy = Z_Malloc(strlen(itemstring) + 1, PU_LEVEL, NULL);
		M_Memcpy(stringcopy, itemstring, strlen(itemstring));
		stringcopy[strlen(itemstring)] = '\0';

		tok = strtok(stringcopy, " ");
		while (tok && *numitemtypes < 128)
		{
			itemtypes[*numitemtypes] = get_number(tok);
			tok = strtok(NULL, " ");
			(*numitemtypes)++;
		}

		Z_Free(stringcopy);
	}
	else
	{
		//If no types are supplied, default to ring
		itemtypes[0] = MT_RING;
		*numitemtypes = 1;
	}
}

void P_SpawnItemPattern(mapthing_t *mthing)
{
	switch (mthing->type)
	{
	// Special placement patterns
	case 600: // 5 vertical rings (yellow spring)
		P_SpawnSingularItemRow(mthing, MT_RING, 5, 0, 64*FRACUNIT, 0);
		return;
	case 601: // 5 vertical rings (red spring)
		P_SpawnSingularItemRow(mthing, MT_RING, 5, 0, 128*FRACUNIT, 0);
		return;
	case 602: // 5 diagonal rings (yellow spring)
		P_SpawnSingularItemRow(mthing, MT_RING, 5, 64*FRACUNIT, 64*FRACUNIT, mthing->angle);
		return;
	case 603: // 10 diagonal rings (red spring)
		P_SpawnSingularItemRow(mthing, MT_RING, 10, 64*FRACUNIT, 64*FRACUNIT, mthing->angle);
		return;
	case 604: // Circle of rings (8 items)
	case 605: // Circle of rings (16 items)
	{
		INT32 numitems = (mthing->type & 1) ? 16 : 8;
		fixed_t size = (mthing->type & 1) ? 192*FRACUNIT : 96*FRACUNIT;
		mobjtype_t itemtypes[1] = { MT_RING };
		P_SpawnItemCircle(mthing, itemtypes, 1, numitems, size);
		return;
	}
	case 610: // Generic item row
	{
		mobjtype_t itemtypes[128]; //If you want to have a row with more than 128 different object types, you're crazy.
		UINT8 numitemtypes;
		if (!udmf)
			return;
		P_ParseItemTypes(mthing->stringargs[0], itemtypes, &numitemtypes);
		P_SpawnItemRow(mthing, itemtypes, numitemtypes, mthing->args[0], mthing->args[1] << FRACBITS, mthing->args[2] << FRACBITS, mthing->angle);
		return;
	}
	case 611: // Generic item circle
	{
		mobjtype_t itemtypes[128]; //If you want to have a circle with more than 128 different object types, you're crazy.
		UINT8 numitemtypes;
		if (!udmf)
			return;
		CONS_Printf("Itemstring: %s\n", mthing->stringargs[0]);
		P_ParseItemTypes(mthing->stringargs[0], itemtypes, &numitemtypes);
		P_SpawnItemCircle(mthing, itemtypes, numitemtypes, mthing->args[0], mthing->args[1] << FRACBITS);
		return;
	}
	default:
		return;
	}
}

//
// P_CheckMissileSpawn
// Moves the missile forward a bit and possibly explodes it right there.
//
boolean P_CheckMissileSpawn(mobj_t *th)
{
	// move a little forward so an angle can be computed if it immediately explodes
	if (!(th->flags & MF_GRENADEBOUNCE)) // hack: bad! should be a flag.
	{
		th->x += th->momx>>1;
		th->y += th->momy>>1;
		th->z += th->momz>>1;
	}

	if (!P_TryMove(th, th->x, th->y, true))
	{
		P_ExplodeMissile(th);
		return false;
	}
	return true;
}

//
// P_SpawnXYZMissile
//
// Spawns missile at specific coords
//
mobj_t *P_SpawnXYZMissile(mobj_t *source, mobj_t *dest, mobjtype_t type,
	fixed_t x, fixed_t y, fixed_t z)
{
	mobj_t *th;
	angle_t an;
	INT32 dist;
	fixed_t speed;

	I_Assert(source != NULL);
	I_Assert(dest != NULL);

	if (source->eflags & MFE_VERTICALFLIP)
		z -= FixedMul(mobjinfo[type].height, source->scale);

	th = P_SpawnMobj(x, y, z, type);

	if (source->eflags & MFE_VERTICALFLIP)
		th->flags2 |= MF2_OBJECTFLIP;

	th->destscale = source->scale;
	P_SetScale(th, source->scale);

	speed = FixedMul(th->info->speed, th->scale);

	if (speed == 0)
	{
		CONS_Debug(DBG_GAMELOGIC, "P_SpawnXYZMissile - projectile has 0 speed! (mobj type %d)\n", type);
		speed = mobjinfo[MT_ROCKET].speed;
	}

	if (th->info->seesound)
		S_StartSound(th, th->info->seesound);

	P_SetTarget(&th->target, source); // where it came from
	an = R_PointToAngle2(x, y, dest->x, dest->y);

	P_InitAngle(th, an);
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(speed, FINECOSINE(an));
	th->momy = FixedMul(speed, FINESINE(an));

	dist = P_AproxDistance(dest->x - x, dest->y - y);
	dist = dist / speed;

	if (dist < 1)
		dist = 1;

	th->momz = (dest->z - z) / dist;

	if (th->flags & MF_MISSILE)
		dist = P_CheckMissileSpawn(th);
	else
		dist = 1;

	return dist ? th : NULL;
}

//
// P_SpawnAlteredDirectionMissile
//
// Spawns a missile with same source as spawning missile yet an altered direction
//
mobj_t *P_SpawnAlteredDirectionMissile(mobj_t *source, mobjtype_t type, fixed_t x, fixed_t y, fixed_t z, INT32 shiftingAngle)
{
	mobj_t *th;
	angle_t an;
	fixed_t dist, speed;

	I_Assert(source != NULL);

	if (!(source->target) || !(source->flags & MF_MISSILE))
		return NULL;

	if (source->eflags & MFE_VERTICALFLIP)
		z -= FixedMul(mobjinfo[type].height, source->scale);

	th = P_SpawnMobj(x, y, z, type);

	if (source->eflags & MFE_VERTICALFLIP)
		th->flags2 |= MF2_OBJECTFLIP;

	th->destscale = source->scale;
	P_SetScale(th, source->scale);

	speed = FixedMul(th->info->speed, th->scale);

	if (speed == 0) // Backwards compatibility with 1.09.2
	{
		CONS_Printf("P_SpawnAlteredDirectionMissile - projectile has 0 speed! (mobj type %d)\nPlease update this SOC.", type);
		speed = mobjinfo[MT_ROCKET].speed;
	}

	if (th->info->seesound)
		S_StartSound(th, th->info->seesound);

	P_SetTarget(&th->target, source->target); // where it came from
	an = R_PointToAngle2(0, 0, source->momx, source->momy) + (ANG1*shiftingAngle);

	P_InitAngle(th, an);
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(speed, FINECOSINE(an));
	th->momy = FixedMul(speed, FINESINE(an));

	dist = P_AproxDistance(source->momx*800, source->momy*800);
	dist = dist / speed;

	if (dist < 1)
		dist = 1;

	th->momz = (source->momz*800) / dist;

	if (th->flags & MF_MISSILE)
	{
		dist = P_CheckMissileSpawn(th);
		th->x -= th->momx>>1;
		th->y -= th->momy>>1;
		th->z -= th->momz>>1;
	}
	else
		dist = 1;

	return dist ? th : NULL;
}

//
// P_SpawnPointMissile
//
// Spawns a missile at specific coords
//
mobj_t *P_SpawnPointMissile(mobj_t *source, fixed_t xa, fixed_t ya, fixed_t za, mobjtype_t type,
	fixed_t x, fixed_t y, fixed_t z)
{
	mobj_t *th;
	angle_t an;
	fixed_t dist, speed;

	I_Assert(source != NULL);

	if (source->eflags & MFE_VERTICALFLIP)
		z -= FixedMul(mobjinfo[type].height, source->scale);

	th = P_SpawnMobj(x, y, z, type);

	if (source->eflags & MFE_VERTICALFLIP)
		th->flags2 |= MF2_OBJECTFLIP;

	th->destscale = source->scale;
	P_SetScale(th, source->scale);

	speed = FixedMul(th->info->speed, th->scale);

	if (speed == 0) // Backwards compatibility with 1.09.2
	{
		CONS_Printf("P_SpawnPointMissile - projectile has 0 speed! (mobj type %d)\nPlease update this SOC.", type);
		speed = mobjinfo[MT_ROCKET].speed;
	}

	if (th->info->seesound)
		S_StartSound(th, th->info->seesound);

	P_SetTarget(&th->target, source); // where it came from
	an = R_PointToAngle2(x, y, xa, ya);

	P_InitAngle(th, an);
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(speed, FINECOSINE(an));
	th->momy = FixedMul(speed, FINESINE(an));

	dist = P_AproxDistance(xa - x, ya - y);
	dist = dist / speed;

	if (dist < 1)
		dist = 1;

	th->momz = (za - z) / dist;

	if (th->flags & MF_MISSILE)
		dist = P_CheckMissileSpawn(th);
	else
		dist = 1;

	return dist ? th : NULL;
}

//
// P_SpawnMissile
//
mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	mobj_t *th;
	angle_t an;
	INT32 dist;
	fixed_t z;
	const fixed_t gsf = (fixed_t)6;
	fixed_t speed;

	I_Assert(source != NULL);
	I_Assert(dest != NULL);
	if (source->type == MT_JETTGUNNER)
	{
		if (source->eflags & MFE_VERTICALFLIP)
			z = source->z + source->height - FixedMul(4*FRACUNIT, source->scale);
		else
			z = source->z + FixedMul(4*FRACUNIT, source->scale);
	}
	else
		z = source->z + source->height/2;

	if (source->eflags & MFE_VERTICALFLIP)
		z -= FixedMul(mobjinfo[type].height, source->scale);

	th = P_SpawnMobj(source->x, source->y, z, type);

	if (source->eflags & MFE_VERTICALFLIP)
		th->flags2 |= MF2_OBJECTFLIP;

	th->destscale = source->scale;
	P_SetScale(th, source->scale);

	if (source->type == MT_METALSONIC_BATTLE && source->health < 4)
		speed = FixedMul(FixedMul(th->info->speed, 3*FRACUNIT/2), th->scale);
	else
		speed = FixedMul(th->info->speed, th->scale);

	if (speed == 0)
	{
		CONS_Debug(DBG_GAMELOGIC, "P_SpawnMissile - projectile has 0 speed! (mobj type %d)\n", type);
		speed = FixedMul(mobjinfo[MT_TURRETLASER].speed, th->scale);
	}

	if (th->info->seesound)
		S_StartSound(source, th->info->seesound);

	P_SetTarget(&th->target, source); // where it came from

	if (type == MT_TURRETLASER || type == MT_ENERGYBALL) // More accurate!
		an = R_PointToAngle2(source->x, source->y,
			dest->x + (dest->momx*gsf),
			dest->y + (dest->momy*gsf));
	else
		an = R_PointToAngle2(source->x, source->y, dest->x, dest->y);

	P_InitAngle(th, an);
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul(speed, FINECOSINE(an));
	th->momy = FixedMul(speed, FINESINE(an));

	if (type == MT_TURRETLASER || type == MT_ENERGYBALL) // More accurate!
		dist = P_AproxDistance(dest->x+(dest->momx*gsf) - source->x, dest->y+(dest->momy*gsf) - source->y);
	else
		dist = P_AproxDistance(dest->x - source->x, dest->y - source->y);

	dist = dist / speed;

	if (dist < 1)
		dist = 1;

	if (type == MT_TURRETLASER || type == MT_ENERGYBALL) // More accurate!
		th->momz = (dest->z + (dest->momz*gsf) - z) / dist;
	else
		th->momz = (dest->z - z) / dist;

	if (th->flags & MF_MISSILE)
		dist = P_CheckMissileSpawn(th);
	else
		dist = 1;
	return dist ? th : NULL;
}

//
// P_ColorTeamMissile
// Colors a player's ring based on their team
//
void P_ColorTeamMissile(mobj_t *missile, player_t *source)
{
	if (G_GametypeHasTeams())
	{
		if (source->ctfteam == 2)
			missile->color = skincolor_bluering;
		else if (source->ctfteam == 1)
			missile->color = skincolor_redring;
	}
	/*
	else
		missile->color = player->mo->color; //copy color
	*/
}

//
// P_SPMAngle
// Fires missile at angle from what is presumably a player
//
mobj_t *P_SPMAngle(mobj_t *source, mobjtype_t type, angle_t angle, UINT8 allowaim, UINT32 flags2)
{
	mobj_t *th;
	angle_t an;
	fixed_t x, y, z, slope = 0, speed;

	// angle at which you fire, is player angle
	an = angle;

	if (allowaim) // aiming allowed?
		slope = AIMINGTOSLOPE(source->player->aiming);

	x = source->x;
	y = source->y;

	if (source->eflags & MFE_VERTICALFLIP)
		z = source->z + 2*source->height/3 - FixedMul(mobjinfo[type].height, source->scale);
	else
		z = source->z + source->height/3;

	th = P_SpawnMobj(x, y, z, type);

	if (source->eflags & MFE_VERTICALFLIP)
		th->flags2 |= MF2_OBJECTFLIP;

	th->destscale = source->scale;
	P_SetScale(th, source->scale);

	th->flags2 |= flags2;

	// The rail ring has no unique thrown object, so we must do this.
	if (th->info->seesound && !(th->flags2 & MF2_RAILRING))
		S_StartSound(source, th->info->seesound);

	P_SetTarget(&th->target, source);

	speed = th->info->speed;

	P_InitAngle(th, an);
	th->momx = FixedMul(speed, FINECOSINE(an>>ANGLETOFINESHIFT));
	th->momy = FixedMul(speed, FINESINE(an>>ANGLETOFINESHIFT));

	if (allowaim)
	{
		th->momx = FixedMul(th->momx,FINECOSINE(source->player->aiming>>ANGLETOFINESHIFT));
		th->momy = FixedMul(th->momy,FINECOSINE(source->player->aiming>>ANGLETOFINESHIFT));
	}

	th->momz = FixedMul(speed, slope);

	//scaling done here so it doesn't clutter up the code above
	th->momx = FixedMul(th->momx, th->scale);
	th->momy = FixedMul(th->momy, th->scale);
	th->momz = FixedMul(th->momz, th->scale);

	slope = P_CheckMissileSpawn(th);

	return slope ? th : NULL;
}

//
// P_FlashPal
// Flashes a player's palette.  ARMAGEDDON BLASTS!
//
void P_FlashPal(player_t *pl, UINT16 type, UINT16 duration)
{
	if (!pl)
		return;
	pl->flashcount = duration;
	pl->flashpal = type;
}

//
// P_ScaleFromMap
// Scales a number relative to the mapobjectscale.
//
fixed_t P_ScaleFromMap(fixed_t n, fixed_t scale)
{
	return FixedMul(n, FixedDiv(scale, mapobjectscale));
}

//
// P_SpawnMobjFromMobjUnscaled
// Spawns an object with offsets relative to the position of another object.
// Scale, gravity flip, etc. is taken into account automatically.
//
mobj_t *P_SpawnMobjFromMobjUnscaled(mobj_t *mobj, fixed_t xofs, fixed_t yofs, fixed_t zofs, mobjtype_t type)
{
	mobj_t *newmobj;

	newmobj = P_SpawnMobj(mobj->x + xofs, mobj->y + yofs, mobj->z + zofs, type);
	if (!newmobj)
		return NULL;

	newmobj->destscale = P_ScaleFromMap(mobj->destscale, newmobj->destscale);
	P_SetScale(newmobj, P_ScaleFromMap(mobj->scale, newmobj->scale));

	if (mobj->eflags & MFE_VERTICALFLIP)
	{
		newmobj->eflags |= MFE_VERTICALFLIP;
		newmobj->flags2 |= MF2_OBJECTFLIP;
		newmobj->z = mobj->z + mobj->height - zofs - newmobj->height;

		newmobj->old_z = mobj->old_z + mobj->height - zofs - newmobj->height;
		newmobj->old_z2 = mobj->old_z2 + mobj->height - zofs - newmobj->height;
	}
	else
	{
		newmobj->old_z = mobj->old_z + zofs;
		newmobj->old_z2 = mobj->old_z2 + zofs;
	}

	newmobj->old_x2 = mobj->old_x2 + xofs;
	newmobj->old_y2 = mobj->old_y2 + yofs;
	newmobj->old_x = mobj->old_x + xofs;
	newmobj->old_y = mobj->old_y + yofs;

	// This angle hack is needed for Lua scripts that set the angle after
	// spawning, to avoid erroneous interpolation.
	if (mobj->player)
	{
		newmobj->old_angle2 = mobj->player->old_drawangle2;
		newmobj->old_angle = mobj->player->old_drawangle;
	}
	else
	{
		newmobj->old_angle2 = mobj->old_angle2;
		newmobj->old_angle = mobj->old_angle;
	}

	newmobj->old_scale2 = mobj->old_scale2;
	newmobj->old_scale = mobj->old_scale;
	newmobj->old_spritexscale = mobj->old_spritexscale;
	newmobj->old_spriteyscale = mobj->old_spriteyscale;
	newmobj->old_spritexoffset = mobj->old_spritexoffset;
	newmobj->old_spriteyoffset = mobj->old_spriteyoffset;

	return newmobj;
}

//
// P_SpawnMobjFromMobj
// Spawns an object with offsets relative to the position of another object.
// Scale, gravity flip, etc. is taken into account automatically.
//
mobj_t *P_SpawnMobjFromMobj(mobj_t *mobj, fixed_t xofs, fixed_t yofs, fixed_t zofs, mobjtype_t type)
{
	mobj_t *newmobj;

	xofs = FixedMul(xofs, mobj->scale);
	yofs = FixedMul(yofs, mobj->scale);
	zofs = FixedMul(zofs, mobj->scale);

	newmobj = P_SpawnMobj(mobj->x + xofs, mobj->y + yofs, mobj->z + zofs, type);
	if (!newmobj)
		return NULL;

	newmobj->destscale = P_ScaleFromMap(mobj->destscale, newmobj->destscale);
	P_SetScale(newmobj, P_ScaleFromMap(mobj->scale, newmobj->scale));

	if (mobj->eflags & MFE_VERTICALFLIP)
	{
		newmobj->eflags |= MFE_VERTICALFLIP;
		newmobj->flags2 |= MF2_OBJECTFLIP;
		newmobj->z = mobj->z + mobj->height - zofs - newmobj->height;

		newmobj->old_z = mobj->old_z + mobj->height - zofs - newmobj->height;
		newmobj->old_z2 = mobj->old_z2 + mobj->height - zofs - newmobj->height;
	}
	else
	{
		newmobj->old_z = mobj->old_z + zofs;
		newmobj->old_z2 = mobj->old_z2 + zofs;
	}

	newmobj->old_x2 = mobj->old_x2 + xofs;
	newmobj->old_y2 = mobj->old_y2 + yofs;
	newmobj->old_x = mobj->old_x + xofs;
	newmobj->old_y = mobj->old_y + yofs;

	// This angle hack is needed for Lua scripts that set the angle after
	// spawning, to avoid erroneous interpolation.
	if (mobj->player)
	{
		newmobj->old_angle2 = mobj->player->old_drawangle2;
		newmobj->old_angle = mobj->player->old_drawangle;
	}
	else
	{
		newmobj->old_angle2 = mobj->old_angle2;
		newmobj->old_angle = mobj->old_angle;
	}

	newmobj->old_scale2 = mobj->old_scale2;
	newmobj->old_scale = mobj->old_scale;
	newmobj->old_spritexscale = mobj->old_spritexscale;
	newmobj->old_spriteyscale = mobj->old_spriteyscale;
	newmobj->old_spritexoffset = mobj->old_spritexoffset;
	newmobj->old_spriteyoffset = mobj->old_spriteyoffset;

	return newmobj;
}

//
// P_GetMobjHead & P_GetMobjFeet
// Returns the top and bottom of an object, follows appearance, not physics,
// in reverse gravity.
//

fixed_t P_GetMobjHead(const mobj_t *mobj)
{
	return P_IsObjectFlipped(mobj) ? mobj->z : mobj->z + mobj->height;
}

fixed_t P_GetMobjFeet(const mobj_t *mobj)
{
	/*
	          |     |
	          |     |
	/--\------/     |
	|               |
	-----------------
	*/

	return P_IsObjectFlipped(mobj) ? mobj->z + mobj->height : mobj->z;
}

//
// P_GetMobjGround
// Returns the object's floor, or ceiling in reverse gravity.
//
fixed_t P_GetMobjGround(const mobj_t *mobj)
{
	return P_IsObjectFlipped(mobj) ? mobj->ceilingz : mobj->floorz;
}

//
// P_GetMobjZMovement
// Returns the Z momentum of the object, accounting for slopes if the object is grounded
//
fixed_t P_GetMobjZMovement(mobj_t *mo)
{
	pslope_t *slope = mo->standingslope;
	angle_t angDiff;
	fixed_t speed;

	if (!P_IsObjectOnGround(mo))
		return mo->momz;

	if (!slope)
		return 0;

	angDiff = R_PointToAngle2(0, 0, mo->momx, mo->momy) - slope->xydirection;
	speed = FixedHypot(mo->momx, mo->momy);

	return P_ReturnThrustY(mo, slope->zangle, P_ReturnThrustX(mo, angDiff, speed));
}

//
// Thing IDs / tags
//
// TODO: Replace this system with taglist_t instead.
// The issue is that those require a static numbered ID
// to determine which struct it belongs to, which mobjs
// don't really have.
//

#define TID_HASH_CHAINS (131)
static mobj_t *TID_Hash[TID_HASH_CHAINS];

//
// P_InitTIDHash
// Initializes mobj tag hash array
//
void P_InitTIDHash(void)
{
	memset(TID_Hash, 0, TID_HASH_CHAINS * sizeof(mobj_t *));
}

//
// P_SetThingTID
// Adds a mobj to the hash array
//
void P_SetThingTID(mobj_t *mo, mtag_t tid)
{
	INT32 key = 0;

	if (tid == 0)
	{
		if (mo->tid != 0)
		{
			P_RemoveThingTID(mo);
		}

		return;
	}

	mo->tid = tid;

	// Insert at the head of this chain
	key = tid % TID_HASH_CHAINS;

	mo->tid_next = TID_Hash[key];
	mo->tid_prev = &TID_Hash[key];
	TID_Hash[key] = mo;

	// Connect to any existing things in chain
	if (mo->tid_next != NULL)
	{
		mo->tid_next->tid_prev = &(mo->tid_next);
	}
}

//
// P_RemoveThingTID
// Removes a mobj from the hash array
//
void P_RemoveThingTID(mobj_t *mo)
{
	if (mo->tid != 0 && mo->tid_prev != NULL)
	{
		// Fix the gap this would leave.
		*(mo->tid_prev) = mo->tid_next;

		if (mo->tid_next != NULL)
		{
			mo->tid_next->tid_prev = mo->tid_prev;
		}
	}

	// Remove TID.
	mo->tid = 0;
}

//
// P_FindMobjFromTID
// Mobj tag search function.
//
mobj_t *P_FindMobjFromTID(mtag_t tid, mobj_t *i, mobj_t *activator)
{
	if (tid == 0)
	{
		// 0 grabs the activator, if applicable,
		// for some ACS functions.

		if (i != NULL)
		{
			// Don't do more than once.
			return NULL;
		}

		return activator;
	}

	i = (i != NULL) ? i->tid_next : TID_Hash[tid % TID_HASH_CHAINS];

	while (i != NULL && i->tid != tid)
	{
		i = i->tid_next;
	}

	return i;
}
