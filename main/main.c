#include "main.h"

#include "blue_app.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "rom/rtc.h"
#include "driver/gpio.h"

void set(void);
void resetCmd(void);
void resetAllCmd(void);
void Allcheck(void);
void check(void);
void checkAndGo(void);



void exp_test(void)
{
    printf("test\n");
    vTaskDelay(3000/portTICK_RATE_MS);
    EndMission();
    appToast("task end.");
}

void app_main(void)
{
    ble_Init("OTA-BLF");        //ble初始化,参数为ble设备名字
    // ble_ota();                  //开始ota
    while(1)
    {
        CheckCmdGoOn(CMD1, exp_test);
        vTaskDelay(10/portTICK_RATE_MS); 
    }
} 


/**
 * 修改命令回弹设置示例
*/
void set(void)
{
    SwitchCmdMode(CMD1, 1);            //开启检修命令一的自动回弹，任意检查检修命令一的函数执行后，检修命令一会自动复位至未触发状态
    SwitchCmdMode(CMD1, 0);            //关闭检修命令一的自动回弹，任意检查检修命令一的函数执行后，检修命令一会保持当前状态
}


/**
 * 复位检修命令状态
*/
void resetCmd(void)
{
    ClrCmdFlag(CMD1);                   //复位检修命令一为未触发状态（0x00）
}


/**
 * 复位所有命令状态
*/
void resetAllCmd(void)
{
    ClrAllCmdFlag();                   //复位所有命令为未触发状态（0x00）
}


/**
 * 查询所有检修命令示例
*/
void Allcheck(void)
{
    uint16_t Cmd = CheckCmdAll();
    if(Cmd & MASKCMD1){
        printf("功能一触发\n");
    }

    if(Cmd & MASKCMD2){
        printf("功能二触发\n");
    }

    if(Cmd & MASKCMD3){
        printf("功能三触发\n");
    }

    if(Cmd & MASKCMD4){
        printf("功能四触发\n");
    }

    if(Cmd & MASKCMD5){
        printf("功能五触发\n");
    }

    if(Cmd & MASKCMD6){
        printf("功能六触发\n");
    }

    if(Cmd & MASKCMD7){
        printf("功能七触发\n");
    }
}


/**
 * 单独查询检修命令示例
*/
void check(void)
{
    if(CheckCmd(CMD1)){
        printf("功能一触发\n");
    }

    if(CheckCmd(CMD2)){
        printf("功能二触发\n");
    }
}


/**
 * 查询检修命令后执行某个无返回值函数示例
*/
void test(void)
{
    printf("test\n");
}
void checkAndGo(void)
{
    CheckCmdGoOn(CMD1, test);           //检查检修命令一，若为触发状态（0x01），则执行test()
}


