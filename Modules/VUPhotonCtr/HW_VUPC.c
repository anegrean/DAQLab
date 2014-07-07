#include <formatio.h>
#include "toolbox.h"
#include <userint.h>
#include "VUPCIkernel_dma.h"
//==============================================================================
//
// Title:		HW_VUPC.c
// Purpose:		A short description of the implementation.
//
// Created on:	2-7-2014 at 14:31:24 by Lex.
// Copyright:	Vrije Universiteit. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include "HW_VUPC.h"

// macro
#define OKfree(ptr) if (ptr) { free(ptr); ptr = NULL; }

#ifndef errChk
#define errChk(fCall) if (error = (fCall), error < 0) \
{goto Error;} else
#endif

//==============================================================================
// Constants
		

		
#define WDTE_ERR_BIT	0x01000000
#define RDTE_ERR_BIT	0x02000000
#define DTE_DONE_BIT	0x80000000
		
#define MAJOR_VERSION	1
#define MINOR_VERSION	5 

		//driver status bits
#define READYFORDATA	0x00000001
#define WORKERACTIVE	0x00000002
		
#define BYTESPERPIXEL	8  
		
// Constants
#define MAXBUFSIZE	1000000  //1 MB ?

#define QUEUE_LENGTH	100   
	
#define NUMTHREADS		1
	


//==============================================================================
// Types

//states
typedef enum {
	PMT_ERROR,
	PMT_IDLE,
	PMT_WAITFORARMED,
	PMT_READREQ
} PMTcontroller_state;


typedef struct _PMTregcommand {
	unsigned long regaddress;
	unsigned long regvalue;
	int RW;
} PMTregcommand;

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

int              		QuitFlag;
unsigned short int* 	Samplebuffer	= NULL;

BOOL         			moviemode;
BOOL         			imagemode; 
int 		 			bufsize; 
CmtThreadPoolHandle 	poolHandle		= 0;

char* gIDstr=NULL; 		 //	   need to save IDstr and image function to pass with the output data
int gimagetype;			 //

DefineThreadSafeScalarVar(int,PMTstate,0); 
DefineThreadSafeScalarVar(int,PMTCommandFlag,0);				    //do a PMT controller register command in pmt thread
DefineThreadSafeScalarVar(unsigned long,PMTControllerData,0); 	    // current PMT controller data
DefineThreadSafeScalarVar(int ,PMTBufsize,0); 	    				// pmt buffer size        
DefineThreadSafeScalarVar(unsigned int,PMTnewbufsizeflag,0); 	    // newbufsize flag       

unsigned int 	PMTThreadID; 
int 			PMTThreadFunctionID;
CmtTSQHandle 	commandqueue;
PMTregcommand 	newcommand;

//==============================================================================
// Global functions
int CVICALLBACK PMTThreadFunction(void *(functionData));
int StartDAQThread(CmtThreadPoolHandle poolHandle) ;
int PMTReset(unsigned long* controlreg);




 int ReadPMTReg(unsigned long regaddress,unsigned long *regval)
{
	 int error=0;
	 
	 //wait until last command is processed
	 do 
	 {
		ProcessSystemEvents(); 
	//	Delay(0.1);
	 }
	 while (GetPMTCommandFlag()==1);
	 
	 //read request, no data read yet
	 newcommand.regaddress=regaddress;
	 newcommand.regvalue=0;
	 newcommand.RW=READ;
	 
 	 
	 
	 CmtWriteTSQData (commandqueue, &newcommand,1,TSQ_INFINITE_TIMEOUT, NULL);
	 SetPMTCommandFlag(1);  
	 // wait until data is available       
	 do 
	 {
		ProcessSystemEvents(); 
	//	Delay(0.1);
	 }
	 while ((GetPMTCommandFlag()==1)&&(QuitFlag==0));
	 
	 //get data
	 *regval=GetPMTControllerData();
	 
	 
	 return error;
}


