//=======================================================================
//			Copyright XashXT Group 2007 �
//			cm_main.c - collision interface
//=======================================================================

#include "cm_local.h"

physic_imp_t	pi;
stdlib_api_t	com;

cvar_t		*cm_triangles;
cvar_t		*cm_noareas;
cvar_t		*cm_nocurves;
cvar_t		*cm_showcurves;
cvar_t		*cm_showtriangles;
cvar_t		*cm_novis;

bool CM_InitPhysics( void )
{
	cms.mempool = Mem_AllocPool( "CM Zone" );
	Mem_Set( cms.nullrow, 0xFF, MAX_MAP_LEAFS / 8 );

	cm_noareas = Cvar_Get( "cm_noareas", "0", 0, "ignore clipmap areas" );
	cm_triangles = Cvar_Get( "cm_triangles", "0", CVAR_ARCHIVE, "convert all collide polygons into triangles" );
	cm_nocurves = Cvar_Get( "cm_nocurves", "0", CVAR_ARCHIVE|CVAR_LATCH, "make patches uncollidable" );
	cm_showcurves = Cvar_Get( "cm_showcurves", "0", 0, "show collision curves" );
	cm_showtriangles = Cvar_Get( "cm_showtris", "0", 0, "show collision triangles" );
	cm_novis = Cvar_Get( "cm_novis", "0", 0, "ignore vis information (perfomance test)" );

	return true;
}

void CM_PhysFrame( float frametime )
{
}

void CM_FreePhysics( void )
{
	CM_FreeWorld();
	CM_FreeModels();
	Mem_FreePool( &cms.mempool );
}

physic_exp_t DLLEXPORT *CreateAPI ( stdlib_api_t *input, physic_imp_t *engfuncs )
{
	static physic_exp_t		Phys;

	com = *input;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if(engfuncs) pi = *engfuncs;

	// generic functions
	Phys.api_size = sizeof( physic_exp_t );
	Phys.com_size = sizeof( stdlib_api_t );
	
	Phys.Init = CM_InitPhysics;
	Phys.Shutdown = CM_FreePhysics;

	Phys.DrawCollision = CM_DrawCollision;
	Phys.Frame = CM_PhysFrame;

	Phys.BeginRegistration = CM_BeginRegistration;
	Phys.RegisterModel = CM_RegisterModel;
	Phys.EndRegistration = CM_EndRegistration;

	Phys.SetAreaPortals = CM_SetAreaPortals;
	Phys.GetAreaPortals = CM_GetAreaPortals;
	Phys.SetAreaPortalState = CM_SetAreaPortalState;
	Phys.BoxLeafnums = CM_BoxLeafnums;
	Phys.WriteAreaBits = CM_WriteAreaBits;
	Phys.AreasConnected = CM_AreasConnected;
	Phys.ClusterPVS = CM_ClusterPVS;
	Phys.ClusterPHS = CM_ClusterPHS;
	Phys.LeafCluster = CM_LeafCluster;
	Phys.PointLeafnum = CM_PointLeafnum;
	Phys.LeafArea = CM_LeafArea;

	Phys.NumShaders = CM_NumShaders;
	Phys.NumBmodels = CM_NumInlineModels;
	Phys.Mod_GetBounds = CM_ModelBounds;
	Phys.Mod_GetFrames = CM_ModelFrames;
	Phys.GetShaderName = CM_ShaderName;
	Phys.Mod_Extradata = CM_Extradata;
	Phys.GetEntityScript = CM_EntityScript;
	Phys.VisData = CM_VisData;

	Phys.PointContents1 = CM_PointContents;
	Phys.PointContents2 = CM_TransformedPointContents;
	Phys.BoxTrace1 = CM_BoxTrace;
	Phys.BoxTrace2 = CM_TransformedBoxTrace;
	Phys.BiSphereTrace1 = CM_BiSphereTrace;
	Phys.BiSphereTrace2 = CM_TransformedBiSphereTrace;

	Phys.TempModel = CM_TempBoxModel;

	// needs to be removed
	Phys.FatPVS = CM_FatPVS;
	Phys.FatPHS = CM_FatPHS;

	return &Phys;
}