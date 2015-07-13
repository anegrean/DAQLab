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


#ifndef errChk
#define errChk(fCall) if (error = (fCall), error < 0) {goto Error;} else
#endif

#ifndef OKfree
#define OKfree(ptr) if (ptr) {free(ptr); ptr = NULL;}
#endif
	
//==============================================================================
// Constants
#define MAXITERDEPTH 		100
#define MAXCHAR		 		260
	
#define IMAGE1_NAME  		"image8bit"
#define NUMELEMENTS_NAME 	"NumElements"
#define PIXELSIZE_NAME		"element_size_um"
	

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
static int 						CreateRootGroup 					(hid_t file_id, char *group_name);
static int 						CreateRelativeGroup 				(hid_t parentgroup_id, char *group_name);
static int 						CreateHDF5Group 					(char* filename, TC_DS_Data_type* dsdata, hid_t* file_id, hid_t* groupid);

	//----------------------------------
	// Attributes
	//----------------------------------
static int 						CreateStringAttr 					(hid_t dataset_id, char *attr_name, char* attr_data);
static int 						CreateLongAttr						(hid_t dataset_id, char *attr_name, long attr_data);
static int 						CreateULongAttr 					(hid_t dataset_id, char *attr_name, unsigned long attr_data);
static int 						CreateLLongAttr 					(hid_t dataset_id, char *attr_name, long long attr_data);
static int 						CreateULLongAttr 					(hid_t dataset_id, char *attr_name, unsigned long long attr_data);
static int 						CreateULLongAttrArr 				(hid_t dataset_id, char *attr_name, unsigned long long attr_data, size_t size);
static int 						ReadNumElemAttr 					(hid_t dataset_id, hsize_t index, unsigned long long* attr_data, size_t size);
static int 						WriteNumElemAttr 					(hid_t dataset_id, hsize_t index, unsigned long long attr_data, hsize_t size);
static int 						CreateIntAttr 						(hid_t dataset_id, char *attr_name, int attr_data);
static int 						CreateUIntAttr 						(hid_t dataset_id, char *attr_name, unsigned int attr_data);
static int 						CreateShortAttr 					(hid_t dataset_id, char *attr_name, long attr_data);
static int 						CreateUShortAttr 					(hid_t dataset_id, char *attr_name, unsigned long attr_data);
static int 						CreateFloatAttr						(hid_t dataset_id,char *attr_name, float attr_data);
static int 						CreateDoubleAttr 					(hid_t dataset_id, char *attr_name, double attr_data);
static int 						CreateDoubleAttrArr					(hid_t dataset_id, char *attr_name, double attr_data, hsize_t size);
static int 						CreatePixelSizeAttr 				(hid_t dataset_id, double* attr_data);
static int 						AddWaveformAttr 					(hid_t dataset_id, Waveform_type* waveform);
static int 						AddWaveformAttrArr 					(hid_t dataset_id, unsigned long long numelements, hsize_t size);
	//----------------------------------
	// Utility
	//----------------------------------
static char* 					RemoveSlashes 						(char* name);


//==============================================================================
// Global variables

//==============================================================================
// Global functions

int CreateHDF5file(char *filename,char* dataset_name) 
{
	herr_t      error		= 0;
	hid_t       file_id		= 0;   // file identifier
	
    // create a new file using default properties
	errChk( file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT) );
	
    // terminate access to the file
    errChk( H5Fclose(file_id) ); 
	
	return 0;
Error:
	return error;
}

