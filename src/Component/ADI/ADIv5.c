/**
 * src/Component/ADI/ADIv5.c
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

#include "Library/log/log.h"
#include "smartocd.h"
#include <stdlib.h>

#include "Component/ADI/ADIv5_private.h"

/**
 * DAP 初始化
 * 上电,初始化寄存器
 */
static int dapInit(struct ADIv5_Dap *dap) {
  assert(dap != NULL);
#if 0
  // 调试部分复位
  dap->skillObj->Reset(dap->skillObj, ADPT_RESET_DEBUG);
  if (dap->skillObj->currTransMode == ADPT_MODE_SWD) {
    uint32_t dpidr = 0;
    // SWD模式下第一个读取的寄存器必须要是DPIDR，这样才能读取其他的寄存器
    // 否则其他寄存器无法读取
    dap->skillObj->DapSingleRead(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_DPIDR, &dpidr);
    // 清空STICKER ERROR 信息
    dap->skillObj->DapSingleWrite(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_ABORT, 0x1e);
    if (dap->skillObj->DapCommit(dap->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      dap->skillObj->DapCleanPending(dap->skillObj);
      log_error("Init DAP failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
    log_info("DAP DPIDR:0x%08X.", dpidr);
  } else if (dap->skillObj->currTransMode == ADPT_MODE_JTAG) {
    dap->skillObj->JtagToState(dap->skillObj, JTAG_TAP_IDLE);
    if (dap->skillObj->JtagCommit(dap->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      dap->skillObj->DapCleanPending(dap->skillObj);
      log_error("Change TAP to IDLE state failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
  } else {
    log_error("Unknow transfer type!");
    return ADI_FAILED;
  }
#endif

  uint32_t ctrl_stat = 0;
  // 清零SELECT寄存器
  dap->skillObj->DapSingleWrite(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, 0);
  // 写0x20到CTRL，并读取
  dap->skillObj->DapSingleWrite(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_CTRL_STAT, 0x20);
  // dap->skillObj->DapSingleRead(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_CTRL_STAT, &ctrl_stat);
  // 写上电请求
  dap->skillObj->DapSingleWrite(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_CTRL_STAT, DP_CTRL_CSYSPWRUPREQ | DP_CTRL_CDBGPWRUPREQ);
  if (dap->skillObj->DapCommit(dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    dap->skillObj->DapCleanPending(dap->skillObj);
    log_error("Init DAP register failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  do {
    dap->skillObj->DapSingleRead(dap->skillObj, SKILL_DAP_DP_REG, DP_REG_CTRL_STAT, &ctrl_stat);
    if (dap->skillObj->DapCommit(dap->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      dap->skillObj->DapCleanPending(dap->skillObj);
      log_error("Read DP CTRL/STAT register failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
  } while ((ctrl_stat & (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK)) != (DP_STAT_CDBGPWRUPACK | DP_STAT_CSYSPWRUPACK));
  log_debug("DAP Power up. CTRL_STAT:0x%08X.", ctrl_stat);
  return ADI_SUCCESS;
}

/**
 * apRead8 读8位数据
 */
static int apRead8(AccessPort self, uint64_t addr, uint8_t *data) {
  assert(self != NULL);
  assert(data != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap CSW 寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  if (!ap->type.memory.config.lessWordTransfers) {
    log_warn("Couldn't support Less Word Transfers.");
    return ADI_ERR_UNSUPPORT;
  }
  cswTmp.regInfo.Size = AP_CSW_SIZE8; // Byte
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  uint32_t data_tmp = 0;
  // 读DRW寄存器
  ap->dap->skillObj->DapSingleRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, &data_tmp);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 根据byte lane获得数据
  *data = (data_tmp >> ((addr & 3) << 3)) & 0xff;
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apRead16 读16位数据
 */
static int apRead16(AccessPort self, uint64_t addr, uint16_t *data) {
  assert(self != NULL);
  assert(data != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 检查对齐
  if (addr & 0x1) {
    log_warn("Memory address is not half word aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap CSW 寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  if (!ap->type.memory.config.lessWordTransfers) {
    log_warn("Couldn't support Less Word Transfers.");
    return ADI_ERR_UNSUPPORT;
  }
  cswTmp.regInfo.Size = AP_CSW_SIZE16; // Half Word
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  uint32_t data_tmp = 0;
  // 读DRW寄存器
  ap->dap->skillObj->DapSingleRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, &data_tmp);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 根据byte lane获得数据
  *data = (data_tmp >> ((addr & 3) << 3)) & 0xffff;
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apRead32 读32位数据
 */
static int apRead32(AccessPort self, uint64_t addr, uint32_t *data) {
  assert(self != NULL);
  assert(data != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 检查对齐
  if (addr & 0x3) {
    log_warn("Memory address is not word aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  cswTmp.regInfo.Size = AP_CSW_SIZE32;      // Word
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  // 读DRW寄存器, 根据byte lane获得数据
  ap->dap->skillObj->DapSingleRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apRead64 读64位数据
 */
static int apRead64(AccessPort self, uint64_t addr, uint64_t *data) {
  assert(self != NULL);
  assert(data != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 检查对齐
  if (addr & 0x7) {
    log_warn("Memory address is not double word aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  if (!ap->type.memory.config.largeData) {
    log_warn("Couldn't support Large Word Transfers.");
    return ADI_ERR_UNSUPPORT;
  }
  cswTmp.regInfo.Size = AP_CSW_SIZE64; // Double Word
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  // 读DRW寄存器
  uint32_t data_tmp[2]; // 定义缓冲区
  // 读取数据，第一次读取的是低位，接下来读取高位，必须两次读取后才能完成本次AP Memory access
  ap->dap->skillObj->DapSingleRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data_tmp);
  ap->dap->skillObj->DapSingleRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data_tmp + 1);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  *data = ((uint64_t)data_tmp[1] << 32) | data_tmp[0];
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apWrite8 写8位数据
 */
static int apWrite8(AccessPort self, uint64_t addr, uint8_t data) {
  assert(self != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  if (!ap->type.memory.config.lessWordTransfers) {
    log_warn("Couldn't support Less Word Transfers.");
    return ADI_ERR_UNSUPPORT;
  }
  cswTmp.regInfo.Size = AP_CSW_SIZE8; // Byte
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  // 写DRW寄存器
  uint32_t data_tmp = data << ((addr & 3) << 3); // 放到Byte Lane确定的位置
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data_tmp);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apWrite16 写16位数据
 */
static int apWrite16(AccessPort self, uint64_t addr, uint16_t data) {
  assert(self != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 检查对齐
  if (addr & 0x1) {
    log_warn("Memory address is not half word aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  if (!ap->type.memory.config.lessWordTransfers) {
    log_warn("Couldn't support Less Word Transfers.");
    return ADI_ERR_UNSUPPORT;
  }
  cswTmp.regInfo.Size = AP_CSW_SIZE16; // Half Word
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  // 写DRW寄存器
  uint32_t data_tmp = data << ((addr & 3) << 3); // 放到Byte Lane确定的位置
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data_tmp);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apWrite32 写32位数据
 */
static int apWrite32(AccessPort self, uint64_t addr, uint32_t data) {
  assert(self != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 检查对齐
  if (addr & 0x3) {
    log_warn("Memory address is not word aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  cswTmp.regInfo.Size = AP_CSW_SIZE32;      // Word
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  // 写DRW寄存器
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * apWrite64 写64位数据
 */
static int apWrite64(AccessPort self, uint64_t addr, uint64_t data) {
  assert(self != NULL);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 检查对齐
  if (addr & 0x7) {
    log_warn("Memory address is not double word aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 设置CSW：Size=Word，AddrInc=off
  cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC; // AddrInc Off
  if (!ap->type.memory.config.largeData) {
    log_warn("Couldn't support Large Word Transfers.");
    return ADI_ERR_UNSUPPORT;
  }
  cswTmp.regInfo.Size = AP_CSW_SIZE64; // Double Word
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }
  // 写入TAR
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
  if (ap->type.memory.config.largeAddress) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
  }
  // 写DRW寄存器，先写低位，再写高位，最后一个高位写完后才初始化Memory access
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, data & 0xFFFFFFFFu);
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, (data >> 32) & 0xFFFFFFFFu);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * Block读
 */
static int apBlockRead(AccessPort self, uint64_t addr, enum addrIncreaseMode mode,
                       enum dataSize size, unsigned int count, uint8_t *data) {
  assert(self != NULL && data != NULL);
  uint32_t *data_out = CAST(uint32_t *, data);
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // some checks
  switch (size) {
  case DataSize_8:
    // 检查是否支持less word Transfer
    if (ap->type.memory.config.lessWordTransfers) { // 支持lessWordTransfer
      cswTmp.regInfo.Size = AP_CSW_SIZE8;           // Half Word
    } else {
      log_warn("Couldn't support less word transfers.");
      return ADI_ERR_UNSUPPORT;
    }
    break;
  case DataSize_16:
    // 检查是否对齐
    if (addr & 0x1) {
      log_warn("Memory address is not half word aligned!");
      return ADI_ERR_BAD_PARAMETER;
    }
    // 检查是否支持less word Transfer
    if (ap->type.memory.config.lessWordTransfers) { // 支持lessWordTransfer
      cswTmp.regInfo.Size = AP_CSW_SIZE16;          // Half Word
    } else {
      log_warn("Couldn't support less word transfers.");
      return ADI_ERR_UNSUPPORT;
    }
    break;
  case DataSize_32:
    if (addr & 0x3) {
      log_warn("Memory address is not word aligned!");
      return ADI_ERR_BAD_PARAMETER;
    }
    cswTmp.regInfo.Size = AP_CSW_SIZE32; // Word
    break;
  case DataSize_64:
  case DataSize_128:
  case DataSize_256:
  default:
    log_warn("Specified data size is not support.");
    return ADI_ERR_UNSUPPORT;
  }
  // 地址自增模式
  switch (mode) {
  case AddrInc_Off:
    cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC;
    break;
  case AddrInc_Single:
    cswTmp.regInfo.AddrInc = AP_CSW_SADDRINC;
    break;
  case AddrInc_Packed:
    cswTmp.regInfo.AddrInc = AP_CSW_PADDRINC;
    break;
  default:
    log_warn("Specified address increase mode is not support.");
    return ADI_ERR_UNSUPPORT;
  }
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }

  // 处理地址自增模式下超过1kb边界的情况，超过1kb的边界了需要拆分，每次地址自增控制在1kb以内
  uint64_t addrCurr = addr, addrEnd;          // 当前地址，结束地址
  uint64_t addrNextBoundary;                  // 地址的下一个1kb边界
  unsigned int thisTimeTransCnt, dataPos = 0; // 指向data_out的偏移

  if (cswTmp.regInfo.AddrInc == AP_CSW_SADDRINC) {
    // 每次写DRW，发起一次memory access，之后自增TAR
    addrEnd = addr + (count << cswTmp.regInfo.Size); 
    while (addrCurr < addrEnd) {
      addrNextBoundary = ((addrCurr >> 10) + 1) << 10; // 找到下一个1kb边界
      // 写入TAR
      ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addrCurr & 0xFFFFFFFFu);
      if (ap->type.memory.config.largeAddress) {
        ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addrCurr >> 32) & 0xFFFFFFFFu);
      }
      // log_debug("SINGLE:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
      // 如果下一个边界大于结束地址
      if (addrNextBoundary > addrEnd) {
        thisTimeTransCnt = (addrEnd - addrCurr) >> cswTmp.regInfo.Size;
        addrCurr = addrEnd;
      } else {
        thisTimeTransCnt = (addrNextBoundary - addrCurr) >> cswTmp.regInfo.Size;
        addrCurr = addrNextBoundary;
      }
      // log_debug("SINGLE:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
      // 读取block
      ap->dap->skillObj->DapMultiRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, thisTimeTransCnt, data_out + dataPos);
      dataPos += thisTimeTransCnt;
    }
  } else if (cswTmp.regInfo.AddrInc == AP_CSW_PADDRINC) {
    addrEnd = addr + (count << 2); // 每次写DRW，发起多次memory access，每次Memory access成功后自增TAR
    while (addrCurr < addrEnd) {
      addrNextBoundary = ((addrCurr >> 10) + 1) << 10; // 找到下一个1kb边界
      // 写入TAR
      ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addrCurr & 0xFFFFFFFFu);
      if (ap->type.memory.config.largeAddress) {
        ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addrCurr >> 32) & 0xFFFFFFFFu);
      }
      // log_debug("PACKED:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
      // 如果下一个边界大于结束地址
      if (addrNextBoundary > addrEnd) {
        thisTimeTransCnt = (addrEnd - addrCurr) >> 2;
        addrCurr = addrEnd;
      } else {
        thisTimeTransCnt = (addrNextBoundary - addrCurr) >> 2;
        addrCurr = addrNextBoundary;
      }
      // log_debug("PACKED:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
      // 读取block
      ap->dap->skillObj->DapMultiRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, thisTimeTransCnt, data_out + dataPos);
      dataPos += thisTimeTransCnt;
    }
  } else { // 地址不增 XXX 没测试
    // 写入TAR
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
    if (ap->type.memory.config.largeAddress) {
      ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
    }
    ap->dap->skillObj->DapMultiRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, count, data_out);
  }
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * Block写
 */
static int apBlockWrite(AccessPort self, uint64_t addr, enum addrIncreaseMode mode,
                        enum dataSize size, unsigned int count, uint8_t *data) {
  assert(self != NULL && data != NULL);
  uint32_t *data_out = CAST(uint32_t *, data);
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  ADIv5_DpSelectRegister selectTmp;
  ADIv5_ApCswRegister cswTmp;
  // 检查AP类型
  if (self->type != AccessPort_Memory) {
    log_error("Not a memory access port!");
    return ADI_ERR_BAD_PARAMETER;
  }
  // 初始化本地临时变量
  selectTmp.regData = ap->dap->select.regData;
  cswTmp.regData = ap->type.memory.csw.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // some checks
  switch (size) {
  case DataSize_8:
    // 检查是否支持less word Transfer
    if (ap->type.memory.config.lessWordTransfers) { // 支持lessWordTransfer
      cswTmp.regInfo.Size = AP_CSW_SIZE8;           // Half Word
    } else {
      log_warn("Couldn't support less word transfers.");
      return ADI_ERR_UNSUPPORT;
    }
    break;
  case DataSize_16:
    // 检查是否对齐
    if (addr & 0x1) {
      log_warn("Memory address is not half word aligned!");
      return ADI_ERR_BAD_PARAMETER;
    }
    // 检查是否支持less word Transfer
    if (ap->type.memory.config.lessWordTransfers) { // 支持lessWordTransfer
      cswTmp.regInfo.Size = AP_CSW_SIZE16;          // Half Word
    } else {
      log_warn("Couldn't support less word transfers.");
      return ADI_ERR_UNSUPPORT;
    }
    break;
  case DataSize_32:
    if (addr & 0x3) {
      log_warn("Memory address is not word aligned!");
      return ADI_ERR_BAD_PARAMETER;
    }
    cswTmp.regInfo.Size = AP_CSW_SIZE32; // Word
    break;
  case DataSize_64:
  case DataSize_128:
  case DataSize_256:
  default:
    log_warn("Specified data size is not support.");
    return ADI_ERR_UNSUPPORT;
  }
  // 地址自增模式
  switch (mode) {
  case AddrInc_Off:
    cswTmp.regInfo.AddrInc = AP_CSW_NADDRINC;
    break;
  case AddrInc_Single:
    cswTmp.regInfo.AddrInc = AP_CSW_SADDRINC;
    break;
  case AddrInc_Packed:
    cswTmp.regInfo.AddrInc = AP_CSW_PADDRINC;
    break;
  default:
    log_warn("Specified address increase mode is not support.");
    return ADI_ERR_UNSUPPORT;
  }
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 是否需要更新CSW寄存器？
  if (ap->type.memory.csw.regData != cswTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, cswTmp.regData);
  }

  // 处理地址自增模式下超过1kb边界的情况，超过1kb的边界了需要拆分，每次地址自增控制在1kb以内
  uint64_t addrCurr = addr, addrEnd;          // 当前地址，结束地址
  uint64_t addrNextBoundary;                  // 地址的下一个1kb边界
  unsigned int thisTimeTransCnt, dataPos = 0; // 指向data_out的偏移

  if (cswTmp.regInfo.AddrInc == AP_CSW_SADDRINC) {
    addrEnd = addr + (count << cswTmp.regInfo.Size); // 每次写DRW，发起一次memory access，之后自增TAR
    while (addrCurr < addrEnd) {
      addrNextBoundary = ((addrCurr >> 10) + 1) << 10; // 找到下一个1kb边界
      // 写入TAR
      ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addrCurr & 0xFFFFFFFFu);
      if (ap->type.memory.config.largeAddress) {
        ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addrCurr >> 32) & 0xFFFFFFFFu);
      }
      log_debug("SINGLE:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
      // 如果下一个边界大于结束地址
      if (addrNextBoundary > addrEnd) {
        thisTimeTransCnt = (addrEnd - addrCurr) >> cswTmp.regInfo.Size;
        addrCurr = addrEnd;
      } else {
        thisTimeTransCnt = (addrNextBoundary - addrCurr) >> cswTmp.regInfo.Size;
        addrCurr = addrNextBoundary;
      }
      // log_debug("SINGLE:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
      // 读取block
      ap->dap->skillObj->DapMultiWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, thisTimeTransCnt, data_out + dataPos);
      dataPos += thisTimeTransCnt;
    }
  } else if (cswTmp.regInfo.AddrInc == AP_CSW_PADDRINC) {
    addrEnd = addr + (count << 2); // 每次写DRW，发起多次memory access，每次Memory access成功后自增TAR
    while (addrCurr < addrEnd) {
      addrNextBoundary = ((addrCurr >> 10) + 1) << 10; // 找到下一个1kb边界
      // 写入TAR
      ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addrCurr & 0xFFFFFFFFu);
      if (ap->type.memory.config.largeAddress) {
        ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addrCurr >> 32) & 0xFFFFFFFFu);
      }
      // log_debug("PACKED:CurrAddr:0x%08X;next boundary:0x%08X.", addrCurr, addrNextBoundary);
      // 如果下一个边界大于结束地址
      if (addrNextBoundary > addrEnd) {
        thisTimeTransCnt = (addrEnd - addrCurr) >> 2;
        addrCurr = addrEnd;
      } else {
        thisTimeTransCnt = (addrNextBoundary - addrCurr) >> 2;
        addrCurr = addrNextBoundary;
      }
      // log_debug("PACKED:dataPos:%d;thisTimeTransCnt:%d.", dataPos, thisTimeTransCnt);
      // 读取block
      ap->dap->skillObj->DapMultiWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, thisTimeTransCnt, data_out + dataPos);
      dataPos += thisTimeTransCnt;
    }
  } else { // 地址不增 XXX 没有测试过这里的代码
    // 写入TAR
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_LSB, addr & 0xFFFFFFFFu);
    if (ap->type.memory.config.largeAddress) {
      ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_TAR_MSB, (addr >> 32) & 0xFFFFFFFFu);
    }
    ap->dap->skillObj->DapMultiWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_DRW, count, data_out);
  }
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = cswTmp.regData;
  return ADI_SUCCESS;
}

/**
 * 读CSW
 */
static int apReadCSW(AccessPort self, uint32_t *data) {
  assert(self != NULL && data != NULL);
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  ADIv5_DpSelectRegister selectTmp;
  selectTmp.regData = ap->dap->select.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 读CSW
  ap->dap->skillObj->DapSingleRead(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, &ap->type.memory.csw.regData);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  *data = ap->type.memory.csw.regData;
  return ADI_SUCCESS;
}

/**
 * 写CSW
 */
static int apWriteCSW(AccessPort self, uint32_t data) {
  assert(self != NULL);
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  ADIv5_DpSelectRegister selectTmp;
  selectTmp.regData = ap->dap->select.regData;
  // 选中当前ap
  selectTmp.regInfo.AP_Sel = ap->index;
  // 选中当前ap寄存器 bank
  selectTmp.regInfo.AP_BankSel = 0x0;
  // 是否需要更新SELECT寄存器?
  if (ap->dap->select.regData != selectTmp.regData) {
    ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, selectTmp.regData);
  }
  // 写CSW
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, data);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 指令执行成功，同步数据到DAP影子寄存器
  ap->dap->select.regData = selectTmp.regData;
  ap->type.memory.csw.regData = data;
  return ADI_SUCCESS;
}

/**
 * 终止本次传输
 */
static int apAbort(AccessPort self) {
  assert(self != NULL);
  struct ADIv5_AccessPort *ap = container_of(self, struct ADIv5_AccessPort, apApi);
  // 写DP Abort
  ap->dap->skillObj->DapSingleWrite(ap->dap->skillObj, SKILL_DAP_DP_REG, DP_REG_ABORT, 0x1);
  // 执行指令队列
  if (ap->dap->skillObj->DapCommit(ap->dap->skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
    log_error("Execute DAP command failed!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  return ADI_SUCCESS;
}

/**
 * fillApConfig 填充AP的配置信息:CSW,CFG
 * 此函数默认AP_BankSel=0xF
 */
static int fillApConfig(struct ADIv5_Dap *dapObj, struct ADIv5_AccessPort *ap) {
  assert(dapObj != NULL);
  assert(ap != NULL);
  if (ap->apApi.type == AccessPort_Memory) {
    uint32_t temp = 0, temp_2 = 0;
    // 读CFG寄存器,并初始化相关标志位
    dapObj->skillObj->DapSingleRead(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_CFG, &temp);
    // 读ROM_LSB寄存器
    dapObj->skillObj->DapSingleRead(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_ROM_LSB, &temp_2);
    if (dapObj->skillObj->DapCommit(dapObj->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
      log_error("Read AP register failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
    ap->type.memory.config.largeAddress = !!(temp & AP_CFG_LARGE_ADDRESS);
    ap->type.memory.config.largeData = !!(temp & AP_CFG_LARGE_DATA);
    ap->type.memory.config.bigEndian = !!(temp & AP_CFG_BIG_ENDIAN);
    // 读ROM_MSB寄存器
    if (ap->type.memory.config.largeAddress) {
      dapObj->skillObj->DapSingleRead(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_ROM_MSB, &temp);
      if (dapObj->skillObj->DapCommit(dapObj->skillObj) != ADPT_SUCCESS) {
        // 清理指令队列
        ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
        log_error("Read AP register failed!");
        return ADI_ERR_INTERNAL_ERROR;
      }
      ap->type.memory.rom = ((uint64_t)temp << 32) | temp_2;
    } else {
      ap->type.memory.rom = temp_2;
    }

    // 读CSW寄存器
    dapObj->select.regInfo.AP_BankSel = 0x0;
    dapObj->skillObj->DapSingleWrite(dapObj->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, dapObj->select.regData);
    dapObj->skillObj->DapSingleRead(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, &ap->type.memory.csw.regData);
    if (dapObj->skillObj->DapCommit(dapObj->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
      log_error("Read/Write AP register failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
    // 判断DeviceEn位
    if ((ap->type.memory.csw.regData & AP_CSW_DEVENABLE) == 0) {
      log_warn("This AP is not enabled.");
      return ADI_FAILED;
    }

    temp = ap->type.memory.csw.regData; // 备份CSW寄存器的初始值
    // 测试Packed和Less word transfer
    ap->type.memory.csw.regInfo.AddrInc = AP_CSW_PADDRINC;
    ap->type.memory.csw.regInfo.Size = AP_CSW_SIZE8;
    dapObj->skillObj->DapSingleWrite(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, ap->type.memory.csw.regData); // 写
    dapObj->skillObj->DapSingleRead(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, &ap->type.memory.csw.regData); // 读
    if (dapObj->skillObj->DapCommit(dapObj->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
      log_error("Read/Write AP register failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
    /**
     * ARM ID080813
     * 第7-133
     * An implementation that supports transfers smaller than word must support packed transfers.
     * Packed transfers cannot be supported on a MEM-AP that only supports whole-word transfers.
     * 第7-136
     * If the MEM-AP Large Data Extention is not supported, then when a MEM-AP implementation
     * supports different sized access, it MUST support word, halfword and byte accesses.
     */
    if (ap->type.memory.csw.regInfo.AddrInc == AP_CSW_PADDRINC) {
      ap->type.memory.config.packedTransfers = 1;
      ap->type.memory.config.lessWordTransfers = 1;
    } else {
      ap->type.memory.config.lessWordTransfers = ap->type.memory.csw.regInfo.Size == AP_CSW_SIZE8 ? 1 : 0;
    }

    ap->type.memory.csw.regData = temp;                                                                         // 恢复CSW记录的数据
    dapObj->skillObj->DapSingleWrite(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_CSW, ap->type.memory.csw.regData); // 写
    if (dapObj->skillObj->DapCommit(dapObj->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
      log_error("Read/Write AP register failed!");
      return ADI_ERR_INTERNAL_ERROR;
    }
    return ADI_SUCCESS;
  } else {
    // TODO 增加JTAG相关的初始化
    return ADI_ERR_UNSUPPORT;
  }
}

/**
 * checkApType 检查AP类型是否匹配
 */
static int checkApType(ADIv5_ApIdrRegister idr, enum AccessPortType type, enum busType bus) {
  // 检查厂商
  if (idr.regInfo.JEP106Code != JEP106_CODE_ARM)
    return ADI_FAILED;
  if (type == AccessPort_JTAG && idr.regInfo.Class == 0) { // JTAG-AP
    return ADI_SUCCESS;
  } else if (type == AccessPort_Memory && idr.regInfo.Class == 0x8) { // 选择MEM-AP
    if (idr.regInfo.Type == bus) {                                    // 检查总线类型
      return ADI_SUCCESS;
    }
  }
  return ADI_FAILED;
}

/**
 * findAP
 */
static int findAP(DAP self, enum AccessPortType type, enum busType bus, AccessPort *apOut) {
  assert(self != NULL);
  struct ADIv5_AccessPort *ap, *ap_t = NULL;
  struct ADIv5_Dap *dapObj = container_of(self, struct ADIv5_Dap, dapApi);
  // 先在链表中搜索,如果没有,则尝试创建新的AP,插入链表
  list_for_each_entry(ap, &dapObj->apList, list_entry) {
    if (checkApType(ap->idr, type, bus) == ADI_SUCCESS) {
      *apOut = &ap->apApi;
      return ADI_SUCCESS;
    }
  }
  // 新建AccessPort对象
  ap_t = calloc(1, sizeof(struct ADIv5_AccessPort));
  if (ap_t == NULL) {
    log_error("Failed to create AccessPort object!");
    return ADI_ERR_INTERNAL_ERROR;
  }
  // 设置接口类型
  INTERFACE_CONST_INIT(enum AccessPortType, ap_t->apApi.type, type);
  ap_t->dap = dapObj;
  switch (type) {
  case AccessPort_Memory:
    ap_t->apApi.Interface.Memory.ReadCSW = apReadCSW;
    ap_t->apApi.Interface.Memory.WriteCSW = apWriteCSW;
    ap_t->apApi.Interface.Memory.Abort = apAbort;

    ap_t->apApi.Interface.Memory.Read8 = apRead8;
    ap_t->apApi.Interface.Memory.Read16 = apRead16;
    ap_t->apApi.Interface.Memory.Read32 = apRead32;
    ap_t->apApi.Interface.Memory.Read64 = apRead64;
    ap_t->apApi.Interface.Memory.BlockRead = apBlockRead;

    ap_t->apApi.Interface.Memory.Write8 = apWrite8;
    ap_t->apApi.Interface.Memory.Write16 = apWrite16;
    ap_t->apApi.Interface.Memory.Write32 = apWrite32;
    ap_t->apApi.Interface.Memory.Write64 = apWrite64;
    ap_t->apApi.Interface.Memory.BlockWrite = apBlockWrite;
    break;
  case AccessPort_JTAG:
    // TODO 设置接口
    break;
  default:
    log_error("Unknow AP type!");
    free(ap_t);
    return ADI_ERR_BAD_PARAMETER;
  }

  // 搜索AP
  dapObj->select.regInfo.AP_BankSel = 0xF; // IDR寄存器的Bank
  for (ap_t->index = 0; ap_t->index < 256; ap_t->index++) {
    // 写SELECT
    dapObj->select.regInfo.AP_Sel = ap_t->index;
    dapObj->skillObj->DapSingleWrite(dapObj->skillObj, SKILL_DAP_DP_REG, DP_REG_SELECT, dapObj->select.regData);
    // 读 APIDR
    dapObj->skillObj->DapSingleRead(dapObj->skillObj, SKILL_DAP_AP_REG, AP_REG_IDR, &ap_t->idr.regData);
    if (dapObj->skillObj->DapCommit(dapObj->skillObj) != ADPT_SUCCESS) {
      // 清理指令队列
      ap->dap->skillObj->DapCleanPending(ap->dap->skillObj);
      log_error("Read AP IDR register failed!");
      free(ap_t);
      return ADI_ERR_INTERNAL_ERROR;
    }
    // 检查AP是否存在
    if (ap_t->idr.regData == 0) {
      log_warn("Arrive at the end of the AP list!");
      free(ap_t);
      return ADI_FAILED;
    }
    log_debug("AP[%d] IDR: 0x%08X.", ap_t->index, ap_t->idr.regData);
    // 检查AP类型
    if (checkApType(ap_t->idr, type, bus) != ADI_SUCCESS) {
      continue;
    }
    // 填充AccessPort对象数据
    if (fillApConfig(dapObj, ap_t) == ADI_SUCCESS) {
      // 初始化接口的ROM Table常量
      INTERFACE_CONST_INIT(uint64_t, ap_t->apApi.Interface.Memory.RomTableBase, ap_t->type.memory.rom);
      // 插入AccessPort链表
      list_add_tail(&ap_t->list_entry, &dapObj->apList);
      *apOut = (AccessPort)&ap_t->apApi;
      return ADI_SUCCESS;
    } else {
      free(ap_t);
      return ADI_ERR_INTERNAL_ERROR;
    }
  }
  free(ap_t);
  return ADI_FAILED;
}

// 创建DAP对象
DAP ADIv5_CreateDap(DapSkill skillObj) {
  assert(skillObj != NULL);
  struct ADIv5_Dap *dap = calloc(1, sizeof(struct ADIv5_Dap));
  if (!dap) {
    log_error("Failed to create DAP object!");
    return NULL;
  }
  INIT_LIST_HEAD(&dap->apList);
  dap->skillObj = skillObj;
  dap->dapApi.FindAccessPort = findAP;
  // 初始化DAP对象
  if (dapInit(dap) != ADI_SUCCESS) {
    free(dap);
    return NULL;
  }
  // 返回接口
  return (DAP)&dap->dapApi;
}

// DestoryDap 销毁Dap对象
void ADIv5_DestoryDap(DAP *self) {
  assert(self != NULL);
  assert(*self != NULL);
  struct ADIv5_Dap *dapObj = container_of(*self, struct ADIv5_Dap, dapApi);
  struct ADIv5_AccessPort *ap, *ap_t;
  // 释放链表
  list_for_each_entry_safe(ap, ap_t, &dapObj->apList, list_entry) {
    list_del(&ap->list_entry); // 将链表中删除
    free(ap);
  }
  free(dapObj);
  *self = NULL;
}

/**
 * 读取Component ID和Peripheral ID
 */
int ADIv5_ReadCidPid(AccessPort self, uint64_t componentBase, uint32_t *cid, uint64_t *pid) {
  assert(self != NULL && cid != NULL && pid != NULL);
  jmp_buf exception;
  if ((componentBase & 0xFFF) != 0) {
    log_warn("Component base address is not 4KB aligned!");
    return ADI_ERR_BAD_PARAMETER;
  }

  *cid = 0;
  *pid = 0;
  uint32_t cid0, cid1, cid2, cid3;
  uint32_t pid0, pid1, pid2, pid3, pid4; // XXX pid5-7全是0，所以不用读
  // 错误处理
  switch (setjmp(exception)) {
  case 1:
    log_error("DAP_ReadMem32:Read Component ID Failed!");
    return ADI_FAILED;
  case 2:
    log_error("DAP_ReadMem32:Read Peripheral ID Failed!");
    return ADI_FAILED;
  case 3:
    log_error("DAP_Execute:Failed!");
    return ADI_FAILED;
  default:
    log_error("Unknow Error.");
    return ADI_FAILED;
  case 0:
    break;
  }
  // 读取Component ID
  if (apRead32(self, componentBase + 0xFF0, &cid0) != ADI_SUCCESS)
    longjmp(exception, 1);
  if (apRead32(self, componentBase + 0xFF4, &cid1) != ADI_SUCCESS)
    longjmp(exception, 1);
  if (apRead32(self, componentBase + 0xFF8, &cid2) != ADI_SUCCESS)
    longjmp(exception, 1);
  if (apRead32(self, componentBase + 0xFFC, &cid3) != ADI_SUCCESS)
    longjmp(exception, 1);
  // 读取Peripheral ID
  if (apRead32(self, componentBase + 0xFE0, &pid0) != ADI_SUCCESS)
    longjmp(exception, 2);
  if (apRead32(self, componentBase + 0xFE4, &pid1) != ADI_SUCCESS)
    longjmp(exception, 2);
  if (apRead32(self, componentBase + 0xFE8, &pid2) != ADI_SUCCESS)
    longjmp(exception, 2);
  if (apRead32(self, componentBase + 0xFEC, &pid3) != ADI_SUCCESS)
    longjmp(exception, 2);
  if (apRead32(self, componentBase + 0xFD0, &pid4) != ADI_SUCCESS)
    longjmp(exception, 2);

  *cid = (cid3 & 0xff) << 24 | (cid2 & 0xff) << 16 | (cid1 & 0xff) << 8 | (cid0 & 0xff);
  *pid = (uint64_t)(pid4 & 0xff) << 32 | (pid3 & 0xff) << 24 | (pid2 & 0xff) << 16 |
         (pid1 & 0xff) << 8 | (pid0 & 0xff);
  return ADI_SUCCESS;
}
