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
#include "LStep4.h"

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
#define Stage_IO_COMPort					"COM8"		// note: format of COM port is "COMx" where x is the number of the port
#define Stage_IO_BaudRate					57600

#ifndef nullWinChk
#define nullWinChk(fCall) if ((fCall) == 0) \
{winError = GetLastError(); goto WinError;} else
#endif
	
#define LStepErrChk(fCall) if ((lstepError = fCall)) {goto LStepError;} else
	
#define LStepWinError		-1
#define LStepAPIError		-2
	
#define ReturnLStepError(functionName) 			\
		return 0; 								\
	WinError:									\
		error 	= LStepWinError;				\
		ReturnErrMsg(functionName);				\
		return error;							\	
	Error:										\
		ReturnErrMsg(functionName);				\
		return error;							\
	LStepError:									\
		error 	= LStepAPIError;  				\
		errMsg 	= GetLStepErrorMsg(lstepError); \
		ReturnErrMsg(functionName);				\
		return error;
	


//----------------------------------------------------------------------
// Stage settings
//----------------------------------------------------------------------

#define Stage_Config_File					"Modules\\XYstages\\Lang L-Step\\L-Step configuration.ls"
#define LStep4_DLL							"Modules\\XYstages\\Lang L-Step\\LStep4.dll"
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
static int			InitStageConnection						(LangLStep_type* stage, char** errorInfo);

	// closes an open connection to the Lang LStep stage controller
static int			CloseStageConnection					(LangLStep_type* stage, char** errorInfo);

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
	CloseStageConnection(stage, NULL);
	
	// discard DLL handle
	if (stage->lstepLibHndl) {
		FreeLibrary(stage->lstepLibHndl);
		stage->lstepLibHndl = 0;
	}
			
	// discard XYStage_type specific data
	discard_XYStage (mod);
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
	int					error		= 0;
	int					lstepError	= 0;
	DWORD				winError	= 0;
	char*				errMsg  	= NULL;
	
	LangLStep_type* 	stage		= (LangLStep_type*)mod;
	
	
	// load DLL
	nullWinChk( (stage->lstepLibHndl = LoadLibrary(LStep4_DLL)) );
	
	errChk( InitStageConnection(stage, &errMsg) );
	
	errChk( XYStage_Load(mod, workspacePanHndl, &errMsg) );
	
	ReturnLStepError("LStep Load");
}

//------------------------------------------------------------------------------------------------------------
// Stage operation
//------------------------------------------------------------------------------------------------------------


static int InitStageConnection (LangLStep_type* stage, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
#define InitStage_Err_ConnectionFailed		-3
#define InitStage_Err_ConfigFileNotLoaded	-4
#define InitStage_Err_ConfigParamNotSent	-5
#define InitStage_Err_SetStageUnits			-6
	
	int 		error					= 0;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	char*		errMsg					= NULL;
	
	FARPROC		procAddr				= NULL;
	
	// connect to stage
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_ConnectSimple") );
	LStepErrChk( (*(PConnectSimple)procAddr) (Stage_IO_InterfaceType, Stage_IO_COMPort, Stage_IO_BaudRate, FALSE) );
	
	// load stage configuration file
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_LoadConfig") );
	LStepErrChk( (*(PLoadConfig)procAddr) (Stage_Config_File) );
	
	// send configuration parameters to the stage
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_SetControlPars") );
	LStepErrChk( (*(PSetControlPars)procAddr) () );
	
	// set units to [mm]
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_SetDimensions") );
	LStepErrChk( (*(PSetDimensions)procAddr) (Stage_Units, Stage_Units, Stage_Units, Stage_Units) );

	ReturnLStepError("LStep InitStageConnection");
}


static int CloseStageConnection	(LangLStep_type* stage, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors

	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// disconnect from stage
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_Disconnect") );
	LStepErrChk( (*(PDisconnect)procAddr) () );
	
	ReturnLStepError("LStep CloseStageConnection"); 
}

