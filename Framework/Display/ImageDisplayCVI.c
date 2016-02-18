#include <userint.h>

//==============================================================================
//
// Title:		ImageDisplayCVI.c
// Purpose:		A short description of the implementation.
//
// Created on:	10/13/2015 at 2:09:46 PM by popescu.andrei1991@gmail.com.
// Modified by: Adrian Negrean.
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

// Pixel depth represents how many bits from the bitmapBitArray each pixel has. If PixelDepth is 24, each pixel is represented by a 24-bit RGB value of the form RRGGBB,
// where RR, GG, and BB represent the red, green, and blue components of the color value. The RR byte is at the lowest memory address of the three bytes.
// When PixelDepth is 24 or 32, valid RR, GG, and BB values range from 0 to 255.
// 4 bytes per pixel
#define PixelDepth 			32
#define ZoomFactor 			0.25
#define maxZoomLevel 		4
		
#define getByte(value, byteNum) ((value >> (8*byteNum)) & 0xff)
		
#define BUTNUM 				6
#define CANVAS_MIN_HEIGHT	200
#define CANVAS_MIN_WIDTH	200
#define CANVAX_MAX_HEIGHT	1000
#define CANVAS_MAX_WIDTH	1000
#define ROI_LABEL_WIDTH		10
#define	ROI_LABEL_HEIGHT	15

#define ROILabel_XOffset	3		// ROI text label offset in the X direction in pixels.
#define ROILabel_YOffset	3		// ROI text label offset in the Y direction in pixels.
		
#define Default_ROI_R_Color					0	   	// red
#define Default_ROI_G_Color					255	   	// green
#define Default_ROI_B_Color					0		// blue
#define Default_ROI_A_Color					0		// alpha
		
#define CROSS_LENGTH						6


// These macros add two numbers by avoiding positive and negative overflow.
// Currently not supported by CVI, can be used if updated in future
// http://digital.ni.com/public.nsf/allkb/73AEAD30C8AF681A86257BBB0054A26B

// Min is 1 << (sizeof(type)*8-1)
#define __MIN(type) ((type)-1 < 1?__MIN_SIGNED(type):(type)0)

// The max value is NOT(operator) the min value
#define __MAX(type) ((type)~__MIN(type))

#define add_of(c,a,b) ({ \
  typeof(a) __a=a; \
  typeof(b) __b=b; \
  (__b)<1 ? \
    ((__MIN(typeof(c))-(__b)<=(__a))?assign(c,__a+__b):1) : \
    ((__MAX(typeof(c))-(__b)>=(__a))?assign(c,__a+__b):1); \
})

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
	
	ListType				images;					// List of Image_type* elements. Data in this image is used to update the display whenever needed. Note: do not modify this data directly!
	BOOL					update;					// If True (default), the display will be updated each time a new image is added to it. If False, the user can use the scrollbar to return to a previous image.
	int						imgBitmapID;			// Bitmap placed on the canvas that is generated using the provided image data.
	unsigned char* 			bitmapBitArray;			// Array of bits used to create the bitmap image.
	int						nBytes;					// Number of bytes in the bitmapBitArray.
	int						imgHeight;				// Image height.
	int 					imgWidth;				// Image width.
	double					zoomLevel;				// Current zoom level.
	int*					interpolationArray; 	// Array used to perform normalization (squashing 4 bytes into an int) for the Bilinear interpolation.
	int*					resizedArray;			// Array used to deserialize bytes from the integer into bitmapBitArray.
	float*					RGBOverlayBuffer[3];	// Image buffer used to sum up and overlay images from multiple RGB channels.
	
	//---------------------------------------------------------------------------------------------------------------
	// UI
	//---------------------------------------------------------------------------------------------------------------
	
	int						displayPanHndl;
	int						canvasPanHndl;
	int						menuBarHndl;
	int						FilemenuID;
	int						ImagemenuID;
	
	//---------------------------------------------------------------------------------------------------------------
	// CALLBACK
	//---------------------------------------------------------------------------------------------------------------
	
	//WaveformDisplayCB_type	callbackFptr;
	void*					callbackData;
};

//==============================================================================
// Static global variables

static int buttonArray[] = {DisplayPan_Zoom, DisplayPan_RectROI, DisplayPan_PointROI, DisplayPan_Select, DisplayPan_PICTUREBUTTON_5, DisplayPan_PICTUREBUTTON_6};


//==============================================================================
// Static functions

static int						DisplayImage				(ImageDisplayCVI_type* imgDisplay, Image_type** image);

static void 					ConvertToBitArray			(ImageDisplayCVI_type* display, Image_type* image);

static void 					BilinearResize				(int* input, int** outputPtr, int sourceWidth, int sourceHeight, int targetWidth, int targetHeight);

static int 						Zoom						(ImageDisplayCVI_type* display, double zoomLevel);

