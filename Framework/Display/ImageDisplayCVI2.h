//==============================================================================
//
// Title:		ImageDisplayCVI2.h
// Purpose:		Image display using CVI functions (no need for NI Vision module).
//
// Created on:	21-2-2016 at 19:19:44 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ .
//
//==============================================================================

#ifndef __ImageDisplayCVI_H__
#define __ImageDisplayCVI_H__

#ifdef __cplusplus
    extern "C" {
#endif

//==============================================================================
// Include files

#include "cvidef.h"
#include "DataTypes.h"
#include "ImageDisplay.h" 

//==============================================================================
// Constants


		
//==============================================================================
// Types
		
typedef struct ImageDisplayCVI	ImageDisplayCVI_type; 


//=============================================================================
// External variables

//==============================================================================
// Global functions

ImageDisplayCVI_type* 		init_ImageDisplayCVI_type 		(int    					parentPanHandl, 
												 			 char   					displayTitle[],
												 			 CallbackGroup_type**		callbackGroupPtr);


void						discard_ImageDisplayCVI_type	(ImageDisplayCVI_type** 	imageDisplayPtr);

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplayCVI2_H__ */
