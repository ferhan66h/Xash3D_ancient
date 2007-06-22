// g_utils.c -- misc utility functions for game module

#include "g_local.h"


void *TagMalloc (int size, int tag)
{
	if(tag == TAG_LEVEL) return Mem_Alloc(zone_level, size);
	else if(tag == TAG_GAME) return Mem_Alloc(zone_game, size);

	gi.dprintf("Warning: try to alloc unknown tag\n");
	return NULL;
}

void FreeTags (int tag)
{
	if(tag == TAG_LEVEL) Mem_EmptyPool(zone_level);
	else if(tag == TAG_GAME) Mem_EmptyPool(zone_game);
	else gi.dprintf("Warning: try to free unknown tag\n");
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/

char	com_token[MAX_INPUTLINE];
//sued by engine\server
char *COM_Parse (const char **data_p)
{
	int		c;
	int		len;
	const char	*data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return NULL;
	}
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return NULL;
		}
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

void G_ProjectSource (vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
	result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
	result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}

void G_ProjectSource2 (vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t up, vec3_t result)
{
	result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1] + up[0] * distance[2];
	result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1] + up[1] * distance[2];
	result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + up[2] * distance[2];
}

/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
edict_t *G_Find (edict_t *from, int fieldofs, char *match)
{
	char	*s;

	if (!from)
		from = g_edicts;
	else
		from++;

	for ( ; from < &g_edicts[globals.num_edicts] ; from++)
	{
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *)from + fieldofs);
		if (!s)
			continue;
		if (!strcasecmp (s, match))
			return from;
	}

	return NULL;
}


/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
edict_t *findradius (edict_t *from, vec3_t org, float rad)
{
	vec3_t	eorg;
	int		j;

	if (!from)
		from = g_edicts;
	else
		from++;
	for ( ; from < &g_edicts[globals.num_edicts]; from++)
	{
		if (!from->inuse)
			continue;
		if (from->solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j])*0.5);
		if (VectorLength(eorg) > rad)
			continue;
		return from;
	}

	return NULL;
}

/*
=================
findradius2

Returns entities that have origins within a spherical area

ROGUE - tweaks for performance for tesla specific code
only returns entities that can be damaged
only returns entities that are SVF_DAMAGEABLE

findradius2 (origin, radius)
=================
*/
edict_t *findradius2 (edict_t *from, vec3_t org, float rad)
{
	// rad must be positive
	vec3_t	eorg;
	int		j;

	if (!from)
		from = g_edicts;
	else
		from++;
	for ( ; from < &g_edicts[globals.num_edicts]; from++)
	{
		if (!from->inuse)
			continue;
		if (from->solid == SOLID_NOT)
			continue;
		if (!from->takedamage)
			continue;
		if (!(from->svflags & SVF_DAMAGEABLE))
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j])*0.5);
		if (VectorLength(eorg) > rad)
			continue;
		return from;
	}

	return NULL;
}

/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
#define MAXCHOICES	8

