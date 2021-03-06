//=======================================================================
//			Copyright XashXT Group 2009 ?
//			cm_trace.c - geometry tracing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"
#include "matrix_lib.h"

#define MAX_POSITION_LEAFS		1024
#define RADIUS_EPSILON		1.0f

// always use bbox vs. bbox collision and never capsule vs. bbox or vice versa
// #define ALWAYS_BBOX_VS_BBOX
// always use capsule vs. capsule collision and never capsule vs. bbox or vice versa
// #define ALWAYS_CAPSULE_VS_CAPSULE

// #define CAPSULE_DEBUG

/*
===============================================================================

BASIC MATH

===============================================================================
*/

/*
================
CM_ProjectPointOntoVector
================
*/
void CM_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vDir, vec3_t vProj )
{
	vec3_t	pVec;

	VectorSubtract( point, vStart, pVec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vDir ), vDir, vProj );
}

/*
================
CM_DistanceFromLineSquared
================
*/
float CM_DistanceFromLineSquared( vec3_t p, vec3_t lp1, vec3_t lp2, vec3_t dir )
{
	vec3_t	proj, t;
	int	j;

	CM_ProjectPointOntoVector( p, lp1, dir, proj );

	for( j = 0; j < 3; j++ )
	{
		if(( proj[j] > lp1[j] && proj[j] > lp2[j] ) || ( proj[j] < lp1[j] && proj[j] < lp2[j] ))
			break;
	}

	if( j < 3 )
	{
		if( fabs( proj[j] - lp1[j] ) < fabs( proj[j] - lp2[j] ))
			VectorSubtract( p, lp1, t );
		else VectorSubtract( p, lp2, t );
		return VectorLength2( t );
	}
	VectorSubtract( p, proj, t );

	return VectorLength2( t );
}

/*
================
SquareRootFloat
================
*/
float SquareRootFloat( float number )
{
	union
	{
		float	f;
		int	i;
	} t;

	float		x, y;
	const float	f = 1.5F;

	x = number * 0.5F;
	t.f = number;
	t.i = 0x5f3759df - (t.i >> 1);
	y = t.f;
	y = y * (f - (x * y * y));
	y = y * (f - (x * y * y));

	return number * y;
}


/*
===============================================================================

POSITION TESTING

===============================================================================
*/

/*
================
CM_TestBoxInBrush
================
*/
static void CM_TestBoxInBrush( traceWork_t *tw, cbrush_t *brush )
{
	int		i;
	cplane_t		*plane;
	float		t, d1, dist;
	cbrushside_t	*side;
	vec3_t		startp;

	if( !brush->numsides )
		return;

	// special test for axial
	if( !BoundsIntersect( tw->bounds[0], tw->bounds[1], brush->bounds[0], brush->bounds[1] ))
		return;

	if( tw->type == TR_CAPSULE )
	{
		// the first six planes are the axial planes, so we only
		// need to test the remainder
		for( i = 6; i < brush->numsides; i++ )
		{
			side = brush->sides + i;
			plane = side->plane;

			// adjust the plane distance apropriately for radius
			dist = plane->dist + tw->sphere.radius;
			// find the closest point on the capsule to the plane
			t = DotProduct( plane->normal, tw->sphere.offset );
			if( t > 0 ) VectorSubtract( tw->start, tw->sphere.offset, startp );
			else VectorAdd( tw->start, tw->sphere.offset, startp );
			d1 = DotProduct( startp, plane->normal ) - dist;

			// if completely in front of face, no intersection
			if( d1 > 0 ) return;
		}
	}
	else
	{
		// the first six planes are the axial planes, so we only
		// need to test the remainder
		for( i = 6; i < brush->numsides; i++ )
		{
			side = brush->sides + i;
			plane = side->plane;

			// adjust the plane distance apropriately for mins/maxs
			dist = plane->dist - DotProduct( tw->offsets[plane->signbits], plane->normal );

			d1 = DotProduct( tw->start, plane->normal ) - dist;

			// if completely in front of face, no intersection
			if( d1 > 0 ) return;
		}
	}

	// inside this brush
	tw->trace.fStartSolid = tw->trace.fAllSolid = true;
	tw->trace.flFraction = 0.0f;
	tw->trace.iContents = brush->contents;
}


/*
====================
CM_PositionTestInSurfaceCollide
====================
*/
static bool CM_PositionTestInSurfaceCollide( traceWork_t *tw, const cSurfaceCollide_t *sc )
{
	int		i, j;
	float		offset, t;
	cmplane_t		*planes;
	cfacet_t		*facet;
	float		plane[4];
	vec3_t		startp;

	if( tw->isPoint )
		return false;

	//
	facet = sc->facets;
	for( i = 0; i < sc->numFacets; i++, facet++ )
	{
		planes = &sc->planes[facet->surfacePlane];
		VectorCopy( planes->plane, plane );
		plane[3] = planes->plane[3];

		if( tw->type == TR_CAPSULE )
		{
			// adjust the plane distance apropriately for radius
			plane[3] += tw->sphere.radius;

			// find the closest point on the capsule to the plane
			t = DotProduct( plane, tw->sphere.offset );
			if( t > 0 ) VectorSubtract( tw->start, tw->sphere.offset, startp );
			else VectorAdd( tw->start, tw->sphere.offset, startp );
		}
		else
		{
			offset = DotProduct(tw->offsets[planes->signbits], plane);
			plane[3] -= offset;
			VectorCopy(tw->start, startp);
		}

		if( DotProduct( plane, startp ) - plane[3] > 0.0f )
			continue;

		for( j = 0; j < facet->numBorders; j++ )
		{
			planes = &sc->planes[facet->borderPlanes[j]];

			if( facet->borderInward[j] )
			{
				VectorNegate( planes->plane, plane );
				plane[3] = -planes->plane[3];
			}
			else
			{
				VectorCopy( planes->plane, plane );
				plane[3] = planes->plane[3];
			}

			if( tw->type == TR_CAPSULE )
			{
				// adjust the plane distance apropriately for radius
				plane[3] += tw->sphere.radius;

				// find the closest point on the capsule to the plane
				t = DotProduct( plane, tw->sphere.offset );
				if( t > 0.0f ) VectorSubtract( tw->start, tw->sphere.offset, startp );
				else VectorAdd( tw->start, tw->sphere.offset, startp );
			}
			else
			{
				// NOTE: this works even though the plane might be flipped because the bbox is centered
				offset = DotProduct( tw->offsets[planes->signbits], plane );
				plane[3] += fabs( offset );
				VectorCopy( tw->start, startp );
			}

			if( DotProduct( plane, startp ) - plane[3] > 0.0f )
				break;
		}

		if( j < facet->numBorders ) continue;
		// inside this patch facet
		return true;
	}
	return false;
}

