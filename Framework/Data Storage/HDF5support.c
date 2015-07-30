//==============================================================================
//
// Title:		HDF5support
// Purpose:		A short description of the application.
//
// Created on:	25-3-2015 at 9:41:17 by Adrian Negrean.
// Copyright:	Vrije Universiteit. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files

#include <formatio.h>
#include <cvirte.h>		
#include <userint.h>
#include "toolbox.h"
#include "hdf5.h"
#include "hdf5_hl.h"
#include "Iterator.h"
#include "HDF5support.h"
#include "DAQLabUtility.h"
#include <stdio.h>
#include <stdlib.h>
								   

#ifndef errChk
#define errChk(fCall) if (error = (fCall), error < 0) {goto Error;} else
#endif
	
#ifndef hdf5ErrChk
#define hdf5ErrChk(fCall) if (error = (fCall), error < 0) {goto HDF5Error;} else
#endif

#ifndef OKfree
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}
#endif
	
//==============================================================================
// Constants
#define MAXITERDEPTH 				100
#define MAXCHAR		 				260
	
#define IMAGE1_NAME  				"image8bit"
#define NUMELEMENTS_NAME 			"NumElements"
#define PIXELSIZE_NAME				"element_size_um"
#define DATETIME_FORMATSTRING		"%c"


// compression
#define GZIP_CompressionLevel		6		// Sets GNU ZIP compression level. 0 - no compression, 9 - maximum compression
#define SZIP_PixelsPerBlock			16

//==============================================================================
// Types
	
typedef struct IteratorData		IteratorData_type; 
	
struct IteratorData {
	char*					name;					// Iterator name.  
	size_t					currentIterIdx; 		// 0-based iteration index used to iterate over iterObjects
	BOOL					stackdata;
};


//==============================================================================
// Static global variables

//==============================================================================
// Static functions

	//----------------------------------
	// Groups
	//----------------------------------
static int 						CreateRootGroup 					(hid_t fileID, char *group_name);
static int 						CreateRelativeGroup 				(hid_t parentgroupID, char *group_name);
static int 						CreateHDF5Group 					(char* fileName, DSInfo_type* dsInfo, hid_t* fileID, hid_t* groupID);

	//----------------------------------
	// Attributes
	//----------------------------------

static int 						CreateStringAttr 					(hid_t datasetID, char attr_name[], char* attr_data);
static int 						CreateLongAttr						(hid_t datasetID, char attr_name[], long attr_data);
static int 						CreateULongAttr 					(hid_t datasetID, char attr_name[], unsigned long attr_data);
static int 						CreateLLongAttr 					(hid_t datasetID, char attr_name[], long long attr_data);
static int 						CreateULLongAttr 					(hid_t datasetID, char attr_name[], unsigned long long attr_data);
static int 						CreateULLongAttrArr 				(hid_t datasetID, char attr_name[], unsigned long long attr_data, size_t size);
static int 						ReadNumElemAttr 					(hid_t datasetID, hsize_t index, unsigned long long* attr_data, size_t size);
static int 						WriteNumElemAttr 					(hid_t datasetID, hsize_t index, unsigned long long attr_data, hsize_t size);
static int 						CreateIntAttr 						(hid_t datasetID, char attr_name[], int attr_data);
static int 						CreateUIntAttr 						(hid_t datasetID, char attr_name[], unsigned int attr_data);
static int 						CreateShortAttr 					(hid_t datasetID, char attr_name[], long attr_data);
static int 						CreateUShortAttr 					(hid_t datasetID, char attr_name[], unsigned long attr_data);
static int 						CreateFloatAttr						(hid_t datasetID, char attr_name[], float attr_data);
static int 						CreateDoubleAttr 					(hid_t datasetID, char attr_name[], double attr_data);
static int 						CreateDoubleAttrArr					(hid_t datasetID, char attr_name[], double attr_data, hsize_t size);

	// Waveforms
static int 						AddWaveformAttr 					(hid_t datasetID, Waveform_type* waveform);
static int 						AddWaveformAttrArr 					(hid_t datasetID, unsigned long long numelements, hsize_t size);

	// Images
static int 						CreatePixelSizeAttr 				(hid_t datasetID, double* attr_data);
static int 						AddImagePixSizeAttributes 			(hid_t datasetID, Image_type* image);
static int 						AddImageAttributes					(hid_t datasetID, Image_type* image, hsize_t size);

	// ROIs (regions of interest)
//static int						


	//----------------------------------
	// Utility
	//----------------------------------
static char* 					RemoveSlashes 						(char* name);
	// coverts waveform data types to HDF5 data types
static void						WaveformDataTypeToHDF5				(WaveformTypes waveformType, hid_t* typeIDPtr, hid_t* memTypeIDPtr);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

int CreateHDF5File(char fileName[], char datasetName[]) 
{
	herr_t      error		= 0;
	hid_t       fileID		= 0;   // file identifier
	
    // create a new file using default properties
	errChk( fileID = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT) );
	
    // terminate access to the file
    errChk( H5Fclose(fileID) ); 
	
	return 0;
Error:
	return error;
}

