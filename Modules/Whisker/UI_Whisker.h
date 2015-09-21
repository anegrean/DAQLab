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

#define  ExpPanel                         1
#define  ExpPanel_ExpMsg                  2       /* control type: textBox, callback function: (none) */
#define  ExpPanel_ExpName                 3       /* control type: string, callback function: (none) */
#define  ExpPanel_DECORATION              4       /* control type: deco, callback function: (none) */
#define  ExpPanel_ExpOk                   5       /* control type: command, callback function: ExperimentInfo_CB */
#define  ExpPanel_ExpCancel               6       /* control type: command, callback function: ExperimentInfo_CB */
#define  ExpPanel_ExpAniAge               7       /* control type: string, callback function: (none) */
#define  ExpPanel_ExpAniNum               8       /* control type: string, callback function: (none) */
#define  ExpPanel_ExpTrainNum             9       /* control type: string, callback function: (none) */
#define  ExpPanel_ExpNum                  10      /* control type: string, callback function: (none) */
#define  ExpPanel_ExpAniWeight            11      /* control type: numeric, callback function: (none) */
#define  ExpPanel_SPLITTER                12      /* control type: splitter, callback function: (none) */

#define  MainPanel                        2
#define  MainPanel_ExperimentInfoTable    2       /* control type: table, callback function: (none) */
#define  MainPanel_ZaberComPort           3       /* control type: ring, callback function: (none) */
#define  MainPanel_DeditecModule          4       /* control type: ring, callback function: (none) */
#define  MainPanel_CheckSync              5       /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  MainPanel_PortSetting            6       /* control type: textMsg, callback function: (none) */
#define  MainPanel_Text_DropIN_6          7       /* control type: textMsg, callback function: (none) */
#define  MainPanel_LickTimer              8       /* control type: timer, callback function: WhiskerLickToggle_CB */
#define  MainPanel_IO_LickDetection       9       /* control type: ring, callback function: IO_Channel_CB */
#define  MainPanel_IO_ZAxis               10      /* control type: ring, callback function: IO_Channel_CB */
#define  MainPanel_Text_DropIN_5          11      /* control type: textMsg, callback function: (none) */
#define  MainPanel_QUITBUTTON             12      /* control type: command, callback function: QuitButton_CB */
#define  MainPanel_LoadSound              13      /* control type: command, callback function: FileSelect_CB */
#define  MainPanel_SoundFilePath          14      /* control type: string, callback function: (none) */
#define  MainPanel_SoundPlay              15      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_ExperimentInfo         16      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_BuildScript            17      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_LickDetection          18      /* control type: LED, callback function: (none) */
#define  MainPanel_Sound                  19      /* control type: textMsg, callback function: (none) */
#define  MainPanel_XYLeft                 20      /* control type: numeric, callback function: XYMovement_CB */
#define  MainPanel_XYRight                21      /* control type: numeric, callback function: XYMovement_CB */
#define  MainPanel_DECORATION_10          22      /* control type: deco, callback function: (none) */
#define  MainPanel_DECORATION             23      /* control type: deco, callback function: (none) */
#define  MainPanel_XYAbs                  24      /* control type: numeric, callback function: XYMovement_CB */
#define  MainPanel_DECORATION_8           25      /* control type: deco, callback function: (none) */
#define  MainPanel_PortClose              26      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_XYUpdatePos            27      /* control type: command, callback function: XYButton_CB */
#define  MainPanel_XYHome                 28      /* control type: command, callback function: XYButton_CB */
#define  MainPanel_XYSetting              29      /* control type: command, callback function: XYButton_CB */
#define  MainPanel_PortOpen               30      /* control type: command, callback function: WhiskerButton_CB */
#define  MainPanel_LED_Port               31      /* control type: LED, callback function: (none) */
#define  MainPanel_ZArrowUp               32      /* control type: pictButton, callback function: ZMovement_CB */
#define  MainPanel_XYArrowUp              33      /* control type: pictButton, callback function: XYMovement_CB */
#define  MainPanel_XYArrowLeft            34      /* control type: pictButton, callback function: XYMovement_CB */
#define  MainPanel_XYArrowRight           35      /* control type: pictButton, callback function: XYMovement_CB */
#define  MainPanel_ZArrowDown             36      /* control type: pictButton, callback function: ZMovement_CB */
#define  MainPanel_XYAddRow               37      /* control type: pictButton, callback function: XYMovement_CB */
#define  MainPanel_XYArrowDown            38      /* control type: pictButton, callback function: XYMovement_CB */
#define  MainPanel_IOChannelTab           39      /* control type: tab, callback function: (none) */
#define  MainPanel_DECORATION_3           40      /* control type: deco, callback function: (none) */
#define  MainPanel_XYPositionsTable       41      /* control type: table, callback function: PositionTable_CB */
#define  MainPanel_TEXTMSG                42      /* control type: textMsg, callback function: (none) */
#define  MainPanel_TEXTMSG_2              43      /* control type: textMsg, callback function: (none) */

