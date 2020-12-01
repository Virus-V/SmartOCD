/*
 * JTAG_test.c
 *
 *  Created on: 2018-3-24
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctest.h"

#include "smartocd.h"

#include "Library/jtag/jtag.h"
#include "Library/log/log.h"

char *itoa(int num, char *str, int radix) { /*索引表*/
  char index[] = "0123456789ABCDEF";
  unsigned unum; /*中间变量*/
  int i = 0, j, k;
  /*确定unum的值*/
  if (radix == 10 && num < 0) /*十进制负数*/
  {
    unum = (unsigned)-num;
    str[i++] = '-';
  } else
    unum = (unsigned)num; /*其他情况*/
  /*转换*/
  do {
    str[i++] = index[unum % (unsigned)radix];
    unum /= radix;
  } while (unum);
  str[i] = '\0';
  /*逆序*/
  if (str[0] == '-')
    k = 1; /*十进制负数*/
  else
    k = 0;
  char temp;
  for (j = k; j <= (i - 1) / 2; j++) {
    temp = str[j];
    str[j] = str[i - 1 + k - j];
    str[i - 1 + k - j] = temp;
  }
  return str;
}

static int parseTMS(uint8_t *buff, uint16_t seqInfo, BOOL TDO_Capture) {
  assert(buff != NULL);
  uint8_t bitCount = seqInfo & 0xff;
  uint8_t TMS_Seq = seqInfo >> 8;
  int writeCount = 1;
  if (bitCount == 0)
    return 0;
  *buff = TDO_Capture ? 0x80 : 0;
  while (bitCount--) {
    *buff |= (TMS_Seq & 0x1) << 6;
    (*buff)++;
    TMS_Seq >>= 1;
    if (bitCount && (((TMS_Seq & 0x1) << 6) ^ (*buff & 0x40))) {
      *++buff = 0;
      *++buff = TDO_Capture ? 0x80 : 0;
      writeCount += 2;
    }
  }
  *++buff = 0;
  return writeCount + 1;
}

/**
 * JTAG 测试
 * 产生时序
 * 在所有时序中任意组合，最长时序不超过8位。
 */
CTEST(jtag, jtag_test) {
  uint8_t buff[128], cnt;
  char s[16];
  uint16_t data;
  memset(buff, 0, sizeof(buff));
  enum JTAG_TAP_State fromStatus, toStatus;
  for (fromStatus = JTAG_TAP_RESET; fromStatus <= JTAG_TAP_IRUPDATE; fromStatus++) {
    for (toStatus = JTAG_TAP_RESET; toStatus <= JTAG_TAP_IRUPDATE; toStatus++) {
      printf("%s ==> %s.\t\t\t", JtagStateToStr(fromStatus), JtagStateToStr(toStatus));
      data = JtagGetTmsSequence(fromStatus, toStatus);
      itoa(data >> 8, s, 2);
      printf("Clocks %d TMS:%s\n", data & 0xff, s);
    }
  }
}
