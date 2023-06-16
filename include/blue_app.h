#ifndef __BLE_APP_H_
#define __BLE_APP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************全局宏**********************/
#define CMD_NONE 0X00	// 无命令
#define CMD_CHECKVERSION 0XC1	// 查询固件版本
#define CMD_USETIMES 0XC2	// 查询使用次数
#define CMD_STARTUPDATE 0XC3	// 属性上报线程开关
#define CMD_STARTOTA 0XC4	// OTA启动
#define CMD_SETDATA 0XC5	// 设置数据
#define CMD_STARTFIX 0XC6	// 开启维修模式
#define CMD_SAVE2CUP 0XC7	// 是否保存到设备杯子

#define CMD_VERSION 0XB1	// 版本
#define CMD_TIMES 0XB2	// 次数
#define CMD_UPDATE 0XB3	//属性上报
#define CMD_OTA 0XB4	// OTA
#define CMD_DATA 0XB5	// 数据
#define CMD_FIX 0XB6	// 检修模式
#define CMD_CUP 0XB7	// 杯子状态

#define STARTDATA (uint8_t)0xff // 起始位
#define ENDDATA (uint8_t)0xff	// 结束位


// OTA状态
typedef enum
{
	OTAREADY = 0X00,
	OTARATE,
	OTAOVERFILE,
	OTASUCESS,
	OTAERROR_1, // 写入错误
	OTAERROR_2, // 数据验证失败
	OTAERROR_3, // OTA结束失败
	OTAERROR_4, // OTA设置boot分区失败
	OTAFILESIZE
} otaPeriod_e;

// 蓝牙模式
typedef enum
{
	CLOSE = 0X00,
	APP,
	FIX,
	OTA,
} bleMode_e;

// 检修模式
typedef enum
{
	NOMODE = 0X00,
	MODE1,
	MODE2,
	MODE3,
	MODE4,
	MODE5,
	MODE6,
	MODE7
} fixMode_e;

/****应用数据格式*****
 * StartData | SumData_1 | ... | SumData_4 | CMD | DataLen | PropertyData_1 | PropertyData_2 | PropertyData_3 | PropertyData_4| PropertyData_5 | PropertyData_6 | PropertyData_7 | PropertyData_8 | EndData
 */
/**
 * 接收数据结构体
 * SUM： 校验和
 * CMD: 命令
 * DataLen: 数据长度
 * PropertyData_1: 数据1
 * PropertyData_2: 数据2
 * PropertyData_3: 数据3
 * PropertyData_4: 数据4
 * PropertyData_5: 数据5
 * PropertyData_6: 数据6
 * PropertyData_7: 数据7
 * PropertyData_8: 数据8
 */
typedef struct
{
	uint32_t SUM;
	uint8_t CMD;
	uint8_t DataLen;
	uint8_t PropertyData_1;
	uint8_t PropertyData_2;
	uint8_t PropertyData_3;
	uint8_t PropertyData_4;
	uint8_t PropertyData_5;
	uint8_t PropertyData_6;
	uint8_t PropertyData_7;
	uint8_t PropertyData_8;
} TEST_DATA_t;

/**
 * 接收数据结构体
 * len：接收数据长度
 * startFlag：是否开始处理数据1
 * index：索引
 * startPosition：开始位置
 * sumData：数据和校验
 * data：数据内容
 * sumCheck：计算接收数据和 */
typedef struct
{
    uint16_t len;
    bool startFlag;
    uint16_t index;
    uint16_t startPosition;
    uint32_t sumData;

	TEST_DATA_t data;

	uint32_t sumCheck;
} Test_RxData_t;

/**
 * 接收文件相关数据结构体
 * 描述：与OTA升级文件接收相关的数据
 * TotalLen：接收数据总长度（文件用）
 * ReceivingProgress：接收百分比
 * LastProgress：上一次的接收百分比
 * TotalFile：文件大小（BYTE）
 * AutoGoOn：是否自动重启系统完成OTA
 */
typedef struct
{
	uint32_t TotalLen;
	uint32_t ReceivingProgress;
	uint32_t LastProgress;
	uint32_t TotalFile;
} FileSize_t;

typedef struct
{
	uint8_t data1;
	uint8_t data2;
	uint8_t data3;
} SettingData_t;

typedef uint8_t *(*dataFuncType_t)(void);

// 蓝牙控制结构体
typedef struct
{
	char *name;
	bleMode_e bleMode;
	fixMode_e fixMode;
	TEST_DATA_t data;
	FileSize_t file;
	SettingData_t setting;
	uint8_t fixModeId;
	dataFuncType_t funcGetVersion;
	dataFuncType_t funcGetTimesMode1;
	dataFuncType_t funcGetTimesMode2;
	dataFuncType_t funcGetTimesMode3;
	dataFuncType_t funcGetAttr;
} bleController_t;

/***************蓝牙特征枚举********************/
enum
{
    IDX_SVC,

    IDX_CHAR_A, //命令
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,

    IDX_CHAR_C,
    IDX_CHAR_VAL_C,
	IDX_CHAR_CFG_C,

    HRS_IDX_NB,
};


/*****************函数声明**********************/
void ble_Init(bleController_t *bleController, char *name, uint8_t *(*funcGetVersion)(void), uint8_t *(*funcGetTimesMode1)(void), uint8_t *(*funcGetTimesMode2)(void), uint8_t *(*funcGetTimesMode3)(void), uint8_t *(*funcGetAttr)(void)); // ble初始化
void ble_close(void); //关闭蓝牙
void endTest(uint8_t id, uint8_t data);
#endif // ble.h