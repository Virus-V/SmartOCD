/**
 * test/ftdi_test.c
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
    ret = ConnectFtdi(data->ftdiObj, vids, pids, NULL);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);

    ret = DisconnectFtdi(data->ftdiObj);
    ASSERT_EQUAL(ADPT_SUCCESS, ret);
  }
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
