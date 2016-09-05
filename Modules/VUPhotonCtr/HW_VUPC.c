//==============================================================================
//
// Title:		HW_VUPC.c
// Purpose:		A short description of the implementation.
//
// Created on:	2-7-2014 at 14:31:24 by Lex van der Gracht.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h"
#include <formatio.h>
#include "cvidef.h"
#include <tcpsupp.h>
#include <toolbox.h>
#include <userint.h>
#include "TaskController.h"
#include "VUPCIkernel_dma.h"
#include "HW_VUPC.h"
#include "VUPhotonCtr.h"
#include "NIDAQmxManager.h"

//==============================================================================
// Constants

#define WDTE_ERR_BIT	0x01000000
#define RDTE_ERR_BIT	0x02000000
#define DTE_DONE_BIT	0x80000000
#define MAJOR_VERSION	3
#define MINOR_VERSION	0 
// driver status bits
#define READYFORDATA	0x00000001
#define WORKERACTIVE	0x00000002
#define BYTESPERPIXEL	8  
#define QUEUE_LENGTH	100   
#define NUMTHREADS		10																			  
#define MAXBUFSIZE		0x40000    
#define MAXPACKETSIZE   0x8000      //bytes

// new definitions
#define MAXBUF 			90000
#define COMMLENGTH		19

#define CommPortNumber  7
#define DataPortNumber  8 
#define ServHostName    "192.168.1.10"
	
//==============================================================================
// Types

//states
typedef enum {
	PMT_ERROR,
	PMT_IDLE,
	PMT_WAITFORARMED,
	PMT_READREQ
} PMTControllerStates;

typedef struct _PMTregcommand {
	
	unsigned long 		regaddress;
	unsigned long 		regvalue;
	int 				RW;
	
} PMTregcommand;

#define MAXBUF 90000

#define STATREG		"STAT"
#define CTRLREG 	"CTRL"
#define PMT1THRES 	"PM1T"
#define PMT1OFFSET 	"PM1O"  
#define PMT1GAIN 	"PM1G"
#define PMT2THRES 	"PM2T"
#define PMT2OFFSET 	"PM2O"  
#define PMT2GAIN 	"PM2G"
#define PMT3THRES 	"PM3T"
#define PMT3OFFSET 	"PM3O"  
#define PMT3GAIN 	"PM3G"
#define PMT4THRES 	"PM4T"
#define PMT4OFFSET 	"PM4O"  
#define PMT4GAIN 	"PM4G"

/*---------------------------------------------------------------------------*/
/* Macros						                                             */
/*---------------------------------------------------------------------------*/
#define tcpChk(f) if ((g_TCPError=(f)) < 0) {ReportTCPError(); goto Done;} else

/*---------------------------------------------------------------------------*/
/* Internal function prototypes                                              */
/*---------------------------------------------------------------------------*/
static int CVICALLBACK TCPCommThreadFunction (void *functionData);
static int CVICALLBACK TCPDataThreadFunction (void *functionData);  
static void CommConnect (void);
static void CommDisconnect (void);
static void DataConnect (void);
static void DataDisconnect (void);
int CVICALLBACK CommandTCPCB (unsigned handle, int event, int error,
                             void *callbackData);
int CVICALLBACK DataTCPCB (unsigned handle, int event, int error,
                             void *callbackData);
static void ReportTCPError (void);
int transmit(char* transmitBuf,int len);
int SendReg(char* reg,unsigned long regvalue);

/*---------------------------------------------------------------------------*/
/* Module-globals                                                            */
/*---------------------------------------------------------------------------*/
static int			gRunning = 0;
static int			gTCPCommThreadFunctionId = 0;
static int			gTCPDataThreadFunctionId = 0;
static unsigned int g_hcommtcp;
static unsigned int g_hdatatcp;  
static int          g_hmainPanel;
static int			g_TCPError = 0;
unsigned long long  gReceivedBytes;
unsigned long long  gpreviousReceivedBytes; 
char 				commreceiveBuf[256] = {0};
unsigned short      dataBuf[MAXBUF];  
int 				testcounter= 15;
double 				previoustime;
int 				filehandle=0;
unsigned int 		gRegvalue; 
unsigned int        g_reply_received; 

unsigned long 		g_controlreg	 = 0;
unsigned short*     tempBuffer=NULL;
unsigned int		tempBufSize =0;



//==============================================================================
// Static global variables

static TaskControl_type*		gtaskControl				= NULL;
static Channel_type*			gchannels[MAX_CHANNELS]		= {NULL};
static double					gSamplingRate				= 0;

//==============================================================================
// Global variables

