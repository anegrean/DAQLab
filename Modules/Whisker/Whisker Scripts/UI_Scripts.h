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

#define  ActionEle                        1
#define  ActionEle_SPLITTER               2       /* control type: splitter, callback function: (none) */
#define  ActionEle_EleName                3       /* control type: textMsg, callback function: (none) */
#define  ActionEle_EleDuration            4       /* control type: numeric, callback function: (none) */
#define  ActionEle_EleDelete              5       /* control type: pictButton, callback function: ActionElementButtons_CB */
#define  ActionEle_EleNum                 6       /* control type: textMsg, callback function: (none) */
#define  ActionEle_EleApply               7       /* control type: pictButton, callback function: ActionElementButtons_CB */
#define  ActionEle_LED                    8       /* control type: LED, callback function: (none) */
#define  ActionEle_EleIO_CH_2             9       /* control type: ring, callback function: (none) */
#define  ActionEle_EleIO_CH               10      /* control type: ring, callback function: (none) */
#define  ActionEle_EleCommand             11      /* control type: ring, callback function: (none) */

#define  CondEle                          2
#define  CondEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  CondEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  CondEle_EleFalse                 4       /* control type: numeric, callback function: (none) */
#define  CondEle_EleDelete                5       /* control type: pictButton, callback function: CondElementButtons_CB */
#define  CondEle_EleTrue                  6       /* control type: numeric, callback function: (none) */
#define  CondEle_EleApply                 7       /* control type: pictButton, callback function: CondElementButtons_CB */
#define  CondEle_LED                      8       /* control type: LED, callback function: (none) */
#define  CondEle_EleNum                   9       /* control type: textMsg, callback function: (none) */
#define  CondEle_EleIO_CH                 10      /* control type: ring, callback function: (none) */
#define  CondEle_EleValue                 11      /* control type: ring, callback function: (none) */
#define  CondEle_EleDuration              12      /* control type: numeric, callback function: (none) */

#define  ContainPan                       3

#define  MsgEle                           4
#define  MsgEle_SPLITTER                  2       /* control type: splitter, callback function: (none) */
#define  MsgEle_EleName                   3       /* control type: textMsg, callback function: (none) */
#define  MsgEle_EleNum                    4       /* control type: textMsg, callback function: (none) */
#define  MsgEle_EleDelete                 5       /* control type: pictButton, callback function: MsgElementButtons_CB */
#define  MsgEle_EleApply                  6       /* control type: pictButton, callback function: MsgElementButtons_CB */
#define  MsgEle_LED                       7       /* control type: LED, callback function: (none) */
#define  MsgEle_EleText                   8       /* control type: string, callback function: (none) */

#define  RepEle                           5
#define  RepEle_SPLITTER                  2       /* control type: splitter, callback function: (none) */
#define  RepEle_EleName                   3       /* control type: textMsg, callback function: (none) */
#define  RepEle_EleStep                   4       /* control type: numeric, callback function: (none) */
#define  RepEle_EleNum                    5       /* control type: textMsg, callback function: (none) */
#define  RepEle_EleDelete                 6       /* control type: pictButton, callback function: RepeatElementButtons_CB */
#define  RepEle_EleApply                  7       /* control type: pictButton, callback function: RepeatElementButtons_CB */
#define  RepEle_LED                       8       /* control type: LED, callback function: (none) */
#define  RepEle_EleNTimes                 9       /* control type: numeric, callback function: (none) */

#define  ScriptPan                        6
#define  ScriptPan_ScriptLog              2       /* control type: textBox, callback function: (none) */
#define  ScriptPan_ScriptImportSetting    3       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptQuit             4       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptDelete           5       /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptSave             6       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptLoad             7       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptNew              8       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptElement          9       /* control type: ring, callback function: (none) */
#define  ScriptPan_ScriptAdd              10      /* control type: command, callback function: ScriptAddElement_CB */
#define  ScriptPan_SPLITTER_2             11      /* control type: splitter, callback function: (none) */
#define  ScriptPan_SPLITTER_3             12      /* control type: splitter, callback function: (none) */
#define  ScriptPan_SPLITTER               13      /* control type: splitter, callback function: (none) */
#define  ScriptPan_NewScript              14      /* control type: textMsg, callback function: (none) */
#define  ScriptPan_LogCheck               15      /* control type: radioButton, callback function: (none) */
#define  ScriptPan_ScriptPause            16      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptStop             17      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptRun              18      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptName             19      /* control type: textMsg, callback function: (none) */
#define  ScriptPan_LickGraph              20      /* control type: strip, callback function: (none) */
#define  ScriptPan_LogFile                21      /* control type: textMsg, callback function: (none) */

