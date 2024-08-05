// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2018-2020 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2018-2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_grandprix.c
/// \brief Grand Prix mode game logic & bot behaviors

#include "k_grandprix.h"
#include "k_boss.h"
#include "doomdef.h"
#include "d_player.h"
#include "g_game.h"
#include "k_bot.h"
#include "k_kart.h"
#include "m_random.h"
#include "r_things.h"

struct grandprixinfo grandprixinfo;

/*--------------------------------------------------
	UINT8 K_BotStartingDifficulty(SINT8 value)

		See header file for description.
--------------------------------------------------*/
UINT8 K_BotStartingDifficulty(SINT8 value)
{
	// startingdifficulty: Easy = 3, Normal = 6, Hard = 9
	SINT8 difficulty = (value + 1) * 3;

	if (difficulty > MAXBOTDIFFICULTY)
	{
		difficulty = MAXBOTDIFFICULTY;
	}
	else if (difficulty < 1)
	{
		difficulty = 1;
	}

	return difficulty;
}

/*--------------------------------------------------
	INT16 K_CalculateGPRankPoints(UINT8 position, UINT8 numplayers)

		See header file for description.
--------------------------------------------------*/
INT16 K_CalculateGPRankPoints(UINT8 position, UINT8 numplayers)
{
	INT16 points;

	if (position >= numplayers || position == 0)
	{
		// Invalid position, no points
		return 0;
	}

	points = numplayers - position;

	// Give bonus to high-ranking players, depending on player count
	// This rounds out the point gain when you get 1st every race,
	// and gives bots able to catch up in points if a player gets an early lead.
	// The maximum points you can get in a cup is: ((number of players - 1) + (max extra points)) * (number of races)
	// 8P: (7 + 5) * 5 = 60 maximum points
	// 12P: (11 + 5) * 5 = 80 maximum points
	// 16P: (15 + 5) * 5 = 100 maximum points
	switch (numplayers)
	{
		case 0: case 1: case 2: // 1v1
			break; // No bonus needed.
		case 3: case 4: // 3-4P
			if (position == 1) { points += 1; } // 1st gets +1 extra point
			break;
		case 5: case 6: // 5-6P
			if (position == 1) { points += 3; } // 1st gets +3 extra points
			else if (position == 2) { points += 1; } // 2nd gets +1 extra point
			break;
		default: // Normal matches
			if (position == 1) { points += 5; } // 1st gets +5 extra points
			else if (position == 2) { points += 3; } // 2nd gets +3 extra points
			else if (position == 3) { points += 1; } // 3rd gets +1 extra point
			break;
	}

	// somehow underflowed?
	if (points < 0)
	{
		points = 0;
	}

	return points;
}

/*--------------------------------------------------
	UINT8 K_BotDefaultSkin(void)

		See header file for description.
--------------------------------------------------*/
UINT8 K_BotDefaultSkin(void)
{
	const char *defaultbotskinname = "eggrobo";
	INT32 defaultbotskin = R_SkinAvailable(defaultbotskinname);

	if (defaultbotskin == -1)
	{
		// This shouldn't happen, but just in case
		defaultbotskin = 0;
	}

	return (UINT8)defaultbotskin;
}

