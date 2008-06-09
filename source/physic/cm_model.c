//=======================================================================
//			Copyright XashXT Group 2007 �
//			cm_model.c - collision model
//=======================================================================

#include "cm_local.h"
#include "basefiles.h"

clipmap_t		cm;
studio_t		studio;
convex_hull_t	hull;
box_t		box;
mapleaf_t		leaf;
tracework_t	maptrace;

cvar_t *cm_noareas;
cmodel_t *loadmodel;
int registration_sequence = 0;

/*
===============================================================================

			CM COMMON UTILS

===============================================================================
*/
/*
=================
CM_GetStringFromTable
=================
*/
const char *CM_GetStringFromTable( int index )
{
	if( cm.stringdata )
	{
		MsgDev(D_NOTE, "CM_GetStringFromTable: %s\n", &cm.stringdata[cm.stringtable[index]] );
		return &cm.stringdata[cm.stringtable[index]];
	}
	return NULL;
}

void CM_GetPoint( int index, vec3_t out )
{
	int vert_index;
	int edge_index = cm.surfedges[index];

	if(edge_index > 0) vert_index = cm.edges[edge_index].v[0];
	else vert_index = cm.edges[-edge_index].v[1];
	CM_ConvertPositionToMeters( out, cm.vertices[vert_index].point );
}

void CM_GetPoint2( int index, vec3_t out )
{
	int vert_index;
	int edge_index = cm.surfedges[index];

	if(edge_index > 0) vert_index = cm.edges[edge_index].v[0];
	else vert_index = cm.edges[-edge_index].v[1];
	CM_ConvertDimensionToMeters( out, cm.vertices[vert_index].point );
}

/*
=================
CM_BoundBrush
=================
*/
void CM_BoundBrush( cbrush_t *b )
{
	cbrushside_t	*sides;
	sides = &cm.brushsides[b->firstbrushside];

	b->bounds[0][0] = -sides[0].plane->dist;
	b->bounds[1][0] = sides[1].plane->dist;

	b->bounds[0][1] = -sides[2].plane->dist;
	b->bounds[1][1] = sides[3].plane->dist;

	b->bounds[0][2] = -sides[4].plane->dist;
	b->bounds[1][2] = sides[5].plane->dist;
}

/*
================
CM_FreeModel
================
*/
void CM_FreeModel( cmodel_t *mod )
{
	Mem_FreePool( &mod->mempool );
	memset(mod->physmesh, 0, MAXSTUDIOMODELS * sizeof(cmesh_t));
	memset(mod, 0, sizeof(*mod));
	mod = NULL;
}

int CM_NumTexinfo( void ) { return cm.numtexinfo; }
int CM_NumClusters( void ) { return cm.numclusters; }
int CM_NumInlineModels( void ) { return cm.numbmodels; }
const char *CM_EntityString( void ) { return cm.entitystring; }
const char *CM_TexName( int index ) { return cm.surfdesc[index].name; }

/*
===============================================================================

					MAP LOADING

===============================================================================
*/
/*
=================
BSP_CreateMeshBuffer
=================
*/
void BSP_CreateMeshBuffer( int modelnum )
{
	dface_t	*m_face;
	int	d, i, j, k;
	int	flags;

	// ignore world or bsplib instance
	if(cm.use_thread || modelnum < 1 || modelnum >= cm.num_models)
		return;

	loadmodel = &cm.bmodels[modelnum];
	loadmodel->type = mod_brush;
	hull.m_pVerts = &studio.vertices[0]; // using studio vertex buffer for bmodels too
	hull.numverts = 0; // clear current count

	for( d = 0, i = loadmodel->firstface; d < loadmodel->numfaces; i++, d++ )
	{
		vec3_t *face;

		m_face = cm.surfaces + i;
		flags = cm.surfdesc[m_face->desc].flags;
		k = m_face->firstedge;

		// sky is noclip for all physobjects
		if(flags & SURF_SKY) continue;
		face = Mem_Alloc( loadmodel->mempool, m_face->numedges * sizeof(vec3_t));
		for(j = 0; j < m_face->numedges; j++ ) 
		{
			CM_GetPoint2( k+j, hull.m_pVerts[hull.numverts] );
			hull.numverts++;
		}
		if( face ) Mem_Free( face ); // faces with 0 edges ?
	}
	if( hull.numverts )
	{
		// grab vertices
		loadmodel->physmesh[loadmodel->numbodies].verts = Mem_Alloc( loadmodel->mempool, hull.numverts * sizeof(vec3_t));
		Mem_Copy( loadmodel->physmesh[loadmodel->numbodies].verts, hull.m_pVerts, hull.numverts * sizeof(vec3_t));
		loadmodel->physmesh[loadmodel->numbodies].numverts = hull.numverts;
		loadmodel->numbodies++;
	}
}

