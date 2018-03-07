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

#include "smart_ocd.h"
#include "debugger/usb.h"
#include "misc/log.h"

// 设备列表
static libusb_device **devs;
static int bulkWrite(USBObject *usbObj, uint8_t *data, int dataLength, int timeout);
static int bulkRead(USBObject *usbObj, uint8_t *data, int dataLength, int timeout);
static int interruptWrite(USBObject *usbObj, uint8_t *data, int dataLength, int timeout);
static int interruptRead(USBObject *usbObj, uint8_t *data, int dataLength, int timeout);
static int unsupportRW(USBObject *usbObj, uint8_t *data, int dataLength, int timeout);
static BOOL defaultInitDeinit(USBObject *usbObj);
static BOOL defaultReset(USBObject *usbObj);

/*
 * 初始化Debugger USB对象
 * desc: 设备的描述
 */
BOOL InitUSBObject(USBObject *object){
	assert(object != NULL);

	if (libusb_init(&object->libusbContext) < 0){
		log_error("libusb_init() failed.");
		return FALSE;
	}
	// 禁止写入操作
	object->Read = object->Write = unsupportRW;
	object->Init = object->Deinit = defaultInitDeinit;
	object->Reset = defaultReset;
	object->clamedIFNum = object->currConfVal = -1;
	return TRUE;
}

/*
 * 销毁USB对象
 * object: 要销毁的USB对象
 */
void DeinitUSBObject(USBObject *object){
	assert(object != NULL
			&& object->libusbContext != NULL);
	libusb_exit(object->libusbContext);
	object->libusbContext = NULL;
}

/**
 * 检查设备的序列号与指定序列号是否一致
 */
static BOOL usbStrDescriptorMatch(libusb_device_handle *devHandle, uint8_t descIndex, const char *snString) {
	int retcode;
	BOOL matched;
	// 描述字符串缓冲区
	char descString[256+1];	// XXX 较大的数组在栈中分配！

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

		// 检查该usb设备
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
		retCode = libusb_set_auto_detach_kernel_driver(devHandle, 1);
		if(LIBUSB_ERROR_NOT_SUPPORTED == retCode){
			log_warn("The current operating system does not support automatic detach kernel drive.");
		}
		// 获得当前默认的configuration
		retCode = libusb_get_configuration(devHandle, &usbObj->currConfVal);
		if (retCode < 0){
			log_warn("libusb_get_configuration():%s", libusb_error_name(retCode));
		}
		// 找到设备，初始化USB对象
		usbObj->devHandle = devHandle;
		usbObj->Pid = devDesc_tmp.idProduct;
		usbObj->Vid = devDesc_tmp.idVendor;
		usbObj->SerialNum = serial ? strdup(serial) : NULL;

		// 释放设备列表
		libusb_free_device_list(devs, 1);
		return TRUE;
	}
	return FALSE;
}

// 关闭USB
void USBClose(USBObject *usbObj) {
	assert(usbObj != NULL
			&& usbObj->devHandle != NULL);

	/* Close device */
	libusb_close(usbObj->devHandle);
	usbObj->devHandle = NULL;
	// 清除配置
	usbObj->clamedIFNum = -1;
	usbObj->currConfVal = -1;
	// 禁止写入
	usbObj->Read = usbObj->Write = unsupportRW;
}

/**
 * USB控制传输
 */
int USBControlTransfer(USBObject *usbObj, uint8_t requestType, uint8_t request, uint16_t wValue,
	uint16_t wIndex, uint8_t *data, uint16_t dataLength, unsigned int timeout)
{
	int retCode;
	assert(usbObj != NULL && usbObj->devHandle != NULL);

	retCode = libusb_control_transfer(usbObj->devHandle, requestType, request, wValue, wIndex, (unsigned char *)data, dataLength, timeout);
	if(retCode < 0){
		log_warn("libusb_control_transfer():%s", libusb_error_name(retCode));
		return 0;
	}
	return retCode;
}

static int bulkWrite(USBObject *usbObj, uint8_t *data, int dataLength, int timeout){
	return USBBulkTransfer(usbObj, usbObj->writeEP, data, dataLength, timeout);
}

static int bulkRead(USBObject *usbObj, uint8_t *data, int dataLength, int timeout){
	return USBBulkTransfer(usbObj, usbObj->readEP, data, dataLength, timeout);
}

