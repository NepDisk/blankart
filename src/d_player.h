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
/// \file  d_player.h
/// \brief player data structures

#ifndef __D_PLAYER__
#define __D_PLAYER__

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

// the player struct stores a waypoint for racing
#include "k_waypoint.h"

// Extra abilities/settings for skins (combinable stuff)
typedef enum
{
	SF_HIRES            = 1, // Draw the sprite at different size?
	SF_MACHINE          = 1<<1, // Beep boop. Are you a robot?
	// free up to and including 1<<31
} skinflags_t;

//
// Player states.
//
typedef enum
{
	// Playing or camping.
	PST_LIVE,
	// Dead on the ground, view follows killer.
	PST_DEAD,
	// Ready to restart/respawn???
	PST_REBORN
} playerstate_t;

//
// Player internal flags
//
typedef enum
{
	// free: 1<<0 to 1<<2

	// Look back VFX has been spawned
	// TODO: Is there a better way to track this?
	PF_GAINAX			= 1<<3,

	// Accessibility and cheats
	PF_KICKSTARTACCEL	= 1<<4, // Is accelerate in kickstart mode?
	PF_GODMODE			= 1<<5,
	PF_NOCLIP 			= 1<<6,

	PF_WANTSTOJOIN		= 1<<7, // Spectator that wants to join

	PF_STASIS			= 1<<8, // Player is not allowed to move
	PF_SKIDDOWN			= 1<<9, // SKIDDOWN
	PF_ELIMINATED		= 1<<10, // Battle-style elimination, no extra penalty
	PF_NOCONTEST 		= 1<<11, // Did not finish (last place explosion)
	PF_LOSTLIFE			= 1<<12, // Do not lose life more than once

	PF_RINGLOCK			= 1<<13, // Prevent picking up rings while SPB is locked on

	// The following four flags are mutually exclusive, although they can also all be off at the same time. If we ever run out of pflags, eventually turn them into a seperate five(+) mode UINT8..?
	PF_USERINGS			= 1<<14, // Have to be not holding the item button to change from using rings to using items (or vice versa) - prevents weirdness
	PF_ITEMOUT			= 1<<15, // Are you holding an item out?
	PF_EGGMANOUT		= 1<<16, // Eggman mark held, separate from PF_ITEMOUT so it doesn't stop you from getting items
	PF_HOLDREADY		= 1<<17, // Hold button-style item is ready to activate

	PF_DRIFTINPUT		= 1<<18, // Drifting!
	PF_GETSPARKS		= 1<<19, // Can get sparks
	PF_DRIFTEND			= 1<<20, // Drift has ended, used to adjust character angle after drift
	PF_BRAKEDRIFT		= 1<<21, // Helper for brake-drift spark spawning

	PF_AIRFAILSAFE		= 1<<22, // Whenever or not try the air boost
	PF_UPDATEMYRESPAWN	= 1<<23, 

	PF_FLIPCAM	= 1<<24,
	//free		= 1<<25,

	PF_HITFINISHLINE	= 1<<26, // Already hit the finish line this tic
	PF_WRONGWAY			= 1<<27, // Moving the wrong way with respect to waypoints?

	PF_SHRINKME			= 1<<28, // "Shrink me" cheat preference
	PF_SHRINKACTIVE		= 1<<29, // "Shrink me" cheat is in effect. (Can't be disabled mid-race)

	// up to 1<<31 is free
} pflags_t;

typedef enum
{
	// Are animation frames playing?
	PA_ETC=0,
	PA_STILL,
	PA_SLOW,
	PA_FAST,
	PA_DRIFT,
	PA_HURT
} panim_t;

typedef enum
{
	CR_NONE = 0,
	// Specific level gimmicks.
	CR_SLIDING,
	CR_ZOOMTUBE,
} carrytype_t; // carry

