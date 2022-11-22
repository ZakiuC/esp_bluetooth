#ifndef __BLE_APP_H_
#define __BLE_APP_H_



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************全局宏**********************/
#define READYOTA    0X03
#define FINISHOTA   0X04
#define NOCMD       0X00

//维修命令
#define CMD1        &BleCmd.OHCmd.Cmd1
#define CMD2        &BleCmd.OHCmd.Cmd2
#define CMD3        &BleCmd.OHCmd.Cmd3
#define CMD4        &BleCmd.OHCmd.Cmd4
#define CMD5        &BleCmd.OHCmd.Cmd5
#define CMD6        &BleCmd.OHCmd.Cmd6
#define CMD7        &BleCmd.OHCmd.Cmd7


//维修命令掩码
#define MASKCMD1        (0x1)
#define MASKCMD2        (0x1 << 1)
#define MASKCMD3        (0x1 << 2)
#define MASKCMD4        (0x1 << 3)
#define MASKCMD5        (0x1 << 4)
#define MASKCMD6        (0x1 << 5)
#define MASKCMD7        (0x1 << 6)



/*****************私有宏**********************/
/**
 * 检修命令结构体
 * 描述：接收检修命令结构体
 * Cmd1：命令1
 * Cmd2：命令2
 * Cmd3：命令3
 * Cmd4：命令4
 * Cmd5：命令5
 * Cmd6：命令6
 * Cmd7：命令7
*/
typedef struct 
{
    uint8_t Cmd1;
    uint8_t Cmd2;
    uint8_t Cmd3;
    uint8_t Cmd4;
    uint8_t Cmd5;
    uint8_t Cmd6;
    uint8_t Cmd7;
}Overhaul_t;

/****数据格式*****
 * StartData | SumData_1 | ... | SumData_4 | Data_0 ~ Data_x | ota_cmd | ota_fileSize_1 | ... | ota_fileSize_4 | EndData 
*/

/**
 * 接收数据结构体
 * len：接收数据长度
 * startFlag：是否开始处理数据1
 * index：索引
 * startPosition：开始位置
 * sumData：数据和校验
 * cmdFix：检修命令
 * ota_cmd：ota命令
 * ota_fileSize：ota文件大小
 * sumCheck：计算接收数据和*/
typedef struct 
{
    uint16_t len;
    bool startFlag;
    uint16_t index;
    uint16_t startPosition;
    uint32_t sumData;
    Overhaul_t cmdFix;
    uint8_t ota_cmd;
    uint32_t ota_fileSize;
    uint32_t sumCheck;
}RxData_t;


#define STARTDATA        (uint8_t)0xff          //起始位
#define ENDDATA          (uint8_t)0xff          //结束位


#define SWITCHON          0X01                  //检修命令开
#define SWITCHOFF         0X00                  //检修命令关

/** 颜色 **/
#define NONE                  "\e[0m"           //清除颜色，即之后的打印为正常输出，之前的不受影响
#define BLACK                 "\e[0;30m"        //深黑
#define L_BLACK             "\e[1;30m"          //亮黑，偏灰褐
#define RED                     "\e[0;31m"      //深红，暗红
#define L_RED                 "\e[1;31m"        //鲜红
#define GREEN                "\e[0;32m"         //深绿，暗绿
#define L_GREEN            "\e[1;32m"           //鲜绿
#define BROWN               "\e[0;33m"          //深黄，暗黄
#define YELLOW              "\e[1;33m"          //鲜黄
#define BLUE                    "\e[0;34m"      //深蓝，暗蓝
#define L_BLUE                "\e[1;34m"        //亮蓝，偏白灰
#define PURPLE               "\e[0;35m"         //深粉，暗粉，偏暗紫
#define L_PURPLE           "\e[1;35m"           //亮粉，偏白灰
#define CYAN                   "\e[0;36m"       //暗青色
#define L_CYAN               "\e[1;36m"         //鲜亮青色
#define GRAY                   "\e[0;37m"       //灰色
#define WHITE                  "\e[1;37m"       //白色，字体粗一点，比正常大，比bold小
#define BOLD                    "\e[1m"         //白色，粗体
#define UNDERLINE         "\e[4m"               //下划线，白色，正常大小
#define BLINK                   "\e[5m"         //闪烁，白色，正常大小
#define REVERSE            "\e[7m"              //反转，即字体背景为白色，字体为黑色
#define HIDE                     "\e[8m"        //隐藏
#define CLEAR                  "\e[2J"          //清除
#define CLRLINE               "\r\e[K"          //清除行
#define HIDECURSOR              "\e[?25l"    



/**
 * 命令结构体
 * 描述：接收命令结构体
 * OHCmd：检修命令
 * OtaCmd：Ota命令
*/
typedef struct                  
{
    Overhaul_t  OHCmd;
    uint8_t OtaCmd;
}Cmd_t;

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
    float TotalFile;                       
}FileSize_t;


/***************全局变量*****************/
extern Cmd_t BleCmd;                           //命令
extern FileSize_t FileSize;                 //文件大小


/***************蓝牙特征枚举********************/
enum
{
    IDX_SVC,

    IDX_CHAR_A,                             //命令  
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,

    HRS_IDX_NB,
};


/*****************函数声明**********************/
void ble_Init(char *name);         //ble初始化
void ble_ota(void);                //ota
uint16_t CheckCmdAll(void);               //确认命令并执行相对应的操作
bool CheckCmd(uint8_t *Cmd);
void ClrAllCmdFlag(void);
void CheckCmdGoOn(uint8_t *Cmd, void (*func)());
void SwitchCmdMode(uint8_t *Cmd,uint8_t Switch);//更改检修命令对应的回弹模式
void ClrCmdFlag(uint8_t *Cmd);
void EndMission(void);
void appToast(char text[]);
void ble_close(void);               //关闭蓝牙
#endif     //ble.h