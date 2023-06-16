#include "esp_bt.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"


#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"

#include "blue_app.h"
#include <arpa/inet.h>
#include <stdarg.h>

// for ota
#include "errno.h"
#include "esp_flash_partitions.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

/* deviceId : 84:F7:03:50:91:76 */
/**************************UUID*************************/
/* 服务 UUID: 000000df-0000-00fd-0000-400882088200 */
static uint8_t service_uuid[16] = {
	/* 低位 <--------------------------------------------------------------------------------> 高位 */
	0x00,
	0x82,
	0x08,
	0x82,
	0x08,
	0x40,
	0x00,
	0x00,
	0xfd,
	0x00,
	0x00,
	0x00,
	0xdf,
	0x00,
	0x00,
	0x00,
};

/*特征：命令 UUID: 000000df-0000-00fe-0000-400882088201*/
static uint8_t char_cmd_uuid[16] = {
	/* 低位 <--------------------------------------------------------------------------------> 高位 */
	0x01,
	0x82,
	0x08,
	0x82,
	0x08,
	0x40,
	0x00,
	0x00,
	0xfe,
	0x00,
	0x00,
	0x00,
	0xdf,
	0x00,
	0x00,
	0x00,
};
/*特征：ota接收数据 UUID: 010203df-0000-00fe-0000-400882088202*/
static uint8_t char_ota_data_uuid[16] = {
	/* 低位 <--------------------------------------------------------------------------------> 高位 */
	0x02,
	0x82,
	0x08,
	0x82,
	0x08,
	0x40,
	0x00,
	0x00,
	0xfe,
	0x00,
	0x00,
	0x00,
	0xdf,
	0x03,
	0x02,
	0x01,
};
/*特征：用户app UUID: 230031df-0220-00fe-0000-400882088201*/
static uint8_t char_usr_uuid[16] = {
	/* 低位 <--------------------------------------------------------------------------------> 高位 */
	0x01,
	0x82,
	0x08,
	0x82,
	0x08,
	0x40,
	0x00,
	0x00,
	0xfe,
	0x00,
	0x20,
	0x02,
	0xdf,
	0x31,
	0x00,
	0x23,
};
/************************** 私有宏 *************************/
#define LOG_TAG "BLE_LOG"
#define OTA_ERROR_TAG "OTA_ERROR"
#define OTA_TAG "OTA_LOG"
#define PROFILE_NUM 1
#define PROFILE_APP_IDX 0
#define ESP_APP_ID 0x66
#define SVC_INST_ID 0
#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500 // 特征值的最大长度。当GATT客户端执行写或准备写操作时。数据长度必须小于GATTS_DEMO_CHAR_VAL_LEN_MAX。
#define TASK_STACK_SIZE 1024 * 4		//  测试任务栈大小
#define UPDATE_TASK_STACK_SIZE 1024 * 4		//  测试任务栈大小
#define PROCESS_TASK_STACK_SIZE 1024 * 4		//  测试任务栈大小

/************************** 变量声明 *************************/
static uint8_t adv_config_done = 0;
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t test_value[] = "I'm test_value.";
static const uint8_t heart_measurement_ccc[2] = {0x00, 0x00};

uint16_t heart_rate_handle_table[HRS_IDX_NB];

static bleController_t *bleCtrler;
TaskHandle_t upDateTaskHandle;

/***** ota相关 *****/
esp_err_t err;
/* 更新句柄：由esp_ota_begin()设置，必须通过esp_ota_end()释放。 */
esp_ota_handle_t update_handle = 0;
const esp_partition_t *update_partition = NULL;
SemaphoreHandle_t semaphore;

/************************** 函数声明 *************************/
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void DefaultConfig(bleController_t *bleController);
static void BleDataProcess(esp_ble_gatts_cb_param_t *param, uint8_t *cmd, uint8_t *data);
static void bleSend(uint8_t cmd, uint8_t dataLen, uint8_t data[]);
static void dataHandle(void);

/************************** gatt配置 *************************/
struct gatts_profile_inst
{
	esp_gatts_cb_t gatts_cb;
	uint16_t gatts_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t char_handle;
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
};

/* 一个基于gatt的配置文件一个app_id和一个gatts_if，这个数组将存储由ESP_GATTS_REG_EVT返回的gatts_if。 */
static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
	[PROFILE_APP_IDX] = {
		.gatts_cb = gatts_profile_event_handler,
		/* 没有得到gatt_if，所以初始是ESP_GATT_IF_NONE */
		.gatts_if = ESP_GATT_IF_NONE,
	},
};

