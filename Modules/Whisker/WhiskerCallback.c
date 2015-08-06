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
			/* This stops only existing module. But fail to remove it from global list.
			 * Thus, we need a way to remove this module from module list.
			 */
			discard_WhiskerModule((DAQLabModule_type **)&whisker_m);
			break;
	}
	return 0;
}

int CVICALLBACK 
XYMovement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	char			com_port[6] = { 0 };
	int				position = 0;
	uint32_t		abs_pos, x_pos, y_pos;
	
	z_dev = &(whisker_m->z_dev);
	
	/* If connection is not opened then do not perform any operation */
	if (z_dev->VALID_DEVICE == FALSE) {
		return -1;
	}
	
	switch (event) {
		case EVENT_COMMIT:
			
			switch(control) {
				case MainPanel_XYAbs:
					/* Get Abs position */
					GetCtrlVal(panel, MainPanel_XYAbs, &(z_dev->absolute_position));
					if (z_dev->absolute_position == 0) {	/* TODO: Value should be minimum and maximum allowed */
						LOG_MSG(9, "Value should be greate than zero"); /* TODO: Alert dialog box */
					}
			
					/* Send Command */ /* TODO: This should be in seperate thread */
					send_MoveABS_cmd(z_dev->port, z_dev->absolute_position);
					break;
					
				case MainPanel_XYLeft:
					GetCtrlVal(panel, control, &position);
					/* TODO: Position has to within limit */
					send_MoveRel_cmd(z_dev->port, ZABER_X_DEV, position); 
					break;
					
				case MainPanel_XYRight:
					GetCtrlVal(panel, control, &position);
					/* TODO: Position has be within limit */
					send_MoveRel_cmd(z_dev->port, ZABER_Y_DEV, position); 
					break;
				
				case MainPanel_XYArrowLeft:
				case MainPanel_XYArrowRight:
				case MainPanel_XYArrowUp:
				case MainPanel_XYArrowDown:
					GetCtrlVal(panel, MainPanel_XYStepNum, &position);
					
					if (control == MainPanel_XYArrowLeft || 
							control == MainPanel_XYArrowDown) {
								position = -position;
					}
					
					if (control == MainPanel_XYArrowLeft || 
							control == MainPanel_XYArrowRight) {
						send_MoveRel_cmd(z_dev->port, ZABER_X_DEV, position);
					} else {
						send_MoveRel_cmd(z_dev->port, ZABER_Y_DEV, position);
					}
					
					/* Once operation is done, it is good to update UI.
					 * Doing callback won't cause delay and we other operation
					 * on the same device will be prevented
					 */
					abs_pos = get_device_data(z_dev->port, ZABER_DEV, "pos");
					x_pos = get_device_data(z_dev->port, ZABER_X_DEV, "pos");
					y_pos = get_device_data(z_dev->port, ZABER_Y_DEV, "pos");
					
					SetCtrlVal(panel, MainPanel_XYAbs, abs_pos);
					SetCtrlVal(panel, MainPanel_XYLeft, x_pos);
					SetCtrlVal(panel, MainPanel_XYRight, y_pos);
					
					SetCtrlVal(panel, control, 0);
					break;
			}
			
	}
	
	return 0;
	
}

int CVICALLBACK 
ZUpDown_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	int				position = 0;
	z_dev = &(whisker_m->z_dev);
	
	switch (event) {
			 case EVENT_COMMIT:
				 GetCtrlVal(panel, control, &position);
				 if (position == 0) {		/* TODO: Check has to be between valid expected values */
					 MessagePopup("Error Msg", "Cannot have relative position zero");  /* TODO: Alert dialog box */
					 return -1;
				 }
				 
				 send_MoveRel_cmd(z_dev->port, 1 /* Device number */, position);
				 break;
	}
	
	return 0;
}

/*
int  CVICALLBACK 
Deditec_Group_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	delib_device_t	*de_dev = NULL;
	int				button;
	static int		selected_button = -1; 
	de_dev = &whisker_m->de_dev;
	
	switch(event) {
		case EVENT_MARK_STATE_CHANGE:
			Radio_GetMarkedOption(panel, MainPanel_Deditec_Group, &button);
			
			if (selected_button != button) {
				if (button == 3) {
					set_with_timer(de_dev, AIR_PUFF_CH + button, ON, 15000);
				} else {
					 set_with_timer(de_dev, AIR_PUFF_CH + button, ON, 500);
				}
				selected_button = button;
			}
			break;
	}
	
	return 0;
}
*/

int  CVICALLBACK
WhiskerButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	delib_device_t	*de_dev = NULL;
	int				index;
	int				ret = 0;
	
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
						 break;
						 
					case MainPanel_PortClose:
						close_zaber_device(z_dev);
						close_deditec_device(de_dev);
						
						SetCtrlAttribute(panel, MainPanel_PortOpen, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_PortClose, ATTR_DIMMED, 1);
						SetCtrlVal(panel, MainPanel_LED_Port, 0);
						SetCtrlAttribute(panel, MainPanel_XYSetting, ATTR_DIMMED, 1); /* XY setting */
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

int  CVICALLBACK WhiskerToggle_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	delib_device_t	*de_dev = NULL;
	int				isSelected = 0;
	unsigned int	time_msec = 0, interval = 0;
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
XYSettingButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	WhiskerUI_t		*whisker_ui = NULL;
	int				error	= 0;
	int				X_min, X_max;
	int				Y_min, Y_max;
	int				max_speed;
	
	whisker_ui = &(whisker_m->whisker_ui);
	
	switch (event) {
		case EVENT_COMMIT:
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

	return 0;
	
Error:
	LOG_MSG(1, "Error Occurred while loading XY setting Panel");
	return error;
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
