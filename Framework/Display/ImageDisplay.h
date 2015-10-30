//==============================================================================
//
// Title:		ImageDisplay.h
// Purpose:		Generic image display interface.
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
// Types


typedef struct ImageDisplay						ImageDisplay_type;					// Generic image display class to inherit from.

typedef struct ChannelGroupDisplay				ChannelGroupDisplay_type;			// Used to group several image displays that show an image in different color channels or combined in a composite image. 
																					// All images must have the same dimension and they share the same regions of interest (ROIs).
																					
typedef struct ChannelGroupDisplayContainer		ChannelGroupDisplayContainer_type;	// Container for multiple channel display groups. Image dimensions between different channel groups need not be the same.
																	

//--------------------------------------------------------------
// Functions
//--------------------------------------------------------------

// Displays or updates an image in a display window
typedef int				(*DisplayImageFptr_type)					(ImageDisplay_type* imgDisplay, Image_type** image);

//--------------------------------------------------------------		
// Enums
//--------------------------------------------------------------		

// Image display events
typedef enum {
	// General
	ImageDisplay_Discard,											// Image display window was discarded.
	ImageDisplay_RestoreSettings,									// The image display window wishes to restore a list of settings that were used to acquire the image.
	
	// Region of interest (ROI) management
	ImageDisplay_ROI_Placed,										// A ROI (region of interest) was placed on the image, but not added to it.
	ImageDisplay_ROI_Added,											// A ROI was added to the image.
	ImageDisplay_ROI_Removed,										// A ROI was removed from the image.
	
} ImageDisplayEvents;

// Allowed operations on ROIs
typedef enum {
	ROI_Visible,													// ROI is visible on the image.
	ROI_Hide,														// ROI is hidden.
	ROI_Delete														// ROI is deleted from the image.
} ROIActions;

//--------------------------------------------------------------		
// Generic image display class
//--------------------------------------------------------------
		
struct ImageDisplay {
	//----------------------------------------------------
	// DATA
	//----------------------------------------------------
	
	CmtTSVHandle						imageTSV;					// Thread safe variable of Image_type* storing the displayed image which can be accessed safely from multiple threads.
	
	
	//----------------------------------------------------
	// METHODS (override with child class implementation)
	//----------------------------------------------------
	
	DiscardFptr_type					imageDisplayDiscardFptr;	// Method to discard the image display.
	DisplayImageFptr_type				displayImageFptr;			// Method to display an image.
	

	//----------------------------------------------------
	// CALLBACKS
	//----------------------------------------------------
	
	CallbackGroup_type*					callbackGroup;				// Callback function of the form CallbackFptr_type within the callback group receive events of ImageDisplayEvents type.  			   
};
	
	
//==============================================================================
// Global functions


//--------------------------------------------------------------------------------------------------------------------------
// Generic image display class management
//--------------------------------------------------------------------------------------------------------------------------

// Initializes the generic image display class.
int										init_ImageDisplay_type						(ImageDisplay_type* 		imageDisplay,
																			 	 	 DiscardFptr_type			imageDisplayDiscardFptr,
																				  	 DisplayImageFptr_type		displayImageFptr,
																			 	 	 CallbackGroup_type**		callbackGroupPtr);

void									discard_ImageDisplay_type					(ImageDisplay_type** imageDisplayPtr);

//--------------------------------------------------------------------------------------------------------------------------
// Channel Group Display
//--------------------------------------------------------------------------------------------------------------------------

// Creates a channel display group with index chanDisplayGroupIdx containing up to nChannels.
ChannelGroupDisplay_type*				init_ChannelGroupDisplay_type				(char name[]);

void									discard_ChannelGroupDisplay_type			(ChannelGroupDisplay_type** chanGroupDisplayPtr);

// Given a 1-based ROI index, the function performs an action on the ROI from a given channel display within the group. If channelIdx is 0 then the action applies to all channels.
// If ROIIdx is 0 then the action applies to all ROIs.
int										ChannelGroupDisplayROIAction				(ChannelGroupDisplay_type* chanGroupDisplay, size_t channelIdx, size_t ROIIdx, ROIActions action, char** errorMsg);

// Adds an image to the channel group display with a given channel index. Any existing ROIs from the provided image are discarded and they must be added back afterwards. The image width and height in pixels
// must be the same for all images added to the channel group display.
int										ChannelGroupDisplayAddImageChannel			(ChannelGroupDisplay_type* chanGroupDisplay, size_t chanIdx, Image_type** imagePtr);

//--------------------------------------------------------------------------------------------------------------------------
// Channel Group Display Container
//--------------------------------------------------------------------------------------------------------------------------

ChannelGroupDisplayContainer_type*		init_ChannelGroupDisplayContainer_type		(void);

void									discard_ChannelGroupDisplayContainer_type	(ChannelGroupDisplayContainer_type** containerPtr);

// Adds a channel group display to the container and returns its 1-based handle index within the container. On failure the function returns 0.
size_t									AddChannelGroupDisplayToContainer			(ChannelGroupDisplayContainer_type* container, ChannelGroupDisplay_type** chanGroupDisplayPtr);

// Gets the 1-based active channel group index within the container. If 0, there is no active channel group display.
size_t									GetActiveChannelGroupDisplayIndex			(ChannelGroupDisplayContainer_type* container);
 

#ifdef __cplusplus
    }
#endif

#endif  /* ndef __ImageDisplay_H__ */