/**
 * 读/写数据块
 */
int USBBulkTransfer(USBObject *usbObj, uint8_t endpoint, uint8_t *data, int dataLength, int timeout) {
	int transferred = 0, retCode;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	retCode = libusb_bulk_transfer(usbObj->devHandle, endpoint, (unsigned char *)data, dataLength, &transferred, timeout);
	if (retCode < 0){
		log_warn("libusb_bulk_transfer():%s", libusb_error_name(retCode));
	}
	return transferred;
}

static int interruptWrite(USBObject *usbObj, uint8_t *data, int dataLength, int timeout){
	return USBInterruptTransfer(usbObj, usbObj->writeEP, data, dataLength, timeout);
}

static int interruptRead(USBObject *usbObj, uint8_t *data, int dataLength, int timeout){
	return USBInterruptTransfer(usbObj, usbObj->readEP, data, dataLength, timeout);
}

/**
 * 中断传输
 */
int USBInterruptTransfer(USBObject *usbObj, uint8_t endpoint, uint8_t *data, int dataLength, int timeout) {
	int transferred = 0, retCode;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	retCode = libusb_interrupt_transfer(usbObj->devHandle, endpoint, (unsigned char *)data, dataLength, &transferred, timeout);
	if (retCode < 0){
		log_warn("libusb_interrupt_transfer():%s", libusb_error_name(retCode));
	}
	return transferred;
}

static int unsupportRW(USBObject *usbObj, uint8_t *data, int dataLength, int timeout){
	(void)usbObj; (void)data; (void)dataLength; (void)timeout;
	log_warn("An endpoint with an unsupported type has been operated or the endpoint has been shut down.");
	return -1;
}

static BOOL defaultInitDeinit(USBObject *usbObj){
	log_warn("Init deinit nothing.");
	return TRUE;
}

/**
 * 设置活跃配置
 * configurationValue: 配置的索引，第一个配置索引为0
 */
BOOL USBSetConfiguration(USBObject *usbObj, int configurationIndex) {
	struct libusb_config_descriptor *config = NULL;
	int retCode;
	libusb_device *dev = NULL;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	// 获得Device
	dev = libusb_get_device(usbObj->devHandle);

	// 获取当前配置
	// output location for the bConfigurationValue of the active configuration (only valid for return code 0)
	retCode = libusb_get_configuration(usbObj->devHandle, &usbObj->currConfVal);
	if (retCode < 0){
		log_warn("libusb_get_configuration():%s", libusb_error_name(retCode));
		return FALSE;
	}
	// 判断是否已经claim interface，如果claim interface要先release
	if(usbObj->clamedIFNum != -1){
		log_info("Release declared interface %d.", usbObj->clamedIFNum);
		retCode = libusb_release_interface(usbObj->devHandle, usbObj->clamedIFNum);
		if(retCode < 0){
			log_warn("libusb_release_interface():%s", libusb_error_name(retCode));
			return FALSE;
		}
		usbObj->clamedIFNum = -1;
	}

	retCode = libusb_get_config_descriptor(dev, configurationIndex, &config);
	if (retCode < 0){
		log_warn("libusb_get_config_descriptor():%s", libusb_error_name(retCode));
		return FALSE;
	}
	if (usbObj->currConfVal != config->bConfigurationValue){
		if((retCode = libusb_set_configuration(usbObj->devHandle, config->bConfigurationValue)) < 0){
			log_warn("libusb_set_configuration():%s", libusb_error_name(retCode));
			libusb_free_config_descriptor(config);
			return FALSE;
		}
		usbObj->currConfVal = config->bConfigurationValue;
	}else{
		log_info("Currently it is configuration indexed by:%d, bConfigurationValue is %x.", configurationIndex, usbObj->currConfVal);
	}
	// 释放
	libusb_free_config_descriptor(config);
	return TRUE;
}

/**
 * 声明interface
 * IFClass:接口的类别码
 * IFSubclass：接口子类别码
 * IFProtocol：接口协议码
 * transType：传输类型，最低两位表示传输类型：0为控制传输
 * 			1为等时传输，2为批量传输，3为中断传输
 */