int         			measurementmode =TASK_FINITE;  //default
//int 		 			bufsize; 
unsigned int 			PMTThreadID; 
unsigned int 			PMTThread2ID;
PMTregcommand 			newcommand;
int 					readerror				=0; 
int 					readdata				=0;
int 					nrsamples_in_iteration=200*100;   				// requested amount of samples
size_t 					nrsamples;				  				// number of samples measured 
int 					iterationnr;              				// current iteration number, passed in data

DefineThreadSafeScalarVar(unsigned int,ReadyForComm,0); 	    // command thread is ready for commands    
DefineThreadSafeScalarVar(unsigned int,ReadyForData,0); 	    // data receiving thread is ready for data


//==============================================================================
// Static functions

static unsigned int 	GetAcquisitionBusy 				(void);
static void 			SetMeasurementMode 				(int mode);
static int 				GetPMTControllerVersion 		(void);

//==============================================================================
// Global functions

//int CVICALLBACK 		PMTThreadFunction				(void *(functionData));
//int CVICALLBACK 		PMTThreadFunction2				(void *(functionData));    
int 					StartThreads			    	(CmtThreadPoolHandle poolHandle) ;
int 					StopThreads				    	(CmtThreadPoolHandle poolHandle);
int 					PMTReset						(void);
int 					PMT_SetGain 					(int PMTnr, double PMTGain);
int 					PMT_SetThresh 					(int PMTnr, double PMTThreshold);
int 					PMT_SetOffset 					(int PMTnr, double PMTOffset);



static void SetMeasurementMode (int mode)
{
	 measurementmode = mode;
}


int ReadPMTReg (unsigned long regaddress, unsigned long *regval)
{
	int 			error = 0;
	unsigned long 	reply = 0;
	char *cmdstr;
	 
	switch (regaddress){
		case CTRL_REG:
			return g_controlreg;
		case STAT_REG:
			cmdstr="PhotonCtr STAT?___\n"; 
			break;
		case VERS_REG:
			cmdstr="PhotonCtr VERS?___\n";   
			break;
		case ERROR_REG:
			cmdstr="PhotonCtr ERRR?___\n"; 
			break;
		case PMT1_CTRL_REG:
			cmdstr="PhotonCtr PMT1?___\n"; 
			break;
		case PMT2_CTRL_REG:
			cmdstr="PhotonCtr PMT2?___\n"; 
			break;
		case PMT3_CTRL_REG:
			cmdstr="PhotonCtr PMT3?___\n"; 
			break;
		case PMT4_CTRL_REG:
			cmdstr="PhotonCtr PMT4?___\n"; 
			break;
	}
	g_reply_received=0;
	error=transmit(cmdstr,COMMLENGTH);
	//wait for reply?
	while (g_reply_received==0){
			//wait
		ProcessSystemEvents();
	}
	*regval=gRegvalue;
	
	return error;
}

int WritePMTReg (unsigned long regaddress, unsigned long regvalue)
{
INIT_ERR 
	 
	 switch (regaddress){
		case CTRL_REG:
			errChk(SendReg(CTRLREG,regvalue));  
			break;
		case STAT_REG:
			errChk(SendReg(STATREG,regvalue)); 
			break;
		case VERS_REG:
			//?  
			break;
		case ERROR_REG:
		//	cmdstr="PhotonCtr ERRR?\n"; 
			break;
		case PMT1_CTRL_REG:
		//	cmdstr="PhotonCtr PMT1?\n"; 
			break;
		case PMT2_CTRL_REG:
		//	cmdstr="PhotonCtr PMT2?\n"; 
			break;
		case PMT3_CTRL_REG:
		//	cmdstr="PhotonCtr PMT3?\n"; 
			break;
		case PMT4_CTRL_REG:
		//	cmdstr="PhotonCtr PMT4?\n"; 
			break;
	}
	

Error:
	
	return errorInfo.error;
}

///  HIFN  Gets the PMT Controller Hardware Version
///  HIRET returns error, no error when 0
static int GetPMTControllerVersion (void)
{
#define GetPMTControllerVersion_Err_WrongVersion	-1
INIT_ERR

	unsigned long 	version		= 0;
	int 			major		= 0;
	int				minor		= 0;
	
	errChk(ReadPMTReg(VERS_REG, &version));
	
	minor = version&0x0000000F;
	major = (version>>4)&0x0000000F;
	if(!((major == MAJOR_VERSION) && (minor == MINOR_VERSION)))
		SET_ERR(GetPMTControllerVersion_Err_WrongVersion, "Wrong PMT Controller version.");
	
Error:
	
	return errorInfo.error;
}


