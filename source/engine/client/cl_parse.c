//=======================================================================
//			Copyright XashXT Group 2009 ?
//	      cl_parse.c  -- parse a message received from the server
//=======================================================================

#include "common.h"
#include "client.h"
#include "net_sound.h"

#define MSG_COUNT		32		// last 32 messages parsed
#define MSG_MASK		(MSG_COUNT - 1)

int CL_UPDATE_BACKUP = SINGLEPLAYER_BACKUP;

char *svc_strings[256] =
{
	"svc_bad",
// user messages space
	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_download",
	"svc_changing",
	"svc_physinfo",
	"svc_packetentities",
	"svc_frame",
	"svc_sound",
	"svc_ambientsound",
	"svc_setangle",
	"svc_addangle",
	"svc_setview",
	"svc_print",
	"svc_centerprint",
	"svc_crosshairangle",
	"svc_setpause",
	"svc_movevars",
	"svc_particle",
	"svc_soundfade",
	"svc_bspdecal",
	"svc_event",
	"svc_event_reliable",
	"svc_serverinfo"
};

typedef struct
{
	int	command;
	int	starting_offset;
	int	frame_number;
} oldcmd_t;

typedef struct
{
	oldcmd_t	oldcmd[MSG_COUNT];   
	int	currentcmd;
	bool	parsing;
} msg_debug_t;

static msg_debug_t	cls_message_debug;
static int	starting_count;

const char *CL_MsgInfo( int cmd )
{
	static string	sz;

	com.strcpy( sz, "???" );

	if( cmd > 200 && cmd < 256 )
	{
		// get engine message name
		com.strncpy( sz, svc_strings[cmd - 200], sizeof( sz ));
	}
	else if( cmd >= 0 && cmd < clgame.numMessages )
	{
		// get user message name
		if( clgame.msg[cmd] && clgame.msg[cmd]->name )
			com.strncpy( sz, clgame.msg[cmd]->name, sizeof( sz ));
	}
	return sz;
}

/*
=====================
CL_Parse_RecordCommand

record new message params into debug buffer
=====================
*/
void CL_Parse_RecordCommand( int cmd, int startoffset )
{
	int	slot;

	if( cmd == svc_nop ) return;
	
	slot = ( cls_message_debug.currentcmd++ & MSG_MASK );
	cls_message_debug.oldcmd[slot].command = cmd;
	cls_message_debug.oldcmd[slot].starting_offset = startoffset;
	cls_message_debug.oldcmd[slot].frame_number = host.framecount;
}

/*
=====================
CL_WriteErrorMessage

write net_message into buffer.dat for debugging
=====================
*/
void CL_WriteErrorMessage( int current_count, sizebuf_t *msg )
{
	file_t		*fp;
	const char	*buffer_file = "buffer.dat";
	
	fp = FS_Open( buffer_file, "wb" );
	if( !fp ) return;

	FS_Write( fp, &starting_count, sizeof( int ));
	FS_Write( fp, &current_count, sizeof( int ));
	FS_Write( fp, msg->data, msg->cursize );
	FS_Close( fp );

	MsgDev( D_INFO, "Wrote erroneous message to %s\n", buffer_file );
}

