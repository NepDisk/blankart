// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2012-2016 by John "JTE" Muniz.
// Copyright (C) 2012-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_playerlib.c
/// \brief player object library for Lua scripting

#include "doomdef.h"
#include "fastcmp.h"
#include "p_mobj.h"
#include "d_player.h"
#include "g_game.h"
#include "p_local.h"
#include "d_clisrv.h"

#include "lua_script.h"
#include "lua_libs.h"
#include "lua_hud.h" // hud_running errors
#include "lua_hook.h" // hook_cmd_running errors

static int lib_iteratePlayers(lua_State *L)
{
	INT32 i = -1;

	if (lua_gettop(L) < 2)
	{
		//return luaL_error(L, "Don't call players.iterate() directly, use it as 'for player in players.iterate do <block> end'.");
		lua_pushcfunction(L, lib_iteratePlayers);
		return 1;
	}

	lua_settop(L, 2);
	lua_remove(L, 1); // state is unused.

	if (!lua_isnil(L, 1))
		i = (INT32)(*((player_t **)luaL_checkudata(L, 1, META_PLAYER)) - players);

	i++;

	if (i == serverplayer)
	{
		return LUA_PushServerPlayer(L);
	}

	for (; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		LUA_PushUserdata(L, &players[i], META_PLAYER);
		return 1;
	}

	return 0;
}

static int lib_getPlayer(lua_State *L)
{
	const char *field;
	// i -> players[i]
	if (lua_type(L, 2) == LUA_TNUMBER)
	{
		lua_Integer i = luaL_checkinteger(L, 2);
		if (i < 0 || i >= MAXPLAYERS)
			return luaL_error(L, "players[] index %d out of range (0 - %d)", i, MAXPLAYERS-1);
		if (i == serverplayer)
			return LUA_PushServerPlayer(L);
		if (!playeringame[i])
			return 0;
		LUA_PushUserdata(L, &players[i], META_PLAYER);
		return 1;
	}

	field = luaL_checkstring(L, 2);
	if (fastcmp(field,"iterate"))
	{
		lua_pushcfunction(L, lib_iteratePlayers);
		return 1;
	}
	return 0;
}

// #players -> MAXPLAYERS
static int lib_lenPlayer(lua_State *L)
{
	lua_pushinteger(L, MAXPLAYERS);
	return 1;
}

// Same deal as the three functions above but for displayplayers

static int lib_iterateDisplayplayers(lua_State *L)
{
	INT32 i = -1;
	INT32 temp = -1;
	INT32 iter = 0;

	if (lua_gettop(L) < 2)
	{
		//return luaL_error(L, "Don't call displayplayers.iterate() directly, use it as 'for player in displayplayers.iterate do <block> end'.");
		lua_pushcfunction(L, lib_iterateDisplayplayers);
		return 1;
	}
	lua_settop(L, 2);
	lua_remove(L, 1); // state is unused.
	if (!lua_isnil(L, 1))
	{
		temp = (INT32)(*((player_t **)luaL_checkudata(L, 1, META_PLAYER)) - players);	// get the player # of the last iterated player.

		// @FIXME:
		// I didn't quite find a better way for this; Here, we go back to which player in displayplayers we last iterated to resume the for loop below for this new function call
		// I don't understand enough about how the Lua stacks work to get this to work in possibly a single line.
		// So anyone feel free to correct this!

		for (; iter < MAXSPLITSCREENPLAYERS; iter++)
		{
			if (displayplayers[iter] == temp)
			{
				i = iter;
				break;
			}
		}
	}

	for (i++; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (i > r_splitscreen || !playeringame[displayplayers[i]])
			return 0;	// Stop! There are no more players for us to go through. There will never be a player gap in displayplayers.

		LUA_PushUserdata(L, &players[displayplayers[i]], META_PLAYER);
		lua_pushinteger(L, i);	// push this to recall what number we were on for the next function call. I suppose this also means you can retrieve the splitscreen player number with 'for p, n in displayplayers.iterate'!
		return 2;
	}
	return 0;
}

static int lib_getDisplayplayers(lua_State *L)
{
	const char *field;
	// i -> players[i]
	if (lua_type(L, 2) == LUA_TNUMBER)
	{
		lua_Integer i = luaL_checkinteger(L, 2);
		if (i < 0 || i >= MAXSPLITSCREENPLAYERS)
			return luaL_error(L, "displayplayers[] index %d out of range (0 - %d)", i, MAXSPLITSCREENPLAYERS-1);
		if (i > r_splitscreen)
			return 0;
		if (!playeringame[displayplayers[i]])
			return 0;
		LUA_PushUserdata(L, &players[displayplayers[i]], META_PLAYER);
		return 1;
	}

	field = luaL_checkstring(L, 2);
	if (fastcmp(field,"iterate"))
	{
		lua_pushcfunction(L, lib_iterateDisplayplayers);
		return 1;
	}
	return 0;
}

