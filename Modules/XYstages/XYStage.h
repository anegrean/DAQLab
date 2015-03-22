//==============================================================================
//
// Title:		XYStage.h
// Purpose:		A short description of the interface.
//
// Created on:	18-3-2015 at 16:38:28 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __XYStage_H__
#define __XYStage_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "DAQLabModule.h"

//==============================================================================
// Constants

#define MOD_XYStage_NAME 	"Generic XY stage"

//==============================================================================
// Types
		
typedef struct XYStage 	XYStage_type;

typedef enum {
	
	XYSTAGE_MOVE_REL,
	XYSTAGE_MOVE_ABS

} StageMoveTypes;

typedef enum {
	
	XYSTAGE_LED_IDLE,
	XYSTAGE_LED_MOVING,
	XYSTAGE_LED_ERROR
	
} StageLEDStates;

typedef enum {
	
	XYSTAGE_X_AXIS,
	XYSTAGE_Y_AXIS,
	
} StageAxes;

//==============================================================================
// Module implementation

// Callback function types supplied by child classes

typedef int (* MoveFptr_type) 				(XYStage_type* stage, StageMoveTypes moveType, StageAxes axis, double moveVal, char** errorInfo);

typedef int	(* UseJoystickFptr_type)		(XYStage_type* stage, BOOL useJoystick, char** errorInfo);

typedef int	(* StopFptr_type)				(XYStage_type* stage, char** errorInfo);

typedef int	(* GetLimitsFptr_type)			(XYStage_type* stage, double* xNegativeLimit, double* xPositiveLimit, double* yNegativeLimit, double* yPositiveLimit, char** errorInfo);

typedef int	(* SetLimitsFptr_type)			(XYStage_type* stage, double xNegativeLimit, double xPositiveLimit, double yNegativeLimit, double yPositiveLimit, char** errorInfo);

typedef int	(* GetVelocityFptr_type)		(XYStage_type* stage, double* velocity, char** errorInfo);

typedef int	(* SetVelocityFptr_type)		(XYStage_type* stage, double velocity, char** errorInfo);

typedef int	(* GetAbsPositionFptr_type)		(XYStage_type* stage, double* xAbsPos, double* yAbsPos, char** errorInfo);

struct XYStage {
	
	// SUPER, must be the first member to inherit from
	//-------------------------------------------------------------------------------------------------------------
	
	DAQLabModule_type 			baseClass;

	// DATA
	//-------------------------------------------------------------------------------------------------------------
	
		// Task controller
		//-------------------------
		
	TaskControl_type*			taskController;	
	
		// UI resources
		//-------------------------
	
	int							controlPanHndl;			// Stage control panel.
	int							setPanHndl;				// Stage settings panel.
	int							menuBarHndl;			// Stage settings menu bar handle.
	int							menuIDSettings;			// Settings menu ID.
	
		// XY stage operation
		//-------------------------		
	
		// Stage position
		
	double						xPos;   				// Current X axis position in [mm], if NULL, position was not determined.
	double						yPos;   				// Current Y axis position in [mm], if NULL, position was not determined.
	BOOL						reverseXAxis;			// Flag to revert X axis movement direction. If True, revert direction, default: False
	BOOL						reverseYAxis;			// Flag to revert Y axis movement direction. If True, revert direction, default: False
	ListType					xyRefPos;				// Reference positions of XYstage of RefPosition_type*.
	
		// Stage velocity
		
	double*						stageVelocity;			// Current stage velocity settings of the XY stage. If NULL, this information is not available.
	double*						lowVelocity;
	double*						midVelocity;
	double*						highVelocity;
	
		// Stage stepping mode
		
	double						xStartAbsPos;			// Absolute X axis position in [mm] from which to start moving the stage in a stepping mode. If NULL, position was not initialized.
	double						yStartAbsPos;			// Absolute Y axis position in [mm] from which to start moving the stage in a stepping mode. If NULL, position was not initialized.
	double						xEndRelPos;				// Relative X axis position in [mm] to which the stage is moved from the absolute X axis start position. If 0, no movement from start position occurs.
	double						yEndRelPos;				// Relative Y axis position in [mm] to which the stage is moved from the absolute Y axis start position. If 0, no movement from start position occurs.
	double						xStepSize;				// X axis step size in [mm].
	double						yStepSize;				// Y axis step size in [mm].
	size_t						nXSteps;				// Number of X axis steps between start and end position with given xStepSize.
	size_t						nYSteps;				// Number of Y axis steps between start and end position with given yStepSize.
	
