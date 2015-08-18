//==============================================================================
//
// Title:		WhiskerScriptCallback.c
// Purpose:		Whisker Script callback functions defined here.
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
	char			file_path[FILE_PATH_LEN];
	
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
					
				case ScriptPan_ScriptApply:		/* Apply UI changes to Local Structure */
					apply_changes(cur_script);
					break;
					
				case ScriptPan_ScriptImportSetting:	/* Import settings from whisker UI */
					import_settings(whisker_script);
					break;
					
				case ScriptPan_ScriptSaveLog:	/* Save log file if it is valid */
					if (cur_script->log_file.VALID_FILE == FALSE) {
						MessagePopup("Save Log", "No log available!");
						return -1;
					}
					/* Ask user file path to save file */
					ret = FileSelectPopup("", "*.txt", "", "Log File",
                        		VAL_SAVE_BUTTON, 0, 1, 1, 0, UNCHECKED(file_path));
					
        			if (!ret) {	/* Cancelled operation */
						return -1;
					}
					
					/* Copy or Move temp file */
					CopyFile(LOG_TEMP_FILE, file_path);
					
					/* Set file path in text box */
					SetCtrlVal(whisker_script->main_panel_handle, ScriptPan_LogFile,
							   										file_path);
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
	size_t			insert_pos = 0;
	int				error = 0;
	
	cur_script = &(whisker_script->cur_script);
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, ScriptPan_ScriptElement, (int *)&index);
			GetCtrlVal(panel, ScriptPan_ScriptPos, &insert_pos);
			SetCtrlVal(panel, ScriptPan_ScriptPos, 0);	/* Reset back to zero */
			
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
					
				case MESSAGE: /* Message Element */
					nullChk(element = init_MessageElement(whisker_script, NULL));
					break;
					
				case SOUND:	/* Sound Element */
					nullChk(element = init_SoundElement(whisker_script, NULL));
					break;
					
				case XYMOVE: /* XY Move Element */
					nullChk(element = init_XYMoveElement(whisker_script, NULL));
					break;
					
				case ZMOVE:	/* Z Move Element */
					nullChk(element = init_ZMoveElement(whisker_script, NULL));
					break;
					
				case JUMP:	/* Jump Element */
					nullChk(element = init_JumpElement(whisker_script, NULL));
					break;
					
				case THWAIT: /* Thread Element */
					nullChk(element = init_THWaitElement(whisker_script, NULL));
					break;
			}
			
			/* Set Panel Attributes */
			SetPanelAttribute(element->panel_handle, ATTR_TITLEBAR_VISIBLE, 0);
			
			/* Adjust position */
			if (insert_pos == 0 || insert_pos > cur_script->num_elements) {
				insert_pos = END_OF_LIST;
			}
			
			/* Add this element into script_elements list */
			if (NULL == ListInsertItem(cur_script->script_elements, &element, 
									   	insert_pos)) {
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

inline void
delete_element(WScript_t *cur_script, int index)
{
	ScriptElement_t	*element = NULL;
	
	ListRemoveItem(cur_script->script_elements, &element, index);
	cur_script->num_elements -= 1;
					
	/* Discard Panel */
	DiscardPanel(element->panel_handle);
	
	if (element->MAGIC_NUM == XYMOVE) {	/* Free saved Positions */
			Position_t	*saved_position = NULL;
			while (ListNumItems(((XYMoveElement_t *)element)->saved_positions)) {
				ListRemoveItem(((XYMoveElement_t *)element)->saved_positions, 
							   		&saved_position, FRONT_OF_LIST);
				OKfree(saved_position);
			}
			OKfreeList(((XYMoveElement_t *)element)->saved_positions);
	}
	
	OKfree(element);
					
	/* Redraw UI */
	redraw_script_elements(cur_script);
	return;
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
			ListGetItem(whisker_script->cur_script.script_elements, &element, index);
			ActionElement_t *action_element = (ActionElement_t *)element;
			
			switch (control) {
				case ActionEle_EleDelete:	/* Delete Element */
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				//case ActionEle_EleApply:	/* Apply all Elements value */
					/* Update Structure */
				//	apply_action_changes(action_element);
				//	break;
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
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case StartEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					StartElement_t *start_element = (StartElement_t *)element;
					
					/* Update duration */
					/*apply_start_changes(start_element);
					break;*/
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
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case CondEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					ConditionElement_t *cond_element = (ConditionElement_t *)element;
					
					/* Update condition structure */
					/*apply_condition_changes(cond_element);
					break;*/
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
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case RepEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					RepeatElement_t *repeat_element = (RepeatElement_t *)element;
					
					/* Update condition structure */
					/*apply_repeat_changes(repeat_element);
					break;*/
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
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case StopEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					StopElement_t *stop_element = (StopElement_t *)element;
					
					/* Update duration */
					/*apply_stop_changes(stop_element);
					break; */
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
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case WaitEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					WaitElement_t *wait_element = (WaitElement_t *)element;
					
					/* Update duration */
					/*apply_wait_changes(wait_element);
					break;*/
			}
			break;
	}
	
	return 0;
}

