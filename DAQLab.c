//==============================================================================
//
// Title:		DAQLab.c
// Purpose:		A short description of the implementation.
//
// Created on:	8-3-2014 at 13:48:00 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
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

#include "PIStage.h"
#include "VUPhotonCtr.h"



//==============================================================================
// Constants
	
	// DAQLab main workspace UI resource
#define DAQLAB_UI_DAQLAB							"UI_DAQLab.uir"
	// name of config file
#define DAQLAB_CFG_FILE								"DAQLabCfg.xml"

#define DAQLAB_CFG_DOM_ROOT_NAME					"DAQLabConfig"
#define DAQLAB_UITASKCONTROLLER_XML_TAG				"UITaskController"
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
	// maximum DAQLab module instance name length
#define DAQLAB_MAX_MODULEINSTANCE_NAME_NCHARS		50

//==============================================================================
// Types

//------------------------------------------------------------------------------------------------
//                              AVAILABLE DAQLab MODULES
//------------------------------------------------------------------------------------------------

// Constant function pointer that is used to launch the initialization of different DAQLabModules 
typedef DAQLabModule_type* (* const ModuleInitAllocFunction_type ) (DAQLabModule_type* mod, char className[], char instanceName[]);

typedef struct {
	
	char*							className;
	ModuleInitAllocFunction_type	ModuleInitFptr;
	BOOL							multipleInstancesFlag;	  // allows multiple instances of the module
	size_t							nInstances;				  // counter for the total number of instances

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
	// errors								// to pass in DAQLab_Msg
	DAQLAB_MSG_ERR_DOM_CREATION,			// HRESULT* error code, 0, 0, 0
	DAQLAB_MSG_ERR_ACTIVEXML,				// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info, 0
	DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES, // HRESULT* error code, 0, 0, 0
	DAQLAB_MSG_ERR_LOADING_MODULE,			// char* module instance name, char* error description
	DAQLAB_MSG_ERR_VCHAN_NOT_FOUND,			// VChan_type*, 0, 0, 0
	
	// warnings									
	DAQLAB_MSG_WARN_NO_CFG_FILE,			// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info, 0       
	
	// success									
	DAQLAB_MSG_OK_DOM_CREATION				// 0, 0, 0, 0 
	

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
AvailableDAQLabModules_type DAQLabModules_InitFunctions[] = {	  // set last parameter, i.e. the instance
																  // counter always to 0
	//{ MOD_PIStage_NAME, initalloc_PIStage, FALSE, 0 },
	{ MOD_VUPhotonCtr_NAME, initalloc_VUPhotonCtr, FALSE, 0 }
	
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
	
	// Different modules that inherit from DAQLabModule_type 
	// List elements of DAQLabModule_type* type
ListType		DAQLabModules				= 0;

	// Virtual channels from all the modules that register such channels with the framework
	// List elements of VChan_type* type
ListType		VChannels					= 0;

							

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

static int					DAQLab_SaveXMLEnvironmentConfig			(void);

static int					DAQLab_VariantToType					(VARIANT* variantVal, DAQLabXMLTypes vartype, void* value);

static BOOL					DAQLab_ValidControllerName				(char name[], void* listPtr);

static BOOL					DAQLab_ValidModuleName					(char name[], void* listPtr);  



//-----------------------------------------
// UI Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	DAQLab_ConfigureUITC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static void					DAQLab_IterateUITC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_StartUITC					(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_DoneUITC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	DAQLab_StoppedUITC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	DAQLab_ResetUITC 					(TaskControl_type* taskControl, BOOL const* abortFlag); 
static void				 	DAQLab_ErrorUITC 					(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

void CVICALLBACK 			DAQLab_MenuModules_CB					(int menuBar, int menuItem, void *callbackData, int panel);





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
	int								error 					= 0;
	BOOL							FoundDAQLabSettingsFlag	= 0;		// if FALSE, no XML settings were found and default will be used
	HRESULT							xmlerror;  
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMNodeList_	xmlUITaskControlers;
	ActiveXMLObj_IXMLDOMNode_		xmlTaskControllerNode;
	long							nUITaskControlers;
									// DAQLab UI data
	int								taskPanTopPos;
	int								taskPanLeftPos;
	int								logPanTopPos;
	int								logPanLeftPos;
	char*							UITCName				= NULL;
	unsigned int					UITCIterations;
	double							UITCWait;
	BOOL							UITCMode;
	TaskControl_type*				newTaskControllerPtr;
	UITaskCtrl_type*				UITaskCtrlsPtr;
									// template to load DAQLab Environment parameters
	DAQLabXMLNode 					attr1[] = { {"TaskPanTopPos", DL_INT, &taskPanTopPos},
							 					{"TaskPanLeftPos", DL_INT, &taskPanLeftPos},
												{"LogPanTopPos", DL_INT, &logPanTopPos},
												{"LogPanLeftPos", DL_INT, &logPanLeftPos} };
									// template to load UI Task Controller parameters			
	DAQLabXMLNode					attr2[] = {	{"Name", DL_CSTRING, &UITCName},
												{"Iterations", DL_UINT, &UITCIterations},
												{"Wait", DL_DOUBLE, &UITCWait}, 
												{"Mode", DL_BOOL, &UITCMode} };
										
	
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
	nullChk ( VChannels		   			= ListCreate(sizeof(VChan_type*)) );		// list with Virtual Channels
	
	
	//---------------------------------------------------------------------------------
	// Adjust Panels
	//---------------------------------------------------------------------------------
	
	// maximize main panel by default
	SetPanelAttribute(mainPanHndl, ATTR_WINDOW_ZOOM, VAL_MAXIMIZE);
	
	// center Tasks panel and Log panel by default (their positions will be overriden if a valid XML config is found)
	SetPanelAttribute(TasksUI.panHndl, ATTR_LEFT, VAL_AUTO_CENTER);
	SetPanelAttribute(TasksUI.panHndl, ATTR_TOP, VAL_AUTO_CENTER);
	SetPanelAttribute(logPanHndl, ATTR_LEFT, VAL_AUTO_CENTER);
	SetPanelAttribute(logPanHndl, ATTR_TOP, VAL_AUTO_CENTER);
	
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
	errChk ( FoundDAQLabSettingsFlag = DAQLab_NewXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl, &DAQLabCfg_RootElement) );
	
	// skip loading of settings if none are found
	if (!FoundDAQLabSettingsFlag) goto DAQLabNoSettings;
	
	// settings were found, apply settings to UI panels
	DLGetXMLElementAttributes(DAQLabCfg_RootElement, attr1, NumElem(attr1));  
	// apply loaded panel positions
	errChk( SetPanelAttribute(TasksUI.panHndl, ATTR_LEFT, taskPanLeftPos) );
	errChk( SetPanelAttribute(TasksUI.panHndl, ATTR_TOP, taskPanTopPos) );
	errChk( SetPanelAttribute(logPanHndl, ATTR_LEFT, logPanLeftPos) );
	errChk( SetPanelAttribute(logPanHndl, ATTR_TOP, logPanTopPos) );
	
	// load UI Task Controller settings
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(DAQLabCfg_RootElement, &xmlERRINFO, DAQLAB_UITASKCONTROLLER_XML_TAG, &xmlUITaskControlers) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(xmlUITaskControlers, &xmlERRINFO, &nUITaskControlers) );
	for (int i = 0; i < nUITaskControlers; i++) {
		// get xml Task Controller Node
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlUITaskControlers, &xmlERRINFO, i, &xmlTaskControllerNode) );
		// read in attributes
		errChk( DLGetXMLNodeAttributes(xmlTaskControllerNode, attr2, NumElem(attr2)) ); 
		// create new UI Task Controller
		newTaskControllerPtr = init_TaskControl_type (UITCName, DAQLab_ConfigureUITC, DAQLab_IterateUITC, DAQLab_StartUITC, 
												 	  DAQLab_ResetUITC, DAQLab_DoneUITC, DAQLab_StoppedUITC, NULL, NULL, DAQLab_ErrorUITC); 
		if (!newTaskControllerPtr) {
			DLMsg("Error: Task Controller could not be created.\n\n", 1);
			return -1;
		}

		// set UITC iterations
		SetTaskControlIterations(newTaskControllerPtr, UITCIterations);
		// set UITC wait between iterations
		SetTaskControlIterationsWait(newTaskControllerPtr, UITCWait);
		// set UITC mode (finite/continuous)
		SetTaskControlMode(newTaskControllerPtr, UITCMode);
		// add new Task Controller to the environment
		UITaskCtrlsPtr = DAQLab_AddTaskControllerToUI(newTaskControllerPtr);
		// attach callback data to the Task Controller
		SetTaskControlModuleData(newTaskControllerPtr, UITaskCtrlsPtr);
		// send a configure event to the Task Controller
		TaskControlEvent(newTaskControllerPtr, TASK_EVENT_CONFIGURE, NULL, NULL);
	}
	
	// discard DOM after loading all settings
	CA_DiscardObjHandle (DAQLabCfg_DOMHndl);
	
	DAQLabNoSettings:
	
	return 0;
	
	Error:
	
	return error;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &DAQLabCfg_DOMHndl, &xmlerror, &xmlERRINFO, 0);
	
	return xmlerror;
	
}


