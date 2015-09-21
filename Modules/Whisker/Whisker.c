//==============================================================================
//
// Title:		Whisker.c
// Purpose:		Whisker Module main file. It implements init, load, discard etc.
//				functions.
//
// Created on:	17-7-2015 at 14:52:54 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h"
#include "Whisker.h"
#include "UI_Whisker.h"
#include <stdint.h> 

//==============================================================================
// Constants
//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions
static int 					LoadCfg(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo);

static int					SaveCfg(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int					Load(DAQLabModule_type* mod, int workspacePanHndl);

static int					DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag);

inline void					set_ctrl_attribute(Whisker_t *whisker);

//==============================================================================

// Global variables

//==============================================================================
// Global functions

DAQLabModule_type* 
initalloc_WhiskerModule(DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	Whisker_t *whisker_m = NULL;
	
	if (!mod) {
		whisker_m = (Whisker_t *) malloc(sizeof(Whisker_t));
		if (!whisker_m) {
			return NULL;
		}
	} else {
		whisker_m = (Whisker_t *) mod;
	}
		
	// initialize base class
	initalloc_DAQLabModule(&whisker_m->baseClass, className, instanceName, workspacePanHndl);
	
	//---------------------------
	// Parent class: DAQLabModule_type 
	
	// DATA
			
	// METHODS
	
	// overriding methods
	whisker_m->baseClass.Discard 		= discard_WhiskerModule;
	whisker_m->baseClass.Load			= Load;
	whisker_m->baseClass.LoadCfg		= NULL;		/* TODO : We can have NULL here */
	whisker_m->baseClass.SaveCfg		= NULL;		/* TODO : We can have NULL here */
	whisker_m->baseClass.DisplayPanels	= DisplayPanels;
	
	//---------------------------
	// Child class: DAQLabModule_type
	
	//----------------------
	// UI
	//----------------------
	whisker_m->whisker_ui.main_panel_handle = 0;
	whisker_m->whisker_ui.XYSetting_panel_handle = 0;
	whisker_m->whisker_ui.experiment_panel_handle = 0;
	//whisker_m->whisker_script = NULL;
	
	if (mod == NULL) {
		return (DAQLabModule_type*)whisker_m;	
	} else {
		return NULL;
	}
}

void 				
discard_WhiskerModule(DAQLabModule_type** mod)
{
	Whisker_t	*whisker_m = (Whisker_t *)(*mod);
	
	if (whisker_m == NULL) {
		return;
	}
	
	/* Stop Lick Detector timer */
	SetCtrlAttribute(whisker_m->whisker_ui.main_panel_handle, MainPanel_LickTimer, 
					 						ATTR_ENABLED, 0);
	
	/* Disconnect zaber device */
	close_zaber_device(&whisker_m->z_dev);
	/* Free comport list */
	for (int i = 0; whisker_m->z_dev.comport_list[i] != NULL; i++) {
		OKfree(whisker_m->z_dev.comport_list[i]);	
	}
	
	/* Close deditec device */
	close_deditec_device(&whisker_m->de_dev);
	
	DisplayPanels(*mod, FALSE);
	discard_DAQLabModule(mod);
	return;
}

static int 
LoadCfg(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
	return 0;	
}


static int
SaveCfg(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
	return 0;
}