// #displayplayers -> MAXSPLITSCREENPLAYERS
static int lib_lenDisplayplayers(lua_State *L)
{
	lua_pushinteger(L, MAXSPLITSCREENPLAYERS);
	return 1;
}

static int player_get(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	const char *field = luaL_checkstring(L, 2);

	if (!plr) {
		if (fastcmp(field,"valid")) {
			lua_pushboolean(L, false);
			return 1;
		}
		return LUA_ErrInvalid(L, "player_t");
	}

	if (fastcmp(field,"valid"))
		lua_pushboolean(L, true);
	else if (fastcmp(field,"name"))
		lua_pushstring(L, player_names[plr-players]);
	else if (fastcmp(field,"mo"))
		LUA_PushUserdata(L, plr->mo, META_MOBJ);
	else if (fastcmp(field,"cmd"))
		LUA_PushUserdata(L, &plr->cmd, META_TICCMD);
	else if (fastcmp(field,"oldcmd"))
		LUA_PushUserdata(L, &plr->oldcmd, META_TICCMD);
	else if (fastcmp(field,"respawn"))
		LUA_PushUserdata(L, &plr->respawn, META_RESPAWN);
	else if (fastcmp(field,"playerstate"))
		lua_pushinteger(L, plr->playerstate);
	else if (fastcmp(field,"viewz"))
		lua_pushfixed(L, plr->viewz);
	else if (fastcmp(field,"viewheight"))
		lua_pushfixed(L, plr->viewheight);
	else if (fastcmp(field,"viewrollangle"))
		lua_pushangle(L, plr->viewrollangle);
	else if (fastcmp(field,"aiming"))
		lua_pushangle(L, plr->aiming);
	else if (fastcmp(field,"drawangle"))
		lua_pushangle(L, plr->drawangle);
	else if (fastcmp(field,"karthud"))
		LUA_PushUserdata(L, plr->karthud, META_KARTHUD);
	else if (fastcmp(field,"nocontrol"))
		lua_pushinteger(L, plr->nocontrol);
	else if (fastcmp(field,"carry"))
		lua_pushinteger(L, plr->carry);
	else if (fastcmp(field,"dye"))
		lua_pushinteger(L, plr->dye);
	else if (fastcmp(field,"position"))
		lua_pushinteger(L, plr->position);
	else if (fastcmp(field,"oldposition"))
		lua_pushinteger(L, plr->oldposition);
	else if (fastcmp(field,"positiondelay"))
		lua_pushinteger(L, plr->positiondelay);
	else if (fastcmp(field,"distancetofinish"))
		lua_pushinteger(L, plr->distancetofinish);
	else if (fastcmp(field,"airtime"))
		lua_pushinteger(L, plr->airtime);
	else if (fastcmp(field,"flashing"))
		lua_pushinteger(L, plr->flashing);
	else if (fastcmp(field,"spinouttimer"))
		lua_pushinteger(L, plr->spinouttimer);
	else if (fastcmp(field,"instashield"))
		lua_pushinteger(L, plr->instashield);
	else if (fastcmp(field,"wipeoutslow"))
		lua_pushinteger(L, plr->wipeoutslow);
	else if (fastcmp(field,"justbumped"))
		lua_pushinteger(L, plr->justbumped);
	else if (fastcmp(field,"justDI"))
		lua_pushinteger(L, plr->justDI);
	else if (fastcmp(field,"flipDI"))
		lua_pushboolean(L, plr->flipDI);
	else if (fastcmp(field,"drift"))
		lua_pushinteger(L, plr->drift);
	else if (fastcmp(field,"driftcharge"))
		lua_pushinteger(L, plr->driftcharge);
	else if (fastcmp(field,"driftboost"))
		lua_pushinteger(L, plr->driftboost);
	else if (fastcmp(field,"aizdriftstraft"))
		lua_pushinteger(L, plr->aizdriftstrat);
	else if (fastcmp(field,"aizdrifttilt"))
		lua_pushinteger(L, plr->aizdrifttilt);
	else if (fastcmp(field,"aizdriftturn"))
		lua_pushinteger(L, plr->aizdriftturn);
	else if (fastcmp(field,"offroad"))
		lua_pushinteger(L, plr->offroad);
	else if (fastcmp(field,"waterskip"))
		lua_pushinteger(L, plr->waterskip);
	else if (fastcmp(field,"springstars"))
		lua_pushinteger(L, plr->springstars);
	else if (fastcmp(field,"springcolor"))
		lua_pushinteger(L, plr->springcolor);
	else if (fastcmp(field,"dashpadcooldown"))
		lua_pushinteger(L, plr->dashpadcooldown);
	else if (fastcmp(field,"boostpower"))
		lua_pushinteger(L, plr->boostpower);
	else if (fastcmp(field,"speedboost"))
		lua_pushinteger(L, plr->speedboost);
	else if (fastcmp(field,"accelboost"))
		lua_pushinteger(L, plr->accelboost);
	else if (fastcmp(field,"handleboost"))
		lua_pushinteger(L, plr->handleboost);
	else if (fastcmp(field,"boostangle"))
		lua_pushangle(L, plr->boostangle);
	else if (fastcmp(field,"tripwireState"))
		lua_pushinteger(L, plr->tripwireState);
	else if (fastcmp(field,"tripwirePass"))
		lua_pushinteger(L, plr->tripwirePass);
	else if (fastcmp(field,"tripwireLeniency"))
		lua_pushinteger(L, plr->tripwireLeniency);
	else if (fastcmp(field,"itemroulette"))
		lua_pushinteger(L, plr->itemroulette);
	else if (fastcmp(field,"roulettetype"))
		lua_pushinteger(L, plr->roulettetype);
	else if (fastcmp(field,"itemtype"))
		lua_pushinteger(L, plr->itemtype);
	else if (fastcmp(field,"itemamount"))
		lua_pushinteger(L, plr->itemamount);
	else if (fastcmp(field,"throwdir"))
		lua_pushinteger(L, plr->throwdir);
	else if (fastcmp(field,"sadtimer"))
		lua_pushinteger(L, plr->sadtimer);
	else if (fastcmp(field,"rings"))
		lua_pushinteger(L, plr->rings);
	else if (fastcmp(field,"pickuprings"))
		lua_pushinteger(L, plr->pickuprings);
	else if (fastcmp(field,"ringdelay"))
		lua_pushinteger(L, plr->ringdelay);
	else if (fastcmp(field,"ringboost"))
		lua_pushinteger(L, plr->ringboost);
	else if (fastcmp(field,"sparkleanim"))
		lua_pushinteger(L, plr->sparkleanim);
	else if (fastcmp(field,"superring"))
		lua_pushinteger(L, plr->superring);
	else if (fastcmp(field,"curshield"))
		lua_pushinteger(L, plr->curshield);
	else if (fastcmp(field,"bubblecool"))
		lua_pushinteger(L, plr->bubblecool);
	else if (fastcmp(field,"bubbleblowup"))
		lua_pushinteger(L, plr->bubbleblowup);
	else if (fastcmp(field,"flamedash"))
		lua_pushinteger(L, plr->flamedash);
	else if (fastcmp(field,"flamemeter"))
		lua_pushinteger(L, plr->flamemeter);
	else if (fastcmp(field,"flamelength"))
		lua_pushinteger(L, plr->flamelength);
	else if (fastcmp(field,"hyudorotimer"))
		lua_pushinteger(L, plr->hyudorotimer);
	else if (fastcmp(field,"stealingtimer"))
		lua_pushinteger(L, plr->stealingtimer);
	else if (fastcmp(field,"sneakertimer"))
		lua_pushinteger(L, plr->sneakertimer);
	else if (fastcmp(field,"floorboost"))
		lua_pushinteger(L, plr->floorboost);
	else if (fastcmp(field,"growshrinktimer"))
		lua_pushinteger(L, plr->growshrinktimer);
	else if (fastcmp(field,"rocketsneakertimer"))
		lua_pushinteger(L, plr->rocketsneakertimer);
	else if (fastcmp(field,"invincibilitytimer"))
		lua_pushinteger(L, plr->invincibilitytimer);
	else if (fastcmp(field,"eggmanexplode"))
		lua_pushinteger(L, plr->eggmanexplode);
	else if (fastcmp(field,"eggmanblame"))
		lua_pushinteger(L, plr->eggmanblame);
	else if (fastcmp(field,"bananadrag"))
		lua_pushinteger(L, plr->bananadrag);
	else if (fastcmp(field,"lastjawztarget"))
		lua_pushinteger(L, plr->lastjawztarget);
	else if (fastcmp(field,"jawztargetdelay"))
		lua_pushinteger(L, plr->jawztargetdelay);
	else if (fastcmp(field,"confirmVictim"))
		lua_pushinteger(L, plr->confirmVictim);
	else if (fastcmp(field,"confirmVictimDelay"))
		lua_pushinteger(L, plr->confirmVictimDelay);
	else if (fastcmp(field,"glanceDir"))
		lua_pushinteger(L, plr->glanceDir);
	else if (fastcmp(field,"roundscore"))
		plr->roundscore = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"emeralds"))
		lua_pushinteger(L, plr->emeralds);
	else if (fastcmp(field,"bumpers"))
		lua_pushinteger(L, plr->bumpers);
	else if (fastcmp(field,"karmadelay"))
		lua_pushinteger(L, plr->karmadelay);
	else if (fastcmp(field,"spheres"))
		lua_pushinteger(L, plr->spheres);
	else if (fastcmp(field,"spheredigestion"))
		lua_pushinteger(L, plr->spheredigestion);
	else if (fastcmp(field,"pflags"))
		lua_pushinteger(L, plr->pflags);
	else if (fastcmp(field,"panim"))
		lua_pushinteger(L, plr->panim);
	else if (fastcmp(field,"flashcount"))
		lua_pushinteger(L, plr->flashcount);
	else if (fastcmp(field,"flashpal"))
		lua_pushinteger(L, plr->flashpal);
	else if (fastcmp(field,"skincolor"))
		lua_pushinteger(L, plr->skincolor);
	else if (fastcmp(field,"skin"))
		lua_pushinteger(L, plr->skin);
	else if (fastcmp(field,"availabilities"))
		lua_pushinteger(L, plr->availabilities);
	else if (fastcmp(field,"score"))
		lua_pushinteger(L, plr->score);
	// SRB2kart
	else if (fastcmp(field,"kartspeed"))
		lua_pushinteger(L, plr->kartspeed);
	else if (fastcmp(field,"kartweight"))
		lua_pushinteger(L, plr->kartweight);
	else if (fastcmp(field,"followerskin"))
		lua_pushinteger(L, plr->followerskin);
	else if (fastcmp(field,"followerready"))
		lua_pushboolean(L, plr->followerready);
	else if (fastcmp(field,"followercolor"))
		lua_pushinteger(L, plr->followercolor);
	else if (fastcmp(field,"follower"))
		LUA_PushUserdata(L, plr->follower, META_MOBJ);
	//
	else if (fastcmp(field,"charflags"))
		lua_pushinteger(L, plr->charflags);
	else if (fastcmp(field,"followitem"))
		lua_pushinteger(L, plr->followitem);
	else if (fastcmp(field,"followmobj"))
		LUA_PushUserdata(L, plr->followmobj, META_MOBJ);
	else if (fastcmp(field,"lives"))
		lua_pushinteger(L, plr->lives);
	else if (fastcmp(field,"xtralife"))
		lua_pushinteger(L, plr->xtralife);
	else if (fastcmp(field,"speed"))
		lua_pushfixed(L, plr->speed);
	else if (fastcmp(field,"lastspeed"))
		lua_pushfixed(L, plr->lastspeed);
	else if (fastcmp(field,"deadtimer"))
		lua_pushinteger(L, plr->deadtimer);
	else if (fastcmp(field,"exiting"))
		lua_pushinteger(L, plr->exiting);
	else if (fastcmp(field,"cmomx"))
		lua_pushfixed(L, plr->cmomx);
	else if (fastcmp(field,"cmomy"))
		lua_pushfixed(L, plr->cmomy);
	else if (fastcmp(field,"rmomx"))
		lua_pushfixed(L, plr->rmomx);
	else if (fastcmp(field,"rmomy"))
		lua_pushfixed(L, plr->rmomy);
	else if (fastcmp(field,"totalring"))
		lua_pushinteger(L, plr->totalring);
	else if (fastcmp(field,"realtime"))
		lua_pushinteger(L, plr->realtime);
	else if (fastcmp(field,"laps"))
		lua_pushinteger(L, plr->laps);
	else if (fastcmp(field,"latestlap"))
		lua_pushinteger(L, plr->latestlap);
	else if (fastcmp(field,"ctfteam"))
		lua_pushinteger(L, plr->ctfteam);
	else if (fastcmp(field,"checkskip"))
		lua_pushinteger(L, plr->checkskip);
	else if (fastcmp(field,"starpostnum"))
		lua_pushinteger(L, plr->starpostnum);
	else if (fastcmp(field,"lastsidehit"))
		lua_pushinteger(L, plr->lastsidehit);
	else if (fastcmp(field,"lastlinehit"))
		lua_pushinteger(L, plr->lastlinehit);
	else if (fastcmp(field,"onconveyor"))
		lua_pushinteger(L, plr->onconveyor);
	else if (fastcmp(field,"awayviewmobj"))
		LUA_PushUserdata(L, plr->awayviewmobj, META_MOBJ);
	else if (fastcmp(field,"awayviewtics"))
		lua_pushinteger(L, plr->awayviewtics);
	else if (fastcmp(field,"awayviewaiming"))
		lua_pushangle(L, plr->awayviewaiming);

	else if (fastcmp(field,"spectator"))
		lua_pushboolean(L, plr->spectator);
	else if (fastcmp(field,"bot"))
		lua_pushboolean(L, plr->bot);
	else if (fastcmp(field,"jointime"))
		lua_pushinteger(L, plr->jointime);
	else if (fastcmp(field,"splitscreenindex"))
		lua_pushinteger(L, plr->splitscreenindex);