edict_t *G_PickTarget (char *targetname)
{
	edict_t	*ent = NULL;
	int		num_choices = 0;
	edict_t	*choice[MAXCHOICES];

	if (!targetname)
	{
		gi.dprintf("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices)
	{
		gi.dprintf("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}



void Think_Delay (edict_t *ent)
{
	G_UseTargets (ent, ent->activator);
	G_FreeEdict (ent);
}

/*
==============================
G_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets (edict_t *ent, edict_t *activator)
{
	edict_t		*t;

//
// check for a delay
//
	if (ent->delay)
	{
	// create a temp object to fire at a later time
		t = G_Spawn();
		t->classname = "DelayedUse";
		t->nextthink = level.time + ent->delay;
		t->think = Think_Delay;
		t->activator = activator;
		if (!activator)
			gi.dprintf ("Think_Delay with no activator\n");
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		t->noise_index = ent->noise_index;
		return;
	}
	
	
//
// print the message
//
	if ((ent->message) && !(activator->svflags & SVF_MONSTER))
	{
//		Lazarus - change so that noise_index < 0 means no sound
		gi.centerprintf (activator, "%s", ent->message);
		if (ent->noise_index > 0)
			gi.sound (activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
		else if (ent->noise_index == 0)
			gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
	}

//
// kill killtargets
//
	if (ent->killtarget)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->killtarget)))
		{
			// Lazarus: remove LIVE killtargeted monsters from total_monsters
			if((t->svflags & SVF_MONSTER) && (t->deadflag == DEAD_NO))
			{
				if(!t->dmgteam || strcmp(t->dmgteam,"player"))
					if(!(t->monsterinfo.aiflags & AI_GOOD_GUY))
						level.total_monsters--;
			}
			// and decrement secret count if target_secret is removed
			else if(t->class_id == ENTITY_TARGET_SECRET)
				level.total_secrets--;
			// same deal with target_goal, but also turn off CD music if applicable
			else if(t->class_id == ENTITY_TARGET_GOAL)
			{
				level.total_goals--;
				if (level.found_goals >= level.total_goals)
				gi.configstring (CS_CDTRACK, "0");
			}
			G_FreeEdict (t);
			if (!ent->inuse)
			{
				gi.dprintf("entity was removed while using killtargets\n");
				return;
			}
		}
	}

//
// fire targets
//
	if (ent->target)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->target)))
		{
			// doors fire area portals in a specific way
			if ( (t->class_id == ENTITY_FUNC_AREAPORTAL) &&
				( (ent->class_id == ENTITY_FUNC_DOOR) || 
				  (ent->class_id == ENTITY_FUNC_DOOR_ROTATING)) ) 
				continue;

			if (t == ent)
			{
				gi.dprintf ("WARNING: Entity used itself.\n");
			}
			else
			{
				if (t->use)
					t->use (t, ent, activator);
			}
			if (!ent->inuse)
			{
				gi.dprintf("entity was removed while using targets\n");
				return;
			}
		}
	}
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float	*tv (float x, float y, float z)
{
	static	int		index;
	static	vec3_t	vecs[8];
	float	*v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1)&7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char	*vtos (vec3_t v)
{
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	sprintf (s, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}


vec3_t VEC_UP		= {0, -1, 0};
vec3_t MOVEDIR_UP	= {0, 0, 1};
vec3_t VEC_DOWN		= {0, -2, 0};
vec3_t MOVEDIR_DOWN	= {0, 0, -1};

void G_SetMovedir (vec3_t angles, vec3_t movedir)
{
	if (VectorCompare (angles, VEC_UP))
	{
		VectorCopy (MOVEDIR_UP, movedir);
	}
	else if (VectorCompare (angles, VEC_DOWN))
	{
		VectorCopy (MOVEDIR_DOWN, movedir);
	}
	else
	{
		AngleVectors (angles, movedir, NULL, NULL);
	}

	VectorClear (angles);
}


float vectoyaw (vec3_t vec)
{
	float	yaw;
	
	if (vec[PITCH] == 0) {
		if (vec[YAW] == 0)
			yaw = 0;
		else if (vec[YAW] > 0)
			yaw = 90;
		else
			yaw = 270;
	} else {
		yaw = (int) (atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}

float vectoyaw2 (vec3_t vec)
{
	float	yaw;
	
	if (vec[PITCH] == 0) {
		if (vec[YAW] == 0)
			yaw = 0;
		else if (vec[YAW] > 0)
			yaw = 90;
		else
			yaw = 270;
	} else {
		yaw = (atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}


void vectoangles (vec3_t value1, vec3_t angles)
{
	float	forward;
	float	yaw, pitch;
	
	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
	// PMM - fixed to correct for pitch of 0
		if (value1[0])
			yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

void vectoangles2 (vec3_t value1, vec3_t angles)
{
	float	forward;
	float	yaw, pitch;
	
	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
	// PMM - fixed to correct for pitch of 0
		if (value1[0])
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

char *G_CopyString (char *in)
{
	char	*out;
	
	out = TagMalloc (strlen(in)+1, TAG_LEVEL);
	strcpy (out, in);
	return out;
}


void G_InitEdict (edict_t *e)
{
	e->inuse = true;
	e->classname = "noclass";
	e->gravity = 1.0;
	e->s.number = e - g_edicts;
	e->org_movetype = -1;
}

/*
=================
G_Spawn

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *G_Spawn (void)
{
	int			i;
	edict_t		*e;

	e = &g_edicts[(int)maxclients->value+1];
	for ( i=maxclients->value+1 ; i<globals.num_edicts ; i++, e++)
	{
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e->inuse && ( e->freetime < 2 || level.time - e->freetime > 0.5 ) )
		{
			G_InitEdict (e);
			return e;
		}
	}

	if (i == game.maxentities)
		gi.error ("ED_Alloc: no free edicts");

	globals.num_edicts++;

	if(developer->value && readout->value)
		gi.dprintf("num_edicts = %d\n",globals.num_edicts);

	G_InitEdict (e);
	return e;
}

/*
=================
G_FreeEdict

Marks the edict as free
=================
*/
void G_FreeEdict (edict_t *ed)
{
	// Lazarus - if part of a movewith chain, remove from
	// the chain and repair broken links
	if(ed->movewith) {
		edict_t	*e;
		edict_t	*parent=NULL;
		int		i;

		for(i=1; i<globals.num_edicts && !parent; i++) {
			e = g_edicts + i;
			if(e->movewith_next == ed) parent=e;
		}
		if(parent) parent->movewith_next = ed->movewith_next;
	}

	gi.unlinkentity (ed);		// unlink from world

	// Lazarus: In SP we no longer reserve slots for bodyque's
	if (deathmatch->value || coop->value) {
		if ((ed - g_edicts) <= (maxclients->value + BODY_QUEUE_SIZE))
		{
//			gi.dprintf("tried to free special edict\n");
			return;
		}
	} else {
		if ((ed - g_edicts) <= maxclients->value)
		{
			return;
		}
	}

	// Lazarus: actor muzzle flash
	if (ed->flash)
	{
		memset (ed->flash, 0, sizeof(*ed));
		ed->flash->classname = "freed";
		ed->flash->freetime  = level.time;
		ed->flash->inuse     = false;
	}

	// Lazarus: reflections
	if (!(ed->flags & FL_REFLECT))
		DeleteReflection(ed,-1);

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = false;
}

/*
============
G_TouchTriggers

============
*/
void	G_TouchTriggers (edict_t *ent)
{
	int			i, num;
	edict_t		*touch[MAX_EDICTS], *hit;

	// Lazarus: nothing touches anything if game is frozen
	if (level.freeze)
		return;

	// dead things don't activate triggers!
	if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0))
		return;

	num = gi.BoxEdicts (ent->absmin, ent->absmax, touch
		, MAX_EDICTS, AREA_TRIGGERS);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		hit = touch[i];
		if (!hit->inuse)
			continue;
		if (!hit->touch)
			continue;
		if (ent->client && ent->client->spycam && !(hit->svflags & SVF_TRIGGER_CAMOWNER))
			continue;
		hit->touch (hit, ent, NULL, NULL);
	}
}

/*
============
G_TouchSolids

Call after linking a new trigger in during gameplay
to force all entities it covers to immediately touch it
============
*/
void	G_TouchSolids (edict_t *ent)
{
	int			i, num;
	edict_t		*touch[MAX_EDICTS], *hit;

	num = gi.BoxEdicts (ent->absmin, ent->absmax, touch
		, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		hit = touch[i];
		if (!hit->inuse)
			continue;
		if (ent->touch)
			ent->touch (hit, ent, NULL, NULL);
		if (!ent->inuse)
			break;
	}
}




/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
bool KillBox (edict_t *ent)
{
	trace_t		tr;

	while (1)
	{
		tr = gi.trace (ent->s.origin, ent->mins, ent->maxs, ent->s.origin, NULL, MASK_PLAYERSOLID);
		if (!tr.ent)
			break;
		// nail it
		T_Damage (tr.ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

		// if we didn't kill it, fail
		if (tr.ent->solid)
			return false;
	}

	return true;		// all clear
}

void AnglesNormalize(vec3_t vec)
{
	while(vec[0] > 180)
		vec[0] -= 360;
	while(vec[0] < -180)
		vec[0] += 360;
	while(vec[1] > 360)
		vec[1] -= 360;
	while(vec[1] < 0)
		vec[1] += 360;
}

float SnapToEights(float x)
{
	x *= 8.0;
	if (x > 0.0)
		x += 0.5;
	else
		x -= 0.5;
	return 0.125 * (int)x;
}


/* Lazarus - added functions */

void stuffcmd(edict_t *pent, char *pszCommand)
{
	gi.WriteByte(svc_stufftext);
	gi.WriteString(pszCommand);
	gi.unicast(pent, true);
}

bool point_infront (edict_t *self, vec3_t point)
{
	vec3_t	vec;
	float	dot;
	vec3_t	forward;
	
	AngleVectors (self->s.angles, forward, NULL, NULL);
	VectorSubtract (point, self->s.origin, vec);
	VectorNormalize (vec);
	dot = DotProduct (vec, forward);
	
	if (dot > 0.3)
		return true;
	return false;
}

float AtLeast(float x, float dx)
{
	float xx;

	xx = (float)(floor(x/dx - 0.5)+1.)*dx;
	if(xx < x) xx += dx;
	return xx;
}

edict_t	*LookingAt(edict_t *ent, int filter, vec3_t endpos, float *range)
{
	edict_t		*who;
	edict_t		*trigger[MAX_EDICTS];
	edict_t		*ignore;
	trace_t		tr;
	vec_t		r;
	vec3_t      end, forward, start;
	vec3_t		dir, entp, mins, maxs;
	int			i, num;

	if(!ent->client) {
		if(endpos) VectorClear(endpos);
		if(range) *range = 0;
		return NULL;
	}
	VectorClear(end);
	if(ent->client->chasetoggle)
	{
		AngleVectors(ent->client->v_angle, forward, NULL, NULL);
		VectorCopy(ent->client->chasecam->s.origin,start);
		ignore = ent->client->chasecam;
	}
	else if(ent->client->spycam)
	{
		AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
		VectorCopy(ent->s.origin,start);
		ignore = ent->client->spycam;
	}
	else
	{
		AngleVectors(ent->client->v_angle, forward, NULL, NULL);
		VectorCopy(ent->s.origin, start);
		start[2] += ent->viewheight;
		ignore = ent;
	}

	VectorMA(start, 8192, forward, end);
	
	/* First check for looking directly at a pickup item */
	VectorSet(mins,-4096,-4096,-4096);
	VectorSet(maxs, 4096, 4096, 4096);
	num = gi.BoxEdicts (mins, maxs, trigger, MAX_EDICTS, AREA_TRIGGERS);
	for (i=0 ; i<num ; i++)
	{
		who = trigger[i];
		if (!who->inuse)
			continue;
		if (!who->item)
			continue;
		if (!visible(ent,who))
			continue;
		if (!infront(ent,who))
			continue;
		VectorSubtract(who->s.origin,start,dir);
		r = VectorLength(dir);
		VectorMA(start, r, forward, entp);
		if(entp[0] < who->s.origin[0] - 17) continue;
		if(entp[1] < who->s.origin[1] - 17) continue;
		if(entp[2] < who->s.origin[2] - 17) continue;
		if(entp[0] > who->s.origin[0] + 17) continue;
		if(entp[1] > who->s.origin[1] + 17) continue;
		if(entp[2] > who->s.origin[2] + 17) continue;
		if(endpos)
			VectorCopy(who->s.origin,endpos);
		if(range)
			*range = r;
		return who;
	}

	tr = gi.trace (start, NULL, NULL, end, ignore, MASK_SHOT);
	if (tr.fraction == 1.0)
	{
		// too far away
		gi.sound (ent, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
		return NULL;
	}
	if(!tr.ent)
	{
		// no hit
		gi.sound (ent, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
		return NULL;
	}
	if(!tr.ent->classname)
	{
		// should never happen
		gi.sound (ent, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
		return NULL;
	}

	if((strstr(tr.ent->classname,"func_") != NULL) && (filter & LOOKAT_NOBRUSHMODELS))
	{
		// don't hit on brush models
		gi.sound (ent, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
		return NULL;
	}
	if( (tr.ent == world) && (filter & LOOKAT_NOWORLD))
	{
		// world brush
		gi.sound (ent, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
		return NULL;
	}
	if(endpos) {
		endpos[0] = tr.endpos[0];
		endpos[1] = tr.endpos[1];
		endpos[2] = tr.endpos[2];
	}
	if(range) {
		VectorSubtract(tr.endpos,start,start);
		*range = VectorLength(start);
	}
	return tr.ent;
}

void GameDirRelativePath(char *filename, char *output)
{
	strcpy(output, filename );
}

/* Lazarus: G_UseTarget is similar to G_UseTargets, but only triggers
            a single target rather than all entities matching target
			criteria. It *does*, however, kill all killtargets */

void Think_Delay_Single (edict_t *ent)
{
	G_UseTarget (ent, ent->activator, ent->target_ent);
	G_FreeEdict (ent);
}

void G_UseTarget (edict_t *ent, edict_t *activator, edict_t *target)
{
	edict_t		*t;

//
// check for a delay
//
	if (ent->delay)
	{
	// create a temp object to fire at a later time
		t = G_Spawn();
		t->classname = "DelayedUse";
		t->nextthink = level.time + ent->delay;
		t->think = Think_Delay_Single;
		t->activator = activator;
		t->target_ent = target;
		if (!activator)
			gi.dprintf ("Think_Delay_Single with no activator\n");
		t->message = ent->message;
		t->target = ent->target;
		t->killtarget = ent->killtarget;
		t->noise_index = ent->noise_index;
		return;
	}
	
	
//
// print the message
//
	if ((ent->message) && !(activator->svflags & SVF_MONSTER))
	{
		gi.centerprintf (activator, "%s", ent->message);
		if (ent->noise_index > 0)
			gi.sound (activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
		else if (ent->noise_index == 0)
			gi.sound (activator, CHAN_AUTO, gi.soundindex ("misc/talk1.wav"), 1, ATTN_NORM, 0);
	}

//
// kill killtargets
//
	if (ent->killtarget)
	{
		t = NULL;
		while ((t = G_Find (t, FOFS(targetname), ent->killtarget)))
		{
			// Lazarus: remove killtargeted monsters from total_monsters
			if(t->svflags & SVF_MONSTER) {
				if(!t->dmgteam || strcmp(t->dmgteam,"player"))
					if(!(t->monsterinfo.aiflags & AI_GOOD_GUY))
						level.total_monsters--;
			}
			G_FreeEdict (t);
			if (!ent->inuse)
			{
				gi.dprintf("entity was removed while using killtargets\n");
				return;
			}
		}
	}

//
// fire target
//
	if (target)
	{
		// doors fire area portals in a specific way
		if ( (target->class_id == ENTITY_FUNC_AREAPORTAL) &&
			((ent->class_id == ENTITY_FUNC_DOOR) ||
			 (ent->class_id == ENTITY_FUNC_DOOR_ROTATING)))
			return;

		if (target == ent)
		{
			gi.dprintf ("WARNING: Entity used itself.\n");
		}
		else
		{
			if (target->use)
				target->use (target, ent, activator);
		}
		if (!ent->inuse)
		{
			gi.dprintf("entity was removed while using target\n");
			return;
		}
	}
}


/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/

//used by engine\server
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

//used by engine\server
void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\"))
	{
//		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
//used by engine\server
bool Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return false;
	if (strstr (s, ";"))
		return false;
	return true;
}

//used by engine\server
void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;
	int		maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Com_Printf ("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	sprintf (newi, "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	*s = 0;
}