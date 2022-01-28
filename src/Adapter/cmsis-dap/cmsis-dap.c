/**
 * src/Adapter/cmsis-dap/cmsis-dap.c
 * Copyright (c) 2020 Virus.V <virusv@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Adapter/cmsis-dap/cmsis-dap.h"

#include <string.h>

#include "Component/ADI/ADIv5.h"
#include "Library/misc/list.h"
#include "Library/usb/usb.h"
#include "Library/log/log.h"

// DAP Transfer Request
#define CMDAP_TRANSFER_APnDP (1U << 0)
#define CMDAP_TRANSFER_RnW (1U << 1)
#define CMDAP_TRANSFER_A2 (1U << 2)
#define CMDAP_TRANSFER_A3 (1U << 3)

// CMSIS-DAP Command IDs
// V1.0
#define CMDAP_ID_DAP_Info 0x00U
#define CMDAP_ID_DAP_HostStatus 0x01U
#define CMDAP_ID_DAP_Connect 0x02U
#define CMDAP_ID_DAP_Disconnect 0x03U
#define CMDAP_ID_DAP_TransferConfigure 0x04U
#define CMDAP_ID_DAP_Transfer 0x05U
#define CMDAP_ID_DAP_TransferBlock 0x06U
#define CMDAP_ID_DAP_TransferAbort 0x07U
#define CMDAP_ID_DAP_WriteABORT 0x08U
#define CMDAP_ID_DAP_Delay 0x09U
#define CMDAP_ID_DAP_ResetTarget 0x0AU
#define CMDAP_ID_DAP_SWJ_Pins 0x10U
#define CMDAP_ID_DAP_SWJ_Clock 0x11U
#define CMDAP_ID_DAP_SWJ_Sequence 0x12U
#define CMDAP_ID_DAP_SWD_Configure 0x13U
#define CMDAP_ID_DAP_JTAG_Sequence 0x14U
#define CMDAP_ID_DAP_JTAG_Configure 0x15U
#define CMDAP_ID_DAP_JTAG_IDCODE 0x16U
// V1.1
#define CMDAP_ID_DAP_SWO_Transport 0x17U
#define CMDAP_ID_DAP_SWO_Mode 0x18U
#define CMDAP_ID_DAP_SWO_Baudrate 0x19U
#define CMDAP_ID_DAP_SWO_Control 0x1AU
#define CMDAP_ID_DAP_SWO_Status 0x1BU
#define CMDAP_ID_DAP_SWO_Data 0x1CU

#define CMDAP_ID_DAP_QueueCommands 0x7EU
#define CMDAP_ID_DAP_ExecuteCommands 0x7FU

// DAP Vendor Command IDs
#define CMDAP_ID_DAP_Vendor0 0x80U
#define CMDAP_ID_DAP_Vendor1 0x81U
#define CMDAP_ID_DAP_Vendor2 0x82U
#define CMDAP_ID_DAP_Vendor3 0x83U
#define CMDAP_ID_DAP_Vendor4 0x84U
#define CMDAP_ID_DAP_Vendor5 0x85U
#define CMDAP_ID_DAP_Vendor6 0x86U
#define CMDAP_ID_DAP_Vendor7 0x87U
#define CMDAP_ID_DAP_Vendor8 0x88U
#define CMDAP_ID_DAP_Vendor9 0x89U
#define CMDAP_ID_DAP_Vendor10 0x8AU
#define CMDAP_ID_DAP_Vendor11 0x8BU
#define CMDAP_ID_DAP_Vendor12 0x8CU
#define CMDAP_ID_DAP_Vendor13 0x8DU
#define CMDAP_ID_DAP_Vendor14 0x8EU
#define CMDAP_ID_DAP_Vendor15 0x8FU
#define CMDAP_ID_DAP_Vendor16 0x90U
#define CMDAP_ID_DAP_Vendor17 0x91U
#define CMDAP_ID_DAP_Vendor18 0x92U
#define CMDAP_ID_DAP_Vendor19 0x93U
#define CMDAP_ID_DAP_Vendor20 0x94U
#define CMDAP_ID_DAP_Vendor21 0x95U
#define CMDAP_ID_DAP_Vendor22 0x96U
#define CMDAP_ID_DAP_Vendor23 0x97U
#define CMDAP_ID_DAP_Vendor24 0x98U
#define CMDAP_ID_DAP_Vendor25 0x99U
#define CMDAP_ID_DAP_Vendor26 0x9AU
#define CMDAP_ID_DAP_Vendor27 0x9BU
#define CMDAP_ID_DAP_Vendor28 0x9CU
#define CMDAP_ID_DAP_Vendor29 0x9DU
#define CMDAP_ID_DAP_Vendor30 0x9EU
#define CMDAP_ID_DAP_Vendor31 0x9FU

#define CMDAP_ID_DAP_Invalid 0xFFU

// DAP Status Code
#define CMDAP_OK 0U
#define CMDAP_ERROR 0xFFU

// DAP ID
#define CMDAP_ID_VENDOR 1U
#define CMDAP_ID_PRODUCT 2U
#define CMDAP_ID_SER_NUM 3U
#define CMDAP_ID_FW_VER 4U
#define CMDAP_ID_DEVICE_VENDOR 5U
#define CMDAP_ID_DEVICE_NAME 6U
#define CMDAP_ID_CAPABILITIES 0xF0U
#define CMDAP_ID_SWO_BUFFER_SIZE 0xFDU // V1.1
#define CMDAP_ID_PACKET_COUNT 0xFEU
#define CMDAP_ID_PACKET_SIZE 0xFFU

// DAP Host Status
#define CMDAP_DEBUGGER_CONNECTED 0U
#define CMDAP_TARGET_RUNNING 1U

// DAP Port
#define CMDAP_PORT_AUTODETECT 0U // Autodetect Port
#define CMDAP_PORT_DISABLED 0U   // Port Disabled (I/O pins in High-Z)
#define CMDAP_PORT_SWD 1U        // SWD Port (SWCLK, SWDIO) + nRESET
#define CMDAP_PORT_JTAG 2U       // JTAG Port (TCK, TMS, TDI, TDO, nTRST) + nRESET

// DAP Transfer Response
#define CMDAP_TRANSFER_OK (1U << 0)
#define CMDAP_TRANSFER_WAIT (1U << 1)
#define CMDAP_TRANSFER_FAULT (1U << 2)
#define CMDAP_TRANSFER_ERROR (1U << 3)
#define CMDAP_TRANSFER_MISMATCH (1U << 4)

// DAP SWO Trace Mode	V1.1
#define DAP_SWO_OFF 0U
#define DAP_SWO_UART 1U
#define DAP_SWO_MANCHESTER 2U

// DAP SWO Trace Status	V1.1
#define CMDAP_SWO_CAPTURE_ACTIVE (1U << 0)
#define CMDAP_SWO_CAPTURE_PAUSED (1U << 1)
#define CMDAP_SWO_STREAM_ERROR (1U << 6)
#define CMDAP_SWO_BUFFER_OVERRUN (1U << 7)

// JTAG底层指令类型
enum JTAG_InstrType {
  JTAG_INS_STATUS_MOVE,   // 状态机改变状态
  JTAG_INS_EXCHANGE_DATA, // 交换TDI-TDO数据
  JTAG_INS_IDLE_WAIT,     // 进入IDLE等待几个时钟周期
};

// JTAG指令对象
struct JTAG_Command {
  enum JTAG_InstrType type;    // JTAG指令类型
  struct list_head list_entry; // JTAG指令链表对象
  // 指令结构共用体
  union {
    struct {
      enum JTAG_TAP_State toState;
    } statusMove;
    struct {
      uint8_t *data;         // 需要交换的数据地址
      unsigned int bitCount; // 交换的二进制位个数
    } exchangeData;
    struct {
      unsigned int clkCount; // 时钟个数
    } idleWait;
  } instr;
};

// DAP指令类型
enum DAP_InstrType {
  DAP_INS_RW_REG_SINGLE, // 单次读写nP寄存器
  DAP_INS_RW_REG_MULTI,  // 多次读写寄存器
};

// DAP指令对象
struct DAP_Command {
  enum DAP_InstrType type;     // DAP指令类型
  struct list_head list_entry; // DAP指令链表对象
  // 指令结构共用体
  union {
    struct {
      /**
       * Bit 0: APnDP: 0 = Debug Port (DP), 1 = Access Port (AP).
       * Bit 1: RnW: 0 = Write Register, 1 = Read Register.
       * Bit 2: A2 Register Address bit 2.
       * Bit 3: A3 Register Address bit 3.
       */
      uint8_t request;
      // 指令的数据
      union {
        uint32_t write; // 写单个寄存器
        uint32_t *read;
      } data;
    } singleReg; // 单次读写寄存器
    struct {
      /**
       * Bit 0: APnDP: 0 = Debug Port (DP), 1 = Access Port (AP).
       * Bit 1: RnW: 0 = Write Register, 1 = Read Register.
       * Bit 2: A2 Register Address bit 2.
       * Bit 3: A3 Register Address bit 3.
       */
      uint8_t request;
      // 指令的数据
      uint32_t *data;
      int count; // 读写次数
    } multiReg;  // 多次读写寄存器
  } instr;
};

/* CMSIS-DAP对象 */
struct cmsis_dap {
  uint32_t signature;       // 代表类型
  USB usbObj;               // USB连接对象
  struct adapter adaperAPI; // Adapter接口对象
  BOOL inited;              // 是否已经初始化
  BOOL connected;           // USB设备是否已连接

  struct jtagSkill jtagSkillAPI; // jtag能力集接口
  struct dapSkill dapSkillAPI;   // dap能力集接口
  int Version;                   // CMSIS-DAP 版本
  int MaxPcaketCount;            // 缓冲区最多容纳包的个数
  int PacketSize;                // 包最大长度
  uint8_t *respBuffer;           // 应答缓冲区
  uint32_t capablityFlag;        // 该仿真器支持的功能

  struct list_head JtagInsQueue; // JTAG指令队列，元素类型：struct JTAG_Command
  struct list_head DapInsQueue;  // DAP指令队列，元素类型struct DAP_Command
  unsigned int tapCount;         // TAP个数
  unsigned int tapIndex;         // 要操作的TAP在扫描链中的索引,
                                 // 在DAP Transfer相关函数中会用到
  // TODO 实现更高版本仿真器支持 SWO、
};

