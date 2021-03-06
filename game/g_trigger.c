// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"

void Item_Pow_Resource_In_Area(gentity_t* ent);

int gTrigFallSound;

void InitTrigger( gentity_t *self ) {
	if (!VectorCompare (self->s.angles, vec3_origin))
		G_SetMovedir (self->s.angles, self->movedir);

	trap_SetBrushModel( self, self->model );
	self->r.contents = CONTENTS_TRIGGER;		// replaces the -1 from trap_SetBrushModel
	self->r.svFlags = SVF_NOCLIENT;
}


// the wait time has passed, so set back up for another activation
void multi_wait( gentity_t *ent ) {
	ent->nextthink = 0;
}


// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger( gentity_t *ent, gentity_t *activator ) {
	gentity_t *rofftarget = NULL, *testent = NULL;
	gentity_t *te;
	int i = MAX_CLIENTS;

	if (ent->teamnodmg &&
		activator && activator->client &&
		ent->teamnodmg == (int)activator->client->sess.sessionTeam &&
		!g_ff_objectives.integer)
	{
		return;
	}

	if (ent->spawnflags & 1)
	{
		if (!activator || !activator->client)
		{
			return;
		}

		if (!(activator->client->pers.cmd.buttons & BUTTON_USE))
		{
			return;
		}
	}

	ent->activator = activator;
	if ( ent->nextthink ) {
		return;		// can't retrigger until the wait is over
	}

	if ( activator && activator->client ) {
		if ( ( ent->spawnflags & 2 ) &&
			activator->client->sess.sessionTeam != TEAM_RED ) {
			return;
		}
		if ( ( ent->spawnflags & 4 ) &&
			activator->client->sess.sessionTeam != TEAM_BLUE ) {
			return;
		}
	}

	G_UseTargets (ent, ent->activator);

	if (ent->roffname && ent->roffid != -1)
	{
		if (ent->rofftarget)
		{
			while (i < MAX_GENTITIES)
			{
				testent = &g_entities[i];

				if (testent && testent->targetname && strcmp(testent->targetname, ent->rofftarget) == 0)
				{
					rofftarget = testent;
					break;
				}
				i++;
			}
		}
		else
		{
			rofftarget = activator;
		}

		if (rofftarget)
		{
			trap_ROFF_Play(rofftarget->s.number, ent->roffid, qfalse);

			//Play it at the same time on the client, so that we can catch client-side notetrack events and not have to send
			//them over from the server (this wouldn't work for things like effects due to lack of ability to precache them
			//on the server)

			//remember the entity's original position in case of a server-side "loop" notetrack
			VectorCopy(rofftarget->s.pos.trBase, rofftarget->s.origin2);
			VectorCopy(rofftarget->s.apos.trBase, rofftarget->s.angles2);

			te = G_TempEntity( rofftarget->s.pos.trBase, EV_PLAY_ROFF );
			te->s.eventParm = ent->roffid;
			te->s.weapon = rofftarget->s.number;
			te->s.trickedentindex = 0;

			//But.. this may not produce desired results for clients who connect while a ROFF is playing.

			rofftarget->roffid = ent->roffid; //let this entity know the most recent ROFF played on him
		}
	}

	if ( ent->wait > 0 ) {
		ent->think = multi_wait;
		ent->nextthink = level.time + ( ent->wait + ent->random * crandom() ) * 1000;
	} else {
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->touch = 0;
		ent->nextthink = level.time + FRAMETIME;
		ent->think = G_FreeEntity;
	}
}

void Use_Multi( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	multi_trigger( ent, activator );
}

void Touch_Multi( gentity_t *self, gentity_t *other, trace_t *trace ) {
	if( !other->client ) {
		return;
	}
	multi_trigger( self, other );
}

