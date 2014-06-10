//==============================================================================
//
// Title:		DAQLab.c
// Purpose:		A short description of the implementation.
//
// Created on:	8-3-2014 at 13:48:00 by Adrian Negrean.
// Copyright:	. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h"			// include this first 
#include <formatio.h>
#include <userint.h>
#include <toolbox.h>
#include "UI_DAQLab.h"
#include "UI_TaskController.h"
#include "DAQLabModule.h"
#include "TaskController.h" 
//==============================================================================
// Include modules

#include "PIMercuryC863.h"



//==============================================================================
// Constants
	
	// DAQLab main workspace UI resource
#define DAQLAB_UI_DAQLAB							"UI_DAQLab.uir"
	// name of config file
#define DAQLAB_CFG_FILE								"DAQLabCfg.xml"

#define DAQLAB_CFG_DOM_ROOT_NAME					"DAQLabConfig"
	// log panel displayed at startup and displays received messages
#define DAQLAB_LOG_PANEL							TRUE

	// number of Task Controllers visible at once. If there are more, a scroll bar appears.
#define DAQLAB_NVISIBLE_TASKCONTROLLERS				4

	// number of pixels between inserted Task Controller panel and outer Tasks panel
#define DAQLAB_TASKCONTROLLERS_PAN_MARGIN			5
	// vertical spacing between Task Controllers in the panel
#define DAQLAB_TASKCONTROLLERS_SPACING				5 

	// maximum number of characters allowed for a UI Task Controller name
#define DAQLAB_MAX_UITASKCONTROLLER_NAME_NCHARS		50

//==============================================================================
// Types

//------------------------------------------------------------------------------------------------
//                              AVAILABLE DAQLab MODULES
//------------------------------------------------------------------------------------------------

// Constant function pointer that is used to launch the initialization of different DAQLabModules 
typedef DAQLabModule_type* (* const ModuleInitAllocFunction_type ) (DAQLabModule_type* mod);

typedef struct {
	
	char*							ModuleName;
	char*							ModuleXMLName;
	ModuleInitAllocFunction_type	ModuleInitFptr;

} AvailableDAQLabModules_type;

//------------------------------------------------------------------------------------------------
// 			Predefined DAQLab errors and warnings to be used with DAQLab_Msg function.
//------------------------------------------------------------------------------------------------
// DAQLab_Msg is a function with variable arguments and types which depend on
// the particular warning or error message. When calling this function with a 
// certain error code from below, pass in the additional parameters with 
// required data type.

typedef enum _DAQLabMessageID{
										// parameter types (data1, data2, data3, data4)
	// errors							// to pass in DAQLab_Msg
	DAQLAB_MSG_ERR_DOM_CREATION,		// HRESULT* error code, 0, 0, 0
	DAQLAB_MSG_ERR_ACTIVEXML,			// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info, 0
	
	// warnings									
	DAQLAB_MSG_WARN_NO_CFG_FILE,		// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info, 0       
	
	// success									
	DAQLAB_MSG_OK_DOM_CREATION			// 0, 0, 0, 0 
	

} DAQLabMessageID;

//-------------------------------------------------------------------------------------------------
//										 Framework Typedefs
//-------------------------------------------------------------------------------------------------

typedef struct {						  // Glues UI Task Controller to panel handle
										  
	int					panHndl;
	TaskControl_type*   taskControl;
	
} UITaskCtrl_type;

//==============================================================================
// Static global variables

//------------------------------------------------------------------------------------------------
//                              AVAILABLE DAQLab MODULES
//------------------------------------------------------------------------------------------------
AvailableDAQLabModules_type DAQLabModules_InitFunctions[] = {
	
	{ MOD_PIMercuryC863_NAME, MOD_PIMercuryC863_XMLTAG, initalloc_PIMercuryC863 } 
	
};

//-------------------------------------------------------------------------------------------------


	// DAQLab XML ActiveX server controller handle for DOM XML management
CAObjHandle							DAQLabCfg_DOMHndl		= 0;
ActiveXMLObj_IXMLDOMElement_   		DAQLabCfg_RootElement 	= 0; 

	// UI resources	
static int				mainPanHndl			= 0;		// Main workspace panel handle
static int				logPanHndl			= 0;		// Log panel handle

static struct TasksUI_ {								// UI data container for Task Controllers
	
	int			panHndl;								// Tasks panel handle
	int			controllerPanHndl;						// Task Controller panel handle
	int			controllerPanWidth;
	int			controllerPanHeight;
	int			menuBarHndl;
	int			menuID_Manage;
	int			menuItem_Add;
	int			menuItem_Delete;
	int			menuItem_SubTasks;
	ListType	UItaskCtrls;							// UI Task Controllers of UITaskCtrl_type*   
	
} 						TasksUI				= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	// different modules that inherit from DAQLabModule_type 
	// list elements of DAQLabModule_type* size
ListType		DAQLabModules				= 0;




							

//==============================================================================
// Static functions
//==============================================================================

static int					DAQLab_Load 							(void);

static int 					DAQLab_Close 							(void);

static void 				DAQLab_Msg 								(DAQLabMessageID msgID, void* data1, void* data2, void* data3, void* data4);

static int					DAQLab_NewXMLDOM	   					(const char fileName[], CAObjHandle* xmlDOM, ActiveXMLObj_IXMLDOMElement_* rootElement); 

