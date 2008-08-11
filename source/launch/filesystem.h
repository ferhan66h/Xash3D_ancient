//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//			zip32.h - zlib custom build
//=======================================================================
#ifndef COM_ZLIB_H
#define COM_ZLIB_H

/*
========================================================================

.PCX image format	(ZSoft Paintbrush)

========================================================================
*/
typedef struct
{
	char	manufacturer;
	char	version;
	char	encoding;
	char	bits_per_pixel;
	word	xmin,ymin,xmax,ymax;
	word	hres,vres;
	byte	palette[48];
	char	reserved;
	char	color_planes;
	word	bytes_per_line;
	word	palette_type;
	char	filler[58];
} pcx_t;

/*
========================================================================

.WAL image format	(Wally textures)

========================================================================
*/
typedef struct wal_s
{
	char	name[32];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
	char	animname[32];	// next frame in animation chain
	int	flags;
	int	contents;
	int	value;
} wal_t;

/*
========================================================================

.LMP image format	(Quake1 gfx lumps)

========================================================================
*/
typedef struct flat_s
{
	short	width;
	short	height;
	short	desc[2];		// probably not used
} flat_t;

/*
========================================================================

.BMP image format

========================================================================
*/
typedef struct
{
	char	id[2];		//bmfh.bfType
	dword	fileSize;		//bmfh.bfSize
	dword	reserved0;	//bmfh.bfReserved1 + bmfh.bfReserved2
	dword	bitmapDataOffset;	//bmfh.bfOffBits
	dword	bitmapHeaderSize;	//bmih.biSize
	dword	width;		//bmih.biWidth
	dword	height;		//bmih.biHeight
	word	planes;		//bmih.biPlanes
	word	bitsPerPixel;	//bmih.biBitCount
	dword	compression;	//bmih.biCompression
	dword	bitmapDataSize;	//bmih.biSizeImage
	dword	hRes;		//bmih.biXPelsPerMeter
	dword	vRes;		//bmih.biYPelsPerMeter
	dword	colors;		//bmih.biClrUsed
	dword	importantColors;	//bmih.biClrImportant
	byte	palette[256][4];	//RGBQUAD palette
} bmp_t;

/*
========================================================================

.TGA image format	(Truevision Targa)

========================================================================
*/
typedef struct tga_s
{
	byte	id_length;
	byte	colormap_type;
	byte	image_type;
	word	colormap_index;
	word	colormap_length;
	byte	colormap_size;
	word	x_origin;
	word	y_origin;
	word	width;
	word	height;
	byte	pixel_size;
	byte	attributes;
} tga_t;

/*
========================================================================

.JPG image format

========================================================================
*/
typedef struct huffman_table_s
{
	// Huffman coding tables
	byte	bits[16];
	byte	hval[256];
	byte	size[256];
	word	code[256];

} huffman_table_t;

typedef struct jpg_s
{
	// not a real header
	file_t	*file;		// file
	byte	*buffer;		// jpg buffer
	
	int	width;		// width image
	int	height;		// height image
	byte	*data;		// image
	int	data_precision;	// bit per component
	int	num_components;	// number component
	int	restart_interval;	// restart interval
	bool	progressive_mode;	// progressive format

	struct
	{
		int     id;	// identifier
		int     h;	// horizontal sampling factor
		int     v;	// vertical sampling factor
		int     t;	// quantization table selector
		int     td;	// DC table selector
		int     ta;	// AC table selector
	} component_info[3];	// RGB (alpha not supported)
    
	huffman_table_t hac[4];	// AC table
	huffman_table_t hdc[4];	// DC table

	int	qtable[4][64];	// quantization table

	struct
	{
		int     ss,se;	// progressive jpeg spectral selection
		int     ah,al;	// progressive jpeg successive approx
	} scan;

	int	dc[3];
	int	curbit;
	byte	curbyte;

} jpg_t;

/*
========================================================================

.DDS image format

========================================================================
*/
#define DDSHEADER	((' '<<24)+('S'<<16)+('D'<<8)+'D') // little-endian "DDS "

// various four-cc types
#define TYPE_DXT1	(('1'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT1"
#define TYPE_DXT3	(('3'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT3"
#define TYPE_DXT5	(('5'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT5"
#define TYPE_$	(('\0'<<24)+('\0'<<16)+('\0'<<8)+'$') // little-endian "$"
#define TYPE_t	(('\0'<<24)+('\0'<<16)+('\0'<<8)+'t') // little-endian "t"

#define DDS_CAPS				0x00000001L
#define DDS_HEIGHT				0x00000002L
#define DDS_WIDTH				0x00000004L

#define DDS_RGB				0x00000040L
#define DDS_PIXELFORMAT			0x00001000L
#define DDS_LUMINANCE			0x00020000L

