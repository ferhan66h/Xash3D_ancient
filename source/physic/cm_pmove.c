//=======================================================================
//			Copyright XashXT Group 2007 �
//		        cm_pmove.c - player movement code
//=======================================================================

#include "cm_local.h"

int		characterID; 
uint		m_jumpTimer;
bool		m_isStopped;
bool		m_isAirBorne;
float		m_maxStepHigh;
float		m_yawAngle;
float		m_maxTranslation;
vec3_t		m_size;
vec3_t		m_stepContact;
matrix4x4		m_matrix;


#define PM_SPEED		160.f
#define STEPSIZE		18
#define OVERCLIP		1.001f
#define MAX_CLIP_PLANES	5
#define JUMP_VELOCITY	270
#define MIN_WALK_NORMAL	0.7f		// can't walk on very steep slopes
#define DEFAULT_VIEWHEIGHT	26
#define CROUCH_VIEWHEIGHT	12
#define DEAD_VIEWHEIGHT	-16

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct
{
	vec3_t		forward, right, up;
	vec3_t		previous_origin;
	vec3_t		previous_velocity;

	vec3_t		movedir;	// already aligned by world
	int		previous_waterlevel;
	float		impact_speed;
	trace_t		groundtrace;
	bool		groundplane;
	float		frametime;

	// states
	bool		air_borne;
	bool		walking;
	bool		onladder;
} pml_t;

pmove_t		*pm;
pml_t		pml;


// movement parameters
float	pm_maxspeed = 300;
float	pm_duckspeed = 100;
float	pm_waterspeed = 400;

float	pm_stopspeed = 100.0f;
float	pm_duckscale = 0.25f;
float	pm_swimscale = 0.50f;
float	pm_wadescale = 0.70f;

float	pm_accelerate = 10.0f;
float	pm_airaccelerate = 0.0f;
float	pm_wateraccelerate = 10.0f;
float	pm_flyaccelerate = 8.0f;
float	pm_friction = 6.0f;
float	pm_waterfriction = 1.0f;
float	pm_flightfriction = 3.0f;

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/

/*
  walking up a step should kill some velocity
*/

void PM_SnapVector( float *v )
{
	int	i;
	float	f;

	f = *v;
	__asm fld	f;
	__asm fistp i;
	*v = i;
	v++;
	f = *v;
	__asm fld f;
	__asm fistp i;
	*v = i;
	v++;
	f = *v;
	__asm fld f;
	__asm fistp i;
	*v = i;
}

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt( edict_t *entity )
{
	int		i;

	if( pm->numtouch == PM_MAXTOUCH )
		return;

	// see if it is already added
	for ( i = 0; i < pm->numtouch; i++ )
	{
		if( pm->touchents[ i ] == entity )
			return;
	}

	// add it
	pm->touchents[pm->numtouch] = entity;
	pm->numtouch++;
}

/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce )
{
	float	backoff;
	float	change;
	int		i;
	
	backoff = DotProduct (in, normal);

	if( backoff < 0 ) backoff *= overbounce;
	else backoff /= overbounce;

	for( i = 0; i < 3; i++ )
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
	}
}