int WriteHDF5Data(char *filename, char* dataset_name, TC_DS_Data_type* dsdata, Waveform_type* waveform, DLDataTypes datatype) 
{
	hid_t       			file_id					= 0;
	hid_t					dataspace_id			= 0;    
	hid_t					dataset_id				= 0;  
	herr_t      			error					= 0;
	size_t 					nElem					= 0;	
	void* 					dset_data				= *(void**)GetWaveformPtrToData(waveform, &nElem);   
   
	hid_t					group_id				= 0;
	hid_t       			cparms					= 0; 
	hid_t 					mem_type_id 			= 0;
	hid_t 					type_id					= 0;
	hid_t        			filespace				= 0;
	hid_t					memspace				= 0;
   
	BOOL					are_equal				= TRUE; 

	//test
	unsigned int			datarank				= GetDSDataRank(dsdata);	 //data rank, 1 for waveforms, 2 for images
   
   
	unsigned int 			indicesrank				= GetDSDataSetRank(dsdata);  //data rank determined by the experiment, equals number of indices
	unsigned int 			rank					= indicesrank + datarank; 
   
	hsize_t*				datadims 				= malloc(datarank * sizeof(hsize_t)); 
	hsize_t*				dims 				    = malloc(rank * sizeof(hsize_t));
	hsize_t*      			maxdims 			    = malloc(rank * sizeof(hsize_t));
	hsize_t*				dimsr 					= malloc(rank * sizeof(hsize_t));
	unsigned int*			indices					= GetDSdataIterIndices(dsdata);   
	hsize_t* 				offset					= malloc(rank * sizeof(hsize_t)); 
	hsize_t*      			size					= malloc(rank * sizeof(hsize_t));
	unsigned long long   	numelements				= 0;
 
	for(size_t i = 0; i < rank; i++) {
		dims[i] = 1;
		maxdims[i] = H5S_UNLIMITED;
	}
   
	dims[datarank-1] = nElem;   //width
	//test
	datadims[0] = nElem;
	//opens the file and the group, 
	//creates one if necessary
	errChk( CreateHDF5Group(filename, dsdata, &file_id, &group_id) );
   
	// modify dataset creation properties, i.e. enable chunking
	errChk( cparms = H5Pcreate (H5P_DATASET_CREATE) );
	errChk( H5Pset_chunk(cparms, rank, dims) );
  
	//datatype switch
	switch (datatype){
		   
		case DL_Char:
			
			type_id		= H5T_STD_I8BE;
			mem_type_id	= H5T_NATIVE_SCHAR;
			break;
		   
		case DL_UChar:
			
			type_id		= H5T_STD_U8BE;
			mem_type_id	= H5T_NATIVE_UCHAR;
			break;	 
		   
		case DL_Short:
			
			type_id		= H5T_STD_I16BE;
			mem_type_id	= H5T_NATIVE_SHORT;
			break;
		   
		case DL_UShort:
			
			type_id		= H5T_STD_U16BE;
			mem_type_id	= H5T_NATIVE_USHORT;
			break;
		   
		case DL_Int:
			
			type_id		= H5T_STD_I32BE;
			mem_type_id	= H5T_NATIVE_INT;
			break;
			
		case DL_UInt:
			
			type_id		= H5T_STD_U32BE;
			mem_type_id	= H5T_NATIVE_UINT;
			break;
			
		case DL_Long:
			
			type_id		= H5T_STD_I64BE;
			mem_type_id	= H5T_NATIVE_LONG;
			break;
			
		case DL_ULong:
			
			type_id		= H5T_STD_U64BE;
		   	mem_type_id	= H5T_NATIVE_ULONG;
			break;
			
		case DL_Float:
			
			type_id		= H5T_IEEE_F32BE;
		   	mem_type_id	= H5T_NATIVE_FLOAT;
			break;
			
		case DL_Double:
			
			type_id		= H5T_IEEE_F64BE;
			mem_type_id	= H5T_NATIVE_DOUBLE;
			break;
	}
   
	// Open an existing or new dataset
	errChk( dataspace_id = H5Screate_simple(rank, dims, maxdims) );
	
	//datasetname shouldn't have slashes in it
	dataset_name = RemoveSlashes(dataset_name);
	
	//open the dataset if it exists
	dataset_id = H5Dopen2(group_id, dataset_name, H5P_DEFAULT );
	if (dataset_id<0) {
		//dataset doesn't exist; create one
		errChk( dataset_id = H5Dcreate2(group_id, dataset_name,type_id, dataspace_id,H5P_DEFAULT, cparms,H5P_DEFAULT) );
		
		//create new entry in offsetlist
	//	datasetdata=malloc(sizeof(struct OffsetData));
	//	datasetdata->dataset_id=dataset_id;
	//	for (i=0;i<datarank;i++){ 
	//		datasetdata->offset[i]=datadims[i];	   //first data written
	//	}
	//	ListInsertItem(offsetlist, &datasetdata, END_OF_LIST);

		// write the dataset
   		errChk( H5Dwrite(dataset_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data) );
		
   		//add attributes to dataset
   		errChk( AddWaveformAttr(dataset_id, waveform) );
		errChk( AddWaveformAttrArr(dataset_id,nElem,1) );  
		
		
	} else {
		// dataset exists, have to add the data to the current data set
		
		// get the size of the dataset
	   	errChk( filespace = H5Dget_space (dataset_id) );
    	errChk( H5Sget_simple_extent_dims (filespace, dimsr, NULL) );
		//check if saved size equals the indices set
		//if so, add data to current set
		//copy read dims into size and offset
		
		for (size_t i = 0; i < indicesrank; i++) {
			size[datarank+i] = indices[i] + 1;      //adjust size to indices   /indices start at zero; dimsr base is  one          
			if (size[datarank+i] != dimsr[datarank+i])
				are_equal = FALSE;  
		}
		
		if (are_equal){
			errChk( ReadNumElemAttr(dataset_id, size[1] - 1, &numelements, size[1]) );   
			//just add to current set
		
			for (size_t i = 0; i < indicesrank; i++)
				// adjust offset for multidimensional data
				offset[datarank+i]= size[datarank+i]-dims[datarank+i]; 		
			
			// adjust dataset size for added data
			if (numelements + dims[0] > dimsr[0])
				size[0] = numelements + dims[0];
			else 
				size[0] = dimsr[0];
			
			// adjust offset
			offset[0] = numelements;      
			numelements += datadims[0];
			
    		errChk(H5Dset_extent (dataset_id, size));
			
    		// Select a hyperslab in extended portion of dataset
    		errChk( filespace = H5Dget_space (dataset_id) );
    	  
		} else { 
			
			//create a larger dataset and put data there
			for (size_t i = 0; i < indicesrank; i++) {       
				size[datarank+i] = indices[i] + 1;
				offset[datarank+i] = size[datarank+i] - dims[datarank+i];    	  
			}
										   
			//adjust data size  
			size[0]   	= dimsr[0];		  // size of data equals previous data
			offset[0] 	= 0;			  // new iteration, offset from beginning
			numelements	= datadims[0];    // set data offset 
			
    		errChk(H5Dset_extent (dataset_id, size));

    		// Select a hyperslab in extended portion of dataset  //
    		errChk( filespace = H5Dget_space (dataset_id) );
    		
		}
		
    	errChk( H5Sselect_hyperslab (filespace, H5S_SELECT_SET, offset, NULL, dims, NULL) );  

    	// Define memory space
   		memspace = H5Screate_simple (rank, dims, NULL); 

    	// Write the data to the extended portion of dataset
    	errChk( H5Dwrite(dataset_id, mem_type_id, memspace, filespace,H5P_DEFAULT, dset_data) );
		errChk( AddWaveformAttrArr(dataset_id, numelements, size[1]) );     
   }
   
  
   // Close the dataset
   errChk(H5Dclose(dataset_id));
   
   // Close the group ids
   errChk(H5Gclose (group_id));
   
   // Close the dataspace
   errChk(H5Sclose(dataspace_id));
   
   // Close the file
   errChk(H5Fclose(file_id));
   
   OKfree(dims);
   OKfree(maxdims);
   OKfree(dimsr);
   OKfree(indices);
   OKfree(offset);
   OKfree(size);
   
   return error;
   
Error:
   
   OKfree(dims);
   OKfree(maxdims);
   OKfree(dimsr);
   OKfree(indices);
   OKfree(offset);
   OKfree(size);
   
   return error;
}

