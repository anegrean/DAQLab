//***********************************************************************************
//***********************************************************************************
//***********************************************************************************
//***********************************************************************************
//***********************************************************************************
//
//
//
//	delib.h
//	project: DELIB
//
//
//	(c) DEDITEC GmbH, 2009-2014
//	web: http://www.deditec.de/
//	mail: vertrieb@deditec.de
//
//
//***********************************************************************************
//***********************************************************************************
//***********************************************************************************
//***********************************************************************************
//***********************************************************************************

// all Modul-ID's
#define USB_Interface8											1			// USB-Controller8/USB-TTL-IN8-OUT8
#define USB_CAN_STICK											2			// USB-CAN-Stick
#define USB_LOGI_500											3			// USB-LOGI-500/USB-LOGI-250
#define USB_SER_DEBUG											4			// USB-SER-DEBUG
#define RO_SER													5			// RO-SER-Serie
#define USB_BITP_200											6			// USB-BITP-200
#define RO_USB1													7			// RO-USB-Serie
#define RO_USB													7			// RO-USB-Serie
#define RO_ETH													8			// RO-ETH-Serie
#define USB_MINI_STICK											9			// USB-MINI-Stick-Serie
#define USB_LOGI_18												10			// USB-LOGI-100
#define RO_CAN													11			// RO-CAN-Serie
#define USB_SPI_MON												12			// USB_SPI_MON
#define USB_WATCHDOG											13			// USB_Watchdog
#define USB_OPTOIN_8											14			// USB-OPTOIN8 / USB-RELAIS-8
#define USB_RELAIS_8											14			// USB-OPTOIN8 / USB-RELAIS-8
#define USB_OPTOIN_8_RELAIS_8									15			// USB-OPTOIN-8-RELAIS-8
#define USB_OPTOIN_16_RELAIS_16									16			// USB-OPTOIN-16-RELAIS-16
#define USB_OPTOIN_32											16			// USB-OPTOIN-16-RELAIS-16
#define USB_RELAIS_32											16			// USB-OPTOIN-16-RELAIS-16
#define USB_OPTOIN_32_RELAIS_32									17			// USB-OPTOIN-32-RELAIS-32
#define USB_OPTOIN_64											17			// USB-OPTOIN-32-RELAIS-32
#define USB_RELAIS_64											17			// USB-OPTOIN-32-RELAIS-32

#define BS_USB_8												15
#define BS_USB_16												16
#define BS_USB_32												17

#define USB_TTL_32												18			// USB-TTL-32
#define USB_TTL_64												18			// USB-TTL-64
#define RO_ETH_INTERN											19

#define BS_SER													20
#define BS_CAN													21
#define BS_ETH													22

#define NET_ETH													23

#define RO_CAN2													24			// RO-CPU2 / 480 MBit/sec - CAN VERSION
#define RO_USB2													25			// RO-CPU2 / 480 MBit/sec - USB/SER Version
#define RO_ETH_LC												26			// RO-ETH-LC
//
#define ETH_RELAIS_8											27
#define ETH_OPTOIN_8											27
#define ETH_O4_R4_ADDA											28
//
#define ETHERNET_MODULE											29
//
#define ETH_TTL_64												30
//
#define NET_USB													31

#define MAX_NR_OF_MODULES 										31
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Declarations

#ifndef _MSC_VER
	#define UCHAR unsigned char
	#define ULONG unsigned long
	#define ULONGLONG unsigned long long
#endif

#ifdef __linux__
	#define DAPI_FUNCTION_PRE	
#else
	#define DAPI_FUNCTION_PRE	__stdcall
#endif

#ifdef _WIN64
	#define DAPI_FUNCTION_PRE64						extern "C" _declspec(dllexport)
#else
	#define DAPI_FUNCTION_PRE64						extern
#endif

// --------------------------------------------------------
// ERROR Codes
#define DAPI_ERR_NONE											0
#define DAPI_ERR_DEVICE_NOT_FOUND								-1
#define DAPI_ERR_COMMUNICATION_ERROR							-2
#define DAPI_ERR_ILLEGAL_HANDLE									-10
#define DAPI_ERR_FUNCTION_NOT_DEFINED							-11
#define DAPI_ERR_ILLEGAL_COM_HANDLE								-12
#define DAPI_ERR_ILLEGAL_MODE									-13
#define DAPI_ERR_WITH_TEXT										-14
#define DAPI_ERR_SW_FEATURE_NOT_SUPPORTED						-15
#define DAPI_ERR_ILLEGAL_IO_TYPE								-16
#define DAPI_ERR_ILLEGAL_CHANNEL								-17

