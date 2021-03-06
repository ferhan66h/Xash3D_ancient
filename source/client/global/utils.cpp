//=======================================================================
//			Copyright XashXT Group 2008 ?
//		     utils.cpp - client game utilities code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "hud.h"

DLL_GLOBAL const Vector g_vecZero = Vector( 0.0f, 0.0f, 0.0f );

static const float bytedirs[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

#ifdef _DEBUG
void DBG_AssertFunction( BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage )
{
	if( fExpr ) return;

	char szOut[512];
	if( szMessage != NULL )
		sprintf( szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage );
	else sprintf( szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine );
	HOST_ERROR( szOut );
}
#endif	// DEBUG


Vector BitsToDir( int bits )
{
	Vector	dir;

	if( bits < 0 || bits >= NUMVERTEXNORMALS )
		return Vector( 0, 0, 0 );
	
	dir.x = bytedirs[bits][0];
	dir.y = bytedirs[bits][1];
	dir.z = bytedirs[bits][2];

	return dir;
}

// NOTE: modify these functions with caution

typedef struct
{
	char	*name;
	byte	*buf;
	int	size;
	int	read;
	BOOL	badRead;
	char	string[2048];	// using for store strings
} user_message_t;

static user_message_t gMsg;

void BEGIN_READ( const char *pszName, int iSize, void *pBuf )
{
	memset( &gMsg, 0, sizeof( gMsg ));

	gMsg.size = iSize;
	gMsg.buf = (byte *)pBuf;
}

void END_READ( void )
{
	if( gMsg.badRead )
	{
		ALERT( at_console, "%s was received with errors\n", gMsg.name );
	}
}

int READ_CHAR( void )
{
	int     c;
	
	if( gMsg.read + 1 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (signed char)gMsg.buf[gMsg.read];
	gMsg.read++;
	
	return c;
}

int READ_BYTE( void )
{
	int     c;
	
	if( gMsg.read+1 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (unsigned char)gMsg.buf[gMsg.read];
	gMsg.read++;
	
	return c;
}

int READ_SHORT( void )
{
	int     c;
	
	if( gMsg.read + 2 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (short)( gMsg.buf[gMsg.read] + ( gMsg.buf[gMsg.read+1] << 8 ));
	gMsg.read += 2;
	
	return c;
}

int READ_WORD( void ) { return READ_SHORT(); }

int READ_LONG( void )
{
	int     c;
	
	if( gMsg.read + 4 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
 	c = gMsg.buf[gMsg.read]+(gMsg.buf[gMsg.read+1]<<8)+(gMsg.buf[gMsg.read+2]<<16)+(gMsg.buf[gMsg.read+3]<<24);
	gMsg.read += 4;
	
	return c;
}

float READ_FLOAT( void )
{
	union { float f; int l; } dat;
	
	dat.l = READ_LONG();
	return dat.f;   
}

char* READ_STRING( void )
{
	int	l, c;

	gMsg.string[0] = 0;

	l = 0;
	do
	{
		if( gMsg.read+1 > gMsg.size ) break; // no more characters

		c = READ_CHAR();
		if( c == -1 || c == '\0' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';

		gMsg.string[l] = c;
		l++;
	} while( l < sizeof( gMsg.string ) - 1 );
	
	gMsg.string[l] = 0; // terminator

	return gMsg.string;
}

//
// Xash3D network specs. Don't modify!
//
float READ_COORD( void )
{
	return READ_FLOAT();
}

float READ_ANGLE( void )
{
	return (float)(READ_SHORT() * (360.0 / 65536));
}

Vector READ_DIR( void )
{
	return BitsToDir( READ_BYTE() );
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE) | (ignoreGlass?0x100:0), pentIgnore, ptr );
}


void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), pentIgnore, ptr );
}


void UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_HULL( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), hullNumber, pentIgnore, ptr );
}

void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, int hullNumber, edict_t *pentModel, TraceResult *ptr )
{
	g_engfuncs.pfnTraceModel( vecStart, vecEnd, pentModel, ptr );
}

