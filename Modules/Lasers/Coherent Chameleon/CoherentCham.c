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
#include <formatio.h> 
#include "asynctmr.h"
#include <rs232.h>
#include "DAQLabUtility.h"
#include "CoherentCham.h"
#include "UI_CoherentCham.h"

//==============================================================================
// Constants

#define MOD_CoherentCham_UI					"./Modules/Lasers/Coherent Chameleon/UI_CoherentCham.uir"
#define LaserStatusQuerryInterval			1.0	// in [s]

#define RS232ErrChk(fCall) if (errorInfo.error = (fCall), errorInfo.error < 0) \
{goto RS232Error;} else
	
// Default Laser COM port settings
#define	Default_COMPort_Number				0
#define Default_COMPort_BaudRate			19200
#define Default_COMPort_Parity				0
#define	Default_COMPort_DataBits			8
#define	Default_COMPort_StopBits			1

//--------------------------------
// Laser status queries
//--------------------------------

#define RS232_GetLaserStatus		   		"?L\r\n"   			// Status.
#define RS232_GetShutter			   		"?S\r\n"   			// Shutter.
#define RS232_GetWavelength	 		 		"?VW\r\n"			// Wavelength.
#define RS232_GetPumpPeakHold		 		"?PHLDP\r\n"		// Pump peak hold.
#define RS232_GetLaserPower	 		 		"?UF\r\n"			// Laser power in [mW].
#define RS232_GetTuning		 		 		"?TS\r\n"			// Tuning status. 0 = Ready (no tuning in progress), 1 = Tuning in progress, 2 = Search for modelock in progress, 3 = Recovery operation in progress
#define RS232_GetMaxWavelength		 		"?TMAX\r\n"			// Maximum tuning wavelength in [nm].
#define RS232_GetMinWavelength		 		"?TMIN\r\n"			// Minimum tuning wavelength in [nm].
#define RS232_GetAlignmentMode		 		"?ALIGN\r\n"		// Returns the status of the alignment mode. 1 = Enabled, 0 = Disabled.
#define RS232_GetAlignmentModePower	 		"?ALIGNP\r\n"		// Returns the laser power available in [mW] with alignment mode enabled.
#define RS232_GetAlignmentModeWavelength	"?ALIGNW\r\n"		// Returns the alignment mode laser wavelength in [nm].
#define RS232_GetOperatingStatusString		"?ST\r\n"			// Returns the operating status of the laser as string such as "Starting", "OK".
#define RS232_GetModelocking				"?MDLK\r\n"			// Returns the modelocked state of the laser. 0 = Off, 1 = Modelocked, 2 = CW.
	
//--------------------------------
// Laser commands
//--------------------------------
	
#define RS232_SetEcho						"ECHO=%d\r\n"		// Echo mode of laser, i.e. if characters sent to the laser are transmitted back to the host. 0 = Echo off, 1 = Echo on.
#define RS232_SetPrompt						"PROMPT=%d\r\n"		// Prompt mode, i.e. the laser returns "Chameleon>" after each command. 0 = Off, 1 = On.
#define RS232_SetLaserStatus				"LASER=%d\r\n"		// Turns laser on/off. 0 = Laser is put to standby. 1 = Faults are cleared and laser is turned on.
#define RS232_SetShutter					"SHUTTER=%d\r\n"	// Opens/Closes shutter. 0 = Closed, 1 = Open.
#define RS232_SetWavelength					"WAVELENGTH=%d\r\n"	// Sets the laser wavelength in [nm]. If the wavelength is out of the tuning range then the wavelength is set to the lower or upper limit.
#define RS232_SetAlignmentMode				"ALIGN=%d\r\n"		// Accesses the alignment mode. 0 = Exits alignment mode, 1 = Enters alignment mode.
#define RS232_SetPumpPeakHold				"PHLDP=%d\r\n"		// Turns on/off the pump peak hold. The pump peak hold stabilizes the laser and beam jitter is reduced but the laser may fall out of modelocking. 0 = Off, 1 = On.
	
