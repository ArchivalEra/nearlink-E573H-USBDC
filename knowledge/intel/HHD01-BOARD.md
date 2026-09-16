---
type: intel
title: "HHD-01 星闪开发板开箱即跑手册（Research Notes）"
language: zh
created: 2026-08-17
tags: [intel, research, notes]
sources:
  - "/mnt/hdd/nearlink-stuff/fbb_ws63"
  - "/mnt/hdd/nearlink-stuff/HopeRun-NearLink"
trust: B
stale_after: 2027-02-17
---

# HHD-01 星闪开发板开箱即跑手册（Research Notes）

**Date:** 2026-08-17
**Status:** Pre-hardware research — no build, no flashing, no hardware performed. All claims from local files (file:line cited). Items marked [VERIFY] must be confirmed against the physical board / official product manual PDFs (not parseable here).

---

## 1. Sources

Local only, read today:

- `/mnt/hdd/nearlink-stuff/HopeRun-NearLink/` (HopeRun 官方仓库镜像)
  - `firmware/README.md` — HH 板↔芯片映射
  - `HH-D01/board/README.md`, `HH-D01/board/IO复用关系.md`, 24 份 `WS63V100 *.pdf` + `ws63-Document-Cn.chm`
  - `HH-D01/*.PDF` — 6 个板级 PDF（三色灯/原理图/教程/产品手册/规格书/AT案例）
  - `demo/23_sle_uart/`, `demo/24_sle_humi/`, `demo/25_sle_led/`, `demo/26_sle_gas/`, `demo/27_sle_oled/`, `demo/sle_uart_demo/`
  - `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg`（1391436 B，预编译固件包）
- `/tmp/ws63_at.txt` — 从 `HH-D01/WS63V100 AT命令使用案例.pdf` 提取的文本（PDF 本体无法在此解析，使用已提取文本，源文件 927007 B）
- `/mnt/hdd/nearlink-stuff/fbb_ws63/` — HiSpark FBB 统一平台 WS63/WS63E SDK
  - `README.md`, `tools/README.md`, `src/application/samples/bt/sle/*`, `src/middleware/utils/at/*`, `src/include/middleware/services/bts/sle/*`, `src/build/config/target_config/ws63/config.py`
- `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/` — 我们自己的 WS73 PC 栈（互连目标）

PDF 处理说明：所有 `.pdf/.chm` 均未解析内容（无工具/只读约束）。`WS63V100 AT命令使用案例.pdf` 的内容经 /tmp/ws63_at.txt 的预提取文本纳入；其余 PDF 仅列出文件名并基于文件名与 README 描述推断覆盖范围，标注为推断。

---

## 2. HHD-01 板卡全貌

### 2.1 芯片与定位
- 官方映射（`HopeRun-NearLink/firmware/README.md:9-13`）：模块 HH-M01 → 开发板 HH-D01 → 芯片 **WS63**；HH-M02 → HH-D02 → **WS63E**；HH-M03 → HH-D03 → **BS21**（BS21 忽略）。
- 注意：demo 的 README 文本把套件描述为"基于海思 **WS63E** 解决方案"（`demo/23_sle_uart/README_CN.md:5`、`demo/24_sle_humi/README_CN.md:5` 等）。即 HH-D01 主控为 WS63V100 系列（WS63 与 WS63E 是同一芯片的产品变体），demo 面向整条 WS63 线，与 firmware 映射的 WS63 不矛盾。
- 套件名：`NearLink_DK_WS63`（`/tmp/ws63_at.txt:59-62`）；OpenHarmony 编译产品号 `nearlink_dk_3863`（`demo/23_sle_uart/README.md:94`，3863=WS63/H3863 系列）。
- `HH-D01/board/README.md:3-5` 指向 "Q353333N1100 系列用户指南"（百度网盘，不在本地）。

