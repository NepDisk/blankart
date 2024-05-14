// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by KartKrew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_boosts.h"

fixed_t N_GetKartSpeed(player_t *player, boolean doboostpower)
{
	fixed_t k_speed = 150;
	fixed_t g_cc = FRACUNIT;
	fixed_t xspd = 3072;		// 4.6875 aka 3/64
	UINT8 kartspeed = player->kartspeed;
	fixed_t finalspeed;

	switch (gamespeed)
	{
		case 0:
			g_cc = 65536 + xspd; // 100cc = 100.00 + 4.69 = 104.69%
			break;
		//expert speed
		case 2:
			g_cc = 90112 + xspd; // 150cc = 118.75 + 4.69 = 123.44%
			break;
		default:
			g_cc = 77824 + xspd; // 150cc = 118.75 + 4.69 = 123.44%
			break;
	}
	k_speed += kartspeed*3; // 153 - 177

	finalspeed = FixedMul(FixedMul(k_speed<<14, g_cc), player->mo->scale);

	if (doboostpower)
		return FixedMul(finalspeed, player->boostpower+player->speedboost);
	return finalspeed;
}

fixed_t N_GetKartAccel(player_t *player)
{
	fixed_t k_accel = 32; // 36;
	UINT8 kartspeed = player->kartspeed;

	k_accel += 4 * (9 - kartspeed); // 32 - 64

	return FixedMul(k_accel, FRACUNIT+player->accelboost);
}

// v2 almost broke sliptiding when it fixed turning bugs!
// This value is fine-tuned to feel like v1 again without reverting any of those changes.
#define SLIPTIDEHANDLING 7*FRACUNIT/8

static boolean K_HasInfiniteTether(player_t *player)
{
	switch (player->curshield)
	{
		case KSHIELD_LIGHTNING:
			return true;
	}

	if (player->eggmanexplode > 0)
		return true;

	if (player->trickcharge)
		return true;

	if (player->infinitether)
		return true;

	return false;
}

static fixed_t K_RingDurationBoost(const player_t *player)
{
	fixed_t ret = FRACUNIT;

	if (K_PlayerUsesBotMovement(player))
	{
		// x2.0 for Lv. 9
		const fixed_t modifier = K_BotMapModifier();
		fixed_t add = ((player->botvars.difficulty-1) * modifier) / (DIFFICULTBOT-1);

		ret += add;

		if (player->botvars.rival == true || cv_levelskull.value)
		{
			// x2.0 for Rival
			ret *= 2;
		}
	}

	return ret;
}

