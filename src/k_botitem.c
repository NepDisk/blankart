// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2018-2020 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2018-2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_botitem.c
/// \brief Bot item usage logic

#include "doomdef.h"
#include "d_player.h"
#include "g_game.h"
#include "r_main.h"
#include "p_local.h"
#include "k_bot.h"
#include "lua_hook.h"
#include "byteptr.h"
#include "d_net.h" // nodetoplayer
#include "k_kart.h"
#include "z_zone.h"
#include "i_system.h"
#include "p_maputl.h"
#include "d_ticcmd.h"
#include "m_random.h"
#include "r_things.h" // numskins

/*--------------------------------------------------
	static inline boolean K_ItemButtonWasDown(player_t *player)

		Looks for players around the bot, and presses the item button
		if there is one in range.

	Input Arguments:-
		player - Bot to check.

	Return:-
		true if the item button was pressed last tic, otherwise false.
--------------------------------------------------*/
static inline boolean K_ItemButtonWasDown(player_t *player)
{
	return (player->oldcmd.buttons & BT_ATTACK);
}

/*--------------------------------------------------
	static boolean K_BotUseItemNearPlayer(player_t *player, ticcmd_t *cmd, fixed_t radius)

		Looks for players around the bot, and presses the item button
		if there is one in range.

	Input Arguments:-
		player - Bot to compare against.
		cmd - The bot's ticcmd.
		radius - The radius to look for players in.

	Return:-
		true if a player was found & we can press the item button, otherwise false.
--------------------------------------------------*/
static boolean K_BotUseItemNearPlayer(player_t *player, ticcmd_t *cmd, fixed_t radius)
{
	UINT8 i;

	if (K_ItemButtonWasDown(player) == true)
	{
		return false;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *target = NULL;
		fixed_t dist = INT32_MAX;

		if (!playeringame[i])
		{
			continue;
		}

		target = &players[i];

		if (target->mo == NULL || P_MobjWasRemoved(target->mo)
			|| player == target || target->spectator
			|| target->flashing)
		{
			continue;
		}

		dist = P_AproxDistance(P_AproxDistance(
			player->mo->x - target->mo->x,
			player->mo->y - target->mo->y),
			(player->mo->z - target->mo->z) / 4
		);

		if (dist <= radius)
		{
			cmd->buttons |= BT_ATTACK;
			return true;
		}
	}

	return false;
}

/*--------------------------------------------------
	static player_t *K_PlayerNearSpot(player_t *player, fixed_t x, fixed_t y, fixed_t radius)

		Looks for players around a specified x/y coordinate.

	Input Arguments:-
		player - Bot to compare against.
		x - X coordinate to look around.
		y - Y coordinate to look around.
		radius - The radius to look for players in.

	Return:-
		The player we found, NULL if nothing was found.
--------------------------------------------------*/
static player_t *K_PlayerNearSpot(player_t *player, fixed_t x, fixed_t y, fixed_t radius)
{
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *target = NULL;
		fixed_t dist = INT32_MAX;

		if (!playeringame[i])
		{
			continue;
		}

		target = &players[i];

		if (target->mo == NULL || P_MobjWasRemoved(target->mo)
			|| player == target || target->spectator
			|| target->flashing)
		{
			continue;
		}

		dist = P_AproxDistance(
			x - target->mo->x,
			y - target->mo->y
		);

		if (dist <= radius)
		{
			return target;
		}
	}

	return NULL;
}

