//=======================================================================
//			Copyright XashXT Group 2007 �
//		        r_sprite.c - render sprite models
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "byteorder.h"
#include "const.h"

/*
=============================================================

  SPRITE MODELS

=============================================================
*/
string		frame_prefix;
uint		frame_type;
uint		group_num;
uint		surfaceParm;
string		sp_name;
ref_shader_t	**frames = NULL;
	
/*
====================
Sprite model loader
====================
*/
dframetype_t *R_SpriteLoadFrame( rmodel_t *mod, void *pin, mspriteframe_t **ppframe, int framenum )
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	string		name, shadername;

	// build uinque frame name
	if( !sp_name[0] ) FS_FileBase( mod->name, sp_name );
	com.snprintf( name, MAX_STRING, "Sprite( %s_%s_%i%i )", sp_name, frame_prefix, framenum/10, framenum%10 );
	
	pinframe = (dspriteframe_t *)pin;
	SwapBlock((int *)pinframe, sizeof( dspriteframe_t ));

	// setup frame description
	pspriteframe = Mem_Alloc( mod->mempool, sizeof( mspriteframe_t ));
	pspriteframe->width = pinframe->width;
	pspriteframe->height = pinframe->height;
	pspriteframe->up = pinframe->origin[1];
	pspriteframe->left = pinframe->origin[0];
	pspriteframe->down = pinframe->origin[1] - pinframe->height;
	pspriteframe->right = pinframe->width + pinframe->origin[0];
	pspriteframe->radius = com.sqrt((pinframe->width * pinframe->width) + (pinframe->height * pinframe->height));
	pspriteframe->texture = R_FindTexture( name, (byte *)pin, pinframe->width * pinframe->height, 0, 0, 0 );
	*ppframe = pspriteframe;

	R_ShaderSetSpriteTexture( pspriteframe->texture );

	if( frame_type == SPR_SINGLE )
	{
		com.snprintf( shadername, MAX_STRING, "sprites/%s.spr/%s_%i%i )", sp_name, frame_prefix, framenum/10, framenum%10 );
		pspriteframe->shader = R_FindShader( shadername, SHADER_SPRITE, surfaceParm )->shadernum;
		frames = Mem_Realloc( mod->mempool, frames, sizeof( ref_shader_t* ) * (mod->numShaders + 1));
		frames[mod->numShaders++] = &r_shaders[pspriteframe->shader];
	}
	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
}

dframetype_t *R_SpriteLoadGroup( rmodel_t *mod, void * pin, mspriteframe_t **ppframe, int framenum )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	shader_t		group_shader;
	int		i, groupsize, numframes;
	string		shadername;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );

	groupsize = sizeof(mspritegroup_t) + (numframes - 1) * sizeof( pspritegroup->frames[0] );
	pspritegroup = Mem_Alloc( mod->mempool, groupsize );
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc( mod->mempool, numframes * sizeof( float ));
	pspritegroup->intervals = poutintervals;

	for( i = 0; i < numframes; i++ )
	{
		*poutintervals = LittleFloat( pin_intervals->interval );
		if( *poutintervals <= 0.0 ) *poutintervals = 1.0f; // set error value
		if( frame_type == SPR_GROUP ) R_ShaderAddSpriteIntervals( *poutintervals );
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame( mod, ptemp, &pspritegroup->frames[i], framenum * 10 + i );
	}

	com.snprintf( shadername, MAX_STRING, "sprites/%s.spr/%s_%i%i", sp_name, frame_prefix, group_num/10, group_num%10 );
	group_shader = R_FindShader( shadername, SHADER_SPRITE, surfaceParm )->shadernum;
	frames = Mem_Realloc( mod->mempool, frames, sizeof( ref_shader_t* ) * (mod->numShaders + 1));
	frames[mod->numShaders++] = &r_shaders[group_shader];

	// apply this shader for all frames in group
	for( i = 0; i < numframes; i++ )
		pspritegroup->frames[i]->shader = group_shader;
	group_num++;

	return (dframetype_t *)ptemp;
}

