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

static unsigned char* 	ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image);

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


// Initializes a display handle using DisplayCVI module. If known, the parameters imageType, imgWidth and imgHeight should be supplied
// as memory allocation can be done only once at this point and reused when updating the display with a similar image.
ImageDisplayCVI_type* init_ImageDisplayCVI_type (int    		parentPanHandl, 
												 char   		displayTitle[],
												 ImageTypes 	imageType,
												 int			imgWidth,
												 int			imgHeight,
												 void*			callbackData
)		 
{
INIT_ERR

	//-------------------------------------
	// Sanity checks
	//-------------------------------------
	
	// height and width only valid if strict positive
	if(imgHeight <= 0 || imgWidth <= 0) 
	return NULL;

	ImageDisplayCVI_type*	imgDisplay = malloc(sizeof(ImageDisplayCVI_type));
	if (!imgDisplay) return NULL;
	
	
	//-------------------------------------
	// Init parent class
	//-------------------------------------
	
	// call parent init fn

	
	init_ImageDisplay_type(&imgDisplay->baseClass, &imgDisplay, NULL, (DiscardFptr_type) discard_ImageDisplayCVI_type, (DisplayImageFptr_type) DisplayImageFunction, NULL, NULL);

	
	//-------------------------------------
	// Init child class
	//-------------------------------------
	
	
	// INIT
	//---------------------------------
	
	imgDisplay->displayPanHndl		= 0;
	imgDisplay->canvasPanHndl		= 0;
	imgDisplay->images				= 0;
	imgDisplay->imgBitmapID			= -1;
	imgDisplay->nBytes				= imgHeight * imgWidth * BytesPerPixelDepth;
	imgDisplay->imgHeight			= imgHeight;
	imgDisplay->imgWidth			= imgWidth;
	imgDisplay->bitmapBitArray		= NULL;

	
	// ALLOC Resources
	//---------------------------------
	
	if(imgHeight > 0 && imgWidth > 0)
		nullChk(imgDisplay->bitmapBitArray = malloc(BytesPerPixelDepth * imgWidth * imgHeight));
	else
		goto Error;
		
	// loading display panel into workspace panel
	errChk( imgDisplay->displayPanHndl = LoadPanel (parentPanHandl, ImageDisplay_UI, DisplayPan) );

	// loading canvas panel into display panel
	errChk( imgDisplay->canvasPanHndl = LoadPanel (imgDisplay->displayPanHndl, ImageDisplay_UI, CanvasPan) ); 
	
	// create image list
	nullChk( imgDisplay->images = ListCreate(sizeof(Image_type*)) );

	// create bitmap bit array
	nullChk( imgDisplay->bitmapBitArray = initBitArray(imgDisplay->nBytes));
	
	// create bitmap image handler
	errChk(NewBitmap (-1, 24, imgWidth, imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID));
	
	

	DisplayPanel (imgDisplay->canvasPanHndl);
	DisplayPanel(imgDisplay->displayPanHndl);
	
	return imgDisplay;
	
Error:
	
	// cleanup
	discard_ImageDisplayCVI_type(&imgDisplay);
	return NULL;
}

unsigned char* initBitArray(int nBytes)
{
	unsigned char* bytes = NULL;
	
	bytes = (unsigned char*) malloc(nBytes * sizeof(unsigned char));
	
	if (!bytes) return NULL;
	return bytes;
}

static void discard_ImageDisplayCVI_type	(ImageDisplayCVI_type** imageDisplayPtr)
{
	ImageDisplayCVI_type* imgDisp = *imageDisplayPtr;
	if (!imgDisp) return;
	
	//--------------
	// Images
	//--------------
	
	DiscardImageList(&imgDisp->images);
	
	if(imgDisp->imgBitmapID >= 0)
		DiscardBitmap(imgDisp->imgBitmapID);
	
	OKfree(imgDisp->bitmapBitArray);
	
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

//TODO: split function in grayscale and color, to avoid unnecessary pixArray iterration
static unsigned char* ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image) 
{
INIT_ERR
		int i 			= 0;
		int j 			= 0;
		int iterrations = display->nBytes;		
		nullChk(image);

		unsigned char* pixArray = (unsigned char*) GetImagePixelArray(image);
			
		
		
		switch(GetImageType(image)) {
				
									
			case Image_UChar:					//8bit - nothing to do 
			break;
			
			case Image_UShort:					// 16 bit unsigned
			case Image_Short:					// 16 bit signed
			break;
			
			case Image_UInt:					// 32 bit unsigned
			case Image_Int:						// 32 bit signed
			break;

			case Image_Float:					// 32 bit float 
			break;
			
		}
		
		//for all greyscale images
			//for each 32 bit value
			for(i = 0; i < iterrations; i = i + BytesPerPixelDepth) {
				
				//for each of the 4 bytes of the 32 bit value
				for(j = 0; j < BytesPerPixelDepth; j++) {			  
					if(j == 0) 
						display->bitmapBitArray[i] = 0;
					else
						display->bitmapBitArray[i+j] = pixArray[i];
				}
			}
			
			// yield access to pixel array generic data
			pixArray = NULL;
Error:
		return NULL;
}


static int DisplayImageFunction (ImageDisplayCVI_type* imgDisplay, Image_type** image)
{

//	unsigned char* bits = ConvertToBitArray(imgDisplay, *image);
//	SetBitmapData(imgDisplay->imgBitmapID, -1, 24, NULL, bits, NULL);
	
	CanvasDrawBitmap (imgDisplay->canvasPanHndl, CanvasPan_Canvas, imgDisplay->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
	
	return 0;
}

