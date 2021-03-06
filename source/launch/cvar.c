//=======================================================================
//			Copyright XashXT Group 2007 ?
//		       cvar.c -- dynamic variable tracking
//=======================================================================

#include "launch.h"

#define MAX_CVARS		2048
#define FILE_HASH_SIZE	256

int		cvar_numIndexes;
int		cvar_modifiedFlags;
cvar_t		cvar_indexes[MAX_CVARS];
static cvar_t	*hashTable[FILE_HASH_SIZE];
cvar_t		*cvar_vars;
cvar_t		*userinfo, *physinfo, *serverinfo;

/*
================
return a hash value for the filename
================
*/
static long Cvar_GetHashValue( const char *fname )
{
	int	i = 0;
	long	hash = 0;
	char	letter;

	while( fname[i] != '\0' )
	{
		letter = com.tolower( fname[i] );
		hash += (long)(letter)*(i + 119);
		i++;
	}

	hash &= (FILE_HASH_SIZE - 1);
	return hash;
}

/*
============
Cvar_InfoValidate
============
*/
static bool Cvar_ValidateString( const char *s, bool isvalue )
{
	if( !s ) return false;
	if( com.strstr( s, "\\" ) && !isvalue )
		return false;
	if( com.strstr( s, "\"" )) return false;
	if( com.strstr( s, ";" )) return false;
	return true;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar( const char *var_name )
{
	cvar_t	*var;
	long	hash;

	hash = Cvar_GetHashValue( var_name );
	
	for( var = hashTable[hash]; var; var = var->hash )
	{
		if (!com.stricmp( var_name, var->name ))
			return var;
	}
	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue( const char *var_name )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if( !var ) return 0;
	return var->value;
}

/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableInteger( const char *var_name )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if( !var ) return 0;
	return var->integer;
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString( const char *var_name )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if (!var) return "";
	return var->string;
}