static int	DAQLab_SaveXMLEnvironmentConfig	(void)
{
	int								error;
	HRESULT							xmlerror; 
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMElement_	newXMLElement; 
	
	// create new DOM
	XMLErrChk ( ActiveXML_NewDOMDocument60IXMLDOMDocument3_ (NULL, 1, LOCALE_NEUTRAL, 0, &DAQLabCfg_DOMHndl) );
	// create new DAQLab Config root element and append it to the DOM
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DAQLabCfg_DOMHndl, &xmlERRINFO, DAQLAB_CFG_DOM_ROOT_NAME, &newXMLElement) );
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(DAQLabCfg_DOMHndl, &xmlERRINFO, newXMLElement, &DAQLabCfg_RootElement) );
	
	// save Tasks and Log panel positions
	int								taskPanTopPos;
	int								taskPanLeftPos;
	int								logPanTopPos;
	int								logPanLeftPos;
	DAQLabXMLNode 					attr1[] = {	{"TaskPanTopPos", DL_INT, &taskPanTopPos},
								 				{"TaskPanLeftPos", DL_INT, &taskPanLeftPos},
												{"LogPanTopPos", DL_INT, &logPanTopPos},
												{"LogPanLeftPos", DL_INT, &logPanLeftPos}	};  
	
	errChk( GetPanelAttribute(TasksUI.panHndl, ATTR_LEFT, &taskPanLeftPos) );
	errChk( GetPanelAttribute(TasksUI.panHndl, ATTR_TOP, &taskPanTopPos) );
	errChk( GetPanelAttribute(logPanHndl, ATTR_LEFT, &logPanLeftPos) );
	errChk( GetPanelAttribute(logPanHndl, ATTR_TOP, &logPanTopPos) );
	
	errChk( DLAddToXMLElem (DAQLabCfg_DOMHndl, DAQLabCfg_RootElement, attr1, DL_ATTRIBUTE, NumElem(attr1)) );
	
	// save UI Task Controllers
	UITaskCtrl_type**				UItaskCtrlPtrPtr;
	DAQLabXMLNode 					attr2[4];
	int								niter;
	double							wait;
	BOOL							mode;
	
	for (int i = 1; i <= ListNumItems(TasksUI.UItaskCtrls); i++) {
		UItaskCtrlPtrPtr = ListGetPtrToItem (TasksUI.UItaskCtrls, i);
		// create new XML UITaskController element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DAQLabCfg_DOMHndl, &xmlERRINFO, DAQLAB_UITASKCONTROLLER_XML_TAG, &newXMLElement) );
		// initialize attributes
		// name
		attr2[0].tag 	= "Name";
		attr2[0].type 	= DL_CSTRING;
		attr2[0].pData	= GetTaskControlName((*UItaskCtrlPtrPtr)->taskControl);
		// iterations
		niter 			= GetTaskControlIterations((*UItaskCtrlPtrPtr)->taskControl);
		attr2[1].tag 	= "Iterations";
		attr2[1].type 	= DL_UINT;
		attr2[1].pData	= &niter; 
		// wait
		wait 			= GetTaskControlIterationsWait((*UItaskCtrlPtrPtr)->taskControl);
		attr2[2].tag 	= "Wait";
		attr2[2].type 	= DL_DOUBLE;
		attr2[2].pData	= &wait; 
		// mode (finite = 1, continuous = 0)
		mode			= GetTaskControlMode((*UItaskCtrlPtrPtr)->taskControl);
		attr2[3].tag 	= "Mode";
		attr2[3].type 	= DL_BOOL;
		attr2[3].pData	= &mode; 
		
		// add attributes to task controller element
		errChk( DLAddToXMLElem (DAQLabCfg_DOMHndl, newXMLElement, attr2, DL_ATTRIBUTE, NumElem(attr2)) );
		// add task controller element to DAQLab root element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (DAQLabCfg_RootElement, &xmlERRINFO, newXMLElement, NULL) );
		// free attributes memory
		OKfree(attr2[0].pData);
	}
	
	
	return 0;
	
	Error:
	
	return error;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &DAQLabCfg_DOMHndl, &xmlerror, &xmlERRINFO, 0);
	
	return xmlerror;
	
}