int SetPMTDefault(void)
{
INIT_ERR

	double zero=0.0;
	
	errChk(PMT_SetGain(PMT1, zero));
	errChk(PMT_SetGain(PMT2, zero));   
	errChk(PMT_SetGain(PMT3, zero));   
	errChk(PMT_SetGain(PMT4, zero)); 
	errChk(PMT_SetThresh(PMT1, 0.1));
	errChk(PMT_SetThresh(PMT2, 0.1));   
	errChk(PMT_SetThresh(PMT3, 0.1));   
	errChk(PMT_SetThresh(PMT4, 0.1)); 
	errChk(PMT_SetOffset(PMT1,0.3));  //default value
	errChk(PMT_SetOffset(PMT2,0.3));  //default value     
	errChk(PMT_SetOffset(PMT3,0.3));  //default value     
	errChk(PMT_SetOffset(PMT4,0.3));  //default value 
	
Error:
	
	return errorInfo.error;  
}

	   
int PMTController_Init (void)
{
INIT_ERR

	unsigned long 	timeout	= 5000;  //in ms
	
	gRunning = 1;
	
	InitializeReadyForComm();
	InitializeReadyForData();  
	
	SetReadyForComm(0);
	SetReadyForData(0);
	
	errChk( StartThreads(DEFAULT_THREAD_POOL_HANDLE));   

	//wait until threads are running
	while (!GetReadyForComm()||!GetReadyForData())
		//wait until thread has started
		ProcessSystemEvents();
	
	//	errChk(GetPMTControllerVersion());
	 //set default values
	SetPMTDefault();

	
Error:
	
	return errorInfo.error;  
}

int PMTController_Finalize (void)	    
{    
INIT_ERR
	 

	 UninitializeReadyForComm();
	 UninitializeReadyForData();    
	
	 
	 //clear control register, make sure PMT's are disabled
	 errChk(WritePMTReg(CTRL_REG,0));     

	 
Error:
	 
	 return errorInfo.error;
}

void SetNrSamples(int numsamples)
{
	  nrsamples_in_iteration=numsamples;
}


static int ParseData (unsigned short int* sampleBuffer,int numbytes)
{
INIT_ERR

	int 					result			= 0;
	DataPacket_type*  		dataPacket		= NULL;
	char*					errMsg			= NULL;  
	unsigned short int*     pmtdata		    = NULL;
	size_t 					numpixels		= 0;
	size_t 					numshorts		= 0;
	long 					errcode			= 0;
	Waveform_type* 			waveform		= NULL;
	int						dataoffset		    = 0;
	size_t					bytesperchannel		= 0;
	DSInfo_type* 			dsInfo			= NULL;
	
	
	
	if (numbytes > 0) {
		//create temp buffer
		if (!tempBuffer) {
			tempBuffer= malloc(numbytes);
			//fill temp buffer          
			memcpy((unsigned short*)tempBuffer, (void*)sampleBuffer,numbytes);         
			tempBufSize=numbytes;
		}
		else {
			nullChk( tempBuffer = realloc(tempBuffer, tempBufSize + numbytes));
			//fill temp buffer          
			memcpy((unsigned short*)tempBuffer+tempBufSize/2, (void*)sampleBuffer,numbytes);         
			tempBufSize+=numbytes;
		}
	
		
		numpixels 	= tempBufSize/8;  //8 bytes per pixel, result gives number of bytes
		bytesperchannel  = numpixels * sizeof(unsigned short);
		
		//send datapackets
		//index i is index of active channels; chanIdx contains the PMT channel index
		for (int i = 0; i < MAX_CHANNELS; i++){
			if (gchannels[i] != NULL){
				if (gchannels[i]->VChan != NULL){
					
					//offset in data
					switch (i) {
																						  
						case 0: 			  
							
							dataoffset = 0;//2;
							break;
							
						case 1:
							
							dataoffset = 1;//3;
							break;
							
						case 2:
							
							dataoffset = 2;//0;
							break;
							
						case 3:
							
							dataoffset = 3;//1;
							break;
					}
					
						nullChk(pmtdata = malloc(bytesperchannel));
						for (int i=0;i<numpixels;i++)
						{
							pmtdata[i]=tempBuffer[4*i+dataoffset];
						}
					
					    //test Lex
						if (*(unsigned short*)pmtdata==0)
							{
								break;
							}
						// prepare waveform
						nullChk( waveform = init_Waveform_type(Waveform_UShort, gSamplingRate, numpixels, &pmtdata) );
						Iterator_type* currentiter = GetTaskControlIterator(gtaskControl);
						nullChk( dsInfo = GetIteratorDSData(currentiter, WAVERANK) );
				    	nullChk( dataPacket	= init_DataPacket_type(DL_Waveform_UShort, (void**)&waveform, &dsInfo, (DiscardFptr_type) discard_Waveform_type));       
					// send data packet with waveform
						errChk( SendDataPacket(gchannels[i]->VChan, &dataPacket, 0, &errMsg) );
				//	}
				}
			}
		}
		//check temp buffer
		int restbytes=tempBufSize%8;
		if (restbytes==0){
			//send all data, remove temp buffer
			tempBufSize=0;
			OKfree(tempBuffer);
		}
		else {
			//adjust temp buffer 
			tempBufSize=restbytes;
		
			
			for(int i=0;i<tempBufSize/2;i++){
				tempBuffer[i]=tempBuffer[i+bytesperchannel*2];
			}
			nullChk( tempBuffer = realloc(tempBuffer, tempBufSize ));       
		}
		if(measurementmode == TASK_FINITE){		 // need to count samples 
			//data is in pixels
			nrsamples = nrsamples + numpixels;
			//determine if iteration is complete
			if (nrsamples >= nrsamples_in_iteration){
				//iteration is done
				nrsamples = 0;
				//needed?
				PMTStopAcq(); 
				errChk( TaskControlIterationDone(gtaskControl, 0, "", FALSE, &errorInfo.errMsg) );   
			}
			else {
				if (((nrsamples_in_iteration-nrsamples)*8)<MAXBUFSIZE) {
			    //read last portion of the data, but reduce read size
		//		SetPMTBufsize((nrsamples_in_iteration-nrsamples)*8);
				}
			}
		}
	}
	
	
	return result;
	
Error:	
	// cleanup
	OKfree(pmtdata);
	discard_Waveform_type(&waveform);
	discard_DataPacket_type(&dataPacket);
	discard_DSInfo_type(&dsInfo);
	
	char* msgBuff = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg);
	TaskControlIterationDone(gtaskControl, errorInfo.error, msgBuff, FALSE, NULL); 
	OKfree(msgBuff);
	
	return errorInfo.error;
}



