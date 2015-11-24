//==============================================================================
//
// Title:		TaskController.c
// Purpose:		Task Controller State Machine.
//
// Created on:	2014.05.11. at 10:35:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================										    
// Include files
#include "DAQLabUtility.h"
#include <windows.h>
#include "asynctmr.h"
#include <formatio.h>
#include "TaskController.h"
#include "VChannel.h"
#include "DAQLab.h"
#include "HWTriggering.h"
#include "Iterator.h"
#include "DataTypes.h"

//==============================================================================
// Constants

#define EVENT_BUFFER_SIZE 10											// Number of maximum events read at once from the Task Controller TSQs

#ifndef TC_WRONG_EVENT_STATE_ERROR
#define TC_WRONG_EVENT_STATE_ERROR \
{ \
	eventStr 	= EventToString(eventPackets[i].event); \
	stateStr 	= TaskControlStateToString(taskControl->currentState); \
	nChars 		= snprintf(msgBuff, 0, "%s event is invalid for %s state", eventStr, stateStr); \
	nullChk( msgBuff = malloc ((nChars + 1) * sizeof(char)) ); \
	snprintf(msgBuff, nChars + 1, "%s event is invalid for %s state", eventStr, stateStr); \
	OKfree(eventStr); \
	OKfree(stateStr); \
	SET_ERR(TaskEventHandler_Error_InvalidEventInState, msgBuff); \
}
#endif


//==============================================================================
// Types

typedef struct {
	size_t							childTCIdx;							// For a TC_Event_UpdateChildTCState event this is a
																		// 1-based index of childTC (from childTCs list) which generated
																		// the event.
	TCStates						newChildTCState;					// For a TC_Event_UpdateChildTCState event this is the new state
																		// of the childTC
} ChildTCEventInfo_type;

typedef struct {
	TCEvents 						event;								// Task Control Event.
	void*							eventData;							// Extra information per event allocated dynamically
	DiscardFptr_type   				discardEventDataFptr;   			// Function pointer to dispose of the eventData
} EventPacket_type;

typedef struct {
	TCStates						childTCState;						// Updated by parent task when informed by childTC that a state change occured.
	TCStates						previousChildTCState;				// Previous child TC state used for logging and debuging.
	TaskControl_type*				childTC;							// Pointer to child TC.
	BOOL							isOutOfDate;						// If True, state of child is not known to the parent and the parent must be updated by its child.
} ChildTCInfo_type;

typedef enum {
	STATE_CHANGE,
	FUNCTION_CALL,
	CHILD_TASK_STATE_UPDATE
} TaskControllerActions;

// Structure binding Task Controller and VChan data for passing to TSQ callback	
typedef struct {
	TaskControl_type* 				taskControl;
	SinkVChan_type* 				sinkVChan;
	DataReceivedFptr_type			DataReceivedFptr;
	CmtTSQCallbackID				itemsInQueueCBID;
} VChanCallbackData_type;

struct TaskControl {
	// Task control data
	char*							taskName;							// Name of Task Controller
	size_t							childTCIdx;							// 1-based index of childTC from parent Task childTCs list. If task doesn't have a parent task then index is 0.
	CmtTSQHandle					eventQ;								// Event queue to which the state machine reacts.
	CmtThreadLockHandle				eventQThreadLock;					// Thread lock used to coordinate multiple writing threads sending events to the queue.
	ListType						dataQs;								// Incoming data queues, list of VChanCallbackData_type*.
	ListType						sourceVChans;						// Source VChans associated with the task controller of SourceVChan_type*.
	unsigned int					eventQThreadID;						// Thread ID in which queue events are processed.
	CmtThreadFunctionID				threadFunctionID;					// ID of ScheduleTaskEventHandler that is executed in a separate thread from the main thread.
	CmtThreadPoolHandle				threadPoolHndl;						// Thread pool handle used to launch task controller threads.
	CmtTSVHandle					stateTSV;							// Task Controller state, thread safe variable of TCStates.
	int								stateTSVLineNumDebug;				// Used to find out where in the code the state TSV is obtained and is not released.
	char							stateTSVFileName[200];				// Used to find out where in the code the state TSV is obtained and is not released. 
	BOOL							stateTSVLockObtained;				// Status of stateTSV lock.
	TCStates						currentState;						// Current Task Controller state for internal use. Its value is updated from stateTSV before processing an event.
	TCStates 						oldState;							// Previous Task Controller state used for logging.
	size_t							repeat;								// Total number of repeats. If repeat is 0, then the iteration function is not called. 
	int								iterTimeout;						// Timeout in [s] until when TaskControlIterationDone can be called. 
	
	TCExecutionModes				executionMode;						// Determines how the iteration block of a Task Controller is executed with respect to its childTCs if any.
	TaskMode_type					mode;								// Finite or continuous type of task controller
	Iterator_type* 					currentIter;						// iteration information structure
	TaskControl_type*				parentTC;							// Pointer to parent task that own this childTC. 
																		// If this is the main task, it has no parent and this is NULL. 
	ListType						childTCs;							// List of childTCs of ChildTCInfo_type.
	void*							moduleData;							// Reference to module specific data that is controlled by the task.
	int								logPanHandle;						// Panel handle in which there is a box control for printing Task Controller execution info useful for debugging. If not used, it is set to 0
	int								logBoxControlID;					// Box control ID for printing Task Controller execution info useful for debugging. If not used, it is set to 0.  
	BOOL							loggingEnabled;						// If True, logging info is printed to the provided Box control.
	char*							errorMsg;							// When switching to an error state, additional error info is written here.
	int								errorID;							// Error code encountered when switching to an error state.
	double							waitBetweenIterations;				// During a RUNNING state, waits specified ammount in seconds between iterations
	BOOL							abortFlag;							// If True, it signals the provided callback functions that they must terminate.
	BOOL							stopIterationsFlag;					// if True, no further TC iterations are performed.
	int								nIterationsFlag;					// When -1, the Task Controller is iterated continuously, 0 iteration stops and 1 one iteration.
	int								iterationTimerID;					// Keeps track of the timeout timer when iteration is performed in another thread.
	BOOL							UITCFlag;							// If TRUE, the Task Controller is meant to be used as an User Interface Task Controller that allows the user to control a Task Tree.
	
