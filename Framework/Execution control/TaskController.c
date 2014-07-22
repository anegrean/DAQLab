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
#include <windows.h>
#include "asynctmr.h"
#include <formatio.h>
#include "TaskController.h"

//==============================================================================
// Constants

#define FCALL_THREAD_SLEEP 10.0							// put thread to sleep in [ms] until function calls return
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

//==============================================================================
// Types

typedef struct {
	size_t					subtaskIdx;					// For a TASK_EVENT_SUBTASK_STATE_CHANGED event this is a
														// 1-based index of subtask (from subtasks list) which generated
														// the event.
	TaskStates_type			newSubTaskState;			// For a TASK_EVENT_SUBTASK_STATE_CHANGED event this is the new state
														// of the subtask
} SubTaskEventInfo_type;

typedef struct {										// For a TASK_EVENT_HWTRIG_SLAVE_ARMED event
	size_t					slaveHWTrigIdx;				// 1-based index of Slave HW Triggered Task Controller from the Master HW Trigerring Slave list.
} SlaveHWTrigTaskEventInfo_type;

typedef struct {
	TaskEvents_type 			event;					// Task Control Event.
	void*						eventInfo;				// Extra information per event allocated dynamically
	DisposeEventInfoFptr_type   disposeEventInfoFptr;   // Function pointer to dispose of the eventInfo
	
} EventPacket_type;

typedef struct {
	TaskStates_type			subtaskState;				// Updated by parent task when informed by subtask that a state
														// change occured.
	TaskControl_type*		subtask;					// Pointer to Subtask Controller.
} SubTask_type;

typedef struct {
	BOOL					armed;						// TRUE if a Slave HW Triggered Task Controller is ready to be triggered by its Master
	TaskControl_type*		slaveHWTrigTask;			// pointer to Slave HW Triggered Task Controller.
} SlaveHWTrigTask_type;

typedef enum {
	TASK_CONTROL_STATE_CHANGE,
	TASK_CONTROL_FUNCTION_CALL
} Action_type;

typedef struct {
	int						errorID;
	char*					errorInfo;
} ErrorMsg_type;

typedef struct TaskControlLog {
	TaskControl_type*		taskControl;				// Pointer to Task Controller for which this record is made.
	double					timestamp;					// Timestamp at which the record was created.
	TaskEvents_type			event;						// Event that triggered the action
	ListType				subtasks;					// SubTask states when the event occured. List of SubTask_type
	size_t					IterIdx;					// Current iteration index when event occured.
	Action_type				action;						// Action type: state change or function call that the event triggered
	TaskStates_type			oldState;					// In case of state change: old state of Task Controller before a state transition.
	TaskStates_type			newState;					// In case of state change: new state of Task Controller after a state transition. 
	TaskFCall_type			fcallID;					// In case of function call: the ID of function which is called.
} TaskControlLog_type;

struct TaskExecutionLog {
	ListType				log;						// Keeps record of Task Controller events, state changes and actions. List of TaskControlLog_type.
	CmtThreadLockHandle		lock;						// Prevents multiple threads to add at the same time elements to the list
	int						logBoxPanHandle;
	int						logBoxControl;
};

struct TaskControl {
	// Task control data
	char*					taskName;					// Name of Task Controller
	size_t					subtaskIdx;					// 1-based index of subtask from parent Task subtasks list. If task doesn't have a parent task then index is 0.
	size_t					slaveHWTrigIdx;				// 1-based index of Slave HW Trig from Master HW Trig Slave list. If Task Controller is not a HW Trig Slave, then index is 0.
	CmtTSQHandle			eventQ;						// Event queue to which the state machine reacts.
	ListType				dataQs;						// Incoming data queues, list of CmtTSQHandle type.
	unsigned int			eventQThreadID;				// Thread ID in which queue events are processed.
	CmtThreadFunctionID		threadFunctionID;			// ID of ScheduleTaskEventHandler that is executed in a separate thread from the main thread.
	TaskStates_type 		state;						// Module execution state.
	size_t					repeat;						// Total number of repeats. If repeat is 0, then the iteration function is not called. 
	int						iterTimeout;				// 0, Task Controller iteration block is complete and execution continues with return from
														// the iteration function call. 1..N > 0, Task Controller iteration block is not yet complete
														// and TASK_EVENT_ITERATION_DONE must be received (from another thread) to consider the iteration block complete.
														// If TASK_EVENT_ITERATION_DONE is not received after iterTimeout seconds then the Task Controller execution will time out 
														// and genrate an error. For values -N..-1, Task Controller iteration block does not time out and waits for TASK_EVENT_ITERATION_DONE. 
														// to signal that iteration block is complete.
	TaskIterMode_type		iterMode;					// Determies how the iteration block of a Task Controller is executed with respect to its subtasks if any.
	TaskMode_type			mode;						// Finite or continuous type of task controller
	size_t					currIterIdx;    			// 1-based task execution iteration index  
	TaskControl_type*		parenttask;					// Pointer to parent task that own this subtask. 
														// If this is the main task, it has no parent and this is NULL. 
	ListType				subtasks;					// List of subtasks of SubTask_type.
	TaskControl_type*		masterHWTrigTask;			// If this is a Slave HW Triggered Task Controller, then masterHWTrigTask is a Task Controller 
	ListType				slaveHWTrigTasks;			// If this is a Master HW Triggered Task Controller then slaveHWTrigTasks is a list of SlaveHWTrigTask_type
														// HW Triggered Slave Task Controllers.
	void*					moduleData;					// Reference to module specific data that is 
														// controlled by the task.
	TaskExecutionLog_type*	logPtr;						// Pointer to logging structure. NULL if logging is not enabled.
	ErrorMsg_type*			errorMsg;					// When switching to an error state, additional error info is written here
	double					waitBetweenIterations;		// During a RUNNING state, waits specified ammount in seconds between iterations
	BOOL					abortFlag;					// When set to TRUE, it signals the provided function pointers that they must abort running processes.
	BOOL					abortIterationFlag;			// When set to TRUE, it signals the external thread running the iteration that it must finish.
	BOOL					slaveArmedFlag;				// TRUE when HW Triggering is enabled for the Task Controller and Slave has been armed before sending TASK_EVENT_ITERATION_DONE.
	int						nIterationsFlag;			// When -1, the Task Controller is iterated continuously, 0 iteration stops and 1 one iteration.
	int						iterationTimerID;			// Keeps track of the timeout timer when iteration is performed in another thread.
				
