//==============================================================================
//
// Title:		CoherentCham.c
// Purpose:		A short description of the implementation.
//
// Created on:	5/17/2015 at 12:15:00 AM by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include "asynctmr.h"
#include <rs232.h>
#include "DAQLabUtility.h"
#include "CoherentCham.h"
#include "UI_CoherentCham.h"

//==============================================================================
// Constants

#define MOD_CoherentCham_UI			"./Modules/Lasers/Coherent Chameleon/UI_CoherentCham.uir"
#define LaserStatusQuerryInterval	1.0	// in [s]

#define RS232ErrChk(fCall) if (error = (fCall), error < 0) \
{goto RS232Error;} else

//==============================================================================
// Types

typedef struct {
	int					portNumber;
	long				baudRate;
	int					parity;
	int					dataBits;
	int					stopBits;
	
	
} COMPortSettings;

typedef struct {
	// SUPER, must be the first member to inherit from
	DAQLabModule_type 	baseClass;
	
	//--------------------------------------------------------
	// CHILD class
	//--------------------------------------------------------
	
	// DATA
	uInt16				wavelength;
	int					statusTimerID;
	COMPortSettings		COMPortInfo;
	
	// UI
	int					controlPanHndl;
	int					menuBarHndl;
	int					menuIDSettings;
	
} CoherentCham_type;

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


static int 							LoadCfg 							(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

static int							SaveCfg								(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int							Load 								(DAQLabModule_type* mod, int workspacePanHndl);

static int CVICALLBACK 				UILaserControls_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			SettingsMenu_CB 					(int menuBar, int menuItem, void *callbackData, int panel);

// querries the laser status and updates the UI
static int							GetLaserStatus						(CoherentCham_type* laser, char** errorInfo);
int CVICALLBACK 					LaserStatusAsyncTimer_CB 			(int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2);






//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* initalloc_CoherentCham (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	CoherentCham_type* laser	= NULL;
	
	if (!mod) {
		laser = malloc (sizeof(CoherentCham_type));
		if (!laser) return NULL;
	} else
		laser = (CoherentCham_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&laser->baseClass, className, instanceName, workspacePanHndl);
	
	//---------------------------
	// Parent class: DAQLabModule_type 
	
		// DATA
			
		// METHODS
	
			// overriding methods
	laser->baseClass.Discard 		= discard_CoherentCham;
	laser->baseClass.Load			= Load;
	laser->baseClass.LoadCfg		= LoadCfg;
	laser->baseClass.SaveCfg		= SaveCfg;
	laser->baseClass.DisplayPanels	= NULL;
	
	//---------------------------
	// Child class: DAQLabModule_type
	
	//----------------------
	// DATA
	//----------------------
	laser->wavelength				= 0;
	laser->statusTimerID			= 0;
	
		// RS232 default COM port settings
	laser->COMPortInfo.portNumber	= 0;
	laser->COMPortInfo.baudRate		= 9600;
	laser->COMPortInfo.parity		= 0;
	laser->COMPortInfo.dataBits		= 7;
	laser->COMPortInfo.stopBits		= 1;
	
	//----------------------
	// UI
	//----------------------
	laser->controlPanHndl			= 0;
	laser->menuBarHndl				= 0;
	laser->menuIDSettings			= 0;
	
	
}

void discard_CoherentCham (DAQLabModule_type** mod)
{
	CoherentCham_type* 		laser = *(CoherentCham_type**) mod;
	
	// discard CoherentCham_type child data
	
	// discard parent class
	discard_DAQLabModule(mod);
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
	return 0;	
}


static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
	return 0;
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	int						error	= 0;
	char*					errMsg	= NULL;
	CoherentCham_type* 		laser 	= (CoherentCham_type*) mod;
	
	// load panel resources
	errChk( laser->controlPanHndl = LoadPanel(workspacePanHndl, MOD_CoherentCham_UI, LaserPan) );
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, UILaserControls_CB, laser->controlPanHndl);
	
	// add "Settings" menu bar item, callback data and callback function
	laser->menuBarHndl = NewMenuBar(laser->controlPanHndl);
	laser->menuIDSettings = NewMenu(laser->menuBarHndl, "Settings", -1);
	SetMenuBarAttribute(laser->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(laser->menuBarHndl, laser->menuIDSettings, ATTR_CALLBACK_DATA, laser);
	SetMenuBarAttribute(laser->menuBarHndl, laser->menuIDSettings, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	// open COM port if assigned and querry laser status
	if (laser->COMPortInfo.portNumber) {
		RS232ErrChk( OpenComConfig(laser->COMPortInfo.portNumber, NULL, laser->COMPortInfo.baudRate, laser->COMPortInfo.parity, laser->COMPortInfo.dataBits, laser->COMPortInfo.stopBits, 0, 0) );  
		// create async timer to querry laser
		errChk( laser->statusTimerID = NewAsyncTimer(LaserStatusQuerryInterval, -1, 1, LaserStatusAsyncTimer_CB, laser) );
	}
	
	return 0;
	
Error:
	
	if (errMsg) {
		DLMsg(errMsg, 1);
		DLMsg("\n\n", 0);
	}
	
	OKfree(errMsg);
	
	return error;
	
RS232Error:
	
	char* RS232ErrMsg = GetRS232ErrorString(error);
	DLMsg(RS232ErrMsg, 1);
	DLMsg("\n\n", 0);
	
	return error;
}

static int CVICALLBACK UILaserControls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	
	return 0;
}

static void CVICALLBACK SettingsMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	
}

int CVICALLBACK LaserStatusAsyncTimer_CB (int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2)
{
	int						error 		= 0;
	char*					errMsg		= NULL;
	CoherentCham_type* 		laser 		= callbackData;
	
	
	errChk( GetLaserStatus(laser, &errMsg) );
	
	return 0;
	
Error:
	
	return 0;
}

static int GetLaserStatus (CoherentCham_type* laser, char** errorInfo)
{
	
	return 0;
}



