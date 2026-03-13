/**
 * main.c — LoRa RF Link simulation
 *
 * Simulates a transmitter node sending 20 sensor packets to a receiver node
 * with ACK-based reliable delivery and exponential backoff retry.
 *
 * Also runs the airtime comparison across SF7 to SF12 that was measured
 * during the project.
 *
 * Build:  make run
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "sx1276.h"
#include "protocol.h"

#define NUM_PACKETS   20U
#define PAYLOAD_LEN   20U   /* Typical fault-status payload size */

int main(void)
{
    printf("LoRa RF Link — Simulation\n");
    printf("Register-level SX1276 driver, ACK + exponential backoff\n\n");

    /* ── Airtime comparison across spreading factors ── */
    printf("=== Airtime vs Spreading Factor (BW=125kHz, CR=4/5, %u byte payload) ===\n",
           PAYLOAD_LEN);

    uint8_t sfs[] = {7, 8, 9, 10, 11, 12};
    for (uint8_t i = 0; i < 6; i++) {
        SX1276Config cfg = {
            .spreading_factor = sfs[i],
            .bandwidth        = BW_125KHZ,
            .coding_rate      = CR_4_5,
            .tx_power_dbm     = 14U,
            .sync_word        = 0x12U,
            .crc_enabled      = true,
        };
        uint32_t at = sx1276_airtime_ms(&cfg, PAYLOAD_LEN);
        printf("  SF%-2u  airtime = %4u ms\n", sfs[i], at);
    }
    printf("\n");

    /* ── Initialise radio with SF12 ── */
    SX1276Config cfg = {
        .spreading_factor = SF12,
        .bandwidth        = BW_125KHZ,
        .coding_rate      = CR_4_5,
        .tx_power_dbm     = 14U,
        .sync_word        = 0x12U,
        .crc_enabled      = true,
    };

    SX1276    radio;
    ProtoStats stats = {0};

    if (!sx1276_init(&radio, &cfg)) {
        printf("ERROR: sx1276_init failed\n");
        return 1;
    }

    printf("\n=== Transmitting %u packets (SF12, +14 dBm) ===\n\n", NUM_PACKETS);

    for (uint8_t seq = 0U; seq < NUM_PACKETS; seq++) {
        /* Build a simulated sensor payload */
        uint8_t payload[PAYLOAD_LEN];
        memset(payload, 0, sizeof(payload));
        payload[0] = 0xF1U;                  /* Message type: fault report   */
        payload[1] = seq;                    /* Sequence number               */
        payload[2] = (uint8_t)(20 + seq % 5); /* Simulated temperature        */
        payload[3] = (uint8_t)(seq % 2);     /* Fault flag: 0=ok, 1=fault     */

        bool ok = proto_send_reliable(&radio, payload, PAYLOAD_LEN, seq, &stats);
        printf("  pkt[%2u] seq=%-3u  %s\n", seq, seq, ok ? "ACK received" : "FAILED");
    }

    proto_stats_print(&stats);

    printf("=== Radio Config ===\n");
    sx1276_print_config(&radio);

    return 0;
}
