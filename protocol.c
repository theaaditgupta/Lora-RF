/**
 * protocol.c
 */

#include "protocol.h"
#include <string.h>
#include <stdio.h>

/* Simulated wall-clock milliseconds — on hardware use SysTick or TIM */
static uint32_t sim_time_ms = 0U;

static void sim_delay_ms(uint32_t ms)
{
    sim_time_ms += ms;
}

static uint32_t sim_get_tick_ms(void)
{
    return sim_time_ms;
}

/* ── Packet encode / decode ────────────────────────────────── */

static uint8_t encode_packet(const Packet *pkt, uint8_t *out_buf)
{
    out_buf[0] = (uint8_t)pkt->type;
    out_buf[1] = pkt->seq;
    out_buf[2] = pkt->payload_len;
    memcpy(&out_buf[3], pkt->payload, pkt->payload_len);
    return (uint8_t)(PROTO_HEADER_LEN + pkt->payload_len);
}

static bool decode_packet(const uint8_t *buf, uint8_t len, Packet *out)
{
    if (len < PROTO_HEADER_LEN) return false;
    out->type        = (PacketType)buf[0];
    out->seq         = buf[1];
    out->payload_len = buf[2];
    if ((uint8_t)(PROTO_HEADER_LEN + out->payload_len) > len) return false;
    memcpy(out->payload, &buf[3], out->payload_len);
    return true;
}

/* ── Simulated channel: the receiver's radio gets a copy of what was sent ── */
/* In real hardware TX and RX are separate physical nodes.
 * Here we share a single loopback FIFO between the two SX1276 instances. */

static uint8_t channel_buf[SX1276_MAX_PAYLOAD];
static uint8_t channel_len = 0U;
static bool    channel_has_data = false;

void proto_sim_channel_write(const uint8_t *buf, uint8_t len)
{
    memcpy(channel_buf, buf, len);
    channel_len = len;
    channel_has_data = true;
}

static bool proto_sim_channel_read(uint8_t *buf, uint8_t *len)
{
    if (!channel_has_data) return false;
    memcpy(buf, channel_buf, channel_len);
    *len = channel_len;
    channel_has_data = false;
    return true;
}

/* ── Simulated ACK channel (receiver -> transmitter) ── */
static uint8_t ack_buf[PROTO_HEADER_LEN];
static bool    ack_pending = false;

static void sim_send_ack(uint8_t seq)
{
    ack_buf[0] = (uint8_t)PKT_ACK;
    ack_buf[1] = seq;
    ack_buf[2] = 0U;
    ack_pending = true;
}

static bool sim_poll_ack(uint8_t expected_seq)
{
    if (!ack_pending) return false;
    bool match = (ack_buf[1] == expected_seq);
    ack_pending = false;
    return match;
}

/* ── Public API ─────────────────────────────────────────────── */

bool proto_send_reliable(SX1276 *radio, const uint8_t *payload,
                         uint8_t len, uint8_t seq, ProtoStats *stats)
{
    if (len > PROTO_MAX_PAYLOAD) return false;

    Packet pkt = {0};
    pkt.type        = PKT_DATA;
    pkt.seq         = seq;
    pkt.payload_len = len;
    memcpy(pkt.payload, payload, len);

    uint8_t encoded[SX1276_MAX_PAYLOAD];
    uint8_t encoded_len = encode_packet(&pkt, encoded);

    uint32_t backoff_ms = BACKOFF_BASE_MS;
    stats->sent++;

    for (uint8_t attempt = 0U; attempt <= MAX_RETRIES; attempt++) {
        if (attempt > 0U) {
            printf("  [retry %u]  backoff %u ms  seq=%u\n",
                   attempt, backoff_ms, seq);
            sim_delay_ms(backoff_ms);
            backoff_ms = (backoff_ms * 2U < BACKOFF_MAX_MS)
                         ? backoff_ms * 2U : BACKOFF_MAX_MS;
            stats->retries_total++;
        }

        /* Transmit */
        sx1276_send(radio, encoded, encoded_len);
        proto_sim_channel_write(encoded, encoded_len);

        /* Simulate receiver processing: immediately ACKs in loopback */
        sim_send_ack(seq);

        /* Wait for ACK */
        uint32_t deadline = sim_get_tick_ms() + ACK_TIMEOUT_MS;
        while (sim_get_tick_ms() < deadline) {
            if (sim_poll_ack(seq)) {
                stats->acked++;
                return true;
            }
            sim_delay_ms(10U);
        }
    }

    stats->failed++;
    printf("  [FAILED] seq=%u after %u retries\n", seq, MAX_RETRIES);
    return false;
}

bool proto_receive(SX1276 *radio, Packet *out, uint32_t timeout_ms)
{
    (void)radio;
    (void)timeout_ms;

    uint8_t buf[SX1276_MAX_PAYLOAD];
    uint8_t len = 0U;

    if (!proto_sim_channel_read(buf, &len)) return false;
    if (!decode_packet(buf, len, out))      return false;
    if (out->type != PKT_DATA)              return false;

    /* Auto-ACK */
    sim_send_ack(out->seq);
    return true;
}

void proto_stats_print(const ProtoStats *stats)
{
    printf("\n=== Protocol Stats ===\n");
    printf("  Packets sent    : %u\n",  stats->sent);
    printf("  ACKed           : %u\n",  stats->acked);
    printf("  Failed          : %u\n",  stats->failed);
    printf("  Total retries   : %u\n",  stats->retries_total);
    if (stats->sent > 0U) {
        printf("  Delivery rate   : %.1f %%\n",
               100.0 * (double)stats->acked / (double)stats->sent);
    }
    printf("======================\n\n");
}
