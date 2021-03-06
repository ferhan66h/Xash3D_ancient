//=======================================================================
//			Copyright XashXT Group 2008 ?
//		        sv_client.c - client interactions
//=======================================================================

#include "common.h"
#include "const.h"
#include "server.h"

#define MAX_FORWARD		6

typedef struct ucmd_s
{
	const char	*name;
	void		(*func)( sv_client_t *cl );
} ucmd_t;

/*
=================
SV_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
void SV_GetChallenge( netadr_t from )
{
	int	i, oldest = 0;
	int	oldestTime;

	oldestTime = 0x7fffffff;
	// see if we already have a challenge for this ip
	for (i = 0; i < MAX_CHALLENGES; i++ )
	{
		if( !svs.challenges[i].connected && NET_CompareAdr( from, svs.challenges[i].adr ))
			break;
		if( svs.challenges[i].time < oldestTime )
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if( i == MAX_CHALLENGES )
	{
		// this is the first time this client has asked for a challenge
		svs.challenges[oldest].challenge = ((rand()<<16) ^ rand()) ^ svs.realtime;
		svs.challenges[oldest].adr = from;
		svs.challenges[oldest].time = svs.realtime;
		svs.challenges[oldest].connected = false;
		i = oldest;
	}

	// send it back
	Netchan_OutOfBandPrint( NS_SERVER, svs.challenges[i].adr, "challenge %i", svs.challenges[i].challenge );
}

/*
==================
SV_DirectConnect

A connection request that did not come from the master
==================
*/
void SV_DirectConnect( netadr_t from )
{
	char		userinfo[MAX_INFO_STRING];
	sv_client_t	temp, *cl, *newcl;
	edict_t		*ent;
	client_frame_t	*frames;
	int		i, edictnum;
	int		version;
	int		qport, count = 0;
	int		challenge;

	version = com.atoi( Cmd_Argv( 1 ));
	if( version != PROTOCOL_VERSION )
	{
		Netchan_OutOfBandPrint( NS_SERVER, from, "print\nServer uses protocol version %i.\n", PROTOCOL_VERSION );
		MsgDev( D_ERROR, "SV_DirectConnect: rejected connect from version %i\n", version );
		return;
	}

	qport = com.atoi( Cmd_Argv( 2 ));
	challenge = com.atoi( Cmd_Argv( 3 ));
	com.strncpy( userinfo, Cmd_Argv( 4 ), sizeof( userinfo ) - 1 );
	userinfo[sizeof(userinfo) - 1] = 0;

	// quick reject
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( NET_CompareBaseAdr(from, cl->netchan.remote_address) && (cl->netchan.qport == qport || from.port == cl->netchan.remote_address.port))
		{
			if(!NET_IsLocalAddress( from ) && (svs.realtime - cl->lastconnect) < sv_reconnect_limit->value * 1000 )
			{
				MsgDev( D_INFO, "%s:reconnect rejected : too soon\n", NET_AdrToString( from ));
				return;
			}
			break;
		}
	}
		
	// see if the challenge is valid (LAN clients don't need to challenge)
	if( !NET_IsLocalAddress( from ))
	{
		for( i = 0; i < MAX_CHALLENGES; i++ )
		{
			if( NET_CompareAdr( from, svs.challenges[i].adr ))
			{
				if( challenge == svs.challenges[i].challenge )
					break; // valid challenge
			}
		}
		if( i == MAX_CHALLENGES )
		{
			Netchan_OutOfBandPrint( NS_SERVER, from, "print\nNo or bad challenge for address.\n" );
			return;
		}
		// force the IP key/value pair so the game can filter based on ip
		Info_SetValueForKey( userinfo, "ip", NET_AdrToString( from ));
		svs.challenges[i].connected = true;
		MsgDev( D_INFO, "Client %i connecting with challenge %p\n", i, challenge );
	}
	else Info_SetValueForKey( userinfo, "ip", "127.0.0.1" ); // force the "ip" info key to "localhost"

	newcl = &temp;

	frames = newcl->frames;	// keep frames pointer
	Mem_Set( newcl, 0, sizeof( sv_client_t ));
	newcl->frames = frames;	// restore it

	// if there is already a slot for this ip, reuse it
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free ) continue;
		if( NET_CompareBaseAdr( from, cl->netchan.remote_address ) && (cl->netchan.qport == qport || from.port == cl->netchan.remote_address.port ))
		{
			MsgDev( D_INFO, "%s:reconnect\n", NET_AdrToString( from ));
			newcl = cl;
			goto gotnewcl;
		}
	}

	// find a client slot
	newcl = NULL;
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
	{
		if( cl->state == cs_free )
		{
			newcl = cl;
			break;
		}
	}
	if( !newcl )
	{
		Netchan_OutOfBandPrint( NS_SERVER, from, "print\nServer is full.\n" );
		MsgDev( D_INFO, "SV_DirectConnect: rejected a connection.\n");
		return;
	}

