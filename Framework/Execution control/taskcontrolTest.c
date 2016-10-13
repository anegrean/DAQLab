//==============================================================================
//
// Title:		taskcontrolTest.c
// Purpose:		Task controller testing.
//
// Created on:	2014.05.15. at 13:39:39 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files

#include <userint.h>
#include <utility.h>
#include "TaskController.h"
#include "taskcontrolTestUI.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

int						ControllerPan;

TaskExecutionLog_type* 	TaskExecutionLog;
TaskControl_type* 		ZStackTask;
TaskControl_type* 		ZStage;
TaskControl_type* 		DevX;


//==============================================================================
// Global functions

void ZStage_Iterate (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

void DevX_Iterate (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

void ZStackTask_ErrorHandler (TaskControl_type* taskControl, char* errorMsg);


int main (int argc, char *argv[])
{
	if (InitCVIRTE (0, argv, 0) == 0) return -1; /* out of memory */
	
	// load UI
	ControllerPan		= LoadPanel(0, "taskcontrolTestUI.uir", ControlPan);
	DisplayPanel(ControllerPan);
	
	//--------------------------------------------------------------------------
	// Task Controller set-up
	//--------------------------------------------------------------------------
	TaskExecutionLog 	= init_TaskExecutionLog_type(ControllerPan, ControlPan_ExecutionLogBox);
	
	// ZStack Task
	ZStackTask			= init_TaskControl_type ("Z Stack Task", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, ZStackTask_ErrorHandler);
	SetTaskControlIterations(ZStackTask, 1);
	SetTaskControlExecutionMode(ZStackTask, TC_Execute_BeforeChildTCs);
	SetTaskControlIterationsWait(ZStackTask, 0);
	SetTaskControlLog(ZStackTask, TaskExecutionLog);
	
	// ZStage
	ZStage				= init_TaskControl_type ("Z Stage", NULL, NULL, ZStage_Iterate, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	SetTaskControlMode(ZStage, TASK_CONTINUOUS);
	SetTaskControlIterations(ZStage, 3);
	SetTaskControlIterationsWait(ZStackTask, 0);
	SetTaskControlLog(ZStage, TaskExecutionLog);
	
	// Device X
	DevX				= init_TaskControl_type ("Device X", NULL, NULL, DevX_Iterate, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	SetTaskControlIterations(DevX, 2);
	
	SetTaskControlLog(DevX, TaskExecutionLog);
	
	// Make ZStage subtask to ZStack task
	AddChildTCToParent(ZStackTask, ZStage);
	//AddChildTCToParent(ZStackTask, DevX);
	
	//AddHWSlaveTrigToMaster(ZStage, DevX); 
	
	RunUserInterface();
	
	
	return 0;
}

void ZStackTask_ErrorHandler (TaskControl_type* taskControl, char* errorMsg)
{
	MessagePopup("Error", errorMsg);
}

void ZStage_Iterate (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	//TaskControlEvent(taskControl, TASK_EVENT_HWTRIG_SLAVE_ARMED, NULL, NULL);    
	//TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL);
	Beep();
}

void DevX_Iterate (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{  
	TaskControlEvent(taskControl, TASK_EVENT_HWTRIG_SLAVE_ARMED, NULL, NULL);    
	TaskControlEvent(taskControl, TC_Event_IterationDone, NULL, NULL);
}

int CVICALLBACK CB_ControlPan (int panel, int event, void *callbackData,
		int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_CLOSE:
			
			QuitUserInterface(0);

			break;
	}
	return 0;
}

int CVICALLBACK CB_TaskController (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
				
				case ControlPan_StartBTTN:
					
					TaskControlEvent(ZStackTask, TC_Event_Start, NULL, NULL); 
					
					break;
					
				case ControlPan_StopBTTN:
					
					TaskControlEvent(ZStackTask, TC_Event_Stop, NULL, NULL); 
					
					break;
					
				case ControlPan_ConfigureBTTN:
					
					TaskControlEvent(ZStackTask, TC_Event_Configure, NULL, NULL); 
					
					break;
					
				case ControlPan_StepBTTN:
					
					TaskControlEvent(ZStackTask, TC_Event_IterateOnce, NULL, NULL); 
					
					break;
					
				case ControlPan_AbortBTTN:
					
					AbortTaskControlExecution (ZStackTask);
					
					break;
					
				case ControlPan_IterCompleteBTTN:
					
					TaskControlIterationDone(ZStage, 0, "", FALSE);
					
					break;
					
				case ControlPan_ArmBTTN:
					
					break;
			}
			
			   

			break;
	}
	return 0;
}