static int CVICALLBACK 			CanvasCallback 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCtrlCallback 		(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCallback			(int panel, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			CanvasPanelCallback 		(int panel, int event, void *callbackData, int eventData1, int eventData2);

void 							NormalizePixelArray			(ImageDisplayCVI_type* display, int **arrayPtr);

static void 					Deserialize					(ImageDisplayCVI_type* display, int* array, int length);

static void 					DrawROIs					(ImageDisplayCVI_type* display);

void CVICALLBACK 				MenuSaveCB 					(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

void CVICALLBACK 				MenuRestoreCB 				(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

void 							GetResultedImageSize		(ColorChannel_type* colorChan, int* heightPtr, int* widthPtr);

void 							AddImagePixels				(float** ResultImagePixArrayPtr, int ResultImageHeight, int ResultImageWidth, ColorChannel_type*  colorChan);

int 							DisplayRGBImageChannels 	(ImageDisplayCVI_type* imgDisplay, ColorChannel_type** RChanPtr, ColorChannel_type** GChanPtr, ColorChannel_type** BChanPtr);

void 							NormalizeFloatArrayToRGBA	(RGBA_type* outputPtr[], float* input, int nPixels, int RGBAChannel);




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

	
	init_ImageDisplay_type(&imgDisplay->baseClass, &imgDisplay, NULL, (DiscardFptr_type) discard_ImageDisplayCVI_type, (DisplayImageFptr_type) DisplayImage, NULL, callbackGroupPtr);

	
	//-------------------------------------
	// Init child class
	//-------------------------------------
	
	
	// INIT
	//---------------------------------
	
	imgDisplay->displayPanHndl		= 0;
	imgDisplay->canvasPanHndl		= 0;
	imgDisplay->images				= 0;
	imgDisplay->imgBitmapID			= 0;
	imgDisplay->nBytes				= imgHeight * imgWidth * 4;
	imgDisplay->imgHeight			= imgHeight;
	imgDisplay->imgWidth			= imgWidth;
	imgDisplay->bitmapBitArray		= NULL;
	imgDisplay->zoomLevel			= 1;
	
	// ALLOC Resources
	//---------------------------------
		
	// loading display panel into workspace panel
	errChk( imgDisplay->displayPanHndl = LoadPanel (parentPanHandl, ImageDisplay_UI, DisplayPan) );

	// loading canvas panel into display panel
	errChk( imgDisplay->canvasPanHndl = LoadPanel (imgDisplay->displayPanHndl, ImageDisplay_UI, CanvasPan) ); 

	// set panel callback
	SetPanelAttribute(imgDisplay->displayPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, DisplayPanCallback);
	SetPanelAttribute(imgDisplay->displayPanHndl, ATTR_CALLBACK_DATA, imgDisplay);
	
	SetPanelAttribute(imgDisplay->canvasPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, CanvasPanelCallback);
	SetPanelAttribute(imgDisplay->canvasPanHndl, ATTR_CALLBACK_DATA, imgDisplay);
	
	// allow access to ImgDisplay
	SetCtrlsInPanCBInfo(imgDisplay, CanvasCallback, imgDisplay->canvasPanHndl);
	SetCtrlsInPanCBInfo(imgDisplay, DisplayPanCtrlCallback, imgDisplay->displayPanHndl);
	
	// set Z plane priortiy for canvas and selection tool
	SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_ZPLANE_POSITION, 0);
    SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_SELECTION, ATTR_ZPLANE_POSITION, 1);
	
	// set canvas pen color  & text background color
	
	SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_PEN_COLOR, VAL_GREEN); 
	SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_PEN_FILL_COLOR, VAL_TRANSPARENT);
	//add menu items
	imgDisplay->menuBarHndl = NewMenuBar(imgDisplay->displayPanHndl);
	imgDisplay->FilemenuID = NewMenu (imgDisplay->menuBarHndl, "File", -1);
	NewMenuItem (imgDisplay->menuBarHndl, imgDisplay->FilemenuID, "Save", -1, VAL_F2_VKEY, MenuSaveCB, imgDisplay);
	
	imgDisplay->ImagemenuID = NewMenu(imgDisplay->menuBarHndl, "Image", -1);
	NewMenuItem(imgDisplay->menuBarHndl, imgDisplay->ImagemenuID, "Restore", -1, VAL_F1_VKEY, MenuRestoreCB, imgDisplay);
	
	
	// create image list
	nullChk( imgDisplay->images = ListCreate(sizeof(Image_type*)) );

	// create bitmap bit array
	if(imgHeight > 0 && imgWidth > 0) {
		nullChk( imgDisplay->bitmapBitArray 		= malloc(4 * imgWidth * imgHeight) );
		nullChk( imgDisplay->resizedArray   		= malloc(imgWidth * imgHeight * sizeof(int)) );
		nullChk( imgDisplay->interpolationArray   	= malloc(imgWidth * imgHeight * sizeof(int)) ); 
		
		//allocate RGB overlay buffers
		for(int i = 0; i < 3; i++) {
			nullChk( imgDisplay->RGBOverlayBuffer[i] = malloc(imgWidth * imgHeight * sizeof(float)) );
		}
	} else
		goto Error;
				 
	// create bitmap image handle
	errChk( NewBitmap (-1, PixelDepth, imgWidth, imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID) );
	
	DisplayPanel (imgDisplay->canvasPanHndl);
	
	return imgDisplay;
	
Error:
	
	// cleanup
	discard_ImageDisplayCVI_type(&imgDisplay);
	return NULL;
}

static void discard_ImageDisplayCVI_type (ImageDisplayCVI_type** imageDisplayPtr)
{
	ImageDisplayCVI_type* imgDisp = *imageDisplayPtr;
	if (!imgDisp) return;
	
	//--------------
	// Images
	//--------------
	
	OKfreeList(&imgDisp->images, (DiscardFptr_type)discard_Image_type);
	
	if(imgDisp->imgBitmapID) {
		DiscardBitmap(imgDisp->imgBitmapID);
		imgDisp->imgBitmapID = 0;
	}
	
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
	size_t				nImages 	= ListNumItems(imageList);
	Image_type**		imagePtr 	= NULL;
	
	for (size_t i = 1; i <= nImages; i++) {
		imagePtr = (Image_type**)ListGetPtrToItem(imageList, i);
		discard_Image_type(imagePtr);
	}
	ListClear(imageList);
}

