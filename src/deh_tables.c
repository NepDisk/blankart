// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  deh_tables.c
/// \brief Define DeHackEd tables.

#include "doomdef.h" // Constants
#include "s_sound.h" // Sound constants
#include "info.h" // Mobj, state, sprite, etc constants
#include "m_menu.h" // Menu constants
#include "y_inter.h" // Intermission constants
#include "p_local.h" // some more constants
#include "r_draw.h" // Colormap constants
#include "lua_script.h" // Lua stuff
#include "m_cond.h" // Emblem constants
#include "v_video.h" // video flags (for lua)
#include "i_sound.h" // musictype_t (for lua)
#include "g_state.h" // gamestate_t (for lua)
#include "r_data.h" // patchalphastyle_t
#include "k_boss.h" // spottype_t (for lua)

#include "deh_tables.h"

char *FREE_STATES[NUMSTATEFREESLOTS];
char *FREE_MOBJS[NUMMOBJFREESLOTS];
char *FREE_SKINCOLORS[NUMCOLORFREESLOTS];
UINT8 used_spr[(NUMSPRITEFREESLOTS / 8) + 1]; // Bitwise flag for sprite freeslot in use! I would use ceil() here if I could, but it only saves 1 byte of memory anyway.

const char NIGHTSGRADE_LIST[] = {
	'F', // GRADE_F
	'E', // GRADE_E
	'D', // GRADE_D
	'C', // GRADE_C
	'B', // GRADE_B
	'A', // GRADE_A
	'S', // GRADE_S
	'\0'
};

struct flickytypes_s FLICKYTYPES[] = {
	{"BLUEBIRD", MT_FLICKY_01}, // Flicky (Flicky)
	{"RABBIT",   MT_FLICKY_02}, // Pocky (1)
	{"CHICKEN",  MT_FLICKY_03}, // Cucky (1)
	{"SEAL",     MT_FLICKY_04}, // Rocky (1)
	{"PIG",      MT_FLICKY_05}, // Picky (1)
	{"CHIPMUNK", MT_FLICKY_06}, // Ricky (1)
	{"PENGUIN",  MT_FLICKY_07}, // Pecky (1)
	{"FISH",     MT_FLICKY_08}, // Nicky (CD)
	{"RAM",      MT_FLICKY_09}, // Flocky (CD)
	{"PUFFIN",   MT_FLICKY_10}, // Wicky (CD)
	{"COW",      MT_FLICKY_11}, // Macky (SRB2)
	{"RAT",      MT_FLICKY_12}, // Micky (2)
	{"BEAR",     MT_FLICKY_13}, // Becky (2)
	{"DOVE",     MT_FLICKY_14}, // Docky (CD)
	{"CAT",      MT_FLICKY_15}, // Nyannyan (Flicky)
	{"CANARY",   MT_FLICKY_16}, // Lucky (CD)
	{"a", 0}, // End of normal flickies - a lower case character so will never fastcmp valid with uppercase tmp
	//{"FLICKER",          MT_FLICKER}, // Flacky (SRB2)
	{"SPIDER",   MT_SECRETFLICKY_01}, // Sticky (SRB2)
	{"BAT",      MT_SECRETFLICKY_02}, // Backy (SRB2)
	{"SEED",                MT_SEED}, // Seed (CD)
	{NULL, 0}
};

// IMPORTANT!
// DO NOT FORGET TO SYNC THIS LIST WITH THE ACTIONNUM ENUM IN INFO.H
actionpointer_t actionpointers[] =
{
	{{A_Explode},                "A_EXPLODE"},
	{{A_Pain},                   "A_PAIN"},
	{{A_Fall},                   "A_FALL"},
	{{A_Look},                   "A_LOOK"},
	{{A_Chase},                  "A_CHASE"},
	{{A_FaceStabChase},          "A_FACESTABCHASE"},
	{{A_FaceStabRev},            "A_FACESTABREV"},
	{{A_FaceStabHurl},           "A_FACESTABHURL"},
	{{A_FaceStabMiss},           "A_FACESTABMISS"},
	{{A_StatueBurst},            "A_STATUEBURST"},
	{{A_FaceTarget},             "A_FACETARGET"},
	{{A_FaceTracer},             "A_FACETRACER"},
	{{A_Scream},                 "A_SCREAM"},
	{{A_BossDeath},              "A_BOSSDEATH"},
	{{A_RingBox},                "A_RINGBOX"},
	{{A_BunnyHop},               "A_BUNNYHOP"},
	{{A_BubbleSpawn},            "A_BUBBLESPAWN"},
	{{A_FanBubbleSpawn},         "A_FANBUBBLESPAWN"},
	{{A_BubbleRise},             "A_BUBBLERISE"},
	{{A_BubbleCheck},            "A_BUBBLECHECK"},
	{{A_AwardScore},             "A_AWARDSCORE"},
	{{A_ScoreRise},              "A_SCORERISE"},
	{{A_AttractChase},           "A_ATTRACTCHASE"},
	{{A_DropMine},               "A_DROPMINE"},
	{{A_FishJump},               "A_FISHJUMP"},
	{{A_SetSolidSteam},          "A_SETSOLIDSTEAM"},
	{{A_UnsetSolidSteam},        "A_UNSETSOLIDSTEAM"},
	{{A_OverlayThink},           "A_OVERLAYTHINK"},
	{{A_JetChase},               "A_JETCHASE"},
	{{A_JetbThink},              "A_JETBTHINK"},
	{{A_JetgThink},              "A_JETGTHINK"},
	{{A_JetgShoot},              "A_JETGSHOOT"},
	{{A_ShootBullet},            "A_SHOOTBULLET"},
	{{A_MinusDigging},           "A_MINUSDIGGING"},
	{{A_MinusPopup},             "A_MINUSPOPUP"},
	{{A_MinusCheck},             "A_MINUSCHECK"},
	{{A_ChickenCheck},           "A_CHICKENCHECK"},
	{{A_MouseThink},             "A_MOUSETHINK"},
	{{A_DetonChase},             "A_DETONCHASE"},
	{{A_CapeChase},              "A_CAPECHASE"},
	{{A_RotateSpikeBall},        "A_ROTATESPIKEBALL"},
	{{A_SlingAppear},            "A_SLINGAPPEAR"},
	{{A_UnidusBall},             "A_UNIDUSBALL"},
	{{A_RockSpawn},              "A_ROCKSPAWN"},
	{{A_SetFuse},                "A_SETFUSE"},
	{{A_CrawlaCommanderThink},   "A_CRAWLACOMMANDERTHINK"},
	{{A_SmokeTrailer},           "A_SMOKETRAILER"},
	{{A_RingExplode},            "A_RINGEXPLODE"},
	{{A_OldRingExplode},         "A_OLDRINGEXPLODE"},
	{{A_MixUp},                  "A_MIXUP"},
	{{A_Boss1Chase},             "A_BOSS1CHASE"},
	{{A_FocusTarget},            "A_FOCUSTARGET"},
	{{A_Boss2Chase},             "A_BOSS2CHASE"},
	{{A_Boss2Pogo},              "A_BOSS2POGO"},
	{{A_BossZoom},               "A_BOSSZOOM"},
	{{A_BossScream},             "A_BOSSSCREAM"},
	{{A_Boss2TakeDamage},        "A_BOSS2TAKEDAMAGE"},
	{{A_GoopSplat},              "A_GOOPSPLAT"},
	{{A_Boss2PogoSFX},           "A_BOSS2POGOSFX"},
	{{A_Boss2PogoTarget},        "A_BOSS2POGOTARGET"},
	{{A_BossJetFume},            "A_BOSSJETFUME"},
	{{A_EggmanBox},              "A_EGGMANBOX"},
	{{A_TurretFire},             "A_TURRETFIRE"},
	{{A_SuperTurretFire},        "A_SUPERTURRETFIRE"},
	{{A_TurretStop},             "A_TURRETSTOP"},
	{{A_JetJawRoam},             "A_JETJAWROAM"},
	{{A_JetJawChomp},            "A_JETJAWCHOMP"},
	{{A_PointyThink},            "A_POINTYTHINK"},
	{{A_CheckBuddy},             "A_CHECKBUDDY"},
	{{A_HoodFire},               "A_HOODFIRE"},
	{{A_HoodThink},              "A_HOODTHINK"},
	{{A_HoodFall},               "A_HOODFALL"},
	{{A_ArrowBonks},             "A_ARROWBONKS"},
	{{A_SnailerThink},           "A_SNAILERTHINK"},
	{{A_SharpChase},             "A_SHARPCHASE"},
	{{A_SharpSpin},              "A_SHARPSPIN"},
	{{A_SharpDecel},             "A_SHARPDECEL"},
	{{A_CrushstaceanWalk},       "A_CRUSHSTACEANWALK"},
	{{A_CrushstaceanPunch},      "A_CRUSHSTACEANPUNCH"},
	{{A_CrushclawAim},           "A_CRUSHCLAWAIM"},
	{{A_CrushclawLaunch},        "A_CRUSHCLAWLAUNCH"},
	{{A_VultureVtol},            "A_VULTUREVTOL"},
	{{A_VultureCheck},           "A_VULTURECHECK"},
	{{A_VultureHover},           "A_VULTUREHOVER"},
	{{A_VultureBlast},           "A_VULTUREBLAST"},
	{{A_VultureFly},             "A_VULTUREFLY"},
	{{A_SkimChase},              "A_SKIMCHASE"},
	{{A_SkullAttack},            "A_SKULLATTACK"},
	{{A_LobShot},                "A_LOBSHOT"},
	{{A_FireShot},               "A_FIRESHOT"},
	{{A_SuperFireShot},          "A_SUPERFIRESHOT"},
	{{A_BossFireShot},           "A_BOSSFIRESHOT"},
	{{A_Boss7FireMissiles},      "A_BOSS7FIREMISSILES"},
	{{A_Boss1Laser},             "A_BOSS1LASER"},
	{{A_Boss4Reverse},           "A_BOSS4REVERSE"},
	{{A_Boss4SpeedUp},           "A_BOSS4SPEEDUP"},
	{{A_Boss4Raise},             "A_BOSS4RAISE"},
	{{A_SparkFollow},            "A_SPARKFOLLOW"},
	{{A_BuzzFly},                "A_BUZZFLY"},
	{{A_GuardChase},             "A_GUARDCHASE"},
	{{A_EggShield},              "A_EGGSHIELD"},
	{{A_SetReactionTime},        "A_SETREACTIONTIME"},
	{{A_Boss1Spikeballs},        "A_BOSS1SPIKEBALLS"},
	{{A_Boss3TakeDamage},        "A_BOSS3TAKEDAMAGE"},
	{{A_Boss3Path},              "A_BOSS3PATH"},
	{{A_Boss3ShockThink},        "A_BOSS3SHOCKTHINK"},
	{{A_LinedefExecute},         "A_LINEDEFEXECUTE"},
	{{A_PlaySeeSound},           "A_PLAYSEESOUND"},
	{{A_PlayAttackSound},        "A_PLAYATTACKSOUND"},
	{{A_PlayActiveSound},        "A_PLAYACTIVESOUND"},
	{{A_SpawnObjectAbsolute},    "A_SPAWNOBJECTABSOLUTE"},
	{{A_SpawnObjectRelative},    "A_SPAWNOBJECTRELATIVE"},
	{{A_ChangeAngleRelative},    "A_CHANGEANGLERELATIVE"},
	{{A_ChangeAngleAbsolute},    "A_CHANGEANGLEABSOLUTE"},
	{{A_RollAngle},              "A_ROLLANGLE"},
	{{A_ChangeRollAngleRelative},"A_CHANGEROLLANGLERELATIVE"},
	{{A_ChangeRollAngleAbsolute},"A_CHANGEROLLANGLEABSOLUTE"},
	{{A_PlaySound},              "A_PLAYSOUND"},
	{{A_FindTarget},             "A_FINDTARGET"},
	{{A_FindTracer},             "A_FINDTRACER"},
	{{A_SetTics},                "A_SETTICS"},
	{{A_SetRandomTics},          "A_SETRANDOMTICS"},
	{{A_ChangeColorRelative},    "A_CHANGECOLORRELATIVE"},
	{{A_ChangeColorAbsolute},    "A_CHANGECOLORABSOLUTE"},
	{{A_Dye},                    "A_DYE"},
	{{A_MoveRelative},           "A_MOVERELATIVE"},
	{{A_MoveAbsolute},           "A_MOVEABSOLUTE"},
	{{A_Thrust},                 "A_THRUST"},
	{{A_ZThrust},                "A_ZTHRUST"},
	{{A_SetTargetsTarget},       "A_SETTARGETSTARGET"},
	{{A_SetObjectFlags},         "A_SETOBJECTFLAGS"},
	{{A_SetObjectFlags2},        "A_SETOBJECTFLAGS2"},
	{{A_RandomState},            "A_RANDOMSTATE"},
	{{A_RandomStateRange},       "A_RANDOMSTATERANGE"},
	{{A_StateRangeByAngle},      "A_STATERANGEBYANGLE"},
	{{A_StateRangeByParameter},  "A_STATERANGEBYPARAMETER"},
	{{A_DualAction},             "A_DUALACTION"},
	{{A_RemoteAction},           "A_REMOTEACTION"},
	{{A_ToggleFlameJet},         "A_TOGGLEFLAMEJET"},
	{{A_OrbitNights},            "A_ORBITNIGHTS"},
	{{A_GhostMe},                "A_GHOSTME"},
	{{A_SetObjectState},         "A_SETOBJECTSTATE"},
	{{A_SetObjectTypeState},     "A_SETOBJECTTYPESTATE"},
	{{A_KnockBack},              "A_KNOCKBACK"},
	{{A_PushAway},               "A_PUSHAWAY"},
	{{A_RingDrain},              "A_RINGDRAIN"},
	{{A_SplitShot},              "A_SPLITSHOT"},
	{{A_MissileSplit},           "A_MISSILESPLIT"},
	{{A_MultiShot},              "A_MULTISHOT"},
	{{A_InstaLoop},              "A_INSTALOOP"},
	{{A_Custom3DRotate},         "A_CUSTOM3DROTATE"},
	{{A_SearchForPlayers},       "A_SEARCHFORPLAYERS"},
	{{A_CheckRandom},            "A_CHECKRANDOM"},
	{{A_CheckTargetRings},       "A_CHECKTARGETRINGS"},
	{{A_CheckRings},             "A_CHECKRINGS"},
	{{A_CheckTotalRings},        "A_CHECKTOTALRINGS"},
	{{A_CheckHealth},            "A_CHECKHEALTH"},
	{{A_CheckRange},             "A_CHECKRANGE"},
	{{A_CheckHeight},            "A_CHECKHEIGHT"},
	{{A_CheckTrueRange},         "A_CHECKTRUERANGE"},
	{{A_CheckThingCount},        "A_CHECKTHINGCOUNT"},
	{{A_CheckAmbush},            "A_CHECKAMBUSH"},
	{{A_CheckCustomValue},       "A_CHECKCUSTOMVALUE"},
	{{A_CheckCusValMemo},        "A_CHECKCUSVALMEMO"},
	{{A_SetCustomValue},         "A_SETCUSTOMVALUE"},
	{{A_UseCusValMemo},          "A_USECUSVALMEMO"},
	{{A_RelayCustomValue},       "A_RELAYCUSTOMVALUE"},
	{{A_CusValAction},           "A_CUSVALACTION"},
	{{A_ForceStop},              "A_FORCESTOP"},
	{{A_ForceWin},               "A_FORCEWIN"},
	{{A_SpikeRetract},           "A_SPIKERETRACT"},
	{{A_InfoState},              "A_INFOSTATE"},
	{{A_Repeat},                 "A_REPEAT"},
	{{A_SetScale},               "A_SETSCALE"},
	{{A_RemoteDamage},           "A_REMOTEDAMAGE"},
	{{A_HomingChase},            "A_HOMINGCHASE"},
	{{A_TrapShot},               "A_TRAPSHOT"},
	{{A_VileTarget},             "A_VILETARGET"},
	{{A_VileAttack},             "A_VILEATTACK"},
	{{A_VileFire},               "A_VILEFIRE"},
	{{A_BrakChase},              "A_BRAKCHASE"},
	{{A_BrakFireShot},           "A_BRAKFIRESHOT"},
	{{A_BrakLobShot},            "A_BRAKLOBSHOT"},
	{{A_NapalmScatter},          "A_NAPALMSCATTER"},
	{{A_SpawnFreshCopy},         "A_SPAWNFRESHCOPY"},
	{{A_FlickySpawn},            "A_FLICKYSPAWN"},
	{{A_FlickyCenter},           "A_FLICKYCENTER"},
	{{A_FlickyAim},              "A_FLICKYAIM"},
	{{A_FlickyFly},              "A_FLICKYFLY"},
	{{A_FlickySoar},             "A_FLICKYSOAR"},
	{{A_FlickyCoast},            "A_FLICKYCOAST"},
	{{A_FlickyHop},              "A_FLICKYHOP"},
	{{A_FlickyFlounder},         "A_FLICKYFLOUNDER"},
	{{A_FlickyCheck},            "A_FLICKYCHECK"},
	{{A_FlickyHeightCheck},      "A_FLICKYHEIGHTCHECK"},
	{{A_FlickyFlutter},          "A_FLICKYFLUTTER"},
	{{A_FlameParticle},          "A_FLAMEPARTICLE"},
	{{A_FadeOverlay},            "A_FADEOVERLAY"},
	{{A_Boss5Jump},              "A_BOSS5JUMP"},
	{{A_LightBeamReset},         "A_LIGHTBEAMRESET"},
	{{A_MineExplode},            "A_MINEEXPLODE"},
	{{A_MineRange},              "A_MINERANGE"},
	{{A_ConnectToGround},        "A_CONNECTTOGROUND"},
	{{A_SpawnParticleRelative},  "A_SPAWNPARTICLERELATIVE"},
	{{A_MultiShotDist},          "A_MULTISHOTDIST"},
	{{A_WhoCaresIfYourSonIsABee},"A_WHOCARESIFYOURSONISABEE"},
	{{A_ParentTriesToSleep},     "A_PARENTTRIESTOSLEEP"},
	{{A_CryingToMomma},          "A_CRYINGTOMOMMA"},
	{{A_CheckFlags2},            "A_CHECKFLAGS2"},
	{{A_Boss5FindWaypoint},      "A_BOSS5FINDWAYPOINT"},
	{{A_DoNPCSkid},              "A_DONPCSKID"},
	{{A_DoNPCPain},              "A_DONPCPAIN"},
	{{A_PrepareRepeat},          "A_PREPAREREPEAT"},
	{{A_Boss5ExtraRepeat},       "A_BOSS5EXTRAREPEAT"},
	{{A_Boss5Calm},              "A_BOSS5CALM"},
	{{A_Boss5CheckOnGround},     "A_BOSS5CHECKONGROUND"},
	{{A_Boss5CheckFalling},      "A_BOSS5CHECKFALLING"},
	{{A_Boss5PinchShot},         "A_BOSS5PINCHSHOT"},
	{{A_Boss5MakeItRain},        "A_BOSS5MAKEITRAIN"},
	{{A_Boss5MakeJunk},          "A_BOSS5MAKEJUNK"},
	{{A_LookForBetter},          "A_LOOKFORBETTER"},
	{{A_Boss5BombExplode},       "A_BOSS5BOMBEXPLODE"},
	{{A_TNTExplode},             "A_TNTEXPLODE"},
	{{A_DebrisRandom},           "A_DEBRISRANDOM"},
	{{A_TrainCameo},             "A_TRAINCAMEO"},
	{{A_TrainCameo2},            "A_TRAINCAMEO2"},
	{{A_CanarivoreGas},          "A_CANARIVOREGAS"},
	{{A_KillSegments},           "A_KILLSEGMENTS"},
	{{A_SnapperSpawn},           "A_SNAPPERSPAWN"},
	{{A_SnapperThinker},         "A_SNAPPERTHINKER"},
	{{A_SaloonDoorSpawn},        "A_SALOONDOORSPAWN"},
	{{A_MinecartSparkThink},     "A_MINECARTSPARKTHINK"},
	{{A_ModuloToState},          "A_MODULOTOSTATE"},
	{{A_LavafallRocks},          "A_LAVAFALLROCKS"},
	{{A_LavafallLava},           "A_LAVAFALLLAVA"},
	{{A_FallingLavaCheck},       "A_FALLINGLAVACHECK"},
	{{A_FireShrink},             "A_FIRESHRINK"},
	{{A_SpawnPterabytes},        "A_SPAWNPTERABYTES"},
	{{A_PterabyteHover},         "A_PTERABYTEHOVER"},
	{{A_RolloutSpawn},           "A_ROLLOUTSPAWN"},
	{{A_RolloutRock},            "A_ROLLOUTROCK"},
	{{A_DragonbomberSpawn},      "A_DRAGONBOMBERSPAWN"},
	{{A_DragonWing},             "A_DRAGONWING"},
	{{A_DragonSegment},          "A_DRAGONSEGMENT"},
	{{A_ChangeHeight},           "A_CHANGEHEIGHT"},

	// SRB2Kart
	{{A_ItemPop},                "A_ITEMPOP"},
	{{A_JawzChase},              "A_JAWZCHASE"},
	{{A_JawzExplode},            "A_JAWZEXPLODE"},
	{{A_SPBChase},               "A_SPBCHASE"},
	{{A_SSMineSearch},           "A_SSMINESEARCH"},
	{{A_SSMineExplode},          "A_SSMINEEXPLODE"},
	{{A_LandMineExplode},		 "A_LANDMINEEXPLODE"},
	{{A_BallhogExplode},         "A_BALLHOGEXPLODE"},
	{{A_LightningFollowPlayer},  "A_LIGHTNINGFOLLOWPLAYER"},
	{{A_FZBoomFlash},            "A_FZBOOMFLASH"},
	{{A_FZBoomSmoke},            "A_FZBOOMSMOKE"},
	{{A_RandomShadowFrame},      "A_RANDOMSHADOWFRAME"},
	{{A_RoamingShadowThinker},   "A_ROAMINGSHADOWTHINKER"},
	{{A_MayonakaArrow},          "A_MAYONAKAARROW"},
	{{A_MementosTPParticles},    "A_MEMENTOSTPPARTICLES"},
	{{A_ReaperThinker},          "A_REAPERTHINKER"},
	{{A_FlameShieldPaper},       "A_FLAMESHIELDPAPER"},
	{{A_InvincSparkleRotate},    "A_INVINCSPARKLEROTATE"},

	{{NULL},                     "NONE"},

	// This NULL entry must be the last in the list
	{{NULL},                   NULL},
};

////////////////////////////////////////////////////////////////////////////////
// CRAZY LIST OF STATE NAMES AND ALL FROM HERE DOWN
// TODO: Make this all a seperate file or something, like part of info.c??
// TODO: Read the list from a text lump in a WAD as necessary instead
// or something, don't just keep it all in memory like this.
// TODO: Make the lists public so we can start using actual mobj
// and state names in warning and error messages! :D

