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
//==============================================================================
// Types

struct WaveformDisplay {
	
	// DATA
	Waveform_type* 			waveform;			// Data in this waveform is used to update the plot whenever needed
												// Note: do not modify this data directly!
	
	// UI
	int						plotPanHndl;
	int						menuBarHndl;
	int						menuID_Close;
	int						menuID_Save;
	
	// CALLBACKS
	CallbackGroup_type*		callbackGroup;
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void CVICALLBACK 		WaveformDisplayMenu_CB 				(int menuBar, int menuItem, void *callbackData, int panel);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

WaveformDisplay_type* init_WaveformDisplay_type (CallbackGroup_type** callbackGroupPtr)
{
	WaveformDisplay_type*	waveDisp = malloc (sizeof(WaveformDisplay_type));
	if (!waveDisp) return NULL;
	
	// DATA
	waveDisp->waveform			= NULL;
	
	// UI
	waveDisp->menuBarHndl		= 0;
	waveDisp->menuID_Close		= 0;
	waveDisp->menuID_Save		= 0;
	
	// CALLBACKS
	if (callbackGroupPtr) {
		waveDisp->callbackGroup	= *callbackGroupPtr;
		*callbackGroupPtr		= NULL;
	} else
		waveDisp->callbackGroup = NULL;
	
	return waveDisp;
}

void discard_WaveformDisplay_type (WaveformDisplay_type** waveformDisplayPtr)
{
	WaveformDisplay_type*	waveDisp = * waveformDisplayPtr;
	if (!waveDisp) return;
	
	OKfreePanHndl(waveDisp->plotPanHndl);
	discard_Waveform_type(&waveDisp->waveform);
	discard_CallbackGroup_type(&waveDisp->callbackGroup);
	
	OKfree(*waveformDisplayPtr);
}

int DisplayWaveform (WaveformDisplay_type* waveformDisplay, Waveform_type** waveformPtr, char plotTitle[], int parentPanHndl, WaveformDisplayColor waveformColor)
{
	int				error				= 0;
	void*			waveformData		= NULL;
	size_t			nSamples			= 0;
	double			samplingRate		= GetWaveformSamplingRate(*waveformPtr);
	char*			waveformName		= GetWaveformName(*waveformPtr);
	char*			waveformUnit		= GetWaveformPhysicalUnit(*waveformPtr);
	double			xIncrement			= 1.0;
	
	// load waveform panel if not loaded already
	if (!waveformDisplay->plotPanHndl) {
		
		errChk( waveformDisplay->plotPanHndl = LoadPanel(parentPanHndl, WaveformDisplay_UI, TSPan) );
		
		// add menu bar
		errChk (waveformDisplay->menuBarHndl		= NewMenuBar(waveformDisplay->plotPanHndl) );
		// Close menu bar item
		errChk (waveformDisplay->menuID_Close		= NewMenu(waveformDisplay->menuBarHndl, "Close", -1) );
		// Save menu bar item
		errChk (waveformDisplay->menuID_Save		= NewMenu(waveformDisplay->menuBarHndl, "Save", -1) );
		
		// add menu callback
		SetMenuBarAttribute(waveformDisplay->menuBarHndl, 0, ATTR_CALLBACK_DATA, waveformDisplay);
		SetMenuBarAttribute(waveformDisplay->menuBarHndl, 0, ATTR_CALLBACK_FUNCTION_POINTER, WaveformDisplayMenu_CB);
	
	}
	
	// clear plot
	errChk( DeleteGraphPlot(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, -1, VAL_IMMEDIATE_DRAW) );
	
	// discard previous waveform and assign new waveform
	discard_Waveform_type(&waveformDisplay->waveform);
	waveformDisplay->waveform = *waveformPtr;
	*waveformPtr = NULL;					  // take data in
	
	DisplayPanel(waveformDisplay->plotPanHndl);
	
	// set X axis label
	if (samplingRate == 0.0) {
		SetCtrlAttribute(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, ATTR_XNAME, "Samples");
		xIncrement = 1.0;
	} else {
		SetCtrlAttribute(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, ATTR_XNAME, "Time (s)");
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
		
		SetCtrlAttribute(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, ATTR_YNAME, waveformName);
	}
	
	// set plot title
	SetCtrlAttribute(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, ATTR_LABEL_TEXT, plotTitle);
	
	waveformData = *(void**)GetWaveformPtrToData(waveformDisplay->waveform, &nSamples); 
	// plot waveforms
	switch (GetWaveformDataType(waveformDisplay->waveform)) {
			
		case Waveform_Char:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_CHAR, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UChar:  
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_UNSIGNED_CHAR, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Short:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_SHORT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UShort:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_UNSIGNED_SHORT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Int:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UInt:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_UNSIGNED_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Int64:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_64BIT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_UInt64:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_UNSIGNED_64BIT_INTEGER, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_SSize:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_SSIZE_T, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Size:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_SIZE_T, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Float:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_FLOAT, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
			
		case Waveform_Double:
			errChk( PlotWaveform(waveformDisplay->plotPanHndl, TSPan_TSGraphPlot, waveformData, nSamples, VAL_DOUBLE, 1.0, 0.0, 0.0, xIncrement, VAL_THIN_LINE, VAL_NO_POINT, VAL_SOLID, 1, waveformColor) );
			break;
	}

	
Error:
	
	OKfree(waveformName);
	OKfree(waveformUnit);
	
	return error;
}

static void CVICALLBACK WaveformDisplayMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	WaveformDisplay_type*	waveformDisplay = callbackData;
	
	if (menuItem == waveformDisplay->menuID_Close) {
		FireCallbackGroup(waveformDisplay->callbackGroup, WaveformDisplay_Close);
		return;
	}
	
	if (menuItem == waveformDisplay->menuID_Save) {
		FireCallbackGroup(waveformDisplay->callbackGroup, WaveformDisplay_Save);
		return;
	}
	
}
