//=======================================================================
//			Copyright XashXT Group 2008 �
//		        cl_progs.c - client.dat interface
//=======================================================================

#include "common.h"
#include "client.h"

/*
================
CL_FadeColor
================
*/
float *CL_FadeColor( float starttime, float endtime )
{
	static vec4_t	color;
	float		time, fade_time;

	if( starttime == 0 ) return NULL;
	time = (cls.realtime * 0.001f) - starttime;
	if( time >= endtime ) return NULL;

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 0.3f, fade_time, 10.0f );

	// fade out
	if((endtime - time) < fade_time)
		color[3] = (endtime - time) * 1.0f / fade_time;
	else color[3] = 1.0;
	color[0] = color[1] = color[2] = 1.0f;

	return color;
}

void CL_DrawHUD( void )
{
	CL_VM_Begin();

	// setup pparms
	prog->globals.cl->health = cl.frame.ps.health;
	prog->globals.cl->maxclients = com.atoi(cl.configstrings[CS_MAXCLIENTS]);
	prog->globals.cl->max_entities = host.max_edicts;
	prog->globals.cl->realtime = cls.realtime * 0.001f;
	prog->globals.cl->paused = cl_paused->integer;

	// setup args
	PRVM_G_FLOAT(OFS_PARM0) = (float)cls.state;
	PRVM_ExecuteProgram (prog->globals.cl->HUD_Render, "HUD_Render");
}

bool CL_ParseUserMessage( int svc_number )
{
	bool	msg_parsed = false;

	// setup pparms
	prog->globals.cl->health = cl.frame.ps.health;
	prog->globals.cl->maxclients = com.atoi(cl.configstrings[CS_MAXCLIENTS]);
	prog->globals.cl->realtime = cls.realtime * 0.001f;
	prog->globals.cl->paused = cl_paused->integer;

	// setup args
	PRVM_G_FLOAT(OFS_PARM0) = (float)svc_number;
	PRVM_ExecuteProgram (prog->globals.cl->HUD_ParseMessage, "HUD_ParseMessage");
	msg_parsed = (bool)PRVM_G_FLOAT(OFS_RETURN);

	return msg_parsed; 
}

/*
====================
StudioEvent

Event callback for studio models
====================
*/
void CL_StudioEvent ( dstudioevent_t *event, entity_state_t *ent )
{
	// setup args
	PRVM_G_FLOAT(OFS_PARM0) = (float)event->event;
	PRVM_G_INT(OFS_PARM1) = PRVM_SetEngineString( event->options );
	VectorCopy( ent->origin, PRVM_G_VECTOR(OFS_PARM2));
	VectorCopy( ent->angles, PRVM_G_VECTOR(OFS_PARM3));
	PRVM_ExecuteProgram( prog->globals.cl->HUD_StudioEvent, "HUD_StudioEvent");
}

/*
====================
GetClientEntity

Render callback for studio models
====================
*/
entity_state_t *CL_GetEdictByIndex( int index )
{
	return &PRVM_EDICT_NUM( index )->priv.cl->current;
}

entity_state_t *CL_GetLocalPlayer( void )
{
	return &PRVM_EDICT_NUM( cl.playernum + 1 )->priv.cl->current;
}

int CL_GetMaxClients( void )
{
	return com.atoi(cl.configstrings[CS_MAXCLIENTS]);
}

/*
===============================================================================
Client Builtin Functions

mathlib, debugger, and various misc helpers
===============================================================================
*/
void CL_BeginIncreaseEdicts( void )
{
	int		i;
	pr_edict_t		*ent;

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
	}
}

void CL_EndIncreaseEdicts( void )
{
	int		i;
	pr_edict_t		*ent;

	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
	}
}

void CL_InitEdict( pr_edict_t *e )
{
	e->priv.cl->serialnumber = PRVM_NUM_FOR_EDICT(e);
	e->priv.cl->free = false;
}