gotnewcl:	
	// build a new connection
	// accept the new client
	// this is the only place a sv_client_t is ever initialized
	*newcl = temp;
	sv_client = newcl;
	edictnum = (newcl - svs.clients) + 1;

	ent = EDICT_NUM( edictnum );
	ent->pvServerData->client = newcl;
	newcl->edict = ent;
	newcl->challenge = challenge; // save challenge for checksumming
	newcl->frames = (client_frame_t *)Z_Malloc( sizeof( client_frame_t ) * SV_UPDATE_BACKUP );
		
	// get the game a chance to reject this connection or modify the userinfo
	if(!(SV_ClientConnect( ent, userinfo )))
	{
		if(*Info_ValueForKey( userinfo, "rejmsg" )) 
			Netchan_OutOfBandPrint( NS_SERVER, from, "print\n%s\nConnection refused.\n", Info_ValueForKey( userinfo, "rejmsg" ));
		else Netchan_OutOfBandPrint( NS_SERVER, from, "print\nConnection refused.\n" );
		MsgDev( D_ERROR, "SV_DirectConnect: game rejected a connection.\n");
		return;
	}

	// parse some info from the info strings
	SV_UserinfoChanged( newcl, userinfo );

	// send the connect packet to the client
	Netchan_OutOfBandPrint( NS_SERVER, from, "client_connect" );

	Netchan_Setup( NS_SERVER, &newcl->netchan, from, qport );
	MSG_Init( &newcl->reliable, newcl->reliable_buf, sizeof( newcl->reliable_buf ));	// reliable buf
	MSG_Init( &newcl->datagram, newcl->datagram_buf, sizeof( newcl->datagram_buf ));	// datagram buf

	newcl->state = cs_connected;
	newcl->lastmessage = svs.realtime;
	newcl->lastconnect = svs.realtime;

	// if this was the first client on the server, or the last client
	// the server can hold, send a heartbeat to the master.
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected ) count++;
	if( count == 1 || count == sv_maxclients->integer )
		svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
==================
SV_FakeConnect

A connection request that came from the game module
==================
*/
edict_t *SV_FakeConnect( const char *netname )
{
	int		i, edictnum;
	char		userinfo[MAX_INFO_STRING];
	sv_client_t	temp, *cl, *newcl;
	edict_t		*ent;

	if( !netname ) netname = "";
	userinfo[0] = '\0';

	// setup fake client name
	Info_SetValueForKey( userinfo, "name", netname );

	// force the IP key/value pair so the game can filter based on ip
	Info_SetValueForKey( userinfo, "ip", "127.0.0.1" );

	// find a client slot
	newcl = &temp;
	Mem_Set( newcl, 0, sizeof( sv_client_t ));
	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
	{
		if( cl->state == cs_free )
		{
			newcl = cl;
			break;
		}
	}
	if( !newcl )
	{
		MsgDev( D_INFO, "SV_DirectConnect: rejected a connection.\n");
		return NULL;
	}

	// build a new connection
	// accept the new client
	// this is the only place a sv_client_t is ever initialized
	*newcl = temp;
	sv_client = newcl;
	edictnum = (newcl - svs.clients) + 1;

	ent = EDICT_NUM( edictnum );
	ent->pvServerData->client = newcl;
	newcl->edict = ent;
	newcl->challenge = -1;		// fake challenge
	ent->v.flags |= FL_FAKECLIENT;	// mark it as fakeclient

	// get the game a chance to reject this connection or modify the userinfo
	if( !SV_ClientConnect( ent, userinfo ))
	{
		MsgDev( D_ERROR, "SV_DirectConnect: game rejected a connection.\n" );
		return NULL;
	}

	// parse some info from the info strings
	SV_UserinfoChanged( newcl, userinfo );

	newcl->state = cs_spawned;
	newcl->lastmessage = svs.realtime;	// don't timeout
	newcl->lastconnect = svs.realtime;
	
	return ent;
}

/*
=====================
SV_ClientCconnect

QC code can rejected a connection for some reasons
e.g. ipban
=====================
*/
bool SV_ClientConnect( edict_t *ent, char *userinfo )
{
	bool result = true;

	// make sure we start with known default
	if( !sv.loadgame ) ent->v.flags = 0;

	MsgDev( D_NOTE, "SV_ClientConnect()\n" );
	svgame.globals->time = sv.time * 0.001f;
	result = svgame.dllFuncs.pfnClientConnect( ent, userinfo );

	return result;
}

/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void SV_DropClient( sv_client_t *drop )
{
	int	i;
	
	if( drop->state == cs_zombie ) return;	// already dropped

	// add the disconnect
	if(!( drop->edict && (drop->edict->v.flags & FL_FAKECLIENT )))
		MSG_WriteByte( &drop->netchan.message, svc_disconnect );

	// let the game known about client state
	svgame.globals->time = sv.time * 0.001f;

	if( drop->edict->v.flags & FL_SPECTATOR )
		svgame.dllFuncs.pfnSpectatorDisconnect( drop->edict );
	else svgame.dllFuncs.pfnClientDisconnect( drop->edict );

	drop->edict->pvServerData->s.ed_type = ED_STATIC;	// remove from server
	if( drop->edict->pvPrivateData )
	{
		// clear any dlls data but keep engine data
		svgame.dllFuncs.pfnOnFreeEntPrivateData( drop->edict );
		Mem_Free( drop->edict->pvPrivateData );
		drop->edict->pvPrivateData = NULL;
	}

//	SV_FreeEdict( drop->edict );
	if( drop->download ) drop->download = NULL;

	drop->state = cs_zombie; // become free in a few seconds
	drop->name[0] = 0;

	SV_RefreshUserinfo(); // refresh userinfo on disconnect

	// if this was the last client on the server, send a heartbeat
	// to the master so it is known the server is empty
	// send a heartbeat now so the master will get up to date info
	// if there is already a slot for this ip, reuse it
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		if( svs.clients[i].state >= cs_connected )
			break;
	}
	if( i == sv_maxclients->integer ) svs.last_heartbeat = MAX_HEARTBEAT;
}

