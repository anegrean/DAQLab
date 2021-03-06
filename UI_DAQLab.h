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

#define  DSMainPan                        1
#define  DSMainPan_Channels               2       /* control type: listBox, callback function: (none) */
#define  DSMainPan_DeleteChannel          3       /* control type: command, callback function: (none) */
#define  DSMainPan_AddChannel             4       /* control type: command, callback function: (none) */
#define  DSMainPan_Close                  5       /* control type: command, callback function: (none) */
#define  DSMainPan_FilePath               6       /* control type: string, callback function: (none) */
#define  DSMainPan_ChangeFilePath         7       /* control type: command, callback function: (none) */
#define  DSMainPan_OverwriteFile          8       /* control type: radioButton, callback function: (none) */

#define  LogPan                           2
#define  LogPan_LogBox                    2       /* control type: textBox, callback function: (none) */

#define  MainPan                          3       /* callback function: CB_DAQLab_MainPan */

#define  ModulesPan                       4
#define  ModulesPan_Plate                 2       /* control type: deco, callback function: (none) */
#define  ModulesPan_Loaded                3       /* control type: listBox, callback function: DAQLab_ManageDAQLabModules_CB */
#define  ModulesPan_Available             4       /* control type: listBox, callback function: DAQLab_ManageDAQLabModules_CB */
#define  ModulesPan_Close                 5       /* control type: command, callback function: CloseDAQLabModulesPan_CB */

#define  TaskLogPan                       5
#define  TaskLogPan_LogBox                2       /* control type: textBox, callback function: (none) */

#define  TaskPan                          6
#define  TaskPan_Plate                    2       /* control type: deco, callback function: (none) */
#define  TaskPan_TaskTree                 3       /* control type: tree, callback function: TaskTree_CB */
#define  TaskPan_Close                    4       /* control type: command, callback function: TaskTree_CB */
#define  TaskPan_ExecMode                 5       /* control type: ring, callback function: TaskTree_CB */
#define  TaskPan_Switchboards             6       /* control type: tab, callback function: (none) */

#define  TasksPan                         7

#define  TCDelPan                         8       /* callback function: DAQLab_TCDelPan_CB */
#define  TCDelPan_Plate                   2       /* control type: deco, callback function: (none) */
#define  TCDelPan_TaskControllers         3       /* control type: listBox, callback function: (none) */
#define  TCDelPan_DelBTTN                 4       /* control type: command, callback function: DAQLab_DelTaskControllersBTTN_CB */

     /* tab page panel controls */
#define  HWTrigTab_Switchboard            2       /* control type: table, callback function: HWTriggersSwitchboard_CB */

     /* tab page panel controls */
#define  VChanTab_Switchboard             2       /* control type: table, callback function: VChanSwitchboard_CB */


     /* Control Arrays: */

          /* (no control arrays in the resource file) */


     /* Menu Bars, Menus, and Menu Items: */

#define  LogBoxMenu                       1
#define  LogBoxMenu_TaskLog               2       /* callback function: LogPanTaskLogMenu_CB */

#define  MainMenu                         2
#define  MainMenu_Modules                 2       /* callback function: DAQLab_ModulesMenu_CB */
#define  MainMenu_TaskManager             3       /* callback function: DAQLab_DisplayTaskManagerMenu_CB */
#define  MainMenu_Config                  4
#define  MainMenu_Config_Save             5       /* callback function: DAQLab_SaveCfg_CB */

#define  TaskLog                          3
#define  TaskLog_Close                    2       /* callback function: TaskLogMenuClose_CB */


     /* Callback Prototypes: */

int  CVICALLBACK CB_DAQLab_MainPan(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK CloseDAQLabModulesPan_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK DAQLab_DelTaskControllersBTTN_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK DAQLab_DisplayTaskManagerMenu_CB(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK DAQLab_ManageDAQLabModules_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK DAQLab_ModulesMenu_CB(int menubar, int menuItem, void *callbackData, int panel);
void CVICALLBACK DAQLab_SaveCfg_CB(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK DAQLab_TCDelPan_CB(int panel, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK HWTriggersSwitchboard_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
void CVICALLBACK LogPanTaskLogMenu_CB(int menubar, int menuItem, void *callbackData, int panel);
void CVICALLBACK TaskLogMenuClose_CB(int menubar, int menuItem, void *callbackData, int panel);
int  CVICALLBACK TaskTree_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
int  CVICALLBACK VChanSwitchboard_CB(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


#ifdef __cplusplus
    }
#endif
