/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/*                                                                        */
/* WARNING: Do not add to, delete from, or otherwise modify the contents  */
/*          of this include file.                                         */
/**************************************************************************/

#include <userint.h>

#ifdef __cplusplus
    extern "C" {
#endif

     /* Panels and Controls: */

#define  MainPanel                        1
#define  MainPanel_ZaberComPort           2       /* control type: ring, callback function: (none) */
#define  MainPanel_DeditecModule          3       /* control type: ring, callback function: (none) */
#define  MainPanel_SPLITTER_4             4       /* control type: splitter, callback function: (none) */
#define  MainPanel_SPLITTER_3             5       /* control type: splitter, callback function: (none) */
#define  MainPanel_SPLITTER_2             6       /* control type: splitter, callback function: (none) */
#define  MainPanel_Text_DropIN            7       /* control type: textMsg, callback function: (none) */
#define  MainPanel_Check_AirPuff          8       /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  MainPanel_Check_DropOut          9       /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  MainPanel_Num_AirPuff            10      /* control type: numeric, callback function: (none) */
#define  MainPanel_Num_DropIN             11      /* control type: numeric, callback function: (none) */
#define  MainPanel_Num_DropOut            12      /* control type: numeric, callback function: (none) */
#define  MainPanel_CheckSync              13      /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  MainPanel_Check_DropIN           14      /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  MainPanel_Text_DropIN_2          15      /* control type: textMsg, callback function: (none) */
#define  MainPanel_Text_DropIN_3          16      /* control type: textMsg, callback function: (none) */
#define  MainPanel_PortSetting            17      /* control type: textMsg, callback function: (none) */
#define  MainPanel_Text_DropIN_4          18      /* control type: textMsg, callback function: (none) */
#define  MainPanel_Text_DropIN_5          19      /* control type: textMsg, callback function: (none) */
#define  MainPanel_Experiment             20      /* control type: textMsg, callback function: (none) */
#define  MainPanel_DECORATION_5           21      /* control type: deco, callback function: (none) */
#define  MainPanel_QUITBUTTON             22      /* control type: command, callback function: QuitButton_CB */
#define  MainPanel_SaveLog                23      /* control type: command, callback function: (none) */
#define  MainPanel_LoadSound              24      /* control type: command, callback function: FileSelect_CB */
#define  MainPanel_LoadScript             25      /* control type: command, callback function: (none) */
#define  MainPanel_Log_File               26      /* control type: string, callback function: (none) */
#define  MainPanel_SoundFilePath          27      /* control type: string, callback function: (none) */
#define  MainPanel_Script_Name            28      /* control type: string, callback function: (none) */
#define  MainPanel_SoundPlay              29      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_LogSave                30      /* control type: command, callback function: (none) */
#define  MainPanel_Script_Stop            31      /* control type: command, callback function: (none) */
#define  MainPanel_BuildScript            32      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_Script_Restart         33      /* control type: command, callback function: (none) */
#define  MainPanel_Script_Pause           34      /* control type: command, callback function: (none) */
#define  MainPanel_Script_Start           35      /* control type: command, callback function: (none) */
#define  MainPanel_Script                 36      /* control type: textMsg, callback function: (none) */
#define  MainPanel_DECORATION_7           37      /* control type: deco, callback function: (none) */
#define  MainPanel_RunningTime            38      /* control type: string, callback function: (none) */
#define  MainPanel_LickDetection          39      /* control type: LED, callback function: (none) */
#define  MainPanel_LogFile                40      /* control type: textMsg, callback function: (none) */
#define  MainPanel_Sound                  41      /* control type: textMsg, callback function: (none) */
#define  MainPanel_DECORATION_9           42      /* control type: deco, callback function: (none) */
#define  MainPanel_XYLeft                 43      /* control type: numeric, callback function: XYRightLeft_CB */
#define  MainPanel_ZDown                  44      /* control type: numeric, callback function: ZUpDown_CB */
#define  MainPanel_ZUp                    45      /* control type: numeric, callback function: ZUpDown_CB */
#define  MainPanel_XYRight                46      /* control type: numeric, callback function: XYRightLeft_CB */
#define  MainPanel_LickGraph              47      /* control type: strip, callback function: (none) */
#define  MainPanel_DECORATION_3           48      /* control type: deco, callback function: (none) */
#define  MainPanel_DECORATION             49      /* control type: deco, callback function: (none) */
#define  MainPanel_DECORATION_4           50      /* control type: deco, callback function: (none) */
#define  MainPanel_XYAbs                  51      /* control type: numeric, callback function: XYAbs_CB */
#define  MainPanel_Toggle_AirPuff         52      /* control type: toggle, callback function: WhiskerToggle_CB */
#define  MainPanel_Toggle_DropOut         53      /* control type: toggle, callback function: WhiskerToggle_CB */
#define  MainPanel_DECORATION_8           54      /* control type: deco, callback function: (none) */
#define  MainPanel_Toggle_DropIN          55      /* control type: toggle, callback function: WhiskerToggle_CB */
#define  MainPanel_IO_AirPuff             56      /* control type: ring, callback function: IO_Channel_CB */
#define  MainPanel_IO_DropOut             57      /* control type: ring, callback function: IO_Channel_CB */
#define  MainPanel_IO_ZAxis               58      /* control type: ring, callback function: IO_Channel_CB */
#define  MainPanel_IO_DropIN              59      /* control type: ring, callback function: IO_Channel_CB */
#define  MainPanel_DECORATION_2           60      /* control type: deco, callback function: (none) */
#define  MainPanel_PortClose              61      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_XYSetting              62      /* control type: command, callback function: XYSettingButton_CB */
#define  MainPanel_PortOpen               63      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_LED_Port               64      /* control type: LED, callback function: (none) */

#define  XYSetPanel                       2
#define  XYSetPanel_YMax                  2       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_YMin                  3       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_XMax                  4       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_XMin                  5       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_X                     6       /* control type: textMsg, callback function: (none) */
#define  XYSetPanel_XYVelocity            7       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_SPLITTER              8       /* control type: splitter, callback function: (none) */
#define  XYSetPanel_XYCancel              9       /* control type: command, callback function: XYSettings_CB */
#define  XYSetPanel_XYApply               10      /* control type: command, callback function: XYSettings_CB */
#define  XYSetPanel_X_2                   11      /* control type: textMsg, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK FileSelect_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK IO_Channel_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK QuitButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerCheck_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerToggle_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYAbs_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYRightLeft_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYSettingButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYSettings_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ZUpDown_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