/*
Available transfer protocols to target:
    Info0 - Bit 0: 1 = SWD Serial Wire Debug communication is implemented (0 =
SWD Commands not implemented). Info0 - Bit 1: 1 = JTAG communication is
implemented (0 = JTAG Commands not implemented).

Serial Wire Trace (SWO) support:
    Info0 - Bit 2: 1 = SWO UART - UART Serial Wire Output is implemented (0 =
not implemented). Info0 - Bit 3: 1 = SWO Manchester - Manchester Serial Wire
Output is implemented (0 = not implemented).

Command extensions for transfer protocol:
    Info0 - Bit 4: 1 = Atomic Commands - Atomic Commands support is implemented
(0 = Atomic Commands not implemented).

Time synchronisation via Test Domain Timer:
    Info0 - Bit 5: 1 = Test Domain Timer - debug unit support for Test Domain
Timer is implemented (0 = not implemented).

SWO Streaming Trace support:
    Info0 - Bit 6: 1 = SWO Streaming Trace is implemented (0 = not implemented).
 */
// CMSIS功能标志位
#define CMDAP_CAP_SWD 0
#define CMDAP_CAP_JTAG 1
#define CMDAP_CAP_SWO_UART 2
#define CMDAP_CAP_SWO_MANCHESTER 3
#define CMDAP_CAP_ATOMIC 4
#define CMDAP_CAP_SWD_SEQUENCE 5
#define CMDAP_CAP_TEST_DOMAIN_TIMER 6
#define CMDAP_CAP_TRACE_DATA_MANAGE 7

/**
 * 在一串数据中获得第n个二进制位，n从0开始
 * lsb
 * data:数据存放的位置指针
 * n:第几位，最低位是第0位
 */
#define GET_Nth_BIT(data, n) ((*(CAST(uint8_t *, (data)) + ((n) >> 3)) >> ((n)&0x7)) & 0x1)
/**
 * 设置data的第n个二进制位
 * data:数据存放的位置
 * n：要修改哪一位，从0开始
 * val：要设置的位，0或者1，只使用最低位
 */
#define SET_Nth_BIT(data, n, val)                               \
  do {                                                          \
    uint8_t tmp_data = *(CAST(uint8_t *, (data)) + ((n) >> 3)); \
    tmp_data &= ~(1 << ((n)&0x7));                              \
    tmp_data |= ((val)&0x1) << ((n)&0x7);                       \
    *(CAST(uint8_t *, (data)) + ((n) >> 3)) = tmp_data;         \
  } while (0);

/**
 * 由多少个比特位构造出多少个字节，包括控制字
 * 比如bitCnt=64
 * 则返回9
 * 如果bitCnt=65
 * 则返回11。因为 发送65位需要两个控制字节了。
 */
#define BIT2BYTE(bitCnt)                       \
  ({                                           \
    int byteCnt, tmp = (bitCnt) >> 6;          \
    byteCnt = tmp + (tmp << 3);                \
    tmp = (bitCnt)&0x3f;                       \
    byteCnt += tmp ? ((tmp + 7) >> 3) + 1 : 0; \
  })

#define OFFSET_ADAPTER offsetof(struct cmsis_dap, adaperAPI)
#define OFFSET_JTAG_SKILL offsetof(struct cmsis_dap, jtagSkillAPI)
#define OFFSET_DAP_SKILL offsetof(struct cmsis_dap, dapSkillAPI)
#define CMDAP_OBJ_FORM_ADAPTER(x) get_cmsis_dap_obj((void *)(x), OFFSET_ADAPTER)
#define CMDAP_OBJ_FORM_JTAG_SKILL(x) get_cmsis_dap_obj((void *)(x), OFFSET_JTAG_SKILL)
#define CMDAP_OBJ_FORM_DAP_SKILL(x) get_cmsis_dap_obj((void *)(x), OFFSET_DAP_SKILL)

// 检查Adapter类型，并返回对应的结构
static struct cmsis_dap *get_cmsis_dap_obj(void *self, size_t offset) {
  assert(self != NULL);
  struct cmsis_dap * obj = (struct cmsis_dap *)((char *)self - offset);
  if (obj->signature != SIGNATURE_32('C', 'D', 'A', 'P')) {
    log_fatal("Adapter object is not CMSIS-DAP!");
    return NULL; // never reach here, to surpress warnings
  }
  return obj;
}

static int dapInit(struct cmsis_dap *cmdapObj);

/**
 * 从仿真器读数据放入cmdapObj->respBuffer中
 * transferred:成功读取的字节数,当该函数返回成功时该值才有效
 * 注意：该函数会读取固定大小 cmsis_dapObj->PacketSize
 */
static int dapRead(struct cmsis_dap *cmdapObj, int *transferred) {
  assert(cmdapObj != NULL);
  assert(cmdapObj->PacketSize != 0);
  // 永久阻塞,所以函数返回时transferred肯定等于cmdapObj->PacketSize
  if (cmdapObj->usbObj->Read(cmdapObj->usbObj, cmdapObj->respBuffer, cmdapObj->PacketSize, 0, transferred) != USB_SUCCESS) {
    log_error("Read from CMSIS-USB failed.");
    return ADPT_ERR_TRANSPORT_ERROR;
  }
  //	log_debug("----------------------Read---------------------");
  //	misc_PrintBulk(cmdapObj->respBuffer, *transferred, 16);
  //	log_debug("----------------------Read---------------------");
  log_trace("Read %d byte(s) from CMSIS-DAP.", *transferred);
  return ADPT_SUCCESS;
}

/**
 * 从仿真器写数据
 * data:数据缓冲区指针
 * len:要发送的数据长度
 * transferred:成功读取的字节数,当该函数返回成功时该值才有效
 */
static int dapWrite(struct cmsis_dap *cmdapObj, uint8_t *data, int len, int *transferred) {
  assert(cmdapObj != NULL);
  assert(cmdapObj->PacketSize != 0);
  if (cmdapObj->usbObj->Write(cmdapObj->usbObj, data, len, 0, transferred) != USB_SUCCESS) {
    log_error("Write to CMSIS-USB failed.");
    return ADPT_ERR_TRANSPORT_ERROR;
  }
  //	log_debug("----------------------Write---------------------");
  //	misc_PrintBulk(data, *transferred, 16);
  //	log_debug("----------------------Write---------------------");
  log_trace("Write %d byte(s) to CMSIS-DAP.", *transferred);
  return ADPT_SUCCESS;
}

/**
 * 简单封装的数据交换的宏,该宏中的代码如果出错会造成函数返回
 */
#define DAP_EXCHANGE_DATA(cmObj, data, len)                  \
  do {                                                       \
    int _tmp, _resu;                                         \
    _resu = dapWrite((cmObj), (data), (len), &_tmp);         \
    if (_resu != ADPT_SUCCESS) {                             \
      log_error("Send command/data to CMSIS-DAP failed.");   \
      return ADPT_ERR_TRANSPORT_ERROR;                       \
    }                                                        \
    _resu = dapRead((cmObj), &_tmp);                         \
    if (_resu != ADPT_SUCCESS) {                             \
      log_error("Read command/data from CMSIS-DAP failed."); \
      return ADPT_ERR_TRANSPORT_ERROR;                       \
    }                                                        \
  } while (0);

/**
 * 搜索并连接CMSIS-DAP仿真器
 */
