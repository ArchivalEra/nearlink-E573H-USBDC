---
type: harvest
title: "Lab Note: HiSilicon NearLink Assessment 2025 — Repository Assessment"
language: en
created: 2026-09-05
tags: [harvest, note, hisilicon, nearlink]
sources:
  - "https://github.com/zzhdegit/HiSilicon_Nearlink_assessment_2025"
trust: B
stale_after: 2027-03-05
---

# Lab Note: HiSilicon NearLink Assessment 2025 — Repository Assessment

- Source: `https://github.com/zzhdegit/HiSilicon_Nearlink_assessment_2025/tree/main/` (332 KB working tree, 31 MB with `.git` packfiles)
- Upstream: https://github.com/zzhdegit/HiSilicon_Nearlink_assessment_2025.git (single commit `24a0579`, shallow/blob:none clone)
- Author context: 2025 National Undergraduate Embedded Chip & System Design Competition (China), HiSilicon NearLink capability assessment track, scored 90/100, written live in a closed 120-minute session
- Platform: Hi3863E SoC on HiHope_NearLink_DK3863E_V03 board, SDK v1.10.101+, HiSpark Studio IDE (README.md:13-16)
- Verdict up front: **this tree contains zero SLE/NearLink connectivity code.** Despite the "NearLink" branding, it is a bare-metal peripheral capability test on a NearLink-capable SoC. Details and salvageable material below.

## 1. Headline finding: no SLE/NearLink usage at all

A grep for `sle|ssap|nearlink|sparklink|ranging|seek|connect|adv|scan|discovery`
across all `.c`/`.h`/`.txt` files returns only false positives:
- `ssd1306_fonts.h:21` and `ssd1306_fonts.c:21` — MIT license boilerplate ("IN CONNECTION WITH THE SOFTWARE")
- `ssd1306.c:144` — "Set COM Output Scan Direction" (OLED panel command)

There is no `#include` of any SLE header (`sle_common.h`, `sle_connection_manager.h`,
`sle_device_discovery.h`, `sle_ssap_client.h`, etc.) in any file. The complete
include list of the main file (`Assessment_2025.c:16-37`) is: pinctrl, watchdog,
tcxo, gpio, hal_gpio, soc_osal, app_init, common_def, i2c, osal_debug,
ssd1306_fonts, ssd1306, spi, adc, adc_porting, timer, chip_core_irq, systick,
hcsr04, pwm, ws2812 — all peripheral/OSAL headers, none wireless.

The README confirms this is by design: the assessment questions (README.md:23-40)
are (1) GPIO button control of WS2812 LEDs, (2) full-color rainbow-flow effect,
(3) timer-driven brightness stepping, (4) ADC capture with OLED waveform display.
No question involves radio operation, connection, or SSAP. The "NearLink" in the
title refers only to the chip family (Hi3863E is a NearLink SoC), not the radio.

**Consequence for our driver project:** the three sub-questions resolve as:
1. *SLE/NearLink usage patterns (adv/scan/conn/SSAP/ranging)?* — none present.
2. *APIs we haven't covered?* — yes, but they are **uapi/OSAL peripheral APIs on
   the SDK's firmware side**, not NearLink APIs. Still potentially useful for
   understanding the WS73/Hi3863 SDK surface that the dongle firmware is built on.
3. *Test/benchmark methodology worth adopting?* — a few genuinely useful
   micro-techniques exist (timing calibration, timeout-guarded echo capture,
   SPI-based protocol emulation). See section 4.

## 2. Inventory of uapi/OSAL APIs exercised (with citations)

Main task entry is `tricolored_task` (`Assessment_2025.c:360`), spawned via the
standard SDK idiom `app_run(blinky_entry)` (`Assessment_2025.c:895`) with
`osal_kthread_create` + `osal_kthread_set_priority` (`Assessment_2025.c:887-890`).

### Pin / GPIO
- `uapi_pin_set_mode(pin, mode)` — used for every peripheral mux
  (`Assessment_2025.c:160-161`, `375`, `429`, `437`; `ws2812.c:26-27`; `hcsr04.c:18,22`)
- `uapi_pin_set_pull(pin, PIN_PULL_TYPE_DOWN)` — button inputs
  (`Assessment_2025.c:378`, `383`, `388`, `393`)
- `gpio_select_core(pin, CORES_APPS_CORE)` — explicit core ownership of a GPIO
  (`Assessment_2025.c:376`, `381`, `386`, `391`). This is a call we had not
  catalogued; relevant if we ever document the firmware's GPIO topology.
