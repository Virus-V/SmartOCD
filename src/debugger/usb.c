/*
 * SmartOCD
 * usb.c
 *
 *  Created on: 2018-1-5
 *  Author:  Virus.V <virusv@live.com>
 * LICENSED UNDER GPL.
 */

#ifdef HAVE_CONFIG
#include "global_config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "smart_ocd.h"
#include "usb.h"
#include "misc/log.h"

// 设备列表
static libusb_device *devs[];
/*
 * 新建Debugger USB对象
 * desc: 设备的描述
 */
USBObject * NewUSBObject(char const *desc){
	USBObject *object = calloc(sizeof(USBObject));
	if(object == NULL){
		// 内存分配失败
		log_error("Failed to make USB Object");
		return NULL;
	}
	object->deviceDesc = desc ? strdup(desc) : NULL;
	return object;
}

/*
 * 销毁USB对象
 * object: 要销毁的USB对象
 */
void FreeUSBObject(USBObject *object){
	// 关闭连接
	if(object->deviceDesc != NULL) free(object->deviceDesc);
	free(object);
}

/**
 * 检查设备的序列号与指定序列号是否一致
 */
static BOOL usbStrDescriptorMatch(libusb_device_handle *devHandle, uint8_t descIndex, const char *snString) {
	int retcode;
	BOOL matched;
	// 描述字符串缓冲区
	char descString[256+1];	// TODO TOFIX：较大的数组在栈中分配！

	if (descIndex == 0) return FALSE;

	retcode = libusb_get_string_descriptor_ascii(devHandle, descIndex, (unsigned char *)descString, sizeof(descString)-1);
	if (retcode < 0) {
		log_error("libusb_get_string_descriptor_ascii() return code:%d", retcode);
		return FALSE;
	}
	// 截断字符串
	descString[256] = 0;

	if (!strncmp(snString, descString, sizeof(descString)) == 0){
		log_warn("The device serial number '%s' is different from the specified serial number '%s'.", descString, snString);
		return FALSE;
	}else return TRUE;
}

/**
 * 根据pid和vid打开USB设备
 */
BOOL USBOpen(USBObject *usbObj, const uint16_t vid, const uint16_t pid, const char *serial) {
	int devCount, index;
	libusb_device_handle *devHandle = NULL;

	// 断言usbObj已经初始化
	assert(usbObj != NULL);

	if (libusb_init(&usbObj->libusbContext) < 0){
		log_error("libusb_init() failed.");
		return FALSE;
	}

	// 获得USB设备总数
	devCount = libusb_get_device_list(usbObj->libusbContext, &devs);
	if(devCount < 0){
		log_error("libusb_get_device_list() failed. error:%s.", libusb_error_name(devCount));
	}else{
		log_debug("Found %d usb devices.", devCount);
	}

	for (index = 0; index < devCount; index++) {
		struct libusb_device_descriptor devDesc_tmp;
		int retCode;

		if (libusb_get_device_descriptor(devs[index], &devDesc_tmp) != 0)
			continue;

		// 检查该usb设备的身份
		//if (!usbMatchDev(&devDesc_tmp, vids, pids))
		//	continue;
		if(devDesc_tmp.idProduct != pid || devDesc_tmp.idVendor != vid)
			continue;

		retCode = libusb_open(devs[index], &devHandle);

		if (retCode) {
			log_warn("libusb_open() error:%s,vid:%x,pid:%x.", libusb_error_name(retCode), devDesc_tmp.idVendor, devDesc_tmp.idProduct);
			continue;
		}
		// 检查设备序列号
		if (serial != NULL && !usbStrDescriptorMatch(devHandle, devDesc_tmp.iSerialNumber, serial)) {
			libusb_close(devHandle);
			continue;
		}
		// 找到设备，初始化USB对象
		usbObj->devHandle = devHandle;
		usbObj->pid = devDesc_tmp.idProduct;
		usbObj->vid = devDesc_tmp.idVendor;
		usbObj->serialNum = serial ? strdup(serial) : NULL;

		// 释放设备列表
		libusb_free_device_list(devs, 1);
		return TRUE;
	}
	// 关闭libusb context
	libusb_exit(usbObj->libusbContext);
	usbObj->libusbContext = NULL;
	return FALSE;
}

// 关闭USB
void USBClose(USBObject *usbObj) {
	assert(usbObj != NULL
			&& usbObj->devHandle != NULL
			&& usbObj->libusbContext != NULL);

	/* Close device */
	libusb_close(usbObj->devHandle);
	usbObj->devHandle = NULL;
	libusb_exit(usbObj->libusbContext);
	usbObj->libusbContext = NULL;
}

/**
 * USB控制传输
 */
int USBControlTransfer(USBObject *usbObj,
		uint8_t requestType,
		uint8_t request,
		uint16_t wValue,
		uint16_t wIndex,
		char *data,
		uint16_t dataLength,
		unsigned int timeout)
{
	int retCode;
	assert(usbObj != NULL && usbObj->devHandle != NULL);

	retCode = libusb_control_transfer(usbObj->devHandle, requestType, request, wValue, wIndex, (unsigned char *)data, dataLength, timeout);
	if(retCode < 0){
		log_warn("%s", libusb_error_name(retCode));
		return 0;
	}
	return retCode;
}

