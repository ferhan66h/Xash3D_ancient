//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "extdll.h"
#include "defaults.h"
#include "utils.h"
#include "cbase.h"
#include "basebeams.h"
#include "baseweapon.h"
#include "decals.h"
#include "basebrush.h"
#include "shake.h"
#include "monsters.h"
#include "client.h"
#include "player.h"
#include "soundent.h"

//=======================================================================
// 		   main functions ()
//=======================================================================
class CBaseParticle : public CBaseLogic
{
public:
	void Spawn( void );
	void Precache( void ){ pev->netname = UTIL_PrecacheAurora( pev->message ); }
	void KeyValue( KeyValueData *pkvd );
	void PostActivate( void ){ Switch(); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Switch( void );
};

void CBaseParticle :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "aurora" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CBaseParticle::Switch( void )
{
	int renderfx = (m_iState ? kRenderFxAurora : kRenderFxNone );
	if( pev->target )
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), m_hActivator );
		while ( pTarget )
		{
			UTIL_SetAurora( pTarget, pev->netname );
			pTarget->pev->renderfx = renderfx;
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ), m_hActivator );
		}
	}
	else
	{
		UTIL_SetAurora( this, pev->netname );
		pev->renderfx = renderfx;
	}
}

void CBaseParticle::Spawn( void )
{
	Precache();

	SetObjectClass( ED_NORMAL );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;

	if( pev->spawnflags & SF_START_ON || FStringNull( pev->targetname ))
		m_iState = STATE_ON;
	else m_iState = STATE_OFF;
}

void CBaseParticle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if ( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON )
	{
		m_iState = STATE_ON;
		Switch();
	}
	else if ( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
		Switch();
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		Msg( "State: %s, ParticleName: %s\n", GetStringForState( GetState()), STRING( pev->netname ));
		if ( pev->target ) Msg( "Attach to: %s\n", STRING( pev->target ));
		else Msg( "\n" );
	}
}
LINK_ENTITY_TO_CLASS( env_particle, CBaseParticle );

//=========================================================
// env_sky, an unreal tournament - style sky effect
//=========================================================
class CEnvSky : public CBaseLogic
{
public: 
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( env_sky, CEnvSky );

void CEnvSky :: Spawn( void )
{
	// Xash3D engine feature
	SetObjectClass( ED_SKYPORTAL );
}

//=========================================================
//	UTIL_ScreenFade (fade screen)
//=========================================================
#define SF_FADE_IN			0x0001	// Fade in, not out
#define SF_FADE_MODULATE		0x0002	// Modulate, don't blend
#define SF_FADE_ALL			0x0004	// fade all clients
#define SF_FADE_CAMERA		0x0008	// fading only for camera

class CFade : public CBaseLogic
{
public:
	void Spawn( void ) { m_iState = STATE_OFF; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
};
LINK_ENTITY_TO_CLASS( env_fade, CFade );

void CFade::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "duration" ))
	{
		pev->dmg_take = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "holdtime" ))
	{
		pev->dmg_save = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CFade::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "Fadecolor: %g %g %g, Fadealpha: %g, Fadetime: %g\n", pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->dmg_take );
		ALERT( at_console, "HoldTime %g, Mode: %s, Modulate: %s\n", pev->dmg_save, pev->spawnflags & SF_FADE_IN ? "FADE IN" : "FADE OUT", pev->spawnflags & SF_FADE_MODULATE ? "YES" : "NO" );
	}
	else
	{
		// setup fade flags
		int fadeFlags = 0;
		if ( pev->spawnflags & SF_FADE_IN )
			fadeFlags |= FFADE_IN;
		else fadeFlags |= FFADE_OUT;
		if ( pev->spawnflags & SF_FADE_MODULATE ) fadeFlags |= FFADE_MODULATE;
		if ( pev->spawnflags & SF_FADE_CAMERA ) fadeFlags |= FFADE_CUSTOMVIEW;
          
		// apply fade
		if ( pev->spawnflags & SF_FADE_ALL )
			UTIL_ScreenFadeAll( pev->rendercolor, pev->dmg_take, pev->dmg_save, pev->renderamt, fadeFlags );
		else UTIL_ScreenFade( pev->rendercolor, pev->dmg_take, pev->dmg_save, pev->renderamt, fadeFlags );
	}
}

//=====================================================
// env_customhud: change player hud
//=====================================================
class CEnvHud : public CBaseLogic
{
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* );
	void SetHUD( int hud_num );
	void ResetHUD( void );
	char* GetStringForMode( void );
};

void CEnvHud :: KeyValue( KeyValueData* pkvd )
{
	if (FStrEq( pkvd->szKeyName, "hud" ))
	{
		pev->skin = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CEnvHud :: SetHUD( int hud_num )
{
	if( m_hActivator != NULL )
	{
		m_iState = STATE_ON;
		((CBasePlayer *)((CBaseEntity *)m_hActivator))->m_iWarHUD = pev->skin = hud_num;
	}
}

void CEnvHud :: ResetHUD( void )
{
	if( m_hActivator != NULL )
	{
		m_iState = STATE_OFF;
		((CBasePlayer *)((CBaseEntity *)m_hActivator))->m_iWarHUD = 0;
		((CBasePlayer *)((CBaseEntity *)m_hActivator))->fadeNeedsUpdate = 1;
	}
}

void CEnvHud :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( pActivator && pActivator->IsPlayer( )) // only at player
	{
		m_hActivator = pActivator; // save activator
	}	
	else if( !IsMultiplayer( ))
	{
		m_hActivator = UTIL_PlayerByIndex( 1 );
	}		
	else
	{
		ALERT( at_warning, "%s: %s activator not player. Ignored.\n", STRING( pev->classname ), STRING( pev->targetname ));		
		return;
	}
	
	if ( useType == USE_TOGGLE )
	{
		if ( m_iState == STATE_OFF )
			useType = USE_ON;
		else useType = USE_OFF;
	}

	if ( useType == USE_ON ) SetHUD( pev->skin );
	else if ( useType == USE_OFF ) ResetHUD();
	else if ( useType == USE_SET )
	{
		if ( value ) SetHUD( value );
		else ResetHUD();
	}
	else if ( useType == USE_RESET ) ResetHUD();
	else if ( useType == USE_SHOWINFO )
	{
		ALERT( at_console, "======/Xash Debug System/======\n" );
		ALERT( at_console, "classname: %s\n", STRING( pev->classname ));
		ALERT( at_console, "State: %s, HUD Mode: %s\n", GetStringForState( GetState()), GetStringForMode());
		SHIFT;
	}
}

char* CEnvHud :: GetStringForMode( void )
{
	switch(pev->skin)
	{
		case 0: return "Disable Custom HUD";
		case 1: return "Draw Redeemer HUD";
		case 2: return "Draw Redeemer Underwater HUD";
		case 3: return "Draw Redeemer Noise Screen";
		case 4: return "Draw Security Camera Screen";
	default:	return "Draw None";
	}
}
LINK_ENTITY_TO_CLASS( env_customhud, CEnvHud );

//=======================================================================
//	env_zoom - change fov for player
//=======================================================================
class CEnvZoom : public CBaseLogic
{
	void Spawn (void ){ if( !pev->frags ) pev->frags = 90.0f; }
	void EXPORT Think( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );
	void SetFadeTime( void );
};

LINK_ENTITY_TO_CLASS( env_zoom,  CEnvZoom );

void CEnvZoom::KeyValue( KeyValueData* pkvd )
{
	if( FStrEq( pkvd->szKeyName, "duration" ))
	{
		pev->takedamage = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fov" ))
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CEnvZoom::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer())
		pActivator = UTIL_PlayerByIndex( 1 );
	m_hActivator = pActivator; // save activator

	if( m_iState == STATE_ON ) return;
	if( useType == USE_TOGGLE || useType == USE_ON ) SetFadeTime();
	else if( useType == USE_OFF )
		((CBasePlayer *)pActivator)->pev->fov = 90.0f;
	else if( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, Fade time %.00f\n", GetStringForState( GetState()), pev->takedamage );
		ALERT( at_console, "Current FOV: %g, Final FOV: %g\n", ((CBasePlayer *)pActivator)->pev->fov, pev->frags );
	}
}

void CEnvZoom::SetFadeTime( void )
{
	float CurFOV;
	float Length;

	if( pev->takedamage == 0 ) // instant apply fov
	{
		((CBasePlayer *)(CBaseEntity *)m_hActivator)->pev->fov = pev->frags;
		return;
	}
	else 
	{
		CurFOV = ((CBasePlayer *)(CBaseEntity *)m_hActivator)->pev->fov;
		if( CurFOV == 0.0f )
			CurFOV = ((CBasePlayer *)(CBaseEntity *)m_hActivator)->pev->fov = 90.0f;
	
		if( CurFOV > pev->frags ) Length = CurFOV - pev->frags;
		else if( CurFOV < pev->frags )
		{
			Length = pev->frags - CurFOV;
			pev->body = 1; // increment fov	
		}
		else return; // no change

		pev->health = pev->takedamage / Length;	
		SetNextThink ( pev->health );
	}
}

void CEnvZoom::Think( void )
{
	if( Q_rint(((CBasePlayer *)(CBaseEntity *)m_hActivator)->pev->fov ) == Q_rint( pev->frags ))
	{
		// time is expired
		SetThink( NULL );
		DontThink();
		m_iState = STATE_OFF;
		// fire target after finished                                   // transmit final fov
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, pev->frags );
                    return;
	}
	else
	{
		if( pev->body ) ((CBasePlayer *)(CBaseEntity *)m_hActivator)->pev->fov += 0.5f * gpGlobals->frametime;
		else ((CBasePlayer *)(CBaseEntity *)m_hActivator)->pev->fov -= 0.5f * gpGlobals->frametime;
	}
	m_iState = STATE_ON;
	SetNextThink ( pev->health );
}

//=====================================================
// env_render: change render parameters
//=====================================================

#define SF_RENDER_MASKFX	(1<<0)
#define SF_RENDER_MASKAMT	(1<<1)
#define SF_RENDER_MASKMODE	(1<<2)
#define SF_RENDER_MASKCOLOR	(1<<3)

