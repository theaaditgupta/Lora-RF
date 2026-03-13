/**
 * sx1276.c
 * SX1276 register-level driver — simulation build.
 *
 * In the simulation the chip's internal register file is a uint8_t array.
 * sx1276_write_reg / sx1276_read_reg act on that array directly instead of
 * going through SPI.  Everything else (config logic, airtime calc, state
 * machine) is identical to what runs on real hardware.
 */

#include "sx1276.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Internal helpers ───────────────────────────────────────── */

static void set_frequency(SX1276 *dev, uint32_t freq_hz)
{
    /* Frf = freq_hz / (32e6 / 2^19) = freq_hz * 2^19 / 32e6 */
    uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000UL;
    sx1276_write_reg(dev, REG_FR_MSB, (uint8_t)((frf >> 16) & 0xFFU));
    sx1276_write_reg(dev, REG_FR_MID, (uint8_t)((frf >>  8) & 0xFFU));
    sx1276_write_reg(dev, REG_FR_LSB, (uint8_t)( frf        & 0xFFU));
}

static void set_tx_power(SX1276 *dev, uint8_t dbm)
{
    /* PA_BOOST path: Pout = 2 + OutputPower (dBm), range 2..17 dBm */
    if (dbm > 17U) dbm = 17U;
    if (dbm <  2U) dbm =  2U;
    sx1276_write_reg(dev, REG_PA_CONFIG,
                     (uint8_t)(PA_BOOST | ((dbm - 2U) & 0x0FU)));
}

static void apply_config(SX1276 *dev, const SX1276Config *cfg)
{
    /* Enter sleep to change modem config */
    sx1276_set_mode(dev, MODE_SLEEP | MODE_LONG_RANGE);
    sx1276_set_mode(dev, MODE_STDBY | MODE_LONG_RANGE);

    set_frequency(dev, LORA_FREQ_HZ);
    set_tx_power(dev, cfg->tx_power_dbm);

    /* REG_MODEM_CONFIG1: bandwidth | coding_rate | implicit_header=0 */
    sx1276_write_reg(dev, REG_MODEM_CONFIG1,
                     (uint8_t)(cfg->bandwidth | cfg->coding_rate | 0x00U));

    /* REG_MODEM_CONFIG2: SF | TX_CONTINUOUS=0 | RX_PAYLOAD_CRC | symb_timeout_msb=0 */
    sx1276_write_reg(dev, REG_MODEM_CONFIG2,
                     (uint8_t)((cfg->spreading_factor << 4)
                               | (cfg->crc_enabled ? 0x04U : 0x00U)));

    /* REG_MODEM_CONFIG3: low-data-rate-optimize for SF11/SF12 at 125kHz */
    uint8_t ldr = 0x04U;  /* AGC auto on */
    if (cfg->spreading_factor >= 11U && cfg->bandwidth == BW_125KHZ) {
        ldr |= 0x08U;     /* LowDataRateOptimize */
    }
    sx1276_write_reg(dev, REG_MODEM_CONFIG3, ldr);

    /* Preamble length: 8 symbols (standard) */
    sx1276_write_reg(dev, REG_PREAMBLE_MSB, 0x00U);
    sx1276_write_reg(dev, REG_PREAMBLE_LSB, 0x08U);

    /* Sync word */
    sx1276_write_reg(dev, REG_SYNC_WORD, cfg->sync_word);

    /* Detection thresholds for SF6 vs SF7-12 */
    if (cfg->spreading_factor == 6U) {
        sx1276_write_reg(dev, REG_DETECTION_OPTIMIZE,  0xC5U);
        sx1276_write_reg(dev, REG_DETECTION_THRESHOLD, 0x0CU);
    } else {
        sx1276_write_reg(dev, REG_DETECTION_OPTIMIZE,  0xC3U);
        sx1276_write_reg(dev, REG_DETECTION_THRESHOLD, 0x0AU);
    }

    /* FIFO base addresses */
    sx1276_write_reg(dev, REG_FIFO_TX_BASE_ADDR, 0x00U);
    sx1276_write_reg(dev, REG_FIFO_RX_BASE_ADDR, 0x00U);

    /* LNA: maximum gain, boost on */
    sx1276_write_reg(dev, REG_LNA, 0x23U);

    memcpy(&dev->cfg, cfg, sizeof(SX1276Config));
}

