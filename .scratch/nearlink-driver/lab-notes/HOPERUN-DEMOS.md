# HopeRun-NearLink repo — demo catalog & HH board/module survey (for HHD-01 first-day bring-up)

**Date:** 2026-08-17
**Status:** Read-only local research. No builds, no flashing, no hardware. PDFs are NOT parsed (only filenames); all claims are from file:line of text sources (READMEs, .c/.h). Items marked [VERIFY] must be confirmed against the physical board / PDFs.

---

## 1. Sources

- `/mnt/hdd/nearlink-stuff/HopeRun-NearLink/` (HopeRun official repo mirror, `origin https://github.com/HopeRunORG/NearLink.git`)
  - `demo/` — 34 demo directories (28 numbered 00–27 + 6 unnumbered)
  - `firmware/README.md` — module/board/chip mapping
  - `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg`, `firmware/BS21/bs21_all_in_one.fwpkg`
  - `HH-D01/`, `HH-D02/`, `HH-D03/`, `HH-K01/`, `HH-M01/`, `HH-M02/`, `HH-M03/`
  - `Image/` — demo screenshot dirs (15/16/17/18/20/21/22 tcp/udp/mqtt + HH-D03 + HH-K01)
- Cross-refs (already-written lab notes):
  - `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/HHD01-BOARD.md` — 23_sle_uart deep-dive + HHD-01 board overview
  - `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/WS63-BUILD-FLASH.md` — build/flash manual

---

## 2. Demo catalog — 28 numbered + 6 unnumbered

All numbered demos share the same README boilerplate ("HopeRun IoT Development Kit based on WS63E solution", `demo/<n>/README.md:1-5`) and target the OpenHarmony/Oniro app path (`applications/sample/wifi-iot/app`, product `nearlink_dk_3863`, e.g. `demo/23_sle_uart/README.md:94`). Compile pattern is identical for all: copy dir → add to `BUILD.gn` features → add ram_component in `config.py` → add to `ohos.cmake` COMPONENT_LIST → `hb set -p nearlink_dk_3863 && hb build -f`.

### 2.1 Category A — RTOS primitives (00–05), pure software, NOT NearLink-related
| # | Name | What it does | NearLink? |
|---|---|---|---|
| 00 | thread | CMSIS-RTOS2 thread create/state/prio/suspend/resume etc. (`README.md:8-30`) | No |
| 01 | timer | osTimerNew/Start/Stop/isrunning (`README.md:8-19`) | No |
| 02 | delay | osDelay + osDelayUntil tick wait (`README.md:10-16`) | No |
| 03 | mutex | Protect shared counter across threads (`README.md:14-21`) | No |
| 04 | semaphore | Producer-consumer with semaphores (`README.md:14-20`) | No |
| 05 | message | MessageQueue put/get/capacity (`README.md:8-18`) | No |

### 2.2 Category B — Peripherals (06–13), board-level, NOT NearLink-related
| # | Name | Peripheral | Pin facts (source of truth) |
|---|---|---|---|
| 06 | gpioled | GPIO output → LED blink | `#define LED_TASK_GPIO 10` (`06_gpioled/led_example.c:27`); README result says "yellow LED on LED board (GPIO10)" (`06_gpioled/README.md:49`); note README line 21 says GPIO9 — stale, source uses GPIO10 |
| 07 | gpiobutton | GPIO input interrupt → toggle LED | `LED_TASK_GPIO 10`, `IOT_GPIO_KEY 13` (`07_gpiobutton/button_example.c:25-26`) |
| 08 | pwmled | PWM breathing LED | `PWM_GPIO = GPIO_10`, IoSetFunc(10,1)+IoTPwmInit (port 1) (`08_pwmled/pwm_demo.c:24-38`) |
| 09 | adcbutton | ADC reads OLED-board buttons | OLED buttons 1/2 → GPIO8 (ADC1) (`09_adcbutton/README.md:12`); reads voltage via `adc_port_read` (`09_adcbutton/adc_button_demo.c:33-40`) |
| 10 | adchuman | ADC motion detection (RGB board) | Motion sensor → GPIO12 (ADC5) (`10_adchuman/README.md:12`); channel 5 (`10_adchuman/adc_human_demo.c:34`) |
| 11 | aht20 | I2C AHT20 temp/humidity | GPIO15→I2C1_SDA, GPIO16→I2C1_SCL, baud 400k, `AHT20_I2C_IDX 1` (`11_aht20/aht20_demo.c:25-26,40-45`); `AHT20_GetMeasureResult(temp,humi)` (`11_aht20/README.md:32`) |
| 12 | oled | I2C SSD1306 OLED | `OLED_I2C_IDX 1` (I2C1), addr 0x3C, 400 kHz (`12_oled/src/oled_ssd1306.c:24-28`); driver split `12_oled/src` (oled_ssd1306) + `12_oled/demo` (Hello,World) (`12_oled/demo/oled_demo.c:38-42`) |
| 13 | adclight | ADC photoresistor (RGB board) | Photoresistor → GPIO9 (ADC2) (`13_adclight/README.md:13`); channel 2 (`13_adclight/adc_light_demo.c:34`) |

