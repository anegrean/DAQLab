//==============================================================================
//
// Title:		taskcontrol.c
// Purpose:		A short description of the implementation.
//
// Created on:	2014.05.11. at 10:35:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include <windows.h>
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

typedef enum {
	TASK_CONTROL_STATE_CHANGE,
	TASK_CONTROL_FUNCTION_CALL
} Action_type;

typedef struct FCallReturn {
	int						retVal;						// Value returned by function call.
	char*					errorInfo;					// In case of error, additional info.
	int						SuspendExecution;			// 0, Task Controller execution continues with return from function call
														// 1..N > 0, Task Controller execution is suspended for N seconds during which a 
														// call to ContinueTaskControlExecution must be made to continue execution.
														// If N seconds pass without the call, a timeout error occurs
														// -N..-1, Task Controller execution is suspended without timeout until a call
														// to ContinueTaskControlExecution is made.
} FCallReturn_type;

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
	size_t					subtaskIdx;					// 1-based index of subtask from subtasks list. If task doesn't have a parent
														// task then index is 0.
	CmtTSQHandle			eventQ;						// Event queue to which the state machine reacts.
	ListType				dataQs;						// Incoming data queues, list of CmtTSQHandle type.
	unsigned int			eventQThreadID;				// Thread ID in which queue events are processed.
	CmtThreadFunctionID		threadFunctionID;			// ID of ScheduleTaskEventHandler that is executed in a separate thread from the main thread.
	TaskStates_type 		state;						// Module execution state.
	size_t					repeat;						// Total number of repeats, 
														// 0 - continuous, 1..N - finite number of repeats
	size_t					currIterIdx;    			// 1-based task execution iteration index  
	TaskControl_type*		parenttask;					// Pointer to parent task that own this subtask. 
														// If this is the main task, it has no parent and this is NULL. 
	ListType				subtasks;					// List of subtasks of SubTask_type.
	void*					moduleData;					// Reference to module specific data that is 
														// controlled by the task.
	TaskExecutionLog_type*	logPtr;						// Pointer to logging structure. NULL if logging is not enabled.
	BOOL					continueFlag;				// Continues execution of a Task Controller after a function pointer call
	FCallReturn_type*		fCallResult;				// Function call result signaled by a continueFlag.
	ErrorMsg_type*			errorMsg;					// When switching to an error state, additional error info is written here
	double					waitBetweenIterations;		// During a RUNNING state, waits specified ammount in seconds between iterations
	BOOL					abortFlag;					// When set to TRUE, it signals the provided function pointers that they must abort running processes.
				
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

static char*					StateToString					(TaskStates_type state);
static char*					EventToString					(TaskEvents_type event);
static char*					FCallToString					(TaskFCall_type fcall);

// eventInfo functions
static SubTaskEventInfo_type* 	init_SubTaskEventInfo_type 		(size_t subtaskIdx, TaskStates_type state);
static void						discard_SubTaskEventInfo_type 	(SubTaskEventInfo_type** a);
static void						disposeSubTaskEventInfo			(void* eventInfo);
static void						disposeCmtTSQHandleEventInfo	(void* eventInfo);

//==============================================================================
// Global variables

int mainThreadID;	// Main thread ID where UI callbacks are processed

//==============================================================================
// Global functions

