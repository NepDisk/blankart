#include "../doomdef.h"
#include "../doomstat.h"
#include "../info.h"
#include "../k_kart.h"
#include "../k_objects.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h"

enum {
	HYU_PATROL,
	HYU_RETURN,
	HYU_HOVER,
};

// TODO: make these general functions

static fixed_t
K_GetSpeed (mobj_t *mobj)
{
	return FixedHypot(mobj->momx, mobj->momy);
}

static void
K_ChangePlayerItem
(		player_t * player,
		INT32 itemtype,
		INT32 itemamount)
{
	player->itemtype = itemtype;
	player->itemamount = itemamount;
	K_UnsetItemOut(player);
}

#define hyudoro_mode(o) ((o)->extravalue1)
#define hyudoro_itemtype(o) ((o)->movefactor)
#define hyudoro_itemcount(o) ((o)->movecount)
#define hyudoro_hover_stack(o) ((o)->threshold)
#define hyudoro_next(o) ((o)->tracer)
#define hyudoro_stackpos(o) ((o)->reactiontime)

// cannot be combined
#define hyudoro_center(o) ((o)->target)
#define hyudoro_target(o) ((o)->target)

#define hyudoro_center_max_radius(o) ((o)->threshold)
#define hyudoro_center_master(o) ((o)->target)

static angle_t
trace_angle (mobj_t *hyu)
{
	mobj_t *center = hyu->target;

	if (hyu->x != center->x || hyu->y != center->y)
	{
		return R_PointToAngle2(
				center->x, center->y, hyu->x, hyu->y);
	}
	else
		return hyu->angle;
}

static angle_t
get_look_angle (mobj_t *thing)
{
	player_t *player = thing->player;

	return player ? player->angleturn : thing->angle;
}

static boolean
is_hyudoro (mobj_t *thing)
{
	return thing && thing->type == MT_HYUDORO;
}

static mobj_t *
get_hyudoro_master (mobj_t *hyu)
{
	mobj_t *center = hyudoro_center(hyu);

	return center ? hyudoro_center_master(center) : NULL;
}

static player_t *
get_hyudoro_target_player (mobj_t *hyu)
{
	mobj_t *target = hyudoro_target(hyu);

	return target ? target->player : NULL;
}

static void
sine_bob
(		mobj_t * hyu,
		angle_t a,
		fixed_t sineofs)
{
	hyu->sprzoff = FixedMul(hyu->height,
			sineofs + FINESINE(a >> ANGLETOFINESHIFT));
}

static void
project_hyudoro (mobj_t *hyu)
{
	mobj_t *center = hyudoro_center(hyu);

	angle_t angleStep = FixedMul(5 * ANG1,
			FixedDiv(hyudoro_center_max_radius(center),
				center->radius));

	angle_t angle = trace_angle(hyu) + angleStep;

	fixed_t d = center->radius;

	fixed_t x = P_ReturnThrustX(center, angle, d);
	fixed_t y = P_ReturnThrustY(center, angle, d);

	hyu->momx = (center->x + x) - hyu->x;
	hyu->momy = (center->y + y) - hyu->y;
	hyu->angle = angle + ANGLE_90;

	sine_bob(hyu, angle, FRACUNIT);
}

static void
project_hyudoro_hover (mobj_t *hyu)
{
	const INT32 bob_speed = 64;

	mobj_t *target = hyudoro_target(hyu);

	// Turns a bit toward its target
	angle_t ang = get_look_angle(target) + ANGLE_67h;
	fixed_t rad = (target->radius * 2) + hyu->radius;

	fixed_t zofs = hyudoro_stackpos(hyu) *
		((target->height / 2) + (hyu->height * 2));

	P_MoveOrigin(hyu,
			target->x - P_ReturnThrustX(hyu, ang, rad),
			target->y - P_ReturnThrustY(hyu, ang, rad),
			target->z + (zofs * P_MobjFlip(target)));

	// Cancel momentum from HYU_RETURN.
	// (And anything else! I don't trust this game!!)
	hyu->momx = 0;
	hyu->momy = 0;

	hyu->angle = ang;

	// copies sprite tilting
	hyu->pitch = target->pitch;
	hyu->roll = target->roll;

	sine_bob(hyu,
			(leveltime & (bob_speed - 1)) *
			(ANGLE_MAX / bob_speed), -(3*FRACUNIT/4));
}