#ifdef HWRENDER
	else if (fastcmp(field,"fovadd"))
		lua_pushfixed(L, plr->fovadd);
#endif
	else if (fastcmp(field,"ping"))
		lua_pushinteger(L, playerpingtable[( plr - players )]);
	else {
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
		I_Assert(lua_istable(L, -1));
		lua_pushlightuserdata(L, plr);
		lua_rawget(L, -2);
		if (!lua_istable(L, -1)) { // no extra values table
			CONS_Debug(DBG_LUA, M_GetText("'%s' has no extvars table or field named '%s'; returning nil.\n"), "player_t", field);
			return 0;
		}
		lua_getfield(L, -1, field);
		if (lua_isnil(L, -1)) // no value for this field
			CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; returning nil.\n"), "player_t", field);
	}

	return 1;
}

#define NOSET luaL_error(L, LUA_QL("player_t") " field " LUA_QS " should not be set directly.", field)
static int player_set(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	const char *field = luaL_checkstring(L, 2);
	if (!plr)
		return LUA_ErrInvalid(L, "player_t");

	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");
	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in CMD building code!");

	if (fastcmp(field,"mo")) {
		mobj_t *newmo = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
		plr->mo->player = NULL; // remove player pointer from old mobj
		(newmo->player = plr)->mo = newmo; // set player pointer for new mobj, and set new mobj as the player's mobj
	}
	else if (fastcmp(field,"cmd"))
		return NOSET;
	else if (fastcmp(field,"oldcmd"))
		return NOSET;
	else if (fastcmp(field,"respawn"))
		return NOSET;
	else if (fastcmp(field,"playerstate"))
		plr->playerstate = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"viewz"))
		plr->viewz = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"viewheight"))
		plr->viewheight = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"viewrollangle"))
		plr->viewrollangle = luaL_checkangle(L, 3);
	else if (fastcmp(field,"aiming")) {
		UINT8 i;
		plr->aiming = luaL_checkangle(L, 3);
		for (i = 0; i <= r_splitscreen; i++)
		{
			if (plr == &players[displayplayers[i]])
			{
				localaiming[i] = plr->aiming;
				break;
			}
		}
	}
	else if (fastcmp(field,"drawangle"))
		plr->drawangle = luaL_checkangle(L, 3);
	else if (fastcmp(field,"pflags"))
		plr->pflags = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"panim"))
		plr->panim = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flashcount"))
		plr->flashcount = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flashpal"))
		plr->flashpal = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"skincolor"))
	{
		UINT16 newcolor = luaL_checkinteger(L,3);
		if (newcolor >= numskincolors)
			return luaL_error(L, "player.skincolor %d out of range (0 - %d).", newcolor, numskincolors-1);
		plr->skincolor = newcolor;
	}
	else if (fastcmp(field,"skin"))
		return NOSET;
	else if (fastcmp(field,"availabilities"))
		return NOSET;
	else if (fastcmp(field,"score"))
		plr->score = luaL_checkinteger(L, 3);
	// SRB2kart
	else if (fastcmp(field,"nocontrol"))
		plr->nocontrol = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"carry"))
		plr->carry = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"dye"))
		plr->dye = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"position"))
		plr->position = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"oldposition"))
		plr->oldposition = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"positiondelay"))
		plr->positiondelay = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"distancetofinish"))
		return NOSET;
	else if (fastcmp(field,"airtime"))
		plr->airtime = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flashing"))
		plr->flashing = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"spinouttimer"))
		plr->spinouttimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"instashield"))
		plr->instashield = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"wipeoutslow"))
		plr->wipeoutslow = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"justbumped"))
		plr->justbumped = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"justDI"))
		plr->justDI = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flipDI"))
		plr->flipDI = luaL_checkboolean(L, 3);
	else if (fastcmp(field,"drift"))
		plr->drift = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"driftcharge"))
		plr->driftcharge = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"driftboost"))
		plr->driftboost = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"aizdriftstraft"))
		plr->aizdriftstrat = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"aizdrifttilt"))
		plr->aizdrifttilt = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"aizdriftturn"))
		plr->aizdriftturn = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"offroad"))
		plr->offroad = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"waterskip"))
		plr->waterskip = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"springstars"))
		plr->springstars = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"springcolor"))
		plr->springcolor = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"dashpadcooldown"))
		plr->dashpadcooldown = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"boostpower"))
		plr->boostpower = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"speedboost"))
		plr->speedboost = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"accelboost"))
		plr->accelboost = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"handleboost"))
		plr->handleboost = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"boostangle"))
		plr->boostangle = luaL_checkangle(L, 3);
	else if (fastcmp(field,"tripwireState"))
		plr->tripwireState = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"tripwirePass"))
		plr->tripwirePass = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"tripwireLeniency"))
		plr->tripwireLeniency = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"itemroulette"))
		plr->itemroulette = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"roulettetype"))
		plr->roulettetype = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"itemtype"))
		plr->itemtype = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"itemamount"))
		plr->itemamount = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"throwdir"))
		plr->throwdir = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"sadtimer"))
		plr->sadtimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"rings"))
		plr->rings = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"pickuprings"))
		plr->pickuprings = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"ringdelay"))
		plr->ringdelay = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"ringboost"))
		plr->ringboost = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"sparkleanim"))
		plr->sparkleanim = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"superring"))
		plr->superring = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"curshield"))
		plr->curshield = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"bubblecool"))
		plr->bubblecool = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"bubbleblowup"))
		plr->bubbleblowup = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flamedash"))
		plr->flamedash = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flamemeter"))
		plr->flamemeter = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"flamelength"))
		plr->flamelength = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"hyudorotimer"))
		plr->hyudorotimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"stealingtimer"))
		plr->stealingtimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"sneakertimer"))
		plr->sneakertimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"floorboost"))
		plr->floorboost = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"growshrinktimer"))
		plr->growshrinktimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"rocketsneakertimer"))
		plr->rocketsneakertimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"invincibilitytimer"))
		plr->invincibilitytimer = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"eggmanexplode"))
		plr->eggmanexplode = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"eggmanblame"))
		plr->eggmanblame = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"bananadrag"))
		plr->bananadrag = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"lastjawztarget"))
		plr->lastjawztarget = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"jawztargetdelay"))
		plr->jawztargetdelay = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"confirmVictim"))
		plr->confirmVictim = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"confirmVictimDelay"))
		plr->confirmVictimDelay = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"glanceDir"))
		plr->glanceDir = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"roundscore"))
		lua_pushinteger(L, plr->roundscore);
	else if (fastcmp(field,"emeralds"))
		plr->emeralds = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"bumpers"))
		plr->bumpers = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"karmadelay"))
		plr->karmadelay = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"spheres"))
		plr->spheres = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"spheredigestion"))
		plr->spheredigestion = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"kartspeed"))
		plr->kartspeed = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"kartweight"))
		plr->kartweight = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"followerskin"))
		plr->followerskin = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"followercolor"))
		plr->followercolor = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"followerready"))
		plr->followerready = luaL_checkboolean(L, 3);
	else if (fastcmp(field,"follower"))	// it's probably best we don't allow the follower mobj to change.
		return NOSET;
	//
	else if (fastcmp(field,"charflags"))
		plr->charflags = (UINT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"followitem"))
		plr->followitem = luaL_checkinteger(L, 3);
	else if (fastcmp(field,"followmobj"))
	{
		mobj_t *mo = NULL;
		if (!lua_isnil(L, 3))
			mo = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
		P_SetTarget(&plr->followmobj, mo);
	}
	else if (fastcmp(field,"lives"))
		plr->lives = (SINT8)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"xtralife"))
		plr->xtralife = (SINT8)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"speed"))
		plr->speed = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"lastspeed"))
		plr->lastspeed = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"deadtimer"))
		plr->deadtimer = (INT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"exiting"))
		plr->exiting = (tic_t)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"cmomx"))
		plr->cmomx = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"cmomy"))
		plr->cmomy = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"rmomx"))
		plr->rmomx = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"rmomy"))
		plr->rmomy = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"totalring"))
		plr->totalring = (INT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"realtime"))
		plr->realtime = (tic_t)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"laps"))
		plr->laps = (UINT8)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"latestlap"))
		plr->latestlap = (UINT8)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"ctfteam"))
		plr->ctfteam = (INT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"checkskip"))
		plr->checkskip = (INT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"starpostnum"))
		plr->starpostnum = (INT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"lastsidehit"))
		plr->lastsidehit = (INT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"lastlinehit"))
		plr->lastlinehit = (INT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"onconveyor"))
		plr->onconveyor = (INT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"awayviewmobj"))
	{
		mobj_t *mo = NULL;
		if (!lua_isnil(L, 3))
			mo = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
		P_SetTarget(&plr->awayviewmobj, mo);
	}
	else if (fastcmp(field,"awayviewtics"))
	{
		plr->awayviewtics = (INT32)luaL_checkinteger(L, 3);
		if (plr->awayviewtics && !plr->awayviewmobj) // awayviewtics must ALWAYS have an awayviewmobj set!!
			P_SetTarget(&plr->awayviewmobj, plr->mo); // but since the script might set awayviewmobj immediately AFTER setting awayviewtics, use player mobj as filler for now.
	}
	else if (fastcmp(field,"awayviewaiming"))
		plr->awayviewaiming = luaL_checkangle(L, 3);
	else if (fastcmp(field,"spectator"))
		plr->spectator = lua_toboolean(L, 3);
	else if (fastcmp(field,"bot"))
		return NOSET;
	else if (fastcmp(field,"jointime"))
		plr->jointime = (tic_t)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"splitscreenindex"))
		return NOSET;
