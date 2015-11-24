#include <userint.h>

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
	
	//---------------------------------------------------------------------------------------------------------------
	// BASE CLASS
	//---------------------------------------------------------------------------------------------------------------
	
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
	unsigned char			ZoomLevel;			// current Zoom level (can be 0,1,2,3,4)
	Image_type* 			crtImage; 			// TODO replace with baseclass img
	
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

static void 					DiscardImageList 			(ListType* imageListPtr);

static int						DisplayImageFunction		(ImageDisplayCVI_type* imgDisplay, Image_type** image);

static void 					ConvertToBitArray			(ImageDisplayCVI_type* display, Image_type* image, int* alternativeInput, unsigned char type);

static void 					BilinearResize				(int* input, int** outputPtr, int sourceWidth, int sourceHeight, int targetWidth, int targetHeight);

static int 						Zoom						(ImageDisplayCVI_type* display, char direction);

static int CVICALLBACK 			CanvasPanCallback 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCallback			(int panel, int event, void *callbackData, int eventData1, int eventData2);

static int* 					NormalizePixelArray			(ImageDisplayCVI_type* display);

static void 					Deserialize					(ImageDisplayCVI_type* display, int* array, int length);


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
ImageDisplayCVI_type* init_ImageDisplayCVI_type (int    				parentPanHandl, 
												 char   				displayTitle[],
												 int					imgWidth,
												 int					imgHeight,
												 CallbackGroup_type**	callbackGroupPtr)		 
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

	
	init_ImageDisplay_type(&imgDisplay->baseClass, &imgDisplay, NULL, (DiscardFptr_type) discard_ImageDisplayCVI_type, (DisplayImageFptr_type) DisplayImageFunction, NULL, callbackGroupPtr);

	
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
	imgDisplay->crtImage			= NULL;
	imgDisplay->ZoomLevel			= 0;

	
	// ALLOC Resources
	//---------------------------------
		
	// loading display panel into workspace panel
	errChk( imgDisplay->displayPanHndl = LoadPanel (parentPanHandl, ImageDisplay_UI, DisplayPan) );

	// loading canvas panel into display panel
	errChk( imgDisplay->canvasPanHndl = LoadPanel (imgDisplay->displayPanHndl, ImageDisplay_UI, CanvasPan) ); 

	// set panel callback
	SetPanelAttribute(imgDisplay->displayPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, DisplayPanCallback);
	SetPanelAttribute(imgDisplay->displayPanHndl, ATTR_CALLBACK_DATA, imgDisplay);
	
	// allow access to ImgDisplay
	SetCtrlsInPanCBInfo(imgDisplay, CanvasPanCallback, imgDisplay->canvasPanHndl); 
	
	// create image list
	nullChk( imgDisplay->images = ListCreate(sizeof(Image_type*)) );

	// create bitmap bit array
	if(imgHeight > 0 && imgWidth > 0)
		nullChk( imgDisplay->bitmapBitArray = malloc(BytesPerPixelDepth * imgWidth * imgHeight) );
	else
		goto Error;
				 
	// create bitmap image handle
	errChk( NewBitmap (-1, pixelDepth, imgWidth, imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID) );
	
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
	
	if(imgDisp->imgBitmapID >= 0)
		DiscardBitmap(imgDisp->imgBitmapID);
	
	OKfree(imgDisp->bitmapBitArray);
	
	//--------------
	// UI
	//--------------
	
	OKfreePanHndl(imgDisp->canvasPanHndl); 	
	OKfreePanHndl(imgDisp->displayPanHndl);

	
	OKfree(* imageDisplayPtr);
	
	// maybe needed in the future to discard parent resources as well
	// discard_ImageDisplay_type (imageDisplayPtr)
	
}


