//==============================================================================
//
// Title:		ImageDisplayCVI2.c
// Purpose:		A short description of the interface.
//
// Created on:	21-2-2016 at 19:19:44 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h"
#include "UI_ImageDisplay.h"
#include "ImageDisplayCVI2.h"

//==============================================================================

// Constants

// Pixel depth represents how many bits from the bitmapBitArray each pixel has. If PixelDepth is 24, each pixel is represented by a 24-bit RGB value of the form RRGGBB,
// where RR, GG, and BB represent the red, green, and blue components of the color value. The RR byte is at the lowest memory address of the three bytes.\

#define PixelDepth 					24
#define ZoomFactor 					0.25
#define maxZoomLevel 				4
		
#define ButtonNum 					6
#define Canvas_Min_Height			200
#define Canvas_Min_Width			200
#define Canvas_Max_Height			1000
#define Canvas_Max_Width			1000
#define ROI_Label_Width				10
#define	ROI_Label_Height			15

#define ROILabel_XOffset			3		// ROI text label offset in the X direction in pixels.
#define ROILabel_YOffset			3		// ROI text label offset in the Y direction in pixels.
		
#define Default_ROI_R_Color			0	   	// red
#define Default_ROI_G_Color			255	   	// green
#define Default_ROI_B_Color			0		// blue
#define Default_ROI_A_Color			0		// alpha
		
#define Cross_Length				6


//==============================================================================
// Types

struct ImageDisplayCVI {
	
	//---------------------------------------------------------------------------------------------------------------
	// BASE CLASS
	//---------------------------------------------------------------------------------------------------------------
	
	ImageDisplay_type 		baseClass; 
	
	//---------------------------------------------------------------------------------------------------------------
	// DATA
	//---------------------------------------------------------------------------------------------------------------
	
	ListType				images;					// List of Image_type* elements. Data in this image is used to update the display whenever needed. Note: do not modify this data directly!
	BOOL					update;					// If True (default), the display will be updated each time a new image is added to it. If False, the user can use the scrollbar to return to a previous image.
	int						imgBitmapID;			// Bitmap placed on the canvas that is generated using the provided image data.
	unsigned char* 			bitmapBitArray;			// Array of bits used to display the image.
	int						imgHeight;				// Height in pixels of the displayed image.
	int 					imgWidth;				// Width in pixels of the displayed image.
	float					zoom;					// Current zoom level.
	
	//---------------------------------------------------------------------------------------------------------------
	// UI
	//---------------------------------------------------------------------------------------------------------------
	
	int						displayPanHndl;
	int						canvasPanHndl;
	int						menuBarHndl;
	int						fileMenuID;
	int						imageMenuID;
	
	//---------------------------------------------------------------------------------------------------------------
	// CALLBACK
	//---------------------------------------------------------------------------------------------------------------
	
	void*					callbackData;
};

//==============================================================================
// Static global variables

static int buttonArray[] = {DisplayPan_Zoom, DisplayPan_RectROI, DisplayPan_PointROI, DisplayPan_Select, DisplayPan_Bttn1, DisplayPan_Bttn2};


//==============================================================================
// Static functions

static int						DisplayImage				(ImageDisplayCVI_type* imgDisp, Image_type** image, char** errorMsg);

static int	 					ConvertToBitArray			(ImageDisplayCVI_type* imgDisp, Image_type* RGBImages[], char** errorMsg);