### 2.2 板载外设（从文件与 demo 代码推断）
| 外设 | 证据 |
| --- | --- |
| 板载三色灯（RGB LED） | `HH-D01/HH-D01 板载三色灯规格说明书.PDF`（文件名） |
| USER 按键（GPIO13） | `demo/25_sle_led/sle_uart_server.c:48` `#define IOT_GPIO_BUTTON 13`，上拉+下降沿中断（`sle_uart_server.c:476-482`） |
| 交通灯扩展板红 LED（GPIO7） | `demo/25_sle_led/sle_uart_client.c:53` `#define RED_GPIO 7`，`IOT_IO_FUNC_GPIO_7_GPIO`（`sle_uart_client.c:374`） |
| 温湿度 AHT20（I2C1，400 kHz） | `demo/24_sle_humi/sle_uart_server.c:43-44` `AHT20_I2C_IDX 1`, `AHT20_BAUDRATE 400000`；`aht20.c` |
| MQ-2 燃气传感器（ADC4=GPIO11） | `demo/26_sle_gas/README_CN.md:54`；`sle_uart_server.c` `adc_port_read(4,...)` |
| 无源蜂鸣器（GPIO9=PWM1） | `demo/26_sle_gas/README_CN.md:53`；`InitBeep()` 用 `IoSetFunc(9,1)`+`IoTPwmInit(1)`（`sle_uart_server.c:100-105`） |
| SSD1306 OLED（0.96" I2C） | `demo/27_sle_oled/README_CN.md:57-63`（依赖 `12_oled/src:oled_ssd1306`）；`sle_uart_client.c:336-340` `OledShowString` |
| UART（默认 115200,8N1，UART0） | 全部 demo：`uapi_uart_init(0,...)` baud 115200（`23/sle_uart_client.c:94-112`、`23/sle_uart_server.c:99-119`）；UART0 引脚 = GPIO17/18（见 2.3） |

### 2.3 IO 复用要点（`HH-D01/board/IO复用关系.md:3-23`）
19 个可用 IO（GPIO_00..GPIO_18）。关键复用（MODE 0=GPIO 默认，其余为复用功能）：

- GPIO_00..GPIO_08：PWM0..7、SPI1、JTAG、I2S 备用
- GPIO_09：PWM1 / RADAR_ANT0_SW / SPI0_OUT / I2S_DO —— **蜂鸣器用 PWM1**
- GPIO_11：PWM3 / RADAR_ANT1_SW / SPI0_IN / I2S_LRCLK —— 环境检测板 ADC 映射 gpio11(ADC4)
- GPIO_13/14：UART1_CTS/RTS、JTAG（25_sle_led 的按键用 GPIO13=GPIO 模式 MODE0）
- UART1_TXD/RXD=GPIO_15/16：MODE2 = I2C1_SDA/SCL —— **AHT20/OLED 走 I2C1**
- UART0_TXD/RXD=GPIO_17/18：MODE2 = I2C0_SDA/SCL
- 有 RADAR_ANT0_SW / ANT0_SW / REFCLK 等雷达/时间同步专用信号，印证芯片支持雷达特性（与 AT 雷达指令集配套）。

### 2.4 供电/烧录接口（推断，[VERIFY]）
- 板上应为 USB（板载串口转接，fbb_ws63 文档用 **CH340G** 驱动：`fbb_ws63/tools/README.md:95`），烧录/AT 串口共用，波特率 115200。
- 烧录方式推断：串口工具"程序加载"+"Connecting, please reset device..."时复位开发板（`fbb_ws63/tools/README.md:105-113`）。OpenHarmony 侧用 hb 编出镜像后烧录工具（HiBurn/DevEco）具体以 `HH-D01 星闪开发板开发教程-20240715.pdf` / `HH-D01 NearLink Development Board Product Manual` 为准 [VERIFY]。
- `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg` 是官方预编译固件包，可作 AT 基础固件烧录候选 [VERIFY 是否即"基础固件"]。

---

## 3. 芯片映射（HH 板 ↔ 芯片）

来源 `HopeRun-NearLink/firmware/README.md:9-13`（逐字）：

| NearLink 模块 | 开发板 | 芯片 |
| --- | --- | --- |
| HH-M01 | HH-D01 | **WS63** |
| HH-M02 | HH-D02 | **WS63E** |
| HH-M03 | HH-D03 | **BS21**（忽略） |

- HH-D01 / HH-D02 的 board 资料目录内容几乎一致（`HH-D02/board/` 有同套 `WS63V100 *.pdf`；`HH-D02/` 另有 `HH-D02 开发板使用手册-V1.2.pdf`），即 WS63 与 WS63E 共享同一 WS63V100 SDK/文档。
- HH-D03 资料为独立 `HH-D03 星闪开发板规格说明书` / `HH-D03 原理图` / `HH-D03 AT手册.pdf`（BS21 芯片，与 WS63 SDK 不同），忽略。

---

## 4. 23_sle_uart 深度拆解（互连首选对端）

实验：两块 WS63 开发板通过 SLE 星闪互转串口数据，默认 115200（`demo/23_sle_uart/README.md:98`）。文件：`sle_uart_server.c` / `sle_uart_server_adv.c` / `sle_uart_client.c`。

