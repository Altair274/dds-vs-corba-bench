/* Latency statistics shared by the DDS (C) and CORBA (C++) benchmarks. */
#ifndef BENCH_STATS_H
#define BENCH_STATS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static inline uint64_t bench_now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline int bench_cmp_u64(const void *a, const void *b)
{
  const uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}

/* Nearest-rank percentile on a sorted array. */
static inline uint64_t bench_pct(const uint64_t *sorted, size_t n, double p)
{
  size_t idx = (size_t)(p / 100.0 * (double)n);
  if (idx >= n) idx = n - 1;
  return sorted[idx];
}

/* Optionally dumps raw round-trip times (ns) to raw_path, then prints one
 * CSV line to stdout (all values in microseconds):
 *   label,size_bytes,samples,min,p50,p90,p99,p99.9,max,mean            */
static inline void bench_report(const char *label, size_t size, uint64_t *rtt,
                                size_t n, const char *raw_path)
{
  if (n == 0) return;
  if (raw_path) {
    FILE *f = fopen(raw_path, "w");
    if (f) {
      for (size_t i = 0; i < n; i++) fprintf(f, "%llu\n", (unsigned long long)rtt[i]);
      fclose(f);
    }
  }
  qsort(rtt, n, sizeof *rtt, bench_cmp_u64);
  double sum = 0.0;
  for (size_t i = 0; i < n; i++) sum += (double)rtt[i];
  printf("%s,%zu,%zu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", label, size, n,
         rtt[0] / 1e3, bench_pct(rtt, n, 50) / 1e3, bench_pct(rtt, n, 90) / 1e3,
         bench_pct(rtt, n, 99) / 1e3, bench_pct(rtt, n, 99.9) / 1e3,
         rtt[n - 1] / 1e3, sum / (double)n / 1e3);
  fflush(stdout);
}

#endif
