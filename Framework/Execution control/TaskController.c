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
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

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
	TCStates 						state;								// Task Controller state.
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
	char*							errorInfo;							// When switching to an error state, additional error info is written here.
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

// Use this function to change the state of a Task Controller
static void 								ChangeState 							(TaskControl_type* taskControl, EventPacket_type* eventPacket, TCStates newState);
// If True, the state of all child TCs is up to date and the same as the given state.
static BOOL 								AllChildTCsInState 						(TaskControl_type* taskControl, TCStates state); 
// Use this function to carry out a Task Controller action using provided function pointers
static int									FunctionCall 							(TaskControl_type* taskControl, EventPacket_type* eventPacket, TCCallbacks fID, void* fCallData, char** errorInfo); 

// Formats a Task Controller log entry based on the action taken
static void									ExecutionLogEntry						(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskControllerActions action, void* info);

static char*								StateToString							(TCStates state);
static char*								EventToString							(TCEvents event);
static char*								FCallToString							(TCCallbacks fcall);

// ChildTC eventData functions
static ChildTCEventInfo_type* 				init_ChildTCEventInfo_type 				(size_t childTCIdx, TCStates state);
static void									discard_ChildTCEventInfo_type 			(ChildTCEventInfo_type** eventDataPtr);

// VChan and Task Control binding
static VChanCallbackData_type*				init_VChanCallbackData_type				(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr);
static void									discard_VChanCallbackData_type			(VChanCallbackData_type** VChanCBDataPtr);

// Informs recursively Task Controllers about the Task Tree status when it changes (active/inactive).
static int									TaskTreeStateChange		 				(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskTreeStates state, char** errorInfo);

// Clears recursively all data packets from the Sink VChans of all Task Controllers in a Task Tree Branch starting with the given Task Controller.
static int									ClearTaskTreeBranchVChans				(TaskControl_type* taskControl, char** errorInfo);

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
										TaskTreeStateChangeFptr_type	TaskTreeStateChangeFptr,
										SetUITCModeFptr_type			SetUITCModeFptr,
										ModuleEventFptr_type			ModuleEventFptr,
										ErrorFptr_type					ErrorFptr)
{
	int		error = 0;
	TaskControl_type* tc = malloc (sizeof(TaskControl_type));
	if (!tc) return NULL;
	
	// init
	tc -> eventQ							= 0;
	tc -> eventQThreadLock					= 0;
	tc -> dataQs							= 0;
	tc -> sourceVChans						= 0;
	tc -> childTCs							= 0;
	
	if (CmtNewTSQ(TC_NEventQueueItems, sizeof(EventPacket_type), 0, &tc->eventQ) < 0) goto Error;
	if (CmtNewLock(NULL, 0, &tc->eventQThreadLock) < 0) goto Error;
	
	nullChk( tc -> dataQs					= ListCreate(sizeof(VChanCallbackData_type*)) );
	nullChk( tc -> sourceVChans				= ListCreate(sizeof(SourceVChan_type*)) );
	
	tc -> eventQThreadID					= CmtGetCurrentThreadID ();
	tc -> threadFunctionID					= 0;
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(tc->eventQ, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskEventItemsInQueue, tc, tc->eventQThreadID, NULL); 
	
	nullChk( tc -> childTCs					= ListCreate(sizeof(ChildTCInfo_type)) );
	
	tc -> taskName 							= StrDup(taskControllerName);
	tc -> moduleData						= moduleData;
	tc -> threadPoolHndl					= tcThreadPoolHndl;
	tc -> childTCIdx						= 0;
	tc -> state								= TC_State_Unconfigured;
	tc -> oldState							= TC_State_Unconfigured;
	tc -> repeat							= 1;
	tc -> iterTimeout						= 10;								
	tc -> executionMode						= TC_Execute_BeforeChildTCs;
	tc -> mode								= TASK_FINITE;
	tc -> currentIter					    = init_Iterator_type(taskControllerName);   
	tc -> parentTC							= NULL;
	tc -> logPanHandle						= 0;
	tc -> logBoxControlID					= 0;
	tc -> loggingEnabled					= FALSE;
	tc -> errorInfo							= NULL;
	tc -> errorID							= 0;
	tc -> waitBetweenIterations				= 0;
	tc -> abortFlag							= FALSE;
	tc -> stopIterationsFlag				= FALSE;
	tc -> nIterationsFlag					= -1;
	tc -> iterationTimerID					= 0;
	tc -> UITCFlag							= FALSE;
	
	// task controller function pointers
	tc -> ConfigureFptr 					= ConfigureFptr;
	tc -> UnconfigureFptr					= UnconfigureFptr;
	tc -> IterateFptr						= IterateFptr;
	tc -> StartFptr							= StartFptr;
	tc -> ResetFptr							= ResetFptr;
	tc -> DoneFptr							= DoneFptr;
	tc -> StoppedFptr						= StoppedFptr;
	tc -> TaskTreeStateChangeFptr			= TaskTreeStateChangeFptr;
	tc -> SetUITCModeFptr					= SetUITCModeFptr;
	tc -> ModuleEventFptr					= ModuleEventFptr;
	tc -> ErrorFptr							= ErrorFptr;
	      
	
	return tc;
	
	Error:
	
	if (tc->eventQ) 				CmtDiscardTSQ(tc->eventQ);
	if (tc->eventQThreadLock)		CmtDiscardLock(tc->eventQThreadLock);
	if (tc->dataQs)	 				ListDispose(tc->dataQs);
	if (tc->sourceVChans)			ListDispose(tc->sourceVChans);
	if (tc->childTCs)    			ListDispose(tc->childTCs);
	
	OKfree(tc);
	
	return NULL;
}

