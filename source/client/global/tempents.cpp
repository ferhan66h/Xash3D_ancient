//=======================================================================
//			Copyright XashXT Group 2008 �
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "effects_api.h"
#include "te_message.h"
#include "hud.h"

void HUD_CreateEntities( void )
{
}

void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity )
{
	float	pitch;

	switch( event->event )
	{
	case 5001:
		// MullzeFlash at attachment 0
		break;
	case 5011:
		// MullzeFlash at attachment 1
		break;
	case 5021:
		// MullzeFlash at attachment 2
		break;
	case 5031:
		// MullzeFlash at attachment 3
		break;
	case 5002:
		// SparkEffect at attachment 0
		break;
	case 5004:		
		// Client side sound
		CL_PlaySound( event->options, 1.0f, entity->v.attachment[0] );
//		ALERT( at_console, "CL_PlaySound( %s )\n", event->options );
		break;
	case 5005:		
		// Client side sound with random pitch
		pitch = 85 + RANDOM_LONG( 0, 0x1F );
//		ALERT( at_console, "CL_PlaySound( %s )\n", event->options );
		CL_PlaySound( event->options, RANDOM_FLOAT( 0.7f, 0.9f ), entity->v.attachment[0], pitch );
		break;
	case 5050:
		// Special event for displacer
		break;
	case 5060:
	          // eject shellEV_EjectShell( event, entity );
		break;
	default:
		break;
	}
}

