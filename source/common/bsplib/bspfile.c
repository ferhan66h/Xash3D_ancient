//=======================================================================
//			Copyright XashXT Group 2007 �
//		    	bspfile.c - read\save bsp file
//=======================================================================

#include <stdio.h>		// sscanf support
#include "bsplib.h"
#include "byteorder.h"
#include "const.h"

//=============================================================================
wfile_t		*handle;
file_t		*wadfile;
dheader_t		*header;

int		num_entities;
bsp_entity_t	entities[MAX_MAP_ENTITIES];

int		nummodels;
dmodel_t		dmodels[MAX_MAP_MODELS];
int		visdatasize;
byte		dvisdata[MAX_MAP_VISIBILITY];
dvis_t		*dvis = (dvis_t *)dvisdata;
int		lightdatasize;
byte		dlightdata[MAX_MAP_LIGHTING];
int		entdatasize;
char		dentdata[MAX_MAP_ENTSTRING];
int		numleafs;
dleaf_t		dleafs[MAX_MAP_LEAFS];
int		numplanes;
dplane_t		dplanes[MAX_MAP_PLANES];
int		numvertexes;
dvertex_t		dvertexes[MAX_MAP_VERTS];
int		numnodes;
dnode_t		dnodes[MAX_MAP_NODES];
int		numtexinfo;
dtexinfo_t	texinfo[MAX_MAP_TEXINFO];
int		numsurfaces;
dsurface_t	dsurfaces[MAX_MAP_SURFACES];
int		numedges;
dedge_t		dedges[MAX_MAP_EDGES];
int		numleafsurfaces;
dleafface_t	dleafsurfaces[MAX_MAP_LEAFFACES];
int		numleafbrushes;
dleafbrush_t	dleafbrushes[MAX_MAP_LEAFBRUSHES];
int		numsurfedges;
dsurfedge_t	dsurfedges[MAX_MAP_SURFEDGES];
int		numbrushes;
dbrush_t		dbrushes[MAX_MAP_BRUSHES];
int		numbrushsides;
dbrushside_t	dbrushsides[MAX_MAP_BRUSHSIDES];
int		numshaders;
dshader_t		dshaders[MAX_MAP_SHADERS];
int		numareas;
darea_t		dareas[MAX_MAP_AREAS];
int		numareaportals;
dareaportal_t	dareaportals[MAX_MAP_AREAPORTALS];
byte		dcollision[MAX_MAP_COLLISION];
int		dcollisiondatasize;

/*
===============
CompressVis

===============
*/
int CompressVis( byte *vis, byte *dest )
{
	int		j;
	int		rep;
	int		visrow;
	byte	*dest_p;
	
	dest_p = dest;
	visrow = (dvis->numclusters + 7)>>3;
	
	for (j=0 ; j<visrow ; j++)
	{
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for ( j++; j<visrow ; j++)
			if (vis[j] || rep == 255)
				break;
			else rep++;
		*dest_p++ = rep;
		j--;
	}
	
	return dest_p - dest;
}


/*
===================
DecompressVis
===================
*/
void DecompressVis( byte *in, byte *decompressed )
{
	int		c;
	byte	*out;
	int		row;

	row = (dvis->numclusters+7)>>3;	
	out = decompressed;

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		if (!c) Sys_Error("DecompressVis: 0 repeat");
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
}

//=============================================================================