// RegEx to generate this from info.h: ^\tS_([^,]+), --> \t"S_\1",
// I am leaving the prefixes solely for clarity to programmers,
// because sadly no one remembers this place while searching for full state names.
const char *const STATE_LIST[] = { // array length left dynamic for sanity testing later.
	"S_NULL",
	"S_UNKNOWN",
	"S_INVISIBLE", // state for invisible sprite

	"S_SPAWNSTATE",
	"S_SEESTATE",
	"S_MELEESTATE",
	"S_MISSILESTATE",
	"S_DEATHSTATE",
	"S_XDEATHSTATE",
	"S_RAISESTATE",

	"S_THOK",
	"S_SHADOW",

	// SRB2kart Frames
	"S_KART_STILL",
	"S_KART_STILL_L",
	"S_KART_STILL_R",
	"S_KART_STILL_GLANCE_L",
	"S_KART_STILL_GLANCE_R",
	"S_KART_STILL_LOOK_L",
	"S_KART_STILL_LOOK_R",
	"S_KART_SLOW",
	"S_KART_SLOW_L",
	"S_KART_SLOW_R",
	"S_KART_SLOW_GLANCE_L",
	"S_KART_SLOW_GLANCE_R",
	"S_KART_SLOW_LOOK_L",
	"S_KART_SLOW_LOOK_R",
	"S_KART_FAST",
	"S_KART_FAST_L",
	"S_KART_FAST_R",
	"S_KART_FAST_GLANCE_L",
	"S_KART_FAST_GLANCE_R",
	"S_KART_FAST_LOOK_L",
	"S_KART_FAST_LOOK_R",
	"S_KART_DRIFT_L",
	"S_KART_DRIFT_L_OUT",
	"S_KART_DRIFT_L_IN",
	"S_KART_DRIFT_R",
	"S_KART_DRIFT_R_OUT",
	"S_KART_DRIFT_R_IN",
	"S_KART_SPINOUT",
	"S_KART_DEAD",
	"S_KART_SIGN",

	// technically the player goes here but it's an infinite tic state
	"S_OBJPLACE_DUMMY",

	"S_KART_LEFTOVER",
	"S_KART_LEFTOVER_NOTIRES",

	"S_KART_TIRE1",
	"S_KART_TIRE2",

	// Blue Crawla
	"S_POSS_STND",
	"S_POSS_RUN1",
	"S_POSS_RUN2",
	"S_POSS_RUN3",
	"S_POSS_RUN4",
	"S_POSS_RUN5",
	"S_POSS_RUN6",

	// Red Crawla
	"S_SPOS_STND",
	"S_SPOS_RUN1",
	"S_SPOS_RUN2",
	"S_SPOS_RUN3",
	"S_SPOS_RUN4",
	"S_SPOS_RUN5",
	"S_SPOS_RUN6",

	// Greenflower Fish
	"S_FISH1",
	"S_FISH2",
	"S_FISH3",
	"S_FISH4",

	// Buzz (Gold)
	"S_BUZZLOOK1",
	"S_BUZZLOOK2",
	"S_BUZZFLY1",
	"S_BUZZFLY2",

	// Buzz (Red)
	"S_RBUZZLOOK1",
	"S_RBUZZLOOK2",
	"S_RBUZZFLY1",
	"S_RBUZZFLY2",

	// Jetty-Syn Bomber
	"S_JETBLOOK1",
	"S_JETBLOOK2",
	"S_JETBZOOM1",
	"S_JETBZOOM2",

	// Jetty-Syn Gunner
	"S_JETGLOOK1",
	"S_JETGLOOK2",
	"S_JETGZOOM1",
	"S_JETGZOOM2",
	"S_JETGSHOOT1",
	"S_JETGSHOOT2",

	// Crawla Commander
	"S_CCOMMAND1",
	"S_CCOMMAND2",
	"S_CCOMMAND3",
	"S_CCOMMAND4",

	// Deton
	"S_DETON1",
	"S_DETON2",
	"S_DETON3",
	"S_DETON4",
	"S_DETON5",
	"S_DETON6",
	"S_DETON7",
	"S_DETON8",
	"S_DETON9",
	"S_DETON10",
	"S_DETON11",
	"S_DETON12",
	"S_DETON13",
	"S_DETON14",
	"S_DETON15",

	// Skim Mine Dropper
	"S_SKIM1",
	"S_SKIM2",
	"S_SKIM3",
	"S_SKIM4",

	// THZ Turret
	"S_TURRET",
	"S_TURRETFIRE",
	"S_TURRETSHOCK1",
	"S_TURRETSHOCK2",
	"S_TURRETSHOCK3",
	"S_TURRETSHOCK4",
	"S_TURRETSHOCK5",
	"S_TURRETSHOCK6",
	"S_TURRETSHOCK7",
	"S_TURRETSHOCK8",
	"S_TURRETSHOCK9",

	// Popup Turret
	"S_TURRETLOOK",
	"S_TURRETSEE",
	"S_TURRETPOPUP1",
	"S_TURRETPOPUP2",
	"S_TURRETPOPUP3",
	"S_TURRETPOPUP4",
	"S_TURRETPOPUP5",
	"S_TURRETPOPUP6",
	"S_TURRETPOPUP7",
	"S_TURRETPOPUP8",
	"S_TURRETSHOOT",
	"S_TURRETPOPDOWN1",
	"S_TURRETPOPDOWN2",
	"S_TURRETPOPDOWN3",
	"S_TURRETPOPDOWN4",
	"S_TURRETPOPDOWN5",
	"S_TURRETPOPDOWN6",
	"S_TURRETPOPDOWN7",
	"S_TURRETPOPDOWN8",

	// Spincushion
	"S_SPINCUSHION_LOOK",
	"S_SPINCUSHION_CHASE1",
	"S_SPINCUSHION_CHASE2",
	"S_SPINCUSHION_CHASE3",
	"S_SPINCUSHION_CHASE4",
	"S_SPINCUSHION_AIM1",
	"S_SPINCUSHION_AIM2",
	"S_SPINCUSHION_AIM3",
	"S_SPINCUSHION_AIM4",
	"S_SPINCUSHION_AIM5",
	"S_SPINCUSHION_SPIN1",
	"S_SPINCUSHION_SPIN2",
	"S_SPINCUSHION_SPIN3",
	"S_SPINCUSHION_SPIN4",
	"S_SPINCUSHION_STOP1",
	"S_SPINCUSHION_STOP2",
	"S_SPINCUSHION_STOP3",
	"S_SPINCUSHION_STOP4",

	// Crushstacean
	"S_CRUSHSTACEAN_ROAM1",
	"S_CRUSHSTACEAN_ROAM2",
	"S_CRUSHSTACEAN_ROAM3",
	"S_CRUSHSTACEAN_ROAM4",
	"S_CRUSHSTACEAN_ROAMPAUSE",
	"S_CRUSHSTACEAN_PUNCH1",
	"S_CRUSHSTACEAN_PUNCH2",
	"S_CRUSHCLAW_AIM",
	"S_CRUSHCLAW_OUT",
	"S_CRUSHCLAW_STAY",
	"S_CRUSHCLAW_IN",
	"S_CRUSHCLAW_WAIT",
	"S_CRUSHCHAIN",

	// Banpyura
	"S_BANPYURA_ROAM1",
	"S_BANPYURA_ROAM2",
	"S_BANPYURA_ROAM3",
	"S_BANPYURA_ROAM4",
	"S_BANPYURA_ROAMPAUSE",
	"S_CDIAG1",
	"S_CDIAG2",
	"S_CDIAG3",
	"S_CDIAG4",
	"S_CDIAG5",
	"S_CDIAG6",
	"S_CDIAG7",
	"S_CDIAG8",

	// Jet Jaw
	"S_JETJAW_ROAM1",
	"S_JETJAW_ROAM2",
	"S_JETJAW_ROAM3",
	"S_JETJAW_ROAM4",
	"S_JETJAW_ROAM5",
	"S_JETJAW_ROAM6",
	"S_JETJAW_ROAM7",
	"S_JETJAW_ROAM8",
	"S_JETJAW_CHOMP1",
	"S_JETJAW_CHOMP2",
	"S_JETJAW_CHOMP3",
	"S_JETJAW_CHOMP4",
	"S_JETJAW_CHOMP5",
	"S_JETJAW_CHOMP6",
	"S_JETJAW_CHOMP7",
	"S_JETJAW_CHOMP8",
	"S_JETJAW_CHOMP9",
	"S_JETJAW_CHOMP10",
	"S_JETJAW_CHOMP11",
	"S_JETJAW_CHOMP12",
	"S_JETJAW_CHOMP13",
	"S_JETJAW_CHOMP14",
	"S_JETJAW_CHOMP15",
	"S_JETJAW_CHOMP16",
	"S_JETJAW_SOUND",

	// Snailer
	"S_SNAILER1",
	"S_SNAILER_FLICKY",

	// Vulture
	"S_VULTURE_STND",
	"S_VULTURE_DRIFT",
	"S_VULTURE_ZOOM1",
	"S_VULTURE_ZOOM2",
	"S_VULTURE_STUNNED",

	// Pointy
	"S_POINTY1",
	"S_POINTYBALL1",

	// Robo-Hood
	"S_ROBOHOOD_LOOK",
	"S_ROBOHOOD_STAND",
	"S_ROBOHOOD_FIRE1",
	"S_ROBOHOOD_FIRE2",
	"S_ROBOHOOD_JUMP1",
	"S_ROBOHOOD_JUMP2",
	"S_ROBOHOOD_JUMP3",

	// Castlebot Facestabber
	"S_FACESTABBER_STND1",
	"S_FACESTABBER_STND2",
	"S_FACESTABBER_STND3",
	"S_FACESTABBER_STND4",
	"S_FACESTABBER_STND5",
	"S_FACESTABBER_STND6",
	"S_FACESTABBER_CHARGE1",
	"S_FACESTABBER_CHARGE2",
	"S_FACESTABBER_CHARGE3",
	"S_FACESTABBER_CHARGE4",
	"S_FACESTABBER_PAIN",
	"S_FACESTABBER_DIE1",
	"S_FACESTABBER_DIE2",
	"S_FACESTABBER_DIE3",
	"S_FACESTABBERSPEAR",

	// Egg Guard
	"S_EGGGUARD_STND",
	"S_EGGGUARD_WALK1",
	"S_EGGGUARD_WALK2",
	"S_EGGGUARD_WALK3",
	"S_EGGGUARD_WALK4",
	"S_EGGGUARD_MAD1",
	"S_EGGGUARD_MAD2",
	"S_EGGGUARD_MAD3",
	"S_EGGGUARD_RUN1",
	"S_EGGGUARD_RUN2",
	"S_EGGGUARD_RUN3",
	"S_EGGGUARD_RUN4",

	// Egg Shield for Egg Guard
	"S_EGGSHIELD",
	"S_EGGSHIELDBREAK",

	// Green Snapper
	"S_SNAPPER_SPAWN",
	"S_SNAPPER_SPAWN2",
	"S_GSNAPPER_STND",
	"S_GSNAPPER1",
	"S_GSNAPPER2",
	"S_GSNAPPER3",
	"S_GSNAPPER4",
	"S_SNAPPER_XPLD",
	"S_SNAPPER_LEG",
	"S_SNAPPER_LEGRAISE",
	"S_SNAPPER_HEAD",

	// Minus
	"S_MINUS_INIT",
	"S_MINUS_STND",
	"S_MINUS_DIGGING1",
	"S_MINUS_DIGGING2",
	"S_MINUS_DIGGING3",
	"S_MINUS_DIGGING4",
	"S_MINUS_BURST0",
	"S_MINUS_BURST1",
	"S_MINUS_BURST2",
	"S_MINUS_BURST3",
	"S_MINUS_BURST4",
	"S_MINUS_BURST5",
	"S_MINUS_POPUP",
	"S_MINUS_AERIAL1",
	"S_MINUS_AERIAL2",
	"S_MINUS_AERIAL3",
	"S_MINUS_AERIAL4",

	// Minus dirt
	"S_MINUSDIRT1",
	"S_MINUSDIRT2",
	"S_MINUSDIRT3",
	"S_MINUSDIRT4",
	"S_MINUSDIRT5",
	"S_MINUSDIRT6",
	"S_MINUSDIRT7",

	// Spring Shell
	"S_SSHELL_STND",
	"S_SSHELL_RUN1",
	"S_SSHELL_RUN2",
	"S_SSHELL_RUN3",
	"S_SSHELL_RUN4",
	"S_SSHELL_SPRING1",
	"S_SSHELL_SPRING2",
	"S_SSHELL_SPRING3",
	"S_SSHELL_SPRING4",

	// Spring Shell (yellow)
	"S_YSHELL_STND",
	"S_YSHELL_RUN1",
	"S_YSHELL_RUN2",
	"S_YSHELL_RUN3",
	"S_YSHELL_RUN4",
	"S_YSHELL_SPRING1",
	"S_YSHELL_SPRING2",
	"S_YSHELL_SPRING3",
	"S_YSHELL_SPRING4",

	// Unidus
	"S_UNIDUS_STND",
	"S_UNIDUS_RUN",
	"S_UNIDUS_BALL",

	// Canarivore
	"S_CANARIVORE_LOOK",
	"S_CANARIVORE_AWAKEN1",
	"S_CANARIVORE_AWAKEN2",
	"S_CANARIVORE_AWAKEN3",
	"S_CANARIVORE_GAS1",
	"S_CANARIVORE_GAS2",
	"S_CANARIVORE_GAS3",
	"S_CANARIVORE_GAS4",
	"S_CANARIVORE_GAS5",
	"S_CANARIVORE_GASREPEAT",
	"S_CANARIVORE_CLOSE1",
	"S_CANARIVORE_CLOSE2",
	"S_CANARIVOREGAS_1",
	"S_CANARIVOREGAS_2",
	"S_CANARIVOREGAS_3",
	"S_CANARIVOREGAS_4",
	"S_CANARIVOREGAS_5",
	"S_CANARIVOREGAS_6",
	"S_CANARIVOREGAS_7",
	"S_CANARIVOREGAS_8",

	// Pyre Fly
	"S_PYREFLY_FLY",
	"S_PYREFLY_BURN",
	"S_PYREFIRE1",
	"S_PYREFIRE2",

	// Pterabyte
	"S_PTERABYTESPAWNER",
	"S_PTERABYTEWAYPOINT",
	"S_PTERABYTE_FLY1",
	"S_PTERABYTE_FLY2",
	"S_PTERABYTE_FLY3",
	"S_PTERABYTE_FLY4",
	"S_PTERABYTE_SWOOPDOWN",
	"S_PTERABYTE_SWOOPUP",

	// Dragonbomber
	"S_DRAGONBOMBER",
	"S_DRAGONWING1",
	"S_DRAGONWING2",
	"S_DRAGONWING3",
	"S_DRAGONWING4",
	"S_DRAGONTAIL_LOADED",
	"S_DRAGONTAIL_EMPTY",
	"S_DRAGONTAIL_EMPTYLOOP",
	"S_DRAGONTAIL_RELOAD",
	"S_DRAGONMINE",
	"S_DRAGONMINE_LAND1",
	"S_DRAGONMINE_LAND2",
	"S_DRAGONMINE_SLOWFLASH1",
	"S_DRAGONMINE_SLOWFLASH2",
	"S_DRAGONMINE_SLOWLOOP",
	"S_DRAGONMINE_FASTFLASH1",
	"S_DRAGONMINE_FASTFLASH2",
	"S_DRAGONMINE_FASTLOOP",

	// Boss Explosion
	"S_BOSSEXPLODE",

	// S3&K Boss Explosion
	"S_SONIC3KBOSSEXPLOSION1",
	"S_SONIC3KBOSSEXPLOSION2",
	"S_SONIC3KBOSSEXPLOSION3",
	"S_SONIC3KBOSSEXPLOSION4",
	"S_SONIC3KBOSSEXPLOSION5",
	"S_SONIC3KBOSSEXPLOSION6",

	"S_JETFUME1",

	// Boss 1
	"S_EGGMOBILE_STND",
	"S_EGGMOBILE_ROFL",
	"S_EGGMOBILE_LATK1",
	"S_EGGMOBILE_LATK2",
	"S_EGGMOBILE_LATK3",
	"S_EGGMOBILE_LATK4",
	"S_EGGMOBILE_LATK5",
	"S_EGGMOBILE_LATK6",
	"S_EGGMOBILE_LATK7",
	"S_EGGMOBILE_LATK8",
	"S_EGGMOBILE_LATK9",
	"S_EGGMOBILE_RATK1",
	"S_EGGMOBILE_RATK2",
	"S_EGGMOBILE_RATK3",
	"S_EGGMOBILE_RATK4",
	"S_EGGMOBILE_RATK5",
	"S_EGGMOBILE_RATK6",
	"S_EGGMOBILE_RATK7",
	"S_EGGMOBILE_RATK8",
	"S_EGGMOBILE_RATK9",
	"S_EGGMOBILE_PANIC1",
	"S_EGGMOBILE_PANIC2",
	"S_EGGMOBILE_PANIC3",
	"S_EGGMOBILE_PANIC4",
	"S_EGGMOBILE_PANIC5",
	"S_EGGMOBILE_PANIC6",
	"S_EGGMOBILE_PANIC7",
	"S_EGGMOBILE_PANIC8",
	"S_EGGMOBILE_PANIC9",
	"S_EGGMOBILE_PANIC10",
	"S_EGGMOBILE_PANIC11",
	"S_EGGMOBILE_PANIC12",
	"S_EGGMOBILE_PANIC13",
	"S_EGGMOBILE_PANIC14",
	"S_EGGMOBILE_PANIC15",
	"S_EGGMOBILE_PAIN",
	"S_EGGMOBILE_PAIN2",
	"S_EGGMOBILE_DIE1",
	"S_EGGMOBILE_DIE2",
	"S_EGGMOBILE_DIE3",
	"S_EGGMOBILE_DIE4",
	"S_EGGMOBILE_FLEE1",
	"S_EGGMOBILE_FLEE2",
	"S_EGGMOBILE_BALL",
	"S_EGGMOBILE_TARGET",

	"S_BOSSEGLZ1",
	"S_BOSSEGLZ2",

	// Boss 2
	"S_EGGMOBILE2_STND",
	"S_EGGMOBILE2_POGO1",
	"S_EGGMOBILE2_POGO2",
	"S_EGGMOBILE2_POGO3",
	"S_EGGMOBILE2_POGO4",
	"S_EGGMOBILE2_POGO5",
	"S_EGGMOBILE2_POGO6",
	"S_EGGMOBILE2_POGO7",
	"S_EGGMOBILE2_PAIN",
	"S_EGGMOBILE2_PAIN2",
	"S_EGGMOBILE2_DIE1",
	"S_EGGMOBILE2_DIE2",
	"S_EGGMOBILE2_DIE3",
	"S_EGGMOBILE2_DIE4",
	"S_EGGMOBILE2_FLEE1",
	"S_EGGMOBILE2_FLEE2",

	"S_BOSSTANK1",
	"S_BOSSTANK2",
	"S_BOSSSPIGOT",

	// Boss 2 Goop
	"S_GOOP1",
	"S_GOOP2",
	"S_GOOP3",
	"S_GOOPTRAIL",

	// Boss 3
	"S_EGGMOBILE3_STND",
	"S_EGGMOBILE3_SHOCK",
	"S_EGGMOBILE3_ATK1",
	"S_EGGMOBILE3_ATK2",
	"S_EGGMOBILE3_ATK3A",
	"S_EGGMOBILE3_ATK3B",
	"S_EGGMOBILE3_ATK3C",
	"S_EGGMOBILE3_ATK3D",
	"S_EGGMOBILE3_ATK4",
	"S_EGGMOBILE3_ATK5",
	"S_EGGMOBILE3_ROFL",
	"S_EGGMOBILE3_PAIN",
	"S_EGGMOBILE3_PAIN2",
	"S_EGGMOBILE3_DIE1",
	"S_EGGMOBILE3_DIE2",
	"S_EGGMOBILE3_DIE3",
	"S_EGGMOBILE3_DIE4",
	"S_EGGMOBILE3_FLEE1",
	"S_EGGMOBILE3_FLEE2",

	// Boss 3 Pinch
	"S_FAKEMOBILE_INIT",
	"S_FAKEMOBILE",
	"S_FAKEMOBILE_ATK1",
	"S_FAKEMOBILE_ATK2",
	"S_FAKEMOBILE_ATK3A",
	"S_FAKEMOBILE_ATK3B",
	"S_FAKEMOBILE_ATK3C",
	"S_FAKEMOBILE_ATK3D",
	"S_FAKEMOBILE_DIE1",
	"S_FAKEMOBILE_DIE2",

	"S_BOSSSEBH1",
	"S_BOSSSEBH2",

	// Boss 3 Shockwave
	"S_SHOCKWAVE1",
	"S_SHOCKWAVE2",

	// Boss 4
	"S_EGGMOBILE4_STND",
	"S_EGGMOBILE4_LATK1",
	"S_EGGMOBILE4_LATK2",
	"S_EGGMOBILE4_LATK3",
	"S_EGGMOBILE4_LATK4",
	"S_EGGMOBILE4_LATK5",
	"S_EGGMOBILE4_LATK6",
	"S_EGGMOBILE4_RATK1",
	"S_EGGMOBILE4_RATK2",
	"S_EGGMOBILE4_RATK3",
	"S_EGGMOBILE4_RATK4",
	"S_EGGMOBILE4_RATK5",
	"S_EGGMOBILE4_RATK6",
	"S_EGGMOBILE4_RAISE1",
	"S_EGGMOBILE4_RAISE2",
	"S_EGGMOBILE4_PAIN1",
	"S_EGGMOBILE4_PAIN2",
	"S_EGGMOBILE4_DIE1",
	"S_EGGMOBILE4_DIE2",
	"S_EGGMOBILE4_DIE3",
	"S_EGGMOBILE4_DIE4",
	"S_EGGMOBILE4_FLEE1",
	"S_EGGMOBILE4_FLEE2",
	"S_EGGMOBILE4_MACE",
	"S_EGGMOBILE4_MACE_DIE1",
	"S_EGGMOBILE4_MACE_DIE2",
	"S_EGGMOBILE4_MACE_DIE3",

	// Boss 4 jet flame
	"S_JETFLAME",

	// Boss 4 Spectator Eggrobo
	"S_EGGROBO1_STND",
	"S_EGGROBO1_BSLAP1",
	"S_EGGROBO1_BSLAP2",
	"S_EGGROBO1_PISSED",

	// Boss 4 Spectator Eggrobo jet flame
	"S_EGGROBOJET",

	// Boss 5
	"S_FANG_SETUP",
	"S_FANG_INTRO0",
	"S_FANG_INTRO1",
	"S_FANG_INTRO2",
	"S_FANG_INTRO3",
	"S_FANG_INTRO4",
	"S_FANG_INTRO5",
	"S_FANG_INTRO6",
	"S_FANG_INTRO7",
	"S_FANG_INTRO8",
	"S_FANG_INTRO9",
	"S_FANG_INTRO10",
	"S_FANG_INTRO11",
	"S_FANG_INTRO12",
	"S_FANG_CLONE1",
	"S_FANG_CLONE2",
	"S_FANG_CLONE3",
	"S_FANG_CLONE4",
	"S_FANG_IDLE0",
	"S_FANG_IDLE1",
	"S_FANG_IDLE2",
	"S_FANG_IDLE3",
	"S_FANG_IDLE4",
	"S_FANG_IDLE5",
	"S_FANG_IDLE6",
	"S_FANG_IDLE7",
	"S_FANG_IDLE8",
	"S_FANG_PAIN1",
	"S_FANG_PAIN2",
	"S_FANG_PATHINGSTART1",
	"S_FANG_PATHINGSTART2",
	"S_FANG_PATHING",
	"S_FANG_BOUNCE1",
	"S_FANG_BOUNCE2",
	"S_FANG_BOUNCE3",
	"S_FANG_BOUNCE4",
	"S_FANG_FALL1",
	"S_FANG_FALL2",
	"S_FANG_CHECKPATH1",
	"S_FANG_CHECKPATH2",
	"S_FANG_PATHINGCONT1",
	"S_FANG_PATHINGCONT2",
	"S_FANG_PATHINGCONT3",
	"S_FANG_SKID1",
	"S_FANG_SKID2",
	"S_FANG_SKID3",
	"S_FANG_CHOOSEATTACK",
	"S_FANG_FIRESTART1",
	"S_FANG_FIRESTART2",
	"S_FANG_FIRE1",
	"S_FANG_FIRE2",
	"S_FANG_FIRE3",
	"S_FANG_FIRE4",
	"S_FANG_FIREREPEAT",
	"S_FANG_LOBSHOT0",
	"S_FANG_LOBSHOT1",
	"S_FANG_LOBSHOT2",
	"S_FANG_WAIT1",
	"S_FANG_WAIT2",
	"S_FANG_WALLHIT",
	"S_FANG_PINCHPATHINGSTART1",
	"S_FANG_PINCHPATHINGSTART2",
	"S_FANG_PINCHPATHING",
	"S_FANG_PINCHBOUNCE0",
	"S_FANG_PINCHBOUNCE1",
	"S_FANG_PINCHBOUNCE2",
	"S_FANG_PINCHBOUNCE3",
	"S_FANG_PINCHBOUNCE4",
	"S_FANG_PINCHFALL0",
	"S_FANG_PINCHFALL1",
	"S_FANG_PINCHFALL2",
	"S_FANG_PINCHSKID1",
	"S_FANG_PINCHSKID2",
	"S_FANG_PINCHLOBSHOT0",
	"S_FANG_PINCHLOBSHOT1",
	"S_FANG_PINCHLOBSHOT2",
	"S_FANG_PINCHLOBSHOT3",
	"S_FANG_PINCHLOBSHOT4",
	"S_FANG_DIE1",
	"S_FANG_DIE2",
	"S_FANG_DIE3",
	"S_FANG_DIE4",
	"S_FANG_DIE5",
	"S_FANG_DIE6",
	"S_FANG_DIE7",
	"S_FANG_DIE8",
	"S_FANG_FLEEPATHING1",
	"S_FANG_FLEEPATHING2",
	"S_FANG_FLEEBOUNCE1",
	"S_FANG_FLEEBOUNCE2",
	"S_FANG_KO",

	"S_BROKENROBOTRANDOM",
	"S_BROKENROBOTA",
	"S_BROKENROBOTB",
	"S_BROKENROBOTC",
	"S_BROKENROBOTD",
	"S_BROKENROBOTE",
	"S_BROKENROBOTF",

	"S_ALART1",
	"S_ALART2",

	"S_VWREF",
	"S_VWREB",

	"S_PROJECTORLIGHT1",
	"S_PROJECTORLIGHT2",
	"S_PROJECTORLIGHT3",
	"S_PROJECTORLIGHT4",
	"S_PROJECTORLIGHT5",

	"S_FBOMB1",
	"S_FBOMB2",
	"S_FBOMB_EXPL1",
	"S_FBOMB_EXPL2",
	"S_FBOMB_EXPL3",
	"S_FBOMB_EXPL4",
	"S_FBOMB_EXPL5",
	"S_FBOMB_EXPL6",
	"S_TNTDUST_1",
	"S_TNTDUST_2",
	"S_TNTDUST_3",
	"S_TNTDUST_4",
	"S_TNTDUST_5",
	"S_TNTDUST_6",
	"S_TNTDUST_7",
	"S_TNTDUST_8",
	"S_FSGNA",
	"S_FSGNB",
	"S_FSGNC",
	"S_FSGND",

	// Metal Sonic (Race)
	"S_METALSONIC_RACE",
	// Metal Sonic (Battle)
	"S_METALSONIC_FLOAT",
	"S_METALSONIC_VECTOR",
	"S_METALSONIC_STUN",
	"S_METALSONIC_RAISE",
	"S_METALSONIC_GATHER",
	"S_METALSONIC_DASH",
	"S_METALSONIC_BOUNCE",
	"S_METALSONIC_BADBOUNCE",
	"S_METALSONIC_SHOOT",
	"S_METALSONIC_PAIN",
	"S_METALSONIC_DEATH1",
	"S_METALSONIC_DEATH2",
	"S_METALSONIC_DEATH3",
	"S_METALSONIC_DEATH4",
	"S_METALSONIC_FLEE1",
	"S_METALSONIC_FLEE2",

	"S_MSSHIELD_F1",
	"S_MSSHIELD_F2",

	// Ring
	"S_RING",
	"S_FASTRING1",
	"S_FASTRING2",
	"S_FASTRING3",
	"S_FASTRING4",
	"S_FASTRING5",
	"S_FASTRING6",
	"S_FASTRING7",
	"S_FASTRING8",
	"S_FASTRING9",
	"S_FASTRING10",
	"S_FASTRING11",
	"S_FASTRING12",

	// Blue Sphere
	"S_BLUESPHERE",
	"S_BLUESPHERE_SPAWN",

	"S_BLUESPHERE_BOUNCE1",
	"S_BLUESPHERE_BOUNCE2",

	"S_BLUESPHERE_BOUNCE3",
	"S_BLUESPHERE_BOUNCE4",

	"S_BLUESPHERE_BOUNCE5",
	"S_BLUESPHERE_BOUNCE6",
	"S_BLUESPHERE_BOUNCE7",
	"S_BLUESPHERE_BOUNCE8",

	"S_BLUESPHERE_BOUNCE9",
	"S_BLUESPHERE_BOUNCE10",
	"S_BLUESPHERE_BOUNCE11",
	"S_BLUESPHERE_BOUNCE12",

	"S_BLUESPHERE_BOUNCE13",
	"S_BLUESPHERE_BOUNCE14",
	"S_BLUESPHERE_BOUNCE15",
	"S_BLUESPHERE_BOUNCE16",
	"S_BLUESPHERE_BOUNCE17",
	"S_BLUESPHERE_BOUNCE18",
	"S_BLUESPHERE_BOUNCE19",
	"S_BLUESPHERE_BOUNCE20",

	"S_BLUESPHERE_BOUNCE21",
	"S_BLUESPHERE_BOUNCE22",
	"S_BLUESPHERE_BOUNCE23",
	"S_BLUESPHERE_BOUNCE24",
	"S_BLUESPHERE_BOUNCE25",
	"S_BLUESPHERE_BOUNCE26",
	"S_BLUESPHERE_BOUNCE27",
	"S_BLUESPHERE_BOUNCE28",

	// Bomb Sphere
	"S_BOMBSPHERE1",
	"S_BOMBSPHERE2",
	"S_BOMBSPHERE3",
	"S_BOMBSPHERE4",

	// NiGHTS Chip
	"S_NIGHTSCHIP",
	"S_NIGHTSCHIPBONUS",

	// NiGHTS Star
	"S_NIGHTSSTAR",
	"S_NIGHTSSTARXMAS",

	// Gravity Wells for special stages
	"S_GRAVWELLGREEN",
	"S_GRAVWELLRED",

	// Individual Team Rings
	"S_TEAMRING",

	// Special Stage Token
	"S_TOKEN",

	// CTF Flags
	"S_REDFLAG",
	"S_BLUEFLAG",

	// Emblem
	"S_EMBLEM1",
	"S_EMBLEM2",
	"S_EMBLEM3",
	"S_EMBLEM4",
	"S_EMBLEM5",
	"S_EMBLEM6",
	"S_EMBLEM7",
	"S_EMBLEM8",
	"S_EMBLEM9",
	"S_EMBLEM10",
	"S_EMBLEM11",
	"S_EMBLEM12",
	"S_EMBLEM13",
	"S_EMBLEM14",
	"S_EMBLEM15",
	"S_EMBLEM16",
	"S_EMBLEM17",
	"S_EMBLEM18",
	"S_EMBLEM19",
	"S_EMBLEM20",
	"S_EMBLEM21",
	"S_EMBLEM22",
	"S_EMBLEM23",
	"S_EMBLEM24",
	"S_EMBLEM25",
	"S_EMBLEM26",

	// Chaos Emeralds
	"S_CHAOSEMERALD1",
	"S_CHAOSEMERALD2",
	"S_CHAOSEMERALD_UNDER",

	"S_EMERALDSPARK1",
	"S_EMERALDSPARK2",
	"S_EMERALDSPARK3",
	"S_EMERALDSPARK4",
	"S_EMERALDSPARK5",
	"S_EMERALDSPARK6",
	"S_EMERALDSPARK7",

	// Emerald hunt shards
	"S_SHRD1",
	"S_SHRD2",
	"S_SHRD3",

	// Bubble Source
	"S_BUBBLES1",
	"S_BUBBLES2",
	"S_BUBBLES3",
	"S_BUBBLES4",

	// Level End Sign
	"S_SIGN_POLE",
	"S_SIGN_BACK",
	"S_SIGN_SIDE",
	"S_SIGN_FACE",
	"S_SIGN_ERROR",

	// Spike Ball
	"S_SPIKEBALL1",
	"S_SPIKEBALL2",
	"S_SPIKEBALL3",
	"S_SPIKEBALL4",
	"S_SPIKEBALL5",
	"S_SPIKEBALL6",
	"S_SPIKEBALL7",
	"S_SPIKEBALL8",

	// Elemental Shield's Spawn
	"S_SPINFIRE1",
	"S_SPINFIRE2",
	"S_SPINFIRE3",
	"S_SPINFIRE4",
	"S_SPINFIRE5",
	"S_SPINFIRE6",

	"S_TEAM_SPINFIRE1",
	"S_TEAM_SPINFIRE2",
	"S_TEAM_SPINFIRE3",
	"S_TEAM_SPINFIRE4",
	"S_TEAM_SPINFIRE5",
	"S_TEAM_SPINFIRE6",

	// Spikes
	"S_SPIKE1",
	"S_SPIKE2",
	"S_SPIKE3",
	"S_SPIKE4",
	"S_SPIKE5",
	"S_SPIKE6",
	"S_SPIKED1",
	"S_SPIKED2",

	// Wall spikes
	"S_WALLSPIKE1",
	"S_WALLSPIKE2",
	"S_WALLSPIKE3",
	"S_WALLSPIKE4",
	"S_WALLSPIKE5",
	"S_WALLSPIKE6",
	"S_WALLSPIKEBASE",
	"S_WALLSPIKED1",
	"S_WALLSPIKED2",

	// Starpost
	"S_STARPOST_IDLE",
	"S_STARPOST_FLASH",
	"S_STARPOST_STARTSPIN",
	"S_STARPOST_SPIN",
	"S_STARPOST_ENDSPIN",

	// Big floating mine
	"S_BIGMINE_IDLE",
	"S_BIGMINE_ALERT1",
	"S_BIGMINE_ALERT2",
	"S_BIGMINE_ALERT3",
	"S_BIGMINE_SET1",
	"S_BIGMINE_SET2",
	"S_BIGMINE_SET3",
	"S_BIGMINE_BLAST1",
	"S_BIGMINE_BLAST2",
	"S_BIGMINE_BLAST3",
	"S_BIGMINE_BLAST4",
	"S_BIGMINE_BLAST5",

	// Cannon Launcher
	"S_CANNONLAUNCHER1",
	"S_CANNONLAUNCHER2",
	"S_CANNONLAUNCHER3",

	// Monitor Miscellany
	"S_BOXSPARKLE1",
	"S_BOXSPARKLE2",
	"S_BOXSPARKLE3",
	"S_BOXSPARKLE4",

	"S_BOX_FLICKER",
	"S_BOX_POP1",
	"S_BOX_POP2",

	"S_GOLDBOX_FLICKER",
	"S_GOLDBOX_OFF1",
	"S_GOLDBOX_OFF2",
	"S_GOLDBOX_OFF3",
	"S_GOLDBOX_OFF4",
	"S_GOLDBOX_OFF5",
	"S_GOLDBOX_OFF6",
	"S_GOLDBOX_OFF7",

	// Monitor States (one per box)
	"S_MYSTERY_BOX",
	"S_RING_BOX",
	"S_PITY_BOX",
	"S_ATTRACT_BOX",
	"S_FORCE_BOX",
	"S_ARMAGEDDON_BOX",
	"S_WHIRLWIND_BOX",
	"S_ELEMENTAL_BOX",
	"S_SNEAKERS_BOX",
	"S_INVULN_BOX",
	"S_1UP_BOX",
	"S_EGGMAN_BOX",
	"S_MIXUP_BOX",
	"S_GRAVITY_BOX",
	"S_RECYCLER_BOX",
	"S_SCORE1K_BOX",
	"S_SCORE10K_BOX",
	"S_FLAMEAURA_BOX",
	"S_BUBBLEWRAP_BOX",
	"S_THUNDERCOIN_BOX",

	// Gold Repeat Monitor States (one per box)
	"S_PITY_GOLDBOX",
	"S_ATTRACT_GOLDBOX",
	"S_FORCE_GOLDBOX",
	"S_ARMAGEDDON_GOLDBOX",
	"S_WHIRLWIND_GOLDBOX",
	"S_ELEMENTAL_GOLDBOX",
	"S_SNEAKERS_GOLDBOX",
	"S_INVULN_GOLDBOX",
	"S_EGGMAN_GOLDBOX",
	"S_GRAVITY_GOLDBOX",
	"S_FLAMEAURA_GOLDBOX",
	"S_BUBBLEWRAP_GOLDBOX",
	"S_THUNDERCOIN_GOLDBOX",

	// Team Ring Boxes (these are special)
	"S_RING_REDBOX1",
	"S_RING_REDBOX2",
	"S_REDBOX_POP1",
	"S_REDBOX_POP2",

	"S_RING_BLUEBOX1",
	"S_RING_BLUEBOX2",
	"S_BLUEBOX_POP1",
	"S_BLUEBOX_POP2",

	// Box Icons -- 2 states each, animation and action
	"S_RING_ICON1",
	"S_RING_ICON2",

	"S_PITY_ICON1",
	"S_PITY_ICON2",

	"S_ATTRACT_ICON1",
	"S_ATTRACT_ICON2",

	"S_FORCE_ICON1",
	"S_FORCE_ICON2",

	"S_ARMAGEDDON_ICON1",
	"S_ARMAGEDDON_ICON2",

	"S_WHIRLWIND_ICON1",
	"S_WHIRLWIND_ICON2",

	"S_ELEMENTAL_ICON1",
	"S_ELEMENTAL_ICON2",

	"S_SNEAKERS_ICON1",
	"S_SNEAKERS_ICON2",

	"S_INVULN_ICON1",
	"S_INVULN_ICON2",

	"S_1UP_ICON1",
	"S_1UP_ICON2",

	"S_EGGMAN_ICON1",
	"S_EGGMAN_ICON2",

	"S_MIXUP_ICON1",
	"S_MIXUP_ICON2",

	"S_GRAVITY_ICON1",
	"S_GRAVITY_ICON2",

	"S_RECYCLER_ICON1",
	"S_RECYCLER_ICON2",

	"S_SCORE1K_ICON1",
	"S_SCORE1K_ICON2",

	"S_SCORE10K_ICON1",
	"S_SCORE10K_ICON2",

	"S_FLAMEAURA_ICON1",
	"S_FLAMEAURA_ICON2",

	"S_BUBBLEWRAP_ICON1",
	"S_BUBBLEWRAP_ICON2",

	"S_THUNDERCOIN_ICON1",
	"S_THUNDERCOIN_ICON2",

	// ---

	"S_ROCKET",

	"S_LASER",
	"S_LASER2",
	"S_LASERFLASH",

	"S_LASERFLAME1",
	"S_LASERFLAME2",
	"S_LASERFLAME3",
	"S_LASERFLAME4",
	"S_LASERFLAME5",

	"S_TORPEDO",

	"S_ENERGYBALL1",
	"S_ENERGYBALL2",

	// Skim Mine, also used by Jetty-Syn bomber
	"S_MINE1",
	"S_MINE_BOOM1",
	"S_MINE_BOOM2",
	"S_MINE_BOOM3",
	"S_MINE_BOOM4",

	// Jetty-Syn Bullet
	"S_JETBULLET1",
	"S_JETBULLET2",

	"S_TURRETLASER",
	"S_TURRETLASEREXPLODE1",
	"S_TURRETLASEREXPLODE2",

	// Cannonball
	"S_CANNONBALL1",

	// Arrow
	"S_ARROW",
	"S_ARROWBONK",

	// Glaregoyle Demon fire
	"S_DEMONFIRE",

	// The letter
	"S_LETTER",

	// GFZ flowers
	"S_GFZFLOWERA",
	"S_GFZFLOWERB",
	"S_GFZFLOWERC",

	"S_BLUEBERRYBUSH",
	"S_BERRYBUSH",
	"S_BUSH",

	// Trees (both GFZ and misc)
	"S_GFZTREE",
	"S_GFZBERRYTREE",
	"S_GFZCHERRYTREE",
	"S_CHECKERTREE",
	"S_CHECKERSUNSETTREE",
	"S_FHZTREE", // Frozen Hillside
	"S_FHZPINKTREE",
	"S_POLYGONTREE",
	"S_BUSHTREE",
	"S_BUSHREDTREE",
	"S_SPRINGTREE",

	// THZ flowers
	"S_THZFLOWERA", // THZ1 Steam flower
	"S_THZFLOWERB", // THZ1 Spin flower (red)
	"S_THZFLOWERC", // THZ1 Spin flower (yellow)

	// THZ Steam Whistle tree/bush
	"S_THZTREE",
	"S_THZTREEBRANCH1",
	"S_THZTREEBRANCH2",
	"S_THZTREEBRANCH3",
	"S_THZTREEBRANCH4",
	"S_THZTREEBRANCH5",
	"S_THZTREEBRANCH6",
	"S_THZTREEBRANCH7",
	"S_THZTREEBRANCH8",
	"S_THZTREEBRANCH9",
	"S_THZTREEBRANCH10",
	"S_THZTREEBRANCH11",
	"S_THZTREEBRANCH12",
	"S_THZTREEBRANCH13",

	// THZ Alarm
	"S_ALARM1",

	// Deep Sea Gargoyle
	"S_GARGOYLE",
	"S_BIGGARGOYLE",

	// DSZ Seaweed
	"S_SEAWEED1",
	"S_SEAWEED2",
	"S_SEAWEED3",
	"S_SEAWEED4",
	"S_SEAWEED5",
	"S_SEAWEED6",

	// Dripping Water
	"S_DRIPA1",
	"S_DRIPA2",
	"S_DRIPA3",
	"S_DRIPA4",
	"S_DRIPB1",
	"S_DRIPC1",
	"S_DRIPC2",

	// Coral
	"S_CORAL1",
	"S_CORAL2",
	"S_CORAL3",
	"S_CORAL4",
	"S_CORAL5",

	// Blue Crystal
	"S_BLUECRYSTAL1",

	// Kelp,
	"S_KELP",

	// Animated algae
	"S_ANIMALGAETOP1",
	"S_ANIMALGAETOP2",
	"S_ANIMALGAESEG",

	// DSZ Stalagmites
	"S_DSZSTALAGMITE",
	"S_DSZ2STALAGMITE",

	// DSZ Light beam
	"S_LIGHTBEAM1",
	"S_LIGHTBEAM2",
	"S_LIGHTBEAM3",
	"S_LIGHTBEAM4",
	"S_LIGHTBEAM5",
	"S_LIGHTBEAM6",
	"S_LIGHTBEAM7",
	"S_LIGHTBEAM8",
	"S_LIGHTBEAM9",
	"S_LIGHTBEAM10",
	"S_LIGHTBEAM11",
	"S_LIGHTBEAM12",

	// CEZ Chain
	"S_CEZCHAIN",

	// Flame
	"S_FLAME",
	"S_FLAMEPARTICLE",
	"S_FLAMEREST",

	// Eggman Statue
	"S_EGGSTATUE1",

	// CEZ hidden sling
	"S_SLING1",
	"S_SLING2",

	// CEZ maces and chains
	"S_SMALLMACECHAIN",
	"S_BIGMACECHAIN",
	"S_SMALLMACE",
	"S_BIGMACE",
	"S_SMALLGRABCHAIN",
	"S_BIGGRABCHAIN",

	// Yellow spring on a ball
	"S_YELLOWSPRINGBALL",
	"S_YELLOWSPRINGBALL2",
	"S_YELLOWSPRINGBALL3",
	"S_YELLOWSPRINGBALL4",
	"S_YELLOWSPRINGBALL5",

	// Red spring on a ball
	"S_REDSPRINGBALL",
	"S_REDSPRINGBALL2",
	"S_REDSPRINGBALL3",
	"S_REDSPRINGBALL4",
	"S_REDSPRINGBALL5",

	// Small Firebar
	"S_SMALLFIREBAR1",
	"S_SMALLFIREBAR2",
	"S_SMALLFIREBAR3",
	"S_SMALLFIREBAR4",
	"S_SMALLFIREBAR5",
	"S_SMALLFIREBAR6",
	"S_SMALLFIREBAR7",
	"S_SMALLFIREBAR8",
	"S_SMALLFIREBAR9",
	"S_SMALLFIREBAR10",
	"S_SMALLFIREBAR11",
	"S_SMALLFIREBAR12",
	"S_SMALLFIREBAR13",
	"S_SMALLFIREBAR14",
	"S_SMALLFIREBAR15",
	"S_SMALLFIREBAR16",

	// Big Firebar
	"S_BIGFIREBAR1",
	"S_BIGFIREBAR2",
	"S_BIGFIREBAR3",
	"S_BIGFIREBAR4",
	"S_BIGFIREBAR5",
	"S_BIGFIREBAR6",
	"S_BIGFIREBAR7",
	"S_BIGFIREBAR8",
	"S_BIGFIREBAR9",
	"S_BIGFIREBAR10",
	"S_BIGFIREBAR11",
	"S_BIGFIREBAR12",
	"S_BIGFIREBAR13",
	"S_BIGFIREBAR14",
	"S_BIGFIREBAR15",
	"S_BIGFIREBAR16",

	"S_CEZFLOWER",
	"S_CEZPOLE",
	"S_CEZBANNER1",
	"S_CEZBANNER2",
	"S_PINETREE",
	"S_CEZBUSH1",
	"S_CEZBUSH2",
	"S_CANDLE",
	"S_CANDLEPRICKET",
	"S_FLAMEHOLDER",
	"S_FIRETORCH",
	"S_WAVINGFLAG",
	"S_WAVINGFLAGSEG1",
	"S_WAVINGFLAGSEG2",
	"S_CRAWLASTATUE",
	"S_FACESTABBERSTATUE",
	"S_SUSPICIOUSFACESTABBERSTATUE_WAIT",
	"S_SUSPICIOUSFACESTABBERSTATUE_BURST1",
	"S_SUSPICIOUSFACESTABBERSTATUE_BURST2",
	"S_BRAMBLES",

	// Big Tumbleweed
	"S_BIGTUMBLEWEED",
	"S_BIGTUMBLEWEED_ROLL1",
	"S_BIGTUMBLEWEED_ROLL2",
	"S_BIGTUMBLEWEED_ROLL3",
	"S_BIGTUMBLEWEED_ROLL4",
	"S_BIGTUMBLEWEED_ROLL5",
	"S_BIGTUMBLEWEED_ROLL6",
	"S_BIGTUMBLEWEED_ROLL7",
	"S_BIGTUMBLEWEED_ROLL8",

	// Little Tumbleweed
	"S_LITTLETUMBLEWEED",
	"S_LITTLETUMBLEWEED_ROLL1",
	"S_LITTLETUMBLEWEED_ROLL2",
	"S_LITTLETUMBLEWEED_ROLL3",
	"S_LITTLETUMBLEWEED_ROLL4",
	"S_LITTLETUMBLEWEED_ROLL5",
	"S_LITTLETUMBLEWEED_ROLL6",
	"S_LITTLETUMBLEWEED_ROLL7",
	"S_LITTLETUMBLEWEED_ROLL8",

	// Cacti
	"S_CACTI1",
	"S_CACTI2",
	"S_CACTI3",
	"S_CACTI4",
	"S_CACTI5",
	"S_CACTI6",
	"S_CACTI7",
	"S_CACTI8",
	"S_CACTI9",
	"S_CACTI10",
	"S_CACTI11",
	"S_CACTITINYSEG",
	"S_CACTISMALLSEG",

	// Warning signs
	"S_ARIDSIGN_CAUTION",
	"S_ARIDSIGN_CACTI",
	"S_ARIDSIGN_SHARPTURN",

	// Oil lamp
	"S_OILLAMP",
	"S_OILLAMPFLARE",

	// TNT barrel
	"S_TNTBARREL_STND1",
	"S_TNTBARREL_EXPL1",
	"S_TNTBARREL_EXPL2",
	"S_TNTBARREL_EXPL3",
	"S_TNTBARREL_EXPL4",
	"S_TNTBARREL_EXPL5",
	"S_TNTBARREL_EXPL6",
	"S_TNTBARREL_EXPL7",
	"S_TNTBARREL_FLYING",

	// TNT proximity shell
	"S_PROXIMITY_TNT",
	"S_PROXIMITY_TNT_TRIGGER1",
	"S_PROXIMITY_TNT_TRIGGER2",
	"S_PROXIMITY_TNT_TRIGGER3",
	"S_PROXIMITY_TNT_TRIGGER4",
	"S_PROXIMITY_TNT_TRIGGER5",
	"S_PROXIMITY_TNT_TRIGGER6",
	"S_PROXIMITY_TNT_TRIGGER7",
	"S_PROXIMITY_TNT_TRIGGER8",
	"S_PROXIMITY_TNT_TRIGGER9",
	"S_PROXIMITY_TNT_TRIGGER10",
	"S_PROXIMITY_TNT_TRIGGER11",
	"S_PROXIMITY_TNT_TRIGGER12",
	"S_PROXIMITY_TNT_TRIGGER13",
	"S_PROXIMITY_TNT_TRIGGER14",
	"S_PROXIMITY_TNT_TRIGGER15",
	"S_PROXIMITY_TNT_TRIGGER16",
	"S_PROXIMITY_TNT_TRIGGER17",
	"S_PROXIMITY_TNT_TRIGGER18",
	"S_PROXIMITY_TNT_TRIGGER19",
	"S_PROXIMITY_TNT_TRIGGER20",
	"S_PROXIMITY_TNT_TRIGGER21",
	"S_PROXIMITY_TNT_TRIGGER22",
	"S_PROXIMITY_TNT_TRIGGER23",

	// Dust devil
	"S_DUSTDEVIL",
	"S_DUSTLAYER1",
	"S_DUSTLAYER2",
	"S_DUSTLAYER3",
	"S_DUSTLAYER4",
	"S_DUSTLAYER5",
	"S_ARIDDUST1",
	"S_ARIDDUST2",
	"S_ARIDDUST3",

	// Minecart
	"S_MINECART_IDLE",
	"S_MINECART_DTH1",
	"S_MINECARTEND",
	"S_MINECARTSEG_FRONT",
	"S_MINECARTSEG_BACK",
	"S_MINECARTSEG_LEFT",
	"S_MINECARTSEG_RIGHT",
	"S_MINECARTSIDEMARK1",
	"S_MINECARTSIDEMARK2",
	"S_MINECARTSPARK",

	// Saloon door
	"S_SALOONDOOR",
	"S_SALOONDOORCENTER",

	// Train cameo
	"S_TRAINCAMEOSPAWNER_1",
	"S_TRAINCAMEOSPAWNER_2",
	"S_TRAINCAMEOSPAWNER_3",
	"S_TRAINCAMEOSPAWNER_4",
	"S_TRAINCAMEOSPAWNER_5",
	"S_TRAINPUFFMAKER",

	// Train
	"S_TRAINDUST",
	"S_TRAINSTEAM",

	// Flame jet
	"S_FLAMEJETSTND",
	"S_FLAMEJETSTART",
	"S_FLAMEJETSTOP",
	"S_FLAMEJETFLAME1",
	"S_FLAMEJETFLAME2",
	"S_FLAMEJETFLAME3",
	"S_FLAMEJETFLAME4",
	"S_FLAMEJETFLAME5",
	"S_FLAMEJETFLAME6",
	"S_FLAMEJETFLAME7",
	"S_FLAMEJETFLAME8",
	"S_FLAMEJETFLAME9",

	// Spinning flame jets
	"S_FJSPINAXISA1", // Counter-clockwise
	"S_FJSPINAXISA2",
	"S_FJSPINAXISB1", // Clockwise
	"S_FJSPINAXISB2",

	// Blade's flame
	"S_FLAMEJETFLAMEB1",
	"S_FLAMEJETFLAMEB2",
	"S_FLAMEJETFLAMEB3",

	// Lavafall
	"S_LAVAFALL_DORMANT",
	"S_LAVAFALL_TELL",
	"S_LAVAFALL_SHOOT",
	"S_LAVAFALL_LAVA1",
	"S_LAVAFALL_LAVA2",
	"S_LAVAFALL_LAVA3",
	"S_LAVAFALLROCK",

	// Rollout Rock
	"S_ROLLOUTSPAWN",
	"S_ROLLOUTROCK",

	// RVZ scenery
	"S_BIGFERNLEAF",
	"S_BIGFERN1",
	"S_BIGFERN2",
	"S_JUNGLEPALM",
	"S_TORCHFLOWER",
	"S_WALLVINE_LONG",
	"S_WALLVINE_SHORT",

	// Glaregoyles
	"S_GLAREGOYLE",
	"S_GLAREGOYLE_CHARGE",
	"S_GLAREGOYLE_BLINK",
	"S_GLAREGOYLE_HOLD",
	"S_GLAREGOYLE_FIRE",
	"S_GLAREGOYLE_LOOP",
	"S_GLAREGOYLE_COOLDOWN",
	"S_GLAREGOYLEUP",
	"S_GLAREGOYLEUP_CHARGE",
	"S_GLAREGOYLEUP_BLINK",
	"S_GLAREGOYLEUP_HOLD",
	"S_GLAREGOYLEUP_FIRE",
	"S_GLAREGOYLEUP_LOOP",
	"S_GLAREGOYLEUP_COOLDOWN",
	"S_GLAREGOYLEDOWN",
	"S_GLAREGOYLEDOWN_CHARGE",
	"S_GLAREGOYLEDOWN_BLINK",
	"S_GLAREGOYLEDOWN_HOLD",
	"S_GLAREGOYLEDOWN_FIRE",
	"S_GLAREGOYLEDOWN_LOOP",
	"S_GLAREGOYLEDOWN_COOLDOWN",
	"S_GLAREGOYLELONG",
	"S_GLAREGOYLELONG_CHARGE",
	"S_GLAREGOYLELONG_BLINK",
	"S_GLAREGOYLELONG_HOLD",
	"S_GLAREGOYLELONG_FIRE",
	"S_GLAREGOYLELONG_LOOP",
	"S_GLAREGOYLELONG_COOLDOWN",

	// ATZ's Red Crystal/Target
	"S_TARGET_IDLE",
	"S_TARGET_HIT1",
	"S_TARGET_HIT2",
	"S_TARGET_RESPAWN",
	"S_TARGET_ALLDONE",

	// ATZ's green flame
	"S_GREENFLAME",

	// ATZ Blue Gargoyle
	"S_BLUEGARGOYLE",

	// Stalagmites
	"S_STG0",
	"S_STG1",
	"S_STG2",
	"S_STG3",
	"S_STG4",
	"S_STG5",
	"S_STG6",
	"S_STG7",
	"S_STG8",
	"S_STG9",

	// Xmas-specific stuff
	"S_XMASPOLE",
	"S_CANDYCANE",
	"S_SNOWMAN",    // normal
	"S_SNOWMANHAT", // with hat + scarf
	"S_LAMPPOST1",  // normal
	"S_LAMPPOST2",  // with snow
	"S_HANGSTAR",
	"S_MISTLETOE",
	// Xmas GFZ bushes
	"S_XMASBLUEBERRYBUSH",
	"S_XMASBERRYBUSH",
	"S_XMASBUSH",
	// FHZ
	"S_FHZICE1",
	"S_FHZICE2",
	"S_ROSY_IDLE1",
	"S_ROSY_IDLE2",
	"S_ROSY_IDLE3",
	"S_ROSY_IDLE4",
	"S_ROSY_JUMP",
	"S_ROSY_WALK",
	"S_ROSY_HUG",
	"S_ROSY_PAIN",
	"S_ROSY_STND",
	"S_ROSY_UNHAPPY",

	// Halloween Scenery
	// Pumpkins
	"S_JACKO1",
	"S_JACKO1OVERLAY_1",
	"S_JACKO1OVERLAY_2",
	"S_JACKO1OVERLAY_3",
	"S_JACKO1OVERLAY_4",
	"S_JACKO2",
	"S_JACKO2OVERLAY_1",
	"S_JACKO2OVERLAY_2",
	"S_JACKO2OVERLAY_3",
	"S_JACKO2OVERLAY_4",
	"S_JACKO3",
	"S_JACKO3OVERLAY_1",
	"S_JACKO3OVERLAY_2",
	"S_JACKO3OVERLAY_3",
	"S_JACKO3OVERLAY_4",
	// Dr Seuss Trees
	"S_HHZTREE_TOP",
	"S_HHZTREE_TRUNK",
	"S_HHZTREE_LEAF",
	// Mushroom
	"S_HHZSHROOM_1",
	"S_HHZSHROOM_2",
	"S_HHZSHROOM_3",
	"S_HHZSHROOM_4",
	"S_HHZSHROOM_5",
	"S_HHZSHROOM_6",
	"S_HHZSHROOM_7",
	"S_HHZSHROOM_8",
	"S_HHZSHROOM_9",
	"S_HHZSHROOM_10",
	"S_HHZSHROOM_11",
	"S_HHZSHROOM_12",
	"S_HHZSHROOM_13",
	"S_HHZSHROOM_14",
	"S_HHZSHROOM_15",
	"S_HHZSHROOM_16",
	// Misc
	"S_HHZGRASS",
	"S_HHZTENT1",
	"S_HHZTENT2",
	"S_HHZSTALAGMITE_TALL",
	"S_HHZSTALAGMITE_SHORT",

	// Botanic Serenity's loads of scenery states
	"S_BSZTALLFLOWER_RED",
	"S_BSZTALLFLOWER_PURPLE",
	"S_BSZTALLFLOWER_BLUE",
	"S_BSZTALLFLOWER_CYAN",
	"S_BSZTALLFLOWER_YELLOW",
	"S_BSZTALLFLOWER_ORANGE",
	"S_BSZFLOWER_RED",
	"S_BSZFLOWER_PURPLE",
	"S_BSZFLOWER_BLUE",
	"S_BSZFLOWER_CYAN",
	"S_BSZFLOWER_YELLOW",
	"S_BSZFLOWER_ORANGE",
	"S_BSZSHORTFLOWER_RED",
	"S_BSZSHORTFLOWER_PURPLE",
	"S_BSZSHORTFLOWER_BLUE",
	"S_BSZSHORTFLOWER_CYAN",
	"S_BSZSHORTFLOWER_YELLOW",
	"S_BSZSHORTFLOWER_ORANGE",
	"S_BSZTULIP_RED",
	"S_BSZTULIP_PURPLE",
	"S_BSZTULIP_BLUE",
	"S_BSZTULIP_CYAN",
	"S_BSZTULIP_YELLOW",
	"S_BSZTULIP_ORANGE",
	"S_BSZCLUSTER_RED",
	"S_BSZCLUSTER_PURPLE",
	"S_BSZCLUSTER_BLUE",
	"S_BSZCLUSTER_CYAN",
	"S_BSZCLUSTER_YELLOW",
	"S_BSZCLUSTER_ORANGE",
	"S_BSZBUSH_RED",
	"S_BSZBUSH_PURPLE",
	"S_BSZBUSH_BLUE",
	"S_BSZBUSH_CYAN",
	"S_BSZBUSH_YELLOW",
	"S_BSZBUSH_ORANGE",
	"S_BSZVINE_RED",
	"S_BSZVINE_PURPLE",
	"S_BSZVINE_BLUE",
	"S_BSZVINE_CYAN",
	"S_BSZVINE_YELLOW",
	"S_BSZVINE_ORANGE",
	"S_BSZSHRUB",
	"S_BSZCLOVER",
	"S_BIG_PALMTREE_TRUNK",
	"S_BIG_PALMTREE_TOP",
	"S_PALMTREE_TRUNK",
	"S_PALMTREE_TOP",

	"S_DBALL1",
	"S_DBALL2",
	"S_DBALL3",
	"S_DBALL4",
	"S_DBALL5",
	"S_DBALL6",
	"S_EGGSTATUE2",

	// Shield Orb
	"S_ARMA1",
	"S_ARMA2",
	"S_ARMA3",
	"S_ARMA4",
	"S_ARMA5",
	"S_ARMA6",
	"S_ARMA7",
	"S_ARMA8",
	"S_ARMA9",
	"S_ARMA10",
	"S_ARMA11",
	"S_ARMA12",
	"S_ARMA13",
	"S_ARMA14",
	"S_ARMA15",
	"S_ARMA16",

	"S_ARMF1",
	"S_ARMF2",
	"S_ARMF3",
	"S_ARMF4",
	"S_ARMF5",
	"S_ARMF6",
	"S_ARMF7",
	"S_ARMF8",
	"S_ARMF9",
	"S_ARMF10",
	"S_ARMF11",
	"S_ARMF12",
	"S_ARMF13",
	"S_ARMF14",
	"S_ARMF15",
	"S_ARMF16",
	"S_ARMF17",
	"S_ARMF18",
	"S_ARMF19",
	"S_ARMF20",
	"S_ARMF21",
	"S_ARMF22",
	"S_ARMF23",
	"S_ARMF24",
	"S_ARMF25",
	"S_ARMF26",
	"S_ARMF27",
	"S_ARMF28",
	"S_ARMF29",
	"S_ARMF30",
	"S_ARMF31",
	"S_ARMF32",

	"S_ARMB1",
	"S_ARMB2",
	"S_ARMB3",
	"S_ARMB4",
	"S_ARMB5",
	"S_ARMB6",
	"S_ARMB7",
	"S_ARMB8",
	"S_ARMB9",
	"S_ARMB10",
	"S_ARMB11",
	"S_ARMB12",
	"S_ARMB13",
	"S_ARMB14",
	"S_ARMB15",
	"S_ARMB16",
	"S_ARMB17",
	"S_ARMB18",
	"S_ARMB19",
	"S_ARMB20",
	"S_ARMB21",
	"S_ARMB22",
	"S_ARMB23",
	"S_ARMB24",
	"S_ARMB25",
	"S_ARMB26",
	"S_ARMB27",
	"S_ARMB28",
	"S_ARMB29",
	"S_ARMB30",
	"S_ARMB31",
	"S_ARMB32",

	"S_WIND1",
	"S_WIND2",
	"S_WIND3",
	"S_WIND4",
	"S_WIND5",
	"S_WIND6",
	"S_WIND7",
	"S_WIND8",

	"S_MAGN1",
	"S_MAGN2",
	"S_MAGN3",
	"S_MAGN4",
	"S_MAGN5",
	"S_MAGN6",
	"S_MAGN7",
	"S_MAGN8",
	"S_MAGN9",
	"S_MAGN10",
	"S_MAGN11",
	"S_MAGN12",
	"S_MAGN13",

	"S_FORC1",
	"S_FORC2",
	"S_FORC3",
	"S_FORC4",
	"S_FORC5",
	"S_FORC6",
	"S_FORC7",
	"S_FORC8",
	"S_FORC9",
	"S_FORC10",

	"S_FORC11",
	"S_FORC12",
	"S_FORC13",
	"S_FORC14",
	"S_FORC15",
	"S_FORC16",
	"S_FORC17",
	"S_FORC18",
	"S_FORC19",
	"S_FORC20",

	"S_FORC21",

	"S_ELEM1",
	"S_ELEM2",
	"S_ELEM3",
	"S_ELEM4",
	"S_ELEM5",
	"S_ELEM6",
	"S_ELEM7",
	"S_ELEM8",
	"S_ELEM9",
	"S_ELEM10",
	"S_ELEM11",
	"S_ELEM12",

	"S_ELEM13",
	"S_ELEM14",

	"S_ELEMF1",
	"S_ELEMF2",
	"S_ELEMF3",
	"S_ELEMF4",
	"S_ELEMF5",
	"S_ELEMF6",
	"S_ELEMF7",
	"S_ELEMF8",
	"S_ELEMF9",
	"S_ELEMF10",

	"S_PITY1",
	"S_PITY2",
	"S_PITY3",
	"S_PITY4",
	"S_PITY5",
	"S_PITY6",
	"S_PITY7",
	"S_PITY8",
	"S_PITY9",
	"S_PITY10",
	"S_PITY11",
	"S_PITY12",

	"S_FIRS1",
	"S_FIRS2",
	"S_FIRS3",
	"S_FIRS4",
	"S_FIRS5",
	"S_FIRS6",
	"S_FIRS7",
	"S_FIRS8",
	"S_FIRS9",

	"S_FIRS10",
	"S_FIRS11",

	"S_FIRSB1",
	"S_FIRSB2",
	"S_FIRSB3",
	"S_FIRSB4",
	"S_FIRSB5",
	"S_FIRSB6",
	"S_FIRSB7",
	"S_FIRSB8",
	"S_FIRSB9",

	"S_FIRSB10",

	"S_BUBS1",
	"S_BUBS2",
	"S_BUBS3",
	"S_BUBS4",
	"S_BUBS5",
	"S_BUBS6",
	"S_BUBS7",
	"S_BUBS8",
	"S_BUBS9",

	"S_BUBS10",
	"S_BUBS11",

	"S_BUBSB1",
	"S_BUBSB2",
	"S_BUBSB3",
	"S_BUBSB4",

	"S_BUBSB5",
	"S_BUBSB6",

	"S_ZAPS1",
	"S_ZAPS2",
	"S_ZAPS3",
	"S_ZAPS4",
	"S_ZAPS5",
	"S_ZAPS6",
	"S_ZAPS7",
	"S_ZAPS8",
	"S_ZAPS9",
	"S_ZAPS10",
	"S_ZAPS11",
	"S_ZAPS12",
	"S_ZAPS13", // blank frame
	"S_ZAPS14",
	"S_ZAPS15",
	"S_ZAPS16",

	"S_ZAPSB1", // blank frame
	"S_ZAPSB2",
	"S_ZAPSB3",
	"S_ZAPSB4",
	"S_ZAPSB5",
	"S_ZAPSB6",
	"S_ZAPSB7",
	"S_ZAPSB8",
	"S_ZAPSB9",
	"S_ZAPSB10",
	"S_ZAPSB11", // blank frame

	//Thunder spark
	"S_THUNDERCOIN_SPARK",

	// Invincibility Sparkles
	"S_IVSP",

	// Super Sonic Spark
	"S_SSPK1",
	"S_SSPK2",
	"S_SSPK3",
	"S_SSPK4",
	"S_SSPK5",

	// Flicky-sized bubble
	"S_FLICKY_BUBBLE",

	// Bluebird
	"S_FLICKY_01_OUT",
	"S_FLICKY_01_FLAP1",
	"S_FLICKY_01_FLAP2",
	"S_FLICKY_01_FLAP3",
	"S_FLICKY_01_STAND",
	"S_FLICKY_01_CENTER",

	// Rabbit
	"S_FLICKY_02_OUT",
	"S_FLICKY_02_AIM",
	"S_FLICKY_02_HOP",
	"S_FLICKY_02_UP",
	"S_FLICKY_02_DOWN",
	"S_FLICKY_02_STAND",
	"S_FLICKY_02_CENTER",

	// Chicken
	"S_FLICKY_03_OUT",
	"S_FLICKY_03_AIM",
	"S_FLICKY_03_HOP",
	"S_FLICKY_03_UP",
	"S_FLICKY_03_FLAP1",
	"S_FLICKY_03_FLAP2",
	"S_FLICKY_03_STAND",
	"S_FLICKY_03_CENTER",

	// Seal
	"S_FLICKY_04_OUT",
	"S_FLICKY_04_AIM",
	"S_FLICKY_04_HOP",
	"S_FLICKY_04_UP",
	"S_FLICKY_04_DOWN",
	"S_FLICKY_04_SWIM1",
	"S_FLICKY_04_SWIM2",
	"S_FLICKY_04_SWIM3",
	"S_FLICKY_04_SWIM4",
	"S_FLICKY_04_STAND",
	"S_FLICKY_04_CENTER",

	// Pig
	"S_FLICKY_05_OUT",
	"S_FLICKY_05_AIM",
	"S_FLICKY_05_HOP",
	"S_FLICKY_05_UP",
	"S_FLICKY_05_DOWN",
	"S_FLICKY_05_STAND",
	"S_FLICKY_05_CENTER",

	// Chipmunk
	"S_FLICKY_06_OUT",
	"S_FLICKY_06_AIM",
	"S_FLICKY_06_HOP",
	"S_FLICKY_06_UP",
	"S_FLICKY_06_DOWN",
	"S_FLICKY_06_STAND",
	"S_FLICKY_06_CENTER",

	// Penguin
	"S_FLICKY_07_OUT",
	"S_FLICKY_07_AIML",
	"S_FLICKY_07_HOPL",
	"S_FLICKY_07_UPL",
	"S_FLICKY_07_DOWNL",
	"S_FLICKY_07_AIMR",
	"S_FLICKY_07_HOPR",
	"S_FLICKY_07_UPR",
	"S_FLICKY_07_DOWNR",
	"S_FLICKY_07_SWIM1",
	"S_FLICKY_07_SWIM2",
	"S_FLICKY_07_SWIM3",
	"S_FLICKY_07_STAND",
	"S_FLICKY_07_CENTER",

	// Fish
	"S_FLICKY_08_OUT",
	"S_FLICKY_08_AIM",
	"S_FLICKY_08_HOP",
	"S_FLICKY_08_FLAP1",
	"S_FLICKY_08_FLAP2",
	"S_FLICKY_08_FLAP3",
	"S_FLICKY_08_FLAP4",
	"S_FLICKY_08_SWIM1",
	"S_FLICKY_08_SWIM2",
	"S_FLICKY_08_SWIM3",
	"S_FLICKY_08_SWIM4",
	"S_FLICKY_08_STAND",
	"S_FLICKY_08_CENTER",

	// Ram
	"S_FLICKY_09_OUT",
	"S_FLICKY_09_AIM",
	"S_FLICKY_09_HOP",
	"S_FLICKY_09_UP",
	"S_FLICKY_09_DOWN",
	"S_FLICKY_09_STAND",
	"S_FLICKY_09_CENTER",

	// Puffin
	"S_FLICKY_10_OUT",
	"S_FLICKY_10_FLAP1",
	"S_FLICKY_10_FLAP2",
	"S_FLICKY_10_STAND",
	"S_FLICKY_10_CENTER",

	// Cow
	"S_FLICKY_11_OUT",
	"S_FLICKY_11_AIM",
	"S_FLICKY_11_RUN1",
	"S_FLICKY_11_RUN2",
	"S_FLICKY_11_RUN3",
	"S_FLICKY_11_STAND",
	"S_FLICKY_11_CENTER",

	// Rat
	"S_FLICKY_12_OUT",
	"S_FLICKY_12_AIM",
	"S_FLICKY_12_RUN1",
	"S_FLICKY_12_RUN2",
	"S_FLICKY_12_RUN3",
	"S_FLICKY_12_STAND",
	"S_FLICKY_12_CENTER",

	// Bear
	"S_FLICKY_13_OUT",
	"S_FLICKY_13_AIM",
	"S_FLICKY_13_HOP",
	"S_FLICKY_13_UP",
	"S_FLICKY_13_DOWN",
	"S_FLICKY_13_STAND",
	"S_FLICKY_13_CENTER",

	// Dove
	"S_FLICKY_14_OUT",
	"S_FLICKY_14_FLAP1",
	"S_FLICKY_14_FLAP2",
	"S_FLICKY_14_FLAP3",
	"S_FLICKY_14_STAND",
	"S_FLICKY_14_CENTER",

	// Cat
	"S_FLICKY_15_OUT",
	"S_FLICKY_15_AIM",
	"S_FLICKY_15_HOP",
	"S_FLICKY_15_UP",
	"S_FLICKY_15_DOWN",
	"S_FLICKY_15_STAND",
	"S_FLICKY_15_CENTER",

	// Canary
	"S_FLICKY_16_OUT",
	"S_FLICKY_16_FLAP1",
	"S_FLICKY_16_FLAP2",
	"S_FLICKY_16_FLAP3",
	"S_FLICKY_16_STAND",
	"S_FLICKY_16_CENTER",

	// Spider
	"S_SECRETFLICKY_01_OUT",
	"S_SECRETFLICKY_01_AIM",
	"S_SECRETFLICKY_01_HOP",
	"S_SECRETFLICKY_01_UP",
	"S_SECRETFLICKY_01_DOWN",
	"S_SECRETFLICKY_01_STAND",
	"S_SECRETFLICKY_01_CENTER",

	// Bat
	"S_SECRETFLICKY_02_OUT",
	"S_SECRETFLICKY_02_FLAP1",
	"S_SECRETFLICKY_02_FLAP2",
	"S_SECRETFLICKY_02_FLAP3",
	"S_SECRETFLICKY_02_STAND",
	"S_SECRETFLICKY_02_CENTER",

	// Fan
	"S_FAN",
	"S_FAN2",
	"S_FAN3",
	"S_FAN4",
	"S_FAN5",

	// Steam Riser
	"S_STEAM1",
	"S_STEAM2",
	"S_STEAM3",
	"S_STEAM4",
	"S_STEAM5",
	"S_STEAM6",
	"S_STEAM7",
	"S_STEAM8",

	// Bumpers
	"S_BUMPER",
	"S_BUMPERHIT",

	// Balloons
	"S_BALLOON",
	"S_BALLOONPOP1",
	"S_BALLOONPOP2",
	"S_BALLOONPOP3",
	"S_BALLOONPOP4",
	"S_BALLOONPOP5",
	"S_BALLOONPOP6",

	// Yellow Spring
	"S_YELLOWSPRING1",
	"S_YELLOWSPRING2",
	"S_YELLOWSPRING3",
	"S_YELLOWSPRING4",

	// Red Spring
	"S_REDSPRING1",
	"S_REDSPRING2",
	"S_REDSPRING3",
	"S_REDSPRING4",

	// Blue Spring
	"S_BLUESPRING1",
	"S_BLUESPRING2",
	"S_BLUESPRING3",
	"S_BLUESPRING4",

	// Grey Spring
	"S_GREYSPRING1",
	"S_GREYSPRING2",
	"S_GREYSPRING3",
	"S_GREYSPRING4",

	// Orange Spring (Pogo)
	"S_POGOSPRING1",
	"S_POGOSPRING2",
	"S_POGOSPRING2B",
	"S_POGOSPRING3",
	"S_POGOSPRING4",

	// Yellow Diagonal Spring
	"S_YDIAG1",
	"S_YDIAG2",
	"S_YDIAG3",
	"S_YDIAG4",

	// Red Diagonal Spring
	"S_RDIAG1",
	"S_RDIAG2",
	"S_RDIAG3",
	"S_RDIAG4",

	// Blue Diagonal Spring
	"S_BDIAG1",
	"S_BDIAG2",
	"S_BDIAG3",
	"S_BDIAG4",

	// Grey Diagonal Spring
	"S_GDIAG1",
	"S_GDIAG2",
	"S_GDIAG3",
	"S_GDIAG4",

	// Yellow Horizontal Spring
	"S_YHORIZ1",
	"S_YHORIZ2",
	"S_YHORIZ3",
	"S_YHORIZ4",

	// Red Horizontal Spring
	"S_RHORIZ1",
	"S_RHORIZ2",
	"S_RHORIZ3",
	"S_RHORIZ4",

	// Blue Horizontal Spring
	"S_BHORIZ1",
	"S_BHORIZ2",
	"S_BHORIZ3",
	"S_BHORIZ4",

	// Grey Horizontal Spring
	"S_GHORIZ1",
	"S_GHORIZ2",
	"S_GHORIZ3",
	"S_GHORIZ4",

	// Booster
	"S_BOOSTERSOUND",
	"S_YELLOWBOOSTERROLLER",
	"S_YELLOWBOOSTERSEG_LEFT",
	"S_YELLOWBOOSTERSEG_RIGHT",
	"S_YELLOWBOOSTERSEG_FACE",
	"S_REDBOOSTERROLLER",
	"S_REDBOOSTERSEG_LEFT",
	"S_REDBOOSTERSEG_RIGHT",
	"S_REDBOOSTERSEG_FACE",

	// Rain
	"S_RAIN1",
	"S_RAINRETURN",

	// Snowflake
	"S_SNOW1",
	"S_SNOW2",
	"S_SNOW3",

	// Blizzard Snowball
	"S_BLIZZARDSNOW1",
	"S_BLIZZARDSNOW2",
	"S_BLIZZARDSNOW3",

	// Water Splish
	"S_SPLISH1",
	"S_SPLISH2",
	"S_SPLISH3",
	"S_SPLISH4",
	"S_SPLISH5",
	"S_SPLISH6",
	"S_SPLISH7",
	"S_SPLISH8",
	"S_SPLISH9",

	// Lava Splish
	"S_LAVASPLISH",

	// added water splash
	"S_SPLASH1",
	"S_SPLASH2",
	"S_SPLASH3",

	// lava/slime damage burn smoke
	"S_SMOKE1",
	"S_SMOKE2",
	"S_SMOKE3",
	"S_SMOKE4",
	"S_SMOKE5",

	// Bubbles
	"S_SMALLBUBBLE",
	"S_MEDIUMBUBBLE",
	"S_LARGEBUBBLE1",
	"S_LARGEBUBBLE2",
	"S_EXTRALARGEBUBBLE", // breathable

	"S_POP1", // Extra Large bubble goes POP!

	"S_WATERZAP",

	// Spindash dust
	"S_SPINDUST1",
	"S_SPINDUST2",
	"S_SPINDUST3",
	"S_SPINDUST4",
	"S_SPINDUST_BUBBLE1",
	"S_SPINDUST_BUBBLE2",
	"S_SPINDUST_BUBBLE3",
	"S_SPINDUST_BUBBLE4",
	"S_SPINDUST_FIRE1",
	"S_SPINDUST_FIRE2",
	"S_SPINDUST_FIRE3",
	"S_SPINDUST_FIRE4",

	"S_FOG1",
	"S_FOG2",
	"S_FOG3",
	"S_FOG4",
	"S_FOG5",
	"S_FOG6",
	"S_FOG7",
	"S_FOG8",
	"S_FOG9",
	"S_FOG10",
	"S_FOG11",
	"S_FOG12",
	"S_FOG13",
	"S_FOG14",

	"S_SEED",

	"S_PARTICLE",

	// Score Logos
	"S_SCRA", // 100
	"S_SCRB", // 200
	"S_SCRC", // 500
	"S_SCRD", // 1000
	"S_SCRE", // 10000
	"S_SCRF", // 400 (mario)
	"S_SCRG", // 800 (mario)
	"S_SCRH", // 2000 (mario)
	"S_SCRI", // 4000 (mario)
	"S_SCRJ", // 8000 (mario)
	"S_SCRK", // 1UP (mario)
	"S_SCRL", // 10

	// Drowning Timer Numbers
	"S_ZERO1",
	"S_ONE1",
	"S_TWO1",
	"S_THREE1",
	"S_FOUR1",
	"S_FIVE1",

	"S_ZERO2",
	"S_ONE2",
	"S_TWO2",
	"S_THREE2",
	"S_FOUR2",
	"S_FIVE2",

	"S_FLIGHTINDICATOR",

	"S_LOCKON1",
	"S_LOCKON2",
	"S_LOCKON3",
	"S_LOCKON4",
	"S_LOCKONINF1",
	"S_LOCKONINF2",
	"S_LOCKONINF3",
	"S_LOCKONINF4",

	// Tag Sign
	"S_TTAG",

	// Got Flag Sign
	"S_GOTFLAG",

	// Finish flag
	"S_FINISHFLAG",

	"S_CORK",
	"S_LHRT",

	// Red Ring
	"S_RRNG1",
	"S_RRNG2",
	"S_RRNG3",
	"S_RRNG4",
	"S_RRNG5",
	"S_RRNG6",
	"S_RRNG7",

	// Weapon Ring Ammo
	"S_BOUNCERINGAMMO",
	"S_RAILRINGAMMO",
	"S_INFINITYRINGAMMO",
	"S_AUTOMATICRINGAMMO",
	"S_EXPLOSIONRINGAMMO",
	"S_SCATTERRINGAMMO",
	"S_GRENADERINGAMMO",

	// Weapon pickup
	"S_BOUNCEPICKUP",
	"S_BOUNCEPICKUPFADE1",
	"S_BOUNCEPICKUPFADE2",
	"S_BOUNCEPICKUPFADE3",
	"S_BOUNCEPICKUPFADE4",
	"S_BOUNCEPICKUPFADE5",
	"S_BOUNCEPICKUPFADE6",
	"S_BOUNCEPICKUPFADE7",
	"S_BOUNCEPICKUPFADE8",

	"S_RAILPICKUP",
	"S_RAILPICKUPFADE1",
	"S_RAILPICKUPFADE2",
	"S_RAILPICKUPFADE3",
	"S_RAILPICKUPFADE4",
	"S_RAILPICKUPFADE5",
	"S_RAILPICKUPFADE6",
	"S_RAILPICKUPFADE7",
	"S_RAILPICKUPFADE8",

	"S_AUTOPICKUP",
	"S_AUTOPICKUPFADE1",
	"S_AUTOPICKUPFADE2",
	"S_AUTOPICKUPFADE3",
	"S_AUTOPICKUPFADE4",
	"S_AUTOPICKUPFADE5",
	"S_AUTOPICKUPFADE6",
	"S_AUTOPICKUPFADE7",
	"S_AUTOPICKUPFADE8",

	"S_EXPLODEPICKUP",
	"S_EXPLODEPICKUPFADE1",
	"S_EXPLODEPICKUPFADE2",
	"S_EXPLODEPICKUPFADE3",
	"S_EXPLODEPICKUPFADE4",
	"S_EXPLODEPICKUPFADE5",
	"S_EXPLODEPICKUPFADE6",
	"S_EXPLODEPICKUPFADE7",
	"S_EXPLODEPICKUPFADE8",

	"S_SCATTERPICKUP",
	"S_SCATTERPICKUPFADE1",
	"S_SCATTERPICKUPFADE2",
	"S_SCATTERPICKUPFADE3",
	"S_SCATTERPICKUPFADE4",
	"S_SCATTERPICKUPFADE5",
	"S_SCATTERPICKUPFADE6",
	"S_SCATTERPICKUPFADE7",
	"S_SCATTERPICKUPFADE8",

	"S_GRENADEPICKUP",
	"S_GRENADEPICKUPFADE1",
	"S_GRENADEPICKUPFADE2",
	"S_GRENADEPICKUPFADE3",
	"S_GRENADEPICKUPFADE4",
	"S_GRENADEPICKUPFADE5",
	"S_GRENADEPICKUPFADE6",
	"S_GRENADEPICKUPFADE7",
	"S_GRENADEPICKUPFADE8",

	// Thrown Weapon Rings
	"S_THROWNBOUNCE1",
	"S_THROWNBOUNCE2",
	"S_THROWNBOUNCE3",
	"S_THROWNBOUNCE4",
	"S_THROWNBOUNCE5",
	"S_THROWNBOUNCE6",
	"S_THROWNBOUNCE7",
	"S_THROWNINFINITY1",
	"S_THROWNINFINITY2",
	"S_THROWNINFINITY3",
	"S_THROWNINFINITY4",
	"S_THROWNINFINITY5",
	"S_THROWNINFINITY6",
	"S_THROWNINFINITY7",
	"S_THROWNAUTOMATIC1",
	"S_THROWNAUTOMATIC2",
	"S_THROWNAUTOMATIC3",
	"S_THROWNAUTOMATIC4",
	"S_THROWNAUTOMATIC5",
	"S_THROWNAUTOMATIC6",
	"S_THROWNAUTOMATIC7",
	"S_THROWNEXPLOSION1",
	"S_THROWNEXPLOSION2",
	"S_THROWNEXPLOSION3",
	"S_THROWNEXPLOSION4",
	"S_THROWNEXPLOSION5",
	"S_THROWNEXPLOSION6",
	"S_THROWNEXPLOSION7",
	"S_THROWNGRENADE1",
	"S_THROWNGRENADE2",
	"S_THROWNGRENADE3",
	"S_THROWNGRENADE4",
	"S_THROWNGRENADE5",
	"S_THROWNGRENADE6",
	"S_THROWNGRENADE7",
	"S_THROWNGRENADE8",
	"S_THROWNGRENADE9",
	"S_THROWNGRENADE10",
	"S_THROWNGRENADE11",
	"S_THROWNGRENADE12",
	"S_THROWNGRENADE13",
	"S_THROWNGRENADE14",
	"S_THROWNGRENADE15",
	"S_THROWNGRENADE16",
	"S_THROWNGRENADE17",
	"S_THROWNGRENADE18",
	"S_THROWNSCATTER",

	"S_RINGEXPLODE",

	"S_COIN1",
	"S_COIN2",
	"S_COIN3",
	"S_COINSPARKLE1",
	"S_COINSPARKLE2",
	"S_COINSPARKLE3",
	"S_COINSPARKLE4",
	"S_GOOMBA1",
	"S_GOOMBA1B",
	"S_GOOMBA2",
	"S_GOOMBA3",
	"S_GOOMBA4",
	"S_GOOMBA5",
	"S_GOOMBA6",
	"S_GOOMBA7",
	"S_GOOMBA8",
	"S_GOOMBA9",
	"S_GOOMBA_DEAD",
	"S_BLUEGOOMBA1",
	"S_BLUEGOOMBA1B",
	"S_BLUEGOOMBA2",
	"S_BLUEGOOMBA3",
	"S_BLUEGOOMBA4",
	"S_BLUEGOOMBA5",
	"S_BLUEGOOMBA6",
	"S_BLUEGOOMBA7",
	"S_BLUEGOOMBA8",
	"S_BLUEGOOMBA9",
	"S_BLUEGOOMBA_DEAD",

	// Mario-specific stuff
	"S_FIREFLOWER1",
	"S_FIREFLOWER2",
	"S_FIREFLOWER3",
	"S_FIREFLOWER4",
	"S_FIREBALL",
	"S_FIREBALLTRAIL1",
	"S_FIREBALLTRAIL2",
	"S_SHELL",
	"S_PUMA_START1",
	"S_PUMA_START2",
	"S_PUMA_UP1",
	"S_PUMA_UP2",
	"S_PUMA_UP3",
	"S_PUMA_DOWN1",
	"S_PUMA_DOWN2",
	"S_PUMA_DOWN3",
	"S_PUMATRAIL1",
	"S_PUMATRAIL2",
	"S_PUMATRAIL3",
	"S_PUMATRAIL4",
	"S_HAMMER",
	"S_KOOPA1",
	"S_KOOPA2",
	"S_KOOPAFLAME1",
	"S_KOOPAFLAME2",
	"S_KOOPAFLAME3",
	"S_AXE1",
	"S_AXE2",
	"S_AXE3",
	"S_MARIOBUSH1",
	"S_MARIOBUSH2",
	"S_TOAD",

	// Nights-specific stuff
	"S_NIGHTSDRONE_MAN1",
	"S_NIGHTSDRONE_MAN2",
	"S_NIGHTSDRONE_SPARKLING1",
	"S_NIGHTSDRONE_SPARKLING2",
	"S_NIGHTSDRONE_SPARKLING3",
	"S_NIGHTSDRONE_SPARKLING4",
	"S_NIGHTSDRONE_SPARKLING5",
	"S_NIGHTSDRONE_SPARKLING6",
	"S_NIGHTSDRONE_SPARKLING7",
	"S_NIGHTSDRONE_SPARKLING8",
	"S_NIGHTSDRONE_SPARKLING9",
	"S_NIGHTSDRONE_SPARKLING10",
	"S_NIGHTSDRONE_SPARKLING11",
	"S_NIGHTSDRONE_SPARKLING12",
	"S_NIGHTSDRONE_SPARKLING13",
	"S_NIGHTSDRONE_SPARKLING14",
	"S_NIGHTSDRONE_SPARKLING15",
	"S_NIGHTSDRONE_SPARKLING16",
	"S_NIGHTSDRONE_GOAL1",
	"S_NIGHTSDRONE_GOAL2",
	"S_NIGHTSDRONE_GOAL3",
	"S_NIGHTSDRONE_GOAL4",

	"S_NIGHTSPARKLE1",
	"S_NIGHTSPARKLE2",
	"S_NIGHTSPARKLE3",
	"S_NIGHTSPARKLE4",
	"S_NIGHTSPARKLESUPER1",
	"S_NIGHTSPARKLESUPER2",
	"S_NIGHTSPARKLESUPER3",
	"S_NIGHTSPARKLESUPER4",
	"S_NIGHTSLOOPHELPER",

	// NiGHTS bumper
	"S_NIGHTSBUMPER1",
	"S_NIGHTSBUMPER2",
	"S_NIGHTSBUMPER3",
	"S_NIGHTSBUMPER4",
	"S_NIGHTSBUMPER5",
	"S_NIGHTSBUMPER6",
	"S_NIGHTSBUMPER7",
	"S_NIGHTSBUMPER8",
	"S_NIGHTSBUMPER9",
	"S_NIGHTSBUMPER10",
	"S_NIGHTSBUMPER11",
	"S_NIGHTSBUMPER12",

	"S_HOOP",
	"S_HOOP_XMASA",
	"S_HOOP_XMASB",

	"S_NIGHTSCORE10",
	"S_NIGHTSCORE20",
	"S_NIGHTSCORE30",
	"S_NIGHTSCORE40",
	"S_NIGHTSCORE50",
	"S_NIGHTSCORE60",
	"S_NIGHTSCORE70",
	"S_NIGHTSCORE80",
	"S_NIGHTSCORE90",
	"S_NIGHTSCORE100",
	"S_NIGHTSCORE10_2",
	"S_NIGHTSCORE20_2",
	"S_NIGHTSCORE30_2",
	"S_NIGHTSCORE40_2",
	"S_NIGHTSCORE50_2",
	"S_NIGHTSCORE60_2",
	"S_NIGHTSCORE70_2",
	"S_NIGHTSCORE80_2",
	"S_NIGHTSCORE90_2",
	"S_NIGHTSCORE100_2",

	// NiGHTS Paraloop Powerups
	"S_NIGHTSSUPERLOOP",
	"S_NIGHTSDRILLREFILL",
	"S_NIGHTSHELPER",
	"S_NIGHTSEXTRATIME",
	"S_NIGHTSLINKFREEZE",
	"S_EGGCAPSULE",

	// Orbiting Chaos Emeralds
	"S_ORBITEM1",
	"S_ORBITEM2",
	"S_ORBITEM3",
	"S_ORBITEM4",
	"S_ORBITEM5",
	"S_ORBITEM6",
	"S_ORBITEM7",
	"S_ORBITEM8",
	"S_ORBIDYA1",
	"S_ORBIDYA2",
	"S_ORBIDYA3",
	"S_ORBIDYA4",
	"S_ORBIDYA5",

	// "Flicky" helper
	"S_NIGHTOPIANHELPER1",
	"S_NIGHTOPIANHELPER2",
	"S_NIGHTOPIANHELPER3",
	"S_NIGHTOPIANHELPER4",
	"S_NIGHTOPIANHELPER5",
	"S_NIGHTOPIANHELPER6",
	"S_NIGHTOPIANHELPER7",
	"S_NIGHTOPIANHELPER8",
	"S_NIGHTOPIANHELPER9",

	// Nightopian
	"S_PIAN0",
	"S_PIAN1",
	"S_PIAN2",
	"S_PIAN3",
	"S_PIAN4",
	"S_PIAN5",
	"S_PIAN6",
	"S_PIANSING",

	// Shleep
	"S_SHLEEP1",
	"S_SHLEEP2",
	"S_SHLEEP3",
	"S_SHLEEP4",
	"S_SHLEEPBOUNCE1",
	"S_SHLEEPBOUNCE2",
	"S_SHLEEPBOUNCE3",

	// Secret badniks and hazards, shhhh
	"S_PENGUINATOR_LOOK",
	"S_PENGUINATOR_WADDLE1",
	"S_PENGUINATOR_WADDLE2",
	"S_PENGUINATOR_WADDLE3",
	"S_PENGUINATOR_WADDLE4",
	"S_PENGUINATOR_SLIDE1",
	"S_PENGUINATOR_SLIDE2",
	"S_PENGUINATOR_SLIDE3",
	"S_PENGUINATOR_SLIDE4",
	"S_PENGUINATOR_SLIDE5",

	"S_POPHAT_LOOK",
	"S_POPHAT_SHOOT1",
	"S_POPHAT_SHOOT2",
	"S_POPHAT_SHOOT3",
	"S_POPHAT_SHOOT4",
	"S_POPSHOT",
	"S_POPSHOT_TRAIL",

	"S_HIVEELEMENTAL_LOOK",
	"S_HIVEELEMENTAL_PREPARE1",
	"S_HIVEELEMENTAL_PREPARE2",
	"S_HIVEELEMENTAL_SHOOT1",
	"S_HIVEELEMENTAL_SHOOT2",
	"S_HIVEELEMENTAL_DORMANT",
	"S_HIVEELEMENTAL_PAIN",
	"S_HIVEELEMENTAL_DIE1",
	"S_HIVEELEMENTAL_DIE2",
	"S_HIVEELEMENTAL_DIE3",

	"S_BUMBLEBORE_SPAWN",
	"S_BUMBLEBORE_LOOK1",
	"S_BUMBLEBORE_LOOK2",
	"S_BUMBLEBORE_FLY1",
	"S_BUMBLEBORE_FLY2",
	"S_BUMBLEBORE_RAISE",
	"S_BUMBLEBORE_FALL1",
	"S_BUMBLEBORE_FALL2",
	"S_BUMBLEBORE_STUCK1",
	"S_BUMBLEBORE_STUCK2",
	"S_BUMBLEBORE_DIE",

	"S_BUGGLEIDLE",
	"S_BUGGLEFLY",

	"S_SMASHSPIKE_FLOAT",
	"S_SMASHSPIKE_EASE1",
	"S_SMASHSPIKE_EASE2",
	"S_SMASHSPIKE_FALL",
	"S_SMASHSPIKE_STOMP1",
	"S_SMASHSPIKE_STOMP2",
	"S_SMASHSPIKE_RISE1",
	"S_SMASHSPIKE_RISE2",

	"S_CACO_LOOK",
	"S_CACO_WAKE1",
	"S_CACO_WAKE2",
	"S_CACO_WAKE3",
	"S_CACO_WAKE4",
	"S_CACO_ROAR",
	"S_CACO_CHASE",
	"S_CACO_CHASE_REPEAT",
	"S_CACO_RANDOM",
	"S_CACO_PREPARE_SOUND",
	"S_CACO_PREPARE1",
	"S_CACO_PREPARE2",
	"S_CACO_PREPARE3",
	"S_CACO_SHOOT_SOUND",
	"S_CACO_SHOOT1",
	"S_CACO_SHOOT2",
	"S_CACO_CLOSE",
	"S_CACO_DIE_FLAGS",
	"S_CACO_DIE_GIB1",
	"S_CACO_DIE_GIB2",
	"S_CACO_DIE_SCREAM",
	"S_CACO_DIE_SHATTER",
	"S_CACO_DIE_FALL",
	"S_CACOSHARD_RANDOMIZE",
	"S_CACOSHARD1_1",
	"S_CACOSHARD1_2",
	"S_CACOSHARD2_1",
	"S_CACOSHARD2_2",
	"S_CACOFIRE1",
	"S_CACOFIRE2",
	"S_CACOFIRE3",
	"S_CACOFIRE_EXPLODE1",
	"S_CACOFIRE_EXPLODE2",
	"S_CACOFIRE_EXPLODE3",
	"S_CACOFIRE_EXPLODE4",

	"S_SPINBOBERT_MOVE_FLIPUP",
	"S_SPINBOBERT_MOVE_UP",
	"S_SPINBOBERT_MOVE_FLIPDOWN",
	"S_SPINBOBERT_MOVE_DOWN",
	"S_SPINBOBERT_FIRE_MOVE",
	"S_SPINBOBERT_FIRE_GHOST",
	"S_SPINBOBERT_FIRE_TRAIL1",
	"S_SPINBOBERT_FIRE_TRAIL2",
	"S_SPINBOBERT_FIRE_TRAIL3",

	"S_HANGSTER_LOOK",
	"S_HANGSTER_SWOOP1",
	"S_HANGSTER_SWOOP2",
	"S_HANGSTER_ARC1",
	"S_HANGSTER_ARC2",
	"S_HANGSTER_ARC3",
	"S_HANGSTER_FLY1",
	"S_HANGSTER_FLY2",
	"S_HANGSTER_FLY3",
	"S_HANGSTER_FLY4",
	"S_HANGSTER_FLYREPEAT",
	"S_HANGSTER_ARCUP1",
	"S_HANGSTER_ARCUP2",
	"S_HANGSTER_ARCUP3",
	"S_HANGSTER_RETURN1",
	"S_HANGSTER_RETURN2",
	"S_HANGSTER_RETURN3",

	"S_CRUMBLE1",
	"S_CRUMBLE2",

	// Spark
	"S_SPRK1",
	"S_SPRK2",
	"S_SPRK3",

	// Robot Explosion
	"S_XPLD_FLICKY",
	"S_XPLD1",
	"S_XPLD2",
	"S_XPLD3",
	"S_XPLD4",
	"S_XPLD5",
	"S_XPLD6",
	"S_XPLD_EGGTRAP",

	// Underwater Explosion
	"S_WPLD1",
	"S_WPLD2",
	"S_WPLD3",
	"S_WPLD4",
	"S_WPLD5",
	"S_WPLD6",

	"S_DUST1",
	"S_DUST2",
	"S_DUST3",
	"S_DUST4",

	"S_ROCKSPAWN",

	"S_ROCKCRUMBLEA",
	"S_ROCKCRUMBLEB",
	"S_ROCKCRUMBLEC",
	"S_ROCKCRUMBLED",
	"S_ROCKCRUMBLEE",
	"S_ROCKCRUMBLEF",
	"S_ROCKCRUMBLEG",
	"S_ROCKCRUMBLEH",
	"S_ROCKCRUMBLEI",
	"S_ROCKCRUMBLEJ",
	"S_ROCKCRUMBLEK",
	"S_ROCKCRUMBLEL",
	"S_ROCKCRUMBLEM",
	"S_ROCKCRUMBLEN",
	"S_ROCKCRUMBLEO",
	"S_ROCKCRUMBLEP",

	// Level debris
	"S_GFZDEBRIS",
	"S_BRICKDEBRIS",
	"S_WOODDEBRIS",
	"S_REDBRICKDEBRIS",
	"S_BLUEBRICKDEBRIS",
	"S_YELLOWBRICKDEBRIS",

	//{ Random Item Box
	"S_RANDOMITEM1",
	"S_RANDOMITEM2",
	"S_RANDOMITEM3",
	"S_RANDOMITEM4",
	"S_RANDOMITEM5",
	"S_RANDOMITEM6",
	"S_RANDOMITEM7",
	"S_RANDOMITEM8",
	"S_RANDOMITEM9",
	"S_RANDOMITEM10",
	"S_RANDOMITEM11",
	"S_RANDOMITEM12",
	"S_DEADRANDOMITEM",

	// Sphere Box (for Battle)
	"S_SPHEREBOX1",
	"S_SPHEREBOX2",
	"S_SPHEREBOX3",
	"S_SPHEREBOX4",
	"S_SPHEREBOX5",
	"S_SPHEREBOX6",
	"S_SPHEREBOX7",
	"S_SPHEREBOX8",
	"S_SPHEREBOX9",
	"S_SPHEREBOX10",
	"S_SPHEREBOX11",
	"S_SPHEREBOX12",
	"S_DEADSPHEREBOX",

	// Random Item Pop
	"S_RANDOMITEMPOP1",
	"S_RANDOMITEMPOP2",
	"S_RANDOMITEMPOP3",
	"S_RANDOMITEMPOP4",
	//}

	"S_ITEMICON",

	// Item capsules
	"S_ITEMCAPSULE",
	"S_ITEMCAPSULE_TOP_SIDE",
	"S_ITEMCAPSULE_BOTTOM_SIDE_AIR",
	"S_ITEMCAPSULE_BOTTOM_SIDE_GROUND",
	//"S_ITEMCAPSULE_TOP",
	//"S_ITEMCAPSULE_BOTTOM",
	//"S_ITEMCAPSULE_INSIDE",

	// Signpost sparkles
	"S_SIGNSPARK1",
	"S_SIGNSPARK2",
	"S_SIGNSPARK3",
	"S_SIGNSPARK4",
	"S_SIGNSPARK5",
	"S_SIGNSPARK6",
	"S_SIGNSPARK7",
	"S_SIGNSPARK8",
	"S_SIGNSPARK9",
	"S_SIGNSPARK10",
	"S_SIGNSPARK11",

	// Drift Sparks
	"S_DRIFTSPARK_A1",
	"S_DRIFTSPARK_A2",
	"S_DRIFTSPARK_A3",
	"S_DRIFTSPARK_B1",
	"S_DRIFTSPARK_C1",
	"S_DRIFTSPARK_C2",
	"S_DRIFTSPARK_D1",
	"S_DRIFTSPARK_D2",

	// Brake drift sparks
	"S_BRAKEDRIFT",

	// Brake dust
	"S_BRAKEDUST1",
	"S_BRAKEDUST2",

	// Drift Smoke
	"S_DRIFTDUST1",
	"S_DRIFTDUST2",
	"S_DRIFTDUST3",
	"S_DRIFTDUST4",

	// Drift Sparkles
	"S_DRIFTWARNSPARK1",
	"S_DRIFTWARNSPARK2",
	"S_DRIFTWARNSPARK3",
	"S_DRIFTWARNSPARK4",

	// Drift electricity
	"S_DRIFTELECTRICITY",
	"S_DRIFTELECTRICSPARK",

	// Fast lines
	"S_FASTLINE1",
	"S_FASTLINE2",
	"S_FASTLINE3",
	"S_FASTLINE4",
	"S_FASTLINE5",

	// Fast dust release
	"S_FASTDUST1",
	"S_FASTDUST2",
	"S_FASTDUST3",
	"S_FASTDUST4",
	"S_FASTDUST5",
	"S_FASTDUST6",
	"S_FASTDUST7",

	// Drift boost effect
	"S_DRIFTEXPLODE1",
	"S_DRIFTEXPLODE2",
	"S_DRIFTEXPLODE3",
	"S_DRIFTEXPLODE4",
	"S_DRIFTEXPLODE5",
	"S_DRIFTEXPLODE6",
	"S_DRIFTEXPLODE7",
	"S_DRIFTEXPLODE8",

	// Drift boost clip
	"S_DRIFTCLIPA1",
	"S_DRIFTCLIPA2",
	"S_DRIFTCLIPA3",
	"S_DRIFTCLIPA4",
	"S_DRIFTCLIPA5",
	"S_DRIFTCLIPA6",
	"S_DRIFTCLIPA7",
	"S_DRIFTCLIPA8",
	"S_DRIFTCLIPA9",
	"S_DRIFTCLIPA10",
	"S_DRIFTCLIPA11",
	"S_DRIFTCLIPA12",
	"S_DRIFTCLIPA13",
	"S_DRIFTCLIPA14",
	"S_DRIFTCLIPA15",
	"S_DRIFTCLIPA16",
	"S_DRIFTCLIPB1",
	"S_DRIFTCLIPB2",
	"S_DRIFTCLIPB3",
	"S_DRIFTCLIPB4",
	"S_DRIFTCLIPB5",
	"S_DRIFTCLIPB6",
	"S_DRIFTCLIPB7",
	"S_DRIFTCLIPB8",

	// Drift boost clip spark
	"S_DRIFTCLIPSPARK",

	// Sneaker boost effect
	"S_BOOSTFLAME",
	"S_BOOSTSMOKESPAWNER",
	"S_BOOSTSMOKE1",
	"S_BOOSTSMOKE2",
	"S_BOOSTSMOKE3",
	"S_BOOSTSMOKE4",
	"S_BOOSTSMOKE5",
	"S_BOOSTSMOKE6",

	// Sneaker Fire Trail
	"S_KARTFIRE1",
	"S_KARTFIRE2",
	"S_KARTFIRE3",
	"S_KARTFIRE4",
	"S_KARTFIRE5",
	"S_KARTFIRE6",
	"S_KARTFIRE7",
	"S_KARTFIRE8",

	// Angel Island Drift Strat Dust (what a mouthful!)
	"S_KARTAIZDRIFTSTRAT",

	// Invincibility Sparks
	"S_KARTINVULN1",
	"S_KARTINVULN2",
	"S_KARTINVULN3",
	"S_KARTINVULN4",
	"S_KARTINVULN5",
	"S_KARTINVULN6",
	"S_KARTINVULN7",
	"S_KARTINVULN8",
	"S_KARTINVULN9",
	"S_KARTINVULN10",
	"S_KARTINVULN11",
	"S_KARTINVULN12",

	"S_KARTINVULNB1",
	"S_KARTINVULNB2",
	"S_KARTINVULNB3",
	"S_KARTINVULNB4",
	"S_KARTINVULNB5",
	"S_KARTINVULNB6",
	"S_KARTINVULNB7",
	"S_KARTINVULNB8",
	"S_KARTINVULNB9",
	"S_KARTINVULNB10",
	"S_KARTINVULNB11",
	"S_KARTINVULNB12",

	// Invincibility flash overlay
	"S_INVULNFLASH1",
	"S_INVULNFLASH2",
	"S_INVULNFLASH3",
	"S_INVULNFLASH4",

	"S_KARTINVLINES1",
	"S_KARTINVLINES2",
	"S_KARTINVLINES3",
	"S_KARTINVLINES4",
	"S_KARTINVLINES5",
	"S_KARTINVLINES6",
	"S_KARTINVLINES7",
	"S_KARTINVLINES8",
	"S_KARTINVLINES9",
	"S_KARTINVLINES10",
	"S_KARTINVLINES11",
	"S_KARTINVLINES12",
	"S_KARTINVLINES13",
	"S_KARTINVLINES14",
	"S_KARTINVLINES15",

	// Wipeout dust trail
	"S_WIPEOUTTRAIL1",
	"S_WIPEOUTTRAIL2",
	"S_WIPEOUTTRAIL3",
	"S_WIPEOUTTRAIL4",
	"S_WIPEOUTTRAIL5",

	// Rocket sneaker
	"S_ROCKETSNEAKER_L",
	"S_ROCKETSNEAKER_R",
	"S_ROCKETSNEAKER_LVIBRATE",
	"S_ROCKETSNEAKER_RVIBRATE",

	//{ Eggman Monitor
	"S_EGGMANITEM1",
	"S_EGGMANITEM2",
	"S_EGGMANITEM3",
	"S_EGGMANITEM4",
	"S_EGGMANITEM5",
	"S_EGGMANITEM6",
	"S_EGGMANITEM7",
	"S_EGGMANITEM8",
	"S_EGGMANITEM9",
	"S_EGGMANITEM10",
	"S_EGGMANITEM11",
	"S_EGGMANITEM12",
	"S_EGGMANITEM_DEAD",
	//}

	// Banana
	"S_BANANA",
	"S_BANANA_DEAD",

	//{ Orbinaut
	"S_ORBINAUT1",
	"S_ORBINAUT2",
	"S_ORBINAUT3",
	"S_ORBINAUT4",
	"S_ORBINAUT5",
	"S_ORBINAUT6",
	"S_ORBINAUT_DEAD",
	"S_ORBINAUT_SHIELD1",
	"S_ORBINAUT_SHIELD2",
	"S_ORBINAUT_SHIELD3",
	"S_ORBINAUT_SHIELD4",
	"S_ORBINAUT_SHIELD5",
	"S_ORBINAUT_SHIELD6",
	"S_ORBINAUT_SHIELDDEAD",
	//}
	//{ Jawz
	"S_JAWZ1",
	"S_JAWZ2",
	"S_JAWZ3",
	"S_JAWZ4",
	"S_JAWZ5",
	"S_JAWZ6",
	"S_JAWZ7",
	"S_JAWZ8",
	"S_JAWZ_DUD1",
	"S_JAWZ_DUD2",
	"S_JAWZ_DUD3",
	"S_JAWZ_DUD4",
	"S_JAWZ_DUD5",
	"S_JAWZ_DUD6",
	"S_JAWZ_DUD7",
	"S_JAWZ_DUD8",
	"S_JAWZ_SHIELD1",
	"S_JAWZ_SHIELD2",
	"S_JAWZ_SHIELD3",
	"S_JAWZ_SHIELD4",
	"S_JAWZ_SHIELD5",
	"S_JAWZ_SHIELD6",
	"S_JAWZ_SHIELD7",
	"S_JAWZ_SHIELD8",
	"S_JAWZ_DEAD1",
	"S_JAWZ_DEAD2",
	//}

	"S_PLAYERRETICULE", // Player reticule

	// Special Stage Mine
	"S_SSMINE1",
	"S_SSMINE2",
	"S_SSMINE3",
	"S_SSMINE4",
	"S_SSMINE_SHIELD1",
	"S_SSMINE_SHIELD2",
	"S_SSMINE_AIR1",
	"S_SSMINE_AIR2",
	"S_SSMINE_DEPLOY1",
	"S_SSMINE_DEPLOY2",
	"S_SSMINE_DEPLOY3",
	"S_SSMINE_DEPLOY4",
	"S_SSMINE_DEPLOY5",
	"S_SSMINE_DEPLOY6",
	"S_SSMINE_DEPLOY7",
	"S_SSMINE_DEPLOY8",
	"S_SSMINE_DEPLOY9",
	"S_SSMINE_DEPLOY10",
	"S_SSMINE_DEPLOY11",
	"S_SSMINE_DEPLOY12",
	"S_SSMINE_DEPLOY13",
	"S_SSMINE_EXPLODE",
	"S_SSMINE_EXPLODE2",

	// New explosion
	"S_QUICKBOOM1",
	"S_QUICKBOOM2",
	"S_QUICKBOOM3",
	"S_QUICKBOOM4",
	"S_QUICKBOOM5",
	"S_QUICKBOOM6",
	"S_QUICKBOOM7",
	"S_QUICKBOOM8",
	"S_QUICKBOOM9",
	"S_QUICKBOOM10",

	"S_SLOWBOOM1",
	"S_SLOWBOOM2",
	"S_SLOWBOOM3",
	"S_SLOWBOOM4",
	"S_SLOWBOOM5",
	"S_SLOWBOOM6",
	"S_SLOWBOOM7",
	"S_SLOWBOOM8",
	"S_SLOWBOOM9",
	"S_SLOWBOOM10",

	// Land Mine
	"S_LANDMINE",
	"S_LANDMINE_EXPLODE",

	// Drop Target
	"S_DROPTARGET",
	"S_DROPTARGET_SPIN",

	// Ballhog
	"S_BALLHOG1",
	"S_BALLHOG2",
	"S_BALLHOG3",
	"S_BALLHOG4",
	"S_BALLHOG5",
	"S_BALLHOG6",
	"S_BALLHOG7",
	"S_BALLHOG8",
	"S_BALLHOG_DEAD",
	"S_BALLHOGBOOM1",
	"S_BALLHOGBOOM2",
	"S_BALLHOGBOOM3",
	"S_BALLHOGBOOM4",
	"S_BALLHOGBOOM5",
	"S_BALLHOGBOOM6",
	"S_BALLHOGBOOM7",
	"S_BALLHOGBOOM8",
	"S_BALLHOGBOOM9",
	"S_BALLHOGBOOM10",
	"S_BALLHOGBOOM11",
	"S_BALLHOGBOOM12",
	"S_BALLHOGBOOM13",
	"S_BALLHOGBOOM14",
	"S_BALLHOGBOOM15",
	"S_BALLHOGBOOM16",

	// Self-Propelled Bomb - just an explosion for now...
	"S_SPB1",
	"S_SPB2",
	"S_SPB3",
	"S_SPB4",
	"S_SPB5",
	"S_SPB6",
	"S_SPB7",
	"S_SPB8",
	"S_SPB9",
	"S_SPB10",
	"S_SPB11",
	"S_SPB12",
	"S_SPB13",
	"S_SPB14",
	"S_SPB15",
	"S_SPB16",
	"S_SPB17",
	"S_SPB18",
	"S_SPB19",
	"S_SPB20",
	"S_SPB_DEAD",

	// Lightning Shield
	"S_LIGHTNINGSHIELD1",
	"S_LIGHTNINGSHIELD2",
	"S_LIGHTNINGSHIELD3",
	"S_LIGHTNINGSHIELD4",
	"S_LIGHTNINGSHIELD5",
	"S_LIGHTNINGSHIELD6",
	"S_LIGHTNINGSHIELD7",
	"S_LIGHTNINGSHIELD8",
	"S_LIGHTNINGSHIELD9",
	"S_LIGHTNINGSHIELD10",
	"S_LIGHTNINGSHIELD11",
	"S_LIGHTNINGSHIELD12",
	"S_LIGHTNINGSHIELD13",
	"S_LIGHTNINGSHIELD14",
	"S_LIGHTNINGSHIELD15",
	"S_LIGHTNINGSHIELD16",
	"S_LIGHTNINGSHIELD17",
	"S_LIGHTNINGSHIELD18",
	"S_LIGHTNINGSHIELD19",
	"S_LIGHTNINGSHIELD20",
	"S_LIGHTNINGSHIELD21",
	"S_LIGHTNINGSHIELD22",
	"S_LIGHTNINGSHIELD23",
	"S_LIGHTNINGSHIELD24",

	// Bubble Shield
	"S_BUBBLESHIELD1",
	"S_BUBBLESHIELD2",
	"S_BUBBLESHIELD3",
	"S_BUBBLESHIELD4",
	"S_BUBBLESHIELD5",
	"S_BUBBLESHIELD6",
	"S_BUBBLESHIELD7",
	"S_BUBBLESHIELD8",
	"S_BUBBLESHIELD9",
	"S_BUBBLESHIELD10",
	"S_BUBBLESHIELD11",
	"S_BUBBLESHIELD12",
	"S_BUBBLESHIELD13",
	"S_BUBBLESHIELD14",
	"S_BUBBLESHIELD15",
	"S_BUBBLESHIELD16",
	"S_BUBBLESHIELD17",
	"S_BUBBLESHIELD18",
	"S_BUBBLESHIELDBLOWUP",
	"S_BUBBLESHIELDTRAP1",
	"S_BUBBLESHIELDTRAP2",
	"S_BUBBLESHIELDTRAP3",
	"S_BUBBLESHIELDTRAP4",
	"S_BUBBLESHIELDTRAP5",
	"S_BUBBLESHIELDTRAP6",
	"S_BUBBLESHIELDTRAP7",
	"S_BUBBLESHIELDTRAP8",
	"S_BUBBLESHIELDWAVE1",
	"S_BUBBLESHIELDWAVE2",
	"S_BUBBLESHIELDWAVE3",
	"S_BUBBLESHIELDWAVE4",
	"S_BUBBLESHIELDWAVE5",
	"S_BUBBLESHIELDWAVE6",

	// Flame Shield
	"S_FLAMESHIELD1",
	"S_FLAMESHIELD2",
	"S_FLAMESHIELD3",
	"S_FLAMESHIELD4",
	"S_FLAMESHIELD5",
	"S_FLAMESHIELD6",
	"S_FLAMESHIELD7",
	"S_FLAMESHIELD8",
	"S_FLAMESHIELD9",
	"S_FLAMESHIELD10",
	"S_FLAMESHIELD11",
	"S_FLAMESHIELD12",
	"S_FLAMESHIELD13",
	"S_FLAMESHIELD14",
	"S_FLAMESHIELD15",
	"S_FLAMESHIELD16",
	"S_FLAMESHIELD17",
	"S_FLAMESHIELD18",

	"S_FLAMESHIELDDASH1",
	"S_FLAMESHIELDDASH2",
	"S_FLAMESHIELDDASH3",
	"S_FLAMESHIELDDASH4",
	"S_FLAMESHIELDDASH5",
	"S_FLAMESHIELDDASH6",
	"S_FLAMESHIELDDASH7",
	"S_FLAMESHIELDDASH8",
	"S_FLAMESHIELDDASH9",
	"S_FLAMESHIELDDASH10",
	"S_FLAMESHIELDDASH11",
	"S_FLAMESHIELDDASH12",

	"S_FLAMESHIELDDASH2_UNDERLAY",
	"S_FLAMESHIELDDASH5_UNDERLAY",
	"S_FLAMESHIELDDASH8_UNDERLAY",
	"S_FLAMESHIELDDASH11_UNDERLAY",

	"S_FLAMESHIELDPAPER",
	"S_FLAMESHIELDLINE1",
	"S_FLAMESHIELDLINE2",
	"S_FLAMESHIELDLINE3",
	"S_FLAMESHIELDFLASH",

	// Caked-Up Booty-Sheet Ghost
	"S_HYUDORO",

	// The legend
	"S_SINK",
	"S_SINK_SHIELD",
	"S_SINKTRAIL1",
	"S_SINKTRAIL2",
	"S_SINKTRAIL3",

	// Battle Mode bumper
	"S_BATTLEBUMPER1",
	"S_BATTLEBUMPER2",
	"S_BATTLEBUMPER3",

	"S_BATTLEBUMPER_EXCRYSTALA1",
	"S_BATTLEBUMPER_EXCRYSTALA2",
	"S_BATTLEBUMPER_EXCRYSTALA3",
	"S_BATTLEBUMPER_EXCRYSTALA4",

	"S_BATTLEBUMPER_EXCRYSTALB1",
	"S_BATTLEBUMPER_EXCRYSTALB2",
	"S_BATTLEBUMPER_EXCRYSTALB3",
	"S_BATTLEBUMPER_EXCRYSTALB4",

	"S_BATTLEBUMPER_EXCRYSTALC1",
	"S_BATTLEBUMPER_EXCRYSTALC2",
	"S_BATTLEBUMPER_EXCRYSTALC3",
	"S_BATTLEBUMPER_EXCRYSTALC4",

	"S_BATTLEBUMPER_EXSHELLA1",
	"S_BATTLEBUMPER_EXSHELLA2",

	"S_BATTLEBUMPER_EXSHELLB1",
	"S_BATTLEBUMPER_EXSHELLB2",

	"S_BATTLEBUMPER_EXSHELLC1",
	"S_BATTLEBUMPER_EXSHELLC2",

	"S_BATTLEBUMPER_EXDEBRIS1",
	"S_BATTLEBUMPER_EXDEBRIS2",

	"S_BATTLEBUMPER_EXBLAST1",
	"S_BATTLEBUMPER_EXBLAST2",
	"S_BATTLEBUMPER_EXBLAST3",
	"S_BATTLEBUMPER_EXBLAST4",
	"S_BATTLEBUMPER_EXBLAST5",
	"S_BATTLEBUMPER_EXBLAST6",
	"S_BATTLEBUMPER_EXBLAST7",
	"S_BATTLEBUMPER_EXBLAST8",
	"S_BATTLEBUMPER_EXBLAST9",
	"S_BATTLEBUMPER_EXBLAST10",

	// Tripwire
	"S_TRIPWIREBOOST_TOP",
	"S_TRIPWIREBOOST_BOTTOM",
	"S_TRIPWIREBOOST_BLAST_TOP",
	"S_TRIPWIREBOOST_BLAST_BOTTOM",

	// DEZ respawn laser
	"S_DEZLASER",
	"S_DEZLASER_TRAIL1",
	"S_DEZLASER_TRAIL2",
	"S_DEZLASER_TRAIL3",
	"S_DEZLASER_TRAIL4",
	"S_DEZLASER_TRAIL5",

	// Audience Members
	"S_RANDOMAUDIENCE",
	"S_AUDIENCE_CHAO_CHEER1",
	"S_AUDIENCE_CHAO_CHEER2",
	"S_AUDIENCE_CHAO_WIN1",
	"S_AUDIENCE_CHAO_WIN2",
	"S_AUDIENCE_CHAO_LOSE",

	// 1.0 Kart Decoratives
	"S_FLAYM1",
	"S_FLAYM2",
	"S_FLAYM3",
	"S_FLAYM4",
	"S_DEVIL",
	"S_ANGEL",
	"S_PALMTREE",
	"S_FLAG",
	"S_HEDGEHOG", // (Rimshot)
	"S_BUSH1",
	"S_TWEE",
	"S_HYDRANT",

	// New Misc Decorations
	"S_BIGPUMA1",
	"S_BIGPUMA2",
	"S_BIGPUMA3",
	"S_BIGPUMA4",
	"S_BIGPUMA5",
	"S_BIGPUMA6",
	"S_APPLE1",
	"S_APPLE2",
	"S_APPLE3",
	"S_APPLE4",
	"S_APPLE5",
	"S_APPLE6",
	"S_APPLE7",
	"S_APPLE8",

	// D00Dkart - Fall Flowers
	"S_DOOD_FLOWER1",
	"S_DOOD_FLOWER2",
	"S_DOOD_FLOWER3",
	"S_DOOD_FLOWER4",
	"S_DOOD_FLOWER5",
	"S_DOOD_FLOWER6",

	// D00Dkart - Super Circuit Box
	"S_DOOD_BOX1",
	"S_DOOD_BOX2",
	"S_DOOD_BOX3",
	"S_DOOD_BOX4",
	"S_DOOD_BOX5",

	// D00Dkart - Diddy Kong Racing Bumper
	"S_DOOD_BALLOON",

	// Chaotix Big Ring
	"S_BIGRING01",
	"S_BIGRING02",
	"S_BIGRING03",
	"S_BIGRING04",
	"S_BIGRING05",
	"S_BIGRING06",
	"S_BIGRING07",
	"S_BIGRING08",
	"S_BIGRING09",
	"S_BIGRING10",
	"S_BIGRING11",
	"S_BIGRING12",

	// SNES Objects
	"S_SNES_DONUTBUSH1",
	"S_SNES_DONUTBUSH2",
	"S_SNES_DONUTBUSH3",

	// GBA Objects
	"S_GBA_BOO1",
	"S_GBA_BOO2",
	"S_GBA_BOO3",
	"S_GBA_BOO4",

	// Sapphire Coast Mobs
	"S_BUZZBOMBER_LOOK1",
	"S_BUZZBOMBER_LOOK2",
	"S_BUZZBOMBER_FLY1",
	"S_BUZZBOMBER_FLY2",
	"S_BUZZBOMBER_FLY3",
	"S_BUZZBOMBER_FLY4",

	"S_CHOMPER_SPAWN",
	"S_CHOMPER_HOP1",
	"S_CHOMPER_HOP2",
	"S_CHOMPER_TURNAROUND",

	"S_PALMTREE2",
	"S_PURPLEFLOWER1",
	"S_PURPLEFLOWER2",
	"S_YELLOWFLOWER1",
	"S_YELLOWFLOWER2",
	"S_PLANT2",
	"S_PLANT3",
	"S_PLANT4",

	// Crystal Abyss Mobs
	"S_SKULL",
	"S_PHANTREE",
	"S_FLYINGGARG1",
	"S_FLYINGGARG2",
	"S_FLYINGGARG3",
	"S_FLYINGGARG4",
	"S_FLYINGGARG5",
	"S_FLYINGGARG6",
	"S_FLYINGGARG7",
	"S_FLYINGGARG8",
	"S_LAMPPOST",
	"S_MOSSYTREE",

	"S_BUMP1",
	"S_BUMP2",
	"S_BUMP3",

	"S_FLINGENERGY1",
	"S_FLINGENERGY2",
	"S_FLINGENERGY3",

	"S_CLASH1",
	"S_CLASH2",
	"S_CLASH3",
	"S_CLASH4",
	"S_CLASH5",
	"S_CLASH6",

	"S_FIREDITEM1",
	"S_FIREDITEM2",
	"S_FIREDITEM3",
	"S_FIREDITEM4",

	"S_INSTASHIELDA1", // No damage instashield effect
	"S_INSTASHIELDA2",
	"S_INSTASHIELDA3",
	"S_INSTASHIELDA4",
	"S_INSTASHIELDA5",
	"S_INSTASHIELDA6",
	"S_INSTASHIELDA7",
	"S_INSTASHIELDB1",
	"S_INSTASHIELDB2",
	"S_INSTASHIELDB3",
	"S_INSTASHIELDB4",
	"S_INSTASHIELDB5",
	"S_INSTASHIELDB6",
	"S_INSTASHIELDB7",

	"S_POWERCLASH", // Invinc/Grow no damage collide VFX

	"S_PLAYERARROW", // Above player arrow
	"S_PLAYERARROW_BOX",
	"S_PLAYERARROW_ITEM",
	"S_PLAYERARROW_NUMBER",
	"S_PLAYERARROW_X",
	"S_PLAYERARROW_WANTED1",
	"S_PLAYERARROW_WANTED2",
	"S_PLAYERARROW_WANTED3",
	"S_PLAYERARROW_WANTED4",
	"S_PLAYERARROW_WANTED5",
	"S_PLAYERARROW_WANTED6",
	"S_PLAYERARROW_WANTED7",

	"S_PLAYERBOMB1", // Player bomb overlay
	"S_PLAYERBOMB2",
	"S_PLAYERBOMB3",
	"S_PLAYERBOMB4",
	"S_PLAYERBOMB5",
	"S_PLAYERBOMB6",
	"S_PLAYERBOMB7",
	"S_PLAYERBOMB8",
	"S_PLAYERBOMB9",
	"S_PLAYERBOMB10",
	"S_PLAYERBOMB11",
	"S_PLAYERBOMB12",
	"S_PLAYERBOMB13",
	"S_PLAYERBOMB14",
	"S_PLAYERBOMB15",
	"S_PLAYERBOMB16",
	"S_PLAYERBOMB17",
	"S_PLAYERBOMB18",
	"S_PLAYERBOMB19",
	"S_PLAYERBOMB20",

	"S_PLAYERITEM1", // Player item overlay
	"S_PLAYERITEM2",
	"S_PLAYERITEM3",
	"S_PLAYERITEM4",
	"S_PLAYERITEM5",
	"S_PLAYERITEM6",
	"S_PLAYERITEM7",
	"S_PLAYERITEM8",
	"S_PLAYERITEM9",
	"S_PLAYERITEM10",
	"S_PLAYERITEM11",
	"S_PLAYERITEM12",

	"S_PLAYERFAKE1", // Player fake overlay
	"S_PLAYERFAKE2",
	"S_PLAYERFAKE3",
	"S_PLAYERFAKE4",
	"S_PLAYERFAKE5",
	"S_PLAYERFAKE6",
	"S_PLAYERFAKE7",
	"S_PLAYERFAKE8",
	"S_PLAYERFAKE9",
	"S_PLAYERFAKE10",
	"S_PLAYERFAKE11",
	"S_PLAYERFAKE12",

	"S_KARMAWHEEL", // Karma player wheels

	"S_BATTLEPOINT1A", // Battle point indicators
	"S_BATTLEPOINT1B",
	"S_BATTLEPOINT1C",
	"S_BATTLEPOINT1D",
	"S_BATTLEPOINT1E",
	"S_BATTLEPOINT1F",
	"S_BATTLEPOINT1G",
	"S_BATTLEPOINT1H",
	"S_BATTLEPOINT1I",

	"S_BATTLEPOINT2A",
	"S_BATTLEPOINT2B",
	"S_BATTLEPOINT2C",
	"S_BATTLEPOINT2D",
	"S_BATTLEPOINT2E",
	"S_BATTLEPOINT2F",
	"S_BATTLEPOINT2G",
	"S_BATTLEPOINT2H",
	"S_BATTLEPOINT2I",

	"S_BATTLEPOINT3A",
	"S_BATTLEPOINT3B",
	"S_BATTLEPOINT3C",
	"S_BATTLEPOINT3D",
	"S_BATTLEPOINT3E",
	"S_BATTLEPOINT3F",
	"S_BATTLEPOINT3G",
	"S_BATTLEPOINT3H",
	"S_BATTLEPOINT3I",

	// Lightning shield use stuff;
	"S_KSPARK1",	// Sparkling Radius
	"S_KSPARK2",
	"S_KSPARK3",
	"S_KSPARK4",
	"S_KSPARK5",
	"S_KSPARK6",
	"S_KSPARK7",
	"S_KSPARK8",
	"S_KSPARK9",
	"S_KSPARK10",
	"S_KSPARK11",
	"S_KSPARK12",
	"S_KSPARK13",	// ... that's an awful lot.

	"S_LZIO11",	// Straight lightning bolt
	"S_LZIO12",
	"S_LZIO13",
	"S_LZIO14",
	"S_LZIO15",
	"S_LZIO16",
	"S_LZIO17",
	"S_LZIO18",
	"S_LZIO19",

	"S_LZIO21",	// Straight lightning bolt (flipped)
	"S_LZIO22",
	"S_LZIO23",
	"S_LZIO24",
	"S_LZIO25",
	"S_LZIO26",
	"S_LZIO27",
	"S_LZIO28",
	"S_LZIO29",

	"S_KLIT1",	// Diagonal lightning. No, it not being straight doesn't make it gay.
	"S_KLIT2",
	"S_KLIT3",
	"S_KLIT4",
	"S_KLIT5",
	"S_KLIT6",
	"S_KLIT7",
	"S_KLIT8",
	"S_KLIT9",
	"S_KLIT10",
	"S_KLIT11",
	"S_KLIT12",

	"S_FZEROSMOKE1", // F-Zero NO CONTEST explosion
	"S_FZEROSMOKE2",
	"S_FZEROSMOKE3",
	"S_FZEROSMOKE4",
	"S_FZEROSMOKE5",

	"S_FZEROBOOM1",
	"S_FZEROBOOM2",
	"S_FZEROBOOM3",
	"S_FZEROBOOM4",
	"S_FZEROBOOM5",
	"S_FZEROBOOM6",
	"S_FZEROBOOM7",
	"S_FZEROBOOM8",
	"S_FZEROBOOM9",
	"S_FZEROBOOM10",
	"S_FZEROBOOM11",
	"S_FZEROBOOM12",

	"S_FZSLOWSMOKE1",
	"S_FZSLOWSMOKE2",
	"S_FZSLOWSMOKE3",
	"S_FZSLOWSMOKE4",
	"S_FZSLOWSMOKE5",

	// Various plants
	"S_SONICBUSH",

	// Marble Zone
	"S_MARBLEFLAMEPARTICLE",
	"S_MARBLETORCH",
	"S_MARBLELIGHT",
	"S_MARBLEBURNER",

	// CD Special Stage
	"S_CDUFO",
	"S_CDUFO_DIE",

	// Rusty Rig
	"S_RUSTYLAMP_ORANGE",
	"S_RUSTYCHAIN",

	// Smokin' & Vapin' (Don't try this at home, kids!)
	"S_PETSMOKE0",
	"S_PETSMOKE1",
	"S_PETSMOKE2",
	"S_PETSMOKE3",
	"S_PETSMOKE4",
	"S_PETSMOKE5",
	"S_VVVAPING0",
	"S_VVVAPING1",
	"S_VVVAPING2",
	"S_VVVAPING3",
	"S_VVVAPING4",
	"S_VVVAPING5",
	"S_VVVAPE",

	// Hill Top Zone
	"S_HTZTREE",
	"S_HTZBUSH",

	// Ports of gardens
	"S_SGVINE1",
	"S_SGVINE2",
	"S_SGVINE3",
	"S_PGTREE",
	"S_PGFLOWER1",
	"S_PGFLOWER2",
	"S_PGFLOWER3",
	"S_PGBUSH",
	"S_DHPILLAR",

	// Midnight Channel stuff:
	"S_SPOTLIGHT",	// Spotlight decoration
	"S_RANDOMSHADOW",	// Random Shadow. They're static and don't do nothing.
	"S_GARU1",
	"S_GARU2",
	"S_GARU3",
	"S_TGARU",
	"S_TGARU1",
	"S_TGARU2",
	"S_TGARU3",	// Wind attack used by Roaming Shadows on Players.
	"S_ROAMINGSHADOW",	// Roaming Shadow (the one that uses above's wind attack or smth)
	"S_MAYONAKAARROW",	// Arrow sign

	// Mementos stuff:
	"S_REAPER_INVIS",		// Reaper waiting for spawning
	"S_REAPER",			// Reaper main frame where its thinker is handled
	"S_MEMENTOSTP",		// Mementos teleporter state. (Used for spawning particles)

	// JackInTheBox
	"S_JITB1",
	"S_JITB2",
	"S_JITB3",
	"S_JITB4",
	"S_JITB5",
	"S_JITB6",

	// Color Drive
	"S_CDMOONSP",
	"S_CDBUSHSP",
	"S_CDTREEASP",
	"S_CDTREEBSP",

	// Daytona Speedway
	"S_PINETREE",
	"S_PINETREE_SIDE",

	// Egg Zeppelin
	"S_EZZPROPELLER",
	"S_EZZPROPELLER_BLADE",

	// Desert Palace
	"S_DP_PALMTREE",

	// Aurora Atoll
	"S_AAZTREE_SEG",
	"S_AAZTREE_COCONUT",
	"S_AAZTREE_LEAF",

	// Barren Badlands
	"S_BBZDUST1", // Dust
	"S_BBZDUST2",
	"S_BBZDUST3",
	"S_BBZDUST4",
	"S_FROGGER", // Frog badniks
	"S_FROGGER_ATTACK",
	"S_FROGGER_JUMP",
	"S_FROGTONGUE",
	"S_FROGTONGUE_JOINT",
	"S_ROBRA", // Black cobra badniks
	"S_ROBRA_HEAD",
	"S_ROBRA_JOINT",
	"S_ROBRASHELL_INSIDE",
	"S_ROBRASHELL_OUTSIDE",
	"S_BLUEROBRA", // Blue cobra badniks
	"S_BLUEROBRA_HEAD",
	"S_BLUEROBRA_JOINT",

	// Eerie Grove
	"S_EERIEFOG1",
	"S_EERIEFOG2",
	"S_EERIEFOG3",
	"S_EERIEFOG4",
	"S_EERIEFOG5",

	// SMK ports
	"S_SMK_PIPE1", // Generic pipes
	"S_SMK_PIPE2",
	"S_SMK_MOLE", // Donut Plains Monty Moles
	"S_SMK_THWOMP", // Bowser Castle Thwomps
	"S_SMK_SNOWBALL", // Vanilla Lake snowballs
	"S_SMK_ICEBLOCK", // Vanilla Lake breakable ice blocks
	"S_SMK_ICEBLOCK2",
	"S_SMK_ICEBLOCK_DEBRIS",
	"S_SMK_ICEBLOCK_DEBRIS2",

	// Ezo's maps
	"S_BLUEFIRE1",
	"S_BLUEFIRE2",
	"S_BLUEFIRE3",
	"S_BLUEFIRE4",
	"S_GREENFIRE1",
	"S_GREENFIRE2",
	"S_GREENFIRE3",
	"S_GREENFIRE4",
	"S_REGALCHEST",
	"S_CHIMERASTATUE",
	"S_DRAGONSTATUE",
	"S_LIZARDMANSTATUE",
	"S_PEGASUSSTATUE",
	"S_ZELDAFIRE1",
	"S_ZELDAFIRE2",
	"S_ZELDAFIRE3",
	"S_ZELDAFIRE4",
	"S_GANBARETHING",
	"S_GANBAREDUCK",
	"S_GANBARETREE",
	"S_MONOIDLE",
	"S_MONOCHASE1",
	"S_MONOCHASE2",
	"S_MONOCHASE3",
	"S_MONOCHASE4",
	"S_MONOPAIN",
	"S_REDZELDAFIRE1",
	"S_REDZELDAFIRE2",
	"S_REDZELDAFIRE3",
	"S_REDZELDAFIRE4",
	"S_BOWLINGPIN",
	"S_BOWLINGHIT1",
	"S_BOWLINGHIT2",
	"S_BOWLINGHIT3",
	"S_BOWLINGHIT4",
	"S_ARIDTOAD",
	"S_TOADHIT1",
	"S_TOADHIT2",
	"S_TOADHIT3",
	"S_TOADHIT4",
	"S_EBARRELIDLE",
	"S_EBARREL1",
	"S_EBARREL2",
	"S_EBARREL3",
	"S_EBARREL4",
	"S_EBARREL5",
	"S_EBARREL6",
	"S_EBARREL7",
	"S_EBARREL8",
	"S_EBARREL9",
	"S_EBARREL10",
	"S_EBARREL11",
	"S_EBARREL12",
	"S_EBARREL13",
	"S_EBARREL14",
	"S_EBARREL15",
	"S_EBARREL16",
	"S_EBARREL17",
	"S_EBARREL18",
	"S_EBARREL19",
	"S_MERRYHORSE",
	"S_BLUEFRUIT",
	"S_ORANGEFRUIT",
	"S_REDFRUIT",
	"S_PINKFRUIT",
	"S_ADVENTURESPIKEA1",
	"S_ADVENTURESPIKEA2",
	"S_ADVENTURESPIKEB1",
	"S_ADVENTURESPIKEB2",
	"S_ADVENTURESPIKEC1",
	"S_ADVENTURESPIKEC2",
	"S_BOOSTPROMPT1",
	"S_BOOSTPROMPT2",
	"S_BOOSTOFF1",
	"S_BOOSTOFF2",
	"S_BOOSTON1",
	"S_BOOSTON2",
	"S_LIZARDMAN",
	"S_LIONMAN",

	"S_KARMAFIREWORK1",
	"S_KARMAFIREWORK2",
	"S_KARMAFIREWORK3",
	"S_KARMAFIREWORK4",
	"S_KARMAFIREWORKTRAIL",

	// Opaque smoke version, to prevent lag
	"S_OPAQUESMOKE1",
	"S_OPAQUESMOKE2",
	"S_OPAQUESMOKE3",
	"S_OPAQUESMOKE4",
	"S_OPAQUESMOKE5",

	"S_FOLLOWERBUBBLE_FRONT",
	"S_FOLLOWERBUBBLE_BACK",

	"S_GCHAOIDLE",
	"S_GCHAOFLY",
	"S_GCHAOSAD1",
	"S_GCHAOSAD2",
	"S_GCHAOSAD3",
	"S_GCHAOSAD4",
	"S_GCHAOHAPPY1",
	"S_GCHAOHAPPY2",
	"S_GCHAOHAPPY3",
	"S_GCHAOHAPPY4",

	"S_CHEESEIDLE",
	"S_CHEESEFLY",
	"S_CHEESESAD1",
	"S_CHEESESAD2",
	"S_CHEESESAD3",
	"S_CHEESESAD4",
	"S_CHEESEHAPPY1",
	"S_CHEESEHAPPY2",
	"S_CHEESEHAPPY3",
	"S_CHEESEHAPPY4",

	"S_RINGDEBT",
	"S_RINGSPARKS1",
	"S_RINGSPARKS2",
	"S_RINGSPARKS3",
	"S_RINGSPARKS4",
	"S_RINGSPARKS5",
	"S_RINGSPARKS6",
	"S_RINGSPARKS7",
	"S_RINGSPARKS8",
	"S_RINGSPARKS9",
	"S_RINGSPARKS10",
	"S_RINGSPARKS11",
	"S_RINGSPARKS12",
	"S_RINGSPARKS13",
	"S_RINGSPARKS14",
	"S_RINGSPARKS15",

	"S_GAINAX_TINY",
	"S_GAINAX_HUGE",
	"S_GAINAX_MID1",
	"S_GAINAX_MID2",

	"S_DRAFTDUST1",
	"S_DRAFTDUST2",
	"S_DRAFTDUST3",
	"S_DRAFTDUST4",
	"S_DRAFTDUST5",

	"S_TIREGREASE",

	"S_OVERTIME_BULB1",
	"S_OVERTIME_BULB2",
	"S_OVERTIME_LASER",
	"S_OVERTIME_CENTER",

	"S_BATTLECAPSULE_SIDE1",
	"S_BATTLECAPSULE_SIDE2",
	"S_BATTLECAPSULE_TOP",
	"S_BATTLECAPSULE_BUTTON",
	"S_BATTLECAPSULE_SUPPORT",
	"S_BATTLECAPSULE_SUPPORTFLY",

	"S_WAYPOINTORB",
	"S_WAYPOINTSPLAT",
	"S_EGOORB",

	"S_WATERTRAIL1",
	"S_WATERTRAIL2",
	"S_WATERTRAIL3",
	"S_WATERTRAIL4",
	"S_WATERTRAIL5",
	"S_WATERTRAIL6",
	"S_WATERTRAIL7",
	"S_WATERTRAIL8",
	"S_WATERTRAILUNDERLAY1",
	"S_WATERTRAILUNDERLAY2",
	"S_WATERTRAILUNDERLAY3",
	"S_WATERTRAILUNDERLAY4",
	"S_WATERTRAILUNDERLAY5",
	"S_WATERTRAILUNDERLAY6",
	"S_WATERTRAILUNDERLAY7",
	"S_WATERTRAILUNDERLAY8",

	"S_SPINDASHDUST",
	"S_SPINDASHWIND",

	"S_SOFTLANDING1",
	"S_SOFTLANDING2",
	"S_SOFTLANDING3",
	"S_SOFTLANDING4",
	"S_SOFTLANDING5",

	"S_DOWNLINE1",
	"S_DOWNLINE2",
	"S_DOWNLINE3",
	"S_DOWNLINE4",
	"S_DOWNLINE5",

	"S_HOLDBUBBLE",

	// Finish line beam
	"S_FINISHBEAM1",
	"S_FINISHBEAM2",
	"S_FINISHBEAM3",
	"S_FINISHBEAM4",
	"S_FINISHBEAM5",
	"S_FINISHBEAMEND1",
	"S_FINISHBEAMEND2",

	// Funny Spike
	"S_DEBTSPIKE1",
	"S_DEBTSPIKE2",
	"S_DEBTSPIKE3",
	"S_DEBTSPIKE4",
	"S_DEBTSPIKE5",
	"S_DEBTSPIKE6",
	"S_DEBTSPIKE7",
	"S_DEBTSPIKE8",
	"S_DEBTSPIKE9",
	"S_DEBTSPIKEA",
	"S_DEBTSPIKEB",
	"S_DEBTSPIKEC",
	"S_DEBTSPIKED",
	"S_DEBTSPIKEE",

	// Sparks when driving on stairs
	"S_JANKSPARK1",
	"S_JANKSPARK2",
	"S_JANKSPARK3",
	"S_JANKSPARK4",
};

