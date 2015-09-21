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
#define  ActionEle_LED                    7       /* control type: LED, callback function: (none) */
#define  ActionEle_EleIO_CH_2             8       /* control type: ring, callback function: (none) */
#define  ActionEle_EleIO_CH               9       /* control type: ring, callback function: (none) */
#define  ActionEle_EleCommand             10      /* control type: ring, callback function: (none) */
#define  ActionEle_EleAsync               11      /* control type: radioButton, callback function: (none) */

#define  CondEle                          2
#define  CondEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  CondEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  CondEle_EleFalse                 4       /* control type: numeric, callback function: (none) */
#define  CondEle_EleDelete                5       /* control type: pictButton, callback function: CondElementButtons_CB */
#define  CondEle_EleTrue                  6       /* control type: numeric, callback function: (none) */
#define  CondEle_LED                      7       /* control type: LED, callback function: (none) */
#define  CondEle_EleNum                   8       /* control type: textMsg, callback function: (none) */
#define  CondEle_EleIO_CH                 9       /* control type: ring, callback function: (none) */
#define  CondEle_EleValue                 10      /* control type: ring, callback function: (none) */
#define  CondEle_EleFullDuration          11      /* control type: radioButton, callback function: (none) */
#define  CondEle_EleDuration              12      /* control type: numeric, callback function: (none) */

#define  ContainPan                       3

#define  DummyEle                         4

#define  JumpEle                          5
#define  JumpEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  JumpEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  JumpEle_EleStep                  4       /* control type: numeric, callback function: (none) */
#define  JumpEle_EleNum                   5       /* control type: textMsg, callback function: (none) */
#define  JumpEle_EleDelete                6       /* control type: pictButton, callback function: JumpElementButtons_CB */
#define  JumpEle_LED                      7       /* control type: LED, callback function: (none) */

#define  MsgEle                           6
#define  MsgEle_SPLITTER                  2       /* control type: splitter, callback function: (none) */
#define  MsgEle_EleName                   3       /* control type: textMsg, callback function: (none) */
#define  MsgEle_EleNum                    4       /* control type: textMsg, callback function: (none) */
#define  MsgEle_EleDelete                 5       /* control type: pictButton, callback function: MsgElementButtons_CB */
#define  MsgEle_LED                       6       /* control type: LED, callback function: (none) */
#define  MsgEle_EleText                   7       /* control type: string, callback function: (none) */

#define  RepEle                           7
#define  RepEle_SPLITTER                  2       /* control type: splitter, callback function: (none) */
#define  RepEle_EleName                   3       /* control type: textMsg, callback function: (none) */
#define  RepEle_EleStep                   4       /* control type: numeric, callback function: (none) */
#define  RepEle_EleNum                    5       /* control type: textMsg, callback function: (none) */
#define  RepEle_EleDelete                 6       /* control type: pictButton, callback function: RepeatElementButtons_CB */
#define  RepEle_LED                       7       /* control type: LED, callback function: (none) */
#define  RepEle_EleNTimes                 8       /* control type: numeric, callback function: (none) */

#define  ResRepEle                        8
#define  ResRepEle_SPLITTER               2       /* control type: splitter, callback function: (none) */
#define  ResRepEle_EleName                3       /* control type: textMsg, callback function: (none) */
#define  ResRepEle_EleStep                4       /* control type: numeric, callback function: (none) */
#define  ResRepEle_EleNum                 5       /* control type: textMsg, callback function: (none) */
#define  ResRepEle_EleDelete              6       /* control type: pictButton, callback function: ResRepElementButtons_CB */
#define  ResRepEle_LED                    7       /* control type: LED, callback function: (none) */

