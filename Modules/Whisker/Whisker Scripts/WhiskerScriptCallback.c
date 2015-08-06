//==============================================================================
//
// Title:		WhiskerScriptCallback.c
// Purpose:		A short description of the implementation.
//
// Created on:	25-7-2015 at 22:57:55 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h"
#include "WhiskerScript.h"
#include "Whisker.h"
#include "UI_Scripts.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions

/// HIFN  What does your function do?
/// HIPAR x/What inputs does your function expect?
/// HIRET What does your function return?

int  CVICALLBACK 
WhiskerScriptButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2) 
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	WScript_t		*cur_script = NULL;
	int				error = 0;
	int				ret = 0;
	
	cur_script = &(whisker_script->cur_script);
	
	switch (event) {
	
		case EVENT_COMMIT:
			
			switch (control) {
				case ScriptPan_ScriptNew:	/* New Script Button pressed */
					if (cur_script->script_elements != NULL) {
						ret = ConfirmPopup ("New Script", "Discard existing script?");
						if (ret == 0) {		/* Cancelled Operation */
							return 0;	
						} 
						discard_cur_script(cur_script);	
					}
					
					/* Init Current Script elements */
					nullChk(cur_script->script_elements = ListCreate(sizeof(ScriptElement_t*)));
					cur_script->num_elements = 0;
					
					/* TODO: UnDim other controls */
					SetCtrlAttribute(panel, ScriptPan_ScriptElement, ATTR_DIMMED, 0);
					SetCtrlAttribute(panel, ScriptPan_ScriptAdd, ATTR_DIMMED, 0);
					
					/* Set File Name to <New Script> */
					SetCtrlVal(panel, ScriptPan_ScriptName, NEW_SCRIPT_NAME);
					
					break;
					
				case ScriptPan_ScriptQuit:	/* Quit Button pressed */
					discard_script_module(whisker_script);
					break;
					
				case ScriptPan_ScriptSave:	/* Save Button pressed */
					save_script(whisker_script);
					break;
					
				case ScriptPan_ScriptLoad:	/* Load Script */
					load_script(whisker_script);
					break;
					
				case ScriptPan_ScriptRun:	/* Run Script */
					CmtGetLock(cur_script->lock);
					if (cur_script->run_status == STOPPED) {  /* No thread running */
						CmtScheduleThreadPoolFunction (DEFAULT_THREAD_POOL_HANDLE, script_runner, 
													   		whisker_script, NULL);
					} else {	/* Thread already running. This is a resume */
						cur_script->run_status = STARTED;
						/* Dim itself */
						SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptRun,
							ATTR_DIMMED, 1);
					
						/* UnDim Start and Stop */
						SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 0);
						SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 0);
					}
					CmtReleaseLock(cur_script->lock);
						
					break;
					
				case ScriptPan_ScriptPause: /* Pause Script */
					CmtGetLock(cur_script->lock);
					cur_script->run_status = PAUSED;
					CmtReleaseLock(cur_script->lock);
					
					/* Dim itself */
					SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 1);
					
					/* UnDim Start and Stop */
					SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptRun,
							ATTR_DIMMED, 0);
					SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 0);
					break;
					
				case ScriptPan_ScriptStop:	/* Stop Script */
					CmtGetLock(cur_script->lock);
					cur_script->run_status = STOPPED;
					CmtReleaseLock(cur_script->lock);
					
					/* Dim itself */
					SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptStop,
							ATTR_DIMMED, 1);
					SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptPause,
							ATTR_DIMMED, 1);
					
					/* UnDim Start and Stop */
					SetCtrlAttribute(whisker_script->main_panel_handle, ScriptPan_ScriptRun,
							ATTR_DIMMED, 0);
					break;
					
				case ScriptPan_ScriptDelete:	/* Delete only script elements */
					
					if (cur_script->script_elements == NULL ||
					   		cur_script->num_elements <= 0) {
						MessagePopup("Delete Script", "No Elements to delete!");
						return -1;
					}
					
					ret = ConfirmPopup ("Delete Script", "Are you sure you want to "
														"delete current script?");
					if (ret == 0) {		/* Cancelled Operation */
						return 0;	
					}
					
					discard_script_elements(cur_script);
					break;
					
				case ScriptPan_ScriptImportSetting:	/* Import settings from whisker UI */
					import_settings(whisker_script);
					break;
			}
			
			break;
	}
	
Error:
	return error;
}

int  CVICALLBACK 
ScriptAddElement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	WScript_t		*cur_script = NULL;
	ElementType_t	index;
	ScriptElement_t	*element = NULL;
	int				error = 0;
	
	cur_script = &(whisker_script->cur_script);
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, ScriptPan_ScriptElement, (int *)&index);
			
			switch (index) {
				case START: /* Start Element */
					nullChk(element = init_StartElement(whisker_script, NULL));
					break;
					
				case ACTION: /* Action Element */
					nullChk(element = init_ActionElement(whisker_script, NULL));
					break;
					
				case CONDITION: /* Condition Element */
					nullChk(element = init_ConditionElement(whisker_script, NULL));
					break;
					
				case REPEAT: /* Repeat Element */
					nullChk(element = init_RepeatElement(whisker_script, NULL));
					break;
					
				case STOP: /* Stop Element */
					nullChk(element = init_StopElement(whisker_script, NULL));
					break;
					
				case WAIT: /* Wait Element */
					nullChk(element = init_WaitElement(whisker_script, NULL));
					break;
			}
			
			/* Set Panel Attributes */
			SetPanelAttribute(element->panel_handle, ATTR_TITLEBAR_VISIBLE, 0);
				
			/* Add this element into script_elements list */
			if (NULL == ListInsertItem(cur_script->script_elements, &element, (cur_script->num_elements + 1))) {
				DiscardPanel(element->panel_handle);
				LOG_MSG(0, "Error when inserting element into list\n");
				goto Error;
			}
			cur_script->num_elements += 1;

			/* Do not use this function to add elements. 
			 * We just have to add elements to the last 
			 */
			redraw_script_elements(cur_script);
			break;
	}
	
	return 0;