static int					DAQLab_SaveXMLDOM						(const char fileName[], CAObjHandle* xmlDOM);

static UITaskCtrl_type*		DAQLab_AddTaskControllerToUI			(TaskControl_type* taskControl);

static int					DAQLab_RemoveTaskControllerFromUI		(int index);

static void					DAQLab_RedrawTaskControllerUI			(void);

static void CVICALLBACK 	DAQLab_TaskMenu_CB 						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void					DAQLab_TaskMenu_AddTaskController 		(void);

static void					DAQLab_TaskMenu_DeleteTaskController 	(void);

static void					DAQLab_TaskMenu_ManageSubTasks 			(void);

static int CVICALLBACK 		DAQLab_TaskControllers_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static UITaskCtrl_type*		DAQLab_init_UITaskCtrl_type				(TaskControl_type* taskControl);  	

static void					DAQLab_discard_UITaskCtrl_type			(UITaskCtrl_type** a);



//-----------------------------------------
// UI Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	DAQLab_ConfigureUITC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_IterateUITC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_StartUITC					(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_DoneUITC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_StoppedUITC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	DAQLab_ResetUITC 					(TaskControl_type* taskControl, BOOL const* abortFlag); 
static FCallReturn_type* 	DAQLab_ErrorUITC 					(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

void CVICALLBACK 			DAQLab_ManageModules_CB 				(int menuBar, int menuItem, void *callbackData, int panel);





/// HIFN  Main entry point
/// HIRET Return indicates if function was successful.
/// HIRET 0 success
/// HIRET -1 fail
int main (int argc, char *argv[])
{
	int error = 0;
	
	// start CVI Run-Time Engine
	if (!InitCVIRTE (0, argv, 0)) return -1;
	
	// load DAQLab environment resources
	//errChk ( )
	DAQLab_Load(); 
	
	


	// run GUI
	errChk ( RunUserInterface() ); 
	
	return 0;
	
	Error:
	
	return error;
}


/// HIFN  Loads DAQLab environment
/// HIRET Return indicates if function was successful.
/// HIRET 0 if success
/// HIRET negative value if fail
static int DAQLab_Load (void)
{
	int						error 			= 0;
	DAQLabModule_type** 	modulePtr;
	
	//---------------------------------------------------------------------------------
	// Load resources
	//---------------------------------------------------------------------------------
	
		// UI
	errChk ( mainPanHndl = LoadPanel (0, DAQLAB_UI_DAQLAB, MainPan) );
	if (DAQLAB_LOG_PANEL)
		errChk ( logPanHndl = LoadPanel (mainPanHndl, DAQLAB_UI_DAQLAB, LogPan) );
	
	errChk ( TasksUI.panHndl 			= LoadPanel (mainPanHndl, DAQLAB_UI_DAQLAB, TasksPan) );
	errChk ( TasksUI.controllerPanHndl 	= LoadPanel (mainPanHndl, TASKCONTROLLER_UI, TCPan1) );
	
		// Framework
	nullChk ( TasksUI.UItaskCtrls 		= ListCreate(sizeof(UITaskCtrl_type*)) );	// list with UI Task Controllers
	nullChk ( DAQLabModules   			= ListCreate(sizeof(DAQLabModule_type*)) );	// list with loaded DAQLab modules
	
	
	//---------------------------------------------------------------------------------
	// Adjust Panels
	//---------------------------------------------------------------------------------
	
	// maximize main panel by default
	SetPanelAttribute(mainPanHndl, ATTR_WINDOW_ZOOM, VAL_MAXIMIZE);
	
	// add menu bar to Tasks panel
	errChk (TasksUI.menuBarHndl			= NewMenuBar(TasksUI.panHndl) );
	errChk (TasksUI.menuID_Manage		= NewMenu(TasksUI.menuBarHndl, "Manage", -1) );
	errChk (TasksUI.menuItem_Add 		= NewMenuItem(TasksUI.menuBarHndl, TasksUI.menuID_Manage, "Add", -1, (VAL_MENUKEY_MODIFIER | 'A'), DAQLab_TaskMenu_CB, &TasksUI) );
	errChk (TasksUI.menuItem_Delete 	= NewMenuItem(TasksUI.menuBarHndl, TasksUI.menuID_Manage, "Delete", -1, (VAL_MENUKEY_MODIFIER | 'D'), DAQLab_TaskMenu_CB, &TasksUI) );
	errChk (TasksUI.menuItem_SubTasks	= NewMenuItem(TasksUI.menuBarHndl, TasksUI.menuID_Manage, "SubTasks", -1, (VAL_MENUKEY_MODIFIER | 'S'), DAQLab_TaskMenu_CB, &TasksUI) );
	// get height and width of Task Controller panel
	GetPanelAttribute(TasksUI.controllerPanHndl, ATTR_HEIGHT, &TasksUI.controllerPanHeight);
	GetPanelAttribute(TasksUI.controllerPanHndl, ATTR_WIDTH, &TasksUI.controllerPanWidth);
	
	
	
	
	
	DAQLab_RedrawTaskControllerUI();
	
	// display main panel
	DisplayPanel(mainPanHndl);
	
	DisplayPanel(TasksUI.panHndl); 
	
	if (DAQLAB_LOG_PANEL)
		DisplayPanel(logPanHndl);
	
	// init DAQLab DOM
	errChk ( DAQLab_NewXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl, &DAQLabCfg_RootElement) );
	
	
	/*
	// init modules list
	modules = DAQLabModule_populate();
	*/
	
	
	
	
	/*
	// load modules to workspace and make their panels visible
	for (int i = 1; i <= ListNumItems(modules); i++) {
		modulePtr = ListGetPtrToItem(modules, i);
		// load config data from XML to module structures
		(*(*modulePtr)->LoadCfg)(*modulePtr, DAQLabCfg_RootElement);
		
		// load module to workspace
		(*(*modulePtr)->Load)(*modulePtr, mainPanHndl);
		DAQLabModule_DisplayWorkspacePanels(*modulePtr, TRUE);
	}
	
	*/
	
	
	/*
	
	// add multiple XML elements to root element
	
	int 		dog_val = -1;
	float		cat_val = 4.0/3;
	DAQLabXMLNode children[] = { {"dog", DL_INT, &dog_val},
								 {"cat", DL_FLOAT, &cat_val}
															 };  
	
	DLAddXMLElem (DAQLabCfg_DOMHndl, DAQLabCfg_RootElement, children, NumElem(children));
	

	
	
	errChk ( DAQLab_SaveXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl) );   */
	
	

	
	
	
	return 0;
	
	Error:
	
	return error;
	
}