/*
============
Cvar_LookupVars
============
*/
void Cvar_LookupVars( int checkbit, void *buffer, void *ptr, setpair_t callback )
{
	cvar_t	*cvar;

	// nothing to process ?
	if( !callback ) return;

	// force checkbit to 0 for lookup all cvars
	for( cvar = cvar_vars; cvar; cvar = cvar->next )
	{
		if( checkbit && !( cvar->flags & checkbit )) continue;
		if( buffer ) callback( cvar->name, cvar->string, buffer, ptr );
		else callback( cvar->name, cvar->string, cvar->description, ptr );
	}
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags, const char *var_desc )
{
	cvar_t	*var;
	long	hash;

	if( !var_name || !var_value )
	{
		MsgDev( D_ERROR, "Cvar_Get: NULL parameter" );
		return NULL;
	}
	
	if( !Cvar_ValidateString( var_name, false ))
	{
		MsgDev( D_WARN, "invalid info cvar name string %s\n", var_name );
		var_value = "noname";
	}
	if( !Cvar_ValidateString( var_value, true ))
	{
		MsgDev( D_WARN, "invalid cvar value string: %s\n", var_value );
		var_value = "default";
	}

	// check for command coexisting
	if( Cmd_Exists( var_name ))
	{
		MsgDev( D_WARN, "Cvar_Get: %s is a command\n", var_name );
		return NULL;
	}

	var = Cvar_FindVar( var_name );
	if( var )
	{
		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value

		if(( var->flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED ) && var_value[0] )
		{
			var->flags &= ~CVAR_USER_CREATED;
			Mem_Free( var->reset_string );
			var->reset_string = copystring( var_value );
			cvar_modifiedFlags |= flags;
		}

		var->flags |= flags;

		// only allow one non-empty reset string without a warning
		if( !var->reset_string )
		{
			// we don't have a reset string yet
			Mem_Free( var->reset_string );
			var->reset_string = copystring( var_value );
		}
		else if( var_value[0] && com.strcmp( var->reset_string, var_value ))
		{
			MsgDev( D_STRING, "cvar \"%s\" given initial values: \"%s\" and \"%s\"\n", var_name, var->reset_string, var_value );
		}

		// if we have a latched string, take that value now
		if( var->latched_string )
		{
			char *s;

			s = var->latched_string;
			var->latched_string = NULL; // otherwise cvar_set2 would free it
			Cvar_Set2( var_name, s, true );
			Mem_Free( s );
		}
		if( var_desc )
		{
			// update description if needs
			if( var->description ) Mem_Free( var->description );
			var->description = copystring( var_desc );
		}
		return var;
	}

	// allocate a new cvar
	if( cvar_numIndexes >= MAX_CVARS )
	{
		MsgDev( D_ERROR, "Cvar_Get: MAX_CVARS limit exceeded\n" );
		return NULL;
	}

	var = &cvar_indexes[cvar_numIndexes];
	cvar_numIndexes++;
	var->name = copystring( var_name );
	var->string = copystring( var_value );
	if( var_desc ) var->description = copystring( var_desc );

	var->modified = true;
	var->modificationCount = 1;
	var->value = com.atof( var->string );
	var->integer = com.atoi( var->string );
	var->reset_string = copystring( var_value );

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;
	hash = Cvar_GetHashValue( var_name );
	var->hash = hashTable[hash];
	hashTable[hash] = var;

	return var;
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2 (const char *var_name, const char *value, bool force)
{
	cvar_t	*var;

	if( !Cvar_ValidateString( var_name, false ))
	{
		MsgDev( D_WARN, "invalid cvar name string: %s\n", var_name );
		var_name = "unknown";
	}

	if ( value && !Cvar_ValidateString( value, true ))
	{
		MsgDev( D_WARN, "invalid cvar value string: %s\n", value );
		value = "default";
	}

	var = Cvar_FindVar( var_name );
	if( !var )
	{
		if( !value ) return NULL;

		// create it
		if( !force ) return Cvar_Get( var_name, value, CVAR_USER_CREATED, NULL );
		else return Cvar_Get (var_name, value, 0, NULL );
	}

	if( !value ) value = var->reset_string;
	if( !com.strcmp( value, var->string )) return var;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	if( !force )
	{
		if( var->flags & CVAR_READ_ONLY )
		{
			MsgDev( D_INFO, "%s is read only.\n", var_name );
			return var;
		}
		if( var->flags & CVAR_INIT )
		{
			MsgDev( D_INFO, "%s is write protected.\n", var_name );
			return var;
		}
		if( var->flags & CVAR_SYSTEMINFO )
		{
			MsgDev( D_INFO, "%s is system variable.\n", var_name );
			return var;
		}
		if( var->flags & ( CVAR_LATCH|CVAR_LATCH_VIDEO|CVAR_LATCH_AUDIO ))
		{
			if( var->latched_string )
			{
				if(!com.strcmp( value, var->latched_string ))
					return var;
				Mem_Free( var->latched_string );
			}
			else
			{
				if( !com.strcmp( value, var->string ))
					return var;
			}

			if( var->flags & CVAR_LATCH && Cvar_VariableInteger( "host_serverstate" ))
			{
				MsgDev( D_INFO, "%s will be changed upon restarting.\n", var->name );
				var->latched_string = copystring( value );
			}
			else if( var->flags & CVAR_LATCH_VIDEO )
			{
				MsgDev( D_INFO, "%s will be changed upon restarting video.\n", var->name );
				var->latched_string = copystring( value );
			}
			else if( var->flags & CVAR_LATCH_AUDIO )
			{
				MsgDev( D_INFO, "%s will be changed upon restarting audio.\n", var->name );
				var->latched_string = copystring( value );
			}
			else
			{
				Mem_Free( var->string );		// free the old value string
				var->string = copystring( value );
				var->value = com.atof( var->string );
				var->integer = com.atoi( var->string );
			}

			var->modified = true;
			var->modificationCount++;
			return var;
		}
		if(( var->flags & CVAR_CHEAT ) && !Cvar_VariableInteger( "sv_cheats" ))
		{
			MsgDev( D_INFO, "%s is cheat protected.\n", var_name );
			return var;
		}
	}
	else
	{
		if( var->latched_string )
		{
			Mem_Free( var->latched_string );
			var->latched_string = NULL;
		}
	}

 	// nothing to change
	if( !com.strcmp( value, var->string ))
		return var;

	var->modified = true;
	var->modificationCount++;

	if( var->flags & CVAR_USERINFO )
		userinfo->modified = true;	// transmit at next oportunity

	if( var->flags & CVAR_PHYSICINFO )
		physinfo->modified = true;	// transmit at next oportunity

	if( var->flags & CVAR_SERVERINFO )
		serverinfo->modified = true;	// transmit at next oportunity
	
	// free the old value string
	Mem_Free( var->string );
	var->string = copystring( value );
	var->value = com.atof( var->string );
	var->integer = com.atoi( var->string );

	return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value )
{
	Cvar_Set2( var_name, value, true );
}

/*
============
Cvar_SetLatched
============
*/
void Cvar_SetLatched( const char *var_name, const char *value )
{
	Cvar_Set2( var_name, value, false );
}

/*
============
Cvar_FullSet
============
*/
void Cvar_FullSet( char *var_name, char *value, int flags )
{
	cvar_t	*var;
	
	var = Cvar_FindVar( var_name );
	if( !var ) 
	{
		// create it
		Cvar_Get( var_name, value, flags, "" );
		return;
	}
	var->modified = true;

	if( var->flags & CVAR_USERINFO )
	{
		// transmit at next oportunity
		userinfo->modified = true;
	}	

	if( var->flags & CVAR_PHYSICINFO )
	{
		// transmit at next oportunity
		physinfo->modified = true;
	}

	Mem_Free( var->string ); // free the old value string
	var->string = copystring( value );
	var->value = com.atof( var->string );
	var->integer = com.atoi( var->string );
	var->flags = flags;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( const char *var_name, float value )
{
	char	val[32];

	if( value == (int)value )
		com.sprintf( val, "%i", (int)value );
	else com.sprintf( val, "%f", value );
	Cvar_Set( var_name, val );
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name )
{
	Cvar_Set2( var_name, NULL, false );
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void )
{
	cvar_t	*var;

	// set all default vars to the safe value
	for( var = cvar_vars; var; var = var->next )
	{
		if( var->flags & CVAR_CHEAT )
		{
			// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
			// because of a different var->latched_string
			if( var->latched_string )
			{
				Mem_Free( var->latched_string );
				var->latched_string = NULL;
			}
			if( com.strcmp( var->reset_string, var->string ))
			{
				Cvar_Set( var->name, var->reset_string );
			}
		}
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
bool Cvar_Command( void )
{
	cvar_t	*v;

	// check variables
	v = Cvar_FindVar( Cmd_Argv( 0 ));
	if( !v ) return false;

	// perform a variable print or set
	if( Cmd_Argc() == 1 )
	{
		if( v->flags & CVAR_INIT ) Msg( "%s: %s\n", v->name, v->string );
		else Msg( "%s: %s ( ^3%s^7 )\n", v->name, v->string, v->reset_string );
		if ( v->latched_string ) Msg( "%s: %s\n", v->name, v->latched_string );
		return true;
	}

	// set the value if forcing isn't required
	Cvar_Set2( v->name, Cmd_Argv(1), false );
	return true;
}

/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding
============
*/
void Cvar_Toggle_f( void )
{
	int	v;

	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: toggle <variable>\n" );
		return;
	}

	v = Cvar_VariableValue(Cmd_Argv( 1 ));
	v = !v;

	Cvar_Set2( Cmd_Argv( 1 ), va( "%i", v ), false );
}

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_Set_f( void )
{
	int	i, c, l = 0, len;
	char	combined[MAX_STRING_TOKENS];

	c = Cmd_Argc();
	if( c < 3 )
	{
		Msg( "Usage: set <variable> <value>\n" );
		return;
	}
	combined[0] = 0;

	for( i = 2; i < c; i++ )
	{
		len = com.strlen( Cmd_Argv(i) + 1 );
		if ( l + len >= MAX_STRING_TOKENS - 2 )
			break;
		com.strcat( combined, Cmd_Argv(i));
		if ( i != c-1 ) com.strcat( combined, " " );
		l += len;
	}
	Cvar_Set2( Cmd_Argv( 1 ), combined, false );
}

/*
============
Cvar_SetU_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetU_f( void )
{
	cvar_t	*v;

	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: setu <variable> <value>\n" );
		return;
	}

	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ));

	if( !v ) return;
	v->flags |= CVAR_USERINFO;
}

/*
============
Cvar_SetP_f

As Cvar_Set, but also flags it as physinfo
============
*/
void Cvar_SetP_f( void )
{
	cvar_t	*v;

	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: setp <variable> <value>\n" );
		return;
	}

	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ));

	if( !v ) return;
	v->flags |= CVAR_PHYSICINFO;
}

