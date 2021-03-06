//=======================================================================
//			Copyright XashXT Group 2007 ?
//			r_studio.c - render studio models
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "clgame_api.h"
#include "tmpent_def.h"
#include "effects_api.h"
#include "const.h"

/*
=============================================================

  STUDIO MODELS

=============================================================
*/
float		(*m_protationmatrix)[4];
matrix4x4		*m_pbonestransform;

// lighting stuff
vec3_t		*m_pxformverts;
vec3_t		*m_pxformnorms;
vec3_t		*m_pxformlight;

// chrome stuff
vec2_t		*m_pchrome;
int		*m_pchromeage;
vec3_t		*m_pchromeup;
vec3_t		*m_pchromeright;

// player gait sequence stuff
int		m_fGaitEstimation;
float		m_flGaitMovement;

// misc variables
int		m_fDoInterp;			
dstudiomodel_t	*m_pSubModel;
dstudiobodyparts_t	*m_pBodyPart;
dstudiohdr_t	*m_pStudioHeader;
dstudiohdr_t	*m_pTextureHeader;
vec3_t		studio_mins, studio_maxs;
float		studio_radius;

// studio cvars
cvar_t		*r_studio_lerping;

typedef struct studioverts_s
{
	vec3_t		*verts;
	vec3_t		*norms;
	vec3_t		*light;			// light values
	vec2_t		*chrome;			// size match with numverts
	int		numverts;
	int		numnorms;
	int		m_nCachedFrame;		// to avoid transform it twice
} studioverts_t;

typedef struct studiovars_s
{
	lerpframe_t	*lerp;			// duplicate e->lerp pointer for consistency

	// cached values, valid only for CURRENT frame
	char		bonenames[MAXSTUDIOBONES][32];// used for attached entities 
	studioverts_t	*mesh[MAXSTUDIOMODELS];
	matrix4x4		rotationmatrix;
	matrix4x4		*bonestransform;
	vec3_t		*chromeright;
	vec3_t		*chromeup;
	int		*chromeage;
	int		numbones;
} studiovars_t;

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	int	i;

	m_pBodyPart = NULL;
	m_pStudioHeader = NULL;
	m_flGaitMovement = 1;

	r_studio_lerping = Cvar_Get( "r_studio_lerping", "1", CVAR_ARCHIVE, "enables studio model animation lerping" );

	for( i = 1; i < MAX_ENTITIES; i++ )
	{
		r_entities[i].mempool = NULL;
		r_entities[i].extradata = NULL;
	}
}

/*
====================
R_StudioFreeAllExtradata

release all ref_entity mempools
====================
*/
void R_StudioFreeAllExtradata( void )
{
	int	i;

	for( i = 1; i < MAX_ENTITIES; i++ )
	{
		Mem_FreePool( &r_entities[i].mempool );
		r_entities[i].extradata = NULL;
	}
	Mem_Set( r_entities, 0, sizeof( r_entities ));
}

/*
====================
R_StudioShutdown

====================
*/
void R_StudioShutdown( void )
{
	R_StudioFreeAllExtradata ();
}

/*
====================
R_StudioUpdateVars

nointerp version
====================
*/
void R_StudioUpdateVars( edict_t *in, ref_entity_t *out )
{
	int	i;

	out->lerp->latched.sequencetime = 0.0f;		// no lerping between sequences
	out->lerp->latched.gaitsequencetime = 0.0f;	// no lerping between gaitsequences
	out->lerp->latched.sequence = out->lerp->curstate.sequence = in->v.sequence; // keep an actual
	out->lerp->latched.animtime = out->lerp->curstate.animtime = in->v.animtime;

	VectorCopy( in->v.origin, out->lerp->curstate.origin );
	VectorCopy( out->lerp->curstate.origin, out->lerp->latched.origin );

	VectorCopy( in->v.angles, out->lerp->curstate.angles );
	VectorCopy( out->lerp->curstate.angles, out->lerp->latched.angles );

	// copy controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
		out->lerp->latched.controller[i] = out->lerp->curstate.controller[i] = in->v.controller[i];

	// copy blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
		out->lerp->latched.blending[i] = out->lerp->curstate.blending[i] = in->v.blending[i];
}

/*
====================
R_StudioUpdateVarsInterpolant

====================
*/
void R_StudioUpdateVarsInterpolant( edict_t *in, ref_entity_t *out )
{
	int	i;
		
	// sequence has changed, hold the previous sequence info
	if( in->v.sequence != out->lerp->curstate.sequence )
	{
		if( out->ent_type == ED_CLIENT )
			out->lerp->latched.sequencetime = out->lerp->curstate.animtime + 0.01f;
		else out->lerp->latched.sequencetime = out->lerp->curstate.animtime + 0.1f;
			
		// save current blends to right lerping from last sequence
		for( i = 0; i < MAXSTUDIOBLENDS; i++ )
			out->lerp->latched.seqblending[i] = out->lerp->curstate.blending[i];
		out->lerp->latched.sequence = out->lerp->curstate.sequence;	// save old sequence
	}
	out->lerp->curstate.sequence = in->v.sequence; // keep an actual

	if( in->v.gaitsequence != out->lerp->curstate.gaitsequence && out->lerp->latched.sequencetime == 0.0f )
	{
		out->lerp->latched.gaitsequencetime = out->lerp->curstate.animtime + 0.01f;
		out->lerp->latched.gaitsequence = out->lerp->curstate.gaitsequence;	// save old gaitsequence
	}
	out->lerp->curstate.gaitsequence = in->v.gaitsequence; // keep an actual

	if( in->v.animtime != out->lerp->curstate.animtime )
	{
		// client got new packet, shuffle animtimes
		out->lerp->latched.animtime = out->lerp->curstate.animtime;
		VectorCopy( in->v.origin, out->lerp->latched.origin );
		VectorCopy( in->v.angles, out->lerp->latched.angles );

		for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
			out->lerp->latched.controller[i] = in->v.controller[i];
	}
	out->lerp->curstate.animtime = in->v.animtime;

	// copy controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
	{
		if( out->lerp->curstate.controller[i] != in->v.controller[i] )
			out->lerp->latched.controller[i] = out->lerp->curstate.controller[i];
		out->lerp->curstate.controller[i] = in->v.controller[i];
	}

	// copy blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
	{
		out->lerp->latched.blending[i] = out->lerp->curstate.blending[i];
		out->lerp->curstate.blending[i] = in->v.blending[i];
	}

	if( !VectorCompare( in->v.origin, out->lerp->curstate.origin ))
		VectorCopy( out->lerp->curstate.origin, out->lerp->latched.origin );
	VectorCopy( in->v.origin, out->lerp->curstate.origin );

	if( !VectorCompare( in->v.angles, out->lerp->curstate.angles ))
		VectorCopy( out->lerp->curstate.angles, out->lerp->latched.angles );
	VectorCopy( in->v.angles, out->lerp->curstate.angles );
}

void R_StudioAllocExtradata( edict_t *in, ref_entity_t *e )
{
	studiovars_t	*studio;
	bool		hasChrome = (((mstudiomodel_t *)e->model->extradata)->phdr->flags & STUDIO_HAS_CHROME) ? true : false;
	int		numbones = ((mstudiomodel_t *)e->model->extradata)->phdr->numbones;
			
	if( !e->mempool ) e->mempool = Mem_AllocPool( va( "Entity Pool %i", e - r_entities ));
	if( !e->extradata ) e->extradata = (void *)Mem_Alloc( e->mempool, sizeof( studiovars_t ));
	studio = (studiovars_t *)e->extradata;
	studio->lerp = e->lerp;

	// any stuidio model MUST have previous data for lerping
	Com_Assert( studio->lerp == NULL );

	if( m_fDoInterp )
	{
		R_StudioUpdateVarsInterpolant( in, e );
	}
	else
	{
		R_StudioUpdateVars( in, e );
	}

	if( studio->numbones != numbones )
	{
		size_t	cache_size = sizeof( matrix4x4 ) * numbones;
		size_t	names_size = numbones * 32; // bonename length

		// allocate or merge bones cache
		studio->bonestransform = (matrix4x4 *)Mem_Realloc( e->mempool, studio->bonestransform, cache_size );
	}

	if( hasChrome )
	{
		if( studio->numbones != numbones || !studio->chromeage || !studio->chromeright || !studio->chromeup )
		{
			// allocate or merge chrome cache
			studio->chromeage = (int *)Mem_Realloc( e->mempool, studio->chromeage, numbones * sizeof( int ));
			studio->chromeright = (vec3_t *)Mem_Realloc( e->mempool, studio->chromeright, numbones * sizeof( vec3_t ));
			studio->chromeup = (vec3_t *)Mem_Realloc( e->mempool, studio->chromeup, numbones * sizeof( vec3_t ));
		}
	}
	else
	{
		if( studio->chromeage ) Mem_Free( studio->chromeage );
		if( studio->chromeright ) Mem_Free( studio->chromeright );
		if( studio->chromeup ) Mem_Free( studio->chromeup );
		studio->chromeright = studio->chromeup = NULL;
		studio->chromeage = NULL;
	}
	studio->numbones = numbones;
}

