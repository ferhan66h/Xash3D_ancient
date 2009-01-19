//=======================================================================
//			Copyright XashXT Group 2006 �
//			basetypes.h - general typedefs
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned __int64	qword;
typedef unsigned int	uint;
typedef signed __int64	int64;
typedef int		func_t;
typedef int		sound_t;
typedef int		model_t;
typedef int		string_t;
typedef int		shader_t;
typedef struct edict_s	edict_t;
typedef struct cl_priv_s	cl_priv_t;
typedef struct sv_priv_s	sv_priv_t;
typedef float		vec_t;

#define DLLEXPORT		__declspec( dllexport )

#ifndef NULL
#define NULL		((void *)0)
#endif

#ifndef BIT
#define BIT( n )		(1<<( n ))
#endif

#endif//BASETYPES_H