/* Message Element Button Control Callback */
int  CVICALLBACK 
MsgElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, MsgEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case MsgEle_EleDelete:
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case MsgEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					MessageElement_t *message_element = (MessageElement_t *)element;
					
					/* Get Text Message */
					/*apply_message_changes(message_element);
					break;*/
			}
			break;
	}
	
	return 0;
}

/* Sound Element panel Callback */
int  CVICALLBACK 
SoundElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	int				ret = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, SoundEle_EleNum, seq_num);
			index = atoi(seq_num);
			ListGetItem(whisker_script->cur_script.script_elements, &element, index);
			SoundElement_t *sound_element = (SoundElement_t *)element;
			
			switch (control) {
				case SoundEle_EleDelete:   /* TODO: Create MACRO or inline function for following statements */
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case SoundEle_EleApply:
					/* Get Text Message */
					//apply_sound_changes(sound_element);
					//break;
					
				case SoundEle_EleLoad:	/* Load Sound file */
					ret = FileSelectPopup("", "*.wav", "", "Select a File",
                               VAL_LOAD_BUTTON, 0, 0, 1, 0, sound_element->file_path.file_path);
            		if (ret) {
            			SetCtrlVal(panel, SoundEle_ElePath, sound_element->file_path.file_path);
						sound_element->file_path.VALID_FILE = TRUE;
					}
					break;
					
				case SoundEle_ElePlay:	/* Play Sound File */
					if (sound_element->file_path.VALID_FILE == FALSE) {
						MessagePopup("Play Sound", "Please load proper sound file!");
						return -1;
					}
					
					sndPlaySound(sound_element->file_path.file_path, SND_SYNC);
					break;
			}
			break;
	}
	
	return 0;
}

/* XY Movement Button Control Callback */
int  CVICALLBACK 
XYMoveElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	Whisker_t		*whisker_m = (Whisker_t *)whisker_script->whisker_m;
	char			seq_num[3];
	int				index = 0;
	int				xyposition_panel_handle = 0;
	int				table_index;
	Position_t		*table_saved_position = NULL;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, XYMoveEle_EleNum, seq_num);
			index = atoi(seq_num);
			ListGetItem(whisker_script->cur_script.script_elements, &element, index);
			XYMoveElement_t *xymove_element = (XYMoveElement_t *)element;
			
			switch (control) {
				case XYMoveEle_EleDelete:
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				//case XYMoveEle_EleApply:
					/* Get Values */
					/*
					GetCtrlVal(panel, XYMoveEle_EleX, &(xymove_element->X));
					GetCtrlVal(panel, XYMoveEle_EleY, &(xymove_element->Y));
					GetCtrlVal(panel, XYMoveEle_ElePos, &(xymove_element->pos));
					*/
					//break;
					
				case XYMoveEle_EleShow:	/* Table to launch */
					xyposition_panel_handle = LoadPanel(0, MOD_WhiskerScript_UI, XYPosPan);
					if (xyposition_panel_handle < 0) {
						MessagePopup("XY Position Table Error", "Failed to load panel!");
							return -1;
					}
				
					/* Set Control Callback */
					SetCtrlAttribute(xyposition_panel_handle, XYPosPan_XYPositionOk,
										 	ATTR_CALLBACK_DATA, (void *)xymove_element);
					SetCtrlAttribute(xyposition_panel_handle, XYPosPan_XYPositionsTable,
										 	ATTR_CALLBACK_DATA, (void *)xymove_element);
					
					/* Add table rows from element->saved_positions */
					for (int i = 1; i <= ListNumItems(xymove_element->saved_positions); 
													i++) {
						ListGetItem(xymove_element->saved_positions, &table_saved_position, i);
						InsertTableRows(xyposition_panel_handle, XYPosPan_XYPositionsTable, i, 
										1, VAL_USE_MASTER_CELL_TYPE);
						SetTableCellVal (xyposition_panel_handle, XYPosPan_XYPositionsTable, 
								MakePoint(XYTABLE_X_COL, i), table_saved_position->X);
						SetTableCellVal (xyposition_panel_handle, XYPosPan_XYPositionsTable,
								MakePoint(XYTABLE_Y_COL, i), table_saved_position->Y);
						SetTableCellVal (xyposition_panel_handle, XYPosPan_XYPositionsTable,
								MakePoint(XYTABLE_PER_COL, i), table_saved_position->percent);
						SetTableCellVal (xyposition_panel_handle, XYPosPan_XYPositionsTable,
								MakePoint(XYTABLE_DEL_COL, i), DEL_LABEL);
					}
					
					DisplayPanel(xyposition_panel_handle);
					break;
					
				case XYMoveEle_EleAdd:	/* Add position from test panel saved positions */
					GetCtrlVal(panel, XYMoveEle_ElePos, &table_index);
					if (table_index > ListNumItems(whisker_m->z_dev.saved_positions)) {
						MessagePopup("Add XY Position Error", "No Such position in the table!");
						return -1;
					}
					ListGetItem(whisker_m->z_dev.saved_positions, &table_saved_position, 
								 							table_index);
					 
					Position_t	*saved_position = (Position_t *)malloc(sizeof(Position_t));
					if (saved_position == NULL) {
						MessagePopup("Positions Add Error", "Failed to allocation memory!");
						return -1;
					}
					*saved_position = *table_saved_position;	/* Copy Saved positions */
					/* Get the Percentage from UI */
					GetCtrlVal(panel, XYMoveEle_ElePercentage, &(saved_position->percent));
					
					/* Add into Element saved positions list */
					if (NULL == ListInsertItem(xymove_element->saved_positions, &saved_position, 
											   	END_OF_LIST)) {
						MessagePopup("Position Intert List Error", "Failed to insert position into the list");
						OKfree(saved_position);
						return -1;
					}
					break;
			}
			break;
	}
	
	return 0;
}

