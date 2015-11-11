//==============================================================================
//
// Title:		ImageDisplay.h
// Purpose:		A short description of the interface.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian Negrean.
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

ImageDisplayCVI_type*		init_ImageDisplayCVI_type		(int parentPanHandl, char displayTitle[],  void *callbackData);
						
void						discard_ImageDisplayCVI_type	(ImageDisplayCVI_type** imageDisplayPtr);

// Adds an image to display. Multiple images can be added to the same display and the user can make use of a scrollbar to view each image.
int							DisplayImage					(ImageDisplayCVI_type* imageDisp, Image_type* images[]);




struct ImageDisplayCVI {
ImageDisplay_type baseClass; 
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	ListType				images;				// List of Image_type* elements. Data in this image is used to update the display whenever needed. Note: do not modify this data directly!
	BOOL					update;				// If True, the plot will display each time the latest waveform added to it. If False, the user can use the scrollbar to return to a previous plot. Default: True.
	
	// UI
	int						displayPanHndl;
	int						canvasPanHndl;
	int						menuBarHndl;
	int						menuID_Close;
	int						menuID_Save;
	//int						scrollbarCtrl;
	
	// CALLBACK
	//WaveformDisplayCB_type	callbackFptr;
	void*					callbackData;
};


#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplayCVI_H__ */