// RegEx to generate this from info.h: ^\tMT_([^,]+), --> \t"MT_\1",
// I am leaving the prefixes solely for clarity to programmers,
// because sadly no one remembers this place while searching for full state names.
const char *const MOBJTYPE_LIST[] = {  // array length left dynamic for sanity testing later.
	"MT_NULL",
	"MT_UNKNOWN",

	"MT_THOK", // Thok! mobj
	"MT_SHADOW", // Linkdraw Shadow (for invisible objects)
	"MT_PLAYER",
	"MT_KART_LEFTOVER",
	"MT_KART_TIRE",

	// Enemies
	"MT_BLUECRAWLA", // Crawla (Blue)
	"MT_REDCRAWLA", // Crawla (Red)
	"MT_GFZFISH", // SDURF
	"MT_GOLDBUZZ", // Buzz (Gold)
	"MT_REDBUZZ", // Buzz (Red)
	"MT_JETTBOMBER", // Jetty-Syn Bomber
	"MT_JETTGUNNER", // Jetty-Syn Gunner
	"MT_CRAWLACOMMANDER", // Crawla Commander
	"MT_DETON", // Deton
	"MT_SKIM", // Skim mine dropper
	"MT_TURRET", // Industrial Turret
	"MT_POPUPTURRET", // Pop-Up Turret
	"MT_SPINCUSHION", // Spincushion
	"MT_CRUSHSTACEAN", // Crushstacean
	"MT_CRUSHCLAW", // Big meaty claw
	"MT_CRUSHCHAIN", // Chain
	"MT_BANPYURA", // Banpyura
	"MT_BANPSPRING", // Banpyura spring
	"MT_JETJAW", // Jet Jaw
	"MT_SNAILER", // Snailer
	"MT_VULTURE", // BASH
	"MT_POINTY", // Pointy
	"MT_POINTYBALL", // Pointy Ball
	"MT_ROBOHOOD", // Robo-Hood
	"MT_FACESTABBER", // Castlebot Facestabber
	"MT_FACESTABBERSPEAR", // Castlebot Facestabber spear aura
	"MT_EGGGUARD", // Egg Guard
	"MT_EGGSHIELD", // Egg Guard's shield
	"MT_GSNAPPER", // Green Snapper
	"MT_SNAPPER_LEG", // Green Snapper leg
	"MT_SNAPPER_HEAD", // Green Snapper head
	"MT_MINUS", // Minus
	"MT_MINUSDIRT", // Minus dirt
	"MT_SPRINGSHELL", // Spring Shell
	"MT_YELLOWSHELL", // Spring Shell (yellow)
	"MT_UNIDUS", // Unidus
	"MT_UNIBALL", // Unidus Ball
	"MT_CANARIVORE", // Canarivore
	"MT_CANARIVORE_GAS", // Canarivore gas
	"MT_PYREFLY", // Pyre Fly
	"MT_PYREFLY_FIRE", // Pyre Fly fire
	"MT_PTERABYTESPAWNER", // Pterabyte spawner
	"MT_PTERABYTEWAYPOINT", // Pterabyte waypoint
	"MT_PTERABYTE", // Pterabyte
	"MT_DRAGONBOMBER", // Dragonbomber
	"MT_DRAGONWING", // Dragonbomber wing
	"MT_DRAGONTAIL", // Dragonbomber tail segment
	"MT_DRAGONMINE", // Dragonbomber mine

	// Generic Boss Items
	"MT_BOSSEXPLODE",
	"MT_SONIC3KBOSSEXPLODE",
	"MT_BOSSFLYPOINT",
	"MT_EGGTRAP",
	"MT_BOSS3WAYPOINT",
	"MT_BOSS9GATHERPOINT",
	"MT_BOSSJUNK",

	// Boss 1
	"MT_EGGMOBILE",
	"MT_JETFUME1",
	"MT_EGGMOBILE_BALL",
	"MT_EGGMOBILE_TARGET",
	"MT_EGGMOBILE_FIRE",

	// Boss 2
	"MT_EGGMOBILE2",
	"MT_EGGMOBILE2_POGO",
	"MT_GOOP",
	"MT_GOOPTRAIL",

	// Boss 3
	"MT_EGGMOBILE3",
	"MT_FAKEMOBILE",
	"MT_SHOCKWAVE",

	// Boss 4
	"MT_EGGMOBILE4",
	"MT_EGGMOBILE4_MACE",
	"MT_JETFLAME",
	"MT_EGGROBO1",
	"MT_EGGROBO1JET",

	// Boss 5
	"MT_FANG",
	"MT_BROKENROBOT",
	"MT_VWREF",
	"MT_VWREB",
	"MT_PROJECTORLIGHT",
	"MT_FBOMB",
	"MT_TNTDUST", // also used by barrel
	"MT_FSGNA",
	"MT_FSGNB",
	"MT_FANGWAYPOINT",

	// Metal Sonic (Boss 9)
	"MT_METALSONIC_RACE",
	"MT_METALSONIC_BATTLE",
	"MT_MSSHIELD_FRONT",
	"MT_MSGATHER",

	// Collectible Items
	"MT_RING",
	"MT_FLINGRING", // Lost ring
	"MT_DEBTSPIKE", // Ring debt funny spike
	"MT_BLUESPHERE",  // Blue sphere for special stages
	"MT_FLINGBLUESPHERE", // Lost blue sphere
	"MT_BOMBSPHERE",
	"MT_REDTEAMRING",  //Rings collectable by red team.
	"MT_BLUETEAMRING", //Rings collectable by blue team.
	"MT_TOKEN", // Special Stage token for special stage
	"MT_REDFLAG", // Red CTF Flag
	"MT_BLUEFLAG", // Blue CTF Flag
	"MT_EMBLEM",
	"MT_EMERALD",
	"MT_EMERALDSPARK",
	"MT_EMERHUNT", // Emerald Hunt
	"MT_EMERALDSPAWN", // Emerald spawner w/ delay

	// Springs and others
	"MT_FAN",
	"MT_STEAM",
	"MT_BUMPER",
	"MT_BALLOON",

	"MT_YELLOWSPRING",
	"MT_REDSPRING",
	"MT_BLUESPRING",
	"MT_GREYSPRING",
	"MT_POGOSPRING",
	"MT_YELLOWDIAG", // Yellow Diagonal Spring
	"MT_REDDIAG", // Red Diagonal Spring
	"MT_BLUEDIAG", // Blue Diagonal Spring
	"MT_GREYDIAG", // Grey Diagonal Spring
	"MT_YELLOWHORIZ", // Yellow Horizontal Spring
	"MT_REDHORIZ", // Red Horizontal Spring
	"MT_BLUEHORIZ", // Blue Horizontal Spring
	"MT_GREYHORIZ", // Grey Horizontal Spring

	"MT_BOOSTERSEG",
	"MT_BOOSTERROLLER",
	"MT_YELLOWBOOSTER",
	"MT_REDBOOSTER",

	// Interactive Objects
	"MT_BUBBLES", // Bubble source
	"MT_SIGN", // Level end sign
	"MT_SIGN_PIECE",
	"MT_SPIKEBALL", // Spike Ball
	"MT_SPINFIRE",
	"MT_SPIKE",
	"MT_WALLSPIKE",
	"MT_WALLSPIKEBASE",
	"MT_STARPOST",
	"MT_BIGMINE",
	"MT_BLASTEXECUTOR",
	"MT_CANNONLAUNCHER",

	// Monitor miscellany
	"MT_BOXSPARKLE",

	// Monitor boxes -- regular
	"MT_RING_BOX",
	"MT_PITY_BOX",
	"MT_ATTRACT_BOX",
	"MT_FORCE_BOX",
	"MT_ARMAGEDDON_BOX",
	"MT_WHIRLWIND_BOX",
	"MT_ELEMENTAL_BOX",
	"MT_SNEAKERS_BOX",
	"MT_INVULN_BOX",
	"MT_1UP_BOX",
	"MT_EGGMAN_BOX",
	"MT_MIXUP_BOX",
	"MT_MYSTERY_BOX",
	"MT_GRAVITY_BOX",
	"MT_RECYCLER_BOX",
	"MT_SCORE1K_BOX",
	"MT_SCORE10K_BOX",
	"MT_FLAMEAURA_BOX",
	"MT_BUBBLEWRAP_BOX",
	"MT_THUNDERCOIN_BOX",

	// Monitor boxes -- repeating (big) boxes
	"MT_PITY_GOLDBOX",
	"MT_ATTRACT_GOLDBOX",
	"MT_FORCE_GOLDBOX",
	"MT_ARMAGEDDON_GOLDBOX",
	"MT_WHIRLWIND_GOLDBOX",
	"MT_ELEMENTAL_GOLDBOX",
	"MT_SNEAKERS_GOLDBOX",
	"MT_INVULN_GOLDBOX",
	"MT_EGGMAN_GOLDBOX",
	"MT_GRAVITY_GOLDBOX",
	"MT_FLAMEAURA_GOLDBOX",
	"MT_BUBBLEWRAP_GOLDBOX",
	"MT_THUNDERCOIN_GOLDBOX",

	// Monitor boxes -- special
	"MT_RING_REDBOX",
	"MT_RING_BLUEBOX",

	// Monitor icons
	"MT_RING_ICON",
	"MT_PITY_ICON",
	"MT_ATTRACT_ICON",
	"MT_FORCE_ICON",
	"MT_ARMAGEDDON_ICON",
	"MT_WHIRLWIND_ICON",
	"MT_ELEMENTAL_ICON",
	"MT_SNEAKERS_ICON",
	"MT_INVULN_ICON",
	"MT_1UP_ICON",
	"MT_EGGMAN_ICON",
	"MT_MIXUP_ICON",
	"MT_GRAVITY_ICON",
	"MT_RECYCLER_ICON",
	"MT_SCORE1K_ICON",
	"MT_SCORE10K_ICON",
	"MT_FLAMEAURA_ICON",
	"MT_BUBBLEWRAP_ICON",
	"MT_THUNDERCOIN_ICON",

	// Projectiles
	"MT_ROCKET",
	"MT_LASER",
	"MT_TORPEDO",
	"MT_TORPEDO2", // silent
	"MT_ENERGYBALL",
	"MT_MINE", // Skim/Jetty-Syn mine
	"MT_JETTBULLET", // Jetty-Syn Bullet
	"MT_TURRETLASER",
	"MT_CANNONBALL", // Cannonball
	"MT_CANNONBALLDECOR", // Decorative/still cannonball
	"MT_ARROW", // Arrow
	"MT_DEMONFIRE", // Glaregoyle fire

	// The letter
	"MT_LETTER",

	// Greenflower Scenery
	"MT_GFZFLOWER1",
	"MT_GFZFLOWER2",
	"MT_GFZFLOWER3",

	"MT_BLUEBERRYBUSH",
	"MT_BERRYBUSH",
	"MT_BUSH",

	// Trees (both GFZ and misc)
	"MT_GFZTREE",
	"MT_GFZBERRYTREE",
	"MT_GFZCHERRYTREE",
	"MT_CHECKERTREE",
	"MT_CHECKERSUNSETTREE",
	"MT_FHZTREE", // Frozen Hillside
	"MT_FHZPINKTREE",
	"MT_POLYGONTREE",
	"MT_BUSHTREE",
	"MT_BUSHREDTREE",
	"MT_SPRINGTREE",

	// Techno Hill Scenery
	"MT_THZFLOWER1",
	"MT_THZFLOWER2",
	"MT_THZFLOWER3",
	"MT_THZTREE", // Steam whistle tree/bush
	"MT_THZTREEBRANCH", // branch of said tree
	"MT_ALARM",

	// Deep Sea Scenery
	"MT_GARGOYLE", // Deep Sea Gargoyle
	"MT_BIGGARGOYLE", // Deep Sea Gargoyle (Big)
	"MT_SEAWEED", // DSZ Seaweed
	"MT_WATERDRIP", // Dripping Water source
	"MT_WATERDROP", // Water drop from dripping water
	"MT_CORAL1", // Coral
	"MT_CORAL2",
	"MT_CORAL3",
	"MT_CORAL4",
	"MT_CORAL5",
	"MT_BLUECRYSTAL", // Blue Crystal
	"MT_KELP", // Kelp
	"MT_ANIMALGAETOP", // Animated algae top
	"MT_ANIMALGAESEG", // Animated algae segment
	"MT_DSZSTALAGMITE", // Deep Sea 1 Stalagmite
	"MT_DSZ2STALAGMITE", // Deep Sea 2 Stalagmite
	"MT_LIGHTBEAM", // DSZ Light beam

	// Castle Eggman Scenery
	"MT_CHAIN", // CEZ Chain
	"MT_FLAME", // Flame (has corona)
	"MT_FLAMEPARTICLE",
	"MT_EGGSTATUE", // Eggman Statue
	"MT_MACEPOINT", // Mace rotation point
	"MT_CHAINMACEPOINT", // Combination of chains and maces point
	"MT_SPRINGBALLPOINT", // Spring ball point
	"MT_CHAINPOINT", // Mace chain
	"MT_HIDDEN_SLING", // Spin mace chain (activatable)
	"MT_FIREBARPOINT", // Firebar
	"MT_CUSTOMMACEPOINT", // Custom mace
	"MT_SMALLMACECHAIN", // Small Mace Chain
	"MT_BIGMACECHAIN", // Big Mace Chain
	"MT_SMALLMACE", // Small Mace
	"MT_BIGMACE", // Big Mace
	"MT_SMALLGRABCHAIN", // Small Grab Chain
	"MT_BIGGRABCHAIN", // Big Grab Chain
	"MT_YELLOWSPRINGBALL", // Yellow spring on a ball
	"MT_REDSPRINGBALL", // Red spring on a ball
	"MT_SMALLFIREBAR", // Small Firebar
	"MT_BIGFIREBAR", // Big Firebar
	"MT_CEZFLOWER", // Flower
	"MT_CEZPOLE1", // Pole (with red banner)
	"MT_CEZPOLE2", // Pole (with blue banner)
	"MT_CEZBANNER1", // Banner (red)
	"MT_CEZBANNER2", // Banner (blue)
	"MT_PINETREE", // Pine Tree
	"MT_CEZBUSH1", // Bush 1
	"MT_CEZBUSH2", // Bush 2
	"MT_CANDLE", // Candle
	"MT_CANDLEPRICKET", // Candle pricket
	"MT_FLAMEHOLDER", // Flame holder
	"MT_FIRETORCH", // Fire torch
	"MT_WAVINGFLAG1", // Waving flag (red)
	"MT_WAVINGFLAG2", // Waving flag (blue)
	"MT_WAVINGFLAGSEG1", // Waving flag segment (red)
	"MT_WAVINGFLAGSEG2", // Waving flag segment (blue)
	"MT_CRAWLASTATUE", // Crawla statue
	"MT_FACESTABBERSTATUE", // Facestabber statue
	"MT_SUSPICIOUSFACESTABBERSTATUE", // :eggthinking:
	"MT_BRAMBLES", // Brambles

	// Arid Canyon Scenery
	"MT_BIGTUMBLEWEED",
	"MT_LITTLETUMBLEWEED",
	"MT_CACTI1", // Tiny Red Flower Cactus
	"MT_CACTI2", // Small Red Flower Cactus
	"MT_CACTI3", // Tiny Blue Flower Cactus
	"MT_CACTI4", // Small Blue Flower Cactus
	"MT_CACTI5", // Prickly Pear
	"MT_CACTI6", // Barrel Cactus
	"MT_CACTI7", // Tall Barrel Cactus
	"MT_CACTI8", // Armed Cactus
	"MT_CACTI9", // Ball Cactus
	"MT_CACTI10", // Tiny Cactus
	"MT_CACTI11", // Small Cactus
	"MT_CACTITINYSEG", // Tiny Cactus Segment
	"MT_CACTISMALLSEG", // Small Cactus Segment
	"MT_ARIDSIGN_CAUTION", // Caution Sign
	"MT_ARIDSIGN_CACTI", // Cacti Sign
	"MT_ARIDSIGN_SHARPTURN", // Sharp Turn Sign
	"MT_OILLAMP",
	"MT_TNTBARREL",
	"MT_PROXIMITYTNT",
	"MT_DUSTDEVIL",
	"MT_DUSTLAYER",
	"MT_ARIDDUST",
	"MT_MINECART",
	"MT_MINECARTSEG",
	"MT_MINECARTSPAWNER",
	"MT_MINECARTEND",
	"MT_MINECARTENDSOLID",
	"MT_MINECARTSIDEMARK",
	"MT_MINECARTSPARK",
	"MT_SALOONDOOR",
	"MT_SALOONDOORCENTER",
	"MT_TRAINCAMEOSPAWNER",
	"MT_TRAINSEG",
	"MT_TRAINDUSTSPAWNER",
	"MT_TRAINSTEAMSPAWNER",
	"MT_MINECARTSWITCHPOINT",

	// Red Volcano Scenery
	"MT_FLAMEJET",
	"MT_VERTICALFLAMEJET",
	"MT_FLAMEJETFLAME",

	"MT_FJSPINAXISA", // Counter-clockwise
	"MT_FJSPINAXISB", // Clockwise

	"MT_FLAMEJETFLAMEB", // Blade's flame

	"MT_LAVAFALL",
	"MT_LAVAFALL_LAVA",
	"MT_LAVAFALLROCK",

	"MT_ROLLOUTSPAWN",
	"MT_ROLLOUTROCK",

	"MT_BIGFERNLEAF",
	"MT_BIGFERN",
	"MT_JUNGLEPALM",
	"MT_TORCHFLOWER",
	"MT_WALLVINE_LONG",
	"MT_WALLVINE_SHORT",

	// Dark City Scenery

	// Egg Rock Scenery

	// Azure Temple Scenery
	"MT_GLAREGOYLE",
	"MT_GLAREGOYLEUP",
	"MT_GLAREGOYLEDOWN",
	"MT_GLAREGOYLELONG",
	"MT_TARGET", // AKA Red Crystal
	"MT_GREENFLAME",
	"MT_BLUEGARGOYLE",

	// Stalagmites
	"MT_STALAGMITE0",
	"MT_STALAGMITE1",
	"MT_STALAGMITE2",
	"MT_STALAGMITE3",
	"MT_STALAGMITE4",
	"MT_STALAGMITE5",
	"MT_STALAGMITE6",
	"MT_STALAGMITE7",
	"MT_STALAGMITE8",
	"MT_STALAGMITE9",

	// Christmas Scenery
	"MT_XMASPOLE",
	"MT_CANDYCANE",
	"MT_SNOWMAN",    // normal
	"MT_SNOWMANHAT", // with hat + scarf
	"MT_LAMPPOST1",  // normal
	"MT_LAMPPOST2",  // with snow
	"MT_HANGSTAR",
	"MT_MISTLETOE",
	// Xmas GFZ bushes
	"MT_XMASBLUEBERRYBUSH",
	"MT_XMASBERRYBUSH",
	"MT_XMASBUSH",
	// FHZ
	"MT_FHZICE1",
	"MT_FHZICE2",
	"MT_ROSY",
	"MT_CDLHRT",

	// Halloween Scenery
	// Pumpkins
	"MT_JACKO1",
	"MT_JACKO2",
	"MT_JACKO3",
	// Dr Seuss Trees
	"MT_HHZTREE_TOP",
	"MT_HHZTREE_PART",
	// Misc
	"MT_HHZSHROOM",
	"MT_HHZGRASS",
	"MT_HHZTENTACLE1",
	"MT_HHZTENTACLE2",
	"MT_HHZSTALAGMITE_TALL",
	"MT_HHZSTALAGMITE_SHORT",

	// Botanic Serenity scenery
	"MT_BSZTALLFLOWER_RED",
	"MT_BSZTALLFLOWER_PURPLE",
	"MT_BSZTALLFLOWER_BLUE",
	"MT_BSZTALLFLOWER_CYAN",
	"MT_BSZTALLFLOWER_YELLOW",
	"MT_BSZTALLFLOWER_ORANGE",
	"MT_BSZFLOWER_RED",
	"MT_BSZFLOWER_PURPLE",
	"MT_BSZFLOWER_BLUE",
	"MT_BSZFLOWER_CYAN",
	"MT_BSZFLOWER_YELLOW",
	"MT_BSZFLOWER_ORANGE",
	"MT_BSZSHORTFLOWER_RED",
	"MT_BSZSHORTFLOWER_PURPLE",
	"MT_BSZSHORTFLOWER_BLUE",
	"MT_BSZSHORTFLOWER_CYAN",
	"MT_BSZSHORTFLOWER_YELLOW",
	"MT_BSZSHORTFLOWER_ORANGE",
	"MT_BSZTULIP_RED",
	"MT_BSZTULIP_PURPLE",
	"MT_BSZTULIP_BLUE",
	"MT_BSZTULIP_CYAN",
	"MT_BSZTULIP_YELLOW",
	"MT_BSZTULIP_ORANGE",
	"MT_BSZCLUSTER_RED",
	"MT_BSZCLUSTER_PURPLE",
	"MT_BSZCLUSTER_BLUE",
	"MT_BSZCLUSTER_CYAN",
	"MT_BSZCLUSTER_YELLOW",
	"MT_BSZCLUSTER_ORANGE",
	"MT_BSZBUSH_RED",
	"MT_BSZBUSH_PURPLE",
	"MT_BSZBUSH_BLUE",
	"MT_BSZBUSH_CYAN",
	"MT_BSZBUSH_YELLOW",
	"MT_BSZBUSH_ORANGE",
	"MT_BSZVINE_RED",
	"MT_BSZVINE_PURPLE",
	"MT_BSZVINE_BLUE",
	"MT_BSZVINE_CYAN",
	"MT_BSZVINE_YELLOW",
	"MT_BSZVINE_ORANGE",
	"MT_BSZSHRUB",
	"MT_BSZCLOVER",
	"MT_BIG_PALMTREE_TRUNK",
	"MT_BIG_PALMTREE_TOP",
	"MT_PALMTREE_TRUNK",
	"MT_PALMTREE_TOP",

	// Misc scenery
	"MT_DBALL",
	"MT_EGGSTATUE2",

	// Powerup Indicators
	"MT_ELEMENTAL_ORB", // Elemental shield mobj
	"MT_ATTRACT_ORB", // Attract shield mobj
	"MT_FORCE_ORB", // Force shield mobj
	"MT_ARMAGEDDON_ORB", // Armageddon shield mobj
	"MT_WHIRLWIND_ORB", // Whirlwind shield mobj
	"MT_PITY_ORB", // Pity shield mobj
	"MT_FLAMEAURA_ORB", // Flame shield mobj
	"MT_BUBBLEWRAP_ORB", // Bubble shield mobj
	"MT_THUNDERCOIN_ORB", // Thunder shield mobj
	"MT_THUNDERCOIN_SPARK", // Thunder spark
	"MT_IVSP", // Invincibility sparkles
	"MT_SUPERSPARK", // Super Sonic Spark

	// Flickies
	"MT_FLICKY_01", // Bluebird
	"MT_FLICKY_01_CENTER",
	"MT_FLICKY_02", // Rabbit
	"MT_FLICKY_02_CENTER",
	"MT_FLICKY_03", // Chicken
	"MT_FLICKY_03_CENTER",
	"MT_FLICKY_04", // Seal
	"MT_FLICKY_04_CENTER",
	"MT_FLICKY_05", // Pig
	"MT_FLICKY_05_CENTER",
	"MT_FLICKY_06", // Chipmunk
	"MT_FLICKY_06_CENTER",
	"MT_FLICKY_07", // Penguin
	"MT_FLICKY_07_CENTER",
	"MT_FLICKY_08", // Fish
	"MT_FLICKY_08_CENTER",
	"MT_FLICKY_09", // Ram
	"MT_FLICKY_09_CENTER",
	"MT_FLICKY_10", // Puffin
	"MT_FLICKY_10_CENTER",
	"MT_FLICKY_11", // Cow
	"MT_FLICKY_11_CENTER",
	"MT_FLICKY_12", // Rat
	"MT_FLICKY_12_CENTER",
	"MT_FLICKY_13", // Bear
	"MT_FLICKY_13_CENTER",
	"MT_FLICKY_14", // Dove
	"MT_FLICKY_14_CENTER",
	"MT_FLICKY_15", // Cat
	"MT_FLICKY_15_CENTER",
	"MT_FLICKY_16", // Canary
	"MT_FLICKY_16_CENTER",
	"MT_SECRETFLICKY_01", // Spider
	"MT_SECRETFLICKY_01_CENTER",
	"MT_SECRETFLICKY_02", // Bat
	"MT_SECRETFLICKY_02_CENTER",
	"MT_SEED",

	// Environmental Effects
	"MT_RAIN", // Rain
	"MT_SNOWFLAKE", // Snowflake
	"MT_BLIZZARDSNOW", // Blizzard Snowball
	"MT_SPLISH", // Water splish!
	"MT_LAVASPLISH", // Lava splish!
	"MT_SMOKE",
	"MT_SMALLBUBBLE", // small bubble
	"MT_MEDIUMBUBBLE", // medium bubble
	"MT_EXTRALARGEBUBBLE", // extra large bubble
	"MT_WATERZAP",
	"MT_SPINDUST", // Spindash dust
	"MT_TFOG",
	"MT_PARTICLE",
	"MT_PARTICLEGEN", // For fans, etc.

	// Game Indicators
	"MT_SCORE", // score logo
	"MT_DROWNNUMBERS", // Drowning Timer
	"MT_GOTEMERALD", // Chaos Emerald (intangible)
	"MT_LOCKON", // Target
	"MT_LOCKONINF", // In-level Target
	"MT_TAG", // Tag Sign
	"MT_GOTFLAG", // Got Flag sign
	"MT_FINISHFLAG", // Finish flag

	// Ambient Sounds
	"MT_AWATERA", // Ambient Water Sound 1
	"MT_AWATERB", // Ambient Water Sound 2
	"MT_AWATERC", // Ambient Water Sound 3
	"MT_AWATERD", // Ambient Water Sound 4
	"MT_AWATERE", // Ambient Water Sound 5
	"MT_AWATERF", // Ambient Water Sound 6
	"MT_AWATERG", // Ambient Water Sound 7
	"MT_AWATERH", // Ambient Water Sound 8
	"MT_RANDOMAMBIENT",
	"MT_RANDOMAMBIENT2",
	"MT_MACHINEAMBIENCE",

	"MT_CORK",
	"MT_LHRT",

	// Ring Weapons
	"MT_REDRING",
	"MT_BOUNCERING",
	"MT_RAILRING",
	"MT_INFINITYRING",
	"MT_AUTOMATICRING",
	"MT_EXPLOSIONRING",
	"MT_SCATTERRING",
	"MT_GRENADERING",

	"MT_BOUNCEPICKUP",
	"MT_RAILPICKUP",
	"MT_AUTOPICKUP",
	"MT_EXPLODEPICKUP",
	"MT_SCATTERPICKUP",
	"MT_GRENADEPICKUP",

	"MT_THROWNBOUNCE",
	"MT_THROWNINFINITY",
	"MT_THROWNAUTOMATIC",
	"MT_THROWNSCATTER",
	"MT_THROWNEXPLOSION",
	"MT_THROWNGRENADE",

	// Mario-specific stuff
	"MT_COIN",
	"MT_FLINGCOIN",
	"MT_GOOMBA",
	"MT_BLUEGOOMBA",
	"MT_FIREFLOWER",
	"MT_FIREBALL",
	"MT_FIREBALLTRAIL",
	"MT_SHELL",
	"MT_PUMA",
	"MT_PUMATRAIL",
	"MT_HAMMER",
	"MT_KOOPA",
	"MT_KOOPAFLAME",
	"MT_AXE",
	"MT_MARIOBUSH1",
	"MT_MARIOBUSH2",
	"MT_TOAD",

	// NiGHTS Stuff
	"MT_AXIS",
	"MT_AXISTRANSFER",
	"MT_AXISTRANSFERLINE",
	"MT_NIGHTSDRONE",
	"MT_NIGHTSDRONE_MAN",
	"MT_NIGHTSDRONE_SPARKLING",
	"MT_NIGHTSDRONE_GOAL",
	"MT_NIGHTSPARKLE",
	"MT_NIGHTSLOOPHELPER",
	"MT_NIGHTSBUMPER", // NiGHTS Bumper
	"MT_HOOP",
	"MT_HOOPCOLLIDE", // Collision detection for NiGHTS hoops
	"MT_HOOPCENTER", // Center of a hoop
	"MT_NIGHTSCORE",
	"MT_NIGHTSCHIP", // NiGHTS Chip
	"MT_FLINGNIGHTSCHIP", // Lost NiGHTS Chip
	"MT_NIGHTSSTAR", // NiGHTS Star
	"MT_FLINGNIGHTSSTAR", // Lost NiGHTS Star
	"MT_NIGHTSSUPERLOOP",
	"MT_NIGHTSDRILLREFILL",
	"MT_NIGHTSHELPER",
	"MT_NIGHTSEXTRATIME",
	"MT_NIGHTSLINKFREEZE",
	"MT_EGGCAPSULE",
	"MT_IDEYAANCHOR",
	"MT_NIGHTOPIANHELPER", // the actual helper object that orbits you
	"MT_PIAN", // decorative singing friend
	"MT_SHLEEP", // almost-decorative sleeping enemy

	// Secret badniks and hazards, shhhh
	"MT_PENGUINATOR",
	"MT_POPHAT",
	"MT_POPSHOT",
	"MT_POPSHOT_TRAIL",

	"MT_HIVEELEMENTAL",
	"MT_BUMBLEBORE",

	"MT_BUGGLE",

	"MT_SMASHINGSPIKEBALL",
	"MT_CACOLANTERN",
	"MT_CACOSHARD",
	"MT_CACOFIRE",
	"MT_SPINBOBERT",
	"MT_SPINBOBERT_FIRE1",
	"MT_SPINBOBERT_FIRE2",
	"MT_HANGSTER",

	// Utility Objects
	"MT_TELEPORTMAN",
	"MT_ALTVIEWMAN",
	"MT_CRUMBLEOBJ", // Sound generator for crumbling platform
	"MT_TUBEWAYPOINT",
	"MT_PUSH",
	"MT_PULL",
	"MT_GHOST",
	"MT_OVERLAY",
	"MT_ANGLEMAN",
	"MT_POLYANCHOR",
	"MT_POLYSPAWN",

	// Skybox objects
	"MT_SKYBOX",

	// Debris
	"MT_SPARK", //spark
	"MT_EXPLODE", // Robot Explosion
	"MT_UWEXPLODE", // Underwater Explosion
	"MT_DUST",
	"MT_ROCKSPAWNER",
	"MT_FALLINGROCK",
	"MT_ROCKCRUMBLE1",
	"MT_ROCKCRUMBLE2",
	"MT_ROCKCRUMBLE3",
	"MT_ROCKCRUMBLE4",
	"MT_ROCKCRUMBLE5",
	"MT_ROCKCRUMBLE6",
	"MT_ROCKCRUMBLE7",
	"MT_ROCKCRUMBLE8",
	"MT_ROCKCRUMBLE9",
	"MT_ROCKCRUMBLE10",
	"MT_ROCKCRUMBLE11",
	"MT_ROCKCRUMBLE12",
	"MT_ROCKCRUMBLE13",
	"MT_ROCKCRUMBLE14",
	"MT_ROCKCRUMBLE15",
	"MT_ROCKCRUMBLE16",

	// Level debris
	"MT_GFZDEBRIS",
	"MT_BRICKDEBRIS",
	"MT_WOODDEBRIS",
	"MT_REDBRICKDEBRIS",
	"MT_BLUEBRICKDEBRIS",
	"MT_YELLOWBRICKDEBRIS",

	// SRB2kart
	"MT_RANDOMITEM",
	"MT_SPHEREBOX",
	"MT_RANDOMITEMPOP",
	"MT_FLOATINGITEM",
	"MT_ITEMCAPSULE",
	"MT_ITEMCAPSULE_PART",

	"MT_SIGNSPARKLE",

	"MT_FASTLINE",
	"MT_FASTDUST",
	"MT_DRIFTEXPLODE",
	"MT_DRIFTCLIP",
	"MT_DRIFTCLIPSPARK",
	"MT_BOOSTFLAME",
	"MT_BOOSTSMOKE",
	"MT_SNEAKERTRAIL",
	"MT_AIZDRIFTSTRAT",
	"MT_SPARKLETRAIL",
	"MT_INVULNFLASH",
	"MT_WIPEOUTTRAIL",
	"MT_DRIFTSPARK",
	"MT_BRAKEDRIFT",
	"MT_BRAKEDUST",
	"MT_DRIFTDUST",
	"MT_DRIFTELECTRICITY",
	"MT_DRIFTELECTRICSPARK",
	"MT_JANKSPARK",

	"MT_ROCKETSNEAKER", // Rocket sneakers

	"MT_EGGMANITEM", // Eggman items
	"MT_EGGMANITEM_SHIELD",

	"MT_BANANA", // Banana Stuff
	"MT_BANANA_SHIELD",

	"MT_ORBINAUT", // Orbinaut stuff
	"MT_ORBINAUT_SHIELD",

	"MT_JAWZ", // Jawz stuff
	"MT_JAWZ_DUD",
	"MT_JAWZ_SHIELD",

	"MT_PLAYERRETICULE", // Jawz reticule

	"MT_SSMINE_SHIELD", // Special Stage Mine stuff
	"MT_SSMINE",

	"MT_SMOLDERING", // New explosion
	"MT_BOOMEXPLODE",
	"MT_BOOMPARTICLE",

	"MT_LANDMINE", // Land Mine

	"MT_DROPTARGET", // Drop Target
	"MT_DROPTARGET_SHIELD",

	"MT_BALLHOG", // Ballhog
	"MT_BALLHOGBOOM",

	"MT_SPB", // Self-Propelled Bomb
	"MT_SPBEXPLOSION",

	"MT_LIGHTNINGSHIELD", // Shields
	"MT_BUBBLESHIELD",
	"MT_FLAMESHIELD",
	"MT_FLAMESHIELDUNDERLAY",
	"MT_FLAMESHIELDPAPER",
	"MT_BUBBLESHIELDTRAP",

	"MT_HYUDORO",
	"MT_HYUDORO_CENTER",

	"MT_SINK", // Kitchen Sink Stuff
	"MT_SINK_SHIELD",
	"MT_SINKTRAIL",

	"MT_BATTLEBUMPER", // Battle Mode bumper
	"MT_BATTLEBUMPER_DEBRIS",
	"MT_BATTLEBUMPER_BLAST",

	"MT_TRIPWIREBOOST",

	"MT_DEZLASER",

	"MT_WAYPOINT",
	"MT_WAYPOINT_RISER",
	"MT_WAYPOINT_ANCHOR",

	"MT_BOTHINT",

	"MT_RANDOMAUDIENCE",

	"MT_FLAYM",
	"MT_DEVIL",
	"MT_ANGEL",
	"MT_PALMTREE",
	"MT_FLAG",
	"MT_HEDGEHOG",
	"MT_BUSH1",
	"MT_TWEE",
	"MT_HYDRANT",

	"MT_BIGPUMA",
	"MT_APPLE",

	"MT_DOOD_FLOWER1",
	"MT_DOOD_FLOWER2",
	"MT_DOOD_FLOWER3",
	"MT_DOOD_FLOWER4",
	"MT_DOOD_BOX",
	"MT_DOOD_BALLOON",
	"MT_BIGRING",

	"MT_SNES_DONUTBUSH1",
	"MT_SNES_DONUTBUSH2",
	"MT_SNES_DONUTBUSH3",

	"MT_GBA_BOO",

	"MT_BUZZBOMBER",
	"MT_CHOMPER",
	"MT_PALMTREE2",
	"MT_PURPLEFLOWER1",
	"MT_PURPLEFLOWER2",
	"MT_YELLOWFLOWER1",
	"MT_YELLOWFLOWER2",
	"MT_PLANT2",
	"MT_PLANT3",
	"MT_PLANT4",

	"MT_SKULL",
	"MT_PHANTREE",
	"MT_FLYINGGARG",
	"MT_LAMPPOST",
	"MT_MOSSYTREE",

	"MT_BUMP",

	"MT_FLINGENERGY",

	"MT_ITEMCLASH",

	"MT_FIREDITEM",

	"MT_INSTASHIELDA",
	"MT_INSTASHIELDB",

	"MT_POWERCLASH", // Invinc/Grow no damage clash VFX

	"MT_PLAYERARROW",
	"MT_PLAYERWANTED",

	"MT_KARMAHITBOX",
	"MT_KARMAWHEEL",

	"MT_BATTLEPOINT",

	"MT_FZEROBOOM",

	// Various plants
	"MT_SONICBUSH",

	// Marble Zone
	"MT_MARBLEFLAMEPARTICLE",
	"MT_MARBLETORCH",
	"MT_MARBLELIGHT",
	"MT_MARBLEBURNER",

	// CD Special Stage
	"MT_CDUFO",

	// Rusty Rig
	"MT_RUSTYLAMP_ORANGE",
	"MT_RUSTYCHAIN",

	// Smokin' & Vapin' (Don't try this at home, kids!)
	"MT_PETSMOKER",
	"MT_PETSMOKE",
	"MT_VVVAPE",

	// Hill Top Zone
	"MT_HTZTREE",
	"MT_HTZBUSH",

	// Ports of gardens
	"MT_SGVINE1",
	"MT_SGVINE2",
	"MT_SGVINE3",
	"MT_PGTREE",
	"MT_PGFLOWER1",
	"MT_PGFLOWER2",
	"MT_PGFLOWER3",
	"MT_PGBUSH",
	"MT_DHPILLAR",

	// Midnight Channel stuff:
	"MT_SPOTLIGHT",		// Spotlight Object
	"MT_RANDOMSHADOW",	// Random static Shadows.
	"MT_ROAMINGSHADOW",	// Roaming Shadows.
	"MT_MAYONAKAARROW",	// Arrow static signs for Mayonaka

	// Mementos stuff
	"MT_REAPERWAYPOINT",
	"MT_REAPER",
	"MT_MEMENTOSTP",
	"MT_MEMENTOSPARTICLE",

	"MT_JACKINTHEBOX",

	// Color Drive:
	"MT_CDMOON",
	"MT_CDBUSH",
	"MT_CDTREEA",
	"MT_CDTREEB",

	// Daytona Speedway
	"MT_DAYTONAPINETREE",
	"MT_DAYTONAPINETREE_SIDE",

	// Egg Zeppelin
	"MT_EZZPROPELLER",
	"MT_EZZPROPELLER_BLADE",

	// Desert Palace
	"MT_DP_PALMTREE",

	// Aurora Atoll
	"MT_AAZTREE_HELPER",
	"MT_AAZTREE_SEG",
	"MT_AAZTREE_COCONUT",
	"MT_AAZTREE_LEAF",

	// Barren Badlands
	"MT_BBZDUST",
	"MT_FROGGER",
	"MT_FROGTONGUE",
	"MT_FROGTONGUE_JOINT",
	"MT_ROBRA",
	"MT_ROBRA_HEAD",
	"MT_ROBRA_JOINT",
	"MT_BLUEROBRA",
	"MT_BLUEROBRA_HEAD",
	"MT_BLUEROBRA_JOINT",

	// Eerie Grove
	"MT_EERIEFOG",
	"MT_EERIEFOGGEN",

	// SMK ports
	"MT_SMK_PIPE",
	"MT_SMK_MOLESPAWNER",
	"MT_SMK_MOLE",
	"MT_SMK_THWOMP",
	"MT_SMK_SNOWBALL",
	"MT_SMK_ICEBLOCK",
	"MT_SMK_ICEBLOCK_SIDE",
	"MT_SMK_ICEBLOCK_DEBRIS",

	// Ezo's maps
	"MT_BLUEFIRE",
	"MT_GREENFIRE",
	"MT_REGALCHEST",
	"MT_CHIMERASTATUE",
	"MT_DRAGONSTATUE",
	"MT_LIZARDMANSTATUE",
	"MT_PEGASUSSTATUE",
	"MT_ZELDAFIRE",
	"MT_GANBARETHING",
	"MT_GANBAREDUCK",
	"MT_GANBARETREE",
	"MT_MONOKUMA",
	"MT_REDZELDAFIRE",
	"MT_BOWLINGPIN",
	"MT_MERRYAMBIENCE",
	"MT_TWINKLECARTAMBIENCE",
	"MT_EXPLODINGBARREL",
	"MT_MERRYHORSE",
	"MT_BLUEFRUIT",
	"MT_ORANGEFRUIT",
	"MT_REDFRUIT",
	"MT_PINKFRUIT",
	"MT_ADVENTURESPIKEA",
	"MT_ADVENTURESPIKEB",
	"MT_ADVENTURESPIKEC",
	"MT_BOOSTPROMPT",
	"MT_BOOSTOFF",
	"MT_BOOSTON",
	"MT_ARIDTOAD",
	"MT_LIZARDMAN",
	"MT_LIONMAN",

	"MT_KARMAFIREWORK",
	"MT_RINGSPARKS",
	"MT_GAINAX",
	"MT_DRAFTDUST",
	"MT_SPBDUST",
	"MT_TIREGREASE",

	"MT_OVERTIME_PARTICLE",
	"MT_OVERTIME_CENTER",

	"MT_BATTLECAPSULE",
	"MT_BATTLECAPSULE_PIECE",

	"MT_FOLLOWER",
	"MT_FOLLOWERBUBBLE_FRONT",
	"MT_FOLLOWERBUBBLE_BACK",

	"MT_WATERTRAIL",
	"MT_WATERTRAILUNDERLAY",

	"MT_SPINDASHDUST",
	"MT_SPINDASHWIND",
	"MT_SOFTLANDING",
	"MT_DOWNLINE",
	"MT_HOLDBUBBLE",

	"MT_PAPERITEMSPOT",

	"MT_BEAMPOINT",
};