static int DAQLab_Close (void)
{   
	// save module specific info
	
	
	// dispose modules list
	DAQLabModule_empty (&DAQLabModules);
	
	QuitUserInterface(0);  
	
	return 0;
}


int CVICALLBACK CB_DAQLab_MainPan (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event == EVENT_CLOSE) DAQLab_Close ();
	
	return 0;
}


/// HIFN Installs callbackData and callbackFn to controls directly within panel.
/// HIPAR callbackData/ Callback data received by all controls in the panel. Set NULL if not used.
/// HIPAR callbackFn/ Callback function received by all controls in the panel. Set NULL if not used.
/// HIPAR panHndl/ Panel handle.
/// HIRET 0 if successful, negative number otherwise.
int	SetCtrlsInPanCBInfo (void* callbackData, CtrlCallbackPtr callbackFn, int panHndl)
{
	int 	error 					= 0;
	int		ctrlID;
	
	GetPanelAttribute(panHndl, ATTR_PANEL_FIRST_CTRL, &ctrlID);
	while (ctrlID) {
		// install callback data
		errChk( SetCtrlAttribute(panHndl, ctrlID, ATTR_CALLBACK_DATA, callbackData) );
		// install callback function
		errChk( SetCtrlAttribute(panHndl, ctrlID, ATTR_CALLBACK_FUNCTION_POINTER, callbackFn) );
		// get next control in the panel
		GetCtrlAttribute(panHndl, ctrlID, ATTR_NEXT_CTRL, &ctrlID);
	}
	
	Error:
	return error;
}

void DLMsg(const char* text, BOOL beep)
{
	if (!DAQLAB_LOG_PANEL) return;
	
	SetCtrlVal(logPanHndl, LogPan_LogBox, text);
	if (beep) Beep();
	
}

/// HIFN Displays predefined error and warning messages in the main workspace log panel.
/// HIPAR msgID/ If required, pass in additional data to format the message.
static void DAQLab_Msg (DAQLabMessageID msgID, void* data1, void* data2, void* data3, void* data4)
{
	char buff[1000]="";
	char buffCA[1000]="";
	
	switch (msgID) {
		
		case DAQLAB_MSG_WARN_NO_CFG_FILE:
			
			DLMsg("Warning: Loading DAQLab config file failed. Default config used instead.\nReason: ", 1);   
			CA_GetAutomationErrorString(*(HRESULT*)data2, buffCA, sizeof(buffCA));
			DLMsg(buffCA, 0);
			DLMsg("\n", 0);
			// additional error info
			if (*(HRESULT*)data2 == DISP_E_EXCEPTION) {
				CA_DisplayErrorInfo(*(CAObjHandle*)data1, NULL, *(HRESULT*)data2, (ERRORINFO*)data3); 
				DLMsg("\n\n", 0);
			} else
				DLMsg("\n", 0);
			
			break;
			
		case DAQLAB_MSG_ERR_DOM_CREATION:
			
			Fmt(buff, "Error: Could not create DAQLab config file \"%s\" XMLDOM. Quit software and resolve issue. \nReason: ", DAQLAB_CFG_FILE);
			CA_GetAutomationErrorString(*(HRESULT*)data1, buffCA, sizeof(buffCA));
			DLMsg(buff, 1);
			DLMsg(buffCA, 0);
			DLMsg("\n\n", 0);
			
			break;
		
		case DAQLAB_MSG_ERR_ACTIVEXML:
			
			CA_GetAutomationErrorString(*(HRESULT*)data2, buffCA, sizeof(buffCA));
			DLMsg(buffCA, 1);
			DLMsg("\n", 0);
			// additional error info
			if (*(HRESULT*)data2 == DISP_E_EXCEPTION) {
				CA_DisplayErrorInfo(*(CAObjHandle*)data1, NULL, *(HRESULT*)data2, (ERRORINFO*)data3); 
				DLMsg("\n\n", 0);
			} else
				DLMsg("\n", 0);
			
			break;
			
		case DAQLAB_MSG_OK_DOM_CREATION:
			
			DLMsg("XML DOM created successfully.\n\n", 0);
			
			break;
			
	}
	
	
}


