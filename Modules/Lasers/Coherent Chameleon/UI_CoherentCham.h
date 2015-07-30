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

#define  COMSetPan                        1
#define  COMSetPan_Plate                  2       /* control type: deco, callback function: (none) */
#define  COMSetPan_Port                   3       /* control type: numeric, callback function: (none) */
#define  COMSetPan_StopBits               4       /* control type: ring, callback function: (none) */
#define  COMSetPan_DataBits               5       /* control type: ring, callback function: (none) */
#define  COMSetPan_Parity                 6       /* control type: ring, callback function: (none) */
#define  COMSetPan_BaudRate               7       /* control type: ring, callback function: (none) */
#define  COMSetPan_OKBttn                 8       /* control type: command, callback function: (none) */

#define  MainPan                          2
#define  MainPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  MainPan_Power                    3       /* control type: numeric, callback function: (none) */
#define  MainPan_Wavelength               4       /* control type: numeric, callback function: (none) */
#define  MainPan_PumpPeakHold             5       /* control type: radioButton, callback function: (none) */
#define  MainPan_Shutter                  6       /* control type: textButton, callback function: (none) */
#define  MainPan_StatusString             7       /* control type: string, callback function: (none) */
#define  MainPan_OnOff                    8       /* control type: textButton, callback function: (none) */
#define  MainPan_Modelocked               9       /* control type: LED, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* (no callbacks specified in the resource file) */ 


#ifdef __cplusplus
    }
#endif
