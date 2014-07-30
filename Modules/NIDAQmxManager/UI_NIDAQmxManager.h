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

#define  AIAOSetPan                       1
#define  AIAOSetPan_AIAOterminal          2       /* control type: ring, callback function: CB_AIAOterminal */
#define  AIAOSetPan_VChanName             3       /* control type: string, callback function: CB_VChanName */
#define  AIAOSetPan_AIAORange             4       /* control type: ring, callback function: CB_AIAORange */

#define  DAQSetPan1                       2
#define  DAQSetPan1_TAB                   2       /* control type: tab, callback function: (none) */

#define  DevListPan                       3
#define  DevListPan_DAQTable              2       /* control type: table, callback function: (none) */
#define  DevListPan_DoneBTTN              3       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_AddBTTN               4       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_Plate                 5       /* control type: deco, callback function: (none) */

#define  DIDOSetPan                       4

#define  NIDAQmxPan                       5
#define  NIDAQmxPan_Devices               2       /* control type: tab, callback function: (none) */

#define  TaskSetPan                       6
#define  TaskSetPan_DelChan               2       /* control type: command, callback function: (none) */
#define  TaskSetPan_AddChan               3       /* control type: command, callback function: (none) */
#define  TaskSetPan_TaskSet               4       /* control type: tab, callback function: (none) */
#define  TaskSetPan_ChanSet               5       /* control type: tab, callback function: (none) */
#define  TaskSetPan_PhysChan              6       /* control type: listBox, callback function: (none) */
#define  TaskSetPan_IOType                7       /* control type: ring, callback function: (none) */
#define  TaskSetPan_IOMode                8       /* control type: ring, callback function: (none) */
#define  TaskSetPan_IO                    9       /* control type: ring, callback function: (none) */

     /* tab page panel controls */
#define  IO_StartTrigSource               2       /* control type: string, callback function: CB_AIAOTaskSet */
#define  IO_RefClkSource                  3       /* control type: string, callback function: CB_AIAOTaskSet */
#define  IO_SampleClkSource               4       /* control type: string, callback function: CB_AIAOTaskSet */
#define  IO_RefClkFreq                    5       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  IO_Timeout                       6       /* control type: numeric, callback function: CB_AIAOTaskSet */

     /* tab page panel controls */
#define  SET_BlockSize                    2       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  SET_SamplingRate                 3       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  SET_Duration                     4       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  SET_Samples                      5       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  SET_TrigType                     6       /* control type: ring, callback function: CB_AIAOTaskSet */
#define  SET_MeasMode                     7       /* control type: ring, callback function: CB_AIAOTaskSet */
#define  SET_UseAsReferenceCHKBOX         8       /* control type: radioButton, callback function: CB_AIAOTaskSet */

     /* tab page panel controls */
#define  TRIG_TrigSlope                   2       /* control type: ring, callback function: CB_AIAOTaskSet */
#define  TRIG_TrigLevel                   3       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  TRIG_TrigWndType                 4       /* control type: ring, callback function: CB_AIAOTaskSet */
#define  TRIG_TrigWndTop                  5       /* control type: numeric, callback function: CB_AIAOTaskSet */
#define  TRIG_TrigWndBttm                 6       /* control type: numeric, callback function: CB_AIAOTaskSet */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK CB_AIAORange(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_AIAOTaskSet(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_AIAOterminal(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CB_VChanName(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK ManageDevices_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