int ConnectCmsisDap(Adapter self, const uint16_t *vids, const uint16_t *pids,
                    const char *serialNum) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);

  int idx = 0;
  //如果当前没有连接,则连接CMSIS-DAP设备
  if (cmdapObj->connected != TRUE) {
    for (; vids[idx] && pids[idx]; idx++) {
      log_debug("Try connecting vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
      if (USB_Open(cmdapObj->usbObj, vids[idx], pids[idx], serialNum) == USB_SUCCESS) {
        log_info("Successfully connected vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
        // 复位设备
        USB_Reset(cmdapObj->usbObj);
        // 标志已连接
        cmdapObj->connected = TRUE;
        // 选择配置和声明接口
        if (USB_SetConfiguration(cmdapObj->usbObj, 0) != USB_SUCCESS) {
          log_warn("USB.SetConfiguration failed.");
          return ADPT_ERR_TRANSPORT_ERROR;
        }
        if (USB_ClaimInterface(cmdapObj->usbObj, 3, 0, 0, 3) != USB_SUCCESS) {
          log_warn("USB.SetConfiguration failed.");
          return ADPT_ERR_TRANSPORT_ERROR;
        }
        goto _TOINIT; // 跳转到初始化部分
      }
    }
    log_warn("No suitable device found.");
    return ADPT_ERR_NO_DEVICE;
  }
_TOINIT:
  // 执行初始化
  if (cmdapObj->inited != TRUE) {
    if (dapInit(cmdapObj) != ADPT_SUCCESS) {
      log_error("Cannot init CMSIS-DAP.");
      return ADPT_FAILED;
    }
    cmdapObj->inited = TRUE;
  }

  log_info("CMSIS-DAP has been initialized.");
  return ADPT_SUCCESS;
}

/**
 * 发送断开指令
 */
int DisconnectCmsisDap(Adapter self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  uint8_t command[1] = {CMDAP_ID_DAP_Disconnect};
  DAP_EXCHANGE_DATA(cmdapObj, command, 1);
  // 检查返回值
  if (cmdapObj->respBuffer[1] != CMDAP_OK) {
    log_warn("Disconnect execution failed.");
    return ADPT_FAILED;
  }
  return ADPT_SUCCESS;
}

/**
 * JTAG协议转SWD
 * 转换后自动增加一个lineReset操作
 */
static int swjJtag2Swd(struct cmsis_dap *cmdapObj) {
  assert(cmdapObj != NULL);
  // SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
  // 12 76 FF FF FF FF FF FF 7B 9E FF FF FF FF FF FF 0F
  uint8_t switchSque[] = {
      CMDAP_ID_DAP_SWJ_Sequence,
      136, // 144
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff, // 56 bit
      0x9e,
      0xe7, // 16 bit
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff, // 56 bit
      0x00, // 8 bit
  };        // DAP_SWJ_Sequence Command
  DAP_EXCHANGE_DATA(cmdapObj, switchSque, sizeof(switchSque));
  return ADPT_SUCCESS;
}

/**
 * SWD协议转JTAG
 */
static int swjSwd2Jtag(struct cmsis_dap *cmdapObj) {
  assert(cmdapObj != NULL);
  // SWJ发送 0xE79E，老版本的ARM发送0xB76D强制切换
  uint8_t switchSque[] = {
      CMDAP_ID_DAP_SWJ_Sequence,
      84,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff, // 56 bit
      0x3c,
      0xe7, // 16 bit
      0xff,
      0x00, // 8 bit
  };        // DAP_SWJ_Sequence Command
  DAP_EXCHANGE_DATA(cmdapObj, switchSque, sizeof(switchSque));
  return ADPT_SUCCESS;
}

// 初始化CMSIS-DAP设备
static int dapInit(struct cmsis_dap *cmdapObj) {
  assert(cmdapObj != NULL);
  char capablity;
  int transferred; // usb传输字节数

  uint8_t command[2] = {CMDAP_ID_DAP_Info, CMDAP_ID_PACKET_SIZE};
  // 这块空间在cmsis_dap对象销毁时释放
  if ((cmdapObj->respBuffer = calloc(cmdapObj->usbObj->readMaxPackSize, sizeof(uint8_t))) == NULL) {
    log_warn("Alloc response buffer failed.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  // 获得DAP_Info 判断
  log_info("Init CMSIS-DAP.");
  // 先以endpoint最大包长读取packet大小，然后读取剩下的
  cmdapObj->usbObj->Write(cmdapObj->usbObj, command, 2, 0, &transferred);
  cmdapObj->usbObj->Read(cmdapObj->usbObj, cmdapObj->respBuffer, cmdapObj->usbObj->readMaxPackSize, 0, &transferred);

  // 判断返回值是否是一个16位的数字
  if (cmdapObj->respBuffer[0] != 0 || cmdapObj->respBuffer[1] != 2) {
    log_warn("Get packet size failed.");
    return ADPT_ERR_PROTOCOL_ERROR;
  }
  // 取得包长度
  cmdapObj->PacketSize = *CAST(uint16_t *, cmdapObj->respBuffer + 2);
  if (cmdapObj->PacketSize == 0) {
    log_warn("Packet Size is Zero!!!");
    return ADPT_ERR_PROTOCOL_ERROR;
  }

  // 重新分配缓冲区大小
  uint8_t *resp_new;
  if ((resp_new = realloc(cmdapObj->respBuffer, cmdapObj->PacketSize * sizeof(uint8_t))) == NULL) {
    log_warn("realloc response buffer failed.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  cmdapObj->respBuffer = resp_new;

  log_info("CMSIS-DAP the maximum Packet Size is %d.", cmdapObj->PacketSize);
  // 读取剩下的内容
  if (cmdapObj->PacketSize > cmdapObj->usbObj->readMaxPackSize) {
    int rest_len = cmdapObj->PacketSize - cmdapObj->usbObj->readMaxPackSize;
    log_debug("Enlarge response buffer %d bytes.", rest_len);
    cmdapObj->usbObj->Read(cmdapObj->usbObj, cmdapObj->respBuffer + cmdapObj->usbObj->readMaxPackSize, rest_len, 0, &transferred);
  }

  // 获得CMSIS-DAP固件版本
  command[1] = CMDAP_ID_FW_VER;
  DAP_EXCHANGE_DATA(cmdapObj, command, 2);
  cmdapObj->Version = (int)(atof((const char *)(cmdapObj->respBuffer + 2)) * 100); // XXX 改成了整数
  log_info("CMSIS-DAP FW Version is %s.", cmdapObj->respBuffer + 2);

  // 获得CMSIS-DAP的最大包长度和最大包个数
  command[1] = CMDAP_ID_PACKET_COUNT;
  DAP_EXCHANGE_DATA(cmdapObj, command, 2);
  cmdapObj->MaxPcaketCount = *CAST(uint8_t *, cmdapObj->respBuffer + 2);
  log_info("CMSIS-DAP the maximum Packet Count is %d.", cmdapObj->MaxPcaketCount);

  // Capabilities. The information BYTE contains bits that indicate which
  // communication methods are provided to the Device.
  command[1] = CMDAP_ID_CAPABILITIES;
  DAP_EXCHANGE_DATA(cmdapObj, command, 2);

  cmdapObj->capablityFlag = 0; // 先把capablityFlag字段清零
  switch (*CAST(uint8_t *, cmdapObj->respBuffer + 1)) {
  case 4:
    cmdapObj->capablityFlag |= *CAST(uint8_t *, cmdapObj->respBuffer + 5) << 24; // INFO3
                                                                                 /* no break */
  case 3:
    cmdapObj->capablityFlag |= *CAST(uint8_t *, cmdapObj->respBuffer + 4) << 16; // INFO2
                                                                                 /* no break */
  case 2:
    cmdapObj->capablityFlag |= *CAST(uint8_t *, cmdapObj->respBuffer + 3) << 8; // INFO1
                                                                                /* no break */
  case 1:
    cmdapObj->capablityFlag |= *CAST(uint8_t *, cmdapObj->respBuffer + 2); // INFO0
    break;
  default:
    log_warn("Capablity Data has unknow length: %d.", *CAST(uint8_t *, cmdapObj->respBuffer + 1));
  }

  log_info("CMSIS-DAP Capabilities 0x%X.", cmdapObj->capablityFlag);

  // 发送Connect命令,自动选择模式
  command[0] = CMDAP_ID_DAP_Connect;
  command[1] = CMDAP_PORT_AUTODETECT;
  DAP_EXCHANGE_DATA(cmdapObj, command, 2);
  uint8_t mode = *CAST(uint8_t *, cmdapObj->respBuffer + 1);
  switch (mode) {
  default:
  case 0:
    log_warn(
        "Cant auto select transfer mode! please use "
        "Adapter.SetTransferMode() service to select mode manually.");
    INTERFACE_CONST_INIT(enum transferMode, cmdapObj->adaperAPI.currTransMode, ADPT_MODE_MAX);
    break;
  case CMDAP_PORT_SWD:
    log_info("Auto select SWD transfer mode.");
    INTERFACE_CONST_INIT(enum transferMode, cmdapObj->adaperAPI.currTransMode, ADPT_MODE_SWD);
    swjJtag2Swd(cmdapObj);
    break;
  case CMDAP_PORT_JTAG:
    log_info("Auto select JTAG transfer mode.");
    INTERFACE_CONST_INIT(enum transferMode, cmdapObj->adaperAPI.currTransMode, ADPT_MODE_JTAG);
    swjSwd2Jtag(cmdapObj);
    break;
  }
  return ADPT_SUCCESS;
}

/**
 * line reset
 */
static int CmdapSwdLineReset(Adapter self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  if (self->currTransMode != ADPT_MODE_SWD) {
    log_error("This operate only available SWD transfer mode.");
    return ADPT_FAILED;
  }
  uint8_t resetSque[] = {
      CMDAP_ID_DAP_SWJ_Sequence,
      55,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0xff,
      0x03, // 51 bit，后面跟一个0
  };        // DAP_SWJ_Sequence Command
  DAP_EXCHANGE_DATA(cmdapObj, resetSque, sizeof(resetSque));
  return ADPT_SUCCESS;
}

// 选择和切换传输协议
static int dapSetTransMode(Adapter self, enum transferMode mode) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  int idx;
  uint8_t command[2] = {CMDAP_ID_DAP_Connect};

  // 判断当前模式是否相同
  if (mode == self->currTransMode) {
    log_info("Already the specified mode.");
    return ADPT_SUCCESS;
  }

  // 先发送断开命令
  if (DisconnectCmsisDap(self) != ADPT_SUCCESS) {
    log_warn("Failed to disconnect.");
    return ADPT_FAILED;
  }

  // 切换协议
  switch (mode) {
  case ADPT_MODE_SWD:
    // 检查是否支持SWD模式
    if ((cmdapObj->capablityFlag & (0x1 << CMDAP_CAP_SWD)) == 0) {
      log_error("This device not support SWD mode.");
      return ADPT_ERR_UNSUPPORT;
    }
    // 切换到SWD模式
    command[1] = CMDAP_PORT_SWD;
    DAP_EXCHANGE_DATA(cmdapObj, command, 2);
    if (*CAST(uint8_t *, cmdapObj->respBuffer + 1) != CMDAP_PORT_SWD) {
      log_warn("Switching SWD mode failed.");
      return ADPT_FAILED;
    } else {
      // 发送切换swd序列
      swjJtag2Swd(cmdapObj);
      log_info("Switch to SWD mode.");
      // 更新当前模式
      INTERFACE_CONST_INIT(enum transferMode, cmdapObj->adaperAPI.currTransMode, ADPT_MODE_SWD);
      return ADPT_SUCCESS;
    }
  case ADPT_MODE_JTAG:
    // 检查是否支持JTAG模式
    if ((cmdapObj->capablityFlag & (0x1 << CMDAP_CAP_JTAG)) == 0) {
      log_error("This device not support JTAG mode.");
      return ADPT_ERR_UNSUPPORT;
    }
    // 切换到JTAG模式
    command[1] = CMDAP_PORT_JTAG;
    DAP_EXCHANGE_DATA(cmdapObj, command, 2);
    if (*CAST(uint8_t *, cmdapObj->respBuffer + 1) != CMDAP_PORT_JTAG) {
      log_warn("Switching JTAG mode failed.");
      return ADPT_FAILED;
    } else {
      // 发送切换JTAG序列
      swjSwd2Jtag(cmdapObj);
      log_info("Switch to JTAG mode.");
      // 更新当前模式
      INTERFACE_CONST_INIT(enum transferMode, cmdapObj->adaperAPI.currTransMode, ADPT_MODE_JTAG);
      // 更新当前TAP状态机
      INTERFACE_CONST_INIT(enum JTAG_TAP_State, cmdapObj->jtagSkillAPI.currState, JTAG_TAP_RESET);
      return ADPT_SUCCESS;
    }
  default:
    log_error("Unsupports specified mode.");
    return ADPT_ERR_UNSUPPORT;
  }
}

/**
 * 设置传输参数
 * 在调用DapTransfer和DapTransferBlock之前要先调用该函数
 * idleCycle：每一次传输后面附加的空闲时钟周期数
 * waitRetry：如果收到WAIT响应，重试的次数
 * matchRetry：如果在匹配模式下发现值不匹配，重试的次数
 * SWD、JTAG模式下均有效
 */
int CmdapTransferConfigure(Adapter self, uint8_t idleCycle, uint16_t waitRetry,
                           uint16_t matchRetry) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);

  uint8_t DAPTransfer[6] = {CMDAP_ID_DAP_TransferConfigure};
  DAPTransfer[1] = idleCycle; // Number of extra idle cycles after each transfer.
  DAPTransfer[2] = BYTE_IDX(waitRetry, 0);
  DAPTransfer[3] = BYTE_IDX(waitRetry, 1);
  DAPTransfer[4] = BYTE_IDX(matchRetry, 0);
  DAPTransfer[5] = BYTE_IDX(matchRetry, 1);

  DAP_EXCHANGE_DATA(cmdapObj, DAPTransfer, sizeof(DAPTransfer));
  // 判断是否成功
  if (cmdapObj->respBuffer[1] != CMDAP_OK) {
    log_warn("Transfer config execution failed.");
    return ADPT_FAILED;
  }
  // XXX 是否需要记录上面这些数据?
  return ADPT_SUCCESS;
}

/**
 * 必须实现指令之 AINS_SET_STATUS
 * DAP_HostStatus 设置仿真器状态灯
 * 参数：status 状态
 */
static int dapHostStatus(Adapter self, enum adapterStatus status) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);

  uint8_t DAP_HostStatusPack[] = {CMDAP_ID_DAP_HostStatus, 0, 0};
  switch (status) {
  case ADPT_STATUS_CONNECTED:
    DAP_HostStatusPack[1] = 0;
    DAP_HostStatusPack[2] = 1;
    break;
  case ADPT_STATUS_DISCONNECT:
    DAP_HostStatusPack[1] = 0;
    DAP_HostStatusPack[2] = 0;
    break;
  case ADPT_STATUS_RUNING:
    DAP_HostStatusPack[1] = 1;
    DAP_HostStatusPack[2] = 1;
    break;
  case ADPT_STATUS_IDLE:
    DAP_HostStatusPack[1] = 1;
    DAP_HostStatusPack[2] = 0;
    break;
  default:
    return ADPT_ERR_UNSUPPORT;
  }
  DAP_EXCHANGE_DATA(cmdapObj, DAP_HostStatusPack, 3);
  INTERFACE_CONST_INIT(enum adapterStatus, self->currStatus, status);
  return ADPT_SUCCESS;
}