int WritePMTReg(unsigned long regaddress,unsigned long regvalue)
{
	 int error=0;
	
	 do 
	 {
		ProcessSystemEvents(); 
	//	Delay(0.1);
	 }
	 while (GetPMTCommandFlag()==1);
	 
	 //need to sync read and writes
	 newcommand.regaddress=regaddress;
	 newcommand.regvalue=regvalue;
	 newcommand.RW=WRITE;
	
 	 
	 CmtWriteTSQData (commandqueue, &newcommand,1,TSQ_INFINITE_TIMEOUT, NULL);
	 SetPMTCommandFlag(1);  
	 // wait until data is processed
	 do 
	 {
		ProcessSystemEvents(); 
	//	Delay(0.1);
	 }
	 while ((GetPMTCommandFlag()==1)&&(QuitFlag==0));
	

	 return error;
}

///  HIFN  Gets the PMT Controller Hardware Version
///  HIRET returns error, no error when 0
int GetPMTControllerVersion(void)
{
	int err=0;
	unsigned long version;
//	char buffer[255];
	int major,minor;
	
	err=ReadPMTReg(VERS_REG,&version);
	
	minor=version&0x0000000F;
	major=(version>>4)&0x0000000F;
//	Fmt (buffer, "%s<PMT controller version %i[w2p0].%i[w2p0]\n", major, minor);
//	AppendString(reply, buffer, -1); 
	if(!((major==MAJOR_VERSION)&&(minor==MINOR_VERSION))) err=-1;
	return err;
}


int PMTController_Init(unsigned long* controlreg)
{
	int error=0;
	unsigned long timeout=1000;  //in ms
	
	
	errChk(VUPCI_Open());
	errChk(CmtNewThreadPool(NUMTHREADS,&poolHandle));
	errChk(StartDAQThread(poolHandle));
	errChk(GetPMTControllerVersion());
	
	error=PMTReset(controlreg);
	VUPCI_Set_Read_Timeout(timeout);
	
Error:
	
	return error;  
}

int PMTController_Finalize(void)	    
{    
	 int error=0;
	 //clear control register, make sure PMT's are disabled
	 error=WritePMTReg(CTRL_REG,0);     
	 VUPCI_Close();
	 
	 return error;
}