/*
==============================================================================

		HUD-SPRITES PARSING

==============================================================================
*/
/*
====================
ParseHudSprite

====================
*/
void ParseHudSprite( const char **pfile, char *psz, client_sprite_t *result )
{
	int x = 0, y = 0, width = 0, height = 0;
	client_sprite_t p;
	int section = 0;
	char *token;
		
	memset( &p, 0, sizeof( client_sprite_t ));
          
	while(( token = COM_ParseToken( pfile )) != NULL )
	{
		if( !stricmp( token, psz ))
		{
			token = COM_ParseToken( pfile );
			if( !stricmp( token, "{" )) section = 1;
		}
		if( section ) // parse section
		{
			if( !stricmp( token, "}" )) break; // end section
			
			if( !stricmp( token, "file" )) 
			{                                          
				token = COM_ParseToken( pfile );
				strncpy( p.szSprite, token, 64 );

				// fill structure at default
				p.hSprite = SPR_Load( p.szSprite );
				width = SPR_Width( p.hSprite, 0 );
				height = SPR_Height( p.hSprite, 0 );
				x = y = 0;
			}
			else if ( !stricmp( token, "name" )) 
			{                                          
				token = COM_ParseToken( pfile );
				strncpy( p.szName, token, 64 );
			}
			else if ( !stricmp( token, "x" )) 
			{                                          
				token = COM_ParseToken( pfile );
				x = atoi( token );
			}
			else if ( !stricmp( token, "y" )) 
			{                                          
				token = COM_ParseToken( pfile );
				y = atoi( token );
			}
			else if ( !stricmp( token, "width" )) 
			{                                          
				token = COM_ParseToken( pfile );
				width = atoi( token );
			}
			else if ( !stricmp( token, "height" )) 
			{                                          
				token = COM_ParseToken( pfile );
				height = atoi( token );
			}
		}
	}

	if( !section ) return; // data not found

	// calculate sprite position
	p.rc.left = x;
	p.rc.right = x + width; 
	p.rc.top = y;
	p.rc.bottom = y + height;

	memcpy( result, &p, sizeof( client_sprite_t ));
}

client_sprite_t *SPR_GetList( const char *psz, int *piCount )
{
	char *pfile = (char *)LOAD_FILE( psz, NULL );
	int iSprCount = 0;

	if( !pfile )
	{
		*piCount = iSprCount;
		return NULL;
	}

	char *token;
	const char *plist = pfile;
	int depth = 0;

	while(( token = COM_ParseToken( &plist )) != NULL ) // calculate count of sprites
	{
		if( !stricmp( token, "{" )) depth++;
		else if( !stricmp( token, "}" )) depth--;
		else if( depth == 0 && !strcmp( token, "hudsprite" ))
			iSprCount++;
	}

	client_sprite_t *phud;
	plist = pfile;

	phud = new client_sprite_t[iSprCount];

	if( depth != 0 ) ALERT( at_console, "%s EOF without closing brace\n", psz );

	for( int i = 0; i < iSprCount; i++ ) //parse structures
	{
		ParseHudSprite( &plist, "hudsprite", &phud[i] );
	}

	if( !iSprCount ) ALERT( at_console, "SPR_GetList: %s doesn't have sprites\n", psz );
	FREE_FILE( pfile );
          
          *piCount = iSprCount;
	return phud;
}

/*
==============================================================================

		DRAW HELPERS

==============================================================================
*/
void Draw_VidInit( void )
{
}

void DrawPause( void )
{
	// pause image
	DrawImageBar( 100, "m_pause" ); // HACKHACK
}

void DrawImageBar( float percent, const char *szSpriteName )
{
	int m_loading = gHUD.GetSpriteIndex( szSpriteName );
	wrect_t rcSize = gHUD.GetSpriteRect( m_loading );

	int w = rcSize.right - rcSize.left;
	int h = rcSize.bottom - rcSize.top;
	DrawImageBar( percent, szSpriteName, (ScreenWidth - w)/2, (ScreenHeight - h)/2 );
}

void DrawImageBar( float percent, const char *szSpriteName, int x, int y )
{
	int	m_loading = gHUD.GetSpriteIndex( szSpriteName );
	HSPRITE	hLoading = gHUD.GetSprite( m_loading );
	wrect_t	rcBar, rcBack;
	float	step;

	rcBar = rcBack = gHUD.GetSpriteRect( m_loading );
	step = (float)(rcBack.right - rcBack.left) / 100;
	rcBar.right = rcBar.left + (int)ceil(percent * step);

	SPR_Set( hLoading, 128, 128, 128 );
	SPR_DrawAdditive( 0, x, y, &rcBack );	// background

	SPR_Set( hLoading, 255, 160, 0 );
	SPR_DrawAdditive( 0, x, y, &rcBar );	// progress bar
}

//
// 27/12/08 moved here from cl_view.c
//
void DrawProgressBar( void )
{
	if( !gHUD.m_iDrawPlaque ) return;
	DrawImageBar( CVAR_GET_FLOAT( "scr_loading" ), "m_loading" );

	if( !CVAR_GET_FLOAT( "scr_download" )) return;
	DrawImageBar( CVAR_GET_FLOAT( "scr_download" ), "m_download", (ScreenWidth - 128)>>1, ScreenHeight - 60 );
}

