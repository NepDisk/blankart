/// \file  k_battle.c
/// \brief SRB2Kart Battle Mode specific code

#include "k_battle.h"
#include "k_boss.h"
#include "k_kart.h"
#include "doomtype.h"
#include "doomdata.h"
#include "g_game.h"
#include "p_mobj.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_slopes.h" // P_GetZAt
#include "r_main.h"
#include "r_defs.h" // MAXFFLOORS
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "r_sky.h" // skyflatnum
#include "k_grandprix.h" // K_CanChangeRules
#include "p_spec.h"

// Battle overtime info
struct battleovertime battleovertime;

// Capsules mode enabled for this map?
boolean battlecapsules = false;

// box respawning in battle mode
INT32 nummapboxes = 0;
INT32 numgotboxes = 0;

// Capsule counters
UINT8 maptargets = 0; // Capsules in map
UINT8 numtargets = 0; // Capsules busted

INT32 K_StartingBumperCount(void)
{
	if (battlecapsules)
		return 1; // always 1 hit in Break the Capsules

	return cv_kartbumpers.value;
}

boolean K_IsPlayerWanted(player_t *player)
{
	UINT8 i = 0, nump = 0, numfirst = 0;
	for (; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		nump++;
		if (players[i].position > 1)
			continue;
		numfirst++;
	}
	return ((numfirst < nump) && !player->spectator && (player->position == 1));
}

void K_SpawnBattlePoints(player_t *source, player_t *victim, UINT8 amount)
{
	statenum_t st;
	mobj_t *pt;

	if (!source || !source->mo)
		return;

	if (amount == 1)
		st = S_BATTLEPOINT1A;
	else if (amount == 2)
		st = S_BATTLEPOINT2A;
	else if (amount == 3)
		st = S_BATTLEPOINT3A;
	else
		return; // NO STATE!

	pt = P_SpawnMobj(source->mo->x, source->mo->y, source->mo->z, MT_BATTLEPOINT);
	P_SetTarget(&pt->target, source->mo);
	P_SetMobjState(pt, st);
	if (victim && victim->skincolor)
		pt->color = victim->skincolor;
	else
		pt->color = source->skincolor;

	if (encoremode)
		pt->renderflags ^= RF_HORIZONTALFLIP;
}

void K_CheckBumpers(void)
{
	UINT8 i;
	UINT8 numingame = 0;
	SINT8 winnernum = -1;
	UINT32 winnerscoreadd = 0, maxroundscore = 0;
	boolean nobumpers = false;

	if (!(gametyperules & GTR_BUMPERS))
		return;

	if (gameaction == ga_completed)
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator) // not even in-game
			continue;

		if (players[i].exiting) // we're already exiting! stop!
			return;

		numingame++;
		winnerscoreadd += players[i].roundscore;

		if (players[i].roundscore > maxroundscore)
		{
			maxroundscore = players[i].roundscore;
		}

		if (players[i].bumpers <= 0) // if you don't have any bumpers, you're probably not a winner
		{
			nobumpers = true;
			continue;
		}
		else if (winnernum != -1) // TWO winners? that's dumb :V
			return;

		winnernum = i;
		winnerscoreadd -= players[i].roundscore;
	}

	if (bossinfo.boss)
	{
		if (nobumpers)
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				players[i].pflags |= PF_NOCONTEST;
				P_DoPlayerExit(&players[i]);
			}
		}
		return;
	}
	else if (numingame <= 1)
	{
		if (!battlecapsules)
		{
			// Reset map to turn on battle capsules
			D_MapChange(gamemap, gametype, encoremode, true, 0, false, false);
		}
		else
		{
			if (nobumpers)
			{
				for (i = 0; i < MAXPLAYERS; i++)
				{
					players[i].pflags |= PF_NOCONTEST;
					P_DoPlayerExit(&players[i]);
				}
			}
		}

		return;
	}

	if (winnernum > -1 && playeringame[winnernum])
	{
		if ((players[winnernum].roundscore+winnerscoreadd) == maxroundscore)
			winnerscoreadd++; // break ties if luigi wins by doing nothing
		players[winnernum].roundscore += winnerscoreadd;
		CONS_Printf(M_GetText("%s recieved %d point%s for winning!\n"), player_names[winnernum], winnerscoreadd, (winnerscoreadd == 1 ? "" : "s"));
	}

	for (i = 0; i < MAXPLAYERS; i++) // This can't go in the earlier loop because winning adds points
		K_KartUpdatePosition(&players[i]);

	for (i = 0; i < MAXPLAYERS; i++) // and it can't be merged with this loop because it needs to be all updated before exiting... multi-loops suck...
	{
		if (!playeringame[i])
			continue;
		if (players[i].spectator)
			continue;
		P_DoPlayerExit(&players[i]);
	}
}

