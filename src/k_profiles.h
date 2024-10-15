// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 2011-2016 by Matthew "Inuyasha" Walsh.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_profiles.h
/// \brief Control profiles definition

#ifndef __PROFILES_H__
#define __PROFILES_H__

#include "doomdef.h"		// MAXPLAYERNAME
//#include "r_skins.h"		// SKINNAMESIZE	// This cuases stupid issues.
#include "g_input.h"		// Input related stuff
#include "string.h"			// strcpy etc
#include "g_game.h"			// game CVs
#include "k_follower.h"		// followers

#ifdef __cplusplus
extern "C" {
#endif

// We have to redefine this because somehow including r_skins.h causes a redefinition of node_t since that's used for both net nodes and BSP nodes too......
// And honestly I don't wanna refactor that.
#define SKINNAMESIZE 16

#define PROFILENAMELEN 6
#define PROFILEVER 2
#define MAXPROFILES 16
#define PROFILESFILE "ringprofiles.prf"
#define PROFILE_GUEST 0

#define PROFILEDEFAULTNAME "GUEST"
#define PROFILEDEFAULTPNAME "Guest"
#define PROFILEDEFAULTSKIN "eggman"
#define PROFILEDEFAULTCOLOR SKINCOLOR_NONE
#define PROFILEDEFAULTFOLLOWER "none"
#define PROFILEDEFAULTFOLLOWERCOLOR FOLLOWERCOLOR_MATCH

#define PROFILEHEADER "Doctor Robotnik's Ring Racers Profiles"

// Man I wish I had more than 16 friends!!

// profile_t definition (WIP)
// If you edit, see PR_SaveProfiles and PR_LoadProfiles
struct profile_t
{

	// Versionning
	UINT8 version;						// Version of the profile, this can be useful for backwards compatibility reading if we ever update the profile structure/format after release.
										// A version of 0 can easily be checked to identify an unitialized profile.

	// Profile header
	char profilename[PROFILENAMELEN+1];	// Profile name (not to be confused with player name)

	// Player data
	char playername[MAXPLAYERNAME+1];	// Player name
	char skinname[SKINNAMESIZE+1];		// Default Skin
	UINT16 color;						// Default player coloUr. ...But for consistency we'll name it color.
	char follower[SKINNAMESIZE+1];		// Follower
	UINT16 followercolor;				// Follower color

	UINT16 powerlevels[PWRLV_NUMTYPES];	// PWRLV for each gametype.

	// Player-specific consvars.
	// @TODO: List all of those
	boolean kickstartaccel;				// cv_kickstartaccel

	// Finally, control data itself
	INT32 controls[num_gamecontrols][MAXINPUTMAPPING];	// Lists of all the controls, defined the same way as default inputs in g_input.c
};


// Functions

// returns how many profiles there are
INT32 PR_GetNumProfiles(void);

// PR_MakeProfile
// Makes a profile from the supplied profile name, player name, colour, follower, followercolour and controls.
// The consvar values are left untouched.
profile_t* PR_MakeProfile(
	const char *prname,
	const char *pname,
	const char *sname, const UINT16 col,
	const char *fname, const UINT16 fcol,
	INT32 controlarray[num_gamecontrols][MAXINPUTMAPPING],
	boolean guest
);

// PR_MakeProfileFromPlayer
// Makes a profile_t from the supplied profile name, player name, colour, follower and followercolour.
// The last argument is a player number to read cvars from; as for convenience, cvars will be set directly when making a profile (since loading another one will overwrite them, this will be inconsequential)
profile_t* PR_MakeProfileFromPlayer(const char *prname, const char *pname, const char *sname, const UINT16 col, const char *fname, UINT16 fcol, UINT8 pnum);

// PR_AddProfile(profile_t p)
// Adds a profile to profilesList and increments numprofiles.
// Returns true if succesful, false if not.
boolean PR_AddProfile(profile_t *p);

// PR_GetProfile(INT32 num)
// Returns a pointer to the profile you're asking for or NULL if the profile is uninitialized.
profile_t* PR_GetProfile(INT32 num);

// PR_DeleteProfile(INT32 n)
// Deletes the specified profile. n cannot be 0. Returns false if the profile couldn't be deleted, true otherwise.
// This will also move every profile back accordingly to ensure the table has no empty profiles inbetween two valid profiles.
boolean PR_DeleteProfile(INT32 n);

// PR_InitNewProfile(void)
// Initializes the first new profile
void PR_InitNewProfile(void);

// PR_SaveProfiles(void)
// Saves all the profiles in profiles.cfg
// This does not save profilesList[0] since that's always going to be the default profile.
void PR_SaveProfiles(void);

// PR_LoadProfiles(void)
// Loads all the profiles saved in profiles.cfg.
// This also loads
void PR_LoadProfiles(void);

// PR_GetProfileColor(profile_t *p)
// Returns the profile's color, or the skin's prefcolor if set to none.
skincolornum_t PR_GetProfileColor(profile_t *p);

// PR_ApplyProfile(UINT8 profilenum, UINT8 playernum)
// Applies the given profile's settings to the given player.
void PR_ApplyProfile(UINT8 profilenum, UINT8 playernum);

// PR_ApplyProfileLight(UINT8 profilenum, UINT8 playernum)
// Similar to PR_ApplyProfile but only applies skin and follower values.
// Controls, kickstartaccel and "current profile" data is *not* modified.
void PR_ApplyProfileLight(UINT8 profilenum, UINT8 playernum);

// PR_ApplyProfilePretend(UINT8 profilenum, UINT8 playernum)
// ONLY modifies "current profile" data.
// Exists because any other option inteferes with rapid testing.
void PR_ApplyProfilePretend(UINT8 profilenum, UINT8 playernum);

// PR_GetProfileNum(profile_t *p)
// Gets the profile's index # in profilesList
UINT8 PR_GetProfileNum(profile_t *p);

// PR_ProfileUsedBy(profile_t *p)
// Returns the player # this profile is used by (if any)
// If the profile belongs to no player, then this returns -1
SINT8 PR_ProfileUsedBy(profile_t *p);

profile_t *PR_GetPlayerProfile(player_t *player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