void AngleMatrix( const vec3_t angles, float (*matrix)[4] )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin( angle );
	cy = cos( angle );
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin( angle );
	cp = cos( angle );
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin( angle );
	cr = cos( angle );

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

//
// hl2 fade - this supports multiple fading
// FIXME: make Class CHudFade instead of C-style code?
//
void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags )
{
	CL_ScreenFade *sf = NULL;

	for( int i = 0; i < HUD_MAX_FADES; i++ )
	{
		// search for free spot
		if( gHUD.m_FadeList[i].fadeFlags == 0 )
		{
			sf = &gHUD.m_FadeList[i];
			break;
		}
	}

	if( !sf ) return; // no free fades found 

	sf->fadeEnd = duration;
	sf->fadeReset = holdTime;	
	sf->fadeColor = fadeColor;
	sf->fadeAlpha = alpha;
	sf->fadeFlags = fadeFlags;
	sf->fadeSpeed = 0;

	// calc fade speed
	if( duration > 0 )
	{
		if( fadeFlags & FFADE_OUT )
		{
			if( sf->fadeEnd )
			{
				sf->fadeSpeed = -(float)sf->fadeAlpha / sf->fadeEnd;
			}

			sf->fadeEnd += gHUD.m_flTime;
			sf->fadeReset += sf->fadeEnd;
		}
		else
		{
			if( sf->fadeEnd )
			{
				sf->fadeSpeed = (float)sf->fadeAlpha / sf->fadeEnd;
			}

			sf->fadeReset += gHUD.m_flTime;
			sf->fadeEnd += sf->fadeReset;
		}
	}

	if( fadeFlags & FFADE_PURGE )
	{
		ClearAllFades();
	}
}

void DrawScreenFade( void )
{
	int	i, numFades = 0;
	
	// Cycle through all fades and remove any that have finished (work backwards)
	for( i = HUD_MAX_FADES - 1; i >= 0; i-- )
	{
		CL_ScreenFade *pFade = &gHUD.m_FadeList[i];

		if( pFade->fadeFlags == 0 ) continue;	// free slot

		// Keep pushing reset time out indefinitely
		if( pFade->fadeFlags & FFADE_STAYOUT )
		{
			pFade->fadeReset = gHUD.m_flTime + 0.1f;
		}

		// All done?
		if(( gHUD.m_flTime > pFade->fadeReset ) && ( gHUD.m_flTime > pFade->fadeEnd ))
		{
			// remove this Fade from the list
			memset( pFade, 0, sizeof( CL_ScreenFade ));
		}
	}

	gHUD.m_bModulate = false;
	gHUD.m_vecFadeColor = Vector( 0, 0, 0 );
	gHUD.m_flFadeAlpha = 0.0f;

	// Cycle through all fades in the list and calculate the overall color/alpha
	for ( i = 0; i < HUD_MAX_FADES; i++ )
	{
		CL_ScreenFade *pFade = &gHUD.m_FadeList[i];

		if( pFade->fadeFlags == 0 ) continue;	// free slot

		// Color
		gHUD.m_vecFadeColor += pFade->fadeColor;

		// Fading...
		int iFadeAlpha;
		if( pFade->fadeFlags & ( FFADE_OUT|FFADE_IN ))
		{
			iFadeAlpha = pFade->fadeSpeed * ( pFade->fadeEnd - gHUD.m_flTime );
			if( pFade->fadeFlags & FFADE_OUT )
			{
				iFadeAlpha += pFade->fadeAlpha;
			}
			iFadeAlpha = min( iFadeAlpha, pFade->fadeAlpha );
			iFadeAlpha = max( 0, iFadeAlpha );
		}
		else
		{
			iFadeAlpha = pFade->fadeAlpha;
		}

		// Use highest alpha
		if( iFadeAlpha > gHUD.m_flFadeAlpha )
		{
			gHUD.m_flFadeAlpha = iFadeAlpha;
		}

		// Modulate?
		if( pFade->fadeFlags & FFADE_MODULATE )
		{
			gHUD.m_bModulate = true;
		}
		numFades++;
	}

	// divide colors
	if( numFades ) gHUD.m_vecFadeColor /= numFades;
	else return;

	const float *RGB = gHUD.m_vecFadeColor;
	FillRGBA( 0, 0, ScreenWidth, ScreenHeight, RGB[0], RGB[1], RGB[2], gHUD.m_flFadeAlpha );
}

