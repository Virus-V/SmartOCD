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
#include "Library/log/log.h"

#include <ftdi.h>

struct ftdi {
  uint32_t signature;
  struct ftdi_context ctx;   // FTDI库相关对象
  struct adapter adapterAPI; // Adapter接口对象
  BOOL connected;            // 设备是否已连接
  int interface;             // 当前选择的FTDI channel/interface

  struct jtagSkill jtagSkillAPI; // jtag能力集接口
  struct list_head JtagInsQueue; // JTAG指令队列，元素类型：struct JTAG_Command
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
                IN const char *serialNum) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);

  int idx = 0;
  //如果当前没有连接,则连接CMSIS-DAP设备
  if (ftdiObj->connected != TRUE) {
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
static int ftdiSetTransMode(IN Adapter self, IN enum transferMode mode) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(self);

  return ADPT_ERR_UNSUPPORT;
}

static int ftdiJtagPins(IN JtagSkill self, IN uint8_t pinMask, IN uint8_t pinDataOut,
                        OUT uint8_t *pinDataIn, IN unsigned int pinWait) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
}

static int ftdiJtagExchangeData(IN JtagSkill self, IN uint8_t *data, IN unsigned int bitCount) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
}

static int ftdiJtagIdle(IN JtagSkill self, IN unsigned int clkCount) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
}

static int ftdiJtagToState(IN JtagSkill self, IN enum JTAG_TAP_State toState) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
}

static int ftdiJtagCommit(IN JtagSkill self) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_JTAG_SKILL(self);
  return ADPT_ERR_UNSUPPORT;
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
  obj->adapterAPI.SetStatus = ftdiHostStatus;
  obj->adapterAPI.SetFrequent = ftdiMpsseFreq;
  obj->adapterAPI.Reset = ftdiReset;
  obj->adapterAPI.SetTransferMode = ftdiSetTransMode;

  INIT_LIST_HEAD(&obj->jtagSkillAPI.header.skills);
  list_add(&obj->jtagSkillAPI.header.skills, &obj->adapterAPI.skills);

  obj->jtagSkillAPI.header.type = ADPT_SKILL_JTAG;
  obj->jtagSkillAPI.JtagPins = ftdiJtagPins;
  obj->jtagSkillAPI.JtagExchangeData = ftdiJtagExchangeData;
  obj->jtagSkillAPI.JtagIdle = ftdiJtagIdle;
  obj->jtagSkillAPI.JtagToState = ftdiJtagToState;
  obj->jtagSkillAPI.JtagCommit = ftdiJtagCommit;
  obj->jtagSkillAPI.JtagCancel = ftdiJtagCancel;

  obj->connected = FALSE;

  ftdi_init(&obj->ctx);

  return (Adapter)&obj->adapterAPI;
}

// 释放FTDI对象
void DestroyFtdi(Adapter *self) {
  struct ftdi *ftdiObj = FTDI_OBJ_FORM_ADAPTER(*self);

  ftdi_deinit(&ftdiObj->ctx);

  free(ftdiObj);
  *self = NULL;
}