void 	CVICALLBACK TaskEventItemsInQueue 				(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

void 	CVICALLBACK TaskDataItemsInQueue 				(CmtTSQHandle queueHandle, unsigned int event, int value, void *callbackData);

int 	CVICALLBACK ScheduleTaskEventHandler 			(void* functionData);

void 	CVICALLBACK TaskEventHandlerExecutionCallback 	(CmtThreadPoolHandle poolHandle, CmtThreadFunctionID functionID, unsigned int event, int value, void *callbackData);

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
	
	a -> taskName 				= StrDup(taskname);
	a -> subtaskIdx				= 0;  
	a -> state					= TASK_STATE_UNCONFIGURED;
	a -> repeat					= 1;
	a -> currIterIdx			= 0;
	a -> parenttask				= NULL;
	a -> moduleData				= NULL;
	a -> logPtr					= NULL;
	a -> continueFlag			= 0;
	a -> fCallResult			= NULL;
	a -> errorMsg				= NULL;
	a -> waitBetweenIterations	= 0;
	a -> abortFlag				= 0;
	
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
	
	if (a->eventQ) 		CmtDiscardTSQ(a->eventQ);
	if (a->dataQs)	 	ListDispose(a->dataQs);
	if (a->subtasks)    ListDispose(a->subtasks);
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
	discard_FCallReturn_type(&(*a)->fCallResult);
	
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
	return taskControl->taskName;
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
	
	nchars 			= snprintf(a->errorInfo, 0, "<%s Error ID: %d, Reason: %s.>", errorOrigin, errorID, errorDescription); 
	a->errorInfo 	= malloc((nchars+1) * sizeof(char));
	if (!a->errorInfo) {free(a); return NULL;}
	snprintf(a->errorInfo, nchars, "<%s Error ID: %d, Reason: %s.>", errorOrigin, errorID, errorDescription); 
	
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
	
	nchars = snprintf(a -> errorInfo, 0, "<%s Error ID: %d, Reason: %s.>", 
					  FCallToString(fID), (*fCallReturn)->retVal, (*fCallReturn)->errorInfo); 
	a -> errorInfo	= malloc ((nchars+1)*sizeof(char));
	if (!a->errorInfo) {free(a); return NULL;}
	snprintf(a -> errorInfo, nchars, "<%s Error ID: %d, Reason: %s.>", 
					  FCallToString(fID), (*fCallReturn)->retVal, (*fCallReturn)->errorInfo); 
	
	discard_FCallReturn_type(fCallReturn);
	
	return a;
}

FCallReturn_type* init_FCallReturn_type (int valFCall, const char errorOrigin[], 
									     const char errorDescription[], int SuspendExecution)
{
	char*				Msg		= NULL;
	size_t				nchars;
	FCallReturn_type* 	result 	= malloc(sizeof(FCallReturn_type));
	
	if (!result) return NULL;
	
	result -> retVal 	= valFCall;
	if (errorDescription) {
		
		nchars 	= snprintf(Msg, 0, "<%s Error ID %d. Reason: %s.>", errorOrigin, valFCall, errorDescription);
		Msg		= malloc((nchars+1)*sizeof(char));
		if (!Msg) {free(result); return NULL;}
		snprintf(Msg, nchars, "<%s Error. Reason: %s.>", errorOrigin, errorDescription);
		result -> errorInfo	= Msg;
	} else
		result -> errorInfo = NULL;
		
	result -> SuspendExecution = SuspendExecution; 
	
	return result;
}

void discard_FCallReturn_type (FCallReturn_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->errorInfo);
	OKfree(*a);
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
			
		case TASK_STATE_STOPPING:
			
			return StrDup("Stopping");
			
		case TASK_STATE_ITERATING:
			
			return StrDup("Iterating");
			
		case TASK_STATE_DONE:
			
			return StrDup("Done"); 
			
		case TASK_STATE_WAITING:
			
			return StrDup("Waiting"); 
			
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

static int ChangeState (TaskControl_type* taskControl, TaskEvents_type event, TaskStates_type newState)
{
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
		return TaskControlEvent(taskControl->parenttask, TASK_EVENT_SUBTASK_STATE_CHANGED, 
								init_SubTaskEventInfo_type(taskControl->subtaskIdx, taskControl->state), disposeSubTaskEventInfo);
	else
		return 0;
}

void ContinueTaskControlExecution (TaskControl_type* taskControl, FCallReturn_type* fCallResult)
{
	discard_FCallReturn_type(&taskControl->fCallResult);
	
	taskControl->fCallResult 	= fCallResult;
	taskControl->continueFlag   = 1;
}