#define  SoundEle                         7
#define  SoundEle_SPLITTER                2       /* control type: splitter, callback function: (none) */
#define  SoundEle_EleName                 3       /* control type: textMsg, callback function: (none) */
#define  SoundEle_EleNum                  4       /* control type: textMsg, callback function: (none) */
#define  SoundEle_EleDelete               5       /* control type: pictButton, callback function: SoundElementButtons_CB */
#define  SoundEle_EleApply                6       /* control type: pictButton, callback function: SoundElementButtons_CB */
#define  SoundEle_LED                     7       /* control type: LED, callback function: (none) */
#define  SoundEle_ElePlay                 8       /* control type: command, callback function: SoundElementButtons_CB */
#define  SoundEle_EleLoad                 9       /* control type: command, callback function: SoundElementButtons_CB */
#define  SoundEle_ElePath                 10      /* control type: string, callback function: (none) */

#define  StartEle                         8
#define  StartEle_SPLITTER                2       /* control type: splitter, callback function: (none) */
#define  StartEle_EleName                 3       /* control type: textMsg, callback function: (none) */
#define  StartEle_EleDelay                4       /* control type: numeric, callback function: (none) */
#define  StartEle_EleNum                  5       /* control type: textMsg, callback function: (none) */
#define  StartEle_EleDelete               6       /* control type: pictButton, callback function: StartElementButtons_CB */
#define  StartEle_EleApply                7       /* control type: pictButton, callback function: StartElementButtons_CB */
#define  StartEle_LED                     8       /* control type: LED, callback function: (none) */

#define  StopEle                          9
#define  StopEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  StopEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  StopEle_EleDelay                 4       /* control type: numeric, callback function: (none) */
#define  StopEle_EleNum                   5       /* control type: textMsg, callback function: (none) */
#define  StopEle_EleDelete                6       /* control type: pictButton, callback function: StopElementButtons_CB */
#define  StopEle_EleApply                 7       /* control type: pictButton, callback function: StopElementButtons_CB */
#define  StopEle_LED                      8       /* control type: LED, callback function: (none) */

#define  WaitEle                          10
#define  WaitEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  WaitEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  WaitEle_EleDelay                 4       /* control type: numeric, callback function: (none) */
#define  WaitEle_EleNum                   5       /* control type: textMsg, callback function: (none) */
#define  WaitEle_EleDelete                6       /* control type: pictButton, callback function: WaitElementButtons_CB */
#define  WaitEle_EleApply                 7       /* control type: pictButton, callback function: WaitElementButtons_CB */
#define  WaitEle_LED                      8       /* control type: LED, callback function: (none) */

#define  XYMoveEle                        11
#define  XYMoveEle_SPLITTER               2       /* control type: splitter, callback function: (none) */
#define  XYMoveEle_EleName                3       /* control type: textMsg, callback function: (none) */
#define  XYMoveEle_EleNum                 4       /* control type: textMsg, callback function: (none) */
#define  XYMoveEle_EleDelete              5       /* control type: pictButton, callback function: XYMoveElementButtons_CB */
#define  XYMoveEle_EleApply               6       /* control type: pictButton, callback function: XYMoveElementButtons_CB */
#define  XYMoveEle_LED                    7       /* control type: LED, callback function: (none) */
#define  XYMoveEle_ElePercentage          8       /* control type: numeric, callback function: (none) */
#define  XYMoveEle_ElePos                 9       /* control type: numeric, callback function: (none) */
#define  XYMoveEle_EleShow                10      /* control type: command, callback function: XYMoveElementButtons_CB */
#define  XYMoveEle_EleAdd                 11      /* control type: command, callback function: XYMoveElementButtons_CB */

#define  XYPosPan                         12
#define  XYPosPan_XYPositionsTable        2       /* control type: table, callback function: XYPosTable_CB */
#define  XYPosPan_XYPositionOk            3       /* control type: command, callback function: XYPosTable_CB */

#define  ZMoveEle                         13
#define  ZMoveEle_SPLITTER                2       /* control type: splitter, callback function: (none) */
#define  ZMoveEle_EleName                 3       /* control type: textMsg, callback function: (none) */
#define  ZMoveEle_EleNum                  4       /* control type: textMsg, callback function: (none) */
#define  ZMoveEle_EleDelete               5       /* control type: pictButton, callback function: ZMoveElementButtons_CB */
#define  ZMoveEle_EleApply                6       /* control type: pictButton, callback function: ZMoveElementButtons_CB */
#define  ZMoveEle_LED                     7       /* control type: LED, callback function: (none) */
#define  ZMoveEle_EleIO_CH                8       /* control type: ring, callback function: (none) */
#define  ZMoveEle_EleCommand              9       /* control type: ring, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK ActionElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CondElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK MsgElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK RepeatElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ScriptAddElement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK SoundElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK StartElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK StopElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WaitElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerScriptButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYMoveElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYPosTable_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ZMoveElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
