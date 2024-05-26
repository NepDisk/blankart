// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_control.h"
#include "../../k_respawn.h"


boolean K_PlayerCanTurn(const player_t* player)
{
	if (player->mo && player->speed > 0) // Moving
		return true;

	if (leveltime > starttime && (player->cmd.buttons & BT_ACCELERATE && player->cmd.buttons & BT_BRAKE)) // Rubber-burn turn
		return true;

	if (player->respawn.state == RESPAWNST_DROP) // Respawning
		return true;

	if (player->spectator || objectplacing) // Not a physical player
		return true;

	return false;
}

// countersteer is how strong the controls are telling us we are turning
// turndir is the direction the controls are telling us to turn, -1 if turning right and 1 if turning left
static INT16 K_GetKartDriftValue(const player_t *player, fixed_t countersteer)
{
	INT16 basedrift, driftangle;
	fixed_t driftweight = player->kartweight*14; // 12

	// If they aren't drifting or on the ground this doesn't apply
	if (player->drift == 0 || !P_IsObjectOnGround(player->mo))
		return 0;

	if (player->pflags & PF_DRIFTEND)
	{
		return -266*player->drift; // Drift has ended and we are tweaking their angle back a bit
	}

	basedrift = 83*player->drift - (driftweight - 14)*player->drift/5; // 415 - 303
	driftangle = abs((252 - driftweight)*player->drift/5);

	return basedrift + FixedMul(driftangle, countersteer);
}

//from sarb2kart itself but modifed for v2
INT16 K_GetKartTurnValue(const player_t *player, INT16 turnvalue)
{
	fixed_t p_topspeed = K_GetKartSpeed(player, false, false);
	fixed_t p_curspeed = min(player->speed, p_topspeed * 2);
	fixed_t p_maxspeed = p_topspeed * 3;
	fixed_t adjustangle = FixedDiv((p_maxspeed>>16) - (p_curspeed>>16), (p_maxspeed>>16) + player->kartweight);

	if (player->spectator)
		return turnvalue;

	if (player->drift != 0 && P_IsObjectOnGround(player->mo))
	{
		// If we're drifting we have a completely different turning value
		if (!(player->pflags & PF_DRIFTEND))
		{
			// 800 is the max set in g_game.c with angleturn
			fixed_t countersteer = FixedDiv(turnvalue*FRACUNIT, 800*FRACUNIT);
			turnvalue = K_GetKartDriftValue(player, countersteer);
		}
		else
			turnvalue = (INT16)(turnvalue + K_GetKartDriftValue(player, FRACUNIT));

		return turnvalue;
	}

	turnvalue = FixedMul(turnvalue, adjustangle); // Weight has a small effect on turning

	if (player->invincibilitytimer || player->sneakertimer || player->growshrinktimer > 0)
		turnvalue = FixedMul(turnvalue, FixedDiv(5*FRACUNIT, 4*FRACUNIT));

	return turnvalue;
}

void KV1_UpdatePlayerAngle(player_t* player)
{
	INT16 angle_diff, max_left_turn, max_right_turn;
	boolean add_delta = true;

	// Kart: store the current turn range for later use
	if (K_PlayerCanTurn(player))
	{
		player->lturn_max[leveltime%MAXPREDICTTICS] = K_GetKartTurnValue(player, KART_FULLTURN)+1;
		player->rturn_max[leveltime%MAXPREDICTTICS] = K_GetKartTurnValue(player, -KART_FULLTURN)-1;
	} else {
		player->lturn_max[leveltime%MAXPREDICTTICS] = player->rturn_max[leveltime%MAXPREDICTTICS] = 0;
	}

	if (leveltime >= starttime)
	{
		// KART: Don't directly apply angleturn! It may have been either A) forged by a malicious client, or B) not be a smooth turn due to a player dropping frames.
		// Instead, turn the player only up to the amount they're supposed to turn accounting for latency. Allow exactly 1 extra turn unit to try to keep old replays synced.
		angle_diff = player->cmd.angleturn - (player->mo->angle>>16);
		max_left_turn = player->lturn_max[(leveltime + MAXPREDICTTICS - player->cmd.latency) % MAXPREDICTTICS];
		max_right_turn = player->rturn_max[(leveltime + MAXPREDICTTICS - player->cmd.latency) % MAXPREDICTTICS];

		CONS_Printf("----------------\nangle diff: %d - turning options: %d to %d - ", angle_diff, max_left_turn, max_right_turn);

		if (angle_diff > max_left_turn)
			angle_diff = max_left_turn;
		else if (angle_diff < max_right_turn)
			angle_diff = max_right_turn;
		else
		{
			// Try to keep normal turning as accurate to 1.0.1 as possible to reduce replay desyncs.
			player->mo->angle = player->cmd.angleturn<<16;
			add_delta = false;
		}
		CONS_Printf("applied turn: %d\n", angle_diff);

		if (add_delta) {
			player->mo->angle += angle_diff<<16;
			player->mo->angle &= ~0xFFFF; // Try to keep the turning somewhat similar to how it was before?
			CONS_Printf("leftover turn (%s): %5d or %4d%%\n",
							player_names[player-players],
							(INT16) (player->cmd.angleturn - (player->mo->angle>>16)),
							(INT16) (player->cmd.angleturn - (player->mo->angle>>16)) * 100 / (angle_diff ? angle_diff : 1));
		}
	}
}