/*
================
CM_TestInLeaf
================
*/
void CM_TestInLeaf( traceWork_t *tw, cleaf_t *leaf )
{
	int		k, brushnum;
	csurface_t	*surface;
	cbrush_t		*b;

	// test box position against all brushes in the leaf
	for( k = 0; k < leaf->numleafbrushes; k++ )
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];
		b = &cm.brushes[brushnum];
		if( b->checkcount == cms.checkcount )
			continue; // already checked this brush in another leaf

		b->checkcount = cms.checkcount;

		if(!( b->contents & tw->contents ))
			continue;

		CM_TestBoxInBrush( tw, b );
		if( tw->trace.fAllSolid )
			return;
	}

	// test against all surfaces
	for( k = 0; k < leaf->numleafsurfaces; k++ )
	{
		surface = cm.surfaces[cm.leafsurfaces[leaf->firstleafsurface + k]];

		if( !surface ) continue;

		if( surface->checkcount == cms.checkcount )
			continue;	// already checked this surface in another leaf

		surface->checkcount = cms.checkcount;

		if(!( surface->contents & tw->contents ))
			continue;

		if( !cm_nocurves->integer )
		{
			if( surface->type == MST_PATCH && surface->sc && CM_PositionTestInSurfaceCollide( tw, surface->sc ))
			{
				tw->trace.fStartSolid = tw->trace.fAllSolid = true;
				tw->trace.flFraction = 0;
				tw->trace.iContents = surface->contents;
				return;
			}
		}

		if( !cm_nomeshes->integer )
		{
			if( surface->type == MST_TRISURF && surface->sc && CM_PositionTestInSurfaceCollide( tw, surface->sc ))
			{
				tw->trace.fStartSolid = tw->trace.fAllSolid = true;
				tw->trace.flFraction = 0;
				tw->trace.iContents = surface->contents;
				return;
			}
		}
	}
}


/*
==================
CM_TestCapsuleInCapsule

capsule inside capsule check
==================
*/
void CM_TestCapsuleInCapsule( traceWork_t *tw, model_t model )
{
	int		i;
	vec3_t		mins, maxs;
	vec3_t		top, bottom;
	vec3_t		p1, p2, tmp;
	vec3_t		offset, symetricSize[2];
	float		radius, halfwidth, halfheight, offs, r;

	CM_ModelBounds( model, mins, maxs );

	VectorAdd( tw->start, tw->sphere.offset, top );
	VectorSubtract( tw->start, tw->sphere.offset, bottom );

	for( i = 0; i < 3; i++ )
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5f;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
	}

	halfwidth = symetricSize[1][0];
	halfheight = symetricSize[1][2];
	radius = (halfwidth > halfheight) ? halfheight : halfwidth;
	offs = halfheight - radius;

	r = Square( tw->sphere.radius + radius );
	// check if any of the spheres overlap
	VectorCopy( offset, p1 );
	p1[2] += offs;
	VectorSubtract( p1, top, tmp );

	if( VectorLength2( tmp ) < r )
	{
		tw->trace.fStartSolid = tw->trace.fAllSolid = true;
		tw->trace.flFraction = 0;
	}
	VectorSubtract(p1, bottom, tmp);
	if(VectorLength2(tmp) < r )
	{
		tw->trace.fStartSolid = tw->trace.fAllSolid = true;
		tw->trace.flFraction = 0;
	}

	VectorCopy( offset, p2 );
	p2[2] -= offs;
	VectorSubtract( p2, top, tmp );

	if( VectorLength2( tmp ) < r )
	{
		tw->trace.fStartSolid = tw->trace.fAllSolid = true;
		tw->trace.flFraction = 0;
	}

	VectorSubtract( p2, bottom, tmp );

	if( VectorLength2( tmp ) < r )
	{
		tw->trace.fStartSolid = tw->trace.fAllSolid = true;
		tw->trace.flFraction = 0;
	}

	// if between cylinder up and lower bounds
	if(( top[2] >= p1[2] && top[2] <= p2[2]) || (bottom[2] >= p1[2] && bottom[2] <= p2[2] ))
	{
		// 2d coordinates
		top[2] = p1[2] = 0;

		// if the cylinders overlap
		VectorSubtract( top, p1, tmp );
		if( VectorLength2( tmp ) < r )
		{
			tw->trace.fStartSolid = tw->trace.fAllSolid = true;
			tw->trace.flFraction = 0;
		}
	}
}

/*
==================
CM_TestBoundingBoxInCapsule

bounding box inside capsule check
==================
*/
void CM_TestBoundingBoxInCapsule( traceWork_t *tw, model_t model )
{
	vec3_t		mins, maxs, offset, size[2];
	cmodel_t		*cmod;
	model_t		h;
	int		i;

	// mins maxs of the capsule
	CM_ModelBounds( model, mins, maxs );

	// offset for capsule center
	for( i = 0; i < 3; i++ )
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5f;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
		tw->start[i] -= offset[i];
		tw->end[i] -= offset[i];
	}

	// replace the bounding box with the capsule
	tw->type = TR_CAPSULE;
	tw->sphere.radius = (size[1][0] > size[1][2]) ? size[1][2] : size[1][0];
	tw->sphere.halfheight = size[1][2];
	VectorSet( tw->sphere.offset, 0, 0, size[1][2] - tw->sphere.radius );

	// replace the capsule with the bounding box
	h = CM_TempBoxModel( tw->size[0], tw->size[1], false );

	// calculate collision
	cmod = CM_ClipHandleToModel( h );
	CM_TestInLeaf( tw, &cmod->leaf );
}

/*
==================
CM_PositionTest
==================
*/
void CM_PositionTest( traceWork_t *tw )
{
	int             leafs[MAX_POSITION_LEAFS];
	leaflist_t      ll;
	int             i;

	// identify the leafs we are touching
	VectorAdd( tw->start, tw->size[0], ll.bounds[0] );
	VectorAdd( tw->start, tw->size[1], ll.bounds[1] );

	for( i = 0; i < 3; i++ )
	{
		ll.bounds[0][i] -= 1;
		ll.bounds[1][i] += 1;
	}

	ll.count = 0;
	ll.maxcount = MAX_POSITION_LEAFS;
	ll.list = leafs;
	ll.lastleaf = 0;
	ll.overflowed = false;

	cms.checkcount++;
	CM_BoxLeafnums_r( &ll, 0 );
	cms.checkcount++;

	// test the contents of the leafs
	for( i = 0; i < ll.count; i++ )
	{
		CM_TestInLeaf( tw, &cm.leafs[leafs[i]] );
		if( tw->trace.fAllSolid ) break;
	}
}

