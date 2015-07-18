#include <userint.h>

//==============================================================================
//
// Title:		WaveformDisplay.c
// Purpose:		A short description of the implementation.
//
// Created on:	16-7-2015 at 13:16:19 by Adrian.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "WaveformDisplay.h"
#include "UI_WaveformDisplay.h"

//==============================================================================
// Constants

#define WaveformDisplay_UI 	"./Framework/Display/UI_WaveformDisplay.uir"

#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}
#define OKfreePanHndl(panelHandle)  	if (panelHandle) {DiscardPanel(panelHandle); panelHandle = 0;}
#define OKfreeList(list) 				if (list) {ListDispose(list); list = 0;}  
//==============================================================================
// Types

struct WaveformDisplay {
	
	// DATA
	ListType				waveforms;			// List of Waveform_type* elements. Data in this waveform is used to update the plot whenever needed. Note: do not modify this data directly!
	BOOL					update;				// If True, the plot will display each time the latest waveform added to it. If False, the user can use the scrollbar to return to a previous plot. Default: True.
	
	// UI
	int						plotPanHndl;
	int						menuBarHndl;
	int						menuID_Close;
	int						menuID_Save;
	int						scrollbarCtrl;
	
	// CALLBACK
	WaveformDisplayCB_type	callbackFptr;
	void*					callbackData;
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void CVICALLBACK 		WaveformDisplayMenu_CB 				(int menuBar, int menuItem, void *callbackData, int panel);

// Callback for operating the scrollbar control placed in TSPan
static int CVICALLBACK 			TSPanScrollbar_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

// Callback for other TSPan controls
static int CVICALLBACK 			TSPanCtrls_CB	 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

// Adds a waveform to a graph. Multiple waveforms can be added to the same graph by calling this function repeatedly.
// Do not free or modify the waveforms while the graph is active as the waveform data is used for graph refresh/updating.
static int						AddWaveformToGraph					(int panel, int control, Waveform_type* waveform); 


//==============================================================================
// Global variables

//==============================================================================
// Global functions

WaveformDisplay_type* init_WaveformDisplay_type (int parentPanHndl, char displayTitle[], WaveformDisplayCB_type waveDispCBFptr, void* callbackData)
{
	int						error		= 0;
	WaveformDisplay_type*	waveDisp 	= malloc (sizeof(WaveformDisplay_type));
	if (!waveDisp) return NULL;
	
	//------------------------------------------------------------------------------------------------------------------------------
	// Init (init here anything that may fail and needs freed)
	//------------------------------------------------------------------------------------------------------------------------------
	
	// DATA
	waveDisp->waveforms			= 0;
	waveDisp->update			= TRUE;
	
	// UI
	waveDisp->plotPanHndl		= 0;
	waveDisp->menuBarHndl		= 0;
	waveDisp->menuID_Close		= 0;
	waveDisp->menuID_Save		= 0;
	waveDisp->scrollbarCtrl		= 0;
	
	// CALLBACKS
	waveDisp->callbackFptr		= waveDispCBFptr;
	waveDisp->callbackData		= callbackData;
	
	//------------------------------------------------------------------------------------------------------------------------------
	// Allocate resources
	//------------------------------------------------------------------------------------------------------------------------------
	
	nullChk( waveDisp->waveforms = ListCreate(sizeof(Waveform_type*)) );
	
	//-----------------------------------------
	// Panel and menus
	//-----------------------------------------
	
	// load waveform panel if not loaded already
	errChk( waveDisp->plotPanHndl = LoadPanel(parentPanHndl, WaveformDisplay_UI, TSPan) );
	// set panel title
	errChk( SetPanelAttribute(waveDisp->plotPanHndl, ATTR_TITLE, displayTitle) );
		
	// add menu bar
	errChk( waveDisp->menuBarHndl = NewMenuBar(waveDisp->plotPanHndl) );
	SetMenuBarAttribute(waveDisp->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0); 
	// Close menu bar item
	errChk( waveDisp->menuID_Close = NewMenu(waveDisp->menuBarHndl, "Close", -1) );
	errChk( SetMenuBarAttribute(waveDisp->menuBarHndl, waveDisp->menuID_Close, ATTR_CALLBACK_DATA, waveDisp) );
	errChk( SetMenuBarAttribute(waveDisp->menuBarHndl, waveDisp->menuID_Close, ATTR_CALLBACK_FUNCTION_POINTER, WaveformDisplayMenu_CB) );
	// Save menu bar item
	errChk( waveDisp->menuID_Save = NewMenu(waveDisp->menuBarHndl, "Save", -1) );
	errChk( SetMenuBarAttribute(waveDisp->menuBarHndl, waveDisp->menuID_Save, ATTR_CALLBACK_DATA, waveDisp) );
	errChk( SetMenuBarAttribute(waveDisp->menuBarHndl, waveDisp->menuID_Save, ATTR_CALLBACK_FUNCTION_POINTER, WaveformDisplayMenu_CB) );
	
	//-----------------------------------------
	// Scrollbar
	//-----------------------------------------
	
	int		sbLeft	= 0;
	int		sbWidth = 0;
	int		sbTop	= 0;
	
	// get position and width where to insert the scrollbar
	GetCtrlAttribute(waveDisp->plotPanHndl, TSPan_SBReserved, ATTR_LEFT, &sbLeft);
	GetCtrlAttribute(waveDisp->plotPanHndl, TSPan_SBReserved, ATTR_TOP, &sbTop);
	GetCtrlAttribute(waveDisp->plotPanHndl, TSPan_SBReserved, ATTR_WIDTH, &sbWidth);
	// create scrollbar
	errChk( waveDisp->scrollbarCtrl = NewCtrl(waveDisp->plotPanHndl, CTRL_NUMERIC_HSLIDE, 0, sbTop, sbLeft) );
	// adjust scrollbar
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_FILL_OPTION, VAL_NO_FILL);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_SHOW_DIG_DISP, 0);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MARKER_STYLE, VAL_NO_MARKERS);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_WIDTH, sbWidth);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_INCDEC_WIDTH, 12);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MIN_VALUE, 0);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MAX_VALUE, 1);   	// will be adjusted if there are more waveforms to plot
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_INCR_VALUE, 1);
	SetCtrlVal(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, 0);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_DIMMED, TRUE);	// dim by default
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_CALLBACK_FUNCTION_POINTER, TSPanScrollbar_CB); 
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_CALLBACK_DATA, waveDisp);
	
	//-----------------------------------------
	// Other controls callbacks
	//-----------------------------------------
	
	SetCtrlVal(waveDisp->plotPanHndl, TSPan_Update, waveDisp->update); 
	SetCtrlAttribute(waveDisp->plotPanHndl, TSPan_Update, ATTR_CALLBACK_FUNCTION_POINTER, TSPanCtrls_CB); 
	SetCtrlAttribute(waveDisp->plotPanHndl, TSPan_Update, ATTR_CALLBACK_DATA, waveDisp);
	
	
	return waveDisp;
	
