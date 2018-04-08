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
#include "target/JTAG.h"

uint32_t bit_reverse(uint32_t n){
	n = ((n >>  1) & 0x55555555u) | ((n <<  1) & 0xaaaaaaaau);
	n = ((n >>  2) & 0x33333333u) | ((n <<  2) & 0xccccccccu);
	n = ((n >>  4) & 0x0f0f0f0fu) | ((n <<  4) & 0xf0f0f0f0u);
	n = ((n >>  8) & 0x00ff00ffu) | ((n <<  8) & 0xff00ff00u);
	n = ((n >> 16) & 0x0000ffffu) | ((n << 16) & 0xffff0000u);
	return n;
}

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

int main(){
	uint32_t tms_seq;
	char str[64];
	tms_seq = 0xff000f0fu;
	printf("%s\n", itoa(tms_seq, str, 2));
	tms_seq = bit_reverse(tms_seq);
	printf("%s\n", itoa(tms_seq, str, 2));
	return 0;
}