/* adv数据的长度必须小于31字节。*/
static esp_ble_adv_data_t adv_data = {
	.set_scan_rsp = false,
	.include_name = true,
	.include_txpower = true,
	.min_interval = 0x0006, // 从属连接最小间隔，时间=最小间隔*1.25毫秒
	.max_interval = 0x0010, // 从属连接最大间隔，时间=最大间隔*1.25毫秒
	.appearance = 0x00,
	.manufacturer_len = 0,		 // TEST_MANUFACTURER_DATA_LEN,
	.p_manufacturer_data = NULL, // test_manufacturer,
	.service_data_len = 0,
	.p_service_data = NULL,
	.service_uuid_len = sizeof(service_uuid),
	.p_service_uuid = service_uuid,
	.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
	.set_scan_rsp = true,
	.include_name = true,
	.include_txpower = true,
	.min_interval = 0x0006,
	.max_interval = 0x0010,
	.appearance = 0x00,
	.manufacturer_len = 0,		 // TEST_MANUFACTURER_DATA_LEN,
	.p_manufacturer_data = NULL, //&test_manufacturer[0],
	.service_data_len = 0,
	.p_service_data = NULL,
	.service_uuid_len = sizeof(service_uuid),
	.p_service_uuid = service_uuid,
	.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// 广播参数
static esp_ble_adv_params_t adv_params = {
	.adv_int_min = 0x20, //   min_int = 0x20*1.25ms = 40ms
	.adv_int_max = 0x40, //   max_int = 0x40*1.25ms = 80ms
	.adv_type = ADV_TYPE_IND,
	.own_addr_type = BLE_ADDR_TYPE_PUBLIC,
	.channel_map = ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/************************** 特征表 *************************/
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
	{
		/* 服务声明 */
		[IDX_SVC] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(service_uuid), sizeof(service_uuid), (uint8_t *)&service_uuid}},

		/* 特征声明 */
		[IDX_CHAR_A] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

		/* 特征值 */
		[IDX_CHAR_VAL_A] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_cmd_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(test_value), (uint8_t *)test_value}},

		/* 客户端特征配置描述值 */
		[IDX_CHAR_CFG_A] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

		/* 特征声明 */
		[IDX_CHAR_B] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

		/* 特征值 */
		[IDX_CHAR_VAL_B] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_ota_data_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(test_value), (uint8_t *)test_value}},

		/* 特征声明 */
		[IDX_CHAR_C] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

		/* 特征值 */
		[IDX_CHAR_VAL_C] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_usr_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(test_value), (uint8_t *)test_value}},

		/* 客户端特征配置描述值 */
		[IDX_CHAR_CFG_C] =
			{{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},
};

/************************** 命令检测线程 *************************/
// static void checkCmd_task(void *arg)
// {
//    while(1)
//    {
//         vTaskDelay(1/portTICK_RATE_MS);
//    }
// }

/************************** ble初始化 ************************
 * name: 蓝牙名称
 * 蓝牙接收命令查看 BleCmd(全局可访问)
 */
void ble_Init(bleController_t *bleController, char *name, uint8_t *(*funcGetVersion)(void), uint8_t *(*funcGetTimesMode1)(void), uint8_t *(*funcGetTimesMode2)(void), uint8_t *(*funcGetTimesMode3)(void), uint8_t *(*funcGetAttr)(void))
{
	memset(bleController, 0, sizeof(*bleController));
	// 填入获取版本函数
	bleCtrler->funcGetVersion = funcGetVersion;
	// 填入获取数据函数
	bleController->funcGetTimesMode1 = funcGetTimesMode1;
	bleController->funcGetTimesMode2 = funcGetTimesMode2;
	bleController->funcGetTimesMode3 = funcGetTimesMode3;
	// 填入获取属性函数
	bleController->funcGetAttr = funcGetAttr;
	bleController->name = name;
	DefaultConfig(bleController);
	esp_err_t ret = nvs_flash_init();
	/* 初始化 NVS. */
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	/* 释放 ESP_BT_MODE_CLASSIC_BT，就是释放经典蓝牙资源，保证设备不工作在经典蓝牙下面： */
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	/* 初始化蓝牙控制器，此函数只能被调用一次，且必须在其他蓝牙功能被调用之前调用 */
	ret = esp_bt_controller_init(&bt_cfg);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
		return;
	}
	/* 使能蓝牙控制器，工作在 BLE mode:
	如果想要动态改变蓝牙模式不能直接调用该函数，先disable关闭蓝牙再使用该API来改变蓝牙模式 */
	ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
		return;
	}
	/* 初始化蓝牙主机，使能蓝牙主机：
	蓝牙栈 `bluedroid stack` 包括了BT和 BLE 使用的基本的define和API */
	ret = esp_bluedroid_init();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s c failed: %s", __func__, esp_err_to_name(ret));
		return;
	}
	ret = esp_bluedroid_enable();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
		return;
	}

	/* 注册 GATT 回调函数 */
	ret = esp_ble_gatts_register_callback(gatts_event_handler);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "gatts register error, error code = %x", ret);
		return;
	}

	/* 注册 GAP 回调函数
		这个回调函数中定义了在广播期间蓝牙设备的一些操作： */
	ret = esp_ble_gap_register_callback(gap_event_handler);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "gap register error, error code = %x", ret);
		return;
	}

	/* 注册 service
	   当调用esp_ble_gatts_app_register()注册一个应用程序Profile(Application Profile)，
	   将触发ESP_GATTS_REG_EVT事件，除了可以完成对应profile的gatts_if的注册,F
	   还可以调用esp_bel_create_attr_tab()来创建profile Attributes 表
	   或创建一个服务esp_ble_gatts_create_service() */
	ret = esp_ble_gatts_app_register(ESP_APP_ID);
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "gatts app register error, error code = %x", ret);
		return;
	}

	/* 设置 mtu ，mtu 相关说明如下：
		MTU: MAXIMUM TRANSMISSION UNIT
		最大传输单元，指在一个PDU 能够传输的最大数据量(多少字节可以一次性传输到对方)。
		PDU：Protocol Data Unit
		协议数据单元,在一个传输单元中的有效传输数据。 */
	esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(ESP_GATT_MAX_MTU_SIZE); /*mtusize大于255时，将gatts_profile_event_handler ESP_GATTS_WRITE_EVT中
																				  */
	if (local_mtu_ret)
	{
		ESP_LOGE(LOG_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
	}

	/* 创建命令检测线程 */
	// xTaskCreate(checkCmd_task, "checkCmd_task", TASK_STACK_SIZE, NULL, 10, NULL);
}

