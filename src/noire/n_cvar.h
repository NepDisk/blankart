// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_CVAR__
#define __N_CVAR__

#ifdef __cplusplus
extern "C" {
#endif

//Rings
extern consvar_t cv_ng_rings;
extern consvar_t cv_ng_ringcap;
extern consvar_t cv_ng_spillcap;
extern consvar_t cv_ng_ringdebt;
extern consvar_t cv_ng_ringsting;
extern consvar_t cv_ng_ringsmarkedfordeath;
extern consvar_t cv_ng_maprings;
extern consvar_t cv_ng_mapringboxes;
extern consvar_t cv_ng_ringboxtransform;

//Collectables
extern consvar_t cv_ng_capsules;

//Items
extern consvar_t cv_ng_oldorbinaut;
extern consvar_t cv_ng_oldjawz;

//Mechanics
extern consvar_t cv_ng_fastfallbounce;
extern consvar_t cv_ng_draft;
extern consvar_t cv_ng_tumble;
extern consvar_t cv_ng_stumble;
extern consvar_t cv_ng_hitlag;
extern consvar_t cv_ng_mapanger;
extern consvar_t cv_ng_disabletripwires;
extern consvar_t cv_ng_nophysicsflag;

//Instawhip
extern consvar_t cv_ng_instawhip;
extern consvar_t cv_ng_instawhipcharge;
extern consvar_t cv_ng_instawhiplockout;
extern consvar_t cv_ng_instawhipdrain;

//Spindash
extern consvar_t cv_ng_spindash;
extern consvar_t cv_ng_spindashthreshold;
extern consvar_t cv_ng_spindashcharge;
extern consvar_t cv_ng_spindashoverheat;

//Driving
extern consvar_t cv_ng_butteredslopes;
extern consvar_t cv_ng_slopeclimb;
extern consvar_t cv_ng_stairjank;
extern consvar_t cv_ng_turnstyle;
extern consvar_t cv_ng_underwaterhandling;

//Bots
extern consvar_t cv_ng_botrubberbandboost;
extern consvar_t cv_ng_rivals;
extern consvar_t cv_ng_rivaltopspeed;
extern consvar_t cv_ng_rivalringpower;
extern consvar_t cv_ng_rivalfrantic;
extern consvar_t cv_ng_rivaldraft;

void NG_Rings_OnChange(void);
void NG_MapRings_OnChange(void);
void NG_Capsules_OnChange(void);
void NG_Tripwire_OnChange(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