void R_SpriteLoadModel( rmodel_t *mod, const void *buffer )
{
	dsprite_t		*pin;
	short		*numi;
	msprite_t		*psprite;
	dframetype_t	*pframetype;
	int		i, size, numframes;

	pin = (dsprite_t *)buffer;
	i = LittleLong( pin->version );
		
	if( i != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", mod->name, i, SPRITE_VERSION );
		return;
	}

	numframes = LittleLong( pin->numframes );
	size = sizeof (msprite_t) + (numframes - 1) * sizeof( psprite->frames );

	psprite = Mem_Alloc( mod->mempool, size );
	mod->extradata = psprite; // make link to extradata
	mod->numShaders = surfaceParm = 0; // reset frames
	
	psprite->type = LittleLong( pin->type );
	psprite->rendermode = LittleLong( pin->texFormat );
	psprite->numframes = numframes;
	mod->mins[0] = mod->mins[1] = -LittleLong( pin->bounds[0] ) / 2;
	mod->maxs[0] = mod->maxs[1] = LittleLong( pin->bounds[0] ) / 2;
	mod->mins[2] = -LittleLong( pin->bounds[1] ) / 2;
	mod->maxs[2] = LittleLong( pin->bounds[1] ) / 2;
	numi = (short *)(pin + 1);

	if( LittleShort( *numi ) == 256 )
	{	
		byte	*src = (byte *)(numi+1);
		rgbdata_t	*pal;
	
		// install palette
		switch( psprite->rendermode )
		{
		case SPR_ADDGLOW:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			surfaceParm |= SURF_GLOW;
			break;
		case SPR_ADDITIVE:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			surfaceParm |= SURF_ADDITIVE;
			break;
                    case SPR_INDEXALPHA:
			pal = FS_LoadImage( "#decal.pal", src, 768 );
			surfaceParm |= SURF_ALPHA;
			break;
		case SPR_ALPHTEST:		
			pal = FS_LoadImage( "#transparent.pal", src, 768 );
			surfaceParm |= SURF_ALPHA;
                              break;
		case SPR_NORMAL:
		default:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			break;
		}
		pframetype = (dframetype_t *)(src + 768);
		FS_FreeImage( pal ); // palette installed, no reason to keep this data
	}
	else 
	{
		MsgDev( D_ERROR, "%s has wrong number of palette colors %i (should be 256)\n", mod->name, numi );
		return;
	}

	if( numframes < 1 )
	{
		MsgDev( D_ERROR, "%s has invalid # of frames: %d\n", mod->name, numframes );
		return;
	}

	MsgDev( D_LOAD, "%s, rendermode %d\n", mod->name, psprite->rendermode );
	mod->registration_sequence = registration_sequence;
	frames = NULL; // invalidate pointer
	sp_name[0] = 0;
	group_num = 0;

	for( i = 0; i < numframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );
		psprite->frames[i].type = frametype;
		frame_type = frametype;
			
		switch( frametype )
		{
		case SPR_SINGLE:
			com.strncpy( frame_prefix, "one", MAX_STRING );
			pframetype = R_SpriteLoadFrame(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case SPR_GROUP:
			com.strncpy( frame_prefix, "grp", MAX_STRING );
			pframetype = R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case SPR_ANGLED:
			com.strncpy( frame_prefix, "ang", MAX_STRING );
			pframetype = R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}
	mod->shaders = frames; // setup texture links
}

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame( ref_entity_t *ent )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int		i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	psprite = ent->model->extradata;
	frame = ent->frame;

	if((frame >= psprite->numframes) || (frame < 0))
	{
		MsgDev( D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, ent->model->name );
		frame = 0;
	}

	if( psprite->frames[frame].type == SPR_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_GROUP) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		time = r_refdef.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if (pintervals[i] > targettime)
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == SPR_ANGLED )
	{
		// e.g. doom-style sprite monsters
		int angleframe = (int)((r_refdef.viewangles[1] - ent->angles[1])/360*8 + 0.5 - 4) & 7;
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}
	return pspriteframe;
}

