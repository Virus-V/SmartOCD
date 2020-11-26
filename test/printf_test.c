/*
 * printf_test.c
 *
 *  Created on: 2018-2-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "smartocd.h"
#include "Library/misc/misc.h"

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

#define SET_Nth_BIT(data,n,val) do{	\
	uint8_t tmp_data = *(CAST(uint8_t *, (data)) + ((n)>>3)); \
	tmp_data &= ~(1 << ((n) & 0x7));	\
	tmp_data |= ((val) & 0x1) << ((n) & 0x7);	\
	*(CAST(uint8_t *, (data)) + ((n)>>3)) = tmp_data;	\
}while(0);


int test_main(int argc, const char *argv[]){
	uint8_t sdad[5];
	memset(sdad, 0, 5);
	for(int i=0;i<40;i++){
		SET_Nth_BIT(sdad, i, 1);
		misc_PrintBulk(sdad, 5, 5);
	}
	for(int i=0;i<40;i++){
		SET_Nth_BIT(sdad, i, 0);
		misc_PrintBulk(sdad, 5, 5);
	}
	return 0;
}