/*
To use: #define FOREACH( name, number )
Do with it whatever you want.
Run this macro, then #undef FOREACH afterward
*/
#define KART_ITEM_ITERATOR \
	FOREACH (SAD,           -1),\
	FOREACH (NONE,           0),\
	FOREACH (SNEAKER,        1),\
	FOREACH (ROCKETSNEAKER,  2),\
	FOREACH (INVINCIBILITY,  3),\
	FOREACH (BANANA,         4),\
	FOREACH (EGGMAN,         5),\
	FOREACH (ORBINAUT,       6),\
	FOREACH (JAWZ,           7),\
	FOREACH (MINE,           8),\
	FOREACH (LANDMINE,       9),\
	FOREACH (BALLHOG,       10),\
	FOREACH (SPB,           11),\
	FOREACH (GROW,          12),\
	FOREACH (SHRINK,        13),\
	FOREACH (LIGHTNINGSHIELD, 14),\
	FOREACH (BUBBLESHIELD,  15),\
	FOREACH (FLAMESHIELD,   16),\
	FOREACH (HYUDORO,       17),\
	FOREACH (POGOSPRING,    18),\
	FOREACH (SUPERRING,     19),\
	FOREACH (KITCHENSINK,   20),\
	FOREACH (DROPTARGET,    21)

typedef enum
{
#define FOREACH( name, n ) KITEM_ ## name = n
	KART_ITEM_ITERATOR,
#undef  FOREACH

	NUMKARTITEMS,

	// Additional roulette numbers, only used for K_KartGetItemResult
	KRITEM_DUALSNEAKER = NUMKARTITEMS,
	KRITEM_TRIPLESNEAKER,
	KRITEM_TRIPLEBANANA,
	KRITEM_TENFOLDBANANA,
	KRITEM_TRIPLEORBINAUT,
	KRITEM_QUADORBINAUT,
	KRITEM_DUALJAWZ,

	NUMKARTRESULTS
} kartitems_t;

typedef enum
{
	KSHIELD_NONE = 0,
	KSHIELD_LIGHTNING = 1,
	KSHIELD_BUBBLE = 2,
	KSHIELD_FLAME = 3,
	NUMKARTSHIELDS
} kartshields_t;

typedef enum
{
	KSPIN_THRUST    = (1<<0),
	KSPIN_IFRAMES   = (1<<1),
	KSPIN_AIRTIMER  = (1<<2),

	KSPIN_TYPEBIT   = (1<<3),
	KSPIN_TYPEMASK  = ~( KSPIN_TYPEBIT - 1 ),

#define KSPIN_TYPE( type ) ( KSPIN_TYPEBIT << type )

	KSPIN_SPINOUT   = KSPIN_TYPE(0)|KSPIN_IFRAMES|KSPIN_THRUST,
	KSPIN_WIPEOUT   = KSPIN_TYPE(1)|KSPIN_IFRAMES,
	KSPIN_STUNG     = KSPIN_TYPE(2),
	KSPIN_EXPLOSION = KSPIN_TYPE(3)|KSPIN_IFRAMES|KSPIN_AIRTIMER,

#undef KSPIN_TYPE
} kartspinoutflags_t;

typedef enum
{
	TRIPSTATE_NONE,
	TRIPSTATE_PASSED,
	TRIPSTATE_BLOCKED,
} tripwirestate_t;

typedef enum
{
	TRIPWIRE_NONE,
	TRIPWIRE_IGNORE,
	TRIPWIRE_BOOST,
	TRIPWIRE_BLASTER,
} tripwirepass_t;

typedef enum
{
	// Unsynced, HUD or clientsided effects
	// Item box
	khud_itemblink,		// Item flashing after roulette, serves as a mashing indicator
	khud_itemblinkmode,	// Type of flashing: 0 = white (normal), 1 = red (mashing), 2 = rainbow (enhanced items)

	// Rings
	khud_ringframe,		// Ring spin frame
	khud_ringtics,		// Tics left until next ring frame
	khud_ringdelay,		// Next frame's tics
	khud_ringspblock,	// Which frame of the SPB ring lock animation to use

	// Lap finish
	khud_lapanimation,	// Used to show the lap start wing logo animation
	khud_laphand,		// Lap hand gfx to use; 0 = none, 1 = :ok_hand:, 2 = :thumbs_up:, 3 = :thumps_down:

	// Camera
	khud_boostcam,		// Camera push forward on boost
	khud_destboostcam,	// Ditto
	khud_timeovercam,	// Camera timer for leaving behind or not

	// Sounds
	khud_enginesnd,		// Engine sound offset this player is using.
	khud_voices,		// Used to stop the player saying more voices than it should
	khud_tauntvoices,	// Used to specifically stop taunt voice spam

	// Battle
	khud_cardanimation,	// Used to determine the position of some full-screen Battle Mode graphics
	khud_yougotem, 		// "You Got Em" gfx when hitting someone as a karma player via a method that gets you back in the game instantly

	NUMKARTHUD
} karthudtype_t;

