//==============================================================================
//
// Title:		TaskController.c
// Purpose:		A short description of the implementation.
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

//==============================================================================
// Constants

#define EVENT_BUFFER_SIZE 10										// Number of maximum events read at once from the Task Controller TSQs
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

//==============================================================================
// Types

typedef struct {
	size_t						subtaskIdx;							// For a TASK_EVENT_SUBTASK_STATE_CHANGED event this is a
																	// 1-based index of subtask (from subtasks list) which generated
																	// the event.
	TaskStates_type				newSubTaskState;					// For a TASK_EVENT_SUBTASK_STATE_CHANGED event this is the new state
																	// of the subtask
} SubTaskEventInfo_type;

typedef struct {
	TaskEvents_type 			event;								// Task Control Event.
	void*						eventInfo;							// Extra information per event allocated dynamically
	DisposeEventInfoFptr_type   disposeEventInfoFptr;   			// Function pointer to dispose of the eventInfo
} EventPacket_type;

typedef struct {
	TaskStates_type				subtaskState;						// Updated by parent task when informed by subtask that a state change occured.
	TaskStates_type				previousSubTaskState;				// Previous Subtask state used for logging and debuging.
	TaskControl_type*			subtask;							// Pointer to Subtask Controller.
} SubTask_type;

typedef enum {
	STATE_CHANGE,
	FUNCTION_CALL,
	CHILD_TASK_STATE_CHANGE
} TaskControllerActions;

// Structure binding Task Controller and VChan data for passing to TSQ callback	
typedef struct {
	TaskControl_type* 			taskControl;
	SinkVChan_type* 			sinkVChan;
	DataReceivedFptr_type		DataReceivedFptr;
	CmtTSQCallbackID			itemsInQueueCBID;
} VChanCallbackData_type;

struct TaskControl {
	// Task control data
	char*						taskName;							// Name of Task Controller
	size_t						subtaskIdx;							// 1-based index of subtask from parent Task subtasks list. If task doesn't have a parent task then index is 0.
	CmtTSQHandle				eventQ;								// Event queue to which the state machine reacts.
	CmtThreadLockHandle			eventQThreadLock;					// Thread lock used to coordinate multiple writing threads sending events to the queue.
	ListType					dataQs;								// Incoming data queues, list of VChanCallbackData_type*.
	unsigned int				eventQThreadID;						// Thread ID in which queue events are processed.
	CmtThreadFunctionID			threadFunctionID;					// ID of ScheduleTaskEventHandler that is executed in a separate thread from the main thread.
	TaskStates_type 			state;								// Task Controller state.
	TaskStates_type 			oldState;							// Previous Task Controller state used for logging.
	size_t						repeat;								// Total number of repeats. If repeat is 0, then the iteration function is not called. 
	int							iterTimeout;						// Timeout in [s] until when TaskControlIterationDone can be called. 
	 //add them to Iterator_Type?
	TaskIterMode_type			iterMode;							// Determines how the iteration block of a Task Controller is executed with respect to its subtasks if any.
	TaskMode_type				mode;								// Finite or continuous type of task controller
	Iterator_type* 				currentiter;						// iteration information structure
	TaskControl_type*			parenttask;							// Pointer to parent task that own this subtask. 
																	// If this is the main task, it has no parent and this is NULL. 
	ListType					subtasks;							// List of subtasks of SubTask_type.
	BOOL						tc_started;							// need this to prevent race : parent should keep track if child TC has started (out of initial state)
	BOOL						tc_done;						    // need this to prevent race : parent should keep track if child TC in a done state 
	void*						moduleData;							// Reference to module specific data that is controlled by the task.
	int							logPanHandle;						// Panel handle in which there is a box control for printing Task Controller execution info useful for debugging. If not used, it is set to 0
	int							logBoxControlID;					// Box control ID for printing Task Controller execution info useful for debugging. If not used, it is set to 0.  
	BOOL						loggingEnabled;						// If True, logging info is printed to the provided Box control.
	char*						errorInfo;							// When switching to an error state, additional error info is written here.
	int							errorID;							// Error code encountered when switching to an error state.
	double						waitBetweenIterations;				// During a RUNNING state, waits specified ammount in seconds between iterations
	BOOL						abortFlag;							// When set to TRUE, it signals the provided function pointers that they must abort running processes.
	BOOL						abortIterationFlag;					// When set to TRUE, it signals the external thread running the iteration that it must finish.
	int							nIterationsFlag;					// When -1, the Task Controller is iterated continuously, 0 iteration stops and 1 one iteration.
	int							iterationTimerID;					// Keeps track of the timeout timer when iteration is performed in another thread.
	BOOL						UITCFlag;							// If TRUE, the Task Controller is meant to be used as an User Interface Task Controller that allows the user to control a Task Tree.
	
