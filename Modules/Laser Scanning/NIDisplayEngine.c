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
#include "Commctrl.h"
#include "NIDisplayEngine.h"
#include "DisplayEngine.h"
#include "toolbox.h"


//==============================================================================
// Constants

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}  

#define	MAX_NI_VISION_DISPLAYS			16

#define ROILabel_XOffset				3		// ROI text label offset in the X direction in pixels.
#define ROILabel_YOffset				3		// ROI text label offset in the Y direction in pixels.

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

static 							imaqToolWindowVisibleFlag				= FALSE;

NIImageDisplay_type**			displays								= NULL;

int								nNIDisplayEngines						= 0;	// counts the number of NIDisplayEngine_type instances

//==============================================================================
// Static functions

static NIImageDisplay_type*		init_NIImageDisplay_type						(NIDisplayEngine_type* displayEngine, int imaqWndID, void* callbackData); 

static void 					discard_NIImageDisplay_type 					(NIImageDisplay_type** displayPtr);  

static void 					discard_NIDisplayEngine_type 					(NIDisplayEngine_type** niVisionDisplayPtr);

static int						DisplayNIVisionImage							(NIImageDisplay_type* imgDisplay, void* pixelArray, int imgHeight, int imgWidth, ImageTypes imageType, 
																				 double pixSize, double imgTopLeftXCoord, double imgTopLeftYCoord, double imgZCoord);

static NIImageDisplay_type*		GetNIImageDisplay								(NIDisplayEngine_type* NIDisplay, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType);

static ImgDisplayCBData_type	GetNIImageDisplayCBData							(NIImageDisplay_type* imgDisplay);

