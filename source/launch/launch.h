//=======================================================================
//			Copyright XashXT Group 2007 ?
//			launch.h - launch.dll main header
//=======================================================================
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <limits.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <winreg.h>
#include <math.h>

#define LAUNCH_DLL			// ignore alias names
#include "launch_api.h"

#define XASH_VERSION		0.70f // current version will be shared across gameinfo struct

#define MAX_NUM_ARGVS		128
#define MAX_STRING_TOKENS		80
#define LOG_QUEUE_SIZE		MAX_MSGLEN * 4	// 128 kb intermediate buffer

// just for last chanse to view message (debug only)
#define MSGBOX(x)			MessageBox(NULL, x, "Xash Error", MB_OK|MB_SETFOREGROUND|MB_ICONSTOP );

enum state_e
{
	SYS_SHUTDOWN = 0,
	SYS_RESTART,
	SYS_CRASH,
	SYS_ABORT,
	SYS_ERROR,
	SYS_FRAME,
};

typedef struct system_s
{
	char			progname[64];		// instance keyword
	char			fmessage[64];		// shutdown final message
	int			app_name;
	int			app_state;

	int			developer;
	int			printlevel;

	// log stuff
	bool			log_active;
	char			log_path[MAX_SYSPATH];
	FILE			*logfile;
	
	bool			hooked_out;
	bool			stuffcmdsrun;
	byte			packet_received[MAX_MSGLEN];	// network data
	int			msg_time;			// GetMessage time
	char			ModuleName[4096];		// exe.filename

	HANDLE			hMutex;
	HINSTANCE			hInstance;
	LPTOP_LEVEL_EXCEPTION_FILTER	oldFilter;
	dll_info_t		*linked_dll;

	char			caption[64];
	bool			con_readonly;
	bool			con_showalways;
	bool			con_showcredits;
	bool			con_silentmode;
	byte			*basepool;
	byte			*imagepool;
	byte			*soundpool;
	byte			*scriptpool;
	byte			*stringpool;
	bool			shutdown_issued;
	bool			error;

	// simply profiling
	double			start, end;

	void (*Con_Print)( const char *msg );
	void ( *Init ) ( int argc, char **argv );
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host
	void (*CPrint)( const char *msg ); // console print
	void (*CmdFwd)( void ); // forward to server
	void (*CmdAuto)( char *complete_string );
} system_t;

typedef struct timer_s
{
	bool	initialized;		// any timer can be setup it
	double	oldtime;
	double	curtime;
	dword	timebase;
} timer_t;

typedef struct cvar_s
{
	// shared part
	char	*name;
	char	*string;		// normal string
	float	value;		// atof( string )
	int	integer;		// atoi( string )
	bool	modified;		// set each time the cvar is changed

	// private to launch.dll
	char	*reset_string;	// cvar_restart will reset to this value
	char	*latched_string;	// for CVAR_LATCH vars
	char	*description;	// variable descrition info
	uint	flags;		// state flags
	uint	modificationCount;	// incremented each time the cvar is changed

	struct cvar_s *next;
	struct cvar_s *hash;
};

extern system_t Sys;
extern timer_t Msec;
extern sysinfo_t SI;
extern stdlib_api_t	com;

//
// console.c
//
void Con_ShowConsole( bool show );
void Con_PrintA(const char *pMsg);
void Con_PrintW(const char *pMsg);
void Con_CreateConsole( void );
void Con_DestroyConsole( void );
char *Sys_Input( void );
void Con_RegisterHotkeys( void );
void Con_DisableInput( void );

//
// system.c
//
void Sys_InitCPU( void );
gameinfo_t Sys_GameInfo( void );
void Sys_ParseCommandLine (LPSTR lpCmdLine);
void Sys_LookupInstance( void );
void Sys_NewInstance( const char *name, const char *fmsg );
double Sys_DoubleTime( void );
dword Sys_Milliseconds( void );
char *Sys_GetClipboardData( void );
char *Sys_GetCurrentUser( void );
bool Sys_GetModuleName( char *buffer, size_t length );
void Sys_Sleep( int msec );
void Sys_Init( void );
void Sys_Exit( void );
void Sys_Abort( void );
bool Sys_LoadLibrary( const char *dll_name, dll_info_t *dll );
void* Sys_GetProcAddress ( dll_info_t *dll, const char* name );
void Sys_ShellExecute( const char *path, const char *parms, bool exit );
byte *Sys_LoadRes( const char *filename, size_t *size );
bool Sys_FreeLibrary ( dll_info_t *dll );
void Sys_WaitForQuit( void );
void Sys_InitLog( void );
void Sys_CloseLog( void );
void Sys_Error(const char *error, ...);
void Sys_Break(const char *error, ...);
void Sys_PrintLog( const char *pMsg );
void Sys_Print(const char *pMsg);
void Sys_Msg( const char *pMsg, ... );
void Sys_MsgDev( int level, const char *pMsg, ... );
sys_event_t Sys_GetEvent( void );
void Sys_QueEvent( int time, ev_type_t type, int value, int value2, int length, void *ptr );
int Sys_GetThreadWork( void );
void Sys_ThreadWorkerFunction( int threadnum );
void Sys_ThreadSetDefault( void );
void Sys_ThreadLock( void );
void Sys_ThreadUnlock( void );
int Sys_GetNumThreads( void );
void Sys_RunThreadsOnIndividual( int workcnt, bool showpacifier, void(*func)(int));
void Sys_RunThreadsOn( int workcnt, bool showpacifier, void(*func)(int));

