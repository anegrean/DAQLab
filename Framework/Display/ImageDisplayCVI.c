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
	
	ListType				images;					// List of Image_type* elements. Data in this image is used to update the display whenever needed. Note: do not modify this data directly!
	BOOL					update;					// If True, the plot will display each time the latest waveform added to it. If False, the user can use the scrollbar to return to a previous plot. Default: True.
	int						imgBitmapID;			// this is the handle to the image provided to the display canvas, it is updated from the "images" array, when needed.
	unsigned char* 			bitmapBitArray;			// Array of bits used to create the bitmap image
	int						nBytes;					// number of bytes in the bitmapBitArray;
	int						imgHeight;				// image height
	int 					imgWidth;				// image width
	unsigned char			ZoomLevel;				// current Zoom level (can be 0,1,2,3,4)
	int*					interpolationArray; 	// array used to perform normalization ( squashing 4 bytes into an int) for the Bilinear interpolation
	int*					resizedArray;			// array used to deserialize bytes from the integer into bitmapBitArray
	float*					RGBOverlayBuffer[3];	// Image buffer used to sum up and overlay images from multiple RGB channels.
	// UI
	int						displayPanHndl;
	int						canvasPanHndl;
	int						menuBarHndl;
	int						FilemenuID;
	int						ImagemenuID;
	
	// CALLBACK
	//WaveformDisplayCB_type	callbackFptr;
	void*					callbackData;
};

//==============================================================================
// Static global variables

static int buttonArray[] = {DisplayPan_PICTUREBUTTON, DisplayPan_PICTUREBUTTON_2, DisplayPan_PICTUREBUTTON_3, DisplayPan_PICTUREBUTTON_4, DisplayPan_PICTUREBUTTON_5, DisplayPan_PICTUREBUTTON_6};


//==============================================================================
// Static functions

static void 					DiscardImageList 			(ListType* imageListPtr);

static int						DisplayImageFunction		(ImageDisplayCVI_type* imgDisplay, Image_type** image); // CPU 

static void 					ConvertToBitArray			(ImageDisplayCVI_type* display, Image_type* image);

static void 					BilinearResize				(int* input, int** outputPtr, int sourceWidth, int sourceHeight, int targetWidth, int targetHeight);

static int 						Zoom						(ImageDisplayCVI_type* display, char direction);

static int CVICALLBACK 			CanvasCallback 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCtrlCallback 		(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCallback			(int panel, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			CanvasPanelCallback 		(int panel, int event, void *callbackData, int eventData1, int eventData2);

void 							NormalizePixelArray			(ImageDisplayCVI_type* display, int **arrayPtr);

static void 					Deserialize					(ImageDisplayCVI_type* display, int* array, int length);

static void 					DrawROIs					(ImageDisplayCVI_type* display);

void CVICALLBACK 				MenuSaveCB 					(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

void CVICALLBACK 				MenuRestoreCB 				(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);




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
	
	SetPanelAttribute(imgDisplay->canvasPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, CanvasPanelCallback);
	SetPanelAttribute(imgDisplay->canvasPanHndl, ATTR_CALLBACK_DATA, imgDisplay);
	
	// allow access to ImgDisplay
	SetCtrlsInPanCBInfo(imgDisplay, CanvasCallback, imgDisplay->canvasPanHndl);
	SetCtrlsInPanCBInfo(imgDisplay, DisplayPanCtrlCallback, imgDisplay->displayPanHndl);
	
	// set Z plane priortiy for canvas and selection tool
	SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_ZPLANE_POSITION, 0);
    SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_SELECTION, ATTR_ZPLANE_POSITION, 1);
	
	// set canvas pen color
	
	SetCtrlAttribute (imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_PEN_COLOR, VAL_GREEN); 
	
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
		nullChk( imgDisplay->bitmapBitArray 		= malloc(BytesPerPixelDepth * imgWidth * imgHeight) );
		nullChk( imgDisplay->resizedArray   		= malloc(imgWidth * imgHeight * sizeof(int)) );
		nullChk( imgDisplay->interpolationArray   	= malloc(imgWidth * imgHeight * sizeof(int)) ); 
	} else
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


