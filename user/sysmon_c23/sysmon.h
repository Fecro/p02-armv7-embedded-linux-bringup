/**
 * @file sysmon.h
 * @brief Data structures, compile-time constants, and API contracts
 *        for the embedded telemetry monitor in ISO C23.
 */
#ifndef SYSMON_H
#define SYSMON_H

#include "c23_compat.h"

/* Compile-time static bounds */
enum { SYSMON_MAX_LINE_LEN = 256U, SYSMON_MAX_MODEL_LEN = 64U };

/* Module error and status codes */
typedef enum {
  SYSMON_OK = 0,
  SYSMON_ERR_NULL_PTR = -1,
  SYSMON_ERR_IO = -2,
  SYSMON_ERR_PARSE = -3,
  SYSMON_ERR_OVERFLOW = -4
} sysmon_status_t;

/* Raw CPU time counters parsed from /proc/stat */
typedef struct {
  uint64_t user;
  uint64_t nice;
  uint64_t system;
  uint64_t idle;
  uint64_t iowait;
  uint64_t irq;
  uint64_t softirq;
  uint64_t steal;
} cpu_ticks_t;

/* System memory metrics parsed from /proc/meminfo */
typedef struct {
  uint64_t mem_total_kb;
  uint64_t mem_free_kb;
  uint64_t mem_available_kb;
  uint64_t buffers_kb;
  uint64_t cached_kb;
  uint64_t mem_used_kb;
  uint32_t mem_usage_permille; /* Memory utilization in parts per thousand
                                  (e.g., 425 = 42.5%) */
} mem_stats_t;

/* General host metrics from /proc/uptime, /proc/loadavg, and device tree */
typedef struct {
  double uptime_sec;
  double idle_sec;
  uint32_t load_1m_permille;
  uint32_t load_5m_permille;
  uint32_t load_15m_permille;
  char board_model[SYSMON_MAX_MODEL_LEN];
} sys_overview_t;

/* Telemetry query and metric calculation contracts */
SYSMON_NODISCARD sysmon_status_t
sysmon_read_cpu_ticks(const char *proc_stat_path, cpu_ticks_t *out_ticks);

SYSMON_NODISCARD sysmon_status_t sysmon_calc_cpu_usage_permille(
    const cpu_ticks_t *prev, const cpu_ticks_t *curr, uint32_t *out_permille);

SYSMON_NODISCARD sysmon_status_t
sysmon_read_meminfo(const char *proc_meminfo_path, mem_stats_t *out_mem);

SYSMON_NODISCARD sysmon_status_t sysmon_read_overview(
    const char *proc_uptime_path, const char *proc_loadavg_path,
    const char *dt_model_path, sys_overview_t *out_overview);

#endif /* SYSMON_H */
