//=======================================================================
//			Copyright XashXT Group 2007 ?
//		        stdlib.c - std lib portable utils
//=======================================================================

#include "launch.h"
#include "mathlib.h"

void com_strnupr(const char *in, char *out, size_t size_out)
{
	if (size_out == 0) return;

	while (*in && size_out > 1)
	{
		if (*in >= 'a' && *in <= 'z')
			*out++ = *in++ + 'A' - 'a';
		else *out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

void com_strupr(const char *in, char *out)
{
	com_strnupr(in, out, 99999 );
}

void com_strnlwr(const char *in, char *out, size_t size_out)
{
	if (size_out == 0) return;

	while (*in && size_out > 1)
	{
		if (*in >= 'A' && *in <= 'Z')
			*out++ = *in++ + 'a' - 'A';
		else *out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

void com_strlwr(const char *in, char *out)
{
	com_strnlwr(in, out, 99999 );
}

/*
==============
isdigit
==============
*/
bool com_isdigit( const char *str )
{
	if( str && *str )
	{
		while( isdigit( *str )) str++;
		if( !*str ) return true;
	}
	return false;
}

/*
============
strlen

returned string length
============
*/
int com_strlen( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		p++;
		len++;
	}
	return len;
}

/*
============
cstrlen

skipped color prefixes
============
*/
int com_cstrlen( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		if(IsColorString( p ))
		{
			p += 2;
			continue;
		}
		p++;
		len++;
	}
	return len;
}

char com_toupper(const char in )
{
	char out;

	if (in >= 'a' && in <= 'z')
		out = in + 'A' - 'a';
	else out = in;

	return out;
}

char com_tolower(const char in )
{
	char out;

	if (in >= 'A' && in <= 'Z')
		out = in + 'a' - 'A';
	else out = in;

	return out;
}

size_t com_strncat(char *dst, const char *src, size_t size)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = size;
	size_t dlen;

	if(!dst || !src || !size) return 0;

	// find the end of dst and adjust bytes left but don't go past end
	while(n-- != 0 && *d != '\0') d++;
	dlen = d - dst;
	n = size - dlen;

	if (n == 0) return(dlen + com_strlen(s));
	while (*s != '\0')
	{
		if (n != 1)
		{
			*d++ = *s;
			n--;
		}
		s++;
	}

	*d = '\0';
	return(dlen + (s - src)); // count does not include NULL
}

size_t com_strcat(char *dst, const char *src )
{
	return com_strncat( dst, src, 99999 );
}

size_t com_strncpy(char *dst, const char *src, size_t size)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = size;

	if(!dst || !src || !size) return 0;

	// copy as many bytes as will fit
	if (n != 0 && --n != 0)
	{
		do
		{
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	// not enough room in dst, add NULL and traverse rest of src
	if (n == 0)
	{
		if (size != 0) *d = '\0'; // NULL-terminate dst
		while (*s++);
	}
	return(s - src - 1); // count does not include NULL
}

size_t com_strcpy(char *dst, const char *src )
{
	return com_strncpy( dst, src, 99999 );
}

char *com_stralloc( byte *mempool, const char *s, const char *filename, int fileline )
{
	char	*b;

	if( !s ) return NULL;
	if( !mempool ) mempool = Sys.stringpool;

	MsgDev( D_STRING, "new system string %p\n", s );
	b = _mem_alloc( mempool, com_strlen(s) + 1, filename, fileline );
	com_strcpy( b, s );

	return b;
}

int com_atoi(const char *str)
{
	int       val = 0;
	int	c, sign;

	if( !str ) return 0;
	
	if(*str == '-')
	{
		sign = -1;
		str++;
	}
	else sign = 1;
		
	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while(1)
		{
			c = *str++;
			if (c >= '0' && c <= '9') val = (val<<4) + c - '0';
			else if (c >= 'a' && c <= 'f') val = (val<<4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') val = (val<<4) + c - 'A' + 10;
			else return val * sign;
		}
	}
	
	// check for character
	if (str[0] == '\'') return sign * str[1];
	
	// assume decimal
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val * sign;
		val = val*10 + c - '0';
	}
	return 0;
}

float com_atof(const char *str)
{
	double	val = 0;
	int	c, sign, decimal, total;

	if( !str ) return 0.0f;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else sign = 1;
		
	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9') val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f') val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') val = (val * 16) + c - 'A' + 10;
			else return val * sign;
		}
	}
	
	// check for character
	if (str[0] == '\'') return sign * str[1];
	
	// assume decimal
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}
		if (c <'0' || c > '9') break;
		val = val*10 + c - '0';
		total++;
	}

	if(decimal == -1) return val * sign;
	while (total > decimal)
	{
		val /= 10;
		total--;
	}
	
	return val * sign;
}

