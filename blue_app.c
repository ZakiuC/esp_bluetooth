#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "blue_app.h"
//for ota 
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "errno.h"

/* deviceId : 84:F7:03:50:91:76 */
/**************************UUID*************************/
/* 服务 UUID: 000000df-0000-00fd-0000-400882088200 */
static uint8_t service_uuid[16] = {
    /* 低位 <--------------------------------------------------------------------------------> 高位 */
    0x00, 0x82, 0x08, 0x82, 0x08, 0x40, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00, 0xdf, 0x00, 0x00, 0x00,
};


/*特征：命令 UUID: 000000df-0000-00fe-0000-400882088201*/
static uint8_t char_cmd_uuid[16] = {
    /* 低位 <--------------------------------------------------------------------------------> 高位 */
    0x01, 0x82, 0x08, 0x82, 0x08, 0x40, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xdf, 0x00, 0x00, 0x00,
};
/*特征：ota接收数据 UUID: 010203df-0000-00fe-0000-400882088202*/
static uint8_t char_ota_data_uuid[16] = {
    /* 低位 <--------------------------------------------------------------------------------> 高位 */
    0x02, 0x82, 0x08, 0x82, 0x08, 0x40, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0xdf, 0x03, 0x02, 0x01,
};
/************************** 私有宏 *************************/
#define LOG_TAG                     "BLE_LOG"     
#define OTA_ERROR_TAG               "OTA_ERROR"
#define OTA_TAG                     "OTA_LOG"          
#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x66
#define SVC_INST_ID                 0
#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500                             // 特征值的最大长度。当GATT客户端执行写或准备写操作时。数据长度必须小于GATTS_DEMO_CHAR_VAL_LEN_MAX。
#define TASK_STACK_SIZE 1024*4                                     //  测试任务栈大小


/************************** 变量声明 *************************/
static uint8_t adv_config_done       = 0;
static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_write          = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t test_value[]                  = "I'm test_value.";
static const uint8_t heart_measurement_ccc[2]      = {0x00, 0x00};

uint16_t heart_rate_handle_table[HRS_IDX_NB];


Cmd_t BleCmd;
Overhaul_t AutoReset;
uint8_t *cmd_p = &BleCmd.OHCmd.Cmd1;//获取首地址
char *device_name = "default_name";
/***** ota相关 *****/
esp_err_t err;
/* 更新句柄：由esp_ota_begin()设置，必须通过esp_ota_end()释放。 */
esp_ota_handle_t update_handle = 0 ;
const esp_partition_t *update_partition = NULL;
FileSize_t FileSize;

esp_gatt_if_t Ggatts_if;
uint16_t Gconn_id;



/*************************** ble提示内容 ********************************/
char rxCmdSuccess[] = "Accepting data successfully.";
char rxCmdFail[] = "Error when receiving data, please check what was sent and resend.";
char rxCmdtest[] = "It's test value.";
char rxCmdOtaStart[] = "Command received, start ota, please send the ota file.";
char rxCmdOtaError[] = "Data verification failed, data is corrupted.";
char rxCmdOtaFail[] = "OTA end failure.";
char rxCmdOtaSetBootFail[] = "OTA failed to set boot partition.";
char rxCmdOtaReSystem[] = "OTA completed ready to reboot the system.";
char *rxCmdCmd[] = {"Cmd1 is triggered.", "Cmd2 is triggered.", "Cmd3 is triggered.", "Cmd4 is triggeredn.",
                    "Cmd5 is triggered.", "Cmd6 is triggered.", "Cmd7 is triggered."};
char Missionend[] = "end.";


/************************** 函数声明 *************************/
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void write2device(esp_gatt_if_t gatts_if, uint16_t conn_id, uint16_t Len, char *Txdata);
static void DefaultConfig(Cmd_t *BleCmd, Overhaul_t *AutoReset, char *name);
static bool CheckStart(uint8_t RxData);
static bool CheckEnd(uint8_t RxData);
static void RxData_Process(esp_ble_gatts_cb_param_t *param);



