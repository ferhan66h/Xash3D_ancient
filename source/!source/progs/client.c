/*
+------+
|Client|
+------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch         |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Handle's "clients" (eg, Players) connecting, disconnecting, etc.                  |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/

//DEFS;

void() CCamChasePlayer;      // From Ccam.qc	
void() CheckImpulses;	     // From Impulses.QC
void() PutClientInServer;  //From Client.QC

//END DEFS;

/*
+=========+
|PPRINT():|    
+=========+==============================================================================+
|Description:                                                                                                                   |
|This function prints a server wide(bprint) 'entity' 'did' 'what" message... v useful for all those|
|client joining, leaving msgs etc.. saves a fair amount of code i hope.. [8 lines i think..]               |
+========================================================================================+
*/

void(entity dude, string did, string what)pprint = 
{
	bprint("\n");
	bprint(dude.netname);
	bprint(did);
	bprint(what);
	bprint("\n");
};

/*
CLIENTRESPAWN();
*/

void() ClientRespawn = 
{
	if (coop)
	{
		// get the spawn parms as they were at level start
		GetLevelParms();
		// respawn		
		PutClientInServer();
	}
	else if (deathmatch)
	{
		// set default spawn parms
		SetNewParms();
		// respawn		
		PutClientInServer();
	}
	else
	{	// restart the entire server
		localcmd ("restart\n");
	}
};

/*
CLIENTOBITURARY()

Description;
Describes the entity 'who_died' in relation to enitity 'who_killed'.
Called when a player gets 'killed' by KILLED(); [DAMAGE.QC]
*/

void(entity who_died, entity who_killed) ClientObiturary = 
{
	local string deathstring;
	local string who;
	local float rnum, msgdt, fragnum;
	
	rnum = random();
	
	if(who_died.flags & FL_CLIENT)
	{
		if(who_killed == world)
		{
			deathstring = "was killed";
			
			if(who_died.watertype == CONTENT_WATER)
				deathstring = " drowned";
			else if(who_died.watertype == CONTENT_SLIME)
				deathstring = " melted";
			else if(who_died.watertype == CONTENT_LAVA)
				deathstring = " got incinerated";
			
			msgdt = TRUE;
		}
		
		if(who_killed.classname == "door")
		{
			if(rnum < 0.25)
			{
				deathstring = " got crushed";
			}
			else
				deathstring = " angered the ";
		}
		
		if(who_killed.classname == "button")
		{
			if(rnum < 0.25)
			{
				deathstring = " pushed it the wrong way";
				msgdt = TRUE;
			}
			else
				deathstring = " angered the ";
		}
		
		if(who_killed.classname == "train")
		{
			deathstring = " jumped infront the ";
		}
		
		if(who_killed.classname == "teledeath")
		{
			deathstring = " was telefragged by ";
		}
		
		if(who_killed.classname == "t_hurt")
		{
			deathstring = " got hurt too much...";
		}
		
		if(who_killed.classname == "t_push")
		{
			deathstring = " got pushed too far...";
		}
		
		if(who_killed == who_died)
		{
			deathstring = " killed themselves...";
			msgdt = TRUE;
		}
		
		bprint(who_died.netname);
		bprint(deathstring);
		
		if(msgdt != TRUE)
		{
			if(who_killed.flags & FL_CLIENT)
				bprint(who_killed.netname);
			else 
				bprint(who_killed.classname);
		}
		
		bprint("\n");
	}
};

/*
===============
|CLIENTKILL():|
=================================================================================
Description:
This function is called when the player enters the 'kill' command in the console.
=================================================================================
*/

void() ClientKill = 
{
	//pprint(self, " has", " killed themselves.");
	T_Damage(self, self, self, self.health);
	ClientRespawn();
};

/*
==================
|CLIENTCONNECT():|
=================================================================================
Description:
This function is called when the player connects to the server.
=================================================================================
*/

void() ClientConnect = 
{
	pprint(self, " has", " joined the game.");
	configstring (2, "sky"); //CS_SKY
}; 

/*
==================
|CLIENTDISCONNECT():|
=================================================================================
Description:
This function is called when the player disconnects from the server.
=================================================================================
*/

void() ClientDisconnect = 
{
	pprint(self, " has", " left the game.");
};

/*
====================
|PLAYERPRETHINK():|
===========================================================
Description:
This function is called every frame *BEFORE* world physics.
===========================================================
*/


void() PlayerPreThink = 
{
	WaterMove ();
	SetClientFrame ();
	CheckImpulses(); 
};

/*
====================
|PLAYERPOSTTHINK():|
===========================================================
Description:
This function is called every frame *AFTER* world physics.
===========================================================
*/

void() PlayerPostThink = {};

/*
======================
|PUTCLIENTINSERVER():|
===========================================================
Description:
This function is called whenever a client enters the world.
It sets up the player entity.
===========================================================
*/

entity() find_spawnspot = 
{
	local entity spot;
	local string a;
	
	if(deathmatch == 1)
		a = "info_player_deathmatch";
	else if(coop == 1)
		a = "info_player_coop";

	else if(!deathmatch || !coop)
		a = "info_player_start";
	
	spot = find (world, classname, a);
	
	return spot;
};

void() PutClientInServer =
{
	local entity spawn_spot;             // This holds where we want to spawn
	spawn_spot = find_spawnspot(); //find (world, classname, "info_player_start"); // Find it :)

	self.classname = "player";           // I'm a player!
	self.health = self.max_health = 100; // My health (and my max) is 100
	self.takedamage = DAMAGE_AIM;        // I can be fired at
	self.solid = SOLID_BBOX;         // Things sort of 'slide' past me
	self.movetype = MOVETYPE_WALK;       // Yep, I want to walk.
	self.flags = FL_CLIENT;              // Yes, I'm a client.

	self.origin = spawn_spot.origin + '0 0 1'; // Move to the spawnspot location
	self.angles = spawn_spot.angles;     // Face the angle the spawnspot indicates
	self.fixangle = TRUE;                // Turn this way immediately

	dprint("PutClientInServer()\n");

	setmodel (self, "models/player.mdl"); // Set my player to the player model
	setsize (self, VEC_HULL_MIN, VEC_HULL_MAX); // Set my size

	self.view_ofs = '0 0 22';            // Center my view

	setsize(self, '-16 -16 -32', '16 16 32' );
	
	if (self.aflag)
		CCamChasePlayer ();

	self.velocity = '0 0 0';             // Stop any old movement

	self.th_pain = PlayerPain;
	self.th_die = PlayerDie;

	setstats( self, STAT_HEALTH_ICON, "i_health");
	setstats( self, STAT_HEALTH, ftos(self.health));
	//setstats( self, STAT_HELPICON, "i_help");
		
	GetLevelParms();
};