static void ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image) 
{
INIT_ERR
	
	int 				iterations 			= 0;
	int					imgHeight			= 0;
	int					imgWidth			= 0;
	unsigned char*  	pixArray			= NULL;
	ImageTypes 			imgType				= GetImageType(image);
	int 				nPixels				= 0;
	uInt64				bitChunkIdx			= 0;
	
	
	GetImageSize(image, &imgWidth, &imgHeight);
	
	nPixels	= imgHeight * imgWidth;
			
	nullChk(pixArray = (unsigned char*) malloc(nPixels * sizeof(unsigned char)));

	switch (imgType) {
		
		// 8 bit  
		case Image_UChar:					
	
			unsigned char* 			ucharPix 			= (unsigned char*) GetImagePixelArray(image);
	
			// pixel mapping B->G->R->ignored byte
			bitChunkIdx = 0;
			for (int i = 0; i < nPixels; i++) {
				bitChunkIdx = i*4;
				display->bitmapBitArray[bitChunkIdx]   	= ucharPix[i];
				display->bitmapBitArray[bitChunkIdx+1] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+2] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+3] 	= 0;
			}
			break;
	
		// 16 bit unsigned
		case Image_UShort:
		
			unsigned short int* 	ushortPix 			= (unsigned short int*) GetImagePixelArray(image);
			unsigned short int		ushort_min	 		= ushortPix[0];
			unsigned short int		ushort_max	 		= ushortPix[0];
	
			for (int i = 0; i < nPixels; i++) {
				if (ushortPix[i] > ushort_max)
					ushort_max = ushortPix[i];
		
				if (ushortPix[i] < ushort_min)
					ushort_min = ushortPix[i];
			}
	
			float					ushortScaleFactor 	= 255.0 / (ushort_max - ushort_min);
	
			// pixel mapping B->G->R->ignored byte
			bitChunkIdx = 0;
			for (int i = 0; i < nPixels; i++) {
				bitChunkIdx = i*4;
				display->bitmapBitArray[bitChunkIdx]   	= (unsigned char) ((ushortPix[i] - ushort_min) * ushortScaleFactor);
				display->bitmapBitArray[bitChunkIdx+1] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+2] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+3] 	= 0;
			}
			break;								
	
		// 16 bit signed 
		case Image_Short:
	
			short int* 				shortPix  			= (short int*) GetImagePixelArray(image);
			short int  				short_min 			= shortPix[0];
			short int  				short_max			= shortPix[0];

			for (int i = 0; i < nPixels; i++) {
				if (shortPix[i] > short_max)
					short_max = shortPix[i];
		
				if (shortPix[i] < short_min)
					short_min = shortPix[i];
			}
	
			float					shortScaleFactor 	= 255.0 / (short_max - short_min); 
	
			// pixel mapping B->G->R->ignored byte
			bitChunkIdx = 0;
			for (int i = 0; i < nPixels; i++) {
				bitChunkIdx = i*4;
				display->bitmapBitArray[bitChunkIdx]   	= (unsigned char)((shortPix[i] - short_min) * shortScaleFactor);
				display->bitmapBitArray[bitChunkIdx+1] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+2] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+3] 	= 0;
			}
			break;
	
		// 32 bit unsigned 
		case Image_UInt:					
		
			unsigned  int* 			uintPix 			= (unsigned int*) GetImagePixelArray(image);
			unsigned  int			uint_min	 		= uintPix[0];
			unsigned  int			uint_max	 		= uintPix[0];
	
			for (int i = 0; i < nPixels; i++) {
				if (uintPix[i] > uint_max)
					uint_max = uintPix[i];
		
				if (uintPix[i] < uint_min)
					uint_min = uintPix[i];
			}
	
			float					uintScaleFactor 	= 255.0 / (uint_max - uint_min);  
	
			// pixel mapping B->G->R->ignored byte
			bitChunkIdx = 0;
			for (int i = 0; i < nPixels; i++) {
				bitChunkIdx = i*4;
				display->bitmapBitArray[bitChunkIdx]   	= (unsigned char)((uintPix[i] - uint_min) * uintScaleFactor);
				display->bitmapBitArray[bitChunkIdx+1] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+2] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+3] 	= 0;
			}
			break;										
	
		// 32 bit signed 
		case Image_Int:

			int* 					intPix 	 			= (int*) GetImagePixelArray(image);
			int	 					int_min	 			= intPix[0];
			int	 					int_max			 	= intPix[0];
	
			for (int i = 0; i < nPixels; i++) {
				if (intPix[i] > int_max)
					int_max = intPix[i];
		
				if (intPix[i] < int_min)
					int_min = intPix[i];
			}
	
			float					intScaleFactor 		= 255.0 / (int_max - int_min);
	
			// pixel mapping B->G->R->ignored byte
			bitChunkIdx = 0;
			for (int i = 0; i < nPixels; i++) {
				bitChunkIdx = i*4;
				display->bitmapBitArray[bitChunkIdx]   	= (unsigned char)((intPix[i] - int_min) * intScaleFactor);
				display->bitmapBitArray[bitChunkIdx+1] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+2] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+3] 	= 0;
			}
			break;

		// 32 bit float
		case Image_Float:					
	
			float* 					floatPix 	 		= (float*) GetImagePixelArray(image);
			float  					float_min	 		= floatPix[0];
			float  					float_max			= floatPix[0];
	
			for (int i = 0; i < nPixels; i++) {
				if (floatPix[i] > float_max)
					float_max = floatPix[i];
		
				if (floatPix[i] < float_min)
					float_min = floatPix[i];
			}
												
			float					floatScaleFactor 	= 255.0 / (float_max - float_min);
	
			// pixel mapping B->G->R->ignored byte
			bitChunkIdx = 0;
			for (int i = 0; i < nPixels; i++) {
				bitChunkIdx = i*4;
				display->bitmapBitArray[bitChunkIdx]   	= (unsigned char) ((floatPix[i] - float_min) * floatScaleFactor);
				display->bitmapBitArray[bitChunkIdx+1] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+2] 	= display->bitmapBitArray[bitChunkIdx];
				display->bitmapBitArray[bitChunkIdx+3] 	= 0;
			}
			break;
			
		case Image_RGBA:

			RGBA_type* 				rgbaPix = (RGBA_type*) GetImagePixelArray(image);

			for (int i = 0; i < nPixels; i++){
				display->bitmapBitArray[i] 	 = (rgbaPix+i)->B;
				display->bitmapBitArray[i+1] = (rgbaPix+i)->G;
				display->bitmapBitArray[i+2] = (rgbaPix+i)->R;
				display->bitmapBitArray[i+3] = 0;
			}
			break;
		
		/* not functional, must be modified accordingly
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
		*/
		
		default:
		
			break;
	
	}
	
Error:
	
	return;
	
}


static int DisplayImage (ImageDisplayCVI_type* imgDisplay, Image_type** imagePtr)
{
INIT_ERR 
	
	Image_type* 	image     		= *imagePtr;
	int 			imgHeight 		= 0;
	int 			imgWidth 		= 0;
	*imagePtr						= NULL;
	
	imgDisplay->baseClass.image		= image;
	*imagePtr = NULL;
	
	GetImageSize(image, &imgWidth, &imgHeight);  
	
	if (imgWidth != imgDisplay->imgWidth || imgHeight != imgDisplay->imgHeight) {
	
		// clear old display internal bitmap image data
		OKfree(imgDisplay->bitmapBitArray);
		if(imgDisplay->imgBitmapID) { 
			DiscardBitmap(imgDisplay->imgBitmapID);
			imgDisplay->imgBitmapID = 0;
		}
		
		// allocate and update display internal data with new image content 
		nullChk( imgDisplay->bitmapBitArray 		= (unsigned char*) malloc(imgHeight * imgWidth * 4) );
		nullChk( imgDisplay->interpolationArray 	= (int*) realloc(imgDisplay->interpolationArray, imgHeight * imgWidth * sizeof(int)) );
		
		imgDisplay->nBytes	  = imgHeight * imgWidth * 4; 
		imgDisplay->imgHeight = imgHeight;
		imgDisplay->imgWidth  = imgWidth;
	
		// convert new image data to bit array and create bitmap image
		ConvertToBitArray(imgDisplay, image);
		errChk( NewBitmap (-1, PixelDepth, imgDisplay->imgWidth, imgDisplay->imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID) );
	
	} else {
		
		// only update bitmap data 
		ConvertToBitArray(imgDisplay, image);
		SetBitmapData(imgDisplay->imgBitmapID, -1, PixelDepth, NULL, imgDisplay->bitmapBitArray, NULL);
	
	}

	// adjust canvas size to match image size
	SetCtrlAttribute(imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , imgWidth);
	SetCtrlAttribute(imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, imgHeight);
	
	// draw bitmap on canvas
	CanvasDrawBitmap (imgDisplay->canvasPanHndl, CanvasPan_Canvas, imgDisplay->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
	
	int panVisible	= FALSE;
	GetPanelAttribute(imgDisplay->displayPanHndl, ATTR_VISIBLE, &panVisible);
	
	if (!panVisible)
		DisplayPanel(imgDisplay->displayPanHndl); 
	
	return 0;
	
Error: 
	
	return errorInfo.error;
	
}

static int CVICALLBACK CanvasCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ImageDisplayCVI_type* display 	= (ImageDisplayCVI_type*) callbackData;   
	int 				  state	  	= 0;

	switch (event) {
			
		case EVENT_CLOSE:
			
			HidePanel(panel);
			
		break;
		
		case EVENT_RIGHT_CLICK:
			
			//if zoom button is toggled 
			GetCtrlVal (display->displayPanHndl, DisplayPan_Zoom, &state); 
			if(state)	
				Zoom(display, display->zoomLevel * (1-ZoomFactor));

		break;
		
		case EVENT_LEFT_CLICK:
			
			//if zoom button is toggled
			GetCtrlVal (display->displayPanHndl, DisplayPan_Zoom, &state); 
			if(state)	
				Zoom(display, display->zoomLevel * (1+ZoomFactor));
			
		break;
	}
	
	return 0;
}