	// Event handler function pointers
	ConfigureFptr_type				ConfigureFptr;
	UnconfigureFptr_type			UnconfigureFptr;
	IterateFptr_type				IterateFptr;
	StartFptr_type					StartFptr;
	ResetFptr_type					ResetFptr;
	DoneFptr_type					DoneFptr;
	StoppedFptr_type				StoppedFptr;
	IterationStopFptr_type			IterationStopFptr;
	TaskTreeStateChangeFptr_type	TaskTreeStateChangeFptr;
	SetUITCModeFptr_type			SetUITCModeFptr;
	ModuleEventFptr_type			ModuleEventFptr;
	ErrorFptr_type					ErrorFptr;
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void 								TaskEventHandler 						(TaskControl_type* taskControl); 
static void 								AddIterationEventWithPriority 			(EventPacket_type* eventPackets, int* nEventPackets, int currentEventIdx);

// Use this function to change the state of a Task Controller
static void 								ChangeState 							(TaskControl_type* taskControl, EventPacket_type* eventPacket, TCStates newState);
// If True, the state of all child TCs is up to date and the same as the given state.
static BOOL 								AllChildTCsInState 						(TaskControl_type* taskControl, TCStates state); 
// Use this function to carry out a Task Controller action using provided function pointers
static int									FunctionCall 							(TaskControl_type* taskControl, EventPacket_type* eventPacket, TCCallbacks fID, void* fCallData, char** errorMsg); 

// Formats a Task Controller log entry based on the action taken
static void									ExecutionLogEntry						(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskControllerActions action, void* info);

static char*								EventToString							(TCEvents event);
static char*								FCallToString							(TCCallbacks fcall);

// ChildTC eventData functions
static ChildTCEventInfo_type* 				init_ChildTCEventInfo_type 				(size_t childTCIdx, TCStates state);
static void									discard_ChildTCEventInfo_type 			(ChildTCEventInfo_type** eventDataPtr);

// VChan and Task Control binding
static VChanCallbackData_type*				init_VChanCallbackData_type				(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr, CmtTSQCallbackID itemsInQueueCBID);
static void									discard_VChanCallbackData_type			(VChanCallbackData_type** VChanCBDataPtr);

// Informs recursively Task Controllers about the Task Tree status when it changes (active/inactive).
static int									TaskTreeStateChange		 				(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskTreeStates state, char** errorMsg);

// Clears recursively all data packets from the Sink VChans of all Task Controllers in a Task Tree Branch starting with the given Task Controller.
static int									ClearTaskTreeBranchVChans				(TaskControl_type* taskControl, char** errorMsg);

static void									SetChildTCsOutOfDate					(TaskControl_type* taskControl);

// macro for debugging where certain events are generated
static int*									NumTag									(int num);

//==============================================================================
// Global variables

int 	mainThreadID;			// Main thread ID where UI callbacks are processed

//==============================================================================
// Global functions

void CVICALLBACK 							TaskEventItemsInQueue 					(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

void CVICALLBACK 							TaskDataItemsInQueue 					(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

int CVICALLBACK 							ScheduleTaskEventHandler 				(void* functionData);

int CVICALLBACK 							ScheduleIterateFunction 				(void* functionData); 

//void CVICALLBACK 							TaskEventHandlerExecutionCallback 		(CmtThreadPoolHandle poolHandle, CmtThreadFunctionID functionID, unsigned int event, int value, void *callbackData);

int CVICALLBACK 							TaskControlIterTimeout 					(int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2); 

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller creation / destruction functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

/// HIFN Initializes a Task controller.
TaskControl_type* init_TaskControl_type(const char						taskControllerName[],
										void*							moduleData,
										CmtThreadPoolHandle				tcThreadPoolHndl, 
										ConfigureFptr_type 				ConfigureFptr,
										UnconfigureFptr_type			UnconfigureFptr,
										IterateFptr_type				IterateFptr,
										StartFptr_type					StartFptr,
										ResetFptr_type					ResetFptr,
										DoneFptr_type					DoneFptr,
										StoppedFptr_type				StoppedFptr,
										IterationStopFptr_type			IterationStopFptr,
										TaskTreeStateChangeFptr_type	TaskTreeStateChangeFptr,
										SetUITCModeFptr_type			SetUITCModeFptr,
										ModuleEventFptr_type			ModuleEventFptr,
										ErrorFptr_type					ErrorFptr)
{
INIT_ERR	
	
	TCStates* 			tcStateTSVPtr 	= NULL; 
	
	TaskControl_type* 	tc 				= malloc (sizeof(TaskControl_type));
	
	if (!tc) return NULL;
	
	//-------------------------------------------------------------------------------------------------------------------
	// Initialization
	//-------------------------------------------------------------------------------------------------------------------
	
	//--------------------------------------
	// task controller data
	
	tc->eventQ								= 0;
	tc->eventQThreadLock					= 0;
	tc->dataQs								= 0;
	tc->sourceVChans						= 0;
	tc->childTCs							= 0;
	tc->taskName							= NULL;
	tc->eventQThreadID						= CmtGetCurrentThreadID ();
	tc->threadFunctionID					= 0;
	tc->moduleData							= moduleData;
	tc->threadPoolHndl						= tcThreadPoolHndl;
	tc->childTCIdx							= 0;
	tc->stateTSV							= 0;
	tc->stateTSVLineNumDebug				= 0;
	tc->stateTSVFileName[0]					= 0;
	tc->stateTSVLockObtained				= FALSE;
	tc->currentState						= TC_State_Unconfigured;
	tc->oldState							= TC_State_Unconfigured;
	tc->repeat								= 1;
	tc->iterTimeout							= 10;								
	tc->executionMode						= TC_Execute_BeforeChildTCs;
	tc->mode								= TASK_FINITE;
	tc->currentIter							= NULL;
	tc->parentTC							= NULL;
	tc->logPanHandle						= 0;
	tc->logBoxControlID						= 0;
	tc->loggingEnabled						= FALSE;
	tc->errorMsg							= NULL;
	tc->errorID								= 0;
	tc->waitBetweenIterations				= 0;
	tc->abortFlag							= FALSE;
	tc->stopIterationsFlag					= FALSE;
	tc->nIterationsFlag						= -1;
	tc->iterationTimerID					= 0;
	tc->UITCFlag							= FALSE;
	
	//--------------------------------------
	// task controller callbacks
	
	tc -> ConfigureFptr 					= ConfigureFptr;
	tc -> UnconfigureFptr					= UnconfigureFptr;
	tc -> IterateFptr						= IterateFptr;
	tc -> StartFptr							= StartFptr;
	tc -> ResetFptr							= ResetFptr;
	tc -> DoneFptr							= DoneFptr;
	tc -> StoppedFptr						= StoppedFptr;
	tc -> IterationStopFptr					= IterationStopFptr;
	tc -> TaskTreeStateChangeFptr			= TaskTreeStateChangeFptr;
	tc -> SetUITCModeFptr					= SetUITCModeFptr;
	tc -> ModuleEventFptr					= ModuleEventFptr;
	tc -> ErrorFptr							= ErrorFptr;
	
	//-------------------------------------------------------------------------------------------------------------------
	// Allocation (may fail)
	//-------------------------------------------------------------------------------------------------------------------
	
	errChk( CmtNewTSV(sizeof(TCStates), &tc->stateTSV) );
	errChk( CmtNewTSQ(TC_NEventQueueItems, sizeof(EventPacket_type), 0, &tc->eventQ) );
	errChk( CmtNewLock(NULL, 0, &tc->eventQThreadLock) );
	nullChk( tc->dataQs						= ListCreate(sizeof(VChanCallbackData_type*)) );
	nullChk( tc->sourceVChans				= ListCreate(sizeof(SourceVChan_type*)) );
	nullChk( tc->childTCs					= ListCreate(sizeof(ChildTCInfo_type)) );
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	errChk( CmtInstallTSQCallback(tc->eventQ, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskEventItemsInQueue, tc, tc->eventQThreadID, NULL) ); 
	nullChk( tc->taskName 					= StrDup(taskControllerName) );
	nullChk( tc->currentIter				= init_Iterator_type(taskControllerName) );
	
	//--------------------------------------------------------------------------------------------------------------------
	// Initialize task controller state
	//--------------------------------------------------------------------------------------------------------------------
	
	errChk( CmtGetTSVPtr(tc->stateTSV, &tcStateTSVPtr) );
	*tcStateTSVPtr = TC_State_Unconfigured;
	errChk( CmtReleaseTSVPtr(tc->stateTSV) );
	tcStateTSVPtr = NULL;
	
	return tc;
	
Error:
	
	//----------------------------
	// cleanup
	//----------------------------
	
	// release state TSV lock if obtained
	if (tcStateTSVPtr) {CmtReleaseTSVPtr(tc->stateTSV); tcStateTSVPtr = NULL;}
	
	discard_TaskControl_type(&tc);
	
	return NULL;
}

/// HIFN Discards recursively a Task controller.
void discard_TaskControl_type(TaskControl_type** taskControllerPtr)
{
	TaskControl_type*	tc = *taskControllerPtr;	
	
	if (!tc) return;
	
	//----------------------------------------------------------------------------
	// Disassemble task tree branch recusively starting with the given parent node
	//----------------------------------------------------------------------------
	DisassembleTaskTreeBranch(tc, NULL);
	
	//----------------------------------------------------------------------------
	// Discard this Task Controller
	//----------------------------------------------------------------------------
	
	// incoming data queues (does not free the queue itself!)
	if (tc->dataQs) {
		RemoveAllSinkVChans(tc, NULL);
		OKfreeList(tc->dataQs);
	}
	
	// discard state TSV
	if (tc->stateTSV) {
		CmtDiscardTSV(tc->stateTSV); 
		tc->stateTSV = 0;
	}
	
	// event queue
	if (tc->eventQ) {
		CmtDiscardTSQ(tc->eventQ);
		tc->eventQ = 0;
	}
	
	// name
	OKfree(tc->taskName);
	
	// event queue thread lock
	if (tc->eventQThreadLock) {
		CmtDiscardLock(tc->eventQThreadLock); 
		tc->eventQThreadLock = 0;
	}
	
	// source VChan list
	OKfreeList(tc->sourceVChans);
	
	// child Task Controllers list
	OKfreeList(tc->childTCs);
	
	// release thread
	if (tc->threadFunctionID) CmtReleaseThreadPoolFunctionID(tc->threadPoolHndl, tc->threadFunctionID);   
	
	// error message storage 
	OKfree(tc->errorMsg);
	
	//iteration info
	discard_Iterator_type (&tc->currentIter); 

	// free Task Controller memory
	OKfree(*taskControllerPtr);
}

void discard_TaskTreeBranch (TaskControl_type** taskControllerPtr)
{
	ChildTCInfo_type* 	childTCPtr		= NULL;
	TaskControl_type*   taskController  = *taskControllerPtr;
	
	while(ListNumItems(taskController->childTCs)){
		childTCPtr = ListGetPtrToItem(taskController->childTCs, 1);
		discard_TaskTreeBranch(&childTCPtr->childTC);
	} 
	
	discard_TaskControl_type(taskControllerPtr);
}

void EnableTaskControlLogging (TaskControl_type* taskControl, BOOL enableLogging)
{
	taskControl->loggingEnabled =  enableLogging;
}

void SetTaskControlUILoggingInfo (TaskControl_type* taskControl, int logPanHandle, int logBoxControlID)
{
	taskControl->logPanHandle 		= logPanHandle;
	taskControl->logBoxControlID	= logBoxControlID;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller set/get functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

char* GetTaskControlName (TaskControl_type* taskControl)
{
	return StrDup(taskControl->taskName);
}

void SetTaskControlName (TaskControl_type* taskControl, char newName[])
{ 
	OKfree(taskControl->taskName);
	taskControl->taskName = StrDup(newName);
	
	// TEMPORARY: also set iterator name to match task controller name. This should be removed in the future!
	SetCurrentIterName(taskControl->currentIter, newName); 
}

int GetTaskControlState_GetLock (TaskControl_type* taskControl, TCStates* tcStatePtr, BOOL* lockObtained, int lineNumDebug, char fileNameDebug[], char** errorMsg)
{
INIT_ERR

	TCStates*	tcStateTSVPtr 	= NULL;
	
	*lockObtained = FALSE;
	CmtErrChk( CmtGetTSVPtr(taskControl->stateTSV, &tcStateTSVPtr) );
	*lockObtained = TRUE;
	
	//-------------------------------------------------------- 
	// debug
	taskControl->stateTSVLineNumDebug = lineNumDebug;
	strcpy(taskControl->stateTSVFileName, fileNameDebug);
	//--------------------------------------------------------
	
	*tcStatePtr = *tcStateTSVPtr;
	
CmtError:
	
Cmt_ERR
	
Error:
	
RETURN_ERR
}

int GetTaskControlState_ReleaseLock (TaskControl_type* taskControl, BOOL* lockObtained, char** errorMsg)
{
INIT_ERR
	
	*lockObtained = TRUE;
	CmtErrChk( CmtReleaseTSVPtr(taskControl->stateTSV) );
	*lockObtained = FALSE;
	
	//-------------------------------------------------------- 
	// debug
	taskControl->stateTSVLineNumDebug = 0;
	strcpy(taskControl->stateTSVFileName, "");
	//--------------------------------------------------------

CmtError:
	
Cmt_ERR

Error:
	
RETURN_ERR
}

void SetTaskControlIterations (TaskControl_type* taskControl, size_t repeat)
{
	Iterator_type* 	currentIter = GetTaskControlIterator(taskControl);
	
	taskControl->repeat = repeat;
	
	// TEMPORARY: task controller should not use repeat anymore but instead it should store this value in the iterator over which it iterates
	SetTotalIterations(currentIter, repeat);
	
}

size_t GetTaskControlIterations	(TaskControl_type* taskControl)
{
	return taskControl->repeat;
}

void SetTaskControlIterationTimeout	(TaskControl_type* taskControl, unsigned int timeout)
{
	taskControl->iterTimeout = timeout;
}

unsigned int GetTaskControlIterationTimeout (TaskControl_type* taskControl)
{
	return taskControl->iterTimeout;
}

void SetTaskControlExecutionMode (TaskControl_type* taskControl, TCExecutionModes executionMode)
{
	taskControl->executionMode = executionMode;
}

TCExecutionModes GetTaskControlExecutionMode (TaskControl_type* taskControl)
{
	return taskControl->executionMode; 	
}

void SetTaskControlIterator (TaskControl_type* taskControl, Iterator_type* currentIter)
{
	taskControl->currentIter = currentIter;
}

Iterator_type* GetTaskControlIterator (TaskControl_type* taskControl)
{
	return taskControl->currentIter; 	
}

void SetTaskControlMode	(TaskControl_type* taskControl, TaskMode_type mode)
{
	taskControl->mode = mode;	
}
 
TaskMode_type GetTaskControlMode (TaskControl_type* taskControl)
{
	return taskControl->mode;
}

void SetTaskControlIterationsWait (TaskControl_type* taskControl, double waitBetweenIterations)
{
	taskControl->waitBetweenIterations = waitBetweenIterations;
}

double GetTaskControlIterationsWait	(TaskControl_type* taskControl)
{
	return taskControl->waitBetweenIterations; 
}

void SetTaskControlModuleData (TaskControl_type* taskControl, void* moduleData)
{
	taskControl->moduleData = moduleData;	
}

void* GetTaskControlModuleData (TaskControl_type* taskControl)
{
	return taskControl->moduleData; 
}

TaskControl_type* GetTaskControlParent (TaskControl_type* taskControl)
{
	return taskControl->parentTC;
}

TaskControl_type* GetTaskControlRootParent (TaskControl_type* taskControl)
{
	TaskControl_type*	rootTC = taskControl;
	
	while (rootTC->parentTC)
		rootTC = rootTC->parentTC;
	
	return rootTC;
}

ListType GetTaskControlChildTCs (TaskControl_type* taskControl)
{
	size_t		nChildTCs 	= ListNumItems(taskControl->childTCs);
	ListType 	ChildTCs 	= ListCreate(sizeof(TaskControl_type*));
	if (!ChildTCs) return 0;
	
	ChildTCInfo_type*	childTCPtr = NULL;
	for (size_t i = 1; i <= nChildTCs; i++) {
		childTCPtr = ListGetPtrToItem (taskControl->childTCs, i);
		ListInsertItem(ChildTCs, &childTCPtr->childTC, END_OF_LIST);
	}
	
	return ChildTCs;
}

void SetTaskControlUITCFlag	(TaskControl_type* taskControl, BOOL UITCFlag)
{
	taskControl->UITCFlag = UITCFlag; 
}

BOOL GetTaskControlUITCFlag	(TaskControl_type* taskControl)
{
	return taskControl->UITCFlag; 
}

BOOL GetTaskControlAbortFlag (TaskControl_type* taskControl)
{
	return taskControl->abortFlag;
}

BOOL GetTaskControlIterationStopFlag (TaskControl_type* taskControl)
{
	return taskControl->stopIterationsFlag;
}

int IsTaskControllerInUse_GetLock (TaskControl_type* taskControl, BOOL* inUsePtr, TCStates* tcState, BOOL* lockObtained, int lineNumDebug, char fileNameDebug[], char** errorMsg)
{
INIT_ERR
	
	TCStates*	tcStateTSVPtr 	= NULL;
	
	if (!taskControl->stateTSV) {
		*lockObtained 	= FALSE;
		*inUsePtr		= FALSE;
		return 0;
	}
	
	*lockObtained = FALSE;
	CmtErrChk( CmtGetTSVPtr(taskControl->stateTSV, &tcStateTSVPtr) );
	*lockObtained = TRUE;
	
	//-------------------------------------------------------- 
	// debug
	taskControl->stateTSVLineNumDebug = lineNumDebug;
	strcpy(taskControl->stateTSVFileName, fileNameDebug);
	//--------------------------------------------------------
	
	*inUsePtr = (*tcStateTSVPtr == TC_State_Idle || *tcStateTSVPtr == TC_State_Running || *tcStateTSVPtr == TC_State_IterationFunctionActive || *tcStateTSVPtr == TC_State_Stopping);
	if (tcState) *tcState = *tcStateTSVPtr;
	
CmtError:
	
Cmt_ERR

Error:
	
RETURN_ERR
}

int IsTaskControllerInUse_ReleaseLock (TaskControl_type* taskControl, BOOL* lockObtained, char** errorMsg)
{
INIT_ERR	
	
	if (!taskControl->stateTSV) {
		*lockObtained = TRUE;
		return 0;
	}
	
	*lockObtained = TRUE;
	CmtErrChk( CmtReleaseTSVPtr(taskControl->stateTSV) );
	*lockObtained = FALSE;
	
	//-------------------------------------------------------- 
	// debug
	taskControl->stateTSVLineNumDebug = 0;
	strcpy(taskControl->stateTSVFileName, "");
	//--------------------------------------------------------

CmtError:
	
Cmt_ERR

Error:
	
RETURN_ERR
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller data queue and data exchange functions
//------------------------------------------------------------------------------------------------------------------------------------------------------
int	AddSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr, char** errorMsg)
{
#define		AddSinkVChan_Err_TaskControllerIsInUse		-1
#define		AddSinkVChan_Err_SinkAlreadyAssigned		-2
	
INIT_ERR
	
	VChanCallbackData_type*		currentVChanTSQData		= NULL;
	VChanCallbackData_type*		newVChanTSQData			= NULL;
	size_t 						nItems 					= ListNumItems(taskControl->dataQs);
	BOOL						tcIsInUse				= FALSE;
	BOOL						tcIsInUseLockObtained 	= FALSE;
	CmtTSQHandle				tsqID 					= GetSinkVChanTSQHndl(sinkVChan);
	char*						tcName					= NULL;
	char*						VChanName				= NULL;
	char*						msgBuff					= NULL;
	
	
	// Check if Sink VChan is already assigned to the Task Controller
	for (size_t i = 1; i <= nItems; i++) {
		currentVChanTSQData = *(VChanCallbackData_type**)ListGetPtrToItem(taskControl->dataQs, i);
		if (currentVChanTSQData->sinkVChan == sinkVChan) {
			nullChk( msgBuff  = StrDup("Sink VChan ") );
			nullChk( VChanName = GetVChanName((VChan_type*)sinkVChan) );
			nullChk( AppendString(&msgBuff, VChanName, -1) );
			nullChk( AppendString(&msgBuff, " has been already assigned to ", -1) );
			nullChk( tcName = GetTaskControlName(taskControl) );
			nullChk( AppendString(&msgBuff, tcName, -1) );
			SET_ERR(AddSinkVChan_Err_SinkAlreadyAssigned, msgBuff);
		}
	}
	
	// Check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) ); 
	
	if (tcIsInUse) {
		
		nullChk( msgBuff = StrDup("Sink VChan ") );
		nullChk( VChanName = GetVChanName((VChan_type*)sinkVChan) );
		nullChk( AppendString(&msgBuff, VChanName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be assigned to ", -1) );    
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " while it is in use", -1) );
		SET_ERR(AddSinkVChan_Err_TaskControllerIsInUse, msgBuff);
	}
	
	nullChk( newVChanTSQData = init_VChanCallbackData_type(taskControl, sinkVChan, DataReceivedFptr, 0) );
	// Add Task Controller TSQ callback. Process queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtErrChk( CmtInstallTSQCallback(tsqID, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskDataItemsInQueue, newVChanTSQData, taskControl->eventQThreadID, &newVChanTSQData->itemsInQueueCBID) );
	
	nullChk( ListInsertItem(taskControl->dataQs, &newVChanTSQData, END_OF_LIST) );
	newVChanTSQData = NULL; // added to the list

	// release lock
	errChk( IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, &errorInfo.errMsg) );
	
	return 0; // success

	
CmtError:

Cmt_ERR

Error:
	
	// cleanup
	discard_VChanCallbackData_type(&newVChanTSQData);
	OKfree(tcName);
	OKfree(VChanName);
	OKfree(msgBuff);

	// try to release lock
	if (tcIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int	AddSourceVChan (TaskControl_type* taskControl, SourceVChan_type* sourceVChan, char** errorMsg)
{
#define		AddSourceVChan_Err_TaskControllerIsInUse	-1
#define		AddSourceVChan_Err_SourceAlreadyAssigned	-2

INIT_ERR
	
	SourceVChan_type*	srcVChan				= NULL;
	size_t 				nItems					= ListNumItems(taskControl->sourceVChans);
	BOOL				tcIsInUse				= FALSE;
	BOOL				tcIsInUseLockObtained 	= FALSE;
	char*				tcName					= NULL;
	char*				VChanName				= NULL;
	char*				msgBuff					= NULL;
	
	// check if Source VChan is already assigned to the Task Controller
	for (size_t i = 1; i <= nItems; i++) {
		srcVChan = *(SourceVChan_type**)ListGetPtrToItem(taskControl->sourceVChans, i);
		if (srcVChan == sourceVChan) {
			nullChk( msgBuff = StrDup("Source VChan ") );
			nullChk( VChanName = GetVChanName((VChan_type*)sourceVChan) );
			nullChk( AppendString(&msgBuff, VChanName, -1) );
			nullChk( AppendString(&msgBuff, " has been already assigned to ", -1) );
			nullChk( tcName = GetTaskControlName(taskControl) );
			nullChk( AppendString(&msgBuff, tcName, -1) );
			SET_ERR(AddSourceVChan_Err_SourceAlreadyAssigned, msgBuff);
		}
	}
	
	// check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Source VChan ") );
		nullChk( VChanName = GetVChanName((VChan_type*)sourceVChan) );
		nullChk( AppendString(&msgBuff, VChanName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be assigned to ", -1) );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " while it is in use", -1) );
		SET_ERR(AddSourceVChan_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// insert source VChan
	nullChk( ListInsertItem(taskControl->sourceVChans, &sourceVChan, END_OF_LIST) );
	
Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(VChanName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int RemoveSourceVChan (TaskControl_type* taskControl, SourceVChan_type* sourceVChan, char** errorMsg)
{
#define		RemoveSourceVChan_Err_TaskControllerIsInUse		-1 
#define		RemoveSourceVChan_Err_SourceVChanNotFound		-2

INIT_ERR
	
	SourceVChan_type*	srcVChanItem			= NULL;
	size_t 				nItems					= ListNumItems(taskControl->sourceVChans);
	BOOL				tcIsInUse				= FALSE;
	BOOL				tcIsInUseLockObtained 	= FALSE;
	char*				tcName					= NULL;
	char*				VChanName				= NULL;
	size_t				srcVChanIdx				= 0;
	char*				msgBuff					= NULL;
	
	
	// Check if Task Controller is currently executing
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Source VChan ") );
		nullChk( VChanName = GetVChanName((VChan_type*)sourceVChan) );
		nullChk( AppendString(&msgBuff, VChanName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be removed from ", -1) );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " while it is in use", -1) );
		SET_ERR(RemoveSourceVChan_Err_TaskControllerIsInUse, msgBuff);
	}
	
	for (size_t i = 1; i <= nItems; i++) {
		srcVChanItem = *(SourceVChan_type**)ListGetPtrToItem(taskControl->sourceVChans, i);
		if (srcVChanItem == sourceVChan) {
			srcVChanIdx = i;
			break;
		}
	}
	
	if (srcVChanIdx)
		ListRemoveItem(taskControl->sourceVChans, 0, srcVChanIdx);
	else {
		nullChk( msgBuff = StrDup("Source VChan ") );
		nullChk( VChanName = GetVChanName((VChan_type*)sourceVChan) );
		nullChk( AppendString(&msgBuff, VChanName, -1) );
		nullChk( AppendString(&msgBuff, " has not been assigned to ", -1) );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " and cannot be removed", -1) );
		SET_ERR(RemoveSourceVChan_Err_SourceVChanNotFound, msgBuff);
	}
	
Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(VChanName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int RemoveAllSourceVChan (TaskControl_type* taskControl, char** errorMsg)
{
#define		RemoveAllSourceVChan_Err_TaskControllerIsInUse	-1
	
INIT_ERR
	
	BOOL		tcIsInUse				= FALSE;
	BOOL		tcIsInUseLockObtained 	= FALSE;
	char*		tcName					= NULL;
	char*		msgBuff					= NULL;
	
	// Check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Source VChannels cannot be removed from ") );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " while it is in use", -1) );
		SET_ERR(RemoveAllSourceVChan_Err_TaskControllerIsInUse, msgBuff);
	}
	
	ListClear(taskControl->sourceVChans);
	
Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int	RemoveSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan, char** errorMsg)
{
#define		RemoveSinkVChan_Err_TaskControllerIsInUse		-1 
#define		RemoveSinkVChan_Err_SinkVChanNotFound			-2

INIT_ERR
	
	VChanCallbackData_type**	VChanTSQDataPtr			= NULL;
	size_t						nDataQs					= ListNumItems(taskControl->dataQs);
	BOOL						tcIsInUse				= FALSE;
	BOOL						tcIsInUseLockObtained 	= FALSE;
	size_t						foundVChanIdx			= 0;
	SinkVChan_type*				foundSinkVChan			= NULL;
	CmtTSQCallbackID			tsqCBID					= 0;
	char*						tcName					= NULL; 
	char*						VChanName				= NULL;
	char*						msgBuff					= NULL;
	
	
	// Check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Sink VChan ") );
		nullChk( VChanName = GetVChanName((VChan_type*)sinkVChan) );
		nullChk( AppendString(&msgBuff, VChanName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be removed from ", -1) );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " while it is in use", -1) );
		SET_ERR(RemoveSinkVChan_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// Remove Sink VChan from Task Controller
	for (size_t i = 1; i <= nDataQs; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if ((*VChanTSQDataPtr)->sinkVChan == sinkVChan) {
			foundVChanIdx 	= i;
			foundSinkVChan  = (*VChanTSQDataPtr)->sinkVChan;
			tsqCBID			= (*VChanTSQDataPtr)->itemsInQueueCBID;
			break;
		}
	}
	
	if (foundVChanIdx) {
		// remove queue Task Controller callback
		errChk( CmtUninstallTSQCallback(GetSinkVChanTSQHndl(foundSinkVChan), tsqCBID) );
		// free memory for queue item
		discard_VChanCallbackData_type(VChanTSQDataPtr);
		// and remove from queue
		ListRemoveItem(taskControl->dataQs, 0, foundVChanIdx);
	} else {
		nullChk( msgBuff = StrDup("Sink VChan ") );
		nullChk( VChanName = GetVChanName((VChan_type*)sinkVChan) );
		nullChk( AppendString(&msgBuff, VChanName, -1) );
		nullChk( AppendString(&msgBuff, " has not been assigned to ", -1) );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " and cannot be removed", -1) );
		SET_ERR(RemoveSinkVChan_Err_SinkVChanNotFound, msgBuff);
	}
	
Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(VChanName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int RemoveAllSinkVChans (TaskControl_type* taskControl, char** errorMsg)
{
#define		RemoveAllSinkVChans_Err_TaskControllerIsInUse		-1
	
INIT_ERR
	
	VChanCallbackData_type** 	VChanTSQDataPtr 		= NULL;
	size_t 						nItems 					= ListNumItems(taskControl->dataQs);
	BOOL						tcIsInUse				= FALSE;
	BOOL						tcIsInUseLockObtained 	= FALSE;
	char*						tcName					= NULL;
	char*						msgBuff					= NULL;
	
	// Check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Sink VChannels cannot be removed from ") );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " while it is in use", -1) );
		SET_ERR(RemoveAllSinkVChans_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// Remove Sink VChans from the Task Controller
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		// remove queue Task Controller callback
		CmtErrChk( CmtUninstallTSQCallback(GetSinkVChanTSQHndl((*VChanTSQDataPtr)->sinkVChan), (*VChanTSQDataPtr)->itemsInQueueCBID) );
		// free memory for queue item
		discard_VChanCallbackData_type(VChanTSQDataPtr);
	}
	
	ListClear(taskControl->dataQs);

CmtError:
	
Cmt_ERR

Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int DisconnectAllSinkVChans (TaskControl_type* taskControl, char** errorMsg)
{
#define		DisconnectAllSinkVChans_Err_TaskControllerIsInUse	-1
	
INIT_ERR	
	
	BOOL						tcIsInUse				= FALSE;
	BOOL						tcIsInUseLockObtained 	= FALSE;
	VChanCallbackData_type** 	VChanTSQDataPtr			= NULL;
	size_t 						nItems 					= ListNumItems(taskControl->dataQs);
	char*						tcName					= NULL;
	char*						msgBuff					= NULL;
	
	
	// Check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Sink VChannels assigned to ") );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be disconnected from their Source VChannels while task controller is in use", -1) );
		SET_ERR(DisconnectAllSinkVChans_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// Disconnect Sink VChans from their Source VChans
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		// disconnect Sink from Source
		VChan_Disconnect((VChan_type*)(*VChanTSQDataPtr)->sinkVChan);
	}
	
Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

int DisconnectAllSourceVChans (TaskControl_type* taskControl, char** errorMsg)
{
#define		DisconnectAllSourceVChans_Err_TaskControllerIsInUse	-1

INIT_ERR

	BOOL					tcIsInUse				= FALSE;
	BOOL					tcIsInUseLockObtained 	= FALSE;
	SourceVChan_type*		srcVChan				= NULL;	
	size_t					nSourceVChans			= ListNumItems(taskControl->sourceVChans);
	char*					tcName					= NULL;
	char*					msgBuff					= NULL;
	
	// Check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Source VChannels assigned to ") );
		nullChk( tcName = GetTaskControlName(taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be disconnected from their Sinks while task controller is in use", -1) );
		SET_ERR(DisconnectAllSourceVChans_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// Disconnect Source VChans from their Sink VChans
	for (size_t i = 1; i <= nSourceVChans; i++) {
		srcVChan = *(SourceVChan_type**)ListGetPtrToItem(taskControl->sourceVChans, i);
		VChan_Disconnect((VChan_type*)srcVChan);
	}
	
Error:
	
	// cleanup
	OKfree(tcName);
	OKfree(msgBuff);
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(taskControl, &tcIsInUseLockObtained, NULL);
	
RETURN_ERR
}

static ChildTCEventInfo_type* init_ChildTCEventInfo_type (size_t childTCIdx, TCStates state)
{
	ChildTCEventInfo_type* childTCEventInfo = malloc(sizeof(ChildTCEventInfo_type));
	if (!childTCEventInfo) return NULL;
	
	childTCEventInfo->childTCIdx 		= childTCIdx;
	childTCEventInfo->newChildTCState	= state;
	
	return childTCEventInfo;
}

static void discard_ChildTCEventInfo_type (ChildTCEventInfo_type** eventDataPtr)
{
	if (!*eventDataPtr) return;
	
	OKfree(*eventDataPtr);
}

static int TaskTreeStateChange (TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskTreeStates state, char** errorMsg)
{
INIT_ERR

	if (!taskControl) return 0;
	
	// status change
	errChk( FunctionCall(taskControl, eventPacket, TASK_FCALL_TASK_TREE_STATUS, &state, &errorInfo.errMsg) );
	
	size_t				nChildTCs 	= ListNumItems(taskControl->childTCs);
	ChildTCInfo_type*	subTaskPtr	= NULL;
	
	for (size_t i = nChildTCs; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->childTCs, i);
		errChk( TaskTreeStateChange (subTaskPtr->childTC, eventPacket, state, &errorInfo.errMsg) );
	}
	
Error:
	
RETURN_ERR
}

int ClearAllSinkVChans	(TaskControl_type* taskControl, char** errorMsg)
{
INIT_ERR

	size_t						nVChans			= ListNumItems(taskControl->dataQs);
	VChanCallbackData_type**	VChanCBDataPtr;
	
	for (size_t i = nVChans; i; i--) {
		VChanCBDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		errChk( ReleaseAllDataPackets((*VChanCBDataPtr)->sinkVChan, &errorInfo.errMsg) );
	}
	
Error:
	
RETURN_ERR
}

static int ClearTaskTreeBranchVChans (TaskControl_type* taskControl, char** errorMsg)
{
INIT_ERR

	if (!taskControl) return 0;  // do nothing
	
	errChk( ClearAllSinkVChans(taskControl, errorMsg) );
	
	size_t				nChildTCs	 	= ListNumItems(taskControl->childTCs);
	ChildTCInfo_type*	subTaskPtr		= NULL;
	
	for (size_t i = nChildTCs; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->childTCs, i);
		errChk( ClearTaskTreeBranchVChans(subTaskPtr->childTC, errorMsg) );
	}
	
Error:
	
RETURN_ERR
}

static void SetChildTCsOutOfDate (TaskControl_type* taskControl)
{
	size_t				nChildTCs 		= ListNumItems(taskControl->childTCs);
	ChildTCInfo_type*   childTCInfo		= NULL;
	
	for (size_t i = 1; i <= nChildTCs; i++) {
		childTCInfo = ListGetPtrToItem(taskControl->childTCs, i);
		childTCInfo->isOutOfDate = TRUE;
	}
}

static int*	NumTag (int num)
{
	int* numTagPtr = malloc(sizeof(int));
	
	if (!numTagPtr) return NULL;
	
	*numTagPtr = num;
	
	return numTagPtr;
}

static VChanCallbackData_type*	init_VChanCallbackData_type	(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr, CmtTSQCallbackID itemsInQueueCBID)
{
	VChanCallbackData_type* VChanCB = malloc(sizeof(VChanCallbackData_type));
	if (!VChanCB) return NULL;
	
	VChanCB->sinkVChan 				= sinkVChan;
	VChanCB->taskControl  			= taskControl;
	VChanCB->DataReceivedFptr		= DataReceivedFptr;
	VChanCB->itemsInQueueCBID		= itemsInQueueCBID;
	
	return VChanCB;
}

static void	discard_VChanCallbackData_type (VChanCallbackData_type** VChanCBDataPtr)
{
	if (!*VChanCBDataPtr) return;
	
	OKfree(*VChanCBDataPtr);
}

void dispose_FCallReturn_EventInfo (void* eventData)
{
	FCallReturn_type* fCallReturnPtr = eventData;
	
	discard_FCallReturn_type(&fCallReturnPtr);
}

char* TaskControlStateToString (TCStates state)
{
	switch (state) {
		
		case TC_State_Unconfigured:
			
			return StrDup("Unconfigured");
			
		case TC_State_Configured:
			
			return StrDup("Configured");
			
		case TC_State_Initial:
			
			return StrDup("Initial");
			
		case TC_State_Idle:
			
			return StrDup("Idle");
			
		case TC_State_Running:
			
			return StrDup("Running");
			
		case TC_State_IterationFunctionActive:
			
			return StrDup("Iteration Function Active");
			
		case TC_State_Stopping:
			
			return StrDup("Stopping");
			
		case TC_State_Done:
			
			return StrDup("Done"); 
			
		case TC_State_Error:
			
			return StrDup("Error"); 
			
	}
	
}

static char* EventToString (TCEvents event)
{
	switch (event) {
		
		case TC_Event_Unconfigure: 
			
			return StrDup("Unconfigure");
		
		case TC_Event_Configure:
			
			return StrDup("Configure");
			
		case TC_Event_Start:
			
			return StrDup("Start");
			
		case TC_Event_IterateOnce:
			
			return StrDup("One Iteration");
			
		case TC_Event_Iterate:
			
			return StrDup("Iterate");
			
		case TC_Event_IterationDone:
			
			return StrDup("Iteration Done"); 
			
		case TC_Event_IterationTimeout:
			
			return StrDup("Iteration Timeout");
			
		case TC_Event_Reset:
			
			return StrDup("Reset");
			
		case TC_Event_Stop:
			
			return StrDup("Stop"); 
			
		case TC_Event_UpdateChildTCState:
			
			return StrDup("Child TC State Update");
			
		case TC_Event_DataReceived:
			
			return StrDup("Data received"); 
			
		case TC_Event_Custom:
			
			return StrDup("Custom event");
			
		case TASK_EVENT_SUBTASK_ADDED_TO_PARENT:
			
			return StrDup("Child TC added to parent TC"); 
			
		case TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT:
			
			return StrDup("Child TC removed from parent TC"); 
			
	}
	
}

static char* FCallToString (TCCallbacks fcall)
{
	switch (fcall) {
		
		case TC_Callback_Configure:
			
			return StrDup("FCall Configure");
			
		case TC_Callback_Unconfigure:
			
			return StrDup("FCall Unconfigure");
		
		case TC_Callback_Iterate:
			
			return StrDup("FCall Iterate");
			
		case TC_Callback_Start:
			
			return StrDup("FCall Start");
			
		case TC_Callback_Reset:
			
			return StrDup("FCall Reset");
			
		case TC_Callback_Done:
			
			return StrDup("FCall Done");
			
		case TC_Callback_Stopped:
			
			return StrDup("FCall Stopped");
			
		case TC_Callback_IterationStop:
			
			return StrDup("FCall Iteration Stop");
			
		case TASK_FCALL_TASK_TREE_STATUS:
			
			return StrDup("FCall Task Tree Status");
			
		case TASK_FCALL_SET_UITC_MODE:
			
			return StrDup("FCall Set UITC Mode");
			
		case TC_Callback_DataReceived:
			
			return StrDup("FCall Data Received");
			
		case TC_Callback_CustomEvent:
			
			return StrDup("FCall Module Event");
			
		case TC_Callback_Error:
			
			return StrDup("FCall Error");
			
	}
	
}

static void ExecutionLogEntry (TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskControllerActions action, void* info)
{
	if (!taskControl->loggingEnabled) return;
		
	ChildTCInfo_type*	childTCPtr		= NULL;
	char* 				output 			= StrDup("");
	char*				eventName		= NULL;
	char*				stateName		= NULL;
	char*				fCallName		= NULL;
	char    			buf[50]			= "";
	
	//---------------------------------------------------------------
	// Task Controller Name
	//---------------------------------------------------------------
	if (taskControl->taskName)
		AppendString(&output, taskControl->taskName, -1);
	else
		AppendString(&output, "No name", -1);
	
	//---------------------------------------------------------------  
	// Iteration index
	//---------------------------------------------------------------  
	AppendString(&output, ",  ", -1);
	Fmt(buf, "%s<(iteration: %d)", GetCurrentIterIndex(taskControl->currentIter));
	AppendString(&output, buf, -1);
	
	//---------------------------------------------------------------
	// Task Controller state
	//---------------------------------------------------------------
	AppendString(&output, ", (state: ", -1);
	// if this is a state change, then print old state as well
	if (action == STATE_CHANGE) {
		AppendString(&output, (stateName = TaskControlStateToString(taskControl->oldState)), -1);
		OKfree(stateName);
		AppendString(&output, "->", -1);
	}
	AppendString(&output, (stateName = TaskControlStateToString(taskControl->currentState)), -1);
	OKfree(stateName);
	AppendString(&output, ")", -1);
	
	//---------------------------------------------------------------
	// Event Name
	//---------------------------------------------------------------
	AppendString(&output, ",  (event: ", -1);
	
	AppendString(&output, (eventName = EventToString(eventPacket->event)), -1);
	OKfree(eventName);
	AppendString(&output, ")", -1);
	
	//---------------------------------------------------------------  
	// Action
	//---------------------------------------------------------------  
	AppendString(&output, ",  (action: ", -1);
	switch (action) {
		
		case STATE_CHANGE:		  
	
			AppendString(&output, "State change", -1);
			break;
		
		case FUNCTION_CALL:
			
			AppendString(&output, (fCallName = FCallToString(*(TCCallbacks*)info)), -1);
			OKfree(fCallName)
			break;
			
		case CHILD_TASK_STATE_UPDATE:
			
			AppendString(&output, "Child state change", -1); 
			break;
	}
	AppendString(&output, ")", -1);  
	
	//---------------------------------------------------------------
	// ChildTC Controller States
	//---------------------------------------------------------------
	AppendString(&output, ",  (child states: {", -1);
	size_t	nChildTCs	= ListNumItems(taskControl->childTCs);
	for (size_t i = 1; i <= nChildTCs; i++) {
		childTCPtr = ListGetPtrToItem(taskControl->childTCs, i);
		AppendString(&output, "(", -1);
		// Task Controller Name
		if (childTCPtr->childTC->taskName)
			AppendString(&output, childTCPtr->childTC->taskName, -1);
		else
			AppendString(&output, "No name", -1);
		
		AppendString(&output, ", ", -1);
		// if childTC state change event occurs, print also previous state of the childTC that changed
		if (action == CHILD_TASK_STATE_UPDATE)
			if (((ChildTCEventInfo_type*)eventPacket->eventData)->childTCIdx == i){
				AppendString(&output, (stateName = TaskControlStateToString(childTCPtr->previousChildTCState)), -1);
				OKfree(stateName);
				AppendString(&output, "->", -1);
			}
				
		AppendString(&output, (stateName = TaskControlStateToString(childTCPtr->childTCState)), -1);
		OKfree(stateName);
		AppendString(&output, ")", -1);
		if (i < nChildTCs)
			AppendString(&output, ", ", -1);
	}
	AppendString(&output, "})", -1);  	
	
	AppendString(&output, "\n", -1);
	
	SetCtrlVal(taskControl->logPanHandle, taskControl->logBoxControlID, output);
	OKfree(output);
}

void CVICALLBACK TaskEventItemsInQueue (CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData)
{
	// Don't use the value parameter! It will be 0 when the function is called manually
	//
	// assigned to main thread which the same as the thread in which the Task Controller
	// state machine was created.
	
	TaskControl_type* 	taskControl 		= callbackData;
	int					ExecutionStatus		= kCmtThreadFunctionComplete;
	
	// launch event handler in new thread if it is not running already
	if (taskControl->threadFunctionID)
		CmtGetThreadPoolFunctionAttribute(taskControl->threadPoolHndl, taskControl->threadFunctionID, 
									  ATTR_TP_FUNCTION_EXECUTION_STATUS, &ExecutionStatus);
	
	if (ExecutionStatus == kCmtThreadFunctionComplete) {
		if (taskControl->threadFunctionID) CmtReleaseThreadPoolFunctionID(taskControl->threadPoolHndl, taskControl->threadFunctionID);
		CmtScheduleThreadPoolFunctionAdv(taskControl->threadPoolHndl, ScheduleTaskEventHandler, taskControl, 
									 	 DEFAULT_THREAD_PRIORITY, NULL, 
									 	 (EVENT_TP_THREAD_FUNCTION_BEGIN | EVENT_TP_THREAD_FUNCTION_END), 
										 taskControl, RUN_IN_SCHEDULED_THREAD, &taskControl->threadFunctionID);	
	}
}

void CVICALLBACK TaskDataItemsInQueue (CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData)
{
INIT_ERR

	VChanCallbackData_type*		VChanTSQData		= callbackData;
	VChanCallbackData_type*		VChanTSQDataCopy	= NULL;
	
	nullChk( VChanTSQDataCopy = init_VChanCallbackData_type(VChanTSQData->taskControl, VChanTSQData->sinkVChan, VChanTSQData->DataReceivedFptr, VChanTSQData->itemsInQueueCBID) );
	
	// inform Task Controller that data was placed in an otherwise empty data queue
	errChk( TaskControlEvent(VChanTSQData->taskControl, TC_Event_DataReceived, (void**)&VChanTSQDataCopy, (DiscardFptr_type)discard_VChanCallbackData_type, &errorInfo.errMsg) );
	
	return;
	
Error:

	// flush queue
	discard_VChanCallbackData_type(&VChanTSQDataCopy);
	CmtFlushTSQ(GetSinkVChanTSQHndl(VChanTSQData->sinkVChan), TSQ_FLUSH_ALL, NULL);
	VChanTSQData->taskControl->errorMsg = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, "Out of memory.");
	VChanTSQData->taskControl->errorID	= errorInfo.error;
	EventPacket_type	eventPacket = {.event = TC_Event_DataReceived, .eventData = NULL, .discardEventDataFptr = NULL};
	ChangeState(VChanTSQData->taskControl, &eventPacket, TC_State_Error); 
}

int CVICALLBACK ScheduleTaskEventHandler (void* functionData)
{
	TaskControl_type* 	taskControl = functionData;
	
	// set thread sleep policy
	SetSleepPolicy(VAL_SLEEP_SOME);
	// dispatch event
	TaskEventHandler(taskControl);
	return 0;
}

void CVICALLBACK IterationFunctionThreadCallback (CmtThreadPoolHandle poolHandle, CmtThreadFunctionID functionID, 
													unsigned int event, int value, void *callbackData)
{
	// change the default thread sleep from more to some, thisgives optimal performance
	SetSleepPolicy(VAL_SLEEP_SOME);
}

int TaskControlEvent (TaskControl_type* RecipientTaskControl, TCEvents event, void** eventDataPtr, DiscardFptr_type discardEventDataFptr, char** errorMsg)
{
INIT_ERR	
	
	EventPacket_type 	eventPacket;
	ChildTCInfo_type*	subTask				= NULL;
	int 				lockObtainedFlag 	= FALSE;
	
	// init event packet
	eventPacket.event 					= event;
	if (eventDataPtr) {
		eventPacket.eventData			= *eventDataPtr;
		*eventDataPtr					= NULL;
	} else
		eventPacket.eventData			= NULL;
	
	eventPacket.discardEventDataFptr	= discardEventDataFptr;	

	// get event TSQ thread lock
	CmtErrChk( CmtGetLockEx(RecipientTaskControl->eventQThreadLock, 0, CMT_WAIT_FOREVER, &lockObtainedFlag) );
	
	// set out of date flag for the parent's record of this child task controller state
	if (RecipientTaskControl->parentTC) {
		subTask = ListGetPtrToItem(RecipientTaskControl->parentTC->childTCs, RecipientTaskControl->childTCIdx);
		subTask->isOutOfDate = TRUE;
	}
	
	CmtErrChk( CmtWriteTSQData(RecipientTaskControl->eventQ, &eventPacket, 1, 0, NULL) );
	
	// release event TSQ thread lock
	if (lockObtainedFlag)
		CmtReleaseLock(RecipientTaskControl->eventQThreadLock);
	
	return 0;
	
CmtError:
	
Cmt_ERR

Error:

	// cleanup event data
	if (eventPacket.eventData && eventPacket.discardEventDataFptr)
		(*eventPacket.discardEventDataFptr) (&eventPacket.eventData);
	else
		OKfree(eventPacket.eventData);
	
	// release event TSQ thread lock
	if (lockObtainedFlag)
		CmtReleaseLock(RecipientTaskControl->eventQThreadLock);
	
RETURN_ERR
}

int	TaskControlIterationDone (TaskControl_type* taskControl, int errorID, char errorInfoString[], BOOL doAnotherIteration, char** errorMsg)
{
INIT_ERR

	FCallReturn_type*	fCallReturn = NULL;
	
	if (errorID) {
		
		nullChk( fCallReturn = init_FCallReturn_type(errorID, "External Task Control Iteration", errorInfoString) );
		errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, (void**)&fCallReturn, (DiscardFptr_type)discard_FCallReturn_type, &errorInfo.errMsg) );
		
	} else {
		
		if (doAnotherIteration) 
			taskControl->repeat++;
		
		errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL, &errorInfo.errMsg) );
		
	}
	
	return 0;
	
Error:
	
	discard_FCallReturn_type(&fCallReturn);
	
RETURN_ERR
}

static void ChangeState (TaskControl_type* taskControl, EventPacket_type* eventPacket, TCStates newState)
{
	TaskControl_type*	parentTC	= NULL;
	
	// change state
	taskControl->oldState   	= taskControl->currentState;
	taskControl->currentState 	= newState;
	
	// add log entry if enabled
	ExecutionLogEntry(taskControl, eventPacket, STATE_CHANGE, NULL);
}

static BOOL AllChildTCsInState (TaskControl_type* taskControl, TCStates state)
{
	size_t				nChildTCs 		= ListNumItems(taskControl->childTCs);
	ChildTCInfo_type*   subTask			= NULL;
	BOOL				allTCsInState	= TRUE;
	
	for (size_t i = 1; i <= nChildTCs; i++) {
		subTask = ListGetPtrToItem(taskControl->childTCs, i);
		if ((subTask->isOutOfDate) || (subTask->childTCState != state)) {
			allTCsInState = FALSE;
			break;
		}
	}
	
	return allTCsInState;
}

void AbortTaskControlExecution (TaskControl_type* taskControl)
{
	ChildTCInfo_type* 	childTCPtr	= NULL;
	
	// set flag to abort iteration function if ongoing
	taskControl->abortFlag 		= TRUE;
	
	// abort ChildTCs recursively
	size_t	nChildTCs = ListNumItems(taskControl->childTCs);
	for (size_t i = 1; i <= nChildTCs; i++) {
		childTCPtr = ListGetPtrToItem(taskControl->childTCs, i);
		AbortTaskControlExecution(childTCPtr->childTC);
	}
	
}

int CVICALLBACK ScheduleIterateFunction (void* functionData)
{
	TaskControl_type* taskControl = functionData;
	
	(*taskControl->IterateFptr)(taskControl, taskControl->currentIter, &taskControl->abortFlag);
	
	return 0;
}


static int FunctionCall (TaskControl_type* taskControl, EventPacket_type* eventPacket, TCCallbacks fID, void* fCallData, char** errorMsg)
{
#define FunctionCall_Error_Invalid_fID 						-1
#define FunctionCall_Error_IterationFunctionNotExecuted		-3

INIT_ERR

	// call function
	
	BOOL	taskActive					= FALSE;
	BOOL	taskActiveLockObtained		= FALSE;
	
	// determine if task tree is active.
	// Note: this should be better done through message passing if task controllers are not local
	errChk( IsTaskControllerInUse_GetLock(GetTaskControlRootParent(taskControl), &taskActive, NULL, &taskActiveLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	// add log entry if enabled
	ExecutionLogEntry(taskControl, eventPacket, FUNCTION_CALL, &fID);
	
	switch (fID) {
		
		case TC_Callback_Configure:
			
			if (!taskControl->ConfigureFptr) break;		// function not provided
			errChk( (*taskControl->ConfigureFptr)(taskControl, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;  									
			
		case TC_Callback_Unconfigure:
			
			if (!taskControl->UnconfigureFptr) break;	// function not provided 
			errChk( (*taskControl->UnconfigureFptr)(taskControl, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;  									
		
		case TC_Callback_Iterate:
			
			if (!taskControl->IterateFptr) { 
				errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL, &errorInfo.errMsg) );
				break; 									// function not provided 
			}
			
			taskControl->stopIterationsFlag = FALSE;
			
			/* Taken out because DiscardAsyncTimer does not discard the timer and after creating 8 timers the function fails and the iteration function is not launched
			
			// call iteration function and set timeout if required to complete the iteration
			if (taskControl->iterTimeout) {  
				// set an iteration timeout async timer until which a TC_Event_IterationDone must be received 
				// if timeout elapses without receiving a TC_Event_IterationDone, a TC_Event_IterationTimeout is generated 
				if ( (taskControl->iterationTimerID = NewAsyncTimer(taskControl->iterTimeout, 1, 1, TaskControlIterTimeout, taskControl)) < 0)
					SET_ERR(taskControl->iterationTimerID, "Async timer error.");	
			}
			*/
			
			// launch provided iteration function pointer in a separate thread
			CmtErrChk( CmtScheduleThreadPoolFunctionAdv(taskControl->threadPoolHndl, ScheduleIterateFunction, taskControl, DEFAULT_THREAD_PRIORITY,
					IterationFunctionThreadCallback, EVENT_TP_THREAD_FUNCTION_BEGIN, taskControl, RUN_IN_SCHEDULED_THREAD, NULL) );
			break;
			
		case TC_Callback_Start:
			
			if (!taskControl->StartFptr) break;			// function not provided
			errChk( (*taskControl->StartFptr)(taskControl, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;
				
		case TC_Callback_Reset:
		
			if (!taskControl->ResetFptr) break;			// function not provided
			errChk( (*taskControl->ResetFptr)(taskControl, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;
			
		case TC_Callback_Done:
			
			if (!taskControl->DoneFptr) break;			// function not provided
			errChk( (*taskControl->DoneFptr)(taskControl, taskControl->currentIter, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;
			
		case TC_Callback_Stopped:
			
			if (!taskControl->StoppedFptr) break;		// function not provided		 
			errChk( (*taskControl->StoppedFptr)(taskControl, taskControl->currentIter, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;
			
		case TC_Callback_IterationStop:
			
			if (!taskControl->IterationStopFptr) break;	// function not provided		 
			errChk( (*taskControl->IterationStopFptr)(taskControl, taskControl->currentIter, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;
			
		case TASK_FCALL_TASK_TREE_STATUS:
			
			if (!taskControl->TaskTreeStateChangeFptr) break;	// function not provided
			errChk( (*taskControl->TaskTreeStateChangeFptr)(taskControl, *(TaskTreeStates*)fCallData, &errorInfo.errMsg) );
			break;
			
		case TASK_FCALL_SET_UITC_MODE:
			
			if (!taskControl->SetUITCModeFptr) break; 
			(*taskControl->SetUITCModeFptr)(taskControl, *(BOOL*)fCallData);
			break;
			
		case TC_Callback_DataReceived:
			
			// call data received callback if one was provided
			VChanCallbackData_type*	VChanCBData = fCallData;
			
			if (!VChanCBData->DataReceivedFptr) break;	// function not provided 
			errChk( (*VChanCBData->DataReceivedFptr)(taskControl, taskControl->currentState, taskActive, VChanCBData->sinkVChan, &taskControl->abortFlag, &errorInfo.errMsg) );
			break;
			
		case TC_Callback_CustomEvent:
			
			if (!taskControl->ModuleEventFptr) break; 	// function not provided
			errChk( (*taskControl->ModuleEventFptr)(taskControl, taskControl->currentState, taskActive, fCallData, &taskControl->abortFlag, &errorInfo.errMsg) )
			break;
			
		case TC_Callback_Error:
			
			// change Task Tree status if this is a Root Task Controller
			if (!taskControl->parentTC) {
				errChk( TaskTreeStateChange (taskControl, eventPacket, TaskTree_Finished, &errorInfo.errMsg) );
			}
			
			// call ErrorFptr
			if (taskControl->ErrorFptr)
				(*taskControl->ErrorFptr)(taskControl, taskControl->errorID, taskControl->errorMsg);
			break;
				
	}

	
CmtError:
	
Cmt_ERR

Error:
	
	// release lock
	if (taskActiveLockObtained)
		IsTaskControllerInUse_ReleaseLock(GetTaskControlRootParent(taskControl), &taskActiveLockObtained, NULL);
		
RETURN_ERR	
}

int	TaskControlEventToChildTCs (TaskControl_type* SenderTaskControl, TCEvents event, void** eventDataPtr, DiscardFptr_type discardEventDataFptr, char** errorMsg)
{
INIT_ERR

	EventPacket_type 		eventPacket;
	ChildTCInfo_type* 		subTask				= NULL;
	size_t					nChildTCs 			= ListNumItems(SenderTaskControl->childTCs);
	int*		 			lockObtainedFlags	= NULL;
	
	// init event packet
	eventPacket.event					= event;
	if (eventDataPtr) {
		eventPacket.eventData			= *eventDataPtr;
		*eventDataPtr					= NULL;
	} else
		eventPacket.eventData			= NULL;
	
	eventPacket.discardEventDataFptr	= discardEventDataFptr;
	
	
	// do nothing if there are no child TCs
	if (!nChildTCs) return 0;
	
	// init lock obtained flags
	nullChk( lockObtainedFlags = calloc(nChildTCs, sizeof(int)) );
	
	// get all childTC eventQ thread locks to place event in all queues before other events can be placed by other threads
	for (size_t i = 0; i < nChildTCs; i++) { 
		subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i+1);
		CmtErrChk( CmtGetLockEx(subTask->childTC->eventQThreadLock, 0, CMT_WAIT_FOREVER, &lockObtainedFlags[i]) );
	}
	
	// dispatch event to all childTCs
	for (size_t i = 1; i <= nChildTCs; i++) { 
		subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i);
		subTask->isOutOfDate = TRUE;
		CmtErrChk( CmtWriteTSQData(subTask->childTC->eventQ, &eventPacket, 1, 0, NULL) );
	}
	
CmtError:
	
Cmt_ERR

Error:
	
	// release all child TC eventQ thread locks
	if (lockObtainedFlags)
		for (size_t i = 0; i < nChildTCs; i++) { 
			subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i+1);
			if (lockObtainedFlags[i])
				CmtReleaseLock(subTask->childTC->eventQThreadLock);  
		}
	OKfree(lockObtainedFlags);
	
RETURN_ERR
}

int	AddChildTCToParent (TaskControl_type* parentTC, TaskControl_type* childTC, char** errorMsg)
{
#define		AddChildTCToParent_Err_TaskControllerIsInUse		-1
	
INIT_ERR

	BOOL						parentTCIsInUse					= FALSE;
	BOOL						childTCIsInUse					= FALSE;
	BOOL						parentTCIsInUseLockObtained 	= FALSE;
	BOOL						childTCIsInUseLockObtained 		= FALSE;
	TCStates					childTCState					= 0;
	BOOL						UITCFlag 						= FALSE;
	char*						childTCName						= NULL;
	char*						parentTCName					= NULL;
	char*						msgBuff							= NULL;
	
	
	// Check if parent or child Task Controllers are in use
	errChk( IsTaskControllerInUse_GetLock(parentTC, &parentTCIsInUse, NULL, &parentTCIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	errChk( IsTaskControllerInUse_GetLock(childTC, &childTCIsInUse, &childTCState, &childTCIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) ); 
	
	if (parentTCIsInUse || childTCIsInUse) {
		nullChk( msgBuff = StrDup("Cannot add child task controller ") );
		nullChk( childTCName = GetTaskControlName(childTC) );
		nullChk( AppendString(&msgBuff, childTCName, -1) );
		nullChk( AppendString(&msgBuff, " to parent task controller ", -1) );    
		nullChk( parentTCName = GetTaskControlName(parentTC) );
		nullChk( AppendString(&msgBuff, parentTCName, -1) );
		nullChk( AppendString(&msgBuff, " while either the parent or child task controllers are in use", -1) );
		SET_ERR(AddChildTCToParent_Err_TaskControllerIsInUse, msgBuff);
	}
	
	ChildTCInfo_type	childTCItem	= {.childTC = childTC, .childTCState = childTCState, .previousChildTCState = childTCState, .isOutOfDate = FALSE};
	
	// call UITC Active function to dim/undim UITC Task Control execution
	if (childTC->UITCFlag) {
		EventPacket_type eventPacket = {TASK_EVENT_SUBTASK_ADDED_TO_PARENT, NULL, NULL};
		errChk( FunctionCall(childTC, &eventPacket, TASK_FCALL_SET_UITC_MODE, &UITCFlag, &errorInfo.errMsg) );
	}
	// insert childTC
	nullChk( ListInsertItem(parentTC->childTCs, &childTCItem, END_OF_LIST) );

	// add parent pointer to childTC
	childTC->parentTC = parentTC;
	// update childTC index within parent list
	childTC->childTCIdx = ListNumItems(parentTC->childTCs); 
	
	// link iteration info
	errChk( IteratorAddIterator(GetTaskControlIterator(parentTC), GetTaskControlIterator(childTC), &errorInfo.errMsg) ); 
	
Error:
	
	// try to release parent and child task controller locks
	if (parentTCIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(parentTC, &parentTCIsInUseLockObtained, NULL);
	if (childTCIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(childTC, &childTCIsInUseLockObtained, NULL);
	
	OKfree(childTCName);
	OKfree(parentTCName);
	OKfree(msgBuff);
	
RETURN_ERR
}

int	RemoveChildTCFromParentTC (TaskControl_type* childTC, char** errorMsg)
{
#define		RemoveChildTCFromParentTC_Err_TaskControllerIsInUse				-1
#define		RemoveChildTCFromParentTC_Err_ParentTCDoesNotContainChildTC		-2
	
INIT_ERR
	
	BOOL					parentTCIsInUse					= FALSE;
	BOOL					childTCIsInUse					= FALSE;
	BOOL					parentTCIsInUseLockObtained 	= FALSE;
	BOOL					childTCIsInUseLockObtained 		= FALSE;
	TCStates				childTCState					= 0;
	TaskControl_type*		parentTC						= childTC->parentTC; 
	char*					childTCName						= NULL;
	char*					parentTCName					= NULL;
	char*					msgBuff							= NULL;
	
	// check if child TC has a parent TC and if not, then do nothing
	if (!parentTC) return 0;
	
	// check if parent or child Task Controllers are in use
	errChk( IsTaskControllerInUse_GetLock(parentTC, &parentTCIsInUse, NULL, &parentTCIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	errChk( IsTaskControllerInUse_GetLock(childTC, &childTCIsInUse, &childTCState, &childTCIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) ); 
	
	if (parentTCIsInUse || childTCIsInUse) {
		nullChk( msgBuff = StrDup("Cannot remove child task controller ") );
		nullChk( childTCName = GetTaskControlName(childTC) );
		nullChk( AppendString(&msgBuff, childTCName, -1) );
		nullChk( AppendString(&msgBuff, " from parent task controller ", -1) );    
		nullChk( parentTCName = GetTaskControlName(childTC->parentTC) );
		nullChk( AppendString(&msgBuff, parentTCName, -1) );
		nullChk( AppendString(&msgBuff, " while either the parent or child task controllers are in use", -1) );
		SET_ERR(AddChildTCToParent_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// remove child TC from parent TC
	ChildTCInfo_type* 	childTCPtr	= NULL;
	size_t				nChildTCs	= ListNumItems(childTC->parentTC->childTCs);
	size_t				childTCIdx	= 0;
	for (size_t i = 1; i <= nChildTCs; i++) {
		childTCPtr = ListGetPtrToItem(childTC->parentTC->childTCs, i);
		if (childTC == childTCPtr->childTC) {
			childTCIdx = i;
			break;
		}
		
	}
	
	if (childTCIdx) {
		ListRemoveItem(childTC->parentTC->childTCs, 0, childTCIdx);
		// remove child iterator from parent
		RemoveFromParentIterator(GetTaskControlIterator(childTC));
		// update childTC indices
		nChildTCs = ListNumItems(childTC->parentTC->childTCs);
		for (size_t i = 1; i <= nChildTCs; i++) {
			childTCPtr = ListGetPtrToItem(childTC->parentTC->childTCs, i);
			childTCPtr->childTC->childTCIdx = i;
		}
		childTC->childTCIdx = 0;
		childTC->parentTC = NULL;
			
		// call UITC Active function to dim/undim UITC Task Control execution
		BOOL	UITCFlag = TRUE;
		if (childTC->UITCFlag) {
			EventPacket_type eventPacket = {TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT, NULL, NULL};  
			errChk( FunctionCall(childTC, &eventPacket, TASK_FCALL_SET_UITC_MODE, &UITCFlag, &errorInfo.errMsg) );
		}
		
	} else {
		nullChk( msgBuff = StrDup("Parent task controller ") );
		nullChk( parentTCName = GetTaskControlName(childTC->parentTC) );
		nullChk( AppendString(&msgBuff, parentTCName, -1) );
		nullChk( AppendString(&msgBuff, " does not contain child task controller ", -1) );    
		nullChk( childTCName = GetTaskControlName(childTC) );
		nullChk( AppendString(&msgBuff, childTCName, -1) );
		SET_ERR(RemoveChildTCFromParentTC_Err_ParentTCDoesNotContainChildTC, msgBuff);
	}
	
Error:
	
	// try to release parent and child task controller locks
	if (parentTCIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(parentTC, &parentTCIsInUseLockObtained, NULL);
	if (childTCIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(childTC, &childTCIsInUseLockObtained, NULL);
	
	OKfree(childTCName);
	OKfree(parentTCName);
	OKfree(msgBuff);
	
RETURN_ERR
}

TaskControl_type* TaskControllerNameExists (ListType TCList, char TCName[], size_t* idx)
{
	TaskControl_type** 	TCPtr;
	char*				listTCName;
	size_t				nTCs			= ListNumItems(TCList);
	
	for (size_t i = 1; i <= nTCs; i++) {
		TCPtr = ListGetPtrToItem(TCList, i);
		listTCName = GetTaskControlName(*TCPtr);
		if (!strcmp(listTCName, TCName)) {
			if (idx) *idx = i;
			OKfree(listTCName);
			return *TCPtr;
		}
		OKfree(listTCName);
	}
	
	if (idx) *idx = 0;
	return NULL;
}

BOOL RemoveTaskControllerFromList (ListType TCList, TaskControl_type* taskController)
{
	size_t					nTCs 	= ListNumItems(TCList);
	TaskControl_type**   	TCPtr	= NULL;
	for (size_t i = nTCs; i ; i--) {
		TCPtr = ListGetPtrToItem(TCList, i);
		if (*TCPtr == taskController) {
			ListRemoveItem(TCList, 0, i);
			return TRUE;
		}
	}
	
	return FALSE;
}

BOOL RemoveTaskControllersFromList (ListType TCList, ListType TCsToBeRemoved)
{
	TaskControl_type**   	TCPtr		= NULL;  
	size_t					nTCs 		= ListNumItems(TCsToBeRemoved); 
	size_t					nRemoved	= 0;
	
	for (size_t i = nTCs; i; i--) {
		TCPtr = ListGetPtrToItem(TCsToBeRemoved, i);
		if (RemoveTaskControllerFromList(TCList, *TCPtr)) nRemoved++;
	}
	
	if (nRemoved == nTCs) return TRUE;
	else return FALSE;
}

char* GetUniqueTaskControllerName (ListType TCList, char baseTCName[])
{
	size_t 	n       		 	= 2;
	char* 	name				= NULL;   
	char   	countstr[500]		= "";
	
	name = StrDup(baseTCName);
	AppendString(&name, " 1", -1);
	while (TaskControllerNameExists (TCList, name, 0)) {
		OKfree(name);
		name = StrDup(baseTCName);
		Fmt(countstr, "%s<%d", n);
		AppendString(&name, " ", -1);
		AppendString(&name, countstr, -1);
		n++;
	}
	
	return name;
}

int	DisassembleTaskTreeBranch (TaskControl_type* taskControlNode, char** errorMsg)
{
INIT_ERR
	
	ChildTCInfo_type* 	childTCPtr	= NULL;  
	
	// disconnect from parent if any
	errChk( RemoveChildTCFromParentTC(taskControlNode, &errorInfo.errMsg) );
	
	// disconnect incoming Source VChans from the Sink VChans assigned to this Task Controller
	errChk( DisconnectAllSinkVChans(taskControlNode, &errorInfo.errMsg) );
	
	// disconnect Sink VChans connected to task controller assigned Source VChans
	errChk( DisconnectAllSourceVChans(taskControlNode, &errorInfo.errMsg) );
	
	// disconnect recursively child Task Controllers if any
	while(ListNumItems(taskControlNode->childTCs)) {
		childTCPtr = ListGetPtrToItem(taskControlNode->childTCs, 1);
		errChk( DisassembleTaskTreeBranch(childTCPtr->childTC, &errorInfo.errMsg) );
	}
	
Error:
	
RETURN_ERR
}

/// HIFN Called after a certain timeout if a TC_Event_IterationTimeout is not received
int CVICALLBACK TaskControlIterTimeout (int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskControl_type* 	taskControl = callbackData;
	
	if (event == EVENT_TIMER_TICK) {
		errChk( TaskControlEvent(taskControl, TC_Event_IterationTimeout, NULL, NULL, NULL) );
	}
	
Error:
	
	return 0;
}

static void AddIterationEventWithPriority (EventPacket_type* eventPackets, int* nEventPackets, int currentEventIdx) 
{
	EventPacket_type	iterEvent = {.event = TC_Event_Iterate, .eventData = NULL, .discardEventDataFptr = NULL};
	
	// shift events in the array with one element beyond the current EventIdx
	for (size_t i = *nEventPackets; i > currentEventIdx + 1; i--)
		eventPackets[i] = eventPackets[i-1];
	
	// insert iteration event
	eventPackets[currentEventIdx+1] = iterEvent;
	
	// increment number of event packets
	(*nEventPackets)++;
}

static void TaskEventHandler (TaskControl_type* taskControl)
{
#define TaskEventHandler_Error_InvalidEventInState			-1
#define TaskEventHandler_Error_ChildTCInErrorState			-2
#define TaskEventHandler_Error_IterateFCallTmeout			-3
#define	TaskEventHandler_Error_IterateExternThread			-4
#define TaskEventHandler_Error_DataPacketsNotCleared		-5

INIT_ERR
	
	EventPacket_type 		eventPackets[EVENT_BUFFER_SIZE];
	ChildTCInfo_type* 		childTCPtr			= NULL;
	ChildTCEventInfo_type*	childTCEventInfo 	= NULL;
	char*					eventStr			= NULL;
	char*					stateStr			= NULL;
	char* 					msgBuff 			= NULL;
	size_t					nChars				= 0;
	size_t					nItems				= 0;
	int						nEventItems			= 0;
	BOOL					stateLockObtained   = FALSE;
	TCStates*				tcStateTSVPtr 		= NULL; 
	
	// get all Task Controller events in the queue
	while ((nEventItems = CmtReadTSQData(taskControl->eventQ, eventPackets, EVENT_BUFFER_SIZE - 1, 0, 0)) > 0) {
		
		for (int i = 0; i < nEventItems; i++) {
		
			// reset abort flag
			taskControl->abortFlag = FALSE;
			
			// get task controller state
			stateLockObtained = FALSE;
			CmtErrChk( CmtGetTSVPtr(taskControl->stateTSV, &tcStateTSVPtr) );
			stateLockObtained = TRUE;
			taskControl->stateTSVLineNumDebug = __LINE__;
			strcpy(taskControl->stateTSVFileName, __FILE__);
			taskControl->currentState = *tcStateTSVPtr;
			
			switch (taskControl->currentState) {
		
				case TC_State_Unconfigured:
		
					switch (eventPackets[i].event) {
				
						case TC_Event_Configure:
					
							// configure this task
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
							
							// send TC_Event_Configure to all childTCs if there are any
							errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
							
							// If there are no childTCs, then reset device/module and make transition here to Initial state and inform parent TC
							if (!ListNumItems(taskControl->childTCs)) {
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter, 0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
						
							} else
								ChangeState(taskControl, &eventPackets[i], TC_State_Configured);
						
							break;
					
						case TC_Event_Stop:
					
							// ignore event
					
							break;
					
						case TC_Event_Unconfigure:
					
							// ignore event
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE; 
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state, then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
							
							break;
					
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR
					}
			
					break;
			
				case TC_State_Configured:
		
					switch (eventPackets[i].event) { 
				
						case TC_Event_Configure: 
					
							// configure this TC again
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
							
							// send TC_Event_Configure to all childTCs if there are any
							errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Stop:
					
							// ignore event
					
							break;
					
						case TC_Event_Unconfigure:
					
							// unconfigure
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Unconfigure, NULL, &errorInfo.errMsg) );
							
							ChangeState(taskControl, &eventPackets[i], TC_State_Unconfigured);
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE;
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state, then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
							
							// consider only transitions from other states to TC_State_Initial 
							if (!(childTCPtr->childTCState == TC_State_Initial && childTCPtr->previousChildTCState != TC_State_Initial))
								break; // stop here
					
							// reset task controller and set it to initial state if all child TCs are in their initial state
							if (AllChildTCsInState(taskControl, TC_State_Initial)) {
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
							}
					
							break;
					
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR
					}
			
					break;
			
				case TC_State_Initial:
		
					switch (eventPackets[i].event) {
				
						case TC_Event_Configure:
					
							// configure again this TC
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Unconfigure:
					
							// unconfigure
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Unconfigure, NULL, &errorInfo.errMsg) );
							
							ChangeState(taskControl, &eventPackets[i], TC_State_Unconfigured);
					
							break;
				
						case TC_Event_Reset:
					
							// ignore this command
							break;
				
						case TC_Event_Start:
						case TC_Event_IterateOnce: 
					
					
							if (!taskControl->parentTC) {
						
								// clear task tree recursively if this is the root task controller (i.e. it doesn't have a parent)
								errChk( ClearTaskTreeBranchVChans(taskControl, &errorInfo.errMsg) );
								
								// if this is the root task controller (i.e. it doesn't have a parent) then change Task Tree status
								errChk( TaskTreeStateChange(taskControl, &eventPackets[i], TaskTree_Started, &errorInfo.errMsg) );
							}
					
							//--------------------------------------------------------------------------------------------------------------- 
							// Call Start Task Controller function pointer to inform that task will start
							//--------------------------------------------------------------------------------------------------------------- 
					
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Start, NULL, &errorInfo.errMsg) );
							
							//---------------------------------------------------------------------------------------------------------------   
							// Set flag to iterate once or continue until done or stopped
							//---------------------------------------------------------------------------------------------------------------   
							if (eventPackets[i].event == TC_Event_IterateOnce) 
								taskControl->nIterationsFlag = 1;
							else
								taskControl->nIterationsFlag = -1;
					
							//---------------------------------------------------------------------------------------------------------------
							// Iterate Task Controller and switch to RUNNING state
							//---------------------------------------------------------------------------------------------------------------
					
							AddIterationEventWithPriority(eventPackets, &nEventItems, i);
							ChangeState(taskControl, &eventPackets[i], TC_State_Running);  
					
							break;
					
						case TC_Event_Stop:  
				
							// ignore event
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr = ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState = childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState = ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate = FALSE;
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state, then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
							
							// if childTC is unconfigured then switch to configured state
							if (childTCPtr->childTCState == TC_State_Unconfigured)
								ChangeState(taskControl, &eventPackets[i], TC_State_Configured);
					
							break;
					
			
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR
					}
			
					break;
			
				case TC_State_Idle:
	
					switch (eventPackets[i].event) {
				
						case TC_Event_Start:
						case TC_Event_IterateOnce: 
					
							if(!taskControl->parentTC) {
					
								// clear task tree recursively if this is the root task controller (i.e. it doesn't have a parent)
								errChk( ClearTaskTreeBranchVChans(taskControl, &errorInfo.errMsg) );
								
								// if this is the root task controller (i.e. it doesn't have a parent) then change Task Tree status
								errChk( TaskTreeStateChange(taskControl, &eventPackets[i], TaskTree_Started, &errorInfo.errMsg) );
							}
					
							//--------------------------------------------------------------------------------------------------------------- 
							// Call Start Task Controller function pointer to inform that task will start
							//--------------------------------------------------------------------------------------------------------------- 
					
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Start, NULL, &errorInfo.errMsg) );
							
							//---------------------------------------------------------------------------------------------------------------   
							// Set flag to iterate once or continue until done or stopped
							//---------------------------------------------------------------------------------------------------------------
					
							if (eventPackets[i].event == TC_Event_IterateOnce)
								taskControl->nIterationsFlag = 1;
							else
								taskControl->nIterationsFlag = -1;
					
							//---------------------------------------------------------------------------------------------------------------
							// Switch to RUNNING state and iterate Task Controller
							//---------------------------------------------------------------------------------------------------------------
					
							AddIterationEventWithPriority(eventPackets, &nEventItems, i);
							ChangeState(taskControl, &eventPackets[i], TC_State_Running);
							
							break;
					
						case TC_Event_Iterate:
					
							// ignore this command
							break;
					
						case TC_Event_Reset:
					
							// send RESET event to all childTCs
							errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Reset, NULL, NULL, &errorInfo.errMsg) );
							
							// change state to INITIAL if there are no childTCs and call reset function
							if (!ListNumItems(taskControl->childTCs)) {
						
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
						
							}
					
							break;
					
						case TC_Event_Stop:  
				
							// ignore event
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE;
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state, then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
							
							// if childTC is unconfigured then switch to configured state
							if (childTCPtr->childTCState == TC_State_Unconfigured) {
								ChangeState(taskControl, &eventPackets[i], TC_State_Configured);
								break;
							}
					
							// consider only transitions from other states to TC_State_Initial 
							if (!(childTCPtr->childTCState == TC_State_Initial && childTCPtr->previousChildTCState != TC_State_Initial))
								break; // stop here
					
							if (AllChildTCsInState(taskControl, TC_State_Initial)) {
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
							}
					
							break;
					
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR	
					
					}
			
					break;
			
				case TC_State_Running:
		 
					switch (eventPackets[i].event) {
				
					
						case TC_Event_Configure:
					
							if (!taskControl->repeat)
								// configure again this TC
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
							 else
								TC_WRONG_EVENT_STATE_ERROR
							
							break;
					
						case TC_Event_Iterate:
					
							//---------------------------------------------------------------------------------------------------------------
							// Check if an iteration is needed:
							// If current iteration index is smaller than the total number of requested repeats, except if repeat = 0 and
							// first iteration or if task is continuous.
							//---------------------------------------------------------------------------------------------------------------
					
							// The iterate event is self generated, when it occurs, all child TCs are in an initial or done state
							// For a Finite TC for both repeat == 0 and repeat == 1 the TC will undergo one iteration, however when 
							// repeat == 0, its iteration callback function will not be called.
							size_t curriterindex = GetCurrentIterIndex(taskControl->currentIter);
					
							if ( taskControl->mode == TASK_FINITE  &&  (curriterindex >= taskControl->repeat  ||  !taskControl->nIterationsFlag)  && curriterindex ) {			
								//---------------------------------------------------------------------------------------------------------------- 	 
								// Task Controller is finite switch to DONE
								//---------------------------------------------------------------------------------------------------------------- 	
										
								// call done function
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Done, NULL, &errorInfo.errMsg) );
								
								ChangeState(taskControl, &eventPackets[i], TC_State_Done);
						
								// change Task Tree status
								if(!taskControl->parentTC) { 
									errChk( TaskTreeStateChange(taskControl, &eventPackets[i], TaskTree_Finished, &errorInfo.errMsg) );
								}
									
								break; // stop here
							}
						
							//---------------------------------------------------------------------------------------------------------------
							// If not first iteration, then wait given number of seconds before iterating    
							//---------------------------------------------------------------------------------------------------------------
					
							if (GetCurrentIterIndex(taskControl->currentIter) && taskControl->nIterationsFlag < 0)
								SyncWait(Timer(), taskControl->waitBetweenIterations);
					
							//---------------------------------------------------------------------------------------------------------------
							// Iterate Task Controller
							//---------------------------------------------------------------------------------------------------------------
					
							switch (taskControl->executionMode) {
						
								case TC_Execute_BeforeChildTCs:
							
									//---------------------------------------------------------------------------------------------------------------
									// Call iteration function if needed	 
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Iterate, NULL, &errorInfo.errMsg) );
									else 
										errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL, &errorInfo.errMsg) );
									
									// switch state and wait for iteration to complete
									ChangeState(taskControl, &eventPackets[i], TC_State_IterationFunctionActive);
									
									break;
							
								case TC_Execute_AfterChildTCsComplete:
							
									//---------------------------------------------------------------------------------------------------------------
									// Start ChildTCs if there are any
									//---------------------------------------------------------------------------------------------------------------
									nItems = ListNumItems(taskControl->childTCs); 		
									if (nItems) {    
										// send START event to all childTCs
										errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Start, NULL, NULL, &errorInfo.errMsg) );
										break; // stop here
									}	
							
									//---------------------------------------------------------------------------------------------------------------
									// There are no ChildTCs, call iteration function if needed
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Iterate, NULL, &errorInfo.errMsg) );
									else
										errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL, &errorInfo.errMsg) );
									
									// switch state and wait for iteration to complete
									ChangeState(taskControl, &eventPackets[i], TC_State_IterationFunctionActive);  
									
									break;
									
								case TC_Execute_InParallelWithChildTCs:
										   
									//---------------------------------------------------------------------------------------------------------------
									// Start ChildTCs if there are any
									//---------------------------------------------------------------------------------------------------------------
							
									errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Start, NULL, NULL, &errorInfo.errMsg) );
									
									//---------------------------------------------------------------------------------------------------------------
									// Call iteration function if needed
									//---------------------------------------------------------------------------------------------------------------
							
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Iterate, NULL, &errorInfo.errMsg) );
									else 
										errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL, &errorInfo.errMsg) );
									
									// switch state and wait for iteration and ChildTCs if there are any to complete
									ChangeState(taskControl, &eventPackets[i], TC_State_IterationFunctionActive);  
									break;
							}
					
							break;
					
						case TC_Event_Stop:
						// Stops iterations and switches to IDLE or DONE states if there are no ChildTC Controllers or to STOPPING state and waits for ChildTCs to complete their iterations
					
							if (!ListNumItems(taskControl->childTCs)) {
						
								// if there are no ChildTC Controllers
								if (taskControl->mode == TASK_CONTINUOUS) {
									// switch to DONE state if continuous task controller
									errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Done, NULL, &errorInfo.errMsg) );
									
									ChangeState(taskControl, &eventPackets[i], TC_State_Done);
							
								} else 
									// switch to IDLE or DONE state if finite task controller
									if (GetCurrentIterIndex(taskControl->currentIter) < taskControl->repeat) {
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Stopped, NULL, &errorInfo.errMsg) );
										ChangeState(taskControl, &eventPackets[i], TC_State_Idle);
									} else {
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Done, NULL, &errorInfo.errMsg) );
										ChangeState(taskControl, &eventPackets[i], TC_State_Done);
									}
						
								// change Task Tree status
								if(!taskControl->parentTC) {
									errChk( TaskTreeStateChange(taskControl, &eventPackets[i], TaskTree_Finished, &errorInfo.errMsg) );
								}
							
							} else {
								// send TC_Event_Stop event to all childTCs
								errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Stop, NULL, NULL, &errorInfo.errMsg) );
								
								ChangeState(taskControl, &eventPackets[i], TC_State_Stopping);
							}
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE; 
					
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					
							// if childTC is in an error state then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
								