static void
spawn_hyudoro_shadow (mobj_t *hyu)
{
	mobj_t *shadow = P_SpawnMobjFromMobj(
			hyu, 0, 0, 0, MT_SHADOW);

	shadow->whiteshadow = true;

	shadow->shadowscale = hyu->shadowscale;
	hyu->shadowscale = 0;

	P_SetTarget(&shadow->tracer, hyu);
}

static void
move_to_player (mobj_t *hyu)
{
	mobj_t *target = hyudoro_target(hyu);

	angle_t angle;
	fixed_t speed;

	if (!target)
		return;

	angle = R_PointToAngle2(
			hyu->x, hyu->y, target->x, target->y);

	speed = (hyu->radius / 2) +
		max(hyu->radius, K_GetSpeed(target));

	// For first place only: cap hyudoro speed at 50%
	// target player's kart speed
	if (target->player && target->player->position == 1)
	{
		const fixed_t normalspeed =
			K_GetKartSpeed(target->player, false, false) / 2;

		speed = min(speed, normalspeed);
	}

	P_InstaThrust(hyu, angle, speed);

	hyu->z = target->z; // stay level with target
	hyu->angle = angle;
}

static void
deliver_item (mobj_t *hyu)
{
	mobj_t *target = hyudoro_target(hyu);
	player_t *player = target->player;

	P_SetTarget(&hyudoro_target(hyu), NULL);

	if (player)
	{
		K_ChangePlayerItem(player,
				hyudoro_itemtype(hyu),
				player->itemamount + hyudoro_itemcount(hyu));
	}

	S_StartSound(target, sfx_itpick);

	// Stop moving here
	hyu->momx = 0;
	hyu->momy = 0;

	hyu->tics = 4;

	hyu->destscale = target->scale / 4;
	hyu->scalespeed =
		abs(hyu->scale - hyu->destscale) / hyu->tics;
}

static void
append_hyudoro
(		mobj_t ** head,
		mobj_t * hyu)
{
	INT32 lastpos = 0;

	while (is_hyudoro(*head))
	{
		lastpos = hyudoro_stackpos(*head);
		head = &hyudoro_next(*head);
	}

	hyudoro_stackpos(hyu) = lastpos + 1;
	*head = hyu;
}

static void
pop_hyudoro (mobj_t **head)
{
	mobj_t *hyu = *head;

	INT32 lastpos;
	INT32 thispos;

	if (is_hyudoro(hyu))
	{
		lastpos = hyudoro_stackpos(hyu);
		hyu = hyudoro_next(hyu);

		while (is_hyudoro(hyu))
		{
			thispos = hyudoro_stackpos(hyu);

			hyudoro_stackpos(hyu) = lastpos;
			lastpos = thispos;

			hyu = hyudoro_next(hyu);
		}
	}

	*head = hyu;
}

static boolean
hyudoro_patrol_hit_player
(		mobj_t * hyu,
		mobj_t * toucher)
{
	player_t *player = toucher->player;

	mobj_t *center = hyudoro_center(hyu);

	if (!player)
		return false;

	// Cannot hit its master
	if (toucher == get_hyudoro_master(hyu))
		return false;

	// Don't punish a punished player
	if (player->hyudorotimer)
		return false;

	// NO ITEM?
	if (!player->itemamount)
		return false;

	hyudoro_mode(hyu) = HYU_RETURN;
	hyudoro_itemtype(hyu) = player->itemtype;
	hyudoro_itemcount(hyu) = player->itemamount;

	K_StripItems(player);

	player->hyudorotimer = hyudorotime;
	player->stealingtimer = hyudorotime;

	P_SetTarget(&hyudoro_target(hyu),
			hyudoro_center_master(center));

	if (center)
		P_RemoveMobj(center);

	hyu->renderflags &= ~(RF_DONTDRAW);

	return true;
}