const char *const MOBJFLAG_LIST[] = {
	"SPECIAL",
	"SOLID",
	"SHOOTABLE",
	"NOSECTOR",
	"NOBLOCKMAP",
	"PAPERCOLLISION",
	"PUSHABLE",
	"BOSS",
	"SPAWNCEILING",
	"NOGRAVITY",
	"AMBIENT",
	"SLIDEME",
	"NOCLIP",
	"FLOAT",
	"BOXICON",
	"MISSILE",
	"SPRING",
	"MONITOR",
	"NOTHINK",
	"NOCLIPHEIGHT",
	"ENEMY",
	"SCENERY",
	"PAIN",
	"STICKY",
	"NIGHTSITEM",
	"NOCLIPTHING",
	"GRENADEBOUNCE",
	"RUNSPAWNFUNC",
	"DONTENCOREMAP",
	"PICKUPFROMBELOW",
	"NOSQUISH",
	"NOHITLAGFORME",
	NULL
};

// \tMF2_(\S+).*// (.+) --> \t"\1", // \2
const char *const MOBJFLAG2_LIST[] = {
	"AXIS",			  // It's a NiGHTS axis! (For faster checking)
	"TWOD",			  // Moves like it's in a 2D level
	"DONTRESPAWN",	  // Don't respawn this object!
	"\x01",			  // free: 1<<3 (name un-matchable)
	"AUTOMATIC",	  // Thrown ring has automatic properties
	"RAILRING",		  // Thrown ring has rail properties
	"BOUNCERING",	  // Thrown ring has bounce properties
	"EXPLOSION",	  // Thrown ring has explosive properties
	"SCATTER",		  // Thrown ring has scatter properties
	"BEYONDTHEGRAVE", // Source of this missile has died and has since respawned.
	"SLIDEPUSH",	  // MF_PUSHABLE that pushes continuously.
	"CLASSICPUSH",    // Drops straight down when object has negative momz.
	"INVERTAIMABLE",  // Flips whether it's targetable by A_LookForEnemies (enemies no, decoys yes)
	"INFLOAT",		  // Floating to a height for a move, don't auto float to target's height.
	"DEBRIS",		  // Splash ring from explosion ring
	"NIGHTSPULL",	  // Attracted from a paraloop
	"JUSTATTACKED",	  // can be pushed by other moving mobjs
	"FIRING",		  // turret fire
	"SUPERFIRE",	  // Firing something with Super Sonic-stopping properties. Or, if mobj has MF_MISSILE, this is the actual fire from it.
	"\x01",			  // free: 1<<20 (name un-matchable)
	"STRONGBOX",	  // Flag used for "strong" random monitors.
	"OBJECTFLIP",	  // Flag for objects that always have flipped gravity.
	"SKULLFLY",		  // Special handling: skull in flight.
	"FRET",			  // Flashing from a previous hit
	"BOSSNOTRAP",	  // No Egg Trap after boss
	"BOSSFLEE",		  // Boss is fleeing!
	"BOSSDEAD",		  // Boss is dead! (Not necessarily fleeing, if a fleeing point doesn't exist.)
	"AMBUSH",         // Alternate behaviour typically set by MTF_AMBUSH
	"LINKDRAW",       // Draw vissprite of mobj immediately before/after tracer's vissprite (dependent on dispoffset and position)
	"SHIELD",         // Thinker calls P_AddShield/P_ShieldLook (must be partnered with MF_SCENERY to use)
	NULL
};

