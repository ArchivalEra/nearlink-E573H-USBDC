---
type: harvest
title: Xinghongpai WS63 firmware program knowledge
language: zh
created: 2026-09-12
tags: []
---

# Xinghongpai WS63 firmware program knowledge

- Inspection date: 2026-09-11
- Source root: `/mnt/hdd/nearlink-stuff/xinghongpai-nearlink-dev-board/firmware/`
- Source revision inspected earlier in this harvest: `914e6c22762f1848231c625db78333ec7a767fdc`
- Mode: read-only program-source inspection; no build, hardware access, or PCB analysis
- Scope: OpenHarmony/Hi3863 application structure, SDK API families, peripheral drivers, Wi-Fi/lwIP behavior, reusable program knowledge, and concrete porting hazards

## Eight-line summary

1. This is an external-SDK-dependent OpenHarmony/Hi3863 example collection, not a standalone firmware project or a maintainer-validated one-command build.
2. The examples use three visible application-registration styles: `APP_FEATURE_INIT`, `SYS_RUN`, and newer `app_run` plus `osal_kthread_create`.
3. The useful reusable material is peripheral protocol logic: AHT20 I2C sequencing, SSD1306 I2C commands, ADC sampling, Wi-Fi STA/SoftAP state machines, and basic lwIP sockets.
4. The tree mixes old `IoTGpio*`/`IoTI2c*` APIs with newer `uapi_*` APIs and contains board-specific pin choices that must not be copied as universal WS63 mappings.
5. No exact `SLE`, `SSAP`, `HADM`, `DLI`, `NearLink`, `SparkLink`, or `USB` protocol implementation occurs in the firmware tree; it does not advance the WS73 USB/SSAP stack directly.
6. Several network examples contain fixed-buffer, string-termination, error-path, and socket-lifetime defects; they are demonstrations, not transport templates.
7. The Wi-Fi helpers still contain hardcoded demo SSIDs/passwords and unbounded retry loops, so credentials and failure policy must be replaced before reuse.
8. For WS73/HHD-01 work, borrow only protocol algorithms and API-dialect observations after checking the target SDK version, pin map, ownership/lifetime rules, and binary-safe framing.

## 1. Repository and build model

The firmware README states that `examples/` comes from the original `02-程序源码.zip`, depends on an external OpenHarmony/HiSilicon WS63 SDK, and has no maintainer-reviewed universal SDK version or one-click build command. It explicitly requires checking the SDK version, example build file, pin assignments, credentials, and full build log before use. [firmware/README.md:2-14]

Most examples are GN `static_library` units rather than complete applications. For example, the RTOS thread example declares `static_library("thread_demo")` with one source and SDK include paths. [examples/00_thread/BUILD.gn:13-20] The AHT20 example similarly packages the driver, demo, and GPIO compatibility source as `static_library("i2c_demo")`. [examples/11_aht20/BUILD.gn:13-26] The OLED example packages its HAL compatibility source, display driver, and demo into one static library. [examples/201_oled/BUILD.gn:1-28]

The top-level application does not enable the whole catalog. Its `lite_component("app")` enables `swb:swb` and `201_oled:201_oled`; Wi-Fi, AHT20, and heart-rate features remain commented out. [examples/BUILD.gn:15-25] The local README's suggested order is OS primitives, board peripherals, networking, then integrated `100`/`200`/`300` applications, while warning that directory names do not prove independent testing. [firmware/README.md:16-23]

The AHT20 and ADC READMEs document a manual SDK integration sequence: copy the example under `applications/sample/wifi-iot/app`, add a feature to the parent `BUILD.gn`, add the component to WS63 SDK `config.py` and `ohos.cmake`, then run `hb set -p nearlink_dk_3863 && hb build -f`. These are documented procedures, not validation by this repository. [examples/11_aht20/README.md:33-54] [examples/13_adclight/README.md:14-37]

Concrete build-shape hazards:

- The ADC example says SDK versions below `1.10.102` may require replacing four SDK ADC locations with the bundled alternatives. [examples/13_adclight/README.md:37-41] The replacement instruction names SDK `porting/adc`, `drivers/driver/adc`, `drivers/hal/adc`, and `include/driver/adc.h`. [examples/13_adclight/adc_driver/替换文件步骤.txt:1-4]
- The TCP client GN file includes `../12_oled/src`, but this firmware tree exposes the OLED implementation as `201_oled`, making that include path stale for a standalone copy. [examples/15_tcpclient/BUILD.gn:29-31]
- The integrated temperature build contains a triple-slash SDK include path, `///device/soc/...`, which should be normalized before treating the GN file as canonical. [examples/205_temperature/BUILD.gn:17-20]
- Traffic, RGB/motion, and relay examples include paths from `hi3516dv300` SDK sources alongside WS63 paths, showing that some helpers were copied across SoC example trees. [examples/103_traffic/BUILD.gn:12-22] [examples/105_colorful_lights/BUILD.gn:10-19] [examples/106_relay/BUILD.gn:9-17]

## 2. Application entry and task models

The source package mixes multiple SDK generations and registration macros:

- `APP_FEATURE_INIT` is used by the CMSIS-RTOS2 primitive examples, OLED, and lwIP network applications. [examples/00_thread/thread.c:105] [examples/201_oled/demo/oled_demo.c:54] [examples/15_tcpclient/connect_wifi_test.c:82]
- `SYS_RUN` is used by GPIO LED, ADC light, fan, traffic, RGB/motion, and relay examples. [examples/101_gpioled/led_example.c:55] [examples/13_adclight/adc_light_demo.c:67] [examples/103_traffic/main.c:155]
- Newer integrated examples use `app_run(test_entry)` and create tasks with `osal_kthread_create`, as shown by flame, temperature, smoke, and soil applications. [examples/102_flame/main.c:42-55] [examples/205_temperature/main.c:139-152] [examples/302_smoke/main.c:50-63]

The older OS-primitive examples consistently use CMSIS-RTOS2 `osThreadAttr_t`, usually with a 2 KiB stack and normal priority, then query or exercise thread state, priority, suspend/resume, stack information, and termination. [examples/00_thread/thread.c:20-31] [examples/00_thread/thread.c:48-85] Timer, mutex, counting/binary semaphore, and message-queue examples exercise the corresponding CMSIS-RTOS2 APIs. [examples/01_timer/timer.c:28-56] [examples/03_mutex/mutex.c:57-83] [examples/04_semaphore/semp.c:79-101] [examples/05_message/message.c:81-110]

These examples are useful as an API-dialect map, but not as lifecycle templates: several terminate worker threads and immediately delete the shared mutex, semaphores, or queue without joining or otherwise proving that workers have stopped. [examples/03_mutex/mutex.c:74-83] [examples/04_semaphore/semp.c:91-101] [examples/05_message/message.c:93-110]

## 3. GPIO, pin multiplexing, and peripheral helpers

The examples mix two API families:

- Older examples call `IoTGpioInit`, `IoTGpioSetDir`, and `IoTGpioSetOutputVal` directly. The LED example identifies GPIO 14 as its LED and toggles it from a CMSIS-RTOS2 task. [examples/101_gpioled/led_example.c:9-33]
- Newer helpers initialize the pin controller and GPIO subsystem, select a pin mode, then use `uapi_gpio_set_dir`/`uapi_gpio_set_val` or `uapi_gpio_get_val`. The traffic-light helper has special pin-mode handling for GPIO 4 and GPIO 5 before configuring outputs. [examples/103_traffic/traffic_light/traffic_light.c:5-74]

The key-programming lesson is that a numeric GPIO alone is insufficient: the examples repeatedly select `PIN_MODE_0`, `PIN_MODE_2`, or `PIN_MODE_4` depending on the pin and peripheral function. The segment-display helper again special-cases GPIO 4 and GPIO 5 and enables enhanced drive strength for GPIO 14. [examples/103_traffic/segment_display/segment_display.c:33-67]

Other peripheral helper behavior:

- The fan helper drives two GPIOs in opposite states for forward/reverse operation. [examples/101_fan_demo/fan/fan_demo.c:3-29]
- The traffic application registers a low-active, 20 ms-debounced key on GPIO 0, polls it every 10 ms, and feeds a watchdog after 100 iterations. [examples/103_traffic/main.c:75-130] The key helper implements a four-state debounce state machine and stores at most two keys. [examples/103_traffic/key/key_demo.c:3-42] [examples/103_traffic/key/key_demo.c:79-165]
- The seven-segment helper multiplexes two digits every 8 ms and contains a common-anode segment table. [examples/103_traffic/segment_display/segment_display.c:18-31] [examples/103_traffic/segment_display/segment_display.c:248-275]
- The RGB helper explicitly says it simulates PWM with a GPIO threshold and that real applications should use hardware PWM. It is therefore not a suitable buzzer/PWM reference. [examples/105_colorful_lights/rgb/rgb_demo.c:16-39]
- The motion and flame helpers are simple polled digital inputs. The flame comment says a high level indicates detection, while the return expression treats a low level as detection, an internal documentation/code contradiction. [examples/105_colorful_lights/sr602/sr602_demo.c:27-39] [examples/102_flame/flame/flame_demo.c:27-39]
- The relay helper assumes active-high operation and contains redundant/conflicting pin-mode selections; its comment explicitly calls the polarity an assumption. [examples/106_relay/relay/relay_demo.c:3-40]

## 4. AHT20 I2C driver

The older AHT20 example uses I2C bus 1 at 400 kHz, address `0x38`, and GPIO 15/16 as SDA/SCL. [examples/11_aht20/aht20_demo.c:24-46] The integrated temperature example instead uses `uapi_i2c_master_init(0, 400000, 0)` with SCL/SDA on pins 17/18. [examples/205_temperature/main.c:23-25] [examples/205_temperature/main.c:74-85] This disagreement is direct evidence that pin assignments are example/revision specific and must not be generalized.

The driver implements the AHT20 command sequence:

- Status command `0x71`, calibration command `0xBE 0x08 0x00`, trigger command `0xAC 0x33 0x00`, and soft reset `0xBA`. [examples/11_aht20/aht20.c:24-43]
- Calibration checks busy bit 7 and calibrated bit 3, resets and recalibrates when needed, then waits 20 ms after reset and 40 ms after the calibration command. [examples/11_aht20/aht20.c:134-161]
- Measurement polls busy for up to ten reads, sleeping 75 ms between reads. [examples/11_aht20/aht20.c:164-200]
- Humidity is assembled from bytes 1-3 plus the high nibble of byte 4 and converted as `raw / 2^20 * 100`. Temperature uses the low nibble of byte 4 plus bytes 5-6 and converts as `raw / 2^20 * 200 - 50`. [examples/11_aht20/aht20.c:202-214]

Reusable program knowledge: keep command framing, busy polling, null-pointer validation, and conversion math separate from board pin configuration. Porting hazard: the calibration path reads six bytes immediately after the status command, and the source does not document the exact SDK I2C transfer semantics beyond the calls; validate that behavior against the target SDK and sensor datasheet. [examples/11_aht20/aht20.c:136-159]

## 5. SSD1306 OLED driver

The OLED driver uses I2C bus 1 at 400 kHz, address `0x3C`, command prefix `0x00`, and data prefix `0x40`. [examples/201_oled/src/oled_ssd1306.c:22-31] It configures GPIO 15/16 for I2C1 and sends a complete SSD1306 initialization command table before enabling the display. [examples/201_oled/src/oled_ssd1306.c:65-120]

The drawing model uses x in pixels and y in 8-pixel pages, fills eight pages of 128 bytes, and supports 8x16 or 6x8 fonts. [examples/201_oled/src/oled_ssd1306.c:123-151] [examples/201_oled/src/oled_ssd1306.c:156-223] The demo creates a 4 KiB task, initializes the display, clears it, and prints `Hello,World!`. [examples/201_oled/demo/oled_demo.c:23-54]

For reuse, the command/data control-byte sequence and page addressing are portable display-driver knowledge. The GPIO numbers and bus index are not portable across these examples.

## 6. ADC examples and SDK compatibility

The light example documents GPIO 9/ADC2 and samples ADC channel 2 every 100 ms. [examples/13_adclight/README.md:10-13] [examples/13_adclight/adc_light_demo.c:28-42] Smoke and soil examples instead sample channel 0 ten times at two-second intervals and explicitly warn that raw millivolts may differ from the external voltage when a divider is present. [examples/302_smoke/main.c:18-44] [examples/303_soil/main.c:18-44]