static int CVICALLBACK DisplayPanCtrlCallback (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ImageDisplayCVI_type* display = (ImageDisplayCVI_type*) callbackData;
	switch(event) {
		case EVENT_COMMIT:
			 int state = 0;
			 switch(control) {
			 	case DisplayPan_Zoom:
				
					GetCtrlVal (display->displayPanHndl, control, &state);
				 
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					break;
				
				case DisplayPan_RectROI:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					break;
				
				case DisplayPan_PointROI:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
				 
					break;
					
				case DisplayPan_Select:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
				 
					break;
					
				case DisplayPan_PICTUREBUTTON_5:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					
					break;
					
				default:
					
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				
					break;
					 
			 };
		break;
	};
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

int CVICALLBACK CanvasPanelCallback (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	ImageDisplayCVI_type* 	display 		= (ImageDisplayCVI_type*) callbackData;
    static int 				mouseDown 		= 0;
    static Point 			oldp;
    Point  					newp;
    static Rect 			oldr;
    static Rect				newr;
    int    					state;
	GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state); 
	
    switch (event) {
			
			
		// COLOR CHANNEL TESTING, TO BE REMOVED LATER ON
		//-----------------------------------------------------------------------------
		/*
		case EVENT_KEYPRESS:
			GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON_5, &state);

			if(state && eventData1 == VAL_TAB_VKEY) {
				
				short int* 		pixArrayRedImg1 = 	malloc(10000 * sizeof(short int));		// 100 x 100
				int*   			pixArrayRedImg2 = 	malloc (5000 * sizeof(int)); 			// 50  x 100
				
				float* 			pixArrayGreenImg1 = malloc(20000 * sizeof(float));			// 200 x 100
				
				unsigned char* 	pixArrayBlueImg1 = 	malloc(25000 * sizeof(unsigned char));	// 100  x 250
				
				ColorChannel_type*	red, *green, *blue;
				Image_type** redImages;
				Image_type** blueImages;
				Image_type** greenImages;
				uInt32 *redImageOffsetX;
				uInt32 *redImageOffsetY;
				uInt32 *noOffset;
				
				redImages = malloc(2 * sizeof(Image_type*));
				blueImages = malloc(sizeof(Image_type*));
				greenImages = malloc(sizeof(Image_type*));

				redImageOffsetX = malloc(2 * sizeof(uInt32));
				redImageOffsetY = malloc(2 * sizeof(uInt32));
				noOffset = malloc(sizeof(uInt32));
				//init pixel data
				for (int i = 0; i < 10000; i++)
					pixArrayRedImg1[i] = 125 + rand() % 125;
				for(int i = 0; i < 5000; i++)
					pixArrayRedImg2[i] = rand() % 200;
				
				for (int i = 0; i < 20000; i++)
					pixArrayGreenImg1[i] = rand () % 500000;
				for(int i = 0; i < 25000; i++)
					pixArrayBlueImg1[i] =  + rand() % 50000 ;
				
				//init Images
				
				redImages[0] = init_Image_type(Image_Short, 100, 100, &pixArrayRedImg1);
				
				redImages[1] = init_Image_type(Image_Int, 50, 100, &pixArrayRedImg2);
				
				redImageOffsetX[0] = 10;  
				redImageOffsetY[0] = 50;
				
				redImageOffsetX[1] = 50;
				redImageOffsetY[1] = 10;
				
				
				greenImages[0] = init_Image_type(Image_Float, 200, 100, &pixArrayGreenImg1);
				
				noOffset[0] = 0;
											 
				blueImages[0] = init_Image_type(Image_UChar, 100, 250, &pixArrayBlueImg1);
				
				red = init_ColorChannel_type (2, &redImages, redImageOffsetX, redImageOffsetY);
				
				green = init_ColorChannel_type(1, &greenImages, noOffset, noOffset);
				
				blue = init_ColorChannel_type(1, &blueImages, noOffset, noOffset);
				

					double start = Timer();
					DisplayRGBImageChannels (display, &red, &green, &blue);
					double end = Timer();

					DebugPrintf("display RGB images took %f\n", end - start);
					
//					DisplayRGBImageChannels(display, NULL, &green, &blue);

//					DisplayRGBImageChannels(display, &red, NULL, &blue);

//					DisplayRGBImageChannels(display, &red, &green, NULL);
				 
				OKfree(redImages);
				OKfree(greenImages);
				OKfree(blueImages);
				OKfree(redImageOffsetX);
				OKfree(redImageOffsetY);
				OKfree(noOffset);
				OKfree(pixArrayRedImg1);
				OKfree(pixArrayRedImg2);
				OKfree(pixArrayGreenImg1);
				OKfree(pixArrayBlueImg1);
				OKfree(red);
				OKfree(green);
				OKfree(blue);
			}
			break;
		//-----------------------------------------------------------------------------  
		*/
        case EVENT_LEFT_CLICK:
			
			
			//if selection ROI button is toggled 
			GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state);  
			if(state) {
				
				//mark that mouse button is down
	            mouseDown = 1;
				
				int x,y;
				
				if(eventData2 > display->imgWidth)
					x = display->imgWidth;
				else
					x = eventData2;
				
				if(eventData1 > display->imgHeight)
					y = display->imgHeight;
				else
					y = eventData1;
				
				//Mark rectangle starting point
	            oldp = MakePoint (x, y);
				
				
				
				//Draw all previous ROI's
				DrawROIs(display);
				
				int canvasBitmap;
				
				//Update the ImgDisplay bitmap with the one with ROI's overlay, in order to avoid constantly redrawing the ROI's
				GetCtrlDisplayBitmap (display->canvasPanHndl, CanvasPan_Canvas, 0, &canvasBitmap);
				display->imgBitmapID = canvasBitmap;

			}
			//If ROI point button is toggled
			GetCtrlVal (display->displayPanHndl, DisplayPan_PointROI, &state);
			if(state) {
				
				int x,y;
				
				if(eventData2 > display->imgWidth)
					x = display->imgWidth;
				else
					x = eventData2;
				
				if(eventData1 > display->imgHeight)
					y = display->imgHeight;
				else
					y = eventData1;
				
				ListType	ROIlist		= 0;
				
				ROI_type*	newROI 		= NULL;
				
				
				//default ROI color
				RGBA_type	color 		= {.R = Default_ROI_R_Color, .G = Default_ROI_G_Color, .B = Default_ROI_B_Color, .alpha = Default_ROI_A_Color}; 
				
				ROIlist = GetImageROIs(display->baseClass.image);
				
				//create new point ROI
				newROI = (ROI_type*)initalloc_Point_type(NULL, "", color, TRUE, x / display->zoomLevel, y / display->zoomLevel);
				
				newROI->ROIName = GetDefaultUniqueROIName(ROIlist); 
				
				Rect	textrect_point = { 	.top 		= y + ROILabel_YOffset,
						 			 	   	.left 		= x + ROILabel_XOffset,						
						 			 		.width 	= ROI_LABEL_WIDTH,
						 			 		.height	= ROI_LABEL_HEIGHT	};
				
				
				//Draw cross to mark point ROI
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(x - CROSS_LENGTH, y), MakePoint(x + CROSS_LENGTH, y));
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(x, y - CROSS_LENGTH), MakePoint(x, y + CROSS_LENGTH));
							   
				CanvasDrawText (display->canvasPanHndl, CanvasPan_Canvas, newROI->ROIName,VAL_APP_META_FONT,textrect_point, VAL_LOWER_LEFT);
				
				AddImageROI(display->baseClass.image, &newROI, NULL);   
			}
            break;
        case EVENT_LEFT_CLICK_UP:
			 
			//If selection button is toggled
			 GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state); 
   	         if(state) {	
				
							mouseDown 	= 0;
            	
				ListType	ROIlist		= 0;
				
				ROI_type*	newROI 		= NULL;
				
				RGBA_type	color 		= {.R = Default_ROI_R_Color, .G = Default_ROI_G_Color, .B = Default_ROI_B_Color, .alpha = Default_ROI_A_Color}; 
			
                SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_VISIBLE, 0);
                SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_WIDTH, 0);
                SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_HEIGHT, 0);
				
				ROIlist = GetImageROIs(display->baseClass.image); 
				
				//transform coordinates to the base, no zoom level
				
				
				// save ROI based on zero magnification level
				int top  	= newr.top / display->zoomLevel;
				int left 	= newr.left / display->zoomLevel;
				int height 	= newr.height / display->zoomLevel;
				int width 	= newr.width / display->zoomLevel;
				
				newROI = (ROI_type*)initalloc_Rect_type(NULL, "", color, TRUE, top, left, height, width); 
				
				ROIlist = GetImageROIs(display->baseClass.image);
				
				newROI->ROIName = GetDefaultUniqueROIName(ROIlist);

				
				Rect	textrect = { .top 		= newr.top  + ROILabel_YOffset,
						 			 .left 		= newr.left + ROILabel_XOffset,
						 			 .width 	= ROI_LABEL_WIDTH,
						 			 .height	= ROI_LABEL_HEIGHT	};
				
				CanvasDrawText (display->canvasPanHndl, CanvasPan_Canvas, newROI->ROIName,VAL_APP_META_FONT,textrect, VAL_LOWER_LEFT);   
				
				AddImageROI(display->baseClass.image, &newROI, NULL);

				CanvasDrawBitmap (display->canvasPanHndl, CanvasPan_Canvas, display->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
				DrawROIs(display);
       	     };
            break;
        case EVENT_MOUSE_POINTER_MOVE:
			//if selection button is toggled
			GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state);  
			if(state) {
				
				if(eventData2 > display->imgWidth || eventData1 > display->imgHeight)
					return 1;
				//if mouse down, we're currently drawing a rectangular selection
	            if (mouseDown) {

	                //redraw the canvas bitmap and the new rectangle

	                newp = MakePoint (eventData2, eventData1);
	                if (newp.x > oldp.x && newp.y > oldp.y) {
	                    newr = MakeRect (oldp.y, oldp.x, newp.y - oldp.y, newp.x - oldp.x);
	                }
	                else if (newp.x > oldp.x && newp.y < oldp.y) {
	                    newr = MakeRect (newp.y, oldp.x, oldp.y - newp.y, newp.x - oldp.x);
	                }
	                else if (newp.x < oldp.x && newp.y > oldp.y) {
	                    newr = MakeRect (oldp.y, newp.x, newp.y - oldp.y, oldp.x - newp.x);
	                }
	                else {
	                    newr = MakeRect (newp.y, newp.x, oldp.y - newp.y, oldp.x - newp.x);
	                }
	                if (oldr.height || oldr.width) {
	                    CanvasDrawBitmap (display->canvasPanHndl, CanvasPan_Canvas, display->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
					//	DrawROIs(display);				   
	                }
	                CanvasDrawRect (display->canvasPanHndl, CanvasPan_Canvas, newr, VAL_DRAW_FRAME);
	                if (newr.width >= 0 && newr.height >= 0) {
	                    SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_TOP, newr.top);
	                    SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_LEFT, newr.left);
	                    SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_WIDTH, newr.width);
	                    SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_HEIGHT, newr.height);
						
						SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_VISIBLE, 1);
	                }
	                memcpy (&oldr, &newr, sizeof (Rect));
	            }
			}
            break;
    }
    return 0;
}