/*  参数说明：
	event:
	esp_gatts_cb_event_t 枚举类型，表示调用该回调函数时的事件(或蓝牙的状态)

	gatts_if:
	esp_gatt_if_t (uint8_t) 这是GATT访问接口类型，
	通常在GATT客户端上不同的应用程序用不同的gatt_if(不同的Application profile对应不同的gatts_if) ，
	调用esp_ble_gatts_app_register()时，
	注册Application profile 就会有一个gatts_if。

	param: esp_ble_gatts_cb_param_t 指向回调函数的参数，是个联合体类型，
	不同的事件类型采用联合体内不同的成员结构体。
	这个函数的主要作用：导入 GATT 的 profiles。*/
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	/* 判断是否是 GATT 的注册事件 */
	if (event == ESP_GATTS_REG_EVT)
	{
		/* 确定底层GATT运行成功触发ESP_GATTS_REG_EVT时，完成对每个profile 的gatts_if 的注册*/
		if (param->reg.status == ESP_GATT_OK)
		{
			heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
		}
		else
		{
			ESP_LOGE(LOG_TAG, "注册app, app_id: %04x, 状态: %d", param->reg.app_id, param->reg.status);
			return;
		}
	}
	/* 如果gatts_if == 某个Profile的gatts_if时，调用对应profile的回调函数处理事情。 */
	do
	{
		int idx;
		for (idx = 0; idx < PROFILE_NUM; idx++)
		{
			/* ESP_GATT_IF_NONE，不指定某个gatt_if，需要调用每个配置文件的cb函数 */
			if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if)
			{
				if (heart_rate_profile_tab[idx].gatts_cb)
				{
					heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
				}
			}
		}
	} while (0);
}