static int DAQLab_Close (void)
{  
	int		error = 0;
	
	
	errChk( DAQLab_SaveXMLEnvironmentConfig() );
	
	// save module specific info
	
	
	// dispose modules list
	DAQLabModule_empty (&DAQLabModules);
	
	// discard VChans list
	ListDispose(VChannels);  
	
	errChk ( DAQLab_SaveXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl) ); 
	
	// discard DOM after saving all settings
	CA_DiscardObjHandle (DAQLabCfg_DOMHndl);
	
	
	
	QuitUserInterface(0);  
	
	return 0;
	
	Error:
	
	MessagePopup("Error", "Could not save new DAQLab settings.");
	
	QuitUserInterface(0);  
	
	return error;
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

/// HIFN Registers a VChan with the DAQLab framework.
/// HIRET TRUE if VChan was added, FALSE if error occured.
BOOL DLRegisterVChan (VChan_type* vchan)
{
	if (!vchan) return FALSE;
	return ListInsertItem(VChannels, &vchan, END_OF_LIST);
}

/// HIFN Disconnects and removes a VChan from the DAQLab framework.
/// HIRET TRUE if successful, FALSE if error occured or channel not found.
int DLUnregisterVChan (VChan_type* vchan)
{   
	size_t			itemPos;
	VChan_type** 	VChanPtr	 = DLVChanExists(vchan, &itemPos);
	if (!VChanPtr) {
		DAQLab_Msg(DAQLAB_MSG_ERR_VCHAN_NOT_FOUND, vchan, 0, 0, 0);
		return FALSE;
	}
	
	VChan_Disconnect(*VChanPtr);
	ListRemoveItem(VChannels, 0, itemPos);
	
	return TRUE;
}

/// HIFN Checks if a VChan object is present in the DAQLab framework.
/// OUT idx 1-based index of VChan object in a ListType list of VChans kept by the DAQLab framework. Pass 0 if this is not needed. If item is not found, *idx is 0.
/// HIRET Pointer to the found VChan_type* if VChan exists, NULL otherwise. 
/// HIRET Pointer is valid until another call to DLRegisterVChan or DLUnregisterVChan. 
VChan_type** DLVChanExists (VChan_type* vchan, size_t* idx)
{
	VChan_type** VChanPtrPtr;
	for (int i = 1; i <= ListNumItems(VChannels); i++) {
		VChanPtrPtr = ListGetPtrToItem(VChannels, i);
		if (*VChanPtrPtr == vchan) {
			if (idx) *idx = i;
			return VChanPtrPtr;
		}
	}
	if (idx) *idx = 0;
	return NULL;
}

