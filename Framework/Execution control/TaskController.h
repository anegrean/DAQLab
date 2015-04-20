//==============================================================================
//
// Title:		TaskController.h
// Purpose:		A short description of the implementation.
//
// Created on:	2014.05.11. at 10:35:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __TaskController_H__
#define __TaskController_H__

#ifdef __cplusplus
    extern "C" {
#endif

#include <toolbox.h>
#include "VChannel.h"
#include "HWTriggering.h"
#include "Iterator.h"

#define TASKCONTROLLER_UI		"./Framework/Execution control/UI_TaskController.uir" 
#define N_TASK_EVENT_QITEMS		1000			// Number of events waiting to be processed by the state machine.

// Handy return type for functions that produce error descriptions

//----------------------------------------
// Task Controller States
//----------------------------------------
typedef enum {
	TASK_STATE_UNCONFIGURED,					// Task Controller is not configured and cannot run until it is configured.
	TASK_STATE_CONFIGURED,						// Task Controller is configured.
	TASK_STATE_INITIAL,							// Initial state of Task Controller before any iterations are performed.
	TASK_STATE_IDLE,							// Task Controller performed at least one iteration and has stopped.
	TASK_STATE_RUNNING,							// Task Controller is being iterated until required number of iterations is reached (if finite) or stopped.
	TASK_STATE_RUNNING_WAITING_ITERATION,		// Task Controller is iterating but the iteration (possibly occuring in another thread) is not yet complete.
												// Iteration is complete when a TASK_EVENT_ITERATION_DONE is received in this state.
	TASK_STATE_STOPPING,						// Task Controller received a STOP event and waits for SubTasks to complete their iterations.
	TASK_STATE_DONE,							// Task Controller finished required iterations if operation was finite.
	TASK_STATE_ERROR
} TaskStates_type;

//----------------------------------------
// Task Controller Events
//----------------------------------------
typedef enum {
	TASK_EVENT_CONFIGURE,						// Configures the Task Controller and its children for execution.
	TASK_EVENT_UNCONFIGURE,						// Unconfigures the Task Controller, in which case it cannot be executed.
	TASK_EVENT_START,							// Starts a Task Controller that is in an IDLE or PAUSED state. 
												// In case of an IDLE state, the Task Controller returns the module or
												// device to its initial state (iteration index = 0). In case of a PAUSED
												// state, it continues iterating the module or device.
	TASK_EVENT_ITERATE,							// Used to signal that another iteration is needed while in a RUNNING state. This may be signalled
												// by the Task Controller or another thread if the iteration occurs in another thread.
	TASK_EVENT_ITERATE_ONCE,					// Performs only one iteration of the Task Controller in an IDLE or DONE state.
	TASK_EVENT_ITERATION_DONE,					// Used to signal that an iteration completed if it was not carried out in the same thread as the call to TASK_FCALL_ITERATE.
	TASK_EVENT_ITERATION_TIMEOUT,				// Generated when TASK_EVENT_ITERATION_DONE is not received after a given timeout. 
	TASK_EVENT_RESET,							// Resets iteration index to 0, calls given reset function and brings State Controller 
												// back to INITIAL state.
	TASK_EVENT_STOP,							// Stops Task Controller iterations and allows SubTask Controllers to complete their iterations.
	TASK_EVENT_UPDATE_SUBTASK_STATE,   			// Used to update the state of a SubTask Controller.
	TASK_EVENT_DATA_RECEIVED,					// When data is placed in an otherwise empty data queue.
	TASK_EVENT_CUSTOM_MODULE_EVENT,				// To signal custom module or device events.
	TASK_EVENT_SUBTASK_ADDED_TO_PARENT,			// When a SubTask is added to a parent Task Controller
	TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT		// When a SubTask is disconnected from a parent task Controller
} TaskEvents_type;

//----------------------------------------
// Task Controller Function Calls
//----------------------------------------
typedef enum {
	TASK_FCALL_CONFIGURE,
	TASK_FCALL_UNCONFIGURE,
	TASK_FCALL_ITERATE,
	TASK_FCALL_ABORT_ITERATION,
	TASK_FCALL_START,
	TASK_FCALL_RESET,
	TASK_FCALL_DONE,							// Called for a FINITE ITERATION Task Controller after reaching a DONE state.
	TASK_FCALL_STOPPED,							// Called when a FINITE  or CONTINUOUS ITERATION Task Controller was stopped manually.
	TASK_FCALL_TASK_TREE_STATUS,				// Called when a Task Controller needs to dim or undim certain module controls and allow/prevent user interaction.
	TASK_FCALL_SET_UITC_MODE,					// Called when an UITC has a parent Task Controller attached to or deattached from it.
	TASK_FCALL_DATA_RECEIVED,					// Called when data is placed in an empty Task Controller data queue, regardless of the Task Controller state.
	TASK_FCALL_ERROR,
	TASK_FCALL_MODULE_EVENT						// Called for custom module events that are not handled directly by the Task Controller
} TaskFCall_type;

//---------------------------------------------------------------
// Task Controller Mode
//---------------------------------------------------------------
typedef enum {
	TASK_CONTINUOUS = FALSE,
	TASK_FINITE 	= TRUE
} TaskMode_type;

//---------------------------------------------------------------
// Task Tree Execution
//---------------------------------------------------------------
typedef enum {
	TASK_TREE_FINISHED 	= FALSE,
	TASK_TREE_STARTED	= TRUE
} TaskTreeExecution_type;

//---------------------------------------------------------------
// Task Controller Iteration Execution Mode
//---------------------------------------------------------------
typedef enum {
	TASK_EXECUTE_BEFORE_SUBTASKS_START,		// The iteration block of the Task Controller is carried out within the call to the provided IterateFptr.
											// IterateFptr is called and completes before sending TASK_EVENT_START to all SubTasks.
	TASK_EXECUTE_AFTER_SUBTASKS_COMPLETE,	// The iteration block of the Task Controller is carried out within the call to the provided IterateFptr.
											// IterateFptr is called after all SubTasks reach TASK_STATE_DONE.
	TASK_EXECUTE_IN_PARALLEL_WITH_SUBTASKS  // The iteration block of the Task Controller is still running after a call to IterateFptr and after a TASK_EVENT_START 
											// is sent to all SubTasks. The iteration block is carried out in parallel with the execution of the SubTasks.
} TaskExecutionMode_type;


typedef struct TaskControl 			TaskControl_type;

//--------------------------------------------------------------------------------
// Task Controller Function Pointer Types
// return 0 for success.
// return negative numbers <-1 in case of error.
//
// The execution of each function when called happens in a separate thread from 
// the main thread (entry thread from OS).
//
// Each Task Controller state machine executes these functions in separate threads 
// from each other.
// 
// Only one of these functions is executed at a time within an individual thread 
// assigned to each Task Controller state machine.
//--------------------------------------------------------------------------------

// Called when a Task Controller needs to be configured based on module or device settings. 
typedef int				 	(*ConfigureFptr_type) 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

// Called when a Task Controller needs to be switched to an unconfigured state which prevents it from being executed.
typedef int				 	(*UnconfigureFptr_type) 		(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 

// Called each time a Task Controller performs an iteration of a device or module. This function is called in a separate thread from the Task Controller thread. 
// The iteration can be completed either within this function call or even in another thread. 
// In either case, to signal back to the Task Controller that the iteration function is complete call TaskControlIterationDone.

typedef void 				(*IterateFptr_type) 			(TaskControl_type* taskControl, BOOL const* abortIterationFlag);

// Called when an iteration must be aborted. This is similar to the use of GetTaskControlAbortIterationFlag except that this function is called back, instead
// of polling a flag during the iteration.
typedef void				(*AbortIterationFptr_type)		(TaskControl_type* taskControl, BOOL const* abortFlag);

// Called before the first iteration starts from an INITIAL state.
typedef int				 	(*StartFptr_type) 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 

// Called when device or module must be returned to its initial state (iteration index = 0).
typedef int				 	(*ResetFptr_type) 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 

// Called automatically when a finite Task Controller finishes required iterations or a continuous Task Controller is stopped manually.
typedef int				 	(*DoneFptr_type) 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 

// Called when a running finite Task Controller is stopped manually, before reaching a DONE state.
typedef int				 	(*StoppedFptr_type) 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 

// Called when a Task Tree in which the Task Controller participates is started or stops/is stopped. This is called before the Start callback of the Task Controller.
typedef int 				(*TaskTreeStatusFptr_type)		(TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo); 

// Called when an UITC has a parent Task Controller attached to or detached from it, in the former case the UITC functioning as a simple Task Controller
// without the possibility for the user to control the Task execution. This function must dim/undim UITC controls that prevent/allow the user to control
// the task execution.
typedef void				(*SetUITCModeFptr_type)			(TaskControl_type* taskControl, BOOL UITCActiveFlag);

// Called when Task Controller encounters an error, to continue Task Controller execution, a return from this function is needed.
typedef void 				(*ErrorFptr_type) 				(TaskControl_type* taskControl, int errorID, char errorMsg[]);

// Called when data is placed in a Task Controller Sink VChan.
typedef int					(*DataReceivedFptr_type)		(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

// Called for passing custom module or device events that are not handled directly by the Task Controller.
typedef int					(*ModuleEventFptr_type)			(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo);


//--------------------------------------------------------------------------------


//======================================== INTERFACE ===================================================================================================
//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller creation/destruction functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

TaskControl_type*  	 	init_TaskControl_type				(const char					taskControllerName[],
															void*						moduleData,
															CmtThreadPoolHandle			tcThreadPoolHndl,
												 	 	 	ConfigureFptr_type 			ConfigureFptr,
															UnconfigureFptr_type		UnconfigureFptr,
												 	 		IterateFptr_type			IterateFptr,
															AbortIterationFptr_type		AbortIterationFptr,
												 		 	StartFptr_type				StartFptr,
												  		 	ResetFptr_type				ResetFptr,
														 	DoneFptr_type				DoneFptr,
														 	StoppedFptr_type			StoppedFptr,
															TaskTreeStatusFptr_type		TaskTreeStatusFptr,
															SetUITCModeFptr_type		SetUITCModeFptr,
														 	ModuleEventFptr_type		ModuleEventFptr,
														 	ErrorFptr_type				ErrorFptr);

	// Disconnects a given Task Controller from its parent, disconnects all its child tasks and HW triggering and then discards the Task Controller.
	// All child Tasks are disconnected from each other including HW triggering dependecies and VChan connections.
void 					discard_TaskControl_type			(TaskControl_type** taskController);

	// Disconnects HW triggering, VChan connections and discards Task Controllers recursively starting with the given taskController
void					discard_TaskTreeBranch				(TaskControl_type** taskController);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller Set/Get functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

	// Sets UI resources for Task Controller logging. Multiple Task Controllers may share the same UI logging resources. 
void					SetTaskControlUILoggingInfo			(TaskControl_type* taskControl, int logPanHandle, int logBoxControlID);

void					EnableTaskControlLogging			(TaskControl_type* taskControl, BOOL enableLogging);

void 					SetTaskControlName 					(TaskControl_type* taskControl, char newName[]);
	// Returns pointer to dynamically allocated Task Controller name (null terminated string) 
char*					GetTaskControlName					(TaskControl_type* taskControl);

	// Return the state of a Task Controller
TaskStates_type			GetTaskControlState					(TaskControl_type* taskControl);
														
	// repeats = 1 by default
void					SetTaskControlIterations			(TaskControl_type* taskControl, size_t repeat);
size_t					GetTaskControlIterations			(TaskControl_type* taskControl);


	// Task Controller Iteration block completion timeout
	// default timeout = 0, Iteration block complete after calling IterateFptr. Otherwise iteration will complete
	// in another thread after a return from a call to IterateFptr. If timeout < 0, Task Controller will wait indefinitely to
	// receive TASK_EVENT_ITERATION_DONE. If timeout > 0, a timeout error is generated if after timeout seconds TASK_EVENT_ITERATION_DONE
	// is not received.
void					SetTaskControlIterationTimeout		(TaskControl_type* taskControl, int timeout);
int						GetTaskControlIterationTimeout		(TaskControl_type* taskControl);

	// Task Controller Iteration Mode
	// default, iterationMode = TASK_EXECUTE_BEFORE_SUBTASKS_START
void					SetTaskControlExecutionMode			(TaskControl_type* taskControl, TaskExecutionMode_type executionMode);
TaskExecutionMode_type	GetTaskControlExecutionMode			(TaskControl_type* taskControl);

// Task Controller Current Iteration    
void					SetTaskControlCurrentIter 			(TaskControl_type* taskControl, Iterator_type* currentIter);
Iterator_type* 			GetTaskControlCurrentIter 			(TaskControl_type* taskControl);
Iterator_type* 			GetTaskControlCurrentIterDup		(TaskControl_type* taskControl);

	// mode = TASK_FINITE by default
void					SetTaskControlMode					(TaskControl_type* taskControl, TaskMode_type mode);
TaskMode_type			GetTaskControlMode					(TaskControl_type* taskControl);

	// Number of seconds to wait between iterations
	// wait = 0 by default
void					SetTaskControlIterationsWait		(TaskControl_type* taskControl, double waitBetweenIterations);
double					GetTaskControlIterationsWait		(TaskControl_type* taskControl);

	// Module specific data associated with the Task Controller
	// moduleData = NULL by default
void					SetTaskControlModuleData			(TaskControl_type* taskControl, void* moduleData);
void*					GetTaskControlModuleData			(TaskControl_type* taskControl);

	// Returns the parent Task Controller, given a Task Controller. If none, returns NULL.
TaskControl_type*		GetTaskControlParent				(TaskControl_type* taskControl);

	// Returns the topmost (root) Task Controller of a given child Task Controller. If it doesn't have any, then it returns the child Task Controller
TaskControl_type*		GetTaskControlRootParent			(TaskControl_type* taskControl);	

	// Creates a list of SubTask Controllers of TaskControl_type* elements, given a Task Controller. 
	// If there are no SubTasks, the list is empty. Use ListDispose to dispose of list.
ListType				GetTaskControlSubTasks				(TaskControl_type* taskControl);

	// Describes whether a Task Controller is meant to be used as an User Interface Task Controller that allows the user to control the execution of a Task Tree.
	// True, TC is of an UITC type, False otherwise. default = False
void					SetTaskControlUITCFlag				(TaskControl_type* taskControl, BOOL UITCFlag);
BOOL					GetTaskControlUITCFlag				(TaskControl_type* taskControl);

	// Returns TRUE if iteration must be stopped
BOOL					GetTaskControlAbortIterationFlag	(TaskControl_type* taskControl);
	//combine iterated data into a one-dimension higher stacked dataset
void 					SetTaskControlStackData 			(TaskControl_type* taskControl, BOOL stack);

BOOL 					GetTaskControlStackData				(TaskControl_type* taskControl);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller composition functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

int						AddSubTaskToParent					(TaskControl_type* parent, TaskControl_type* child);

int						RemoveSubTaskFromParent				(TaskControl_type* child);

	// Disconnects a given Task Controller from its parent, disconnects all child nodes from each other as well as any VChan or HW-triggers
int						DisassembleTaskTreeBranch			(TaskControl_type* taskControlNode); 


//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller management functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

	// Searches for a given Task Controller by name among a list of TaskControl_type* elements. Provide idx if the 1-based list index of the Task Controller is needed,
	// otherwise set this to 0. If no Task Controller is found then the returned value is NULL and idx = 0.
TaskControl_type*		TaskControllerNameExists			(ListType TCList, char TCName[], size_t* idx);

	// Removes a given task Controller from a list of task Controllers of TaskControl_type*. Returns TRUE if given Task Controller was found and removed.
BOOL					RemoveTaskControllerFromList		(ListType TCList, TaskControl_type* taskController);

	// Removes a list of Task Controllers from another list of Task Controllers. Returns TRUE if all Task Controllers to be removed were found and removed, FALSE otherwise.
BOOL					RemoveTaskControllersFromList		(ListType TCList, ListType TCsToBeRemoved);

	// Returns a unique Task Controller name among a given list of Task Controllers. The naming convention uses a given baseTCName and adds a number to the name.
char* 					GetUniqueTaskControllerName			(ListType TCList, char baseTCName[]);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller event posting and execution control functions
//------------------------------------------------------------------------------------------------------------------------------------------------------
														
	// Pass NULL to eventInfo if there is no additional data carried by the event 
	// Pass NULL to disposeEventInfoFptr if eventInfo should NOT be disposed after processing the event
	// 
int 					TaskControlEvent					(TaskControl_type* RecipientTaskControl, TaskEvents_type event, void* eventData, DiscardFptr_type discardEventDataFptr); 

	// Used to signal the Task Controller that an iteration is done.
	// Pass to errorInfo an empty string as "", if there is no error and the iteration completed succesfully. Otherwise,
	// pass an error message string. Pass 0 to errorID if there is no error, otherwise pass an error code. If another iteration is asked besides the already set number of
	// iterations in case of a Finite Task Controller, then pass doAnotherIteration = TRUE. Like so, a finite number of iterations can be performed conditionally.
int						TaskControlIterationDone			(TaskControl_type* taskControl, int errorID, char errorInfo[], BOOL doAnotherIteration);


int						TaskControlEventToSubTasks  		(TaskControl_type* SenderTaskControl, TaskEvents_type event, void* eventData, DiscardFptr_type discardEventDataFptr); 

	// Aborts iterations for the entire Nested Task Controller hierarchy
void					AbortTaskControlExecution			(TaskControl_type* taskControl);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller function call management functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

	// When calling TaskControlEvent and passing a FCallReturn_type* as eventInfo, pass this function pointer to
	// disposeEventInfoFptr
void					dispose_FCallReturn_EventInfo		(void* eventInfo);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller data queue and data exchange functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

	// Adds a VChan to the Task Controller that is used to receive incoming data and binds it to a given callback when data is received.
int						AddSinkVChan						(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr); 

	// Removes a Sink VChan assigned to the Task Controller. Note that this function does not destroy the VChan object nor does it disconnect it from an incoming
	// Source VChan.
int						RemoveSinkVChan 					(TaskControl_type* taskControl, SinkVChan_type* sinkVChan);

	// Removes all Sink VChans assigned to the Task Controller. Note that this function does not destroy the VChan object nor does it disconnect it from an incoming
	// Source VChan. 
int 					RemoveAllSinkVChans 				(TaskControl_type* taskControl);

	// Disconnects Source VChans from all Sink VChans assigned to the Task Controller but does not remove the Sink VChans from the Task Controller.
void					DisconnectAllSinkVChans				(TaskControl_type* taskControl);

	// Clears all data packets from all the Sink VChans of a Task Controller
int						ClearAllSinkVChans					(TaskControl_type* taskControl, char** errorInfo);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __TaskController_H__ */	
	
	
	