void P_UpdateBotAngle(player_t *player)
{
	angle_t angleChange = K_GetKartTurnValue(player, player->cmd.driftturn) << TICCMD_REDUCE;
	UINT8 i;


	player->cmd.angleturn += angleChange;
	player->mo->angle = player->cmd.angleturn;

	player->aiming += (player->cmd.aiming << TICCMD_REDUCE);
	player->aiming = G_ClipAimingPitch((INT32 *)&player->aiming);

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			localaiming[i] = player->aiming;
			// SRB2kart - no additional angle if not moving
			if (K_PlayerCanTurn(player))
				localangle[i] += (player->cmd.angleturn<<16);
			break;
		}
	}
}

void N_PogoSidemove(player_t *player)
{
	fixed_t movepushside = 0;
	angle_t movepushangle = 0, movepushsideangle = 0;
	fixed_t sidemove[2] = {2<<FRACBITS>>16, 4<<FRACBITS>>16};
	fixed_t side = player->pogosidemove;

	if (player->drift != 0)
		movepushangle = player->mo->angle-(ANGLE_45/5)*player->drift;
	else
		movepushangle = player->mo->angle;

	movepushsideangle = movepushangle-ANGLE_90;

	// let movement keys cancel each other out
	if (player->cmd.driftturn < 0)
	{;
		side += sidemove[1];
	}
	else if (player->cmd.driftturn > 0 )
	{
		side -= sidemove[1];
	}

	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	if (side !=0 && (!player->pogospring))
		side = 0;

	// Sideways movement
	if (side != 0 && !((player->exiting || mapreset)))
	{
		if (side > 0)
			movepushside = (side * FRACUNIT/128) + FixedDiv(player->speed, K_GetKartSpeed(player, true, false));
		else
			movepushside = (side * FRACUNIT/128) - FixedDiv(player->speed, K_GetKartSpeed(player, true, false));

		player->mo->momx += P_ReturnThrustX(player->mo, movepushsideangle, movepushside);
		player->mo->momy += P_ReturnThrustY(player->mo, movepushsideangle, movepushside);
	}

}

void N_LegacyStart(player_t *player)
{

	if (leveltime <= starttime)
		player->nocontrol = 1;

	// Start charging once you're given the opportunity.
	if (leveltime >= starttime-(2*TICRATE) && leveltime <= starttime)
	{
		if (player->cmd.buttons & BT_ACCELERATE)
		{
			if (player->boostcharge == 0)
				player->boostcharge = player->cmd.latency;

			player->boostcharge++;
		}
		else
			player->boostcharge = 0;
	}

	// Increase your size while charging your engine.
	if (leveltime < starttime+10)
	{
		player->mo->scalespeed = mapobjectscale/12;
		player->mo->destscale = mapobjectscale + (FixedMul(mapobjectscale, player->boostcharge*131));
		if (K_PlayerShrinkCheat(player) && (modeattacking == ATTACKING_NONE) && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	// Determine the outcome of your charge.
	if (leveltime > starttime && player->boostcharge)
	{
		// Not even trying?
		if (player->boostcharge < 35)
		{
			if (player->boostcharge > 17)
				S_StartSound(player->mo, sfx_cdfm00); // chosen instead of a conventional skid because it's more engine-like
		}
		// Get an instant boost!
		else if (player->boostcharge <= 50)
		{
			player->startboost = (50-player->boostcharge)+20;

			if (player->boostcharge <= 36)
			{
				player->startboost = 0;
				K_DoSneaker(player, 0);
				player->sneakertimer = 70; // PERFECT BOOST!!

				if (!player->floorboost || player->floorboost == 3) // Let everyone hear this one
					S_StartSound(player->mo, sfx_s25f);
			}
			else
			{
				K_SpawnDashDustRelease(player); // already handled for perfect boosts by K_DoSneaker
				if ((!player->floorboost || player->floorboost == 3) && P_IsPartyPlayer(player))
				{
					if (player->boostcharge <= 40)
						S_StartSound(player->mo, sfx_cdfm01); // You were almost there!
					else
						S_StartSound(player->mo, sfx_s23c); // Nope, better luck next time.
				}
			}
		}
		// You overcharged your engine? Those things are expensive!!!
		else if (player->boostcharge > 50)
		{
			player->nocontrol = 40;
			//S_StartSound(player->mo, sfx_kc34);
			S_StartSound(player->mo, sfx_s3k83);
			player->pflags |= PF_SKIDDOWN; // cheeky pflag reuse
		}

		player->boostcharge = 0;
	}

}

