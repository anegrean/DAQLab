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

// Used to group several image displays that show an image in different color channels or combined in a composite image. 
// All images must have the same dimension and they share the same ROIs.

struct ChannelGroupDisplay {
	char*							name;							// Channel group display name.
	size_t							displayGroupIdx;				// Index assigned to this display group which can be used to identify it uniquely among several groups within a channel group display container.
	ImageDisplay_type**				imgDisplays;					// Array of image displays, with the 0-th index being the composite image and continuing with each scan channel. Unused channels are set to NULL.
	size_t							nDisplays;						// Number of elements in the imgDisplays array.
};

struct ChannelGroupDisplayContainer {
	ChannelGroupDisplay_type**		channelGroupDisplays;			// Array of display groups generated with each new framescan.
	size_t							nChannelGroupDisplays;			// Number of elements in the diplayGroups array.
	size_t							activeChannelGroupDisplayIdx;   // 1-based index of the active channel group display within the container. If 0, there are no active displays.
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

int init_ImageDisplay_type (ImageDisplay_type* 				imageDisplay,
							void*							imageDisplayOwner,
							Image_type**					imagePtr,
							DiscardFptr_type				imageDisplayDiscardFptr,
							DisplayImageFptr_type			displayImageFptr,
							ROIActionsFptr_type				ROIActionsFptr,
							CallbackGroup_type**			callbackGroupPtr)
{
INIT_ERR
	
	//----------------------------------------------------------
	// Init
	//----------------------------------------------------------
	
	// data
	imageDisplay->imageDisplayOwner			= imageDisplayOwner;
	
	if (imagePtr) {
		imageDisplay->image					= *imagePtr;
		*imagePtr							= NULL;
	} else
		imageDisplay->image					= NULL;
		
	imageDisplay->visible					= FALSE;
	imageDisplay->selectionROI				= NULL;
	imageDisplay->addROIToImage				= FALSE;
	
	// methods
	imageDisplay->imageDisplayDiscardFptr 	= imageDisplayDiscardFptr;
	imageDisplay->displayImageFptr			= displayImageFptr;
	imageDisplay->ROIActionsFptr			= ROIActionsFptr;
	
	// callbacks
	if (callbackGroupPtr) {
		imageDisplay->callbackGroup			= *callbackGroupPtr;
		*callbackGroupPtr					= NULL;
		SetCallbackGroupOwner(imageDisplay->callbackGroup, imageDisplay);
	} else
		imageDisplay->callbackGroup			= NULL;
		
	
 
Error:
	
	return errorInfo.error;
}

void discard_ImageDisplay_type (ImageDisplay_type** imageDisplayPtr)
{
	ImageDisplay_type* 		imageDisplay 	= *imageDisplayPtr;
	
	if (!imageDisplay) return;
	
	// discard ROI selection if any
	if (imageDisplay->selectionROI)
		(*imageDisplay->selectionROI->discardFptr) ((void**)&imageDisplay->selectionROI);
	
	// dispose image
	discard_Image_type(&imageDisplay->image);
	
	// discard callback group
	discard_CallbackGroup_type(&imageDisplay->callbackGroup);
	
	OKfree(*imageDisplayPtr);
}

//-----------------------------------------------------------------------------------------------------------------------
// Channel group display
//-----------------------------------------------------------------------------------------------------------------------

ChannelGroupDisplay_type* init_ChannelGroupDisplay_type	(char name[])
{
INIT_ERR

	ChannelGroupDisplay_type*	chanGroupDisplay = malloc (sizeof(ChannelGroupDisplay_type));
	
	if (!chanGroupDisplay) return NULL;
	
	// init
	chanGroupDisplay->name					= NULL;
	chanGroupDisplay->displayGroupIdx		= 0;
	chanGroupDisplay->nDisplays				= 0;
	chanGroupDisplay->imgDisplays			= NULL;
	
	// alloc
	nullChk( chanGroupDisplay->name	= StrDup(name) );
	
	return chanGroupDisplay;
	
Error:
	
	discard_ChannelGroupDisplay_type(&chanGroupDisplay);
	return NULL;
}

void discard_ChannelGroupDisplay_type (ChannelGroupDisplay_type** chanGroupDisplayPtr)
{
	ChannelGroupDisplay_type*	chanGroupDisplay = *chanGroupDisplayPtr;
	if (!chanGroupDisplay) return;
	
	// discard image displays
	for (size_t i = 0; i < chanGroupDisplay->nDisplays; i++)
		discard_ImageDisplay_type(&chanGroupDisplay->imgDisplays[i]);
	
	OKfree(chanGroupDisplay->imgDisplays);
	
	// group name
	OKfree(chanGroupDisplay->name);
	
	OKfree(*chanGroupDisplayPtr);
}

int ChannelGroupDisplayROIAction (ChannelGroupDisplay_type* chanGroupDisplay, size_t channelIdx, size_t ROIIdx, ROIActions action, char** errorMsg)
{
INIT_ERR

#define ChannelGroupDisplayROIAction_Err_NoMethod	-1
#define ChannelGroupDisplayROIAction_Err_OutOfRange	-2
#define ChannelGroupDisplayROIAction_Err_NoChannels	-3


	// check if there are any channels in the group
	if (!chanGroupDisplay->nDisplays)
		SET_ERR(ChannelGroupDisplayROIAction_Err_NoChannels, "There are no channels in the group on which to perform the requested ROI action.");

	// check if there is an ROI actions method implemented for all channels
	for (size_t i = 0; i < chanGroupDisplay->nDisplays; i++)
		if (chanGroupDisplay->imgDisplays[i] && !chanGroupDisplay->imgDisplays[i]->ROIActionsFptr)
			SET_ERR(ChannelGroupDisplayROIAction_Err_NoMethod, "ROI actions method not implemented for one or more image channels in the channel group display.");

	// check if requested channel index is within range
	if (channelIdx > chanGroupDisplay->nDisplays)
		SET_ERR(ChannelGroupDisplayROIAction_Err_OutOfRange, "Channel index is out of range.");
	
	if (!channelIdx) {
		for (size_t i = 0; i < chanGroupDisplay->nDisplays; i++)
			if (chanGroupDisplay->imgDisplays[i])
				(*chanGroupDisplay->imgDisplays[i]->ROIActionsFptr) (chanGroupDisplay->imgDisplays[i], ROIIdx, action);
	} else
		if (chanGroupDisplay->imgDisplays[channelIdx-1])
				(*chanGroupDisplay->imgDisplays[channelIdx-1]->ROIActionsFptr) (chanGroupDisplay->imgDisplays[channelIdx-1], ROIIdx, action);
	
Error:
	
RETURN_ERR
}

int ChannelGroupDisplayAddImageDisplay (ChannelGroupDisplay_type* chanGroupDisplay, size_t chanIdx, ImageDisplay_type** imageDisplayPtr, char** errorMsg)
{
INIT_ERR

#define ChannelGroupDisplayAddImageChannel_Err_ImageSizeMismatch	-1

	// incoming image display
	ImageDisplay_type*		imageDisplay					= *imageDisplayPtr;
	int						imageWidth						= 0;
	int						imageHeight						= 0;
	
	// image display from channel group
	int						chanGroupImageWidth				= 0;
	int						chanGroupImageHeight			= 0;
	
	// get incoming image dimensions
	GetImageSize(imageDisplay->image, &imageWidth, &imageHeight);
	
	// check first if image size is the same compared to the first non-empty channel
	for (size_t i = 0; i < chanGroupDisplay->nDisplays; i++)
		if (chanGroupDisplay->imgDisplays[i]) {
			// get channel group image dimensions
			GetImageSize(chanGroupDisplay->imgDisplays[i]->image, &chanGroupImageWidth, &chanGroupImageHeight);
			// compare dimensions
			if (chanGroupImageWidth != imageWidth || chanGroupImageHeight != imageHeight)
				SET_ERR(ChannelGroupDisplayAddImageChannel_Err_ImageSizeMismatch, "Dimensions of image to be added must be the same with the dimension of all images in the channel group.");
			break;
		}
	
	// if index is larger than the current number of channels, then allocate more memory and initialize extra channels to NULL
	if (chanIdx >= chanGroupDisplay->nDisplays) {
		nullChk( chanGroupDisplay->imgDisplays = realloc(chanGroupDisplay->imgDisplays, (chanIdx+1)*sizeof(ImageDisplay_type*)) );
		for (size_t i = chanGroupDisplay->nDisplays; i <= chanIdx; i++)
			chanGroupDisplay->imgDisplays[i] = NULL;
	}
	
	// discard existing channel display
	discard_ImageDisplay_type(&chanGroupDisplay->imgDisplays[chanIdx]);
	
	// assign new image display to the chosen channel within the channel display group
	chanGroupDisplay->imgDisplays[chanIdx] = *imageDisplayPtr;
	*imageDisplayPtr = NULL;

Error:
	
RETURN_ERR	
}

//--------------------------------------------------------------------------------------------------------------------------
// Channel Group Display Container
//--------------------------------------------------------------------------------------------------------------------------

ChannelGroupDisplayContainer_type* init_ChannelGroupDisplayContainer_type (void)
{
	ChannelGroupDisplayContainer_type*	container = malloc (sizeof(ChannelGroupDisplayContainer_type));
	if (!container) return NULL;
	
	// init
	container->nChannelGroupDisplays			= 0;
	container->channelGroupDisplays				= NULL;
	container->activeChannelGroupDisplayIdx		= 0;
	
	return container;
}

void discard_ChannelGroupDisplayContainer_type (ChannelGroupDisplayContainer_type** containerPtr)
{
	ChannelGroupDisplayContainer_type*	container = *containerPtr;
	if (!container) return;
	
	// discard channel display groups
	for (size_t i = 0; i < container->nChannelGroupDisplays; i++)
		discard_ChannelGroupDisplay_type(&container->channelGroupDisplays[i]);
	
	OKfree(container->channelGroupDisplays);
	
	OKfree(*containerPtr);
}

// Adds a channel group display to the container and returns its 1-based handle index within the container. On failure the function returns 0.
size_t AddChannelGroupDisplayToContainer (ChannelGroupDisplayContainer_type* container, ChannelGroupDisplay_type** chanGroupDisplayPtr)
{
INIT_ERR

	ChannelGroupDisplay_type**		emptyGroupPtr			= NULL;
	ChannelGroupDisplay_type**		enlargedDisplayGroups	= NULL;
	size_t							emptyGroupIdx			= 0;
	
	// check if there is an empty position for adding the display group
	for (emptyGroupIdx = 0; emptyGroupIdx < container->nChannelGroupDisplays; emptyGroupIdx++)
		if (!container->channelGroupDisplays[emptyGroupIdx]) {
			emptyGroupPtr = &container->channelGroupDisplays[emptyGroupIdx];
			break;
		}
	
	// if there is no empty position, then increase number of display groups in the container
	if (!emptyGroupPtr) {
		nullChk( enlargedDisplayGroups = realloc (container->channelGroupDisplays, (container->nChannelGroupDisplays+1) * sizeof(ChannelGroupDisplay_type*)) );
		emptyGroupIdx 									= container->nChannelGroupDisplays; 
		container->nChannelGroupDisplays++;
		container->channelGroupDisplays 				= enlargedDisplayGroups;
		container->channelGroupDisplays[emptyGroupIdx] 	= NULL;
		emptyGroupPtr									= &container->channelGroupDisplays[emptyGroupIdx];
	}
	
	// add channel group display to the container
	*emptyGroupPtr 			= *chanGroupDisplayPtr;
	*chanGroupDisplayPtr 	= NULL;
	
	// update active channel group index
	container->activeChannelGroupDisplayIdx = emptyGroupIdx+1;
	
	return emptyGroupIdx+1;
	
Error:
	
	return 0;
	
}

size_t GetActiveChannelGroupDisplayIndex (ChannelGroupDisplayContainer_type* container)
{
	return container->activeChannelGroupDisplayIdx;
}