/*QUAKED trigger_multiple (.5 .5 .5) ? USE_BUTTON RED_ONLY BLUE_ONLY
USE_BUTTON - Won't fire unless player is in it and pressing use button (in addition to any other conditions)
RED_ONLY - Only red team can use
BLUE_ONLY - Only blue team can use

"roffname"		If set, will play a roff upon activation
"rofftarget"	If set with roffname, will activate the roff an entity with
				this as its targetname. Otherwise uses roff on activating entity.
"wait" : Seconds between triggerings, 0.5 default, -1 = one time only.
"random"	wait variance, default is 0
Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)
*/
void SP_trigger_multiple( gentity_t *ent ) {
	if (ent->roffname)
	{
		ent->roffid = trap_ROFF_Cache(ent->roffname);
	}
	else
	{
		ent->roffid = -1;
	}

	G_SpawnFloat( "wait", "0.5", &ent->wait );
	G_SpawnFloat( "random", "0", &ent->random );

	if ( ent->random >= ent->wait && ent->wait >= 0 ) {
		ent->random = ent->wait - FRAMETIME;
		G_Printf( "trigger_multiple has random >= wait\n" );
	}

	ent->touch = Touch_Multi;
	ent->use = Use_Multi;

	InitTrigger( ent );
	trap_LinkEntity (ent);
}



/*
==============================================================================

trigger_always

==============================================================================
*/

void trigger_always_think( gentity_t *ent ) {
	G_UseTargets(ent, ent);
	G_FreeEntity( ent );
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always (gentity_t *ent) {
	// we must have some delay to make sure our use targets are present
	ent->nextthink = level.time + 300;
	ent->think = trigger_always_think;
}


/*
==============================================================================

trigger_push

==============================================================================
*/

void trigger_push_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {

	if ( !other->client ) {
		return;
	}

	BG_TouchJumpPad( &other->client->ps, &self->s );
}


/*
=================
AimAtTarget

Calculate origin2 so the target apogee will be hit
=================
*/
void AimAtTarget( gentity_t *self ) {
	gentity_t	*ent;
	vec3_t		origin;
	float		height, gravity, time, forward;
	float		dist;

	VectorAdd( self->r.absmin, self->r.absmax, origin );
	VectorScale ( origin, 0.5, origin );

	ent = G_PickTarget( self->target );
	if ( !ent ) {
		G_FreeEntity( self );
		return;
	}

	height = ent->s.origin[2] - origin[2];
	if ( height <= 0 ) {
		G_FreeEntity( self );
		return;
	}
	gravity = g_gravity.value;
	time = sqrt( height / ( .5 * gravity ) );

	// set s.origin2 to the push velocity
	VectorSubtract ( ent->s.origin, origin, self->s.origin2 );
	self->s.origin2[2] = 0;
	dist = VectorNormalize( self->s.origin2);

	forward = dist / time;
	VectorScale( self->s.origin2, forward, self->s.origin2 );

	self->s.origin2[2] = time * gravity;
}


/*QUAKED trigger_push (.5 .5 .5) ?
Must point at a target_position, which will be the apex of the leap.
This will be client side predicted, unlike target_push
*/
void SP_trigger_push( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	self->r.svFlags &= ~SVF_NOCLIENT;

	// make sure the client precaches this sound
	G_SoundIndex("sound/weapons/force/jump.wav");

	self->s.eType = ET_PUSH_TRIGGER;
	self->touch = trigger_push_touch;
	self->think = AimAtTarget;
	self->nextthink = level.time + FRAMETIME;
	trap_LinkEntity (self);
}


void Use_target_push( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !activator->client ) {
		return;
	}

	if ( activator->client->ps.pm_type != PM_NORMAL && activator->client->ps.pm_type != PM_FLOAT ) {
		return;
	}

	VectorCopy (self->s.origin2, activator->client->ps.velocity);

	// play fly sound every 1.5 seconds
	if ( activator->fly_sound_debounce_time < level.time ) {
		activator->fly_sound_debounce_time = level.time + 1500;
		if (self->noise_index)
		{
			G_Sound( activator, CHAN_AUTO, self->noise_index );
		}
	}
}