/* Z Movement Button Control Callback */
int  CVICALLBACK 
ZMoveElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, ZMoveEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case ZMoveEle_EleDelete:	/* Generic Delete */
					delete_element(&(whisker_script->cur_script), index);
					break;
				
				/*case ZMoveEle_EleApply:
					ListGetItem(whisker_script->cur_script.script_elements, &element, index);
					ZMoveElement_t *zmove_element = (ZMoveElement_t *)element;
					
					/* Get Values */
					/*apply_zmove_changes(zmove_element);
					break;*/
			}
			break;
	}
	
	return 0;
}

/* Jump Element Callback Handler */
int  CVICALLBACK 
JumpElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, JumpEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case JumpEle_EleDelete:	/* Generic Delete */
					delete_element(&(whisker_script->cur_script), index);
					break;
			}
			break;
	}
	return 0;
}

/* Thread WAit element callback */
int  CVICALLBACK 
THWaitElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	WhiskerScript_t	*whisker_script = (WhiskerScript_t *)callbackData;
	ScriptElement_t	*element = NULL;
	char			seq_num[3];
	int				index = 0;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, THWaitEle_EleNum, seq_num);
			index = atoi(seq_num);
			
			switch (control) {
				case THWaitEle_EleDelete:	/* Generic Delete */
					delete_element(&(whisker_script->cur_script), index);
					break;
			}
			break;
	}
	return 0;	
}

/* XY Position Callbacks */
int  CVICALLBACK 
XYPosTable_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	XYMoveElement_t *xymove_element = (XYMoveElement_t *)callbackData;
	Point			focus;
	Position_t		*saved_position = NULL;
	
	switch (event) {
		case EVENT_COMMIT:
			switch (control) {
				case XYPosPan_XYPositionOk:	
					/* Discard table and the panel */
					/* TODO: We do not need to explicitely remove table rows */
					DiscardPanel(panel);
					break;
					
				case XYPosPan_XYPositionsTable:	/* Controls embedded in Tables */
					GetActiveTableCell(panel, control, &focus);
					if (focus.x == XYTABLE_DEL_COL) {
						/* Remove saved position from list */
						ListRemoveItem(xymove_element->saved_positions, &saved_position, 
									   				focus.y);
						OKfree(saved_position);
						/* Remove row from table */
						DeleteTableRows(panel, control, focus.y, 1);
					}
					break;
			}
			break;
	}
	return 0;
}