	// Event handler function pointers
	ConfigureFptr_type			ConfigureFptr;
	UnconfigureFptr_type		UnconfigureFptr;
	IterateFptr_type			IterateFptr;
	AbortIterationFptr_type		AbortIterationFptr;
	StartFptr_type				StartFptr;
	ResetFptr_type				ResetFptr;
	DoneFptr_type				DoneFptr;
	StoppedFptr_type			StoppedFptr;
	DimUIFptr_type				DimUIFptr;
	SetUITCModeFptr_type		SetUITCModeFptr;
	ModuleEventFptr_type		ModuleEventFptr;
	ErrorFptr_type				ErrorFptr;
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void 								TaskEventHandler 						(TaskControl_type* taskControl); 

// Use this function to change the state of a Task Controller
static int 									ChangeState 							(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskStates_type newState);
// Use this function to carry out a Task Controller action using provided function pointers
static int									FunctionCall 							(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskFCall_type fID, void* fCallData, char** errorInfo); 

// Formats a Task Controller log entry based on the action taken
static void									ExecutionLogEntry						(TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskControllerActions action, void* info);

// Task Controller iteration done return info when performing iteration in another thread
static void									disposeTCIterDoneInfo 					(void* eventInfo);


static char*								StateToString							(TaskStates_type state);
static char*								EventToString							(TaskEvents_type event);
static char*								FCallToString							(TaskFCall_type fcall);

// SubTask eventInfo functions
static SubTaskEventInfo_type* 				init_SubTaskEventInfo_type 				(size_t subtaskIdx, TaskStates_type state);
static void									discard_SubTaskEventInfo_type 			(SubTaskEventInfo_type** a);
static void									disposeSubTaskEventInfo					(void* eventInfo);

// VChan and Task Control binding
static VChanCallbackData_type*				init_VChanCallbackData_type				(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr);
static void									discard_VChanCallbackData_type			(VChanCallbackData_type** a);
static void									disposeCmtTSQVChanEventInfo				(void* eventInfo);

// Dims and undims recursively a Task Controller and all its SubTasks by calling the provided function pointer (i.e. dims/undims an entire Task Tree branch)
static void									DimTaskTreeBranch 						(TaskControl_type* taskControl, EventPacket_type* eventPacket, BOOL dimmed);

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
TaskControl_type* init_TaskControl_type(const char					taskControllerName[],
										void*						moduleData,
										ConfigureFptr_type 			ConfigureFptr,
										UnconfigureFptr_type		UnconfigureFptr,
										IterateFptr_type			IterateFptr,
										AbortIterationFptr_type		AbortIterationFptr,
										StartFptr_type				StartFptr,
										ResetFptr_type				ResetFptr,
										DoneFptr_type				DoneFptr,
										StoppedFptr_type			StoppedFptr,
										DimUIFptr_type				DimUIFptr,
										SetUITCModeFptr_type		SetUITCModeFptr,
										ModuleEventFptr_type		ModuleEventFptr,
										ErrorFptr_type				ErrorFptr)
{
	int		error = 0;
	TaskControl_type* tc = malloc (sizeof(TaskControl_type));
	if (!tc) return NULL;
	
	// init
	tc -> eventQ							= 0;
	tc -> eventQThreadLock					= 0;
	tc -> dataQs							= 0;
	tc -> subtasks							= 0;
	
	if (CmtNewTSQ(N_TASK_EVENT_QITEMS, sizeof(EventPacket_type), 0, &tc->eventQ) < 0) goto Error;
	if (CmtNewLock(NULL, 0, &tc->eventQThreadLock) < 0) goto Error;
	
	nullChk( tc -> dataQs					= ListCreate(sizeof(VChanCallbackData_type*)) );
	
	tc -> eventQThreadID					= CmtGetCurrentThreadID ();
	tc -> threadFunctionID					= 0;
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(tc->eventQ, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskEventItemsInQueue, tc, tc->eventQThreadID, NULL); 
	
	nullChk( tc -> subtasks					= ListCreate(sizeof(SubTask_type)) );
	
	tc -> taskName 							= StrDup(taskControllerName);
	tc -> moduleData						= moduleData;
	tc -> subtaskIdx						= 0;
	tc -> state								= TASK_STATE_UNCONFIGURED;
	tc -> oldState							= TASK_STATE_UNCONFIGURED;
	tc -> repeat							= 1;
	tc -> iterTimeout						= 0;								
	tc -> iterMode							= TASK_ITERATE_BEFORE_SUBTASKS_START;
	tc -> mode								= TASK_FINITE;
	tc -> currentiter					    = init_Iterator_type(taskControllerName);   
	tc -> parenttask						= NULL;
	tc -> logPanHandle						= 0;
	tc -> logBoxControlID					= 0;
	tc -> loggingEnabled					= FALSE;
	tc -> errorInfo							= NULL;
	tc -> errorID							= 0;
	tc -> waitBetweenIterations				= 0;
	tc -> abortFlag							= 0;
	tc -> abortIterationFlag				= FALSE;
	tc -> nIterationsFlag					= -1;
	tc -> iterationTimerID					= 0;
	tc -> UITCFlag							= FALSE;
	
	
	
	// task controller function pointers
	tc -> ConfigureFptr 					= ConfigureFptr;
	tc -> UnconfigureFptr					= UnconfigureFptr;
	tc -> IterateFptr						= IterateFptr;
	tc -> AbortIterationFptr				= AbortIterationFptr;
	tc -> StartFptr							= StartFptr;
	tc -> ResetFptr							= ResetFptr;
	tc -> DoneFptr							= DoneFptr;
	tc -> StoppedFptr						= StoppedFptr;
	tc -> DimUIFptr							= DimUIFptr;
	tc -> SetUITCModeFptr					= SetUITCModeFptr;
	tc -> ModuleEventFptr					= ModuleEventFptr;
	tc -> ErrorFptr							= ErrorFptr;
	      
	
	return tc;
	
	Error:
	
	if (tc->eventQ) 				CmtDiscardTSQ(tc->eventQ);
	if (tc->eventQThreadLock)		CmtDiscardLock(tc->eventQThreadLock);
	if (tc->dataQs)	 				ListDispose(tc->dataQs);
	if (tc->subtasks)    			ListDispose(tc->subtasks);
	
	OKfree(tc);
	
	return NULL;
}

/// HIFN Discards recursively a Task controller.
void discard_TaskControl_type(TaskControl_type** taskController)
{
	Iterator_type* iterator;
	
	if (!*taskController) return;
	
	//----------------------------------------------------------------------------
	// Disassemble task tree branch recusively starting with the given parent node
	//----------------------------------------------------------------------------
	DisassembleTaskTreeBranch(*taskController);
	
	//----------------------------------------------------------------------------
	// Discard this Task Controller
	//----------------------------------------------------------------------------
	
	// name
	OKfree((*taskController)->taskName);
	
	// event queue
	CmtDiscardTSQ((*taskController)->eventQ);
	
	// release thread
	if ((*taskController)->threadFunctionID) CmtReleaseThreadPoolFunctionID(DEFAULT_THREAD_POOL_HANDLE, (*taskController)->threadFunctionID);   
	
	// event queue thread lock
	CmtDiscardLock((*taskController)->eventQThreadLock); 
	
	// incoming data queues (does not free the queue itself!)
	RemoveAllSinkVChans(*taskController);
	ListDispose((*taskController)->dataQs);
	
	// error message storage 
	OKfree((*taskController)->errorInfo);
	
	//iteration info
	iterator=(*taskController)->currentiter;
	discard_Iterator_type (&iterator,TRUE,FALSE); 

	// child Task Controllers list
	ListDispose((*taskController)->subtasks);
	
	// free Task Controller memory
	OKfree(*taskController);
	
}

void discard_TaskTreeBranch (TaskControl_type** taskController)
{
	SubTask_type* subtaskPtr;
	while(ListNumItems((*taskController)->subtasks)){
		subtaskPtr = ListGetPtrToItem((*taskController)->subtasks, 1);
		discard_TaskTreeBranch(&subtaskPtr->subtask);
	} 
	
	discard_TaskControl_type(taskController);
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
}

TaskStates_type	GetTaskControlState (TaskControl_type* taskControl)
{
	return taskControl->state;
}

void SetTaskControlIterations (TaskControl_type* taskControl, size_t repeat)
{
	taskControl->repeat = repeat;
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

int	SetTaskControlIterMode (TaskControl_type* taskControl, TaskIterMode_type iterMode)
{
	taskControl->iterMode = iterMode;
	return 0; // set succefully
}

TaskIterMode_type GetTaskControlIterMode (TaskControl_type* taskControl)
{
	return taskControl->iterMode; 	
}

int	SetTaskControlCurrentIter (TaskControl_type* taskControl, Iterator_type* currentiter)
{
	taskControl->currentiter = currentiter;
	return 0; // set succefully
}

Iterator_type* GetTaskControlCurrentIter (TaskControl_type* taskControl)
{
	return taskControl->currentiter; 	
}

Iterator_type* GetTaskControlCurrentIterDup (TaskControl_type* taskControl)
{
	Iterator_type* dup=DupIterator(taskControl->currentiter);
	
	return dup; 	
}

void SetTaskControlMode	(TaskControl_type* taskControl, TaskMode_type mode)
{
	taskControl->mode = mode;	
}

BOOL GetTaskControlTCDone (TaskControl_type* taskControl)
{
	return taskControl->tc_done;
}

void SetTaskControlTCDone (TaskControl_type* taskControl,BOOL done)
{
	taskControl->tc_done=done;
}

BOOL GetTaskControlTCStarted (TaskControl_type* taskControl)
{
	return taskControl->tc_started;
}

void SetTaskControlTCStarted (TaskControl_type* taskControl,BOOL started)
{
	taskControl->tc_started=started;
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
	return taskControl->parenttask;
}

TaskControl_type* GetTaskControlRootParent (TaskControl_type* taskControl)
{
	TaskControl_type*	rootTC = taskControl;
	
	while (rootTC->parenttask)
		rootTC = rootTC->parenttask;
	
	return rootTC;
}

ListType GetTaskControlSubTasks (TaskControl_type* taskControl)
{
	size_t	nSubTasks = ListNumItems(taskControl->subtasks);
	
	ListType SubTasks = ListCreate(sizeof(TaskControl_type*));
	if (!SubTasks) return 0;
	
	SubTask_type*	subtaskPtr;
	for (size_t i = 1; i <= nSubTasks; i++) {
		subtaskPtr = ListGetPtrToItem (taskControl->subtasks, i);
		ListInsertItem(SubTasks, &subtaskPtr->subtask, END_OF_LIST);
	}
	
	return SubTasks;
}

void SetTaskControlUITCFlag	(TaskControl_type* taskControl, BOOL UITCFlag)
{
	taskControl->UITCFlag = UITCFlag; 
}

BOOL GetTaskControlUITCFlag	(TaskControl_type* taskControl)
{
	return taskControl->UITCFlag; 
}

BOOL GetTaskControlAbortIterationFlag (TaskControl_type* taskControl)
{
	return taskControl->abortIterationFlag;
}


//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller data queue and data exchange functions
//------------------------------------------------------------------------------------------------------------------------------------------------------
int	AddSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr)
{
#define 	AddSinkVChan_Err_OutOfMemory				-1
#define		AddSinkVChan_Err_SinkAlreadyAssigned		-2
#define		AddSinkVChan_Err_TaskControllerIsActive		-3				// Task Controller must not be executing when a Sink VChan is added to it
	// check if Sink VChan is already assigned to the Task Controller
	VChanCallbackData_type**	VChanTSQDataPtr;
	size_t 						nItems = ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if ((*VChanTSQDataPtr)->sinkVChan == sinkVChan) return AddSinkVChan_Err_SinkAlreadyAssigned;
	}
	
