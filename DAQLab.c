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
#include "Module_Header.h"
#include "DataStorage.h"
#include "Iterator.h" 



//==============================================================================
// Constants
	
	// DAQLab main workspace UI resource
#define DAQLAB_UI_DAQLAB									"UI_DAQLab.uir"
	// name of config file
#define DAQLAB_CFG_FILE										"DAQLabCfg.xml"

#define DAQLAB_CFG_DOM_ROOT_NAME							"DAQLab_Config"
#define DAQLAB_UITaskController_XML_TAG						"DAQLab_UITaskController"
#define DAQLAB_TaskController_XML_TAG						"TaskController"
#define DAQLAB_Switchboard_XML_TAG							"Switchboard"
#define DAQLAB_HWTriggers_XML_TAG							"HWTriggers"
#define DAQLAB_HWTrigMaster_XML_TAG							"HWTrigMaster"
#define DAQLAB_HWTrigSlave_XML_TAG							"HWTrigSlave"
#define DAQLAB_SourceVChan_XML_TAG							"SourceVChan"
#define DAQLAB_SinkVChan_XML_TAG							"SinkVChan"
#define DAQLAB_TaskControllerSettings_XML_TAG				"TaskControllerSettings"
#define DAQLAB_MODULES_XML_TAG								"DAQLab_Modules"

	// number of Task Controllers visible at once. If there are more, a scroll bar appears.
#define DAQLAB_NVISIBLE_TASKCONTROLLERS						4

	// number of pixels between inserted Task Controller panel and outer Tasks panel
#define DAQLAB_TASKCONTROLLERS_PAN_MARGIN					5
	// vertical spacing between Task Controllers in the panel
#define DAQLAB_TASKCONTROLLERS_SPACING						5 

	// maximum number of characters allowed for a UI Task Controller name
#define DAQLAB_MAX_UITASKCONTROLLER_NAME_NCHARS				50

	// Data storage settings
#define MAXBASEFILEPATH			MAX_PATHNAME_LEN

#define DATAFILEBASEPATH 		"C:\\Rawdata\\"
#define VChanDataTimeout									1e4					// Timeout in [ms] for Sink VChans to receive data  



// Macros
#define XMLErrChk(fCall) if (xmlerror = (fCall), xmlerror < 0) \
{goto XMLError;} else

//==============================================================================
// Types

//------------------------------------------------------------------------------------------------
//                              AVAILABLE DAQLab MODULES
//------------------------------------------------------------------------------------------------

// Constant function pointer that is used to launch the initialization of different DAQLabModules 
typedef DAQLabModule_type* (* const ModuleInitAllocFunction_type ) (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl);

typedef struct {
	
	char*							className;
	ModuleInitAllocFunction_type	ModuleInitFptr;
	BOOL							multipleInstancesFlag;	  // allows multiple instances of the module
	size_t							nInstances;				  // counter for the total number of instances

} AvailableDAQLabModules_type;


AvailableDAQLabModules_type DAQLabModules_InitFunctions[] = {	  // set last parameter, i.e. the instance
																  // counter always to 0
	//{ MOD_PIStage_NAME, initalloc_PIStage, FALSE, 0 },
	//{ MOD_LangLStep_NAME, initalloc_LangLStep, FALSE, 0},
	{ MOD_NIDAQmxManager_NAME, initalloc_NIDAQmxManager, FALSE, 0 },
	{ MOD_LaserScanning_NAME, initalloc_LaserScanning, FALSE, 0},
	//{ MOD_VUPhotonCtr_NAME, initalloc_VUPhotonCtr, FALSE, 0 },
	{ MOD_DataStorage_NAME, initalloc_DataStorage, FALSE, 0 },
	{ MOD_Pockells_NAME, initalloc_PockellsModule, FALSE, 0 },
	{ MOD_CoherentCham_NAME, initalloc_CoherentCham, FALSE, 0}
};

//-------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// 			Predefined DAQLab errors and warnings to be used with DAQLab_Msg function.
//------------------------------------------------------------------------------------------------
// DAQLab_Msg is a function with variable arguments and types which depend on
// the particular warning or error message. When calling this function with a 
// certain error code from below, pass in the additional parameters with 
// required data type.

typedef enum _DAQLabMessageID{
													// parameter types (data1, data2, data3, data4)
	// errors										// to pass in DAQLab_Msg
	DAQLAB_MSG_ERR_DOM_CREATION,					// HRESULT* error code, 0, 0, 0
	DAQLAB_MSG_ERR_ACTIVEXML,						// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info, 0
	DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES, 		// HRESULT* error code, 0, 0, 0
	DAQLAB_MSG_ERR_LOADING_MODULE,					// char* module instance name, char* error description
	DAQLAB_MSG_ERR_LOADING_MODULE_CONFIG,			// char* module instance name
	DAQLAB_MSG_ERR_VCHAN_NOT_FOUND,					// VChan_type*, 0, 0, 0
	DAQLAB_MSG_ERR_NOT_UNIQUE_CLASS_NAME,			// char* class name, 0, 0, 0
	DAQLAB_MSG_ERR_NOT_UNIQUE_INSTANCE_NAME,		// char* instance name, 0, 0, 0
	DAQLAB_MSG_ERR_NOT_UNIQUE_TASKCONTROLLER_NAME,	// char* task controller name, 0, 0, 0
	
	
	
	// warnings									
	DAQLAB_MSG_WARN_NO_CFG_FILE,			// CAObjHandle* object handle, HRESULT* error code, ERRORINFO* additional error info, 0       
	
	// success									
	DAQLAB_MSG_OK_DOM_CREATION				// 0, 0, 0, 0 
	

} DAQLabMessageID;

//-------------------------------------------------------------------------------------------------
//										 Framework Typedefs
//-------------------------------------------------------------------------------------------------

	// Glues UI Task Controller to panel handle
typedef struct {						  
										  
	int					panHndl;
	TaskControl_type*   taskControl;
	ListType			dataStorageSinkVChans; 		// Sink VChans used to receive incoming data;

} UITaskCtrl_type;

// Task Tree Manager 
#define TaskTreeManager_Modules_Label						"Modules"
#define TaskTreeManager_UITCs_Label							"UI Task Controllers"
	// Glues Task Tree nodes to Task Controllers. 
	// If node does not have a Task Controller then taskControl = NULL
typedef struct {
	TaskControl_type*   taskControl;
	BOOL				acceptsTCChild;				// If True, this node accepts a Task Controller child node
	BOOL				acceptsUITCChild;   		// If True, this node accepts a user interface type Task Controller
	BOOL				canBeDeleted;				// If True, node can be deleted from its current position in the tree
            
} TaskTreeNode_type;
	

	
	

//==============================================================================
// Static global variables


	// DAQLab XML ActiveX server controller handle for DOM XML management
CAObjHandle							DAQLabCfg_DOMHndl		= 0;
ActiveXMLObj_IXMLDOMElement_   		DAQLabCfg_RootElement 	= 0; 

	// UI resources	
static int				workspacePanHndl		= 0;		// Main workspace panel handle
static int				logPanHndl				= 0;		// Log panel handle
static int				DAQLabModulesPanHndl	= 0;		// Modules list panel handle
static int				taskLogPanHndl			= 0;		// Log panel for task controller execution

static struct TasksUI_ {									// UI data container for Task Controllers
	
	// panel resource handles
	int			mainPanHndl;								// Task management main panel handle.
	int			dataStoragePanHndl;							// Data storage panel handle.
	int			controllerPanHndl;							// Task Controller panel handle
									
	int			controllerPanWidth;
	int			controllerPanHeight;
	
	// menu resources
	int			menuBarHndl;
	
	int			menuID_Manage;
	int			menuManageItem_Add;
	int			menuManageItem_Delete;
	
	int			menuID_Data;
	int			menuDataItem_Storage;
	
	ListType	UItaskCtrls;								// UI Task Controllers of UITaskCtrl_type*   
	
} 						TasksUI						= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	// List of modules loaded in the framework. Modules inherit from DAQLabModule_type 
	// List of DAQLabModule_type* type
ListType				DAQLabModules				= 0;

	// List of Task Controllers provided by all modules (including UI Task Controllers) to the framework
	// List of TaskControl_type* elements
ListType				DAQLabTCs					= 0;

	// Virtual channels from all the modules that register such channels with the framework
	// List elements of VChan_type* type
ListType				VChannels					= 0;

	// List of HW Master triggers of HWTrigMaster_type*
ListType				HWTrigMasters				= 0;

	// List of HW Slave triggers of HWTrigSlave_type*	
ListType				HWTrigSlaves				= 0;

	// Panel handle for the Task Tree Manager 
int						TaskTreeManagerPanHndl		= 0;

	// Panel handle for VChan Switchboard
int						VChanSwitchboardPanHndl		= 0;

	// Panel handle for HW Triggers Switchboard
int						HWTrigSwitchboardPanHndl	= 0;

	// List of Task Tree nodes of TaskTreeNode_type needed to operate the Task Tree Manager 
ListType				TaskTreeNodes				= 0;

	// Custom thread pool handle
CmtThreadPoolHandle		DLThreadPoolHndl			= 0;



//==============================================================================
// Static functions
//==============================================================================

static int					DAQLab_Load 								(void);

static int					DAQLab_Save									(void);

static int 					DLSaveUITCTaskTree 							(TaskControl_type* taskController, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ UITCTaskTreeXMLElement, ERRORINFO* xmlErrorInfo);

static int 					SaveVChanConnections 						(ListType VChans, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int 					LoadVChanConnections 						(ListType VChans, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int 					SaveHWTriggerConnections 					(ListType hwTrigMasters, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int 					LoadHWTriggerConnections 					(ListType hwTrigMasters, ListType hwTrigSlaves, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo);

static int 					ConnectTaskTrees 							(ActiveXMLObj_IXMLDOMElement_ UITCXMLElement, ERRORINFO* xmlErrorInfo, char** errorMsg);

static TaskControl_type*	GetTaskController							(char tcName[]);

static int 					DAQLab_NewXMLDOM 							(const char fileName[], CAObjHandle* xmlDOM, ActiveXMLObj_IXMLDOMElement_* rootElement);

static int					DAQLab_SaveXMLDOM 							(const char fileName[], CAObjHandle* xmlDOM);

static int 					DAQLab_Close 								(void);

static void 				DAQLab_Msg 									(DAQLabMessageID msgID, void* data1, void* data2, void* data3, void* data4);

static UITaskCtrl_type*		DAQLab_AddTaskControllerToUI				(TaskControl_type* taskControl);

static int					RemoveTaskControllerFromUI					(int index, char** errorMsg);

static void					DAQLab_RedrawTaskControllerUI				(void);

static void 				UpdateVChanSwitchboard	 					(int panHandle, int tableControlID);

static void 				UpdateHWTriggersSwitchboard					(int panHandle, int tableControlID);

static void CVICALLBACK 	DAQLab_TaskMenu_CB 							(int menuBarHndl, int menuItemID, void *callbackData, int panelHndl);

	// Displays log panel to debug task execution for all active task controllers. This slows down the TCs execution considerably.
void CVICALLBACK 			LogPanTaskLogMenu_CB						(int menuBar, int menuItem, void *callbackData, int panel);

	// Closes Task log panel and stops logging of active Task Controllers.
void CVICALLBACK 			TaskLogMenuClose_CB 						(int menuBar, int menuItem, void *callbackData, int panel); 

static int					DAQLab_TaskMenu_AddTaskController 			(char** errorMsg);

static void					DAQLab_TaskMenu_DeleteTaskController 		(void);

static void 				DAQLab_TaskMenu_DisplayDataStorage 			(void);

static int CVICALLBACK 		DAQLab_TaskControllers_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static UITaskCtrl_type*		DAQLab_init_UITaskCtrl_type					(TaskControl_type* taskControl);  	

static void					DAQLab_discard_UITaskCtrl_type				(UITaskCtrl_type** UITaskCtrlPtr);

static int					DAQLab_StringToType							(char text[], BasicDataTypes vartype, void* value);

static BOOL 				DAQLab_CheckValidModuleName 				(char name[], void* dataPtr);

static BOOL					DAQLab_ValidControllerName 					(char name[], void* listPtr);

static void					DisplayTaskTreeManager	 					(int parentPanHndl, ListType UITCs, ListType modules);

static void 				AddRecursiveTaskTreeItems 					(int panHndl, int TreeCtrlID, int parentIdx, TaskControl_type* taskControl);

int 						CVICALLBACK TaskTree_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

//-----------------------------------------
// UI Task Controller Callbacks
//-----------------------------------------

static int					ConfigureUITC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static int					UnconfigureUITC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static void					IterateUITC									(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);
static int					StartUITC									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);
static int					DoneUITC									(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);
static int					StoppedUITC									(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);
static int					TaskTreeStateChangeUITC 					(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);
static void					UITCActive									(TaskControl_type* taskControl, BOOL UITCActiveFlag);
static int				 	ResetUITC 									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 
static void				 	ErrorUITC 									(TaskControl_type* taskControl, int errorID, char* errorMsg);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

void CVICALLBACK 			DAQLab_ModulesMenu_CB						(int menuBarHndl, int menuItem, void *callbackData, int panelHndl);

void CVICALLBACK 			DAQLab_DisplayTaskManagerMenu_CB 			(int menuBarHndl, int menuItemID, void *callbackData, int panelHndl); 

void CVICALLBACK 			DAQLab_SaveCfg_CB 							(int menuBarHndl, int menuItemID, void *callbackData, int panelHndl); 

int CVICALLBACK 			DAQLab_ManageDAQLabModules_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

int CVICALLBACK 			VChanSwitchboard_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

int CVICALLBACK 			HWTriggersSwitchboard_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 		DataStorage_CB			 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

/// HIFN  Main entry point
/// HIRET Return indicates if function was successful.
/// HIRET 0 success
/// HIRET -1 fail
int main (int argc, char *argv[])
{
INIT_ERR

	// start CVI Run-Time Engine
	if (!InitCVIRTE (0, argv, 0)) return -1;
	
	// set main thread sleep policy
	errChk( SetSleepPolicy(VAL_SLEEP_SOME) );
	
	// create custom thread pool
	errChk( CmtNewThreadPool(UNLIMITED_THREAD_POOL_THREADS, &DLThreadPoolHndl) );
	
	// adjust thread pool
	errChk( CmtSetThreadPoolAttribute(DLThreadPoolHndl, ATTR_TP_PROCESS_EVENTS_WHILE_WAITING, TRUE) );
	
	
	// load DAQLab environment resources
	
	DAQLab_Load(); 
	
	// run GUI
	errChk ( RunUserInterface() ); 
	
	// discard thread pool
	errChk( CmtDiscardThreadPool(DLThreadPoolHndl) );
	
	return 0;
	
Error:
	
	return errorInfo.error;
}

/// HIFN  Loads DAQLab environment
/// HIRET Return indicates if function was successful.
/// HIRET 0 if success
/// HIRET negative value if fail
static int DAQLab_Load (void)
{
INIT_ERR
	
	BOOL							FoundDAQLabSettingsFlag				= 0;		// if FALSE, no XML settings were found and default will be used
	HRESULT							xmlerror							= 0;  
	ERRORINFO						xmlErrorInfo						= {.wCode = 0, .sCode = 0, .source = "", .description = "", .helpFile = "", .helpContext = 0, .errorParamPos = 0};;
	ActiveXMLObj_IXMLDOMNodeList_	UITaskControllersXMLNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_		UITaskControllerXMLNode				= 0;
	long							nUITaskControlers					= 0;
									// DAQLab UI data
	int								taskPanTopPos						= 0;
	int								taskPanLeftPos						= 0;
	int								logPanTopPos						= 0;
	int								logPanLeftPos						= 0;
	TaskControl_type*				newTaskController					= NULL;
	UITaskCtrl_type*				UITaskCtrlsPtr						= NULL;
									// template to load DAQLab Environment parameters
	DAQLabXMLNode 					attr1[] = { {"TaskPanTopPos", 		BasicData_Int, 		&taskPanTopPos},
							 					{"TaskPanLeftPos", 		BasicData_Int, 		&taskPanLeftPos},
												{"LogPanTopPos", 		BasicData_Int, 		&logPanTopPos},
												{"LogPanLeftPos",		 BasicData_Int, 	&logPanLeftPos} };
									
	//---------------------------------------------------------------------------------
	// Load resources
	//---------------------------------------------------------------------------------
	
		// UI
	errChk ( workspacePanHndl = LoadPanel (0, DAQLAB_UI_DAQLAB, MainPan) );
	errChk ( logPanHndl = LoadPanel (workspacePanHndl, DAQLAB_UI_DAQLAB, LogPan) );
	errChk ( TasksUI.mainPanHndl 		= LoadPanel (workspacePanHndl, DAQLAB_UI_DAQLAB, TasksPan) );
	errChk ( TasksUI.controllerPanHndl 	= LoadPanel (workspacePanHndl, TaskControllerUI, TCPan1) );
	
		// Framework
	nullChk( TasksUI.UItaskCtrls 		= ListCreate(sizeof(UITaskCtrl_type*)) );		// list with UI Task Controllers
	nullChk( DAQLabModules   			= ListCreate(sizeof(DAQLabModule_type*)) );		// list with loaded DAQLab modules
	nullChk( DAQLabTCs					= ListCreate(sizeof(TaskControl_type*)) );		// list with loaded Task Controllers
	nullChk( VChannels		   			= ListCreate(sizeof(VChan_type*)) );			// list with Virtual Channels
	nullChk( HWTrigMasters				= ListCreate(sizeof(HWTrigMaster_type*)) );		// list with Master HW Triggers 
	nullChk( HWTrigSlaves				= ListCreate(sizeof(HWTrigSlave_type*)) );		// list with Slave HW Triggers
	
		// Log panel for task controller execution
	errChk ( taskLogPanHndl = LoadPanel (workspacePanHndl, DAQLAB_UI_DAQLAB, TaskLogPan) );  
	
	
	//---------------------------------------------------------------------------------
	// Adjust Panels
	//---------------------------------------------------------------------------------
	
	// maximize main panel by default
	SetPanelAttribute(workspacePanHndl, ATTR_WINDOW_ZOOM, VAL_MAXIMIZE);
	
	// center Tasks panel, Log panel by default (their positions will be overriden if a valid XML config is found)
	SetPanelAttribute(TasksUI.mainPanHndl, ATTR_LEFT, VAL_AUTO_CENTER);
	SetPanelAttribute(TasksUI.mainPanHndl, ATTR_TOP, VAL_AUTO_CENTER);
	SetPanelAttribute(logPanHndl, ATTR_LEFT, VAL_AUTO_CENTER);
	SetPanelAttribute(logPanHndl, ATTR_TOP, VAL_AUTO_CENTER);
	
	// also center task execution log panel
	SetPanelAttribute(taskLogPanHndl, ATTR_LEFT, VAL_AUTO_CENTER);
	SetPanelAttribute(taskLogPanHndl, ATTR_TOP, VAL_AUTO_CENTER);
	
	// add menu bar to Tasks panel
	errChk (TasksUI.menuBarHndl				= NewMenuBar(TasksUI.mainPanHndl) );
	// Manage task controllers
	errChk (TasksUI.menuID_Manage			= NewMenu(TasksUI.menuBarHndl, "Manage", -1) );
	errChk (TasksUI.menuManageItem_Add 		= NewMenuItem(TasksUI.menuBarHndl, TasksUI.menuID_Manage, "Add", -1, (VAL_MENUKEY_MODIFIER | 'A'), DAQLab_TaskMenu_CB, &TasksUI) );
	errChk (TasksUI.menuManageItem_Delete 	= NewMenuItem(TasksUI.menuBarHndl, TasksUI.menuID_Manage, "Delete", -1, (VAL_MENUKEY_MODIFIER | 'D'), DAQLab_TaskMenu_CB, &TasksUI) );
	// Data storage and display
	errChk (TasksUI.menuID_Data				= NewMenu(TasksUI.menuBarHndl, "Data", -1) );
	errChk (TasksUI.menuDataItem_Storage	= NewMenuItem(TasksUI.menuBarHndl, TasksUI.menuID_Data, "Storage", -1, (VAL_MENUKEY_MODIFIER | 'S'), DAQLab_TaskMenu_CB, &TasksUI) );
	
	// get height and width of Task Controller panel
	GetPanelAttribute(TasksUI.controllerPanHndl, ATTR_HEIGHT, &TasksUI.controllerPanHeight);
	GetPanelAttribute(TasksUI.controllerPanHndl, ATTR_WIDTH, &TasksUI.controllerPanWidth);
	
	
	DAQLab_RedrawTaskControllerUI();
	
	// display main panel
	DisplayPanel(workspacePanHndl);
	
	DisplayPanel(TasksUI.mainPanHndl); 
	
	DisplayPanel(logPanHndl);
	
	// init DAQLab DOM
	errChk ( FoundDAQLabSettingsFlag = DAQLab_NewXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl, &DAQLabCfg_RootElement) );
	
	// skip loading of settings if none are found
	if (!FoundDAQLabSettingsFlag) goto DAQLabNoSettings;
	
	// settings were found, apply settings to UI panels
	DLGetXMLElementAttributes(DAQLabCfg_RootElement, attr1, NumElem(attr1));  
	// apply loaded panel positions
	errChk( SetPanelAttribute(TasksUI.mainPanHndl, ATTR_LEFT, taskPanLeftPos) );
	errChk( SetPanelAttribute(TasksUI.mainPanHndl, ATTR_TOP, taskPanTopPos) );
	errChk( SetPanelAttribute(logPanHndl, ATTR_LEFT, logPanLeftPos) );
	errChk( SetPanelAttribute(logPanHndl, ATTR_TOP, logPanTopPos) );
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Load UI Task Controller settings
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(DAQLabCfg_RootElement, &xmlErrorInfo, DAQLAB_UITaskController_XML_TAG, &UITaskControllersXMLNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(UITaskControllersXMLNodeList, &xmlErrorInfo, &nUITaskControlers) );
	for (long i = 0; i < nUITaskControlers; i++) {
		
		// get xml Task Controller Node
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(UITaskControllersXMLNodeList, &xmlErrorInfo, i, &UITaskControllerXMLNode) );
		
		// create a new default UI Task Controller
		newTaskController = init_TaskControl_type ("", NULL, DLThreadPoolHndl, ConfigureUITC, UnconfigureUITC, IterateUITC, StartUITC, 
												 	  ResetUITC, DoneUITC, StoppedUITC, NULL, TaskTreeStateChangeUITC, UITCActive, NULL, ErrorUITC); // module data added to the task controller below
		
		errChk( DLLoadTaskControllerSettingsFromXML(newTaskController, (ActiveXMLObj_IXMLDOMElement_)UITaskControllerXMLNode, &xmlErrorInfo) );
		
		// mark Task Controller as UITC
		SetTaskControlUITCFlag(newTaskController, TRUE);
		
		// add new Task Controller to the environment and framework
		UITaskCtrlsPtr = DAQLab_AddTaskControllerToUI(newTaskController);
		// attach callback data to the Task Controller
		SetTaskControlModuleData(newTaskController, UITaskCtrlsPtr);
		// send a configure event to the Task Controller
		errChk( TaskControlEvent(newTaskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
		
		// cleanup
		OKfreeCAHndl(UITaskControllerXMLNode);
		
	}
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Load DAQLab Modules and apply saved settings
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	ActiveXMLObj_IXMLDOMNodeList_	xmlNodeList;																
	ActiveXMLObj_IXMLDOMNodeList_	xmlModulesNodeList;																// Modules installed
	ActiveXMLObj_IXMLDOMNode_		xmlModulesNode;																	// "Modules" node containing module XML elements
	ActiveXMLObj_IXMLDOMNode_		xmlModuleNode;																	// "Modules" node
	char*							moduleClassName;																// Module class name to be loaded
	char*							moduleInstanceName;																// module instance name
																													// DAQLab module element node to be loaded
	long							nModules;																		// Number of modules to be loaded
	DAQLabXMLNode					attrModule[] = {	{"ClassName", BasicData_CString, &moduleClassName},			// Common module attributes to load (same for all modules)
														{"InstanceName", BasicData_CString, &moduleInstanceName}	// Note: module specific attributes are managed by the modules
																									  			
																										};
	DAQLabModule_type*				newModule;
	
	// get "Modules" named node list
	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(DAQLabCfg_RootElement, &xmlErrorInfo, DAQLAB_MODULES_XML_TAG, &xmlNodeList) );
	// get "Modules" node
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlNodeList, &xmlErrorInfo, 0, &xmlModulesNode) );
	// get modules node list contained in the "Modules" node
	XMLErrChk ( ActiveXML_IXMLDOMNode_GetchildNodes(xmlModulesNode, &xmlErrorInfo, &xmlModulesNodeList) );
	// get number of modules to be loaded
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(xmlModulesNodeList, &xmlErrorInfo, &nModules) );
	// load modules and their settings
	for (long i = 0; i < nModules; i++) {
		// get DAQLab module node
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlModulesNodeList, &xmlErrorInfo, i, &xmlModuleNode) );
		// get DAQlab module attributes such as class name listed in attrModules
		DLGetXMLNodeAttributes(xmlModuleNode, attrModule, NumElem(attrModule)); 
		// get module index from the framework to call its init function 
		for (size_t j = 0; j < NumElem(DAQLabModules_InitFunctions); j++)
			if (DAQLabModules_InitFunctions[j].className && moduleClassName && !strcmp(DAQLabModules_InitFunctions[j].className, moduleClassName)) {
				// call module init function
				newModule = (*DAQLabModules_InitFunctions[j].ModuleInitFptr)	(NULL, DAQLabModules_InitFunctions[j].className, moduleInstanceName, workspacePanHndl);
				// load module configuration data if specified
				if (newModule->LoadCfg)
					if( (xmlerror = (*newModule->LoadCfg) (newModule, xmlModuleNode, &xmlErrorInfo)) < 0) {
						DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &DAQLabCfg_DOMHndl, &xmlerror, &xmlErrorInfo, 0);
						DAQLab_Msg(DAQLAB_MSG_ERR_LOADING_MODULE_CONFIG, moduleInstanceName, NULL, NULL, NULL);
						// dispose of module if not loaded properly
						(*newModule->Discard) (&newModule);
						continue;
					}
				
				// call module load function
				if ( (*newModule->Load) 	(newModule, workspacePanHndl, &errorInfo.errMsg) < 0) {
					DAQLab_Msg(DAQLAB_MSG_ERR_LOADING_MODULE, moduleInstanceName, errorInfo.errMsg, NULL, NULL);
					OKfree(errorInfo.errMsg);
					// dispose of module if not loaded properly
					(*newModule->Discard) 	(&newModule);
					continue;
				}
					
				// insert module to modules list
				ListInsertItem(DAQLabModules, &newModule, END_OF_LIST);
				// increase module instance counter
				DAQLabModules_InitFunctions[j].nInstances++;
				
				// display module panels if method is defined
				if (newModule->DisplayPanels)
					(*newModule->DisplayPanels) (newModule, TRUE);
			}
		
		//free  attributes
		OKfree(*(char**)attrModule[0].pData);
		OKfree(*(char**)attrModule[1].pData);
	   	OKfreeCAHndl(xmlModuleNode);
		
	}
	
	    
	OKfreeCAHndl(xmlNodeList);
	OKfreeCAHndl(xmlModulesNode);
	OKfreeCAHndl(xmlModulesNodeList);
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Connect Task Trees
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	
	for (long i = 0; i < nUITaskControlers; i++) {
		
		// get xml Task Controller Node
		XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(UITaskControllersXMLNodeList, &xmlErrorInfo, i, &UITaskControllerXMLNode) );
		
		errChk( ConnectTaskTrees ((ActiveXMLObj_IXMLDOMElement_)UITaskControllerXMLNode, &xmlErrorInfo, &errorInfo.errMsg) );
		
		// cleanup
		OKfreeCAHndl(UITaskControllerXMLNode);
	}
	
	// cleanup Lex
	OKfreeCAHndl(UITaskControllersXMLNodeList);
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Connect VChans
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	
	errChk( LoadVChanConnections(VChannels, DAQLabCfg_RootElement, &xmlErrorInfo) );
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Connect HW Triggers
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	
	errChk( LoadHWTriggerConnections(HWTrigMasters, HWTrigSlaves, DAQLabCfg_RootElement, &xmlErrorInfo) );
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// discard DOM after loading all settings
	OKfreeCAHndl(DAQLabCfg_DOMHndl);
	OKfreeCAHndl(DAQLabCfg_RootElement);
	
	DAQLabNoSettings:
	
	return 0;
	