#ifdef HWRENDER
	else if (fastcmp(field,"fovadd"))
		plr->fovadd = luaL_checkfixed(L, 3);
#endif
	else {
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
		I_Assert(lua_istable(L, -1));
		lua_pushlightuserdata(L, plr);
		lua_rawget(L, -2);
		if (lua_isnil(L, -1)) {
			// This index doesn't have a table for extra values yet, let's make one.
			lua_pop(L, 1);
			CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; adding it as Lua data.\n"), "player_t", field);
			lua_newtable(L);
			lua_pushlightuserdata(L, plr);
			lua_pushvalue(L, -2); // ext value table
			lua_rawset(L, -4); // LREG_EXTVARS table
		}
		lua_pushvalue(L, 3); // value to store
		lua_setfield(L, -2, field);
		lua_pop(L, 2);
	}

	return 0;
}

#undef NOSET

static int player_num(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	if (!plr)
		return luaL_error(L, "accessed player_t doesn't exist anymore.");
	lua_pushinteger(L, plr-players);
	return 1;
}

// karthud, ks -> karthud[ks]
static int karthud_get(lua_State *L)
{
	INT32 *karthud = *((INT32 **)luaL_checkudata(L, 1, META_KARTHUD));
	karthudtype_t ks = luaL_checkinteger(L, 2);
	if (ks >= NUMKARTHUD)
		return luaL_error(L, LUA_QL("karthudtype_t") " cannot be %u", ks);
	lua_pushinteger(L, karthud[ks]);
	return 1;
}

