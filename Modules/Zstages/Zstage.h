//==============================================================================
//
// Title:		Zstage.h
// Purpose:		A short description of the interface.
//
// Created on:	9-3-2014 at 20:11:54 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
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
		
typedef struct Zstage 	Zstage_type;

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
	
	int					controlPanHndl;
	
		// Current position of Zstage, if NULL, position was not determined.
	double*				zPos;   		// in [mm]
	
		// Absolute position from which to start moving the stage in a stepping mode
		// if NULL, position was not initialized
	double*				startAbsPos;	// in [mm]
		// Relative position to which the stage is moved from the absolute start position
		// if 0, no movement from start position occurs
	double				endRelPos;		// in [mm]
		
		// The Z stage step size
	double				stepSize;		// in [mm]
	
		// Number of steps between start and end position with given zStepSize
	size_t				nZSteps;
	
		// Flag to revert movement direction, default 0, set to 1 to revert direction
	BOOL				revertDirection;
	
		// Reference positions of Zstage of Zstage_RefPosition_type*
	ListType			zRefPos;
	
		// Upper limit position of Zstage, if NULL, there is no limit 			( zLLimPos < zULimPos )
	double*				zULimPos;
	
		// Lower limit position of Zstage, if NULL, there is no lower limit
	double*				zLLimPos;
	
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
	int					(* MoveZ) 					(Zstage_type* self, Zstage_move_type moveType, double moveVal);
		// Stops Z stage motion
	int					(* StopZ)					(Zstage_type* self);
		// Changes the status of the LED on the Zstage control panel
	int					(* StatusLED)   			(Zstage_type* self, Zstage_LED_type status);
		// Updates position display of stage from structure data
	int					(* UpdatePositionDisplay)	(Zstage_type* self);
		// Updates number of steps between an absolute start and relative end position to start position given step size 
		// and rounds up if necessary the end position such that between the start and end positions there is an integer 
		// multiple of step sizes. These parameters are written to the structure data.
		// It also updates these values on the display and outputs the number of steps
	int					(* UpdateZSteps)			(Zstage_type* self);
	
	void				(* SetStepCounter)			(Zstage_type* self, size_t val);
	
	void				(* DimWhenRunning)			(Zstage_type* self, BOOL dimmed);
		

};

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_Zstage 	(DAQLabModule_type* mod, char className[], char instanceName[]);
void 				discard_Zstage 		(DAQLabModule_type** mod);

	// loads generic Z stage resources
int					Zstage_Load 		(DAQLabModule_type* mod, int workspacePanHndl);

	// loads generic Z stage configuration from an XML file  
//int 				Zstage_LoadCfg 		(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  DAQLabCfg_RootElement);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __Zstage_H__ */
