/*
 * printf_test.c
 *
 *  Created on: 2018-2-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "smart_ocd.h"
#include "misc/misc.h"

char* itoa(int num, char *str, int radix) {
	/*索引表*/
	char index[] = "0123456789ABCDEF";
	unsigned unum;/*中间变量*/
	int i = 0, j, k;
	/*确定unum的值*/
	if (radix == 10 && num < 0)/*十进制负数*/
	{
		unum = (unsigned) -num;
		str[i++] = '-';
	} else
		unum = (unsigned) num;/*其他情况*/
	/*转换*/
	do {
		str[i++] = index[unum % (unsigned) radix];
		unum /= radix;
	} while (unum);
	str[i] = '\0';
	/*逆序*/
	if (str[0] == '-')
		k = 1;/*十进制负数*/
	else k = 0;
	char temp;
	for (j = k; j <= (i - 1) / 2; j++) {
		temp = str[j];
		str[j] = str[i - 1 + k - j];
		str[i - 1 + k - j] = temp;
	}
	return str;
}

#define BIT2BYTE(bitCnt) ({	\
	int byteCnt = 0;	\
	int tmp = (bitCnt) >> 6;	\
	byteCnt = tmp + (tmp << 3);	\
	tmp = bitCnt & 0x3f;	\
	byteCnt += tmp ? ((tmp + 7) >> 3) + 1 : 0;	\
})

int main(){
	uint8_t data[5];
	uint32_t tmp;
	for(int n=0,j=0;n<5;n++){
		printf("%d\n", j++);
	}
	printf("%d\n", BIT2BYTE(65));
	return 0;
}