void AbortTaskControlExecution (TaskControl_type* taskControl)
{
	SubTask_type* subtaskPtr;
		
	taskControl->abortFlag = TRUE;
	
	// send STOP event to self
	TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL);
	
	for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
		subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
		AbortTaskControlExecution(subtaskPtr->subtask);
	}
	
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
	
	taskControl->continueFlag = 0;
	switch (fID) {
		
		case TASK_FCALL_NONE:
				
			return init_ErrorMsg_type(FunctionCall_Error_Invalid_fID, 
				"FunctionCall", "Not a valid function ID"); // not a valid fID, error
			
		case TASK_FCALL_CONFIGURE:
			
			if (taskControl->ConfigureFptr) fCallResult = (*taskControl->ConfigureFptr)(taskControl, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_ITERATE:
			
			if (taskControl->IterateFptr) fCallResult = (*taskControl->IterateFptr)(taskControl, taskControl->currIterIdx, &taskControl->abortFlag);
			else functionMissingFlag = 1;
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
			
		case TASK_EVENT_CUSTOM_MODULE_EVENT:
			
			if (taskControl->ModuleEventFptr) fCallResult = (*taskControl->ModuleEventFptr)(taskControl, taskControl->state, taskControl->currIterIdx, fCallData, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
			
		case TASK_FCALL_ERROR:
			
			if (taskControl->ErrorFptr)  fCallResult = (*taskControl->ErrorFptr)(taskControl, taskControl->errorMsg->errorInfo, &taskControl->abortFlag);
			else functionMissingFlag = 1;
			break;
				
	}
	
	
	if (!fCallResult)
		if (functionMissingFlag)
			return NULL;																				// function not provided
		else
			return init_ErrorMsg_type(FunctionCall_Error_OutOfMemory, "FunctionCall", "Out of memory"); // out of memory   
	
	// in case of error, return error message
	if (fCallResult->retVal	< 0) 
		return FCallReturnToErrorMsg(&fCallResult, fID);
	
	// continue Task Control execution
	if (!fCallResult->SuspendExecution) return NULL;	// no error for function call
	
	// fCallResult->SuspendExecution is !=0, indefinite of finite suspension of execution 
	double t1 = Timer();
	double t2;
	while (!taskControl->continueFlag) {
		Sleep(FCALL_THREAD_SLEEP);
		t2 = Timer();
		if ((fCallResult->SuspendExecution > 0) && (t2 > t1 + fCallResult->SuspendExecution))
					// timeout, ContinueTaskControlExecution was not called within fCallResult seconds
			return init_ErrorMsg_type(FunctionCall_Error_FCall_Timeout, "FunctionCall", "Function call timeout");
	}
		
	return FCallReturnToErrorMsg(&taskControl->fCallResult, fID);
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
			child->parenttask = NULL;
			return 0; // found and removed
		}
		
	}
	
	return -1; // not found
}