//called in pmt thread
int ReadBuffer(unsigned short int* samplebuffer,int bufsize)
{
//	int err=0;
	int result;
//	DataPacket_type  datapacket_pmt1;
//	DataPacket_type  datapacket_pmt2;
//	DataPacket_type  datapacket_pmt3;
//	DataPacket_type  datapacket_pmt4;
	int numpixels;
	int numshorts;
	int ndatapoints;
	unsigned short* pmt1dataptr;
	unsigned short* pmt2dataptr;
	unsigned short* pmt3dataptr;
	unsigned short* pmt4dataptr;
	static int testcounter=0;
//	unsigned long reply;
//	int i;
	
	result = VUPCI_Read_Buffer((unsigned short*)Samplebuffer,bufsize);
	if( result<0){
				//err
		//SetPMTState(PMT_ERROR);
		//err=1;
		//error, stop polling
		moviemode = FALSE;
		imagemode = FALSE; 
	}
	else {
		if (result==0){
			//no data, just a timeout
			result=0;
		}
		else{
			//test data on tokens
			numshorts=bufsize/2;
		/*	for (i=0;i<numshorts;i++){
				if (Samplebuffer[i]>65530){
					//found a token.. program should'n break here!!
					break;
				}
			}		  */
	
			// deinterlace data here
			numpixels=result/64;  //64 bytes per pixel, result gives number of bytes
			numshorts=result/2;
			ndatapoints=numshorts/4;
			//transpose data array
			TransposeData(Samplebuffer,VAL_SHORT_INTEGER,numshorts,4);
			pmt1dataptr=malloc(ndatapoints*sizeof(unsigned short));
			pmt2dataptr=malloc(ndatapoints*sizeof(unsigned short)); 
			pmt3dataptr=malloc(ndatapoints*sizeof(unsigned short)); 
			pmt4dataptr=malloc(ndatapoints*sizeof(unsigned short)); 
			memcpy(pmt1dataptr,&Samplebuffer[0],ndatapoints*sizeof(unsigned short));
			memcpy(pmt2dataptr,&Samplebuffer[ndatapoints],ndatapoints*sizeof(unsigned short)); 
			memcpy(pmt3dataptr,&Samplebuffer[2*ndatapoints],ndatapoints*sizeof(unsigned short)); 
			memcpy(pmt4dataptr,&Samplebuffer[3*ndatapoints],ndatapoints*sizeof(unsigned short)); 
		  
			
			
			testcounter++;
			if (testcounter==5)  {
				testcounter=0;
			}
			//test
		//	ArrayToFile(rawfilename,pmt1dataptr,VAL_SHORT_INTEGER,ndatapoints,1,VAL_GROUPS_TOGETHER,VAL_GROUPS_AS_COLUMNS,VAL_SEP_BY_TAB,0,VAL_BINARY,VAL_APPEND);
			
			//send datapackets
//			if(init_DataPacket_type(VData_USHORT_Waveform, sizeof(unsigned short), ndatapoints, pmt1dataptr, NULL, 1, &datapacket_pmt1) < 0) return -1;  
			//insert saved IDstr and function
//			datapacket_pmt1.imagetype=gimagetype;
//			datapacket_pmt1.idstr=StrDup(gIDstr);
			
//			err=SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt1);
//			if (err<0){
//				MessagePopup("Error","Sending PMT Data");
//			}
			//ProcessSystemEvents();
			// CHECK HERE IF SendData IS NEGATIVE AND STOP TASKS IF SUCH ERROR OCCURS, THEN RELEASE DATA PACKETS LEFT IN THE QUEUES
		
		//	if(init_DataPacket_type(VData_USHORT_Waveform,, sizeof(short), ndatapoints, pmt2dataptr, NULL, 1, &datapacket_pmt2) < 0) return -1;  
		//	SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt2);
		//	if(init_DataPacket_type(VData_USHORT_Waveform,, sizeof(short), ndatapoints, pmt3dataptr, NULL, 1, &datapacket_pmt3) < 0) return -1;  
		//	SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt3);
		//	if(init_DataPacket_type(VData_USHORT_Waveform,, sizeof(short), ndatapoints, pmt4dataptr, NULL, 1, &datapacket_pmt4) < 0) return -1;  
		//	SendData(ActiveTaskIndex, PMT1_SOURCE, &datapacket_pmt4);
		}
		
	/*	OKfree(pmt1dataptr);
		OKfree(pmt2dataptr);
		OKfree(pmt3dataptr);
		OKfree(pmt4dataptr);	   */
		
	}
	
	return result;
}


int PMT_SetMode (unsigned long* controlreg, int PMTnr, PMT_Mode_type mode)  
{
	int error = 0;     
	
	if(mode==PMT_MODE_ON){
		switch(PMTnr){
			case PMT1:
				*controlreg=*controlreg|PMT1HV_BIT|APPSTART_BIT;  //set bit
				break;
			case PMT2:
				*controlreg=*controlreg|PMT2HV_BIT|APPSTART_BIT;
				break;
			case PMT3:
				*controlreg=*controlreg|PMT3HV_BIT|APPSTART_BIT;
				break;
			case PMT4:
				*controlreg=*controlreg|PMT4HV_BIT|APPSTART_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				*controlreg=*controlreg&(~PMT1HV_BIT);   //clear bit
				break;
			case PMT2:
				*controlreg=*controlreg&(~PMT2HV_BIT);
				break;
			case PMT3:
				*controlreg=*controlreg&(~PMT3HV_BIT);
				break;
			case PMT4:
				*controlreg=*controlreg&(~PMT4HV_BIT);
				break;
		}
	}
		//write control register
	error=WritePMTReg(CTRL_REG,*controlreg);      
	
	return error;
}