- `uapi_gpio_set_dir` / `uapi_gpio_get_val` / `uapi_gpio_set_val`
  (`Assessment_2025.c:377`, `458-465`; `hcsr04.c:19-23`, `33-43`)

### Raw register pokes (timing-critical bit-banging)
- `uapi_reg_setbit(0x44028030, n)` / `(0x44028034, n)` — direct GPIO
  set/clear register writes, bypassing the GPIO driver entirely, used for
  sub-microsecond WS2812 waveform generation (`Assessment_2025.c:60-76`, `439`,
  `848`). The comments identify `0x44028030` as the "GPIO set-high register"
  and `0x44028034` as "set-low", referencing `platform_core.h`
  (`Assessment_2025.c:60-62`).
- `uapi_reg_read32(0x44028030, preg5)` used as a **calibrated no-op delay**:
  the author measured "one register read ≈ 50 ns" and loops 7 times to build a
  sub-µs delay because `uapi_tcxo_delay_us` is inaccurate below 1 µs
  (`Assessment_2025.c:71-75`). This is the same SDK register block family our
  WS73 SDK tree exposes; the address comments are a handy cross-check.

### Clocks / delays / watchdog
- `uapi_tcxo_delay_count(ticks)`, `uapi_tcxo_delay_us`, `uapi_tcxo_delay_ms`
  (`Assessment_2025.c:56`, `61-63`, `457`; declared extern at line 56)
- `uapi_systick_get_us()` and `uapi_systick_delay_us()` — microsecond wall
  clock used for echo timing (`hcsr04.c:30`, `34`, `38`, `40-46`)
- `uapi_watchdog_kick()` — peppered through every busy-wait loop, including
  inside button-debounce waits (`Assessment_2025.c:452`, `456`, `475`, `483`,
  `756`). Pattern worth noting: they kick the watchdog inside every blocking
  poll loop rather than restructuring to sleep.

### Timers
- `uapi_timer_init()`, `uapi_timer_adapter(index, irqn, prio)` — adapter call
  binds TIMER1 to `TIMER_1_IRQN` with priority 1 (`Assessment_2025.c:415-416`,
  defines at `110-113`)
- `uapi_timer_create` / `uapi_timer_start(handle, us, cb, 0)` /
  `uapi_timer_stop` / `uapi_timer_delete` — one-shot 1,000,000 µs (1 s) timer
  with self-cleaning callback (`Assessment_2025.c:804-814`, callback at
  `122-131` which stops and deletes its own handle — a create-per-use pattern,
  guarded by `g_timer1 == NULL` before re-create at line 801)

### ADC
- `uapi_adc_init(ADC_CLOCK_NONE)` (`Assessment_2025.c:420`, with a pin-map
  comment: ch0-5 = gpio7..gpio12, line 420)
- `adc_port_read(5, &voltage5)` — the porting-layer read returning millivolts
  (`Assessment_2025.c:467`, `755`). Note they call `adc_port_read` directly
  rather than a `uapi_adc_*` wrapper; the header `adc_porting.h`
  (`Assessment_2025.c:30`) is the include that provides it.

### I2C (OLED)
- `uapi_i2c_master_init(bus, 400000, hscode)` — 400 kHz fast mode
  (`Assessment_2025.c:367`, defines `151-156`)
- `uapi_i2c_master_write(bus, dev_addr, &data)` — single-byte command writes
  inside `ssd1306_WriteCommand` (`ssd1306.c:46`)

### SPI (WS2812 emulation)
- Full `spi_attr_t` / `spi_extra_attr_t` init: master, 32 MHz bus clk,
  3 MHz SCK, CPOL=1/CPHA=1, standard frame format, 8-bit frames
  (`ws2812.c:35-57`)
- `uapi_spi_master_write(bus, &xfer, 0xFFFFFFFF)` with
  `spi_xfer_data_t{tx_buff, tx_bytes, rx_buff=NULL, rx_bytes=0}` — TX-only
  transfer with an effectively infinite timeout (`ws2812.c:96-104`, `125-132`)

### PWM (unused but present)
- `uapi_pwm_init`, `uapi_pwm_open(ch, &cfg)`, `uapi_pwm_set_group(group,
  &channel_id, 1)`, `uapi_pwm_start(group)` — all commented out
  (`Assessment_2025.c:423-436`). The config struct literal at `Assessment_2025.c:423-427`
  documents the field order: {period_ticks, high_ticks, offset, repeat_count,
  loop_bool}, with a note that high time = ticks / 32 MHz.

