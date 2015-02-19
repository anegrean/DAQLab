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
#include "Commctrl.h"
#include "NIVisionDisplay.h"
#include "DisplayEngine.h"
#include "toolbox.h"


//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  

#define		MAX_NI_VISION_DISPLAYS			16

//==============================================================================
// Types

typedef enum {
	NIDisplayMenu_Save,
	NIDisplayMenu_Restore
} NIDisplayMenuItems;

typedef struct {
	NIVisionDisplay_type*				niDisplay;		
	void*								callbackData;   // Callback data set by NewNIVisionDisplayHandle() 
	int									imaqWndID;		// Assigned IMAQ window ID for display
	Image*								imaqImg;		// Buffer to store the displayed image
	//ListType							ROIs;			// List of ROIs added to imaqImg of ROI_type*
	LONG_PTR							imaqWndProc;	// Pointer to the original imaq window procedure. This will be called after the custom window procedure
	
	// callbacks to restore image settings to the various modules
	size_t 								nRestoreSettingsCBs;
	RestoreImgSettings_CBFptr_type* 	restoreSettingsCBs;
	void** 								restoreSettingsCBsData;
	DiscardFptr_type* 					discardCallbackDataFunctions;
} Display_type;

struct NIVisionDisplay {
	// BASE CLASS
	DisplayEngine_type					baseClass;
	
	// DATA
	intptr_t 							parentWindowHndl;
	
	
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

static Display_type*			GetNIVisionDisplayHandle						(NIVisionDisplay_type* NIDisplay, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType);

static void*					GetDisplayHandleCBData							(Display_type* displayHndl);

static int						SetRestoreImgSettingsCBs						(Display_type* displayHndl, size_t nCallbackFunctions, RestoreImgSettings_CBFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions);

static int						OverlayNIVisionROI								(Display_type* displayHndl, ROI_type* ROI);

static void						ClearAllROI										(Display_type* displayHndl);


	// For receiving NI Vision callbacks such as placement of ROI
static void IMAQ_CALLBACK		Display_CB										(WindowEventType type, int windowNumber, Tool tool, Rect rect); 

	// For processing custom NI Vision window events
LRESULT CALLBACK 				CustomDisplay_CB 								(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

NIVisionDisplay_type* init_NIVisionDisplay_type (	intptr_t 					parentWindowHndl,
													ROIPlaced_CBFptr_type		ROIPlacedCBFptr,
													ROIRemoved_CBFptr_type		ROIRemovedCBFptr,
													ErrorHandlerFptr_type		errorHandlerCBFptr	)
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
								(GetDisplayHandleCBDataFptr_type) GetDisplayHandleCBData,
								(SetRestoreImgSettingsCBsFptr_type) SetRestoreImgSettingsCBs,
								(OverlayROIFptr_type) OverlayNIVisionROI,
								(ClearAllROIFptr_type) ClearAllROI,
								ROIPlacedCBFptr,
								ROIRemovedCBFptr,
						   		errorHandlerCBFptr);
	
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
	disp->niDisplay 					= NIDisplay;
	disp->callbackData					= callbackData;
	disp->imaqWndID						= imaqWndID;
	disp->imaqImg						= NULL;
	disp->imaqWndProc					= 0;
	//disp->ROIs						= 0;
	
	// allocate
	//if ( !(disp->ROIs		= ListCreate(sizeof(ROI_type*))) ) 		goto Error;
	
	// callbacks to restore image settings back to the modules that determine the image (scan engine, xyz stages, etc)
	disp->nRestoreSettingsCBs			= 0;
	disp->restoreSettingsCBs			= NULL;
	disp->restoreSettingsCBsData		= NULL;
	disp->discardCallbackDataFunctions  = NULL;
	
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
	
	// restore settings callback data
	for (size_t i = 0; i < display->nRestoreSettingsCBs; i++)
		if (display->discardCallbackDataFunctions[i])
			(*display->discardCallbackDataFunctions[i]) (&display->restoreSettingsCBsData[i]);	
		else
			OKfree(display->restoreSettingsCBsData[i]);
	
