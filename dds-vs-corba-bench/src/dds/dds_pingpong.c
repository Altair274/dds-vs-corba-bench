/* DDS ping-pong latency benchmark (Eclipse Cyclone DDS, C API).
 *
 *   pong:  dds_pingpong -m pong
 *   ping:  dds_pingpong -m ping -s 1024 -n 10000 -w 1000 -l dds-udp
 *
 * The ping side publishes Bench::Payload on "BenchPing", the pong side
 * echoes every sample back on "BenchPong", and ping measures the round
 * trip time. This mirrors corba_client/corba_server, where the round trip
 * is a synchronous roundtrip() call.
 *
 * Output: one CSV line (see common/stats.h).
 */
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/dds.h"
#include "Bench.h"
#include "stats.h"

#define STOP_SEQ UINT64_MAX

typedef struct {
  bool ping;
  size_t size;
  size_t count;
  size_t warmup;
  const char *label;
  const char *raw_path;
} opts_t;

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage: %s -m ping|pong [-s bytes] [-n samples] [-w warmup] [-l label] [-r raw.txt]\n",
          prog);
  exit(2);
}

static opts_t parse(int argc, char **argv)
{
  opts_t o = { .ping = true, .size = 64, .count = 10000, .warmup = 1000, .label = "dds" };
  bool mode_set = false;
  int c;
  while ((c = getopt(argc, argv, "m:s:n:w:l:r:h")) != -1) {
    switch (c) {
      case 'm': o.ping = strcmp(optarg, "ping") == 0; mode_set = true;
                if (!o.ping && strcmp(optarg, "pong") != 0) usage(argv[0]); break;
      case 's': o.size = strtoull(optarg, NULL, 10); break;
      case 'n': o.count = strtoull(optarg, NULL, 10); break;
      case 'w': o.warmup = strtoull(optarg, NULL, 10); break;
      case 'l': o.label = optarg; break;
      case 'r': o.raw_path = optarg; break;
      default: usage(argv[0]);
    }
  }
  if (!mode_set) usage(argv[0]);
  return o;
}

/* Discovery is asynchronous: wait until our writer and reader have matched
 * the peer, so no sample is sent before anybody can receive it. */
static bool wait_for_peer(dds_entity_t writer, dds_entity_t reader, dds_duration_t timeout)
{
  const dds_time_t end = dds_time() + timeout;
  while (dds_time() < end) {
    dds_publication_matched_status_t pm;
    dds_subscription_matched_status_t sm;
    dds_get_publication_matched_status(writer, &pm);
    dds_get_subscription_matched_status(reader, &sm);
    if (pm.current_count > 0 && sm.current_count > 0) return true;
    dds_sleepfor(DDS_MSECS(20));
  }
  return false;
}

int main(int argc, char **argv)
{
  const opts_t o = parse(argc, argv);

  dds_entity_t pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (pp < 0) { fprintf(stderr, "participant: %s\n", dds_strretcode(-pp)); return 1; }

  /* Reliable + keep-all: every ping gets exactly one pong, nothing is dropped. */
  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, 0);

  dds_entity_t t_ping = dds_create_topic(pp, &Bench_Payload_desc, "BenchPing", qos, NULL);
  dds_entity_t t_pong = dds_create_topic(pp, &Bench_Payload_desc, "BenchPong", qos, NULL);
  dds_entity_t writer = dds_create_writer(pp, o.ping ? t_ping : t_pong, qos, NULL);
  dds_entity_t reader = dds_create_reader(pp, o.ping ? t_pong : t_ping, qos, NULL);
  dds_delete_qos(qos);
  if (writer < 0 || reader < 0) { fprintf(stderr, "writer/reader creation failed\n"); return 1; }

  dds_entity_t ws = dds_create_waitset(pp);
  dds_entity_t rc = dds_create_readcondition(reader, DDS_ANY_STATE);
  dds_waitset_attach(ws, rc, reader);

  if (!wait_for_peer(writer, reader, o.ping ? DDS_SECS(10) : DDS_SECS(60))) {
    fprintf(stderr, "%s: no peer discovered\n", o.ping ? "ping" : "pong");
    dds_delete(pp);
    return 1;
  }

  void *samples[1] = { NULL };
  dds_sample_info_t info;
  int ret = 0;

  if (!o.ping) {
    /* ---------------- pong: echo everything until the stop sentinel ---------------- */
    for (;;) {
      if (dds_waitset_wait(ws, NULL, 0, DDS_SECS(30)) <= 0) {
        fprintf(stderr, "pong: timed out waiting for ping\n");
        ret = 1;
        break;
      }
      int n = dds_take(reader, samples, &info, 1, 1);
      if (n <= 0) continue;
      bool stop = false;
      if (info.valid_data) {
        const Bench_Payload *p = samples[0];
        stop = p->seq == STOP_SEQ;
        dds_write(writer, p);
      }
      dds_return_loan(reader, samples, n);
      if (stop) break;
    }
    dds_sleepfor(DDS_MSECS(100)); /* let the last pong reach the ping side */
  } else {
    /* ---------------- ping: measure round trips ---------------- */
    uint8_t *buf = calloc(o.size ? o.size : 1, 1);
    uint64_t *rtt = malloc(o.count * sizeof *rtt);
    Bench_Payload msg = { 0 };
    msg.data._buffer = buf;
    msg.data._length = msg.data._maximum = (uint32_t)o.size;
    msg.data._release = false;

    const size_t total = o.warmup + o.count;
    for (size_t i = 0; i < total; i++) {
      msg.seq = i;
      const uint64_t t0 = bench_now_ns();
      dds_write(writer, &msg);

      bool got = false;
      while (!got) {
        if (dds_waitset_wait(ws, NULL, 0, DDS_SECS(5)) <= 0) {
          fprintf(stderr, "ping: timeout waiting for pong %zu\n", i);
          ret = 1;
          goto done;
        }
        int n = dds_take(reader, samples, &info, 1, 1);
        if (n > 0) {
          got = info.valid_data && ((const Bench_Payload *)samples[0])->seq == i;
          dds_return_loan(reader, samples, n);
        }
      }
      const uint64_t t1 = bench_now_ns();
      if (i >= o.warmup) rtt[i - o.warmup] = t1 - t0;
    }
    bench_report(o.label, o.size, rtt, o.count, o.raw_path);

  done:
    msg.seq = STOP_SEQ;
    msg.data._length = 0;
    dds_write(writer, &msg);
    dds_sleepfor(DDS_MSECS(100));
    free(rtt);
    free(buf);
  }

  dds_delete(pp);
  return ret;
}