### 4.1 关键标识（与我们 PC 栈互连的协议锚点）
| 项 | 值 | 位置 |
| --- | --- | --- |
| Server 广播名 | `"sle_uart_server"` | `sle_uart_server_adv.c:55` |
| Server 本地地址 | `78:70:60:88:96:45`（announce param）/ `78:70:60:88:96:46`（adv init） | `sle_uart_server_adv.c:146,259` |
| Server app（server）UUID | `{0x12,0x34}`（16-bit 0x1234） | `sle_uart_server.c:50` |
| **服务 UUID** | **0x2222** | `sle_uart_server.h:30` `SLE_UUID_SERVER_SERVICE` |
| **特征 UUID** | **0x2323** | `sle_uart_server.h:33` `SLE_UUID_SERVER_NTF_REPORT` |
| 128-bit base UUID | `{37,BE,A8,80,FC,70,11,EA,B7,20,00,00,00,00,00,00}` | `sle_uart_server.c:77-78`（16-bit 0x2222/0x2323 映射到 base 低 2 字节，`Encode2byteLittle` 小端） |
| 特征权限 | SSAP_PERMISSION_READ \| WRITE；operate=读|写 | `sle_uart_server.h:36,39` |
| 描述符 | SSAP_DESCRIPTOR_CLIENT_CONFIGURATION，值 `{0x01,0x02}` | `sle_uart_server.c:277-282` |
| MTU | 520 | `sle_uart_server.c:46`、`sle_uart_client.c:38` |

### 4.2 Server 流程（`sle_uart_server.c` + `sle_uart_server_adv.c`）
1. 任务 `SleTask`：`usleep(1s)` → `UartInitConfig()`（UART0 115200,8N1，`UART_RX_CONDITION_FULL_OR_IDLE` 接收回调，注意注释：接收字节数不能是 16 的整数倍否则需多发 1 字节触发）→ `sle_uart_server_init()`（`sle_uart_server.c:463-470`）。
2. `sle_uart_server_init`：依次注册 announce 回调、连接回调、SSAPS 回调 → `EnableSle()`（`sle_uart_server.c:411-437`）。
3. `sle_enable_cbk`（adv 文件）：`osal_msleep(1000)` 后 `sle_enable_server_cbk()`（`sle_uart_server_adv.c:231-236`）。
4. `sle_enable_server_cbk`：`sle_uart_server_add()` → `sle_uart_server_adv_init()`（`sle_uart_server.c:439-453`）。
   - `sle_uart_server_add`：`SsapsRegisterServer(0x1234)` → `SsapsAddServiceSync(0x2222)` → `SsapsAddPropertySync(0x2323, 值长 8)` → `SsapsAddDescriptorSync(CCCD 0x0102)` → `SsapsStartService`（`sle_uart_server.c:301-328`）。
   - `sle_uart_server_adv_init`：`SleSetLocalAddr` → `SleSetAnnounceParam`（connectable+scanable，adv interval 25ms，conn interval 12.5ms，supervision timeout 5000ms）→ `SleSetAnnounceData`（adv data 含 DISCOVERY_LEVEL + ACCESS_MODE，scan-rsp 含 TX_POWER + 本地名）→ `SleStartAnnounce(handle 1)`（`sle_uart_server_adv.c:255-271`）。
5. 连接建立（`sle_connect_state_changed_cbk`）：CONNECTED → `ssaps_set_info(mtu=520)`（`sle_uart_server.c:354-359`）。
6. **数据上行（UART→SLE）**：UART0 RX 回调 → `UartSleSendData` → `sle_uart_server_send_report_by_handle` → `SsapsNotifyIndicate(propertyHandle)`（`sle_uart_server.c:88-97,331-343,455-461`）。
7. **数据下行（SLE→UART）**：`ssaps_write_request_callbacks` 打印 `client_send_data: %s`（`sle_uart_server.c:400-408`）。
8. 断开：`SleStartAnnounce` 重新广播（`sle_uart_server.c:360-364`）。

