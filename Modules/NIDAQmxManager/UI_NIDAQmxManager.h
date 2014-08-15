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

#define  AIAOChSet                        1
#define  AIAOChSet_Plate                  2       /* control type: deco, callback function: (none) */
#define  AIAOChSet_Terminal               3       /* control type: ring, callback function: (none) */
#define  AIAOChSet_VChanName              4       /* control type: string, callback function: (none) */
#define  AIAOChSet_Range                  5       /* control type: ring, callback function: (none) */
#define  AIAOChSet_OnDemand               6       /* control type: radioButton, callback function: (none) */

#define  AIAOTskSet                       2
#define  AIAOTskSet_Tab                   2       /* control type: tab, callback function: (none) */

#define  DevListPan                       3
#define  DevListPan_DAQTable              2       /* control type: table, callback function: (none) */
#define  DevListPan_DoneBTTN              3       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_AddBTTN               4       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_Plate                 5       /* control type: deco, callback function: (none) */

#define  DIDOChSet                        4
#define  DIDOChSet_Plate                  2       /* control type: deco, callback function: (none) */

#define  NIDAQmxPan                       5
#define  NIDAQmxPan_Plate                 2       /* control type: deco, callback function: (none) */
#define  NIDAQmxPan_Devices               3       /* control type: tab, callback function: (none) */

#define  RefTrig1                         6
#define  RefTrig1_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig1_Slope                   3       /* control type: ring, callback function: (none) */
#define  RefTrig1_TrigType                4       /* control type: ring, callback function: (none) */
#define  RefTrig1_Duration                5       /* control type: numeric, callback function: (none) */
#define  RefTrig1_NSamples                6       /* control type: numeric, callback function: (none) */
#define  RefTrig1_Source                  7       /* control type: string, callback function: (none) */

#define  RefTrig2                         7
#define  RefTrig2_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig2_TrigType                3       /* control type: ring, callback function: (none) */
#define  RefTrig2_NSamples                4       /* control type: numeric, callback function: (none) */
#define  RefTrig2_Duration                5       /* control type: numeric, callback function: (none) */

#define  RefTrig3                         8
#define  RefTrig3_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig3_Slope                   3       /* control type: ring, callback function: (none) */
#define  RefTrig3_Source                  4       /* control type: string, callback function: (none) */
#define  RefTrig3_NSamples                5       /* control type: numeric, callback function: (none) */
#define  RefTrig3_Duration                6       /* control type: numeric, callback function: (none) */
#define  RefTrig3_Level                   7       /* control type: numeric, callback function: (none) */
#define  RefTrig3_TrigType                8       /* control type: ring, callback function: (none) */

#define  RefTrig4                         9
#define  RefTrig4_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig4_Source                  3       /* control type: string, callback function: (none) */
#define  RefTrig4_NSamples                4       /* control type: numeric, callback function: (none) */
#define  RefTrig4_Duration                5       /* control type: numeric, callback function: (none) */
#define  RefTrig4_TrigType                6       /* control type: ring, callback function: (none) */
#define  RefTrig4_WndType                 7       /* control type: ring, callback function: (none) */
#define  RefTrig4_TrigWndTop              8       /* control type: numeric, callback function: (none) */
#define  RefTrig4_TrigWndBttm             9       /* control type: numeric, callback function: (none) */

#define  StartTrig1                       10
#define  StartTrig1_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig1_Slope                 3       /* control type: ring, callback function: (none) */
#define  StartTrig1_Source                4       /* control type: string, callback function: (none) */
#define  StartTrig1_TrigType              5       /* control type: ring, callback function: (none) */

#define  StartTrig2                       11
#define  StartTrig2_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig2_TrigType              3       /* control type: ring, callback function: (none) */

#define  StartTrig3                       12
#define  StartTrig3_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig3_Slope                 3       /* control type: ring, callback function: (none) */
#define  StartTrig3_Source                4       /* control type: string, callback function: (none) */
#define  StartTrig3_Level                 5       /* control type: numeric, callback function: (none) */
#define  StartTrig3_TrigType              6       /* control type: ring, callback function: (none) */

#define  StartTrig4                       13
#define  StartTrig4_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig4_Source                3       /* control type: string, callback function: (none) */
#define  StartTrig4_TrigType              4       /* control type: ring, callback function: (none) */
#define  StartTrig4_WndType               5       /* control type: ring, callback function: (none) */
#define  StartTrig4_TrigWndTop            6       /* control type: numeric, callback function: (none) */
#define  StartTrig4_TrigWndBttm           7       /* control type: numeric, callback function: (none) */

#define  TaskSetPan                       14
#define  TaskSetPan_DAQTasks              2       /* control type: tab, callback function: (none) */
#define  TaskSetPan_Wait                  3       /* control type: numeric, callback function: (none) */
#define  TaskSetPan_Plate                 4       /* control type: deco, callback function: (none) */
#define  TaskSetPan_Repeat                5       /* control type: numeric, callback function: (none) */
#define  TaskSetPan_TotalIterations       6       /* control type: numeric, callback function: (none) */
#define  TaskSetPan_PhysChan              7       /* control type: listBox, callback function: (none) */
#define  TaskSetPan_IOType                8       /* control type: ring, callback function: (none) */
#define  TaskSetPan_IOMode                9       /* control type: ring, callback function: (none) */
#define  TaskSetPan_Mode                  10      /* control type: radioButton, callback function: (none) */
#define  TaskSetPan_IO                    11      /* control type: ring, callback function: (none) */

     /* tab page panel controls */
#define  Chan_ChanSet                     2       /* control type: tab, callback function: (none) */

     /* tab page panel controls */
#define  Set_BlockSize                    2       /* control type: numeric, callback function: (none) */
#define  Set_SamplingRate                 3       /* control type: numeric, callback function: (none) */
#define  Set_Plate                        4       /* control type: deco, callback function: (none) */
#define  Set_Duration                     5       /* control type: numeric, callback function: (none) */
#define  Set_NSamples                     6       /* control type: numeric, callback function: (none) */
#define  Set_MeasMode                     7       /* control type: ring, callback function: (none) */
#define  Set_UseRefSampRate               8       /* control type: radioButton, callback function: (none) */
#define  Set_UseRefNSamples               9       /* control type: radioButton, callback function: (none) */

     /* tab page panel controls */
#define  Timing_RefClkSource              2       /* control type: string, callback function: (none) */
#define  Timing_Plate                     3       /* control type: deco, callback function: (none) */
#define  Timing_SampleClkSource           4       /* control type: string, callback function: (none) */
#define  Timing_RefClkFreq                5       /* control type: numeric, callback function: (none) */
#define  Timing_Timeout                   6       /* control type: numeric, callback function: (none) */

     /* tab page panel controls */
#define  Trig_TrigSet                     2       /* control type: tab, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK ManageDevices_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif