// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Sally "TehRealSalt" Cochenour
// Copyright (C) 2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  orbinaut.c
/// \brief Orbinaut item code.

#include "../doomdef.h"
#include "../doomstat.h"
#include "../info.h"
#include "../k_kart.h"
#include "../k_objects.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h"
#include "../g_game.h"
#include "../z_zone.h"
#include "../k_waypoint.h"
#include "../k_respawn.h"
#include "../k_collide.h"

#define orbinaut_selfdelay(o) ((o)->threshold)

#define orbinaut_owner(o) ((o)->target)

enum {
	ORBI_WATERSKI	= 0x10, // this orbinaut can waterski
};

#define orbinaut_flags(o) ((o)->movedir)
#define orbinaut_spin(o) ((o)->extravalue2)

void Obj_OrbinautThink(mobj_t *mobj)
{
	boolean grounded = P_IsObjectOnGround(mobj);
	if (mobj->flags2 & MF2_AMBUSH)
	{
		if (grounded && (mobj->flags & MF_NOCLIPTHING))
		{
			mobj->momx = 1;
			mobj->momy = 0;
			mobj->frame = 3;
			S_StartSound(mobj, mobj->info->activesound);
			mobj->flags &= ~MF_NOCLIPTHING;
		}
		else if (mobj->movecount)
			mobj->movecount--;
		else if (mobj->frame < 3)
		{
			mobj->movecount = 2;
			mobj->frame++;
		}
	}
	else
	{
		fixed_t finalspeed = mobj->movefactor;

		P_SpawnGhostMobj(mobj);

		mobj->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy);
		if (mobj->health <= 5)
		{
			INT32 i;
			for (i = 5; i >= mobj->health; i--)
				finalspeed = FixedMul(finalspeed, FRACUNIT-FRACUNIT/4);
		}

		P_InstaThrust(mobj, mobj->angle, finalspeed);

		if (grounded)
		{
			/*sector_t *sec2 = P_ThingOnSpecial3DFloor(mobj);
			if ((sec2 && GETSECSPECIAL(sec2->special, 3) == 1)
				|| (P_IsObjectOnRealGround(mobj, mobj->subsector->sector)
				&& GETSECSPECIAL(mobj->subsector->sector->special, 3) == 1))
				K_DoPogoSpring(mobj, 0, 1);*/
		}

		if (mobj->threshold > 0)
			mobj->threshold--;

		if (leveltime % 6 == 0)
			S_StartSound(mobj, mobj->info->activesound);
	}
}

boolean Obj_OrbinautJawzCollide(mobj_t *t1, mobj_t *t2)
{
	boolean damageitem = false;
	boolean sprung = false;

	if (t1->health <= 0 || t2->health <= 0)
	{
		return true;
	}

	if ((orbinaut_owner(t1) == t2)
		|| (!(t2->flags & (MF_ENEMY|MF_BOSS)) && (orbinaut_owner(t1) == t2->target)))
	{
		if ((orbinaut_selfdelay(t1) > 0 && t2->type == MT_PLAYER)
			|| (orbinaut_selfdelay(t2) > 0 && t2->type != MT_PLAYER))
		{
			return true;
		}
	}

	if ((t1->type == MT_ORBINAUT_SHIELD || t1->type == MT_JAWZ_SHIELD) && t1->lastlook
		&& (t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD) && t2->lastlook
		&& (orbinaut_owner(t1) == t2->target)) // Don't hit each other if you have the same target
	{
		return true;
	}

	if (t2->player)
	{
		if ((t2->player->flashing > 0)
			&& !(t1->type == MT_ORBINAUT || t1->type == MT_JAWZ || t1->type == MT_JAWZ_DUD))
			return true;

		if (t2->player->hyudorotimer)
			return true; // no interaction

		if (t2->player->flamedash && t2->player->itemtype == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
		}
		else
		{
			// Player Damage
			if ((t1->type == MT_ORBINAUT_SHIELD || t1->type == MT_JAWZ_SHIELD)
				&& !t2->player->invincibilitytimer && !K_IsBigger(t2, t1)) // UGH. Stumble ignores invinc. Fix this damage type someday.
			{
				// If you do this a third time, please make it a part of the damage system.
				//                                    ^ remove all of P_DamageMobj and start over
				P_PlayRinglossSound(t2);
				P_PlayerRingBurst(t2->player, 5);
				P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
			}
			else
			{
				P_DamageMobj(t2, t1, t1->target, 1, DMG_WIPEOUT);
			}
			K_KartBouncing(t2, t1);
			S_StartSound(t2, sfx_s3k7b);
		}

		damageitem = true;
	}
	else if (t2->type == MT_ORBINAUT || t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD
		|| t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD
		|| t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_BALLHOG)
	{
		// Other Item Damage
		angle_t bounceangle = K_GetCollideAngle(t1, t2);

		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 24*FRACUNIT, false);
		P_InstaThrust(t2, bounceangle, 16*FRACUNIT);

		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);

		damageitem = true;
	}
	else if (t2->type == MT_SSMINE_SHIELD || t2->type == MT_SSMINE || t2->type == MT_LANDMINE)
	{
		damageitem = true;
		// Bomb death
		P_KillMobj(t2, t1, t1, DMG_NORMAL);
	}
	else if (t2->flags & MF_SPRING && (t1->type != MT_ORBINAUT_SHIELD && t1->type != MT_JAWZ_SHIELD))
	{
		// Let thrown items hit springs!
		sprung = P_DoSpring(t2, t1);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		damageitem = true;
	}
	else if (t2->flags & MF_PAIN)
	{
		// Hazard blocks
		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);
		damageitem = true;
	}

	if (damageitem && P_MobjWasRemoved(t1) == false)
	{
		angle_t bounceangle;
		if (P_MobjWasRemoved(t2) == false)
		{
			bounceangle = K_GetCollideAngle(t2, t1);
		}
		else
		{
			bounceangle = K_MomentumAngle(t1) + ANGLE_90;
			t2 = NULL; // handles the arguments to P_KillMobj
		}

		// This Item Damage
		S_StartSound(t1, t1->info->deathsound);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);

		P_SetObjectMomZ(t1, 24*FRACUNIT, false);

		P_InstaThrust(t1, bounceangle, 16*FRACUNIT);

	}

	if (sprung)
	{
		return false;
	}

	return true;
}

