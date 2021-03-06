//=======================================================================
//			Copyright XashXT Group 2007 ?
//		         net_msg.h - message io functions
//=======================================================================
#ifndef NET_MSG_H
#define NET_MSG_H

enum net_types_e
{
	NET_BAD = 0,
	NET_CHAR,
	NET_BYTE,
	NET_SHORT,
	NET_WORD,
	NET_LONG,
	NET_FLOAT,
	NET_ANGLE8,	// angle 2 char
	NET_ANGLE,	// angle 2 short
	NET_SCALE,
	NET_COORD,
	NET_COLOR,
	NET_TYPES,
};

typedef union
{
	float	f;
	long	l;
} ftol_t;

typedef struct net_desc_s
{
	int	type;	// pixelformat
	char	name[8];	// used for debug
	int	min_range;
	int	max_range;
} net_desc_t;

// communication state description
typedef struct net_field_s
{
	char	*name;
	int	offset;
	int	bits;
	bool	force;			// will be send for newentity
} net_field_t;

// server to client
enum svc_ops_e
{
	// user messages
	svc_bad = 0,		// don't send!

	// engine messages
	svc_nop = 201,		// end of user messages
	svc_disconnect,		// kick client from server
	svc_reconnect,		// reconnecting server request
	svc_stufftext,		// [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,		// [long] protocol ...
	svc_configstring,		// [short] [string]
	svc_spawnbaseline,		// valid only at spawn		
	svc_download,		// [short] size [size bytes]
	svc_changing,		// changelevel server request
	svc_physinfo,		// [physinfo string]
	svc_packetentities,		// [...]
	svc_frame,		// server frame
	svc_sound,		// <see code>
	svc_ambientsound,		// <see code>
	svc_setangle,		// [short short short] set the view angle to this absolute value
	svc_addangle,		// [short short] add angles when client turn on mover
	svc_setview,		// [short] entity number
	svc_print,		// [byte] id [string] null terminated string
	svc_centerprint,		// [string] to put in center of the screen
	svc_crosshairangle,		// [short][short][short]
	svc_setpause,		// [byte] 0 = unpaused, 1 = paused
	svc_movevars,		// [movevars_t]
	svc_particle,		// [float*3][char*3][byte][byte]
	svc_soundfade,		// [float*4] sound fade parms
	svc_bspdecal,		// [float*3][short][short][short]
	svc_event,		// playback event queue
	svc_event_reliable,		// playback event directly from message, not queue
	svc_updateuserinfo,		// [byte] playernum, [string] userinfo
	svc_serverinfo,		// [string] key [string] value
};

// client to server
enum clc_ops_e
{
	clc_bad = 0,

	// engine messages
	clc_nop = 201, 		
	clc_move,			// [[usercmd_t]
	clc_deltamove,		// [[usercmd_t]
	clc_userinfo,		// [[userinfo string]
	clc_stringcmd,		// [string] message
};

static const net_desc_t NWDesc[] =
{
{ NET_BAD,	"none",	0,		0	}, // full range
{ NET_CHAR,	"Char",	-128,		127	},
{ NET_BYTE,	"Byte",	0,		255	},
{ NET_SHORT,	"Short",	-32767,		32767	},
{ NET_WORD,	"Word",	0,		65535	},
{ NET_LONG,	"Long",	0,		0	}, // can't overflow
{ NET_FLOAT,	"Float",	0,		0	}, // can't overflow
{ NET_ANGLE8,	"Angle",	-360,		360	},
{ NET_ANGLE,	"Angle",	-360,		360	},
{ NET_SCALE,	"Scale",	-128,		127	},
{ NET_COORD,	"Coord",	-262140,		262140	},
{ NET_COLOR,	"Color",	0,		255	},
};

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#include "usercmd.h"
#include "event_api.h"
#include "pm_movevars.h"
#include "entity_state.h"

#define ES_FIELD( x )		#x,(int)&((entity_state_t*)0)->x
#define EV_FIELD( x )		#x,(int)&((event_args_t*)0)->x
#define PM_FIELD( x )		#x,(int)&((movevars_t*)0)->x
#define CM_FIELD( x )		#x,(int)&((usercmd_t*)0)->x

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most CS_SIZE characters.
#define CS_SIZE			64	// size of one config string
#define CS_TIME			16	// size of time string
#define CS_NAME			0	// map name
#define CS_MAPCHECKSUM		1	// level checksum (for catching cheater maps)
#define CS_SKYNAME			2	// skybox shader name
#define CS_BACKGROUND_TRACK		3	// basename of background track
#define CS_SERVERFLAGS		4	// shared server flags

// 5 - 32 it's a reserved strings

