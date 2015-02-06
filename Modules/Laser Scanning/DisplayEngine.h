//==============================================================================
//
// Title:		DisplayEngine.h
// Purpose:		A short description of the interface.
//
// Created on:	2-2-2015 at 15:38:29 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __DisplayEngine_H__
#define __DisplayEngine_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types
		
typedef struct DisplayEngine	DisplayEngine_type;		// Generic display engine.

typedef void*					DisplayHandle_type;		// Display handle for one display window that can be used by
														// an external module to interact with the display.
														
//--------------------------------------
// Methods typedefs
//--------------------------------------
														
	// Discards a display engine 
typedef void 							(* DiscardDisplayEngineFptr_type) 			(void** displayEnginePtr);

	// Displays or updates an image in a display window
typedef int								(* DisplayImageFptr_type)					(DisplayHandle_type displayHandle, void* pixelArray, int imgWidth, int imgHeight, ImageTypes imageType);

	// Obtains a display handle from the display engine that can be passed to other functions like updating the image
typedef DisplayHandle_type				(* GetDisplayHandleFptr_type)				(DisplayEngine_type* displayEngine, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType);

	// Places an ROI overlay over the displayed image 
typedef int								(* OverlayROIFptr_type)						(DisplayHandle_type displayHandle, ROI_type* ROI);

	// Clears all ROI overlays
typedef void							(* ClearAllROIFptr_type)					(DisplayHandle_type displayHandle);


//--------------------------------------
// Callback typedefs
//--------------------------------------

	// ROI was placed over the image (but not added to it)
typedef void							(* ROIPlaced_CBFptr_type)					(DisplayHandle_type displayHandle, void* callbackData, ROI_type** ROI);

	// An overlaid ROI was removed from the image
typedef void							(* ROIRemoved_CBFptr_type)					(DisplayHandle_type displayHandle, void* callbackData, ROI_type* ROI);


	
//==============================================================================
// Class to inherit from

struct DisplayEngine {
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	
	//---------------------------------------------------------------------------------------------------------------
	// METHODS (override with child class implementation)
	//---------------------------------------------------------------------------------------------------------------
	
	DiscardDisplayEngineFptr_type		discardFptr;
	
	DisplayImageFptr_type				displayImageFptr;
	
	GetDisplayHandleFptr_type			getDisplayHandleFptr;
	
	OverlayROIFptr_type					overlayROIFptr;
	
	ClearAllROIFptr_type				clearAllROIFptr;
	
	//---------------------------------------------------------------------------------------------------------------
	// CALLBACKS (provide action to be taken by the module making use of the display engine)
	//---------------------------------------------------------------------------------------------------------------
	
	ROIPlaced_CBFptr_type				ROIPlacedCBFptr;
	
	ROIRemoved_CBFptr_type				ROIRemovedCBFptr;
	
	
};

//==============================================================================
// External variables

//==============================================================================
// Global functions

//--------------------------------------------------------------------------------------------------------------------------
// Display Engine management
//--------------------------------------------------------------------------------------------------------------------------

	// Initializes a generic display engine.
void									init_DisplayEngine_type					(	DisplayEngine_type* 					displayEngine,
																				 	DiscardDisplayEngineFptr_type			discardFptr,
																				 	DisplayImageFptr_type					displayImageFptr,
																				 	GetDisplayHandleFptr_type				getDisplayHandleFptr,
																				 	OverlayROIFptr_type						overlayROIFptr,
																					ClearAllROIFptr_type					clearAllROIFptr,
																				 	ROIPlaced_CBFptr_type					ROIPlacedCBFptr,
																				 	ROIRemoved_CBFptr_type					ROIRemovedCBFptr		);


	// Disposes all types of display engines by invoking the specific dispose method.
void 									discard_DisplayEngine_type 				(DisplayEngine_type** displayEnginePtr);

	// Disposes of the DisplayEngine base class. Use this to implement child class dispose method.
void									discard_DisplayEngineClass				(DisplayEngine_type** displayEnginePtr);



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DisplayEngine_H__ */