#define Msg Sys_Msg
#define MsgDev Sys_MsgDev

// registry common tools
bool REG_GetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer);
bool REG_SetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer);

//
// network.c
//
void NET_Init( void );
void NET_Shutdown( void );
void NET_Sleep( int msec );
void NET_Config( bool net_enable );
bool NET_IsLocalAddress( netadr_t adr );
char *NET_AdrToString( const netadr_t a );
char *NET_BaseAdrToString( const netadr_t a );
bool NET_StringToAdr( const char *string, netadr_t *adr );
bool NET_CompareAdr( const netadr_t a, const netadr_t b );
bool NET_CompareBaseAdr( const netadr_t a, const netadr_t b );
bool NET_GetPacket( netsrc_t sock, netadr_t *from, sizebuf_t *msg );
void NET_SendPacket( netsrc_t sock, size_t length, const void *data, netadr_t to );

//
// stdlib.c
//
void com_strnupr(const char *in, char *out, size_t size_out);
void com_strupr(const char *in, char *out);
void com_strnlwr(const char *in, char *out, size_t size_out);
void com_strlwr(const char *in, char *out);
int com_strlen( const char *string );
int com_cstrlen( const char *string );
char com_toupper(const char in );
char com_tolower(const char in );
size_t com_strncat(char *dst, const char *src, size_t siz);
size_t com_strcat(char *dst, const char *src );
size_t com_strncpy(char *dst, const char *src, size_t siz);
size_t com_strcpy(char *dst, const char *src );
char *com_stralloc(byte *mempool, const char *s, const char *filename, int fileline);
bool com_isdigit( const char *str );
int com_atoi(const char *str);
float com_atof(const char *str);
void com_atov( float *vec, const char *str, size_t siz );
char *com_strchr(const char *s, char c);
char *com_strrchr(const char *s, char c);
int com_strnicmp(const char *s1, const char *s2, int n);
int com_stricmp(const char *s1, const char *s2);
int com_strncmp (const char *s1, const char *s2, int n);
int com_strcmp (const char *s1, const char *s2);
bool com_stricmpext( const char *s1, const char *s2 );
const char* com_timestamp( int format );
char *com_stristr( const char *string, const char *string2 );
char *com_strstr( const char *string, const char *string2 );
size_t com_strpack( byte *buffer, size_t pos, char *string, int n );
size_t com_strunpack( byte *buffer, size_t pos, char *string );
int com_vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args);
int com_vsprintf(char *buffer, const char *format, va_list args);
int com_snprintf(char *buffer, size_t buffersize, const char *format, ...);
int com_sprintf(char *buffer, const char *format, ...);
char *com_pretifymem( float value, int digitsafterdecimal );
char *va(const char *format, ...);
#define copystring( str ) com_stralloc( NULL, str, __FILE__, __LINE__)
#define copystring2( pool, str ) com_stralloc( pool, str, __FILE__, __LINE__)

//
// utils.c
//
long Com_RandomLong( long lMin, long lMax );
float Com_RandomFloat( float fMin, float fMax );
uint Com_HashKey( const char *string, uint hashSize );

//
// math.c
//
void Com_BuildSqrtTable( void );
void Com_BuildSinCosTable( void );

float com_sqrt( float x );
float amd_sqrt( float x );
float sse_sqrt( float x );

