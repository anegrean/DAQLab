//==============================================================================
//
// Title:		TaskController.h
// Purpose:		A short description of the implementation.
//
// Created on:	2014.05.11. at 10:35:38 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

/*=====================================================================================================================================
Nested Task Controller
=======================================================================================================================================

A Task Controller is an automation engine for executing user provided function pointers that control the
behavior of a device or module.

There are two types of Task Controllers: a) finite and b) continuous, depending on the number of iterations
required to complete a task. The goal in carrying out a task is to move from the INITIAL state to the DONE state 
once a START event is received and call the appropriate function pointer with each iteration.

From the user perspective, a Task Controller receives the following types of events: START, STOP and PAUSE.
A START event begins (if in INITIAL state) or continues (if in IDLE state) the iterations of a Task Controller until:

- a DONE state is reached in the case of a finite Task Controller when all iterations are complete. 
- a STOP button is pressed in the case of both finite and continuous Task Controllers which ends up in a DONE state.
- a PAUSE button is pressed in the case of both finite and continuous Task Controllers which ends up in an IDLE state. 

Commonly, complex tasks require the coordination of multiple modules or devices. A complex task can be
modelled as a series of nested tasks, in which case the execution state of a parent task and child tasks are
closely linked together. In the following, several examples will be discussed:

------------------------
1. Z-Stack Acquisition |
------------------------

In laser scanning microscopy, when acquiring a stack of images at different
depths from a fluorescently labelled sample, this complex task may be represented as follows:

START/STOP/CONFIG 
	|- Z Stack (1)
	:  		 |- Z Stage (N)
	:		 		  |- DAQ Device (1)
					  |- PMT Data (1)
	:  		   		  |- Scan Engine (1) 	
	:  		
	:- Display (x) 
The execution of this complex task is as follows. The parent "Z Stack" task is executed once and its completion,
i.e. reaching a DONE state, depends on all its subtasks, in this case the "Z Stage" and "Display" SubTask Controllers to reach 
a DONE state.

As it is shown, for "Z Stage" to reach a DONE state, the task must be completed N times, corresponding to N different positions
at which the microscope objective must be placed to acquire a framescan. At the second nesting level from the parent "Z Stack", 
the generation of a frame scan is controlled by the "DAQ Device" and "Scan Engine" which are all carried out one time inside the 
"Z Stage" Task Control loop. "Scan Engine", when iterated once, calculates galvo mirror scan signals which are sent to "DAQ Device" 
that controls the galvo mirrors. "Scan Engine" does not complete its iteration until it receives all pixel data from "DAQ Device" 
after which it can send a complete image to "Display" and by doing so, "Scan Engine" reaches a DONE state. While "Z-Stage" is a 
Task Controller with a finite N iterations, "Display" is a continuous Task Controller that requires a STOP command to switch to a DONE state.
This STOP command is provided by the "Z Stage" once it reaches a DONE state.

----------------------------------------------
2. Continuous Frame Scan Acquisition (Movie) |
----------------------------------------------

START/STOP/CONFIG 
	|- Movie (1)
	:	   |- DAQ Device (1)        
	:      |- Scan Engine (?)	  i.e. the number of iterations is not predefined 
	:
	:- Display (x)
	 
-----------------------------------------
3. 2P uncaging and dendritic inhibition |
-----------------------------------------

START/STOP/CONFIG
	|- Dendritic Mapping (5)													Repeat experiment 5x at certain intervals
	:    			   |- Pockells Module (10)									Intensity of uncaging beam is changed with each iteration
	:				                    |- Laser Modulator (5)					Timing of laser modulator is changed with each iteration
	:			                    	 				 |- DAQ Device 1 (1)	Galvo command, Pockells uncaging command
	:				   						  			 |- DAQ Device 2 (1)    Laser modulation, Ephys command and recording
	:				     		 				  		 |- Scan Engine (1)
	:			  	   		 				  			 |- EPhys Module (1)
	:			 	   		  
	:
	:- Display (x)


DATA FLOW

"Scan Engine" generates waveforms to position the galvos in the uncaging location, which may be either a point or a line. If it is a line,
the uncaging may occur along the line and there may be multiple line scans at the same location. "Scan Engine" then sends the waveform to position 
the galvos to "DAQ Device 1". "Scan Engine" also generates a digital waveform that corresponds to the ON/OFF pattern for the photostimulation which
is synced with the galvo control waveforms. This digital uncaging waveform is sent to "Pockells Module" which transforms it into an analog waveform 
that controls the Pockells cell. "Pockells Module" then sends this analog waveform to "DAQ Device 1". "EPhys Module" sends analog waveform command 
to "DAQ Device 2" with e.g. the holding potential of the cell and receives from "DAQ Device 2" analog waveform input containing the cell's membrane 
potential or holding current. "Laser Modulator" sends analog waveform to "DAQ Device 2" that controls the intensity of the wide-field laser 
illumination. Finally, "EPhys Module" sends to "Display" the processed analog waveform it received from "DAQ Device 2". Also "Pockells Module" 
and "Laser Modulator" send the uncaging and laser modulation waveform to "Display". 

TASK CONTROLER EXECUTION  

All Task Controllers start in an UNCONFIGURED state. The user sends a CONFIGURE command to "Dendritic Mapping" which calls its configuration 
function and forwards the CONFIGURE command to its SubTask "Pockells Module". Since "Dendritic Mapping" has a SubTask which at 
this point is not in an INITIAL state, it switches to a CONFIGURED state. Later, it will switch to an INITIAL state when "Pockells Module" 
will switch to an INITIAL state. 

At this point all Task Controllers are in the INITIAL state. Sending a START command to "Dendritic Mapping" will do the following:
1. 	"Dendritic Mapping" starting function pointer will be called:
		- cleanup of queues to make sure there are no data packets left

2. 	"Dendritic Mapping" iteration function pointer will be called:
		- no functionality
		
3. 	If iteration was succesful then iteration index will be incremented.

4. 	If there are SubTask Controllers, the START event is forwarded to its SubTasks, in this case "Pockells Module".

5. 	If more iterations are needed, state will be switched to RUNNING, otherwise to DONE. If there are no SubTasks (not this case) and
	more iterations are needed then ITERATE event is posted to self and another iteration is performed while in the RUNNING state.

At this point "Dendritic Mapping" is in a RUNNING state, while the rest of the Task Controllers are in an INITIAL state. "Pockells Module"
receives a START event and undergoes the same events as "Dendritic Mapping" and switches to a RUNNING state. 

For the "Pockells Module" the function pointers from steps 1 and 2 will do the following:

1. 	"Pockells Module" starting function pointer will be called:
		- no functionality

2. 	"Pockells Module" iteration function pointer will be called:
		- increase level of beam intensity which is applied to each digital waveform uncaging data packet that is received.
		
3. 	If iteration successful, iteration index is incremented

4.	Send START event to "Laser Modulator"

5.	"Pockells Module" state switched to RUNNING 
		
For the "Laser Modulator" the function pointers from steps 1 and 2 will do the following:

1.	"Laser Modulator" starting function pointer will be called:
		- opens beam block/shutter
		
2.	"Laser Modulator" iteration function pointer will be called:
		- with each iteration shifts phase of sine wave modulation and sends new waveform data packet to "DAQ Device 2"
		
3.	If iteration was succesful then iteration index will be incremented. 

4.	Send START event to "DAQ Device 1", "DAQ Device 2", "Scan Engine", "Ephys Module"

5.	"Laser Modulator" state switched to RUNNING

At this point "Dendritic Mapping", "Pockells Module" and "Laser Modulator" are running and the other Task Controllers are in their INITIAL state.

In the diagram above, when the SubTasks of "laser Modulator" receive the START command, each SubTask Controller will process this command in a
separate thread, thus the four Task Controllers "DAQ Device 1", "DAQ Device 2", "Scan Engine" and "Ephys Module" run in parallel in separate threads.

When "DAQ Device 1" receives the START command and switches from the INITIAL to DONE state (1 iteration), it performs a similar sequence of actions as before:

1. 	"DAQ Device 1" starting function pointer will be called:
		- no functionality

2. 	"DAQ Device 1" iteration function pointer will be called:
		- Waits until there is data available in the output buffer. This data has to be received from "Scan Engine" and "Pockells Module".
		- Starts the DAQmx Tasks and waits for their completion. Since DAQmx tasks are carried out in separate threads, to signal completion of the iteration
		when DAQmx finished, the Task Controller thread is kept idle at this point and execution of the Task Controller resumes once 
		ContinueTaskControlExecution is called when DAQmx finishes. 
		(It is unclear here how a finite data generation is set up, and how to handle hardware triggerable tasks)
																	 
3. 	If iteration successful, iteration index is incremented, and since there are no more iterations (RUNNING state is not reached):
		- done function pointer is called
		- "DAQ Device 1" state is switched to DONE  


When "DAQ Device 2" receives the START command:

1.	"DAQ Device 2" starting function pointer will be called:
		- no functionality
		
2.	"DAQ Device 2" iteration function pointer will be called:
		- Waits until there is data available in the output buffer. This data has to be received from "Laser Modulator" and "EPhys Module".
		- Starts the DAQmx Tasks and waits for their completion. Since DAQmx tasks are carried out in separate threads, to signal completion of the iteration
		when DAQmx finished, the Task Controller thread is kept idle at this point and execution of the Task Controller resumes once 
		ContinueTaskControlExecution is called when DAQmx finishes. 
		(It is unclear here how a finite data generation is set up, and how to handle hardware triggerable tasks)
		
3. 	If iteration successful, iteration index is incremented, and since there are no more iterations (RUNNING state is not reached):
		- done function pointer is called
		- "DAQ Device 2" state is switched to DONE  

NOTE: It is unclear how to resolve hardware timing issues between DAQ Devices and how to represent this in the Nested Task Controller scheme.
IDEA: Introduce choice to iterate Task Controller before or after sending a START command to its subtask and perhaps the option to serialize
Task Controllers as now SubTasks are executed in parallel. Or instead of blocking the Thread and Task Controller while waiting for the iteration to
complete, create a new state to which the Task Controller can switch in the meantime like TASK_STATE_RUNNING_WAITING_FOR_ITERATION. Then there can
be an option to send a START event while waiting for iteration completion, or after the iteration completed.

When "Scan Engine" receives the START command:

1. "Scan Engine" starting function pointer will be called:
		- no functionality
		
2. "Scan Engine" iteration function pointer will be called:
		- send data packet with galvo position to "DAQ Device 1" and data packet with uncaging modulation to "Pockells Module".
		
3.	If iteration successful, iteration index is incremented and "Scan Engine" state is switched to DONE (RUNNING state is not reached). 

When "EPhys Module" receives the START command:

1. "EPhys Module" starting function pointer will be called:
		- no functionality
		
2. "EPhys Module" iteration function pointer will be called:
		- waits until it receives data from "DAQ Device 2" and sends data packet to "Display" with the EPhys recording.
		
3. If iteration successful, iteration index is incremented and "DAQ Device 2" state is switched to DONE (RUNNING state is not reached).

The Task Controllers at this point are in the following states:

"Dendritic Mapping"	- RUNNING
"Pockells Module"	- RUNNING
"Laser Modulator"	- RUNNING
"DAQ Device 1"		- INITIAL (until iteration is complete, actually there should be a more clear way to describe the state of waiting for data)
"DAQ Device 2"		- INITIAL (same issue as for DAQ Device 1)
"Scan Engine"		- DONE
"EPhys Module"		- DONE

Eventually "DAQ Device 1" receives data with the galvo positions from "Scan Engine" and with the uncaging waveform from "Pockells Module" which in 
turn received the data also from "Scan Engine" and switches to a DONE state. Similarly "DAQ Device 2" receives its data from "Laser Modulator" and
"EPhys module" and before switching to a DONE state, sends a data packet with the cell's membrane potential response to "Display".

When the last SubTask of "Laser Modulator" reaches a DONE state, "Laser Modulator" is informed of this state change and checks if more iterations are 
needed. If yes, then stays in a RUNNING state and sends to self an ITERATE event. Otherwise, it switches to a DONE state and calls its done function
pointer. Similarly, when "Pockells Module" and "Dendritic Mapping" finish their iterations, they switch to a DONE state.



========================================================================================================================================*/