//--------------------------------
// COM settings
//--------------------------------

#define COMPort_ReplyTimeout				10.0				// Timeout to wait for a reply from the laser in [s]
#define COMPort_Timeout						0.02				// Timeout in [s] for read and write operations
#define COMPort_NBytes						50					// Number of bytes to read at once
	
//--------------------------------
	
#define NumErrorBeeps						10					// Number of beeps to generate while laser in error state

//==============================================================================
// Types

typedef struct {
	
	int						portNumber;
	long					baudRate;
	int						parity;
	int						dataBits;
	int						stopBits;
	
} COMPortSettings_type;

typedef struct {
	
	uInt32					status;					// Laser Status. 0 = Laser is Off, 1 = Laser is On, 2 = Laser Fault.  
	BOOL					shutter;				// Shutter. True = Open, False = Closed.
	BOOL					pumpPeakHold;			// Pump peak hold. True = Hold on, False = Hold off. 
													// Note: If hold is on then the laser beam intensity is stabilized but the laser may fall out of modelocking.
	uInt32					modelocked;				// Modelocking. 0 = Laser off (standby), 1 = Modelocked, 2 = CW (continuous emission).
	uInt32					wavelength;				// Wavelength in [nm].
	uInt32					maxWavelength;			// Maximum wavelength in [nm] that can be set.
	uInt32					minWavelength;  		// Minimum wavelength in [nm] that can be set.
	uInt32					power;					// Laser output power in [mW].
	uInt32					tuning;					// Tuning status. 0 = Ready (no tuning in progress), 1 = Tuning in progress, 2 = Search for modelock in progress, 3 = Recovery operation in progress.
	BOOL					alignmentMode;  		// If True, laser power is reduced and laser is set to CW. If False, laser operates normally.
	uInt32					alignmentWavelength;	// Laser wavelength in [nm] when set to alignment mode.
	char					statusString[100];		// laser operating status as a string e.g. "Starting", "OK".
	
	
} LaserStatus_type;

typedef struct {
	// SUPER, must be the first member to inherit from
	DAQLabModule_type 		baseClass;
	
	//--------------------------------------------------------
	// CHILD class
	//--------------------------------------------------------
	
	// DATA
	int						statusTimerID;
	BOOL					updating;			// If True, the timer callback is active and is updating the laser parameters and the UI.
	LaserStatus_type		laserStatus;		// Laser operation parameters.
	COMPortSettings_type*	COMPortInfo;		// COM port connection info.
	uInt32					beepCounter;		// Number of beeps to signal error.
	
	// UI
	int						mainPanHndl;
	int*					mainPanTopPos;
	int*					mainPanLeftPos;
	int						serialCOMSetPanHndl;
	int						menuBarHndl;
	int						menuIDSettings;
	
	
} CoherentCham_type;

//==============================================================================
// Static global variables

//==============================================================================
// Static functions


