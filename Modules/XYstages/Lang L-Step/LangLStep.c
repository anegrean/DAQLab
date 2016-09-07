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
#include "DAQLabErrHandling.h"
#include "LangLStep.h"
#include "LStep4X.h"

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
#define Stage_IO_COMPort					"COM11"		// note: format of COM port is "COMx" where x is the number of the port
#define Stage_IO_BaudRate					57600

#ifndef nullWinChk
#define nullWinChk(fCall) if ((fCall) == 0) \
{winError = GetLastError(); goto WinError;} else
#endif
	
#define LStepErrChk(fCall) if ((lstepError = fCall)) {goto LStepError;} else
	
#define LStepWinError		-1
#define LStepAPIError		-2
	
#define LSTEP_RETURN_ERR \
		return 0; \
	WinError: \
		errorInfo.error		= LStepWinError; \
		RETURN_ERR \
	Error: \
		RETURN_ERR \
	LStepError: \
		errorInfo.error 	= LStepAPIError; \
		errorInfo.errMsg 	= GetLStepErrorMsg(lstepError); \
		RETURN_ERR 
		
	


//----------------------------------------------------------------------
// Stage settings
//----------------------------------------------------------------------

#define Stage_Config_File					"Modules\\XYstages\\Lang L-Step\\L-Step configuration.ls"
#define LStep4X_DLL							"Modules\\XYstages\\Lang L-Step\\lstep4x.dll"
#define Stage_Units							2			// position in [mm]
#define Stage_WaitUntilMovementComplete		TRUE		// thread that issues movement command is blocked until movement completes


								

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


static int			Load 									(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

//------------------------------------------------------------------------------------------------------------
// Stage operation
//------------------------------------------------------------------------------------------------------------

	// opens a connection to the Lang LStep stage controller
static int			InitStageConnection						(LangLStep_type* stage, char** errorMsg);

	// closes an open connection to the Lang LStep stage controller
static int			CloseStageConnection					(LangLStep_type* stage, char** errorMsg);

	// moves the stage
static int			MoveStage								(LangLStep_type* stage, StageMoveTypes moveType, StageAxes axis, double moveVal, char** errorMsg);

static int			StopStage								(LangLStep_type* stage, char** errorMsg);

static int			SetStageLimits							(LangLStep_type* stage, double xNegativeLimit, double xPositiveLimit, double yNegativeLimit, double yPositiveLimit, char** errorMsg);

static int			GetStageLimits							(LangLStep_type* stage, double* xNegativeLimit, double* xPositiveLimit, double* yNegativeLimit, double* yPositiveLimit, char** errorMsg);
	// sets the same velocity for all axes
static int			SetStageVelocity						(LangLStep_type* stage, double velocity, char** errorMsg);
	// gets the velocity of all axes, and gives error if they are not all the same
static int			GetStageVelocity						(LangLStep_type* stage, double* velocity, char** errorMsg);

static int			GetStageAbsPosition						(LangLStep_type* stage, double* xAbsPos, double* yAbsPos, char** errorMsg);
	// returns a detailed error message given an LStep error code. If error code is not recognized, returns NULL
static char*		GetLStepErrorMsg						(int errorCode);


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
	langStage->lstepLibHndl		= NULL;
	langStage->LSID				= 0;
		
	
		// METHODS
	
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) xyStage;
	else
		return NULL;
	
}