static boolean
award_immediately (mobj_t *hyu)
{
	player_t *player = get_hyudoro_target_player(hyu);

	if (player)
	{
		if (player->itemamount &&
				player->itemtype != hyudoro_itemtype(hyu))
		{
			return false;
		}

		// Same as picking up paper items; get stacks
		// immediately
		if (!P_CanPickupItem(player, 3))
			return false;
	}

	deliver_item(hyu);

	return true;
}

static boolean
hyudoro_return_hit_player
(		mobj_t * hyu,
		mobj_t * toucher)
{
	if (toucher != hyudoro_target(hyu))
		return false;

	// If the player already has an item, just hover beside
	// them until they use/lose it.
	if (!award_immediately(hyu))
	{
		hyudoro_mode(hyu) = HYU_HOVER;
		append_hyudoro(&toucher->player->hoverhyudoro, hyu);
	}

	return true;
}

static boolean
hyudoro_hover_await_stack (mobj_t *hyu)
{
	player_t *player = get_hyudoro_target_player(hyu);

	if (!player)
		return false;

	// First in stack goes first
	if (hyu != player->hoverhyudoro)
		return false;

	if (!award_immediately(hyu))
		return false;

	pop_hyudoro(&player->hoverhyudoro);

	return true;
}

void
Obj_HyudoroDeploy (mobj_t *master)
{
	mobj_t *center = P_SpawnMobjFromMobj(
			master, 0, 0, 0, MT_HYUDORO_CENTER);

	mobj_t *hyu = P_SpawnMobjFromMobj(
			center, 0, 0, 0, MT_HYUDORO);

	// This allows a Lua override
	if (!hyudoro_center_max_radius(center))
	{
		hyudoro_center_max_radius(center) =
			128 * center->scale;
	}

	center->radius = hyu->radius;

	P_InitAngle(hyu, master->angle);
	P_SetTarget(&hyudoro_center(hyu), center);
	P_SetTarget(&hyudoro_center_master(center), master);

	hyudoro_mode(hyu) = HYU_PATROL;

	// Set splitscreen player visibility
	if (master->player)
	{
		hyu->renderflags |= RF_DONTDRAW &
			~(K_GetPlayerDontDrawFlag(master->player));
	}

	spawn_hyudoro_shadow(hyu); // this sucks btw

	S_StartSound(master, sfx_s3k92); // scary ghost noise
}

void
Obj_HyudoroThink (mobj_t *hyu)
{
	// Might get set from clipping slopes
	hyu->momz = 0;

	switch (hyudoro_mode(hyu))
	{
		case HYU_PATROL:
			if (hyudoro_center(hyu))
				project_hyudoro(hyu);

			if (leveltime & 1)
			{
				mobj_t *ghost = P_SpawnGhostMobj(hyu);

				// Flickers every frame
				ghost->extravalue1 = 1;
				ghost->extravalue2 = 2;

				// copy per-splitscreen-player visibility
				ghost->renderflags =
					(hyu->renderflags & RF_DONTDRAW);

				ghost->tics = 8;

				P_SetTarget(&ghost->tracer, hyu);
			}
			break;

		case HYU_RETURN:
			move_to_player(hyu);
			break;

		case HYU_HOVER:
			if (hyudoro_target(hyu))
			{
				project_hyudoro_hover(hyu);
				hyudoro_hover_await_stack(hyu);
			}
			break;
	}
}

void
Obj_HyudoroCenterThink (mobj_t *center)
{
	fixed_t max_radius = hyudoro_center_max_radius(center);

	if (center->radius < max_radius)
		center->radius += max_radius / 64;
}

void
Obj_HyudoroCollide
(		mobj_t * hyu,
		mobj_t * toucher)
{
	switch (hyudoro_mode(hyu))
	{
		case HYU_PATROL:
			hyudoro_patrol_hit_player(hyu, toucher);
			break;

		case HYU_RETURN:
			hyudoro_return_hit_player(hyu, toucher);
			break;
	}
}