	// Event handler function pointers
	ConfigureFptr_type		ConfigureFptr;
	IterateFptr_type		IterateFptr;
	StartFptr_type			StartFptr;
	ResetFptr_type			ResetFptr;
	DoneFptr_type			DoneFptr;
	StoppedFptr_type		StoppedFptr;
	DataReceivedFptr_type	DataReceivedFptr;
	ModuleEventFptr_type	ModuleEventFptr;
	ErrorFptr_type			ErrorFptr;
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
static void						HWTrigSlavesArmedStatusReset	(TaskControl_type* master);

// Queue eventInfo functions
static void						disposeCmtTSQHandleEventInfo	(void* eventInfo);

//==============================================================================
// Global variables

int mainThreadID;	// Main thread ID where UI callbacks are processed

//==============================================================================
// Global functions

void 	CVICALLBACK TaskEventItemsInQueue 				(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

void 	CVICALLBACK TaskDataItemsInQueue 				(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

int 	CVICALLBACK ScheduleTaskEventHandler 			(void* functionData);

int 	CVICALLBACK ScheduleIterateFunction 			(void* functionData); 

void 	CVICALLBACK TaskEventHandlerExecutionCallback 	(CmtThreadPoolHandle poolHandle, CmtThreadFunctionID functionID, unsigned int event, int value, void *callbackData);

int 	CVICALLBACK TaskControlIterTimeout 				(int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2); 

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller creation / destruction functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

/// HIFN Initializes a Task controller.
TaskControl_type* init_TaskControl_type(const char				taskname[], 
										ConfigureFptr_type 		ConfigureFptr,
										IterateFptr_type		IterateFptr,
										StartFptr_type			StartFptr,
										ResetFptr_type			ResetFptr,
										DoneFptr_type			DoneFptr,
										StoppedFptr_type		StoppedFptr,
										DataReceivedFptr_type	DataReceivedFptr,
										ModuleEventFptr_type	ModuleEventFptr,
										ErrorFptr_type			ErrorFptr)
{
	TaskControl_type* a = malloc (sizeof(TaskControl_type));
	if (!a) return NULL;
	
	a -> eventQ				= 0;
	a -> dataQs				= 0;
	a -> subtasks			= 0;
	a -> slaveHWTrigTasks	= 0;
	
	if (CmtNewTSQ(N_TASK_EVENT_QITEMS, sizeof(EventPacket_type), 0, &a->eventQ) < 0)
		goto Error;
	
	a -> dataQs				= ListCreate(sizeof(CmtTSQHandle));	
	if (!a->dataQs) goto Error;
		
	a -> eventQThreadID		= CmtGetCurrentThreadID ();
	a -> threadFunctionID	= 0;
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(a->eventQ, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskEventItemsInQueue, a, a -> eventQThreadID, NULL); 
	
	a -> subtasks			= ListCreate(sizeof(SubTask_type));
	if (!a->subtasks) goto Error;
	
	a -> slaveHWTrigTasks	= ListCreate(sizeof(SlaveHWTrigTask_type));
	if (!a->slaveHWTrigTasks) goto Error;
	
	a -> taskName 				= StrDup(taskname);
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
	a -> moduleData				= NULL;
	a -> logPtr					= NULL;
	a -> errorMsg				= NULL;
	a -> waitBetweenIterations	= 0;
	a -> abortFlag				= 0;
	a -> abortIterationFlag		= 0;
	a -> slaveArmedFlag			= FALSE;
	a -> nIterationsFlag		= -1;
	a -> iterationTimerID		= 0;
	
	// task controller function pointers
	a -> ConfigureFptr 			= ConfigureFptr;
	a -> IterateFptr			= IterateFptr;
	a -> StartFptr				= StartFptr;
	a -> ResetFptr				= ResetFptr;
	a -> DoneFptr				= DoneFptr;
	a -> StoppedFptr			= StoppedFptr;
	a -> DataReceivedFptr		= DataReceivedFptr;
	a -> ModuleEventFptr		= ModuleEventFptr;
	a -> ErrorFptr				= ErrorFptr;
	
	return a;
	
	Error:
	
	if (a->eventQ) 				CmtDiscardTSQ(a->eventQ);
	if (a->dataQs)	 			ListDispose(a->dataQs);
	if (a->subtasks)    		ListDispose(a->subtasks);
	if (a->slaveHWTrigTasks)	ListDispose(a->slaveHWTrigTasks);
	OKfree(a);
	
	return NULL;
}

/// HIFN Discards recursively a Task controller.
void discard_TaskControl_type(TaskControl_type** a)
{
	CmtTSQHandle*	tsqPtr;
	if (!*a) return;
	
	OKfree((*a)->taskName);
	CmtDiscardTSQ((*a)->eventQ);
	for (size_t i = 1; i <= ListNumItems((*a)->dataQs); i++) {
		tsqPtr = ListGetPtrToItem((*a)->dataQs, i);
		CmtDiscardTSQ(*tsqPtr);
	}
	ListDispose((*a)->dataQs);
	
	discard_ErrorMsg_type(&(*a)->errorMsg);
	
	// discard all subtasks recursively
	SubTask_type* subtaskPtr;
	for (size_t i = 1; i <= ListNumItems((*a)->subtasks); i++) {
		subtaskPtr = ListGetPtrToItem((*a)->subtasks, i);
		discard_TaskControl_type(&subtaskPtr->subtask);
	}
	ListDispose((*a)->subtasks);
	
	// discard memory allocated for this structure
	OKfree(*a);
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

void SetTaskControlState (TaskControl_type* taskControl, TaskStates_type newState)
{
	taskControl->state = newState;
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

BOOL GetTaskControlAbortIterationFlag (TaskControl_type* taskControl)
{
	return taskControl->abortIterationFlag;
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

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller data queue and data exchange functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

int	AddDataQueue (TaskControl_type* taskControl, CmtTSQHandle dataQID)
{
	if (!dataQID) return -1;	// insert only non-zero QIDs
	
	if (!ListInsertItem(taskControl->dataQs, &dataQID, END_OF_LIST)) 
		return -1;
	
	// add callback
	// process items in queue events in the same thread that is used to initialize the task control (generally main thread)
	CmtInstallTSQCallback(dataQID, EVENT_TSQ_ITEMS_IN_QUEUE, 1, TaskDataItemsInQueue, taskControl, taskControl -> eventQThreadID, NULL); 
	
	return 0;
}

int	RemoveDataQueue (TaskControl_type* taskControl, CmtTSQHandle dataQID)
{
	CmtTSQHandle* QHndlPtr;
	for (size_t i = 1; i <= ListNumItems(taskControl->dataQs); i++) {
		QHndlPtr = ListGetPtrToItem(taskControl->dataQs, i);
		if (*QHndlPtr == dataQID) {
			CmtDiscardTSQ(*QHndlPtr);
			ListRemoveItem(taskControl->dataQs, 0, i);
			return 0; 	// queue found and removed
		}
	}
	
	return -1;			// queue not found
}

void RemoveAllDataQueues (TaskControl_type* taskControl)
{
	CmtTSQHandle* QHndlPtr;
	for (size_t i = 1; i <= ListNumItems(taskControl->dataQs); i++) {
		QHndlPtr = ListGetPtrToItem(taskControl->dataQs, i);
		CmtDiscardTSQ(*QHndlPtr);
		ListRemoveItem(taskControl->dataQs, 0, i);
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
	
	for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
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
	
	for (size_t i = 1; i <= ListNumItems(master->slaveHWTrigTasks); i++) {
		slaveHWTrigPtr = ListGetPtrToItem(master->slaveHWTrigTasks, i);
		if (!slaveHWTrigPtr->armed) {
			ArmedFlag = 0;
			break;
		}
	}
	
	return ArmedFlag; 
}

static void	HWTrigSlavesArmedStatusReset (TaskControl_type* master)
{
	SlaveHWTrigTask_type*   slaveHWTrigPtr; 
	
	for (size_t i = 1; i <= ListNumItems(master->slaveHWTrigTasks); i++) {
		slaveHWTrigPtr = ListGetPtrToItem(master->slaveHWTrigTasks, i);
		slaveHWTrigPtr->armed = FALSE;
	}
}


static void	disposeCmtTSQHandleEventInfo (void* eventInfo)
{
	if (!eventInfo) return;
	free(eventInfo);
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

FCallReturn_type* init_FCallReturn_type (int valFCall, const char errorOrigin[], const char errorDescription[])
{
	char*				Msg		= NULL;
	size_t				nchars;
	FCallReturn_type* 	result 	= malloc(sizeof(FCallReturn_type));
	
	if (!result) return NULL;
	
	result -> retVal 	= valFCall;
	if (errorDescription) {
		
		nchars 	= snprintf(Msg, 0, "<%s Error ID %d. Reason: %s>", errorOrigin, valFCall, errorDescription);
		Msg		= malloc((nchars+1)*sizeof(char));
		if (!Msg) {free(result); return NULL;}
		snprintf(Msg, nchars+1, "<%s Error. Reason: %s>", errorOrigin, errorDescription);
		result -> errorInfo	= Msg;
	} else
		result -> errorInfo = NULL;
		
	return result;
}

void discard_FCallReturn_type (FCallReturn_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->errorInfo);
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
			
		case TASK_EVENT_ONE_ITERATION:
			
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
			
		case TASK_EVENT_ERROR_OUT_OF_MEMORY:
			
			return StrDup("Out of memory");
			
		case TASK_EVENT_CUSTOM_MODULE_EVENT:
			
			return StrDup("Device or Module specific event");
			
	}
	
	return StrDup("?");
	
}

static char* FCallToString (TaskFCall_type fcall)
{
	switch (fcall) {
		
		case TASK_FCALL_NONE:
			
			return StrDup("FunctionCall None");
		
		case TASK_FCALL_CONFIGURE:
			
			return StrDup("FunctionCall Configure");
		
		case TASK_FCALL_ITERATE:
			
			return StrDup("FunctionCall Iterate");
			
		case TASK_FCALL_START:
			
			return StrDup("FunctionCall Start");
			
		case TASK_FCALL_RESET:
			
			return StrDup("FunctionCall Reset");
			
		case TASK_FCALL_DONE:
			
			return StrDup("FunctionCall Done");
			
		case TASK_FCALL_STOPPED:
			
			return StrDup("FunctionCall Stopped");
			
		case TASK_FCALL_DATA_RECEIVED:
			
			return StrDup("FunctionCall Data Received");
			
		case TASK_FCALL_MODULE_EVENT:
			
			return StrDup("FunctionCall Module Event");
			
		case TASK_FCALL_ERROR:
			
			return StrDup("FunctionCall Error");
			
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
	for (size_t i = 1; i <= ListNumItems(logItem->subtasks); i++) {
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
		if (i < ListNumItems(logItem->subtasks))
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
	
	if (ExecutionStatus == kCmtThreadFunctionComplete)
		CmtScheduleThreadPoolFunctionAdv(DEFAULT_THREAD_POOL_HANDLE, ScheduleTaskEventHandler, taskControl, 
									 	 DEFAULT_THREAD_PRIORITY, TaskEventHandlerExecutionCallback, 
									 	 (EVENT_TP_THREAD_FUNCTION_BEGIN | EVENT_TP_THREAD_FUNCTION_END), 
										 taskControl, taskControl->eventQThreadID, &taskControl->threadFunctionID);
}

void CVICALLBACK TaskDataItemsInQueue (CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData)
{
	TaskControl_type* 	taskControl 		= callbackData;
	CmtTSQHandle*		QHndlPtr			= malloc(sizeof(CmtTSQHandle));
	if (!QHndlPtr) {
		// flush queue
		CmtFlushTSQ(queueHandle, TSQ_FLUSH_ALL, NULL);
		TaskControlEvent(taskControl, TASK_EVENT_ERROR_OUT_OF_MEMORY, NULL, NULL);
	} else {
		*QHndlPtr = queueHandle;
		// inform Task Controller that data was placed in an otherwise empty data queue
		TaskControlEvent(taskControl, TASK_EVENT_DATA_RECEIVED, QHndlPtr, disposeCmtTSQHandleEventInfo);
	}

}

int CVICALLBACK ScheduleTaskEventHandler (void* functionData)
{
	TaskControl_type* 	taskControl = functionData;
	
	// dispatch event
	TaskEventHandler(taskControl);
	return 0;
}

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

int TaskControlEvent (TaskControl_type* RecipientTaskControl, TaskEvents_type event, void* eventInfo,
					  DisposeEventInfoFptr_type disposeEventInfoFptr)
{
	EventPacket_type eventpacket = {event, eventInfo, disposeEventInfoFptr};
	
	if (CmtWriteTSQData(RecipientTaskControl->eventQ, &eventpacket, 1, 0, NULL)) return 0;
	else return -1;
}

int	TaskControlIterationDone (TaskControl_type* taskControl, int errorID, char errorInfo[])
{
	if (errorID)
		return TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, init_ErrorMsg_type(errorID, "External Task Control Iteration", errorInfo), disposeTCIterDoneInfo);
	else
		return TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL);
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
		
	taskControl->abortFlag = TRUE;
	
	// send STOP event to self
	TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL);
	
	// abort SubTasks recursively
	for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
		subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		AbortTaskControlExecution(subtaskPtr->subtask);
	}
	
}

int CVICALLBACK ScheduleIterateFunction (void* functionData)
{
	TaskControl_type* taskControl = functionData;
	
	(*taskControl->IterateFptr)(taskControl, taskControl->currIterIdx, &taskControl->abortFlag);
	
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
			
		case TASK_FCALL_DATA_RECEIVED:
			
			if (taskControl->DataReceivedFptr) fCallResult = (*taskControl->DataReceivedFptr)(taskControl, taskControl->state, *(CmtTSQHandle*)fCallData, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_MODULE_EVENT:
			
			if (taskControl->ModuleEventFptr) fCallResult = (*taskControl->ModuleEventFptr)(taskControl, taskControl->state, taskControl->currIterIdx, fCallData, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_ERROR:
			
			if (taskControl->ErrorFptr)  (*taskControl->ErrorFptr)(taskControl, taskControl->errorMsg->errorInfo, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
				
	}
	
	
	if (!fCallResult && fID != TASK_FCALL_ITERATE && fID != TASK_FCALL_ERROR) 
		if (functionMissingFlag)
			return NULL;																				// function not provided
		else
			return init_ErrorMsg_type(FunctionCall_Error_OutOfMemory, "FunctionCall", "Out of memory"); // out of memory   
	
	// in case of error, return error message otherwise return NULL
	return FCallReturnToErrorMsg(&fCallResult, fID);
}

int	TaskControlEventToSubTasks  (TaskControl_type* SenderTaskControl, TaskEvents_type event, void* eventInfo,
								 DisposeEventInfoFptr_type disposeEventInfoFptr)
{
	SubTask_type* 	subtaskPtr;
	// dispatch event to all subtasks
	for (size_t i = 1; i <= ListNumItems(SenderTaskControl->subtasks); i++) { 
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
	SubTask_type* subtaskPtr;
	for (size_t i = 1; i <= ListNumItems(child->parenttask->subtasks); i++) {
		subtaskPtr = ListGetPtrToItem(child->parenttask->subtasks, i);
		if (child == subtaskPtr->subtask) {
			ListRemoveItem(child->parenttask->subtasks, 0, i);
			// update subtask indices
			for (size_t i = 1; i <= ListNumItems(child->parenttask->subtasks); i++) {
				subtaskPtr = ListGetPtrToItem(child->parenttask->subtasks, i);
				subtaskPtr->subtask->subtaskIdx = i;
			}
			child->subtaskIdx = 0;
			child->parenttask = NULL;
			
			return 0; // found and removed
		}
		
	}
	
	return -2; // not found
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
	
	for (size_t i = 1; i <= ListNumItems(slave->masterHWTrigTask->slaveHWTrigTasks); i++) {
		slavePtr = ListGetPtrToItem(slave->masterHWTrigTask->slaveHWTrigTasks, i);
		if (slave == slavePtr->slaveHWTrigTask) {
			ListRemoveItem(slave->masterHWTrigTask->slaveHWTrigTasks, 0, i);
			// update slave indices from master list
			for (size_t i = 1; i <= ListNumItems(slave->masterHWTrigTask->slaveHWTrigTasks); i++) {
				slavePtr = ListGetPtrToItem(slave->masterHWTrigTask->slaveHWTrigTasks, i);
				slavePtr->slaveHWTrigTask->slaveHWTrigIdx = i;
			}
			slave->slaveHWTrigIdx = 0;
			slave->masterHWTrigTask = NULL;
			
			return 0; // found and removed
		}
		
	}
	
	return -2; // not found
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
#define TaskEventHandler_Error_NoFunctionality				-12
#define TaskEventHandler_Error_NoHWTriggerArmedEvent		-13
#define	TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout  -14
#define TaskEventHandler_Error_HWTrigSlaveNotArmed			-15

	
	EventPacket_type 		eventpacket;
	SubTask_type* 			subtaskPtr; 
	SlaveHWTrigTask_type*   slaveHWTrigPtr;
	int						nItems;
	ErrorMsg_type*			errMsg			= NULL;
	ErrorMsg_type*			tmpErrMsg		= NULL;
	char*					buff			= NULL;
	char*					eventStr;
	char*					stateStr;
	size_t					nchars;
	
	// get Task Controler event 
	// (since this function was called, there should be at least one event in the queue)
	if (CmtReadTSQData(taskControl->eventQ, &eventpacket, 1, 0, 0) <= 0) 
		return; // in case there are no items or there is an error
	
	// reset abort flag
	taskControl->abortFlag = 0;
	
	switch (taskControl->state) {
		
		case TASK_STATE_UNCONFIGURED:
		/* 
		Possible Task and SubTask Controller state combinations:
		
		Task in UNCONFIGURED + SubTasks in any of UNCONFIGURED, CONFIGURED, INITIAL or ERROR. Initially all Task Controllers start in the UNCONFIGURED state.
		
		Possible state transitions:
		
			--------------------------
			UNCONFIGURED -> CONFIGURED
			--------------------------
				TASK_EVENT_CONFIGURE
				
					IF  	TASK_FCALL_CONFIGURE is successful 
									
					AND 	Task Controller has SubTask Controllers
									
					THEN 	Switch state to CONFIGURED.
							Feed forward action: send TASK_EVENT_CONFIGURE to SubTasks.
						 	Feedback action: inform parent Task (if any) of state change to CONFIGURED.
												
			--------------------------
			UNCONFIGURED -> INITIAL	
			--------------------------
				TASK_EVENT_CONFIGURE
				
					IF 		TASK_FCALL_CONFIGURE is successful 
									
					AND	 	Task Controller does not have SubTask Controllers 
					AND		TASK_FCALL_RESET is successful 
									
					THEN	Switch state to INITIAL.
							Feedback action: inform parent Task (if any) of state change to INITIAL.
				
			--------------------------
			UNCONFIGURED -> ERROR
			--------------------------
				TASK_EVENT_CONFIGURE 
				
					IF 		TASK_FCALL_CONFIGURE failed
									
					OR 		sending TASK_EVENT_CONFIGURE to SubTasks failed
					OR		TASK_FCALL_RESET failed 
									
					THEN	Switch state to ERROR.
							Feedback action: inform parent Task (if any) of state change to ERROR.
											
				TASK_EVENT_SUBTASK_STATE_CHANGED
				
					IF 		SubTask in Error state
					
					THEN 	Feedback action: inform parent Task (if any) of state change to ERROR.   
		
												
				OTHER EVENTS (that are not handled explicitely)
				
							Switch to ERROR state.
		
		*/	
			switch (eventpacket.event) {
				
				case TASK_EVENT_CONFIGURE:
					
					// configure this task
					
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_CONFIGURE, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					// send TASK_EVENT_CONFIGURE to all subtasks if there are any
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE posting to SubTasks failed"); 
							
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					} 
					
					// If there are no subtasks, then reset device/module and make transition here to 
					// INITIAL state and inform parent task if there is any
					if (!ListNumItems(taskControl->subtasks)) {
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						// reset armed status of Slave HW Triggered Task Controllers, if any
						HWTrigSlavesArmedStatusReset (taskControl);
						
						ChangeState(taskControl, eventpacket.event, TASK_STATE_INITIAL);
						
					} else
						ChangeState(taskControl, eventpacket.event, TASK_STATE_CONFIGURED);
						
					break;
					
				case TASK_EVENT_STOP:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
			}
			
			break;
			
		case TASK_STATE_CONFIGURED:
		/* 
		Possible Task and SubTask Controller state combinations:
		
		Task in CONFIGURED + SubTasks in any of UNCONFIGURED, CONFIGURED, INITIAL or ERROR.
		
		Possible state transitions:
		
			--------------------------
			CONFIGURED -> UNCONFIGURED
			--------------------------
				TASK_EVENT_CONFIGURE 
				
							Feedback action: switch to UNCONFIGURED state
							Send CONFIGURE command to self
												
			--------------------------
			CONFIGURED -> INITIAL	
			--------------------------
				TASK_EVENT_SUBTASK_STATE_CHANGED
				
					IF 		All SubTasks in INITIAL state
					
					AND		TASK_FCALL_RESET is successful
					
					THEN	Switch to INITIAL state
							Feedback action: inform parent Task (if any) of state change to INITIAL.
									
			--------------------------
			CONFIGURED -> ERROR
			--------------------------
				TASK_EVENT_CONFIGURE 
				
					IF		Sending TASK_EVENT_CONFIGURE to self failed
					THEN	Switch to ERROR state.
							Feedback action: inform parent Task (if any) of state change to ERROR.
											
											
				TASK_EVENT_SUBTASK_STATE_CHANGED
				
					IF		SubTask in Error state
					
					THEN	Switch to ERROR state.
							Feedback action: inform parent Task (if any) of state change to ERROR.
						
					IF 		All SubTasks in INITIAL state
					AND		TASK_FCALL_RESET failed
					
					THEN	Switch to ERROR state.
							Feedback action: inform parent Task (if any) of state change to ERROR.
		
												
				OTHER EVENTS (that are not handled explicitely)
				
							Switch to ERROR state.
		
		*/	
			
			switch (eventpacket.event) { 
				
				case TASK_EVENT_CONFIGURE: 
					// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_STOP:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					// check states of all subtasks and transition to INITIAL state if all subtasks are in INITIAL state
					BOOL InitialStateFlag = 1; // assume all subtasks are in INITIAL state
					for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_INITIAL) {
							InitialStateFlag = 0;
							break;
						}
					}
					
					if (InitialStateFlag) {
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						ChangeState(taskControl, eventpacket.event, TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
						 
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);	
			}
			
			break;
			
		case TASK_STATE_INITIAL:
		// Possible state transitions:
		// INITIAL -> UNCONFIGURED
		// INITIAL -> RUNNING
		// INITIAL -> ERROR
			
			switch (eventpacket.event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ONE_ITERATION: 
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_START, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Set flag to iterate once or continue until done or stopped
					//---------------------------------------------------------------------------------------------------------------   
					if (eventpacket.event == TASK_EVENT_ONE_ITERATION) 
						taskControl->nIterationsFlag = 1;
					else
						taskControl->nIterationsFlag = -1;
					
					//---------------------------------------------------------------------------------------------------------------
					// Check if there are no SubTasks and iteration function is not repeated at least once, give error
					//---------------------------------------------------------------------------------------------------------------
					
					if (taskControl->mode == TASK_FINITE && !taskControl->repeat && !ListNumItems(taskControl->subtasks)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_NoFunctionality, taskControl->taskName, "Task Controller has no functionality");
										
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state and iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);   
					
					break;
					
				case TASK_EVENT_STOP:  
					
					//ignore this command 
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured, switch to unconfigured
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						// insert action to perform
						ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
						break;	
					}
					
					break;
					
			
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
			
			}
			
			break;
			
		case TASK_STATE_IDLE:
		// Possible state transitions:
		// IDLE -> UNCONFIGURED
		// IDLE -> WAITING
		// IDLE -> RUNNING	
		// IDLE -> ERROR
		
			switch (eventpacket.event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
				case TASK_EVENT_ONE_ITERATION: 
					
					//--------------------------------------------------------------------------------------------------------------- 
					// Call Start Task Controller function pointer to inform that task will start
					//--------------------------------------------------------------------------------------------------------------- 
					
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_START, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Set flag to iterate once or continue until done or stopped
					//---------------------------------------------------------------------------------------------------------------   
					if (eventpacket.event == TASK_EVENT_ONE_ITERATION)
						taskControl->nIterationsFlag = 1;
					else
						taskControl->nIterationsFlag = -1;
					
					//---------------------------------------------------------------------------------------------------------------
					// Switch to RUNNING state and iterate Task Controller
					//---------------------------------------------------------------------------------------------------------------
					
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);   
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					// ignore this command
						
					break;
					
				case TASK_EVENT_RESET:
					
					// reset Device or Module to bring it to its INITIAL state with iteration index 0.
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
						
					// reset iteration index
					taskControl->currIterIdx = 0;	
					
					// send TASK_EVENT_RESET to all subtasks if there are any
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_RESET, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_RESET posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					} 
					
					// if there are no subtasks, then make transition here to INITIAL state and inform
					// parent task, if there is any, that subtask is in INITIAL state
					if (!ListNumItems(taskControl->subtasks)) 
						ChangeState(taskControl, eventpacket.event, TASK_STATE_INITIAL);
					
					break;
					
				case TASK_EVENT_STOP:  
					
					ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);  // just inform parent
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);  // just inform parent
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured, switch to unconfigured
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						// insert action to perform
						ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
						break;	
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
					
			}
			