//==============
static int Zoom (ImageDisplayCVI_type* display, double zoomLevel)
{
INIT_ERR 

	int 	newWidth 			= 0;
	int 	newHeight 			= 0;
	int 	imageWidth			= 0;
	int 	imageHeight			= 0;
	
	display->zoomLevel = zoomLevel;
	
	GetImageSize(display->baseClass.image, &imageWidth, &imageHeight); 

	newWidth 	= RoundRealToNearestInteger(imageWidth * display->zoomLevel);
	newHeight 	= RoundRealToNearestInteger(imageHeight * display->zoomLevel);

	// clean old bitmap since we'll rebuild the image
	if (display->imgBitmapID) {
		DiscardBitmap(display->imgBitmapID);
		display->imgBitmapID = 0;
	}
	
	nullChk ( display->resizedArray  =  (int*) realloc(display->resizedArray, newWidth * newHeight * sizeof(int)) );
 
	// update display image parameters (Image structure in baseClass is unmodified)
	display->imgHeight 	= imageHeight;
	display->imgWidth 	= imageWidth;
	display->nBytes 	= imageHeight * imageWidth * 4;
	
	// convert the pixel array to byte array of pixels (PixelDepth 32 -> 4 bytes per pixel)	
	ConvertToBitArray(display, display->baseClass.image);
	NormalizePixelArray(display, &display->interpolationArray);
	
	// free the byte array and interpolate a new one
	OKfree(display->bitmapBitArray); 
	BilinearResize(display->interpolationArray, &display->resizedArray, imageWidth, imageHeight, newWidth, newHeight);
	
	// update image array size and allocate memory
	display->nBytes = newWidth * newHeight * 4;
	
	// update display info																												
	display->imgWidth 	= newWidth;
	display->imgHeight 	= newHeight;

	nullChk( display->bitmapBitArray = (unsigned char*) malloc(display->nBytes * sizeof(unsigned char)) );  
	
	Deserialize(display, display->resizedArray, newWidth * newHeight);

	// resize canvas according to image size, within defined limits
	if(newWidth < CANVAS_MIN_WIDTH) 
		SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , CANVAS_MIN_WIDTH); 
	else if (newWidth > CANVAS_MAX_WIDTH)
		SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , CANVAS_MAX_WIDTH);    
	else 
		SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , newWidth);
	
	 
	if(newHeight < CANVAS_MIN_HEIGHT)
		SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, CANVAS_MIN_HEIGHT);
	else if (newHeight > CANVAX_MAX_HEIGHT)
		SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, CANVAX_MAX_HEIGHT); 
	else
	SetCtrlAttribute(display->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, newHeight);
	
	errChk( NewBitmap (-1, PixelDepth, display->imgWidth, display->imgHeight, NULL, display->bitmapBitArray, NULL, &display->imgBitmapID) );
	CanvasDrawBitmap (display->canvasPanHndl, CanvasPan_Canvas, display->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
	DrawROIs(display);
	
Error:
	
	return errorInfo.error; 
	
	
}