const char *const MOBJEFLAG_LIST[] = {
	"ONGROUND", // The mobj stands on solid floor (not on another mobj or in air)
	"JUSTHITFLOOR", // The mobj just hit the floor while falling, this is cleared on next frame
	"TOUCHWATER", // The mobj stands in a sector with water, and touches the surface
	"UNDERWATER", // The mobj stands in a sector with water, and his waist is BELOW the water surface
	"JUSTSTEPPEDDOWN", // used for ramp sectors
	"VERTICALFLIP", // Vertically flip sprite/allow upside-down physics
	"GOOWATER", // Goo water
	"TOUCHLAVA", // The mobj is touching a lava block
	"PUSHED", // Mobj was already pushed this tic
	"SPRUNG", // Mobj was already sprung this tic
	"APPLYPMOMZ", // Platform movement
	"TRACERANGLE", // Compute and trigger on mobj angle relative to tracer
	"JUSTBOUNCEDWALL",
	"DAMAGEHITLAG",
	"SLOPELAUNCHED",
	NULL
};

const char *const MAPTHINGFLAG_LIST[4] = {
	"EXTRA", // Extra flag for objects.
	"OBJECTFLIP", // Reverse gravity flag for objects.
	"OBJECTSPECIAL", // Special flag used with certain objects.
	"AMBUSH" // Deaf monsters/do not react to sound.
};