_inline float sqrtf( float x ) { return ((float)sqrt((double)x)); }
_inline float sinf( float x ) { return ((float)sin((double)x)); }
_inline float asinf( float x ) { return ((float)asin((double)x)); }
_inline float cosf( float x ) { return ((float)cos((double)x)); }
_inline float acosf( float x ) { return ((float)acos((double)x)); }
_inline float tanf( float x ) { return ((float)tan((double)x)); }
_inline float atan2f( float x, float y ) { return ((float)atan2((double)x,(double)y)); }
void SinCos( float radians, float *sine, float *cosine );

//
// memlib.c
//
void Memory_Init( void );
void Memory_Shutdown( void );
void Memory_Init_Commands( void );
void _mem_move(byte *poolptr, void **dest, void *src, size_t size, const char *filename, int fileline);
void *_mem_realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline);
void _com_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _crt_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _asm_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _mmx_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _amd_mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline);
void _com_mem_set(void *dest, int set, size_t size, const char *filename, int fileline);
void _crt_mem_set(void *dest, int set, size_t size, const char *filename, int fileline);
void _asm_mem_set(void* dest, int set, size_t size, const char *filename, int fileline);
void _mmx_mem_set(void* dest, int set, size_t size, const char *filename, int fileline);
void *_mem_alloc(byte *poolptr, size_t size, const char *filename, int fileline);
byte *_mem_allocpool(const char *name, const char *filename, int fileline);
void _mem_freepool(byte **poolptr, const char *filename, int fileline);
void _mem_emptypool(byte *poolptr, const char *filename, int fileline);
void _mem_free(void *data, const char *filename, int fileline);
byte *_mem_alloc_array( byte *poolptr, size_t recordsize, int count, const char *filename, int fileline );
void _mem_free_array( byte *arrayptr, const char *filename, int fileline );
void *_mem_alloc_array_element( byte *arrayptr, const char *filename, int fileline );
void _mem_free_array_element( byte *arrayptr, void *element, const char *filename, int fileline );
void *_mem_get_array_element( byte *arrayptr, size_t index );
size_t _mem_array_size( byte *arrayptr );
void _mem_check(const char *filename, int fileline);
bool _is_allocated( byte *poolptr, void *data );

#define Mem_Alloc(pool, size) _mem_alloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) _mem_realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Free(mem) _mem_free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) _mem_allocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) _mem_freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _mem_emptypool(pool, __FILE__, __LINE__)
#define Mem_Move(pool, dest, src, size ) _mem_move(pool, dest, src, size, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) com.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, val, size ) com.memset(dest, val, size, __FILE__, __LINE__)
#define Mem_CreateArray( p, s, n ) _mem_alloc_array( p, s, n, __FILE__, __LINE__)
#define Mem_RemoveArray( array ) _mem_free_array( array, __FILE__, __LINE__)
#define Mem_AllocElement( array ) _mem_alloc_array_element( array, __FILE__, __LINE__)
#define Mem_FreeElement( array, el ) _mem_free_array_element( array, el, __FILE__, __LINE__ )
#define Mem_GetElement( array, idx ) _mem_get_array_element( array, idx )
#define Mem_ArraySize( array ) _mem_array_size( array )
#define Mem_IsAllocated( mem ) _is_allocated( NULL, mem )
#define Mem_Check() _mem_check(__FILE__, __LINE__)
#define Mem_Pretify( x ) com_pretifymem(x, 3)
#define Malloc( size ) Mem_Alloc( Sys.basepool, size )

//
// filesystem.c
//
void FS_Init( void );
void FS_Path( void );
void FS_Shutdown( void );
void FS_InitEditor( void );
void FS_InitRootDir( char *path );
void FS_ClearSearchPath( void );
void FS_AllowDirectPaths( bool enable );
void FS_AddGameHierarchy( const char *dir, int flags );
int FS_CheckParm( const char *parm );
void FS_LoadGameInfo( const char *rootfolder );
void FS_FileBase( const char *in, char *out );
const char *FS_FileExtension( const char *in );
void FS_DefaultExtension( char *path, const char *extension );
bool FS_GetParmFromCmdLine( char *parm, char *out, size_t size );
void FS_ExtractFilePath( const char* const path, char* dest );
void FS_UpdateEnvironmentVariables( void );
const char *FS_FileWithoutPath( const char *in );
extern char sys_rootdir[];
extern char *fs_argv[];
extern int fs_argc;

// wadsystem.c
wfile_t *W_Open( const char *filename, const char *mode );
byte *W_LoadLump( wfile_t *wad, const char *lumpname, size_t *lumpsizeptr, const char type );
fs_offset_t W_SaveLump( wfile_t *wad, const char *lump, const void* data, size_t datasize, char type, char cmp );
int W_Check( const char *filename );
void W_Close( wfile_t *wad );

