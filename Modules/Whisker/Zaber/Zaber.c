//==============================================================================
//
// Title:		Zaber.c
// Purpose:		It contains function specific to Zaber device. 
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
    char	reply[RESPONSE_LEN] = { 0 };
	struct	za_reply decoded_reply;
	char	cmd[CMD_LEN];
	int		i = 2;
    
	/* We use za_decode() to decode this string into more manageable parts,
	 * sorting them into the fields of a za_reply struct, then we test
	 * the device_status field. Possible values for device_status are "IDLE"
	 * and "BUSY". 
	 */
	while (i > 0) {
		sprintf(cmd, "/%d\n", i);
        za_send(port, cmd);
        za_receive(port, reply, sizeof(reply));
		za_decode(&decoded_reply, reply);
		
        if(strncmp(decoded_reply.device_status, "BUSY", 4) == 0) {
			ZABER_DELAY(100);	/* TODO: Check non-sleep way */
		} else {
			i--;
		}
    }
}

inline void
print_decoded_reply(struct za_reply decoded_reply)
{
	if (_CVI_DEBUG_) {
		printf("%c %d %d %s %s %s %s\n", decoded_reply.message_type, 
			   			decoded_reply.device_address,
		   				decoded_reply.axis_number, 
						decoded_reply.reply_flags, 
						decoded_reply.device_status,
						decoded_reply.warning_flags, 
						decoded_reply.response_data);
	}
}

void
send_cmd(z_port port, char *cmd, char *data_buf, int mode)
{   
	struct za_reply decoded_reply;
	char reply[256] = { 0 };
	
	/* TODO : Error Check */
	za_send(port, cmd);
	za_receive(port, reply, sizeof(reply));
	za_decode(&decoded_reply, reply);
	
	print_decoded_reply(decoded_reply);
	if (mode == SYNC) {
		poll_until_idle(port);
	}
	
	if (data_buf != NULL) {
		strncpy(data_buf, decoded_reply.response_data, 128);
	}
	return;																	
}

int
send_MoveMaxMin_cmd(z_port port, int device, char *sub_cmd)
{
	char	cmd[CMD_LEN];
	sprintf(cmd, "/%d move %s\n", device, sub_cmd);
	
	send_cmd(port, cmd, NULL, ASYNC);
	return 0;
}

int
send_stop_cmd(z_port port, int device) 
{
	char	cmd[CMD_LEN];
	sprintf(cmd, "/%d stop\n", device);
	
	send_cmd(port, cmd, NULL, ASYNC);
	return 0;
}

int
send_MoveRel_cmd(z_port port, int device, int position) 
{
	char	cmd[CMD_LEN];
	
	sprintf(cmd, "/%d move rel %d\n", device, position);
	LOG_MSG1(9, "Following move relative command is requested-> %s\n", cmd); 
	send_cmd(port, cmd, NULL, ASYNC);	/* TODO : Error check */
	
	return 0;
}

int
send_MoveABS_cmd(z_port port, int device, uint32_t abs_position, int mode)
{
	char	cmd[CMD_LEN];
	
	sprintf(cmd, "/%d move abs %u\n", device, abs_position);
	
	LOG_MSG1(9, "Following absolute move command is requested--> %s", cmd);
	send_cmd(port, cmd, NULL, mode);	/* TODO : Error check */
	
	return 0;
}

/* Get device settings and return values in integer.
 * All functions assume that connection is already
 * made with the device and port is valid.
 */
int
get_device_data(z_port port, int device, char	*sub_cmd)
{
	char	cmd[CMD_LEN];
	char	response_data[RESPONSE_DATA_LEN];
	
	sprintf(cmd, "/%d get %s\n", device, sub_cmd);
	LOG_MSG1(9, "Get device data command %s", cmd);
	
	/* Draining previos responses from zaber device is neccessary
	 * Otherwise, you wont get the desired response immediately.
	 */
	za_drain(port);
	send_cmd(port, cmd, response_data, ASYNC);
	
	/* Return converted value */
	return (uint32_t)atoi(response_data);
}

void
set_device_data(z_port port, int device, char *sub_cmd, uint32_t value)
{
	char	cmd[CMD_LEN];
	
	sprintf(cmd, "%d set %s %d\n", device, sub_cmd, value);
	LOG_MSG1(9, "Set device data command %s", cmd);
	
	send_cmd(port, cmd, NULL, SYNC);
	
	return;
}

/* Init zaber device */
int
init_zaber_device(zaber_device_t *z_dev)
{   
	uint8_t	reply[256] = { 0 };
	int		ret = 0;
	
	if (za_connect(&z_dev->port, z_dev->device_name) != Z_SUCCESS) {
		LOG_MSG1(5, "Could not connect to device %s.\n", z_dev->device_name);
		return -1;
	}
	
	za_send(z_dev->port, "/home\n");
	za_receive(z_dev->port, reply, sizeof(reply));
	if (reply[0] == '\0') {
		LOG_MSG1(5, "Read no reply from device %s. "
				"It may be running at a different baud rate "
				"and/or in the binary protocol.\n", z_dev->device_name);
		return -1;
	}
	
    poll_until_idle(z_dev->port);

	z_dev->VALID_DEVICE = TRUE;
	/* Create Saved Positions list */
	z_dev->saved_positions = ListCreate(sizeof(Position_t *));
	
	LOG_MSG(9, "Zaber device connected successfuly\n");
	return 0;
}

/* Close Zaber Device */
int
close_zaber_device(zaber_device_t *z_dev)
{
	Position_t	*saved_position =  NULL;
	
	if (!z_dev->VALID_DEVICE) {
		return -1;
	}
	
	za_disconnect(z_dev->port);
	z_dev->VALID_DEVICE = FALSE;
	
	/* Free saved positions list */
	while (ListNumItems(z_dev->saved_positions)) {
		ListRemoveItem(z_dev->saved_positions, &saved_position, FRONT_OF_LIST);
		OKfree(saved_position);
	}
	OKfreeList(z_dev->saved_positions);
	
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