ADC demos 09/10/13 each ship an `adc_driver/` patch set (替换1..4 + `替换文件步骤.txt`) required when SDK < 1.10.102 — see `09_adcbutton/README.md:37`.

### 2.3 Category C — Network (14–22), WiFi/BLE/MQTT, NOT SLE (BLE_uart is BLE, not NearLink)
| # | Name | What it does | Notes |
|---|---|---|---|
| 14 | easy_wifi | WiFi STA connect + AP hotspot (`14_easy_wifi/README.md:4-46`) | lib-style dir (`src/` + `demo/`) |
| 15 | tcpclient | WS63 as TCP client | `TCP_SERVER_IP 192.168.8.48:1234` (`15_tcpclient/connect_wifi_test.c:31-34`); needs 12_oled + 14_easy_wifi (`README.md:7-8`) |
| 16 | tcpserver | WS63 as TCP server (listen/accept) (`16_tcpserver/tcp_server_test.c:86-111`) | needs 12_oled + 14_easy_wifi |
| 17 | udpclient | WS63 as UDP client | `UDP_SERVER_IP 192.168.100.199:1234` (`17_udpclient/connect_wifi_test.c:31-34`); needs 12_oled + 14_easy_wifi |
| 18 | udpserver | WS63 as UDP server | needs 12_oled + 14_easy_wifi |
| 19 | ble_uart | **BLE** GATT serial forwarding (see §3) | needs 2 boards |
| 20 | mqtt_demo | MQTT pub/sub vs local Mosquitto + MQTTX | `MQTT_SERVER_IP 192.168.8.31`, port 1888 (`20_mqtt_demo/mqtt_demo.c:33-36`); topic `topic/a` (`README.md:93`); needs paho_mqtt |
| 21 | mqtt_led | MQTTX controls traffic-light red LED | `RED_GPIO 7` (`21_mqtt_led/mqtt_led_demo.c:42-66`); topic `control_led_topic` (`21_mqtt_led/mqtt_led_demo.c:184`); needs paho_mqtt |
| 22 | mqtt_sensor | MQTT publishes AHT20 temp/humidity | topic `device_sensor_data` (`22_mqtt_sensor/mqtt_sensor_demo.c:175`); AHT20 on I2C1 GPIO15/16 (`22_mqtt_sensor/mqtt_sensor_demo.c:32-75`); needs paho_mqtt |

All MQTT demos depend on `paho_mqtt` (MQTTPacket + MQTTClient-C) and require commenting out the SDK's built-in `# "mqtt"` ram_component (`20_mqtt_demo/README.md:17-26`).

### 2.4 Category D — SLE (23–27), NearLink-relevant (23 already deep-dived in HHD01-BOARD.md)
| # | Name | What it does | Key pins/behavior |
|---|---|---|---|
| 23 | sle_uart | SLE SSAP serial forwarding, 2 boards | service UUID 0x2222, char 0x2323, MTU 520 — see HHD01-BOARD.md §4 |
| 24 | sle_humi | SLE sends AHT20 temp/humidity every 1 s | server→client every 1 s, 115200 (`24_sle_humi/README_CN.md:103`); AHT20 I2C1 |
| 25 | sle_led | SLE toggles traffic-light LED via USER key | press server-board USER button → client-board traffic LED toggles (`25_sle_led/README_CN.md:102`) |
| 26 | sle_gas | SLE sends MQ-2 gas sensor + buzzer alarm | buzzer gpio9(PWM1), MQ-2 ADC gpio11(ADC4) (`26_sle_gas/README_CN.md:53-54`); sends every 100 ms, buzzer if ADC >1000 (`26_sle_gas/README_CN.md:103`) |
| 27 | sle_oled | SLE serial forwarding + OLED display | received data shown on OLED, ≤64 bytes English (`27_sle_oled/README.md:98`) |

### 2.5 Unnumbered 6 (duplicates/variants of numbered ones)
| Dir | Is | Note |
|---|---|---|
| button_example | variant of 07_gpiobutton | `button_example_main.c` vs `07/button_example.c` differ; has `hal_iot_gpio_ex.c`/`iot_gpio_ex.h`/`iot_ssl_gpio.h` |
| demo_uart | UART echo/loopback sanity | IoTUartInit/Write; "Serial Port 1 and 2 print received data", 115200 (`demo_uart/README.md:71`) |
| easy_wifi | variant of 14_easy_wifi | `src/`+`demo/`, wifi_connect_demo + wifi_hotspot_demo; README differs from 14's |
| led_demo | RGB traffic-light board | cycles RGB (GPIO7/10/11) (`led_demo/README.md:77`); `app_demo_led_control.c:30-37` |
| oled_demo | variant of 12_oled | same `oled_ssd1306.c` driver, different BUILD.gn/README |
| sle_uart_demo | lower-level variant of 23_sle_uart | raw OH SLE API (EnableSle, SleSetAnnounceData, SleStartAnnounce/Seek...) (`sle_uart_demo/README.md:8-23`); files sle_uart_server/client.c |