const char *const PLAYERFLAG_LIST[] = {
	// True if button down last tic.
	"ATTACKDOWN",
	"ACCELDOWN",
	"BRAKEDOWN",
	"LOOKDOWN",

	// Accessibility and cheats
	"KICKSTARTACCEL", // Is accelerate in kickstart mode?
	"GODMODE",
	"NOCLIP",

	"WANTSTOJOIN", // Spectator that wants to join

	"STASIS", // Player is not allowed to move
	"FAULT", // F A U L T
	"ELIMINATED", // Battle-style elimination, no extra penalty
	"NOCONTEST", // Did not finish (last place explosion)
	"LOSTLIFE", // Do not lose life more than once

	"RINGLOCK", // Prevent picking up rings while SPB is locked on

	// The following four flags are mutually exclusive, although they can also all be off at the same time. If we ever run out of pflags, eventually turn them into a seperate five(+) mode UINT8..?
	"USERINGS", // Have to be not holding the item button to change from using rings to using items (or vice versa) - prevents weirdness
	"ITEMOUT", // Are you holding an item out?
	"EGGMANOUT", // Eggman mark held, separate from PF_ITEMOUT so it doesn't stop you from getting items
	"HOLDREADY", // Hold button-style item is ready to activate

	"DRIFTINPUT", // Drifting!
	"GETSPARKS", // Can get sparks
	"DRIFTEND", // Drift has ended, used to adjust character angle after drift
	"BRAKEDRIFT", // Helper for brake-drift spark spawning

	"AIRFAILSAFE", // Whenever or not try the air boost
	"TRICKDELAY", // Prevent tricks until control stick is neutral

	"TUMBLELASTBOUNCE", // One more time for the funny
	"TUMBLESOUND", // Don't play more than once

	"HITFINISHLINE", // Already hit the finish line this tic
	"WRONGWAY", // Moving the wrong way with respect to waypoints?

	"SHRINKME",
	"SHRINKACTIVE",

	NULL // stop loop here.
};

