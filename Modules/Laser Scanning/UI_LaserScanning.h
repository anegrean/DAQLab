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

#define  EnginesPan                       1
#define  EnginesPan_DoneBTTN              2       /* control type: command, callback function: (none) */
#define  EnginesPan_Plate_3               3       /* control type: deco, callback function: (none) */
#define  EnginesPan_ScanTypes             4       /* control type: listBox, callback function: (none) */

#define  NonResGCal                       2
#define  NonResGCal_Tab                   2       /* control type: tab, callback function: (none) */
#define  NonResGCal_GalvoPlot             3       /* control type: graph, callback function: CB_GalvoPlot */
#define  NonResGCal_CursorY               4       /* control type: numeric, callback function: (none) */
#define  NonResGCal_CursorX               5       /* control type: numeric, callback function: (none) */
#define  NonResGCal_Done                  6       /* control type: command, callback function: (none) */
#define  NonResGCal_SaveCalibData         7       /* control type: command, callback function: (none) */
#define  NonResGCal_Abort                 8       /* control type: command, callback function: (none) */

#define  RectRaster                       3
#define  RectRaster_PixelSize             2       /* control type: numeric, callback function: (none) */
#define  RectRaster_Plate_3               3       /* control type: deco, callback function: (none) */
#define  RectRaster_Duration              4       /* control type: numeric, callback function: (none) */
#define  RectRaster_Plate_2               5       /* control type: deco, callback function: (none) */
#define  RectRaster_Plate                 6       /* control type: deco, callback function: (none) */
#define  RectRaster_FPS                   7       /* control type: numeric, callback function: (none) */
#define  RectRaster_HeightOffset          8       /* control type: numeric, callback function: (none) */
#define  RectRaster_WidthOffset           9       /* control type: numeric, callback function: (none) */
#define  RectRaster_Width                 10      /* control type: numeric, callback function: (none) */
#define  RectRaster_PSF_FWHM              11      /* control type: numeric, callback function: (none) */
#define  RectRaster_PixelDwell            12      /* control type: string, callback function: (none) */
#define  RectRaster_Height                13      /* control type: string, callback function: (none) */
#define  RectRaster_Mode                  14      /* control type: ring, callback function: (none) */
#define  RectRaster_Averages              15      /* control type: numeric, callback function: (none) */

#define  ScanPan                          4
#define  ScanPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  ScanPan_ScanEngines              3       /* control type: tab, callback function: (none) */

#define  ScanSetPan                       5
#define  ScanSetPan_AddImgChan            2       /* control type: command, callback function: (none) */
#define  ScanSetPan_Close                 3       /* control type: command, callback function: (none) */
#define  ScanSetPan_NewSlowAxisCal        4       /* control type: command, callback function: (none) */
#define  ScanSetPan_NewFastAxisCal        5       /* control type: command, callback function: (none) */
#define  ScanSetPan_Plate                 6       /* control type: deco, callback function: (none) */
#define  ScanSetPan_FastAxisPosition      7       /* control type: string, callback function: (none) */
#define  ScanSetPan_FastAxisCommand       8       /* control type: string, callback function: (none) */
#define  ScanSetPan_SlowAxisPosition      9       /* control type: string, callback function: (none) */
#define  ScanSetPan_SlowAxisCommand       10      /* control type: string, callback function: (none) */
#define  ScanSetPan_ImageOut              11      /* control type: string, callback function: (none) */
#define  ScanSetPan_ImgChans              12      /* control type: listBox, callback function: (none) */
#define  ScanSetPan_SlowAxisType          13      /* control type: ring, callback function: (none) */
#define  ScanSetPan_SlowAxisCal           14      /* control type: ring, callback function: (none) */
#define  ScanSetPan_FastAxisCal           15      /* control type: ring, callback function: (none) */
#define  ScanSetPan_FastAxisType          16      /* control type: ring, callback function: (none) */

     /* tab page panel controls */
#define  Cal_CommMaxV                     2       /* control type: numeric, callback function: CB_GalvoCalIO */
#define  Cal_ResponseLag                  3       /* control type: numeric, callback function: (none) */
#define  Cal_ScanTime                     4       /* control type: numeric, callback function: (none) */
#define  Cal_MinStep                      5       /* control type: numeric, callback function: (none) */
#define  Cal_TEXTMSG                      6       /* control type: textMsg, callback function: (none) */
#define  Cal_PosMaxV                      7       /* control type: numeric, callback function: CB_GalvoCalIO */
#define  Cal_PosStdDev                    8       /* control type: numeric, callback function: (none) */
#define  Cal_Offset_b                     9       /* control type: numeric, callback function: (none) */
#define  Cal_Slope_a                      10      /* control type: numeric, callback function: (none) */
#define  Cal_ParkedV                      11      /* control type: numeric, callback function: CB_GalvoCalUIset */
#define  Cal_PosMinV                      12      /* control type: numeric, callback function: CB_GalvoCalIO */
#define  Cal_CommMinV                     13      /* control type: numeric, callback function: CB_GalvoCalIO */
#define  Cal_CalStatus                    14      /* control type: listBox, callback function: (none) */
#define  Cal_Plate_2                      15      /* control type: deco, callback function: (none) */
#define  Cal_Plate                        16      /* control type: deco, callback function: (none) */
#define  Cal_Resolution                   17      /* control type: numeric, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CB_GalvoCalIO(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_GalvoCalUIset(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_GalvoPlot(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
