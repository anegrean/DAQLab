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

	
	   //init_ImageDisplay_type(&imgDisplay->baseClass,NULL, &imgDisplay, (DiscardFptr_type)discard_ImageDisplayCVI_type, (DisplayImageFptr_type)DisplayImageFunction,NULL , NULL);
	   init_ImageDisplay_type(&imgDisplay->baseClass, &imgDisplay, NULL, (DiscardFptr_type) discard_ImageDisplayCVI_type, (DisplayImageFptr_type) DisplayImageFunction, NULL, NULL);

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


static unsigned char* convertToBitArray(Image_type* image) 
{
INIT_ERR
	
	nullChk(image);

	int 			width 	 	= 0;
	int 			height	 	= 0;
	int 			nBits 	 	= 0;	  
	int 			i 		 	= 0;
	unsigned char*	bits		= NULL;

	GetImageSize(image, &width, &height);
	
	nBits = width * height;

	nullChk( bits = (unsigned char*) malloc(nBits * sizeof(unsigned char)) );                              
	
	switch(GetImageType(image)) {
		
		case Image_UChar:					// 8 bit unsigned 
			
			unsigned char* iterr_char = GetImagePixelArray(image);
		
			for(i = 0; i < nBits; i++)
				bits[i] = iterr_char[i];
			
			break;
		
		case Image_UShort:					// 16 bit unsigned   
		case Image_Short:					// 16 bit signed
			unsigned short int max_short, min_short;
			

			unsigned short int* iterr_short = GetImagePixelArray(image);
		
			max_short = iterr_short[0];
			min_short = iterr_short[0];
			
			for(i = 0; i < nBits; i++) {
			
				if(max_short > iterr_short[i])
					max_short = iterr_short[i];
			
				if(min_short < iterr_short[i])
					min_short = iterr_short[i];
			
			}
			int intervalLength_short = (max_short - min_short) / sizeof(unsigned char);
			
			for(i = 0; i < nBits; i++) {
				bits[i] = iterr_short[i] / intervalLength_short;
			}
		break;
		
		case Image_UInt:					// 32 bit unsigned
		case Image_Int:						// 32 bit signed
			
			unsigned  int max_int, min_int;
			
			unsigned int* iterr_int = (unsigned  int*) GetImagePixelArray(image);
						;
			max_int = iterr_int[0];
			min_int = iterr_int[0];
			
			for(i = 0; i < nBits; i++) {
			
				if(max_int > iterr_int[i])
					max_int = iterr_int[i];
			
				if(min_int < iterr_int[i])
					min_int = iterr_int[i];
			
			}
			
			int intervalLength_int = (max_int - min_int) / sizeof(unsigned char);
			
			for(i = 0; i < nBits; i++) {
				bits[i] = iterr_int[i] / intervalLength_int;
			}
		break;
		
		case Image_Float:					// 32 bit float   :
			
			float max_float, min_float;
			float* iterr_float = (float*) GetImagePixelArray(image);
			
			max_float = iterr_float[0];
			min_float = iterr_float[0];
			
			for(i = 0; i < nBits; i++) {
			
				if(max_float > iterr_float[i])
					max_float = iterr_float[i];
			
				if(min_float < iterr_float[i])
					min_float = iterr_float[i];
			
			}
			
			int intervalLength_float = (max_float - min_float) / sizeof(unsigned char);
			
			for(i = 0; i < nBits; i++) {
				bits[i] = iterr_float[i] / intervalLength_float;
			}
		break;

	
	}
		return bits;
Error:
		return NULL;
}

static int Image_typeToBitmap(Image_type** image)
{
INIT_ERR 

	int BitmapID	 = 0;
	int width 		 = 0;
	int height 		 = 0;
	
	Image_type* ImageCVI = *image;
	unsigned char *bits;
	bits = convertToBitArray(ImageCVI);
	
	GetImageSize(ImageCVI, &width, &height); 
	errChk(NewBitmap (-1, 24, width, height, NULL, bits, NULL, &BitmapID));
	
	return BitmapID;
Error:
	return -1;
}

static int DisplayImageFunction (ImageDisplayCVI_type* imgDisplay, Image_type** image)
{

	int ImageID = Image_typeToBitmap(image);
	return 0;
}

