//==============================================================================
//
// Title:		ImageDisplayCVI2.h
// Purpose:		A short description of the interface.
//
// Created on:	21-2-2016 at 19:19:44 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
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