/*
==============================================================================

SVC COMMAND REDIRECT

==============================================================================
*/
void SV_BeginRedirect( netadr_t adr, int target, char *buffer, int buffersize, void (*flush))
{
	if( !target || !buffer || !buffersize || !flush )
		return;

	host.rd.target = target;
	host.rd.buffer = buffer;
	host.rd.buffersize = buffersize;
	host.rd.flush = flush;
	host.rd.address = adr;
	host.rd.buffer[0] = 0;
}

void SV_FlushRedirect( netadr_t adr, int dest, char *buf )
{
	if( sv_client->edict && (sv_client->edict->v.flags & FL_FAKECLIENT))
		return;

	switch( dest )
	{
	case RD_PACKET:
		Netchan_OutOfBandPrint( NS_SERVER, adr, "print\n%s", buf );
		break;
	case RD_CLIENT:
		if( !sv_client ) return; // client not set
		MSG_WriteByte( &sv_client->netchan.message, svc_print );
		MSG_WriteByte( &sv_client->netchan.message, PRINT_HIGH );
		MSG_WriteString( &sv_client->netchan.message, buf );
		break;
	case RD_NONE:
		MsgDev( D_ERROR, "SV_FlushRedirect: %s: invalid destination\n", NET_AdrToString( adr ));
		break;
	}
}

void SV_EndRedirect( void )
{
	host.rd.flush( host.rd.address, host.rd.target, host.rd.buffer );

	host.rd.target = 0;
	host.rd.buffer = NULL;
	host.rd.buffersize = 0;
	host.rd.flush = NULL;
}

/*
===============
SV_StatusString

Builds the string that is sent as heartbeats and status replies
===============
*/
char *SV_StatusString( void )
{
	char		player[1024];
	static char	status[MAX_MSGLEN - 16];
	int		i;
	sv_client_t	*cl;
	int		statusLength;
	int		playerLength;

	com.strcpy( status, Cvar_Serverinfo());
	com.strcat( status, "\n" );
	statusLength = com.strlen(status);

	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		cl = &svs.clients[i];
		if( cl->state == cs_connected || cl->state == cs_spawned )
		{
			com.sprintf( player, "%i %i \"%s\"\n", (int)cl->edict->v.frags, cl->ping, cl->name );
			playerLength = com.strlen(player);
			if( statusLength + playerLength >= sizeof(status))
				break; // can't hold any more
			com.strcpy( status + statusLength, player );
			statusLength += playerLength;
		}
	}
	return status;
}

/*
================
SV_Status

Responds with all the info that qplug or qspy can see
================
*/
void SV_Status( netadr_t from )
{
	Netchan_OutOfBandPrint( NS_SERVER, from, "print\n%s", SV_StatusString());
}

/*
================
SV_Ack

================
*/
void SV_Ack( netadr_t from )
{
	Msg( "ping %s\n", NET_AdrToString( from ));
}

/*
================
SV_Info

Responds with short info for broadcast scans
The second parameter should be the current protocol version number.
================
*/
void SV_Info( netadr_t from )
{
	char	string[MAX_INFO_STRING];
	int	i, count = 0;
	int	version;

	// ignore in single player
	if( sv_maxclients->integer == 1 )
		return;

	version = com.atoi( Cmd_Argv( 1 ));
	string[0] = '\0';

	if( version != PROTOCOL_VERSION )
	{
		com.snprintf( string, sizeof( string ), "%s: wrong version\n", hostname->string );
	}
	else
	{
		for( i = 0; i < sv_maxclients->integer; i++ )
			if( svs.clients[i].state >= cs_connected )
				count++;

		Info_SetValueForKey( string, "host", hostname->string );
		Info_SetValueForKey( string, "map", sv.name );
		Info_SetValueForKey( string, "dm", va( "%i", svgame.globals->deathmatch ));
		Info_SetValueForKey( string, "team", va( "%i", svgame.globals->teamplay ));
		Info_SetValueForKey( string, "coop", va( "%i", svgame.globals->coop ));
		Info_SetValueForKey( string, "numcl", va( "%i", count ));
		Info_SetValueForKey( string, "maxcl", va( "%i", sv_maxclients->integer ));
	}
	Netchan_OutOfBandPrint( NS_SERVER, from, "info\n%s", string );
}

/*
================
SV_Ping

Just responds with an acknowledgement
================
*/
void SV_Ping( netadr_t from )
{
	Netchan_OutOfBandPrint( NS_SERVER, from, "ack" );
}

bool Rcon_Validate( void )
{
	if( !com.strlen( rcon_password->string ))
		return false;
	if( !com.strcmp( Cmd_Argv( 1 ), rcon_password->string ))
		return false;
	return true;
}

/*
===============
SV_RemoteCommand

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
void SV_RemoteCommand( netadr_t from, sizebuf_t *msg )
{
	char		remaining[1024];
	static char	outputbuf[MAX_MSGLEN - 16];
	int		i;

	if(!Rcon_Validate()) MsgDev(D_INFO, "Bad rcon from %s:\n%s\n", NET_AdrToString( from ), msg->data + 4 );
	else MsgDev( D_INFO, "Rcon from %s:\n%s\n", NET_AdrToString( from ), msg->data + 4 );
	SV_BeginRedirect( from, RD_PACKET, outputbuf, MAX_MSGLEN - 16, SV_FlushRedirect );

	if( !Rcon_Validate( ))
	{
		MsgDev( D_WARN, "Bad rcon_password.\n" );
	}
	else
	{
		remaining[0] = 0;
		for( i = 2; i < Cmd_Argc(); i++ )
		{
			com.strcat( remaining, Cmd_Argv( i ));
			com.strcat( remaining, " " );
		}
		Cmd_ExecuteString( remaining );
	}
	SV_EndRedirect();
}

/*
===================
SV_FullClientUpdate

Writes all update values to a sizebuf
===================
*/
void SV_FullClientUpdate( sv_client_t *cl, sizebuf_t *msg )
{
	int	i;
	char	info[MAX_INFO_STRING];
	
	i = cl - svs.clients;

	MSG_WriteByte( msg, svc_updateuserinfo );
	MSG_WriteByte( msg, i );

	if( cl->name[0] )
	{
		MSG_WriteByte( msg, true );

		com.strncpy( info, cl->userinfo, sizeof( info ));

		// remove server passwords, etc.
		Info_RemovePrefixedKeys( info, '_' );
		MSG_WriteString( msg, info );
	}
	else MSG_WriteByte( msg, false );
	
	MSG_DirectSend( MSG_ALL, vec3_origin, NULL );
	MSG_Clear( msg );
}