---

## 3. BLE demo — 19_ble_uart

- **What it is:** WS63 uses **Bluetooth** (BLE GATT) to forward serial (UART) data between **two** boards — server + client. Requires two boards; server and client compiled by flipping the `sources` list in BUILD.gn (`19_ble_uart/README.md:56-74`).
- **Identity:** GATT custom service `BLE_UART_UUID_SERVER_SERVICE 0xABCD`, TX char `0xCDEF`, RX char `0xEFEF`, CCCD `0x2902` (`19_ble_uart/ble_uart_server.h:28-34`). Client uses scan + GATT connect (`ble_uart_client_scan.c`).
- **Relationship to SLE passthrough (23_sle_uart):** exact functional twin but on a different radio stack. 19 uses BLE (`bts_le_gap.h`/`bts_gatt_client.h`, `19_ble_uart/ble_uart_client.c:22-24`); 23 uses SLE SSAP (`SsapsRegisterServer`/`SsapsNotifyIndicate`, see HHD01-BOARD.md §4). Both are "radio-transparent serial" — 19 is the BLE precursor; 23 is the NearLink version we actually target for interop. BLE is irrelevant to our WS73/SLE work except as a readable architecture reference (announce/seek ≈ adv/scan pattern in 23).

---

## 4. Other HH boards & modules

### 4.1 Chip map (`firmware/README.md:9-13`)
| Module | Board | Chip |
|---|---|---|
| HH-M01 | HH-D01 | WS63 |
| HH-M02 | HH-D02 | WS63E |
| HH-M03 | HH-D03 | BS21 |

Firmware blobs present: `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg`, `firmware/BS21/bs21_all_in_one.fwpkg`.

