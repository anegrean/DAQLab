//==============================================================================
//
// Title:		DataStorage.c
// Purpose:		A short description of the implementation.
//
// Created on:	12-9-2014 at 12:08:22 by Systeembeheer.
// Copyright:	IT-Groep FEW, Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first
#include <formatio.h>
#include <userint.h>
#include "DataStorage.h"

//==============================================================================
// Constants

//==============================================================================
// Types


//==============================================================================
// Module implementation

struct DatStore {

	// SUPER, must be the first member to inherit from

	DAQLabModule_type 	baseClass;

	// DATA
	
		//-------------------------
		// Task Controller
		//-------------------------
		
	TaskControl_type*	taskController;

};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//-----------------------------------------
// Data Storage Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	ConfigureTC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static void					IterateTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag);
static FCallReturn_type*	StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	DoneTC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	StoppedTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag);
static void					DimTC					(TaskControl_type* taskControl, BOOL dimmed);
static void 				ErrorTC 				(TaskControl_type* taskControl, char* errorMsg);
static FCallReturn_type*	ModuleEventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