Error:
	
	// cleanup
	OKfreeList(TasksUI.UItaskCtrls);
	OKfreeList(DAQLabModules);
	OKfreeList(DAQLabTCs);
	OKfreeList(VChannels);
	OKfreeList(HWTrigMasters);
	OKfreeList(HWTrigSlaves);
	
	return errorInfo.error;
	
XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &DAQLabCfg_DOMHndl, &xmlerror, &xmlErrorInfo, 0);
	
	return xmlerror;
	
}

static int	DAQLab_Save	(void)
{
INIT_ERR

	HRESULT							xmlerror							= 0; 
	ERRORINFO						xmlErrorInfo;
	ActiveXMLObj_IXMLDOMElement_	newXMLElement						= 0;

	// create new DOM
	XMLErrChk ( ActiveXML_NewDOMDocument60IXMLDOMDocument3_ (NULL, 1, LOCALE_NEUTRAL, 0, &DAQLabCfg_DOMHndl) );
	// create new DAQLab Config root element and append it to the DOM
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DAQLabCfg_DOMHndl, &xmlErrorInfo, DAQLAB_CFG_DOM_ROOT_NAME, &newXMLElement) );
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(DAQLabCfg_DOMHndl, &xmlErrorInfo, newXMLElement, &DAQLabCfg_RootElement) );
	OKfreeCAHndl(newXMLElement);   
	// save Tasks and Log panel positions
	int								taskPanTopPos;
	int								taskPanLeftPos;
	int								logPanTopPos;
	int								logPanLeftPos;
	DAQLabXMLNode 					attr1[] = {	{"TaskPanTopPos", BasicData_Int, &taskPanTopPos},
								 				{"TaskPanLeftPos", BasicData_Int, &taskPanLeftPos},
												{"LogPanTopPos", BasicData_Int, &logPanTopPos},
												{"LogPanLeftPos", BasicData_Int, &logPanLeftPos}	};  
	
	errChk( GetPanelAttribute(TasksUI.mainPanHndl, ATTR_LEFT, &taskPanLeftPos) );
	errChk( GetPanelAttribute(TasksUI.mainPanHndl, ATTR_TOP, &taskPanTopPos) );
	errChk( GetPanelAttribute(logPanHndl, ATTR_LEFT, &logPanLeftPos) );
	errChk( GetPanelAttribute(logPanHndl, ATTR_TOP, &logPanTopPos) );
	
	errChk( DLAddToXMLElem (DAQLabCfg_DOMHndl, DAQLabCfg_RootElement, attr1, DL_ATTRIBUTE, NumElem(attr1), &xmlErrorInfo) );
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Save UI Task Controllers
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	UITaskCtrl_type*		UITaskCtrl		= NULL;
	size_t					nUITCs 			= ListNumItems(TasksUI.UItaskCtrls);
	for (size_t i = 1; i <= nUITCs; i++) {
		UITaskCtrl = *(UITaskCtrl_type**)ListGetPtrToItem (TasksUI.UItaskCtrls, i);
		// create new XML UITaskController element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DAQLabCfg_DOMHndl, &xmlErrorInfo, DAQLAB_UITaskController_XML_TAG, &newXMLElement) );
		// add task controller settings
		errChk( DLSaveTaskControllerSettingsToXML(UITaskCtrl->taskControl, DAQLabCfg_DOMHndl, newXMLElement, &xmlErrorInfo) );
		// add child TCs recursively if root TC
		if (!GetTaskControlParent(UITaskCtrl->taskControl))
			errChk( DLSaveUITCTaskTree(UITaskCtrl->taskControl, DAQLabCfg_DOMHndl, newXMLElement, &xmlErrorInfo) );
		// add task controller element to DAQLab root element
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild (DAQLabCfg_RootElement, &xmlErrorInfo, newXMLElement, NULL) );
		// free attributes memory
		OKfreeCAHndl(newXMLElement);
	}
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Save Switchboard VChan connections
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	
	errChk( SaveVChanConnections(VChannels, DAQLabCfg_DOMHndl, DAQLabCfg_RootElement, &xmlErrorInfo) );
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Save HW Trigger connections
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	
	errChk(SaveHWTriggerConnections(HWTrigMasters, DAQLabCfg_DOMHndl, DAQLabCfg_RootElement, &xmlErrorInfo) ); 
	
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	// Save DAQLab Modules
	//---------------------------------------------------------------------------------------------------------------------------------------------------------
	DAQLabModule_type*					DLModule			= NULL;
	size_t								nModules 			= ListNumItems(DAQLabModules);
	ActiveXMLObj_IXMLDOMElement_		modulesXMLElement	= 0;  
	ActiveXMLObj_IXMLDOMElement_		moduleXMLElement	= 0; 
	DAQLabXMLNode 						attr3[2];									// Common module attributes managed by the framework.
																					// Note: module specific attributes must be saved by the module
	
	// create "Modules" XML element and add it to the Root
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DAQLabCfg_DOMHndl, &xmlErrorInfo, DAQLAB_MODULES_XML_TAG, &newXMLElement) );
	XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild(DAQLabCfg_RootElement, &xmlErrorInfo, newXMLElement, &modulesXMLElement) );
	OKfreeCAHndl(newXMLElement);
	
	for (size_t i = 1; i <= nModules; i++) {
		DLModule = *(DAQLabModule_type**)ListGetPtrToItem(DAQLabModules, i);
		// create "Module" XML element and add it to the "Modules" element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (DAQLabCfg_DOMHndl, &xmlErrorInfo, "Module", &newXMLElement) );
		XMLErrChk ( ActiveXML_IXMLDOMElement_appendChild(modulesXMLElement, &xmlErrorInfo, newXMLElement, &moduleXMLElement) );
		OKfreeCAHndl(newXMLElement);
		//-----------------------------------------------------------------------------------
		// Attributes managed by the framework
		//-----------------------------------------------------------------------------------
		// class name
		attr3[0].tag    = "ClassName";
		attr3[0].type	= BasicData_CString;
		attr3[0].pData	= DLModule->className;
		// instance name
		attr3[1].tag    = "InstanceName";
		attr3[1].type	= BasicData_CString;
		attr3[1].pData	= DLModule->instanceName;
		
	
		
		
		//--------------
		
		// add attributes to the module element
		errChk( DLAddToXMLElem(DAQLabCfg_DOMHndl, moduleXMLElement, attr3, DL_ATTRIBUTE, NumElem(attr3), &xmlErrorInfo) );
		// call module saving method if defined
		if (DLModule->SaveCfg)
			XMLErrChk( (*DLModule->SaveCfg) (DLModule, DAQLabCfg_DOMHndl, moduleXMLElement, &xmlErrorInfo) );
		
		OKfreeCAHndl(moduleXMLElement); 
		
	}
	
	//free used handles
	OKfreeCAHndl(modulesXMLElement); 
	OKfreeCAHndl(DAQLabCfg_RootElement);
	
	return 0;
	
Error:
	
	return errorInfo.error;
	
XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, &DAQLabCfg_DOMHndl, &xmlerror, &xmlErrorInfo, 0);
	
	return xmlerror;
	
}

static int DLSaveUITCTaskTree (TaskControl_type* taskController, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_ 		taskControllerXMLElement   	= 0;
	ListType							childTCs					= 0;
	
	char*								tcName						= GetTaskControlName(taskController);
	uInt32								tcExecutionMode				= (uInt32)GetTaskControlExecutionMode(taskController);
	DAQLabXMLNode 						attr[] 						= { {"Name",				BasicData_CString,		tcName}, 
																		{"ExecutionMode",		BasicData_UInt,			&tcExecutionMode} }; 
	
	
	// create new TaskController XML element
	errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_TaskController_XML_TAG, &taskControllerXMLElement) );
	// add task controller attributes
	errChk( DLAddToXMLElem (xmlDOM, taskControllerXMLElement, attr, DL_ATTRIBUTE, NumElem(attr), xmlErrorInfo) );
	// add task controller XML element to parent element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, taskControllerXMLElement, NULL) );
	
	// add recursively child TCs as well
	nullChk( childTCs = GetTaskControlChildTCs(taskController) );
	size_t				nChildTCs	= ListNumItems(childTCs);
	TaskControl_type*	childTC		= NULL;
	for (size_t i =	1; i <= nChildTCs; i++) {
		childTC = *(TaskControl_type**)ListGetPtrToItem(childTCs, i);
		errChk( DLSaveUITCTaskTree(childTC, xmlDOM, taskControllerXMLElement, xmlErrorInfo) );  
	}
	
Error:
	
	// cleanup
	OKfreeList(childTCs);
	OKfree((char*)attr[0].pData); // "Name" field
	OKfreeCAHndl(taskControllerXMLElement);
	
	return errorInfo.error;
}

static int SaveVChanConnections (ListType VChans, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_ 		switchboardXMLElement   	= 0;
	ActiveXMLObj_IXMLDOMElement_ 		sourceVChanXMLElement  		= 0;
	ActiveXMLObj_IXMLDOMElement_ 		sinkVChanXMLElement  		= 0;
	char*								sinkVChanName				= NULL;
	char*								sourceVChanName				= NULL;
	size_t								nVChans						= ListNumItems(VChans);
	VChan_type*							vChan						= NULL;
	SinkVChan_type*						sinkVChan					= NULL;
	ListType							sinkVChans					= 0;   // of SinkVChan_type* elements
	size_t								nSinkVChans					= 0;
	
	
	// create Switchboard xml element
	errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_Switchboard_XML_TAG, &switchboardXMLElement) );
	
	// iterate over all VChan in the provided list
	for (size_t i = 1; i <= nVChans; i++) {
		vChan = *(VChan_type**)ListGetPtrToItem(VChans, i);
		if (GetVChanDataFlowType(vChan) == VChan_Sink) continue; 	// select Source VChans
		if (!IsVChanConnected(vChan)) continue;						// select connected Source VChans 
		
		// create Source VChan xml element
		errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_SourceVChan_XML_TAG, &sourceVChanXMLElement) );
		nullChk( sourceVChanName = GetVChanName(vChan) );
		DAQLabXMLNode sourceVChanAttr[] = { {"Name",		BasicData_CString,		sourceVChanName} };
		// add attributes to source VChan xml element
		errChk( DLAddToXMLElem(xmlDOM, sourceVChanXMLElement, sourceVChanAttr, DL_ATTRIBUTE, NumElem(sourceVChanAttr), xmlErrorInfo) );
		// add sink VChan elements to source VChan
		sinkVChans = GetSinkVChanList((SourceVChan_type*)vChan);
		nSinkVChans = ListNumItems(sinkVChans);
		for (size_t j = 1; j <= nSinkVChans; j++) {
			sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(sinkVChans, j);
			// create sink VChan xml element
			errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_SinkVChan_XML_TAG, &sinkVChanXMLElement) );
			nullChk( sinkVChanName = GetVChanName((VChan_type*)sinkVChan) );
			DAQLabXMLNode sinkVChanAttr[] = { {"Name",		BasicData_CString,		sinkVChanName} };
			// add attributes to sink VChan xml element
			errChk( DLAddToXMLElem(xmlDOM, sinkVChanXMLElement, sinkVChanAttr, DL_ATTRIBUTE, NumElem(sinkVChanAttr), xmlErrorInfo) );
			// add Sink VChan xml element to Source VChan xml element
			errChk( ActiveXML_IXMLDOMElement_appendChild (sourceVChanXMLElement, xmlErrorInfo, sinkVChanXMLElement, NULL) );
			
			// cleanup
			OKfree(sinkVChanName);
			OKfreeCAHndl(sinkVChanXMLElement);
		}
		
		// add Source VChan xml element to parent Switchboard xml element
		errChk( ActiveXML_IXMLDOMElement_appendChild (switchboardXMLElement, xmlErrorInfo, sourceVChanXMLElement, NULL) );
		
		// cleanup
		OKfree(sourceVChanName);
		OKfreeCAHndl(sourceVChanXMLElement)
	}
	
	// add Switchboard xml element to parent xml element
	errChk( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, switchboardXMLElement, NULL) );
	
	// cleanup
	OKfreeCAHndl(switchboardXMLElement);
	
	return 0;
	
Error:
	
	OKfreeCAHndl(switchboardXMLElement);
	OKfreeCAHndl(sourceVChanXMLElement);
	OKfreeCAHndl(sinkVChanXMLElement);
	OKfree(sourceVChanName);
	OKfree(sinkVChanName);
	
	return errorInfo.error;
}