/// HIFN Searches for a given VChan name and if found, return pointer to it.
/// OUT idx 1-based index of VChan object in a ListType list of VChans kept by the DAQLab framework. Pass 0 if this is not needed. If item is not found, *idx is 0.
/// HIRET Pointer to the found VChan_type* if VChan exists, NULL otherwise. 
/// HIRET Pointer is valid until another call to DLRegisterVChan or DLUnregisterVChan. 
VChan_type** DLVChanNameExists (char name[], size_t* idx)
{
	VChan_type** 	VChanPtrPtr;
	char*			listName;
	for (int i = 1; i <= ListNumItems(VChannels); i++) {
		VChanPtrPtr = ListGetPtrToItem(VChannels, i);
		listName = GetVChanName(*VChanPtrPtr);
		if (!strcmp(listName, name)) {
			if (idx) *idx = i;
			OKfree(listName);
			return VChanPtrPtr;
		}
		OKfree(listName);
	}
	
	if (idx) *idx = 0;
	return NULL;
	
}

/// HIFN Validate function for VChan names.
/// HIRET True if newVChanName is unique among the framwork VChan names, False otherwise
BOOL DLValidateVChanName (char newVChanName[], void* null)
{
	if (DLVChanNameExists(newVChanName, 0))
		return FALSE;
	else
		return TRUE;
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
	char 	buff[1000]		="";
	char 	buffCA[1000]	="";
	char*	name;
	
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
			
		case DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES:
			CA_GetAutomationErrorString(*(HRESULT*)data1, buffCA, sizeof(buffCA));
			DLMsg("Error: Could not obtain XML attribute value from element.", 1);
			DLMsg(buffCA, 0);
			DLMsg("\n", 0);
			break;
			
		case DAQLAB_MSG_ERR_LOADING_MODULE:
			DLMsg("Error: Could not load module ", 1);
			DLMsg((char*)data1, 0);	// module name
			DLMsg(" to DAQLab environment.", 0);
			if (data2) {
				DLMsg(" Reason:\n\t", 0);
				DLMsg((char*)data2, 0); // error description
				DLMsg("\n\n", 0);
			}
			break;
			
		case DAQLAB_MSG_ERR_VCHAN_NOT_FOUND:
			name = GetVChanName(data1);
			Fmt(buff, "Error: Could not find virtual channel \"%s\" in the DAQLab framework.", name);
			DLMsg(buff, 1);
			OKfree(name);
			break;
			
		case DAQLAB_MSG_OK_DOM_CREATION:
			
			DLMsg("XML DOM created successfully.\n\n", 0);
			
			break;
			
	}
	
	
}


/// HIFN Creates an MSXML DOM object using ActiveX connection to Microsoft XML 6.0 from a specified .xml file.
/// HIFN If no .xml file is found, an empty DOM is created.
/// HIPAR fileName/ String of XML file name, including .xml file extension.
/// HIRET 0 if DOM object handle was loaded from .xml file and root element was found.
/// HIRET 1 if an empty DOM was created or a new root element had to be created.
/// OUT xmlDOM 
/// OUT rootElement 
static int DAQLab_NewXMLDOM (const char fileName[], CAObjHandle* xmlDOM, ActiveXMLObj_IXMLDOMElement_* rootElement)
{
	HRESULT								xmlerror;
	ERRORINFO							xmlERRINFO;
	VBOOL								xmlLoaded;
	BOOL								rootElementNameOK		= FALSE;
	BOOL								foundXMLDOMFlag			= 1;
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
		
		foundXMLDOMFlag = 0;
	} 
	
	return foundXMLDOMFlag;
	
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