void Obj_OrbinautJawzMoveHeld(player_t *player)
{
			mobj_t *cur = player->mo->hnext;
				fixed_t speed = ((8 - min(4, player->itemamount)) * cur->info->speed) / 7;

				player->bananadrag = 0; // Just to make sure

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius); // mobj's distance from its Target, or Radius.
					fixed_t z;

					if (!cur->health)
					{
						cur = cur->hnext;
						continue;
					}

					cur->color = player->skincolor;

					cur->angle -= ANGLE_90;
					cur->angle += FixedAngle(speed);

					if (cur->extravalue1 < radius)
						cur->extravalue1 += P_AproxDistance(cur->extravalue1, radius) / 12;
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					// If the player is on the ceiling, then flip your items as well.
					if (player && player->mo->eflags & MFE_VERTICALFLIP)
						cur->eflags |= MFE_VERTICALFLIP;
					else
						cur->eflags &= ~MFE_VERTICALFLIP;

					if (P_MobjFlip(cur) > 0)
						z = player->mo->z;
					else
						z = player->mo->z + player->mo->height - cur->height;

					cur->flags |= MF_NOCLIPTHING; // temporarily make them noclip other objects so they can't hit anyone while in the player
					P_MoveOrigin(cur, player->mo->x, player->mo->y, z);
					cur->momx = FixedMul(FINECOSINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->momy = FixedMul(FINESINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->flags &= ~MF_NOCLIPTHING;

					if (!P_TryMove(cur, player->mo->x + cur->momx, player->mo->y + cur->momy, true, NULL))
					{
						P_SlideMove(cur, NULL);
						if (P_MobjWasRemoved(cur))
							continue;
					}

					if (P_IsObjectOnGround(player->mo))
					{
						if (P_MobjFlip(cur) > 0)
						{
							if (cur->floorz > player->mo->z - cur->height)
								z = cur->floorz;
						}
						else
						{
							if (cur->ceilingz < player->mo->z + player->mo->height + cur->height)
								z = cur->ceilingz - cur->height;
						}
					}

					// Center it during the scale up animation
					z += (FixedMul(mobjinfo[cur->type].height, player->mo->scale - cur->scale)>>1) * P_MobjFlip(cur);

					cur->z = z;
					cur->momx = cur->momy = 0;
					cur->angle += ANGLE_90;

					cur = cur->hnext;
				}
}

boolean Obj_OrbinautCanRunOnWater(mobj_t *th)
{
	return (orbinaut_flags(th) & ORBI_WATERSKI) == ORBI_WATERSKI;
}