int PMT_SetFan (unsigned long* controlreg, int PMTnr, BOOL value)  
{
	int error = 0; 
	
	if(value){				//Fan On
		switch(PMTnr){
			case PMT1:
				*controlreg=*controlreg|PMT1FAN_BIT;  //set bit
				break;
			case PMT2:
				*controlreg=*controlreg|PMT2FAN_BIT;
				break;
			case PMT3:
				*controlreg=*controlreg|PMT3FAN_BIT;
				break;
			case PMT4:
				*controlreg=*controlreg|PMT4FAN_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				*controlreg=*controlreg&(~PMT1FAN_BIT);   //clear bit
				break;
			case PMT2:
				*controlreg=*controlreg&(~PMT2FAN_BIT);
				break;
			case PMT3:
				*controlreg=*controlreg&(~PMT3FAN_BIT);
				break;
			case PMT4:
				*controlreg=*controlreg&(~PMT4FAN_BIT);
				break;
		}
	}
	//write control register
	error=WritePMTReg(CTRL_REG,*controlreg);        
	
	return error;
}


int PMT_SetCooling (unsigned long* controlreg, int PMTnr, BOOL value)  
{
	int error = 0; 
	
	if(value){			//Peltier ON
		switch(PMTnr){
			case PMT1:
				*controlreg=*controlreg|PMT1PELT_BIT;  //set bit
				break;
			case PMT2:
				*controlreg=*controlreg|PMT2PELT_BIT;
				break;
			case PMT3:
				*controlreg=*controlreg|PMT3PELT_BIT;
				break;
			case PMT4:
				*controlreg=*controlreg|PMT4PELT_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				*controlreg=*controlreg&(~PMT1PELT_BIT);   //clear bit
				break;
			case PMT2:
				*controlreg=*controlreg&(~PMT2PELT_BIT);
				break;
			case PMT3:
				*controlreg=*controlreg&(~PMT3PELT_BIT);
				break;
			case PMT4:
				*controlreg=*controlreg&(~PMT4PELT_BIT);
				break;
		}
	}
	//write control register
	error=WritePMTReg(CTRL_REG,*controlreg);        
	
	return error;
}


///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain, range ......
///  HIRET returns error, no error when 0
int PMT_SetGainTresh(unsigned long* controlreg,int PMTnr,unsigned int PMTGain,unsigned int PMTThreshold)
{
	int error=0;
	
	unsigned long combinedval;
	unsigned long THMASK=0x0000FFFF;
	unsigned long GAINMASK=0xFFFF0000;   
 
	//read control register
	error=ReadPMTReg(CTRL_REG,controlreg);
	combinedval=((PMTGain<<16)&GAINMASK)+(PMTThreshold&THMASK);   
	
	switch(PMTnr){
		case PMT1:
			error=WritePMTReg(PMT1_CTRL_REG,combinedval);  
			//write control register
			*controlreg=*controlreg|UPDPMT12_BIT|APPSTART_BIT; //set bit  
		//	controlreg=controlreg&~APPSTART_BIT;  //clear bit 
			//write control register
			error=WritePMTReg(CTRL_REG,*controlreg);   
			*controlreg=*controlreg&~UPDPMT12_BIT;  //clear bit 
			break;
		case PMT2:
			error=WritePMTReg(PMT2_CTRL_REG,combinedval);  
				//write control register
			*controlreg=*controlreg|UPDPMT12_BIT|APPSTART_BIT;  //set bit 
			//write control register
			error=WritePMTReg(CTRL_REG,*controlreg);   
			*controlreg=*controlreg&~UPDPMT12_BIT;  //clear bit 
			break;
		case PMT3:
			error=WritePMTReg(PMT3_CTRL_REG,combinedval);
				//write control register
			*controlreg=*controlreg|UPDPMT34_BIT|APPSTART_BIT;  //set bit   
			//write control register
			error=WritePMTReg(CTRL_REG,*controlreg);   
			*controlreg=*controlreg&~UPDPMT34_BIT;  //clear bit 
			break;
		case PMT4:
			error=WritePMTReg(PMT4_CTRL_REG,combinedval); 
			//write control register
			*controlreg=*controlreg|UPDPMT34_BIT|APPSTART_BIT;  //set bit   
			//write control register
			error=WritePMTReg(CTRL_REG,*controlreg);   
			*controlreg=*controlreg&~UPDPMT34_BIT;  //clear bit 
			break;
	}
	return error;
}


