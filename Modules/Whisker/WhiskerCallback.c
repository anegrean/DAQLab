//==============================================================================
//
// Title:		WhiskerCallback.c
// Purpose:		Define all Whisker Callback functions
//
// Created on:	18-7-2015 at 16:48:56 by Vinod Nigade.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "DAQLab.h"
#include "Whisker.h"
#include "UI_Whisker.h"
#include "WhiskerScript.h"

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

int CVICALLBACK 
QuitButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t	*whisker_m = (Whisker_t *)callbackData;
	
	switch (event) {
		case EVENT_COMMIT:	/* Text Box have commit only when enter key is pressed */
			/* If script module is running, discard it */
			discard_script_module();
			
			/* Hiding panel seems better way to handle quit panel.
			 * Module discard function will be called when the entire application is closed.
		     * When entire application is restarted, whisker module will be restarted if it
			 * remains in the open module list. Otherwise have to start manually.
			 */
			HidePanel(whisker_m->whisker_ui.main_panel_handle);
			break;
	}
	return 0;
}

inline void
update_xy_positions(zaber_device_t *z_dev, int panel)
{
	size_t	abs_pos, x_pos, y_pos;
	
	abs_pos = get_device_data(z_dev->port, ZABER_DEV, "pos");
	x_pos = get_device_data(z_dev->port, ZABER_X_DEV, "pos");
	y_pos = get_device_data(z_dev->port, ZABER_Y_DEV, "pos");
					
	SetCtrlVal(panel, MainPanel_XYAbs, abs_pos);
	SetCtrlVal(panel, MainPanel_XYLeft, y_pos);
	SetCtrlVal(panel, MainPanel_XYRight, x_pos);
	return;
}

