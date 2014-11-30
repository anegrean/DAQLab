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

//==============================================================================
// Constants

#define EVENT_BUFFER_SIZE 10								// Number of maximum events read at once from the Task Controller TSQs
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

//==============================================================================
// Types

typedef struct {
	size_t						subtaskIdx;					// For a TASK_EVENT_SUBTASK_STATE_CHANGED event this is a
															// 1-based index of subtask (from subtasks list) which generated
															// the event.
	TaskStates_type				newSubTaskState;			// For a TASK_EVENT_SUBTASK_STATE_CHANGED event this is the new state
															// of the subtask
} SubTaskEventInfo_type;

typedef struct {											// For a TASK_EVENT_HWTRIG_SLAVE_ARMED event
	size_t						slaveHWTrigIdx;				// 1-based index of Slave HW Triggered Task Controller from the Master HW Trigerring Slave list.
} SlaveHWTrigTaskEventInfo_type;

typedef struct {
	TaskEvents_type 			event;						// Task Control Event.
	void*						eventInfo;					// Extra information per event allocated dynamically
	DisposeEventInfoFptr_type   disposeEventInfoFptr;   	// Function pointer to dispose of the eventInfo
} EventPacket_type;

typedef struct {
	TaskStates_type				subtaskState;				// Updated by parent task when informed by subtask that a state change occured.
	TaskControl_type*			subtask;					// Pointer to Subtask Controller.
} SubTask_type;

struct SlaveHWTrigTask{
	BOOL						armed;						// TRUE if a Slave HW Triggered Task Controller is ready to be triggered by its Master
	TaskControl_type*			slaveHWTrigTask;			// pointer to Slave HW Triggered Task Controller.
} ;

typedef enum {
	TASK_CONTROL_STATE_CHANGE,
	TASK_CONTROL_FUNCTION_CALL
} Action_type;

typedef struct {
	int							errorID;
	char*						errorInfo;
} ErrorMsg_type;

typedef struct TaskControlLog {
	TaskControl_type*			taskControl;				// Pointer to Task Controller for which this record is made.
	double						timestamp;					// Timestamp at which the record was created.
	TaskEvents_type				event;						// Event that triggered the action
	ListType					subtasks;					// SubTask states when the event occured. List of SubTask_type
	size_t						IterIdx;					// Current iteration index when event occured.
	Action_type					action;						// Action type: state change or function call that the event triggered
	TaskStates_type				oldState;					// In case of state change: old state of Task Controller before a state transition.
	TaskStates_type				newState;					// In case of state change: new state of Task Controller after a state transition. 
	TaskFCall_type				fcallID;					// In case of function call: the ID of function which is called.
} TaskControlLog_type;

struct TaskExecutionLog {
	ListType					log;						// Keeps record of Task Controller events, state changes and actions. List of TaskControlLog_type.
	CmtThreadLockHandle			lock;						// Prevents multiple threads to add at the same time elements to the list
	int							logBoxPanHandle;
	int							logBoxControl;
};

// Structure binding Task Controller and VChan data for passing to TSQ callback	
typedef struct {
	TaskControl_type* 			taskControl;
	SinkVChan_type* 			sinkVChan;
	DataReceivedFptr_type		DataReceivedFptr;
	CmtTSQCallbackID			itemsInQueueCBID;
} VChanCallbackData_type;