void BSP_LoadModels( lump_t *l )
{
	dmodel_t	*in;
	cmodel_t	*out;
	int	*indexes;
	int	i, j, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadModels: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if(count < 1) Host_Error("Map %s without models\n", cm.name );
	if(count > MAX_MODELS ) Host_Error("Map %s has too many models\n", cm.name );
	cm.numbmodels = cm.num_models = count;
	out = &cm.bmodels[0];

	for ( i = 0; i < count; i++, in++, out++)
	{
		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
		}
		out->firstface = LittleLong( in->firstface );
		out->numfaces = LittleLong( in->numfaces );
		out->firstbrush = LittleLong( in->firstbrush );
		out->numbrushes = LittleLong( in->numbrushes );

		com.strncpy( out->name, va("*%i", i ), sizeof(out->name));
		out->mempool = Mem_AllocPool( out->name );

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numleafbrushes = LittleLong( in->numbrushes );
		indexes = Mem_Alloc( out->mempool, out->leaf.numleafbrushes * sizeof(dword));
		out->leaf.firstleafbrush = indexes - cm.leafbrushes;
		for( j = 0; j < out->leaf.numleafbrushes; j++ )
			indexes[j] = LittleLong( in->firstbrush ) + j;
		BSP_CreateMeshBuffer( i ); // bsp physic
	}
}

/*
=================
BSP_LoadSurfDesc
=================
*/
void BSP_LoadSurfDesc( lump_t *l )
{
	dsurfdesc_t	*in;
	csurface_t	*out;
	int 		i, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadSurfDesc: funny lump size\n" );
	count = l->filelen / sizeof(*in);

	out = cm.surfdesc = (csurface_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numtexinfo = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		com.strncpy(out->name, CM_GetStringFromTable(LittleLong( in->texid )), MAX_STRING );
		out->flags = LittleLong( in->flags );
		out->value = LittleLong( in->value );
	}
}

/*
=================
BSP_LoadNodes
=================
*/
void BSP_LoadNodes( lump_t *l )
{
	dnode_t	*in;
	cnode_t	*out;
	int	child, i, j, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadNodes: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if(count < 1) Host_Error("Map %s has no nodes\n", cm.name );
	out = cm.nodes = (cnode_t *)Mem_Alloc( cmappool, (count + 6) * sizeof(*out));
	cm.numnodes = count;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->plane = cm.planes + LittleLong(in->planenum);
		for (j = 0; j < 2; j++)
		{
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}

}

/*
=================
BSP_LoadBrushes
=================
*/
void BSP_LoadBrushes( lump_t *l )
{
	dbrush_t	*in;
	cbrush_t	*out;
	int	i, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadBrushes: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = cm.brushes = (cbrush_t *)Mem_Alloc( cmappool, (count + 1) * sizeof(*out));
	cm.numbrushes = count;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
		CM_BoundBrush( out );
	}

}

/*
=================
BSP_LoadLeafs
=================
*/
void BSP_LoadLeafs( lump_t *l )
{
	dleaf_t 	*in;
	cleaf_t	*out;
	int	i, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadLeafs: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if( count < 1 ) Host_Error("Map %s with no leafs\n", cm.name );
	out = cm.leafs = (cleaf_t *)Mem_Alloc( cmappool, (count + 1) * sizeof(*out));
	cm.numleafs = count;
	cm.numclusters = 0;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->firstleafbrush = LittleLong( in->firstleafbrush );
		out->numleafbrushes = LittleLong( in->numleafbrushes );
		out->contents = LittleLong( in->contents );
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area );
		if( out->cluster >= cm.numclusters )
			cm.numclusters = out->cluster + 1;
	}

	// probably any wall it's liquid ?
	if( cm.leafs[0].contents != CONTENTS_SOLID )
		Host_Error("Map %s with leaf 0 is not CONTENTS_SOLID\n", cm.name );

	cm.solidleaf = 0;
	cm.emptyleaf = -1;

	for( i = 1; i < count; i++ )
	{
		if(!cm.leafs[i].contents)
		{
			cm.emptyleaf = i;
			break;
		}
	}

	// stuck into brushes
	if( cm.emptyleaf == -1 ) Host_Error("Map %s does not have an empty leaf\n", cm.name );
}

/*
=================
BSP_LoadPlanes
=================
*/
void BSP_LoadPlanes( lump_t *l )
{
	dplane_t	*in;
	cplane_t	*out;
	int	i, j, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadPlanes: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if (count < 1) Host_Error("Map %s with no planes\n", cm.name );
	out = cm.planes = (cplane_t *)Mem_Alloc( cmappool, (count + 12) * sizeof(*out));
	cm.numplanes = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++) 
			out->normal[j] = LittleFloat(in->normal[j]);
		out->dist = LittleFloat( in->dist );
		PlaneClassify( out ); // automatic plane classify		
	}
}

/*
=================
BSP_LoadLeafBrushes
=================
*/
void BSP_LoadLeafBrushes( lump_t *l )
{
	dword	*in, *out;
	int	i, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadLeafBrushes: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if( count < 1 ) Host_Error("Map %s with no leaf brushes\n", cm.name );
	out = cm.leafbrushes = (dword *)Mem_Alloc( cmappool, (count + 1) * sizeof(*out));
	cm.numleafbrushes = count;
	for ( i = 0; i < count; i++, in++, out++) *out = LittleShort(*in);
}