int CVICALLBACK 
XYMovement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	char			com_port[6] = { 0 };
	int				position = 0;
	int				value;
	int				num_rows;
	int				isSelected = 0;
	Position_t		*saved_position = NULL;
	
	z_dev = &(whisker_m->z_dev);
	
	/* If connection is not opened then do not perform any operation */
	if (z_dev->VALID_DEVICE == FALSE) {
		return -1;
	}
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &position);
			switch(control) {
				case MainPanel_XYAbs:
					/* Send ABS Command */
					send_MoveABS_cmd(z_dev->port, ZABER_DEV, position, SYNC);
					update_xy_positions(z_dev, panel);
					break;
					
				case MainPanel_XYLeft:
					send_MoveABS_cmd(z_dev->port, ZABER_Y_DEV, position, SYNC);
					update_xy_positions(z_dev, panel);
					break;
					
				case MainPanel_XYRight:
					send_MoveABS_cmd(z_dev->port, ZABER_X_DEV, position, SYNC); 
					update_xy_positions(z_dev, panel);
					break;
				
				case MainPanel_XYArrowLeft:
				case MainPanel_XYArrowRight:
				case MainPanel_XYArrowUp:
				case MainPanel_XYArrowDown:
					value = position;
					if (value == 1) {			/* Button pressed */
						if (control == MainPanel_XYArrowRight) {
							send_MoveMaxMin_cmd(z_dev->port, ZABER_X_DEV, "max");
						} else if (control == MainPanel_XYArrowLeft) {
							send_MoveMaxMin_cmd(z_dev->port, ZABER_X_DEV, "min");
						} else if (control == MainPanel_XYArrowUp) {
							send_MoveMaxMin_cmd(z_dev->port, ZABER_Y_DEV, "max");
						} else if (control == MainPanel_XYArrowDown) {
							send_MoveMaxMin_cmd(z_dev->port, ZABER_Y_DEV, "min");
						}
						
						isSelected = 1;
					} else if (value == 0) {	/* Button unpressed */
						send_stop_cmd(z_dev->port, ZABER_DEV);
						update_xy_positions(z_dev, panel);
					}
					
					/* Dim Undim other controls */
					if (control == MainPanel_XYArrowRight) {
						SetCtrlAttribute(panel, MainPanel_XYArrowLeft, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowUp, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowDown, ATTR_DIMMED, isSelected);
					} else if (control == MainPanel_XYArrowLeft) {
						SetCtrlAttribute(panel, MainPanel_XYArrowRight, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowUp, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowDown, ATTR_DIMMED, isSelected);
					} else if (control == MainPanel_XYArrowUp) {
						SetCtrlAttribute(panel, MainPanel_XYArrowRight, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowLeft, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowDown, ATTR_DIMMED, isSelected);
					} else if (control == MainPanel_XYArrowDown) {
						SetCtrlAttribute(panel, MainPanel_XYArrowRight, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowUp, ATTR_DIMMED, isSelected);
						SetCtrlAttribute(panel, MainPanel_XYArrowLeft, ATTR_DIMMED, isSelected);
					}
					
					break;
					
				case MainPanel_XYAddRow: /* Add current values of X and Y to saved positions list */
					if (z_dev->VALID_DEVICE == FALSE) {
						MessagePopup("Zaber Device", "Device is not open!");
						return -1;
					}
					
					saved_position = (Position_t *)malloc(sizeof(Position_t));
					if (saved_position == NULL) {
						MessagePopup("Zaber Position", "Failed to allocation memory!");
						return -1;
					}
					
					/* Get current X and Y values */
					GetCtrlVal(panel, MainPanel_XYRight, &(saved_position->X));
					GetCtrlVal(panel, MainPanel_XYLeft, &(saved_position->Y));
					
					/* Insert into the list */
					if (NULL == ListInsertItem(z_dev->saved_positions, &saved_position, 
											   	END_OF_LIST)) {
						MessagePopup("Zaber Position", "Failed to insert position into the list");
						OKfree(saved_position);
						return -1;
					}
					
					/* Create a new row and update table values */
					GetNumTableRows (panel, MainPanel_XYPositionsTable, &num_rows);
					num_rows++;
					InsertTableRows(panel, MainPanel_XYPositionsTable, num_rows, 1, 
																VAL_USE_MASTER_CELL_TYPE);
					SetTableCellVal (panel, MainPanel_XYPositionsTable, 
									 			MakePoint(XYTABLE_X_COL, num_rows), saved_position->X);
					SetTableCellVal (panel, MainPanel_XYPositionsTable,
									 			MakePoint(XYTABLE_Y_COL, num_rows), saved_position->Y);
					SetTableCellVal (panel, MainPanel_XYPositionsTable,
									 			MakePoint(XYTABLE_GO_COL, num_rows), GO_LABEL);
					SetTableCellVal (panel, MainPanel_XYPositionsTable,
									 			MakePoint(XYTABLE_DEL_COL, num_rows), DEL_LABEL);
					/* unToggle this */
					SetCtrlVal(panel, control, 0);
					break;
			}
			
	}
	
	return 0;
	
}

/* Position Table callbacks. Handles Go and Delete operations */
int  CVICALLBACK 
PositionTable_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	Point			focus;
	Position_t		*saved_position = NULL;
	
	z_dev = &(whisker_m->z_dev);
	
	switch (event) {
		case EVENT_COMMIT:
			GetActiveTableCell(panel, control, &focus);
			
			if (focus.x == XYTABLE_GO_COL) {			/* Move zaber using desired position */
				ListGetItem(z_dev->saved_positions, &saved_position, focus.y);
				/* Move Zaber device in X direction */
				send_MoveABS_cmd(z_dev->port, ZABER_X_DEV, saved_position->X, SYNC);
				send_MoveABS_cmd(z_dev->port, ZABER_Y_DEV, saved_position->Y, SYNC);
			} else if (focus.x == XYTABLE_DEL_COL) { 	/* Delete row from the table and list */
				/* Remove saved position from list */
				ListRemoveItem(z_dev->saved_positions, &saved_position, focus.y);
				OKfree(saved_position);
				/* Remove row from table */
				DeleteTableRows(panel, control, focus.y, 1);
			}
			break;
	}
	return 0;
}