/// HIFN Adds a list of attributes or elements to an element
/// HIPAR nodeTypeFlag/ set 1 to add elements or 0 to add attributes
int	DLAddToXMLElem (CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLNodes[], DAQLabXMLNodeTypes nodeType, size_t nNodes)
{
	HRESULT							xmlerror;
	ERRORINFO						xmlERRINFO;
	ActiveXMLObj_IXMLDOMElement_	newXMLElement;
	VARIANT							xmlVal				= CA_VariantNULL ();
	BSTR							bstrVal;
	
	for (int i = 0; i < nNodes; i++) {
		
		if (nodeType == DL_ELEMENT)
			// create new element
			XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, childXMLNodes[i].tag, &newXMLElement) );
	
		// convert to variant
		switch (childXMLNodes[i].type) {
			
			case DL_NULL:
				
				xmlVal = CA_VariantNULL ();
				break;
			
			case DL_BOOL:
				
				xmlVal = CA_VariantBool ((VBOOL)*(BOOL*)childXMLNodes[i].pData);
				break;
				
			case DL_CHAR:
				
				xmlVal = CA_VariantChar(*(char*)childXMLNodes[i].pData);
				break;
				
			case DL_UCHAR:
				
				xmlVal = CA_VariantUChar(*(unsigned char*)childXMLNodes[i].pData);
				break;
				
			case DL_SHORT:
				
				xmlVal = CA_VariantShort(*(short*)childXMLNodes[i].pData);
				break;
				
			case DL_USHORT:
				
				xmlVal = CA_VariantUShort(*(unsigned short*)childXMLNodes[i].pData);
				break;
			
			case DL_INT:
				
				xmlVal = CA_VariantInt(*(int*)(childXMLNodes[i].pData));
				break;							   
				
			case DL_UINT:
				
				xmlVal = CA_VariantUInt(*(unsigned int*)childXMLNodes[i].pData);
				break;
				
			case DL_LONG:
				
				xmlVal = CA_VariantLong(*(long*)childXMLNodes[i].pData);
				break;
			
			case DL_ULONG:
				
				xmlVal = CA_VariantULong(*(unsigned long*)childXMLNodes[i].pData);
				break;
				
			case DL_LONGLONG:
				
				xmlVal = CA_VariantLongLong(*(long long*)childXMLNodes[i].pData);
				break;
				
			case DL_ULONGLONG:
				
				xmlVal = CA_VariantULongLong(*(unsigned long long*)childXMLNodes[i].pData);
				break;
			
			case DL_FLOAT:
				
				xmlVal = CA_VariantFloat(*(float*)childXMLNodes[i].pData);
				break;
			
			case DL_DOUBLE:
				
				xmlVal = CA_VariantDouble(*(double*)childXMLNodes[i].pData);
				break;
				
			case DL_CSTRING:
				
				CA_CStringToBSTR((char*)childXMLNodes[i].pData, &bstrVal); 
				xmlVal = CA_VariantBSTR(bstrVal);
				break;
		}
		
		switch (nodeType) {
			
			case DL_ELEMENT:
				
				// set element typed value
				XMLErrChk ( ActiveXML_IXMLDOMElement_SetnodeTypedValue(newXMLElement, &xmlERRINFO, xmlVal) );
				// add new element as child
				XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, &xmlERRINFO, newXMLElement, NULL) );
				
				break;
				
			case DL_ATTRIBUTE:
				
				// add attribute to element
				XMLErrChk ( ActiveXML_IXMLDOMElement_setAttribute(parentXMLElement, &xmlERRINFO, childXMLNodes[i].tag, xmlVal) );  
				
				break;
		}
		
	}
	
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &xmlDOM, &xmlerror, &xmlERRINFO, 0);
	
	return xmlerror;
}

static int DAQLab_VariantToType	(VARIANT* varPtr, DAQLabXMLTypes vartype, void* valuePtr)
{
	int	error;
	
	switch (vartype) {
		
		case DL_NULL:
			
			break;
		
		case DL_BOOL:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_BOOL, valuePtr) ); 
			break;
			
		case DL_CHAR:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_CHAR, valuePtr) ); 
			break;
			
		case DL_UCHAR:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_UCHAR, valuePtr) ); 
			break;
			
		case DL_SHORT:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_SHORT, valuePtr) ); 
			break;
			
		case DL_USHORT:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_USHORT, valuePtr) ); 
			break;
		
		case DL_INT:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_INT, valuePtr) ); 
			break;
			
		case DL_UINT:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_UINT, valuePtr) ); 
			break;
			
		case DL_LONG:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_LONG, valuePtr) ); 
			break;
		
		case DL_ULONG:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_ULONG, valuePtr) ); 
			break;
			
		case DL_LONGLONG:
			
			
			break;
			
		case DL_ULONGLONG:
			
			
			break;
		
		case DL_FLOAT:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_FLOAT, valuePtr) ); 
			break;
		
		case DL_DOUBLE:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_DOUBLE, valuePtr) ); 
			break;
			
		case DL_CSTRING:
			
			errChk( CA_VariantConvertToType(varPtr, CAVT_CSTRING, valuePtr) ); 
			break;
			
	}
		
	return 0;
		
	Error:  // conversion error
	
	return error;
	
}

int DLGetXMLElementAttributes (ActiveXMLObj_IXMLDOMElement_ XMLElement, DAQLabXMLNode Attributes[], size_t nAttributes)
{
	int								error;
	HRESULT							xmlerror;
	ERRORINFO						xmlERRINFO;
	VARIANT							xmlVal;
	
	for (int i = 0; i < nAttributes; i++) {
		
		// get attribute value
		XMLErrChk ( ActiveXML_IXMLDOMElement_getAttribute(XMLElement, &xmlERRINFO, Attributes[i].tag, &xmlVal) ); 
		
		// convert from variant to values
		errChk( DAQLab_VariantToType(&xmlVal, Attributes[i].type, Attributes[i].pData) );
		
	}
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES, &xmlerror, 0, 0, 0);
	
	return xmlerror;
	
	Error:  // conversion error
	
	return error;

}

