//==============================================================================
//
// Title:		ImageDisplay.c
// Purpose:		Generic image display interface.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//

#include "ImageDisplay.h"
#include "DAQLabUtility.h"

//==============================================================================
// Constants

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

int init_ImageDisplay_type (ImageDisplay_type* 				imageDisplay,
							DiscardFptr_type				imageDisplayDiscardFptr,
							DisplayImageFptr_type			displayImageFptr,
							CallbackGroup_type**			callbackGroupPtr)
{
INIT_ERR
	
	Image_type**	imagePtr	= NULL;
	
	//----------------------------------------------------------
	// Init
	//----------------------------------------------------------
	
	// data
	imageDisplay->imageTSV					= 0;
	
	// methods
	imageDisplay->imageDisplayDiscardFptr 	= imageDisplayDiscardFptr;
	imageDisplay->displayImageFptr			= displayImageFptr;
	
	// callbacks
	imageDisplay->callbackGroup				= *callbackGroupPtr;
	*callbackGroupPtr						= NULL;
	
	//----------------------------------------------------------
	// Alloc
	//----------------------------------------------------------
	
	errChk( CmtNewTSV(sizeof(Image_type*), &imageDisplay->imageTSV) );
	// init image TSV
	errChk( CmtGetTSVPtr(imageDisplay->imageTSV, &imagePtr) );
	*imagePtr = NULL;
	CmtReleaseTSVPtr(imageDisplay->imageTSV);
	
Error:
	
	return errorInfo.error;
}

void discard_ImageDisplay_type (ImageDisplay_type** imageDisplayPtr)
{
	ImageDisplay_type* 		imageDisplay 	= *imageDisplayPtr;
	Image_type**			imagePtr		= NULL;
	
	if (!imageDisplay) return;
	
	// dispose image
	if ( CmtGetTSVPtr(imageDisplay->imageTSV, &imagePtr)  < 0) goto SkipDiscardImage;
	discard_Image_type(imagePtr);
	CmtReleaseTSVPtr(imageDisplay->imageTSV);
	
SkipDiscardImage:
	// discard image TSV
	if (imageDisplay->imageTSV) {
		CmtDiscardTSV(imageDisplay->imageTSV);
		imageDisplay->imageTSV = 0;
	}
	
	discard_CallbackGroup_type(&imageDisplay->callbackGroup);
	
	OKfree(*imageDisplayPtr);
}