class CRenderFxManager : public CBaseLogic
{
public:
	void Affect( void );
	void KeyValue( KeyValueData *pkvd );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		m_hActivator = pActivator; Affect();
	}
};
LINK_ENTITY_TO_CLASS( env_render, CRenderFxManager );

void CRenderFxManager :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "fadetime"))
	{
		pev->speed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CRenderFxManager::Affect( void )
{
	BOOL first = TRUE;

	if(FStringNull(pev->target)) return;
	CBaseEntity* pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target), m_hActivator);

	while ( pTarget != NULL )
	{
		entvars_t *pevTarget = pTarget->pev;

		if (pev->speed == 0) // aplly settings instantly ?
		{
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
				pevTarget->renderamt = pev->renderamt;
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
				pevTarget->rendercolor = pev->rendercolor;
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKFX ) )
				pevTarget->renderfx = pev->renderfx;
			if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKMODE ) )
				pevTarget->rendermode = pev->rendermode;

			pevTarget->scale = pev->scale;
			pevTarget->framerate = pev->framerate;
			if (first) UTIL_FireTargets( pev->netname, pTarget, this, USE_TOGGLE );
		}
		else
		{
			if (!pevTarget->renderamt && !pevTarget->rendermode) pevTarget->renderamt = 255;
			pevTarget->renderfx = pev->renderfx;
			pevTarget->rendermode = pev->rendermode;
			CBaseEntity *pFader = Create( "faderent", pev->origin, pev->angles, pTarget->edict());
			pFader->pev->renderamt = pevTarget->renderamt;
			pFader->pev->rendercolor = pevTarget->rendercolor;
			pFader->pev->scale = pevTarget->scale;
			if (pFader->pev->scale == 0) pFader->pev->scale = 1;
			pFader->pev->spawnflags = pev->spawnflags;

			if (first) pFader->pev->target = pev->netname;
                              
			pFader->pev->max_health = pev->framerate - pevTarget->framerate;
			pFader->pev->health = pev->renderamt - pevTarget->renderamt;
			pFader->pev->movedir.x = pev->rendercolor.x - pevTarget->rendercolor.x;
			pFader->pev->movedir.y = pev->rendercolor.y - pevTarget->rendercolor.y;
			pFader->pev->movedir.z = pev->rendercolor.z - pevTarget->rendercolor.z;

			if ( pev->scale ) pFader->pev->frags = pev->scale - pevTarget->scale;
			else pFader->pev->frags = 0;

			pFader->pev->dmgtime = gpGlobals->time;
			pFader->pev->speed = pev->speed;
			pFader->SetNextThink( 0 );
			pFader->Spawn();
		}

		first = FALSE;//not a first target
		pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target), m_hActivator );
	}		
}

//=========================================================
//		display message
//=========================================================
class CMessage : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBaseEntity *pPlayer = NULL;

		if ( IsMultiplayer() ) UTIL_ShowMessageAll( STRING(pev->message) );
		else
		{
			if ( pActivator && pActivator->IsPlayer() ) pPlayer = pActivator;
			else pPlayer = UTIL_PlayerByIndex( 1 );
		}

		if ( pPlayer ) UTIL_ShowMessage( STRING(pev->message), pPlayer );
		if ( pev->spawnflags & 1 ) UTIL_Remove( this );
	}
};
LINK_ENTITY_TO_CLASS( env_message, CMessage );

//=========================================================
// set global fog on a map
//=========================================================
class CEnvFog : public CBaseLogic
{
public:
	void PostActivate( void );
	void KeyValue( KeyValueData *pkvd );
	void TurnOn( void );
	void TurnOff( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( env_fog, CEnvFog );

void CEnvFog :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "startdist" ))
	{
		pev->dmg_take = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "enddist" ))
	{
		pev->dmg_save = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "fadetime" ))
	{
		m_flDelay = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CEnvFog :: PostActivate ( void )
{
	if ( pev->spawnflags & SF_START_ON ) 
	{
		TurnOn();
		ClearBits(pev->spawnflags, SF_START_ON);
	}
}

void CEnvFog :: TurnOn ( void )
{
	m_iState = STATE_ON;
	UTIL_SetFogAll(pev->rendercolor, m_flDelay, pev->dmg_take, pev->dmg_save );
}

void CEnvFog :: TurnOff ( void )
{
	m_iState = STATE_OFF;
	UTIL_SetFogAll(pev->rendercolor, -m_flDelay, pev->dmg_take, pev->dmg_save );
}

void CEnvFog :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_TOGGLE )
	{
		if ( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON ) TurnOn();
	else if ( useType == USE_OFF ) TurnOff();
}

//=========================================================
//	env_rain, custom client weather effects
//=========================================================
class CEnvRain : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "radius" ))
		{
			pev->armorvalue = atof( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "mode" ))
		{
			pev->button = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
	}
	void Precache( void )
	{
		if( pev->button )
		{
			// snow
			UTIL_PrecacheModel( "sprites/snowflake.spr" );
		}
		else
		{
			// it's a rainy day :)
			UTIL_PrecacheModel( "sprites/hi_rain.spr" );
			UTIL_PrecacheModel( "sprites/waterring.spr" );
		}
	}
	void Spawn() { Precache(); }
};
LINK_ENTITY_TO_CLASS( env_rain, CEnvRain );

//=========================================================
//	UTIL_RainModify (set new rain or snow)
//=========================================================
void CEnvRainModify::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	if( IsMultiplayer() || FStringNull( pev->targetname ))
		SetBits( pev->spawnflags, SF_START_ON );
}
LINK_ENTITY_TO_CLASS( env_rainmodify, CEnvRainModify );

TYPEDESCRIPTION CEnvRainModify::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvRainModify, randXY, FIELD_RANGE ),
	DEFINE_FIELD( CEnvRainModify, windXY, FIELD_RANGE ),
};
IMPLEMENT_SAVERESTORE( CEnvRainModify, CPointEntity );

void CEnvRainModify::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "drips" ))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "fadetime"))
	{
		pev->dmg = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "rand" ))
	{
		randXY = RandomRange( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "wind" ))
	{
		windXY = RandomRange( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
}

void CEnvRainModify::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "Wind X: %g Y: %g. Rand X: %g Y: %g\n", windXY[1], windXY[0], randXY[1], randXY[0] );
		ALERT( at_console, "Fade time %g. Drips per second %d\n", pev->dmg, pev->button );
	}
	else if( !pev->spawnflags & SF_START_ON )
	{
		for ( int i = 0; i < gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i+1 );

			if( FNullEnt( pPlayer->pev )) continue;

			if ( pev->dmg )
			{
				pPlayer->Rain_ideal_dripsPerSecond = pev->button;
				pPlayer->Rain_ideal_randX = randXY[1];
				pPlayer->Rain_ideal_randY = randXY[0];
				pPlayer->Rain_ideal_windX = windXY[1];
				pPlayer->Rain_ideal_windY = windXY[0];
				pPlayer->Rain_endFade = gpGlobals->time + pev->dmg;
				pPlayer->Rain_nextFadeUpdate = gpGlobals->time + 1;
			}
			else
			{
				pPlayer->Rain_dripsPerSecond = pev->button;
				pPlayer->Rain_randX = randXY[1];
				pPlayer->Rain_randY = randXY[0];
				pPlayer->Rain_windX = windXY[1];
				pPlayer->Rain_windY = windXY[0];
				pPlayer->rainNeedsUpdate = 1;
			}
		}
	}
}

//=======================================================================
// 		   env_sprite - classic HALF-LIFE sprites
//=======================================================================
void CSprite::Spawn( void )
{
	Precache();

	UTIL_SetModel( ENT(pev), pev->model );
          
	// Smart Field System ?
          if( !pev->renderamt ) pev->renderamt = 200; // light transparency
          if( !pev->framerate ) pev->framerate = Frames(); // play sequence at one second
	if( !pev->rendermode ) pev->rendermode = kRenderTransAdd;

	pev->solid	= SOLID_NOT;
	pev->movetype	= MOVETYPE_NOCLIP;
	pev->frame	= 0;
	SetBits( pFlags, PF_POINTENTITY );
	
	pev->angles.x	=  -pev->angles.x;
	pev->angles.y	=  180 - pev->angles.y;
	pev->angles[1]	=  0 - pev->angles[1];
	TurnOff();
}

void CSprite::PostSpawn( void )
{
	m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));
	if( m_pGoalEnt ) UTIL_SetOrigin( this, m_pGoalEnt->pev->origin );
}

void CSprite::Think( void )
{
	SetNextThink( 0 );

	if( pev->spawnflags & SF_TEMPSPRITE && gpGlobals->time > pev->dmg_take ) UTIL_Remove(this);
	else if( pev->framerate ) Animate( pev->framerate * (gpGlobals->time - pev->dmgtime) );

	Move();
	pev->dmgtime = gpGlobals->time;
}

void CSprite::Move( void )
{
	// Not moving on a path, return
	if( !m_pGoalEnt ) return;

	// Subtract movement from the previous frame
	pev->frags -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if ( pev->frags <= 0 )
	{
		// Fire the passtarget if there is one
		if ( m_pGoalEnt->pev->message )
		{
			UTIL_FireTargets( m_pGoalEnt->pev->message, this, this, USE_TOGGLE, 0 );
			if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_FIREONCE ) )
				m_pGoalEnt->pev->message = 0;
		}

		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_TELEPORT ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNext();
			if ( m_pGoalEnt ) UTIL_AssignOrigin( this, m_pGoalEnt->pev->origin ); 
		}

		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_WAITFORTRIG ) )
		{
			TurnOff(); //wait for retrigger
			return;
		}
		
		// Time to go to the next target
		m_pGoalEnt = m_pGoalEnt->GetNext();

		// Set up next corner
		if ( !m_pGoalEnt ) UTIL_SetVelocity( this, g_vecZero );
		else
		{
			pev->target = m_pGoalEnt->pev->targetname; // save last corner
			pev->armorvalue = m_pGoalEnt->pev->speed;

			Vector delta = m_pGoalEnt->pev->origin - pev->origin;
			pev->frags = delta.Length();
			pev->movedir = delta.Normalize();
			m_flDelay = gpGlobals->time + m_pGoalEnt->GetDelay();
		}
	}

	if ( m_flDelay > gpGlobals->time )
		pev->speed = UTIL_Approach( 0, pev->speed, 5 * gpGlobals->frametime );
	else pev->speed = UTIL_Approach( pev->armorvalue, pev->speed, 5 * gpGlobals->frametime );

	float fraction = 2 * gpGlobals->frametime;
	UTIL_SetVelocity( this, ((pev->movedir * pev->speed) * fraction) + (pev->velocity * (1-fraction)));
}

