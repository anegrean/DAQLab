//==============================================================================
//
// Title:		NIDisplayEngine.c
// Purpose:		A short description of the implementation.
//
// Created on:	3-2-2015 at 17:41:58 by Adrian Negrean.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "windows.h"  			// include this first!
#include "nivision.h"
#include <formatio.h>
#include "Commctrl.h"
#include "ImageDisplayNIVision.h"
#include "DAQLabUtility.h"
#include "ImageDisplay.h"
#include "toolbox.h"
#include "DataStorage.h"



//==============================================================================
// Constants

#define OKfreeIMAQ(ptr) if (ptr) {imaqDispose(ptr); ptr = NULL;}

#define	MAX_NI_VISION_DISPLAYS				16

#define ROILabel_XOffset					2		// ROI text label offset in the X direction in pixels.
#define ROILabel_YOffset					2		// ROI text label offset in the Y direction in pixels.

// Default image display settings
#define Default_Max_Zoom					3.0		// Maximum zoom factor applied to the image to try to fill the target display area
#define Default_WorkspaceScaling			0.5		// Factor that multiplies the workspace area yielding the display area which should not be exceeded by the displayed window

#define NIDisplayFileSaveDefaultFormat		NIDisplay_Save_TIFF


//==============================================================================
// Types

typedef enum {
	NIDisplayMenu_Save,
	NIDisplayMenu_Restore,
	NIDisplayMenu_Image,
	NIDisplayMenu_Image_Equalize,
	NIDisplayMenu_Image_Equalize_Linear,
	NIDisplayMenu_Image_Equalize_Logarithmic,	
} NIDisplayMenuItems;

typedef enum {
	NIDisplay_Save_TIFF,
	NIDisplay_Save_PNG
} NIDisplayFileSaveFormats;

struct ImageDisplayNIVision {
	// BASE CLASS
	ImageDisplay_type					baseClass;
	
	// DATA
	Image*								NIImage;				// NI Image displayed in the window. Depending on various transformations that can be applied to the original image, this image may be different from the original image kept in the base class.
	int									imaqWndID;				// Assigned IMAQ window ID for display.
	HWND								imaqWndWindowsHndl;		// Windows window handle assigned to display the IMAQ image.
	
	LONG_PTR							imaqWndProc;			// Pointer to the original imaq window procedure. This will be called by windows after the custom window procedure.
	
	// METHODS
};

//==============================================================================
// Static global variables

	// NI Vision Engine
BOOL							NIVisionEngineInitialized				= FALSE;

	// Binding between NI window IDs and display structure data
ImageDisplayNIVision_type*		NIDisplays[MAX_NI_VISION_DISPLAYS]		= {NULL};

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

//==============================================================================
// Static functions

static int								InitializeNIVisionEngine						(void);

//static int								CloseNIVisionEngine								(void);

static int								AdjustNIVisionImageDisplay						(ImageDisplayNIVision_type* imgDisplay);

static int								DisplayNIVisionImage							(ImageDisplayNIVision_type* imgDisplay, Image_type** newImagePtr, char** errorMsg);

static int								DisplayNIVisionRGBImage							(ImageDisplayNIVision_type* imgDisplay, Image_type** imageR, Image_type** imageG, Image_type** imageB);	

	// displays a file selection popup-box and saves a given NI image as two grayscale TIFF files with ZIP compression with and without ROI flattened
static int 								ImageSavePopup 									(Image* image, NIDisplayFileSaveFormats fileFormat, char** errorMsg);

//static ImageDisplayNIVision_type*		GetImageDisplayNIVision							(NIDisplayEngine_type* NIDisplay, void* callbackData, int imgHeight, int imgWidth, ImageTypes imageType);

static int 								DrawROI 										(Image* image, ROI_type* ROI);

static void								NIVisionROIActions								(ImageDisplayNIVision_type* imgDisplay, char ROIName[], ROIActions action);

static void								DiscardImaqImg									(Image** image);


	// For receiving NI Vision callbacks such as placement of ROI
static void IMAQ_CALLBACK				ImageDisplayNIVision_CB							(WindowEventType type, int windowNumber, Tool tool, Rect rect); 

	// For processing custom NI Vision window events
