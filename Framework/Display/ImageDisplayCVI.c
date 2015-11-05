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

	
	   init_ImageDisplay_type(&imgDisplay->baseClass,(DiscardFptr_type)discard_ImageDisplayCVI_type, (DisplayImageFptr_type)DisplayImageFunction, NULL);							
	
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

/*  to see the types
size_t GetImageSizeofData (Image_type* image)
{
	size_t dataTypeSize = 0;
	
	switch (image->imageType) {
			
		case Image_UChar:					// 8 bit unsigned
		  	dataTypeSize = sizeof(char);
			break;
	
		case Image_UShort:					// 16 bit unsigned   
		case Image_Short:					// 16 bit signed   
		   dataTypeSize = sizeof(short int);
			break;
			
		case Image_UInt:					// 32 bit unsigned
		case Image_Int:						// 32 bit signed
			dataTypeSize = sizeof(int);
			break;
			
		case Image_Float:					// 32 bit float   :
			dataTypeSize = sizeof(float);
			break;
			
		case Image_RGBA:
			dataTypeSize = sizeof (RGBA_type);
			break;
			
		case Image_RGBAU64:
			dataTypeSize = sizeof (RGBAU64_type);
			break;
			
	}
	
	return dataTypeSize;
}
*/

static unsigned char* convertToBitArray(Image_type* image) 
{
INIT_ERR
	
	nullChk(image);

	int size = image->width * image->height;
	int i 	 = 0;
	unsigned char bits[size];
	
	switch(image->imageType) {
		
		case Image_UChar:					// 8 bit unsigned 
			
			unsigned char* iterr;
			iterr = (unsigned char*) image->pixData[i];
			
			for(i = 0; i < size; i++) {
				bits[i] = iterr[i];
			}
		break;
		
		case Image_UShort:					// 16 bit unsigned   
		case Image_Short:					// 16 bit signed
			unsigned short int max, min;
			
			max = (unsigned short int) image->pixData[0];
			min = (unsigned short int) image->pixData[0];
			
			unsigned short int* iterr;
			iterr = (unsigned short int*) image->pixData;
			for(i = 0; i < size; i++) {
			
			if(max > iterr[i])
				max = iterr[i];
			
			if(min < iterr[i])
				min = iterr[i];
			
			}
			int intervalLength = (max - min) / sizeof(unsigned char);
			
			for(i = 0; i < size; i++) {
				bits[i] = iterr[i] / intervalLength;
			}
			
		break;
		
		case Image_UInt:					// 32 bit unsigned
		case Image_Int:						// 32 bit signed
			
			unsigned  int max, min;
			
			max = (unsigned  int) image->pixData[0];
			min = (unsigned  int) image->pixData[0];
			
			unsigned int* iterr;
			iterr = (unsigned  int*) image->pixData;
			
			for(i = 0; i < size; i++) {
			
				if(max > iterr[i])
					max = iterr[i];
			
				if(min < iterr[i])
					min = iterr[i];
			
			}
			
			int intervalLength = (max - min) / sizeof(unsigned char);
			
			for(i = 0; i < size; i++) {
				bits[i] = iterr[i] / intervalLength;
			}
		break;
		
		case Image_Float:					// 32 bit float   :
			 float max, min;
			 
			max = (float) image->pixData[0];
			min = (float) image->pixData[0];
			
			float* iterr;
			iterr = (float*) image->pixData;
			
			for(i = 0; i < size; i++) {
			
				if(max > iterr[i])
					max = iterr[i];
			
				if(min < iterr[i])
					min = iterr[i];
			
			}
			
			int intervalLength = (max - min) / sizeof(unsigned char);
			
			for(i = 0; i < size; i++) {
				bits[i] = iterr[i] / intervalLength;
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

	int BitmapID = 0;
	Image_type* ImageCVI = *image;
	unsigned char bits[] = convertToBitArray(ImageCVI);;
	errChk(NewBitmap (-1, 24, ImageCVI->width, ImageCVI->height, NULL, bits, NULL, &BitmapID));
	
	return BitmapID;
Error:
	return -1;
}

static int DisplayImageFunction (ImageDisplayCVI_type* imgDisplay, Image_type** image)
{
	return 0;
}