int DLGetXMLNodeAttributes (ActiveXMLObj_IXMLDOMNode_ XMLNode, DAQLabXMLNode Attributes[], size_t nAttributes)
{
	int									error;
	HRESULT								xmlerror;
	ERRORINFO							xmlERRINFO;
	VARIANT								xmlVal;
	ActiveXMLObj_IXMLDOMNamedNodeMap_	xmlNamedNodeMap;	 // list of attributes
	ActiveXMLObj_IXMLDOMNode_			xmlAttributeNode;	 // selected attribute
	
	// get list of attributes
	XMLErrChk ( ActiveXML_IXMLDOMNode_Getattributes(XMLNode, &xmlERRINFO, &xmlNamedNodeMap) );  
	
	for (int i = 0; i < nAttributes; i++) {
		// get attribute as variant
		XMLErrChk ( ActiveXML_IXMLDOMNamedNodeMap_getNamedItem(xmlNamedNodeMap, &xmlERRINFO, Attributes[i].tag, &xmlAttributeNode) );  
		XMLErrChk ( ActiveXML_IXMLDOMNode_GetnodeTypedValue(xmlAttributeNode, &xmlERRINFO, &xmlVal) );  
		// convert from variant to values
		errChk( DAQLab_VariantToType (&xmlVal, Attributes[i].type, Attributes[i].pData) );
		
	}
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES, &xmlerror, 0, 0, 0);
	
	return xmlerror;
	
	Error:  // conversion error
	
	return error;
	
}

static UITaskCtrl_type*	DAQLab_init_UITaskCtrl_type (TaskControl_type* taskControl)
{
	int 				error = 0;
	UITaskCtrl_type* 	newUItaskCtrl = malloc(sizeof(UITaskCtrl_type));
	char*				name;
	if (!newUItaskCtrl) return NULL;
	
	
	// duplicate Task Controller panel
	errChk( newUItaskCtrl->panHndl		= DuplicatePanel(TasksUI.panHndl, TasksUI.controllerPanHndl, "", 0, DAQLAB_TASKCONTROLLERS_PAN_MARGIN) );
	newUItaskCtrl->taskControl 			= taskControl;
	// add taskControl data and callback to handle events
	SetCtrlsInPanCBInfo(taskControl, DAQLab_TaskControllers_CB, newUItaskCtrl->panHndl);
	// set UI Task Controller name
	name = GetTaskControlName(taskControl);
	SetCtrlVal(newUItaskCtrl->panHndl, TCPan1_Name, name);
	OKfree(name);
	// set UI Task Controller repeats
	SetCtrlVal(newUItaskCtrl->panHndl, TCPan1_Repeat, GetTaskControlIterations(taskControl)); 
	// set UI Task Controller wait
	SetCtrlVal(newUItaskCtrl->panHndl, TCPan1_Wait, GetTaskControlIterationsWait(taskControl));
	// set UI Task Controller mode
	SetCtrlVal(newUItaskCtrl->panHndl, TCPan1_Mode, GetTaskControlMode(taskControl));
	// dim repeat if TC mode is continuous, otherwise undim
	if (GetTaskControlMode(taskControl) == TASK_FINITE) 
		// finite
		SetCtrlAttribute(newUItaskCtrl->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	else
		// continuous
		SetCtrlAttribute(newUItaskCtrl->panHndl, TCPan1_Repeat, ATTR_DIMMED, 1);
	
	
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
	
	if (state != TASK_STATE_UNCONFIGURED && state != TASK_STATE_CONFIGURED && 
		state != TASK_STATE_INITIAL && state != TASK_STATE_IDLE && state != TASK_STATE_DONE && state != TASK_STATE_ERROR)
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
		
		// remove scroll bars
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

void CVICALLBACK DAQLab_MenuModules_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	int						modulesPanHndl;
	DAQLabModule_type**		modulePtrPtr;
	char*					fullModuleName;
	
	modulesPanHndl = LoadPanel(mainPanHndl, DAQLAB_UI_DAQLAB, ModulesPan);
	
	// list available modules
	for (int i = 0; i < NumElem(DAQLabModules_InitFunctions); i++) {
		InsertListItem(modulesPanHndl, ModulesPan_Available, -1, DAQLabModules_InitFunctions[i].className, i);  
	}
	// if there are modules to add, undim Add button
	if (NumElem(DAQLabModules_InitFunctions))
		SetCtrlAttribute(modulesPanHndl, ModulesPan_Add, ATTR_DIMMED, 0);
	else
		SetCtrlAttribute(modulesPanHndl, ModulesPan_Add, ATTR_DIMMED, 1);
		
	// list installed modules
	for (size_t i = 1; i <= ListNumItems(DAQLabModules); i++) {
		modulePtrPtr = ListGetPtrToItem(DAQLabModules, i);
		fullModuleName = StrDup((*modulePtrPtr)->className);
		AppendString(&fullModuleName, ": ", -1);
		AppendString(&fullModuleName, (*modulePtrPtr)->instanceName, -1);
		InsertListItem(modulesPanHndl, ModulesPan_Installed, -1, fullModuleName, (*modulePtrPtr)->instanceName);
		OKfree(fullModuleName);
	} 
	// if there are modules installed, undim Remove button
	if (ListNumItems(DAQLabModules))
		SetCtrlAttribute(modulesPanHndl, ModulesPan_Remove, ATTR_DIMMED, 0); 
	else
		SetCtrlAttribute(modulesPanHndl, ModulesPan_Remove, ATTR_DIMMED, 1); 
	
	// display panel
	DisplayPanel(modulesPanHndl);
	
	return;
}



static BOOL	DAQLab_ValidModuleName (char name[], void* listPtr)
{
	ListType				modules = *(ListType*)listPtr;
	DAQLabModule_type**		modulePtrPtr;
	
	for (int i = 1; i <= ListNumItems(modules); i++) {
		modulePtrPtr = ListGetPtrToItem(modules , i);
		if (!strcmp((*modulePtrPtr)->instanceName, name)) return FALSE; // module with same instance name exists already
	}
	
	return TRUE;
}

int CVICALLBACK DAQLab_ManageDAQLabModules_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			char*					newInstanceName;
			char*					fullModuleName;
			char*					moduleName;
			int						moduleidx;		// 0-based index of selected module
			DAQLabModule_type*  	newModulePtr;
			DAQLabModule_type**		modulePtrPtr;
			int						nchars;
			
			
			switch (control) {
					
				case ModulesPan_Add:
					
					newInstanceName = DLGetUINameInput ("New Module Instance", DAQLAB_MAX_MODULEINSTANCE_NAME_NCHARS, DAQLab_ValidModuleName, &DAQLabModules);
					if (!newInstanceName) return 0; // operation cancelled, do nothing
					
					GetCtrlIndex(panel, ModulesPan_Available, &moduleidx);
					// call module init function
					newModulePtr = (*DAQLabModules_InitFunctions[moduleidx].ModuleInitFptr)	(NULL, DAQLabModules_InitFunctions[moduleidx].className, newInstanceName);
					// call module load function
					if ( (*newModulePtr->Load) 	(newModulePtr, mainPanHndl) < 0) {
						// dispose of module if not loaded properly
						(*newModulePtr->Discard ) 	(&newModulePtr);
						break;
					}
					// display module panels if method is defined
					if (newModulePtr->DisplayPanels)
						(*newModulePtr->DisplayPanels) (newModulePtr, TRUE);
					
					// insert module to modules list
					ListInsertItem(DAQLabModules, &newModulePtr, END_OF_LIST);
					// add full module name to list box
					fullModuleName = StrDup(newModulePtr->className);
					AppendString(&fullModuleName, ": ", -1);
					AppendString(&fullModuleName, newModulePtr->instanceName, -1);
					InsertListItem(panel, ModulesPan_Installed, -1, fullModuleName, newModulePtr->instanceName);
					OKfree(fullModuleName);
					// undim remove module button
					SetCtrlAttribute(panel, ModulesPan_Remove, ATTR_DIMMED, 0); 
					
					break;
					
				case ModulesPan_Remove:
					
					GetCtrlIndex(panel, ModulesPan_Installed, &moduleidx);
					GetValueLengthFromIndex(panel, ModulesPan_Installed, moduleidx, &nchars);
					moduleName = malloc ((nchars+1)*sizeof(char));
					if (!moduleName) {
						DLMsg("Error: Out of memory.", 1);
						return 0;
					}
					// get module instance name
					GetValueFromIndex(panel, ModulesPan_Installed, moduleidx, moduleName); 
					// remove module if found
					for (int i = 1; i <= ListNumItems(DAQLabModules); i++) {
						modulePtrPtr = ListGetPtrToItem (DAQLabModules, i);
						if (!strcmp((*modulePtrPtr)->instanceName, moduleName)) {
							// call module discard function
							(*(*modulePtrPtr)->Discard)	(modulePtrPtr);
							// also remove from DAQLab modules list
							ListRemoveItem(DAQLabModules, 0, i);
							break;
						}
					}
					OKfree(moduleName);
					// remove from installed modules list
					DeleteListItem(panel, ModulesPan_Installed, moduleidx, 1);
					
					break;
			}

			break;
	}
	return 0;
}