const char *const GAMETYPERULE_LIST[] = {
	"CAMPAIGN",
	"RINGSLINGER",
	"SPECTATORS",
	"LIVES",
	"TEAMS",
	"FIRSTPERSON",
	"POWERSTONES",
	"TEAMFLAGS",
	"FRIENDLY",
	"SPECIALSTAGES",
	"EMERALDTOKENS",
	"EMERALDHUNT",
	"RACE",
	"TAG",
	"POINTLIMIT",
	"TIMELIMIT",
	"OVERTIME",
	"HURTMESSAGES",
	"FRIENDLYFIRE",
	"STARTCOUNTDOWN",
	"HIDEFROZEN",
	"BLINDFOLDED",
	"RESPAWNDELAY",
	"PITYSHIELD",
	"DEATHPENALTY",
	"NOSPECTATORSPAWN",
	"DEATHMATCHSTARTS",
	"SPAWNINVUL",
	"SPAWNENEMIES",
	"ALLOWEXIT",
	"NOTITLECARD",
	"CUTSCENES",
	NULL
};

// Linedef flags
const char *const ML_LIST[16] = {
	"IMPASSABLE",
	"BLOCKPLAYERS",
	"TWOSIDED",
	"DONTPEGTOP",
	"DONTPEGBOTTOM",
	"EFFECT1",
	"NOCLIMB",
	"EFFECT2",
	"EFFECT3",
	"EFFECT4",
	"EFFECT5",
	"NOSONIC",
	"NOTAILS",
	"NOKNUX",
	"BOUNCY",
	"TFERLINE"
};

const char *COLOR_ENUMS[] = {
	"NONE",			// SKINCOLOR_NONE
	"WHITE",		// SKINCOLOR_WHITE
	"SILVER",		// SKINCOLOR_SILVER
	"GREY",			// SKINCOLOR_GREY
	"NICKEL",		// SKINCOLOR_NICKEL
	"BLACK",		// SKINCOLOR_BLACK
	"SKUNK",		// SKINCOLOR_SKUNK
	"FAIRY",		// SKINCOLOR_FAIRY
	"POPCORN",		// SKINCOLOR_POPCORN
	"ARTICHOKE",	// SKINCOLOR_ARTICHOKE
	"PIGEON",		// SKINCOLOR_PIGEON
	"SEPIA",		// SKINCOLOR_SEPIA
	"BEIGE",		// SKINCOLOR_BEIGE
	"CARAMEL",		// SKINCOLOR_CARAMEL
	"PEACH",		// SKINCOLOR_PEACH
	"BROWN",		// SKINCOLOR_BROWN
	"LEATHER",		// SKINCOLOR_LEATHER
	"PINK",			// SKINCOLOR_PINK
	"ROSE",			// SKINCOLOR_ROSE
	"CINNAMON",		// SKINCOLOR_CINNAMON
	"RUBY",			// SKINCOLOR_RUBY
	"RASPBERRY",	// SKINCOLOR_RASPBERRY
	"RED",			// SKINCOLOR_RED
	"CRIMSON",		// SKINCOLOR_CRIMSON
	"MAROON",		// SKINCOLOR_MAROON
	"LEMONADE",		// SKINCOLOR_LEMONADE
	"SCARLET",		// SKINCOLOR_SCARLET
	"KETCHUP",		// SKINCOLOR_KETCHUP
	"DAWN",			// SKINCOLOR_DAWN
	"SUNSLAM",		// SKINCOLOR_SUNSLAM
	"CREAMSICLE",	// SKINCOLOR_CREAMSICLE
	"ORANGE",		// SKINCOLOR_ORANGE
	"ROSEWOOD",		// SKINCOLOR_ROSEWOOD
	"TANGERINE",	// SKINCOLOR_TANGERINE
	"TAN",			// SKINCOLOR_TAN
	"CREAM",		// SKINCOLOR_CREAM
	"GOLD",			// SKINCOLOR_GOLD
	"ROYAL",		// SKINCOLOR_ROYAL
	"BRONZE",		// SKINCOLOR_BRONZE
	"COPPER",		// SKINCOLOR_COPPER
	"YELLOW",		// SKINCOLOR_YELLOW
	"MUSTARD",		// SKINCOLOR_MUSTARD
	"BANANA",		// SKINCOLOR_BANANA
	"OLIVE",		// SKINCOLOR_OLIVE
	"CROCODILE",	// SKINCOLOR_CROCODILE
	"PERIDOT",		// SKINCOLOR_PERIDOT
	"VOMIT",		// SKINCOLOR_VOMIT
	"GARDEN",		// SKINCOLOR_GARDEN
	"LIME",			// SKINCOLOR_LIME
	"HANDHELD",		// SKINCOLOR_HANDHELD
	"TEA",			// SKINCOLOR_TEA
	"PISTACHIO",	// SKINCOLOR_PISTACHIO
	"MOSS",			// SKINCOLOR_MOSS
	"CAMOUFLAGE",	// SKINCOLOR_CAMOUFLAGE
	"MINT",			// SKINCOLOR_MINT
	"GREEN",		// SKINCOLOR_GREEN
	"PINETREE",		// SKINCOLOR_PINETREE
	"TURTLE",		// SKINCOLOR_TURTLE
	"SWAMP",		// SKINCOLOR_SWAMP
	"DREAM",		// SKINCOLOR_DREAM
	"PLAGUE",		// SKINCOLOR_PLAGUE
	"EMERALD",		// SKINCOLOR_EMERALD
	"ALGAE",		// SKINCOLOR_ALGAE
	"AQUAMARINE",	// SKINCOLOR_AQUAMARINE
	"TURQUOISE",	// SKINCOLOR_TURQUOISE
	"TEAL",			// SKINCOLOR_TEAL
	"ROBIN",		// SKINCOLOR_ROBIN
	"CYAN",			// SKINCOLOR_CYAN
	"JAWZ",			// SKINCOLOR_JAWZ
	"CERULEAN",		// SKINCOLOR_CERULEAN
	"NAVY",			// SKINCOLOR_NAVY
	"PLATINUM",		// SKINCOLOR_PLATINUM
	"SLATE",		// SKINCOLOR_SLATE
	"STEEL",		// SKINCOLOR_STEEL
	"THUNDER",		// SKINCOLOR_THUNDER
	"NOVA",			// SKINCOLOR_NOVA
	"RUST",			// SKINCOLOR_RUST
	"WRISTWATCH",	// SKINCOLOR_WRISTWATCH
	"JET",			// SKINCOLOR_JET
	"SAPPHIRE",		// SKINCOLOR_SAPPHIRE
	"ULTRAMARINE",	// SKINCOLOR_ULTRAMARINE
	"PERIWINKLE",	// SKINCOLOR_PERIWINKLE
	"BLUE",			// SKINCOLOR_BLUE
	"MIDNIGHT",		// SKINCOLOR_MIDNIGHT
	"BLUEBERRY",	// SKINCOLOR_BLUEBERRY
	"THISTLE",		// SKINCOLOR_THISTLE
	"PURPLE",		// SKINCOLOR_PURPLE
	"PASTEL",		// SKINCOLOR_PASTEL
	"MOONSET",		// SKINCOLOR_MOONSET
	"DUSK",			// SKINCOLOR_DUSK
	"VIOLET",		// SKINCOLOR_VIOLET
	"MAGENTA",		// SKINCOLOR_MAGENTA
	"FUCHSIA",		// SKINCOLOR_FUCHSIA
	"TOXIC",		// SKINCOLOR_TOXIC
	"MAUVE",		// SKINCOLOR_MAUVE
	"LAVENDER",		// SKINCOLOR_LAVENDER
	"BYZANTIUM",	// SKINCOLOR_BYZANTIUM
	"POMEGRANATE",	// SKINCOLOR_POMEGRANATE
	"LILAC",		// SKINCOLOR_LILAC
	"BLOSSOM",		// SKINCOLOR_BLOSSOM
	"TAFFY",		// SKINCOLOR_TAFFY

	// Special super colors
	// Super Sonic Yellow
	"SUPERSILVER1",
	"SUPERSILVER2",
	"SUPERSILVER3",
	"SUPERSILVER4",
	"SUPERSILVER5",

	"SUPERRED1",
	"SUPERRED2",
	"SUPERRED3",
	"SUPERRED4",
	"SUPERRED5",

	"SUPERORANGE1",
	"SUPERORANGE2",
	"SUPERORANGE3",
	"SUPERORANGE4",
	"SUPERORANGE5",

	"SUPERGOLD1",
	"SUPERGOLD2",
	"SUPERGOLD3",
	"SUPERGOLD4",
	"SUPERGOLD5",

	"SUPERPERIDOT1",
	"SUPERPERIDOT2",
	"SUPERPERIDOT3",
	"SUPERPERIDOT4",
	"SUPERPERIDOT5",

	"SUPERSKY1",
	"SUPERSKY2",
	"SUPERSKY3",
	"SUPERSKY4",
	"SUPERSKY5",

	"SUPERPURPLE1",
	"SUPERPURPLE2",
	"SUPERPURPLE3",
	"SUPERPURPLE4",
	"SUPERPURPLE5",

	"SUPERRUST1",
	"SUPERRUST2",
	"SUPERRUST3",
	"SUPERRUST4",
	"SUPERRUST5",

	"SUPERTAN1",
	"SUPERTAN2",
	"SUPERTAN3",
	"SUPERTAN4",
	"SUPERTAN5",

	"CHAOSEMERALD1",
	"CHAOSEMERALD2",
	"CHAOSEMERALD3",
	"CHAOSEMERALD4",
	"CHAOSEMERALD5",
	"CHAOSEMERALD6",
	"CHAOSEMERALD7",

	"INVINCFLASH"
};

const char *const KARTHUD_LIST[] = {
	"ITEMBLINK",
	"ITEMBLINKMODE",

	"RINGFRAME",
	"RINGTICS",
	"RINGDELAY",
	"RINGSPBBLOCK",

	"LAPANIMATION",
	"LAPHAND",

	"FAULT",

	"BOOSTCAM",
	"DESTBOOSTCAM",
	"TIMEOVERCAM",

	"ENGINESND",
	"VOICES",
	"TAUNTVOICES",

	"CARDANIMATION",
	"YOUGOTEM",
};

const char *const HUDITEMS_LIST[] = {
	"LIVES",

	"RINGS",
	"RINGSNUM",
	"RINGSNUMTICS",

	"SCORE",
	"SCORENUM",

	"TIME",
	"MINUTES",
	"TIMECOLON",
	"SECONDS",
	"TIMETICCOLON",
	"TICS",

	"SS_TOTALRINGS",

	"GETRINGS",
	"GETRINGSNUM",
	"TIMELEFT",
	"TIMELEFTNUM",
	"TIMEUP",
	"HUNTPICS",
	"POWERUPS"
};

const char *const MENUTYPES_LIST[] = {
	"NONE",

	"MAIN",

	// Single Player
	"SP_MAIN",

	"SP_LOAD",
	"SP_PLAYER",

	"SP_LEVELSELECT",
	"SP_LEVELSTATS",

	"SP_TIMEATTACK",
	"SP_TIMEATTACK_LEVELSELECT",
	"SP_GUESTREPLAY",
	"SP_REPLAY",
	"SP_GHOST",

	"SP_NIGHTSATTACK",
	"SP_NIGHTS_LEVELSELECT",
	"SP_NIGHTS_GUESTREPLAY",
	"SP_NIGHTS_REPLAY",
	"SP_NIGHTS_GHOST",

	// Multiplayer
	"MP_MAIN",
	"MP_SPLITSCREEN", // SplitServer
	"MP_SERVER",
	"MP_CONNECT",
	"MP_ROOM",
	"MP_PLAYERSETUP", // MP_PlayerSetupDef shared with SPLITSCREEN if #defined NONET
	"MP_SERVER_OPTIONS",

	// Options
	"OP_MAIN",

	"OP_P1CONTROLS",
	"OP_CHANGECONTROLS", // OP_ChangeControlsDef shared with P2
	"OP_P1MOUSE",
	"OP_P1JOYSTICK",
	"OP_JOYSTICKSET", // OP_JoystickSetDef shared with P2
	"OP_P1CAMERA",

	"OP_P2CONTROLS",
	"OP_P2MOUSE",
	"OP_P2JOYSTICK",
	"OP_P2CAMERA",

	"OP_PLAYSTYLE",

	"OP_VIDEO",
	"OP_VIDEOMODE",
	"OP_COLOR",
	"OP_OPENGL",
	"OP_OPENGL_LIGHTING",

	"OP_SOUND",

	"OP_SERVER",
	"OP_MONITORTOGGLE",

	"OP_DATA",
	"OP_ADDONS",
	"OP_SCREENSHOTS",
	"OP_ERASEDATA",

	// Extras
	"SR_MAIN",
	"SR_PANDORA",
	"SR_LEVELSELECT",
	"SR_UNLOCKCHECKLIST",
	"SR_EMBLEMHINT",
	"SR_PLAYER",
	"SR_SOUNDTEST",

	// Addons (Part of MISC, but let's make it our own)
	"AD_MAIN",

	// MISC
	// "MESSAGE",
	// "SPAUSE",

	// "MPAUSE",
	// "SCRAMBLETEAM",
	// "CHANGETEAM",
	// "CHANGELEVEL",

	// "MAPAUSE",
	// "HELP",

	"SPECIAL"
};

struct int_const_s const INT_CONST[] = {
	// If a mod removes some variables here,
	// please leave the names in-tact and just set
	// the value to 0 or something.

	// integer type limits, from doomtype.h
	// INT64 and UINT64 limits not included, they're too big for most purposes anyway
	// signed
	{"INT8_MIN",INT8_MIN},
	{"INT16_MIN",INT16_MIN},
	{"INT32_MIN",INT32_MIN},
	{"INT8_MAX",INT8_MAX},
	{"INT16_MAX",INT16_MAX},
	{"INT32_MAX",INT32_MAX},
	// unsigned
	{"UINT8_MAX",UINT8_MAX},
	{"UINT16_MAX",UINT16_MAX},
	{"UINT32_MAX",UINT32_MAX},

	// fixed_t constants, from m_fixed.h
	{"FRACUNIT",FRACUNIT},
	{"FU"      ,FRACUNIT},
	{"FRACBITS",FRACBITS},
	{"M_TAU_FIXED",M_TAU_FIXED},

	// doomdef.h constants
	{"TICRATE",TICRATE},
	{"MUSICRATE",MUSICRATE},
	{"RING_DIST",RING_DIST},
	{"PUSHACCEL",PUSHACCEL},
	{"MODID",MODID}, // I don't know, I just thought it would be cool for a wad to potentially know what mod it was loaded into.
	{"MODVERSION",MODVERSION}, // or what version of the mod this is.
	{"CODEBASE",CODEBASE}, // or what release of SRB2 this is.
	{"NEWTICRATE",NEWTICRATE}, // TICRATE*NEWTICRATERATIO
	{"NEWTICRATERATIO",NEWTICRATERATIO},

	// Special linedef executor tag numbers!
	{"LE_PINCHPHASE",LE_PINCHPHASE}, // A boss entered pinch phase (and, in most cases, is preparing their pinch phase attack!)
	{"LE_ALLBOSSESDEAD",LE_ALLBOSSESDEAD}, // All bosses in the map are dead (Egg capsule raise)
	{"LE_BOSSDEAD",LE_BOSSDEAD}, // A boss in the map died (Chaos mode boss tally)
	{"LE_BOSS4DROP",LE_BOSS4DROP}, // CEZ boss dropped its cage
	{"LE_BRAKVILEATACK",LE_BRAKVILEATACK}, // Brak's doing his LOS attack, oh noes
	{"LE_TURRET",LE_TURRET}, // THZ turret
	{"LE_BRAKPLATFORM",LE_BRAKPLATFORM}, // v2.0 Black Eggman destroys platform
	{"LE_CAPSULE2",LE_CAPSULE2}, // Egg Capsule
	{"LE_CAPSULE1",LE_CAPSULE1}, // Egg Capsule
	{"LE_CAPSULE0",LE_CAPSULE0}, // Egg Capsule
	{"LE_KOOPA",LE_KOOPA}, // Distant cousin to Gay Bowser
	{"LE_AXE",LE_AXE}, // MKB Axe object
	{"LE_PARAMWIDTH",LE_PARAMWIDTH},  // If an object that calls LinedefExecute has a nonzero parameter value, this times the parameter will be subtracted. (Mostly for the purpose of coexisting bosses...)

	/// \todo Get all this stuff into its own sections, maybe. Maybe.

	// Frame settings
	{"FF_FRAMEMASK",FF_FRAMEMASK},
	{"FF_SPR2SUPER",FF_SPR2SUPER},
	{"FF_SPR2ENDSTATE",FF_SPR2ENDSTATE},
	{"FF_SPR2MIDSTART",FF_SPR2MIDSTART},
	{"FF_ANIMATE",FF_ANIMATE},
	{"FF_RANDOMANIM",FF_RANDOMANIM},
	{"FF_GLOBALANIM",FF_GLOBALANIM},
	{"FF_FULLBRIGHT",FF_FULLBRIGHT},
	{"FF_FULLDARK",FF_FULLDARK},
	{"FF_SEMIBRIGHT",FF_SEMIBRIGHT},
	{"FF_VERTICALFLIP",FF_VERTICALFLIP},
	{"FF_HORIZONTALFLIP",FF_HORIZONTALFLIP},
	{"FF_PAPERSPRITE",FF_PAPERSPRITE},
	{"FF_FLOORSPRITE",FF_FLOORSPRITE},
	{"FF_BLENDMASK",FF_BLENDMASK},
	{"FF_BLENDSHIFT",FF_BLENDSHIFT},
	{"FF_ADD",FF_ADD},
	{"FF_SUBTRACT",FF_SUBTRACT},
	{"FF_REVERSESUBTRACT",FF_REVERSESUBTRACT},
	{"FF_MODULATE",FF_MODULATE},
	{"FF_OVERLAY",FF_OVERLAY},
	{"FF_TRANSMASK",FF_TRANSMASK},
	{"FF_TRANSSHIFT",FF_TRANSSHIFT},
	// new preshifted translucency (used in source)
	{"FF_TRANS10",FF_TRANS10},
	{"FF_TRANS20",FF_TRANS20},
	{"FF_TRANS30",FF_TRANS30},
	{"FF_TRANS40",FF_TRANS40},
	{"FF_TRANS50",FF_TRANS50},
	{"FF_TRANS60",FF_TRANS60},
	{"FF_TRANS70",FF_TRANS70},
	{"FF_TRANS80",FF_TRANS80},
	{"FF_TRANS90",FF_TRANS90},
	// temporary, for testing
	{"FF_TRANSADD",FF_ADD},
	{"FF_TRANSSUB",FF_SUBTRACT},
	// compatibility
	// Transparency for SOCs is pre-shifted
	{"TR_TRANS10",tr_trans10<<FF_TRANSSHIFT},
	{"TR_TRANS20",tr_trans20<<FF_TRANSSHIFT},
	{"TR_TRANS30",tr_trans30<<FF_TRANSSHIFT},
	{"TR_TRANS40",tr_trans40<<FF_TRANSSHIFT},
	{"TR_TRANS50",tr_trans50<<FF_TRANSSHIFT},
	{"TR_TRANS60",tr_trans60<<FF_TRANSSHIFT},
	{"TR_TRANS70",tr_trans70<<FF_TRANSSHIFT},
	{"TR_TRANS80",tr_trans80<<FF_TRANSSHIFT},
	{"TR_TRANS90",tr_trans90<<FF_TRANSSHIFT},
	// Transparency for Lua is not, unless capitalized as above.
	{"tr_trans10",tr_trans10},
	{"tr_trans20",tr_trans20},
	{"tr_trans30",tr_trans30},
	{"tr_trans40",tr_trans40},
	{"tr_trans50",tr_trans50},
	{"tr_trans60",tr_trans60},
	{"tr_trans70",tr_trans70},
	{"tr_trans80",tr_trans80},
	{"tr_trans90",tr_trans90},
	{"NUMTRANSMAPS",NUMTRANSMAPS},

