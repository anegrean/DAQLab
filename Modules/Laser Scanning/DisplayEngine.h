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
} ROIEvents;

typedef enum {
	ROI_Visible,										// ROI is visible on the image.
	ROI_Hide,											// ROI is hidden.
	ROI_Delete											// ROI is deleted from the image.
} ROIActions;

typedef enum {
	ImageDisplay_Close,
	ImageDisplay_RestoreSettings
} ImageDisplayEvents;

//--------------------------------------
// Callback typedefs
//--------------------------------------

	// ROI was placed over the image (but not added to it)
typedef void							(*ROIEvents_CBFptr_type)					(ImageDisplay_type* imgDisplay, void* callbackData, ROIEvents event, ROI_type* ROI);

	// Called when a display module error occurs
typedef void							(*ErrorHandlerFptr_type)					(ImageDisplay_type* imgDisplay, int errorID, char* errorMsg);

//--------------------------------------
// Methods typedefs
//--------------------------------------
														
	// Displays or updates an image in a display window
typedef int								(*DisplayImageFptr_type)					(ImageDisplay_type* imgDisplay, Image_type** image);

	// Displays an 8 bit RGB image in a display window regardless of the input image format
typedef int								(*DisplayRGBImageFptr_type)					(ImageDisplay_type* imgDisplay, Image_type** imageR, Image_type** imageG, Image_type** imageB);	

	// Obtains a display handle from the display engine that can be passed to other functions like updating the image
typedef ImageDisplay_type*				(*GetImageDisplayFptr_type)					(DisplayEngine_type* displayEngine, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType);

	// Returns the callback data associated with the display handle
typedef ImgDisplayCBData_type			(*GetImageDisplayCBDataFptr_type)			(ImageDisplay_type* imgDisplay);

	// Places an ROI overlay over the displayed image 
typedef ROI_type*						(*OverlayROIFptr_type)						(ImageDisplay_type* imgDisplay, ROI_type* ROI);

	// Clears ROI overlays. ROIIdx is the 1-based ROI index from the image display ROI list. If ROIIdx is 0, all ROIs are cleared
typedef void							(*ROIActionsFptr_type)						(ImageDisplay_type* imgDisplay, int ROIIdx, ROIActions action);

	// Copies image data 
typedef void*							(*CopyImageFptr_type)						(void* image);




	
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
	
	DiscardFptr_type					imageDiscardFptr;				// Method to discard imageData.
	
	CopyImageFptr_type					imageCopyFptr;					// Method to copy image data
	
	DisplayImageFptr_type				displayImageFptr;
	
	DisplayRGBImageFptr_type			displayRGBImageFptr;
	
	GetImageDisplayFptr_type			getImageDisplayFptr;
	
	GetImageDisplayCBDataFptr_type		getImageDisplayCBDataFptr;
	
	OverlayROIFptr_type					overlayROIFptr;
	
	ROIActionsFptr_type					ROIActionsFptr;
	
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
	
	//----------------------------------
	// Image display data binding
	//----------------------------------
	
	DisplayEngine_type*					displayEngine;					// Reference to the display engine to which this Image display belongs.
	ImgDisplayCBData_type				imageDisplayCBData;				// Callback data associated with the image display.
	//Image_type*							image;						// image data container 
	CmtTSVHandle						imageTSV;						// Thread safe variable of Image_type*

	ROIEvents							ROIEvent;						// Parameter passed to the ROI callback.
	
	//---------------------------------------------------------------------------------------------------------------
	// METHODS (override with child class implementation)
	//---------------------------------------------------------------------------------------------------------------
	
	DiscardFptr_type					discardFptr;					// Method to discard child class data
	
	//---------------------------------------------------------------------------------------------------------------
	// CALLBACKS (provide action to be taken by the module making use of the display engine)
	//---------------------------------------------------------------------------------------------------------------

	CallbackGroup_type*					callbacks;
	
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
																				 DisplayRGBImageFptr_type				displayRGBImageFptr,
																				 DiscardFptr_type						imageDiscardFptr,
																				 GetImageDisplayFptr_type				getImageDisplayFptr,
																				 GetImageDisplayCBDataFptr_type			getImageDisplayCBDataFptr,
																				 OverlayROIFptr_type					overlayROIFptr,
																				 ROIActionsFptr_type					ROIActionsFptr,
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