/*--------------------------------------------------
	static player_t *K_PlayerPredictThrow(player_t *player, UINT8 extra)

		Looks for players around the predicted coordinates of their thrown item.

	Input Arguments:-
		player - Bot to compare against.
		extra - Extra throwing distance, for aim forward on mines.

	Return:-
		The player we're trying to throw at, NULL if none was found.
--------------------------------------------------*/
static player_t *K_PlayerPredictThrow(player_t *player, UINT8 extra)
{
	const fixed_t dist = (30 + (extra * 10)) * player->mo->scale;
	const UINT32 airtime = FixedDiv(dist + player->mo->momz, gravity);
	const fixed_t throwspeed = FixedMul(82 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	const fixed_t estx = player->mo->x + P_ReturnThrustX(NULL, player->mo->angle, (throwspeed + player->speed) * airtime);
	const fixed_t esty = player->mo->y + P_ReturnThrustY(NULL, player->mo->angle, (throwspeed + player->speed) * airtime);
	return K_PlayerNearSpot(player, estx, esty, player->mo->radius * 2);
}

/*--------------------------------------------------
	static player_t *K_PlayerInCone(player_t *player, UINT16 cone, boolean flip)

		Looks for players in the .

	Input Arguments:-
		player - Bot to compare against.
		radius - How far away the targets can be.
		cone - Size of cone, in degrees as an integer.
		flip - If true, look behind. Otherwise, check in front of the player.

	Return:-
		true if a player was found in the cone, otherwise false.
--------------------------------------------------*/
static player_t *K_PlayerInCone(player_t *player, fixed_t radius, UINT16 cone, boolean flip)
{
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *target = NULL;
		fixed_t dist = INT32_MAX;

		if (!playeringame[i])
		{
			continue;
		}

		target = &players[i];

		if (target->mo == NULL || P_MobjWasRemoved(target->mo)
			|| player == target || target->spectator
			|| target->flashing
			|| !P_CheckSight(player->mo, target->mo))
		{
			continue;
		}

		dist = P_AproxDistance(P_AproxDistance(
			player->mo->x - target->mo->x,
			player->mo->y - target->mo->y),
			(player->mo->z - target->mo->z) / 4
		);

		if (dist <= radius)
		{
			angle_t a = player->mo->angle - R_PointToAngle2(player->mo->x, player->mo->y, target->mo->x, target->mo->y);
			INT16 ad = 0;

			if (a < ANGLE_180)
			{
				ad = AngleFixed(a)>>FRACBITS;
			}
			else 
			{
				ad = 360-(AngleFixed(a)>>FRACBITS);
			}

			ad = abs(ad);

			if (flip)
			{
				if (ad >= 180-cone)
				{
					return target;
				}
			}
			else
			{
				if (ad <= cone)
				{
					return target;
				}
			}
		}
	}

	return NULL;
}

/*--------------------------------------------------
	static boolean K_RivalBotAggression(player_t *bot, player_t *target)

		Returns if a bot is a rival & wants to be aggressive to a player.

	Input Arguments:-
		bot - Bot to check.
		target - Who the bot wants to attack.

	Return:-
		false if not the rival. false if the target is another bot. Otherwise, true.
--------------------------------------------------*/
static boolean K_RivalBotAggression(player_t *bot, player_t *target)
{
	if (bot == NULL || target == NULL)
	{
		// Invalid.
		return false;
	}

	if (bot->bot == false)
	{
		// lol
		return false;
	}

	if (bot->botvars.rival == false)
	{
		// Not the rival, we aren't self-aware.
		return false;
	}

	if (target->bot == false)
	{
		// This bot knows that the real threat is the player.
		return true;
	}

	// Calling them your friends is misleading, but you'll at least spare them.
	return false;
}

/*--------------------------------------------------
	static void K_ItemConfirmForTarget(player_t *bot, player_t *target, UINT16 amount)

		Handles updating item confirm values for offense items.

	Input Arguments:-
		bot - Bot to check.
		target - Who the bot wants to attack.
		amount - Amount to increase item confirm time by.

	Return:-
		None
--------------------------------------------------*/
static void K_ItemConfirmForTarget(player_t *bot, player_t *target, UINT16 amount)
{
	if (bot == NULL || target == NULL)
	{
		return;
	}

	if (K_RivalBotAggression(bot, target) == true)
	{
		// Double the rate when you're aggressive.
		bot->botvars.itemconfirm += amount << 1;
	}
	else
	{
		// Do as normal.
		bot->botvars.itemconfirm += amount;
	}
}

