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
	void*					callbackData;   // Callback data set by NewNIVisionDisplayHandle() 
	int						imaqWndID;		// Assigned IMAQ window ID for display
	Image*					imaqImg;		// Buffer to store the displayed image
	//ListType				ROIs;			// List of ROIs added to imaqImg of ROI_type*
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

static int						DisplayNIVisionImage							(Display_type* displayHndl, void* pixelArray, int imgWidth, int imgHeight, ImageTypes imageType);

static DisplayHandle_type		GetNIVisionDisplayHandle						(NIVisionDisplay_type* NIDisplay, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType);

static int						OverlayNIVisionROI								(Display_type* displayHndl, ROI_type* ROI);

static void						ClearAllROI										(Display_type* displayHndl);


	// For receiving NI Vision callbacks such as placement of ROI
static void IMAQ_CALLBACK		Display_CB										(WindowEventType type, int windowNumber, Tool tool, Rect rect); 

//==============================================================================
// Global variables

//==============================================================================
// Global functions

NIVisionDisplay_type* init_NIVisionDisplay_type (	intptr_t 					parentWindowHndl,
													ROIPlaced_CBFptr_type		ROIPlacedCBFptr,
													ROIRemoved_CBFptr_type		ROIRemovedCBFptr		)
{
	int						error				= 0;
	intptr_t				imaqToolWndHndl		= 0; 
	NIVisionDisplay_type*	niVision 			= malloc(sizeof(NIVisionDisplay_type));
	
	if (!niVision) return NULL;
	
	//--------------------------
	// init base class
	//--------------------------
	init_DisplayEngine_type(	&niVision->baseClass, 
								(DiscardDisplayEngineFptr_type) discard_NIVisionDisplay, 
								(DisplayImageFptr_type) DisplayNIVisionImage, 
								(GetDisplayHandleFptr_type) GetNIVisionDisplayHandle,
								(OverlayROIFptr_type) OverlayNIVisionROI,
								(ClearAllROIFptr_type) ClearAllROI,
								ROIPlacedCBFptr,
								ROIRemovedCBFptr											);
	
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
	
	// init
	disp->niDisplay 		= NIDisplay;
	disp->callbackData		= callbackData;
	disp->imaqWndID			= imaqWndID;
	disp->imaqImg			= NULL;
	//disp->ROIs				= 0;
	
	// allocate
	//if ( !(disp->ROIs		= ListCreate(sizeof(ROI_type*))) ) 		goto Error;
	
	return disp;
	
//Error:

	//discard_Display_type(&disp);
	//return NULL;
}

static void discard_Display_type (Display_type** displayPtr)
{
	Display_type*	display = *displayPtr;
	
	if (!display) return;
	
	// NI Vision image
	if (display->imaqImg) {
		imaqDispose(display->imaqImg);
		display->imaqImg = NULL;
	}
	
	// ROIs
	/*
	if (display->ROIs) {
		size_t		nROIs = ListNumItems(display->ROIs);
		ROI_type**  ROIPtr;
		for (size_t i = 1; i <= nROIs; i++) {
			ROIPtr = ListGetPtrToItem(display->ROIs, i);
			discard_ROI_type(ROIPtr);
		}
		ListDispose(display->ROIs);
		display->ROIs = 0;
	}
	*/
	
	OKfree(*displayPtr);
}