/*
===============================================================================

TRACING

===============================================================================
*/
/*
====================
CM_TracePointThroughSurfaceCollide

special case for point traces because the surface collide "brushes" have no volume
====================
*/
void CM_TracePointThroughSurfaceCollide( traceWork_t *tw, const cSurfaceCollide_t *sc )
{
	static bool	frontFacing[SHADER_MAX_TRIANGLES];
	static float	intersection[SHADER_MAX_TRIANGLES];
	float		intersect;
	const cmplane_t	*planes;
	const cfacet_t	*facet;
	int		i, j, k;
	float		offset;
	float		d1, d2;

	if( !tw->isPoint ) return;

	// determine the trace's relationship to all planes
	planes = sc->planes;
	for( i = 0; i < sc->numPlanes; i++, planes++ )
	{
		offset = DotProduct( tw->offsets[planes->signbits], planes->plane );
		d1 = DotProduct( tw->start, planes->plane ) - planes->plane[3] + offset;
		d2 = DotProduct( tw->end, planes->plane ) - planes->plane[3] + offset;

		if( d1 <= 0 ) frontFacing[i] = false;
		else frontFacing[i] = true;

		if( d1 == d2 ) intersection[i] = 99999;
		else
		{
			intersection[i] = d1 / (d1 - d2);
			if( intersection[i] <= 0 ) intersection[i] = 99999;
		}
	}

	// see if any of the surface planes are intersected
	for( i = 0, facet = sc->facets; i < sc->numFacets; i++, facet++ )
	{
		if( !frontFacing[facet->surfacePlane] )
			continue;

		intersect = intersection[facet->surfacePlane];
		if( intersect < 0.0f )
			continue;	// surface is behind the starting point

		if( intersect > tw->trace.flFraction )
			continue; // already hit something closer

		for( j = 0; j < facet->numBorders; j++ )
		{
			k = facet->borderPlanes[j];
			if( frontFacing[k] ^ facet->borderInward[j] )
			{
				if( intersection[k] > intersect )
					break;
			}
			else
			{
				if( intersection[k] < intersect )
					break;
			}
		}

		if( j == facet->numBorders )
		{
			// we hit this facet
			debugSurfaceCollide = sc;
			debugFacet = facet;

			planes = &sc->planes[facet->surfacePlane];

			// calculate intersection with a slight pushoff
			offset = DotProduct( tw->offsets[planes->signbits], planes->plane );
			d1 = DotProduct( tw->start, planes->plane ) - planes->plane[3] + offset;
			d2 = DotProduct( tw->end, planes->plane ) - planes->plane[3] + offset;
			tw->trace.flFraction = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);

			if( tw->trace.flFraction < 0 )
				tw->trace.flFraction = 0;

			VectorCopy( planes->plane, tw->trace.vecPlaneNormal );
			tw->trace.flPlaneDist = planes->plane[3];
		}
	}
}

/*
====================
CM_CheckFacetPlane
====================
*/
int CM_CheckFacetPlane( float *plane, vec3_t start, vec3_t end, float *enterFrac, float *leaveFrac, int *hit )
{
	float	d1, d2, f;

	*hit = false;

	d1 = DotProduct( start, plane ) - plane[3];
	d2 = DotProduct( end, plane ) - plane[3];

	// if completely in front of face, no intersection with the entire facet
	if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ))
		return false;

	// if it doesn't cross the plane, the plane isn't relevant
	if( d1 <= 0 && d2 <= 0 )
		return true;

	// crosses face
	if( d1 > d2 )
	{
		// enter
		f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
		if( f < 0 ) f = 0;

		// always favor previous plane hits and thus also the surface plane hit
		if( f > *enterFrac )
		{
			*enterFrac = f;
			*hit = true;
		}
	}
	else
	{
		// leave
		f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
		if( f > 1 ) f = 1;

		if( f < *leaveFrac ) *leaveFrac = f;
	}

	return true;
}

/*
====================
CM_TraceThroughSurfaceCollide
====================
*/
void CM_TraceThroughSurfaceCollide( traceWork_t *tw, const cSurfaceCollide_t *sc )
{
	int		i, j, hit, hitnum;
	float		offset, enterFrac, leaveFrac, t;
	cmplane_t		*planes;
	cfacet_t		*facet;
	float		plane[4] = { 0, 0, 0, 0 };
	float		bestplane[4] = { 0, 0, 0, 0 };
	vec3_t		startp, endp;

	if(!CM_BoundsIntersect( tw->bounds[0], tw->bounds[1], sc->bounds[0], sc->bounds[1] ))
		return;

	if( tw->isPoint )
	{
		CM_TracePointThroughSurfaceCollide( tw, sc );
		return;
	}

	for( i = 0, facet = sc->facets; i < sc->numFacets; i++, facet++ )
	{
		enterFrac = -1.0;
		leaveFrac = 1.0;
		hitnum = -1;

		planes = &sc->planes[facet->surfacePlane];
		VectorCopy(planes->plane, plane);
		plane[3] = planes->plane[3];

		if( tw->type == TR_CAPSULE )
		{
			// adjust the plane distance appropriately for radius
			plane[3] += tw->sphere.radius;

			// find the closest point on the capsule to the plane
			t = DotProduct( plane, tw->sphere.offset );

			if( t > 0.0f )
			{
				VectorSubtract( tw->start, tw->sphere.offset, startp );
				VectorSubtract( tw->end, tw->sphere.offset, endp );
			}
			else
			{
				VectorAdd( tw->start, tw->sphere.offset, startp );
				VectorAdd( tw->end, tw->sphere.offset, endp );
			}
		}
		else
		{
			offset = DotProduct( tw->offsets[planes->signbits], plane );
			plane[3] -= offset;
			VectorCopy( tw->start, startp );
			VectorCopy( tw->end, endp );
		}

		if( !CM_CheckFacetPlane( plane, startp, endp, &enterFrac, &leaveFrac, &hit ))
			continue;

		if( hit ) Vector4Copy( plane, bestplane );

		for( j = 0; j < facet->numBorders; j++ )
		{
			planes = &sc->planes[facet->borderPlanes[j]];

			if( facet->borderInward[j] )
			{
				VectorNegate( planes->plane, plane );
				plane[3] = -planes->plane[3];
			}
			else
			{
				VectorCopy( planes->plane, plane );
				plane[3] = planes->plane[3];
			}

			if( tw->type == TR_CAPSULE )
			{
				// adjust the plane distance apropriately for radius
				plane[3] += tw->sphere.radius;

				// find the closest point on the capsule to the plane
				t = DotProduct( plane, tw->sphere.offset );
				if( t > 0.0f )
				{
					VectorSubtract( tw->start, tw->sphere.offset, startp );
					VectorSubtract( tw->end, tw->sphere.offset, endp );
				}
				else
				{
					VectorAdd( tw->start, tw->sphere.offset, startp );
					VectorAdd( tw->end, tw->sphere.offset, endp );
				}
			}
			else
			{
				// NOTE: this works even though the plane might be flipped because the bbox is centered
				offset = DotProduct( tw->offsets[planes->signbits], plane );
				plane[3] += fabs( offset );
				VectorCopy( tw->start, startp );
				VectorCopy( tw->end, endp );
			}

			if(!CM_CheckFacetPlane( plane, startp, endp, &enterFrac, &leaveFrac, &hit ))
				break;

			if( hit )
			{
				hitnum = j;
				Vector4Copy( plane, bestplane );
			}
		}

		if( j < facet->numBorders )
			continue;

		// never clip against the back side
		if( hitnum == facet->numBorders - 1 )
			continue;

		if( enterFrac < leaveFrac && enterFrac >= 0 )
		{
			if( enterFrac < tw->trace.flFraction )
			{
				if( enterFrac < 0 )
					enterFrac = 0;

				debugSurfaceCollide = sc;
				debugFacet = facet;

				tw->trace.flFraction = enterFrac;
				VectorCopy( bestplane, tw->trace.vecPlaneNormal );
				tw->trace.flPlaneDist = bestplane[3];
			}
		}
	}
}