/*--------------------------------------------------
	static boolean K_BotGenericPressItem(player_t *player, ticcmd_t *cmd, SINT8 dir)

		Presses the item button & aim buttons for the bot.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.
		dir - Aiming direction: 1 for forwards, -1 for backwards, 0 for neutral.

	Return:-
		true if we could press, false if not.
--------------------------------------------------*/
static boolean K_BotGenericPressItem(player_t *player, ticcmd_t *cmd, SINT8 dir)
{
	if (K_ItemButtonWasDown(player) == true)
	{
		return false;
	}

	cmd->throwdir = KART_FULLTURN * dir;
	cmd->buttons |= BT_ATTACK;
	player->botvars.itemconfirm = 0;
	return true;
}

/*--------------------------------------------------
	static void K_BotItemGenericTap(player_t *player, ticcmd_t *cmd)

		Item usage for generic items that you need to tap.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemGenericTap(player_t *player, ticcmd_t *cmd)
{
	if (K_ItemButtonWasDown(player) == false)
	{
		cmd->buttons |= BT_ATTACK;
		player->botvars.itemconfirm = 0;
	}
}

/*--------------------------------------------------
	static boolean K_BotRevealsGenericTrap(player_t *player, INT16 turnamt, boolean mine)

		Decides if a bot is ready to reveal their trap item or not.

	Input Arguments:-
		player - Bot that has the banana.
		turnamt - How hard they currently are turning.
		mine - Set to true to handle Mine-specific behaviors.

	Return:-
		true if we want the bot to reveal their banana, otherwise false.
--------------------------------------------------*/
static boolean K_BotRevealsGenericTrap(player_t *player, INT16 turnamt, boolean mine)
{
	const fixed_t coneDist = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));

	if (abs(turnamt) >= KART_FULLTURN/2)
	{
		// DON'T reveal on turns, we can place bananas on turns whenever we have multiple to spare,
		// or if you missed your intentioned throw/place on a player.
		return false;
	}

	// Check the predicted throws.
	if (K_PlayerPredictThrow(player, 0) != NULL)
	{
		return true;
	}

	if (mine)
	{
		if (K_PlayerPredictThrow(player, 1) != NULL)
		{
			return true;
		}
	}

	// Check your behind.
	if (K_PlayerInCone(player, coneDist, 15, true) != NULL)
	{
		return true;
	}

	return false;
}