void K_CheckEmeralds(player_t *player)
{
	UINT8 i;

	if (!ALLCHAOSEMERALDS(player->emeralds))
	{
		return;
	}

	player->roundscore++; // lol

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			continue;
		}

		if (&players[i] == player)
		{
			continue;
		}

		players[i].bumpers = 0;
	}

	K_CheckBumpers();
}

mobj_t *K_SpawnChaosEmerald(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT32 emeraldType)
{
	boolean validEmerald = true;
	mobj_t *emerald = P_SpawnMobj(x, y, z, MT_EMERALD);
	mobj_t *overlay;

	P_Thrust(emerald,
		FixedAngle(P_RandomFixed() * 180) + angle,
		24 * mapobjectscale);

	emerald->momz = flip * 24 * mapobjectscale;
	if (emerald->eflags & MFE_UNDERWATER)
		emerald->momz = (117 * emerald->momz) / 200;

	emerald->threshold = 10;

	switch (emeraldType)
	{
		case EMERALD_CHAOS1:
			emerald->color = SKINCOLOR_CHAOSEMERALD1;
			break;
		case EMERALD_CHAOS2:
			emerald->color = SKINCOLOR_CHAOSEMERALD2;
			break;
		case EMERALD_CHAOS3:
			emerald->color = SKINCOLOR_CHAOSEMERALD3;
			break;
		case EMERALD_CHAOS4:
			emerald->color = SKINCOLOR_CHAOSEMERALD4;
			break;
		case EMERALD_CHAOS5:
			emerald->color = SKINCOLOR_CHAOSEMERALD5;
			break;
		case EMERALD_CHAOS6:
			emerald->color = SKINCOLOR_CHAOSEMERALD6;
			break;
		case EMERALD_CHAOS7:
			emerald->color = SKINCOLOR_CHAOSEMERALD7;
			break;
		default:
			CONS_Printf("Invalid emerald type %d\n", emeraldType);
			validEmerald = false;
			break;
	}

	if (validEmerald == true)
	{
		emerald->extravalue1 = emeraldType;
	}

	overlay = P_SpawnMobjFromMobj(emerald, 0, 0, 0, MT_OVERLAY);
	P_SetTarget(&overlay->target, emerald);
	P_SetMobjState(overlay, S_CHAOSEMERALD_UNDER);
	overlay->color = emerald->color;

	return emerald;
}

mobj_t *K_SpawnSphereBox(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT8 amount)
{
	mobj_t *drop = P_SpawnMobj(x, y, z, MT_SPHEREBOX);

	P_InitAngle(drop, angle);
	P_Thrust(drop,
		FixedAngle(P_RandomFixed() * 180) + angle,
		P_RandomRange(4, 12) * mapobjectscale);

	drop->momz = flip * 12 * mapobjectscale;
	if (drop->eflags & MFE_UNDERWATER)
		drop->momz = (117 * drop->momz) / 200;

	drop->flags &= ~(MF_NOGRAVITY|MF_NOCLIPHEIGHT);

	drop->extravalue1 = amount;

	return drop;
}

void K_DropEmeraldsFromPlayer(player_t *player, UINT32 emeraldType)
{
	UINT8 i;
	SINT8 flip = P_MobjFlip(player->mo);

	for (i = 0; i < 14; i++)
	{
		UINT32 emeraldFlag = (1 << i);

		if ((player->emeralds & emeraldFlag) && (emeraldFlag & emeraldType))
		{
			mobj_t *emerald = K_SpawnChaosEmerald(player->mo->x, player->mo->y, player->mo->z, player->mo->angle - ANGLE_90, flip, emeraldFlag);
			P_SetTarget(&emerald->target, player->mo);

			player->emeralds &= ~emeraldFlag;
		}
	}
}

UINT8 K_NumEmeralds(player_t *player)
{
	UINT8 i;
	UINT8 num = 0;

	for (i = 0; i < 14; i++)
	{
		UINT32 emeraldFlag = (1 << i);

		if (player->emeralds & emeraldFlag)
		{
			num++;
		}
	}

	return num;
}