static int SaveHWTriggerConnections (ListType hwTrigMasters, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_ 		hwTriggersXMLElement   		= 0;
	ActiveXMLObj_IXMLDOMElement_ 		masterHWTriggerXMLElement  	= 0;
	ActiveXMLObj_IXMLDOMElement_ 		slaveHWTriggerXMLElement  	= 0;
	char*								masterHWTriggerName			= NULL;
	char*								slaveHWTriggerName			= NULL;
	size_t								nMasterHWTriggers			= ListNumItems(hwTrigMasters);
	size_t								nHWTrigSlaves				= 0;  
	HWTrigMaster_type*					hwTrigMaster				= NULL;
	HWTrigSlave_type*					hwTrigSlave					= NULL;
	
	
	
	// create HWTriggers xml element
	errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_HWTriggers_XML_TAG, &hwTriggersXMLElement) );
	
	// iterate over Master HW Triggers
	for (size_t i = 1; i <= nMasterHWTriggers; i++) {
		hwTrigMaster = *(HWTrigMaster_type**)ListGetPtrToItem(hwTrigMasters, i);
		nHWTrigSlaves = GetNumHWTrigSlaves(hwTrigMaster);
		if (!nHWTrigSlaves) continue; // select only connected Master HW Triggers
		
		// create Master HW Trigger xml element
		errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_HWTrigMaster_XML_TAG, &masterHWTriggerXMLElement) );
		nullChk( masterHWTriggerName = GetHWTrigMasterName(hwTrigMaster) );
		DAQLabXMLNode masterHWTriggerAttr[] = { {"Name",		BasicData_CString,		masterHWTriggerName} };
		// add attributes to Master HW Trigger xml element
		errChk( DLAddToXMLElem(xmlDOM, masterHWTriggerXMLElement, masterHWTriggerAttr, DL_ATTRIBUTE, NumElem(masterHWTriggerAttr), xmlErrorInfo) );
		
		for (size_t j = 1; j <= nHWTrigSlaves; j++) {
			hwTrigSlave = GetHWTrigSlaveFromMaster(hwTrigMaster, j);
			// create Slave HW Trigger xml element
			errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, DAQLAB_HWTrigSlave_XML_TAG, &slaveHWTriggerXMLElement) );
			nullChk( slaveHWTriggerName = GetHWTrigSlaveName(hwTrigSlave) );
			DAQLabXMLNode slaveHWTriggerAttr[] = { {"Name",		BasicData_CString,		slaveHWTriggerName} };
			// add attributes to Slave HW Trigger xml element
			errChk( DLAddToXMLElem(xmlDOM, slaveHWTriggerXMLElement, slaveHWTriggerAttr, DL_ATTRIBUTE, NumElem(slaveHWTriggerAttr), xmlErrorInfo) );
			// add Slave HW Trigger xml element to Master HW Trigger xml element
			errChk( ActiveXML_IXMLDOMElement_appendChild (masterHWTriggerXMLElement, xmlErrorInfo, slaveHWTriggerXMLElement, NULL) );
			
			// cleanup
			OKfree(slaveHWTriggerName);
			OKfreeCAHndl(slaveHWTriggerXMLElement);
		}
		
		// add Master HW Trigger xml element to parent HWTriggers xml element
		errChk( ActiveXML_IXMLDOMElement_appendChild (hwTriggersXMLElement, xmlErrorInfo, masterHWTriggerXMLElement, NULL) );
		
		// cleanup
		OKfree(masterHWTriggerName);
		OKfreeCAHndl(masterHWTriggerXMLElement)
	}
	
	// add HWTriggers xml element to parent xml element
	errChk( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, hwTriggersXMLElement, NULL) );
	
	// cleanup
	OKfreeCAHndl(hwTriggersXMLElement);
	
	return 0;
	
Error:
	
	OKfreeCAHndl(hwTriggersXMLElement);
	OKfreeCAHndl(masterHWTriggerXMLElement);
	OKfreeCAHndl(slaveHWTriggerXMLElement);
	OKfree(masterHWTriggerName);
	OKfree(slaveHWTriggerName);
	
	return errorInfo.error;
}

static int LoadHWTriggerConnections (ListType hwTrigMasters, ListType hwTrigSlaves, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR
	
	ActiveXMLObj_IXMLDOMElement_ 		hwTrigXMLElement   			= 0;
	ActiveXMLObj_IXMLDOMNode_			hwTrigMasterXMLNode			= 0;
	ActiveXMLObj_IXMLDOMNode_			hwTrigSlaveXMLNode			= 0;
	ActiveXMLObj_IXMLDOMNodeList_		hwTrigMasterXMLNodeList		= 0;
	ActiveXMLObj_IXMLDOMNodeList_		hwTrigSlaveXMLNodeList		= 0;
	char*								hwTrigMasterName			= NULL;
	char*								hwTrigSlaveName				= NULL;
	HWTrigMaster_type*					hwTrigMaster				= NULL;
	HWTrigSlave_type*					hwTrigSlave					= NULL;
	long								nHWTrigSlaves				= 0;
	long								nHWTrigMasters				= 0;
	
	
	// get HWTriggers xml element  from parent xml element
	errChk( DLGetSingleXMLElementFromElement(parentXMLElement, DAQLAB_HWTriggers_XML_TAG, &hwTrigXMLElement) );
	if (!hwTrigXMLElement) return 0;
	
	// get list of Master HW Trigger XML elements
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(hwTrigXMLElement, xmlErrorInfo, DAQLAB_HWTrigMaster_XML_TAG, &hwTrigMasterXMLNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(hwTrigMasterXMLNodeList, xmlErrorInfo, &nHWTrigMasters) );
	for (long i = 0; i < nHWTrigMasters; i++) { 
		// get Master HW Trigger XML element
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(hwTrigMasterXMLNodeList, xmlErrorInfo, i, &hwTrigMasterXMLNode) );
		DAQLabXMLNode masterHWTrigAttr[] = { {"Name",		BasicData_CString,		&hwTrigMasterName} };
		// get Source VChan XML element attributes
		errChk( DLGetXMLElementAttributes((ActiveXMLObj_IXMLDOMElement_)hwTrigMasterXMLNode, masterHWTrigAttr, NumElem(masterHWTrigAttr)) );
		
		// check if Master HW Trigger with given name exists in DAQLab, if not, check another Master HW Trigger
		if (!(hwTrigMaster = HWTrigMasterNameExists(hwTrigMasters, hwTrigMasterName, 0))) {
			OKfreeCAHndl(hwTrigMasterXMLNode);
			OKfree(hwTrigMasterName);
			continue;
		}
		
		// get list of Slave HW Trigger XML elements connected to this Master HW Trigger
		errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName((ActiveXMLObj_IXMLDOMElement_)hwTrigMasterXMLNode, xmlErrorInfo, DAQLAB_HWTrigSlave_XML_TAG, &hwTrigSlaveXMLNodeList) );
		errChk ( ActiveXML_IXMLDOMNodeList_Getlength(hwTrigSlaveXMLNodeList, xmlErrorInfo, &nHWTrigSlaves) );
		for (long j = 0; j < nHWTrigSlaves; j++) {
			// get Slave HW Trigger XML element
			errChk ( ActiveXML_IXMLDOMNodeList_Getitem(hwTrigSlaveXMLNodeList, xmlErrorInfo, j, &hwTrigSlaveXMLNode) );
			DAQLabXMLNode slaveHWTrigAttr[] = { {"Name",		BasicData_CString,		&hwTrigSlaveName} };
			// get Slave HW Trigger XML element attributes
			errChk( DLGetXMLElementAttributes((ActiveXMLObj_IXMLDOMElement_)hwTrigSlaveXMLNode, slaveHWTrigAttr, NumElem(slaveHWTrigAttr)) );
		
			// check if Slave HW Trigger with given name exists in DAQLab, if not, check another Slave HW Trigger
			if (!(hwTrigSlave = HWTrigSlaveNameExists(hwTrigSlaves, hwTrigSlaveName, 0))) {
				OKfreeCAHndl(hwTrigSlaveXMLNode);
				OKfree(hwTrigSlaveName);
				continue;
			}
			
			// connect Slave HW Trigger to Master HW Trigger
			errChk(AddHWTrigSlaveToMaster(hwTrigMaster, hwTrigSlave, NULL));
			
			// cleanup
			OKfreeCAHndl(hwTrigSlaveXMLNode);
			OKfree(hwTrigSlaveName);
		}
		
		// cleanup
		OKfreeCAHndl(hwTrigSlaveXMLNodeList);
		OKfreeCAHndl(hwTrigMasterXMLNode);
		OKfree(hwTrigMasterName);
		
	}
	
	// cleanup
	OKfreeCAHndl(hwTrigMasterXMLNodeList);
	OKfreeCAHndl(hwTrigXMLElement);
		
	return 0;
	
Error:
	
	OKfreeCAHndl(hwTrigXMLElement);
	OKfreeCAHndl(hwTrigMasterXMLNode);
	OKfreeCAHndl(hwTrigSlaveXMLNode);
	OKfreeCAHndl(hwTrigMasterXMLNodeList);
	OKfreeCAHndl(hwTrigSlaveXMLNodeList);
	OKfree(hwTrigMasterName);
	OKfree(hwTrigSlaveName);
	
	return errorInfo.error;
}

static int LoadVChanConnections (ListType VChans, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, ERRORINFO* xmlErrorInfo)
{
#define  LoadVChanConnections_Err_WrongVChanDataFlow				-2
#define  LoadVChanConnections_Err_VChansCouldNotBeConnected			-3
	
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_ 		switchboardXMLElement   	= 0;
	ActiveXMLObj_IXMLDOMNode_			sourceVChanXMLNode			= 0;
	ActiveXMLObj_IXMLDOMNode_			sinkVChanXMLNode			= 0;
	ActiveXMLObj_IXMLDOMNodeList_		sourceVChanXMLNodeList		= 0;
	ActiveXMLObj_IXMLDOMNodeList_		sinkVChanXMLNodeList		= 0;
	long								nSourceVChans				= 0;
	long								nSinkVChans					= 0;
	char*								sourceVChanName				= NULL;
	char*								sinkVChanName				= NULL;
	VChan_type*							vChan						= NULL;
	SourceVChan_type*					sourceVChan					= NULL;
	SinkVChan_type*						sinkVChan					= NULL;
	
	// get Switchboard xml element from parent xml element
	errChk( DLGetSingleXMLElementFromElement(parentXMLElement, DAQLAB_Switchboard_XML_TAG, &switchboardXMLElement) );
	if (!switchboardXMLElement) return 0;
	
	// get list of Source VChan XML elements
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(switchboardXMLElement, xmlErrorInfo, DAQLAB_SourceVChan_XML_TAG, &sourceVChanXMLNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(sourceVChanXMLNodeList, xmlErrorInfo, &nSourceVChans) );
	for (long i = 0; i < nSourceVChans; i++) {
		// get Source VChan XML element
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(sourceVChanXMLNodeList, xmlErrorInfo, i, &sourceVChanXMLNode) );
		DAQLabXMLNode sourceVChanAttr[] = { {"Name",		BasicData_CString,		&sourceVChanName} };
		// get Source VChan XML element attributes
		errChk( DLGetXMLElementAttributes((ActiveXMLObj_IXMLDOMElement_)sourceVChanXMLNode, sourceVChanAttr, NumElem(sourceVChanAttr)) );
		
		// check if Source VChan with given name exists in DAQLab, if not, check another Source VChan
		if (!(vChan = VChanNameExists(VChans, sourceVChanName, 0))) {
			OKfreeCAHndl(sourceVChanXMLNode);
			OKfree(sourceVChanName);
			continue;
		}
		
		// check if found vChan is indeed a Source VChan as expected
		if (GetVChanDataFlowType(vChan) != VChan_Source)
			SET_ERR(LoadVChanConnections_Err_WrongVChanDataFlow, "Wrong VChan data flow direction.")
		else
			sourceVChan = (SourceVChan_type*)vChan;
			
		
		// get list of Sink VChan XML elements connected to this Source VChan
		errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName((ActiveXMLObj_IXMLDOMElement_)sourceVChanXMLNode, xmlErrorInfo, DAQLAB_SinkVChan_XML_TAG, &sinkVChanXMLNodeList) );
		errChk ( ActiveXML_IXMLDOMNodeList_Getlength(sinkVChanXMLNodeList, xmlErrorInfo, &nSinkVChans) );
		for (long j = 0; j < nSinkVChans; j++) {
			// get Sink VChan XML element
			errChk ( ActiveXML_IXMLDOMNodeList_Getitem(sinkVChanXMLNodeList, xmlErrorInfo, j, &sinkVChanXMLNode) );
			DAQLabXMLNode sinkVChanAttr[] = { {"Name",		BasicData_CString,		&sinkVChanName} };
			// get Sink VChan XML element attributes
			errChk( DLGetXMLElementAttributes((ActiveXMLObj_IXMLDOMElement_)sinkVChanXMLNode, sinkVChanAttr, NumElem(sinkVChanAttr)) );
		
			// check if Sink VChan with given name exists in DAQLab, if not, check another Sink VChan
			if (!(vChan = VChanNameExists(VChans, sinkVChanName, 0))) {
				OKfreeCAHndl(sinkVChanXMLNode);
				OKfree(sinkVChanName);
				continue;
			}
		
			// check if found vChan is indeed a Source VChan as expected
			if (GetVChanDataFlowType(vChan) != VChan_Sink)
				SET_ERR(LoadVChanConnections_Err_WrongVChanDataFlow, "Wrong data flow direction.")
			else
				sinkVChan = (SinkVChan_type*)vChan;
				
			// connect Sink VChan to Source VChan
			if (!VChan_Connect(sourceVChan, sinkVChan)) {
				errorInfo. error = LoadVChanConnections_Err_VChansCouldNotBeConnected;
				goto Error;
			}
			
			// cleanup
			OKfreeCAHndl(sinkVChanXMLNode);
			OKfree(sinkVChanName);
		}
		
		// cleanup
		OKfreeCAHndl(sinkVChanXMLNodeList);
		OKfreeCAHndl(sourceVChanXMLNode);
		OKfree(sourceVChanName);
	}
	
	// cleanup
	OKfreeCAHndl(sourceVChanXMLNodeList);
	OKfreeCAHndl(switchboardXMLElement);
		
	return 0;
	
Error:
	
	OKfreeCAHndl(switchboardXMLElement);
	OKfreeCAHndl(sourceVChanXMLNode);
	OKfreeCAHndl(sinkVChanXMLNode);
	OKfreeCAHndl(sourceVChanXMLNodeList);
	OKfreeCAHndl(sinkVChanXMLNodeList);
	OKfree(sourceVChanName);
	OKfree(sinkVChanName);
	
	return errorInfo.error;
	
}

static int ConnectTaskTrees (ActiveXMLObj_IXMLDOMElement_ UITCXMLElement, ERRORINFO* xmlErrorInfo, char** errorMsg)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMNodeList_	tcXMLNodeList						= 0;
	ActiveXMLObj_IXMLDOMNodeList_	childTCXMLNodeList					= 0;
	ActiveXMLObj_IXMLDOMNode_		parentTCXMLNode						= 0;
	ActiveXMLObj_IXMLDOMNode_		childTCXMLNode						= 0;
	long							nTCs								= 0;
	long							nChildTCs							= 0;
	char*							parentTCName						= NULL;
	char*							childTCName							= NULL;
	uInt32							tcExecutionMode						= 0;
	TaskControl_type*				parentTC							= NULL;
	TaskControl_type*				childTC								= NULL;
	DAQLabXMLNode 					parentTCAttr[]						= { {"Name",			BasicData_CString,		&parentTCName} };
	DAQLabXMLNode 					childTCAttr[]						= { {"Name",			BasicData_CString,		&childTCName},
																			{"ExecutionMode",	BasicData_UInt,			&tcExecutionMode},	};
	
	
	// get all TC XML elements (not just child TCs but all XML elements with given tag name)
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(UITCXMLElement, xmlErrorInfo, DAQLAB_TaskController_XML_TAG, &tcXMLNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(tcXMLNodeList, xmlErrorInfo, &nTCs) );
	for (long i = 0; i < nTCs; i++) {
		// get parent TC xml node
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(tcXMLNodeList, xmlErrorInfo, i, &parentTCXMLNode) );
		// get child TC xml node list
		errChk( ActiveXML_IXMLDOMElement_GetchildNodes(parentTCXMLNode, xmlErrorInfo, &childTCXMLNodeList) );
		errChk ( ActiveXML_IXMLDOMNodeList_Getlength(childTCXMLNodeList, xmlErrorInfo, &nChildTCs) );
		// get parent TC xml attributes
		errChk( DLGetXMLElementAttributes((ActiveXMLObj_IXMLDOMElement_)parentTCXMLNode, parentTCAttr, NumElem(parentTCAttr)) );
		// try to get parent TC from DAQLab list, if not found, display warning
		parentTC = GetTaskController(parentTCName);
		if (parentTC)
			for (long j = 0; j < nChildTCs; j++) {
				// get child TC xml node
				errChk ( ActiveXML_IXMLDOMNodeList_Getitem(childTCXMLNodeList, xmlErrorInfo, j, &childTCXMLNode) );
				// get child TC xml attributes
				errChk( DLGetXMLElementAttributes((ActiveXMLObj_IXMLDOMElement_)childTCXMLNode, childTCAttr, NumElem(childTCAttr)) );
				// try to get child TC from DAQLab list
				childTC = GetTaskController(childTCName);
				// connect child TC to parent TC
				if (childTC) {
					errChk( AddChildTCToParent(parentTC, childTC, &errorInfo.errMsg) );
					// set child task controller execution mode
					SetTaskControlExecutionMode(childTC, tcExecutionMode); 
				}
					
				// cleanup
				OKfreeCAHndl(childTCXMLNode);
				OKfree(childTCName);
			}
		else {
			// display warning
			DLMsg("Task controller ", 1);
			DLMsg(parentTCName, 0);
			DLMsg(" was not found.\n\n", 0);
		}
		
		OKfreeCAHndl(parentTCXMLNode);
		OKfree(parentTCName);
		OKfreeCAHndl(childTCXMLNodeList);
	}
	
	OKfreeCAHndl(tcXMLNodeList);
				
	return 0;
	
Error:
	
	OKfreeCAHndl(tcXMLNodeList);
	OKfreeCAHndl(childTCXMLNodeList);
	OKfreeCAHndl(parentTCXMLNode);
	OKfreeCAHndl(childTCXMLNode);
	OKfree(parentTCName);
	OKfree(childTCName);
	
RETURN_ERR			
}

static TaskControl_type* GetTaskController (char tcName[])
{
	size_t				nTCs				= ListNumItems(DAQLabTCs);
	TaskControl_type*	taskController		= NULL;
	char*				existingTCName		= NULL;
	
	for (size_t i = 1; i <= nTCs; i++) {
		taskController = *(TaskControl_type**) ListGetPtrToItem(DAQLabTCs, i);
		existingTCName = GetTaskControlName(taskController);
		if (!strcmp(existingTCName, tcName)) {
			OKfree(existingTCName);
			return taskController;	 	// found
		}
		OKfree(existingTCName);
	}
	
	return NULL; 						// not found
}

static int DAQLab_Close (void)
{  
INIT_ERR	
	
	// close task tree panel if open (no need to update VChans & task controllers as modules close)
	OKfreePanHndl(TaskTreeManagerPanHndl);
	VChanSwitchboardPanHndl		= 0;
	HWTrigSwitchboardPanHndl	= 0;
	
	// save settings to XML
	errChk( DAQLab_Save() );
	
	// dispose modules list
	DAQLabModule_empty (&DAQLabModules);
	
	// discard UITCs
	size_t				nUITCs 	= ListNumItems(TasksUI.UItaskCtrls);
	UITaskCtrl_type**   UITCPtr	= NULL;
	for (size_t i = 1; i <= nUITCs; i++) {
		UITCPtr = ListGetPtrToItem(TasksUI.UItaskCtrls, i);
		DAQLab_discard_UITaskCtrl_type(UITCPtr);
	}
	
	ListDispose(TasksUI.UItaskCtrls);
	if (TasksUI.menuBarHndl) DiscardMenuBar(TasksUI.menuBarHndl);

	// discard Task Controller list
	ListDispose(DAQLabTCs);
	
	// discard Task Tree node list
	if (TaskTreeNodes) ListDispose(TaskTreeNodes);
	
	// discard VChans list
	if (VChannels) ListDispose(VChannels);
	
	// discard HW Trigger lists
	if (HWTrigMasters) ListDispose(HWTrigMasters);
	if (HWTrigSlaves) ListDispose(HWTrigSlaves);
	
	
	
	errChk( DAQLab_SaveXMLDOM(DAQLAB_CFG_FILE, &DAQLabCfg_DOMHndl) ); 
	
	// discard DOM after saving all settings
	OKfreeCAHndl(DAQLabCfg_DOMHndl);
	
	DiscardPanel(workspacePanHndl);
	
	QuitUserInterface(0);  
	
	return 0;
	
Error:
	
	MessagePopup("Error", "Could not save new DAQLab settings.");
	
	QuitUserInterface(0);  
	
	return errorInfo.error;
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
INIT_ERR

	int		ctrlID	= 0;
	
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
	
	return errorInfo.error;
}

