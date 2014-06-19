/**************************************************************************/
/* LabWindows/CVI User Interface Resource (UIR) Include File              */
/* Copyright (c) National Instruments 2014. All Rights Reserved.          */
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
#define  CounterPan_NUM_COMMAND           2       /* control type: numeric, callback function: (none) */
#define  CounterPan_NUM_STATUS            3       /* control type: numeric, callback function: (none) */
#define  CounterPan_BTTN_SYNC             4       /* control type: command, callback function: CB_SyncHW */
#define  CounterPan_BTTN_FFRESET          5       /* control type: command, callback function: CB_ResetFifo */
#define  CounterPan_BTTN_RESET            6       /* control type: command, callback function: CB_ResetHW */
#define  CounterPan_LED_TRIGFAIL          7       /* control type: LED, callback function: (none) */
#define  CounterPan_LED_FIFO_OVERFLOW     8       /* control type: LED, callback function: (none) */
#define  CounterPan_LED_FIFO_AFULL        9       /* control type: LED, callback function: (none) */
#define  CounterPan_LED_FIFO_QFULL        10      /* control type: LED, callback function: (none) */
#define  CounterPan_LED_FIFO_EMPTY        11      /* control type: LED, callback function: (none) */
#define  CounterPan_LED_FIFO_UNDER        12      /* control type: LED, callback function: (none) */
#define  CounterPan_LED_RUNNING           13      /* control type: LED, callback function: (none) */
#define  CounterPan_BTTN_TestMode         14      /* control type: textButton, callback function: CB_TestMode */

#define  VUPCChan                         2
#define  VUPCChan_Fan                     2       /* control type: radioButton, callback function: (none) */
#define  VUPCChan_Cooling                 3       /* control type: radioButton, callback function: (none) */
#define  VUPCChan_Plate                   4       /* control type: deco, callback function: (none) */
#define  VUPCChan_Mode                    5       /* control type: ring, callback function: (none) */
#define  VUPCChan_LED_STATE1              6       /* control type: LED, callback function: (none) */
#define  VUPCChan_LED_CURR1               7       /* control type: LED, callback function: (none) */
#define  VUPCChan_LED_TEMP1               8       /* control type: LED, callback function: (none) */
#define  VUPCChan_Threshold               9       /* control type: numeric, callback function: (none) */
#define  VUPCChan_Gain                    10      /* control type: numeric, callback function: (none) */

#define  VUPCMain                         3

#define  VUPCSet                          4
#define  VUPCSet_Channels                 2       /* control type: listBox, callback function: (none) */
#define  VUPCSet_Background               3       /* control type: deco, callback function: (none) */
#define  VUPCSet_SamplingRate             4       /* control type: numeric, callback function: (none) */
#define  VUPCSet_Duration                 5       /* control type: numeric, callback function: (none) */
#define  VUPCSet_NSamples                 6       /* control type: numeric, callback function: (none) */
#define  VUPCSet_MeasMode                 7       /* control type: ring, callback function: (none) */
#define  VUPCSet_Close                    8       /* control type: command, callback function: (none) */
#define  VUPCSet_UseRefNSamples           9       /* control type: radioButton, callback function: (none) */
#define  VUPCSet_UseRefSampling           10      /* control type: radioButton, callback function: (none) */
#define  VUPCSet_RefChan                  11      /* control type: ring, callback function: (none) */

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


     /* Callback Prototypes: */

int  CVICALLBACK CB_ResetFifo(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_ResetHW(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_SyncHW(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_TestMode(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