// QUICKLY GET RING TOTAL, INCLUDING RINGS CURRENTLY IN THE PICKUP ANIMATION
#define RINGTOTAL(p) (p->rings + p->pickuprings)

#define TRIPWIRETIME (7)

//}

// for kickstartaccel
#define ACCEL_KICKSTART (TICRATE)

// player_t struct for all bot variables
typedef struct botvars_s
{
	UINT8 difficulty; // Bot's difficulty setting
	UINT8 diffincrease; // In GP: bot difficulty will increase this much next round
	boolean rival; // If true, they're the GP rival

	fixed_t rubberband; // Bot rubberband value
	UINT16 controller; // Special bot controller linedef ID

	tic_t itemdelay; // Delay before using item at all
	tic_t itemconfirm; // When high enough, they will use their item

	SINT8 turnconfirm; // Confirm turn direction

} botvars_t;

typedef struct {
	tic_t enter_tic, exit_tic;
	tic_t zoom_in_speed, zoom_out_speed;
	fixed_t dist;
	angle_t pan;
	fixed_t pan_speed; // in degrees
	tic_t pan_accel, pan_back;
} sonicloopcamvars_t;

// player_t struct for loop state
typedef struct {
	fixed_t radius;
	fixed_t revolution, min_revolution, max_revolution;
	angle_t yaw;
	vector3_t origin;
	vector2_t shift;
	boolean flip;
	sonicloopcamvars_t camera;
} sonicloopvars_t;