// --------------------------------------------------------
// Special Function-Codes
#define DAPI_SPECIAL_CMD_GET_MODULE_CONFIG						1
#define DAPI_SPECIAL_CMD_TIMEOUT								2
#define DAPI_SPECIAL_CMD_DI										10
#define DAPI_SPECIAL_CMD_SET_DIR_DX_1							3
#define DAPI_SPECIAL_CMD_SET_DIR_DX_8							4
#define DAPI_SPECIAL_CMD_GET_MODULE_VERSION						5
#define DAPI_SPECIAL_CMD_DA										6
#define DAPI_SPECIAL_CMD_WATCHDOG								7
#define DAPI_SPECIAL_CMD_COUNTER								8
#define DAPI_SPECIAL_CMD_AD										9
#define DAPI_SPECIAL_CMD_CNT48									11
#define DAPI_SPECIAL_CMD_SOFTWARE_FIFO							12
#define DAPI_SPECIAL_CMD_MODULE_REBOOT							13
#define DAPI_SPECIAL_CMD_MODULE_RESCAN							14
#define DAPI_SPECIAL_CMD_RESTART_CHECK_MODULE_CONFIG			15

// values for PAR1
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_DI					1
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_DI_FF				7
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_DI_COUNTER			8
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_DO					2
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_DX					3
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_AD					4
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_DA					5
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_TEMP					9
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_STEPPER				6
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_CNT48				10
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_PULSE_GEN			11
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_PWM_OUT				12
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_HW_INTERFACE1		13
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_SW_FEATURE1			14
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_HW_GROUP				15
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_SW_CLASS				16
#define DAPI_SPECIAL_GET_MODULE_CONFIG_PAR_MODULE_ID			17
//
#define DAPI_SPECIAL_GET_MODULE_PAR_VERSION_0					0
#define DAPI_SPECIAL_GET_MODULE_PAR_VERSION_1					1
#define DAPI_SPECIAL_GET_MODULE_PAR_VERSION_2					2
#define DAPI_SPECIAL_GET_MODULE_PAR_VERSION_3					3
//
#define DAPI_SPECIAL_TIMEOUT_SET_VALUE_SEC						1
#define DAPI_SPECIAL_TIMEOUT_ACTIVATE							2
#define DAPI_SPECIAL_TIMEOUT_DEACTIVATE							3
#define DAPI_SPECIAL_TIMEOUT_GET_STATUS							4
//
#define DAPI_SPECIAL_DI_FF_FILTER_VALUE_SET						1
#define DAPI_SPECIAL_DI_FF_FILTER_VALUE_GET						2
//
#define DAPI_SPECIAL_AD_READ_MULTIPLE_AD						1
#define DAPI_SPECIAL_AD_FIFO_ACTIVATE							2
#define DAPI_SPECIAL_AD_FIFO_DEACTIVATE							3
#define DAPI_SPECIAL_AD_FIFO_GET_STATUS							4
#define DAPI_SPECIAL_AD_FIFO_SET_INTERVAL_MS					5
#define DAPI_SPECIAL_AD_FIFO_SET_CHANNEL						6
#define DAPI_SPECIAL_AD_FIFO_INIT								7
#define DAPI_SPECIAL_AD_FILTER_SET								8
//
#define DAPI_SPECIAL_DA_PAR_DA_LOAD_DEFAULT						1
#define DAPI_SPECIAL_DA_PAR_DA_SAVE_EEPROM_CONFIG				2
#define DAPI_SPECIAL_DA_PAR_DA_LOAD_EEPROM_CONFIG				3
//
#define DAPI_SPECIAL_WATCHDOG_SET_TIMEOUT_MSEC					1
#define DAPI_SPECIAL_WATCHDOG_GET_TIMEOUT_MSEC					2
#define DAPI_SPECIAL_WATCHDOG_GET_STATUS						3
#define DAPI_SPECIAL_WATCHDOG_GET_WD_COUNTER_MSEC				4
#define DAPI_SPECIAL_WATCHDOG_GET_TIMEOUT_RELAIS_COUNTER_MSEC	5
#define DAPI_SPECIAL_WATCHDOG_SET_TIMEOUT_REL1_COUNTER_MSEC		6
#define DAPI_SPECIAL_WATCHDOG_SET_TIMEOUT_REL2_COUNTER_MSEC		7
//
#define DAPI_SPECIAL_COUNTER_LATCH_ALL							1
#define DAPI_SPECIAL_COUNTER_LATCH_ALL_WITH_RESET				2
//
#define DAPI_SPECIAL_CNT48_RESET_SINGLE							1
#define DAPI_SPECIAL_CNT48_RESET_GROUP8							2
#define DAPI_SPECIAL_CNT48_LATCH_GROUP8							3
#define DAPI_SPECIAL_CNT48_DI_GET1								4
//
#define DAPI_SPECIAL_SOFTWARE_FIFO_ACTIVATE						1
#define DAPI_SPECIAL_SOFTWARE_FIFO_DEACTIVATE					2
#define DAPI_SPECIAL_SOFTWARE_FIFO_GET_STATUS					3