/// HIFN Displays a popup box where the user can give a string after which a validate function pointer is called. 
/// HIFN If the string is not validated for example because the name already exists, then the popup box appears again until a valid name is given or until it is cancelled.
/// HIPAR validateInputFptr/ if function returns TRUE, input is valid, otherwise not and popup is displayed again.
/// HIRET Dynamically allocated string if input was succesful or NULL if operation was cancelled.
char* DLGetUINameInput (char popupWndName[], size_t maxInputLength, ValidateInputFptr_type validateInputFptr, void* dataPtr)
{
	char* 	newName 				= malloc ((maxInputLength+1)*sizeof(char));
	char	msg1[]					= "Enter name.";
	char	msg2[]					= "Invalid name. Enter new name.";
	char	msg3[]					= "Names must be different. Enter new name.";
	char*	popupMsg;								 
	int		selectedBTTN;
	
	if (!validateInputFptr) {OKfree(newName); return NULL;}  // do nothing 
	popupMsg = msg1; 
	do {
		NewInput:
		// obtain new name if one is given, else nothing happens
		selectedBTTN = GenericMessagePopup (popupWndName, popupMsg, "Ok", "Cancel", "", newName, maxInputLength * sizeof(char),
											 0, VAL_GENERIC_POPUP_INPUT_STRING,VAL_GENERIC_POPUP_BTN1,VAL_GENERIC_POPUP_BTN2);
		// remove surrounding white spaces 
		RemoveSurroundingWhiteSpace(newName);
		
		// if OK and name is not valid
		if ( (selectedBTTN == VAL_GENERIC_POPUP_BTN1) && !*newName ) {
			popupMsg = msg2;
			goto NewInput;  // display again popup to enter new name
		} else
			// if cancelled
			if (selectedBTTN == VAL_GENERIC_POPUP_BTN2) {OKfree(newName); return NULL;}  // do nothing
			
		// name is valid, provided function pointer is called for additional check 
			
		// if another Task Controller with the same name exists display popup again
		popupMsg = msg3;
		
	} while (!(*validateInputFptr)(newName, dataPtr)); // repeat while input is not valid
	
	return 	newName;
	
}