/****************************** gatts事件处理 *******************************/
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	switch (event)
	{
		/* 一个Service的创建，包括GATT注册事件，添加 service的基本信息，设置BLE名称 */
		case ESP_GATTS_REG_EVT:
		{ // gatt注册事件
			ESP_LOGI(LOG_TAG, "通用属性协议 注册");
			esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(bleCtrler->name);
			if (set_dev_name_ret)
			{
				ESP_LOGE(LOG_TAG, "设置设备名失败, 错误代码 = %x", set_dev_name_ret);
			}
			esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
			if (ret)
			{
				ESP_LOGE(LOG_TAG, "配置广播数据失败, 错误代码 = %x", ret);
			}
			adv_config_done |= ADV_CONFIG_FLAG;
			ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
			if (ret)
			{
				ESP_LOGE(LOG_TAG, "配置advu数据失败, 错误代码 = %x", ret);
			}
			adv_config_done |= SCAN_RSP_CONFIG_FLAG;
			esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
			if (create_attr_ret)
			{
				ESP_LOGE(LOG_TAG, "创建特征表失败, 错误代码 = %x", create_attr_ret);
			}
		}
		break;
		case ESP_GATTS_READ_EVT: // GATT读取事件，手机读取开发板的数据
			if (heart_rate_handle_table[IDX_CHAR_VAL_A] == param->read.handle)
			{
				ESP_LOGI(LOG_TAG, "特征值 A 被读");
			}
			if (heart_rate_handle_table[IDX_CHAR_VAL_B] == param->read.handle)
			{
				ESP_LOGI(LOG_TAG, "特征值 B 被读");
			}
			if (heart_rate_handle_table[IDX_CHAR_VAL_C] == param->read.handle)
			{
				ESP_LOGI(LOG_TAG, "特征值 C 被读");
			}
				break;
		case ESP_GATTS_WRITE_EVT: // GATT写事件，手机给开发板的发送数据，不需要回复
			if (!param->write.is_prep)
			{
				if (heart_rate_handle_table[IDX_CHAR_VAL_C] == param->write.handle)
				{
					/* 需要看接收到的原始数据开下方注释 */
					// uint8_t rx_Data[param->write.len];
					// printf("接收数据（十六进制）: ");
					// for (int a = 0; a < param->write.len; a++)
					// {
					// 	rx_Data[a] = param->write.value[a];
					// 	printf(" 0x%02x ", rx_Data[a]);
					// }
					// printf("\n");
					/** 处理接收数据 **/
					BleDataProcess(param, &bleCtrler->data.CMD, &bleCtrler->data.PropertyData_1);
					dataHandle();
				}
				if (heart_rate_handle_table[IDX_CHAR_VAL_A] == param->write.handle)
				{
					/* 需要看接收到的原始数据开下方注释 */
					// uint8_t rx_Data[param->write.len];
					// printf("接收数据（十六进制）: ");
					// for(int a=0;a< param->write.len;a++)
					// {
					//     rx_Data[a] = param->write.value[a];
					//     printf(" 0x%02x ", rx_Data[a]);
					// }
					// printf("\n");
					/** 处理接收数据 **/
					/** 更新gatts_if和conn_id **/
				}
				// OTA固件写入特征值
				if (heart_rate_handle_table[IDX_CHAR_VAL_B] == param->write.handle)
				{
					uint16_t length = param->write.len; // 当mtu大于255时，将uint8_t修改为uint16_t
					bleCtrler->file.TotalLen += length;
					bleCtrler->file.ReceivingProgress = (int)(((float)bleCtrler->file.TotalLen / (float)bleCtrler->file.TotalFile) * 100);
					if (bleCtrler->file.LastProgress != bleCtrler->file.ReceivingProgress)
					{
						ESP_LOGI(OTA_TAG, "OTA固件已接收 %d%%数据", bleCtrler->file.ReceivingProgress);
						// 发送通知，OTA接收固件进度
						uint8_t data[2] = {OTARATE, (uint8_t)bleCtrler->file.ReceivingProgress};
						bleSend(CMD_OTA, 2, data);
					}
					if (bleCtrler->file.ReceivingProgress == 100)
					{
						ESP_LOGI(OTA_TAG, "OTA固件已接受完成");
						// 发送通知，OTA文件已接收完毕
						uint8_t data = OTAOVERFILE;
						bleSend(CMD_OTA, 1, &data);
					}
					err = esp_ota_write(update_handle, (const void *)param->write.value, length);
					if (err != ESP_OK)
					{
						esp_ota_abort(update_handle);
						ESP_LOGI(OTA_TAG, "OTA数据写入错误!");
						// 发送通知，OTA报错
						uint8_t data = OTAERROR_1;
						bleSend(CMD_OTA, 1, &data);
					}
					bleCtrler->file.LastProgress = bleCtrler->file.ReceivingProgress;
				}
			}
			break;
		case ESP_GATTS_EXEC_WRITE_EVT: // GATT写事件，手机给开发板的发送数据，需要回复
			ESP_LOGI(LOG_TAG, "gatt 被写");
			break;
		case ESP_GATTS_MTU_EVT:
			ESP_LOGI(LOG_TAG, "gatt MTU交换, MTU %d", param->mtu.mtu);
			break;
		case ESP_GATTS_CONF_EVT:
			// 打开注释即可看到每次更新的通知
			// ESP_LOGI(LOG_TAG, "gatt 修改, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
			break;
		case ESP_GATTS_START_EVT:
			ESP_LOGI(LOG_TAG, "gatt 服务开始, status %d, service_handle %d", param->start.status, param->start.service_handle);
			break;
		case ESP_GATTS_CONNECT_EVT: // GATT连接事件
			ESP_LOGI(LOG_TAG, "gatt 连接, conn_id = %d", param->connect.conn_id);
			esp_log_buffer_hex("连接设备MAC地址:", param->connect.remote_bda, 6);
			esp_ble_conn_update_params_t conn_params = {0};
			memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
			/* 对于iOS系统，请参考苹果官方文件中关于BLE连接参数的限制。
				● Interval Max * (Slave Latency + 1) ≤ 2 seconds
				● Interval Min ≥ 20 ms
				● Interval Min + 20 ms ≤ Interval Max Slave Latency ≤ 4
				● connSupervisionTimeout ≤ 6 seconds
				● Interval Max * (Slave Latency + 1) * 3 < connSupervisionTimeout*/
			conn_params.latency = 0;
			conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
			conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
			conn_params.timeout = 1000; // timeout = 400*10ms = 4000ms
			// 开始向对等设备发送更新连接参数。
			esp_ble_gap_update_conn_params(&conn_params);
			break;
		case ESP_GATTS_DISCONNECT_EVT:
			ESP_LOGI(LOG_TAG, "gatt 断开, 原因 = 0x%x", param->disconnect.reason);
			esp_ble_gap_start_advertising(&adv_params);
			break;
		case ESP_GATTS_CREAT_ATTR_TAB_EVT:
		{ // GATT创建事件,包含基本参数的设置，将Characteristic加到service中，完成触发下面事件
			if (param->add_attr_tab.status != ESP_GATT_OK)
			{
				ESP_LOGE(LOG_TAG, "创建特征表失败, 错误代码=0x%x", param->add_attr_tab.status);
			}
			else if (param->add_attr_tab.num_handle != HRS_IDX_NB)
			{
				ESP_LOGE(LOG_TAG, "创建特征表异常, num_handle (%d) \
                            不等于 HRS_IDX_NB(%d)",
						 param->add_attr_tab.num_handle, HRS_IDX_NB);
			}
			else
			{
				ESP_LOGI(LOG_TAG, "创建特征表成功, number handle = %d\n", param->add_attr_tab.num_handle);
				memcpy(heart_rate_handle_table, param->add_attr_tab.handles, sizeof(heart_rate_handle_table));
				esp_ble_gatts_start_service(heart_rate_handle_table[IDX_SVC]);
			}
			break;
		}
		case ESP_GATTS_STOP_EVT:
		case ESP_GATTS_OPEN_EVT:
		case ESP_GATTS_CANCEL_OPEN_EVT:
		case ESP_GATTS_CLOSE_EVT:
		case ESP_GATTS_LISTEN_EVT:
		case ESP_GATTS_CONGEST_EVT:
		case ESP_GATTS_UNREG_EVT:
		case ESP_GATTS_DELETE_EVT:
		default:
			break;
	}
}

