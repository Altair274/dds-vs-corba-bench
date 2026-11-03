/* Hello-world DDS publisher: publishes the status of a few synthetic radios.
 *
 * QoS used (and why):
 *   RELIABLE        - status updates must not be silently lost
 *   TRANSIENT_LOCAL - a subscriber that starts late still gets the latest
 *                     status of every radio straight away
 *   KEEP_LAST(1)    - only the newest status per radio matters
 *
 * Usage: status_pub [iterations] [num_radios]
 */
#include <stdio.h>
#include <stdlib.h>
#include "dds/dds.h"
#include "RadioStatus.h"

int main(int argc, char **argv)
{
  const int iterations = argc > 1 ? atoi(argv[1]) : 20;
  const int num_radios = argc > 2 ? atoi(argv[2]) : 3;

  dds_entity_t pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (pp < 0) { fprintf(stderr, "participant: %s\n", dds_strretcode(-pp)); return 1; }

  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

  dds_entity_t topic = dds_create_topic(pp, &Demo_RadioStatus_desc, "RadioStatus", qos, NULL);
  dds_entity_t writer = dds_create_writer(pp, topic, qos, NULL);
  dds_delete_qos(qos);
  if (topic < 0 || writer < 0) { fprintf(stderr, "topic/writer creation failed\n"); return 1; }

  printf("Publishing %d radios x %d updates on topic 'RadioStatus'\n", num_radios, iterations);
  for (unsigned seq = 1; seq <= (unsigned)iterations; seq++) {
    for (int r = 0; r < num_radios; r++) {
      Demo_RadioStatus s = {
        .radio_id = r,
        .seq = seq,
        .rssi_dbm = -60.0f - (float)((seq * 7 + (unsigned)r * 13) % 30),
        .mode = (Demo_RadioMode)((seq + (unsigned)r) % 3),
      };
      dds_return_t rc = dds_write(writer, &s);
      if (rc != DDS_RETCODE_OK) fprintf(stderr, "write: %s\n", dds_strretcode(-rc));
    }
    printf("  update %u sent\n", seq);
    fflush(stdout);
    dds_sleepfor(DDS_MSECS(500));
  }

  /* Disposing tells readers each radio is gone (instance state NOT_ALIVE_DISPOSED). */
  for (int r = 0; r < num_radios; r++) {
    Demo_RadioStatus key = { .radio_id = r };
    dds_dispose(writer, &key);
  }
  dds_sleepfor(DDS_MSECS(200));
  dds_delete(pp);
  return 0;
}
