//==============================================================================
//
// Title:		TaskController.h
// Purpose:		Task Controller State Machine.
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

#define TaskControllerUI		"./Framework/Execution control/UI_TaskController.uir" 
#define TC_NEventQueueItems		1000			// Number of events waiting to be processed by the state machine.

// Handy return type for functions that produce error descriptions

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller States
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum {
	TC_State_Unconfigured,						// TC and/or device are not configured and cannot run.
	TC_State_Configured,						// TC is configured for execution but one or more child TCs are not yet configured.
	TC_State_Initial,							// TC is ready to run and has not performed yet any iterations.
	TC_State_Idle,								// TC performed one or more iterations and has been paused. Starting it will continue iterating.
	TC_State_Running,							// TC is active but its iteration function is not yet in progress. 
	TC_State_IterationFunctionActive,			// TC is active and its iteration function is in progress.
	TC_State_Stopping,							// TC is waiting to complete its iteration function and is also waiting for its child TCs to complete their iteration functions.
	TC_State_Done,								// TC is not active and performed one or more iterations. Iterations will start again from TC_State_Initial.
	TC_State_Error								// TC encountered an error and cannot run.
} TCStates;

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller Events
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum {
	TC_Event_Unconfigure,						// Unconfigures the device/module and the TC cannot be executed.
	TC_Event_Configure,							// Configures the TC and its child TCs for execution.
	TC_Event_Start,								// Starts, restarts or resumes TC iterations.
	TC_Event_IterateOnce,						// Performs only one TC iteration.
	TC_Event_Iterate,							// [Internal event] Used to signal the TC that another iteration is needed.
	TC_Event_IterationDone,						// Used to signal that the iteration function of a TC finished execution.
	TC_Event_IterationTimeout,					// [Internal event] Generated when a TC iteration functions takes too long to execute.
	TC_Event_Reset,								// Returns the TC back to its Initial state.
	TC_Event_Stop,								// Waits until iteration functions in progress finish and stops further TC iterations for all TCs in a Task Tree.
	TC_Event_UpdateChildTCState,   				// [Internal event] A parent TC is informed of the new state of one of its child TCs.
	TC_Event_DataReceived,						// [Internal event] Event generated when data is written to a Sink VChan registered with the TC.
	TC_Event_Custom,							// Used to signal custom device/module events.
	TASK_EVENT_SUBTASK_ADDED_TO_PARENT,			// When a SubTask is added to a parent Task Controller
	TASK_EVENT_SUBTASK_REMOVED_FROM_PARENT		// When a SubTask is disconnected from a parent task Controller
} TCEvents;

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller Callbacks
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum {
	TC_Callback_Configure,						// Configures the TC for execution.
	TC_Callback_Unconfigure,					// Makes TC unavailable for execution. 
	TC_Callback_Iterate,						// Main TC function called with each TC iteration.
	TC_Callback_Start,							// Called before a TC starts executing.
	TC_Callback_Reset,							// Returns the TC to its Initial state.
	TC_Callback_Done,							// Called when either a Finite TC finished all its iterations or when a Continuous TC was stopped.
	TC_Callback_Stopped,						// Called when a TC was stopped manually.
	TC_Callback_IterationStop,					// Called when a TC receives a TC_Event_Stop event while its iteration function is active. Use this carefully in conjuction with GetTaskControlIterationStopFlag () to stop ongoing processes and terminate the iteration.
	TASK_FCALL_TASK_TREE_STATUS,				// Called when a Task Controller needs to dim or undim certain module controls and allow/prevent user interaction.
	TASK_FCALL_SET_UITC_MODE,					// Called when an UITC has a parent Task Controller attached to or deattached from it.
	TC_Callback_DataReceived,					// Called when data is written to a Sink VChan registered with the TC.
	TC_Callback_Error,							// Called when a TC encountered an error.
	TC_Callback_CustomEvent						// Called for custom events that are not handled by the TC.
} TCCallbacks;

//---------------------------------------------------------------
// Task Controller Mode
//---------------------------------------------------------------
typedef enum {
	TASK_CONTINUOUS 	= FALSE,
	TASK_FINITE 		= TRUE
} TaskMode_type;

//---------------------------------------------------------------
// Task Tree Execution
//---------------------------------------------------------------
typedef enum {
	TaskTree_Finished 	= FALSE,
	TaskTree_Started	= TRUE
} TaskTreeStates;