/************************** gatt配置 *************************/
struct gatts_profile_inst {
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
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, //从属连接最小间隔，时间=最小间隔*1.25毫秒
    .max_interval        = 0x0010, //从属连接最大间隔，时间=最大间隔*1.25毫秒
    .appearance          = 0x00,
    .manufacturer_len    = 0,    //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //test_manufacturer,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

//广播参数
static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,  //   min_int = 0x20*1.25ms = 40ms
    .adv_int_max         = 0x40,  //   max_int = 0x40*1.25ms = 80ms
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};



/************************** 特征表 *************************/
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
{
    /* 服务声明 */
    [IDX_SVC]         =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(service_uuid), sizeof(service_uuid), (uint8_t *)&service_uuid}},


    /* 特征声明 */
    [IDX_CHAR_A]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

    /* 特征值 */
    [IDX_CHAR_VAL_A]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_cmd_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(test_value), (uint8_t *)test_value}},
    
    /* 客户端特征配置描述值 */
    [IDX_CHAR_CFG_A]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},
    

    /* 特征声明 */
    [IDX_CHAR_B]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    /* 特征值 */
    [IDX_CHAR_VAL_B]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_ota_data_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(test_value), (uint8_t *)test_value}},
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
void ble_Init(char *name)
{
    DefaultConfig(&BleCmd, &AutoReset, name);
    esp_err_t ret = nvs_flash_init();
    /* 初始化 NVS. */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();}
    ESP_ERROR_CHECK( ret );
    /* 释放 ESP_BT_MODE_CLASSIC_BT，就是释放经典蓝牙资源，保证设备不工作在经典蓝牙下面： */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    /* 初始化蓝牙控制器，此函数只能被调用一次，且必须在其他蓝牙功能被调用之前调用 */
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;}
    /* 使能蓝牙控制器，工作在 BLE mode:
    如果想要动态改变蓝牙模式不能直接调用该函数，先disable关闭蓝牙再使用该API来改变蓝牙模式 */
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;}
    /* 初始化蓝牙主机，使能蓝牙主机：
    蓝牙栈 `bluedroid stack` 包括了BT和 BLE 使用的基本的define和API */
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s c failed: %s", __func__, esp_err_to_name(ret));
        return;}
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;}
    
    /* 注册 GATT 回调函数 */
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(LOG_TAG, "gatts register error, error code = %x", ret);
        return;}

    /* 注册 GAP 回调函数
        这个回调函数中定义了在广播期间蓝牙设备的一些操作： */
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(LOG_TAG, "gap register error, error code = %x", ret);
        return;}

     /* 注册 service 
        当调用esp_ble_gatts_app_register()注册一个应用程序Profile(Application Profile)，
        将触发ESP_GATTS_REG_EVT事件，除了可以完成对应profile的gatts_if的注册,F
        还可以调用esp_bel_create_attr_tab()来创建profile Attributes 表
        或创建一个服务esp_ble_gatts_create_service() */
    ret = esp_ble_gatts_app_register(ESP_APP_ID);
    if (ret){
        ESP_LOGE(LOG_TAG, "gatts app register error, error code = %x", ret);
        return;}
    
    /* 设置 mtu ，mtu 相关说明如下：
        MTU: MAXIMUM TRANSMISSION UNIT
        最大传输单元，指在一个PDU 能够传输的最大数据量(多少字节可以一次性传输到对方)。
        PDU：Protocol Data Unit 
        协议数据单元,在一个传输单元中的有效传输数据。 */
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(ESP_GATT_MAX_MTU_SIZE);   /*mtusize大于255时，将gatts_profile_event_handler ESP_GATTS_WRITE_EVT中
    */
    if (local_mtu_ret){
        ESP_LOGE(LOG_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);}

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
    if (event == ESP_GATTS_REG_EVT) {
        /* 确定底层GATT运行成功触发ESP_GATTS_REG_EVT时，完成对每个profile 的gatts_if 的注册*/
        if (param->reg.status == ESP_GATT_OK) {
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(LOG_TAG, "注册app, app_id: %04x, 状态: %d", param->reg.app_id, param->reg.status);
            return;
            }
        }
    /* 如果gatts_if == 某个Profile的gatts_if时，调用对应profile的回调函数处理事情。 */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE，不指定某个gatt_if，需要调用每个配置文件的cb函数 */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if) {
                if (heart_rate_profile_tab[idx].gatts_cb) {
                    heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);}
            }
        }
    } while (0);
}