void SV_RefreshUserinfo( void )
{
	int		i;
	sv_client_t	*cl;

	for( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ )
		if( cl->state >= cs_connected && !(cl->edict && cl->edict->v.flags & FL_FAKECLIENT ))
			cl->sendinfo = cl->sendmovevars = true;
}

/*
===================
SV_UpdatePhysinfo

this is send all movevars values when client connected
otherwise see code SV_UpdateMovevars()
===================
*/
void SV_UpdatePhysinfo( sv_client_t *cl, sizebuf_t *msg )
{
	movevars_t	nullmovevars;

	Mem_Set( &nullmovevars, 0, sizeof( nullmovevars ));
	MSG_WriteDeltaMovevars( msg, &nullmovevars, &svgame.movevars );
	MSG_DirectSend( MSG_ONE, NULL, cl->edict );
	MSG_Clear( msg );
}

/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void SV_PutClientInServer( edict_t *ent )
{
	int		index;
	sv_client_t	*client;

	index = NUM_FOR_EDICT( ent ) - 1;
	client = ent->pvServerData->client;

	svgame.globals->time = sv.time * 0.001f;
	ent->pvServerData->s.ed_type = ED_CLIENT; // init edict type

	if( !sv.loadgame )
	{	
		if( ent->v.flags & FL_SPECTATOR )
		{
			// setup spectatormaxspeed and refresh physinfo
			SV_SetClientMaxspeed( client, svgame.movevars.spectatormaxspeed );

			svgame.dllFuncs.pfnSpectatorConnect( ent );
		}
		else
		{
			// copy signon buffer
			MSG_WriteData( &client->netchan.message, sv.signon.data, sv.signon.cursize );

			// setup maxspeed and refresh physinfo
			SV_SetClientMaxspeed( client, svgame.movevars.maxspeed );

			if( sv_maxclients->integer > 1 )
				ent->v.netname = MAKE_STRING(Info_ValueForKey( client->userinfo, "name" ));
			else ent->v.netname = 0;
			ent->v.view_ofs[2] = GI->viewheight[0];
	
			// fisrt entering
			svgame.dllFuncs.pfnClientPutInServer( ent );

			SV_BaselineForEntity( ent );
		}
	}
	else
	{
		// NOTE: we needs to setup angles on restore here
		if( ent->v.fixangle == 1 )
		{
			MSG_WriteByte( &sv.multicast, svc_setangle );
			MSG_WriteAngle32( &sv.multicast, ent->v.angles[0] );
			MSG_WriteAngle32( &sv.multicast, ent->v.angles[1] );
			MSG_WriteAngle32( &sv.multicast, 0 );
			MSG_DirectSend( MSG_ONE, vec3_origin, client->edict );
			ent->v.fixangle = 0;
		}
		ent->pvServerData->s.ed_flags |= (ESF_NODELTA|ESF_NO_PREDICTION);
	}

	client->pViewEntity = NULL; // reset pViewEntity

	if( !( ent->v.flags & FL_FAKECLIENT ))
	{
		MSG_WriteByte( &client->netchan.message, svc_setview );
		MSG_WriteWord( &client->netchan.message, NUM_FOR_EDICT( client->edict ));
		MSG_Send( MSG_ONE, NULL, client->edict );
	}

	// clear any temp states
	sv.loadgame = false;
	sv.paused = false;

	if( sv_maxclients->integer == 1 ) // singleplayer profiler
		MsgDev( D_INFO, "level loaded at %g sec\n", (Sys_Milliseconds() - svs.timestart) * 0.001f );
}

/*
==================
SV_TogglePause
==================
*/
void SV_TogglePause( const char *msg )
{
	sv.paused ^= 1;

	if( msg ) SV_BroadcastPrintf( PRINT_HIGH, "%s", msg );

	// send notification to all clients
	MSG_Begin( svc_setpause );
	MSG_WriteByte( &sv.multicast, sv.paused );
	MSG_Send( MSG_ALL, vec3_origin, NULL );
}