static int CVICALLBACK 			CanvasCB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCtrlCB 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			DisplayPanCB				(int panel, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 			CanvasPanelCB 				(int panel, int event, void *callbackData, int eventData1, int eventData2);

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
												 CallbackGroup_type**	callbackGroupPtr)		 
{
INIT_ERR

	//-------------------------------------
	// Init parent class
	//-------------------------------------
	
	ImageDisplayCVI_type*	imgDisp = malloc(sizeof(ImageDisplayCVI_type));
	if (!imgDisp) return NULL;
	
	
	init_ImageDisplay_type(&imgDisp->baseClass, &imgDisp, NULL, (DiscardFptr_type) discard_ImageDisplayCVI_type, (DisplayImageFptr_type) DisplayImage, NULL, callbackGroupPtr);

	//-------------------------------------
	// Init child class
	//-------------------------------------
	
	
	// Init
	//---------------------------------
	
	// Data
	imgDisp->images				= 0;
	imgDisp->imgBitmapID		= 0;
	imgDisp->imgHeight			= 0;
	imgDisp->imgWidth			= 0;
	imgDisp->bitmapBitArray		= NULL;
	imgDisp->zoom				= 1;
	
	// UI
	imgDisp->displayPanHndl		= 0;
	imgDisp->canvasPanHndl		= 0;
	imgDisp->menuBarHndl		= 0;
	imgDisp->fileMenuID			= 0;
	imgDisp->imageMenuID		= 0;
	
	// Alloc
	//---------------------------------
		
	// loading display panel into workspace panel
	errChk( imgDisp->displayPanHndl = LoadPanel (parentPanHandl, ImageDisplay_UI, DisplayPan) );

	// loading canvas panel into display panel
	errChk( imgDisp->canvasPanHndl = LoadPanel (imgDisp->displayPanHndl, ImageDisplay_UI, CanvasPan) ); 

	// set panel callback
	SetPanelAttribute(imgDisp->displayPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, DisplayPanCB);
	SetPanelAttribute(imgDisp->displayPanHndl, ATTR_CALLBACK_DATA, imgDisp);
	
	SetPanelAttribute(imgDisp->canvasPanHndl, ATTR_CALLBACK_FUNCTION_POINTER, CanvasPanelCB);
	SetPanelAttribute(imgDisp->canvasPanHndl, ATTR_CALLBACK_DATA, imgDisp);
	
	SetCtrlsInPanCBInfo(imgDisp, CanvasCB, imgDisp->canvasPanHndl);
	SetCtrlsInPanCBInfo(imgDisp, DisplayPanCtrlCB, imgDisp->displayPanHndl);
	
	// set Z plane priortiy for canvas and selection tool
	SetCtrlAttribute (imgDisp->canvasPanHndl, CanvasPan_Canvas, ATTR_ZPLANE_POSITION, 0);
    SetCtrlAttribute (imgDisp->canvasPanHndl, CanvasPan_SELECTION, ATTR_ZPLANE_POSITION, 1);
	
	// set canvas pen color & text background color
	SetCtrlAttribute (imgDisp->canvasPanHndl, CanvasPan_Canvas, ATTR_PEN_COLOR, VAL_GREEN); 
	SetCtrlAttribute (imgDisp->canvasPanHndl, CanvasPan_Canvas, ATTR_PEN_FILL_COLOR, VAL_TRANSPARENT);
	
	// add menu items
	imgDisp->menuBarHndl = NewMenuBar(imgDisp->displayPanHndl);
	// File
	imgDisp->fileMenuID = NewMenu (imgDisp->menuBarHndl, "File", -1);
	// File->Save
	NewMenuItem(imgDisp->menuBarHndl, imgDisp->fileMenuID, "Save", -1, VAL_F2_VKEY, MenuSaveCB, imgDisp);
	// Image
	imgDisp->imageMenuID = NewMenu(imgDisp->menuBarHndl, "Image", -1);
	// Image->Restore
	NewMenuItem(imgDisp->menuBarHndl, imgDisp->imageMenuID, "Restore", -1, VAL_F1_VKEY, MenuRestoreCB, imgDisp);
	
	// create image list
	nullChk( imgDisp->images = ListCreate(sizeof(Image_type*)) );

	DisplayPanel (imgDisp->canvasPanHndl);
	
	return imgDisp;
	
Error:
	
	// cleanup
	discard_ImageDisplayCVI_type(&imgDisp);
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

	
	//--------------
	// Parent Class
	//--------------
	
	discard_ImageDisplay_type ((ImageDisplay_type**)imageDisplayPtr);
}