The bundled ADC porting code shows the SDK API shape: `uapi_adc_init`, `uapi_adc_deinit`, `uapi_adc_power_en`, and `adc_port_read`. [examples/13_adclight/adc_driver/替换4/adc.h:126-152] The porting implementation registers the v154 HAL and LSADC interrupt, reads calibration values from efuse, and stores the last automatic-scan result in a global variable. [examples/13_adclight/adc_driver/替换1/adc/adc_porting.c:58-80] [examples/13_adclight/adc_driver/替换1/adc/adc_porting.c:103-123]

Concrete portability defects in that helper:

- `adc_port_callback` indexes `buffer[length - 1]` without checking that `length` is nonzero. [examples/13_adclight/adc_driver/替换1/adc/adc_porting.c:126-131]
- `adc_port_read` assigns `*data` only inside `#if defined(CONFIG_ADC_SUPPORT_AUTO_SCAN)`. If that configuration is absent, the function can return success without initializing the output value. [examples/13_adclight/adc_driver/替换1/adc/adc_porting.c:133-151]

These examples are useful for identifying the SDK generation and API migration boundary, but their raw ADC values are not calibrated sensor outputs.

## 7. Wi-Fi STA and SoftAP state machines

The `easy_wifi` support library exposes two paths:

- STA connection through `ConnectToHotspot`, scan/match/connect, lwIP `wlan0`, DHCP, and `DisconnectWithHotspot`. [examples/14_easy_wifi/src/wifi_connecter.c:95-105] [examples/14_easy_wifi/src/wifi_connecter.c:175-293]
- SoftAP through `StartHotspot`, `ap0`, static addressing, and DHCP server startup. [examples/14_easy_wifi/src/wifi_starter.c:31-97]

The STA helper's state sequence is enable, scan, copy the matching SSID/BSSID/security/key, connect, wait for association, find `wlan0`, start DHCP, and wait for an IPv4 address. [examples/14_easy_wifi/src/wifi_connecter.c:175-287] The integrated temperature tree contains a newer retry-counting variant that disables STA after ten failed attempts. [examples/205_temperature/wifi/wifi_connect.c:298-384]

The SoftAP helper configures a local subnet and starts `dhcps`; the newer STA helper separately demonstrates SoftAP address, advanced Wi-Fi parameters, DHCP server startup, and cleanup. [examples/14_easy_wifi/src/wifi_starter.c:44-96] [examples/205_temperature/wifi/wifi_sta.c:11-70] [examples/205_temperature/wifi/wifi_sta.c:73-102]

Program-level hazards:

- The older STA connection uses a global state variable and an outer `while (a)` where `a` remains 1; failures retry without a global attempt limit. [examples/14_easy_wifi/src/wifi_connecter.c:42-51] [examples/14_easy_wifi/src/wifi_connecter.c:188-238]
- The scan callback only records scan completion, while connection state is also polled; a port should define one authoritative state machine rather than copying both mechanisms blindly. [examples/14_easy_wifi/src/wifi_connecter.c:69-93] [examples/14_easy_wifi/src/wifi_connecter.c:219-238]
- The old helper starts DHCP again after it has already observed a nonzero IPv4 address. [examples/14_easy_wifi/src/wifi_connecter.c:267-283]
- The source still contains hardcoded demo SSIDs/passwords, including in the TCP client. These must be replaced with local test credentials and must not be propagated into commits. [examples/14_easy_wifi/demo/wifi_connect_demo.c:22-24] [examples/15_tcpclient/connect_wifi_test.c:23-33] Placeholder credentials also remain in several server/client examples. [examples/16_tcpserver/connect_wifi_test.c:24-33] [examples/205_temperature/main.c:27-30]
- The SoftAP helper copies `WIFI_MAX_KEY_LEN` bytes from the input key regardless of `strlen(key)`, which is unsafe for shorter input strings. [examples/14_easy_wifi/src/wifi_starter.c:48-52] The STA helper copies only `strlen(key)` bytes and does not explicitly append a terminator to the destination field. [examples/14_easy_wifi/src/wifi_connecter.c:163-169]

## 8. lwIP TCP and UDP examples