// 格式化时钟频率
// 返回：除去单位之后的数字部分
// 参数：freq 待格式化的始终频率数字
// unit：单位部分字符串
static double formatFreq(double freq, const char **unit) {
  static const char *unit_str[] = {"Hz", "KHz", "MHz", "GHz", "THz", "PHz", "ZHz"}; // 7
  int unitPos = 0;
  while (freq > 1000.0) {
    freq /= 1000.0;
    unitPos++;
  }
  if (unitPos > 6) {
    *unit = "Infinite";
    return 0;
  }
  *unit = unit_str[unitPos];
  return freq;
}

/**
 * 必须实现指令之 AINS_SET_CLOCK
 * 设置SWJ的最大频率
 * 参数 clockHz 时钟频率，单位是 Hz
 */
static int dapSwjClock(Adapter self, unsigned int freq) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  uint8_t clockHzPack[5] = {CMDAP_ID_DAP_SWJ_Clock};
  uint32_t clockHz = CAST(uint32_t, freq);

  clockHzPack[1] = BYTE_IDX(clockHz, 0);
  clockHzPack[2] = BYTE_IDX(clockHz, 1);
  clockHzPack[3] = BYTE_IDX(clockHz, 2);
  clockHzPack[4] = BYTE_IDX(clockHz, 3);

  DAP_EXCHANGE_DATA(cmdapObj, clockHzPack, sizeof(clockHzPack));
  // 判断是否成功
  if (cmdapObj->respBuffer[1] != CMDAP_OK) {
    log_warn("SWJ Clock execution failed.");
    return ADPT_FAILED;
  }

  INTERFACE_CONST_INIT(unsigned int, self->currFrequency, freq);

  const char *unit;
  double freq_tmp = formatFreq(clockHz, &unit);
  log_info("CMSIS-DAP Clock Frequency: %.2lf%s.", freq_tmp, unit);
  return ADPT_SUCCESS;
}

/**
 * 执行JTAG时序
 * sequenceCount:需要产生多少组时序
 * data ：时序表示数据
 * response：TDO返回数据
 * 该函数会造成TAP状态机状态与CMSIS-DAP类记录的不一样
 */
static int cmdapJtagSequence(struct cmsis_dap *cmdapObj, int sequenceCount, uint8_t *data, uint8_t *response) {
  assert(cmdapObj != NULL);
  assert(cmdapObj->PacketSize != 0 && cmdapObj->MaxPcaketCount != 0);
  // 判断当前是否是JTAG模式
  if (cmdapObj->adaperAPI.currTransMode != ADPT_MODE_JTAG) {
    log_error("Current transfer mode is not JTAG.");
    return ADPT_FAILED;
  }

  //分配所有缓冲区，包括max packet count和packet buff
  uint8_t *buff = calloc(sizeof(int) * cmdapObj->MaxPcaketCount + cmdapObj->PacketSize, sizeof(uint8_t));
  if (buff == NULL) {
    log_error("Unable to allocate send packet buffer, the heap may be full.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  // 记录每次分包需要接收的result
  int *resultLength = CAST(int *, buff);
  // 发送包缓冲区
  uint8_t *sendPackBuff = buff + sizeof(int) * cmdapObj->MaxPcaketCount;
  int inputIdx = 0, outputIdx = 0,
      seqIdx = 0;                               // data数据索引，response数据索引，sequence索引
  int packetStartIdx = 0;                       // 当前的data的偏移
  int sendPackCnt = 0;                          // 当前发包计数
  int readPayloadLen, sendPayloadLen;           // 要发送和接收的载荷总字节，不包括头部
  int transferred;                              // USB传输字节数
  uint8_t seqCount;                             // 统计当前有多少个seq
  sendPackBuff[0] = CMDAP_ID_DAP_JTAG_Sequence; // 指令头部 0

MAKE_PACKT:
  seqCount = 0;
  readPayloadLen = 0;
  sendPayloadLen = 0;
  packetStartIdx = inputIdx;
  // 计算空间长度
  for (; seqIdx < sequenceCount; seqIdx++) {
    // GNU编译器下面可以直接这样用：uint8_t tckCount = data[idx] & 0x3f ? : 64;
    uint8_t tckCount = data[inputIdx] & 0x3f;
    tckCount = tckCount ? tckCount : 64;
    uint8_t tdiByte = (tckCount + 7) >> 3; // 将TCK个数圆整到字节，表示后面跟几个byte的tdi数据
    // log_debug("Offset:%d, seqIdx:%d, Seq:0x%02x, tck:%d, tdiByte:%d,
    // TMS:%d.", inputIdx, seqIdx, data[inputIdx], tckCount, tdiByte,
    // !!(data[inputIdx] & 0x40));
    // 如果当前数据长度加上tdiByte之后大于包长度，+3的意思是两个指令头部和SeqInfo字节
    if (sendPayloadLen + tdiByte + 3 > cmdapObj->PacketSize) {
      break;
    } else
      sendPayloadLen += tdiByte + 1; // FIX:+1是要计算头部
    //如果TDO Capture标志置位，则从TDO接收tdiByte字节的数据
    if ((data[inputIdx] & 0x80)) {
      // readByteCount
      // 一定会比sendByteCount少，所以上面检查过sendByteCount不超过最大包长度，readByteCount必不会超过
      readPayloadLen += tdiByte;
    }
    inputIdx += tdiByte + 1; // 跳到下一个sequence info
    seqCount++;
    if (seqCount == 255) { // 判断是否到达最大序列数目
      // FIXBUG: 达到255后没有增加seqIdx，而inputIdx增加了，导致内存访问越界
      seqIdx++;
      break;
    }
  }
  /**
   * 到达这儿，有三种情况：
   * 1、seqIdx == sequenceCount
   * 2、发送的总数据马上要大于包长度
   * 3、seqCount == 255
   */
  sendPackBuff[1] = seqCount; // sequence count
  memcpy(sendPackBuff + 2, data + packetStartIdx, sendPayloadLen);

  // 发送包
  dapWrite(cmdapObj, sendPackBuff, sendPayloadLen + 2, &transferred);
  log_trace("Write %d byte.", transferred);
  // 本次包的响应包包含多少个数据
  resultLength[sendPackCnt++] = readPayloadLen;

  /**
   * 如果没发完，而且没有达到最大包数量，则再构建一个包发送过去
   */
  if (seqIdx < (sequenceCount - 1) && sendPackCnt < cmdapObj->MaxPcaketCount)
    goto MAKE_PACKT;

  BOOL result = TRUE;
  // 读数据
  for (int readPackCnt = 0; readPackCnt < sendPackCnt; readPackCnt++) {
    dapRead(cmdapObj, &transferred);
    log_trace("Read %d byte.", transferred);
    // misc_PrintBulk(cmdapObj->respBuffer, transferred, 16);
    if (result == TRUE && cmdapObj->respBuffer[1] == CMDAP_OK) {
      // 拷贝数据
      if (response) {
        memcpy(response + outputIdx, cmdapObj->respBuffer + 2, resultLength[readPackCnt]);
        outputIdx += resultLength[readPackCnt];
      }
    } else {
      result = FALSE;
    }
  }
  // 中间有错误发生
  if (result == FALSE) {
    free(buff);
    log_error("An error occurred during the transfer.");
    return ADPT_FAILED;
  }
  // 判断是否处理完，如果没有则跳回去重新处理
  if (seqIdx < (sequenceCount - 1)) {
    // 将已发送包清零
    sendPackCnt = 0;
    goto MAKE_PACKT;
  }
  // log_debug("Write Back Len:%d.", outputIdx);
  free(buff);
  return ADPT_SUCCESS;
}

/**
 * pinSelect：选择哪一个pin
 * pinDataOut：引脚将要输出的信号
 * pinDataIn：pinDataOut的信号稳定后，读取全部的引脚数据
 * pinWait：死区时间
 * 0 = no wait
 * 1 .. 3000000 = time in µs (max 3s)
 * 返回值 TRUE；
 */
static int dapSwjPins(JtagSkill self, uint8_t pinMask, uint8_t pinDataOut, uint8_t *pinDataIn,
                      unsigned int pinWait) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_JTAG_SKILL(self);
  uint8_t DAP_PinPack[7] = {CMDAP_ID_DAP_SWJ_Pins};
  uint32_t wait_us = CAST(uint32_t, pinWait);
  // 构造包数据
  DAP_PinPack[1] = pinDataOut;
  DAP_PinPack[2] = pinMask;
  DAP_PinPack[3] = BYTE_IDX(wait_us, 0);
  DAP_PinPack[4] = BYTE_IDX(wait_us, 1);
  DAP_PinPack[5] = BYTE_IDX(wait_us, 2);
  DAP_PinPack[6] = BYTE_IDX(wait_us, 3);
  DAP_EXCHANGE_DATA(cmdapObj, DAP_PinPack, sizeof(DAP_PinPack));
  *pinDataIn = *(cmdapObj->respBuffer + 1);
  return ADPT_SUCCESS;
}

/**
 * 设置JTAG信息
 * count：扫描链中TAP的个数，不超过8个
 * irData：每个TAP的IR寄存器长度
 */
int CmdapJtagConfig(Adapter self, uint8_t count, uint8_t *irData) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);

  if (count > 8) {
    log_warn("TAP Too lot.");
    return ADPT_FAILED;
  }
  uint8_t *DAP_JTAG_ConfigPack = malloc((2 + count) * sizeof(uint8_t));
  if (!DAP_JTAG_ConfigPack) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  // 构造包数据
  DAP_JTAG_ConfigPack[0] = CMDAP_ID_DAP_JTAG_Configure;
  DAP_JTAG_ConfigPack[1] = count;
  memcpy(DAP_JTAG_ConfigPack + 2, irData, count);

  DAP_EXCHANGE_DATA(cmdapObj, DAP_JTAG_ConfigPack, 2 + count);
  free(DAP_JTAG_ConfigPack);
  if (*(cmdapObj->respBuffer + 1) != CMDAP_OK) {
    return ADPT_FAILED;
  }
  // 记录当前TAP个数
  cmdapObj->tapCount = count;
  return ADPT_SUCCESS;
}