void CL_FreeEdict( pr_edict_t *ed )
{
	ed->priv.cl->freetime = cl.time * 0.001f;
	ed->priv.cl->free = true;

	ed->progs.cl->model = 0;
	ed->progs.cl->modelindex = 0;
	ed->progs.cl->soundindex = 0;
	ed->progs.cl->skin = 0;
	ed->progs.cl->frame = 0;
	VectorClear(ed->progs.cl->origin);
	VectorClear(ed->progs.cl->angles);
}

void CL_FreeEdicts( void )
{
	int	i;
	pr_edict_t	*ent;

	CL_VM_Begin();
	for( i = 1; prog && i < prog->num_edicts; i++ )
	{
		ent = PRVM_EDICT_NUM(i);
		CL_FreeEdict( ent );
	}
	CL_VM_End();
}

void CL_CountEdicts( void )
{
	pr_edict_t	*ent;
	int	i, active = 0, models = 0;

	for (i = 0; i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.cl->free) continue;
		active++;
		if (ent->progs.cl->model) models++;
	}

	Msg("num_edicts:%3i\n", prog->num_edicts);
	Msg("active    :%3i\n", active);
	Msg("view      :%3i\n", models);
}

void CL_VM_Begin( void )
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_CLIENTPROG );

	if( prog ) *prog->time = cl.time * 0.001f;
}

void CL_VM_End( void )
{
	PRVM_End;
}

bool CL_LoadEdict( pr_edict_t *ent )
{
	return true;
}

static void PF_BeginRead( void )
{
}

static void PF_EndRead( void )
{
}

static void PF_ReadChar (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadChar( cls.multicast ); }
static void PF_ReadByte (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadByte( cls.multicast ); }
static void PF_ReadShort (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadShort( cls.multicast ); }
static void PF_ReadLong (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadLong( cls.multicast ); }
static void PF_ReadFloat (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadFloat( cls.multicast ); }
static void PF_ReadAngle (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadBits( cls.multicast, NET_FLOAT ); }
static void PF_ReadCoord (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadBits( cls.multicast, NET_FLOAT ); }
static void PF_ReadString (void){ PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( MSG_ReadString( cls.multicast) ); }
static void PF_ReadEntity (void){ VM_RETURN_EDICT( PRVM_PROG_TO_EDICT( MSG_ReadShort( cls.multicast ))); } // entindex

/*
=========
PF_drawnet

void DrawNet( vector pos, string image )
=========
*/
static void PF_drawnet( void )
{
	float	*pos;
	shader_t	shader;

	if(!VM_ValidateArgs( "DrawNet", 2 ))
		return;
	if(cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP-1)
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));
	pos = PRVM_G_VECTOR(OFS_PARM0);
	shader = re->RegisterShader( PRVM_G_STRING(OFS_PARM1), SHADER_NOMIP ); 

	SCR_DrawPic( pos[0], pos[1], -1, -1, shader );
}

/*
=========
PF_drawfps

void DrawFPS( vector pos )
=========
*/
static void PF_drawfps( void )
{
	float		calc;
	static double	nexttime = 0, lasttime = 0;
	static double	framerate = 0;
	static long	framecount = 0;
	double		newtime;
	bool		red = false; // fps too low
	char		fpsstring[32];
	float		*color, *pos;

	if(cls.state != ca_active) return; 
	if(!cl_showfps->integer) return;
	if(!VM_ValidateArgs( "drawfps", 1 ))
		return;
	
	newtime = Sys_DoubleTime();
	if( newtime >= nexttime )
	{
		framerate = framecount / (newtime - lasttime);
		lasttime = newtime;
		nexttime = max(nexttime + 1, lasttime - 1);
		framecount = 0;
	}
	framecount++;
	calc = framerate;
	pos = PRVM_G_VECTOR(OFS_PARM0);

	if ((red = (calc < 1.0f)))
	{
		com.snprintf(fpsstring, sizeof(fpsstring), "%4i spf", (int)(1.0f / calc + 0.5));
		color = g_color_table[1];
	}
	else
	{
		com.snprintf(fpsstring, sizeof(fpsstring), "%4i fps", (int)(calc + 0.5));
		color = g_color_table[3];
          }
	SCR_DrawBigStringColor(pos[0], pos[1], fpsstring, color );
}