void K_RunPaperItemSpawners(void)
{
	const boolean overtime = (battleovertime.enabled >= 10*TICRATE);
	tic_t interval = 8*TICRATE;

	const boolean canmakeemeralds = true; //(!(battlecapsules || bossinfo.boss));

	UINT32 emeraldsSpawned = 0;
	UINT32 firstUnspawnedEmerald = 0;

	thinker_t *th;
	mobj_t *mo;

	UINT8 pcount = 0;
	INT16 i;

	if (battlecapsules || bossinfo.boss)
	{
		// Gametype uses paper items, but this specific expression doesn't
		return;
	}

	if (leveltime < starttime)
	{
		// Round hasn't started yet!
		return;
	}

	if (overtime == true)
	{
		// Double frequency of items
		interval /= 2;
	}

	if (((leveltime - starttime) % interval) != 0)
	{
		return;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			continue;
		}

		emeraldsSpawned |= players[i].emeralds;

		if ((players[i].exiting > 0 || (players[i].pflags & PF_ELIMINATED))
			|| ((gametyperules & GTR_BUMPERS) && players[i].bumpers <= 0))
		{
			continue;
		}

		pcount++;
	}

	if (overtime == true)
	{
		SINT8 flip = 1;

		// Just find emeralds, no paper spots
		for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
		{
			if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
				continue;

			mo = (mobj_t *)th;

			if (mo->type == MT_EMERALD)
			{
				emeraldsSpawned |= mo->extravalue1;
			}
		}

		if (canmakeemeralds)
		{
			for (i = 0; i < 7; i++)
			{
				UINT32 emeraldFlag = (1 << i);

				if (!(emeraldsSpawned & emeraldFlag))
				{
					firstUnspawnedEmerald = emeraldFlag;
					break;
				}
			}
		}

		if (firstUnspawnedEmerald != 0)
		{
			K_SpawnChaosEmerald(
				battleovertime.x, battleovertime.y, battleovertime.z + (128 * mapobjectscale * flip),
				FixedAngle(P_RandomRange(0, 359) * FRACUNIT), flip,
				firstUnspawnedEmerald
			);
		}
		else
		{
			K_CreatePaperItem(
				battleovertime.x, battleovertime.y, battleovertime.z + (128 * mapobjectscale * flip),
				FixedAngle(P_RandomRange(0, 359) * FRACUNIT), flip,
				0, 0
			);

			if (gametyperules & GTR_SPHERES)
			{
				K_SpawnSphereBox(
					battleovertime.x, battleovertime.y, battleovertime.z + (128 * mapobjectscale * flip),
					FixedAngle(P_RandomRange(0, 359) * FRACUNIT), flip,
					10
				);
			}
		}
	}
	else
	{
		if (pcount > 0)
		{
#define MAXITEM 64
			mobj_t *spotList[MAXITEM];
			UINT8 spotMap[MAXITEM];
			UINT8 spotCount = 0, spotBackup = 0;

			INT16 starti = 0;

			for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
			{
				if (th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed)
					continue;

				mo = (mobj_t *)th;

				if (mo->type == MT_EMERALD)
				{
					emeraldsSpawned |= mo->extravalue1;
				}

				if (mo->type != MT_PAPERITEMSPOT)
					continue;

				if (spotCount >= MAXITEM)
					continue;

				spotList[spotCount] = mo;
				spotMap[spotCount] = spotCount;
				spotCount++;
			}

			if (spotCount <= 0)
			{
				return;
			}

			if (canmakeemeralds)
			{
				for (i = 0; i < 7; i++)
				{
					UINT32 emeraldFlag = (1 << i);

					if (!(emeraldsSpawned & emeraldFlag))
					{
						firstUnspawnedEmerald = emeraldFlag;
						starti = -1;
						break;
					}
				}
			}

			//CONS_Printf("leveltime = %d ", leveltime);

			spotBackup = spotCount;
			for (i = starti; i < pcount; i++)
			{
				UINT8 r = 0, key = 0;
				mobj_t *drop = NULL;
				SINT8 flip = 1;

				if (spotCount == 0)
				{
					// all are accessible again
					spotCount = spotBackup;
				}

				if (spotCount == 1)
				{
					key = 0;
				}
				else
				{
					key = P_RandomKey(spotCount);
				}

				r = spotMap[key];

				//CONS_Printf("[%d %d %d] ", i, key, r);

				flip = P_MobjFlip(spotList[r]);

				// When -1, we're spawning a Chaos Emerald.
				if (i == -1)
				{
					drop = K_SpawnChaosEmerald(
						spotList[r]->x, spotList[r]->y, spotList[r]->z + (128 * mapobjectscale * flip),
						FixedAngle(P_RandomRange(0, 359) * FRACUNIT), flip,
						firstUnspawnedEmerald
					);
				}
				else
				{
					if (gametyperules & GTR_SPHERES)
					{
						drop = K_SpawnSphereBox(
							spotList[r]->x, spotList[r]->y, spotList[r]->z + (128 * mapobjectscale * flip),
								FixedAngle(P_RandomRange(0, 359) * FRACUNIT), flip,
								10
						);
						K_FlipFromObject(drop, spotList[r]);
					}

					drop = K_CreatePaperItem(
						spotList[r]->x, spotList[r]->y, spotList[r]->z + (128 * mapobjectscale * flip),
						FixedAngle(P_RandomRange(0, 359) * FRACUNIT), flip,
						0, 0
					);
				}

				K_FlipFromObject(drop, spotList[r]);

				spotCount--;
				if (key != spotCount)
				{
					// So the core theory of what's going on is that we keep every
					// available option at the front of the array, so we don't have
					// to skip over any gaps or do recursion to avoid doubles.
					// But because spotCount can be reset in the case of a low
					// quanitity of item spawnpoints in a map, we still need every
					// entry in the array, even outside of the "visible" range.
					// A series of swaps allows us to adhere to both constraints.
					// -toast 22/03/22 (semipalindromic!)
					spotMap[key] = spotMap[spotCount];
					spotMap[spotCount] = r; // was set to spotMap[key] previously
				}
			}
			//CONS_Printf("\n");
		}
	}
}

