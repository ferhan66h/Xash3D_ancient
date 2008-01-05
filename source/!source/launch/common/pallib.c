//=======================================================================
//			Copyright XashXT Group 2007 �
//			pallib.c - create palette images
//=======================================================================

#include "launch.h"
#include "image.h"
#include "mathlib.h"

#define PAL_MAXCOLORS	256
#define RED		0
#define GREEN		1
#define BLUE		2

typedef struct pixel_box_s
{
	int	r0;  // min value, exclusive
	int	r1;  // max value, inclusive
	int	g0;  
	int	g1;  
	int	b0;  
	int	b1;
	int	vol;
} pixel_box_t;

// Histogram is in elements 1..HISTSIZE along each axis,
// element 0 is for base or marginal value
// NB: these must start out 0!
float	gm2[33][33][33];
int	wt[33][33][33];
int	mr[33][33][33];
int	mg[33][33][33];
int	mb[33][33][33];
uint	size;		// image size
int	K;		// colour look-up table size
word	*Qadd;
int	WindW, WindH, WindD;
int	i;
byte	*buffer;
static int Width, Height, Depth, Comp;

uint n2(int s)
{
	int	i;
	int	res = 1;

	for (i = 0; i < s; i++) 
		res = res*2;
	return res;
}

// build 3-D color histogram of counts, r/g/b, c^2
void Hist3d( byte *Ir, byte *Ig, byte *Ib, int *vwt, int *vmr, int *vmg, int *vmb, float *m2 )
{
	int	ind, r, g, b;
	int	inr, ing, inb, table[2560];
	uint	i;
		
	for (i = 0; i < 256; i++) table[i] = i * i;
	Qadd = (word*)Mem_Alloc( Sys.imagepool, sizeof(word) * size );
        
	for (i = 0; i < size; i++)
	{
		r = Ir[i]; g = Ig[i]; b = Ib[i];
		inr = (r>>3) + 1; 
		ing = (g>>3) + 1; 
		inb = (b>>3) + 1; 
		Qadd[i] = ind = (inr<<10) + (inr<<6) + inr + (ing<<5) + ing + inb;

		//[inr][ing][inb]
		vwt[ind]++;
		vmr[ind] += r;
		vmg[ind] += g;
		vmb[ind] += b;
		m2[ind] += (float)(table[r] + table[g] + table[b]);
	}
}

// At conclusion of the histogram step, we can interpret
// wt[r][g][b] = sum over voxel of P(c)
// mr[r][g][b] = sum over voxel of r*P(c), similarly for mg, mb
// m2[r][g][b] = sum over voxel of c^2*P(c)
// Actually each of these should be divided by 'size' to give the usual
// interpretation of P() as ranging from 0 to 1, but we needn't do that here.
// We now convert histogram into moments so that we can rapidly calculate
// the sums of the above quantities over any desired Box.

// compute cumulative moments
void M3d( int *vwt, int *vmr, int *vmg, int *vmb, float *m2 )
{
	word		ind1, ind2;
	byte		i, r, g, b;
	int		line, line_r, line_g, line_b, area[33], area_r[33], area_g[33], area_b[33];
	float		line2, area2[33];

	for (r = 1; r <= 32; r++)
	{
		for (i = 0; i <= 32; i++)
		{
			area2[i] = 0.0f;
			area[i] = area_r[i] = area_g[i] = area_b[i] = 0;
		}
		for (g = 1; g <= 32; g++)
		{
			line2 = 0.0f;
			line = line_r = line_g = line_b = 0;
			for (b = 1; b <= 32; b++)
			{
				ind1 = (r<<10) + (r<<6) + r + (g<<5) + g + b; // [r][g][b]
				line += vwt[ind1];
				line_r += vmr[ind1]; 
				line_g += vmg[ind1]; 
				line_b += vmb[ind1];
				line2 += m2[ind1];
				area[b] += line;
				area_r[b] += line_r;
				area_g[b] += line_g;
				area_b[b] += line_b;
				area2[b] += line2;
				ind2 = ind1 - 1089; // [r-1][g][b]
				vwt[ind1] = vwt[ind2] + area[b];
				vmr[ind1] = vmr[ind2] + area_r[b];
				vmg[ind1] = vmg[ind2] + area_g[b];
				vmb[ind1] = vmb[ind2] + area_b[b];
				m2[ind1] = m2[ind2] + area2[b];
			}
		}
	}
}

