//==============================================================================
//
// Title:		DisplayEngine.c
// Purpose:		A short description of the implementation.
//
// Created on:	2-2-2015 at 15:38:29 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DisplayEngine.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

#define Default_ROI_Label_FontSize		12			// in points

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions


void init_DisplayEngine_type (	DisplayEngine_type* 					displayEngine,
								DiscardFptr_type						discardFptr,
								DisplayImageFptr_type					displayImageFptr,
								DisplayRGBImageFptr_type				displayRGBImageFptr,
								DiscardFptr_type						imageDiscardFptr,
								GetImageDisplayFptr_type				getImageDisplayFptr,
								GetImageDisplayCBDataFptr_type			getImageDisplayCBDataFptr,
								OverlayROIFptr_type						overlayROIFptr,
								ROIActionsFptr_type						ROIActionsFptr,
							 	ROIEvents_CBFptr_type					ROIEventsCBFptr,
							 	ErrorHandlerFptr_type					errorHandlerCBFptr		)
{
	displayEngine->discardFptr 					= discardFptr;
	displayEngine->displayImageFptr 			= displayImageFptr;
	displayEngine->displayRGBImageFptr			= displayRGBImageFptr;
	displayEngine->imageDiscardFptr				= imageDiscardFptr;
	displayEngine->getImageDisplayFptr 			= getImageDisplayFptr;
	displayEngine->getImageDisplayCBDataFptr	= getImageDisplayCBDataFptr;
	displayEngine->overlayROIFptr				= overlayROIFptr;
	displayEngine->ROIActionsFptr				= ROIActionsFptr;
	displayEngine->ROIEventsCBFptr				= ROIEventsCBFptr;
	displayEngine->errorHandlerCBFptr			= errorHandlerCBFptr;
	
}

int init_ImageDisplay_type	(	ImageDisplay_type*						imageDisplay,
								DiscardFptr_type						discardFptr,
								DisplayEngine_type* 					displayEngine,
								ImgDisplayCBData_type					imageDisplayCBData 		)
{

	//--------------------------------------
	// init
	//--------------------------------------
	
	// image display data binding
	imageDisplay->displayEngine					= displayEngine;
	imageDisplay->imageDisplayCBData			= imageDisplayCBData;
	
	// image data
	imageDisplay->image							= NULL;
	
	
	// ROI management
	imageDisplay->ROITextBackground.R			= 0;
	imageDisplay->ROITextBackground.G			= 0;	
	imageDisplay->ROITextBackground.B			= 0;	
	imageDisplay->ROITextBackground.alpha		= 255;	// transparent
	imageDisplay->ROITextFontSize				= Default_ROI_Label_FontSize;
	imageDisplay->ROIEvent						= ROI_Placed;
	
	// methods
	imageDisplay->discardFptr					= discardFptr;       
	
	// callbacks
	imageDisplay->callbacks						= NULL;
	
	return 0;
	
}

void discard_ImageDisplay_type (ImageDisplay_type** imageDisplayPtr)
{
	ImageDisplay_type* 	imageDisplay 	= *imageDisplayPtr;
	
	if (!imageDisplay) return;
	
	discard_Image_type(&imageDisplay->image);
	
	discard_CallbackGroup_type(&imageDisplay->callbacks);
	
	//OKfree(*imageDisplayPtr);
}

void discard_DisplayEngine_type (DisplayEngine_type** displayEnginePtr)
{
	if (!*displayEnginePtr) return;
	
	// call child class dispose method
	(*(*displayEnginePtr)->discardFptr) ((void**)displayEnginePtr);
}

void discard_DisplayEngineClass (DisplayEngine_type** displayEnginePtr)
{
	// free displayEnginePtr
	OKfree(*displayEnginePtr); 
}