/*********************** gap事件处理 *********************************/
/*其中开始广播 adv_params 的参数定义见上方 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	switch (event)
	{
		case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 广播数据设置完成事件标志
			adv_config_done &= (~ADV_CONFIG_FLAG);
			if (adv_config_done == 0)
			{
				esp_ble_gap_start_advertising(&adv_params); // 开始广播
			}
			break;
		case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: // 广播扫描相应设置完成标志
			adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
			if (adv_config_done == 0)
			{
				esp_ble_gap_start_advertising(&adv_params);
			}
			break;
		case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: // 开始广播事件标志
			/* 广播启动完成事件，表示广播启动成功或失败 */
			if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
			{
				ESP_LOGE(LOG_TAG, "广播启动失败");
			}
			else
			{
				ESP_LOGI(LOG_TAG, "广播启动成功");
			}
			break;
		case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: // 停止广播事件标志
			if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
			{
				ESP_LOGE(LOG_TAG, "停止广播失败");
			}
			else
			{
				ESP_LOGI(LOG_TAG, "停止广播成功\n");
			}
			break;
		case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: // 设备连接事件,可获取当前连接的设备信息
			ESP_LOGI(LOG_TAG, "连接设备 状态 = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
					 param->update_conn_params.status,
					 param->update_conn_params.min_int,
					 param->update_conn_params.max_int,
					 param->update_conn_params.conn_int,
					 param->update_conn_params.latency,
					 param->update_conn_params.timeout);
			break;
		default:
			break;
	}
}

/*
 * 接收文件前执行
 * ota命令: 0x03 */
void ota_ready(void)
{
	ESP_LOGI(OTA_TAG, "======开始OTA======");
	update_partition = esp_ota_get_next_update_partition(NULL);
	// 发送通知，OTA已准备开始接收文件
	bleSend(CMD_OTA, 1, OTAREADY);
	ESP_LOGI(OTA_TAG, "写入偏移量为 0x%x的分区子类型%d, 等待发送文件 文件大小为 %.f Byte",
			 update_partition->subtype, update_partition->address, (float)bleCtrler->file.TotalFile);
	assert(update_partition != NULL);
	err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(OTA_ERROR_TAG, "OTA开启失败 (%s)", esp_err_to_name(err));
		esp_ota_abort(update_handle);
	}
}


/*
 * 接收文件结束后执行
 * 命令: 0x04 */
void ota_start(void)
{
	bleCtrler->file.TotalLen = 0; // 接收数据长度计数重置
	ESP_LOGI(OTA_TAG, "======OTA数据接收结束======");
	err = esp_ota_end(update_handle);
	if (err != ESP_OK)
	{
		if (err == ESP_ERR_OTA_VALIDATE_FAILED)
		{
			// 发送通知，OTA报错
			uint8_t data = OTAERROR_2;
			bleSend(CMD_OTA, 1, &data);
			ESP_LOGE(OTA_ERROR_TAG, "数据验证失败，数据已损坏");
		}
		// 发送通知，OTA报错
		uint8_t data = OTAERROR_3;
		bleSend(CMD_OTA, 1, &data);
		ESP_LOGE(OTA_ERROR_TAG, "OTA结束失败 (%s)!", esp_err_to_name(err));
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK)
	{
		// 发送通知，OTA报错
		uint8_t data = OTAERROR_4;
		bleSend(CMD_OTA, 1, &data);
		ESP_LOGE(OTA_ERROR_TAG, "OTA设置boot分区失败 (%s)!", esp_err_to_name(err));
	}
	// 发送通知，OTA成功
	uint8_t data = OTASUCESS;
	bleSend(CMD_OTA, 1, &data);
	ESP_LOGI(OTA_TAG, "准备重启系统!");
	esp_restart();
	return;
}


/**
 * @brief    等待OTA线程    
 * @param    (void) *arg : 
 * @return   (void)
 */