/*
=============
SwapBSPFile

Byte swaps all data in a bsp file.
=============
*/
void SwapBSPFile( bool todisk )
{
	int	i, j;
	
	// models	
	SwapBlock((int *)dmodels, nummodels * sizeof( dmodels[0] ));

	// vertexes
	SwapBlock( (int *)dvertexes, numvertexes * sizeof( dvertexes[0] ));

	// planes
	SwapBlock( (int *)dplanes, numplanes * sizeof( dplanes[0] ));
	
	// texinfos
	SwapBlock( (int *)texinfo, numtexinfo * sizeof( texinfo[0] ));

	// nodes
	SwapBlock( (int *)dnodes, numnodes * sizeof( dnodes[0] ));

	// leafs
	SwapBlock( (int *)dleafs, numleafs * sizeof( dleafs[0] ));

	// leaffaces
	SwapBlock( (int *)dleafsurfaces, numleafsurfaces * sizeof( dleafsurfaces[0] ));

	// leafbrushes
	SwapBlock( (int *)dleafbrushes, numleafbrushes * sizeof( dleafbrushes[0] ));

	// surfedges
	SwapBlock( (int *)dsurfedges, numsurfedges * sizeof( dsurfedges[0] ));

	// edges
	SwapBlock( (int *)dedges, numedges * sizeof( dedges[0] ));

	// brushes
	SwapBlock( (int *)dbrushes, numbrushes * sizeof( dbrushes[0] ));

	// areas
	SwapBlock( (int *)dareas, numareas * sizeof( dareas[0] ));

	// areasportals
	SwapBlock( (int *)dareaportals, numareaportals * sizeof( dareaportals[0] ));

	// brushsides
	SwapBlock( (int *)dbrushsides, numbrushsides * sizeof( dbrushsides[0] ));

	// shaders
	for( i = 0; i < numshaders; i++ )
	{
		dshaders[i].size[0] = LittleLong( dshaders[i].size[0] );
		dshaders[i].size[1] = LittleLong( dshaders[i].size[1] );
		dshaders[i].surfaceFlags = LittleLong( dshaders[i].surfaceFlags );
		dshaders[i].contentFlags = LittleLong( dshaders[i].contentFlags );
	}

	// faces
	for( i = 0; i < numsurfaces; i++ )
	{
		dsurfaces[i].planenum = LittleLong( dsurfaces[i].planenum );
		dsurfaces[i].side = LittleLong (dsurfaces[i].side);
		dsurfaces[i].firstedge = LittleLong( dsurfaces[i].firstedge );
		dsurfaces[i].numedges = LittleLong( dsurfaces[i].numedges );
		dsurfaces[i].texinfo = LittleLong( dsurfaces[i].texinfo );	
		dsurfaces[i].lightofs = LittleLong( dsurfaces[i].lightofs );
	}

	// visibility
	if( todisk ) j = dvis->numclusters;
	else j = LittleLong( dvis->numclusters );
	dvis->numclusters = LittleLong( dvis->numclusters );
	for( i = 0; i < j; i++ )
	{
		dvis->bitofs[i][0] = LittleLong( dvis->bitofs[i][0] );
		dvis->bitofs[i][1] = LittleLong( dvis->bitofs[i][1] );
	}
}

#ifdef BSP_WADFILE
size_t CopyLump( const char *lumpname, void *dest, size_t block_size )
{
	size_t	length;
	byte	*in;

	if( !handle ) return 0;

	in = WAD_Read( handle, lumpname, &length, TYPE_BINDATA );
	if( length % block_size ) Sys_Break( "LoadBSPFile: %s funny lump size\n", lumpname );
	Mem_Copy( dest, in, length );
	Mem_Free( in ); // no need more
	return length / block_size;
}

void AddLump( const char *lumpname, const void *data, size_t length )
{
	int	compress = CMP_NONE;

	if( !handle ) return;
	if( length > 0xffff ) compress = CMP_ZLIB; // save hdd space
	WAD_Write( handle, lumpname, data, length, TYPE_BINDATA, compress );
}

#else

size_t CopyLump( const int lumpname, void *dest, size_t block_size )
{
	size_t	length, ofs;

	length = header->lumps[lumpname].filelen;
	ofs = header->lumps[lumpname].fileofs;
	
	if( length % block_size) Sys_Break( "LoadBSPFile: %i funny lump size\n", lumpname );
	Mem_Copy(dest, (byte *)header + ofs, length);

	return length / block_size;
}