struct TaskControl {
	// Task control data
	char*						taskName;					// Name of Task Controller
	size_t						subtaskIdx;					// 1-based index of subtask from parent Task subtasks list. If task doesn't have a parent task then index is 0.
	size_t						slaveHWTrigIdx;				// 1-based index of Slave HW Trig from Master HW Trig Slave list. If Task Controller is not a HW Trig Slave, then index is 0.
	CmtTSQHandle				eventQ;						// Event queue to which the state machine reacts.
	CmtThreadLockHandle			eventQThreadLock;			// Thread lock used to coordinate multiple writing threads sending events to the queue.
	ListType					dataQs;						// Incoming data queues, list of VChanCallbackData_type*.
	unsigned int				eventQThreadID;				// Thread ID in which queue events are processed.
	CmtThreadFunctionID			threadFunctionID;			// ID of ScheduleTaskEventHandler that is executed in a separate thread from the main thread.
	TaskStates_type 			state;						// Module execution state.
	size_t						repeat;						// Total number of repeats. If repeat is 0, then the iteration function is not called. 
	int							iterTimeout;				// 0, Task Controller iteration block is complete and execution continues with return from
															// the iteration function call. 1..N > 0, Task Controller iteration block is not yet complete
															// and TASK_EVENT_ITERATION_DONE must be received (from another thread) to consider the iteration block complete.
															// If TASK_EVENT_ITERATION_DONE is not received after iterTimeout seconds then the Task Controller execution will time out 
															// and genrate an error. For values -N..-1, Task Controller iteration block does not time out and waits for TASK_EVENT_ITERATION_DONE. 
															// to signal that iteration block is complete.
	TaskIterMode_type			iterMode;					// Determies how the iteration block of a Task Controller is executed with respect to its subtasks if any.
	TaskMode_type				mode;						// Finite or continuous type of task controller
	size_t						currIterIdx;    			// 1-based task execution iteration index  
	TaskControl_type*			parenttask;					// Pointer to parent task that own this subtask. 
															// If this is the main task, it has no parent and this is NULL. 
	ListType					subtasks;					// List of subtasks of SubTask_type.
	TaskControl_type*			masterHWTrigTask;			// If this is a Slave HW Triggered Task Controller, then masterHWTrigTask is a Task Controller 
	ListType					slaveHWTrigTasks;			// If this is a Master HW Triggered Task Controller then slaveHWTrigTasks is a list of SlaveHWTrigTask_type
															// HW Triggered Slave Task Controllers.
	void*						moduleData;					// Reference to module specific data that is 
															// controlled by the task.
	TaskExecutionLog_type*		logPtr;						// Pointer to logging structure. NULL if logging is not enabled.
	ErrorMsg_type*				errorMsg;					// When switching to an error state, additional error info is written here
	double						waitBetweenIterations;		// During a RUNNING state, waits specified ammount in seconds between iterations
	BOOL						abortFlag;					// When set to TRUE, it signals the provided function pointers that they must abort running processes.
	BOOL						abortIterationFlag;			// When set to TRUE, it signals the external thread running the iteration that it must finish.
	BOOL						slaveArmedFlag;				// TRUE when HW Triggering is enabled for the Task Controller and Slave has been armed before sending TASK_EVENT_ITERATION_DONE.
	int							nIterationsFlag;			// When -1, the Task Controller is iterated continuously, 0 iteration stops and 1 one iteration.
	int							iterationTimerID;			// Keeps track of the timeout timer when iteration is performed in another thread.
	BOOL						UITCFlag;					// If TRUE, the Task Controller is meant to be used as an User Interface Task Controller that allows the user to control a Task Tree.
	
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

static void 					TaskEventHandler 				(TaskControl_type* taskControl); 

// Use this function to change the state of a Task Controller
static int 						ChangeState 					(TaskControl_type* taskControl, TaskEvents_type event, TaskStates_type newState);
// Use this function to carry out a Task Controller action using provided function pointers
static ErrorMsg_type*			FunctionCall 					(TaskControl_type* taskControl, TaskEvents_type event, TaskFCall_type fID, void* fCallData); 

// Formats a TaskControlLog_type entry to a string which should be disposed with free().
static char*					ExecutionLogEntry				(TaskControlLog_type* logItem);

// Error formatting functions
static ErrorMsg_type* 			init_ErrorMsg_type 				(int errorID, const char errorOrigin[], const char errorDescription[]);
static void						discard_ErrorMsg_type 			(ErrorMsg_type** a);
static ErrorMsg_type*			FCallReturnToErrorMsg			(FCallReturn_type** fCallReturn, TaskFCall_type fID);

// Task Controller iteration done return info when performing iteration in another thread
static void						disposeTCIterDoneInfo 			(void* eventInfo);


static char*					StateToString					(TaskStates_type state);
static char*					EventToString					(TaskEvents_type event);
static char*					FCallToString					(TaskFCall_type fcall);

// SubTask eventInfo functions
static SubTaskEventInfo_type* 	init_SubTaskEventInfo_type 		(size_t subtaskIdx, TaskStates_type state);
static void						discard_SubTaskEventInfo_type 	(SubTaskEventInfo_type** a);
static void						disposeSubTaskEventInfo			(void* eventInfo);

// HW Trigger eventInfo functions
static SlaveHWTrigTaskEventInfo_type* 	init_SlaveHWTrigTaskEventInfo_type 		(size_t slaveHWTrigIdx);
static void								discard_SlaveHWTrigTaskEventInfo_type	(SlaveHWTrigTaskEventInfo_type** a);
static void								disposeSlaveHWTrigTaskEventInfo			(void* eventInfo);

// Determine type of Master and Slave HW Trigger Task Controller
static BOOL						MasterHWTrigTaskHasChildSlaves	(TaskControl_type* taskControl);
static BOOL						SlaveHWTrigTaskHasParentMaster	(TaskControl_type* taskControl); 

// Determine if HW Triggered Slaves from a HW Triggering Master are Armed
static BOOL						HWTrigSlavesAreArmed			(TaskControl_type* master);

// Reset Armed status of HW Triggered Slaves from the list of a Master HW Triggering Task Controller
static void						ResetHWTrigSlavesArmedStatus	(TaskControl_type* master);

// VChan and Task Control binding
static VChanCallbackData_type*	init_VChanCallbackData_type		(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr);
static void						discard_VChanCallbackData_type	(VChanCallbackData_type** a);
static void						disposeCmtTSQVChanEventInfo		(void* eventInfo);

// Dims and undims recursively a Task Controller and all its SubTasks by calling the provided function pointer (i.e. dims/undims an entire Task Tree branch)
static void						DimTaskTreeBranch 				(TaskControl_type* taskControl, TaskEvents_type event, BOOL dimmed);

// Clears recursively all data packets from the Sink VChans of all Task Controllers in a Task Tree Branch starting with the given Task Controller.
static void						ClearTaskTreeBranchVChans		(TaskControl_type* taskControl);

//==============================================================================
// Global variables

int 	mainThreadID;			// Main thread ID where UI callbacks are processed

//==============================================================================
// Global functions

void 	CVICALLBACK TaskEventItemsInQueue 				(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

void 	CVICALLBACK TaskDataItemsInQueue 				(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

int 	CVICALLBACK ScheduleTaskEventHandler 			(void* functionData);

int 	CVICALLBACK ScheduleIterateFunction 			(void* functionData); 

//void 	CVICALLBACK TaskEventHandlerExecutionCallback 	(CmtThreadPoolHandle poolHandle, CmtThreadFunctionID functionID, unsigned int event, int value, void *callbackData);

int 	CVICALLBACK TaskControlIterTimeout 				(int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2); 

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
	TaskControl_type* a = malloc (sizeof(TaskControl_type));
	if (!a) return NULL;
	
	a -> eventQ				= 0;
	a -> eventQThreadLock	= 0;
	a -> dataQs				= 0;
	a -> subtasks			= 0;
	a -> slaveHWTrigTasks	= 0;
	
	if (CmtNewTSQ(N_TASK_EVENT_QITEMS, sizeof(EventPacket_type), 0, &a->eventQ) < 0)
		goto Error;
	if (CmtNewLock(NULL, 0, &a->eventQThreadLock) < 0)
		goto Error;
	
	a -> dataQs				= ListCreate(sizeof(VChanCallbackData_type*));	
	if (!a->dataQs) goto Error;
		
	a -> eventQThreadID		= CmtGetCurrentThreadID ();
	a -> threadFunctionID	= 0;
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(a->eventQ, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskEventItemsInQueue, a, a -> eventQThreadID, NULL); 
	
	a -> subtasks			= ListCreate(sizeof(SubTask_type));
	if (!a->subtasks) goto Error;
	
	a -> slaveHWTrigTasks	= ListCreate(sizeof(SlaveHWTrigTask_type));
	if (!a->slaveHWTrigTasks) goto Error;
	
	a -> taskName 				= StrDup(taskControllerName);
	a -> moduleData				= moduleData;
	a -> subtaskIdx				= 0;
	a -> slaveHWTrigIdx			= 0;
	a -> state					= TASK_STATE_UNCONFIGURED;
	a -> repeat					= 1;
	a -> iterTimeout			= 0;								
	a -> iterMode				= TASK_ITERATE_BEFORE_SUBTASKS_START;
	a -> mode					= TASK_FINITE;
	a -> currIterIdx			= 0;
	a -> parenttask				= NULL;
	a -> masterHWTrigTask		= NULL;
	a -> logPtr					= NULL;
	a -> errorMsg				= NULL;
	a -> waitBetweenIterations	= 0;
	a -> abortFlag				= 0;
	a -> abortIterationFlag		= FALSE;
	a -> slaveArmedFlag			= FALSE;
	a -> nIterationsFlag		= -1;
	a -> iterationTimerID		= 0;
	a -> UITCFlag				= FALSE;
	
	// task controller function pointers
	a -> ConfigureFptr 			= ConfigureFptr;
	a -> UnconfigureFptr		= UnconfigureFptr;
	a -> IterateFptr			= IterateFptr;
	a -> AbortIterationFptr		= AbortIterationFptr;
	a -> StartFptr				= StartFptr;
	a -> ResetFptr				= ResetFptr;
	a -> DoneFptr				= DoneFptr;
	a -> StoppedFptr			= StoppedFptr;
	a -> DimUIFptr				= DimUIFptr;
	a -> SetUITCModeFptr		= SetUITCModeFptr;
	a -> ModuleEventFptr		= ModuleEventFptr;
	a -> ErrorFptr				= ErrorFptr;
	      
	
	return a;
	
	Error:
	
	if (a->eventQ) 				CmtDiscardTSQ(a->eventQ);
	if (a->eventQThreadLock)	CmtDiscardLock(a->eventQThreadLock);
	if (a->dataQs)	 			ListDispose(a->dataQs);
	if (a->subtasks)    		ListDispose(a->subtasks);
	if (a->slaveHWTrigTasks)	ListDispose(a->slaveHWTrigTasks);
	OKfree(a);
	
	return NULL;
}

/// HIFN Discards recursively a Task controller.
void discard_TaskControl_type(TaskControl_type** taskController)
{
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
	
	// event queue thread lock
	CmtDiscardLock((*taskController)->eventQThreadLock); 
	
	// incoming data queues (does not free the queue itself!)
	RemoveAllSinkVChans(*taskController);
	
	// error message storage 
	discard_ErrorMsg_type(&(*taskController)->errorMsg);
	
	// child Task Controllers list
	ListDispose((*taskController)->subtasks);
	
	// free Task Controller memory
	OKfree(*taskController);
	/*
	// discard all subtasks recursively
	SubTask_type* subtaskPtr;
	nItems = ListNumItems((*a)->subtasks);
	for (size_t i = 1; i <= nItems; i++) {
		subtaskPtr = ListGetPtrToItem((*a)->subtasks, i);
		discard_TaskControl_type(&subtaskPtr->subtask);
	} */
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

void SetTaskControlLog (TaskControl_type* taskControl, TaskExecutionLog_type* logPtr)
{
	taskControl->logPtr = logPtr;
}
  

HWTrigger_type GetTaskControlHWTrigger (TaskControl_type* taskControl)
{
	if (!taskControl->masterHWTrigTask && ListNumItems(taskControl->slaveHWTrigTasks ))
		return TASK_MASTER_HWTRIGGER;
	else
		if (taskControl->masterHWTrigTask && !ListNumItems(taskControl->slaveHWTrigTasks ))
			return TASK_SLAVE_HWTRIGGER;
		else
			return TASK_NO_HWTRIGGER; 
}
  

TaskControl_type* GetTaskControlParent (TaskControl_type* taskControl)
{
	return taskControl->parenttask;
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

TaskControl_type* GetTaskControlMasterHWTrigTask (TaskControl_type* taskControl)
{
	return taskControl->masterHWTrigTask;
}

TaskControl_type* GetSlaveHWTrigTask (SlaveHWTrigTask_type* tasktype)
{
	return tasktype->slaveHWTrigTask;
}

ListType GetTaskControlSlaveHWTrigTasks (TaskControl_type* taskControl)
{
	size_t	nSlaves = ListNumItems(taskControl->slaveHWTrigTasks);
	
	ListType Slaves = ListCreate(sizeof(SlaveHWTrigTask_type*));
	if (!Slaves) return 0;
	
	SlaveHWTrigTask_type*	slavetrigtaskPtr;
	for (size_t i = 1; i <= nSlaves; i++) {
		slavetrigtaskPtr = ListGetPtrToItem (taskControl->slaveHWTrigTasks, i);
		ListInsertItem(Slaves, &slavetrigtaskPtr, END_OF_LIST);
	}
	
	return Slaves;
}

 


//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller data queue and data exchange functions
//------------------------------------------------------------------------------------------------------------------------------------------------------
int	AddSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr, TaskVChanFuncAssign_type VChanFunc)
{
	// check if Sink VChan is already assigned to the Task Controller
	VChanCallbackData_type**	VChanTSQDataPtrPtr;
	size_t 						nItems = ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtrPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if ((*VChanTSQDataPtrPtr)->sinkVChan == sinkVChan) return -2;
	}
	
	CmtTSQHandle				tsqID 				= GetSinkVChanTSQHndl(sinkVChan);
	VChanCallbackData_type*		VChanTSQDataPtr		= init_VChanCallbackData_type(taskControl, sinkVChan, DataReceivedFptr);
	
	if (!VChanTSQDataPtr) return -1;
	if (!ListInsertItem(taskControl->dataQs, &VChanTSQDataPtr, END_OF_LIST)) 
		return -1;
	
	// add callback
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(tsqID, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskDataItemsInQueue, VChanTSQDataPtr, taskControl -> eventQThreadID, &VChanTSQDataPtr->itemsInQueueCBID); 
	
	return 0;
}

int	RemoveSinkVChan (TaskControl_type* taskControl, SinkVChan_type* sinkVChan)
{
	VChanCallbackData_type**	VChanTSQDataPtrPtr;
	size_t						nDataQs				= ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nDataQs; i++) {
		VChanTSQDataPtrPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if ((*VChanTSQDataPtrPtr)->sinkVChan == sinkVChan) {
			// remove queue Task Controller callback
			CmtUninstallTSQCallback(GetSinkVChanTSQHndl((*VChanTSQDataPtrPtr)->sinkVChan), (*VChanTSQDataPtrPtr)->itemsInQueueCBID);
			// free memory for queue item
			discard_VChanCallbackData_type(VChanTSQDataPtrPtr);
			// and remove from queue
			ListRemoveItem(taskControl->dataQs, 0, i);
			return 0; 	// Sink VChan found and removed
		}
	}
	
	return -1;			// Sink VChan not found
}

void RemoveAllSinkVChans (TaskControl_type* taskControl)
{
	VChanCallbackData_type** 	VChanTSQDataPtrPtr;
	size_t 						nItems = ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtrPtr = ListGetPtrToItem(taskControl->dataQs, i);
		// remove queue Task Controller callback
		CmtUninstallTSQCallback(GetSinkVChanTSQHndl((*VChanTSQDataPtrPtr)->sinkVChan), (*VChanTSQDataPtrPtr)->itemsInQueueCBID);
		// free memory for queue item
		discard_VChanCallbackData_type(VChanTSQDataPtrPtr);
	}
	ListClear(taskControl->dataQs);
}

