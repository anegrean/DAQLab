#include <userint.h>

//==============================================================================
//
// Title:		PIStage.c
// Purpose:		A short description of the implementation.
//
// Created on:	10-3-2014 at 12:06:57 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first 
#include <formatio.h> 
#include <ansi_c.h> 
#include "PIStage.h"
#include "PI_GCS2_DLL.h"	   
#include "asynctmr.h"

//==============================================================================
// Constants

//------------------------------------------------------------------------------
// Custom PID parameters and settings for the servo motion controller
//------------------------------------------------------------------------------
#define PIStage_PPAR						0x01	 	// P parameter list ID, see hardware documentation
#define PIStage_IPAR						0x02	 	// I parameter list ID
#define PIStage_DPAR						0x03	 	// D parameter list ID

#define PIStage_PTERM 						40		 
#define PIStage_ITERM 						1800
#define PIStage_DTERM 						3300
#define	PIStage_USECUSTOMPID				TRUE	 	// Overrides factory PID settings

//------------------------------------------------------------------------------
// Soft limits parameters for limiting the travel range of the stage
//------------------------------------------------------------------------------
#define PIStage_MAX_TRAVEL_RAGE_POS			0x15		// Maximum stage position in the positive direction
#define PIStage_MIN_TRAVEL_RAGE_POS			0x30		// Minimum stage position in the negative direction
												

//------------------------------------------------------------------------------
// Custom stage settings
//------------------------------------------------------------------------------
	
#define PIStage_SETTLING_PRECISION			0.0001		// Positive value, in [mm]
#define PIStage_SETTLING_TIMEOUT			3			// Timeout in [s]
#define PIStage_POSITION_UPDATE_INTERVAL	0.2			// Time intervals in [s] between position updates when using a joystick

//------------------------------------------------------------------------------
// USB connection ID
//------------------------------------------------------------------------------
#define PIStage_ID 							"0115500620"

//------------------------------------------------------------------------------
// PI Stage type
//------------------------------------------------------------------------------
#define PIStage_STAGE_TYPE 					"M-605.2DD"


//==============================================================================
// Types

typedef struct {
	Zstage_move_type	moveType;
	double				moveVal;
} MoveCommand_type;

//==============================================================================
// Static global variables

int						positionTrackerTimerID = 0;

//==============================================================================
// Static functions

static int							Load 								(DAQLabModule_type* mod, int workspacePanHndl);