/*
=================
BSP_LoadBrushSides
=================
*/
void BSP_LoadBrushSides( lump_t *l )
{
	dbrushside_t 	*in;
	cbrushside_t	*out;
	int		i, j, num,count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadBrushSides: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = cm.brushsides = (cbrushside_t *)Mem_Alloc( cmappool, (count + 6) * sizeof(*out));
	cm.numbrushsides = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		num = LittleLong(in->planenum);
		out->plane = cm.planes + num;
		j = LittleLong(in->surfdesc);
		j = bound(0, j, cm.numtexinfo - 1);
		out->surface = cm.surfdesc + j;
	}
}

/*
=================
BSP_LoadAreas
=================
*/
void BSP_LoadAreas( lump_t *l )
{
	darea_t 		*in;
	carea_t		*out;
	int		i, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
  	out = cm.areas = (carea_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numareas = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->numareaportals = LittleLong(in->numareaportals);
		out->firstareaportal = LittleLong(in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

/*
=================
BSP_LoadAreaPortals
=================
*/
void BSP_LoadAreaPortals( lump_t *l )
{
	dareaportal_t	*in, *out;
	int		i, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadAreaPortals: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = cm.areaportals = (dareaportal_t *)Mem_Alloc( cmappool, count * sizeof(*out));
	cm.numareaportals = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->portalnum = LittleLong(in->portalnum);
		out->otherarea = LittleLong(in->otherarea);
	}
}

/*
=================
BSP_LoadVisibility
=================
*/
void BSP_LoadVisibility( lump_t *l )
{
	int	i;

	if(!l->filelen)
	{
		cm.vis = NULL;
		return;
	}

	cm.visibility = (byte *)Mem_Alloc( cmappool, l->filelen );
	Mem_Copy( cm.visibility, cm.mod_base + l->fileofs, l->filelen );
	cm.vis = (dvis_t *)cm.visibility; // conversion
	cm.vis->numclusters = LittleLong( cm.vis->numclusters );
	for (i = 0; i < cm.vis->numclusters; i++)
	{
		cm.vis->bitofs[i][0] = LittleLong(cm.vis->bitofs[i][0]);
		cm.vis->bitofs[i][1] = LittleLong(cm.vis->bitofs[i][1]);
	}
}

/*
=================
BSP_LoadEntityString
=================
*/
void BSP_LoadEntityString( lump_t *l )
{
	cm.entitystring = (byte *)Mem_Alloc( cmappool, l->filelen );
	Mem_Copy( cm.entitystring, cm.mod_base + l->fileofs, l->filelen );
}

/*
=================
BSP_LoadVerts
=================
*/
void BSP_LoadVerts( lump_t *l )
{
	dvertex_t		*in, *out;
	int		i, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadVerts: funny lump size\n");
	count = l->filelen / sizeof(*in);
	cm.vertices = out = Mem_Alloc( cmappool, count * sizeof(*out));

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->point[0] = LittleFloat(in->point[0]);
		out->point[1] = LittleFloat(in->point[1]);
		out->point[2] = LittleFloat(in->point[2]);
	}
}

/*
=================
BSP_LoadEdges
=================
*/
void BSP_LoadEdges( lump_t *l )
{
	dedge_t	*in, *out;
	int 	i, count;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadEdges: funny lump size\n");
	count = l->filelen / sizeof(*in);
	cm.edges = out = Mem_Alloc( cmappool, count * sizeof(*out));

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = LittleLong(in->v[0]);
		out->v[1] = LittleLong(in->v[1]);
	}
}

/*
=================
BSP_LoadSurfedges
=================
*/
void BSP_LoadSurfedges( lump_t *l )
{	
	int	*in, *out;
	int	i, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadSurfedges: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES) Host_Error("BSP_LoadSurfedges: funny lump size\n");
	cm.surfedges = out = Mem_Alloc( cmappool, count * sizeof(*out));	
	for ( i = 0; i < count; i++) out[i] = LittleLong(in[i]);
}

/*
=================
BSP_LoadFaces
=================
*/
void BSP_LoadFaces( lump_t *l )
{
	dface_t		*in, *out;
	int		i;

	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadFaces: funny lump size\n");
	cm.numfaces = l->filelen / sizeof(*in);
	cm.surfaces = out = Mem_Alloc( cmappool, cm.numfaces * sizeof(*out));	

	for( i = 0; i < cm.numfaces; i++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleLong(in->numedges);		
		out->desc = LittleLong(in->desc);
	}
}

/*
=================
BSP_LoadCollision
=================
*/
void BSP_LoadCollision( lump_t *l )
{
	byte	*in;
	int	count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("BSP_LoadCollision: funny lump size\n");
	count = l->filelen / sizeof(*in);
	cm.world_tree = VFS_Create( in, count );	
}