void AddLump( const int lumpname, const void *data, size_t length )
{
	lump_t *lump;

	lump = &header->lumps[lumpname];
	lump->fileofs = LittleLong( FS_Tell( wadfile ));
	lump->filelen = LittleLong( length );

	FS_Write( wadfile, data, (length + 3) & ~3 );
}
#endif


/*
=============
LoadBSPFile
=============
*/
bool LoadBSPFile( void )
{
	byte	*buffer;
	
	buffer = (byte *)FS_LoadFile( va("maps/%s.bsp", gs_filename ), NULL );
	if( !buffer ) return false;

	header = (dheader_t *)buffer; // load the file header
	if( pe ) pe->LoadBSP( buffer );

	MsgDev( D_NOTE, "reading %s.bsp\n", gs_filename );
	
	// swap the header
	SwapBlock( (int *)header, sizeof( header ));

	if( header->ident != IDBSPMODHEADER )
		Sys_Break( "%s.bsp is not a IBSP file\n", gs_filename );
	if( header->version != BSPMOD_VERSION )
		Sys_Break( "%s.bsp is version %i, not %i\n", gs_filename, header->version, BSPMOD_VERSION );

	entdatasize = CopyLump( LUMP_ENTITIES, dentdata, 1 );
	numplanes = CopyLump( LUMP_PLANES, dplanes, sizeof( dplanes[0] ));
	numvertexes = CopyLump( LUMP_VERTEXES, dvertexes, sizeof( dvertexes[0] ));
	visdatasize = CopyLump( LUMP_VISIBILITY, dvisdata, 1 );
	numnodes = CopyLump( LUMP_NODES, dnodes, sizeof( dnodes[0] ));
	numtexinfo = CopyLump( LUMP_TEXINFO, texinfo, sizeof( texinfo[0] ));
	numsurfaces = CopyLump( LUMP_SURFACES, dsurfaces, sizeof( dsurfaces[0] ));
	lightdatasize = CopyLump( LUMP_LIGHTING, dlightdata, 1 );
	numleafs = CopyLump( LUMP_LEAFS, dleafs, sizeof( dleafs[0] ));
	numleafsurfaces = CopyLump( LUMP_LEAFFACES, dleafsurfaces, sizeof( dleafsurfaces[0] ));
	numleafbrushes = CopyLump( LUMP_LEAFBRUSHES, dleafbrushes, sizeof( dleafbrushes[0] ));
	numedges = CopyLump( LUMP_EDGES, dedges, sizeof( dedges[0] ));
	numsurfedges = CopyLump( LUMP_SURFEDGES, dsurfedges, sizeof( dsurfedges[0] ));
	nummodels = CopyLump( LUMP_MODELS, dmodels, sizeof( dmodels[0] ));
	numbrushes = CopyLump( LUMP_BRUSHES, dbrushes, sizeof( dbrushes[0] ));
	numbrushsides = CopyLump( LUMP_BRUSHSIDES, dbrushsides, sizeof( dbrushsides[0] ));
	dcollisiondatasize = CopyLump( LUMP_COLLISION, dcollision, 1 );
	numshaders = CopyLump( LUMP_SHADERS, dshaders, sizeof( dshaders[0] ));
	numareas = CopyLump ( LUMP_AREAS, dareas, sizeof( dareas[0] ));
	numareaportals = CopyLump( LUMP_AREAPORTALS, dareaportals, sizeof( dareaportals[0] ));

	// swap everything
	SwapBSPFile( false );

	return true;
}