/*
=====================
CL_WriteMessageHistory

list last 32 messages for debugging net troubleshooting
=====================
*/
void CL_WriteMessageHistory( void )
{
	int	i, thecmd;
	oldcmd_t	*old, *failcommand;
	sizebuf_t	*msg = &net_message;

	if( !cls.initialized || cls.state == ca_disconnected )
		return;

	if( !cls_message_debug.parsing )
		return;

	MsgDev( D_INFO, "Last %i messages parsed.\n", MSG_COUNT );

	// finish here
	thecmd = cls_message_debug.currentcmd - 1;
	thecmd -= ( MSG_COUNT - 1 );	// back up to here

	for( i = 0; i < MSG_COUNT - 1; i++ )
	{
		thecmd &= CMD_MASK;
		old = &cls_message_debug.oldcmd[thecmd];

		MsgDev( D_INFO,"%i %04i %s\n", old->frame_number, old->starting_offset, CL_MsgInfo( old->command ));

		thecmd++;
	}

	failcommand = &cls_message_debug.oldcmd[thecmd];

	MsgDev( D_INFO, "BAD:  %3i:%s\n", msg->readcount - 1, CL_MsgInfo( failcommand->command ));

	if( host.developer >= 3 )
	{
		CL_WriteErrorMessage( msg->readcount - 1, msg );
	}
	cls_message_debug.parsing = false;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
bool CL_CheckOrDownloadFile( const char *filename )
{
	string	name;
	file_t	*f;

	if( FS_FileExists( filename ))
	{
		// it exists, no need to download
		return true;
	}

	com.strncpy( cls.downloadname, filename, MAX_STRING );
	com.strncpy( cls.downloadtempname, filename, MAX_STRING );

	// download to a temp name, and only rename to the real name when done,
	// so if interrupted a runt file won't be left
	FS_StripExtension( cls.downloadtempname );
	FS_DefaultExtension( cls.downloadtempname, ".tmp" );
	com.strncpy( name, cls.downloadtempname, MAX_STRING );

	f = FS_Open( name, "a+b" );
	if( f )
	{
		// it exists
		size_t	len = FS_Tell( f );

		cls.download = f;
		// give the server an offset to start the download
		MsgDev( D_INFO, "Resume download %s at %i\n", cls.downloadname, len );
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, va("download %s %i", cls.downloadname, len ));
	}
	else
	{
		MsgDev( D_INFO, "Start download %s\n", cls.downloadname );
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, va("download %s", cls.downloadname ));
	}

	cls.downloadnumber++;
	return false;
}

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload( sizebuf_t *msg )
{
	int		size, percent;
	string		name;
	int		r;

	// read the data
	size = MSG_ReadShort( msg );
	percent = MSG_ReadByte( msg );

	if( size == -1 )
	{
		Msg( "Server does not have this file.\n" );
		if( cls.download )
		{
			// if here, we tried to resume a file but the server said no
			FS_Close( cls.download );
			cls.download = NULL;
		}
		CL_RequestNextDownload();
		return;
	}

	// open the file if not opened yet
	if( !cls.download )
	{
		com.strncpy( name, cls.downloadtempname, MAX_STRING );
		cls.download = FS_Open ( name, "wb" );

		if( !cls.download )
		{
			msg->readcount += size;
			Msg( "Failed to open %s\n", cls.downloadtempname );
			CL_RequestNextDownload();
			return;
		}
	}

	FS_Write( cls.download, msg->data + msg->readcount, size );
	msg->readcount += size;

	if( percent != 100 )
	{
		// request next block
		Cvar_SetValue("scr_download", percent );
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_Print( &cls.netchan.message, "nextdl" );
	}
	else
	{
		FS_Close( cls.download );

		// rename the temp file to it's final name
		r = FS_Rename( cls.downloadtempname, cls.downloadname );
		if( r ) MsgDev( D_ERROR, "failed to rename.\n" );

		cls.download = NULL;
		Cvar_SetValue( "scr_download", 0.0f );

		// get another file if needed
		CL_RequestNextDownload();
	}
}

void CL_RunBackgroundTrack( void )
{
	string	intro, main, track;

	// run background track
	com.strncpy( track, cl.configstrings[CS_BACKGROUND_TRACK], MAX_STRING );
	com.snprintf( intro, MAX_STRING, "%s_intro", cl.configstrings[CS_BACKGROUND_TRACK] );
	com.snprintf( main, MAX_STRING, "%s_main", cl.configstrings[CS_BACKGROUND_TRACK] );

	if( FS_FileExists( va( "media/%s.ogg", intro )) && FS_FileExists( va( "media/%s.ogg", main )))
	{
		// combined track with introduction and main loop theme
		S_StartBackgroundTrack( intro, main );
	}
	else if( FS_FileExists( va( "media/%s.ogg", track )))
	{
		// single looped theme
		S_StartBackgroundTrack( track, track );
	}
	else if( !com.strcmp( track, "" ))
	{
		// blank name stopped last track
		S_StopBackgroundTrack();
	}
}