/// HIFN Creates an MSXML DOM object using ActiveX connection to Microsoft XML 6.0 from a specified .xml file.
/// HIFN If no .xml file is found, an empty DOM is created.
/// HIPAR fileName/ String of XML file name, including .xml file extension.
/// HIRET New DOM object handle from loaded .xml file or an empty DOM if file not found or not specified.
/// HIRET New Topmost (parent) element handle of the DOM to which other elements and attributes can be added.   
/// OUT xmlDOM 
/// OUT rootElement 
static int DAQLab_NewXMLDOM (const char fileName[], CAObjHandle* xmlDOM, ActiveXMLObj_IXMLDOMElement_* rootElement)
{
	HRESULT								xmlerror;
	ERRORINFO							xmlERRINFO;
	VBOOL								xmlLoaded;
	BOOL								rootElementNameOK		= FALSE;
	ActiveXMLObj_IXMLDOMElement_		newElement;
	BSTR								bstrFileName;
	double 								t1, t2;
	char								timeStr[10];
	char								msg[100];
	char*								rootElementName;
	
	
	// create new ActiveX DOM object
	if ( (xmlerror = ActiveXML_NewDOMDocument60IXMLDOMDocument3_ (NULL, 1, LOCALE_NEUTRAL, 0, xmlDOM)) < 0 ) {
			DAQLab_Msg(DAQLAB_MSG_ERR_DOM_CREATION, &xmlerror, 0, 0, 0);
			return xmlerror;
		} else 
			DAQLab_Msg(DAQLAB_MSG_OK_DOM_CREATION, 0, 0, 0, 0);
	
	// load DAQLab XML config file to DOM or create new DOM if file not found
	DLMsg("Loading XML file ", 0);
	DLMsg(fileName, 0);
	DLMsg(" ...", 0);
	
	t1 = Timer();     
	
	CA_CStringToBSTR(fileName, &bstrFileName); 
	
	// load XML file
	xmlerror = ActiveXML_IXMLDOMDocument3_load (*xmlDOM, &xmlERRINFO, CA_VariantBSTR(bstrFileName), &xmlLoaded);
	t2 = Timer();
	
	if (xmlLoaded == VTRUE)	{
		DLMsg("loaded in ", 0);
		Fmt(timeStr, "%s<%f[p2] s.\n\n", t2 - t1);
		DLMsg(timeStr, 0);
	} else
		DAQLab_Msg(DAQLAB_MSG_WARN_NO_CFG_FILE, xmlDOM, &xmlerror, &xmlERRINFO, 0);
	
	// if file was loaded check root element name if it matches the name defined in DAQLAB_CFG_DOM_ROOT_NAME 
	if (xmlLoaded == VARIANT_TRUE) {
		// get document root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_GetdocumentElement (*xmlDOM, &xmlERRINFO, rootElement) );
		
		// check if root element name is correct
		XMLErrChk ( ActiveXML_IXMLDOMElement_GettagName(*rootElement, &xmlERRINFO, &rootElementName) );
		
		if(strcmp(rootElementName, DAQLAB_CFG_DOM_ROOT_NAME)) {
			rootElementNameOK = FALSE;
			Fmt(msg, "Root element %s was not found. Empty DOM will be used instead.\n\n", DAQLAB_CFG_DOM_ROOT_NAME);
			DLMsg(msg, 1);
		} else
			rootElementNameOK = TRUE;
	} 
	
	if (xmlLoaded == VARIANT_FALSE || !rootElementNameOK) {
		// create root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (*xmlDOM, &xmlERRINFO, DAQLAB_CFG_DOM_ROOT_NAME, &newElement) );
	
		// append root element to DOM and return handle to root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(*xmlDOM, &xmlERRINFO, newElement, rootElement) );
	} 
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, xmlDOM, &xmlerror, &xmlERRINFO, 0);
	
	return xmlerror;
	
}

static int	DAQLab_SaveXMLDOM (const char fileName[], CAObjHandle* xmlDOM)
{
	HRESULT				xmlerror;
	ERRORINFO			xmlERRINFO;
	BSTR				bstrFileName;
	double				t1, t2;
	char				timeStr[10]; 
	
	// save XML file
	DLMsg("Saving XML file ", 0);
	DLMsg(fileName, 0);
	DLMsg("... ", 0);
	
	t1 = Timer();
	CA_CStringToBSTR(fileName, &bstrFileName); 
	// write file
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_save(*xmlDOM, &xmlERRINFO, CA_VariantBSTR(bstrFileName)) );
	
	t2 = Timer();
	
	DLMsg("saved in ", 0);
	Fmt(timeStr, "%s<%f[p2] s.\n\n", t2 - t1);
	DLMsg(timeStr, 0);
	
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, xmlDOM, &xmlerror, &xmlERRINFO, 0);
	
	return xmlerror;
}