void CSprite::TurnOff( void )
{
	SetBits( pev->effects, EF_NODRAW );
	m_iState = STATE_OFF;
	if( m_pGoalEnt ) m_pGoalEnt = m_pGoalEnt->GetPrev();
	UTIL_SetVelocity( this, g_vecZero );
	DontThink();
}

void CSprite::TurnOn( void )
{
	pev->dmgtime = gpGlobals->time;
	ClearBits( pev->effects, EF_NODRAW );
	pev->frame = 0;
	m_iState = STATE_ON;
	
	pev->armorvalue = pev->speed;
	m_flDelay = gpGlobals->time;
	pev->frags = 0;
	
	Move();
	SetNextThink( 0 );
}

void CSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	pActivator = m_hActivator;
	
	if ( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON ) TurnOn();
	else if ( useType == USE_OFF ) TurnOff();
	else if ( useType == USE_SET ) ClearBits( pev->effects, EF_NODRAW );
	else if ( useType == USE_RESET ) SetBits( pev->effects, EF_NODRAW );
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, speed %g\n", GetStringForState( GetState()), pev->speed );
		if( m_pGoalEnt )
			ALERT( at_console, "NextPath: %s, framerate %g\n", STRING( m_pGoalEnt->pev->targetname ), pev->framerate );
		else ALERT( at_console, "NextPath: no path, framerate %g\n", pev->framerate );
	}
}
LINK_ENTITY_TO_CLASS( env_glow, CSprite );
LINK_ENTITY_TO_CLASS( env_sprite, CSprite );

//=================================================================
// env_model: like env_sprite, except you can specify a sequence.
//=================================================================
#define SF_DROPTOFLOOR	4

class CEnvModel : public CBaseAnimating
{
	void Spawn( void );
	void Precache( void ){ UTIL_PrecacheModel( pev->model ); }
	void EXPORT Animate( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetAnim( void );
};
LINK_ENTITY_TO_CLASS( env_model, CEnvModel );

void CEnvModel::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "mode"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSequence"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseAnimating::KeyValue( pkvd );
}

void CEnvModel :: Spawn( void )
{
	Precache();
	UTIL_SetModel( ENT(pev), pev->model );
	UTIL_SetOrigin( this, pev->origin );

          pev->takedamage = DAMAGE_NO;
	if ( !( pev->spawnflags & SF_NOTSOLID ))
	{
		pev->solid = SOLID_SLIDEBOX;
		UTIL_AutoSetSize();
	}

	if ( pev->spawnflags & SF_DROPTOFLOOR )
	{
		pev->origin.z += 1;
		DROP_TO_FLOOR ( ENT(pev) );
	}

	InitBoneControllers();
	ResetSequenceInfo( );
	m_iState = STATE_OFF;
	if(pev->spawnflags & SF_START_ON) Use( this, this, USE_ON, 0 );
}

void CEnvModel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON )
	{
		SetAnim();
		SetThink( Animate );
		SetNextThink( 0.1 );
		m_iState = STATE_ON;
	}
	else if ( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
		DontThink();
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
	}
}

void CEnvModel::SetAnim( void )
{
	pev->sequence = LookupSequence( STRING( pev->netname ));
	if (pev->sequence == -1)pev->sequence = 0;
	pev->frame = 0;
	ResetSequenceInfo();
}

void CEnvModel::Animate( void )
{
	SetNextThink( 0.1 );
	StudioFrameAdvance();
	
	if (m_fSequenceFinished )
	{
		if(pev->button)
		{
			//pev->frame = 0;
			m_iState = STATE_OFF;
			DontThink();
		}
		else SetAnim();
	}
}

//=======================================================================
// 	env_counter - digital 7 segment indicator as default
//=======================================================================
class CDecLED : public CBaseLogic
{
public:
	void	Spawn( void );
	void	CheckState( void );
	void	Precache( void ){ UTIL_PrecacheModel( pev->model, "sprites/decimal.spr" ); }
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	float 	Frames( void ) { return MODEL_FRAMES( pev->modelindex ) - 1; }
	float	flashtime;
	float	curframe( void )
	{
		if( pev->frame - floor( pev->frame ) > 0.5f )
			return ceil( pev->frame );
		return floor( pev->frame );
	}
};
LINK_ENTITY_TO_CLASS( env_counter, CDecLED );
LINK_ENTITY_TO_CLASS( logic_indicator, CDecLED );	// Xash 0.2 name

void CDecLED :: Spawn( void )
{
	Precache( );
	UTIL_SetModel( ENT(pev), pev->model, "sprites/decimal.spr" );
          
	// Smart Field System ?
	if( !pev->rendermode ) pev->rendermode = kRenderTransAdd;
	if( !pev->frags ) pev->frags = Frames();
	if( !pev->impulse ) pev->impulse = -1;

	CheckState();

	pev->solid = SOLID_NOT;
	pev->effects = EF_NOINTERP;	// g-cont. criticall stuff, don't touch!
	pev->movetype = MOVETYPE_NONE;
	pev->angles.x = -pev->angles.x;
	pev->angles.y = 180 - pev->angles.y;
	pev->angles[1] = 0 - pev->angles[1];	
}

void CDecLED :: CheckState( void )
{
	switch( pev->skin )
	{
	case 1:
		if( pev->impulse >= curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	case 2:
		if( pev->impulse <= curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	default:
		if( pev->impulse == curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	}
}

void CDecLED :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq( pkvd->szKeyName, "maxframe" ))
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "keyframe" ))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq( pkvd->szKeyName, "type" ))
	{
		pev->skin = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseLogic::KeyValue( pkvd );
}

void CDecLED :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_ON || useType == USE_TOGGLE )
	{
		if( pev->frame >= pev->frags ) 
		{
			UTIL_FireTargets( pev->target, pActivator, this, useType, value );
			pev->frame = 0;
		}
		else pev->frame++;
		
		pev->effects &= ~EF_NODRAW;
		flashtime = gpGlobals->time + 0.5f;
	}
	else if ( useType == USE_OFF )
	{
		if( pev->frame <= 0.0f ) 
		{
         			UTIL_FireTargets( pev->target, pActivator, this, useType, value );
			pev->frame = pev->frags;
		}
		else pev->frame--;

		pev->effects &= ~EF_NODRAW;
		flashtime = gpGlobals->time + 0.5f;
	}
	else if ( useType == USE_SET ) // set custom frame
	{ 
		if ( value != 0.0f )
		{
			pev->frame = fmod( value, pev->frags + 1.0f );
                             	float next = value / ( pev->frags + 1.0f );
                             	if( (int)next ) UTIL_FireTargets( pev->target, pActivator, this, useType, (int)next );

			pev->effects &= ~EF_NODRAW;
			flashtime = gpGlobals->time + 0.5f;
		}
		else if( gpGlobals->time > flashtime )
		{
			if( pev->effects & EF_NODRAW )
			{
				pev->effects &= ~EF_NODRAW;
			}
			else
			{
				pev->effects |= EF_NODRAW;
			}
		}
	}
	else if ( useType == USE_RESET ) // immediately reset
	{ 
		pev->frame = 0;
		pev->effects &= ~EF_NODRAW;
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		Msg( "Current frame: %.0f, Keyframe %d\n", pev->frame, pev->impulse );
		Msg( "State: %s, Maxframe %g\n", GetStringForState( GetState()), pev->frags );
	}
	CheckState();
}

//=======================================================================
// 		   env_shake - earthquake (screen shake)
//=======================================================================
class CShake : public CBaseLogic
{
public:
	void Spawn( void ){ m_iState = STATE_OFF; }
	void Think( void ) { m_iState = STATE_OFF; };
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if( useType == USE_SHOWINFO )
		{
			DEBUGHEAD;
			Msg( "State: %s, Amplitude: %g, Frequency: %g\n", GetStringForState( GetState()), pev->scale, pev->dmg_save );
			Msg( "Radius: %g, Duration: %d\n", pev->dmg, pev->dmg_take );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, Amplitude(), Frequency(), Duration(), Radius() );
			m_iState = STATE_ON;
			SetNextThink( Duration() );
		}
	}
	void KeyValue( KeyValueData *pkvd )
	{
		if (FStrEq(pkvd->szKeyName, "amplitude" ))
		{
			SetAmplitude( atof(pkvd->szValue ));
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "frequency" ))
		{
			SetFrequency( atof( pkvd->szValue ));
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "duration"))
		{
			SetDuration( atof(pkvd->szValue) );
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "radius"))
		{
			SetRadius( atof(pkvd->szValue) );
			pkvd->fHandled = TRUE;
		}
		else	CBaseEntity::KeyValue( pkvd );
	}

	inline float Amplitude( void ) { return pev->scale; }
	inline float Frequency( void ) { return pev->dmg_save; }
	inline float Duration( void ) { return pev->dmg_take; }
	inline float Radius( void ) { return pev->dmg; }
	inline void SetAmplitude( float amplitude ) { pev->scale = amplitude; }
	inline void SetFrequency( float frequency ) { pev->dmg_save = frequency; }
	inline void SetDuration( float duration ) { pev->dmg_take = duration; }
	inline void SetRadius( float radius ) { pev->dmg = radius; }
};
LINK_ENTITY_TO_CLASS( env_shake, CShake );

