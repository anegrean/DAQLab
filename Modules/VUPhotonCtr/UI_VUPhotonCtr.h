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

#define  CounterPan                       1
#define  CounterPan_Command               2       /* control type: numeric, callback function: (none) */
#define  CounterPan_Background            3       /* control type: deco, callback function: (none) */
#define  CounterPan_Status                4       /* control type: numeric, callback function: (none) */
#define  CounterPan_FIFOReset             5       /* control type: command, callback function: (none) */
#define  CounterPan_Reset                 6       /* control type: command, callback function: (none) */
#define  CounterPan_LEDCounterOverflow    7       /* control type: LED, callback function: (none) */
#define  CounterPan_LEDDoorOpen           8       /* control type: LED, callback function: (none) */
#define  CounterPan_Close                 9       /* control type: command, callback function: (none) */
#define  CounterPan_LEDFIFOFull           10      /* control type: LED, callback function: (none) */
#define  CounterPan_LEDFIFOAlmostFull     11      /* control type: LED, callback function: (none) */
#define  CounterPan_LEDRunning            12      /* control type: LED, callback function: (none) */
#define  CounterPan_TestMode              13      /* control type: textButton, callback function: (none) */

#define  VUPCChan                         2
#define  VUPCChan_Fan                     2       /* control type: radioButton, callback function: (none) */
#define  VUPCChan_Cooling                 3       /* control type: radioButton, callback function: (none) */
#define  VUPCChan_Plate                   4       /* control type: deco, callback function: (none) */
#define  VUPCChan_Mode                    5       /* control type: ring, callback function: (none) */
#define  VUPCChan_LED_STATE1              6       /* control type: LED, callback function: (none) */
#define  VUPCChan_LED_TEMP1               7       /* control type: LED, callback function: (none) */
#define  VUPCChan_Threshold               8       /* control type: numeric, callback function: (none) */
#define  VUPCChan_Gain                    9       /* control type: numeric, callback function: (none) */

#define  VUPCMain                         3

#define  VUPCSet                          4
#define  VUPCSet_Channels                 2       /* control type: listBox, callback function: (none) */
#define  VUPCSet_Background               3       /* control type: deco, callback function: (none) */
#define  VUPCSet_SamplingRate             4       /* control type: numeric, callback function: (none) */
#define  VUPCSet_Duration                 5       /* control type: numeric, callback function: (none) */
#define  VUPCSet_NSamples                 6       /* control type: numeric, callback function: (none) */
#define  VUPCSet_MeasMode                 7       /* control type: ring, callback function: (none) */
#define  VUPCSet_Close                    8       /* control type: command, callback function: (none) */

#define  VUPCStatus                       5

#define  VUPCTask                         6
#define  VUPCTask_Wait                    2       /* control type: numeric, callback function: (none) */
#define  VUPCTask_Background              3       /* control type: deco, callback function: (none) */
#define  VUPCTask_Repeat                  4       /* control type: numeric, callback function: (none) */
#define  VUPCTask_TotalIterations         5       /* control type: numeric, callback function: (none) */
#define  VUPCTask_Mode                    6       /* control type: radioButton, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  VUPC                             1
#define  VUPC_UNNAMEDM                    2


     /* (no callbacks specified in the resource file) */ 


#ifdef __cplusplus
    }
#endif