All network applications first call the shared STA helper, initialize the OLED, then create a task that calls the TCP/UDP test. [examples/15_tcpclient/connect_wifi_test.c:36-82] [examples/16_tcpserver/connect_wifi_test.c:35-79] [examples/17_udpclient/connect_wifi_test.c:36-80] [examples/18_udpserver/connect_wifi_test.c:33-77] The GN units link the shared Wi-Fi helper and lwIP 2.1.3 SDK include paths. [examples/15_tcpclient/BUILD.gn:13-35]

Verified API shape:

- TCP client: `socket(AF_INET, SOCK_STREAM, 0)`, `inet_pton`, `connect`, `send`, and `recv`. [examples/15_tcpclient/tcp_client_test.c:30-90]
- TCP server: `socket`, `bind(INADDR_ANY)`, `listen`, `accept`, `send`, and `recv`. [examples/16_tcpserver/tcp_server_test.c:29-133]
- UDP client: connected-less `sendto` and `recvfrom` with peer address capture. [examples/17_udpclient/udp_client_test.c:34-118]
- UDP server: bind to `INADDR_ANY`, then `recvfrom` to learn the peer. [examples/18_udpserver/udp_server_test.c:29-105]
- Integrated temperature: connect Wi-Fi, create a broadcast UDP socket, enable `SO_BROADCAST`, send AHT20 text to `255.255.255.255:8000` once per second. [examples/205_temperature/main.c:31-67] [examples/205_temperature/main.c:100-133]

These files should not be used as robust transport templates:

- TCP/UDP display buffers are 11 bytes, but the code writes `display_data[11] = '\0'` after copying 10 bytes; index 11 is outside the array. This occurs in all four standalone socket examples. [examples/15_tcpclient/tcp_client_test.c:22-25] [examples/15_tcpclient/tcp_client_test.c:103-106] [examples/16_tcpserver/tcp_server_test.c:24-27] [examples/16_tcpserver/tcp_server_test.c:146-149] [examples/17_udpclient/udp_client_test.c:23-28] [examples/17_udpclient/udp_client_test.c:123-125] [examples/18_udpserver/udp_server_test.c:23-27] [examples/18_udpserver/udp_server_test.c:110-112]
- The same examples clear buffers with `memset(..., strlen(buffer))` before the buffer is known to contain a terminator, which can read past the object. [examples/15_tcpclient/tcp_client_test.c:85-90] [examples/16_tcpserver/tcp_server_test.c:128-133]
- The UDP client passes `&response` to `recvfrom` even though `response` is already an array, then writes `response[ret]` even after `ret <= 0`; a full 100-byte datagram also writes one byte beyond the array. [examples/17_udpclient/udp_client_test.c:88-115]
- The UDP server sends its initial message to a zero-initialized peer address before any `recvfrom` has populated `clientAddr`; the first send therefore targets `0.0.0.0:0`, and the example never sends a response back to the discovered peer. [examples/18_udpserver/udp_server_test.c:38-78] [examples/18_udpserver/udp_server_test.c:88-105]
- TCP and UDP examples frequently continue after failure paths that already closed the socket, and they leave sockets open at the end of the infinite receive loop. [examples/15_tcpclient/tcp_client_test.c:45-62] [examples/15_tcpclient/tcp_client_test.c:112-114] [examples/16_tcpserver/tcp_server_test.c:62-107] [examples/17_udpclient/udp_client_test.c:51-73]
- The TCP client sends `sizeof(request)` rather than the string length, while most logging and display logic assumes C strings. [examples/15_tcpclient/tcp_client_test.c:22-24] [examples/15_tcpclient/tcp_client_test.c:72-83]
- The temperature application ignores the return value of `func_wifi_connect` and proceeds to create and use a socket. [examples/205_temperature/main.c:31-48]

A production adapter should use explicit lengths, bounded copies, checked return values, deterministic retry limits, owned peer state, and close/unwind paths for every failure branch.

## 9. Integrated application knowledge