### 4.3 Client 流程（`sle_uart_client.c`）
1. `SleTask`：`UartInitConfig()` → `SleUartClientInit()`（`sle_uart_client.c:357-364`）。
2. `SleUartClientInit`：本地地址 `13:67:5c:07:00:51` → `SsapcRegisterClient`（app UUID 128-bit `{39,BE,A8,80,FC,70,11,EA,B7,20,...}`）→ 注册 seek / conn / ssapc 回调 → `EnableSle()` → `SleSetLocalAddr`（`sle_uart_client.c:343-355`）。
3. 扫描：`sle_enable_cbk` 后 `SleUartStartScan`（seek interval/window=100）（`sle_uart_client.c:114-136`）；`seek_result_cbk` 用 `strstr(adv_data, "sle_uart_server")` 匹配 → 保存地址 → `SleStopSeek`（`sle_uart_client.c:145-154`）→ `seek_disable_cbk` 里 `SleConnectRemoteDevice`（`sle_uart_client.c:156-163`）。
4. 连接：CONNECTED → `SsapcExchangeInfoReq(mtu=520)` + `SlePairRemoteDevice`（`sle_uart_client.c:182-188`）；`exchange_info_cbk` 里 `ssapc_find_structure`（`sle_uart_client.c:206-219`）→ `find_property_cbk` 保存 write handle（`sle_uart_client.c:245-247`）。
5. **数据上行（UART→SLE）**：UART0 RX 回调 → `uart_sle_client_send_data`（含 `osal_mdelay(100)` 限速）→ `sle_uart_client_send_report_by_handle` → `SsapWriteReq`（`sle_uart_client.c:83-92,296-319`）。注意：写请求 `param.handle` 用的是 `g_sle_uart_find_service_result.start_hdl`（服务起始 handle，`sle_uart_client.c:300`），而 find_property 得到的 handle 存进了 `g_sle_uart_send_param`（另一个变量）——**对端必须能接受对服务 handle 的写**（open question，见 §10）。
6. **数据下行（SLE→UART）**：`ssapc_notification_callbacks` 打印 `server_send_data: %s`（`sle_uart_client.c:321-330`）。
7. 断开：`SleRemovePairedRemoteDevice` + 重新扫描（`sle_uart_client.c:189-194`）。

### 4.4 编译方式（OpenHarmony 路径，`demo/23_sle_uart/README.md:53-94`）
1. 复制 `23_sle_uart` 到 OH 源码 `applications/sample/wifi-iot/app`。
2. 改 app 级 `BUILD.gn` features 加 `"23_sle_uart:sle_uart"`。
3. `device/soc/hisilicon/ws63v100/sdk/build/config/target_config/ws63/config.py` 的 `'ws63-liteos-app'` `ram_component` 加 `"sle_uart"`。
4. `.../sdk/libs_url/ws63/cmake/ohos.cmake` 的 `COMPONENT_LIST` 加 `"sle_uart"`。
5. 套件内 `BUILD.gn` 选 client 或 server 三件套（`sle_uart_client.c` 或 `sle_uart_server.c`+`sle_uart_server_adv.c`；`BUILD.gn:14-19` 默认 client）。
6. `rm -rf out && hb set -p nearlink_dk_3863 && hb build -f`，编译两次烧两块板。

### 4.5 对我们 PC 栈的意义
互连对端 = HHD-01 上跑 `23_sle_uart` **server**（广播名 `sle_uart_server`，服务 0x2222，特征 0x2323，base UUID `{37,BE,...}`，MTU 520）。我们 WS73 栈侧需做：scan/seek 匹配广播名 → connect → exchange MTU（520）→ find service（0x2222）→ enable CCCD → 写/通知收发数据。参考我们栈的 SSAP 定义：`stack/ssap/include/hwsle_transport.h`（SSAP 走 tcid 0x0A = TCID_SLE_SMTC，datatype 0xA3）与 `ssap_server.h`（handle 从 0x0001，值长上限 1024）。

---

## 5. 其他 SLE demo 简述

四个 demo 与 23_sle_uart 结构完全同源（同一套 `sle_uart_server.c/client.c/adv.c` 骨架、同一广播名、同一 0x2222/0x2323 UUID、同一 128-bit base），差异只在收发两端挂了不同外设：

| demo | 做什么 | 关键 API / 差异点 |
| --- | --- | --- |
| **24_sle_humi** | server 每 ~100ms（`osDelay(100)`）读 AHT20 温湿度，`sprintf "temp:%.2f,humi:%.2f"` 经 `SsapsNotifyIndicate` 发给 client | `aht20.c/h`；`InitTempHumiSensor`（I2C1,400k）；`iot_gpio.h/ex.h`；连接后才发（`connect_success_flag`）（`sle_uart_server.c:484-495`） |
| **25_sle_led** | 按 server 板 USER 键（GPIO13，下降沿中断）→ 发 1 字节 `led_flag`；client 收到 notification 翻转交通灯板红 LED（GPIO7） | `iot_gpio_ex.h` + `hal_iot_gpio_ex.c`；`IoTGpioRegisterIsrFunc`（`sle_uart_server.c:466-489`）；`IoTGpioSetOutputVal`（`sle_uart_client.c:329-340`） |
| **26_sle_gas** | server 每 ~100ms 读 MQ-2 ADC4，`adc_port_read(4)`；>1000 鸣蜂鸣器（GPIO9/PWM1），并把数值 `itoa` 后经 SLE 发 client | `adc.h`, `iot_pwm.h`；`InitBeep/InitMQ2`（`sle_uart_server.c:100-110`）；SDK<1.10.102 需按 `adc_driver/替换文件步骤.txt` 替换 ADC 驱动（`README_CN.md:100`） |
| **27_sle_oled** | 串口透传（同 23）并在 client 侧把收到的数据实时显示到 SSD1306 OLED（最多 64 英文字符） | `oled_ssd1306.h`；`OledShowString`（`sle_uart_client.c:336-340`）；需同时编译 `12_oled/src:oled_ssd1306`（`README_CN.md:57-63`） |