void NormalizePixelArray(ImageDisplayCVI_type* display, int **arrayPtr) {
	
	int 	imgHeight   = 0;
	int 	imgWidth	= 0;
	int* 	array		= *arrayPtr;
	
	GetImageSize(display->baseClass.image, &imgHeight, &imgWidth);
	
	int 	nPixels 	= imgHeight * imgWidth; 

	// create integer pixel array from byte array
	for(int i = 0; i < nPixels; i++) {
		array[i] = ( display->bitmapBitArray[4 * i] << 24) | ( display->bitmapBitArray[4 * i + 1] << 16) |
				 	( display->bitmapBitArray[4 * i + 2] << 8) | display->bitmapBitArray[4 * i + 3] ;
	
	}
}

static void Deserialize(ImageDisplayCVI_type* display, int* array, int length) {

	for(int i = 0; i < length; i++) {
		display->bitmapBitArray[4 * i] 	   = getByte(array[i], 3);
		display->bitmapBitArray[4 * i + 1] = getByte(array[i], 2);
		display->bitmapBitArray[4 * i + 2] = getByte(array[i], 1);
		display->bitmapBitArray[4 * i + 3] = getByte(array[i], 0);	     
	}
}

static void BilinearResize(int* input, int** outputPtr, int sourceWidth, int sourceHeight, int targetWidth, int targetHeight)
{    
    int a, b, c, d, x, y, index;
	float x_diff, y_diff, blue, red, green;
	
    float 	x_ratio 	= ((float)(sourceWidth - 1)) / targetWidth;
    float 	y_ratio 	= ((float)(sourceHeight - 1)) / targetHeight;
    int*  	output 		= *outputPtr;
    int 	offset 		= 0 ;
 
    for (int i = 0; i < targetHeight; i++) {
        for (int j = 0; j < targetWidth; j++) {
			
            x 		= (int)(x_ratio * j) ;
            y 		= (int)(y_ratio * i) ;
            x_diff 	= (x_ratio * j) - x ;
            y_diff 	= (y_ratio * i) - y ;
            index 	= (y * sourceWidth + x) ;
			
            a 		= input[index] ;
            b 		= input[index + 1] ;
            c 		= input[index + sourceWidth] ;
            d 		= input[index + sourceWidth + 1] ;

			// blue element
            blue 	= getByte(a, 3) * (1-x_diff)*(1-y_diff) + getByte(b, 3) * (x_diff)*(1-y_diff) +
                   	  getByte(c, 3) * (y_diff)*(1-x_diff)   + getByte(d, 3) * (x_diff*y_diff);

			// green element
            green 	= getByte(a, 2) * (1-x_diff)*(1-y_diff) + getByte(b, 2) * (x_diff)*(1-y_diff) +
                  	  getByte(c, 2) * (y_diff)*(1-x_diff)   + getByte(d, 2) * (x_diff*y_diff);
			
 			
			// red element
			red 	= getByte(a, 1) * (1-x_diff)*(1-y_diff) + getByte(b, 1) * (x_diff)*(1-y_diff) +
                      getByte(c, 1) * (y_diff)*(1-x_diff)   + getByte(d, 1) * (x_diff*y_diff);
			
            output [offset++] =
                    0x0 | // alpha
                    ((((int)blue) & 0xff)   << 24) |
                    ((((int)green) & 0xff) << 16) |
                    ((((int)red) & 0xff)  << 8);
			
        }
    }
}


static void DrawROIs(ImageDisplayCVI_type* display) {
	
	ROI_type*	ROI_iterr;
	ListType	ROIlist				= GetImageROIs(display->baseClass.image);
	size_t 		nROIs 				= ListNumItems(ROIlist);
	float		magnify				= 0;    
	
	// start batch drawing, for higher drawing speed
	CanvasStartBatchDraw (display->canvasPanHndl, CanvasPan_Canvas);

	for (int i = 0; i < nROIs; i++) {
	   
	   ROI_iterr = *(ROI_type**) ListGetPtrToItem(ROIlist, i);  
	   
	   if (ROI_iterr->active) {
		    
		   
		    // get the current zoom level
			// all ROI's are stored based on no zoom
			// and current magnification level needs to be applied
		    magnify = display->zoomLevel;
			
			// in case current roi is a rectangle/selection
	   		if (ROI_iterr->ROIType == ROI_Rectangle) {
				
				Rect		rect = { .top 		= ((Rect_type*)ROI_iterr)->top * magnify,
						 			 .left 		= ((Rect_type*)ROI_iterr)->left * magnify,
						 			 .width 	= ((Rect_type*)ROI_iterr)->width * magnify,
						 			 .height	= ((Rect_type*)ROI_iterr)->height * magnify	};
				
				
				Rect	textrect = { .top 		= ((Rect_type*)ROI_iterr)->top * magnify + ROILabel_YOffset,
						 			 .left 		= ((Rect_type*)ROI_iterr)->left * magnify + ROILabel_XOffset,
						 			 .width 	= ROI_LABEL_WIDTH,
						 			 .height	= ROI_LABEL_HEIGHT	};
				
				CanvasDrawRect (display->canvasPanHndl, CanvasPan_Canvas, rect, VAL_DRAW_FRAME);
				CanvasDrawText (display->canvasPanHndl, CanvasPan_Canvas, ROI_iterr->ROIName,VAL_APP_META_FONT,textrect, VAL_LOWER_LEFT);
			}
			
			// in case current roi is a point
			if (ROI_iterr->ROIType == ROI_Point) { 
			
				Point 		point = { .x = ((Point_type*)ROI_iterr)->x * magnify, 
								  	  .y = ((Point_type*)ROI_iterr)->y * magnify	};
			
				Rect	textrect_point = { 	.top 		= point.y + ROILabel_YOffset,
						 			 	   	.left 		= point.x + ROILabel_XOffset,
						 			 		.width 	= ROI_LABEL_WIDTH,
						 			 		.height	= ROI_LABEL_HEIGHT	};
				
				
				// draw the cross that marks a ROI point
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(point.x - CROSS_LENGTH, point.y), MakePoint(point.x + CROSS_LENGTH, point.y));
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(point.x, point.y - CROSS_LENGTH), MakePoint(point.x, point.y + CROSS_LENGTH));
				CanvasDrawText(display->canvasPanHndl, CanvasPan_Canvas, ROI_iterr->ROIName,VAL_APP_META_FONT,textrect_point, VAL_LOWER_LEFT);
					
				
			}
	   	}
	}
	
	CanvasEndBatchDraw (display->canvasPanHndl, CanvasPan_Canvas);  
}

