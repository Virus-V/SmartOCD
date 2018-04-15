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

#define MAKE_nPACC_CHAIN_DATA(buff,data,ctrl) {	\
	uint32_t data_t = data;					\
	uint8_t msb3 = ((data_t) & 0xe0) >> 5;	\
	uint8_t lsb3 = (ctrl) & 0x7;			\
	int n;									\
	for(n=0; n<4; n++){						\
		*(CAST(uint8_t *,(buff)) + n) = (((data_t) & 0xff) << 3) | lsb3;	\
		(data_t) >>= 8;						\
		lsb3 = msb3;						\
		msb3 = ((data_t) & 0xe0) >> 5;		\
	}										\
	*(CAST(uint8_t *,(buff)) + n) = lsb3;	\
}

#define GET_nPACC_CHAIN_DATA(buff) ({	\
	uint8_t *data_t = CAST(uint8_t *, (buff)), lsb3;	\
	uint32_t tmp = 0;	\
	for(int n=0;n<4;n++){	\
		lsb3 = (data_t[n+1] & 0x7) << 5;	\
		tmp |= ((data_t[n] >> 3) | lsb3) << (n << 3);	\
	}	\
	tmp;	\
})

uint32_t getdata(uint8_t *data){
	uint8_t *data_t = CAST(uint8_t *, data);
	uint8_t lsb3;
	uint32_t tmp = 0;
	for(int n=0;n<4;n++){
		lsb3 = (data_t[n+1] & 0x7) << 5;
		tmp |= ((data_t[n] >> 3) | lsb3) << (n << 3);
	}
	return tmp;
}

int main(){
	uint8_t data[5];
	uint32_t tmp;
	MAKE_nPACC_CHAIN_DATA(data, 0xdeadbeefu, 0x6);
	tmp = GET_nPACC_CHAIN_DATA(data);
	misc_PrintBulk(data, 5, 5);
	printf("0x%x\n", tmp);
	return 0;
}