/*
================
CM_TraceThroughSurface
================
*/
void CM_TraceThroughSurface( traceWork_t *tw, csurface_t *surface )
{
	float	oldFrac;

	oldFrac = tw->trace.flFraction;

	if( !cm_nocurves->integer && surface->type == MST_PATCH && surface->sc )
	{
		CM_TraceThroughSurfaceCollide( tw, surface->sc );
	}

	if( !cm_nomeshes->integer && surface->type == MST_TRISURF && surface->sc )
	{
		CM_TraceThroughSurfaceCollide( tw, surface->sc );
	}

	if( tw->trace.flFraction < oldFrac )
	{
		tw->trace.pTexName = surface->name;
		tw->trace.iContents = surface->contents;
	}
}

/*
================
CM_TraceThroughBrush
================
*/
void CM_TraceThroughBrush( traceWork_t *tw, cbrush_t *brush )
{

	cplane_t		*plane, *clipplane;
	float		enterFrac, leaveFrac;
	float		dist, d1, d2, t, f;
	bool		getout, startout;
	cbrushside_t	*side, *leadside;
	vec3_t		startp, endp;
	int		i;

	enterFrac = -1.0;
	leaveFrac = 1.0;
	clipplane = NULL;

	if( !brush->numsides )
		return;

	getout = false;
	startout = false;

	leadside = NULL;

	if( tw->type == TR_BISPHERE )
	{
		// compare the trace against all planes of the brush
		// find the latest time the trace crosses a plane towards the interior
		// and the earliest time the trace crosses a plane towards the exterior
		for( i = 0; i < brush->numsides; i++ )
		{
			side = brush->sides + i;
			plane = side->plane;

			// adjust the plane distance apropriately for radius
			d1 = DotProduct( tw->start, plane->normal ) - ( plane->dist + tw->biSphere.startRadius );
			d2 = DotProduct( tw->end, plane->normal ) - ( plane->dist + tw->biSphere.endRadius );

			if( d2 > 0.0f ) getout = true; // endpoint is not in solid
			if( d1 > 0.0f ) startout = true;

			// if completely in front of face, no intersection with the entire brush
			if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ))
				return;

			// if it doesn't cross the plane, the plane isn't relevent
			if( d1 <= 0 && d2 <= 0 ) continue;

			brush->collided = true;

			// crosses face
			if( d1 > d2 )
			{
				// enter
				f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f < 0.0f ) f = 0.0f;
				if( f > enterFrac )
				{
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else
			{
				// leave
				f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f > 1.0f ) f = 1.0f;
				if( f < leaveFrac ) leaveFrac = f;
			}
		}
	}
	else if( tw->type == TR_CAPSULE )
	{
		// compare the trace against all planes of the brush
		// find the latest time the trace crosses a plane towards the interior
		// and the earliest time the trace crosses a plane towards the exterior
		for( i = 0; i < brush->numsides; i++ )
		{
			side = brush->sides + i;
			plane = side->plane;

			// adjust the plane distance appropriately for radius
			dist = plane->dist + tw->sphere.radius;

			// find the closest point on the capsule to the plane
			t = DotProduct( plane->normal, tw->sphere.offset );
			if( t > 0.0f )
			{
				VectorSubtract( tw->start, tw->sphere.offset, startp );
				VectorSubtract( tw->end, tw->sphere.offset, endp );
			}
			else
			{
				VectorAdd( tw->start, tw->sphere.offset, startp );
				VectorAdd( tw->end, tw->sphere.offset, endp );
			}

			d1 = DotProduct(startp, plane->normal) - dist;
			d2 = DotProduct(endp, plane->normal) - dist;

			if( d2 > 0.0f ) getout = true; // endpoint is not in solid
			if( d1 > 0.0f ) startout = true;

			// if completely in front of face, no intersection with the entire brush
			if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ))
				return;

			// if it doesn't cross the plane, the plane isn't relevant
			if( d1 <= 0 && d2 <= 0 ) continue;

			brush->collided = true;

			// crosses face
			if( d1 > d2 )
			{	
				// enter
				f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f < 0.0f ) f = 0.0f;
				if( f > enterFrac )
				{
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else
			{	
				// leave
				f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f > 1.0f ) f = 1.0f;
				if( f < leaveFrac ) leaveFrac = f;
			}
		}
	}
	else
	{
		// compare the trace against all planes of the brush
		// find the latest time the trace crosses a plane towards the interior
		// and the earliest time the trace crosses a plane towards the exterior
		for( i = 0; i < brush->numsides; i++ )
		{
			side = brush->sides + i;
			plane = side->plane;

			// adjust the plane distance appropriately for mins/maxs
			dist = plane->dist - DotProduct( tw->offsets[plane->signbits], plane->normal );

			d1 = DotProduct( tw->start, plane->normal ) - dist;
			d2 = DotProduct( tw->end, plane->normal ) - dist;

			if( d2 > 0.0f ) getout = true; // endpoint is not in solid
			if( d1 > 0.0f ) startout = true;

			// if completely in front of face, no intersection with the entire brush
			if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ))
				return;

			// if it doesn't cross the plane, the plane isn't relevant
			if( d1 <= 0 && d2 <= 0 ) continue;

			brush->collided = true;

			// crosses face
			if( d1 > d2 )
			{	
				// enter
				f = ( d1 - SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f < 0.0f ) f = 0.0f;
				if( f > enterFrac )
				{
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else
			{	
				// leave
				f = ( d1 + SURFACE_CLIP_EPSILON ) / ( d1 - d2 );
				if( f > 1.0f ) f = 1.0f;
				if( f < leaveFrac ) leaveFrac = f;
			}
		}
	}

	// all planes have been checked, and the trace was not
	// completely outside the brush
	if( !startout )
	{
		// original point was inside brush
		tw->trace.fStartSolid = true;
		if( !getout )
		{
			tw->trace.fAllSolid = true;
			tw->trace.flFraction = 0;
			tw->trace.iContents = brush->contents;
		}
		else
		{
			// set q1 trace flags
			if( brush->contents == BASECONT_NONE )
				tw->trace.fInOpen = true;
			else if( brush->contents & MASK_WATER )
				tw->trace.fInWater = true;
		}
		return;
	}

	if( enterFrac < leaveFrac )
	{
		if( enterFrac > -1 && enterFrac < tw->trace.flFraction )
		{
			if( enterFrac < 0.0f ) enterFrac = 0.0f;
			tw->trace.flFraction = enterFrac;
			VectorCopy( clipplane->normal, tw->trace.vecPlaneNormal );
			tw->trace.flPlaneDist = clipplane->dist;
			tw->trace.pTexName = cm.shaders[leadside->shadernum].name;
			tw->trace.iContents = brush->contents;
		}
	}
}