static void CVIROIActions (ImageDisplayCVI_type* display, int ROIIdx, ROIActions action)
{
	ROI_type**		ROIPtr 		= NULL;
	ROI_type*		ROI			= NULL;
	ListType		ROIlist 	= GetImageROIs(display->baseClass.image);
	size_t			nROIs		= ListNumItems(ROIlist);

	if (ROIIdx) {
		ROIPtr = ListGetPtrToItem(ROIlist, ROIIdx);
		ROI = *ROIPtr;
		switch (action) {
			case ROI_Delete:
				
				// discard ROI data
				(*ROI->discardFptr)((void**)ROIPtr);
				// remove ROI from image display list
				ListRemoveItem(ROIlist, 0, ROIIdx);
				DrawROIs(display);
				break;
			case ROI_Show:
				
				if (!ROI->active) {
					DrawROIs(display);
					ROI->active = TRUE;
				}
				
				break;
			case ROI_Hide:
				
				// clear ROI group (shape and label)
				if (ROI->active) {
					DrawROIs(display);
					ROI->active = FALSE;
				}
				
				break;
		}
	} else {
		for (size_t i = 1; i <= nROIs; i++) {
			ROIPtr = ListGetPtrToItem(ROIlist, i);
			ROI = *ROIPtr;
			switch (action) {
				
				case ROI_Show:
				
					if (!ROI->active) {
						DrawROIs(display);
						ROI->active = TRUE;
					}
				
					break;
				
				case ROI_Hide:
				
					// clear imaq ROI group (shape and label)
					if (ROI->active) {
						DrawROIs(display);
						ROI->active = FALSE;
					}
				
					break;
				
				case ROI_Delete:
					
					// discard ROI data
					(*ROI->discardFptr)((void**)ROIPtr);
					// remove ROI from image display list
					ListRemoveItem(ROIlist, 0, ROIIdx);
					DrawROIs(display); 
					break;
			}
		}
	}
}

//Callback triggered when "Save" button is clicked
void CVICALLBACK MenuSaveCB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle) {
		ImageDisplayCVI_type* 	display 		= (ImageDisplayCVI_type*) callbackData;

		char fileSaveDir[MAX_DIRNAME_LEN+4]="";
		char FileName[MAX_FILENAME_LEN]="";
		char PathName[MAX_PATHNAME_LEN]="";
		int bitmapID;
		time_t Time;
		struct tm *TM;
	
		//get current date and time
		Time=time(NULL);
		TM=localtime(&Time);
		strftime (FileName, 80, "Image-%Y%m%d-%H%M%S", TM); //make format more lisible
		strcat(FileName, ".jpg");

		//fire dir select popup
		if(VAL_DIRECTORY_SELECTED!=DirSelectPopup (fileSaveDir, "Save image to folder", 1, 1, fileSaveDir))
					return;
		//assemble path + filename			
		MakePathname (fileSaveDir, FileName, PathName);
		
		//save bitmap
		GetCtrlDisplayBitmap (display->canvasPanHndl, CanvasPan_Canvas, 0, &bitmapID);
		SaveBitmapToJPEGFile (bitmapID, PathName, JPEG_DCTFLOAT, 100); //CHANGE2TIFF
};

//Callback triggered when "Restore" button is clicked  
void CVICALLBACK MenuRestoreCB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle) {
		
		ImageDisplayCVI_type* 	display 		= (ImageDisplayCVI_type*) callbackData;     
		FireCallbackGroup(display->baseClass.callbackGroup, ImageDisplay_RestoreSettings, NULL); 
};

int DisplayRGBImageChannels (ImageDisplayCVI_type* imgDisplay, ColorChannel_type** RChanPtr, ColorChannel_type** GChanPtr, ColorChannel_type** BChanPtr) {

INIT_ERR
	
	ColorChannel_type* RChan; 			
	ColorChannel_type* GChan; 
	ColorChannel_type* BChan;
	
	if(RChanPtr)
		RChan	= *RChanPtr;
	else
		RChan	= NULL;
	
	if(GChanPtr)
		GChan	= *GChanPtr;
	else
		GChan	= NULL;

	if(BChanPtr)
		BChan	= *BChanPtr;
	else
		BChan	= NULL;
	
	int 		RImageHeight		= 0;	
	int 		RImageWidth			= 0;	
	int 		GImageHeight		= 0;	
	int 		GImageWidth			= 0;	
	int 		BImageHeight		= 0;	
	int 		BImageWidth			= 0;	
	int 		finalImageHeight	= 0;
	int 		finalImageWidth		= 0;
	RGBA_type* 	pixelArray			= NULL;
	int			nPixels				= 0;
	Image_type* finalImage			= NULL;
	
	if(RChan == NULL && GChan == NULL && BChan == NULL)
		return -1;
	
	if(RChan) {
		
		//get max height and width for the red Channel resulted image, including all offsets
		GetResultedImageSize(RChan, &RImageHeight, &RImageWidth);

		//reallocate buffers to fit red channel resulted image pixels
		nullChk(imgDisplay->RGBOverlayBuffer[0] = realloc(imgDisplay->RGBOverlayBuffer[0], RImageHeight * RImageWidth * sizeof(float)) );

		//sum up pixels for each image of the red Channel
		AddImagePixels(&imgDisplay->RGBOverlayBuffer[0], RImageHeight, RImageWidth, RChan); 
		
	};
	
	if(GChan) {
		
		//get max height and width for the green Channel resulted image, including all offsets
		GetResultedImageSize(GChan, &GImageHeight, &GImageWidth);
		
		//reallocate buffers to fit green channel resulted image pixels
		nullChk(imgDisplay->RGBOverlayBuffer[1] = realloc(imgDisplay->RGBOverlayBuffer[1], GImageHeight * GImageWidth * sizeof(float)) );

		//sum up pixels for each image of the green Channel  
		AddImagePixels(&imgDisplay->RGBOverlayBuffer[1], GImageHeight, GImageWidth, GChan); 	
	}

	
	if (BChan) {
		//get max height and width for the blue Channel resulted image, including all offsets 
		GetResultedImageSize(BChan, &BImageHeight, &BImageWidth);
		
		//reallocate buffers to fit blue channel resulted image pixels
		nullChk(imgDisplay->RGBOverlayBuffer[2] = realloc(imgDisplay->RGBOverlayBuffer[2], BImageHeight * BImageWidth * sizeof(float)) );
		
		//sum up pixels for each image of the blue Channel 
		AddImagePixels(&imgDisplay->RGBOverlayBuffer[2], BImageHeight, BImageWidth, BChan); 
	}
	
	//adjust final image to the size of the channel resulted image with highest dimensions
	if(finalImageHeight < RImageHeight)
		finalImageHeight = RImageHeight;
	if(finalImageWidth < RImageWidth)
		finalImageWidth = RImageWidth;
	
	
	if(finalImageHeight < GImageHeight)
		finalImageHeight = GImageHeight;
	if(finalImageWidth < GImageWidth)
		finalImageWidth = GImageWidth;
	
	
	if(finalImageHeight < BImageHeight)
		finalImageHeight = BImageHeight;
	if(finalImageWidth < BImageWidth)
		finalImageWidth = BImageWidth;
	
	nPixels = finalImageWidth * finalImageHeight;
	
	//allocate pixelArray to be used when constructing the final image
	nullChk( pixelArray=(RGBA_type*) calloc(nPixels, sizeof(RGBA_type)) );
	
	//set R, G or B field in the pixelArray for the final Image, Normalize convers from [max, min] range in RGBOverlayBuffer[i] to [0, 255]
	if(RChan){
		int RedPixelsNum = RImageHeight * RImageWidth;
		NormalizeFloatArrayToRGBA(&pixelArray, imgDisplay->RGBOverlayBuffer[0], RedPixelsNum, 0);
	}
	
	if(GChan){
		int GreenPixelsNum = GImageHeight * GImageWidth;
		NormalizeFloatArrayToRGBA(&pixelArray, imgDisplay->RGBOverlayBuffer[1], GreenPixelsNum, 1);  
	}
	
	if(BChan){
		int BluePixelsNum = BImageHeight * BImageWidth;
		NormalizeFloatArrayToRGBA(&pixelArray, imgDisplay->RGBOverlayBuffer[2], BluePixelsNum, 2);  
		
	}

	finalImage = init_Image_type(Image_RGBA, finalImageHeight, finalImageWidth, &pixelArray);
	
	DisplayImage(imgDisplay, &finalImage);
	
	return 0;
	
	
Error:
	return errorInfo.error;
}