static int 							LoadCfg 							(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

	// retrieves serial COM settings from a given XML element
static int 							LoadCfg_SerialCOM					(COMPortSettings_type* COMSettingsPtr, ActiveXMLObj_IXMLDOMElement_ COMSettingsXMLElement, ERRORINFO* xmlErrorInfo);

static int							SaveCfg								(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

	// saves serial COM settings by adding a COM settings XML element to a given parent XML element 
static int							SaveCfg_SerialCOM					(COMPortSettings_type* COMSettingsPtr, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int							Load 								(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

	// initializes the communication with the laser
static int 							InitLaserCOM						(CoherentCham_type* laser, char** errorMsg);

static int CVICALLBACK 				UILaserControls_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				UISerialCOMSettings_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			SettingsMenu_CB 					(int menuBar, int menuItem, void *callbackData, int panel);

// querries the laser status and updates the UI
static int							GetLaserStatus						(CoherentCham_type* laser, char** errorMsg);
static void 						UpdateLaserUI 						(CoherentCham_type* laser);
int CVICALLBACK 					LaserStatusAsyncTimer_CB 			(int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2);
static int 							SendLaserCommand 					(int COMPortNumber, char message[], char* replyBuffer, char** errorMsg);

// Serial COM
static COMPortSettings_type*		init_COMPortSettings_type			(int portNumber, long baudRate, int parity, int dataBits, int stopBits);
static void							discard_COMPortSettings_type		(COMPortSettings_type** COMSettingsPtr);






//==============================================================================
// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* initalloc_CoherentCham (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	CoherentCham_type* 	laser	= NULL;
	
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
	laser->baseClass.Discard 				= discard_CoherentCham;
	laser->baseClass.Load					= Load;
	laser->baseClass.LoadCfg				= LoadCfg;
	laser->baseClass.SaveCfg				= SaveCfg;
	laser->baseClass.DisplayPanels			= NULL;
	
	//---------------------------
	// Child class: DAQLabModule_type
	
	//----------------------
	// DATA
	//----------------------
	
	laser->statusTimerID					= 0;
	laser->updating							= 0;
	
	laser->laserStatus.alignmentMode		= FALSE;
	laser->laserStatus.alignmentWavelength	= 0;
	laser->laserStatus.maxWavelength		= 0;
	laser->laserStatus.minWavelength		= 0;
	laser->laserStatus.modelocked			= 0;
	laser->laserStatus.power				= 0;
	laser->laserStatus.pumpPeakHold			= FALSE;
	laser->laserStatus.shutter				= FALSE;
	laser->laserStatus.status				= 0;
	laser->laserStatus.tuning				= 0;
	laser->laserStatus.wavelength			= 0;
	
	// RS232 COM port settings
	laser->COMPortInfo						= NULL;
	
	//----------------------
	// UI
	//----------------------
	laser->mainPanHndl						= 0;
	laser->mainPanLeftPos					= NULL;
	laser->mainPanTopPos					= NULL;
	laser->serialCOMSetPanHndl				= 0;
	laser->menuBarHndl						= 0;
	laser->menuIDSettings					= 0;
	
	return (DAQLabModule_type*)laser;
}

void discard_CoherentCham (DAQLabModule_type** mod)
{
	CoherentCham_type* 		laser = *(CoherentCham_type**) mod;
	
	//------------------------------------------------------------
	// discard CoherentCham_type child data
	
	// discard status timer callback (do this before closing the COM port and discarding the panel)
	if (laser->statusTimerID) {
		DiscardAsyncTimer(laser->statusTimerID); 
		laser->statusTimerID = 0;
	}
	
	// close RS232 COM port
	if (laser->COMPortInfo->portNumber) {
		CloseCom(laser->COMPortInfo->portNumber);
		laser->COMPortInfo->portNumber = 0;
	}
	
	discard_COMPortSettings_type(&laser->COMPortInfo);
	
	// discard UI
	OKfree(laser->mainPanLeftPos);
	OKfree(laser->mainPanTopPos);
	OKfreePanHndl(laser->mainPanHndl);
	OKfreePanHndl(laser->serialCOMSetPanHndl);
	if (laser->menuBarHndl) {DiscardMenuBar(laser->menuBarHndl); laser->menuBarHndl = 0;}
	
	//------------------------------------------------------------
	// discard parent class
	
	discard_DAQLabModule(mod);
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR	
	
	CoherentCham_type* 				laser 						= (CoherentCham_type*) mod;     
	ActiveXMLObj_IXMLDOMElement_	COMSettingsXMLElement		= 0;
	
	nullChk( laser->mainPanLeftPos 	= malloc(sizeof(int)) );
	nullChk( laser->mainPanTopPos 	= malloc(sizeof(int)) );
	
	DAQLabXMLNode					laserAttr[]					= {	{"PanLeftPos", 	BasicData_Int, laser->mainPanLeftPos},
											  						{"PanTopPos", 	BasicData_Int, laser->mainPanTopPos} };
	
	// load laser attributes
	errChk( DLGetXMLElementAttributes(moduleElement, laserAttr, NumElem(laserAttr)) );
	
	// get COM settings XML element from module XML element
	errChk( DLGetSingleXMLElementFromElement(moduleElement, "SerialCOMSettings", &COMSettingsXMLElement) );
	if (!COMSettingsXMLElement) goto SkipCOMSettings; // element not found
	
	nullChk( laser->COMPortInfo = init_COMPortSettings_type(Default_COMPort_Number, Default_COMPort_BaudRate, Default_COMPort_Parity, Default_COMPort_DataBits, Default_COMPort_StopBits) );
	errChk( LoadCfg_SerialCOM(laser->COMPortInfo, COMSettingsXMLElement, xmlErrorInfo) ); 

SkipCOMSettings:
Error:
	
	return errorInfo.error;	
}

static int LoadCfg_SerialCOM (COMPortSettings_type* COMSettingsPtr, ActiveXMLObj_IXMLDOMElement_ COMSettingsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR	
	
	int					baudRate			= 0;
	DAQLabXMLNode		COMSettingsAttr[]	= { {"Port", 		BasicData_Int,	&COMSettingsPtr->portNumber},
												{"BaudRate", 	BasicData_Int,	&baudRate},
												{"Parity",		BasicData_Int,	&COMSettingsPtr->parity},
												{"DataBits",	BasicData_Int,	&COMSettingsPtr->dataBits},
												{"StopBits",	BasicData_Int,	&COMSettingsPtr->stopBits}	};
	
	// get COM settings
	errChk( DLGetXMLElementAttributes(COMSettingsXMLElement, COMSettingsAttr, NumElem(COMSettingsAttr)) ); 
	COMSettingsPtr->baudRate = baudRate;
	
Error:
	
	return errorInfo.error;
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR
	
	CoherentCham_type* 		laser 			= (CoherentCham_type*) mod; 
	int						panLeftPos		= 0;
	int						panTopPos		= 0;
	DAQLabXMLNode			laserAttr[]		= {	{"PanTopPos", 	BasicData_Int, &panTopPos},
											  	{"PanLeftPos", 	BasicData_Int, &panLeftPos} };
	
	// get main panel position
	errChk( GetPanelAttribute(laser->mainPanHndl, ATTR_LEFT, &panLeftPos) );
	errChk( GetPanelAttribute(laser->mainPanHndl, ATTR_TOP, &panTopPos) );
	
	// add laser module attributes
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, laserAttr, DL_ATTRIBUTE, NumElem(laserAttr), xmlErrorInfo) );
	
	// add serial COM config
	errChk( SaveCfg_SerialCOM(laser->COMPortInfo, xmlDOM, moduleElement, xmlErrorInfo) );

Error:
	
	return errorInfo.error;
}

static int SaveCfg_SerialCOM (COMPortSettings_type* COMSettingsPtr, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR	
	
	ActiveXMLObj_IXMLDOMElement_	COMSettingsXMLElement		= 0;
	int								baudRate					= COMSettingsPtr->baudRate;
	DAQLabXMLNode					COMSettingsAttr[]			= { {"Port", 		BasicData_Int,	&COMSettingsPtr->portNumber},
																	{"BaudRate", 	BasicData_Int,	&baudRate},
																	{"Parity",		BasicData_Int,	&COMSettingsPtr->parity},
																	{"DataBits",	BasicData_Int,	&COMSettingsPtr->dataBits},
																	{"StopBits",	BasicData_Int,	&COMSettingsPtr->stopBits}	};
	// create COM setttings XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "SerialCOMSettings", &COMSettingsXMLElement) );
	// save COM settings
	errChk( DLAddToXMLElem(xmlDOM, COMSettingsXMLElement, COMSettingsAttr, DL_ATTRIBUTE, NumElem(COMSettingsAttr), xmlErrorInfo) );
	// add COM settings XML element to parent XML element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, COMSettingsXMLElement, NULL) );
	
Error:
	
	// cleanup
	OKfreeCAHndl(COMSettingsXMLElement);
	
	return errorInfo.error;
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
INIT_ERR	
	
	CoherentCham_type* 		laser 	= (CoherentCham_type*) mod;
	
	// load main panel
	errChk( laser->mainPanHndl = LoadPanel(workspacePanHndl, MOD_CoherentCham_UI, MainPan) );
	
	// set main panel position
	if (laser->mainPanLeftPos)
		SetPanelAttribute(laser->mainPanHndl, ATTR_LEFT, *laser->mainPanLeftPos);
	if (laser->mainPanTopPos)
		SetPanelAttribute(laser->mainPanHndl, ATTR_TOP, *laser->mainPanTopPos);
	
	// connect module data and user interface callbackFn to all direct controls in the panels
	SetCtrlsInPanCBInfo(laser, UILaserControls_CB, laser->mainPanHndl);
	
	// add "Settings" menu bar item, callback data and callback function
	laser->menuBarHndl = NewMenuBar(laser->mainPanHndl);
	laser->menuIDSettings = NewMenu(laser->menuBarHndl, "Settings", -1);
	SetMenuBarAttribute(laser->menuBarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	SetMenuBarAttribute(laser->menuBarHndl, laser->menuIDSettings, ATTR_CALLBACK_DATA, laser);
	SetMenuBarAttribute(laser->menuBarHndl, laser->menuIDSettings, ATTR_CALLBACK_FUNCTION_POINTER, SettingsMenu_CB);
	
	// if Serial COM settings have not been loaded, then use default
	if (!laser->COMPortInfo)
		nullChk( laser->COMPortInfo = init_COMPortSettings_type(Default_COMPort_Number, Default_COMPort_BaudRate, Default_COMPort_Parity, Default_COMPort_DataBits, Default_COMPort_StopBits) );
	
	errChk( InitLaserCOM(laser, &errorInfo.errMsg) );
	
	DisplayPanel(laser->mainPanHndl);
	
Error:
	
RETURN_ERR
}

static int InitLaserCOM (CoherentCham_type* laser, char** errorMsg)
{
INIT_ERR

	char	command[100] 	= "";
	char	response[100] 	= "";
	
	// open COM port if assigned and querry laser status
	if (laser->COMPortInfo->portNumber) {
		
		// open COM port
		RS232ErrChk( OpenComConfig(laser->COMPortInfo->portNumber, NULL, laser->COMPortInfo->baudRate, laser->COMPortInfo->parity, laser->COMPortInfo->dataBits, laser->COMPortInfo->stopBits, 0, 0) );  
		RS232ErrChk( SetComTime(laser->COMPortInfo->portNumber, COMPort_Timeout) );
		RS232ErrChk( FlushOutQ(laser->COMPortInfo->portNumber) );
		RS232ErrChk( FlushInQ(laser->COMPortInfo->portNumber) );
		// set prompt off
		Fmt(command, RS232_SetPrompt, 0);
		errChk( SendLaserCommand(laser->COMPortInfo->portNumber, command, response, errorMsg) );
		// set laser echo off
		Fmt(command, RS232_SetEcho, 0);
		errChk( SendLaserCommand(laser->COMPortInfo->portNumber, command, response, errorMsg) );
		// querry tuning range only once here and set values
		errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetMaxWavelength, response, errorMsg) );
		errChk( Scan(response, "%d\r\n", &laser->laserStatus.maxWavelength) );
		errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetMinWavelength, response, errorMsg) );
		errChk( Scan(response, "%d\r\n", &laser->laserStatus.minWavelength) );
		// set min and max wavelength limits on the UI control
		errChk( SetCtrlAttribute(laser->mainPanHndl, MainPan_Wavelength, ATTR_MIN_VALUE, laser->laserStatus.minWavelength) );
		errChk( SetCtrlAttribute(laser->mainPanHndl, MainPan_Wavelength, ATTR_MAX_VALUE, laser->laserStatus.maxWavelength) );
		// get laser wavelength
		errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetWavelength, response, errorMsg) );
		errChk( Scan(response, "%d\r\n", &laser->laserStatus.wavelength) );
		SetCtrlVal(laser->mainPanHndl, MainPan_Wavelength, laser->laserStatus.wavelength);   
		
		// get status
		errChk( GetLaserStatus(laser, errorMsg) );
		// update UI
		UpdateLaserUI(laser);
		
		// create async timer to querry laser
		errChk( laser->statusTimerID = NewAsyncTimer(LaserStatusQuerryInterval, -1, TRUE, LaserStatusAsyncTimer_CB, laser) );
		
		SetPanelAttribute(laser->mainPanHndl, ATTR_DIMMED, FALSE);
		
	} else
		SetPanelAttribute(laser->mainPanHndl, ATTR_DIMMED, TRUE);
	
	
	return 0;
	
