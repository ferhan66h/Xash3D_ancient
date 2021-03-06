//=======================================================================
//			Copyright XashXT Group 2010 ?
//			snd_ogg.c - ogg format load & save
//=======================================================================

#include "soundlib.h"

typedef __int64 ogg_int64_t;
typedef __int32 ogg_int32_t;
typedef unsigned __int32 ogg_uint32_t;
typedef __int16 ogg_int16_t;
typedef unsigned __int16 ogg_uint16_t;

/*
=======================================================================
		OGG PACK DEFINITION
=======================================================================
*/
struct ogg_chain_s
{
	void		*ptr;
	struct alloc_chain	*next;
};

typedef struct oggpack_buffer_s
{
	long		endbyte;
	int		endbit;
	byte		*buffer;
	byte		*ptr;
	long		storage;
} oggpack_buffer_t;

typedef struct ogg_sync_state_s
{
	byte		*data;
	int		storage;
	int		fill;
	int		returned;
	int		unsynced;
	int		headerbytes;
	int		bodybytes;
} ogg_sync_state_t;

typedef struct ogg_stream_state_s
{
	byte		*body_data;
	long		body_storage;
	long		body_fill;
	long		body_returned;
	int		*lacing_vals;
	ogg_int64_t	*granule_vals;
	long		lacing_storage;
	long		lacing_fill;
	long		lacing_packet;
	long		lacing_returned;
	byte		header[282];
	int		header_fill;
	int		e_o_s;
	int		b_o_s;
	long		serialno;
	long		pageno;
	ogg_int64_t	packetno;
	ogg_int64_t	granulepos;
} ogg_stream_state_t;

/*
=======================================================================
		VORBIS FILE DEFINITION
=======================================================================
*/
typedef struct vorbis_info_s
{
	int		version;
	int		channels;
	long		rate;
	long		bitrate_upper;
	long		bitrate_nominal;
	long		bitrate_lower;
	long		bitrate_window;
	void		*codec_setup;
} vorbis_info_t;

typedef struct vorbis_comment_s
{
	char		**user_comments;
	int		*comment_lengths;
	int		comments;
	char		*vendor;
} vorbis_comment_t;

typedef struct vorbis_dsp_state_s
{
	int		analysisp;
	vorbis_info_t	*vi;
	float		**pcm;
	float		**pcmret;
	int		pcm_storage;
	int		pcm_current;
	int		pcm_returned;
	int		preextrapolate;
	int		eofflag;
	long		lW;
	long		W;
	long		nW;
	long		centerW;
	ogg_int64_t	granulepos;
	ogg_int64_t	sequence;
	ogg_int64_t	glue_bits;
	ogg_int64_t	time_bits;
	ogg_int64_t	floor_bits;
	ogg_int64_t	res_bits;
	void		*backend_state;
} vorbis_dsp_state_t;

typedef struct vorbis_block_s
{
	float		**pcm;
	oggpack_buffer_t	opb;
	long		lW;
	long		W;
	long		nW;
	int		pcmend;
	int		mode;
	int		eofflag;
	ogg_int64_t	granulepos;
	ogg_int64_t	sequence;
	vorbis_dsp_state_t	*vd;
	void		*localstore;
	long		localtop;
	long		localalloc;
	long		totaluse;
	struct ogg_chain_s	*reap;
	long		glue_bits;
	long		time_bits;
	long		floor_bits;
	long		res_bits;
	void		*internal;
} vorbis_block_t;

typedef struct ov_callbacks_s
{
	size_t (*read_func)( void *ptr, size_t size, size_t nmemb, void *datasource );
	int (*seek_func)( void *datasource, ogg_int64_t offset, int whence );
	int (*close_func)( void *datasource );
	long (*tell_func)( void *datasource );
} ov_callbacks_t;

typedef struct
{
	byte		*buffer;
	ogg_int64_t	ind;
	ogg_int64_t	buffsize;
} ov_decode_t;

typedef struct vorbisfile_s
{
	void		*datasource;
	int		seekable;
	ogg_int64_t	offset;
	ogg_int64_t	end;
	ogg_sync_state_t	oy;
	int		links;
	ogg_int64_t	*offsets;
	ogg_int64_t	*dataoffsets;
	long		*serialnos;
	ogg_int64_t	*pcmlengths;
	vorbis_info_t	*vi;
	vorbis_comment_t	*vc;
	ogg_int64_t	pcm_offset;
	int		ready_state;
	long		current_serialno;
	int		current_link;
	double		bittrack;
	double		samptrack;
	ogg_stream_state_t	os;
	vorbis_dsp_state_t	vd;
	vorbis_block_t	vb;
	ov_callbacks_t	callbacks;

} vorbisfile_t;