int WriteHDF5Waveform (char fileName[], char datasetName[], DSInfo_type* dsInfo, Waveform_type* waveform, CompressionMethods compression, char** errorInfo) 
{
	//=============================================================================================
	// INITIALIZATION (no fail)
	//=============================================================================================
	
	herr_t      			error					= 0;
	char*					errMsg					= NULL;
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// data
	
	size_t 					nElem					= 0;	
	void* 					waveformData			= *(void**)GetWaveformPtrToData(waveform, &nElem);
	unsigned int			dataRank				= GetDSDataRank(dsInfo);	 		// Data rank, 1 for waveforms, 2 for images.
	unsigned int 			indicesRank				= GetDSInfoDatasetRank(dsInfo);  	// Dataset rank determined by the experiment, equals number of indices.
	unsigned int 			totalRank				= indicesRank + dataRank; 			// Number of dimensions used in the HDF5 dataspace.
	unsigned int*			indices					= GetDSInfoIterIndices(dsInfo);   
   
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// HDF5
	
	hid_t       			fileID					= 0;
	hid_t        			fileSpaceID				= 0;
	hid_t					groupID					= 0;
	hid_t					datasetID				= 0;
	hid_t					dataSpaceID				= 0;
	hid_t					memSpaceID				= 0;
	hid_t 					memTypeID 				= 0;
	hid_t 					typeID					= 0;
	hid_t       			propertyListID			= 0; 
	
	hsize_t*				dims 				    = NULL;
	hsize_t*      			maxDims 				= NULL;
	hsize_t*				dimsr 					= NULL;
	hsize_t* 				offset					= NULL;
	hsize_t*      			size					= NULL;
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	BOOL					are_equal				= TRUE; 
	unsigned long long   	numelements				= 0;
	
	//=============================================================================================
	// MEMORY ALLOCATION (can fail)
	//=============================================================================================
 
	// mem alloc
	nullChk( dims		= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( maxDims	= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( dimsr		= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( offset		= malloc(totalRank * sizeof(hsize_t)) );
	nullChk( size		= malloc(totalRank * sizeof(hsize_t)) );
	
	// open the file and the group or creates one if necessary
	errChk( CreateHDF5Group(fileName, dsInfo, &fileID, &groupID) );
	
	// init dataspace dimensions
	for(size_t i = 0; i < totalRank; i++) {
		dims[i] 	= 1;
		maxDims[i] 	= H5S_UNLIMITED;
	}
	dims[dataRank-1] = nElem;   // number of elements in the wavefornm = width of dataspace
	
	// modify dataset creation properties, i.e. enable chunking
	hdf5ErrChk( propertyListID = H5Pcreate (H5P_DATASET_CREATE) );
	hdf5ErrChk( H5Pset_chunk(propertyListID, totalRank, dims) );
	
	// add compression if requested
	switch (compression) {
		
		case Compression_None:
			
			break;
			
		case Compression_GZIP:
			
			hdf5ErrChk( H5Pset_deflate (propertyListID, GZIP_CompressionLevel) );
			break;
			
		case Compression_SZIP:
			
			unsigned int szipOptionsMask 		= H5_SZIP_NN_OPTION_MASK;
			unsigned int szipPixelsPerBlock  	= SZIP_PixelsPerBlock;
			
			hdf5ErrChk( H5Pset_szip (propertyListID, szipOptionsMask, szipPixelsPerBlock) );
			break;
	}
  
	// convert waveform type to HDF5 types
	WaveformDataTypeToHDF5(GetWaveformDataType(waveform), &typeID, &memTypeID); 
   
	// create data space
	hdf5ErrChk( dataSpaceID = H5Screate_simple(totalRank, dims, maxDims) );
	
	// dataset name shouldn't have slashes in it
	datasetName = RemoveSlashes(datasetName);
	
	// Open the dataset if it exists
	datasetID = H5Dopen2(groupID, datasetName, H5P_DEFAULT );
	if (datasetID < 0) {
		//-----------------------------------------------
		// Dataset doesn't exist. A new one is created
		//-----------------------------------------------
		
		hdf5ErrChk( datasetID = H5Dcreate2(groupID, datasetName, typeID, dataSpaceID,H5P_DEFAULT, propertyListID,H5P_DEFAULT) );
		
		// write the dataset
   		hdf5ErrChk( H5Dwrite(datasetID, memTypeID, H5S_ALL, H5S_ALL, H5P_DEFAULT, waveformData) );
		
   		// add attributes to dataset
   		errChk( AddWaveformAttr(datasetID, waveform) );
		errChk( AddWaveformAttrArr(datasetID, nElem, 1) );  
		
	} else {
		//------------------------------------------------
		// Dataset exists. Add new to the current dataset
		//------------------------------------------------
		
		// get the size of the dataset
	   	hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
		// get dataspace dimension size and maximum size. 
    	hdf5ErrChk( H5Sget_simple_extent_dims (fileSpaceID, dimsr, NULL) );
		//check if saved size equals the indices set
		//if so, add data to current set
		//copy read dims into size and offset
		
		for (size_t i = 0; i < indicesRank; i++) {
			size[dataRank+i] = indices[i] + 1;      //adjust size to indices   /indices start at zero; dimsr base is  one          
			if (size[dataRank+i] != dimsr[dataRank+i])
				are_equal = FALSE;  
		}
		
		if (are_equal){
			errChk( ReadNumElemAttr(datasetID, size[1] - 1, &numelements, size[1]) );   
			//just add to current set
		
			for (size_t i = 0; i < indicesRank; i++)
				// adjust offset for multidimensional data
				offset[dataRank+i]= size[dataRank+i]-dims[dataRank+i]; 		
			
			// adjust dataset size for added data
			if (numelements + dims[0] > dimsr[0])
				size[0] = numelements + dims[0];
			else 
				size[0] = dimsr[0];
			
			// adjust offset
			offset[0] = numelements;      
			numelements += nElem;
			
    		hdf5ErrChk(H5Dset_extent (datasetID, size));
			
    		// Select a hyperslab in extended portion of dataset
    		hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
    	  
		} else { 
			
			//create a larger dataset and put data there
			for (size_t i = 0; i < indicesRank; i++) {       
				size[dataRank+i] = indices[i] + 1;
				offset[dataRank+i] = size[dataRank+i] - dims[dataRank+i];    	  
			}
										   
			//adjust data size  
			size[0]   	= dimsr[0];		// size of data equals previous data
			offset[0] 	= 0;			// new iteration, offset from beginning
			numelements	= nElem;    	// set data offset 
			
    		hdf5ErrChk( H5Dset_extent (datasetID, size) );

    		// Select a hyperslab in extended portion of dataset  //
    		hdf5ErrChk( fileSpaceID = H5Dget_space (datasetID) );
    		
		}
		
    	hdf5ErrChk( H5Sselect_hyperslab (fileSpaceID, H5S_SELECT_SET, offset, NULL, dims, NULL) );  

    	// Define memory space
   		memSpaceID = H5Screate_simple (totalRank, dims, NULL); 

    	// Write the data to the extended portion of dataset
    	hdf5ErrChk( H5Dwrite(datasetID, memTypeID, memSpaceID, fileSpaceID,H5P_DEFAULT, waveformData) );
		errChk( AddWaveformAttrArr(datasetID, numelements, size[1]) );     
	}
   
	// Close the dataset
	hdf5ErrChk(H5Dclose(datasetID));
   
	// Close the group ids
	hdf5ErrChk(H5Gclose (groupID));
   
	// Close the dataspace
	hdf5ErrChk(H5Sclose(dataSpaceID));
   
	// Close the file
	hdf5ErrChk(H5Fclose(fileID));
   
	OKfree(dims);
	OKfree(maxDims);
	OKfree(dimsr);
	OKfree(offset);
	OKfree(size);
   
	return 0;

HDF5Error:
   
	OKfree(dims);
	OKfree(maxDims);
	OKfree(dimsr);
	OKfree(offset);
	OKfree(size);
   			 
	/*
	FILE* tmpFile = tmpfile();			<--- wrong FILE type as defined by CVI's implementation of stdio compared to what H5Eprint
	H5Eprint(H5E_DEFAULT, tmpFile);
	fseek(tmpFile, 0, SEEK_END);
	long fsize = ftell(tmpFile);
	fseek(tmpFile, 0, SEEK_SET);
	errMsg = malloc((fsize+1) * sizeof(char));
	fread(errMsg, fsize, 1, tmpFile);
	fclose(tmpFile);
    ReturnErrMsg("Data Storage WriteHDF5Waveform");
	*/
	
	ReturnErrMsg("Data Storage WriteHDF5Waveform");
	return error;
   
Error:
   
	OKfree(dims);
	OKfree(maxDims);
	OKfree(dimsr);
	OKfree(offset);
	OKfree(size);
   
	ReturnErrMsg("Data Storage WriteHDF5Waveform");
	return error;
}

int WriteHDF5WaveformList (char fileName[], ListType waveformList, CompressionMethods compression, char** errorInfo)
{
#define WriteHDF5WaveformList_Err_NoFileName	-1
	
	herr_t      			error					= 0;
	char*					errMsg					= NULL;
	
	size_t					nWaveforms				= ListNumItems(waveformList);
	Waveform_type*			waveform				= NULL;
	void* 					waveformData			= NULL;
	char					datasetName[100]		= "";
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	// HDF5
	
	hid_t       			fileID					= 0;
	hid_t 					memTypeID 				= 0;
	hid_t 					typeID					= 0;
	hid_t       			propertyListID			= 0;
	hid_t					dataSpaceID 			= 0;
	hid_t					datasetID				= 0;
	
	hsize_t					dims[1] 			    = {0};				 // number of elements in the waveform
	hsize_t      			maxDims[1] 				= {H5S_UNLIMITED};
	
	//-----------------------------------------------------------------------------------------------------------------------------------------------------
	
	// if there are no waveforms to be saved, then do nothing
	if (!nWaveforms) return 0;
	
	// check if a file name was given
	if (!fileName || !fileName[0]) {
		error 	= WriteHDF5WaveformList_Err_NoFileName;
		errMsg 	= StrDup("No file name was provided.");
		goto Error;
	}
	
	// create file
   	hdf5ErrChk( fileID = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT) );
	// create dataset creation property list
	hdf5ErrChk( propertyListID = H5Pcreate (H5P_DATASET_CREATE) );
	
	// create data set for each waveform and add it to the file
	for (size_t i = 1; i <= nWaveforms; i++) {
		waveform 		= *(Waveform_type**)ListGetPtrToItem(waveformList, i);
		waveformData	= *(void**)GetWaveformPtrToData(waveform, &dims[0]); 
		
		// modify the dataset creation property list to enable chunking
		hdf5ErrChk( H5Pset_chunk(propertyListID, 1, dims) );
	
		// add compression if requested
		switch (compression) {
		
			case Compression_None:
			
				break;
			
			case Compression_GZIP:
			
				hdf5ErrChk( H5Pset_deflate (propertyListID, GZIP_CompressionLevel) );
				break;
			
			case Compression_SZIP:
			
				unsigned int szipOptionsMask 		= H5_SZIP_NN_OPTION_MASK;
				unsigned int szipPixelsPerBlock  	= SZIP_PixelsPerBlock;
			
				hdf5ErrChk( H5Pset_szip (propertyListID, szipOptionsMask, szipPixelsPerBlock) );
				break;
		}
		
		// convert waveform type to HDF5 types
		WaveformDataTypeToHDF5(GetWaveformDataType(waveform), &typeID, &memTypeID);
		
		// create data space
		hdf5ErrChk( dataSpaceID = H5Screate_simple(1, dims, maxDims) );
		
		// create data set
		Fmt(datasetName, "%d", i);
		hdf5ErrChk( datasetID = H5Dcreate(fileID, datasetName, typeID, dataSpaceID, H5P_DEFAULT, propertyListID, H5P_DEFAULT) );
		
		// add attributes to the dataset
   		errChk( AddWaveformAttr(datasetID, waveform) );
		
		// write waveform data to the dataset
   		hdf5ErrChk( H5Dwrite(datasetID, memTypeID, H5S_ALL, H5S_ALL, H5P_DEFAULT, waveformData) );
		
		// close the dataspace
		if (dataSpaceID > 0) {H5Sclose(dataSpaceID); dataSpaceID = 0;}
		// close dataset
		if (datasetID > 0) {H5Dclose(datasetID); datasetID = 0;}
	}
	
	
HDF5Error:
	
	if (error < 0)
		errMsg 	= StrDup("HDF5 error");  
	
Error:
	
	// close dataset creation property list
	if (propertyListID > 0) {H5Pclose(propertyListID); propertyListID = 0;}
	// close the dataspace
	if (dataSpaceID > 0) {H5Sclose(dataSpaceID); dataSpaceID = 0;}
	// close data set
	if (datasetID > 0) {H5Dclose(datasetID); datasetID = 0;}
	
	// close file
	if (fileID > 0) {H5Fclose(fileID); fileID = 0;}
	
	ReturnErrMsg("WriteHDF5WaveformList");
	return error;
}

int WriteHDF5Image(char fileName[], char datasetName[], DSInfo_type* dsInfo, Image_type* image, CompressionMethods compression, char** errorInfo) 
{ 
	int   					error        			= 0;
	
	hid_t 					fileID					= 0;
	hid_t					groupID					= 0;
	char**					groupName				= NULL; 
	hid_t       			propertyListID			= 0; 
   	hid_t 					memTypeID 				= 0;
   	hid_t 					typeID					= 0;
   	hid_t        			fileSpaceID				= 0;
   	hid_t					memSpaceID				= 0;
   	hsize_t					dimsr[3]				= {0};
   	hsize_t      			size[3]					= {0};
   	hsize_t      			offset[3]				= {0};
	hid_t					datasetID				= 0;
	hid_t					dataSpaceID				= 0;
	int 					height					= 0;
    int 					width					= 0;
	
	GetImageSize(image, &width, &height);
	
    int						numimages				= 1;  			
    void* 					DataPtr					= NULL;
    hsize_t      			imagedims[3]			= {1, height, width};
	hsize_t      			stackdims[3]			= {numimages, height, width}; 
    hsize_t      			maxstackdims[3] 		= {H5S_UNLIMITED, H5S_UNLIMITED, H5S_UNLIMITED};
	

	// opens the file and the group, 
	// creates one if necessary
	errChk( CreateHDF5Group(fileName, dsInfo, &fileID, &groupID) );
   
	ImageTypes type = GetImageType(image);
   
    // Modify dataset creation properties, i.e. enable chunking
	propertyListID = H5Pcreate (H5P_DATASET_CREATE);
	errChk(H5Pset_chunk ( propertyListID, 3, imagedims));
	
	// add compression if requested
	switch (compression) {
		
		case Compression_None:
			
			break;
			
		case Compression_GZIP:
			
			errChk( H5Pset_deflate (propertyListID, GZIP_CompressionLevel) );
			break;
			
		case Compression_SZIP:
			
			unsigned int szipOptionsMask 		= H5_SZIP_NN_OPTION_MASK;
			unsigned int szipPixelsPerBlock  	= SZIP_PixelsPerBlock;
			
			errChk( H5Pset_szip (propertyListID, szipOptionsMask, szipPixelsPerBlock) );
			break;
	}
  
	//datatype switch
	switch (type) {
		   
		case Image_UChar:
			
			typeID		= H5T_STD_U8BE;
		   	memTypeID	= H5T_NATIVE_UCHAR;
			break;
		   
		case Image_Short:
			
			typeID		= H5T_STD_I16BE;
			memTypeID	= H5T_NATIVE_SHORT;
			break;
		   
		case Image_UShort:
			
			typeID		= H5T_STD_U16BE;
			memTypeID	= H5T_NATIVE_USHORT;
			break;
		   
		case Image_Int:
			typeID		= H5T_STD_I32BE;
			memTypeID	= H5T_NATIVE_INT;
			break;
		   
		case Image_UInt:
			typeID		= H5T_STD_U32BE;
			memTypeID	= H5T_NATIVE_UINT;
			break;
		   
		case Image_Float:
			typeID		= H5T_IEEE_F32BE;
			memTypeID	= H5T_NATIVE_FLOAT;
			break;
	}
   
	// open an existing or new dataset
	dataSpaceID = H5Screate_simple(3, stackdims, maxstackdims);

   	// datasetname shouldn't have slashes in it
	datasetName = RemoveSlashes(datasetName);
	DataPtr = GetImagePixelArray(image);
   
	// open the dataset if it exists
	datasetID = H5Dopen2(groupID, datasetName, H5P_DEFAULT );
	if (datasetID < 0) {
		datasetID = H5Dcreate2(groupID, datasetName,typeID, dataSpaceID,H5P_DEFAULT, propertyListID,H5P_DEFAULT);
		if (datasetID < 0)
	   		return -1;
   	
		// write the dataset
		errChk(H5Dwrite(datasetID, memTypeID, H5S_ALL, H5S_ALL, H5P_DEFAULT, DataPtr));
   		// add attributes to dataset
		errChk(AddImagePixSizeAttributes(datasetID,image));
		errChk(AddImageAttributes(datasetID, image, 1)); 
  	} else {
		// dataset existed, have to add the data to the current data set
	   	// get the size of the dataset
		fileSpaceID = H5Dget_space (datasetID);
		errChk(H5Sget_simple_extent_dims (fileSpaceID, dimsr, NULL));
		size[0] = dimsr[0]+1;  
		size[1] = dimsr[1];  
		size[2] = dimsr[2];
		errChk(H5Dset_extent (datasetID, size));

    	// select a hyperslab in extended portion of dataset
    	fileSpaceID = H5Dget_space (datasetID);
    	offset[0] = dimsr[0]; 
    	offset[1] = 0;
		offset[2] = 0;  
    	errChk(H5Sselect_hyperslab (fileSpaceID, H5S_SELECT_SET, offset, NULL, imagedims, NULL));  

    	// define memory space
   		memSpaceID = H5Screate_simple (3, imagedims, NULL); 

    	// Write the data to the extended portion of dataset
    	errChk(H5Dwrite (datasetID, memTypeID, memSpaceID, fileSpaceID,H5P_DEFAULT, DataPtr));
		errChk(AddImageAttributes(datasetID,image, size[0]));
   	}
	   
    // close the dataset
	errChk(H5Dclose(datasetID));
   
	// close the group ids
	errChk(H5Gclose (groupID));
   
	free(groupName);
   
	// close the dataspace
	errChk(H5Sclose(dataSpaceID));
	
	// close the file. */
	errChk(H5Fclose(fileID));
   
	return error;
   
Error:
	
   return error;
}

static int CreateRootGroup (hid_t fileID, char *group_name)
{
	herr_t		error		= 0;   
	hid_t		groupID	= 0;
	
	errChk( groupID = H5Gcreate(fileID, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
  	errChk( H5Gclose (groupID) );
	
	return 0;
Error:
	return error;
}

static int CreateRelativeGroup (hid_t parentgroupID, char *group_name)
{
	herr_t      error		= 0;   
	hid_t		groupID	= 0;
	
	errChk( groupID = H5Gcreate2(parentgroupID, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
  	errChk(H5Gclose (groupID));
	
	return 0;
Error:
	return error;
}

static int CreateHDF5Group (char* fileName, DSInfo_type* dsInfo, hid_t* fileIDPtr, hid_t* groupIDPtr)
{
#define CreateHDF5Group_Err_NoFile		-1
	
   	herr_t      			error			= 0; 
	
	hid_t					fileID			= 0;
	hid_t* 					groupIDs 		= NULL;
	char**					groupNames		= NULL;
	size_t 					nGroupNames		= 0;
	char*					tmpGroupName	= NULL;
	char* 					pch				= NULL;
	
	
	// init
	*fileIDPtr 	= 0;
	*groupIDPtr = 0;
	
	// check if a file name is given
	if (!fileName || !fileName[0])
		return CreateHDF5Group_Err_NoFile;
	
	// open an existing file
   	errChk( fileID = H5Fopen(fileName, H5F_ACC_RDWR, H5P_DEFAULT) );
   	
	// get group names
	nullChk( groupNames = malloc(MAXITERDEPTH * sizeof(char*)) );
	nullChk( tmpGroupName = GetDSInfoGroupName(dsInfo) );
  	pch = strtok (tmpGroupName,"/");
	while (pch != NULL) {
    	nullChk( groupNames[nGroupNames] = StrDup(pch) );
		nGroupNames++;
    	pch = strtok (NULL, "/");
	}
	
	// create groups if necessary
	nullChk( groupIDs = malloc((nGroupNames + 1) * sizeof(hid_t)) );
	groupIDs[0] = fileID; // initialize the first groupID with the fileID
	for (size_t i = 1; i <= nGroupNames; i++)
		groupIDs[i] = 0;
   	
	for (size_t i = 0; i < nGroupNames; i++){
		// remove any slashes from dataset name
		groupNames[i] = RemoveSlashes(groupNames[i]);
		
		// open group if it exists
		groupIDs[i+1] = H5Gopen2(groupIDs[i], groupNames[i], H5P_DEFAULT );
		
		if (groupIDs[i+1] < 0) {
			
			// create group if it didn't exist (reply is negative)
			errChk( groupIDs[i+1] = H5Gcreate2(groupIDs[i], groupNames[i], H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
			*groupIDPtr = groupIDs[i+1]; 
			
		} else 
			*groupIDPtr = groupIDs[i+1]; 
	}
   
	// assign file ID
	*fileIDPtr = fileID;
   
	//--------------------
	// cleanup
	//--------------------
   
	// close all groupIDs except the last one and the first one which is the fileID
	for (size_t i = 1; i < nGroupNames; i++)
		H5Gclose(groupIDs[i]);
	
	OKfree(groupIDs);   
	
	// group names
	for (size_t i = 0; i < nGroupNames; i++)
			OKfree(groupNames[i]);
	OKfree(groupNames);
	
   	OKfree(tmpGroupName);
    
	return 0;
   
Error:
   
	// clear group names
	if (groupNames)
		for (size_t i = 0; i < nGroupNames; i++)
			OKfree(groupNames[i]);
	OKfree(groupNames);
	
	// clear group IDs
	if (groupIDs)
		for (size_t i = 1; i <= nGroupNames; i++)
			if (groupIDs[i] > 0) H5Gclose(groupIDs[i]);
	OKfree(groupIDs);
	
	// clear file ID (same as groupID[0])
	if (fileID > 0) H5Fclose(fileID);
	
	OKfree(tmpGroupName);
	
	return error;
}

static int CreateStringAttr (hid_t datasetID, char attr_name[], char* attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	hid_t		datatype		= 0;
	size_t  	dims			= 0; 	// Return value
	hsize_t  	dimsa[1]		= {1}; 
	
	dims 					= strlen(attr_data);
	errChk( dataSpaceID 	= H5Screate_simple (1, dimsa, NULL) );
   	datatype 				= H5Tcopy(H5T_C_S1);
   	errChk( H5Tset_size(datatype, dims) );
   	errChk( H5Tset_strpad(datatype, H5T_STR_NULLTERM) );
   	errChk( attributeID = H5Acreate2(datasetID, attr_name, datatype, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
   	errChk( H5Awrite(attributeID, datatype, attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
Error:
	
	return error;
}

static int CreateLongAttr (hid_t datasetID, char attr_name[], long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_LONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_LONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
Error:
	
	return error;
}

static int CreateULongAttr (hid_t datasetID, char attr_name[], unsigned long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_ULONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	
Error:
	
	return error;
}

static int CreateLLongAttr (hid_t datasetID, char attr_name[], long long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_LLONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_LLONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
Error:
	
	return error;
}

static int CreateULLongAttr (hid_t datasetID, char attr_name[], unsigned long long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULLONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) );
	
Error:
	
	return error;
}

static int CreateULLongAttrArr (hid_t datasetID, char attr_name[], unsigned long long attr_data, size_t size)
{
	herr_t      			error				= 0;
	hid_t     				dataSpaceID			= 0;  
	hid_t					attributeID			= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;			     // Return value
	
	nullChk( attr_array = malloc(size * sizeof(unsigned long long)) );
	
	errChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret	= H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);
 
	attributeID = H5Aopen( datasetID, attr_name, 0);
	if (attributeID < 0) {
		//attribute didn't extist, create one
		errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULLONG, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
		errChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, &attr_data) );  
	} else {
		errChk( H5Aread(attributeID, H5T_NATIVE_ULLONG, attr_array) );
		// delete previous attribute
		H5Aclose(attributeID);
		H5Adelete(datasetID, attr_name);
		 
		errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_ULLONG, dataSpaceID, H5P_DEFAULT,H5P_DEFAULT) );  
		attr_array[size-1] = attr_data;
		errChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, attr_array) );
	}
   
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace.
    errChk( H5Sclose(dataSpaceID) ); 
	
	OKfree(attr_array);
	
	return 0;
	
Error:
	
	OKfree(attr_array);
	return error;
}

static int ReadNumElemAttr (hid_t datasetID, hsize_t index, unsigned long long* attr_data, size_t size)
{
	herr_t      			error				= 0;
	hid_t     				dataSpaceID			= 0;  
	hid_t					attributeID			= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size*sizeof(unsigned long long)) );
	
	errChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret = H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);

	//open attribute
	errChk( attributeID = H5Aopen(datasetID, NUMELEMENTS_NAME, 0) ); 
	errChk( H5Aread(attributeID, H5T_NATIVE_ULLONG, attr_array) );
	*attr_data = attr_array[index];	 
   
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	OKfree(attr_array);
	
	return 0;
	
Error:
	
	OKfree(attr_array);
	return error;
}

