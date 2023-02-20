/**
 * Copyright (c) 2023, Virus.V <virusv@live.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of SmartOCD nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Copyright 2023 Virus.V <virusv@live.com>
 */

#include <stdio.h>
#include <unistd.h>
#include "ctest.h"

#include "Adapter/ftdi/ftdi.h"
#include "Library/jtag/jtag.h"
#include "Library/log/log.h"

#include <libftdi1/ftdi.h>

uint16_t vids[] = {0x0403, 0};
uint16_t pids[] = {0x6010, 0};

CTEST_DATA(ftdi) {
    Adapter ftdiObj;
};

CTEST_SETUP(ftdi) {
  data->ftdiObj = CreateFtdi();
  ASSERT_NOT_NULL(data->ftdiObj);
}

CTEST_TEARDOWN(ftdi) {
  DestroyFtdi(&data->ftdiObj);
  ASSERT_NULL(data->ftdiObj);
}

// 连接断开测试
CTEST2(ftdi, connect_test) {
  int ret, i;

  for(i =0; i<5; i++) {
    ret = ConnectFtdi(data->ftdiObj, vids, pids, NULL, 2);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    ret = DisconnectFtdi(data->ftdiObj);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);
  }
}

// 指令队列测试
CTEST2(ftdi, jtag_queue_test) {
  int ret, i;
  JtagSkill jtagObj;
  uint32_t idcode_ir = 0, dtmcs_ir = 0;
  uint32_t idcode = 0, dtmcs = 0;
  uint8_t test_data[65536];
  memset(test_data, 0xa5, sizeof(test_data));

  // 连接channel B
  ret = ConnectFtdi(data->ftdiObj, vids, pids, NULL, 2);
  ASSERT_EQUAL(ADPT_SUCCESS, ret);
  jtagObj = (JtagSkill)Adapter_GetSkill(data->ftdiObj, ADPT_SKILL_JTAG);
  ASSERT_NOT_NULL(jtagObj);

  for (i = 0; i < 5000; i++) {
    // 状态更新到IRSHIFT，更新IR
    ret = jtagObj->JtagToState(jtagObj, JTAG_TAP_IRSHIFT);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    idcode_ir = 0x01;  // IDCODE
    ret = jtagObj->JtagExchangeData(jtagObj, (uint8_t *)&idcode_ir, 5);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    // 状态到DRSHIFT，读取IDCODE
    ret = jtagObj->JtagToState(jtagObj, JTAG_TAP_DRSHIFT);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    ret = jtagObj->JtagExchangeData(jtagObj, (uint8_t *)&idcode, 32);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    // 状态更新到IRSHIFT，更新IR
    ret = jtagObj->JtagToState(jtagObj, JTAG_TAP_IRSHIFT);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    dtmcs_ir = 0x10;  // dtmcs
    ret = jtagObj->JtagExchangeData(jtagObj, (uint8_t *)&dtmcs_ir, 5);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    // 状态到DRSHIFT，读取dtmcs
    ret = jtagObj->JtagToState(jtagObj, JTAG_TAP_DRSHIFT);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    ret = jtagObj->JtagExchangeData(jtagObj, (uint8_t *)&dtmcs, 32);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

#if 1
    ret = jtagObj->JtagToState(jtagObj, JTAG_TAP_IDLE);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    ret = jtagObj->JtagIdle(jtagObj, 64);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);
#endif

    ret = jtagObj->JtagCommit(jtagObj);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);
    log_info("idcode:%x, dtmcs:%x, idcode_ir:%x, dtmcs_ir:%x.", idcode, dtmcs, idcode_ir, dtmcs_ir);
    ASSERT_EQUAL(0x20000c05, idcode);
  }
  ret = DisconnectFtdi(data->ftdiObj);
  ASSERT_EQUAL(ADPT_SUCCESS, ret);
}
// 裸ftdi库接口测试
CTEST_DATA(ftdi_playground) {
    struct ftdi_context ctx;
};