/*--------------------------------------------------
	void K_InitGrandPrixBots(void)

		See header file for description.
--------------------------------------------------*/
void K_InitGrandPrixBots(void)
{
	const UINT8 defaultbotskin = K_BotDefaultSkin();

	const UINT8 startingdifficulty = K_BotStartingDifficulty(grandprixinfo.gamespeed);
	UINT8 difficultylevels[MAXPLAYERS];

	UINT8 playercount = 8;
	UINT8 wantedbots = 0;

	UINT8 numplayers = 0;
	UINT8 competitors[MAXSPLITSCREENPLAYERS];

	UINT8 usableskins;
	UINT8 grabskins[MAXSKINS+1];

	UINT8 botskinlist[MAXPLAYERS];
	UINT8 botskinlistpos = 0;

	UINT8 newplayernum = 0;
	UINT8 i, j;

	memset(competitors, MAXPLAYERS, sizeof (competitors));
	memset(botskinlist, defaultbotskin, sizeof (botskinlist));

	// Init usable bot skins list
	for (usableskins = 0; usableskins < numskins; usableskins++)
	{
		grabskins[usableskins] = usableskins;
	}
	grabskins[usableskins] = MAXSKINS;

#if MAXPLAYERS != 16
	I_Error("GP bot difficulty levels need rebalanced for the new player count!\n");
#endif

	if (grandprixinfo.masterbots)
	{
		// Everyone is max difficulty!!
		memset(difficultylevels, MAXBOTDIFFICULTY, sizeof (difficultylevels));
	}
	else
	{
		// init difficulty levels list
		difficultylevels[0] = max(1, startingdifficulty);
		difficultylevels[1] = max(1, startingdifficulty-1);
		difficultylevels[2] = max(1, startingdifficulty-2);
		difficultylevels[3] = max(1, startingdifficulty-3);
		difficultylevels[4] = max(1, startingdifficulty-3);
		difficultylevels[5] = max(1, startingdifficulty-4);
		difficultylevels[6] = max(1, startingdifficulty-4);
		difficultylevels[7] = max(1, startingdifficulty-4);
		difficultylevels[8] = max(1, startingdifficulty-5);
		difficultylevels[9] = max(1, startingdifficulty-5);
		difficultylevels[10] = max(1, startingdifficulty-5);
		difficultylevels[11] = max(1, startingdifficulty-6);
		difficultylevels[12] = max(1, startingdifficulty-6);
		difficultylevels[13] = max(1, startingdifficulty-7);
		difficultylevels[14] = max(1, startingdifficulty-7);
		difficultylevels[15] = max(1, startingdifficulty-8);
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			if (numplayers < MAXSPLITSCREENPLAYERS && !players[i].spectator)
			{
				competitors[numplayers] = i;
				grabskins[players[i].skin] = MAXSKINS;
				numplayers++;
			}
			else
			{
				players[i].spectator = true; // force spectate for all other players, if they happen to exist?
			}
		}
	}

	if (numplayers > 2)
	{
		// Add 3 bots per player beyond 2P
		playercount += (numplayers-2) * 3;
	}

	if (numbosswaypoints > 0)
	{
		CONS_Alert(CONS_ERROR, "Bots do not work on maps using the legacy checkpoint system.\nPlease consider using waypoints instead if bot support is desired!\n");
		wantedbots = 0;
	}
	else
	{
	
		wantedbots = playercount - numplayers;
	}

	// Create rival list
	if (numplayers > 0)
	{
		for (i = 0; i < SKINRIVALS; i++)
		{
			for (j = 0; j < numplayers; j++)
			{
				player_t *p = &players[competitors[j]];
				char *rivalname = skins[p->skin].rivals[i];
				INT32 rivalnum = R_SkinAvailable(rivalname);

				// Intentionally referenced before (currently dummied out) unlock check. Such a tease!
				if (rivalnum != -1 && grabskins[(UINT8)rivalnum] != MAXSKINS)
				{
					botskinlist[botskinlistpos++] = (UINT8)rivalnum;
					grabskins[(UINT8)rivalnum] = MAXSKINS;
				}
			}
		}
	}

	// Rearrange usable bot skins list to prevent gaps for randomised selection
	for (i = 0; i < usableskins; i++)
	{
		if (!(grabskins[i] == MAXSKINS /*|| K_SkinLocked(grabskins[i])*/))
			continue;
		while (usableskins > i && (grabskins[usableskins] == MAXSKINS /*|| K_SkinLocked(grabskins[i])*/))
		{
			usableskins--;
		}
		grabskins[i] = grabskins[usableskins];
		grabskins[usableskins] = MAXSKINS;
	}

	// Pad the remaining list with random skins if we need to
	if (botskinlistpos < wantedbots)
	{
		while (botskinlistpos < wantedbots)
		{
			UINT8 skinnum = defaultbotskin;

			if (usableskins > 0)
			{
				UINT8 index = M_RandomKey(usableskins);
				skinnum = grabskins[index];
				grabskins[index] = grabskins[--usableskins];
			}

			botskinlist[botskinlistpos++] = skinnum;
		}
	}

	for (i = 0; i < wantedbots; i++)
	{
		if (!K_AddBot(botskinlist[i], difficultylevels[i], &newplayernum))
		{
			break;
		}
	}
}

/*--------------------------------------------------
	static INT16 K_RivalScore(player_t *bot)

		Creates a "rival score" for a bot, used to determine which bot is the
		most deserving of the rival status.

	Input Arguments:-
		bot - Player to check.

	Return:-
		"Rival score" value.
--------------------------------------------------*/
static INT16 K_RivalScore(player_t *bot)
{
	const UINT16 difficulty = bot->botvars.difficulty;
	const UINT16 score = bot->score;
	SINT8 roundnum = 1, roundsleft = 1;
	UINT16 lowestscore = UINT16_MAX;
	UINT8 lowestdifficulty = MAXBOTDIFFICULTY;
	UINT8 i;

	if (grandprixinfo.cup != NULL)
	{
		roundnum = grandprixinfo.roundnum;
		roundsleft = grandprixinfo.cup->numlevels - grandprixinfo.roundnum;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			continue;
		}

		if (players[i].score < lowestscore)
		{
			lowestscore = players[i].score;
		}

		if (players[i].bot == true && players[i].botvars.difficulty < lowestdifficulty)
		{
			lowestdifficulty = players[i].botvars.difficulty;
		}
	}

	// In the early game, difficulty is more important.
	// This will try to influence the higher difficulty bots to get rival more often & get even more points.
	// However, when we're running low on matches left, we need to focus more on raw score!

	return ((difficulty - lowestdifficulty) * roundsleft) + ((score - lowestscore) * roundnum);
}