// compute sum over a Box of any given statistic
int Vol(pixel_box_t *cube, int mmt[33][33][33]) 
{
	return( mmt[cube->r1][cube->g1][cube->b1] -mmt[cube->r1][cube->g1][cube->b0] -mmt[cube->r1][cube->g0][cube->b1] +mmt[cube->r1][cube->g0][cube->b0] -mmt[cube->r0][cube->g1][cube->b1] +mmt[cube->r0][cube->g1][cube->b0] +mmt[cube->r0][cube->g0][cube->b1] -mmt[cube->r0][cube->g0][cube->b0] );
}

// the next two routines allow a slightly more efficient calculation
// of Vol() for a proposed subBox of a given Box.  The sum of Top()
// and Bottom() is the Vol() of a subBox split in the given direction
// and with the specified new upper bound.
// compute part of Vol(cube, mmt) that doesn't depend on r1, g1, or b1
// (depending on dir)

int Bottom(pixel_box_t *cube, byte dir, int mmt[33][33][33])
{
	switch(dir)
	{
	case RED: return( -mmt[cube->r0][cube->g1][cube->b1] +mmt[cube->r0][cube->g1][cube->b0] +mmt[cube->r0][cube->g0][cube->b1] -mmt[cube->r0][cube->g0][cube->b0] );
	case GREEN: return( -mmt[cube->r1][cube->g0][cube->b1] +mmt[cube->r1][cube->g0][cube->b0] +mmt[cube->r0][cube->g0][cube->b1] -mmt[cube->r0][cube->g0][cube->b0] );
	case BLUE: return( -mmt[cube->r1][cube->g1][cube->b0] +mmt[cube->r1][cube->g0][cube->b0] +mmt[cube->r0][cube->g1][cube->b0] -mmt[cube->r0][cube->g0][cube->b0] );
	}
	return 0;
}


// compute remainder of Vol(cube, mmt), substituting pos for
// r1, g1, or b1 (depending on dir)

int Top(pixel_box_t *cube, byte dir, int pos, int mmt[33][33][33])
{
	switch (dir)
	{
	case RED: return( mmt[pos][cube->g1][cube->b1] -mmt[pos][cube->g1][cube->b0] -mmt[pos][cube->g0][cube->b1] +mmt[pos][cube->g0][cube->b0] );
	case GREEN: return( mmt[cube->r1][pos][cube->b1] -mmt[cube->r1][pos][cube->b0] -mmt[cube->r0][pos][cube->b1] +mmt[cube->r0][pos][cube->b0] );
	case BLUE: return( mmt[cube->r1][cube->g1][pos] -mmt[cube->r1][cube->g0][pos] -mmt[cube->r0][cube->g1][pos] +mmt[cube->r0][cube->g0][pos] );
	}
	return 0;
}

// compute the weighted variance of a Box
// NB: as with the raw statistics, this is really the variance * size
float Var(pixel_box_t *cube)
{
	float	dr, dg, db, xx;

	dr = (float)Vol(cube, mr); 
	dg = (float)Vol(cube, mg); 
	db = (float)Vol(cube, mb);
	xx = gm2[cube->r1][cube->g1][cube->b1] -gm2[cube->r1][cube->g1][cube->b0] -gm2[cube->r1][cube->g0][cube->b1] +gm2[cube->r1][cube->g0][cube->b0] -gm2[cube->r0][cube->g1][cube->b1] +gm2[cube->r0][cube->g1][cube->b0] +gm2[cube->r0][cube->g0][cube->b1] -gm2[cube->r0][cube->g0][cube->b0];

	return xx - (dr*dr+dg*dg+db*db) / (float)Vol(cube, wt);
}

// We want to minimize the sum of the variances of two subBoxes.
// The sum(c^2) terms can be ignored since their sum over both subBoxes
// is the same (the sum for the whole Box) no matter where we split.
// The remaining terms have a minus sign in the variance formula,
// so we drop the minus sign and MAXIMIZE the sum of the two terms.

