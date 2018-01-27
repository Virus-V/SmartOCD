/*
 * usb_test.c
 *
 *  Created on: 2018-1-27
 *      Author: virusv
 */


#include <stdio.h>
#include <stdlib.h>

#include "smart_ocd.h"
#include "debugger/usb.h"
#include "misc/log.h"
char indata[64], outdata[] = {0x0,0x04};

int usb_main(){
	USBObject *doggle;
	unsigned int readep, outep;


	log_set_level(LOG_TRACE);
	doggle = NewUSBObject("test doggle(CMSIS-DAP)");
	if(doggle == NULL){
		log_fatal("make usb object failed");
		exit(1);
	}
	if(USBOpen(doggle, 0xc251, 0xf001, NULL) != TRUE){
		log_fatal("Couldnt open usb device.");
		exit(1);
	}

	if(USBSetConfiguration(doggle, 0) != TRUE){
		log_fatal("USBSetConfiguration Failed.");
		exit(1);
	}

	if(USBClaimInterface(doggle, &readep, &outep, 3, 0, 0, 3) != TRUE){
		log_fatal("Claim interface failed.");
		exit(1);
	}
	log_debug("read ep:%d, write ep:%d.", readep, outep);
	log_debug("write to doggle %d byte.", USBInterruptTransfer(doggle, outep, outdata, 2, 0));
	log_debug("read from doggle %d byte.", USBInterruptTransfer(doggle, readep, indata, 64, 0));
	log_debug("receive data:%x %x %x %x %x %x", indata[0], indata[1], indata[2],indata[3], indata[4], indata[5]);
	log_debug("serial num:%s", indata+2);
	USBClose(doggle);
	FreeUSBObject(doggle);
	exit(0);
}