char* GetStringFromControl (int panelHandle, int stringControlID)
{
	size_t     nChars;
	char*      name; 

	if (GetCtrlAttribute(panelHandle, stringControlID, ATTR_STRING_TEXT_LENGTH, &nChars) < 0) return NULL;
	name = malloc((nChars+1)*sizeof(char));
	if (!name) return NULL;
	
	GetCtrlVal(panelHandle, stringControlID, name);
	RemoveSurroundingWhiteSpace(name);
		
	return name;
}

/// HIFN Checks if all strings in a list are unique.
/// HIPAR idx/ 1-based index of string item in the list that is not unique. If items are unique than this is 0.
/// HIRET True if all strings are unique, False otherwise.
BOOL DLUniqueStrings (ListType stringList, size_t* idx)
{
	char** strPtr1 = NULL;
	char** strPtr2 = NULL;
	
	size_t ni = ListNumItems(stringList);
	for (size_t i = 1; i < ni; i++)
		for (size_t j = i+1; j <= ni; j++) {
			strPtr1 = ListGetPtrToItem (stringList, i);
			strPtr2 = ListGetPtrToItem (stringList, j);
			if (!strcmp(*strPtr1, *strPtr2)) {
				if (idx) *idx = i;
				return FALSE;
			}
		}
	
	if (idx) *idx = 0;
	return TRUE;
}

/// HIFN Copies a list of strings to a new list.
/// HIPAR src/ list of char* null-terminated strings.
/// HIRET New list of strings if succesful, otherwise 0.
ListType StringListCpy(ListType src)
{
	ListType 	dest 		= ListCreate(sizeof(char*));
	size_t 		n 			= ListNumItems(src);
  	char* 		dupStr;
	char** 		stringPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		stringPtr 	= ListGetPtrToItem(src, n);	  
		dupStr 		= StrDup (*stringPtr);
  		ListInsertItem (dest, &dupStr, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
}

/// HIFN Registers a VChan with the DAQLab framework.
/// HIRET TRUE if VChan was added, FALSE if error occured.
BOOL DLRegisterVChan (DAQLabModule_type* mod, VChan_type* VChan)
{
	if (!VChan) return FALSE;
	
	// check if VChan name is unique
	char*			VChanName;
	VChan_type*		existingVChan;
	existingVChan = DLVChanNameExists((VChanName = GetVChanName(VChan)), 0);
	OKfree(VChanName);
	if (existingVChan) return FALSE; // there is already a VChan with the same name
	
	// add VChan to framework
	if (!ListInsertItem(VChannels, &VChan, END_OF_LIST)) return FALSE;
	// add VChan to module list of VChans
	if (mod)
		if (!ListInsertItem(mod->VChans, &VChan, END_OF_LIST)) return FALSE;
	
	UpdateVChanSwitchboard(VChanSwitchboardPanHndl, VChanTab_Switchboard);
	
	return TRUE;
}

/// HIFN Disconnects and removes a VChan from the DAQLab framework.
/// HIRET TRUE if successful, FALSE if error occured or channel not found.
int DLUnregisterVChan (DAQLabModule_type* mod, VChan_type* VChan)
{   
	size_t			DLItemPos;
	size_t			ModItemPos;
	// get VChan item position in DAQLab VChan list
	if (!DLVChanExists(VChan, &DLItemPos))
		return TRUE; // ignore if not found
	
	// get VChan item position in the Module VChan list
	VChanExists(mod->VChans, VChan, &ModItemPos);
	
	// disconnect VChan from other VChans
	VChan_Disconnect(VChan);
	// remove VChan from module list
	ListRemoveItem(mod->VChans, 0, ModItemPos);
	// remove VChan from framework list
	ListRemoveItem(VChannels, 0, DLItemPos);
	// update UI
	UpdateVChanSwitchboard(VChanSwitchboardPanHndl, VChanTab_Switchboard);
	
	return TRUE;
}

void DLUpdateSwitchboard (void)
{
	UpdateVChanSwitchboard(VChanSwitchboardPanHndl, VChanTab_Switchboard);
}

void DLUnregisterModuleVChans (DAQLabModule_type* mod)
{
	size_t			nVChans = ListNumItems(mod->VChans);
	VChan_type**	VChanPtr;
	size_t			DLItemPos;
	
	for(size_t i = nVChans; i; i--) {
		VChanPtr = ListGetPtrToItem(mod->VChans, i);
		// disconnect VChan from other VChans
		VChan_Disconnect(*VChanPtr);
		// remove VChan from framework list
		DLVChanExists(*VChanPtr, &DLItemPos);
		ListRemoveItem(VChannels, 0, DLItemPos);
	}
	ListClear(mod->VChans);
	// update UI
	UpdateVChanSwitchboard(VChanSwitchboardPanHndl, VChanTab_Switchboard);
}

/// HIFN Checks if a VChan object is present in the DAQLab framework.
/// OUT idx 1-based index of VChan object in a ListType list of VChans kept by the DAQLab framework. Pass 0 if this is not needed. If item is not found, *idx is 0.
/// HIRET TRUE if VChan exists, FALSE otherwise. 
BOOL DLVChanExists (VChan_type* VChan, size_t* idx)
{
	return VChanExists(VChannels, VChan, idx);
}

/// HIFN Searches for a given VChan name and if found, return pointer to it.
/// OUT idx 1-based index of VChan object in a ListType list of VChans kept by the DAQLab framework. Pass 0 if this is not needed. If item is not found, *idx is 0.
/// HIRET Pointer to the found VChan_type* if VChan exists, NULL otherwise. 
/// HIRET Pointer is valid until another call to DLRegisterVChan or DLUnregisterVChan. 
VChan_type* DLVChanNameExists (char VChanName[], size_t* idx)
{
	return VChanNameExists (VChannels, VChanName, idx); 
}

/// HIFN Generates a VChan Name having the form "ModuleInstanceName: TaskControllerName: VChanName idx".
/// HIFN ModuleInstanceName, TaskControllerName and idx are optional and should be set to NULL or 0 if not used. 
/// HIRET On success the function generates the requested name, and if there isn't sufficient memory, it returns NULL.
char* DLVChanName (DAQLabModule_type* mod, TaskControl_type* taskController, char VChanName[], uInt32 idx)
{
INIT_ERR

	char*	name		 	= NULL;
	char*	tcName			= NULL;
	char	idxString[50]	= "";
	
	nullChk( name = StrDup("") );
	
	// add module instance name
	if (mod) {
		nullChk( AppendString(&name, mod->instanceName, -1) );
		nullChk( AppendString(&name, "- ", -1) );
	}
	
	// add task controller name
	if (taskController) {
		nullChk( tcName = GetTaskControlName(taskController) );
		nullChk( AppendString(&name, tcName, -1) );
		OKfree(tcName);
		nullChk( AppendString(&name, "- ", -1) );
	}
	
	// add VChan name
	nullChk( AppendString(&name, VChanName, -1) );
	
	// add indexing if needed
	if (idx) {
		Fmt(idxString, " %d", idx);
		nullChk( AppendString(&name, idxString, -1) );
	}
	
	return name;
	
Error:
	
	OKfree(name);
	OKfree(tcName);
	
	return NULL;
}

/// HIFN Validate function for VChan names.
/// HIRET True if newVChanName is unique among the framework VChan names, False otherwise
BOOL DLValidateVChanName (char newVChanName[], void* null)
{
	if (DLVChanNameExists(newVChanName, 0))
		return FALSE;
	else
		return TRUE;
}

/// HIFN Renames a VChan in the DAQLab framework.
/// HIRET True if renaming was successful, False otherwise.
BOOL DLRenameVChan (VChan_type* VChan, char newName[])
{
	// validate new VChan name
	if (!DLValidateVChanName(newName, NULL)) return FALSE;
	
	// assign new name
	SetVChanName(VChan, newName);
	
	// update UI
	UpdateVChanSwitchboard(VChanSwitchboardPanHndl, VChanTab_Switchboard);
	
	return TRUE;
}

BOOL DLRegisterHWTrigMaster (HWTrigMaster_type* master)
{
	if (!master) return FALSE;
	
	// check if HW Trig Master name is unique
	char*					masterName;
	HWTrigMaster_type*		existingMaster;
	existingMaster = HWTrigMasterNameExists(HWTrigMasters, (masterName = GetHWTrigMasterName(master)), 0);
	OKfree(masterName);
	if (existingMaster) return FALSE; // there is already a HW Trig Master with the same name
	
	// add master HW trigger to DAQLab framework
	if (!ListInsertItem(HWTrigMasters, &master, END_OF_LIST)) return FALSE;
	
	// update UI
	if (TaskTreeManagerPanHndl)
		DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
	
	return TRUE;
}

BOOL DLUnregisterHWTrigMaster (HWTrigMaster_type* master)
{
	if (!master) return TRUE;
	size_t 					nMasters = ListNumItems(HWTrigMasters);
	HWTrigMaster_type**		masterPtr;
	for (size_t i = 1; i <= nMasters; i++) {
		masterPtr = ListGetPtrToItem(HWTrigMasters, i);
		if (*masterPtr == master) {
			// remove from DAQLab framework
			ListRemoveItem(HWTrigMasters, 0, i);
			// disconnect from slaves
			RemoveHWTrigMasterFromSlaves(master);
			// update UI
			if (TaskTreeManagerPanHndl)
				DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
			
			return TRUE; // item found and removed
		}
		
	}
	
	return FALSE; // item not found
}

BOOL DLRegisterHWTrigSlave (HWTrigSlave_type* slave)
{
	if (!slave) return FALSE;
	
	// add slave HW trigger to DAQLab framework
	if (!ListInsertItem(HWTrigSlaves, &slave, END_OF_LIST)) return FALSE;
	
	// update UI
	if (TaskTreeManagerPanHndl)
		DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
	
	return TRUE;
}

BOOL DLUnregisterHWTrigSlave (HWTrigSlave_type* slave)
{
	if (!slave) return TRUE;
	
	size_t 					nSlaves = ListNumItems(HWTrigSlaves);
	HWTrigSlave_type**		slavePtr;
	for (size_t i = 1; i <= nSlaves; i++) {
		slavePtr = ListGetPtrToItem(HWTrigSlaves, i);
		if (*slavePtr == slave) {
			// disconnect slave from master
			RemoveHWTrigSlaveFromMaster(slave);
			// remove from DAQLab framework
			ListRemoveItem(HWTrigSlaves, 0, i);
			// update UI
			if (TaskTreeManagerPanHndl)
				DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
			return TRUE; // item found and removed
		}
		
	}
	
	return FALSE; // item not found
}

void DLMsg(const char* text, BOOL beep)
{
	if (!text) return;
	
	SetCtrlVal(logPanHndl, LogPan_LogBox, text);
	if (beep) {
		DisplayPanel(logPanHndl); // bring to focus
		Beep();
	}
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
			} else
				DLMsg("\n", 0);
			break;
			
		case DAQLAB_MSG_ERR_LOADING_MODULE_CONFIG:
			DLMsg("Error: Could not load configuration for module ", 1);
			DLMsg((char*)data1, 0);	// module name 
			if (data2) {
				DLMsg(" Reason:\n\t", 0);
				DLMsg((char*)data2, 0); // error description
				DLMsg("\n\n", 0);
			} else
				DLMsg("\n", 0);
			break;
			
		case DAQLAB_MSG_ERR_VCHAN_NOT_FOUND:
			name = GetVChanName(data1);
			Fmt(buff, "Error: Could not find virtual channel \"%s\" in the DAQLab framework.", name);
			DLMsg(buff, 1);
			OKfree(name);
			break;
			
		case DAQLAB_MSG_ERR_NOT_UNIQUE_CLASS_NAME:
			DLMsg((char*)data1, 1);
			DLMsg(" is not a unique module class name.\n\n", 0);
			break;
			
		case DAQLAB_MSG_ERR_NOT_UNIQUE_INSTANCE_NAME:
			DLMsg((char*)data1, 1);
			DLMsg(" is not a unique module instance name.\n\n", 0);
			break;
			
		case DAQLAB_MSG_ERR_NOT_UNIQUE_TASKCONTROLLER_NAME:
			DLMsg((char*)data1, 1);
			DLMsg(" is not a unique Task Controller name.\n\n", 0);
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
	ERRORINFO							xmlErrorInfo;
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
	xmlerror = ActiveXML_IXMLDOMDocument3_load (*xmlDOM, &xmlErrorInfo, CA_VariantBSTR(bstrFileName), &xmlLoaded);
	t2 = Timer();
	
	if (xmlLoaded == VTRUE)	{
		DLMsg("loaded in ", 0);
		Fmt(timeStr, "%s<%f[p2] s.\n\n", t2 - t1);
		DLMsg(timeStr, 0);
	} else
		DAQLab_Msg(DAQLAB_MSG_WARN_NO_CFG_FILE, xmlDOM, &xmlerror, &xmlErrorInfo, 0);
	
	// if file was loaded check root element name if it matches the name defined in DAQLAB_CFG_DOM_ROOT_NAME 
	if (xmlLoaded == VARIANT_TRUE) {
		// get document root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_GetdocumentElement (*xmlDOM, &xmlErrorInfo, rootElement) );
		
		// check if root element name is correct
		XMLErrChk ( ActiveXML_IXMLDOMElement_GettagName(*rootElement, &xmlErrorInfo, &rootElementName) );
		
		if(strcmp(rootElementName, DAQLAB_CFG_DOM_ROOT_NAME)) {
			rootElementNameOK = FALSE;
			Fmt(msg, "Root element %s was not found. Empty DOM will be used instead.\n\n", DAQLAB_CFG_DOM_ROOT_NAME);
			DLMsg(msg, 1);
		} else
			rootElementNameOK = TRUE;
	} 
	
	if (xmlLoaded == VARIANT_FALSE || !rootElementNameOK) {
		// create root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_createElement (*xmlDOM, &xmlErrorInfo, DAQLAB_CFG_DOM_ROOT_NAME, &newElement) );
	
		// append root element to DOM and return handle to root element
		XMLErrChk ( ActiveXML_IXMLDOMDocument3_appendChild(*xmlDOM, &xmlErrorInfo, newElement, rootElement) );
		
		foundXMLDOMFlag = 0;
	} 
	
	   
	
	return foundXMLDOMFlag;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, xmlDOM, &xmlerror, &xmlErrorInfo, 0);
	
	return xmlerror;
	
}

static int	DAQLab_SaveXMLDOM (const char fileName[], CAObjHandle* xmlDOM)
{
	HRESULT				xmlerror;
	ERRORINFO			xmlErrorInfo;
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
	XMLErrChk ( ActiveXML_IXMLDOMDocument3_save(*xmlDOM, &xmlErrorInfo, CA_VariantBSTR(bstrFileName)) );
	
	t2 = Timer();
	
	DLMsg("saved in ", 0);
	Fmt(timeStr, "%s<%f[p2] s.\n\n", t2 - t1);
	DLMsg(timeStr, 0);
	
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML, xmlDOM, &xmlerror, &xmlErrorInfo, 0);
	
	return xmlerror;
}

/// HIFN Adds a list of attributes or elements to an element
/// HIPAR nodeTypeFlag/ set 1 to add elements or 0 to add attributes
int	DLAddToXMLElem (CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ parentXMLElement, DAQLabXMLNode childXMLNodes[], DAQLabXMLNodeTypes nodeType, size_t nNodes, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_	newXMLElement		= 0;
	VARIANT							xmlVal				= CA_VariantNULL ();
	BSTR							bstrVal;
	
	for (size_t i = 0; i < nNodes; i++) {
		
		// ignore elements or attributes that have NULL data
		if (!childXMLNodes[i].pData) continue;
			
		if (nodeType == DL_ELEMENT)
			// create new element
			errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, childXMLNodes[i].tag, &newXMLElement) );
	
		// convert to variant
		switch (childXMLNodes[i].type) {
			
			case BasicData_Null:
				
				xmlVal = CA_VariantNULL ();
				break;
			
			case BasicData_Bool:
				
				xmlVal = CA_VariantBool ((VBOOL)*(BOOL*)childXMLNodes[i].pData);
				break;
				
			case BasicData_Char:
				
				xmlVal = CA_VariantChar(*(char*)childXMLNodes[i].pData);
				break;
				
			case BasicData_UChar:
				
				xmlVal = CA_VariantUChar(*(unsigned char*)childXMLNodes[i].pData);
				break;
				
			case BasicData_Short:
				
				xmlVal = CA_VariantShort(*(short*)childXMLNodes[i].pData);
				break;
				
			case BasicData_UShort:
				
				xmlVal = CA_VariantUShort(*(unsigned short*)childXMLNodes[i].pData);
				break;
			
			case BasicData_Int:
				
				xmlVal = CA_VariantInt(*(int*)(childXMLNodes[i].pData));
				break;							   
				
			case BasicData_UInt:
				
				xmlVal = CA_VariantUInt(*(unsigned int*)childXMLNodes[i].pData);
				break;
				
			case BasicData_Int64:
				
				xmlVal = CA_VariantLongLong(*(long long*)childXMLNodes[i].pData);
				break;
				
			case BasicData_UInt64:
				
				xmlVal = CA_VariantULongLong(*(unsigned long long*)childXMLNodes[i].pData);
				break;
			
			case BasicData_Float:
				
				xmlVal = CA_VariantFloat(*(float*)childXMLNodes[i].pData);
				break;
			
			case BasicData_Double:
				
				xmlVal = CA_VariantDouble(*(double*)childXMLNodes[i].pData);
				break;
				
			case BasicData_CString:
				
				CA_CStringToBSTR((char*)childXMLNodes[i].pData, &bstrVal); 
				xmlVal = CA_VariantBSTR(bstrVal);
				break;
		}
		
		switch (nodeType) {
			
			case DL_ELEMENT:
				
				// set element typed value
				errChk ( ActiveXML_IXMLDOMElement_SetnodeTypedValue(newXMLElement, xmlErrorInfo, xmlVal) );
				// add new element as child
				errChk ( ActiveXML_IXMLDOMElement_appendChild (parentXMLElement, xmlErrorInfo, newXMLElement, NULL) );
				// cleanup
				OKfreeCAHndl(newXMLElement);
				
				break;
				
			case DL_ATTRIBUTE:
				
				// add attribute to element
				errChk ( ActiveXML_IXMLDOMElement_setAttribute(parentXMLElement, xmlErrorInfo, childXMLNodes[i].tag, xmlVal) );  
				
				break;
		}
		
	}
	
	
	return 0;
	
Error:
	
return errorInfo.error;	
}

static int DAQLab_StringToType	(char text[], BasicDataTypes vartype, void* valuePtr)
{
INIT_ERR
	
	switch (vartype) {
		
		case BasicData_Null:
			break;
		case BasicData_Bool:
			errChk( Scan(text, "%s>%d", (BOOL*)valuePtr) );
			break;
		case BasicData_Char:
			errChk( Scan(text, "%s>%d[b1]", (char*)valuePtr) );
			break;
		case BasicData_UChar:
			errChk( Scan(text, "%s>%d[b1u]", (unsigned char*)valuePtr) );
			break;
		case BasicData_Short:
			errChk( Scan(text, "%s>%d[b2]", (short*)valuePtr) );
			break;
		case BasicData_UShort:
			errChk( Scan(text, "%s>%d[b2u]", (unsigned short*)valuePtr) );
			break;
		case BasicData_Int:
			errChk( Scan(text, "%s>%d[b4]", (int*)valuePtr) );
			break;
		case BasicData_UInt:
			errChk( Scan(text, "%s>%d[b4u]", (unsigned int*)valuePtr) );
			break;
		case BasicData_Int64:
			errChk( Scan(text, "%s>%d[b8]", (long long*)valuePtr) );
			break;
		case BasicData_UInt64:
			errChk( Scan(text, "%s>%d[b8u]", (unsigned long long*)valuePtr) );
			break;
		case BasicData_Float:
			double doubleVal;
			errChk( Scan(text, "%s>%f", &doubleVal) );  
			*(float*)valuePtr = (float) doubleVal;
			break;
		case BasicData_Double:
			errChk( Scan(text, "%s>%f", &doubleVal) );  
			*(double*)valuePtr = doubleVal;
			break;
		case BasicData_CString:
			*(char**)valuePtr = StrDup(text);
			break;
	}
		
	return 0;
		
Error:  // conversion error
	
	return errorInfo.error;;
	
}