static BOOL	DAQLab_ValidControllerName (char name[], void* listPtr)
{
	ListType			controllerList		= *(ListType*) listPtr;
	BOOL 				foundFlag 			= FALSE; 
	UITaskCtrl_type**	UITaskCtrlsPtrPtr;
	char*				nameTC				= NULL;
	
	// check if there is already another Task Controller with the same name
	for (size_t i = 1; i <= ListNumItems(controllerList); i++) {
		UITaskCtrlsPtrPtr = ListGetPtrToItem(controllerList, i);
		nameTC = GetTaskControlName((*UITaskCtrlsPtrPtr)->taskControl);
		if (strcmp (nameTC, name) == 0) {
			foundFlag = TRUE;
			break;
		}
		OKfree(nameTC);
	}
	OKfree(nameTC);
	return !foundFlag; // valid input if there is no other controller with the same name
}

static void	DAQLab_TaskMenu_AddTaskController 	(void) 
{
	UITaskCtrl_type*	UITaskCtrlsPtr;
	TaskControl_type* 	newTaskControllerPtr;
	char*				newControllerName;
	
	newControllerName = DLGetUINameInput ("New Task Controller", DAQLAB_MAX_UITASKCONTROLLER_NAME_NCHARS, DAQLab_ValidControllerName, &TasksUI.UItaskCtrls);
	if (!newControllerName) return; // operation cancelled, do nothing
	
	// create new task controller
	newTaskControllerPtr = init_TaskControl_type (newControllerName, DAQLab_ConfigureUITC, DAQLab_IterateUITC, DAQLab_StartUITC, 
												  DAQLab_ResetUITC, DAQLab_DoneUITC, DAQLab_StoppedUITC, NULL, NULL, DAQLab_ErrorUITC); 
	OKfree(newControllerName);
	
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
	UITaskCtrl_type** 	UITaskCtrlsPtrPtr;
	char*				name;
	
	delPanHndl = LoadPanel(0, DAQLAB_UI_DAQLAB, TCDelPan);
	
	// list Task Controllers
	
	for (size_t i = 1; i <= ListNumItems(TasksUI.UItaskCtrls); i++) {
		UITaskCtrlsPtrPtr = ListGetPtrToItem(TasksUI.UItaskCtrls, i);
		name = GetTaskControlName((*UITaskCtrlsPtrPtr)->taskControl);
		InsertListItem(delPanHndl, TCDelPan_TaskControllers, -1, name, i); 
		OKfree(name);
	}
	
	// display popup panel
	InstallPopup(delPanHndl);
	
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
			
			int		idx;	 // 0-based list index
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
	BOOL             	starttask;					  	// 1 - task start, 0 - task stop 
	double				waitDVal;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
			
				case TCPan1_Repeat:
				case TCPan1_Reset:
				case TCPan1_Mode:
					
					TaskControlEvent(taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
					
					break;
					
				case TCPan1_StartStop:
					
					GetCtrlVal(panel,control,&starttask);
					if (starttask) {
						// dim controls
						SetCtrlAttribute(panel, TCPan1_Repeat, ATTR_DIMMED, 1);
						SetCtrlAttribute(panel, TCPan1_Wait, ATTR_DIMMED, 1);
						SetCtrlAttribute(panel, TCPan1_Reset, ATTR_DIMMED, 1);
						SetCtrlAttribute(panel, TCPan1_Mode, ATTR_DIMMED, 1);
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
	BOOL				taskControllerMode;		// TRUE - finite iterations, FALSE - continuous
	
	// change Task Controller name background color to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// undim Repeat button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// undim Start button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Repeat, &repeat);
	SetTaskControlIterations(taskControl, repeat);
	
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Mode, &taskControllerMode);
	if (taskControllerMode == TASK_FINITE) {
		// finite iterations
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
		SetTaskControlMode(taskControl, TASK_FINITE);
	} else {
		// continuous iterations
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 1);
		SetTaskControlMode(taskControl, TASK_CONTINUOUS);
	}
	
	return init_FCallReturn_type(0, "", "");
}

static void DAQLab_IterateUITC	(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// iteration complete, update current iteration number
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, currentIteration + 1);
	
	TaskControlEvent(taskControl, TASK_EVENT_ITERATION_DONE, NULL, NULL);
}

static FCallReturn_type* DAQLab_StartUITC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from gray (0x00F0F0F0) to green (0x002BD22F)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x002BD22F);
	ProcessSystemEvents();	// required to update the background color
	
	return init_FCallReturn_type(0, "", "");
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
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// update iterations display
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, currentIteration);
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* DAQLab_ResetUITC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, 0);
	
	return init_FCallReturn_type(0, "", ""); 
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
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	// undim Start/Stop button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	// update iterations display
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, currentIteration);
	
	return init_FCallReturn_type(0, "", ""); 
}

static void DAQLab_ErrorUITC (TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag)
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
}

int CVICALLBACK CloseDAQLabModulesPan_CB (int panel, int control, int event,
		void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			DiscardPanel(panel);

			break;
	}
	return 0;
}