// karthud, ks, value -> karthud[ks] = value
static int karthud_set(lua_State *L)
{
	INT32 *karthud = *((INT32 **)luaL_checkudata(L, 1, META_KARTHUD));
	karthudtype_t ks = luaL_checkinteger(L, 2);
	INT32 i = (INT32)luaL_checkinteger(L, 3);
	if (ks >= NUMKARTHUD)
		return luaL_error(L, LUA_QL("karthudtype_t") " cannot be %u", ks);
	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");
	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in CMD building code!");
	karthud[ks] = i;
	return 0;
}

// #karthud -> NUMKARTHUD
static int karthud_len(lua_State *L)
{
	lua_pushinteger(L, NUMKARTHUD);
	return 1;
}

// player.cmd get/set
#define NOFIELD luaL_error(L, LUA_QL("ticcmd_t") " has no field named " LUA_QS, field)
#define NOSET luaL_error(L, LUA_QL("ticcmd_t") " field " LUA_QS " cannot be set.", field)

static int ticcmd_get(lua_State *L)
{
	ticcmd_t *cmd = *((ticcmd_t **)luaL_checkudata(L, 1, META_TICCMD));
	const char *field = luaL_checkstring(L, 2);
	if (!cmd)
		return LUA_ErrInvalid(L, "player_t");

	if (fastcmp(field,"forwardmove"))
		lua_pushinteger(L, cmd->forwardmove);
	else if (fastcmp(field,"sidemove"))
		lua_pushinteger(L, cmd->sidemove);
	else if (fastcmp(field,"turning"))
		lua_pushinteger(L, cmd->turning);
	else if (fastcmp(field,"throwdir"))
		lua_pushinteger(L, cmd->throwdir);
	else if (fastcmp(field,"aiming"))
		lua_pushinteger(L, cmd->aiming);
	else if (fastcmp(field,"buttons"))
		lua_pushinteger(L, cmd->buttons);
	else if (fastcmp(field,"latency"))
		lua_pushinteger(L, cmd->latency);
	else if (fastcmp(field,"flags"))
		lua_pushinteger(L, cmd->flags);
	else
		return NOFIELD;

	return 1;
}