共同使用的 SLE API（OHOS 包装）：`EnableSle`/`SleSetLocalAddr`/`SleSetAnnounceData`/`SleSetAnnounceParam`/`SleStartAnnounce`/`SleSetSeekParam`/`SleStartSeek`/`SleStopSeek`/`SleAnnounceSeekRegisterCallbacks`/`SleConnectRemoteDevice`/`SleConnectionRegisterCallbacks`/`SlePairRemoteDevice`/`SsapsRegisterServer`/`SsapsAddDescriptorSync`/`SsapsAddPropertySync`/`SsapsAddServiceSync`/`SsapsStartService`/`SsapsNotifyIndicate`/`SsapsRegisterCallbacks`/`SsapcRegisterClient`/`SsapWriteReq`（各 demo README 的 OH API 表）。

---

## 6. AT 案例说明 + AT 桥接路径

### 6.1 PDF 覆盖范围（`HH-D01/WS63V100 AT命令使用案例.pdf`，927007 B）
文本已提取（`/tmp/ws63_at.txt`）。模型 `NearLink_DK_WS63`，三个案例章节：
1. **Wi-Fi 连热点**：`AT+STARTAP` / `AT+STARTSTA` / `AT+RECONN` / `AT+SCAN` / `AT+SCANRESULT` / `AT+CONN`（`/tmp/ws63_at.txt:72-93`）。
2. **SLE 通信**：完整服务端/客户端指令序列（见 6.2）。
3. **BLE 通信**：`AT+BLEENABLE` / `AT+BLESETNAME` / `AT+BLESETADDR` / `AT+GATTSREGCBK` / `AT+GATTSREGSRV` / `AT+GATTSSYNCADDSERV/ADDCHAR/ADDDESCR` / `AT+GATTSSTARTSERV` / `AT+BLESETADVPAR/ADVDATA` / `AT+BLESTARTADV` / `AT+GATTCREGCBK/CREG` / `AT+BLECONN` / `AT+BLEPAIR` / `AT+BLEGETPAIREDDEV` / `AT+GATTCFNDSERV` / `AT+GATTCWRITEREQ` / `AT+GATTSSNDNTFY` / `AT+GATTCREADBYHDL`（`/tmp/ws63_at.txt:170-232`）。
4. 本 PDF **不含雷达案例**；雷达指令集在配套 `HH-D01/board/WS63V100 AT命令 使用指南_03.pdf`（"主要包括Wi-Fi、BLE、SLE、雷达指令集"，`HH-D01/board/README.md:12`）。

### 6.2 SLE AT 完整指令序列（可直接在串口复现，`/tmp/ws63_at.txt:112-161`）
**服务端**：
```
AT+SLEENABLE
AT+SLESETADDR=0,0x112233445566
AT+SSAPSREGCBK
AT+SSAPSADDSRV=0x1234            ; server UUID
AT+SSAPSADDSERV=0x2222,1         ; 服务 0x2222 —— 与 23_sle_uart 一致
AT+SSAPSADDPROPERTY=1,0x2323,5,5,2,0x1234
AT+SSAPSADDDESCR=1,2,0x3333,5,5,2,2,0x0200
AT+SSAPSSTARTSERV=1
AT+SLESETADVPAR=1,3,200,200,0,0x112233445566,0,0x000000000000
AT+SLESETADVDATA=1,10,4,aabbccddeeff11223344,11224455
AT+SLESTARTADV=1
```
**客户端**：
```
AT+SLEENABLE
AT+SLESETADDR=0,0x112233445577
AT+SSAPCREGCBK
AT+SLESETSCANPAR=1,0x48,0x48
AT+SLESTARTSCAN
AT+SLESTOPSCAN
AT+SLECONN=0,112233445566
AT+SLEPAIR=0,112233445566
AT+SSAPCFNDSTRU=1,0,1
```
**收发**：client→server `AT+SSAPCWRITECMD=0,0,1,0,5,0x1122334455`；server→client `AT+SSAPSSNDNTFY=0,1,0,5,0x66778899AA`。

