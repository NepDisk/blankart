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

#include "../command.h"

#ifdef __cplusplus
extern "C" {
#endif


//Player
extern consvar_t cv_colorizedhud;
extern consvar_t cv_colorizeditembox;
extern consvar_t cv_darkitembox;
extern consvar_t cv_colorizedhudcolor;

//Rings
extern consvar_t cv_ng_rings;
extern consvar_t cv_ng_ringcap;
extern consvar_t cv_ng_spillcap;
extern consvar_t cv_ng_ringdebt;
extern consvar_t cv_ng_ringsting;
extern consvar_t cv_ng_ringdeathmark;
extern consvar_t cv_ng_maprings;
extern consvar_t cv_ng_mapringboxes;
extern consvar_t cv_ng_ringboxtransform;

//Collectables
extern consvar_t cv_ng_capsules;

//Items
extern consvar_t cv_ng_oldorbinaut;
extern consvar_t cv_ng_oldjawz;

//Mechanics
extern consvar_t cv_ng_mapanger;
extern consvar_t cv_ng_tripwires;
extern consvar_t cv_ng_lives;
extern consvar_t cv_ng_continuesrank;
extern consvar_t cv_ng_dospecialstage;
extern consvar_t cv_ng_forcenoposition;

//Driving
extern consvar_t cv_ng_oldpogooverride;

//Bots
extern consvar_t cv_ng_botrubberbandboost;
extern consvar_t cv_ng_rivals;
extern consvar_t cv_ng_rivaltopspeed;
extern consvar_t cv_ng_rivalringpower;
extern consvar_t cv_ng_rivalfrantic;
extern consvar_t cv_ng_charsetrivals;

void NG_Generic_OnChange(void);
void NG_Rings_OnChange(void);
void NG_Lives_OnChange(void);
void NG_OldPogoOverride_OnChange(void);
void NG_Rivals_OnChange(void);
void ColorHUD_OnChange(void);
void NG_ForceNoPosition_OnChange(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
