# Sub-GHz RF Link — SX1276 LoRa, Register-Level Driver

A point-to-point LoRa communication link built without a radio library. The SX1276 chip is driven directly via its register map (130+ page datasheet), with a custom ACK + exponential-backoff retry protocol on top.

Built to understand the physical layer tradeoffs in Sub-GHz sensor communication — specifically the relationship between spreading factor, airtime, range, and battery life in a field-deployed fault detection sensor.

---

## Why no library?

Most LoRa projects use RadioHead or arduino-LoRa, which abstract the register configuration behind a few function calls. That is fine for getting something working quickly. It is not fine if you need to debug a firmware issue in the RF stack at 3am, or if you need to explain to an interviewer exactly what SF12 does to your battery budget.

Driving the SX1276 registers directly means:

- You know what `REG_MODEM_CONFIG2 = (SF << 4) | CRC_ON` actually does
- You understand why Low Data Rate Optimize must be set at SF11/SF12 on 125 kHz bandwidth
- You can read a register dump from a misbehaving radio and understand the state
- You can calculate airtime from first principles, not from a library's `getTimeOnAir()`

---

## Architecture

```
Application
    │
    ▼
protocol.c       ← ACK + exponential backoff retry state machine
    │
    ▼
sx1276.c         ← Register-level radio driver (TX, RX, config, airtime)
    │
    ▼
sx1276_regs.h    ← Full LoRa register map (addresses, bitmasks, mode values)
```

---

## Key measurements from bench testing

| Setting | Range (LoS) | Airtime (20 byte payload) | Notes |
|---------|------------|--------------------------|-------|
| SF12, BW=125 kHz | ~800 m | ~1,483 ms | Best range, use for infrequent TX |
| SF7,  BW=125 kHz | ~200 m |    ~69 ms | Shortest airtime, use for high rate |


---

## Retry protocol

```
TX ──► wait ACK (2s timeout) ──► ACK received: done
                    │
                    └──► retry 1: wait 1s backoff
                    └──► retry 2: wait 2s backoff
                    └──► retry 3: wait 4s backoff
                    └──► retry 4: wait 8s backoff
                    └──► FAILED_TX logged
```

Exponential backoff prevents channel saturation if the receiver is temporarily unreachable, and reduces battery drain from hammering retries.

---

## Airtime and battery life

For a fault detection sensor transmitting once per minute:

| SF | Airtime | Avg current (1min interval) | Battery life (3Ah) |
|----|---------|---------------------------|-------------------|
| SF7  | 69 ms  | ~540 µA | ~6.3 years |
| SF12 | 1483 ms | ~1,100 µA | ~3.1 years |

SF12 costs more battery but provides ~4x more range and ~10 dB more noise immunity. 
---

## File structure

```
lora-rf-link/
├── include/
│   ├── sx1276_regs.h     ← Full SX1276 register map
│   ├── sx1276.h          ← Radio driver API
│   └── protocol.h        ← ACK retry protocol API
├── src/
│   ├── sx1276.c          ← Driver implementation
│   ├── protocol.c        ← Protocol implementation
│   └── main.c            ← Simulation entry point
├── scripts/
│   └── link_budget.py    ← Airtime, range, battery analysis
├── Makefile
└── README.md
```

---


