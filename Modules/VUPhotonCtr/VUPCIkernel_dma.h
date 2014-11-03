 /*-----------------------------------------------------------------------------------------*/
 /*                                                                                         */
 /* FILE:    VU_PCIkernel.h                                                                 */
 /*                                                                                         */
 /* PURPOSE: This is the header file for VU_PCI_kernel.dll.  				    */
 /*-----------------------------------------------------------------------------------------*/


// VUPCI_Open needs to be called as first function
// and opens the kernel driver
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Open(void);
#ifdef __cplusplus
}
#endif

// VUPCI_Close frees the dll handles and closes the kernel driver
#ifdef __cplusplus
extern "C" {
#endif
void __stdcall VUPCI_Close(void);
#ifdef __cplusplus
}
#endif

// VUPCI_Reset resets the VUPCI device
// Returns FALSE if failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Reset(void);
#ifdef __cplusplus
}
#endif


// VUPCI_TrigEn enables triggering of the HW VUPCI device
// by setting the trigen bit in the control register
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_TrigEn(void);
#ifdef __cplusplus
}
#endif

// VUPCI_TrigDis disables triggering of the HW VUPCI device
// by clearing the trigen bit in the control register
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_TrigDis(void);
#ifdef __cplusplus
}
#endif


// VUPCI_Set_Reg sets the HW VUPCI register value
// by setting the register value
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Set_Reg(unsigned long offset,unsigned long data) ;
#ifdef __cplusplus
}
#endif

// VUPCI_Get_Reg gets the HW VUPCI register value
// by getting the register value
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Get_Reg(unsigned long offset,unsigned long *data)  ;
#ifdef __cplusplus
}
#endif


// VUPCI_Set_Read_Timeout sets DMA read timeout im ms
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Set_Read_Timeout(unsigned long timeout_in_ms);
#ifdef __cplusplus
}
#endif


// VUPCI_Read_Buffer requests numbytes from the available HW
// Returns NULL when failed, else bulkbuffer pointer
#ifdef __cplusplus
extern "C" {
#endif
long  __stdcall VUPCI_Read_Buffer(unsigned short int *bufptr,int numbytes) ;
#ifdef __cplusplus
}
#endif


// VUPCI_Get_DriverInfo returns the Driver version info
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Get_DriverInfo(unsigned long *majorversion,unsigned long *minorversion);
#ifdef __cplusplus
}
#endif

// VUPCI_Get_Status returns the Driver and hardware status
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Get_Status(unsigned long *driverstatus);
#ifdef __cplusplus
}
#endif


// VUPCI_Start_DMA starts the DMA transfers
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Start_DMA(void);
#ifdef __cplusplus
}
#endif


// VUPCI_Stop_DMA stops the DMA transfers
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Stop_DMA(void);
#ifdef __cplusplus
}
#endif

// VUPCI_Set_DTE_Size passes the DTE size to the driver
// returns FALSE when failed
#ifdef __cplusplus
extern "C" {
#endif
BOOL __stdcall VUPCI_Set_DTE_Size(unsigned long DTESize) ;
#ifdef __cplusplus
}
#endif
