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

#define  AxisSelect                       1
#define  AxisSelect_AxisType              2       /* control type: ring, callback function: (none) */
#define  AxisSelect_Plate_3               3       /* control type: deco, callback function: (none) */
#define  AxisSelect_CancelBTTN            4       /* control type: command, callback function: (none) */
#define  AxisSelect_OKBTTN                5       /* control type: command, callback function: (none) */

#define  EnginesPan                       2
#define  EnginesPan_DoneBTTN              2       /* control type: command, callback function: (none) */
#define  EnginesPan_Plate_3               3       /* control type: deco, callback function: (none) */
#define  EnginesPan_ScanTypes             4       /* control type: listBox, callback function: (none) */

#define  ManageAxis                       3
#define  ManageAxis_AxisCalibList         2       /* control type: listBox, callback function: (none) */
#define  ManageAxis_Plate_3               3       /* control type: deco, callback function: (none) */
#define  ManageAxis_Close                 4       /* control type: command, callback function: (none) */
#define  ManageAxis_New                   5       /* control type: command, callback function: (none) */

#define  NewObjPan                        4
#define  NewObjPan_Cancel                 2       /* control type: command, callback function: (none) */
#define  NewObjPan_OK                     3       /* control type: command, callback function: (none) */
#define  NewObjPan_Name                   4       /* control type: string, callback function: (none) */
#define  NewObjPan_ObjectiveLensFL        5       /* control type: numeric, callback function: (none) */
#define  NewObjPan_Plate                  6       /* control type: deco, callback function: (none) */

#define  NonResGCal                       5
#define  NonResGCal_Tab                   2       /* control type: tab, callback function: (none) */
#define  NonResGCal_GalvoPlot             3       /* control type: graph, callback function: (none) */
#define  NonResGCal_CursorY               4       /* control type: numeric, callback function: (none) */
#define  NonResGCal_CursorX               5       /* control type: numeric, callback function: (none) */
#define  NonResGCal_Plate                 6       /* control type: deco, callback function: (none) */
#define  NonResGCal_Done                  7       /* control type: command, callback function: (none) */
#define  NonResGCal_SaveCalib             8       /* control type: command, callback function: (none) */

#define  RectRaster                       6
#define  RectRaster_Tab                   2       /* control type: tab, callback function: (none) */

#define  ScanPan                          7
#define  ScanPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  ScanPan_ScanEngines              3       /* control type: tab, callback function: (none) */

#define  ScanSetPan                       8
#define  ScanSetPan_AddChan               2       /* control type: command, callback function: (none) */
#define  ScanSetPan_AddObjective          3       /* control type: command, callback function: (none) */
#define  ScanSetPan_Close                 4       /* control type: command, callback function: (none) */
#define  ScanSetPan_GalvoSamplingRate     5       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_PixelDelay            6       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_RefClkFreq            7       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_TubeLensFL            8       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_ScanLensFL            9       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_Plate                 10      /* control type: deco, callback function: (none) */
#define  ScanSetPan_Objectives            11      /* control type: listBox, callback function: (none) */
#define  ScanSetPan_SlowAxisCal           12      /* control type: ring, callback function: (none) */
#define  ScanSetPan_FastAxisCal           13      /* control type: ring, callback function: (none) */
#define  ScanSetPan_Channels              14      /* control type: table, callback function: (none) */

     /* tab page panel controls */
#define  Cal_MechanicalResponse           2       /* control type: numeric, callback function: (none) */
#define  Cal_CommMaxV                     3       /* control type: numeric, callback function: (none) */
#define  Cal_ResponseLag                  4       /* control type: numeric, callback function: (none) */
#define  Cal_ScanTime                     5       /* control type: numeric, callback function: (none) */
#define  Cal_MinStep                      6       /* control type: numeric, callback function: (none) */
#define  Cal_TEXTMSG                      7       /* control type: textMsg, callback function: (none) */
#define  Cal_PosStdDev                    8       /* control type: numeric, callback function: (none) */
#define  Cal_Offset_b                     9       /* control type: numeric, callback function: (none) */
#define  Cal_Slope_a                      10      /* control type: numeric, callback function: (none) */
#define  Cal_ParkedV                      11      /* control type: numeric, callback function: (none) */
#define  Cal_CalStatus                    12      /* control type: listBox, callback function: (none) */
#define  Cal_Plate_2                      13      /* control type: deco, callback function: (none) */
#define  Cal_Plate                        14      /* control type: deco, callback function: (none) */
#define  Cal_Resolution                   15      /* control type: numeric, callback function: (none) */

     /* tab page panel controls */