/****************************** gatts事件处理 *******************************/
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        /* 一个Service的创建，包括GATT注册事件，添加 service的基本信息，设置BLE名称 */
        case ESP_GATTS_REG_EVT:{//gatt注册事件
            ESP_LOGI(LOG_TAG, "gatt 注册");
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(device_name);
            if (set_dev_name_ret){  
                ESP_LOGE(LOG_TAG, "设置设备名失败, 错误代码 = %x", set_dev_name_ret);
            }esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(LOG_TAG, "配置adv数据失败, 错误代码 = %x", ret);
            }adv_config_done |= ADV_CONFIG_FLAG;
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(LOG_TAG, "配置advu数据失败, 错误代码 = %x", ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(LOG_TAG, "创建特征表失败, 错误代码 = %x", create_attr_ret);
            }
        }
       	    break;
        case ESP_GATTS_READ_EVT: //GATT读取事件，手机读取开发板的数据
            ESP_LOGI(LOG_TAG, "gatt 被读");         
       	    break;
        case ESP_GATTS_WRITE_EVT://GATT写事件，手机给开发板的发送数据，不需要回复
            if (!param->write.is_prep){
                // ESP_LOGI(LOG_TAG, "gatt 被写, 句柄 = %d, 接收值长度 = %d, 值(字符串格式) : %s", param->write.handle, param->write.len, param->write.value);
                if (heart_rate_handle_table[IDX_CHAR_VAL_A] == param->write.handle){
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
                    RxData_Process(param);
                    /** 更新gatts_if和conn_id **/
                    Ggatts_if = gatts_if;
                    Gconn_id = param->write.conn_id;         
                }
                if (heart_rate_handle_table[IDX_CHAR_VAL_B] == param->write.handle){
                    uint16_t length = param->write.len;//当mtu大于255时，将uint8_t修改为uint16_t
                    FileSize.TotalLen += length;
                    FileSize.ReceivingProgress = (int)(((float)FileSize.TotalLen/FileSize.TotalFile)*100);
                        /*** 彩虹🌈进度条 ***/
                        // if(FileSize.LastProgress != FileSize.ReceivingProgress){
                        //     printf(NONE "\r");                                                                                                                                                                                
                        //     printf(BROWN "[");
                        //     for(int j= 0;j < FileSize.ReceivingProgress;j++){
                        //         switch(j%10){
                        //             case 0:printf(RED ">");break;
                        //             case 1:printf(L_RED ">");break;                         
                        //             case 2:printf(GREEN ">");break;                                   
                        //             case 3:printf(L_GREEN ">");break;
                        //             case 4:printf(BROWN ">");break;
                        //             case 5:printf(YELLOW ">");break;
                        //             case 6:printf(BLUE ">");break;
                        //             case 7:printf(L_BLUE ">");break;
                        //             case 8:printf(PURPLE ">");break;
                        //             case 9:printf(L_PURPLE ">");break;
                        //         }
                        //         fflush(stdout);
                        //     }
                        //     if(FileSize.ReceivingProgress!=100){
                        //         switch(FileSize.ReceivingProgress%4){
                        //             case 0:printf(BROWN "|");break;
                        //             case 1:printf(BROWN "/");break;                         
                        //             case 2:printf(BROWN "-");break;                                   
                        //             case 3:printf(BROWN "\\");break;
                        //         }
                        //     }else{
                        //         printf(RED ">");
                        //     }
                        //     for(int k= 0;k < 100-FileSize.ReceivingProgress;k++){
                        //         printf(NONE " ");
                        //     }
                        //     printf(BROWN "]");
                        //     printf(L_PURPLE "%d%%", FileSize.ReceivingProgress);
                        //     fflush(stdout);
                        //   ESP_LOGI(LOG_TAG, "OTA数据已接收 %d%%数据", FileSize.ReceivingProgress);
                        // }
                        if(FileSize.LastProgress != FileSize.ReceivingProgress)
                        {
                            ESP_LOGI(OTA_TAG, "OTA数据已接收 %d%%数据", FileSize.ReceivingProgress);
                        }
                        if(FileSize.ReceivingProgress == 100){
                            // printf(NONE "\n");
                            write2device(Ggatts_if, Gconn_id, sizeof(rxCmdOtaReSystem), rxCmdOtaReSystem);
                        }	
					err = esp_ota_write( update_handle, (const void *)param->write.value, length);
		            if (err != ESP_OK) {
		                esp_ota_abort(update_handle);
						ESP_LOGI(OTA_TAG, "OTA数据写入错误!");
		            }
                    FileSize.LastProgress = FileSize.ReceivingProgress;
                }
             }       
            break;
        case ESP_GATTS_EXEC_WRITE_EVT://GATT写事件，手机给开发板的发送数据，需要回复
            ESP_LOGI(LOG_TAG, "gatt 被写");
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(LOG_TAG, "gatt MTU交换, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(LOG_TAG, "gatt 修改, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(LOG_TAG, "gatt 服务开始, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT://GATT连接事件
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
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 1000;    // timeout = 400*10ms = 4000ms
            //开始向对等设备发送更新连接参数。
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(LOG_TAG, "gatt 断开, 原因 = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{ //GATT创建事件,包含基本参数的设置，将Characteristic加到service中，完成触发下面事件
                if (param->add_attr_tab.status != ESP_GATT_OK){
                    ESP_LOGE(LOG_TAG, "创建特征表失败, 错误代码=0x%x", param->add_attr_tab.status);
                }else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
                    ESP_LOGE(LOG_TAG, "创建特征表异常, num_handle (%d) \
                            不等于 HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
                }else{
                    ESP_LOGI(LOG_TAG, "创建特征表成功, number handle = %d\n",param->add_attr_tab.num_handle);
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
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT://广播数据设置完成事件标志
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);//开始广播
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT://广播扫描相应设置完成标志
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT://开始广播事件标志
            /* 广播启动完成事件，表示广播启动成功或失败 */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(LOG_TAG, "广播启动失败");
            }else{
                ESP_LOGI(LOG_TAG, "广播启动成功");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:// 停止广播事件标志
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(LOG_TAG, "停止广播失败");
            }else{
                ESP_LOGI(LOG_TAG, "停止广播成功\n");
            }
            break;  
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:// 设备连接事件,可获取当前连接的设备信息
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
    write2device(Ggatts_if, Gconn_id, sizeof(rxCmdOtaStart), rxCmdOtaStart); 
    ESP_LOGI(OTA_TAG, "写入偏移量为 0x%x的分区子类型%d, 等待发送文件 文件大小为 %.f Byte",
            update_partition->subtype, update_partition->address, FileSize.TotalFile);
    assert(update_partition != NULL);
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_ERROR_TAG, "OTA开启失败 (%s)", esp_err_to_name(err));
        esp_ota_abort(update_handle);
    }
}
/*
 * 接收文件结束后执行 
 * 命令: 0x04 */