#define  XYSetPanel                       3
#define  XYSetPanel_YMax                  2       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_YMin                  3       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_XMax                  4       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_XMin                  5       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_X                     6       /* control type: textMsg, callback function: (none) */
#define  XYSetPanel_XYSpeed               7       /* control type: numeric, callback function: (none) */
#define  XYSetPanel_SPLITTER              8       /* control type: splitter, callback function: (none) */
#define  XYSetPanel_XYCancel              9       /* control type: command, callback function: XYSettings_CB */
#define  XYSetPanel_XYApply               10      /* control type: command, callback function: XYSettings_CB */
#define  XYSetPanel_X_2                   11      /* control type: textMsg, callback function: (none) */

     /* tab page panel controls */
#define  TabAirPuff_Check_AirPuff         2       /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  TabAirPuff_Inter_AirPuff         3       /* control type: numeric, callback function: (none) */
#define  TabAirPuff_Num_AirPuff           4       /* control type: numeric, callback function: (none) */
#define  TabAirPuff_Toggle_Inter_AirPuff  5       /* control type: toggle, callback function: WhiskerToggle_CB */
#define  TabAirPuff_Toggle_AirPuff        6       /* control type: toggle, callback function: WhiskerToggle_CB */
#define  TabAirPuff_IO_AirPuff            7       /* control type: ring, callback function: IO_Channel_CB */

     /* tab page panel controls */
#define  TabDropIn_Check_DropIN           2       /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  TabDropIn_Inter_DropIN           3       /* control type: numeric, callback function: (none) */
#define  TabDropIn_Num_DropIN             4       /* control type: numeric, callback function: (none) */
#define  TabDropIn_Toggle_Inter_DropIN    5       /* control type: toggle, callback function: WhiskerToggle_CB */
#define  TabDropIn_Toggle_DropIN          6       /* control type: toggle, callback function: WhiskerToggle_CB */
#define  TabDropIn_IO_DropIN              7       /* control type: ring, callback function: IO_Channel_CB */

     /* tab page panel controls */
#define  TabDropOut_Check_DropOut         2       /* control type: radioButton, callback function: WhiskerCheck_CB */
#define  TabDropOut_Inter_DropOut         3       /* control type: numeric, callback function: (none) */
#define  TabDropOut_Num_DropOut           4       /* control type: numeric, callback function: (none) */
#define  TabDropOut_Toggle_Inter_DropOut  5       /* control type: toggle, callback function: WhiskerToggle_CB */
#define  TabDropOut_Toggle_DropOut        6       /* control type: toggle, callback function: WhiskerToggle_CB */
#define  TabDropOut_IO_DropOut            7       /* control type: ring, callback function: IO_Channel_CB */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK ExperimentInfo_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK FileSelect_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK IO_Channel_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK PositionTable_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK QuitButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerCheck_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerLickToggle_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerToggle_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYMovement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYSettings_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ZMovement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
