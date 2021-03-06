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

#define  ADTskSet                         1
#define  ADTskSet_Tab                     2       /* control type: tab, callback function: (none) */

#define  AIVoltage                        2
#define  AIVoltage_Plate                  2       /* control type: deco, callback function: (none) */
#define  AIVoltage_Terminal               3       /* control type: ring, callback function: (none) */
#define  AIVoltage_VChanName              4       /* control type: string, callback function: (none) */
#define  AIVoltage_ScaleMax               5       /* control type: numeric, callback function: (none) */
#define  AIVoltage_Gain                   6       /* control type: numeric, callback function: (none) */
#define  AIVoltage_Offset                 7       /* control type: numeric, callback function: (none) */
#define  AIVoltage_ScaleMin               8       /* control type: numeric, callback function: (none) */
#define  AIVoltage_AIDataType             9       /* control type: ring, callback function: (none) */
#define  AIVoltage_Range                  10      /* control type: ring, callback function: (none) */
#define  AIVoltage_OnDemand               11      /* control type: radioButton, callback function: (none) */
#define  AIVoltage_Integrate              12      /* control type: radioButton, callback function: (none) */

#define  AOVoltage                        3
#define  AOVoltage_Plate                  2       /* control type: deco, callback function: (none) */
#define  AOVoltage_Terminal               3       /* control type: ring, callback function: (none) */
#define  AOVoltage_VChanName              4       /* control type: string, callback function: (none) */
#define  AOVoltage_Range                  5       /* control type: ring, callback function: (none) */
#define  AOVoltage_OnDemand               6       /* control type: radioButton, callback function: (none) */

#define  CICOChSet                        4
#define  CICOChSet_TAB                    2       /* control type: tab, callback function: (none) */

#define  CICOTskSet                       5
#define  CICOTskSet_ChanSet               2       /* control type: tab, callback function: (none) */

#define  COFreqPan                        6
#define  COFreqPan_NPulses                2       /* control type: numeric, callback function: (none) */
#define  COFreqPan_Mode                   3       /* control type: ring, callback function: (none) */
#define  COFreqPan_Plate                  4       /* control type: deco, callback function: (none) */
#define  COFreqPan_IdleState              5       /* control type: ring, callback function: (none) */
#define  COFreqPan_Frequency              6       /* control type: numeric, callback function: (none) */
#define  COFreqPan_InitialDelay           7       /* control type: numeric, callback function: (none) */
#define  COFreqPan_DutyCycle              8       /* control type: numeric, callback function: (none) */

#define  COTicksPan                       7
#define  COTicksPan_NPulses               2       /* control type: numeric, callback function: (none) */
#define  COTicksPan_Mode                  3       /* control type: ring, callback function: (none) */
#define  COTicksPan_Plate                 4       /* control type: deco, callback function: (none) */
#define  COTicksPan_IdleState             5       /* control type: ring, callback function: (none) */
#define  COTicksPan_LowTicks              6       /* control type: numeric, callback function: (none) */
#define  COTicksPan_InitialDelay          7       /* control type: numeric, callback function: (none) */
#define  COTicksPan_HighTicks             8       /* control type: numeric, callback function: (none) */

#define  COTimePan                        8
#define  COTimePan_NPulses                2       /* control type: numeric, callback function: (none) */
#define  COTimePan_Mode                   3       /* control type: ring, callback function: (none) */
#define  COTimePan_Plate                  4       /* control type: deco, callback function: (none) */
#define  COTimePan_IdleState              5       /* control type: ring, callback function: (none) */
#define  COTimePan_LowTime                6       /* control type: numeric, callback function: (none) */
#define  COTimePan_InitialDelay           7       /* control type: numeric, callback function: (none) */
#define  COTimePan_HighTime               8       /* control type: numeric, callback function: (none) */

#define  DevListPan                       9
#define  DevListPan_DAQTable              2       /* control type: table, callback function: ManageDevices_CB */
#define  DevListPan_DoneBTTN              3       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_AddBTTN               4       /* control type: command, callback function: ManageDevices_CB */
#define  DevListPan_Plate                 5       /* control type: deco, callback function: (none) */

#define  DIDOLChSet                       10
#define  DIDOLChSet_Plate                 2       /* control type: deco, callback function: (none) */
#define  DIDOLChSet_VChanName             3       /* control type: string, callback function: (none) */
#define  DIDOLChSet_Invert                4       /* control type: radioButton, callback function: (none) */
#define  DIDOLChSet_OutputBTTN            5       /* control type: radioButton, callback function: (none) */
#define  DIDOLChSet_OnDemand              6       /* control type: radioButton, callback function: (none) */

#define  DIDOPChSet                       11
#define  DIDOPChSet_Plate                 2       /* control type: deco, callback function: (none) */
#define  DIDOPChSet_VChanName             3       /* control type: string, callback function: (none) */
#define  DIDOPChSet_Invert                4       /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_7         5       /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_OnDemand              6       /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_6         7       /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_5         8       /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_4         9       /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_3         10      /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_2         11      /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_1         12      /* control type: radioButton, callback function: (none) */
#define  DIDOPChSet_RADIOBUTTON_0         13      /* control type: radioButton, callback function: (none) */

#define  NIDAQmxPan                       12
#define  NIDAQmxPan_Plate                 2       /* control type: deco, callback function: (none) */
#define  NIDAQmxPan_Devices               3       /* control type: tab, callback function: (none) */