							//---------------------------------------------------------------------------------------------------------------- 
							// If ChildTCs are not yet complete, stay in RUNNING state and wait for their completion
							//---------------------------------------------------------------------------------------------------------------- 
					
							// consider only transitions from other states to TC_State_Done 
							if (!(childTCPtr->childTCState == TC_State_Done && childTCPtr->previousChildTCState != TC_State_Done))
								break; // stop here
					
							if (!AllChildTCsInState(taskControl, TC_State_Done))  
								break; // stop here
					
							// put all child TC's out of date to prevent race here
							//SetChildTCsOutOfDate(taskControl);
					
							//---------------------------------------------------------------------------------------------------------------- 
							// Decide on state transition
							//---------------------------------------------------------------------------------------------------------------- 
							switch (taskControl->executionMode) {
						
								case TC_Execute_BeforeChildTCs:			   // iteration function is not active
								case TC_Execute_InParallelWithChildTCs:	   // iteration function is not active
							
							
									//---------------------------------------------------------------------------------------------------------------
									// Increment iteration index
									//---------------------------------------------------------------------------------------------------------------
									SetCurrentIterIndex(taskControl->currentIter, GetCurrentIterIndex(taskControl->currentIter)+1);
									if (taskControl->nIterationsFlag > 0)
										taskControl->nIterationsFlag--;
							   
									//---------------------------------------------------------------------------------------------------------------- 
									// Ask for another iteration and check later if iteration is needed or possible
									//---------------------------------------------------------------------------------------------------------------- 
							
									AddIterationEventWithPriority(eventPackets, &nEventItems, i);
									
									// stay in TC_State_Running
									break;
							
								case TC_Execute_AfterChildTCsComplete:
						
									//---------------------------------------------------------------------------------------------------------------
									// ChildTCs are complete, call iteration function
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Iterate, NULL, &errorInfo.errMsg) );
									else 
										errChk( TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL, &errorInfo.errMsg) );
									
										
									ChangeState(taskControl, &eventPackets[i], TC_State_IterationFunctionActive); 	
									break;
							
							}
					
							break;
					
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
							
							TC_WRONG_EVENT_STATE_ERROR
							
					}
			
					break;
			
				case TC_State_IterationFunctionActive:
			
					switch (eventPackets[i].event) {
				
						case TC_Event_IterationDone:
					
							//---------------------------------------------------------------------------------------------------------------   
							// Remove timeout timer if one was set
							//---------------------------------------------------------------------------------------------------------------   
							if (taskControl->iterationTimerID > 0) {
								DiscardAsyncTimer(taskControl->iterationTimerID);
								taskControl->iterationTimerID = 0;
							}
					
							//---------------------------------------------------------------------------------------------------------------   
							// Check if error occured during iteration 
							// (which may be also because it was aborted and this caused an error for the TC)
							//---------------------------------------------------------------------------------------------------------------   
							if (eventPackets[i].eventData) {
								FCallReturn_type* fCallReturn = eventPackets[i].eventData;
						
								if (fCallReturn->retVal < 0)
									SET_ERR(TaskEventHandler_Error_IterateExternThread, fCallReturn->errorInfo); 
							}
					
							//---------------------------------------------------------------------------------------------------------------
							// If iteration was aborted or stopped, switch to IDLE or DONE state if there are no ChildTC Controllers,
							// otherwise switch to STOPPING state and wait for ChildTCs to complete their iterations
							//---------------------------------------------------------------------------------------------------------------
					
							if (taskControl->abortFlag || taskControl->stopIterationsFlag) {  // added || taskControl->stopIterationsFlag
						
								if (!ListNumItems(taskControl->childTCs)) {
							
									// if there are no ChildTC Controllers
									if (taskControl->mode == TASK_CONTINUOUS) {
										// switch to DONE state if continuous task controller
										errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Done, NULL, &errorInfo.errMsg) );
										
										ChangeState(taskControl, &eventPackets[i], TC_State_Done);
								
									} else 
										// switch to IDLE or DONE state if finite task controller
							
										if ( GetCurrentIterIndex(taskControl->currentIter) < taskControl->repeat) {
											errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Stopped, NULL, &errorInfo.errMsg) );
											
											ChangeState(taskControl, &eventPackets[i], TC_State_Idle);
									
										} else { 
									
											errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Done, NULL, &errorInfo.errMsg) )
											
											ChangeState(taskControl, &eventPackets[i], TC_State_Done);
										}
							
									// change Task Tree status
									if(!taskControl->parentTC) {
										errChk( TaskTreeStateChange(taskControl, &eventPackets[i], TaskTree_Finished, &errorInfo.errMsg) );
									}
										
								} else {
							
									// send TC_Event_Stop event to all childTCs
									errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Stop, NULL, NULL, &errorInfo.errMsg) );
									
									ChangeState(taskControl, &eventPackets[i], TC_State_Stopping);
								}
						
								break; // stop here
							}
					
							//---------------------------------------------------------------------------------------------------------------   
							// Decide how to switch state
							//--------------------------------------------------------------------------------------------------------------- 
					
							switch (taskControl->executionMode) {
						
								case TC_Execute_BeforeChildTCs:
							
									//----------------------------------------------------------------------------------------------------------------
									// Start ChildTCs if there are any
									//----------------------------------------------------------------------------------------------------------------
							
									if (ListNumItems(taskControl->childTCs)) {
										// send START event to all childTCs
										errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Start, NULL, NULL, &errorInfo.errMsg) );
										
										// switch to RUNNING state and wait for ChildTCs to complete
										ChangeState(taskControl, &eventPackets[i], TC_State_Running);
										break; // stop here
									} 
							
									// If there are no ChildTCs, fall through the case and ask for another iteration unless only one iteration was requested
							
								case TC_Execute_AfterChildTCsComplete:
							
									//---------------------------------------------------------------------------------------------------------------
									// Increment iteration index
									//---------------------------------------------------------------------------------------------------------------
									SetCurrentIterIndex(taskControl->currentIter,GetCurrentIterIndex(taskControl->currentIter)+1);
									if (taskControl->nIterationsFlag > 0)
										taskControl->nIterationsFlag--;
							
									//---------------------------------------------------------------------------------------------------------------- 
									// If not stopped, ask for another iteration and check later if iteration is needed or possible
									//---------------------------------------------------------------------------------------------------------------- 
							
									// ask for another iteration
									AddIterationEventWithPriority(eventPackets, &nEventItems, i);
									ChangeState(taskControl, &eventPackets[i], TC_State_Running); 
									break;
									
								case TC_Execute_InParallelWithChildTCs:
							
									//---------------------------------------------------------------------------------------------------------------- 
									// If ChildTCs are not yet complete, switch to RUNNING state and wait for their completion
									//---------------------------------------------------------------------------------------------------------------- 
							
									if (!AllChildTCsInState(taskControl, TC_State_Done)) {
										ChangeState(taskControl, &eventPackets[i], TC_State_Running);
										break;  // stop here
									}
							
								    //---------------------------------------------------------------------------------------------------------------
									// Increment iteration index
									//---------------------------------------------------------------------------------------------------------------
									SetCurrentIterIndex(taskControl->currentIter,GetCurrentIterIndex(taskControl->currentIter)+1);
							
									if (taskControl->nIterationsFlag > 0)
										taskControl->nIterationsFlag--;
							
									//---------------------------------------------------------------------------------------------------------------- 
									// Ask for another iteration and check later if iteration is needed or possible
									//---------------------------------------------------------------------------------------------------------------- 
							
									// ask for another iteration
									AddIterationEventWithPriority(eventPackets, &nEventItems, i);
									ChangeState(taskControl, &eventPackets[i], TC_State_Running); 
									break;
							}
							break;
					
						case TC_Event_IterationTimeout:
					
							// generate timeout error
							SET_ERR(TaskEventHandler_Error_IterateFCallTmeout, "Iteration function call timeout.");
							
						case TC_Event_Stop:
				
							taskControl->stopIterationsFlag = TRUE; // interation function in progress completes normally and no further iterations are requested.
					
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_IterationStop, NULL, &errorInfo.errMsg) );
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE;  
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
							
							break;
					
						case TC_Event_DataReceived:
							
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR
					}
					break;
			
				case TC_State_Stopping:
			
					switch (eventPackets[i].event) {
				
						case TC_Event_Stop:
				
							// ignore this command
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE;  
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state, then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
							
							BOOL	IdleOrDoneFlag	= TRUE;		// assume all childTCs are either IDLE or DONE
							nItems = ListNumItems(taskControl->childTCs);
					
							for (size_t j = 1; j <= nItems; j++) {
								childTCPtr = ListGetPtrToItem(taskControl->childTCs, j);
								if (!((childTCPtr->childTCState == TC_State_Idle) || (childTCPtr->childTCState == TC_State_Done)) || childTCPtr->isOutOfDate) {
									IdleOrDoneFlag = FALSE;
									break;
								}
							}
					
							// if all ChildTCs are IDLE or DONE, switch to IDLE state
							if (IdleOrDoneFlag) {
						
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Stopped, NULL, &errorInfo.errMsg) );
								
								// change Task Tree status
								if(!taskControl->parentTC) {
									errChk( TaskTreeStateChange(taskControl, &eventPackets[i], TaskTree_Finished, &errorInfo.errMsg) );
								}
								
								ChangeState(taskControl, &eventPackets[i], TC_State_Idle);
								break;
							}
							
							// reset task controller and set it to initial state if all child TCs are in their initial state
							if (AllChildTCsInState(taskControl, TC_State_Initial)) {
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial); 
								break;
							}
							break;
					
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR
					}
			
					break;
			
				case TC_State_Done:
				// This state can be reached only if all ChildTC Controllers are in a DONE state
			
					switch (eventPackets[i].event) {
				
						case TC_Event_Configure:
					
							// configure again this TC
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Unconfigure:
					
							// call unconfigure function and switch to unconfigured state
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Unconfigure, NULL, &errorInfo.errMsg) );
							
							ChangeState(taskControl, &eventPackets[i], TC_State_Unconfigured);
							break;
					
						case TC_Event_Start:
					
							// reset device/module
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
							
						    // reset iterations
							SetCurrentIterIndex(taskControl->currentIter, 0);
					
							// switch to INITIAL state
							ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
					
							// send START event to self
							errChk( TaskControlEvent(taskControl, TC_Event_Start, NULL, NULL, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Reset:
					
							// send RESET event to all childTCs
							errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Reset, NULL, NULL, &errorInfo.errMsg) );
							
							// change state to INITIAL if there are no childTCs and call reset function
							if (!ListNumItems(taskControl->childTCs)) {
						
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
						
							}
					
							break;
					
						case TC_Event_Stop:  
					
							// ignore event
					
							break;
					
						case TC_Event_UpdateChildTCState:
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState; 
							childTCPtr->isOutOfDate 			= FALSE;  
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if childTC is in an error state, then switch to error state
							if (childTCPtr->childTCState == TC_State_Error)
								SET_ERR(TaskEventHandler_Error_ChildTCInErrorState, childTCPtr->childTC->errorMsg);
								
							// if childTC is unconfigured then switch to configured state
							if (childTCPtr->childTCState == TC_State_Unconfigured) {
								ChangeState(taskControl, &eventPackets[i], TC_State_Configured);
								break;
							}
					
					
							// consider only transitions from other states to TC_State_Initial 
							if (!(childTCPtr->childTCState == TC_State_Initial && childTCPtr->previousChildTCState != TC_State_Initial))
								break; // stop here
					
							// check states of all childTCs and transition to INITIAL state if all childTCs are in INITIAL state
							if (AllChildTCsInState(taskControl, TC_State_Initial)) {
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
							}
					
							break;
					
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							TC_WRONG_EVENT_STATE_ERROR	
					}
					break;
			
				case TC_State_Error:
					switch (eventPackets[i].event) {
				
						case TC_Event_Configure:
							// Reconfigures Task Controller
					
							ChangeState(taskControl, &eventPackets[i], TC_State_Unconfigured);
					
							errChk( TaskControlEvent(taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
							
							// clear error
							OKfree(taskControl->errorMsg);
							taskControl->errorID = 0;
							break;
					
						case TC_Event_Reset:
					
							// send RESET event to all childTCs
							errChk( TaskControlEventToChildTCs(taskControl, TC_Event_Reset, NULL, NULL, &errorInfo.errMsg) );
							
							// reset and change state to Initial if there are no childTCs
							if (!ListNumItems(taskControl->childTCs)) {
						
								// configure this task controller again
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
								
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
						
							}
					
							// clear error
							OKfree(taskControl->errorMsg);
							taskControl->errorID = 0;
							break;
					
						case TC_Event_Unconfigure:
					
							// unconfigure
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Unconfigure, NULL, &errorInfo.errMsg) );
							
							ChangeState(taskControl, &eventPackets[i], TC_State_Unconfigured);
							break;
					
						case TC_Event_UpdateChildTCState: 
					
							// update childTC state
							childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventPackets[i].eventData)->childTCIdx);
							childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
							childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventPackets[i].eventData)->newChildTCState;
							childTCPtr->isOutOfDate 			= FALSE;  
							ExecutionLogEntry(taskControl, &eventPackets[i], CHILD_TASK_STATE_UPDATE, NULL);
					
							// if the error has been cleared and all child TCs are in an initial state, then switch to initial state as well
							if (!taskControl->errorID && AllChildTCsInState(taskControl, TC_State_Initial)) {
								
								// configure this task controller again
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Configure, NULL, &errorInfo.errMsg) );
								
								// reset device/module
								errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Reset, NULL, &errorInfo.errMsg) );
								
								// reset iterations
								SetCurrentIterIndex(taskControl->currentIter,0);
						
								ChangeState(taskControl, &eventPackets[i], TC_State_Initial);
							}
					
							break;
					
			
						case TC_Event_DataReceived:
					
							// call data received event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_DataReceived, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						case TC_Event_Custom:
					
							// call custom module event function
							errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_CustomEvent, eventPackets[i].eventData, &errorInfo.errMsg) );
							break;
					
						default:
					
							// ignore and stay in error state
							break;
					
					}
			
					break;
			}
	
			// if there is a parent task controller, update it on the state of this child task controller
			if (taskControl->parentTC) {
				
				childTCEventInfo = NULL;
				nullChk( childTCEventInfo = init_ChildTCEventInfo_type(taskControl->childTCIdx, taskControl->currentState) );
				
				errChk( TaskControlEvent(taskControl->parentTC, TC_Event_UpdateChildTCState, (void**)&childTCEventInfo, (DiscardFptr_type)discard_ChildTCEventInfo_type, &errorInfo.errMsg) );
				
			}

			// assign new state and release state lock
			*tcStateTSVPtr = taskControl->currentState;
			stateLockObtained = TRUE;
			CmtErrChk( CmtReleaseTSVPtr(taskControl->stateTSV) );
			tcStateTSVPtr = NULL;
			stateLockObtained = FALSE;
			taskControl->stateTSVLineNumDebug = 0;
			strcpy(taskControl->stateTSVFileName, "");
			
			// free memory for extra eventData if any
			if (eventPackets[i].eventData && eventPackets[i].discardEventDataFptr)
				(*eventPackets[i].discardEventDataFptr)(&eventPackets[i].eventData);
			else
				OKfree(eventPackets[i].eventData);
			
			// process another event if there is any
			continue;
			
		CmtError:

		Cmt_ERR

		Error:
			//-------------------------------------------
			// cleanup
			//-------------------------------------------
	
			discard_ChildTCEventInfo_type(&childTCEventInfo);
		
			// remove iteration timeout timer
			if (taskControl->iterationTimerID > 0) {
				DiscardAsyncTimer(taskControl->iterationTimerID);
				taskControl->iterationTimerID = 0;
			}
		
			// change state
			ChangeState(taskControl, &eventPackets[i], TC_State_Error);
			
			// assign new state and try to release lock if obtained
			if (stateLockObtained) {
				*tcStateTSVPtr = taskControl->currentState;
				CmtReleaseTSVPtr(taskControl->stateTSV);
				tcStateTSVPtr = NULL;
				stateLockObtained = FALSE;
				taskControl->stateTSVLineNumDebug = 0;
				strcpy(taskControl->stateTSVFileName, "");
			}
			
			// store error
			OKfree(msgBuff);
			taskControl->errorMsg = StrDup("Task Controller \"");
			AppendString(&taskControl->errorMsg, taskControl->taskName, -1);
			AppendString(&taskControl->errorMsg, "\" error: ", -1);
			msgBuff = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg);
			AppendString(&taskControl->errorMsg, msgBuff, -1);
			OKfree(msgBuff);
			taskControl->errorID = errorInfo.error;
			
			// abort execution in the entire Task Tree
			if (GetTaskControlParent(taskControl))
				AbortTaskControlExecution(GetTaskControlParent(taskControl));
			else
				AbortTaskControlExecution(taskControl);
			
			// call error function to handle error
			errChk( FunctionCall(taskControl, &eventPackets[i], TC_Callback_Error, NULL, &errorInfo.errMsg) );
		
			// if there is a parent task controller, update it on the state of this child task controller
			if (taskControl->parentTC) {
				childTCEventInfo = init_ChildTCEventInfo_type(taskControl->childTCIdx, taskControl->currentState);
				TaskControlEvent(taskControl->parentTC, TC_Event_UpdateChildTCState, (void**)&childTCEventInfo, (DiscardFptr_type)discard_ChildTCEventInfo_type, &errorInfo.errMsg);
			}

			// free memory for extra eventData if any
			if (eventPackets[i].eventData && eventPackets[i].discardEventDataFptr)
				(*eventPackets[i].discardEventDataFptr)(&eventPackets[i].eventData);
			else
				OKfree(eventPackets[i].eventData);
		
		} 
		
	} // check if there are more events

}
