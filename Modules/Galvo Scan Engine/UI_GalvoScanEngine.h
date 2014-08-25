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

#define  ScanPan                          1
#define  ScanPan_PixelSize                2       /* control type: numeric, callback function: CB_SetPixelSize */
#define  ScanPan_Plate_3                  3       /* control type: deco, callback function: (none) */
#define  ScanPan_Wait                     4       /* control type: numeric, callback function: (none) */
#define  ScanPan_Duration                 5       /* control type: numeric, callback function: (none) */
#define  ScanPan_Plate_2                  6       /* control type: deco, callback function: (none) */
#define  ScanPan_Plate                    7       /* control type: deco, callback function: (none) */
#define  ScanPan_ImageFPS                 8       /* control type: numeric, callback function: (none) */
#define  ScanPan_HeightOffset             9       /* control type: numeric, callback function: (none) */
#define  ScanPan_WidthOffset              10      /* control type: numeric, callback function: (none) */
#define  ScanPan_Width                    11      /* control type: numeric, callback function: CB_SetScanWidth */
#define  ScanPan_PSF_FWHM                 12      /* control type: numeric, callback function: (none) */
#define  ScanPan_PixelDwell               13      /* control type: string, callback function: CB_SetPixelDwellTime */
#define  ScanPan_Height                   14      /* control type: string, callback function: CB_SetScanHeight */
#define  ScanPan_Mode                     15      /* control type: ring, callback function: (none) */
#define  ScanPan_Averages                 16      /* control type: numeric, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CB_SetPixelDwellTime(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_SetPixelSize(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_SetScanHeight(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_SetScanWidth(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