static void ConvertToBitArray(ImageDisplayCVI_type* display, Image_type* image) 
{
INIT_ERR
	
	int 				iterrations 		= 0;
	int					imgHeight			= 0;
	int					imgWidth			= 0;
	unsigned char*  	pixArray			= NULL;
	ImageTypes 			imgType				= GetImageType(image);
	
	
	GetImageSize(image, &imgWidth, &imgHeight);
	
	int 				nPixels				= imgHeight * imgWidth;
			
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
		nullChk( imgDisplay->bitmapBitArray 		= (unsigned char*) malloc(imgHeight * imgWidth * BytesPerPixelDepth) );
		nullChk( imgDisplay->interpolationArray 	= (int*) realloc(imgDisplay->interpolationArray, imgHeight * imgWidth * sizeof(int)));
		
		imgDisplay->nBytes	  = imgHeight * imgWidth * BytesPerPixelDepth; 
		imgDisplay->imgHeight = imgHeight;
		imgDisplay->imgWidth  = imgWidth;
	
	
		//convert new image data to bit array and create bitmap image
		ConvertToBitArray(imgDisplay, image);
		errChk( NewBitmap (-1, pixelDepth, imgDisplay->imgWidth, imgDisplay->imgHeight, NULL, imgDisplay->bitmapBitArray, NULL, &imgDisplay->imgBitmapID) );
	
	} else {
		
		//only update bitmap data 
		ConvertToBitArray(imgDisplay, image);
		SetBitmapData(imgDisplay->imgBitmapID, -1, pixelDepth, NULL, imgDisplay->bitmapBitArray, NULL);
	
	}

	SetCtrlAttribute(imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , imgWidth);
	SetCtrlAttribute(imgDisplay->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, imgHeight);
	imgDisplay->ZoomLevel = 0;
	
	//draw bitmap on canvas
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
			GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON, &state); 
			if(state)	
				Zoom(display, 1);

		break;
		
		case EVENT_LEFT_CLICK:
			
			//if zoom button is toggled
			GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON, &state); 
			if(state)	
				Zoom(display, -1);
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
			 	case DisplayPan_PICTUREBUTTON:
				
					GetCtrlVal (display->displayPanHndl, control, &state);
				 
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					break;
				
				case DisplayPan_PICTUREBUTTON_2:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					break;
				
				case DisplayPan_PICTUREBUTTON_3:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < BUTNUM; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
				 
					break;
					
				case DisplayPan_PICTUREBUTTON_4:  
			
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
	GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON_2, &state); 
	
    switch (event) {
		
        case EVENT_LEFT_CLICK:
			
			
			//if selection ROI button is toggled 
			GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON_2, &state);  
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
			GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON_3, &state);
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
				newROI = (ROI_type*)initalloc_Point_type(NULL, "", color, TRUE, x / (1 + display->ZoomLevel * zoomFactor), y / (1 + display->ZoomLevel * zoomFactor));
				
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
			 GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON_2, &state); 
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
				int top  = newr.top / (1 + display->ZoomLevel * zoomFactor);
				int left = newr.left / (1 + display->ZoomLevel * zoomFactor);
				int height = newr.height / (1 + display->ZoomLevel * zoomFactor);
				int width = newr.width / (1 + display->ZoomLevel * zoomFactor);
				
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
			GetCtrlVal (display->displayPanHndl, DisplayPan_PICTUREBUTTON_2, &state);  
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
static int Zoom (ImageDisplayCVI_type* display, char direction)
{
INIT_ERR 

	int 	newWidth 			= 0;
	int 	newHeight 			= 0;
	int 	imageWidth			= 0;
	int 	imageHeight			= 0;
	
	//zoom out, if zoom strictly positive
	if (display->ZoomLevel > 0 && direction < 0)
		display->ZoomLevel--;

	//zoom in, if zoom smaller than max zoom level
	if (display->ZoomLevel < maxZoomLevel && direction > 0)
		display->ZoomLevel++;

	GetImageSize(display->baseClass.image, &imageWidth, &imageHeight); 

	newWidth = (int) (imageWidth * (1 + display->ZoomLevel * zoomFactor));
	newHeight = (int) (imageHeight * (1 + display->ZoomLevel * zoomFactor));

	//clean old bitmap since we'll rebuild the image
	DiscardBitmap(display->imgBitmapID);
	
	nullChk ( display->resizedArray  =  (int*) realloc(display->resizedArray, newWidth * newHeight * sizeof(int)));
 
	
	//update display image parameters ( Image structure in baseClass is unmodified ! )
	display->imgHeight = imageHeight;
	display->imgWidth = imageWidth;
	display->nBytes = imageHeight * imageWidth * BytesPerPixelDepth;
	
	//convert the pixel array to byte array of pixels (PixelDepth 32 -> 4 bytes per pixel)	
	ConvertToBitArray(display, display->baseClass.image);
	NormalizePixelArray(display, &display->interpolationArray);
	
	//free the byte array and interpolate a new one
	OKfree(display->bitmapBitArray); 
	BilinearResize(display->interpolationArray, &display->resizedArray, imageWidth, imageHeight, newWidth, newHeight);
	
	//update image array size and allocate memory
	display->nBytes = newWidth * newHeight * BytesPerPixelDepth;
	
	//update display info																												
	display->imgWidth = newWidth;
	display->imgHeight = newHeight;

	nullChk( display->bitmapBitArray = (unsigned char*) malloc(display->nBytes * sizeof(unsigned char)) );  
	
	Deserialize(display, display->resizedArray, newWidth * newHeight);

	
	//resize canvas according to image size, within defined limits
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
	
	errChk( NewBitmap (-1, pixelDepth, display->imgWidth, display->imgHeight, NULL, display->bitmapBitArray, NULL, &display->imgBitmapID) );
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

	//create integer pixel array from byte array
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
            blue = getByte(a, 3) * (1-x_diff)*(1-y_diff) + getByte(b, 3) * (x_diff)*(1-y_diff) +
                   getByte(c, 3) * (y_diff)*(1-x_diff)   + getByte(d, 3) * (x_diff*y_diff);

			// green element
            green = getByte(a, 2) * (1-x_diff)*(1-y_diff) + getByte(b, 2) * (x_diff)*(1-y_diff) +
                  	getByte(c, 2) * (y_diff)*(1-x_diff)   + getByte(d, 2) * (x_diff*y_diff);
			
 			
			// red element
			red = 	getByte(a, 1) * (1-x_diff)*(1-y_diff) + getByte(b, 1) * (x_diff)*(1-y_diff) +
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
	
	//start batch drawing, for higher drawing speed
	CanvasStartBatchDraw (display->canvasPanHndl, CanvasPan_Canvas);

	for (int i = 0; i < nROIs; i++) {
	   
	   ROI_iterr = *(ROI_type**) ListGetPtrToItem(ROIlist, i);  
	   
	   if(ROI_iterr->active) {
		    
		   
		    //get the current zoom level
			//all ROI's are stored based on no zoom
			//and current magnification level needs to be applied
		    magnify = (1 + display->ZoomLevel * zoomFactor);
			
			//in case current roi is a rectangle/selection
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
			
			//in case current roi is a point
			if (ROI_iterr->ROIType == ROI_Point) { 
			
				Point 		point = { .x = ((Point_type*)ROI_iterr)->x * magnify, 
								  	  .y = ((Point_type*)ROI_iterr)->y * magnify	};
			
				Rect	textrect_point = { 	.top 		= point.y + ROILabel_YOffset,
						 			 	   	.left 		= point.x + ROILabel_XOffset,
						 			 		.width 	= ROI_LABEL_WIDTH,
						 			 		.height	= ROI_LABEL_HEIGHT	};
				
				
				//draw the cross that marks a ROI point
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
		SaveBitmapToJPEGFile (bitmapID, PathName, JPEG_DCTFLOAT, 100);
};

//Callback triggered when "Restore" button is clicked  
void CVICALLBACK MenuRestoreCB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle) {
		
		ImageDisplayCVI_type* 	display 		= (ImageDisplayCVI_type*) callbackData;     
		FireCallbackGroup(display->baseClass.callbackGroup, ImageDisplay_RestoreSettings, NULL); 
};