// values for PAR2
#define DAPI_SPECIAL_AD_CH0_CH15								0
#define DAPI_SPECIAL_AD_CH16_CH31								1

// --------------------------------------------------------
// DapiScanModules-Codes
#define DAPI_SCANMODULE_GET_MODULES_AVAILABLE					1

// ----------------------------------------------------------------------------
// SOFTWARE Features Class
#define DAPI_SW_CLASS_TYPE_OF_MODULE_IDENTIFICATION				(1<<0)			// Bit 0
#define DAPI_SW_CLASS_DI_DO_DX_CHANNELS							(1<<1)			// Bit 1
#define DAPI_SW_CLASS_AD_DA_CHANNELS							(1<<2)			// Bit 2

// ----------------------------------------------------------------------------
// Hardware Group
#define DAPI_HW_GROUP_RO										(1<<0)			// Bit 0
#define DAPI_HW_GROUP_LOGICANALYZER								(1<<1)			// Bit 1
#define DAPI_HW_GROUP_DI_DO_AD_DA								(1<<2)			// Bit 2
#define DAPI_HW_GROUP_USB										(1<<3)			// Bit 3
#define DAPI_HW_GROUP_BS										(1<<4)			// Bit 4
#define DAPI_HW_GROUP_NET										(1<<5)			// Bit 5
#define DAPI_HW_GROUP_ETH										(1<<6)			// Bit 6

// --------------------------------------------------------
// Software Feature Bits
#define DAPI_SW_FEATURE_BIT_SUPPORTED_BY_FIRMWARE				0x00000001
#define DAPI_SW_FEATURE_BIT_SOFTWARE_FIFO						0x00000002
#define DAPI_SW_FEATURE_BIT_CFG_DO_CMD_SET_CLR_BIT_32			0x00000004
#define DAPI_SW_FEATURE_BIT_EEPROM_RN23							0x00000008
#define DAPI_SW_FEATURE_BIT_EEPROM_E2_2K						0x00000020
#define DAPI_SW_FEATURE_BIT_DX_1_MODE							0x00000010
#define DAPI_SW_FEATURE_BIT_AUTO_OUTPUTS_OFF_TIMEOUT			0x02000000
#define DAPI_SW_FEATURE_BIT_SUPP_INDIVIDUAL_CH_NAMES  			0x00001000
#define DAPI_SW_FEATURE_BIT_DEV_IO_REG_ERR_SUPP					0x00002000

#define DAPI_SW_FEATURE_BIT_CFG_WATCHDOG						0x10000000

#define DAPI_SW_FEATURE_BIT_CFG_DI								0x20000000
#define DAPI_SW_FEATURE_BIT_CFG_DI_CNT							0x40000000
#define DAPI_SW_FEATURE_BIT_CFG_DI_CNT_LATCH          			0x00080000
#define DAPI_SW_FEATURE_BIT_CFG_DI_FF							0x80000000

#define DAPI_SW_FEATURE_BIT_CFG_DO								0x01000000
#define DAPI_SW_FEATURE_BIT_CFG_DO_WITH_TIMER        			0x00040000

#define DAPI_SW_FEATURE_BIT_CFG_PWM_OUT							0x04000000

#define DAPI_SW_FEATURE_BIT_CFG_DX								0x08000000

#define DAPI_SW_FEATURE_BIT_CFG_DA								0x00100000
#define DAPI_SW_FEATURE_BIT_CFG_AD								0x00200000

#define DAPI_SW_FEATURE_BIT_CFG_CNT_OUT32						0x00400000
#define DAPI_SW_FEATURE_BIT_CFG_CNT_IN48						0x00800000

#define DAPI_SW_FEATURE_BIT_CFG_TEMP							0x00010000
#define DAPI_SW_FEATURE_BIT_CFG_STEPPER							0x00020000

// --------------------------------------------------------
// Hardware Interface Bits
#define DAPI_HW_INTERFACE_BIT_SUPPORTED_BY_FIRMWARE				0x00000001
#define DAPI_HW_INTERFACE_BIT_CFG_ETH							0x00000002
#define DAPI_HW_INTERFACE_BIT_CFG_CAN							0x00000004
#define DAPI_HW_INTERFACE_BIT_CFG_RS232							0x00000008
#define DAPI_HW_INTERFACE_BIT_CFG_RS232_RS485					0x00000010
#define DAPI_HW_INTERFACE_BIT_CFG_USB1							0x00000020
#define DAPI_HW_INTERFACE_BIT_CFG_USB2							0x00000040

