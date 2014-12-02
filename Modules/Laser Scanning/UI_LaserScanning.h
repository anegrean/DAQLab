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
#define  NonResGCal_Done                  6       /* control type: command, callback function: (none) */
#define  NonResGCal_SaveCalib             7       /* control type: command, callback function: (none) */

#define  RectRaster                       6
#define  RectRaster_PixelSize             2       /* control type: numeric, callback function: (none) */
#define  RectRaster_Plate_3               3       /* control type: deco, callback function: (none) */
#define  RectRaster_Duration              4       /* control type: numeric, callback function: (none) */
#define  RectRaster_Plate_2               5       /* control type: deco, callback function: (none) */
#define  RectRaster_Plate                 6       /* control type: deco, callback function: (none) */
#define  RectRaster_FPS                   7       /* control type: numeric, callback function: (none) */
#define  RectRaster_HeightOffset          8       /* control type: numeric, callback function: (none) */
#define  RectRaster_WidthOffset           9       /* control type: numeric, callback function: (none) */
#define  RectRaster_Width                 10      /* control type: numeric, callback function: (none) */
#define  RectRaster_PixelDwell            11      /* control type: string, callback function: (none) */
#define  RectRaster_Height                12      /* control type: string, callback function: (none) */
#define  RectRaster_Objective             13      /* control type: ring, callback function: (none) */
#define  RectRaster_Mode                  14      /* control type: ring, callback function: (none) */
#define  RectRaster_Averages              15      /* control type: numeric, callback function: (none) */

#define  ScanPan                          7
#define  ScanPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  ScanPan_ScanEngines              3       /* control type: tab, callback function: (none) */

#define  ScanSetPan                       8
#define  ScanSetPan_AddObjective          2       /* control type: command, callback function: (none) */
#define  ScanSetPan_AddImgChan            3       /* control type: command, callback function: (none) */
#define  ScanSetPan_Close                 4       /* control type: command, callback function: (none) */
#define  ScanSetPan_GalvoSamplingRate     5       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_PixelClockFrequency   6       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_TubeLensFL            7       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_ScanLensFL            8       /* control type: numeric, callback function: (none) */
#define  ScanSetPan_Plate                 9       /* control type: deco, callback function: (none) */
#define  ScanSetPan_FastAxisPosition      10      /* control type: string, callback function: (none) */
#define  ScanSetPan_FastAxisCommand       11      /* control type: string, callback function: (none) */
#define  ScanSetPan_SlowAxisPosition      12      /* control type: string, callback function: (none) */
#define  ScanSetPan_SlowAxisCommand       13      /* control type: string, callback function: (none) */
#define  ScanSetPan_PixelSettings         14      /* control type: string, callback function: (none) */
#define  ScanSetPan_ShutterCommand        15      /* control type: string, callback function: (none) */
#define  ScanSetPan_ImageOut              16      /* control type: string, callback function: (none) */
#define  ScanSetPan_Objectives            17      /* control type: listBox, callback function: (none) */
#define  ScanSetPan_ImgChans              18      /* control type: listBox, callback function: (none) */
#define  ScanSetPan_SlowAxisCal           19      /* control type: ring, callback function: (none) */
#define  ScanSetPan_FastAxisCal           20      /* control type: ring, callback function: (none) */

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


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* (no callbacks specified in the resource file) */ 


#ifdef __cplusplus
    }
#endif