			break;
			
		case TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES:
			
			switch (eventpacket.event) {
				
				case TASK_EVENT_SUBTASK_STATE_CHANGED: 
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_HWTRIG_SLAVE_ARMED:
					
					// update Slave HW Trig Task Controller armed flag in master list
					slaveHWTrigPtr = ListGetPtrToItem(taskControl->slaveHWTrigTasks, ((SlaveHWTrigTaskEventInfo_type*)eventpacket.eventInfo)->slaveHWTrigIdx);
					slaveHWTrigPtr->armed = TRUE; 
					
					if (!HWTrigSlavesAreArmed(taskControl))
						break; // stop here, not all Slaves are armed 
					
									
					//---------------------------------------------------------------------------------------------------------------
					// Slaves are ready to be triggered, reset Slaves armed status
					//---------------------------------------------------------------------------------------------------------------
									
					HWTrigSlavesArmedStatusReset(taskControl);
									
					//---------------------------------------------------------------------------------------------------------------
					// Iterate and fire Master HW Trigger 
					//---------------------------------------------------------------------------------------------------------------
									
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
					// switch state and wait for iteration to complete
					ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
							
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
							
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
			}
			
			break;
			
		case TASK_STATE_RUNNING:
		 
			switch (eventpacket.event) {
				
				case TASK_EVENT_ITERATE:
					
					//---------------------------------------------------------------------------------------------------------------
					// Check if an iteration is needed:
					// If current iteration index is smaller than the total number of requested repeats, except if repeat = 0 and
					// first iteration or if task is continuous.
					//---------------------------------------------------------------------------------------------------------------
					
					if (!((taskControl->currIterIdx < taskControl->repeat || (!taskControl->repeat && !taskControl->currIterIdx) || taskControl->mode == TASK_CONTINUOUS) && taskControl->nIterationsFlag))  {
								
						//---------------------------------------------------------------------------------------------------------------- 	 
						// Task Controller is finite switch to DONE
						//---------------------------------------------------------------------------------------------------------------- 	
										
						// call done function
						if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
							break;
						}
								
						ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
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
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
											break;  // stop here
										}
										
									
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if HW triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
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
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break;
									}
										
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									HWTrigSlavesArmedStatusReset(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// Iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if trigger settings are valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (SlaveHWTrigTaskHasParentMaster(taskControl) && (taskControl->repeat != 1 || taskControl->mode == TASK_CONTINUOUS)) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMismatch, taskControl->taskName, "If a Slave HW Triggered Task Controller has a Master HW Triggering Task Controller as its parent task, then the Slave HW Triggered Task Controller must have only one iteration");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break; // stop here
										
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION); 
									
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
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
									break;
								}
								
								break; // stop here
							}	
							
							switch (GetTaskControlHWTrigger(taskControl)) {
								
								case TASK_NO_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// There are no SubTasks, call iteration function
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
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
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break;
									}	
									
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here, Slaves are not ready
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									HWTrigSlavesArmedStatusReset(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// There are no SubTasks, iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if iteration timeout setting is valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->iterTimeout) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout, taskControl->taskName, "A Slave HW Triggered Task Controller cannot have 0 timeout");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION); 
										
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
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
									break;
								}
							
							switch (GetTaskControlHWTrigger(taskControl)) {
								
								case TASK_NO_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Call iteration function if needed
									//---------------------------------------------------------------------------------------------------------------
									
									if (taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
											break;
										}
										
									// switch state and wait for iteration and SubTasks if there are any to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Make sure that when HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here, Slaves are not ready
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									HWTrigSlavesArmedStatusReset(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// Iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
										
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Make sure that when HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break;
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if iteration timeout setting is valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->iterTimeout) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout, taskControl->taskName, "A Slave HW Triggered Task Controller cannot have 0 timeout");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION); 
									
									break;
							}
							
							break;
					}
					
					break;
					
				case TASK_EVENT_STOP:
				// Stops iterations and switches to IDLE or DONE states if there are no SubTask Controllers or to STOPPING state and waits for SubTasks to complete their iterations
					
					if (!ListNumItems(taskControl->subtasks)) {
						// if there are no SubTask Controllers
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
							
						} else {
							// switch to IDLE state if finite task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_STOPPED, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
						}
					} else {
						// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
						if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
						
						ChangeState(taskControl, eventpacket.event, TASK_STATE_STOPPING);
					}
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// send STOP command to self if continuous task controller and forward TASK_EVENT_STOP_CONTINUOUS_TASK to subtasks
					if (taskControl->mode == TASK_CONTINUOUS) 
						if (TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL) < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_STOP self posting failed"); 
								
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
							break;
						}
					
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
						
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state or unconfigured, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR || subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					//---------------------------------------------------------------------------------------------------------------- 
					// If SubTasks are not yet complete, switch to RUNNING state and wait for their completion
					//---------------------------------------------------------------------------------------------------------------- 
					
					// consider only transitions to TASK_STATE_DONE 
					if (subtaskPtr->subtaskState != TASK_STATE_DONE)
						break; // stop here
					
					// assume all subtasks are done 
					BOOL AllDoneFlag = 1;
							
					// check SubTasks
					for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
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
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
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
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									else 
										if (TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL) < 0) {
											taskControl->errorMsg =
											init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATION_DONE posting to self failed"); 
						
											FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
											ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
											break;
										}
									
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION); 	
									
									break;
									
								case TASK_MASTER_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if HW Triggering is enabled, Task Controller has at least one iteration
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->repeat && taskControl->mode == TASK_FINITE) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_HWTriggeringMinRepeat, taskControl->taskName, "When HW Triggering is enabled, the Task Controller should iterate at least once");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
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
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break;
									}	
									
									//---------------------------------------------------------------------------------------------------------------
									// If Slaves are not ready, wait until they are ready
									//---------------------------------------------------------------------------------------------------------------
									
									if (!HWTrigSlavesAreArmed(taskControl)) {
										ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES);
										break; // stop here, Slaves are not ready
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Slaves are ready to be triggered, reset Slaves armed status
									//---------------------------------------------------------------------------------------------------------------
									
									HWTrigSlavesArmedStatusReset(taskControl);
									
									//---------------------------------------------------------------------------------------------------------------
									// There are no SubTasks, iterate and fire Master HW Trigger 
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
									// switch state and wait for iteration to complete
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION);  
									
									break;
									
								case TASK_SLAVE_HWTRIGGER:
									
									//---------------------------------------------------------------------------------------------------------------
									// Check if iteration timeout setting is valid
									//---------------------------------------------------------------------------------------------------------------
									
									if (!taskControl->iterTimeout) {
										
										taskControl->errorMsg = 
										init_ErrorMsg_type(TaskEventHandler_Error_SlaveHWTriggeredZeroTimeout, taskControl->taskName, "A Slave HW Triggered Task Controller cannot have 0 timeout");
										
										FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
										ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
										break; // stop here
									}
									
									//---------------------------------------------------------------------------------------------------------------
									// Call the iteration function and arm the Slave HWT Task Controller. Inform the Task Controller that it is armed
									// by sending a TASK_EVENT_HWTRIG_SLAVE_ARMED to self with no additional parameters before terminating the iteration
									//---------------------------------------------------------------------------------------------------------------
									
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL);
										
									ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING_WAITING_ITERATION); 
										
									break;
							}
							
							break;
					}
					
					break;
					
				case TASK_EVENT_HWTRIG_SLAVE_ARMED:
					
					// update Slave HW Trig Task Controller armed flag
					slaveHWTrigPtr = ListGetPtrToItem(taskControl->slaveHWTrigTasks, ((SlaveHWTrigTaskEventInfo_type*)eventpacket.eventInfo)->slaveHWTrigIdx);
					slaveHWTrigPtr->armed = TRUE; 
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
					
			}
			
			break;
			
		case TASK_STATE_RUNNING_WAITING_ITERATION:
			switch (eventpacket.event) {
				
				case TASK_EVENT_ITERATE:
					
					//---------------------------------------------------------------------------------------------------------------- 	
					// There are no SubTasks or SubTasks are done, ask for another iteration if more are needed 
					//---------------------------------------------------------------------------------------------------------------- 
									
					//if (((taskControl->currIterIdx < taskControl->repeat || taskControl->mode == TASK_CONTINUOUS)) && !taskControl->iterateOnceFlag) {
					if ((taskControl->currIterIdx < taskControl->repeat || (!taskControl->repeat && !taskControl->currIterIdx) || taskControl->mode == TASK_CONTINUOUS) && taskControl->nIterationsFlag ) { 
								
						// ask for another iteration
						if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) { 	// post to self ITERATE event
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
								
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
							break;
						}
							
						// switch to RUNNING state
						ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);
						
					} else {
								
						//---------------------------------------------------------------------------------------------------------------- 	 
						// If Task Controller is continuous and only one iteration was requested, switch to IDLE state
						//---------------------------------------------------------------------------------------------------------------- 	 
								
						if (taskControl->mode == TASK_CONTINUOUS) {
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
							break; // stop here
						}
								
						//---------------------------------------------------------------------------------------------------------------- 	 
						// If Task Controller is finite switch to DONE or IDLE state depending whether more iterations are needed
						//---------------------------------------------------------------------------------------------------------------- 	
										
						if (taskControl->currIterIdx < taskControl->repeat)
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
						else {
							// call done function
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
								break;
							}
								
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);					
						}
					}
									
					break;
				
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
					//---------------------------------------------------------------------------------------------------------------   
					if (eventpacket.eventInfo) {
						ErrorMsg_type* errMsg = eventpacket.eventInfo;
						
						if (errMsg->errorID < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_IterateExternThread, taskControl->taskName, errMsg->errorInfo); 
							// abort the entire Nested Task Controller hierarchy
							AbortTaskControlExecution(taskControl);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								
							break;
						}
					}
					
					//---------------------------------------------------------------------------------------------------------------   
					// Check if Task Controller is a Slave, that it has been armed before completing the iteration
					//---------------------------------------------------------------------------------------------------------------   
					if (GetTaskControlHWTrigger(taskControl) == TASK_SLAVE_HWTRIGGER && !taskControl->slaveArmedFlag) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_HWTrigSlaveNotArmed, taskControl->taskName, "A Slave HW Triggered Task Controller must be armed by TASK_EVENT_HWTRIG_SLAVE_ARMED before completing the call to its iteration function");
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					
					//---------------------------------------------------------------------------------------------------------------   
					// If an iteration was performed, increment iteration index 
					//--------------------------------------------------------------------------------------------------------------- 
					
					taskControl->currIterIdx++;
					if (taskControl->nIterationsFlag > 0)
						taskControl->nIterationsFlag--;
					
					//---------------------------------------------------------------------------------------------------------------   
					// If iteration was aborted
					//---------------------------------------------------------------------------------------------------------------   
					if (taskControl->abortIterationFlag) {
						// Stops iterations and switches to IDLE or DONE states if there are no SubTask Controllers or to STOPPING state and waits for SubTasks to complete their iterations
					
						if (!ListNumItems(taskControl->subtasks)) {
							// if there are no SubTask Controllers
							if (taskControl->mode == TASK_CONTINUOUS) {
								// switch to DONE state if continuous task controller
								if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL))) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
							
							} else {
								// switch to IDLE state if finite task controller
								if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_STOPPED, NULL))) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
									break;
								}
							
								ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
							}
						} else {
							
							// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
							if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
						
							ChangeState(taskControl, eventpacket.event, TASK_STATE_STOPPING);
						}
						
						break;
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
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
									break;
								}
										
								// switch to RUNNING state and wait for SubTasks to complete
								ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);
								break; // stop here
										
							} 
							
							// If there are no SubTasks, fall through the case and ask for another iteration unless only one iteration was requested
							
						case TASK_ITERATE_AFTER_SUBTASKS_COMPLETE:
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
								
							}
							
							// stay in TASK_STATE_RUNNING_WAITING_ITERATION
										
							break;
									
						case TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS:
							
							//---------------------------------------------------------------------------------------------------------------- 
							// If SubTasks are not yet complete, switch to RUNNING state and wait for their completion
							//---------------------------------------------------------------------------------------------------------------- 
							
							// assume all subtasks are done 
							BOOL AllDoneFlag = 1;
							
							// check SubTasks
							for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
								subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
								if (subtaskPtr->subtaskState != TASK_STATE_DONE) {
									AllDoneFlag = 0;
									break;
								}
							}
					
							if (!AllDoneFlag) {
								ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING); 
								break;
							}
							
							//---------------------------------------------------------------------------------------------------------------- 
							// Ask for another iteration and check later if iteration is needed or possible
							//---------------------------------------------------------------------------------------------------------------- 
							
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE posting to self failed"); 
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
								
							}
							
							// stay in TASK_STATE_RUNNING_WAITING_ITERATION
							
							break;
					}
							
					break;
					
				case TASK_EVENT_ITERATION_TIMEOUT:
					
					// reset timerID
					taskControl->iterationTimerID = 0;
					
					// generate timeout error
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_IterateFCallTmeout, taskControl->taskName, "Iteration function call timeout"); 
						
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_STOP:
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
				
					// set abort flag to signal external thread executing the iteration that it must finish
					taskControl->abortIterationFlag = TRUE;
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state or unconfigured, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR || subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					break;
					
				case TASK_EVENT_HWTRIG_SLAVE_ARMED:
					
					switch (GetTaskControlHWTrigger(taskControl)) {  
						
						case TASK_NO_HWTRIGGER:
							
							// not allowed, generate error
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_NoHWTriggerArmedEvent, taskControl->taskName, "Slave Task Controller Armed event received but no HW triggering enabled for this Task Controller");
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							
							break;
							
						case TASK_MASTER_HWTRIGGER:
							
							// update Slave HW Trig Task Controller armed flag
							slaveHWTrigPtr = ListGetPtrToItem(taskControl->slaveHWTrigTasks, ((SlaveHWTrigTaskEventInfo_type*)eventpacket.eventInfo)->slaveHWTrigIdx);
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
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							// remove timeout timer
							if (taskControl->iterationTimerID > 0) {
								DiscardAsyncTimer(taskControl->iterationTimerID);
								taskControl->iterationTimerID = 0;
							}
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					// remove timeout timer
					if (taskControl->iterationTimerID > 0) {
						DiscardAsyncTimer(taskControl->iterationTimerID);
						taskControl->iterationTimerID = 0;
					}
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						// remove timeout timer
						if (taskControl->iterationTimerID > 0) {
							DiscardAsyncTimer(taskControl->iterationTimerID);
							taskControl->iterationTimerID = 0;
						}
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				default:
					
					// remove timeout timer
					if (taskControl->iterationTimerID > 0) {
						DiscardAsyncTimer(taskControl->iterationTimerID);
						taskControl->iterationTimerID = 0;
					}
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
					
			}
					
			break;
			
		case TASK_STATE_STOPPING:
			
			switch (eventpacket.event) {
				
				case TASK_EVENT_STOP:
					
					// Stops iterations and switches to IDLE or DONE states if there are no SubTask Controllers or stays in STOPPING state and waits for SubTasks to complete their iterations
					
					if (!ListNumItems(taskControl->subtasks)) {
						// if there are no SubTask Controllers
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
							
						} else {
							// switch to IDLE state if finite task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_STOPPED, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
						}
					} else 
						// send TASK_EVENT_STOP_CONTINUOUS_TASK event to all continuous subtasks (since they do not stop by themselves)
						if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_STOP_CONTINUOUS_TASK, NULL, NULL) < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_STOP_CONTINUOUS_TASK posting to SubTasks failed"); 
						
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
						
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured, switch to unconfigured
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						// insert action to perform
						ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
						break;	
					}
					
					BOOL	IdleOrDoneFlag	= 1;	// assume all subtasks are either IDLE or DONE
					for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if ((subtaskPtr->subtaskState != TASK_STATE_IDLE) && (subtaskPtr->subtaskState != TASK_STATE_DONE)) {
							IdleOrDoneFlag = 0;
							break;
						}
					}
					
					// If all subtasks are IDLE or DONE, then stop this Task Controller as well
					if (IdleOrDoneFlag) 
						if (taskControl->mode == TASK_CONTINUOUS) {
							// switch to DONE state if continuous task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
							
						} else {
							// switch to IDLE state if finite task controller
							if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_STOPPED, NULL))) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
						}
						
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 	
			}
			
			break;
			
		case TASK_STATE_DONE:
		// This state can be reached only if all SubTask Controllers are in a DONE state
			
			switch (eventpacket.event) {
				
				case TASK_EVENT_CONFIGURE:
				// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_START:
					
					// reset device/module
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
						
					// reset iterations
					taskControl->currIterIdx = 0;
					
					// switch to INITIAL state
					ChangeState(taskControl, eventpacket.event, TASK_STATE_INITIAL);
					
					// send START event to self
					if (TaskControlEvent(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_START self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_RESET:
					
					// send RESET event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_RESET, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_RESET posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					// change state to INITIAL if there are no subtasks and call reset function
					if (!ListNumItems(taskControl->subtasks)) {
						
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						ChangeState(taskControl, eventpacket.event, TASK_STATE_INITIAL);
						
					}
					
					break;
					
				case TASK_EVENT_STOP:  
					
					ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);  // just inform parent 
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);  // just inform parent
					
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// update subtask state
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					// if subtask is in an error state, then switch to error state
					if (subtaskPtr->subtaskState == TASK_STATE_ERROR) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_SubTaskInErrorState, taskControl->taskName, subtaskPtr->subtask->errorMsg->errorInfo); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;	
					}
					
					// if subtask is unconfigured, switch to unconfigured
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						// insert action to perform
						ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
						break;	
					}
					
					// check states of all subtasks and transition to INITIAL state if all subtasks are in INITIAL state
					BOOL InitialStateFlag = 1; // assume all subtasks are in INITIAL state
					for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_INITIAL) {
							InitialStateFlag = 0;
							break;
						}
					}
					
					if (InitialStateFlag) {
						// reset device/module
						if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
						
						// reset iterations
						taskControl->currIterIdx = 0;
						
						ChangeState(taskControl, eventpacket.event, TASK_STATE_INITIAL);
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				case TASK_EVENT_ERROR_OUT_OF_MEMORY:
					
					taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_OutOfMemory, taskControl->taskName, "Out of memory");
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
					
				default:
					
					eventStr = EventToString(eventpacket.event);
					stateStr = StateToString(taskControl->state);	 	
					nchars = snprintf(buff, 0, "%s event is invalid for %s state", eventStr, stateStr);
					buff = malloc ((nchars+1)*sizeof(char));
					snprintf(buff, nchars+1, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 	
				
			}
			
			break;
			
		case TASK_STATE_ERROR:
			switch (eventpacket.event) {
				
				case TASK_EVENT_CONFIGURE:
					// Reconfigures Task Controller
					
					// configure again only this task control
					ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
					if (TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_CONFIGURE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
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
					subtaskPtr = ListGetPtrToItem(taskControl->subtasks, ((SubTaskEventInfo_type*)eventpacket.eventInfo)->subtaskIdx);
					subtaskPtr->subtaskState = ((SubTaskEventInfo_type*)eventpacket.eventInfo)->newSubTaskState; 
					
					break;
					
			
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo))) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
							
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					break;
					
				case TASK_EVENT_CUSTOM_MODULE_EVENT:
					
					// call custom module event function
					if ((errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo))) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
							
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
							break;
						}
					
					break;
			}
			
			break;
	}
	
	// free memory for extra eventInfo if any
	if (eventpacket.eventInfo && eventpacket.disposeEventInfoFptr)
		(*eventpacket.disposeEventInfoFptr)(eventpacket.eventInfo);
}