int	DLAddXMLElem (CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLElements[], size_t nElements)
{
	HRESULT							xmlerror;
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMElement_	newXMLElement;
	VARIANT							xmlVal;
	BSTR							bstrVal;
	
	for (int i = 0; i < nElements; i++) {
		
		// create new element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, childXMLElements[i].tag, &newXMLElement) );
		
		// convert to variant
		switch (childXMLElements[i].type) {
			
			case DL_NULL:
				
				xmlVal = CA_VariantNULL ();
				break;
			
			case DL_BOOL:
				
				xmlVal = CA_VariantBool ((VBOOL)*(BOOL*)childXMLElements[i].pData);
				break;
				
			case DL_CHAR:
				
				xmlVal = CA_VariantChar(*(char*)childXMLElements[i].pData);
				break;
				
			case DL_UCHAR:
				
				xmlVal = CA_VariantUChar(*(unsigned char*)childXMLElements[i].pData);
				break;
				
			case DL_SHORT:
				
				xmlVal = CA_VariantShort(*(short*)childXMLElements[i].pData);
				break;
				
			case DL_USHORT:
				
				xmlVal = CA_VariantUShort(*(unsigned short*)childXMLElements[i].pData);
				break;
			
			case DL_INT:
				
				xmlVal = CA_VariantInt(*(int*)childXMLElements[i].pData);
				break;
				
			case DL_UINT:
				
				xmlVal = CA_VariantUInt(*(unsigned int*)childXMLElements[i].pData);
				break;
				
			case DL_LONG:
				
				xmlVal = CA_VariantLong(*(long*)childXMLElements[i].pData);
				break;
			
			case DL_ULONG:
				
				xmlVal = CA_VariantULong(*(unsigned long*)childXMLElements[i].pData);
				break;
				
			case DL_LONGLONG:
				
				xmlVal = CA_VariantLongLong(*(long long*)childXMLElements[i].pData);
				break;
				
			case DL_ULONGLONG:
				
				xmlVal = CA_VariantULongLong(*(unsigned long long*)childXMLElements[i].pData);
				break;
			
			case DL_FLOAT:
				
				xmlVal = CA_VariantFloat(*(float*)childXMLElements[i].pData);
				break;
			
			case DL_DOUBLE:
				
				xmlVal = CA_VariantDouble(*(double*)childXMLElements[i].pData);
				break;
				
			case DL_CSTRING:
				
				CA_CStringToBSTR((char*)childXMLElements[i].pData, &bstrVal); 
				xmlVal = CA_VariantBSTR(bstrVal);
				break;
		}
		
		// set element typed value
		XMLErrChk ( ActiveXML_IXMLDOMElement_SetnodeTypedValue(newXMLElement, &xmlERRINFO, xmlVal) );
		
		// add new element as child
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, &xmlERRINFO, newXMLElement, NULL) );
	}
	
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &xmlDOM, &xmlerror, &xmlERRINFO, 0);
	
	return xmlerror;
}

static UITaskCtrl_type*	DAQLab_init_UITaskCtrl_type (TaskControl_type* taskControl)
{
	int error = 0;
	UITaskCtrl_type* newUItaskCtrl = malloc(sizeof(UITaskCtrl_type));
	if (!newUItaskCtrl) return NULL;
	
	// duplicate Task Controller panel
	errChk( newUItaskCtrl->panHndl		= DuplicatePanel(TasksUI.panHndl, TasksUI.controllerPanHndl, "", 0, DAQLAB_TASKCONTROLLERS_PAN_MARGIN) );
	newUItaskCtrl->taskControl 			= taskControl;
	// add taskControl data and callback to handle events
	SetCtrlsInPanCBInfo(taskControl, DAQLab_TaskControllers_CB, newUItaskCtrl->panHndl);
	// set UI Task Controller name
	SetCtrlVal(newUItaskCtrl->panHndl, TCPan1_Name, GetTaskControlName(taskControl));  
	
	return newUItaskCtrl;
	
	Error:
	
	OKfree(newUItaskCtrl);
	
	return NULL;
}

static void	DAQLab_discard_UITaskCtrl_type	(UITaskCtrl_type** a)
{
	if (!*a) return;
	// remove Task Controller resources
	discard_TaskControl_type(&(*a)->taskControl);
	DiscardPanel((*a)->panHndl);
	
	OKfree(*a);
}

/// HIFN Adds a Task Controller to the user interface.
/// HIRET 0 for success, -1 error.
static UITaskCtrl_type* DAQLab_AddTaskControllerToUI (TaskControl_type* taskControl)    
{
	int					error 		= 0;
	UITaskCtrl_type*	newUItaskCtrlPtr;
	
	if (!taskControl) return NULL;
	
	newUItaskCtrlPtr = DAQLab_init_UITaskCtrl_type(taskControl);
	
	nullChk( ListInsertItem(TasksUI.UItaskCtrls, &newUItaskCtrlPtr, END_OF_LIST) );
	
	DAQLab_RedrawTaskControllerUI();

	return newUItaskCtrlPtr;
	
	Error:
	
	DAQLab_discard_UITaskCtrl_type(&newUItaskCtrlPtr);
	
	return NULL;
}

/// HIFN Removes a Task Controller from the user interface.
/// HIPAR index/ provide a 1-based index for the Task Controller in the TasksUI.UItaskCtrls list 
/// HIRET 0 for success, -1 function error, -2 cannot remove Task Controllers in the following states: RUNNING, STOPPING, ITERATING, WAITING.
static int DAQLab_RemoveTaskControllerFromUI (int index)
{
	UITaskCtrl_type**	UItaskCtrlPtrPtr = ListGetPtrToItem (TasksUI.UItaskCtrls, index);
	
	if (!UItaskCtrlPtrPtr) return -1;
	if (!*UItaskCtrlPtrPtr) return -1;
	TaskStates_type	state = GetTaskControlState((*UItaskCtrlPtrPtr)->taskControl);
	
	if (state == TASK_STATE_RUNNING || state == TASK_STATE_STOPPING || state == TASK_STATE_ITERATING || state == TASK_STATE_WAITING)
		return -2;
	
	DAQLab_discard_UITaskCtrl_type(UItaskCtrlPtrPtr);
	
	ListRemoveItem(TasksUI.UItaskCtrls, 0, index);
	
	DAQLab_RedrawTaskControllerUI();
	
	return 0;
}