#ifndef __TaskController_H__
#define __TaskController_H__

#ifdef __cplusplus
    extern "C" {
#endif

#include "DAQLabUtility.h"
#include <toolbox.h>
#include "VChannel.h"

#define TASKCONTROLLER_UI		"./Framework/Execution control/UI_TaskController.uir" 
#define N_TASK_EVENT_QITEMS		100		// Number of events waiting to be processed by the state machine.

// Handy return type for functions that produce error descriptions

//----------------------------------------
// Task Controller States
//----------------------------------------
typedef enum {
	TASK_STATE_UNCONFIGURED,					// Task Controller is not configured.
	TASK_STATE_CONFIGURED,						// Task Controller is configured.
	TASK_STATE_INITIAL,							// Initial state of Task Controller before any iterations are performed.
	TASK_STATE_IDLE,							// Task Controller is configured.
	TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES,   // A HW Master Trigger Task Controller is waiting for HW Slave Trigger Task Controllers to be armed.
	TASK_STATE_RUNNING,							// Task Controller is being iterated until required number of iterations is reached (if finite)  or stopped.
	TASK_STATE_RUNNING_WAITING_ITERATION,		// Task Controller is iterating but the iteration (possibly occuring in another thread) is not yet complete.
												// Iteration is complete when a TASK_EVENT_ITERATION_DONE is received in this state.
	TASK_STATE_STOPPING,						// Task Controller received a STOP event and waits for SubTasks to complete their iterations.
	TASK_STATE_DONE,							// Task Controller finished required iterations if operation was finite
	TASK_STATE_ERROR
} TaskStates_type;

//----------------------------------------
// Task Controller Events
//----------------------------------------
typedef enum {
	TASK_EVENT_CONFIGURE,
	TASK_EVENT_START,							// Starts a Task Controller that is in an IDLE or PAUSED state. 
												// In case of an IDLE state, the Task Controller returns the module or
												// device to its initial state (iteration index = 0). In case of a PAUSED
												// state, it continues iterating the module or device.
	TASK_EVENT_ITERATE,							// Used to signal that another iteration is needed while in a RUNNING state. This may be signalled
												// by the Task Controller or another thread if the iteration occurs in another thread.
	TASK_EVENT_ITERATION_DONE,					// Used to signal that an iteration completed if it was not carried out in the same thread as the call to TASK_FCALL_ITERATE.
	TASK_EVENT_ITERATION_TIMEOUT,				// Generated when TASK_EVENT_ITERATION_DONE is not received after a given timeout. 
	TASK_EVENT_ONE_ITERATION,					// Performs only one iteration of the Task Controller in an IDLE or DONE state.
	TASK_EVENT_RESET,							// Resets iteration index to 0, calls given reset function and brings State Controller 
												// back to INITIAL state.
	TASK_EVENT_STOP,							// Stops Task Controller iterations and allows SubTask Controllers to complete their iterations.
	TASK_EVENT_STOP_CONTINUOUS_TASK,			// Event sent from parent Task Controller to its continuous SubTasks to stop them.
	TASK_EVENT_SUBTASK_STATE_CHANGED,   		// When one of the SubTask Controllers switches to another state.
	TASK_EVENT_HWTRIG_SLAVE_ARMED,				// When a Slave HW Trig Task Controller is armed, it informs the Master HW Triggering Task Controller. 
	TASK_EVENT_DATA_RECEIVED,					// When data is placed in an otherwise empty data queue of a Task Controller.
	TASK_EVENT_ERROR_OUT_OF_MEMORY,				// To signal that an out of memory event occured.
	TASK_EVENT_CUSTOM_MODULE_EVENT,				// To signal custom module or device events.
	TASK_EVENT_DIM_UI
} TaskEvents_type;

//----------------------------------------
// Task Controller Function Calls
//----------------------------------------
typedef enum {
	TASK_FCALL_NONE,
	TASK_FCALL_CONFIGURE,
	TASK_FCALL_ITERATE,
	TASK_FCALL_START,
	TASK_FCALL_RESET,
	TASK_FCALL_DONE,					// Called for a FINITE ITERATION Task Controller after reaching a DONE state.
	TASK_FCALL_STOPPED,					// Called when a FINITE  or CONTINUOUS ITERATION Task Controller was stopped manually.
	TASK_FCALL_DIM_UI,					// Called when a Task Controller needs to dim or undim certain module controls and allow/prevent user interaction.
	TASK_FCALL_DATA_RECEIVED,			// Called when data is placed in an empty Task Controller data queue, regardless of the Task Controller state.
	TASK_FCALL_ERROR,
	TASK_FCALL_MODULE_EVENT				// Called for custom module events that are not handled directly by the Task Controller
} TaskFCall_type;

//---------------------------------------------------------------
// Task Controller Mode
//---------------------------------------------------------------
typedef enum {
	TASK_CONTINUOUS,  // == FALSE
	TASK_FINITE		  // == TRUE
} TaskMode_type;

//---------------------------------------------------------------
// Task Controller Iteration Execution Mode
//---------------------------------------------------------------
typedef enum {
	TASK_ITERATE_BEFORE_SUBTASKS_START,		// The iteration block of the Task Controller is carried out within the call to the provided IterateFptr.
											// IterateFptr is called and completes before sending TASK_EVENT_START to all SubTasks.
	TASK_ITERATE_AFTER_SUBTASKS_COMPLETE,	// The iteration block of the Task Controller is carried out within the call to the provided IterateFptr.
											// IterateFptr is called after all SubTasks reach TASK_STATE_DONE.
	TASK_ITERATE_IN_PARALLEL_WITH_SUBTASKS  // The iteration block of the Task Controller is still running after a call to IterateFptr and after a TASK_EVENT_START 
											// is sent to all SubTasks. The iteration block is carried out in parallel with the execution of the SubTasks.
} TaskIterMode_type;

//---------------------------------------------------------------
// Task Controller Virtual Channel Function Assignment
//---------------------------------------------------------------
typedef enum {
	TASK_VCHAN_FUNC_NONE,
	TASK_VCHAN_FUNC_START,
	TASK_VCHAN_FUNC_ITERATE,
	TASK_VCHAN_FUNC_DONE
} TaskVChanFuncAssign_type;
	

//---------------------------------------------------------------
// Task Controller HW Triggerring (for both Slaves and Masters)
//---------------------------------------------------------------
typedef enum {
	TASK_NO_HWTRIGGER,
	TASK_MASTER_HWTRIGGER,
	TASK_SLAVE_HWTRIGGER
} HWTrigger_type;

typedef struct TaskControl 			TaskControl_type;
typedef struct TaskExecutionLog		TaskExecutionLog_type;

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
typedef FCallReturn_type* 	(*ConfigureFptr_type) 			(TaskControl_type* taskControl, BOOL const* abortFlag);

// Called each time a Task Controller performs an iteration of a device or module
// This function is called in a separate thread from the Task Controller thread. The iteration can be completed either within this function call
// or even in another thread. In either case, to signal back to the Task Controller that the iteration function is complete, send a
// TASK_EVENT_ITERATION_DONE event using TaskControlEvent and passing for eventInfo init_FCallReturn_type (...) and for disposeEventInfoFptr
// discard_FCallReturn_type.
typedef void 				(*IterateFptr_type) 			(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

// Called before the first iteration starts from an INITIAL state.
typedef FCallReturn_type* 	(*StartFptr_type) 				(TaskControl_type* taskControl, BOOL const* abortFlag); 

// Called when device or module must be returned to its initial state (iteration index = 0).
typedef FCallReturn_type* 	(*ResetFptr_type) 				(TaskControl_type* taskControl, BOOL const* abortFlag); 

// Called automatically when a finite Task Controller finishes required iterations or a continuous Task Controller is stopped manually.
typedef FCallReturn_type* 	(*DoneFptr_type) 				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag); 

// Called when a running finite Task Controller is stopped manually, before reaching a DONE state .
typedef FCallReturn_type* 	(*StoppedFptr_type) 			(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag); 

// Called when a Task Controller needs to dim or undim certain module controls to allow/prevent user interaction.
typedef void 				(*DimUIFptr_type)	 			(TaskControl_type* taskControl, BOOL dimmed); 

// Called when Task Controller encounters an error, to continue Task Controller execution, a return from this function is needed.
typedef void 				(*ErrorFptr_type) 				(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);

// Called when data is placed in a Task Controller Sink VChan.
typedef FCallReturn_type*	(*DataReceivedFptr_type)		(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag);

// Called for passing custom module or device events that are not handled directly by the Task Controller.
typedef FCallReturn_type*	(*ModuleEventFptr_type)			(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);

// Called after receiving a task control event and eventInfo must be disposed of.
typedef void				(*DisposeEventInfoFptr_type)	(void* eventInfo);



//--------------------------------------------------------------------------------


//======================================== INTERFACE ===================================================================================================
//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller creation/destruction functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

TaskControl_type*  	 	init_TaskControl_type				(const char				taskname[], 
												 	 	 	ConfigureFptr_type 		ConfigureFptr,
												 	 		IterateFptr_type		IterateFptr,
												 		 	StartFptr_type			StartFptr,
												  		 	ResetFptr_type			ResetFptr,
														 	DoneFptr_type			DoneFptr,
														 	StoppedFptr_type		StoppedFptr,
															DimUIFptr_type			DimUIFptr,
														 	DataReceivedFptr_type	DataReceivedFptr,
														 	ModuleEventFptr_type	ModuleEventFptr,
														 	ErrorFptr_type			ErrorFptr);

void 					discard_TaskControl_type			(TaskControl_type** a);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller set/get functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

void 					SetTaskControlName 					(TaskControl_type* taskControl, char newName[]);
	// Returns pointer to dynamically allocated Task Controller name (null terminated string) 
char*					GetTaskControlName					(TaskControl_type* taskControl);

	// Use this function wisely, mostly intended to be used in a custom module event callback
void					SetTaskControlState					(TaskControl_type* taskControl, TaskStates_type	newState);
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
	// default, iterationMode = TASK_ITERATE_BEFORE_SUBTASKS_START
int						SetTaskControlIterMode				(TaskControl_type* taskControl, TaskIterMode_type iterMode);
TaskIterMode_type		GetTaskControlIterMode				(TaskControl_type* taskControl);

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

	// Logs Task Controller activity. Multiple Task Controllers may share the same log.
	// logPtr = NULL by default
void					SetTaskControlLog					(TaskControl_type* taskControl, TaskExecutionLog_type*	logPtr); 

	// Obtains status of the abort flag that can be used to abort an iteration happening in another thread outside of the provided IterateFptr 
BOOL					GetTaskControlAbortIterationFlag	(TaskControl_type* taskControl);

	// Task Controller HW Trigger types based on how the Task Controller is connected using AddHWSlaveTrigToMaster and RemoveHWSlaveTrigFromMaster 
HWTrigger_type          GetTaskControlHWTrigger				(TaskControl_type* taskControl);

	// Returns the parent Task Controller, given a Task Controller. If none, returns NULL.
TaskControl_type*		GetTaskControlParent				(TaskControl_type* taskControl);

	// Creates a list of SubTask Controllers of TaskControl_type* elements, given a Task Controller. 
	// If there are no SubTasks, the list is empty. Use ListDispose to dispose of list.
ListType				GetTaskControlSubTasks				(TaskControl_type* taskControl);

	// Describes whether a Task Controller is meant to be used as an User Interface Task Controller that allows the user to control the execution of a Task Tree.
	// True, TC is of an UITC type, False otherwise. default = False
void					SetTaskControlUITCFlag				(TaskControl_type* taskControl, BOOL UITCFlag);
BOOL					GetTaskControlUITCFlag				(TaskControl_type* taskControl);
//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller composition functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

int						AddSubTaskToParent					(TaskControl_type* parent, TaskControl_type* child);

int						RemoveSubTaskFromParent				(TaskControl_type* child);


//------------------------------------------------------------------------------------------------------------------------------------------------------
// HW trigger dependencies
//------------------------------------------------------------------------------------------------------------------------------------------------------

int						AddHWSlaveTrigToMaster				(TaskControl_type* master, TaskControl_type* slave);
		
int						RemoveHWSlaveTrigFromMaster			(TaskControl_type* slave);

int 					RemoveAllHWSlaveTrigsFromMaster		(TaskControl_type* master);

	// Disconnects a given Task Controller from its parent, disconnects all child nodes from each other as well as any VChan or HW-triggers
int						DisassembleTaskTreeBranch			(TaskControl_type* taskControlNode); 

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller event posting and execution control functions
//------------------------------------------------------------------------------------------------------------------------------------------------------
														
	// Pass NULL to eventInfo if there is no additional data carried by the event 
	// Pass NULL to disposeEventInfoFptr if eventInfo should NOT be disposed after processing the event
	// 
int 					TaskControlEvent					(TaskControl_type* RecipientTaskControl, TaskEvents_type event, void* eventInfo, 
														 	DisposeEventInfoFptr_type disposeEventInfoFptr); 

	// Used to signal the Task Controller that an iteration is done.
	// Pass to errorInfo an empty string as "", if there is no error and the iteration completed succesfully. Otherwise,
	// pass an error message string. Pass 0 to errorID if there is no error, otherwise pass an error code.
int						TaskControlIterationDone			(TaskControl_type* taskControl, int errorID, char errorInfo[]);


int						TaskControlEventToSubTasks  		(TaskControl_type* SenderTaskControl, TaskEvents_type event, void* eventInfo, 
														 	DisposeEventInfoFptr_type disposeEventInfoFptr); 

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

	// Adds a VChan to the Task Controller that is used to receive incoming data.
int						AddSinkVChan						(TaskControl_type* taskControl, SinkVChan_type* sinkVChan, TaskVChanFuncAssign_type VChanFunc); 
int						RemoveSinkVChan 					(TaskControl_type* taskControl, SinkVChan_type* sinkVChan);
void 					RemoveAllSinkVChans 				(TaskControl_type* taskControl);

//------------------------------------------------------------------------------------------------------------------------------------------------------
// Task Controller logging functions
//------------------------------------------------------------------------------------------------------------------------------------------------------

	// Pass panel handle and text box control IDs if any to display task controller execution.
	// Pass 0 for both if this is not required.
TaskExecutionLog_type*	init_TaskExecutionLog_type			(int logBoxPanHandle, int logBoxControl); 

void					discard_TaskExecutionLog_type		(TaskExecutionLog_type** a);

	




#ifdef __cplusplus
    }
#endif

#endif  /* ndef __TaskController_H__ */	
	
	
	