struct dap_pack_info {
  uint8_t seqCnt; // 数据包中时序个数
  int dataLen;    // 数据包的读取的数据长度
};
/**
 * SWD和JTAG模式下均有效
 * 具体手册参考CMSIS-DAP DAP_Transfer这一小节
 * sequenceCnt:要发送的Sequence个数
 * okSeqCnt：执行成功的Sequence个数
 * XXX：由于DAP指令可能会出错，出错之后要立即停止执行后续指令，所以就
 * 不使用批量功能 ,此函数内默认cmsis_dapObj->MaxPcaketCount = 1
 */
static int cmdapTransfer(struct cmsis_dap *cmdapObj, uint8_t index, int sequenceCnt, uint8_t *data,
                         uint8_t *response, int *okSeqCnt) {
  assert(cmdapObj != NULL && okSeqCnt != NULL);
  assert(cmdapObj->PacketSize != 0);
  // 先清零
  *okSeqCnt = 0;
  /**
   * 分配所有缓冲区，包括max packet count和packet buff
   */
  // uint8_t *buff = calloc(sizeof(struct dap_pack_info) *
  // cmsis_dapObj->MaxPcaketCount + cmsis_dapObj->PacketSize, sizeof(uint8_t));
  uint8_t *buff = calloc(sizeof(struct dap_pack_info) * 1 + cmdapObj->PacketSize, sizeof(uint8_t));
  if (buff == NULL) {
    log_warn("Unable to allocate send packet buffer, the heap may be full.");
    return FALSE;
  }
  // 记录每次分包需要接收的result
  struct dap_pack_info *packetInfo = CAST(struct dap_pack_info *, buff);
  // 发送包缓冲区
  // uint8_t *sendPackBuff = buff + sizeof(struct dap_pack_info) *
  // cmsis_dapObj->MaxPcaketCount;
  uint8_t *sendPackBuff = buff + sizeof(struct dap_pack_info) * 1;
  int readCount = 0, writeCount = 0, seqIdx = 0;
  // 指向下一个sequence控制字节的索引，数据包的开始索引，发送数据包的个数
  int idx = 0, outIdx = 0, packetStartIdx, sendPackCnt = 0;
  int transferred;
  uint8_t thisPackSeqCnt; //本次数据包中Sequence个数

  // ===============构造本次数据包==================
MAKE_PACKT:
  // log_debug("make packet.");
  thisPackSeqCnt = 0;
  readCount = 0;
  writeCount = 0;
  packetStartIdx = idx;
  // 统计一些信息
  for (; seqIdx < sequenceCnt; seqIdx++) {
    data[idx] &= 0xf; // 只保留[3:0]位
    // 判断是否是读寄存器
    if ((data[idx] & CMDAP_TRANSFER_RnW) == CMDAP_TRANSFER_RnW) {
      // 判断是否超出最大包长度
      if ((3 + readCount + 4) > cmdapObj->PacketSize || (3 + writeCount + 1) > cmdapObj->PacketSize) {
        break;
      }
      idx += 1;
      readCount += 4;
      writeCount++;
    } else { // 写寄存器
      if ((3 + writeCount + 5) > cmdapObj->PacketSize) {
        break;
      }
      idx += 5;
      writeCount += 5;
    }
    thisPackSeqCnt++; // 本数据包sequence个数自增
    // 判断数据包个数是否达到最大
    if (thisPackSeqCnt == 0xFFu) {
      seqIdx++;
      break;
    }
  }
  // 构造数据包头部
  sendPackBuff[0] = CMDAP_ID_DAP_Transfer;
  // DAP index, JTAG ScanChain 中的位置，在SWD模式下忽略该参数
  sendPackBuff[1] = index;
  // 传输多少个request
  sendPackBuff[2] = thisPackSeqCnt;

  // 将数据拷贝到包中
  memcpy(sendPackBuff + 3, data + packetStartIdx, writeCount);
  // 交换数据
  dapWrite(cmdapObj, sendPackBuff, 3 + writeCount, &transferred);
  log_trace("Write %d byte.", transferred);
  // 本次包的响应包包含多少个数据
  packetInfo[sendPackCnt].dataLen = readCount;
  packetInfo[sendPackCnt].seqCnt = thisPackSeqCnt;
  sendPackCnt++;

  /**
   * 如果没发完，而且没有达到最大包数量，则再构建一个包发送过去
   */
  // if(seqIdx < (sequenceCnt-1) && sendPackCnt < cmsis_dapObj->MaxPcaketCount)
  // goto MAKE_PACKT;
  if (seqIdx < (sequenceCnt - 1) && sendPackCnt < 1)
    goto MAKE_PACKT;

  BOOL result = TRUE;
  // NOTIC 这个循环也就执行一次
  for (int readPackCnt = 0; readPackCnt < sendPackCnt; readPackCnt++) {
    dapRead(cmdapObj, &transferred);
    log_trace("Read %d byte.", transferred);
    // 本次执行的Sequence是否等于应该执行的个数
    if (result == TRUE) {
      *okSeqCnt += cmdapObj->respBuffer[1];
      if (cmdapObj->respBuffer[1] != packetInfo[readPackCnt].seqCnt) {
        log_warn("Last Response: %d.", cmdapObj->respBuffer[2]);
        result = FALSE;
      }
      // 拷贝数据
      if (response) {
        memcpy(response + outIdx, cmdapObj->respBuffer + 3, packetInfo[readPackCnt].dataLen);
        outIdx += packetInfo[readPackCnt].dataLen;
      }
    }
  }
  if (result == FALSE) {
    free(buff);
    log_error("An error occurred during the transfer.");
    return ADPT_FAILED;
  }
  // 判断是否处理完，如果没有则跳回去重新处理
  if (seqIdx < (sequenceCnt - 1)) {
    // 将已发送包清零
    sendPackCnt = 0;
    goto MAKE_PACKT;
  }
  free(buff);
  return ADPT_SUCCESS;
}

/**
 * DAP_TransferBlock
 * 对单个寄存器进行多次读写，常配合地址自增使用
 * 参数列表和意义与DAP_Transfer相同
 */
