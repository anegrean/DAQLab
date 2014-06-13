#include <userint.h>

//==============================================================================
//
// Title:		PIMercuryC863.c
// Purpose:		A short description of the implementation.
//
// Created on:	10-3-2014 at 12:06:57 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first 
#include <formatio.h> 
#include <ansi_c.h> 
#include "PIMercuryC863.h"
#include "PI_GCS2_DLL.h"	   

//==============================================================================
// Constants

//------------------------------------------------------------------------------
// Custom PID settings for the servo motion controller
//------------------------------------------------------------------------------
#define PIMERCURYC863_PPAR			0x01	 //P parameter list ID, see hardware documentation
#define PIMERCURYC863_IPAR			0x02	 //I parameter list ID
#define PIMERCURYC863_DPAR			0x03	 //D parameter list ID

#define PIMERCURYC863_PTERM 		40		 
#define PIMERCURYC863_ITERM 		1800
#define PIMERCURYC863_DTERM 		3300
#define	PIMERCURYC863_USECUSTOMPID	TRUE	 // overrides factory PID settings
//------------------------------------------------------------------------------
// USB connection ID
//------------------------------------------------------------------------------
#define PIMercuryC863_ID "0115500620"

//------------------------------------------------------------------------------
// PI Stage type
//------------------------------------------------------------------------------
#define PIMercuryC863_STAGE_TYPE "M-605.2DD"


//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static int PIMercuryC863_Load (DAQLabModule_type* mod, int workspacePanHndl);

static int PIMercuryC863_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement);

static int PIMercuryC863_Move (Zstage_type* self, Zstage_move_type moveType, double moveVal);

static int PIMercuryC863_InitHardware (PIMercuryC863_type* self);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_PIMercuryC863	(DAQLabModule_type* mod)
{
	PIMercuryC863_type* 	PIMzstage;
	Zstage_type*  			zstage;
	
	if (!mod) {
		PIMzstage = malloc (sizeof(PIMercuryC863_type));
		if (!PIMzstage) return NULL;
	} else
		PIMzstage = (PIMercuryC863_type*) mod;
	
	zstage = (Zstage_type*) PIMzstage;
	
	// initialize base class
	initalloc_Zstage ((DAQLabModule_type*)zstage);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
	
		// updating name
	OKfree(zstage->module_base.className);
	zstage->module_base.className	= StrDup(MOD_PIMercuryC863_NAME);
		
	
		//METHODS
	
		// overriding methods
	zstage->module_base.Discard 	= discard_PIMercuryC863;
	zstage->module_base.Load 		= PIMercuryC863_Load;
	zstage->module_base.LoadCfg		= NULL; //PIMercuryC863_LoadCfg;
		
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
	
	
		// METHODS
	
		// overriding methods
	zstage->MoveZ					= PIMercuryC863_Move;  
		
		
	
	//--------------------------
	// Child Level 2: PIMercuryC863_type
	
		// DATA
		
	PIMzstage->PIStageID			= -1;
	PIMzstage->assignedAxis[0]		= 0;
	
	
		// METHODS
	
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) zstage;
	else
		return NULL;
}

void discard_PIMercuryC863 (DAQLabModule_type* mod)
{
	PIMercuryC863_type* PIMzstage = (PIMercuryC863_type*) mod;
	
	// if connection was established and if still connected, close connection otherwise the DLL thread throws an error
	if (PIMzstage->PIStageID != -1)
		if (PI_IsConnected(PIMzstage->PIStageID))
			PI_CloseConnection(PIMzstage->PIStageID);
			
	
	// discard PIMercuryC863_type specific data
	
	// discard Zstage_type specific data
	discard_Zstage (mod);
}

/// HIFN Loads PIMercuryC863 motorized stage specific resources. 
static int PIMercuryC863_Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	if (PIMercuryC863_InitHardware((PIMercuryC863_type*) mod) < 0) return -1;
	
	// load generic Z stage resources
	Zstage_Load (mod, workspacePanHndl); 
	return 0;
	
}

