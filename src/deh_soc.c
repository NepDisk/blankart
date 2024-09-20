// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  deh_soc.c
/// \brief Load SOC file and change tables and text

#include "doomdef.h"
#include "d_main.h" // for srb2home
#include "g_game.h"
#include "sounds.h"
#include "info.h"
#include "d_think.h"
#include "m_argv.h"
#include "z_zone.h"
#include "w_wad.h"
#include "y_inter.h"
#include "m_menu.h"
#include "m_misc.h"
#include "f_finale.h"
#include "st_stuff.h"
#include "i_system.h"
#include "p_setup.h"
#include "r_data.h"
#include "r_textures.h"
#include "r_draw.h"
#include "r_picformats.h"
#include "r_things.h" // R_Char2Frame
#include "r_sky.h"
#include "fastcmp.h"
#include "lua_script.h" // Reluctantly included for LUA_EvalMath
#include "d_clisrv.h"

#ifdef HWRENDER
#include "hardware/hw_light.h"
#endif

#include "m_cond.h"

#include "dehacked.h"
#include "deh_soc.h"
#include "deh_lua.h" // included due to some LUA_SetLuaAction hack smh
// also used for LUA_UpdateSprName
#include "deh_tables.h"

// SRB2Kart
#include "filesrch.h" // refreshdirmenu
#include "k_follower.h"
#include "doomstat.h" // MAXMUSNAMES

// Loops through every constant and operation in word and performs its calculations, returning the final value.
fixed_t get_number(const char *word)
{
	return LUA_EvalMath(word);

	/*// DESPERATELY NEEDED: Order of operations support! :x
	fixed_t i = find_const(&word);
	INT32 o;
	while(*word) {
		o = operation_pad(&word);
		if (o != -1)
			i = OPERATIONS[o].v(i,find_const(&word));
		else
			break;
	}
	return i;*/
}

#define PARAMCHECK(n) do { if (!params[n]) { deh_warning("Too few parameters, need %d", n); return; }} while (0)

/* ======================================================================== */
// Load a dehacked file format
/* ======================================================================== */
/* a sample to see
                   Thing 1 (Player)       {           // MT_PLAYER
INT32 doomednum;     ID # = 3232              -1,             // doomednum
INT32 spawnstate;    Initial frame = 32       "PLAY",         // spawnstate
INT32 spawnhealth;   Hit points = 3232        100,            // spawnhealth
INT32 seestate;      First moving frame = 32  "PLAY_RUN1",    // seestate
INT32 seesound;      Alert sound = 32         sfx_None,       // seesound
INT32 reactiontime;  Reaction time = 3232     0,              // reactiontime
INT32 attacksound;   Attack sound = 32        sfx_None,       // attacksound
INT32 painstate;     Injury frame = 32        "PLAY_PAIN",    // painstate
INT32 painchance;    Pain chance = 3232       255,            // painchance
INT32 painsound;     Pain sound = 32          sfx_plpain,     // painsound
INT32 meleestate;    Close attack frame = 32  "NULL",         // meleestate
INT32 missilestate;  Far attack frame = 32    "PLAY_ATK1",    // missilestate
INT32 deathstate;    Death frame = 32         "PLAY_DIE1",    // deathstate
INT32 xdeathstate;   Exploding frame = 32     "PLAY_XDIE1",   // xdeathstate
INT32 deathsound;    Death sound = 32         sfx_pldeth,     // deathsound
INT32 speed;         Speed = 3232             0,              // speed
INT32 radius;        Width = 211812352        16*FRACUNIT,    // radius
INT32 height;        Height = 211812352       56*FRACUNIT,    // height
INT32 dispoffset;    DispOffset = 0           0,              // dispoffset
INT32 mass;          Mass = 3232              100,            // mass
INT32 damage;        Missile damage = 3232    0,              // damage
INT32 activesound;   Action sound = 32        sfx_None,       // activesound
INT32 flags;         Bits = 3232              MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH,
INT32 raisestate;    Respawn frame = 32       S_NULL          // raisestate
                                         }, */

#ifdef HWRENDER
static INT32 searchvalue(const char *s)
{
	while (s[0] != '=' && s[0])
		s++;
	if (s[0] == '=')
		return atoi(&s[1]);
	else
	{
		deh_warning("No value found");
		return 0;
	}
}

static float searchfvalue(const char *s)
{
	while (s[0] != '=' && s[0])
		s++;
	if (s[0] == '=')
		return (float)atof(&s[1]);
	else
	{
		deh_warning("No value found");
		return 0;
	}
}
#endif

// These are for clearing all of various things
void clear_conditionsets(void)
{
	UINT8 i;
	for (i = 0; i < MAXCONDITIONSETS; ++i)
		M_ClearConditionSet(i+1);
}

void clear_levels(void)
{
	INT16 i;

	// This is potentially dangerous but if we're resetting these headers,
	// we may as well try to save some memory, right?
	for (i = 0; i < NUMMAPS; ++i)
	{
		if (!mapheaderinfo[i] || i == (tutorialmap-1))
			continue;

		// Custom map header info
		// (no need to set num to 0, we're freeing the entire header shortly)
		Z_Free(mapheaderinfo[i]->customopts);

		P_DeleteFlickies(i);
		P_DeleteGrades(i);

		Z_Free(mapheaderinfo[i]);
		mapheaderinfo[i] = NULL;
	}

	// Realloc the one for the current gamemap as a safeguard
	P_AllocMapHeader(gamemap-1);
}

static boolean findFreeSlot(INT32 *num)
{
	// Send the character select entry to a free slot.
	while (*num < MAXSKINS && (description[*num].used))
		*num = *num+1;

	// No more free slots. :(
	if (*num >= MAXSKINS)
		return false;

	// Redesign your logo. (See M_DrawSetupChoosePlayerMenu in m_menu.c...)
	description[*num].picname[0] = '\0';
	description[*num].nametag[0] = '\0';
	description[*num].displayname[0] = '\0';
	description[*num].oppositecolor = SKINCOLOR_NONE;
	description[*num].tagtextcolor = SKINCOLOR_NONE;
	description[*num].tagoutlinecolor = SKINCOLOR_NONE;

	// Found one! ^_^
	return (description[*num].used = true);
}

// Reads a player.
// For modifying the character select screen
void readPlayer(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *displayname = ZZ_Alloc(MAXLINELEN+1);
	INT32 i;
	boolean slotfound = false;

	#define SLOTFOUND \
		if (!slotfound && (slotfound = findFreeSlot(&num)) == false) \
			goto done;

	displayname[MAXLINELEN] = '\0';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			for (i = 0; i < MAXLINELEN-3; i++)
			{
				char *tmp;
				if (s[i] == '=')
				{
					tmp = &s[i+2];
					strncpy(displayname, tmp, SKINNAMESIZE);
					break;
				}
			}

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "PLAYERTEXT"))
			{
				char *playertext = NULL;

				SLOTFOUND

				for (i = 0; i < MAXLINELEN-3; i++)
				{
					if (s[i] == '=')
					{
						playertext = &s[i+2];
						break;
					}
				}
				if (playertext)
				{
					strcpy(description[num].notes, playertext);
					strcat(description[num].notes, myhashfgets(playertext, sizeof (description[num].notes), f));
				}
				else
					strcpy(description[num].notes, "");

				// For some reason, cutting the string did not work above. Most likely due to strcpy or strcat...
				// It works down here, though.
				{
					INT32 numline = 0;
					for (i = 0; (size_t)i < sizeof(description[num].notes)-1; i++)
					{
						if (numline < 20 && description[num].notes[i] == '\n')
							numline++;

						if (numline >= 20 || description[num].notes[i] == '\0' || description[num].notes[i] == '#')
							break;
					}
				}
				description[num].notes[strlen(description[num].notes)-1] = '\0';
				description[num].notes[i] = '\0';
				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);

			if (fastcmp(word, "PICNAME"))
			{
				SLOTFOUND
				strncpy(description[num].picname, word2, 8);
			}
			// new character select
			else if (fastcmp(word, "DISPLAYNAME"))
			{
				SLOTFOUND
				// replace '#' with line breaks
				// (also remove any '\n')
				{
					char *cur = NULL;

					// remove '\n'
					cur = strchr(displayname, '\n');
					if (cur)
						*cur = '\0';

					// turn '#' into '\n'
					cur = strchr(displayname, '#');
					while (cur)
					{
						*cur = '\n';
						cur = strchr(cur, '#');
					}
				}
				// copy final string
				strncpy(description[num].displayname, displayname, SKINNAMESIZE);
			}
			else if (fastcmp(word, "OPPOSITECOLOR") || fastcmp(word, "OPPOSITECOLOUR"))
			{
				SLOTFOUND
				description[num].oppositecolor = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "NAMETAG") || fastcmp(word, "TAGNAME"))
			{
				SLOTFOUND
				strncpy(description[num].nametag, word2, 8);
			}
			else if (fastcmp(word, "TAGTEXTCOLOR") || fastcmp(word, "TAGTEXTCOLOUR"))
			{
				SLOTFOUND
				description[num].tagtextcolor = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "TAGOUTLINECOLOR") || fastcmp(word, "TAGOUTLINECOLOUR"))
			{
				SLOTFOUND
				description[num].tagoutlinecolor = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "STATUS"))
			{
				/*
					You MAY disable previous entries if you so desire...
					But try to enable something that's already enabled and you will be sent to a free slot.

					Because of this, you are allowed to edit any previous entries you like, but only if you
					signal that you are purposely doing so by disabling and then reenabling the slot.
				*/
				if (i && !slotfound && (slotfound = findFreeSlot(&num)) == false)
					goto done;

				description[num].used = (!!i);
			}
			else if (fastcmp(word, "SKINNAME"))
			{
				// Send to free slot.
				SLOTFOUND
				strlcpy(description[num].skinname, word2, sizeof description[num].skinname);
				strlwr(description[num].skinname);
			}
			else
				deh_warning("readPlayer %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty
	#undef SLOTFOUND
done:
	Z_Free(displayname);
	Z_Free(s);
}