LRESULT CALLBACK 						CustomImageDisplayNIVision_CB					(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

	// Applies a display transform function to the image. On success returns nonzero value, on failure returns 0.
static int								ApplyDisplayTransform							(Image* image, ImageDisplayTransforms dispTransformFunc);

	// Converts Image_type to NI image. Pass an existing (allocated) NI image of the same size as the image to be converted. On success return nonzero, on failure returns 0.
static int								ConvertImageTypeToNIImage						(Image* NIImage, Image_type* image, char** errorMsg);

//==============================================================================
// Global variables

//==============================================================================
// Global functions

static int InitializeNIVisionEngine (void)
{
INIT_ERR

	// check if already initialized
	if (NIVisionEngineInitialized) return 0;

	// IMAQ tool window setup
	nullChk( imaqSetupToolWindow(TRUE, 2, &imaqTools) );
	nullChk( imaqSetCurrentTool(IMAQ_PAN_TOOL) );
	
	// set up display callback
	errChk( imaqSetEventCallback(ImageDisplayNIVision_CB, FALSE) );
	
	NIVisionEngineInitialized = TRUE;
	
Error:
	
	return errorInfo.error;	
	
}

static int AdjustNIVisionImageDisplay (ImageDisplayNIVision_type* imgDisplay)
{
INIT_ERR	
	
	// monitor settings and window display
	HMONITOR		hMonitor			= NULL;			// Monitor handle used to display the image.
	MONITORINFO		monitorInfo			= {.cbSize = sizeof(MONITORINFO)};
	LONG			maxWindowHeight		= 0;
	LONG			maxWindowWidth		= 0;
	float			imgZoom				= 0;			// Final zoom factor that will be applied, not exceeding Default_Max_Zoom.
	int				imgHeight			= 0;
	int				imgWidth			= 0;
	float			heightZoom			= 0; 			// Zoom factor needed to match imgHeight to maxWindowHeight*Default_WorkspaceScaling.
	float			widthZoom			= 0;			// Zoom factor needed to match imgWidth to maxWindowWidth*Default_WorkspaceScaling.
	
	if (!imgDisplay->NIImage) return 0; // no image, do nothing
	
	nullChk( imaqGetImageSize(imgDisplay->NIImage, &imgWidth, &imgHeight) );
	
	// adjust window size and zoom based on the screen dimensions
	hMonitor = MonitorFromWindow(imgDisplay->imaqWndWindowsHndl, MONITOR_DEFAULTTONEAREST);
	if (hMonitor && imgWidth && imgHeight) {
		nullChk( GetMonitorInfo(hMonitor, &monitorInfo) );
		maxWindowHeight = labs(monitorInfo.rcWork.top - monitorInfo.rcWork.bottom);
		heightZoom = (float)maxWindowHeight*(float)Default_WorkspaceScaling/(float)imgHeight;
		maxWindowWidth = labs(monitorInfo.rcWork.left - monitorInfo.rcWork.right);
		widthZoom = (float)maxWindowWidth*(float)Default_WorkspaceScaling/(float)imgWidth;
		
		// pick smallest zoom factor so that the zoomed in image does not exceed the boundaries set by Default_WorkspaceScaling
		imgZoom = min(min(heightZoom, widthZoom), Default_Max_Zoom);
		
		// adjust image zoom
		nullChk( imaqZoomWindow2(imgDisplay->imaqWndID, imgZoom, imgZoom, IMAQ_NO_POINT) );
		
		// adjust window size to match displayed image size
		RECT	targetWindowSize	 	= {.left = 0, .top = 0, .right = (LONG)((float)imgWidth*imgZoom), .bottom = (LONG)((float)imgHeight*imgZoom)};
		DWORD	imaqWindowStyle			= (DWORD)GetWindowLong(imgDisplay->imaqWndWindowsHndl, GWL_STYLE);
		
		nullChk( AdjustWindowRect(&targetWindowSize, imaqWindowStyle, TRUE) );
		nullChk( imaqSetWindowSize(imgDisplay->imaqWndID, abs(targetWindowSize.left - targetWindowSize.right), abs(targetWindowSize.top - targetWindowSize.bottom)) );
		
	}
	
	// set window on top
	nullChk( SetWindowPos(imgDisplay->imaqWndWindowsHndl, HWND_TOPMOST, 0, 0, 0, 0, (SWP_ASYNCWINDOWPOS | SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE)) );
	
	
Error:
	
	return errorInfo.error;
	
}

/*
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
*/

ImageDisplayNIVision_type* init_ImageDisplayNIVision_type	(void*						imageDisplayOwner,
															 ImageTypes 				imageType,
															 int						imgWidth,
															 int						imgHeight,
															 CallbackGroup_type**		callbackGroupPtr)
{
INIT_ERR

	ImageType		imaqImgType				= 0;
	
	HMENU			imaqWndMenuHndl			= 0;	// menu bar for the imaq window
	HMENU			imageMenuHndl			= 0;	// submenu handle for "Image" menu item in the imaq window
	HMENU			equalizeMenuHndl		= 0;	// submenu handle for "Image->Equalize" menu item in the imaq window
	int				newIMAQWindowID			= 0;
										
	ImageDisplayNIVision_type*	niImgDisp 	= malloc(sizeof(ImageDisplayNIVision_type));
	if (!niImgDisp) return NULL;
	
	//------------------------------------------------------------------------------------
	// INIT
	//------------------------------------------------------------------------------------
	
	// Child class
	//----------------------------
	
	niImgDisp->NIImage					= NULL;
	niImgDisp->imaqWndID				= -1;
	niImgDisp->imaqWndWindowsHndl		= 0;	
	niImgDisp->imaqWndProc				= 0;
	
	// Base class
	//----------------------------
	
	errChk( init_ImageDisplay_type(	&niImgDisp->baseClass,
									imageDisplayOwner,
									NULL,
									(DiscardFptr_type) discard_ImageDisplayNIVision_type,
									(DisplayImageFptr_type) DisplayNIVisionImage,
									(ROIActionsFptr_type) NIVisionROIActions,
									callbackGroupPtr) );
	
	
	//------------------------------------------------------------------------------------
	// ALLOC
	//------------------------------------------------------------------------------------
	
	// initialize NI Vision engine
	errChk( InitializeNIVisionEngine() );
	
	// get an available IMAQ window handle
	nullChk ( imaqGetWindowHandle(&newIMAQWindowID) );
	errChk(newIMAQWindowID);
	niImgDisp->imaqWndID = newIMAQWindowID;
	
	// get windows window handle
	nullChk( niImgDisp->imaqWndWindowsHndl = (HWND) imaqGetSystemWindowHandle(niImgDisp->imaqWndID) ); 
	
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
	
	// pre allocate image memory
	nullChk( niImgDisp->NIImage = imaqCreateImage(imaqImgType, 0) );
	nullChk( imaqSetImageSize(niImgDisp->NIImage, imgWidth, imgHeight) );
	
	//--------------------------------------------------------------------------------------------------------
	// Create menu bar and add menu items
	//--------------------------------------------------------------------------------------------------------
	
	imaqWndMenuHndl = CreateMenu();
	// add menu item "Save"
	AppendMenu(imaqWndMenuHndl, MF_STRING, NIDisplayMenu_Save, "&Save");
	// add menu item "Restore"
	AppendMenu(imaqWndMenuHndl, MF_STRING, NIDisplayMenu_Restore, "&Restore");
	
	// add menu bar to the imaq window
	SetMenu(niImgDisp->imaqWndWindowsHndl, imaqWndMenuHndl);
	
	//-------------------------------------------------------------------------
	// Add Menu->Image menu and add menu items
	//-------------------------------------------------------------------------
	
	// "Image" menu
	imageMenuHndl = CreateMenu();
	
	// add menu item "Image" to imaq Menu
	AppendMenu(imaqWndMenuHndl, MF_STRING | MF_POPUP, (UINT_PTR)imageMenuHndl, "&Image");
	
	//-------------------------------------------------------------------------
	// Add Menu->Image->Equalize popup menu and add menu items
	//-------------------------------------------------------------------------
	
	equalizeMenuHndl = CreatePopupMenu();
	// add menu item "Linear"
	AppendMenu(equalizeMenuHndl, MF_STRING, NIDisplayMenu_Image_Equalize_Linear, "&Linear");
	// add menu item "Logarithmic"
	AppendMenu(equalizeMenuHndl, MF_STRING, NIDisplayMenu_Image_Equalize_Logarithmic, "&Logarithmic");
	
	// add menu item "Equalize" to Menu->Image
	AppendMenu(imageMenuHndl, MF_STRING | MF_POPUP, (UINT_PTR)equalizeMenuHndl, "&Equalize");
	
	
	// add custom window callback function
	SetWindowSubclass(niImgDisp->imaqWndWindowsHndl, CustomImageDisplayNIVision_CB, 0, (DWORD_PTR)niImgDisp);
	
	// adjust display window
	AdjustNIVisionImageDisplay(niImgDisp);
	
	return niImgDisp;
	
Error:

	discard_ImageDisplayNIVision_type(&niImgDisp);
	return NULL;
}

static void discard_ImageDisplayNIVision_type (ImageDisplayNIVision_type** displayPtr)
{
	ImageDisplayNIVision_type*	display = *displayPtr;
	
	if (!display) return;
	
	//---------------------------------------------------
	// discard child class data
	
	if (display->imaqWndID >= 0)
		NIDisplays[display->imaqWndID] = NULL;
	
	// discard NI image
	DiscardImaqImg((Image**)&display->NIImage);

	//---------------------------------------------------
	// discard parent class
	discard_ImageDisplay_type((ImageDisplay_type**) displayPtr);
}

static int DisplayNIVisionImage (ImageDisplayNIVision_type* imgDisplay, Image_type** newImagePtr, char** errorMsg)
{								
INIT_ERR

	// bind image display data to window ID
	NIDisplays[imgDisplay->imaqWndID] = imgDisplay;
	
	// apply current image transform if there was any to the new image
	if (imgDisplay->baseClass.image)
		SetImageDisplayTransform(*newImagePtr, GetImageDisplayTransform(imgDisplay->baseClass.image));
	
	// discard current image and assign new image
	discard_Image_type(&imgDisplay->baseClass.image);
	imgDisplay->baseClass.image = *newImagePtr;
	*newImagePtr = NULL;
	
	errChk( ConvertImageTypeToNIImage(imgDisplay->NIImage, imgDisplay->baseClass.image, &errorInfo.errMsg) );
	
	// display NI image
	nullChk( imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE) );
			 
	// display IMAQ tool window
	int		isToolWindowVisible = 0;
	nullChk( imaqIsToolWindowVisible(&isToolWindowVisible) );
	
	if (!isToolWindowVisible) {
		nullChk( imaqShowToolWindow(TRUE) );
	}
	
	return 0;
		    
Error:
	
RETURN_ERR
}

