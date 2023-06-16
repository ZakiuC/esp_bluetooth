English | [简体中文](./README.md)
#
ble MTU size is 512 and includes the following services and features:
| | UUID | 
| --- | --- | 
| service | 000000df-0000-0000fd-0000-400882088200 |
| features | 230031df-0220-00fe-0000-400882088201 |
#
**Interaction data (written to feature 3) format:**
| StartData | SumData_1 | ... | SumData_4 | CMD | Data_len | Data_1 | ... | Data_8 | EndData |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0xFF | 0x??? | 0x??? | 0x??? | 0x??? | 0x??? | 0x??? | 0x? | 0x??? | 0xFF |
| Data start | Check sum bits 7 and 8 | Check sum bits 3, 4, 5 and 6 | Check sum bits 1 and 2 | Command bits | Data length bits | Data bit 1 | Data bits 2, 3, 4, 5, 6 and 7 | Data bit 8 | Data end |

#
**Explanation:**

 Example of data interaction: After the app side connects to the device, it needs to write data to the feature value according to the format (CMD: 0xC3) to start the device data reporting, and the app side turns on the listening to the feature value at the same time, the device will write a notification to the feature value (CMD: 0xB3, Data_len: 0x03, Data_1: attribute data 1, Data_2: attribute data 2, and The app needs to parse the received data and display it to the UI.
#
| command | value | description |
| --- | --- | --- |
| APP ---> Device |
| NONE | 0x00 | No command |
| CHECKVERSION | 0xC1 | Query the version number |
| USETIMES | 0xC2 | Query the number of uses |
| STARTUPDATE | 0xC3 | Attribute reporting |
| STARTOTA | 0xC4 | Start OTA |
| SETDATA | 0xC5 | Set Data |
| STARTFIX | 0xC6 | Turn on maintenance mode |

| APP <-- Device |
| VERSION | 0xB1 | Returns the version information |
| TIMES | 0xB2 | Returns the number of uses |
| UPDATE | 0xB3 | Returns the device properties |
| OTA | 0xB4 | Returns the OTA status of the device.
| DATA | 0xB5 | The return value is whether the data was successful or not.
| FIX | 0xB6 | The return value is the repair mode number and whether it was successful or not.

**Detail:**

- The app sends the command to enable attribute reporting.
- For example: 
-open is 01, close is 00, after converting to hexadecimal is 01 and 00, then
APP sends the command as follows 
```
FF 00 00 00 C5 C3 01 01 00 00 00 00 00 00 00 FF (on)
FF 00 00 00 C4 C3 01 00 00 00 00 00 00 00 FF (off)
```

- To start the OTA you need to bring the firmware file size.
- For example:
- Firmware size is 1,565,792 bytes, after converting to hexadecimal it is 17E460, then
The command sent by APP is 
```
FF 00 17 E5 27 C4 03 17 E4 60 00 00 00 00 00 00 FF
```

- To set the data, you need to bring the data number and data content.
- For example:
- The target data is the first data and the data content is 2, then
APP sends the command as follows 
```
FF 00 00 00 CA C5 02 01 02 00 00 00 00 00 00 00 00 FF
```

- The version information in the command returned by the device.
- For example:
- Version 1.2.3.dev, converted to hexadecimal: 01020300 (development version is 00, official version is 01)
The data returned by the device is
```
FF 00 00 00 00 BB B1 04 01 02 03 00 00 00 00 00 00 00 FF
```

- Number of uses.
 - For example:
 - Mode 1 was used 26535 times, Mode 2 was used 235 times, and Mode 3 was used 116535 times.
The mode numbers are 1, 2, 3, converted to hexadecimal 167A7, 2EB, 31C737, then
The data returned by the device is as follows
```
FF 00 00 01 C4 B2 03 01 67 A7 00 00 00 00 00 FF (Mode 1)
FF 00 00 01 A1 B2 02 02 EB 00 00 00 00 00 00 FF (mode 2)
FF 00 00 01 E8 B2 04 03 31 C7 37 00 00 00 FF (Mode 3)
```

- Device properties. 
- For example:
- Attribute 1 is 10, Attribute 2 is 3, and Attribute 3 is 2. After converting to hexadecimal, A0302, then
The data returned by the device would be
```
FF 00 00 00 12 B3 03 0A 03 02 00 00 00 00 00 FF
```

- Status of the device OTA.
- Ready for OTA 00 
- Accepted firmware percentage 01 
- Accepted firmware complete 02
- OTA successful 03
- OTA error (1-4) 04 - 07
- The data returned by the device is:
```
FF 00 00 00 B5 B4 01 00 00 00 00 00 00 00 00 FF (OTA ready)
FF 00 00 00 B7 B4 02 01 60 00 00 00 00 00 00 FF (96% of firmware accepted)
FF 00 00 00 B7 B4 01 02 00 00 00 00 00 00 FF (Acceptance of firmware complete)
FF 00 00 00 B8 B4 01 03 00 00 00 00 00 00 FF (OTA successful)
FF 00 00 00 B9 B4 01 04 00 00 00 00 00 00 FF (OTA error 1)
FF 00 00 00 BA B4 01 05 00 00 00 00 00 00 00 FF (OTA error 2)
FF 00 00 00 BB B4 01 06 00 00 00 00 00 00 00 FF (OTA error 3)
FF 00 00 00 BC B4 01 07 00 00 00 00 00 00 00 00 FF (OTA error 4)