/*--------------------------------------------------
	static void K_BotItemGenericTrapShield(player_t *player, ticcmd_t *cmd, INT16 turnamt, boolean mine)

		Item usage for Eggman shields.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.
		turnamt - How hard they currently are turning.
		mine - Set to true to handle Mine-specific behaviors.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemGenericTrapShield(player_t *player, ticcmd_t *cmd, INT16 turnamt, boolean mine)
{
	if (player->pflags & PF_ITEMOUT)
	{
		return;
	}

	if (K_BotRevealsGenericTrap(player, turnamt, mine) || (player->botvars.itemconfirm++ > 5*TICRATE))
	{
		K_BotGenericPressItem(player, cmd, 0);
	}
}

/*--------------------------------------------------
	static void K_BotItemGenericOrbitShield(player_t *player, ticcmd_t *cmd)

		Item usage for orbitting shields.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemGenericOrbitShield(player_t *player, ticcmd_t *cmd)
{
	if (player->pflags & PF_ITEMOUT)
	{
		return;
	}

	K_BotGenericPressItem(player, cmd, 0);
}

/*--------------------------------------------------
	static void K_BotItemSneaker(player_t *player, ticcmd_t *cmd)

		Item usage for sneakers.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemSneaker(player_t *player, ticcmd_t *cmd)
{
	if ((player->offroad && K_ApplyOffroad(player)) // Stuck in offroad, use it NOW
		|| K_GetWaypointIsShortcut(player->nextwaypoint) == true // Going toward a shortcut!
		|| player->speed < K_GetKartSpeed(player, false, true) / 2 // Being slowed down too much
		|| player->speedboost > (FRACUNIT/8) // Have another type of boost (tethering)
		|| player->botvars.itemconfirm > 4*TICRATE) // Held onto it for too long
	{
		if (player->sneakertimer == 0 && K_ItemButtonWasDown(player) == false)
		{
			cmd->buttons |= BT_ATTACK;
			player->botvars.itemconfirm = 2*TICRATE;
		}
	}
	else
	{
		player->botvars.itemconfirm++;
	}
}

/*--------------------------------------------------
	static void K_BotItemRocketSneaker(player_t *player, ticcmd_t *cmd)

		Item usage for rocket sneakers.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemRocketSneaker(player_t *player, ticcmd_t *cmd)
{
	if (player->botvars.itemconfirm > TICRATE)
	{
		if (player->sneakertimer == 0 && K_ItemButtonWasDown(player) == false)
		{
			cmd->buttons |= BT_ATTACK;
			player->botvars.itemconfirm = 0;
		}
	}
	else
	{
		player->botvars.itemconfirm++;
	}
}

/*--------------------------------------------------
	static void K_BotItemBanana(player_t *player, ticcmd_t *cmd, INT16 turnamt)

		Item usage for trap item throwing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.
		turnamt - How hard they currently are turning.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemBanana(player_t *player, ticcmd_t *cmd, INT16 turnamt)
{
	const fixed_t coneDist = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	SINT8 throwdir = -1;
	boolean tryLookback = false;
	player_t *target = NULL;

	player->botvars.itemconfirm++;

	target = K_PlayerInCone(player, coneDist, 15, true);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
		throwdir = -1;
		tryLookback = true;
	}

	if (abs(turnamt) >= KART_FULLTURN/2)
	{
		player->botvars.itemconfirm += player->botvars.difficulty / 2;
		throwdir = -1;
	}
	else
	{
		target = K_PlayerPredictThrow(player, 0);

		if (target != NULL)
		{
			K_ItemConfirmForTarget(player, target, player->botvars.difficulty * 2);
			throwdir = 1;
		}
	}

	if (tryLookback == true && throwdir == -1)
	{
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 10*TICRATE || player->bananadrag >= TICRATE)
	{
		K_BotGenericPressItem(player, cmd, throwdir);
	}
}

/*--------------------------------------------------
	static void K_BotItemMine(player_t *player, ticcmd_t *cmd, INT16 turnamt)

		Item usage for trap item throwing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.
		turnamt - How hard they currently are turning.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemMine(player_t *player, ticcmd_t *cmd, INT16 turnamt)
{
	const fixed_t coneDist = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	SINT8 throwdir = 0;
	boolean tryLookback = false;
	player_t *target = NULL;

	player->botvars.itemconfirm++;

	target = K_PlayerInCone(player, coneDist, 15, true);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
		throwdir = -1;
	}

	if (abs(turnamt) >= KART_FULLTURN/2)
	{
		player->botvars.itemconfirm += player->botvars.difficulty / 2;
		throwdir = -1;
		tryLookback = true;
	}
	else
	{
		target = K_PlayerPredictThrow(player, 0);
		if (target != NULL)
		{
			K_ItemConfirmForTarget(player, target, player->botvars.difficulty * 2);
			throwdir = 0;
		}

		target = K_PlayerPredictThrow(player, 1);
		if (target != NULL)
		{
			K_ItemConfirmForTarget(player, target, player->botvars.difficulty * 2);
			throwdir = 1;
		}
	}

	if (tryLookback == true && throwdir == -1)
	{
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 10*TICRATE || player->bananadrag >= TICRATE)
	{
		K_BotGenericPressItem(player, cmd, throwdir);
	}
}

/*--------------------------------------------------
	static void K_BotItemLandmine(player_t *player, ticcmd_t *cmd, INT16 turnamt)

		Item usage for landmine tossing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.
		turnamt - How hard they currently are turning.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemLandmine(player_t *player, ticcmd_t *cmd, INT16 turnamt)
{
	const fixed_t coneDist = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	player_t *target = NULL;

	player->botvars.itemconfirm++;

	if (abs(turnamt) >= KART_FULLTURN/2)
	{
		player->botvars.itemconfirm += player->botvars.difficulty / 2;
	}

	target = K_PlayerInCone(player, coneDist, 15, true);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 10*TICRATE)
	{
		K_BotGenericPressItem(player, cmd, -1);
	}
}

/*--------------------------------------------------
	static void K_BotItemEggman(player_t *player, ticcmd_t *cmd)

		Item usage for Eggman item throwing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemEggman(player_t *player, ticcmd_t *cmd)
{
	const fixed_t coneDist = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	const UINT8 stealth = K_EggboxStealth(player->mo->x, player->mo->y);
	SINT8 throwdir = -1;
	boolean tryLookback = false;
	player_t *target = NULL;

	player->botvars.itemconfirm++;

	target = K_PlayerPredictThrow(player, 0);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty / 2);
		throwdir = 1;
	}

	target = K_PlayerInCone(player, coneDist, 15, true);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
		throwdir = -1;
		tryLookback = true;
	}

	if (stealth > 1 || player->itemroulette > 0)
	{
		player->botvars.itemconfirm += player->botvars.difficulty * 4;
		throwdir = -1;
	}

	if (tryLookback == true && throwdir == -1)
	{
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 10*TICRATE || player->bananadrag >= TICRATE)
	{
		K_BotGenericPressItem(player, cmd, throwdir);
	}
}

/*--------------------------------------------------
	static boolean K_BotRevealsEggbox(player_t *player)

		Decides if a bot is ready to place their Eggman item or not.

	Input Arguments:-
		player - Bot that has the eggbox.

	Return:-
		true if we want the bot to reveal their eggbox, otherwise false.
--------------------------------------------------*/
static boolean K_BotRevealsEggbox(player_t *player)
{
	const fixed_t coneDist = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	const UINT8 stealth = K_EggboxStealth(player->mo->x, player->mo->y);
	player_t *target = NULL;

	// This is a stealthy spot for an eggbox, lets reveal it!
	if (stealth > 1)
	{
		return true;
	}

	// Check the predicted throws.
	target = K_PlayerPredictThrow(player, 0);
	if (target != NULL)
	{
		return true;
	}

	// Check your behind.
	target = K_PlayerInCone(player, coneDist, 15, true);
	if (target != NULL)
	{
		return true;
	}

	return false;
}