//---------------------------------------------------------------
// Task Controller Iteration Function Execution Mode
//---------------------------------------------------------------
typedef enum {
	TC_Execute_BeforeChildTCs,					// The iteration function of the TC is carried out before starting child TCs.
	TC_Execute_AfterChildTCsComplete,			// The iteration function of the TC is carried out after all child TC iterations are done.
	TC_Execute_InParallelWithChildTCs  			// The iteration function of the TC is carried out in parallel with the iterations of the child TCs.
} TCExecutionModes;


typedef struct TaskControl 		TaskControl_type;

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
typedef int				(*ConfigureFptr_type) 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

// Called when a Task Controller needs to be switched to an unconfigured state which prevents it from being executed.
typedef int				(*UnconfigureFptr_type) 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 

// Called each time a Task Controller performs an iteration of a device or module. This function is called in a separate thread from the Task Controller thread. 
// The iteration can be completed either within this function call or even in another thread. 
// In either case, to signal back to the Task Controller that the iteration function is complete call TaskControlIterationDone.

typedef void 			(*IterateFptr_type) 				(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);

// Called before the first iteration starts from an INITIAL state.
typedef int				(*StartFptr_type) 					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 

// Called when device or module must be returned to its initial state (iteration index = 0).
typedef int				(*ResetFptr_type) 					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 

// Called automatically when a finite Task Controller finishes required iterations or a continuous Task Controller is stopped manually.
typedef int				(*DoneFptr_type) 					(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg); 

// Called when a running finite Task Controller is stopped manually, before reaching a DONE state.
typedef int				(*StoppedFptr_type) 				(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg); 

// Called when the task controller receives a TC_Event_Stop while its iteration function is active and needs to terminate its iteration. After shutting down ongoing
// operations, call if necessary TaskControlIterationDone ().
// Note: This is similar to using GetTaskControlIterationStopFlag () which returns a flag. Do not make use of both methods at the same time to stop an ogoing process.

typedef int				(*IterationStopFptr_type) 			(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg); 

// Called when a Task Tree in which the Task Controller participates is started or stops/is stopped. This is called before the Start callback of the Task Controller.
typedef int 			(*TaskTreeStateChangeFptr_type)		(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg); 

// Called when an UITC has a parent Task Controller attached to or detached from it, in the former case the UITC functioning as a simple Task Controller
// without the possibility for the user to control the Task execution. This function must dim/undim UITC controls that prevent/allow the user to control
// the task execution.
typedef void			(*SetUITCModeFptr_type)				(TaskControl_type* taskControl, BOOL UITCActiveFlag);

// Called when Task Controller encounters an error, to continue Task Controller execution, a return from this function is needed.
typedef void 			(*ErrorFptr_type) 					(TaskControl_type* taskControl, int errorID, char errorMsg[]);

// Called when data is placed in a Task Controller Sink VChan.
typedef int				(*DataReceivedFptr_type)			(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);

// Called for passing custom module or device events that are not handled directly by the Task Controller.
typedef int				(*ModuleEventFptr_type)				(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg);


//--------------------------------------------------------------------------------


//======================================== INTERFACE ===================================================================================================
//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller creation/destruction functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

TaskControl_type*  	 	init_TaskControl_type				(const char						taskControllerName[],
															 void*							moduleData,
															 CmtThreadPoolHandle			tcThreadPoolHndl,
												 	 	 	 ConfigureFptr_type 			ConfigureFptr,
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
														 	 ErrorFptr_type					ErrorFptr);

	// Disconnects a given Task Controller from its parent, disconnects all its child tasks and HW triggering and then discards the Task Controller.
	// All child Tasks are disconnected from each other including HW triggering dependecies and VChan connections.
void 					discard_TaskControl_type			(TaskControl_type** taskControllerPtr);

	// Disconnects HW triggering, VChan connections and discards Task Controllers recursively starting with the given taskController
void					discard_TaskTreeBranch				(TaskControl_type** taskControllerPtr);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller Set/Get functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

	// Sets UI resources for Task Controller logging. Multiple Task Controllers may share the same UI logging resources. 
void					SetTaskControlUILoggingInfo			(TaskControl_type* taskControl, int logPanHandle, int logBoxControlID);

void					EnableTaskControlLogging			(TaskControl_type* taskControl, BOOL enableLogging);