// simply files managment interface
byte *FS_LoadFile (const char *path, fs_offset_t *filesizeptr );
bool FS_WriteFile (const char *filename, const void *data, fs_offset_t len);

// imagelib exports
rgbdata_t *FS_LoadImage( const char *name, const byte *buffer, size_t size );
bool Image_Process( rgbdata_t **pix, int w, int h, uint flags );
bool FS_SaveImage( const char *name, rgbdata_t *image );
bpc_desc_t *Image_GetPixelFormat( pixformat_t type );
void FS_FreeImage( rgbdata_t *pack );

// soundlib exports
wavdata_t *FS_LoadSound( const char *name, const byte *buffer, size_t size );
bool Sound_Process( wavdata_t **wav, int rate, int width, uint flags );
bool FS_SaveSound( const char *name, wavdata_t *sound );
void FS_FreeSound( wavdata_t *pack );

stream_t *FS_OpenStream( const char *filename );
wavdata_t *FS_StreamInfo( stream_t *stream );
long FS_ReadStream( stream_t *stream, int bytes, void *buffer );
void FS_FreeStream( stream_t *stream );

search_t *FS_Search(const char *pattern, int caseinsensitive );
search_t *FS_SearchDirs(const char *pattern, int caseinsensitive );

// files managment (like fopen, fread etc)
file_t *FS_Open( const char *filepath, const char *mode );
file_t* _FS_Open( const char *filepath, const char *mode, bool quiet );
fs_offset_t FS_Write( file_t *file, const void *data, size_t datasize );
fs_offset_t FS_Read( file_t *file, void *buffer, size_t buffersize );
int FS_VPrintf( file_t *file, const char *format, va_list ap );
int FS_Seek( file_t *file, fs_offset_t offset, int whence );
int FS_Gets( file_t *file, byte *string, size_t bufsize );
int FS_Printf( file_t *file, const char *format, ... );
fs_offset_t FS_FileSize( const char *filename );
fs_offset_t FS_FileTime( const char *filename );
int FS_Print( file_t *file, const char *msg );
bool FS_Rename( const char *oldname, const char *newname );
bool FS_FileExists( const char *filename );
bool FS_Delete( const char *path );
int FS_UnGetc( file_t *file, byte c );
void FS_StripExtension( char *path );
fs_offset_t FS_Tell( file_t *file );
bool FS_Eof( file_t *file );
void FS_Purge( file_t *file );
int FS_Close( file_t *file );
int FS_Getc( file_t *file );
bool FS_Eof( file_t *file );

//
// cvar.c
//
cvar_t *Cvar_FindVar( const char *var_name );
cvar_t *Cvar_Get( const char *var_name, const char *value, int flags, const char *description );
void Cvar_Set( const char *var_name, const char *value );
cvar_t *Cvar_Set2( const char *var_name, const char *value, bool force );
void Cvar_LookupVars( int checkbit, void *buffer, void *ptr, setpair_t callback );
void Cvar_FullSet (char *var_name, char *value, int flags );
void Cvar_SetLatched( const char *var_name, const char *value );
void Cvar_SetValue( const char *var_name, float value );
float Cvar_VariableValue( const char *var_name );
int Cvar_VariableInteger( const char *var_name );
char *Cvar_VariableString( const char *var_name );
bool Cvar_Command( void );
void Cvar_WriteVariables( file_t *f );
void Cvar_Init( void );
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );
extern bool userinfo_modified;
char *Info_ValueForKey( char *s, char *key );
void Info_RemoveKey( char *s, char *key );
void Info_SetValueForKey( char *s, char *key, char *value );
bool Info_Validate( char *s );
void Info_Print( char *s );
extern cvar_t *cvar_vars;

//
// cmd.c
//
void Cbuf_Init( void );
void Cbuf_AddText( const char *text );
void Cbuf_InsertText( const char *text );
void Cbuf_ExecuteText( int exec_when, const char *text );
void Cbuf_Execute (void);
uint Cmd_Argc( void );
char *Cmd_Args( void );
char *Cmd_Argv( uint arg );
void Cmd_Init( void );
void Cmd_AddCommand( const char *cmd_name, xcommand_t function, const char *cmd_desc );
void Cmd_RemoveCommand( const char *cmd_name );
bool Cmd_Exists( const char *cmd_name );
void Cmd_LookupCmds( char *buffer, void *ptr, setpair_t callback );
bool Cmd_GetMapList( const char *s, char *completedname, int length );
bool Cmd_GetDemoList( const char *s, char *completedname, int length );
bool Cmd_GetMovieList( const char *s, char *completedname, int length );
void Cmd_TokenizeString( const char *text );
void Cmd_ExecuteString( const char *text );
void Cmd_ForwardToServer( void );