/* Z Axis movement Handler */
int CVICALLBACK 
ZMovement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	delib_device_t	*de_dev = NULL;
	
	de_dev = &(whisker_m->de_dev);
	
	switch (event) {
		case EVENT_COMMIT:
			switch (control) {
				/* Z Axis IO channel handle seperately in the function IO_Channel_CB */
				case MainPanel_ZArrowUp:	/* Move up */
					set_without_timer(de_dev, de_dev->IO_Channel[ZAXIS_MOVE_CH], 1); 
					break;
				case MainPanel_ZArrowDown:	/* Move Down */
					set_without_timer(de_dev, de_dev->IO_Channel[ZAXIS_MOVE_CH], 0);
					break;
			}
			
			/* UN Toggle the button */
			SetCtrlVal(panel, control, 0);
			break;
	}
	
	return 0;
}

int  CVICALLBACK
WhiskerButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	WhiskerUI_t		*whisker_ui = NULL;
	zaber_device_t	*z_dev = NULL;
	delib_device_t	*de_dev = NULL;
	int				index;
	int				ret = 0;
	
	whisker_ui = &(whisker_m->whisker_ui);
	z_dev = &(whisker_m->z_dev);
	de_dev = &(whisker_m->de_dev);
	
	switch (event) {
			 case EVENT_COMMIT:
				 /* It is safe to call these init functions from UI callback context
				  * Because we do not expect any operation on other UI componants.
				  */
			 	switch (control) {
					case MainPanel_PortOpen:	/* Port Open */
						 GetCtrlVal(panel, MainPanel_ZaberComPort, &index);
						 if (index < 0) {
							 MessagePopup ("COM Port Open Error", "Please select zaber port!");
							 return -1;
						 }
						 strncpy(z_dev->device_name, z_dev->comport_list[index], 5);
				 
						 /* Init zaber device */
						 ret = init_zaber_device(&whisker_m->z_dev);
				 
						 if (ret < 0) {
							 MessagePopup ("COM Port Open Error", "Fail to init zaber device!");
							 return -1;
						 }
				 
						 GetCtrlVal(panel, MainPanel_DeditecModule, &index);
						 if (index < 0) {
							 close_zaber_device(z_dev);
							 MessagePopup ("Deditec module Open Error", "Please select deditec module!");
							 return -1;
						 } else if (index == 0) {
							 de_dev->module_number = USB_TTL_32;
						 } else if (index == 1) {
							 de_dev->module_number = USB_TTL_64;
						 }
				 
						 /* Init deditec device */
						 ret = init_deditec_device(de_dev);
				 
						 if (ret < 0) {
							close_zaber_device(z_dev); 
							MessagePopup ("Deditec module Open Error", "Fail to init deditec module!");
							return -1;	 
						 }
				 
						 /* Dim this button */
						 SetCtrlAttribute(panel, MainPanel_PortOpen, ATTR_DIMMED, 1);
						 SetCtrlAttribute(panel, MainPanel_PortClose, ATTR_DIMMED, 0);
						 SetCtrlAttribute (panel, MainPanel_LED_Port, ATTR_ON_COLOR, VAL_GREEN);
						 SetCtrlVal(panel, MainPanel_LED_Port, 1);
						 SetCtrlAttribute(panel, MainPanel_XYSetting, ATTR_DIMMED, 0); /* XY setting */
						 SetCtrlAttribute(panel, MainPanel_XYHome, ATTR_DIMMED, 0);	 /* Home Position */
						 SetCtrlAttribute(panel, MainPanel_XYUpdatePos, ATTR_DIMMED, 0); /* Update Current Position */
						 
						 /* Start Lick Detector timer */
						 SetCtrlAttribute (panel, MainPanel_LickTimer, ATTR_ENABLED, 1);
						 break;
						 
					case MainPanel_PortClose:
						/* Stop Lick Detector Timer */
						SetCtrlAttribute (panel, MainPanel_LickTimer, ATTR_ENABLED, 0);
						
						close_zaber_device(z_dev);
						close_deditec_device(de_dev);
						
						SetCtrlAttribute(panel, MainPanel_PortOpen, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_PortClose, ATTR_DIMMED, 1);
						SetCtrlVal(panel, MainPanel_LED_Port, 0);
						SetCtrlAttribute(panel, MainPanel_XYSetting, ATTR_DIMMED, 1); 	/* XY setting */
						SetCtrlAttribute(panel, MainPanel_XYHome, ATTR_DIMMED, 1);	 	/* Home Position */
						SetCtrlAttribute(panel, MainPanel_XYUpdatePos, ATTR_DIMMED, 1); /* Update Current Position */
						
						break;
						 
					case MainPanel_SoundPlay:	/* Play Sound */
						if (whisker_m->sound.VALID_FILE == FALSE) {
							MessagePopup ("Play Sound File Error", "Before playing a sound file,"
										  " you have to select a file!");
							return -1;
						}
						sndPlaySound(whisker_m->sound.file_path, 
									 		whisker_m->sound.isSYNC ? SND_SYNC : SND_ASYNC);
						break;
						
					case MainPanel_BuildScript: /* Launch Build Script Panel */
						init_display_script((void *)whisker_m);
						break;
						
					case MainPanel_ExperimentInfo:	/* Launch Experiment Info Panel */
						if (whisker_ui->experiment_panel_handle != 0) {	/* Poor User asking for another panel */
							LOG_MSG(1, "Experiment info is already running ");
							return -1;
						}
						whisker_ui->experiment_panel_handle =  LoadPanel(panel, MOD_Whisker_UI, 
															   					ExpPanel);
						if (whisker_ui->experiment_panel_handle < 0) {
							MessagePopup("Experiment Info Error", "Failed to load panel!");
							return -1;
						}
						
						//SetCtrlAttribute(whisker_ui->experiment_panel_handle, ExpPanel_ExpMsg,
						//				 	ATTR_NO_EDIT_TEXT, 0);
						SetCtrlAttribute(whisker_ui->experiment_panel_handle, ExpPanel_ExpOk,
										 	ATTR_CALLBACK_DATA, (void *)whisker_m);
						SetCtrlAttribute(whisker_ui->experiment_panel_handle, ExpPanel_ExpCancel,
										 	ATTR_CALLBACK_DATA, (void *)whisker_m);
						
						/* If experiment info is valid, then show it in UI */
						if (whisker_m->exp_info.VALID_INFO == TRUE) {
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpName, 
								   								whisker_m->exp_info.user_name);
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpNum, 
								   								whisker_m->exp_info.exp_num);
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpMsg, 	
								   								whisker_m->exp_info.extra_msg);
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpAniNum, 	
								   								whisker_m->exp_info.animal_num);
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpAniAge, 	
								   								whisker_m->exp_info.animal_age);
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpAniWeight, 	
								   								whisker_m->exp_info.animal_weight);
							SetCtrlVal(whisker_ui->experiment_panel_handle, ExpPanel_ExpTrainNum, 	
								   								whisker_m->exp_info.training_num);
						}
						
						/* Display Panel */
						DisplayPanel(whisker_ui->experiment_panel_handle);
						break;
				 }
				 
				 break;
	}
	
	return 0;
}