Error:
	
	// cleanup
	discard_WaveformDisplay_type(&waveDisp);
	return NULL;
}

void discard_WaveformDisplay_type (WaveformDisplay_type** waveDispPtr)
{
	WaveformDisplay_type*	waveDisp = * waveDispPtr;
	if (!waveDisp) return;
	
	//--------------
	// Waveforms
	//--------------
	
	DiscardWaveformList(&waveDisp->waveforms);
	
	//--------------
	// UI
	//--------------
	
	if (waveDisp->plotPanHndl && waveDisp->scrollbarCtrl) {
		DiscardCtrl(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl);
		waveDisp->scrollbarCtrl = 0;
	}
	
	if (waveDisp->menuBarHndl) {
		DiscardMenuBar(waveDisp->menuBarHndl);
		waveDisp->menuBarHndl = 0;
	}
	
	OKfreePanHndl(waveDisp->plotPanHndl);
	
	OKfree(*waveDispPtr);
}

void SetWaveformDisplayCB (WaveformDisplay_type* waveDisp, WaveformDisplayCB_type waveDispCBFptr, void* callbackData)
{
	waveDisp->callbackFptr = waveDispCBFptr;
	waveDisp->callbackData = callbackData;
}

ListType GetDisplayWaveformList (WaveformDisplay_type* waveDisp)
{
	return waveDisp->waveforms;
}

int DisplayWaveform (WaveformDisplay_type* waveDisp, Waveform_type** waveformPtr)
{
	int				error				= 0;
	size_t			nWaveforms			= 0;	 	// number of waveforms contained by the waveform display
	
	// add new waveform to list
	nullChk( ListInsertItem(waveDisp->waveforms, waveformPtr, END_OF_LIST) );
	*waveformPtr = NULL;  // take data in
	nWaveforms = ListNumItems(waveDisp->waveforms);
	
	// update scrollbar min max range
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MIN_VALUE, 1);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MAX_VALUE, nWaveforms);
	// undim scrollbar if there is more than one waveform in the list
	if (nWaveforms > 1)
		SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_DIMMED, FALSE);
	else
		SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_DIMMED, TRUE);
		
	// update plot if needed
	if (waveDisp->update) {
		// clear plot
		errChk( DeleteGraphPlot(waveDisp->plotPanHndl, TSPan_TSGraphPlot, -1, VAL_IMMEDIATE_DRAW) );
		// add waveform to plot
		errChk( AddWaveformToGraph(waveDisp->plotPanHndl, TSPan_TSGraphPlot, *(Waveform_type**)ListGetPtrToItem(waveDisp->waveforms, nWaveforms)) );
		// update scrollbar and display
		SetCtrlVal(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, nWaveforms);
		SetCtrlVal(waveDisp->plotPanHndl, TSPan_SBIndex, nWaveforms);
	}
	
	DisplayPanel(waveDisp->plotPanHndl);
	
Error:
	
	return error;
}