void DisconnectAllSinkVChans (TaskControl_type* taskControl)
{
	VChanCallbackData_type** 	VChanTSQDataPtrPtr;
	size_t 						nItems = ListNumItems(taskControl->dataQs);
	for (size_t i = 1; i <= nItems; i++) {
		VChanTSQDataPtrPtr = ListGetPtrToItem(taskControl->dataQs, i);
		// disconnect Sink from Source
		VChan_Disconnect((VChan_type*)(*VChanTSQDataPtrPtr)->sinkVChan);
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller logging functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

TaskExecutionLog_type* init_TaskExecutionLog_type (int logBoxPanHandle, int logBoxControl)
{
	TaskExecutionLog_type* a = malloc(sizeof(TaskExecutionLog_type));
	if (!log) return NULL;
	
	a -> logBoxPanHandle 	= logBoxPanHandle;
	a -> logBoxControl		= logBoxControl;
	// create log list
	a -> log = ListCreate(sizeof(TaskControlLog_type));
	if (!a->log) {free(a); return NULL;}
	
	// create a thread lock
	if (CmtNewLock(NULL, 0, &a->lock) < 0)
	{ListDispose(a->log); free(a); return NULL;}
	
	return a;
}

void discard_TaskExecutionLog_type (TaskExecutionLog_type** a)
{
	if (!a) return;
	if (!*a) return;
	
	size_t 					nLogItems = ListNumItems((*a)->log);
	TaskControlLog_type*	logptr;
	
	for (size_t i = 1; i <= nLogItems; i++) {
		logptr = ListGetPtrToItem((*a)->log, i);
		ListDispose(logptr->subtasks);
	}
	
	ListDispose((*a)->log);
	CmtDiscardLock((*a)->lock);
	
	OKfree(*a);
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

static SlaveHWTrigTaskEventInfo_type* init_SlaveHWTrigTaskEventInfo_type (size_t slaveHWTrigIdx)
{
	SlaveHWTrigTaskEventInfo_type* a = malloc(sizeof(SlaveHWTrigTaskEventInfo_type));
	if (!a) return NULL;
	
	a -> slaveHWTrigIdx 		= slaveHWTrigIdx;
	
	return a;
}

static void	discard_SlaveHWTrigTaskEventInfo_type (SlaveHWTrigTaskEventInfo_type** a)
{
	if (!*a) return;
	OKfree(*a);
}

static void	disposeSlaveHWTrigTaskEventInfo	(void* eventInfo)
{
	SlaveHWTrigTaskEventInfo_type* slaveInfo = eventInfo;
	
	discard_SlaveHWTrigTaskEventInfo_type (&slaveInfo);
}

static BOOL	MasterHWTrigTaskHasChildSlaves	(TaskControl_type* taskControl)
{
	SubTask_type*   subtaskPtr; 
	BOOL 			flag			= 0;	// assume all Slave HW Triggered Task Controllers are not a SubTask of taskControl
	size_t			nSubTasks		= ListNumItems(taskControl->subtasks);
	for (size_t i = 1; i <= nSubTasks; i++) {
		subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		if (subtaskPtr->subtask->masterHWTrigTask == taskControl) {
			flag = 1;
			break;
		}
	}
	
	return flag;
}

static BOOL	SlaveHWTrigTaskHasParentMaster	(TaskControl_type* taskControl)
{
	return taskControl->parenttask == taskControl->masterHWTrigTask;
}

static BOOL	HWTrigSlavesAreArmed (TaskControl_type* master)
{
	BOOL 					ArmedFlag			= 1;	// assume all Slave HW Triggered Task Controllers are armed
	SlaveHWTrigTask_type*   slaveHWTrigPtr; 
	size_t					nSlaves				= ListNumItems(master->slaveHWTrigTasks);
	for (size_t i = 1; i <= nSlaves; i++) {
		slaveHWTrigPtr = ListGetPtrToItem(master->slaveHWTrigTasks, i);
		if (!slaveHWTrigPtr->armed) {
			ArmedFlag = 0;
			break;
		}
	}
	
	return ArmedFlag; 
}

static void	ResetHWTrigSlavesArmedStatus (TaskControl_type* master)
{
	SlaveHWTrigTask_type*   slaveHWTrigPtr; 
	size_t					nSlaves				= ListNumItems(master->slaveHWTrigTasks);
	for (size_t i = 1; i <= nSlaves; i++) {
		slaveHWTrigPtr = ListGetPtrToItem(master->slaveHWTrigTasks, i);
		slaveHWTrigPtr->armed = FALSE;
	}
}


static void	disposeCmtTSQVChanEventInfo (void* eventInfo)
{
	if (!eventInfo) return;
	free(eventInfo);
}

static void	DimTaskTreeBranch (TaskControl_type* taskControl, TaskEvents_type event, BOOL dimmed)
{
	if (!taskControl) return;
	
	// dim/undim
	FunctionCall(taskControl, event, TASK_FCALL_DIM_UI, &dimmed); 
	
	size_t			nSubTasks = ListNumItems(taskControl->subtasks);
	SubTask_type*	subTaskPtr;
	
	for (size_t i = nSubTasks; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		DimTaskTreeBranch (subTaskPtr->subtask, event, dimmed);
	}
}

void ClearAllSinkVChans	(TaskControl_type* taskControl)
{
	size_t						nVChans		= ListNumItems(taskControl->dataQs);
	VChanCallbackData_type**	VChanCBDataPtr;
	
	for (size_t i = nVChans; i; i--) {
		VChanCBDataPtr = ListGetPtrToItem(taskControl->dataQs, i);
		ReleaseAllDataPackets((*VChanCBDataPtr)->sinkVChan);
	}
}

static void	ClearTaskTreeBranchVChans (TaskControl_type* taskControl)
{
	if (!taskControl) return;
	
	ClearAllSinkVChans(taskControl);
	
	size_t			nSubTasks	 = ListNumItems(taskControl->subtasks);
	SubTask_type*	subTaskPtr;
	
	for (size_t i = nSubTasks; i; i--) {
		subTaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		ClearTaskTreeBranchVChans(subTaskPtr->subtask);
	}
}

static VChanCallbackData_type*	init_VChanCallbackData_type	(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr)
{
	VChanCallbackData_type* VChanCB= malloc(sizeof(VChanCallbackData_type));
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

static ErrorMsg_type* init_ErrorMsg_type (int errorID, const char errorOrigin[], const char errorDescription[])
{
	size_t	nchars;
	
	ErrorMsg_type* a = malloc (sizeof(ErrorMsg_type));
	if (!a) return NULL;
	
	a -> errorID = errorID;
	
	nchars 			= snprintf(a->errorInfo, 0, "<%s Error ID: %d, Reason:\n\t %s>", errorOrigin, errorID, errorDescription); 
	a->errorInfo 	= malloc((nchars+1) * sizeof(char));
	if (!a->errorInfo) {free(a); return NULL;}
	snprintf(a->errorInfo, nchars+1, "<%s Error ID: %d, Reason:\n\t %s>", errorOrigin, errorID, errorDescription); 
	
	return a;
}

static void discard_ErrorMsg_type (ErrorMsg_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->errorInfo);
	OKfree(*a);
}

static ErrorMsg_type* FCallReturnToErrorMsg	(FCallReturn_type** fCallReturn, TaskFCall_type fID)
{
	size_t	nchars;
	
	if (!*fCallReturn) return NULL;
	if ((*fCallReturn)->retVal >= 0) return NULL;		// no error
	
	ErrorMsg_type* a = malloc(sizeof(ErrorMsg_type));
	if (!a) return NULL;
	
	a -> errorID 	= (*fCallReturn)->retVal;
	
	nchars = snprintf(a -> errorInfo, 0, "<%s Error ID: %d, Reason:\n\t %s>", 
					  FCallToString(fID), (*fCallReturn)->retVal, (*fCallReturn)->errorInfo); 
	a -> errorInfo	= malloc ((nchars+1)*sizeof(char));
	if (!a->errorInfo) {free(a); return NULL;}
	snprintf(a -> errorInfo, nchars+1, "<%s Error ID: %d, Reason:\n\t %s>", 
					  FCallToString(fID), (*fCallReturn)->retVal, (*fCallReturn)->errorInfo); 
	
	discard_FCallReturn_type(fCallReturn);
	
	return a;
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
			
		case TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES:
			
			return StrDup("Running and Waiting For HW Trig Slaves");
			
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
	}
	
	return StrDup("?");
	
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
			
		case TASK_EVENT_HWTRIG_SLAVE_ARMED:
			
			return StrDup("Slave HW Triggered Task Controller is Armed"); 
			
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

static char* ExecutionLogEntry (TaskControlLog_type* logItem)
{
	SubTask_type*	subtaskPtr;
	char* 			output = StrDup("");
	char*			eventName;
	char*			stateName;
	char*			fcallName;
	char    		buf[50];
	
	// Task Controller Name
	if (logItem->taskControl->taskName)
		AppendString(&output, logItem->taskControl->taskName, -1);
	else
		AppendString(&output, "No name", -1);
		
	
	// Timestamp
	AppendString(&output, ",  ", -1); 
	Fmt(buf, "%s<%f[e3]", logItem->timestamp);
	AppendString(&output, buf, -1);
	
	// Event Name
	AppendString(&output, ",  ", -1);
	eventName = EventToString(logItem->event);
	AppendString(&output, eventName, -1);
	OKfree(eventName);
	
	// SubTask Controller States
	AppendString(&output, ",  {", -1);
	size_t	nSubTasks	= ListNumItems(logItem->subtasks);
	for (size_t i = 1; i <= nSubTasks; i++) {
		subtaskPtr = ListGetPtrToItem(logItem->subtasks, i);
		AppendString(&output, "(", -1);
		// Task Controller Name
		if (subtaskPtr->subtask->taskName)
			AppendString(&output, subtaskPtr->subtask->taskName, -1);
		else
			AppendString(&output, "No name", -1);
		
		AppendString(&output, ", ", -1);
		stateName = StateToString(subtaskPtr->subtaskState);
		AppendString(&output, stateName, -1);
		OKfree(stateName);
		AppendString(&output, ")", -1);
		if (i < nSubTasks)
			AppendString(&output, ", ", -1);
	}
	
	// Iteration index
	AppendString(&output, "},  ", -1);
	Fmt(buf, "%s<iter %d", logItem->IterIdx);
	AppendString(&output, buf, -1);
	
	// Action
	AppendString(&output, ",  ", -1);
	switch (logItem->action) {
		
		case TASK_CONTROL_STATE_CHANGE:
			
			AppendString(&output, "state change ", -1);
			stateName = StateToString(logItem->oldState);
			AppendString(&output, stateName, -1);
			OKfree(stateName);
			AppendString(&output, "->", -1);
			stateName = StateToString(logItem->newState);
			AppendString(&output, stateName, -1);
			OKfree(stateName);
			break;
		
		case TASK_CONTROL_FUNCTION_CALL:
			
			fcallName = FCallToString(logItem->fcallID);
			AppendString(&output, fcallName, -1);
			break;
	}
	
	AppendString(&output, "\n", -1);
	return output;
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
	VChanCallbackData_type*		VChanTSQData		= callbackData;
	VChanCallbackData_type**	VChanTSQDataPtr		= malloc(sizeof(SinkVChan_type*));
	if (!VChanTSQDataPtr) {
		// flush queue
		CmtFlushTSQ(GetSinkVChanTSQHndl(VChanTSQData->sinkVChan), TSQ_FLUSH_ALL, NULL);
		VChanTSQData->taskControl->errorMsg = init_ErrorMsg_type(-1, VChanTSQData->taskControl->taskName, "Out of memory");
		FunctionCall(VChanTSQData->taskControl, TASK_EVENT_DATA_RECEIVED, TASK_FCALL_ERROR, NULL);
		ChangeState(VChanTSQData->taskControl, TASK_EVENT_DATA_RECEIVED, TASK_STATE_ERROR); 
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


/*  doesn't seem to be neccessary anymore since checking for more events is done in TaskEventHandler   
void CVICALLBACK TaskEventHandlerExecutionCallback (CmtThreadPoolHandle poolHandle, CmtThreadFunctionID functionID, 
													unsigned int event, int value, void *callbackData)
{
	// assigned to main thread which the same as the thread in which the Task Controller
	// state machine was created
	
	TaskControl_type* 	taskControl = callbackData;
	size_t				nItems;
	
	switch (event) {
		
		case EVENT_TP_THREAD_FUNCTION_BEGIN:
			
			break;
			
		case EVENT_TP_THREAD_FUNCTION_END:
			
			// Restart manually this function if there are more event items in the queue.
			// If there are no more items, execution continues processing other events
			CmtGetTSQAttribute(taskControl->eventQ, ATTR_TSQ_ITEMS_IN_QUEUE, &nItems);
			if (nItems)
				TaskEventItemsInQueue(taskControl->eventQ, EVENT_TSQ_ITEMS_IN_QUEUE, 0, taskControl);
			
			break;
	}
	
}
*/

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
		return TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, init_ErrorMsg_type(errorID, "External Task Control Iteration", errorInfo), disposeTCIterDoneInfo);
	else {
		if (doAnotherIteration) taskControl->repeat++;
		return TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL);
	}
}

static void disposeTCIterDoneInfo (void* eventInfo) 
{
	discard_ErrorMsg_type((ErrorMsg_type**)&eventInfo);
}

static int ChangeState (TaskControl_type* taskControl, TaskEvents_type event, TaskStates_type newState)
{
	int error;
	// if logging enabled, record state change
	if (taskControl->logPtr) {
		TaskControlLog_type logItem = {taskControl, 
									   Timer(),
									   event,
									   ListCopy(taskControl->subtasks),
									   taskControl->currIterIdx,
									   TASK_CONTROL_STATE_CHANGE,
									   taskControl->state,
									   newState,
									   TASK_FCALL_NONE};
		
		CmtGetLock(taskControl->logPtr->lock);
		
			ListInsertItem(taskControl->logPtr->log, &logItem, END_OF_LIST);
			if (taskControl->logPtr->logBoxPanHandle && taskControl->logPtr->logBoxControl) {
				char* logEntry	= ExecutionLogEntry(&logItem);
				SetCtrlVal(taskControl->logPtr->logBoxPanHandle, taskControl->logPtr->logBoxControl, logEntry);
				OKfree(logEntry);
			}
		
		CmtReleaseLock(taskControl->logPtr->lock);
	}
	
	taskControl->state = newState;
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
	
	(*taskControl->IterateFptr)(taskControl, taskControl->currIterIdx, &taskControl->abortIterationFlag);
	
	return 0;
}




static ErrorMsg_type* FunctionCall (TaskControl_type* taskControl, TaskEvents_type event, TaskFCall_type fID, void* fCallData)
{
#define FunctionCall_Error_OutOfMemory			-1
#define FunctionCall_Error_Invalid_fID 			-2
#define FunctionCall_Error_FCall_Timeout 		-3
#define FunctionCall_Error_FCall_Error			-4

	// if logging enabled, record function call action
	if (taskControl->logPtr) {
		TaskControlLog_type logItem = {taskControl, 
									   Timer(),
									   event,
									   ListCopy(taskControl->subtasks),
									   taskControl->currIterIdx,
									   TASK_CONTROL_FUNCTION_CALL,
									   taskControl->state,
									   taskControl->state,
									   fID};
		
		CmtGetLock(taskControl->logPtr->lock);
		
			ListInsertItem(taskControl->logPtr->log, &logItem, END_OF_LIST);
			if (taskControl->logPtr->logBoxPanHandle && taskControl->logPtr->logBoxControl) {
				char* logEntry	= ExecutionLogEntry(&logItem);
				SetCtrlVal(taskControl->logPtr->logBoxPanHandle, taskControl->logPtr->logBoxControl, logEntry);
				OKfree(logEntry);
			}
			
		CmtReleaseLock(taskControl->logPtr->lock);
	}
	
	// call function
	FCallReturn_type* 	fCallResult 			= NULL;
	BOOL				functionMissingFlag		= 0;		// function pointer not provided
	
	switch (fID) {
		
		case TASK_FCALL_NONE:
				
			return init_ErrorMsg_type(FunctionCall_Error_Invalid_fID, 
				"FunctionCall", "Not a valid function ID"); // not a valid fID, error
			
		case TASK_FCALL_CONFIGURE:
			
			if (taskControl->ConfigureFptr) fCallResult = (*taskControl->ConfigureFptr)(taskControl, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_UNCONFIGURE:
			
			if (taskControl->UnconfigureFptr) fCallResult = (*taskControl->UnconfigureFptr)(taskControl, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_ITERATE:
			
			if (!taskControl->IterateFptr) { 
				functionMissingFlag = 1;
				TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL);
				break; // stop here
			}
			
			// reset abort iteration flag
			taskControl->abortIterationFlag = FALSE;
			
			// reset Slave armed flag
			taskControl->slaveArmedFlag		= FALSE;
			
			// call iteration function and set timeout if required to complete the iteration
			if (taskControl->iterTimeout) {  
				// set an iteration timeout async timer until which a TASK_EVENT_ITERATION_DONE must be received 
				// if timeout elapses without receiving a TASK_EVENT_ITERATION_DONE, a TASK_EVENT_ITERATION_TIMEOUT is generated 
				taskControl->iterationTimerID = NewAsyncTimer(taskControl->iterTimeout, 1, 1, TaskControlIterTimeout, taskControl);
			}
			
			// launch provided iteration function pointer in a separate thread
			CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, ScheduleIterateFunction, taskControl, NULL);
			
			break;
			
		case TASK_FCALL_ABORT_ITERATION:
			
			if (taskControl->AbortIterationFptr) (*taskControl->AbortIterationFptr)(taskControl, taskControl->currIterIdx, &taskControl->abortFlag);
			break;
			
		case TASK_FCALL_START:
			
			if (taskControl->StartFptr) fCallResult = (*taskControl->StartFptr)(taskControl, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
				
		case TASK_FCALL_RESET:
		
			if (taskControl->ResetFptr) fCallResult = (*taskControl->ResetFptr)(taskControl, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_DONE:
			
			if (taskControl->DoneFptr) fCallResult = (*taskControl->DoneFptr)(taskControl, taskControl->currIterIdx, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_STOPPED:
			
			if (taskControl->StoppedFptr) fCallResult = (*taskControl->StoppedFptr)(taskControl, taskControl->currIterIdx, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_DIM_UI:
			
			if (taskControl->DimUIFptr) (*taskControl->DimUIFptr)(taskControl, *(BOOL*)fCallData); 
			break;
			
		case TASK_FCALL_SET_UITC_MODE:
			
			if (taskControl->SetUITCModeFptr) (*taskControl->SetUITCModeFptr)(taskControl, *(BOOL*)fCallData);
			break;
			
		case TASK_FCALL_DATA_RECEIVED:
			
			// call data received callback if one was provided
			if ((*(VChanCallbackData_type**)fCallData)->DataReceivedFptr) fCallResult = (*(*(VChanCallbackData_type**)fCallData)->DataReceivedFptr)(taskControl, taskControl->state, (*(VChanCallbackData_type**)fCallData)->sinkVChan, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_MODULE_EVENT:
			
			if (taskControl->ModuleEventFptr) fCallResult = (*taskControl->ModuleEventFptr)(taskControl, taskControl->state, taskControl->currIterIdx, fCallData, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_ERROR:
			
			// undim Task tree if this is a Root Task Controller
			if (!taskControl->parenttask)
				DimTaskTreeBranch (taskControl, event, FALSE);

			// call ErrorFptr
			if (taskControl->ErrorFptr) (*taskControl->ErrorFptr)(taskControl, taskControl->errorMsg->errorInfo);
			break;
				
	}
	
	
	if (!fCallResult && (fID != TASK_FCALL_ITERATE) && (fID != TASK_FCALL_ABORT_ITERATION) && (fID != TASK_FCALL_DIM_UI) && (fID != TASK_FCALL_SET_UITC_MODE) && (fID != TASK_FCALL_ERROR)) 
		if (functionMissingFlag)
			return NULL;																				// function not provided
		else
			return init_ErrorMsg_type(FunctionCall_Error_OutOfMemory, "FunctionCall", "Function return value missing or out of memory");
	
	// in case of error, return error message otherwise return NULL
	return FCallReturnToErrorMsg(&fCallResult, fID);
}

int	TaskControlEventToSubTasks  (TaskControl_type* SenderTaskControl, TaskEvents_type event, void* eventInfo,
								 DisposeEventInfoFptr_type disposeEventInfoFptr)
{
	SubTask_type* 	subtaskPtr;
	// dispatch event to all subtasks
	size_t	nSubTasks = ListNumItems(SenderTaskControl->subtasks);
	for (size_t i = 1; i <= nSubTasks; i++) { 
		subtaskPtr = ListGetPtrToItem(SenderTaskControl->subtasks, i);
		if (TaskControlEvent(subtaskPtr->subtask, event, eventInfo, disposeEventInfoFptr) < 0) return -1;
	}
	
	return 0;
}

int	AddSubTaskToParent (TaskControl_type* parent, TaskControl_type* child)
{
	SubTask_type	subtaskItem;
	if (!parent || !child) return -1;
	
	subtaskItem.subtask			= child;
	subtaskItem.subtaskState	= child->state;
	
	// call UITC Active function to dim/undim UITC Task Control execution
	BOOL	UITCFlag = FALSE;
	if (child->UITCFlag)
		FunctionCall(child, TASK_EVENT_SUBTASK_ADDED_TO_PARENT, TASK_FCALL_SET_UITC_MODE, &UITCFlag);
	
	// insert subtask
	if (!ListInsertItem(parent->subtasks, &subtaskItem, END_OF_LIST)) return -1;

	// add parent pointer to subtask
	child->parenttask = parent;
	// update subtask index within parent list
	child->subtaskIdx = ListNumItems(parent->subtasks);  
	
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
			if (child->UITCFlag)
				FunctionCall(child, TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT, TASK_FCALL_SET_UITC_MODE, &UITCFlag);
			
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

int	AddHWSlaveTrigToMaster (TaskControl_type* master, TaskControl_type* slave)
{
	SlaveHWTrigTask_type newSlave;
	if (!master || !slave) return -1; 
	
	if (slave->masterHWTrigTask) 
		return -2; // slave already assigned
	
	if (ListNumItems(slave->slaveHWTrigTasks))
		return -3; // Task Controller is not of Slave HW Trig type
	
	if (master->masterHWTrigTask)
		return -4; // task Controller is not of a Master HW Trig type
	
	// assign master to slave
	slave->masterHWTrigTask = master;
	
	// assign slave to master
	newSlave.slaveHWTrigTask 	= slave;
	newSlave.armed				= FALSE;
	
	// insert subtask
	if (!ListInsertItem(master->slaveHWTrigTasks, &newSlave, END_OF_LIST)) return -1;
	
	// update Slave HW Trig index within Master HW Trig Slave list
	slave->slaveHWTrigIdx = ListNumItems(master->slaveHWTrigTasks);
	
	return 0;
}

int RemoveHWSlaveTrigFromMaster (TaskControl_type* slave)
{
	SlaveHWTrigTask_type* slavePtr; 
	
	if (!slave || !slave->masterHWTrigTask) return -1;
	
	size_t	nSlaves = ListNumItems(slave->masterHWTrigTask->slaveHWTrigTasks);
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem(slave->masterHWTrigTask->slaveHWTrigTasks, i);
		if (slave == slavePtr->slaveHWTrigTask) {
			ListRemoveItem(slave->masterHWTrigTask->slaveHWTrigTasks, 0, i);
			// update slave indices from master list
			nSlaves = ListNumItems(slave->masterHWTrigTask->slaveHWTrigTasks);
			for (size_t i = 1; i <= nSlaves; i++) {
				slavePtr = ListGetPtrToItem(slave->masterHWTrigTask->slaveHWTrigTasks, i);
				slavePtr->slaveHWTrigTask->slaveHWTrigIdx = i;
			}
			slave->slaveHWTrigIdx 	= 0;
			slave->masterHWTrigTask = NULL;
			
			return 0; // found and removed
		}
		
	}
	
	return -2; // not found
}

int RemoveAllHWSlaveTrigsFromMaster	(TaskControl_type* master)
{
	if (!master) return -1;
	SlaveHWTrigTask_type* 	slavePtr; 
	size_t					nSlaves = ListNumItems(master->slaveHWTrigTasks);
	
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem(master->slaveHWTrigTasks, i);
		slavePtr->slaveHWTrigTask->slaveHWTrigIdx 	= 0;
		slavePtr->slaveHWTrigTask->masterHWTrigTask	= NULL;
	}
	ListClear(master->slaveHWTrigTasks);
	
	return 0;
}

int	DisassembleTaskTreeBranch (TaskControl_type* taskControlNode)
{
	if (!taskControlNode) return -1;
	
	// disconnect from parent if any
	RemoveSubTaskFromParent(taskControlNode);
	
	// disconnect HW triggering
	RemoveHWSlaveTrigFromMaster(taskControlNode);
	RemoveAllHWSlaveTrigsFromMaster(taskControlNode);
	
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
#define TaskEventHandler_Error_HWTriggeringNotAllowed		-9
#define TaskEventHandler_Error_HWTriggeringMismatch			-10
#define TaskEventHandler_Error_HWTriggeringMinRepeat		-11	
#define TaskEventHandler_Error_NoHWTriggerArmedEvent		-12
#define	TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout  -13
#define TaskEventHandler_Error_HWTrigSlaveNotArmed			-14

	
	EventPacket_type 		eventpacket[EVENT_BUFFER_SIZE];
	SubTask_type* 			subtaskPtr; 
	SlaveHWTrigTask_type*   slaveHWTrigPtr;
	ErrorMsg_type*			errMsg			= NULL;
	char*					buff			= NULL;
	char*					eventStr;
	char*					stateStr;
	size_t					nchars;
	size_t					nItems;
	int						nEventItems;
	double					delay;
	char*					buf[100]; 
	
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
					
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_CONFIGURE, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					// send TASK_EVENT_CONFIGURE to all subtasks if there are any
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE posting to SubTasks failed"); 
							
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					} 
					
					// If there are no subtasks, then reset device/module and make transition here to 
					// INITIAL state and inform parent task if there is any
					if (!ListNumItems(taskControl->subtasks)) {
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						ResetHWTrigSlavesArmedStatus(taskControl);
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
						
					} else
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_CONFIGURED);
						
					break;
					
				case TASK_EVENT_STOP:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
			}
			
			break;
			
		case TASK_STATE_CONFIGURED:
		
			switch (eventpacket[i].event) { 
				
				case TASK_EVENT_CONFIGURE: 
					// Reconfigures Task Controller and all its SubTasks
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_STOP:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// unconfigure
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						ResetHWTrigSlavesArmedStatus(taskControl);
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
						 
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);	
			}
			
			break;
			
		case TASK_STATE_INITIAL:
		
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ITERATE_ONCE: 
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_START, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					// Iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					//-------------------------------------------------------------------------------------------------------------------------
					// If this is a Root Task Controller, i.e. it doesn't have a parent, then: 
					// - call dim UI function recursively for its SubTasks
					// - clear data packets from SubTask Sink VChans recursively
					//-------------------------------------------------------------------------------------------------------------------------
					
					if(!taskControl->parenttask) {
						ClearTaskTreeBranchVChans(taskControl);
						DimTaskTreeBranch(taskControl, eventpacket[i].event, TRUE);
					}
					
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state
					//---------------------------------------------------------------------------------------------------------------
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING);  
					
					break;
					
				case TASK_EVENT_STOP:  
					
					//ignore this command 
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured then switch to unconfigured state
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED)
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						} else {
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
							break;
						}
					
					break;
					
			
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
			}
			
			break;
			
		case TASK_STATE_IDLE:
	
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// call unconfigure function and switch to unconfigured state
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					} else {
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ITERATE_ONCE: 
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_START, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING);
					
					//-------------------------------------------------------------------------------------------------------------------------
					// If this is a Root Task Controller, i.e. it doesn't have a parent, then call dim UI function recursively for its SubTasks
					//---------------------------------------------------------------------------------------------------------------------
					
					if(taskControl->parenttask) break; // stop here
					DimTaskTreeBranch(taskControl, eventpacket[i].event, TRUE);
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					// ignore this command
						
					break;
					
				case TASK_EVENT_RESET:
					
					// send RESET event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_RESET, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_RESET posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					// change state to INITIAL if there are no subtasks and call reset function
					if (!ListNumItems(taskControl->subtasks)) {
						
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						ResetHWTrigSlavesArmedStatus(taskControl);
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
						
					}
					
					break;
					
				case TASK_EVENT_STOP:  
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_IDLE);  // just inform parent
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_IDLE);  // just inform parent
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured then switch to unconfigured state
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED)
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						} else {
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
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
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						ResetHWTrigSlavesArmedStatus(taskControl);
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
					
			}
			
			break;
			
		case TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES:
			
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_SUBTASK_STATE_CHANGED: 
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_HWTRIG_SLAVE_ARMED:
					
					// update Slave HW Trig Task Controller armed flag in master list
					slaveHWTrigPtr = ListGetPtrToItem(taskControl->slaveHWTrigTasks, ((SlaveHWTrigTaskEventInfo_type*)eventpacket[i].eventInfo)->slaveHWTrigIdx);
					slaveHWTrigPtr->armed = TRUE; 
					
					if (!HWTrigSlavesAreArmed(taskControl))
						break; // stop here, not all Slaves are armed 
					
									
					//---------------------------------------------------------------------------------------------------------------
					// Slaves are ready to be triggered, reset Slaves armed status
					//---------------------------------------------------------------------------------------------------------------
									
					ResetHWTrigSlavesArmedStatus(taskControl);
									
					//---------------------------------------------------------------------------------------------------------------
					// Iterate and fire Master HW Trigger 
					//---------------------------------------------------------------------------------------------------------------
									
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
					// switch state and wait for iteration to complete
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
							
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
							
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
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
					
					if ( taskControl->mode == TASK_FINITE  && (taskControl->currIterIdx >= taskControl->repeat  ||  !taskControl->nIterationsFlag)  && taskControl->currIterIdx ) {			
						//---------------------------------------------------------------------------------------------------------------- 	 
						// Task Controller is finite switch to DONE
						//---------------------------------------------------------------------------------------------------------------- 	
										
						// call done function
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DONE, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
							break;
						}
								
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);
						
						// undim Task Tree if this is a Root Task Controller
						if(!taskControl->parenttask) 
						DimTaskTreeBranch(taskControl, eventpacket[i].event, FALSE);
						
						break; // stop here
					}
						
					//---------------------------------------------------------------------------------------------------------------
					// If not first iteration, then wait given number of seconds before iterating    
					//---------------------------------------------------------------------------------------------------------------
					
					if (taskControl->currIterIdx && taskControl->nIterationsFlag < 0)
						SyncWait(Timer(), taskControl->waitBetweenIterations);
					
					//---------------------------------------------------------------------------------------------------------------
					// Iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					switch (taskControl->iterMode) {
						
						case TASK_ITERATE_BEFORE_SUBTASKS_START:
							
							switch (GetTaskControlHWTrigger(taskControl)) {
								
								case TASK_NO_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Call iteration function if needed	 
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
									
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
								        
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
											break;  // stop here
										}
										
									
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if HW triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if triggering is consistent. Do not allow child Task Controllers to be HW Triggered Slaves
									//---------------------------------------------------------------------------------------------------------------
									
									if (MasterHWTrigTaskHasChildSlaves	(taskControl)) {
										
										// Not allowed because the slaves cannot be put in an armed state waiting for the trigger. They don't receive a TASK_EVENT_START
										// until the iteration doesn't finish.
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringNotAllowed, taskControl->taskName, "A Master HW Trigger Task Control cannot have a Slave HW Triggered Task Control as a child SubTask which is armed after the Master sends its trigger");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}
										
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									ResetHWTrigSlavesArmedStatus(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// Iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if trigger settings are valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (SlaveHWTrigTaskHasParentMaster(taskControl) && (taskControl->repeat != 1 || taskControl->mode == TASK_CONTINUOUS)) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMismatch, taskControl->taskName, "If a Slave HW Triggered Task Controller has a Master HW Triggering Task Controller as its parent task, then the Slave HW Triggered Task Controller must have only one iteration");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break; // stop here
										
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION); 
									
									break;
							}
							
							break;
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
							
							//---------------------------------------------------------------------------------------------------------------
							// Start SubTasks if there are any
							//---------------------------------------------------------------------------------------------------------------
									
							if (ListNumItems(taskControl->subtasks)) {    
								// send START event to all subtasks
								if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
									taskControl->errorMsg =
									init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
								
								break; // stop here
							}	
							
							switch (GetTaskControlHWTrigger(taskControl)) {
								
								case TASK_NO_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// There are no SubTasks, call iteration function if needed
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
											break;  // stop here
										}
									
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if triggering is consistent. Do not allow child Task Controllers to be HW Triggered Slaves
									//---------------------------------------------------------------------------------------------------------------
									if (MasterHWTrigTaskHasChildSlaves	(taskControl)) {
										
										// Not allowed because the slaves cannot be put in an armed state waiting for the trigger. They don't receive a TASK_EVENT_START
										// until the iteration doesn't finish.
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringNotAllowed, taskControl->taskName, "A Master HW Trigger Task Control cannot have a Slave HW Triggered Task Control as a child SubTask that must terminate before it can be triggered");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}	
									
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here, Slaves are not ready
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									ResetHWTrigSlavesArmedStatus(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// There are no SubTasks, iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if iteration timeout setting is valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->iterTimeout) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout, taskControl->taskName, "A Slave HW Triggered Task Controller cannot have 0 timeout");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION); 
										
									break;
							}
							
							break;
							
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
							
							//---------------------------------------------------------------------------------------------------------------
							// Start SubTasks if there are any
							//---------------------------------------------------------------------------------------------------------------
									
							if (ListNumItems(taskControl->subtasks)) 
								// send START event to all subtasks
								if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
									taskControl->errorMsg =
									init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
							
							switch (GetTaskControlHWTrigger(taskControl)) {
								
								case TASK_NO_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Call iteration function if needed
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
											break;
										}
										
									// switch state and wait for iteration and SubTasks if there are any to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Make sure that when HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here, Slaves are not ready
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									ResetHWTrigSlavesArmedStatus(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// Iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
										
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Make sure that when HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if iteration timeout setting is valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->iterTimeout) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout, taskControl->taskName, "A Slave HW Triggered Task Controller cannot have 0 timeout");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION); 
									
									break;
							}
							
							break;
					}
					
					break;
					
				case TASK_EVENT_STOP:
				// Stops iterations and switches to IDLE or DONE states if there are no SubTask Controllers or to STOPPING state and waits for SubTasks to complete their iterations
					
					if (!ListNumItems(taskControl->subtasks)) {
						
						// undim Task Tree if this is a root Task Controller
							if(!taskControl->parenttask)
							DimTaskTreeBranch(taskControl, eventpacket[i].event, FALSE);
							
						// if there are no SubTask Controllers
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DONE, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);
							
						} else 
							// switch to IDLE or DONE state if finite task controller
							if (taskControl->currIterIdx < taskControl->repeat) {
								if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_STOPPED, NULL))) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_IDLE);
									
							} else {
								if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DONE, NULL))) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);
							}
							  
					} else {
						// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
						if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_STOPPING);
					}
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// send STOP command to self if continuous task controller and forward TASK_EVENT_STOP_CONTINUOUS_TASK to subtasks
					if (taskControl->mode == TASK_CONTINUOUS) 
						if (TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL) < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_STOP self posting failed"); 
								
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
							break;
						}
					
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
						
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					//---------------------------------------------------------------------------------------------------------------- 
					// If SubTasks are not yet complete, stay in RUNNING state and wait for their completion
					//---------------------------------------------------------------------------------------------------------------- 
					
					// consider only transitions to TASK_STATE_DONE 
					if (subtaskPtr->subtaskState != TASK_STATE_DONE)
						break; // stop here
					
					// assume all subtasks are done 
					BOOL AllDoneFlag = 1;
							
					// check SubTasks
					nItems = ListNumItems(taskControl->subtasks);
					for (size_t i = 1; i <= nItems; i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_DONE) {
							AllDoneFlag = 0;
							break;
						}
					}
					
					if (!AllDoneFlag) 
						break; // stop here
					
					//---------------------------------------------------------------------------------------------------------------- 
					// Decide on state transition
					//---------------------------------------------------------------------------------------------------------------- 
					switch (taskControl->iterMode) {
						
						case TASK_ITERATE_BEFORE_SUBTASKS_START:
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
							
							//---------------------------------------------------------------------------------------------------------------
							// Increment iteration index
							//---------------------------------------------------------------------------------------------------------------
							taskControl->currIterIdx++;
							if (taskControl->nIterationsFlag > 0)
								taskControl->nIterationsFlag--;
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
								FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								break;
								
							}
							
							// stay in TASK_STATE_RUNNING
							
							break;
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
						
							switch (GetTaskControlHWTrigger(taskControl)) {
								
								case TASK_NO_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// SubTasks are complete, call iteration function
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
											break;
										}
									
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION); 	
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if triggering is consistent. Do not allow child Task Controllers to be HW Triggered Slaves
									//---------------------------------------------------------------------------------------------------------------
									if (MasterHWTrigTaskHasChildSlaves	(taskControl)) {
										
										// Not allowed because the slaves cannot be put in an armed state waiting for the trigger. They don't receive a TASK_EVENT_START
										// until the iteration doesn't finish.
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringNotAllowed, taskControl->taskName, "A Master HW Trigger Task Control cannot have a Slave HW Triggered Task Control as a child SubTask that must terminate before it can be triggered");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break;
									}	
									
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here, Slaves are not ready
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									ResetHWTrigSlavesArmedStatus(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// There are no SubTasks, iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if iteration timeout setting is valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->iterTimeout) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout, taskControl->taskName, "A Slave HW Triggered Task Controller cannot have 0 timeout");
										
										FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING_WAITING_ITERATION); 
										
									break;
							}
							
							break;
					}
					
					break;
					
				case TASK_EVENT_HWTRIG_SLAVE_ARMED:
					
					// update Slave HW Trig Task Controller armed flag
					slaveHWTrigPtr = ListGetPtrToItem(taskControl->slaveHWTrigTasks, ((SlaveHWTrigTaskEventInfo_type*)eventpacket[i].eventInfo)->slaveHWTrigIdx);
					slaveHWTrigPtr->armed = TRUE; 
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
					
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
						ErrorMsg_type* errMsg = eventpacket[i].eventInfo;
						
						if (errMsg->errorID < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_IterateExternThread, taskControl->taskName, errMsg->errorInfo); 
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								
							break;   // stop here
						}
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Check if Task Controller is a Slave, that it has been armed before completing the iteration
					//---------------------------------------------------------------------------------------------------------------   
					if (GetTaskControlHWTrigger(taskControl) == TASK_SLAVE_HWTRIGGER && !taskControl->slaveArmedFlag) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_HWTrigSlaveNotArmed, taskControl->taskName, "A Slave HW Triggered Task Controller must be armed by TASK_EVENT_HWTRIG_SLAVE_ARMED before completing the call to its iteration function");
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------
					// If iteration was aborted , stop iterations and switch to IDLE or DONE state if there are no 
					// SubTask Controllers, otherwise switch to STOPPING state and wait for SubTasks to complete their iterations
					//---------------------------------------------------------------------------------------------------------------
					
					if (taskControl->abortIterationFlag) {
						
						if (!ListNumItems(taskControl->subtasks)) {
							// undim Task Tree if this is a root Task Controller
							if(!taskControl->parenttask)
							DimTaskTreeBranch(taskControl, eventpacket[i].event, FALSE);
								
						// if there are no SubTask Controllers
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DONE, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);
								
						} else 
							// switch to IDLE or DONE state if finite task controller
							if (taskControl->currIterIdx < taskControl->repeat) {
								if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_STOPPED, NULL))) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_IDLE);
									
							} else {
								if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DONE, NULL))) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);
							}
						
						} else {
							
							// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
							if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
								FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								break;
							}
						
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_STOPPING);
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
									
							if (ListNumItems(taskControl->subtasks)) {
								
								// send START event to all subtasks
								if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
									taskControl->errorMsg =
									init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
									FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
									break;
								}
										
								// switch to RUNNING state and wait for SubTasks to complete
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING);
								break; // stop here
										
							} 
							
							// If there are no SubTasks, fall through the case and ask for another iteration unless only one iteration was requested
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
							
							//---------------------------------------------------------------------------------------------------------------
							// Increment iteration index
							//---------------------------------------------------------------------------------------------------------------
							taskControl->currIterIdx++;
							if (taskControl->nIterationsFlag > 0)
								taskControl->nIterationsFlag--;
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
								FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								break;
								
							}
							
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING); 
							
							break;
									
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
							
							//---------------------------------------------------------------------------------------------------------------- 
							// If SubTasks are not yet complete, switch to RUNNING state and wait for their completion
							//---------------------------------------------------------------------------------------------------------------- 
							
							// assume all subtasks are done (even if there are none)
							BOOL AllDoneFlag = 1;
							
							// check SubTasks
							nItems = ListNumItems(taskControl->subtasks);
							for (size_t i = 1; i <= nItems; i++) {
								subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
								if (subtaskPtr->subtaskState != TASK_STATE_DONE) {
									AllDoneFlag = 0;
									break;
								}
							}
					
							if (!AllDoneFlag) {
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING);
								break;  // stop here
							}
							
							//---------------------------------------------------------------------------------------------------------------
							// Increment iteration index
							//---------------------------------------------------------------------------------------------------------------
							taskControl->currIterIdx++;
							if (taskControl->nIterationsFlag > 0)
								taskControl->nIterationsFlag--;
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
								FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
								break;
								
							}
							
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_RUNNING);
							
							break;
					}
					
					break;
					
				case TASK_EVENT_ITERATION_TIMEOUT:
					
					// reset timerID
					taskControl->iterationTimerID = 0;
					
					// generate timeout error
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_IterateFCallTmeout, taskControl->taskName, "Iteration function call timeout"); 
						
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_STOP:
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
				
					taskControl->abortIterationFlag = TRUE;
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ABORT_ITERATION, NULL);
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_HWTRIG_SLAVE_ARMED:
					
					switch (GetTaskControlHWTrigger(taskControl)) {  
						
						case TASK_NO_HWTRIGGER:
							
							// not allowed, generate error
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_NoHWTriggerArmedEvent, taskControl->taskName, "Slave Task Controller Armed event received but no HW triggering enabled for this Task Controller");
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							
							break;
							
						case TASK_MASTER_HWTRIGGER:
							
							// update Slave HW Trig Task Controller armed flag
							slaveHWTrigPtr = ListGetPtrToItem(taskControl->slaveHWTrigTasks, ((SlaveHWTrigTaskEventInfo_type*)eventpacket[i].eventInfo)->slaveHWTrigIdx);
							slaveHWTrigPtr->armed = TRUE; 
							
							break;
							
						case TASK_SLAVE_HWTRIGGER:
							
							// mark Slave as armed
							taskControl->slaveArmedFlag = TRUE;
							
							// inform Master Task Controller that Slave is armed
							TaskControlEvent(taskControl->masterHWTrigTask, TASK_EVENT_HWTRIG_SLAVE_ARMED, 
								init_SlaveHWTrigTaskEventInfo_type(taskControl->slaveHWTrigIdx), disposeSlaveHWTrigTaskEventInfo);
							
							break;
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							// remove timeout timer
							if (taskControl->iterationTimerID > 0) {
								DiscardAsyncTimer(taskControl->iterationTimerID);
								taskControl->iterationTimerID = 0;
							}
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
					
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
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					BOOL	IdleOrDoneFlag	= TRUE;		// assume all subtasks are either IDLE or DONE
					nItems = ListNumItems(taskControl->subtasks);
					
					for (size_t i = 1; i <= nItems; i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if ((subtaskPtr->subtaskState != TASK_STATE_IDLE) && (subtaskPtr->subtaskState != TASK_STATE_DONE)) {
							IdleOrDoneFlag = FALSE;
							break;
						}
					}
					
					// if all SubTasks are IDLE or DONE, switch to IDLE state
					if (IdleOrDoneFlag) {
						// undim Task Tree if this is a root Task Controller
						if(!taskControl->parenttask)
							DimTaskTreeBranch(taskControl, eventpacket[i].event, FALSE);
							
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_STOPPED, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
							
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_IDLE);
					}
						
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 	
			}
			
			break;
			
		case TASK_STATE_DONE:
		// This state can be reached only if all SubTask Controllers are in a DONE state
			
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// call unconfigure function and switch to unconfigured state
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					} else {
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
					
					// reset device/module
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
						
					// reset iterations
					taskControl->currIterIdx = 0;
					
					// reset armed status of Slave HW Triggered Task Controllers, if any
					ResetHWTrigSlavesArmedStatus(taskControl);
					
					// switch to INITIAL state
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
					
					// send START event to self
					if (TaskControlEvent(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_START self posting failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_RESET:
					
					// send RESET event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_RESET, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_RESET posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					// change state to INITIAL if there are no subtasks and call reset function
					if (!ListNumItems(taskControl->subtasks)) {
						
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						ResetHWTrigSlavesArmedStatus(taskControl);
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
						
					}
					
					break;
					
				case TASK_EVENT_STOP:  
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);  // just inform parent 
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_DONE);  // just inform parent
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured then switch to unconfigured state
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED)
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						} else {
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
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
						if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						ResetHWTrigSlavesArmedStatus(taskControl);
						
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 	
				
			}
			
			break;
			
		case TASK_STATE_ERROR:
			switch (eventpacket[i].event) {
				
				case TASK_EVENT_CONFIGURE:
					// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR); 
						break;
					}
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_UNCONFIGURE:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					// unconfigure
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_UNCONFIGURE, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, eventpacket[i].event, TASK_STATE_UNCONFIGURED);
					
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
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket[i].eventInfo)->newSubTaskState; 
					
					break;
					
			
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_DATA_RECEIVED, eventpacket[i].eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
							
						FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_MODULE_EVENT, eventpacket[i].eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket[i].event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket[i].event, TASK_STATE_ERROR);
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
