//==============================================================================
//
// Title:		ImageDisplayCVI.c
// Purpose:		A short description of the implementation.
//
// Created on:	10/13/2015 at 2:09:46 PM by popescu.andrei1991@gmail.com.
// Copyright:	Vrije University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h"
#include "UI_ImageDisplay.h"
#include "ImageDisplayCVI.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void 			DiscardImageList 			(ListType* imageListPtr);

static int				DisplayImageFunction		(ImageDisplayCVI_type* imgDisplay, Image_type** image);

//==============================================================================
// Global variables

//==============================================================================
// Global functions


//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ImageCVIs
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------



//==============================================================================
// Constants

#define ImageDisplay_UI 	"./Framework/Display/UI_ImageDisplay.uir"   



ImageDisplayCVI_type* init_ImageDisplayCVI_type (int parentPanHandl, char displayTitle[],  void *callbackData)		  //namechng
{
INIT_ERR

	ImageDisplayCVI_type*	imgDisplay = malloc(sizeof(ImageDisplayCVI_type));
	if (!imgDisplay) return NULL;
	
	//-------------------------------------
	// Init parent class
	//-------------------------------------
	
	// call parent init fn

	
	  init_GenericImageDisplay_type(&imgDisplay->baseClass,(DiscardFptr_type)discard_ImageDisplayCVI_type, (DisplayImageFptr_type)DisplayImageFunction, NULL);							
	
	//-------------------------------------
	// Init child class
	//-------------------------------------
	
	
	// INIT
	//---------------------------------
	
	imgDisplay->displayPanHndl		= 0;
	imgDisplay->canvasPanHndl		= 0;
	imgDisplay->images				= 0;

	
	// ALLOC Resources
	//---------------------------------
	
	// loading display panel into workspace panel
	errChk( imgDisplay->displayPanHndl = LoadPanel (parentPanHandl, ImageDisplay_UI, DisplayPan) );
	
	// loading canvas panel into display panel
	errChk( imgDisplay->canvasPanHndl = LoadPanel (imgDisplay->displayPanHndl, ImageDisplay_UI, CanvasPan) ); 
	
	// create image list
	nullChk( imgDisplay->images = ListCreate(sizeof(Image_type*)) );    

	DisplayPanel (imgDisplay->canvasPanHndl);
	
	return imgDisplay;
	
Error:
	
	// cleanup
	discard_ImageDisplayCVI_type(&imgDisplay);
	return NULL;
}

static void discard_ImageDisplayCVI_type	(ImageDisplayCVI_type** imageDisplayPtr)
{
	ImageDisplayCVI_type* imgDisp = *imageDisplayPtr;
	if (!imgDisp) return;
	
	//--------------
	// Images
	//--------------
	
	DiscardImageList(&imgDisp->images);
	
	//--------------
	// UI
	//--------------
	
	OKfreePanHndl(imgDisp->displayPanHndl);
	OKfreePanHndl(imgDisp->canvasPanHndl);
	
	OKfree(* imageDisplayPtr);
	
	// maybe needed in the future to discard parent resources as well
	// discard_ImageDisplay_type (imageDisplayPtr)
	
}


void discard_ImageCVI_type (ImageDisplayCVI_type** ImageCVIPtr)
{
	ImageDisplayCVI_type* ImageCVI = *ImageCVIPtr;
	if (!ImageCVI) return;				   //TODO: CHANGE TO PTRS
	

	
	OKfreePanHndl(ImageCVI->displayPanHndl);
	OKfreePanHndl(ImageCVI->canvasPanHndl);
	
	OKfree(ImageCVI);
}

void ClearImageCVIList (ListType imageList)
{
	size_t				nImages 		= ListNumItems(imageList);
	ImageDisplayCVI_type**		imageDisplayPtr = NULL;
	
	for (size_t i = 1; i <= nImages; i++) {
		imageDisplayPtr = ListGetPtrToItem(imageList, i);
		discard_ImageDisplayCVI_type(imageDisplayPtr);
	}
	ListClear(imageList);
}

void DiscardImageList (ListType* imageListPtr)
{
	ClearImageCVIList (*imageListPtr);
	OKfreeList(*imageListPtr);
}


static int Image_typeToBitmap(Image_type** image)
{
INIT_ERRaluca, [30.10.15 13:50]
hehehe pai in mod normal nu te intelegi cu propozitiileR 
	
	int BitmapID = 0;
	Image_type* ImageCVI = *image;
	
errChk(NewBitmap (ImageCVI->width, 1, ImageCVI->width, ImageCVI->height, int colorTable[], ImageCVI->pixData, unsigned char mask[], &BitmapID));
	return 0;
Error:
	return -1;
}

static int DisplayImageFunction (ImageDisplayCVI_type* imgDisplay, Image_type** image)
{
	return 0;
}