// libvorbis exports
extern int ov_open_callbacks( void *datasrc, vorbisfile_t *vf, char *initial, long ibytes, ov_callbacks_t callbacks );
extern long ov_read( vorbisfile_t *vf, char *buffer, int length, int bigendianp, int word, int sgned, int *bitstream );
extern char *vorbis_comment_query( vorbis_comment_t *vc, char *tag, int count );
extern vorbis_comment_t *ov_comment( vorbisfile_t *vf, int link );
extern ogg_int64_t ov_pcm_total( vorbisfile_t *vf, int i );
extern vorbis_info_t *ov_info( vorbisfile_t *vf, int link );
extern int ov_raw_seek( vorbisfile_t *vf, ogg_int64_t pos );
extern ogg_int64_t ov_raw_tell( vorbisfile_t *vf );
extern int ov_clear( vorbisfile_t *vf);

static int ovc_close( void *datasrc ) { return 0; }	// close callback generic stub

/*
=================================================================

	Memory reading funcs

=================================================================
*/
static size_t ovcm_read( void *ptr, size_t size, size_t nb, void *datasrc )
{
	ov_decode_t	*oggfile = (ov_decode_t *)datasrc;
	size_t		remain, length;

	remain = oggfile->buffsize - oggfile->ind;
	length = size * nb;

	if( remain < length )
		length = remain - remain % size;

	Mem_Copy( ptr, oggfile->buffer + oggfile->ind, length );
	oggfile->ind += length;

	return length / size;
}

static int ovcm_seek( void *datasrc, ogg_int64_t offset, int whence )
{
	ov_decode_t	*oggfile = (ov_decode_t*)datasrc;

	switch( whence )
	{
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += oggfile->ind;
		break;
	case SEEK_END:
		offset += oggfile->buffsize;
		break;
	default:
		return -1;
	}

	if( offset < 0 || offset > oggfile->buffsize )
		return -1;

	oggfile->ind = offset;
	return 0;
}

static long ovcm_tell( void *datasrc )
{
	return ((ov_decode_t *)datasrc)->ind;
}

/*
=================================================================

	OGG decompression

=================================================================
*/
static size_t ovcf_read( void *ptr, size_t size, size_t nb, void *datasrc )
{
	stream_t	*track = (stream_t *)datasrc;

	if( !size || !nb )
		return 0;

	return FS_Read( track->file, ptr, size * nb ) / size;
}

static int ovcf_seek( void *datasrc, ogg_int64_t offset, int whence )
{
	stream_t	*track = (stream_t *)datasrc;

	switch( whence )
	{
	case SEEK_SET:
		FS_Seek( track->file, (int)offset, SEEK_SET );
		break;
	case SEEK_CUR:
		FS_Seek( track->file, (int)offset, SEEK_CUR );
		break;
	case SEEK_END:
		FS_Seek( track->file, (int)offset, SEEK_END );
		break;
	default:
		return -1;
	}
	return 0;
}

static long ovcf_tell( void *datasrc )
{
	stream_t	*track = (stream_t *)datasrc;

	return FS_Tell( track->file );
}

/*
=================================================================

	OGG decompression

=================================================================
*/