static int MoveStage (LangLStep_type* stage, StageMoveTypes moveType, StageAxes axis, double moveVal, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors

	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;

	switch (moveType) {
			
		case XYSTAGE_MOVE_REL:
			
			switch(axis) {
			
				case XYSTAGE_X_AXIS:
			 
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_MoveRelSingleAxis") );
					LStepErrChk( (*(PMoveRelSingleAxis)procAddr) (XYSTAGE_X_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;
			
				case XYSTAGE_Y_AXIS:
			
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_MoveRelSingleAxis") );
					LStepErrChk( (*(PMoveRelSingleAxis)procAddr) (XYSTAGE_Y_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;	
			}
			break;
			
		case XYSTAGE_MOVE_ABS:
			
			switch(axis) {
			
				case XYSTAGE_X_AXIS:
			 
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_MoveAbsSingleAxis") );
					LStepErrChk( (*(PMoveAbsSingleAxis)procAddr) (XYSTAGE_X_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;
			
				case XYSTAGE_Y_AXIS:
			
					nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_MoveAbsSingleAxis") );
					LStepErrChk( (*(PMoveAbsSingleAxis)procAddr) (XYSTAGE_Y_AXIS + 1, moveVal, Stage_WaitUntilMovementComplete) );
					break;	
			}
			break;
	}
	
	ReturnLStepError("LStep MoveStage");	
}

static int StopStage (LangLStep_type* stage, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors

	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_StopAxes") );
	LStepErrChk( (*(PStopAxes)procAddr) () );
	
	ReturnLStepError("LStep StopStage");	
}

static int SetStageLimits (LangLStep_type* stage, double xNegativeLimit, double xPositiveLimit, double yNegativeLimit, double yPositiveLimit, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// set X axis limit
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_SetLimit") );
	LStepErrChk( (*(PSetLimit)procAddr) (XYSTAGE_X_AXIS + 1, xNegativeLimit, xPositiveLimit) );
	
	// set Y axis limit
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_SetLimit") );
	LStepErrChk( (*(PSetLimit)procAddr) (XYSTAGE_Y_AXIS + 1, yNegativeLimit, yPositiveLimit) );
	
	ReturnLStepError("LStep SetStageLimits");	
}

static int GetStageLimits (LangLStep_type* stage, double* xNegativeLimit, double* xPositiveLimit, double* yNegativeLimit, double* yPositiveLimit, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	
	// add code here
	
	
	
	ReturnLStepError("LStep GetStageLimits");
}

static int SetStageVelocity (LangLStep_type* stage, double velocity, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors

	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_SetVel") );
	LStepErrChk( (*(PSetVel)procAddr) (velocity, velocity, velocity, velocity) );
	
	ReturnLStepError("LStep SetStageVelocity");
}

static int GetStageVelocity (LangLStep_type* stage, double* velocity, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors

	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	// add code here
	
	ReturnLStepError("LStep GetStageVelocity");
	
}

static int GetStageAbsPosition (LangLStep_type* stage, double* xAbsPos, double* yAbsPos, char** errorInfo)
{
// return codes -1 and -2 are reserved for Windows and LStep API errors
	
	int 		error					= 0;
	char*		errMsg					= NULL;
	int			lstepError				= 0;
	DWORD		winError				= 0;
	
	FARPROC		procAddr				= NULL;
	
	double		xPos					= 0;
	double		yPos					= 0;
	double		zPos					= 0;
	double		aPos					= 0;
	
	nullWinChk( procAddr = GetProcAddress(stage->lstepLibHndl, "LS_GetPos") );
	LStepErrChk( (*(PGetPos)procAddr) (&xPos, &yPos, &zPos, &aPos) );
	
	*xAbsPos = xPos;
	*yAbsPos = yPos;
	
	ReturnLStepError("LStep GetStageAbsPosition");
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