static int ConvertToBitArray (ImageDisplayCVI_type* imgDisp, Image_type* RGBImages[], char** errorMsg) 
{
#define ConvertToBitArray_Err_DimensionsMismatch		-1
#define ConvertToBitArray_Err_ImageTypeNotSupported   	-2
INIT_ERR
	
	int					imgHeight_R		= 0;
	int					imgWidth_R		= 0;
	int					imgHeight		= 0;
	int					imgWidth		= 0;
	int 				nPixels			= 0;
	unsigned char*		newBitArray		= NULL;
	
	int					px				= 0;
	int					py				= 0;
	float				zoom_ratio		= 0;
	
	// check if image dimensions match
	GetImageSize(RGBImages[0], &imgWidth_R, &imgHeight_R);
	for (int i = 1; i < 3; i++) {
		GetImageSize(RGBImages[i], &imgWidth, &imgHeight);
		if (imgWidth != imgWidth_R || imgHeight != imgHeight_R)
			SET_ERR(ConvertToBitArray_Err_DimensionsMismatch, "Image dimensions on all RGB channels must match.");
		
	}
	
	nPixels	= imgHeight * imgWidth;
	imgDisp->imgHeight 	= RoundRealToNearestInteger(imgHeight * imgDisp->zoom);
	imgDisp->imgWidth 	= RoundRealToNearestInteger(imgWidth * imgDisp->zoom); 
	zoom_ratio			= (float)imgHeight/imgDisp->imgHeight;
	
	// reallocate bitmap array
	nullChk( newBitArray = realloc(imgDisp->bitmapBitArray, imgDisp->imgHeight * imgDisp->imgWidth * 3) );
	imgDisp->bitmapBitArray = newBitArray;
		
	
	for (int chan = 0; chan < 3; chan++) {	
		switch (GetImageType(RGBImages[chan])) {
		
			// 8 bit  
			case Image_UChar:					
	
				unsigned char* 			ucharPix 			= (unsigned char*) GetImagePixelArray(RGBImages[chan]);
	
				for (int i = 0; i < imgDisp->imgHeight; i++)
					for (int j = 0; j < imgDisp->imgWidth; j++) {
						px = (int) (j * zoom_ratio);
						py = (int) (i * zoom_ratio);
						imgDisp->bitmapBitArray[((i*imgDisp->imgWidth)+j)*3+chan] = ucharPix[py*imgWidth+px];
					}
						
				
				break;
	
			// 16 bit unsigned
			case Image_UShort:
		
				unsigned short int* 	ushortPix 			= (unsigned short int*) GetImagePixelArray(RGBImages[chan]);
				unsigned short int		ushort_min	 		= ushortPix[0];
				unsigned short int		ushort_max	 		= ushortPix[0];
	
				for (int i = 0; i < nPixels; i++) {
					if (ushortPix[i] > ushort_max)
						ushort_max = ushortPix[i];
		
					if (ushortPix[i] < ushort_min)
						ushort_min = ushortPix[i];
				}
	
				float					ushortScaleFactor 	= (float)255.0 / (ushort_max - ushort_min);
	
				for (int i = 0; i < nPixels; i++)
					imgDisp->bitmapBitArray[i*3+chan]   	= (unsigned char) ((ushortPix[i] - ushort_min) * ushortScaleFactor);
				
				break;								
	
			// 16 bit signed 
			case Image_Short:
	
				short int* 				shortPix  			= (short int*) GetImagePixelArray(RGBImages[chan]);
				short int  				short_min 			= shortPix[0];
				short int  				short_max			= shortPix[0];

				for (int i = 0; i < nPixels; i++) {
					if (shortPix[i] > short_max)
						short_max = shortPix[i];
		
					if (shortPix[i] < short_min)
						short_min = shortPix[i];
				}
	
				float					shortScaleFactor 	= (float)255.0 / (short_max - short_min); 
	
				for (int i = 0; i < nPixels; i++)
					imgDisp->bitmapBitArray[i*3+chan]   	= (unsigned char)((shortPix[i] - short_min) * shortScaleFactor);
				
				break;
	
			// 32 bit unsigned 
			case Image_UInt:					
		
				unsigned  int* 			uintPix 			= (unsigned int*) GetImagePixelArray(RGBImages[chan]);
				unsigned  int			uint_min	 		= uintPix[0];
				unsigned  int			uint_max	 		= uintPix[0];
	
				for (int i = 0; i < nPixels; i++) {
					if (uintPix[i] > uint_max)
						uint_max = uintPix[i];
		
					if (uintPix[i] < uint_min)
						uint_min = uintPix[i];
				}
	
				float					uintScaleFactor 	= (float)255.0 / (uint_max - uint_min);  
	
				for (int i = 0; i < nPixels; i++)
					imgDisp->bitmapBitArray[i*3+chan]  	= (unsigned char)((uintPix[i] - uint_min) * uintScaleFactor);
					
				break;										
	
			// 32 bit signed 
			case Image_Int:

				int* 					intPix 	 			= (int*) GetImagePixelArray(RGBImages[chan]);
				int	 					int_min	 			= intPix[0];
				int	 					int_max			 	= intPix[0];
	
				for (int i = 0; i < nPixels; i++) {
					if (intPix[i] > int_max)
						int_max = intPix[i];
		
					if (intPix[i] < int_min)
						int_min = intPix[i];
				}
	
				float					intScaleFactor 		= (float)255.0 / (int_max - int_min);
	
				for (int i = 0; i < nPixels; i++)
					imgDisp->bitmapBitArray[i*3+chan]	  	= (unsigned char)((intPix[i] - int_min) * intScaleFactor);
					
				break;

			// 32 bit float
			case Image_Float:					
	
				float* 					floatPix 	 		= (float*) GetImagePixelArray(RGBImages[chan]);
				float  					float_min	 		= floatPix[0];
				float  					float_max			= floatPix[0];
	
				for (int i = 0; i < nPixels; i++) {
					if (floatPix[i] > float_max)
						float_max = floatPix[i];
		
					if (floatPix[i] < float_min)
						float_min = floatPix[i];
				}
												
				float					floatScaleFactor 	= (float)255.0 / (float_max - float_min);
	
				for (int i = 0; i < nPixels; i++)
					imgDisp->bitmapBitArray[i*3+chan]   	= (unsigned char) ((floatPix[i] - float_min) * floatScaleFactor);
					
				break;
				
			default:
			
				SET_ERR(ConvertToBitArray_Err_ImageTypeNotSupported, "Image type not supported.");
		
				break;
	
		}
	}
		
Error:
	
RETURN_ERR
	
}


