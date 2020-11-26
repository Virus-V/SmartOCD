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
#include "ctest.h"

#include "Adapter/ftdi/ftdi.h"

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

// 设置接口测试
CTEST(ftdi, test2) {
}