void ClearPermanentFades( void )
{
	for( int i = HUD_MAX_FADES - 1; i >= 0; i-- )
	{
		CL_ScreenFade *pFade = &gHUD.m_FadeList[i];
		if( pFade->fadeFlags == 0 ) continue;	// free slot

		if( pFade->fadeFlags & FFADE_STAYOUT )
		{
			// remove this Fade from the list
			memset( pFade, 0, sizeof( CL_ScreenFade ));
		}
	}
}

void ClearAllFades( void )
{
	memset( gHUD.m_FadeList, 0, sizeof( gHUD.m_FadeList ));
}

/*
====================
RotatePointAroundVector
====================
*/
void RotatePointAroundVector( Vector &dst, const Vector &dir, const Vector &point, float degrees )
{
	float	t0, t1;
	float	angle, c, s, d;
	Vector	vr, vu, vf;

	angle = degrees * (M_PI / 180.f);
	s = sin( angle );
	c = cos( angle );

	vf.Init( dir.x, dir.y, dir.z );
	vr.Init( vf.z, -vf.x, vf.y );

	// find a three vectors
	d = DotProduct( vf, vr );
	vr = (vr + ( vf * -d )).Normalize();
	vu = CrossProduct( vr, vf );

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

/*
====================
UTIL_Probe

client explosion utility
====================
*/
float UTIL_Probe( const Vector &origin, Vector *vecDirection, float strength )
{
	// press out
	Vector endpos = origin + (( *vecDirection ) * strength );

	//Trace into the world
	TraceResult	tr;
	UTIL_TraceLine( origin, endpos, dont_ignore_monsters, NULL, &tr );

	// push back a proportional amount to the probe
	(*vecDirection) = -(*vecDirection) * (1.0f - tr.flFraction);

	ASSERT(( 1.0f - tr.flFraction ) >= 0.0f );

	// return the impacted proportion of the probe
	return (1.0f - tr.flFraction);
}

void UTIL_GetForceDirection( const Vector &origin, float magnitude, Vector *resultDirection, float *resultForce )
{
	Vector	d[6];

	// all cardinal directions
	d[0] = Vector(  1,  0,  0 );
	d[1] = Vector( -1,  0,  0 );
	d[2] = Vector(  0,  1,  0 );
	d[3] = Vector(  0, -1,  0 );
	d[4] = Vector(  0,  0,  1 );
	d[5] = Vector(  0,  0, -1 );

	//Init the results
	(*resultDirection).Init();
	(*resultForce) = 1.0f;
	
	// Get the aggregate force vector
	for ( int i = 0; i < 6; i++ )
	{
		(*resultForce) += UTIL_Probe( origin, &d[i], magnitude );
		(*resultDirection) += d[i];
	}

	// If we've hit nothing, then point up
	if (( *resultDirection ) == g_vecZero )
	{
		(*resultDirection) = Vector( 0, 0, 1 );
		(*resultForce) = 2.0f; // default value
	}

	// Just return the direction
	(*resultDirection) = (*resultDirection).Normalize();
}

/*
====================
Sys LoadGameDLL

some code from Darkplaces
====================
*/
BOOL Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts )
{
	const dllfunction_t	*gamefunc;
	dllhandle_t dllhandle = 0;

	if( handle == NULL ) return false;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->funcvariable = NULL;

	dllhandle = (dllhandle_t)LOAD_LIBRARY( dllname );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
	{
		if(!( *gamefunc->funcvariable = (void *) Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_UnloadLibrary( &dllhandle );
			return false;
		}
	}          

	ALERT( at_loading, "%s loaded succesfully!\n", dllname );
	*handle = dllhandle;
	return true;
}

void Sys_UnloadLibrary( dllhandle_t *handle )
{
	if( handle == NULL || *handle == NULL )
		return;

	FREE_LIBRARY( *handle );
	*handle = NULL;
}

void* Sys_GetProcAddress( dllhandle_t handle, const char* name )
{
	return (void *)GET_PROC_ADDRESS( handle, name );
}

//============
// UTIL_FileExtension
// returns file extension
//============
const char *UTIL_FileExtension( const char *in )
{
	const char *separator, *backslash, *colon, *dot;

	separator = strrchr( in, '/' );
	backslash = strrchr( in, '\\' );
	if( !separator || separator < backslash )
		separator = backslash;
	colon = strrchr( in, ':' );
	if( !separator || separator < colon )
		separator = colon;
	dot = strrchr( in, '.' );
	if( dot == NULL || (separator && ( dot < separator )))
		return "";
	return dot + 1;
}

/*
====================
VGui_ConsolePrint

VGUI not implemented, wait for version 0.75
====================
*/
void VGui_ConsolePrint( const char *text )
{
}