/*--------------------------------------------------
	static void K_BotItemEggmanShield(player_t *player, ticcmd_t *cmd)

		Item usage for Eggman shields.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemEggmanShield(player_t *player, ticcmd_t *cmd)
{
	if (player->pflags & PF_EGGMANOUT)
	{
		return;
	}

	if (K_BotRevealsEggbox(player) == true || (player->botvars.itemconfirm++ > 20*TICRATE))
	{
		K_BotGenericPressItem(player, cmd, 0);
	}
}

/*--------------------------------------------------
	static void K_BotItemEggmanExplosion(player_t *player, ticcmd_t *cmd)

		Item usage for Eggman explosions.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemEggmanExplosion(player_t *player, ticcmd_t *cmd)
{
	if (player->position == 1)
	{
		// Hey, we aren't gonna find anyone up here...
		// why don't we slow down a bit? :)
		cmd->forwardmove /= 2;
	}

	K_BotUseItemNearPlayer(player, cmd, 128*player->mo->scale);
}

/*--------------------------------------------------
	static void K_BotItemOrbinaut(player_t *player, ticcmd_t *cmd)

		Item usage for Orbinaut throwing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemOrbinaut(player_t *player, ticcmd_t *cmd)
{
	const fixed_t topspeed = K_GetKartSpeed(player, false, true);
	fixed_t radius = FixedMul(2560 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	SINT8 throwdir = -1;
	boolean tryLookback = false;
	UINT8 snipeMul = 2;
	player_t *target = NULL;

	if (player->speed > topspeed)
	{
		radius = FixedMul(radius, FixedDiv(player->speed, topspeed));
		snipeMul = 3; // Confirm faster when you'll throw it with a bunch of extra speed!!
	}

	player->botvars.itemconfirm++;

	target = K_PlayerInCone(player, radius, 15, false);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty * snipeMul);
		throwdir = 1;
	}
	else
	{
		target = K_PlayerInCone(player, radius, 15, true);

		if (target != NULL)
		{
			K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
			throwdir = -1;
			tryLookback = true;
		}
	}

	if (tryLookback == true && throwdir == -1)
	{
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 25*TICRATE)
	{
		K_BotGenericPressItem(player, cmd, throwdir);
	}
}

/*--------------------------------------------------
	static void K_BotItemDropTarget(player_t *player, ticcmd_t *cmd)

		Item usage for Drop Target throwing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemDropTarget(player_t *player, ticcmd_t *cmd)
{
	const fixed_t topspeed = K_GetKartSpeed(player, false, true);
	fixed_t radius = FixedMul(1280 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	SINT8 throwdir = -1;
	boolean tryLookback = false;
	UINT8 snipeMul = 2;
	player_t *target = NULL;

	if (player->speed > topspeed)
	{
		radius = FixedMul(radius, FixedDiv(player->speed, topspeed));
		snipeMul = 3; // Confirm faster when you'll throw it with a bunch of extra speed!!
	}

	player->botvars.itemconfirm++;

	target = K_PlayerInCone(player, radius, 15, false);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty * snipeMul);
		throwdir = 1;
	}
	else
	{
		target = K_PlayerInCone(player, radius, 15, true);

		if (target != NULL)
		{
			K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
			throwdir = -1;
			tryLookback = true;
		}
	}

	if (tryLookback == true && throwdir == -1)
	{
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 25*TICRATE)
	{
		K_BotGenericPressItem(player, cmd, throwdir);
	}
}

/*--------------------------------------------------
	static void K_BotItemJawz(player_t *player, ticcmd_t *cmd)

		Item usage for Jawz throwing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemJawz(player_t *player, ticcmd_t *cmd)
{
	const fixed_t topspeed = K_GetKartSpeed(player, false, true);
	fixed_t radius = FixedMul(2560 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed));
	SINT8 throwdir = 1;
	boolean tryLookback = false;
	UINT8 snipeMul = 2;
	INT32 lastTarg = player->lastjawztarget;
	player_t *target = NULL;

	if (player->speed > topspeed)
	{
		radius = FixedMul(radius, FixedDiv(player->speed, topspeed));
		snipeMul = 3; // Confirm faster when you'll throw it with a bunch of extra speed!!
	}

	player->botvars.itemconfirm++;

	target = K_PlayerInCone(player, radius, 15, true);
	if (target != NULL)
	{
		K_ItemConfirmForTarget(player, target, player->botvars.difficulty);
		throwdir = -1;
		tryLookback = true;
	}

	if (lastTarg != -1
		&& playeringame[lastTarg] == true
		&& players[lastTarg].spectator == false
		&& players[lastTarg].mo != NULL
		&& P_MobjWasRemoved(players[lastTarg].mo) == false)
	{
		mobj_t *targMo = players[lastTarg].mo;
		mobj_t *mobj = NULL, *next = NULL;
		boolean targettedAlready = false;

		target = &players[lastTarg];

		// Make sure no other Jawz are targetting this player.
		for (mobj = kitemcap; mobj; mobj = next)
		{
			next = mobj->itnext;

			if (mobj->type == MT_JAWZ && mobj->target == targMo)
			{
				targettedAlready = true;
				break;
			}
		}

		if (targettedAlready == false)
		{
			K_ItemConfirmForTarget(player, target, player->botvars.difficulty * snipeMul);
			throwdir = 1;
		}
	}

	if (tryLookback == true && throwdir == -1)
	{
		cmd->buttons |= BT_LOOKBACK;
	}

	if (player->botvars.itemconfirm > 25*TICRATE)
	{
		K_BotGenericPressItem(player, cmd, throwdir);
	}
}

/*--------------------------------------------------
	static void K_BotItemLightning(player_t *player, ticcmd_t *cmd)

		Item usage for Lightning Shield.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemLightning(player_t *player, ticcmd_t *cmd)
{
	if (K_BotUseItemNearPlayer(player, cmd, 192*player->mo->scale) == false)
	{
		if (player->botvars.itemconfirm > 10*TICRATE)
		{
			K_BotGenericPressItem(player, cmd, 0);
		}
		else
		{
			player->botvars.itemconfirm++;
		}
	}
}

/*--------------------------------------------------
	static void K_BotItemBubble(player_t *player, ticcmd_t *cmd)

		Item usage for Bubble Shield.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemBubble(player_t *player, ticcmd_t *cmd)
{
	boolean hold = false;

	if (player->bubbleblowup <= 0)
	{
		UINT8 i;

		player->botvars.itemconfirm++;

		if (player->bubblecool <= 0)
		{
			const fixed_t radius = 192 * player->mo->scale;

			for (i = 0; i < MAXPLAYERS; i++)
			{
				player_t *target = NULL;
				fixed_t dist = INT32_MAX;

				if (!playeringame[i])
				{
					continue;
				}

				target = &players[i];

				if (target->mo == NULL || P_MobjWasRemoved(target->mo)
					|| player == target || target->spectator
					|| target->flashing)
				{
					continue;
				}

				dist = P_AproxDistance(P_AproxDistance(
					player->mo->x - target->mo->x,
					player->mo->y - target->mo->y),
					(player->mo->z - target->mo->z) / 4
				);

				if (dist <= radius)
				{
					hold = true;
					break;
				}
			}
		}
	}
	else if (player->bubbleblowup >= bubbletime)
	{
		if (player->botvars.itemconfirm > 10*TICRATE)
		{
			hold = true;
		}
	}
	else if (player->bubbleblowup < bubbletime)
	{
		hold = true;
	}

	if (hold && (player->pflags & PF_HOLDREADY))
	{
		cmd->buttons |= BT_ATTACK;
	}
}

/*--------------------------------------------------
	static void K_BotItemFlame(player_t *player, ticcmd_t *cmd)

		Item usage for Flame Shield.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemFlame(player_t *player, ticcmd_t *cmd)
{
	if (player->botvars.itemconfirm > 0)
	{
		player->botvars.itemconfirm--;
	}
	else if (player->pflags & PF_HOLDREADY)
	{
		INT32 flamemax = player->flamelength * flameseg;

		if (player->flamemeter < flamemax || flamemax == 0)
		{
			cmd->buttons |= BT_ATTACK;
		}
		else
		{
			player->botvars.itemconfirm = 3*flamemax/4;
		}
	}
}

/*--------------------------------------------------
	static void K_BotItemRings(player_t *player, ticcmd_t *cmd)

		Item usage for rings.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemRings(player_t *player, ticcmd_t *cmd)
{
	INT32 saferingsval = 16 - K_GetKartRingPower(player, false);

	if (player->speed < K_GetKartSpeed(player, false, true) / 2 // Being slowed down too much
		|| player->speedboost > (FRACUNIT/5)) // Have another type of boost (tethering)
	{
		saferingsval -= 5;
	}

	if (player->rings > saferingsval)
	{
		cmd->buttons |= BT_ATTACK;
	}
}

/*--------------------------------------------------
	static void K_BotItemRouletteMash(player_t *player, ticcmd_t *cmd)

		Item usage for item roulette mashing.

	Input Arguments:-
		player - Bot to do this for.
		cmd - Bot's ticcmd to edit.

	Return:-
		None
--------------------------------------------------*/
static void K_BotItemRouletteMash(player_t *player, ticcmd_t *cmd)
{
	boolean mash = false;

	if (K_ItemButtonWasDown(player) == true)
	{
		return;
	}

	if (player->rings < 0 && cv_superring.value)
	{
		// Uh oh, we need a loan!
		// It'll be better in the long run for bots to lose an item set for 10 free rings.
		mash = true;
	}

	// TODO: Mash based on how far behind you are, when items are
	// almost garantueed to be in your favor.

	if (mash == true)
	{
		cmd->buttons |= BT_ATTACK;
	}
}