static int DisplayImage (ImageDisplayCVI_type* imgDisp, Image_type** imagePtr, char** errorMsg)
{
INIT_ERR 
	
	Image_type* 		image     		= *imagePtr;
	int 				imgOldHeight 	= 0;
	int 				imgOldWidth 	= 0;
	Image_type*			RGBImages[3]	= {image, image, image};	// display as grayscale for now
	
	// take in the image
	imgDisp->baseClass.image = image;
	*imagePtr = NULL;
	
	// retain old display image dimensions to compare later
	imgOldHeight = imgDisp->imgHeight;
	imgOldWidth = imgDisp->imgWidth;
	
	// convert new image data to bit array
	errChk( ConvertToBitArray(imgDisp, RGBImages, &errorInfo.errMsg) );
	
	if (imgOldWidth != imgDisp->imgWidth || imgOldHeight != imgDisp->imgHeight) {
	
		// clear old display bitmap data
		if(imgDisp->imgBitmapID) { 
			DiscardBitmap(imgDisp->imgBitmapID);
			imgDisp->imgBitmapID = 0;
		}
		
		errChk( NewBitmap (-1, PixelDepth, imgDisp->imgWidth, imgDisp->imgHeight, NULL, imgDisp->bitmapBitArray, NULL, &imgDisp->imgBitmapID) );
	
	} else
		// only update bitmap data 
		SetBitmapData(imgDisp->imgBitmapID, -1, PixelDepth, NULL, imgDisp->bitmapBitArray, NULL);
	
	// adjust canvas size to match image size
	SetCtrlAttribute(imgDisp->canvasPanHndl, CanvasPan_Canvas, ATTR_WIDTH , imgDisp->imgWidth);
	SetCtrlAttribute(imgDisp->canvasPanHndl, CanvasPan_Canvas, ATTR_HEIGHT, imgDisp->imgHeight);
	
	// draw bitmap on canvas
	CanvasDrawBitmap (imgDisp->canvasPanHndl, CanvasPan_Canvas, imgDisp->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
	
	// draw ROIs
	DrawROIs(imgDisp);
	
	int panVisible	= FALSE;
	GetPanelAttribute(imgDisp->displayPanHndl, ATTR_VISIBLE, &panVisible);
	
	if (!panVisible)
		DisplayPanel(imgDisp->displayPanHndl); 
	
	return 0;
	
Error: 
	
	return errorInfo.error;
	
}

static int CVICALLBACK CanvasCB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ImageDisplayCVI_type* display 	= (ImageDisplayCVI_type*) callbackData;   
	int 				  state	  	= 0;

	switch (event) {
			
		case EVENT_CLOSE:
			
			HidePanel(panel);
			
		break;
		
		case EVENT_RIGHT_CLICK:
			
			// zoom out if zoom button is selected
			GetCtrlVal (display->displayPanHndl, DisplayPan_Zoom, &state);
			
			if(state) {
				display->zoom = display->zoom * (1-ZoomFactor);
				Image_type*	image = display->baseClass.image;
				errChk( DisplayImage(display, &image, &errorInfo.errMsg) );	// update display
			}

		break;
		
		case EVENT_LEFT_CLICK:
			
			// zoom in if zoom button is selected
			GetCtrlVal (display->displayPanHndl, DisplayPan_Zoom, &state); 
			
			if(state) {
				display->zoom = display->zoom * (1+ZoomFactor);
				Image_type*	image = display->baseClass.image;
				errChk( DisplayImage(display, &image, &errorInfo.errMsg) );	// update display
			}
			
		break;
	}
	
	return 0;
	
Error:
	
PRINT_ERR
	return 0;
}