/*
=================
BSP_LoadStringData
=================
*/
void BSP_LoadStringData( lump_t *l )
{	
	if(!l->filelen)
	{
		cm.stringdata = NULL;
		return;
	}
	cm.stringdata = (char *)Mem_Alloc( cmappool, l->filelen );
	Mem_Copy( cm.stringdata, cm.mod_base + l->fileofs, l->filelen );
}

/*
=================
BSP_LoadBuiltinProgs
=================
*/
void BSP_LoadBuiltinProgs( lump_t *l )
{	
	// not implemented
	if(!l->filelen)
	{
		return;
	}
}

/*
=================
BSP_LoadStringTable
=================
*/
void BSP_LoadStringTable( lump_t *l )
{	
	int	*in, *out;
	int	i, count;
	
	in = (void *)(cm.mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("CMod_LoadStringTable: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = cm.stringtable = (int *)Mem_Alloc( cmappool, l->filelen );
	for ( i = 0; i < count; i++ ) out[i] = LittleLong(in[i]);
}


/*
===============================================================================

			BSPLIB COLLISION MAKER

===============================================================================
*/
void BSP_BeginBuildTree( void )
{
	// create tree collision
	cm.collision = NewtonCreateTreeCollision( gWorld, NULL );
	NewtonTreeCollisionBeginBuild( cm.collision );
}

void BSP_AddCollisionFace( int facenum )
{
	dface_t	*m_face;
	int	j, k;
	int	flags;

	if(facenum < 0 || facenum >= cm.numfaces)
	{
		MsgDev(D_ERROR, "invalid face number %d, must be in range [0 == %d]\n", facenum, cm.numfaces - 1);
		return;
	}

	m_face = cm.surfaces + facenum;
	flags = cm.surfdesc[m_face->desc].flags;
	k = m_face->firstedge;

	// sky is noclip for all physobjects
	if(flags & SURF_SKY) return;

	if( cm_use_triangles->integer )
	{
		// convert polygon to triangles
		for(j = 0; j < m_face->numedges - 2; j++)
		{
			vec3_t	face[3]; // triangle
			CM_GetPoint( k,	face[0] );
			CM_GetPoint( k+j+1, face[1] );
			CM_GetPoint( k+j+2, face[2] );
			NewtonTreeCollisionAddFace( cm.collision, 3, (float *)face[0], sizeof(vec3_t), 1 );
		}
	}
	else
	{
		vec3_t *face = Mem_Alloc( cmappool, m_face->numedges * sizeof(vec3_t));
		for(j = 0; j < m_face->numedges; j++ ) CM_GetPoint( k+j, face[j] );
		NewtonTreeCollisionAddFace( cm.collision, m_face->numedges, (float *)face[0], sizeof(vec3_t), 1);
		if( face ) Mem_Free( face ); // polygons with 0 edges ?
	}
}

void BSP_EndBuildTree( void )
{
	if( cm.use_thread ) Msg("Optimize collision tree..." );
	NewtonTreeCollisionEndBuild( cm.collision, true );
	if( cm.use_thread ) Msg(" done\n");
}

static void BSP_LoadTree( vfile_t* handle, void* buffer, size_t size )
{
	VFS_Read( handle, buffer, size );
}

void CM_LoadBSP( const void *buffer )
{
	dheader_t		header;

	header = *(dheader_t *)buffer;
	cm.mod_base = (byte *)buffer;

	// loading level
	BSP_LoadVerts(&header.lumps[LUMP_VERTEXES]);
	BSP_LoadEdges(&header.lumps[LUMP_EDGES]);
	BSP_LoadSurfedges(&header.lumps[LUMP_SURFEDGES]);
	BSP_LoadFaces(&header.lumps[LUMP_FACES]);
	BSP_LoadSurfDesc(&header.lumps[LUMP_SURFDESC]);
	BSP_LoadModels(&header.lumps[LUMP_MODELS]);
	BSP_LoadCollision(&header.lumps[LUMP_COLLISION]);
	cm.loaded = true;
	cm.use_thread = true;

	// keep bspdata because we want create bsp models
	// as kinematic objects: doors, fans, pendulums etc
}

void CM_FreeBSP( void )
{
	CM_FreeWorld();
}

void CM_MakeCollisionTree( void )
{
	int	i, world = 0; // world index

	if(!cm.loaded) Host_Error("CM_MakeCollisionTree: map not loaded\n");
	if(cm.collision) return; // already generated
	if(cm.use_thread) Msg("Building collision tree...\n" );

	BSP_BeginBuildTree();

	// world firstface index always equal 0
	if(cm.use_thread) RunThreadsOnIndividual( cm.bmodels[world].numfaces, true, BSP_AddCollisionFace );
	else for( i = 0; i < cm.bmodels[world].numfaces; i++ ) BSP_AddCollisionFace( i );

	BSP_EndBuildTree();
}

void CM_SaveCollisionTree( file_t *f, cmsave_t callback )
{
	CM_MakeCollisionTree(); // create if needed
	NewtonTreeCollisionSerialize( cm.collision, callback, f );
}

void CM_LoadCollisionTree( void )
{
	cm.collision = NewtonCreateTreeCollisionFromSerialization( gWorld, NULL, BSP_LoadTree, cm.world_tree );
	VFS_Close( cm.world_tree );
	cm.world_tree = NULL;
}

void CM_LoadWorld( const void *buffer )
{
	vec3_t		boxP0, boxP1;
	vec3_t		extra = { 10.0f, 10.0f, 10.0f }; 

	CM_LoadBSP( buffer ); // loading bspdata

	cm.use_thread = false;
	if(cm.world_tree) CM_LoadCollisionTree();
	else CM_MakeCollisionTree(); // can be used for old maps

	cm.body = NewtonCreateBody( gWorld, cm.collision );
	NewtonBodyGetMatrix( cm.body, &cm.matrix[0][0] );	// set the global position of this body 
	NewtonCollisionCalculateAABB( cm.collision, &cm.matrix[0][0], &boxP0[0], &boxP1[0] ); 
	NewtonReleaseCollision( gWorld, cm.collision );

	VectorSubtract( boxP0, extra, boxP0 );
	VectorAdd( boxP1, extra, boxP1 );

	NewtonSetWorldSize( gWorld, &boxP0[0], &boxP1[0] ); 
	NewtonSetSolverModel( gWorld, cm_solver_model->integer );
	NewtonSetFrictionModel( gWorld, cm_friction_model->integer );
}

void CM_FreeWorld( void )
{
	int 	i;
	cmodel_t	*mod;

	// free old stuff
	if( cm.loaded ) Mem_EmptyPool( cmappool );
	cm.numplanes = cm.numnodes = cm.numleafs = 0;
	cm.num_models = cm.numfaces = cm.numbmodels = 0;
	cm.name[0] = 0;
	memset( cm.matrix, 0, sizeof(matrix4x4));
	
	// free bmodels too
	for (i = 0, mod = &cm.bmodels[0]; i < cm.numbmodels; i++, mod++)
	{
		if(!mod->name[0]) continue;
		if(mod->registration_sequence != registration_sequence)
			CM_FreeModel( mod );
	}

	if( cm.body )
	{
		// and physical body release too
		NewtonDestroyBody( gWorld, cm.body );
		cm.body = NULL;
		cm.collision = NULL;
	}
	cm.loaded = false;
}

/*
==================
CM_BeginRegistration

Loads in the map and all submodels
==================
*/
cmodel_t *CM_BeginRegistration( const char *name, bool clientload, uint *checksum )
{
	uint		*buf;
	dheader_t		*hdr;
	size_t		length;

	if(!com.strlen(name))
	{
		CM_FreeWorld(); // release old map
		// cinematic servers won't have anything at all
		cm.numleafs = cm.numclusters = cm.numareas = 1;
		*checksum = 0;
		return &cm.bmodels[0];
	}
	if(!com.strcmp( cm.name, name ) && cm.loaded )
	{
		// singleplayer mode: serever already loading map
		*checksum = cm.checksum;
		if(!clientload)
		{
			// rebuild portals for server
			memset( cm.portalopen, 0, sizeof(cm.portalopen));
			CM_FloodAreaConnections();
		}
		// still have the right version
		return &cm.bmodels[0];
	}

	CM_FreeWorld();		// release old map
	registration_sequence++;	// all models are invalid

	// load the newmap
	buf = (uint *)FS_LoadFile( name, &length );
	if(!buf) Host_Error("Couldn't load %s\n", name);

	*checksum = cm.checksum = LittleLong(Com_BlockChecksum (buf, length));
	hdr = (dheader_t *)buf;
	SwapBlock( (int *)hdr, sizeof(dheader_t));	
	if( hdr->version != BSPMOD_VERSION )
		Host_Error("CM_LoadMap: %s has wrong version number (%i should be %i)\n", name, hdr->version, BSPMOD_VERSION);
	cm.mod_base = (byte *)buf;

	// load into heap
	BSP_LoadStringData(&hdr->lumps[LUMP_STRINGDATA]);
	BSP_LoadStringTable(&hdr->lumps[LUMP_STRINGTABLE]);
	BSP_LoadSurfDesc(&hdr->lumps[LUMP_SURFDESC]);
	BSP_LoadLeafs(&hdr->lumps[LUMP_LEAFS]);
	BSP_LoadLeafBrushes(&hdr->lumps[LUMP_LEAFBRUSHES]);
	BSP_LoadPlanes(&hdr->lumps[LUMP_PLANES]);
	BSP_LoadBrushSides(&hdr->lumps[LUMP_BRUSHSIDES]);
	BSP_LoadBrushes(&hdr->lumps[LUMP_BRUSHES]);
	BSP_LoadNodes(&hdr->lumps[LUMP_NODES]);
	BSP_LoadAreas(&hdr->lumps[LUMP_AREAS]);
	BSP_LoadAreaPortals(&hdr->lumps[LUMP_AREAPORTALS]);
	BSP_LoadVisibility(&hdr->lumps[LUMP_VISIBILITY]);
	BSP_LoadEntityString(&hdr->lumps[LUMP_ENTITIES]);
	
	CM_LoadWorld( buf );// load physics collision
	Mem_Free( buf );	// release map buffer
	CM_InitBoxHull();

	com.strncpy( cm.name, name, MAX_STRING );
	memset( cm.portalopen, 0, sizeof(cm.portalopen));
	CM_FloodAreaConnections();
	cm.loaded = true;

	return &cm.bmodels[0];
}

void CM_EndRegistration( void )
{
	cmodel_t	*mod;
	int	i;

	for (i = 0, mod = &cm.cmodels[0]; i < cm.numcmodels; i++, mod++)
	{
		if(!mod->name[0]) continue;
		if(mod->registration_sequence != registration_sequence)
			CM_FreeModel( mod );
	}
}

int CM_LeafContents( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafContents: bad number %d\n", leafnum );
	return cm.leafs[leafnum].contents;
}

int CM_LeafCluster( int leafnum )
{
	if( leafnum < 0 || leafnum >= cm.numleafs )
		Host_Error("CM_LeafCluster: bad number %d\n", leafnum );
	return cm.leafs[leafnum].cluster;
}

int CM_LeafArea( int leafnum )
{
	if (leafnum < 0 || leafnum >= cm.numleafs)
		Host_Error("CM_LeafArea: bad number %d\n", leafnum );
	return cm.leafs[leafnum].area;
}

/*
===============================================================================

			CM_BOX HULL

===============================================================================
*/
/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull( void )
{
	cplane_t		*p;
	cbrushside_t	*s;
	int		i, side;
	
	box.planes = &cm.planes[cm.numplanes];
	box.brush = &cm.brushes[cm.numbrushes];
	box.brush->numsides = 6;
	box.brush->firstbrushside = cm.numbrushsides;
	box.brush->contents = CONTENTS_MONSTER;//FIXME
	box.model = &cm.bmodels[BOX_MODEL_HANDLE];
	com.strcpy( box.model->name, "*4095" );
	box.model->leaf.numleafbrushes = 1;
	box.model->leaf.firstleafbrush = cm.numleafbrushes;
	cm.leafbrushes[cm.numleafbrushes] = cm.numbrushes;

	for (i = 0; i < 6; i++)
	{
		side = i & 1;

		// brush sides
		s = &cm.brushsides[cm.numbrushsides+i];
		s->plane = cm.planes+(cm.numplanes+i*2+side);
		s->surface = &cm.nullsurface;

		// planes
		p = &box.planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear(p->normal);
		p->normal[i>>1] = 1;

		p = &box.planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;
		PlaneClassify( p );
	}	

	// capsule name
	com.strcpy( cm.bmodels[CAPSULE_MODEL_HANDLE].name, "*4094" );
}

/*
===================
CM_TempBoxModel

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
Capsules are handled differently though.
===================
*/
cmodel_t *CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule )
{
	VectorCopy( mins, box.model->mins );
	VectorCopy( maxs, box.model->maxs );

	if( capsule )
	{
		return &cm.bmodels[CAPSULE_MODEL_HANDLE];
	}

	box.planes[0].dist = maxs[0];
	box.planes[1].dist = -maxs[0];
	box.planes[2].dist = mins[0];
	box.planes[3].dist = -mins[0];
	box.planes[4].dist = maxs[1];
	box.planes[5].dist = -maxs[1];
	box.planes[6].dist = mins[1];
	box.planes[7].dist = -mins[1];
	box.planes[8].dist = maxs[2];
	box.planes[9].dist = -maxs[2];
	box.planes[10].dist = mins[2];
	box.planes[11].dist = -mins[2];

	VectorCopy( mins, box.brush->bounds[0] );
	VectorCopy( maxs, box.brush->bounds[1] );

	return &cm.bmodels[BOX_MODEL_HANDLE];
}

/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( cmodel_t *cmod, vec3_t mins, vec3_t maxs )
{
	if( cmod )
	{
		VectorCopy( cmod->mins, mins );
		VectorCopy( cmod->maxs, maxs );
	}
	else
	{
		VectorSet( mins, -32, -32, -32 );
		VectorSet( maxs,  32,  32,  32 );
		MsgWarn("can't compute bounding box, use default size\n");
	}
}


/*
===============================================================================

STUDIO SHARED CMODELS

===============================================================================
*/
int CM_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

void CM_GetBodyCount( void )
{
	if(studio.hdr)
	{
		studio.bodypart = (mstudiobodyparts_t *)((byte *)studio.hdr + studio.hdr->bodypartindex);
		studio.bodycount = studio.bodypart->nummodels;
	}
	else studio.bodycount = 0; // just reset it
}

/*
====================
CM_StudioCalcBoneQuaterion
====================
*/
void CM_StudioCalcBoneQuaterion( mstudiobone_t *pbone, float *q )
{
	int	i;
	vec3_t	angle1;

	for(i = 0; i < 3; i++) angle1[i] = pbone->value[i+3];
	AngleQuaternion( angle1, q );
}

/*
====================
CM_StudioCalcBonePosition
====================
*/
void CM_StudioCalcBonePosition( mstudiobone_t *pbone, float *pos )
{
	int	i;
	for(i = 0; i < 3; i++) pos[i] = pbone->value[i];
}

/*
====================
CM_StudioSetUpTransform
====================
*/
void CM_StudioSetUpTransform ( void )
{
	vec3_t	mins, maxs;
	vec3_t	modelpos;

	hull.numverts = 0; // clear current count
	CM_StudioExtractBbox( studio.hdr, 0, mins, maxs );// adjust model center
	VectorAdd( mins, maxs, modelpos );
	VectorScale( modelpos, -0.5, modelpos );

	VectorSet( vec3_angles, 0.0f, -90.0f, 90.0f );	// rotate matrix for 90 degrees
	AngleVectors( vec3_angles, studio.rotmatrix[0], studio.rotmatrix[2], studio.rotmatrix[1] );

	studio.rotmatrix[0][3] = modelpos[0];
	studio.rotmatrix[1][3] = modelpos[1];
	studio.rotmatrix[2][3] = (fabs(modelpos[2]) > 0.25) ? modelpos[2] : mins[2]; // stupid newton bug
	studio.rotmatrix[2][2] *= -1;
}

void CM_StudioCalcRotations ( float pos[][3], vec4_t *q )
{
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);
	int		i;

	for (i = 0; i < studio.hdr->numbones; i++, pbone++ ) 
	{
		CM_StudioCalcBoneQuaterion( pbone, q[i] );
		CM_StudioCalcBonePosition( pbone, pos[i]);
	}
}