static int						SetRestoreImgSettingsCBs						(NIImageDisplay_type* imgDisplay, size_t nCallbackFunctions, RestoreImgSettings_CBFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions);

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
													ImageDisplay_CBFptr_type	imgDisplayEventCBFptr,
													ErrorHandlerFptr_type		errorHandlerCBFptr	)
{
	int						error				= 0;
	intptr_t				imaqToolWndHndl		= 0; 
	NIDisplayEngine_type*	niVision 			= malloc(sizeof(NIDisplayEngine_type));
	
	if (!niVision) return NULL;
	
	//--------------------------
	// init base class
	//--------------------------
	init_DisplayEngine_type(	&niVision->baseClass, 
								(DiscardFptr_type) discard_NIDisplayEngine_type, 
								(DisplayImageFptr_type) DisplayNIVisionImage,
								(DiscardFptr_type) DiscardImaqImg,
								(GetImageDisplayFptr_type) GetNIImageDisplay,
								(GetImageDisplayCBDataFptr_type) GetNIImageDisplayCBData,
								(SetRestoreImgSettingsCBsFptr_type) SetRestoreImgSettingsCBs,
								(OverlayROIFptr_type) OverlayNIVisionROI,
								(ROIActionsFptr_type) NIVisionROIActions,
								ROIEventsCBFptr,
								imgDisplayEventCBFptr,
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
	
	// confine window to parent if any
	niVision->parentWindowHndl = parentWindowHndl;
	if (parentWindowHndl) {
		nullChk( imaqToolWndHndl = (intptr_t) imaqGetToolWindowHandle() );
		SetParent( (HWND) imaqToolWndHndl, (HWND)parentWindowHndl);
	}
	
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
	
	// if there are no more NI Vision Displays, then clean up display array
	if (!nNIDisplayEngines) {
		for (int i = 0; i < MAX_NI_VISION_DISPLAYS; i++)
			discard_NIImageDisplay_type(&displays[i]);
		OKfree(displays);
	}
	
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
	niImgDisp->imaqWndID					= imaqWndID;
	niImgDisp->imaqWndProc					= 0;
	
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
	
	// discard image data
	DiscardImaqImg((Image**)&display->baseClass.image);
	
	//---------------------------------------------------
	// discard parent class
	
	discard_ImageDisplay_type((ImageDisplay_type**) displayPtr);
}

static int DisplayNIVisionImage (NIImageDisplay_type* imgDisplay, void* pixelArray, int imgHeight, int imgWidth, ImageTypes imageType, 
								 double pixSize, double imgTopLeftXCoord, double imgTopLeftYCoord, double imgZCoord)
{								
	int		error		= 0;
	
	// copy image settings
	imgDisplay->baseClass.imgHeight 			= imgHeight;
	imgDisplay->baseClass.imgWidth	 			= imgWidth;
	imgDisplay->baseClass.pixSize	 			= pixSize;
	imgDisplay->baseClass.imgTopLeftXCoord	 	= imgTopLeftXCoord;
	imgDisplay->baseClass.imgTopLeftYCoord	 	= imgTopLeftYCoord;
	imgDisplay->baseClass.imgZCoord	 			= imgZCoord;
	imgDisplay->baseClass.imageType				= imageType;
	
	// display image
	nullChk( imaqArrayToImage((Image*)imgDisplay->baseClass.image, pixelArray, imgWidth, imgHeight) );
	nullChk( imaqDisplayImage((Image*)imgDisplay->baseClass.image, imgDisplay->imaqWndID, FALSE) );
			 
	// display IMAQ tool window
	if (!imaqToolWindowVisibleFlag) {
		imaqShowToolWindow(TRUE);
		imaqToolWindowVisibleFlag = TRUE;
	}
	
	return 0;
	
Error:
	
	return error;
}

static NIImageDisplay_type* GetNIImageDisplay (NIDisplayEngine_type* NIDisplay, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType)
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
	nullChk(displays[imaqHndl] = init_NIImageDisplay_type(NIDisplay, imaqHndl, callbackData) );
	
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
	
	nullChk( displays[imaqHndl]->baseClass.image = imaqCreateImage(imaqImgType, 0) );
	// pre allocate image memory
	nullChk( imaqSetImageSize((Image*)displays[imaqHndl]->baseClass.image, imgWidth, imgHeight) );
	
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

static int SetRestoreImgSettingsCBs (NIImageDisplay_type* imgDisplay, size_t nCallbackFunctions, RestoreImgSettings_CBFptr_type* callbackFunctions, void** callbackData, DiscardFptr_type* discardCallbackDataFunctions)
{
	int error = 0;
	
	// discard restore settings callback data
	for (size_t i = 0; i < imgDisplay->baseClass.nRestoreSettingsCBs; i++)
		if (imgDisplay->baseClass.discardCallbackDataFunctions[i])
			(*imgDisplay->baseClass.discardCallbackDataFunctions[i]) (&imgDisplay->baseClass.restoreSettingsCBsData[i]);	
		else
			OKfree(imgDisplay->baseClass.restoreSettingsCBsData[i]);
	
	OKfree(imgDisplay->baseClass.restoreSettingsCBs);
	OKfree(imgDisplay->baseClass.restoreSettingsCBsData); 
	OKfree(imgDisplay->baseClass.discardCallbackDataFunctions);

	imgDisplay->baseClass.nRestoreSettingsCBs = 0;
	
	// allocate restore settings data
	imgDisplay->baseClass.nRestoreSettingsCBs = nCallbackFunctions;
	
	nullChk( imgDisplay->baseClass.discardCallbackDataFunctions 	= malloc (nCallbackFunctions*sizeof(DiscardFptr_type)) );
	nullChk( imgDisplay->baseClass.restoreSettingsCBs 				= malloc (nCallbackFunctions*sizeof(RestoreImgSettings_CBFptr_type)) );
	nullChk( imgDisplay->baseClass.restoreSettingsCBsData 			= malloc (nCallbackFunctions*sizeof(void*)) );
	
	for (size_t i = 0; i < nCallbackFunctions; i++) {
		imgDisplay->baseClass.restoreSettingsCBs[i] 				= callbackFunctions[i];
		imgDisplay->baseClass.restoreSettingsCBsData[i]				= callbackData[i];
		imgDisplay->baseClass.discardCallbackDataFunctions[i] 		= discardCallbackDataFunctions[i];
	}
	
	return 0;
	
Error: 
	
	OKfree(imgDisplay->baseClass.discardCallbackDataFunctions);
	OKfree(imgDisplay->baseClass.restoreSettingsCBs);
	OKfree(imgDisplay->baseClass.restoreSettingsCBsData);
	
	return error;
	
}

static int DrawROI (NIImageDisplay_type* imgDisplay, ROI_type* ROI)
{
	int					error 				= 0;
	RGBValue			rgbVal 				= { .R		= ROI->rgba.R,
												.G		= ROI->rgba.G,
												.B		= ROI->rgba.B,
												.alpha	= ROI->rgba.A	};
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
																			   imgDisplay->baseClass.ROITextBackground.A},
												.angle						= 0				};
	
	// overlay ROI on the image
	switch (ROI->ROIType) {
			
		case ROI_Point:
			
			Point 		point = { .x = ((Point_type*)ROI)->x, 
								  .y = ((Point_type*)ROI)->y	};
			
			textPoint.x = point.x + ROILabel_XOffset;
			textPoint.y = point.y + ROILabel_YOffset;
			
			// point overlay 			
			nullChk( imaqOverlayPoints((Image*)imgDisplay->baseClass.image, &point, 1, &rgbVal, 1, IMAQ_POINT_AS_CROSS, NULL, ROI->ROIName) );   
			// label overlay
			nullChk( imaqOverlayText((Image*)imgDisplay->baseClass.image, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			
			break;
			
		case ROI_Rectangle:
			
			Rect		rect = { .top 		= ((Rect_type*)ROI)->top,
								 .left 		= ((Rect_type*)ROI)->left,
								 .width 	= ((Rect_type*)ROI)->width,
								 .height	= ((Rect_type*)ROI)->height	};
			
			textPoint.x = rect.left + ROILabel_XOffset;
			textPoint.y = rect.top + ROILabel_YOffset;
			
			// rectangle overlay
			nullChk( imaqOverlayRect((Image*)imgDisplay->baseClass.image, rect, &rgbVal, IMAQ_DRAW_VALUE, ROI->ROIName) );
			// label overlay
			nullChk( imaqOverlayText((Image*)imgDisplay->baseClass.image, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			
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
	nullChk( ROICopy = copy_ROI_type(ROI) );
	
	// add ROI overlay to list
	ListInsertItem(imgDisplay->baseClass.ROIs, &ROICopy, END_OF_LIST);
	
	// update image
	imaqDisplayImage((Image*)imgDisplay->baseClass.image, imgDisplay->imaqWndID, FALSE);
	
	return ROICopy;
	
Error:
	
	// cleanup
	discard_ROI_type(&ROICopy);
	
	return NULL;
}

static void NIVisionROIActions (NIImageDisplay_type* imgDisplay, int ROIIdx, ROIActions action)
{
	ROI_type**	ROIPtr 	= NULL;
	size_t		nROIs	= ListNumItems(imgDisplay->baseClass.ROIs);
	if (ROIIdx) {
		ROIPtr = ListGetPtrToItem(imgDisplay->baseClass.ROIs, ROIIdx);
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
					imaqClearOverlay((Image*)imgDisplay->baseClass.image, (*ROIPtr)->ROIName);
					(*ROIPtr)->active = FALSE;
				}
				
				break;
				
			case ROI_Delete:
				
				// clear imaq ROI group (shape and label)
				imaqClearOverlay((Image*)imgDisplay->baseClass.image, (*ROIPtr)->ROIName);
				// discard ROI data
				discard_ROI_type(ROIPtr);
				// remove ROI from image display list
				ListRemoveItem(imgDisplay->baseClass.ROIs, 0, ROIIdx);
				break;
		}
	} else {
		for (size_t i = 1; i <= nROIs; i++) {
			ROIPtr = ListGetPtrToItem(imgDisplay->baseClass.ROIs, i);
			
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
						imaqClearOverlay((Image*)imgDisplay->baseClass.image, (*ROIPtr)->ROIName);
						(*ROIPtr)->active = FALSE;
					}
				
					break;
				
				case ROI_Delete:
				
					// clear imaq ROI group (shape and label)
					imaqClearOverlay((Image*)imgDisplay->baseClass.image, (*ROIPtr)->ROIName);
					// discard ROI data
					discard_ROI_type(ROIPtr);
					// remove ROI from image display list
					ListRemoveItem(imgDisplay->baseClass.ROIs, 0, ROIIdx);
					break;
			}
		}
		
		if (action == ROI_Delete)
			ListClear(imgDisplay->baseClass.ROIs);
	}
	
	// update imaq display
	imaqDisplayImage((Image*)imgDisplay->baseClass.image, imgDisplay->imaqWndID, FALSE);
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
					
					PointROI =  init_Point_type("", rect.left, rect.top);
					
					// execute callback
					if (display->baseClass.displayEngine->ROIEventsCBFptr)
						(*display->baseClass.displayEngine->ROIEventsCBFptr) ((ImageDisplay_type*)display, display->baseClass.imageDisplayCBData, display->baseClass.ROIEvent, (ROI_type*) PointROI);
					
					discard_ROI_type((ROI_type**)&PointROI);
					
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
					
					RectROI =  init_Rect_type("", rect.top, rect.left, rect.height, rect.width);
					
					// execute callback
					if (display->baseClass.displayEngine->ROIEventsCBFptr)
						(*display->baseClass.displayEngine->ROIEventsCBFptr) ((ImageDisplay_type*)display, display->baseClass.imageDisplayCBData, display->baseClass.ROIEvent,(ROI_type*) RectROI);
					
					discard_ROI_type((ROI_type**)&RectROI);
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case IMAQ_ACTIVATE_EVENT:
			
			break;
			
		case IMAQ_CLOSE_EVENT:
			
			// call Close callback
			(*display->baseClass.displayEngine->imgDisplayEventCBFptr) (&display->baseClass, display->baseClass.imageDisplayCBData, ImageDisplay_Close);
			
			// discard image display data
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
					
					break;
					
				case NIDisplayMenu_Restore:
					
					// process each restore function sequencially within this thread
					for (size_t i = 0; i < disp->baseClass.nRestoreSettingsCBs; i++)
						errChk( (*disp->baseClass.restoreSettingsCBs[i]) (disp->baseClass.displayEngine, (ImageDisplay_type*)disp, disp->baseClass.restoreSettingsCBsData[i], &errMsg) );
						
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
	(*disp->baseClass.displayEngine->errorHandlerCBFptr) ((ImageDisplay_type*)disp, error, errMsg);
	OKfree(errMsg);
	
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}


void discard_Image_type (Image** image)
{
	if (!*image) return;
	
	imaqDispose(*image); 
	
	*image = NULL;
}