//=======================================================================
// 		   env_sparks - classic HALF-LIFE sparks
//=======================================================================
class CEnvSpark : public CBaseLogic
{
public:
	void Spawn(void);
	void Think(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS(env_spark, CEnvSpark);

void CEnvSpark::Spawn(void)
{
	if( !m_flDelay ) m_flDelay = 1.0; // Smart field system ?
	if( pev->spawnflags & SF_START_ON ) Use( this, this, USE_ON, 0 );
}

void CEnvSpark::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType == USE_TOGGLE)
	{
		if ( m_iState == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON )
	{
		m_iState = STATE_ON;
		SetNextThink( m_flDelay );
	}
	else if ( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
		DontThink();		
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, MaxDelay %.2f\n\n", GetStringForState( GetState()), m_flDelay );
	}
}

void CEnvSpark::Think( void )
{
	float flVolume = RANDOM_FLOAT ( 0.25 , 0.75 ) * 0.4; // random volume range
	switch( RANDOM_LONG( 0, 5 ))
	{
	case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark1.wav", flVolume, ATTN_NORM); break;
	case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark2.wav", flVolume, ATTN_NORM); break;
	case 2: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark3.wav", flVolume, ATTN_NORM); break;
	case 3: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark4.wav", flVolume, ATTN_NORM); break;
	case 4: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark5.wav", flVolume, ATTN_NORM); break;
	case 5: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark6.wav", flVolume, ATTN_NORM); break;
	}

	UTIL_Sparks( pev->origin + pev->size * 0.5 );
	if( m_iState == STATE_ON ) SetNextThink( 0.1 + RANDOM_FLOAT( 0, m_flDelay )); //!!!
}

//=========================================================
// 		env_explosion (classic explode )
//=========================================================
class CEnvExplosion : public CBaseLogic
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Precache( void ) { if( pev->message ) pev->button = UTIL_PrecacheModel( pev->message ); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Explode( void );
};
LINK_ENTITY_TO_CLASS( env_explosion, CEnvExplosion );

void CEnvExplosion::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->dmg = atoi(pkvd->szValue);
		if(pev->dmg < 10) pev->dmg = 10;
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "sprite"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CEnvExplosion::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	if ( useType == USE_TOGGLE || useType == USE_ON )
	{
		Explode();
	}
	else if ( useType == USE_SET )
	{
		pev->dmg = value;
		if( pev->dmg < 10 ) pev->dmg = 10;
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "Radius %g\n", pev->dmg );
		SHIFT;
	}
}

void CEnvExplosion::Explode( void )
{
	TraceResult tr;
	Vector vecSpot = pev->origin;// trace starts here!
	UTIL_TraceLine ( vecSpot + Vector( 0, 0, 8 ), vecSpot + Vector ( 0, 0, -32 ),  ignore_monsters, ENT(pev), &tr);

	// Pull out of the wall a bit
	if ( tr.flFraction != 1.0 && tr.vecPlaneNormal[2] < 0.7f )
		pev->origin = tr.vecEndPos + (tr.vecPlaneNormal * (pev->dmg - 24) * 0.6);
          int iContents = UTIL_PointContents ( pev->origin );

	entvars_t *pevOwner;
	if ( pev->owner ) pevOwner = VARS( pev->owner );
	else pevOwner = NULL;
	pev->owner = NULL; // can't traceline attack owner if this is set
         
	if ( pev->dmg <= 0 )
          	pev->dmg = 100; // Smart field system ?
          

          if( iContents == CONTENTS_WATER )
          {
          	SFX_Explode( g_sModelIndexWExplosion, pev->origin, pev->dmg, TE_EXPLFLAG_NONE );
          	RadiusDamage ( pev->origin, pev, pevOwner, pev->dmg / 2, pev->dmg * 2.5, CLASS_NONE, DMG_BLAST );
          }             
          else
          {
          	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, pev->dmg * 5, 3.0 );
		// custom sprite
		if( pev->button ) SFX_Explode( pev->button, pev->origin, pev->dmg, TE_EXPLFLAG_NONE );
          	else SFX_Explode( g_sModelIndexFireball, pev->origin, pev->dmg, TE_EXPLFLAG_NONE );
          	RadiusDamage ( pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 2.5, CLASS_NONE, DMG_BLAST );

		CBaseEntity *pWreckage = Create( "smokeent", pev->origin, pev->angles );
		pWreckage->SetNextThink( 0.2 );
		pWreckage->pev->impulse = ( pev->dmg - 50) * 0.6;
		pWreckage->pev->dmgtime = gpGlobals->time + 1.5;
		
		int sparkCount = RANDOM_LONG( 0, 3 );	//make sparks
		for ( int i = 0; i < sparkCount; i++ )
			Create( "sparkleent", pev->origin, tr.vecPlaneNormal, NULL );

		if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 ) UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
		else UTIL_DecalTrace( &tr, DECAL_SCORCH2 );
	}

	if( pev->spawnflags & SF_FIREONCE ) UTIL_Remove( this );
}

//=========================================================
// 	gibs (sprite or models gib)
//=========================================================
void CGib :: Spawn( const char *szGibModel )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.55; // deading the bounce a bit
	pev->renderamt = 255;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->solid = SOLID_TRIGGER;
	pev->classname = MAKE_STRING( "gib" );

	SetObjectClass( ED_NORMAL );          
	UTIL_SetModel( ENT( pev ), szGibModel );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	m_lifeTime = MAX_GIB_LIFETIME;


	m_material = None;
	m_cBloodDecals = 5;// how many blood decals this gib can place (1 per bounce until none remain). 
}

void CGib :: WaitTillLand ( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if ( pev->velocity == g_vecZero )
	{
		if(m_lifeTime == -1) //waiting for pvs
		{
			SetThink( PVSRemove );
			SetNextThink( MAX_GIB_LIFETIME );
		}
		else
		{
			SetThink( Fadeout );
			SetNextThink( m_lifeTime );
		}
		if(m_bloodColor != DONT_BLEED)
		{
			CSoundEnt::InsertSound ( bits_SOUND_MEAT, pev->origin, 384, 25 );
		}
	}
	else SetNextThink( 0.5 );
}

void CGib :: BounceGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult tr;
	
	if ( pOther != g_pWorld) UTIL_FireTargets( pev->target, pOther, this, USE_ON );
	else UTIL_FireTargets( pev->target, pOther, this, USE_SET ); //world touch
	
	if( pev->flags & FL_ONGROUND )
	{
		pev->velocity = pev->velocity * 0.9;
		pev->angles.x = 0;
		pev->angles.z = 0;
		pev->avelocity.x = 0;
		pev->avelocity.z = 0;
	}
	else
	{
		if (m_cBloodDecals > 0 && m_bloodColor != DONT_BLEED )
		{
			vecSpot = pev->origin + Vector ( 0 , 0 , 8 ); //move up a bit, and trace down.
			UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  ignore_monsters, ENT(pev), & tr);
			UTIL_BloodDecalTrace( &tr, m_bloodColor );
			m_cBloodDecals--; 
		}
		if ( m_material != None && RANDOM_LONG(0,2) == 0 )
		{
			float volume;
			float zvel = fabs(pev->velocity.z);
			volume = 0.8 * min(1.0, ((float)zvel) / 450.0);
			CBaseBrush::PlayRandomSound( edict(), (Materials)m_material, volume );
		}
	}
}

void CGib :: StickyGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult tr;
	
	if(m_lifeTime == -1) //waiting for pvs
	{
		SetThink( PVSRemove );
		SetNextThink( MAX_GIB_LIFETIME );
	}
	else
	{
		SetThink( Fadeout );
		SetNextThink( m_lifeTime );
	}
	
	if ( pOther != g_pWorld )
	{
		UTIL_FireTargets( pev->target, pOther, this, USE_ON );
		UTIL_Remove( this );
		return;
	}
          else UTIL_FireTargets( pev->target, pOther, this, USE_SET ); //world touch
          
	UTIL_TraceLine ( pev->origin, pev->origin + pev->velocity * 32,  ignore_monsters, ENT(pev), & tr);
	UTIL_BloodDecalTrace( &tr, m_bloodColor );

	pev->velocity = tr.vecPlaneNormal * -1;
	pev->angles = UTIL_VecToAngles ( pev->velocity );
	pev->velocity = g_vecZero; 
	pev->avelocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
}

CGib *CGib :: CreateGib( CBaseEntity *pVictim, const char *szGibModel, int gibtype )
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );
	pGib->Spawn( szGibModel );

	dstudiohdr_t *pstudiohdr = (dstudiohdr_t *)(GET_MODEL_PTR( ENT(pGib->pev) ));
	if (! pstudiohdr) return NULL;
	dstudiobodyparts_t *pbodypart = (dstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex);

	if ( pVictim )	//spawn at gibbed monster
	{
		if(gibtype == 1) //skull gib
		{
			pGib->pev->body = 0; //spawn skull
			pGib->pev->origin = pVictim->pev->origin + pVictim->pev->view_ofs;
		
			edict_t *pentPlayer = FIND_CLIENT_IN_PVS( pGib->edict());

			if ( RANDOM_LONG ( 0, 100 ) <= 5 && pentPlayer )
			{
				// 5% chance head will be thrown at player's face.
				entvars_t	*pevPlayer;

				pevPlayer = VARS( pentPlayer );
				pGib->pev->velocity = ( ( pevPlayer->origin + pevPlayer->view_ofs ) - pGib->pev->origin ).Normalize() * 300;
				pGib->pev->velocity.z += 100;
			}
			else pGib->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));
		}
		else 
		{
			pGib->pev->body = RANDOM_LONG(1, pbodypart->nummodels - 1); //random gibs
	
			// spawn the gib somewhere in the monster's bounding volume
			pGib->pev->origin.x = pVictim->pev->absmin.x + pVictim->pev->size.x * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.y = pVictim->pev->absmin.y + pVictim->pev->size.y * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.z = pVictim->pev->absmin.z + pVictim->pev->size.z * (RANDOM_FLOAT ( 0 , 1 ) ) + 1;

			// make the gib fly away from the attack vector
			pGib->pev->velocity = g_vecAttackDir * -1;

			// mix in some noise
			pGib->pev->velocity.x += RANDOM_FLOAT ( -0.25, 0.25 );
			pGib->pev->velocity.y += RANDOM_FLOAT ( -0.25, 0.25 );
			pGib->pev->velocity.z += RANDOM_FLOAT ( -0.25, 0.25 );

			pGib->pev->velocity = pGib->pev->velocity * RANDOM_FLOAT ( 300, 400 );
		}

		//shared settings
		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );
		pGib->m_bloodColor = (CBaseEntity::Instance(pVictim->edict()))->BloodColor();
			
		if ( pVictim->pev->health > -50) pGib->pev->velocity = pGib->pev->velocity * 0.7;
		else if ( pVictim->pev->health > -200) pGib->pev->velocity = pGib->pev->velocity * 2;
		else pGib->pev->velocity = pGib->pev->velocity * 4;
	          
		//LimitVelocity
		float length = pGib->pev->velocity.Length();
		if ( length > (MAX_VELOCITY / 2))
			pGib->pev->velocity = pGib->pev->velocity.Normalize() * MAX_VELOCITY / 2;
	}
	
	// global settings
	UTIL_SetSize ( pGib->pev, g_vecZero, g_vecZero );
			
	if(gibtype == 0 || gibtype == 1) //normal random gibs
	{
		pGib->pev->solid = SOLID_BBOX;
		pGib->SetTouch( BounceGibTouch );
		pGib->SetThink( WaitTillLand );
		pGib->SetNextThink( 4 );
	}
	if(gibtype == 2) //sticky gib
	{
		pGib->pev->movetype = MOVETYPE_TOSS;
		pGib->SetTouch( StickyGibTouch );
	}
	return pGib;
}

