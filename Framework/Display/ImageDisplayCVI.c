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

struct ImageDisplayCVI {
ImageDisplay_type baseClass; 
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	ListType				images;				// List of Image_type* elements. Data in this image is used to update the display whenever needed. Note: do not modify this data directly!
	BOOL					update;				// If True, the plot will display each time the latest waveform added to it. If False, the user can use the scrollbar to return to a previous plot. Default: True.
	int						imgBitmapID;		// this is the handle to the image provided to the display canvas, it is updated from the "images" array, when needed.
	unsigned char* 			bitmapBitArray;		// Array of bits used to create the bitmap image
	int						nBytes;				// number of bytes in the bitmapBitArray;
	int						imgHeight;			// image height
	int 					imgWidth;			// image width
	
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

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void 			DiscardImageList 			(ListType* imageListPtr);

static int				DisplayImageFunction		(ImageDisplayCVI_type* imgDisplay, Image_type** image);

static void			 	ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image);

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
	errChk(NewBitmap (-1, pixelDepth, imgWidth, imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID));
	
	

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


static void ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image) 
{
INIT_ERR
	
	int 				iterrations 		= display->nBytes;		
	int 				nPixels				= display->imgHeight * display->imgWidth;
	unsigned char*  	pixArray			= NULL;
	ImageTypes 			imgType				= GetImageType(image); 
	
	nullChk(pixArray = (unsigned char*) malloc(nPixels * sizeof(unsigned char)));
		
	switch (imgType) {
			
		case Image_UChar:					// 8bit
		
			unsigned char* uchar_iterr = (unsigned char*) GetImagePixelArray(image);
		
			for (int i = 0; i < nPixels; i++) 
				pixArray[i] = uchar_iterr[i];
		
			break;
		
		case Image_UShort:					// 16 bit unsigned
			
			unsigned short int* ushort_iterr = (unsigned short int*) GetImagePixelArray(image);
			unsigned short int	ushort_min	 = ushort_iterr[0];
			unsigned short int	ushort_max	 = ushort_iterr[0];
		
			for (int i = 0; i < nPixels; i++) {
				if (ushort_iterr[i] > ushort_max)
					ushort_max = ushort_iterr[i];
			
				if (ushort_iterr[i] < ushort_min)
					ushort_min = ushort_iterr[i];
			}
		
			unsigned short ushort_interval = ushort_max - ushort_min;
		
			for (int i = 0; i < nPixels; i++)
				pixArray[i] = (unsigned char) ((ushort_iterr[i] - ushort_min) * 255.0 / ushort_interval);
		
			break;								
		
		case Image_Short:					// 16 bit signed
		
			short int* short_iterr  = (short int*) GetImagePixelArray(image);
			short int  short_min 	= short_iterr[0];
			short int  short_max	= short_iterr[0];

			for (int i = 0; i < nPixels; i++) {
				if (ushort_iterr[i] > short_max)
					short_max = short_iterr[i];
			
				if (ushort_iterr[i] < short_min)
					short_min = short_iterr[i];
			}
		
			short int short_interval   = short_max - short_min;
		
			for (int i = 0; i < nPixels; i++)
				pixArray[i] = (unsigned char)((short_iterr[i] - short_min) * 255.0 / short_interval);
			
			break;
		
		case Image_UInt:					// 32 bit unsigned
			
			unsigned  int* 	uint_iterr 		= (unsigned int*) GetImagePixelArray(image);
			unsigned  int	uint_min	 	= uint_iterr[0];
			unsigned  int	uint_max	 	= uint_iterr[0];
		
			for (int i = 0; i < nPixels; i++) {
				if (uint_iterr[i] > uint_max)
					uint_max = uint_iterr[i];
			
				if (uint_iterr[i] < uint_min)
					uint_min = uint_iterr[i];
			}
		
			unsigned int uint_interval = uint_max - uint_min; 
		
			for (int i = 0; i < nPixels; i++)
				pixArray[i] = (unsigned char)((uint_iterr[i]-uint_min) * 255.0 / uint_interval);
			
			break;										
		
		case Image_Int:						// 32 bit signed

			int* int_iterr 	 = (int*) GetImagePixelArray(image);
			int	 int_min	 = int_iterr[0];
			int	 int_max	 = int_iterr[0];
		
			for (int i = 0; i < nPixels; i++) {
				if (int_iterr[i] > int_max)
					int_max = int_iterr[i];
			
				if (int_iterr[i] < int_min)
					int_min = int_iterr[i];
			}
		
			int int_interval = int_max - int_min;
		
			for (int i = 0; i < nPixels; i++)
				pixArray[i] = (unsigned char)((int_iterr[i] - int_min) * 255.0 / int_interval);
			
			break;

		case Image_Float:					// 32 bit float
		
			float* float_iterr 	 = (float*) GetImagePixelArray(image);
			float  float_min	 = float_iterr[0];
			float  float_max	 = float_iterr[0];
		
			for (int i = 0; i < nPixels; i++) {
				if (float_iterr[i] > float_max)
					float_max = float_iterr[i];
			
				if (float_iterr[i] < float_min)
					float_min = float_iterr[i];
			}
													
			float float_interval = float_max - float_min;
		
			for (int i = 0; i < nPixels; i++)
				pixArray[i] = (unsigned char) ((float_iterr[i] - float_min) * 255.0 / float_interval);
		
			break;
			
		default:
			
			break;
		
	}
	
	//Image mapping B->G->R->ignored byte
	switch(imgType) { 
		
		case Image_RGBA:
	
			RGBA_type* iterr = (RGBA_type*) GetImagePixelArray(image);

			for (int i = 0; i < nPixels; i++, iterr++){
				display->bitmapBitArray[i] 	 = iterr->B;
				display->bitmapBitArray[i+1] = iterr->G;
				display->bitmapBitArray[i+2] = iterr->R;
				display->bitmapBitArray[i+3] = 0;
			}
	
			break;

		case Image_RGBAU64:
	
			RGBAU64_type* RGBAU64_ptr   = (RGBAU64_type*) GetImagePixelArray(image);
			RGBAU64_type* iterr_u64		= NULL;
			unsigned short maxR			= iterr->R;
			unsigned short maxG			= iterr->G;
			unsigned short maxB			= iterr->B;
			unsigned short minR			= iterr->R;
			unsigned short minG			= iterr->G;
			unsigned short minB		    = iterr->B;
			unsigned short intervalR	= 0;
			unsigned short intervalG	= 0;
			unsigned short intervalB	= 0;

			for (int i = 0; i < nPixels; i++, iterr_u64++) {
	
				if(maxR < iterr->R)
					maxR = iterr->R;
				if(minR > iterr->R)
					minR = iterr->R;
	
				if(maxG < iterr->G)
					maxG = iterr->G;
				if(minG > iterr->G)
					minG = iterr->G;
	
				if(maxB < iterr->B)
					maxB = iterr->B;
				if(minB > iterr->B)
					minB = iterr->B;
			}

			//reset pointer position
			iterr_u64 = RGBAU64_ptr;

			intervalB = maxB - minB;
			intervalG = maxG - minG;
			intervalR = maxR - minR;

			for (int i = 0; i < nPixels; i++, iterr_u64++){
				display->bitmapBitArray[i]   = (unsigned char) ((iterr_u64->B - minB) * 255 /intervalB);
				display->bitmapBitArray[i+1] = (unsigned char) ((iterr_u64->G - minG) * 255 /intervalG);
				display->bitmapBitArray[i+2] = (unsigned char) ((iterr_u64->R - minR) * 255 /intervalR);
				display->bitmapBitArray[i+3] = 0;
			}

			break;
		
		default:
		
			//for each 32 bit value
			for (int i = 0; i < iterrations; i = i + BytesPerPixelDepth) {

				//for each of the 4 bytes of the 32 bit value
				for (int j = 0; j < BytesPerPixelDepth; j++) {			  
					if (j == 3) 
						display->bitmapBitArray[i+j] = 0;
					else
						display->bitmapBitArray[i+j] = pixArray[i / BytesPerPixelDepth];
	
				}
			}
			break;
	
	
	}	 


Error:
	
	// free linear transformation array memory               
	OKfree(pixArray);
}


static int DisplayImageFunction (ImageDisplayCVI_type* imgDisplay, Image_type** image)
{

	ConvertToBitArray(imgDisplay, *image);
	
	SetBitmapData(imgDisplay->imgBitmapID, -1, pixelDepth, NULL, imgDisplay->bitmapBitArray, NULL);
	
	CanvasDrawBitmap (imgDisplay->canvasPanHndl, CanvasPan_Canvas, imgDisplay->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
	
	return 0;
}