#define  ScriptPan                        9
#define  ScriptPan_ScriptLog              2       /* control type: textBox, callback function: (none) */
#define  ScriptPan_ScriptGraphCtr         3       /* control type: graph, callback function: (none) */
#define  ScriptPan_ScriptImportSetting    4       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptQuit             5       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptDelete           6       /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptApply            7       /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptPrint            8       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptSaveLog          9       /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptLickDetGraph     10      /* control type: strip, callback function: (none) */
#define  ScriptPan_ScriptSave             11      /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptLoad             12      /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptNew              13      /* control type: command, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptElement          14      /* control type: ring, callback function: (none) */
#define  ScriptPan_ScriptAdd              15      /* control type: command, callback function: ScriptAddElement_CB */
#define  ScriptPan_SPLITTER_2             16      /* control type: splitter, callback function: (none) */
#define  ScriptPan_SPLITTER_3             17      /* control type: splitter, callback function: (none) */
#define  ScriptPan_SPLITTER               18      /* control type: splitter, callback function: (none) */
#define  ScriptPan_NewScript              19      /* control type: textMsg, callback function: (none) */
#define  ScriptPan_ScriptPause            20      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptStop             21      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptRun              22      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptPos              23      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptName             24      /* control type: string, callback function: (none) */
#define  ScriptPan_LogFile                25      /* control type: string, callback function: (none) */
#define  ScriptPan_ScriptDropOutCtr       26      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptDropInCtr        27      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptFalseAlarmCtr    28      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptCorrectRejCtr    29      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptIncorrectRejCtr  30      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptAirPuffCtr       31      /* control type: numeric, callback function: (none) */
#define  ScriptPan_ScriptCorrectLickCtr   32      /* control type: numeric, callback function: (none) */
#define  ScriptPan_DECORATION             33      /* control type: deco, callback function: (none) */
#define  ScriptPan_ScriptPrinter          34      /* control type: pictButton, callback function: WhiskerScriptButton_CB */
#define  ScriptPan_ScriptCounter          35      /* control type: textMsg, callback function: (none) */

#define  SoundEle                         10
#define  SoundEle_SPLITTER                2       /* control type: splitter, callback function: (none) */
#define  SoundEle_EleName                 3       /* control type: textMsg, callback function: (none) */
#define  SoundEle_EleNum                  4       /* control type: textMsg, callback function: (none) */
#define  SoundEle_EleDelete               5       /* control type: pictButton, callback function: SoundElementButtons_CB */
#define  SoundEle_LED                     6       /* control type: LED, callback function: (none) */
#define  SoundEle_ElePlay                 7       /* control type: command, callback function: SoundElementButtons_CB */
#define  SoundEle_EleLoad                 8       /* control type: command, callback function: SoundElementButtons_CB */
#define  SoundEle_ElePath                 9       /* control type: string, callback function: (none) */
#define  SoundEle_EleAsync                10      /* control type: radioButton, callback function: (none) */

#define  StartEle                         11
#define  StartEle_SPLITTER                2       /* control type: splitter, callback function: (none) */
#define  StartEle_EleName                 3       /* control type: textMsg, callback function: (none) */
#define  StartEle_EleDelay                4       /* control type: numeric, callback function: (none) */
#define  StartEle_EleNum                  5       /* control type: textMsg, callback function: (none) */
#define  StartEle_EleDelete               6       /* control type: pictButton, callback function: StartElementButtons_CB */
#define  StartEle_LED                     7       /* control type: LED, callback function: (none) */

#define  StopEle                          12
#define  StopEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  StopEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  StopEle_EleDelay                 4       /* control type: numeric, callback function: (none) */
#define  StopEle_EleNum                   5       /* control type: textMsg, callback function: (none) */
#define  StopEle_EleDelete                6       /* control type: pictButton, callback function: StopElementButtons_CB */
#define  StopEle_LED                      7       /* control type: LED, callback function: (none) */

#define  THWaitEle                        13
#define  THWaitEle_SPLITTER               2       /* control type: splitter, callback function: (none) */
#define  THWaitEle_EleName                3       /* control type: textMsg, callback function: (none) */
#define  THWaitEle_EleNum                 4       /* control type: textMsg, callback function: (none) */
#define  THWaitEle_EleDelete              5       /* control type: pictButton, callback function: THWaitElementButtons_CB */
#define  THWaitEle_LED                    6       /* control type: LED, callback function: (none) */
#define  THWaitEle_EleText                7       /* control type: textMsg, callback function: (none) */

