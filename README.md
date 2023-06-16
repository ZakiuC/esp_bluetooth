#
ble MTU大小为512，包括以下服务和特征：
|  | UUID | 
| --- | --- | 
| 服务 | 000000df-0000-00fd-0000-400882088200 |
| 特征 | 230031df-0220-00fe-0000-400882088201 |
#
**交互数据（写入特征3）格式：**
| StartData | SumData_1 | ... | SumData_4 | CMD | Data_len | Data_1 | ... | Data_8 | EndData |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0xFF | 0x?? | 0x?? | 0x?? | 0x?? | 0x?? | 0x?? | 0x?? | 0x?? | 0xFF |
| 数据开始 | 检验和的第7、8位 | 检验和的第3、4、5、6位 | 检验和的第1、2位 | 命令位 | 数据长度位 | 数据位1 | 数据位2、3、4、5、6、7 | 数据位8 | 数据结束 |

#
**说明:**

 数据交互举例：在app端连接上设备后，需要向特征值按格式写入数据（CMD: 0xC3）以启动设备数据上报，app端同时开启对特征值的监听，设备会往特征值写入通知（CMD: 0xB3，Data_len: 0x03，Data_1: 属性数据1，Data_2: 属性数据2，Data_3: 属性3）。App需要解析接收到的数据并显示到UI上。
#
| 命令 | 值 | 描述 |
| --- | --- | --- |
| APP ---> 设备 |
| NONE | 0x00 | 无命令 |
| CHECKVERSION | 0xC1 | 查询版本号 |
| USETIMES | 0xC2 | 查询使用次数 |
| STARTUPDATE | 0xC3 | 属性上报 |
| STARTOTA | 0xC4 | 开始OTA |
| SETDATA | 0xC5 | 设置数据 |
| STARTFIX | 0xC6 | 开启维修模式 |

| APP <--- 设备 |
| VERSION | 0xB1 | 返回的是版本信息 |
| TIMES | 0xB2 | 返回的是使用次数 |
| UPDATE | 0xB3 | 返回的是设备属性 |
| OTA | 0xB4 | 返回的是设备OTA的状态 |
| DATA | 0xB5 | 返回的是数据是否成功 |
| FIX | 0xB6 | 返回的是维修模式号和是否成功 |

**详细说明:**

- app发送的命令中 开启属性上报:
- 例如： 
- - 开启为 01，关闭为 00，转为十六进制后为 01 和 00，那么
APP发送的命令即为： 
```
FF 00 00 00 C5 C3 01 01 00 00 00 00 00 00 FF (开启)
FF 00 00 00 C4 C3 01 00 00 00 00 00 00 00 FF (关闭)
```

- 开始OTA需要带上固件文件大小:
- 例如：
- - 固件大小为 1,565,792字节，转为十六进制后为 17E460,那么
APP发送的命令即为： 
```
FF 00 17 E5 27 C4 03 17 E4 60 00 00 00 00 00 FF
```

- 设置数据需要带上数据号和数据内容:
- 例如：
- - 目标数据是第一位数据，数据内容是2，那么
APP发送的命令即为： 
```
FF 00 00 00 CA C5 02 01 02 00 00 00 00 00 00 FF
```

- 设备返回的命令中，版本信息:
- 例如：
- - 版本为1.2.3.dev, 转换成十六进制后为： 01020300 (开发版本为 00，正式版本为 01)
设备返回的数据即为：
```
FF 00 00 00 BB B1 04 01 02 03 00 00 00 00 00 FF
```

- 使用次数:
 - 例如：
 - - 模式一使用了 26535次，模式二使用了 235次，模式三使用了 116535次，
模式号分别为1，2，3，转换成十六进制后分别为 167A7, 2EB, 31C737,那么
设备返回的数据即为：
```
FF 00 00 01 C4 B2 03 01 67 A7 00 00 00 00 FF (模式一)
FF 00 00 01 A1 B2 02 02 EB 00 00 00 00 00 FF (模式二)
FF 00 00 01 E8 B2 04 03 31 C7 37 00 00 00 FF (模式三)
```

- 设备属性: 
- 例如：
- - 属性一是 10，属性二是 3，属性三是 2，转为十六进制后 A0302，那么
设备返回的数据即为：
```
FF 00 00 00 12 B3 03 0A 03 02 00 00 00 00 FF
```

- 设备OTA的状态:
- - 已准备OTA	00 
- - 已接受固件百分比	01 
- - 接受固件完成	02
- - OTA成功		03
- - OTA报错(1-4) 04 - 07
- 设备返回的数据即为：
```
FF 00 00 00 B5 B4 01 00 00 00 00 00 00 00 FF (已准备OTA)
FF 00 00 00 B7 B4 02 01 60 00 00 00 00 00 FF (已接受固件百分比96%)
FF 00 00 00 B7 B4 01 02 00 00 00 00 00 00 FF (接受固件完成)
FF 00 00 00 B8 B4 01 03 00 00 00 00 00 00 FF (OTA成功)
FF 00 00 00 B9 B4 01 04 00 00 00 00 00 00 FF (OTA报错1)
FF 00 00 00 BA B4 01 05 00 00 00 00 00 00 FF (OTA报错2)
FF 00 00 00 BB B4 01 06 00 00 00 00 00 00 FF (OTA报错3)
FF 00 00 00 BC B4 01 07 00 00 00 00 00 00 FF (OTA报错4)