	// check if Task Controller is currently executing
	if (!(taskControl->state == TASK_STATE_UNCONFIGURED || taskControl->state == TASK_STATE_CONFIGURED || 
		  taskControl->state == TASK_STATE_INITIAL || taskControl->state == TASK_STATE_DONE || taskControl->state == TASK_STATE_ERROR))
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

int	RemoveSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan)
{
#define		RemoveSinkVChan_Err_SinkNotFound				-1
#define		RemoveSinkVChan_Err_TaskControllerIsActive		-2
	
	VChanCallbackData_type**	VChanTSQDataPtr;
	size_t						nDataQs					= ListNumItems(taskControl->dataQs);
	
	
	// check if Task Controller is active
	if (!(taskControl->state == TASK_STATE_UNCONFIGURED || taskControl->state == TASK_STATE_CONFIGURED || 
		  taskControl->state == TASK_STATE_INITIAL || taskControl->state == TASK_STATE_DONE || taskControl->state == TASK_STATE_ERROR))
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
	if (!(taskControl->state == TASK_STATE_UNCONFIGURED || taskControl->state == TASK_STATE_CONFIGURED || 
		  taskControl->state == TASK_STATE_INITIAL || taskControl->state == TASK_STATE_DONE || taskControl->state == TASK_STATE_ERROR))
		return RemoveAllSinkVChans_Err_TaskControllerIsActive;
	
	VChanCallbackData_type** 	VChanTSQDataPtr;
	size_t 						nItems = ListNumItems(taskControl->dataQs);
	
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

static SubTaskEventInfo_type* init_SubTaskEventInfo_type (size_t subtaskIdx, TaskStates_type state)
{
	SubTaskEventInfo_type* a = malloc(sizeof(SubTaskEventInfo_type));
	if (!a) return NULL;
	
	a -> subtaskIdx 		= subtaskIdx;
	a -> newSubTaskState	= state;
	
	return a;
}

static void discard_SubTaskEventInfo_type (SubTaskEventInfo_type** a)
{
	if (!*a) return;
	OKfree(*a);
}

static void	disposeSubTaskEventInfo	(void* eventInfo)
{
	SubTaskEventInfo_type* eventInfoPtr = eventInfo;
	
	discard_SubTaskEventInfo_type(&eventInfoPtr);
}

static void	disposeCmtTSQVChanEventInfo (void* eventInfo)
{
	if (!eventInfo) return;
	free(eventInfo);
}

static void	DimTaskTreeBranch (TaskControl_type* taskControl, EventPacket_type* eventPacket, BOOL dimmed)
{
	if (!taskControl) return;
	
	// dim/undim
	FunctionCall(taskControl, eventPacket, TASK_FCALL_DIM_UI, &dimmed, NULL); 
	
	size_t			nSubTasks = ListNumItems(taskControl->subtasks);
	SubTask_type*	subTaskPtr;
	
	for (size_t i = nSubTasks; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		DimTaskTreeBranch (subTaskPtr->subtask, eventPacket, dimmed);
	}
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
	
	size_t			nSubTasks	 = ListNumItems(taskControl->subtasks);
	SubTask_type*	subTaskPtr;
	
	for (size_t i = nSubTasks; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		errChk( ClearTaskTreeBranchVChans(subTaskPtr->subtask, errorInfo) );
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

static void	discard_VChanCallbackData_type (VChanCallbackData_type** a)
{
	if (!*a) return;
	
	OKfree(*a);
}

void dispose_FCallReturn_EventInfo (void* eventInfo)
{
	FCallReturn_type* fCallReturnPtr = eventInfo;
	
	discard_FCallReturn_type(&fCallReturnPtr);
}

static char* StateToString (TaskStates_type state)
{
	switch (state) {
		
		case TASK_STATE_UNCONFIGURED:
			
			return StrDup("Unconfigured");
			
		case TASK_STATE_CONFIGURED:
			
			return StrDup("Configured");
			
		case TASK_STATE_INITIAL:
			
			return StrDup("Initial");
			
		case TASK_STATE_IDLE:
			
			return StrDup("Idle");
			
		case TASK_STATE_RUNNING:
			
			return StrDup("Running");
			
		case TASK_STATE_RUNNING_WAITING_ITERATION:
			
			return StrDup("Running and Waiting For Iteration Completion");
			
		case TASK_STATE_STOPPING:
			
			return StrDup("Stopping");
			
		case TASK_STATE_DONE:
			
			return StrDup("Done"); 
			
		case TASK_STATE_ERROR:
			
			return StrDup("Error"); 
			
		default:
			
			return StrDup("?");  
	}
	
}

static char* EventToString (TaskEvents_type event)
{
	switch (event) {
		
		case TASK_EVENT_CONFIGURE:
			
			return StrDup("Configure");
			
		case TASK_EVENT_START:
			
			return StrDup("Start");
			
		case TASK_EVENT_ITERATE:
			
			return StrDup("Iterate");
			
		case TASK_EVENT_ITERATION_DONE:
			
			return StrDup("Iteration Done"); 
			
		case TASK_EVENT_ITERATION_TIMEOUT:
			
			return StrDup("Iteration Timeout");
			
		case TASK_EVENT_ITERATE_ONCE:
			
			return StrDup("One Iteration");
			
		case TASK_EVENT_RESET:
			
			return StrDup("Reset");
			
		case TASK_EVENT_STOP:
			
			return StrDup("Stop"); 
			
		case TASK_EVENT_STOP_CONTINUOUS_TASK:
			
			return StrDup("Stop continuous SubTask Controllers only"); 
			
		case TASK_EVENT_SUBTASK_STATE_CHANGED:
			
			return StrDup("SubTask State Changed");
			
		case TASK_EVENT_DATA_RECEIVED:
			
			return StrDup("Data received"); 
			
		case TASK_EVENT_CUSTOM_MODULE_EVENT:
			
			return StrDup("Device or Module specific event");
			
		case TASK_EVENT_SUBTASK_ADDED_TO_PARENT:
			
			return StrDup("SubTask added to parent Task"); 
			
		case TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT:
			
			return StrDup("SubTask removed from parent Task"); 
			
	}
	
	return StrDup("?");
	
}

static char* FCallToString (TaskFCall_type fcall)
{
	switch (fcall) {
		
		case TASK_FCALL_NONE:
			
			return StrDup("FCall None");
		
		case TASK_FCALL_CONFIGURE:
			
			return StrDup("FCall Configure");
			
		case TASK_FCALL_UNCONFIGURE:
			
			return StrDup("FCall Unconfigure");
		
		case TASK_FCALL_ITERATE:
			
			return StrDup("FCall Iterate");
			
		case TASK_FCALL_ABORT_ITERATION:
			
			return StrDup("FCall Abort Iteration");
			
		case TASK_FCALL_START:
			
			return StrDup("FCall Start");
			
		case TASK_FCALL_RESET:
			
			return StrDup("FCall Reset");
			
		case TASK_FCALL_DONE:
			
			return StrDup("FCall Done");
			
		case TASK_FCALL_STOPPED:
			
			return StrDup("FCall Stopped");
			
		case TASK_FCALL_DIM_UI:
			
			return StrDup("FCall Dim/Undim UI");
			
		case TASK_FCALL_SET_UITC_MODE:
			
			return StrDup("FCall Set UITC Mode");
			
		case TASK_FCALL_DATA_RECEIVED:
			
			return StrDup("FCall Data Received");
			
		case TASK_FCALL_MODULE_EVENT:
			
			return StrDup("FCall Module Event");
			
		case TASK_FCALL_ERROR:
			
			return StrDup("FCall Error");
			
	}
	
	return StrDup("?");
	
}

static void ExecutionLogEntry (TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskControllerActions action, void* info)
{
	if (!taskControl->loggingEnabled) return;
		
	SubTask_type*	subtaskPtr;
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
	Fmt(buf, "%s<(iteration: %d)", GetCurrentIterationIndex(taskControl->currentiter));
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
			
			AppendString(&output, (fCallName = FCallToString(*(TaskFCall_type*)info)), -1);
			OKfree(fCallName)
			break;
			
		case CHILD_TASK_STATE_CHANGE:
			
			AppendString(&output, "Child state change", -1); 
			break;
	}
	AppendString(&output, ")", -1);  
	
	//---------------------------------------------------------------
	// SubTask Controller States
	//---------------------------------------------------------------
	AppendString(&output, ",  (child states: {", -1);
	size_t	nSubTasks	= ListNumItems(taskControl->subtasks);
	for (size_t i = 1; i <= nSubTasks; i++) {
		subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		AppendString(&output, "(", -1);
		// Task Controller Name
		if (subtaskPtr->subtask->taskName)
			AppendString(&output, subtaskPtr->subtask->taskName, -1);
		else
			AppendString(&output, "No name", -1);
		
		AppendString(&output, ", ", -1);
		// if subtask state change event occurs, print also previous state of the subtask that changed
		if (action == CHILD_TASK_STATE_CHANGE)
			if (((SubTaskEventInfo_type*)eventPacket->eventInfo)->subtaskIdx == i){
				AppendString(&output, (stateName = StateToString(subtaskPtr->previousSubTaskState)), -1);
				OKfree(stateName);
				AppendString(&output, "->", -1);
			}
				
		AppendString(&output, (stateName = StateToString(subtaskPtr->subtaskState)), -1);
		OKfree(stateName);
		AppendString(&output, ")", -1);
		if (i < nSubTasks)
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
		CmtGetThreadPoolFunctionAttribute(DEFAULT_THREAD_POOL_HANDLE, taskControl->threadFunctionID, 
									  ATTR_TP_FUNCTION_EXECUTION_STATUS, &ExecutionStatus);
	
	if (ExecutionStatus == kCmtThreadFunctionComplete) {
		if (taskControl->threadFunctionID) CmtReleaseThreadPoolFunctionID(DEFAULT_THREAD_POOL_HANDLE, taskControl->threadFunctionID);
		CmtScheduleThreadPoolFunctionAdv(DEFAULT_THREAD_POOL_HANDLE, ScheduleTaskEventHandler, taskControl, 
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
	EventPacket_type			eventPacket			= {TASK_EVENT_DATA_RECEIVED, NULL, NULL};
	if (!VChanTSQDataPtr) {
		// flush queue
		CmtFlushTSQ(GetSinkVChanTSQHndl(VChanTSQData->sinkVChan), TSQ_FLUSH_ALL, NULL);
		VChanTSQData->taskControl->errorInfo 	= FormatMsg(TaskDataItemsInQueue_Err_OutOfMemory, VChanTSQData->taskControl->taskName, "Out of memory");
		VChanTSQData->taskControl->errorID		= TaskDataItemsInQueue_Err_OutOfMemory;
		FunctionCall(VChanTSQData->taskControl, &eventPacket, TASK_FCALL_ERROR, NULL, NULL);
		ChangeState(VChanTSQData->taskControl, &eventPacket, TASK_STATE_ERROR); 
	} else {
		*VChanTSQDataPtr = VChanTSQData;
		// inform Task Controller that data was placed in an otherwise empty data queue
		TaskControlEvent(VChanTSQData->taskControl, TASK_EVENT_DATA_RECEIVED, VChanTSQDataPtr, disposeCmtTSQVChanEventInfo);
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

int TaskControlEvent (TaskControl_type* RecipientTaskControl, TaskEvents_type event, void* eventInfo,
					  DisposeEventInfoFptr_type disposeEventInfoFptr)
{
	EventPacket_type eventpacket = {event, eventInfo, disposeEventInfoFptr};
	
	int 	lockObtainedFlag;
	int 	error;
	
	// get event TSQ thread lock
	CmtGetLockEx(RecipientTaskControl->eventQThreadLock, 0, CMT_WAIT_FOREVER, &lockObtainedFlag);
	
	if (CmtWriteTSQData(RecipientTaskControl->eventQ, &eventpacket, 1, 0, NULL)) error = 0;
	else error = -1;
	
	// release event TSQ thread lock
	CmtReleaseLock(RecipientTaskControl->eventQThreadLock);
	
	return error;
}

int	TaskControlIterationDone (TaskControl_type* taskControl, int errorID, char errorInfo[], BOOL doAnotherIteration)
{
	if (errorID)
		return TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, init_FCallReturn_type(errorID, "External Task Control Iteration", errorInfo), disposeTCIterDoneInfo);
	else {
		if (doAnotherIteration) taskControl->repeat++;
		return TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL);
	}
}

static void disposeTCIterDoneInfo (void* eventInfo) 
{
	discard_FCallReturn_type((FCallReturn_type**)&eventInfo);
}

static int ChangeState (TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskStates_type newState)
{
	int error;
			  
	// change state
	taskControl->oldState   = taskControl->state;
	taskControl->state 		= newState;
	
	
	// add log entry if enabled
	ExecutionLogEntry(taskControl, eventPacket, STATE_CHANGE, NULL);

	
	// if there is a parent task, inform it of subtask state chage
	if (taskControl->parenttask)
		if ((error = TaskControlEvent(taskControl->parenttask, TASK_EVENT_SUBTASK_STATE_CHANGED, 
					init_SubTaskEventInfo_type(taskControl->subtaskIdx, taskControl->state), disposeSubTaskEventInfo)) < 0) goto Error;
		
	return 0;
	
	Error:
	
	return error;
}

void AbortTaskControlExecution (TaskControl_type* taskControl)
{
	SubTask_type* subtaskPtr;
		
	taskControl->abortFlag 				= TRUE;
	taskControl->abortIterationFlag		= TRUE;
	
	// send STOP event to self
	TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL);
	
	// abort SubTasks recursively
	size_t	nSubTasks = ListNumItems(taskControl->subtasks);
	for (size_t i = 1; i <= nSubTasks; i++) {
		subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		AbortTaskControlExecution(subtaskPtr->subtask);
	}
	
}

int CVICALLBACK ScheduleIterateFunction (void* functionData)
{
	TaskControl_type* taskControl = functionData;
	
	(*taskControl->IterateFptr)(taskControl, &taskControl->abortIterationFlag);
	
	return 0;
}




static int FunctionCall (TaskControl_type* taskControl, EventPacket_type* eventPacket, TaskFCall_type fID, void* fCallData, char** errorInfo)
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
		case TASK_STATE_UNCONFIGURED:
		case TASK_STATE_CONFIGURED:
		case TASK_STATE_INITIAL:
		case TASK_STATE_IDLE:
		case TASK_STATE_DONE:
		case TASK_STATE_ERROR:
			
			taskActive = FALSE;
			break;
			
		case TASK_STATE_RUNNING:
		case TASK_STATE_RUNNING_WAITING_ITERATION:
		case TASK_STATE_STOPPING:
			
			taskActive = TRUE;
			break;
	}
	
	// add log entry if enabled
	ExecutionLogEntry(taskControl, eventPacket, FUNCTION_CALL, &fID);
	
	switch (fID) {
		
		case TASK_FCALL_NONE:
			
			if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_Invalid_fID, "FCall", "Not a valid function ID");
			return FunctionCall_Error_Invalid_fID;			// not a valid fID, error
			
		case TASK_FCALL_CONFIGURE:
			
			if (!taskControl->ConfigureFptr) return 0;		// function not provided
				
			if ( (fCallError = (*taskControl->ConfigureFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0 ) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Configure", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;  										// success
			
		case TASK_FCALL_UNCONFIGURE:
			
			if (!taskControl->UnconfigureFptr) return 0;	// function not provided 
			
			if ( (fCallError = (*taskControl->UnconfigureFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0 ) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Unconfigure", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success		
					
		case TASK_FCALL_ITERATE:
			
			if (!taskControl->IterateFptr) { 
				TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL);
				return 0;  									// function not provided 
			}
			
			// reset abort iteration flag
			taskControl->abortIterationFlag = FALSE;
			
			// call iteration function and set timeout if required to complete the iteration
			if (taskControl->iterTimeout) {  
				// set an iteration timeout async timer until which a TASK_EVENT_ITERATION_DONE must be received 
				// if timeout elapses without receiving a TASK_EVENT_ITERATION_DONE, a TASK_EVENT_ITERATION_TIMEOUT is generated 
				taskControl->iterationTimerID = NewAsyncTimer(taskControl->iterTimeout, 1, 1, TaskControlIterTimeout, taskControl);
			}
			
			// launch provided iteration function pointer in a separate thread
			if ( (fCallError = CmtScheduleThreadPoolFunctionAdv(DEFAULT_THREAD_POOL_HANDLE, ScheduleIterateFunction, taskControl, DEFAULT_THREAD_PRIORITY,
					IterationFunctionThreadCallback, EVENT_TP_THREAD_FUNCTION_BEGIN, taskControl, RUN_IN_SCHEDULED_THREAD, NULL)) < 0) {
						
				CmtGetErrorMessage(fCallError, CmtErrorMsg);
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_IterationFunctionNotExecuted, "FCall Iterate", CmtErrorMsg);
				return FunctionCall_Error_IterationFunctionNotExecuted;
			}	
			
			
			return 0;										// success
			
		case TASK_FCALL_ABORT_ITERATION:
			
			if (!taskControl->AbortIterationFptr) return 0;	// function not provided
			
			(*taskControl->AbortIterationFptr)(taskControl, &taskControl->abortFlag);
			return 0;										// success
			
		case TASK_FCALL_START:
			
			if (!taskControl->StartFptr) return 0;			// function not provided
				
			if ( (fCallError = (*taskControl->StartFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Start", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
				
		case TASK_FCALL_RESET:
		
			if (!taskControl->ResetFptr) return 0;			// function not provided
			
			if ( (fCallError = (*taskControl->ResetFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Reset", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TASK_FCALL_DONE:
			
			if (!taskControl->DoneFptr) return 0;			// function not provided
			
			if ( (fCallError = (*taskControl->DoneFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Done", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TASK_FCALL_STOPPED:
			
			if (!taskControl->StoppedFptr) return 0;		// function not provided		 
				
			if ( (fCallError = (*taskControl->StoppedFptr)(taskControl, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Stopped", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TASK_FCALL_DIM_UI:
			
			if (!taskControl->DimUIFptr) return 0;			// function not provided
				
			(*taskControl->DimUIFptr)(taskControl, *(BOOL*)fCallData); 
			return 0;										// success
			
		case TASK_FCALL_SET_UITC_MODE:
			
			if (!taskControl->SetUITCModeFptr) return 0; 
				
			(*taskControl->SetUITCModeFptr)(taskControl, *(BOOL*)fCallData);
			return 0;
			
		case TASK_FCALL_DATA_RECEIVED:
			
			// call data received callback if one was provided
			if (!(*(VChanCallbackData_type**)fCallData)->DataReceivedFptr) return 0;	// function not provided 
				
			if ( (fCallError = (*(*(VChanCallbackData_type**)fCallData)->DataReceivedFptr)(taskControl, taskControl->state, taskActive,
							   (*(VChanCallbackData_type**)fCallData)->sinkVChan, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Data Received", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;																	// success
			
		case TASK_FCALL_MODULE_EVENT:
			
			if (!taskControl->ModuleEventFptr) return 0; 	// function not provided
				
			if ( (fCallError = (*taskControl->ModuleEventFptr)(taskControl, taskControl->state, taskActive, fCallData, &taskControl->abortFlag, &fCallErrorMsg)) < 0) {
				if (errorInfo) *errorInfo = FormatMsg(FunctionCall_Error_FCall_Error, "FCall Module Event", fCallErrorMsg);
				OKfree(fCallErrorMsg);
				return FunctionCall_Error_FCall_Error;
			}
			return 0;										// success
			
		case TASK_FCALL_ERROR:
			
			// undim Task tree if this is a Root Task Controller
			if (!taskControl->parenttask)
				DimTaskTreeBranch (taskControl, eventPacket, FALSE);

			// call ErrorFptr
			if (!taskControl->ErrorFptr) return 0;			// function not provided
				
			(*taskControl->ErrorFptr)(taskControl, taskControl->errorID, taskControl->errorInfo);
			return 0;										// success
				
	}
	
}

int	TaskControlEventToSubTasks  (TaskControl_type* SenderTaskControl, TaskEvents_type event, void* eventInfo,
								 DisposeEventInfoFptr_type disposeEventInfoFptr)
{
	SubTask_type* 		subtaskPtr;
	size_t				nSubTasks 			= ListNumItems(SenderTaskControl->subtasks);
	int			 		lockObtainedFlag	= 0;
	int					error				= 0;
	EventPacket_type 	eventpacket 		= {event, eventInfo, disposeEventInfoFptr}; 
	
	// get all subtask eventQ thread locks to place event in all queues before other events can be placed by other threads
	for (size_t i = 1; i <= nSubTasks; i++) { 
		subtaskPtr = ListGetPtrToItem(SenderTaskControl->subtasks, i);
		CmtGetLockEx(subtaskPtr->subtask->eventQThreadLock, 0, CMT_WAIT_FOREVER, &lockObtainedFlag);
	}
	
	// dispatch event to all subtasks
	for (size_t i = 1; i <= nSubTasks; i++) { 
		subtaskPtr = ListGetPtrToItem(SenderTaskControl->subtasks, i);
		errChk(CmtWriteTSQData(subtaskPtr->subtask->eventQ, &eventpacket, 1, 0, NULL));
	}
	
	// release all subtask eventQ thread locks
	for (size_t i = 1; i <= nSubTasks; i++) { 
		subtaskPtr = ListGetPtrToItem(SenderTaskControl->subtasks, i);
		CmtReleaseLock(subtaskPtr->subtask->eventQThreadLock);  
	}
	 
	return 0;
	
Error:
	
	// release all subtask eventQ thread locks
	for (size_t i = 1; i <= nSubTasks; i++) { 
		subtaskPtr = ListGetPtrToItem(SenderTaskControl->subtasks, i);
		CmtReleaseLock(subtaskPtr->subtask->eventQThreadLock);  
	}
	
	return error;
}

int	AddSubTaskToParent (TaskControl_type* parent, TaskControl_type* child)
{
	SubTask_type	subtaskItem;
	if (!parent || !child) return -1;
	
	subtaskItem.subtask					= child;
	subtaskItem.subtaskState			= child->state;
	subtaskItem.previousSubTaskState	= child->state;
	
	// call UITC Active function to dim/undim UITC Task Control execution
	BOOL	UITCFlag = FALSE;
	if (child->UITCFlag) {
		EventPacket_type eventPacket = {TASK_EVENT_SUBTASK_ADDED_TO_PARENT, NULL, NULL};
		FunctionCall(child, &eventPacket, TASK_FCALL_SET_UITC_MODE, &UITCFlag, NULL);
	}
	// insert subtask
	if (!ListInsertItem(parent->subtasks, &subtaskItem, END_OF_LIST)) return -1;

	// add parent pointer to subtask
	child->parenttask = parent;
	// update subtask index within parent list
	child->subtaskIdx = ListNumItems(parent->subtasks); 
	
	//link iteration info
	IteratorAddIterator	(GetTaskControlCurrentIter(parent), GetTaskControlCurrentIter(child)); 
	
	return 0;
}

int	RemoveSubTaskFromParent	(TaskControl_type* child)
{
	if (!child || !child->parenttask) return -1;
	SubTask_type* 	subtaskPtr;
	size_t			nSubTasks	= ListNumItems(child->parenttask->subtasks);
	for (size_t i = 1; i <= nSubTasks; i++) {
		subtaskPtr = ListGetPtrToItem(child->parenttask->subtasks, i);
		if (child == subtaskPtr->subtask) {
			ListRemoveItem(child->parenttask->subtasks, 0, i);
			// update subtask indices
			nSubTasks = ListNumItems(child->parenttask->subtasks);
			for (size_t i = 1; i <= nSubTasks; i++) {
				subtaskPtr = ListGetPtrToItem(child->parenttask->subtasks, i);
				subtaskPtr->subtask->subtaskIdx = i;
			}
			child->subtaskIdx = 0;
			child->parenttask = NULL;
			
			// call UITC Active function to dim/undim UITC Task Control execution
			BOOL	UITCFlag = TRUE;
			if (child->UITCFlag) {
				EventPacket_type eventPacket = {TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT, NULL, NULL};  
				FunctionCall(child, &eventPacket, TASK_FCALL_SET_UITC_MODE, &UITCFlag, NULL);
			}
			
			//remove iterator from parent
			RemoveFromParentIterator(GetTaskControlCurrentIter(child));
			
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
	RemoveSubTaskFromParent(taskControlNode);
	
	// disconnect incoming Source VChans from the Sink VChans assigned to this Task Controller
	DisconnectAllSinkVChans(taskControlNode);
	
	// disconnect recursively child Task Controllers if any
	SubTask_type* 	subtaskPtr;
	while(ListNumItems(taskControlNode->subtasks)) {
		subtaskPtr = ListGetPtrToItem(taskControlNode->subtasks, 1);
		DisassembleTaskTreeBranch(subtaskPtr->subtask);
	}
	
	return 0;
}

/// HIFN Called after a certain timeout if a TASK_EVENT_ITERATION_TIMEOUT is not received
int CVICALLBACK TaskControlIterTimeout (int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskControl_type* taskControl = callbackData; 
	
	if (event == EVENT_TIMER_TICK)
		TaskControlEvent(taskControl, TASK_EVENT_ITERATION_TIMEOUT, NULL, NULL);
	
	return 0;
}

static void TaskEventHandler (TaskControl_type* taskControl)
{
#define TaskEventHandler_Error_OutOfMemory					-1
#define TaskEventHandler_Error_FunctionCallFailed			-2
#define TaskEventHandler_Error_MsgPostToSubTaskFailed		-3
#define TaskEventHandler_Error_MsgPostToSelfFailed			-4
#define TaskEventHandler_Error_SubTaskInErrorState			-5
#define TaskEventHandler_Error_InvalidEventInState			-6
#define TaskEventHandler_Error_IterateFCallTmeout			-7
#define	TaskEventHandler_Error_IterateExternThread			-8
#define TaskEventHandler_Error_DataPacketsNotCleared		-9

	EventPacket_type 		eventpacket[EVENT_BUFFER_SIZE];
	SubTask_type* 			subtaskPtr; 
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
		
		case TASK_STATE_UNCONFIGURED:
		
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
					
					// configure this task
					
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_CONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					// send TASK_EVENT_CONFIGURE to all subtasks if there are any
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE posting to SubTasks failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;	
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					} 
					
					// If there are no subtasks, then reset device/module and make transition here to 
					// INITIAL state and inform parent task if there is any
					if (!ListNumItems(taskControl->subtasks)) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						SetCurrentIterationIndex(taskControl->currentiter,0);
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
						
					} else
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_CONFIGURED);
						
					break;
					
				case TASK_EVENT_STOP:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
			}
			
			break;
			
		case TASK_STATE_CONFIGURED:
		
			switch (eventpacket[i].event) { 
				
				case TASK_EVENT_CONFIGURE: 
					// Reconfigures Task Controller and all its SubTasks
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_STOP:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					//added lex
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					// check states of all subtasks and transition to INITIAL state if all subtasks are in INITIAL state
					BOOL InitialStateFlag = 1; // assume all subtasks are in INITIAL state
					nItems = ListNumItems(taskControl->subtasks); 
					for (size_t i = 1; i <= nItems; i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_INITIAL) {
							InitialStateFlag = 0;
							break;
						}
					}
					
					if (InitialStateFlag) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						SetCurrentIterationIndex(taskControl->currentiter,0);
						
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);	
			}
			
			break;
			
		case TASK_STATE_INITIAL:
		
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed");
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
						break;
					}
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ITERATE_ONCE: 
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_START, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Set flag to iterate once or continue until done or stopped
					//---------------------------------------------------------------------------------------------------------------   
					if (eventpacket[i].event == TASK_EVENT_ITERATE_ONCE) 
						taskControl->nIterationsFlag = 1;
					else
						taskControl->nIterationsFlag = -1;
					
					//-------------------------------------------------------------------------------------------------------------------------
					// If this is a Root Task Controller, i.e. it doesn't have a parent, then: 
					// - call dim UI function recursively for its SubTasks
					// - clear data packets from SubTask Sink VChans recursively
					//-------------------------------------------------------------------------------------------------------------------------
					
					if(!taskControl->parenttask) {
						if (ClearTaskTreeBranchVChans(taskControl, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_DataPacketsNotCleared, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_DataPacketsNotCleared;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						DimTaskTreeBranch(taskControl, &eventpacket[i], TRUE);
					}

					//---------------------------------------------------------------------------------------------------------------
					// Iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed");
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
									
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state
					//---------------------------------------------------------------------------------------------------------------
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING);  
					
					break;
					
				case TASK_EVENT_STOP:  
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// just inform the parent that the current state is INITIAL
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					//added lex
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured then switch to configured state
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED)
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_CONFIGURED);
					
					break;
					
			
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
			}
			
			break;
			
		case TASK_STATE_IDLE:
	
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						taskControl->errorID 	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// call unconfigure function and switch to unconfigured state
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					} else {
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ITERATE_ONCE: 
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_START, NULL, &errMsg) <0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Set flag to iterate once or continue until done or stopped
					//---------------------------------------------------------------------------------------------------------------   
					if (eventpacket[i].event == TASK_EVENT_ITERATE_ONCE)
						taskControl->nIterationsFlag = 1;
					else
						taskControl->nIterationsFlag = -1;
					
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state and iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING);
					
					//-------------------------------------------------------------------------------------------------------------------------
					// If this is a Root Task Controller, i.e. it doesn't have a parent, then call dim UI function recursively for its SubTasks
					//---------------------------------------------------------------------------------------------------------------------
					
					if(!taskControl->parenttask)
						DimTaskTreeBranch(taskControl, &eventpacket[i], TRUE);
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					// ignore this command
						
					break;
					
				case TASK_EVENT_RESET:
					
					// send RESET event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_RESET, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_RESET posting to SubTasks failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					// change state to INITIAL if there are no subtasks and call reset function
					if (!ListNumItems(taskControl->subtasks)) {
						
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID 	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						SetCurrentIterationIndex(taskControl->currentiter,0);
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
						
					}
					
					break;
					
				case TASK_EVENT_STOP:  
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_IDLE);  // just inform parent
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					//added lex
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured then switch to unconfigured state
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED)
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						} else {
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
							break;
						}
					
					// check states of all subtasks and transition to INITIAL state if all subtasks are in INITIAL state
					BOOL InitialStateFlag = 1; // assume all subtasks are in INITIAL state
					nItems = ListNumItems(taskControl->subtasks);
					for (size_t i = 1; i <= nItems; i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_INITIAL) {
							InitialStateFlag = 0;
							break;
						}
					}
					
					if (InitialStateFlag) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) <0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						SetCurrentIterationIndex(taskControl->currentiter,0);
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
					
			}
			
			break;
			
		case TASK_STATE_RUNNING:
		 
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_ITERATE:
					
					//---------------------------------------------------------------------------------------------------------------
					// Check if an iteration is needed:
					// If current iteration index is smaller than the total number of requested repeats, except if repeat = 0 and
					// first iteration or if task is continuous.
					//---------------------------------------------------------------------------------------------------------------
					
					// The iterate event is self generated, when it occurs, all child TCs are in an initial or done state
					// For a Finite TC for both repeat == 0 and repeat == 1 the TC will undergo one iteration, however when 
					// repeat == 0, its iteration callback function will not be called.
					size_t curriterindex=GetCurrentIterationIndex(taskControl->currentiter);
					if ( taskControl->mode == TASK_FINITE  &&  (curriterindex>= taskControl->repeat  ||  !taskControl->nIterationsFlag)  && curriterindex ) {			
						//---------------------------------------------------------------------------------------------------------------- 	 
						// Task Controller is finite switch to DONE
						//---------------------------------------------------------------------------------------------------------------- 	
										
						// call done function
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DONE, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
							break;
						}
								
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);
						
						// undim Task Tree if this is a Root Task Controller
						if(!taskControl->parenttask) 
						DimTaskTreeBranch(taskControl, &eventpacket[i], FALSE);
						
						break; // stop here
					}
						
					//---------------------------------------------------------------------------------------------------------------
					// If not first iteration, then wait given number of seconds before iterating    
					//---------------------------------------------------------------------------------------------------------------
					
					if (GetCurrentIterationIndex(taskControl->currentiter) && taskControl->nIterationsFlag < 0)
						SyncWait(Timer(), taskControl->waitBetweenIterations);
					
					//---------------------------------------------------------------------------------------------------------------
					// Iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					switch (taskControl->iterMode) {
						
						case TASK_ITERATE_BEFORE_SUBTASKS_START:
							
							//---------------------------------------------------------------------------------------------------------------
							// Call iteration function if needed	 
							//---------------------------------------------------------------------------------------------------------------
									
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ITERATE, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;  // stop here
								}
										
								
							// switch state and wait for iteration to complete
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING_WAITING_ITERATION);
									
							break;
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
							
							//---------------------------------------------------------------------------------------------------------------
							// Start SubTasks if there are any
							//---------------------------------------------------------------------------------------------------------------
							nItems = ListNumItems(taskControl->subtasks); 		
							if (nItems) {    
								// send START event to all subtasks
								//reset subtasks info 
								for (size_t i = 1; i <= nItems; i++) {									 
									subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
									SetTaskControlTCDone(subtaskPtr->subtask,FALSE);  
									SetTaskControlTCStarted(subtaskPtr->subtask,FALSE);  
								}
					
								if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
								
								break; // stop here
							}	
							
							//---------------------------------------------------------------------------------------------------------------
							// There are no SubTasks, call iteration function if needed
							//---------------------------------------------------------------------------------------------------------------
									
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ITERATE, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;  // stop here
								}
									
							// switch state and wait for iteration to complete
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING_WAITING_ITERATION);  
									
							break;
									
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
										   
							//---------------------------------------------------------------------------------------------------------------
							// Start SubTasks if there are any
							//---------------------------------------------------------------------------------------------------------------
							nItems=ListNumItems(taskControl->subtasks);		
							if (nItems){ 		//check this part LEX
								// send START event to all subtasks
									//reset num_subtasks_started, to keep track whether all child TC started
								//reset subtasks info 
								for (size_t i = 1; i <= nItems; i++) {									 
									subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
									SetTaskControlTCDone(subtaskPtr->subtask,FALSE);  
									SetTaskControlTCStarted(subtaskPtr->subtask,FALSE);  
								}
								
								if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
							}
							//---------------------------------------------------------------------------------------------------------------
							// Call iteration function if needed
							//---------------------------------------------------------------------------------------------------------------
							
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ITERATE, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
										
							// switch state and wait for iteration and SubTasks if there are any to complete
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING_WAITING_ITERATION);  
									
							break;
									
					}
					
					break;
					
				case TASK_EVENT_STOP:
				// Stops iterations and switches to IDLE or DONE states if there are no SubTask Controllers or to STOPPING state and waits for SubTasks to complete their iterations
					
					if (!ListNumItems(taskControl->subtasks)) {
						
						// undim Task Tree if this is a root Task Controller
							if(!taskControl->parenttask)
							DimTaskTreeBranch(taskControl, &eventpacket[i], FALSE);
							
						// if there are no SubTask Controllers
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DONE, NULL, &errMsg) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
								taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
								OKfree(errMsg);
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);
							
						} else 
							// switch to IDLE or DONE state if finite task controller
							if (GetCurrentIterationIndex(taskControl->currentiter) < taskControl->repeat) {
								if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_STOPPED, NULL, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_IDLE);
									
							} else {
								if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DONE, NULL, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);
							}
							  
					} else {
						// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
						if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
							taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_STOPPING);
					}
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// send STOP command to self if continuous task controller and forward TASK_EVENT_STOP_CONTINUOUS_TASK to subtasks
					if (taskControl->mode == TASK_CONTINUOUS) 
						if (TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL) < 0) {
							taskControl->errorInfo  = FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_STOP self posting failed"); 
							taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;	
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
							break;
						}
					
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
						
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					
					// if subtask is in an error state then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo); 
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					//---------------------------------------------------------------------------------------------------------------- 
					// If SubTasks are not yet complete, stay in RUNNING state and wait for their completion
					//---------------------------------------------------------------------------------------------------------------- 
					
					// because of a possible race condition, the parent TC of multiple child TC's have to check if all child TC's
					// have left their done state
					
					// consider only transitions to TASK_STATE_DONE OR transitions from TASK_STATE_INITIAL
					if ((subtaskPtr->subtaskState != TASK_STATE_DONE)&&(subtaskPtr->previousSubTaskState != TASK_STATE_INITIAL))
						break; // stop here
					
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// check all SubTasks
					BOOL AllStartedFlag = TRUE;    
					nItems = ListNumItems(taskControl->subtasks);
					for (size_t i = 1; i <= nItems; i++) {									 
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (!GetTaskControlTCStarted(subtaskPtr->subtask)){
							AllStartedFlag = FALSE;
							break;
						}
					}
					if (!AllStartedFlag) break;
					//skip checking if not all child TCs have started
					
					//all modules have started their iteration, now it is time to check if all child TCs are done   
					// assume all subtasks are done 
					
					BOOL AllDoneFlag=TRUE;		
					// check SubTasks
					nItems = ListNumItems(taskControl->subtasks);
					for (size_t i = 1; i <= nItems; i++) {									 
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (!GetTaskControlTCDone(subtaskPtr->subtask)){
							AllDoneFlag = FALSE;
							break;
						}
					}
		
					if (!AllDoneFlag)  break; // stop here
					
					//---------------------------------------------------------------------------------------------------------------- 
					// Decide on state transition
					//---------------------------------------------------------------------------------------------------------------- 
					switch (taskControl->iterMode) {
						
						case TASK_ITERATE_BEFORE_SUBTASKS_START:
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
							
							//---------------------------------------------------------------------------------------------------------------
							// Increment iteration index
							//---------------------------------------------------------------------------------------------------------------
							SetCurrentIterationIndex(taskControl->currentiter, GetCurrentIterationIndex(taskControl->currentiter)+1);
							if (taskControl->nIterationsFlag > 0)
								taskControl->nIterationsFlag--;
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
								break;
								
							}
							
							// stay in TASK_STATE_RUNNING
							
							break;
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
						
							//---------------------------------------------------------------------------------------------------------------
							// SubTasks are complete, call iteration function
							//---------------------------------------------------------------------------------------------------------------
									
							if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ITERATE, NULL, NULL);
							else 
								if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
									
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING_WAITING_ITERATION); 	
								
							break;
							
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					
					taskControl->errorInfo  = FormatMsg(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					taskControl->errorID	= TaskEventHandler_Error_InvalidEventInState;
					OKfree(buff);
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
					
			}
			
			break;
			
		case TASK_STATE_RUNNING_WAITING_ITERATION:
			
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_ITERATION_DONE:
					
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
					if (eventpacket[i].eventInfo) {
						FCallReturn_type* fCallReturn = eventpacket[i].eventInfo;
						
						if (fCallReturn->retVal < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_IterateExternThread, taskControl->taskName, fCallReturn->errorInfo); 
							taskControl->errorID	= TaskEventHandler_Error_IterateExternThread;
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
								
							break;   // stop here
						}
					}
					
					//---------------------------------------------------------------------------------------------------------------
					// If iteration was aborted , stop iterations and switch to IDLE or DONE state if there are no 
					// SubTask Controllers, otherwise switch to STOPPING state and wait for SubTasks to complete their iterations
					//---------------------------------------------------------------------------------------------------------------
					
					if (taskControl->abortIterationFlag) {
						
						if (!ListNumItems(taskControl->subtasks)) {
							// undim Task Tree if this is a root Task Controller
							if(!taskControl->parenttask)
							DimTaskTreeBranch(taskControl, &eventpacket[i], FALSE);
								
							// if there are no SubTask Controllers
							if (taskControl->mode == TASK_CONTINUOUS) {
								// switch to DONE state if continuous task controller
								if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DONE, NULL, &errMsg) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
									taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
									OKfree(errMsg);
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);
								
							} else 
								// switch to IDLE or DONE state if finite task controller
							
								if ( GetCurrentIterationIndex(taskControl->currentiter) < taskControl->repeat) {
									if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_STOPPED, NULL, &errMsg) < 0) {
										taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
										taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
										OKfree(errMsg);
										FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
										ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
										break;
									}
							
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_IDLE);
									
								} else { 
									
									if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DONE, NULL, &errMsg) < 0) {
										taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
										taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
										OKfree(errMsg);
										FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
										ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
										break;
									}
							
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);
								}
						
						} else {
							
							// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
							if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
								break;
							}
						
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_STOPPING);
						}
						
						break; // stop here
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Decide how to switch state
					//--------------------------------------------------------------------------------------------------------------- 
					
					switch (taskControl->iterMode) {
						
						case TASK_ITERATE_BEFORE_SUBTASKS_START:
							
							//----------------------------------------------------------------------------------------------------------------
							// Start SubTasks if there are any
							//----------------------------------------------------------------------------------------------------------------
							nItems=ListNumItems(taskControl->subtasks);		
							if (nItems) {
								// send START event to all subtasks
								//reset num_subtasks_started, to keep track whether all child TC started
								for (size_t i = 1; i <= nItems; i++) {									 
									subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
									SetTaskControlTCDone(subtaskPtr->subtask,FALSE);  
									SetTaskControlTCStarted(subtaskPtr->subtask,FALSE);  
								}
								if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
									taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
									taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
									FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
									ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
									break;
								}
										
								// switch to RUNNING state and wait for SubTasks to complete
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING);
								break; // stop here
										
							} 
							
							// If there are no SubTasks, fall through the case and ask for another iteration unless only one iteration was requested
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
							
							//---------------------------------------------------------------------------------------------------------------
							// Increment iteration index
							//---------------------------------------------------------------------------------------------------------------
							SetCurrentIterationIndex(taskControl->currentiter,GetCurrentIterationIndex(taskControl->currentiter)+1);
							if (taskControl->nIterationsFlag > 0)
								taskControl->nIterationsFlag--;
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING); 
							
							break;
									
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
							
							//---------------------------------------------------------------------------------------------------------------- 
							// If SubTasks are not yet complete, switch to RUNNING state and wait for their completion
							//---------------------------------------------------------------------------------------------------------------- 
							
				
							BOOL AllDoneFlag=TRUE;		
							// check SubTasks
							nItems = ListNumItems(taskControl->subtasks);
							for (size_t i = 1; i <= nItems; i++) {									 
								subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
								if (!GetTaskControlTCDone(subtaskPtr->subtask)){
									AllDoneFlag = FALSE;
									break;
								}
							}
					
							if (!AllDoneFlag) {
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING);
								break;  // stop here
							}
							
						    //---------------------------------------------------------------------------------------------------------------
							// Increment iteration index
							//---------------------------------------------------------------------------------------------------------------
							SetCurrentIterationIndex(taskControl->currentiter,GetCurrentIterationIndex(taskControl->currentiter)+1);
							
							if (taskControl->nIterationsFlag > 0)
								taskControl->nIterationsFlag--;
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
								taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
								FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
								ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
								break;
								
							}
							
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_RUNNING);
							
							break;
					}
					
					break;
					
				case TASK_EVENT_ITERATION_TIMEOUT:
					
					// reset timerID
					taskControl->iterationTimerID = 0;
					
					// generate timeout error
					taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_IterateFCallTmeout, taskControl->taskName, "Iteration function call timeout"); 
					taskControl->errorID	= TaskEventHandler_Error_IterateFCallTmeout;	
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_STOP:
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
				
					taskControl->abortIterationFlag = TRUE;
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ABORT_ITERATION, NULL, NULL);
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					//added lex
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// if subtask is in an error state then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo); 
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
							
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
							
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				default:
					
					// remove timeout timer
					if (taskControl->iterationTimerID > 0) {
						DiscardAsyncTimer(taskControl->iterationTimerID);
						taskControl->iterationTimerID = 0;
					}
					
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
					
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
					
			}
					
			break;
			
		case TASK_STATE_STOPPING:
			
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_STOP:
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState;
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					//added lex
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo);
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					BOOL	IdleOrDoneFlag	= TRUE;		// assume all subtasks are either IDLE or DONE
					nItems = ListNumItems(taskControl->subtasks);
					
					for (size_t i = 1; i <= nItems; i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (!((subtaskPtr->subtaskState == TASK_STATE_IDLE) || (subtaskPtr->subtaskState == TASK_STATE_DONE))) {
							IdleOrDoneFlag = FALSE;
							break;
						}
					}
					
					// if all SubTasks are IDLE or DONE, switch to IDLE state
					if (IdleOrDoneFlag) {
						// undim Task Tree if this is a root Task Controller
						if(!taskControl->parenttask)
							DimTaskTreeBranch(taskControl, &eventpacket[i], FALSE);
							
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_STOPPED, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
							
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_IDLE);
					}
						
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 	
			}
			
			break;
			
		case TASK_STATE_DONE:
		// This state can be reached only if all SubTask Controllers are in a DONE state
			
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// call unconfigure function and switch to unconfigured state
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					} else {
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
					
					// reset device/module
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
						
				    // reset iterations
					SetCurrentIterationIndex(taskControl->currentiter,0);
					
					// switch to INITIAL state
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
					
					// send START event to self
					if (TaskControlEvent(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_START self posting failed");
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_RESET:
					
					// send RESET event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_RESET, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_RESET posting to SubTasks failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSubTaskFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					// change state to INITIAL if there are no subtasks and call reset function
					if (!ListNumItems(taskControl->subtasks)) {
						
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						SetCurrentIterationIndex(taskControl->currentiter,0);
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
						
					}
					
					break;
					
				case TASK_EVENT_STOP:  
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);  // just inform parent 
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_DONE);  // just inform parent
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					//added lex
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorInfo); 
						taskControl->errorID	= TaskEventHandler_Error_SubTaskInErrorState;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured then switch to configured state
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_CONFIGURED);
						break;
					}
					
					// check states of all subtasks and transition to INITIAL state if all subtasks are in INITIAL state
					BOOL InitialStateFlag = 1; // assume all subtasks are in INITIAL state
					nItems = ListNumItems(taskControl->subtasks);
					for (size_t i = 1; i <= nItems; i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_INITIAL) {
							InitialStateFlag = 0;
							break;
						}
					}
					
					if (InitialStateFlag) {
						// reset device/module
						if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_RESET, NULL, &errMsg) < 0) {
							taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
							taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
							OKfree(errMsg);
							FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
							ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						SetCurrentIterationIndex(taskControl->currentiter,0);
						
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
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
					
					FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 	
				
			}
			
			break;
			
		case TASK_STATE_ERROR:
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
					// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						taskControl->errorID	= TaskEventHandler_Error_MsgPostToSelfFailed;
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR); 
						break;
					}
					
					// clear error
					OKfree(taskControl->errorInfo);
					taskControl->errorID = 0;
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// unconfigure
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_UNCONFIGURE, NULL, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, &eventpacket[i], TASK_STATE_UNCONFIGURED);
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ITERATE:
				case TASK_EVENT_RESET:
				case TASK_EVENT_STOP:
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
				case TASK_EVENT_ITERATION_TIMEOUT:
					
					// ignore and stay in error state
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED: 
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->previousSubTaskState = subtaskPtr->subtaskState; // save old state for debuging purposes 
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					ExecutionLogEntry(taskControl, &eventpacket[i], CHILD_TASK_STATE_CHANGE, NULL);
					
					if (subtaskPtr->previousSubTaskState == TASK_STATE_INITIAL) SetTaskControlTCStarted(subtaskPtr->subtask,TRUE); 
					if (subtaskPtr->subtaskState == TASK_STATE_DONE) SetTaskControlTCDone(subtaskPtr->subtask,TRUE); 
					
					break;
					
			
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if (FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo, &errMsg) < 0) {
						taskControl->errorInfo 	= FormatMsg(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg);
						taskControl->errorID	= TaskEventHandler_Error_FunctionCallFailed;
						OKfree(errMsg);
						FunctionCall(taskControl, &eventpacket[i], TASK_FCALL_ERROR, NULL, NULL);
						ChangeState(taskControl, &eventpacket[i], TASK_STATE_ERROR);
						break;
					}
					
					break;
					
			}
			
			break;
	}
	
	// free memory for extra eventInfo if any
	if (eventpacket[i].eventInfo && eventpacket[i].disposeEventInfoFptr)
		(*eventpacket[i].disposeEventInfoFptr)(eventpacket[i].eventInfo);
		
	} // process another event if there is any
	} // check if there are more events
}