	// Alpha styles (blend modes)
	//{"AST_COPY",AST_COPY}, -- not useful in lua
	//{"AST_TRANSLUCENT",AST_TRANSLUCENT}, -- ditto
	{"AST_ADD",AST_ADD},
	{"AST_SUBTRACT",AST_SUBTRACT},
	{"AST_REVERSESUBTRACT",AST_REVERSESUBTRACT},
	{"AST_MODULATE",AST_MODULATE},
	{"AST_OVERLAY",AST_OVERLAY},

	// Render flags
	{"RF_HORIZONTALFLIP",RF_HORIZONTALFLIP},
	{"RF_VERTICALFLIP",RF_VERTICALFLIP},
	{"RF_ABSOLUTEOFFSETS",RF_ABSOLUTEOFFSETS},
	{"RF_FLIPOFFSETS",RF_FLIPOFFSETS},
	{"RF_SPLATMASK",RF_SPLATMASK},
	{"RF_SLOPESPLAT",RF_SLOPESPLAT},
	{"RF_OBJECTSLOPESPLAT",RF_OBJECTSLOPESPLAT},
	{"RF_NOSPLATBILLBOARD",RF_NOSPLATBILLBOARD},
	{"RF_NOSPLATROLLANGLE",RF_NOSPLATROLLANGLE},
	{"RF_BRIGHTMASK",RF_BRIGHTMASK},
	{"RF_FULLBRIGHT",RF_FULLBRIGHT},
	{"RF_FULLDARK",RF_FULLDARK},
	{"RF_SEMIBRIGHT",RF_SEMIBRIGHT},
	{"RF_NOCOLORMAPS",RF_NOCOLORMAPS},
	{"RF_SPRITETYPEMASK",RF_SPRITETYPEMASK},
	{"RF_PAPERSPRITE",RF_PAPERSPRITE},
	{"RF_FLOORSPRITE",RF_FLOORSPRITE},
	{"RF_SHADOWDRAW",RF_SHADOWDRAW},
	{"RF_SHADOWEFFECTS",RF_SHADOWEFFECTS},
	{"RF_DROPSHADOW",RF_DROPSHADOW},
	{"RF_DONTDRAW",RF_DONTDRAW},
	{"RF_DONTDRAWP1",RF_DONTDRAWP1},
	{"RF_DONTDRAWP2",RF_DONTDRAWP2},
	{"RF_DONTDRAWP3",RF_DONTDRAWP3},
	{"RF_DONTDRAWP4",RF_DONTDRAWP4},
	{"RF_BLENDMASK",RF_BLENDMASK},
	{"RF_BLENDSHIFT",RF_BLENDSHIFT},
	{"RF_ADD",RF_ADD},
	{"RF_SUBTRACT",RF_SUBTRACT},
	{"RF_REVERSESUBTRACT",RF_REVERSESUBTRACT},
	{"RF_MODULATE",RF_MODULATE},
	{"RF_OVERLAY",RF_OVERLAY},
	{"RF_TRANSMASK",RF_TRANSMASK},
	{"RF_TRANSSHIFT",RF_TRANSSHIFT},
	{"RF_TRANS10",RF_TRANS10},
	{"RF_TRANS20",RF_TRANS20},
	{"RF_TRANS30",RF_TRANS30},
	{"RF_TRANS40",RF_TRANS40},
	{"RF_TRANS50",RF_TRANS50},
	{"RF_TRANS60",RF_TRANS60},
	{"RF_TRANS70",RF_TRANS70},
	{"RF_TRANS80",RF_TRANS80},
	{"RF_TRANS90",RF_TRANS90},
	{"RF_GHOSTLY",RF_GHOSTLY},
	{"RF_GHOSTLYMASK",RF_GHOSTLYMASK},

	// Level flags
	{"LF_SCRIPTISFILE",LF_SCRIPTISFILE},
	{"LF_NOZONE",LF_NOZONE},
	{"LF_SECTIONRACE",LF_SECTIONRACE},
	{"LF_SUBTRACTNUM",LF_SUBTRACTNUM},
	// And map flags
	{"LF2_HIDEINMENU",LF2_HIDEINMENU},
	{"LF2_HIDEINSTATS",LF2_HIDEINSTATS},
	{"LF2_NOTIMEATTACK",LF2_NOTIMEATTACK},
	{"LF2_VISITNEEDED",LF2_VISITNEEDED},

	// Emeralds
	{"EMERALD_CHAOS1",EMERALD_CHAOS1},
	{"EMERALD_CHAOS2",EMERALD_CHAOS2},
	{"EMERALD_CHAOS3",EMERALD_CHAOS3},
	{"EMERALD_CHAOS4",EMERALD_CHAOS4},
	{"EMERALD_CHAOS5",EMERALD_CHAOS5},
	{"EMERALD_CHAOS6",EMERALD_CHAOS6},
	{"EMERALD_CHAOS7",EMERALD_CHAOS7},
	{"EMERALD_ALLCHAOS",EMERALD_ALLCHAOS},
	{"EMERALD_SUPER1",EMERALD_SUPER1},
	{"EMERALD_SUPER2",EMERALD_SUPER2},
	{"EMERALD_SUPER3",EMERALD_SUPER3},
	{"EMERALD_SUPER4",EMERALD_SUPER4},
	{"EMERALD_SUPER5",EMERALD_SUPER5},
	{"EMERALD_SUPER6",EMERALD_SUPER6},
	{"EMERALD_SUPER7",EMERALD_SUPER7},
	{"EMERALD_ALLSUPER",EMERALD_ALLSUPER},
	{"EMERALD_ALL",EMERALD_ALL},

	// SKINCOLOR_ doesn't include these..!
	{"MAXSKINCOLORS",MAXSKINCOLORS},
	{"FIRSTSUPERCOLOR",FIRSTSUPERCOLOR},
	{"NUMSUPERCOLORS",NUMSUPERCOLORS},

	// Precipitation
	{"PRECIP_NONE",PRECIP_NONE},
	{"PRECIP_RAIN",PRECIP_RAIN},
	{"PRECIP_SNOW",PRECIP_SNOW},
	{"PRECIP_BLIZZARD",PRECIP_BLIZZARD},
	{"PRECIP_STORM",PRECIP_STORM},
	{"PRECIP_STORM_NORAIN",PRECIP_STORM_NORAIN},
	{"PRECIP_STORM_NOSTRIKES",PRECIP_STORM_NOSTRIKES},

	// Carrying
	{"CR_NONE",CR_NONE},
	{"CR_ZOOMTUBE",CR_ZOOMTUBE},

	// Character flags (skinflags_t)
	{"SF_HIRES",SF_HIRES},
	{"SF_MACHINE",SF_MACHINE},

	// Sound flags
	{"SF_TOTALLYSINGLE",SF_TOTALLYSINGLE},
	{"SF_NOMULTIPLESOUND",SF_NOMULTIPLESOUND},
	{"SF_OUTSIDESOUND",SF_OUTSIDESOUND},
	{"SF_X4AWAYSOUND",SF_X4AWAYSOUND},
	{"SF_X8AWAYSOUND",SF_X8AWAYSOUND},
	{"SF_NOINTERRUPT",SF_NOINTERRUPT},
	{"SF_X2AWAYSOUND",SF_X2AWAYSOUND},

	// Global emblem var flags
	// none in kart yet

	// Map emblem var flags
	{"ME_ENCORE",ME_ENCORE},

	// p_local.h constants
	{"FLOATSPEED",FLOATSPEED},
	{"MAXSTEPMOVE",MAXSTEPMOVE},
	{"USERANGE",USERANGE},
	{"MELEERANGE",MELEERANGE},
	{"MISSILERANGE",MISSILERANGE},
	{"ONFLOORZ",ONFLOORZ}, // INT32_MIN
	{"ONCEILINGZ",ONCEILINGZ}, //INT32_MAX

	// for P_FlashPal
	{"PAL_WHITE",PAL_WHITE},
	{"PAL_MIXUP",PAL_MIXUP},
	{"PAL_RECYCLE",PAL_RECYCLE},
	{"PAL_NUKE",PAL_NUKE},

	// for P_DamageMobj
	//// Damage types
	{"DMG_NORMAL",DMG_NORMAL},
	{"DMG_WIPEOUT",DMG_WIPEOUT},
	{"DMG_EXPLODE",DMG_EXPLODE},
	{"DMG_TUMBLE",DMG_TUMBLE},
	{"DMG_STING",DMG_STING},
	{"DMG_KARMA",DMG_KARMA},
	//// Death types
	{"DMG_INSTAKILL",DMG_INSTAKILL},
	{"DMG_DEATHPIT",DMG_DEATHPIT},
	{"DMG_CRUSHED",DMG_CRUSHED},
	{"DMG_SPECTATOR",DMG_SPECTATOR},
	{"DMG_TIMEOVER",DMG_TIMEOVER},
	//// Masks
	{"DMG_STEAL",DMG_STEAL},
	{"DMG_CANTHURTSELF",DMG_CANTHURTSELF},
	{"DMG_WOMBO", DMG_WOMBO},
	{"DMG_DEATHMASK",DMG_DEATHMASK},
	{"DMG_TYPEMASK",DMG_TYPEMASK},

	// Intermission types
	{"int_none",int_none},
	{"int_race",int_race},
	{"int_battle",int_battle},
	{"int_battletime", int_battletime},

	// Jingles (jingletype_t)
	{"JT_NONE",JT_NONE},
	{"JT_OTHER",JT_OTHER},
	{"JT_MASTER",JT_MASTER},
	{"JT_1UP",JT_1UP},
	{"JT_SHOES",JT_SHOES},
	{"JT_INV",JT_INV},
	{"JT_MINV",JT_MINV},
	{"JT_DROWN",JT_DROWN},
	{"JT_SUPER",JT_SUPER},
	{"JT_GOVER",JT_GOVER},
	{"JT_NIGHTSTIMEOUT",JT_NIGHTSTIMEOUT},
	{"JT_SSTIMEOUT",JT_SSTIMEOUT},
	// {"JT_LCLEAR",JT_LCLEAR},
	// {"JT_RACENT",JT_RACENT},
	// {"JT_CONTSC",JT_CONTSC},

	// Player state (playerstate_t)
	{"PST_LIVE",PST_LIVE}, // Playing or camping.
	{"PST_DEAD",PST_DEAD}, // Dead on the ground, view follows killer.
	{"PST_REBORN",PST_REBORN}, // Ready to restart/respawn???

	// Player animation (panim_t)
	{"PA_ETC",PA_ETC},
	{"PA_STILL",PA_STILL},
	{"PA_SLOW",PA_SLOW},
	{"PA_FAST",PA_FAST},
	{"PA_DRIFT",PA_DRIFT},
	{"PA_HURT",PA_HURT},

	// Value for infinite lives
	{"INFLIVES",INFLIVES},

	// Got Flags, for player->gotflag!
	// Used to be MF_ for some stupid reason, now they're GF_ to stop them looking like mobjflags
	{"GF_REDFLAG",GF_REDFLAG},
	{"GF_BLUEFLAG",GF_BLUEFLAG},

	// Customisable sounds for Skins, from sounds.h
	{"SKSSPIN",SKSSPIN},
	{"SKSPUTPUT",SKSPUTPUT},
	{"SKSPUDPUD",SKSPUDPUD},
	{"SKSPLPAN1",SKSPLPAN1}, // Ouchies
	{"SKSPLPAN2",SKSPLPAN2},
	{"SKSPLPAN3",SKSPLPAN3},
	{"SKSPLPAN4",SKSPLPAN4},
	{"SKSPLDET1",SKSPLDET1}, // Deaths
	{"SKSPLDET2",SKSPLDET2},
	{"SKSPLDET3",SKSPLDET3},
	{"SKSPLDET4",SKSPLDET4},
	{"SKSPLVCT1",SKSPLVCT1}, // Victories
	{"SKSPLVCT2",SKSPLVCT2},
	{"SKSPLVCT3",SKSPLVCT3},
	{"SKSPLVCT4",SKSPLVCT4},
	{"SKSTHOK",SKSTHOK},
	{"SKSSPNDSH",SKSSPNDSH},
	{"SKSZOOM",SKSZOOM},
	{"SKSSKID",SKSSKID},
	{"SKSGASP",SKSGASP},
	{"SKSJUMP",SKSJUMP},
	// SRB2kart
	{"SKSKWIN",SKSKWIN}, // Win quote
	{"SKSKLOSE",SKSKLOSE}, // Lose quote
	{"SKSKPAN1",SKSKPAN1}, // Pain
	{"SKSKPAN2",SKSKPAN2},
	{"SKSKATK1",SKSKATK1}, // Offense item taunt
	{"SKSKATK2",SKSKATK2},
	{"SKSKBST1",SKSKBST1}, // Boost item taunt
	{"SKSKBST2",SKSKBST2},
	{"SKSKSLOW",SKSKSLOW}, // Overtake taunt
	{"SKSKHITM",SKSKHITM}, // Hit confirm taunt
	{"SKSKPOWR",SKSKPOWR}, // Power item taunt

	// 3D Floor/Fake Floor/FOF/whatever flags
	{"FF_EXISTS",FF_EXISTS},                   ///< Always set, to check for validity.
	{"FF_BLOCKPLAYER",FF_BLOCKPLAYER},         ///< Solid to player, but nothing else
	{"FF_BLOCKOTHERS",FF_BLOCKOTHERS},         ///< Solid to everything but player
	{"FF_SOLID",FF_SOLID},                     ///< Clips things.
	{"FF_RENDERSIDES",FF_RENDERSIDES},         ///< Renders the sides.
	{"FF_RENDERPLANES",FF_RENDERPLANES},       ///< Renders the floor/ceiling.
	{"FF_RENDERALL",FF_RENDERALL},             ///< Renders everything.
	{"FF_SWIMMABLE",FF_SWIMMABLE},             ///< Is a water block.
	{"FF_NOSHADE",FF_NOSHADE},                 ///< Messes with the lighting?
	{"FF_CUTSOLIDS",FF_CUTSOLIDS},             ///< Cuts out hidden solid pixels.
	{"FF_CUTEXTRA",FF_CUTEXTRA},               ///< Cuts out hidden translucent pixels.
	{"FF_CUTLEVEL",FF_CUTLEVEL},               ///< Cuts out all hidden pixels.
	{"FF_CUTSPRITES",FF_CUTSPRITES},           ///< Final step in making 3D water.
	{"FF_BOTHPLANES",FF_BOTHPLANES},           ///< Render inside and outside planes.
	{"FF_EXTRA",FF_EXTRA},                     ///< Gets cut by ::FF_CUTEXTRA.
	{"FF_TRANSLUCENT",FF_TRANSLUCENT},         ///< See through!
	{"FF_FOG",FF_FOG},                         ///< Fog "brush."
	{"FF_INVERTPLANES",FF_INVERTPLANES},       ///< Only render inside planes.
	{"FF_ALLSIDES",FF_ALLSIDES},               ///< Render inside and outside sides.
	{"FF_INVERTSIDES",FF_INVERTSIDES},         ///< Only render inside sides.
	{"FF_DOUBLESHADOW",FF_DOUBLESHADOW},       ///< Make two lightlist entries to reset light?
	{"FF_FLOATBOB",FF_FLOATBOB},               ///< Floats on water and bobs if you step on it.
	{"FF_NORETURN",FF_NORETURN},               ///< Used with ::FF_CRUMBLE. Will not return to its original position after falling.
	{"FF_CRUMBLE",FF_CRUMBLE},                 ///< Falls 2 seconds after being stepped on, and randomly brings all touching crumbling 3dfloors down with it, providing their master sectors share the same tag (allows crumble platforms above or below, to also exist).
	{"FF_SHATTERBOTTOM",FF_SHATTERBOTTOM},     ///< Used with ::FF_BUSTUP. Like FF_SHATTER, but only breaks from the bottom. Good for springing up through rubble.
	{"FF_MARIO",FF_MARIO},                     ///< Acts like a question block when hit from underneath. Goodie spawned at top is determined by master sector.
	{"FF_BUSTUP",FF_BUSTUP},                   ///< You can spin through/punch this block and it will crumble!
	{"FF_QUICKSAND",FF_QUICKSAND},             ///< Quicksand!
	{"FF_PLATFORM",FF_PLATFORM},               ///< You can jump up through this to the top.
	{"FF_REVERSEPLATFORM",FF_REVERSEPLATFORM}, ///< A fall-through floor in normal gravity, a platform in reverse gravity.
	{"FF_INTANGIBLEFLATS",FF_INTANGIBLEFLATS}, ///< Both flats are intangible, but the sides are still solid.
	{"FF_INTANGABLEFLATS",FF_INTANGIBLEFLATS}, ///< Both flats are intangable, but the sides are still solid.
	{"FF_SHATTER",FF_SHATTER},                 ///< Used with ::FF_BUSTUP. Bustable on mere touch.
	{"FF_SPINBUST",FF_SPINBUST},               ///< Used with ::FF_BUSTUP. Also bustable if you're in your spinning frames.
	{"FF_STRONGBUST",FF_STRONGBUST},           ///< Used with ::FF_BUSTUP. Only bustable by "strong" characters (Knuckles) and abilities (bouncing, twinspin, melee).
	{"FF_RIPPLE",FF_RIPPLE},                   ///< Ripple the flats
	{"FF_COLORMAPONLY",FF_COLORMAPONLY},       ///< Only copy the colormap, not the lightlevel
	{"FF_GOOWATER",FF_GOOWATER},               ///< Used with ::FF_SWIMMABLE. Makes thick bouncey goop.

	// PolyObject flags
	{"POF_CLIPLINES",POF_CLIPLINES},               ///< Test against lines for collision
	{"POF_CLIPPLANES",POF_CLIPPLANES},             ///< Test against tops and bottoms for collision
	{"POF_SOLID",POF_SOLID},                       ///< Clips things.
	{"POF_TESTHEIGHT",POF_TESTHEIGHT},             ///< Test line collision with heights
	{"POF_RENDERSIDES",POF_RENDERSIDES},           ///< Renders the sides.
	{"POF_RENDERTOP",POF_RENDERTOP},               ///< Renders the top.
	{"POF_RENDERBOTTOM",POF_RENDERBOTTOM},         ///< Renders the bottom.
	{"POF_RENDERPLANES",POF_RENDERPLANES},         ///< Renders top and bottom.
	{"POF_RENDERALL",POF_RENDERALL},               ///< Renders everything.
	{"POF_INVERT",POF_INVERT},                     ///< Inverts collision (like a cage).
	{"POF_INVERTPLANES",POF_INVERTPLANES},         ///< Render inside planes.
	{"POF_INVERTPLANESONLY",POF_INVERTPLANESONLY}, ///< Only render inside planes.
	{"POF_PUSHABLESTOP",POF_PUSHABLESTOP},         ///< Pushables will stop movement.
	{"POF_LDEXEC",POF_LDEXEC},                     ///< This PO triggers a linedef executor.
	{"POF_ONESIDE",POF_ONESIDE},                   ///< Only use the first side of the linedef.
	{"POF_NOSPECIALS",POF_NOSPECIALS},             ///< Don't apply sector specials.
	{"POF_SPLAT",POF_SPLAT},                       ///< Use splat flat renderer (treat cyan pixels as invisible).

#ifdef HAVE_LUA_SEGS
	// Node flags
	{"NF_SUBSECTOR",NF_SUBSECTOR}, // Indicate a leaf.
#endif

	// Slope flags
	{"SL_NOPHYSICS",SL_NOPHYSICS},
	{"SL_DYNAMIC",SL_DYNAMIC},

	// Angles
	{"ANG1",ANG1},
	{"ANG2",ANG2},
	{"ANG10",ANG10},
	{"ANG15",ANG15},
	{"ANG20",ANG20},
	{"ANG30",ANG30},
	{"ANG60",ANG60},
	{"ANG64h",ANG64h},
	{"ANG105",ANG105},
	{"ANG210",ANG210},
	{"ANG255",ANG255},
	{"ANG340",ANG340},
	{"ANG350",ANG350},
	{"ANGLE_11hh",ANGLE_11hh},
	{"ANGLE_22h",ANGLE_22h},
	{"ANGLE_45",ANGLE_45},
	{"ANGLE_67h",ANGLE_67h},
	{"ANGLE_90",ANGLE_90},
	{"ANGLE_112h",ANGLE_112h},
	{"ANGLE_135",ANGLE_135},
	{"ANGLE_157h",ANGLE_157h},
	{"ANGLE_180",ANGLE_180},
	{"ANGLE_202h",ANGLE_202h},
	{"ANGLE_225",ANGLE_225},
	{"ANGLE_247h",ANGLE_247h},
	{"ANGLE_270",ANGLE_270},
	{"ANGLE_292h",ANGLE_292h},
	{"ANGLE_315",ANGLE_315},
	{"ANGLE_337h",ANGLE_337h},
	{"ANGLE_MAX",ANGLE_MAX},

	// P_Chase directions (dirtype_t)
	{"DI_NODIR",DI_NODIR},
	{"DI_EAST",DI_EAST},
	{"DI_NORTHEAST",DI_NORTHEAST},
	{"DI_NORTH",DI_NORTH},
	{"DI_NORTHWEST",DI_NORTHWEST},
	{"DI_WEST",DI_WEST},
	{"DI_SOUTHWEST",DI_SOUTHWEST},
	{"DI_SOUTH",DI_SOUTH},
	{"DI_SOUTHEAST",DI_SOUTHEAST},
	{"NUMDIRS",NUMDIRS},

	// Buttons (ticcmd_t)
	{"BT_ACCELERATE",BT_ACCELERATE},
	{"BT_DRIFT",BT_DRIFT},
	{"BT_BRAKE",BT_BRAKE},
	{"BT_ATTACK",BT_ATTACK},
	{"BT_CUSTOM1",BT_CUSTOM1}, // Lua customizable
	{"BT_CUSTOM2",BT_CUSTOM2}, // Lua customizable
	{"BT_CUSTOM3",BT_CUSTOM3}, // Lua customizable

	// Lua command registration flags
	{"COM_ADMIN",COM_ADMIN},
	{"COM_LOCAL",COM_LOCAL},
	{"COM_PLAYER2",COM_PLAYER2},
	{"COM_PLAYER3",COM_PLAYER3},
	{"COM_PLAYER4",COM_PLAYER4},
	{"COM_SPLITSCREEN",COM_SPLITSCREEN},
	{"COM_SSSHIFT",COM_SSSHIFT},

	// cvflags_t
	{"CV_SAVE",CV_SAVE},
	{"CV_CALL",CV_CALL},
	{"CV_NETVAR",CV_NETVAR},
	{"CV_NOINIT",CV_NOINIT},
	{"CV_FLOAT",CV_FLOAT},
	{"CV_NOTINNET",CV_NOTINNET},
	{"CV_MODIFIED",CV_MODIFIED},
	{"CV_SHOWMODIF",CV_SHOWMODIF},
	{"CV_SHOWMODIFONETIME",CV_SHOWMODIFONETIME},
	{"CV_NOSHOWHELP",CV_NOSHOWHELP},
	{"CV_HIDEN",CV_HIDEN},
	{"CV_HIDDEN",CV_HIDEN},
	{"CV_CHEAT",CV_CHEAT},
	{"CV_NOLUA",CV_NOLUA},

	// v_video flags
	{"V_NOSCALEPATCH",V_NOSCALEPATCH},
	{"V_SMALLSCALEPATCH",V_SMALLSCALEPATCH},
	{"V_MEDSCALEPATCH",V_MEDSCALEPATCH},
	{"V_6WIDTHSPACE",V_6WIDTHSPACE},
	{"V_OLDSPACING",V_OLDSPACING},
	{"V_MONOSPACE",V_MONOSPACE},

	{"V_PURPLEMAP",V_PURPLEMAP},
	{"V_YELLOWMAP",V_YELLOWMAP},
	{"V_GREENMAP",V_GREENMAP},
	{"V_BLUEMAP",V_BLUEMAP},
	{"V_REDMAP",V_REDMAP},
	{"V_GRAYMAP",V_GRAYMAP},
	{"V_ORANGEMAP",V_ORANGEMAP},
	{"V_SKYMAP",V_SKYMAP},
	{"V_LAVENDERMAP",V_LAVENDERMAP},
	{"V_GOLDMAP",V_GOLDMAP},
	{"V_AQUAMAP",V_AQUAMAP},
	{"V_MAGENTAMAP",V_MAGENTAMAP},
	{"V_PINKMAP",V_PINKMAP},
	{"V_BROWNMAP",V_BROWNMAP},
	{"V_TANMAP",V_TANMAP},

	{"V_TRANSLUCENT",V_TRANSLUCENT},
	{"V_10TRANS",V_10TRANS},
	{"V_20TRANS",V_20TRANS},
	{"V_30TRANS",V_30TRANS},
	{"V_40TRANS",V_40TRANS},
	{"V_50TRANS",V_TRANSLUCENT}, // alias
	{"V_60TRANS",V_60TRANS},
	{"V_70TRANS",V_70TRANS},
	{"V_80TRANS",V_80TRANS},
	{"V_90TRANS",V_90TRANS},
	{"V_HUDTRANSHALF",V_HUDTRANSHALF},
	{"V_HUDTRANS",V_HUDTRANS},
	{"V_BLENDSHIFT",V_BLENDSHIFT},
	{"V_BLENDMASK",V_BLENDMASK},
	{"V_ADD",V_ADD},
	{"V_SUBTRACT",V_SUBTRACT},
	{"V_REVERSESUBTRACT",V_REVERSESUBTRACT},
	{"V_MODULATE",V_MODULATE},
	{"V_OVERLAY",V_OVERLAY},
	{"V_ALLOWLOWERCASE",V_ALLOWLOWERCASE},
	{"V_FLIP",V_FLIP},
	{"V_SNAPTOTOP",V_SNAPTOTOP},
	{"V_SNAPTOBOTTOM",V_SNAPTOBOTTOM},
	{"V_SNAPTOLEFT",V_SNAPTOLEFT},
	{"V_SNAPTORIGHT",V_SNAPTORIGHT},
	{"V_NOSCALESTART",V_NOSCALESTART},
	{"V_SPLITSCREEN",V_SPLITSCREEN},
	{"V_SLIDEIN",V_SLIDEIN},

	{"V_PARAMMASK",V_PARAMMASK},
	{"V_SCALEPATCHMASK",V_SCALEPATCHMASK},
	{"V_SPACINGMASK",V_SPACINGMASK},
	{"V_CHARCOLORMASK",V_CHARCOLORMASK},
	{"V_ALPHAMASK",V_ALPHAMASK},

	{"V_CHARCOLORSHIFT",V_CHARCOLORSHIFT},
	{"V_ALPHASHIFT",V_ALPHASHIFT},

	//Kick Reasons
	{"KR_KICK",KR_KICK},
	{"KR_PINGLIMIT",KR_PINGLIMIT},
	{"KR_SYNCH",KR_SYNCH},
	{"KR_TIMEOUT",KR_TIMEOUT},
	{"KR_BAN",KR_BAN},
	{"KR_LEAVE",KR_LEAVE},

	// translation colormaps
	{"TC_DEFAULT",TC_DEFAULT},
	{"TC_BOSS",TC_BOSS},
	{"TC_METALSONIC",TC_METALSONIC},
	{"TC_ALLWHITE",TC_ALLWHITE},
	{"TC_RAINBOW",TC_RAINBOW},
	{"TC_BLINK",TC_BLINK},
	{"TC_DASHMODE",TC_DASHMODE},
	{"TC_HITLAG",TC_HITLAG},

	// marathonmode flags
	{"MA_INIT",MA_INIT},
	{"MA_RUNNING",MA_RUNNING},
	{"MA_NOCUTSCENES",MA_NOCUTSCENES},
	{"MA_INGAME",MA_INGAME},

	// music types
	{"MU_NONE", MU_NONE},
	{"MU_WAV", MU_WAV},
	{"MU_MOD", MU_MOD},
	{"MU_MID", MU_MID},
	{"MU_OGG", MU_OGG},
	{"MU_MP3", MU_MP3},
	{"MU_FLAC", MU_FLAC},
	{"MU_GME", MU_GME},
	{"MU_MOD_EX", MU_MOD_EX},
	{"MU_MID_EX", MU_MID_EX},

	// gamestates
	{"GS_NULL",GS_NULL},
	{"GS_LEVEL",GS_LEVEL},
	{"GS_INTERMISSION",GS_INTERMISSION},
	{"GS_CONTINUING",GS_CONTINUING},
	{"GS_TITLESCREEN",GS_TITLESCREEN},
	{"GS_TIMEATTACK",GS_TIMEATTACK},
	{"GS_CREDITS",GS_CREDITS},
	{"GS_EVALUATION",GS_EVALUATION},
	{"GS_GAMEEND",GS_GAMEEND},
	{"GS_INTRO",GS_INTRO},
	{"GS_ENDING",GS_ENDING},
	{"GS_CUTSCENE",GS_CUTSCENE},
	{"GS_DEDICATEDSERVER",GS_DEDICATEDSERVER},
	{"GS_WAITINGPLAYERS",GS_WAITINGPLAYERS},

	// SRB2Kart
	// kartitems_t
#define FOREACH( name, n ) { #name, KITEM_ ## name }
	KART_ITEM_ITERATOR, // Actual items (can be set for k_itemtype)
#undef  FOREACH
	{"NUMKARTITEMS",NUMKARTITEMS},
	{"KRITEM_DUALSNEAKER",KRITEM_DUALSNEAKER}, // Additional roulette IDs (not usable for much in Lua besides K_GetItemPatch)
	{"KRITEM_TRIPLESNEAKER",KRITEM_TRIPLESNEAKER},
	{"KRITEM_TRIPLEBANANA",KRITEM_TRIPLEBANANA},
	{"KRITEM_TENFOLDBANANA",KRITEM_TENFOLDBANANA},
	{"KRITEM_TRIPLEORBINAUT",KRITEM_TRIPLEORBINAUT},
	{"KRITEM_QUADORBINAUT",KRITEM_QUADORBINAUT},
	{"KRITEM_DUALJAWZ",KRITEM_DUALJAWZ},
	{"NUMKARTRESULTS",NUMKARTRESULTS},

	// kartshields_t
	{"KSHIELD_NONE",KSHIELD_NONE},
	{"KSHIELD_LIGHTNING",KSHIELD_LIGHTNING},
	{"KSHIELD_BUBBLE",KSHIELD_BUBBLE},
	{"KSHIELD_FLAME",KSHIELD_FLAME},
	{"NUMKARTSHIELDS",NUMKARTSHIELDS},

	// kartspinoutflags_t
	{"KSPIN_THRUST",KSPIN_THRUST},
	{"KSPIN_IFRAMES",KSPIN_IFRAMES},
	{"KSPIN_AIRTIMER",KSPIN_AIRTIMER},

	{"KSPIN_TYPEBIT",KSPIN_TYPEBIT},
	{"KSPIN_TYPEMASK",KSPIN_TYPEMASK},

	{"KSPIN_SPINOUT",KSPIN_SPINOUT},
	{"KSPIN_WIPEOUT",KSPIN_WIPEOUT},
	{"KSPIN_STUNG",KSPIN_STUNG},
	{"KSPIN_EXPLOSION",KSPIN_EXPLOSION},

	// spottype_t
	{"SPOT_NONE",SPOT_NONE},
	{"SPOT_WEAK",SPOT_WEAK},
	{"SPOT_BUMP",SPOT_BUMP},

	// precipeffect_t
	{"PRECIPFX_THUNDER",PRECIPFX_THUNDER},
	{"PRECIPFX_LIGHTNING",PRECIPFX_LIGHTNING},
	{"PRECIPFX_WATERPARTICLES",PRECIPFX_WATERPARTICLES},

	{NULL,0}
};

// For this to work compile-time without being in this file,
// this function would need to check sizes at runtime, without sizeof
void DEH_TableCheck(void)
{
#if defined(_DEBUG) || defined(PARANOIA)
	const size_t dehstates = sizeof(STATE_LIST)/sizeof(const char*);
	const size_t dehmobjs  = sizeof(MOBJTYPE_LIST)/sizeof(const char*);
	const size_t dehcolors = sizeof(COLOR_ENUMS)/sizeof(const char*);

	if (dehstates != S_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked states list, you dolt!\n(%d states defined, versus %s in the Dehacked list)\n", S_FIRSTFREESLOT, sizeu1(dehstates));

	if (dehmobjs != MT_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked mobjtype list, you dolt!\n(%d mobj types defined, versus %s in the Dehacked list)\n", MT_FIRSTFREESLOT, sizeu1(dehmobjs));

	if (dehcolors != SKINCOLOR_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked colors list, you dolt!\n(%d colors defined, versus %s in the Dehacked list)\n", SKINCOLOR_FIRSTFREESLOT, sizeu1(dehcolors));
#endif
}