RS232Error:
	
	errorInfo.errMsg = StrDup(GetRS232ErrorString(errorInfo.error));

Error:
	
	SetPanelAttribute(laser->mainPanHndl, ATTR_DIMMED, TRUE);   
	
RETURN_ERR
}

static int CVICALLBACK UILaserControls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR
	
	CoherentCham_type* 	laser	= callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			char	command[100] 	= "";
			char	reply[100] 		= "";
			
			switch (control) {
				
				case MainPan_OnOff:
					
					BOOL	laserOn = 0;
					GetCtrlVal(panel, control, &laserOn);
					Fmt(command, RS232_SetLaserStatus, laserOn);
					errChk( SendLaserCommand(laser->COMPortInfo->portNumber, command, reply, &errorInfo.errMsg) );
					break;	
				
				case MainPan_Wavelength:
					
					uInt32	newWavelength = 0;
					GetCtrlVal(panel, control, &newWavelength);
					Fmt(command, RS232_SetWavelength, newWavelength);
					errChk( SendLaserCommand(laser->COMPortInfo->portNumber, command, reply, &errorInfo.errMsg) );
					break;
					
				case MainPan_Shutter:
					
					BOOL	shutter = FALSE;
					GetCtrlVal(panel, control, &shutter);
					Fmt(command, RS232_SetShutter, shutter);
					errChk( SendLaserCommand(laser->COMPortInfo->portNumber, command, reply, &errorInfo.errMsg) );
					break;
					
				case MainPan_PumpPeakHold:
					
					BOOL	pumpPeakHold = FALSE;
					GetCtrlVal(panel, control, &pumpPeakHold);
					Fmt(command, RS232_SetPumpPeakHold, pumpPeakHold);
					errChk( SendLaserCommand(laser->COMPortInfo->portNumber, command, reply, &errorInfo.errMsg) );
					break;
			}
			
			break;
	}
	
	return 0;
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK UISerialCOMSettings_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR
	
	CoherentCham_type* 	laser	= callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case COMSetPan_Port:										  
					
					int	newPortNumber = 0;
					
					GetCtrlVal(panel, control, &newPortNumber);
					
					// stop async timer polling the laser if there is not port number
					if (!newPortNumber) {
						DiscardAsyncTimer(laser->statusTimerID); 
						laser->statusTimerID = 0;
					}
					
					// close port in case it is open
					if (laser->COMPortInfo->portNumber) {
						RS232ErrChk( CloseCom(laser->COMPortInfo->portNumber) );
						laser->COMPortInfo->portNumber = 0;
					}
					
					// assign new port number and initialize polling
					laser->COMPortInfo->portNumber = newPortNumber;
					errChk( InitLaserCOM(laser, &errorInfo.errMsg) );
					break;
					
				case COMSetPan_BaudRate:
					
					uInt32	baudRate = 0;
					GetCtrlVal(panel, control, &baudRate);
					laser->COMPortInfo->baudRate = (long)baudRate;
					break;
					
				case COMSetPan_Parity:
					
					GetCtrlVal(panel, control, &laser->COMPortInfo->parity);
					break;
					
				case COMSetPan_DataBits:
					
					GetCtrlVal(panel, control, &laser->COMPortInfo->dataBits);
					break;
					
				case COMSetPan_StopBits:
					
					GetCtrlVal(panel, control, &laser->COMPortInfo->stopBits);
					break;
					
				case COMSetPan_OKBttn:
					
					OKfreePanHndl(laser->serialCOMSetPanHndl);
					break;
			}
			
			break;
	}
	
	return 0;
	
