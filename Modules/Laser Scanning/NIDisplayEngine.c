//==============================================================================
//
// Title:		NIDisplayEngine.c
// Purpose:		A short description of the implementation.
//
// Created on:	3-2-2015 at 17:41:58 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "windows.h"  			// include this first!
#include <formatio.h>
#include "Commctrl.h"
#include "NIDisplayEngine.h"
#include "DisplayEngine.h"
#include "toolbox.h"
#include "DataStorage.h"



//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  

#define	MAX_NI_VISION_DISPLAYS				16

#define ROILabel_XOffset					3		// ROI text label offset in the X direction in pixels.
#define ROILabel_YOffset					3		// ROI text label offset in the Y direction in pixels.

// Default image display settings
#define Default_Max_Zoom					3.0		// Maximum zoom factor applied to the image to try to fill the target display area
#define Default_WorkspaceScaling			0.5		// Factor that multiplies the workspace area yielding the display area which should not be exceeded by the displayed window

//==============================================================================
// Types

typedef enum {
	NIDisplayMenu_Save,
	NIDisplayMenu_Restore
} NIDisplayMenuItems;

struct NIDisplayEngine {
	// BASE CLASS
	DisplayEngine_type					baseClass;
	
	// DATA
	intptr_t 							parentWindowHndl;
	
	// METHODS
	
};

struct NIImageDisplay {
	// BASE CLASS
	ImageDisplay_type					baseClass;
	
	// DATA
	Image*								NIImage;		// NI Image displayed in the window
	int									imaqWndID;		// Assigned IMAQ window ID for display
	LONG_PTR							imaqWndProc;	// Pointer to the original imaq window procedure. This will be called after the custom window procedure
	
	// METHODS
};

//==============================================================================
// Static global variables

	// IMAQ tools setup
