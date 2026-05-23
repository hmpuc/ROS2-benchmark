#!/usr/bin/env bash
set -euo pipefail

#
# Run from the central machine.
#
# Required:
#   SENSOR_HOST=user@sensor-machine ./scripts/run_two_machine_battery.sh
#
# Assumptions:
#   - Run this script on the central machine host shell.
#   - The ROS2 commands are executed inside the Docker containers.
#   - Both machines are running ROS2 inside Docker containers.
#   - Both containers were started with --net=host.
#   - This workspace is mounted at WORKSPACE_DIR inside both containers.
#   - SSH from the central machine to SENSOR_HOST works without password prompts.
#

CENTRAL_CONTAINER="${CENTRAL_CONTAINER:-ros2_humble}"
SENSOR_CONTAINER="${SENSOR_CONTAINER:-ros2_humble}"
SENSOR_HOST="${SENSOR_HOST:-}"

WORKSPACE_DIR="${WORKSPACE_DIR:-/root/ros2_ws}"
ROS_SETUP="${ROS_SETUP:-/opt/ros/humble/setup.bash}"
ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-42}"

PAYLOAD_SIZE_KB="${PAYLOAD_SIZE_KB:-20}"
PUBLISH_RATE_HZ="${PUBLISH_RATE_HZ:-100}"
BENCHMARK_DURATION_SEC="${BENCHMARK_DURATION_SEC:-180}"
START_SIGNAL_DELAY_SEC="${START_SIGNAL_DELAY_SEC:-3}"

WARMUP_MIN_SENSORS="${WARMUP_MIN_SENSORS:-2}"
WARMUP_MAX_SENSORS="${WARMUP_MAX_SENSORS:-20}"

TEST_MIN_SENSORS="${TEST_MIN_SENSORS:-2}"
TEST_MAX_SENSORS="${TEST_MAX_SENSORS:-100}"
TEST_REPETITIONS="${TEST_REPETITIONS:-10}"

BETWEEN_RUNS_SLEEP_SEC="${BETWEEN_RUNS_SLEEP_SEC:-5}"
STARTUP_SLEEP_SEC="${STARTUP_SLEEP_SEC:-5}"

LOG_DIR="${LOG_DIR:-battery_logs}"

if [[ -z "${SENSOR_HOST}" ]]; then
  echo "ERROR: SENSOR_HOST is required."
  echo "Example: SENSOR_HOST=user@192.168.0.20 $0"
  exit 1
fi

mkdir -p "${LOG_DIR}"

central_exec() {
  docker exec "${CENTRAL_CONTAINER}" bash -lc "$1"
}

clean_benchmarks() {
  echo "[cleanup] deleting benchmarks directory"
  central_exec "cd ${WORKSPACE_DIR} && rm -rf benchmarks && mkdir -p benchmarks"
}

run_benchmark() {
  local sensor_count="$1"
  local benchmark_number="$2"
  local phase="$3"

  local label="${phase}_s${sensor_count}_b${benchmark_number}"
  local central_log="${LOG_DIR}/${label}_central.log"
  local sensor_log="${LOG_DIR}/${label}_sensor.log"

  echo
  echo "[run] phase=${phase} benchmark=${benchmark_number} sensors=${sensor_count} rate=${PUBLISH_RATE_HZ}Hz payload=${PAYLOAD_SIZE_KB}KB duration=${BENCHMARK_DURATION_SEC}s"

  echo "[start] remote sensor container=${SENSOR_CONTAINER}"
  ssh "${SENSOR_HOST}" "docker exec ${SENSOR_CONTAINER} bash -lc '
    cd ${WORKSPACE_DIR} &&
    source ${ROS_SETUP} &&
    source install/setup.bash &&
    export ROS_DOMAIN_ID=${ROS_DOMAIN_ID} &&
    ros2 run cassio_driver sensor_node --ros-args \
      -p sensor_count:=${sensor_count} \
      -p publish_rate_hz:=${PUBLISH_RATE_HZ} \
      -p payload_size_kb:=${PAYLOAD_SIZE_KB} \
      -p hardware_id:=1 \
      -p instance_count:=1 \
      -p benchmark_duration_sec:=${BENCHMARK_DURATION_SEC}
  '" > "${sensor_log}" 2>&1 &

  local sensor_pid="$!"

  sleep "${STARTUP_SLEEP_SEC}"

  echo "[start] local central container=${CENTRAL_CONTAINER}"
  docker exec "${CENTRAL_CONTAINER}" bash -lc "
    cd ${WORKSPACE_DIR} &&
    source ${ROS_SETUP} &&
    source install/setup.bash &&
    export ROS_DOMAIN_ID=${ROS_DOMAIN_ID} &&
    ros2 run node_central node_central --ros-args \
      -p instance_id:=0 \
      -p instance_count:=1 \
      -p benchmark_number:=\"${benchmark_number}\" \
      -p sensor_count:=\"${sensor_count}\" \
      -p benchmark_duration_sec:=${BENCHMARK_DURATION_SEC} \
      -p start_signal_delay_sec:=${START_SIGNAL_DELAY_SEC}
  " > "${central_log}" 2>&1 &

  local central_pid="$!"

  wait "${central_pid}"
  wait "${sensor_pid}"

  echo "[done] ${label}"

  sleep "${BETWEEN_RUNS_SLEEP_SEC}"
}

main() {
  local benchmark_number=0

  echo "[config] central_container=${CENTRAL_CONTAINER}"
  echo "[config] sensor_host=${SENSOR_HOST}"
  echo "[config] sensor_container=${SENSOR_CONTAINER}"
  echo "[config] ros_domain_id=${ROS_DOMAIN_ID}"
  echo "[config] payload=${PAYLOAD_SIZE_KB}KB rate=${PUBLISH_RATE_HZ}Hz duration=${BENCHMARK_DURATION_SEC}s"

  echo
  echo "[warmup] sensors ${WARMUP_MIN_SENSORS}..${WARMUP_MAX_SENSORS}, one run each"

  for sensor_count in $(seq "${WARMUP_MIN_SENSORS}" "${WARMUP_MAX_SENSORS}"); do
    run_benchmark "${sensor_count}" "warmup_${sensor_count}" "warmup"
  done

  clean_benchmarks

  echo
  echo "[battery] sensors ${TEST_MIN_SENSORS}..${TEST_MAX_SENSORS}, ${TEST_REPETITIONS} repetitions each"

  for sensor_count in $(seq "${TEST_MIN_SENSORS}" "${TEST_MAX_SENSORS}"); do
    for repetition in $(seq 1 "${TEST_REPETITIONS}"); do
      run_benchmark "${sensor_count}" "${benchmark_number}" "test_r${repetition}"
      benchmark_number=$((benchmark_number + 1))
    done
  done

  echo
  echo "[all done] benchmark CSV files are in ${WORKSPACE_DIR}/benchmarks inside ${CENTRAL_CONTAINER}"
}

main "$@"