## 3. What "ranging" means here (and what it doesn't)

The only ranging code is an HC-SR04 **ultrasonic** driver, unrelated to SLE
ranging. It was written for the assessment ("the contest file said it might be
tested, but it wasn't used" — README.md:55) and is commented out in the main
loop (`Assessment_2025.c:421`, `468`).

The technique is still a decent reference for any latency-measurement code:
- 10 µs+ trigger pulse via GPIO (`hcsr04.c:33-35`)
- Echo pulse width measured with `uapi_systick_get_us()` deltas
  (`hcsr04.c:37-46`)
- **Both edge-poll loops have explicit 30 ms timeouts** returning -1
  (`hcsr04.c:37-39`, `43-45`) — never trust an unbounded GPIO wait
- Distance = `(end - start) * 0.0346 / 2.0` cm (`hcsr04.c:49`) — speed of sound
  0.0346 cm/µs, halved for round trip

Contrast with SLE ranging (what our E573H dongle could eventually expose): SLE
ranging is a two-way protocol exchange reported through the SLE stack, not a
GPIO pulse width. Nothing in this repo informs SLE ranging; the only overlap is
the general "timestamp, bound the wait, sanity-check" discipline.

## 4. Methodology / techniques worth adopting (or explicitly rejecting)

### Worth keeping in mind
1. **SPI-timing emulation of a fast single-wire protocol** (`ws2812.c`): encode
   each protocol bit as 4 SPI bits at 3 MHz (1 bit ≈ 333 ns, 4 bits ≈ 1.33 µs
   matching the WS2812 bit period — `ws2812.c:15`), with the nibble table
   `0 -> 0b1000, 1 -> 0b1110` (`ws2812.c:22`). Generalizes to any "bit-bang
   too fast for software" problem: pick a hardware serial engine whose bit time
   divides the target waveform, and encode. If we ever need to drive a
   proprietary 1-wire peripheral from the firmware side, this is the pattern.
2. **Official warning against register-poke timing**: README.md:45 records that
   HiSilicon/HiHope officially state that direct register-address bit-banging
   for WS2812 **locks up / reboots the MCU** ("HiSilicon/HiHope official: calling
   register addresses directly for WS2812 bit operations will cause the MCU to
   hang and restart; use SPI-emulated timing instead"). The code still contains
   the raw-register variant (`Assessment_2025.c:58-105`) — i.e. the surviving
   code is the *known-bad* path kept only as a failed experiment. Lesson: treat
   `uapi_reg_setbit` waveform generation on these SoCs as unreliable.
3. **Calibrated software delay**: `uapi_tcxo_delay_us` is documented (by the
   author, `Assessment_2025.c:72`) as inaccurate below 1 µs; the workaround is
   looping a measured ~50 ns register read (`Assessment_2025.c:71-75`). If we
   ever write firmware-side microbenchmarks on the WS73 SDK, calibrate, don't
   trust the API's lower bound.
4. **Timeout-guarded polling everywhere**: every unbounded wait (button release,
   echo edge, sensor loop) carries a watchdog kick and/or a wall-clock timeout
   (`Assessment_2025.c:474-477`; `hcsr04.c:37-45`). Cheap discipline that
   translates directly to driver code (our USB URB waits, command timeouts).
5. **Competition format as a test-methodology data point** (README.md:6-8,
   59-68): the assessment is a 120-minute closed-room, no-AI, screen-recorded
   peripheral skills test. The author's key advice — pre-build and pre-verify a
   personal driver library, and **physically verify every pin's real mux
   behavior because official docs lie** (README.md:62-68, incl. an observed bug
   where reading several IO levels simultaneously disables one IO) — is a
   useful reminder that HiSilicon's pin-mux documentation for this SDK
   generation cannot be trusted on paper. If our WS73 SDK docs and the
   DK3863E-era docs share lineage, pin-collision surprises on firmware are
   plausible.
6. **SDK-version data point**: this ran on SDK v1.10.101+ (README.md:15); our
   tree is `sdk/ws73_sdk_linux_WS73_1.10.110/`. Same 1.10.x family, so the uapi
   signatures above should be greppable in our own SDK tree for cross-checking.

### Not applicable / rejecting
- The entire application (LED effects, OLED display, button state machines,
  e.g. `Assessment_2025.c:216-350`, `470-781`) is competition scaffolding with
  heavy copy-paste (the brightness-stepping block at `Assessment_2025.c:565-752`
  repeats the same 10-LED frame ten times with `goto s3bg` at line 749). No
  structural patterns worth importing.
- The timer create-per-use pattern (`Assessment_2025.c:801-815`) self-deletes
  inside its own callback (`Assessment_2025.c:128-130`); convenient for a demo
  but a lifecycle anti-pattern for anything long-lived.
- No throughput/latency benchmarking of the radio, no packet counters, no RSSI
  sampling, no measurement harness of any kind exists in the tree.

## 5. Answers to the posed questions, condensed

**Q1 — SLE/NearLink usage patterns (adv/scan/conn/SSAP/ranging)?**
None. Zero wireless API calls in ~950 lines of application code. The code is
pure bare-metal peripheral work (GPIO, SPI, I2C, ADC, timers, PWM, systick,
watchdog) on a NearLink SoC. `Assessment_2025.c:16-37` include list is the
proof; the grep in section 1 found no `sle_*` symbol anywhere. Any expectation
that this repo documents adv/scan/conn/SSAP flows should be discarded; for
those patterns we must rely on the SDK samples under
`sdk/ws73_sdk_linux_WS73_1.10.110/` and prior lab notes, not this source.

**Q2 — APIs we hadn't covered?**
Yes, on the firmware/uapi side (section 2 above): `gpio_select_core`,
`uapi_pin_set_pull`, `uapi_reg_setbit`/`uapi_reg_read32`, `uapi_timer_adapter`
/`create`/`start`/`stop`/`delete`, `adc_port_read` (porting layer),
`uapi_i2c_master_init` with hscode, full `spi_attr_t`/`spi_extra_attr_t`
initialization, `uapi_systick_get_us`, `uapi_tcxo_delay_count`,
`osal_kthread_create`/`set_priority`/`kthread_lock`, `app_run`. None of these
are NearLink APIs; all live in the same SDK generation as our WS73 tree and are
verifiable against our local SDK headers.

**Q3 — Test/benchmark methodology worth adopting?**
Section 4, items 1-5: SPI-encoded protocol emulation, calibrated sub-µs delays,
timeout-guarded polling, the "docs lie about pins, verify on hardware" lesson,
and the echo-measurement timeout pattern. There is no benchmark harness here —
the value is in micro-techniques and in the empirical warnings (register-poke
waveform generation destabilizes the SoC; simultaneous multi-IO reads can
disable an IO), not in any measurement framework.

## 6. Filing notes

- This source is firmware-side application code; it neither touches the USB
  transport (`ffff:3733`) nor any host-driver concern from our repo. Its only
  plausible future use here is as a Rosetta-stone sample of 1.10.x uapi usage
  when reading SDK firmware sources.
- Original README is Chinese-only; quotes above are translated. Per repo rules
  this note is English-only and lives under `.scratch/nearlink-driver/lab-notes/`.

---

## Summary (8 lines)

1. Assessed `https://github.com/zzhdegit/HiSilicon_Nearlink_assessment_2025/tree/main/` (2025 China embedded-competition NearLink capability test, 90/100, Hi3863E, SDK v1.10.101+).
2. **Key finding: zero SLE/NearLink connectivity code** — no adv/scan/conn/SSAP/ranging APIs anywhere; "NearLink" refers only to the SoC family (grep evidence in section 1; includes at `Assessment_2025.c:16-37`).
3. It is a bare-metal peripheral assessment: GPIO+WS2812 LEDs, rainbow-flow, timer brightness stepping, ADC+OLED waveform (README.md:23-40).
4. Catalogued ~25 uncovered firmware-side uapi/OSAL APIs (`gpio_select_core`, `uapi_timer_*`, `adc_port_read`, `uapi_reg_setbit`, full SPI attr init, `osal_kthread_*`, `app_run`) — see section 2 with file:line citations.
5. Only "ranging" is a commented-out HC-SR04 ultrasonic driver with timeout-guarded systick echo timing (`hcsr04.c:27-51`) — unrelated to SLE ranging.
6. Adoptable micro-techniques: SPI-encoded fast single-wire emulation (`ws2812.c:15-22`), calibrated ~50 ns register-read delays, watchdog-kicked polling loops, and "verify every pin on hardware because docs lie" (README.md:62-68).
7. Empirical warning worth recording: direct register-poke waveform generation allegedly hangs/reboots these SoCs per HiSilicon/HiHope (README.md:45).
8. Note filed at `.scratch/nearlink-driver/lab-notes/NEW-HiSilicon-Assessment.md`; no other files touched; SLE stack patterns must come from our local SDK samples instead.