static void	DAQLab_RedrawTaskControllerUI (void)
{
	size_t					nControllers = ListNumItems(TasksUI.UItaskCtrls);
	UITaskCtrl_type**		controllerPtrPtr;
	int						menubarHeight;
	
	GetPanelAttribute(TasksUI.panHndl, ATTR_MENU_HEIGHT, &menubarHeight);
	
	if (!nControllers) {
		// adjust size of Tasks panel to match the size of one Task Controller panel
		SetPanelAttribute(TasksUI.panHndl, ATTR_HEIGHT, TasksUI.controllerPanHeight + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);
		SetPanelAttribute(TasksUI.panHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN); 
		
		// dimm delete and subtasks menu items since there are no Task Controllers
		SetMenuBarAttribute(TasksUI.menuBarHndl, TasksUI.menuItem_Delete, ATTR_DIMMED, 1);
		SetMenuBarAttribute(TasksUI.menuBarHndl, TasksUI.menuItem_SubTasks, ATTR_DIMMED, 1);
		
		return;
	}
	
	// undim delete and subtasks menu items 
	SetMenuBarAttribute(TasksUI.menuBarHndl, TasksUI.menuItem_Delete, ATTR_DIMMED, 0);
	SetMenuBarAttribute(TasksUI.menuBarHndl, TasksUI.menuItem_SubTasks, ATTR_DIMMED, 0);
	
	// reposition Task Controller panels and display them
	for (int i = 1; i <= nControllers; i++) {
		controllerPtrPtr = ListGetPtrToItem(TasksUI.UItaskCtrls, i);
		SetPanelAttribute((*controllerPtrPtr)->panHndl, ATTR_TOP, (TasksUI.controllerPanHeight + DAQLAB_TASKCONTROLLERS_SPACING) * (i-1) + DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);  
		DisplayPanel((*controllerPtrPtr)->panHndl);
	}
	
	// adjust Tasks panel height and insert scrollbar if necessary, in which case, adjust also the width
	if (nControllers > DAQLAB_NVISIBLE_TASKCONTROLLERS) {
		
		// adjust size of Tasks panel to its maximum
		SetPanelAttribute(TasksUI.panHndl, ATTR_HEIGHT, TasksUI.controllerPanHeight * DAQLAB_NVISIBLE_TASKCONTROLLERS + 
						  DAQLAB_TASKCONTROLLERS_SPACING * (DAQLAB_NVISIBLE_TASKCONTROLLERS - 1)  + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);
		// increase width and add scrollbars
		SetPanelAttribute(TasksUI.panHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + VAL_LARGE_SCROLL_BARS);
		SetPanelAttribute(TasksUI.panHndl, ATTR_SCROLL_BARS, VAL_VERT_SCROLL_BAR);
		
	} else {
		
		// adjust size of Tasks panel to match the size of one Task Controller panel
		SetPanelAttribute(TasksUI.panHndl, ATTR_HEIGHT, TasksUI.controllerPanHeight * nControllers + DAQLAB_TASKCONTROLLERS_SPACING * (nControllers - 1)  
							+ 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);
		SetPanelAttribute(TasksUI.panHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN); 
		
		// return to normal width and remove scroll bars
		SetPanelAttribute(TasksUI.panHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN);
		SetPanelAttribute(TasksUI.panHndl, ATTR_SCROLL_BARS, VAL_NO_SCROLL_BARS);
		
	}
	
}

static void CVICALLBACK DAQLab_TaskMenu_CB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	// menu item Add
	if (menuItemID == TasksUI.menuItem_Add) DAQLab_TaskMenu_AddTaskController();
		
	// menu item Delete
	if (menuItemID == TasksUI.menuItem_Delete ) DAQLab_TaskMenu_DeleteTaskController();
		
	// menu item SubTasks
	if (menuItemID == TasksUI.menuItem_SubTasks) DAQLab_TaskMenu_ManageSubTasks();
	
}

void CVICALLBACK DAQLab_ManageModules_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	return;
}