要点：**AT 案例的 UUID（服务 0x2222 / 特征 0x2323 / server 0x1234）与 23_sle_uart demo 完全一致** —— AT 固件与 demo 固件跑的是同一套 SSAP 服务，对互连测试是好消息。

### 6.3 与 fbb_ws63 的 AT 支持关系
- fbb_ws63 SDK 内置完整 AT 框架：`src/middleware/utils/at/` 下 `at/`（内核）、`at_wifi_cmd/`、`at_bt_cmd/`、`at_radar_cmd/`、`at_plt_cmd/`。
- **SLE/BLE AT 命令表**：`src/middleware/utils/at/at_bt_cmd/at/at_bt_cmd_table.h`（约 1317 行起）含 `SLEENABLE` / `SLESETADV` / `SLEENABLEADV` / `SLESETADVPAR` / `SLESETADVDATA` / `SLECONNADDR` / `SLEDISCONN` / `SLEPAIRREMOTE` / `SLEADDSERVER` / `SLEADDSERVICE` / `SLEADDPROPERTY` / `SLEADDDESCRIPTOR` / `SLESTARTSERVICE` / `SLESSAPSERREGISTER` / `SLESSAPCENREGISTER` / `SLEDISCOVERYSERVICES` / `SLESSAPCENWRITE` / `SLESSAPCENREAD` / `SLECLIENTINIT`，外加 BLE GATT 命令。
- **Wi-Fi AT 命令表**：`at_wifi_cmd/at/at_wifi_cmd_table.h`（`CONN`/`DISCONN`/`RECONN`/`SCAN`/`SCANRESULT`/`STARTAP`/`STARTSTA` 等，与 AT 案例 PDF 的 WiFi 用例对应）。
- **雷达 AT 命令表**：`at_radar_cmd/at/at_radar_cmd_table.h`（`RADARSETST`/`RADARSETPARA`/`RADARALGCTRL`/`RADARGETST` 等）。
- 预编译库 `at_bt_cmd/ws63-liteos-app/libbt_at.a` 内已有 `bt_at_enable_sle_cmd`、`bt_at_sle_register_callback_cmd`、`bt_at_sle_rf_*`（RF 测试）符号，并引用 `sle_at_cmd_factory_register_cbks`/`enable_sle`——说明 SLE AT 处理逻辑主体在 SDK/预编译库里，fbb_ws63 源码只暴露命令表与注册点。
- **命名差异**：AT 案例 PDF（OpenHarmony 时代）用 `AT+SSAPSADDSRV/AT+SLECONN/AT+SSAPCWRITECMD`；fbb_ws63（FBB 统一平台）命令表用 `SLESSAPSERREGISTER/SLECONNADDR/SLESSAPCENWRITE`。两套词汇功能同构但名字不同——**互连测试时须先确定 HHD-01 出厂 AT 固件是哪套词汇** [VERIFY]。

### 6.4 明天串口 AT 直接驱动 HHD-01 的路径（零编译）
1. 给 HHD-01 烧"基础固件"（AT 固件；`/tmp/ws63_at.txt:69,100,168` 准备项即"烧录基础固件"；候选镜像 `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg` [VERIFY 是否含 AT]）。
2. 串口助手 115200,8N1，无流控（`/tmp/ws63_at.txt:69-70`）。
3. 按 6.2 服务端序列配置 HHD-01，让它广播 `0x2222/0x2323` 服务。
4. 随后可用第二块 WS63 或直接对我们 WS73 栈验证：PC 栈 scan 到广播名 → connect → 按 0x2222/0x2323 交互。AT 路径可作为 PC 栈互连的"金标准"参照（无需自己编译 WS63 固件）。

---

## 7. 明天上手 checklist（HHD-01 烧 23_sle_uart server + PC 栈互连）

标注 [VERIFY] = 需实机验证/查产品手册。

### 阶段 A：环境准备
1. [VERIFY] 确认板载串口芯片（文档指向 CH340G，`fbb_ws63/tools/README.md:95`），装驱动。
2. 准备串口助手（115200,8N1,无校验,无流控，`/tmp/ws63_at.txt:69-70`）。
3. 通电后先发 `AT` 或 `AT+SYSINFO`（`demo/26_sle_gas/README_CN.md:100` 提到 `AT+SYSINFO`）确认固件是 AT 固件还是 demo 固件，并确认 AT 命令词汇版本 [VERIFY]。

