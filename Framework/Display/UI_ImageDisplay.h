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

#define  CanvasPan                        1
#define  CanvasPan_Canvas                 2       /* control type: canvas, callback function: mouse_callback */

#define  DisplayPan                       2
#define  DisplayPan_NUMERICSLIDE          2       /* control type: scale, callback function: (none) */
#define  DisplayPan_PICTUREBUTTON         3       /* control type: pictButton, callback function: (none) */
#define  DisplayPan_PICTUREBUTTON_2       4       /* control type: pictButton, callback function: (none) */
#define  DisplayPan_PICTUREBUTTON_3       5       /* control type: pictButton, callback function: (none) */
#define  DisplayPan_PICTUREBUTTON_4       6       /* control type: pictButton, callback function: (none) */
#define  DisplayPan_PICTUREBUTTON_5       7       /* control type: pictButton, callback function: (none) */
#define  DisplayPan_PICTUREBUTTON_6       8       /* control type: pictButton, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  menubar                          1
#define  menubar_file                     2
#define  menubar_file_new                 3
#define  menubar_exit                     4


     /* Callback Prototypes: */

int  CVICALLBACK mouse_callback(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
