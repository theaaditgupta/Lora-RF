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

Measured in suburban Vancouver, line-of-sight, RFM95W modules at +14 dBm.

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

SF12 costs more battery but provides ~4x more range and ~10 dB more noise immunity. For a pole-mounted transformer sensor in a rural area, the range benefit outweighs the power cost.

---

## Getting started

```bash
git clone https://github.com/aadit-gupta/lora-rf-link
cd lora-rf-link

# Build and run simulation
make run

# Run link budget analysis
make budget
```

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

## On real hardware

The simulation build replaces SPI with a local register array. To run on real hardware:

1. Implement `spi_transfer(uint8_t *tx, uint8_t *rx, uint8_t len)` for your MCU
2. Replace `sx1276_read_reg` / `sx1276_write_reg` with SPI transactions:
   - Write: pull NSS low, send `addr | 0x80`, send value, pull NSS high
   - Read: pull NSS low, send `addr & 0x7F`, read byte, pull NSS high
3. Wire DIO0 to a GPIO interrupt for TX_DONE and RX_DONE events
4. Replace the blocking IRQ poll in `sx1276_send` / `sx1276_receive` with interrupt-driven callbacks
