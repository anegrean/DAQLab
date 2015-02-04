//==============================================================================
//
// Title:		NIVisionDisplay.c
// Purpose:		A short description of the implementation.
//
// Created on:	3-2-2015 at 17:41:58 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "windows.h"  			// include this first!
#include "NIVisionDisplay.h"
#include "DisplayEngine.h"
#include "toolbox.h"


//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  

#define		MAX_NI_VISION_DISPLAYS			16

//==============================================================================
// Types

typedef struct {
	NIVisionDisplay_type*	niDisplay;		
	void*					callbackData;   // callback data set by NewNIVisionDisplayHandle() 
	int						imaqWndID;		// assigned IMAQ window ID for display
	Image*					imaqImg;		// buffer to store the displayed image
} Display_type;

struct NIVisionDisplay {
	// BASE CLASS
	DisplayEngine_type		baseClass;
	
	// DATA
	intptr_t 				parentWindowHndl;
	
	
	// METHODS
	
	
};

//==============================================================================
// Static global variables

	// IMAQ tools setup
static ToolWindowOptions 		imaqTools 								= {	.showSelectionTool 			= TRUE,
																			.showZoomTool				= TRUE,
																			.showPointTool				= TRUE,
																			.showLineTool				= FALSE,
																			.showRectangleTool			= TRUE,
																			.showOvalTool				= FALSE,
																			.showPolygonTool			= FALSE,
																			.showClosedFreehandTool		= FALSE,
																			.showPolyLineTool			= FALSE,
																			.showFreehandTool			= FALSE,
																			.showAnnulusTool			= FALSE,
																			.showRotatedRectangleTool	= FALSE,
																			.showPanTool				= TRUE,
																			.showZoomOutTool			= TRUE,
																			.reserved2					= FALSE,
																			.reserved3					= FALSE,
																			.reserved4					= FALSE	   };

static 							imaqToolWindowVisibleFlag				= FALSE;

Display_type**					displays								= NULL;

int								nNIVisionDisplays						= 0;	// counts the number of NIVisionDisplay_type* instances

//==============================================================================
// Static functions

static Display_type*			init_Display_type								(NIVisionDisplay_type* NIDisplay, int imaqWndID, void* callbackData); 

static void 					discard_Display_type 							(Display_type** displayPtr);  

static void 					discard_NIVisionDisplay 						(NIVisionDisplay_type** niVisionDisplayPtr);

static int						DisplayNIVisionImage							(Display_type* displayHandle, void* pixelArray, int imgWidth, int imgHeight, ImageTypes imageType);

static DisplayHandle_type		GetNIVisionDisplayHandle						(NIVisionDisplay_type* NIDisplay, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType);

	// For receiving display callbacks such as placement of ROI
static void IMAQ_CALLBACK		Display_CB										(WindowEventType type, int windowNumber, Tool tool, Rect rect); 

//==============================================================================
// Global variables

//==============================================================================
// Global functions

NIVisionDisplay_type* init_NIVisionDisplay_type (intptr_t parentWindowHndl)
{
	int						error				= 0;
	intptr_t				imaqToolWndHndl		= 0; 
	NIVisionDisplay_type*	niVision 			= malloc(sizeof(NIVisionDisplay_type));
	
	if (!niVision) return NULL;
	
	//--------------------------
	// init base class
	//--------------------------
	init_DisplayEngine_type(&niVision->baseClass, (DiscardDisplayEngineFptr_type)discard_NIVisionDisplay, (DisplayImageFptr_type)DisplayNIVisionImage, (GetDisplayHandleFptr_type)GetNIVisionDisplayHandle);
	
	//--------------------------
	// init NI Vision Data
	//--------------------------
	
	// global display array shared by multiple init_NIVisionDisplay_type*
	if (!displays)
		nullChk( displays = calloc (MAX_NI_VISION_DISPLAYS, sizeof(Display_type*)) );
		
	// IMAQ tool window setup
	nullChk( imaqSetupToolWindow(TRUE, 2, &imaqTools) );
	nullChk( imaqSetCurrentTool(IMAQ_PAN_TOOL) );
	
	// confine window to parent if any
	niVision->parentWindowHndl = parentWindowHndl;
	if (parentWindowHndl) {
		nullChk( imaqToolWndHndl = (intptr_t) imaqGetToolWindowHandle() );
		SetParent( (HWND) imaqToolWndHndl, (HWND)parentWindowHndl);
	}
	
	// set up display callback
	errChk( imaqSetEventCallback(Display_CB, FALSE) ); 
	
	// increase instance count
	nNIVisionDisplays++;
	
	return niVision;
	
Error:
	
	OKfree(niVision);
	
	return NULL;
}