int PMT_SetMode (int PMTnr, PMT_Mode_type mode)  
{
INIT_ERR
	
	if(mode == PMT_MODE_ON){
		switch(PMTnr){
				
			case PMT1:
				
				g_controlreg = g_controlreg|PMT1HV_BIT|PMT1EN_BIT;  //set bit
				break;
				
			case PMT2:
				
				g_controlreg = g_controlreg|PMT2HV_BIT|PMT2EN_BIT;
				break;
				
			case PMT3:
				
				g_controlreg = g_controlreg|PMT3HV_BIT|PMT3EN_BIT;
				break;
				
			case PMT4:
				
				g_controlreg = g_controlreg|PMT4HV_BIT|PMT4EN_BIT;
				break;
		}
	} else {
		switch(PMTnr){
				
			case PMT1:
				
				g_controlreg = g_controlreg&(~PMT1HV_BIT);   //clear bit
				g_controlreg = g_controlreg|PMT1EN_BIT;   
				break;
				
			case PMT2:
				
				g_controlreg = g_controlreg&(~PMT2HV_BIT);
				g_controlreg = g_controlreg|PMT2EN_BIT;   
				break;
				
			case PMT3:
				
				g_controlreg = g_controlreg&(~PMT3HV_BIT);
				g_controlreg = g_controlreg|PMT3EN_BIT;    
				break;
				
			case PMT4:
				g_controlreg = g_controlreg&(~PMT4HV_BIT);
				g_controlreg = g_controlreg|PMT4EN_BIT; 
				break;
		}
	}
		//write control register
	errChk( WritePMTReg(CTRL_REG, g_controlreg) );  
	
Error:
	
	return errorInfo.error;
}


int PMT_SetFan (int PMTnr, BOOL value)  
{
INIT_ERR

	
	
	if(value){				//Fan On
		switch(PMTnr){
				
			case PMT1:
				g_controlreg = g_controlreg|PMT1FAN_BIT|PMT1EN_BIT;  //set bit
				break;
				
			case PMT2:
				
				g_controlreg = g_controlreg|PMT2FAN_BIT|PMT2EN_BIT;
				break;
				
			case PMT3:
				
				g_controlreg = g_controlreg|PMT3FAN_BIT|PMT3EN_BIT;
				break;
				
			case PMT4:
				
				g_controlreg = g_controlreg|PMT4FAN_BIT|PMT4EN_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				
				g_controlreg = g_controlreg&(~PMT1FAN_BIT);   //clear bit
				g_controlreg = g_controlreg|PMT1EN_BIT; 
				break;
				
			case PMT2:
				
				g_controlreg = g_controlreg&(~PMT2FAN_BIT);
				g_controlreg = g_controlreg|PMT2EN_BIT; 
				break;
				
			case PMT3:
				
				g_controlreg = g_controlreg&(~PMT3FAN_BIT);
				g_controlreg = g_controlreg|PMT3EN_BIT; 
				break;
				
			case PMT4:
				
				g_controlreg = g_controlreg&(~PMT4FAN_BIT);
				g_controlreg = g_controlreg|PMT4EN_BIT; 
				break;
		}
	}
	//write control register
	errChk(WritePMTReg(CTRL_REG,g_controlreg)); 
	
Error:
	
	return errorInfo.error;
}