/*--------------------------------------------------
	void K_UpdateGrandPrixBots(void)

		See header file for description.
--------------------------------------------------*/
void K_UpdateGrandPrixBots(void)
{
	player_t *oldrival = NULL;
	player_t *newrival = NULL;
	UINT16 newrivalscore = 0;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].bot)
		{
			continue;
		}

		players[i].spectator = (grandprixinfo.eventmode != GPEVENT_NONE);
	}

	// Find the rival.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].bot)
		{
			continue;
		}

		if (players[i].botvars.diffincrease)
		{
			players[i].botvars.difficulty += players[i].botvars.diffincrease;

			if (players[i].botvars.difficulty > MAXBOTDIFFICULTY)
			{
				players[i].botvars.difficulty = MAXBOTDIFFICULTY;
			}

			players[i].botvars.diffincrease = 0;
		}

		if (players[i].botvars.rival)
		{
			if (oldrival == NULL)
			{
				// Record the old rival to compare with our calculated new rival
				oldrival = &players[i];
			}
			else
			{
				// Somehow 2 rivals were set?!
				// Let's quietly fix our mess...
				players[i].botvars.rival = false;
			}
		}
	}

	// Find the bot with the best average of score & difficulty.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT16 ns = 0;

		if (!playeringame[i] || players[i].spectator || !players[i].bot)
		{
			continue;
		}

		ns = K_RivalScore(&players[i]);

		if (ns > newrivalscore)
		{
			newrival = &players[i];
			newrivalscore = ns;
		}
	}

	// Even if there's a new rival, we want to make sure that they're a better fit than the current one.
	if (oldrival != newrival)
	{
		if (oldrival != NULL)
		{
			UINT16 os = K_RivalScore(oldrival);

			if (newrivalscore < os + 5)
			{
				// This rival's only *slightly* better, no need to fire the old one.
				// Our current rival's just fine, thank you very much.
				return;
			}

			// Hand over your badge.
			oldrival->botvars.rival = false;
		}

		// Set our new rival!
		newrival->botvars.rival = true;
	}
}

/*--------------------------------------------------
	static UINT8 K_BotExpectedStanding(player_t *bot)

		Predicts what placement a bot was expected to be in.
		Used for determining if a bot's difficulty should raise.

	Input Arguments:-
		bot - Player to check.

	Return:-
		Position number the bot was expected to be in.
--------------------------------------------------*/
static UINT8 K_BotExpectedStanding(player_t *bot)
{
	UINT8 pos = 1;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (i == (bot - players))
		{
			continue;
		}

		if (playeringame[i] && !players[i].spectator)
		{
			if (players[i].bot)
			{
				if (players[i].botvars.difficulty > bot->botvars.difficulty)
				{
					pos++;
				}
				else if (players[i].score > bot->score)
				{
					pos++;
				}
			}
			else
			{
				// Human player, always increment
				pos++;
			}
		}
	}

	return pos;
}

/*--------------------------------------------------
	void K_IncreaseBotDifficulty(player_t *bot)

		See header file for description.
--------------------------------------------------*/
void K_IncreaseBotDifficulty(player_t *bot)
{
	UINT8 expectedstanding;
	INT16 standingdiff;

	if (bot->botvars.difficulty >= MAXBOTDIFFICULTY)
	{
		// Already at max difficulty, don't need to increase
		return;
	}

	// Increment bot difficulty based on what position you were meant to come in!
	expectedstanding = K_BotExpectedStanding(bot);
	standingdiff = expectedstanding - bot->position;

	if (standingdiff >= -2)
	{
		UINT8 increase;

		if (standingdiff > 5)
		{
			increase = 3;
		}
		else if (standingdiff > 2)
		{
			increase = 2;
		}
		else
		{
			increase = 1;
		}

		bot->botvars.diffincrease = increase;
	}
	else
	{
		bot->botvars.diffincrease = 0;
	}
}