/*
================
CM_TraceThroughLeaf
================
*/
void CM_TraceThroughLeaf( traceWork_t *tw, cleaf_t *leaf )
{
	int		k;
	int		brushnum;
	csurface_t	*surface;
	cbrush_t		*b;

	// trace line against all brushes in the leaf
	for( k = 0; k < leaf->numleafbrushes; k++ )
	{
		brushnum = cm.leafbrushes[leaf->firstleafbrush + k];

		b = &cm.brushes[brushnum];
		if( b->checkcount == cms.checkcount )
			continue; // already checked this brush in another leaf

		b->checkcount = cms.checkcount;

		if(!( b->contents & tw->contents ))
			continue;
		b->collided = false;

		if( !CM_BoundsIntersect( tw->bounds[0], tw->bounds[1], b->bounds[0], b->bounds[1] ))
			continue;

		CM_TraceThroughBrush( tw, b );

		if( !tw->trace.flFraction )
			return;
	}

	// trace line against all surfaces in the leaf
	for( k = 0; k < leaf->numleafsurfaces; k++ )
	{
		surface = cm.surfaces[cm.leafsurfaces[leaf->firstleafsurface + k]];

		if( !surface ) continue;

		if( surface->checkcount == cms.checkcount )
			continue;	// already checked this surface in another leaf

		surface->checkcount = cms.checkcount;

		if(!( surface->contents & tw->contents ))
			continue;

		if( !CM_BoundsIntersect( tw->bounds[0], tw->bounds[1], surface->sc->bounds[0], surface->sc->bounds[1] ))
			continue;

		CM_TraceThroughSurface( tw, surface );

		if( !tw->trace.flFraction )
			return;
	}
}

/*
================
CM_TraceThroughSphere

get the first intersection of the ray with the sphere
================
*/
void CM_TraceThroughSphere( traceWork_t *tw, vec3_t origin, float radius, vec3_t start, vec3_t end )
{
	float	l1, l2, length, scale, fraction;
	float	a, b, c, d, sqrtd;
	vec3_t	v1, dir, intersection;

	// if inside the sphere
	VectorSubtract( start, origin, dir );
	l1 = VectorLength2( dir );

	if( l1 < Square( radius ))
	{
		tw->trace.flFraction = 0.0f;
		tw->trace.fStartSolid = true;

		// test for allsolid
		VectorSubtract( end, origin, dir );
		l1 = VectorLength2( dir );
		if( l1 < Square( radius ))
			tw->trace.fAllSolid = true;
		return;
	}

	VectorSubtract( end, start, dir );
	length = VectorNormalizeLength( dir );

	l1 = CM_DistanceFromLineSquared( origin, start, end, dir );
	VectorSubtract( end, origin, v1 );
	l2 = VectorLength2( v1 );

	// if no intersection with the sphere and the end point is at least an epsilon away
	if( l1 >= Square( radius ) && l2 > Square( radius + SURFACE_CLIP_EPSILON ))
		return;

	//  | origin - (start + t * dir) | = radius
	//  a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	//  b = 2 * (dir[0] * (start[0] - origin[0]) + dir[1] * (start[1] - origin[1]) + dir[2] * (start[2] - origin[2]));
	//  c = (start[0] - origin[0])^2 + (start[1] - origin[1])^2 + (start[2] - origin[2])^2 - radius^2;

	VectorSubtract( start, origin, v1 );

	// dir is normalized so a = 1
	a = 1.0f;	// dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
	b = 2.0f * (dir[0] * v1[0] + dir[1] * v1[1] + dir[2] * v1[2]);
	c = v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2] - (radius + RADIUS_EPSILON) * (radius + RADIUS_EPSILON);

	d = b * b - 4.0f * c; // * a;

	if( d > 0.0f )
	{
		sqrtd = SquareRootFloat( d );
		// = (- b + sqrtd) * 0.5f; // / (2.0f * a);
		fraction = ( -b - sqrtd ) * 0.5f; // / (2.0f * a);

		if( fraction < 0.0f ) fraction = 0.0f;
		else fraction /= length;

		if( fraction < tw->trace.flFraction )
		{
			tw->trace.flFraction = fraction;
			VectorSubtract( end, start, dir );
			VectorMA( start, fraction, dir, intersection );
			VectorSubtract( intersection, origin, dir );
			scale = 1.0f / ( radius + RADIUS_EPSILON );
			VectorScale( dir, scale, dir );
			VectorCopy( dir, tw->trace.vecPlaneNormal );
			VectorAdd( tw->modelOrigin, intersection, intersection );
			tw->trace.flPlaneDist = DotProduct( tw->trace.vecPlaneNormal, intersection );
			tw->trace.iContents = BASECONT_BODY;
		}
	}
	else if( d == 0.0f )
	{
		// t1 = (- b ) / 2;
		// slide along the sphere
	}
	// no intersection at all
}

