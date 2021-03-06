//=======================================================================
//			Copyright XashXT Group 2007 ?
//		         mathlib.h - base math functions
//=======================================================================
#ifndef BASEMATH_H
#define BASEMATH_H

#include <math.h>

// euler angle order
#define PITCH		0
#define YAW		1
#define ROLL		2

#ifndef M_PI
#define M_PI		(float)3.14159265358979323846
#endif

#ifndef M_PI2
#define M_PI2		(float)6.28318530717958647692
#endif

#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON		2
#define SIDE_CROSS		-2

#define PLANE_X		0	// 0 - 2 are axial planes
#define PLANE_Y		1	// 3 needs alternate calc
#define PLANE_Z		2
#define PLANE_NONAXIAL	3

#define METERS_PER_INCH	0.0254f
#define EQUAL_EPSILON	0.001f
#define STOP_EPSILON	0.1f
#define ON_EPSILON		0.1f

#define RAD2DEG( x )	((float)(x) * (float)(180.f / M_PI))
#define DEG2RAD( x )	((float)(x) * (float)(M_PI / 180.f))
#define METER2INCH(x)	(float)(x * (1.0f/METERS_PER_INCH))
#define INCH2METER(x)	(float)(x * (METERS_PER_INCH/1.0f))
#define RAD_TO_STUDIO	(32768.0 / M_PI)
#define STUDIO_TO_RAD	(M_PI / 32768.0)
#define nanmask		(255<<23)

#define Q_rint(x)		((x) < 0 ? ((int)((x)-0.5f)) : ((int)((x)+0.5f)))
#define IS_NAN(x)		(((*(int *)&x)&nanmask)==nanmask)
#define RANDOM_LONG(MIN, MAX)	((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))
#define RANDOM_FLOAT(MIN,MAX)	(((float)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))
#define NUMVERTEXNORMALS	162	// quake avertex normals

