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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "smartocd.h"

/**
 * 32位按位镜像翻转
 * BitReverse
 */
uint32_t misc_BitReverse(uint32_t n) {
  n = ((n >> 1) & 0x55555555u) | ((n << 1) & 0xaaaaaaaau);
  n = ((n >> 2) & 0x33333333u) | ((n << 2) & 0xccccccccu);
  n = ((n >> 4) & 0x0f0f0f0fu) | ((n << 4) & 0xf0f0f0f0u);
  n = ((n >> 8) & 0x00ff00ffu) | ((n << 8) & 0xff00ff00u);
  n = ((n >> 16) & 0x0000ffffu) | ((n << 16) & 0xffff0000u);
  return n;
}

/*
 * 打印数据块
 * data：数据内容
 * length：数据长度
 * rowLen：每一行打印多少个byte
 * 返回：打印的行数
 */
int misc_PrintBulk(char *data, int length, int rowLen) {
  // 计算要打印多少行
  int lineCnt, placeCnt, tmp;  // 数据总行数，行数的数字位数（用来计算前导0）
  int currRow, printedCnt = 0; // 当前输出的行和列
  // 如果参数不对则直接返回
  if (length == 0 || rowLen == 0)
    return 0;

  lineCnt = (length / rowLen) + 1; // 计算行数
  for (tmp = length - (length % rowLen), placeCnt = 1; tmp >= 16; placeCnt++) {
    tmp /= 16;
  }
  // 输出一行数据：前导位置信息：数据...
  for (currRow = 0; currRow < lineCnt && length > 0; currRow++) {
    int currPlaceCnt; // 已输出的数据数字位数和已输出的数据
    // 计算位数
    for (tmp = printedCnt, currPlaceCnt = 1; tmp >= 16; currPlaceCnt++) {
      tmp /= 16;
    }
    // 输出前导信息
    for (tmp = placeCnt - currPlaceCnt; tmp > 0; tmp--) {
      putchar('0');
    }
    printf("%X: ", printedCnt);
    // 输出数据
    int dataLen = length > rowLen ? rowLen : length;
    int blankLen = rowLen - dataLen;
    for (tmp = 0; tmp < dataLen; tmp++) {
      printf("%02hhX ", *(data + printedCnt + tmp));
    }
    // 打印占位符
    for (tmp = 0; tmp < blankLen * 3; tmp++) {
      printf(" ");
    }
    printf("| ");
    // 打印ascii
    for (tmp = 0; tmp < dataLen; tmp++) {
      printf("%c", (0x20 <= *(data + printedCnt + tmp) &&
                    *(data + printedCnt + tmp) <= 0x7e)
                       ? *(data + printedCnt + tmp)
                       : '.');
    }
    for (tmp = 0; tmp < blankLen; tmp++) {
      printf(" ");
    }
    printf(" |");
    length -= dataLen;
    // 加入输出总长度
    printedCnt += dataLen;
    printf("\n");
  }
  return currRow;
}

int msleep(long msec) {
  struct timespec ts;
  int res;

  if (msec < 0) {
    errno = EINVAL;
    return -1;
  }

  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  do {
    res = nanosleep(&ts, &ts);
  } while (res && errno == EINTR);

  return res;
}
