//==============================================================================
//
// Title:		Zaber.c
// Purpose:		A short description of the implementation.
//
// Created on:	22-7-2015 at 16:01:44 by Vinod Nigade.
// Copyright:	VU University Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "Zaber.h"

//==============================================================================
// Constants

//==============================================================================
// Types

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

//==============================================================================
// Global variables

//==============================================================================
// Global functions
inline void print_decoded_reply(struct za_reply decoded_reply);

/************************************************************
 * Zaber Device functions will follow after this section  	*
 ************************************************************/

void
poll_until_idle(z_port port)
{
    char reply[256] = { 0 };
	struct za_reply decoded_reply;
    //const struct timespec ts = { 0, 100000000 }; /* 100mil nanosec = 100ms */
    
	/* We use za_decode() to decode this string into more manageable parts,
	 * sorting them into the fields of a za_reply struct, then we test
	 * the device_status field. Possible values for device_status are "IDLE"
	 * and "BUSY". */
    for(;;)
    {
        za_send(port, "/\n");
        za_receive(port, reply, sizeof(reply));
		za_decode(&decoded_reply, reply);
		
        if(strncmp(decoded_reply.device_status, "BUSY", 4) == 0)
		{
			Sleep(100);	/* TODO: Check non-sleep way */
		}
        else 
		{
			break;
		}
    }
}

inline void
print_zaber_data(uint8_t *reply, int length)
{
	printf("Received Data from Zaber:\t"); 
	for (int i = 0; i < length; i++) {
		printf("%2x", reply[i]);			
	}
	
	printf("\n");
	return;
}

inline void
print_decoded_reply(struct za_reply decoded_reply)
{
	/* TODO: Log this without printf */
	printf("%c %d %d %s %s %s %s\n", decoded_reply.message_type, decoded_reply.device_address,
		   			decoded_reply.axis_number, decoded_reply.reply_flags, decoded_reply.device_status,
					decoded_reply.warning_flags, decoded_reply.response_data);
}

inline void
send_cmd(z_port port, char *cmd, char *data_buf)
{   
	struct za_reply decoded_reply;
	char reply[256] = { 0 };
	
	/* TODO : Error Check */
	za_send(port, cmd);
	za_receive(port, reply, sizeof(reply));
	za_decode(&decoded_reply, reply);
	
	print_decoded_reply(decoded_reply);
	poll_until_idle(port);
	
	if (data_buf != NULL) {
		strncpy(data_buf, decoded_reply.response_data, 128);
	}
	return;
}

int
send_MoveRel_cmd(z_port port, int device, int position) 
{
	char	cmd[CMD_LEN];
	
	sprintf(cmd, "/%d move rel %d\n", device, position);
	LOG_MSG1(9, "Following move relative command is requested-> %s\n", cmd); 
	send_cmd(port, cmd, NULL);	/* TODO : Error check */
	
	return 0;
}

int
send_MoveABS_cmd(z_port port, uint32_t abs_position)
{
	char	cmd[CMD_LEN];
	
	sprintf(cmd, "/move abs %u\n", abs_position);
	
	LOG_MSG1(9, "Following absolute move command is requested--> %s", cmd);
	send_cmd(port, cmd, NULL);	/* TODO : Error check */
	
	return 0;
}

int
get_device_data(z_port port, int device, char	*sub_cmd)
{
	char	cmd[CMD_LEN];
	char	response_data[128];
	
	//sprintf(cmd, "/%d get %s\n", device, sub_cmd);
	sprintf(cmd, "/00 0 get peripheralid\n"); 
	
	LOG_MSG1(9, "Get Limit command %s", cmd);
	send_cmd(port, cmd, response_data);
	
	/* Do processing on received data, like converting it to int and return */
	//
	return 0;
}

/* Init zaber device */
int
init_zaber_device(zaber_device_t *z_dev)
{   
	uint8_t	reply[256] = { 0 };
	int		ret = 0;
	
	if (za_connect(&z_dev->port, z_dev->device_name) != Z_SUCCESS) {
		printf("Could not connect to device %s.\n", z_dev->device_name);
		return -1;
	}
	
	
	/* Biinary Protocol
	//uint8_t	cmd[6] = { 0, 1, 0, 0, 0, 0 };
	//uint8_t	cmd[6];
	//zb_encode(cmd, 0, 1, 0);

	ret = zb_receive(z_dev->port, reply);
	if (ret == Z_ERROR_SYSTEM_ERROR) {
		printf("Read no reply from device %s. ", z_dev->device_name);
		return -1;
	}
	*/
	
	za_send(z_dev->port, "/home\n");
	za_receive(z_dev->port, reply, sizeof(reply));
	if (reply[0] == '\0') {
		printf("Read no reply from device %s. "
				"It may be running at a different baud rate "
				"and/or in the binary protocol.\n", z_dev->device_name);
		return -1;
	}
	
    poll_until_idle(z_dev->port);   /* TODO: No need to have this when we are going to other thing */
	
	/* Get all device properties
	 */
	get_device_data(z_dev->port, 2, "x");
	
	z_dev->VALID_DEVICE = TRUE;
	LOG_MSG(9, "Zaber device connected successfuly\n");
	return 0;
}

/* Close Zaber Device */
int
close_zaber_device(zaber_device_t *z_dev)
{
	if (!z_dev->VALID_DEVICE) {
		return -1;
	}
	
	za_disconnect(z_dev->port);
	z_dev->VALID_DEVICE = FALSE;
	
	return 0;
}


/* Get available COM port.
 * Not the best way but couldn't find other way
 */
void
get_available_COMport(char *comport[])
{
	z_port	port;
	char	port_name[DEVICE_NAME_LEN];
	int		j = 0;
	
	/* Iterate over COM0 to COM20 port and check if they are
	 * available.
	 */
	for (int i = 0 ; i < 20; i++) {
		sprintf(port_name, "COM%d", i);
		
		port = CreateFileA(port_name,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			0);
		
		if (port == INVALID_HANDLE_VALUE) {
			LOG_MSG1(9, "Port %s cannot be opened", port_name);
		} else {
			comport[j++] = StrDup(port_name);
			SYSCALL(CloseHandle(port));
		}
	}
	
	comport[j] = NULL;
	
	return;
}