// ========================================================================
//                          PLAYER STRUCTURE
// ========================================================================
typedef struct player_s
{
	mobj_t *mo;

	// Caveat: ticcmd_t is ATTRPACK! Be careful what precedes it.
	ticcmd_t cmd;
	ticcmd_t oldcmd; // from the previous tic

	playerstate_t playerstate;

	// Focal origin above r.z
	fixed_t viewz;
	// Base height above floor for viewz.
	fixed_t viewheight;
	// Bob/squat speed.
	fixed_t deltaviewheight;
	// bounded/scaled total momentum.
	fixed_t bob;
	fixed_t cameraOffset;

	angle_t viewrollangle;
	// camera tilt
	// TODO: expose to lua
	angle_t tilt;

	angle_t angleturn;

	// Mouse aiming, where the guy is looking at!
	// It is updated with cmd->aiming.
	angle_t aiming;

	// fun thing for player sprite
	angle_t drawangle;
	angle_t old_drawangle; // interp
	angle_t old_drawangle2;
	
	INT16 lturn_max[MAXPREDICTTICS]; // What's the expected turn value for full-left for a number of frames back (to account for netgame latency)?
	INT16 rturn_max[MAXPREDICTTICS]; // Ditto but for full-right

	// Bit flags.
	// See pflags_t, above.
	pflags_t pflags;

	// playing animation.
	panim_t panim;

	// For screen flashing (bright).
	UINT16 flashcount;
	UINT16 flashpal;

	// Player skin colorshift, 0-15 for which color to draw player.
	UINT16 skincolor;

	INT32 skin;
	UINT32 availabilities;

	UINT8 kartspeed; // Kart speed stat between 1 and 9
	UINT8 kartweight; // Kart weight stat between 1 and 9

	INT32 followerskin;		// Kart: This player's follower "skin"
	boolean followerready;	// Kart: Used to know when we can have a follower or not. (This is set on the first NameAndColor follower update)
	UINT16 followercolor;	// Kart: Used to store the follower colour the player wishes to use
	mobj_t *follower;		// Kart: This is the follower object we have. (If any)

	UINT32 charflags; // Extra abilities/settings for skins (combinable stuff)
	                 // See SF_ flags

	mobjtype_t followitem; // Object # to spawn for Smiles
	mobj_t *followmobj; // Smiles all around

	UINT32 score; // player score

	UINT16 nocontrol; //for linedef exec 427
	UINT8 carry;
	UINT16 dye;

	// SRB2kart stuff
	INT32 karthud[NUMKARTHUD];

	// Basic gameplay things
	UINT8 position;			// Used for Kart positions, mostly for deterministic stuff
	UINT8 oldposition;		// Used for taunting when you pass someone
	UINT8 positiondelay;	// Used for position number, so it can grow when passing/being passed
	UINT32 distancetofinish;
	UINT32 distancetofinishprev;
	waypoint_t *currentwaypoint;
	waypoint_t *nextwaypoint;
	tic_t airtime; 			// Keep track of how long you've been in the air
	UINT8 startboost;		// (0 to 125) - Boost you get from start of race or respawn drop dash

	// Respawn
	UINT8 dropdash;
	UINT8 respawn;
	
	UINT16 flashing;
	UINT16 spinouttimer;	// Spin-out from a banana peel or oil slick (was "pw_bananacam")
	UINT8 spinouttype;		// Determines the mode of spinout/wipeout, see kartspinoutflags_t
	UINT8 instashield;		// Instashield no-damage animation timer
	UINT8 wipeoutslow;		// Timer before you slowdown when getting wiped out
	UINT8 justbumped;		// Prevent players from endlessly bumping into each other

	SINT8 drift;			// (-5 to 5) - Drifting Left or Right, plus a bigger counter = sharper turn
	fixed_t driftcharge;	// Charge your drift so you can release a burst of speed
	UINT8 driftboost;		// (0 to 125) - Boost you get from drifting

	SINT8 aizdriftstrat;	// (-1 to 1) - Let go of your drift while boosting? Helper for the SICK STRATZ (sliptiding!) you have just unlocked
	INT32 aizdrifttilt;
	INT32 aizdriftturn;

	INT32 underwatertilt;

	fixed_t offroad;		// In Super Mario Kart, going offroad has lee-way of about 1 second before you start losing speed
	UINT8 pogospring;		// Pogo spring bounce effect
	UINT8 brakestop;		// Wait until you've made a complete stop for a few tics before letting brake go in reverse.
	UINT8 waterskip;		// Water skipping counter

	UINT8 dashpadcooldown;	// Separate the vanilla SA-style dash pads from using flashing
	
	fixed_t boostpower;		// Base boost value, for offroad
	fixed_t speedboost;		// Boost value smoothing for max speed
	fixed_t accelboost;		// Boost value smoothing for acceleration
	angle_t boostangle;		// angle set when not spun out OR boosted to determine what direction you should keep going at if you're spun out and boosted.

	UINT8 tripwireState; // see tripwirestate_t
	UINT8 tripwirePass; // see tripwirepass_t
	UINT16 tripwireLeniency;	// When reaching a state that lets you go thru tripwire, you get an extra second leniency after it ends to still go through it.
	UINT8 tripwireReboundDelay; // When failing Tripwire, brieftly lock out speed-based tripwire pass (anti-cheese)

	UINT16 itemroulette;	// Used for the roulette when deciding what item to give you (was "pw_kartitem")
	UINT8 roulettetype;		// Used for the roulette, for deciding type (0 = normal, 1 = better, 2 = eggman mark)

	// Item held stuff
	SINT8 itemtype;		// KITEM_ constant for item number
	UINT8 itemamount;	// Amount of said item
	SINT8 throwdir; 	// Held dir of controls; 1 = forward, 0 = none, -1 = backward (was "player->heldDir")

	UINT8 sadtimer;		// How long you've been sad

	// player's ring count
	SINT8 rings;
	UINT8 pickuprings;	// Number of rings being picked up before added to the counter (prevents rings from being deleted forever over 20)
	UINT8 ringdelay;	// (0 to 3) - 3 tic delay between every ring usage
	UINT16 ringboost;	// Ring boost timer
	UINT16 superring;	// Spawn rings on top of you every tic!

	UINT8 curshield;	// see kartshields_t
	UINT8 bubblecool;	// Bubble Shield use cooldown
	UINT8 bubbleblowup;	// Bubble Shield usage blowup
	UINT16 flamedash;	// Flame Shield dash power
	UINT16 flamemeter;	// Flame Shield dash meter left
	UINT8 flamelength;	// Flame Shield dash meter, number of segments

	UINT16 hyudorotimer;	// Duration of the Hyudoro offroad effect itself
	SINT8 stealingtimer;	// you are stealing
	SINT8 stolentimer;		// you are being stolen from

	UINT16 sneakertimer;	// Duration of a Sneaker Boost (from Sneakers or level boosters)
	UINT8 floorboost;		// (0 to 3) - Prevents Sneaker sounds for a brief duration when triggered by a floor panel
	boolean waterrun; 		// Tracks condition of water run

	UINT8 boostcharge; 		// Charge during race start
	
	INT16 growshrinktimer;		// > 0 = Big, < 0 = small
	INT16 growcancel;			// Duration of grow canceling
	INT16 squishedtimer;		// Duration of being squished
	UINT16 rocketsneakertimer;	// Rocket Sneaker duration timer
	UINT16 invincibilitytimer;	// Invincibility timer

	UINT8 eggmanexplode;	// Fake item recieved, explode in a few seconds
	SINT8 eggmanblame;		// (-1 to 15) - Fake item recieved, who set this fake

	UINT8 bananadrag;		// After a second of holding a banana behind you, you start to slow down

	SINT8 lastjawztarget;	// (-1 to 15) - Last person you target with jawz, for playing the target switch sfx
	UINT8 jawztargetdelay;	// (0 to 5) - Delay for Jawz target switching, to make it less twitchy

	UINT8 confirmVictim;		// Player ID that you dealt damage to
	UINT8 confirmVictimDelay;	// Delay before playing the sound

	UINT32 roundscore; // battle score this round
	UINT8 emeralds;
	UINT8 bumper;
	INT16 karmadelay;
	tic_t overtimekarma; // time to live in overtime comeback
	INT16 spheres;
	tic_t spheredigestion;

	SINT8 glanceDir; // Direction the player is trying to look backwards in

	//

	SINT8 lives;

	SINT8 xtralife; // Ring Extra Life counter

	fixed_t speed; // Player's speed (distance formula of MOMX and MOMY values)
	fixed_t lastspeed;

	INT32 deadtimer; // End game if game over lasts too long
	tic_t exiting; // Exitlevel timer

	////////////////////////////
	// Conveyor Belt Movement //
	////////////////////////////
	fixed_t cmomx; // Conveyor momx
	fixed_t cmomy; // Conveyor momy
	fixed_t rmomx; // "Real" momx (momx - cmomx)
	fixed_t rmomy; // "Real" momy (momy - cmomy)

	INT16 totalring; // Total number of rings obtained for GP
	tic_t realtime; // integer replacement for leveltime
	UINT8 laps; // Number of laps (optional)
	UINT8 latestlap;
	
	// Starpost information
	INT16 starpostx;
	INT16 starposty;
	INT16 starpostz;
	angle_t starpostangle; // Angle that the starpost is facing - you respawn facing this way
	INT32 starpostnum; // The number of the last starpost you hit
	tic_t starposttime; // NOIRE: The time of the last cheatcheck you hit
	boolean starpostflip;
	
	INT32 prevcheck; // Distance from Previous Legacy Checkpoint
	INT32 nextcheck; // Distace to Next Legacy Checkpoint

	UINT8 ctfteam; // 0 == Spectator, 1 == Red, 2 == Blue

	UINT8 checkskip; // Skipping checkpoints? Oh no no no

	INT16 lastsidehit, lastlinehit;

	//UINT8 timeshit; // That's TIMES HIT, not TIME SHIT, you doofus! -- in memoriam

	INT32 onconveyor; // You are on a conveyor belt if nonzero

	mobj_t *awayviewmobj;
	INT32 awayviewtics;
	angle_t awayviewaiming; // Used for cut-away view

	boolean spectator;
	tic_t spectatewait;		// reimplementable as UINT8 queue - How long have you been waiting as a spectator

	boolean bot;
	botvars_t botvars;

	UINT8 splitscreenindex;

	tic_t jointime; // Timer when player joins game to change skin/color
	tic_t spectatorreentry;

	tic_t grieftime;
	UINT8 griefstrikes;

	UINT8 typing_timer; // Counts down while keystrokes are not emitted
	UINT8 typing_duration; // How long since resumed timer

	UINT8 kickstartaccel;
	
	fixed_t outrun; // Milky Way road effect
	UINT8 outruntime; // Used to bypass the speed cap for fall off
	
	sonicloopvars_t loop;

#ifdef HWRENDER
	fixed_t fovadd; // adjust FOV for hw rendering
#endif
} player_t;

// Value for infinite lives
#define INFLIVES 0x7F

#endif