### 阶段 B：编译 23_sle_uart server（二选一）
路径 B1（OpenHarmony/Oniro，官方 demo 路径）：
1. 复制 `demo/23_sle_uart/` 到 OH 源码 `applications/sample/wifi-iot/app`（`README.md:56`）。
2. 改 app 级 `BUILD.gn` features → `"23_sle_uart:sle_uart"`（`README.md:57-65`）。
3. `config.py` `'ws63-liteos-app'` 的 `ram_component` 加 `"sle_uart"`（`README.md:66-69`）。
4. `ohos.cmake` 的 `COMPONENT_LIST` 加 `"sle_uart"`（`README.md:71-74`）。
5. 套件 `BUILD.gn` sources 改 server 三件套：`sle_uart_server_adv.c` + `sle_uart_server.c`（注释 `sle_uart_client.c`）（`README.md:76-92`）。
6. `rm -rf out && hb set -p nearlink_dk_3863 && hb build -f`（`README.md:94`）。
路径 B2（fbb_ws63 统一平台，可选）：
1. 克隆/使用 `/mnt/hdd/nearlink-stuff/fbb_ws63/`（LiteOS 版，`README.md:5`；Windows 用 HiSpark Studio，`tools/README.md:7`）。
2. menuconfig 选 `SAMPLE_SUPPORT_SLE_UUID_SERVER_SAMPLE`（`src/application/samples/bt/sle/Kconfig:11-24`；该样例 UUID 为 0xABCD/0x1122，见 §8）。
3. 编译 + HiSpark Studio"程序加载"烧录（`tools/README.md:89-113`）。

### 阶段 C：烧录 HHD-01
1. [VERIFY] 用对应烧录工具（OH 侧 HiBurn/DevEco；fbb 侧 HiSpark Studio），选串口，加载镜像。
2. 出现 "Connecting, please reset device..." 时复位开发板（`tools/README.md:105-113`）。
3. 烧录后复位，串口应看到 `[sle uart server] init ok` / `create SleTask successfully !` 类打印（`sle_uart_server.c:435,486`）与 announce 数据打印（`sle_uart_server_adv.c:187-203`）。

### 阶段 D：PC 栈（WS73 dongle）互连 23_sle_uart server
1. 我们栈启用 scan/seek，匹配广播名 `sle_uart_server`（`sle_uart_server_adv.c:55`）。
2. connect 后 exchange MTU=520（`sle_uart_server.c:46,357`）。
3. find service 0x2222 / 特征 0x2323（`sle_uart_server.h:30,33`），enable CCCD（`sle_uart_server.c:277-282`）。
4. 数据面：PC→HHD-01 用 **write**（服务端 `ssaps_write_request_callbacks`，`sle_uart_server.c:400-408`）；HHD-01→PC 用 **notify**（`SsapsNotifyIndicate`，`sle_uart_server.c:331-343`）。注意 MTU 520 上限、负载末尾被加/设 `'\0'`（收发各 `len+1`，`sle_uart_server.c:338`、`sle_uart_client.c:303`）。
5. 我们传输层参考：`stack/ssap/include/hwsle_transport.h`（SSAP PDU 以 datatype 0xA3 / tcid 0x0A 走 `/dev/hwsle`）。
6. [VERIFY] 若 HHD-01 跑的是 **AT 固件**而非 23_sle_uart demo，则按 §6.2 的 AT 序列先把它配置成 server，再用 PC 栈以同样 UUID 互连。

### 阶段 E：验证
- 双板测试（有第二块 WS63 时）：串口互相收发，115200（`README.md:98`）。
- PC 栈对拍：HHD-01 串口输入字符 → PC 侧 notification 收到；PC 写 → HHD-01 串口打印 `client_send_data: ...`。
- [VERIFY] 若失败，先查 UART RX 16 字节整倍数触发缺陷（`sle_uart_client.c:109` 注释）与配对（`SlePairRemoteDevice`/AT `SLEPAIR`）要求。

---

## 8. 与 fbb_ws63 的关系（demo 是否同源）

**结论：同源但分叉。** 两者都基于同一海思 WS63V100 SDK 的 SLE/SSAP 栈，但 API 面与构建系统不同。