int PMT_SetTestMode(unsigned long* controlreg,BOOL testmode)
{
	int error=0;
	
	if (testmode) *controlreg=*controlreg|TESTMODE0_BIT;  //set bit
	else *controlreg=*controlreg&(~TESTMODE0_BIT);  //clear bit 
	//write control register
	error=WritePMTReg(CTRL_REG,*controlreg); 
	
	return error;
}

 /*
int PMT_UpdateDisplay (VUPhotonCtr_type* vupc)
{
	int error = 0;
	int i;
	BOOL HV;
	BOOL CurrentErr;
	unsigned long statreg;
	
	
	//Get status from Hardware
	error=ReadPMTReg(CTRL_REG,vupc->controlreg);  
	error=ReadPMTReg(STAT_REG,&statreg); 
	
	//Update UI
	// photon counter
	SetCtrlVal (vupc->counterPanHndl,CounterPan_NUM_STATUS, statreg);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_NUM_COMMAND, vupc->controlreg); 
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_RUNNING, statreg&RUNNING_BIT);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_UNDER, statreg&FFUNDRFL_BIT); 
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_EMPTY, statreg&FFEMPTY_BIT); 
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_QFULL, statreg&FFQFULL_BIT); 
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_AFULL, statreg&FFALMFULL_BIT); 
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_OVERFLOW, statreg&FFOVERFLOW_BIT); 
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_TRIGFAIL, statreg&TRIGFAIL_BIT);
	
	for(i=0;i<MAX_CHANNELS;i++)  {
		if(vupc->channels[i]) {
			if(vupc->channels[i]->chanIdx==PMT1){
			//PMT1 is selected
				HV=vupc->controlreg&PMT1HV_BIT;
				CurrentErr=statreg&PMT1CURR_BIT;
			
				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);    
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
			
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr); 
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT1TEMP_BIT);
			}
			if(vupc->channels[i]->chanIdx==PMT2){
			//PMT2 is selected
				HV=vupc->controlreg&PMT2HV_BIT;
				CurrentErr=statreg&PMT2CURR_BIT;
			
				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);    
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr); 
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT2TEMP_BIT);
			}
			if(vupc->channels[i]->chanIdx==PMT3){
			//PMT3 is selected
				HV=vupc->controlreg&PMT3HV_BIT;
				CurrentErr=statreg&PMT3CURR_BIT;
			
				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);    
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr); 
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT3TEMP_BIT); 
			}
			if(vupc->channels[i]->chanIdx==PMT4){
			//PMT4 is selected
				HV=vupc->controlreg&PMT4HV_BIT;
				CurrentErr=statreg&PMT4CURR_BIT;
			
				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);    
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr); 
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT4TEMP_BIT); 
			}
		}
	}
	
	return error;
}
 */

int PMTReadActions(unsigned short int* samplebuffer,int numbytes)
{
	int i;
//	int err;
	int result;
	
	
	//prep buffer
	for (i=0;i<bufsize;i++) samplebuffer[i]=0;
	//read buffer
	result = ReadBuffer(samplebuffer,numbytes);
	return result;
}



void SetNewBuffer(void)
{
	int bufsize;

	bufsize=GetPMTBufsize();
	OKfree(Samplebuffer); 
//OKfree(Samplebuffer2);     
	Samplebuffer = malloc(bufsize); 
//	Samplebuffer2 = malloc(bufsize); 
	SetPMTnewbufsizeflag(0); 
}


///  HIFN  starts the PMT Controller Acquisition
 /// HIRET returns error, no error when 0