#define DDS_ALPHAPIXELS			0x00000001L
#define DDS_ALPHA				0x00000002L
#define DDS_FOURCC				0x00000004L
#define DDS_PITCH				0x00000008L
#define DDS_COMPLEX				0x00000008L
#define DDS_CUBEMAP				0x00000200L
#define DDS_TEXTURE				0x00001000L
#define DDS_MIPMAPCOUNT			0x00020000L
#define DDS_LINEARSIZE			0x00080000L
#define DDS_VOLUME				0x00200000L
#define DDS_MIPMAP				0x00400000L
#define DDS_DEPTH				0x00800000L

#define DDS_CUBEMAP_POSITIVEX			0x00000400L
#define DDS_CUBEMAP_NEGATIVEX			0x00000800L
#define DDS_CUBEMAP_POSITIVEY			0x00001000L
#define DDS_CUBEMAP_NEGATIVEY			0x00002000L
#define DDS_CUBEMAP_POSITIVEZ			0x00004000L
#define DDS_CUBEMAP_NEGATIVEZ			0x00008000L

typedef struct dds_pf_s
{
	uint	dwSize;
	uint	dwFlags;
	uint	dwFourCC;
	uint	dwRGBBitCount;
	uint	dwRBitMask;
	uint	dwGBitMask;
	uint	dwBBitMask;
	uint	dwABitMask;
} dds_pixf_t;

//  DDCAPS2
typedef struct dds_caps_s
{
	uint	dwCaps1;
	uint	dwCaps2;
	uint	dwCaps3;
	uint	dwCaps4;
} dds_caps_t;

typedef struct dds_s
{
	uint		dwIdent;		// must matched with DDSHEADER
	uint		dwSize;
	uint		dwFlags;		// determines what fields are valid
	uint		dwHeight;
	uint		dwWidth;
	uint		dwLinearSize;	// Formless late-allocated optimized surface size
	uint		dwDepth;		// depth if a volume texture
	uint		dwMipMapCount;	// number of mip-map levels requested
	uint		dwAlphaBitDepth;	// depth of alpha buffer requested
	float		fReflectivity[3];	// average reflectivity value
	float		fBumpScale;	// bumpmapping scale factor
	uint		dwReserved1[6];	// reserved for future expansions
	dds_pixf_t	dsPixelFormat;
	dds_caps_t	dsCaps;
	uint		dwTextureStage;
} dds_t;

/*
========================================================================
PAK FILES

The .pak files are just a linear collapse of a directory tree
========================================================================
*/
// header
#define IDPACKV1HEADER	(('K'<<24)+('C'<<16)+('A'<<8)+'P')	// little-endian "PACK"
#define IDPACKV2HEADER	(('2'<<24)+('K'<<16)+('A'<<8)+'P')	// little-endian "PAK2"
#define IDPACKV3HEADER	(('\4'<<24)+('\3'<<16)+('K'<<8)+'P')	// little-endian "PK\3\4"
#define IDPK3CDRHEADER	(('\2'<<24)+('\1'<<16)+('K'<<8)+'P')	// little-endian "PK\1\2"
#define IDPK3ENDHEADER	(('\6'<<24)+('\5'<<16)+('K'<<8)+'P')	// little-endian "PK\5\6"

#define MAX_FILES_IN_PACK	65536 // pack\pak2

typedef struct
{
	int		ident;
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char		name[56];		// total 64 bytes
	int		filepos;
	int		filelen;
} dpackfile_t;

typedef struct
{
	char		name[116];	// total 128 bytes
	int		filepos;
	int		filelen;
	uint		attribs;		// file attributes
} dpak2file_t;

typedef struct
{
	int		ident;
	word		disknum;
	word		cdir_disknum;	// number of the disk with the start of the central directory
	word		localentries;	// number of entries in the central directory on this disk
	word		nbentries;	// total number of entries in the central directory on this disk
	uint		cdir_size;	// size of the central directory
	uint		cdir_offset;	// with respect to the starting disk number
	word		comment_size;
} dpak3file_t;

#define IDIWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'I')	// little-endian "IWAD" doom1 game wad
#define IDPWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'P')	// little-endian "PWAD" doom1 game wad
#define IDWAD2HEADER	(('2'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD2" quake1 gfx.wad

#define TYPE_FLMP		59 // doom1 hud picture (doom1 mapped lump)
#define TYPE_SND		60 // doom1 wav sound (doom1 mapped lump)
#define TYPE_MUS		61 // doom1 music file (doom1 mapped lump)
#define TYPE_SKIN		62 // doom1 sprite model (doom1 mapped lump)
#define TYPE_FLAT		63 // doom1 wall texture (doom1 mapped lump)

// doom1 and doom2 lump header
typedef struct
{
	int		filepos;
	int		size;
	char		name[8];		// null not included
} dlumpfile_t;

#define ZLIB_VERSION	"1.2.3"
#define MAX_WBITS		15

