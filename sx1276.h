/**
 * sx1276.h
 * SX1276 LoRa radio driver — register-level, no library.
 *
 * On real hardware all register access goes through SPI.
 * In the simulation build, a software register file stands in for the chip.
 *
 * Design decisions:
 *   - No dynamic memory allocation
 *   - All configuration passed at init time
 *   - Caller is responsible for chip select (NSS) toggling
 *   - TX/RX are blocking in simulation; on hardware they would be IRQ-driven
 */

#ifndef SX1276_H
#define SX1276_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sx1276_regs.h"

#define SX1276_MAX_PAYLOAD  255U
#define SX1276_VERSION_ID   0x12U   /* Expected REG_VERSION value */

/* Channel frequency for 915 MHz ISM band (North America) */
#define LORA_FREQ_HZ        915000000UL

typedef struct {
    uint8_t  spreading_factor;   /* SF7 – SF12           */
    uint8_t  bandwidth;          /* BW_125KHZ etc.       */
    uint8_t  coding_rate;        /* CR_4_5 etc.          */
    uint8_t  tx_power_dbm;       /* 2 – 17 dBm           */
    uint8_t  sync_word;          /* 0x12 public, 0x34 private */
    bool     crc_enabled;
} SX1276Config;

typedef struct {
    /* Simulated register file — on hardware this lives inside the chip */
    uint8_t  regs[0x70];
    bool     initialised;
    SX1276Config cfg;

    /* Stats tracked for link budget analysis */
    int8_t   last_packet_rssi;
    int8_t   last_packet_snr;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t crc_errors;
} SX1276;

/**
 * sx1276_init()
 * Verify chip version, apply config, set STDBY mode.
 * Returns true on success.
 */
bool sx1276_init(SX1276 *dev, const SX1276Config *cfg);

/**
 * sx1276_send()
 * Transmit len bytes from buf.  Blocks until TX_DONE IRQ fires (simulated).
 * Returns true on success.
 */
bool sx1276_send(SX1276 *dev, const uint8_t *buf, uint8_t len);

/**
 * sx1276_receive()
 * Block in RX_SINGLE mode until a packet arrives or timeout_ms elapses.
 * Writes received bytes into buf, sets *len to byte count.
 * Returns true if a valid packet was received.
 */
bool sx1276_receive(SX1276 *dev, uint8_t *buf, uint8_t *len,
                    uint32_t timeout_ms);

/**
 * sx1276_read_reg() / sx1276_write_reg()
 * Direct register access.  On real hardware these wrap SPI transactions.
 */
uint8_t sx1276_read_reg(SX1276 *dev, uint8_t addr);
void    sx1276_write_reg(SX1276 *dev, uint8_t addr, uint8_t value);

/**
 * sx1276_set_mode()
 * Set operating mode (MODE_SLEEP, MODE_STDBY, MODE_TX, etc.)
 */
void    sx1276_set_mode(SX1276 *dev, uint8_t mode);

/**
 * sx1276_airtime_ms()
 * Calculate on-air time in milliseconds for a payload of payload_bytes.
 * Formula from Semtech AN1200.13.
 */
uint32_t sx1276_airtime_ms(const SX1276Config *cfg, uint8_t payload_bytes);

/**
 * sx1276_print_config()
 * Print current register config to stdout.
 */
void sx1276_print_config(const SX1276 *dev);

#endif /* SX1276_H */