// virtual files managment
vfile_t *VFS_Create( const byte *buffer, size_t buffsize );
vfile_t *VFS_Open( file_t *handle, const char* mode );
fs_offset_t VFS_Write( vfile_t *file, const void *buf, size_t size );
fs_offset_t VFS_Read( vfile_t* file, void* buffer, size_t buffersize );
int VFS_Print( vfile_t* file, const char *msg );
int VFS_Printf( vfile_t* file, const char* format, ... );
int VFS_Seek( vfile_t *file, fs_offset_t offset, int whence );
int VFS_Gets( vfile_t *file, byte *string, size_t bufsize );
bool VFS_Unpack( void *compbuf, size_t compsize, void **buf, size_t size );
byte *VFS_GetBuffer( vfile_t *file );
fs_offset_t VFS_Tell (vfile_t *file );
file_t *VFS_Close( vfile_t *file );
bool VFS_Eof( vfile_t *file );

//
// crclib.c
//
void CRC_Init( CRC16_t *crcvalue );
CRC16_t CRC_Block( byte *start, int count );
void CRC_ProcessByte( CRC16_t *crcvalue, byte data );
byte CRC_BlockSequence( byte *base, int length, int sequence );
uint Com_BlockChecksum( void *buffer, int length );
uint Com_BlockChecksumKey( void *buffer, int length, int key );
void CRC32_ProcessBuffer( CRC32_t *pulCRC, const void *pBuffer, int nBuffer );
void CRC32_ProcessByte( CRC32_t *pulCRC, byte ch );
void CRC32_Init( CRC32_t *pulCRC );
void CRC32_Final( CRC32_t *pulCRC );

//
// parselib.c
//
typedef struct
{
	const char	*name;
	uint		type;
} punctuation_t;

typedef struct script_s
{
	// shared part of script
	char		*buffer;
	char		*text;
	size_t		size;
	char		TXcommand;	// (quark .map comment)

	// private part of script
	string		name;
	int		line;
	bool		allocated;
	punctuation_t	*punctuations;
	bool		tokenAvailable;
	token_t		token;
};

bool PS_ReadToken( script_t *script, scFlags_t flags, token_t *token );
void PS_SaveToken( script_t *script, token_t *token );
bool PS_GetString( script_t *script, int flags, char *value, size_t size );
bool PS_GetDouble( script_t *script, int flags, double *value );
bool PS_GetFloat( script_t *script, int flags, float *value );
bool PS_GetUnsigned( script_t *script, int flags, uint *value );
bool PS_GetInteger( script_t *script, int flags, int *value );

void PS_SkipWhiteSpace( script_t *script );
void PS_SkipRestOfLine( script_t *script );
void PS_SkipBracedSection( script_t *script, int depth );

void PS_ScriptError( script_t *script, scFlags_t flags, const char *fmt, ... );
void PS_ScriptWarning( script_t *script, scFlags_t flags, const char *fmt, ... );

bool PS_MatchToken( token_t *token, const char *keyword );
void PS_SetPunctuationsTable( script_t *script, punctuation_t *punctuationsTable );
void PS_ResetScript( script_t *script );
bool PS_EndOfScript( script_t *script );

script_t	*PS_LoadScript( const char *filename, const char *buf, size_t size );
void	PS_FreeScript( script_t *script );

//
// parselib.c
//
void Patch_Evaluate( const float *p, int *numcp, const int *tess, float *dest, int comp );
void Patch_GetFlatness( float maxflat, const float *points, int comp, const int *patch_cp, int *flat );

//
// imglib.c
//
void Image_Init( void );
void Image_Setup( const char *formats, const uint flags );
void Image_Shutdown( void );

//
// sndlib.c
//
void Sound_Init( void );
void Sound_Setup( const char *formats, const uint flags );
void Sound_Shutdown( void );

//
// stringtable.c
//
int StringTable_CreateNewSystem( const char *name, size_t max_strings );
string_t StringTable_SetString( int handle, const char *string );
const char *StringTable_GetString( int handle, string_t index );
int StringTable_LoadSystem( wfile_t *wad, const char *name );
bool StringTable_SaveSystem( int h, wfile_t *wad );
const char *StringTable_GetName( int handle );
void StringTable_DeleteSystem( int handle );
void StringTable_ClearSystem( int handle );
void StringTable_Info_f( void );

#endif//LAUNCHER_H