bool Sound_LoadOGG( const char *name, const byte *buffer, size_t filesize )
{
	vorbisfile_t	vf;
	vorbis_info_t	*vi;
	vorbis_comment_t	*vc;
	ov_decode_t	ov_decode;
	ov_callbacks_t	ov_callbacks = { ovcm_read, ovcm_seek, ovc_close, ovcm_tell };
	long		done = 0, ret;
	const char	*comm;
	int		dummy;

	// load the file
	if( !buffer || filesize <= 0 )
		return false;

	// Open it with the VorbisFile API
	ov_decode.buffer = (byte *)buffer;
	ov_decode.buffsize = filesize;
	ov_decode.ind = 0;

	if( ov_open_callbacks( &ov_decode, &vf, NULL, 0, ov_callbacks ) < 0 )
	{
		MsgDev( D_ERROR, "Sound_LoadOGG: couldn't open ogg stream %s\n", name );
		return false;
	}

	// get the stream information
	vi = ov_info( &vf, -1 );

	if( vi->channels != 1 )
	{
		MsgDev( D_ERROR, "Sound_LoadOGG: only mono OGG files supported (%s)\n", name );
		ov_clear( &vf );
		return false;
	}

	sound.channels = vi->channels;
	sound.rate = vi->rate;
	sound.width = 2; // always 16-bit PCM
	sound.loopstart = -1;
	sound.size = ov_pcm_total( &vf, -1 ) * vi->channels * 2;  // 16 bits => "* 2"

	if( !sound.size )
	{
		// bad ogg file
		MsgDev( D_ERROR, "Sound_LoadOGG: (%s) is probably corrupted\n", name );
		ov_clear( &vf );
		return false;
	}

	sound.type = WF_PCMDATA;
	sound.wav = (byte *)Mem_Alloc( Sys.soundpool, sound.size );

	// decompress ogg into pcm wav format
	while(( ret = ov_read( &vf, &sound.wav[done], (int)(sound.size - done), big_endian, 2, 1, &dummy )) > 0 )
		done += ret;
	sound.samples = done / ( vi->channels * 2 );
	vc = ov_comment( &vf, -1 );

	if( vc )
	{
		comm = vorbis_comment_query( vc, "LOOP_START", 0 );
		if( comm ) 
		{
			// FXIME: implement
			Msg( "ogg 'cue' %d\n", com.atoi(comm) );
			//sound.loopstart = bound( 0, com.atoi( comm ), sound.samples );
 		}
 	}

	// close file
	ov_clear( &vf );

	return true;
}

/*
=================
Stream_OpenOGG
=================
*/
stream_t *Stream_OpenOGG( const char *filename )
{
	vorbisfile_t	*vorbisFile;
	vorbis_info_t	*vorbisInfo;
	ov_callbacks_t	vorbisCallbacks = { ovcf_read, ovcf_seek, ovc_close, ovcf_tell };
	stream_t		*stream;
	file_t		*file;

	file = FS_Open( filename, "rb" );
	if( !file ) return NULL;

	// at this point we have valid stream
	stream = Mem_Alloc( Sys.soundpool, sizeof( stream_t ));
	stream->file = file;

	vorbisFile = Mem_Alloc( Sys.soundpool, sizeof( vorbisfile_t ));

	if( ov_open_callbacks( stream, vorbisFile, NULL, 0, vorbisCallbacks ) < 0 )
	{
		MsgDev( D_ERROR, "Stream_OpenOGG: couldn't open %s\n", filename );
		ov_clear( vorbisFile );
		Mem_Free( vorbisFile );
		Mem_Free( stream );
		FS_Close( file );
		return NULL;
	}

	vorbisInfo = ov_info( vorbisFile, -1 );
	if( vorbisInfo->channels != 1 && vorbisInfo->channels != 2 )
	{
		MsgDev( D_ERROR, "Stream_OpenOGG: only mono and stereo ogg files supported %s\n", filename );
		ov_clear( vorbisFile );
		Mem_Free( vorbisFile );
		Mem_Free( stream );
		FS_Close( file );
		return NULL;
	}

	stream->pos = ov_raw_tell( vorbisFile );
	stream->channels = vorbisInfo->channels;
	stream->width = 2;	// always 16 bit
	stream->rate = vorbisInfo->rate; // save rate instead of constant width
	stream->ptr = vorbisFile;
	stream->type = WF_OGGDATA;

	return stream;
}

/*
=================
Stream_ReadOGG

assume stream is valid
=================
*/
long Stream_ReadOGG( stream_t *stream, long bytes, void *buffer )
{
	// buffer handling
	int	bytesRead, bytesLeft;
	int	c, dummy;
	char	*bufPtr;

	bytesRead = 0;
	bytesLeft = bytes;
	bufPtr = buffer;

	// cycle until we have the requested or all available bytes read
	while( 1 )
	{
		// read some bytes from the OGG codec
		c = ov_read(( vorbisfile_t *)stream->ptr, bufPtr, bytesLeft, big_endian, 2, 1, &dummy );
		
		// no more bytes are left
		if( c <= 0 ) break;

		bytesRead += c;
		bytesLeft -= c;
		bufPtr += c;
  
		// we have enough bytes
		if( bytesLeft <= 0 )
			break;
	}
	return bytesRead;
}

/*
=================
Stream_FreeOGG

assume stream is valid
=================
*/
void Stream_FreeOGG( stream_t *stream )
{
	if( stream->ptr )
	{
		ov_clear( stream->ptr );
		Mem_Free( stream->ptr );
	}

	if( stream->file )
	{
		FS_Close( stream->file );
	}

	Mem_Free( stream );
}