int  CVICALLBACK 
IO_Channel_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	delib_device_t	*de_dev = NULL;
	int				index;
	
	de_dev = &(whisker_m->de_dev);
	
	switch (event) {
			 case EVENT_COMMIT:
				GetCtrlVal(panel, control, &index);
				
				if (panel == whisker_m->whisker_ui.tab_air_puff && 
							control == TabAirPuff_IO_AirPuff) {
						de_dev->IO_Channel[AIR_PUFF_CH] = index;
						/* Undim operation for this channel */
						SetCtrlAttribute(panel, TabAirPuff_Check_AirPuff, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabAirPuff_Num_AirPuff, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabAirPuff_Toggle_AirPuff, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabAirPuff_Inter_AirPuff, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabAirPuff_Toggle_Inter_AirPuff, ATTR_DIMMED, 0);
						
				} else if (panel == whisker_m->whisker_ui.tab_drop_in && 
						 	control == TabDropIn_IO_DropIN) {
						de_dev->IO_Channel[DROP_IN_CH] = index;
						
						/* Undim operation for this channel */
						SetCtrlAttribute(panel, TabDropIn_Check_DropIN, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropIn_Num_DropIN, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropIn_Toggle_DropIN, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropIn_Inter_DropIN, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropIn_Toggle_Inter_DropIN, ATTR_DIMMED, 0);
						
				} else if (panel == whisker_m->whisker_ui.tab_drop_out &&
						 	control == TabDropOut_IO_DropOut) {
						de_dev->IO_Channel[DROP_OUT_CH] = index;
						/* Undim operation for this channel */
						SetCtrlAttribute(panel, TabDropOut_Check_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropOut_Num_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropOut_Toggle_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropOut_Inter_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, TabDropOut_Toggle_Inter_DropOut, ATTR_DIMMED, 0);
						
				} else if (panel == whisker_m->whisker_ui.main_panel_handle && 
						 	control == MainPanel_IO_ZAxis) {
						
						de_dev->IO_Channel[ZAXIS_MOVE_CH] = index;
						/* Undim operation for this channel */
						/* This should be decided once we decide about rod movement.
						SetCtrlAttribute(panel, MainPanel_Check_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Num_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Toggle_DropOut, ATTR_DIMMED, 0);
						*/
				} else if (panel == whisker_m->whisker_ui.main_panel_handle &&
						 	control == MainPanel_IO_LickDetection) {
						de_dev->IO_Channel[LICK_DET_CH] = index;
				}
				
				break;
	}
	
	return 0;
}