#define VectorIsNAN(v) (IS_NAN(v[0]) || IS_NAN(v[1]) || IS_NAN(v[2]))	
#define VectorToPhysic(v) { v[0] = INCH2METER(v[0]), v[1] = INCH2METER(v[1]), v[2] = INCH2METER(v[2]); }
#define VectorToServer(v) { v[0] = METER2INCH(v[0]), v[1] = METER2INCH(v[1]), v[2] = METER2INCH(v[2]); }
#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define Vector4Subtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define Vector2Copy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1])
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define Vector4Copy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define Vector4Scale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale),(out)[3] = (in)[3] * (scale))
#define VectorMultiply(a,b,c) ((c)[0]=(a)[0]*(b)[0],(c)[1]=(a)[1]*(b)[1],(c)[2]=(a)[2]*(b)[2])
#define VectorCompare(v1,v2)	((v1)[0]==(v2)[0] && (v1)[1]==(v2)[1] && (v1)[2]==(v2)[2])
#define VectorDivide( in, d, out ) VectorScale( in, (1.0f / (d)), out )
#define VectorMax(a) ( max((a)[0], max((a)[1], (a)[2])) )
#define VectorAvg(a) ( ((a)[0] + (a)[1] + (a)[2]) / 3 )
#define VectorLength(a) (com.sqrt( DotProduct( a, a )))
#define VectorLength2(a) (DotProduct( a, a ))
#define VectorDistance(a, b) (com.sqrt( VectorDistance2( a, b )))
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorAverage(a,b,o)	((o)[0]=((a)[0]+(b)[0])*0.5,(o)[1]=((a)[1]+(b)[1])*0.5,(o)[2]=((a)[2]+(b)[2])*0.5)
#define Vector2Set(v, x, y) ((v)[0]=(x),(v)[1]=(y))
#define VectorSet(v, x, y, z) ((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define Vector4Set(v, a, b, c, d) ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3] = (d))
#define VectorClear(x) ((x)[0]=(x)[1]=(x)[2]=0)
#define Vector4Clear(x) ((x)[0]=(x)[1]=(x)[2]=(x)[3]=0)
#define VectorLerp( v1, lerp, v2, c ) ((c)[0] = (v1)[0] + (lerp) * ((v2)[0] - (v1)[0]), (c)[1] = (v1)[1] + (lerp) * ((v2)[1] - (v1)[1]), (c)[2] = (v1)[2] + (lerp) * ((v2)[2] - (v1)[2]))
#define VectorNormalize( v ) { float ilength = (float)com.sqrt(DotProduct(v, v));if (ilength) ilength = 1.0f / ilength;v[0] *= ilength;v[1] *= ilength;v[2] *= ilength; }
#define VectorNormalize2( v, dest ) {float ilength = (float)com.sqrt(DotProduct(v,v));if (ilength) ilength = 1.0f / ilength;dest[0] = v[0] * ilength;dest[1] = v[1] * ilength;dest[2] = v[2] * ilength; }
#define VectorNormalizeFast( v ) {float	ilength = (float)rsqrt(DotProduct(v,v)); v[0] *= ilength; v[1] *= ilength; v[2] *= ilength; }
#define VectorNegate(x, y) {y[0] =-x[0]; y[1]=-x[1]; y[2]=-x[2];}
#define VectorM(scale1, b1, c) ((c)[0] = (scale1) * (b1)[0],(c)[1] = (scale1) * (b1)[1],(c)[2] = (scale1) * (b1)[2])
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorMAM(scale1, b1, scale2, b2, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2])
#define VectorMAMAM(scale1, b1, scale2, b2, scale3, b3, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2])
#define VectorMAMAMAM(scale1, b1, scale2, b2, scale3, b3, scale4, b4, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0] + (scale4) * (b4)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1] + (scale4) * (b4)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2] + (scale4) * (b4)[2])
#define VectorReflect( a, r, b, c ) do{ double d; d = DotProduct((a), (b)) * -(1.0 + (r)); VectorMA((a), (d), (b), (c)); } while( 0 )
#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])
#define BoxInsideBox(a,b,c,d) ((a)[0] >= (c)[0] && (b)[0] <= (d)[0] && (a)[1] >= (c)[1] && (b)[1] <= (d)[1] && (a)[2] >= (c)[2] && (b)[2] <= (d)[2])
#define TriangleOverlapsBox( a, b, c, d, e ) (min((a)[0], min((b)[0], (c)[0])) < (e)[0] && max((a)[0], max((b)[0], (c)[0])) > (d)[0] && min((a)[1], min((b)[1], (c)[1])) < (e)[1] && max((a)[1], max((b)[1], (c)[1])) > (d)[1] && min((a)[2], min((b)[2], (c)[2])) < (e)[2] && max((a)[2], max((b)[2], (c)[2])) > (d)[2])
#define TriangleNormal( a, b, c, n) ((n)[0] = ((a)[1] - (b)[1]) * ((c)[2] - (b)[2]) - ((a)[2] - (b)[2]) * ((c)[1] - (b)[1]), (n)[1] = ((a)[2] - (b)[2]) * ((c)[0] - (b)[0]) - ((a)[0] - (b)[0]) * ((c)[2] - (b)[2]), (n)[2] = ((a)[0] - (b)[0]) * ((c)[1] - (b)[1]) - ((a)[1] - (b)[1]) * ((c)[0] - (b)[0]))
#define MakeRGBA( out, x, y, z, w ) Vector4Set( out, x, y, z, w )
#define Square(x) ((x)*(x))
_inline float anglemod(const float a){ return(360.0/65536) * ((int)(a*(65536/360.0)) & 65535); }

// NOTE: this code contain bug, what may invoked infinity loop
_inline int nearest_pow( int size )
{
	int i = 2;

	while( 1 ) 
	{
		i <<= 1;
		if( size == i ) return i;
		if( size > i && size < (i <<1)) 
		{
			if( size >= ((i+(i<<1))/2))
				return i<<1;
			else return i;
		}
	}
}

/*
=================
rsqrt
=================
*/
_inline float rsqrt( float number )
{
	int	i;
	float	x, y;

	if( number == 0.0f )
		return 0.0f;

	x = number * 0.5f;
	i = *(int *)&number;	// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);	// what the fuck?
	y = *(float *)&i;
	y = y * (1.5f - (x * y * y));	// first iteration

	return y;
}

// remap a value in the range [A,B] to [C,D].
_inline float RemapVal( float val, float A, float B, float C, float D )
{
	return C + (D - C) * (val - A) / (B - A);
}

_inline float ApproachVal( float target, float value, float speed )
{
	float	delta = target - value;

	if( delta > speed )
		value += speed;
	else if( delta < -speed )
		value -= speed;
	else value = target;

	return value;
}