// zlib errors
#define Z_OK		0
#define Z_STREAM_END	1
#define Z_SYNC_FLUSH	2
#define Z_FINISH		4

typedef struct z_stream_s
{
	byte		*next_in;		// next input byte
	uint		avail_in;		// number of bytes available at next_in
	dword		total_in;		// total nb of input bytes read so far
	byte		*next_out;	// next output byte should be put there
	uint		avail_out;	// remaining free space at next_out
	dword		total_out;	// total nb of bytes output so far
	char		*msg;		// last error message, NULL if no error
	byte		*state;		// not visible by applications
	byte*		(*zalloc)();	// used to allocate the internal state
	void		(*zfree)();	// used to free the internal state
	byte*		opaque;		// private data object passed to zalloc and zfree
	int		data_type;	// best guess about the data type: binary or text
	dword		adler;		// adler32 value of the uncompressed data
	dword		reserved;		// reserved for future use
} z_stream;

// zlib exported functions
extern int inflate(z_stream *strm, int flush);
extern int inflateEnd(z_stream *strm);
extern int inflateInit_(z_stream *strm, const char *version, int stream_size);
extern int inflateInit2_(z_stream *strm, int windowBits, const char *version, int stream_size);
extern int inflateReset(z_stream *strm);
extern int deflate (z_stream *strm, int flush);
extern int deflateEnd (z_stream *strm);
extern int deflateInit_(z_stream *strm, int level, const char *version, int stream_size);
extern int deflateInit2_ (z_stream *strm, int level, int method, int windowBits, int memLevel, int strategy, const char *version, int stream_size);
extern int deflateReset (z_stream *strm);

#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))
#define deflateInit(strm, level) deflateInit_((strm), (level), ZLIB_VERSION, sizeof(z_stream))
#define deflateInit2(strm, level, method, windowBits, memLevel, strategy) deflateInit2_((strm),(level),(method),(windowBits),(memLevel),(strategy), ZLIB_VERSION, sizeof(z_stream))

// imagelib definitions
#define IMAGE_MAXWIDTH	4096
#define IMAGE_MAXHEIGHT	4096
#define LERPBYTE(i)		r = resamplerow1[i];out[i] = (byte)((((resamplerow2[i] - r) * lerp)>>16) + r)
#define LUMP_NORMAL		0
#define LUMP_TRANSPARENT	1
#define LUMP_DECAL		2
#define LUMP_QFONT		3

extern int image_width, image_height;
extern byte image_num_layers;	// num layers in
extern byte image_num_mips;	// build mipmaps
extern uint image_type;	// main type switcher
extern uint image_flags;	// additional image flags
extern byte image_bits_count;	// bits per RGBA
extern size_t image_size;	// image rgba size
extern byte *image_rgba;	// image pointer (see image_type for details)
extern byte *image_palette;	// palette pointer
extern uint *d_currentpal;	// installed version of internal palette
extern cvar_t *img_oldformats;
extern cvar_t *fs_wadsupport;

bool Image_AddMipmapToPack( const byte *in, int width, int height, bool expand );
void Image_RoundDimensions( int *scaled_width, int *scaled_height );
byte *Image_ResampleInternal( const void *indata, int inwidth, int inheight, int outwidth, int outheight, int intype );
void Image_FreeImage( rgbdata_t *pack );
void Image_Save( const char *filename, rgbdata_t *pix );
rgbdata_t *Image_Load(const char *filename, const byte *buffer, size_t buffsize );
bool Image_Copy8bitRGBA( const byte *in, byte *out, int pixels );
bool FS_AddMipmapToPack( const byte *in, int width, int height, bool expand );
vec_t Image_NormalizeColor( vec3_t in, vec3_t out );
bool Image_DecompressDXTC( rgbdata_t **image );
bool Image_DecompressARGB( rgbdata_t **image );
void Image_GetPaletteLMP( const byte *pal, int rendermode );
void Image_GetPalettePCX( const byte *pal );
void Image_CopyPalette24bit( void );
void Image_CopyPalette32bit( void );
void Image_GetPaletteQ2( void );
void Image_GetPaletteQ1( void );
void Image_GetPaletteD1( void );	// doom 2 on TNT :)


//
// formats load
//
bool Image_LoadMIP( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadTGA( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadDDS( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadBMP( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadJPG( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadPNG( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadPCX( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadLMP( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadWAL( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadFLT( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadPAL( const char *name, const byte *buffer, size_t filesize );

//
// formats save
//
bool Image_SaveTGA( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SaveDDS( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SaveBMP( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SavePNG( const char *name, rgbdata_t *pix, int saveformat );

//
// img_utils.c
//
bool Image_ValidSize( const char *name );
bool Image_LumpValidSize( const char *name );
bool Image_DecompressDXTC( rgbdata_t **image );	// compilers version of decompressor
bool Image_DecompressARGB( rgbdata_t **image );

#endif//COM_ZLIB_H