/*
================
CM_TraceThroughVerticalCylinder

get the first intersection of the ray with the cylinder
the cylinder extends halfheight above and below the origin
================
*/
void CM_TraceThroughVerticalCylinder( traceWork_t *tw, vec3_t origin, float radius, float halfheight, vec3_t start, vec3_t end )
{
	float	length, scale, fraction, l1, l2;
	float	a, b, c, d, sqrtd;
	vec3_t	v1, dir, start2d, end2d, org2d, intersection;

	// 2d coordinates
	VectorSet( start2d, start[0], start[1], 0.0f );
	VectorSet( end2d, end[0], end[1], 0.0f );
	VectorSet( org2d, origin[0], origin[1], 0.0f );

	// if between lower and upper cylinder bounds
	if( start[2] <= origin[2] + halfheight && start[2] >= origin[2] - halfheight )
	{
		// if inside the cylinder
		VectorSubtract( start2d, org2d, dir );
		l1 = VectorLength2( dir );

		if( l1 < Square( radius ))
		{
			tw->trace.flFraction = 0;
			tw->trace.fStartSolid = true;
			VectorSubtract( end2d, org2d, dir );
			l1 = VectorLength2( dir );

			if( l1 < Square( radius ))
				tw->trace.fAllSolid = true;
			return;
		}
	}

	VectorSubtract( end2d, start2d, dir );
	length = VectorNormalizeLength( dir );
	l1 = CM_DistanceFromLineSquared( org2d, start2d, end2d, dir );
	VectorSubtract( end2d, org2d, v1 );
	l2 = VectorLength2( v1 );

	// if no intersection with the cylinder and the end point is at least an epsilon away
	if( l1 >= Square( radius ) && l2 > Square( radius + SURFACE_CLIP_EPSILON ))
		return;

	// (start[0] - origin[0] - t * dir[0]) ^ 2 + (start[1] - origin[1] - t * dir[1]) ^ 2 = radius ^ 2
	// (v1[0] + t * dir[0]) ^ 2 + (v1[1] + t * dir[1]) ^ 2 = radius ^ 2;
	// v1[0] ^ 2 + 2 * v1[0] * t * dir[0] + (t * dir[0]) ^ 2 +
	//                      v1[1] ^ 2 + 2 * v1[1] * t * dir[1] + (t * dir[1]) ^ 2 = radius ^ 2
	// t ^ 2 * (dir[0] ^ 2 + dir[1] ^ 2) + t * (2 * v1[0] * dir[0] + 2 * v1[1] * dir[1]) +
	//                      v1[0] ^ 2 + v1[1] ^ 2 - radius ^ 2 = 0

	VectorSubtract( start, origin, v1 );

	// dir is normalized so we can use a = 1
	a = 1.0f; // * (dir[0] * dir[0] + dir[1] * dir[1]);
	b = 2.0f * (v1[0] * dir[0] + v1[1] * dir[1]);
	c = v1[0] * v1[0] + v1[1] * v1[1] - (radius + RADIUS_EPSILON) * (radius + RADIUS_EPSILON);

	d = b * b - 4.0f * c; // * a;

	if( d > 0.0f )
	{
		sqrtd = SquareRootFloat( d );
		// = (- b + sqrtd) * 0.5f;// / (2.0f * a);
		fraction = (-b - sqrtd) * 0.5f; // / (2.0f * a);

		if( fraction < 0.0f ) fraction = 0.0f;
		else fraction /= length;

		if( fraction < tw->trace.flFraction )
		{
			VectorSubtract( end, start, dir );
			VectorMA( start, fraction, dir, intersection );

			// if the intersection is between the cylinder lower and upper bound
			if( intersection[2] <= origin[2] + halfheight && intersection[2] >= origin[2] - halfheight )
			{
				tw->trace.flFraction = fraction;
				VectorSubtract( intersection, origin, dir );
				dir[2] = 0;
				scale = 1 / ( radius + RADIUS_EPSILON );
				VectorScale( dir, scale, dir );
				VectorCopy( dir, tw->trace.vecPlaneNormal );
				VectorAdd( tw->modelOrigin, intersection, intersection );
				tw->trace.flPlaneDist = DotProduct( tw->trace.vecPlaneNormal, intersection );
				tw->trace.iContents = BASECONT_BODY;
			}
		}
	}
	else if( d == 0.0f )
	{
		// t[0] = (- b ) / 2 * a;
		// slide along the cylinder
	}
	// no intersection at all
}

/*
================
CM_TraceCapsuleThroughCapsule

capsule vs. capsule collision (not rotated)
================
*/
void CM_TraceCapsuleThroughCapsule( traceWork_t *tw, model_t model )
{
	int		i;
	vec3_t		mins, maxs;
	vec3_t		top, bottom, starttop, startbottom, endtop, endbottom;
	vec3_t		offset, symetricSize[2];
	float		radius, halfwidth, halfheight, offs, h;

	CM_ModelBounds( model, mins, maxs );

	// test trace bounds vs. capsule bounds
	if( tw->bounds[0][0] > maxs[0] + RADIUS_EPSILON || tw->bounds[0][1] > maxs[1] + RADIUS_EPSILON
	|| tw->bounds[0][2] > maxs[2] + RADIUS_EPSILON || tw->bounds[1][0] < mins[0] - RADIUS_EPSILON
	|| tw->bounds[1][1] < mins[1] - RADIUS_EPSILON || tw->bounds[1][2] < mins[2] - RADIUS_EPSILON )
	{
		return;
	}
	// top origin and bottom origin of each sphere at start and end of trace
	VectorAdd( tw->start, tw->sphere.offset, starttop );
	VectorSubtract( tw->start, tw->sphere.offset, startbottom );
	VectorAdd( tw->end, tw->sphere.offset, endtop );
	VectorSubtract( tw->end, tw->sphere.offset, endbottom );

	// calculate top and bottom of the capsule spheres to collide with
	for( i = 0; i < 3; i++ )
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5f;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
	}

	halfwidth = symetricSize[1][0];
	halfheight = symetricSize[1][2];
	radius = (halfwidth > halfheight) ? halfheight : halfwidth;
	offs = halfheight - radius;
	VectorCopy( offset, top );
	top[2] += offs;
	VectorCopy( offset, bottom );
	bottom[2] -= offs;

	// expand radius of spheres
	radius += tw->sphere.radius;

	// if there is horizontal movement
	if( tw->start[0] != tw->end[0] || tw->start[1] != tw->end[1] )
	{
		// height of the expanded cylinder is the height of both cylinders minus the radius of both spheres
		h = halfheight + tw->sphere.halfheight - radius;
		// if the cylinder has a height
		if( h > 0.0f )
		{
			// test for collisions between the cylinders
			CM_TraceThroughVerticalCylinder( tw, offset, radius, h, tw->start, tw->end );
		}
	}
	// test for collision between the spheres
	CM_TraceThroughSphere( tw, top, radius, startbottom, endbottom );
	CM_TraceThroughSphere( tw, bottom, radius, starttop, endtop );
}

/*
================
CM_TraceBoundingBoxThroughCapsule

bounding box vs. capsule collision
================
*/
void CM_TraceBoundingBoxThroughCapsule( traceWork_t *tw, model_t model )
{
	vec3_t	mins, maxs, offset, size[2];
	cmodel_t	*cmod;
	model_t	h;
	int	i;

	// mins maxs of the capsule
	CM_ModelBounds( model, mins, maxs );

	// offset for capsule center
	for( i = 0; i < 3; i++ )
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5f;
		size[0][i] = mins[i] - offset[i];
		size[1][i] = maxs[i] - offset[i];
		tw->start[i] -= offset[i];
		tw->end[i] -= offset[i];
	}

	// replace the bounding box with the capsule
	tw->type = TR_CAPSULE;
	tw->sphere.radius = (size[1][0] > size[1][2]) ? size[1][2] : size[1][0];
	tw->sphere.halfheight = size[1][2];
	VectorSet( tw->sphere.offset, 0, 0, size[1][2] - tw->sphere.radius );

	// replace the capsule with the bounding box
	h = CM_TempBoxModel( tw->size[0], tw->size[1], false );

	// calculate collision
	cmod = CM_ClipHandleToModel( h );
	CM_TraceThroughLeaf( tw, &cmod->leaf );
}

//=========================================================================================