static ToolWindowOptions 		imaqTools 								= {	.showSelectionTool 			= TRUE,
																			.showZoomTool				= TRUE,
																			.showPointTool				= FALSE,
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

NIImageDisplay_type**			displays								= NULL;

int								nNIDisplayEngines						= 0;	// counts the number of NIDisplayEngine_type instances

//==============================================================================
// Static functions

static NIImageDisplay_type*		init_NIImageDisplay_type						(NIDisplayEngine_type* displayEngine, int imaqWndID, void* callbackData); 

static void 					discard_NIImageDisplay_type 					(NIImageDisplay_type** displayPtr);  

static void 					discard_NIDisplayEngine_type 					(NIDisplayEngine_type** niVisionDisplayPtr);

static int						DisplayNIVisionImage							(NIImageDisplay_type* imgDisplay, Image_type** image);

static int						DisplayNIVisionRGBImage							(NIImageDisplay_type* imgDisplay, Image_type** imageR, Image_type** imageG, Image_type** imageB);	

	// displays a file selection popup-box and saves a given NI image as two grayscale TIFF files with ZIP compression with and without ROI flattened
static int 						ImageSavePopup 									(Image* image, char** errorInfo);

static NIImageDisplay_type*		GetNIImageDisplay								(NIDisplayEngine_type* NIDisplay, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType);

static ImgDisplayCBData_type	GetNIImageDisplayCBData							(NIImageDisplay_type* imgDisplay);

static int 						DrawROI 										(NIImageDisplay_type* imgDisplay, ROI_type* ROI);

static ROI_type*				OverlayNIVisionROI								(NIImageDisplay_type* imgDisplay, ROI_type* ROI);

static void						NIVisionROIActions								(NIImageDisplay_type* imgDisplay, int ROIIdx, ROIActions action);

static void						DiscardImaqImg									(Image** image);


	// For receiving NI Vision callbacks such as placement of ROI
static void IMAQ_CALLBACK		NIImageDisplay_CB								(WindowEventType type, int windowNumber, Tool tool, Rect rect); 

	// For processing custom NI Vision window events
LRESULT CALLBACK 				CustomNIImageDisplay_CB							(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

NIDisplayEngine_type* init_NIDisplayEngine_type (	intptr_t 					parentWindowHndl,
													ROIEvents_CBFptr_type		ROIEventsCBFptr,
													ErrorHandlerFptr_type		errorHandlerCBFptr	)
{
	int						error				= 0;
	NIDisplayEngine_type*	niVision 			= malloc(sizeof(NIDisplayEngine_type));
	
	if (!niVision) return NULL;
	
	//--------------------------
	// init base class
	//--------------------------
	init_DisplayEngine_type(	&niVision->baseClass, 
								(DiscardFptr_type) discard_NIDisplayEngine_type, 
								(DisplayImageFptr_type) DisplayNIVisionImage,
								(DisplayRGBImageFptr_type) DisplayNIVisionRGBImage,
								(DiscardFptr_type) DiscardImaqImg,
								(GetImageDisplayFptr_type) GetNIImageDisplay,
								(GetImageDisplayCBDataFptr_type) GetNIImageDisplayCBData,
								(OverlayROIFptr_type) OverlayNIVisionROI,
								(ROIActionsFptr_type) NIVisionROIActions,
								ROIEventsCBFptr,
						   		errorHandlerCBFptr);
	
	//--------------------------
	// init NI Vision Data
	//--------------------------
	
	// global display array shared by multiple init_NIDisplayEngine_type*
	if (!displays)
		nullChk( displays = calloc (MAX_NI_VISION_DISPLAYS, sizeof(NIImageDisplay_type*)) );
		
	// IMAQ tool window setup
	nullChk( imaqSetupToolWindow(TRUE, 2, &imaqTools) );
	nullChk( imaqSetCurrentTool(IMAQ_PAN_TOOL) );
	imaqGetToolWindowHandle();
	
	// confine window to parent if any
	niVision->parentWindowHndl = parentWindowHndl;
	//if (parentWindowHndl) {
	//	nullChk( imaqToolWndHndl = (intptr_t) imaqGetToolWindowHandle() );
	//	SetParent( (HWND) imaqToolWndHndl, (HWND)parentWindowHndl);
	//}
	
	// set up display callback
	errChk( imaqSetEventCallback(NIImageDisplay_CB, FALSE) ); 
	
	// increase instance count
	nNIDisplayEngines++;
	
	return niVision;
	
Error:
	
	OKfree(niVision);
	
	return NULL;
}

static void discard_NIDisplayEngine_type (NIDisplayEngine_type** niVisionDisplayPtr)
{
	if (!*niVisionDisplayPtr) return;
	
	// discard NI Vision Display specific data
	imaqSetEventCallback(NULL, FALSE); // make sure there are no more callbacks when the window is disposed 
	
	OKfree(displays);
	
	// discard Display Engine base class data
	discard_DisplayEngineClass((DisplayEngine_type**)niVisionDisplayPtr);
	
	nNIDisplayEngines--;
}

static NIImageDisplay_type* init_NIImageDisplay_type (NIDisplayEngine_type* displayEngine, int imaqWndID, void* callbackData)
{
	int						error 		= 0;
	NIImageDisplay_type*	niImgDisp 	= malloc(sizeof(NIImageDisplay_type));
	if (!niImgDisp) return NULL;
	
	// init base class
	errChk( init_ImageDisplay_type(	&niImgDisp->baseClass, 
									(DiscardFptr_type) discard_NIImageDisplay_type,
									(DisplayEngine_type*) displayEngine,
									(ImgDisplayCBData_type) callbackData ) );
	
	// init data
	niImgDisp->NIImage					= NULL;
	niImgDisp->imaqWndID				= imaqWndID;
	niImgDisp->imaqWndProc				= 0;
	
	return niImgDisp;
	
Error:

	discard_NIImageDisplay_type(&niImgDisp);
	return NULL;
}

static void discard_NIImageDisplay_type (NIImageDisplay_type** displayPtr)
{
	NIImageDisplay_type*	display = *displayPtr;
	
	if (!display) return;
	
	//---------------------------------------------------
	// discard child class data
	
	// discard NI image
	DiscardImaqImg((Image**)&display->NIImage);

	//---------------------------------------------------
	// discard parent class
	discard_ImageDisplay_type((ImageDisplay_type**) displayPtr);
}

static int DisplayNIVisionImage (NIImageDisplay_type* imgDisplay, Image_type** image)
{								
	int			error		= 0;
	int			imgWidth	= 0;
	int			imgHeight	= 0;
	void*		pixelArray	= GetImagePixelArray(*image);
	
	// update display image
	discard_Image_type(&imgDisplay->baseClass.image);
	GetImageSize(*image, &imgWidth, &imgHeight);
	imgDisplay->baseClass.image = *image;
	*image = NULL;
	
	// convert pixel array to image
	nullChk( imaqArrayToImage(imgDisplay->NIImage, pixelArray, imgWidth, imgHeight) );
	
	// draw ROIs
	ListType	ROIList	= GetImageROIs(imgDisplay->baseClass.image);
	size_t		nROIs	= ListNumItems(ROIList);
	ROI_type*	ROI 	= NULL;
	
	for (size_t i = 1; i <= nROIs; i++) {
		ROI = *(ROI_type**) ListGetPtrToItem(ROIList, i);
		if (ROI->active)
			errChk( DrawROI(imgDisplay, ROI) );
	}
	
	// display NI image
	nullChk( imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE) );
			 
	// display IMAQ tool window
	int		isToolWindowVisible = 0;
	nullChk( imaqIsToolWindowVisible(&isToolWindowVisible) );
	if (!isToolWindowVisible)
		nullChk( imaqShowToolWindow(TRUE) );
		    
	return 0;
	
Error:
	
	return error;
}

static int DisplayNIVisionRGBImage (NIImageDisplay_type* imgDisplay, Image_type** imageR, Image_type** imageG, Image_type** imageB)
{
	
	return 0;
}

static NIImageDisplay_type* GetNIImageDisplay (NIDisplayEngine_type* NIDisplay, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType)
{
	int				imaqHndl				= 0;	// imaq window ID
	int				error					= 0;
	ImageType		imaqImgType				= 0;
	HWND			imaqWndHndl				= 0;	// windows handle of the imaq window
	HMENU			imaqWndMenuHndl			= 0;	// menu bar for the imaq window
	HMONITOR		hMonitor				= NULL;
	MONITORINFO		monitorInfo				= {.cbSize = sizeof(MONITORINFO)};
	LONG			maxWindowHeight			= 0;
	LONG			maxWindowWidth			= 0;
	float			imgZoom					= 0;	// final zoom factor that will be applied, not exceeding Default_Max_Zoom
	float			heightZoom				= 0; 	// zoom factor needed to match imgHeight to maxWindowHeight*Default_WorkspaceScaling
	float			widthZoom				= 0;	// zoom factor needed to match imgWidth to maxWindowWidth*Default_WorkspaceScaling
	
	// get IMAQ window handle
	if (!imaqGetWindowHandle(&imaqHndl)) return NULL;
	// check if an available imaq window handle was obtained
	if (imaqHndl < 0) return NULL;
	// get windows window handle
	nullChk( imaqWndHndl	= (HWND) imaqGetSystemWindowHandle(imaqHndl) ); 
	
	//-----------------------------------------------------------
	// adjust window size and zoom based on the screen dimensions
	//-----------------------------------------------------------
	hMonitor = MonitorFromWindow(imaqWndHndl, MONITOR_DEFAULTTONEAREST);
	if (hMonitor) {
		nullChk( GetMonitorInfo(hMonitor, &monitorInfo) );
		maxWindowHeight = labs(monitorInfo.rcWork.top - monitorInfo.rcWork.bottom);
		heightZoom = (float)maxWindowHeight*(float)Default_WorkspaceScaling/(float)imgHeight;
		maxWindowWidth = labs(monitorInfo.rcWork.left - monitorInfo.rcWork.right);
		widthZoom = (float)maxWindowWidth*(float)Default_WorkspaceScaling/(float)imgWidth;
		
		// pick smallest zoom factor so that the zoomed in image does not exceed the boundaries set by Default_WorkspaceScaling
		imgZoom = min(min(heightZoom, widthZoom), Default_Max_Zoom);
		
		// adjust image zoom
		nullChk( imaqZoomWindow2(imaqHndl, imgZoom, imgZoom, IMAQ_NO_POINT) );
		
		// adjust window size to match displayed image size
		RECT	targetWindowSize	 	= {.left = 0, .top = 0, .right = (LONG)((float)imgWidth*imgZoom), .bottom = (LONG)((float)imgHeight*imgZoom)};
		DWORD	imaqWindowStyle			= (DWORD)GetWindowLong(imaqWndHndl, GWL_STYLE);
		
		nullChk( AdjustWindowRect(&targetWindowSize, imaqWindowStyle, TRUE) );
		nullChk( imaqSetWindowSize(imaqHndl, abs(targetWindowSize.left - targetWindowSize.right), abs(targetWindowSize.top - targetWindowSize.bottom)) );
		
	}
	
	// assign data structure
	nullChk(displays[imaqHndl] = init_NIImageDisplay_type(NIDisplay, imaqHndl, callbackData) );
	
	// set window on top
	nullChk( SetWindowPos(imaqWndHndl, HWND_TOPMOST, 0, 0, 0, 0, (SWP_ASYNCWINDOWPOS | SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE)) );
	
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
			
		case Image_UInt:
			imaqImgType 		= IMAQ_IMAGE_SGL;	 // no U32 support in IMAQ, image needs to be displayed as a float
			break;
			
		case Image_Int:
			imaqImgType 		= IMAQ_IMAGE_SGL;	 // no I32 support in IMAQ  image needs to be displayed as a float 
			break;
			
		case Image_Float:
			imaqImgType 		= IMAQ_IMAGE_SGL; 
			break;
			
		case Image_RGBA:
			imaqImgType 		= IMAQ_IMAGE_RGB; 
			break;
			
		case Image_RGBAU64:
			imaqImgType 		= IMAQ_IMAGE_RGB_U64; 
			break;
			
		default:
			
			goto Error;
	}
	
	nullChk( displays[imaqHndl]->NIImage = imaqCreateImage(imaqImgType, 0) );
	// pre allocate image memory
	nullChk( imaqSetImageSize(displays[imaqHndl]->NIImage, imgWidth, imgHeight) );
	// confine image window to parent window if provided  ( sometimes this freezes the UI!! )
	//if (NIDisplay->parentWindowHndl)
	//	SetParent( imaqWndHndl, (HWND)NIDisplay->parentWindowHndl);
	// create menu bar
	imaqWndMenuHndl = CreateMenu();
	// add menu item "Save"
	AppendMenu(imaqWndMenuHndl, MF_STRING, NIDisplayMenu_Save, "&Save");
	// add menu item "Restore"
	AppendMenu(imaqWndMenuHndl, MF_STRING, NIDisplayMenu_Restore, "&Restore");
	
	// add menu bar to the imaq window
	SetMenu(imaqWndHndl, imaqWndMenuHndl);
	
	// add custom window callback function
	SetWindowSubclass(imaqWndHndl, CustomNIImageDisplay_CB, 0, (DWORD_PTR)displays[imaqHndl]);
	
	
	return displays[imaqHndl];
	
Error:
	
	// cleanup
	discard_NIImageDisplay_type(&displays[imaqHndl]);
	
	return NULL;
}