static int							SaveCfg								(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int 							LoadCfg 							(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

static int							Move 								(Zstage_type* zstage, Zstage_move_type moveType, double moveVal);

static int							UseJoystick							(Zstage_type* zstage, BOOL useJoystick);

int CVICALLBACK 					TrackStagePosition 					(int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2);

static int							SetStageVelocity					(Zstage_type* zstage, double velocity);

static int							GetStageVelocity					(Zstage_type* zstage, double* velocity);

static int							TaskControllerMove					(PIStage_type* PIStage, Zstage_move_type moveType, double moveVal, char** errorInfo); 

static int							Stop								(Zstage_type* zstage);

static int							InitHardware 						(PIStage_type* PIstage);

static int							GetHWStageLimits					(Zstage_type* zstage, double* minimumLimit, double* maximumLimit);

static int							SetHWStageLimits					(Zstage_type* zstage, double minimumLimit, double maximumLimit);

//---------------------------------
// module task controller commands
//---------------------------------

static MoveCommand_type*			init_MoveCommand_type				(Zstage_move_type moveType, double moveVal);
static void							discard_MoveCommand_type			(MoveCommand_type** moveCommandPtr);


//-----------------------------------------
// PIStage Task Controller Callbacks
//-----------------------------------------

static int							ConfigureTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static void							IterateTC							(TaskControl_type* taskControl, BOOL const* abortIterationFlag);
static int							StartTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int							DoneTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int							StoppedTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int							TaskTreeStatus						(TaskControl_type* taskControl, TaskTreeStates status, char** errorInfo);
static int				 			ResetTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static void				 			ErrorTC 							(TaskControl_type* taskControl, int errorID, char errorMsg[]);
static int							ZStageEventHandler					(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorInfo);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_PIStage	(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	PIStage_type* 		PIzstage;
	Zstage_type*  		zstage;
	TaskControl_type*	tc;
	
	if (!mod) {
		PIzstage = malloc (sizeof(PIStage_type));
		if (!PIzstage) return NULL;
	} else
		PIzstage = (PIStage_type*) mod;
	
	zstage = (Zstage_type*) PIzstage;
	
	// initialize base class
	initalloc_Zstage ((DAQLabModule_type*)zstage, className, instanceName, workspacePanHndl);
	
	// create PIStage Task Controller
	tc = init_TaskControl_type (instanceName, PIzstage, DLGetCommonThreadPoolHndl(), ConfigureTC, NULL, IterateTC, StartTC, ResetTC, DoneTC, StoppedTC, TaskTreeStatus, NULL, ZStageEventHandler, ErrorTC);
	if (!tc) {discard_DAQLabModule((DAQLabModule_type**)&PIzstage); return NULL;}
	
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
	
		// adding Task Controller to module list
	ListInsertItem(zstage->baseClass.taskControllers, &tc, END_OF_LIST); 
	
		//METHODS
	
		// overriding methods
	zstage->baseClass.Discard 	= discard_PIStage;
	zstage->baseClass.Load 		= Load;
	zstage->baseClass.LoadCfg	= LoadCfg;
	zstage->baseClass.SaveCfg	= SaveCfg;
	
	//---------------------------
	// Child Level 1: Zstage_type 
	
		// DATA
		
			// Task Controller
	zstage->taskController		= tc;
	
	
		// METHODS
	
		// overriding methods
	zstage->MoveZ				= Move;
	zstage->SetStageVelocity	= SetStageVelocity;
	zstage->GetStageVelocity	= GetStageVelocity;
	zstage->UseJoystick			= UseJoystick;
	zstage->StopZ				= Stop;
	zstage->GetHWStageLimits	= GetHWStageLimits;
	zstage->SetHWStageLimits	= SetHWStageLimits;
		
		
	
	//--------------------------
	// Child Level 2: PIStage_type
	
		// DATA
		
	PIzstage->PIStageID			= -1;
	PIzstage->assignedAxis[0]	= 0;
	
	
		// METHODS
	
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) zstage;
	else
		return NULL;
}

void discard_PIStage (DAQLabModule_type** mod)
{
	PIStage_type* PIstage = (PIStage_type*) (*mod);
	
	if (!PIstage) return;
	
	// make sure the joystick is not left active
	UseJoystick (&PIstage->baseClass, FALSE); 
	
	// if connection was established and if still connected, close connection otherwise the DLL thread throws an error
	if (PIstage->PIStageID != -1)
		if (PI_IsConnected(PIstage->PIStageID))
			PI_CloseConnection(PIstage->PIStageID);
			
	// discard Zstage_type specific data
	discard_Zstage (mod);
}

static MoveCommand_type* init_MoveCommand_type (Zstage_move_type moveType, double moveVal)
{
	MoveCommand_type* a = malloc(sizeof(MoveCommand_type));
	if (!a) return NULL;
	
	a -> moveType 	= moveType;
	a -> moveVal	= moveVal;
	
	return a;
}

static void	discard_MoveCommand_type (MoveCommand_type** moveCommandPtr)
{
	if (!*moveCommandPtr) return;
	
	OKfree(*moveCommandPtr);
}

/// HIFN Loads PIStage motorized stage specific resources. 
static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	if (InitHardware((PIStage_type*) mod) < 0) return -1;
	
	// load generic Z stage resources
	ZStage_Load (mod, workspacePanHndl);
	
	return 0;
	
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
	int				error		= 0;
	PIStage_type* 	PIStage 	= (PIStage_type*) mod;  
	
	//--------------------------------------------------------
	// Saving PI Stage specific parameters
	//--------------------------------------------------------
	
	
	//---------------------------------------------------------
	// Saving Z Stage generic paramenters
	//---------------------------------------------------------
	
	errChk( ZStage_SaveCfg (mod, xmlDOM, moduleElement, xmlErrorInfo) );
	
	return 0;
	
Error:
	
	return error;
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
	int				error		= 0;
	PIStage_type* 	PIStage		= (PIStage_type*) mod;
	
	//---------------------------------------------------------
	// Loading PI Stage specific settings
	//---------------------------------------------------------
	
	
	//---------------------------------------------------------
	// Loading generic Z Stage settings
	//---------------------------------------------------------
	errChk( ZStage_LoadCfg(mod, moduleElement, xmlErrorInfo) ); 
	
	return 0;
	
Error:
	
	return error;
}

