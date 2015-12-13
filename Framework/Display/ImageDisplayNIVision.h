//==============================================================================
//
// Title:		ImageDisplayNIVision.h
// Purpose:		A short description of the interface.
//
// Created on:	3-2-2015 at 17:41:58 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __ImageDisplayNIVision_H__
#define __ImageDisplayNIVision_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "ImageDisplay.h"

//==============================================================================
// Constants

//==============================================================================
// Types

typedef struct ImageDisplayNIVision		ImageDisplayNIVision_type;	// Child class of ImageDisplay_type 

//==============================================================================
// External variables

//==============================================================================
// Global functions	


// Initializes a display handle using NI Vision development module. If known, the parameters imageType, imgWidth and imgHeight should be supplied
// as memory allocation can be done only once at this point and reused when updating the display with a similar image.
ImageDisplayNIVision_type* 			init_ImageDisplayNIVision_type			(void*						imageDisplayOwner,
																			 int						parentPanHndl, 
																			 ImageTypes 				imageType,
																			 int						imgWidth,
																			 int						imgHeight,
																			 CallbackGroup_type**		callbackGroupPtr);

void								discard_ImageDisplayNIVision_type		(ImageDisplayNIVision_type** imageDisplayPtr);								

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplayNIVision_H__ */