#define CS_MODELS			32				// configstrings starts here
#define CS_SOUNDS			(CS_MODELS+MAX_MODELS)		// sound names
#define CS_DECALS			(CS_SOUNDS+MAX_SOUNDS)		// server decal indexes
#define CS_EVENTS			(CS_DECALS+MAX_DECALS)		// queue events
#define CS_GENERICS			(CS_EVENTS+MAX_EVENTS)		// edicts classnames
#define CS_CLASSNAMES		(CS_GENERICS+MAX_GENERICS)		// generic resources (e.g. color decals)
#define CS_LIGHTSTYLES		(CS_CLASSNAMES+MAX_CLASSNAMES)	// lightstyle patterns
#define CS_USER_MESSAGES		(CS_LIGHTSTYLES+MAX_LIGHTSTYLES)	// names of user messages
#define MAX_CONFIGSTRINGS		(CS_USER_MESSAGES+MAX_USER_MESSAGES)	// total count

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
void MSG_Init( sizebuf_t *buf, byte *data, size_t length );
void MSG_Clear( sizebuf_t *buf );
void MSG_Print( sizebuf_t *msg, const char *data );
void _MSG_WriteBits( sizebuf_t *msg, long value, const char *name, int bits, const char *filename, const int fileline );
long _MSG_ReadBits( sizebuf_t *msg, const char *name, int bits, const char *filename, const int fileline );
void _MSG_Begin( int dest, const char *filename, int fileline );
void _MSG_WriteString( sizebuf_t *sb, const char *s, const char *filename, int fileline );
void _MSG_WriteFloat( sizebuf_t *sb, float f, const char *filename, int fileline );
void _MSG_WriteDouble( sizebuf_t *sb, double f, const char *filename, int fileline );
void _MSG_WriteAngle8( sizebuf_t *sb, float f, const char *filename, int fileline );
void _MSG_WriteAngle16( sizebuf_t *sb, float f, const char *filename, int fileline );
void _MSG_WriteCoord16( sizebuf_t *sb, float f, const char *filename, int fileline );
void _MSG_WritePos( sizebuf_t *sb, const vec3_t pos, const char *filename, int fileline );
void _MSG_WriteData( sizebuf_t *sb, const void *data, size_t length, const char *filename, int fileline );
void _MSG_WriteDeltaUsercmd( sizebuf_t *sb, usercmd_t *from, usercmd_t *cmd, const char *filename, const int fileline );
bool _MSG_WriteDeltaMovevars( sizebuf_t *sb, movevars_t *from, movevars_t *cmd, const char *filename, const int fileline );
void _MSG_WriteDeltaEvent( sizebuf_t *msg, event_args_t *from, event_args_t *to, const char *filename, const int fileline );
void _MSG_WriteDeltaEntity( entity_state_t *from, entity_state_t *to, sizebuf_t *msg, bool force, bool newentity, const char *file, int line );
bool _MSG_Send( int dest, const vec3_t origin, const edict_t *ent, bool direct, const char *filename, int fileline );

#define MSG_Begin( x ) _MSG_Begin( x, __FILE__, __LINE__)
#define MSG_WriteChar(x,y) _MSG_WriteBits (x, y, NWDesc[NET_CHAR].name, NET_CHAR, __FILE__, __LINE__)
#define MSG_WriteByte(x,y) _MSG_WriteBits (x, y, NWDesc[NET_BYTE].name, NET_BYTE, __FILE__, __LINE__)
#define MSG_WriteShort(x,y) _MSG_WriteBits(x, y, NWDesc[NET_SHORT].name, NET_SHORT,__FILE__, __LINE__)
#define MSG_WriteWord(x,y) _MSG_WriteBits (x, y, NWDesc[NET_WORD].name, NET_WORD, __FILE__, __LINE__)
#define MSG_WriteLong(x,y) _MSG_WriteBits (x, y, NWDesc[NET_LONG].name, NET_LONG, __FILE__, __LINE__)
#define MSG_WriteFloat(x,y) _MSG_WriteFloat(x, y, __FILE__, __LINE__)
#define MSG_WriteDouble(x,y) _MSG_WriteDouble(x, y, __FILE__, __LINE__)
#define MSG_WriteString(x,y) _MSG_WriteString (x, y, __FILE__, __LINE__)
#define MSG_WriteCoord16(x, y) _MSG_WriteCoord16(x, y, __FILE__, __LINE__)
#define MSG_WriteCoord32(x, y) _MSG_WriteFloat(x, y, __FILE__, __LINE__)
#define MSG_WriteAngle8(x, y) _MSG_WriteAngle8(x, y, __FILE__, __LINE__)
#define MSG_WriteAngle16(x, y) _MSG_WriteAngle16(x, y, __FILE__, __LINE__)
#define MSG_WriteAngle32(x, y) _MSG_WriteFloat(x, y, __FILE__, __LINE__)
#define MSG_WritePos(x, y) _MSG_WritePos( x, y, __FILE__, __LINE__ )
#define MSG_WriteData(x,y,z) _MSG_WriteData( x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaEvent(x, y, z) _MSG_WriteDeltaEvent(x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaUsercmd(x, y, z) _MSG_WriteDeltaUsercmd(x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaMovevars(x, y, z) _MSG_WriteDeltaMovevars(x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaEntity(from, to, msg, force, new ) _MSG_WriteDeltaEntity (from, to, msg, force, new, __FILE__, __LINE__)
#define MSG_WriteBits( buf, value, name, bits ) _MSG_WriteBits( buf, value, name, bits, __FILE__, __LINE__ )
#define MSG_ReadBits( buf, name, bits ) _MSG_ReadBits( buf, name, bits, __FILE__, __LINE__ )
#define MSG_Send(x, y, z) _MSG_Send(x, y, z, false, __FILE__, __LINE__)
#define MSG_DirectSend(x, y, z) _MSG_Send(x, y, z, true, __FILE__, __LINE__)