void 					SetTaskControlName 					(TaskControl_type* taskControl, char newName[]);
	// Returns pointer to dynamically allocated Task Controller name (null terminated string) 
char*					GetTaskControlName					(TaskControl_type* taskControl);

	// Obtains the current state of a Task Controller. A call to this function blocks ongoing state transitions and if TC state knowledge is not needed anymore,
	// this call must be followed each time it is called by GetTaskControlState_ReleaseLock. On success returns 0, on failure a negative number.
int 					GetTaskControlState_GetLock 		(TaskControl_type* taskControl, TCStates* tcStatePtr, BOOL* lockObtained, int lineNumDebug, char fileNameDebug[], char** errorMsg);
	// Releases GetTaskControlState lock so state transitions may resume. On success returns 0 and lockObtained is set back to FALSE. On failure returns a negative value and lockObtained remains TRUE. 
int						GetTaskControlState_ReleaseLock		(TaskControl_type* taskControl, BOOL* lockObtained, char** errorMsg);

														
	// repeats = 1 by default
void					SetTaskControlIterations			(TaskControl_type* taskControl, size_t repeat);
size_t					GetTaskControlIterations			(TaskControl_type* taskControl);


	// Task Controller Iteration Function completion timeout. Default timeout = 10 sec. If timeout < 0, the Task Controller will wait indefinitely to
	// receive TC_Event_IterationDone. If timeout > 0, a timeout error is generated if after timeout seconds TC_Event_IterationDone is not received.
void					SetTaskControlIterationTimeout		(TaskControl_type* taskControl, unsigned int timeout);
unsigned int			GetTaskControlIterationTimeout		(TaskControl_type* taskControl);

	// Task Controller Iteration Mode
	// default, iterationMode = TC_Execute_BeforeChildTCs
void					SetTaskControlExecutionMode			(TaskControl_type* taskControl, TCExecutionModes executionMode);
TCExecutionModes		GetTaskControlExecutionMode			(TaskControl_type* taskControl);

// Task Controller Current Iteration    
void					SetTaskControlIterator 				(TaskControl_type* taskControl, Iterator_type* currentIter);
Iterator_type* 			GetTaskControlIterator 				(TaskControl_type* taskControl);

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
ListType				GetTaskControlChildTCs				(TaskControl_type* taskControl);

	// Describes whether a Task Controller is meant to be used as an User Interface Task Controller that allows the user to control the execution of a Task Tree.
	// True, TC is of an UITC type, False otherwise. default = False
void					SetTaskControlUITCFlag				(TaskControl_type* taskControl, BOOL UITCFlag);
BOOL					GetTaskControlUITCFlag				(TaskControl_type* taskControl);

	// Returns TRUE if iteration function must be aborted
BOOL					GetTaskControlAbortFlag				(TaskControl_type* taskControl);

	// Returns TRUE if an active iteration received a TC_Event_Stop and the iteration must be stopped. After shutting down ongoing
	// operations, call if necessary TaskControlIterationDone ().
	// Note: This is similar to using the IterationStopFptr_type callback. Do not make use of both methods at the same time to stop an ogoing process.
BOOL					GetTaskControlIterationStopFlag		(TaskControl_type* taskControl);

	// Checks if a Task Controller is in use, i.e. if it is in any of the following states: Idle, Running, IterationFunctionActive, Stopping.
	// A call to this function blocks ongoing state transitions and if TC state knowledge is not needed anymore, this call must be followed each time 
	// it is called by GetTaskControlState_ReleaseLock. On success returns 0 and sets lockObtained to TRUE, on failure returns a negative number and sets lockObtained to FALSE.
int 					IsTaskControllerInUse_GetLock 		(TaskControl_type* taskControl, BOOL* inUsePtr, TCStates* tcState, BOOL* lockObtained, int lineNumDebug, char fileNameDebug[], char** errorMsg);
	// Releases IsTaskControllerInUse lock so state transitions maye resume. On success returns 0 and lockObtained is set back to FALSE. On failure returns a negative value and lockObtained remains TRUE. 
int 					IsTaskControllerInUse_ReleaseLock 	(TaskControl_type* taskControl, BOOL* lockObtained, char** errorMsg);

	// Check if the task tree of a given task controller is in use, i.e. if the root task controller is in any of the following states: Idle, Running, IterationFunctionActive, Stopping.
	// A call to this function blocks ongoing state transitions and if task tree state knowledge is not needed anymore, this call must be followed each time 
	// it is called by IsTaskTreeInUse_ReleaseLock. On success returns 0 and sets lockObtained to TRUE, on failure returns a negative number and sets lockObtained to FALSE.