static int 
Load(DAQLabModule_type* mod, int workspacePanHndl)
{
INIT_ERR

	int				error	= 0;
	Whisker_t		*whisker_m 	= (Whisker_t*)mod;
	char			IO_channel[7] = { 0 };
	
	/* load panel resources */
	errChk(whisker_m->whisker_ui.main_panel_handle = LoadPanel(0, MOD_Whisker_UI, MainPanel));
	
	/* Get Tab panel handles */
	GetPanelHandleFromTabPage(whisker_m->whisker_ui.main_panel_handle, MainPanel_IOChannelTab, TAB_DROP_IN,
							    &(whisker_m->whisker_ui.tab_drop_in));
	GetPanelHandleFromTabPage(whisker_m->whisker_ui.main_panel_handle, MainPanel_IOChannelTab, TAB_DROP_OUT,
							    &(whisker_m->whisker_ui.tab_drop_out));
	GetPanelHandleFromTabPage(whisker_m->whisker_ui.main_panel_handle, MainPanel_IOChannelTab, TAB_AIR_PUFF,
							    &(whisker_m->whisker_ui.tab_air_puff));
	
	/* Set control attribute of entire module UI */
	set_ctrl_attribute(whisker_m);

	/* Get available ports and insert list items into Zaber Port ring control */
	get_available_COMport(whisker_m->z_dev.comport_list);
	for (int i = 0; whisker_m->z_dev.comport_list[i] != NULL; i++) {
		InsertListItem(whisker_m->whisker_ui.main_panel_handle, MainPanel_ZaberComPort, i, 
					   whisker_m->z_dev.comport_list[i], i);
	}
	whisker_m->z_dev.VALID_DEVICE = FALSE;
	
	/* Init Sound structure */
	whisker_m->sound.VALID_FILE = FALSE;
	
	/* Invalid deditec handle */
	whisker_m->de_dev.handle = 0; /* TODO: Fail to understand why global varibale is not initialized to zero
 								   * Compiler dependent operation. Thus, do not rely on it */
	
	/* Init Expo info */
	whisker_m->exp_info.VALID_INFO = FALSE;
	
	/* Insert IO channels list into AIR_PUFF, DROP_IN, DROP_OUT UI.
	 * One can do it from UI, but 32 * 3 entries have to entered manually.
	 * However, it's a one time job in UI.
	 */
	for (int i = 0; i < TOT_IO_CH; i++) {
		sprintf(IO_channel, "CH %d", i+1);	/* Show Channels to User */
		InsertListItem(whisker_m->whisker_ui.tab_air_puff, TabAirPuff_IO_AirPuff, i,
					   IO_channel, i);
		InsertListItem(whisker_m->whisker_ui.tab_drop_out, TabDropOut_IO_DropOut, i,
					   IO_channel, i);
		InsertListItem(whisker_m->whisker_ui.tab_drop_in, TabDropIn_IO_DropIN, i,
					   IO_channel, i);
		InsertListItem(whisker_m->whisker_ui.main_panel_handle, MainPanel_IO_ZAxis, i,
					   IO_channel, i);
		InsertListItem(whisker_m->whisker_ui.main_panel_handle, MainPanel_IO_LickDetection, i,
					   IO_channel, i);
	}
	/* Set default value of IO Channels */
	whisker_m->de_dev.IO_Channel[AIR_PUFF_CH] = AIRPUFF_DEFAULT_CH;
	whisker_m->de_dev.IO_Channel[DROP_OUT_CH] = DROPOUT_DEFAULT_CH;
	whisker_m->de_dev.IO_Channel[DROP_IN_CH] = DROPIN_DEFAULT_CH;
	whisker_m->de_dev.IO_Channel[ZAXIS_MOVE_CH] = ZAXIS_DEFAULT_CH;
	whisker_m->de_dev.IO_Channel[LICK_DET_CH] = LICKDET_DEFAULT_CH;
	
	SetCtrlVal(whisker_m->whisker_ui.tab_air_puff, TabAirPuff_IO_AirPuff, 
									AIRPUFF_DEFAULT_CH);
	SetCtrlVal(whisker_m->whisker_ui.tab_drop_out, TabDropOut_IO_DropOut, 
									DROPOUT_DEFAULT_CH);
	SetCtrlVal(whisker_m->whisker_ui.tab_drop_in, TabDropIn_IO_DropIN,
									DROPIN_DEFAULT_CH); 
	SetCtrlVal(whisker_m->whisker_ui.main_panel_handle, MainPanel_IO_ZAxis,
									ZAXIS_DEFAULT_CH);
	SetCtrlVal(whisker_m->whisker_ui.main_panel_handle, MainPanel_IO_LickDetection,
			   						LICKDET_DEFAULT_CH);
	
	/* Load Initial screen to ask user about experiment info */
	whisker_m->whisker_ui.experiment_panel_handle =  
			LoadPanel(whisker_m->whisker_ui.main_panel_handle, MOD_Whisker_UI, ExpPanel);
	if (whisker_m->whisker_ui.experiment_panel_handle < 0) {
		MessagePopup("Experiment Info Error", "Failed to load panel!");
	} else {
		SetCtrlAttribute(whisker_m->whisker_ui.experiment_panel_handle, ExpPanel_ExpOk,
								ATTR_CALLBACK_DATA, (void *)whisker_m);
		SetCtrlAttribute(whisker_m->whisker_ui.experiment_panel_handle, ExpPanel_ExpCancel,
								ATTR_CALLBACK_DATA, (void *)whisker_m);
	}
	
	return 0;
	
Error:
	LOG_MSG(1, "Error occured in the load function");
	return errorInfo.error;
}

