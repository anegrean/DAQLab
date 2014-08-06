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
#define  AIAOChSet_Terminal               2       /* control type: ring, callback function: (none) */
#define  AIAOChSet_VChanName              3       /* control type: string, callback function: (none) */
#define  AIAOChSet_Range                  4       /* control type: ring, callback function: (none) */
#define  AIAOChSet_OnDemand               5       /* control type: radioButton, callback function: (none) */

#define  AIAOTskSet                       2
#define  AIAOTskSet_Tab                   2       /* control type: tab, callback function: (none) */

#define  DevListPan                       3
#define  DevListPan_DAQTable              2       /* control type: table, callback function: (none) */
#define  DevListPan_DoneBTTN              3       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_AddBTTN               4       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_Plate                 5       /* control type: deco, callback function: (none) */

#define  DIDOChSet                        4

#define  NIDAQmxPan                       5
#define  NIDAQmxPan_Devices               2       /* control type: tab, callback function: (none) */

#define  RefTrig1                         6
#define  RefTrig1_Slope                   2       /* control type: ring, callback function: (none) */
#define  RefTrig1_TrigType                3       /* control type: ring, callback function: (none) */
#define  RefTrig1_Duration                4       /* control type: numeric, callback function: (none) */
#define  RefTrig1_NSamples                5       /* control type: numeric, callback function: (none) */
#define  RefTrig1_Source                  6       /* control type: string, callback function: (none) */

#define  RefTrig2                         7
#define  RefTrig2_TrigType                2       /* control type: ring, callback function: (none) */
#define  RefTrig2_NSamples                3       /* control type: numeric, callback function: (none) */
#define  RefTrig2_Duration                4       /* control type: numeric, callback function: (none) */

#define  RefTrig3                         8
#define  RefTrig3_Source                  2       /* control type: string, callback function: (none) */
#define  RefTrig3_NSamples                3       /* control type: numeric, callback function: (none) */
#define  RefTrig3_Duration                4       /* control type: numeric, callback function: (none) */
#define  RefTrig3_Level                   5       /* control type: numeric, callback function: (none) */
#define  RefTrig3_TrigType                6       /* control type: ring, callback function: (none) */

#define  RefTrig4                         9
#define  RefTrig4_Source                  2       /* control type: string, callback function: (none) */
#define  RefTrig4_NSamples                3       /* control type: numeric, callback function: (none) */
#define  RefTrig4_Duration                4       /* control type: numeric, callback function: (none) */
#define  RefTrig4_TrigType                5       /* control type: ring, callback function: (none) */
#define  RefTrig4_WndType                 6       /* control type: ring, callback function: (none) */
#define  RefTrig4_TrigWndTop              7       /* control type: numeric, callback function: (none) */
#define  RefTrig4_TrigWndBttm             8       /* control type: numeric, callback function: (none) */

#define  StartTrig1                       10
#define  StartTrig1_Slope                 2       /* control type: ring, callback function: (none) */
#define  StartTrig1_Source                3       /* control type: string, callback function: (none) */
#define  StartTrig1_TrigType              4       /* control type: ring, callback function: (none) */

#define  StartTrig2                       11
#define  StartTrig2_TrigType              2       /* control type: ring, callback function: (none) */

#define  StartTrig3                       12
#define  StartTrig3_Source                2       /* control type: string, callback function: (none) */
#define  StartTrig3_Level                 3       /* control type: numeric, callback function: (none) */
#define  StartTrig3_TrigType              4       /* control type: ring, callback function: (none) */

#define  StartTrig4                       13
#define  StartTrig4_Source                2       /* control type: string, callback function: (none) */
#define  StartTrig4_TrigType              3       /* control type: ring, callback function: (none) */
#define  StartTrig4_WndType               4       /* control type: ring, callback function: (none) */
#define  StartTrig4_TrigWndTop            5       /* control type: numeric, callback function: (none) */
#define  StartTrig4_TrigWndBttm           6       /* control type: numeric, callback function: (none) */

#define  TaskSetPan                       14
#define  TaskSetPan_DAQTasks              2       /* control type: tab, callback function: (none) */
#define  TaskSetPan_PhysChan              3       /* control type: listBox, callback function: (none) */
#define  TaskSetPan_IOType                4       /* control type: ring, callback function: (none) */
#define  TaskSetPan_IOMode                5       /* control type: ring, callback function: (none) */
#define  TaskSetPan_IO                    6       /* control type: ring, callback function: (none) */

     /* tab page panel controls */
#define  Chan_ChanSet                     2       /* control type: tab, callback function: (none) */

     /* tab page panel controls */
#define  Set_BlockSize                    2       /* control type: numeric, callback function: (none) */
#define  Set_SamplingRate                 3       /* control type: numeric, callback function: (none) */
#define  Set_Duration                     4       /* control type: numeric, callback function: (none) */
#define  Set_NSamples                     5       /* control type: numeric, callback function: (none) */
#define  Set_MeasMode                     6       /* control type: ring, callback function: (none) */
#define  Set_UseRefSampRate               7       /* control type: radioButton, callback function: (none) */
#define  Set_UseRefNSamples               8       /* control type: radioButton, callback function: (none) */

     /* tab page panel controls */
#define  Timing_RefClkSource              2       /* control type: string, callback function: (none) */
#define  Timing_SampleClkSource           3       /* control type: string, callback function: (none) */
#define  Timing_RefClkFreq                4       /* control type: numeric, callback function: (none) */
#define  Timing_Timeout                   5       /* control type: numeric, callback function: (none) */

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
