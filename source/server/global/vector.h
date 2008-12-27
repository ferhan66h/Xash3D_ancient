//=======================================================================
//			Copyright (C) Shambler Team 2005
//		         vector.h - shared vector operations 
//=======================================================================
#ifndef VECTOR_H
#define VECTOR_H

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>

#pragma warning(disable : 4244)		// int or float down-conversion

#ifdef CLIENT_DLL
			// Header file containing definition of globalvars_t and entvars_t
typedef int   func_t;	//
typedef int string_t;	// from engine's pr_comp.h;
typedef float  vec_t;	// needed before including progdefs.h
#endif

//=========================================================
// 2DVector - used for many pathfinding and many other 
// operations that are treated as planar rather than 3d.
//=========================================================
class Vector2D
{
public:
	inline Vector2D(void) { }
	inline Vector2D(float X, float Y) { x = X; y = Y; }
	inline Vector2D operator+(const Vector2D& v) const { return Vector2D(x+v.x, y+v.y); }
	inline Vector2D operator-(const Vector2D& v) const { return Vector2D(x-v.x, y-v.y); }
	inline Vector2D operator*(float fl) const { return Vector2D(x*fl, y*fl); }
	inline Vector2D operator/(float fl) const { return Vector2D(x/fl, y/fl); }
	
#ifdef CLIENT_DLL
	inline float Length(void) const { return (float)sqrt(x*x + y*y ); }
#else
	inline float Length(void) const { return sqrt(x*x + y*y ); }
#endif
	inline Vector2D Normalize ( void ) const
	{
		Vector2D vec2;

		float flLen = Length();
		if ( flLen == 0 )
		{
#ifdef CLIENT_DLL
			return Vector2D( (float)0, (float)0 );
#else
			return Vector2D( 0, 0 );
#endif
		}
		else
		{
			flLen = 1 / flLen;
			return Vector2D( x * flLen, y * flLen );
		}
	}
	vec_t	x, y;
};

#define nanmask		(255<<23)
#define IS_NAN(x)		(((*(int *)&x)&nanmask)==nanmask)

inline float DotProduct(const Vector2D& a, const Vector2D& b) { return( a.x*b.x + a.y*b.y ); }
inline Vector2D operator*(float fl, const Vector2D& v)	{ return v * fl; }

//=========================================================
// 3D Vector
//=========================================================
class Vector						// same data-layout as engine's vec3_t,
{								//		which is a vec_t[3]
public:
	// Construction/destruction
	inline Vector(void)				{ }
	inline Vector(float X, float Y, float Z)	{ x = X; y = Y; z = Z;						}
	inline Vector(const Vector& v)		{ x = v.x; y = v.y; z = v.z;		   } 
	inline Vector(float rgfl[3])			{ x = rgfl[0]; y = rgfl[1]; z = rgfl[2];   }

	// Initialization
	void Init(vec_t ix=0.0f, vec_t iy=0.0f, vec_t iz=0.0f){ x = ix; y = iy; z = iz; }

	// Operators
	inline Vector operator-(void) const		{ return Vector(-x,-y,-z);		   }
	inline int operator==(const Vector& v) const	{ return x==v.x && y==v.y && z==v.z;	   }
	inline int operator!=(const Vector& v) const	{ return !(*this==v);		   }
	inline Vector operator+(const Vector& v) const	{ return Vector(x+v.x, y+v.y, z+v.z);	   }
	inline Vector operator-(const Vector& v) const	{ return Vector(x-v.x, y-v.y, z-v.z);	   }
	inline Vector operator*(float fl) const		{ return Vector(x*fl, y*fl, z*fl);	   }
	inline Vector operator/(float fl) const		{ return Vector(x/fl, y/fl, z/fl);	   }

	_forceinline Vector&	operator+=(const Vector &v)
	{
		x+=v.x; y+=v.y; z += v.z;	
		return *this;
	}			
	_forceinline Vector&	operator-=(const Vector &v)
	{
		x-=v.x; y-=v.y; z -= v.z;	
		return *this;
	}		
	_forceinline Vector&	operator*=(const Vector &v)			
	{
		x *= v.x; y *= v.y; z *= v.z;
		return *this;
	}
	_forceinline Vector&	operator*=(float s)
	{
		x *= s; y *= s; z *= s;
		return *this;
	}
	_forceinline Vector&	operator/=(const Vector &v)
	{
		x /= v.x; y /= v.y; z /= v.z;
		return *this;
	}		
	_forceinline Vector&	operator/=(float s)
	{
		float oofl = 1.0f / s;
		x *= oofl; y *= oofl; z *= oofl;
		return *this;
	}
#ifndef CLIENT_DLL
	_forceinline Vector& fixangle(void)
	{
		if(!IS_NAN(x))
		{
			while ( x < 0 ) x += 360;
			while ( x > 360 ) x -= 360;
		}
		if(!IS_NAN(y))
		{
			while ( y < 0 ) y += 360;
			while ( y > 360 ) y -= 360;
		}
		if(!IS_NAN(z))
		{
			while ( z < 0 ) z += 360;
			while ( z > 360 ) z -= 360;
		}
		return *this;
	}
#endif
	_forceinline Vector MA(  float scale, const Vector &start, const Vector &direction ) const
	{
		return Vector(start.x + scale * direction.x, start.y + scale * direction.y, start.z + scale * direction.z) ;
	}
	