/*
=================
R_AddSpriteModelToList

add spriteframe to list
=================
*/
void R_AddSpriteModelToList( ref_entity_t *entity )
{
	mspriteframe_t	*frame;

	frame = R_GetSpriteFrame( entity );

	if( R_CullSphere( entity->origin, frame->radius, MAX_CLIPFLAGS ))
		return;

	// copy frame params
	m_pCurrentEntity = entity;
	entity->radius = frame->radius;
	entity->rotation = 0;
	entity->shader = &r_shaders[frame->shader];

	// add it
	R_AddMeshToList( MESH_SPRITE, NULL, entity->shader, entity, 0 );
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel( void )
{
	mspriteframe_t	*frame;
	msprite_t		*psprite;
	vec3_t		forward, right, up;
	float		angle, sr, cr, alpha;
	vec3_t		point;
	ref_entity_t	*e;
	int		i;

	e = m_pCurrentEntity;
	frame = R_GetSpriteFrame( e );
	psprite = (msprite_t *)m_pRenderModel->extradata;

	// setup orientation
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		VectorCopy( e->matrix[0], forward );
		VectorCopy( e->matrix[1], right );
		VectorCopy( e->matrix[2], up );
		VectorScale( forward, 0.01, forward ); // to avoid z-fighting
		VectorSubtract( e->origin, forward, e->origin );
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet( right, e->origin[1] - r_origin[1], -(e->origin[0] - r_origin[0]), 0 );
		VectorNegate( r_forward, forward );
		VectorSet( up, 0, 0, 1 );
		VectorNormalize( right );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		VectorSet( right, r_forward[1], -r_forward[0], 0 );
		VectorNegate( r_forward, forward );
		VectorSet( up, 0, 0, 1 );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * (M_PI*2.0f/360.0f);
		sr = com.sin( angle );
		cr = com.cos( angle );
		for( i = 0; i < 3; i++ )
		{
			forward[i] = -r_forward[i];
			right[i] = -(r_right[i] * cr + r_up[i] * sr);
			up[i] = r_right[i] * -sr + r_up[i] * cr;
		}
		break;
	case SPR_FWD_PARALLEL:
	default: // normal sprite
		VectorNegate( r_forward, forward );
		VectorNegate( r_right, right );
		VectorCopy( r_up, up );
		break;
	}

	if((m_pCurrentEntity->rendermode == kRenderGlow) || (m_pCurrentShader->surfaceParm & SURF_GLOW))
	{
		if( ri.Trace( m_pCurrentEntity->origin, vec3_origin, vec3_origin, r_refdef.vieworg ))
		{ 
			float dist = VectorDistance( m_pCurrentEntity->origin, r_refdef.vieworg );
			e->scale = bound( 1.0, dist * 0.005f, 10.0f );
			alpha = bound( 0, dist / 1000, 1.0f );
			e->renderamt = 255 * alpha;
		}
		else return; // occluded
	}

	// draw it
	RB_CheckMeshOverflow( 6, 4 );

	GL_Begin( GL_QUADS );
		GL_TexCoord2f( 0, 0 );
		VectorMA( e->origin, frame->up * e->scale, up, point );
		VectorMA( point, frame->left * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );

		GL_TexCoord2f( 1, 0 );		
		VectorMA( e->origin, frame->up * e->scale, up, point );
		VectorMA( point, frame->right * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );

		GL_TexCoord2f( 1, 1 );
		VectorMA( e->origin, frame->down * e->scale, up, point );
		VectorMA( point, frame->right * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );

          	GL_TexCoord2f( 0, 1 );
		VectorMA( e->origin, frame->down * e->scale, up, point );
		VectorMA( point, frame->left * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );
	GL_End();
}