static void	DAQLab_TaskMenu_AddTaskController 	(void) 
{
	
	char 				newControllerName[DAQLAB_MAX_UITASKCONTROLLER_NAME_NCHARS+1]; 
	char				msg1[]													= "Enter name.";
	char				msg2[]													= "Invalid name. Enter new name.";
	char				msg3[]													= "Names must be different. Enter new name.";
	char*				popupAddControllerMsg;								 
	int					selectedBTTN;
	BOOL 				foundFlag;
	UITaskCtrl_type**	UITaskCtrlsPtrPtr;
	UITaskCtrl_type*	UITaskCtrlsPtr;
	TaskControl_type* 	newTaskControllerPtr;
	
	popupAddControllerMsg = msg1; 
	do {
		// obtain new Task Controller name if one is given, else nothing happens
		selectedBTTN = GenericMessagePopup ("New Task Controller", popupAddControllerMsg, "Ok", "Cancel", "", newControllerName, DAQLAB_MAX_UITASKCONTROLLER_NAME_NCHARS * sizeof(char),
											 0, VAL_GENERIC_POPUP_INPUT_STRING,VAL_GENERIC_POPUP_BTN1,VAL_GENERIC_POPUP_BTN2);
		// if OK and name is not valid
		if ( (selectedBTTN == VAL_GENERIC_POPUP_BTN1) && (!strcmp (newControllerName,"")) ) {
			popupAddControllerMsg = msg2;
			continue;  // display again popup to enter new name
		} else
			// if cancelled
			if (selectedBTTN == VAL_GENERIC_POPUP_BTN2) return;  // do nothing
			
		// name is valid, but there may be another Task Controller with the same name already, which is not allowed
			
		// remove surrounding white spaces 
		RemoveSurroundingWhiteSpace(newControllerName);
		
		// check if there is already another Task Controller with the same name
		foundFlag = FALSE;
		for (size_t i = 1; i <= ListNumItems(TasksUI.UItaskCtrls); i++) {
			UITaskCtrlsPtrPtr = ListGetPtrToItem(TasksUI.UItaskCtrls, i);
			if (strcmp (GetTaskControlName((*UITaskCtrlsPtrPtr)->taskControl), newControllerName) == 0) {
				foundFlag = TRUE;
				break;
			}
		}
		
		// if another Task Controller with the same name exists display popup again
		popupAddControllerMsg = msg3;
		
	} while (foundFlag);
		
	// create new task controller
	newTaskControllerPtr = init_TaskControl_type (newControllerName, DAQLab_ConfigureUITC, DAQLab_IterateUITC, DAQLab_StartUITC, 
												  DAQLab_ResetUITC, DAQLab_DoneUITC, DAQLab_StoppedUITC, NULL, NULL, DAQLab_ErrorUITC); 
	
	
	if (!newTaskControllerPtr) {
		DLMsg("Error: Task Controller could not be created.\n\n", 1);
		return;
	};
	
	// add new Task Controller to the environment
	UITaskCtrlsPtr = DAQLab_AddTaskControllerToUI(newTaskControllerPtr);
	
	// attach callback data to the Task Controller
	SetTaskControlModuleData(newTaskControllerPtr, UITaskCtrlsPtr);
	// send a configure event to the Task Controller
	TaskControlEvent(newTaskControllerPtr, TASK_EVENT_CONFIGURE, NULL, NULL);
	
}

static void	DAQLab_TaskMenu_DeleteTaskController (void)
{
	int					delPanHndl;
	int					error;
	UITaskCtrl_type** 	UITaskCtrlsPtrPtr;
	
	errChk( delPanHndl = LoadPanel(0, DAQLAB_UI_DAQLAB, TCDelPan) );
	
	// list Task Controllers
	
	for (size_t i = 1; i <= ListNumItems(TasksUI.UItaskCtrls); i++) {
		UITaskCtrlsPtrPtr = ListGetPtrToItem(TasksUI.UItaskCtrls, i);
		InsertListItem(delPanHndl, TCDelPan_TaskControllers, -1, GetTaskControlName((*UITaskCtrlsPtrPtr)->taskControl), i); 
	}
	
	// display popup panel
	InstallPopup(delPanHndl);
	
	Error:
	
}


static void	DAQLab_TaskMenu_ManageSubTasks	(void)
{
	
}

int CVICALLBACK DAQLab_TCDelPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_CLOSE:
			
			DiscardPanel(panel);

			break;
	}
	
	return 0;
}

int CVICALLBACK DAQLab_DelTaskControllersBTTN_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			size_t	idx;	 // 0-based list index
			int		result;
			int		nitems;
			
			// get selected Task Controller
			GetCtrlIndex(panel, TCDelPan_TaskControllers, &idx); 
			if (idx <0) return 0; // no items in the list
			
			result = DAQLab_RemoveTaskControllerFromUI (idx+1);
			if (result == -2) {
				MessagePopup("Delete Task Controller", "Cannot delete Task Controllers in the following states:\n RUNNING, STOPPING, ITERATING or WAITING.");
				return 0;
			}
			else
				if (result == -1) {
					DLMsg("Error: Cannot delete Task Controller from list.\n\n", 1);
					return 0;
				}
				
			DeleteListItem(panel, TCDelPan_TaskControllers, idx, 1);
			
			// if there are no more items in the list, then discard the panel
			GetNumListItems(panel, TCDelPan_TaskControllers, &nitems);
			if (!nitems) DiscardPanel(panel);
			
			break;
	}
	return 0;
}