static ImgDisplayCBData_type GetNIImageDisplayCBData (NIImageDisplay_type* imgDisplay)
{
	return imgDisplay->baseClass.imageDisplayCBData;
}

static int DrawROI (NIImageDisplay_type* imgDisplay, ROI_type* ROI)
{
	int					error 				= 0;
	RGBValue			rgbVal 				= { .R		= ROI->rgba.R,
												.G		= ROI->rgba.G,
												.B		= ROI->rgba.B,
												.alpha	= ROI->rgba.alpha };
	Point				textPoint;
	
	OverlayTextOptions	overlayTextOptions	= {	.fontName					="Arial",
						 						.fontSize					= imgDisplay->baseClass.ROITextFontSize,
												.bold						= FALSE,
												.italic						= FALSE,
												.underline					= FALSE,
												.strikeout					= FALSE,
												.horizontalTextAlignment	= IMAQ_LEFT,
												.verticalTextAlignment		= IMAQ_TOP,
												.backgroundColor			= {imgDisplay->baseClass.ROITextBackground.R,
														  					   imgDisplay->baseClass.ROITextBackground.G,
																			   imgDisplay->baseClass.ROITextBackground.B,
																			   imgDisplay->baseClass.ROITextBackground.alpha},
												.angle						= 0				};
	
	// overlay ROI on the image
	switch (ROI->ROIType) {
			
		case ROI_Point:
			
			Point 		point = { .x = ((Point_type*)ROI)->x, 
								  .y = ((Point_type*)ROI)->y	};
			
			textPoint.x = point.x + ROILabel_XOffset;
			textPoint.y = point.y + ROILabel_YOffset;
			
			// point overlay 
			nullChk( imaqOverlayPoints(imgDisplay->NIImage, &point, 1, &rgbVal, 1, IMAQ_POINT_AS_CROSS, NULL, ROI->ROIName) );   
			// label overlay
			nullChk( imaqOverlayText(imgDisplay->NIImage, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			break;
			
		case ROI_Rectangle:
			
			Rect		rect = { .top 		= ((Rect_type*)ROI)->top,
								 .left 		= ((Rect_type*)ROI)->left,
								 .width 	= ((Rect_type*)ROI)->width,
								 .height	= ((Rect_type*)ROI)->height	};
			
			textPoint.x = rect.left + ROILabel_XOffset;
			textPoint.y = rect.top + ROILabel_YOffset;
			
			// rectangle overlay
			nullChk( imaqOverlayRect(imgDisplay->NIImage, rect, &rgbVal, IMAQ_DRAW_VALUE, ROI->ROIName) );
			// label overlay
			nullChk( imaqOverlayText(imgDisplay->NIImage, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			break;
			
	}
	
	return 0;
	
Error:
	
	return error;
	
}

static ROI_type* OverlayNIVisionROI (NIImageDisplay_type* imgDisplay, ROI_type* ROI)
{
	int					error 				= 0;
	ROI_type*			ROICopy				= NULL;
	
	errChk( DrawROI(imgDisplay, ROI) );
	
	// make ROI Copy
	nullChk( ROICopy = (*ROI->copyFptr) ((void*)ROI) );
	
	// add ROI overlay to list
	ListInsertItem(GetImageROIs(imgDisplay->baseClass.image), &ROICopy, END_OF_LIST);
	
	// update image
	imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE);
	
	return ROICopy;
	
Error:
	
	// cleanup
	(*ROICopy->discardFptr)((void**)&ROICopy);
	
	return NULL;
}

static void NIVisionROIActions (NIImageDisplay_type* imgDisplay, int ROIIdx, ROIActions action)
{
	ROI_type**	ROIPtr 	= NULL;
	ListType	ROIlist = GetImageROIs(imgDisplay->baseClass.image); 
	size_t		nROIs	= ListNumItems(ROIlist);
	
	if (ROIIdx) {
		ROIPtr = ListGetPtrToItem(ROIlist, ROIIdx);
		switch (action) {
				
			case ROI_Visible:
				
				if (!(*ROIPtr)->active) {
					DrawROI(imgDisplay, *ROIPtr);
					(*ROIPtr)->active = TRUE;
				}
				
				break;
				
			case ROI_Hide:
				
				// clear imaq ROI group (shape and label)
				if ((*ROIPtr)->active) {
					imaqClearOverlay(imgDisplay->NIImage, (*ROIPtr)->ROIName);
					(*ROIPtr)->active = FALSE;
				}
				
				break;
				
			case ROI_Delete:
				
				// clear imaq ROI group (shape and label)
				imaqClearOverlay(imgDisplay->NIImage, (*ROIPtr)->ROIName);
				// discard ROI data
				(*(*ROIPtr)->discardFptr)((void**)ROIPtr);
				// remove ROI from image display list
				ListRemoveItem(ROIlist, 0, ROIIdx);
				break;
		}
	} else {
		for (size_t i = 1; i <= nROIs; i++) {
			ROIPtr = ListGetPtrToItem(ROIlist, i);
			
			switch (action) {
				
				case ROI_Visible:
				
					if (!(*ROIPtr)->active) {
						DrawROI(imgDisplay, *ROIPtr);
						(*ROIPtr)->active = TRUE;
					}
				
					break;
				
				case ROI_Hide:
				
					// clear imaq ROI group (shape and label)
					if ((*ROIPtr)->active) {
						imaqClearOverlay(imgDisplay->NIImage, (*ROIPtr)->ROIName);
						(*ROIPtr)->active = FALSE;
					}
				
					break;
				
				case ROI_Delete:
					
					// clear imaq ROI group (shape and label)
					imaqClearOverlay(imgDisplay->NIImage, (*ROIPtr)->ROIName);
					// discard ROI data
					(*(*ROIPtr)->discardFptr)((void**)ROIPtr);
					// remove ROI from image display list
					ListRemoveItem(ROIlist, 0, ROIIdx);
					break;
			}
		}
		
		//if (action == ROI_Delete)
		//	ListClear(ROIlist);
	}
	
	//SetImageROIs(imgDisplay->baseClass.image, ROIlist);
	
	// update imaq display
	imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE);
	
}

static void	DiscardImaqImg (Image** image)
{
	if (!*image) return;
	
	imaqDispose(*image);
	*image = NULL;
}

static void IMAQ_CALLBACK NIImageDisplay_CB (WindowEventType event, int windowNumber, Tool tool, Rect rect)
{
	NIImageDisplay_type*	display 		= displays[windowNumber];
	Point_type*				PointROI		= NULL;
	Rect_type*				RectROI			= NULL;
	
	switch (event) {
			
		case IMAQ_CLICK_EVENT:
			
			switch (tool) {
					
				case IMAQ_SELECTION_TOOL:
					
					PointROI =  initalloc_Point_type(NULL, "", rect.left, rect.top);
					
					// execute callback
					if (display->baseClass.displayEngine->ROIEventsCBFptr)
						(*display->baseClass.displayEngine->ROIEventsCBFptr) ((ImageDisplay_type*)display, display->baseClass.imageDisplayCBData, display->baseClass.ROIEvent, (ROI_type*) PointROI);
					
					(*PointROI->baseClass.discardFptr) ((void**)&PointROI);
					
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
					
				case IMAQ_LINE_TOOL:
					
					break;
					
				case IMAQ_RECTANGLE_TOOL:
					
					RectROI =  initalloc_Rect_type(NULL, "", rect.top, rect.left, rect.height, rect.width);
					
					// execute callback
					if (display->baseClass.displayEngine->ROIEventsCBFptr)
						(*display->baseClass.displayEngine->ROIEventsCBFptr) ((ImageDisplay_type*)display, display->baseClass.imageDisplayCBData, display->baseClass.ROIEvent,(ROI_type*) RectROI);
					
					(*RectROI->baseClass.discardFptr) ((void**)&RectROI);
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case IMAQ_ACTIVATE_EVENT:
			
			break;
			
		case IMAQ_CLOSE_EVENT:
			
			// broadcast close event to callbacks
			FireCallbackGroup(display->baseClass.callbacks, ImageDisplay_Close); 
			
			// close window and discard image display data
			imaqCloseWindow(windowNumber);
			displays[display->imaqWndID] = NULL;
			(*display->baseClass.discardFptr) ((void**)&display);
			
			break;
			
	
			
		default:
			
			break;
	}
	
}

LRESULT CALLBACK CustomNIImageDisplay_CB (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{   
	NIImageDisplay_type*	disp 			= (NIImageDisplay_type*) dwRefData;
	WORD					wParamHighWord	= HIWORD(wParam);
	WORD					wParamLowWord	= LOWORD(wParam);
	int						error			= 0;
	char*					errMsg			= NULL;
	
	switch (msg) {
			
		case WM_COMMAND:
			
			// filter menu events (wParamHighWord = 0)
			if (wParamHighWord) break;
			
			switch (wParamLowWord) {
					
				case NIDisplayMenu_Save:
					//save as tiff with and without overlays
					//popup and remember dir
					errChk(ImageSavePopup(disp->NIImage, &errMsg));
					break;
					
				case NIDisplayMenu_Restore:
					
					// broadcast RestoreSettings event to callbacks
					FireCallbackGroup(disp->baseClass.callbacks, ImageDisplay_RestoreSettings);
					break;
			}
			
			break;
			
		case WM_KEYDOWN:
			
			switch (wParam) {
				case VK_CONTROL:
					
					disp->baseClass.ROIEvent	= ROI_Added;
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case WM_KEYUP:
			
			switch (wParam) {
				case VK_CONTROL:
					
					disp->baseClass.ROIEvent	= ROI_Placed;
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		default:
			break;
	}
	
	return DefSubclassProc(hWnd, msg, wParam, lParam);
	
Error:
	// call display error handler
	if (!errMsg)
		errMsg = StrDup("Unknown error.\n\n");
	
	
	(*disp->baseClass.displayEngine->errorHandlerCBFptr) ((ImageDisplay_type*)disp, error, errMsg);
	OKfree(errMsg);
	
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static int ImageSavePopup (Image* image, char** errorInfo)
{
	int 				error							= 0;
	char*				errMsg							= NULL;
	int 				fileSelection					= 0;
	char 				fullPathName[MAX_PATHNAME_LEN]	= "";
	char*				strippedPathName				= NULL;
	char 				fileName[MAX_PATHNAME_LEN]		= ""; 
	Image*				imageCopy						= NULL;
	ImageType			imgType							= 0;
	int					imgBorderSize					= 0;
	TIFFFileOptions		tiffOptions						= {.rowsPerStrip = 0, .photoInterp = IMAQ_BLACK_IS_ZERO, .compressionType = IMAQ_ZIP};

	
	fileSelection = FileSelectPopupEx ("","*.tiff", "*.tiff", "Save Image as tiff", VAL_SAVE_BUTTON, 0, 1, fullPathName);
	
	switch (fileSelection) {
			
		case VAL_NO_FILE_SELECTED:
			
			return 0;
			
		case VAL_EXISTING_FILE_SELECTED:
		case VAL_NEW_FILE_SELECTED:
			
			// strip extension and add .tiff extension back
			strippedPathName = strtok(fullPathName,".");
			Fmt(fileName, "%s<%s.tiff", strippedPathName);
			
			// save image as tiff without overlays
			nullChk( imaqWriteTIFFFile(image, fileName, &tiffOptions, NULL) ); 
			
			// make a copy of the image and flatten overlays
			nullChk( imaqGetImageType(image, &imgType) );
			nullChk( imaqGetBorderSize(image, &imgBorderSize) );
			nullChk( imageCopy = imaqCreateImage(imgType, imgBorderSize) );
			nullChk( imaqDuplicate(imageCopy, image) );
			nullChk( imaqMergeOverlay(imageCopy, image, NULL, 0, NULL) );
			
			// save image with ROI overlay and "_ROI" appended to the file name
			Fmt(fileName, "%s<%s_ROI.tiff", strippedPathName);
			nullChk( imaqWriteTIFFFile(imageCopy, fileName, &tiffOptions, NULL) ); 
			
			imaqDispose(imageCopy);
			imageCopy = NULL;
			
			break;
			
		default:   // negative values for errors
			
			return fileSelection;
	}
	
   	return 0;
	
Error:
	
	// cleanup
	if (imageCopy) {
		imaqDispose(imageCopy);
		imageCopy = NULL;
	}
	
	errMsg = imaqGetErrorText(error);
	if (errorInfo)
		*errorInfo = StrDup(errMsg);
	imaqDispose(errMsg);
	
	return error;
}	




