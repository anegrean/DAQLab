//==============================================================================
//
// Title:		ImageDisplay.h
// Purpose:		Generic image display interface.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
// License:     This Source Code Form is subject to the terms of the Mozilla Public 
//              License v. 2.0. If a copy of the MPL was not distributed with this 
//              file, you can obtain one at https://mozilla.org/MPL/2.0/ . 
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
		
#define Default_ROI_R_Color					0	   	// red
#define Default_ROI_G_Color					255	   	// green
#define Default_ROI_B_Color					0		// blue
#define Default_ROI_A_Color					0		// alpha


//==============================================================================
// Types


typedef struct ImageDisplay						ImageDisplay_type;					// Generic image display class to inherit from.

typedef struct ChannelGroupDisplay				ChannelGroupDisplay_type;			// Used to group several image displays that show an image in different color channels or combined in a composite image. 
																					// All images must have the same dimension and they share the same regions of interest (ROIs).
																					
typedef struct ChannelGroupDisplayContainer		ChannelGroupDisplayContainer_type;	// Container for multiple channel display groups. Image dimensions between different channel groups need not be the same.
																	

//--------------------------------------------------------------		
// Enums
//--------------------------------------------------------------		

// Image display events passed to the callbackGroup
typedef enum {
	// General
	ImageDisplay_Close,												// Image display will be discarded. Listeners should not discard the image display directly, instead just set the display references to NULL.
	ImageDisplay_RestoreSettings,									// The image display window wishes to restore a list of settings that were used to acquire the image.
	
	// Region of interest (ROI) events
	// callbackData is of ROI_type*
	ImageDisplay_ROI_Placed,										// A ROI (region of interest) was placed on the image, but not added to it. Event data in CallbackFptr_type will be a ROI_type* of ROI selection on the image.
	ImageDisplay_ROI_Added,											// A ROI was added to the image. Event data in CallbackFptr_type will be a ROI_type* of the added ROI to the image.
	ImageDisplay_ROI_Removed,										// A ROI was removed from the image.
	
} ImageDisplayEvents;

// Allowed operations on ROIs
typedef enum {
	ROI_Show,														// ROI is visible on the image.
	ROI_Hide,														// ROI is hidden.
	ROI_Delete														// ROI is deleted from the image.
} ROIActions;

//--------------------------------------------------------------
// Functions
//--------------------------------------------------------------

// Displays or updates an image in a display window
typedef int				(*DisplayImageFptr_type)					(ImageDisplay_type* imgDisplay, Image_type** image, char** errorMsg);

// Displays a composite image from multiple images from separate R, G and B colors. All image dimensions must be the same.
typedef struct {
	size_t			nImages;	// Number of images belonging to this channel.
	Image_type** 	images;		// Image array.
	uInt32*			yOffsets;	// Image position offset in the height direction;
	uInt32*			xOffsets;   // Image position offset in the width direction;
} ColorChannel_type; 

typedef int				(*DisplayRGBImageChannels_type)				(ImageDisplay_type* imgDisplay, ColorChannel_type** RChanPtr, ColorChannel_type** GChanPtr, ColorChannel_type** BChanPtr);

// Performs a ROI operation on the image display. ROIName is the name of the ROI or ROIs from the image ROI list to which the operation applies. If ROIName is NULL, the operation applies to all ROIs.
typedef void			(*ROIActionsFptr_type)						(ImageDisplay_type* imgDisplay, char ROIName[], ROIActions action);


//--------------------------------------------------------------		
// Generic image display class
//--------------------------------------------------------------
		
struct ImageDisplay {
	//----------------------------------------------------
	// DATA
	//----------------------------------------------------
	
	void*								imageDisplayOwner;			// Reference to object that owns this image display.
	Image_type*							image;						// Image object used for display.
	BOOL								visible;					// If True, the image window is visible, False otherwise. This flag should be by the displayImageFptr method to either display a new window or just update the current image.
	ROI_type*							selectionROI;				// Keeps track of the last placed ROI on the image.
	BOOL								addROIToImage;				// If True, the selected ROI will be added to the image.
	
	
	//----------------------------------------------------
	// METHODS (override with child class implementation)
	//----------------------------------------------------
	
	DiscardFptr_type					imageDisplayDiscardFptr;	// Method to discard the image display.
	DisplayImageFptr_type				displayImageFptr;			// Method to display or update an image depending on the visible flag.
	ROIActionsFptr_type					ROIActionsFptr;				// Method to apply ROI actions to the image.
	
	
	//----------------------------------------------------
	// CALLBACKS
	//----------------------------------------------------
	
	CallbackGroup_type*					callbackGroup;				// Callback function of the form CallbackFptr_type within the callback group receive events of ImageDisplayEvents type.
																	// Since the CallbackFptr_type has the form typedef void (*CallbackFptr_type) (void* objectRef, int event, void* eventData, void* callbackFunctionData)
																	// objectRef will be the reference to the image display of ImageDisplay_type* while callbackData will depend on the event type among ImageDisplayEvents.
};
	
	
//==============================================================================
// Global functions


//--------------------------------------------------------------------------------------------------------------------------
// Generic image display class management
//--------------------------------------------------------------------------------------------------------------------------

// Initializes the generic image display class. Provide a pointer to an image if already available, otherwise set imagePtr to NULL.
int										init_ImageDisplay_type						(ImageDisplay_type* 		imageDisplay,
																					 void*						imageDisplayOwner,
																					 Image_type**				imagePtr,
																			 	 	 DiscardFptr_type			imageDisplayDiscardFptr,
																				  	 DisplayImageFptr_type		displayImageFptr,
																					 ROIActionsFptr_type		ROIActionsFptr,
																			 	 	 CallbackGroup_type**		callbackGroupPtr);

void									discard_ImageDisplay_type					(ImageDisplay_type** imageDisplayPtr);

//--------------------------------------------------------------------------------------------------------------------------
// Color Channels
//--------------------------------------------------------------------------------------------------------------------------

ColorChannel_type*						init_ColorChannel_type						(size_t nImages, Image_type** imagePtrs[], uInt32 xOffsets[], uInt32 yOffsets[]);

void									discard_ColorChannel_type					(ColorChannel_type** colorChanPtr);

//--------------------------------------------------------------------------------------------------------------------------
// Channel Group Display
//--------------------------------------------------------------------------------------------------------------------------

// Creates a channel display group with index chanDisplayGroupIdx containing up to nChannels.
ChannelGroupDisplay_type*				init_ChannelGroupDisplay_type				(char name[]);

void									discard_ChannelGroupDisplay_type			(ChannelGroupDisplay_type** chanGroupDisplayPtr);

// Given a ROI name, the function performs an action on the ROI from a given channel display within the group. If channelIdx is 0 then the action applies to all channels.
// If ROIIdx is "" then the action applies to all ROIs.
int										ChannelGroupDisplayROIAction				(ChannelGroupDisplay_type* chanGroupDisplay, size_t channelIdx, char ROIName[], ROIActions action, char** errorMsg);

// Adds an image to the channel group display with a given 0-based channel index. Any existing ROIs from the provided image are discarded and they must be added back afterwards. The image width and height in pixels
// must be the same for all images added to the channel group display.
int										ChannelGroupDisplayAddImageDisplay			(ChannelGroupDisplay_type* chanGroupDisplay, size_t chanIdx, ImageDisplay_type** imageDisplayPtr, char** errorMsg);

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




