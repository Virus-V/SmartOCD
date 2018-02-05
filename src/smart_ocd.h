/*
 * smart_ocd.h
 *
 *  Created on: 2018-1-19
 *      Author: virusv
 */

#ifndef SRC_SMART_OCD_H_
#define SRC_SMART_OCD_H_

// 定义BOOL类型
typedef enum {
	FALSE = 0,
	TRUE
} BOOL;

// 用户输入lua命令历史记录文件
#define COMMAND_HISTORY "smartocd_history.txt"
#endif /* SRC_SMART_OCD_H_ */
