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
								GetImageDisplayFptr_type				getImageDisplayFptr,
								GetImageDisplayCBDataFptr_type			getImageDisplayCBDataFptr,
								SetRestoreImgSettingsCBsFptr_type		setRestoreImgSettingsFptr, 
								OverlayROIFptr_type						overlayROIFptr,
								ClearAllROIFptr_type					clearAllROIFptr,
							 	ROIEvents_CBFptr_type					ROIEventsCBFptr,
							 	ErrorHandlerFptr_type					errorHandlerCBFptr		)
{
	displayEngine->discardFptr 					= discardFptr;
	displayEngine->displayImageFptr 			= displayImageFptr; 
	displayEngine->getImageDisplayFptr 			= getImageDisplayFptr;
	displayEngine->getImageDisplayCBDataFptr	= getImageDisplayCBDataFptr;
	displayEngine->setRestoreImgSettingsFptr	= setRestoreImgSettingsFptr;
	displayEngine->overlayROIFptr				= overlayROIFptr;
	displayEngine->clearAllROIFptr				= clearAllROIFptr;
	displayEngine->ROIEventsCBFptr				= ROIEventsCBFptr;
	displayEngine->errorHandlerCBFptr			= errorHandlerCBFptr;
}

int init_ImageDisplay_type	(	ImageDisplay_type*						imageDisplay,
								DiscardFptr_type						discardFptr,
								DisplayEngine_type* 					displayEngine,
								ImgDisplayCBData_type					imageDisplayCBData		)
{
	int		error	= 0;
	
	// init
	imageDisplay->discardFptr					= discardFptr;
	imageDisplay->displayEngine					= displayEngine;
	imageDisplay->imageDisplayCBData			= imageDisplayCBData;
	imageDisplay->ROIs							= 0;
	imageDisplay->ROIAction						= ROI_Placed;
	
	imageDisplay->nRestoreSettingsCBs			= 0;
	imageDisplay->restoreSettingsCBs			= NULL;
	imageDisplay->restoreSettingsCBsData		= NULL;
	imageDisplay->discardCallbackDataFunctions	= NULL;
	
	// allocate
	nullChk( imageDisplay->ROIs					= ListCreate(sizeof(ROI_type*)) );
	
	return 0;
	
Error:
	
	return error;
	
}

void discard_ImageDisplay_type (ImageDisplay_type** imageDisplayPtr)
{
	ImageDisplay_type* 	imageDisplay = *imageDisplayPtr;
	
	if (!imageDisplay) return;
	
	//----------------------------------------------------------
	// ROIs
	size_t		nROIs 	= ListNumItems(imageDisplay->ROIs);
	ROI_type** 	ROIPtr	= NULL;
	for (size_t i = 1; i <= nROIs; i++) {
		ROIPtr = ListGetPtrToItem(imageDisplay->ROIs, i);
		discard_ROI_type(ROIPtr);
	}
	ListDispose(imageDisplay->ROIs);
	//----------------------------------------------------------
	
	//----------------------------------------------------------
	// Restore callbacks
	for (size_t i = 0; i < imageDisplay->nRestoreSettingsCBs; i++)
		if (imageDisplay->discardCallbackDataFunctions[i])
			(*imageDisplay->discardCallbackDataFunctions[i]) (&imageDisplay->restoreSettingsCBsData[i]);	
		else
			OKfree(imageDisplay->restoreSettingsCBsData[i]);
	
	imageDisplay->nRestoreSettingsCBs = 0;
	OKfree(imageDisplay->discardCallbackDataFunctions);
	OKfree(imageDisplay->restoreSettingsCBs);
	OKfree(imageDisplay->restoreSettingsCBsData);
	//----------------------------------------------------------
	
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
	
}