/// HIFN Discards recursively a Task controller.
void discard_TaskControl_type(TaskControl_type** taskControllerPtr)
{
	TaskControl_type*	taskController = *taskControllerPtr;	
	
	if (!taskController) return;
	
	//----------------------------------------------------------------------------
	// Disassemble task tree branch recusively starting with the given parent node
	//----------------------------------------------------------------------------
	DisassembleTaskTreeBranch(taskController);
	
	//----------------------------------------------------------------------------
	// Discard this Task Controller
	//----------------------------------------------------------------------------
	
	// name
	OKfree(taskController->taskName);
	
	// event queue
	CmtDiscardTSQ(taskController->eventQ);
	
	// source VChan list
	ListDispose(taskController->sourceVChans);
	
	// release thread
	if (taskController->threadFunctionID) CmtReleaseThreadPoolFunctionID(taskController->threadPoolHndl, taskController->threadFunctionID);   
	
	// event queue thread lock
	CmtDiscardLock(taskController->eventQThreadLock); 
	
	// incoming data queues (does not free the queue itself!)
	RemoveAllSinkVChans(taskController);
	ListDispose(taskController->dataQs);
	
	// error message storage 
	OKfree(taskController->errorInfo);
	
	//iteration info
	discard_Iterator_type (&taskController->currentIter); 

	// child Task Controllers list
	ListDispose(taskController->childTCs);
	
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

char* GetTaskControlStateName (TaskControl_type* taskControl)
{
	return StateToString(taskControl->state);
}

void SetTaskControlName (TaskControl_type* taskControl, char newName[])
{ 
	OKfree(taskControl->taskName);
	taskControl->taskName = StrDup(newName);
	
	// TEMPORARY: also set iterator name to match task controller name. This should be removed in the future!
	SetCurrentIterName(taskControl->currentIter, newName); 
}

TCStates GetTaskControlState (TaskControl_type* taskControl)
{
	return taskControl->state;
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


void SetTaskControlIterationTimeout	(TaskControl_type* taskControl, int timeout)
{
	taskControl->iterTimeout = timeout;
}

int	GetTaskControlIterationTimeout (TaskControl_type* taskControl)
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

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller data queue and data exchange functions
//------------------------------------------------------------------------------------------------------------------------------------------------------
int	AddSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr)
{
#define 	AddSinkVChan_Err_OutOfMemory				-1
#define		AddSinkVChan_Err_TaskControllerIsActive		-2
#define		AddSinkVChan_Err_SinkAlreadyAssigned		-3
	
				// Task Controller must not be executing when a Sink VChan is added to it
	// check if Sink VChan is already assigned to the Task Controller
	VChanCallbackData_type**	VChanTSQDataPtr		= NULL;
	size_t 						nItems 				= ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if ((*VChanTSQDataPtr)->sinkVChan == sinkVChan) return AddSinkVChan_Err_SinkAlreadyAssigned;
	}
	
	// check if Task Controller is currently executing
	if (!(taskControl->state == TC_State_Unconfigured || taskControl->state == TC_State_Configured || 
		  taskControl->state == TC_State_Initial || taskControl->state == TC_State_Done || taskControl->state == TC_State_Error))
		return AddSinkVChan_Err_TaskControllerIsActive;
	
	CmtTSQHandle				tsqID 				= GetSinkVChanTSQHndl(sinkVChan);
	VChanCallbackData_type*		VChanTSQData		= init_VChanCallbackData_type(taskControl, sinkVChan, DataReceivedFptr);
	
	if (!VChanTSQData) return AddSinkVChan_Err_OutOfMemory;
	if (!ListInsertItem(taskControl->dataQs, &VChanTSQData, END_OF_LIST)) 
		return AddSinkVChan_Err_OutOfMemory;
	
	// add callback
	// process queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(tsqID, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskDataItemsInQueue, VChanTSQData, taskControl -> eventQThreadID, &VChanTSQData->itemsInQueueCBID);
	
	return 0;
}

int	AddSourceVChan (TaskControl_type* taskControl, SourceVChan_type* sourceVChan)
{
#define 	AddSourceVChan_Err_OutOfMemory				-1
#define		AddSourceVChan_Err_TaskControllerIsActive	-2
#define		AddSourceVChan_Err_SourceAlreadyAssigned	-3

	// check if Source VChan is already assigned to the Task Controller
	SourceVChan_type**	VChanPtr		= NULL;
	size_t 				nItems			= ListNumItems(taskControl->sourceVChans);
	for (size_t i = 1; i <= nItems; i++) {
		VChanPtr = ListGetPtrToItem(taskControl->sourceVChans, i);
		if (*VChanPtr == sourceVChan) return AddSourceVChan_Err_SourceAlreadyAssigned;
	}
	
	// check if Task Controller is currently executing
	if (!(taskControl->state == TC_State_Unconfigured || taskControl->state == TC_State_Configured || 
		  taskControl->state == TC_State_Initial || taskControl->state == TC_State_Done || taskControl->state == TC_State_Error))
		return AddSourceVChan_Err_TaskControllerIsActive;
	
	// insert source VChan
	if (!ListInsertItem(taskControl->sourceVChans, &sourceVChan, END_OF_LIST)) 
		return AddSinkVChan_Err_OutOfMemory;
	
	return 0;
}

int RemoveSourceVChan (TaskControl_type* taskControl, SourceVChan_type* sourceVChan)
{
#define		RemoveSourceVChan_Err_SinkNotFound				-1
#define		RemoveSourceVChan_Err_TaskControllerIsActive	-2
	
	// check if Task Controller is active
	if (!(taskControl->state == TC_State_Unconfigured || taskControl->state == TC_State_Configured || 
		  taskControl->state == TC_State_Initial || taskControl->state == TC_State_Done || taskControl->state == TC_State_Error))
		return RemoveSourceVChan_Err_TaskControllerIsActive;
	
	SourceVChan_type**	VChanPtr		= NULL;
	size_t 				nItems			= ListNumItems(taskControl->sourceVChans);
	for (size_t i = 1; i <= nItems; i++) {
		VChanPtr = ListGetPtrToItem(taskControl->sourceVChans, i);
		if (*VChanPtr == sourceVChan) {
			ListRemoveItem(taskControl->sourceVChans, 0, i);
			return 0;
		}
	}
	
	return RemoveSourceVChan_Err_SinkNotFound;
}

int RemoveAllSourceVChan (TaskControl_type* taskControl)
{
#define		RemoveAllSourceVChan_Err_TaskControllerIsActive	-1
	
	// check if Task Controller is active
	if (!(taskControl->state == TC_State_Unconfigured || taskControl->state == TC_State_Configured || 
		  taskControl->state == TC_State_Initial || taskControl->state == TC_State_Done || taskControl->state == TC_State_Error))
		return RemoveAllSourceVChan_Err_TaskControllerIsActive;
	
	ListClear(taskControl->sourceVChans);
	return 0;
}

int	RemoveSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan)
{
#define		RemoveSinkVChan_Err_SinkNotFound				-1
#define		RemoveSinkVChan_Err_TaskControllerIsActive		-2
	
	VChanCallbackData_type**	VChanTSQDataPtr		= NULL;
	size_t						nDataQs				= ListNumItems(taskControl->dataQs);
	
	
	// check if Task Controller is active
	if (!(taskControl->state == TC_State_Unconfigured || taskControl->state == TC_State_Configured || 
		  taskControl->state == TC_State_Initial || taskControl->state == TC_State_Done || taskControl->state == TC_State_Error))
		return RemoveSinkVChan_Err_TaskControllerIsActive;
	
	for (size_t i = 1; i <= nDataQs; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if ((*VChanTSQDataPtr)->sinkVChan == sinkVChan) {
			// remove queue Task Controller callback
			CmtUninstallTSQCallback(GetSinkVChanTSQHndl((*VChanTSQDataPtr)->sinkVChan), (*VChanTSQDataPtr)->itemsInQueueCBID);
			// free memory for queue item
			discard_VChanCallbackData_type(VChanTSQDataPtr);
			// and remove from queue
			ListRemoveItem(taskControl->dataQs, 0, i);
			return 0; 	// Sink VChan found and removed
		}
	}
	
	return RemoveSinkVChan_Err_SinkNotFound;			// Sink VChan not found
}


int RemoveAllSinkVChans (TaskControl_type* taskControl)
{
#define		RemoveAllSinkVChans_Err_TaskControllerIsActive		-1
	
	// check if Task Controller is active
	if (!(taskControl->state == TC_State_Unconfigured || taskControl->state == TC_State_Configured || 
		  taskControl->state == TC_State_Initial || taskControl->state == TC_State_Done || taskControl->state == TC_State_Error))
		return RemoveAllSinkVChans_Err_TaskControllerIsActive;
	
	VChanCallbackData_type** 	VChanTSQDataPtr = NULL;
	size_t 						nItems 			= ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		// remove queue Task Controller callback
		CmtUninstallTSQCallback(GetSinkVChanTSQHndl((*VChanTSQDataPtr)->sinkVChan), (*VChanTSQDataPtr)->itemsInQueueCBID);
		// free memory for queue item
		discard_VChanCallbackData_type(VChanTSQDataPtr);
	}
	
	ListClear(taskControl->dataQs);
	
	return 0;
}