/*
====================
CM_StudioSetupBones
====================
*/
void CM_StudioSetupBones( void )
{
	int		i;
	mstudiobone_t	*pbones;
	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix3x4		bonematrix;

	CM_StudioCalcRotations( pos, q );
	pbones = (mstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);

	for (i = 0; i < studio.hdr->numbones; i++) 
	{
		QuaternionMatrix( q[i], bonematrix );
		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1) R_ConcatTransforms (studio.rotmatrix, bonematrix, studio.bones[i]);
		else R_ConcatTransforms(studio.bones[pbones[i].parent], bonematrix, studio.bones[i]);
	}
}

void CM_StudioSetupModel ( int bodypart, int body )
{
	int index;

	if(bodypart > studio.hdr->numbodyparts) bodypart = 0;
	studio.bodypart = (mstudiobodyparts_t *)((byte *)studio.hdr + studio.hdr->bodypartindex) + bodypart;

	index = body / studio.bodypart->base;
	index = index % studio.bodypart->nummodels;
	studio.submodel = (mstudiomodel_t *)((byte *)studio.hdr + studio.bodypart->modelindex) + index;
}

void CM_StudioAddMesh( int mesh )
{
	mstudiomesh_t	*pmesh = (mstudiomesh_t *)((byte *)studio.hdr + studio.submodel->meshindex) + mesh;
	short		*ptricmds = (short *)((byte *)studio.hdr + pmesh->triindex);
	int		i;

	while(i = *(ptricmds++))
	{
		for(i = abs(i); i > 0; i--, ptricmds += 4)
		{
			hull.m_pVerts[hull.numverts][0] = INCH2METER(studio.transform[ptricmds[0]][0]);
			hull.m_pVerts[hull.numverts][1] = INCH2METER(studio.transform[ptricmds[0]][1]);
			hull.m_pVerts[hull.numverts][2] = INCH2METER(studio.transform[ptricmds[0]][2]);
			hull.numverts++;
		}
	}
}

