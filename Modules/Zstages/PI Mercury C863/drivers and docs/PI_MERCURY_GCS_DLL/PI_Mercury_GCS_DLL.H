                         //
// This is a part of the PI-Software Sources
// Copyright (C) 1995-2009 PHYSIK INSTRUMENTE GmbH
// All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
	#undef FUNC_DECL
	#ifdef MERCURY_DLL_EXPORTS
		#define MERCURY_FUNC_DECL __declspec(dllexport) WINAPI
	#else
		#define MERCURY_FUNC_DECL __declspec(dllimport) WINAPI
	#endif
#else
	#define MERCURY_FUNC_DECL
#endif
	
#ifndef WIN32
#ifndef BOOL
	typedef int BOOL;
#endif

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif
#endif

                         //
// DLL initialization and comm functions
#ifndef _GENERIC_I_NO_GUI
int		MERCURY_FUNC_DECL	Mercury_InterfaceSetupDlg(const char* szRegKeyName);
#endif
int		MERCURY_FUNC_DECL	Mercury_ConnectRS232(int port, int baudrate);
#ifndef WIN32
int		MERCURY_FUNC_DECL	Mercury_ConnectRS232ByDevName(const char* szDevName, int iBaudRate);
#endif
BOOL	MERCURY_FUNC_DECL	Mercury_IsConnected(int ID);
void	MERCURY_FUNC_DECL	Mercury_CloseConnection(int ID);
int		MERCURY_FUNC_DECL	Mercury_GetError(int ID);
BOOL	MERCURY_FUNC_DECL	Mercury_SetErrorCheck(int ID, BOOL bErrorCheck);
BOOL	MERCURY_FUNC_DECL	Mercury_TranslateError(int errNr, char* szBuffer, int maxlen);


                         //