/// HIFN Moves a motorized stage 
static int PIMercuryC863_Move (Zstage_type* self, Zstage_move_type moveType, double moveVal)
{
	return 0;
}

/// HIFN Connects to a PIMercuryC863 motion controller and adjusts its motion parameters
/// HIRET 0 if connection was established and -1 for error
static int PIMercuryC863_InitHardware (PIMercuryC863_type* self)
{
	BOOL 			ServoONFlag				= TRUE; 
	BOOL			IsReferencedFlag;
	BOOL			HasLimitSwitchesFlag;
	BOOL			ReadyFlag;
	int				PIMerror;
	int				idx;
	unsigned int	pidParameters[3]		= {PIMERCURYC863_PPAR, PIMERCURYC863_IPAR, PIMERCURYC863_DPAR};  
	double			pidValues[3]			= {PIMERCURYC863_PTERM, PIMERCURYC863_ITERM, PIMERCURYC863_DTERM};
	size_t			nOcurrences;
	size_t			nbuff;
	char			msg[500]				="";
	char			buff[10000]				="";
	char			axes[255]				="";
	long 			ID; 						
	char 			szStrings[255]			="";
	
	
	// establish connection
	DLMsg("Connecting to MercuryC863 servo controller...\n\n", 0); 
	if ((ID = PI_ConnectUSB(PIMercuryC863_ID)) < 0) {
		Fmt(msg,"Could not connect to Mercury servo controller with USB ID %s.\n\n", PIMercuryC863_ID);
		DLMsg(msg, 1);
		return -1;
	} else
		self->PIStageID = ID;
	
	// print servo controller info
	if (!PI_qIDN(ID, buff, 10000)) {
		PIMerror = PI_GetError(ID); 
		Fmt(msg, "Error reading Mercury servo controller info. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
		DLMsg(msg, 1);
		return -1;
	} else {
		Fmt(msg, "Connected to: %s\n", buff);
		DLMsg(msg,0);
	}
	
	// get all possible axes that the servo controller can move
	if (!PI_qSAI_ALL(ID, axes, 255)) {
		PIMerror = PI_GetError(ID);
		Fmt(msg, "Error getting Mercury servo controller available axes. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
		DLMsg(msg, 1);
		return -1;
	}
	
	// assign by default the first axis found if this is not already set
	if (!self->assignedAxis[0]) {
		self->assignedAxis[0] = axes[0];
		self->assignedAxis[1] = 0;		// set the null character
	}
	
	// check if stages database is present from which parameters can be loaded
	if (!PI_qVST(ID, buff, 10000)) {
		PIMerror = PI_GetError(ID);
		Fmt(msg, "Error getting list of installed stages. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
		DLMsg(msg, 1);
		return -1;
	}
	// process buff to check if there are any stages in the list by checking the number of times "\n" or number 10 is present
	// if there are no stages, then buff will contain "NOSTAGES \n" and thus only one occurrence of "\n".
	idx 		= 0;
	nOcurrences = 0;
	nbuff		= strlen(buff);
	while (idx < 10000 && idx < nbuff) {
		if (buff[idx] == 10) nOcurrences++; 
		idx++;
	}
	
	if (nOcurrences <= 1) {
		DLMsg("Error loading Mercury servo controller. No stage database was found.\n\n", 1);
		return -1;
	}
	
	// loading stage specific info (mechanics, limits, factory PID values, etc.)
	DLMsg("Loading MercuryC863 servo controller settings...", 0);
	if (!PI_CST(ID, self->assignedAxis, PIMercuryC863_STAGE_TYPE)) {
		PIMerror = PI_GetError(ID);
		Fmt(msg, "\n\nError loading factory stage settings. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
		DLMsg(msg, 1);
		return -1;
	} else
		DLMsg("Loaded.\n\n", 0);
		
	// adjust PID settings simultaneously for the assigned axis
	if (PIMERCURYC863_USECUSTOMPID) {
		for (int i = 0; i < 3; i++)
			if (!PI_SPA(ID, self->assignedAxis, pidParameters + i, pidValues + i, szStrings)){
				PIMerror = PI_GetError(ID);
				Fmt(msg, "Error changing PID servo parameters. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
				DLMsg(msg, 1);
				return -1;	
			}
		
	}
	
	// turn servo on for the given axis
	if (!PI_SVO(ID, self->assignedAxis, &ServoONFlag)) {
		PIMerror = PI_GetError(ID);
		Fmt(msg, "Error turning servo ON. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
		DLMsg(msg, 1);
		return -1;
	}
	
	// check if axis is already referenced
	if (!PI_qFRF(ID, self->assignedAxis, &IsReferencedFlag)) {   
		PIMerror = PI_GetError(ID);
		Fmt(msg, "Error checking if servo is referenced already. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
		DLMsg(msg, 1);
		return -1;
	}
	
	if (IsReferencedFlag) {
		// if referenced read in current position
		self->zstage_base.zPos = malloc(sizeof(double));
		if(!self->zstage_base.zPos) return -1;
		if (!PI_qPOS(ID, self->assignedAxis, self->zstage_base.zPos)) {
			OKfree(self->zstage_base.zPos);
			PIMerror = PI_GetError(ID);
			Fmt(msg, "Error getting stage position. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
			DLMsg(msg, 1);
			return -1;
		}
	} else {
		// check if there are limit switches
		if (!PI_qLIM(ID, self->assignedAxis, &HasLimitSwitchesFlag)) {
			PIMerror = PI_GetError(ID);
			Fmt(msg, "Checking if servo has limit switches failed. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
			DLMsg(msg, 1);
			return -1;
		}
		 
		// reference axis
		if (HasLimitSwitchesFlag) {
			if (!PI_FPL(ID, self->assignedAxis)) {
				PIMerror = PI_GetError(ID);
				Fmt(msg, "Referencing stage failed. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
				DLMsg(msg, 1);
				return -1;
			}
		} else {
			DLMsg("Stage does not have limit switches and cannot be referenced.\n\n", 1);   
			return -1;
		} 
		
		// wait until stage motion completes
		do {
			Sleep(500);
			if (!PI_IsControllerReady(ID, &ReadyFlag)) {
				PIMerror = PI_GetError(ID);
				Fmt(msg, "Could not query controller status. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
				DLMsg(msg, 1);
				return -1;
			}
		} while (!ReadyFlag);
		
		// check if stage is referenced
		if (!PI_qFRF(ID, self->assignedAxis, &IsReferencedFlag)) {   
			PIMerror = PI_GetError(ID);
			Fmt(msg, "Error checking if servo is referenced already. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
			DLMsg(msg, 1);
			return -1;
		}
	
		if (IsReferencedFlag) {
			// if referenced read in current position
			self->zstage_base.zPos = malloc(sizeof(double));
			if(!self->zstage_base.zPos) return -1;
			if (!PI_qPOS(ID, self->assignedAxis, self->zstage_base.zPos)) {
				OKfree(self->zstage_base.zPos);
				PIMerror = PI_GetError(ID);
				Fmt(msg, "Error getting stage position. PI Mercury DLL Error ID: %d.\n\n", PIMerror);
				DLMsg(msg, 1);
				return -1;
			}
		} else {
			// referencing failed
			DLMsg("Failed to reference stage.\n\n", 1);
			return -1;
		}
	}
	
	return 0;
}

/*
static int PIMercuryC863_LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement)
{
	PIMercuryC863_type* 	PIMzstage	= (PIMercuryC863_type*) mod;
	
	return Zstage_LoadCfg(mod, DAQLabCfg_RootElement); 
}
*/