static int CVICALLBACK DAQLab_TaskControllers_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskControl_type* 	taskControl = callbackData;
	BOOL             	starttask;					  // 1 - task start, 0 - task stop 
	double				waitDVal;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
			
				case TCPan1_Repeat:
				case TCPan1_Reset:
					
					TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
					
					break;
					
				case TCPan1_StartStop:
					
					GetCtrlVal(panel,control,&starttask);
					if (starttask) {
						// dim controls
						SetCtrlAttribute(panel, TCPan1_Repeat, ATTR_DIMMED, 1);
						SetCtrlAttribute(panel, TCPan1_Wait, ATTR_DIMMED, 1);
						SetCtrlAttribute(panel, TCPan1_Reset, ATTR_DIMMED, 1);
						// undim abort button
						SetCtrlAttribute(panel, TCPan1_Abort, ATTR_DIMMED, 0);
						// send start task event
						TaskControlEvent(taskControl, TASK_EVENT_START, NULL, NULL);
					}
					else {
						// dim button so the user cannot start the task again until it stops
						SetCtrlAttribute(panel, TCPan1_StartStop, ATTR_DIMMED, 1);
						TaskControlEvent(taskControl, TASK_EVENT_STOP, NULL, NULL);
					}
						
					break;
					
				case TCPan1_Abort:
					
					AbortTaskControlExecution (taskControl);
					
					break;
					
				case TCPan1_Wait:
					
					GetCtrlVal(panel, TCPan1_Wait, &waitDVal);  
					SetTaskControlIterationsWait(taskControl, waitDVal);
					
					break;
					
			}
			
			break;
	}
	
	return 0;
}

//-----------------------------------------
// UI Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* DAQLab_ConfigureUITC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	unsigned int		repeat;
	
	// change Task Controller name background color to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// undim Repeat button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// undim Start button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Repeat, &repeat);
	SetTaskControlIterations(taskControl, repeat);
	
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* DAQLab_IterateUITC	(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// make a test error
	if (currentIteration > 5) return init_FCallReturn_type(-2, "DAQLab_IterateUITC", "Test", 0);
	
	// iteration complete, update current iteration number
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, currentIteration + 1);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* DAQLab_StartUITC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from gray (0x00F0F0F0) to green (0x002BD22F)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x002BD22F);
	ProcessSystemEvents();	// required to update the background color
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* DAQLab_DoneUITC  (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from green (0x002BD22F) to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// switch Stop button back to Start button
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_StartStop, 0);
	// undim Start/Stop button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	// dim Abort button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 1);
	// undim Reset button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	// undim Repeat button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// update iterations display
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, currentIteration);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* DAQLab_ResetUITC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, 0);
	
	return init_FCallReturn_type(0, "", "", 0); 
}

static FCallReturn_type* DAQLab_StoppedUITC	(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from green (0x002BD22F) to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// switch Stop button back to Start button
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_StartStop, 0);
	// dim Abort button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 1);
	// undim Reset button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	// undim Repeat button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// undim Start/Stop button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	// update iterations display
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, currentIteration);
	
	return init_FCallReturn_type(0, "", "", 0); 
}

static FCallReturn_type* DAQLab_ErrorUITC (TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color to light red (0x00FF3333)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00FF3333);
	// print error info and make some noise
	DLMsg(errorMsg, 1);
	DLMsg("\n\n", 0);
	// switch Stop button back to Start button
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_StartStop, 0);
	// dim Start button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 1);
	// dim Abort button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 1);
	// undim Reset button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	 
	
	
	return init_FCallReturn_type(0, "", "", 0); 
}


/*
	// create root element node
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DOMHndl, &xmlERRINFO, "DAQLab", &DAQLab_root) );
	
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createNode(DOMHndl, &xmlERRINFO, CA_VariantInt(NODE_ELEMENT), "root", "", &DAQLab_rootNode) ); 
	
	// create attribute node
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createAttribute (DOMHndl, &xmlERRINFO, "Size", &attrNode) );
	
	// add double value to attribute node
	XMLErrChk ( ActiveXML_IXMLDOMAttribute_Setvalue (attrNode, &xmlERRINFO, CA_VariantDouble(3.141)) );
	
	
	// set element attribute
	XMLErrChk ( ActiveXML_IXMLDOMElement_setAttribute(DAQLab_rootNode, &xmlERRINFO, "Size", CA_VariantDouble(3.141)) ); 
	
		/*   this works */
	/*
	t1 = Timer();
	
	// create root element by loading XML syntacs directly to DOM
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_loadXML (DOMHndl, &xmlERRINFO, "<root></root>", &XMLrootCreated) );
	
	// get document root element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_GetdocumentElement (DOMHndl, &xmlERRINFO, &DAQLab_rootElement) );
	
	// add attribute to root element
	XMLErrChk ( ActiveXML_IXMLDOMElement_setAttribute(DAQLab_rootElement, &xmlERRINFO, "Size", CA_VariantDouble(3.141)) ); 
	/*	*/
			/*
	t2 = Timer();
		
	diff = (t2 - t1) * 1000; // in ms
	
	/
	// create root element
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DOMHndl, &xmlERRINFO, "DAQLab", &DAQLab_rootElement) );
	
	// append root element to DOM
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(DOMHndl, &xmlERRINFO, DAQLab_rootElement, &DAQLab_rootNode) )
	
	*/


/*


/// HIFN  Returns a list of available modules that are loaded from DAQLabModules_InitFunctions[] declared in DAQLabModule.c 
/// HIRET Returns non-zero list handle if successful
/// HIRET 0 if error occured
ListType DAQLabModule_populate (void)
{
	ListType modules = ListCreate(sizeof(DAQLabModule_type*));
	if (!modules) return 0;
	
	DAQLabModule_type* modptr;
	for (int i = 0; i < NumElem(DAQLabModules_InitFunctions); i++) {
		modptr = (*DAQLabModules_InitFunctions[i])(NULL);
		ListInsertItem(modules, &modptr, END_OF_LIST);
	}
	
	return modules;
}

*/