static int 
DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
INIT_ERR

	Whisker_t	*whisker_t 	= (Whisker_t*) mod;
	int 		error 	= 0;

	if (whisker_t->whisker_ui.main_panel_handle < 0) {
		  LOG_MSG1(5, "Invalid handle received Display Panel %d",
				   				whisker_t->whisker_ui.main_panel_handle);
		  return -1;
	}
		
	if (visibleFlag) {
		errChk(DisplayPanel(whisker_t->whisker_ui.main_panel_handle));
		if (whisker_t->whisker_ui.experiment_panel_handle >= 0) {
			errChk(DisplayPanel(whisker_t->whisker_ui.experiment_panel_handle));
		}
	} else {
		errChk(DiscardPanel(whisker_t->whisker_ui.main_panel_handle));
		whisker_t->whisker_ui.main_panel_handle = -1;
	}

Error:
	return errorInfo.error;
}

inline void
set_ctrl_attribute(Whisker_t *whisker_m)
{
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_QUITBUTTON, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYAbs, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYRight, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYLeft, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYArrowLeft, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYArrowRight, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYArrowUp, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYArrowDown, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYAddRow, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYPositionsTable, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYHome, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYUpdatePos, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_ZArrowUp, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_ZArrowDown, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	/* Port Buttons */
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_PortOpen, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_PortClose, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	/* IO channel ring controls */
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_in, TabDropIn_IO_DropIN, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_out, TabDropOut_IO_DropOut, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_air_puff, TabAirPuff_IO_AirPuff, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_IO_ZAxis, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_IO_LickDetection, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	/* Buttons */
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_LoadSound, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_SoundPlay, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_BuildScript, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfo, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	/* Check boxes */
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_CheckSync, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_air_puff, TabAirPuff_Check_AirPuff, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_in, TabDropIn_Check_DropIN, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_out, TabDropOut_Check_DropOut, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	/* Toggle buttons */
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_in, TabDropIn_Toggle_DropIN, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_out, TabDropOut_Toggle_DropOut, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_air_puff, TabAirPuff_Toggle_AirPuff, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_in, TabDropIn_Toggle_Inter_DropIN, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_drop_out, TabDropOut_Toggle_Inter_DropOut, ATTR_CALLBACK_DATA, (void *)whisker_m);
	SetCtrlAttribute (whisker_m->whisker_ui.tab_air_puff, TabAirPuff_Toggle_Inter_AirPuff, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_LickTimer, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	/* XY setting button that launches Setting Panel */
	SetCtrlAttribute (whisker_m->whisker_ui.main_panel_handle, MainPanel_XYSetting, ATTR_CALLBACK_DATA, (void *)whisker_m);
	
	
	return;
}

