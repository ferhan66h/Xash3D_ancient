//=======================================================================
//			Copyright XashXT Group 2008 �
//		     hud_utils.cpp - client game utilities code
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
#include "hud.h"

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
	return READ_FLOAT();
}

float READ_ANGLE16( void )
{
	return (float)(READ_SHORT() * (360.0 / 65536));
}

//
// Sprites draw stuff
//
typedef struct
{
	// temp handle
	HSPRITE	hSprite;
	HSPRITE	hPause;		// pause pic

	// crosshair members
	HSPRITE	hCrosshair;
	wrect_t	rcCrosshair;
	Vector	rgbCrosshair;
} draw_stuff_t;

typedef struct
{
	float	fadeSpeed;	// How fast to fade (tics / second) (+ fade in, - fade out)
	float	fadeEnd;		// When the fading hits maximum
	float	fadeTotalEnd;	// Total End Time of the fade (used for FFADE_OUT)
	float	fadeReset;	// When to reset to not fading (for fadeout and hold)
	Vector	fadeColor;
	float	fadealpha;
	int	fadeFlags;	// Fading flags
} screenfade_t;

static draw_stuff_t ds;
static screenfade_t sf;
	
int SPR_Frames( HSPRITE hPic )
{
	int Frames;

	GetParms( NULL, NULL, &Frames, 0, hPic );

	return Frames;
}

int SPR_Height( HSPRITE hPic, int frame )
{
	int Height;

	GetParms( NULL, &Height, NULL, frame, hPic );

	return Height;
}

int SPR_Width( HSPRITE hPic, int frame )
{
	int Width;

	GetParms( &Width, NULL, NULL, frame, hPic );

	return Width;
}

void Draw_VidInit( void )
{
	memset( &ds, 0, sizeof( ds ));
}