void ota_start(void)
{
    FileSize.TotalLen = 0;//接收数据长度计数重置
    ESP_LOGI(OTA_TAG, "======OTA数据接收结束======");
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            write2device(Ggatts_if, Gconn_id, sizeof(rxCmdOtaError), rxCmdOtaError);
            ESP_LOGE(OTA_ERROR_TAG, "数据验证失败，数据已损坏");
        }
        write2device(Ggatts_if, Gconn_id, sizeof(rxCmdOtaFail), rxCmdOtaFail);
        ESP_LOGE(OTA_ERROR_TAG, "OTA结束失败 (%s)!", esp_err_to_name(err));
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        write2device(Ggatts_if, Gconn_id, sizeof(rxCmdOtaSetBootFail), rxCmdOtaSetBootFail);
        ESP_LOGE(OTA_ERROR_TAG, "OTA设置boot分区失败 (%s)!", esp_err_to_name(err));
    }
    ESP_LOGI(OTA_TAG, "准备重启系统!");
    esp_restart();
    return ;
}

/**
 * 等待OTA线程 */
static void waitOTA_task(void *arg)
{
    ESP_LOGI(OTA_TAG, "OTA线程已开启");
    while(1)
    {
            if(BleCmd.OtaCmd == READYOTA)
            {
                BleCmd.OtaCmd = NOCMD;
                ota_ready();
                ESP_LOGI(OTA_TAG, "请等待文件发送完毕");
                while(FileSize.ReceivingProgress != 100){vTaskDelay(100/portTICK_RATE_MS);}
                ota_start();  
            }
            vTaskDelay(10/portTICK_RATE_MS);
    }
}