/*
==================
CM_TraceThroughTree

Traverse all the contacted leafs from the start to the end position.
If the trace is a point, they will be exactly in order, but for larger
trace volumes it is possible to hit something in a later leaf with
a smaller intercept fraction.
==================
*/
static void CM_TraceThroughTree( traceWork_t *tw, int num, float p1f, float p2f, vec3_t p1, vec3_t p2 )
{
	cnode_t		*node;
	cplane_t		*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	vec3_t		mid;
	int		side;
	float		midf;

	if( tw->trace.flFraction <= p1f )
		return; // already hit something nearer

	// if < 0, we are in a leaf node
	if( num < 0.0f )
	{
		CM_TraceThroughLeaf( tw, &cm.leafs[-1 - num] );
		return;
	}

	// find the point distances to the seperating plane
	// and the offset for the size of the box
	node = cm.nodes + num;
	plane = node->plane;

	// adjust the plane distance apropriately for mins/maxs
	if( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = tw->extents[plane->type];
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
		if( tw->isPoint ) offset = 0;
		else offset = 2048; // FIXME: this is silly !!!
	}

	// see which sides we need to consider
	if( t1 >= offset + 1 && t2 >= offset + 1 )
	{
		CM_TraceThroughTree( tw, node->children[0], p1f, p2f, p1, p2 );
		return;
	}

	if( t1 < -offset - 1 && t2 < -offset - 1 )
	{
		CM_TraceThroughTree( tw, node->children[1], p1f, p2f, p1, p2 );
		return;
	}

	// put the crosspoint SURFACE_CLIP_EPSILON pixels on the near side
	if( t1 < t2 )
	{
		idist = 1.0f / (t1 - t2);
		side = 1;
		frac2 = ( t1 + offset + SURFACE_CLIP_EPSILON ) * idist;
		frac = ( t1 - offset + SURFACE_CLIP_EPSILON ) * idist;
	}
	else if( t1 > t2 )
	{
		idist = 1.0f / (t1 - t2);
		side = 0;
		frac2 = ( t1 - offset - SURFACE_CLIP_EPSILON ) * idist;
		frac = ( t1 + offset + SURFACE_CLIP_EPSILON ) * idist;
	}
	else
	{
		side = 0;
		frac = 1.0f;
		frac2 = 0.0f;
	}

	// move up to the node
	if( frac < 0.0f ) frac = 0.0f;
	if( frac > 1.0f ) frac = 1.0f;

	midf = p1f + (p2f - p1f) * frac;

	mid[0] = p1[0] + frac * (p2[0] - p1[0]);
	mid[1] = p1[1] + frac * (p2[1] - p1[1]);
	mid[2] = p1[2] + frac * (p2[2] - p1[2]);

	CM_TraceThroughTree( tw, node->children[side], p1f, midf, p1, mid );

	// go past the node
	if( frac2 < 0.0f ) frac2 = 0.0f;
	if( frac2 > 1.0f ) frac2 = 1.0f;

	midf = p1f + (p2f - p1f) * frac2;

	mid[0] = p1[0] + frac2 * (p2[0] - p1[0]);
	mid[1] = p1[1] + frac2 * (p2[1] - p1[1]);
	mid[2] = p1[2] + frac2 * (p2[2] - p1[2]);

	CM_TraceThroughTree( tw, node->children[side ^ 1], midf, p2f, mid, p2 );
}


//======================================================================


/*
==================
CM_Trace
==================
*/
static void CM_Trace( trace_t *tr, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, model_t model, const vec3_t origin, int mask, trType_t type, sphere_t *sphere )
{
	int		i;
	traceWork_t	tw;
	vec3_t		offset;
	cmodel_t		*cmod;

	cmod = CM_ClipHandleToModel( model );
	if( !cmod ) cmod = sv_models[1]; // force to world any invalid handle

	cms.checkcount++; // for multi-check avoidance

	// fill in a default trace
	Mem_Set( &tw, 0, sizeof( tw ));

	tw.trace.flFraction = 1.0f; // assume it goes the entire distance until shown otherwise
	VectorCopy( origin, tw.modelOrigin );
	tw.type = type;

	if( !cm.numnodes )
	{
		*tr = tw.trace;
		return; // map not loaded, shouldn't happen
	}

	// allow NULL to be passed in for 0,0,0
	if( !mins ) mins = vec3_origin;
	if( !maxs ) maxs = vec3_origin;

	// set basic parms
	tw.contents = mask;

	// adjust so that mins and maxs are always symetric, which
	// avoids some complications with plane expanding of rotated
	// bmodels
	for( i = 0; i < 3; i++ )
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5f;
		tw.size[0][i] = mins[i] - offset[i];
		tw.size[1][i] = maxs[i] - offset[i];
		tw.start[i] = start[i] + offset[i];
		tw.end[i] = end[i] + offset[i];
	}

	// if a sphere is already specified
	if( sphere ) tw.sphere = *sphere;
	else
	{
		tw.sphere.radius = (tw.size[1][0] > tw.size[1][2]) ? tw.size[1][2] : tw.size[1][0];
		tw.sphere.halfheight = tw.size[1][2];
		VectorSet( tw.sphere.offset, 0, 0, tw.size[1][2] - tw.sphere.radius );
	}

	tw.maxOffset = tw.size[1][0] + tw.size[1][1] + tw.size[1][2];

	// tw.offsets[signbits] = vector to apropriate corner from origin
	tw.offsets[0][0] = tw.size[0][0];
	tw.offsets[0][1] = tw.size[0][1];
	tw.offsets[0][2] = tw.size[0][2];

	tw.offsets[1][0] = tw.size[1][0];
	tw.offsets[1][1] = tw.size[0][1];
	tw.offsets[1][2] = tw.size[0][2];

	tw.offsets[2][0] = tw.size[0][0];
	tw.offsets[2][1] = tw.size[1][1];
	tw.offsets[2][2] = tw.size[0][2];

	tw.offsets[3][0] = tw.size[1][0];
	tw.offsets[3][1] = tw.size[1][1];
	tw.offsets[3][2] = tw.size[0][2];

	tw.offsets[4][0] = tw.size[0][0];
	tw.offsets[4][1] = tw.size[0][1];
	tw.offsets[4][2] = tw.size[1][2];

	tw.offsets[5][0] = tw.size[1][0];
	tw.offsets[5][1] = tw.size[0][1];
	tw.offsets[5][2] = tw.size[1][2];

	tw.offsets[6][0] = tw.size[0][0];
	tw.offsets[6][1] = tw.size[1][1];
	tw.offsets[6][2] = tw.size[1][2];

	tw.offsets[7][0] = tw.size[1][0];
	tw.offsets[7][1] = tw.size[1][1];
	tw.offsets[7][2] = tw.size[1][2];

	// calculate bounds
	if( tw.type == TR_CAPSULE )
	{
		for( i = 0; i < 3; i++ )
		{
			if( tw.start[i] < tw.end[i] )
			{
				tw.bounds[0][i] = tw.start[i] - fabs( tw.sphere.offset[i] ) - tw.sphere.radius;
				tw.bounds[1][i] = tw.end[i] + fabs( tw.sphere.offset[i] ) + tw.sphere.radius;
			}
			else
			{
				tw.bounds[0][i] = tw.end[i] - fabs( tw.sphere.offset[i] ) - tw.sphere.radius;
				tw.bounds[1][i] = tw.start[i] + fabs( tw.sphere.offset[i] ) + tw.sphere.radius;
			}
		}
	}
	else
	{
		for( i = 0; i < 3; i++ )
		{
			if( tw.start[i] < tw.end[i] )
			{
				tw.bounds[0][i] = tw.start[i] + tw.size[0][i];
				tw.bounds[1][i] = tw.end[i] + tw.size[1][i];
			}
			else
			{
				tw.bounds[0][i] = tw.end[i] + tw.size[0][i];
				tw.bounds[1][i] = tw.start[i] + tw.size[1][i];
			}
		}
	}

	// check for position test special case
	if( start[0] == end[0] && start[1] == end[1] && start[2] == end[2] )
	{
		if( model && cmod->type != mod_world )
		{
#ifdef ALWAYS_BBOX_VS_BBOX
			if( model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE )
			{
				tw.type = TR_AABB;
				CM_TestInLeaf( &tw, &cmod->leaf );
			}
			else
#elif defined(ALWAYS_CAPSULE_VS_CAPSULE)
			if( model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE )
			{
				CM_TestCapsuleInCapsule( &tw, model );
			}
			else
#endif
			if( model == CAPSULE_MODEL_HANDLE )
			{
				if( tw.type == TR_CAPSULE )
					CM_TestCapsuleInCapsule( &tw, model );
				else CM_TestBoundingBoxInCapsule( &tw, model );
			}
			else CM_TestInLeaf( &tw, &cmod->leaf );
		}
		else CM_PositionTest( &tw );
	}
	else
	{
		// check for point special case
		if( tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0 )
		{
			tw.isPoint = true;
			VectorClear( tw.extents );
		}
		else
		{
			tw.isPoint = false;
			tw.extents[0] = tw.size[1][0];
			tw.extents[1] = tw.size[1][1];
			tw.extents[2] = tw.size[1][2];
		}

		// general sweeping through world
		if( model && cmod->type != mod_world )
		{
#ifdef ALWAYS_BBOX_VS_BBOX
			if( model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE )
			{
				tw.type = TR_AABB;
				CM_TraceThroughLeaf( &tw, &cmod->leaf );
			}
			else
#elif defined(ALWAYS_CAPSULE_VS_CAPSULE)
			if( model == BOX_MODEL_HANDLE || model == CAPSULE_MODEL_HANDLE )
			{
				CM_TraceCapsuleThroughCapsule(&tw, model);
			}
			else
#endif
			if( model == CAPSULE_MODEL_HANDLE )
			{
				if( tw.type == TR_CAPSULE )
					CM_TraceCapsuleThroughCapsule( &tw, model );
				else CM_TraceBoundingBoxThroughCapsule( &tw, model );
			}
			else CM_TraceThroughLeaf( &tw, &cmod->leaf );
		}
		else CM_TraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end );
	}

	// generate endpos from the original, unmodified start/end
	if( tw.trace.flFraction == 1.0 ) VectorCopy( end, tw.trace.vecEndPos );
	else VectorLerp( start, tw.trace.flFraction, end, tw.trace.vecEndPos );

	// If allsolid is set (was entirely inside something solid), the plane is not valid.
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	*tr = tw.trace;
}