int PMT_SetCooling (int PMTnr, BOOL value)  
{
INIT_ERR

	
	if(value){			//Peltier ON
		switch(PMTnr){
				
			case PMT1:
				
				g_controlreg = g_controlreg|PMT1PELT_BIT|PMT1EN_BIT;  //set bit
				break;
				
			case PMT2:
				
				g_controlreg = g_controlreg|PMT2PELT_BIT|PMT2EN_BIT;
				break;
				
			case PMT3:
				
				g_controlreg = g_controlreg|PMT3PELT_BIT|PMT3EN_BIT;
				break;
				
			case PMT4:
				g_controlreg = g_controlreg|PMT4PELT_BIT|PMT4EN_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
				
			case PMT1:
				
				g_controlreg = g_controlreg&(~PMT1PELT_BIT);   //clear bit
				g_controlreg = g_controlreg|PMT1EN_BIT; 
				break;
				
			case PMT2:
				
				g_controlreg = g_controlreg&(~PMT2PELT_BIT);
				g_controlreg = g_controlreg|PMT2EN_BIT; 
				break;
				
			case PMT3:
				
				g_controlreg = g_controlreg&(~PMT3PELT_BIT);
				g_controlreg = g_controlreg|PMT3EN_BIT; 
				break;
				
			case PMT4:
				
				g_controlreg = g_controlreg&(~PMT4PELT_BIT);
				g_controlreg = g_controlreg|PMT4EN_BIT; 
				break;
		}
	}
	//write control register
	errChk(WritePMTReg(CTRL_REG,g_controlreg));
	
Error:
	return errorInfo.error;
}


///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain in volts
///  HIRET returns error, no error when 0
int PMT_SetGain (int PMTnr, double PMTGain)
{
INIT_ERR

	unsigned short bitval;
	
	bitval=ceil(PMTGain*(4096/1.0));
	switch (PMTnr){
		case PMT1:	errChk(SendReg(PMT1GAIN,bitval)); 
			break;
		case PMT2:	errChk(SendReg(PMT2GAIN,bitval)); 
			break;
		case PMT3:	errChk(SendReg(PMT3GAIN,bitval)); 
			break;
		case PMT4:	errChk(SendReg(PMT4GAIN,bitval)); 
			break;
	}
	
Error:
	return errorInfo.error;
}

///  HIFN  Sets the PMT threshold
///  HIPAR PMTnr/PMT number,PMTThreshold in volts.
///  HIRET returns error, no error when 0
int PMT_SetThresh (int PMTnr, double PMTThreshold)
{
INIT_ERR

	unsigned short bitval;
	
	bitval=ceil(PMTThreshold*(65535/2.5)); 
	switch (PMTnr){
		case PMT1:	errChk(SendReg(PMT1THRES,bitval)); 
			break;
		case PMT2:	errChk(SendReg(PMT2THRES,bitval)); 
			break;
		case PMT3:	errChk(SendReg(PMT3THRES,bitval)); 
			break;
		case PMT4:	errChk(SendReg(PMT4THRES,bitval)); 
			break;
	}
	
Error:
	return errorInfo.error;
}


///  HIFN  Sets the PMT offset
///  HIPAR PMTnr/PMT number,PMTOffset in volts
///  HIRET returns error, no error when 0
int PMT_SetOffset (int PMTnr, double PMTOffset)
{
INIT_ERR

	unsigned short bitval;

	//offset=0.55V with bitvalue 0
	//and -0.195V with bitvalue 65535
	//so offset=(-0.195-0.55)/65535.bitvalue+0.55
	// bitvalue=
	bitval=ceil((PMTOffset-0.55)*65535/(-0.195-0.55));
	switch (PMTnr){
		case PMT1:	errChk(SendReg(PMT1OFFSET,bitval)); 
			break;
		case PMT2:	errChk(SendReg(PMT2OFFSET,bitval)); 
			break;
		case PMT3:	errChk(SendReg(PMT3OFFSET,bitval)); 
			break;
		case PMT4:	errChk(SendReg(PMT4OFFSET,bitval)); 
			break;
	}
	
Error:
	return errorInfo.error;
}