//============================================================================

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( void )
{		
	static dheader_t	outheader;
	
	header = &outheader;
	Mem_Set( header, 0, sizeof( dheader_t ));
	
	SwapBSPFile( true );

	header->ident = LittleLong( IDBSPMODHEADER );
	header->version = LittleLong( BSPMOD_VERSION );
	
	MsgDev( D_NOTE, "\n\nwriting %s.bsp\n", gs_filename );
	if( pe ) pe->FreeBSP();
	
	wadfile = FS_Open( va( "maps/%s.bsp", gs_filename ), "wb" );
	FS_Write( wadfile, header, sizeof( dheader_t ));	// overwritten later

	AddLump( LUMP_ENTITIES, dentdata, entdatasize );
	AddLump( LUMP_PLANES, dplanes, numplanes * sizeof( dplanes[0] ));
	AddLump( LUMP_VERTEXES, dvertexes, numvertexes * sizeof( dvertexes[0] ));
	AddLump( LUMP_VISIBILITY, dvisdata, visdatasize );
	AddLump( LUMP_NODES, dnodes, numnodes * sizeof( dnodes[0] ));
	AddLump( LUMP_TEXINFO, texinfo, numtexinfo * sizeof( texinfo[0] ));
	AddLump( LUMP_SURFACES, dsurfaces, numsurfaces * sizeof( dsurfaces[0] ));
	AddLump( LUMP_LIGHTING, dlightdata, lightdatasize );
	AddLump( LUMP_LEAFS, dleafs, numleafs * sizeof( dleafs[0] ));
	AddLump( LUMP_LEAFFACES, dleafsurfaces, numleafsurfaces * sizeof( dleafsurfaces[0] ));
	AddLump( LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes * sizeof( dleafbrushes[0] ));
	AddLump( LUMP_EDGES, dedges, numedges * sizeof( dedges[0] ));
	AddLump( LUMP_SURFEDGES, dsurfedges, numsurfedges * sizeof( dsurfedges[0] ));
	AddLump( LUMP_MODELS, dmodels, nummodels * sizeof( dmodels[0] ));
	AddLump( LUMP_BRUSHES, dbrushes, numbrushes * sizeof( dbrushes[0] ));
	AddLump( LUMP_BRUSHSIDES, dbrushsides, numbrushsides * sizeof( dbrushsides[0] ));
	AddLump( LUMP_COLLISION, dcollision, dcollisiondatasize );
	AddLump( LUMP_SHADERS, dshaders, numshaders * sizeof( dshaders[0] ));
	AddLump( LUMP_AREAS, dareas, numareas * sizeof( dareas[0] ));
	AddLump( LUMP_AREAPORTALS, dareaportals, numareaportals * sizeof( dareaportals[0] ));

	// merge header
	FS_Seek( wadfile, 0, SEEK_SET );
	FS_Write( wadfile, header, sizeof( dheader_t ));
	FS_Close( wadfile );
}

//============================================
//	misc parse functions
//============================================