static void discard_NIVisionDisplay (NIVisionDisplay_type** niVisionDisplayPtr)
{
	if (!*niVisionDisplayPtr) return;
	
	// discard NI Vision Display specific data
	
	// if there are no more NI Vision Displays, then clean up display array
	if (!nNIVisionDisplays) {
		for (int i = 0; i < MAX_NI_VISION_DISPLAYS; i++)
			discard_Display_type(&displays[i]);
		OKfree(displays);
	}
	
	// discard Display Engine base class data
	discard_DisplayEngineClass((DisplayEngine_type**)niVisionDisplayPtr);
	
	nNIVisionDisplays--;
}

static Display_type* init_Display_type (NIVisionDisplay_type* NIDisplay, int imaqWndID, void* callbackData)
{
	Display_type*	disp = malloc(sizeof(Display_type));
	if (!disp) return NULL;
	
	disp->niDisplay 		= NIDisplay;
	disp->callbackData		= callbackData;
	disp->imaqWndID			= imaqWndID;
	disp->imaqImg			= NULL;
	
	return disp;
}

static void discard_Display_type (Display_type** displayPtr)
{
	if (!*displayPtr) return;
	
	if ((*displayPtr)->imaqImg) {
		imaqDispose((*displayPtr)->imaqImg);
		(*displayPtr)->imaqImg = NULL;
	}
	
	OKfree(*displayPtr);
}

static int DisplayNIVisionImage (Display_type* displayHandle, void* pixelArray, int imgHeight, int imgWidth, ImageTypes imageType)
{
	int		error		= 0;
	
	// display image
	nullChk( imaqArrayToImage(displayHandle->imaqImg, pixelArray, imgHeight, imgWidth) );
	
	nullChk( imaqDisplayImage(displayHandle->imaqImg, displayHandle->imaqWndID, FALSE) );
			 
	// display IMAQ tool window
	if (!imaqToolWindowVisibleFlag) {
		imaqShowToolWindow(TRUE);
		imaqToolWindowVisibleFlag = TRUE;
	}
	
	return 0;
	
Error:
	
	return error;
}

static DisplayHandle_type GetNIVisionDisplayHandle (NIVisionDisplay_type* NIDisplay, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType)
{
	int				imaqHndl		= 0;
	int				error			= 0;
	ImageType		imaqImgType		= 0;
	
	// get IMAQ window handle
	if (!imaqGetWindowHandle(&imaqHndl)) return NULL;
	
	// assign data structure
	discard_Display_type(&displays[imaqHndl]);
	nullChk(displays[imaqHndl] = init_Display_type(NIDisplay, imaqHndl, callbackData) );
	
	// init IMAQ image buffer 
	switch (imageType) {
		case Image_UChar:
			imaqImgType 		= IMAQ_IMAGE_U8;
			break;
		
		case Image_UShort:
			imaqImgType 		= IMAQ_IMAGE_U16;
			break;
			
		case Image_Short:
			imaqImgType 		= IMAQ_IMAGE_I16; 
			break;
			
		case Image_Float:
			imaqImgType 		= IMAQ_IMAGE_SGL; 
			break;
			
		default:
			
			goto Error;
	}
	
	nullChk( displays[imaqHndl]->imaqImg = imaqCreateImage(imaqImgType, 0) );
	// pre allocate image memory
	nullChk( imaqSetImageSize(displays[imaqHndl]->imaqImg, imgWidth, imgHeight) );
	
	// confine image window to parent window if provided
	
	if (NIDisplay->parentWindowHndl) {
		intptr_t	imaqWndHandle	= (intptr_t) imaqGetSystemWindowHandle(imaqHndl); 
		SetParent( (HWND)imaqWndHandle, (HWND)NIDisplay->parentWindowHndl);    
	}
	
	return displays[imaqHndl];
	
Error:
	
	// cleanup
	discard_Display_type(&displays[imaqHndl]);
	
	return NULL;
}

static void IMAQ_CALLBACK Display_CB (WindowEventType event, int windowNumber, Tool tool, Rect rect)
{
	
	/*
	switch (event) {
			
		
			
	}
	*/
}

void discard_Image_type (Image** image)
{
	if (!*image) return;
	
	imaqDispose(*image); 
	
	*image = NULL;
}