// 这个值是开始收数据后，在ftdi芯片内部缓存5ms之后在发到usb总线，这样可以
// 降低usb 总线的负载，这个时间段内积累数据。
#define TEST_LATENCY 5
CTEST_SETUP(ftdi_playground) {
  unsigned char latency = TEST_LATENCY;
  int ret;
  unsigned char wbuf[32] = {0};

  ret = ftdi_init(&data->ctx);
  ASSERT_EQUAL(0, ret);

  ret = ftdi_set_interface(&data->ctx, INTERFACE_B);
  ASSERT_EQUAL(0, ret);

  ret = ftdi_usb_open_desc(&data->ctx, 0x0403, 0x6010, NULL, NULL);
  ASSERT_EQUAL(0, ret);
  ftdi_usb_reset(&data->ctx);

  ret = ftdi_set_latency_timer(&data->ctx, latency);
  ASSERT_EQUAL(0, ret);

  ret = ftdi_get_latency_timer(&data->ctx, &latency);
  ASSERT_EQUAL(0, ret);
  ASSERT_EQUAL(TEST_LATENCY, latency);

  ret = ftdi_set_bitmode(&data->ctx, 0x0b, BITMODE_MPSSE);
  ASSERT_EQUAL(0, ret);

	// 设置分频
	wbuf[0] = TCK_DIVISOR;
  wbuf[1] = 0x02;
  wbuf[2] = 0x00;
  ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // 结束LOOPBACK
  wbuf[0] = LOOPBACK_END;
  ret = ftdi_write_data(&data->ctx, wbuf, 1);
  ASSERT_EQUAL(1, ret);

  wbuf[0] = SET_BITS_LOW;
  wbuf[1] = 0x08; // TCK/SK, TDI/DU low, TMS/CS high
  wbuf[2] = 0x0b; // TCK/SK, TDI/DU, TMS/CS output, TDO/D1 and GPIO L1 -> L3 input
  ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);
}

CTEST_TEARDOWN(ftdi_playground) {
  ftdi_usb_close(&data->ctx);
  ftdi_deinit(&data->ctx);
}
// 读取ID CODE寄存器
CTEST2(ftdi_playground, idcode_test) {
  int ret;
  unsigned char wbuf[32] = {0};
  unsigned char rbuf[32] = {0};

  //ret = ftdi_tcioflush(&data->ctx);
  ret = ftdi_usb_purge_buffers(&data->ctx);
  ASSERT_EQUAL(0, ret);

  // bad command test
  wbuf[0] = 0xAA;
  ret = ftdi_write_data(&data->ctx, wbuf, 1);
  ASSERT_EQUAL(1, ret);

  ret = ftdi_read_data(&data->ctx, rbuf, 2);
  ASSERT_EQUAL(2, ret);

  ASSERT_EQUAL(0xFA, rbuf[0]);
  ASSERT_EQUAL(0xAA, rbuf[1]);

	// 复位TMS 5个1, 到RESET/IDLE状态
	wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = 0x4;
	wbuf[2] = 0x1f;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // 进到Shift-DR状态
  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = 0x3; // 会输出3个时钟
	wbuf[2] = 0x2; //
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // shift DR out 32bit
  wbuf[0] = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ;
  wbuf[1] = 0x3;
  wbuf[2] = 0;
  wbuf[3] = 0;
  wbuf[4] = 0;
  wbuf[5] = 0;
  wbuf[6] = 0;
  ret = ftdi_write_data(&data->ctx, wbuf, 7);
  ASSERT_EQUAL(7, ret);

  usleep(5*1000);
  ret = ftdi_read_data(&data->ctx, rbuf, 32);
  ASSERT_EQUAL(4, ret);
  log_info("idcode: %02x%02x%02x%02x.", rbuf[3], rbuf[2], rbuf[1], rbuf[0]);
}

CTEST2(ftdi_playground, jtaglib_test) {
  int ret;
  unsigned char wbuf[32] = {0};
  unsigned char rbuf[32] = {0};
  TMS_SeqInfo seq;

  // 复位TMS 5个1, 到RESET/IDLE状态
	wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = 0x4;
	wbuf[2] = 0x1f;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  seq = JtagGetTmsSequence(JTAG_TAP_RESET, JTAG_TAP_DRSHIFT);
  log_info("RESET->DRSHIFT:seq:%x, cnt:%x", seq >> 8, seq & 0xFF);

  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = (seq & 0xFF) - 1;
	wbuf[2] = (seq >> 8) & 0xFF;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // shift DR out 32bit
  wbuf[0] = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ;
  wbuf[1] = 0x3;
  wbuf[2] = 0;
  wbuf[3] = 0;
  wbuf[4] = 0;
  wbuf[5] = 0;
  wbuf[6] = 0;
  ret = ftdi_write_data(&data->ctx, wbuf, 7);
  ASSERT_EQUAL(7, ret);

  usleep(5*1000);
  ret = ftdi_read_data(&data->ctx, rbuf, 32);
  ASSERT_EQUAL(4, ret);
  log_info("idcode: %02x%02x%02x%02x.", rbuf[3], rbuf[2], rbuf[1], rbuf[0]);

  seq = JtagGetTmsSequence(JTAG_TAP_DRSHIFT, JTAG_TAP_IDLE);
  log_info("RESET->DRSHIFT:seq:%x, cnt:%x", seq >> 8, seq & 0xFF);

  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = (seq & 0xFF) - 1;
	wbuf[2] = (seq >> 8) & 0xFF;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  seq = JtagGetTmsSequence(JTAG_TAP_IDLE, JTAG_TAP_DRSHIFT);
  log_info("RESET->DRSHIFT:seq:%x, cnt:%x", seq >> 8, seq & 0xFF);

  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = (seq & 0xFF) - 1;
	wbuf[2] = (seq >> 8) & 0xFF;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // shift DR out 32bit
  wbuf[0] = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ;
  wbuf[1] = 0x3;
  wbuf[2] = 0;
  wbuf[3] = 0;
  wbuf[4] = 0;
  wbuf[5] = 0;
  wbuf[6] = 0;
  ret = ftdi_write_data(&data->ctx, wbuf, 7);
  ASSERT_EQUAL(7, ret);

  usleep(5*1000);
  ret = ftdi_read_data(&data->ctx, rbuf, 32);
  ASSERT_EQUAL(4, ret);
  log_info("idcode: %02x%02x%02x%02x.", rbuf[3], rbuf[2], rbuf[1], rbuf[0]);
}