/*
====================
SPRITE_GetList

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

void SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	ds.hSprite = hPic;
	SetColor((r / 255.0f), (g / 255.0f), (b / 255.0f), 1.0f );
}

void SPR_Set( HSPRITE hPic, int r, int g, int b, int a )
{
	ds.hSprite = hPic;
	SetColor((r / 255.0f), (g / 255.0f), (b / 255.0f), (a / 255.0f));
}

inline static void SPR_DrawGeneric( int frame, float x, float y, float width, float height, const wrect_t *prc )
{
	float	s1, s2, t1, t2;

	if( width == -1 && height == -1 )
	{
		int w, h;
		GetParms( &w, &h, NULL, frame, ds.hSprite );
		width = w;
		height = h;
	}

	if( prc )
	{
		// calc rectangle
		s1 = (float)prc->left / width;
		t1 = (float)prc->top / height;
		s2 = (float)prc->right / width;
		t2 = (float)prc->bottom / height;
		width = prc->right - prc->left;
		height = prc->bottom - prc->top;
	}
	else
	{
		s1 = t1 = 0.0f;
		s2 = t2 = 1.0f;
	}

	float	xscale, yscale;

	// scale for screen sizes
	xscale = gHUD.m_scrinfo.iRealWidth / (float)gHUD.m_scrinfo.iWidth;
	yscale = gHUD.m_scrinfo.iRealHeight / (float)gHUD.m_scrinfo.iHeight;

	x *= xscale;
	y *= yscale;
	width *= xscale;
	height *= yscale;

	DrawImageExt( ds.hSprite, x, y, width, height, s1, t1, s2, t2 );
} 

void FillRGBA( int x, int y, int width, int height, int r, int g, int b, int a )
{
	Vector	RGB;

	RGB.x = (float)(r / 255.0f);
	RGB.y = (float)(g / 255.0f);
	RGB.z = (float)(b / 255.0f);

	float	xscale, yscale;

	// scale for screen sizes
	xscale = gHUD.m_scrinfo.iRealWidth / (float)gHUD.m_scrinfo.iWidth;
	yscale = gHUD.m_scrinfo.iRealHeight / (float)gHUD.m_scrinfo.iHeight;

	x *= xscale;
	y *= yscale;
	width *= xscale;
	height *= yscale;

	g_engfuncs.pfnFillRGBA( x, y, width, height, RGB, (float)(a / 255.0f));
}

void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderNormal, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_Draw( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderNormal, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SPR_DrawTransColor( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderTransColor, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_DrawTransColor( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderTransColor, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderTransAlpha, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_DrawHoles( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderTransAlpha, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderTransAdd, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_DrawAdditive( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderTransAdd, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	ds.rgbCrosshair.x = (float)r / 255.0f;
	ds.rgbCrosshair.y = (float)g / 255.0f;
	ds.rgbCrosshair.z = (float)b / 255.0f;
	ds.hCrosshair = hspr;
	ds.rcCrosshair = rc;
}

void DrawCrosshair( void )
{
	if( ds.hCrosshair == 0 ) return;

	// FIXME: apply crosshair angles
	int x = (ScreenWidth - ds.rcCrosshair.right) / 2; 
	int y = (ScreenHeight - ds.rcCrosshair.bottom) / 2;

	ds.hSprite = ds.hCrosshair;
	SetParms( ds.hCrosshair, kRenderTransAlpha, 0 );
	SetColor( ds.rgbCrosshair.x, ds.rgbCrosshair.y, ds.rgbCrosshair.z, 1.0f );
	SPR_DrawGeneric( 0, x, y, -1, -1, &ds.rcCrosshair );
}

void DrawPause( void )
{
	// pause image
	if( !CVAR_GET_FLOAT( "paused" ) || !CVAR_GET_FLOAT( "scr_showpause" ))
		return;

	DrawImageBar( 100, "m_pause" ); // HACKHACK
}

void DrawImageRectangle( HSPRITE hImage )
{
	DrawImageExt( hImage, 0, 0, ActualWidth, ActualHeight, 0, 0, 1, 1 );
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
void V_RenderPlaque( void )
{
	const char *levelshot;

	levelshot = CVAR_GET_STRING( "cl_levelshot_name" );
	if( !strcmp( levelshot, "" )) levelshot = "<black>";

	// logo that shows up while upload next level
	DrawImageRectangle( LOAD_SHADER( levelshot ));
	DrawImageBar( CVAR_GET_FLOAT( "scr_loading" ), "m_loading" );

	if( !CVAR_GET_FLOAT( "scr_download" )) return;

	DrawImageBar( CVAR_GET_FLOAT( "scr_download" ), "m_download", (ScreenWidth-128)/2, ScreenHeight-60 );
}

void V_RenderSplash( void )
{
	DrawImageRectangle( LOAD_SHADER( "gfx/shell/splash" )); 
}

void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags )
{
	sf.fadeColor = fadeColor;
	sf.fadealpha = alpha;
	sf.fadeFlags = fadeFlags;
	sf.fadeEnd = gHUD.m_flTime + duration;
	sf.fadeTotalEnd = sf.fadeEnd + holdTime;
	sf.fadeSpeed = duration / gHUD.m_flTimeDelta;
}

void DrawScreenFade( void )
{
	// FIXME: implement
}

/*
====================
Sys LoadGameDLL

====================
*/
BOOL Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts )
{
	const dllfunction_t	*gamefunc;
	char dllpath[256], gamedir[256];
	dllhandle_t dllhandle = 0;

	if( handle == NULL ) return false;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->funcvariable = NULL;

	GET_GAME_DIR( gamedir );
	sprintf( dllpath, "%s/bin/%s", gamedir, dllname );
	dllhandle = LoadLibrary( dllpath );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		if(!( *gamefunc->funcvariable = (void *) Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_UnloadLibrary( &dllhandle );
			return false;
		}
          
	ALERT( at_loading, "%s loaded succesfully!\n", dllname );
	*handle = dllhandle;
	return true;
}

void Sys_UnloadLibrary( dllhandle_t *handle )
{
	if( handle == NULL || *handle == NULL )
		return;

	FreeLibrary( *handle );
	*handle = NULL;
}

void* Sys_GetProcAddress( dllhandle_t handle, const char* name )
{
	return (void *)GetProcAddress( handle, name );
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va( const char *format, ... )
{
	va_list argptr;
	static char string[16][1024], *s;
	static int stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 15;
	va_start( argptr, format );
	_vsnprintf( s, sizeof( string[0] ), format, argptr );
	va_end( argptr );
	return s;
}

/*
==============
COM_ParseToken

Parse a token out of a string
==============
*/
char *COM_ParseToken( const char **data_p )
{
	int		c;
	int		len = 0;
	const char	*data;
	static char	token[512];
	
	token[0] = 0;
	data = *data_p;
	
	if( !data ) 
	{
		*data_p = NULL;
		return NULL;
	}		

	// skip whitespace
skipwhite:
	while(( c = *data) <= ' ' )
	{
		if( c == 0 )
		{
			*data_p = NULL;
			return NULL; // end of file;
		}
		data++;
	}
	
	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// skip /* comments
	if( c=='/' && data[1] == '*' )
	{
		while( data[1] && (data[0] != '*' || data[1] != '/' ))
			data++;
		data += 2;
		goto skipwhite;
	}
	

	// handle quoted strings specially
	if( *data == '\"' || *data == '\'' )
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if( c=='\"' || c=='\0' )
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
	{
		token[len] = c;
		data++;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if( c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',' )
			break;
	} while( c > 32 );
	
	token[len] = 0;
	*data_p = data;
	return token;
}