	display->nRestoreSettingsCBs = 0;
	OKfree(display->discardCallbackDataFunctions);
	OKfree(display->restoreSettingsCBs);
	OKfree(display->restoreSettingsCBsData);
	
	
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

static Display_type* GetNIVisionDisplayHandle (NIVisionDisplay_type* NIDisplay, void* callbackData, int imgWidth, int imgHeight, ImageTypes imageType)
{
	int				imaqHndl				= 0;	// imaq window ID
	int				error					= 0;
	ImageType		imaqImgType				= 0;
	HWND			imaqWndHndl				= 0;	// windows handle of the imaq window
	HMENU			imaqWndMenuHndl			= 0;	// menu bar for the imaq window
	
	// get IMAQ window handle
	if (!imaqGetWindowHandle(&imaqHndl)) return NULL;
	// check if an available imaq window handle was obtained
	if (imaqHndl < 0) return NULL;
	// get windows window handle
	imaqWndHndl	= (HWND) imaqGetSystemWindowHandle(imaqHndl); 
	
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
	if (NIDisplay->parentWindowHndl)
		SetParent( imaqWndHndl, (HWND)NIDisplay->parentWindowHndl);
	// create menu bar
	imaqWndMenuHndl = CreateMenu();
	// add menu item "Save"
	AppendMenu(imaqWndMenuHndl, MF_STRING, NIDisplayMenu_Save, "&Save");
	// add menu item "Restore"
	AppendMenu(imaqWndMenuHndl, MF_STRING, NIDisplayMenu_Restore, "&Restore");
	
	// add menu bar to the imaq window
	SetMenu(imaqWndHndl, imaqWndMenuHndl);
	
	// add custom window callback function
	SetWindowSubclass(imaqWndHndl, CustomDisplay_CB, 0, (DWORD_PTR)displays[imaqHndl]);
	
	
	return displays[imaqHndl];
	
Error:
	
	// cleanup
	discard_Display_type(&displays[imaqHndl]);
	
	return NULL;
}

static void* GetDisplayHandleCBData (Display_type* displayHndl)
{
	return displayHndl->callbackData;
}

static int SetRestoreImgSettingsCBs (Display_type* displayHndl, size_t nCallbackFunctions, RestoreImgSettings_CBFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions)
{
	int error = 0;
	
	// discard restore settings callback data
	for (size_t i = 0; i < displayHndl->nRestoreSettingsCBs; i++)
		if (displayHndl->discardCallbackDataFunctions[i])
			(*displayHndl->discardCallbackDataFunctions[i]) (&displayHndl->restoreSettingsCBsData[i]);	
		else
			OKfree(displayHndl->restoreSettingsCBsData[i]);
	
	OKfree(displayHndl->restoreSettingsCBs);
	OKfree(displayHndl->restoreSettingsCBsData); 
	OKfree(displayHndl->discardCallbackDataFunctions);

	displayHndl->nRestoreSettingsCBs = 0;
	
	// allocate restore settings data
	displayHndl->nRestoreSettingsCBs = nCallbackFunctions;
	
	nullChk( displayHndl->discardCallbackDataFunctions 	= malloc (nCallbackFunctions*sizeof(DiscardFptr_type)) );
	nullChk( displayHndl->restoreSettingsCBs 			= malloc (nCallbackFunctions*sizeof(RestoreImgSettings_CBFptr_type)) );
	nullChk( displayHndl->restoreSettingsCBsData 		= malloc (nCallbackFunctions*sizeof(void*)) );
	
	for (size_t i = 0; i < nCallbackFunctions; i++) {
		displayHndl->restoreSettingsCBs[i] 				= callbackFunctions[i];
		displayHndl->restoreSettingsCBsData[i]			= callbackData[i];
		displayHndl->discardCallbackDataFunctions[i] 	= discardCallbackDataFunctions[i];
	}
	
	return 0;
	
Error: 
	
	OKfree(displayHndl->discardCallbackDataFunctions);
	OKfree(displayHndl->restoreSettingsCBs);
	OKfree(displayHndl->restoreSettingsCBsData);
	
	return error;
	
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
					
				case IMAQ_POINT_TOOL:
					
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

LRESULT CALLBACK CustomDisplay_CB (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{   
	Display_type*	disp 			= (Display_type*) dwRefData;
	WORD			wParamHighWord	= HIWORD(wParam);
	WORD			wParamLowWord	= LOWORD(wParam);
	int				error			= 0;
	char*			errMsg			= NULL;
	
	switch (msg) {
			
		case WM_COMMAND:
			
			// filter menu events (wParamHighWord = 0)
			if (wParamHighWord) break;
			
			switch (wParamLowWord) {
					
				case NIDisplayMenu_Save:
					
					break;
					
				case NIDisplayMenu_Restore:
					
					// process each restore function sequencially within this thread
					for (size_t i = 0; i < disp->nRestoreSettingsCBs; i++)
						errChk( (*disp->restoreSettingsCBs[i]) ((DisplayEngine_type*)disp->niDisplay, (DisplayHandle_type)disp, disp->restoreSettingsCBsData[i], &errMsg) );
						
					break;
			}
			
			
			break;
			
		default:
			break;
	}
	
	return DefSubclassProc(hWnd, msg, wParam, lParam);
	
Error:
	
	// call display error handler
	(*disp->niDisplay->baseClass.errorHandlerCBFptr) ((DisplayHandle_type)disp->niDisplay, error, errMsg);
	OKfree(errMsg);
	
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}


void discard_Image_type (Image** image)
{
	if (!*image) return;
	
	imaqDispose(*image); 
	
	*image = NULL;
}