void discard_LangLStep (DAQLabModule_type** mod)
{
	int					lstepError				= 0;
	DWORD				winError				= 0;
	
	FARPROC				procAddr				= NULL;
	
	LangLStep_type* 	stage 					= *(LangLStep_type**) mod;
	
	if (!stage) return;
	
	// if connection was established and if still connected, close connection
	CloseStageConnection(stage, NULL);
	
	// free LSID
	if ( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_FreeLSID") )
		(*(PCreateLSID)procAddr) (&stage->LSID);
	
	// discard DLL handle
	if (stage->lstepLibHndl) {
		FreeLibrary(stage->lstepLibHndl);
		stage->lstepLibHndl = 0;
	}
			
	// discard XYStage_type specific data
	discard_XYStage (mod);
	
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR
	
	int					lstepError	= 0;
	DWORD				winError	= 0;

	LangLStep_type* 	stage		= (LangLStep_type*)mod;
	
	
	// load DLL
	nullWinChk( (stage->lstepLibHndl = LoadLibrary(LStep4X_DLL)) );
	
	errChk( InitStageConnection(stage, &errorInfo.errMsg) );
	
	errChk( XYStage_Load(mod, workspacePanHndl, &errorInfo.errMsg) );

LSTEP_RETURN_ERR
}

//------------------------------------------------------------------------------------------------------------
// Stage operation
//------------------------------------------------------------------------------------------------------------


static int InitStageConnection (LangLStep_type* stage, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
#define InitStage_Err_ConnectionFailed		-3
#define InitStage_Err_ConfigFileNotLoaded	-4
#define InitStage_Err_ConfigParamNotSent	-5
#define InitStage_Err_SetStageUnits			-6

INIT_ERR
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// create LSID to identify stage with subsequent commands
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_CreateLSID") );
	LStepErrChk( (*(PCreateLSID)procAddr) (&stage->LSID) );
	
	// connect to stage
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_ConnectSimple") );
	LStepErrChk( (*(PConnectSimple)procAddr) (stage->LSID, Stage_IO_InterfaceType, Stage_IO_COMPort, Stage_IO_BaudRate, FALSE) );
	
	// load stage configuration file
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_LoadConfig") );
	LStepErrChk( (*(PLoadConfig)procAddr) (stage->LSID, Stage_Config_File) );
	
	// send configuration parameters to the stage
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_SetControlPars") );
	LStepErrChk( (*(PSetControlPars)procAddr) (stage->LSID) );
	
	// set units to [mm]
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_SetDimensions") );
	LStepErrChk( (*(PSetDimensions)procAddr) (stage->LSID, Stage_Units, Stage_Units, Stage_Units, Stage_Units) );

LSTEP_RETURN_ERR
}


static int CloseStageConnection	(LangLStep_type* stage, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// disconnect from stage
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_Disconnect") );
	LStepErrChk( (*(PDisconnect)procAddr) (stage->LSID) );
	
LSTEP_RETURN_ERR 
}

static int MoveStage (LangLStep_type* stage, StageMoveTypes moveType, StageAxes axis, double moveVal, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;

	switch (moveType) {
			
		case XYSTAGE_MOVE_REL:
			
			switch(axis) {
			
				case XYSTAGE_X_AXIS:
			 
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_MoveRelSingleAxis") );
					LStepErrChk( (*(PMoveRelSingleAxis)procAddr) (stage->LSID, XYSTAGE_X_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;
			
				case XYSTAGE_Y_AXIS:
			
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_MoveRelSingleAxis") );
					LStepErrChk( (*(PMoveRelSingleAxis)procAddr) (stage->LSID, XYSTAGE_Y_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;	
			}
			break;
			
		case XYSTAGE_MOVE_ABS:
			
			switch(axis) {
			
				case XYSTAGE_X_AXIS:
			 
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_MoveAbsSingleAxis") );
					LStepErrChk( (*(PMoveAbsSingleAxis)procAddr) (stage->LSID, XYSTAGE_X_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;
			
				case XYSTAGE_Y_AXIS:
			
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_MoveAbsSingleAxis") );
					LStepErrChk( (*(PMoveAbsSingleAxis)procAddr) (stage->LSID, XYSTAGE_Y_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;	
			}
			break;
	}
	
LSTEP_RETURN_ERR	
}

static int StopStage (LangLStep_type* stage, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_StopAxes") );
	LStepErrChk( (*(PStopAxes)procAddr) (stage->LSID) );
	
LSTEP_RETURN_ERR	
}

static int SetStageLimits (LangLStep_type* stage, double xNegativeLimit, double xPositiveLimit, double yNegativeLimit, double yPositiveLimit, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR

	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// set X axis limit
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_SetLimit") );
	LStepErrChk( (*(PSetLimit)procAddr) (stage->LSID, XYSTAGE_X_AXIS + 1, xNegativeLimit, xPositiveLimit) );
	
	// set Y axis limit
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_SetLimit") );
	LStepErrChk( (*(PSetLimit)procAddr) (stage->LSID, XYSTAGE_Y_AXIS + 1, yNegativeLimit, yPositiveLimit) );
	
LSTEP_RETURN_ERR	
}

static int GetStageLimits (LangLStep_type* stage, double* xNegativeLimit, double* xPositiveLimit, double* yNegativeLimit, double* yPositiveLimit, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR	
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	
	// add code here
	
	
	
LSTEP_RETURN_ERR
}

static int SetStageVelocity (LangLStep_type* stage, double velocity, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_SetVel") );
	LStepErrChk( (*(PSetVel)procAddr) (stage->LSID, velocity, velocity, velocity, velocity) );
	
LSTEP_RETURN_ERR
}

static int GetStageVelocity (LangLStep_type* stage, double* velocity, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR
	
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// add code here
	
LSTEP_RETURN_ERR
}

static int GetStageAbsPosition (LangLStep_type* stage, double* xAbsPos, double* yAbsPos, char** errorMsg)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
INIT_ERR

	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	double		xPos					= 0;
	double		yPos					= 0;
	double		zPos					= 0;
	double		aPos					= 0;
	
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LSX_GetPos") );
	LStepErrChk( (*(PGetPos)procAddr) (stage->LSID, &xPos, &yPos, &zPos, &aPos) );
	
	*xAbsPos = xPos;
	*yAbsPos = yPos;
	
LSTEP_RETURN_ERR
}

static char* GetLStepErrorMsg (int errorCode)
{
	switch (errorCode) {
			
		case 0:
			
			return StrDup("No error.");
			
		case 4001:
		case 4002:
			
			return StrDup("Internal error.");
			
		case 4003:
			
			return StrDup("Undefined error.");
			
		case 4004:
			
			return StrDup("Interface type unknown (may occur with Connect..).");
			
		case 4005:
			
			return StrDup("Interface initialization error.");
			
		case 4006:
			
			return StrDup("No connection to the controller (e.g. when SetPitch is called before Connect).");
			
		case 4007:
			
			return StrDup("Timeout while reading from the interface.");
			
		case 4008:
			
			return StrDup("Command transmission error to LStep.");
			
		case 4009:
			
			return StrDup("Command terminated (with SetAbortFlag).");
			
		case 4010:
			
			return StrDup("Command not supported by API.");
			
		case 4011:
			
			return StrDup("Joystick set to manual (may occur with SetJoystick On/Off).");
			
		case 4012:
			
			return StrDup("Travel command not possible, as joystick is in manual mode.");
			
		case 4013:
			
			return StrDup("Controller timeout.");
			
		case 4015:
			
			return StrDup("Actuates limit switch in moving direction.");
			
		case 4017:
			
			return StrDup("Fault during calibration (limit switch was not set free correctly).");
			
		case 4101:
			
			return StrDup("Valid axis designation missing.");
			
		case 4102:
			
			return StrDup("Non-executable function.");
			
		case 4103:
			
			return StrDup("Command string has too many characters.");
			
		case 4104:
			
			return StrDup("Invalid command.");
			
		case 4105:
			
			return StrDup("Not within valid numerical range.");
			
		case 4106:
			
			return StrDup("Incorrect number of parameters.");
			
		case 4107:
			
			return StrDup("None! or ?.");
			
		case 4108:
			
			return StrDup("TVR not possible because axis is active.");
			
		case 4109:
			
			return StrDup("Axes cannot be switched on or off because TVR is active.");
			
		case 4110:
			
			return StrDup("Function not configured.");
			
		case 4111:
			
			return StrDup("Move command not possible, as joystick is in manual mode.");
			
		case 4112:
			
			return StrDup("Limit switch tripped.");
			
		case 4113:
			
			return StrDup("Function cannot be carried out because encoder was recognized.");
			
		case 4114:
			
			return StrDup("Fault during calibration (limit switch was not set free correctly).");
			
		case 4115:
			
			return StrDup("This function is interrupted activated while releasing the encoder during calibrating or table stroke measuring if the opposite encoder is activated.");
			
		case 4120:
			
			return StrDup("Driver relay defective (safety circle K3/K4).");
			
		case 4121:
			
			return StrDup("Only single vectors may be driven(setup mode).");
			
		case 4122:
			
			return StrDup("No calibrating, measuring table stroke or joystick operation can be carried out (door open or setup mode).");
			
		case 4123:
			
			return StrDup("Security error X-axis.");
			
		case 4124:
			
			return StrDup("Security error Y-axis.");
			
		case 4125:
			
			return StrDup("Security error Z-axis.");
			
		case 4126:
			
			return StrDup("Security error A-axis.");
			
		case 4127:
			
			return StrDup("Stop active.");
			
		case 4128:
			
			return StrDup("Fault in the door switch safety circle (only with LS44/Solero).");
			
		case 4129:
			
			return StrDup("Power stages are not switched on (only with LS44).");
			
		case 4130:
			
			return StrDup("GAL security error (only with LS44).");
			
		case 4131:
			
			return StrDup("Joystick cannot be activated because Move is active.");
			
		default:
			
			return NULL;
	}
}