void DisconnectAllSinkVChans (TaskControl_type* taskControl)
{
	VChanCallbackData_type** 	VChanTSQDataPtr;
	size_t 						nItems = ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		// disconnect Sink from Source
		VChan_Disconnect((VChan_type*)(*VChanTSQDataPtr)->sinkVChan);
	}
}

static ChildTCEventInfo_type* init_ChildTCEventInfo_type (size_t childTCIdx, TCStates state)
{
	ChildTCEventInfo_type* a = malloc(sizeof(ChildTCEventInfo_type));
	if (!a) return NULL;
	
	a -> childTCIdx 		= childTCIdx;
	a -> newChildTCState	= state;
	
	return a;
}

static void discard_ChildTCEventInfo_type (ChildTCEventInfo_type** eventDataPtr)
{
	if (!*eventDataPtr) return;
	
	OKfree(*eventDataPtr);
}

static int TaskTreeStateChange (TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskTreeStates state, char** errorInfo)
{
	int		error = 0;
	if (!taskControl) return 0;
	
	// status change
	if ( (error = FunctionCall(taskControl, eventPacket, TASK_FCALL_TASK_TREE_STATUS, &state, errorInfo)) < 0) return error; 
	
	size_t			nChildTCs = ListNumItems(taskControl->childTCs);
	ChildTCInfo_type*	subTaskPtr;
	
	for (size_t i = nChildTCs; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->childTCs, i);
		if ( (error = TaskTreeStateChange (subTaskPtr->childTC, eventPacket, state, errorInfo)) < 0) return error;
	}
	
	return 0;
}

int ClearAllSinkVChans	(TaskControl_type* taskControl, char** errorInfo)
{
	size_t						nVChans			= ListNumItems(taskControl->dataQs);
	VChanCallbackData_type**	VChanCBDataPtr;
	int							error			= 0;
	
	for (size_t i = nVChans; i; i--) {
		VChanCBDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		errChk( ReleaseAllDataPackets((*VChanCBDataPtr)->sinkVChan, errorInfo) );
	}
	
	return 0;
	
Error:
	
	return error;
}

static int ClearTaskTreeBranchVChans (TaskControl_type* taskControl, char** errorInfo)
{
	int		error 	= 0;
	
	if (!taskControl) return 0;  // do nothing
	
	errChk( ClearAllSinkVChans(taskControl, errorInfo) );
	
	size_t			nChildTCs	 = ListNumItems(taskControl->childTCs);
	ChildTCInfo_type*	subTaskPtr;
	
	for (size_t i = nChildTCs; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->childTCs, i);
		errChk( ClearTaskTreeBranchVChans(subTaskPtr->childTC, errorInfo) );
	}
	
	return 0;
	
Error:
	
	return error;
}

static VChanCallbackData_type*	init_VChanCallbackData_type	(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr)
{
	VChanCallbackData_type* VChanCB = malloc(sizeof(VChanCallbackData_type));
	if (!VChanCB) return NULL;
	
	VChanCB->sinkVChan 				= sinkVChan;
	VChanCB->taskControl  			= taskControl;
	VChanCB->DataReceivedFptr		= DataReceivedFptr;
	VChanCB->itemsInQueueCBID		= 0;
	
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

static char* StateToString (TCStates state)
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
		
	ChildTCInfo_type*	childTCPtr;
	char* 			output 			= StrDup("");
	char*			eventName		= NULL;
	char*			stateName		= NULL;
	char*			fCallName		= NULL;
	char    		buf[50];
	
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
		AppendString(&output, (stateName = StateToString(taskControl->oldState)), -1);
		OKfree(stateName);
		AppendString(&output, "->", -1);
	}
	AppendString(&output, (stateName = StateToString(taskControl->state)), -1);
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
				AppendString(&output, (stateName = StateToString(childTCPtr->previousChildTCState)), -1);
				OKfree(stateName);
				AppendString(&output, "->", -1);
			}
				
		AppendString(&output, (stateName = StateToString(childTCPtr->childTCState)), -1);
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
#define TaskDataItemsInQueue_Err_OutOfMemory	-1
	
	VChanCallbackData_type*		VChanTSQData		= callbackData;
	VChanCallbackData_type**	VChanTSQDataPtr		= malloc(sizeof(SinkVChan_type*));
	EventPacket_type			eventPacket			= {TC_Event_DataReceived, NULL, NULL};
	if (!VChanTSQDataPtr) {
		// flush queue
		CmtFlushTSQ(GetSinkVChanTSQHndl(VChanTSQData->sinkVChan), TSQ_FLUSH_ALL, NULL);
		VChanTSQData->taskControl->errorInfo 	= FormatMsg(TaskDataItemsInQueue_Err_OutOfMemory, VChanTSQData->taskControl->taskName, "Out of memory");
		VChanTSQData->taskControl->errorID		= TaskDataItemsInQueue_Err_OutOfMemory;
		ChangeState(VChanTSQData->taskControl, &eventPacket, TC_State_Error); 
	} else {
		*VChanTSQDataPtr = VChanTSQData;
		// inform Task Controller that data was placed in an otherwise empty data queue
		TaskControlEvent(VChanTSQData->taskControl, TC_Event_DataReceived, VChanTSQDataPtr, (DiscardFptr_type)discard_VChanCallbackData_type);
	}
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

int TaskControlEvent (TaskControl_type* RecipientTaskControl, TCEvents event, void* eventData,
					  DiscardFptr_type discardEventDataFptr)
{
	EventPacket_type 	eventpacket = {event, eventData, discardEventDataFptr};
	ChildTCInfo_type*	subTask		= NULL;
	
	int 	lockObtainedFlag;
	int 	error;
	
	// get event TSQ thread lock
	CmtGetLockEx(RecipientTaskControl->eventQThreadLock, 0, CMT_WAIT_FOREVER, &lockObtainedFlag);
	
	// set out of date flag for the parent's record of this child task controller state
	if (RecipientTaskControl->parentTC) {
		subTask = ListGetPtrToItem(RecipientTaskControl->parentTC->childTCs, RecipientTaskControl->childTCIdx);
		subTask->isOutOfDate = TRUE;
	}
	
	if (CmtWriteTSQData(RecipientTaskControl->eventQ, &eventpacket, 1, 0, NULL)) error = 0;
	else error = -1;
	
	// release event TSQ thread lock
	CmtReleaseLock(RecipientTaskControl->eventQThreadLock);
	
	return error;
}

int	TaskControlIterationDone (TaskControl_type* taskControl, int errorID, char errorInfo[], BOOL doAnotherIteration)
{
	if (errorID)
		return TaskControlEvent(taskControl, TC_Event_IterationDone, init_FCallReturn_type(errorID, "External Task Control Iteration", errorInfo), (DiscardFptr_type)discard_FCallReturn_type);
	else {
		if (doAnotherIteration) taskControl->repeat++;
		return TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL);
	}
}