	// Methods
	inline void CopyToArray(float* rgfl) const	{ rgfl[0] = x, rgfl[1] = y, rgfl[2] = z;   }
#ifdef CLIENT_DLL
	inline float Length(void) const		{ return (float)sqrt(x*x + y*y + z*z); }
#else
	inline float Length(void) const		{ return sqrt(x*x + y*y + z*z); }
#endif
	operator float *()				{ return &x; } // Vectors will now automatically convert to float * when needed
	operator const float *() const		{ return &x; } // Vectors will now automatically convert to float * when needed

	// array access...
	vec_t operator[](int i) const { return ((vec_t*)this)[i];}
	vec_t& operator[](int i)  { return ((vec_t*)this)[i];}
	
	inline Vector Normalize(void) const
	{
		float flLen = Length();
		if (flLen == 0) return Vector(0,0,1); // ????
		flLen = 1 / flLen;
		return Vector(x * flLen, y * flLen, z * flLen);
	}
	vec_t	Dot(Vector const& vOther) const
          {
          	return(x*vOther.x+y*vOther.y+z*vOther.z);
          }
	Vector	Cross(const Vector &vOther) const
	{
		return Vector(y*vOther.z - z*vOther.y, z*vOther.x - x*vOther.z, x*vOther.y - y*vOther.x);
	}
	inline Vector2D Make2D ( void ) const
	{
		Vector2D	Vec2;
		Vec2.x = x;
		Vec2.y = y;
		return Vec2;
	}
#ifdef CLIENT_DLL
	inline float Length2D(void) const { return (float)sqrt(x*x + y*y); }
#else
	inline float Length2D(void) const { return sqrt(x*x + y*y); }
#endif
	// Members
	vec_t x, y, z;
};
inline Vector operator*(float fl, const Vector& v)	{ return v * fl; }
inline float DotProduct(const Vector& a, const Vector& b) { return(a.x*b.x+a.y*b.y+a.z*b.z); }
inline Vector CrossProduct(const Vector& a, const Vector& b) { return Vector( a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x ); }
#define vec3_t Vector

//=========================================================
// 4D Vector - for matrix operations
//=========================================================
class Vector4D					
{
public:
	// Members
	vec_t x, y, z, w;

	// Construction/destruction
	Vector4D(void){}
	Vector4D(vec_t X, vec_t Y, vec_t Z, vec_t W) { x = X; y = Y; z = Z; w = W;}
	Vector4D(double X, double Y, double Z, double W) { x = (double)X; y = (double)Y; z = (double)Z; w = (double)W;}
	Vector4D(const float *pFloat) { x = pFloat[0]; y = pFloat[1]; z = pFloat[2]; w = pFloat[3];}
        
	// array access...
	vec_t operator[](int i) const { return ((vec_t*)this)[i];}
	vec_t& operator[](int i)  { return ((vec_t*)this)[i];}

	// equality
	bool operator==(const Vector4D& src) const{ return(src.x == x) && (src.y == y) && (src.z == z) && (src.w == w);}
	bool operator!=(const Vector4D& src) const{ return(src.x != x) || (src.y != y) || (src.z != z) || (src.w != w);}	

	// arithmetic operations
	Vector4D&	operator+=(const Vector4D &v){ x+=v.x; y+=v.y; z += v.z; return *this;}			
	Vector4D&	operator-=(const Vector4D &v){ x-=v.x; y-=v.y; z -= v.z; return *this;}		
	Vector4D	operator+ (const Vector4D &v)const {Vector4D res; res.x = x + v.x; res.y = y + v.y; res.z = z + v.z; res.w = w; return res;}
	Vector4D	operator- (const Vector4D &v)const {Vector4D res; res.x = x - v.x; res.y = y - v.y; res.z = z - v.z; res.w = w; return res;}
	Vector4D	operator* (const Vector4D &v)const {Vector4D res; res.x = y * v.z - z * v.y; res.y = z * v.x - x * v.z; res.z = x * v.y - y * v.x; res.w = w; return res;}
	float	operator% (const Vector4D &v)const { return (x * v.x + y * v.y + z * v.z); }
	Vector4D Scale( float scale)const {Vector4D res; res.x = x * scale; res.y = y * scale; res.z = z * scale; res.w = w;return res; } 
	Vector4D CompProduct (const Vector4D &v)const {Vector4D res; res.x = x * v.x; res.y = y * v.y; res.z = z * v.z; res.w = w; return res;}
};

//=========================================================
// RandomRange - for random values
//=========================================================
class RandomRange
{
public:
	float m_flMax, m_flMin;//class members

	RandomRange() { m_flMin = m_flMax = 0; }
	RandomRange(float fValue) { m_flMin = m_flMax = fValue; }
	RandomRange(float fMin, float fMax) { m_flMin = fMin; m_flMax = fMax; }
	RandomRange( char *szToken )
	{
		char *cOneDot = NULL;
		m_flMin = m_flMax = 0;
	
		for (char *c = szToken; *c; c++)
		{
			if (*c == '.')
			{
				if (cOneDot != NULL)
				{
					// found two dots in a row - it's a range

					*cOneDot = 0; // null terminate the first number
					m_flMin = atof(szToken); // parse the first number
					*cOneDot = '.'; // change it back, just in case
					c++;
					m_flMax = atof(c); // parse the second number
					return;
				}
				else	cOneDot = c;
			}
			else cOneDot = NULL;
		}

		// no range, just record the number
		m_flMax = m_flMin = atof(szToken);
          }
	
	// FIXME: float Random() { return RANDOM_FLOAT(m_flMin, m_flMax); }
	float Random() { return m_flMin - m_flMax; }

	// array access...
	float operator[](int i) const { return ((float*)this)[i];}
	float& operator[](int i)  { return ((float*)this)[i];}
};

#endif