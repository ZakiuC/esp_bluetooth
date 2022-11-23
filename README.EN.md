English | [简体中文](./README.md)
# Esp32s3 ble

Esp32s3's low-power Bluetooth component communicates with WeChat applets


## Adaptation of small programs.

WeChat search 咩诶诶诶 small program

## Function Description

    Initialization:ble_Init(name);                         name is the name of the ble broadcast
    Turn off Bluetooth:ble_close();                        Turn off Bluetooth
    Check all service commands:CheckCmdAll();              Return 16 bits of data, each bit represents a command, low bit -> high bit (command 1->command x)
    Individual inspection and maintenance commands:CheckCmd(CMDX);               The parameter is CMDX,X is the command label, for example, CheckCmd(CMD1), that is, CheckCmd1 returns true then there is a command, fasle means there is no command
    Check the maintenance command separately and execute the corresponding function:CheckCmdGoOn(CMDX, FUNC);   The parameter CMDX is the same as the previous function, and the parameter FUNC is the function to be executed after confirming the receipt of the command, this function should have no return value and no parameters.
    Modify command rebound settings:SwitchCmdMode(CMDX, Switch);  The parameter CMDX is the same as the previous function, the parameter Switch is 1 or 0. 1 is to turn on the rebound setting, 0 is to turn off
    Separate reset service command:ClrCmdFlag(CMDX);             The parameter CMDX is the same as the previous function, for example, ClrCmdFlag(CMD1) is to reset Cmd1 to 0x00
    All commands reset:ClrAllCmdFlag();                  Reset all command states to 0x00
    Start Bluetooth OTA:ble_ota();                         Calling this function opens a thread waiting for the controller to send an ota command or send an OTA upgrade file to service1 feature1 and process it.


## Command

### Inspection orders:
            Function 1: On 0x01 Off 0x00
            Function 2: On 0x01 Off 0x00
            Function 3: On 0x01 Off 0x00
            Function 4: On 0x01 Off 0x00
            Function 5: On 0x01 Off 0x00
            Function 6: On 0x01 Off 0x00
            Function 7: On 0x01 Off 0x00

### ota command:
            Start ota: On 0x03

## ota process:

After turning on Bluetooth OTA, the sender sends the start ota command and OTA file size (Byte), the sender receives the message that the file can be sent (need to turn on listening to feature value 1), sends the OTA file, and automatically restarts the system after receiving the file to complete the OTA.