/*
 * bleota 调用此函数启动bleaOTA功能  
 * 命令: 0x03 */
void ble_ota(void)
{
    xTaskCreate(waitOTA_task, "waitOTA_task", TASK_STACK_SIZE, NULL, 3, NULL);
}


/**
 * 设置接收文件大小
 * size: 接收文件大小（单位:Byte）*/
static void set_FileSize(float size)
{
    FileSize.TotalFile = size;
}

/**
 * 向特征值A(命令处理特征)写入数据，会被监听设备监听到 */
static void write2device(esp_gatt_if_t gatts_if, uint16_t conn_id, uint16_t Len, char *Txdata)
{
    esp_ble_gatts_send_indicate(gatts_if, conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], Len, (uint8_t *)Txdata, false);
}

/**
 * 初始化接收命令相关参数 */
static void DefaultConfig(Cmd_t *BleCmd, Overhaul_t *AutoReset, char *name)
{
    device_name = name;
    uint16_t len = sizeof(*AutoReset);

    for(int i=0;i<len;i++){
        *(&(BleCmd->OHCmd.Cmd1)+i) = SWITCHOFF;
        SwitchCmdMode(&(BleCmd->OHCmd.Cmd1)+i, 1);
    }
}

/**
 * 检查开始位标志 */
static bool CheckStart(uint8_t RxData)
{
    if(RxData != STARTDATA){
        ESP_LOGE(OTA_ERROR_TAG, "StartData is error.Please check ble data sent.");
        return false;
    }
    return true;
}

/**
 * 检查结束位标志 
 */
static bool CheckEnd(uint8_t RxData)
{
    if(RxData != ENDDATA){
        ESP_LOGE(OTA_ERROR_TAG, "EndData is error.Please check ble data sent.RxData ：%x", RxData);
        return false;
    }
    return true;
}

/**
 * Receiving data processing functions
 * 接收数据处理函数
 * Description: A function that processes ble features received data by control command 
 * 描述：按控制命令处理ble特征接收数据的函数 
 */