int  CVICALLBACK 
FileSelect_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	WhiskerSound_t	*sound = NULL;
	int				ret = 0;
	
	sound = &(whisker_m->sound);
	
	switch (event) {
		case EVENT_COMMIT:
			ret = FileSelectPopup ("", "*.wav", "", "Select a File",
                                        VAL_LOAD_BUTTON, 0, 0, 1, 0, sound->file_path);
            if (ret) {
            	SetCtrlVal (panel, MainPanel_SoundFilePath, sound->file_path);
				sound->VALID_FILE = TRUE;
			}
			break;
	}
	
	return 0;
}

int  CVICALLBACK 
WhiskerCheck_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	WhiskerUI_t		*whisker_ui = NULL;
	delib_device_t	*de_dev = NULL;
	int				isSelected = 0;
	
	de_dev = &(whisker_m->de_dev);
	whisker_ui = &(whisker_m->whisker_ui);
	
	switch (event) {
			
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &isSelected);
			LOG_MSG1(9, "Check Box is changed %d", isSelected);
			
			if (panel == whisker_m->whisker_ui.main_panel_handle && 
						 	control == MainPanel_CheckSync) {
					whisker_m->sound.isSYNC = isSelected;
			} else if (panel == whisker_m->whisker_ui.tab_drop_in &&
					 control == TabDropIn_Check_DropIN) {
					
					/* Dim your duration operation */
					SetCtrlAttribute(panel, TabDropIn_Num_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabDropIn_Toggle_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabDropIn_Inter_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabDropIn_Toggle_Inter_DropIN, ATTR_DIMMED, isSelected);
					
						
					/* Dim continous and duration operation of other tabs */
					SetTabPageAttribute(whisker_ui->main_panel_handle, MainPanel_IOChannelTab, 
																TAB_DROP_OUT, ATTR_DIMMED, isSelected);
					SetTabPageAttribute(whisker_ui->main_panel_handle, MainPanel_IOChannelTab, 
																TAB_AIR_PUFF, ATTR_DIMMED, isSelected);
					
					/* Perform operation */
					set_without_timer(de_dev, de_dev->IO_Channel[DROP_IN_CH], isSelected);
			} else if (panel == whisker_m->whisker_ui.tab_drop_out && 
					 control == TabDropOut_Check_DropOut) {
				
					/* Dim your duration operation */
					SetCtrlAttribute(panel, TabDropOut_Num_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabDropOut_Toggle_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabDropOut_Inter_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabDropOut_Toggle_Inter_DropOut, ATTR_DIMMED, isSelected);
					
					/* Dim continous and duration operation of other tab */
					SetTabPageAttribute(whisker_ui->main_panel_handle, MainPanel_IOChannelTab, 
																TAB_DROP_IN, ATTR_DIMMED, isSelected);
					SetTabPageAttribute(whisker_ui->main_panel_handle, MainPanel_IOChannelTab, 
																TAB_AIR_PUFF, ATTR_DIMMED, isSelected);
					
					/* Perform operation */
					set_without_timer(de_dev, de_dev->IO_Channel[DROP_OUT_CH], isSelected);
			} else if (panel == whisker_m->whisker_ui.tab_air_puff &&
					 control == TabAirPuff_Check_AirPuff) {
		
					/* Dim your duration operation */
					SetCtrlAttribute(panel, TabAirPuff_Num_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabAirPuff_Toggle_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabAirPuff_Inter_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, TabAirPuff_Toggle_Inter_AirPuff, ATTR_DIMMED, isSelected);
					
					
					/* Dim continous and duration operation of other tab */
					SetTabPageAttribute(whisker_ui->main_panel_handle, MainPanel_IOChannelTab, 
																TAB_DROP_IN, ATTR_DIMMED, isSelected);
					SetTabPageAttribute(whisker_ui->main_panel_handle, MainPanel_IOChannelTab, 
																TAB_DROP_OUT, ATTR_DIMMED, isSelected);
					
					/* Perform operation */
					set_without_timer(de_dev, de_dev->IO_Channel[AIR_PUFF_CH], isSelected);
			}
			
			break;
	}
	
	return 0;
		
}