RS232Error:
	
	errorInfo.errMsg = StrDup(GetRS232ErrorString(errorInfo.error));

Error:
	
PRINT_ERR

	return 0;
}

static void CVICALLBACK SettingsMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
INIT_ERR

	CoherentCham_type* 		laser 		= callbackData;
	
	// load serial COM settings panel if not loaded already
	if (!laser->serialCOMSetPanHndl)
		errChk( laser->serialCOMSetPanHndl = LoadPanel(laser->baseClass.workspacePanHndl, MOD_CoherentCham_UI, COMSetPan) );
	
	SetCtrlsInPanCBInfo(laser, UISerialCOMSettings_CB, laser->serialCOMSetPanHndl);
	
	// populate UI control values
	SetCtrlVal(laser->serialCOMSetPanHndl, COMSetPan_Port, laser->COMPortInfo->portNumber);
	SetCtrlVal(laser->serialCOMSetPanHndl, COMSetPan_BaudRate, laser->COMPortInfo->baudRate);
	SetCtrlVal(laser->serialCOMSetPanHndl, COMSetPan_Parity, laser->COMPortInfo->parity);
	SetCtrlVal(laser->serialCOMSetPanHndl, COMSetPan_DataBits, laser->COMPortInfo->dataBits);
	SetCtrlVal(laser->serialCOMSetPanHndl, COMSetPan_StopBits, laser->COMPortInfo->stopBits);
	
	DisplayPanel(laser->serialCOMSetPanHndl);
	