int DLGetXMLElementAttributes (ActiveXMLObj_IXMLDOMElement_ XMLElement, DAQLabXMLNode Attributes[], size_t nAttributes)
{
INIT_ERR

	HRESULT								xmlerror;
	ERRORINFO							xmlErrorInfo;
	ActiveXMLObj_IXMLDOMAttribute_		xmlAttribute;
	char*								attributeString;	
	
	for (size_t i = 0; i < nAttributes; i++) {
		// get attribute node
		XMLErrChk ( ActiveXML_IXMLDOMElement_getAttributeNode(XMLElement, &xmlErrorInfo, Attributes[i].tag, &xmlAttribute) );
		if (!xmlAttribute) continue; // ignore attributes that were not found
		// get attribute text
		XMLErrChk ( ActiveXML_IXMLDOMAttribute_Gettext(xmlAttribute, &xmlErrorInfo, &attributeString) );
		OKfreeCAHndl(xmlAttribute);
		// convert string to values
		errChk( DAQLab_StringToType(attributeString, Attributes[i].type, Attributes[i].pData) );
		CA_FreeMemory(attributeString);
	}
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES, &xmlerror, 0, 0, 0);
	
	return xmlerror;
	
Error:  // conversion error
	
	return errorInfo.error;

}

int DLGetXMLNodeAttributes (ActiveXMLObj_IXMLDOMNode_ XMLNode, DAQLabXMLNode Attributes[], size_t nAttributes)
{
INIT_ERR
	
	HRESULT								xmlerror;
	ERRORINFO							xmlErrorInfo;
	char*								attributeString;
	ActiveXMLObj_IXMLDOMNamedNodeMap_	xmlNamedNodeMap;	 // list of attributes
	ActiveXMLObj_IXMLDOMNode_			xmlAttributeNode;	 // selected attribute
	
	// get list of attributes
	XMLErrChk ( ActiveXML_IXMLDOMNode_Getattributes(XMLNode, &xmlErrorInfo, &xmlNamedNodeMap) );  
	
	for (size_t i = 0; i < nAttributes; i++) {
		// get attribute node
		XMLErrChk ( ActiveXML_IXMLDOMNamedNodeMap_getNamedItem(xmlNamedNodeMap, &xmlErrorInfo, Attributes[i].tag, &xmlAttributeNode) );
		// get attribute node text
		XMLErrChk ( ActiveXML_IXMLDOMNode_Gettext(xmlAttributeNode, &xmlErrorInfo, &attributeString) );
		OKfreeCAHndl(xmlAttributeNode);
		// convert to values
		errChk( DAQLab_StringToType (attributeString, Attributes[i].type, Attributes[i].pData) );
		CA_FreeMemory(attributeString);
	}
	
	OKfreeCAHndl(xmlNamedNodeMap); 
	
	return 0;
	
	XMLError:
	
	DAQLab_Msg(DAQLAB_MSG_ERR_ACTIVEXML_GETATTRIBUTES, &xmlerror, 0, 0, 0);
	
	return xmlerror;
	
Error:  // conversion error
	
	return errorInfo.error;
	
}

int DLGetSingleXMLElementFromElement (ActiveXMLObj_IXMLDOMElement_ parentXMLElement, char elementName[], ActiveXMLObj_IXMLDOMElement_* childXMLElement)
{
#define DLGetSingleXMLElementFromElement_Err_NotASingleElement		-1
	HRESULT								xmlerror;
	ERRORINFO							xmlErrorInfo;
	ActiveXMLObj_IXMLDOMNodeList_		xmlNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_			xmlNode			= 0; 
	long								nXMLElements	= 0;

	XMLErrChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(parentXMLElement, &xmlErrorInfo, elementName, &xmlNodeList) );
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getlength(xmlNodeList, &xmlErrorInfo, &nXMLElements) );
	
	// if there are no elements, return null element
	if (!nXMLElements) {
		*childXMLElement = 0;
		// cleanup
		if (xmlNodeList) OKfreeCAHndl(xmlNodeList);
		return 0;
	}
	
	XMLErrChk ( ActiveXML_IXMLDOMNodeList_Getitem(xmlNodeList, &xmlErrorInfo, 0, &xmlNode) );
	
	// cleanup
	if (xmlNodeList) OKfreeCAHndl(xmlNodeList);
	
	*childXMLElement = (ActiveXMLObj_IXMLDOMElement_) xmlNode;
	
	return 0;
	
XMLError: 
	
	// cleanup
	if (xmlNodeList) OKfreeCAHndl(xmlNodeList);
	if (xmlNode) OKfreeCAHndl(xmlNode); 
	
	return xmlerror;
}

int DLLoadTaskControllerSettingsFromXML	(TaskControl_type* taskController, ActiveXMLObj_IXMLDOMElement_ taskControllerSettingsXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	char*								tcName				= NULL;
	uInt32								tcNIter				= 0;
	double								tcWait				= 0;
	uInt32								tcExecutionMode		= 0;
	BOOL								tcIterationMode		= 0;
	DAQLabXMLNode 						attr[] 				= { {"Name",				BasicData_CString,		&tcName},
																{"NIterations",			BasicData_UInt,			&tcNIter},
																{"Wait",				BasicData_Double,		&tcWait},
																{"ExecutionMode",		BasicData_UInt,			&tcExecutionMode},
																{"IterationMode",		BasicData_Bool,			&tcIterationMode} };
	// get task controller settings														
	errChk( DLGetXMLElementAttributes(taskControllerSettingsXMLElement, attr, NumElem(attr)) );
	
	// set task controller settings
	SetTaskControlName(taskController, tcName);
	OKfree(tcName);
	SetTaskControlIterations(taskController, tcNIter);
	SetTaskControlIterationsWait(taskController, tcWait);
	SetTaskControlExecutionMode(taskController, (TCExecutionModes) tcExecutionMode);
	SetTaskControlMode(taskController, (TaskMode_type) tcIterationMode);
	
	return 0;
	
Error:
	
	OKfree(tcName);
	
	return errorInfo.error;
	
}

static UITaskCtrl_type*	DAQLab_init_UITaskCtrl_type (TaskControl_type* taskControl)
{
INIT_ERR

	UITaskCtrl_type* 	newUItaskCtrl 	= malloc(sizeof(UITaskCtrl_type));
	char*				name			= NULL;
	
	if (!newUItaskCtrl) return NULL;
	
	// duplicate Task Controller panel
	errChk( newUItaskCtrl->panHndl = DuplicatePanel(TasksUI.mainPanHndl, TasksUI.controllerPanHndl, "", 0, DAQLAB_TASKCONTROLLERS_PAN_MARGIN) );
	newUItaskCtrl->taskControl = taskControl;
	nullChk( newUItaskCtrl->dataStorageSinkVChans = ListCreate(sizeof(SinkVChan_type*)) );
	
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
	SetCtrlVal(newUItaskCtrl->panHndl, TCPan1_Mode, !GetTaskControlMode(taskControl));
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

static void	DAQLab_discard_UITaskCtrl_type	(UITaskCtrl_type** UITaskCtrlPtr)
{
	UITaskCtrl_type*	UITaskCtrl = *UITaskCtrlPtr;	
	
	if (!UITaskCtrl) return;
	
	// disconnect SinkVChans from task controller
	RemoveAllSinkVChans(UITaskCtrl->taskControl, NULL);
	// discard task controller
	discard_TaskControl_type(&UITaskCtrl->taskControl);
	
	// discard SinkVChans
	size_t				nVChans 		= ListNumItems(UITaskCtrl->dataStorageSinkVChans);
	SinkVChan_type**	sinkVChanPtr	= NULL;
	for (size_t i = 1; i <= nVChans; i++) {
		sinkVChanPtr = ListGetPtrToItem(UITaskCtrl->dataStorageSinkVChans, i);
		discard_VChan_type((VChan_type**)sinkVChanPtr);
	}
	OKfreeList(UITaskCtrl->dataStorageSinkVChans);
	
	DiscardPanel(UITaskCtrl->panHndl);
	
	OKfree(*UITaskCtrlPtr);
}

/// HIFN Adds a Task Controller to the user interface and to the framework
/// HIRET 0 for success, -1 error.
static UITaskCtrl_type* DAQLab_AddTaskControllerToUI (TaskControl_type* taskControl)    
{
INIT_ERR

	UITaskCtrl_type*	newUItaskCtrlPtr;
	
	if (!taskControl) return NULL;
	
	newUItaskCtrlPtr = DAQLab_init_UITaskCtrl_type(taskControl);
	
	nullChk(ListInsertItem(TasksUI.UItaskCtrls, &newUItaskCtrlPtr, END_OF_LIST) );
	
	// add Task Controller to the framework
	DLAddTaskController (NULL, taskControl);
	
	DAQLab_RedrawTaskControllerUI();
	
	return newUItaskCtrlPtr;
	
Error:
	
	DAQLab_discard_UITaskCtrl_type(&newUItaskCtrlPtr);
	
	return NULL;
}

/// HIFN Removes a Task Controller from the user interface.
/// HIPAR index/ provide a 1-based index for the Task Controller in the TasksUI.UItaskCtrls list 
/// HIRET 0 for success, -1 function error, -2 cannot remove Task Controllers in the following states: RUNNING, STOPPING, ITERATING, WAITING.
static int RemoveTaskControllerFromUI (int index, char** errorMsg)
{
#define		RemoveTaskControllerFromUI_Err_TaskControllerIsInUse				-1   
#define		RemoveTaskControllerFromUI_Err_TaskControllerNotExistingAnymore		-2
	
INIT_ERR	
	
	UITaskCtrl_type*	UITaskCtrl 				= *(UITaskCtrl_type**)ListGetPtrToItem (TasksUI.UItaskCtrls, index);
	BOOL				tcIsInUse				= FALSE;
	BOOL				tcIsInUseLockObtained 	= FALSE;
	char*				tcName					= NULL;
	ListType 			tcList					= 0;
	char*				msgBuff					= NULL;
	
	
	if (!UITaskCtrl) {
		errorInfo.error 	= RemoveTaskControllerFromUI_Err_TaskControllerNotExistingAnymore;
		errorInfo.errMsg 	= StrDup("Requested task controller could not be found and cannot be removed from DAQLab");
		goto Error;
	}
	
	// check if Task Controller is in use
	errChk( IsTaskControllerInUse_GetLock(UITaskCtrl->taskControl, &tcIsInUse, NULL, &tcIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
	
	if (tcIsInUse) {
		nullChk( msgBuff = StrDup("Task controller ") );
		nullChk( tcName = GetTaskControlName(UITaskCtrl->taskControl) );
		nullChk( AppendString(&msgBuff, tcName, -1) );
		nullChk( AppendString(&msgBuff, " cannot be removed from DAQLab while it is in use", -1) );
		SET_ERR(RemoveTaskControllerFromUI_Err_TaskControllerIsInUse, msgBuff);
	}
	
	// remove from DAQLab framework
	nullChk( tcList = ListCreate(sizeof(TaskControl_type*)) );
	nullChk( ListInsertItem(tcList, &UITaskCtrl->taskControl, END_OF_LIST) );
	
	UITaskCtrl_type*	removedUITC = NULL;
	ListRemoveItem(TasksUI.UItaskCtrls, &removedUITC, index);  
	if (!DLRemoveTaskControllers (NULL, tcList))
		return -1;
	
	// release lock
	if (tcIsInUseLockObtained) 
		errChk( IsTaskControllerInUse_ReleaseLock(UITaskCtrl->taskControl, &tcIsInUseLockObtained, &errorInfo.errMsg) );
	
	DAQLab_discard_UITaskCtrl_type(&removedUITC);
	
	DAQLab_RedrawTaskControllerUI();
	
Error:
	
	// release lock
	if (tcIsInUseLockObtained) 
		IsTaskControllerInUse_ReleaseLock(UITaskCtrl->taskControl, &tcIsInUseLockObtained, &errorInfo.errMsg);
	
	OKfree(tcName);
	OKfreeList(tcList);
	OKfree(msgBuff);

RETURN_ERR
}

/// HIFN Removes a list of Task Controllers provided by tcList as TaskControl_type* list elements from the DAQLab framework.
/// HIRET True if all Task Controllers were removed successfully or if there are no Task Controllers to be removed, False otherwise.
BOOL DLRemoveTaskControllers (DAQLabModule_type* mod, ListType tcList)
{
	// remove from the framework list
	RemoveTaskControllersFromList(DAQLabTCs, tcList);
	
	// remove from the module's list of Task Controllers
	if (mod)
		RemoveTaskControllersFromList(mod->taskControllers, tcList);
	
	// update Task Tree if it is displayed
	if (TaskTreeManagerPanHndl)
		DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
	
	return TRUE;
}

BOOL DLRemoveTaskController (DAQLabModule_type* mod, TaskControl_type* taskController)
{
	// remove from the module's list of Task Controllers  
	if (mod)
		RemoveTaskControllerFromList(mod->taskControllers, taskController);
	
	// remove from the framework list
	RemoveTaskControllerFromList(DAQLabTCs, taskController);
	
	// update Task Tree if it is displayed
	if (TaskTreeManagerPanHndl)
		DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
	
	return TRUE;
}

CmtThreadPoolHandle	DLGetCommonThreadPoolHndl (void)
{
	return DLThreadPoolHndl;
}

/// HIFN Adds a list of Task Controllers provided by tcList as TaskControl_type* list elements to the DAQLab framework if their names are unique.
/// HIRET TRUE if successful or if list is empty.
/// HIRET FALSE if Task Controller names in tcList and the framework are not unique.
BOOL DLAddTaskControllers (DAQLabModule_type* mod, ListType tcList)
{
	ListType 			tcNamesList 	= ListCreate(sizeof(char*));
	size_t				nTCs			= ListNumItems(tcList);
	char*				tcName;
	char**				tcNamePtr;
	TaskControl_type**  tcPtr;
	size_t				tcNameIdx;
	
	if (!nTCs) return TRUE;
	
	//---------------------------------------------------------
	// check if the provided Task Controllers have unique names
	//---------------------------------------------------------
	
	// build up list with Task Controller names
	for (size_t i = 1; i <= nTCs; i++) {
		tcPtr = ListGetPtrToItem(tcList, i);
		tcName = GetTaskControlName(*tcPtr);
		ListInsertItem(tcNamesList, &tcName, END_OF_LIST);
	}
	// check if names are unique among themselves
	if (!DLUniqueStrings(tcNamesList, &tcNameIdx)) {
		tcNamePtr = ListGetPtrToItem(tcNamesList, tcNameIdx);
		DAQLab_Msg(DAQLAB_MSG_ERR_NOT_UNIQUE_TASKCONTROLLER_NAME, *tcNamePtr, NULL, NULL, NULL);	
		ListDisposePtrList(tcNamesList);
		return FALSE;	
	}
	
	// check if names are unique within the framework
	for (size_t i = 1; i <= nTCs; i++) {
		tcNamePtr = ListGetPtrToItem(tcNamesList, i);
		if (!DLValidTaskControllerName(*tcNamePtr)) {
			DAQLab_Msg(DAQLAB_MSG_ERR_NOT_UNIQUE_TASKCONTROLLER_NAME, *tcNamePtr, NULL, NULL, NULL);	
			return FALSE;
		}
	}
	
	ListDispose(tcNamesList);  
	
	// add Task Controllers to the module's list of Task Controllers if they belong to a module
	if (mod)
		ListAppend(mod->taskControllers, tcList);
	// add Task Controllers to the framework
	ListAppend(DAQLabTCs, tcList);
	
	// add UI resources for task controller execution logging
	for (size_t i = 1; i <= nTCs; i++) {
		tcPtr = ListGetPtrToItem(tcList, i);
		SetTaskControlUILoggingInfo(*tcPtr, taskLogPanHndl, TaskLogPan_LogBox);
	}
	
	// update Task Tree if it is displayed
	if (TaskTreeManagerPanHndl)
		DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
	
	return TRUE;
}

/// HIFN Adds a Task Controller to the DAQLab framework if its names is unique among existing Task Controllers.
/// HIRET TRUE if successful, FALSE if the provided Task Controller name is not unique within the framework.
BOOL DLAddTaskController (DAQLabModule_type* mod, TaskControl_type* taskController)
{
	// check if Task Controller name is unique
	char* tcName = GetTaskControlName(taskController);
	if (!DLValidTaskControllerName(tcName)) {
		DAQLab_Msg(DAQLAB_MSG_ERR_NOT_UNIQUE_TASKCONTROLLER_NAME, tcName, NULL, NULL, NULL);
		OKfree(tcName);
		return FALSE;
	}
	
	// add Task Controller to the module's list of Task Controllers if it belongs to a module
	if (mod)  {
		ListInsertItem(mod->taskControllers, &taskController, END_OF_LIST);
	}
	OKfree(tcName);
	// add Task Controller to the framework's list of Task Controllers
	ListInsertItem(DAQLabTCs, &taskController, END_OF_LIST);
	
	// add UI resources for task controller execution logging     
	SetTaskControlUILoggingInfo(taskController, taskLogPanHndl, TaskLogPan_LogBox);  
	
	// update the Task Tree if it is displayed
	if (TaskTreeManagerPanHndl)
		DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
	
	return TRUE;
}

static void	DAQLab_RedrawTaskControllerUI (void)
{
	size_t					nControllers = ListNumItems(TasksUI.UItaskCtrls);
	UITaskCtrl_type**		controllerPtrPtr;
	int						menubarHeight;
	
	GetPanelAttribute(TasksUI.mainPanHndl, ATTR_MENU_HEIGHT, &menubarHeight);
	
	if (!nControllers) {
		// adjust size of Tasks panel to match the size of one Task Controller panel
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_HEIGHT, TasksUI.controllerPanHeight + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN); 
		
		// dimm delete menu item since there are no Task Controllers
		SetMenuBarAttribute(TasksUI.menuBarHndl, TasksUI.menuManageItem_Delete, ATTR_DIMMED, 1);
		return;
	}
	
	// undim delete and task tree menu items 
	SetMenuBarAttribute(TasksUI.menuBarHndl, TasksUI.menuManageItem_Delete, ATTR_DIMMED, 0);
	
	// reposition Task Controller panels and display them
	for (size_t i = 1; i <= nControllers; i++) {
		controllerPtrPtr = ListGetPtrToItem(TasksUI.UItaskCtrls, i);
		SetPanelAttribute((*controllerPtrPtr)->panHndl, ATTR_TOP, (TasksUI.controllerPanHeight + DAQLAB_TASKCONTROLLERS_SPACING) * (i-1) + DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);  
		DisplayPanel((*controllerPtrPtr)->panHndl);
	}
	
	// adjust Tasks panel height and insert scrollbar if necessary, in which case, adjust also the width
	if (nControllers > DAQLAB_NVISIBLE_TASKCONTROLLERS) {
		
		// adjust size of Tasks panel to its maximum
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_HEIGHT, TasksUI.controllerPanHeight * DAQLAB_NVISIBLE_TASKCONTROLLERS + 
						  DAQLAB_TASKCONTROLLERS_SPACING * (DAQLAB_NVISIBLE_TASKCONTROLLERS - 1) + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);
		
		// increase width and add scrollbars
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + VAL_LARGE_SCROLL_BARS);
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_SCROLL_BARS, VAL_VERT_SCROLL_BAR);
		
	} else {
		
		// adjust size of Tasks panel to match the size of one Task Controller panel
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_HEIGHT, TasksUI.controllerPanHeight * nControllers + DAQLAB_TASKCONTROLLERS_SPACING * (nControllers - 1)  
							+ 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN + menubarHeight);
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_WIDTH, TasksUI.controllerPanWidth + 2 * DAQLAB_TASKCONTROLLERS_PAN_MARGIN); 
		
		// remove scroll bars and reset scroll bar offset
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_VSCROLL_OFFSET, 0);
		SetPanelAttribute(TasksUI.mainPanHndl, ATTR_SCROLL_BARS, VAL_NO_SCROLL_BARS);
		
	}
	
}