int  CVICALLBACK 
WhiskerToggle_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	delib_device_t	*de_dev = NULL;
	size_t			time_msec = 0, interval = 0;
	int				ch = 0;
	
	de_dev = &(whisker_m->de_dev);
			 
	switch (event) {
		case EVENT_COMMIT:
			
			if (panel == whisker_m->whisker_ui.tab_drop_in) {
				
					ch = de_dev->IO_Channel[DROP_IN_CH];	
					GetCtrlVal(panel, TabDropIn_Num_DropIN, &time_msec); /* Duration */
					
					if (control == TabDropIn_Toggle_DropIN) {	/* Duration toggle button */
						interval = time_msec;
					} else if (time_msec > 0) {					/* Interval toggle button & time_msec has
																 * to be greater than zero */
						GetCtrlVal(panel, TabDropIn_Inter_DropIN, &interval);
						if (interval < time_msec) {
							interval = 0;
						}
					} 
			} else if (panel == whisker_m->whisker_ui.tab_drop_out) {
					
					ch = de_dev->IO_Channel[DROP_OUT_CH];
					/* Get the duration value */
					GetCtrlVal (panel, TabDropOut_Num_DropOut, &time_msec);
					
					if (control == TabDropOut_Toggle_DropOut) {
						interval = time_msec;
					} else if (time_msec > 0) {
						GetCtrlVal(panel, TabDropOut_Inter_DropOut, &interval);
						if (interval < time_msec) {
							interval = 0;
						}
					} 		
				
			} else if (panel == whisker_m->whisker_ui.tab_air_puff) {
				
					ch = de_dev->IO_Channel[AIR_PUFF_CH];
					/* Get the duration value */
					GetCtrlVal (panel, TabAirPuff_Num_AirPuff, &time_msec);
					
					if (control == TabAirPuff_Toggle_AirPuff) {			
						interval = time_msec;
					} else if (time_msec > 0) {
						GetCtrlVal(panel, TabAirPuff_Inter_AirPuff, &interval);
						if (interval < time_msec) {
							interval = 0;
						}
					} 	
			}
			
			while (interval > 0) {
				set_with_timer(de_dev, ch, ON, time_msec);
				interval -= time_msec;
			}
			SetCtrlVal(panel, control, 0);
			
			break;
	}
	
	return 0;
}