static int CVICALLBACK DisplayPanCtrlCB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ImageDisplayCVI_type* display = (ImageDisplayCVI_type*) callbackData;
	
	switch(event) {
			
		case EVENT_COMMIT:
			
			 int state = 0;
			 
			 switch(control) {
					 
			 	case DisplayPan_Zoom:
				
					GetCtrlVal (display->displayPanHndl, control, &state);
				 
					for(int i = 0; i < ButtonNum; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					break;
				
				case DisplayPan_RectROI:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < ButtonNum; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					break;
				
				case DisplayPan_PointROI:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < ButtonNum; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
				 
					break;
					
				case DisplayPan_Select:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < ButtonNum; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
				 
					break;
					
				case DisplayPan_Bttn1:  
			
					GetCtrlVal (display->displayPanHndl, control, &state);
				
					for(int i = 0; i < ButtonNum; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				 
					SetCtrlVal (display->displayPanHndl, control, !state);
					
					break;
					
				default:
					
					for(int i = 0; i < ButtonNum; i++) {
						SetCtrlVal(display->displayPanHndl, buttonArray[i], 0);
					}
				
					break;
					 
			 }
		break;
	}
	return 0;
}

static int CVICALLBACK DisplayPanCB (int panel, int event, void *callbackData, int eventData1, int eventData2) 
{  
	switch (event) { 
			
		case EVENT_CLOSE:
			
			HidePanel(panel);
			break;
	}
	
	return 0;
}

int CVICALLBACK CanvasPanelCB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	ImageDisplayCVI_type* 	display 		= (ImageDisplayCVI_type*) callbackData;
    static int 				mouseDown 		= 0;
	int    					state			= 0;
	
    static Point 			oldp;
    Point  					newp;
    static Rect 			oldr;
    static Rect				newr;
    
	GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state); 
	
    switch (event) {
			
        case EVENT_LEFT_CLICK:
			
			
			// if selection ROI button is toggled 
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
				
				// mark rectangle starting point
	            oldp = MakePoint (x, y);
				
				
				
				// draw all previous ROI's
				DrawROIs(display);
				
				int canvasBitmap;
				
				// update the ImgDisplay bitmap with the one with ROI's overlay, in order to avoid constantly redrawing the ROI's
				GetCtrlDisplayBitmap (display->canvasPanHndl, CanvasPan_Canvas, 0, &canvasBitmap);
				display->imgBitmapID = canvasBitmap;

			}
			
			// if ROI point button is toggled
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
				
				
				// default ROI color
				RGBA_type	color 		= {.R = Default_ROI_R_Color, .G = Default_ROI_G_Color, .B = Default_ROI_B_Color, .alpha = Default_ROI_A_Color}; 
				
				ROIlist = GetImageROIs(display->baseClass.image);
				
				// create new point ROI
				newROI = (ROI_type*)initalloc_Point_type(NULL, "", color, TRUE, x / display->zoom, y / display->zoom);
				
				newROI->ROIName = GetDefaultUniqueROIName(ROIlist); 
				
				Rect	textrect_point = { 	.top 		= y + ROILabel_YOffset,
						 			 	   	.left 		= x + ROILabel_XOffset,						
						 			 		.width 	= ROI_Label_Width,
						 			 		.height	= ROI_Label_Height	};
				
				
				// draw cross to mark point ROI
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(x - Cross_Length, y), MakePoint(x + Cross_Length, y));
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(x, y - Cross_Length), MakePoint(x, y + Cross_Length));
							   
				CanvasDrawText (display->canvasPanHndl, CanvasPan_Canvas, newROI->ROIName,VAL_APP_META_FONT,textrect_point, VAL_LOWER_LEFT);
				
				AddImageROI(display->baseClass.image, &newROI, NULL);   
			}
            break;
			
        case EVENT_LEFT_CLICK_UP:
			 
			// if selection button is toggled
			 GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state); 
   	         if(state) {	
				
				mouseDown = 0;
            	
				ListType	ROIlist		= 0;
				
				ROI_type*	newROI 		= NULL;
				
				RGBA_type	color 		= {.R = Default_ROI_R_Color, .G = Default_ROI_G_Color, .B = Default_ROI_B_Color, .alpha = Default_ROI_A_Color}; 
			
                SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_VISIBLE, 0);
                SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_WIDTH, 0);
                SetCtrlAttribute (display->canvasPanHndl, CanvasPan_SELECTION, ATTR_HEIGHT, 0);
				
				ROIlist = GetImageROIs(display->baseClass.image); 
				
				// transform coordinates to the base, no zoom level
				
				
				// save ROI based on zero magnification level
				int top  	= newr.top / display->zoom;
				int left 	= newr.left / display->zoom;
				int height 	= newr.height / display->zoom;
				int width 	= newr.width / display->zoom;
				
				newROI = (ROI_type*)initalloc_Rect_type(NULL, "", color, TRUE, top, left, height, width); 
				
				ROIlist = GetImageROIs(display->baseClass.image);
				
				newROI->ROIName = GetDefaultUniqueROIName(ROIlist);

				
				Rect	textrect = { .top 		= newr.top  + ROILabel_YOffset,
						 			 .left 		= newr.left + ROILabel_XOffset,
						 			 .width 	= ROI_Label_Width,
						 			 .height	= ROI_Label_Height	};
				
				CanvasDrawText (display->canvasPanHndl, CanvasPan_Canvas, newROI->ROIName,VAL_APP_META_FONT,textrect, VAL_LOWER_LEFT);   
				
				AddImageROI(display->baseClass.image, &newROI, NULL);

				CanvasDrawBitmap (display->canvasPanHndl, CanvasPan_Canvas, display->imgBitmapID, VAL_ENTIRE_OBJECT, VAL_ENTIRE_OBJECT);
				DrawROIs(display);
       	    }
			
            break;
			
        case EVENT_MOUSE_POINTER_MOVE:
			
			// if selection button is toggled
			GetCtrlVal (display->displayPanHndl, DisplayPan_RectROI, &state);
			
			if(state) {
				
				if(eventData2 > display->imgWidth || eventData1 > display->imgHeight)
					return 1;
				
				// if mouse down, we're currently drawing a rectangular selection
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
		    magnify = display->zoom;
			
			// in case current roi is a rectangle/selection
	   		if (ROI_iterr->ROIType == ROI_Rectangle) {
				
				Rect		rect = { .top 		= ((Rect_type*)ROI_iterr)->top * magnify,
						 			 .left 		= ((Rect_type*)ROI_iterr)->left * magnify,
						 			 .width 	= ((Rect_type*)ROI_iterr)->width * magnify,
						 			 .height	= ((Rect_type*)ROI_iterr)->height * magnify	};
				
				
				Rect	textrect = { .top 		= ((Rect_type*)ROI_iterr)->top * magnify + ROILabel_YOffset,
						 			 .left 		= ((Rect_type*)ROI_iterr)->left * magnify + ROILabel_XOffset,
						 			 .width 	= ROI_Label_Width,
						 			 .height	= ROI_Label_Height	};
				
				CanvasDrawRect (display->canvasPanHndl, CanvasPan_Canvas, rect, VAL_DRAW_FRAME);
				CanvasDrawText (display->canvasPanHndl, CanvasPan_Canvas, ROI_iterr->ROIName,VAL_APP_META_FONT,textrect, VAL_LOWER_LEFT);
			}
			
			// in case current roi is a point
			if (ROI_iterr->ROIType == ROI_Point) { 
			
				Point 		point = { .x = ((Point_type*)ROI_iterr)->x * magnify, 
								  	  .y = ((Point_type*)ROI_iterr)->y * magnify	};
			
				Rect	textrect_point = { 	.top 		= point.y + ROILabel_YOffset,
						 			 	   	.left 		= point.x + ROILabel_XOffset,
						 			 		.width 	= ROI_Label_Width,
						 			 		.height	= ROI_Label_Height	};
				
				
				// draw the cross that marks a ROI point
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(point.x - Cross_Length, point.y), MakePoint(point.x + Cross_Length, point.y));
				CanvasDrawLine(display->canvasPanHndl, CanvasPan_Canvas, MakePoint(point.x, point.y - Cross_Length), MakePoint(point.x, point.y + Cross_Length));
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

// callback triggered when "Save" button is clicked
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
}

// callback triggered when "Restore" button is clicked  
void CVICALLBACK MenuRestoreCB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle) {
		
		ImageDisplayCVI_type* 	display 		= (ImageDisplayCVI_type*) callbackData;     
		FireCallbackGroup(display->baseClass.callbackGroup, ImageDisplay_RestoreSettings, NULL); 
}