int PMTStartAcq(char* IDstr, int imagetype)
{
	int error=0;
	unsigned long controlreg;
	
	//save IDstr and function, need them on sending the data 
	gIDstr=StrDup(IDstr);
	gimagetype=imagetype;
	
	error=ReadPMTReg(CTRL_REG,&controlreg);
	//set app start bit   
	controlreg=controlreg|APPSTART_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);
	SetPMTstate(PMT_WAITFORARMED);
	
	return error;
}

///  HIFN  stops the PMT Controller Acquisition
///  HIRET returns error, no error when 0
int PMTStopAcq(void)
{
	int error=0;
	unsigned long controlreg;    
	
	if (gIDstr!=NULL) free(gIDstr);    
	
	error=ReadPMTReg(CTRL_REG,&controlreg);
	//clear app start bit
	controlreg=controlreg&~APPSTART_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);
	SetPMTstate(PMT_IDLE);
	return error;
}


///  HIFN  resets the PMT Controller 
 /// HIRET returns error, no error when 0
int PMTReset(unsigned long* controlreg)
{
	int error=0;

	//error=ReadPMTReg(CTRL_REG,controlreg);
	*controlreg=0;   //clear control reg
	//set reset bit   
	*controlreg=*controlreg|RESET_BIT; 
	error=WritePMTReg(CTRL_REG,*controlreg);
	//clear reset bit
	*controlreg=*controlreg&~RESET_BIT;
	error=WritePMTReg(CTRL_REG,*controlreg); 
	
	return error;
}


int PMTClearFifo(void)
{
	int error=0;
	unsigned long controlreg;  
	
	//set fifo reset bit
	error=ReadPMTReg(CTRL_REG,&controlreg);   
	controlreg=controlreg|FFRESET_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);  
	
	//clear fifo reset bit
	controlreg=controlreg&~FFRESET_BIT;
	error=WritePMTReg(CTRL_REG,controlreg);
	
	return error;
}

int PMT_ClearControl(unsigned long* controlreg,int PMTnr)
{
	int error=0;
	
	switch(PMTnr){
		case PMT1:
			*controlreg=*controlreg&~0x70000;    //clear control FAN,TEC and HV bitsfor PMT1
			break;
		case PMT2:
			*controlreg=*controlreg&~0x700000;   //clear control FAN,TEC and HV bitsfor PMT2        
			break;
		case PMT3:
			*controlreg=*controlreg&~0x7000000;  //clear control FAN,TEC and HV bitsfor PMT3       
			break;
		case PMT4:
			*controlreg=*controlreg&~0x70000000; //clear control FAN,TEC and HV bitsfor PMT4       
			break;
	}
	error=WritePMTReg(CTRL_REG,*controlreg);
	
	return error;
}


int StartDAQThread(CmtThreadPoolHandle poolHandle)
{
	int error=0;
	
//	parentThreadID=CmtGetCurrentThreadID (); 
	
	InitializePMTCommandFlag();
	InitializePMTControllerData();
	InitializePMTBufsize();   
	InitializePMTnewbufsizeflag();  
	
	SetPMTCommandFlag(0);
	SetPMTBufsize(0);   
	SetPMTnewbufsizeflag(0);
	
	QuitFlag=0;  
	//create threads
	error=CmtNewThreadPool (1,&poolHandle); 
	
	error=CmtScheduleThreadPoolFunctionAdv(poolHandle,PMTThreadFunction,NULL,THREAD_PRIORITY_TIME_CRITICAL,NULL,0,NULL,0,NULL);   	   //&PMTThreadFunctionID
	if (error<0) return error;  
	
	ProcessSystemEvents();  //to start the tread functions      
	
	/* create queue that holds 100 PMTqueue_elements and grows if needed */
	CmtNewTSQ(QUEUE_LENGTH, sizeof(PMTregcommand), OPT_TSQ_DYNAMIC_SIZE, &commandqueue);
	/* QueueReadCallback will be called when ANALYSIS_LENGTH items are in the Queue */     
	//CmtInstallTSQCallback (commandqueue, EVENT_TSQ_ITEMS_IN_QUEUE, ANALYSIS_LENGTH, QueueReadCallback, 0, PMTThreadID, NULL);
	
	InitializePMTstate();  
	
	return error;
}