///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain, range ......
///  HIRET returns error, no error when 0
int SetPMTTresh (int PMTnr,double threshold)
{
INIT_ERR

	errChk( PMT_SetThresh(PMTnr,threshold));       
Error:
	
	return errorInfo.error;
}

///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain, range ......
///  HIRET returns error, no error when 0
int SetPMTGain (int PMTnr, double gain)
{
INIT_ERR

	errChk( PMT_SetGain(PMTnr,gain));
     
Error:
	
	return errorInfo.error;
}


int PMT_SetTestMode(BOOL testmode)
{
INIT_ERR

	
	if (testmode) 
		g_controlreg = g_controlreg|TESTMODE0_BIT;  //set bit
	else 
		g_controlreg = g_controlreg&(~TESTMODE0_BIT);  //clear bit 
	
	errChk( WritePMTReg(CTRL_REG, g_controlreg) ); 
		   
Error:
	return errorInfo.error;
}

/// HIFN  starts the PMT Controller Acquisition
/// HIRET returns error, no error when 0
int PMTStartAcq(TaskMode_type mode, TaskControl_type* taskControl, double samplingRate, Channel_type** channels)
{
INIT_ERR

	
//test lex
	//filehandle=OpenFile("test.bin",VAL_WRITE_ONLY,VAL_TRUNCATE,VAL_BINARY);
	
	// assign task controller to global (should be avoided in the future!)
	gtaskControl = taskControl;
	
	// assign sampling rate to global (should be avoided in the future!)
	gSamplingRate = samplingRate;
	
	for (int i = 0; i < MAX_CHANNELS; i++)
		if (channels[i]) {
			nullChk( gchannels[i] = malloc(sizeof(Channel_type)) );
		 	*gchannels[i] = *channels[i];
		} else 
			gchannels[i] = NULL;
	
	SetMeasurementMode(mode);
	errChk( PMTClearFifo() ); 
	
	
	
	readdata = 1;   //start reading       
	 
	//set app start bit  
	g_controlreg = g_controlreg|APPSTART_BIT;
	errChk( WritePMTReg(CTRL_REG, g_controlreg) );
	
	while (!GetReadyForData())
		//wait until thread has started
		ProcessSystemEvents();
	
Error:
	
	return errorInfo.error;
}																																				   

///  HIFN  stops the PMT Controller Acquisition
///  HIRET returns error, no error when 0
int PMTStopAcq(void)
{
INIT_ERR

//test lex
//	if (filehandle!=0) {
//		CloseFile(filehandle);
//		filehandle=0;
//	}
	
	//send null packet(s)
	for (int i = 0; i < MAX_CHANNELS; i++)
		if (gchannels[i] && gchannels[i]->VChan)
			errChk( SendNullPacket(gchannels[i]->VChan, &errorInfo.errMsg) );
	
	
	readdata = 0;  //stop reading  


	for (int i = 0; i < MAX_CHANNELS; i++)
		OKfree(gchannels[i]);
	
	//tell hardware to stop
	//set app start bit  
	g_controlreg = g_controlreg&~APPSTART_BIT; //clear appstart bit
	errChk( WritePMTReg(CTRL_REG,g_controlreg) );
	
Error:

	return errorInfo.error;
}




/// HIFN  resets the PMT Controller 
/// HIRET returns error, no error when 0
int PMTReset(void)
{
INIT_ERR  

	

	// set reset bit   
/*	g_controlreg = g_controlreg|RESET_BIT; 
	errChk(WritePMTReg(CTRL_REG, g_controlreg));
	
	// clear reset bit
	g_controlreg = g_controlreg&~RESET_BIT;
	errChk(WritePMTReg(CTRL_REG, g_controlreg));
	 */
	g_controlreg=0;
	
	//set gain to zero, threshold level to 300mV,offset to 100 mV
	errChk(SetPMTDefault());

	
Error:
	return errorInfo.error;
}

//can only be called when acq is finished
int PMTClearFifo(void)
{
INIT_ERR

	unsigned long 	statreg	= 0;
	
	
	errChk(ReadPMTReg(STAT_REG,&statreg));
		//clear fifo status bits if set
	if ((statreg&FFALMFULL_BIT)||(statreg&FFOVERFLOW_BIT)){ 
			if (statreg&FFALMFULL_BIT) statreg = statreg&~FFALMFULL_BIT;  //clear bit
			if (statreg&FFOVERFLOW_BIT) statreg = statreg&~FFOVERFLOW_BIT;  //clear bit    
			errChk( WritePMTReg(STAT_REG,statreg) );    
	}
	
	//here?
	nrsamples = 0;
	
Error:
	return errorInfo.error;
}