/*--------------------------------------------------
	void K_BotItemUsage(player_t *player, ticcmd_t *cmd, INT16 turnamt)

		See header file for description.
--------------------------------------------------*/
void K_BotItemUsage(player_t *player, ticcmd_t *cmd, INT16 turnamt)
{
	if (player->pflags & PF_USERINGS)
	{
		// Use rings!

		if (leveltime > starttime && !player->exiting)
		{
			K_BotItemRings(player, cmd);
		}
	}
	else
	{
		if (player->botvars.itemdelay)
		{
			player->botvars.itemdelay--;
			player->botvars.itemconfirm = 0;
			return;
		}

		if (player->itemroulette)
		{
			// Mashing behaviors
			K_BotItemRouletteMash(player, cmd);
			return;
		}

		if (player->stealingtimer == 0)
		{
			if (player->eggmanexplode)
			{
				K_BotItemEggmanExplosion(player, cmd);
			}
			else if (player->pflags & PF_EGGMANOUT)
			{
				K_BotItemEggman(player, cmd);
			}
			else if (player->rocketsneakertimer > 0)
			{
				K_BotItemRocketSneaker(player, cmd);
			}
			else
			{
				switch (player->itemtype)
				{
					default:
						if (player->itemtype != KITEM_NONE)
						{
							K_BotItemGenericTap(player, cmd);
						}

						player->botvars.itemconfirm = 0;
						break;
					case KITEM_INVINCIBILITY:
					case KITEM_SPB:
					case KITEM_GROW:
					case KITEM_SHRINK:
					case KITEM_HYUDORO:
					case KITEM_SUPERRING:
						K_BotItemGenericTap(player, cmd);
						break;
					case KITEM_ROCKETSNEAKER:
						if (player->rocketsneakertimer <= 0)
						{
							K_BotItemGenericTap(player, cmd);
						}
						break;
					case KITEM_SNEAKER:
						K_BotItemSneaker(player, cmd);
						break;
					case KITEM_BANANA:
						if (!(player->pflags & PF_ITEMOUT))
						{
							K_BotItemGenericTrapShield(player, cmd, turnamt, false);
						}
						else
						{
							K_BotItemBanana(player, cmd, turnamt);
						}
						break;
					case KITEM_EGGMAN:
						K_BotItemEggmanShield(player, cmd);
						break;
					case KITEM_ORBINAUT:
						if (!(player->pflags & PF_ITEMOUT))
						{
							K_BotItemGenericOrbitShield(player, cmd);
						}
						else if (player->position != 1) // Hold onto orbiting items when in 1st :)
						/* FALLTHRU */
					case KITEM_BALLHOG:
						{
							K_BotItemOrbinaut(player, cmd);
						}
						break;
					case KITEM_JAWZ:
						if (!(player->pflags & PF_ITEMOUT))
						{
							K_BotItemGenericOrbitShield(player, cmd);
						}
						else if (player->position != 1) // Hold onto orbiting items when in 1st :)
						{
							K_BotItemJawz(player, cmd);
						}
						break;
					case KITEM_MINE:
						if (!(player->pflags & PF_ITEMOUT))
						{
							K_BotItemGenericTrapShield(player, cmd, turnamt, true);
						}
						else
						{
							K_BotItemMine(player, cmd, turnamt);
						}
						break;
					case KITEM_LANDMINE:
						K_BotItemLandmine(player, cmd, turnamt);
						break;
					case KITEM_DROPTARGET:
						if (!(player->pflags & PF_ITEMOUT))
						{
							K_BotItemGenericTrapShield(player, cmd, turnamt, false);
						}
						else
						{
							K_BotItemDropTarget(player, cmd);
						}
						break;
					case KITEM_LIGHTNINGSHIELD:
						K_BotItemLightning(player, cmd);
						break;
					case KITEM_BUBBLESHIELD:
						K_BotItemBubble(player, cmd);
						break;
					case KITEM_FLAMESHIELD:
						K_BotItemFlame(player, cmd);
						break;
				}
			}
		}
	}
}