void StripTrailing( char *e )
{
	char	*s;

	s = e + com.strlen( e ) - 1;
	while( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair( token_t *token )
{
	epair_t	*e;

	e = Malloc( sizeof( epair_t ));
	
	if( com.strlen( token->string ) >= MAX_KEY - 1 )
		Sys_Break( "ParseEpair: token too long\n" );
	e->key = copystring( token->string );
	Com_ReadToken( mapfile, SC_PARSE_GENERIC, token );
	if( com.strlen( token->string ) >= MAX_VALUE - 1 )
		Sys_Break( "ParseEpair: token too long\n" );
	e->value = copystring( token->string );

	// strip trailing spaces
	StripTrailing( e->key );
	StripTrailing( e->value );

	return e;
}

/*
================
ParseEntity
================
*/
bool ParseEntity( void )
{
	epair_t		*e;
	token_t		token;
	bsp_entity_t	*mapent;

	if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token ))
		return false;

	if( com.stricmp( token.string, "{" )) Sys_Break( "ParseEntity: '{' not found\n" );
	if( num_entities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit excceded\n" );

	mapent = &entities[num_entities];
	num_entities++;

	while( 1 )
	{
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			Sys_Break( "ParseEntity: EOF without closing brace\n" );
		if( !com.stricmp( token.string, "}" )) break;
		e = ParseEpair( &token );
		e->next = mapent->epairs;
		mapent->epairs = e;
	}
	return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities( void )
{
	num_entities = 0;
	mapfile = Com_OpenScript( "entities", dentdata, entdatasize );
	if( mapfile ) while( ParseEntity( ));
	Com_CloseScript( mapfile );
}

/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities( void )
{
	epair_t		*ep;
	char		*buf, *end;
	char		line[2048];
	char		key[MAX_KEY], value[MAX_VALUE];
	const char	*value2;
	int		i;

	buf = dentdata;
	end = buf;
	*end = 0;
	
	for( i = 0; i < num_entities; i++ )
	{
		ep = entities[i].epairs;
		if( !ep ) continue;	// ent got removed

		// certain entities get stripped from bsp file */
		value2 = ValueForKey( &entities[i], "classname" );
		if(!com.stricmp( value2, "_decal" ) || !com.stricmp( value2, "_skybox" ))
			continue;
		
		com.strcat( end, "{\n" );
		end += 2;
				
		for( ep = entities[i].epairs; ep; ep = ep->next )
		{
			com.strncpy( key, ep->key, MAX_KEY );
			StripTrailing( key );
			com.strncpy( value, ep->value, MAX_VALUE );
			StripTrailing( value );
				
			com.snprintf( line, 2048, "\"%s\" \"%s\"\n", key, value );
			com.strcat( end, line );
			end += com.strlen( line );
		}
		com.strcat( end, "}\n" );
		end += 2;

		if( end > buf + MAX_MAP_ENTSTRING )
			Sys_Break( "Entity text too long\n" );
	}
	entdatasize = end - buf + 1;
}

void SetKeyValue( bsp_entity_t *ent, const char *key, const char *value )
{
	epair_t	*ep;
	
	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if(!com.strcmp( ep->key, key ))
		{
			Mem_Free( ep->value );
			ep->value = copystring( value );
			return;
		}
	}
	ep = Malloc( sizeof( *ep ));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring( key );
	ep->value = copystring( value );
}

char *ValueForKey( const bsp_entity_t *ent, const char *key )
{
	epair_t	*ep;

	if( !ent ) return "";
	
	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if(!com.strcmp( ep->key, key ))
			return ep->value;
	}
	return "";
}

long IntForKey( const bsp_entity_t *ent, const char *key )
{
	char	*k;
	
	k = ValueForKey( ent, key );
	return com.atoi( k );
}

vec_t FloatForKey( const bsp_entity_t *ent, const char *key )
{
	char	*k;
	
	k = ValueForKey( ent, key );
	return com.atof( k );
}

void GetVectorForKey( const bsp_entity_t *ent, const char *key, vec3_t vec )
{
	char	*k;
	double	v1, v2, v3;

	k = ValueForKey( ent, key );

	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	VectorSet( vec, v1, v2, v3 );
}

/*
================
FindTargetEntity

finds an entity target
================
*/
bsp_entity_t *FindTargetEntity( const char *target )
{
	int		i;
	const char	*n;

	
	// walk entity list
	for( i = 0; i < num_entities; i++ )
	{
		n = ValueForKey( &entities[i], "targetname" );
		if( !com.strcmp( n, target ))
			return &entities[i];
	}
	return NULL;
}

void Com_CheckToken( script_t *script, const char *match )
{
	token_t	token;
	
	Com_ReadToken( script, SC_ALLOW_NEWLINES, &token );

	if( com.stricmp( token.string, match ))
	{
		Sys_Break( "Com_CheckToken: \"%s\" not found\n", match );
	}
}

void Com_Parse1DMatrix( script_t *script, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( script, "(" );
	for( i = 0; i < x; i++ )
		Com_ReadFloat( script, false, &m[i] );
	Com_CheckToken( script, ")" );
}

void Com_Parse2DMatrix( script_t *script, int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( script, "(" );
	for( i = 0; i < y; i++ )
		Com_Parse1DMatrix( script, x, m+i*x );
	Com_CheckToken( script, ")" );
}