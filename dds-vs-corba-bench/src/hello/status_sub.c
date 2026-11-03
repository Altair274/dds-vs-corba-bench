/* Hello-world DDS subscriber for the RadioStatus topic.
 *
 * Try starting it *after* status_pub: thanks to TRANSIENT_LOCAL durability
 * it immediately receives the latest status of every radio.
 *
 * Usage: status_sub [seconds]   (default: run until Ctrl+C)
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "dds/dds.h"
#include "RadioStatus.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static const char *mode_str(Demo_RadioMode m)
{
  switch (m) {
    case Demo_MODE_IDLE: return "IDLE";
    case Demo_MODE_RX:   return "RX";
    case Demo_MODE_TX:   return "TX";
  }
  return "?";
}

int main(int argc, char **argv)
{
  const int seconds = argc > 1 ? atoi(argv[1]) : 0;
  signal(SIGINT, on_sigint);

  dds_entity_t pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (pp < 0) { fprintf(stderr, "participant: %s\n", dds_strretcode(-pp)); return 1; }

  /* Must be compatible with the writer's QoS, otherwise they never match. */
  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

  dds_entity_t topic = dds_create_topic(pp, &Demo_RadioStatus_desc, "RadioStatus", qos, NULL);
  dds_entity_t reader = dds_create_reader(pp, topic, qos, NULL);
  dds_delete_qos(qos);
  if (topic < 0 || reader < 0) { fprintf(stderr, "topic/reader creation failed\n"); return 1; }

  dds_entity_t ws = dds_create_waitset(pp);
  dds_entity_t rc = dds_create_readcondition(reader, DDS_ANY_STATE);
  dds_waitset_attach(ws, rc, reader);

  printf("Waiting for RadioStatus samples (Ctrl+C to quit)...\n");
  const dds_time_t deadline = seconds > 0 ? dds_time() + DDS_SECS(seconds) : DDS_NEVER;

  while (!g_stop && dds_time() < deadline) {
    if (dds_waitset_wait(ws, NULL, 0, DDS_SECS(1)) <= 0) continue;

    void *samples[16] = { NULL };
    dds_sample_info_t infos[16];
    int n = dds_take(reader, samples, infos, 16, 16);   /* loaned samples */
    for (int i = 0; i < n; i++) {
      const Demo_RadioStatus *s = samples[i];
      if (infos[i].valid_data) {
        printf("radio %d  seq %3u  rssi %6.1f dBm  mode %s\n",
               s->radio_id, s->seq, s->rssi_dbm, mode_str(s->mode));
      } else if (infos[i].instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE) {
        printf("radio %d  disposed by its writer\n", s->radio_id);
      } else if (infos[i].instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) {
        printf("radio %d  has no writers left\n", s->radio_id);
      }
    }
    if (n > 0) dds_return_loan(reader, samples, n);
    fflush(stdout);
  }

  dds_delete(pp);
  return 0;
}
