//=======================================================================
//			Copyright XashXT Group 2008 ?
//		        cl_frame.c - client world snapshot
//=======================================================================

#include "common.h"
#include "client.h"

/*
=========================================================================

FRAME PARSING

=========================================================================
*/
void CL_UpdateEntityFields( edict_t *ent )
{
	// these fields user can overwrite if need
	ent->v.model = MAKE_STRING( cl.configstrings[CS_MODELS+ent->pvClientData->current.modelindex] );

	clgame.dllFuncs.pfnUpdateEntityVars( ent, &ent->pvClientData->current, &ent->pvClientData->prev );

	if( ent->pvClientData->current.ed_flags & ESF_LINKEDICT )
	{
		CL_LinkEdict( ent, false );
		// to avoids multiple relinks when wait for next packet
		ent->pvClientData->current.ed_flags &= ~ESF_LINKEDICT;
	}

	// always keep an actual (users can't replace this)
	ent->serialnumber = ent->pvClientData->current.number;
	ent->v.classname = cl.edict_classnames[ent->pvClientData->current.classname];
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity( sizebuf_t *msg, frame_t *frame, int newnum, entity_state_t *old, bool unchanged )
{
	edict_t		*ent;
	entity_state_t	*state;
	bool		newent = (old) ? false : true;

	ent = EDICT_NUM( newnum );
	state = &cl.entity_curstates[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];

	if( newent ) old = &clgame.baselines[newnum];

	if( unchanged ) *state = *old;
	else MSG_ReadDeltaEntity( msg, old, state, newnum );

	if( state->number == MAX_EDICTS )
	{
		if( newent ) Host_Error( "Cl_DeltaEntity: tried to release new entity\n" );
		if( !ent->free ) CL_FreeEdict( ent );
		return; // entity was delta removed
	}

	cl.parse_entities++;
	frame->num_entities++;

	if( ent->free ) CL_InitEdict( ent );

	// some data changes will force no lerping
	if( state->ed_flags & ESF_NODELTA ) ent->pvClientData->serverframe = -99;
	if( newent ) state->ed_flags |= ESF_LINKEDICT; // need to relink

	if( ent->pvClientData->serverframe != cl.frame.serverframe - 1 )
	{	
		// duplicate the current state so lerping doesn't hurt anything
		ent->pvClientData->prev = *state;
	}
	else
	{	// shuffle the last state to previous
		ent->pvClientData->prev = ent->pvClientData->current;
	}

	ent->pvClientData->serverframe = cl.frame.serverframe;
	ent->pvClientData->current = *state;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( sizebuf_t *msg, frame_t *oldframe, frame_t *newframe )
{
	int		newnum;
	entity_state_t	*oldstate;
	int		oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	oldstate = NULL;
	if( !oldframe ) oldnum = MAX_ENTNUMBER;
	else
	{
		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while( 1 )
	{
		// read the entity index number
		newnum = MSG_ReadShort( msg );
		if( !newnum ) break; // end of packet entities

		if( msg->error )
			Host_Error("CL_ParsePacketEntities: end of message[%d > %d]\n", msg->readcount, msg->cursize );

		while( newnum >= clgame.globals->numEntities )
			clgame.globals->numEntities++;

		while( oldnum < newnum )
		{	
			// one or more entities from the old packet are unchanged
			CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
			
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}
		if( oldnum == newnum )
		{	
			// delta from previous state
			CL_DeltaEntity( msg, newframe, newnum, oldstate, false );
			oldindex++;

			if( oldindex >= oldframe->num_entities )
			{
				oldnum = MAX_ENTNUMBER;
			}
			else
			{
				oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if( oldnum > newnum )
		{	
			// delta from baseline ?
			CL_DeltaEntity( msg, newframe, newnum, NULL, false );
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while( oldnum != MAX_ENTNUMBER )
	{	
		// one or more entities from the old packet are unchanged
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, true );
		oldindex++;

		if( oldindex >= oldframe->num_entities )
		{
			oldnum = MAX_ENTNUMBER;
		}
		else
		{
			oldstate = &cl.entity_curstates[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}

/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame( sizebuf_t *msg )
{
	int	cmd, len, client_idx;
	edict_t	*clent;
          
	Mem_Set( &cl.frame, 0, sizeof( cl.frame ));

	cl.frame.serverframe = MSG_ReadLong( msg );
	cl.frame.servertime = MSG_ReadLong( msg );
	cl.serverframetime = MSG_ReadLong( msg );
	cl.frame.deltaframe = MSG_ReadLong( msg );
	cl.surpressCount = MSG_ReadByte( msg );
	client_idx = MSG_ReadByte( msg );

	// read clientindex
	clent = EDICT_NUM( client_idx ); // get client
	if(( client_idx - 1 ) != cl.playernum )
		Host_Error( "CL_ParseFrame: invalid playernum (%d should be %d)\n", client_idx - 1, cl.playernum );
	
	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if( cl.frame.deltaframe <= 0 )
	{
		cl.frame.valid = true;	// uncompressed frame
		cls.demowaiting = false;	// we can start recording now
		cl.oldframe = NULL;
	}
	else
	{
		cl.oldframe = &cl.frames[cl.frame.deltaframe & CL_UPDATE_MASK];
		if( !cl.oldframe->valid )
		{	
			// should never happen
			MsgDev( D_INFO, "delta from invalid frame (not supposed to happen!)\n" );
		}
		if( cl.oldframe->serverframe != cl.frame.deltaframe )
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			MsgDev( D_INFO, "delta frame too old\n" );
		}
		else if( cl.parse_entities - cl.oldframe->parse_entities > MAX_PARSE_ENTITIES - 128 )
		{
			MsgDev( D_INFO, "delta parse_entities too old\n" );
		}
		else cl.frame.valid = true;	// valid delta parse
	}

	// read areabits
	len = MSG_ReadByte( msg );
	MSG_ReadData( msg, &cl.frame.areabits, len );

	// read packet entities
	cmd = MSG_ReadByte( msg );
	if( cmd != svc_packetentities ) Host_Error("CL_ParseFrame: not packetentities[%d]\n", cmd );
	CL_ParsePacketEntities( msg, cl.oldframe, &cl.frame );

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & CL_UPDATE_MASK] = cl.frame;

	if( !cl.frame.valid ) return;

	if( cls.state != ca_active )
	{
		edict_t	*player;

		// client entered the game
		cls.state = ca_active;
		cl.force_refdef = true;
		cls.drawplaque = true;

		player = CL_GetLocalPlayer();
		SCR_MakeLevelShot();	// make levelshot if needs

		Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar	
		// getting a valid frame message ends the connection process
		VectorCopy( player->pvClientData->current.origin, cl.predicted_origin );
		VectorCopy( player->v.viewangles, cl.predicted_angles );
	}

	CL_CheckPredictionError();
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/
/*
===============
CL_AddPacketEntities

===============
*/
void CL_AddPacketEntities( frame_t *frame )
{
	edict_t	*ent;
	int	e, ed_type;

	// now recalc actual entcount
	for( ; EDICT_NUM( clgame.globals->numEntities - 1 )->free; clgame.globals->numEntities-- );

	for( e = 1; e < clgame.globals->numEntities; e++ )
	{
		ent = CL_GetEdictByIndex( e );
		if( !CL_IsValidEdict( ent )) continue;

		ed_type = ent->pvClientData->current.ed_type;
		CL_UpdateEntityFields( ent );

		if( clgame.dllFuncs.pfnAddVisibleEntity( ent, ed_type ))
		{
			if( ed_type == ED_PORTAL && !VectorCompare( ent->v.origin, ent->v.oldorigin ))
				cl.render_flags |= RDF_PORTALINVIEW;
		}
		// NOTE: skyportal entity never added to rendering
		if( ed_type == ED_SKYPORTAL ) cl.render_flags |= RDF_SKYPORTALINVIEW;
	}

	if( cl.oldframe && !memcmp( cl.oldframe->areabits, cl.frame.areabits, sizeof( cl.frame.areabits )))
		cl.render_flags |= RDF_OLDAREABITS;
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities( void )
{
	if( cls.state != ca_active )
		return;

	cl.render_flags = 0;

	clgame.dllFuncs.pfnStartFrame();	// new frame has begin
	CL_AddPacketEntities( &cl.frame );
	clgame.dllFuncs.pfnCreateEntities();

	CL_FireEvents();	// so tempents can be created immediately
	CL_AddParticles();
	CL_AddDLights();
	CL_AddLightStyles();
	CL_AddDecals();

	// perfomance test
	CL_TestEntities();
	CL_TestLights();
}

//
// sound engine implementation
//
bool CL_GetEntitySpatialization( int entnum, soundinfo_t *info )
{
	edict_t	*pSound;

	// world is always audible
	if( entnum == 0 )
		return true;

	if( entnum < 0 || entnum >= GI->max_edicts )
	{
		MsgDev( D_ERROR, "CL_GetEntitySoundSpatialization: invalid entnum %d\n", entnum );
		return false;
	}

	// while explosion entity can be died before sound played completely
	if( entnum >= clgame.globals->numEntities )
		return false;

	pSound = CL_GetEdictByIndex( entnum );

	// out of PVS, removed etc
	if( !pSound ) return false;
	
	if( !pSound->v.modelindex )
		return true;

	if( info->pflRadius )
	{
		vec3_t	mins, maxs;

		Mod_GetBounds( pSound->v.modelindex, mins, maxs );
		*info->pflRadius = RadiusFromBounds( mins, maxs );
	}
	
	if( info->pOrigin )
	{
		VectorCopy( pSound->v.origin, info->pOrigin );

		if( CM_GetModelType( pSound->v.modelindex ) == mod_brush )
		{
			vec3_t	mins, maxs, center;

			Mod_GetBounds( pSound->v.modelindex, mins, maxs );
			VectorAverage( mins, maxs, center );
			VectorAdd( info->pOrigin, center, info->pOrigin );
		}
	}

	if( info->pAngles )
	{
		VectorCopy( pSound->v.angles, info->pAngles );
	}
	return true;
}

void CL_GetEntitySoundSpatialization( int entnum, vec3_t origin, vec3_t velocity )
{
	edict_t	*ent;
	vec3_t	mins, maxs, midPoint;

	if( entnum < 0 || entnum >= GI->max_edicts )
	{
		MsgDev( D_ERROR, "CL_GetEntitySoundSpatialization: invalid entnum %d\n", entnum );
		VectorCopy( vec3_origin, origin );
		VectorCopy( vec3_origin, velocity );
		return;
	}

	// while explosion entity can be died before sound played completely
	if( entnum >= clgame.globals->numEntities ) return;

	ent = CL_GetEdictByIndex( entnum );
	if( !CL_IsValidEdict( ent )) return; // leave uncahnged

	// setup origin and velocity
	VectorCopy( ent->v.origin, origin );
	VectorCopy( ent->v.velocity, velocity );

	// if a brush model, offset the origin
	if( CM_GetModelType( ent->v.modelindex ) == mod_brush )
	{
		Mod_GetBounds( ent->v.modelindex, mins, maxs );
		VectorAverage( mins, maxs, midPoint );
		VectorAdd( origin, midPoint, origin );
	}
}