void CM_StudioLookMeshes ( void )
{
	int	i;

	for (i = 0; i < studio.submodel->nummesh; i++) 
		CM_StudioAddMesh( i );
}

void CM_StudioGetVertices( void )
{
	int		i;
	vec3_t		*pstudioverts;
	byte		*pvertbone;

	pvertbone = ((byte *)studio.hdr + studio.submodel->vertinfoindex);
	pstudioverts = (vec3_t *)((byte *)studio.hdr + studio.submodel->vertindex);

	for (i = 0; i < studio.submodel->numverts; i++)
	{
		VectorTransform( pstudioverts[i], studio.bones[pvertbone[i]], studio.transform[i]);
	}
	CM_StudioLookMeshes();
}

void CM_CreateMeshBuffer( byte *buffer )
{
	int	i, j;

	// setup global pointers
	studio.hdr = (studiohdr_t *)buffer;
	hull.m_pVerts = &studio.vertices[0];

	CM_GetBodyCount();

	for( i = 0; i < studio.bodycount; i++)
	{
		// already loaded
		if( loadmodel->physmesh[i].verts ) continue;

		CM_StudioSetUpTransform();
		CM_StudioSetupBones();

		// lookup all bodies
		for (j = 0; j < studio.hdr->numbodyparts; j++)
		{
			CM_StudioSetupModel( j, i );
			CM_StudioGetVertices();
		}
		if( hull.numverts )
		{
			loadmodel->physmesh[i].verts = Mem_Alloc( loadmodel->mempool, hull.numverts * sizeof(vec3_t));
			Mem_Copy( loadmodel->physmesh[i].verts, hull.m_pVerts, hull.numverts * sizeof(vec3_t));
			loadmodel->physmesh[i].numverts = hull.numverts;
			loadmodel->numbodies++;
		}
	}
}