float Maximize( pixel_box_t *cube, byte dir, int first, int last, int *cut, int whole_r, int whole_g, int whole_b, int whole_w )
{
	int	half_r, half_g, half_b, half_w;
	int	base_r, base_g, base_b, base_w;
	int	i;
	float	temp, max = 0.0f;

	base_r = Bottom(cube, dir, mr);
	base_g = Bottom(cube, dir, mg);
	base_b = Bottom(cube, dir, mb);
	base_w = Bottom(cube, dir, wt);
	*cut = -1;

	for (i = first; i < last; ++i)
	{
		half_r = base_r + Top(cube, dir, i, mr);
		half_g = base_g + Top(cube, dir, i, mg);
		half_b = base_b + Top(cube, dir, i, mb);
		half_w = base_w + Top(cube, dir, i, wt);

		// now half_x is sum over lower half of pixel_box_t, if split at i 
		if (half_w == 0) continue; // never split into an empty pixel_box_t
		else temp = ((float)half_r*half_r + (float)half_g * half_g + (float)half_b*half_b) / half_w;

		half_r = whole_r - half_r;
		half_g = whole_g - half_g;
		half_b = whole_b - half_b;
		half_w = whole_w - half_w;
		if (half_w == 0) continue; // never split into an empty pixel_box_t
		else temp += ((float)half_r*half_r + (float)half_g * half_g + (float)half_b*half_b) / half_w;

		if (temp > max)
		{
			max = temp;
			*cut = i;
		}
	}
	return max;
}

int Cut( pixel_box_t *set1, pixel_box_t *set2 )
{
	byte	dir;
	int	cutr, cutg, cutb;
	float	maxr, maxg, maxb;
	int	whole_r, whole_g, whole_b, whole_w;

	whole_r = Vol(set1, mr);
	whole_g = Vol(set1, mg);
	whole_b = Vol(set1, mb);
	whole_w = Vol(set1, wt);

	maxr = Maximize( set1, RED, set1->r0+1, set1->r1, &cutr, whole_r, whole_g, whole_b, whole_w );
	maxg = Maximize( set1, GREEN, set1->g0+1, set1->g1, &cutg, whole_r, whole_g, whole_b, whole_w );
	maxb = Maximize( set1, BLUE, set1->b0+1, set1->b1, &cutb, whole_r, whole_g, whole_b, whole_w );

	if((maxr >= maxg) && (maxr >= maxb))
	{
		dir = RED;
		if(cutr < 0)return 0; // can't split the pixel_box_t
	}
	else if ((maxg >= maxr) && (maxg >= maxb))
		dir = GREEN;
	else dir = BLUE;

	set2->r1 = set1->r1;
	set2->g1 = set1->g1;
	set2->b1 = set1->b1;

	switch (dir)
	{
	case RED:
		set2->r0 = set1->r1 = cutr;
		set2->g0 = set1->g0;
		set2->b0 = set1->b0;
		break;
	case GREEN:
		set2->g0 = set1->g1 = cutg;
		set2->r0 = set1->r0;
		set2->b0 = set1->b0;
		break;
	case BLUE:
		set2->b0 = set1->b1 = cutb;
		set2->r0 = set1->r0;
		set2->g0 = set1->g0;
		break;
	}

	set1->vol = (set1->r1-set1->r0) * (set1->g1-set1->g0) * (set1->b1-set1->b0);
	set2->vol = (set2->r1-set2->r0) * (set2->g1-set2->g0) * (set2->b1-set2->b0);
	return 1;
}


void Mark( pixel_box_t *cube, int label, byte *tag )
{
	int	r, g, b;

	for (r = cube->r0 + 1; r <= cube->r1; r++)
	{
		for (g = cube->g0 + 1; g <= cube->g1; g++)
		{
			for (b = cube->b0 + 1; b <= cube->b1; b++)
			{
				tag[(r<<10) + (r<<6) + r + (g<<5) + g + b] = label;
			}
		}
	}
}