// TODO: Figure out how to do undolines for this....
// TODO: Warnings for running out of freeslots
void readfreeslots(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word,*type;
	char *tmp;
	int i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			type = strtok(s, "_");
			if (type)
				strupr(type);
			else
				break;

			word = strtok(NULL, "\n");
			if (word)
				strupr(word);
			else
				break;

			// TODO: Check for existing freeslot mobjs/states/etc. and make errors.
			// TODO: Out-of-slots warnings/errors.
			// TODO: Name too long (truncated) warnings.
			if (fastcmp(type, "SFX"))
				S_AddSoundFx(word, false, 0, false);
			else if (fastcmp(type, "SPR"))
			{
				for (i = SPR_FIRSTFREESLOT; i <= SPR_LASTFREESLOT; i++)
				{
					if (used_spr[(i-SPR_FIRSTFREESLOT)/8] & (1<<(i%8)))
					{
						if (!sprnames[i][4] && memcmp(sprnames[i],word,4)==0)
							sprnames[i][4] = (char)f->wad;
						continue; // Already allocated, next.
					}
					// Found a free slot!
					strncpy(sprnames[i],word,4);
					//sprnames[i][4] = 0;
					used_spr[(i-SPR_FIRSTFREESLOT)/8] |= 1<<(i%8); // Okay, this sprite slot has been named now.
					// Lua needs to update the value in _G if it exists
					LUA_UpdateSprName(word, i);
					break;
				}
			}
			else if (fastcmp(type, "S"))
			{
				for (i = 0; i < NUMSTATEFREESLOTS; i++)
					if (!FREE_STATES[i]) {
						FREE_STATES[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_STATES[i],word);
						freeslotusage[0][0]++;
						break;
					}
			}
			else if (fastcmp(type, "MT"))
			{
				for (i = 0; i < NUMMOBJFREESLOTS; i++)
					if (!FREE_MOBJS[i]) {
						FREE_MOBJS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_MOBJS[i],word);
						freeslotusage[1][0]++;
						break;
					}
			}
			else if (fastcmp(type, "SKINCOLOR"))
			{
				for (i = 0; i < NUMCOLORFREESLOTS; i++)
					if (!FREE_SKINCOLORS[i]) {
						FREE_SKINCOLORS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_SKINCOLORS[i],word);
						M_AddMenuColor(numskincolors++);
						break;
					}
			}
			else if (fastcmp(type, "SPR2"))
			{
				// Search if we already have an SPR2 by that name...
				for (i = SPR2_FIRSTFREESLOT; i < (int)free_spr2; i++)
					if (memcmp(spr2names[i],word,4) == 0)
						break;
				// We found it? (Two mods using the same SPR2 name?) Then don't allocate another one.
				if (i < (int)free_spr2)
					continue;
				// Copy in the spr2 name and increment free_spr2.
				if (free_spr2 < NUMPLAYERSPRITES) {
					strncpy(spr2names[free_spr2],word,4);
					spr2defaults[free_spr2] = 0;
					spr2names[free_spr2++][4] = 0;
				} else
					deh_warning("Ran out of free SPR2 slots!\n");
			}
			else if (fastcmp(type, "TOL"))
			{
				// Search if we already have a typeoflevel by that name...
				for (i = 0; TYPEOFLEVEL[i].name; i++)
					if (fastcmp(word, TYPEOFLEVEL[i].name))
						break;

				// We found it? Then don't allocate another one.
				if (TYPEOFLEVEL[i].name)
					continue;

				// We don't, so freeslot it.
				if (lastcustomtol == (UINT32)MAXTOL) // Unless you have way too many, since they're flags.
					deh_warning("Ran out of free typeoflevel slots!\n");
				else
				{
					G_AddTOL(lastcustomtol, word);
					lastcustomtol <<= 1;
				}
			}
			else if (fastcmp(type, "PRECIP"))
			{
				// Search if we already have a PRECIP by that name...
				for (i = PRECIP_FIRSTFREESLOT; i < (int)precip_freeslot; i++)
					if (fastcmp(word, precipprops[i].name))
						break;

				// We found it? Then don't allocate another one.
				if (i < (int)precip_freeslot)
					continue;

				// We don't, so allocate a new one.
				if (precip_freeslot < MAXPRECIP)
				{
					precipprops[i].name = Z_StrDup(word);
					precip_freeslot++;
				} else
					deh_warning("Ran out of free PRECIP slots!\n");
			}
			else
				deh_warning("Freeslots: unknown enum class '%s' for '%s_%s'", type, type, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readthing(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "MAPTHINGNUM") || fastcmp(word, "DOOMEDNUM"))
			{
				mobjinfo[num].doomednum = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "SPAWNSTATE"))
			{
				mobjinfo[num].spawnstate = get_number(word2);
			}
			else if (fastcmp(word, "SPAWNHEALTH"))
			{
				mobjinfo[num].spawnhealth = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "SEESTATE"))
			{
				mobjinfo[num].seestate = get_number(word2);
			}
			else if (fastcmp(word, "SEESOUND"))
			{
				mobjinfo[num].seesound = get_number(word2);
			}
			else if (fastcmp(word, "REACTIONTIME"))
			{
				mobjinfo[num].reactiontime = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "ATTACKSOUND"))
			{
				mobjinfo[num].attacksound = get_number(word2);
			}
			else if (fastcmp(word, "PAINSTATE"))
			{
				mobjinfo[num].painstate = get_number(word2);
			}
			else if (fastcmp(word, "PAINCHANCE"))
			{
				mobjinfo[num].painchance = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "PAINSOUND"))
			{
				mobjinfo[num].painsound = get_number(word2);
			}
			else if (fastcmp(word, "MELEESTATE"))
			{
				mobjinfo[num].meleestate = get_number(word2);
			}
			else if (fastcmp(word, "MISSILESTATE"))
			{
				mobjinfo[num].missilestate = get_number(word2);
			}
			else if (fastcmp(word, "DEATHSTATE"))
			{
				mobjinfo[num].deathstate = get_number(word2);
			}
			else if (fastcmp(word, "DEATHSOUND"))
			{
				mobjinfo[num].deathsound = get_number(word2);
			}
			else if (fastcmp(word, "XDEATHSTATE"))
			{
				mobjinfo[num].xdeathstate = get_number(word2);
			}
			else if (fastcmp(word, "SPEED"))
			{
				mobjinfo[num].speed = get_number(word2);
			}
			else if (fastcmp(word, "RADIUS"))
			{
				mobjinfo[num].radius = get_number(word2);
			}
			else if (fastcmp(word, "HEIGHT"))
			{
				mobjinfo[num].height = get_number(word2);
			}
			else if (fastcmp(word, "DISPOFFSET"))
			{
				mobjinfo[num].dispoffset = get_number(word2);
			}
			else if (fastcmp(word, "MASS"))
			{
				mobjinfo[num].mass = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "DAMAGE"))
			{
				mobjinfo[num].damage = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "ACTIVESOUND"))
			{
				mobjinfo[num].activesound = get_number(word2);
			}
			else if (fastcmp(word, "FLAGS"))
			{
				mobjinfo[num].flags = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "RAISESTATE"))
			{
				mobjinfo[num].raisestate = get_number(word2);
			}
			else
				deh_warning("Thing %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readskincolor(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;

	Color_cons_t[num].value = num;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			if (fastcmp(word, "NAME"))
			{
				size_t namesize = sizeof(skincolors[num].name);
				char truncword[namesize];
				UINT16 dupecheck;

				deh_strlcpy(truncword, word2, namesize, va("Skincolor %d: name", num)); // truncate here to check for dupes
				dupecheck = R_GetColorByName(truncword);
				if (truncword[0] != '\0' && (!stricmp(truncword, skincolors[SKINCOLOR_NONE].name) || (dupecheck && dupecheck != num)))
				{
					size_t lastchar = strlen(truncword);
					char oldword[lastchar+1];
					char dupenum = '1';

					strlcpy(oldword, truncword, lastchar+1);
					lastchar--;
					if (lastchar == namesize-2) // exactly max length, replace last character with 0
						truncword[lastchar] = '0';
					else // append 0
					{
						strcat(truncword, "0");
						lastchar++;
					}

					while (R_GetColorByName(truncword))
					{
						truncword[lastchar] = dupenum;
						if (dupenum == '9')
							dupenum = 'A';
						else if (dupenum == 'Z') // give up :?
							break;
						else
							dupenum++;
					}

					deh_warning("Skincolor %d: name %s is a duplicate of another skincolor's name - renamed to %s", num, oldword, truncword);
				}

				strlcpy(skincolors[num].name, truncword, namesize); // already truncated
			}
			else if (fastcmp(word, "RAMP"))
			{
				UINT8 i;
				tmp = strtok(word2,",");
				for (i = 0; i < COLORRAMPSIZE; i++) {
					skincolors[num].ramp[i] = (UINT8)get_number(tmp);
					if ((tmp = strtok(NULL,",")) == NULL)
						break;
				}
				skincolor_modified[num] = true;
			}
			else if (fastcmp(word, "INVCOLOR"))
			{
				UINT16 v = (UINT16)get_number(word2);
				if (v < numskincolors)
					skincolors[num].invcolor = v;
				else
					skincolors[num].invcolor = SKINCOLOR_GREEN;
			}
			else if (fastcmp(word, "INVSHADE"))
			{
				skincolors[num].invshade = get_number(word2)%COLORRAMPSIZE;
			}
			else if (fastcmp(word, "CHATCOLOR"))
			{
				skincolors[num].chatcolor = get_number(word2);
			}
			else if (fastcmp(word, "ACCESSIBLE"))
			{
				if (num > FIRSTSUPERCOLOR)
					skincolors[num].accessible = (boolean)(atoi(word2) || word2[0] == 'T' || word2[0] == 'Y');
			}
			else
				deh_warning("Skincolor %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

#ifdef HWRENDER
void readlight(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *tmp;
	INT32 value;
	float fvalue;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			fvalue = searchfvalue(s);
			value = searchvalue(s);

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "TYPE"))
			{
				lspr[num].type = (UINT16)value;
			}
			else if (fastcmp(word, "OFFSETX"))
			{
				lspr[num].light_xoffset = fvalue;
			}
			else if (fastcmp(word, "OFFSETY"))
			{
				lspr[num].light_yoffset = fvalue;
			}
			else if (fastcmp(word, "CORONACOLOR"))
			{
				lspr[num].corona_color = value;
			}
			else if (fastcmp(word, "CORONARADIUS"))
			{
				lspr[num].corona_radius = fvalue;
			}
			else if (fastcmp(word, "DYNAMICCOLOR"))
			{
				lspr[num].dynamic_color = value;
			}
			else if (fastcmp(word, "DYNAMICRADIUS"))
			{
				lspr[num].dynamic_radius = fvalue;

				/// \note Update the sqrradius! unnecessary?
				lspr[num].dynamic_sqrradius = fvalue * fvalue;
			}
			else
				deh_warning("Light %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}
#endif // HWRENDER

void readsprite2(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "DEFAULT"))
				spr2defaults[num] = get_number(word2);
			else
				deh_warning("Sprite2 %s: unknown word '%s'", spr2names[num], word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

// copypasted from readPlayer :]
void readgametype(MYFILE *f, char *gtname)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2, *word2lwr = NULL;
	char *tmp;
	INT32 i, j;

	INT16 newgtidx = 0;
	UINT32 newgtrules = 0;
	UINT32 newgttol = 0;
	INT32 newgtpointlimit = 0;
	INT32 newgttimelimit = 0;
	INT16 newgtrankingstype = -1;
	int newgtinttype = 0;
	char gtconst[MAXLINELEN];

	// Empty strings.
	gtconst[0] = '\0';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
			{
				if (!word2lwr)
					word2lwr = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
				strcpy(word2lwr, word2);
				strupr(word2);
			}
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);

			// Game type rules
			if (fastcmp(word, "RULES"))
			{
				// GTR_
				newgtrules = (UINT32)get_number(word2);
			}
			// Identifier
			else if (fastcmp(word, "IDENTIFIER"))
			{
				// GT_
				strncpy(gtconst, word2, MAXLINELEN);
			}
			// Point and time limits
			else if (fastcmp(word, "DEFAULTPOINTLIMIT"))
				newgtpointlimit = (INT32)i;
			else if (fastcmp(word, "DEFAULTTIMELIMIT"))
				newgttimelimit = (INT32)i;
			// Rankings type
			else if (fastcmp(word, "RANKINGTYPE"))
			{
				// Case insensitive
				newgtrankingstype = (int)get_number(word2);
			}
			// Intermission type
			else if (fastcmp(word, "INTERMISSIONTYPE"))
			{
				// Case sensitive
				newgtinttype = (int)get_number(word2lwr);
			}
			// Type of level
			else if (fastcmp(word, "TYPEOFLEVEL"))
			{
				if (i) // it's just a number
					newgttol = (UINT32)i;
				else
				{
					UINT32 tol = 0;
					tmp = strtok(word2,",");
					do {
						for (i = 0; TYPEOFLEVEL[i].name; i++)
							if (fasticmp(tmp, TYPEOFLEVEL[i].name))
								break;
						if (!TYPEOFLEVEL[i].name)
							deh_warning("readgametype %s: unknown typeoflevel flag %s\n", gtname, tmp);
						tol |= TYPEOFLEVEL[i].flag;
					} while((tmp = strtok(NULL,",")) != NULL);
					newgttol = tol;
				}
			}
			// The SOC probably provided gametype rules as words,
			// instead of using the RULES keyword.
			// Like for example "NOSPECTATORSPAWN = TRUE".
			// This is completely valid, and looks better anyway.
			else
			{
				UINT32 wordgt = 0;
				for (j = 0; GAMETYPERULE_LIST[j]; j++)
					if (fastcmp(word, GAMETYPERULE_LIST[j])) {
						wordgt |= (1<<j);
						if (i || word2[0] == 'T' || word2[0] == 'Y')
							newgtrules |= wordgt;
						break;
					}
				if (!wordgt)
					deh_warning("readgametype %s: unknown word '%s'", gtname, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	// Free strings.
	Z_Free(s);
	if (word2lwr)
		Z_Free(word2lwr);

	// Ran out of gametype slots
	if (gametypecount == NUMGAMETYPEFREESLOTS)
	{
		CONS_Alert(CONS_WARNING, "Ran out of free gametype slots!\n");
		return;
	}

	// Add the new gametype
	newgtidx = G_AddGametype(newgtrules);
	G_AddGametypeTOL(newgtidx, newgttol);

	// Not covered by G_AddGametype alone.
	if (newgtrankingstype == -1)
		newgtrankingstype = newgtidx;
	gametyperankings[newgtidx] = newgtrankingstype;
	intermissiontypes[newgtidx] = newgtinttype;
	pointlimits[newgtidx] = newgtpointlimit;
	timelimits[newgtidx] = newgttimelimit;

	// Write the new gametype name.
	Gametype_Names[newgtidx] = Z_StrDup((const char *)gtname);

	// Write the constant name.
	if (gtconst[0] == '\0')
		strncpy(gtconst, gtname, MAXLINELEN);
	G_AddGametypeConstant(newgtidx, (const char *)gtconst);

	// Update gametype_cons_t accordingly.
	G_UpdateGametypeSelections();

	CONS_Printf("Added gametype %s\n", Gametype_Names[newgtidx]);
}

static mapheader_lighting_t *usemaplighting(INT32 mapnum, const char *word)
{
	if (fastncmp(word, "ENCORE", 6))
	{
		mapheaderinfo[mapnum-1]->use_encore_lighting = true;

		return &mapheaderinfo[mapnum-1]->lighting_encore;
	}
	else
	{
		return &mapheaderinfo[mapnum-1]->lighting;
	}
}

void readlevelheader(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	//char *word3; // Non-uppercase version of word2
	char *tmp;
	INT32 i;

	// Reset all previous map header information
	P_AllocMapHeader((INT16)(num-1));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word, because some things (Lua.) move it
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			i = atoi(word2); // used for numerical settings


			if (fastcmp(word, "LEVELNAME"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->lvlttl, word2,
					sizeof(mapheaderinfo[num-1]->lvlttl), va("Level header %d: levelname", num));
				strlcpy(mapheaderinfo[num-1]->selectheading, word2, sizeof(mapheaderinfo[num-1]->selectheading)); // not deh_ so only complains once
				continue;
			}
			// CHEAP HACK: move this over here for lowercase subtitles
			if (fastcmp(word, "SUBTITLE"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->subttl, word2,
					sizeof(mapheaderinfo[num-1]->subttl), va("Level header %d: subtitle", num));
				continue;
			}

			// Lua custom options also go above, contents may be case sensitive.
			if (fastncmp(word, "LUA.", 4))
			{
				UINT8 j;
				customoption_t *modoption;

				// Note: we actualy strlwr word here, so things are made a little easier for Lua
				strlwr(word);
				word += 4; // move past "lua."

				// ... and do a simple name sanity check; the name must start with a letter
				if (*word < 'a' || *word > 'z')
				{
					deh_warning("Level header %d: invalid custom option name \"%s\"", num, word);
					continue;
				}

				// Sanity limit of 128 params
				if (mapheaderinfo[num-1]->numCustomOptions == 128)
				{
					deh_warning("Level header %d: too many custom parameters", num);
					continue;
				}
				j = mapheaderinfo[num-1]->numCustomOptions++;

				mapheaderinfo[num-1]->customopts =
					Z_Realloc(mapheaderinfo[num-1]->customopts,
						sizeof(customoption_t) * mapheaderinfo[num-1]->numCustomOptions, PU_STATIC, NULL);

				// Newly allocated
				modoption = &mapheaderinfo[num-1]->customopts[j];

				strncpy(modoption->option, word,  31);
				modoption->option[31] = '\0';
				strncpy(modoption->value,  word2, 255);
				modoption->value[255] = '\0';
				continue;
			}

			// Now go to uppercase
			strupr(word2);

			// List of flickies that are be freed in this map
			if (fastcmp(word, "FLICKYLIST") || fastcmp(word, "ANIMALLIST"))
			{
				if (fastcmp(word2, "NONE"))
					P_DeleteFlickies(num-1);
				else if (fastcmp(word2, "DEMO"))
					P_SetDemoFlickies(num-1);
				else if (fastcmp(word2, "ALL"))
				{
					mobjtype_t tmpflickies[MAXFLICKIES];

					for (mapheaderinfo[num-1]->numFlickies = 0;
					((mapheaderinfo[num-1]->numFlickies < MAXFLICKIES) && FLICKYTYPES[mapheaderinfo[num-1]->numFlickies].type);
					mapheaderinfo[num-1]->numFlickies++)
						tmpflickies[mapheaderinfo[num-1]->numFlickies] = FLICKYTYPES[mapheaderinfo[num-1]->numFlickies].type;

					if (mapheaderinfo[num-1]->numFlickies) // just in case...
					{
						size_t newsize = sizeof(mobjtype_t) * mapheaderinfo[num-1]->numFlickies;
						mapheaderinfo[num-1]->flickies = Z_Realloc(mapheaderinfo[num-1]->flickies, newsize, PU_STATIC, NULL);
						M_Memcpy(mapheaderinfo[num-1]->flickies, tmpflickies, newsize);
					}
				}
				else
				{
					mobjtype_t tmpflickies[MAXFLICKIES];
					mapheaderinfo[num-1]->numFlickies = 0;
					tmp = strtok(word2,",");
					// get up to the first MAXFLICKIES flickies
					do {
						if (mapheaderinfo[num-1]->numFlickies == MAXFLICKIES) // never going to get above that number
						{
							deh_warning("Level header %d: too many flickies\n", num);
							break;
						}

						if (fastncmp(tmp, "MT_", 3)) // support for specified mobjtypes...
						{
							i = get_mobjtype(tmp);
							if (!i)
							{
								//deh_warning("Level header %d: unknown flicky mobj type %s\n", num, tmp); -- no need for this line as get_mobjtype complains too
								continue;
							}
							tmpflickies[mapheaderinfo[num-1]->numFlickies] = i;
						}
						else // ...or a quick, limited selection of default flickies!
						{
							for (i = 0; FLICKYTYPES[i].name; i++)
								if (fastcmp(tmp, FLICKYTYPES[i].name))
									break;

							if (!FLICKYTYPES[i].name)
							{
								deh_warning("Level header %d: unknown flicky selection %s\n", num, tmp);
								continue;
							}
							tmpflickies[mapheaderinfo[num-1]->numFlickies] = FLICKYTYPES[i].type;
						}
						mapheaderinfo[num-1]->numFlickies++;
					} while ((tmp = strtok(NULL,",")) != NULL);

					if (mapheaderinfo[num-1]->numFlickies)
					{
						size_t newsize = sizeof(mobjtype_t) * mapheaderinfo[num-1]->numFlickies;
						mapheaderinfo[num-1]->flickies = Z_Realloc(mapheaderinfo[num-1]->flickies, newsize, PU_STATIC, NULL);
						// now we add them to the list!
						M_Memcpy(mapheaderinfo[num-1]->flickies, tmpflickies, newsize);
					}
					else
						deh_warning("Level header %d: no valid flicky types found\n", num);
				}
			}

			// Strings that can be truncated
			else if (fastcmp(word, "ZONETITLE"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->zonttl, word2,
					sizeof(mapheaderinfo[num-1]->zonttl), va("Level header %d: zonetitle", num));
			}
			else if (fastcmp(word, "SCRIPTNAME"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->scriptname, word2,
					sizeof(mapheaderinfo[num-1]->scriptname), va("Level header %d: scriptname", num));
			}
			else if (fastcmp(word, "RUNSOC"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->runsoc, word2,
					sizeof(mapheaderinfo[num-1]->runsoc), va("Level header %d: runsoc", num));
			}
			else if (fastcmp(word, "ACT"))
			{
				/*if (i >= 0 && i < 20) // 0 for no act number, TTL1 through TTL19
					mapheaderinfo[num-1]->actnum = (UINT8)i;
				else
					deh_warning("Level header %d: invalid act number %d", num, i);*/
				deh_strlcpy(mapheaderinfo[num-1]->actnum, word2,
					sizeof(mapheaderinfo[num-1]->actnum), va("Level header %d: actnum", num));
			}
			else if (fastcmp(word, "NEXTLEVEL"))
			{
				if      (fastcmp(word2, "TITLE"))      i = 1100;
				else if (fastcmp(word2, "EVALUATION")) i = 1101;
				else if (fastcmp(word2, "CREDITS"))    i = 1102;
				else if (fastcmp(word2, "ENDING"))     i = 1103;
				else
				// Support using the actual map name,
				// i.e., Nextlevel = AB, Nextlevel = FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);

				mapheaderinfo[num-1]->nextlevel = (INT16)i;
			}
			else if (fastcmp(word, "MARATHONNEXT"))
			{
				if      (fastcmp(word2, "TITLE"))      i = 1100;
				else if (fastcmp(word2, "EVALUATION")) i = 1101;
				else if (fastcmp(word2, "CREDITS"))    i = 1102;
				else if (fastcmp(word2, "ENDING"))     i = 1103;
				else
				// Support using the actual map name,
				// i.e., MarathonNext = AB, MarathonNext = FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);

				mapheaderinfo[num-1]->marathonnext = (INT16)i;
			}
			else if (fastcmp(word, "TYPEOFLEVEL"))
			{
				if (i) // it's just a number
					mapheaderinfo[num-1]->typeoflevel = (UINT32)i;
				else
				{
					UINT32 tol = 0;
					tmp = strtok(word2,",");
					do {
						for (i = 0; TYPEOFLEVEL[i].name; i++)
							if (fastcmp(tmp, TYPEOFLEVEL[i].name))
								break;
						if (!TYPEOFLEVEL[i].name)
							deh_warning("Level header %d: unknown typeoflevel flag %s\n", num, tmp);
						tol |= TYPEOFLEVEL[i].flag;
					} while((tmp = strtok(NULL,",")) != NULL);
					mapheaderinfo[num-1]->typeoflevel = tol;
				}
			}
			else if (fastcmp(word, "KEYWORDS"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->keywords, word2,
						sizeof(mapheaderinfo[num-1]->keywords), va("Level header %d: keywords", num));
			}
			else if (fastcmp(word, "MUSIC"))
			{
				if (fastcmp(word2, "NONE"))
				{
					mapheaderinfo[num-1]->musname[0][0] = 0; // becomes empty string
					mapheaderinfo[num-1]->musname_size = 0;
				}
				else
				{
					UINT8 j = 0; // i was declared elsewhere
					tmp = strtok(word2, ",");
					do {
						if (j >= MAXMUSNAMES)
							break;
						deh_strlcpy(mapheaderinfo[num-1]->musname[j], tmp,
							sizeof(mapheaderinfo[num-1]->musname[j]), va("Level header %d: music", num));
						j++;
					} while ((tmp = strtok(NULL,",")) != NULL);
					
					if (tmp != NULL)
						deh_warning("Level header %d: additional music slots past %d discarded", num, MAXMUSNAMES);
					mapheaderinfo[num-1]->musname_size = j;
				}
			}
			else if (fastcmp(word, "MUSICSLOT"))
				deh_warning("Level header %d: MusicSlot parameter is deprecated and will be removed.\nUse \"Music\" instead.", num);
			else if (fastcmp(word, "MUSICTRACK"))
				mapheaderinfo[num-1]->mustrack = ((UINT16)i - 1);
			else if (fastcmp(word, "MUSICPOS"))
				mapheaderinfo[num-1]->muspos = (UINT32)get_number(word2);
			else if (fastcmp(word, "FORCECHARACTER"))
			{
				strlcpy(mapheaderinfo[num-1]->forcecharacter, word2, SKINNAMESIZE+1);
				strlwr(mapheaderinfo[num-1]->forcecharacter); // skin names are lowercase
			}
			else if (fastcmp(word, "WEATHER"))
				mapheaderinfo[num-1]->weather = get_precip(word2);
			else if (fastcmp(word, "SKYTEXTURE"))
				deh_strlcpy(mapheaderinfo[num-1]->skytexture, word2,
					sizeof(mapheaderinfo[num-1]->skytexture), va("Level header %d: sky texture", num));
			else if (fastcmp(word, "SKYNUM"))
				deh_strlcpy(mapheaderinfo[num-1]->skytexture, va("SKY%s", word2),
					sizeof(mapheaderinfo[num-1]->skytexture), va("Level header %d: sky texture", num));
			else if (fastcmp(word, "PRECUTSCENENUM"))
				mapheaderinfo[num-1]->precutscenenum = (UINT8)i;
			else if (fastcmp(word, "CUTSCENENUM"))
				mapheaderinfo[num-1]->cutscenenum = (UINT8)i;
			else if (fastcmp(word, "PALETTE"))
				mapheaderinfo[num-1]->palette = (UINT16)i;
			else if (fastcmp(word, "ENCOREPAL"))
				mapheaderinfo[num-1]->encorepal = (UINT16)i;
			else if (fastcmp(word, "NUMLAPS"))
				mapheaderinfo[num-1]->numlaps = (UINT8)i;
			else if (fastcmp(word, "UNLOCKABLE"))
			{
				if (i >= 0 && i <= MAXUNLOCKABLES) // 0 for no unlock required, anything else requires something
					mapheaderinfo[num-1]->unlockrequired = (SINT8)i - 1;
				else
					deh_warning("Level header %d: invalid unlockable number %d", num, i);
			}
			else if (fastcmp(word, "LEVELSELECT"))
				mapheaderinfo[num-1]->levelselect = (UINT8)i;
			else if (fastcmp(word, "SKYBOXSCALE"))
				mapheaderinfo[num-1]->skybox_scalex = mapheaderinfo[num-1]->skybox_scaley = mapheaderinfo[num-1]->skybox_scalez = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEX"))
				mapheaderinfo[num-1]->skybox_scalex = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEY"))
				mapheaderinfo[num-1]->skybox_scaley = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEZ"))
				mapheaderinfo[num-1]->skybox_scalez = (INT16)i;
			else if (fastcmp(word, "LEVELFLAGS"))
				mapheaderinfo[num-1]->levelflags = get_number(word2);
			else if (fastcmp(word, "MENUFLAGS"))
				mapheaderinfo[num-1]->menuflags = get_number(word2);
			// SRB2Kart
			else if (fastcmp(word, "MOBJSCALE"))
				mapheaderinfo[num-1]->mobj_scale = get_number(word2);
			else if (fastcmp(word, "DEFAULTWAYPOINTRADIUS"))
				mapheaderinfo[num-1]->default_waypoint_radius = get_number(word2);
			else if (fastcmp(word, "LIGHTCONTRAST"))
			{
				usemaplighting(num, word)->light_contrast = (UINT8)i;
			}
			else if (fastcmp(word, "SPRITEBACKLIGHT") || fastcmp(word, "ENCORESPRITEBACKLIGHT"))
			{
				usemaplighting(num, word)->sprite_backlight = (SINT8)i;
			}
			else if (fastcmp(word, "LIGHTANGLE") || fastcmp(word, "ENCORELIGHTANGLE"))
			{
				mapheader_lighting_t *lighting = usemaplighting(num, word);

				if (fastcmp(word2, "EVEN"))
				{
					lighting->use_light_angle = false;
					lighting->light_angle = 0;
				}
				else
				{
					lighting->use_light_angle = true;
					lighting->light_angle = FixedAngle(FloatToFixed(atof(word2)));
				}
			}
			// Individual triggers for level flags, for ease of use (and 2.0 compatibility)
			else if (fastcmp(word, "SCRIPTISFILE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SCRIPTISFILE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SCRIPTISFILE;
			}
			else if (fastcmp(word, "NOZONE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_NOZONE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_NOZONE;
			}
			else if (fastcmp(word, "SECTIONRACE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SECTIONRACE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SECTIONRACE;
			}
			else if (fastcmp(word, "SUBTRACTNUM"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SUBTRACTNUM;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SUBTRACTNUM;
			}

			// Individual triggers for menu flags
			else if (fastcmp(word, "HIDDEN"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_HIDEINMENU;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_HIDEINMENU;
			}
			else if (fastcmp(word, "HIDEINSTATS"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_HIDEINSTATS;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_HIDEINSTATS;
			}
			else if (fastcmp(word, "TIMEATTACK") || fastcmp(word, "RECORDATTACK"))
			{ // RECORDATTACK is an accepted alias
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags &= ~LF2_NOTIMEATTACK;
				else
					mapheaderinfo[num-1]->menuflags |= LF2_NOTIMEATTACK;
			}
			else if (fastcmp(word, "VISITNEEDED"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_VISITNEEDED;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_VISITNEEDED;
			}
			else if (fastcmp(word, "NOVISITNEEDED"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags &= ~LF2_VISITNEEDED;
				else
					mapheaderinfo[num-1]->menuflags |= LF2_VISITNEEDED;
			}
			else if (fastcmp(word, "GRAVITY"))
				mapheaderinfo[num-1]->gravity = FLOAT_TO_FIXED(atof(word2));
			else if (fastcmp(word, "WALLTRANSFER") || fastcmp(word, "WALLRUNNING"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->use_walltransfer = true;
				else
					mapheaderinfo[num-1]->use_walltransfer = false;
			}
			else
				deh_warning("Level header %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readcutscenescene(MYFILE *f, INT32 num, INT32 scenenum)
{
	char *s = Z_Calloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	INT32 i;
	UINT16 usi;
	UINT8 picid;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "SCENETEXT"))
			{
				char *scenetext = NULL;
				char *buffer;
				const int bufferlen = 4096;

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '=')
					{
						scenetext = &s[i+2];
						break;
					}
				}

				if (!scenetext)
				{
					Z_Free(cutscenes[num]->scene[scenenum].text);
					cutscenes[num]->scene[scenenum].text = NULL;
					continue;
				}

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '\0')
					{
						s[i] = '\n';
						s[i+1] = '\0';
						break;
					}
				}

				buffer = Z_Malloc(4096, PU_STATIC, NULL);
				strcpy(buffer, scenetext);

				strcat(buffer,
					myhashfgets(scenetext, bufferlen
					- strlen(buffer) - 1, f));

				// A cutscene overwriting another one...
				Z_Free(cutscenes[num]->scene[scenenum].text);

				cutscenes[num]->scene[scenenum].text = Z_StrDup(buffer);

				Z_Free(buffer);

				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);
			usi = (UINT16)i;


			if (fastcmp(word, "NUMBEROFPICS"))
			{
				cutscenes[num]->scene[scenenum].numpics = (UINT8)i;
			}
			else if (fastncmp(word, "PIC", 3))
			{
				picid = (UINT8)atoi(word + 3);
				if (picid > 8 || picid == 0)
				{
					deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
					continue;
				}
				--picid;

				if (fastcmp(word+4, "NAME"))
				{
					strncpy(cutscenes[num]->scene[scenenum].picname[picid], word2, 8);
				}
				else if (fastcmp(word+4, "HIRES"))
				{
					cutscenes[num]->scene[scenenum].pichires[picid] = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				}
				else if (fastcmp(word+4, "DURATION"))
				{
					cutscenes[num]->scene[scenenum].picduration[picid] = usi;
				}
				else if (fastcmp(word+4, "XCOORD"))
				{
					cutscenes[num]->scene[scenenum].xcoord[picid] = usi;
				}
				else if (fastcmp(word+4, "YCOORD"))
				{
					cutscenes[num]->scene[scenenum].ycoord[picid] = usi;
				}
				else
					deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(cutscenes[num]->scene[scenenum].musswitch, word2, 7);
				cutscenes[num]->scene[scenenum].musswitch[6] = 0;
			}
			else if (fastcmp(word, "MUSICTRACK"))
			{
				cutscenes[num]->scene[scenenum].musswitchflags = ((UINT16)i) & MUSIC_TRACKMASK;
			}
			else if (fastcmp(word, "MUSICPOS"))
			{
				cutscenes[num]->scene[scenenum].musswitchposition = (UINT32)get_number(word2);
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				cutscenes[num]->scene[scenenum].musicloop = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "TEXTXPOS"))
			{
				cutscenes[num]->scene[scenenum].textxpos = usi;
			}
			else if (fastcmp(word, "TEXTYPOS"))
			{
				cutscenes[num]->scene[scenenum].textypos = usi;
			}
			else if (fastcmp(word, "FADEINID"))
			{
				cutscenes[num]->scene[scenenum].fadeinid = (UINT8)i;
			}
			else if (fastcmp(word, "FADEOUTID"))
			{
				cutscenes[num]->scene[scenenum].fadeoutid = (UINT8)i;
			}
			else if (fastcmp(word, "FADECOLOR"))
			{
				cutscenes[num]->scene[scenenum].fadecolor = (UINT8)i;
			}
			else
				deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readcutscene(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	// Allocate memory for this cutscene if we don't yet have any
	if (!cutscenes[num])
		cutscenes[num] = Z_Calloc(sizeof (cutscene_t), PU_STATIC, NULL);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "NUMSCENES"))
			{
				cutscenes[num]->numscenes = value;
			}
			else if (fastcmp(word, "SCENE"))
			{
				if (1 <= value && value <= 128)
				{
					readcutscenescene(f, num, value - 1);
				}
				else
					deh_warning("Scene number %d out of range (1 - 128)", value);

			}
			else
				deh_warning("Cutscene %d: unknown word '%s', Scene <num> expected.", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readtextpromptpage(MYFILE *f, INT32 num, INT32 pagenum)
{
	char *s = Z_Calloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	INT32 i;
	UINT16 usi;
	UINT8 picid;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "PAGETEXT"))
			{
				char *pagetext = NULL;
				char *buffer;
				const int bufferlen = 4096;

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '=')
					{
						pagetext = &s[i+2];
						break;
					}
				}

				if (!pagetext)
				{
					Z_Free(textprompts[num]->page[pagenum].text);
					textprompts[num]->page[pagenum].text = NULL;
					continue;
				}

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '\0')
					{
						s[i] = '\n';
						s[i+1] = '\0';
						break;
					}
				}

				buffer = Z_Malloc(4096, PU_STATIC, NULL);
				strcpy(buffer, pagetext);

				// \todo trim trailing whitespace before the #
				// and also support # at the end of a PAGETEXT with no line break

				strcat(buffer,
					myhashfgets(pagetext, bufferlen
					- strlen(buffer) - 1, f));

				// A text prompt overwriting another one...
				Z_Free(textprompts[num]->page[pagenum].text);

				textprompts[num]->page[pagenum].text = Z_StrDup(buffer);

				Z_Free(buffer);

				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);
			usi = (UINT16)i;

			// copypasta from readcutscenescene
			if (fastcmp(word, "NUMBEROFPICS"))
			{
				textprompts[num]->page[pagenum].numpics = (UINT8)i;
			}
			else if (fastcmp(word, "PICMODE"))
			{
				UINT8 picmode = 0; // PROMPT_PIC_PERSIST
				if (usi == 1 || word2[0] == 'L') picmode = PROMPT_PIC_LOOP;
				else if (usi == 2 || word2[0] == 'D' || word2[0] == 'H') picmode = PROMPT_PIC_DESTROY;
				textprompts[num]->page[pagenum].picmode = picmode;
			}
			else if (fastcmp(word, "PICTOLOOP"))
				textprompts[num]->page[pagenum].pictoloop = (UINT8)i;
			else if (fastcmp(word, "PICTOSTART"))
				textprompts[num]->page[pagenum].pictostart = (UINT8)i;
			else if (fastcmp(word, "PICSMETAPAGE"))
			{
				if (usi && usi <= textprompts[num]->numpages)
				{
					UINT8 metapagenum = usi - 1;

					textprompts[num]->page[pagenum].numpics = textprompts[num]->page[metapagenum].numpics;
					textprompts[num]->page[pagenum].picmode = textprompts[num]->page[metapagenum].picmode;
					textprompts[num]->page[pagenum].pictoloop = textprompts[num]->page[metapagenum].pictoloop;
					textprompts[num]->page[pagenum].pictostart = textprompts[num]->page[metapagenum].pictostart;

					for (picid = 0; picid < MAX_PROMPT_PICS; picid++)
					{
						strncpy(textprompts[num]->page[pagenum].picname[picid], textprompts[num]->page[metapagenum].picname[picid], 8);
						textprompts[num]->page[pagenum].pichires[picid] = textprompts[num]->page[metapagenum].pichires[picid];
						textprompts[num]->page[pagenum].picduration[picid] = textprompts[num]->page[metapagenum].picduration[picid];
						textprompts[num]->page[pagenum].xcoord[picid] = textprompts[num]->page[metapagenum].xcoord[picid];
						textprompts[num]->page[pagenum].ycoord[picid] = textprompts[num]->page[metapagenum].ycoord[picid];
					}
				}
			}
			else if (fastncmp(word, "PIC", 3))
			{
				picid = (UINT8)atoi(word + 3);
				if (picid > MAX_PROMPT_PICS || picid == 0)
				{
					deh_warning("textpromptscene %d: unknown word '%s'", num, word);
					continue;
				}
				--picid;

				if (fastcmp(word+4, "NAME"))
				{
					strncpy(textprompts[num]->page[pagenum].picname[picid], word2, 8);
				}
				else if (fastcmp(word+4, "HIRES"))
				{
					textprompts[num]->page[pagenum].pichires[picid] = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				}
				else if (fastcmp(word+4, "DURATION"))
				{
					textprompts[num]->page[pagenum].picduration[picid] = usi;
				}
				else if (fastcmp(word+4, "XCOORD"))
				{
					textprompts[num]->page[pagenum].xcoord[picid] = usi;
				}
				else if (fastcmp(word+4, "YCOORD"))
				{
					textprompts[num]->page[pagenum].ycoord[picid] = usi;
				}
				else
					deh_warning("textpromptscene %d: unknown word '%s'", num, word);
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(textprompts[num]->page[pagenum].musswitch, word2, 7);
				textprompts[num]->page[pagenum].musswitch[6] = 0;
			}
			else if (fastcmp(word, "MUSICTRACK"))
			{
				textprompts[num]->page[pagenum].musswitchflags = ((UINT16)i) & MUSIC_TRACKMASK;
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				textprompts[num]->page[pagenum].musicloop = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
			}
			// end copypasta from readcutscenescene
			else if (fastcmp(word, "NAME"))
			{
				if (*word2 != '\0')
				{
					INT32 j;

					// HACK: Add yellow control char now
					// so the drawing function doesn't call it repeatedly
					char name[34];
					name[0] = '\x82'; // color yellow
					name[1] = 0;
					strncat(name, word2, 33);
					name[33] = 0;

					// Replace _ with ' '
					for (j = 0; j < 32 && name[j]; j++)
					{
						if (name[j] == '_')
							name[j] = ' ';
					}

					strncpy(textprompts[num]->page[pagenum].name, name, 32);
				}
				else
					*textprompts[num]->page[pagenum].name = '\0';
			}
			else if (fastcmp(word, "ICON"))
				strncpy(textprompts[num]->page[pagenum].iconname, word2, 8);
			else if (fastcmp(word, "ICONALIGN"))
				textprompts[num]->page[pagenum].rightside = (i || word2[0] == 'R');
			else if (fastcmp(word, "ICONFLIP"))
				textprompts[num]->page[pagenum].iconflip = (i || word2[0] == 'T' || word2[0] == 'Y');
			else if (fastcmp(word, "LINES"))
				textprompts[num]->page[pagenum].lines = usi;
			else if (fastcmp(word, "BACKCOLOR"))
			{
				INT32 backcolor;
				if      (i == 0 || fastcmp(word2, "WHITE")) backcolor = 0;
				else if (i == 1 || fastcmp(word2, "GRAY") || fastcmp(word2, "GREY") ||
					fastcmp(word2, "BLACK")) backcolor = 1;
				else if (i == 2 || fastcmp(word2, "SEPIA")) backcolor = 2;
				else if (i == 3 || fastcmp(word2, "BROWN")) backcolor = 3;
				else if (i == 4 || fastcmp(word2, "PINK")) backcolor = 4;
				else if (i == 5 || fastcmp(word2, "RASPBERRY")) backcolor = 5;
				else if (i == 6 || fastcmp(word2, "RED")) backcolor = 6;
				else if (i == 7 || fastcmp(word2, "CREAMSICLE")) backcolor = 7;
				else if (i == 8 || fastcmp(word2, "ORANGE")) backcolor = 8;
				else if (i == 9 || fastcmp(word2, "GOLD")) backcolor = 9;
				else if (i == 10 || fastcmp(word2, "YELLOW")) backcolor = 10;
				else if (i == 11 || fastcmp(word2, "EMERALD")) backcolor = 11;
				else if (i == 12 || fastcmp(word2, "GREEN")) backcolor = 12;
				else if (i == 13 || fastcmp(word2, "CYAN") || fastcmp(word2, "AQUA")) backcolor = 13;
				else if (i == 14 || fastcmp(word2, "STEEL")) backcolor = 14;
				else if (i == 15 || fastcmp(word2, "PERIWINKLE")) backcolor = 15;
				else if (i == 16 || fastcmp(word2, "BLUE")) backcolor = 16;
				else if (i == 17 || fastcmp(word2, "PURPLE")) backcolor = 17;
				else if (i == 18 || fastcmp(word2, "LAVENDER")) backcolor = 18;
				else if (i >= 256 && i < 512) backcolor = i; // non-transparent palette index
				else if (i < 0) backcolor = INT32_MAX; // CONS_BACKCOLOR user-configured
				else backcolor = 1; // default gray
				textprompts[num]->page[pagenum].backcolor = backcolor;
			}
			else if (fastcmp(word, "ALIGN"))
			{
				UINT8 align = 0; // left
				if (usi == 1 || word2[0] == 'R') align = 1;
				else if (usi == 2 || word2[0] == 'C' || word2[0] == 'M') align = 2;
				textprompts[num]->page[pagenum].align = align;
			}
			else if (fastcmp(word, "VERTICALALIGN"))
			{
				UINT8 align = 0; // top
				if (usi == 1 || word2[0] == 'B') align = 1;
				else if (usi == 2 || word2[0] == 'C' || word2[0] == 'M') align = 2;
				textprompts[num]->page[pagenum].verticalalign = align;
			}
			else if (fastcmp(word, "TEXTSPEED"))
				textprompts[num]->page[pagenum].textspeed = get_number(word2);
			else if (fastcmp(word, "TEXTSFX"))
				textprompts[num]->page[pagenum].textsfx = get_number(word2);
			else if (fastcmp(word, "HIDEHUD"))
			{
				UINT8 hidehud = 0;
				if ((word2[0] == 'F' && (word2[1] == 'A' || !word2[1])) || word2[0] == 'N') hidehud = 0; // false
				else if (usi == 1 || word2[0] == 'T' || word2[0] == 'Y') hidehud = 1; // true (hide appropriate HUD elements)
				else if (usi == 2 || word2[0] == 'A' || (word2[0] == 'F' && word2[1] == 'O')) hidehud = 2; // force (hide all HUD elements)
				textprompts[num]->page[pagenum].hidehud = hidehud;
			}
			else if (fastcmp(word, "METAPAGE"))
			{
				if (usi && usi <= textprompts[num]->numpages)
				{
					UINT8 metapagenum = usi - 1;

					strncpy(textprompts[num]->page[pagenum].name, textprompts[num]->page[metapagenum].name, 32);
					strncpy(textprompts[num]->page[pagenum].iconname, textprompts[num]->page[metapagenum].iconname, 8);
					textprompts[num]->page[pagenum].rightside = textprompts[num]->page[metapagenum].rightside;
					textprompts[num]->page[pagenum].iconflip = textprompts[num]->page[metapagenum].iconflip;
					textprompts[num]->page[pagenum].lines = textprompts[num]->page[metapagenum].lines;
					textprompts[num]->page[pagenum].backcolor = textprompts[num]->page[metapagenum].backcolor;
					textprompts[num]->page[pagenum].align = textprompts[num]->page[metapagenum].align;
					textprompts[num]->page[pagenum].verticalalign = textprompts[num]->page[metapagenum].verticalalign;
					textprompts[num]->page[pagenum].textspeed = textprompts[num]->page[metapagenum].textspeed;
					textprompts[num]->page[pagenum].textsfx = textprompts[num]->page[metapagenum].textsfx;
					textprompts[num]->page[pagenum].hidehud = textprompts[num]->page[metapagenum].hidehud;

					// music: don't copy, else each page change may reset the music
				}
			}
			else if (fastcmp(word, "TAG"))
				strncpy(textprompts[num]->page[pagenum].tag, word2, 33);
			else if (fastcmp(word, "NEXTPROMPT"))
				textprompts[num]->page[pagenum].nextprompt = usi;
			else if (fastcmp(word, "NEXTPAGE"))
				textprompts[num]->page[pagenum].nextpage = usi;
			else if (fastcmp(word, "NEXTTAG"))
				strncpy(textprompts[num]->page[pagenum].nexttag, word2, 33);
			else if (fastcmp(word, "TIMETONEXT"))
				textprompts[num]->page[pagenum].timetonext = get_number(word2);
			else
				deh_warning("PromptPage %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readtextprompt(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	// Allocate memory for this prompt if we don't yet have any
	if (!textprompts[num])
		textprompts[num] = Z_Calloc(sizeof (textprompt_t), PU_STATIC, NULL);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "NUMPAGES"))
			{
				textprompts[num]->numpages = min(max(value, 0), MAX_PAGES);
			}
			else if (fastcmp(word, "PAGE"))
			{
				if (1 <= value && value <= MAX_PAGES)
				{
					textprompts[num]->page[value - 1].backcolor = 1; // default to gray
					textprompts[num]->page[value - 1].hidehud = 1; // hide appropriate HUD elements
					readtextpromptpage(f, num, value - 1);
				}
				else
					deh_warning("Page number %d out of range (1 - %d)", value, MAX_PAGES);

			}
			else
				deh_warning("Prompt %d: unknown word '%s', Page <num> expected.", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readmenu(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = (tmp += 2);
			strupr(word2);

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "BACKGROUNDNAME"))
			{
				strncpy(menupres[num].bgname, word2, 8);
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDEBACKGROUND"))
			{
				menupres[num].bghide = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "BACKGROUNDCOLOR"))
			{
				menupres[num].bgcolor = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDETITLEPICS") || fastcmp(word, "HIDEPICS") || fastcmp(word, "TITLEPICSHIDE"))
			{
				// true by default, except MM_MAIN
				menupres[num].hidetitlepics = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSMODE"))
			{
				if (fastcmp(word2, "USER"))
					menupres[num].ttmode = TTMODE_USER;
				else if (fastcmp(word2, "HIDE") || fastcmp(word2, "HIDDEN") || fastcmp(word2, "NONE"))
				{
					menupres[num].ttmode = TTMODE_USER;
					menupres[num].ttname[0] = 0;
					menupres[num].hidetitlepics = true;
				}
				else if (fastcmp(word2, "KART"))
					menupres[num].ttmode = TTMODE_KART;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSSCALE"))
			{
				// Don't handle Alacroix special case here; see Maincfg section.
				menupres[num].ttscale = max(1, min(8, (UINT8)get_number(word2)));
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSNAME"))
			{
				strncpy(menupres[num].ttname, word2, 9);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSX"))
			{
				menupres[num].ttx = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSY"))
			{
				menupres[num].tty = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSLOOP"))
			{
				menupres[num].ttloop = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSTICS"))
			{
				menupres[num].tttics = (UINT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLSPEED") || fastcmp(word, "TITLESCROLLXSPEED")
				|| fastcmp(word, "SCROLLSPEED") || fastcmp(word, "SCROLLXSPEED"))
			{
				menupres[num].titlescrollxspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLYSPEED") || fastcmp(word, "SCROLLYSPEED"))
			{
				menupres[num].titlescrollyspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(menupres[num].musname, word2, 7);
				menupres[num].musname[6] = 0;
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSICTRACK"))
			{
				menupres[num].mustrack = ((UINT16)value - 1);
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				// true by default except MM_MAIN
				menupres[num].muslooping = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "NOMUSIC"))
			{
				menupres[num].musstop = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "IGNOREMUSIC"))
			{
				menupres[num].musignore = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "FADESTRENGTH"))
			{
				// one-based, <= 0 means use default value. 1-32
				menupres[num].fadestrength = get_number(word2)-1;
				titlechanged = true;
			}
			else if (fastcmp(word, "NOENTERBUBBLE"))
			{
				menupres[num].enterbubble = !(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "NOEXITBUBBLE"))
			{
				menupres[num].exitbubble = !(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "ENTERTAG"))
			{
				menupres[num].entertag = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "EXITTAG"))
			{
				menupres[num].exittag = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "ENTERWIPE"))
			{
				menupres[num].enterwipe = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "EXITWIPE"))
			{
				menupres[num].exitwipe = get_number(word2);
				titlechanged = true;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readframe(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word1;
	char *word2 = NULL;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word1 = strtok(s, " ");
			if (word1)
				strupr(word1);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word1, "SPRITENUMBER") || fastcmp(word1, "SPRITENAME"))
			{
				states[num].sprite = get_sprite(word2);
			}
			else if (fastcmp(word1, "SPRITESUBNUMBER") || fastcmp(word1, "SPRITEFRAME"))
			{
				states[num].frame = (INT32)get_number(word2); // So the FF_ flags get calculated
			}
			else if (fastcmp(word1, "DURATION"))
			{
				states[num].tics = (INT32)get_number(word2); // So TICRATE can be used
			}
			else if (fastcmp(word1, "NEXT"))
			{
				states[num].nextstate = get_state(word2);
			}
			else if (fastcmp(word1, "VAR1"))
			{
				states[num].var1 = (INT32)get_number(word2);
			}
			else if (fastcmp(word1, "VAR2"))
			{
				states[num].var2 = (INT32)get_number(word2);
			}
			else if (fastcmp(word1, "ACTION"))
			{
				size_t z;
				boolean found = false;
				char actiontocompare[32];

				memset(actiontocompare, 0x00, sizeof(actiontocompare));
				strlcpy(actiontocompare, word2, sizeof (actiontocompare));
				strupr(actiontocompare);

				for (z = 0; z < 32; z++)
				{
					if (actiontocompare[z] == '\n' || actiontocompare[z] == '\r')
					{
						actiontocompare[z] = '\0';
						break;
					}
				}

				for (z = 0; actionpointers[z].name; z++)
				{
					if (actionpointers[z].action.acv == states[num].action.acv)
						break;
				}

				z = 0;
				found = LUA_SetLuaAction(&states[num], actiontocompare);
				if (!found)
					while (actionpointers[z].name)
					{
						if (fastcmp(actiontocompare, actionpointers[z].name))
						{
							states[num].action = actionpointers[z].action;
							states[num].action.acv = actionpointers[z].action.acv; // assign
							states[num].action.acp1 = actionpointers[z].action.acp1;
							found = true;
							break;
						}
						z++;
					}

				if (!found)
					deh_warning("Unknown action %s", actiontocompare);
			}
			else
				deh_warning("Frame %d: unknown word '%s'", num, word1);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

void readsound(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "SINGULAR"))
			{
				S_sfx[num].singularity = value;
			}
			else if (fastcmp(word, "PRIORITY"))
			{
				S_sfx[num].priority = value;
			}
			else if (fastcmp(word, "FLAGS"))
			{
				S_sfx[num].pitch = value;
			}
			else if (fastcmp(word, "VOLUME"))
			{
				S_sfx[num].volume = value;
			}
			else if (fastcmp(word, "CAPTION") || fastcmp(word, "DESCRIPTION"))
			{
				deh_strlcpy(S_sfx[num].caption, word2,
					sizeof(S_sfx[num].caption), va("Sound effect %d: caption", num));
			}
			else
				deh_warning("Sound %d : unknown word '%s'",num,word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

/** Checks if a game data file name for a mod is good.
 * "Good" means that it contains only alphanumerics, _, and -;
 * ends in ".dat"; has at least one character before the ".dat";
 * and is not "gamedata.dat" (tested case-insensitively).
 *
 * Assumption: that gamedata.dat is the only .dat file that will
 * ever be treated specially by the game.
 *
 * Note: Check for the tail ".dat" case-insensitively since at
 * present, we get passed the filename in all uppercase.
 *
 * \param s Filename string to check.
 * \return True if the filename is good.
 * \sa readmaincfg()
 * \author Graue <graue@oceanbase.org>
 */
static boolean GoodDataFileName(const char *s)
{
	const char *p;
	const char *tail = ".dat";

	for (p = s; *p != '\0'; p++)
		if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.')
			return false;

	p = s + strlen(s) - strlen(tail);
	if (p <= s) return false; // too short
	if (!fasticmp(p, tail)) return false; // doesn't end in .dat
	if (fasticmp(s, "gamedata.dat")) return false;

	return true;
}

void reademblemdata(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	memset(&emblemlocations[num-1], 0, sizeof(emblem_t));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			// Up here to allow lowercase in hints
			if (fastcmp(word, "HINT"))
			{
				while ((tmp = strchr(word2, '\\')))
					*tmp = '\n';
				deh_strlcpy(emblemlocations[num-1].hint, word2, sizeof (emblemlocations[num-1].hint), va("Emblem %d: hint", num));
				continue;
			}
			strupr(word2);

			if (fastcmp(word, "TYPE"))
			{
				if (fastcmp(word2, "GLOBAL"))
					emblemlocations[num-1].type = ET_GLOBAL;
				else if (fastcmp(word2, "MAP"))
					emblemlocations[num-1].type = ET_MAP;
				else if (fastcmp(word2, "TIME"))
					emblemlocations[num-1].type = ET_TIME;
				else
					emblemlocations[num-1].type = (UINT8)value;
			}
			else if (fastcmp(word, "TAG"))
				emblemlocations[num-1].tag = (INT16)value;
			else if (fastcmp(word, "MAPNUM"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);

				emblemlocations[num-1].level = (INT16)value;
			}
			else if (fastcmp(word, "SPRITE"))
			{
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = word2[0];
				else
					value += 'A'-1;

				if (value < 'A' || value > 'Z')
					deh_warning("Emblem %d: sprite must be from A - Z (1 - 26)", num);
				else
					emblemlocations[num-1].sprite = (UINT8)value;
			}
			else if (fastcmp(word, "COLOR"))
				emblemlocations[num-1].color = get_number(word2);
			else if (fastcmp(word, "VAR"))
				emblemlocations[num-1].var = get_number(word2);
			else
				deh_warning("Emblem %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f));

	// Default sprite and color definitions for lazy people like me
	if (!emblemlocations[num-1].sprite) switch (emblemlocations[num-1].type)
	{
		case ET_TIME:
			emblemlocations[num-1].sprite = 'B'; break;
		default:
			emblemlocations[num-1].sprite = 'A'; break;
	}
	if (!emblemlocations[num-1].color) switch (emblemlocations[num-1].type)
	{
		case ET_TIME: //case ET_NTIME:
			emblemlocations[num-1].color = SKINCOLOR_GREY; break;
		default:
			emblemlocations[num-1].color = SKINCOLOR_GOLD; break;
	}

	Z_Free(s);
}

void readextraemblemdata(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	memset(&extraemblems[num-1], 0, sizeof(extraemblem_t));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "NAME"))
				deh_strlcpy(extraemblems[num-1].name, word2,
					sizeof (extraemblems[num-1].name), va("Extra emblem %d: name", num));
			else if (fastcmp(word, "OBJECTIVE"))
				deh_strlcpy(extraemblems[num-1].description, word2,
					sizeof (extraemblems[num-1].description), va("Extra emblem %d: objective", num));
			else if (fastcmp(word, "CONDITIONSET"))
				extraemblems[num-1].conditionset = (UINT8)value;
			else if (fastcmp(word, "SHOWCONDITIONSET"))
				extraemblems[num-1].showconditionset = (UINT8)value;
			else
			{
				strupr(word2);
				if (fastcmp(word, "SPRITE"))
				{
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						value = word2[0];
					else
						value += 'A'-1;

					if (value < 'A' || value > 'Z')
						deh_warning("Emblem %d: sprite must be from A - Z (1 - 26)", num);
					else
						extraemblems[num-1].sprite = (UINT8)value;
				}
				else if (fastcmp(word, "COLOR"))
					extraemblems[num-1].color = get_number(word2);
				else
					deh_warning("Extra emblem %d: unknown word '%s'", num, word);
			}
		}
	} while (!myfeof(f));

	if (!extraemblems[num-1].sprite)
		extraemblems[num-1].sprite = 'C';
	if (!extraemblems[num-1].color)
		extraemblems[num-1].color = SKINCOLOR_RED;

	Z_Free(s);
}

void readunlockable(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 i;

	memset(&unlockables[num], 0, sizeof(unlockable_t));
	unlockables[num].objective[0] = '/';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			i = atoi(word2); // used for numerical settings

			if (fastcmp(word, "NAME"))
				deh_strlcpy(unlockables[num].name, word2,
					sizeof (unlockables[num].name), va("Unlockable %d: name", num));
			else if (fastcmp(word, "OBJECTIVE"))
				deh_strlcpy(unlockables[num].objective, word2,
					sizeof (unlockables[num].objective), va("Unlockable %d: objective", num));
			else
			{
				strupr(word2);

				if (fastcmp(word, "CONDITIONSET"))
					unlockables[num].conditionset = (UINT8)i;
				else if (fastcmp(word, "SHOWCONDITIONSET"))
					unlockables[num].showconditionset = (UINT8)i;
				else if (fastcmp(word, "NOCECHO"))
					unlockables[num].nocecho = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				else if (fastcmp(word, "NOCHECKLIST"))
					unlockables[num].nochecklist = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				else if (fastcmp(word, "TYPE"))
				{
					if (fastcmp(word2, "NONE"))
						unlockables[num].type = SECRET_NONE;
					else if (fastcmp(word2, "HEADER"))
						unlockables[num].type = SECRET_HEADER;
					else if (fastcmp(word2, "SKIN"))
						unlockables[num].type = SECRET_SKIN;
					else if (fastcmp(word2, "WARP"))
						unlockables[num].type = SECRET_WARP;
					else if (fastcmp(word2, "LEVELSELECT"))
						unlockables[num].type = SECRET_LEVELSELECT;
					else if (fastcmp(word2, "TIMEATTACK"))
						unlockables[num].type = SECRET_TIMEATTACK;
					else if (fastcmp(word2, "BREAKTHECAPSULES"))
						unlockables[num].type = SECRET_BREAKTHECAPSULES;
					else if (fastcmp(word2, "SOUNDTEST"))
						unlockables[num].type = SECRET_SOUNDTEST;
					else if (fastcmp(word2, "CREDITS"))
						unlockables[num].type = SECRET_CREDITS;
					else if (fastcmp(word2, "ITEMFINDER"))
						unlockables[num].type = SECRET_ITEMFINDER;
					else if (fastcmp(word2, "EMBLEMHINTS"))
						unlockables[num].type = SECRET_EMBLEMHINTS;
					else if (fastcmp(word2, "ENCORE"))
						unlockables[num].type = SECRET_ENCORE;
					else if (fastcmp(word2, "HARDSPEED"))
						unlockables[num].type = SECRET_HARDSPEED;
					else if (fastcmp(word2, "HELLATTACK"))
						unlockables[num].type = SECRET_HELLATTACK;
					else if (fastcmp(word2, "PANDORA"))
						unlockables[num].type = SECRET_PANDORA;
					else
						unlockables[num].type = (INT16)i;
				}
				else if (fastcmp(word, "VAR"))
				{
					// Support using the actual map name,
					// i.e., Level AB, Level FZ, etc.

					// Convert to map number
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						i = M_MapNumber(word2[0], word2[1]);

					unlockables[num].variable = (INT16)i;
				}
				else
					deh_warning("Unlockable %d: unknown word '%s'", num+1, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readcondition(UINT8 set, UINT32 id, char *word2)
{
	INT32 i;
	char *params[4]; // condition, requirement, extra info, extra info
	char *spos;

	conditiontype_t ty;
	INT32 re;
	INT16 x1 = 0, x2 = 0;

	INT32 offset = 0;

	spos = strtok(word2, " ");

	for (i = 0; i < 4; ++i)
	{
		if (spos != NULL)
		{
			params[i] = spos;
			spos = strtok(NULL, " ");
		}
		else
			params[i] = NULL;
	}

	if (!params[0])
	{
		deh_warning("condition line is empty");
		return;
	}

	if				(fastcmp(params[0], "PLAYTIME")
	|| (++offset && fastcmp(params[0], "MATCHESPLAYED")))
	{
		PARAMCHECK(1);
		ty = UC_PLAYTIME + offset;
		re = atoi(params[1]);
	}
	else if (fastcmp(params[0], "POWERLEVEL"))
	{
		PARAMCHECK(2);
		ty = UC_POWERLEVEL;
		re = atoi(params[1]);
		x1 = atoi(params[2]);

		if (x1 < 0 || x1 >= PWRLV_NUMTYPES)
		{
			deh_warning("Power level type %d out of range (0 - %d)", x1, PWRLV_NUMTYPES-1);
			return;
		}
	}
	else if (fastcmp(params[0], "GAMECLEAR"))
	{
		ty = UC_GAMECLEAR;
		re = (params[1]) ? atoi(params[1]) : 1;
	}
	else if (fastcmp(params[0], "OVERALLTIME"))
	{
		PARAMCHECK(1);
		ty = UC_OVERALLTIME;
		re = atoi(params[1]);
	}
	else if ((offset=0) || fastcmp(params[0], "MAPVISITED")
	||        (++offset && fastcmp(params[0], "MAPBEATEN")))
	{
		PARAMCHECK(1);
		ty = UC_MAPVISITED + offset;

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			re = M_MapNumber(params[1][0], params[1][1]);
		else
			re = atoi(params[1]);

		if (re < 0 || re >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", re, NUMMAPS);
			return;
		}
	}
	else if (fastcmp(params[0], "MAPTIME"))
	{
		PARAMCHECK(2);
		ty = UC_MAPTIME;
		re = atoi(params[2]);

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			x1 = (INT16)M_MapNumber(params[1][0], params[1][1]);
		else
			x1 = (INT16)atoi(params[1]);

		if (x1 < 0 || x1 >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", x1, NUMMAPS);
			return;
		}
	}
	else if (fastcmp(params[0], "TRIGGER"))
	{
		PARAMCHECK(1);
		ty = UC_TRIGGER;
		re = atoi(params[1]);

		// constrained by 32 bits
		if (re < 0 || re > 31)
		{
			deh_warning("Trigger ID %d out of range (0 - 31)", re);
			return;
		}
	}
	else if (fastcmp(params[0], "TOTALEMBLEMS"))
	{
		PARAMCHECK(1);
		ty = UC_TOTALEMBLEMS;
		re = atoi(params[1]);
	}
	else if (fastcmp(params[0], "EMBLEM"))
	{
		PARAMCHECK(1);
		ty = UC_EMBLEM;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXEMBLEMS)
		{
			deh_warning("Emblem %d out of range (1 - %d)", re, MAXEMBLEMS);
			return;
		}
	}
	else if (fastcmp(params[0], "EXTRAEMBLEM"))
	{
		PARAMCHECK(1);
		ty = UC_EXTRAEMBLEM;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXEXTRAEMBLEMS)
		{
			deh_warning("Extra emblem %d out of range (1 - %d)", re, MAXEXTRAEMBLEMS);
			return;
		}
	}
	else if (fastcmp(params[0], "CONDITIONSET"))
	{
		PARAMCHECK(1);
		ty = UC_CONDITIONSET;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXCONDITIONSETS)
		{
			deh_warning("Condition set %d out of range (1 - %d)", re, MAXCONDITIONSETS);
			return;
		}
	}
	else
	{
		deh_warning("Invalid condition name %s", params[0]);
		return;
	}

	M_AddRawCondition(set, (UINT8)id, ty, re, x1, x2);
}

void readconditionset(MYFILE *f, UINT8 setnum)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	UINT8 id;
	UINT8 previd = 0;

	M_ClearConditionSet(setnum);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			if (fastncmp(word, "CONDITION", 9))
			{
				id = (UINT8)atoi(word + 9);
				if (id == 0)
				{
					deh_warning("Condition set %d: unknown word '%s'", setnum, word);
					continue;
				}
				else if (previd > id)
				{
					// out of order conditions can cause problems, so enforce proper order
					deh_warning("Condition set %d: conditions are out of order, ignoring this line", setnum);
					continue;
				}
				previd = id;

				readcondition(setnum, id, word2);
			}
			else
				deh_warning("Condition set %d: unknown word '%s'", setnum, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readmaincfg(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "EXECCFG"))
			{
				if (strchr(word2, '.'))
					COM_BufAddText(va("exec \"%s\" -immediate\n", word2));
				else
				{
					lumpnum_t lumpnum;
					char newname[9];

					strncpy(newname, word2, 8);

					newname[8] = '\0';

					lumpnum = W_CheckNumForName(newname);

					if (lumpnum == LUMPERROR || W_LumpLength(lumpnum) == 0)
						CONS_Debug(DBG_SETUP, "SOC Error: script lump %s not found/not valid.\n", newname);
					else
						COM_BufInsertText(W_CacheLumpNum(lumpnum, PU_CACHE));
				}
			}

			else if (fastcmp(word, "SPSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				spstage_start = spmarathon_start = (INT16)value;
			}
			else if (fastcmp(word, "SPMARATHON_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				spmarathon_start = (INT16)value;
			}
			else if (fastcmp(word, "SSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				sstage_start = (INT16)value;
				sstage_end = (INT16)(sstage_start+7); // 7 special stages total plus one weirdo
			}
			else if (fastcmp(word, "SMPSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				smpstage_start = (INT16)value;
				smpstage_end = (INT16)(smpstage_start+6); // 7 special stages total
			}
			else if (fastcmp(word, "REDTEAM"))
			{
				skincolor_redteam = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "BLUETEAM"))
			{
				skincolor_blueteam = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "REDRING"))
			{
				skincolor_redring = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "BLUERING"))
			{
				skincolor_bluering = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "INVULNTICS"))
			{
				invulntics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "SNEAKERTICS"))
			{
				sneakertics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "FLASHINGTICS"))
			{
				flashingtics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "TAILSFLYTICS"))
			{
				tailsflytics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "UNDERWATERTICS"))
			{
				underwatertics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "SPACETIMETICS"))
			{
				spacetimetics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "EXTRALIFETICS"))
			{
				extralifetics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "NIGHTSLINKTICS"))
			{
				nightslinktics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "GAMEOVERTICS"))
			{
				gameovertics = get_number(word2);
			}
			else if (fastcmp(word, "AMMOREMOVALTICS"))
			{
				ammoremovaltics = get_number(word2);
			}
			else if (fastcmp(word, "INTROTOPLAY"))
			{
				introtoplay = (UINT8)get_number(word2);
				// range check, you morons.
				if (introtoplay > 128)
					introtoplay = 128;
				introchanged = true;
			}
			else if (fastcmp(word, "CREDITSCUTSCENE"))
			{
				creditscutscene = (UINT8)get_number(word2);
				// range check, you morons.
				if (creditscutscene > 128)
					creditscutscene = 128;
			}
			else if (fastcmp(word, "USEBLACKROCK"))
			{
				useBlackRock = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "LOOPTITLE"))
			{
				looptitle = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				titlemap = (INT16)value;
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDETITLEPICS") || fastcmp(word, "TITLEPICSHIDE"))
			{
				hidetitlepics = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSMODE"))
			{
				if (fastcmp(word2, "USER"))
					ttmode = TTMODE_USER;
				else if (fastcmp(word2, "HIDE") || fastcmp(word2, "HIDDEN") || fastcmp(word2, "NONE"))
				{
					ttmode = TTMODE_USER;
					ttname[0] = 0;
					hidetitlepics = true;
				}
				else if (fastcmp(word2, "KART"))
					ttmode = TTMODE_KART;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSNAME"))
			{
				strncpy(ttname, word2, 9);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSX"))
			{
				ttx = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSY"))
			{
				tty = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSLOOP"))
			{
				ttloop = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSTICS"))
			{
				tttics = (UINT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLSPEED") || fastcmp(word, "TITLESCROLLXSPEED"))
			{
				titlescrollxspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLYSPEED"))
			{
				titlescrollyspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "NUMDEMOS"))
			{
				numDemos = (UINT8)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "DEMODELAYTIME"))
			{
				demoDelayTime = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "DEMOIDLETIME"))
			{
				demoIdleTime = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "USE1UPSOUND"))
			{
				use1upSound = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "MAXXTRALIFE"))
			{
				maxXtraLife = (UINT8)get_number(word2);
			}

			else if (fastcmp(word, "GAMEDATA"))
			{
				size_t filenamelen;

				// Check the data filename so that mods
				// can't write arbitrary files.
				if (!GoodDataFileName(word2))
					I_Error("Maincfg: bad data file name '%s'\n", word2);

				G_SaveGameData();
				strlcpy(gamedatafilename, word2, sizeof (gamedatafilename));
				strlwr(gamedatafilename);
				savemoddata = true;
				majormods = false;

				// Also save a time attack folder
				filenamelen = strlen(gamedatafilename)-4;  // Strip off the extension
				filenamelen = min(filenamelen, sizeof (timeattackfolder));
				strncpy(timeattackfolder, gamedatafilename, filenamelen);
				timeattackfolder[min(filenamelen, sizeof (timeattackfolder) - 1)] = '\0';

				strcpy(savegamename, timeattackfolder);
				strlcat(savegamename, "%u.ssg", sizeof(savegamename));
				// can't use sprintf since there is %u in savegamename
				strcatbf(savegamename, srb2home, PATHSEP);

				strcpy(liveeventbackup, va("live%s.bkp", timeattackfolder));
				strcatbf(liveeventbackup, srb2home, PATHSEP);

				refreshdirmenu |= REFRESHDIR_GAMEDATA;
				gamedataadded = true;
				titlechanged = true;
			}
			else if (fastcmp(word, "RESETDATA"))
			{
				P_ResetData(value);
				titlechanged = true;
			}
			else if (fastcmp(word, "CUSTOMVERSION"))
			{
				strlcpy(customversionstring, word2, sizeof (customversionstring));
				//titlechanged = true;
			}
			else if (fastcmp(word, "BOOTMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				bootmap = (INT16)value;
				//titlechanged = true;
			}
			else if (fastcmp(word, "TUTORIALMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				tutorialmap = (INT16)value;
			}
			else
				deh_warning("Maincfg: unknown word '%s'", word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

void readwipes(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *pword = word;
	char *word2;
	char *tmp;
	INT32 value;
	INT32 wipeoffset;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			if (value < -1 || value > 99)
			{
				deh_warning("Wipes: bad value '%s'", word2);
				continue;
			}
			else if (value == -1)
				value = UINT8_MAX;

			// error catching
			wipeoffset = -1;

			if (fastncmp(word, "LEVEL_", 6))
			{
				pword = word + 6;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_level_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_level_final;
			}
			else if (fastncmp(word, "INTERMISSION_", 13))
			{
				pword = word + 13;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_intermission_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_intermission_final;
			}
			else if (fastncmp(word, "VOTING_", 7))
			{
				pword = word + 7;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_voting_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_voting_final;
			}
			else if (fastncmp(word, "CONTINUING_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_continuing_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_continuing_final;
			}
			else if (fastncmp(word, "TITLESCREEN_", 12))
			{
				pword = word + 12;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_titlescreen_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_titlescreen_final;
			}
			else if (fastncmp(word, "TIMEATTACK_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_timeattack_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_timeattack_final;
			}
			else if (fastncmp(word, "CREDITS_", 8))
			{
				pword = word + 8;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_credits_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_credits_final;
				else if (fastcmp(pword, "INTERMEDIATE"))
					wipeoffset = wipe_credits_intermediate;
			}
			else if (fastncmp(word, "EVALUATION_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_evaluation_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_evaluation_final;
			}
			else if (fastncmp(word, "GAMEEND_", 8))
			{
				pword = word + 8;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_gameend_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_gameend_final;
			}
			else if (fastncmp(word, "ENCORE_", 7))
			{
				pword = word + 7;
				if (fastcmp(pword, "TOINVERT"))
					wipeoffset = wipe_encore_toinvert;
				else if (fastcmp(pword, "TOWHITE"))
					wipeoffset = wipe_encore_towhite;
			}

			if (wipeoffset < 0)
			{
				deh_warning("Wipes: unknown word '%s'", word);
				continue;
			}

			if (value == UINT8_MAX
			 && (wipeoffset <= wipe_level_toblack || wipeoffset >= wipe_encore_toinvert))
			{
				 // Cannot disable non-toblack wipes
				 // (or the level toblack wipe, or the special encore wipe)
				deh_warning("Wipes: can't disable wipe of type '%s'", word);
				continue;
			}

			wipedefs[wipeoffset] = (UINT8)value;
		}
	} while (!myfeof(f));

	Z_Free(s);
}

//
// SRB2KART
//

void readcupheader(MYFILE *f, cupheader_t *cup)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word, because some things (Lua.) move it
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			i = atoi(word2); // used for numerical settings
			strupr(word2);

			if (fastcmp(word, "ICON"))
			{
				deh_strlcpy(cup->icon, word2,
					sizeof(cup->icon), va("%s Cup: icon", cup->name));
			}
			else if (fastcmp(word, "LEVELLIST"))
			{
				cup->numlevels = 0;

				tmp = strtok(word2,",");
				do {
					INT32 map = atoi(tmp);

					if (tmp[0] >= 'A' && tmp[0] <= 'Z' && tmp[2] == '\0')
						map = M_MapNumber(tmp[0], tmp[1]);

					if (!map)
						break;

					if (cup->numlevels >= MAXLEVELLIST)
					{
						deh_warning("%s Cup: reached max levellist (%d)\n", cup->name, MAXLEVELLIST);
						break;
					}

					cup->levellist[cup->numlevels] = map - 1;
					cup->numlevels++;
				} while((tmp = strtok(NULL,",")) != NULL);
			}
			else if (fastcmp(word, "BONUSGAME"))
			{
				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);
				cup->bonusgame = (INT16)i - 1;
			}
			else if (fastcmp(word, "SPECIALSTAGE"))
			{
				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);
				cup->specialstage = (INT16)i - 1;
			}
			else if (fastcmp(word, "EMERALDNUM"))
			{
				if (i >= 0 && i <= 14)
					cup->emeraldnum = (UINT8)i;
				else
					deh_warning("%s Cup: invalid emerald number %d", cup->name, i);
			}
			else if (fastcmp(word, "UNLOCKABLE"))
			{
				if (i >= 0 && i <= MAXUNLOCKABLES) // 0 for no unlock required, anything else requires something
					cup->unlockrequired = (SINT8)i - 1;
				else
					deh_warning("%s Cup: invalid unlockable number %d", cup->name, i);
			}
			else
				deh_warning("%s Cup: unknown word '%s'", cup->name, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

void readfollower(MYFILE *f)
{
	char *s;
	char *word, *word2, dname[SKINNAMESIZE+1];
	char *tmp;
	char testname[SKINNAMESIZE];

	boolean nameset;
	INT32 fallbackstate = 0;
	INT32 res;
	INT32 i;

	if (numfollowers > MAXFOLLOWERS)
	{
		deh_warning("Error: Too many followers, cannot add anymore.\n");
		return;
	}

	s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);

	// Ready the default variables for followers. We will overwrite them as we go! We won't set the name or states RIGHT HERE as this is handled down instead.
	followers[numfollowers].mode = FOLLOWERMODE_FLOAT;
	followers[numfollowers].scale = FRACUNIT;
	followers[numfollowers].bubblescale = 0; // No bubble by default
	followers[numfollowers].atangle = FixedAngle(230 * FRACUNIT);
	followers[numfollowers].dist = 32*FRACUNIT; // changed from 16 to 32 to better account for ogl models
	followers[numfollowers].height = 16*FRACUNIT;
	followers[numfollowers].zoffs = 32*FRACUNIT;
	followers[numfollowers].horzlag = 3*FRACUNIT;
	followers[numfollowers].vertlag = 6*FRACUNIT;
	followers[numfollowers].anglelag = 8*FRACUNIT;
	followers[numfollowers].bobspeed = (TICRATE*2)*FRACUNIT;
	followers[numfollowers].bobamp = 4*FRACUNIT;
	followers[numfollowers].hitconfirmtime = TICRATE;
	followers[numfollowers].defaultcolor = SKINCOLOR_GREEN;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");

			if (!word2)
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "NAME"))
			{
				strcpy(followers[numfollowers].name, word2);
				nameset = true;
			}
			else if (fastcmp(word, "MODE"))
			{
				if (word2)
					strupr(word2);

				if (fastcmp(word2, "FLOAT") || fastcmp(word2, "DEFAULT"))
					followers[numfollowers].mode = FOLLOWERMODE_FLOAT;
				else if (fastcmp(word2, "GROUND"))
					followers[numfollowers].mode = FOLLOWERMODE_GROUND;
				else
					deh_warning("Follower %d: unknown follower mode '%s'", numfollowers, word2);
			}
			else if (fastcmp(word, "DEFAULTCOLOR"))
			{
				if (word2)
					strupr(word2);

				if (fastcmp(word2, "MATCH"))
					followers[numfollowers].defaultcolor = FOLLOWERCOLOR_MATCH;
				else if (fastcmp(word2, "OPPOSITE"))
					followers[numfollowers].defaultcolor = FOLLOWERCOLOR_OPPOSITE;
				else
					followers[numfollowers].defaultcolor = get_skincolor(word2);
			}
			else if (fastcmp(word, "SCALE"))
			{
				followers[numfollowers].scale = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "BUBBLESCALE"))
			{
				followers[numfollowers].bubblescale = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "ATANGLE"))
			{
				followers[numfollowers].atangle = (angle_t)(get_number(word2) * ANG1);
			}
			else if (fastcmp(word, "HORZLAG"))
			{
				followers[numfollowers].horzlag = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "VERTLAG"))
			{
				followers[numfollowers].vertlag = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "ANGLELAG"))
			{
				followers[numfollowers].anglelag = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "BOBSPEED"))
			{
				followers[numfollowers].bobspeed = (tic_t)get_number(word2);
			}
			else if (fastcmp(word, "BOBAMP"))
			{
				followers[numfollowers].bobamp = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "ZOFFSET") || (fastcmp(word, "ZOFFS")))
			{
				followers[numfollowers].zoffs = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "DISTANCE") || (fastcmp(word, "DIST")))
			{
				followers[numfollowers].dist = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "HEIGHT"))
			{
				followers[numfollowers].height = (fixed_t)get_number(word2);
			}
			else if (fastcmp(word, "IDLESTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].idlestate = get_number(word2);
				fallbackstate = followers[numfollowers].idlestate;
			}
			else if (fastcmp(word, "FOLLOWSTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].followstate = get_number(word2);
			}
			else if (fastcmp(word, "HURTSTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].hurtstate = get_number(word2);
			}
			else if (fastcmp(word, "LOSESTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].losestate = get_number(word2);
			}
			else if (fastcmp(word, "WINSTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].winstate = get_number(word2);
			}
			else if (fastcmp(word, "HITSTATE") || (fastcmp(word, "HITCONFIRMSTATE")))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].hitconfirmstate = get_number(word2);
			}
			else if (fastcmp(word, "HITTIME") || (fastcmp(word, "HITCONFIRMTIME")))
			{
				followers[numfollowers].hitconfirmtime = (tic_t)get_number(word2);
			}
			else
			{
				deh_warning("Follower %d: unknown word '%s'", numfollowers, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	if (!nameset)
	{
		// well this is problematic.
		strcpy(followers[numfollowers].name, va("Follower%d", numfollowers)); // this is lazy, so what
	}

	// set skin name (this is just the follower's name in lowercases):
	// but before we do, let's... actually check if another follower isn't doing the same shit...

	strcpy(testname, followers[numfollowers].name);

	// lower testname for skin checks...
	strlwr(testname);
	res = K_FollowerAvailable(testname);
	if (res > -1)	// yikes, someone else has stolen our name already
	{
		INT32 startlen = strlen(testname);
		char cpy[2];
		//deh_warning("There was already a follower with the same name. (%s)", testname);	This warning probably isn't necessary anymore?
		sprintf(cpy, "%d", numfollowers);
		memcpy(&testname[startlen], cpy, 2);
		// in that case, we'll be very lazy and copy numfollowers to the end of our skin name.
	}

	strcpy(followers[numfollowers].skinname, testname);
	strcpy(dname, followers[numfollowers].skinname);	// display name, just used for printing succesful stuff or errors later down the line.

	// now that the skin name is ready, post process the actual name to turn the underscores into spaces!
	for (i = 0; followers[numfollowers].name[i]; i++)
	{
		if (followers[numfollowers].name[i] == '_')
			followers[numfollowers].name[i] = ' ';
	}

	// fallbacks for variables
	// Print a warning if the variable is on a weird value and set it back to the minimum available if that's the case.

	if (followers[numfollowers].mode < FOLLOWERMODE_FLOAT || followers[numfollowers].mode >= FOLLOWERMODE__MAX)
	{
		followers[numfollowers].mode = FOLLOWERMODE_FLOAT;
		deh_warning("Follower '%s': Value for 'mode' should be between %d and %d.", dname, FOLLOWERMODE_FLOAT, FOLLOWERMODE__MAX-1);
	}

#define FALLBACK(field, field2, threshold, set) \
if ((signed)followers[numfollowers].field < threshold) \
{ \
	followers[numfollowers].field = set; \
	deh_warning("Follower '%s': Value for '%s' is too low! Minimum should be %d. Value was overwritten to %d.", dname, field2, threshold, set); \
} \

	FALLBACK(dist, "DIST", 0, 0);
	FALLBACK(height, "HEIGHT", 1, 1);
	FALLBACK(zoffs, "ZOFFS", 0, 0);
	FALLBACK(horzlag, "HORZLAG", FRACUNIT, FRACUNIT);
	FALLBACK(vertlag, "VERTLAG", FRACUNIT, FRACUNIT);
	FALLBACK(anglelag, "ANGLELAG", FRACUNIT, FRACUNIT);
	FALLBACK(bobamp, "BOBAMP", 0, 0);
	FALLBACK(bobspeed, "BOBSPEED", 0, 0);
	FALLBACK(hitconfirmtime, "HITCONFIRMTIME", 1, 1);
	FALLBACK(scale, "SCALE", 1, 1);				// No null/negative scale
	FALLBACK(bubblescale, "BUBBLESCALE", 0, 0);	// No negative scale

#undef FALLBACK

	// Special case for color I suppose
	if (followers[numfollowers].defaultcolor > (unsigned)(numskincolors-1)
	 && followers[numfollowers].defaultcolor != FOLLOWERCOLOR_MATCH
	 && followers[numfollowers].defaultcolor != FOLLOWERCOLOR_OPPOSITE)
	{
		followers[numfollowers].defaultcolor = FOLLOWERCOLOR_MATCH;
		deh_warning("Follower \'%s\': Value for 'color' should be between 1 and %d.\n", dname, numskincolors-1);
	}

	// also check if we forgot states. If we did, we will set any missing state to the follower's idlestate.
	// Print a warning in case we don't have a fallback and set the state to S_INVISIBLE (rather than S_NULL) if unavailable.

#define NOSTATE(field, field2) \
if (!followers[numfollowers].field) \
{ \
	followers[numfollowers].field = fallbackstate ? fallbackstate : S_INVISIBLE; \
	if (!fallbackstate) \
		deh_warning("Follower '%s' is missing state definition for '%s', no idlestate fallback was found", dname, field2); \
} \

	NOSTATE(idlestate, "IDLESTATE");
	NOSTATE(followstate, "FOLLOWSTATE");
	NOSTATE(hurtstate, "HURTSTATE");
	NOSTATE(losestate, "LOSESTATE");
	NOSTATE(winstate, "WINSTATE");
	NOSTATE(hitconfirmstate, "HITCONFIRMSTATE");
#undef NOSTATE

	CONS_Printf("Added follower '%s'\n", dname);
	numfollowers++; // add 1 follower
	Z_Free(s);
}

void readweather(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			if (fastcmp(word, "TYPE"))
			{
				precipprops[num].type = get_mobjtype(word2);
			}
			else if (fastcmp(word, "EFFECTS"))
			{
				precipprops[num].effects = get_number(word2);
			}
			else
				deh_warning("Weather %d : unknown word '%s'", num, word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

//
//
//

mobjtype_t get_mobjtype(const char *word)
{ // Returns the value of MT_ enumerations
	mobjtype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("MT_",word,3))
		word += 3; // take off the MT_
	for (i = 0; i < NUMMOBJFREESLOTS; i++) {
		if (!FREE_MOBJS[i])
			break;
		if (fastcmp(word, FREE_MOBJS[i]))
			return MT_FIRSTFREESLOT+i;
	}
	for (i = 0; i < MT_FIRSTFREESLOT; i++)
		if (fastcmp(word, MOBJTYPE_LIST[i]+3))
			return i;
	deh_warning("Couldn't find mobjtype named 'MT_%s'",word);
	return MT_NULL;
}

statenum_t get_state(const char *word)
{ // Returns the value of S_ enumerations
	statenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("S_",word,2))
		word += 2; // take off the S_
	for (i = 0; i < NUMSTATEFREESLOTS; i++) {
		if (!FREE_STATES[i])
			break;
		if (fastcmp(word, FREE_STATES[i]))
			return S_FIRSTFREESLOT+i;
	}
	for (i = 0; i < S_FIRSTFREESLOT; i++)
		if (fastcmp(word, STATE_LIST[i]+2))
			return i;
	deh_warning("Couldn't find state named 'S_%s'",word);
	return S_NULL;
}

skincolornum_t get_skincolor(const char *word)
{ // Returns the value of SKINCOLOR_ enumerations
	skincolornum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SKINCOLOR_",word,10))
		word += 10; // take off the SKINCOLOR_
	for (i = 0; i < NUMCOLORFREESLOTS; i++) {
		if (!FREE_SKINCOLORS[i])
			break;
		if (fastcmp(word, FREE_SKINCOLORS[i]))
			return SKINCOLOR_FIRSTFREESLOT+i;
	}
	for (i = 0; i < SKINCOLOR_FIRSTFREESLOT; i++)
		if (fastcmp(word, COLOR_ENUMS[i]))
			return i;
	deh_warning("Couldn't find skincolor named 'SKINCOLOR_%s'",word);
	return SKINCOLOR_GREEN;
}

spritenum_t get_sprite(const char *word)
{ // Returns the value of SPR_ enumerations
	spritenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SPR_",word,4))
		word += 4; // take off the SPR_
	for (i = 0; i < NUMSPRITES; i++)
		if (!sprnames[i][4] && memcmp(word,sprnames[i],4)==0)
			return i;
	deh_warning("Couldn't find sprite named 'SPR_%s'",word);
	return SPR_NULL;
}

playersprite_t get_sprite2(const char *word)
{ // Returns the value of SPR2_ enumerations
	playersprite_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SPR2_",word,5))
		word += 5; // take off the SPR2_
	for (i = 0; i < NUMPLAYERSPRITES; i++)
		if (!spr2names[i][4] && memcmp(word,spr2names[i],4)==0)
			return i;
	deh_warning("Couldn't find sprite named 'SPR2_%s'",word);
	return SPR2_STIN;
}

sfxenum_t get_sfx(const char *word)
{ // Returns the value of SFX_ enumerations
	sfxenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SFX_",word,4))
		word += 4; // take off the SFX_
	else if (fastncmp("DS",word,2))
		word += 2; // take off the DS
	for (i = 0; i < NUMSFX; i++)
		if (S_sfx[i].name && fasticmp(word, S_sfx[i].name))
			return i;
	deh_warning("Couldn't find sfx named 'SFX_%s'",word);
	return sfx_None;
}

menutype_t get_menutype(const char *word)
{ // Returns the value of MN_ enumerations
	menutype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("MN_",word,3))
		word += 3; // take off the MN_
	for (i = 0; i < NUMMENUTYPES; i++)
		if (fastcmp(word, MENUTYPES_LIST[i]))
			return i;
	deh_warning("Couldn't find menutype named 'MN_%s'",word);
	return MN_NONE;
}

/*static INT16 get_gametype(const char *word)
{ // Returns the value of GT_ enumerations
	INT16 i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("GT_",word,3))
		word += 3; // take off the GT_
	for (i = 0; i < NUMGAMETYPES; i++)
		if (fastcmp(word, Gametype_ConstantNames[i]+3))
			return i;
	deh_warning("Couldn't find gametype named 'GT_%s'",word);
	return GT_COOP;
}*/

preciptype_t get_precip(const char *word)
{ // Returns the value of PRECIP_ enumerations
	preciptype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("PRECIP_",word,4))
		word += 7; // take off the PRECIP_
	for (i = 0; i < MAXPRECIP; i++)
	{
		if (precipprops[i].name == NULL)
			break;
		if (fasticmp(word, precipprops[i].name))
			return i;
	}
	deh_warning("Couldn't find weather type named 'PRECIP_%s'",word);
	return PRECIP_RAIN;
}

/// \todo Make ANY of this completely over-the-top math craziness obey the order of operations.
static fixed_t op_mul(fixed_t a, fixed_t b) { return a*b; }
static fixed_t op_div(fixed_t a, fixed_t b) { return a/b; }
static fixed_t op_add(fixed_t a, fixed_t b) { return a+b; }
static fixed_t op_sub(fixed_t a, fixed_t b) { return a-b; }
static fixed_t op_or(fixed_t a, fixed_t b) { return a|b; }
static fixed_t op_and(fixed_t a, fixed_t b) { return a&b; }
static fixed_t op_lshift(fixed_t a, fixed_t b) { return a<<b; }
static fixed_t op_rshift(fixed_t a, fixed_t b) { return a>>b; }

struct {
	const char c;
	fixed_t (*v)(fixed_t,fixed_t);
} OPERATIONS[] = {
	{'*',op_mul},
	{'/',op_div},
	{'+',op_add},
	{'-',op_sub},
	{'|',op_or},
	{'&',op_and},
	{'<',op_lshift},
	{'>',op_rshift},
	{0,NULL}
};

// Returns the full word, cut at the first symbol or whitespace
/*static char *read_word(const char *line)
{
	// Part 1: You got the start of the word, now find the end.
  const char *p;
	INT32 i;
	for (p = line+1; *p; p++) {
		if (*p == ' ' || *p == '\t')
			break;
		for (i = 0; OPERATIONS[i].c; i++)
			if (*p == OPERATIONS[i].c) {
				i = -1;
				break;
			}
		if (i == -1)
			break;
	}

	// Part 2: Make a copy of the word and return it.
	{
		size_t len = (p-line);
		char *word = malloc(len+1);
		M_Memcpy(word,line,len);
		word[len] = '\0';
		return word;
	}
}

static INT32 operation_pad(const char **word)
{ // Brings word the next operation and returns the operation number.
	INT32 i;
	for (; **word; (*word)++) {
		if (**word == ' ' || **word == '\t')
			continue;
		for (i = 0; OPERATIONS[i].c; i++)
			if (**word == OPERATIONS[i].c)
			{
				if ((**word == '<' && *(*word+1) == '<') || (**word == '>' && *(*word+1) == '>')) (*word)++; // These operations are two characters long.
				else if (**word == '<' || **word == '>') continue; // ... do not accept one character long.
				(*word)++;
				return i;
			}
		deh_warning("Unknown operation '%c'",**word);
		return -1;
	}
	return -1;
}

static void const_warning(const char *type, const char *word)
{
	deh_warning("Couldn't find %s named '%s'",type,word);
}

static fixed_t find_const(const char **rword)
{ // Finds the value of constants and returns it, bringing word to the next operation.
	INT32 i;
	fixed_t r;
	char *word = read_word(*rword);
	*rword += strlen(word);
	if ((*word >= '0' && *word <= '9') || *word == '-') { // Parse a number
		r = atoi(word);
		free(word);
		return r;
	}
	if (!*(word+1) && // Turn a single A-z symbol into numbers, like sprite frames.
	 ((*word >= 'A' && *word <= 'Z') || (*word >= 'a' && *word <= 'z'))) {
		r = R_Char2Frame(*word);
		free(word);
		return r;
	}
	if (fastncmp("MF_", word, 3)) {
		char *p = word+3;
		for (i = 0; MOBJFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj flag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("MF2_", word, 4)) {
		char *p = word+4;
		for (i = 0; MOBJFLAG2_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG2_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj flag2",word);
		free(word);
		return 0;
	}
	else if (fastncmp("MFE_", word, 4)) {
		char *p = word+4;
		for (i = 0; MOBJEFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJEFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj eflag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("PF_", word, 3)) {
		char *p = word+3;
		for (i = 0; PLAYERFLAG_LIST[i]; i++)
			if (fastcmp(p, PLAYERFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}
		if (fastcmp(p, "FULLSTASIS"))
			return PF_FULLSTASIS;

		// Not found error
		const_warning("player flag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("S_",word,2)) {
		r = get_state(word);
		free(word);
		return r;
	}
	else if (fastncmp("SKINCOLOR_",word,10)) {
		r = get_skincolor(word);
		free(word);
		return r;
	}
	else if (fastncmp("MT_",word,3)) {
		r = get_mobjtype(word);
		free(word);
		return r;
	}
	else if (fastncmp("SPR_",word,4)) {
		r = get_sprite(word);
		free(word);
		return r;
	}
	else if (fastncmp("SFX_",word,4) || fastncmp("DS",word,2)) {
		r = get_sfx(word);
		free(word);
		return r;
	}
	else if (fastncmp("PW_",word,3)) {
		r = get_power(word);
		free(word);
		return r;
	}
	else if (fastncmp("MN_",word,3)) {
		r = get_menutype(word);
		free(word);
		return r;
	}
	else if (fastncmp("GT_",word,3)) {
		r = get_gametype(word);
		free(word);
		return r;
	}
	else if (fastncmp("GTR_", word, 4)) {
		char *p = word+4;
		for (i = 0; GAMETYPERULE_LIST[i]; i++)
			if (fastcmp(p, GAMETYPERULE_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("game type rule",word);
		free(word);
		return 0;
	}
	else if (fastncmp("TOL_", word, 4)) {
		char *p = word+4;
		for (i = 0; TYPEOFLEVEL[i].name; i++)
			if (fastcmp(p, TYPEOFLEVEL[i].name)) {
				free(word);
				return TYPEOFLEVEL[i].flag;
			}

		// Not found error
		const_warning("typeoflevel",word);
		free(word);
		return 0;
	}
	else if (fastncmp("GRADE_",word,6))
	{
		char *p = word+6;
		for (i = 0; NIGHTSGRADE_LIST[i]; i++)
			if (*p == NIGHTSGRADE_LIST[i])
			{
				free(word);
				return i;
			}
		const_warning("NiGHTS grade",word);
		free(word);
		return 0;
	}
	for (i = 0; INT_CONST[i].n; i++)
		if (fastcmp(word,INT_CONST[i].n)) {
			free(word);
			return INT_CONST[i].v;
		}

	// Not found error.
	const_warning("constant",word);
	free(word);
	return 0;
}*/