Error:
	
PRINT_ERR

	return;
}

static void UpdateLaserUI (CoherentCham_type* laser)
{
	char	statusDisplay[100] 	= "";   
	
	// status
	switch (laser->laserStatus.status) {
		
		case 0:
			// laser off
			SetCtrlVal(laser->mainPanHndl, MainPan_OnOff, 0);
			laser->beepCounter = NumErrorBeeps;
			break;
			
		case 1:
			// laser on
			SetCtrlVal(laser->mainPanHndl, MainPan_OnOff, 1);
			laser->beepCounter = NumErrorBeeps;
			break;
			
		case 2:
			// laser error
			if (laser->beepCounter) {
				Beep();
				laser->beepCounter--;
			}
			break;
	}
	// status string
	Fmt(statusDisplay, "%s (%d nm)",laser->laserStatus.statusString, laser->laserStatus.wavelength); 
	SetCtrlVal(laser->mainPanHndl, MainPan_StatusString, statusDisplay);
	// wavelength: Do not update as it will interfere with the user input
	//SetCtrlVal(laser->mainPanHndl, MainPan_Wavelength, laser->laserStatus.wavelength);
	// power
	SetCtrlVal(laser->mainPanHndl, MainPan_Power, laser->laserStatus.power);
	// shutter
	SetCtrlVal(laser->mainPanHndl, MainPan_Shutter, laser->laserStatus.shutter);
	// pump peak hold
	SetCtrlVal(laser->mainPanHndl, MainPan_PumpPeakHold, laser->laserStatus.pumpPeakHold);
	// modelocked
	if (laser->laserStatus.modelocked == 1)
		SetCtrlVal(laser->mainPanHndl, MainPan_Modelocked, TRUE);
	else
		SetCtrlVal(laser->mainPanHndl, MainPan_Modelocked, FALSE);
	
}

