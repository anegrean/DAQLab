//==============================================================================
//
// Title:		LangLStep.c
// Purpose:		A short description of the implementation.
//
// Created on:	22-3-2015 at 18:31:38 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first 
#include "DAQLabUtility.h"
#include "LangLStep.h"
#include "LV_LangLStep4.h"

//==============================================================================
// Constants
						  
//----------------------------------------------------------------------
// Stage RS232 connection
//----------------------------------------------------------------------
// Interface selects the interface type:
// 1 = RS232
// 3 = ISA/DPRAM
// 4 = PCI
//  
//  set ComPort = 1 to select 'COM1'
//  
//  BaudRate = 9600 or 57600 (RS232)
//  
//  I/O Adress is the base adress of the LSTEP-PC card (ISA/DPRAM) 
//  
//  Card Index is the zero-based index of the LStep-PCI card (PCI)

#define Stage_IO_InterfaceType				1	 		// RS232
#define Stage_IO_COMPort					8
#define Stage_IO_BaudRate					57600
#define Stage_IO_ISAAddress					0
#define	Stage_IO_PCICardIndex				0

//----------------------------------------------------------------------
// Stage settings
//----------------------------------------------------------------------

#define Stage_Config_File					"Modules\\XYstages\\Lang L-Step\\L-Step configuration.ls"
#define Stage_Units							2			// position in [mm]
#define Stage_WaitUntilMovementComplete		TRUE		// thread that issues movement command is blocked until movement completes




//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


static int			Load 									(DAQLabModule_type* mod, int workspacePanHndl, char** errorInfo);

//------------------------------------------------------------------------------------------------------------
// Stage operation
//------------------------------------------------------------------------------------------------------------

	// opens a connection to the Lang LStep stage controller
static int			InitStageConnection						(char** errorInfo);

	// closes an open connection to the Lang LStep stage controller
static int			CloseStageConnection					(char** errorInfo);

	// moves the stage
static int			MoveStage								(LangLStep_type* stage, StageMoveTypes moveType, StageAxes axis, double moveVal, char** errorInfo);

static int			StopStage								(LangLStep_type* stage, char** errorInfo);

static int			SetStageLimits							(LangLStep_type* stage, double xNegativeLimit, double xPositiveLimit, double yNegativeLimit, double yPositiveLimit, char** errorInfo);

static int			GetStageLimits							(LangLStep_type* stage, double* xNegativeLimit, double* xPositiveLimit, double* yNegativeLimit, double* yPositiveLimit, char** errorInfo);
	// sets the same velocity for all axes
static int			SetStageVelocity						(LangLStep_type* stage, double velocity, char** errorInfo);
	// gets the velocity of all axes, and gives error if they are not all the same
static int			GetStageVelocity						(LangLStep_type* stage, double* velocity, char** errorInfo);

static int			GetStageAbsPosition						(LangLStep_type* stage, double* xAbsPos, double* yAbsPos, char** errorInfo);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* initalloc_LangLStep (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	LangLStep_type* 	langStage;
	XYStage_type*  		xyStage;
	
	if (!mod) {
		langStage = malloc (sizeof(LangLStep_type));
		if (!langStage) return NULL;
	} else
		langStage = (LangLStep_type*) mod;
	
	xyStage = (XYStage_type*) langStage;
	
	// initialize base class
	initalloc_XYStage ((DAQLabModule_type*)xyStage, className, instanceName, workspacePanHndl);
	
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
	
		//METHODS
	
		// overriding methods
	xyStage->baseClass.Load		= Load;	
	xyStage->baseClass.Discard 	= discard_XYStage;
	
	//---------------------------
	// Child Level 1: XYStage_type 
	
		// DATA
		
	
		// METHODS
	
		// overriding methods
	xyStage->Move				= (MoveFptr_type) 				MoveStage;
	xyStage->Stop				= (StopFptr_type) 				StopStage;
	//xyStage->GetLimits		= (GetLimitsFptr_type) 			GetStageLimits;
	xyStage->SetLimits			= (SetLimitsFptr_type) 			SetStageLimits;
	//xyStage->GetVelocity		= (GetVelocityFptr_type) 		GetStageVelocity;
	xyStage->SetVelocity		= (SetVelocityFptr_type) 		SetStageVelocity;
	xyStage->GetAbsPosition		= (GetAbsPositionFptr_type) 	GetStageAbsPosition;
	
	
	//--------------------------
	// Child Level 2: LangLStep_type
	
		// DATA
		
	
		// METHODS
	
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) xyStage;
	else
		return NULL;
	
}