void MSG_BeginReading (sizebuf_t *sb);
#define MSG_ReadChar( x ) _MSG_ReadBits( x, NWDesc[NET_CHAR].name, NET_CHAR, __FILE__, __LINE__ )
#define MSG_ReadByte( x ) _MSG_ReadBits( x, NWDesc[NET_BYTE].name, NET_BYTE, __FILE__, __LINE__ )
#define MSG_ReadShort( x) _MSG_ReadBits( x, NWDesc[NET_SHORT].name, NET_SHORT, __FILE__, __LINE__ )
#define MSG_ReadWord( x ) _MSG_ReadBits( x, NWDesc[NET_WORD].name, NET_WORD, __FILE__, __LINE__ )
#define MSG_ReadLong( x ) _MSG_ReadBits( x, NWDesc[NET_LONG].name, NET_LONG, __FILE__, __LINE__ )
#define MSG_ReadCoord32( x ) MSG_ReadFloat( x )
#define MSG_ReadAngle32( x ) MSG_ReadFloat( x )
float MSG_ReadFloat( sizebuf_t *msg );
char *MSG_ReadString( sizebuf_t *sb );
float MSG_ReadAngle8( sizebuf_t *msg );
float MSG_ReadAngle16( sizebuf_t *msg );
float MSG_ReadCoord16( sizebuf_t *msg );
double MSG_ReadDouble( sizebuf_t *msg );
char *MSG_ReadStringLine( sizebuf_t *sb );
void MSG_ReadPos( sizebuf_t *sb, vec3_t pos );
void MSG_ReadData( sizebuf_t *sb, void *buffer, size_t size );
void MSG_ReadDeltaUsercmd( sizebuf_t *sb, usercmd_t *from, usercmd_t *cmd );
void MSG_ReadDeltaMovevars( sizebuf_t *sb, movevars_t *from, movevars_t *cmd );
void MSG_ReadDeltaEvent( sizebuf_t *msg, event_args_t *from, event_args_t *to );
void MSG_ReadDeltaEntity( sizebuf_t *sb, entity_state_t *from, entity_state_t *to, int number );

// huffman compression
void Huff_Init( void );
void Huff_CompressPacket( sizebuf_t *msg, int offset );
void Huff_DecompressPacket( sizebuf_t *msg, int offset );

/*
==============================================================

NET

==============================================================
*/
#define MAX_LATENT			32

typedef struct netchan_s
{
	bool			fatal_error;
	netsrc_t			sock;

	int			dropped;			// between last packet and previous
	bool			compress;			// enable huffman compression

	long			last_received;		// for timeouts
	long			last_sent;		// for retransmits

	int			drop_count;		// dropped packets, cleared each level
	int			good_count;		// cleared each level

	netadr_t			remote_address;
	int			qport;			// qport value to write when transmitting

	// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

	// reliable staging and holding areas
	sizebuf_t			message;				// writing buffer to send to server
	byte			message_buf[MAX_MSGLEN-16];		// leave space for header

	// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte			reliable_buf[MAX_MSGLEN-16];		// unacked reliable message

	// time and size data to calculate bandwidth
	int			outgoing_size[MAX_LATENT];
	long			outgoing_time[MAX_LATENT];

} netchan_t;

extern netadr_t		net_from;
extern sizebuf_t		net_message;
extern byte		net_message_buffer[MAX_MSGLEN];

#define PROTOCOL_VERSION	36
#define PORT_MASTER		27900
#define PORT_CLIENT		27901
#define PORT_SERVER		27910
#define MULTIPLAYER_BACKUP	64	// how many data slots to use when in multiplayer (must be power of 2)
#define SINGLEPLAYER_BACKUP	16	// same for single player   
#define MAX_FLAGS		32	// 32 bits == 32 flags
#define MASK_FLAGS		(MAX_FLAGS - 1)

void Netchan_Init( void );
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );
bool Netchan_NeedReliable( netchan_t *chan );
void Netchan_Transmit( netchan_t *chan, int length, byte *data );
void Netchan_OutOfBand( int net_socket, netadr_t adr, int length, byte *data );
void Netchan_OutOfBandPrint( int net_socket, netadr_t adr, char *format, ... );
bool Netchan_Process( netchan_t *chan, sizebuf_t *msg );

#endif//NET_MSG_H