#define  RefTrig1                         13
#define  RefTrig1_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig1_Slope                   3       /* control type: ring, callback function: (none) */
#define  RefTrig1_TrigType                4       /* control type: ring, callback function: (none) */
#define  RefTrig1_Duration                5       /* control type: numeric, callback function: (none) */
#define  RefTrig1_Source                  6       /* control type: string, callback function: (none) */
#define  RefTrig1_NSamples                7       /* control type: numeric, callback function: (none) */

#define  RefTrig2                         14
#define  RefTrig2_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig2_TrigType                3       /* control type: ring, callback function: (none) */
#define  RefTrig2_NSamples                4       /* control type: numeric, callback function: (none) */
#define  RefTrig2_Duration                5       /* control type: numeric, callback function: (none) */

#define  RefTrig3                         15
#define  RefTrig3_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig3_Slope                   3       /* control type: ring, callback function: (none) */
#define  RefTrig3_Source                  4       /* control type: string, callback function: (none) */
#define  RefTrig3_NSamples                5       /* control type: numeric, callback function: (none) */
#define  RefTrig3_Duration                6       /* control type: numeric, callback function: (none) */
#define  RefTrig3_Level                   7       /* control type: numeric, callback function: (none) */
#define  RefTrig3_TrigType                8       /* control type: ring, callback function: (none) */

#define  RefTrig4                         16
#define  RefTrig4_Plate                   2       /* control type: deco, callback function: (none) */
#define  RefTrig4_Source                  3       /* control type: string, callback function: (none) */
#define  RefTrig4_NSamples                4       /* control type: numeric, callback function: (none) */
#define  RefTrig4_Duration                5       /* control type: numeric, callback function: (none) */
#define  RefTrig4_TrigType                6       /* control type: ring, callback function: (none) */
#define  RefTrig4_WndType                 7       /* control type: ring, callback function: (none) */
#define  RefTrig4_TrigWndTop              8       /* control type: numeric, callback function: (none) */
#define  RefTrig4_TrigWndBttm             9       /* control type: numeric, callback function: (none) */

#define  StartTrig1                       17
#define  StartTrig1_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig1_Slope                 3       /* control type: ring, callback function: (none) */
#define  StartTrig1_Source                4       /* control type: string, callback function: (none) */
#define  StartTrig1_TrigType              5       /* control type: ring, callback function: (none) */

#define  StartTrig2                       18
#define  StartTrig2_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig2_TrigType              3       /* control type: ring, callback function: (none) */

#define  StartTrig3                       19
#define  StartTrig3_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig3_Slope                 3       /* control type: ring, callback function: (none) */
#define  StartTrig3_Source                4       /* control type: string, callback function: (none) */
#define  StartTrig3_Level                 5       /* control type: numeric, callback function: (none) */
#define  StartTrig3_TrigType              6       /* control type: ring, callback function: (none) */

#define  StartTrig4                       20
#define  StartTrig4_Plate                 2       /* control type: deco, callback function: (none) */
#define  StartTrig4_Source                3       /* control type: string, callback function: (none) */
#define  StartTrig4_TrigType              4       /* control type: ring, callback function: (none) */
#define  StartTrig4_WndType               5       /* control type: ring, callback function: (none) */
#define  StartTrig4_TrigWndTop            6       /* control type: numeric, callback function: (none) */
#define  StartTrig4_TrigWndBttm           7       /* control type: numeric, callback function: (none) */

#define  TaskSetPan                       21
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
#define  Set_AutoOversampling             3       /* control type: radioButton, callback function: (none) */
#define  Set_ActualSamplingRate           4       /* control type: numeric, callback function: (none) */
#define  Set_TargetSamplingRate           5       /* control type: numeric, callback function: (none) */
#define  Set_SamplingRate                 6       /* control type: numeric, callback function: (none) */
#define  Set_Plate                        7       /* control type: deco, callback function: (none) */
#define  Set_Duration                     8       /* control type: numeric, callback function: (none) */
#define  Set_NSamples                     9       /* control type: numeric, callback function: (none) */
#define  Set_MeasMode                     10      /* control type: ring, callback function: (none) */
#define  Set_Oversampling                 11      /* control type: numeric, callback function: (none) */

     /* tab page panel controls */
#define  Timing_RefClkSource              2       /* control type: string, callback function: (none) */
#define  Timing_Plate                     3       /* control type: deco, callback function: (none) */
#define  Timing_StartRouting              4       /* control type: string, callback function: (none) */
#define  Timing_SampleClkSource           5       /* control type: string, callback function: (none) */
#define  Timing_RefClkFreq                6       /* control type: numeric, callback function: (none) */
#define  Timing_Timeout                   7       /* control type: numeric, callback function: (none) */

     /* tab page panel controls */
#define  TimingPan_Plate                  2       /* control type: deco, callback function: (none) */
#define  TimingPan_ClockSource            3       /* control type: string, callback function: (none) */
#define  TimingPan_ClkFreq                4       /* control type: numeric, callback function: (none) */

     /* tab page panel controls */
#define  Trig_TrigSet                     2       /* control type: tab, callback function: (none) */

     /* tab page panel controls */
#define  TrigPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  TrigPan_Slope                    3       /* control type: ring, callback function: (none) */
#define  TrigPan_TrigType                 4       /* control type: ring, callback function: (none) */
#define  TrigPan_Source                   5       /* control type: string, callback function: (none) */

     /* tab page panel controls */
#define  VChanPan_Plate                   2       /* control type: deco, callback function: (none) */
#define  VChanPan_SinkVChanName           3       /* control type: string, callback function: (none) */
#define  VChanPan_SrcVChanName            4       /* control type: string, callback function: (none) */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

          /* (no menu bars in the resource file) */


     /* Callback Prototypes: */

int  CVICALLBACK ManageDevices_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