/// HIFN Moves a motorized stage 
static int Move (Zstage_type* zstage, Zstage_move_type moveType, double moveVal)
{
	return TaskControlEvent(zstage->taskController, TC_Event_Custom, init_MoveCommand_type(moveType, moveVal), (DiscardFptr_type)discard_MoveCommand_type); 
}

static int UseJoystick (Zstage_type* zstage, BOOL useJoystick)
{
	PIStage_type* 	PIstage				= (PIStage_type*) zstage;
	int 			PIerror;
	int				joystickAxis;
	char			buff[10000]			= ""; 
	char			msg[10000]			= ""; 
	
	
	Scan(PIstage->assignedAxis, "%s>%d", &joystickAxis);
	
	if (!PI_JAX(PIstage->PIStageID, joystickAxis, 1, PIstage->assignedAxis)) {
		PIerror = PI_GetError(PIstage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	 
	if (!PI_JON(PIstage->PIStageID, &joystickAxis, &useJoystick, 1)) {
		PIerror = PI_GetError(PIstage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	 
	// discard position tracker timer if there is any
	if (positionTrackerTimerID > 0) {
		DiscardAsyncTimer(positionTrackerTimerID);
		positionTrackerTimerID = 0;
	}
	// create new position tracker async timer
	if (useJoystick)
		positionTrackerTimerID = NewAsyncTimer(PIStage_POSITION_UPDATE_INTERVAL, -1, 1, TrackStagePosition, PIstage);
				   
	return 0;
}

int CVICALLBACK TrackStagePosition (int reserved, int timerId, int event, void* callbackData, int eventData1, int eventData2)
{
	PIStage_type* 	PIstage		= (PIStage_type*) callbackData;
	int 			PIerror;
	char			buff[10000]			= ""; 
	char			msg[10000]			= ""; 
	
	switch (event) {
			
		case EVENT_TIMER_TICK:
			// just make sure that if structure data has been disposed and there are still timer event in the queue, that nothing happens
			if (!PIstage) return 0;
			if (!(PIstage->baseClass.zPos)) return 0;
			
			if (!PI_qPOS(PIstage->PIStageID, PIstage->assignedAxis, PIstage->baseClass.zPos)) {
				OKfree(PIstage->baseClass.zPos);
				PIerror = PI_GetError(PIstage->PIStageID);
				if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
				Fmt(msg, "Error getting stage position. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
				DLMsg(msg, 1);
				return -1;
			}
			
			// update position in the UI
			(*PIstage->baseClass.UpdatePositionDisplay)	(&PIstage->baseClass);
			
			break;
	}
	
	return 0;
}

static int SetStageVelocity	(Zstage_type* zstage, double velocity)
{
	PIStage_type* 	PIstage				= (PIStage_type*) zstage;
	int 			PIerror;
	char			buff[10000]			= ""; 
	char			msg[10000]			= ""; 
	
	if (!PI_VEL(PIstage->PIStageID, PIstage->assignedAxis, &velocity)) {
		PIerror = PI_GetError(PIstage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	
	return 0;
}

static int GetStageVelocity	(Zstage_type* zstage, double* velocity)
{
	PIStage_type* 	PIstage				= (PIStage_type*) zstage;
	int 			PIerror;
	char			buff[10000]			= ""; 
	char			msg[10000]			= ""; 
	
	if (!PI_qVEL(PIstage->PIStageID, PIstage->assignedAxis, velocity)) {
		PIerror = PI_GetError(PIstage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	
	return 0;
}

/// HIFN Stops stage motion
static int Stop (Zstage_type* zstage)
{
	PIStage_type* 	PIstage 			= (PIStage_type*) zstage; 
		
	int 			PIerror;
	char			buff[10000]			=""; 
	char			msg[10000]			=""; 
	
	if (!PI_HLT(PIstage->PIStageID, PIstage->assignedAxis)) {
		PIerror = PI_GetError(PIstage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error stopping stage. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	
	// stop the Task Controller as well
	TaskControlEvent(zstage->taskController, TC_Event_Stop, NULL, NULL);
	
	return 0;
}

/// HIFN Connects to a PIStage motion controller and adjusts its motion parameters
/// HIRET 0 if connection was established and -1 for error
static int InitHardware (PIStage_type* PIstage)
{
	BOOL 			ServoONFlag				= TRUE; 
	BOOL			IsReferencedFlag;
	BOOL			HasLimitSwitchesFlag;
	BOOL			ReadyFlag;
	int				PIerror;
	size_t			idx;
	unsigned int	pidParameters[3]		= {PIStage_PPAR, PIStage_IPAR, PIStage_DPAR};  
	double			pidValues[3]			= {PIStage_PTERM, PIStage_ITERM, PIStage_DTERM};
	size_t			nOcurrences;
	size_t			nchars;
	char			msg[10000]				="";
	char			buff[10000]				="";
	char			stagelist[10000]			="";
	char			axes[255]				="";
	long 			ID; 						
	char 			szStrings[255]			="";
	
	
	// establish connection
	DLMsg("Connecting to PI stage controller...\n\n", 0); 
	if ((ID = PI_ConnectUSB(PIStage_ID)) < 0) {
		Fmt(msg,"Could not connect to PI stage controller with USB ID %s.\n\n", PIStage_ID);
		DLMsg(msg, 1);
		return -1;
	} else
		PIstage->PIStageID = ID;
	
	// print servo controller info
	if (!PI_qIDN(ID, buff, 10000)) {
		PIerror = PI_GetError(ID); 
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error reading PI stage controller info. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	} else {
		Fmt(msg, "Connected to: %s\n", buff);
		DLMsg(msg,0);
	}
	
	// get all possible axes that the servo controller can move
	if (!PI_qSAI_ALL(ID, axes, 255)) {
		PIerror = PI_GetError(ID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error getting PI stage controller available axes. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	
	// assign by default the first axis found if this is not already set
	if (!PIstage->assignedAxis[0]) {
		PIstage->assignedAxis[0] = axes[0];
		PIstage->assignedAxis[1] = 0;		// set the null character
	}
	
	// check if stages database is present from which parameters can be loaded
	if (!PI_qVST(ID, stagelist, 10000)) {
		PIerror = PI_GetError(ID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error getting list of installed stages. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	// process buff to check if there are any stages in the list by checking the number of times "\n" or number 10 is present
	// if there are no stages, then buff will contain "NOSTAGES \n" and thus only one occurrence of "\n".
	idx 		= 0;
	nOcurrences = 0;
	nchars		= strlen(stagelist);
	while (idx < 10000 && idx < nchars) {
		if (stagelist[idx] == 10) nOcurrences++; 
		idx++;
	}
	
	if (nOcurrences <= 1) {
		DLMsg("Error loading PI stage controller. No stage database was found.\n\n", 1);
		return -1;
	}
	
	// loading stage specific info (mechanics, limits, factory PID values, etc.)
	DLMsg("Loading PI stage servo controller settings...", 0);
	if (!PI_CST(ID, PIstage->assignedAxis, PIStage_STAGE_TYPE)) {
		PIerror = PI_GetError(ID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "\n\nError loading factory stage settings. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	} else
		DLMsg("Loaded.\n\n", 0);
		
	// adjust PID settings simultaneously for the assigned axis
	if (PIStage_USECUSTOMPID)
		for (int i = 0; i < 3; i++)
			if (!PI_SPA(ID, PIstage->assignedAxis, pidParameters + i, pidValues + i, szStrings)){
				PIerror = PI_GetError(ID);
				if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
				Fmt(msg, "Error changing PID servo parameters. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
				DLMsg(msg, 1);
				return -1;	
			}
	
	// if any of the low, mid or high velocities is missing, set them to current stage velocity
	if (!PIstage->baseClass.lowVelocity) {
		PIstage->baseClass.lowVelocity = malloc(sizeof(double));
		GetStageVelocity(&PIstage->baseClass, PIstage->baseClass.lowVelocity);
	}
	
	if (!PIstage->baseClass.midVelocity) {
		PIstage->baseClass.midVelocity = malloc(sizeof(double));
		GetStageVelocity(&PIstage->baseClass, PIstage->baseClass.midVelocity);
	}
	
	if (!PIstage->baseClass.highVelocity) {
		PIstage->baseClass.highVelocity = malloc(sizeof(double));
		GetStageVelocity(&PIstage->baseClass, PIstage->baseClass.highVelocity);
	}
	
	// set stage to high velocity for speeding up the referencing
	SetStageVelocity(&PIstage->baseClass, *PIstage->baseClass.highVelocity);
		
	// turn servo on for the given axis
	if (!PI_SVO(ID, PIstage->assignedAxis, &ServoONFlag)) {
		PIerror = PI_GetError(ID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error turning servo ON. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	
	// set stage limits if available from loaded settings
	if (PIstage->baseClass.zMinimumLimit && PIstage->baseClass.zMaximumLimit) 
		(*PIstage->baseClass.SetHWStageLimits)	((Zstage_type*)PIstage, *PIstage->baseClass.zMinimumLimit, *PIstage->baseClass.zMaximumLimit);
	
	// make sure the joystick is not active before referencing
	UseJoystick (&PIstage->baseClass, FALSE); 
	
	// check if axis is already referenced
	if (!PI_qFRF(ID, PIstage->assignedAxis, &IsReferencedFlag)) {   
		PIerror = PI_GetError(ID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error checking if axis is referenced already. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
	
	if (IsReferencedFlag) {
		// if referenced read in current position
		if (!PIstage->baseClass.zPos) {
			PIstage->baseClass.zPos = malloc(sizeof(double));
			if(!PIstage->baseClass.zPos) return -1;
		}
		if (!PI_qPOS(ID, PIstage->assignedAxis, PIstage->baseClass.zPos)) {
			OKfree(PIstage->baseClass.zPos);
			PIerror = PI_GetError(ID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
			Fmt(msg, "Error getting stage position. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
			DLMsg(msg, 1);
			return -1;
		}
	} else {
		// check if there are limit switches
		if (!PI_qLIM(ID, PIstage->assignedAxis, &HasLimitSwitchesFlag)) {
			PIerror = PI_GetError(ID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
			Fmt(msg, "Checking if stage has limit switches failed. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
			DLMsg(msg, 1);
			return -1;
		}
		 
		// reference axis
		if (HasLimitSwitchesFlag) {
			if (!PI_FPL(ID, PIstage->assignedAxis)) {
				PIerror = PI_GetError(ID);
				if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
				Fmt(msg, "Referencing stage failed. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
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
			if (!PI_qONT(ID, PIstage->assignedAxis, &ReadyFlag)) {
				PIerror = PI_GetError(ID);
				if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
				Fmt(msg, "Could not query if target reached. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
				DLMsg(msg, 1);
				return -1;
			}
		} while (!ReadyFlag);
		
		Sleep(PIStage_SETTLING_TIMEOUT * 1000);	// make sure stage settles before reading position
		
		// check if stage is referenced
		if (!PI_qFRF(ID, PIstage->assignedAxis, &IsReferencedFlag)) {   
			PIerror = PI_GetError(ID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
			Fmt(msg, "Error checking if stage is referenced already. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
			DLMsg(msg, 1);
			return -1;
		}
	
		if (IsReferencedFlag) {
			// if referenced read in current position
			if (!PIstage->baseClass.zPos) {
				PIstage->baseClass.zPos = malloc(sizeof(double));
				if(!PIstage->baseClass.zPos) return -1;
			}
			
			if (!PI_qPOS(ID, PIstage->assignedAxis, PIstage->baseClass.zPos)) {
				OKfree(PIstage->baseClass.zPos);
				PIerror = PI_GetError(ID);
				if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
				Fmt(msg, "Error getting stage position. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
				DLMsg(msg, 1);
				return -1;
			}
			// read stage limits if they are not loaded from a file already
			if (!PIstage->baseClass.zMinimumLimit || !PIstage->baseClass.zMaximumLimit) {
				if (!PIstage->baseClass.zMinimumLimit) {
					PIstage->baseClass.zMinimumLimit = malloc(sizeof(double));
					if (!PIstage->baseClass.zMinimumLimit) return -1;
				}
			
				if (!PIstage->baseClass.zMaximumLimit) {
					PIstage->baseClass.zMaximumLimit = malloc(sizeof(double));
					if (!PIstage->baseClass.zMaximumLimit) return -1;
				}
			
				(*PIstage->baseClass.GetHWStageLimits)	((Zstage_type*)PIstage, PIstage->baseClass.zMinimumLimit, PIstage->baseClass.zMaximumLimit);
			}
			
		} else {
			// referencing failed
			DLMsg("Failed to reference stage.\n\n", 1);
			return -1;
		}
	}
	
	return 0;
}

static int GetHWStageLimits (Zstage_type* zstage, double* minimumLimit, double* maximumLimit)
{ 
	PIStage_type* 		PIStage 		= (PIStage_type*) zstage;
	int					PIerror;
	char				buff[10000]		= "";
	char				msg[10000]		= "";
	
	if (!PI_qTMN(PIStage->PIStageID, PIStage->assignedAxis, minimumLimit)) {
		PIerror = PI_GetError(PIStage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error getting negative position limit. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;	
	}
	
	if (!PI_qTMX(PIStage->PIStageID, PIStage->assignedAxis, maximumLimit)) {
		PIerror = PI_GetError(PIStage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error getting positive position limit. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;	
	}
	
	return 0;
}

static int SetHWStageLimits	(Zstage_type* zstage, double minimumLimit, double maximumLimit)
{
	PIStage_type* 		PIStage 			= (PIStage_type*) zstage;
	int					PIerror;
	char				buff[10000]			= "";
	char				msg[10000]			= "";
	unsigned int		limitsParamID[2]	= {PIStage_MAX_TRAVEL_RAGE_POS, PIStage_MIN_TRAVEL_RAGE_POS};
	char 				szStrings[255]		= "";
	BOOL				HasLimitSwitchesFlag;
	BOOL				ReadyFlag;
	
	// set maximum stage position limit
	if (!PI_SPA(PIStage->PIStageID, PIStage->assignedAxis, limitsParamID, &maximumLimit, szStrings)){
		PIerror = PI_GetError(PIStage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error setting maximum stage position limit. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;	
	}
	
	// set minimum stage position limit
	if (!PI_SPA(PIStage->PIStageID, PIStage->assignedAxis, &limitsParamID[1], &minimumLimit, szStrings)){
		PIerror = PI_GetError(PIStage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Error setting minimum stage position limit. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;	
	}
	
	// reference axis again
	// check if there are limit switches
	if (!PI_qLIM(PIStage->PIStageID, PIStage->assignedAxis, &HasLimitSwitchesFlag)) {
		PIerror = PI_GetError(PIStage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
		Fmt(msg, "Checking if stage has limit switches failed. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
		DLMsg(msg, 1);
		return -1;
	}
		 
	// reference axis
	if (HasLimitSwitchesFlag) {
		if (!PI_FPL(PIStage->PIStageID, PIStage->assignedAxis)) {
			PIerror = PI_GetError(PIStage->PIStageID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
			Fmt(msg, "Referencing stage failed. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
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
		if (!PI_qONT(PIStage->PIStageID, PIStage->assignedAxis, &ReadyFlag)) {
			PIerror = PI_GetError(PIStage->PIStageID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0; 
			Fmt(msg, "Could not query if target reached. PI stage DLL Error ID %d: %s.\n\n", PIerror, buff);
			DLMsg(msg, 1);
			return -1;
		}
	} while (!ReadyFlag);
		
	Sleep(PIStage_SETTLING_TIMEOUT * 1000);	// make sure stage settles before reading position
	
	return 0;
}

//-----------------------------------------
// PI ZStage Task Controller Callbacks
//-----------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	Zstage_type* 	zstage  = GetTaskControlModuleData(taskControl);
	
	// set number of Task Controller iterations
	// first iteration the stage doesn't move from the starting position, therefore for nZSteps there will be nZSteps+1 iterations
	SetTaskControlIterations(taskControl, zstage->nZSteps + 1);
	
	return 0;
}

static void IterateTC (TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
	Zstage_type* 		zstage 			= GetTaskControlModuleData(taskControl);
	Iterator_type*		currentiter 	= GetTaskControlCurrentIter(taskControl);
	size_t 				iterindex		= GetCurrentIterationIndex(currentiter);
	int					error			= 0;
	char*				errMsg			= NULL;
	
	// update iteration counter
	if (zstage->SetStepCounter)
		(*zstage->SetStepCounter) (zstage, iterindex);
	
	// move to start position for the first iteration
	if (!iterindex) {
		errChk( TaskControllerMove ((PIStage_type*)zstage, ZSTAGE_MOVE_ABS, *zstage->startAbsPos, &errMsg) );
		TaskControlIterationDone(taskControl, 0, NULL, FALSE);
		return;
	}
	
	// step stage
	if (zstage->endRelPos > 0)
		errChk( TaskControllerMove ((PIStage_type*)zstage, ZSTAGE_MOVE_REL, zstage->stepSize, &errMsg) );
	else
		errChk( TaskControllerMove ((PIStage_type*)zstage, ZSTAGE_MOVE_REL, -zstage->stepSize, &errMsg) );
	
	
	TaskControlIterationDone(taskControl, 0, NULL, FALSE);
	return;
	
Error:
	
	TaskControlIterationDone(taskControl, error, errMsg, FALSE);
	OKfree(errMsg);
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PIStage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	// dim items
	(*zstage->baseClass.DimWhenRunning) ((Zstage_type*)zstage, TRUE);
	
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PIStage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	// undim items
	(*zstage->baseClass.DimWhenRunning) ((Zstage_type*)zstage, FALSE);
	
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	PIStage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	// undim items
	(*zstage->baseClass.DimWhenRunning) ((Zstage_type*)zstage, FALSE);
	
	return 0;
}

static int	TaskTreeStatus (TaskControl_type* taskControl, TaskTreeStates status, char** errorInfo)
{
	//PIStage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	Zstage_type* 		zstage = GetTaskControlModuleData(taskControl);
	
	// turn off error LED
	if (zstage->StatusLED)
		(*zstage->StatusLED) (zstage, ZSTAGE_LED_IDLE);
	
	if (zstage->SetStepCounter)
		(*zstage->SetStepCounter) (zstage, 0); 
	
	return 0;
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char errorMsg[])
{
	PIStage_type* 		PIStage = GetTaskControlModuleData(taskControl);
	Zstage_type*		zstage	= (Zstage_type*) PIStage;
	
	// turn on error LED
	if (zstage->StatusLED)
		(*zstage->StatusLED) (zstage, ZSTAGE_LED_ERROR);
	
	// update position values
	if (zstage->UpdatePositionDisplay)
	(*zstage->UpdatePositionDisplay) (zstage);	
	
	// print error message
	DLMsg(errorMsg, 1);
	DLMsg("\n\n", 0);
	
	// undim items
	(*zstage->DimWhenRunning) (zstage, FALSE);
	
}

static int ZStageEventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	MoveCommand_type*	command 		= eventData;
	
	return TaskControllerMove (GetTaskControlModuleData(taskControl), command->moveType, command->moveVal, errorInfo); 
}

static int TaskControllerMove (PIStage_type* PIStage, Zstage_move_type moveType, double moveVal, char** errorInfo)
{
#define TaskControllerMove_Err_PIError		-1
	
	Zstage_type*			zStage			= (Zstage_type*) PIStage; 
	BOOL					ReadyFlag;
	int						PIerror			= 0;
	int						error			= 0;
	char					msg[10000]		= "";
	char					buff[10000]		= "";
	double					actualPos;
	double					targetPos;
	double					time;
	BOOL					moveTypeFlag	= 0;
	
	switch (moveType) {
			
		case ZSTAGE_MOVE_REL:
			
			moveTypeFlag = TRUE;
			
			break;
			
		case ZSTAGE_MOVE_ABS:
			
			moveTypeFlag = FALSE;
			
			break; 
	}
			
	// move relative to current position or to new absolute position with given amount in [mm]
	targetPos = (*zStage->zPos) * moveTypeFlag + moveVal;
	if (!PI_MOV(PIStage->PIStageID, PIStage->assignedAxis, &targetPos)) {
		PIerror = PI_GetError(PIStage->PIStageID);
		if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0;
		Fmt(msg, "Moving stage relative to current position failed. PI stage DLL Error ID %d: %s.", PIerror, buff);
		error = TaskControllerMove_Err_PIError;
		goto Error;
	}
			
	// turn on moving LED
	if (zStage->StatusLED)
		(*zStage->StatusLED) (zStage, ZSTAGE_LED_MOVING);
	
	// wait until stage is on target
	do {
		Sleep(100);
		if (!PI_qONT(PIStage->PIStageID, PIStage->assignedAxis, &ReadyFlag)) {
			PIerror = PI_GetError(PIStage->PIStageID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0;
			Fmt(msg, "Could not query if target reached. PI stage DLL Error ID %d: %s.", PIerror, buff);
			error = TaskControllerMove_Err_PIError;
			goto Error;
		}
	} while (!ReadyFlag);
	
	// wait until stage settles within given precision
	time = Timer();
	do {
		// get target position
		if (!PI_qMOV(PIStage->PIStageID, PIStage->assignedAxis, &targetPos)) {
			PIerror = PI_GetError(PIStage->PIStageID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0;
			Fmt(msg, "Error getting target position. PI stage DLL Error ID %d: %s.", PIerror, buff);
			error = TaskControllerMove_Err_PIError;
			goto Error;
		}
		Sleep (100);
		// get current position
		if (!PI_qPOS(PIStage->PIStageID, PIStage->assignedAxis, &actualPos)) {
			PIerror = PI_GetError(PIStage->PIStageID);
			if (!PI_TranslateError(PIerror, buff, 10000)) buff[0]=0;
			Fmt(msg, "Error getting current position. PI stage DLL Error ID %d: %s.", PIerror, buff);
			error = TaskControllerMove_Err_PIError;
			goto Error;
		}
	} while ( (( actualPos > targetPos + PIStage_SETTLING_PRECISION/2.0) ||
			   ( actualPos < targetPos - PIStage_SETTLING_PRECISION/2.0)) && (Timer() < time + PIStage_SETTLING_TIMEOUT));	// quit loop if after 2 sec positions don't match
			
	// turn off moving LED
	if (zStage->StatusLED)
		(*zStage->StatusLED) (zStage, ZSTAGE_LED_IDLE);
			
	// update position in structure data
	*zStage->zPos = actualPos;
			
	// update displayed position
	if (zStage->UpdatePositionDisplay)
		(*zStage->UpdatePositionDisplay) (zStage);
			
	return 0;
	
Error:
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "TaskControllerMove", msg);
	
	return error;
}