int DiscardWaveforms (WaveformDisplay_type* waveDisp)
{
	int		error = 0;
	
	// clear plot
	errChk( DeleteGraphPlot(waveDisp->plotPanHndl, TSPan_TSGraphPlot, -1, VAL_IMMEDIATE_DRAW) );
	// discard waveforms
	ClearWaveformList(waveDisp->waveforms);
	// update scrollbar maximum range and index display
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MIN_VALUE, 0);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_MAX_VALUE, 0);
	SetCtrlVal(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, 0);
	SetCtrlAttribute(waveDisp->plotPanHndl, waveDisp->scrollbarCtrl, ATTR_DIMMED, TRUE);
	SetCtrlVal(waveDisp->plotPanHndl, TSPan_SBIndex, 0);
	// set update to True
	waveDisp->update = TRUE;
	SetCtrlVal(waveDisp->plotPanHndl, TSPan_Update, TRUE);
	
	// hide panel
	errChk( HidePanel(waveDisp->plotPanHndl) );

Error:
	
	return error;
}

static void CVICALLBACK WaveformDisplayMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	WaveformDisplay_type*	waveDisp = callbackData;
	
	if (menuItem == waveDisp->menuID_Close) {
		
		if (waveDisp->callbackFptr)
			(*waveDisp->callbackFptr) (waveDisp, WaveformDisplay_Close, waveDisp->callbackData);
		
		return;
	}
	
	if (menuItem == waveDisp->menuID_Save) {
		
		
		
		return;
	}
	
}

// callback for operating the scrollbar control placed in TSPan
static int CVICALLBACK TSPanScrollbar_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	int						error			= 0;
	WaveformDisplay_type*	waveDisp 		= callbackData; 
	uInt32					waveformIdx		= 0;
	Waveform_type*			waveform		= NULL;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			GetCtrlVal(panel, control, &waveformIdx);	// 1-based waveform index
			SetCtrlVal(panel, TSPan_SBIndex, waveformIdx);
			// get waveform
			waveform = *(Waveform_type**)ListGetPtrToItem(waveDisp->waveforms, waveformIdx);
			// clear plot
			errChk( DeleteGraphPlot(waveDisp->plotPanHndl, TSPan_TSGraphPlot, -1, VAL_IMMEDIATE_DRAW) );
			// add waveform to plot
			errChk( AddWaveformToGraph(waveDisp->plotPanHndl, TSPan_TSGraphPlot, waveform) );
			break;
	}
	
	return 0;
	
Error:
	
	return 0;
}

static int CVICALLBACK TSPanCtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WaveformDisplay_type*	waveDisp = callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case TSPan_Update:
					
					GetCtrlVal(panel, control, &waveDisp->update);
					
					break;
			}
			
			break;
	}
	
	
	return 0;
}

static int AddWaveformToGraph (int panel, int control, Waveform_type* waveform)
{
	int				error				= 0;
	
	void*			waveformData		= NULL;
	size_t			nSamples			= 0;
	double			samplingRate		= GetWaveformSamplingRate(waveform);
	char*			waveformName		= GetWaveformName(waveform);
	char*			waveformUnit		= GetWaveformPhysicalUnit(waveform);
	WaveformColors	waveformColor		= GetWaveformColor(waveform);
	double			xIncrement			= 1.0;
	
	// set X axis label
	if (samplingRate == 0.0) {
		SetCtrlAttribute(panel, control, ATTR_XNAME, "Samples");
		xIncrement = 1.0;
	} else {
		SetCtrlAttribute(panel, control, ATTR_XNAME, "Time (s)");
		xIncrement = 1/samplingRate;
	}
	
	// set Y axis label
	if (waveformName && waveformName[0]) {
		// add unit to waveform name if there is one
		if (waveformUnit && waveformUnit[0]) {
			nullChk( AppendString(&waveformName, " (", -1) );
			nullChk( AppendString(&waveformName, waveformUnit, -1) );
			nullChk( AppendString(&waveformName, ")", -1) );
		}
		
		SetCtrlAttribute(panel, control, ATTR_YNAME, waveformName);
	}
	
	// set plot title
	//SetCtrlAttribute(panel, control, ATTR_LABEL_TEXT, plotTitle);
	
	waveformData = *(void**)GetWaveformPtrToData(waveform, &nSamples);
	
	// plot waveforms
	switch (GetWaveformDataType(waveform)) {
			
		case Waveform_Char:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_CHAR, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UChar:  
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_UNSIGNED_CHAR, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Short:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_SHORT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UShort:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_UNSIGNED_SHORT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Int:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UInt:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_UNSIGNED_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Int64:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_64BIT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UInt64:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_UNSIGNED_64BIT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_SSize:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_SSIZE_T, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Size:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_SIZE_T, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Float:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_FLOAT, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Double:
			errChk( PlotWaveform(panel, control, waveformData, nSamples, VAL_DOUBLE, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
	}
	
	// refresh plot
	RefreshGraph(panel, control); 
	
		
Error:
	
	OKfree(waveformName);
	OKfree(waveformUnit);
	
	return error;
	
}