static void K_SpawnOvertimeLaser(fixed_t x, fixed_t y, fixed_t scale)
{
	UINT8 i, j;

	for (i = 0; i <= r_splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];
		fixed_t zpos;
		SINT8 flip;

		if (player == NULL || player->mo == NULL || P_MobjWasRemoved(player->mo) == true)
		{
			continue;
		}

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			zpos = player->mo->z + player->mo->height;
		}
		else
		{
			zpos = player->mo->z;
		}

		flip = P_MobjFlip(player->mo);

		for (j = 0; j < 3; j++)
		{
			mobj_t *mo = P_SpawnMobj(x, y, zpos, MT_OVERTIME_PARTICLE);

			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				mo->flags2 |= MF2_OBJECTFLIP;
				mo->eflags |= MFE_VERTICALFLIP;
			}

			P_InitAngle(mo, R_PointToAngle2(mo->x, mo->y, battleovertime.x, battleovertime.y) + ANGLE_90);
			mo->renderflags |= (RF_DONTDRAW & ~(K_GetPlayerDontDrawFlag(player)));

			P_SetScale(mo, scale);

			switch (j)
			{
				case 0:
					P_SetMobjState(mo, S_OVERTIME_BULB1);

					if (leveltime & 1)
						mo->frame += 1;

					//P_SetScale(mo, mapobjectscale);
					zpos += 35 * mo->scale * flip;
					break;
				case 1:
					P_SetMobjState(mo, S_OVERTIME_LASER);

					if (leveltime & 1)
						mo->frame += 3;
					else
						mo->frame += (leveltime / 2) % 3;

					//P_SetScale(mo, scale);
					zpos += 346 * mo->scale * flip;

					if (battleovertime.enabled < 10*TICRATE)
						mo->renderflags |= RF_TRANS50;
					break;
				case 2:
					P_SetMobjState(mo, S_OVERTIME_BULB2);

					if (leveltime & 1)
						mo->frame += 1;

					//P_SetScale(mo, mapobjectscale);
					break;
				default:
					I_Error("Bruh moment has occured\n");
					return;
			}
		}
	}
}