/*
=================
CL_ExplosionParticles
=================
*/
void CL_ExplosionParticles( const Vector pos )
{
	cparticle_t	src;
	int		flags;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	flags = (PARTICLE_STRETCH|PARTICLE_BOUNCE|PARTICLE_FRICTION);

	for( int i = 0; i < 384; i++ )
	{
		src.origin.x = pos.x + RANDOM_LONG( -16, 16 );
		src.origin.y = pos.y + RANDOM_LONG( -16, 16 );
		src.origin.z = pos.z + RANDOM_LONG( -16, 16 );
		src.velocity.x = RANDOM_LONG( -256, 256 );
		src.velocity.y = RANDOM_LONG( -256, 256 );
		src.velocity.z = RANDOM_LONG( -256, 256 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -60 + RANDOM_FLOAT( -30, 30 );
		src.color = Vector( 1, 1, 1 );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -3.0;
		src.radius = 0.5 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}

	// smoke
	flags = PARTICLE_VERTEXLIGHT;

	for( i = 0; i < 5; i++ )
	{
		src.origin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.origin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.origin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.velocity.x = RANDOM_FLOAT( -10, 10 );
		src.velocity.y = RANDOM_FLOAT( -10, 10 );
		src.velocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0, 0, 0 );
		src.colorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.alpha = 0.5;
		src.alphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.radius = 30 + RANDOM_FLOAT( -15, 15 );
		src.radiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BubbleParticles
=================
*/
void CL_BubbleParticles( const Vector org, int count, float magnitude )
{
	cparticle_t	src;
	int		i;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	for( i = 0; i < count; i++ )
	{
		src.origin.x = org[0] + RANDOM_FLOAT( -magnitude, magnitude );
		src.origin.y = org[1] + RANDOM_FLOAT( -magnitude, magnitude );
		src.origin.z = org[2] + RANDOM_FLOAT( -magnitude, magnitude );
		src.velocity.x = RANDOM_FLOAT( -5, 5 );
		src.velocity.y = RANDOM_FLOAT( -5, 5 );
		src.velocity.z = RANDOM_FLOAT( -5, 5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 1, 1, 1 );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.radius = 1 + RANDOM_FLOAT( -0.5, 0.5 );
		src.radiusVelocity = 0;
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = 0;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hBubble, PARTICLE_UNDERWATER ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CL_BulletParticles( const Vector org, const Vector dir )
{
	cparticle_t	src;
	int		flags;
	int		i, count;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	count = RANDOM_LONG( 3, 8 );

	if( POINT_CONTENTS( org ) & MASK_WATER )
	{
		CL_BubbleParticles( org, count, 0 );
		return;
	}

	// sparks
	flags = (PARTICLE_STRETCH|PARTICLE_BOUNCE|PARTICLE_FRICTION);

	for( i = 0; i < count; i++ )
	{
		src.origin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}

	// smoke
	flags = PARTICLE_VERTEXLIGHT;

	for( i = 0; i < 3; i++ )
	{
		src.origin.x = org[0] + dir[0] * 5 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 5 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 5 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = RANDOM_FLOAT( -2.5, 2.5 );
		src.velocity.y = RANDOM_FLOAT( -2.5, 2.5 );
		src.velocity.z = RANDOM_FLOAT( -2.5, 2.5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0.4, 0.4, 0.4 );
		src.colorVelocity = Vector( 0.2, 0.2, 0.2 );
		src.alpha = 0.5;
		src.alphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.radius = 3 + RANDOM_FLOAT( -1.5, 1.5 );
		src.radiusVelocity = 5 + RANDOM_FLOAT( -2.5, 2.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_TeleportParticles

creates a particle box
=================
*/
void CL_TeleportParticles( const Vector org )
{
	cparticle_t	src;
	vec3_t		dir;
	float		vel, color;
	int		x, y, z;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	for( x = -16; x <= 16; x += 4 )
	{
		for( y = -16; y <= 16; y += 4 )
		{
			for( z = -16; z <= 32; z += 4 )
			{
				dir = Vector( y*8, x*8, z*8 );
				dir.Normalize();

				vel = 50 + RANDOM_LONG( 0, 64 );
				color = RANDOM_FLOAT( 0.1, 0.3 );
				src.origin.x = org[0] + x + RANDOM_LONG( 0, 4 );
				src.origin.y = org[1] + y + RANDOM_LONG( 0, 4 );
				src.origin.z = org[2] + z + RANDOM_LONG( 0, 4 );
				src.velocity[0] = dir[0] * vel;
				src.velocity[1] = dir[1] * vel;
				src.velocity[2] = dir[2] * vel;
				src.accel[0] = 0;
				src.accel[1] = 0;
				src.accel[2] = -40;
				src.color = Vector( color, color, color );
				src.colorVelocity = Vector( 0, 0, 0 );
				src.alpha = 1.0;
				src.alphaVelocity = -1.0 / (0.3 + RANDOM_LONG( 0, 0.16 ));
				src.radius = 2;
				src.radiusVelocity = 0;
				src.length = 1;
				src.lengthVelocity = 0;
				src.rotation = 0;

				if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hGlowParticle, 0 ))
					return;

			}
		}
	}
}

void CL_PlaceDecal( Vector pos, Vector dir, float scale, HSPRITE hDecal )
{
	float	rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	int	flags = DECAL_FADEALPHA;

	g_engfuncs.pEfxAPI->R_SetDecal( pos, dir, rgba, RANDOM_LONG( 0, 360 ), scale, hDecal, flags );
}

void CL_AllocDLight( Vector pos, float radius, float decay, float time )
{
	float	rgb[3] = { 1.0f, 1.0f, 1.0f };

	g_engfuncs.pEfxAPI->CL_AllocDLight( pos, rgb, radius, decay, time, 0 );
}

void HUD_ParseTempEntity( void )
{
	Vector	pos, dir, color;
	float	time, radius, decay;
	int	flags, scale;

	switch( READ_BYTE() )
	{
	case TE_GUNSHOT:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		CL_BulletParticles( pos, Vector( 0, 0, -1 ));
		break;
	case TE_GUNSHOTDECAL:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		READ_SHORT(); // FIXME: skip entindex
		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, 10, dir );
		CL_BulletParticles( pos, Vector( 0, 0, -1 ));
		CL_PlaceDecal( pos, dir, 10, g_engfuncs.pEfxAPI->CL_DecalIndex( READ_BYTE() ));
		break;
	case TE_DECAL:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, 10, dir );
		CL_PlaceDecal( pos, dir, 10, g_engfuncs.pEfxAPI->CL_DecalIndex( READ_BYTE() ));
		READ_SHORT(); // FIXME: skip entindex
		break;	
	case TE_EXPLOSION:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		READ_SHORT(); // FIXME: use sprite index as shader index
		scale = READ_BYTE();
		READ_BYTE(); // FIXME: use framerate for shader
		flags = READ_BYTE();

		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, scale, dir );
		if(!(flags & TE_EXPLFLAG_NOPARTICLES )) CL_ExplosionParticles( pos );
		if( RANDOM_LONG( 0, 1 ))
			CL_PlaceDecal( pos, dir, scale, g_engfuncs.pEfxAPI->CL_DecalIndexFromName( "{scorch1" ));
		else CL_PlaceDecal( pos, dir, scale, g_engfuncs.pEfxAPI->CL_DecalIndexFromName( "{scorch2" )); 
		if(!(flags & TE_EXPLFLAG_NODLIGHTS )) CL_AllocDLight( pos, 250.0f, 0.28f, 0.8f );
		if(!(flags & TE_EXPLFLAG_NOSOUND )) CL_PlaySound( "weapons/explode3.wav", 1.0f, pos );
		break;
	case TE_TELEPORT:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		break;
	case TE_DLIGHT:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		radius = (float)READ_BYTE() * 10.0f;
		color.x = (float)READ_BYTE() / 255.0f;
		color.y = (float)READ_BYTE() / 255.0f;
		color.z = (float)READ_BYTE() / 255.0f;
		time = (float)READ_BYTE() * 0.1f;
		decay = (float)READ_BYTE() * 0.1f;
		g_engfuncs.pEfxAPI->CL_AllocDLight( pos, color, radius, decay, time, 0 );
		break;
	}
}