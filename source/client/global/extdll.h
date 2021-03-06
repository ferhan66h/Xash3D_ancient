//=======================================================================
//			Copyright XashXT Group 2008 ?
//		 extdll.h - must be included into all client files
//=======================================================================

#ifndef EXTDLL_H
#define EXTDLL_H

// shut-up compiler warnings
#pragma warning(disable : 4305)	// int or float data truncation
#pragma warning(disable : 4201)	// nameless struct/union
#pragma warning(disable : 4514)	// unreferenced inline function removed
#pragma warning(disable : 4100)	// unreferenced formal parameter

#include "windows.h"
#include "basetypes.h"

// Misc C-runtime library headers
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// shared engine/DLL constants
#include "const.h"

// Vector class
#include "vector.h"

// Shared header describing protocol between engine and DLLs
#include "entity_def.h"
#include "entity_state.h"
#include "clgame_api.h"
#include "game_shared.h"

#endif//EXTDLL_H