/// HIFN Updates a table control with connections between a given list of VChans.
static void UpdateVChanSwitchboard (int panHandle, int tableControlID)
{
	int	    	      		nColumns				= 0;
	int						nRows					= 0;
	int						nCurrentColumns			= 0;
	int						nCurrentRows			= 0;
	int             		nItems;
	int             		colWidth;
	SourceVChan_type* 		srcVChan				= NULL;
	VChan_type*				VChan1					= NULL;
	SinkVChan_type*			sinkVChan				= NULL;
	Point           		cell;
	Rect            		cellRange;
	BOOL					extraColumn;
	size_t					nVChans					= ListNumItems(VChannels);
	int						nSinks;
	char*					VChanName				= NULL;
	int 					rowLabelWidth;
	int						maxRowLabelWidth		= 0;
	
	if (!panHandle) return; // do nothing if panel is not loaded or list is not initialized
	if (!VChannels) return;
	
	// clear table cell contents
	GetNumTableRows(panHandle, tableControlID, &nCurrentRows);
	GetNumTableColumns(panHandle, tableControlID, &nCurrentColumns);
	cellRange.left 		= 1;
	cellRange.top		= 1;
	cellRange.height	= nCurrentRows;
	cellRange.width		= nCurrentColumns;
	DeleteTableCellRangeRingItems(panHandle, tableControlID, cellRange, 0, -1);
	
	// insert source VChans as rows
	for (size_t chanIdx = 1; chanIdx <= nVChans; chanIdx++) {
		srcVChan = *(SourceVChan_type**)ListGetPtrToItem(VChannels, chanIdx);
		
		if (GetVChanDataFlowType((VChan_type*)srcVChan) != VChan_Source) continue;	// select only Source VChans
		
		nRows++;
		if (nRows > nCurrentRows || !nCurrentRows)
			InsertTableRows(panHandle, tableControlID, -1, 1, VAL_CELL_RING);
		
		
		// adjust table display
		if (IsVChanOpen((VChan_type*)srcVChan))
			SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_LABEL_COLOR, VAL_DK_GREEN);
		else
			SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_LABEL_COLOR, VAL_DK_RED);
		
		
		SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_USE_LABEL_TEXT, 1);
		SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_LABEL_JUSTIFY, VAL_CENTER_LEFT_JUSTIFIED);
		SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_LABEL_TEXT, (VChanName = GetVChanName((VChan_type*)srcVChan)));
		GetTextDisplaySize(VChanName, VAL_DIALOG_META_FONT, NULL, &rowLabelWidth);
		if (rowLabelWidth > maxRowLabelWidth) maxRowLabelWidth = rowLabelWidth;
		OKfree(VChanName);
			
		nSinks = (int) GetNSinkVChans(srcVChan);
		// keep track of required number of columns
		if (nSinks > nColumns) 
			nColumns = nSinks;
		
		// add columns and create combo boxes for the sinks of this source VChan if there are not enough columns already
		if (nColumns > nCurrentColumns) {
			InsertTableColumns(panHandle, tableControlID, -1, nColumns - nCurrentColumns, VAL_CELL_RING);
			nCurrentColumns += nColumns - nCurrentColumns;
		}
		
		// add assigned sink VChans
		for (int idx = 1; idx <= nSinks; idx++) {
			cell.x = idx;	 // column
			cell.y = nRows;  // row
			sinkVChan = *(SinkVChan_type**)ListGetPtrToItem(GetSinkVChanList(srcVChan), idx);
			
			
			if (IsVChanOpen((VChan_type*)sinkVChan))
				SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_TEXT_COLOR, VAL_DK_GREEN);
			else
				SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_TEXT_COLOR, VAL_DK_RED);
			
			InsertTableCellRingItem(panHandle, tableControlID, cell, 0, "");    
			InsertTableCellRingItem(panHandle, tableControlID, cell, 1, (VChanName = GetVChanName((VChan_type*)sinkVChan)));
			OKfree(VChanName);
			// select the assigned sink VChan
			SetTableCellValFromIndex(panHandle, tableControlID, cell, 1); 
		}
			
		// add unassigned sink VChans of the same type
		extraColumn= FALSE;
		for (size_t vchanIdx = 1; vchanIdx <= nVChans; vchanIdx++) {
			VChan1 = *(VChan_type**)ListGetPtrToItem(VChannels, vchanIdx);
				
			// select only sinks
			if (GetVChanDataFlowType(VChan1) != VChan_Sink) continue;
				
			// select only compatible unassigned sinks
			if (GetSourceVChan((SinkVChan_type*)VChan1) || !VChansAreCompatible(srcVChan, (SinkVChan_type*)VChan1)) continue;
			
			// keep track of required number of columns
			if (nSinks+1 > nColumns) 
				nColumns = nSinks+1;
		
			if (nColumns > nCurrentColumns) {
				extraColumn = TRUE;
				InsertTableColumns(panHandle, tableControlID, -1, 1, VAL_CELL_RING);
				nCurrentColumns += nColumns - nCurrentColumns;
			}
			
			cellRange.top    = nRows;
			cellRange.left   = 1;
			cellRange.height = 1; 
			cellRange.width  = nSinks+1;
			InsertTableCellRangeRingItem(panHandle, tableControlID, cellRange, -1, (VChanName = GetVChanName(VChan1)));
			// add empty selection to extra column and select it
			InsertTableCellRingItem(panHandle, tableControlID, MakePoint(nSinks+1, nRows), 0, "");
			SetTableCellValFromIndex(panHandle, tableControlID, MakePoint(nSinks+1, nRows), 0); 
			OKfree(VChanName);
		} 
		
	}
	
	// adjust width of row label column
	SetCtrlAttribute(panHandle, tableControlID, ATTR_ROW_LABELS_WIDTH, maxRowLabelWidth + 4);
	
	// adjust table column width to widest cell
	for (int i = 1; i <= nColumns; i++) {
		SetColumnWidthToWidestCellContents(panHandle, tableControlID, i);
		GetTableColumnAttribute(panHandle, tableControlID, i, ATTR_COLUMN_ACTUAL_WIDTH, &colWidth);
		// set a minimum column width
		if (colWidth < 50)
			SetTableColumnAttribute(panHandle, tableControlID, i, ATTR_COLUMN_WIDTH, 50);
	}
	
	// dim ring controls with only empty selection
	for (int i = 1; i <= nRows; i++)
		for (int j = 1; j <= nColumns; j++) {
			cell.x = j; 
			cell.y = i;
			GetNumTableCellRingItems(panHandle, tableControlID, cell, &nItems);
			if (nItems <= 1) 
				SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_CELL_DIMMED, 1);
			else
				SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_CELL_DIMMED, 0);
		}
	
	// delete extra rows and columns
	if (nCurrentRows > nRows)
		DeleteTableRows(panHandle, tableControlID, nRows+1, nCurrentRows - nRows);
	
	if (nCurrentColumns > nColumns)
		DeleteTableColumns(panHandle, tableControlID, nColumns+1, nCurrentColumns - nColumns);
	
}

/// HIFN Updates a table control with connections between a given list master and slave HW Triggers.
static void UpdateHWTriggersSwitchboard (int panHandle, int tableControlID)
{
	int		          		nColumns				= 0;
	int						nRows					= 0;
	int						nCurrentColumns			= 0;
	int						nCurrentRows			= 0;
	int             		nItems;
	int             		colWidth;
	HWTrigMaster_type** 	masterPtr				= NULL;
	HWTrigSlave_type**		slavePtr				= NULL;
	HWTrigSlave_type*		slave					= NULL;
	Point           		cell;
	Rect            		cellRange;
	BOOL					extraColumn;
	size_t					nMasters				= ListNumItems(HWTrigMasters);
	size_t					nSlaves					= ListNumItems(HWTrigSlaves);
	int						nConnectedSlaves;
	char*					itemName				= NULL;
	int 					rowLabelWidth;
	int						maxRowLabelWidth		= 0;
	
	if (!panHandle) return; // do nothing if panel is not loaded or list is not initialized
	
	// clear table cell contents
	GetNumTableRows(panHandle, tableControlID, &nCurrentRows);
	GetNumTableColumns(panHandle, tableControlID, &nCurrentColumns);
	cellRange.left 		= 1;
	cellRange.top		= 1;
	cellRange.height	= nCurrentRows;
	cellRange.width		= nCurrentColumns;
	DeleteTableCellRangeRingItems(panHandle, tableControlID, cellRange, 0, -1); 
	
	// insert HW Trig Master as rows
	for (size_t masterIdx = 1; masterIdx <= nMasters; masterIdx++) {
		masterPtr = ListGetPtrToItem(HWTrigMasters, masterIdx);
		
		nRows++;
		if (nRows > nCurrentRows || !nCurrentRows)
			InsertTableRows(panHandle, tableControlID, -1, 1, VAL_CELL_RING);
			
		// adjust table display
		SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_USE_LABEL_TEXT, 1);
		SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_LABEL_JUSTIFY, VAL_CENTER_CENTER_JUSTIFIED);
		SetTableRowAttribute(panHandle, tableControlID, nRows, ATTR_LABEL_TEXT, (itemName = GetHWTrigMasterName(*masterPtr)));
		GetTextDisplaySize(itemName, VAL_DIALOG_META_FONT, NULL, &rowLabelWidth);
		if (rowLabelWidth > maxRowLabelWidth) maxRowLabelWidth = rowLabelWidth;
		OKfree(itemName);
			
		nConnectedSlaves = (int) GetNumHWTrigSlaves(*masterPtr);
		// keep track of required number of columns
		if (nConnectedSlaves > nColumns) 
			nColumns = nConnectedSlaves;
		
		// add columns and create combo boxes for the slaves connected to this master if there are not enough columns already
		if (nColumns > nCurrentColumns) {
			InsertTableColumns(panHandle, tableControlID, -1, nColumns - nCurrentColumns, VAL_CELL_RING);
			nCurrentColumns += nColumns - nCurrentColumns;
		}
		
		
		// add assigned slaves
		for (int idx = 1; idx <= nConnectedSlaves; idx++) {
			cell.x = idx;	 // column
			cell.y = nRows;  // row
			slave = GetHWTrigSlaveFromMaster(*masterPtr, idx);
			InsertTableCellRingItem(panHandle, tableControlID, cell, 0, "");    
			InsertTableCellRingItem(panHandle, tableControlID, cell, 1, (itemName = GetHWTrigSlaveName(slave)));
			OKfree(itemName);
			// select the assigned HW Trig Slave
			SetTableCellValFromIndex(panHandle, tableControlID, cell, 1); 
		}
			
		// add unassigned HW Trig Slaves
		extraColumn= FALSE;
		for (size_t idx = 1; idx <= nSlaves; idx++) {
			slavePtr = ListGetPtrToItem(HWTrigSlaves, idx);
				
			if (GetHWTrigSlaveMaster(*slavePtr)) continue; // select unassigned slaves
			
			// keep track of required number of columns
			if (nConnectedSlaves+1 > nColumns) 
				nColumns = nConnectedSlaves+1;
		
			if (nColumns > nCurrentColumns) {
				extraColumn = TRUE;
				InsertTableColumns(panHandle, tableControlID, -1, 1, VAL_CELL_RING);
				nCurrentColumns += nColumns - nCurrentColumns;
			}
			
			cellRange.top    = nRows;
			cellRange.left   = 1;
			cellRange.height = 1; 
			cellRange.width  = nConnectedSlaves+1;
			InsertTableCellRangeRingItem(panHandle, tableControlID, cellRange, -1, (itemName = GetHWTrigSlaveName(*slavePtr)));
			// add empty selection to extra column and select it
			InsertTableCellRingItem(panHandle, tableControlID, MakePoint(nConnectedSlaves+1, nRows), 0, "");
			SetTableCellValFromIndex(panHandle, tableControlID, MakePoint(nConnectedSlaves+1, nRows), 0); 
			OKfree(itemName);
		} 
		
	}
	
	// adjust width of row label column
	SetCtrlAttribute(panHandle, tableControlID, ATTR_ROW_LABELS_WIDTH, maxRowLabelWidth + 4);
	
	// adjust table column width to widest cell
	for (int i = 1; i <= nColumns; i++) {
		SetColumnWidthToWidestCellContents(panHandle, tableControlID, i);
		GetTableColumnAttribute(panHandle, tableControlID, i, ATTR_COLUMN_ACTUAL_WIDTH, &colWidth);
		// set a minimum column width
		if (colWidth < 50)
			SetTableColumnAttribute(panHandle, tableControlID, i, ATTR_COLUMN_WIDTH, 50);
	}
	
	// dim ring controls with only empty selection
	for (int i = 1; i <= nRows; i++)
		for (int j = 1; j <= nColumns; j++) {
			cell.x = j; 
			cell.y = i;
			GetNumTableCellRingItems(panHandle, tableControlID, cell, &nItems);
			if (nItems <= 1) 
				SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_CELL_DIMMED, 1);
			else
				SetTableCellAttribute(panHandle, tableControlID, cell, ATTR_CELL_DIMMED, 0);
		}
	
	// delete extra rows and columns
	if (nCurrentRows > nRows)
		DeleteTableRows(panHandle, tableControlID, nRows+1, nCurrentRows - nRows);
	
	if (nCurrentColumns > nColumns)
		DeleteTableColumns(panHandle, tableControlID, nColumns+1, nCurrentColumns - nColumns);
}

// callback to operate the VChan switchboard
int CVICALLBACK VChanSwitchboard_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event != EVENT_COMMIT) return 0; // continue only if event is commit
	
	// eventData1 - 1-based row index
	// eventData2 - 1-based column index
	SourceVChan_type*		sourceVChan;
	SinkVChan_type*			sinkVChan;
	char*         			sourceVChanName;
	char*         			sinkVChanName;
	int	        			nChars;
	Point        			cell;
	
	// find source VChan from the table row in the list
	GetTableRowAttribute(panel, control, eventData1, ATTR_LABEL_TEXT_LENGTH, &nChars);
	sourceVChanName = malloc((nChars+1) * sizeof(char));
	if (!sourceVChanName) return 0;
	
	GetTableRowAttribute(panel, control, eventData1, ATTR_LABEL_TEXT, sourceVChanName);
	sourceVChan = (SourceVChan_type*)VChanNameExists (VChannels, sourceVChanName, 0);
	// read in new sink VChan name from the ring control
	cell.x = eventData2;
	cell.y = eventData1;
	GetTableCellValLength(panel, control, cell, &nChars);
	sinkVChanName = malloc((nChars+1) * sizeof(char));
	if (!sinkVChanName) return 0;
	
	GetTableCellVal(panel, control, cell, sinkVChanName);
	
	int nSinks = (int) GetNSinkVChans(sourceVChan);
			
	// disconnect selected Sink VChan from Source VChan
	if (nSinks && (eventData2 <= nSinks)) {
		sinkVChan = GetSinkVChan(sourceVChan, eventData2);
		VChan_Disconnect((VChan_type*)sinkVChan);
	}
		
	// reconnect new Sink VChan to the Source Vchan
	if (strcmp(sinkVChanName, "")) {
		sinkVChan =	(SinkVChan_type*)VChanNameExists(VChannels, sinkVChanName, 0);
		VChan_Connect(sourceVChan, sinkVChan);
	}
			
	// update table
	UpdateVChanSwitchboard(panel, control);
			
	OKfree(sourceVChanName);
	OKfree(sinkVChanName);
	
	return 0;
}

int CVICALLBACK HWTriggersSwitchboard_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	if (event != EVENT_COMMIT) return 0; // continue only if event is commit
	
	// eventData1 - 1-based row index
	// eventData2 - 1-based column index
	HWTrigMaster_type*		master;
	HWTrigSlave_type*		slave;
	char*         			slaveName;
	char*         			masterName;
	int	        			nChars;
	Point        			cell;
	
	// find Master HW Trigger from the table row in the list
	GetTableRowAttribute(panel, control, eventData1, ATTR_LABEL_TEXT_LENGTH, &nChars);
	masterName = malloc((nChars+1) * sizeof(char));
	if (!masterName) return 0;
	
	GetTableRowAttribute(panel, control, eventData1, ATTR_LABEL_TEXT, masterName);
	master = HWTrigMasterNameExists(HWTrigMasters, masterName, 0);
	
	// read in new slave name from the ring control
	cell.x = eventData2;
	cell.y = eventData1;
	GetTableCellValLength(panel, control, cell, &nChars);
	slaveName = malloc((nChars+1) * sizeof(char));
	if (!slaveName) return 0;
	GetTableCellVal(panel, control, cell, slaveName);
	
	int nSlaves = (int) GetNumHWTrigSlaves(master);
	
	// disconnect selected slave from master
	if (nSlaves && (eventData2 <= nSlaves)) {
		slave = GetHWTrigSlaveFromMaster(master, eventData2);
		RemoveHWTrigSlaveFromMaster(slave);
	}
	
	// reconnect new slave to master
	if (strcmp(slaveName, "")) {
		slave = HWTrigSlaveNameExists(HWTrigSlaves, slaveName, 0);
		AddHWTrigSlaveToMaster(master, slave, 0);
	}
			
	// update table
	UpdateHWTriggersSwitchboard(panel, control);
	
	OKfree(masterName);
	OKfree(slaveName);
	return 0;
}

static int CVICALLBACK DataStorage_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case DSMainPan_Channels:
					
					break;
					
				case DSMainPan_AddChannel:
					
					break;
					
				case DSMainPan_DeleteChannel:
					
					break;
					
				case DSMainPan_ChangeFilePath:
					
					break;
				
				case DSMainPan_OverwriteFile:
					
					break;
					
				case DSMainPan_Close:
					
					OKfreePanHndl(TasksUI.dataStoragePanHndl);
					break;
					
			}
			
			break;
			
	}
	
	return 0;
}

static void CVICALLBACK DAQLab_TaskMenu_CB (int menuBarHndl, int menuItemID, void *callbackData, int panelHndl)
{
INIT_ERR

	// menu item Add
	if (menuItemID == TasksUI.menuManageItem_Add) errChk( DAQLab_TaskMenu_AddTaskController(&errorInfo.errMsg) );
		
	// menu item Delete
	if (menuItemID == TasksUI.menuManageItem_Delete ) DAQLab_TaskMenu_DeleteTaskController();
	
	// menu item Storage
	if (menuItemID == TasksUI.menuDataItem_Storage ) DAQLab_TaskMenu_DisplayDataStorage();
	
Error:
	
PRINT_ERR	
}

void CVICALLBACK DAQLab_DisplayTaskManagerMenu_CB (int menuBarHndl, int menuItemID, void *callbackData, int panelHndl)
{
	DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
}

void CVICALLBACK DAQLab_SaveCfg_CB (int menuBarHndl, int menuItemID, void *callbackData, int panelHndl)
{
	int		fileSelection						= 0;
	char 	fullPathName[MAX_PATHNAME_LEN]		= "";
	
	fileSelection = FileSelectPopupEx ("","*.xml", "*.xml", "Save DAQLab configuration", VAL_SAVE_BUTTON, 0, 1, fullPathName);
	
	switch (fileSelection) {
			
		case VAL_NO_FILE_SELECTED:
			
			return;
			
		case VAL_EXISTING_FILE_SELECTED:
		case VAL_NEW_FILE_SELECTED:
			
			//DAQLab_Save
			
			return;
			
		default:
			
			return;	
	}
	
}

void CVICALLBACK DAQLab_ModulesMenu_CB (int menuBarHndl, int menuItem, void *callbackData, int panelHndl)
{
	DAQLabModule_type**		modulePtrPtr;
	char*					fullModuleName;
	int						nModules;			
	ListType				moduleClassNames;	// of char* type
	size_t					idx;
	
	// check if module class names are unique
	moduleClassNames = ListCreate(sizeof(char*));
	nModules = NumElem(DAQLabModules_InitFunctions);
	for (int i = 0; i < nModules; i++)
		if (DAQLabModules_InitFunctions[i].className != NULL)
			ListInsertItem(moduleClassNames, &DAQLabModules_InitFunctions[i].className, END_OF_LIST);
	
	if (!DLUniqueStrings(moduleClassNames, &idx)) {
		DAQLab_Msg(DAQLAB_MSG_ERR_NOT_UNIQUE_CLASS_NAME, DAQLabModules_InitFunctions[idx].className, NULL, NULL, NULL); 
		return;
	}
	ListDispose(moduleClassNames);
	
	if (!DAQLabModulesPanHndl)
		DAQLabModulesPanHndl = LoadPanel(workspacePanHndl, DAQLAB_UI_DAQLAB, ModulesPan);
	else {
		DisplayPanel(DAQLabModulesPanHndl); // bring to focus
		return;
	}
	
	// list available modules
	for (int i = 0; i < nModules; i++)
		if (DAQLabModules_InitFunctions[i].className != NULL) 
		InsertListItem(DAQLabModulesPanHndl, ModulesPan_Available, -1, DAQLabModules_InitFunctions[i].className, i);  
	
	// list installed modules
	nModules = ListNumItems(DAQLabModules);
	for (int i = 1; i <= nModules; i++) {
		modulePtrPtr = ListGetPtrToItem(DAQLabModules, i);
		fullModuleName = StrDup((*modulePtrPtr)->className);
		AppendString(&fullModuleName, ": ", -1);
		AppendString(&fullModuleName, (*modulePtrPtr)->instanceName, -1);
		InsertListItem(DAQLabModulesPanHndl, ModulesPan_Loaded, -1, fullModuleName, (*modulePtrPtr)->instanceName);
		OKfree(fullModuleName);
	} 
	
	// display panel
	DisplayPanel(DAQLabModulesPanHndl);
	
	return;
}