rgbdata_t *Image_CopyRGBA8bit( rgbdata_t *pix, int numcolors )
{
	pixel_box_t	cube[PAL_MAXCOLORS];
	byte		lut_r[PAL_MAXCOLORS];
	byte		lut_g[PAL_MAXCOLORS];
	byte		lut_b[PAL_MAXCOLORS];
	byte		*tag = NULL;
	int		next = 0;
	int		weight;
	uint		k;
	float		vv[PAL_MAXCOLORS], temp;
	byte		*NewData = NULL, *Palette = NULL;
	byte		*Ir = NULL, *Ig = NULL, *Ib = NULL;
	rgbdata_t		*newpix = NULL;
	int		num_alloced_colors;

	if(!pix) return NULL;

	switch( pix->type )
	{
	case PF_RGBA_32: break;
	default:
		MsgDev(D_ERROR, "Image_ConvertTo8bit: unsupported pixelformat %s\n", PFDesc[pix->type].name ); 
		return NULL;
	}

	num_alloced_colors = bound( 2, numcolors, 256 );
	buffer = pix->buffer;
	WindW = Width = pix->width;
	WindH = Height = pix->height;
	WindD = Depth = pix->numLayers;
	Comp = PFDesc[pix->type].bpp;
	Qadd = NULL;

	NewData = (byte *)Mem_Alloc( Sys.imagepool, Width * Height * Depth );
	Palette = (byte *)Mem_Alloc( Sys.imagepool, 3 * num_alloced_colors ); // 768 bytes

	Ir = Mem_Alloc( Sys.imagepool, Width * Height * Depth);
	Ig = Mem_Alloc( Sys.imagepool, Width * Height * Depth);
	Ib = Mem_Alloc( Sys.imagepool, Width * Height * Depth);
	size = Width * Height * Depth;

	for (k = 0; k < size; k++)
	{
		Ir[k] = pix->buffer[k*3+2];
		Ig[k] = pix->buffer[k*3+1];
		Ib[k] = pix->buffer[k*3+0];
	}
        
	// set new colors number
	K = numcolors;

	// begin Wu's color quantization algorithm
	// may have "leftovers" from a previous run.
	memset(gm2, 0, 33 * 33 * 33 * sizeof(float));
	memset(wt, 0, 33 * 33 * 33 * sizeof(int));
	memset(mr, 0, 33 * 33 * 33 * sizeof(int));
	memset(mg, 0, 33 * 33 * 33 * sizeof(int));
	memset(mb, 0, 33 * 33 * 33 * sizeof(int));
                
	// build 3d color histogramm
	Hist3d(Ir, Ig, Ib, (int*)wt, (int*)mr, (int*)mg, (int*)mb, (float*)gm2);
	M3d((int*)wt, (int*)mr, (int*)mg, (int*)mb, (float*)gm2);

	cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
	cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;

	for (i = 1; i < K; i++)
	{
		if(Cut(&cube[next], &cube[i]))
		{
			// volume test ensures we won't try to cut one-cell box
			vv[next] = (cube[next].vol > 1) ? Var(&cube[next]) : 0.0f;
			vv[i] = (cube[i].vol>1) ? Var(&cube[i]) : 0.0f;
		}
		else
		{
			vv[next] = 0.0;   // don't try to split this Box again
			i--;              // didn't create Box i
		}
		next = 0;
		temp = vv[0];
		for (k = 1; (int)k <= i; ++k)
		{
			if (vv[k] > temp) 
				temp = vv[k];
			next = k;
		}
		
		if (temp <= 0.0)
		{
			// only got K boxes
			K = i + 1;
			break;
		}
	}

	tag = (byte*)Mem_Alloc( Sys.imagepool, 33 * 33 * 33 * sizeof(byte));

	for (k = 0; (int)k < K; k++)
	{
		Mark(&cube[k], k, tag);
		weight = Vol(&cube[k], wt);
		if (weight)
		{
			lut_r[k] = (byte)(Vol(&cube[k], mr) / weight);
			lut_g[k] = (byte)(Vol(&cube[k], mg) / weight);
			lut_b[k] = (byte)(Vol(&cube[k], mb) / weight);
		}
		else
		{
			// bogus box
			lut_r[k] = lut_g[k] = lut_b[k] = 0;		
		}
	}
	for (i = 0; i < (int)size; i++) NewData[i] = tag[Qadd[i]];

	Mem_Free( tag );
	Mem_Free( Qadd);

	// fill palette
	for (k = 0; k < numcolors; k++)
	{
		Palette[k * 3]     = lut_b[k];
		Palette[k * 3 + 1] = lut_g[k];
		Palette[k * 3 + 2] = lut_r[k];
	}

	Mem_Free( Ig );
	Mem_Free( Ib );
	Mem_Free( Ir );

	newpix = (rgbdata_t *)Mem_Alloc( Sys.imagepool, sizeof(rgbdata_t));
	newpix->width = pix->width;
	newpix->height = pix->height;
	newpix->numLayers = pix->numLayers;
	newpix->numMips = pix->numMips;
	newpix->type = PF_INDEXED_24;	// FIXME: do alpha-channel
	newpix->flags = pix->flags;
	newpix->palette = Palette;
	newpix->buffer = NewData;
	newpix->size = pix->width * pix->height * pix->numLayers;
	//FS_FreeImage( pix ); // free old image

	return newpix;
}