bool CM_StudioModel( byte *buffer, uint filesize )
{
	studiohdr_t	*phdr;
	mstudioseqdesc_t	*pseqdesc;

	phdr = (studiohdr_t *)buffer;
	if( phdr->version != STUDIO_VERSION )
	{
		MsgWarn("CM_StudioModel: %s has wrong version number (%i should be %i)", phdr->name, phdr->version, STUDIO_VERSION);
		return false;
	}

	loadmodel->numbodies = 0;
	loadmodel->type = mod_studio;

	// calcualte bounding box
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	VectorCopy( pseqdesc[0].bbmin, loadmodel->mins );
	VectorCopy( pseqdesc[0].bbmax, loadmodel->maxs );
	loadmodel->numframes = pseqdesc[0].numframes;	// FIXME: get numframes from current sequence (not first)

	CM_CreateMeshBuffer( buffer ); // newton collision mesh

	return true;
}

bool CM_SpriteModel( byte *buffer, uint filesize )
{
	dsprite_t		*phdr;

	phdr = (dsprite_t *)buffer;

	if( phdr->version != SPRITE_VERSION )
	{
		MsgWarn("CM_SpriteModel: %s has wrong version number (%i should be %i)\n", loadmodel->name, phdr->version, SPRITE_VERSION );
		return false;
	}
          
	loadmodel->type = mod_sprite;
	loadmodel->numbodies = 0; // sprites don't have bodies
	loadmodel->numframes = phdr->numframes;
	loadmodel->mins[0] = loadmodel->mins[1] = -phdr->bounds[0] / 2;
	loadmodel->maxs[0] = loadmodel->maxs[1] = phdr->bounds[0] / 2;
	loadmodel->mins[2] = -phdr->bounds[1] / 2;
	loadmodel->maxs[2] = phdr->bounds[1] / 2;
	return true;
}