CTEST2(ftdi_playground, read_dtmcs) {
  int ret;
  unsigned char wbuf[32] = {0};
  unsigned char rbuf[32] = {0};
  TMS_SeqInfo seq;

  // 复位TMS 5个1, 到RESET/IDLE状态
	wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = 0x4;
	wbuf[2] = 0x1f;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  seq = JtagGetTmsSequence(JTAG_TAP_RESET, JTAG_TAP_IRSHIFT);
  log_info("RESET->IRSHIFT:seq:%x, cnt:%x", seq >> 8, seq & 0xFF);

  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = (seq & 0xFF) - 1;
	wbuf[2] = (seq >> 8) & 0xFF;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // write 0x10 to IR, 5 bits
  wbuf[0] = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ | MPSSE_BITMODE;
  wbuf[1] = 0x3; // 发送前四个，全0
  wbuf[2] = 0x0;
  ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  seq = JtagGetTmsSequence(JTAG_TAP_IRSHIFT, JTAG_TAP_IREXIT1);
  log_info("IR-SHIFT->IR-EXIT1:seq:%x, cnt:%x", seq >> 8, seq & 0xFF);
  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG | MPSSE_DO_READ; // 0x6b
	wbuf[1] = (seq & 0xFF) - 1;
	wbuf[2] = ((seq >> 8) & 0xFF) | 0x80; // 将bit7置位，在Clock到来之前输出到TDI，
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  usleep(5*1000);
  ret = ftdi_read_data(&data->ctx, rbuf, 32);
  ASSERT_EQUAL(2, ret);
  // 低位依次出来，从高位进入，
  // 输出4个，所以读出来的数据是高4位
  // 输出1个，读出来的数据在最高位
  ASSERT_EQUAL(0x1, ((rbuf[0] >> 4) & 0xF) | ((rbuf[1] >> 3) & 0x10));

  // to DR SHIFT
  seq = JtagGetTmsSequence(JTAG_TAP_IREXIT1, JTAG_TAP_DRSHIFT);
  log_info("IR-EXIT1->DR-SHIFT:seq:%x, cnt:%x", seq >> 8, seq & 0xFF);

  wbuf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG;
	wbuf[1] = (seq & 0xFF) - 1;
	wbuf[2] = (seq >> 8) & 0xFF;
	ret = ftdi_write_data(&data->ctx, wbuf, 3);
  ASSERT_EQUAL(3, ret);

  // 读取 dtmcs
  // shift DR out 32bit
  wbuf[0] = MPSSE_LSB | MPSSE_WRITE_NEG | MPSSE_DO_WRITE | MPSSE_DO_READ;
  wbuf[1] = 0x3;
  wbuf[2] = 0;
  wbuf[3] = 0;
  wbuf[4] = 0;
  wbuf[5] = 0;
  wbuf[6] = 0;
  ret = ftdi_write_data(&data->ctx, wbuf, 7);
  ASSERT_EQUAL(7, ret);

  usleep(5*1000);
  ret = ftdi_read_data(&data->ctx, rbuf, 32);
  ASSERT_EQUAL(4, ret);
  log_info("dtmcs: %02x%02x%02x%02x.", rbuf[3], rbuf[2], rbuf[1], rbuf[0]);
}