/*
==================
CL_ParseSoundPacket

==================
*/
void CL_ParseSoundPacket( sizebuf_t *msg, bool is_ambient )
{
	vec3_t	pos_;
	float	*pos = NULL;
	int 	chan, sound;
	float 	volume, attn;  
	int	flags, pitch, entnum;

	flags = MSG_ReadWord( msg );
	sound = MSG_ReadWord( msg );
	chan = MSG_ReadByte( msg );

	if( flags & SND_VOLUME )
		volume = MSG_ReadByte( msg ) / 255.0f;
	else volume = VOL_NORM;

	if( flags & SND_ATTENUATION )
		attn = MSG_ReadByte( msg ) / 64.0f;
	else attn = ATTN_NONE;	

	if( flags & SND_PITCH )
		pitch = MSG_ReadByte( msg );
	else pitch = PITCH_NORM;

	// entity reletive
	entnum = MSG_ReadWord( msg ); 

	// positioned in space
	if( flags & SND_FIXED_ORIGIN )
	{
		pos = pos_;
		MSG_ReadPos( msg, pos );
	}

	if( is_ambient )
	{
		S_AmbientSound( pos, entnum, chan, cl.sound_precache[sound], volume, attn, pitch, flags );
	}
	else
	{
		S_StartSound( pos, entnum, chan, cl.sound_precache[sound], volume, attn, pitch, flags );
	}
}

/*
==================
CL_ParseMovevars

==================
*/
void CL_ParseMovevars( sizebuf_t *msg )
{
	MSG_ReadDeltaMovevars( msg, &clgame.oldmovevars, &clgame.movevars );
	Mem_Copy( &clgame.oldmovevars, &clgame.movevars, sizeof( movevars_t ));
}

/*
==================
CL_ParseParticles

==================
*/
void CL_ParseParticles( sizebuf_t *msg )
{
	vec3_t		org, dir;
	int		i, count, color;
	
	MSG_ReadPos( msg, org );	

	for( i = 0; i < 3; i++ )
		dir[i] = MSG_ReadChar( msg ) * (1.0f / 16);

	count = MSG_ReadByte( msg );
	color = MSG_ReadByte( msg );
	if( count == 255 ) count = 1024;

	CL_ParticleEffect( org, dir, color, count );
}

/*
==================
CL_ParseStaticDecal

==================
*/
void CL_ParseStaticDecal( sizebuf_t *msg )
{
	vec3_t	origin;
	int	decalIndex, entityIndex, modelIndex;

	MSG_ReadPos( msg, origin );
	decalIndex = MSG_ReadWord( msg );
	entityIndex = MSG_ReadShort( msg );

	if( entityIndex > 0 )
		modelIndex = MSG_ReadWord( msg );

	CL_SpawnStaticDecal( origin, decalIndex, entityIndex, modelIndex );
}

void CL_ParseSoundFade( sizebuf_t *msg )
{
	float	fadePercent, fadeOutSeconds;
	float	holdTime, fadeInSeconds;

	fadePercent = MSG_ReadFloat( msg );
	fadeOutSeconds = MSG_ReadFloat( msg );
	holdTime = MSG_ReadFloat( msg );
	fadeInSeconds = MSG_ReadFloat( msg );

	S_FadeClientVolume( fadePercent, fadeOutSeconds, holdTime, fadeInSeconds );
}

void CL_ParseReliableEvent( sizebuf_t *msg, int flags )
{
	int		event_index;
	event_args_t	nullargs, args;
	float		delay;

	Mem_Set( &nullargs, 0, sizeof( nullargs ));
	event_index = MSG_ReadWord( msg );		// read event index
	delay = MSG_ReadWord( msg ) / 100.0f;		// read event delay
	MSG_ReadDeltaEvent( msg, &nullargs, &args );	// FIXME: zero-compressing

	CL_QueueEvent( flags, event_index, delay, &args );
}

