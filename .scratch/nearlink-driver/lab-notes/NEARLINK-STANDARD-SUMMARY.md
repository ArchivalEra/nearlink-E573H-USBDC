# NearLink SLE Standard — Public Research Summary

Researched 2026-08-16. Confirmed from official ISLA/SparkLink sources (isla.org.cn, sparklink.org.cn, admin.isla.org API, white papers, TR PDFs).

## (a) SLE key parameters

SLE spec: T/XS 10002-2025 V2.1.1 "Synchronous Low-Power Air Interface SLE" (NearLink 2.0 series, 2025-03-26).

| Parameter | Value |
|---|---|
| Band | 2.4 GHz ISM, 79 frequency points |
| Bandwidth | 1/2/4 MHz |
| Waveform | Single-carrier |
| Modulation | GFSK, BPSK, QPSK, 8PSK |
| Coding | Polar (FEC) |
| Peak PHY rate | 12 Mbps (8PSK @4MHz), ~6x BLE |
| Latency | 250 µs round-trip; 49 µs super-short frame @4MHz |
| RX sensitivity | -110 dBm (min SINR -3 dB) |
| Concurrent users | 256 |
| Power | < 2 mA |
| Topology | Unicast + multicast; C-frame central scheduling |
| Positioning | Phase-diff + TOF, 79 freq points |
| Security | PHY random perturbation; SM4 data-link; ZUC/AES app |

## (b) SLB vs SLE

| | SLB | SLE |
|---|---|---|
| Spec | T/XS 10001-2025 V2.0.0 | T/XS 10002-2025 V2.1.1 |
| Waveform | OFDM/OFDMA, ultra-short frames | Single-carrier |
| Bandwidth | 20-320 MHz | 1/2/4 MHz |
| Modulation | QPSK-1024QAM | GFSK-8PSK |
| Peak rate | 920 Mbps (20MHz 8x8 MIMO) | 12 Mbps |
| Latency | < 20 µs one-way | 250 µs round-trip |
| Users | 4096 | 256 |

Shared upper architecture: Basic Service Layer + Basic Application Layer.

## (c) Spec documents
- 63-standards catalog public metadata (admin.isla.org/api/standard); **PDF texts member-only** (403 without login)
- Public free: TR/XS 00001-2021 air-interface performance report (id=464), SparkLink 1.0 white paper (id=615), security white paper (id=638), NearLink 2.0 white papers (id=852/853/875)

## (d) Confirmed protocol details
- SLB physical frame 20.833 µs; G-link (G→T) / T-link (T→G); G-node scheduler
- HARQ (PHY, CC+IR) + link ARQ; audio codec T/XS 30006-2023
- NearLink 2.0: 21 new standards (Mar 2024), SLE renamed "Synchronous Low-Power Air Interface" V2.1.1; 3.0 pre-standardization started 2025-05-14
- Modes: SLE(E), SLB(B), SLP(precision positioning ~0.6m indoor), SLZ(RFID-like)

## (e) Member-only
Full T/XS texts, identifier allocation, SLB 2.0 internals, detailed PDU formats.

## Implication for WS73
WS73 SLE = 12 Mbps / 250 µs / 256 users — matches our hardware findings. SLB (920 Mbps) is a different chip line; confirmed why WS73 has none.