void R_StudioAllocTentExtradata( TEMPENTITY *in, ref_entity_t *e )
{
	studiovars_t	*studio;
	bool		hasChrome = (((mstudiomodel_t *)e->model->extradata)->phdr->flags & STUDIO_HAS_CHROME) ? true : false;
	int		i, numbones = ((mstudiomodel_t *)e->model->extradata)->phdr->numbones;
			
	if( !e->mempool ) e->mempool = Mem_AllocPool( va( "TempEntity Pool %i", e - r_entities ));
	if( !e->extradata ) e->extradata = (void *)Mem_Alloc( e->mempool, sizeof( studiovars_t ));
	studio = (studiovars_t *)e->extradata;
	studio->lerp = e->lerp;

	// any stuidio model MUST have lerpframe data for lerping
	Com_Assert( studio->lerp == NULL );

	e->lerp->latched.sequencetime = 0.0f;					// no lerping between sequences
	e->lerp->latched.gaitsequencetime = 0.0f;				// no lerping between gaitsequences
	e->lerp->latched.sequence = e->lerp->curstate.sequence = in->m_iSequence;	// keep an actual
	e->lerp->latched.animtime = e->lerp->curstate.animtime = in->m_flFrameMax;	// HACKHACK: used m_flFrameMax as animtime

	VectorCopy( in->origin, e->lerp->curstate.origin );
	VectorCopy( e->lerp->curstate.origin, e->lerp->latched.origin );

	VectorCopy( in->angles, e->lerp->curstate.angles );
	VectorCopy( e->lerp->curstate.angles, e->lerp->latched.angles );

	// reset controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
		e->lerp->latched.controller[i] = e->lerp->curstate.controller[i] = 0x7F;

	// reset blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
		e->lerp->latched.blending[i] = e->lerp->curstate.blending[i] = 0x7F;

	if( studio->numbones != numbones )
	{
		size_t	cache_size = sizeof( matrix4x4 ) * numbones;
		size_t	names_size = numbones * 32; // bonename length

		// allocate or merge bones cache
		studio->bonestransform = (matrix4x4 *)Mem_Realloc( e->mempool, studio->bonestransform, cache_size );
	}

	if( hasChrome )
	{
		if( studio->numbones != numbones || !studio->chromeage || !studio->chromeright || !studio->chromeup )
		{
			// allocate or merge chrome cache
			studio->chromeage = (int *)Mem_Realloc( e->mempool, studio->chromeage, numbones * sizeof( int ));
			studio->chromeright = (vec3_t *)Mem_Realloc( e->mempool, studio->chromeright, numbones * sizeof( vec3_t ));
			studio->chromeup = (vec3_t *)Mem_Realloc( e->mempool, studio->chromeup, numbones * sizeof( vec3_t ));
		}
	}
	else
	{
		if( studio->chromeage ) Mem_Free( studio->chromeage );
		if( studio->chromeright ) Mem_Free( studio->chromeright );
		if( studio->chromeup ) Mem_Free( studio->chromeup );
		studio->chromeright = studio->chromeup = NULL;
		studio->chromeage = NULL;
	}
	studio->numbones = numbones;
}

/*
=======================================================

	MISC STUDIO UTILS

=======================================================
*/
/*
====================
StudioSetupRender

====================
*/
static void R_StudioSetupRender( ref_entity_t *e, ref_model_t *mod )
{
	mstudiomodel_t	*m_pRefModel = (mstudiomodel_t *)mod->extradata;

	// set global pointers
	m_pStudioHeader = m_pRefModel->phdr;
          m_pTextureHeader = m_pRefModel->thdr;

	Com_Assert( e->extradata == NULL );

	// set cached bones
	m_protationmatrix = ((studiovars_t *)e->extradata)->rotationmatrix;
	m_pbonestransform = ((studiovars_t *)e->extradata)->bonestransform;

	// set chrome bones
	m_pchromeup = ((studiovars_t *)e->extradata)->chromeup;
	m_pchromeage = ((studiovars_t *)e->extradata)->chromeage;
	m_pchromeright = ((studiovars_t *)e->extradata)->chromeright;

	// misc info
	if( e->ent_type == ED_VIEWMODEL )
	{
		// viewmodel can't properly animate without lerping
		m_fDoInterp = true;
	}
	else if( r_studio_lerping->integer )
	{
		m_fDoInterp = (e->flags & EF_NOINTERP) ? false : true;
	}
	else
	{
		m_fDoInterp = false;
	}
}

// extract texture filename from modelname
char *R_StudioTexName( ref_model_t *mod )
{
	static string	texname;

	com.strncpy( texname, mod->name, MAX_STRING );
	FS_StripExtension( texname );
	com.strncat( texname, "T.mdl", MAX_STRING );
	return texname;
}