static int cmdapTransferBlock(struct cmsis_dap *cmdapObj, uint8_t index, int sequenceCnt, uint8_t *data,
                              uint8_t *response, int *okSeqCnt) {
  assert(cmdapObj != NULL && okSeqCnt != NULL);

  assert(cmdapObj->PacketSize != 0);
  int result = ADPT_FAILED;
  // 本次传输长度
  int restCnt = 0, readCnt = 0, writeCnt = 0;
  // 发送数据包可以装填的数据个数
  int sentPacketMaxCnt = (cmdapObj->PacketSize - 5) >> 2;
  // 接收数据包可以装填的数据个数
  int readPacketMaxCnt = (cmdapObj->PacketSize - 4) >> 2;

  // 开辟本次数据包的空间
  uint8_t *buff = calloc(cmdapObj->PacketSize, sizeof(uint8_t));
  if (buff == NULL) {
    log_warn("Unable to allocate send packet buffer, the heap may be full.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  // 构造数据包头部
  buff[0] = CMDAP_ID_DAP_TransferBlock;
  buff[1] = index; // DAP index, JTAG ScanChain 中的位置，在SWD模式下忽略该参数
  for (*okSeqCnt = 0; *okSeqCnt < sequenceCnt; (*okSeqCnt)++) {
    restCnt = *CAST(int *, data + readCnt);
    readCnt += sizeof(int);
    uint8_t seq = *CAST(uint8_t *, data + readCnt++);
    if (seq & CMDAP_TRANSFER_RnW) { // 读操作
      while (restCnt > 0) {
        if (restCnt > readPacketMaxCnt) {
          *CAST(uint16_t *, buff + 2) = readPacketMaxCnt;
          buff[4] = seq;
          // 交换数据
          DAP_EXCHANGE_DATA(cmdapObj, buff, 5);
          // 判断操作成功，写回数据 XXX 小端字节序
          if (*CAST(uint16_t *, cmdapObj->respBuffer + 1) == readPacketMaxCnt && cmdapObj->respBuffer[3] == CMDAP_TRANSFER_OK) { // 成功
            memcpy(response + writeCnt, cmdapObj->respBuffer + 4, readPacketMaxCnt << 2);
            writeCnt += readPacketMaxCnt << 2;
            restCnt -= readPacketMaxCnt; // 成功读取readPacketMaxCnt个字
          } else {                       // 失败
            goto END;
          }
        } else {
          *CAST(uint16_t *, buff + 2) = restCnt;
          buff[4] = seq;
          // 交换数据
          DAP_EXCHANGE_DATA(cmdapObj, buff, 5);
          // 判断操作成功，写回数据 XXX 小端字节序
          if (*CAST(uint16_t *, cmdapObj->respBuffer + 1) == restCnt && cmdapObj->respBuffer[3] == CMDAP_TRANSFER_OK) { // 成功
            memcpy(response + writeCnt, cmdapObj->respBuffer + 4, restCnt << 2);
            writeCnt += restCnt << 2;
            restCnt = 0; // 全部发送完了
          } else {       // 失败
            goto END;
          }
        }
      }
    } else { // 写操作
      while (restCnt > 0) {
        if (restCnt > sentPacketMaxCnt) {
          *CAST(uint16_t *, buff + 2) = sentPacketMaxCnt;
          buff[4] = seq;
          // 拷贝数据
          memcpy(buff + 5, data + readCnt, sentPacketMaxCnt << 2);
          readCnt += sentPacketMaxCnt << 2;
          // 交换数据
          DAP_EXCHANGE_DATA(cmdapObj, buff, 5 + (sentPacketMaxCnt << 2));
          // 判断操作成功 XXX 小端字节序
          if (*CAST(uint16_t *, cmdapObj->respBuffer + 1) == sentPacketMaxCnt && cmdapObj->respBuffer[3] == CMDAP_TRANSFER_OK) { // 成功
            restCnt -= sentPacketMaxCnt;
          } else { // 失败
            goto END;
          }
        } else {
          *CAST(uint16_t *, buff + 2) = restCnt;
          buff[4] = seq;
          // 拷贝数据
          memcpy(buff + 5, data + readCnt, restCnt << 2);
          readCnt += restCnt << 2;
          // 交换数据 FIXED 以后运算符根据优先级都要打上括号！！MMP
          DAP_EXCHANGE_DATA(cmdapObj, buff, 5 + (restCnt << 2));
          // 判断操作成功 XXX 小端字节序
          if (*CAST(uint16_t *, cmdapObj->respBuffer + 1) == restCnt && cmdapObj->respBuffer[3] == CMDAP_TRANSFER_OK) { // 成功
            restCnt = 0;
          } else { // 失败
            goto END;
          }
        }
      }
    }
  }
  result = ADPT_SUCCESS;
END:
  free(buff);
  return result;
}

/**
 * 复位操作
 * hard：是否硬复位（nTRST、nSRST）
 * srst：系统复位nRESET
 * pinWait：死区时间
 * 注意：有的电路没有连接nTRST，所以TAP状态机复位尽量使用软复位
 */
static int dapReset(Adapter self, enum targetResetType type) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  uint8_t pinData = 0, pinMask = 0; // 状态机复位，
  switch (type) {
  case ADPT_RESET_SYSTEM: // 系统复位,assert nSRST
    pinMask |= JTAG_PIN_nRESET;
    if (dapSwjPins((JtagSkill)&cmdapObj->jtagSkillAPI, pinMask, pinData, &pinData, 100000) != ADPT_SUCCESS) { // 死区时间100ms
      log_error("Assert reset pin failed!");
      return ADPT_FAILED;
    }
    // 取消复位
    pinData = 0xFF;
    if (dapSwjPins((JtagSkill)&cmdapObj->jtagSkillAPI, pinMask, pinData, &pinData, 0) != ADPT_SUCCESS) {
      log_error("Deassert reset pin failed!");
      return ADPT_FAILED;
    }
    // 更新TAP状态机
    INTERFACE_CONST_INIT(enum JTAG_TAP_State, cmdapObj->jtagSkillAPI.currState, JTAG_TAP_RESET);
    return ADPT_SUCCESS;
  case ADPT_RESET_DEBUG:
    if (self->currTransMode == ADPT_MODE_JTAG) {
      // TMS上面5周期的高电平
      uint8_t resetSeq[] = {0x45, 0x00};
      if (cmdapJtagSequence(cmdapObj, 1, resetSeq, NULL) != ADPT_SUCCESS) {
        log_error("Failed to send 5 clock high level signal to TMS.");
        return ADPT_FAILED;
      }
    } else if (self->currTransMode == ADPT_MODE_SWD) {
      // 发送SWD Line Reset
      return CmdapSwdLineReset(self);
    } else {
      return ADPT_ERR_UNSUPPORT;
    }
    // 更新TAP状态机
    INTERFACE_CONST_INIT(enum JTAG_TAP_State, cmdapObj->jtagSkillAPI.currState, JTAG_TAP_RESET);
    return ADPT_SUCCESS;
  }
  return ADPT_ERR_UNSUPPORT;
}

/**
 * 配置SWD
 */
int CmdapSwdConfig(Adapter self, uint8_t cfg) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  uint8_t DAP_SWDCFGPack[2] = {CMDAP_ID_DAP_SWD_Configure};
  // 构造包数据
  DAP_SWDCFGPack[1] = cfg;

  DAP_EXCHANGE_DATA(cmdapObj, DAP_SWDCFGPack, sizeof(DAP_SWDCFGPack));
  if (*(cmdapObj->respBuffer + 1) != CMDAP_OK) {
    return ADPT_FAILED;
  }
  return ADPT_SUCCESS;
}

/**
 * 解析TMS信息，并写入到buff
 * seqInfo:由JTAG_getTMSSequence函数返回的TMS时序信息
 * seqCnt:
 * 返回写入的字节数
 * Sequence Info: Contains number of TDI bits and fixed TMS value
    Bit 5 .. 0: Number of TCK cycles: 1 .. 64 (64 encoded as 0)
    Bit 6: TMS value
    Bit 7: TDO Capture
 *
 */
static int parseTMS(uint8_t *buff, TMS_SeqInfo seqInfo, int *seqCnt) {
  assert(buff != NULL);
  uint8_t bitCount = seqInfo & 0xff;
  uint8_t TMS_Seq = seqInfo >> 8;
  if (bitCount == 0)
    return 0;
  int writeCount = 1;
  *buff = (TMS_Seq & 0x1) << 6;
  // sequence 个数+1
  (*seqCnt)++;
  while (bitCount--) {
    (*buff)++;
    TMS_Seq >>= 1;
    if (bitCount && (((TMS_Seq & 0x1) << 6) ^ (*buff & 0x40))) {
      *++buff = 0;
      *++buff = (TMS_Seq & 0x1) << 6;
      (*seqCnt)++;
      writeCount += 2;
    }
  }
  *++buff = 0;
  return writeCount + 1;
}

/**
 * 解析TDI数据
 */
static int parseTDI(uint8_t *buff, uint8_t *TDIData, int bitCnt, int *seqCnt) {
  assert(buff != NULL);
  int writeCnt = 0; // 写入了多少个字节
  for (int n = bitCnt - 1, readCnt = 0; n > 0;) {
    if (n >= 64) {    // 当生成的时序字节小于8个
      *buff++ = 0x80; // TMS=0;TDO Capture=1; 64Bit
      (*seqCnt)++;
      memcpy(buff, TDIData + readCnt, 8);
      buff += 8;
      n -= 64;
      readCnt += 8;
      writeCnt += 9; // 数据加上一个头部
    } else {
      int bytesCnt = (n + 7) >> 3;
      *buff++ = 0x80 + n;
      (*seqCnt)++;
      memcpy(buff, TDIData + readCnt, bytesCnt);
      buff += bytesCnt;
      readCnt += bytesCnt;
      writeCnt += bytesCnt + 1; // 数据加上一个头部
      n = 0;
    }
  }
  // 解析最后一位
  *buff++ = 0xC1; // 0xC1 TMS=1 TCLK=1 TDO Capature=1
  *buff++ = GET_Nth_BIT(TDIData, bitCnt - 1);
  writeCnt += 2;
  (*seqCnt)++;
  return writeCnt;
}

/**
 * 解析IDLE Wait
 */
static int parseIDLEWait(uint8_t *buff, int wait, int *seqCnt) {
  assert(buff != NULL);
  int writeCnt = 0; // 写入了多少个字节
  for (int n = wait; n > 0;) {
    if (n >= 64) {    // 当生成的时序字节小于8个
      *buff++ = 0x00; // TMS=0;TDO Capture=0; 64Bit
      (*seqCnt)++;
      memset(buff, 0x0, 8);
      buff += 8;
      n -= 64;
      writeCnt += 9; // 数据加上一个头部
    } else {
      int bytesCnt = (n + 7) >> 3;
      *buff++ = 0x00 + n;
      (*seqCnt)++;
      memset(buff, 0x0, bytesCnt);
      buff += bytesCnt;
      writeCnt += bytesCnt + 1; // 数据加上一个头部
      n = 0;
    }
  }
  return writeCnt;
}

/**
 * 解析执行JTAG指令队列
 */