/*
============================================================

CLIENT COMMAND EXECUTION

============================================================
*/
/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f( sv_client_t *cl )
{
	int	playernum;
	edict_t	*ent;

	if( cl->state != cs_connected )
	{
		MsgDev( D_INFO, "new is not valid from the console\n" );
		return;
	}

	playernum = cl - svs.clients;

	// send the serverdata
	MSG_WriteByte( &cl->netchan.message, svc_serverdata );
	MSG_WriteLong( &cl->netchan.message, PROTOCOL_VERSION );
	MSG_WriteLong( &cl->netchan.message, svs.spawncount );
	MSG_WriteByte( &cl->netchan.message, playernum );
	MSG_WriteByte( &cl->netchan.message, svgame.globals->maxClients );
	MSG_WriteWord( &cl->netchan.message, svgame.globals->maxEntities );
	MSG_WriteString( &cl->netchan.message, sv.configstrings[CS_NAME] );
	MSG_WriteString( &cl->netchan.message, STRING( EDICT_NUM( 0 )->v.message ));	// Map Message

	// refresh userinfo on spawn
	SV_RefreshUserinfo();

	// game server
	if( sv.state == ss_active )
	{
		// set up the entity for the client
		ent = EDICT_NUM( playernum + 1 );
		ent->serialnumber = playernum + 1;
		cl->edict = ent;
		Mem_Set( &cl->lastcmd, 0, sizeof( cl->lastcmd ));

		// begin fetching configstrings
		MSG_WriteByte( &cl->netchan.message, svc_stufftext );
		MSG_WriteString( &cl->netchan.message, va( "cmd configstrings %i %i\n", svs.spawncount, 0 ));
	}
}

