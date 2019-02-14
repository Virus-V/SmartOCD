/*
 * misc.c
 *
 *  Created on: 2018-4-7
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smart_ocd.h"

/**
 * 32位按位镜像翻转
 * BitReverse
 */
uint32_t misc_BitReverse(uint32_t n){
	n = ((n >>  1) & 0x55555555u) | ((n <<  1) & 0xaaaaaaaau);
	n = ((n >>  2) & 0x33333333u) | ((n <<  2) & 0xccccccccu);
	n = ((n >>  4) & 0x0f0f0f0fu) | ((n <<  4) & 0xf0f0f0f0u);
	n = ((n >>  8) & 0x00ff00ffu) | ((n <<  8) & 0xff00ff00u);
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
int misc_PrintBulk(char *data, int length, int rowLen){
	// 计算要打印多少行
	int lineCnt, placeCnt, tmp;	// 数据总行数，行数的数字位数（用来计算前导0）
	int currRow, printedCnt = 0;	// 当前输出的行和列
	// 如果参数不对则直接返回
	if(length == 0 || rowLen == 0) return 0;

	lineCnt = (length / rowLen) + 1;	// 计算行数
	for(tmp = length - (length % rowLen), placeCnt = 1; tmp >= 16; placeCnt++){
		tmp /= 16;
	}
    // 输出一行数据：前导位置信息：数据...
	for(currRow = 0; currRow < lineCnt && length > 0; currRow ++){
		int currPlaceCnt;	// 已输出的数据数字位数和已输出的数据
		// 计算位数
		for(tmp = printedCnt, currPlaceCnt = 1; tmp >= 16; currPlaceCnt++){
			tmp /= 16;
		}
		// 输出前导信息
		for(tmp = placeCnt - currPlaceCnt; tmp > 0; tmp--){
			putchar('0');
		}
		printf("%X: ", printedCnt);
		// 输出数据
        int dataLen = length > rowLen ? rowLen : length;
        int blankLen = rowLen - dataLen;
        for(tmp = 0; tmp < dataLen; tmp++){
            printf("%02hhX ", *(data + printedCnt + tmp));
        }
        // 打印占位符
        for(tmp = 0; tmp < blankLen * 3; tmp++){
           printf(" "); 
        }
        printf("| ");
        // 打印ascii
        for(tmp = 0; tmp < dataLen; tmp++){
            printf("%c", (0x20 <= *(data + printedCnt + tmp) && *(data + printedCnt + tmp) <= 0x7e) ? *(data + printedCnt + tmp) : '.');
        }
        for(tmp = 0; tmp < blankLen; tmp++){
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