static int executeJtagCmd(JtagSkill self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_JTAG_SKILL(self);

  enum JTAG_TAP_State tempState = cmdapObj->jtagSkillAPI.currState; // 临时JTAG状态机状态
  int seqCnt = 0;                                                   // CMSIS-DAP的Sequence个数，具体看CMSIS-DAP手册
  int writeBuffLen = 0;                                             // 生成指令缓冲区的长度
  int readBuffLen = 0;                                              // 需要读的字节个数
  int writeCnt = 0, readCnt = 0;                                    // 写入数据个数，读取数据个数
  // 遍历指令，计算解析后的数据长度，开辟空间
  struct JTAG_Command *cmd, *cmd_t;
  list_for_each_entry(cmd, &cmdapObj->JtagInsQueue, list_entry) {
    switch (cmd->type) {
    case JTAG_INS_STATUS_MOVE: // 状态机切换
      // 如果要到达的状态与当前状态一致,则跳过该指令
      if (cmd->instr.statusMove.toState == tempState) {
        continue;
      }
      do {
        // 电平状态数乘以2，因为CMSIS-DAP的JTAG指令格式是一个seqinfo加一个tdi数据，而且TAP状态机的任意两个状态切换tms不会多于8个时钟
        // 高8位为时序信息
        TMS_SeqInfo tmsSeq = JtagGetTmsSequence(tempState, cmd->instr.statusMove.toState);
        writeBuffLen += JtagCalTmsLevelState(tmsSeq >> 8, tmsSeq & 0xff) * 2;
      } while (0);
      // 更新当前临时状态机
      tempState = cmd->instr.statusMove.toState;
      break;
    // 交换TDI和TDO之间的数据
    case JTAG_INS_EXCHANGE_DATA:
      //检查当前TAP状态
      if (tempState != JTAG_TAP_DRSHIFT && tempState != JTAG_TAP_IRSHIFT) {
        log_error(
            "Current TAP status is not JTAG_TAP_DRSHIFT or "
            "JTAG_TAP_IRSHIFT!");
        return ADPT_FAILED;
      }
      do {
        int writeCnt = 0, readCnt = 0;
        for (int n = cmd->instr.exchangeData.bitCount - 1; n > 0;) {
          if (n >= 64) {
            n -= 64;
            readCnt += 8;
            writeCnt += 9; // 数据加上一个头部
          } else {
            int bytesCnt = (n + 7) >> 3;
            readCnt += bytesCnt;
            writeCnt += bytesCnt + 1; // 数据加上一个头部
            n = 0;
          }
        }
        writeBuffLen += writeCnt + 2; // 最后加2是跳出SHIFT-xR状态
        readBuffLen += readCnt + 1;   // 最后+1是跳出SHIFT-xR的状态需要的一位数据
      } while (0);
      // 更新当前JTAG状态机到下一个状态
      tempState++;
      break;

    case JTAG_INS_IDLE_WAIT:
      if (tempState != JTAG_TAP_IDLE) { //检查当前TAP状态
        log_error("Current TAP status is not JTAG_TAP_IDLE!");
        return ADPT_FAILED;
      }
      // 根据等待时钟周期数来计算需要多少个控制字，因为CMSIS-DAP的等待时钟周期每个控制字最多可以有64个
      // 所以等待周期多的时候会需要多个控制字
      writeBuffLen += BIT2BYTE(cmd->instr.idleWait.clkCount);
      break;
    }
  }

  // 创建内存空间
  log_trace("CMSIS-DAP JTAG Parsed buff length: %d, read buff length: %d.", writeBuffLen, readBuffLen);
  uint8_t *writeBuff = malloc(writeBuffLen * sizeof(uint8_t));
  if (writeBuff == NULL) {
    log_warn("CMSIS-DAP JTAG Instruct buff allocte failed.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  uint8_t *readBuff = malloc(readBuffLen * sizeof(uint8_t));
  if (readBuff == NULL) {
    log_warn("CMSIS-DAP JTAG Read buff allocte failed.");
    free(writeBuff);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  tempState = cmdapObj->jtagSkillAPI.currState; // 重置临时JTAG状态机状态
  // 第二次遍历，生成指令对应的数据
  list_for_each_entry(cmd, &cmdapObj->JtagInsQueue, list_entry) {
    switch (cmd->type) {
    case JTAG_INS_STATUS_MOVE: // 状态机切换
      do {
        // 高8位为时序信息
        TMS_SeqInfo tmsSeq = JtagGetTmsSequence(tempState, cmd->instr.statusMove.toState);
        writeCnt += parseTMS(writeBuff + writeCnt, tmsSeq, &seqCnt);
        // 更新当前临时状态机
        tempState = cmd->instr.statusMove.toState;
      } while (0);
      break;
    case JTAG_INS_EXCHANGE_DATA: // 交换IO
      writeCnt += parseTDI(writeBuff + writeCnt, cmd->instr.exchangeData.data, cmd->instr.exchangeData.bitCount, &seqCnt);
      // 更新当前JTAG状态机到下一个状态
      tempState++;
      break;
    case JTAG_INS_IDLE_WAIT: // 进入IDLE状态等待
      writeCnt += parseIDLEWait(writeBuff + writeCnt, cmd->instr.idleWait.clkCount, &seqCnt);
      break;
    }
  }

  // 执行指令
  if (cmdapJtagSequence(cmdapObj, seqCnt, writeBuff, readBuff) != ADPT_SUCCESS) {
    log_warn("Execute JTAG Instruction Failed.");
    free(writeBuff);
    free(readBuff);
    return ADPT_FAILED;
  }
  //	int misc_PrintBulk(char *data, int length, int rowLen);
  //	misc_PrintBulk(writeBuff, writeBuffLen, 32);
  //	misc_PrintBulk(readBuff, readBuffLen, 32);

  // 第三次遍历：同步数据，并删除执行成功的指令
  list_for_each_entry_safe(cmd, cmd_t, &cmdapObj->JtagInsQueue, list_entry) {
    // 跳过状态机改变指令
    if (cmd->type == JTAG_INS_STATUS_MOVE || cmd->type == JTAG_INS_IDLE_WAIT) {
      goto FREE_CMD;
    }
    // 所占的数据长度
    int byteCnt = (cmd->instr.exchangeData.bitCount + 7) >> 3;
    // 剩余二进制位个数
    int restBit = (cmd->instr.exchangeData.bitCount - 1) & 0x7;
    memcpy(cmd->instr.exchangeData.data, readBuff + readCnt, byteCnt);
    readCnt += byteCnt;
    /**
     * bitCnt-1之后如果是8的倍数，就不需要组合最后一字节的数据
     */
    // 将最后一个字节的数据组合到前一个字节上
    if (restBit != 0) {
      *(cmd->instr.exchangeData.data + byteCnt - 1) |= (*(readBuff + readCnt) & 1) << restBit;
      readCnt++;
    }
  FREE_CMD:
    list_del(&cmd->list_entry);
    free(cmd);
  }
  // 更新当前TAP状态机
  INTERFACE_CONST_INIT(enum JTAG_TAP_State, cmdapObj->jtagSkillAPI.currState, tempState);

  // 释放资源
  free(writeBuff);
  free(readBuff);
  return ADPT_SUCCESS;
}

// 创建新的JTAG指令对象，并将其插入JTAG指令队列尾部
static struct JTAG_Command *newJtagCommand(struct cmsis_dap *cmdapObj) {
  assert(cmdapObj != NULL);
  // 新建指令对象
  struct JTAG_Command *command = calloc(1, sizeof(struct JTAG_Command));
  if (command == NULL) {
    log_error("Failed to create a new JTAG Command object.");
    return NULL;
  }
  // 将指令插入链表尾部
  list_add_tail(&command->list_entry, &cmdapObj->JtagInsQueue);
  return command;
}

/**
 * 交换TDI和TDO的数据，在传输完成后会自动将状态机从SHIFT-xR跳转到EXTI1-xR
 * bitCount：需要传输的位个数
 * dataPtr:传输的数据
 */
static int addJtagExchangeData(JtagSkill self, uint8_t *dataPtr, unsigned int bitCount) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_JTAG_SKILL(self);
  // 新建指令
  struct JTAG_Command *command = newJtagCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = JTAG_INS_EXCHANGE_DATA;
  command->instr.exchangeData.bitCount = bitCount;
  command->instr.exchangeData.data = dataPtr;
  return ADPT_SUCCESS;
}

/**
 * JTAG Idle
 */
static int addJtagIdle(JtagSkill self, unsigned int clkCnt) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_JTAG_SKILL(self);
  // 新建指令
  struct JTAG_Command *command = newJtagCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = JTAG_INS_IDLE_WAIT;
  command->instr.idleWait.clkCount = clkCnt;
  return ADPT_SUCCESS;
}

/**
 * JTAG to state
 */
static int addJtagToState(JtagSkill self, enum JTAG_TAP_State toState) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_JTAG_SKILL(self);
  // 新建指令
  struct JTAG_Command *command = newJtagCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = JTAG_INS_STATUS_MOVE;
  command->instr.statusMove.toState = toState;
  return ADPT_SUCCESS;
}

/**
 * JTAG Clean pending
 */
static int cleanJtagInsQueue(JtagSkill self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_JTAG_SKILL(self);

  struct JTAG_Command *cmd, *cmd_t;
  list_for_each_entry_safe(cmd, cmd_t, &cmdapObj->JtagInsQueue, list_entry) {
    list_del(&cmd->list_entry);
    free(cmd);
  }
  return ADPT_SUCCESS;
}

/**
 * 解析执行DAP指令队列
 * 注意：对于读操作，成功之后才写入内存地址，如果读取失败，则值保持不变，不要清零
 */
