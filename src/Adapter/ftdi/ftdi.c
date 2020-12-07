/**
 * src/Adapter/ftdi/ftdi.c
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
#include "smartocd.h"

#include "Adapter/ftdi/ftdi.h"

#include "Library/misc/list.h"
#include "Library/misc/misc.h"
#include "Library/log/log.h"

#include <libftdi1/ftdi.h>

// FTDI 支持最大传输多少个byte
#define FTDI_MAX_PKT_LEN  65536

struct ftdi {
  uint32_t signature;
  struct ftdi_context ctx;   // FTDI库相关对象
  struct adapter adapterAPI; // Adapter接口对象
  BOOL connected;            // 设备是否已连接
  int interface;             // 当前选择的FTDI channel/interface
  unsigned char latency;     // 延迟定时器，当收到数据之后，在buffer内缓冲n ms之后再发向usb总线

  struct jtagSkill jtagSkillAPI; // jtag能力集接口
  struct list_head JtagInsQueue; // JTAG指令队列，元素类型：struct JTAG_Command
};

// JTAG指令定义和CMSIS-DAP中一样
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

#define OFFSET_ADAPTER offsetof(struct ftdi, adapterAPI)
#define OFFSET_JTAG_SKILL offsetof(struct ftdi, jtagSkillAPI)
#define FTDI_OBJ_FORM_ADAPTER(x) get_ftdi_obj((void *)(x), OFFSET_ADAPTER)
#define FTDI_OBJ_FORM_JTAG_SKILL(x) get_ftdi_obj((void *)(x), OFFSET_JTAG_SKILL)

// 检查Adapter类型，并返回对应的结构
static struct ftdi *get_ftdi_obj(void *self, size_t offset) {
  assert(self != NULL);
  struct ftdi *obj = (struct ftdi *)((char *)self - offset);
  if (obj->signature != SIGNATURE_32('F', 'T', 'D', 'I')) {
    log_fatal("Adapter object is not FTDI!");
    return NULL; // never reach here, to surpress warnings
  }
  return obj;
}

// 连接FTDI设备
int ConnectFtdi(IN Adapter self, IN const uint16_t *vids, IN const uint16_t *pids,
                IN const char *serialNum, IN int channel) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);
  int ret;
  unsigned char latency_curr;
  uint8_t wbuf[3];

  if (channel < INTERFACE_ANY || channel > INTERFACE_D) {
    log_error("Invalid FTDI interface.");
    return ADPT_ERR_BAD_PARAMETER;
  }

  if (ftdiObj->connected != TRUE) {
    int idx = 0;

    if ((ret = ftdi_set_interface(&ftdiObj->ctx, (enum ftdi_interface)channel)) != 0) {
      log_error("FTDI set interface failed. error code:%d.", ret);
      return ADPT_ERR_INTERNAL_ERROR;
    }

    //如果当前没有连接,则连接CMSIS-DAP设备
    for (; vids[idx] && pids[idx]; idx++) {
      log_debug("Try connecting vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
      if (ftdi_usb_open_desc(&ftdiObj->ctx, vids[idx], pids[idx], NULL, serialNum) == 0) {
        log_info("Successfully connected vid: 0x%02x, pid: 0x%02x usb device.", vids[idx], pids[idx]);
        // 复位设备
        ftdi_usb_reset(&ftdiObj->ctx);
        // 标志已连接
        ftdiObj->connected = TRUE;
        goto _TOINIT; // 跳转到初始化部分
      }
    }
    log_warn("No suitable device found.");
    return ADPT_ERR_NO_DEVICE;
  }

_TOINIT:
  ret = ftdi_set_latency_timer(&ftdiObj->ctx, ftdiObj->latency);
  if (ret != 0) {
    log_error("FTDI set latency timer failed. error code:%d.", ret);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  ret = ftdi_get_latency_timer(&ftdiObj->ctx, &latency_curr);
  if (ret != 0 || ftdiObj->latency != latency_curr) {
    log_error("FTDI set latency timer failed. error code:%d.", ret);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  // 设置工作模式和输入输出模式
  ret = ftdi_set_bitmode(&ftdiObj->ctx, 0x0b, BITMODE_MPSSE);
  if (ret != 0) {
    log_error("FTDI set bit mode failed. error code:%d.", ret);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  // 结束LOOPBACK模式
  wbuf[0] = LOOPBACK_END;
  ret = ftdi_write_data(&ftdiObj->ctx, wbuf, 1);
  if (ret != 1) {
    log_error("FTDI send command error. error code:%d.", ret);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  wbuf[0] = SET_BITS_LOW;
  wbuf[1] = 0x08; // TCK/SK, TDI/DU low, TMS/CS high
  wbuf[2] = 0x0b; // TCK/SK, TDI/DU, TMS/CS output, TDO/D1 and GPIO L1 -> L3 input
  ret = ftdi_write_data(&ftdiObj->ctx, wbuf, 3);
  if (ret != 3) {
    log_error("FTDI send command error. error code:%d.", ret);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  // TAP复位
  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
  wbuf[1] = 0x4;
  wbuf[2] = 0x1f;
  ret = ftdi_write_data(&ftdiObj->ctx, wbuf, 3);
  if (ret != 3) {
    log_error("FTDI send command error. Could not reset TAP. error code:%d.", ret);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  // 当前TAP状态复位
  INTERFACE_CONST_INIT(enum JTAG_TAP_State, ftdiObj->jtagSkillAPI.currState, JTAG_TAP_RESET);

  log_info("FTDI has been initialized.");
  return ADPT_SUCCESS;
}

int DisconnectFtdi(IN Adapter self) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);
  int ret;

  if (ftdiObj->connected) {
    ftdiObj->connected = FALSE;
    ret = ftdi_usb_close(&ftdiObj->ctx);
    if (ret != 0) {
      log_error("Close FTDI failed. error code:%d", ret);
      return ADPT_ERR_INTERNAL_ERROR;
    }
  }
  return ADPT_SUCCESS;
}

// 设置仿真器状态指示灯，不支持
static int ftdiHostStatus(IN Adapter self, IN enum adapterStatus status) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);
  return ADPT_SUCCESS;
}

// 设置mpsse的频率
static int ftdiMpsseFreq(IN Adapter self, IN unsigned int freq) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);
  return ADPT_ERR_UNSUPPORT;
}

// 复位FTDI
static int ftdiReset(IN Adapter self, IN enum targetResetType type) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);

  return ADPT_ERR_UNSUPPORT;
}

// 设置传输模式
// 只支持JTAG
static int ftdiSetTransMode(IN Adapter self, IN enum transferMode mode) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);

  return ADPT_ERR_UNSUPPORT;
}

static int ftdiJtagPins(IN JtagSkill self, IN uint8_t pinMask, IN uint8_t pinDataOut,
                        OUT uint8_t *pinDataIn, IN unsigned int pinWait) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
}

// 创建新的JTAG指令对象，并将其插入JTAG指令队列尾部
static struct JTAG_Command *newJtagCommand(struct ftdi *ftdiObj) {
  assert(ftdiObj != NULL);
  // 新建指令对象
  struct JTAG_Command *command = calloc(1, sizeof(struct JTAG_Command));
  if (command == NULL) {
    log_error("Failed to create a new JTAG Command object.");
    return NULL;
  }
  // 将指令插入链表尾部
  list_add_tail(&command->list_entry, &ftdiObj->JtagInsQueue);
  return command;
}

// 交换TDI-TDO数据
static int ftdiJtagExchangeData(IN JtagSkill self, IN uint8_t *data, IN unsigned int bitCount) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  if (data == NULL || bitCount < 1) {
    log_error("Parameter error. data:%p, bitCount:%d.", data, bitCount);
    return ADPT_ERR_BAD_PARAMETER;
  }

  struct JTAG_Command *command = newJtagCommand(ftdiObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  log_trace("JTAG exchange data. data:%p, bitCount:%d.", data, bitCount);
  command->type = JTAG_INS_EXCHANGE_DATA;
  command->instr.exchangeData.bitCount = bitCount;
  command->instr.exchangeData.data = data;
  return ADPT_SUCCESS;
}

static int ftdiJtagIdle(IN JtagSkill self, IN unsigned int clkCount) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  struct JTAG_Command *command = newJtagCommand(ftdiObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  log_trace("JTAG idle. clkCount:%d.", clkCount);
  command->type = JTAG_INS_IDLE_WAIT;
  command->instr.idleWait.clkCount = clkCount;
  return ADPT_SUCCESS;
}

static int ftdiJtagToState(IN JtagSkill self, IN enum JTAG_TAP_State toState) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  if (JTAG_TAP_RESET > toState || toState > JTAG_TAP_IRUPDATE) {
    log_error("Parameter error. toState:%p.", toState);
    return ADPT_ERR_BAD_PARAMETER;
  }

  struct JTAG_Command *command = newJtagCommand(ftdiObj);
  if (command == NULL) {
    return ADPT_ERR_INTERNAL_ERROR;
  }
  log_trace("JTAG to state. toState:%d, %s.", toState, JtagStateToStr(toState));
  command->type = JTAG_INS_STATUS_MOVE;
  command->instr.statusMove.toState = toState;
  return ADPT_SUCCESS;
}

// 计算要传输的bit个数需要多少个指令
static void getCommandCnt(size_t bitCnt, int *writeCnt, int *readCnt) {
  int writes = 0, reads = 0;
  for (size_t n = bitCnt; n > 0;) {
    // FTDI 最大支持传输65536个字节
    if (n >= (FTDI_MAX_PKT_LEN << 3)) {
      n -= FTDI_MAX_PKT_LEN << 3;
      reads += FTDI_MAX_PKT_LEN;
      writes += FTDI_MAX_PKT_LEN + 3; // 数据加上一个头部
    } else {
      int bytesCnt = n >> 3;
      int restBits = n & 0x7;

      if (bytesCnt > 0) {
        writes += 3 + bytesCnt;
        reads += bytesCnt;
      }

      if (restBits > 0) {
        writes += 3;
        reads += 1;
      }

      n = 0;
    }
  }
  if (writeCnt)
    *writeCnt = writes;
  if (readCnt)
    *readCnt = reads;
}

// 解析TMS时序，生成对应的指令
// 返回往buffer写入的指令个数
static int parseTMS(uint8_t *buff, TMS_SeqInfo seqInfo) {
  assert(buff != NULL);
  int writeCnt = 0;
  if ((seqInfo & 0xFF) == 0) {
    return 0;
  } else if ((seqInfo & 0xFF) > 5) {
    // 如果TMS时序大于5个，则拆分为两条指令传输
    // 规避FT2232的bug
    *buff++ = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
    *buff++ = 0x4; // 传输4+1个clock
    *buff++ = seqInfo >> 8;

    *buff++ = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
    *buff++ = (seqInfo & 0xFF) - 6;
    *buff++ = (seqInfo >> (8 + 5));

    writeCnt = 6;
  } else {
    *buff++ = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
    *buff++ = (seqInfo & 0xFF) - 1;
    *buff++ = seqInfo >> 8;

    writeCnt = 3;
  }

  return writeCnt;
}

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

// 解析TDI数据
static int parseTDI(uint8_t *buff, uint8_t *TDIData, int bitCnt) {
  assert(buff != NULL);
  int writeCnt = 0; // 写入了多少个字节
  for (int n = bitCnt - 1, readCnt = 0; n > 0;) {
    if (n >= FTDI_MAX_PKT_LEN << 3) {
      *buff++ = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ;
      *buff++ = (FTDI_MAX_PKT_LEN - 1) & 0xFF;        // LengthL
      *buff++ = ((FTDI_MAX_PKT_LEN - 1) >> 8) & 0xFF; // lengthH
      memcpy(buff, TDIData + readCnt, FTDI_MAX_PKT_LEN);

      buff += FTDI_MAX_PKT_LEN;
      n -= FTDI_MAX_PKT_LEN << 3;
      readCnt += FTDI_MAX_PKT_LEN;
      writeCnt += FTDI_MAX_PKT_LEN + 3; // 数据加上一个头部
    } else {
      int bytesCnt = n >> 3;
      int restBits = n & 0x7;

      if (bytesCnt > 0) {
        *buff++ = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ;
        *buff++ = (bytesCnt - 1) % 0xFF;
        *buff++ = (bytesCnt - 1) >> 8;
        memcpy(buff, TDIData + readCnt, bytesCnt);

        buff += bytesCnt;
        n -= bytesCnt << 3;
        readCnt += bytesCnt;
        writeCnt += bytesCnt + 3;
      }

      if (restBits > 0) {
        *buff++ = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ | MPSSE_BITMODE;
        *buff++ = restBits - 1;
        *buff++ = *(TDIData + readCnt); // 最后一位不在此命令中传送

        n -= restBits;
        writeCnt += 3;
        assert(n == 0);
      }
    }
  }

  // 解析最后一位
  *buff++ = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG | MPSSE_DO_READ;
  *buff++ = 0; // 1个bit
  *buff++ = 0x1 | (GET_Nth_BIT(TDIData, bitCnt - 1) << 7);
  writeCnt += 3;
  return writeCnt;
}

static int parseIDLEWait(uint8_t *buff, int wait) {
  assert(buff != NULL);
  int writeCnt = 0; // 写入了多少个字节
  for (int n = wait; n > 0;) {
    if (n >= FTDI_MAX_PKT_LEN << 3) {
      *buff++ = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE;
      *buff++ = (FTDI_MAX_PKT_LEN - 1) & 0xFF;        // LengthL
      *buff++ = ((FTDI_MAX_PKT_LEN - 1) >> 8) & 0xFF; // lengthH
      memset(buff, 0x0, FTDI_MAX_PKT_LEN);

      buff += FTDI_MAX_PKT_LEN;
      n -= FTDI_MAX_PKT_LEN << 3;
      writeCnt += FTDI_MAX_PKT_LEN + 3; // 数据加上一个头部
    } else {
      int bytesCnt = n >> 3;
      int restBits = n & 0x7;

      if (bytesCnt > 0) {
        *buff++ = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE;
        *buff++ = (bytesCnt - 1) % 0xFF;
        *buff++ = (bytesCnt - 1) >> 8;
        memset(buff, 0x0, bytesCnt);

        buff += bytesCnt;
        n -= bytesCnt << 3;
        writeCnt += bytesCnt + 3;
      }

      if (restBits > 0) {
        *buff++ = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_BITMODE;
        *buff++ = restBits - 1;
        *buff++ = 0;

        n -= restBits;
        writeCnt += 3;
        assert(n == 0);
      }
    }
  }
  return writeCnt;
}

static int ftdiJtagCommit(IN JtagSkill self) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);

  enum JTAG_TAP_State tempState = ftdiObj->jtagSkillAPI.currState; // 临时JTAG状态机状态
  int writeBuffLen = 0;                                            // 生成指令缓冲区的长度
  int readBuffLen = 0;                                             // 需要读的字节个数
  int writeCnt = 0, readCnt = 0;                                   // 写入数据个数，读取数据个数
  // 遍历指令，检查，计算解析后的数据长度，开辟空间
  struct JTAG_Command *cmd, *cmd_t;
  list_for_each_entry(cmd, &ftdiObj->JtagInsQueue, list_entry) {
    switch (cmd->type) {
    case JTAG_INS_STATUS_MOVE: // 状态机切换
      // 如果要到达的状态与当前状态一致,则跳过该指令
      if (cmd->instr.statusMove.toState == tempState) {
        continue;
      }
      do {
        // 电平状态数乘以2，而且TAP状态机的任意两个状态切换tms不会多于8个时钟
        // 高8位为时序信息
        TMS_SeqInfo tmsSeq = JtagGetTmsSequence(tempState, cmd->instr.statusMove.toState);
        writeBuffLen += 3;
        // 如果时序个数大于5，则剩下的时序在一个新的指令中进行
        // 规避FTDI2232的bug
        if ((tmsSeq & 0xFF) > 5) {
          writeBuffLen += 3;
        }
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
        assert(cmd->instr.exchangeData.bitCount > 1);
        getCommandCnt(cmd->instr.exchangeData.bitCount - 1, &writeCnt, &readCnt);
        writeBuffLen += writeCnt + 3; // 最后加3是跳出SHIFT-xR状态
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

      // 根据等待时钟周期数来计算需要多少个控制字
      do {
        int writeCnt = 0;
        getCommandCnt(cmd->instr.idleWait.clkCount, &writeCnt, NULL);
        writeBuffLen += writeCnt;
      } while (0);
      break;
    }
  }

  // 创建内存空间
  log_trace("FTDI JTAG Parsed buff length: %d, read buff length: %d.", writeBuffLen, readBuffLen);
  uint8_t *writeBuff = malloc(writeBuffLen * sizeof(uint8_t));
  if (writeBuff == NULL) {
    log_warn("FTDI JTAG Instruct buff allocte failed.");
    return ADPT_ERR_INTERNAL_ERROR;
  }
  uint8_t *readBuff = readBuffLen > 0 ? malloc(readBuffLen * sizeof(uint8_t)) : NULL;
  if (readBuffLen > 0 && readBuff == NULL) {
    log_warn("FTDI JTAG Read buff allocte failed.");
    free(writeBuff);
    return ADPT_ERR_INTERNAL_ERROR;
  }

  tempState = ftdiObj->jtagSkillAPI.currState; // 重置临时JTAG状态机状态
  // 第二次遍历，生成指令对应的数据
  list_for_each_entry(cmd, &ftdiObj->JtagInsQueue, list_entry) {
    switch (cmd->type) {
    case JTAG_INS_STATUS_MOVE: // 状态机切换
      do {
        // 高8位为时序信息
        TMS_SeqInfo tmsSeq = JtagGetTmsSequence(tempState, cmd->instr.statusMove.toState);
        writeCnt += parseTMS(writeBuff + writeCnt, tmsSeq);
        // 更新当前临时状态机
        tempState = cmd->instr.statusMove.toState;
      } while (0);
      break;
    case JTAG_INS_EXCHANGE_DATA: // 交换
      writeCnt += parseTDI(writeBuff + writeCnt, cmd->instr.exchangeData.data, cmd->instr.exchangeData.bitCount);
      // 更新当前JTAG状态机到下一个状态
      tempState++;
      break;
    case JTAG_INS_IDLE_WAIT: // 进入IDLE状态等待
      writeCnt += parseIDLEWait(writeBuff + writeCnt, cmd->instr.idleWait.clkCount);
      break;
    }
  }

  assert(writeCnt == writeBuffLen);
  // 执行指令
  do {
    int ret;

    ret = ftdi_write_data(&ftdiObj->ctx, writeBuff, writeBuffLen);
    if (ret != writeBuffLen) {
      log_error("FTDI write commands failed. error code:%d.", ret);
      free(writeBuff);
      if (readBuff) free(readBuff);
      return ADPT_ERR_INTERNAL_ERROR;
    }
    
    if (readBuffLen > 0) {
      assert(readBuff != NULL);
      // waiting latency milliseconds
      msleep(ftdiObj->latency);
      ret = ftdi_read_data(&ftdiObj->ctx, readBuff, readBuffLen);
      if (ret != readBuffLen) {
        log_error("FTDI read response failed. error code:%d.", ret);
        free(writeBuff);
        free(readBuff);
        return ADPT_ERR_INTERNAL_ERROR;
      }
    }
  } while(0);
  
  // misc_PrintBulk(writeBuff, writeBuffLen, 32);
  // misc_PrintBulk(readBuff, readBuffLen, 32);

  // 第三次遍历：同步数据，并删除执行成功的指令
  list_for_each_entry_safe(cmd, cmd_t, &ftdiObj->JtagInsQueue, list_entry) {
    // 跳过状态机改变指令
    if (cmd->type == JTAG_INS_STATUS_MOVE || cmd->type == JTAG_INS_IDLE_WAIT) {
      goto FREE_CMD;
    }

    // 如果readbuffer为空，直接释放所有命令
    if (readBuffLen == 0) {
      goto FREE_CMD;
    }

    int bytesCnt = (cmd->instr.exchangeData.bitCount - 1) >> 3;
    int restBits = (cmd->instr.exchangeData.bitCount - 1) & 0x7;
    bytesCnt += restBits > 0 ? 1 : 0;
    memcpy(cmd->instr.exchangeData.data, readBuff + readCnt, bytesCnt);
    readCnt += bytesCnt;

    // 将最后一个字节的数据组合到前一个字节上
    // 倒数第二个字节是高位有效，从高位向低位shift过来，而最后一个字节是只有一个数据，所以
    // 在最后一位
    uint8_t last_byte = *(cmd->instr.exchangeData.data + bytesCnt - 1);
    last_byte = (last_byte >> (8-restBits)) | ((*(readBuff + readCnt) & 0x1) << restBits);
    *(cmd->instr.exchangeData.data + bytesCnt - 1) = last_byte;
    readCnt++;
  FREE_CMD:
    list_del(&cmd->list_entry);
    free(cmd);
  }
  // 更新当前TAP状态机
  INTERFACE_CONST_INIT(enum JTAG_TAP_State, ftdiObj->jtagSkillAPI.currState, tempState);

  // 释放资源
  free(writeBuff);
  free(readBuff);
  return ADPT_SUCCESS;
}

static int ftdiJtagCancel(IN JtagSkill self) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
}

/**
 * 创建新的FTDI仿真器对象
 */