int  CVICALLBACK
XYButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	WhiskerUI_t		*whisker_ui = NULL;
	zaber_device_t	*z_dev = NULL;
	int				error	= 0;
	int				X_min, X_max;
	int				Y_min, Y_max;
	int				max_speed;
	
	whisker_ui = &(whisker_m->whisker_ui);
	z_dev = &(whisker_m->z_dev);
	
	switch (event) {
		case EVENT_COMMIT:
			
			switch (control) {
				case MainPanel_XYHome:
					send_cmd(whisker_m->z_dev.port, "/home", NULL, SYNC);
				
				case MainPanel_XYUpdatePos:
					/* Get abolute postion and update UI */
					update_xy_positions(z_dev, panel);
					break;
					
				case MainPanel_XYSetting:
					/* Load Setting Panel */
					errChk(whisker_ui->XYSetting_panel_handle = LoadPanel(panel, MOD_Whisker_UI, XYSetPanel));
			
					/* Add call back to each UI componant */
					SetCtrlAttribute (whisker_ui->XYSetting_panel_handle, XYSetPanel_XYApply, 
							  		ATTR_CALLBACK_DATA, (void *)whisker_m);
					SetCtrlAttribute (whisker_ui->XYSetting_panel_handle, XYSetPanel_XYCancel, 
							  		ATTR_CALLBACK_DATA, (void *)whisker_m);
			
					/* Query zaber device and populate values */
					X_min = get_device_data(whisker_m->z_dev.port, ZABER_X_DEV, "limit.min");
					X_max = get_device_data(whisker_m->z_dev.port, ZABER_X_DEV, "limit.max");
					Y_min = get_device_data(whisker_m->z_dev.port, ZABER_Y_DEV, "limit.min");
					Y_max = get_device_data(whisker_m->z_dev.port, ZABER_Y_DEV, "limit.max");
					max_speed = get_device_data(whisker_m->z_dev.port, ZABER_DEV, "maxspeed");
			
					SetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_XMin, X_min);
					SetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_XMax, X_max);
					SetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_YMin, Y_min);
					SetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_YMax, Y_max);
					SetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_XYSpeed, max_speed);
			
					/* Display Panel */
					DisplayPanel(whisker_ui->XYSetting_panel_handle);
					break;
			}
			break;
	}

	return 0;
	
Error:
	LOG_MSG(1, "Error Occurred while loading XY setting Panel");
	return errorInfo.error;
}

int  CVICALLBACK 
WhiskerLickToggle_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t	*whisker_m = (Whisker_t *)callbackData;
	int			input_value;
	
	switch (event) {
		case EVENT_TIMER_TICK:
			input_value = get_input_value(&whisker_m->de_dev, 
											whisker_m->de_dev.IO_Channel[LICK_DET_CH]);
			LOG_MSG1(9, "Lick Detection : %d", input_value);
	 		if (input_value == 0) {	/* Lick Detected */
				SetCtrlAttribute (panel, MainPanel_LickDetection, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal(panel, MainPanel_LickDetection, 1);
				break;
	 		} else {
				SetCtrlVal(panel, MainPanel_LickDetection, 0);
			}
			break;
	}
	return 0;
}