int PMT_ClearControl(int PMTnr)
{
INIT_ERR
	
	switch(PMTnr){
		case PMT1:
			g_controlreg = g_controlreg&~0x70000;    //clear control FAN,TEC and HV bitsfor PMT1
			break;
		case PMT2:
			g_controlreg = g_controlreg&~0x700000;   //clear control FAN,TEC and HV bitsfor PMT2        
			break;
		case PMT3:
			g_controlreg = g_controlreg&~0x7000000;  //clear control FAN,TEC and HV bitsfor PMT3       
			break;
		case PMT4:
			g_controlreg = g_controlreg&~0x70000000; //clear control FAN,TEC and HV bitsfor PMT4       
			break;
	}
	
	errChk(WritePMTReg(CTRL_REG,g_controlreg));
	
Error:
	return errorInfo.error;
}


int StartThreads(CmtThreadPoolHandle poolHandle)
{
INIT_ERR
	
//	SetAcqBusy(1);   
	
	
	CmtScheduleThreadPoolFunction (poolHandle,TCPCommThreadFunction, NULL, &gTCPCommThreadFunctionId);
	CmtScheduleThreadPoolFunction (poolHandle,TCPDataThreadFunction, NULL, &gTCPDataThreadFunctionId);  
	
	ProcessSystemEvents();  //to start the tread functions      
	
	//InitializePMTstate();
	
Error:
	
	return errorInfo.error;
}

int StopThreads(CmtThreadPoolHandle poolHandle)
{
INIT_ERR

	if (gTCPCommThreadFunctionId != 0){
			/*
			* Client is running on its own thread.
			* Wait for TCP thread to exit and clean up.
			*/
			CmtWaitForThreadPoolFunctionCompletion (poolHandle,gTCPCommThreadFunctionId, 0);
			CmtReleaseThreadPoolFunctionID (poolHandle,gTCPCommThreadFunctionId);
			gTCPCommThreadFunctionId = 0;
	}
	if (gTCPDataThreadFunctionId != 0){
			/*
			* Client is running on its own thread.
			* Wait for TCP thread to exit and clean up.
			*/
			CmtWaitForThreadPoolFunctionCompletion (poolHandle,gTCPDataThreadFunctionId, 0);
			CmtReleaseThreadPoolFunctionID (poolHandle,gTCPDataThreadFunctionId);
			gTCPDataThreadFunctionId = 0;
	}
	
	return errorInfo.error;
}




//added



int transmit(char* transmitBuf,int len)
{
	int error=0;
	if ( ClientTCPWrite (g_hcommtcp, transmitBuf,len, 1000) < 0) 
		error =-1;
	if (error<0)  MessagePopup ("ClientTCPWrite", "Error");   
	return error;
}



int SendReg(char* reg,unsigned long regvalue)
{
	int error=0;
	char cmdstr[1024];
	
	Fmt(cmdstr,"PhotonCtr %s",reg);
	cmdstr[15]=regvalue&0x000000FF;
	cmdstr[16]=(regvalue&0x0000FF00)>>8; 
	cmdstr[17]=(regvalue&0x00FF0000)>>16;
	cmdstr[18]=(regvalue&0xFF000000)>>24;  
	
	error=transmit(cmdstr,19);
	return error;
}

void ParseReceived(char* receiveBuf,int dataSize)
{
	if (dataSize==19) {
		g_reply_received=1; //valid reply 
		gRegvalue=receiveBuf[15]&0x000000FF;
		gRegvalue+=(receiveBuf[16]&0x000000FF)<<8;
		gRegvalue+=(receiveBuf[17]&0x000000FF)<<16;   
		gRegvalue+=(receiveBuf[18]&0x000000FF)<<24; 
	}
	  
}

/*---------------------------------------------------------------------------*/
/* This is the TCP client's TCP callback.  This function will receive event  */
/* notification, similar to a UI callback, whenever a TCP event occurs.      */
/* We'll respond to the DATAREADY event and read in the available data from  */
/* the server and display it.  We'll also respond to DISCONNECT events, and  */
/* tell the user when the server disconnects us.                             */
/*---------------------------------------------------------------------------*/
int CVICALLBACK CommandTCPCB (unsigned handle, int event, int error,
                             void *callbackData)
{
	
    
    ssize_t commSize         = sizeof (commreceiveBuf) - 1;
    switch (event)
        {
        case TCP_DATAREADY:
            if ((commSize = ClientTCPRead (g_hcommtcp, commreceiveBuf,
                                           commSize, 1000))
                < 0)
                {
                	//read failed
                }
            else
            	{
            	commreceiveBuf[commSize] = '\0';
				ParseReceived(commreceiveBuf,commSize);
				
            	}
            break;
        case TCP_DISCONNECT:
            MessagePopup ("TCP Client", "Server has closed connection!");
            SetReadyForComm(0);
            break;
    }
    return 0;
}