| 维度 | HopeRun demo（23_sle_uart 等） | fbb_ws63 samples |
| --- | --- | --- |
| 位置 | `HopeRun-NearLink/demo/*` | `fbb_ws63/src/application/samples/bt/sle/{sle_uuid,sle_speed}_{server,client}` |
| API 面 | OHOS 包装（`SleSetLocalAddr`/`SsapsAddServiceSync`/`SleConnectRemoteDevice`，含 `ohos_sle_*` 头） | 原生 API（`sle_set_local_addr`/`ssaps_add_service_sync`/`sle_connect_remote_device` 风格；`sle_uuid_server.c:89-151` 全小写） |
| 服务/特征 UUID | 0x2222 / 0x2323（`sle_uart_server.h:30,33`） | **0xABCD / 0x1122**（`sle_uuid_server.h:18-21`） |
| 128-bit base UUID | `{37,BE,A8,80,FC,70,11,EA,B7,20,...}`（`sle_uart_server.c:77-78`） | 完全相同 `{0x37,0xBE,...}`（`sle_uuid_server.c:46`） |
| 广播名 | `sle_uart_server` | `sle_speed_server` 也用 `"sle_uart_server"`（`sle_speed_server_adv.c:36`）——强血缘证据 |
| 构建 | OpenHarmony `hb` + `config.py`/`ohos.cmake` + `BUILD.gn` | FBB 统一平台：Kconfig choice + `build_config.json` + CMake（`samples/bt/Kconfig`、`src/build.py`） |
| 头文件来源 | 同一批 `sle_ssap_server.h`/`sle_device_discovery.h`（SDK `include/middleware/services/bts/sle/`，`fbb_ws63/src/include/...` 下可见） | 同左（原生头） |

底层 SSAP 协议结构（服务/特征/描述符/权限位）两边一致，可互操作；差异在应用层 UUID 与命令/API 命名。互连时以 **HHD-01 实际烧录固件** 的 UUID 为准（23_sle_uart=0x2222/0x2323；fbb_ws63 的 sle_uuid 样例=0xABCD/0x1122；AT 固件=0x2222/0x2323）。

---

## 9. 明天 AT 桥接路径（补充，含对 PC 栈意义）

- **零编译路径**：烧基础固件 → 串口 AT 把 HHD-01 配成 SSAP server（§6.2 序列）→ PC 栈 scan/connect/find/读写 0x2222/0x2323。可先于任何 WS63 编译工作完成互连验证。
- **意义**：AT 固件 = HiSilicon 官方对 SSAP 服务的"参考客户端/服务端"，其命令参数（UUID、handle、权限位 `5,5,2`=read|write/read|write/CCCD 值 0x0200）直接对应 OHOS SSAP API 的参数语义，可用来校验我们 `stack/ssap` 编解码与 server 实现的字段定义（对照 `ssap_server.h`/`ssap_codec.h`）。
- **风险**：AT 词汇有两代（OH 时代 vs FBB 时代，§6.3），且该 PDF 未覆盖雷达/产线工装指令；板级 AT 固件具体版本需 `AT+SYSINFO` 实测 [VERIFY]。

---

## 10. Open Questions

1. [VERIFY] HHD-01 出厂固件是 AT 基础固件还是 demo 固件？AT 命令词汇属于 OH 时代（SSAPSADDSRV…）还是 FBB 时代（SLESSAPSERREGISTER…）？—— 决定 §6/§7 走哪条路。用 `AT+SYSINFO` 实测。
2. [VERIFY] `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg` 是否就是 AT 基础固件？能否直接烧到 HHD-01（HH-D01=WS63）？
3. [VERIFY] 板级烧录工具/流程（HiBurn vs HiSpark Studio vs DevEco）、USB 串口芯片（CH340G?）、供电方式 —— 均未在可读文档中确认。
4. 23_sle_uart client 的写 handle 用的是服务起始 handle 而非特征 handle（`sle_uart_client.c:300` vs `:245-247`）——对端 server 是否按服务 handle 路由写请求？我们栈做 client 时应写哪个 handle？（看 WS63 服务端 `ssaps_write_request_callbacks` 未区分，`sle_uart_server.c:400-408`）
5. demo 同时声称 "WS63"（firmware 映射）与 "WS63E 解决方案"（demo README）——HHD-01 实体芯片丝印是 WS63 还是 WS63E？两者 SDK 兼容（同 WS63V100），不影响互连，但影响选 fwpkg 镜像。
6. fbb_ws63 `sle_speed_*`（吞吐样例）与服务端 `sle_uart_server` 广播名共用 —— 若环境里有吞吐节点，scan 匹配会误连；需确认对端真是 23_sle_uart 广播。
7. WS63 SDK 版本门槛：26_sle_gas 要求 SDK≥1.10.102（`README_CN.md:100`）；23_sle_uart 未提版本。fbb_ws63 的 WS63 SDK 版本是否满足、与 OH 路径 SDK 是否同源未知 [VERIFY]。