void discard_ImageCVI_type (ImageDisplayCVI_type** ImageCVIPtr)
{
	ImageDisplayCVI_type* ImageCVI = *ImageCVIPtr;
	if (!ImageCVI) return;				   
	

	
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


static void ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image, int* alternativeInput, unsigned char type) 
{
INIT_ERR
	
	int 				iterrations 		= 0;
	int					imgHeight			= 0;
	int					imgWidth			= 0;
	unsigned char*  	pixArray			= NULL;
	ImageTypes 			imgType				= GetImageType(image);
	
	
	GetImageSize(image, &imgWidth, &imgHeight);
	
	int 				nPixels				= imgHeight * imgWidth;
	
	
	
	if(type == 0) {
		
		nullChk(pixArray = (unsigned char*) malloc(nPixels * sizeof(unsigned char)));
		iterrations = nPixels * BytesPerPixelDepth;
		
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
		switch (imgType) { 
		
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
	} else {
		
			nullChk(pixArray = (unsigned char*) malloc(display->nBytes * sizeof(unsigned char)));
			
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
}


Error:
	
	// free linear transformation array memory               
	OKfree(pixArray);
}


static int DisplayImageFunction (ImageDisplayCVI_type* imgDisplay, Image_type** imagePtr)
{
INIT_ERR 
	
	Image_type* 	image     		= *imagePtr;
	int 			imgHeight 		= 0;
	int 			imgWidth 		= 0;
	imgDisplay->baseClass.image		= image;
	*imagePtr						= NULL;
	
	GetImageSize(image, &imgWidth, &imgHeight);  
	
	if(imgWidth != imgDisplay->imgWidth || imgHeight != imgDisplay->imgHeight) {
	
		//clear old display internal bitmap image data
		OKfree(imgDisplay->bitmapBitArray);
		DiscardBitmap(imgDisplay->imgBitmapID);
		

		//allocate and update display internal data with new image content 
		nullChk( imgDisplay->bitmapBitArray = (unsigned char*) malloc(imgHeight * imgWidth * BytesPerPixelDepth) );
		imgDisplay->nBytes	  = imgHeight * imgWidth * BytesPerPixelDepth; 
		imgDisplay->imgHeight = imgHeight;
		imgDisplay->imgWidth  = imgWidth;
	
	
		//convert new image data to bit array and create bitmap image
		ConvertToBitArray(imgDisplay, image, NULL, 0);
		errChk( NewBitmap (-1, pixelDepth, imgDisplay->imgWidth, imgDisplay->imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID) );
	
	} else {
		
		//only update bitmap data 
		ConvertToBitArray(imgDisplay, image, NULL, 0);
		SetBitmapData(imgDisplay->imgBitmapID, -1, pixelDepth, NULL, imgDisplay->bitmapBitArray, NULL);
	
	}
	
	//draw bitmap on canvas
	CanvasDrawBitmap (imgDisplay->canvasPanHndl, CanvasPan_Canvas, imgDisplay->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
	DisplayPanel(imgDisplay->displayPanHndl); 
	
	return 0;
	
Error: 
	
	return errorInfo.error;
	
}

static int CVICALLBACK CanvasPanCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ImageDisplayCVI_type* display = (ImageDisplayCVI_type*) callbackData;   

	switch (event) {
			
		case EVENT_CLOSE:
			
			HidePanel(panel);
			
		break;
		
		case EVENT_RIGHT_CLICK:
			
			
			
			Zoom(display, 1);
			/*
			
			GetScaledCtrlDisplayBitmap (display->canvasPanHndl, CanvasPan_Canvas, 0, 1.5 * display->imgHeight, 1.5 * display->imgWidth, &display->imgBitmapID);
			CanvasDrawBitmap (display->canvasPanHndl, CanvasPan_Canvas, display->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);  
			 */
		break;
		
		case EVENT_LEFT_CLICK:
			
			Zoom(display, -1);
		break;
	}
	
	return 0;
}

static int CVICALLBACK DisplayPanCallback (int panel, int event, void *callbackData, int eventData1, int eventData2) 
{
	switch (event) { 
			
		case EVENT_CLOSE:
			
			HidePanel(panel);
			
		break;
			
	}
	
	return 0;
	
}
static int Zoom (ImageDisplayCVI_type* display, char direction)
{
INIT_ERR 
	
	int 	newWidth 			= 0;
	int 	newHeight 			= 0;
	int 	imageWidth			= 0;
	int 	imageHeight			= 0;
	int*	interpolationArray 	= NULL;
	int*	resizedArray		= NULL;

	
	if (display->ZoomLevel > 0 && direction < 0)
		display->ZoomLevel--;
	
	if (display->ZoomLevel < maxZoomLevel && direction > 0)
		display->ZoomLevel++;

		GetImageSize(display->baseClass.image, &imageWidth, &imageHeight); 
	//  GetImageSize(display->crtImage, &imageWidth, &imageHeight);
	newWidth = imageWidth * (1 + display->ZoomLevel * zoomFactor);
	newHeight = imageHeight * (1 + display->ZoomLevel * zoomFactor);
	
	DiscardBitmap(display->imgBitmapID);
	
	nullChk ( interpolationArray = (int*) malloc (imageWidth * imageHeight * sizeof(int)));
	nullChk ( resizedArray 		=  (int*) malloc (newWidth * newHeight * sizeof(int)));

	
	ConvertToBitArray(display, display->baseClass.image, NULL, 0);
	interpolationArray = NormalizePixelArray(display);
	
	OKfree(display->bitmapBitArray);
	//update display info
	display->imgWidth = newWidth;
	display->imgHeight = newHeight;
	
	//update image array size and allocate memory
	display->nBytes = newWidth * newHeight * BytesPerPixelDepth;
	nullChk( display->bitmapBitArray = (unsigned char*) malloc(display->nBytes * sizeof(unsigned char)) );
	
	BilinearResize(interpolationArray, &resizedArray, imageWidth, imageHeight, newWidth, newHeight);
	
	//ConvertToBitArray(display, display->baseClass.image, resizedArray, 1);
	Deserialize(display, resizedArray, newWidth * newHeight);								   
	
	SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , newWidth);
	SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, newHeight);
	
	errChk( NewBitmap (-1, pixelDepth, display->imgWidth, display->imgHeight, NULL, display->bitmapBitArray, NULL, &display->imgBitmapID) );   