#define  WaitEle                          14
#define  WaitEle_SPLITTER                 2       /* control type: splitter, callback function: (none) */
#define  WaitEle_EleName                  3       /* control type: textMsg, callback function: (none) */
#define  WaitEle_EleDelay                 4       /* control type: numeric, callback function: (none) */
#define  WaitEle_EleNum                   5       /* control type: textMsg, callback function: (none) */
#define  WaitEle_EleDelete                6       /* control type: pictButton, callback function: WaitElementButtons_CB */
#define  WaitEle_LED                      7       /* control type: LED, callback function: (none) */

#define  XYCondEle                        15
#define  XYCondEle_SPLITTER               2       /* control type: splitter, callback function: (none) */
#define  XYCondEle_EleName                3       /* control type: textMsg, callback function: (none) */
#define  XYCondEle_EleNum                 4       /* control type: textMsg, callback function: (none) */
#define  XYCondEle_EleDelete              5       /* control type: pictButton, callback function: XYMoveElementButtons_CB */
#define  XYCondEle_LED                    6       /* control type: LED, callback function: (none) */
#define  XYCondEle_EleFalse               7       /* control type: numeric, callback function: (none) */
#define  XYCondEle_EleValue               8       /* control type: ring, callback function: (none) */
#define  XYCondEle_EleTrue                9       /* control type: numeric, callback function: (none) */

#define  XYMoveEle                        16
#define  XYMoveEle_SPLITTER               2       /* control type: splitter, callback function: (none) */
#define  XYMoveEle_EleName                3       /* control type: textMsg, callback function: (none) */
#define  XYMoveEle_EleNum                 4       /* control type: textMsg, callback function: (none) */
#define  XYMoveEle_EleDelete              5       /* control type: pictButton, callback function: XYMoveElementButtons_CB */
#define  XYMoveEle_LED                    6       /* control type: LED, callback function: (none) */
#define  XYMoveEle_ElePercentage          7       /* control type: numeric, callback function: (none) */
#define  XYMoveEle_ElePos                 8       /* control type: numeric, callback function: (none) */
#define  XYMoveEle_EleShow                9       /* control type: command, callback function: XYMoveElementButtons_CB */
#define  XYMoveEle_EleGoState             10      /* control type: ring, callback function: (none) */
#define  XYMoveEle_EleAsync               11      /* control type: radioButton, callback function: (none) */
#define  XYMoveEle_EleAdd                 12      /* control type: command, callback function: XYMoveElementButtons_CB */

#define  XYPosPan                         17
#define  XYPosPan_XYPositionsTable        2       /* control type: table, callback function: XYPosTable_CB */
#define  XYPosPan_XYPositionOk            3       /* control type: command, callback function: XYPosTable_CB */

#define  ZMoveEle                         18
#define  ZMoveEle_SPLITTER                2       /* control type: splitter, callback function: (none) */
#define  ZMoveEle_EleName                 3       /* control type: textMsg, callback function: (none) */
#define  ZMoveEle_EleNum                  4       /* control type: textMsg, callback function: (none) */
#define  ZMoveEle_EleDelete               5       /* control type: pictButton, callback function: ZMoveElementButtons_CB */
#define  ZMoveEle_LED                     6       /* control type: LED, callback function: (none) */
#define  ZMoveEle_EleIO_CH                7       /* control type: ring, callback function: (none) */
#define  ZMoveEle_EleCommand              8       /* control type: ring, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK ActionElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CondElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK JumpElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK MsgElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK RepeatElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ResRepElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ScriptAddElement_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK SoundElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK StartElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK StopElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK THWaitElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WaitElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK WhiskerScriptButton_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYMoveElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK XYPosTable_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ZMoveElementButtons_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