static int ticcmd_set(lua_State *L)
{
	ticcmd_t *cmd = *((ticcmd_t **)luaL_checkudata(L, 1, META_TICCMD));
	const char *field = luaL_checkstring(L, 2);
	if (!cmd)
		return LUA_ErrInvalid(L, "ticcmd_t");

	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");

	if (fastcmp(field,"forwardmove"))
		cmd->forwardmove = (SINT8)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"turning"))
		cmd->turning = (INT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"throwdir"))
		cmd->throwdir = (INT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"aiming"))
		cmd->aiming = (INT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"buttons"))
		cmd->buttons = (UINT16)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"latency"))
		return NOSET;
	else if (fastcmp(field,"flags"))
		return NOSET;
	else
		return NOFIELD;

	return 0;
}

#undef NOFIELD

// Same shit for player.respawn variable... Why is everything in different sub-variables again now???
#define RNOFIELD luaL_error(L, LUA_QL("respawnvars_t") " has no field named " LUA_QS, field)
#define RUNIMPLEMENTED luaL_error(L, LUA_QL("respawnvars_t") " unimplemented field " LUA_QS " cannot be read or set.", field)
// @TODO: Waypoints in Lua possibly maybe? No don't count on me to do it...

static int respawn_get(lua_State *L)
{
	respawnvars_t *rsp = *((respawnvars_t **)luaL_checkudata(L, 1, META_RESPAWN));
	const char *field = luaL_checkstring(L, 2);
	if (!rsp)
		return LUA_ErrInvalid(L, "player_t");

	if (fastcmp(field,"state"))
		lua_pushinteger(L, rsp->state);
	else if (fastcmp(field,"waypoint"))
		return RUNIMPLEMENTED;
	else if (fastcmp(field,"pointx"))
		lua_pushfixed(L, rsp->pointx);
	else if (fastcmp(field,"pointy"))
		lua_pushfixed(L, rsp->pointy);
	else if (fastcmp(field,"pointz"))
		lua_pushfixed(L, rsp->pointz);
	else if (fastcmp(field,"flip"))
		lua_pushboolean(L, rsp->flip);
	else if (fastcmp(field,"timer"))
		lua_pushinteger(L, rsp->timer);
	else if (fastcmp(field,"distanceleft"))
		lua_pushinteger(L, rsp->distanceleft);	// Can't possibly foresee any problem when pushing UINT32 to Lua's INT32 hahahahaha, get ready for dumb hacky shit on high distances.
	else if (fastcmp(field,"dropdash"))
		lua_pushinteger(L, rsp->dropdash);
	else
		return RNOFIELD;

	return 1;
}