static int DisplayNIVisionImage (Display_type* displayHndl, void* pixelArray, int imgHeight, int imgWidth, ImageTypes imageType)
{
	int		error		= 0;
	
	// display image
	nullChk( imaqArrayToImage(displayHndl->imaqImg, pixelArray, imgHeight, imgWidth) );
	
	nullChk( imaqDisplayImage(displayHndl->imaqImg, displayHndl->imaqWndID, FALSE) );
			 
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

static int OverlayNIVisionROI (Display_type* displayHndl, ROI_type* ROI)
{
	int					error 				= 0;
	RGBValue			rgbVal 				= { .R		= ROI->rgba.R,
												.G		= ROI->rgba.G,
												.B		= ROI->rgba.B,
												.alpha	= ROI->rgba.A	};
	
	OverlayTextOptions	overlayTextOptions	= {	.fontName					="Arial",
						 						.fontSize					= 12,
												.bold						= FALSE,
												.italic						= FALSE,
												.underline					= FALSE,
												.strikeout					= FALSE,
												.horizontalTextAlignment	= IMAQ_LEFT,
												.verticalTextAlignment		= IMAQ_TOP,
												.backgroundColor			= {0,0,0,0},
												.angle						= 0				};
	
	// overlay ROI on the image
	switch (ROI->ROIType) {
			
		case ROI_Point:
			
			Point 		point = { .x = ((Point_type*)ROI)->x, 
								  .y = ((Point_type*)ROI)->y	};
			
			// point overlay 			
			nullChk( imaqOverlayPoints(displayHndl->imaqImg, &point, 1, &rgbVal, 1, IMAQ_POINT_AS_CROSS, NULL, ROI->ROIName) );   
			// label overlay
			nullChk( imaqOverlayText(displayHndl->imaqImg, point, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			
			break;
			
		case ROI_Rectangle:
			
			Rect		rect = { .top 		= ((Rect_type*)ROI)->top,
								 .left 		= ((Rect_type*)ROI)->left,
								 .width 	= ((Rect_type*)ROI)->width,
								 .height	= ((Rect_type*)ROI)->height	};
			
			Point		textPoint = { .x = rect.left, .y = rect.top };
			
			// rectangle overlay
			nullChk( imaqOverlayRect(displayHndl->imaqImg, rect, &rgbVal, IMAQ_DRAW_VALUE, ROI->ROIName) );
			// label overlay
			nullChk( imaqOverlayText(displayHndl->imaqImg, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			
			break;
			
	}
	
	// add ROI overlay to list
	//ListInsertItem(displayHndl->ROIs, &ROI, END_OF_LIST);
	
	return 0;
	
Error:
	
	return error;
}

static void ClearAllROI (Display_type* displayHndl)
{
	imaqClearOverlay(displayHndl->imaqImg, NULL);
}

static void IMAQ_CALLBACK Display_CB (WindowEventType event, int windowNumber, Tool tool, Rect rect)
{
	Display_type*	display = displays[windowNumber];
	
	switch (event) {
			
		case IMAQ_CLICK_EVENT:
			
			switch (tool) {
					
				case IMAQ_SELECTION_TOOL:
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case IMAQ_DOUBLE_CLICK_EVENT:
			
			switch (tool) {
					
				case IMAQ_SELECTION_TOOL:
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case IMAQ_DRAW_EVENT:
			
			switch (tool) {
					
				case IMAQ_POINT_TOOL:
					
					Point_type*	PointROI =  init_Point_type(NULL, rect.left, rect.top);
					
					// execute callback
					if (display->niDisplay->baseClass.ROIPlacedCBFptr)
						(*display->niDisplay->baseClass.ROIPlacedCBFptr) (display, display->callbackData, (ROI_type**) &PointROI);
					
					
					break;
					
				case IMAQ_LINE_TOOL:
					
					break;
					
				case IMAQ_RECTANGLE_TOOL:
					
					Rect_type*	RectROI =  init_Rect_type(NULL, rect.top, rect.left, rect.height, rect.width);
					
					// execute callback
					if (display->niDisplay->baseClass.ROIPlacedCBFptr)
						(*display->niDisplay->baseClass.ROIPlacedCBFptr) (display, display->callbackData, (ROI_type**) &RectROI);
					
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case IMAQ_ACTIVATE_EVENT:
			
			break;
			
		case IMAQ_CLOSE_EVENT:
			
			break;
			
	
			
		default:
			
			break;
	}
	
}

void discard_Image_type (Image** image)
{
	if (!*image) return;
	
	imaqDispose(*image); 
	
	*image = NULL;
}