void com_atov( float *vec, const char *str, size_t siz )
{
	string	buffer;
	char	*pstr, *pfront;
	int	j;

	com.strncpy( buffer, str, MAX_STRING );
	Mem_Set( vec, 0, sizeof(vec_t) * siz );
	pstr = pfront = buffer;

	for ( j = 0; j < siz; j++ )
	{
		vec[j] = com.atof( pfront );

		// valid separator is space
		while( *pstr && *pstr != ' ' )
			pstr++;

		if (!*pstr) break;
		pstr++;
		pfront = pstr;
	}
}

/*
============
strchr

find one charcster in string
============
*/
char *com_strchr( const char *s, char c )
{
	int	len = com_strlen(s);

	while(len--) if(*++s == c) return(char *)s;
	return 0;
}

/*
============
strrchr

find one charcster in string
============
*/
char *com_strrchr( const char *s, char c )
{
	int	len = com_strlen(s);
	s += len;
	while(len--) if(*--s == c) return (char *)s;
	return 0;
}

int com_strnicmp(const char *s1, const char *s2, int n)
{
	int             c1, c2;

	if( s1 == NULL )
	{
		if ( s2 == NULL ) return 0;
		else return -1;
	}
	else if ( s2 == NULL ) return 1;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if(!n--) return 0; // strings are equal until end point
		
		if(c1 != c2)
		{
			if(c1 >= 'a' && c1 <= 'z') c1 -= ('a' - 'A');
			if(c2 >= 'a' && c2 <= 'z') c2 -= ('a' - 'A');
			if(c1 != c2) return c1 < c2 ? -1 : 1;
		}
	} while (c1);

	// strings are equal
	return 0;
}

int com_strncmp (const char *s1, const char *s2, int n)
{
	int		c1, c2;

	if( s1 == NULL )
	{
		if ( s2 == NULL ) return 0;
		else return -1;
	}
	else if ( s2 == NULL ) return 1;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		// strings are equal until end point
		if (!n--) return 0;
		if (c1 != c2) return c1 < c2 ? -1 : 1;

	} while (c1);
	
	// strings are equal
	return 0;
}

int com_stricmp(const char *s1, const char *s2)
{
	return com_strnicmp(s1, s2, 99999 );
}

int com_strcmp (const char *s1, const char *s2)
{
	return com_strncmp(s1, s2, 99999 );
}

/*
==============
Q_WildCmpAfterStar
==============
*/
static bool com_starcmp( const char *pattern, const char *text )
{
	char		c, c1;
	const char	*p = pattern, *t = text;

	while(( c = *p++ ) == '?' || c == '*' )
	{
		if( c == '?' && *t++ == '\0' )
			return false;
	}

	if( c == '\0' ) return true;

	for( c1 = ((c == '\\') ? *p : c); ; )
	{
		if( com_tolower( *t ) == c1 && com_stricmpext( p - 1, t ))
			return true;
		if( *t++ == '\0' ) return false;
	}
}

/*
==============
stricmpext
==============
*/
bool com_stricmpext( const char *pattern, const char *text )
{
	char	c;

	while( (c = *pattern++) != '\0' )
	{
		switch( c )
		{
			case '?':
				if( *text++ == '\0' )
					return false;
				break;
			case '\\':
				if( com_tolower( *pattern++ ) != com_tolower( *text++ ))
					return false;
				break;
			case '*':
				return com_starcmp( pattern, text );
			default:
				if( com_tolower( c ) != com_tolower( *text++ ))
					return false;
		}
	}
	return (*text == '\0');
}

/*
====================
timestamp
====================
*/
const char* com_timestamp( int format )
{
	static string	timestamp;
	time_t		crt_time;
	const struct tm	*crt_tm;
	string		timestring;

	time( &crt_time );
	crt_tm = localtime( &crt_time );
	switch( format )
	{
	case TIME_FULL:
		// Build the full timestamp (ex: "Apr03 2007 [23:31.55]");
		strftime(timestring, sizeof (timestring), "%b%d %Y [%H:%M.%S]", crt_tm);
		break;
	case TIME_DATE_ONLY:
		// Build the date stamp only (ex: "Apr03 2007");
		strftime(timestring, sizeof (timestring), "%b%d %Y", crt_tm);
		break;
	case TIME_TIME_ONLY:
		// Build the time stamp only (ex: "23:31.55");
		strftime(timestring, sizeof (timestring), "%H:%M.%S", crt_tm);
		break;
	case TIME_NO_SECONDS:
		// Build the time stamp exclude seconds (ex: "13:46");
		strftime(timestring, sizeof (timestring), "%H:%M", crt_tm);
		break;
	case TIME_YEAR_ONLY:
		// Build the date stamp year only (ex: "2006");
		strftime(timestring, sizeof (timestring), "%Y", crt_tm);
		break;
	case TIME_FILENAME:
		// Build a timestamp that can use for filename (ex: "Nov2006-26 (19.14.28)");
		strftime(timestring, sizeof (timestring), "%b%Y-%d_%H.%M.%S", crt_tm);
		break;
	default: return NULL;
	}

	com.strncpy( timestamp, timestring, MAX_STRING );
	return timestamp;
}

