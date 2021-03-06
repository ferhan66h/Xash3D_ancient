//=======================================================================
//			Copyright XashXT Group 2009 ?
//		         com_world.h - shared world trace
//=======================================================================
#ifndef COM_WORLD_H
#define COM_WORLD_H

#define MOVE_NORMAL		0	// normal trace
#define MOVE_NOMONSTERS	1	// ignore monsters (edicts with flags (FL_MONSTER|FL_FAKECLIENT|FL_CLIENT) set)
#define MOVE_MISSILE	2	// extra size for monsters
#define MOVE_WORLDONLY	3	// clip only world

#define FMOVE_IGNORE_GLASS	0x100
#define FMOVE_SIMPLEBOX	0x200

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/
#define EDICT_FROM_AREA( l )		EDICT_NUM( l->entnum )
#define MAX_TOTAL_ENT_LEAFS		128
#define AREA_NODES			64
#define AREA_DEPTH			5

#define AREA_SOLID			1
#define AREA_TRIGGERS		2
#define AREA_CUSTOM			3	// user skins - water, lava, fog etc

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev;
	struct link_s	*next;
	int		entnum;	// NUM_FOR_EDICT
} link_t;

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
	link_t		water_edicts;	// func water
} areanode_t;

typedef struct area_s
{
	const float	*mins;
	const float	*maxs;
	edict_t		**list;
	int		count;
	int		maxcount;
	int		type;
} area_t;

typedef struct moveclip_s
{
	vec3_t		boxmins;	// enclose the test object along entire move
	vec3_t		boxmaxs;
	float		*mins;
	float		*maxs;	// size of the moving object
	vec3_t		mins2;
	vec3_t		maxs2;
	const float	*start;
	const float	*end;
	trace_t		trace;
	edict_t		*passedict;
	uint		umask;	// contents mask
	trType_t		type;
	int		flags;	// trace flags
} moveclip_t;

extern const char		*ed_name[];

// linked list
void InsertLinkBefore( link_t *l, link_t *before, int entnum );
void RemoveLink( link_t *l );
void ClearLink( link_t *l );

// trace common
model_t World_HullForEntity( const edict_t *ent );
void World_MoveBounds( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs );

// contents
int World_ConvertContents( int basecontents );
uint World_MaskForEdict( const edict_t *e );
uint World_ContentsForEdict( const edict_t *e );

#include "pm_shared.h"

/*
===============================================================================

	EVENTS QUEUES (hl1 events code)

===============================================================================
*/
#include "event_api.h"

#define MAX_EVENT_QUEUE	64		// 16 simultaneous events, max

typedef struct event_info_s
{
	word		index;		// 0 implies not in use
	short		packet_index;	// Use data from state info for entity in delta_packet .
					// -1 implies separate info based on event
					// parameter signature
	short		entity_index;	// The edict this event is associated with
	float		fire_time;	// if non-zero, the time when the event should be fired
					// ( fixed up on the client )
	event_args_t	args;
	int		flags;		// reliable or not, etc. ( CLIENT ONLY )
} event_info_t;

typedef struct event_state_s
{
	event_info_t	ei[MAX_EVENT_QUEUE];
} event_state_t;
	
#endif//COM_WORLD_H