/*
==================
CM_BoxTrace
==================
*/
void CM_BoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, model_t model, int mask, trType_t type )
{
	CM_Trace( tr, start, end, mins, maxs, model, vec3_origin, mask, type, NULL );
}

/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
void CM_TransformedBoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, model_t model, int mask, const vec3_t origin, const vec3_t angles, trType_t type )
{
	trace_t		trace;
	vec3_t		start_l, end_l, offset;
	vec3_t		startRotated, endRotated;
	vec3_t		symetricSize[2];
	matrix4x4		rotation, transform, inverse;
	float		halfwidth, halfheight, t;
	bool		rotated;
	sphere_t		sphere;
	int		i;

	if( !mins ) mins = vec3_origin;
	if( !maxs ) maxs = vec3_origin;

	// adjust so that mins and maxs are always symmetric, which
	// avoids some complications with plane expanding of rotated
	// bmodels
	for( i = 0; i < 3; i++ )
	{
		offset[i] = (mins[i] + maxs[i]) * 0.5f;
		symetricSize[0][i] = mins[i] - offset[i];
		symetricSize[1][i] = maxs[i] - offset[i];
		start_l[i] = start[i] + offset[i];
		end_l[i] = end[i] + offset[i];
	}

	// rotate start and end into the models frame of reference
	if( model != BOX_MODEL_HANDLE && !VectorIsNull( angles ))
		rotated = true;
	else rotated = false;

	halfwidth = symetricSize[1][0];
	halfheight = symetricSize[1][2];

	sphere.radius = (halfwidth > halfheight) ? halfheight : halfwidth;
	sphere.halfheight = halfheight;
	t = halfheight - sphere.radius;

	if( rotated )
	{
		// rotation on trace line (start-end) instead of rotating the bmodel
		// NOTE: This is still incorrect for bounding boxes because the actual bounding
		//       box that is swept through the model is not rotated. We cannot rotate
		//       the bounding box or the bmodel because that would make all the brush
		//       bevels invalid.
		//       However this is correct for capsules since a capsule itself is rotated too.

		Matrix4x4_CreateFromEntity( rotation, 0.0f, 0.0f, 0.0f, angles[PITCH], angles[YAW], angles[ROLL], 1.0f );
		Matrix4x4_Copy( transform, rotation );
		Matrix4x4_SetOrigin( transform, origin[0], origin[1], origin[2] );
		Matrix4x4_Invert_Simple( inverse, transform );

		// transform trace line into the clipModel's space
		Matrix4x4_VectorTransform( inverse, start_l, startRotated );
		Matrix4x4_VectorTransform( inverse, end_l, endRotated );

		// extract up vector from the rotation matrix as rotated sphere offset for capsule
#ifdef OPENGL_STYLE
		sphere.offset[0] = rotation[2][0] * t;
		sphere.offset[1] = rotation[2][1] * t;
		sphere.offset[2] = rotation[2][2] * t;
#else
		sphere.offset[0] = rotation[0][2] * t;
		sphere.offset[1] = rotation[1][2] * t;
		sphere.offset[2] = rotation[2][2] * t;
#endif
	}
	else
	{
		VectorSubtract( start_l, origin, startRotated );
		VectorSubtract( end_l, origin, endRotated );
		VectorSet( sphere.offset, 0, 0, t );
	}

	// sweep the box through the model
	CM_Trace( &trace, startRotated, endRotated, symetricSize[0], symetricSize[1], model, origin, mask, type, &sphere );

	// if the bmodel was rotated and there was a collision
	if( rotated && trace.flFraction != 1.0f )
	{
		vec3_t	normal;	// untransformed normal

		VectorCopy( trace.vecPlaneNormal, normal );

		// Tr3B: we rotated our trace into the space of the clipModel
		// so we have to rotate the trace plane normal back to world space
		Matrix4x4_VectorRotate( rotation, normal, trace.vecPlaneNormal );
	}

	// re-calculate the end position of the trace because the trace.vecEndPos
	// calculated by CM_Trace could be rotated and have an offset
	VectorLerp( start, trace.flFraction, end, trace.vecEndPos );

	*tr = trace;
}