static int respawn_set(lua_State *L)
{
	respawnvars_t *rsp = *((respawnvars_t **)luaL_checkudata(L, 1, META_RESPAWN));
	const char *field = luaL_checkstring(L, 2);
	if (!rsp)
		return LUA_ErrInvalid(L, "respawnvars_t");

	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");
	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in CMD building code!");

	if (fastcmp(field,"state"))
		rsp->state = (UINT8)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"waypoint"))
		return RUNIMPLEMENTED;
	else if (fastcmp(field,"pointx"))
		rsp->pointx = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"pointy"))
		rsp->pointy = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"pointz"))
		rsp->pointz = luaL_checkfixed(L, 3);
	else if (fastcmp(field,"flip"))
		rsp->flip = luaL_checkboolean(L, 3);
	else if (fastcmp(field,"timer"))
		rsp->timer = (tic_t)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"distanceleft"))
		rsp->distanceleft = (UINT32)luaL_checkinteger(L, 3);
	else if (fastcmp(field,"dropdash"))
		rsp->dropdash = (tic_t)luaL_checkinteger(L, 3);
	else
		return RNOFIELD;

	return 0;
}

#undef RNOFIELD
#undef RUNIMPLEMENTED


int LUA_PlayerLib(lua_State *L)
{
	luaL_newmetatable(L, META_PLAYER);
		lua_pushcfunction(L, player_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, player_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, player_num);
		lua_setfield(L, -2, "__len");
	lua_pop(L,1);

	luaL_newmetatable(L, META_KARTHUD);
		lua_pushcfunction(L, karthud_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, karthud_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, karthud_len);
		lua_setfield(L, -2, "__len");
	lua_pop(L,1);

	luaL_newmetatable(L, META_RESPAWN);
		lua_pushcfunction(L, respawn_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, respawn_set);
		lua_setfield(L, -2, "__newindex");
	lua_pop(L,1);

	luaL_newmetatable(L, META_TICCMD);
		lua_pushcfunction(L, ticcmd_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, ticcmd_set);
		lua_setfield(L, -2, "__newindex");
	lua_pop(L,1);

	lua_newuserdata(L, 0);
		lua_createtable(L, 0, 2);
			lua_pushcfunction(L, lib_getPlayer);
			lua_setfield(L, -2, "__index");

			lua_pushcfunction(L, lib_lenPlayer);
			lua_setfield(L, -2, "__len");
		lua_setmetatable(L, -2);
	lua_setglobal(L, "players");

	// push displayplayers in the same fashion
	lua_newuserdata(L, 0);
		lua_createtable(L, 0, 2);
			lua_pushcfunction(L, lib_getDisplayplayers);
			lua_setfield(L, -2, "__index");

			lua_pushcfunction(L, lib_lenDisplayplayers);
			lua_setfield(L, -2, "__len");
		lua_setmetatable(L, -2);
	lua_setglobal(L, "displayplayers");

	return 0;
}
