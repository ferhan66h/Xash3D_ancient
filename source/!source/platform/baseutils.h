//=======================================================================
//			Copyright XashXT Group 2007 �
//			baseutils.h - shared engine utility
//=======================================================================
#ifndef BASEUTILS_H
#define BASEUTILS_H

#include <time.h>

#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#define SYSTEM_SLASH_CHAR  '\\'

// Processor Information:
typedef struct cpuinfo_s
{
	bool m_bRDTSC	: 1;	// Is RDTSC supported?
	bool m_bCMOV	: 1;	// Is CMOV supported?
	bool m_bFCMOV	: 1;	// Is FCMOV supported?
	bool m_bSSE	: 1;	// Is SSE supported?
	bool m_bSSE2	: 1;	// Is SSE2 Supported?
	bool m_b3DNow	: 1;	// Is 3DNow! Supported?
	bool m_bMMX	: 1;	// Is MMX supported?
	bool m_bHT	: 1;	// Is HyperThreading supported?

	byte m_usNumLogicCore;	// Number op logical processors.
	byte m_usNumPhysCore;	// Number of physical processors

	int64 m_speed;		// In cycles per second.
	int   m_size;		// structure size
	char* m_szCPUID;		// Processor vendor Identification.
} cpuinfo_t;
cpuinfo_t GetCPUInformation( void );

//internal filesystem functions
void FS_Path (void);
void FS_InitEditor( void );
void FS_InitPath( char *path );
int FS_CheckParm (const char *parm);
void FS_LoadGameInfo( char *filename );
void FS_FileBase (char *in, char *out);
void FS_InitCmdLine( int argc, char **argv );
const char *FS_FileExtension (const char *in);
void FS_DefaultExtension (char *path, const char *extension );
bool FS_GetParmFromCmdLine( char *parm, char *out );


//files managment (like fopen, fread etc)
file_t *FS_Open (const char* filepath, const char* mode, bool quiet, bool nonblocking);
fs_offset_t FS_Write (file_t* file, const void* data, size_t datasize);
fs_offset_t FS_Read (file_t* file, void* buffer, size_t buffersize);
int FS_VPrintf(file_t* file, const char* format, va_list ap);
int FS_Seek (file_t* file, fs_offset_t offset, int whence);
int FS_Printf(file_t* file, const char* format, ...);
fs_offset_t FS_FileSize (const char *filename);
int FS_Print(file_t* file, const char *msg);
bool FS_FileExists (const char *filename);
int FS_UnGetc (file_t* file, byte c);
void FS_StripExtension (char *path);
fs_offset_t FS_Tell (file_t* file);
void FS_Purge (file_t* file);
int FS_Close (file_t* file);
int FS_Getc (file_t* file);
bool FS_Eof( file_t* file);

//virtual files managment
int VFS_OpenWrite (char *filename, int maxsize);
void VFS_Write( int hand, void *buf, long count );
int VFS_Seek( int hand, int ofs, int mode );
void VFS_Close( int hand );

void InitMemory (void); //must be init first at application
void FreeMemory( void );

void FS_Init( void );
void FS_Shutdown (void);

#define Mem_Alloc(pool, size) _Mem_Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Free(mem) _Mem_Free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) _Mem_AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) _Mem_FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _Mem_EmptyPool(pool, __FILE__, __LINE__)

//only for internal use
dword BuffBigLong (const byte *buffer);
word BuffBigShort (const byte *buffer);
dword BuffLittleLong (const byte *buffer);
word BuffLittleShort (const byte *buffer);

system_api_t gSysFuncs;
extern gameinfo_t GI;

#define Msg gSysFuncs.sys_msg
#define MsgDev gSysFuncs.sys_dev
#define Sys_Error gSysFuncs.sys_err

#define Malloc(size)	Mem_Alloc(basepool, size)  
#define Z_Malloc(size)	Mem_Alloc(zonepool, size)  
#define Free(mem)	 	Mem_Free(mem) 

extern char fs_rootdir[ MAX_SYSPATH ];	//root directory of engine
extern char fs_basedir[ MAX_SYSPATH ];	//base directory of game
extern char fs_gamedir[ MAX_SYSPATH ];	//game current directory
extern char token[ MAX_SYSPATH ];
extern char gs_mapname[ 64 ];
extern char gs_basedir[ MAX_SYSPATH ];
extern char g_TXcommand;
extern bool endofscript;


extern int fs_argc;
extern char **fs_argv;
extern byte *qccpool;
extern byte *studiopool;

//misc common functions
char *copystring(char *s);
char *strupr (char *start);
char *strlower (char *start);
char* FlipSlashes(char* string);
char *va(const char *format, ...);
char *stristr( const char *string, const char *string2 );
void ExtractFilePath(const char* const path, char* dest);
byte *ReadBMP (char *filename, byte **palette, int *width, int *height);
byte *ReadTGA (char *filename, byte **palette, int *width, int *height);

extern int numthreads;
void ThreadLock (void);
void ThreadUnlock (void);
void ThreadSetDefault (void);
void RunThreadsOn (int workcnt, bool showpacifier, void(*func)(int));
void RunThreadsOnIndividual (int workcnt, bool showpacifier, void(*func)(int));

_inline double I_FloatTime (void) { time_t t; time(&t); return t; }

//misc
bool MakeSprite ( void );
bool MakeModel  ( void );

#endif//BASEUTILS_H