int 					IsTaskTreeInUse_GetLock 			(TaskControl_type* taskControl, BOOL* inUsePtr, TCStates* tcState, BOOL* lockObtained, int lineNumDebug, char fileNameDebug[], char** errorMsg);
	// Releases IsTaskTreeInUse lock so state transitions maye resume. On success returns 0 and lockObtained is set back to FALSE. On failure returns a negative value and lockObtained remains TRUE.
int 					IsTaskTreeInUse_ReleaseLock 		(TaskControl_type* taskControl, BOOL* lockObtained, char** errorMsg);

	// Converts a task Controller state ID into a string
char*					TaskControlStateToString			(TCStates state);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller composition functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

int						AddChildTCToParent					(TaskControl_type* parentTC, TaskControl_type* childTC, char** errorMsg);

int						RemoveChildTCFromParentTC			(TaskControl_type* childTC, char** errorMsg);

	// Disconnects a given Task Controller from its parent, disconnects all child nodes from each other as well as any VChan or HW-triggers
int						DisassembleTaskTreeBranch			(TaskControl_type* taskControlNode, char** errorMsg); 


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
														
	// Pass NULL to eventData if there is no additional data carried by the event 
	// Pass NULL to discardEventDataFptr if eventData should be discarded using free().
int 					TaskControlEvent					(TaskControl_type* RecipientTaskControl, TCEvents event, void** eventDataPtr, DiscardFptr_type discardEventDataFptr, char** errorMsg); 

	// Used to signal the Task Controller that an iteration is done.
	// Pass to errorInfoString an empty string as "", if there is no error and the iteration completed succesfully. Otherwise,
	// pass an error message string. Pass 0 to errorID if there is no error, otherwise pass an error code. If another iteration is asked besides the already set number of
	// iterations in case of a Finite Task Controller, then pass doAnotherIteration = TRUE. Like so, a finite number of iterations can be performed conditionally.
int						TaskControlIterationDone			(TaskControl_type* taskControl, int errorID, char errorInfoString[], BOOL doAnotherIteration, char** errorMsg);


int						TaskControlEventToChildTCs 			(TaskControl_type* SenderTaskControl, TCEvents event, void** eventDataPtr, DiscardFptr_type discardEventDataFptr, char** errorMsg);

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
int						AddSinkVChan						(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, DataReceivedFptr_type DataReceivedFptr, char** errorMsg); 
	// Removes a Sink VChan assigned to the Task Controller. Note that this function does not destroy the VChan object nor does it disconnect it from an incoming
	// Source VChan.
int						RemoveSinkVChan 					(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, char** errorMsg);

	// Removes all Sink VChans assigned to the Task Controller. Note that this function does not destroy the VChan object nor does it disconnect it from an incoming
	// Source VChan. 
int 					RemoveAllSinkVChans 				(TaskControl_type* taskControl, char** errorMsg);

	// Associates a SourceVChan with the Task Controller
int						AddSourceVChan						(TaskControl_type* taskControl, SourceVChan_type* sourceVChan, char** errorMsg);

	// Removes a Source VChan assigned to the Task Controller. Note that this function does not destroy the VChan object.
int						RemoveSourceVChan					(TaskControl_type* taskControl, SourceVChan_type* sourceVChan, char** errorMsg);

	// Removes all Source VChans assigned to the Task Controller. Note that this function does not destroy the VChan objects.
int						RemoveAllSourceVChan				(TaskControl_type* taskControl, char** errorMsg);

	// Disconnects Source VChans from all Sink VChans assigned to the Task Controller but does not remove the Sink VChans from the Task Controller.
int						DisconnectAllSinkVChans				(TaskControl_type* taskControl, char** errorMsg);

	// Disconnects Sink VChans from all Source VChans assigned to the Task Controller but does not remove the Source VChans from the Task Controller.
int						DisconnectAllSourceVChans			(TaskControl_type* taskControl, char** errorMsg);

	// Clears all data packets from all the Sink VChans of a Task Controller
int						ClearAllSinkVChans					(TaskControl_type* taskControl, char** errorMsg);


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __TaskController_H__ */	
	
	
	