int CVICALLBACK LaserStatusAsyncTimer_CB (int reserved, int timerId, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	CoherentCham_type* 	laser 	= callbackData;
	
	// querry laser status
	errChk( GetLaserStatus(laser, &errorInfo.errMsg) );
	// update UI
	UpdateLaserUI(laser);
	
	return 0;
	
Error:
	
PRINT_ERR

	return 0;
}

static int GetLaserStatus (CoherentCham_type* laser, char** errorMsg)
{
INIT_ERR

	char	readBuffer[100]	= "";
	
	// get laser status
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetLaserStatus, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.status) );
	
	// get laser shutter
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetShutter, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.shutter) );
	
	// get laser wavelength
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetWavelength, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.wavelength) );
	
	// get pump peak hold
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetPumpPeakHold, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.pumpPeakHold) );
	
	// get alignment mode
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetAlignmentMode, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.alignmentMode) );
	
	// get alignment wavelength
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetAlignmentModeWavelength, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.alignmentWavelength) );
	
	// get actual laser power
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetLaserPower, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.power) );
	
	// get tuning status
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetTuning, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.tuning) );
	
	// get operating status string
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetOperatingStatusString, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%s\r\n", laser->laserStatus.statusString) );
	
	// get modelocking
	errChk( SendLaserCommand(laser->COMPortInfo->portNumber, RS232_GetModelocking, readBuffer, errorMsg) );
	errChk( Scan(readBuffer, "%d\r\n", &laser->laserStatus.modelocked) );
	