_inline static bool VectorCompareEpsilon( const vec3_t v1, const vec3_t v2, const float epsilon )
{
	vec3_t	d;

	VectorSubtract( v1, v2, d );
	d[0] = fabs( d[0] );
	d[1] = fabs( d[1] );
	d[2] = fabs( d[2] );

	if( d[0] > epsilon || d[1] > epsilon || d[2] > epsilon )
		return false;
	return true;
}


_inline void VectorBound( const float min, vec3_t v, const float max )
{
	v[0] = bound(min, v[0], max);
	v[1] = bound(min, v[1], max);
	v[2] = bound(min, v[2], max);
}

// FIXME: convert to #define
_inline float VectorNormalizeLength2( const vec3_t v, vec3_t out )
{
	float	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = com.sqrt( length );

	if( length )
	{
		ilength = 1.0f / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}
	else out[0] = out[1] = out[2] = 0.0f;

	return length;
}
#define VectorNormalizeLength( v )	VectorNormalizeLength2((v), (v))

_inline bool VectorIsNull( const vec3_t v )
{
	int i;
	float result = 0;

	if(!v) return true;
	for (i = 0; i< 3; i++) result += v[i];
	if(result != 0) return false;
	return true;		
}

_inline void CrossProduct( vec3_t v1, vec3_t v2, vec3_t cross )
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

_inline void ClearBounds( vec3_t mins, vec3_t maxs )
{
	// make bogus range
	mins[0] = mins[1] = mins[2] =  999999;
	maxs[0] = maxs[1] = maxs[2] = -999999;
}

_inline void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs )
{
	float	val;
	int	i;

	for( i = 0; i < 3; i++ )
	{
		val = v[i];
		if( val < mins[i] ) mins[i] = val;
		if( val > maxs[i] ) maxs[i] = val;
	}
}

_inline void VectorVectors( vec3_t forward, vec3_t right, vec3_t up )
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	VectorMA(right, -d, forward, right);
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

_inline void VectorAngles( const vec3_t forward, vec3_t angles )
{
	float	tmp, yaw, pitch;
	
	if( forward[1] == 0 && forward[0] == 0 )
	{
		yaw = 0.0f;
		if( forward[2] > 0 )
			pitch = 270.0f;
		else pitch = 90.0f;
	}
	else
	{
		yaw = ( atan2( forward[1], forward[0] ) * 180 / M_PI );
		if( yaw < 0 ) yaw += 360;

		tmp = com.sqrt( forward[0] * forward[0] + forward[1] * forward[1] );
		pitch = ( atan2( -forward[2], tmp ) * 180 / M_PI );
		if( pitch < 0 ) pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

// similar to MakeNormalVectors but for rotational matrices
// (FIXME: weird, what's the diff between this and VectorVectors?)
_inline void NormalVectorToAxis( const vec3_t forward, vec3_t axis[3] )
{
	VectorCopy( forward, axis[0] );
	if( forward[0] || forward[1] )
	{
		VectorSet( axis[1], forward[1], -forward[0], 0 );
		VectorNormalize( axis[1] );
		CrossProduct( axis[0], axis[1], axis[2] );
	}
	else
	{
		VectorSet( axis[1], 1, 0, 0 );
		VectorSet( axis[2], 0, 1, 0 );
	}
}

_inline void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float	angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	com.sincos( angle, &sy, &cy );
	angle = angles[PITCH] * (M_PI*2 / 360);
	com.sincos( angle, &sp, &cp );
	if( forward )
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if( right || up )
	{
		if( angles[ROLL] )
		{
			angle = angles[ROLL] * (M_PI*2 / 360);
			com.sincos( angle, &sr, &cr );
			if( right )
			{
				right[0] = -1*(sr*sp*cy+cr*-sy);
				right[1] = -1*(sr*sp*sy+cr*cy);
				right[2] = -1*(sr*cp);
			}
			if( up )
			{
				up[0] = (cr*sp*cy+-sr*-sy);
				up[1] = (cr*sp*sy+-sr*cy);
				up[2] = cr*cp;
			}
		}
		else
		{
			if( right )
			{
				right[0] = sy;
				right[1] = -cy;
				right[2] = 0;
			}
			if( up )
			{
				up[0] = (sp*cy);
				up[1] = (sp*sy);
				up[2] = cp;
			}
		}
	}
}

_inline void AngleVectorsFLU(const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up)
{
	float	angle, sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	com.sincos( angle, &sy, &cy );
	angle = angles[PITCH] * (M_PI*2 / 360);
	com.sincos( angle, &sp, &cp );

	if( forward )
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if( left || up )
	{
		if( angles[ROLL] )
		{
			angle = angles[ROLL] * (M_PI*2 / 360);
			com.sincos( angle, &sr, &cr );
			if( left )
			{
				left[0] = sr*sp*cy+cr*-sy;
				left[1] = sr*sp*sy+cr*cy;
				left[2] = sr*cp;
			}
			if( up )
			{
				up[0] = cr*sp*cy+-sr*-sy;
				up[1] = cr*sp*sy+-sr*cy;
				up[2] = cr*cp;
			}
		}
		else
		{
			if( left )
			{
				left[0] = -sy;
				left[1] = cy;
				left[2] = 0;
			}
			if( up )
			{
				up[0] = sp*cy;
				up[1] = sp*sy;
				up[2] = cp;
			}
		}
	}
}

// FIXME: get rid of this
_inline void MatrixAngles( matrix3x3 matrix, vec3_t angles )
{ 
	vec3_t		forward, right, up;
	float		xyDist;

	forward[0] = matrix[0][0];
	forward[1] = matrix[0][2];
	forward[2] = matrix[0][1];
	right[0] = matrix[1][0];
	right[1] = matrix[1][2];
	right[2] = matrix[1][1];
	up[2] = matrix[2][1];
	
	xyDist = com.sqrt( forward[0] * forward[0] + forward[1] * forward[1] );
	
	if ( xyDist > EQUAL_EPSILON )	// enough here to get angles?
	{
		angles[1] = RAD2DEG( com.atan2( forward[1], forward[0] ));
		angles[0] = RAD2DEG( com.atan2( -forward[2], xyDist ));
		angles[2] = RAD2DEG( com.atan2( -right[2], up[2] ));
	}
	else
	{
		angles[1] = RAD2DEG( com.atan2( right[0], -right[1] ) );
		angles[0] = RAD2DEG( com.atan2( -forward[2], xyDist ) );
		angles[2] = 0;
	}
}

/*
====================
AngleQuaternion

====================
*/
_inline void AngleQuaternion( float *angles, vec4_t q )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles[2] * 0.5;
	com.sincos( angle, &sy, &cy );
	angle = angles[1] * 0.5;
	com.sincos( angle, &sp, &cp );
	angle = angles[0] * 0.5;
	com.sincos( angle, &sr, &cr );

	q[0] = sr*cp*cy-cr*sp*sy; // X
	q[1] = cr*sp*cy+sr*cp*sy; // Y
	q[2] = cr*cp*sy-sr*sp*cy; // Z
	q[3] = cr*cp*cy+sr*sp*sy; // W
}