/*--------------------------------------------------
	void K_RetireBots(void)

		See header file for description.
--------------------------------------------------*/
void K_RetireBots(void)
{
	const UINT8 defaultbotskin = K_BotDefaultSkin();
	SINT8 newDifficulty;

	UINT8 usableskins;
	UINT8 grabskins[MAXSKINS+1];

	UINT8 i;

	if (grandprixinfo.gp == true
		&& ((grandprixinfo.roundnum >= grandprixinfo.cup->numlevels)
		|| grandprixinfo.eventmode != GPEVENT_NONE))
	{
		// No replacement.
		return;
	}

	// Init usable bot skins list
	for (usableskins = 0; usableskins < numskins; usableskins++)
	{
		grabskins[usableskins] = usableskins;
	}
	grabskins[usableskins] = MAXSKINS;

	// Exclude player skins
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		if (players[i].spectator)
			continue;

		grabskins[players[i].skin] = MAXSKINS;
	}

	// Rearrange usable bot skins list to prevent gaps for randomised selection
	for (i = 0; i < usableskins; i++)
	{
		if (!(grabskins[i] == MAXSKINS /*|| K_SkinLocked(grabskins[i])*/))
			continue;
		while (usableskins > i && (grabskins[usableskins] == MAXSKINS /*|| K_SkinLocked(grabskins[i])*/))
			usableskins--;
		grabskins[i] = grabskins[usableskins];
		grabskins[usableskins] = MAXSKINS;
	}

	if (!grandprixinfo.gp) // Sure, let's let this happen all the time :)
	{
		newDifficulty = cv_kartbot.value;
	}
	else
	{
		const UINT8 startingdifficulty = K_BotStartingDifficulty(grandprixinfo.gamespeed);
		newDifficulty = startingdifficulty - 4 + grandprixinfo.roundnum;
	}

	if (newDifficulty > MAXBOTDIFFICULTY)
	{
		newDifficulty = MAXBOTDIFFICULTY;
	}
	else if (newDifficulty < 1)
	{
		newDifficulty = 1;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *bot = NULL;

		if (!playeringame[i] || !players[i].bot || players[i].spectator)
		{
			continue;
		}

		bot = &players[i];

		if (bot->pflags & PF_NOCONTEST)
		{
			UINT8 skinnum = defaultbotskin;

			if (usableskins > 0)
			{
				UINT8 index = P_RandomKey(usableskins);
				skinnum = grabskins[index];
				grabskins[index] = grabskins[--usableskins];
			}

			bot->botvars.difficulty = newDifficulty;
			bot->botvars.diffincrease = 0;

			SetPlayerSkinByNum(bot - players, skinnum);
			bot->skincolor = skins[skinnum].prefcolor;
			sprintf(player_names[bot - players], "%s", skins[skinnum].realname);

			bot->score = 0;
			bot->pflags &= ~PF_NOCONTEST;
		}
	}
}

/*--------------------------------------------------
	void K_FakeBotResults(player_t *bot)

		See header file for description.
--------------------------------------------------*/
void K_FakeBotResults(player_t *bot)
{
	const UINT32 distfactor = FixedMul(32 * mapobjectscale, K_GetKartGameSpeedScalar(gamespeed)) / FRACUNIT;
	UINT32 worstdist = 0;
	tic_t besttime = UINT32_MAX;
	UINT8 numplayers = 0;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator)
		{
			numplayers++;

			if (players[i].exiting && players[i].realtime < besttime)
			{
				besttime = players[i].realtime;
			}

			if (players[i].distancetofinish > worstdist)
			{
				worstdist = players[i].distancetofinish;
			}
		}
	}

	if (besttime == UINT32_MAX // No one finished, so you don't finish either.
	|| bot->distancetofinish >= worstdist) // Last place, you aren't going to finish.
	{
		bot->pflags |= PF_NOCONTEST;
		return;
	}

	// hey, you "won"
	bot->exiting = 2;
	bot->realtime += (bot->distancetofinish / distfactor);
	bot->distancetofinish = 0;
	K_IncreaseBotDifficulty(bot);
}

/*--------------------------------------------------
	void K_PlayerLoseLife(player_t *player)

		See header file for description.
--------------------------------------------------*/
void K_PlayerLoseLife(player_t *player)
{
	if (!G_GametypeUsesLives())
	{
		return;
	}

	if (player->spectator || player->exiting || player->bot || (player->pflags & PF_LOSTLIFE))
	{
		return;
	}

	player->lives--;
	player->pflags |= PF_LOSTLIFE;

#if 0
	if (player->lives <= 0)
	{
		if (P_IsLocalPlayer(player))
		{
			S_StopMusic();
			S_ChangeMusicInternal("gmover", false);
		}
	}
#endif
}

/*--------------------------------------------------
	boolean K_CanChangeRules(void)

		See header file for description.
--------------------------------------------------*/
boolean K_CanChangeRules(void)
{
	if (demo.playback)
	{
		// We've already got our important settings!
		return false;
	}

	if (grandprixinfo.gp == true && grandprixinfo.roundnum > 0)
	{
		// Don't cheat the rules of the GP!
		return false;
	}

	if (bossinfo.boss == true)
	{
		// Don't cheat the boss!
		return false;
	}

	if (modeattacking == true)
	{
		// Don't cheat the rules of Time Trials!
		return false;
	}

	return true;
}