void K_RunBattleOvertime(void)
{
	if (battleovertime.enabled < 10*TICRATE)
	{
		battleovertime.enabled++;
		if (battleovertime.enabled == TICRATE)
			S_StartSound(NULL, sfx_bhurry);
		if (battleovertime.enabled == 10*TICRATE)
			S_StartSound(NULL, sfx_kc40);
	}
	else if (battleovertime.radius > 0)
	{
		const fixed_t minradius = 768 * mapobjectscale;

		if (battleovertime.radius > minradius)
			battleovertime.radius -= 2*mapobjectscale;
		else
			battleovertime.radius = minradius;
	}

	if (battleovertime.radius > 0)
	{
		const INT32 orbs = 32;
		const angle_t angoff = ANGLE_MAX / orbs;
		const UINT8 spriteSpacing = 128;

		fixed_t circumference = FixedMul(M_PI_FIXED, battleovertime.radius * 2);
		fixed_t scale = max(circumference / spriteSpacing / orbs, mapobjectscale);

		fixed_t size = FixedMul(mobjinfo[MT_OVERTIME_PARTICLE].radius, scale);
		fixed_t posOffset = max(battleovertime.radius - size, 0);

		INT32 i;

		for (i = 0; i < orbs; i++)
		{
			angle_t ang = (i * angoff) + FixedAngle((leveltime * FRACUNIT) / 4);

			fixed_t x = battleovertime.x + P_ReturnThrustX(NULL, ang, posOffset);
			fixed_t y = battleovertime.y + P_ReturnThrustY(NULL, ang, posOffset);

			K_SpawnOvertimeLaser(x, y, scale);
		}
	}
}

void K_SetupMovingCapsule(mapthing_t *mt, mobj_t *mobj)
{
	UINT8 sequence = mt->args[0] - 1;
	fixed_t speed = (FRACUNIT >> 3) * mt->args[1];
	boolean backandforth = (mt->args[2] & TMBCF_BACKANDFORTH);
	boolean reverse = (mt->args[2] & TMBCF_REVERSE);
	mobj_t *target = NULL;

	// Find the inital target
	if (reverse)
	{
		target = P_GetLastTubeWaypoint(sequence);
	}
	else
	{
		target = P_GetFirstTubeWaypoint(sequence);
	}

	if (!target)
	{
		CONS_Alert(CONS_WARNING, "No target waypoint found for moving capsule (seq: #%d)\n", sequence);
		return;
	}

	P_SetTarget(&mobj->target, target);
	mobj->lastlook = sequence;
	mobj->movecount = target->health;
	mobj->movefactor = speed;

	if (backandforth) {
		mobj->flags2 |= MF2_AMBUSH;
	} else {
		mobj->flags2 &= ~MF2_AMBUSH;
	}

	if (reverse) {
		mobj->cvmem = -1;
	} else {
		mobj->cvmem = 1;
	}
}

void K_SpawnPlayerBattleBumpers(player_t *p)
{
	if (!p->mo || p->bumpers <= 0)
		return;

	{
		INT32 i;
		angle_t diff = FixedAngle(360*FRACUNIT/p->bumpers);
		angle_t newangle = p->mo->angle;
		mobj_t *bump;

		for (i = 0; i < p->bumpers; i++)
		{
			bump = P_SpawnMobjFromMobj(p->mo,
				P_ReturnThrustX(p->mo, newangle + ANGLE_180, 64*FRACUNIT),
				P_ReturnThrustY(p->mo, newangle + ANGLE_180, 64*FRACUNIT),
				0, MT_BATTLEBUMPER);
			bump->threshold = i;
			P_SetTarget(&bump->target, p->mo);
			bump->angle = newangle;
			bump->color = p->mo->color;
			if (p->mo->renderflags & RF_DONTDRAW)
				bump->renderflags |= RF_DONTDRAW;
			else
				bump->renderflags &= ~RF_DONTDRAW;
			newangle += diff;
		}
	}
}

void K_BattleInit(void)
{
	size_t i;

	if ((gametyperules & GTR_CAPSULES) && !battlecapsules && !bossinfo.boss)
	{
		mapthing_t *mt;
		if (modeattacking != ATTACKING_CAPSULES)
		{
			UINT8 n = 0;

			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i] || players[i].spectator)
					continue;
				n++;
			}

			if (n > 1)
				goto aftercapsules;
		}

		mt = mapthings;
		for (i = 0; i < nummapthings; i++, mt++)
		{
			if (mt->type == mobjinfo[MT_BATTLECAPSULE].doomednum)
				P_SpawnMapThing(mt);
		}

		battlecapsules = true;
	}
aftercapsules:

	if (gametyperules & GTR_BUMPERS)
	{
		INT32 maxbumpers = K_StartingBumperCount();

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;
			players[i].bumpers = maxbumpers;
			K_SpawnPlayerBattleBumpers(players+i);
		}
	}
}