bool CM_BrushModel( byte *buffer, uint filesize )
{
	MsgDev( D_WARN, "CM_BrushModel: not implemented\n");
	return false;
}

cmodel_t *CM_RegisterModel( const char *name )
{
	byte	*buf;
	int	i, size;
	cmodel_t	*mod;

	if(!name[0]) return NULL;
	if(name[0] == '*') 
	{
		i = com.atoi( name + 1);
		if( i < 1 || !cm.loaded || i >= cm.numbmodels)
		{
			MsgDev(D_WARN, "CM_InlineModel: bad submodel number %d\n", i );
			return NULL;
		}

		// prolonge registration
		cm.bmodels[i].registration_sequence = registration_sequence;
		return &cm.bmodels[i];
	}
	for( i = 0; i < cm.numcmodels; i++ )
          {
		mod = &cm.cmodels[i];
		if(!mod->name[0]) continue;
		if(!com.strcmp( name, mod->name ))
		{
			// prolonge registration
			mod->registration_sequence = registration_sequence;
			return mod;
		}
	} 

	// find a free model slot spot
	for( i = 0, mod = cm.cmodels; i < cm.numcmodels; i++, mod++)
	{
		if(!mod->name[0]) break; // free spot
	}
	if( i == cm.numcmodels)
	{
		if( cm.numcmodels == MAX_MODELS )
		{
			MsgWarn("CM_LoadModel: MAX_MODELS limit exceeded\n" );
			return NULL;
		}
		cm.numcmodels++;
	}

	com.strncpy( mod->name, name, sizeof(mod->name));
	buf = FS_LoadFile( name, &size );
	if(!buf)
	{
		MsgWarn("CM_LoadModel: %s not found\n", name );
		memset(mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	MsgDev(D_NOTE, "CM_LoadModel: load %s\n", name );
	mod->mempool = Mem_AllocPool(mod->name);
	loadmodel = mod;

	// call the apropriate loader
	switch(LittleLong(*(uint *)buf))
	{
	case IDSTUDIOHEADER:
		CM_StudioModel( buf, size );
		break;
	case IDSPRITEHEADER:
		CM_SpriteModel( buf, size );
		break;
	case IDBSPMODHEADER:
		CM_BrushModel( buf, size );//FIXME
		break;
	}
	Mem_Free( buf ); 
	return mod;
}