Adapter CreateFtdi(void) {
  struct ftdi *obj = calloc(1, sizeof(struct ftdi));
  if (!obj) {
    log_error("CreateFtdi:Can not create object.");
    return NULL;
  }

  INIT_LIST_HEAD(&obj->JtagInsQueue);
  INIT_LIST_HEAD(&obj->adapterAPI.skills);

  // 设置接口参数
  obj->signature = SIGNATURE_32('F', 'T', 'D', 'I');
  obj->interface = -1;
  obj->latency = 5;
  obj->adapterAPI.SetStatus = ftdiHostStatus;
  obj->adapterAPI.SetFrequent = ftdiMpsseFreq;
  obj->adapterAPI.Reset = ftdiReset;
  obj->adapterAPI.SetTransferMode = ftdiSetTransMode;

  INIT_LIST_HEAD(&obj->jtagSkillAPI.header.skills);
  list_add(&obj->jtagSkillAPI.header.skills, &obj->adapterAPI.skills);

  obj->jtagSkillAPI.header.type = ADPT_SKILL_JTAG;
  obj->jtagSkillAPI.Pins = ftdiJtagPins;
  obj->jtagSkillAPI.ExchangeData = ftdiJtagExchangeData;
  obj->jtagSkillAPI.Idle = ftdiJtagIdle;
  obj->jtagSkillAPI.ToState = ftdiJtagToState;
  obj->jtagSkillAPI.Commit = ftdiJtagCommit;
  obj->jtagSkillAPI.Cancel = ftdiJtagCancel;

  obj->connected = FALSE;

  ftdi_init(&obj->ctx);

  log_trace("Create FTDI object: %p.", obj);
  return (Adapter)&obj->adapterAPI;
}

// 释放FTDI对象
void DestroyFtdi(Adapter *self) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(*self);

  ftdi_deinit(&ftdiObj->ctx);

  free(ftdiObj);
  *self = NULL;
}