static int DisplayNIVisionRGBImage (ImageDisplayNIVision_type* imgDisplay, Image_type** imageR, Image_type** imageG, Image_type** imageB)
{
	return 0;
}

static int DrawROI (Image* image, ROI_type* ROI)
{
INIT_ERR

	RGBValue			rgbVal 				= { .R		= ROI->rgba.R,
												.G		= ROI->rgba.G,
												.B		= ROI->rgba.B,
												.alpha	= ROI->rgba.alpha };
	Point				textPoint;
	
	OverlayTextOptions	overlayTextOptions	= {	.fontName					="Arial",
						 						.fontSize					= ROI->textFontSize,
												.bold						= FALSE,
												.italic						= FALSE,
												.underline					= FALSE,
												.strikeout					= FALSE,
												.horizontalTextAlignment	= IMAQ_LEFT,
												.verticalTextAlignment		= IMAQ_TOP,
												.backgroundColor			= {ROI->textBackground.R,
														  					   ROI->textBackground.G,
																			   ROI->textBackground.B,
																			   ROI->textBackground.alpha},
												.angle						= 0				};
	
	// overlay ROI on the image
	switch (ROI->ROIType) {
			
		case ROI_Point:
			
			Point 		point = { .x = ((Point_type*)ROI)->x, 
								  .y = ((Point_type*)ROI)->y	};
			
			textPoint.x = point.x + ROILabel_XOffset;
			textPoint.y = point.y + ROILabel_YOffset;
			
			// point overlay 
			nullChk( imaqOverlayPoints(image, &point, 1, &rgbVal, 1, IMAQ_POINT_AS_CROSS, NULL, ROI->ROIName) );   
			// label overlay
			nullChk( imaqOverlayText(image, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			break;
			
		case ROI_Rectangle:
			
			Rect		rect = { .top 		= ((Rect_type*)ROI)->top,
								 .left 		= ((Rect_type*)ROI)->left,
								 .width 	= ((Rect_type*)ROI)->width,
								 .height	= ((Rect_type*)ROI)->height	};
			
			textPoint.x = rect.left + ROILabel_XOffset;
			textPoint.y = rect.top + ROILabel_YOffset;
			
			// rectangle overlay
			nullChk( imaqOverlayRect(image, rect, &rgbVal, IMAQ_DRAW_VALUE, ROI->ROIName) );
			// label overlay
			nullChk( imaqOverlayText(image, textPoint, ROI->ROIName, &rgbVal, &overlayTextOptions, ROI->ROIName) ); 
			break;
			
	}
	
	
Error:
	
	return errorInfo.error;
	
}

static void NIVisionROIActions (ImageDisplayNIVision_type* imgDisplay, char ROIName[], ROIActions action)
{
	ROI_type**		ROIPtr 		= NULL;
	ROI_type*		ROI			= NULL;
	ListType		ROIlist 	= GetImageROIs(imgDisplay->baseClass.image);
	size_t			nROIs		= ListNumItems(ROIlist);
	size_t			i			= 1;
	
	while (i <= nROIs) {
		
		ROIPtr = ListGetPtrToItem(ROIlist, i);
		ROI = *ROIPtr;
		
		// check if ROIName is given if it matches with any ROI names in the image list
		if (ROIName && strcmp(ROIName, ROI->ROIName)) {
			i++;
			continue;
		}
		
		switch (action) {
				
			case ROI_Show:
				
				if (!ROI->active) {
					DrawROI(imgDisplay->NIImage, ROI);
					ROI->active = TRUE;
				}
				i++;
				break;
				
			case ROI_Hide:
				
				// clear imaq ROI group (shape and label)
				if (ROI->active) {
					imaqClearOverlay(imgDisplay->NIImage, ROI->ROIName);
					ROI->active = FALSE;
				}
				i++;
				break;
				
			case ROI_Delete:
				
				// clear imaq ROI group (shape and label)
				if (ROI->active)
					imaqClearOverlay(imgDisplay->NIImage, ROI->ROIName);
				
				// discard ROI data
				(*ROI->discardFptr)((void**)ROIPtr);
				// remove ROI from image display list
				ListRemoveItem(ROIlist, 0, i);
				nROIs--;
				break;
		}
		
	}
	
	// update imaq display
	imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE);
	
	return;
	
}

static void	DiscardImaqImg (Image** image)
{
	if (!*image) return;
	
	OKfreeIMAQ(*image);
}

static void IMAQ_CALLBACK ImageDisplayNIVision_CB (WindowEventType event, int windowNumber, Tool tool, Rect rect)
{
	ImageDisplayNIVision_type*		imgDisplay 		= NIDisplays[windowNumber];
	
	switch (event) {
		
		case IMAQ_CLICK_EVENT:
			
			switch (tool) {
					
				case IMAQ_POINT_TOOL:
					
					RGBA_type	color 		= {.R = Default_ROI_R_Color, .G = Default_ROI_G_Color, .B = Default_ROI_B_Color, .alpha = Default_ROI_A_Color};
					ListType	ROIlist		= 0;
					
					// discard previous ROI selection if any
					if (imgDisplay->baseClass.selectionROI)
						(*imgDisplay->baseClass.selectionROI->discardFptr) ((void**)&imgDisplay->baseClass.selectionROI);
					
					// update current selection
					imgDisplay->baseClass.selectionROI = (ROI_type*)initalloc_Point_type(NULL, "", color, TRUE, rect.left, rect.top);
					
					// inform that ROI was placed
					FireCallbackGroup(imgDisplay->baseClass.callbackGroup, ImageDisplay_ROI_Placed, imgDisplay->baseClass.selectionROI);
					
					// inform if ROI must be added to the image
					if (imgDisplay->baseClass.addROIToImage) {
						
						// add a unique name to the ROI selection
						OKfree(imgDisplay->baseClass.selectionROI->ROIName);
						ROIlist = GetImageROIs(imgDisplay->baseClass.image);
						imgDisplay->baseClass.selectionROI->ROIName = GetDefaultUniqueROIName(ROIlist);
						
						
						ROI_type*	ROICopy 		= (*imgDisplay->baseClass.selectionROI->copyFptr) (imgDisplay->baseClass.selectionROI);
						ROI_type*	ROIReference	= ROICopy;
						
						// add ROI to image
						AddImageROI(imgDisplay->baseClass.image, &ROICopy, NULL);

						// update image
						DrawROI(imgDisplay->NIImage, ROIReference);
						imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE);
						
						// inform that ROI was added
						FireCallbackGroup(imgDisplay->baseClass.callbackGroup, ImageDisplay_ROI_Added, ROIReference);
					}

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
			
			RGBA_type	color 		= {.R = Default_ROI_R_Color, .G = Default_ROI_G_Color, .B = Default_ROI_B_Color, .alpha = Default_ROI_A_Color};
			ListType	ROIlist		= 0;
					
			// discard previous ROI selection if any
			if (imgDisplay->baseClass.selectionROI)
				(*imgDisplay->baseClass.selectionROI->discardFptr) ((void**)&imgDisplay->baseClass.selectionROI);
			
			switch (tool) {
					
				case IMAQ_RECTANGLE_TOOL:
					
					// update current selection
					imgDisplay->baseClass.selectionROI = (ROI_type*)initalloc_Rect_type(NULL, "", color, TRUE, rect.top, rect.left, rect.height, rect.width);
					break;
					
				default:
					
					return;
					
			}
					
			// inform that ROI was placed
			FireCallbackGroup(imgDisplay->baseClass.callbackGroup, ImageDisplay_ROI_Placed, imgDisplay->baseClass.selectionROI);
		
			// inform if ROI must be added to the image
			if (imgDisplay->baseClass.addROIToImage) {
			
				// add a unique name to the ROI selection
				OKfree(imgDisplay->baseClass.selectionROI->ROIName);
				ROIlist = GetImageROIs(imgDisplay->baseClass.image);
				imgDisplay->baseClass.selectionROI->ROIName = GetDefaultUniqueROIName(ROIlist);
			
			
				ROI_type*	ROICopy 		= (*imgDisplay->baseClass.selectionROI->copyFptr) (imgDisplay->baseClass.selectionROI);
				ROI_type*	ROIReference	= ROICopy;
			
			
				// add ROI to image
				AddImageROI(imgDisplay->baseClass.image, &ROICopy, NULL);
			
				// update image
				DrawROI(imgDisplay->NIImage, ROIReference);
				imaqDisplayImage(imgDisplay->NIImage, imgDisplay->imaqWndID, FALSE);

				// inform that ROI was added
				FireCallbackGroup(imgDisplay->baseClass.callbackGroup, ImageDisplay_ROI_Added, ROIReference);
			}

			break;
				
					
		case IMAQ_ACTIVATE_EVENT:
			
			break;
			
		case IMAQ_CLOSE_EVENT:
			
			// broadcast close event to callbacks
			FireCallbackGroup(imgDisplay->baseClass.callbackGroup, ImageDisplay_Close, NULL);   // Note: listeners should only set their reference to the display to NULL and not discard the display directly.
			
			// close window and discard image display data
			NIDisplays[imgDisplay->imaqWndID] = NULL;
			(*imgDisplay->baseClass.imageDisplayDiscardFptr) ((void**)&imgDisplay);
			
			break;
			
		default:
			
			break;
	}
	
}

LRESULT CALLBACK CustomImageDisplayNIVision_CB (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
INIT_ERR

	ImageDisplayNIVision_type*	disp 				= (ImageDisplayNIVision_type*) dwRefData;
	WORD						wParamHighWord		= HIWORD(wParam);
	WORD						wParamLowWord		= LOWORD(wParam);
	
	switch (msg) {
		
		case WM_COMMAND:
			
			// filter menu events (wParamHighWord = 0)
			if (wParamHighWord) break;
			
			switch (wParamLowWord) {
					
				case NIDisplayMenu_Save:
					//save as tiff with and without overlays
					//popup and remember dir
					errChk(ImageSavePopup(disp->NIImage, NIDisplayFileSaveDefaultFormat, &errorInfo.errMsg));
					break;
					
				case NIDisplayMenu_Restore:
					
					// broadcast RestoreSettings event to callbacks
					FireCallbackGroup(disp->baseClass.callbackGroup, ImageDisplay_RestoreSettings, NULL);
					break;
					
				case NIDisplayMenu_Image_Equalize_Linear:
					
					// convert image
					SetImageDisplayTransform(disp->baseClass.image, ImageDisplayTransform_Linear);
					errChk( ConvertImageTypeToNIImage(disp->NIImage, disp->baseClass.image, &errorInfo.errMsg) );
					
					// display image
					nullChk( imaqDisplayImage(disp->NIImage, disp->imaqWndID, FALSE) );
					break;
					
				case NIDisplayMenu_Image_Equalize_Logarithmic:
					
					// convert image
					SetImageDisplayTransform(disp->baseClass.image, ImageDisplayTransform_Logarithmic);
					errChk( ConvertImageTypeToNIImage(disp->NIImage, disp->baseClass.image, &errorInfo.errMsg) );
					
					// display image
					nullChk( imaqDisplayImage(disp->NIImage, disp->imaqWndID, FALSE) );
					break;
			}
			
			break;
			
		case WM_KEYDOWN:
			
			switch (wParam) {
					
				case VK_SPACE:
					
					disp->baseClass.addROIToImage	= TRUE;
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		case WM_KEYUP:
			
			switch (wParam) {
					
				case VK_SPACE:
					
					disp->baseClass.addROIToImage	= FALSE;
					
					break;
					
				default:
					
					break;
			}
			
			break;
			
		default:
			break;
	}
	
Error:
	
	OKfree(errorInfo.errMsg);
	return DefSubclassProc(hWnd, msg, wParam, lParam);;
}

static int ApplyDisplayTransform (Image* image, ImageDisplayTransforms dispTransformFunc)
{
INIT_ERR
	
	QuantifyReport*		imageReport	= NULL;
	
	// get global image statistics (note: could take more time than just getting min and max which are the only parameters needed)
	nullChk( imageReport = imaqQuantify(image, NULL) );
	
	// apply transform
	switch (dispTransformFunc) {
			
		case ImageDisplayTransform_Linear:
			
			nullChk( imaqMathTransform(image, image, IMAQ_TRANSFORM_LINEAR, imageReport->global.min, imageReport->global.max, 0, NULL) );
			break;
			
		case ImageDisplayTransform_Logarithmic:
			
			nullChk( imaqMathTransform(image, image, IMAQ_TRANSFORM_LOG, imageReport->global.min, imageReport->global.max, 0, NULL) );
			break;
	}
	
Error:
	
	// cleanup
	OKfreeIMAQ(imageReport);
	
	return errorInfo.error;
}

static int ConvertImageTypeToNIImage (Image* NIImage, Image_type* image, char** errorMsg)
{
#define ConvertImageTypeToNIImage_Err_ZeroSizeImage	-1
#define ConvertImageTypeToNIImage_Err_NoImagePixels	-2
INIT_ERR
	
	int			imgWidth	= 0;
	int			imgHeight	= 0;
	void*		pixelArray	= GetImagePixelArray(image);
	ListType	ROIList		= GetImageROIs(image);
	size_t		nROIs		= ListNumItems(ROIList);
	ROI_type*	ROI 		= NULL;
	
	GetImageSize(image, &imgWidth, &imgHeight);
	
	// check image
	if (!imgWidth || !imgHeight)
		SET_ERR(ConvertImageTypeToNIImage_Err_ZeroSizeImage, "Image must have non-zero pixel width and height.");
	
	if (!pixelArray)
		SET_ERR(ConvertImageTypeToNIImage_Err_NoImagePixels, "Image has no pixel data.");
		
	// convert pixel array to image
	nullChk( imaqArrayToImage(NIImage, pixelArray, imgWidth, imgHeight) );
	
	// draw ROIs
	for (size_t i = 1; i <= nROIs; i++) {
		ROI = *(ROI_type**) ListGetPtrToItem(ROIList, i);
		if (ROI->active) {
			errChk( DrawROI(NIImage, ROI) );
		}
	}
	
	// apply display transform 
	errChk( ApplyDisplayTransform(NIImage, GetImageDisplayTransform(image)) );
	
Error:
	
RETURN_ERR
}

static int ImageSavePopup (Image* image, NIDisplayFileSaveFormats fileFormat, char** errorMsg)
{
INIT_ERR

	int 				fileSelection					= 0;
	char 				fileName[MAX_PATHNAME_LEN]		= ""; 
	Image*				imageCopy						= NULL;
	ImageType			imgType							= 0;
	int					imgBorderSize					= 0;
	TIFFFileOptions		tiffOptions						= {.rowsPerStrip = 0, .photoInterp = IMAQ_BLACK_IS_ZERO, .compressionType = IMAQ_ZIP};
	//TransformBehaviors	overlayTransformBehavior		= {.ShiftBehavior = IMAQ_GROUP_TRANSFORM, .ScaleBehavior = IMAQ_GROUP_TRANSFORM, .RotateBehavior = IMAQ_GROUP_TRANSFORM, .SymmetryBehavior = IMAQ_GROUP_TRANSFORM};

	
	switch (fileFormat) {
			
		case NIDisplay_Save_TIFF:
			fileSelection = FileSelectPopupEx ("","*.tiff", "*.tiff", "Save Image as tiff", VAL_SAVE_BUTTON, 0, 1, fileName);
			break;
			
		case NIDisplay_Save_PNG:
			fileSelection = FileSelectPopupEx ("","*.png", "*.png", "Save Image as png", VAL_SAVE_BUTTON, 0, 1, fileName);
			break;
	}
	
	switch (fileSelection) {
			
		case VAL_NO_FILE_SELECTED:
			
			return 0;
			
		case VAL_EXISTING_FILE_SELECTED:
		case VAL_NEW_FILE_SELECTED:
			
			// save only the image without overlays
			switch (fileFormat) {
					
				case NIDisplay_Save_TIFF:
					
					nullChk( imaqWriteTIFFFile(image, fileName, &tiffOptions, NULL) ); 
					break;
				
				case NIDisplay_Save_PNG:
					
					nullChk( imaqWritePNGFile2(image, fileName, 0, NULL, TRUE) );
					break;
			}
			
			// make a copy of the image and flatten overlays
			nullChk( imaqGetImageType(image, &imgType) );
			nullChk( imaqGetBorderSize(image, &imgBorderSize) );
			nullChk( imageCopy = imaqCreateImage(imgType, imgBorderSize) );
			nullChk( imaqDuplicate(imageCopy, image) );
			//nullChk( imaqSetOverlayProperties(imageCopy, NULL, &overlayTransformBehavior) ); 
			nullChk( imaqMergeOverlay(imageCopy, image, NULL, 0, NULL) );
			
			// save image with overlays and "_ROI" appended to the file name
			switch (fileFormat) {
					
				case NIDisplay_Save_TIFF:
					
					strcpy(strrchr(fileName, 46), "_ROI.tiff");
					nullChk( imaqWriteTIFFFile(imageCopy, fileName, &tiffOptions, NULL) ); 
					break;
				
				case NIDisplay_Save_PNG:
					
					strcpy(strrchr(fileName, 46), "_ROI.png");
					nullChk( imaqWritePNGFile2(imageCopy, fileName, 0, NULL, TRUE) );
					break;
			}
			
			OKfreeIMAQ(imageCopy);
			break;
			
		default:   // negative values for errors
			
			return fileSelection;
	}
	
   	return 0;
	
Error:
	
	// cleanup
	OKfreeIMAQ(imageCopy);
	
	char* IMAQErrMsg = imaqGetErrorText(errorInfo.error);
	errorInfo.errMsg = StrDup(IMAQErrMsg);
	OKfreeIMAQ(IMAQErrMsg);
	
RETURN_ERR
}	




