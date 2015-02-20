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
#include "toolbox.h"
#include "DataTypes.h"

//==============================================================================
// Constants

//==============================================================================
// Types
	
	//------------
	// Classes
	//------------
	
typedef struct DisplayEngine	DisplayEngine_type;		// Generic display engine class

typedef struct ImageDisplay		ImageDisplay_type;		// Generic image display class

	//------------													
	// Data types
	//------------
	
typedef void* 					ImgDisplayCBData_type;

typedef enum {
	ROI_Placed,											// ROI placed on the image, but not added to it.
	ROI_Added,											// ROI added to the image.
	ROI_Removed											// ROI removed.
} ROIActions;

//--------------------------------------
// Callback typedefs
//--------------------------------------

	// ROI was placed over the image (but not added to it)
typedef void							(*ROIEvents_CBFptr_type)					(ImageDisplay_type* imgDisplay, void* callbackData, ROIActions action, ROI_type* ROI);

	// Restores the module configuration stored previously with the image (scan settings, moves stages to the stored positions, etc)
	// each display handle can have multiple such callbacks, each with its own callback data
typedef int								(*RestoreImgSettings_CBFptr_type)			(DisplayEngine_type* displayEngine, ImageDisplay_type* imgDisplay, void* callbackData, char** errorInfo);

	// Called when a display module error occurs
typedef void							(*ErrorHandlerFptr_type)					(ImageDisplay_type* imgDisplay, int errorID, char* errorInfo);


//--------------------------------------
// Methods typedefs
//--------------------------------------
														
	// Displays or updates an image in a display window
typedef int								(*DisplayImageFptr_type)					(ImageDisplay_type* imgDisplay, void* pixelArray, int imgWidth, int imgHeight, ImageTypes imageType);

	// Obtains a display handle from the display engine that can be passed to other functions like updating the image
typedef ImageDisplay_type*				(*GetImageDisplayFptr_type)					(DisplayEngine_type* displayEngine, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType);

	// Returns the callback data associated with the display handle
typedef ImgDisplayCBData_type			(*GetImageDisplayCBDataFptr_type)			(ImageDisplay_type* imgDisplay);

	// Sets callbacks to a display handle to restore the image settings to the various modules/devices that contributed to the image. Each callback function has its own dinamically allocated callback data.
	// The callback data is automatically disposed if the display handle is discarded by calling the provided discardCallbackDataFunctions. If a discard callback data function is NULL then free() is called by default.
	// These callbacks should be set before calling DisplayImageFptr_type
typedef int								(*SetRestoreImgSettingsCBsFptr_type)		(ImageDisplay_type* imgDisplay, size_t nCallbackFunctions, RestoreImgSettings_CBFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions);

	// Places an ROI overlay over the displayed image 
typedef ROI_type*						(*OverlayROIFptr_type)						(ImageDisplay_type* imgDisplay, ROI_type* ROI);

	// Clears all ROI overlays
typedef void							(*ClearAllROIFptr_type)						(ImageDisplay_type* imgDisplay);




	
//==============================================================================
// Classes to inherit from

struct DisplayEngine {
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	
	//---------------------------------------------------------------------------------------------------------------
	// METHODS (override with child class implementation)
	//---------------------------------------------------------------------------------------------------------------
	
	DiscardFptr_type					discardFptr;					// method to discard child class data
	
	DisplayImageFptr_type				displayImageFptr;
	
	GetImageDisplayFptr_type			getImageDisplayFptr;
	
	GetImageDisplayCBDataFptr_type		getImageDisplayCBDataFptr;
	
	SetRestoreImgSettingsCBsFptr_type	setRestoreImgSettingsFptr;
	
	OverlayROIFptr_type					overlayROIFptr;
	
	ClearAllROIFptr_type				clearAllROIFptr;
	
	//---------------------------------------------------------------------------------------------------------------
	// CALLBACKS (provide action to be taken by the module making use of the display engine)
	//---------------------------------------------------------------------------------------------------------------
	
	ROIEvents_CBFptr_type				ROIEventsCBFptr;
	
	ErrorHandlerFptr_type				errorHandlerCBFptr;
	
	
};

struct ImageDisplay {
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	DisplayEngine_type*					displayEngine;					// Reference to the display engine to which this Image display belongs.
	ImgDisplayCBData_type				imageDisplayCBData;				// Callback data associated with the image display.
	ListType							ROIs;							// List of ROIs added to the image of ROI_type*
	ROIActions							ROIAction;						// Parameter passed to the ROI callback.
	
	//---------------------------------------------------------------------------------------------------------------
	// METHODS (override with child class implementation)
	//---------------------------------------------------------------------------------------------------------------
	
	DiscardFptr_type					discardFptr;					// method to discard child class data
	
	
	//---------------------------------------------------------------------------------------------------------------
	// CALLBACKS (provide action to be taken by the module making use of the display engine)
	//---------------------------------------------------------------------------------------------------------------
	
	// callbacks to restore image settings to the various modules
	size_t 								nRestoreSettingsCBs;			// Number of callbacks to be called when restoring image settings.
	RestoreImgSettings_CBFptr_type* 	restoreSettingsCBs;				// Callback array called sequncially when data must be restored from the image.
	void** 								restoreSettingsCBsData;			// Array of callback data assigned to each restore callback function.
	DiscardFptr_type* 					discardCallbackDataFunctions;   // Array of methods to discard the callback data once not needed anymore.
	
};

//==============================================================================
// External variables

//==============================================================================
// Global functions

//--------------------------------------------------------------------------------------------------------------------------
// Display Engine management
//--------------------------------------------------------------------------------------------------------------------------

	// Initializes a generic display engine instance.
void									init_DisplayEngine_type					(DisplayEngine_type* 					displayEngine,
																				 DiscardFptr_type						discardFptr,
																				 DisplayImageFptr_type					displayImageFptr,
																				 GetImageDisplayFptr_type				getImageDisplayFptr,
																				 GetImageDisplayCBDataFptr_type			getImageDisplayCBDataFptr,
																				 SetRestoreImgSettingsCBsFptr_type		setRestoreImgSettingsFptr,
																				 OverlayROIFptr_type					overlayROIFptr,
																				 ClearAllROIFptr_type					clearAllROIFptr,
																				 ROIEvents_CBFptr_type					ROIEventsCBFptr,
																				 ErrorHandlerFptr_type					errorHandlerCBFptr);

	// Disposes all types of display engines by invoking the specific dispose method.
void 									discard_DisplayEngine_type 				(DisplayEngine_type** displayEnginePtr);

	// Disposes of the DisplayEngine base class. Use this to implement child class dispose method.
void									discard_DisplayEngineClass				(DisplayEngine_type** displayEnginePtr);

	// Inititalizes a generic image display instance.
int										init_ImageDisplay_type					(ImageDisplay_type*						imageDisplay,
																				 DiscardFptr_type						discardFptr,
																				 DisplayEngine_type* 					displayEngine,
																				 ImgDisplayCBData_type					imageDisplayCBData);

	// Discards data from a generic image display instance. Call this from the child class discard function after disposing of child-specific data.
void									discard_ImageDisplay_type				(ImageDisplay_type**					imageDisplayPtr);



#ifdef __cplusplus
    }
#endif

#endif  /* ndef __DisplayEngine_H__ */