static void waitOTA_task(void *arg)
{
	ESP_LOGI(OTA_TAG, "OTA线程已开启");
	while (1)
	{
		if (bleCtrler->bleMode == OTA)
		{
			ota_ready();
			ESP_LOGI(OTA_TAG, "请等待文件发送完毕");
			while (bleCtrler->file.ReceivingProgress != 100)
			{
				vTaskDelay(100 / portTICK_RATE_MS);
			}
			ota_start();
			vTaskDelete(NULL);
		}
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}


/**
 * @brief    属性上传线程
 * @param    (void) *arg : 
 * @return   (void)
 */
static void UpDate_task(void *arg)
{
	ESP_LOGI(LOG_TAG, "UpDate线程已开启");
	uint8_t *attr;
	while (1)
	{
		// xSemaphoreTake(semaphore, portMAX_DELAY);
		// 1s上报一次属性
		if (bleCtrler->bleMode == APP)
		{
			// 获取数据
			attr = bleCtrler->funcGetAttr();
			bleSend(CMD_UPDATE, 3, attr);
		}
		else
		{
			ESP_LOGI(LOG_TAG, "mode错误");
		}
		// xSemaphoreGive(semaphore);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}


/**
 * @brief    初始化配置参数
 * @param    (bleController_t) *bleController :
 * @return   (void)
 */
static void DefaultConfig(bleController_t *bleController)
{
	bleCtrler = bleController;  
	bleCtrler->bleMode = APP;
	bleCtrler->fixMode = NOMODE;
	bleCtrler->data.CMD = 0;
	bleCtrler->data.DataLen = 0;
	bleCtrler->data.PropertyData_1 = 0;
	bleCtrler->data.PropertyData_2 = 0;
	bleCtrler->data.PropertyData_3 = 0;
	bleCtrler->data.PropertyData_4 = 0;
	bleCtrler->data.PropertyData_5 = 0;
	bleCtrler->data.PropertyData_6 = 0;
	bleCtrler->data.PropertyData_7 = 0;
	bleCtrler->data.PropertyData_8 = 0;
	bleCtrler->data.SUM = 0;
	bleCtrler->file.TotalLen = 0;
	bleCtrler->file.LastProgress = 0;
	bleCtrler->file.ReceivingProgress = 0;
	bleCtrler->file.TotalFile = 0;
	bleCtrler->fixModeId = 0;
}

/**
 * @brief    关闭时参数恢复
 * @param    (void)
 * @return   (void)
 */
static void CloseConfig(void)
{
	bleCtrler->bleMode = CLOSE;
	bleCtrler->fixMode = NOMODE;
	bleCtrler->data.CMD = 0;
	bleCtrler->data.DataLen = 0;
	bleCtrler->data.PropertyData_1 = 0;
	bleCtrler->data.PropertyData_2 = 0;
	bleCtrler->data.PropertyData_3 = 0;
	bleCtrler->data.PropertyData_4 = 0;
	bleCtrler->data.PropertyData_5 = 0;
	bleCtrler->data.PropertyData_6 = 0;
	bleCtrler->data.PropertyData_7 = 0;
	bleCtrler->data.PropertyData_8 = 0;
	bleCtrler->data.SUM = 0;
	bleCtrler->file.TotalLen = 0;
	bleCtrler->file.LastProgress = 0;
	bleCtrler->file.ReceivingProgress = 0;
	bleCtrler->file.TotalFile = 0;
}

/*
 *  关闭蓝牙
 *  描述：关闭蓝牙和蓝牙控制器
 */
void ble_close(void)
{
	esp_err_t ret;
	ret = esp_bluedroid_disable();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s 关闭蓝牙失败，错误: %s", __func__, esp_err_to_name(ret));
		return;
	}
	else
	{
		ESP_LOGI(LOG_TAG, "已成功关闭蓝牙");
	}
	ret = esp_bluedroid_deinit();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s 失能蓝牙失败，错误: %s", __func__, esp_err_to_name(ret));
		return;
	}
	else
	{
		ESP_LOGI(LOG_TAG, "已成功失能蓝牙");
	}
	ret = esp_bt_controller_disable();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s 关闭蓝牙控制器失败，错误: %s", __func__, esp_err_to_name(ret));
		return;
	}
	else
	{
		ESP_LOGI(LOG_TAG, "已成功关闭蓝牙控制器");
	}
	ret = esp_bt_controller_deinit();
	if (ret)
	{
		ESP_LOGE(LOG_TAG, "%s 失能蓝牙控制器失败，错误: %s", __func__, esp_err_to_name(ret));
		return;
	}
	else
	{
		ESP_LOGI(LOG_TAG, "已成功失能蓝牙控制器");
	}
	CloseConfig();
}

void endTest(uint8_t id, uint8_t data) 
{
	uint8_t datas[2] = {id, data};
	bleSend(CMD_FIX, 2, data);
}