/*---------------------------------------------------------------------------*/
/* This is the TCP client's TCP callback.  This function will receive event  */
/* notification, similar to a UI callback, whenever a TCP event occurs.      */
/* We'll respond to the DATAREADY event and read in the available data from  */
/* the server and display it.  We'll also respond to DISCONNECT events, and  */
/* tell the user when the server disconnects us.                             */
/*---------------------------------------------------------------------------*/
int CVICALLBACK DataTCPCB (unsigned handle, int event, int error,
                             void *callbackData)
{
	
    
    ssize_t dataSize         = sizeof (dataBuf);
	
	double MB_received;
	double time;
	double rate;
	int err;

    switch (event)
        {
        case TCP_DATAREADY:
            if ((dataSize = ClientTCPRead (g_hdatatcp, dataBuf,
                                           dataSize, 1000))
                < 0)
                {
                // error?
                }
            else
            	{
		//		if (filehandle) WriteFile(filehandle,dataBuf,dataSize); 
				gReceivedBytes+=dataSize;
				err=ParseData (dataBuf,dataSize); 
				
				   
            	}
            break;
        case TCP_DISCONNECT:
            MessagePopup ("TCP Client", "Server has closed connection!");
            SetReadyForComm(0);
            break;
    }
    return 0;
}



/*---------------------------------------------------------------------------*/
/* Report TCP Errors if any                         						 */
/*---------------------------------------------------------------------------*/
static void ReportTCPError(void)
{
	if (g_TCPError < 0)
		{
		char	messageBuffer[1024];
		sprintf(messageBuffer, 
			"TCP library error message: %s\nSystem error message: %s", 
			GetTCPErrorString (g_TCPError), GetTCPSystemErrorString());
		MessagePopup ("Error", messageBuffer);
		g_TCPError = 0;
		}
}
/*---------------------------------------------------------------------------*/





/*---------------------------------------------------------------------------*/
/* TCP thread entry-point function.                         				 */
/*---------------------------------------------------------------------------*/
static int CVICALLBACK TCPCommThreadFunction (void *functionData)
{
	CommConnect ();
	while (gRunning)
		{
		ProcessSystemEvents ();
		}
	CommDisconnect ();													   
	return 0;
}

/*---------------------------------------------------------------------------*/
/* Connect to the TCP server.                         						 */
/*---------------------------------------------------------------------------*/
static void CommConnect (void)
{
	int		tcpErr = 0;																											 
	char tempBuf[256] = {0};    
	
	tcpChk (ConnectToTCPServer (&g_hcommtcp,  CommPortNumber, ServHostName, 
		CommandTCPCB, 0, 0));
	SetReadyForComm(1); 
	    /* We are successfully connected -- gather info */
	
Done:
	return;
}

/*---------------------------------------------------------------------------*/
/* Disconnect from the TCP server.                         					 */
/*---------------------------------------------------------------------------*/					   
static void CommDisconnect (void)
{
	int		tcpErr = 0;
	
	if (!GetReadyForComm())
		{
		DisconnectFromTCPServer (g_hcommtcp);
		SetReadyForComm(0);
		}

Done:
	return;
}

/*---------------------------------------------------------------------------*/
/* TCP thread entry-point function.                         				 */
/*---------------------------------------------------------------------------*/
static int CVICALLBACK TCPDataThreadFunction (void *functionData)
{
	DataConnect ();
	
	while (gRunning)
		{
		ProcessSystemEvents ();
		}
	DataDisconnect ();
	return 0;
}

/*---------------------------------------------------------------------------*/
/* Connect to the TCP server.                         						 */
/*---------------------------------------------------------------------------*/
static void DataConnect (void)
{
	int		tcpErr = 0;
	char tempBuf[256] = {0};    
	
	tcpChk (ConnectToTCPServer (&g_hdatatcp,  DataPortNumber, ServHostName, 
		DataTCPCB, 0, 0));
	SetReadyForData(1);
	
Done:
	return;
}

/*---------------------------------------------------------------------------*/
/* Disconnect from the TCP server.                         					 */
/*---------------------------------------------------------------------------*/
static void DataDisconnect (void)
{
	int		tcpErr = 0;
	
	DisconnectFromTCPServer (g_hdatatcp);
	SetReadyForData(0);     
		
Done:
	return;
}