Error:
	
	return errorInfo.error; 
	
	
}

static int* NormalizePixelArray(ImageDisplayCVI_type* display) {
INIT_ERR
	int 	imgHeight   = 0;
	int 	imgWidth	= 0;
	int* 	result  	= NULL;
	int 	nPixels		= 0;
	
	
	GetImageSize(display->baseClass.image, &imgHeight, &imgWidth);
	nPixels = imgHeight * imgWidth;
	nullChk(result = (int*) malloc (nPixels * sizeof(int)));
	
	for(int i = 0; i < nPixels; i++)
		result[i] = ( display->bitmapBitArray[4 * i] << 24) | ( display->bitmapBitArray[4 * i + 1] << 16) |
				 	( display->bitmapBitArray[4 * i + 2] << 8) | display->bitmapBitArray[4 * i + 3];
	return result;
Error:
	OKfree(result);
	return NULL;
}

static void Deserialize(ImageDisplayCVI_type* display, int* array, int length) {

	for(int i = 0; i < length; i++) {
		display->bitmapBitArray[4 * i] = ((array[i] >> 24)  &0x000000ff);
		display->bitmapBitArray[4 * i + 1] = ((array[i] >> 16) & 0x000000ff);
		display->bitmapBitArray[4 * i + 2] = ((array[i] >> 8) &0x000000ff);
		display->bitmapBitArray[4 * i + 3] = ((array[i])  &0x000000ff);
	}
};

static void BilinearResize(int* input, int** outputPtr, int sourceWidth, int sourceHeight, int targetWidth, int targetHeight)
{    
    int a, b, c, d, x, y, index;
	float x_diff, y_diff, blue, red, green;
	
    float x_ratio = ((float)(sourceWidth - 1)) / targetWidth;
    float y_ratio = ((float)(sourceHeight - 1)) / targetHeight;
    int*  output 	= *outputPtr;
    int offset = 0 ;
 
    for (int i = 0; i < targetHeight; i++)
    {
        for (int j = 0; j < targetWidth; j++)
        {
            x = (int)(x_ratio * j) ;
            y = (int)(y_ratio * i) ;
            x_diff = (x_ratio * j) - x ;
            y_diff = (y_ratio * i) - y ;
            index = (y * sourceWidth + x) ;                
            a = input[index] ;
            b = input[index + 1] ;
            c = input[index + sourceWidth] ;
            d = input[index + sourceWidth + 1] ;
 			
            // blue element
            blue = ((a>>24)&0xff)*(1-x_diff)*(1-y_diff) + ((b>>24)&0xff)*(x_diff)*(1-y_diff) +
                   ((c>>24)&0xff)*(y_diff)*(1-x_diff)   + ((d>>24)&0xff)*(x_diff*y_diff);
 		
            // green element
            green = ((a>>8)&0xff)*(1-x_diff)*(1-y_diff) + ((b>>8)&0xff)*(x_diff)*(1-y_diff) +
                    ((c>>8)&0xff)*(y_diff)*(1-x_diff)   + ((d>>8)&0xff)*(x_diff*y_diff);
 			
            // red element
            red = ((a>>16)&0xff)*(1-x_diff)*(1-y_diff) + ((b>>16)&0xff)*(x_diff)*(1-y_diff) +
                  ((c>>16)&0xff)*(y_diff)*(1-x_diff)   + ((d>>16)&0xff)*(x_diff*y_diff);
		
 		     /* 
            output [offset++] =
                    0x000000ff | // alpha
                    ((((int))   << 24)&0xff0000) |
                    ((((int)green) << 16)&0xff00) |
                    ((((int))  << 8)&0xff00);
			*/ 
			output [offset++] =
                    (int)blue &0x000000ff| 
                    ((((int)blue)   << 24)&0x000000ff) |
                    ((((int)green) << 16)&0x000000ff) |
                    ((((int)red)  << 8)&0x000000ff);
				
			
        }
    }
}