Error:	
	
RETURN_ERR	
}

// pass a reply buffer sufficiently large for the laser reply to fit and make sure the buffer is initialized to 0.
static int SendLaserCommand (int COMPortNumber, char message[], char* replyBuffer, char** errorMsg)
{
#define	SendLaserCommand_ReplyTimeout		-1		// laser does not reply within the default timeout period COMPort_ReplyTimeout
#define SendLaserCommand_LaserCommandError	-2		// laser does not understand the command or command has wrong parameters.

INIT_ERR
	
	int		nChars		= 0;
	double	startTime	= 0; 
	BOOL	timeout		= FALSE;
	
	// send
	RS232ErrChk( ComWrt(COMPortNumber, message, strlen(message)) );
	
	// receive
	startTime = Timer();
	while (!nChars && !timeout) {
		RS232ErrChk( nChars = ComRd(COMPortNumber, replyBuffer, COMPort_NBytes) );
		timeout = (startTime + COMPort_ReplyTimeout < Timer());
	}
	
	// check for reply timeout error
	if (timeout) {
		errorInfo.error 	= SendLaserCommand_ReplyTimeout;
		errorInfo.errMsg 	= StrDup("Laser reply timeout. Check if laser is connected or if COM port settings are correct.");
		goto Error;
	}
	
	// check for laser error reply
	if (nChars && (strstr(replyBuffer, "Error") || strstr(replyBuffer, "ERROR"))) {
		errorInfo.error 	= SendLaserCommand_LaserCommandError;
		errorInfo.errMsg 	= StrDup("Wrong command or parameters.");
		goto Error;
	}
	
	return 0;
	
RS232Error:
	
	errorInfo.errMsg = StrDup(GetRS232ErrorString(errorInfo.error));

Error:
	
RETURN_ERR
}

static COMPortSettings_type* init_COMPortSettings_type (int portNumber, long baudRate, int parity, int dataBits, int stopBits)
{
	COMPortSettings_type*	comSet = malloc(sizeof(COMPortSettings_type));
	if (!comSet) return NULL;
	
	comSet->portNumber 	= portNumber;
	comSet->baudRate	= baudRate;
	comSet->parity		= parity;
	comSet->dataBits	= dataBits;
	comSet->stopBits	= stopBits;
	
	return comSet;
}

static void discard_COMPortSettings_type (COMPortSettings_type** COMSettingsPtr)
{
	OKfree(*COMSettingsPtr);
}



