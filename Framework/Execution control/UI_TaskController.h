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

#define  TaskPan                          1
#define  TaskPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  TaskPan_TaskTree                 3       /* control type: tree, callback function: (none) */
#define  TaskPan_Close                    4       /* control type: command, callback function: CloseDAQLabModulesPan_CB */

#define  TCPan1                           2
#define  TCPan1_StartStop                 2       /* control type: textButton, callback function: (none) */
#define  TCPan1_Wait                      3       /* control type: numeric, callback function: (none) */
#define  TCPan1_Repeat                    4       /* control type: numeric, callback function: (none) */
#define  TCPan1_TotalIterations           5       /* control type: numeric, callback function: (none) */
#define  TCPan1_Abort                     6       /* control type: command, callback function: (none) */
#define  TCPan1_Reset                     7       /* control type: command, callback function: (none) */
#define  TCPan1_Name                      8       /* control type: string, callback function: (none) */
#define  TCPan1_Mode                      9       /* control type: radioButton, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CloseDAQLabModulesPan_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