//Converts a float array to a [0, 255] range array, representing R, G or B field in RGBA_type pixel array
void NormalizeFloatArrayToRGBA(RGBA_type* outputPtr[], float* input, int nPixels, int RGBAChannel) {
	
	float 		min 		= input[0];
	float 		max 		= input[0];
	float 		interval 	=  0;
	RGBA_type* 	output 		= *outputPtr;
	
	for(int i = 0; i < nPixels; i++) {
		if (input[i] > max)
			max = input[i];
		if (input[i] < min)
			min = input[i];
	}
	interval = max - min;
	
	for(int i = 0; i < nPixels; i++) {
		if(RGBAChannel == 0) {
			output[i].R = (unsigned char) ((input[i] - min)	* 255.0 / interval);
		}
		if(RGBAChannel == 1) {
			output[i].G = (unsigned char) ((input[i] - min)	* 255.0 / interval);
		}
		if(RGBAChannel == 2) {
			output[i].B = (unsigned char) ((input[i] - min)	* 255.0 / interval);
		}
	}
}
//Taking into account all offsets, calculate the max size(height and width) for the image
// resulted by summing up pixels from all of the channel's images.
void GetResultedImageSize(ColorChannel_type* colorChan, int* heightPtr, int* widthPtr) {
  
  int MaxHeight = 0;
  int MaxWidth 	= 0;
  int crtHeight = 0;
  int crtWidth  = 0;
  
  for (int i = 0; i < colorChan->nImages; i++) {
  	GetImageSize( colorChan->images[i], &crtHeight, &crtWidth);
	
	if ( (crtHeight + colorChan->xOffsets[i])  > MaxHeight)
		MaxHeight = crtHeight + colorChan->xOffsets[i];
	
	if ( (crtWidth + colorChan->yOffsets[i]) > MaxWidth)
		MaxWidth = crtWidth + colorChan->yOffsets[i];
  }
  
  *heightPtr = MaxHeight;
  *widthPtr  = MaxWidth;
  
};

//Sum pixels in each position for all of the channel's images, taking into account the offset
void AddImagePixels(float** ResultImagePixArrayPtr, int ResultImageHeight, int ResultImageWidth, ColorChannel_type*  colorChan) {
	
	ImageTypes 			imgType;
	int					height;
	int 				width;
	int 				pixels;
	float*				array = *ResultImagePixArrayPtr;
	for(int i = 0; i < colorChan->nImages; i++) {
		
		imgType = GetImageType(colorChan->images[i]);
		GetImageSize(colorChan->images[i], &height, &width);
		pixels = height * width;
		
		int offset = colorChan->xOffsets[i] * ResultImageHeight + colorChan->yOffsets[i];
		
		switch(imgType) {
			case Image_UChar:
			
			unsigned char* uchar_iterr = (unsigned char*) GetImagePixelArray(colorChan->images[i]);
			for(int j = 0; j < pixels; j++) {
				// add_of(array[offset + j], aray[offset + j] , uchar_iterr[j])
				array[offset + j] = array[offset + j] +  uchar_iterr[j];
			}
			
			break;
			
			case Image_UShort:					// 16 bit unsigned
			
			unsigned short int* ushort_iterr = (unsigned short int*) GetImagePixelArray(colorChan->images[i]);
			for(int j = 0; j < pixels; j++) {
				array[offset + j] = array[offset + j] +  ushort_iterr[j];
			}
			
			break;
			
			case Image_Short:
			
			short int* short_iterr = (short int*) GetImagePixelArray(colorChan->images[i]);
			for(int j = 0; j < pixels; j++) {
				array[offset + j] = array[offset + j] +  short_iterr[j];
			}
			
			break;
			
			case Image_UInt:					// 32 bit unsigned
			
			unsigned  int* 	uint_iterr 		= (unsigned int*) GetImagePixelArray(colorChan->images[i]);
			for(int j = 0; j < pixels; j++) {
				array[offset + j] = array[offset + j] +  uint_iterr[j];
			}
			
			break;
			
			case Image_Int:						// 32 bit signed
			
			int* 	int_iterr 		= (int*) GetImagePixelArray(colorChan->images[i]);
			for(int j = 0; j < pixels; j++) {
				array[offset + j] = array[offset + j] +  int_iterr[j];
			}
			
			break;

			case Image_Float:					// 32 bit float 
			
			float* 	float_iterr 		= (float*) GetImagePixelArray(colorChan->images[i]);
			for(int j = 0; j < pixels; j++) {
				array[offset + j] = array[j] +  float_iterr[j];
			}
			
			break;
			
		}
	}
	
}
			
