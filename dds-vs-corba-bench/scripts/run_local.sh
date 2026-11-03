#!/usr/bin/env bash
# Runs the single-host latency matrix:
#   corba-tcp   omniORB, GIOP over TCP loopback
#   corba-unix  omniORB, GIOP over a Unix domain socket
#   dds-udp     Cyclone DDS, RTPS over UDP loopback
# for a range of payload sizes, and writes a CSV summary.
#
# usage: scripts/run_local.sh [build_dir] [out_dir]
#   env: SIZES="64 1024 65536"  COUNT=10000  WARMUP=1000
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
OUT="${2:-$ROOT/results/latest}"
SIZES="${SIZES:-64 256 1024 4096 16384 65536 262144 1048576}"
COUNT="${COUNT:-10000}"
WARMUP="${WARMUP:-1000}"

mkdir -p "$OUT"
SUMMARY="$OUT/summary.csv"
echo "transport,size_bytes,samples,min_us,p50_us,p90_us,p99_us,p999_us,max_us,mean_us" > "$SUMMARY"

export CYCLONEDDS_URI="${CYCLONEDDS_URI:-file://$ROOT/config/cyclonedds-local.xml}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"; jobs -p | xargs -r kill 2>/dev/null || true' EXIT

wait_for_file() {
  for _ in $(seq 100); do [[ -s "$1" ]] && return 0; sleep 0.05; done
  echo "timed out waiting for $1" >&2; return 1
}

run_corba() {  # $1 label, $2 endpoint, $3 size
  local ior="$TMP/echo.ior"
  rm -f "$ior" "$TMP/echo.sock"
  "$BUILD/corba_server" -i "$ior" -ORBendPoint "$2" &
  local srv=$!
  wait_for_file "$ior"
  "$BUILD/corba_client" -i "$ior" -s "$3" -n "$COUNT" -w "$WARMUP" -l "$1" -q >> "$SUMMARY"
  wait "$srv"
}

run_dds() {  # $1 label, $2 size
  "$BUILD/dds_pingpong" -m pong &
  local pong=$!
  "$BUILD/dds_pingpong" -m ping -s "$2" -n "$COUNT" -w "$WARMUP" -l "$1" >> "$SUMMARY"
  wait "$pong"
}

for size in $SIZES; do
  echo ">> payload $size bytes"
  run_corba corba-tcp  "giop:tcp:127.0.0.1:"        "$size"
  run_corba corba-unix "giop:unix:$TMP/echo.sock"   "$size"
  run_dds   dds-udp                                 "$size"
done

echo
if command -v column >/dev/null; then column -s, -t < "$SUMMARY"; else cat "$SUMMARY"; fi
echo
echo "Summary written to $SUMMARY"