/*
==================
PM_SlideMove

Returns true if the velocity was clipped in some way
==================
*/
bool PM_SlideMove( bool gravity )
{
	int		bumpcount, numbumps = 4;
	vec3_t		dir;
	float		d;
	int		numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int		i, j, k;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	
	VectorCopy (pm->ps.velocity, primal_velocity );

	if( gravity )
	{
		VectorCopy( pm->ps.velocity, endVelocity );
		endVelocity[2] -= pm->ps.gravity * pml.frametime;
		pm->ps.velocity[2] = ( pm->ps.velocity[2] + endVelocity[2] ) * 0.5f;
		primal_velocity[2] = endVelocity[2];
		if( pml.groundplane )
		{
			// slide along the ground plane
			PM_ClipVelocity (pm->ps.velocity, pml.groundtrace.plane.normal, pm->ps.velocity, OVERCLIP );
		}
	}
	time_left = pml.frametime;

	// never turn against the ground plane
	if( pml.groundplane )
	{
		numplanes = 1;
		VectorCopy( pml.groundtrace.plane.normal, planes[0] );
	}
	else numplanes = 0;

	// never turn against original velocity
	VectorNormalize2( pm->ps.velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ )
	{

		// calculate position we are trying to move to
		VectorMA( pm->ps.origin, time_left, pm->ps.velocity, end );

		// see if we can make it there
		trace = pm->trace( pm->ps.origin, pm->mins, pm->maxs, end );

		if( trace.allsolid )
		{
			// entity is completely trapped in another solid
			pm->ps.velocity[2] = 0; // don't build up falling damage, but allow sideways acceleration
			return true;
		}

		if( trace.fraction > 0 )
		{
			// actually covered some distance
			VectorCopy (trace.endpos, pm->ps.origin);
		}

		if( trace.fraction == 1 ) break; // moved the entire distance

		// save entity for contact
		PM_AddTouchEnt( trace.ent );

		time_left -= time_left * trace.fraction;

		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			VectorClear( pm->ps.velocity );
			return true;
		}

		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		for ( i = 0; i < numplanes; i++ )
		{
			if( DotProduct( trace.plane.normal, planes[i] ) > 0.99 )
			{
				VectorAdd( trace.plane.normal, pm->ps.velocity, pm->ps.velocity );
				break;
			}
		}
		if( i < numplanes ) continue;
		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for ( i = 0; i < numplanes; i++ )
		{
			into = DotProduct( pm->ps.velocity, planes[i] );
			if( into >= 0.1f ) continue;	// move doesn't interact with the plane

			// see how hard we are hitting things
			if( -into > pml.impact_speed )
			{
				pml.impact_speed = -into;
			}

			// slide along the plane
			PM_ClipVelocity( pm->ps.velocity, planes[i], clipVelocity, OVERCLIP );

			// slide along the plane
			PM_ClipVelocity( endVelocity, planes[i], endClipVelocity, OVERCLIP );

			// see if there is a second plane that the new move enters
			for ( j = 0; j < numplanes; j++ )
			{
				if( j == i ) continue;
				if( DotProduct( clipVelocity, planes[j] ) >= 0.1f )
				{
					// move doesn't interact with the plane
					continue;
				}

				// try clipping the move to the plane
				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				// see if it goes back into the first clip plane
				if( DotProduct( clipVelocity, planes[i] ) >= 0 )
				{
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct( planes[i], planes[j], dir );
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps.velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct( planes[i], planes[j], dir );
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the the new move enters
				for( k = 0 ; k < numplanes ; k++ )
				{
					if( k == i || k == j ) continue;
					if( DotProduct( clipVelocity, planes[k] ) >= 0.1f )
					{
						// move doesn't interact with the plane
						continue;
					}

					// stop dead at a tripple plane interaction
					VectorClear( pm->ps.velocity );
					return true;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps.velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if( gravity )
	{
		VectorCopy( endVelocity, pm->ps.velocity );
	}

	// don't change velocity if in a timer (FIXME: is this correct?)
	if( pm->ps.pm_time )
	{
		VectorCopy( primal_velocity, pm->ps.velocity );
	}
	return ( bumpcount != 0 );
}

/*
==================
PM_StepSlideMove

==================
*/
void PM_StepSlideMove( bool gravity )
{
	vec3_t		start_o, start_v;
	vec3_t		down_o, down_v;
	trace_t		trace;
	vec3_t		up, down;
	float		stepSize;

	VectorCopy( pm->ps.origin, start_o );
	VectorCopy( pm->ps.velocity, start_v );

	if( !PM_SlideMove( gravity ))
	{
		// we got exactly where we wanted to go first try	
		return;
	}

	VectorCopy( start_o, down );
	down[2] -= STEPSIZE;
	trace = pm->trace( start_o, pm->mins, pm->maxs, down );
	VectorSet( up, 0, 0, 1 );
	// never step up when you still have up velocity
	if( pm->ps.velocity[2] > 0 && (trace.fraction == 1.0 || DotProduct(trace.plane.normal, up) < 0.7))
	{
		return;
	}

	VectorCopy (pm->ps.origin, down_o);
	VectorCopy (pm->ps.velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += STEPSIZE;

	// test the player position if they were a stepheight higher
	trace = pm->trace( start_o, pm->mins, pm->maxs, up );
	if( trace.allsolid ) return; // can't step up

	stepSize = trace.endpos[2] - start_o[2];
	// try slidemove from this position
	VectorCopy( trace.endpos, pm->ps.origin );
	VectorCopy( start_v, pm->ps.velocity );

	PM_SlideMove( gravity );

	// push down the final amount
	VectorCopy (pm->ps.origin, down);
	down[2] -= stepSize;
	trace = pm->trace( pm->ps.origin, pm->mins, pm->maxs, down );
	if( !trace.allsolid )
	{
		VectorCopy (trace.endpos, pm->ps.origin);
	}
	if( trace.fraction < 1.0 )
	{
		PM_ClipVelocity( pm->ps.velocity, trace.plane.normal, pm->ps.velocity, OVERCLIP );
	}
	// add some code for footsteps sound here
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
void PM_Friction( void )
{
	vec3_t	vec;
	float	*vel;
	float	speed, newspeed, control;
	float	drop = 0;
	
	vel = pm->ps.velocity;

	VectorCopy( vel, vec );	
	if( pml.walking ) vec[2] = 0;	// ignore slope movement

	speed = VectorLength( vec );

	if( speed < 1 )
	{
		vel[0] = vel[1] = 0; // allow sinking underwater
		// FIXME: still have z friction underwater?
		return;
	}

	// apply ground friction
	if( pm->waterlevel <= 1 )
	{
		if( pml.walking && !(pml.groundtrace.flags & SURF_SLICK) || (pml.onladder))
		{
			control = speed < pm_stopspeed ? pm_stopspeed : speed;
			drop += control * pm_friction * pml.frametime;
		}
	}

	// apply water friction
	if( pm->waterlevel && !pml.onladder ) drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0) newspeed = 0;
	newspeed /= speed;

	VectorScale( vel, newspeed, vel );
}


/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
void PM_Accelerate( vec3_t wishdir, float wishspeed, float accel )
{
	int	i;
	float	addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (pm->ps.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if( addspeed <= 0 ) return;
	accelspeed = accel * pml.frametime * wishspeed;
	if( accelspeed > addspeed ) accelspeed = addspeed;
	
	for( i = 0; i < 3; i++ )
		pm->ps.velocity[i] += accelspeed * wishdir[i];	
}

/*
=============
PM_AddCurrents
=============
*/
void PM_AddCurrents( vec3_t wishvel )
{
	vec3_t	v;
	float	s;

	// account for onladders
	if (pml.onladder && fabs(pm->ps.velocity[2]) <= 200)
	{
		if ((pm->ps.viewangles[PITCH] <= -15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = 200;
		else if ((pm->ps.viewangles[PITCH] >= 15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = -200;
		else if (pm->cmd.upmove > 0)
			wishvel[2] = 200;
		else if (pm->cmd.upmove < 0)
			wishvel[2] = -200;
		else wishvel[2] = 0;

		// limit horizontal speed when on a onladder
		if (wishvel[0] < -25) wishvel[0] = -25;
		else if (wishvel[0] > 25) wishvel[0] = 25;

		if (wishvel[1] < -25) wishvel[1] = -25;
		else if (wishvel[1] > 25) wishvel[1] = 25;
	}


	//
	// add water currents
	//

	if (pm->watertype & MASK_CURRENT)
	{
		VectorClear (v);

		if (pm->watertype & CONTENTS_CURRENT_0)
			v[0] += 1;
		if (pm->watertype & CONTENTS_CURRENT_90)
			v[1] += 1;
		if (pm->watertype & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if (pm->watertype & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if (pm->watertype & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if (pm->watertype & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		s = pm_waterspeed;
		if ((pm->waterlevel == 1) && (pm->ps.groundentity))
			s /= 2;

		VectorMA (wishvel, s, v, wishvel);
	}

	//
	// add conveyor belt velocities
	//

	if (pm->ps.groundentity)
	{
		VectorClear (v);

		if (pml.groundtrace.contents & CONTENTS_CURRENT_0)
			v[0] += 1;
		if (pml.groundtrace.contents & CONTENTS_CURRENT_90)
			v[1] += 1;
		if (pml.groundtrace.contents & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if (pml.groundtrace.contents & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if (pml.groundtrace.contents & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if (pml.groundtrace.contents & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		VectorMA (wishvel, 100 /* pm->ps.groundentity->speed */, v, wishvel);
	}
}

/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale( usercmd_t *cmd )
{
	int	max;
	float	total;

	max = abs( cmd->forwardmove );
	if( abs( cmd->sidemove ) > max )
	{
		max = abs( cmd->sidemove );
	}
	if( abs( cmd->upmove ) > max )
	{
		max = abs( cmd->upmove );
	}
	if( !max ) return 0.0f;

	total = sqrt( cmd->forwardmove * cmd->forwardmove + cmd->sidemove * cmd->sidemove + cmd->upmove * cmd->upmove );
	return (float)PM_SPEED * max / ( 127.0 * total );
}

/*
=============
PM_CheckJump
=============
*/
static bool PM_CheckJump( void )
{
	if( pm->cmd.upmove < 10 )
	{
		// not holding jump
		pm->ps.pm_flags &= ~PMF_JUMP_HELD;
		return false;
	}

	// must wait for jump to be released
	if (pm->ps.pm_flags & PMF_JUMP_HELD)
	{
		// clear upmove so cmdscale doesn't lower running speed
		pm->cmd.upmove = 0;
		return false;
	}

	pml.groundplane = false;		// jumping away
	pml.walking = false;
	pm->ps.groundentity = NULL;
	pm->ps.pm_flags |= PMF_JUMP_HELD;
	pm->ps.velocity[2] = JUMP_VELOCITY;

	return true;
}

static void PM_CheckOnLadder( void )
{
	vec3_t	spot;
	vec3_t	flatforward;
	trace_t	trace;

	if (pm->ps.pm_time) return;

	pml.onladder = false;

	// check for onladder
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	VectorMA (pm->ps.origin, 1, flatforward, spot);
	trace = pm->trace (pm->ps.origin, pm->mins, pm->maxs, spot);
	if((trace.fraction < 1) && (trace.contents & CONTENTS_LADDER))
	{
		pml.onladder = true;
		pml.walking = false;
	}
}

/*
=============
PM_CheckWaterJump
=============
*/
static bool PM_CheckWaterJump( void )
{
	vec3_t	spot;
	int	cont;
	vec3_t	flatforward;

	if( pm->ps.pm_time ) return false;

	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize( flatforward );

	// check for water jump
	if( pm->waterlevel != 2 ) return false;

	VectorMA( pm->ps.origin, 30, flatforward, spot );
	spot[2] += 4;
	cont = pm->pointcontents(spot );
	if(!(cont & CONTENTS_SOLID)) return false;

	spot[2] += 16;
	cont = pm->pointcontents( spot );
	if( cont ) return false;

	// jump out of water
	VectorScale( pml.forward, 50, pm->ps.velocity );
	pm->ps.velocity[2] = 350;

	pm->ps.pm_flags |= PMF_TIME_WATERJUMP;
	pm->ps.pm_time = 255;

	return true;
}

/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove( void )
{
	// waterjump has no control, but falls
	PM_StepSlideMove( true );

	pm->ps.velocity[2] -= pm->ps.gravity * pml.frametime;
	if( pm->ps.velocity[2] < 0 )
	{
		// cancel as soon as we are falling down again
		pm->ps.pm_flags &= ~PMF_ALL_TIMES;
		pm->ps.pm_time = 0;
	}
}

/*
===================
PM_WaterMove
===================
*/
static void PM_WaterMove( void )
{
	int	i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;
	float	vel;

	if( PM_CheckWaterJump())
	{
		PM_WaterJumpMove();
		return;
	}

	PM_Friction();

	scale = PM_CmdScale( &pm->cmd );

	// user intentions
	if( !scale )
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = -60;		// sink towards bottom
	}
	else
	{
		for(i = 0; i < 3; i++)
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.sidemove;
		wishvel[2] += scale * pm->cmd.upmove;
	}

	PM_AddCurrents( wishvel );
	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );

	if( wishspeed > PM_SPEED * pm_swimscale )
	{
		wishspeed = PM_SPEED * pm_swimscale;
	}

	PM_Accelerate( wishdir, wishspeed, pm_wateraccelerate );

	// make sure we can go up slopes easily under water
	if ( pml.groundplane && DotProduct( pm->ps.velocity, pml.groundtrace.plane.normal ) < 0 )
	{
		vel = VectorLength(pm->ps.velocity);
		// slide along the ground plane
		PM_ClipVelocity (pm->ps.velocity, pml.groundtrace.plane.normal, pm->ps.velocity, OVERCLIP );
		VectorNormalize(pm->ps.velocity);
		VectorScale(pm->ps.velocity, vel, pm->ps.velocity);
	}
	PM_SlideMove( false );
}


/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove( void )
{
	int	i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	float	scale;

	// normal slowdown
	PM_Friction();

	scale = PM_CmdScale( &pm->cmd );

	// user intentions
	if( !scale )
	{
		wishvel[0] = 0;
		wishvel[1] = 0;
		wishvel[2] = 0;
	}
	else
	{
		for( i = 0; i < 3; i++)
		{
			wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.sidemove;
		}
		wishvel[2] += scale * pm->cmd.upmove;
	}

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize(wishdir);

	PM_Accelerate( wishdir, wishspeed, pm_flyaccelerate );
	PM_StepSlideMove( false );
}

/*
===================
PM_AirMove
===================
*/
static void PM_AirMove( void )
{
	int		i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;
	usercmd_t		cmd;

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );

	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 2; i++ )
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}
	wishvel[2] = 0;

	PM_AddCurrents( wishvel );
	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	// not on ground, so little effect on velocity
	PM_Accelerate( wishdir, wishspeed, pm_airaccelerate );

	// we may have a ground plane that is very steep, even
	// though we don't have a groundentity
	// slide along the steep plane
	if ( pml.onladder )
	{
		PM_Accelerate( wishdir, wishspeed, pm_accelerate );
		if( !wishvel[2] )
		{
			if( pm->ps.velocity[2] > 0 )
			{
				pm->ps.velocity[2] -= pm->ps.gravity * pml.frametime;
				if( pm->ps.velocity[2] < 0 ) pm->ps.velocity[2] = 0;
			}
			else
			{
				pm->ps.velocity[2] += pm->ps.gravity * pml.frametime;
				if( pm->ps.velocity[2] > 0 ) pm->ps.velocity[2] = 0;
			}
		}
	}
	else if( pml.groundplane )
	{
		PM_ClipVelocity (pm->ps.velocity, pml.groundtrace.plane.normal, pm->ps.velocity, OVERCLIP );
	}

	PM_StepSlideMove ( true );
}

/*
===================
PM_WalkMove
===================
*/
static void PM_WalkMove( void )
{
	int		i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;
	usercmd_t		cmd;
	float		accelerate;
	float		vel;

	if( pm->waterlevel > 2 && DotProduct( pml.forward, pml.groundtrace.plane.normal ) > 0 )
	{
		// begin swimming
		PM_WaterMove();
		return;
	}

	if( PM_CheckJump())
	{
		// jumped away
		if( pm->waterlevel > 1 )
			PM_WaterMove();
		else PM_AirMove();
		return;
	}

	PM_Friction();

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	cmd = pm->cmd;
	scale = PM_CmdScale( &cmd );
	
	// project moves down to flat plane
	pml.forward[2] = 0;
	pml.right[2] = 0;

	// project the forward and right directions onto the ground plane
	PM_ClipVelocity( pml.forward, pml.groundtrace.plane.normal, pml.forward, OVERCLIP );
	PM_ClipVelocity( pml.right, pml.groundtrace.plane.normal, pml.right, OVERCLIP );
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
	{
		wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	}
	// when going up or down slopes the wish velocity should Not be zero
	PM_AddCurrents( wishvel );

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	// clamp the speed lower if ducking
	if( pm->ps.pm_flags & PMF_DUCKED )
	{
		if( wishspeed > pm_duckspeed )
		{
			wishspeed = pm_duckspeed;
		}
	}

	// clamp the speed lower if wading or walking on the bottom
	if( pm->waterlevel )
	{
		float	waterScale;

		waterScale = pm->waterlevel / 3.0;
		waterScale = 1.0 - ( 1.0 - pm_swimscale ) * waterScale;
		if ( wishspeed > PM_SPEED * waterScale )
		{
			wishspeed = PM_SPEED * waterScale;
		}
	}

	// when a player gets hit, they temporarily lose
	// full control, which allows them to be moved a bit
	if( pml.groundtrace.flags & SURF_SLICK )
	{
		accelerate = pm_airaccelerate;
	}
	else
	{
		accelerate = pm_accelerate;
	}

	PM_Accelerate( wishdir, wishspeed, accelerate );

	//Msg("velocity = %1.1f %1.1f %1.1f\n", pm->ps.velocity[0], pm->ps.velocity[1], pm->ps.velocity[2]);
	//Msg("velocity1 = %1.1f\n", VectorLength(pm->ps.velocity));

	if( pml.groundtrace.flags & SURF_SLICK )
	{
		pm->ps.velocity[2] -= pm->ps.gravity * pml.frametime;
	}

	vel = VectorLength( pm->ps.velocity );

	// slide along the ground plane
	PM_ClipVelocity (pm->ps.velocity, pml.groundtrace.plane.normal, pm->ps.velocity, OVERCLIP );

	// don't decrease velocity when going up or down a slope
	VectorNormalize( pm->ps.velocity );
	VectorScale( pm->ps.velocity, vel, pm->ps.velocity );

	// don't do anything if standing still
	if( !pm->ps.velocity[0] && !pm->ps.velocity[1] )
	{
		return;
	}

	PM_StepSlideMove( false );

	//Msg("velocity2 = %1.1f\n", VectorLength(pm->ps.velocity));
}


/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove( void )
{
	float	forward;

	if( !pml.walking ) return;

	// extra friction
	forward = VectorLength( pm->ps.velocity );
	forward -= 20;
	if( forward <= 0 )
	{
		VectorClear( pm->ps.velocity );
	}
	else
	{
		VectorNormalize( pm->ps.velocity );
		VectorScale( pm->ps.velocity, forward, pm->ps.velocity );
	}
}


/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove( void )
{
	float		speed, drop, friction, control, newspeed;
	int		i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		scale;

	// friction
	speed = VectorLength( pm->ps.velocity );
	if( speed < 1 )
	{
		VectorCopy( vec3_origin, pm->ps.velocity );
	}
	else
	{
		drop = 0;
		friction = pm_friction * 1.5f; // extra friction
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control*friction*pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if( newspeed < 0 ) newspeed = 0;
		newspeed /= speed;
		VectorScale( pm->ps.velocity, newspeed, pm->ps.velocity );
	}

	// accelerate
	scale = PM_CmdScale( &pm->cmd );
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;
	
	for (i = 0; i < 3; i++) wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy( wishvel, wishdir );
	wishspeed = VectorNormalize( wishdir );
	wishspeed *= scale;

	PM_Accelerate( wishdir, wishspeed, pm_accelerate );

	// move
	VectorMA (pm->ps.origin, pml.frametime, pm->ps.velocity, pm->ps.origin);
}

/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand( void )
{
	float	delta;
	float	dist;
	float	vel, acc;
	float	t;
	float	a, b, c, den;

	// calculate the exact velocity on landing
	dist = pm->ps.origin[2] - pml.previous_origin[2];
	vel = pml.previous_velocity[2];
	acc = -pm->ps.gravity;

	a = acc / 2;
	b = vel;
	c = -dist;

	den =  b * b - 4 * a * c;
	if( den < 0 ) return;
	t = (-b - sqrt( den ) ) / ( 2 * a );

	delta = vel + t * acc;
	delta = delta * delta * 0.0001;

	// ducking while falling doubles damage
	if( pm->ps.pm_flags & PMF_DUCKED ) delta *= 2;

	// never take falling damage if completely underwater
	if( pm->waterlevel == 3 ) return;

	// reduce falling damage if there is standing water
	if( pm->waterlevel == 2 ) delta *= 0.25;
	if( pm->waterlevel == 1 ) delta *= 0.5;
	if( delta < 1 ) return;

	// start footstep cycle over
	pm->ps.bobcycle = 0;
}

/*
=============
PM_CorrectAllSolid
=============
*/
static int PM_CorrectAllSolid( trace_t *trace )
{
	int		i, j, k;
	vec3_t		point;

	// jitter around
	for( i = -1; i <= 1; i++ )
	{
		for( j = -1; j <= 1; j++ )
		{
			for( k = -1; k <= 1; k++)
			{
				VectorCopy( pm->ps.origin, point );
				point[0] += (float)i;
				point[1] += (float)j;
				point[2] += (float)k;
				*trace = pm->trace( point, pm->mins, pm->maxs, point );
				if( !trace->allsolid )
				{
					point[0] = pm->ps.origin[0];
					point[1] = pm->ps.origin[1];
					point[2] = pm->ps.origin[2] - 0.25;

					pml.groundtrace = pm->trace( pm->ps.origin, pm->mins, pm->maxs, point );
					trace = &pml.groundtrace;
					return true;
				}
			}
		}
	}

	pm->ps.groundentity = NULL;
	pml.groundplane = false;
	pml.walking = false;
	return false;
}

/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed( void )
{
	trace_t		trace;
	vec3_t		point;

	if( pm->ps.groundentity )
	{
		// if they aren't in a jumping animation and the ground is a ways away, force into it
		// if we didn't do the trace, the player would be backflipping down staircases
		VectorCopy( pm->ps.origin, point );
		point[2] -= 64;

		trace = pm->trace( pm->ps.origin, pm->mins, pm->maxs, point );
		if ( trace.fraction == 1.0 )
		{
			if( pm->cmd.forwardmove >= 0 )
			{
				// anim jump forward
			}
			else
			{
				// anim jump backward
			}
		}
	}

	pm->ps.groundentity = NULL;
	pml.groundplane = false;
	pml.walking = false;
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace( void )
{
	vec3_t		point;
	trace_t		trace;

	point[0] = pm->ps.origin[0];
	point[1] = pm->ps.origin[1];
	point[2] = pm->ps.origin[2] - 0.25;

	trace = pm->trace( pm->ps.origin, pm->mins, pm->maxs, point );
	pml.groundtrace = trace;

	// do something corrective if the trace starts in a solid...
	if( trace.allsolid )
	{
		if(!PM_CorrectAllSolid(&trace))
			return;
	}

	// if the trace didn't hit anything, we are in free fall
	if( trace.fraction == 1.0 )
	{
		PM_GroundTraceMissed();
		pml.groundplane = false;
		pml.walking = false;
		return;
	}

	// check if getting thrown off the ground
	if( pm->ps.velocity[2] > 0 && DotProduct( pm->ps.velocity, trace.plane.normal ) > 10 )
	{
		// go into jump animation
		if( pm->cmd.forwardmove >= 0 )
		{
			// anim jump forward
		}
		else
		{
			// anim jump backward
		}

		pm->ps.groundentity = NULL;
		pml.groundplane = false;
		pml.walking = false;
		return;
	}
	
	// slopes that are too steep will not be considered onground
	if ( trace.plane.normal[2] < MIN_WALK_NORMAL )
	{
		// FIXME: if they can't slide down the slope, let them
		// walk (sharp crevices)
		pm->ps.groundentity = NULL;
		pml.groundplane = true;
		pml.walking = false;
		return;
	}

	pml.groundplane = true;
	pml.walking = true;

	// hitting solid ground will end a waterjump
	if( pm->ps.pm_flags & PMF_TIME_WATERJUMP )
	{
		pm->ps.pm_flags &= ~(PMF_TIME_WATERJUMP|PMF_TIME_LAND);
		pm->ps.pm_time = 0;
	}

	if( !pm->ps.groundentity )
	{
		PM_CrashLand();

		// don't do landing time if we were just going down a slope
		if( pml.previous_velocity[2] < -200 )
		{
			// don't allow another jump for a little while
			pm->ps.pm_flags |= PMF_TIME_LAND;
			pm->ps.pm_time = 250;
		}
	}

	pm->ps.groundentity = trace.ent;
	PM_AddTouchEnt( trace.ent );
}


/*
=============
PM_SetWaterLevel	FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevel( void )
{
	vec3_t		point;
	int		cont;
	int		sample1;
	int		sample2;

	// get waterlevel, accounting for ducking
	pm->waterlevel = 0;
	pm->watertype = 0;

	point[0] = pm->ps.origin[0];
	point[1] = pm->ps.origin[1];
	point[2] = pm->ps.origin[2] - 0.25;	
	cont = pm->pointcontents( point );

	if( cont & MASK_WATER )
	{
		sample2 = pm->ps.viewheight - pm->mins[2];
		sample1 = sample2 / 2;

		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pm->ps.origin[2] + pm->mins[2] + sample1;
		cont = pm->pointcontents( point );
		if( cont & MASK_WATER )
		{
			pm->waterlevel = 2;
			point[2] = pm->ps.origin[2] + pm->mins[2] + sample2;
			cont = pm->pointcontents (point );
			if( cont & MASK_WATER ) pm->waterlevel = 3;
		}
	}

}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps.viewheight
==============
*/
static void PM_CheckDuck( void )
{
	trace_t	trace;

	pm->mins[0] = -16;
	pm->mins[1] = -16;
	pm->maxs[0] = 16;
	pm->maxs[1] = 16;

	if( pm->ps.pm_type == PM_GIB )
	{
		pm->mins[2] = 0;
		pm->maxs[2] = 16;
		pm->ps.viewheight = 8;
		return;
	}

	pm->mins[2] = -24;
	
	if( pm->cmd.upmove < 0 )
	{	
		// duck
		pm->ps.pm_flags |= PMF_DUCKED;
	}
	else
	{	// stand up if possible
		if( pm->ps.pm_flags & PMF_DUCKED )
		{
			// try to stand up
			pm->maxs[2] = 32;
			trace = pm->trace( pm->ps.origin, pm->mins, pm->maxs, pm->ps.origin );
			if( !trace.allsolid ) pm->ps.pm_flags &= ~PMF_DUCKED;
		}
	}

	if( pm->ps.pm_flags & PMF_DUCKED )
	{
		pm->maxs[2] = 4;
		pm->ps.viewheight = -2;
	}
	else
	{
		pm->maxs[2] = 32;
		pm->ps.viewheight = 22;
	}
}

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers( void )
{
	if( pm->ps.pm_time )
	{
		int	msec;

		msec = pm->cmd.msec>>3;
		if(!msec) msec = 1;

		if( msec >= pm->ps.pm_time) 
		{
			pm->ps.pm_flags &= ~PMF_ALL_TIMES;
			pm->ps.pm_time = 0;
		}
		else pm->ps.pm_time -= msec;
	}
}

/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move
================
*/
void PM_UpdateViewAngles( player_state_t *ps, const usercmd_t *cmd )
{
	short		temp;
	int		i;

	if( ps->pm_type == PM_INTERMISSION )
	{
		return;		// no view changes at all
	}
	if( ps->pm_type == PM_DEAD )
	{
		return;		// no view changes at all
	}
	if( pm->ps.pm_flags & PMF_TIME_TELEPORT)
	{
		ps->viewangles[YAW] = SHORT2ANGLE( pm->cmd.angles[YAW] + pm->ps.delta_angles[YAW] );
		ps->viewangles[PITCH] = 0;
		ps->viewangles[ROLL] = 0;
	}

	// circularly clamp the angles with deltas
	for( i = 0; i < 3; i++ )
	{
		temp = pm->cmd.angles[i] + pm->ps.delta_angles[i];
		ps->viewangles[i] = SHORT2ANGLE(temp);
	}

	// don't let the player look up or down more than 90 degrees
	if( ps->viewangles[PITCH] > 89 && ps->viewangles[PITCH] < 180 )
		ps->viewangles[PITCH] = 89;
	else if( ps->viewangles[PITCH] < 271 && ps->viewangles[PITCH] >= 180 )
		ps->viewangles[PITCH] = 271;
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Quake_PMove( pmove_t *pmove )
{
	pm = pmove;

	// clear results
	pm->numtouch = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	// clear all pmove local vars
	memset (&pml, 0, sizeof(pml));

	// save old org in case we get stuck
	VectorCopy( pm->ps.origin, pml.previous_origin );
	// save old velocity for crashlanding
	VectorCopy( pm->ps.velocity, pml.previous_velocity );

	pml.frametime = pm->cmd.msec * 0.001;

	// update the viewangles
	PM_UpdateViewAngles( &pm->ps, &pm->cmd );
	AngleVectors( pm->ps.viewangles, pml.forward, pml.right, pml.up );

	if( pm->cmd.upmove < 10 )
	{
		// not holding jump
		pm->ps.pm_flags &= ~PMF_JUMP_HELD;
	}

	if( pm->ps.pm_type >= PM_DEAD )
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if( pm->ps.pm_type == PM_SPECTATOR )
	{
		PM_CheckDuck();
		PM_FlyMove();
		PM_DropTimers();
		return;
	}

	if( pm->ps.pm_type == PM_NOCLIP )
	{
		PM_NoclipMove();
		PM_DropTimers();
		return;
	}

	if(pm->ps.pm_type == PM_FREEZE)
	{
		return; // no movement at all
	}

	if ( pm->ps.pm_type == PM_INTERMISSION )
	{
		return; // no movement at all
	}

	// set watertype, and waterlevel
	PM_SetWaterLevel();
	pml.previous_waterlevel = pmove->waterlevel;

	// set mins, maxs, and viewheight
	PM_CheckDuck();

	// set groundentity
	PM_GroundTrace();

	if( pm->ps.pm_type == PM_DEAD )
	{
		PM_DeadMove();
	}

	PM_CheckOnLadder();
	PM_DropTimers();

	if(pm->ps.pm_flags & PMF_TIME_WATERJUMP)
	{
		PM_WaterJumpMove();
	}
	else if( pm->waterlevel > 1 )
	{
		// swimming
		PM_WaterMove();
	}
	else if( pml.walking )
	{
		// walking on ground
		PM_WalkMove();
	}
	else
	{
		// airborne
		PM_AirMove();
	}

	// set groundentity, watertype, and waterlevel
	PM_GroundTrace();
	PM_SetWaterLevel();

	PM_SnapVector( pm->ps.velocity );

	if( pmove->ps.pm_flags & PMF_JUMP_HELD )
	{
		pmove->cmd.upmove = 20;
	}
	//PM_CheckStuck();
}

void CM_CmdUpdateForce( void )
{
	float	fmove, smove;
	int	i;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	// project moves down to flat plane
	pml.forward[2] = pml.right[2] = 0;
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
		pml.movedir[i] = pml.forward[i] * fmove + pml.right[i] * smove;

	ConvertDirectionToPhysic( pml.movedir );

	if( pm->cmd.upmove > 0.0f )
	{
		m_jumpTimer = 4;
	}
}

void CM_ServerMove( pmove_t *pmove )
{
	float	mass, Ixx, Iyy, Izz, dist, floor;
	float	deltaHeight, steerAngle, accelY, timestepInv;
	vec3_t	force, omega, torque, heading, velocity, step;
	matrix4x4	matrix, collisionPadding, transpose;

	pm = pmove;

	PM_UpdateViewAngles( &pm->ps, &pm->cmd );
	AngleVectors( pm->ps.viewangles, pml.forward, pml.right, pml.up );
	CM_CmdUpdateForce(); // get movement direction

	// Get the current world timestep
	pml.frametime = NewtonGetTimeStep( gWorld );
	timestepInv = 1.0f / pml.frametime;

	// get the character mass
	NewtonBodyGetMassMatrix( pm->body, &mass, &Ixx, &Iyy, &Izz );

	// apply the gravity force, cheat a little with the character gravity
	VectorSet( force, 0.0f, mass * -9.8f, 0.0f );

	// Get the velocity vector
	NewtonBodyGetVelocity( pm->body, &velocity[0] );

	// determine if the character have to be snap to the ground
	NewtonBodyGetMatrix( pm->body, &matrix[0][0] );

	// if the floor is with in reach then the character must be snap to the ground
	// the max allow distance for snapping i 0.25 of a meter
	if( m_isAirBorne && !m_jumpTimer )
	{ 
		floor = CM_FindFloor( matrix[3], m_size[2] + 0.25f );
		deltaHeight = ( matrix[3][1] - m_size[2] ) - floor;

		if((deltaHeight < (0.25f - 0.001f)) && (deltaHeight > 0.01f))
		{
			// snap to floor only if the floor is lower than the character feet		
			accelY = - (deltaHeight * timestepInv + velocity[1]) * timestepInv;
			force[1] = mass * accelY;
		}
	}
	else if( m_jumpTimer == 4 )
	{
		vec3_t	veloc = { 0.0f, 0.3f, 0.0f };
		NewtonAddBodyImpulse( pm->body, &veloc[0], &matrix[3][0] );
	}

	m_jumpTimer = m_jumpTimer ? m_jumpTimer - 1 : 0;

	{
		float	speed_factor;
		vec3_t	tmp1, tmp2, result;

		speed_factor = sqrt( DotProduct( pml.movedir, pml.movedir ) + 1.0e-6f );  
		VectorScale( pml.movedir, 1.0f / speed_factor, heading ); 

		VectorScale( heading, mass * 20.0f, tmp1 );
		VectorScale( heading, 2.0f * DotProduct( velocity, heading ), tmp2 );

		VectorSubtract( tmp1, tmp2, result );
		VectorAdd( force, result, force );
		NewtonBodySetForce( pm->body, &force[0] );

		VectorScale( force, pml.frametime / mass, tmp1 );
		VectorAdd( tmp1, velocity, step );
		VectorScale( step, pml.frametime, step );
	}

	VectorSet(step, DotProduct(step,cm.matrix[0]),DotProduct(step,cm.matrix[1]),DotProduct(step,cm.matrix[2])); 	
	MatrixLoadIdentity( collisionPadding );

	step[1] = 0.0f;

	dist = DotProduct( step, step );

	if( dist > m_maxTranslation * m_maxTranslation )
	{
		// when the velocity is high enough that can miss collision we will enlarge the collision 
		// long the vector velocity
		dist = sqrt( dist );
		VectorScale( step, 1.0f / dist, step );

		// make a rotation matrix that will align the velocity vector with the front vector
		collisionPadding[0][0] =  step[0];
		collisionPadding[0][2] = -step[2];
		collisionPadding[2][0] =  step[2];
		collisionPadding[2][2] =  step[0];

		// get the transpose of the matrix                    
		MatrixTranspose( transpose, collisionPadding );
		VectorScale( transpose[0], dist / m_maxTranslation, transpose[0] ); // scale factor

		// calculate and oblique scale matrix by using a similar transformation matrix of the for, R'* S * R
		MatrixConcat( collisionPadding, collisionPadding, transpose );
	}

	// set the collision modifierMatrix;
//NewtonConvexHullModifierSetMatrix( NewtonBodyGetCollision(pm->body), &collisionPadding[0][0]);
          
	steerAngle = asin( bound( -1.0f, pml.forward[2], 1.0f ));
	
	// calculate the torque vector
	NewtonBodyGetOmega( pm->body, &omega[0]);

	VectorSet( torque, 0.0f, 0.5f * Iyy * (steerAngle * timestepInv - omega[1] ) * timestepInv, 0.0f );
	NewtonBodySetTorque( pm->body, &torque[0] );


	// assume the character is on the air. this variable will be set to false if the contact detect 
	// the character is landed 
	m_isAirBorne = true;
	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   

	pm->ps.viewheight = 22;
	NewtonUpVectorSetPin( cm.upVector, &vec3_up[0] );
}

void CM_ClientMove( pmove_t *pmove )
{

}

physbody_t *Phys_CreatePlayer( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform )
{
	NewtonCollision	*col, *hull;
	NewtonBody	*body;

	matrix4x4		trans;
	vec3_t		radius, mins, maxs, upDirection;

	if( !cm_physics_model->integer )
		return NULL;

	// setup matrixes
	MatrixLoadIdentity( trans );

	if( mod )
	{
		// player m_size
		VectorCopy( mod->mins, mins );
		VectorCopy( mod->maxs, maxs );
		VectorSubtract( maxs, mins, m_size );
		VectorScale( m_size, 0.5f, radius );
	}
	ConvertDimensionToPhysic( m_size );
	ConvertDimensionToPhysic( radius );

	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   
	m_maxTranslation = m_size[0] * 0.25f;
	m_maxStepHigh = -m_size[2] * 0.5f;

	VectorCopy( transform[3], trans[3] );	// copy position only

	trans[3][1] = CM_FindFloor( trans[3], 32768 ) + radius[2]; // merge floor position

	// place a sphere at the center
	col = NewtonCreateSphere( gWorld, radius[0], radius[2], radius[1], NULL ); 

	// wrap the character collision under a transform, modifier for tunneling trught walls avoidance
	hull = NewtonCreateConvexHullModifier( gWorld, col );
	NewtonReleaseCollision( gWorld, col );		
	body = NewtonCreateBody( gWorld, hull );	// create the rigid body
	NewtonBodySetAutoFreeze( body, 0 );		// disable auto freeze management for the player
	NewtonWorldUnfreezeBody( gWorld, body );	// keep the player always active 

	// reset angular and linear damping
	NewtonBodySetLinearDamping( body, 0.0f );
	NewtonBodySetAngularDamping( body, vec3_origin );
	NewtonBodySetMaterialGroupID( body, characterID );// set material Id for this object

	// setup generic callback to engine.dll
	NewtonBodySetUserData( body, ed );
	NewtonBodySetTransformCallback( body, Callback_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback( body, Callback_PmoveApplyForce );
	NewtonBodySetMassMatrix( body, 20.0f, m_size[0], m_size[1], m_size[2] ); // 20 kg
	NewtonBodySetMatrix(body, &trans[0][0] );// origin

	// release the collision geometry when not need it
	NewtonReleaseCollision( gWorld, hull );

  	// add and up vector constraint to help in keeping the body upright
	VectorSet( upDirection, 0.0f, 1.0f, 0.0f );
	cm.upVector = NewtonConstraintCreateUpVector( gWorld, &upDirection[0], body ); 
	NewtonBodySetContinuousCollisionMode( body, 1 );

	return (physbody_t *)body;
}

/*
===============================================================================
			PMOVE ENTRY POINT
===============================================================================
*/
void CM_PlayerMove( pmove_t *pmove, bool clientmove )
{
	if( !cm_physics_model->integer )
		Quake_PMove( pmove );
}