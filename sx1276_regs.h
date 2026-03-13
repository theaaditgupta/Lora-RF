/**
 * sx1276_regs.h
 * SX1276 register map — LoRa mode registers only.
 *
 * Source: Semtech SX1276/77/78/79 Datasheet Rev 7, Section 6.
 *
 * These are the actual register addresses used in the project.
 * Driving them directly (no library) forces you to understand what
 * each field does before you can use it.
 */

#ifndef SX1276_REGS_H
#define SX1276_REGS_H

#include <stdint.h>

/* ── Common registers ───────────────────────────────────────── */
#define REG_FIFO                0x00U
#define REG_OP_MODE             0x01U
#define REG_FR_MSB              0x06U
#define REG_FR_MID              0x07U
#define REG_FR_LSB              0x08U
#define REG_PA_CONFIG           0x09U
#define REG_PA_RAMP             0x0AU
#define REG_OCP                 0x0BU
#define REG_LNA                 0x0CU

/* ── LoRa registers ─────────────────────────────────────────── */
#define REG_FIFO_ADDR_PTR       0x0DU
#define REG_FIFO_TX_BASE_ADDR   0x0EU
#define REG_FIFO_RX_BASE_ADDR   0x0FU
#define REG_FIFO_RX_CURRENT     0x10U
#define REG_IRQ_FLAGS_MASK      0x11U
#define REG_IRQ_FLAGS           0x12U
#define REG_RX_NB_BYTES         0x13U
#define REG_PKT_SNR_VALUE       0x19U
#define REG_PKT_RSSI_VALUE      0x1AU
#define REG_RSSI_VALUE          0x1BU
#define REG_MODEM_CONFIG1       0x1DU
#define REG_MODEM_CONFIG2       0x1EU
#define REG_SYMB_TIMEOUT_LSB    0x1FU
#define REG_PREAMBLE_MSB        0x20U
#define REG_PREAMBLE_LSB        0x21U
#define REG_PAYLOAD_LENGTH      0x22U
#define REG_MODEM_CONFIG3       0x26U
#define REG_DETECTION_OPTIMIZE  0x31U
#define REG_DETECTION_THRESHOLD 0x37U
#define REG_SYNC_WORD           0x39U
#define REG_DIO_MAPPING1        0x40U
#define REG_VERSION             0x42U   /* Expected: 0x12 on SX1276 */
#define REG_PA_DAC              0x4DU

/* ── REG_OP_MODE values ─────────────────────────────────────── */
#define MODE_LONG_RANGE         0x80U   /* Bit 7: LoRa mode */
#define MODE_SLEEP              0x00U
#define MODE_STDBY              0x01U
#define MODE_TX                 0x03U
#define MODE_RX_CONTINUOUS      0x05U
#define MODE_RX_SINGLE          0x06U

/* ── REG_IRQ_FLAGS bitmasks ─────────────────────────────────── */
#define IRQ_TX_DONE             0x08U
#define IRQ_PAYLOAD_CRC_ERROR   0x20U
#define IRQ_RX_DONE             0x40U

/* ── REG_PA_CONFIG ──────────────────────────────────────────── */
#define PA_BOOST                0x80U   /* Use PA_BOOST pin (+2 to +17 dBm) */

/* ── Spreading factors (stored in REG_MODEM_CONFIG2 bits 7:4) ─ */
#define SF7                     7U
#define SF8                     8U
#define SF9                     9U
#define SF10                    10U
#define SF11                    11U
#define SF12                    12U

/* ── Bandwidth values (REG_MODEM_CONFIG1 bits 7:4) ─────────── */
#define BW_7_8KHZ               0x00U
#define BW_10_4KHZ              0x10U
#define BW_15_6KHZ              0x20U
#define BW_20_8KHZ              0x30U
#define BW_31_25KHZ             0x40U
#define BW_41_7KHZ              0x50U
#define BW_62_5KHZ              0x60U
#define BW_125KHZ               0x70U
#define BW_250KHZ               0x80U
#define BW_500KHZ               0x90U

/* ── Coding rates (REG_MODEM_CONFIG1 bits 3:1) ──────────────── */
#define CR_4_5                  0x02U
#define CR_4_6                  0x04U
#define CR_4_7                  0x06U
#define CR_4_8                  0x08U

#endif /* SX1276_REGS_H */