/**
 * 读/写数据块
 */
int USBBulkTransfer(USBObject *usbObj, int endpoint, char *data, int dataLength, int timeout) {
	int transferred = 0, retCode;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	retCode = libusb_bulk_transfer(usbObj->devHandle, endpoint, (unsigned char *)data, dataLength, &transferred, timeout);
	if (retCode < 0){
		log_warn("%s", libusb_error_name(retCode));
	}
	return transferred;
}

/**
 * 中断传输
 */
int USBInterruptTransfer(USBObject *usbObj, int endpoint, char *data, int dataLength, int timeout) {
	int transferred = 0, retCode;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	retCode = libusb_interrupt_transfer(usbObj->devHandle, endpoint, (unsigned char *)data, dataLength, &transferred, timeout);
	if (retCode < 0){
		log_warn("%s", libusb_error_name(retCode));
	}
	return transferred;
}

/**
 * 设置活跃配置
 */
BOOL USBSetConfiguration(USBObject *usbObj, int configurationIndex) {
	struct libusb_config_descriptor *config = NULL;
	int currentConfig = 0, retCode;
	libusb_device *dev = NULL;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	// 获得Device
	dev = libusb_get_device(usbObj->devHandle);

	// 获取当前配置
	retCode = libusb_get_configuration(usbObj->devHandle, &currentConfig);
	if (retCode < 0){
		log_warn("%s", libusb_error_name(retCode));
		return FALSE;
	}
	// TODO 判断是否claim interface，如果claim interface要先release

	retCode = libusb_get_config_descriptor(dev, configurationIndex, &config);
	if (retCode < 0){
		log_warn("%s", libusb_error_name(retCode));
		return FALSE;
	}
	// TODO FIX
	if (currentConfig != config->bConfigurationValue){
		if((retCode = libusb_set_configuration(usbObj->devHandle, config->bConfigurationValue)) < 0){
			log_warn("%s", libusb_error_name(retCode));
			libusb_free_config_descriptor(config);
			return FALSE;
		}
	}
	// 释放
	libusb_free_config_descriptor(config);
	return TRUE;
}

/**
 * 声明interface
 */
BOOL USBClaimInterface(USBObject *usbObj,
		unsigned int *ReadEndPoint_out,
		unsigned int *WriteEndPoint_out,
		int IFClass, int IFSubclass, int IFProtocol, int transType)
{
	struct libusb_device *dev = NULL;
	struct libusb_config_descriptor *config;
	int retCode;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	dev = libusb_get_device(usbObj->devHandle);

	retCode = libusb_get_config_descriptor(dev, 0, &config);
	if(retCode != 0){
		log_warn("%s", libusb_error_name(retCode));
		return FALSE;
	}
	// TODO 判断是否claim interface，如果claim interface要先release
	for (int i = 0; i < (int)config->bNumInterfaces; i++) {
		// 获得interface
		const struct libusb_interface *interface = &config->interface[i];
		const struct libusb_interface_descriptor *interfaceDesc = &interface->altsetting[0];
		if(interface->num_altsetting > 1){
			log_info("The interface has more than one settings.");
		}

		// 匹配class
		if ((IFClass > 0 && interfaceDesc->bInterfaceClass != IFClass) ||
			(IFSubclass > 0 && interfaceDesc->bInterfaceSubClass != IFSubclass) ||
			(IFProtocol > 0 && interfaceDesc->bInterfaceProtocol != IFProtocol))
		{
			continue;
		}

		for (int k = 0; k < (int)interfaceDesc->bNumEndpoints; k++) {
			uint8_t epNum; // Endpoint Number
			const struct libusb_endpoint_descriptor *epDesc;

			epDesc = &interfaceDesc->endpoint[k];

			if (transType > 0 && (epDesc->bmAttributes & 0x3) != transType)
				continue;

			epNum = epDesc->bEndpointAddress;

			if (epNum & 0x80){
				*ReadEndPoint_out = epNum;
				log_debug("usb end point 'in' %02x", epNum);
			}else{
				*WriteEndPoint_out = epNum;
				log_debug("usb end point 'out' %02x", epNum);
			}

			if (*ReadEndPoint_out && *WriteEndPoint_out) {
				log_debug("Claiming interface %d", (int)interfaceDesc->bInterfaceNumber);
				libusb_claim_interface(dev, (int)interfaceDesc->bInterfaceNumber);
				libusb_free_config_descriptor(config);
				return TRUE;
			}

		} //for (int k = 0; k < (int)interfaceDesc->bNumEndpoints; k++)
	} //for (int i = 0; i < (int)config->bNumInterfaces; i++)
	libusb_free_config_descriptor(config);

	return FALSE;
}

/**
 * 获得PID和VID
 */
BOOL USBGetPidVid(libusb_device *dev, uint16_t *pid_out, uint16_t *vid_out) {
	struct libusb_device_descriptor devDesc;
	if(pid_out == NULL && vid_out == NULL){
		log_info("Nothing to do.");
		return TRUE;
	}
	if (libusb_get_device_descriptor(dev, &devDesc) == 0) {
		if(pid_out != NULL) *pid_out = devDesc.idProduct;
		if(vid_out != NULL) *vid_out = devDesc.idVendor;
		return TRUE;
	}

	return TRUE;
}


