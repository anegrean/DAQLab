//==============================================================================
//
// Title:		NIDisplayEngine.h
// Purpose:		A short description of the interface.
//
// Created on:	3-2-2015 at 17:41:58 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __NIDisplayEngine_H__
#define __NIDisplayEngine_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include <nivision.h>
#include "DisplayEngine.h"

//==============================================================================
// Constants

//==============================================================================
// Types

typedef struct NIDisplayEngine	NIDisplayEngine_type;	// Child class of DisplayEngine_type

typedef struct NIImageDisplay	NIImageDisplay_type;	// Child class of ImageDisplay_type 

//==============================================================================
// External variables

//==============================================================================
// Global functions
		

//--------------------------------------------------------------------------------------------------------------------------
// NI Vision Engine management
//--------------------------------------------------------------------------------------------------------------------------
		
NIDisplayEngine_type*				init_NIDisplayEngine_type		(intptr_t 						parentWindowHndl,
																     ROIEvents_CBFptr_type			ROIEventsCBFptr,
																	 ImageDisplay_CBFptr_type		imgDisplayEventCBFptr,
																	 ErrorHandlerFptr_type			errorHandlerCBFptr);

	// Disposes NI Vision Images
void 								discard_Image_type 				(Image** image);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __NIDisplayEngine_H__ */