/* ── Public API ─────────────────────────────────────────────── */

uint8_t sx1276_read_reg(SX1276 *dev, uint8_t addr)
{
    if (addr >= 0x70U) return 0x00U;
    return dev->regs[addr];
}

void sx1276_write_reg(SX1276 *dev, uint8_t addr, uint8_t value)
{
    if (addr >= 0x70U) return;
    dev->regs[addr] = value;
}

void sx1276_set_mode(SX1276 *dev, uint8_t mode)
{
    sx1276_write_reg(dev, REG_OP_MODE, mode);
}

bool sx1276_init(SX1276 *dev, const SX1276Config *cfg)
{
    memset(dev, 0, sizeof(SX1276));

    /* Simulate chip version register */
    dev->regs[REG_VERSION] = SX1276_VERSION_ID;

    uint8_t version = sx1276_read_reg(dev, REG_VERSION);
    if (version != SX1276_VERSION_ID) {
        printf("[SX1276] ERROR: unexpected version 0x%02X (expected 0x%02X)\n",
               version, SX1276_VERSION_ID);
        return false;
    }

    apply_config(dev, cfg);
    dev->initialised = true;

    printf("[SX1276] Initialised: SF%u  BW%s  CR4/%u  %u dBm\n",
           cfg->spreading_factor,
           (cfg->bandwidth == BW_125KHZ) ? "125k" :
           (cfg->bandwidth == BW_250KHZ) ? "250k" : "500k",
           (cfg->coding_rate == CR_4_5) ? 5U :
           (cfg->coding_rate == CR_4_6) ? 6U :
           (cfg->coding_rate == CR_4_7) ? 7U : 8U,
           cfg->tx_power_dbm);
    return true;
}

bool sx1276_send(SX1276 *dev, const uint8_t *buf, uint8_t len)
{
    /* len is uint8_t and SX1276_MAX_PAYLOAD is 255, so len > 255 is always
     * false — guard only against zero length here. Callers must ensure
     * payload fits within SX1276_MAX_PAYLOAD before calling. */
    if (!dev->initialised || len == 0U) {
        return false;
    }

    /* Set FIFO pointer to TX base */
    sx1276_write_reg(dev, REG_FIFO_ADDR_PTR,
                     sx1276_read_reg(dev, REG_FIFO_TX_BASE_ADDR));

    /* Write payload to FIFO */
    for (uint8_t i = 0U; i < len; i++) {
        sx1276_write_reg(dev, REG_FIFO, buf[i]);
    }
    sx1276_write_reg(dev, REG_PAYLOAD_LENGTH, len);

    /* Trigger TX */
    sx1276_set_mode(dev, MODE_TX | MODE_LONG_RANGE);

    /* In real firmware we wait for DIO0 (TX_DONE) interrupt here.
     * Simulation: set the flag immediately. */
    sx1276_write_reg(dev, REG_IRQ_FLAGS, IRQ_TX_DONE);

    /* Clear IRQ flags */
    sx1276_write_reg(dev, REG_IRQ_FLAGS, 0xFFU);

    sx1276_set_mode(dev, MODE_STDBY | MODE_LONG_RANGE);

    dev->packets_sent++;
    return true;
}