static void dataHandle(void)
{
	// 测试数据
	uint8_t dataRd[1] = {1};
	dataFuncType_t funcs[] = {bleCtrler->funcGetTimesMode1, bleCtrler->funcGetTimesMode2, bleCtrler->funcGetTimesMode3};
	const char *messages[] = {"没有查询模式1使用次数的函数，请检查蓝牙初始化", "没有查询模式2使用次数的函数，请检查蓝牙初始化", "没有查询模式3使用次数的函数，请检查蓝牙初始化"};

	switch (bleCtrler->data.CMD)
	{
		// 获取版本
		case CMD_CHECKVERSION:
			if (bleCtrler->funcGetVersion != NULL)
			{
				uint8_t *version = bleCtrler->funcGetVersion();
				bleSend(CMD_VERSION, 4, version);
				ESP_LOGI(LOG_TAG, "固件版本号已上传");
			}else{
				ESP_LOGW(LOG_TAG, "没有查询版本号的函数，请检查初始化");
			}
			break;

		// 获取使用次数
		case CMD_USETIMES:	
			for (int i = 0; i < 3; i++)
			{
				if (funcs[i] != NULL)
				{
					uint8_t *times = funcs[i]();
					bleSend(CMD_TIMES, 5, times);
					ESP_LOGI(LOG_TAG, "各模式使用次数已上传");
				}
				else
				{
					ESP_LOGW(LOG_TAG, "%s", messages[i]);
				}
			}
			break;

		// 开始更新属性
		case CMD_STARTUPDATE:
			if (bleCtrler->data.PropertyData_1 == 0x01)
			{
				// 创建上报线程
				if (upDateTaskHandle == NULL)
				{
					// semaphore = xSemaphoreCreateBinary();
					// xSemaphoreGive(semaphore); // give semaphore initially
					xTaskCreate(UpDate_task, "UpDate_task", UPDATE_TASK_STACK_SIZE, NULL, 3, &upDateTaskHandle);
					ESP_LOGI(LOG_TAG, "属性上报线程已开启");
				}else{
					ESP_LOGW(LOG_TAG, "属性上报线程已开启,请勿重复开启");
				}
				// 开启成功反馈
			}
			else if (bleCtrler->data.PropertyData_1 == 0x00)
			{
				if (upDateTaskHandle != NULL)
				{
					// xSemaphoreTake(semaphore, portMAX_DELAY);
					vTaskDelete(upDateTaskHandle);
					upDateTaskHandle = NULL;
					// xSemaphoreGive(semaphore);
					// 关闭成功反馈
					ESP_LOGI(LOG_TAG, "属性上报线程已关闭");
				}else{
					ESP_LOGW(LOG_TAG, "属性上报线程已关闭,请勿重复关闭");
				}
				
			}
			else
			{
				// 反馈接收数据错误
				ESP_LOGI(LOG_TAG, "属性上报线程控制数据错误，请检查发送数据");
			}

			break;

		// 开始OTA
		case CMD_STARTOTA:
			// 更新蓝牙模式
			bleCtrler->bleMode = OTA;
			// 接收OTA文件大小
			bleCtrler->file.TotalFile |= (uint32_t)bleCtrler->data.PropertyData_1 << 24;
			bleCtrler->file.TotalFile |= (uint32_t)bleCtrler->data.PropertyData_2 << 16;
			bleCtrler->file.TotalFile |= (uint32_t)bleCtrler->data.PropertyData_3 << 8;
			bleCtrler->file.TotalFile |= (uint32_t)bleCtrler->data.PropertyData_4;
			// 创建OTA线程
			xTaskCreate(waitOTA_task, "waitOTA_task", TASK_STACK_SIZE, NULL, 3, NULL);
			break;

		// 设定数据
		case CMD_SETDATA:
			*(&bleCtrler->setting.data1 + bleCtrler->data.PropertyData_1 - 1) = bleCtrler->data.PropertyData_2;
			bleSend(CMD_DATA, 1, dataRd);
			ESP_LOGI(LOG_TAG, "接收设定数据: %d\n%d\n%d已更新本地数据", bleCtrler->setting.data1, bleCtrler->setting.data2, bleCtrler->setting.data3);
			break;

		case CMD_STARTFIX:
			bleCtrler->fixModeId = bleCtrler->data.PropertyData_1;
			break;
		default:
			break;
	}
}

/**
 * @brief    处理接收数据
 * @param    (esp_ble_gatts_cb_param_t) *param : 数据
 * @param    (uint8_t) *cmd : 存放解析完的命令
 * @param    (uint8_t) data : 存放解析完的数据
 * @return   (void)
 */