static void TaskEventHandler (TaskControl_type* taskControl)
{
#define TaskEventHandler_Error_OutOfMemory				-1
#define TaskEventHandler_Error_FunctionCallFailed		-2
#define TaskEventHandler_Error_MsgPostToSubTaskFailed	-3
#define TaskEventHandler_Error_MsgPostToSelfFailed		-4
#define TaskEventHandler_Error_SubTaskInErrorState		-5
#define TaskEventHandler_Error_InvalidEventInState		-6

	
	EventPacket_type 	eventpacket;
	SubTask_type* 		subtaskPtr;  
	int					nItems;
	ErrorMsg_type*		errMsg			= NULL;
	ErrorMsg_type*		tmpErrMsg		= NULL;
	char*				buff			= NULL;
	char*				eventStr;
	char*				stateStr;
	size_t				nchars;
	
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
					
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_CONFIGURE, NULL)) {
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
						if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
						if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
				 
					// call start Task Controller function pointer to inform that task will start
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_START, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
						
					// call iteration function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// increment iteration index
					taskControl->currIterIdx++;
					
					// send START event to all subtasks (if there are any)
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					if ((taskControl->currIterIdx < taskControl->repeat || !taskControl->repeat)) {  	// if more iterations for this Task Controller are needed
						if (!ListNumItems(taskControl->subtasks)) 										// if there are no SubTask Controllers
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {   // post to self ITERATE event
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
								
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
								break;
							}
							
						ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);				// switch to RUNNING state
						
					} else {
						// call done function
						if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
							taskControl->errorMsg = 
							init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
							discard_ErrorMsg_type(&errMsg);
						
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
							break;
						}
						
						ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);					// switch to DONE state  
					}
						
					
					break;
					
				case TASK_EVENT_STOP:  
					
					//ignore command 
					
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_ONE_ITERATION:
					
					// call iteration function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// increment iteration index
					taskControl->currIterIdx++;
					
					// send START event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					if (!ListNumItems(taskControl->subtasks)) {
						// if there are no subtasks check if done or idle
						if (!taskControl->repeat) {
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
							
						} else
							if (taskControl->currIterIdx < taskControl->repeat)
								ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
							else {
								// call done function
								if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
									break;
								}
								
								ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
							}
					} else
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ITERATING);
						
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
				// Continues iterating until all iterations are complete or stopped if action is finite 
				// or until stopped if iterations are continuous
					
					// call start Task Controller function pointer to inform that task will start
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_START, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					
					// send START event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					
					// iterate
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// change state
					ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					// ignore this command
						
					break;
					
				case TASK_EVENT_ONE_ITERATION:
					
				// Continues iterating for a single cycle.If the Task Controller has multiple SubTask Controllers
				// nested, then they are iterated until completion after which the parent Task Controller 
				// is switch to an IDLE or done state.
				
					// call iteration function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// increment iteration index
					taskControl->currIterIdx++;
					
					// send START event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					if (!ListNumItems(taskControl->subtasks)) {
						// if there are no subtasks check if done or idle
						if (!taskControl->repeat) {
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
							
						} else
							if (taskControl->currIterIdx < taskControl->repeat)
								ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
							else {
								// call done function
								if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
									taskControl->errorMsg = 
									init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
									discard_ErrorMsg_type(&errMsg);
						
									FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
									ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
									break;
								}
								
								ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
							}
					} else
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ITERATING);
					
					
					break;
					
				case TASK_EVENT_RESET:
					
					// reset Device or Module to bring it to its INITIAL state with iteration index 0.
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
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
					
					//ignore command 
					
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
					
					// if not first iteration, then wait given number of seconds before iterating
					if (taskControl->currIterIdx)
						SyncWait(Timer(), taskControl->waitBetweenIterations);
					
					// call iteration function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						// abort the entire Nested Task Controller hierarchy
						AbortTaskControlExecution(taskControl);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// increment iteration index
					taskControl->currIterIdx++;
					
					if (!ListNumItems(taskControl->subtasks))
						if ((taskControl->currIterIdx < taskControl->repeat || !taskControl->repeat)) {
							// continue iterations, stay in RUNNING state
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
								
								// abort the entire Nested Task Controller hierarchy
								AbortTaskControlExecution(taskControl);
							
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								
								break;
							}
							
						} else {
							// call done function
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								// abort the entire Nested Task Controller hierarchy
								AbortTaskControlExecution(taskControl);
								
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
								
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
						}
						
					break;
					
			
				case TASK_EVENT_STOP:
				// Stops iterations and switches to IDLE or DONE states if there are no SubTask Controllers or to STOPPING state and waits for SubTasks to complete their iterations
					
					if (!ListNumItems(taskControl->subtasks)) {
						if (!taskControl->repeat) {
							// switch to DONE state if continuous task controller
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
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
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_STOPPED, NULL)) {
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
					
					// send STOP command to self if continuous task controller otherwise do nothing
					if (!taskControl->repeat)
						if (TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL) < 0) {
							taskControl->errorMsg =
							init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_STOP self posting failed"); 
								
							FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
							ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
							break;
						}
						
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
					
					
					BOOL 			AllDoneFlag			= 1;	// assume all subtasks are done
					for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_DONE) AllDoneFlag = 0;
					}
					
					// If all subtasks are done and there are iterations left (for finite mode) or Task Controller
					// is simply in continuous mode, perform another iteration
					if (AllDoneFlag)  
						if ((taskControl->currIterIdx < taskControl->repeat || !taskControl->repeat)) {
							// continue iterations, stay in RUNNING state
							if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
								
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
								break;
							}
							
							// send START event to all subtasks
							if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
								taskControl->errorMsg =
								init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
								
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
						} else {
							// call done function
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
								
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
						}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
						if (!taskControl->repeat) {
							// switch to DONE state if continuous task controller
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
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
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_STOPPED, NULL)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 	
			}
			
			break;
			
		case TASK_STATE_ITERATING:
		// Possible state transitions:
		// ITERATING -> DONE
		// ITERATING -> IDLE
		// ITERATING -> ERROR
		
			switch (eventpacket.event) {
				
				case TASK_EVENT_ITERATE:
					
					// call iteration function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ITERATE, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// increment iteration index
					taskControl->currIterIdx++;
					
					// send START event to all subtasks (if there are any)
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					if (!ListNumItems(taskControl->subtasks))
						if ((taskControl->currIterIdx < taskControl->repeat && taskControl->repeat))
							// if finite Task Controller and there are more iterations, switch to IDLE state
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
						
						else {
							// if continuous Task Controller or finite and iteration are done, switch to DONE state
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
						}
						
					break;
					
				case TASK_EVENT_STOP:  
					
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
					
					
					BOOL 			AllDoneFlag			= 1;	// assume all subtasks are done
					for (size_t i = 1; i <= ListNumItems(taskControl->subtasks); i++) {
						subtaskPtr = ListGetPtrToItem(taskControl->subtasks, i);
						if (subtaskPtr->subtaskState != TASK_STATE_DONE) {
							AllDoneFlag = 0;
							break;
						}
					}
					
					// If all subtasks are done and there are iterations left switch to IDLE state
					if (AllDoneFlag) 
						if ((taskControl->currIterIdx < taskControl->repeat || !taskControl->repeat)) {
							ChangeState(taskControl, eventpacket.event, TASK_STATE_IDLE);
						} else {
							// call done function
							if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DONE, NULL)) {
								taskControl->errorMsg = 
								init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
								discard_ErrorMsg_type(&errMsg);
						
								FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
								ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
								break;
							}
							
							ChangeState(taskControl, eventpacket.event, TASK_STATE_DONE);
						}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
						
					// reset iterations
					taskControl->currIterIdx = 0;
					
					// send START event to all subtasks
					if (TaskControlEventToSubTasks(taskControl, TASK_EVENT_START, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSubTaskFailed, taskControl->taskName, "TASK_EVENT_START posting to SubTasks failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
					
					// call start Task Controller function pointer to inform that task will start
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_START, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
						
					// iterate
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// change state
					ChangeState(taskControl, eventpacket.event, TASK_STATE_RUNNING);
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					// ignore this command
					
					break;
					
				case TASK_EVENT_ONE_ITERATION:
					
					// reset device/module
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
						taskControl->errorMsg = 
						init_ErrorMsg_type(TaskEventHandler_Error_FunctionCallFailed, taskControl->taskName, errMsg->errorInfo);
						discard_ErrorMsg_type(&errMsg);
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR);
						break;
					}
						
					// reset iterations
					taskControl->currIterIdx = 0;
					
					// iterate
					if (TaskControlEvent(taskControl, TASK_EVENT_ITERATE, NULL, NULL) < 0) {
						taskControl->errorMsg =
						init_ErrorMsg_type(TaskEventHandler_Error_MsgPostToSelfFailed, taskControl->taskName, "TASK_EVENT_ITERATE self posting failed"); 
						
						FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
						ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 
						break;
					}
					
					// change state
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ITERATING);   
					
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
						if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
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
					
					//ignore command 
					
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
						if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_RESET, NULL)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
					OKfree(eventStr);
					OKfree(stateStr);
					
					taskControl->errorMsg =
					init_ErrorMsg_type(TaskEventHandler_Error_InvalidEventInState, taskControl->taskName, buff); 
					OKfree(buff);
					
					FunctionCall(taskControl, eventpacket.event, TASK_FCALL_ERROR, NULL);
					ChangeState(taskControl, eventpacket.event, TASK_STATE_ERROR); 	
				
			}
			
			break;
			
		case TASK_STATE_WAITING:
			switch (eventpacket.event) {
				
				case TASK_EVENT_CONFIGURE:
					
					break;
					
				case TASK_EVENT_START:
					
					break;
					
				case TASK_EVENT_ITERATE:
					
					break;
					
				case TASK_EVENT_RESET:
					
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
					
					// if subtask is unconfigured, switch to unconfigured
					if (subtaskPtr->subtaskState == TASK_STATE_UNCONFIGURED) {
						// insert action to perform
						ChangeState(taskControl, eventpacket.event, TASK_STATE_UNCONFIGURED);
						break;	
					}
					
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// call data received event function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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
					snprintf(buff, nchars, "%s event is invalid for %s state", eventStr, stateStr);
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
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_ITERATE:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_RESET:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_STOP:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_STOP_CONTINUOUS_TASK:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_SUBTASK_STATE_CHANGED:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					break;
					
				case TASK_EVENT_DATA_RECEIVED:
					
					// clear error
					discard_ErrorMsg_type(&taskControl->errorMsg);
					
					// call data received event function
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_DATA_RECEIVED, eventpacket.eventInfo)) {
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
					if (errMsg = FunctionCall(taskControl, eventpacket.event, TASK_FCALL_MODULE_EVENT, eventpacket.eventInfo)) {
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