- Traffic combines a polled/debounced key, a two-thread design, watchdog feeding, and an 8 ms multiplexed two-digit display. [examples/103_traffic/main.c:24-63] [examples/103_traffic/main.c:68-131]
- RGB plus motion is a simple polling loop: motion high selects red; otherwise green. [examples/105_colorful_lights/main.c:21-76]
- Heart-rate/SpO2 code initializes I2C on pins 16/15, waits on a MAX30102 interrupt GPIO, collects 500 red/IR samples, runs Maxim's algorithm, then maintains a 500-sample rolling buffer. [examples/202_heart_rate/max30102_test.c:31-113] [examples/202_heart_rate/max30102_test.c:117-198] It contains nested unbounded waits for the interrupt and repeatedly feeds the watchdog. [examples/202_heart_rate/max30102_test.c:82-110] [examples/202_heart_rate/max30102_test.c:138-157] The inspected source has no visible `APP_FEATURE_INIT`, `SYS_RUN`, or `app_run` entry macro even though its GN unit includes the test source. [examples/202_heart_rate/BUILD.gn:13-20]
- Smoke and soil are nearly identical ADC polling applications using channel 0 and ten samples. [examples/302_smoke/main.c:23-46] [examples/303_soil/main.c:23-46]
- The integrated temperature example is the clearest end-to-end application: Wi-Fi STA, broadcast UDP, newer I2C API, AHT20 calibration/measurement, text formatting, and a one-second loop. [examples/205_temperature/main.c:31-133]

## 10. Protocol-scope verdict

An exact-token search over C, header, GN, Markdown, text, CMake, and Python files in the firmware tree found zero occurrences of `SLE`, `SSAP`, `HADM`, `DLI`, `NearLink`, `SparkLink`, or `USB`. The firmware package therefore contains no direct implementation or dialect evidence for the WS73 Linux USB transport, SSAP client/server behavior, HADM ranging, or DLI framing.

Its actual program value is narrower:

1. A map of OpenHarmony/Hi3863 SDK API generations and application-registration macros.
2. I2C command sequencing for AHT20 and SSD1306.
3. ADC driver migration clues and SDK-version fallback files.
4. Wi-Fi STA/SoftAP and lwIP API usage.
5. Examples of what not to copy: unbounded retries, fixed string buffers, unclear payload lifetimes, inconsistent pin assignments, and incomplete error handling.

## 11. Reuse recommendations for WS73/HHD-01 work

### Safe to borrow as algorithmic references

- AHT20 busy polling, calibration state check, 20-bit humidity conversion, and 20-bit temperature conversion, after validating I2C transfer semantics.
- SSD1306 control/data bytes, initialization ordering, page addressing, and font rendering.
- The distinction between Wi-Fi association and DHCP readiness.
- Explicit task/queue decomposition for sensor acquisition and network transmission.
- SDK API-dialect comparisons between `IoTGpio*`/`IoTI2c*` and `uapi_*` families.

### Do not copy directly

- GPIO numbers, I2C bus indexes, pin modes, or peripheral assignments.
- GN include paths, component names, or the manual SDK patch procedure without matching the target SDK revision.
- Hardcoded Wi-Fi credentials or infinite retry loops.
- Fixed-size C-string network handling.
- The RGB GPIO-threshold routine as a PWM implementation.
- Any assumption that these examples provide SLE/SSAP/USB behavior.

### Required adaptation boundary

Before reusing a source fragment, pin down the exact WS63/OpenHarmony SDK version, verify the physical pin mapping separately, define payload ownership and completion semantics, use binary-safe lengths, add bounded retries and cleanup, and test each peripheral independently. For WS73 host work, keep these examples as firmware-side reference material only; the Linux USB/DLI/SSAP implementation must come from the WS73 driver and SSAP sources.

## Verified facts versus recommendations

### Verified from source

- The package is external-SDK dependent and not standalone.
- It contains CMSIS-RTOS2, OpenHarmony registration macros, GPIO/I2C/ADC, Wi-Fi, lwIP, and integrated sensor examples.
- AHT20 uses address `0x38`; SSD1306 uses address `0x3C` in the older I2C examples.
- Different examples select different I2C buses and GPIO pins.
- The network examples contain the concrete buffer and control-flow defects listed above.
- No exact SLE/SSAP/HADM/DLI/NearLink/SparkLink/USB protocol implementation is present in the firmware tree.

### Recommendations, not verified behavior

- Treat all pin numbers as non-portable.
- Treat all documented `hb build` procedures as unvalidated until run against a recorded SDK revision.
- Replace the network helpers with length-aware, state-machine-driven code before production use.
- Use the AHT20/SSD1306 algorithmic pieces only after target-SDK and electrical integration checks.
- Do not use this repository as evidence for WS73 USB, DLI, SSAP, or HADM compatibility.