Error:
	OKfree(element);
	return error;
}

/* Action Element Control related callback */
int  CVICALLBACK 
ActionElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, ActionEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case ActionEle_EleDelete:
					ListRemoveItem(whisker_script->cur_script.script_elements, &element, index);
					whisker_script->cur_script.num_elements -= 1;
					
					/* Discard Panel */
					DiscardPanel(element->panel_handle);
					OKfree(element);
					
					/* Redraw UI */
					redraw_script_elements(&(whisker_script->cur_script));
					break;
				
				case ActionEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					ActionElement_t *action_element = (ActionElement_t *)element;
					
					/* Update Structure */
					GetCtrlVal(panel, ActionEle_EleDuration, &(action_element->duration));
					GetCtrlVal(panel, ActionEle_EleCommand, (int *)&(action_element->action));
					GetCtrlVal(panel, ActionEle_EleIO_CH, &(action_element->IO_Channel)); /* 0 : Default */
					break;
			}
			break;
	}
	
	return 0;
}

/* Start Element Button Control Callback */
int  CVICALLBACK 
StartElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, StartEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case StartEle_EleDelete:
					ListRemoveItem(whisker_script->cur_script.script_elements, &element, index);
					whisker_script->cur_script.num_elements -= 1;
					
					/* Discard Panel */
					DiscardPanel(element->panel_handle);
					OKfree(element);
					
					/* Redraw UI */
					redraw_script_elements(&(whisker_script->cur_script));
					break;
				
				case StartEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					StartElement_t *start_element = (StartElement_t *)element;
					
					/* Update duration */
					GetCtrlVal(panel, StartEle_EleDelay, &(start_element->delay));
					break;
			}
			break;
	}
	
	return 0;
}

/* Condition Element Button Control Callback */
int  CVICALLBACK 
CondElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, CondEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case CondEle_EleDelete:
					ListRemoveItem(whisker_script->cur_script.script_elements, &element, index);
					whisker_script->cur_script.num_elements -= 1;
					
					/* Discard Panel */
					DiscardPanel(element->panel_handle);
					OKfree(element);
					
					/* Redraw UI */
					redraw_script_elements(&(whisker_script->cur_script));
					break;
				
				case CondEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					ConditionElement_t *cond_element = (ConditionElement_t *)element;
					
					/* Update condition structure */
					GetCtrlVal(panel, CondEle_EleIO_CH, &(cond_element->IO_channel));
					GetCtrlVal(panel, CondEle_EleValue, &(cond_element->value));
					GetCtrlVal(panel, CondEle_EleTrue, &(cond_element->true_step));
					GetCtrlVal(panel, CondEle_EleFalse, &(cond_element->false_step));
					break;
			}
			break;
	}
	
	return 0;
}

/* Repeat Element Button Control Callback */
int  CVICALLBACK 
RepeatElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, RepEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case RepEle_EleDelete:
					ListRemoveItem(whisker_script->cur_script.script_elements, &element, index);
					whisker_script->cur_script.num_elements -= 1;
					
					/* Discard Panel */
					DiscardPanel(element->panel_handle);
					OKfree(element);
					
					/* Redraw UI */
					redraw_script_elements(&(whisker_script->cur_script));
					break;
				
				case RepEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					RepeatElement_t *repeat_element = (RepeatElement_t *)element;
					
					/* Update condition structure */
					GetCtrlVal(panel, RepEle_EleNTimes, &(repeat_element->ntimes));
					GetCtrlVal(panel, RepEle_EleStep, &(repeat_element->repeat_step));
				
					break;
			}
			break;
	}
	
	return 0;
}

/* Stop Element Button Control Callback */
int  CVICALLBACK 
StopElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, StopEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case StopEle_EleDelete:
					ListRemoveItem(whisker_script->cur_script.script_elements, &element, index);
					whisker_script->cur_script.num_elements -= 1;
					
					/* Discard Panel */
					DiscardPanel(element->panel_handle);
					OKfree(element);
					
					/* Redraw UI */
					redraw_script_elements(&(whisker_script->cur_script));
					break;
				
				case StopEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					StopElement_t *stop_element = (StopElement_t *)element;
					
					/* Update duration */
					GetCtrlVal(panel, StopEle_EleDelay, &(stop_element->delay));
					break;
			}
			break;
	}
	
	return 0;
}

/* Wait Element Button Control Callback */
int  CVICALLBACK 
WaitElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, WaitEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case WaitEle_EleDelete:
					ListRemoveItem(whisker_script->cur_script.script_elements, &element, index);
					whisker_script->cur_script.num_elements -= 1;
					
					/* Discard Panel */
					DiscardPanel(element->panel_handle);
					OKfree(element);
					
					/* Redraw UI */
					redraw_script_elements(&(whisker_script->cur_script));
					break;
				
				case WaitEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					WaitElement_t *wait_element = (WaitElement_t *)element;
					
					/* Update duration */
					GetCtrlVal(panel, WaitEle_EleDelay, &(wait_element->delay));
					break;
			}
			break;
	}
	
	return 0;
}