void discard_LangLStep (DAQLabModule_type** mod)
{
	LangLStep_type* stage = *(LangLStep_type**) mod;
	
	if (!stage) return;
	
	// if connection was established and if still connected, close connection
	CloseStageConnection(NULL);
			
	// discard XYStage_type specific data
	discard_XYStage (mod);
	
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorInfo)
{
	int		error	 = 0;
	char*	errMsg   = NULL;
	
	errChk( InitStageConnection(&errMsg) );
	
	errChk( XYStage_Load(mod, workspacePanHndl, &errMsg) );
	
	return 0;
	
Error:
	
	ReturnErrMsg("Lang LStep Load");
	return error;
}

//------------------------------------------------------------------------------------------------------------
// Stage operation
//------------------------------------------------------------------------------------------------------------

static int InitStageConnection (char** errorInfo)
{
#define InitStage_Err_ConnectionFailed		-1
#define InitStage_Err_ConfigFileNotLoaded	-2
#define InitStage_Err_ConfigParamNotSent	-3
#define InitStage_Err_SetStageUnits			-4
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8 		showProtocol			= FALSE;
	uInt8		isInitialized			= FALSE;
	uInt8	 	commandExecuted			= FALSE;
	
	// connect to stage
	LS4ConnectSimple(Stage_IO_InterfaceType, Stage_IO_COMPort, Stage_IO_BaudRate, Stage_IO_ISAAddress, Stage_IO_PCICardIndex, &showProtocol, &isInitialized);
	if (!isInitialized){
		errMsg 	= StrDup("Lang LStep stage initialization failed");
		error 	= InitStage_Err_ConnectionFailed;
		goto Error;
	}
	
	// load stage configuration file
	LS4LoadConfig(Stage_Config_File, &commandExecuted);
	if (!commandExecuted){
		errMsg 	= StrDup("Lang LStep stage configuration file could not be loaded");
		error	= InitStage_Err_ConfigFileNotLoaded;
		goto Error;
	}
	
	// send configuration parameters to the stage
	LS4SetControlPars(&commandExecuted); 
	if (!commandExecuted){
		errMsg = StrDup("Lang LStep stage configuration parameters could not be sent to the stage");
		error = InitStage_Err_ConfigParamNotSent;
		goto Error;
	}
	
	// set units to [mm]
	LS4SetDimensions(Stage_Units, Stage_Units, Stage_Units, Stage_Units, &commandExecuted);
	if (!commandExecuted){
		errMsg = StrDup("Lang LStep stage units could not be set");
		error = InitStage_Err_SetStageUnits;
		goto Error;
	}
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "InitStageConnection", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int CloseStageConnection	(char** errorInfo)
{
#define CloseStageConnection_Err_ConnectionNotClosed	-1 
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	
	// disconnect from stage
	LS4Disconnect(&commandExecuted);
	if (!commandExecuted){
		errMsg = StrDup("Lang LStep stage connection could not be closed");
		error 	= CloseStageConnection_Err_ConnectionNotClosed;
		goto Error;
	}
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "CloseStageConnection", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int MoveStage (LangLStep_type* stage, StageMoveTypes moveType, StageAxes axis, double moveVal, char** errorInfo)
{
#define Move_Err_XAxis	-1
#define Move_Err_YAxis	-2
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	uInt8 		wait					= Stage_WaitUntilMovementComplete;
	
	switch (moveType) {
			
		case XYSTAGE_MOVE_REL:
			
			switch(axis) {
			
				case XYSTAGE_X_AXIS:
			 
					LS4MoveRelSingleAxis(XYSTAGE_X_AXIS + 1, moveVal, &wait, &commandExecuted); 
					if (!commandExecuted){
						errMsg 	= StrDup("Lang LStep X axis movement failed");
						error 	= Move_Err_XAxis;
						goto Error;
					}
					break;
			
				case XYSTAGE_Y_AXIS:
			
					LS4MoveRelSingleAxis(XYSTAGE_Y_AXIS + 1, moveVal, &wait, &commandExecuted);
					if (!commandExecuted){
						errMsg 	= StrDup("Lang LStep Y axis movement failed");
						error 	= Move_Err_YAxis;
						goto Error;
					}
					break;	
			}
			
			break;
			
		case XYSTAGE_MOVE_ABS:
			
			switch(axis) {
			
				case XYSTAGE_X_AXIS:
			 
					LS4MoveAbsSingleAxis(XYSTAGE_X_AXIS + 1, moveVal, &wait, &commandExecuted); 
					if (!commandExecuted){
						errMsg 	= StrDup("Lang LStep X axis movement failed");
						error 	= Move_Err_XAxis;
						goto Error;
					}
					break;
			
				case XYSTAGE_Y_AXIS:
			
					LS4MoveAbsSingleAxis(XYSTAGE_Y_AXIS + 1, moveVal, &wait, &commandExecuted);
					if (!commandExecuted){
						errMsg 	= StrDup("Lang LStep Y axis movement failed");
						error 	= Move_Err_YAxis;
						goto Error;
					}
					break;	
			}
			break;
	}
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "MoveStage", errMsg);
	OKfree(errMsg);
	
	return error;
	
}

static int StopStage (LangLStep_type* stage, char** errorInfo)
{
#define StopStage_Err	-1
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;

	LS4StopAxes(&commandExecuted);
	if (!commandExecuted){
		errMsg 	= StrDup("Lang LStep stage could not be stopped");
		error 	= StopStage_Err;
		goto Error;
	}
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "MoveStage", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int SetStageLimits (LangLStep_type* stage, double xNegativeLimit, double xPositiveLimit, double yNegativeLimit, double yPositiveLimit, char** errorInfo)
{
#define SetStageLimits_Err_XAxis 	-1
#define SetStageLimits_Err_YAxis 	-2
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	
	// set X axis limit
	LS4SetLimit(XYSTAGE_X_AXIS, xNegativeLimit, xPositiveLimit, &commandExecuted);
	if (!commandExecuted){
		errMsg 	= StrDup("Lang LStep X axis limit could not be set");
		error 	= SetStageLimits_Err_XAxis;
		goto Error;
	}
	
	// set Y axis limit
	LS4SetLimit(XYSTAGE_Y_AXIS, yNegativeLimit, yPositiveLimit, &commandExecuted);
	if (!commandExecuted){
		errMsg 	= StrDup("Lang LStep Y axis limit could not be set");
		error 	= SetStageLimits_Err_YAxis;
		goto Error;
	}

	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "SetStageLimits", errMsg);
	OKfree(errMsg);
	
	return error;
	
}

static int GetStageLimits (LangLStep_type* stage, double* xNegativeLimit, double* xPositiveLimit, double* yNegativeLimit, double* yPositiveLimit, char** errorInfo)
{
#define GetStageLimits_Err_XAxis 	-1
#define GetStageLimits_Err_YAxis 	-2
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	
	
	
	
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "GetStageLimits", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int SetStageVelocity (LangLStep_type* stage, double velocity, char** errorInfo)
{
#define SetStageVelocity_Err 	-1

	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	
	LS4SetVel(velocity, velocity, velocity, velocity, &commandExecuted);
	if (!commandExecuted){
		errMsg 	= StrDup("Lang LStep could not set stage velocity");
		error 	= SetStageVelocity_Err;
		goto Error;
	}
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "SetStageVelocity", errMsg);
	OKfree(errMsg);
	
	return error;
	
}

static int GetStageVelocity (LangLStep_type* stage, double* velocity, char** errorInfo)
{
#define GetStageVelocity_Err 						-1
#define GetStageVelocity_Err_VelocitiesNotEqual 	-2

	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "GetStageVelocity", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int GetStageAbsPosition (LangLStep_type* stage, double* xAbsPos, double* yAbsPos, char** errorInfo)
{
#define GetStageAbsPosition_Err		-1
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	uInt8	 	commandExecuted			= FALSE;
	double		xPos					= 0;
	double		yPos					= 0;
	double		zPos					= 0;
	double		aPos					= 0;
	
	LS4GetPos(&xPos, &yPos, &zPos, &aPos, &commandExecuted);
	if (!commandExecuted){
		errMsg 	= StrDup("Lang LStep could not get absolute position");
		error 	= GetStageAbsPosition_Err;
		goto Error;
	}
	
	*xAbsPos = xPos;
	*yAbsPos = yPos;
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Unknown error");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "GetStageAbsPosition", errMsg);
	OKfree(errMsg);
	
	return error;
	
}