static int WriteNumElemAttr (hid_t datasetID, hsize_t index, unsigned long long attr_data, hsize_t size)
{
	herr_t      			error				= 0;
	hid_t     				dataSpaceID			= 0;  
	hid_t					attributeID			= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size * sizeof(unsigned long long)) );
	
	errChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret  = H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);

	//open attribute
	errChk( attributeID = H5Aopen(datasetID, NUMELEMENTS_NAME, NULL) );
	errChk( H5Aread(attributeID, H5T_NATIVE_ULLONG, attr_array) ); 
	attr_array[index] = attr_data;
	errChk( H5Awrite(attributeID, H5T_NATIVE_ULLONG, attr_array) );
		 
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	OKfree(attr_array);
	
	return 0;
	
Error:
	
	OKfree(attr_array);
	return error;
}

static int CreateIntAttr (hid_t datasetID, char attr_name[], int attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_INT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_INT, &attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

static int CreateUIntAttr (hid_t datasetID, char attr_name[], unsigned int attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_UINT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_UINT, &attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

static int CreateShortAttr (hid_t datasetID, char attr_name[], long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_SHORT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_SHORT, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

static int CreateUShortAttr (hid_t datasetID, char attr_name[], unsigned long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_USHORT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_USHORT, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

static int CreateFloatAttr(hid_t datasetID, char attr_name[], float attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
    errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_FLOAT, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_FLOAT, &attr_data) );
	
	// Close the attribute. */
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace. */
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

static int CreateDoubleAttr (hid_t datasetID, char attr_name[], double attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataSpaceID		= 0;  
	hid_t       attributeID		= 0;
	
	errChk( dataSpaceID = H5Screate(H5S_SCALAR) );
	errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT));
    errChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, &attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

static int CreateDoubleAttrArr(hid_t datasetID, char attr_name[], double attr_data, hsize_t size)
{
	herr_t      error				= 0;
	
	hid_t     	dataSpaceID			= 0;  
	hid_t		attributeID			= 0;
	hsize_t     dims[1]				= {size};
	double*		attr_array			= NULL;
	herr_t  	ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size * sizeof(double)) );
	
	errChk( dataSpaceID = H5Screate(H5S_SIMPLE) );
    ret = H5Sset_extent_simple(dataSpaceID, 1, dims, NULL);

	if (size==1) {
		//create attribute
    	errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
		errChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, &attr_data) );  
	} else {
		//open attribute
		errChk( attributeID = H5Aopen( datasetID, attr_name, NULL) ); 
		errChk( H5Aread(attributeID, H5T_NATIVE_DOUBLE, attr_array) );
		//test
		errChk( H5Aclose(attributeID) );
		H5Adelete(datasetID, attr_name);
		 
		errChk( attributeID = H5Acreate2(datasetID, attr_name, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT,H5P_DEFAULT) );  
		attr_array[size-1] = attr_data;
		errChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, attr_array) );      
	}
   
	// Close the attribute
  	errChk( H5Aclose(attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	OKfree(attr_array);
	
	return 0;
	
Error:
	
	OKfree(attr_array);
	return error;
}

// Adds waveform information as dataset attributes
static int AddWaveformAttr (hid_t datasetID, Waveform_type* waveform)
{

	int 	error				= 0;
	char* 	waveformName		= GetWaveformName(waveform);			// Name of signal represented by the waveform.    
	char* 	unitName			= GetWaveformPhysicalUnit(waveform);	// Physical SI unit such as V, A, Ohm, etc. 
	double 	timestamp			= GetWaveformDateTimestamp(waveform);	// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double 	samplingRate		= GetWaveformSamplingRate(waveform);  	// Sampling rate in [Hz]. If 0, sampling rate is not given.   
	size_t 	dateTimeBuffLen		= 0;
	char*	dateTimeString		= NULL;
	
	// waveform name
	if (waveformName) 
		errChk( CreateStringAttr(datasetID, "Name", waveformName) );
	
	// waveform unit name
	if (unitName) 
		errChk( CreateStringAttr(datasetID, "Units", unitName) );
	
	// waveform start timestamp
	errChk( dateTimeBuffLen = FormatDateTimeString(timestamp, DATETIME_FORMATSTRING, NULL, 0) );
	nullChk( dateTimeString	= malloc((dateTimeBuffLen + 1) * sizeof(char)) );
	errChk( FormatDateTimeString(timestamp, DATETIME_FORMATSTRING, dateTimeString, dateTimeBuffLen + 1 ) );
	errChk( CreateStringAttr(datasetID, "Timestamp", dateTimeString) );
	
	// sampling rate
	errChk( CreateDoubleAttr(datasetID, "SamplingRate", samplingRate) );
	
	// sampling interval
	if (samplingRate) {
		errChk( CreateDoubleAttr(datasetID, "dt", 1/samplingRate) );
	}
	
Error: 
	 
	// cleanup
	OKfree(waveformName);
	OKfree(unitName);
	OKfree(dateTimeString);
	 
	return error;
}

static char* RemoveSlashes (char* name)
{
	size_t	len = strlen(name);
	
	for (int i = 0; i < len; i++)
		if (name[i] == '/') name[i]='_';
	
	return name;
}

static void WaveformDataTypeToHDF5 (WaveformTypes waveformType, hid_t* typeIDPtr, hid_t* memTypeIDPtr)
{
	switch (waveformType) {
		   
		case Waveform_Char:
			
			*typeIDPtr		= H5T_STD_I8BE;
			*memTypeIDPtr	= H5T_NATIVE_SCHAR;
			break;
		   
		case Waveform_UChar:
			
			*typeIDPtr		= H5T_STD_U8BE;
			*memTypeIDPtr	= H5T_NATIVE_UCHAR;
			break;	 
		   
		case Waveform_Short:
			
			*typeIDPtr		= H5T_STD_I16BE;
			*memTypeIDPtr	= H5T_NATIVE_SHORT;
			break;
		   
		case Waveform_UShort:
			
			*typeIDPtr		= H5T_STD_U16BE;
			*memTypeIDPtr	= H5T_NATIVE_USHORT;
			break;
		   
		case Waveform_Int:
			
			*typeIDPtr		= H5T_STD_I32BE;
			*memTypeIDPtr	= H5T_NATIVE_INT;
			break;
			
		case Waveform_UInt:
			
			*typeIDPtr		= H5T_STD_U32BE;
			*memTypeIDPtr	= H5T_NATIVE_UINT;
			break;
			
		case Waveform_Int64:
			
			*typeIDPtr		= H5T_STD_I64BE;
			*memTypeIDPtr	= H5T_NATIVE_LLONG;
			break;
			
		case Waveform_UInt64:
			
			*typeIDPtr		= H5T_STD_U64BE;
		   	*memTypeIDPtr	= H5T_NATIVE_ULLONG;
			break;
			
		case Waveform_SSize:
			
			*typeIDPtr		= H5T_STD_I64BE;
			*memTypeIDPtr	= H5T_NATIVE_HSSIZE;
			break;
			
		case Waveform_Size:
			
			*typeIDPtr		= H5T_STD_U64BE;
		   	*memTypeIDPtr	= H5T_NATIVE_HSIZE;
			break;
		
		case Waveform_Float:
			
			*typeIDPtr		= H5T_IEEE_F32BE;
		   	*memTypeIDPtr	= H5T_NATIVE_FLOAT;
			break;
			
		case Waveform_Double:
			
			*typeIDPtr		= H5T_IEEE_F64BE;
			*memTypeIDPtr	= H5T_NATIVE_DOUBLE;
			break;
	}
}

static int AddWaveformAttrArr (hid_t datasetID, unsigned long long numelements, hsize_t size)
{
	 int error	= 0;
	 
	 errChk( CreateULLongAttrArr(datasetID, NUMELEMENTS_NAME, numelements, size) ); 		
	 
	 return 0;
Error:
	 return error;
}

static int CreatePixelSizeAttr (hid_t datasetID, double* attr_data)
{
	herr_t      error				= 0;
	hid_t     	dataSpaceID			= 0;  
	hid_t       attributeID			= 0;
	int 		rank				= 1;
	hsize_t 	dims[]			    = {3};
	hsize_t 	maxDims[]			= {3}; 
	
	errChk(dataSpaceID = H5Screate_simple(rank, dims, maxDims) );
	   
    errChk( attributeID = H5Acreate2(datasetID, PIXELSIZE_NAME, H5T_NATIVE_DOUBLE, dataSpaceID, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attributeID, H5T_NATIVE_DOUBLE, attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose (attributeID) );
	// Close the dataspace
    errChk( H5Sclose(dataSpaceID) ); 
	
	return 0;
	
Error:
	
	return error;
}

// Adds image pixel size info attribute to the image data set.
static int AddImagePixSizeAttributes (hid_t datasetID, Image_type* image)
{
	 int 	error				= 0; 
	 double	pixSize				= GetImagePixSize(image);		// Image pixel size in [um].
	

	 double pixsize[3];
	 pixsize[0]=pixSize;   //Z size
	 pixsize[1]=pixSize;   //X size  
	 pixsize[2]=pixSize;   //Y size  

	 errChk(CreatePixelSizeAttr(datasetID,pixsize));          
	 return error;
Error:
	 return error;
}	

// Adds all image information attributes.
static int AddImageAttributes (hid_t datasetID, Image_type* image, hsize_t size)
{
	 int 	error				= 0; 
	 double	imgTopLeftXCoord	= 0;	// Image top-left corner X-Axis coordinates in [um].
	 double	imgTopLeftYCoord	= 0;	// Image top-left corner Y-Axis coordinates in [um].
	 double	imgZCoord			= 0;	// Image z-axis (height) location in [um].
	 
	 GetImageCoordinates(image, &imgTopLeftXCoord, &imgTopLeftYCoord, &imgZCoord);
		 
	 errChk(CreateDoubleAttrArr(datasetID,"TopLeftXCoord", imgTopLeftXCoord, size));    	
	 errChk(CreateDoubleAttrArr(datasetID,"TopLeftYCoord", imgTopLeftYCoord, size));  
	 errChk(CreateDoubleAttrArr(datasetID,"ZCoord", imgTopLeftXCoord, size)); 
          
	 return error;
Error:
	 return error;
}