static BOOL DAQLab_CheckValidModuleName (char name[], void* dataPtr)
{
	return DLValidModuleInstanceName(name);	
}

/// HIFN Checks if module instance name is unique and valid.
/// HIRET True if name is unique and valid, False otherwise.
BOOL DLValidModuleInstanceName (char name[])
{
	DAQLabModule_type**		modulePtrPtr;
	size_t					nDLModules 		= ListNumItems(DAQLabModules);
	
	for (size_t i = 1; i <= nDLModules; i++) {
		modulePtrPtr = ListGetPtrToItem(DAQLabModules, i);
		if (!strcmp((*modulePtrPtr)->instanceName, name)) return FALSE; // module with same instance name exists already
	}
	
	return TRUE;
}

int CVICALLBACK DAQLab_ManageDAQLabModules_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	switch (event)
	{
		case EVENT_LEFT_DOUBLE_CLICK:
			
			char*					newInstanceName		= NULL;
			char*					fullModuleName		= NULL;
			char*					moduleName			= NULL;
			int						moduleidx			= 0;		// 0-based index of selected module
			DAQLabModule_type*  	newModulePtr		= NULL;
			DAQLabModule_type**		modulePtrPtr		= NULL;
			int						nchars				= 0;
			size_t					nInstalledModules	= 0;
			size_t					nAvailableModules 	= NumElem(DAQLabModules_InitFunctions);
			
			switch (control) {
					
				case ModulesPan_Available:
					
					GetCtrlVal(panel, ModulesPan_Available, &moduleidx);
					if (moduleidx < 0) return 0; // no modules listed   
					
					// do nothing if multiple instances are not allowed and there is already such a module installed
					if (!DAQLabModules_InitFunctions[moduleidx].multipleInstancesFlag && DAQLabModules_InitFunctions[moduleidx].nInstances)
						return 0;
			
					if (DAQLabModules_InitFunctions[moduleidx].multipleInstancesFlag) {
						newInstanceName = DLGetUINameInput ("New Module Instance", DAQLAB_MAX_MODULEINSTANCE_NAME_NCHARS, DAQLab_CheckValidModuleName, NULL);
						if (!newInstanceName) return 0; // operation cancelled, do nothing
					} else 
						// use class name as instance name since there is only one instance
						// check if class name is unique among existing module instance names
						if (!DLValidModuleInstanceName(DAQLabModules_InitFunctions[moduleidx].className)) {
							// ask for unique instance name
							newInstanceName = DLGetUINameInput ("New Module Instance", DAQLAB_MAX_MODULEINSTANCE_NAME_NCHARS, DAQLab_CheckValidModuleName, NULL);
							if (!newInstanceName) return 0; // operation cancelled, do nothing
						} else
							newInstanceName = StrDup(DAQLabModules_InitFunctions[moduleidx].className);
					
					// call module init function
					newModulePtr = (*DAQLabModules_InitFunctions[moduleidx].ModuleInitFptr)	(NULL, DAQLabModules_InitFunctions[moduleidx].className, newInstanceName, workspacePanHndl);
					// call module load function
					
					if ( (*newModulePtr->Load) 	(newModulePtr, workspacePanHndl, &errorInfo.errMsg) < 0) {
						DAQLab_Msg(DAQLAB_MSG_ERR_LOADING_MODULE, newInstanceName, errorInfo.errMsg, NULL, NULL);
						OKfree(errorInfo.errMsg);
						OKfree(newInstanceName);
						// dispose of module if not loaded properly
						(*newModulePtr->Discard) 	(&newModulePtr);
						return 0;
					}
					OKfree(newInstanceName);
					
					// insert module to modules list
					ListInsertItem(DAQLabModules, &newModulePtr, END_OF_LIST);
					// increase module instance counter
					DAQLabModules_InitFunctions[moduleidx].nInstances++;
					
					// make sure that the Task Tree is updated
					if (TaskTreeManagerPanHndl)
						DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
					
					// display module panels if method is defined
					if (newModulePtr->DisplayPanels)
						(*newModulePtr->DisplayPanels) (newModulePtr, TRUE);
					
					// add full module name to list box
					fullModuleName = StrDup(newModulePtr->className);
					AppendString(&fullModuleName, ": ", -1);
					AppendString(&fullModuleName, newModulePtr->instanceName, -1);
					InsertListItem(panel, ModulesPan_Loaded, -1, fullModuleName, newModulePtr->instanceName);
					OKfree(fullModuleName);
					
					break;
					
				case ModulesPan_Loaded:
					
					GetCtrlIndex(panel, ModulesPan_Loaded, &moduleidx);
					if (moduleidx < 0) return 0; // no modules listed
					
					GetValueLengthFromIndex(panel, ModulesPan_Loaded, moduleidx, &nchars);
					moduleName = malloc ((nchars+1)*sizeof(char));
					if (!moduleName) {
						DLMsg("Error: Out of memory.", 1);
						return 0;
					}
					// get module instance name
					GetValueFromIndex(panel, ModulesPan_Loaded, moduleidx, moduleName); 
					// remove module if found
					nInstalledModules = ListNumItems(DAQLabModules);
					for (size_t i = 1; i <= nInstalledModules; i++) {
						modulePtrPtr = ListGetPtrToItem (DAQLabModules, i);
						if (!strcmp((*modulePtrPtr)->instanceName, moduleName)) {
							// update number of instances
							for (size_t j = 0; j < nAvailableModules; j++)
								if (DAQLabModules_InitFunctions[j].className && (*modulePtrPtr)->className && !strcmp(DAQLabModules_InitFunctions[j].className, (*modulePtrPtr)->className)) {
									DAQLabModules_InitFunctions[j].nInstances--;
									break;
								}
							// remove from DAQLab modules list
							DAQLabModule_type* removedDAQLabModule;
							ListRemoveItem(DAQLabModules, &removedDAQLabModule, i);
							// remove Task Controllers and VChans from the framework
							DLRemoveTaskControllers(removedDAQLabModule, removedDAQLabModule->taskControllers);
							DLUnregisterModuleVChans(removedDAQLabModule);
							// call module discard function
							(*removedDAQLabModule->Discard)	(&removedDAQLabModule);
							
							break;
						}
					}
					
					OKfree(moduleName);
					// remove from installed modules list
					DeleteListItem(panel, ModulesPan_Loaded, moduleidx, 1);
					
					break; 
			}
					
			break;
			
	}
	return 0;
}

int DLSaveTaskControllerSettingsToXML (TaskControl_type* taskController, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ taskControllerXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	ActiveXMLObj_IXMLDOMElement_ 		newXMLElement   = 0;
	int									tcNIter			= GetTaskControlIterations(taskController); 
	double								tcWait			= GetTaskControlIterationsWait(taskController);  
	uInt32								tcExecutionMode	= (uInt32) GetTaskControlExecutionMode(taskController);		
	BOOL								tcIterationMode	= (BOOL) GetTaskControlMode(taskController); 		// finite = 1, continuous = 0  
	DAQLabXMLNode 						attr[] 			= { {"Name",				BasicData_CString,		GetTaskControlName(taskController)},
															{"NIterations",			BasicData_UInt,			&tcNIter},
															{"Wait",				BasicData_Double,		&tcWait},
															{"ExecutionMode",		BasicData_UInt,			&tcExecutionMode},
															{"IterationMode",		BasicData_Bool,			&tcIterationMode} };
	
	
	// add task controller settings as attributes
	errChk( DLAddToXMLElem (xmlDOM, taskControllerXMLElement, attr, DL_ATTRIBUTE, NumElem(attr), xmlErrorInfo) );
	
	// cleanup
	OKfree(attr[0].pData);
	
	return 0;
	
Error:
	
	OKfreeCAHndl(newXMLElement);
	
	return errorInfo.error;
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

BOOL DLValidTaskControllerName (char name[])
{
	TaskControl_type*	taskController		= NULL;
	char*				nameTC				= NULL; 
	size_t				n					= ListNumItems(DAQLabTCs);
	
	// check if there is already another Task Controller with the same name
	for (size_t i = 1; i <= n; i++) {
		OKfree(nameTC);
		taskController = *(TaskControl_type**)ListGetPtrToItem(DAQLabTCs, i);
		nameTC = GetTaskControlName(taskController);
		if (!strcmp(nameTC, name)) {
			OKfree(nameTC);
			return FALSE;
		}
	}
	OKfree(nameTC);
	return TRUE;
}

char* DLGetUniqueTaskControllerName	(char baseTCName[])
{
	return GetUniqueTaskControllerName(DAQLabTCs, baseTCName);
}

static BOOL	DAQLab_ValidControllerName (char name[], void* listPtr)
{
	return DLValidTaskControllerName (name);
}

static int DAQLab_TaskMenu_AddTaskController (char** errorMsg) 
{
INIT_ERR

	UITaskCtrl_type*	UITaskCtrlsPtr			= NULL;
	TaskControl_type* 	newTaskControllerPtr	= NULL;
	char*				newControllerName		= NULL;
	
	newControllerName = DLGetUINameInput ("New Task Controller", DAQLAB_MAX_UITASKCONTROLLER_NAME_NCHARS, DAQLab_ValidControllerName, NULL);
	if (!newControllerName) return 0; // operation cancelled, do nothing
	
	// create new task controller
	nullChk(newTaskControllerPtr = init_TaskControl_type (newControllerName, NULL, DLThreadPoolHndl, ConfigureUITC, UnconfigureUITC, IterateUITC, StartUITC, 
												  ResetUITC, DoneUITC, StoppedUITC, NULL, TaskTreeStateChangeUITC, UITCActive, NULL, ErrorUITC) ); // module data added to the task controller below
	OKfree(newControllerName);
	
	// add new Task Controller to the UI environment and to the framework list
	UITaskCtrlsPtr = DAQLab_AddTaskControllerToUI(newTaskControllerPtr);
	
	// attach callback data to the Task Controller
	SetTaskControlModuleData(newTaskControllerPtr, UITaskCtrlsPtr);
	// mark the Task Controller as an User Interface Task Controller type
	SetTaskControlUITCFlag(newTaskControllerPtr, TRUE);
	// send a configure event to the Task Controller
	errChk( TaskControlEvent(newTaskControllerPtr, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}

static void DAQLab_TaskMenu_DisplayDataStorage (void)
{
INIT_ERR	
	
	if (TasksUI.dataStoragePanHndl) {
		DisplayPanel(TasksUI.dataStoragePanHndl);
		return; // stop here
	}
	
	// load and display panel
	errChk( TasksUI.dataStoragePanHndl = LoadPanel (workspacePanHndl, DAQLAB_UI_DAQLAB, DSMainPan) );
	
	// add callback for controls in the panel
	SetCtrlsInPanCBInfo(NULL, DataStorage_CB, TasksUI.dataStoragePanHndl);
	
	DisplayPanel(TasksUI.dataStoragePanHndl);
	
Error:
	
	return;
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
INIT_ERR

	switch (event)
	{
		case EVENT_COMMIT:
			
			int		idx;	 // 0-based list index
			int		nitems;
			
			// get selected Task Controller
			GetCtrlIndex(panel, TCDelPan_TaskControllers, &idx); 
			if (idx <0) return 0; // no items in the list
			
			errChk( RemoveTaskControllerFromUI (idx+1, &errorInfo.errMsg) );
			DeleteListItem(panel, TCDelPan_TaskControllers, idx, 1);
			
			// if there are no more items in the list, then discard the panel
			GetNumListItems(panel, TCDelPan_TaskControllers, &nitems);
			if (!nitems) DiscardPanel(panel);
			
			break;
	}
	
	return 0;
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK DAQLab_TaskControllers_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskControl_type* 	taskControl = callbackData;
	BOOL             	starttask	= FALSE;					  	// 1 - task start, 0 - task stop 
	double				waitDVal	= 0.0;
	BOOL				stack		= FALSE;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
				
				case TCPan1_Reset:
					
					errChk( TaskControlEvent(taskControl, TC_Event_Reset, NULL, NULL, &errorInfo.errMsg) );
					break;
					
				case TCPan1_Mode:
				case TCPan1_Repeat:  	
					
					errChk( TaskControlEvent(taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
					break;
					
				case TCPan1_StartStop:
					
					
					GetCtrlVal(panel,control,&starttask);
					if (starttask) {
						// dim controls
						SetCtrlAttribute(panel, TCPan1_Repeat, ATTR_DIMMED, TRUE);
						SetCtrlAttribute(panel, TCPan1_Wait, ATTR_DIMMED, TRUE);
						SetCtrlAttribute(panel, TCPan1_Reset, ATTR_DIMMED, TRUE);
						SetCtrlAttribute(panel, TCPan1_Mode, ATTR_DIMMED, TRUE);
						SetCtrlAttribute(panel, TCPan1_Stack, ATTR_DIMMED, TRUE);    
						// undim abort button if UITC doesn't have a parent task Controller
						if (!GetTaskControlParent(taskControl))
							SetCtrlAttribute(panel, TCPan1_Abort, ATTR_DIMMED, FALSE);
						
						// send start task event
						errChk( TaskControlEvent(taskControl, TC_Event_Start, NULL, NULL, &errorInfo.errMsg) );
					}
					else {
						// dim button so the user cannot start the task again until it stops
						SetCtrlAttribute(panel, TCPan1_StartStop, ATTR_DIMMED, TRUE);
						errChk( TaskControlEvent(taskControl, TC_Event_Stop, NULL, NULL, &errorInfo.errMsg) );
					}
						
					break;
					
				case TCPan1_Abort:
					
					AbortTaskControlExecution (taskControl);
					
					break;
					
				case TCPan1_Wait:
					
					GetCtrlVal(panel, TCPan1_Wait, &waitDVal);
					SetTaskControlIterationsWait(taskControl, waitDVal);
					
					break;
				case TCPan1_Stack:
					
					GetCtrlVal(panel, TCPan1_Stack, &stack);  
					SetIteratorStackData(GetTaskControlIterator(taskControl), stack);
					break;
			}
			break;
			
	}
	
Error:
	
PRINT_ERR

	return 0;
}


/// HIFN Provides GUI for assembling Task Control trees from Module provided Task Controllers and User Interface Task Controllers (UITCs).
/// HIPAR parentPanHndl/ Panel handle in which to display the Task Tree Manager. If 0, the UI is displayed as a main panel.
/// HIPAR UITCList/ Provide a list of User Interface Task Controllers (UITCs) of UITaskCtrl_type* type that allow the user to control the execution of each Task Tree.
/// HIPAR modules/ Provide a list of installed DAQLab modules of DAQLabModule_type* type. 
static void DisplayTaskTreeManager (int parentPanHndl, ListType UITCs, ListType modules)
{
	size_t					nUITCs			= ListNumItems(UITCs);
	size_t					nModules		= ListNumItems(modules);
	size_t					nTCs;
	TaskControl_type**  	tcPtrPtr;
	TaskControl_type*		tcPtr;
	UITaskCtrl_type**		UITCPtrPtr;
	DAQLabModule_type**		modulePtrPtr;
	DAQLabModule_type*		modulePtr;
	int						parentIdx;
	int						childIdx;
	char*					name;
	TaskTreeNode_type		node;
	
	
	// load UI
	if (!TaskTreeManagerPanHndl) {
		TaskTreeManagerPanHndl = LoadPanel(parentPanHndl, DAQLAB_UI_DAQLAB, TaskPan);
		
		// get VChan Switchboard panel handle
		GetPanelHandleFromTabPage(TaskTreeManagerPanHndl, TaskPan_Switchboards, 0, &VChanSwitchboardPanHndl);
		// get HW Triggers Switchboard panel handle
		GetPanelHandleFromTabPage(TaskTreeManagerPanHndl, TaskPan_Switchboards, 1, &HWTrigSwitchboardPanHndl);
		
		// populate execution mode ring control
		InsertListItem(TaskTreeManagerPanHndl, TaskPan_ExecMode, TC_Execute_BeforeChildTCs, "Before Child Task Controllers Start", TC_Execute_BeforeChildTCs);
		InsertListItem(TaskTreeManagerPanHndl, TaskPan_ExecMode, TC_Execute_AfterChildTCsComplete, "After Child Task Controllers Complete", TC_Execute_AfterChildTCsComplete);
		InsertListItem(TaskTreeManagerPanHndl, TaskPan_ExecMode, TC_Execute_InParallelWithChildTCs, "In parallel with Child Task Controllers", TC_Execute_InParallelWithChildTCs);
		// set execution mode to TC_Execute_BeforeChildTCs 
		SetCtrlIndex(TaskTreeManagerPanHndl, TaskPan_ExecMode, 0);
		// dim execution mode control cause the item with index 0 is the modules node
		SetCtrlAttribute(TaskTreeManagerPanHndl, TaskPan_ExecMode, ATTR_DIMMED, 1);
		
	}
	
	// clear previous Task Tree nodes list
	if (!TaskTreeNodes) TaskTreeNodes = ListCreate(sizeof(TaskTreeNode_type));
	ListClear(TaskTreeNodes);
	
	// clear Task Tree
	DeleteListItem(TaskTreeManagerPanHndl, TaskPan_TaskTree, 0, -1); 
	
	// populate available module task controllers
	// insert Task Tree node
	node.taskControl 			= NULL;
	node.acceptsUITCChild   	= FALSE;
	node.acceptsTCChild			= FALSE;
	node.canBeDeleted			= FALSE;
	ListInsertItem(TaskTreeNodes, &node, END_OF_LIST);
	parentIdx = InsertTreeItem(TaskTreeManagerPanHndl, TaskPan_TaskTree, VAL_SIBLING, 0, VAL_LAST, TaskTreeManager_Modules_Label, NULL, NULL, ListNumItems(TaskTreeNodes));
	
	for (size_t i = 1; i <= nModules; i++) {
		modulePtrPtr 	= ListGetPtrToItem(modules, i);
		modulePtr		= *modulePtrPtr;
		if (!modulePtr) break;  //skip if NULL
		
		// insert Task Tree node
		node.taskControl 			= NULL;
		node.acceptsUITCChild   	= FALSE;
		node.acceptsTCChild			= FALSE;
		node.canBeDeleted			= FALSE;
		
		ListInsertItem(TaskTreeNodes, &node, END_OF_LIST);
		childIdx = InsertTreeItem(TaskTreeManagerPanHndl, TaskPan_TaskTree, VAL_CHILD, parentIdx, VAL_LAST, modulePtr->instanceName, NULL, NULL, ListNumItems(TaskTreeNodes));
		
		// add all Task Controllers from the module that are not yet assigned to any task tree, i.e. they don't have a parent task controller.
		nTCs = ListNumItems(modulePtr->taskControllers);
		for (size_t j = 1; j <= nTCs; j++) {
			tcPtrPtr = ListGetPtrToItem(modulePtr->taskControllers, j);
			tcPtr = *tcPtrPtr;
			if (GetTaskControlParent(tcPtr)) continue; // skip if it has a parent
			
			// insert Task Tree node
			node.taskControl 			= tcPtr;
			node.acceptsUITCChild   	= FALSE;
			node.acceptsTCChild			= FALSE;
			node.canBeDeleted			= FALSE;
			ListInsertItem(TaskTreeNodes, &node, END_OF_LIST);
			name = GetTaskControlName(tcPtr);
			InsertTreeItem(TaskTreeManagerPanHndl, TaskPan_TaskTree, VAL_CHILD, childIdx, VAL_LAST, name, NULL, NULL, ListNumItems(TaskTreeNodes));
			OKfree(name);
		}
	}
	
	// populate UITC tree
	// insert Task Tree node
	node.taskControl 			= NULL;
	node.acceptsUITCChild   	= TRUE;
	node.acceptsTCChild			= FALSE;
	node.canBeDeleted			= FALSE;
	ListInsertItem(TaskTreeNodes, &node, END_OF_LIST);
	parentIdx = InsertTreeItem(TaskTreeManagerPanHndl, TaskPan_TaskTree, VAL_SIBLING, 0, VAL_LAST, TaskTreeManager_UITCs_Label, NULL, NULL, ListNumItems(TaskTreeNodes));
	
	for (size_t i = 1; i <= nUITCs; i++) {
		UITCPtrPtr = ListGetPtrToItem(UITCs, i);
		tcPtr = (*UITCPtrPtr)->taskControl;
		if (GetTaskControlParent(tcPtr)) continue; // skip if it has a parent
		
		// insert Task Tree node
		node.taskControl 			= tcPtr;
		node.acceptsUITCChild   	= TRUE;
		node.acceptsTCChild			= TRUE;
		node.canBeDeleted			= TRUE;
		ListInsertItem(TaskTreeNodes, &node, END_OF_LIST);
		name = GetTaskControlName(tcPtr);
		
		childIdx = InsertTreeItem(TaskTreeManagerPanHndl, TaskPan_TaskTree, VAL_CHILD, parentIdx, VAL_LAST, name, NULL, NULL, ListNumItems(TaskTreeNodes));   
		OKfree(name);
		AddRecursiveTaskTreeItems(TaskTreeManagerPanHndl, TaskPan_TaskTree, childIdx, tcPtr);
	}
	
	// since the first item in the tree is selected which is not a task controller, the extecution mode must be dimmed
	SetCtrlAttribute(TaskTreeManagerPanHndl, TaskPan_ExecMode, ATTR_DIMMED, 1); 
	
	// update VChan Switchboard
	UpdateVChanSwitchboard(VChanSwitchboardPanHndl, VChanTab_Switchboard);
	
	// update HWTrigger Switchboard
	UpdateHWTriggersSwitchboard(HWTrigSwitchboardPanHndl, HWTrigTab_Switchboard);
	
	DisplayPanel(TaskTreeManagerPanHndl);
}

static void AddRecursiveTaskTreeItems (int panHndl, int TreeCtrlID, int parentIdx, TaskControl_type* taskControl)
{
	ListType				SubTasks		= GetTaskControlChildTCs(taskControl);
	size_t					nSubTaskTCs		= ListNumItems(SubTasks);
	int						childIdx;
	TaskControl_type**		tcPtrPtr;
	char*					name;
	TaskTreeNode_type		node;

	for (size_t i = 1; i <= nSubTaskTCs; i++) {
		tcPtrPtr = ListGetPtrToItem(SubTasks, i);
		
		// insert Task Tree node
		node.taskControl 			= *tcPtrPtr;
		node.acceptsUITCChild   	= TRUE;
		node.acceptsTCChild			= TRUE;
		node.canBeDeleted			= TRUE;
		ListInsertItem(TaskTreeNodes, &node, END_OF_LIST);
		name = GetTaskControlName(*tcPtrPtr);
		
		childIdx = InsertTreeItem(panHndl, TaskPan_TaskTree, VAL_CHILD, parentIdx, VAL_LAST, name, NULL, NULL, ListNumItems(TaskTreeNodes)); 
		OKfree(name);
		
		AddRecursiveTaskTreeItems(panHndl, TaskPan_TaskTree, childIdx, *tcPtrPtr); 
	}
	
	ListDispose(SubTasks);
}

int CVICALLBACK TaskTree_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR
	
	static TaskTreeNode_type*	dragTreeNodePtr					= NULL;
	static TaskTreeNode_type*   targetTreeNodePtr				= NULL;
	static int					relation						= 0;
	int 						nodeListIdx						= 0; 
	int							selectedNodeIdx					= 0;
	TaskTreeNode_type*			selectedTreeNodePtr				= NULL; 
	static BOOL					tcIsInUse						= FALSE;
	static BOOL					childTCIsInUseLockObtained 		= FALSE;
	static BOOL					parentTCIsInUseLockObtained 	= FALSE;
	
	
	switch (control) {
			
		case TaskPan_Close:
			
			if (event == EVENT_COMMIT) {
				OKfreePanHndl(TaskTreeManagerPanHndl);
				VChanSwitchboardPanHndl		= 0;
				HWTrigSwitchboardPanHndl	= 0;
				
			}
			break;
			
		case TaskPan_TaskTree:
			
			switch (event) {
					
				case EVENT_DRAG:
					
					// allow only Task Controllers to be dragged
					GetValueFromIndex(panel, control, eventData2, &nodeListIdx);
					dragTreeNodePtr = ListGetPtrToItem(TaskTreeNodes, nodeListIdx);
					if (!dragTreeNodePtr->taskControl) return 1; // swallow drag event
					
					// Check if Task Controller is in use
					errChk( IsTaskControllerInUse_GetLock(dragTreeNodePtr->taskControl, &tcIsInUse, NULL, &childTCIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) ); 
					
					if (tcIsInUse) {
						// release lock
						errChk( IsTaskControllerInUse_ReleaseLock(dragTreeNodePtr->taskControl, &childTCIsInUseLockObtained, &errorInfo.errMsg) ); 
						// swallow drag event
						return 1;
					}
					break;
					
				case EVENT_DROP:
					
					// do not allow a Task Controller to be added if this is not possible
					relation = LoWord(eventData1); 				// VAL_CHILD or VAL_SIBLING
					// int	position = HiWord(eventData1);  	// VAL_PREV, VAL_NEXT, or VAL_FIRST 
					
					// allow only child to parent drops
					if (relation != VAL_CHILD) return 1; // swallow drop event
					
					// eventData2 = relativeIndex. relativeIndex is the index of the relative of the new potential drop location.
					GetValueFromIndex(panel, control, eventData2, &nodeListIdx);
					targetTreeNodePtr = ListGetPtrToItem(TaskTreeNodes, nodeListIdx);
					
					if (!((targetTreeNodePtr->acceptsTCChild && !GetTaskControlUITCFlag(dragTreeNodePtr->taskControl)) ||
						(targetTreeNodePtr->acceptsUITCChild && GetTaskControlUITCFlag(dragTreeNodePtr->taskControl)))) return 1; // swallow drop event
					
					// do not allow drop to root element
					int				treeLevel;
					GetTreeItemLevel(panel, control, eventData2, &treeLevel);
					if (!treeLevel && relation == VAL_SIBLING) return 1;  
					
					
					// do not add child if target parent TC is in use
					if (targetTreeNodePtr->taskControl) {
						errChk( IsTaskControllerInUse_GetLock(targetTreeNodePtr->taskControl, &tcIsInUse, NULL, &parentTCIsInUseLockObtained, __LINE__, __FILE__, &errorInfo.errMsg) );
						if (tcIsInUse) {
							// release lock
							errChk( IsTaskControllerInUse_ReleaseLock(targetTreeNodePtr->taskControl, &parentTCIsInUseLockObtained, &errorInfo.errMsg) ); 
							// swallow drag event
							return 1;
							
						} else
							// release lock
							errChk( IsTaskControllerInUse_ReleaseLock(targetTreeNodePtr->taskControl, &parentTCIsInUseLockObtained, &errorInfo.errMsg) ); 
					}
					
					break;
					
				case EVENT_DROPPED:
					
					// eventData2 - item index
					TaskControl_type* 	parentTaskController;
					
					if (targetTreeNodePtr->taskControl)
						parentTaskController = GetTaskControlParent(targetTreeNodePtr->taskControl);
					else
						parentTaskController = NULL;
					
					// disconnect Task Controller that was dragged from its parent
					errChk( RemoveChildTCFromParentTC(dragTreeNodePtr->taskControl, &errorInfo.errMsg) );
					
					if (relation == VAL_CHILD && targetTreeNodePtr->taskControl)
						errChk( AddChildTCToParent(targetTreeNodePtr->taskControl, dragTreeNodePtr->taskControl, &errorInfo.errMsg) ); 
					else
						if (relation == VAL_SIBLING && parentTaskController)
							errChk( AddChildTCToParent(parentTaskController, dragTreeNodePtr->taskControl, &errorInfo.errMsg) ); 
					
					// allow dropped task controller to be deleted
					dragTreeNodePtr->canBeDeleted = TRUE;
					// allow dropped task controller to accept other task controllers
					dragTreeNodePtr->acceptsTCChild = TRUE;
					dragTreeNodePtr->acceptsUITCChild = TRUE;
					
					// release lock
					errChk( IsTaskControllerInUse_ReleaseLock(dragTreeNodePtr->taskControl, &childTCIsInUseLockObtained, &errorInfo.errMsg) ); 
					
					// refresh Task Tree
					DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
					
					break;
					
				case EVENT_KEYPRESS:
					
					// continue only if Del key is pressed
					if (eventData1 != VAL_FWD_DELETE_VKEY) break;
					
					GetActiveTreeItem(panel, control, &selectedNodeIdx);
					GetValueFromIndex(panel, control, selectedNodeIdx, &nodeListIdx);
					selectedTreeNodePtr = ListGetPtrToItem(TaskTreeNodes, nodeListIdx);
					
					// continue only if node can be deleted
					if (!selectedTreeNodePtr->canBeDeleted) break;
					
					// disconnect Task Controller branch and dissasemble its components including their HW Trigger and Virtual Channel connections 
					errChk( DisassembleTaskTreeBranch(selectedTreeNodePtr->taskControl, &errorInfo.errMsg) );
					
					DeleteListItem(panel, control, selectedNodeIdx, 1);
					// refresh Task Tree
					DisplayTaskTreeManager(workspacePanHndl, TasksUI.UItaskCtrls, DAQLabModules);
					
					break;
					
				case EVENT_VAL_CHANGED:
					
					if (eventData1 != ACTIVE_ITEM_CHANGE) break;
					
					GetActiveTreeItem(panel, control, &selectedNodeIdx);
					GetValueFromIndex(panel, control, selectedNodeIdx, &nodeListIdx);
					selectedTreeNodePtr = ListGetPtrToItem(TaskTreeNodes, nodeListIdx);
					
					// undim execution mode control ring if selection is a module task controller
					if (!selectedTreeNodePtr->taskControl || GetTaskControlUITCFlag(selectedTreeNodePtr->taskControl)) {
						SetCtrlAttribute(panel, TaskPan_ExecMode, ATTR_DIMMED, 1);
						break;  // stop here
					} else 
						SetCtrlAttribute(panel, TaskPan_ExecMode, ATTR_DIMMED, 0);
					
					// update execution mode ring control
					SetCtrlIndex(panel, TaskPan_ExecMode, GetTaskControlExecutionMode(selectedTreeNodePtr->taskControl));
					
					break;
					
			}
			break;
			
		case TaskPan_ExecMode:
			
			// filter only commit events
			if (event != EVENT_COMMIT) break;
			
			GetActiveTreeItem(panel, TaskPan_TaskTree, &selectedNodeIdx);
			GetValueFromIndex(panel, TaskPan_TaskTree, selectedNodeIdx, &nodeListIdx);
			selectedTreeNodePtr = ListGetPtrToItem(TaskTreeNodes, nodeListIdx);
			
			int 	execMode;
			GetCtrlVal(panel, control, &execMode);
			SetTaskControlExecutionMode(selectedTreeNodePtr->taskControl, (TCExecutionModes) execMode);
			
			break;
			
	}
	
	return 0;
	
Error:
	
	// release locks
	if (childTCIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(dragTreeNodePtr->taskControl, &childTCIsInUseLockObtained, NULL);
	
	if (parentTCIsInUseLockObtained)
		IsTaskControllerInUse_ReleaseLock(targetTreeNodePtr->taskControl, &parentTCIsInUseLockObtained, NULL); 
	
	
	
PRINT_ERR

	return 0;
}

//-----------------------------------------
// UI Task Controller Callbacks
//-----------------------------------------

static int ConfigureUITC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	unsigned int		repeat;
	BOOL				taskControllerMode;		// TRUE - finite iterations, FALSE - continuous
	
	// change Task Controller name background color to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// undim Iteration Stack button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Stack, ATTR_DIMMED, 0);
	// undim Start button if UITC doesn't have a parent Task Controller
	if (!GetTaskControlParent(controllerUIDataPtr->taskControl))
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Repeat, &repeat);
	SetTaskControlIterations(taskControl, repeat);
	
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Mode, &taskControllerMode);
	if (!taskControllerMode == TASK_FINITE) {
		// finite iterations
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
		SetTaskControlMode(taskControl, TASK_FINITE);
	} else {
		// continuous iterations
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 1);
		SetTaskControlMode(taskControl, TASK_CONTINUOUS);
	}
	
	return 0;
}