#define  PointTab_ROIs                    2       /* control type: listBox, callback function: (none) */
#define  PointTab_NRepeat                 3       /* control type: numeric, callback function: (none) */
#define  PointTab_Repeat                  4       /* control type: numeric, callback function: (none) */
#define  PointTab_Plate                   5       /* control type: deco, callback function: (none) */
#define  PointTab_Averaging               6       /* control type: ring, callback function: (none) */
#define  PointTab_Mode                    7       /* control type: ring, callback function: (none) */
#define  PointTab_NAveraging              8       /* control type: numeric, callback function: (none) */
#define  PointTab_NPulses                 9       /* control type: numeric, callback function: (none) */
#define  PointTab_PulseOFF                10      /* control type: numeric, callback function: (none) */
#define  PointTab_PulseON                 11      /* control type: numeric, callback function: (none) */
#define  PointTab_StartDelayIncrement     12      /* control type: numeric, callback function: (none) */
#define  PointTab_SequencePeriod          13      /* control type: numeric, callback function: (none) */
#define  PointTab_StartDelay              14      /* control type: numeric, callback function: (none) */
#define  PointTab_StimDelay               15      /* control type: numeric, callback function: (none) */
#define  PointTab_Hold                    16      /* control type: numeric, callback function: (none) */
#define  PointTab_DECORATION_2            17      /* control type: deco, callback function: (none) */
#define  PointTab_DECORATION_3            18      /* control type: deco, callback function: (none) */
#define  PointTab_DECORATION              19      /* control type: deco, callback function: (none) */
#define  PointTab_SequenceLabel           20      /* control type: textMsg, callback function: (none) */
#define  PointTab_TEXTMSG_2               21      /* control type: textMsg, callback function: (none) */
#define  PointTab_TEXTMSG                 22      /* control type: textMsg, callback function: (none) */
#define  PointTab_ParkedAtSeqEnd          23      /* control type: radioButton, callback function: (none) */
#define  PointTab_Stimulate               24      /* control type: radioButton, callback function: (none) */
#define  PointTab_Record                  25      /* control type: radioButton, callback function: (none) */

     /* tab page panel controls */
#define  ScanTab_PixelSize                2       /* control type: numeric, callback function: (none) */
#define  ScanTab_FramesAcquired           3       /* control type: numeric, callback function: (none) */
#define  ScanTab_NFrames                  4       /* control type: numeric, callback function: (none) */
#define  ScanTab_ExecutionMode            5       /* control type: radioButton, callback function: (none) */
#define  ScanTab_Objective                6       /* control type: ring, callback function: (none) */
#define  ScanTab_Plate                    7       /* control type: deco, callback function: (none) */
#define  ScanTab_FPS                      8       /* control type: numeric, callback function: (none) */
#define  ScanTab_WidthOffset              9       /* control type: numeric, callback function: (none) */
#define  ScanTab_HeightOffset             10      /* control type: numeric, callback function: (none) */
#define  ScanTab_Width                    11      /* control type: string, callback function: (none) */
#define  ScanTab_Height                   12      /* control type: numeric, callback function: (none) */
#define  ScanTab_Ready                    13      /* control type: LED, callback function: (none) */
#define  ScanTab_PixelDwell               14      /* control type: string, callback function: (none) */
#define  ScanTab_DECORATION_4             15      /* control type: deco, callback function: (none) */
#define  ScanTab_DECORATION_3             16      /* control type: deco, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* (no callbacks specified in the resource file) */ 


#ifdef __cplusplus
    }
#endif