static void RxData_Process(esp_ble_gatts_cb_param_t *param)
{
    RxData_t Rxdata;      //定义接收数据结构体
    /* 初始化结构体数据 */             
    Rxdata.len = 0;       
    Rxdata.startFlag = false;
    Rxdata.index = 0;
    Rxdata.startPosition = 0;
    Rxdata.sumData = 0;
    Rxdata.ota_cmd = 0;
    Rxdata.ota_fileSize = 0;
    Rxdata.sumCheck = 0;
    Rxdata.len = param->write.len;
    /* 初始化变量 */
    uint8_t Rxbuf[Rxdata.len];
    uint8_t i=0;
    /* 将接收数据存入待处理缓存区 */
    for(i=0;i<Rxdata.len;i++){
        Rxbuf[i]=param->write.value[i];
    }
    /* 检查起始符以及记录起始符在接收数据中的位置 */
    for(i=0;i<Rxdata.len;i++){
       if(CheckStart(Rxbuf[i])){
            Rxdata.startFlag=true;
            Rxdata.startPosition = i;
            break;
       }
    }
    /* 处理接收数据 */
    if(Rxdata.startFlag){
        Rxdata.index++;     //更新索引
        /* 接收的校验和 */
        Rxdata.sumData = Rxbuf[Rxdata.startPosition+ Rxdata.index]<<24 | Rxbuf[Rxdata.startPosition+ Rxdata.index+1]<<16
            | Rxbuf[Rxdata.startPosition+ Rxdata.index+2]<<8 | Rxbuf[Rxdata.startPosition+ Rxdata.index+3];
        Rxdata.index+=sizeof(Rxdata.sumData);//更新索引
        /* 记录接收到的检修命令 */
        for(i=0;i<sizeof(Rxdata.cmdFix);i++){
            *(&Rxdata.cmdFix.Cmd1+i) = Rxbuf[Rxdata.startPosition+Rxdata.index+i];
            Rxdata.sumCheck+=*(&Rxdata.cmdFix.Cmd1+i);      //统计校验和
        }
        Rxdata.index+=sizeof(Rxdata.cmdFix);//更新索引
        /* 记录ota命令 */
        Rxdata.ota_cmd = Rxbuf[Rxdata.startPosition+Rxdata.index];
        Rxdata.sumCheck += Rxdata.ota_cmd;//统计校验和
        Rxdata.index+=sizeof(Rxdata.ota_cmd);//更新索引
        /* 记录接收的ota文件大小 */
        Rxdata.ota_fileSize = Rxbuf[Rxdata.startPosition+ Rxdata.index]<<24 | Rxbuf[Rxdata.startPosition+ Rxdata.index+1]<<16
            | Rxbuf[Rxdata.startPosition+ Rxdata.index+2]<<8 | Rxbuf[Rxdata.startPosition+ Rxdata.index+3];
        Rxdata.sumCheck += Rxdata.ota_fileSize;//统计校验和
        Rxdata.index+=sizeof(Rxdata.ota_fileSize);//更新索引
        /* 检查是否有结束符 */
        if(CheckEnd(Rxbuf[Rxdata.index])){
            /* 检验和对比 */
            if(Rxdata.sumCheck == Rxdata.sumData)
            {
                /* 保留接收的各类命令以及文件大小 */
                for(int i=0;i<sizeof(Rxdata.cmdFix);i++){
                *(&(BleCmd.OHCmd.Cmd1)+i) = *(&Rxdata.cmdFix.Cmd1+i);
                }
                BleCmd.OtaCmd = Rxdata.ota_cmd;
                set_FileSize(Rxdata.ota_fileSize);
            }
            else
            {
                ESP_LOGE(OTA_ERROR_TAG, "Data checksum error, please check the sent data.SumData ：%02x\tRxSumData ：%02x", Rxdata.sumCheck, Rxdata.sumData);
            }  
        }
    }
}


/**
 * Check all service commands
 * 检查所有检修命令
 * Return: 16-bit value, each bit represents a service command
 * 返回：16位的值，每一位代表一个检修命令
 */
uint16_t CheckCmdAll(void)
{
    uint16_t result = 0;
    uint8_t i = 0;

    for(i=0;i<sizeof(BleCmd.OHCmd);i++)
    {
        if(*(&(BleCmd.OHCmd.Cmd1)+i) == SWITCHON){
            // printf("已接受到命令%d\r\n", i+1);
            write2device(Ggatts_if, Gconn_id, strlen(rxCmdCmd[i]), rxCmdCmd[i]);
            result |= (1<<i);
            if(*(&(AutoReset.Cmd1)+i))
            {
                *(&(BleCmd.OHCmd.Cmd1)+i)=SWITCHOFF;
            }
        }
    }
    return result;
}