static void ChangeState (TaskControl_type* taskControl, EventPacket_type* eventPacket, TCStates newState)
{
	TaskControl_type*	parentTC	= NULL;
	
	// change state
	taskControl->oldState   = taskControl->state;
	taskControl->state 		= newState;
	
	// add here actions on new state entry
	switch (newState) {
			
		case TC_State_Error:
			
			parentTC = GetTaskControlParent(taskControl);
			// abort execution in the entire Task Tree
			if (parentTC)
				AbortTaskControlExecution(parentTC);
			else
				AbortTaskControlExecution(taskControl);
			
			// call error function to handle error
			FunctionCall(taskControl, eventPacket, TC_Callback_Error, NULL, NULL); 
			break;
			
		default:
			
			break;
	}
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


static int FunctionCall (TaskControl_type* taskControl, EventPacket_type* eventPacket, TCCallbacks fID, void* fCallData, char** errorInfo)
{
#define FunctionCall_Error_Invalid_fID 						-1
#define FunctionCall_Error_FCall_Error						-2
#define FunctionCall_Error_IterationFunctionNotExecuted		-3

	// call function
	int		fCallError 								= 0;
	char*	fCallErrorMsg							= NULL;
	char	CmtErrorMsg[CMT_MAX_MESSAGE_BUF_SIZE]	= "";
	BOOL	taskActive								= FALSE;
	
	// determine if task tree is active or not
	switch ((GetTaskControlRootParent(taskControl))->state) {
		case TC_State_Unconfigured:
		case TC_State_Configured:
		case TC_State_Initial:
		case TC_State_Idle:
		case TC_State_Done:
		case TC_State_Error:
			
			taskActive = FALSE;
			break;
			
		case TC_State_Running:
		case TC_State_IterationFunctionActive:
		case TC_State_Stopping:
			
			taskActive = TRUE;
			break;
	}
	
	// add log entry if enabled
	ExecutionLogEntry(taskControl, eventPacket, FUNCTION_CALL, &fID);
	
	switch (fID) {
		
		case TC_Callback_Configure:
			
			if (!taskControl->ConfigureFptr) return 0;		// function not provided
				
			if ( (fCallError = (*taskControl->ConfigureFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0 ) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Configure", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;  										// success
			
		case TC_Callback_Unconfigure:
			
			if (!taskControl->UnconfigureFptr) return 0;	// function not provided 
			
			if ( (fCallError = (*taskControl->UnconfigureFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0 ) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Unconfigure", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success		
					
		case TC_Callback_Iterate:
			
			if (!taskControl->IterateFptr) { 
				TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL);
				return 0;  									// function not provided 
			}
			
			taskControl->stopIterationsFlag = FALSE;
			
			// call iteration function and set timeout if required to complete the iteration
			if (taskControl->iterTimeout) {  
				// set an iteration timeout async timer until which a TC_Event_IterationDone must be received 
				// if timeout elapses without receiving a TC_Event_IterationDone, a TC_Event_IterationTimeout is generated 
				taskControl->iterationTimerID = NewAsyncTimer(taskControl->iterTimeout, 1, 1, TaskControlIterTimeout, taskControl);
			}
			
			// launch provided iteration function pointer in a separate thread
			if ( (fCallError = CmtScheduleThreadPoolFunctionAdv(taskControl->threadPoolHndl, ScheduleIterateFunction, taskControl, DEFAULT_THREAD_PRIORITY,
					IterationFunctionThreadCallback, EVENT_TP_THREAD_FUNCTION_BEGIN, taskControl, RUN_IN_SCHEDULED_THREAD, NULL)) < 0) {
						
				CmtGetErrorMessage(fCallError, CmtErrorMsg);
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_IterationFunctionNotExecuted, "FCall Iterate", CmtErrorMsg);
				return FunctionCall_Error_IterationFunctionNotExecuted;
			}	
			
			
			return 0;										// success
			
		case TC_Callback_Start:
			
			if (!taskControl->StartFptr) return 0;			// function not provided
				
			if ( (fCallError = (*taskControl->StartFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Start", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
				
		case TC_Callback_Reset:
		
			if (!taskControl->ResetFptr) return 0;			// function not provided
			
			if ( (fCallError = (*taskControl->ResetFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Reset", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TC_Callback_Done:
			
			if (!taskControl->DoneFptr) return 0;			// function not provided
			
			if ( (fCallError = (*taskControl->DoneFptr)(taskControl, taskControl->currentIter, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Done", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TC_Callback_Stopped:
			
			if (!taskControl->StoppedFptr) return 0;		// function not provided		 
				
			if ( (fCallError = (*taskControl->StoppedFptr)(taskControl, taskControl->currentIter, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Stopped", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TASK_FCALL_TASK_TREE_STATUS:
			
			if (!taskControl->TaskTreeStateChangeFptr) return 0;	// function not provided
			
			if ( (fCallError = (*taskControl->TaskTreeStateChangeFptr)(taskControl, *(TaskTreeStates*)fCallData, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Task Tree Status", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			
			return 0;										// success
			
		case TASK_FCALL_SET_UITC_MODE:
			
			if (!taskControl->SetUITCModeFptr) return 0; 
				
			(*taskControl->SetUITCModeFptr)(taskControl, *(BOOL*)fCallData);
			return 0;
			
		case TC_Callback_DataReceived:
			
			// call data received callback if one was provided
			if (!(*(VChanCallbackData_type**)fCallData)->DataReceivedFptr) return 0;	// function not provided 
				
			if ( (fCallError = (*(*(VChanCallbackData_type**)fCallData)->DataReceivedFptr)(taskControl, taskControl->state, taskActive,
							   (*(VChanCallbackData_type**)fCallData)->sinkVChan, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Data Received", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;																	// success
			
		case TC_Callback_CustomEvent:
			
			if (!taskControl->ModuleEventFptr) return 0; 	// function not provided
				
			if ( (fCallError = (*taskControl->ModuleEventFptr)(taskControl, taskControl->state, taskActive, fCallData, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Module Event", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TC_Callback_Error:
			
			// change Task Tree status if this is a Root Task Controller
			if (!taskControl->parentTC) {
				TaskTreeStateChange (taskControl, eventPacket, TaskTree_Finished, &fCallErrorMsg);
				OKfree(fCallErrorMsg);
			}
			
			// call ErrorFptr
			if (taskControl->ErrorFptr)
				(*taskControl->ErrorFptr)(taskControl, taskControl->errorID, taskControl->errorInfo);
			
			
			
			return 0;										// success
				
	}
	
}

int	TaskControlEventToChildTCs  (TaskControl_type* SenderTaskControl, TCEvents event, void* eventData,
								 DiscardFptr_type discardEventDataFptr)
{
	ChildTCInfo_type* 		subTask;
	size_t				nChildTCs 			= ListNumItems(SenderTaskControl->childTCs);
	int			 		lockObtainedFlag	= 0;
	int					error				= 0;
	EventPacket_type 	eventpacket 		= {event, eventData, discardEventDataFptr}; 
	
	// get all childTC eventQ thread locks to place event in all queues before other events can be placed by other threads
	for (size_t i = 1; i <= nChildTCs; i++) { 
		subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i);
		CmtGetLockEx(subTask->childTC->eventQThreadLock, 0, CMT_WAIT_FOREVER, &lockObtainedFlag);
	}
	
	// dispatch event to all childTCs
	for (size_t i = 1; i <= nChildTCs; i++) { 
		subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i);
		subTask->isOutOfDate = TRUE;
		errChk(CmtWriteTSQData(subTask->childTC->eventQ, &eventpacket, 1, 0, NULL));
	}
	
	// release all childTC eventQ thread locks
	for (size_t i = 1; i <= nChildTCs; i++) { 
		subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i);
		CmtReleaseLock(subTask->childTC->eventQThreadLock);  
	}
	 
	return 0;
	
Error:
	
	// release all childTC eventQ thread locks
	for (size_t i = 1; i <= nChildTCs; i++) { 
		subTask = ListGetPtrToItem(SenderTaskControl->childTCs, i);
		CmtReleaseLock(subTask->childTC->eventQThreadLock);  
	}
	
	return error;
}

int	AddChildTCToParent (TaskControl_type* parent, TaskControl_type* child)
{
	ChildTCInfo_type	childTCItem	= {.childTC = child, .childTCState = child->state, .previousChildTCState = child->state, .isOutOfDate = FALSE};
	if (!parent || !child) return -1;
	
	// call UITC Active function to dim/undim UITC Task Control execution
	BOOL	UITCFlag = FALSE;
	if (child->UITCFlag) {
		EventPacket_type eventPacket = {TASK_EVENT_SUBTASK_ADDED_TO_PARENT, NULL, NULL};
		FunctionCall(child, &eventPacket, TASK_FCALL_SET_UITC_MODE, &UITCFlag, NULL);
	}
	// insert childTC
	if (!ListInsertItem(parent->childTCs, &childTCItem, END_OF_LIST)) return -1;

	// add parent pointer to childTC
	child->parentTC = parent;
	// update childTC index within parent list
	child->childTCIdx = ListNumItems(parent->childTCs); 
	
	//link iteration info
	IteratorAddIterator	(GetTaskControlIterator(parent), GetTaskControlIterator(child)); 
	
	return 0;
}

int	RemoveChildTCFromParentTC (TaskControl_type* child)
{
	if (!child || !child->parentTC) return -1;
	ChildTCInfo_type* 	childTCPtr;
	size_t			nChildTCs	= ListNumItems(child->parentTC->childTCs);
	for (size_t i = 1; i <= nChildTCs; i++) {
		childTCPtr = ListGetPtrToItem(child->parentTC->childTCs, i);
		if (child == childTCPtr->childTC) {
			ListRemoveItem(child->parentTC->childTCs, 0, i);
			// update childTC indices
			nChildTCs = ListNumItems(child->parentTC->childTCs);
			for (size_t j = 1; j <= nChildTCs; j++) {
				childTCPtr = ListGetPtrToItem(child->parentTC->childTCs, j);
				childTCPtr->childTC->childTCIdx = j;
			}
			child->childTCIdx = 0;
			child->parentTC = NULL;
			
			// call UITC Active function to dim/undim UITC Task Control execution
			BOOL	UITCFlag = TRUE;
			if (child->UITCFlag) {
				EventPacket_type eventPacket = {TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT, NULL, NULL};  
				FunctionCall(child, &eventPacket, TASK_FCALL_SET_UITC_MODE, &UITCFlag, NULL);
			}
			
			//remove iterator from parent
			RemoveFromParentIterator(GetTaskControlIterator(child));
			
			return 0; // found and removed
		}
		
	}
	
	return -2; // not found
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
	TaskControl_type**   	TCPtr;
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
	TaskControl_type**   	TCPtr;  
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
	size_t n        = 2;
	char*  name;   
	char   countstr [500];
	
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

int	DisassembleTaskTreeBranch (TaskControl_type* taskControlNode)
{
	if (!taskControlNode) return -1;
	
	// disconnect from parent if any
	RemoveChildTCFromParentTC(taskControlNode);
	
	// disconnect incoming Source VChans from the Sink VChans assigned to this Task Controller
	DisconnectAllSinkVChans(taskControlNode);
	
	// disconnect recursively child Task Controllers if any
	ChildTCInfo_type* 	childTCPtr;
	while(ListNumItems(taskControlNode->childTCs)) {
		childTCPtr = ListGetPtrToItem(taskControlNode->childTCs, 1);
		DisassembleTaskTreeBranch(childTCPtr->childTC);
	}
	
	return 0;
}

/// HIFN Called after a certain timeout if a TC_Event_IterationTimeout is not received
int CVICALLBACK TaskControlIterTimeout (int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskControl_type* taskControl = callbackData; 
	
	if (event == EVENT_TIMER_TICK)
		TaskControlEvent(taskControl, TC_Event_IterationTimeout, NULL, NULL);
	
	return 0;
}

static void TaskEventHandler (TaskControl_type* taskControl)
{
#define TaskEventHandler_Error_OutOfMemory					-1
#define TaskEventHandler_Error_FunctionCallFailed			-2
#define TaskEventHandler_Error_MsgPostToChildTCFailed		-3
#define TaskEventHandler_Error_MsgPostToSelfFailed			-4
#define TaskEventHandler_Error_ChildTCInErrorState			-5
#define TaskEventHandler_Error_InvalidEventInState			-6
#define TaskEventHandler_Error_IterateFCallTmeout			-7
#define	TaskEventHandler_Error_IterateExternThread			-8
#define TaskEventHandler_Error_DataPacketsNotCleared		-9

	EventPacket_type 		eventpacket[EVENT_BUFFER_SIZE];
	ChildTCInfo_type* 		childTCPtr; 
	char*					buff			= NULL;
	char*					errMsg			= NULL;
	char*					eventStr;
	char*					stateStr;
	size_t					nchars;
	size_t					nItems;
	int						nEventItems;
	
	// get all Task Controller events in the queue
	while ((nEventItems = CmtReadTSQData(taskControl->eventQ, eventpacket, EVENT_BUFFER_SIZE, 0, 0)) > 0) {
		
	for (int i = 0; i < nEventItems; i++) {
		
	// reset abort flag
	taskControl->abortFlag = FALSE;
	
	switch (taskControl->state) {
		
		case TC_State_Unconfigured:
		
			switch (eventpacket[i].event) {
				
				case TC_Event_Configure:
					
					// configure this task
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Configure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					// send TC_Event_Configure to all childTCs if there are any
					if (TaskControlEventToChildTCs(taskControl, TC_Event_Configure, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Configure posting to ChildTCs failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;	
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					} 
					
					// If there are no childTCs, then reset device/module and make transition here to Initial state and inform parent TC
					if (!ListNumItems(taskControl->childTCs)) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter, 0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
						
					} else
						ChangeState(taskControl, &eventpacket[i], TC_State_Configured);
						
					break;
					
				case TC_Event_Stop:
					
					// ignore event
					
					break;
					
				case TC_Event_Unconfigure:
					
					// ignore event
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE; 
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state, then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					break;
					
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr 	= EventToString(eventpacket[i].event);
					stateStr 	= StateToString(taskControl->state);	 	
					nchars 		= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 		= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff);
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error);
			}
			
			break;
			
		case TC_State_Configured:
		
			switch (eventpacket[i].event) { 
				
				case TC_Event_Configure: 
					
					// configure this TC again
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Configure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					// send TC_Event_Configure to all childTCs if there are any
					if (TaskControlEventToChildTCs(taskControl, TC_Event_Configure, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Configure posting to ChildTCs failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;	
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					} 
					
					break;
					
				case TC_Event_Stop:
					
					// ignore event
					
					break;
					
				case TC_Event_Unconfigure:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Unconfigure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Unconfigured);
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state, then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					// reset task controller and set it to initial state if all child TCs are in their initial state
					if (AllChildTCsInState(taskControl, TC_State_Initial)) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
					}
					
					break;
					
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr 	= EventToString(eventpacket[i].event);
					stateStr 	= StateToString(taskControl->state);	 	
					nchars 		= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 		= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff);
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error);	
			}
			
			break;
			
		case TC_State_Initial:
		
			switch (eventpacket[i].event) {
				
				case TC_Event_Configure:
					
					// configure again this TC
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Configure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					break;
					
				case TC_Event_Unconfigure:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Unconfigure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Unconfigured);
					
					break;
				
				case TC_Event_Reset:
					
					// ignore this command
					break;
				
				case TC_Event_Start:
				case TC_Event_IterateOnce: 
					
					
					if (!taskControl->parentTC) {
						
						// clear task tree recursively if this is the root task controller (i.e. it doesn't have a parent)
						if (ClearTaskTreeBranchVChans(taskControl, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_DataPacketsNotCleared, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_DataPacketsNotCleared;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// if this is the root task controller (i.e. it doesn't have a parent) then change Task Tree status
						if (TaskTreeStateChange(taskControl, &eventpacket[i], TaskTree_Started, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
					}
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Start, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Set flag to iterate once or continue until done or stopped
					//---------------------------------------------------------------------------------------------------------------   
					if (eventpacket[i].event == TC_Event_IterateOnce) 
						taskControl->nIterationsFlag = 1;
					else
						taskControl->nIterationsFlag = -1;
					
					//---------------------------------------------------------------------------------------------------------------
					// Iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TC_Event_Iterate, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Iterate posting to self failed");
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
									
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state
					//---------------------------------------------------------------------------------------------------------------
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Running);  
					
					break;
					
				case TC_Event_Stop:  
				
					// ignore event
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr = ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState = childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState = ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate = FALSE;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state, then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					// if childTC is unconfigured then switch to configured state
					if (childTCPtr->childTCState == TC_State_Unconfigured)
						ChangeState(taskControl, &eventpacket[i], TC_State_Configured);
					
					break;
					
			
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket[i].event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff);
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
			}
			
			break;
			
		case TC_State_Idle:
	
			switch (eventpacket[i].event) {
				
				case TC_Event_Configure:
				
					// ignore this command
					break;
					
				case TC_Event_Unconfigure:
					
					// call unconfigure function and switch to unconfigured state
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Unconfigure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						
					} else 
						ChangeState(taskControl, &eventpacket[i], TC_State_Unconfigured);
					
					break;
					
				case TC_Event_Start:
				case TC_Event_IterateOnce: 
					
					if(!taskControl->parentTC) {
					
						// clear task tree recursively if this is the root task controller (i.e. it doesn't have a parent)
						if (ClearTaskTreeBranchVChans(taskControl, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_DataPacketsNotCleared, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_DataPacketsNotCleared;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
					
						// if this is the root task controller (i.e. it doesn't have a parent) then change Task Tree status
						if (TaskTreeStateChange(taskControl, &eventpacket[i], TaskTree_Started, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
					}
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Start, NULL, &errMsg) <0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Set flag to iterate once or continue until done or stopped
					//---------------------------------------------------------------------------------------------------------------
					
					if (eventpacket[i].event == TC_Event_IterateOnce)
						taskControl->nIterationsFlag = 1;
					else
						taskControl->nIterationsFlag = -1;
					
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state and iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TC_Event_Iterate, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Iterate posting to self failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Running);
					
					break;
					
				case TC_Event_Iterate:
					
					// ignore this command
					break;
					
				case TC_Event_Reset:
					
					// send RESET event to all childTCs
					if (TaskControlEventToChildTCs(taskControl, TC_Event_Reset, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Reset posting to ChildTCs failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					// change state to INITIAL if there are no childTCs and call reset function
					if (!ListNumItems(taskControl->childTCs)) {
						
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID 	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
						
					}
					
					break;
					
				case TC_Event_Stop:  
				
					// ignore event
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state, then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					// if childTC is unconfigured then switch to configured state
					if (childTCPtr->childTCState == TC_State_Unconfigured) {
						ChangeState(taskControl, &eventpacket[i], TC_State_Configured);
						break;
					}
					
					if (AllChildTCsInState(taskControl, TC_State_Initial)) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) <0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
					}
					
					break;
					
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr 		= EventToString(eventpacket[i].event);
					stateStr 		= StateToString(taskControl->state);	 	
					nchars 			= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 			= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
					
			}
			
			break;
			
		case TC_State_Running:
		 
			switch (eventpacket[i].event) {
				
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
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Done, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
							break;
						}
								
						ChangeState(taskControl, &eventpacket[i], TC_State_Done);
						
						// change Task Tree status
						if(!taskControl->parentTC) 
							if (TaskTreeStateChange(taskControl, &eventpacket[i], TaskTree_Finished, &errMsg) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
								taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
								OKfree(errMsg);
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
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
								FunctionCall(taskControl, &eventpacket[i], TC_Callback_Iterate, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_IterationDone posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;  // stop here
								}
										
								
							// switch state and wait for iteration to complete
							ChangeState(taskControl, &eventpacket[i], TC_State_IterationFunctionActive);
									
							break;
							
						case TC_Execute_AfterChildTCsComplete:
							
							//---------------------------------------------------------------------------------------------------------------
							// Start ChildTCs if there are any
							//---------------------------------------------------------------------------------------------------------------
							nItems = ListNumItems(taskControl->childTCs); 		
							if (nItems) {    
								// send START event to all childTCs
								if (TaskControlEventToChildTCs(taskControl, TC_Event_Start, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Start posting to ChildTCs failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
								
								break; // stop here
							}	
							
							//---------------------------------------------------------------------------------------------------------------
							// There are no ChildTCs, call iteration function if needed
							//---------------------------------------------------------------------------------------------------------------
									
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TC_Callback_Iterate, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_IterationDone posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;  // stop here
								}
									
							// switch state and wait for iteration to complete
							ChangeState(taskControl, &eventpacket[i], TC_State_IterationFunctionActive);  
									
							break;
									
						case TC_Execute_InParallelWithChildTCs:
										   
							//---------------------------------------------------------------------------------------------------------------
							// Start ChildTCs if there are any
							//---------------------------------------------------------------------------------------------------------------
							
							if (TaskControlEventToChildTCs(taskControl, TC_Event_Start, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Start posting to ChildTCs failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
							}
							
							//---------------------------------------------------------------------------------------------------------------
							// Call iteration function if needed
							//---------------------------------------------------------------------------------------------------------------
							
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TC_Callback_Iterate, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_IterationDone posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
										
							// switch state and wait for iteration and ChildTCs if there are any to complete
							ChangeState(taskControl, &eventpacket[i], TC_State_IterationFunctionActive);  
									
							break;
									
					}
					
					break;
					
				case TC_Event_Stop:
				// Stops iterations and switches to IDLE or DONE states if there are no ChildTC Controllers or to STOPPING state and waits for ChildTCs to complete their iterations
					
					if (!ListNumItems(taskControl->childTCs)) {
						
						// if there are no ChildTC Controllers
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Done, NULL, &errMsg) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
								taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
								OKfree(errMsg);
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
							}
							
							ChangeState(taskControl, &eventpacket[i], TC_State_Done);
							
						} else 
							// switch to IDLE or DONE state if finite task controller
							if (GetCurrentIterIndex(taskControl->currentIter) < taskControl->repeat) {
								if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Stopped, NULL, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
							
								ChangeState(taskControl, &eventpacket[i], TC_State_Idle);
									
							} else {
								if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Done, NULL, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
							
								ChangeState(taskControl, &eventpacket[i], TC_State_Done);
							}
						
						// change Task Tree status
						if(!taskControl->parentTC)
							if (TaskTreeStateChange(taskControl, &eventpacket[i], TaskTree_Finished, &errMsg) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
								taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
								OKfree(errMsg);
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
							}
							  
					} else {
						// send TC_Event_Stop event to all childTCs
						if (TaskControlEventToChildTCs(taskControl, TC_Event_Stop, NULL, NULL) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Stop posting to ChildTCs failed"); 
							taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Stopping);
					}
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE; 
					//test lex
					if ((childTCPtr->previousChildTCState==TC_State_Done)&&childTCPtr->childTCState==TC_State_Done){
						//break test
						childTCPtr->previousChildTCState=TC_State_Done;	
					}
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					
					// if childTC is in an error state then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo); 
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					//---------------------------------------------------------------------------------------------------------------- 
					// If ChildTCs are not yet complete, stay in RUNNING state and wait for their completion
					//---------------------------------------------------------------------------------------------------------------- 
					
					// consider only transitions to TC_State_Done 
					if (childTCPtr->childTCState != TC_State_Done)
						break; // stop here
					
					if (!AllChildTCsInState(taskControl, TC_State_Done))  
						break; // stop here
					
					//test lex
					//put all child TC's out of date to prevent race here
					size_t	nChildTCs 		= ListNumItems(taskControl->childTCs);
					ChildTCInfo_type*   subTask			= NULL; 
					for (size_t j = 1; j <= nChildTCs; j++) {
						subTask = ListGetPtrToItem(taskControl->childTCs, j);
						subTask->isOutOfDate = TRUE;
					}
					
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
							
							if (TaskControlEvent(taskControl, TC_Event_Iterate, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Iterate posting to self failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
								
							}
							
							// stay in TC_State_Running
							break;
							
						case TC_Execute_AfterChildTCsComplete:
						
							//---------------------------------------------------------------------------------------------------------------
							// ChildTCs are complete, call iteration function
							//---------------------------------------------------------------------------------------------------------------
									
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TC_Callback_Iterate, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_IterationDone posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
									
							ChangeState(taskControl, &eventpacket[i], TC_State_IterationFunctionActive); 	
								
							break;
							
					}
					
					break;
					
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr 		= EventToString(eventpacket[i].event);
					stateStr 		= StateToString(taskControl->state);	 	
					nchars 			= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 			= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo  = FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
					
			}
			
			break;
			
		case TC_State_IterationFunctionActive:
			
			switch (eventpacket[i].event) {
				
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
					if (eventpacket[i].eventData) {
						FCallReturn_type* fCallReturn = eventpacket[i].eventData;
						
						if (fCallReturn->retVal < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_IterateExternThread, taskControl->taskName, fCallReturn->errorInfo); 
							taskControl->errorID	= TaskEventHandler_Error_IterateExternThread;
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								
							break;   // stop here
						}
					}
					
					//---------------------------------------------------------------------------------------------------------------
					// If iteration was aborted or stopped, switch to IDLE or DONE state if there are no ChildTC Controllers,
					// otherwise switch to STOPPING state and wait for ChildTCs to complete their iterations
					//---------------------------------------------------------------------------------------------------------------
					
					if (taskControl->abortFlag) {
						
						if (!ListNumItems(taskControl->childTCs)) {
							
							// if there are no ChildTC Controllers
							if (taskControl->mode == TASK_CONTINUOUS) {
								// switch to DONE state if continuous task controller
								if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Done, NULL, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
							
								ChangeState(taskControl, &eventpacket[i], TC_State_Done);
								
							} else 
								// switch to IDLE or DONE state if finite task controller
							
								if ( GetCurrentIterIndex(taskControl->currentIter) < taskControl->repeat) {
									if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Stopped, NULL, &errMsg) < 0) {
										taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
										taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
										OKfree(errMsg);
										ChangeState(taskControl, &eventpacket[i], TC_State_Error);
										break;
									}
							
									ChangeState(taskControl, &eventpacket[i], TC_State_Idle);
									
								} else { 
									
									if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Done, NULL, &errMsg) < 0) {
										taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
										taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
										OKfree(errMsg);
										ChangeState(taskControl, &eventpacket[i], TC_State_Error);
										break;
									}
							
									ChangeState(taskControl, &eventpacket[i], TC_State_Done);
								}
							
							// change Task Tree status
							if(!taskControl->parentTC)
								if (TaskTreeStateChange(taskControl, &eventpacket[i], TaskTree_Finished, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
						
						} else {
							
							// send TC_Event_Stop event to all childTCs
							if (TaskControlEventToChildTCs(taskControl, TC_Event_Stop, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Stop posting to ChildTCs failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
							}
						
							ChangeState(taskControl, &eventpacket[i], TC_State_Stopping);
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
								if (TaskControlEventToChildTCs(taskControl, TC_Event_Start, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Start posting to ChildTCs failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
										
								// switch to RUNNING state and wait for ChildTCs to complete
								ChangeState(taskControl, &eventpacket[i], TC_State_Running);
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
							
							if (!taskControl->stopIterationsFlag) {
								if (TaskControlEvent(taskControl, TC_Event_Iterate, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Iterate posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
							} else
								if (TaskControlEvent(taskControl, TC_Event_Stop, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Stop posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
								
							ChangeState(taskControl, &eventpacket[i], TC_State_Running); 
							
							break;
									
						case TC_Execute_InParallelWithChildTCs:
							
							//---------------------------------------------------------------------------------------------------------------- 
							// If ChildTCs are not yet complete, switch to RUNNING state and wait for their completion
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (!AllChildTCsInState(taskControl, TC_State_Done)) {
								ChangeState(taskControl, &eventpacket[i], TC_State_Running);
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
							
							if (!taskControl->stopIterationsFlag) { 
								if (TaskControlEvent(taskControl, TC_Event_Iterate, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Iterate posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
							} else
								if (TaskControlEvent(taskControl, TC_Event_Stop, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Stop posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									ChangeState(taskControl, &eventpacket[i], TC_State_Error);
									break;
								}
								
							
							ChangeState(taskControl, &eventpacket[i], TC_State_Running);
							
							break;
					}
					
					break;
					
				case TC_Event_IterationTimeout:
					
					// reset timerID
					taskControl->iterationTimerID = 0;
					
					// generate timeout error
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_IterateFCallTmeout, taskControl->taskName, "Iteration function call timeout"); 
					taskControl->errorID	= TaskEventHandler_Error_IterateFCallTmeout;	
					ChangeState(taskControl, &eventpacket[i], TC_State_Error);
					
					break;
					
				case TC_Event_Stop:
				
					taskControl->stopIterationsFlag = TRUE; // interation function in progress completes normally and no further iterations are requested.
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE;  
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo); 
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					break;
					
				case TC_Event_DataReceived:
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
							
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
							
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					// remove timeout timer
					if (taskControl->iterationTimerID > 0) {
						DiscardAsyncTimer(taskControl->iterationTimerID);
						taskControl->iterationTimerID = 0;
					}
					
					eventStr 		= EventToString(eventpacket[i].event);
					stateStr 		= StateToString(taskControl->state);	 	
					nchars 			= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 			= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff);
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
					
			}
					
			break;
			
		case TC_State_Stopping:
			
			switch (eventpacket[i].event) {
				
				case TC_Event_Stop:
				
					// ignore this command
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE;  
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state, then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
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
						
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Stopped, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
							
						ChangeState(taskControl, &eventpacket[i], TC_State_Idle);
						
						// change Task Tree status
						if(!taskControl->parentTC)
							if (TaskTreeStateChange(taskControl, &eventpacket[i], TaskTree_Finished, &errMsg) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
								taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
								OKfree(errMsg);
								ChangeState(taskControl, &eventpacket[i], TC_State_Error);
								break;
							}
					}
						
					break;
					
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr 		= EventToString(eventpacket[i].event);
					stateStr 		= StateToString(taskControl->state);	 	
					nchars 			= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 			= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff);
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error); 	
			}
			
			break;
			
		case TC_State_Done:
		// This state can be reached only if all ChildTC Controllers are in a DONE state
			
			switch (eventpacket[i].event) {
				
				case TC_Event_Configure:
					
					// configure again this TC
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Configure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					break;
					
				case TC_Event_Unconfigure:
					
					// call unconfigure function and switch to unconfigured state
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Unconfigure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
					
					} else 
						ChangeState(taskControl, &eventpacket[i], TC_State_Unconfigured);
					
					
					break;
					
				case TC_Event_Start:
					
					// reset device/module
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
						
				    // reset iterations
					SetCurrentIterIndex(taskControl->currentIter,0);
					
					// switch to INITIAL state
					ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
					
					// send START event to self
					if (TaskControlEvent(taskControl, TC_Event_Start, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Start self posting failed");
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
						break;
					}
					
					break;
					
				case TC_Event_Reset:
					
					// send RESET event to all childTCs
					if (TaskControlEventToChildTCs(taskControl, TC_Event_Reset, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Reset posting to ChildTCs failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					// change state to INITIAL if there are no childTCs and call reset function
					if (!ListNumItems(taskControl->childTCs)) {
						
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
						
					}
					
					break;
					
				case TC_Event_Stop:  
					
					// ignore event
					
					break;
					
				case TC_Event_UpdateChildTCState:
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState; 
					childTCPtr->isOutOfDate 			= FALSE;  
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if childTC is in an error state, then switch to error state
					if (childTCPtr->childTCState == TC_State_Error) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_ChildTCInErrorState, taskControl->taskName, childTCPtr->childTC->errorInfo); 
						taskControl->errorID	= TaskEventHandler_Error_ChildTCInErrorState;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;	
					}
					
					// if childTC is unconfigured then switch to configured state
					if (childTCPtr->childTCState == TC_State_Unconfigured) {
						ChangeState(taskControl, &eventpacket[i], TC_State_Configured);
						break;
					}
					
					// check states of all childTCs and transition to INITIAL state if all childTCs are in INITIAL state
					if (AllChildTCsInState(taskControl, TC_State_Initial)) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
					}
					
					break;
					
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					eventStr 		= EventToString(eventpacket[i].event);
					stateStr 		= StateToString(taskControl->state);	 	
					nchars 			= snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff 			= malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					ChangeState(taskControl, &eventpacket[i], TC_State_Error); 	
				
			}
			
			break;
			
		case TC_State_Error:
			switch (eventpacket[i].event) {
				
				case TC_Event_Configure:
					// Reconfigures Task Controller
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Unconfigured);
					
					if (TaskControlEvent(taskControl, TC_Event_Configure, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TC_Event_Configure self posting failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error); 
						break;
					}
					
					// clear error
					OKfree(taskControl->errorInfo);
					taskControl->errorID = 0;
					break;
					
				case TC_Event_Reset:
					
					// send RESET event to all childTCs
					if (TaskControlEventToChildTCs(taskControl, TC_Event_Reset, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToChildTCFailed, taskControl->taskName, "TC_Event_Reset posting to ChildTCs failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToChildTCFailed;
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					// reset and change state to Initial if there are no childTCs
					if (!ListNumItems(taskControl->childTCs)) {
						
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID 	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
						
					}
					
					// clear error
					OKfree(taskControl->errorInfo);
					taskControl->errorID = 0;
					break;
					
				case TC_Event_Unconfigure:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Unconfigure, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TC_State_Unconfigured);
					
					break;
					
				case TC_Event_UpdateChildTCState: 
					
					// update childTC state
					childTCPtr 							= ListGetPtrToItem(taskControl->childTCs, ((ChildTCEventInfo_type*)eventpacket[i].eventData)->childTCIdx);
					childTCPtr->previousChildTCState 	= childTCPtr->childTCState; // save old state for debuging purposes 
					childTCPtr->childTCState 			= ((ChildTCEventInfo_type*)eventpacket[i].eventData)->newChildTCState;
					childTCPtr->isOutOfDate 			= FALSE;  
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_UPDATE, NULL);
					
					// if the error has been cleared and all child TCs are in an initial state, then switch to initial state as well
					if (!taskControl->errorID && AllChildTCsInState(taskControl, TC_State_Initial)) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_Reset, NULL, &errMsg) <0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							ChangeState(taskControl, &eventpacket[i], TC_State_Error);
							break;
						}
						
						// reset iterations
						SetCurrentIterIndex(taskControl->currentIter,0);
						
						ChangeState(taskControl, &eventpacket[i], TC_State_Initial);
					}
					
					break;
					
			
				case TC_Event_DataReceived:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_DataReceived, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				case TC_Event_Custom:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TC_Callback_CustomEvent, eventpacket[i].eventData, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						ChangeState(taskControl, &eventpacket[i], TC_State_Error);
						break;
					}
					
					break;
					
				default:
					
					// ignore and stay in error state
					break;
					
			}
			
			break;
	}
	
	// if there is a parent task controller, update it on the state of this child task controller
	if (taskControl->parentTC)
		TaskControlEvent(taskControl->parentTC, TC_Event_UpdateChildTCState, init_ChildTCEventInfo_type(taskControl->childTCIdx, taskControl->state), (DiscardFptr_type)discard_ChildTCEventInfo_type);

	// free memory for extra eventData if any
	if (eventpacket[i].eventData && eventpacket[i].discardEventDataFptr)
		(*eventpacket[i].discardEventDataFptr)(&eventpacket[i].eventData);
		
	} // process another event if there is any
	} // check if there are more events
}