/*
==================
SV_Configstrings_f
==================
*/
void SV_Configstrings_f( sv_client_t *cl )
{
	int	start;
	string	cs;

	if( cl->state != cs_connected )
	{
		MsgDev( D_INFO, "configstrings is not valid from the console\n" );
		return;
	}

	// handle the case of a level changing while a client was connecting
	if( com.atoi( Cmd_Argv( 1 )) != svs.spawncount )
	{
		MsgDev( D_INFO, "configstrings from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi( Cmd_Argv( 2 ));

	// write a packet full of data
	while( cl->netchan.message.cursize < (MAX_MSGLEN / 2) && start < MAX_CONFIGSTRINGS )
	{
		if( sv.configstrings[start][0])
		{
			MSG_WriteByte( &cl->netchan.message, svc_configstring );
			MSG_WriteShort( &cl->netchan.message, start );
			MSG_WriteString( &cl->netchan.message, sv.configstrings[start] );
		}
		start++;
	}
	if( start == MAX_CONFIGSTRINGS ) com.snprintf( cs, MAX_STRING, "cmd baselines %i %i\n", svs.spawncount, 0 );
	else com.snprintf( cs, MAX_STRING, "cmd configstrings %i %i\n", svs.spawncount, start );

	// send next command
	MSG_WriteByte( &cl->netchan.message, svc_stufftext );
	MSG_WriteString( &cl->netchan.message, cs );
}

/*
==================
SV_Baselines_f
==================
*/
void SV_Baselines_f( sv_client_t *cl )
{
	int		start;
	entity_state_t	*base, nullstate;
	string		baseline;

	if( cl->state != cs_connected )
	{
		MsgDev( D_INFO, "baselines is not valid from the console\n" );
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if( com.atoi( Cmd_Argv( 1 )) != svs.spawncount )
	{
		MsgDev( D_INFO, "baselines from different level\n" );
		SV_New_f( cl );
		return;
	}
	
	start = com.atoi( Cmd_Argv( 2 ));

	Mem_Set( &nullstate, 0, sizeof( nullstate ));

	// write a packet full of data
	while( cl->netchan.message.cursize < MAX_MSGLEN / 2 && start < GI->max_edicts )
	{
		base = &svs.baselines[start];
		if( base->modelindex || base->effects )
		{
			MSG_WriteByte( &cl->netchan.message, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &nullstate, base, &cl->netchan.message, true, true );
		}
		start++;
	}

	if( start == GI->max_edicts ) com.snprintf( baseline, MAX_STRING, "precache %i\n", svs.spawncount );
	else com.snprintf( baseline, MAX_STRING, "cmd baselines %i %i\n", svs.spawncount, start );

	// send next command
	MSG_WriteByte( &cl->netchan.message, svc_stufftext );
	MSG_WriteString( &cl->netchan.message, baseline );
}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f( sv_client_t *cl )
{
	// handle the case of a level changing while a client was connecting
	if( com.atoi( Cmd_Argv( 1 )) != svs.spawncount )
	{
		Msg( "begin from different level\n" );
		SV_New_f( cl );
		return;
	}

	cl->state = cs_spawned;
	SV_PutClientInServer( cl->edict );

	// if we are paused, tell the client
	if( sv.paused )
	{
		MSG_Begin( svc_setpause );
		MSG_WriteByte( &sv.multicast, sv.paused );
		MSG_Send( MSG_ONE, vec3_origin, cl->edict );
		SV_ClientPrintf( cl, PRINT_HIGH, "Server is paused.\n" );
	}
}

/*
==================
SV_NextDownload_f
==================
*/
void SV_NextDownload_f( sv_client_t *cl )
{
	int	percent;
	int	r, size;

	if( !cl->download ) return;

	r = cl->downloadsize - cl->downloadcount;
	if( r > 1024 ) r = 1024;

	MSG_WriteByte( &cl->netchan.message, svc_download );
	MSG_WriteShort( &cl->netchan.message, r );

	cl->downloadcount += r;
	size = cl->downloadsize;
	if( !size ) size = 1;
	percent = cl->downloadcount * 100 / size;
	MSG_WriteByte( &cl->netchan.message, percent );
	MSG_WriteData( &cl->netchan.message, cl->download + cl->downloadcount - r, r );
	if( cl->downloadcount == cl->downloadsize ) cl->download = NULL;
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f( sv_client_t *cl )
{
	char	*name;
	int	offset = 0;

	name = Cmd_Argv( 1 );
	if(Cmd_Argc() > 2 ) offset = com.atoi(Cmd_Argv(2)); // continue download from
	cl->download = FS_LoadFile( name, &cl->downloadsize );
	cl->downloadcount = offset;
	if( offset > cl->downloadsize ) cl->downloadcount = cl->downloadsize;

	if( !allow_download->integer || !cl->download )
	{
		MsgDev( D_ERROR, "SV_BeginDownload_f: couldn't download %s to %s\n", name, cl->name );
		if( cl->download ) Mem_Free( cl->download );
		MSG_WriteByte( &cl->netchan.message, svc_download );
		MSG_WriteShort( &cl->netchan.message, -1 );
		MSG_WriteByte( &cl->netchan.message, 0 );
		cl->download = NULL;
		return;
	}

	SV_NextDownload_f( cl );
	MsgDev( D_INFO, "Downloading %s to %s\n", name, cl->name );
}

/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Disconnect_f( sv_client_t *cl )
{
	SV_DropClient( cl );	
}

/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f( sv_client_t *cl )
{
	Info_Print( Cvar_Serverinfo());
}

/*
==================
SV_Pause_f
==================
*/
void SV_Pause_f( sv_client_t *cl )
{
	string	message;

	if( !sv_pausable->integer )
	{
		SV_ClientPrintf( cl, PRINT_HIGH, "Pause not allowed.\n" );
		return;
	}

	if( cl->edict->v.flags & FL_SPECTATOR )
	{
		SV_ClientPrintf( cl, PRINT_HIGH, "Spectators can not pause.\n" );
		return;
	}

	if( !sv.paused ) com.snprintf( message, MAX_STRING, "%s paused the game\n", cl->name );
	else com.snprintf( message, MAX_STRING, "%s unpaused the game\n", cl->name );

	SV_TogglePause( message );
}


/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_UserinfoChanged( sv_client_t *cl, const char *userinfo )
{
	edict_t	*ent = cl->edict;
	char	*val;
	int	i;

	if( !userinfo || !userinfo[0] ) return; // ignored

	com.strncpy( cl->userinfo, userinfo, sizeof( cl->userinfo ));

	// name for C code (make colored string)
	com.snprintf( cl->name, sizeof( cl->name ), "^2%s^7", Info_ValueForKey( cl->userinfo, "name" ));

	// rate command
	val = Info_ValueForKey( cl->userinfo, "rate" );
	if( com.strlen( val ))
	{
		i = com.atoi( val );
		cl->rate = i;
		cl->rate = bound ( 100, cl->rate, 15000 );
	}
	else cl->rate = 5000;

	// msg command
	val = Info_ValueForKey( cl->userinfo, "msg" );
	if( com.strlen( val ))
		cl->messagelevel = com.atoi( val );

	if( SV_IsValidEdict( ent ))
	{
		if( sv_maxclients->integer > 1 )
		{
			const char *model = Info_ValueForKey( cl->userinfo, "model" );

			// apply custom playermodel
			if( com.strlen( model ) && com.stricmp( model, "player" ))
			{
				const char *path = va( "models/player/%s/%s.mdl", model, model );
				CM_RegisterModel( path, SV_ModelIndex( path )); // register model
				SV_SetModel( ent, path );
				cl->modelindex = ent->v.modelindex;
			}
			else cl->modelindex = 0;
		}
		else cl->modelindex = 0;
	}

	// call prog code to allow overrides
	svgame.globals->time = sv.time * 0.001f;
	svgame.globals->frametime = sv.frametime * 0.001f;
	svgame.dllFuncs.pfnClientUserInfoChanged( cl->edict, cl->userinfo );

	if( SV_IsValidEdict( ent ))
	{
		if( sv_maxclients->integer > 1 )
			ent->v.netname = MAKE_STRING(Info_ValueForKey( cl->userinfo, "name" ));
		else ent->v.netname = 0;
	}
	if( cl->state >= cs_connected ) cl->sendinfo = true; // needs for update client info 
}

/*
==================
SV_UpdateUserinfo_f
==================
*/
static void SV_UpdateUserinfo_f( sv_client_t *cl )
{
	SV_UserinfoChanged( cl, Cmd_Argv( 1 ));
}

ucmd_t ucmds[] =
{
{ "new", SV_New_f },
{ "begin", SV_Begin_f },
{ "pause", SV_Pause_f },
{ "baselines", SV_Baselines_f },
{ "info", SV_ShowServerinfo_f },
{ "nextdl", SV_NextDownload_f },
{ "disconnect", SV_Disconnect_f },
{ "download", SV_BeginDownload_f },
{ "userinfo", SV_UpdateUserinfo_f },
{ "configstrings", SV_Configstrings_f },
{ NULL, NULL }
};

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteClientCommand( sv_client_t *cl, char *s )
{
	ucmd_t	*u;

	Cmd_TokenizeString( s );
	for( u = ucmds; u->name; u++ )
	{
		if( !com.strcmp( Cmd_Argv( 0 ), u->name ))
		{
			MsgDev( D_NOTE, "ucmd->%s()\n", u->name );
			u->func( cl );
			break;
		}
	}

	if( !u->name && sv.state == ss_active )
	{
		// custom client commands
		svgame.globals->time = sv.time * 0.001f;
		svgame.globals->frametime = sv.frametime * 0.001f;
		svgame.dllFuncs.pfnClientCommand( cl->edict );
	}
}

/*
=================
MSG_Begin

Misc helper function
=================
*/

void _MSG_Begin( int dest, const char *filename, int fileline )
{
	_MSG_WriteBits( &sv.multicast, dest, "MSG_Begin", NET_BYTE, filename, fileline );
}

/*
=================
SV_Send

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MULTICAST_ONE	send to one client (ent can't be NULL)
MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
bool _MSG_Send( int dest, const vec3_t origin, const edict_t *ent, bool direct, const char *filename, int fileline )
{
	byte		*mask = NULL;
	int		leafnum = 0, cluster = 0;
	int		numsends = 0, area1 = 0, area2 = 0;
	int		j, numclients = sv_maxclients->integer;
	sv_client_t	*cl, *current = svs.clients;
	bool		reliable = false;
	bool		specproxy = false;

	switch( dest )
	{
	case MSG_INIT:
		if( sv.state == ss_loading )
		{
			// copy signon buffer
			MSG_WriteData( &sv.signon, sv.multicast.data, sv.multicast.cursize );
			MSG_Clear( &sv.multicast );
			return true;
		}
		// intentional fallthrough (in-game MSG_INIT it's a MSG_ALL reliable)
	case MSG_ALL:
		reliable = true;
		// intentional fallthrough
	case MSG_BROADCAST:
		// nothing to sort	
		break;
	case MSG_PAS_R:
		reliable = true;
		// intentional fallthrough
	case MSG_PAS:
		if( origin == NULL ) return false;
		leafnum = CM_PointLeafnum( origin );
		cluster = CM_LeafCluster( leafnum );
		mask = CM_ClusterPHS( cluster );
		area1 = CM_LeafArea( leafnum );
		break;
	case MSG_PVS_R:
		reliable = true;
		// intentional fallthrough
	case MSG_PVS:
		if( origin == NULL ) return false;
		leafnum = CM_PointLeafnum( origin );
		cluster = CM_LeafCluster( leafnum );
		mask = CM_ClusterPVS( cluster );
		area1 = CM_LeafArea( leafnum );
		break;
	case MSG_ONE:
		reliable = true;
		// intentional fallthrough
	case MSG_ONE_UNRELIABLE:
		if( ent == NULL ) return false;
		j = NUM_FOR_EDICT( ent );
		if( j < 1 || j > numclients ) return false;
		current = svs.clients + (j - 1);
		numclients = 1; // send to one
		break;
	case MSG_SPEC:
		specproxy = reliable = true;
		break;
	default:
		Host_Error( "MSG_Send: bad dest: %i (called at %s:%i)\n", dest, filename, fileline );
		return false;
	}

	// send the data to all relevent clients (or once only)
	for( j = 0, cl = current; j < numclients; j++, cl++ )
	{
		if( cl->state == cs_free || cl->state == cs_zombie )
			continue;
		if( cl->state != cs_spawned && !reliable )
			continue;

		if( specproxy && !( cl->edict->v.flags & FL_PROXY ))
			continue;

		if( !cl->edict || ( cl->edict->v.flags & FL_FAKECLIENT ))
			continue;

		if( mask )
		{
			leafnum = CM_PointLeafnum( cl->edict->v.origin );
			cluster = CM_LeafCluster( leafnum );
			area2 = CM_LeafArea( leafnum );
			if( !CM_AreasConnected( area1, area2 )) continue;
			if( mask && (!(mask[cluster>>3] & (1<<( cluster & 7 )))))
				continue;
		}

		if( reliable )
		{
			if( direct ) MSG_WriteData( &cl->netchan.message, sv.multicast.data, sv.multicast.cursize );
			else MSG_WriteData( &cl->reliable, sv.multicast.data, sv.multicast.cursize );
		}
		else MSG_WriteData( &cl->datagram, sv.multicast.data, sv.multicast.cursize );
		numsends++;
	}
	MSG_Clear( &sv.multicast );

	// 25% chanse for simulate random network bugs
	if( sv.write_bad_message && Com_RandomLong( 0, 32 ) <= 8 )
	{
		// just for network debugging (send only for local client)
		MSG_WriteByte( &sv.multicast, svc_bad );
		MSG_WriteLong( &sv.multicast, rand( ));		// send some random data
		MSG_WriteString( &sv.multicast, host.finalmsg );	// send final message
		MSG_Send( MSG_ALL, vec3_origin, NULL );
		sv.write_bad_message = false;
	}
	return numsends;	// debug
}

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
void SV_ConnectionlessPacket( netadr_t from, sizebuf_t *msg )
{
	char	*s;
	char	*c;

	MSG_BeginReading( msg );
	MSG_ReadLong( msg );// skip the -1 marker

	s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	c = Cmd_Argv( 0 );
	MsgDev( D_NOTE, "SV_ConnectionlessPacket: %s : %s\n", NET_AdrToString( from ), c );

	if( !com.strcmp( c, "ping" )) SV_Ping( from );
	else if( !com.strcmp( c, "ack" )) SV_Ack( from );
	else if( !com.strcmp( c, "status" )) SV_Status( from );
	else if( !com.strcmp( c, "info" )) SV_Info( from );
	else if( !com.strcmp( c, "getchallenge" )) SV_GetChallenge( from );
	else if( !com.strcmp( c, "connect" )) SV_DirectConnect( from );
	else if( !com.strcmp( c, "rcon" )) SV_RemoteCommand( from, msg );
	else MsgDev( D_ERROR, "bad connectionless packet from %s:\n%s\n", NET_AdrToString( from ), s );
}

/*
===============
SV_SetIdealPitch
===============
*/
void SV_SetIdealPitch( sv_client_t *cl )
{
	float		angleval, sinval, cosval;
	trace_t		tr;
	vec3_t		top, bottom;
	float		z[MAX_FORWARD];
	int		i, j;
	int		step, dir, steps;
	edict_t		*ent = cl->edict;

	if( !( ent->v.flags & FL_ONGROUND ))
		return;
		
	angleval = ent->v.angles[YAW] * M_PI * 2 / 360;
	com.sincos( angleval, &sinval, &cosval );

	for( i = 0; i < MAX_FORWARD; i++ )
	{
		top[0] = ent->v.origin[0] + cosval * (i + 3) * 12;
		top[1] = ent->v.origin[1] + sinval * (i + 3) * 12;
		top[2] = ent->v.origin[2] + ent->v.view_ofs[2];
		
		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;
		
		tr = SV_Move( top, vec3_origin, vec3_origin, bottom, MOVE_NOMONSTERS, ent );
		if( tr.fAllSolid )
			return;	// looking at a wall, leave ideal the way is was

		if( tr.flFraction == 1.0f )
			return;	// near a dropoff
		
		z[i] = top[2] + tr.flFraction * (bottom[2] - top[2]);
	}
	
	dir = 0;
	steps = 0;
	for( j = 1; j < i; j++ )
	{
		step = z[j] - z[j-1];
		if( step > -ON_EPSILON && step < ON_EPSILON )
			continue;

		if( dir && ( step-dir > ON_EPSILON || step-dir < -ON_EPSILON ))
			return; // mixed changes

		steps++;	
		dir = step;
	}
	
	if( !dir )
	{
		ent->v.ideal_pitch = 0;
		return;
	}
	
	if( steps < 2 ) return;
	ent->v.ideal_pitch = -dir * sv_idealpitchscale->value;
}

/*
==================
SV_ReadClientMove

The message usually contains all the movement commands 
that were in the last three packets, so that the information
in dropped packets can be recovered.

On very fast clients, there may be multiple usercmd packed into
each of the backup packets.
==================
*/
static void SV_ReadClientMove( sv_client_t *cl, sizebuf_t *msg )
{
	int	checksum1, checksum2;
	int	key, lastframe, net_drop;
	usercmd_t	oldest, oldcmd, newcmd, nulcmd;

	key = msg->readcount;
	checksum1 = MSG_ReadByte( msg );
	lastframe = MSG_ReadLong( msg );

	if( lastframe != cl->lastframe )
	{
		cl->lastframe = lastframe;
		if( cl->lastframe > 0 )
		{
			client_frame_t *frame = &cl->frames[cl->lastframe & SV_UPDATE_MASK];
			frame->latency = svs.realtime - frame->senttime;
		}
	}

	cl->packet_loss = SV_CalcPacketLoss( cl );

	Mem_Set( &nulcmd, 0, sizeof( nulcmd ));
	MSG_ReadDeltaUsercmd( msg, &nulcmd, &oldest );
	MSG_ReadDeltaUsercmd( msg, &oldest, &oldcmd );
	MSG_ReadDeltaUsercmd( msg, &oldcmd, &newcmd );

	if( cl->state != cs_spawned )
	{
		cl->lastframe = -1;
		return;
	}

	// if the checksum fails, ignore the rest of the packet
	checksum2 = CRC_Sequence( msg->data + key + 1, msg->readcount - key - 1, cl->netchan.incoming_sequence );
	if( checksum2 != checksum1 )
	{
		MsgDev( D_ERROR, "SV_UserMove: failed command checksum for %s (%d != %d)\n", cl->name, checksum2, checksum1 );
		return;
	}

	if( !sv.paused )
	{
		SV_PreRunCmd( cl, &newcmd );	// get random_seed from newcmd

		net_drop = cl->netchan.dropped;

		if( net_drop < 20 )
		{
			while( net_drop > 2 )
			{
				SV_RunCmd( cl, &cl->lastcmd );
				net_drop--;
			}

			if( net_drop > 1 ) SV_RunCmd( cl, &oldest );
			if( net_drop > 0 ) SV_RunCmd( cl, &oldcmd );

		}
		SV_RunCmd( cl, &newcmd );
		SV_PostRunCmd( cl );
	}

	cl->lastcmd = newcmd;
	cl->lastcmd.buttons = 0; // avoid multiple fires on lag
}

/*
===================
SV_ExecuteClientMessage

Parse a client packet
===================
*/
void SV_ExecuteClientMessage( sv_client_t *cl, sizebuf_t *msg )
{
	int	c, stringCmdCount = 0;
	bool	move_issued = false;
	char	*s;

	// make sure the reply sequence number matches the incoming sequence number 
	if( cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence )
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else cl->send_message = false; // don't reply, sequences have slipped	
		
	// read optional clientCommand strings
	while( cl->state != cs_zombie )
	{
		c = MSG_ReadByte( msg );
		if( c == -1 ) break;

		if( msg->error )
		{
			MsgDev( D_ERROR, "SV_ReadClientMessage: clc_bad\n" );
			SV_DropClient( cl );
			return;
		}	

		switch( c )
		{
		case clc_nop:
			break;
		case clc_userinfo:
			SV_UserinfoChanged( cl, MSG_ReadString( msg ));
			break;
		case clc_move:
			if( move_issued ) return; // someone is trying to cheat...
			move_issued = true;
			SV_ReadClientMove( cl, msg );
			break;
		case clc_stringcmd:	
			s = MSG_ReadString( msg );
			// malicious users may try using too many string commands
			if( ++stringCmdCount < 8 ) SV_ExecuteClientCommand( cl, s );
			if( cl->state == cs_zombie ) return; // disconnect command
			break;
		default:
			MsgDev( D_ERROR, "SV_ReadClientMessage: clc_bad\n" );
			SV_DropClient( cl );
			return;
		}
	}
}