int StopDAQThread(CmtThreadPoolHandle poolHandle)
{
	int error=0;	
//	int DAQThreadFunctionStatus;
//	int PMTThreadFunctionStatus;   
//	int PMTSyncThreadFunctionStatus;   
//	char test[255];
	
	error=PMTController_Finalize();

	QuitFlag=1;  
	
//	CmtGetThreadPoolFunctionAttribute(poolHandle,PMTThreadFunctionID,ATTR_TP_FUNCTION_EXECUTION_STATUS,&PMTThreadFunctionStatus);
//	if(PMTThreadFunctionStatus!=kCmtThreadFunctionComplete) 
//		CmtWaitForThreadPoolFunctionCompletion(poolHandle,PMTThreadFunctionID,OPT_TP_PROCESS_EVENTS_WHILE_WAITING);
//	CmtReleaseThreadPoolFunctionID(poolHandle,PMTThreadFunctionID);
	

	UninitializePMTstate();     
	UninitializePMTCommandFlag();
	UninitializePMTControllerData();
	UninitializePMTBufsize();   
	UninitializePMTnewbufsizeflag();  
	
	CmtDiscardTSQ(commandqueue);
	
	return error;
}


//called from pmt thread!!
unsigned long ImReadReg(unsigned long regaddress)
{
	 unsigned long reply=0;
	
	 if(!VUPCI_Get_Reg(regaddress,&reply)){ 
		reply=-1;
	 };
	 
	 return reply;
}

//called from pmt thread!! 
int ImWriteReg(unsigned long regaddress,unsigned long regvalue)
{
	 int error=0;
	 
	 if(!VUPCI_Set_Reg(regaddress,regvalue)){ 
		error=-1;
	 };
	 
	 return error;
}

//running in the pmt thread
void DoCommand(void)
{
	int error;
	unsigned long data;
	
	int cmtStatusOrItemsRead;
	int done=0;
	
	//read queue until empty or error
	while (done==0){ 
		cmtStatusOrItemsRead=CmtReadTSQData (commandqueue, &newcommand, 1,0 , 0);  		 //TSQ_INFINITE_TIMEOUT
		if (cmtStatusOrItemsRead<0){
			// error  ,do something here?
			done=1;
		}
		else if (cmtStatusOrItemsRead==0){
			// queue empty
			done=1;
		}
		else  {
			if (newcommand.RW==WRITE){ 
				//write command
				error=ImWriteReg(newcommand.regaddress,newcommand.regvalue);
				SetPMTCommandFlag(0);
			}
			else {
			//read request
				data=ImReadReg(newcommand.regaddress);
				SetPMTControllerData(data);
				SetPMTCommandFlag(0); 
			}
		}
	}
}		


//thread function,
//run pmt read actions in this separate thread to prevent other daq tasks from underrunning
int CVICALLBACK PMTThreadFunction(void *(functionData))
{
	int result; 
	int numbytes;  
	
	PMTThreadID=CmtGetCurrentThreadID ();  
	

	while (QuitFlag==0)
	{
		if (GetPMTnewbufsizeflag()==1) {
			SetNewBuffer();
		}
		if (GetPMTCommandFlag()==1) DoCommand(); 
		if ((moviemode==TRUE)||(imagemode==TRUE)) {
			numbytes=GetPMTBufsize();      
			result=PMTReadActions(Samplebuffer,numbytes);
			//test, read once?
			if ((imagemode==TRUE)&&(result>0)){
				imagemode=FALSE;
			}
		}
	//	ProcessSystemEvents();      
    }
    //quit
	SetPMTCommandFlag(0); 
	
	return 0;
}