static int executeDapCmd(DapSkill self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_DAP_SKILL(self);

  struct DAP_Command *cmd, *cmd_t;
  int readCnt, writeCnt, seqCnt;
  int readBuffLen, writeBuffLen;

REEXEC:;
  readCnt = 0;
  writeCnt = 0;
  seqCnt = 0;
  readBuffLen = 0;
  writeBuffLen = 0;
  // 本次处理的指令类型，找到指令队列中第一个指令的类型
  enum DAP_InstrType thisType = container_of(cmdapObj->DapInsQueue.next, struct DAP_Command, list_entry)->type;
  // 第一次遍历，计算所占用的空间
  list_for_each_entry(cmd, &cmdapObj->DapInsQueue, list_entry) {
    if (cmd->type != thisType) {
      break;
    }
    switch (cmd->type) {
    case DAP_INS_RW_REG_SINGLE: // 单次读写寄存器
      writeBuffLen += 1;
      if ((cmd->instr.singleReg.request & 0x2) == 0x2) { // 读操作
        readBuffLen += 4;
      } else {
        writeBuffLen += 4;
      }
      break;

    case DAP_INS_RW_REG_MULTI:                          // 多次读写寄存器
      writeBuffLen += 1 + sizeof(int);                  // int:blockCnt, byte:seq
      if ((cmd->instr.multiReg.request & 0x2) == 0x2) { // 读操作
        readBuffLen += cmd->instr.multiReg.count << 2;
      } else {
        writeBuffLen += cmd->instr.multiReg.count << 2;
      }
      break;
    }
  }
  // 分配内存空间
  log_trace("CMSIS-DAP DAP Parsed buff length: %d, read buff length: %d.", writeBuffLen, readBuffLen);
  uint8_t *writeBuff = malloc(writeBuffLen * sizeof(uint8_t));
  if (writeBuff == NULL) {
    log_warn("CMSIS-DAP DAP Instruct buff allocte failed.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  uint8_t *readBuff = malloc(readBuffLen * sizeof(uint8_t));
  if (readBuff == NULL) {
    log_warn("CMSIS-DAP DAP Read buff allocte failed.");
    free(writeBuff);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  // 第二次遍历 生成指令数据
  list_for_each_entry(cmd, &cmdapObj->DapInsQueue, list_entry) {
    if (cmd->type != thisType) {
      break;
    }
    switch (cmd->type) {
    case DAP_INS_RW_REG_SINGLE:
      *(writeBuff + writeCnt++) = cmd->instr.singleReg.request;

      /*
        do {
          log_debug("-----------------------------");
          log_debug("%s %s Reg %x.",
                    cmd->instr.singleReg.request & 0x2 ? "Read" : "Write",
                    cmd->instr.singleReg.request & 0x1 ? "AP" : "DP",
                    cmd->instr.singleReg.request & 0xC);
          if ((cmd->instr.singleReg.request & 0x2) == 0) {
            log_debug("0x%08X", cmd->instr.singleReg.data.write);
          }
        } while (0);
        */

      // 如果是写操作
      if ((cmd->instr.singleReg.request & 0x2) == 0) {
        // XXX 小端字节序
        memcpy(writeBuff + writeCnt, CAST(uint8_t *, &cmd->instr.singleReg.data.write), 4);
        writeCnt += 4;
      }
      seqCnt++;
      break;

    case DAP_INS_RW_REG_MULTI:
      // 写入本次操作的次数
      *CAST(int *, writeBuff + writeCnt) = cmd->instr.multiReg.count;
      writeCnt += sizeof(int);
      *(writeBuff + writeCnt++) = cmd->instr.multiReg.request;
      // 如果是写操作
      if ((cmd->instr.multiReg.request & 0x2) == 0) {
        // XXX 小端字节序
        memcpy(writeBuff + writeCnt, CAST(uint8_t *, cmd->instr.multiReg.data), cmd->instr.multiReg.count << 2);
        writeCnt += cmd->instr.multiReg.count << 2;
      }
      seqCnt++;
      break;
    }
  }

  // 执行成功的Sequence个数
  int okSeqCnt = 0;
  int result = ADPT_SUCCESS;
  switch (thisType) {
  case DAP_INS_RW_REG_SINGLE:
    // 执行指令 DAP_Transfer
    if (cmdapTransfer(cmdapObj, cmdapObj->tapIndex, seqCnt, writeBuff, readBuff, &okSeqCnt) != ADPT_SUCCESS) {
      log_error(
          "DAP_Transfer:Some DAP Instruction Execute Failed. Success:%d, "
          "All:%d.",
          okSeqCnt, seqCnt);
      result = ADPT_FAILED;
    }
    break;

  case DAP_INS_RW_REG_MULTI:
    // transfer block
    if (cmdapTransferBlock(cmdapObj, cmdapObj->tapIndex, seqCnt, writeBuff, readBuff, &okSeqCnt) != ADPT_SUCCESS) {
      log_error(
          "DAP_TransferBlock:Some DAP Instruction Execute Failed. "
          "Success:%d, All:%d.",
          okSeqCnt, seqCnt);
      result = ADPT_FAILED;
    }
    break;
  }

  // 第三次遍历：同步数据
  list_for_each_entry_safe(cmd, cmd_t, &cmdapObj->DapInsQueue, list_entry) {
    if (cmd->type != thisType) {
      break;
    }
    if (okSeqCnt <= 0) {
      // 未执行的指令保存在指令队列中
      break;
    }
    okSeqCnt--;                                                                              // 只同步执行成功的Seq个数
    if (cmd->type == DAP_INS_RW_REG_SINGLE && (cmd->instr.singleReg.request & 0x2) == 0x2) { // 单次读寄存器
      memcpy(cmd->instr.singleReg.data.read, readBuff + readCnt, 4);
      readCnt += 4;
    } else if (cmd->type == DAP_INS_RW_REG_MULTI && (cmd->instr.multiReg.request & 0x2) == 0x2) { // 多次读寄存器
      memcpy(cmd->instr.multiReg.data, readBuff + readCnt, cmd->instr.multiReg.count << 2);
      readCnt += cmd->instr.multiReg.count << 2;
    }
    list_del(&cmd->list_entry);
    free(cmd);
  }
  free(writeBuff);
  free(readBuff);
  // 判断是否继续执行
  if (result == ADPT_SUCCESS && !list_empty(&cmdapObj->DapInsQueue)) {
    goto REEXEC;
  }
  return result;
}

// 创建新的DAP指令对象，并将其插入JTAG指令队列尾部
static struct DAP_Command *newDapCommand(struct cmsis_dap *cmdapObj) {
  assert(cmdapObj != NULL);
  // 新建指令对象
  struct DAP_Command *command = calloc(1, sizeof(struct DAP_Command));
  if (command == NULL) {
    log_error("Failed to create a new DAP Command object.");
    return NULL;
  }
  // 将指令插入链表尾部
  list_add_tail(&command->list_entry, &cmdapObj->DapInsQueue);
  return command;
}

/* 增加单次读寄存器指令 */
static int addDapSingleRead(DapSkill self, enum dapRegType type, int reg, uint32_t *data) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_DAP_SKILL(self);

  // 新建指令
  struct DAP_Command *command = newDapCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = DAP_INS_RW_REG_SINGLE;
  command->instr.singleReg.request = (reg & 0xC) | 0x2;
  if (type == SKILL_DAP_AP_REG) {
    command->instr.singleReg.request |= 0x1;
  }
  command->instr.singleReg.data.read = data;
  return ADPT_SUCCESS;
}

/* 增加单次写寄存器指令 */
static int addDapSingleWrite(DapSkill self, enum dapRegType type, int reg, uint32_t data) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_DAP_SKILL(self);

  // 新建指令
  struct DAP_Command *command = newDapCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = DAP_INS_RW_REG_SINGLE;
  command->instr.singleReg.request = (reg & 0xC);
  if (type == SKILL_DAP_AP_REG) {
    command->instr.singleReg.request |= 0x1;
  }
  command->instr.singleReg.data.write = data;
  return ADPT_SUCCESS;
}

/* 增加多次读寄存器指令 */
static int addDapMultiRead(DapSkill self, enum dapRegType type, int reg, int count, uint32_t *data) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_DAP_SKILL(self);
  if (count <= 0) {
    log_error("Count must be greater than 0.");
    return ADPT_ERR_BAD_PARAMETER;
  }
  // 新建指令
  struct DAP_Command *command = newDapCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = DAP_INS_RW_REG_MULTI;
  command->instr.multiReg.request = (reg & 0xC) | 0x2;
  if (type == SKILL_DAP_AP_REG) {
    command->instr.multiReg.request |= 0x1;
  }
  command->instr.multiReg.data = data;
  command->instr.multiReg.count = count;
  return ADPT_SUCCESS;
}

/* 增加多次写寄存器指令 */
static int addDapMultiWrite(DapSkill self, enum dapRegType type, int reg, int count,
                            uint32_t *data) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_DAP_SKILL(self);

  if (count <= 0) {
    log_error("Count must be greater than 0.");
    return ADPT_ERR_BAD_PARAMETER;
  }
  // 新建指令
  struct DAP_Command *command = newDapCommand(cmdapObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  command->type = DAP_INS_RW_REG_MULTI;
  command->instr.multiReg.request = (reg & 0xC);
  if (type == SKILL_DAP_AP_REG) {
    command->instr.multiReg.request |= 0x1;
  }
  command->instr.multiReg.data = data;
  command->instr.multiReg.count = count;
  return ADPT_SUCCESS;
}

/* 清空DAP指令队列 */
static int cleanDapInsQueue(DapSkill self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_DAP_SKILL(self);

  struct DAP_Command *cmd, *cmd_t;
  list_for_each_entry_safe(cmd, cmd_t, &cmdapObj->DapInsQueue, list_entry) {
    list_del(&cmd->list_entry);
    free(cmd);
  }
  return ADPT_SUCCESS;
}

/**
 * DAP写ABORT寄存器
 * The DAP_WriteABORT Command writes an abort request to the CoreSight ABORT
 * register of the Target Device.
 */
int CmdapWriteAbort(Adapter self, uint32_t data) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);

  uint8_t DAP_AbortPacket[6] = {CMDAP_ID_DAP_WriteABORT};
  DAP_AbortPacket[1] = cmdapObj->tapIndex;
  DAP_AbortPacket[2] = BYTE_IDX(data, 0);
  DAP_AbortPacket[3] = BYTE_IDX(data, 1);
  DAP_AbortPacket[4] = BYTE_IDX(data, 2);
  DAP_AbortPacket[5] = BYTE_IDX(data, 3);

  DAP_EXCHANGE_DATA(cmdapObj, DAP_AbortPacket, sizeof(DAP_AbortPacket));
  if (cmdapObj->respBuffer[1] != CMDAP_OK) {
    return ADPT_FAILED;
  }
  return ADPT_SUCCESS;
}

/**
 * 创建新的CMSIS-DAP仿真器对象
 */
Adapter CreateCmsisDap(void) {
  struct cmsis_dap *obj = calloc(1, sizeof(struct cmsis_dap));
  if (!obj) {
    log_error("CreateCmsisDap:Can not create object.");
    return NULL;
  }

  // 创建USB对象
  USB usbObj = CreateUSB();
  if (!usbObj) {
    log_error("Failed to get USB object.");
    free(obj);
    return NULL;
  }

  // 初始化指令和传输协议链表
  INIT_LIST_HEAD(&obj->JtagInsQueue);
  INIT_LIST_HEAD(&obj->DapInsQueue);
  INIT_LIST_HEAD(&obj->adaperAPI.skills);

  // 设置参数
  obj->usbObj = usbObj;
  obj->signature = SIGNATURE_32('C', 'D', 'A', 'P');
  // 设置接口参数
  obj->adaperAPI.SetStatus = dapHostStatus;
  obj->adaperAPI.SetFrequency = dapSwjClock;
  obj->adaperAPI.Reset = dapReset;
  obj->adaperAPI.SetTransferMode = dapSetTransMode;

  INIT_LIST_HEAD(&obj->jtagSkillAPI.header.skills);
  list_add(&obj->jtagSkillAPI.header.skills, &obj->adaperAPI.skills);

  obj->jtagSkillAPI.header.type = ADPT_SKILL_JTAG;
  obj->jtagSkillAPI.Pins = dapSwjPins;
  obj->jtagSkillAPI.ExchangeData = addJtagExchangeData;
  obj->jtagSkillAPI.Idle = addJtagIdle;
  obj->jtagSkillAPI.ToState = addJtagToState;
  obj->jtagSkillAPI.Commit = executeJtagCmd;
  obj->jtagSkillAPI.Cancel = cleanJtagInsQueue;

  INIT_LIST_HEAD(&obj->dapSkillAPI.header.skills);
  list_add(&obj->dapSkillAPI.header.skills, &obj->adaperAPI.skills);

  obj->dapSkillAPI.header.type = ADPT_SKILL_DAP;
  obj->dapSkillAPI.SingleRead = addDapSingleRead;
  obj->dapSkillAPI.SingleWrite = addDapSingleWrite;
  obj->dapSkillAPI.MultiRead = addDapMultiRead;
  obj->dapSkillAPI.MultiWrite = addDapMultiWrite;
  obj->dapSkillAPI.Commit = executeDapCmd;
  obj->dapSkillAPI.Cancel = cleanDapInsQueue;

  obj->connected = FALSE;

  return (Adapter)&obj->adaperAPI;
}

// 释放CMSIS-DAP对象
void DestroyCmsisDap(Adapter *self) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(*self);

  // 关闭USB对象
  if (cmdapObj->connected == TRUE) {
    log_debug("DestroyCmsisDap: Disconnect USB.");
    // 断开USB
    USB_Close(cmdapObj->usbObj);
    cmdapObj->connected = FALSE;
  }
  // 释放USB对象
  DestoryUSB(&cmdapObj->usbObj);

  // 释放respBuffer
  if (cmdapObj->respBuffer != NULL) {
    free(cmdapObj->respBuffer);
  }

  free(cmdapObj);
  *self = NULL;
}

/**
 * 设置DAP模式下,TAP在扫描链中的索引位置
 */
int CmdapSetTapIndex(Adapter self, unsigned int index) {
  struct cmsis_dap *cmdapObj = CMDAP_OBJ_FORM_ADAPTER(self);
  if (index >= cmdapObj->tapCount) {
    log_error("The index of the TAP is greater than the value of tapCount.");
    return ADPT_ERR_BAD_PARAMETER;
  }
  cmdapObj->tapIndex = index;
  return ADPT_SUCCESS;
}