//=========================================================
// 	env_shooter (sprite or models shooter)
//=========================================================
class CEnvShooter : public CBaseLogic
{
	void Precache( void ) { if(ParseGibFile()) Setup(); }
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
          int ParseGibFile( void ); 
          void Setup( void );
          void Think( void );
	CBaseEntity *CreateGib( Vector vecPos, Vector vecVel );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	//this is will be restoring on precache - no need to save
	Materials	m_Material;
	Physics m_Physics;
	string_t entity;

	RandomRange velocity;
	RandomRange variance;
	RandomRange lifetime;
};
LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

void CEnvShooter::Spawn( void )
{
	Precache();
	pev->effects = EF_NODRAW;
	if ( m_flDelay == 0 ) m_flDelay = 0.1;
	UTIL_LinearVector( this );//movement direction
}

void CEnvShooter :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "file"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibcount"))
	{
		pev->impulse = pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibname"))
	{
		m_sSet = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibtarget"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}


void CEnvShooter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	SetNextThink( 0 );
}

void CEnvShooter :: Think ( void )
{
          TraceResult tr; //don't create stack monsters
	UTIL_TraceLine ( pev->origin, pev->origin - Vector ( 0, 0, 2048 ), ignore_monsters, ENT(pev), &tr );
	Vector mins = pev->origin - Vector( 34, 34, 0 );
	Vector maxs = pev->origin + Vector( 34, 34, 0 );
	maxs.z = pev->origin.z;
	mins.z = tr.vecEndPos.z;

	CBaseEntity *pList[2];
	if (UTIL_EntitiesInBox( pList, 2, mins, maxs, FL_CLIENT|FL_MONSTER ))
	{
		// don't build a stack of monsters!
		SetNextThink( m_flDelay );
		return;
	}
	Vector vecShootDir = pev->movedir;
	vecShootDir = vecShootDir + gpGlobals->v_up * variance.Random();
	vecShootDir = vecShootDir + gpGlobals->v_right * variance.Random();
	vecShootDir = vecShootDir + gpGlobals->v_forward * variance.Random();
	vecShootDir = vecShootDir.Normalize();                  
	CBaseEntity *pGib = CreateGib(pev->origin, vecShootDir * velocity.Random() );
	
	if ( pGib )UTIL_FireTargets( pev->target, pGib, this, USE_TOGGLE, 0 );
	if(pev->button > 0) pev->impulse--;

	if ( pev->impulse <= 0 )
	{
		if ( pev->spawnflags & SF_FIREONCE ) 
		{
			UTIL_Remove( this );
			return;
		}
		else
		{
			pev->impulse = pev->button;
			DontThink();
		}
	}
	SetNextThink( m_flDelay );
}


void CEnvShooter :: Setup( void ) 
{
	//Smart field system ?
	if(!FStringNull( m_sMaster )) UTIL_PrecacheModel( m_sMaster );
	if(!FStringNull( entity )) UTIL_PrecacheEntity( entity );
	if(velocity.m_flMax == 0) velocity.m_flMax = 100;
	if(velocity.m_flMax > MAX_VELOCITY) velocity.m_flMax = MAX_VELOCITY;
	if(lifetime.Random() == 0)lifetime = RandomRange( -1.0 );
	if(pev->scale == 0)pev->scale = 1;
	CBaseBrush::MaterialSoundPrecache( m_Material );

	//setup physics
	pev->solid = SOLID_SLIDEBOX;
	switch( m_Physics )
	{
	case Noclip: pev->team = MOVETYPE_NOCLIP; pev->solid = SOLID_NOT;	break;
	case Bounce: pev->team = MOVETYPE_BOUNCE; break;
	case Sticky: pev->team = MOVETYPE_TOSS; pev->solid = SOLID_BBOX; break;
	case Fly: pev->team = MOVETYPE_FLY; break;
	case Toss: pev->team = MOVETYPE_TOSS; break;
	}
}

int CEnvShooter :: ParseGibFile( void ) 
{
	char *token;
	char *afile = (char *)LOAD_FILE( STRING( pev->message ), NULL );
	const char *pfile = afile;
	
	if( !pfile )
	{
 		ALERT( at_warning, "Gib script file for %s not found!\n", STRING( pev->targetname ));
 		return 0;
	}
	else
	{
		while( pfile )
		{
			token = COM_ParseToken( &pfile );

			if ( !FStriCmp( token, "model" ))
			{
				token = COM_ParseToken( &pfile );
				m_sMaster = ALLOC_STRING( token );
			} 
			else if ( !FStriCmp( token, "entity" ))
			{
				token = COM_ParseToken( &pfile );
				entity = ALLOC_STRING( token );
			}
			else if ( !FStriCmp( token, "speed" ))
			{
				token = COM_ParseToken( &pfile );
				velocity = RandomRange((char *)STRING(ALLOC_STRING(token)));
			}
			else if ( !FStriCmp( token, "variance" ))
			{
				token = COM_ParseToken( &pfile );
				variance = RandomRange((char *)STRING(ALLOC_STRING(token)));
			}
			else if ( !FStriCmp( token, "livetime" ))
			{
				token = COM_ParseToken( &pfile );
				lifetime = RandomRange((char *)STRING(ALLOC_STRING(token)));
			}
			else if ( !FStriCmp( token, "friction" ))
			{
				token = COM_ParseToken( &pfile );
				RandomRange((char *)STRING(ALLOC_STRING(token)));
				pev->friction = RANDOM_FLOAT( m_flMin, m_flMax );
			}
			else if ( !FStriCmp( token, "scale" ))
			{
				token = COM_ParseToken( &pfile );
				RandomRange((char *)STRING(ALLOC_STRING(token)));
				pev->scale = RANDOM_FLOAT( m_flMin, m_flMax );
			}
			else if ( !FStriCmp( token, "body" ))
			{
				token = COM_ParseToken( &pfile );
				RandomRange((char *)STRING(ALLOC_STRING(token)));
				pev->body = RANDOM_FLOAT( m_flMin, m_flMax - 1 );
			}
			else if ( !FStriCmp( token, "skin" ))
			{
				token = COM_ParseToken( &pfile );
				RandomRange((char *)STRING(ALLOC_STRING(token)));
				pev->skin = RANDOM_FLOAT( m_flMin, m_flMax );
			}
			else if ( !FStriCmp( token, "frame" ))
			{
				token = COM_ParseToken( &pfile );
				RandomRange((char *)STRING(ALLOC_STRING(token)));
				pev->frame = RANDOM_FLOAT( m_flMin, m_flMax );
			}
			else if ( !FStriCmp( token, "alpha" ))
			{
				token = COM_ParseToken( &pfile );
				RandomRange((char *)STRING(ALLOC_STRING(token)));
				pev->renderamt = RANDOM_FLOAT( m_flMin, m_flMax );
			}
			else if ( !FStriCmp( token, "color" ))
			{
				token = COM_ParseToken( &pfile );
				UTIL_StringToVector((float *)pev->rendercolor, token );
			}
			else if ( !FStriCmp( token, "rendermode" ))
			{
				token = COM_ParseToken( &pfile );
				if ( !FStriCmp( token, "normal" )) pev->rendermode = kRenderNormal;
				else if ( !FStriCmp( token, "color" )) pev->rendermode = kRenderTransColor;
				else if ( !FStriCmp( token, "texture" )) pev->rendermode = kRenderTransTexture;
				else if ( !FStriCmp( token, "glow" )) pev->rendermode = kRenderGlow;
				else if ( !FStriCmp( token, "solid" )) pev->rendermode = kRenderTransAlpha;
				else if ( !FStriCmp( token, "additive" )) pev->rendermode = kRenderTransAdd;
			}
			else if ( !FStriCmp( token, "sfx" ))
			{
				token = COM_ParseToken( &pfile );
				if ( !FStriCmp( token, "hologramm" )) pev->renderfx = kRenderFxHologram;
				else if ( !FStriCmp( token, "glowshell" )) pev->renderfx = kRenderFxGlowShell;
			}
			else if ( !FStriCmp( token, "size" ))
			{
				UTIL_StringToVector((float*)pev->size, token);
				pev->size = pev->size/2;
			}
			else if ( !FStriCmp( token, "material" ))
			{
				token = COM_ParseToken( &pfile );
				if ( !FStriCmp( token, "none" )) m_Material = None;
				else if ( !FStriCmp( token, "bones" )) m_Material = Bones;
				else if ( !FStriCmp( token, "flesh" )) m_Material = Flesh;
				else if ( !FStriCmp( token, "cinder block" )) m_Material = CinderBlock;
				else if ( !FStriCmp( token, "concrete" )) m_Material = Concrete;
				else if ( !FStriCmp( token, "rocks" )) m_Material = Rocks;				
				else if ( !FStriCmp( token, "computer" )) m_Material = Computer;
				else if ( !FStriCmp( token, "glass" )) m_Material = Glass;
				else if ( !FStriCmp( token, "metalplate" )) m_Material = MetalPlate;
				else if ( !FStriCmp( token, "metal" )) m_Material = Metal;
				else if ( !FStriCmp( token, "airduct" )) m_Material = AirDuct;
				else if ( !FStriCmp( token, "ceiling tile" )) m_Material = CeilingTile;
				else if ( !FStriCmp( token, "wood" )) m_Material = Wood;
			}
			else if ( !FStriCmp( token, "physics" ))
			{
				token = COM_ParseToken( &pfile );
				if ( !FStriCmp( token, "bounce" )) m_Physics = Bounce;
				else if ( !FStriCmp( token, "sticky" )) m_Physics = Sticky;
				else if ( !FStriCmp( token, "noclip" )) m_Physics = Noclip;
				else if ( !FStriCmp( token, "fly" )) m_Physics = Fly;				
				else if ( !FStriCmp( token, "toss" )) m_Physics = Toss;
			}
		}

		COM_FreeFile( afile );

		if( FStringNull( m_sMaster) && FStringNull( entity ))
		{
			ALERT( at_warning, "model or entity not specified for %s\n", STRING( pev->targetname ));
			return 0;
		}
		return 1;
 	}
}

CBaseEntity *CEnvShooter :: CreateGib ( Vector vecPos, Vector vecVel )
{
	if( !FStringNull( entity ))//custom precached entity
          {
          	CBaseEntity *pEnt = CBaseEntity::CreateGib( (char *)STRING( entity ), (char *)STRING( m_sMaster));
          	//CBaseEntity *pEnt = CBaseEntity::Create( (char *)STRING( entity ), pev->origin, g_vecZero, edict() );
		//if(!pEnt && pEnt->edict()) return NULL;
                    if(!pEnt) return NULL;
		
		//UTIL_SetOrigin( pEnt, vecPos );
		pEnt->pev->origin = vecPos;
		pEnt->pev->velocity = vecVel;
		pEnt->pev->renderamt = pev->renderamt;
		//pEnt->pev->model = m_sMaster;
		pEnt->pev->rendermode = pev->rendermode;
		pEnt->pev->rendercolor = pev->rendercolor;
		pEnt->pev->renderfx = pev->renderfx;
		pEnt->pev->target = pev->netname;
		pEnt->pev->skin = pev->skin;
		pEnt->pev->body = pev->body;
		pEnt->pev->scale = pev->scale;
		pEnt->pev->frame = pev->frame;
		pEnt->pev->friction = pev->friction;
                    pEnt->pev->movetype = pev->team;
		pEnt->pev->solid = pev->solid;
		pEnt->pev->targetname = m_sSet;
		
		//UTIL_SetModel(ENT(pEnt->pev), m_sMaster );	
		return pEnt;
	}
	else if(m_Physics == Bounce || m_Physics == Sticky) // normal or sticky gib
	{
		CGib *pGib = CGib::CreateGib( NULL, (char *)STRING( m_sMaster ), m_Physics );
		if(!pGib && pGib->edict()) return NULL;
                    
		pGib->pev->body = pev->body;
		pGib->pev->origin = vecPos;
		pGib->pev->velocity = vecVel;
		pGib->m_material = m_Material;
		pGib->pev->rendermode = pev->rendermode;
		pGib->pev->renderamt = pev->renderamt;
		pGib->pev->rendercolor = pev->rendercolor;
		pGib->pev->renderfx = pev->renderfx;
		pGib->pev->target = pev->netname;
		pGib->pev->friction = pev->friction;
		pGib->pev->scale = pev->scale;
		pGib->pev->skin = pev->skin;
		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );
		pGib->m_lifeTime = pev->health;
		pGib->pev->scale = pev->scale;
		pGib->pev->frame = pev->frame;
		pGib->pev->targetname = m_sSet;

		return pGib;
	}
	else //not custom entity, other physics type
	{
          	CShot *pShot = CShot::CreateShot ( (char *)STRING( m_sMaster ), pev->size );
		if(!pShot && pShot->edict()) return NULL;
		
		pShot->pev->origin = vecPos;
		pShot->pev->velocity = vecVel;
		pShot->pev->renderamt = pev->renderamt;
		pShot->pev->rendermode = pev->rendermode;
		pShot->pev->rendercolor = pev->rendercolor;
		pShot->pev->renderfx = pev->renderfx;
		pShot->pev->target = pev->netname;
		pShot->pev->skin = pev->skin;
		pShot->pev->body = pev->body;
		pShot->pev->scale = pev->scale;
		pShot->pev->frame = pev->frame;
		pShot->pev->framerate = pev->framerate;
		pShot->pev->friction = pev->friction;
                    pShot->pev->movetype = pev->team;
                    pShot->pev->targetname = m_sSet;
                    
                    if (pev->health || pev->health == -1)
                    {
			//animate it
			if (pShot->pev->framerate && pShot->Frames() > 1.0)
			{
				pShot->AnimateAndDie( 10 );
				pShot->pev->dmg_take = gpGlobals->time + pev->health;
				pShot->SetNextThink( 0 );
				pShot->pev->dmgtime = gpGlobals->time;
			}
			else if( pev->health )
			{
				pShot->SetThink( Fadeout );
				pShot->SetNextThink(pev->health);
                              }
			else
			{
				pShot->SetThink( PVSRemove );
				pShot->SetNextThink( MAX_GIB_LIFETIME );
			}
		}
		return pShot;
	}
	return NULL;
}