### 4.2 Boards
- **HH-D02** (WS63E): board-doc set nearly identical to HH-D01 — same `board/IO复用关系.md` (byte-identical to D01's, verified `diff`), same 24 `WS63V100 *.pdf` + `ws63-Document-Cn.chm` (`HH-D02/board/`). Extra top-level: `HH-D02 开发板使用手册-V1.2.pdf` (use manual), Product Manual V02, Specs V03, schematic, tri-color LED spec. **Relevance:** HHD-01's WS63 and HH-D02's WS63E share the WS63V100 SDK/docs, so D02's use manual is a cross-check for HHD-01 first-day flow [VERIFY against D01's own manuals].
- **HH-D03** (BS21): only 3 files — `HH-D03 星闪开发板规格说明书-20241212-V02.pdf`, `HH-D03 原理图.pdf`, `HH-D03 AT手册.pdf`. **Ignore** (BS21 = different chip/SDK; confirmed irrelevant per instructions).
- **HH-K01** (星闪派物联网开发套件, NearLink Pi IoT dev kit): the kit every demo README displays (`Image/HH-K01.png`) and compiles for (product `nearlink_dk_3863`, `23_sle_uart/README.md:94`). Files: `HH-K01 星闪派物联网开发套件产品说明书-20240816-V02.pdf` + `使用说明书_V1.1.pdf`, plus schematics `原理图/HIHOPE_NEARLINK_DK_3863E_V03.pdf` (mainboard, WS63E 3863E), and `HiSpark_WiFi_IoT_{EM,EXB,OLED,SSL}_VER.A.pdf` (EM = core board, EXB = expansion, OLED, SSL = traffic light). So HH-K01 is a **kit** (mainboard + expansion shields) whose core is the 3863E board — the demos target the kit's boards, not a bare HHD-01. Relevant because our 06–13/19–22 demos assume this kit's board pinouts.

### 4.3 Modules (PDFs not parsed — params inferred from filenames only, [VERIFY])
- **HH-M01** (WS63): `HH-M01 NearLink Module Product Manual - 20241212-V03.pdf` + `Specifications Manual - 20241212-V03.pdf`. Used on HH-D01.
- **HH-M02** (WS63E): Product Manual V02 + Specifications V03 + `HH-SPARK-WS63E_V03.pdf` — the WS63E module appears brand-named **"HH-SPARK-WS63E"**. Used on HH-D02.
- **HH-M03** (BS21): only `HH-M03 星闪模组规格说明书-20241212-V02.pdf`. Used on HH-D03.

No size/interface numbers extractable without parsing PDFs. **TV-box integration possibility:** plausible — WS63/WS63E are small radio SoCs (WiFi+BLE+SLE+radar, 19 IO pins per `IO复用关系.md`), and the modules are packaged stand-alone (`HH-SPARK-WS63E` naming implies a solderable RF module). But module pinout/dimensions/antenna type live in the unparsed Spec PDFs — [VERIFY] before any TV-box hardware design. Note also the WS63V100 SDK covers WiFi+BLE+SLE+radar (`HH-D01/board/README.md` doc table), making a WS63/WS63E module the realistic "integrated-module" candidate for a TV-box accessory, not BS21 (HH-M03).

---

## 5. HHD-01 on-board peripheral verification path

From HHD01-BOARD.md §2.2, HHD-01 on-board/exposed peripherals: RGB tri-color LED, USER button (GPIO13), UART0 115200; expansion peripherals AHT20 (I2C1 GPIO15/16), MQ-2 gas (ADC4=GPIO11), buzzer (GPIO9 PWM1), SSD1306 OLED (I2C1 0x3C).

**Single-board (no second board needed) verification demos — recommended first-day burn order:**
1. `demo_uart` — UART sanity: serial echo at 115200 proves flash+console OK (`demo_uart/README.md:71`). Equivalent to any demo's boot log, but the smallest footprint.
2. `06_gpioled` — on-board LED blink on GPIO10 (`06_gpioled/led_example.c:27`, `README.md:49`). Proves GPIO output + board LED wiring.
3. `07_gpiobutton` — USER button (GPIO13) toggles LED (GPIO10) (`07_gpiobutton/button_example.c:25-26`). Proves GPIO input interrupt + USER key.
4. `08_pwmled` — PWM breathing on GPIO10 (`08_pwmled/pwm_demo.c:24-38`). Proves PWM peripheral.
5. `11_aht20` — AHT20 temp/humidity over I2C1 (`11_aht20/aht20_demo.c:25-45`). Proves I2C + on-board sensor.
6. `12_oled` — SSD1306 "Hello,World" on I2C1 0x3C (`12_oled/src/oled_ssd1306.c:24-28`, `12_oled/demo/oled_demo.c:38-42`). Proves I2C + OLED.

If only HHD-01 + no expansion shield: steps 1–4 are guaranteed; 5–6 need the AHT20/OLED expansion boards. ADC demos (09/10/13) need the OLED/RGB expansion boards and require the SDK ≥1.10.102 ADC driver patch (`09_adcbutton/README.md:37`).

**Two-board / network demos** (all need a second WS63 board, a PC broker, or WiFi AP): 14–22 and 23–27. Among these, `23_sle_uart` is the primary SLE interop target (see HHD01-BOARD.md); `24_sle_humi` / `26_sle_gas` / `25_sle_led` / `27_sle_oled` are the same SLE server/client plumbing with different expansion sensors and are useful as near-copies if 23's UUID/MTU must be tweaked.

---

## 6. Conclusion — first-day HHD-01 plan

1. Build/flash path: fbb_ws63 SDK build (`build.py -c ws63-liteos-app`) → `ws63-liteos-app_all.fwpkg` (WS63-BUILD-FLASH.md), or HopeRun OH path (`hb set -p nearlink_dk_3863`).
2. On-board self-check (single board): `demo_uart` → `06_gpioled` → `07_gpiobutton` → `08_pwmled` → (`11_aht20` → `12_oled` if expansion shields present). None of these involve NearLink; they validate the board before radio work.
3. SLE interop: `23_sle_uart` (already deep-dived) with two boards; optionally `25_sle_led` as the fastest "SLE works" smoke test (press USER key → remote LED toggles).
4. Ignore: BS21 (HH-D03, HH-M03), BLE (19_ble_uart) except as code reference.
5. Kit-vs-board caveat: demo pinouts assume the HH-K01 kit's boards; HHD-01's own expansion pinout should be cross-checked against `HH-D01 开发板原理图.pdf` and Product Manual before wiring [VERIFY].

---

## 7. Open questions

- HH-D02 use manual (`HH-D02 开发板使用手册-V1.2.pdf`) content — likely the fastest A-to-Z flash+run reference; parseable only with a PDF tool (not done, out of scope).
- Module HH-M01/M02/M03 physical params (dimensions, antenna, footprint) — in unparsed Spec PDFs. TV-box integration feasibility rests on this.
- Exact HHD-01 expansion interface vs HH-K01 EXB/SSL/OLED shield pin compatibility — needs schematic PDFs.
- `06_gpioled` README line 21 says LED on GPIO9 but source and result text say GPIO10 — minor upstream doc inconsistency; trust source (`led_example.c:27`).
- Whether 24/26/27 SLE demos are drop-in on HHD-01 without the HH-K01 kit's expansion shields (they assume the kit's sensor boards).