#define DAPI_HW_INTERFACE_PRODUCT_ID_MASK						0xFF000000
#define DAPI_HW_INTERFACE_PRODUCT_ID_RO							0x01000000
#define DAPI_HW_INTERFACE_PRODUCT_ID_RO2						0x02000000
#define DAPI_HW_INTERFACE_PRODUCT_ID_BS							0x03000000
#define DAPI_HW_INTERFACE_PRODUCT_ID_NET						0x04000000
#define DAPI_HW_INTERFACE_PRODUCT_ID_NET_IP						0x05000000

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifdef __BORLANDC__
	#pragma message	("DELIB.H * BORLAND C Compiler used")
#endif

#ifdef _MSC_VER
	#pragma message	("DELIB.H * Microsoft C Compiler used")
#endif

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Prototypes for DELIB-Functions
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifdef __cplusplus
	extern "C"
	{
	#pragma message	("DELIB.H Mode for C++ used")

#else
	#pragma message	("DELIB.H - Mode for C used")
#endif

// --------------------------------------------------------
// 

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiOpenModule(ULONG moduleID, ULONG nr);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiOpenModuleEx(ULONG moduleID, ULONG nr, unsigned char* exbuffer, ULONG open_options);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiCloseModule(ULONG handle);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiScanModule(ULONG moduleID, ULONG cmd);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiScanAllModulesAvailable(ULONG todo);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiScanAllModulesAvailableListEcecute(unsigned long nr, unsigned long todo);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiGetDELIBVersion(ULONG mode, ULONG par);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiPing(ULONG handle, ULONG value);

// --------------------------------------------------------
// Register Access
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWriteByte(ULONG handle, ULONG adress, ULONG value);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWriteWord(ULONG handle, ULONG adress, ULONG value);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWriteLong(ULONG handle, ULONG adress, ULONG value);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWriteLongLong(ULONG handle, ULONG adress, ULONGLONG value);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiReadByte(ULONG handle, ULONG adress);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiReadWord(ULONG handle, ULONG adress);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiReadLong(ULONG handle, ULONG adress);
DAPI_FUNCTION_PRE64 ULONGLONG DAPI_FUNCTION_PRE DapiReadLongLong(ULONG handle, ULONG adress);

// --------------------------------------------------------
// Error Handling
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiGetLastError();
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiGetLastErrorText(UCHAR * msg, ULONG msg_length);
DAPI_FUNCTION_PRE64 void  DAPI_FUNCTION_PRE DapiClearLastError(void);

// --------------------------------------------------------
// Digital Inputs
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDIGet1(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDIGet8(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDIGet16(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDIGet32(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONGLONG DAPI_FUNCTION_PRE DapiDIGet64(ULONG handle, ULONG ch);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDIGetFF32(ULONG handle, ULONG ch);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDIGetCounter(ULONG handle, ULONG ch, ULONG mode);

// --------------------------------------------------------
// Digital Outputs
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSet1(ULONG handle, ULONG ch, ULONG data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSet8(ULONG handle, ULONG ch, ULONG data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSet16(ULONG handle, ULONG ch, ULONG data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSet32(ULONG handle, ULONG ch, ULONG data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSet64(ULONG handle, ULONG ch, ULONGLONG data);

DAPI_FUNCTION_PRE64 ULONG     DAPI_FUNCTION_PRE DapiDOReadback32(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONGLONG DAPI_FUNCTION_PRE DapiDOReadback64(ULONG handle, ULONG ch);

DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSet1_WithTimer(ULONG handle, ULONG ch, ULONG data, ULONG time_ms);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOSetBit32(ULONG handle, ULONG ch, ULONG data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDOClrBit32(ULONG handle, ULONG ch, ULONG data);

// --------------------------------------------------------
// Analog Inputs
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiADGet(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 float DAPI_FUNCTION_PRE DapiADGetValueModeUnit(ULONG handle, ULONG ch, ULONG *mode, ULONG *unit);
DAPI_FUNCTION_PRE64 float DAPI_FUNCTION_PRE DapiADGetVolt(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 float DAPI_FUNCTION_PRE DapiADGetmA(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 void  DAPI_FUNCTION_PRE DapiADSetMode(ULONG handle, ULONG ch, ULONG mode);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiADGetMode(ULONG handle, ULONG ch);

// --------------------------------------------------------
// Analog Outputs
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDASet(ULONG handle, ULONG ch, ULONG data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDASetVolt(ULONG handle, ULONG ch, float data);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiDASetmA(ULONG handle, ULONG ch, float data);
DAPI_FUNCTION_PRE64 float DAPI_FUNCTION_PRE DapiDAGetValueModeUnit(ULONG handle, ULONG ch, ULONG *mode, ULONG *unit);
DAPI_FUNCTION_PRE64 void  DAPI_FUNCTION_PRE DapiDASetMode(ULONG handle, ULONG ch, ULONG mode);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiDAGetMode(ULONG handle, ULONG ch);

// --------------------------------------------------------
// Temperature Inputs
DAPI_FUNCTION_PRE64 float DAPI_FUNCTION_PRE DapiTempGet(ULONG handle, ULONG ch);

// --------------------------------------------------------
// Counter48 Inputs
DAPI_FUNCTION_PRE64 void   DAPI_FUNCTION_PRE DapiCnt48ModeSet(ULONG handle, ULONG ch, ULONG mode);
DAPI_FUNCTION_PRE64 ULONG  DAPI_FUNCTION_PRE DapiCnt48ModeGet(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONG  DAPI_FUNCTION_PRE DapiCnt48CounterGet32(ULONG handle, ULONG ch);
DAPI_FUNCTION_PRE64 ULONGLONG  DAPI_FUNCTION_PRE DapiCnt48CounterGet48(ULONG handle, ULONG ch);

// --------------------------------------------------------
// Pulse-Generator Outputs
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiPulseGenSet(ULONG handle, ULONG ch, ULONG mode, ULONG par0, ULONG par1, ULONG par2);

// --------------------------------------------------------
// PWM Outputs
DAPI_FUNCTION_PRE64 void   DAPI_FUNCTION_PRE DapiPWMOutSet(ULONG handle, ULONG ch, float data);

// --------------------------------------------------------
// Stepper
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiStepperCommand(ULONG handle, ULONG motor, ULONG cmd, ULONG par1, ULONG par2, ULONG par3, ULONG par4);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiStepperCommandEx(ULONG handle, ULONG motor, ULONG cmd, ULONG par1, ULONG par2, ULONG par3, ULONG par4, ULONG par5, ULONG par6, ULONG par7);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiStepperGetStatus(ULONG handle, ULONG motor, ULONG cmd);

// --------------------------------------------------------
// Watchdog
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWatchdogEnable(ULONG handle);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWatchdogDisable(ULONG handle);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiWatchdogRetrigger(ULONG handle);

// --------------------------------------------------------
// Fifo
DAPI_FUNCTION_PRE64 unsigned long DAPI_FUNCTION_PRE DapiReadFifo(ULONG handle, ULONG type, UCHAR * buff, ULONG buff_len);

// --------------------------------------------------------
// CAN
typedef struct
{
	ULONG address;
	ULONG timestamp;
	ULONG use_extended_ids;
	ULONG rtr;
	ULONG data_count;
	UCHAR data[8];
} DAPI_CAN_MESSAGE_STRUCT;

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiCANCommand(ULONG handle, ULONG cmd, ULONG par1, ULONG par2, ULONG par3);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiCANGetPacket(ULONG handle, ULONG message_box_nr, DAPI_CAN_MESSAGE_STRUCT *can_msg);
DAPI_FUNCTION_PRE64 void DAPI_FUNCTION_PRE DapiCANSendPacket(ULONG handle, DAPI_CAN_MESSAGE_STRUCT *can_msg);

// --------------------------------------------------------
// Special
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiSpecialCommand(ULONG handle, ULONG cmd, ULONG par1, ULONG par2, ULONG par3);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiSpecialCommandExt(ULONG handle, ULONG cmd, ULONG par1, ULONG par2, ULONG par3, ULONG *par4, ULONG *par5, ULONG *par6, UCHAR *buff1, ULONG buff1_size, UCHAR *buff2, ULONG buff2_size, UCHAR *buff3, ULONG buff3_size, ULONG *buff3_length);

DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiReadMultipleBytes(ULONG handle, ULONG adress, ULONG adress_depth, ULONG increments, UCHAR * buff, ULONG buff_len);
DAPI_FUNCTION_PRE64 ULONG DAPI_FUNCTION_PRE DapiWriteMultipleBytes(ULONG handle, ULONG adress, ULONG adress_depth, ULONG increments, UCHAR * buff, ULONG buff_len);

// --------------------------------------------------------
#ifdef __cplusplus
	}
#endif

// --------------------------------------------------------
// DI - Counter Mode

#define DAPI_CNT_MODE_READ            							0x00
#define DAPI_CNT_MODE_READ_WITH_RESET							0x01
#define DAPI_CNT_MODE_READ_LATCHED								0x02

// --------------------------------------------------------
// A/D and D/A Modes

#define DAPI_ADDA_MODE_UNIPOL_10V								0x00
#define DAPI_ADDA_MODE_UNIPOL_5V								0x01
#define DAPI_ADDA_MODE_UNIPOL_2V5								0x02

#define DAPI_ADDA_MODE_BIPOL_10V								0x40
#define DAPI_ADDA_MODE_BIPOL_5V									0x41
#define DAPI_ADDA_MODE_BIPOL_2V5								0x42

#define DAPI_ADDA_MODE_0_20mA									0x80
#define DAPI_ADDA_MODE_4_20mA									0x81
#define DAPI_ADDA_MODE_0_24mA									0x82
//#define DAPI_ADDA_MODE_0_25mA									0x83
#define DAPI_ADDA_MODE_0_50mA									0x84

#define DAPI_ADDA_MODE_BI_CAL_MODE								0xfd
#define DAPI_ADDA_MODE_0_20mA_TESTMODE							0xfe
#define DAPI_ADDA_MODE_BIPOL_10V_TESTMODE						0xff

#define DAPI_ADDA_MODE_DA_DISABLE								0x100
#define DAPI_ADDA_MODE_DA_ENABLE								0x200

#define DAPI_ADDA_MODE_PREVENT_DAPI_MODE_ERROR					0x8000

// --------------------------------------------------------
// A/D and D/A units

#define DAPI_ADDA_UNIT_ILLEGAL									0x00
#define DAPI_ADDA_UNIT_VOLT										0x01
#define DAPI_ADDA_UNIT_MA										0x02
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

// Stepper-Defines

// ------------------------------------
// ERROR Codes

#define DAPI_STEPPER_ERR_NONE									0		// es liegt kein Fehler vor 
#define DAPI_STEPPER_ERR_PARAMETER								1		// Parameter hat falschen Wertebereich 
#define DAPI_STEPPER_ERR_MOTOR_MOVE								2		// Kommando abgelehnt, da sich der Motor dreht
#define DAPI_STEPPER_ERR_DISABLE_MOVE							3		// Kommando abgehelnt, da Motorbewegung disabled ist
#define DAPI_STEPPER_ERR_DEVICE_NOT_FOUND						-1		// es liegt kein Fehler vor 

// ------------------------------------
// Special Stepper Function-Codes

#define DAPI_STEPPER_RETURN_0_BYTES 							0x00000000		// Kommando schickt 0 Byte als Antwort
#define DAPI_STEPPER_RETURN_1_BYTES 							0x40000000		// Kommando schickt 1 Byte als Antwort
#define DAPI_STEPPER_RETURN_2_BYTES 							0x80000000		// Kommando schickt 2 Byte als Antwort
#define DAPI_STEPPER_RETURN_4_BYTES 							0xc0000000		// Kommando schickt 4 Byte als Antwort


#define DAPI_STEPPER_CMD_SET_MOTORCHARACTERISTIC                ( 0x00000001 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_GET_MOTORCHARACTERISTIC                ( 0x00000002 + DAPI_STEPPER_RETURN_4_BYTES )  
#define DAPI_STEPPER_CMD_SET_POSITION                           ( 0x00000003 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_GO_POSITION                            ( 0x00000004 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_GET_POSITION                           ( 0x00000005 + DAPI_STEPPER_RETURN_4_BYTES )  
#define DAPI_STEPPER_CMD_SET_FREQUENCY                          ( 0x00000006 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_SET_FREQUENCY_DIRECTLY                 ( 0x00000007 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_GET_FREQUENCY                          ( 0x00000008 + DAPI_STEPPER_RETURN_2_BYTES )  
#define DAPI_STEPPER_CMD_FULLSTOP                               ( 0x00000009 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_STOP                                   ( 0x00000010 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_GO_REFSWITCH                           ( 0x00000011 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_DISABLE                                ( 0x00000014 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_MOTORCHARACTERISTIC_LOAD_DEFAULT		( 0x00000015 + DAPI_STEPPER_RETURN_0_BYTES )
#define DAPI_STEPPER_CMD_MOTORCHARACTERISTIC_EEPROM_SAVE		( 0x00000016 + DAPI_STEPPER_RETURN_0_BYTES )
#define DAPI_STEPPER_CMD_MOTORCHARACTERISTIC_EEPROM_LOAD		( 0x00000017 + DAPI_STEPPER_RETURN_0_BYTES )
#define DAPI_STEPPER_CMD_GET_CPU_TEMP							( 0x00000018 + DAPI_STEPPER_RETURN_1_BYTES )
#define DAPI_STEPPER_CMD_GET_MOTOR_SUPPLY_VOLTAGE				( 0x00000019 + DAPI_STEPPER_RETURN_2_BYTES )
#define DAPI_STEPPER_CMD_GO_POSITION_RELATIVE                   ( 0x00000020 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_EEPROM_ERASE							( 0x00000021 + DAPI_STEPPER_RETURN_0_BYTES )  
#define DAPI_STEPPER_CMD_SET_VECTORMODE                         ( 0x00000040 + DAPI_STEPPER_RETURN_0_BYTES )  

// ------------------------------------
// values for PAR1 for DAPI_STEPPER_CMD_SET_MOTORCHARACTERISTIC

#define DAPI_STEPPER_MOTORCHAR_PAR_STEPMODE						1					// Schrittmode (Voll-, Halb-, Viertel-, Achtel-, Sechszehntelschritt 
#define DAPI_STEPPER_MOTORCHAR_PAR_GOFREQUENCY					2					// Schrittfrequenz bei GoPosition [Vollschritt / s]
#define DAPI_STEPPER_MOTORCHAR_PAR_STARTFREQUENCY				3					// Startfrequenz [Vollschritt / s]
#define DAPI_STEPPER_MOTORCHAR_PAR_STOPFREQUENCY				4					// Stopfrequenz [Vollschritt / s]
#define DAPI_STEPPER_MOTORCHAR_PAR_MAXFREQUENCY					5					// maximale Frequenz [Vollschritt / s]
#define DAPI_STEPPER_MOTORCHAR_PAR_ACCELERATIONSLOPE			6					// Beschleunigung in [Vollschritten / ms]
#define DAPI_STEPPER_MOTORCHAR_PAR_DECELERATIONSLOPE			7					// Bremsung in [Vollschritten / ms]
#define DAPI_STEPPER_MOTORCHAR_PAR_PHASECURRENT					8					// Phasenstrom [mA]
#define DAPI_STEPPER_MOTORCHAR_PAR_HOLDPHASECURRENT				9					// Phasenstrom bei Motorstillstand [mA]
#define DAPI_STEPPER_MOTORCHAR_PAR_HOLDTIME 					10					// Zeit in der der Haltestrom fließt nach Motorstop [s]
#define DAPI_STEPPER_MOTORCHAR_PAR_STATUSLEDMODE				11					// Betriebsart der Status-LED
#define DAPI_STEPPER_MOTORCHAR_PAR_INVERT_ENDSW1				12					// invertiere Funktion des Endschalter1  
#define DAPI_STEPPER_MOTORCHAR_PAR_INVERT_ENDSW2				13					// invertiere Funktion des Endschalter12 
#define DAPI_STEPPER_MOTORCHAR_PAR_INVERT_REFSW1				14					// invertiere Funktion des Referenzschalterschalter1
#define DAPI_STEPPER_MOTORCHAR_PAR_INVERT_REFSW2				15					// invertiere Funktion des Referenzschalterschalter2
#define DAPI_STEPPER_MOTORCHAR_PAR_INVERT_DIRECTION 			16					// invertiere alle Richtungsangaben
#define DAPI_STEPPER_MOTORCHAR_PAR_ENDSWITCH_STOPMODE			17					// Bei Endschalter soll (0=full stop/1=stop mit rampe)
#define DAPI_STEPPER_MOTORCHAR_PAR_GOREFERENCEFREQUENCY_TOENDSWITCH		18			// Motor Frequency for GoReferenceCommand
#define DAPI_STEPPER_MOTORCHAR_PAR_GOREFERENCEFREQUENCY_AFTERENDSWITCH	19			// Motor Frequency for GoReferenceCommand
#define DAPI_STEPPER_MOTORCHAR_PAR_GOREFERENCEFREQUENCY_TOOFFSET 20					// Motor Frequency for GoReferenceCommand

// --------------------------------------------------------
// values for PAR1 for DAPI_STEPPER_CMD_GO_REFSWITCH

#define DAPI_STEPPER_GO_REFSWITCH_PAR_REF1						1
#define DAPI_STEPPER_GO_REFSWITCH_PAR_REF2						2
#define DAPI_STEPPER_GO_REFSWITCH_PAR_REF_LEFT					4
#define DAPI_STEPPER_GO_REFSWITCH_PAR_REF_RIGHT					8
#define DAPI_STEPPER_GO_REFSWITCH_PAR_REF_GO_POSITIVE			16
#define DAPI_STEPPER_GO_REFSWITCH_PAR_REF_GO_NEGATIVE			32
#define DAPI_STEPPER_GO_REFSWITCH_PAR_SET_POS_0					64

// ------------------------------------
// Stepper Read Status
#define DAPI_STEPPER_STATUS_GET_POSITION						0x01
#define DAPI_STEPPER_STATUS_GET_SWITCH							0x02
#define DAPI_STEPPER_STATUS_GET_ACTIVITY						0x03


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// CAN-Defines



// ------------------------------------
// CAN Commands

#define DAPI_CAN_CMD_SET_BITRATE								1
#define DAPI_CAN_CMD_SET_MASK0									2
#define DAPI_CAN_CMD_SET_RX_ADDRESS								3
#define DAPI_CAN_CMD_CLR_TIMESTAMP								4
#define DAPI_CAN_CMD_GET_BITRATE								5
#define DAPI_CAN_CMD_GET_TIMESTAMP								6

#define DAPI_CAN_CMD_TEST_GEN_RX_PACK							0xfffffff0


#define DAPI_PAR_CAN_MESSAGE_BOX_0								0
#define DAPI_PAR_CAN_MESSAGE_BOX_1								1
#define DAPI_PAR_CAN_MESSAGE_BOX_2								2
#define DAPI_PAR_CAN_MESSAGE_BOX_3								3

#define DAPI_CAN_BITRATE_10000									10000
#define DAPI_CAN_BITRATE_20000									20000
#define DAPI_CAN_BITRATE_50000									50000
#define DAPI_CAN_BITRATE_100000									100000
#define DAPI_CAN_BITRATE_125000									125000
#define DAPI_CAN_BITRATE_250000									250000
#define DAPI_CAN_BITRATE_500000									500000
#define DAPI_CAN_BITRATE_1000000 								1000000

#define DAPI_CAN_MASK_SINGLE									0xffffffff
#define DAPI_CAN_MASK_ALL										0x0


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// CNT48 Commands


#define DAPI_CNT48_FILTER_20ns								0x0000
#define DAPI_CNT48_FILTER_100ns								0x1000
#define DAPI_CNT48_FILTER_250ns								0x2000
#define DAPI_CNT48_FILTER_500ns								0x3000
#define DAPI_CNT48_FILTER_1us								0x4000
#define DAPI_CNT48_FILTER_2_5us								0x5000
#define DAPI_CNT48_FILTER_5us								0x6000
#define DAPI_CNT48_FILTER_10us								0x7000
#define DAPI_CNT48_FILTER_25us								0x8000
#define DAPI_CNT48_FILTER_50us								0x9000
#define DAPI_CNT48_FILTER_100us								0xA000
#define DAPI_CNT48_FILTER_250us								0xB000
#define DAPI_CNT48_FILTER_500us								0xC000
#define DAPI_CNT48_FILTER_1ms								0xD000
#define DAPI_CNT48_FILTER_2_5ms								0xE000
#define DAPI_CNT48_FILTER_5ms								0xF000

#define DAPI_CNT48_MODE_COUNT_RISING_EDGE					0x0000
#define DAPI_CNT48_MODE_COUNT_RISING_EDGE_X2              	0x0001
#define DAPI_CNT48_MODE_COUNT_RISING_EDGE_X4              	0x0002
#define DAPI_CNT48_MODE_T									0x000D
#define DAPI_CNT48_MODE_FREQUENCY							0x000E
#define DAPI_CNT48_MODE_PWM									0x000F

#define DAPI_CNT48_SUBMODE_NO_RESET							0x0000
#define DAPI_CNT48_SUBMODE_RESET_WITH_READ 					0x0010
#define DAPI_CNT48_SUBMODE_NO_RESET_NO_HW_RESET             0x0020
#define DAPI_CNT48_SUBMODE_RESET_WITH_READ_NO_HW_RESET      0x0030
#define DAPI_CNT48_SUBMODE_RESET_ON_CH_7 					0x0070
#define DAPI_CNT48_SUBMODE_LATCH_COMMON 					0x0080
#define DAPI_CNT48_SUBMODE_FREQUENCY_TIME_BASE_1ms			0x0030
#define DAPI_CNT48_SUBMODE_FREQUENCY_TIME_BASE_10ms			0x0020
#define DAPI_CNT48_SUBMODE_FREQUENCY_TIME_BASE_100ms		0x0010
#define DAPI_CNT48_SUBMODE_FREQUENCY_TIME_BASE_1sec			0x0000

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Software FIFO Commands

#define DAPI_FIFO_TYPE_READ_AD_FIFO							0x01

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Definitions for DapiOpenEx

typedef struct
{
	unsigned char address[256];
	ULONG timeout;
	ULONG portno;
	
	ULONG encryption_type;
	unsigned char encryption_password[32];
} DAPI_OPENMODULEEX_STRUCT;

#define DAPI_OPEN_MODULE_OPTION_USE_EXBUFFER			(1<<0)	// Bit 0
#define DAPI_OPEN_MODULE_OPTION_NO_RESCAN				(1<<1)	// Bit 1

#define DAPI_OPEN_MODULE_ENCRYPTION_TYPE_NONE			0
#define DAPI_OPEN_MODULE_ENCRYPTION_TYPE_NORMAL			1
#define DAPI_OPEN_MODULE_ENCRYPTION_TYPE_ADMIN			2
#define DAPI_OPEN_MODULE_ENCRYPTION_TYPE_ADMIN_TEMP		3

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