//=========================================================
// LRC - Decal effect
//=========================================================
class CEnvDecal : public CPointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );
};

LINK_ENTITY_TO_CLASS( env_decal, CEnvDecal );

void CEnvDecal :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CEnvDecal::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg("Decal name %s\n\n", GetStringForDecalName( pev->message ));
	}
	else
	{
		int m_decal = UTIL_LoadDecalPreset( pev->message );
		switch(m_decal)
		{
		case 1: pev->skin = gDecals[ DECAL_GUNSHOT1 + RANDOM_LONG(0,4)].index; break;
		case 2: pev->skin = gDecals[ DECAL_BLOOD1 + RANDOM_LONG(0,5)].index; break;
		case 3: pev->skin = gDecals[ DECAL_YBLOOD1 + RANDOM_LONG(0,5)].index; break;
		case 4: pev->skin = gDecals[ DECAL_GLASSBREAK1 + RANDOM_LONG(0,2)].index; break;
		case 5: pev->skin = gDecals[ DECAL_BIGSHOT1 + RANDOM_LONG(0,4)].index; break;
		case 6: pev->skin = gDecals[ DECAL_SCORCH1 + RANDOM_LONG(0,1)].index; break;
		case 7: pev->skin = gDecals[ DECAL_SPIT1 + RANDOM_LONG(0,1)].index; break;
		default: pev->skin = DECAL_INDEX(STRING(m_decal)); break;
		}

		Vector vecPos = pev->origin;
		UTIL_MakeVectors(pev->angles);
		Vector vecOffs = gpGlobals->v_forward;
		vecOffs = vecOffs.Normalize() * MAP_HALFSIZE;
		TraceResult trace;
		UTIL_TraceLine( vecPos, vecPos+vecOffs, ignore_monsters, NULL, &trace );
		if (trace.flFraction == 1.0) return; // didn't hit anything, oh well
		int entityIndex = (short)ENTINDEX(trace.pHit);
		SFX_Decal( trace.vecEndPos, pev->skin, entityIndex, (int)VARS(trace.pHit)->modelindex );          
	}
}

//=========================================================
// 		env_warpball
//=========================================================
#define SF_REMOVE_ON_FIRE		0x0001
#define SF_KILL_CENTER		0x0002

class CEnvWarpBall : public CBaseLogic
{
public:
	void Precache( void );
	void Spawn( void ) { Precache(); }
	void KeyValue( KeyValueData *pkvd );
	void Think( void );
	void Affect( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector vecOrigin;
};
LINK_ENTITY_TO_CLASS( env_warpball, CEnvWarpBall );

void CEnvWarpBall :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "warp_target"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "damage_delay"))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CEnvWarpBall::Precache( void )
{
	UTIL_PrecacheModel( "sprites/lgtning.spr" );
	UTIL_PrecacheModel( "sprites/Fexplo1.spr" );
	UTIL_PrecacheSound( "debris/beamstart2.wav" );
	UTIL_PrecacheSound( "debris/beamstart7.wav" );
}

void CEnvWarpBall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	pActivator = m_hActivator; //save it for later
	
	if (useType == USE_TOGGLE || useType == USE_ON) Affect();
	else if( useType == USE_SET && value > 10) pev->button = value;
	else if( useType == USE_RESET ) pev->button = 100; //default radius
	else if( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "Radius: %d, Warp target %s\n\n", pev->button, STRING( pev->message ));
	}
}

void CEnvWarpBall::Affect( void )
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam *pBeam;
	CBaseEntity *pEntity = UTIL_FindEntityByTargetname ( NULL, STRING(pev->message));
	edict_t *pos;
		
	if(pEntity)//target found ?
	{
		vecOrigin = pEntity->pev->origin;
		pos = pEntity->edict();
	}
	else
	{         //use as center
		vecOrigin = pev->origin;
		pos = edict();
	}	
	EMIT_SOUND( pos, CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM );
	UTIL_ScreenShake( vecOrigin, 6, 160, 1.0, pev->button );
	CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", vecOrigin, TRUE );
	pSpr->SetTransparency(kRenderGlow,  77, 210, 130,  255, kRenderFxNoDissipation);
	pSpr->AnimateAndDie( 18 );

	EMIT_SOUND( pos, CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM );
	int iBeams = RANDOM_LONG(20, 40);
	while (iDrawn < iBeams && iTimes < (iBeams * 3))
	{
		vecDest = pev->button * (Vector(RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1)).Normalize());
		UTIL_TraceLine( vecOrigin, vecOrigin + vecDest, ignore_monsters, NULL, &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate("sprites/lgtning.spr", 200);
			pBeam->PointsInit( vecOrigin, tr.vecEndPos );
			pBeam->SetColor( 20, 243, 20 );
			pBeam->SetNoise( 65 );
			pBeam->SetBrightness( 220 );
			pBeam->SetWidth( 30 );
			pBeam->SetScrollRate( 35 );
			pBeam->SetThink( Remove );
			pBeam->pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.5, 1.6);
		}
		iTimes++;
	}

	if ( pev->spawnflags & SF_REMOVE_ON_FIRE )
		UTIL_Remove( this );
	else SetNextThink( pev->frags );
}