		// Safety limits	
	
	double*						xMaximumLimit;			// Positive X axis position limit (towards increasing values of the stage position variable). If NULL, there is no limit.
	double*						xMinimumLimit;			// Negative X axis position limit. If NULL, there is no limit. Also xMinimumLimit <= xMaximumLimit.
	double*						yMaximumLimit;			// Positive Y axis position limit (towards increasing values of the stage position variable). If NULL, there is no limit.
	double*						yMinimumLimit;			// Negative Y axis position limit. If NULL, there is no limit. Also yMinimumLimit <= yMaximumLimit.
	
	// METHODS
	//-------------------------------------------------------------------------------------------------------------
	
		// Callback to install on controls from selected panel in UI_XYstage.uir
		// Default set to XYstage_UI_CB in XYstage.c
		// Override: Optional, to change UI panel behavior. 
		// For hardware specific functionality override other methods such as MoveX.
	CtrlCallbackPtr				uiCtrlsCB;
	
		// Callback to install on the panel from UI_XYtage.uir
	PanelCallbackPtr			uiPanelCB;
	
		// Default NULL, functionality not implemented.
		// Override: Required, provides hardware specific movement of XYStage.
		
		// moveVal in [mm] same units as xPos or yPos
	MoveFptr_type				Move;
		// Activates XY stage joystick control
	UseJoystickFptr_type		UseJoystick;
		// Stops XY stage motion
	StopFptr_type				Stop;
		// Gets stage negative and positive limits in [mm] if this functionality is implemented for the specific hardware. 
		// If not, then by default both negative and positive limits are set to the current position of the stage and the stage will not
		// move until xMinimumLimit < xMaximumLimit and yMinimumLimit < yMaximumLimit.
	GetLimitsFptr_type			GetLimits;
		// Sets stage negative and positive limits in [mm] if this functionality is implemented for the specific hardware. This is an extra
		// safety measure if software set limits xMaximumLimit and xMinimumLimit or yMaximumLimit and yMinimumLimit fail to stop the stage.
	SetLimitsFptr_type			SetLimits;
		// Gets stage velocity
	GetVelocityFptr_type		GetVelocity;
		// Sets stage velocity
	SetVelocityFptr_type		SetVelocity;
		// Gets the current absolute position of the stage in [mm]
	GetAbsPositionFptr_type 	GetAbsPosition;
	
};

//==============================================================================
// Global functions

DAQLabModule_type*	initalloc_XYStage 				(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);
void 				discard_XYStage 				(DAQLabModule_type** mod);

	// Loads generic XY stage module resources
int					XYStage_Load 					(DAQLabModule_type* mod, int workspacePanHndl);

	// Saves generic settings for a XY stage
int					XYStage_SaveCfg					(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement);

	// Loads generic settings for a XY stage
int					XYStage_LoadCfg					(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement);

//----------------------------------------------------------------------------------------------
// Generic XY stage operation
//----------------------------------------------------------------------------------------------

	// Changed stage LED status
void	 			XYStage_ChangeLEDStatus			(XYStage_type* stage, StageLEDStates state);

	// Updates position display of stage from structure data
void				XYStage_UpdatePositionDisplay	(XYStage_type* stage);

	// Updates number of steps between an absolute start and relative end position given step size. If necessary it rounds up the end position 
	// such that between the start and end positions there is an integer multiple of step sizes. These parameters are written to the structure data.
	// It also updates these values on the display and outputs the number of steps.
void				XYStage_UpdateSteps				(XYStage_type* stage);

void				XYStage_SetStepCounter			(XYStage_type* stage, uInt32 xSteps, uInt32 ySteps);
	
void				XYStage_DimWhenRunning			(XYStage_type* stage, BOOL dimmed);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __XYstage_H__ */