/*QUAKED target_push (.5 .5 .5) (-8 -8 -8) (8 8 8) bouncepad
Pushes the activator in the direction.of angle, or towards a target apex.
"speed"		defaults to 1000
if "bouncepad", play bounce noise instead of none
*/
void SP_target_push( gentity_t *self ) {
	if (!self->speed) {
		self->speed = 1000;
	}
	G_SetMovedir (self->s.angles, self->s.origin2);
	VectorScale (self->s.origin2, self->speed, self->s.origin2);

	if ( self->spawnflags & 1 ) {
		self->noise_index = G_SoundIndex("sound/weapons/force/jump.wav");
	} else {
		self->noise_index = 0;	//G_SoundIndex("sound/misc/windfly.wav");
	}
	if ( self->target ) {
		VectorCopy( self->s.origin, self->r.absmin );
		VectorCopy( self->s.origin, self->r.absmax );
		self->think = AimAtTarget;
		self->nextthink = level.time + FRAMETIME;
	}
	self->use = Use_target_push;
}

/*
==============================================================================

trigger_teleport

==============================================================================
*/

void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {
	gentity_t	*dest;

	if ( !other->client ) {
		return;
	}
	if ( other->client->ps.pm_type == PM_DEAD ) {
		return;
	}
	// Spectators only?
	if ( ( self->spawnflags & 1 ) && 
		other->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		return;
	}


	dest = 	G_PickTarget( self->target );
	if (!dest) {
		G_Printf ("Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( other, dest->s.origin, dest->s.angles );
}


/*QUAKED trigger_teleport (.5 .5 .5) ? SPECTATOR
Allows client side prediction of teleportation events.
Must point at a target_position, which will be the teleport destination.

If spectator is set, only spectators can use this teleport
Spectator teleporters are not normally placed in the editor, but are created
automatically near doors to allow spectators to move through them
*/
void SP_trigger_teleport( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	// unless is a spectator trigger
	if ( self->spawnflags & 1 ) {
		self->r.svFlags |= SVF_NOCLIENT;
	} else {
		self->r.svFlags &= ~SVF_NOCLIENT;
	}

	// make sure the client precaches this sound
	G_SoundIndex("sound/weapons/force/speed.wav");

	self->s.eType = ET_TELEPORT_TRIGGER;
	self->touch = trigger_teleporter_touch;

	trap_LinkEntity (self);
}


/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF CAN_TARGET SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.
It does dmg points of damage each server frame
Targeting the trigger will toggle its on / off state.

SILENT			supresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"dmg"			default 5 (whole numbers only)
If dmg is set to -1 this brush will use the fade-kill method

*/
void hurt_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if (activator && activator->inuse && activator->client)
	{
		self->activator = activator;
	}
	else
	{
		self->activator = NULL;
	}

	if ( self->r.linked ) {
		trap_UnlinkEntity( self );
	} else {
		trap_LinkEntity( self );
	}
}

void hurt_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	int		dflags;

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if (self->damage == -1 && other && other->client && other->health < 1)
	{
		other->client->ps.fallingToDeath = 0;
		respawn(other);
		return;
	}

	if (self->damage == -1 && other && other->client && other->client->ps.fallingToDeath)
	{
		return;
	}

	if ( self->spawnflags & 16 ) {
		self->timestamp = level.time + 1000;
	} else {
		self->timestamp = level.time + FRAMETIME;
	}

	// play sound
	if ( !(self->spawnflags & 4) && self->damage != -1 ) {
		G_Sound( other, CHAN_AUTO, self->noise_index );
	}

	if (self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;

	if (self->damage == -1 && other && other->client)
	{
		if (other->client->ps.otherKillerTime > level.time)
		{ //we're as good as dead, so if someone pushed us into this then remember them
			other->client->ps.otherKillerTime = level.time + 20000;
			other->client->ps.otherKillerDebounceTime = level.time + 10000;
		}
		other->client->ps.fallingToDeath = level.time;

		self->timestamp = 0; //do not ignore others
		G_EntitySound(other, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
	}
	else	
	{
		int dmg = self->damage;

		if (dmg == -1)
		{ //so fall-to-blackness triggers destroy evertyhing
			dmg = 99999;
			self->timestamp = 0;
		}
		if (self->activator && self->activator->inuse && self->activator->client)
		{
			G_Damage (other, self->activator, self->activator, NULL, NULL, dmg, dflags|DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
		}
		else
		{
			G_Damage (other, self, self, NULL, NULL, dmg, dflags|DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
		}
	}
}

void SP_trigger_hurt( gentity_t *self ) {
	InitTrigger (self);

	gTrigFallSound = G_SoundIndex("*falling1.wav");

	self->noise_index = G_SoundIndex( "sound/weapons/force/speed.wav" );
	self->touch = hurt_touch;

	if ( !self->damage ) {
		self->damage = 5;
	}

	self->r.contents = CONTENTS_TRIGGER;

	if ( self->spawnflags & 2 ) {
		self->use = hurt_use;
	}

	// link in to the world if starting active
	if ( ! (self->spawnflags & 1) ) {
		trap_LinkEntity (self);
	}
	else if (self->r.linked)
	{
		trap_UnlinkEntity(self);
	}
}


/*
==============================================================================

timer

==============================================================================
*/


/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
This should be renamed trigger_timer...
Repeatedly fires its targets.
Can be turned on or off by using.

"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0
so, the basic time between firing is a random time between
(wait - random) and (wait + random)

*/
void func_timer_think( gentity_t *self ) {
	G_UseTargets (self, self->activator);
	// set time before next firing
	self->nextthink = level.time + 1000 * ( self->wait + crandom() * self->random );
}

void func_timer_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	self->activator = activator;

	// if on, turn it off
	if ( self->nextthink ) {
		self->nextthink = 0;
		return;
	}

	// turn it on
	func_timer_think (self);
}

void SP_func_timer( gentity_t *self ) {
	G_SpawnFloat( "random", "1", &self->random);
	G_SpawnFloat( "wait", "1", &self->wait );

	self->use = func_timer_use;
	self->think = func_timer_think;

	if ( self->random >= self->wait ) {
		self->random = self->wait - FRAMETIME;
		G_Printf( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
	}

	if ( self->spawnflags & 1 ) {
		self->nextthink = level.time + FRAMETIME;
		self->activator = self;
	}

	self->r.svFlags = SVF_NOCLIENT;
}

/*
==============================================================================

PowTecH: RP - Item Spawning in an area
- classname	:	trigger_pow_resource_spawn_area
- count		:	Money player gets for picking it up
- random	:	Deviation in the money -+
- wait		:	How long before the item respawns
- roffname	:	md3 file path and name for the item

==============================================================================
*/

float findNewRandomNumberBetweenTwo(float min, float max) {
	//RNG garbage...
	int rng = 0;
	int wtf = 0;
	float i, j, result;
	int k;

	//Generate random spawn location in the area
	i = (int)(crandom() * 1000);
	j = (int)(crandom() * 1000);
	k = irand(0, j);
	wtf = level.time + i * k;
	Rand_Init(wtf);

	return result = irand(min, max);
}

void giveMoneyToPlayer(gentity_t *ent, float amount, float deviation) {
	int result;
	float min, max;

	min = amount - deviation;
	max = amount + deviation;

	result = (int)findNewRandomNumberBetweenTwo(min, max);
	ent->client->sess.rpMoney += result;
	trap_SendServerCommand(ent - g_entities, va("print \"^5[^7Credits: ^2+^3%d^5]^7\n\"", result));
}

trace_t setupTheResource(gentity_t* ent, float x, float y, float z) {
	vec3_t dest;
	trace_t tr;
	int minsResource[3] = { -8, -8, 0 };
	int maxsResource[3] = { 8, 8, 32 };

	//set up its dimentions
	VectorSet(ent->s.origin, x, y, z);
	VectorSet(ent->r.mins, minsResource[0], minsResource[1], minsResource[2]);
	VectorSet(ent->r.maxs, maxsResource[0], maxsResource[1], maxsResource[2]);

	VectorSet(dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
	trap_Trace(&tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID);

	return tr;
}

void Pow_Resource(gentity_t* self, gentity_t* activator) {
	if (!activator->client)
	{
		return;
	}

	if (!(activator->client->pers.cmd.buttons & BUTTON_USE))
	{
		return;
	}

	//print and give the money to the ent
	giveMoneyToPlayer(activator, self->parent->parent->count, self->parent->parent->random);
	trap_UnlinkEntity(self);

	//setup the trigger area to spawn another item
	self->parent->parent->think = Item_Pow_Resource_In_Area;
	self->parent->parent->nextthink = level.time + self->parent->parent->wait * 1000;
	
	//free the trigger then free the item
	G_FreeEntity(self->parent);
	G_FreeEntity(self);
}

void Use_Pow_Resource(gentity_t* self, gentity_t* other, gentity_t* activator) {
	Pow_Resource(self, activator);
}

void Touch_Pow_Resource(gentity_t* self, gentity_t* other, trace_t* trace) {
	if (!other->client) {
		return;
	}
	Pow_Resource(self, other);
}

void Item_Pow_Resource_In_Area_Trigger(gentity_t* ent) {
	gentity_t* newEnt;
	vec3_t		mins, maxs;

	// find the bounds of everything on the team
	VectorCopy(ent->r.absmin, mins);
	VectorCopy(ent->r.absmax, maxs);

	//can pick up the bomb from any angle
	maxs[0] += 16;
	maxs[1] += 16;
	maxs[2] += 16;

	mins[0] -= 16;
	mins[1] -= 16;
	mins[2] -= 16;

	newEnt = G_Spawn();
	VectorCopy(mins, newEnt->r.mins);
	VectorCopy(maxs, newEnt->r.maxs);
	newEnt->parent = ent;
	newEnt->r.contents = CONTENTS_TRIGGER;
	newEnt->touch = Touch_Pow_Resource;
	newEnt->use = Use_Pow_Resource;

	trap_LinkEntity(newEnt);
}

void Item_Pow_Resource_In_Area(gentity_t* ent) {
	gentity_t* newEnt;
	trace_t tr;
	float rngx, rngy;

	//Pick a spawn location within that trigger
	rngx = findNewRandomNumberBetweenTwo(ent->r.absmin[0], ent->r.absmax[0]);
	rngy = findNewRandomNumberBetweenTwo(ent->r.absmin[1], ent->r.absmax[1]);

	//Spawn the resource
	newEnt = G_Spawn();
	newEnt->parent = ent;
	newEnt->s.modelindex = G_ModelIndex(ent->roffname);

	newEnt->s.eFlags = 0;
	newEnt->r.contents = CONTENTS_SOLID;
	newEnt->clipmask = MASK_SOLID;
	newEnt->nextthink = level.time + FRAMETIME;
	newEnt->think = Item_Pow_Resource_In_Area_Trigger;

	tr = setupTheResource(newEnt, rngx, rngy, ent->r.absmin[2] + 2.00);

	while (!tr.startsolid) {
		G_Printf("^6Got stuck in something%s\n", vtos(newEnt->s.origin));

		//Pick a NEW spawn location within that trigger
		rngx = findNewRandomNumberBetweenTwo(ent->r.absmin[0], ent->r.absmax[0]);
		rngy = findNewRandomNumberBetweenTwo(ent->r.absmin[1], ent->r.absmax[1]);

		tr = setupTheResource(newEnt, rngx, rngy, ent->r.absmin[2] + 2.00);
	}

	G_SetOrigin(newEnt, newEnt->s.origin);
	VectorCopy(newEnt->s.angles, newEnt->s.apos.trBase);
	//G_SetAngles(newEnt, newEnt->s.angles);

	trap_LinkEntity(newEnt);

	G_SoundIndex("sound/movers/objects/useshieldstation.wav");

	newEnt->s.modelindex2 = G_ModelIndex("models/map_objects/bespin/bench.md3");
}

void SP_Trigger_Pow_Resource_Spawn_Area(gentity_t* ent) {
	ent->nextthink = level.time + FRAMETIME;
	ent->think = Item_Pow_Resource_In_Area;

	G_SpawnInt("count", "1", &ent->count);
	G_SpawnFloat("random", "0", &ent->wait);

	//Init Trigger
	InitTrigger(ent);
	trap_LinkEntity(ent);
}