static void BleDataProcess(esp_ble_gatts_cb_param_t *param, uint8_t *cmd, uint8_t *data)
{
	Test_RxData_t Rxdata =
		{
			.index = 0,
			.len = param->write.len,
			.data.CMD = 0,
			.data.DataLen = 0,
			.data.PropertyData_1 = 0,
			.data.PropertyData_2 = 0,
			.data.PropertyData_3 = 0,
			.data.PropertyData_4 = 0,
			.data.PropertyData_5 = 0,
			.data.PropertyData_6 = 0,
			.data.PropertyData_7 = 0,
			.data.PropertyData_8 = 0,
			.startFlag = false,
			.startPosition = 0,
			.sumCheck = 0,
			.sumData = 0,
		};
	uint8_t Rxbuf[Rxdata.len];
	uint8_t i = 0;
	/* 将接收数据存入待处理缓存区 */
	if (Rxdata.len <= sizeof(Rxbuf) && param->write.value != NULL)
	{
		memcpy(Rxbuf, param->write.value, Rxdata.len);
	}
	else
	{
		ESP_LOGW(LOG_TAG, "接收数据出错，检查接收数据是否有内容");
	}

	/* 检查起始符以及记录起始符在接收数据中的位置 */
	for (i = 0; i < Rxdata.len; i++)
	{
		if (Rxbuf[i] == STARTDATA)
		{
			Rxdata.startFlag = true;
			Rxdata.startPosition = i;
			break;
		}
	}
	/* 处理接收数据 */
	if (Rxdata.startFlag)
	{
		Rxdata.index++; // 更新索引
		uint32_t tempData;
		memcpy(&tempData, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(tempData));
		Rxdata.sumData = ntohl(tempData);															  // 接收校验和
		Rxdata.index += sizeof(Rxdata.sumData);														  // 更新索引
		memcpy(&Rxdata.data.CMD, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.CMD)); // 接收命令位
		Rxdata.index += sizeof(Rxdata.data.CMD);														// 更新索引
		Rxdata.sumCheck += Rxdata.data.CMD;																// 更新检验和
		memcpy(&Rxdata.data.DataLen, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.DataLen)); // 接收数据长度
		Rxdata.index += sizeof(Rxdata.data.DataLen);															// 更新索引
		Rxdata.sumCheck += Rxdata.data.DataLen;																	// 更新检验和
		memcpy(&Rxdata.data.PropertyData_1, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_1));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_1); // 更新索引
		Rxdata.sumCheck += Rxdata.data.PropertyData_1;		// 更新检验和
		memcpy(&Rxdata.data.PropertyData_2, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_2));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_2);
		Rxdata.sumCheck += Rxdata.data.PropertyData_2;
		memcpy(&Rxdata.data.PropertyData_3, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_3));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_3);
		Rxdata.sumCheck += Rxdata.data.PropertyData_3;
		memcpy(&Rxdata.data.PropertyData_4, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_4));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_4);
		Rxdata.sumCheck += Rxdata.data.PropertyData_4;
		memcpy(&Rxdata.data.PropertyData_5, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_5));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_5);
		Rxdata.sumCheck += Rxdata.data.PropertyData_5;
		memcpy(&Rxdata.data.PropertyData_6, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_6));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_6);
		Rxdata.sumCheck += Rxdata.data.PropertyData_6;
		memcpy(&Rxdata.data.PropertyData_7, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_7));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_7);
		Rxdata.sumCheck += Rxdata.data.PropertyData_7;
		memcpy(&Rxdata.data.PropertyData_8, &Rxbuf[Rxdata.startPosition + Rxdata.index], sizeof(Rxdata.data.PropertyData_8));
		Rxdata.index += sizeof(Rxdata.data.PropertyData_8);
		Rxdata.sumCheck += Rxdata.data.PropertyData_8;
		if (Rxbuf[Rxdata.index] == ENDDATA)
		{
			if (Rxdata.sumData == Rxdata.sumCheck) // 检验和对比
			{
				// 处理接收到的数据（打开注释即可看到每次收到的数据）
				// ESP_LOGI(LOG_TAG, "命令位：%02x\t數據長度位：%02x\t數據位1：%02x\t數據位2：%02x\t數據位3：%02x\t數據位4：%02x\t數據位5： %02x\t數據位6： %02x\t數據位7： %02x\t數據位8： %02x",
				// 		 Rxdata.data.CMD, Rxdata.data.DataLen, Rxdata.data.PropertyData_1, Rxdata.data.PropertyData_2, Rxdata.data.PropertyData_3,
				// 		 Rxdata.data.PropertyData_4, Rxdata.data.PropertyData_5, Rxdata.data.PropertyData_6, Rxdata.data.PropertyData_7, Rxdata.data.PropertyData_8);
				*cmd = Rxdata.data.CMD;
				for (int i = 0; i < Rxdata.data.DataLen; i++)
				{
					*(data+i) = *(&Rxdata.data.PropertyData_1 + i);
				}
				for (int i = 8; i >= Rxdata.data.DataLen; i--)
				{
					*(data+i) = 0xFF;
				}
				
				// 解析完的原始数据
				// ESP_LOGI("蓝牙测试", "CMD: %02x\tdata1: %02x\tdata2: %02x\tdata3: %02x\tdata4: %02x\tdata5: %02x\tdata6: %02x\tdata7: %02x\tdata8: %02x\t",
				// 	bleCtrler->data.CMD, bleCtrler->data.PropertyData_1, bleCtrler->data.PropertyData_2, bleCtrler->data.PropertyData_3, bleCtrler->data.PropertyData_4,
				// 	bleCtrler->data.PropertyData_5, bleCtrler->data.PropertyData_6, bleCtrler->data.PropertyData_7, bleCtrler->data.PropertyData_8);
			}
			else
			{
				ESP_LOGE(LOG_TAG, "校驗和驗證錯誤，計算得校驗和：%02x\t接受到的校驗和 ：%02x", Rxdata.sumCheck, Rxdata.sumData);
			}
		}
	}
}


/**
 * @brief	 蓝牙发送数据    
 * @param    (uint8_t) cmd : 待格式化命令 
 * @param    (uint8_t) dataLen : 待格式化数据长度
 * @param    (uint8_t) data[] : 待格式化数据
 * @return   (void)
 */
static void bleSend(uint8_t cmd, uint8_t dataLen, uint8_t data[])
{
	uint32_t sum = cmd + dataLen;
	if (data != NULL)
	{
		for (uint8_t i = 0; i < dataLen; i++)
		{
			sum += data[i];
		}
	}

	uint8_t Txdata[16] = {STARTDATA, (sum >> 24), (sum >> 16) & 0xff, (sum >> 8) & 0xff, (sum & 0xff), cmd, dataLen};
	if (data != NULL)
	{
		memcpy(Txdata + 7, data, dataLen);
		memset(Txdata + 7 + dataLen, 0x00, 8 - dataLen);
	}
	else
	{
		memset(Txdata + 7, 0x00, 8);
	}
	Txdata[15] = ENDDATA;
	esp_ble_gatts_send_indicate(heart_rate_profile_tab[0].gatts_if, heart_rate_profile_tab[0].conn_id, heart_rate_handle_table[IDX_CHAR_VAL_C], sizeof(Txdata), Txdata, false);
}