/*
============
Cvar_SetS_f

As Cvar_Set, but also flags it as serverinfo
============
*/
void Cvar_SetS_f( void )
{
	cvar_t	*v;

	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: sets <variable> <value>\n" );
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ));

	if( !v ) return;
	v->flags |= CVAR_SERVERINFO;
}

/*
============
Cvar_SetA_f

As Cvar_Set, but also flags it as archived
============
*/
void Cvar_SetA_f( void )
{
	cvar_t	*v;

	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: seta <variable> <value>\n" );
		return;
	}

	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ));

	if( !v ) return;
	v->flags |= CVAR_ARCHIVE;
}

/*
============
Cvar_SetC_f

As Cvar_Set, but also flags it as systeminfo
============
*/
void Cvar_SetC_f( void )
{
	if( Cmd_Argc() != 3 )
	{
		Msg( "Usage: setc <variable> <value>\n" );
		return;
	}

	Cvar_FullSet( Cmd_Argv( 1 ), Cmd_Argv( 2 ), CVAR_SYSTEMINFO );
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Msg( "Usage: reset <variable>\n" );
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ));
}

/*
============
Cvar_List_f
============
*/
void Cvar_List_f( void )
{
	cvar_t	*var;
	char	*match;
	int	i = 0, j = 0;

	if( Cmd_Argc() > 1 ) match = Cmd_Argv(1);
	else match = NULL;

	for( var = cvar_vars; var; var = var->next, i++ )
	{
		if( match && !com_stricmpext( match, var->name ))
			continue;

		if( var->flags & CVAR_SERVERINFO ) Msg( "SV   " );
		else Msg( " " );

		if( var->flags & CVAR_USERINFO ) Msg( "USER " );
		else Msg( " " );

		if( var->flags & CVAR_PHYSICINFO ) Msg( "PHYS " );
		else Msg( " " );

		if( var->flags & CVAR_READ_ONLY ) Msg( "READ " );
		else Msg( " " );

		if( var->flags & CVAR_INIT ) Msg( "INIT " );
		else Msg( " " );

		if( var->flags & CVAR_ARCHIVE ) Msg( "ARCH " );
		else Msg( " " );

		if( var->flags & CVAR_LATCH ) Msg( "LATCH" );
		else Msg( " " );

		if( var->flags & CVAR_LATCH_VIDEO ) Msg( "VIDEO" );
		else Msg( " " );

		if( var->flags & CVAR_LATCH_AUDIO ) Msg( "AUDIO" );
		else Msg( " " );

		if( var->flags & CVAR_CHEAT ) Msg( "CHEAT" );
		else Msg( " " );
		Msg(" %s \"%s\"\n", var->name, var->string );
		j++;
	}

	Msg( "\n%i cvars\n", j );
	Msg( "%i total cvars\n", i );
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f( void )
{
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;

	while( 1 )
	{
		var = *prev;
		if( !var ) break;

		// don't mess with rom values, or some inter-module
		// communication will get broken (com_cl_running, etc)
		if( var->flags & ( CVAR_READ_ONLY|CVAR_INIT|CVAR_SYSTEMINFO ))
		{
			prev = &var->next;
			continue;
		}

		// throw out any variables the user created
		if( var->flags & CVAR_USER_CREATED )
		{
			*prev = var->next;
			if( var->name ) Mem_Free( var->name );
			if( var->string ) Mem_Free( var->string );
			if( var->latched_string ) Mem_Free( var->latched_string );
			if( var->reset_string ) Mem_Free( var->reset_string );
			if( var->description ) Mem_Free( var->description );

			// clear the var completely, since we
			// can't remove the index from the list
			Mem_Set( var, 0, sizeof( var ));
			continue;
		}
		Cvar_Set( var->name, var->reset_string );
		prev = &var->next;
	}
}

/*
============
Cvar_Latched_f

Now all latched strings is valid
============
*/
void Cvar_Latched_f( void )
{
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;

	while( 1 )
	{
		var = *prev;
		if( !var ) break;

		if( var->flags & CVAR_LATCH && var->latched_string )
		{
			Cvar_FullSet( var->name, var->latched_string, var->flags );
			Mem_Free( var->latched_string );
			var->latched_string = NULL;
		}
		prev = &var->next;
	}
}

/*
============
Cvar_Latched_f

Now all latched video strings is valid
============
*/
void Cvar_LatchedVideo_f( void )
{
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;

	while ( 1 )
	{
		var = *prev;
		if( !var ) break;

		if( var->flags & CVAR_LATCH_VIDEO && var->latched_string )
		{
			Cvar_FullSet( var->name, var->latched_string, var->flags );
			Mem_Free( var->latched_string );
			var->latched_string = NULL;
		}
		prev = &var->next;
	}
}

/*
============
Cvar_Latched_f

Now all latched audio strings is valid
============
*/
void Cvar_LatchedAudio_f( void )
{
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;

	while ( 1 )
	{
		var = *prev;
		if( !var ) break;

		if( var->flags & CVAR_LATCH_AUDIO && var->latched_string )
		{
			Cvar_FullSet( var->name, var->latched_string, var->flags );
			Mem_Free( var->latched_string );
			var->latched_string = NULL;
		}
		prev = &var->next;
	}
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init( void )
{
	cvar_vars = NULL;
	cvar_numIndexes = cvar_modifiedFlags = 0;
	ZeroMemory( cvar_indexes, sizeof( cvar_t ) * MAX_CVARS );
	ZeroMemory( hashTable, sizeof( *hashTable ) * FILE_HASH_SIZE );
	userinfo = Cvar_Get( "@userinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	physinfo = Cvar_Get( "@physinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only
	serverinfo = Cvar_Get( "@serverinfo", "0", CVAR_READ_ONLY, "" ); // use ->modified value only

	Cmd_AddCommand ("toggle", Cvar_Toggle_f, "toggles a console variable's values (use for more info)" );
	Cmd_AddCommand ("set", Cvar_Set_f, "create or change the value of a console variable" );
	Cmd_AddCommand ("sets", Cvar_SetS_f, "create or change the value of a serverinfo variable" );
	Cmd_AddCommand ("setu", Cvar_SetU_f, "create or change the value of a userinfo variable" );
	Cmd_AddCommand ("setp", Cvar_SetP_f, "create or change the value of a physicinfo variable" );
	Cmd_AddCommand ("setc", Cvar_SetC_f, "create or change the value of a systeminfo variable" );
	Cmd_AddCommand ("seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to vars.rc" );
	Cmd_AddCommand ("reset", Cvar_Reset_f, "reset any type variable to initial value" );
	Cmd_AddCommand ("latch", Cvar_Latched_f, "apply latched values" );
	Cmd_AddCommand ("vidlatch", Cvar_LatchedVideo_f, "apply latched values for video subsystem" );
	Cmd_AddCommand ("sndlatch", Cvar_LatchedAudio_f, "apply latched values for audio subsytem" );
	Cmd_AddCommand ("cvarlist", Cvar_List_f, "display all console variables beginning with the specified prefix" );
	Cmd_AddCommand ("unsetall", Cvar_Restart_f, "reset all console variables to their default values" );

}
