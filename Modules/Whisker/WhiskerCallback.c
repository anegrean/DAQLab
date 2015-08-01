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
XYAbs_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	char			com_port[6] = { 0 };
	
	z_dev = &(whisker_m->z_dev);
	switch (event) {
		case EVENT_COMMIT:
			if (z_dev->VALID_DEVICE == FALSE) {
				return -1;
			}
			
			/* Get Abs position */
			GetCtrlVal(panel, MainPanel_XYAbs, &(z_dev->absolute_position));
			if (z_dev->absolute_position == 0) {	/* TODO: Value should be minimum and maximum allowed */
				printf("Value should be greate than zero"); /* TODO: Alert dialog box */
			}
			
			/* Send Command */ /* TODO: This should be in seperate thread */
			send_MoveABS_cmd(z_dev->port, z_dev->absolute_position);
			break;
	}
	
	return 0;
	
}

int CVICALLBACK 
XYRightLeft_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Whisker_t		*whisker_m = (Whisker_t *)callbackData;
	zaber_device_t	*z_dev = NULL;
	int				position = 0;
	
	z_dev = &(whisker_m->z_dev);
	switch (event) {
			 case EVENT_COMMIT:
				 GetCtrlVal(panel, control, &position);
				 if (position == 0) {		/* TODO: Check has to be between valid expected values */
					 printf("Cannot have relative position zero");  /* TODO: Alert dialog box */
					 return -1;
				 }
				 
				 send_MoveRel_cmd(z_dev->port, 2 /* Device number */, position);
				 break;
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
					 printf("Cannot have relative position zero");  /* TODO: Alert dialog box */
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
						 SetCtrlVal(panel, MainPanel_LED_Port, 1);
						 break;
						 
					case MainPanel_PortClose:
						close_zaber_device(z_dev);
						close_deditec_device(de_dev);
						
						SetCtrlAttribute(panel, MainPanel_PortOpen, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_PortClose, ATTR_DIMMED, 1);
						SetCtrlVal(panel, MainPanel_LED_Port, 0);
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
				
				switch(control) {
					case MainPanel_IO_AirPuff:
						de_dev->IO_Channel[AIR_PUFF_CH] = index;
						/* Undim operation for this channel */
						SetCtrlAttribute(panel, MainPanel_Check_AirPuff, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Num_AirPuff, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Toggle_AirPuff, ATTR_DIMMED, 0);
						break;
						
					case MainPanel_IO_DropIN:
						de_dev->IO_Channel[DROP_IN_CH] = index;
						
						/* Undim operation for this channel */
						SetCtrlAttribute(panel, MainPanel_Check_DropIN, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Num_DropIN, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Toggle_DropIN, ATTR_DIMMED, 0);
						break;
						
					case MainPanel_IO_DropOut:
						de_dev->IO_Channel[DROP_OUT_CH] = index;
						/* Undim operation for this channel */
						SetCtrlAttribute(panel, MainPanel_Check_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Num_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Toggle_DropOut, ATTR_DIMMED, 0);
						break;
						
					case MainPanel_IO_ZAxis:
						de_dev->IO_Channel[ZAXIS_MOVE_CH] = index;
						/* Undim operation for this channel */
						/* This should be decided once we decide about rod movement.
						SetCtrlAttribute(panel, MainPanel_Check_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Num_DropOut, ATTR_DIMMED, 0);
						SetCtrlAttribute(panel, MainPanel_Toggle_DropOut, ATTR_DIMMED, 0);
						*/
						break;
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
	delib_device_t	*de_dev = NULL;
	int				isSelected = 0;
	
	de_dev = &(whisker_m->de_dev);
	
	switch (event) {
			
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &isSelected);
			LOG_MSG1(9, "Check Box is changed %d", isSelected);
			
			switch (control) {
				case MainPanel_CheckSync:
					whisker_m->sound.isSYNC = isSelected;
					break;
					
				case MainPanel_Check_DropIN:
					
					/* Dim your duration operation */
					SetCtrlAttribute(panel, MainPanel_Num_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_DropIN, ATTR_DIMMED, isSelected);
						
					/* Dim continous and duration operation of other commands */
					SetCtrlAttribute(panel, MainPanel_Check_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Num_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_DropOut, ATTR_DIMMED, isSelected);
						
					SetCtrlAttribute(panel, MainPanel_Check_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Num_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_AirPuff, ATTR_DIMMED, isSelected);
					
					/* Perform operation */
					set_without_timer(de_dev, de_dev->IO_Channel[DROP_IN_CH], isSelected);
					
					break;
					
				case MainPanel_Check_DropOut:
					
					/* Dim your duration operation */
					SetCtrlAttribute(panel, MainPanel_Num_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_DropOut, ATTR_DIMMED, isSelected);
					
					/* Dim continous and duration operation of other commands */
					SetCtrlAttribute(panel, MainPanel_Check_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Num_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_DropIN, ATTR_DIMMED, isSelected);
						
					SetCtrlAttribute(panel, MainPanel_Check_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Num_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_AirPuff, ATTR_DIMMED, isSelected);
					
					/* Perform operation */
					set_without_timer(de_dev, de_dev->IO_Channel[DROP_OUT_CH], isSelected);
					
					break;
					
				case MainPanel_Check_AirPuff:
					
					/* Dim your duration operation */
					SetCtrlAttribute(panel, MainPanel_Num_AirPuff, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_AirPuff, ATTR_DIMMED, isSelected);
					
					/* Dim continous and duration operation of other commands */
					SetCtrlAttribute(panel, MainPanel_Check_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Num_DropIN, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_DropIN, ATTR_DIMMED, isSelected);
						
					SetCtrlAttribute(panel, MainPanel_Check_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Num_DropOut, ATTR_DIMMED, isSelected);
					SetCtrlAttribute(panel, MainPanel_Toggle_DropOut, ATTR_DIMMED, isSelected);
					
					/* Perform operation */
					set_without_timer(de_dev, de_dev->IO_Channel[AIR_PUFF_CH], isSelected);
					
					break;
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
	unsigned int	time_msec = 0;
	
	de_dev = &(whisker_m->de_dev);
			 
	switch (event) {
		case EVENT_COMMIT:
			
			switch (control) {
				case MainPanel_Toggle_DropIN:
					
					/* Get the duration value */
					GetCtrlVal (panel, MainPanel_Num_DropIN, &time_msec);
					set_with_timer(de_dev, de_dev->IO_Channel[DROP_IN_CH], ON, time_msec);
					
					SetCtrlVal(panel, control, 0);
					break;
					
				case MainPanel_Toggle_DropOut:
					
					/* Get the duration value */
					GetCtrlVal (panel, MainPanel_Num_DropOut, &time_msec);
					set_with_timer(de_dev, de_dev->IO_Channel[DROP_OUT_CH], ON, time_msec);
					
					SetCtrlVal(panel, control, 0);
					break;
					
				case MainPanel_Toggle_AirPuff:
					
					/* Get the duration value */
					GetCtrlVal (panel, MainPanel_Num_AirPuff, &time_msec);
					set_with_timer(de_dev, de_dev->IO_Channel[AIR_PUFF_CH], ON, time_msec);
					
					SetCtrlVal(panel, control, 0);
					break;
			}
			
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
	
	whisker_ui = &(whisker_m->whisker_ui);
	
	switch (event) {
			case EVENT_COMMIT:
				
				switch (control) {
					case XYSetPanel_XYApply:
						/* Get attribute values */
						/* Set them into local structure and set them into device configuration */
						
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