/*
============
strstr

search case - sensitive for string2 in string
============
*/
char *com_strstr( const char *string, const char *string2 )
{
	return strstr( string, string2 ); //FIXME
}

/*
============
stristr

search case - insensitive for string2 in string
============
*/
char *com_stristr( const char *string, const char *string2 )
{
	int c, len;

	if (!string || !string2) return NULL;

	c = com_tolower( *string2 );
	len = com_strlen( string2 );

	while (string)
	{
		for ( ; *string && com_tolower( *string ) != c; string++ );
		if (*string)
		{
			if(!com_strnicmp( string, string2, len ))
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
}

size_t com_strpack( byte *buffer, size_t pos, char *string, int n )
{
	if(!buffer || !string) return 0;

	n++; // get space for terminator	

	com_strncpy(buffer + pos, string, n ); 
	return pos + n;
}

size_t com_strunpack( byte *buffer, size_t pos, char *string )
{
	int	n = 0;
	char	*in;

	if(!buffer || !string) return 0;
	in = buffer + pos;

	do { in++, n++; } while(*in != '\0' && in != NULL );

	com_strncpy( string, in - (n - 1), n ); 
	return pos + n;
}

int com_vsnprintf(char *buffer, size_t buffersize, const char *format, va_list args)
{
	int result;

	result = _vsnprintf (buffer, buffersize, format, args);
	if (result < 0 || (size_t)result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}
	return result;
}

int com_vsprintf(char *buffer, const char *format, va_list args)
{
	return com_vsnprintf(buffer, 99999, format, args);
}

int com_snprintf(char *buffer, size_t buffersize, const char *format, ...)
{
	va_list args;
	int result;

	va_start (args, format);
	result = com_vsnprintf (buffer, buffersize, format, args);
	va_end (args);

	return result;
}

int com_sprintf(char *buffer, const char *format, ...)
{
	va_list	args;
	int	result;

	va_start (args, format);
	result = com_vsnprintf (buffer, 99999, format, args);
	va_end (args);

	return result;
}

char *com_pretifymem( float value, int digitsafterdecimal )
{
	static char output[8][32];
	static int  current;

	float	onekb = 1024.0f;
	float	onemb = onekb * onekb;
	char	suffix[8];
	char	*out = output[current];
	char	val[32], *i, *o, *dot;
	int	pos;

	current = ( current + 1 ) & ( 8 - 1 );

	// first figure out which bin to use
	if ( value > onemb )
	{
		value /= onemb;
		com_sprintf( suffix, " Mb" );
	}
	else if ( value > onekb )
	{
		value /= onekb;
		com_sprintf( suffix, " Kb" );
	}
	else com_sprintf( suffix, " bytes" );

	// clamp to >= 0
	digitsafterdecimal = max( digitsafterdecimal, 0 );
	// if it's basically integral, don't do any decimals
	if(fabs( value - (int)value ) < 0.00001)
	{
		com_sprintf( val, "%i%s", (int)value, suffix );
	}
	else
	{
		char fmt[32];

		// otherwise, create a format string for the decimals
		com_sprintf( fmt, "%%.%if%s", digitsafterdecimal, suffix );
		com_sprintf( val, fmt, value );
	}

	// copy from in to out
	i = val;
	o = out;

	// search for decimal or if it was integral, find the space after the raw number
	dot = strstr( i, "." );
	if ( !dot ) dot = strstr( i, " " );
	pos = dot - i;	// compute position of dot
	pos -= 3;		// don't put a comma if it's <= 3 long

	while ( *i )
	{
		// if pos is still valid then insert a comma every third digit, except if we would be
		// putting one in the first spot
		if ( pos >= 0 && !( pos % 3 ))
		{
			// never in first spot
			if ( o != out ) *o++ = ',';
		}
		pos--;		// count down comma position
		*o++ = *i++;	// copy rest of data as normal
	}
	*o = 0; // terminate

	return out;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va( const char *format, ... )
{
	va_list		argptr;
	static char	string[256][1024], *s;	// g-cont. 256 temporary strings should be enough...
	static int	stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 255;
	va_start( argptr, format );
	com_vsnprintf( s, sizeof(string[0]), format, argptr );
	va_end( argptr );
	return s;
}