// general
BOOL MERCURY_FUNC_DECL Mercury_qERR(int ID, int* pnError);
BOOL MERCURY_FUNC_DECL Mercury_qIDN(int ID, char* buffer, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_qVER(int ID, char* buffer, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_INI(int ID, const char* szAxes);

BOOL MERCURY_FUNC_DECL Mercury_MOV(int ID, const char* szAxes, const double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_qMOV(int ID, const char* szAxes, double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_MVR(int ID, const char* szAxes, const double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_IsMoving(int ID, const char* szAxes, BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qONT(int ID, const char* szAxes, BOOL* pbValarray);

BOOL MERCURY_FUNC_DECL Mercury_DFF(int ID, const char* szAxes, const double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_qDFF(int ID, const char* szAxes, double* pdValarray);

BOOL MERCURY_FUNC_DECL Mercury_DFH(int ID, const char* szAxes);
BOOL MERCURY_FUNC_DECL Mercury_qDFH(int ID, const char* szAxes, double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_GOH(int ID, const char* szAxes);

BOOL MERCURY_FUNC_DECL Mercury_qPOS(int ID, const char* szAxes, double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_POS(int ID, const char* szAxes, const double* pdValarray);

BOOL MERCURY_FUNC_DECL Mercury_HLT(int ID, const char* szAxes);
BOOL MERCURY_FUNC_DECL Mercury_STP(int ID);

BOOL MERCURY_FUNC_DECL Mercury_qCST(int ID, const char* szAxes, char* names, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_CST(int ID, const char* szAxes, const char* names);
BOOL MERCURY_FUNC_DECL Mercury_qVST(int ID, char* buffer, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_qTVI(int ID, char* axes, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_SAI(int ID, const char* szOldAxes, const char* szNewAxes);
BOOL MERCURY_FUNC_DECL Mercury_qSAI(int ID, char* axes, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_qSAI_ALL(int ID, char* axes, int maxlen);

BOOL MERCURY_FUNC_DECL Mercury_SVO(int ID, const char* szAxes, const BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qSVO(int ID, const char* szAxes, BOOL* pbValarray);

BOOL MERCURY_FUNC_DECL Mercury_VEL(int ID, const char* szAxes, const double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_qVEL(int ID, const char* szAxes, double* pdValarray);

BOOL MERCURY_FUNC_DECL Mercury_SPA(int ID, const char* szAxes, const int* iCmdarray, const double* dValarray, const char* szStageNames);
BOOL MERCURY_FUNC_DECL Mercury_qSPA(int ID, const char* szAxes, const int* iCmdarray, double* dValarray, char* szStageNames, int iMaxNameSize);

BOOL MERCURY_FUNC_DECL Mercury_qSRG(int ID, const char* szAxes, const int* iCmdarray, int* iValarray);

BOOL MERCURY_FUNC_DECL Mercury_GetInputChannelNames(int ID, char* szBuffer, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_GetOutputChannelNames(int ID, char* szBuffer, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_DIO(int ID, const char* szChannels, const BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qDIO(int ID, const char* szChannels, BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qTIO(int ID, int* pINr, int* pONr);

BOOL MERCURY_FUNC_DECL Mercury_qTAC(int ID, int* pnNr);
BOOL MERCURY_FUNC_DECL Mercury_qTAV(int ID, int nChannel, double* pdValue);

BOOL MERCURY_FUNC_DECL Mercury_BRA(int ID, const char* szAxes, const BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qBRA(int ID, char* szBuffer, int maxlen);

BOOL MERCURY_FUNC_DECL Mercury_qHLP(int ID, char* buffer, int maxlen);
BOOL MERCURY_FUNC_DECL Mercury_qHPA(long ID, char* szBuffer, long iBufferSize);

                         //
// String commands
BOOL MERCURY_FUNC_DECL Mercury_SendNonGCSString(int ID, const char* szString);
BOOL MERCURY_FUNC_DECL Mercury_ReceiveNonGCSString(int ID, char* szString, int iMaxSize);

BOOL MERCURY_FUNC_DECL Mercury_GcsCommandset(int ID, const char* szCommand);
BOOL MERCURY_FUNC_DECL Mercury_GcsGetAnswer(int ID, char* szAnswer, int bufsize);
BOOL MERCURY_FUNC_DECL Mercury_GcsGetAnswerSize(int ID, int* iAnswerSize);



                         //
// limits
BOOL MERCURY_FUNC_DECL Mercury_MNL(int ID, const char* szAxes);
BOOL MERCURY_FUNC_DECL Mercury_MPL(int ID, const char* szAxes);
BOOL MERCURY_FUNC_DECL Mercury_REF(int ID, const char* szAxes);
BOOL MERCURY_FUNC_DECL Mercury_qREF(int ID, const char* szAxes, BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qLIM(int ID, const char* szAxes, BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_IsReferencing(int ID, const char* szAxes, BOOL* pbIsReferencing);
BOOL MERCURY_FUNC_DECL Mercury_GetRefResult(int ID, const char* szAxes, int* pnResult);
BOOL MERCURY_FUNC_DECL Mercury_IsReferenceOK(int ID, const char* szAxes, BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qTMN(int ID, const char* szAxes, double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_qTMX(int ID, const char* szAxes, double* pdValarray);
BOOL MERCURY_FUNC_DECL Mercury_RON(int ID, const char* szAxes, const BOOL* pbValarray);
BOOL MERCURY_FUNC_DECL Mercury_qRON(int ID, const char* szAxes, BOOL* pbValarray);

                         //
// Macros
BOOL MERCURY_FUNC_DECL Mercury_IsRecordingMacro(int ID, BOOL* pbRecordingMacro);
BOOL MERCURY_FUNC_DECL Mercury_IsRunningMacro(int ID, BOOL* pbRunningMacro);

BOOL MERCURY_FUNC_DECL Mercury_DEL(int ID, int nMilliSeconds);
BOOL MERCURY_FUNC_DECL Mercury_MAC_BEG(int ID, const char* szMacroName);
BOOL MERCURY_FUNC_DECL Mercury_MAC_START(int ID, const char* szMacroName);
BOOL MERCURY_FUNC_DECL Mercury_MAC_NSTART(int ID, const char* szMacroName, int nrRuns);
BOOL MERCURY_FUNC_DECL Mercury_MAC_END(int ID);
BOOL MERCURY_FUNC_DECL Mercury_MEX(int ID, const char* szCondition);
BOOL MERCURY_FUNC_DECL Mercury_MAC_DEL(int ID, const char* szMacroName);
BOOL MERCURY_FUNC_DECL Mercury_qMAC(int ID, const char* szMacroName, char* szBuffer, int maxlen);

BOOL MERCURY_FUNC_DECL Mercury_WAC(int ID, const char* szCondition);

BOOL MERCURY_FUNC_DECL Mercury_JON(int ID, const int* iJoystickIDs, const BOOL* pbValarray, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qJON(int ID, const int* iJoystickIDs, BOOL* pbValarray, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qTNJ(int ID, int* pnNr);
BOOL MERCURY_FUNC_DECL Mercury_qJAX(int ID, const int* iJoystickIDs, const int* iAxesIDs, int iArraySize, char* szAxesBuffer, int iBufferSize);
BOOL MERCURY_FUNC_DECL Mercury_JDT(int ID, const int* iJoystickIDs, const int* iValarray, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qJBS(int ID, const int* iJoystickIDs,  const int* iAxesIDs, BOOL* pbValueArray, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qJAS(int ID, const int* iJoystickIDs,  const int* iAxesIDs, double* pdValueArray, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_JLT(int ID, int iJoystickID, int iAxisID, int iStartAdress, const double* pdValueArray,int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qJLT(int ID, const int* iJoystickIDsArray, const int* iAxisIDsArray,  int iNumberOfTables,  int iOffsetOfFirstPointInTable,  int iNumberOfValues, double** pdValueArray, char* szGcsArrayHeader, int iGcsArrayHeaderMaxSize);

BOOL MERCURY_FUNC_DECL Mercury_CTO(int ID, const long* iTriggerLines, const long* iParamID,  const char* szValues, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qCTO(int ID, const long* iTriggerLines, const long* pParamID, char* szBuffer, int iArraySize, int iBufferMaxlen);

BOOL MERCURY_FUNC_DECL Mercury_TRO(int ID, const long* iTriggerLines, const BOOL* pbValarray, int iArraySize);
BOOL MERCURY_FUNC_DECL Mercury_qTRO(int ID, const long* iTriggerLines, BOOL* pbValarray, int iArraySize);

                         //
// Spezial
BOOL	MERCURY_FUNC_DECL Mercury_AddStage(int ID, const char* szAxes);
BOOL	MERCURY_FUNC_DECL Mercury_RemoveStage(int ID, const char* szStageName);
#ifndef _GENERIC_I_NO_GUI
BOOL	MERCURY_FUNC_DECL Mercury_OpenUserStagesEditDialog(int ID);
BOOL	MERCURY_FUNC_DECL Mercury_OpenPiStagesEditDialog(int ID);
#endif
BOOL	MERCURY_FUNC_DECL Mercury_GetStatus(int ID, const char* szAxes, const int* icmdarry, int* piValarray);
int		MERCURY_FUNC_DECL Mercury_GetAsyncBufferIndex(long ID);

#ifdef __cplusplus
}
#endif