int R_StudioExtractBbox( dstudiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	dstudioseqdesc_t	*pseqdesc;
	pseqdesc = (dstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if( sequence < 0 || sequence >= phdr->numseq )
		return 0;
	
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

void R_StudioModelLerpBBox( ref_entity_t *e, ref_model_t *mod )
{
	// FIXME: implement
}

void R_StudioModelBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs )
{
	dstudiohdr_t	*hdr;

	if( !e->model ) return;
	R_StudioSetupRender( e, e->model );

	hdr = ((mstudiomodel_t *)e->model->extradata)->phdr;
	if( !hdr ) return;
	R_StudioExtractBbox( hdr, e->lerp->curstate.sequence, mins, maxs );
}

/*
====================
Studio model loader
====================
*/
void R_StudioSurfaceParm( dstudiotexture_t *tex )
{
	if( tex->flags & STUDIO_NF_TRANSPARENT )
		R_ShaderSetRenderMode( kRenderTransAlpha, false );
	else if( tex->flags & STUDIO_NF_ADDITIVE )
		R_ShaderSetRenderMode( kRenderTransAdd, false );
	else if( tex->flags & STUDIO_NF_BLENDED )
		R_ShaderSetRenderMode( kRenderTransTexture, false );
	else R_ShaderSetRenderMode( kRenderNormal, false );
}

texture_t *R_StudioLoadTexture( ref_model_t *mod, dstudiohdr_t *phdr, dstudiotexture_t *ptexture )
{
	size_t	size;
	int	flags = 0;
	string	name;

	if( ptexture->flags & STUDIO_NF_TRANSPARENT )
		flags |= (TF_CLAMP|TF_NOMIPMAP);

	if( ptexture->flags & (STUDIO_NF_NORMALMAP|STUDIO_NF_HEIGHTMAP ))
		flags |= TF_NORMALMAP;

	// NOTE: replace index with pointer to start of imagebuffer, ImageLib expected it
	ptexture->index = (int)((byte *)phdr) + ptexture->index;
	size = sizeof( dstudiotexture_t ) + ptexture->width * ptexture->height + 768;

	// build the texname
	com.snprintf( name, sizeof( name ), "\"%s/%s\"", mod->name, ptexture->name );

	if( ptexture->flags & STUDIO_NF_HEIGHTMAP )
		return R_FindTexture( va( "mergeDepthmap( heightMap( Studio( %s ), 2.0 ), Studio( %s ));", name, name ), (byte *)ptexture, size, flags );
	return R_FindTexture( va( "Studio( %s );", name ), (byte *)ptexture, size, flags );
}

static int R_StudioLoadTextures( ref_model_t *mod, dstudiohdr_t *phdr )
{
	texture_t		*texs[4];
	dstudiotexture_t	*ptexture = (dstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);
	string		modname, texname, shadername;
	string		normalmap, heightmap;
	int		nm_index, hm_index;
	int		i, j, numshaders = 0;

	FS_FileBase( mod->name, modname );
	
	for( i = 0; i < phdr->numtextures; i++ )
	{
		if( ptexture[i].flags & (STUDIO_NF_NORMALMAP|STUDIO_NF_HEIGHTMAP|STUDIO_NF_GLOSSMAP|STUDIO_NF_DECALMAP))
			continue; // doesn't produce dead shaders

		// misc
		R_StudioSurfaceParm( &ptexture[i] );
		FS_FileBase( ptexture[i].name, texname );
		if( ptexture[i].flags & STUDIO_NF_CHROME ) phdr->flags |= STUDIO_HAS_CHROME;
		com.snprintf( shadername, MAX_STRING, "%s.mdl/%s", modname, texname );

		if( R_ShaderCheckCache( shadername ))
 			goto load_shader;	// external shader found
 
		Mem_Set( texs, 0, sizeof( texs ));
		normalmap[0] = heightmap[0] = '\0';
		nm_index = hm_index = 0;

		texs[0] = R_StudioLoadTexture( mod, phdr, ptexture + i );
		if( !texs[0] ) texs[0] = tr.defaultTexture;
		R_ShaderAddStageTexture( texs[0] ); // load diffuse texture
				
		// check for normalmap
		for( j = 0; j < phdr->numtextures; j++ )
		{
			if( !com.strnicmp( texname, ptexture[j].name, com.strlen( texname )))
			{
				if( ptexture[j].flags & STUDIO_NF_HEIGHTMAP )
				{
					// determine heightmap name
					com.snprintf( heightmap, sizeof( heightmap ), "%s/%s", mod->name, ptexture[j].name );
        					hm_index = j;
        				}
				else if( ptexture[j].flags & STUDIO_NF_NORMALMAP )
				{
					// determine normalmap name
					com.snprintf( normalmap, sizeof( normalmap ), "%s/%s", mod->name, ptexture[j].name );
					nm_index = j;
				}
			}
		}

		// load it in
		if( heightmap[0] && normalmap[0] )
		{
			// merge both textures into single and turn parallax on
			texs[1] = R_FindTexture( va( "mergeDepthmap( Studio( \"%s\" ), Studio( \"%s\" ));", normalmap, heightmap ), NULL, 0, TF_NORMALMAP );
		}
		else if( normalmap[0] ) texs[1] = R_StudioLoadTexture( mod, phdr, ptexture + nm_index );
		else if( heightmap[0] ) texs[1] = R_StudioLoadTexture( mod, phdr, ptexture + hm_index );

		if( texs[1] == NULL ) goto load_shader;	// failed to load material
		R_ShaderAddStageTexture( texs[1] );

		// check for glossmap
		for( j = 0; j < phdr->numtextures; j++ )
		{
			if( ptexture[j].flags & (STUDIO_NF_GLOSSMAP) && !com.strnicmp( texname, ptexture[j].name, com.strlen( texname )))
			{
				texs[2] = R_StudioLoadTexture( mod, phdr, ptexture + j );
				break;
			}
		}
		R_ShaderAddStageTexture( texs[2] );

		// check for decalmap
		for( j = 0; j < phdr->numtextures; j++ )
		{
			if( ptexture[j].flags & (STUDIO_NF_DECALMAP) && !com.strnicmp( texname, ptexture[j].name, com.strlen( texname )))
			{
				texs[3] = R_StudioLoadTexture( mod, phdr, ptexture + j );
				break;
			}
		}
		R_ShaderAddStageTexture( texs[3] );
load_shader:
		mod->shaders[numshaders] = R_LoadShader( shadername, SHADER_STUDIO, 0, 0, SHADER_INVALID );
		ptexture[numshaders].shader = mod->shaders[numshaders]->shadernum;
		numshaders++;
	}
	return numshaders;
}

dstudiohdr_t *R_StudioLoadHeader( ref_model_t *mod, const uint *buffer )
{
	byte		*pin;
	dstudiohdr_t	*phdr;
	dstudiotexture_t	*ptexture;
	string		modname;
	
	pin = (byte *)buffer;
	phdr = (dstudiohdr_t *)pin;

	if( phdr->version != STUDIO_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", phdr->name, phdr->version, STUDIO_VERSION );
		return NULL;
	}	
	FS_FileBase( mod->name, modname );

	ptexture = (dstudiotexture_t *)(((byte *)phdr) + phdr->textureindex);
	if( phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS )
	{
		mod->shaders = Mod_Malloc( mod, sizeof( shader_t* ) * phdr->numtextures );
		mod->numshaders = R_StudioLoadTextures( mod, phdr );

		if( mod->numshaders != phdr->numtextures ) // bump, gloss will be merged into single shader
			mod->shaders = Mod_Realloc( mod, mod->shaders, sizeof( shader_t* ) * mod->numshaders );
	}
	return (dstudiohdr_t *)buffer;
}

void Mod_StudioLoadModel( ref_model_t *mod, const void *buffer )
{
	dstudiohdr_t	*phdr = R_StudioLoadHeader( mod, buffer );
	dstudiohdr_t	*thdr = NULL;
	mstudiomodel_t	*poutmodel;
	void		*texbuf;
	
	if( !phdr ) return; // there were problems
	mod->extradata = poutmodel = (mstudiomodel_t *)Mod_Malloc( mod, sizeof( mstudiomodel_t ));
	poutmodel->phdr = (dstudiohdr_t *)Mod_Malloc( mod, LittleLong( phdr->length ));
	Mem_Copy( poutmodel->phdr, buffer, LittleLong( phdr->length ));
	
	if( phdr->numtextures == 0 )
	{
		texbuf = FS_LoadFile( R_StudioTexName( mod ), NULL );
		if( texbuf ) thdr = R_StudioLoadHeader( mod, texbuf );
		else MsgDev( D_ERROR, "StudioLoadModel: %s missing textures file\n", mod->name ); 

		if( !thdr ) return; // there were problems
		poutmodel->thdr = (dstudiohdr_t *)Mod_Malloc( mod, LittleLong( thdr->length ));
          	Mem_Copy( poutmodel->thdr, texbuf, LittleLong( thdr->length ));
		if( texbuf ) Mem_Free( texbuf );
	}
	else poutmodel->thdr = poutmodel->phdr; // just make link
	poutmodel->phdr->flags |= poutmodel->thdr->flags;	// copy STUDIO_HAS_CHROME flag

	R_StudioExtractBbox( phdr, 0, mod->mins, mod->maxs );
	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
	mod->touchFrame = tr.registration_sequence;
	mod->type = mod_studio;
}

/*
====================
R_StudioProcessEvents

====================
*/
void R_StudioProcessEvents( ref_entity_t *e, edict_t *ed )
{
	dstudioseqdesc_t	*pseqdesc;
	dstudioevent_t	*pevent;
	float		flEventFrame;
	bool		bLooped = false;
	int		i;

	switch( e->ent_type )
	{
	case ED_VIEWMODEL:
	case ED_TEMPENTITY:
		break;
	default:	return;
	}

	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;
	pevent = (dstudioevent_t *)((byte *)m_pStudioHeader + pseqdesc->eventindex);

	// curstate.frame not used for viewmodel animating
	flEventFrame = e->lerp->latched.frame;

	if( pseqdesc->numevents == 0 )
		return;

	//Msg( "%i frame %f\n", r_framecount, e->lerp->latched.frame );

	if( e->lerp->m_iEventSequence != e->lerp->curstate.sequence )
	{
		flEventFrame = 0.0f;
		e->lerp->m_flPrevEventFrame = -0.01f; // back up to get 0'th frame animations
		e->lerp->m_iEventSequence = e->lerp->curstate.sequence;
	}

	// stalled?
	if( flEventFrame == e->lerp->m_flPrevEventFrame )
		return;

	//Msg( "(seq %d cycle %.3f ) evframe %.3f prevevframe %.3f (time %.3f)\n", e->lerp->curstate.sequence, e->lerp->latched.frame, flEventFrame, e->lerp->m_flPrevEventFrame, RI.refdef.time );

	// check for looping
	if( flEventFrame <= e->lerp->m_flPrevEventFrame )
	{
		if( e->lerp->m_flPrevEventFrame - flEventFrame > 0.5f )
		{
			bLooped = true;
		}
		else
		{
			// things have backed up, which is bad since it'll probably result in a hitch in the animation playback
			// but, don't play events again for the same time slice
			return;
		}
	}

	for( i = 0; i < pseqdesc->numevents; i++ )
	{
		// ignore all non-client-side events
		if( pevent[i].event < EVENT_CLIENT )
			continue;

		// looped
		if( bLooped )
		{
			if(( pevent[i].frame > e->lerp->m_flPrevEventFrame || pevent[i].frame <= flEventFrame ))
			{
				//Msg( "FE %i Looped frame %i, prev %f ev %f (time %.3f)\n", pevent[i].event, pevent[i].frame, e->lerp->m_flPrevEventFrame, flEventFrame, RI.refdef.time );
				ri.StudioEvent( &pevent[i], ed );
			}
		}
		else
		{
			if(( pevent[i].frame > e->lerp->m_flPrevEventFrame && pevent[i].frame <= flEventFrame ))
			{
				//Msg( "FE %i Normal frame %i, prev %f ev %f (time %.3f)\n", pevent[i].event, pevent[i].frame, e->lerp->m_flPrevEventFrame, flEventFrame, RI.refdef.time );
				ri.StudioEvent( &pevent[i], ed );
			}
		}
	}

	e->lerp->m_flPrevEventFrame = flEventFrame;
}

/*
====================
StudioCalcBoneAdj

====================
*/
void R_StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen )
{
	int			i, j;
	float			value;
	dstudiobonecontroller_t	*pbonecontroller;
	
	pbonecontroller = (dstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;

		if( i == STUDIO_MOUTH )
		{
			// mouth hardcoded at controller 4
			value = mouthopen / 64.0f;
			if( value > 1.0f ) value = 1.0f;				
			value = (1.0f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			//Msg( "%d %f\n", mouthopen, value );
		}
		else if( i <= MAXSTUDIOCONTROLLERS )
		{
			// check for 360% wrapping
			if( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					float	a, b;
					a = fmod(( pcontroller1[j] + 128 ), 256 );
					b = fmod(( pcontroller2[j] + 128 ), 256 );
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0/256.0) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0/256.0) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				value = bound( 0.0f, value, 1.0f );
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			//Msg( "%d %d %f : %f\n", RI.currententity->lerp->curstate.controller[j], RI.currententity->lerp->latched.controller[j], value, dadt );
		}

		switch( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

/*
====================
StudioCalcBoneQuaterion

====================
*/
void R_StudioCalcBoneQuaterion( int frame, float s, dstudiobone_t *pbone, dstudioanim_t *panim, float *adj, float *q )
{
	int		j, k;
	vec4_t		q1, q2;
	vec3_t		angle1, angle2;
	dstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		if( panim->offset[j+3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (dstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			k = frame;
			
			// debug
			if( panimvalue->num.total < panimvalue->num.valid ) k = 0;
			
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				// debug
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// bah, missing blend!
			if( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k+1].value;

				if( panimvalue->num.valid > k + 1 )
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if( panimvalue->num.total > k + 1 )
						angle2[j] = angle1[j];
					else angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;

				if( panimvalue->num.total > k + 1 )
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}

			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if( pbone->bonecontroller[j+3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if( !VectorCompare( angle1, angle2 ))
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void R_StudioCalcBonePosition( int frame, float s, dstudiobone_t *pbone, dstudioanim_t *panim, float *adj, float *pos )
{
	int		j, k;
	dstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim->offset[j] != 0 )
		{
			panimvalue = (dstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			
			//if( j == 0 ) Msg( "%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			k = frame;

			// debug
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

  				// debug
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// if we're inside the span
			if( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if( panimvalue->num.valid > k + 1 )
				{
					pos[j] += (panimvalue[k+1].value * (1.0f - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k+1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if( panimvalue->num.total <= k + 1 )
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void R_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int	i;
	vec4_t	q3;
	float	s1;

	s = bound( 0.0f, s, 1.0f );

	s1 = 1.0f - s;

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
Cache_Check

consistency check
====================
*/
void *Cache_Check( byte *mempool, cache_user_t *c )
{
	if( !c->data )
		return NULL;

	if( !Mem_IsAllocated( mempool, c->data ))
		return NULL;

	return c->data;
}

/*
====================
StudioGetAnim

====================
*/
dstudioanim_t *R_StudioGetAnim( ref_model_t *m_pRefModel, dstudioseqdesc_t *pseqdesc )
{
	dstudioseqgroup_t	*pseqgroup;
	cache_user_t	*paSequences;
	mstudiomodel_t	*m_pSubModel = (mstudiomodel_t *)m_pRefModel->extradata;
          size_t		filesize;
          byte		*buf;

	Com_Assert( m_pSubModel == NULL );	

	pseqgroup = (dstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (dstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		paSequences = (cache_user_t *)Mem_Alloc( m_pRefModel->mempool, MAXSTUDIOGROUPS * sizeof( cache_user_t ));
		m_pSubModel->submodels = (void *)paSequences;
	}

	if( !Cache_Check( m_pRefModel->mempool, ( cache_user_t *)&( paSequences[pseqdesc->seqgroup] )))
	{
		string	filepath, modelname, modelpath;

		FS_FileBase( m_pRefModel->name, modelname );
		FS_ExtractFilePath( m_pRefModel->name, modelpath );
		com.snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

		buf = FS_LoadFile( filepath, &filesize );
		if( !buf || !filesize ) Host_Error( "R_StudioGetAnim: can't load %s\n", filepath );
		if( IDSEQGRPHEADER != LittleLong(*(uint *)buf ))
			Host_Error( "R_StudioGetAnim: %s is corrupted\n", filepath );

		MsgDev( D_LOAD, "R_StudioGetAnim: %s\n", filepath );
			
		paSequences[pseqdesc->seqgroup].data = Mem_Alloc( m_pRefModel->mempool, filesize );
		Mem_Copy( paSequences[pseqdesc->seqgroup].data, buf, filesize );
		Mem_Free( buf );
	}
	return (dstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void R_StudioPlayerBlend( dstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);

	if( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0f;
		*pBlend = 0;
	}
	else if( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0f;
		*pBlend = 255;
	}
	else
	{
		if( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1f ) // catch qc error
			*pBlend = 127;
		else *pBlend = 255.0f * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void R_StudioSetUpTransform( ref_entity_t *e, bool trivial_accept )
{
	vec3_t	angles, origin;
	int	i;

	if( trivial_accept )
	{ 
		Matrix4x4_Copy( RI.objectMatrix, ((studiovars_t *)e->extradata)->rotationmatrix );
		Matrix4x4_ConcatTransforms( RI.modelviewMatrix, RI.worldviewMatrix, RI.objectMatrix );
		GL_LoadMatrix( RI.modelviewMatrix );
		tr.modelviewIdentity = false;
		return;
	}

	if( RP_FOLLOWENTITY( e ))
	{
		Matrix4x4_Copy( m_protationmatrix, ((studiovars_t *)e->parent->extradata)->rotationmatrix );
		m_protationmatrix = matrix4x4_identity;
		return;
	}

	VectorCopy( e->origin, origin );
	VectorCopy( e->angles, angles );

	if( e->movetype == MOVETYPE_STEP ) 
	{
		float	d, f = 0.0f;
		edict_t	*m_pEntity, *m_pGroundEntity;

		// don't do it if the goalstarttime hasn't updated in a while.
		// NOTE: Because we need to interpolate multiplayer characters, the interpolation time limit
		// was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if( m_fDoInterp && ( RI.refdef.time < e->lerp->curstate.animtime + 1.0f ) && ( e->lerp->curstate.animtime != e->lerp->latched.animtime ))
		{
			f = ( RI.refdef.time - e->lerp->curstate.animtime ) / ( e->lerp->curstate.animtime - e->lerp->latched.animtime );
			//Msg( "%4.2f %.2f %.2f\n", f, e->lerp->curstate.animtime, RI.refdef.time );
		}

		if( m_fDoInterp )
		{
			// ugly hack to interpolate angle, position.
			// current is reached 0.1 seconds after being set
			f = f - 1.0f;
		}

		m_pEntity = ri.GetClientEdict( e->index );
		if( m_pEntity ) m_pGroundEntity = m_pEntity->v.groundentity;

		if( m_pGroundEntity && m_pGroundEntity->v.movetype == MOVETYPE_PUSH && !VectorIsNull( m_pGroundEntity->v.velocity ))
		{
			dstudioseqdesc_t		*pseqdesc;
		
			pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;
			d = RI.lerpFrac;

			origin[0] += ( e->origin[0] - m_pEntity->v.oldorigin[0] ) * d;
			origin[1] += ( e->origin[1] - m_pEntity->v.oldorigin[1] ) * d;
			origin[2] += ( e->origin[2] - m_pEntity->v.oldorigin[2] ) * d;

			d = f - d;

			// monster walking on moving platform
			if( pseqdesc->motiontype & STUDIO_LX )
			{
				origin[0] += ( e->lerp->curstate.origin[0] - e->lerp->latched.origin[0] ) * d;
				origin[1] += ( e->lerp->curstate.origin[1] - e->lerp->latched.origin[1] ) * d;
				origin[2] += ( e->lerp->curstate.origin[2] - e->lerp->latched.origin[2] ) * d;
			}
		}
		else
		{
			origin[0] += ( e->lerp->curstate.origin[0] - e->lerp->latched.origin[0] ) * f;
			origin[1] += ( e->lerp->curstate.origin[1] - e->lerp->latched.origin[1] ) * f;
			origin[2] += ( e->lerp->curstate.origin[2] - e->lerp->latched.origin[2] ) * f;
		}

		for( i = 0; i < 3; i++ )
		{
			float	ang1, ang2;

			ang1 = e->lerp->curstate.angles[i];
			ang2 = e->lerp->latched.angles[i];

			d = ang1 - ang2;

			if( d > 180 ) d -= 360;
			else if( d < -180 ) d += 360;

			angles[i] += d * f;
		}
	}

	if( e->ent_type == ED_CLIENT || e->ent_type == ED_MONSTER )
		angles[PITCH] = 0; // don't rotate clients and monsters, only aim

	if( e->ent_type == ED_VIEWMODEL ) angles[PITCH] = -angles[PITCH]; // stupid Half-Life bug

	Matrix4x4_CreateFromEntity( m_protationmatrix, origin[0], origin[1], origin[2], -angles[PITCH], angles[YAW], angles[ROLL], e->scale );

	if( e->ent_type == ED_VIEWMODEL && r_lefthand->integer == 1 )
	{
		m_protationmatrix[0][1] = -m_protationmatrix[0][1];
		m_protationmatrix[1][1] = -m_protationmatrix[1][1];
		m_protationmatrix[2][1] = -m_protationmatrix[2][1];
	}

	// save matrix for gl_transaform, use identity matrix for bones
	m_protationmatrix = matrix4x4_identity;
}


/*
====================
StudioEstimateInterpolant

====================
*/
float R_StudioEstimateInterpolant( void )
{
	float	dadt = 1.0f;

	if( m_fDoInterp && ( RI.currententity->lerp->curstate.animtime >= RI.currententity->lerp->latched.animtime + 0.01f ))
	{
		dadt = ( RI.refdef.time - RI.currententity->lerp->curstate.animtime ) / 0.1f;
		if( dadt > 2.0f ) dadt = 2.0f;
	}
	return dadt;
}


/*
====================
StudioCalcRotations

====================
*/
void R_StudioCalcRotations( float pos[][3], vec4_t *q, dstudioseqdesc_t *pseqdesc, dstudioanim_t *panim, float f )
{
	int		i;
	int		frame;
	dstudiobone_t	*pbone;
	studiovars_t	*pstudio;
	float		s, mouthopen;
	float		adj[MAXSTUDIOCONTROLLERS];
	float		dadt;

	if( f > pseqdesc->numframes - 1 )
	{
		f = 0.0f; // bah, fix this bug with changing sequences too fast
	}
	else if( f < -0.01f )
	{
		// BUGBUG ( somewhere else ) but this code should validate this data.
		// This could cause a crash if the frame # is negative, so we'll go ahead
		// and clamp it here
		MsgDev( D_ERROR, "R_StudioCalcRotations: f = %g\n", f );
		f = -0.01f;
	}

	frame = (int)f;
	pstudio = RI.currententity->extradata;
	Com_Assert( pstudio == NULL );

	//Msg( "%d %.4f %.4f %.4f %.4f %d\n", RI.currententity->lerp->curstate.sequence, RI.refdef.time, RI.currententity->lerp->curstate.animtime, RI.currententity->lerp->curstate.frame, f, frame );
	//Msg( "%f %f %f\n", RI.currententity->angles[ROLL], RI.currententity->angles[PITCH], RI.currententity->angles[YAW] );
	//Msg( "frame %d %d\n", frame1, frame2 );

	dadt = R_StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	mouthopen = ri.GetMouthOpen( RI.currententity->index );

	R_StudioCalcBoneAdj( dadt, adj, pstudio->lerp->curstate.controller, pstudio->lerp->latched.controller, mouthopen );

	for( i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++ ) 
	{
		R_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		R_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
		// if( 0 && i == 0 ) Msg( "%d %d %d %d\n", RI.currententity->lerp->curstate.sequence, frame, j, k );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;

	// FIXME: enable this ? Half-Life 2 get rid of this code
	s = 0 * ((1.0f - (f - (int)(f))) / (pseqdesc->numframes)) * RI.currententity->framerate;

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioEstimateFrame

====================
*/
float R_StudioEstimateFrame( dstudioseqdesc_t *pseqdesc )
{
	double	dfdt, f;
	
	if( m_fDoInterp )
	{
		if( RI.refdef.time < RI.currententity->lerp->curstate.animtime ) dfdt = 0;
		else dfdt = (RI.refdef.time - RI.currententity->lerp->curstate.animtime) * RI.currententity->framerate * pseqdesc->fps;
	}
	else dfdt = 0;

	if( pseqdesc->numframes <= 1 ) f = 0.0;
	else f = (RI.currententity->lerp->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
 
	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 ) f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		if( f < 0 ) f += (pseqdesc->numframes - 1);
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 ) f = pseqdesc->numframes - 1.001;
		if( f < 0.0 )  f = 0.0;
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
void R_StudioSetupBones( ref_entity_t *e )
{
	int		i;
	double		f;

	dstudiobone_t	*pbones;
	dstudioseqdesc_t	*pseqdesc;
	dstudioanim_t	*panim;
	studiovars_t	*pstudio;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;
	edict_t		*cl_entity;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	Com_Assert( e->model == NULL || e->model->extradata == NULL );

	// bones already cached for this frame
	if( e->m_nCachedFrameCount == r_framecount2 )
		return;

	RI.currententity = e;
	cl_entity = ri.GetClientEdict( e->index );
	if( e->lerp->curstate.sequence >= m_pStudioHeader->numseq ) e->lerp->curstate.sequence = 0;
	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;

	f = R_StudioEstimateFrame( pseqdesc );

//	if( e->lerp->curstate.frame > f ) Msg( "%f %f\n", e->lerp->curstate.frame, f );

	pstudio = e->extradata;
	Com_Assert( pstudio == NULL );

	panim = R_StudioGetAnim( e->model, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt;

		panim += m_pStudioHeader->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = R_StudioEstimateInterpolant();
		s = (pstudio->lerp->curstate.blending[0] * dadt + pstudio->lerp->latched.blending[0] * (1.0 - dadt)) / 255.0;

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (pstudio->lerp->curstate.blending[0] * dadt + pstudio->lerp->latched.blending[0] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (pstudio->lerp->curstate.blending[1] * dadt + pstudio->lerp->latched.blending[1] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if( m_fDoInterp && pstudio->lerp->latched.sequencetime && ( pstudio->lerp->latched.sequencetime + 0.2 > RI.refdef.time) && ( pstudio->lerp->latched.sequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float  pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pstudio->lerp->latched.sequence;
		panim = R_StudioGetAnim( e->model, pseqdesc );

		// clip prevframe
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, pstudio->lerp->latched.frame );

		if( pseqdesc->numblends > 1 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, pstudio->lerp->latched.frame );

			s = (pstudio->lerp->latched.seqblending[0]) / 255.0;
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if( pseqdesc->numblends == 4 )
			{
				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, pstudio->lerp->latched.frame );

				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, pstudio->lerp->latched.frame );

				s = (pstudio->lerp->latched.seqblending[0]) / 255.0;
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (pstudio->lerp->latched.seqblending[1]) / 255.0;
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0f - ( RI.refdef.time - pstudio->lerp->latched.sequencetime ) / 0.2f;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		// store prevframe otherwise
		//Msg( "prevframe = %4.2f\n", f );
		pstudio->lerp->latched.frame = f;
	}

	pbones = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if( e->gaitsequence != 0 )
	{
		if( e->gaitsequence >= m_pStudioHeader->numseq ) 
			e->gaitsequence = 0;

		pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->gaitsequence;

		panim = R_StudioGetAnim( e->model, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, pstudio->lerp->curstate.gaitframe );

		for( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			// g-cont. hey, what a hell ?
			if( !com.strcmp( pbones[i].name, "Bip01 Spine" ))
				break;
			Mem_Copy( pos[i], pos2[i], sizeof( pos[i] ));
			Mem_Copy( q[i], q2[i], sizeof( q[i] ));
		}
	}

	if( m_fDoInterp && pstudio->lerp->latched.gaitsequencetime && ( pstudio->lerp->latched.gaitsequencetime + 0.2 > RI.refdef.time) && ( pstudio->lerp->latched.gaitsequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float  pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;

		pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pstudio->lerp->latched.gaitsequence;
		panim = R_StudioGetAnim( e->model, pseqdesc );

		// clip prevframe
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, pstudio->lerp->latched.gaitframe );

		s = 1.0f - ( RI.refdef.time - pstudio->lerp->latched.gaitsequencetime ) / 0.2f;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		// store prevframe otherwise
		//Msg( "prevgaitframe = %4.2f\n", pstudio->lerp->curstate.gaitframe );
		pstudio->lerp->latched.gaitframe = pstudio->lerp->curstate.gaitframe;
	}
          
	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) 
		{
			Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_protationmatrix, bonematrix );

			// apply client-side effects to the transformation matrix
			if( cl_entity ) ri.StudioFxTransform( cl_entity, m_pbonestransform[i] );
		} 
		else Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_pbonestransform[pbones[i].parent], bonematrix );
	}
}

/*
====================
StudioSaveBones

====================
*/
static void R_StudioSaveBones( ref_entity_t *e )
{
	dstudiobone_t	*pbones = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	int		i;

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
		com.strncpy( ((studiovars_t *)e->extradata)->bonenames[i], pbones[i].name, 32 );
}

/*
====================
StudioMergeBones

====================
*/
void R_StudioMergeBones( ref_entity_t *e, ref_model_t *m_pSubModel )
{
	int		i, j;
	double		f;
	int		sequence = e->lerp->curstate.sequence;
	dstudiobone_t	*pbones;
	dstudioseqdesc_t	*pseqdesc;
	dstudioanim_t	*panim;
	studiovars_t	*pstudio;
	matrix4x4		bonematrix;
	static vec4_t	q[MAXSTUDIOBONES];
	static float	pos[MAXSTUDIOBONES][3];
	static matrix4x4	localbones[MAXSTUDIOBONES];
	edict_t		*cl_entity;

	Com_Assert( e->parent == NULL );
	cl_entity = ri.GetClientEdict( e->parent->index );
	pstudio = ((studiovars_t *)e->extradata);

	Com_Assert( pstudio == NULL );
	Mem_Copy( localbones, pstudio->bonestransform, sizeof( matrix4x4 ) * pstudio->numbones );
		
	pstudio = ((studiovars_t *)e->parent->extradata);

	// weaponmodel can't change e->sequence!
	if( sequence >= m_pStudioHeader->numseq ) sequence = 0;
	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	f = R_StudioEstimateFrame( pseqdesc );

//	if( e->lerp->curstate.frame > f ) Msg( "%f %f\n", e->lerp->latched.frame, f );

	panim = R_StudioGetAnim( m_pSubModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );
	pbones = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		for( j = 0; j < pstudio->numbones; j++ )
		{
			if( !com.stricmp( pbones[i].name, pstudio->bonenames[j] ))
			{
				Matrix4x4_Copy( localbones[i], pstudio->bonestransform[j] );
				break;
			}
		}
		if( j >= pstudio->numbones )
		{
			Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
			if( pbones[i].parent == -1 ) 
			{
				Matrix4x4_ConcatTransforms( localbones[i], m_protationmatrix, bonematrix );

				// apply client-side effects to the transformation matrix
				if( cl_entity ) ri.StudioFxTransform( cl_entity, m_pbonestransform[i] );
			} 
			else Matrix4x4_ConcatTransforms( localbones[i], m_pbonestransform[pbones[i].parent], bonematrix );
		}
	}

	pstudio = ((studiovars_t *)e->extradata);
	Com_Assert( pstudio == NULL );

	// copy bones back to the merged entity
	Mem_Copy( pstudio->bonestransform, localbones, sizeof( matrix4x4 ) * m_pStudioHeader->numbones );
}


/*
====================
StudioCalcAttachments

====================
*/
static void R_StudioCalcAttachments( ref_entity_t *e )
{
	int		i;
	matrix4x4		out;
	dstudioattachment_t	*pAtt;
	vec3_t		axis[3];
	vec3_t		localOrg, localAng, bonepos;

	if( m_pStudioHeader->numattachments <= 0 )
	{
		// clear attachments
		for( i = 0; i < MAXSTUDIOATTACHMENTS; i++ )
			ri.SetAttachment( e->index, i, vec3_origin, vec3_origin );
		return;
	}
	else if( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		MsgDev( D_WARN, "R_StudioCalcAttahments: too many attachments on %s\n", e->model->name );
	}

	// calculate attachment points
	pAtt = (dstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for( i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		// NOTE: m_pbonestransform not contained rotate component so we need do it here
		Matrix4x4_Concat( out, ((studiovars_t *)e->extradata)->rotationmatrix, m_pbonestransform[pAtt[i].bone] );

		// compute pos and angles
		Matrix4x4_VectorTransform( out, pAtt[i].org, localOrg );
		Matrix4x4_OriginFromMatrix( out, bonepos );
		VectorSubtract( localOrg, bonepos, axis[0] );	// make forward
		VectorNormalizeFast( axis[0] );
		VectorVectors( axis[0], axis[1], axis[2] );	// make right and up

		Matrix3x3_ToAngles( axis, localAng, false );	// FIXME: dll's uses FLU ?
		ri.SetAttachment( e->index, i, localOrg, localAng );
	}
}

bool R_StudioComputeBBox( vec3_t bbox[8] )
{
	vec3_t		vectors[3], angles, tmp;
	ref_entity_t	*e = RI.currententity;
	int		i, seq = RI.currententity->lerp->curstate.sequence;

	if( !R_StudioExtractBbox( m_pStudioHeader, seq, studio_mins, studio_maxs ))
		return false;
	
	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );
		
	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
		if( i & 1 ) tmp[0] = studio_mins[0];
		else tmp[0] = studio_maxs[0];
		if( i & 2 ) tmp[1] = studio_mins[1];
		else tmp[1] = studio_maxs[1];
		if( i & 4 ) tmp[2] = studio_mins[2];
		else tmp[2] = studio_maxs[2];
		VectorCopy( tmp, bbox[i] );
	}

	// rotate the bounding box
	VectorScale( e->angles, -1, angles );

	if( e->ent_type == ED_CLIENT || e->ent_type == ED_MONSTER )
		angles[PITCH] = 0; // don't rotate player model, only aim
	AngleVectorsFLU( angles, vectors[0], vectors[1], vectors[2] );

	for( i = 0; i < 8; i++ )
	{
		VectorCopy( bbox[i], tmp );
		bbox[i][0] = DotProduct( vectors[0], tmp );
		bbox[i][1] = DotProduct( vectors[1], tmp );
		bbox[i][2] = DotProduct( vectors[2], tmp );
	}
	return true;
}

/*
====================
StudioSetupSubModel

====================
*/
static void R_StudioSetupSubModel( int body, int bodypart )
{
	int	index;

	if( bodypart > m_pStudioHeader->numbodyparts ) bodypart = 0;
	m_pBodyPart = (dstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (dstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void R_StudioSetupChrome( float *pchrome, int modelnum, int bone, float *normal )
{
	float	n;

	if( m_pchromeage[bone] != ( r_framecount2 + modelnum ))
	{
		vec3_t	chromeupvec;		// g_chrome t vector in world reference frame
		vec3_t	chromerightvec;		// g_chrome s vector in world reference frame
		vec3_t	tmp;			// vector pointing at bone in world reference frame
		vec3_t	v_right = { 0, -1, 0 };	// inverse right vector
		
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		Matrix4x4_OriginFromMatrix( m_pbonestransform[bone], tmp );
		VectorNormalize( tmp );
		CrossProduct( tmp, v_right, chromeupvec );

		VectorNormalize( chromeupvec );
		CrossProduct( tmp, chromeupvec, chromerightvec );
		VectorNormalize( chromerightvec );

		Matrix4x4_VectorIRotate( m_pbonestransform[bone], chromeupvec, m_pchromeup[bone] );
		Matrix4x4_VectorIRotate( m_pbonestransform[bone], chromerightvec, m_pchromeright[bone] );
		m_pchromeage[bone] = (r_framecount2+modelnum);
	}

	// calc s coord
	n = DotProduct( normal, m_pchromeright[bone] );
	pchrome[0] = (n + 1.0f) * 32.0f;

	// calc t coord
	n = DotProduct( normal, m_pchromeup[bone] );
	pchrome[1] = (n + 1.0f) * 32.0f;
}

void R_StudioDrawMesh( const meshbuffer_t *mb, short *ptricmds, float s, float t, int features, int flags )
{
	int		i;
	float		*av, *lv;
	ref_shader_t	*shader;

	MB_NUM2SHADER( mb->shaderkey, shader );

	while( i = *( ptricmds++ ))
	{
		int	vertexState = 0;
		bool	tri_strip;
		
		if( i < 0 )
		{
			i = -i;
			tri_strip = false;
		}
		else tri_strip = true;
			
		for( ; i > 0; i--, ptricmds += 4 )
		{
			if( vertexState++ < 3 )
			{
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
			}
			else if( tri_strip )
			{
				// flip triangles between clockwise and counter clockwise
				if( vertexState & 1 )
				{
					// draw triangle [n-2 n-1 n]
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 2;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
				}
				else
				{
					// draw triangle [n-1 n-2 n]
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 2;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
				}
			}
			else
			{
				// draw triangle fan [0 n-1 n]
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - ( vertexState - 1 );
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
			}

			if( flags & STUDIO_NF_CHROME )
				Vector2Set( inCoordsArray[r_backacc.numVerts], m_pchrome[ptricmds[1]][0] * s, m_pchrome[ptricmds[1]][1] * t );
			else Vector2Set( inCoordsArray[r_backacc.numVerts], ptricmds[2] * s, ptricmds[3] * t );

			if( features & MF_NORMALS )
			{
				lv = m_pxformnorms[ptricmds[1]]; // normals
				Vector4Set( inNormalsArray[r_backacc.numVerts], lv[0], lv[1], lv[2], 1.0f );
			}

			av = m_pxformverts[ptricmds[0]]; // verts
			Vector4Set( inVertsArray[r_backacc.numVerts], av[0], av[1], av[2], 1.0f );
			r_backacc.c_totalVerts++;	// r_speeds issues
			r_backacc.numVerts++;
		}
	}

	if( features & MF_SVECTORS )
		R_BuildTangentVectors( r_backacc.numVerts, inVertsArray, inNormalsArray, inCoordsArray, r_backacc.numElems / 3, inElemsArray, inSVectorsArray );
	
	R_StudioSetUpTransform( RI.currententity, true );
	r_features = features;
	R_RenderMeshBuffer( mb );
}

void R_StudioDrawBones( void )
{
	dstudiobone_t	*pbones = (dstudiobone_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	vec3_t		point;
	int		i;

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		if( pbones[i].parent >= 0 )
		{
			pglPointSize( 3.0f );
			pglColor3f( 1, 0.7f, 0 );
			pglBegin( GL_LINES );
			
			Matrix4x4_OriginFromMatrix( m_pbonestransform[pbones[i].parent], point );
			pglVertex3fv( point );
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			
			pglEnd();

			pglColor3f( 0, 0, 0.8f );
			pglBegin( GL_POINTS );
			if( pbones[pbones[i].parent].parent != -1 )
			{
				Matrix4x4_OriginFromMatrix( m_pbonestransform[pbones[i].parent], point );
				pglVertex3fv( point );
			}
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			pglEnd();
		}
		else
		{
			// draw parent bone node
			pglPointSize( 5.0f );
			pglColor3f( 0.8f, 0, 0 );
			pglBegin( GL_POINTS );
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			pglEnd();
		}
	}
	pglPointSize( 1.0f );
}

void R_StudioDrawHitboxes( void )
{
	int	i, j;

	pglColor4f( 1, 0, 0, 0.5f );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	for( i = 0; i < m_pStudioHeader->numhitboxes; i++ )
	{
		dstudiobbox_t	*pbboxes = (dstudiobbox_t *)((byte *)m_pStudioHeader + m_pStudioHeader->hitboxindex);
		vec3_t		v[8], v2[8], bbmin, bbmax;

		VectorCopy( pbboxes[i].bbmin, bbmin );
		VectorCopy( pbboxes[i].bbmax, bbmax );

		v[0][0] = bbmin[0];
		v[0][1] = bbmax[1];
		v[0][2] = bbmin[2];

		v[1][0] = bbmin[0];
		v[1][1] = bbmin[1];
		v[1][2] = bbmin[2];

		v[2][0] = bbmax[0];
		v[2][1] = bbmax[1];
		v[2][2] = bbmin[2];

		v[3][0] = bbmax[0];
		v[3][1] = bbmin[1];
		v[3][2] = bbmin[2];

		v[4][0] = bbmax[0];
		v[4][1] = bbmax[1];
		v[4][2] = bbmax[2];

		v[5][0] = bbmax[0];
		v[5][1] = bbmin[1];
		v[5][2] = bbmax[2];

		v[6][0] = bbmin[0];
		v[6][1] = bbmax[1];
		v[6][2] = bbmax[2];

		v[7][0] = bbmin[0];
		v[7][1] = bbmin[1];
		v[7][2] = bbmax[2];

		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[0], v2[0] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[1], v2[1] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[2], v2[2] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[3], v2[3] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[4], v2[4] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[5], v2[5] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[6], v2[6] );
		Matrix4x4_VectorTransform( m_pbonestransform[pbboxes[i].bone], v[7], v2[7] );

		pglBegin( GL_QUAD_STRIP );
		for( j = 0; j < 10; j++ )
			pglVertex3fv( v2[j & 7] );
		pglEnd( );
	
		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv( v2[6] );
		pglVertex3fv( v2[0] );
		pglVertex3fv( v2[4] );
		pglVertex3fv( v2[2] );
		pglEnd( );

		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv( v2[1] );
		pglVertex3fv( v2[7] );
		pglVertex3fv( v2[3] );
		pglVertex3fv( v2[5] );
		pglEnd( );			
	}
	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

void R_StudioDrawAttachments( void )
{
	int	i;
	
	for( i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		dstudioattachment_t	*pattachments = (dstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
		vec3_t		v[4];
		
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].org, v[0] );
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[0], v[1] );
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[1], v[2] );
		Matrix4x4_VectorTransform( m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[2], v[3] );
		
		pglBegin( GL_LINES );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[1] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[2] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[3] );
		pglEnd( );

		pglPointSize( 5.0f );
		pglColor3f( 0, 1, 0 );
		pglBegin( GL_POINTS );
		pglVertex3fv( v[0] );
		pglEnd( );
		pglPointSize( 1.0f );
	}
}

void R_StudioDrawHulls( void )
{
	int	i;
	vec3_t	bbox[8];

	// looks ugly, skip
	if( RI.currententity->ent_type == ED_VIEWMODEL )
		return;

	if(!R_StudioComputeBBox( bbox )) return;

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglBegin( GL_LINES );
	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}
	pglEnd();
}

/*
====================
StudioDrawDebug

====================
*/
void R_StudioDrawDebug( void )
{
	int	i;

	if(( RI.refdef.flags & RDF_NOWORLDMODEL ) || ( r_drawentities->integer < 2 ))
		return;

	pglDisable( GL_TEXTURE_2D );
	GL_SetState( GLSTATE_NO_DEPTH_TEST|GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	pglDepthRange( 0, 0 );

	for( i = 1; i < r_numEntities; i++ )
	{
		RI.previousentity = RI.currententity;
		RI.currententity = &r_entities[i];

		if( !RI.currententity->model || RI.currententity->model->type != mod_studio )
			continue;

		if( RP_LOCALCLIENT( RI.currententity ))
		{
			// ignore localcient in firstperson mode
			if(!(RI.refdef.flags & RDF_THIRDPERSON) && !( RI.params & ( RP_MIRRORVIEW|RP_SHADOWMAPVIEW )))
				continue;
		}

		if( RP_FOLLOWENTITY( RI.currententity ) && RP_LOCALCLIENT( RI.currententity->parent ) && !(RI.refdef.flags & RDF_THIRDPERSON ))
		{
			// ignore entities that linked to localcient
			if(!( RI.params & ( RP_MIRRORVIEW|RP_SHADOWMAPVIEW )))
				continue;
		}

		if( RI.currententity->ent_type == ED_VIEWMODEL )
		{
			if( RI.params & RP_NONVIEWERREF )
				continue;
		}
#if 0
		// this stuff doesn't working corretly with mirrors. disabled
		if( RI.currententity->m_nCachedFrameCount != r_framecount )
			continue; // culled
#endif
		R_StudioSetupRender( RI.currententity, RI.currententity->model );
		R_StudioSetUpTransform( RI.currententity, true );
		
		switch( r_drawentities->integer )
		{
		case 2: R_StudioDrawBones(); break;
		case 3: R_StudioDrawHitboxes(); break;
		case 4: R_StudioDrawAttachments(); break;
		case 5: R_StudioDrawHulls(); break;
		}
	}

	pglDepthRange( gldepthmin, gldepthmax );
	pglEnable( GL_TEXTURE_2D );
}

/*
====================
StudioEstimateGait

====================
*/
void R_StudioEstimateGait( ref_entity_t *e, edict_t *pplayer )
{
	float		dt;
	vec3_t		est_velocity;
	studiovars_t	*pstudio = (studiovars_t *)e->extradata;

	Com_Assert( pstudio == NULL );	
	dt = bound( 0.0f, RI.refdef.frametime, 1.0f );

	if( dt == 0 || e->m_nCachedFrameCount == r_framecount2 )
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->v.velocity, pplayer->v.prediction_error, est_velocity );
	if( m_fGaitEstimation )
	{
		VectorSubtract( e->origin, pstudio->lerp->gaitorigin, est_velocity );
		VectorCopy( e->origin, pstudio->lerp->gaitorigin );

		m_flGaitMovement = VectorLength( est_velocity );
		if( dt <= 0 || m_flGaitMovement / dt < 5 )
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->v.velocity, est_velocity );
		m_flGaitMovement = VectorLength( est_velocity ) * dt;
	}

	if( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = e->angles[YAW] - pstudio->lerp->gaityaw;

		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if( flYawDiff > 180 ) flYawDiff -= 360;
		if( flYawDiff < -180 ) flYawDiff += 360;

		if( dt < 0.25f ) flYawDiff *= dt * 4;
		else flYawDiff *= dt;

		pstudio->lerp->gaityaw += flYawDiff;
		pstudio->lerp->gaityaw = pstudio->lerp->gaityaw - (int)(pstudio->lerp->gaityaw / 360) * 360;
		m_flGaitMovement = 0;
	}
	else
	{
		pstudio->lerp->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if( pstudio->lerp->gaityaw > 180 ) pstudio->lerp->gaityaw = 180;
		if( pstudio->lerp->gaityaw < -180 ) pstudio->lerp->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void R_StudioProcessGait( ref_entity_t *e, edict_t *pplayer, studiovars_t *pstudio )
{
	dstudioseqdesc_t	*pseqdesc;
	float		dt, flYaw;	// view direction relative to movement
	int		iBlend;

	if( e->lerp->curstate.sequence >= m_pStudioHeader->numseq ) 
		e->lerp->curstate.sequence = 0;

	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->lerp->curstate.sequence;
	R_StudioPlayerBlend( pseqdesc, &iBlend, &e->angles[PITCH] );

	pstudio->lerp->curstate.blending[0] = iBlend;
	pstudio->lerp->latched.blending[0] = pstudio->lerp->curstate.blending[0];
	pstudio->lerp->latched.seqblending[0] = pstudio->lerp->curstate.blending[0];

	//Msg( "%f %d\n", e->angles[PITCH], pstudio->lerp->curstate.blending[0] );

	dt = bound( 0.0f, RI.refdef.frametime, 1.0f );

	R_StudioEstimateGait( e, pplayer );

	//Msg( "%f %f\n", e->angles[YAW], pstudio->lerp->gaityaw );

	// calc side to side turning
	flYaw = e->angles[YAW] - pstudio->lerp->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if( flYaw < -180 ) flYaw = flYaw + 360;
	if( flYaw > 180 ) flYaw = flYaw - 360;

	if( flYaw > 120 )
	{
		pstudio->lerp->gaityaw = pstudio->lerp->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if( flYaw < -120 )
	{
		pstudio->lerp->gaityaw = pstudio->lerp->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	pstudio->lerp->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	pstudio->lerp->latched.controller[0] = pstudio->lerp->curstate.controller[0];
	pstudio->lerp->latched.controller[1] = pstudio->lerp->curstate.controller[1];
	pstudio->lerp->latched.controller[2] = pstudio->lerp->curstate.controller[2];
	pstudio->lerp->latched.controller[3] = pstudio->lerp->curstate.controller[3];

	e->angles[YAW] = pstudio->lerp->gaityaw;
	if( e->angles[YAW] < -0 ) e->angles[YAW] += 360;

	if( pplayer->v.gaitsequence >= m_pStudioHeader->numseq ) 
		pplayer->v.gaitsequence = 0;

	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->v.gaitsequence;

	// calc gait frame
	if( pseqdesc->linearmovement[0] > 0 )
	{
		pstudio->lerp->curstate.gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		pstudio->lerp->curstate.gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	pstudio->lerp->curstate.gaitframe = pstudio->lerp->curstate.gaitframe - (int)(pstudio->lerp->curstate.gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if( pstudio->lerp->curstate.gaitframe < 0 ) pstudio->lerp->curstate.gaitframe += pseqdesc->numframes;
}

static bool R_StudioSetupModel( ref_entity_t *e, ref_model_t *mod )
{
	edict_t		*m_pEntity = ri.GetClientEdict( e->index );
	studiovars_t	*pstudio = e->extradata;
	int		i, m_nPlayerIndex;

	if( e->m_nCachedFrameCount == r_framecount2 )
		return 1;

	Com_Assert( pstudio == NULL );

	// special handle for player model
	if( e->ent_type == ED_CLIENT || e->renderfx == kRenderFxDeadPlayer )
	{
		//Msg( "DrawPlayer %d\n", pstudio->lerp->blending[0] );
		//Msg( "DrawPlayer %d %d (%d)\n", r_framecount2, m_pEntity->serialnumber, e->lerp->curstate.sequence );
		//Msg( "Player %.2f %.2f %.2f\n", m_pEntity->v.velocity[0], m_pEntity->v.velocity[1], m_pEntity->v.velocity[2] );

		if( e->renderfx == kRenderFxDeadPlayer )
		{
			// prepare to draw dead player
			m_pEntity = ri.GetClientEdict( e->renderamt );
			if( m_pEntity ) m_pEntity->v.gaitsequence = 0;
		}

		if( !m_pEntity ) return 0;
		m_nPlayerIndex = m_pEntity->serialnumber - 1;

		if( m_nPlayerIndex < 0 || m_nPlayerIndex >= ri.GetMaxClients( ))
			return 0;	// weird client ?

		if( m_pEntity->v.gaitsequence <= 0 )
		{
			for( i = 0; i < 4; i++ )	// clear torso controllers
				pstudio->lerp->latched.controller[i] = pstudio->lerp->curstate.controller[i] = 0x7F;
			e->gaitsequence = 0;	// StudioSetupBones() issuses

			R_StudioSetUpTransform ( e, false );
		}
		else
		{
			vec3_t	save_angles;

			VectorCopy( e->angles, save_angles );
			R_StudioProcessGait( e, m_pEntity, pstudio );
			R_StudioSetUpTransform ( e, false );
			VectorCopy( save_angles, e->angles );
		}

		if( r_himodels->integer ) e->body = 255; // show highest resolution multiplayer model
		if( !( glw_state.developer == 0 && ri.GetMaxClients() == 1 )) e->body = 1; // force helmet

	}
	else R_StudioSetUpTransform ( e, false );

	if( e->movetype == MOVETYPE_FOLLOW && e->parent )
	{
		R_StudioMergeBones( e, mod );
	}
	else
	{
		R_StudioSetupBones( e );	
		R_StudioSaveBones( e );
	}

	if( m_pEntity && e->movetype != MOVETYPE_FOLLOW && !RI.refdef.paused && e->m_nCachedFrameCount != r_framecount2 )
	{
		R_StudioCalcAttachments( e );
		R_StudioProcessEvents( e, m_pEntity );
	}
	e->m_nCachedFrameCount = r_framecount2;	// cached frame

	return 1;
}

void R_StudioDrawPoints( const meshbuffer_t *mb, ref_entity_t *e )
{
	int		i, j, m_skinnum = RI.currententity->skin;
	int		modelnum = ((-mb->infokey - 1) & 0xFF);
	int		meshnum = (((-mb->infokey - 1) & 0xFF00)>>8);
	ref_model_t	*model = Mod_ForHandle( mb->modhandle );
	vec3_t		*pstudioverts, *pstudionorms;
	byte		*pvertbone, *pnormbone;
	short		*pskinref, *ptricmds;
	int		flags, features;
	dstudiotexture_t	*ptexture;
	studiovars_t	*studio;
	dstudiomesh_t	*pmesh;
	ref_shader_t	*shader;
	float		s, t;

	MB_NUM2SHADER( mb->shaderkey, shader );

	R_StudioSetupRender( e, model );
	R_StudioSetupSubModel( RI.currententity->body, modelnum );

	if( meshnum > m_pSubModel->nummesh ) return; // invalid mesh

	// setup all mesh pointers
	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + meshnum;
	ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
	ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
	if( m_skinnum != 0 && m_skinnum < m_pTextureHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pTextureHeader->numskinref);
	s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
	t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;
	flags = ptexture[pskinref[pmesh->skinref]].flags;
	studio = (studiovars_t *)e->extradata;

	// detect mesh features
	features = MF_NONBATCHED | shader->features;
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		features &= ~( MF_COLORS|MF_SVECTORS|MF_ENABLENORMALS );
		if(!( shader->features & MF_DEFORMVS ))
			features &= ~MF_NORMALS;
	}
	else
	{
		if(( features & MF_SVECTORS ) || r_shownormals->integer )
			features |= MF_NORMALS;
		if( RI.currententity->outlineHeight )
			features |= MF_NORMALS|(GL_Support( R_SHADER_GLSL100_EXT ) ? MF_ENABLENORMALS : 0);
	}

	Com_Assert( studio == NULL );
	
	// initialize vertex cache
	if( !studio->mesh[modelnum] )
		studio->mesh[modelnum] = Mem_Alloc( e->mempool, sizeof( studioverts_t ));		
				
	if( studio->mesh[modelnum]->numverts != m_pSubModel->numverts )
		studio->mesh[modelnum]->verts = Mem_Realloc( e->mempool, studio->mesh[modelnum]->verts, sizeof( vec3_t ) * m_pSubModel->numverts );

	if( studio->mesh[modelnum]->numnorms != m_pSubModel->numnorms )
	{
		studio->mesh[modelnum]->norms = Mem_Realloc( e->mempool, studio->mesh[modelnum]->norms, sizeof( vec3_t ) * m_pSubModel->numnorms );
		studio->mesh[modelnum]->chrome = Mem_Realloc( e->mempool, studio->mesh[modelnum]->chrome, sizeof( vec2_t ) * m_pSubModel->numnorms );
	}

	studio->mesh[modelnum]->numverts = m_pSubModel->numverts;
	studio->mesh[modelnum]->numnorms = m_pSubModel->numnorms;

	m_pxformverts = studio->mesh[modelnum]->verts;
	m_pxformnorms = studio->mesh[modelnum]->norms;
	m_pxformlight = studio->mesh[modelnum]->light;
	m_pchrome = studio->mesh[modelnum]->chrome;

	// cache transforms
	if( studio->mesh[modelnum]->m_nCachedFrame != r_framecount2 )
	{
		dstudiomesh_t	*pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
		float		*lv = (float *)m_pxformlight;
		int		vertspermesh = 0;

		for( i = 0; i < m_pSubModel->numverts; i++ )
			Matrix4x4_VectorTransform( m_pbonestransform[pvertbone[i]], pstudioverts[i], m_pxformverts[i] );

		for( i = 0; ( i < m_pSubModel->numnorms ) && ( features & MF_NORMALS ); i++ )
		{
			Matrix4x4_VectorRotate( m_pbonestransform[pnormbone[i]], pstudionorms[i], m_pxformnorms[i] );
		}

		if( m_pStudioHeader->flags & STUDIO_HAS_CHROME )
		{
			for( i = 0; i < m_pSubModel->nummesh; i++ ) 
			{
				int	texflags = ptexture[pskinref[pmesh[i].skinref]].flags;

				for( j = 0; j < pmesh[i].numnorms; j++, lv += 3, pstudionorms++, pnormbone++ )
				{
					if(!( texflags & STUDIO_NF_CHROME )) continue;
					R_StudioSetupChrome( m_pchrome[(float (*)[3])lv - m_pxformlight], modelnum, *pnormbone, (float *)pstudionorms );
				}
			}
		}
		studio->mesh[modelnum]->m_nCachedFrame = r_framecount2;
	}

	R_StudioDrawMesh( mb, ptricmds, s, t, features, flags );
}

void R_DrawStudioModel( const meshbuffer_t *mb )
{
	ref_entity_t	*e;
	int		meshnum = -mb->infokey - 1;

	MB_NUM2ENTITY( mb->sortkey, e );

	if( OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ))
	{
		ref_shader_t	*shader;

		MB_NUM2SHADER( mb->shaderkey, shader );
		if( !R_GetOcclusionQueryResultBool( shader->type == 1 ? OQ_PLANARSHADOW : OQ_ENTITY, e - r_entities, true ))
			return;
	}

	// hack the depth range to prevent view model from poking into walls
	if( e->ent_type == ED_VIEWMODEL )
	{
		pglDepthRange( gldepthmin, gldepthmin + 0.3 * ( gldepthmax - gldepthmin ) );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 ) GL_FrontFace( !glState.frontFace );
          }

	R_StudioDrawPoints( mb, e );

	if( e->ent_type == ED_VIEWMODEL )
	{
		pglDepthRange( gldepthmin, gldepthmax );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 ) GL_FrontFace( !glState.frontFace );
	}
}

bool R_CullStudioModel( ref_entity_t *e )
{
	int		i, j, clipped;
	int		sequence;
	bool		frustum, query;
	uint		modhandle;
	dstudiotexture_t	*ptexture;
	short		*pskinref;
	meshbuffer_t	*mb;

	if( !e->model->extradata )
		return true;

	if( e->ent_type == ED_VIEWMODEL && r_lefthand->integer >= 2 )
		return true; // hidden

	modhandle = Mod_Handle( e->model );

	if( RP_FOLLOWENTITY( e ))
	{
		// cull child entities with parent volume
		R_StudioSetupRender( e->parent, e->parent->model );
		sequence = e->parent->lerp->curstate.sequence;
	}
	else
	{
		R_StudioSetupRender( e, e->model );
		sequence = e->lerp->curstate.sequence;
	}

	if( !R_StudioExtractBbox( m_pStudioHeader, sequence, studio_mins, studio_maxs ))
		return true; // invalid sequence

	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );
	clipped = R_CullModel( e, studio_mins, studio_maxs, studio_radius );
	frustum = clipped & 1;
	if( clipped & 2 ) return true;

	query = OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ) ? true : false;
	if( !frustum && query ) R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_ENTITY, e - r_entities ), e, studio_mins, studio_maxs );

	if(( RI.refdef.flags & RDF_NOWORLDMODEL) || (r_shadows->integer != 1 && !(r_shadows->integer == 2 && (e->flags & EF_PLANARSHADOW))) || R_CullPlanarShadow( e, studio_mins, studio_maxs, query ))
		return frustum; // entity is not in PVS or shadow is culled away by frustum culling

	R_StudioSetupRender( e, e->model );

	// setup bones, play events etc
	R_StudioSetupModel( e, e->model );

	// add it
	ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		R_StudioSetupSubModel( RI.currententity->body, i );
		pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
		if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
			pskinref += (e->skin * m_pTextureHeader->numskinref);

		for( j = 0; j < m_pSubModel->nummesh; j++ ) 
		{
			ref_shader_t	*shader;
			dstudiomesh_t	*pmesh;
	
			pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
			if( e->customShader ) shader = e->customShader;
			else shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];

			if( shader && ( shader->sort <= SORT_ALPHATEST ))
			{
				mb = R_AddMeshToList( MB_MODEL, NULL, R_PlanarShadowShader(), -(((j<<8)|i)+1 ));
				if( mb ) mb->modhandle = modhandle;
			}
		}
	}
	return frustum;
}

void R_AddStudioModelToList( ref_entity_t *e )
{
	uint		modhandle;
	uint		entnum = e - r_entities;
	ref_model_t	*mod = e->model;
	mfog_t		*fog = NULL;
	dstudiotexture_t	*ptexture;
	short		*pskinref;
	int		sequence;
	int		i, j;

	if( !e->model->extradata ) return;

	if( RP_FOLLOWENTITY( e ))
	{
		// cull child entities with parent volume
		R_StudioSetupRender( e->parent, e->parent->model );
		sequence = e->parent->lerp->curstate.sequence;
	}
	else
	{
		R_StudioSetupRender( e, e->model );
		sequence = e->lerp->curstate.sequence;
	}

	if( !R_StudioExtractBbox( m_pStudioHeader, sequence, studio_mins, studio_maxs ))
		return; // invalid sequence

	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );
	modhandle = Mod_Handle( mod );

	R_StudioSetupRender( e, e->model );
	if( m_pStudioHeader->numbodyparts == 0 ) return; // nothing to draw

	if( RI.params & RP_SHADOWMAPVIEW )
	{
		if( r_entShadowBits[entnum] & RI.shadowGroup->bit )
		{
			if( !r_shadows_self_shadow->integer )
				r_entShadowBits[entnum] &= ~RI.shadowGroup->bit;
			if( e->ent_type == ED_VIEWMODEL )
				return;
		}
		else
		{
			R_StudioModelLerpBBox( e, mod );
			if( !R_CullModel( e, studio_mins, studio_maxs, studio_radius ))
				r_entShadowBits[entnum] |= RI.shadowGroup->bit;
			return; // mark as shadowed, proceed with caster otherwise
		}
	}
	else
	{
		fog = R_FogForSphere( e->origin, studio_radius );
#if 0
		if( !( e->ent_type == ED_VIEWMODEL ) && fog )
		{
			R_StudioModelLerpBBox( e, mod );
			if( R_CompletelyFogged( fog, e->origin, studio_radius ))
				return;
		}
#endif
	}

	// setup bones, play events etc
	R_StudioSetupModel( e, e->model );

	// add base model
	ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		R_StudioSetupSubModel( e->body, i );
		pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
		if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
			pskinref += (e->skin * m_pTextureHeader->numskinref);

		for( j = 0; j < m_pSubModel->nummesh; j++ ) 
		{
			ref_shader_t	*shader;
			dstudiomesh_t	*pmesh;
	
			pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;

			if( e->customShader ) shader = e->customShader;
			else shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];

			if( shader ) R_AddModelMeshToList( modhandle, fog, shader, ((j<<8)|i));
		}
	}
}

void R_StudioRunEvents( ref_entity_t *e )
{
	if( !e->model->extradata ) return;
	if( RP_FOLLOWENTITY( e )) return;

	RI.currententity = e;
	R_StudioSetupRender( e, e->model );
	if( m_pStudioHeader->numbodyparts == 0 )
		return; // nothing to draw

	// setup bones, play events etc
	R_StudioSetupModel( e, e->model );
}