bool sx1276_receive(SX1276 *dev, uint8_t *buf, uint8_t *len,
                    uint32_t timeout_ms)
{
    if (!dev->initialised) return false;
    (void)timeout_ms;   /* Simulation ignores timeout */

    /* In simulation a packet is always "waiting" after sx1276_send() from
     * the other node writes into our simulated FIFO.  On real hardware we
     * would poll or wait for DIO0 (RX_DONE) interrupt. */

    uint8_t irq = sx1276_read_reg(dev, REG_IRQ_FLAGS);

    if (irq & IRQ_PAYLOAD_CRC_ERROR) {
        dev->crc_errors++;
        sx1276_write_reg(dev, REG_IRQ_FLAGS, 0xFFU);
        return false;
    }

    if (!(irq & IRQ_RX_DONE)) {
        return false;   /* No packet yet */
    }

    *len = sx1276_read_reg(dev, REG_RX_NB_BYTES);
    sx1276_write_reg(dev, REG_FIFO_ADDR_PTR,
                     sx1276_read_reg(dev, REG_FIFO_RX_CURRENT));

    for (uint8_t i = 0U; i < *len; i++) {
        buf[i] = sx1276_read_reg(dev, REG_FIFO);
    }

    /* Simulate RSSI and SNR */
    dev->last_packet_rssi = -85;   /* Typical value at ~800m with SF12 */
    dev->last_packet_snr  = 7;

    sx1276_write_reg(dev, REG_IRQ_FLAGS, 0xFFU);
    dev->packets_received++;
    return true;
}

uint32_t sx1276_airtime_ms(const SX1276Config *cfg, uint8_t payload_bytes)
{
    /*
     * Airtime formula from Semtech AN1200.13:
     *
     *   T_sym   = 2^SF / BW
     *   N_preamble = 8 (symbols) + 4.25
     *   H = 0 (explicit header)
     *   DE = LowDataRateOptimize (1 if SF>=11 and BW=125kHz)
     *   N_payload = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*H) /
     *                            (4*(SF - 2*DE))) * CR, 0)
     *   T_packet = (N_preamble + N_payload) * T_sym
     */
    double bw_hz;
    switch (cfg->bandwidth) {
        case BW_125KHZ: bw_hz = 125000.0; break;
        case BW_250KHZ: bw_hz = 250000.0; break;
        case BW_500KHZ: bw_hz = 500000.0; break;
        default:        bw_hz = 125000.0; break;
    }

    uint8_t sf  = cfg->spreading_factor;
    uint8_t cr_num = 5U;
    switch (cfg->coding_rate) {
        case CR_4_5: cr_num = 5U; break;
        case CR_4_6: cr_num = 6U; break;
        case CR_4_7: cr_num = 7U; break;
        case CR_4_8: cr_num = 8U; break;
    }

    int de = (sf >= 11U && cfg->bandwidth == BW_125KHZ) ? 1 : 0;
    int crc = cfg->crc_enabled ? 1 : 0;

    double t_sym_s = pow(2.0, sf) / bw_hz;

    double n_payload_raw =
        8.0 + fmax(
            ceil((8.0 * payload_bytes - 4.0 * sf + 28.0 + 16.0 * crc)
                 / (4.0 * (sf - 2.0 * de))) * cr_num,
            0.0);

    double n_preamble = 8.0 + 4.25;
    double t_packet_s = (n_preamble + n_payload_raw) * t_sym_s;

    return (uint32_t)(t_packet_s * 1000.0);
}

void sx1276_print_config(const SX1276 *dev)
{
    const SX1276Config *c = &dev->cfg;
    printf("  Spreading factor  : SF%u\n",  c->spreading_factor);
    printf("  Bandwidth         : %s\n",
           c->bandwidth == BW_125KHZ ? "125 kHz" :
           c->bandwidth == BW_250KHZ ? "250 kHz" : "500 kHz");
    printf("  Coding rate       : 4/%u\n",
           c->coding_rate == CR_4_5 ? 5U :
           c->coding_rate == CR_4_6 ? 6U :
           c->coding_rate == CR_4_7 ? 7U : 8U);
    printf("  TX power          : %u dBm\n", c->tx_power_dbm);
    printf("  CRC               : %s\n", c->crc_enabled ? "enabled" : "disabled");
    printf("  Sync word         : 0x%02X\n", c->sync_word);
    printf("  Packets sent      : %u\n",     dev->packets_sent);
    printf("  Packets received  : %u\n",     dev->packets_received);
    printf("  CRC errors        : %u\n",     dev->crc_errors);
    printf("  Last RSSI         : %d dBm\n", dev->last_packet_rssi);
    printf("  Last SNR          : %d dB\n",  dev->last_packet_snr);
}