static int CreateRootGroup (hid_t file_id, char *group_name)
{
	herr_t		error		= 0;   
	hid_t		group_id	= 0;
	
	errChk( group_id = H5Gcreate(file_id, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
  	errChk( H5Gclose (group_id) );
	
	return 0;
Error:
	return error;
}

static int CreateRelativeGroup (hid_t parentgroup_id, char *group_name)
{
	herr_t      error		= 0;   
	hid_t		group_id	= 0;
	
	errChk( group_id = H5Gcreate2(parentgroup_id, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
  	errChk(H5Gclose (group_id));
	
	return 0;
Error:
	return error;
}

static int CreateHDF5Group (char* filename, TC_DS_Data_type* dsdata, hid_t* file_id, hid_t* groupid)
{
   	herr_t      			error			= 0; 
	hid_t* 					group_id 		= NULL;
	char**					groupname		= NULL; 
	char* 					pch				= NULL;
	int 					groupdepth		= 0;
	
	
   	// Open an existing file
   	errChk( *file_id = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT) );
   	
	nullChk( groupname = malloc(MAXITERDEPTH * sizeof(char*)) );
	
  	pch = strtok (GetDSdataGroupname(dsdata),"/");
	while (pch != NULL) {
    	groupname[groupdepth] = StrDup(pch);
		groupdepth++;
    	pch = strtok (NULL, "/");
	}
	
	nullChk( group_id = malloc((groupdepth+1)*sizeof(hid_t)) );
	//create groups if necessary
	for(int i = 0; i < groupdepth; i++)
	    group_id[i] = 0;
   	
	group_id[0] = *file_id;
	
	for(int i = 0; i < groupdepth; i++){
		// datasetname shouldn't have slashes in it
		groupname[i] = RemoveSlashes(groupname[i]);
		// open group if it exists
		group_id[i+1] = H5Gopen2(group_id[i], groupname[i], H5P_DEFAULT );
		
		if (group_id[i+1]<0) {
			//create group if it didn't exist (reply is negative)
			errChk( group_id[i+1] = H5Gcreate2(group_id[i], groupname[i], H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
			*groupid = group_id[i+1]; 
			
		} else 
			*groupid=group_id[i+1]; 
	   
   }
   
   
   // Close all group ids except the last one
   for(int i = 0; i < groupdepth - 1; i++){
		errChk( H5Gclose(group_id[i+1]) );
		OKfree(groupname[i]);
   }
   
   OKfree(groupname[groupdepth-1]); 
   OKfree(groupname);
   OKfree(group_id);
   
   return 0;
   
Error:
   
	// Close the file. */
	H5Fclose(*file_id);
	OKfree(groupname);
	OKfree(group_id);  
   
	return error;
}

static int CreateStringAttr (hid_t dataset_id, char *attr_name, char* attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	hid_t		datatype		= 0;
	size_t  	dims			= 0; 	// Return value
	hsize_t  	dimsa[1]		= {1}; 
	
	dims 					= strlen(attr_data);
	errChk( dataspace_id 	= H5Screate_simple (1, dimsa, NULL) );
   	datatype 				= H5Tcopy(H5T_C_S1);
   	errChk( H5Tset_size(datatype, dims) );
   	errChk( H5Tset_strpad(datatype, H5T_STR_NULLTERM) );
   	errChk( attribute_id = H5Acreate2(dataset_id, attr_name, datatype, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
   	errChk( H5Awrite(attribute_id, datatype, attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateLongAttr (hid_t dataset_id, char *attr_name, long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_LONG, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_LONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateULongAttr (hid_t dataset_id, char *attr_name, unsigned long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_ULONG, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_ULONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateLLongAttr (hid_t dataset_id, char *attr_name, long long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_LLONG, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_LLONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateULLongAttr (hid_t dataset_id, char *attr_name, unsigned long long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_ULLONG, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_ULLONG, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) );
	
	return 0;
Error:
	return error;
}

static int CreateULLongAttrArr (hid_t dataset_id, char *attr_name, unsigned long long attr_data, size_t size)
{
	herr_t      			error				= 0;
	hid_t     				dataspace_id		= 0;  
	hid_t					attribute_id		= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;			     // Return value
	
	nullChk( attr_array = malloc(size * sizeof(unsigned long long)) );
	
	errChk( dataspace_id = H5Screate(H5S_SIMPLE) );
    ret	= H5Sset_extent_simple(dataspace_id, 1, dims, NULL);
 
	attribute_id = H5Aopen( dataset_id, attr_name, 0);
	if (attribute_id < 0) {
		//attribute didn't extist, create one
		errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_ULLONG, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
		errChk( H5Awrite(attribute_id, H5T_NATIVE_ULLONG, &attr_data) );  
	} else {
		errChk( H5Aread(attribute_id,H5T_NATIVE_ULLONG,attr_array) );
		//delete previous attribute
		H5Adelete(dataset_id,attr_name);
		 
		errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_ULLONG, dataspace_id, H5P_DEFAULT,H5P_DEFAULT) );  
		attr_array[size-1] = attr_data;
		errChk( H5Awrite(attribute_id, H5T_NATIVE_ULLONG, attr_array) );
	}
   
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace.
    errChk( H5Sclose(dataspace_id) ); 
	
	OKfree(attr_array);
	
	return 0;
Error:
	OKfree(attr_array);
	return error;
}

static int ReadNumElemAttr (hid_t dataset_id, hsize_t index, unsigned long long* attr_data, size_t size)
{
	herr_t      			error				= 0;
	hid_t     				dataspace_id		= 0;  
	hid_t					attribute_id		= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size*sizeof(unsigned long long)) );
	
	errChk( dataspace_id = H5Screate(H5S_SIMPLE) );
    ret = H5Sset_extent_simple(dataspace_id, 1, dims, NULL);

	//open attribute
	errChk( attribute_id = H5Aopen(dataset_id, NUMELEMENTS_NAME, 0) ); 
	errChk( H5Aread(attribute_id,H5T_NATIVE_ULLONG,attr_array) );
	*attr_data = attr_array[index];	 
   
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	OKfree(attr_array);
	
	return 0;
Error:
	OKfree(attr_array);
	return error;
}

static int WriteNumElemAttr (hid_t dataset_id, hsize_t index, unsigned long long attr_data, hsize_t size)
{
	herr_t      			error				= 0;
	hid_t     				dataspace_id		= 0;  
	hid_t					attribute_id		= 0;
	hsize_t     			dims[1]				= {size};
	unsigned long long*		attr_array			= NULL;
	herr_t  				ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size * sizeof(unsigned long long)) );
	
	errChk( dataspace_id = H5Screate(H5S_SIMPLE) );
    ret  = H5Sset_extent_simple(dataspace_id, 1, dims, NULL);

	//open attribute
	errChk( attribute_id = H5Aopen(dataset_id, NUMELEMENTS_NAME, NULL) );
	errChk( H5Aread(attribute_id,H5T_NATIVE_ULLONG,attr_array) ); 
	attr_array[index] = attr_data;
	errChk( H5Awrite(attribute_id,H5T_NATIVE_ULLONG,attr_array) );
		 
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	OKfree(attr_array);
	
	return 0;
Error:
	OKfree(attr_array);
	return error;
}

static int CreateIntAttr (hid_t dataset_id, char *attr_name, int attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_INT, &attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateUIntAttr (hid_t dataset_id, char *attr_name, unsigned int attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_UINT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_UINT, &attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateShortAttr (hid_t dataset_id, char *attr_name, long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_SHORT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_SHORT, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateUShortAttr (hid_t dataset_id, char *attr_name, unsigned long attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_USHORT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_USHORT, &attr_data) ); 
	
	// Close the attribute
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateFloatAttr(hid_t dataset_id,char *attr_name, float attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
    errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_FLOAT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_FLOAT, &attr_data) );
	
	// Close the attribute. */
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace. */
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateDoubleAttr (hid_t dataset_id, char *attr_name, double attr_data)
{
	herr_t      error			= 0;
	hid_t     	dataspace_id	= 0;  
	hid_t       attribute_id	= 0;
	
	errChk( dataspace_id = H5Screate(H5S_SCALAR) );
	errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT));
    errChk( H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, &attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

static int CreateDoubleAttrArr(hid_t dataset_id, char *attr_name, double attr_data, hsize_t size)
{
	herr_t      error				= 0;
	hid_t     	dataspace_id		= 0;  
	hid_t		attribute_id		= 0;
	hsize_t     dims[1]				= {size};
	double*		attr_array			= NULL;
	herr_t  	ret					= 0;                // Return value
	
	nullChk( attr_array = malloc(size*sizeof(double)) );
	
	errChk( dataspace_id = H5Screate(H5S_SIMPLE) );
    ret = H5Sset_extent_simple(dataspace_id, 1, dims, NULL);

	if (size==1) {
		//create attribute
    	errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
		errChk( H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, &attr_data) );  
	} else {
		//open attribute
		errChk( attribute_id = H5Aopen( dataset_id, attr_name,NULL) ); 
		errChk( H5Aread(attribute_id, H5T_NATIVE_DOUBLE, attr_array) );
		 //test
		H5Adelete(dataset_id, attr_name);
		 
		errChk( attribute_id = H5Acreate2(dataset_id, attr_name, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT,H5P_DEFAULT) );  
		attr_array[size-1] = attr_data;
		errChk( H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, attr_array) );      
	}
   
	// Close the attribute
  	errChk( H5Aclose(attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	OKfree(attr_array);
	
	return 0;
Error:
	OKfree(attr_array);
	return error;
}

static int CreatePixelSizeAttr (hid_t dataset_id, double* attr_data)
{
	herr_t      error				= 0;
	hid_t     	dataspace_id		= 0;  
	hid_t       attribute_id		= 0;
	int 		rank				= 1;
	hsize_t 	dims[]			    = {3};
	hsize_t 	maxdims[]			= {3}; 
	
	errChk(dataspace_id = H5Screate_simple(rank, dims, maxdims) );
	   
    errChk( attribute_id = H5Acreate2(dataset_id, PIXELSIZE_NAME, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT) );
    errChk( H5Awrite(attribute_id, H5T_NATIVE_DOUBLE, attr_data) );
	
	// Close the attribute
  	errChk( H5Aclose (attribute_id) );
	// Close the dataspace
    errChk( H5Sclose(dataspace_id) ); 
	
	return 0;
Error:
	return error;
}

// Adds waveform information as dataset attributes
static int AddWaveformAttr (hid_t dataset_id, Waveform_type* waveform)
{
	int 	error			= 0;
	char* 	name			= GetWaveformName(waveform);			// Name of signal represented by the waveform.    
	char* 	unitname		= GetWaveformPhysicalUnit(waveform);	// Physical SI unit such as V, A, Ohm, etc. 
	double 	datetimestamp	= GetWaveformDateTimestamp(waveform);	// Number of seconds since midnight, January 1, 1900 in the local time zone.
	double 	samplingrate	= GetWaveformSamplingRate(waveform);  	// Sampling rate in [Hz]. If 0, sampling rate is not given.   
	 
	if (name) 
		errChk( CreateStringAttr(dataset_id,"Name", name) );
	
	if (unitname) 
		errChk( CreateStringAttr(dataset_id,"Units", unitname) );
	
	if (datetimestamp) 
		errChk( CreateDoubleAttr(dataset_id,"DateTimeStamp", datetimestamp) );
	
	 errChk( CreateDoubleAttr(dataset_id,"SamplingRate", samplingrate) );
		
	 return 0;
Error:
	 return error;
}

static char* RemoveSlashes (char* name)
{
	size_t	len = strlen(name);
	
	for (int i = 0; i < len; i++)
		if (name[i] == '/') name[i]='_';
	
	return name;
}

static int AddWaveformAttrArr (hid_t dataset_id, unsigned long long numelements, hsize_t size)
{
	 int error	= 0;
	 
	 errChk( CreateULLongAttrArr(dataset_id, NUMELEMENTS_NAME, numelements, size) ); 		
	 
	 return 0;
Error:
	 return error;
}

//converts pixel size info into attributes, 
//and adds them to the dataset
int AddImagePixSizeAttributes(hid_t dataset_id,Image_type*	receivedimage)
{
	 int 	error				= 0; 
	 double	pixSize				= GetImagePixSize(receivedimage);		// Image pixel size in [um].
	

	 double pixsize[3];
	 pixsize[0]=pixSize;   //Z size
	 pixsize[1]=pixSize;   //X size  
	 pixsize[2]=pixSize;   //Y size  

	 errChk(CreatePixelSizeAttr(dataset_id,pixsize));          
	 return error;
Error:
	 return error;
}	

//converts all image information into attributes, 
//and adds them to the dataset
int AddImageAttributes(hid_t dataset_id,Image_type*	receivedimage,hsize_t size)
{
	 int 	error				= 0; 
	 double	imgTopLeftXCoord	= 0;	// Image top-left corner X-Axis coordinates in [um].
	 double	imgTopLeftYCoord	= 0;	// Image top-left corner Y-Axis coordinates in [um].
	 double	imgZCoord			= 0;		// Image z-axis (height) location in [um].
	 
	 GetImageCoordinates(receivedimage, &imgTopLeftXCoord, &imgTopLeftYCoord, &imgZCoord);
		 
	 errChk(CreateDoubleAttrArr(dataset_id,"TopLeftXCoord", imgTopLeftXCoord,size));    	
	 errChk(CreateDoubleAttrArr(dataset_id,"TopLeftYCoord", imgTopLeftYCoord,size));  
	 errChk(CreateDoubleAttrArr(dataset_id,"ZCoord", imgTopLeftXCoord,size)); 
          
	 return error;
Error:
	 return error;
}	

int WriteHDF5Image(char *filename, char* dataset_name, TC_DS_Data_type* dsdata, Image_type* receivedimage) 
{ 
	int   					error        			= 0;
	hid_t 					file_id					= 0;
	hid_t					group_id				= 0;
	char**					groupname				= NULL; 
	hid_t       			cparms; 
   	hid_t 					mem_type_id 			= 0;
   	hid_t 					type_id					= 0;
   	hid_t        			filespace;
   	hid_t					memspace;
   	hsize_t					dimsr[3];
   	hsize_t      			size[3];
   	hsize_t      			offset[3];
	hid_t					dataset_id;
	hid_t					dataspace_id;
	int 					height					= 0;
    int 					width					= 0;
	
	GetImageSize(receivedimage, &width, &height);
	
    int						numimages				= 1;  			
    void* 					dset_data				= NULL;
    hsize_t      			imagedims[3]			= {1,height,width};
	hsize_t      			stackdims[3]			= {numimages,height,width}; 
    hsize_t      			maxstackdims[3] 		= {H5S_UNLIMITED,H5S_UNLIMITED,H5S_UNLIMITED};
	

	// opens the file and the group, 
	// creates one if necessary
	errChk(CreateHDF5Group(filename,dsdata,&file_id,&group_id));
   
	ImageTypes type = GetImageType(receivedimage);
   
    // Modify dataset creation properties, i.e. enable chunking
	cparms = H5Pcreate (H5P_DATASET_CREATE);
	errChk(H5Pset_chunk ( cparms, 3, imagedims));
  
	//datatype switch
	switch (type) {
		   
		case Image_UChar:
			type_id=H5T_STD_U8BE;
		   	mem_type_id=H5T_NATIVE_UCHAR;
			break;
		   
		case Image_Short:
			type_id=H5T_STD_I16BE;
			mem_type_id=H5T_NATIVE_SHORT;
			break;
		   
		case Image_UShort:
			type_id=H5T_STD_U16BE;
			mem_type_id=H5T_NATIVE_USHORT;
			break;
		   
		case Image_Int:
			type_id=H5T_STD_I32BE;
			mem_type_id=H5T_NATIVE_INT;
			break;
		   
		case Image_UInt:
			type_id=H5T_STD_U32BE;
			mem_type_id=H5T_NATIVE_UINT;
			break;
		   
		case Image_Float:
			type_id=H5T_IEEE_F32BE;
			mem_type_id=H5T_NATIVE_FLOAT;
			break;
	}
   
	// open an existing or new dataset
	dataspace_id = H5Screate_simple(3, stackdims, maxstackdims);

   	// datasetname shouldn't have slashes in it
	dataset_name = RemoveSlashes(dataset_name);
	dset_data = GetImagePixelArray(receivedimage);
   
	// open the dataset if it exists
	dataset_id=H5Dopen2(group_id, dataset_name, H5P_DEFAULT );
	if (dataset_id < 0) {
		dataset_id = H5Dcreate2(group_id, dataset_name,type_id, dataspace_id,H5P_DEFAULT, cparms,H5P_DEFAULT);
		if (dataset_id < 0)
	   		return -1;
   	
		// write the dataset
		errChk(H5Dwrite(dataset_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data));
   		// add attributes to dataset
		errChk(AddImagePixSizeAttributes(dataset_id,receivedimage));
		errChk(AddImageAttributes(dataset_id,receivedimage,1)); 
  	} else {
		// dataset existed, have to add the data to the current data set
	   	// get the size of the dataset
		filespace = H5Dget_space (dataset_id);
		errChk(H5Sget_simple_extent_dims (filespace, dimsr, NULL));
		size[0] = dimsr[0]+1;  
		size[1] = dimsr[1];  
		size[2] = dimsr[2];
		errChk(H5Dset_extent (dataset_id, size));

    	// select a hyperslab in extended portion of dataset
    	filespace = H5Dget_space (dataset_id);
    	offset[0] = dimsr[0]; 
    	offset[1] = 0;
		offset[2] = 0;  
    	errChk(H5Sselect_hyperslab (filespace, H5S_SELECT_SET, offset, NULL,imagedims, NULL));  

    	// define memory space
   		memspace = H5Screate_simple (3, imagedims, NULL); 

    	// Write the data to the extended portion of dataset
    	errChk(H5Dwrite (dataset_id, mem_type_id, memspace, filespace,H5P_DEFAULT, dset_data));
		errChk(AddImageAttributes(dataset_id,receivedimage,size[0]));
   	}
	   
    // close the dataset
	errChk(H5Dclose(dataset_id));
   
	// close the group ids
	errChk(H5Gclose (group_id));
   
	free(groupname);
   
	// close the dataspace
	errChk(H5Sclose(dataspace_id));
	
	// close the file. */
	errChk(H5Fclose(file_id));
   
	return error;
   
Error:
	
   return error;
}