/*
=========
PF_drawcenterprint

void DrawCenterPrint( void )
=========
*/
static void PF_drawcenterprint( void )
{
	char	*start;
	int	l, x, y, w;
	float	*color;

	if(!cl.centerPrintTime ) return;
	if(!VM_ValidateArgs( "DrawCenterPrint", 0 ))
		return;

	color = CL_FadeColor( cl.centerPrintTime * 0.001f, scr_centertime->value );
	if( !color ) 
	{
		cl.centerPrintTime = 0;
		return;
	}

	re->SetColor( color );
	start = cl.centerPrint;
	y = cl.centerPrintY - cl.centerPrintLines * BIGCHAR_HEIGHT/2;

	while( 1 )
	{
		char linebuffer[1024];

		for ( l = 0; l < 50; l++ )
		{
			if ( !start[l] || start[l] == '\n' )
				break;
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = cl.centerPrintCharWidth * com.cstrlen( linebuffer );
		x = ( SCREEN_WIDTH - w )>>1;

		SCR_DrawStringExt( x, y, cl.centerPrintCharWidth, BIGCHAR_HEIGHT, linebuffer, color, false );

		y += cl.centerPrintCharWidth * 1.5;
		while ( *start && ( *start != '\n' )) start++;
		if( !*start ) break;
		start++;
	}
	re->SetColor( NULL );
}

/*
=========
PF_centerprint

void HUD_CenterPrint( string text, float y, float charwidth )
=========
*/
static void PF_centerprint( void )
{
	float		y, width;
	const char	*text;
	char		*s;

	if(!VM_ValidateArgs( "HUD_CenterPrint", 3 ))
		return;

	text = PRVM_G_STRING(OFS_PARM0);
	y = PRVM_G_FLOAT(OFS_PARM1);
	width = PRVM_G_FLOAT(OFS_PARM2);
	VM_ValidateString( text );

	com.strncpy( cl.centerPrint, text, sizeof(cl.centerPrint));

	cl.centerPrintTime = cls.realtime;
	cl.centerPrintY = y;
	cl.centerPrintCharWidth = width;

	// count the number of lines for centering
	cl.centerPrintLines = 1;
	s = cl.centerPrint;
	while( *s )
	{
		if (*s == '\n') cl.centerPrintLines++;
		s++;
	}
}

/*
=========
PF_levelshot

float HUD_MakeLevelShot( void )
=========
*/
static void PF_levelshot( void )
{
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if(!VM_ValidateArgs( "HUD_MakeLevelShot", 0 ))
		return;
	
	if( cl.make_levelshot )
	{
		Con_ClearNotify();
		cl.make_levelshot = false;

		// make levelshot at nextframe()
		Cbuf_ExecuteText( EXEC_APPEND, "levelshot\n" );
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
=========
PF_setcolor

void HUD_SetColor( vector rgb, float alpha )
=========
*/
static void PF_setcolor( void )
{
	float	*rgb, alpha;

	if(!VM_ValidateArgs( "HUD_SetColor", 2 ))
		return;

	rgb = PRVM_G_VECTOR(OFS_PARM0);
	alpha = PRVM_G_FLOAT(OFS_PARM1);
	re->SetColor( GetRGBA( rgb[0], rgb[1], rgb[2], alpha ));
}

/*
=========
PF_startsound

void CL_StartSound( vector pos, entity e, float chan, float sfx, float vol, float attn, float pitch, float localsound )
=========
*/
static void PF_startsound( void )
{
	float 	volume;
	int 	channel;
	sound_t 	sound_num;
	int 	attenuation;
	float	*pos = NULL;
	bool	client_sound;
	int	ent = 0;
	float	pitch;

	if( !VM_ValidateArgs( "CL_StartSound", 8 ))
		return;

	pos = PRVM_G_VECTOR(OFS_PARM0);
	ent = PRVM_G_EDICTNUM(OFS_PARM1);
	channel = (int)PRVM_G_FLOAT(OFS_PARM2);
	sound_num = (sound_t)PRVM_G_FLOAT(OFS_PARM3);
	volume = PRVM_G_FLOAT(OFS_PARM4);
	attenuation = (int)PRVM_G_FLOAT(OFS_PARM5);
	pitch = (int)PRVM_G_FLOAT(OFS_PARM6);
	client_sound = (bool)PRVM_G_FLOAT(OFS_PARM7);

	if( client_sound )
	{
		S_StartSound( pos, ent, channel, sound_num, volume, attenuation, pitch );
	}
	else if( cl.sound_precache[sound_num] )
	{
		S_StartSound( pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, pitch );
	}
	else VM_Warning( "CL_StartSound: can't play sound with index %i\n", sound_num );
}

/*
=========
PF_pointcontents

float CL_PointContents( vector point )
=========
*/
static void PF_pointcontents( void )
{
	if( !VM_ValidateArgs( "CL_PointContents", 1 ))
		return;
	PRVM_G_FLOAT(OFS_RETURN) = CL_PointContents(PRVM_G_VECTOR(OFS_PARM0));	
}

/*
=========
PF_startsound

sound_t CL_PrecacheSound( string samp )
=========
*/
static void PF_precachesound( void )
{
	if( !VM_ValidateArgs( "CL_PrecacheSound", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = S_RegisterSound(PRVM_G_STRING(OFS_PARM0));
}

/*
=================
PF_findexplosionplane

vector CL_FindExplosionPlane( vector org, float radius )
=================
*/
static void PF_findexplosionplane( void )
{
	static vec3_t	planes[6] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}};
	trace_t		trace;
	float		best = 1.0, radius;
	vec3_t		point, dir;
	const float	*org;
	int		i;

	if( !VM_ValidateArgs( "CL_FindExplosionPlane", 2 ))
		return;

	org = PRVM_G_VECTOR(OFS_PARM0);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	VectorClear( dir );

	for( i = 0; i < 6; i++ )
	{
		VectorMA( org, radius, planes[i], point );

		trace = CL_Trace( org, vec3_origin, vec3_origin, point, MOVE_WORLDONLY, NULL, MASK_SOLID );
		if( trace.allsolid || trace.fraction == 1.0 )
			continue;

		if( trace.fraction < best )
		{
			best = trace.fraction;
			VectorCopy( trace.plane.normal, dir );
		}
	}
	VectorCopy( dir, PRVM_G_VECTOR( OFS_RETURN ));
}

// NOTE: intervals between various "interfaces" was leave for future expansions
prvm_builtin_t vm_cl_builtins[] = 
{
NULL,				// #0  (leave blank as default, but can include easter egg ) 

// system events
VM_ConPrintf,			// #1  void Con_Printf( ... )
VM_ConDPrintf,			// #2  void Con_DPrintf( float level, ... )
VM_HostError,			// #3  void Com_Error( ... )
VM_SysExit,			// #4  void Sys_Exit( void )
VM_CmdArgv,			// #5  string Cmd_Argv( float arg )
VM_CmdArgc,			// #6  float Cmd_Argc( void )
NULL,				// #7  -- reserved --
NULL,				// #8  -- reserved --
NULL,				// #9  -- reserved --
NULL,				// #10 -- reserved --

// common tools
VM_ComTrace,			// #11 void Com_Trace( float enable )
VM_ComFileExists,			// #12 float Com_FileExists( string filename )
VM_ComFileSize,			// #13 float Com_FileSize( string filename )
VM_ComFileTime,			// #14 float Com_FileTime( string filename )
VM_ComLoadScript,			// #15 float Com_LoadScript( string filename )
VM_ComResetScript,			// #16 void Com_ResetScript( void )
VM_ComReadToken,			// #17 string Com_ReadToken( float newline )
VM_Random,			// #18 float Random( void )
VM_ComSearchFiles,			// #19 float Com_Search( string mask, float casecmp )
VM_ComSearchNames,			// #20 string Com_SearchFilename( float num )
VM_RandomLong,			// #21 float RandomLong( float min, float max )
VM_RandomFloat,			// #22 float RandomFloat( float min, float max )
VM_RandomVector,			// #23 vector RandomVector( vector min, vector max )
VM_CvarRegister,			// #24 void Cvar_Register( string name, string value, float flags )
VM_CvarSetValue,			// #25 void Cvar_SetValue( string name, float value )
VM_CvarGetValue,			// #26 float Cvar_GetValue( string name )
VM_CvarSetString,			// #27 void Cvar_SetString( string name, string value )
VM_CvarGetString,			// #28 void VM_CvarGetString( void )
VM_ComVA,				// #29 string va( ... )
VM_ComStrlen,			// #30 float strlen( string text )
VM_TimeStamp,			// #31 string Com_TimeStamp( float format )
VM_LocalCmd,			// #32 void LocalCmd( ... )
VM_SubString,			// #33 string substring( string s, float start, float length )
VM_AddCommand,			// #34 void Add_Command( string s )
VM_atof,				// #35 float atof( string s )
VM_atoi,				// #36 float atoi( string s )
VM_atov,				// #37 vector atov( string s )
NULL,				// #38 -- reserved --
NULL,				// #39 -- reserved --
NULL,				// #40 -- reserved --

// quakec intrinsics ( compiler will be lookup this functions, don't remove or rename )
VM_SpawnEdict,			// #41 entity spawn( void )
VM_RemoveEdict,			// #42 void remove( entity ent )
VM_NextEdict,			// #43 entity nextent( entity ent )
VM_CopyEdict,			// #44 void copyentity( entity src, entity dst )
NULL,				// #45 -- reserved --
NULL,				// #46 -- reserved --
NULL,				// #47 -- reserved --
NULL,				// #48 -- reserved --
NULL,				// #49 -- reserved --
NULL,				// #50 -- reserved --

// filesystem
VM_FS_Open,			// #51 float fopen( string filename, float mode )
VM_FS_Close,			// #52 void fclose( float handle )
VM_FS_Gets,			// #53 string fgets( float handle )
VM_FS_Puts,			// #54 void fputs( float handle, string s )
NULL,				// #55 -- reserved --
NULL,				// #56 -- reserved --
NULL,				// #57 -- reserved --
NULL,				// #58 -- reserved --
NULL,				// #59 -- reserved --
NULL,				// #60 -- reserved --

// mathlib
VM_min,				// #61 float min(float a, float b )
VM_max,				// #62 float max(float a, float b )
VM_bound,				// #63 float bound(float min, float val, float max)
VM_pow,				// #64 float pow(float x, float y)
VM_sin,				// #65 float sin(float f)
VM_cos,				// #66 float cos(float f)
VM_tan,				// #67 float tan(float f)
VM_asin,				// #68 float asin(float f)
VM_acos,				// #69 float acos(float f)
VM_atan,				// #70 float atan(float f)
VM_sqrt,				// #71 float sqrt(float f)
VM_rint,				// #72 float rint (float v)
VM_floor,				// #73 float floor(float v)
VM_ceil,				// #74 float ceil (float v)
VM_fabs,				// #75 float fabs (float f)
VM_abs,				// #76 float abs (float f)
NULL,				// #77 -- reserved --
NULL,				// #78 -- reserved --
VM_VectorNormalize,			// #79 vector VectorNormalize( vector v )
VM_VectorLength,			// #80 float VectorLength( vector v )
e10, e10,				// #81 - #100 are reserved for future expansions

// network messaging
PF_BeginRead,			// #101 void MsgBegin( void )
PF_ReadByte,			// #102 float ReadByte( void )
PF_ReadChar,			// #103 float ReadChar( void )
PF_ReadShort,			// #104 float ReadShort( void )
PF_ReadLong,			// #105 float ReadLong( void )
PF_ReadFloat,			// #106 float ReadFloat( void )
PF_ReadAngle,			// #107 float ReadAngle( void )
PF_ReadCoord,			// #108 float ReadCoord( void )
PF_ReadString,			// #109 string ReadString( void )
PF_ReadEntity,			// #110 entity ReadEntity( void )
PF_EndRead,			// #111 void MsgEnd( void )

// clientfuncs_t
VM_precache_pic,			// #112 float precache_pic( string pic )
VM_drawcharacter,			// #113 float DrawChar( vector pos, float char, vector scale, vector rgb, float alpha )
VM_drawstring,			// #114 float DrawString( vector pos, string text, vector scale, vector rgb, float alpha )
VM_drawpic,			// #115 float DrawPic( vector pos, string pic, vector size, vector rgb, float alpha )
VM_drawfill,			// #116 void DrawFill( vector pos, vector size, vector rgb, float alpha )
VM_drawmodel,			// #117 void DrawModel( vector pos, vector size, string mod, vector org, vector ang, float seq )
NULL,				// #118 -- reserved -- 
VM_getimagesize,			// #119 vector getimagesize( string pic )
PF_drawnet,			// #120 void DrawNet( vector pos, string image )
PF_drawfps,			// #121 void DrawFPS( vector pos )
PF_drawcenterprint,			// #122 void DrawCenterPrint( void )
PF_centerprint,			// #123 void HUD_CenterPrint( string text, float y, float charwidth )
PF_levelshot,			// #124 float HUD_MakeLevelShot( void )
PF_setcolor,			// #125 void HUD_SetColor( vector rgb, float alpha )
VM_localsound,			// #126 void HUD_PlaySound( string sample )
PF_startsound,			// #127 void CL_StartSound( vector, entity, float, float, float, float, float, float )
PF_addparticle,			// #128 float AddParticle(vector, vector, vector, vector, vector, vector, vector, string, float)
PF_pointcontents,			// #129 float CL_PointContents( vector point )
PF_precachesound,			// #130 sound_t CL_PrecacheSound( string samp )
PF_adddecal,			// #131 void AddDecal( vector, vector, vector, float, float, float, float, string, float )
PF_addlight,			// #132 void AddLight( vector pos, vector col, float rad, float decay, float time, float key )
NULL,				// #133
NULL,				// #134
NULL,				// #135
NULL,				// #136
NULL,				// #137
PF_findexplosionplane,		// #138 vector CL_FindExplosionPlane( vector org, float radius )
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t); //num of builtins

void CL_InitClientProgs( void )
{
	Msg("\n");
	PRVM_Begin;

	PRVM_InitProg( PRVM_CLIENTPROG );
	if( !prog->loaded )
	{       
		prog->progs_mempool = Mem_AllocPool( "Client Progs" );
		prog->name = "client";
		prog->builtins = vm_cl_builtins;
		prog->numbuiltins = vm_cl_numbuiltins;
		prog->edictprivate_size = sizeof(cl_edict_t);
		prog->max_edicts = host.max_edicts<<2;
		prog->limit_edicts = host.max_edicts;
		prog->begin_increase_edicts = CL_BeginIncreaseEdicts;
		prog->end_increase_edicts = CL_EndIncreaseEdicts;
		prog->init_edict = CL_InitEdict;
		prog->free_edict = CL_FreeEdict;
		prog->count_edicts = CL_CountEdicts;
		prog->load_edict = CL_LoadEdict;
		prog->filecrc = PROG_CRC_CLIENT;

		// using default builtins
		prog->init_cmd = VM_Cmd_Init;
		prog->reset_cmd = VM_Cmd_Reset;
		prog->error_cmd = VM_Error;
		PRVM_LoadProgs( va("%s/client.dat", GI->vprogs_dir ));
	}

	// init some globals
	prog->globals.cl->realtime = cls.realtime * 0.001f;
	prog->globals.cl->pev = 0;
	prog->globals.cl->mapname = PRVM_SetEngineString( cls.servername );
	prog->globals.cl->playernum = cl.playernum;

	// call the prog init
	PRVM_ExecuteProgram( prog->globals.cl->HUD_Init, "HUD_Init" );
	PRVM_End;
}

void CL_FreeClientProgs( void )
{
	CL_VM_Begin();

	prog->globals.cl->realtime = cls.realtime * 0.001f;
	prog->globals.cl->pev = 0;
	PRVM_ExecuteProgram(prog->globals.cl->HUD_Shutdown, "HUD_Shutdown");
	PRVM_ResetProg();

	CL_VM_End();
}