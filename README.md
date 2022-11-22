<!-- [English](./README.EN.md) | 简体中文 -->
# Blue App

Esp32s3 的低功耗蓝牙组件

## 使用

    初始化：ble_Init(name);                         name为ble广播的名字
    关闭蓝牙：ble_close();                          关闭蓝牙
    检查所有检修命令：CheckCmdAll();                
    返回16位数据，每一位代表一个命令，低位->高位（命令1->命令x）
    单独检查检修命令：CheckCmd(CMDX);               参数为CMDX,X为命令标号，例如 CheckCmd(CMD1) 即检查Cmd1    返回true则有命令，fasle则表示没有命令
    单独检查检修命令并执行对应函数：CheckCmdGoOn(CMDX, FUNC);   参数CMDX同上一个函数，参数FUNC为确认接收有命令后所要执行的函数，此函数需无返回值无参，使用此函数在结束时调用EndMission(CMDX)函数来完成相关参数的处理，以及上报app结束消息。
    修改命令回弹设置：SwitchCmdMode(CMDX, Switch);  参数CMDX同上一个函数，参数 Switch 为 1 或者 0。1 为开启回弹设置，0 为关闭
    单独复位检修命令：ClrCmdFlag(CMDX);             参数CMDX同上一个函数，例如 ClrCmdFlag(CMD1) 即为复位Cmd1为 0x00
    全部命令复位：ClrAllCmdFlag();                  复位所有的命令状态为 0x00
    开始蓝牙OTA：ble_ota();                         调用此函数后会开启一个线程等待控制器向服务1 特征值1 发送ota 命令或是发送OTA升级文件，并处理。


## 命令

### 检修命令：
            功能一： 开 0x01     关 0x00
            功能二： 开 0x01     关 0x00
            功能三： 开 0x01     关 0x00
            功能四： 开 0x01     关 0x00
            功能五： 开 0x01     关 0x00
            功能六： 开 0x01     关 0x00
            功能七： 开 0x01     关 0x00

### ota命令：
            开始ota： 开 0x03

## ota流程：

开启蓝牙OTA后，发送端发送 开始ota命令以及OTA文件大小（Byte）,发送端收到可以发送文件的消息后（需要开启对特征值1的监听），发送OTA文件，接受完文件后自动重启系统完成OTA。