void CEnvWarpBall::Think( void )
{
	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
 
	if ( pev->spawnflags & SF_KILL_CENTER ) // Blue-Shift strange feature
	{
		CBaseEntity *pMonster = NULL;
		while ((pMonster = UTIL_FindEntityInSphere( pMonster, vecOrigin, 72 )) != NULL)
		{
			if ( FBitSet( pMonster->pev->flags, FL_MONSTER ) || FBitSet( pMonster->pev->flags, FL_CLIENT ))
				pMonster->TakeDamage ( pev, pev, 100, DMG_GENERIC );
		}
	}
	if ( pev->spawnflags & SF_REMOVE_ON_FIRE ) UTIL_Remove( this );
}

//=======================================================================
// 		   base beams ()
//=======================================================================
LINK_ENTITY_TO_CLASS( beam, CBeam );

void CBeam::Spawn( void )
{
	pev->solid = SOLID_NOT;
}

const Vector &CBeam::GetStartPos( void )
{
	int	type = GetType();

	if( type == BEAM_ENTS )
	{
		edict_t	*pent = GetStartEntity();

		if ( pent )
			return pent->v.origin;
	}
	return pev->origin;
}

const Vector &CBeam::GetEndPos( void )
{
	int	type = GetType();

	if( type == BEAM_ENTS || type == BEAM_ENTPOINT )
	{
		edict_t *pent =  GetEndEntity();

		if ( pent )
			return pent->v.oldorigin;
          }
	return pev->oldorigin;
}

CBeam *CBeam::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam = GetClassPtr( (CBeam *)NULL );
	pBeam->pev->classname = MAKE_STRING( "beam" );
	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}

void CBeam::BeamInit( const char *pSpriteName, int width )
{
	SetObjectClass( ED_BEAM );

	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING( pSpriteName );
	SetTexture( UTIL_PrecacheModel( pev->model ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}

void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::PointEntInit( const Vector &start, edict_t *pEnd )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( pEnd );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( edict_t *pStart, edict_t *pEnd )
{
	SetType( BEAM_ENTS );
	SetStartEntity( pStart );
	SetEndEntity( pEnd );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::RelinkBeam( void )
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->mins.x = min( startPos.x, endPos.x );
	pev->mins.y = min( startPos.y, endPos.y );
	pev->mins.z = min( startPos.z, endPos.z );
	pev->maxs.x = max( startPos.x, endPos.x );
	pev->maxs.y = max( startPos.y, endPos.y );
	pev->maxs.z = max( startPos.z, endPos.z );
	pev->mins = pev->mins - pev->origin;
	pev->maxs = pev->maxs - pev->origin;

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
}

void CBeam::SetObjectCollisionBox( void )
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->absmin.x = min( startPos.x, endPos.x );
	pev->absmin.y = min( startPos.y, endPos.y );
	pev->absmin.z = min( startPos.z, endPos.z );
	pev->absmax.x = max( startPos.x, endPos.x );
	pev->absmax.y = max( startPos.y, endPos.y );
	pev->absmax.z = max( startPos.z, endPos.z );
}

void CBeam::Touch( CBaseEntity *pOther )
{
	if ( pOther->pev->flags & (FL_CLIENT | FL_MONSTER) )
	{
		if ( pev->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
	}
}

CBaseEntity* CBeam::GetTripEntity( TraceResult *ptr )
{
	CBaseEntity* pTrip;

	if (ptr->flFraction == 1.0 || ptr->pHit == NULL) return NULL;

	pTrip = CBaseEntity::Instance(ptr->pHit);
	if (pTrip == NULL) return NULL;

	if (pTrip->pev->flags & (FL_CLIENT | FL_MONSTER )  && !pTrip->IsPushable())//physics ents can move too
		return pTrip;
	return NULL;
}

void CBeam::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();
	if ( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance(ptr->pHit);
		if ( pHit )
		{
			if ( pev->dmg > 0 )
			{
				ClearMultiDamage();
				pHit->TraceAttack( pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - pev->origin).Normalize(), ptr, DMG_ENERGYBEAM );
				ApplyMultiDamage( pev, pev );
				if ( pev->dmg > 40 && pHit->IsBSPModel())//wall damage
				{
					UTIL_DecalTrace( ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0,4) );
				}
			}
			else pHit->TakeHealth( -(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC );
		}
	}
	pev->dmgtime = gpGlobals->time;
}

CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;
	while ((pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL)
	{
		total++;
		if (RANDOM_LONG(0,total-1) < 1) pEntity = pNewEntity;
	}
	return pEntity;
}

void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if ( pev->dmg > 100 ) UTIL_Sparks( start );
	if ( pev->dmg > 40 ) UTIL_Sparks( end );
}

//=======================================================================
// 		   env_beam - toggleable beam
//=======================================================================
class CEnvBeam : public CBeam
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );
	void	Think( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
          int	IsPointEntity( CBaseEntity *pEnt );
	
	void	BeamUpdatePoints( void );
	void	BeamUpdateVars( void );

	CBaseEntity *pEnd;
};
LINK_ENTITY_TO_CLASS( env_beam, CEnvBeam );

void CEnvBeam::Precache( void )
{
	pev->team = UTIL_PrecacheModel( pev->message, "sprites/laserbeam.spr" );
	CBeam::Precache();
}

int CEnvBeam::IsPointEntity( CBaseEntity *pEnt )
{
	if( pEnt->pev->modelindex && ( pEnt->m_iClassType != ED_BEAM ))
		return 0;
	return 1;
}

void CEnvBeam::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "endpoint"))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sprite"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atoi(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "noisewidth"))
	{
		SetNoise( atoi(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shade"))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBeam::KeyValue( pkvd );
}

void CEnvBeam::BeamUpdateVars( void )
{
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	SetTexture( pev->team );

	BeamUpdatePoints();
	SetFrame( 0 );
	SetScrollRate( pev->speed );
	if ( pev->frags == 1 ) SetFlags( FBEAM_SHADEIN );
	if ( pev->frags == 2 ) SetFlags( FBEAM_SHADEOUT );
	if ( pev->renderamt == 255 ) SetFlags( FBEAM_SOLID );
	else SetFlags( 0 );
}

void CEnvBeam::Activate( void )
{
	BeamUpdateVars();
	CBeam::Activate();
}


void CEnvBeam::BeamUpdatePoints( void )
{
	int beamType;

	pEnd  = UTIL_FindEntityByTargetname ( NULL, STRING(pev->netname) );
	if( !pEnd ) return;

	int pointEnd = IsPointEntity( pEnd );

	beamType = BEAM_ENTS;

	if ( !pointEnd )
		beamType = BEAM_ENTPOINT;
	else beamType = BEAM_POINTS;

	SetType( beamType );

	if ( beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE )
	{
		SetStartPos( pev->origin );
		if ( beamType == BEAM_POINTS || beamType == BEAM_HOSE )
			SetEndPos( pEnd->pev->origin );
		else SetEndEntity( ENT( pEnd->pev ) );
	}
	else
	{
		SetStartEntity( ENT( pev ) );
		SetEndEntity( ENT( pEnd->pev ) );
	}

	RelinkBeam();
}

void CEnvBeam::Spawn( void )
{
	SetObjectClass( ED_BEAM );
	UTIL_SetModel( edict(), "sprites/null.spr" ); // beam start point
	pev->solid = SOLID_NOT; // remove model & collisions

	Precache();

	if( FStringNull( pev->netname ))
	{
		ALERT( at_warning, "%s end entity not found!\n", STRING( pev->classname ));
		UTIL_Remove( this );
		return;
	}

	pev->dmgtime = gpGlobals->time;
	if( pev->rendercolor == g_vecZero ) pev->rendercolor = Vector( 255, 255, 255 );
	if( pev->spawnflags & SF_START_ON ) Use( this, this, USE_ON, 0 );
}

void CEnvBeam::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		m_iState = STATE_ON;
		BeamUpdatePoints();
		pev->effects &= ~EF_NODRAW;
		SetNextThink( 0 );
		pev->dmgtime = gpGlobals->time;
	}
	else if (useType == USE_OFF)
	{
		m_iState = STATE_OFF;
		pev->effects |= EF_NODRAW;
		DontThink();		
	}
	else if(useType == USE_SET) //set new damage level
	{
		pev->dmg = value;
	}
	else if(useType == USE_RESET) //set new brightness
	{
		if(value)pev->renderamt = value;
		BeamUpdateVars();
	}
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		Msg( "State: %s, Damage %g\n", GetStringForState( GetState()), pev->dmg );
		Msg("Beam model %s\n", STRING( pev->message ));
	}
}

void CEnvBeam::Think( void )
{
	if ( pev->dmg || !FStringNull( pev->target )) // apply damage & trip entity
	{
		TraceResult tr;
		UTIL_TraceLine( pev->origin, pEnd->pev->origin, dont_ignore_monsters, NULL, &tr );
		BeamDamage( &tr );

		CBaseEntity* pTrip = GetTripEntity( &tr );
		if (pTrip)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				UTIL_FireTargets(pev->target, pTrip, this, USE_TOGGLE);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else	pev->spawnflags &= ~SF_BEAM_TRIPPED;
	}
	SetNextThink( 0.1 );
}

//=======================================================================
// 		   env_laser - classic HALF-LIFE laser
//=======================================================================
LINK_ENTITY_TO_CLASS( env_laser, CLaser );