static int UnconfigureUITC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// dim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 1);
	// dim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 1); 
	// dim Start button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 1);
	
	return 0;
}

static void IterateUITC	(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
INIT_ERR

	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// iteration complete, update current iteration number
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, GetCurrentIterIndex(iterator) + 1);
	
	TaskControlIterationDone(taskControl, 0, NULL, FALSE, NULL);
}

static int StartUITC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from gray (0x00F0F0F0) to green (0x002BD22F)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x002BD22F);
	
	return 0;
}

static int DoneUITC  (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from green (0x002BD22F) to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// switch Stop button back to Start button
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_StartStop, 0);
	// undim Start/Stop and Configure buttons if UITC doesn't have a parent task Controller
	if (!GetTaskControlParent(controllerUIDataPtr->taskControl)) {
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	}
	// dim Abort button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 1);
	
	// undim Repeat button if task is finite
	if (GetTaskControlMode(taskControl) == TASK_FINITE)
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// update iterations display
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, GetCurrentIterIndex(iterator));
	
	return 0;
}

static int ResetUITC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, 0);
	
	// change Task Controller name background color to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	// undim Repeat button if finite
	if (GetTaskControlMode(taskControl) == TASK_FINITE)
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// undim Iteration Stack button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Stack, ATTR_DIMMED, 0);
	// undim Start button if UITC doesn't have a parent Task Controller
	if (!GetTaskControlParent(controllerUIDataPtr->taskControl))
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
	
	return 0; 
}

static int StoppedUITC	(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// change Task Controller name background color from green (0x002BD22F) to gray (0x00F0F0F0)
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Name, ATTR_TEXT_BGCOLOR, 0x00F0F0F0);
	// switch Stop button back to Start button
	SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_StartStop, 0);
	// undim Start/Stop and Configure buttons if UITC doesn't have a parent task Controller
	if (!GetTaskControlParent(controllerUIDataPtr->taskControl)) {
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	}
	// dim Abort button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 1);
	
	// Taken out because Task Controller is in an IDLE state and these should not be changed since pressing Start execution will continue
	/*
	// undim Repeat button if task is finite 
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Mode, &taskControllerMode);  
	if (!taskControllerMode == TASK_FINITE)
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
	
	// undim Iteration Wait button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Wait, ATTR_DIMMED, 0);
	// undim Mode button
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Mode, ATTR_DIMMED, 0);
	*/
	// update iterations display
	//SetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_TotalIterations, GetCurrentIterIndex(iterator));
	
	return 0; 
}

static int TaskTreeStateChangeUITC (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	// ignore if this is a Root UITC
	if (GetTaskControlParent(controllerUIDataPtr->taskControl))
		SetPanelAttribute(controllerUIDataPtr->panHndl, ATTR_DIMMED, (int) state);
	
	return 0;
}

static void	UITCActive (TaskControl_type* taskControl, BOOL UITCActiveFlag)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	
	if (UITCActiveFlag) {
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 0);
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 0);
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	} else {
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_StartStop, ATTR_DIMMED, 1);
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Abort, ATTR_DIMMED, 1);
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 1);
	}
	
}

static void ErrorUITC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	UITaskCtrl_type*	controllerUIDataPtr		= GetTaskControlModuleData(taskControl);
	BOOL				taskControllerMode;
	
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
	// undim Configure button if UITC doesn't have a parent task Controller
	if (!GetTaskControlParent(controllerUIDataPtr->taskControl))
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	
	SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Reset, ATTR_DIMMED, 0);
	// undim Repeat button if task is finite 
	GetCtrlVal(controllerUIDataPtr->panHndl, TCPan1_Mode, &taskControllerMode);  
	if (!taskControllerMode == TASK_FINITE)
		SetCtrlAttribute(controllerUIDataPtr->panHndl, TCPan1_Repeat, ATTR_DIMMED, 0);
}

int CVICALLBACK CloseDAQLabModulesPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			DiscardPanel(DAQLabModulesPanHndl);
			DAQLabModulesPanHndl = 0;
			break;
			
	}
	return 0;
}

void CVICALLBACK LogPanTaskLogMenu_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
INIT_ERR	
	
	size_t					nTCs 			= ListNumItems(DAQLabTCs);
	size_t					nVChans			= ListNumItems(VChannels);
	TaskControl_type*   	tc				= NULL;
	VChan_type*				VChan			= NULL;
	TCStates*				tcStates		= NULL;
	BOOL*					tcStateLocks	= NULL;
	char*					tcName			= NULL;
	char*					VChanName		= NULL;
	char*					tcStateName		= NULL;
	size_t					nTSQElements	= 0;
	char					nElemStr[50]	= "";
	
	// clear log box
	DeleteTextBoxLines(taskLogPanHndl, TaskLogPan_LogBox, 0, -1);
	
	// display log panel
	DisplayPanel(taskLogPanHndl);
	
	// get task controller states
	if (nTCs) {
		nullChk( tcStates = calloc(nTCs, sizeof(TCStates)) );
		nullChk( tcStateLocks = calloc(nTCs, sizeof(BOOL)) );
	}
	
	for (size_t i = 0; i < nTCs; i++) {
		tc = *(TaskControl_type**)ListGetPtrToItem(DAQLabTCs, i+1);
		errChk( GetTaskControlState_GetLock(tc, &tcStates[i], &tcStateLocks[i], __LINE__, __FILE__, &errorInfo.errMsg) );
	}
	
	// print current states of all task controllers
	SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, "Task Controller states:\n");
	for (size_t i = 0; i < nTCs; i++) {
		tc = *(TaskControl_type**)ListGetPtrToItem(DAQLabTCs, i+1);
		tcName		= GetTaskControlName(tc);
		tcStateName	= TaskControlStateToString(tcStates[i]);
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, tcName);
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, " = ");
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, tcStateName);
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, "\n");
		OKfree(tcName);
		OKfree(tcStateName);
	}
	
	// print number of elements left unprocessed in the Sink VChans
	SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, "\n\nUnprocessed Sink VChan elements:\n");
	for (size_t i = 1; i <= nVChans; i++) {
		VChan = *(VChan_type**)ListGetPtrToItem(VChannels, i);
		if (GetVChanDataFlowType(VChan) == VChan_Source) continue; // select Sink VChans
		
		CmtGetTSQAttribute(GetSinkVChanTSQHndl((SinkVChan_type*)VChan), ATTR_TSQ_ITEMS_IN_QUEUE, &nTSQElements);
		Fmt(nElemStr, "%s<%d", nTSQElements);
		VChanName		= GetVChanName(VChan);
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, VChanName);
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, " = ");
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, nElemStr);
		SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, "\n");
		OKfree(VChanName);
	}
	
	// enable logging for all task controllers
	SetCtrlVal(taskLogPanHndl, TaskLogPan_LogBox, "\n\nTask Controller actions:\n");
	for (size_t i = 1; i <= nTCs; i++) {
		tc = *(TaskControl_type**)ListGetPtrToItem(DAQLabTCs, i);
		EnableTaskControlLogging(tc, TRUE);
	}
	
Error:
	
	// release locks
	if (tcStateLocks)
		for (size_t i = 0; i < nTCs; i++) {
			tc = *(TaskControl_type**)ListGetPtrToItem(DAQLabTCs, i+1);
			if (tcStateLocks[i]) 
				GetTaskControlState_ReleaseLock(tc, &tcStateLocks[i], &errorInfo.errMsg);
		}
	OKfree(tcStateLocks);
	OKfree(tcStates);

PRINT_ERR
}

void CVICALLBACK TaskLogMenuClose_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	// disable logging for all task controllers
	size_t					nTCs = ListNumItems(DAQLabTCs);
	TaskControl_type**   	tcPtr;
	for (size_t i = 1; i <= nTCs; i++) {
		tcPtr = ListGetPtrToItem(DAQLabTCs, i);
		EnableTaskControlLogging(*tcPtr, FALSE);
	}
	
	// clear log box
	DeleteTextBoxLines(taskLogPanHndl, TaskLogPan_LogBox, 0, -1);
	
	// hide log panel
	HidePanel(taskLogPanHndl);
}
