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

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

static char indata[64], outdata[][3] = {
		//{0x01, 0x00, 0x01},
		//{0x01, 0x01, 0x01},
		//{0x00, 0xf0},
		//{0x00, 0xf1},
		//{0x00, 0xf2},
		{0x00, 0x04},
};

int main(){
	USBObject *doggle;
	int tmp;

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

	if(USBClaimInterface(doggle, 3, 0, 0, 3) != TRUE){
		log_fatal("Claim interface failed.");
		exit(1);
	}
	for(tmp=0; tmp < ARRAY_COUNT(outdata); tmp++){
		log_debug("write to doggle %d byte.", doggle->Write(doggle, outdata[tmp], sizeof(outdata[tmp]), 0));
		log_debug("read from doggle %d byte.", doggle->Read(doggle, indata, 64, 0));
		log_debug("receive data:");
		print_bulk(indata, sizeof(indata), 8);
	}

	USBClose(doggle);
	FreeUSBObject(doggle);
	exit(0);
}