void CL_ParseEvent( sizebuf_t *msg )
{
	int	i, num_events;

	num_events = MSG_ReadByte( msg );

	// parse events queue
	for( i = 0 ; i < num_events; i++ )
		CL_ParseReliableEvent( msg, 0 );
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/
/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData( sizebuf_t *msg )
{
	string	str;
	int	i;

	MsgDev( D_NOTE, "Serverdata packet received.\n" );

	// wipe the client_t struct
	CL_ClearState();
	UI_SetActiveMenu( UI_CLOSEMENU );
	cls.state = ca_connected;

	// parse protocol version number
	i = MSG_ReadLong( msg );
	cls.serverProtocol = i;

	if( i != PROTOCOL_VERSION )
		Host_Error( "Server use invalid protocol (%i should be %i)\n", i, PROTOCOL_VERSION );

	cl.servercount = MSG_ReadLong( msg );
	cl.playernum = MSG_ReadByte( msg );
	clgame.globals->maxClients = MSG_ReadByte( msg );
	clgame.globals->maxEntities = MSG_ReadWord( msg );
	com.strncpy( str, MSG_ReadString( msg ), MAX_STRING );
	com.strncpy( clgame.maptitle, MSG_ReadString( msg ), MAX_STRING );

	// no effect for local client
	// merge entcount only for remote clients 
	GI->max_edicts = clgame.globals->maxEntities;

	CL_InitEdicts (); // re-arrange edicts

	// get splash name
	Cvar_Set( "cl_levelshot_name", va( "levelshots/%s", str ));
	Cvar_SetValue( "scr_loading", 0.0f ); // reset progress bar

	// FIXME: Quake3 may be use both 'jpg' and 'tga' levelshot types
	if( !FS_FileExists( va( "%s.%s", cl_levelshot_name->string, SI->levshot_ext )) && cls.drawplaque ) 
	{
		Cvar_Set( "cl_levelshot_name", MAP_DEFAULT_SHADER );	// render a black screen
		cls.scrshot_request = scrshot_plaque;			// make levelshot
	}

	// seperate the printfs so the server message can have a color
	Msg("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
	Msg( "^2%s\n", clgame.maptitle );

	// need to prep refresh at next oportunity
	cl.video_prepped = false;
	cl.audio_prepped = false;

	// initialize world and clients
	CL_InitWorld ();
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline( sizebuf_t *msg )
{
	int		newnum;
	entity_state_t	nullstate;
	entity_state_t	*baseline;
	edict_t		*ent;

	Mem_Set( &nullstate, 0, sizeof( nullstate ));
	newnum = MSG_ReadWord( msg );

	if( newnum < 0 ) Host_Error( "CL_SpawnEdict: invalid number %i\n", newnum );
	if( newnum > clgame.globals->maxEntities ) Host_Error( "CL_AllocEdict: no free edicts\n" );

	// increase edicts
	while( newnum >= clgame.globals->numEntities )
		clgame.globals->numEntities++;

	ent = EDICT_NUM( newnum );
	if( ent->free ) CL_InitEdict( ent ); // initialize edict

	baseline = &clgame.baselines[newnum];
	MSG_ReadDeltaEntity( msg, &nullstate, baseline, newnum );
	CL_LinkEdict( ent, false ); // first entering, link always
}

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString( sizebuf_t *msg )
{
	int	i;

	i = MSG_ReadShort( msg );
	if( i < 0 || i >= MAX_CONFIGSTRINGS )
		Host_Error( "configstring > MAX_CONFIGSTRINGS\n" );
	com.strcpy( cl.configstrings[i], MSG_ReadString( msg ));
		
	// do something apropriate 
	if( i == CS_SKYNAME && cl.video_prepped )
	{
		re->RegisterShader( cl.configstrings[CS_SKYNAME], SHADER_SKY );
	}
	else if( i == CS_SERVERFLAGS )
	{
		// update shared serverflags
		clgame.globals->serverflags = com.atoi( cl.configstrings[CS_SERVERFLAGS] );
	}
	else if( i > CS_SERVERFLAGS && i < CS_MODELS )
	{
		Host_Error( "CL_ParseConfigString: reserved configstring #%i are used\n", i );
	}
	else if( i == CS_BACKGROUND_TRACK && cl.audio_prepped )
	{
		CL_RunBackgroundTrack();
	}
	else if( i >= CS_MODELS && i < CS_MODELS+MAX_MODELS && cl.video_prepped )
	{
		re->RegisterModel( cl.configstrings[i], i-CS_MODELS );
		CM_RegisterModel( cl.configstrings[i], i-CS_MODELS );
	}
	else if( i >= CS_SOUNDS && i < CS_SOUNDS+MAX_SOUNDS && cl.audio_prepped )
	{
		cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound( cl.configstrings[i] );
	}
	else if( i >= CS_DECALS && i < CS_DECALS+MAX_DECALS && cl.video_prepped )
	{
		cl.decal_shaders[i-CS_DECALS] = re->RegisterShader( cl.configstrings[i], SHADER_GENERIC );
	}
	else if( i >= CS_USER_MESSAGES && i < CS_USER_MESSAGES+MAX_USER_MESSAGES )
	{
		CL_LinkUserMessage( cl.configstrings[i], i - CS_USER_MESSAGES );
	}
	else if( i >= CS_EVENTS && i < CS_EVENTS+MAX_EVENTS )
	{
		CL_SetEventIndex( cl.configstrings[i], i - CS_EVENTS );
	}
	else if( i >= CS_CLASSNAMES && i < CS_CLASSNAMES+MAX_CLASSNAMES )
	{
		// edicts classnames for search by classname on client
		cl.edict_classnames[i-CS_CLASSNAMES] = MAKE_STRING( cl.configstrings[i] );
	}
	else if( i >= CS_LIGHTSTYLES && i < CS_LIGHTSTYLES+MAX_LIGHTSTYLES )
	{
		CL_SetLightstyle( i - CS_LIGHTSTYLES );
	}
}

/*
================
CL_ParseSetAngle

set the view angle to this absolute value
================
*/
void CL_ParseSetAngle( sizebuf_t *msg )
{
	cl.refdef.cl_viewangles[0] = MSG_ReadAngle32( msg );
	cl.refdef.cl_viewangles[1] = MSG_ReadAngle32( msg );
	cl.refdef.cl_viewangles[2] = MSG_ReadAngle32( msg );

	if( cl.refdef.cl_viewangles[0] > 180 ) cl.refdef.cl_viewangles[0] -= 360;
	if( cl.refdef.cl_viewangles[1] > 180 ) cl.refdef.cl_viewangles[1] -= 360;
	if( cl.refdef.cl_viewangles[2] > 180 ) cl.refdef.cl_viewangles[2] -= 360;
}

/*
================
CL_ParseAddAngle

add the view angle yaw
================
*/
void CL_ParseAddAngle( sizebuf_t *msg )
{
	float		ang_total;
	float		ang_final;
	float		apply_now;
	add_angle_t	*a;
	
	ang_total = MSG_ReadAngle32( msg );
	ang_final = MSG_ReadAngle32( msg );

	if( ang_total > 180.0f )
	{
		ang_total -= 360.0f;
	}

	if( ang_final > 180.0f )
	{
		ang_final -= 360.0f;
	}

	// apply this angle after prediction
	a = &cl.addangle[(cl.frame.serverframe) & CMD_MASK];
	a->yawdelta = ang_final;
	a->accum = 0.0f;

	apply_now = ang_total - ang_final;

	cl.refdef.cl_viewangles[1] += apply_now;
}
/*
================
CL_ParseCrosshairAngle

offset crosshair angles
================
*/
void CL_ParseCrosshairAngle( sizebuf_t *msg )
{
	cl.refdef.crosshairangle[0] = MSG_ReadAngle8( msg );
	cl.refdef.crosshairangle[1] = MSG_ReadAngle8( msg );
	cl.refdef.crosshairangle[2] = 0.0f; // not used for screen space
}

/*
================
CL_UpdateUserinfo

collect userinfo from all players
================
*/
void CL_UpdateUserinfo( sizebuf_t *msg )
{
	int		slot;
	bool		active;
	player_info_t	*player;

	slot = MSG_ReadByte( msg );

	if( slot >= MAX_CLIENTS )
		Host_Error( "CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS\n" );

	player = &cl.players[slot];
	active = MSG_ReadByte( msg ) ? true : false;

	if( active )
	{
		com.strncpy( player->userinfo, MSG_ReadString( msg ), sizeof( player->userinfo ));
		com.strncpy( player->name, Info_ValueForKey( player->userinfo, "name" ), sizeof( player->name ));
		com.strncpy( player->model, Info_ValueForKey( player->userinfo, "model" ), sizeof( player->model ));
	}
	else Mem_Set( player, 0, sizeof( *player ));
}

/*
==============
CL_ServerInfo

change serverinfo
==============
*/
void CL_ServerInfo( sizebuf_t *msg )
{
	char	key[MAX_MSGLEN];
	char	value[MAX_MSGLEN];

	com.strncpy( key, MSG_ReadString( msg ), sizeof( key ));
	com.strncpy( value, MSG_ReadString( msg ), sizeof( value ));
	Info_SetValueForKey( cl.serverinfo, key, value );
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/
/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage( sizebuf_t *msg )
{
	char	*s;
	int	i, cmd;
	int	bufStart;

	cls_message_debug.parsing = true;	// begin parsing
	starting_count = msg->readcount;	// updates each frame
	
	// parse the message
	while( 1 )
	{
		if( msg->error )
		{
			Host_Error( "CL_ParseServerMessage: bad server message\n" );
			return;
		}

		// mark start position
		bufStart = msg->readcount;

		cmd = MSG_ReadByte( msg );
		if( cmd == -1 ) break;

		// record command for debugging spew on parse problem
		CL_Parse_RecordCommand( cmd, bufStart );

		// other commands
		switch( cmd )
		{
		case svc_nop:
			MsgDev( D_ERROR, "CL_ParseServerMessage: user message out of bounds\n" );
			break;
		case svc_disconnect:
			CL_Drop ();
			Host_AbortCurrentFrame();
			break;
		case svc_changing:
			cls.drawplaque = false;
			Cmd_ExecuteString( "hud_changelevel\n" );
		case svc_reconnect:
			if( cls.drawplaque )
				Msg( "Server disconnected, reconnecting\n" );
			if( cls.download )
			{
				FS_Close( cls.download );
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
			break;
		case svc_stufftext:
			s = MSG_ReadString( msg );
			Cbuf_AddText( s );
			break;
		case svc_serverdata:
			Cbuf_Execute(); // make sure any stuffed commands are done
			CL_ParseServerData( msg );
			break;
		case svc_configstring:
			CL_ParseConfigString( msg );
			break;
		case svc_spawnbaseline:
			CL_ParseBaseline( msg );
			break;
		case svc_download:
			CL_ParseDownload( msg );
			break;
		case svc_sound:
			CL_ParseSoundPacket( msg, false );
			break;
		case svc_ambientsound:
			CL_ParseSoundPacket( msg, true );
			break;
		case svc_setangle:
			CL_ParseSetAngle( msg );
			break;
		case svc_addangle:
			CL_ParseAddAngle( msg );
			break;
		case svc_setview:
			cl.refdef.viewentity = MSG_ReadWord( msg );
			break;
		case svc_crosshairangle:
			CL_ParseCrosshairAngle( msg );
			break;
		case svc_physinfo:
			com.strncpy( cl.physinfo, MSG_ReadString( msg ), sizeof( cl.physinfo ));
			break;
		case svc_print:
			i = MSG_ReadByte( msg );
			if( i == PRINT_CHAT ) // chat
				S_StartLocalSound( "misc/talk.wav", 1.0f, 100, NULL );
			Con_Print( va( "^6%s\n", MSG_ReadString( msg )));
			break;
		case svc_centerprint:
			CL_CenterPrint( MSG_ReadString( msg ), 160, SMALLCHAR_WIDTH );
			break;
		case svc_setpause:
			cl.refdef.paused = (MSG_ReadByte( msg ) != 0 );
			break;
		case svc_movevars:
			CL_ParseMovevars( msg );
			break;
		case svc_particle:
			CL_ParseParticles( msg );
			break;
		case svc_bspdecal:
			CL_ParseStaticDecal( msg );
			break;
		case svc_soundfade:
			CL_ParseSoundFade( msg );
			break;
		case svc_event:
			CL_ParseEvent( msg );
			break;
		case svc_event_reliable:
			CL_ParseReliableEvent( msg, FEV_RELIABLE );
			break;
		case svc_updateuserinfo:
			CL_UpdateUserinfo( msg );
			break;
		case svc_serverinfo:
			CL_ServerInfo( msg );
			break;
		case svc_frame:
			CL_ParseFrame( msg );
			break;
		case svc_packetentities:
			Host_Error( "CL_ParseServerMessage: out of place frame data\n" );
			break;
		case svc_bad:
			Host_Error( "CL_ParseServerMessage: svc_bad\n" );
			break;
		default:
			CL_ParseUserMessage( msg, cmd );
			break;
		}
	}

	cls_message_debug.parsing = false;	// done
}