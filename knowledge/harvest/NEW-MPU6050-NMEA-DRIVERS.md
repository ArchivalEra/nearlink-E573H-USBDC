---
type: harvest
title: "nearlink-contrib MPU6050 driver + sle_mesh_new NMEA parser — WS63 I2C pin map, GPS coordinate e6 pipeline"
language: en
created: 2026-09-13
tags: [mpu6050, i2c, ws63, nmea, gps, location, driver, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# nearlink-contrib MPU6050 driver + sle_mesh_new NMEA parser — WS63 I2C pin map, GPS coordinate e6 pipeline

- Inspection date: 2026-09-13 (both current)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **nearlink-contrib MPU6050 driver** documents the WS63 I2C pin map: I2C0 = GPIO17(SCL) + GPIO18(SDA), I2C1 = GPIO16(SCL) + GPIO15(SDA), both MODE 2, with dedicated bus/pin macros — a clean pin-assignment reference for any WS63 I2C peripheral integration. [`mpu6050.h:34-39`]
2. The Step1/Step2 integration-guide convention (driver-lib guide + test-case guide as separate markdown docs per component) provides a template for third-party component documentation on WS63.
3. **sle_mesh_new NMEA parser (313 lines)** handles GPRMC/GNRMC (position/speed) and GPGGA/GNGGA (fix quality) sentences, parsing latitude/longitude into **e6 fixed-point integers** (`latitude_e6 = nmea_parse_coord_e6(field[3], field[4])`) — the standard embedded GPS coordinate representation that avoids float on MCU. [`sle_team_nmea.c:213-275`]
4. `sle_team_location.c` (81 lines) is the bridge from NMEA-parsed state to the mesh packet's position fields — a clean sensor-to-protocol pipeline layer.

## Boundaries and gaps

- The mpu6050 driver's init/read sequence was not line-read; focus was on the I2C pin-map contribution.
- NMEA parser handles only RMC/GGA; other sentences (GSV, GSA) not parsed.

## Reusable for our stack

- The WS63 I2C pin map (I2C0/I2C1 GPIO assignments, MODE 2) is the reference for any I2C peripheral we attach to the dongle.
- The e6 coordinate + NMEA→mesh-packet pipeline is directly portable for GPS-enabled NearLink deployments.

## Comparison anchors (vs existing reports)

- `NEW-SLE-TEAM-MESH-V456.md`: the location/NMEA layer feeding the mesh packet position fields.
- `NEW-TEKI128-MINIMAL-PAIR.md`: same WS63 platform, peripheral integration layer.
