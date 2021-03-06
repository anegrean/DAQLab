//==============================================================================
//
// Title:		Zstage.h
// Purpose:		Generic Z-stage.
//
// Created on:	9-3-2014 at 20:11:54 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

#ifndef __Zstage_H__
#define __Zstage_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants

#define MOD_Zstage_NAME 	"Generic Z stage"

//==============================================================================
// Types
		
typedef struct Zstage 	ZStage_type;

typedef enum {
	
	ZSTAGE_MOVE_REL,
	ZSTAGE_MOVE_ABS

} Zstage_move_type;

typedef enum {
	
	ZSTAGE_LED_IDLE,
	ZSTAGE_LED_MOVING,
	ZSTAGE_LED_ERROR
	
} Zstage_LED_type;

//==============================================================================
// Module implementation

struct Zstage {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;

	// DATA
	
	TaskControl_type*	taskController;
	
		// Stage control panel
	int					controlPanHndl;
	int*				controlPanTopPos;
	int*				controlPanLeftPos;
	
		// Stage settings panel
	int					setPanHndl;
	
		// Stage settings menu bar handle
	int					menuBarHndl;
	
		// Settings menu ID
	int					menuIDSettings;
	
		// Current position of Zstage, if NULL, position was not determined.
	double*				zPos;   			// in [mm]
	
		// Current stage velocity. If NULL, this information is not available
	double*				stageVelocity;
	
	double*				lowVelocity;
	
	double*				midVelocity;
	
	double*				highVelocity;
	
		// Absolute position from which to start moving the stage in a stepping mode
		// if NULL, position was not initialized
	double*				startAbsPos;		// in [mm]
		// Relative position to which the stage is moved from the absolute start position
		// if 0, no movement from start position occurs
	double				endRelPos;			// in [mm]
		
		// The Z stage step size
	double				stepSize;			// in [mm]
	
		// Number of steps between start and end position with given zStepSize
	size_t				nZSteps;
	
		// Flag to revert movement direction, default 0, set to 1 to revert direction
	BOOL				revertDirection;
	
		// Reference positions of Zstage of RefPosition_type*
	ListType			zRefPos;
	
		// Positive stage position limit (towards increasing values of the stage position variable). If NULL, there is no limit.
	double*				zMaximumLimit;
	
		// Negative stage position limit. If NULL, there is no limit. Also, zMinimumLimit <= zMaximumLimit
	double*				zMinimumLimit;
	
	// METHODS
	
		// Callback to install on controls from selected panel in UI_ZStage.uir
		// Default set to Zstage_UI_CB in Zstage.c
		// Override: Optional, to change UI panel behavior. 
		// For hardware specific functionality override other methods such as MoveZ.
	CtrlCallbackPtr		uiCtrlsCB;
	
		// Callback to install on the panel from UI_ZStage.uir
	PanelCallbackPtr	uiPanelCB;
	
		// Default NULL, functionality not implemented.
		// Override: Required, provides hardware specific movement of Zstage.
		
		// moveVal in [mm] same units as zPos
	int					(*MoveZ) 					(ZStage_type* self, Zstage_move_type moveType, double moveVal, char** errorMsg);
		// Activates Z stage joystick control
	int					(*UseJoystick)				(ZStage_type* self, BOOL useJoystick, char** errorMsg);	
		// Stops Z stage motion
	int					(*StopZ)					(ZStage_type* self, char** errorMsg);
		// Changes the status of the LED on the Zstage control panel
	int					(*StatusLED)   				(ZStage_type* self, Zstage_LED_type status);
		// Updates position display of stage from structure data
	int					(*UpdatePositionDisplay)	(ZStage_type* self);
		// Gets stage negative and positive limits in [mm] if this functionality is implemented for the specific hardware. 
		// If not, then by default both negative and positive limits are set to the current position of the stage and the stage will not
		// move until zMinimumLimit < zMaximumLimit.
	int					(*GetHWStageLimits)			(ZStage_type* self, double* negativeLimit, double* positiveLimit, char** errorMsg);
		// Sets stage negative and positive limits in [mm] if this functionality is implemented for the specific hardware. This is an extra
		// safety measure if software set limits zMaximumLimit and zMinimumLimit fail to stop the stage.
	int					(*SetHWStageLimits)			(ZStage_type* self, double negativeLimit, double positiveLimit, char** errorMsg);
	
		// Gets stage velocity
	int					(*GetStageVelocity)			(ZStage_type* self, double* velocity, char** errorMsg);
		// Sets stage velocity
	int					(*SetStageVelocity)			(ZStage_type* self, double velocity, char** errorMsg);
		// Updates number of steps between an absolute start and relative end position to start position given step size 
		// and rounds up if necessary the end position such that between the start and end positions there is an integer 
		// multiple of step sizes. These parameters are written to the structure data.
		// It also updates these values on the display and outputs the number of steps
	int					(*UpdateZSteps)				(ZStage_type* self);
	
	void				(*SetStepCounter)			(ZStage_type* self, size_t val);
	
	void				(*DimWhenRunning)			(ZStage_type* self, BOOL dimmed);
		

};

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_Zstage 	(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_Zstage 		(DAQLabModule_type** mod);

	// loads generic Z stage module resources
int					ZStage_Load 		(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

	// saves generic settings for a Z stage
int					ZStage_SaveCfg		(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

	// loads generic settings for a Z stage
int					ZStage_LoadCfg		(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Zstage_H__ */