BOOL USBClaimInterface(USBObject *usbObj, int IFClass, int IFSubclass, int IFProtocol, int transType)
{
	struct libusb_device *dev = NULL;
	struct libusb_config_descriptor *config;
	int retCode;

	assert(usbObj != NULL && usbObj->devHandle != NULL);

	if(usbObj->currConfVal == 0){
		log_error("The active configuration doesn't set.");
		return FALSE;
	}

	dev = libusb_get_device(usbObj->devHandle);
	retCode = libusb_get_config_descriptor_by_value(dev, usbObj->currConfVal, &config);
	if(retCode != 0){
		log_warn("libusb_get_config_descriptor():%s", libusb_error_name(retCode));
		return FALSE;
	}

	for (int i = 0; i < (int)config->bNumInterfaces; i++) {
		// 获得interface
		const struct libusb_interface *interface = &config->interface[i];
		const struct libusb_interface_descriptor *interfaceDesc = &interface->altsetting[0];
		// XXX altsetting的理解或许有问题
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
		if(usbObj->clamedIFNum == interfaceDesc->bInterfaceNumber){
			log_info("Currently it is interface %d.", usbObj->clamedIFNum);
			return TRUE;
		}
		// 判断是否已经声明了interface，如果声明了与当前不同的interface，则释放原来的
		if(usbObj->clamedIFNum != -1){
			log_info("Release declared interface %d.", usbObj->clamedIFNum);
			retCode = libusb_release_interface(usbObj->devHandle, usbObj->clamedIFNum);
			if(retCode < 0){
				log_warn("libusb_release_interface():%s", libusb_error_name(retCode));
				return FALSE;
			}
			usbObj->clamedIFNum = -1;
		}

		for (int k = 0; k < (int)interfaceDesc->bNumEndpoints; k++) {
			uint8_t epNum; // Endpoint Number
			const struct libusb_endpoint_descriptor *epDesc;

			epDesc = &interfaceDesc->endpoint[k];

			if (transType > 0 && (epDesc->bmAttributes & 0x3) != transType)
				continue;

			epNum = epDesc->bEndpointAddress;

			if (epNum & 0x80){
				usbObj->readEP = epNum;
				// 获得传输的包大小
				usbObj->readEPMaxPackSize = epDesc->wMaxPacketSize & 0x7ff;
				log_debug("usb end point 'in' 0x%02x, max packet size %d bytes.", epNum, usbObj->readEPMaxPackSize);
			}else{
				usbObj->writeEP = epNum;
				usbObj->writeEPMaxPackSize = epDesc->wMaxPacketSize & 0x7ff;
				log_debug("usb end point 'out' 0x%02x, max packet size %d bytes.", epNum, usbObj->writeEPMaxPackSize);
			}
			// TODO 获取最大数据包大小

			// XXX 这里没有考虑端点0
			if (usbObj->readEP && usbObj->writeEP) {
				// 写入回调
				switch(transType){
				case 3:	//中断传输
					usbObj->Write = interruptWrite;
					usbObj->Read = interruptRead;
					break;

				case 2:	// 批量传输
					usbObj->Write = bulkWrite;
					usbObj->Read = bulkRead;
					break;
				default:
					usbObj->Write = usbObj->Read = unsupportRW;
					log_info("Unsupported endpoint type.");
				}
				usbObj->clamedIFNum = interfaceDesc->bInterfaceNumber;
				log_debug("Claiming interface %d", (int)interfaceDesc->bInterfaceNumber);
				libusb_claim_interface(usbObj->devHandle, (int)interfaceDesc->bInterfaceNumber);
				libusb_free_config_descriptor(config);
				return TRUE;
			}

		} //for (int k = 0; k < (int)interfaceDesc->bNumEndpoints; k++)
	} //for (int i = 0; i < (int)config->bNumInterfaces; i++)
	libusb_free_config_descriptor(config);

	return FALSE;
}

static BOOL defaultReset(USBObject *usbObj){
	log_info("nothing reset.");
	return FALSE;
}

/**
 * 复位设备
 */
BOOL USBResetDevice(USBObject *usbObj){
	assert(usbObj != NULL && usbObj->devHandle != NULL);
	int retCode = libusb_reset_device(usbObj->devHandle);
	if(retCode == 0){
		return TRUE;
	}else{
		log_warn("libusb_reset_device():%s", libusb_error_name(retCode));
		return FALSE;
	}
}