/*
====================
QuaternionSlerp

====================
*/
_inline void QuaternionSlerp( vec4_t p, vec4_t q, float t, vec4_t qt )
{
	int i;
	float	omega, cosom, sinom, sclp, sclq;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;

	for (i = 0; i < 4; i++)
	{
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ((1.0 + cosom) > 0.000001)
	{
		if ((1.0 - cosom) > 0.000001)
		{
			omega = com.acos( cosom );
			sinom = com.sin( omega );
			sclp = com.sin( (1.0 - t)*omega) / sinom;
			sclq = com.sin( t*omega ) / sinom;
		}
		else
		{
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = com.sin( (1.0 - t) * (0.5 * M_PI));
		sclq = com.sin( t * (0.5 * M_PI));
		for (i = 0; i < 3; i++)
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

/*
=================
BoundsIntersect
=================
*/
_inline bool BoundsIntersect( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2 )
{
	if( mins1[0] > maxs2[0] || mins1[1] > maxs2[1] || mins1[2] > maxs2[2] )
		return false;
	if( maxs1[0] < mins2[0] || maxs1[1] < mins2[1] || maxs1[2] < mins2[2] )
		return false;
	return true;
}

/*
=================
BoundsAndSphereIntersect
=================
*/
_inline bool BoundsAndSphereIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t origin, float radius )
{
	if( mins[0] > origin[0] + radius || mins[1] > origin[1] + radius || mins[2] > origin[2] + radius )
		return false;
	if( maxs[0] < origin[0] - radius || maxs[1] < origin[1] - radius || maxs[2] < origin[2] - radius )
		return false;
	return true;
}

_inline bool PlaneIntersect( vec3_t p_n, vec_t p_d, vec3_t l_o, vec3_t l_n, vec3_t out )
{
	float	dot, t;

	dot = DotProduct( p_n, l_n );

	if( dot > -0.001 )
		return false;

	t = (p_d - (l_o[0] * p_n[0]) - (l_o[1] * p_n[1]) - (l_o[2] * p_n[2])) / dot;

	if( out )
	{
		out[0] = l_o[0] + (t * l_n[0]);
		out[1] = l_o[1] + (t * l_n[1]);
		out[2] = l_o[2] + (t * l_n[2]);
	}                
	return true;
}

_inline static int PlaneTypeForNormal( const vec3_t normal )
{
	if( normal[0] == 1.0 ) return PLANE_X;
	if( normal[1] == 1.0 ) return PLANE_Y;
	if( normal[2] == 1.0 ) return PLANE_Z;
	return PLANE_NONAXIAL;
}

/*
=================
SignbitsForPlane

fast box on planeside test
=================
*/
_inline static int SignbitsForPlane( const vec3_t normal )
{
	int	bits, i;

	for( bits = i = 0; i < 3; i++ )
		if( normal[i] < 0.0f ) bits |= 1<<i;
	return bits;
}

#define PlaneDist(point,plane)  ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

#define NUM_HULL_ROUNDS	(sizeof(hull_table) / sizeof(word))
#define HULL_PRECISION	4
static word hull_table[] = { 0, 4, 8, 16, 18, 24, 28, 30, 32, 40, 48, 54, 56, 60, 64, 72, 80, 112, 120, 128, 140, 176 };

_inline void CM_RoundUpHullSize( vec3_t size )
{
          int	i, j;
	
	for(i = 0; i < 3; i++)
	{
		bool negative = false;
                    float result, value;

		value = ceil(size[i] + 0.5f); // round it
		if(value < 0) negative = true;
		value = fabs( value ); // make positive

		// lookup hull table
		for(j = 0; j < NUM_HULL_ROUNDS; j++)
          	{
			result = value - hull_table[j];
			if(result <= HULL_PRECISION)
			{ 
				result = negative ? -hull_table[j] : hull_table[j];
				break;
			}
		}
		size[i] = result;	// copy new value
	}
}

/*
=================
RadiusFromBounds
=================
*/
_inline float RadiusFromBounds( vec3_t mins, vec3_t maxs )
{
	int	i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}
	return VectorLength( corner );
}

/*
====================
RotatePointAroundVector
====================
*/
_inline void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float	t0, t1;
	float	angle, c, s;
	vec3_t	vr, vu, vf;

	angle = DEG2RAD( degrees );
	com.sincos( angle, &s, &c );
	VectorCopy( dir, vf );
	VectorVectors( vf, vr, vu );

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

// assumes "src" is normalized
_inline void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	// LordHavoc: optimized to death and beyond
	int	pos;
	float	minelem;

	if( src[0] )
	{
		dst[0] = 0;
		if( src[1] )
		{
			dst[1] = 0;
			if( src[2] )
			{
				dst[2] = 0;
				pos = 0;
				minelem = fabs( src[0] );
				if( fabs(src[1]) < minelem )
				{
					pos = 1;
					minelem = fabs( src[1] );
				}
				if( fabs( src[2]) < minelem )
					pos = 2;

				dst[pos] = 1;
				dst[0] -= src[pos] * src[0];
				dst[1] -= src[pos] * src[1];
				dst[2] -= src[pos] * src[2];

				// normalize the result
				VectorNormalize( dst );
			}
			else dst[2] = 1;
		}
		else
		{
			dst[1] = 1;
			dst[2] = 0;
		}
	}
	else
	{
		dst[0] = 1;
		dst[1] = 0;
		dst[2] = 0;
	}
}

/*
=================
SimpleSpline

NOTE: ripped from hl2 source
hermite basis function for smooth interpolation
Similar to Gain() above, but very cheap to call
value should be between 0 & 1 inclusive
=================
*/
_inline float SimpleSpline( float value )
{
	float	valueSquared = value * value;

	// nice little ease-in, ease-out spline-like curve
	return (3 * valueSquared - 2 * valueSquared * value);
}


/*
=================
NearestPOW
=================
*/
_inline int NearestPOW( int value, bool roundDown )
{
	int	n = 1;

	if( value <= 0 ) return 1;
	while( n < value ) n <<= 1;

	if( roundDown )
	{
		if( n > value ) n >>= 1;
	}
	return n;
}

static vec3_t vec3_origin = { 0, 0, 0 };

#endif//BASEMATH_H