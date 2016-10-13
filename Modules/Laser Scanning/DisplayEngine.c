//==============================================================================
//
// Title:		DisplayEngine.c
// Purpose:		Used for displaying images.
//
// Created on:	2-2-2015 at 15:38:29 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
//
//==============================================================================

//==============================================================================
// Include files

#include "DisplayEngine.h"

//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}

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
	int				error 		= 0;
	Image_type**	imagePtr	= NULL;
	
	//--------------------------------------
	// init
	//--------------------------------------
	
	// image display data binding
	imageDisplay->displayEngine					= displayEngine;
	imageDisplay->imageDisplayCBData			= imageDisplayCBData;
	// image data
	imageDisplay->imageTSV						= 0;
	imageDisplay->ROIEvent						= ROI_Placed;
	// methods
	imageDisplay->discardFptr					= discardFptr;       
	// callbacks
	imageDisplay->callbacks						= NULL;
	
	//--------------------------------------
	// alloc (can fail)
	//--------------------------------------
	
	errChk( CmtNewTSV(sizeof(Image_type*), &imageDisplay->imageTSV) );
	// init image
	errChk( CmtGetTSVPtr(imageDisplay->imageTSV, &imagePtr) );
	*imagePtr = NULL;
	CmtReleaseTSVPtr(imageDisplay->imageTSV);
	
Error:
	
	return error;
}

void discard_ImageDisplay_type (ImageDisplay_type** imageDisplayPtr)
{
	ImageDisplay_type* 		imageDisplay 	= *imageDisplayPtr;
	Image_type**			imagePtr		= NULL;
	
	if (!imageDisplay) return;
	
	// dispose image
	if( CmtGetTSVPtr(imageDisplay->imageTSV, &imagePtr)  < 0) goto SkipDiscardImage;
	discard_Image_type(imagePtr);
	CmtReleaseTSVPtr(imageDisplay->imageTSV);
	
	SkipDiscardImage:
	// discard image TSV
	if (imageDisplay->imageTSV) {
		CmtDiscardTSV(imageDisplay->imageTSV);
		imageDisplay->imageTSV = 0;
	}
	
	discard_CallbackGroup_type(&imageDisplay->callbacks);
	
	OKfree(*imageDisplayPtr);
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