/**************************************************************
 * XY Settings Panel Callback 								  *
 **************************************************************/

int  CVICALLBACK 
XYSettings_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t	*whisker_m = (Whisker_t *)callbackData;
	WhiskerUI_t		*whisker_ui = NULL;
	int				X_min, X_max;
	int				Y_min, Y_max;
	int				max_speed;
	
	whisker_ui = &(whisker_m->whisker_ui);
	
	switch (event) {
			case EVENT_COMMIT:
				
				switch (control) {
					case XYSetPanel_XYApply:
						/* Get attribute values */
						GetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_XMin, &X_min);
						GetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_XMax, &X_max);
						GetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_YMin, &Y_min);
						GetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_YMax, &Y_max);
						GetCtrlVal(whisker_ui->XYSetting_panel_handle, XYSetPanel_XYSpeed, &max_speed);
						
						/* TODO: Perform validation on maximum allowed values */
						
						
						/* Device settings */
						set_device_data(whisker_m->z_dev.port, ZABER_X_DEV, "limit.min", X_min);
						set_device_data(whisker_m->z_dev.port, ZABER_X_DEV, "limit.max", X_max);
						set_device_data(whisker_m->z_dev.port, ZABER_Y_DEV, "limit.min", Y_min);
						set_device_data(whisker_m->z_dev.port, ZABER_Y_DEV, "limit.max", Y_max);
						set_device_data(whisker_m->z_dev.port, ZABER_DEV, "maxspeed", max_speed);
						
					case XYSetPanel_XYCancel:
						/* Do not do anything */
						/* Perhaps set UI control values to default */
						DiscardPanel(whisker_ui->XYSetting_panel_handle);
						whisker_ui->XYSetting_panel_handle = 0;
						break;
				}
			break;
	}
	
	return 0;
}

/****************************************************************
 * Experiment Info Panel Callback.								*
 ****************************************************************/
int  CVICALLBACK 
ExperimentInfo_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t	*whisker_m = (Whisker_t *)callbackData;
	
	switch (event) {
		case EVENT_COMMIT:
			switch (control) {
				case ExpPanel_ExpOk:
					/* Get Information from Panel and save it to stucture */
					GetCtrlVal(panel, ExpPanel_ExpName, whisker_m->exp_info.user_name);
					GetCtrlVal(panel, ExpPanel_ExpNum, whisker_m->exp_info.exp_num);
					GetCtrlVal(panel, ExpPanel_ExpMsg, 	whisker_m->exp_info.extra_msg);
					GetCtrlVal(panel, ExpPanel_ExpTrainNum, whisker_m->exp_info.training_num);
					GetCtrlVal(panel, ExpPanel_ExpAniNum, whisker_m->exp_info.animal_num);
					GetCtrlVal(panel, ExpPanel_ExpAniAge, whisker_m->exp_info.animal_age);
					GetCtrlVal(panel, ExpPanel_ExpAniWeight, &(whisker_m->exp_info.animal_weight));
					
					whisker_m->exp_info.VALID_INFO = TRUE;
					
					/* Update Experiment Table view */
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 1), whisker_m->exp_info.user_name);
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 2), whisker_m->exp_info.exp_num);
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 3), whisker_m->exp_info.training_num);
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 4), whisker_m->exp_info.animal_num);
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 5), whisker_m->exp_info.animal_age);
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 6), whisker_m->exp_info.animal_weight);
					SetTableCellVal (whisker_m->whisker_ui.main_panel_handle, MainPanel_ExperimentInfoTable, 
							MakePoint(EXPTABLE_VALUE_COL, 7), whisker_m->exp_info.extra_msg);
					
				case ExpPanel_ExpCancel:
					DiscardPanel(whisker_m->whisker_ui.experiment_panel_handle);
					whisker_m->whisker_ui.experiment_panel_handle = 0;
					break;
			}
			break;
	}
	return 0;
}
