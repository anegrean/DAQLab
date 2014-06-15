//==============================================================================
//
// Title:		Zstage.h
// Purpose:		A short description of the interface.
//
// Created on:	9-3-2014 at 20:11:54 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
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
	
	DAQLabModule_type 	module_base;

	// DATA
	
	int					controlPanHndl;
	
		// Current position of Zstage, if NULL, position was not determined.
	double*				zPos;   // in [mm]
	
		// Flag to revert movement direction, default 0, set to 1 to revert direction
	BOOL				revertDirection;
	
		// Reference positions of Zstage of Zstage_RefPosition_type*
	ListType			zRefPos;
	
		// Upper limit position of Zstage, if NULL, there is no limit 			( zLLimPos < zULimPos )
	double*				zULimPos;
	
		// Lower limit position of Zstage, if NULL, there is no lower limit
	double*				zLLimPos;
	
		// Step size used for iterating the position of the stage, must be a positive number
	double				zStackStep;
	
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

};

//==============================================================================
// Constants

#define MOD_Zstage_NAME 	"Generic Z stage"


//==============================================================================
// External variables

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