/**
 * Modify the command reset mode of the service mode
 * 修改检修模式的命令复位模式
 * Cmd: the address of the command to be modified
 * Cmd：待修改命令地址
 * Switch: 1 (default, i.e. rebound on trigger)
 *         0 (turn off automatic rebound)
 * Switch：1（默认，即触发就回弹）
 *         0（关闭自动回弹）
 * Example: SwitchCmdMode(&(BleCmd.OHCmd.Cmd3), 0); Turn off the automatic rebound of Cmd3
 * 例子：SwitchCmdMode(&(BleCmd.OHCmd.Cmd3), 0);  关闭Cmd3的自动回弹
*/
void SwitchCmdMode(uint8_t *Cmd,uint8_t Switch)
{
    *(&AutoReset.Cmd1 + (Cmd - &BleCmd.OHCmd.Cmd1)) = Switch;
}


/**
 * 复位某个检修命令
 * Cmd: the address of the command to be modified
 * Cmd：命令地址
*/
void ClrCmdFlag(uint8_t *Cmd)
{
    *Cmd = SWITCHOFF;
}


/**
 * 复位所有检修命令（包括了OTA命令）
*/
void ClrAllCmdFlag(void)
{
    uint8_t i = 0;
    for(i=0;i<8;i++)
    {
        *(&(BleCmd.OHCmd.Cmd1)+i) = SWITCHOFF;
    }
    BleCmd.OtaCmd = SWITCHOFF;
}

/**
 * 检测开始时调用(需要自己结束调用的函数)
*/
void StartMission(uint8_t po)
{
    write2device(Ggatts_if, Gconn_id, strlen(rxCmdCmd[po]), rxCmdCmd[po]);
}


/**
 * Individual inspection and maintenance commands
 * 单独检查检修命令
*/
bool CheckCmd(uint8_t *Cmd)
{
    uint8_t po = Cmd - &BleCmd.OHCmd.Cmd1;


    if(*Cmd == SWITCHON)
    {
        StartMission(po);
        return true;
    }
    if(*(&AutoReset.Cmd1 + po))
    {
        *Cmd = SWITCHOFF;
    }
    return false;
}


/**
 * 单独检查检修命令,若有命令则执行 func
 * 只可传入无参无返回值函数
*/
void CheckCmdGoOn(uint8_t *Cmd, void (*func)(void))
{
    uint8_t po = Cmd - &BleCmd.OHCmd.Cmd1;

    if(*Cmd == SWITCHON){
        StartMission(po);
        func();
    }
    if(*(&AutoReset.Cmd1 + po)){
        *Cmd = SWITCHOFF;
    }
}


/**
 * 检测结束时调用(需要自己结束调用的函数)
*/
void EndMission(void)
{
    write2device(Ggatts_if, Gconn_id, strlen(Missionend), Missionend);
}


/**
 * App Toast
 * App端提示
 * text：提示消息内容
*/
void appToast(char *text)
{
    char ToastFlag[] = "toast:";
    char TxText[strlen(text) + sizeof(ToastFlag)];

    sprintf(TxText, "%s%s", ToastFlag, text);
    write2device(Ggatts_if, Gconn_id, sizeof(TxText), TxText);
}


/*
*  关闭蓝牙
*  描述：关闭蓝牙和蓝牙控制器
*/
void ble_close(void)
{
    esp_err_t ret;
    ret = esp_bluedroid_disable();
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s disable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;}
    else{
        ESP_LOGI(LOG_TAG, "disable bluetooth sucessed");
    }
    ret = esp_bluedroid_deinit();
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s deinit bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;}
    else{
        ESP_LOGI(LOG_TAG, "deinit bluetooth sucessed");
    }
    ret = esp_bt_controller_disable();
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s disable bt_controller failed: %s", __func__, esp_err_to_name(ret));
        return;}
    else{
        ESP_LOGI(LOG_TAG, "disable bt_controller sucessed");
    }
    ret = esp_bt_controller_deinit();
    if (ret) {
        ESP_LOGE(LOG_TAG, "%s deinit bt_controller failed: %s", __func__, esp_err_to_name(ret));
        return;}
    else{
        ESP_LOGI(LOG_TAG, "deinit bt_controller sucessed");
    }
}