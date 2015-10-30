//==============================================================================
//
// Title:		ImageDisplay.h
// Purpose:		A short description of the interface.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

#ifndef __ImageDisplay_H__
#define __ImageDisplay_H__

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
	//------------
	// Classes
	//------------
	
	typedef struct genericImageDisplay		GenericImageDisplay_type;	  // Generic image display class
	
	//------------													
	// Data types
	//------------
	typedef void* GetImgDisplayCBData_type;
	typedef void* ImgDisplayCBData_type; 
	
	// TO DO:
	
	// add enum with minimum set of events
	// add extra event parameter to image display CB
	
	//--------------------------------------
	// Callback typedefs
	//--------------------------------------
		// Displays or updates an image in a display window
	typedef int								(*DisplayImageFptr_type)					(GenericImageDisplay_type* imgDisplay, Image_type** image);

	// Generic function type to dispose of dinamically allocated data
	typedef void 							(*DiscardFptr_type) 						(void** objectPtr);    
		// Obtains a display handle from the display engine that can be passed to other functions like updating the image
	typedef GenericImageDisplay_type*		(*GetImageDisplayFptr_type)					(GenericImageDisplay_type* displayEngine, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType);

		// Returns the callback data associated with the display handle
	typedef GetImgDisplayCBData_type		(*GetImageDisplayCBDataFptr_type)			(GenericImageDisplay_type* imgDisplay);

	
//==============================================================================
// Classes to inherit from
	
	struct genericImageDisplay {  //parent obj
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	
	
	//---------------------------------------------------------------------------------------------------------------
	// METHODS (override with child class implementation)
	//---------------------------------------------------------------------------------------------------------------
	
	DiscardFptr_type					imageDiscardFptr;				// Method to discard imageData.
	
	DisplayImageFptr_type				displayImageFptr;
	
	GetImageDisplayFptr_type			getImageFptr;

	//---------------------------------------------------------------------------------------------------------------
	// CALLBACKS (provide action to be taken by the module making use of the display engine)
	//---------------------------------------------------------------------------------------------------------------
	
	
	CallbackGroup_type*					callbackGroup; 
	
	}; 
	

	
//==============================================================================
// External variables

//==============================================================================
// Global functions

	
// Initializes a generic display engine instance.
	
void									init_GenericImageDisplay_type			(GenericImageDisplay_type* 				ImageDisplay,
																				 DiscardFptr_type						imageDiscardFptr,
																				 DisplayImageFptr_type					displayImageFptr,
																				 CallbackGroup_type**					callbackGroupPtr);
 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplay_H__ */

 /*
GenericImageDisplay_type  <- clasa parinte = interfata ca sa cadem de acord

ImageDisplayCVI_type

// look in displayEngine .h and .c 

*/


