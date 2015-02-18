//==============================================================================
//
// Title:		NIVisionDisplay.h
// Purpose:		A short description of the interface.
//
// Created on:	3-2-2015 at 17:41:58 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

#ifndef __NIVisionDisplay_H__
#define __NIVisionDisplay_H__

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

typedef struct NIVisionDisplay	NIVisionDisplay_type;	// child class of DisplayEngine_type	 		

//==============================================================================
// External variables

//==============================================================================
// Global functions
		

//--------------------------------------------------------------------------------------------------------------------------
// NI Vision Engine management
//--------------------------------------------------------------------------------------------------------------------------
		
NIVisionDisplay_type*				init_NIVisionDisplay_type		(	intptr_t 						parentWindowHndl,
																	 	ROIPlaced_CBFptr_type			ROIPlacedCBFptr,
																		ROIRemoved_CBFptr_type			ROIRemovedCBFptr,
																		ErrorHandlerFptr_type			errorHandlerCBFptr);

	// Disposes NI Vision Images
void 								discard_Image_type 				(Image** image);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __NIVisionDisplay_H__ */