TYPEDESCRIPTION	CLaser::m_SaveData[] = 
{
	DEFINE_FIELD( CLaser, m_pStartSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CLaser, m_pEndSprite, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CLaser, CBeam );

void CLaser::Spawn( void )
{
	SetObjectClass( ED_BEAM );
	pev->frame = 0;
	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache();
}

void CLaser::Activate( void )
{
	if ( m_pStartSprite && m_pEndSprite ) 
		EntsInit( m_pStartSprite->edict(), m_pEndSprite->edict() );
}

void CLaser::SetObjectCollisionBox( void )
{
	if ( m_pStartSprite && m_pEndSprite )
	{ 
		const Vector &startPos = m_pStartSprite->pev->origin, &endPos = m_pEndSprite->pev->origin;
		pev->absmin.x = min( startPos.x, endPos.x );
		pev->absmin.y = min( startPos.y, endPos.y );
		pev->absmin.z = min( startPos.z, endPos.z );
		pev->absmax.x = max( startPos.x, endPos.x );
		pev->absmax.y = max( startPos.y, endPos.y );
		pev->absmax.z = max( startPos.z, endPos.z );
	}
	else
	{
		pev->absmin = g_vecZero;
		pev->absmax = g_vecZero;
	}
}

void CLaser::PostSpawn( void )
{
	if ( !FStringNull( pev->netname ))
	{
		m_pStartSprite = CSprite::SpriteCreate( STRING( pev->netname ), pev->origin, TRUE );
		if ( m_pStartSprite ) m_pStartSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
		else m_pStartSprite = CSprite::SpriteCreate( "sprites/null.spr", pev->origin, TRUE );
	}
	else m_pStartSprite = CSprite::SpriteCreate( "sprites/null.spr", pev->origin, TRUE );
	m_pEndSprite = CSprite::SpriteCreate( "sprites/null.spr", pev->origin, TRUE );
	
	if ( pev->spawnflags & SF_START_ON )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}

void CLaser::Precache( void )
{
	UTIL_PrecacheModel( pev->message, "sprites/laserbeam.spr" );
	if( pev->netname ) UTIL_PrecacheModel( pev->netname );
}

void CLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "sprite"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "startsprite"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "noisewidth"))
	{
		SetNoise( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBeam::KeyValue( pkvd );
}

void CLaser::TurnOff( void )
{
	SetBits(pev->effects, EF_NODRAW );
	m_iState = STATE_OFF;
	if ( m_pStartSprite )
	{
		m_pStartSprite->TurnOff();
		UTIL_SetVelocity(m_pStartSprite, g_vecZero);
	}
	if ( m_pEndSprite )
	{
		m_pEndSprite->TurnOff();
		UTIL_SetVelocity(m_pEndSprite, g_vecZero);
	}
	DontThink();
}

void CLaser::TurnOn( void )
{
	ClearBits( pev->effects, EF_NODRAW );

	if ( m_pStartSprite )
		m_pStartSprite->TurnOn();

	if ( m_pEndSprite )
		m_pEndSprite->TurnOn();

	pev->dmgtime = gpGlobals->time;
          m_iState = STATE_ON;
	SetScrollRate( pev->speed );
	SetFlags( FBEAM_SHADEOUT );
	SetNextThink( 0 );
}

void CLaser::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if ( useType == USE_ON ) TurnOn();
	else if ( useType == USE_OFF ) TurnOff();
	else if ( useType == USE_SET ) // set new damage level
	{
		pev->dmg = value;
	}
	else if( useType == USE_RESET ) // set new brightness
	{
		if( value ) pev->renderamt = value;
		RelinkBeam();
	}
	else if ( useType == USE_SHOWINFO )
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, Damage %g\n", GetStringForState( GetState()), pev->dmg );
		ALERT( at_console, "Laser model %s\n", STRING( pev->message ));
	}
}

void CLaser::FireAtPoint( Vector startpos, TraceResult &tr )
{
	if ( m_pStartSprite && m_pEndSprite )
	{
		UTIL_SetVelocity( m_pStartSprite, ( startpos - m_pStartSprite->pev->origin ) * 100 );
		UTIL_SetVelocity( m_pEndSprite, ( tr.vecEndPos - m_pEndSprite->pev->origin ) * 100 );
	}

	BeamDamage( &tr );
	DoSparks( startpos, tr.vecEndPos );
}

void CLaser:: Think( void )
{
	Vector startpos = pev->origin;
	TraceResult tr;
          
	UTIL_MakeVectors( pev->angles );
	Vector vecProject = startpos + gpGlobals->v_forward * MAP_HALFSIZE;
	UTIL_TraceLine( startpos, vecProject, dont_ignore_monsters, ignore_glass, NULL, &tr );
	FireAtPoint( startpos, tr );
	if( m_iState == STATE_ON ) SetNextThink( 0.01 ); //!!!
}

//=========================================================
//	env_lightning - random zap strike
//=========================================================
class CEnvLightning : public CBeam
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Think( void );
	void	RandomPoint( Vector &vecSrc );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
          void	BeamDamage( TraceResult *ptr );
};
LINK_ENTITY_TO_CLASS( env_lightning, CEnvLightning );

void CEnvLightning::Precache( void )
{
	pev->team = UTIL_PrecacheModel( pev->message, "sprites/laserbeam.spr" );
	CBeam::Precache();
}

void CEnvLightning::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "lifetime"))//beam lifetime (leave blank for toggle beam)
	{
		pev->armorvalue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sprite"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "noisewidth"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBeam::KeyValue( pkvd );
}

void CEnvLightning::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();
	if ( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance(ptr->pHit);
		if ( pHit )
		{
			if (pev->dmg > 0)
			{
				ClearMultiDamage();
				pHit->TraceAttack( pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - pev->origin).Normalize(), ptr, DMG_ENERGYBEAM );
				ApplyMultiDamage( pev, pev );
				if ( pev->dmg > 40 && pHit->IsBSPModel())//wall damage
				{
					UTIL_DecalTrace( ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0,4) );
				}
			}
			else pHit->TakeHealth( -(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC );
		}
	}
	pev->dmgtime = gpGlobals->time;
}

void CEnvLightning::RandomPoint( Vector &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * pev->frags, ignore_monsters, ENT(pev), &tr1 );

		if ((tr1.vecEndPos - vecSrc).Length() < pev->frags * 0.1) continue;
		if (tr1.flFraction == 1.0) continue;

		SFX_Zap ( pev, vecSrc, tr1.vecEndPos );
                    DoSparks( vecSrc, tr1.vecEndPos );
		BeamDamage( &tr1 );
		UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
		break;
	}
}

void CEnvLightning::Spawn( void )
{
	UTIL_SetModel(edict(), "sprites/null.spr");//beam start point
	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache();

	pev->dmgtime = gpGlobals->time;
	if (pev->rendercolor == g_vecZero) pev->rendercolor = Vector(255, 255, 255);
	if(pev->spawnflags & SF_START_ON) Use( this, this, USE_ON, 0 );
}

void CEnvLightning::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		m_iState = STATE_ON;
		pev->effects &= ~EF_NODRAW;
		SetNextThink( 0 );
		pev->dmgtime = gpGlobals->time;
	}
	else if (useType == USE_OFF)
	{
		m_iState = STATE_OFF;
		pev->effects |= EF_NODRAW;
		DontThink();		
	}
	else if(useType == USE_SET)
	{
	}
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, MaxDelay %.2f\n\n", GetStringForState( GetState()), m_flDelay );
	}
}

void CEnvLightning::Think( void )
{
	RandomPoint( pev->origin );
	SetNextThink( pev->armorvalue + RANDOM_FLOAT( 0, m_flDelay ) );
}

//=======================================================================
// 		   env_beamring - make ring from beams
//=======================================================================
class CEnvBeamRing : public CBeam
{
public:
	void	Spawn( void );
	void	Think( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( env_beamring, CEnvBeamRing );

void CEnvBeamRing::Precache( void )
{
	pev->team = UTIL_PrecacheModel( pev->message, "sprites/laserbeam.spr" );
	CBeam::Precache();
}

void CEnvBeamRing::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "lifetime"))//beam lifetime (leave blank for toggle beam)
	{
		pev->armorvalue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sprite"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "noisewidth"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBeam::KeyValue( pkvd );
}

void CEnvBeamRing::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (useType == USE_TOGGLE)
	{
		if(m_iState == STATE_ON) useType = USE_OFF;
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		m_iState = STATE_ON;
		pev->effects &= ~EF_NODRAW;
		SetNextThink( 0 );
		pev->dmgtime = gpGlobals->time;
	}
	else if (useType == USE_OFF)
	{
		m_iState = STATE_OFF;
		pev->effects |= EF_NODRAW;
		DontThink();		
	}
	else if(useType == USE_SET)
	{
	}
	else if (useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
		ALERT( at_console, "State: %s, MaxDelay %.2f\n\n", GetStringForState( GetState()), m_flDelay );
	}
}

void CEnvBeamRing::Spawn( void )
{
	UTIL_SetModel(edict(), "sprites/null.spr");//beam start point
	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache();

	pev->dmgtime = gpGlobals->time;
	if (pev->rendercolor == g_vecZero) pev->rendercolor = Vector(255, 255, 255);
	if(pev->frags == 0)pev->frags = 20;

	//create second point
	Vector vecAngles, vecPos;
	vecAngles = pev->angles;
	UTIL_MakeVectors(vecAngles);
	vecPos = pev->origin + (gpGlobals->v_forward * pev->frags);
	CBaseEntity *pRing = CBaseEntity::Create( "info_target", vecPos, vecAngles, edict() );
	if(m_iParent)pRing->m_iParent = m_iParent;
	pev->enemy = pRing->edict(); //save our pointer

	if(pev->spawnflags & SF_START_ON) Use( this, this, USE_ON, 0 );
}

void CEnvBeamRing::Think( void )
{
	CBaseEntity *pRing = CBaseEntity::Instance(pev->enemy);
	SFX_Ring( pev, pRing->pev );
	SetNextThink( pev->armorvalue + RANDOM_FLOAT( 0, m_flDelay ) );
}