void N_GetKartBoostPower(player_t *player)
{
	fixed_t boostpower = FRACUNIT;
	fixed_t speedboost = 0, accelboost = 0, handleboost = 0;
	UINT8 numboosts = 0;

	if (player->spinouttimer && player->wipeoutslow == 1) // Slow down after you've been bumped
	{
		player->boostpower = player->speedboost = player->accelboost = 0;
		return;
	}

	// Offroad is separate, it's difficult to factor it in with a variable value anyway.
	if (!(player->invincibilitytimer || player->hyudorotimer || player->sneakertimer) && player->offroad >= 0)
		boostpower = FixedDiv(boostpower, player->offroad + FRACUNIT);

	if (player->bananadrag > TICRATE)
		boostpower = (4*boostpower)/5;

	// Banana drag/offroad dust
	if (boostpower < FRACUNIT
		&& player->mo && P_IsObjectOnGround(player->mo)
		&& player->speed > 0
		&& !player->spectator)
	{
		K_SpawnWipeoutTrail(player->mo);
		if (leveltime % 6 == 0)
			S_StartSound(player->mo, sfx_cdfm70);
	}

#define ADDBOOST(s,a,h) { \
	numboosts++; \
	speedboost += max(s, speedboost); \
	accelboost += max(a, accelboost); \
	handleboost = max(h, handleboost); \
}

	if (player->sneakertimer) // Sneaker
	{
		fixed_t sneakerspeedboost = 0;
		switch (gamespeed)
		{
			case 0:
				sneakerspeedboost = 32768;
				break;
			case 2:
				sneakerspeedboost = 14706;
				break;
			default:
				sneakerspeedboost = 17294+768;
				break;
		}
			ADDBOOST(sneakerspeedboost,8*FRACUNIT,SLIPTIDEHANDLING)
	}

	if (player->invincibilitytimer) // Invincibility
	{
		ADDBOOST(3*FRACUNIT/8,3*FRACUNIT,0)
	}

	if (player->growshrinktimer > 0) // Grow
	{
		ADDBOOST(FRACUNIT/5,0,0)
	}

	if (player->driftboost) // Drift Boost
	{
		ADDBOOST(FRACUNIT/4,4*FRACUNIT,0)
	}

	if (player->startboost) // Startup Boost
	{
		ADDBOOST(FRACUNIT, 4*FRACUNIT, 0);
	}

	if (player->dropdashboost) // Drop dash
	{
		ADDBOOST(FRACUNIT/4, 6*FRACUNIT, 0);
	}

		if (player->trickboost)	// Trick pannel up-boost
	{
		ADDBOOST(player->trickboostpower, 5*FRACUNIT, 0);	// <trickboostpower>% speed, 500% accel, 0% handling
	}

	if (player->gateBoost) // SPB Juicebox boost
	{
		ADDBOOST(3*FRACUNIT/4, 4*FRACUNIT, 0); // + 75% top speed, + 400% acceleration, +25% handling
	}

	if (player->ringboost) // Ring Boost
	{
		// This one's a little special: we add extra top speed per tic of ringboost stored up, to allow for Ring Box to really rocket away.
		// (We compensate when decrementing ringboost to avoid runaway exponential scaling hell.)
		fixed_t rb = FixedDiv(player->ringboost * FRACUNIT, max(FRACUNIT, K_RingDurationBoost(player)));
		ADDBOOST(
			FRACUNIT/4 + FixedMul(FRACUNIT / 1750, rb),
			4*FRACUNIT,
			Easing_InCubic(min(FRACUNIT, rb / (TICRATE*12)), 0, 0)
		); // + 20% + ???% top speed, + 400% acceleration, +???% handling
	}

	if (player->eggmanexplode) // Ready-to-explode
	{
		ADDBOOST(6*FRACUNIT/20, FRACUNIT, 0); // + 30% top speed, + 100% acceleration, +0% handling
	}

	if (player->trickcharge)
	{
		// NB: This is an acceleration-only boost.
		// If this is applied earlier in the chain, it will diminish real speed boosts.
		ADDBOOST(0, FRACUNIT, 0); // 0% speed 100% accel 20% handle
	}

	// This should always remain the last boost stack before tethering
	if (player->botvars.rubberband > FRACUNIT && K_PlayerUsesBotMovement(player) == true && cv_ng_botrubberbandboost.value)
	{
		ADDBOOST(player->botvars.rubberband - FRACUNIT, 0, 0);
	}

	if (player->draftpower > 0) // Drafting
	{
		// 30% - 44%, each point of speed adds 1.75%
		fixed_t draftspeed = ((3*FRACUNIT)/10) + ((player->kartspeed-1) * ((7*FRACUNIT)/400));

		if (gametyperules & GTR_CLOSERPLAYERS)
		{
			draftspeed *= 2;
		}

		if (K_HasInfiniteTether(player))
		{
			// infinite tether
			draftspeed *= 2;
		}

		speedboost += FixedMul(draftspeed, player->draftpower); // (Drafting suffers no boost stack penalty.)
		numboosts++;
	}


	// don't average them anymore, this would make a small boost and a high boost less useful
	// just take the highest we want instead
	player->boostpower = boostpower;

	// value smoothing
	if (speedboost > player->speedboost)
		player->speedboost = speedboost;
	//brakemod. slowdown on braking or sliptide (based on version from booststack)
	else if ((player->aizdriftstrat && abs(player->drift) < 5) || (player->cmd.buttons & BT_BRAKE))
		player->speedboost = max(speedboost - FloatToFixed(0.05), min(player->speedboost, 3*FRACUNIT/8));
	else
		player->speedboost += (speedboost - player->speedboost)/(TICRATE/2);

	player->accelboost = accelboost;
	player->handleboost = handleboost;

}


fixed_t N_3dKartMovement(const player_t *player, fixed_t forwardmove)
{
	fixed_t accelmax = 4000;
	fixed_t newspeed, oldspeed, finalspeed;
	fixed_t p_speed = K_GetKartSpeed(player, true, false);
	fixed_t p_accel = K_GetKartAccel(player);

	if (!P_IsObjectOnGround(player->mo)) return 0; // If the player isn't on the ground, there is no change in speed

	// ACCELCODE!!!1!11!
	oldspeed = R_PointToDist2(0, 0, player->rmomx, player->rmomy); // FixedMul(P_AproxDistance(player->rmomx, player->rmomy), player->mo->scale);
	newspeed = FixedDiv(FixedDiv(FixedMul(oldspeed, accelmax - p_accel) + FixedMul(p_speed, p_accel), accelmax), ORIG_FRICTION);

	finalspeed = newspeed - oldspeed;

	// forwardmove is:
	//  50 while accelerating,
	//  25 while clutching,
	//   0 with no gas, and
	// -25 when only braking.

	finalspeed *= forwardmove/25;
	finalspeed /= 2;

	if (forwardmove < 0 && finalspeed > mapobjectscale*2)
		return finalspeed/2;
	else if (forwardmove < 0)
		return -mapobjectscale/2;

	if (finalspeed < 0)
		finalspeed = 0;

	return finalspeed;
}
