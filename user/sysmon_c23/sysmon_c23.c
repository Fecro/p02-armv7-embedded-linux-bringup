/**
 * @file sysmon_c23.c
 * @brief Embedded telemetry monitor implementation in ISO C23 for ARM Versatile
 * Express (vexpress-a9).
 *
 * Compliant with:
 *  - ISO/IEC 9899:2024 (C23): [[nodiscard]], nullptr, constexpr, <stdckdint.h>.
 *  - Effective C (2nd Ed., Robert C. Seacord): checked integer arithmetic
 * against overflow, bounded buffer reading, deterministic file descriptor
 * release.
 *  - Learn C the Hard Way (Zed A. Shaw): defensive dbg.h macros with
 * single-exit cleanup jump.
 *  - MISRA C:2025: zero dynamic memory allocation in sampling loop (100% stack
 * allocated), explicit pointer validation, and robust numeric conversions via
 * strtoul.
 */
#define _POSIX_C_SOURCE 200809L

#include "dbg.h"
#include "sysmon.h"

#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Signal-safe termination flag */
static volatile sig_atomic_t g_keep_running = 1;

static void handle_signal(int sig) {
  (void)sig;
  g_keep_running = 0;
}

/**
 * @brief Safely sums all CPU tick components using checked arithmetic.
 */
SYSMON_NODISCARD static sysmon_status_t
sum_cpu_ticks_safe(const cpu_ticks_t *ticks, uint64_t *out_total,
                   uint64_t *out_idle_all) {
  check_msg(ticks != nullptr && out_total != nullptr && out_idle_all != nullptr,
            "Null pointer passed to sum_cpu_ticks_safe");

  uint64_t total = 0U;
  uint64_t idle_all = 0U;

  if (ckd_add(&idle_all, ticks->idle, ticks->iowait)) {
    sentinel_msg("Integer overflow calculating idle + iowait ticks");
  }

  const uint64_t fields[8] = {ticks->user,    ticks->nice,   ticks->system,
                              ticks->idle,    ticks->iowait, ticks->irq,
                              ticks->softirq, ticks->steal};

  for (size_t i = 0U; i < 8U; ++i) {
    if (ckd_add(&total, total, fields[i])) {
      sentinel_msg("Integer overflow accumulating total CPU tick count");
    }
  }

  *out_total = total;
  *out_idle_all = idle_all;
  return SYSMON_OK;

error:
  return SYSMON_ERR_OVERFLOW;
}

SYSMON_NODISCARD sysmon_status_t
sysmon_read_cpu_ticks(const char *proc_stat_path, cpu_ticks_t *out_ticks) {
  FILE *fp = nullptr;
  sysmon_status_t status = SYSMON_ERR_IO;

  if (proc_stat_path == nullptr || out_ticks == nullptr) {
    return SYSMON_ERR_NULL_PTR;
  }

  fp = fopen(proc_stat_path, "r");
  check_fmt(fp != nullptr, "Failed to open %s", proc_stat_path);

  char line[SYSMON_MAX_LINE_LEN] = {0};
  check_fmt(fgets(line, (int)sizeof(line), fp) != nullptr,
            "Empty or failed read on %s", proc_stat_path);

  cpu_ticks_t parsed = {0};
  int matched =
      sscanf(line,
             "cpu  %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
             " %" SCNu64 " %" SCNu64 " %" SCNu64,
             &parsed.user, &parsed.nice, &parsed.system, &parsed.idle,
             &parsed.iowait, &parsed.irq, &parsed.softirq, &parsed.steal);
  if (matched < 4) {
    status = SYSMON_ERR_PARSE;
    sentinel_fmt("Invalid format on 'cpu' line in %s (matched=%d)",
                 proc_stat_path, matched);
  }

  *out_ticks = parsed;
  fclose(fp);
  return SYSMON_OK;

error:
  if (fp != nullptr) {
    fclose(fp);
  }
  return status;
}

SYSMON_NODISCARD sysmon_status_t sysmon_calc_cpu_usage_permille(
    const cpu_ticks_t *prev, const cpu_ticks_t *curr, uint32_t *out_permille) {
  if (prev == nullptr || curr == nullptr || out_permille == nullptr) {
    return SYSMON_ERR_NULL_PTR;
  }

  uint64_t prev_total = 0U, prev_idle = 0U;
  uint64_t curr_total = 0U, curr_idle = 0U;

  sysmon_status_t st = sum_cpu_ticks_safe(prev, &prev_total, &prev_idle);
  if (st != SYSMON_OK) {
    return st;
  }
  st = sum_cpu_ticks_safe(curr, &curr_total, &curr_idle);
  if (st != SYSMON_OK) {
    return st;
  }

  uint64_t total_delta = 0U;
  uint64_t idle_delta = 0U;

  if (ckd_sub(&total_delta, curr_total, prev_total) ||
      ckd_sub(&idle_delta, curr_idle, prev_idle)) {
    return SYSMON_ERR_OVERFLOW;
  }

  if (total_delta == 0U || idle_delta >= total_delta) {
    *out_permille = 0U;
    return SYSMON_OK;
  }

  uint64_t active_delta = 0U;
  uint64_t scaled_active = 0U;
  if (ckd_sub(&active_delta, total_delta, idle_delta) ||
      ckd_mul(&scaled_active, active_delta, 1000ULL)) {
    return SYSMON_ERR_OVERFLOW;
  }

  *out_permille = (uint32_t)(scaled_active / total_delta);
  return SYSMON_OK;
}

SYSMON_NODISCARD sysmon_status_t
sysmon_read_meminfo(const char *proc_meminfo_path, mem_stats_t *out_mem) {
  FILE *fp = nullptr;
  sysmon_status_t status = SYSMON_ERR_IO;

  if (proc_meminfo_path == nullptr || out_mem == nullptr) {
    return SYSMON_ERR_NULL_PTR;
  }

  fp = fopen(proc_meminfo_path, "r");
  check_fmt(fp != nullptr, "Failed to open %s", proc_meminfo_path);

  mem_stats_t stats = {0};
  bool has_total = false;
  bool has_avail = false;
  char line[SYSMON_MAX_LINE_LEN] = {0};

  while (fgets(line, (int)sizeof(line), fp) != nullptr) {
    uint64_t val = 0U;
    if (sscanf(line, "MemTotal: %" SCNu64 " kB", &val) == 1) {
      stats.mem_total_kb = val;
      has_total = true;
    } else if (sscanf(line, "MemFree: %" SCNu64 " kB", &val) == 1) {
      stats.mem_free_kb = val;
    } else if (sscanf(line, "MemAvailable: %" SCNu64 " kB", &val) == 1) {
      stats.mem_available_kb = val;
      has_avail = true;
    } else if (sscanf(line, "Buffers: %" SCNu64 " kB", &val) == 1) {
      stats.buffers_kb = val;
    } else if (sscanf(line, "Cached: %" SCNu64 " kB", &val) == 1) {
      stats.cached_kb = val;
    }
  }

  if (!has_total || stats.mem_total_kb == 0U) {
    status = SYSMON_ERR_PARSE;
    sentinel_fmt("MemTotal missing or zero in %s", proc_meminfo_path);
  }

  /* Fallback estimation for legacy Linux kernels without MemAvailable */
  if (!has_avail) {
    stats.mem_available_kb =
        stats.mem_free_kb + stats.buffers_kb + stats.cached_kb;
  }

  if (stats.mem_available_kb <= stats.mem_total_kb) {
    stats.mem_used_kb = stats.mem_total_kb - stats.mem_available_kb;
  } else {
    stats.mem_used_kb = 0U;
  }

  uint64_t scaled_used = 0U;
  if (ckd_mul(&scaled_used, stats.mem_used_kb, 1000ULL)) {
    status = SYSMON_ERR_OVERFLOW;
    sentinel_msg("Integer overflow calculating memory utilization permille");
  }
  stats.mem_usage_permille = (uint32_t)(scaled_used / stats.mem_total_kb);

  *out_mem = stats;
  fclose(fp);
  return SYSMON_OK;

error:
  if (fp != nullptr) {
    fclose(fp);
  }
  return status;
}

SYSMON_NODISCARD sysmon_status_t sysmon_read_overview(
    const char *proc_uptime_path, const char *proc_loadavg_path,
    const char *dt_model_path, sys_overview_t *out_overview) {
  FILE *fp = nullptr;

  if (proc_uptime_path == nullptr || proc_loadavg_path == nullptr ||
      out_overview == nullptr) {
    return SYSMON_ERR_NULL_PTR;
  }

  sys_overview_t ov = {0};
  snprintf(ov.board_model, sizeof(ov.board_model),
           "Generic Embedded Linux Target");

  fp = fopen(proc_uptime_path, "r");
  check_fmt(fp != nullptr, "Failed to open %s", proc_uptime_path);
  check_fmt(fscanf(fp, "%lf %lf", &ov.uptime_sec, &ov.idle_sec) == 2,
            "Failed parsing %s", proc_uptime_path);
  fclose(fp);
  fp = nullptr;

  fp = fopen(proc_loadavg_path, "r");
  check_fmt(fp != nullptr, "Failed to open %s", proc_loadavg_path);
  double l1 = 0.0, l5 = 0.0, l15 = 0.0;
  check_fmt(fscanf(fp, "%lf %lf %lf", &l1, &l5, &l15) == 3, "Failed parsing %s",
            proc_loadavg_path);
  fclose(fp);
  fp = nullptr;

  ov.load_1m_permille = (l1 >= 0.0) ? (uint32_t)(l1 * 1000.0) : 0U;
  ov.load_5m_permille = (l5 >= 0.0) ? (uint32_t)(l5 * 1000.0) : 0U;
  ov.load_15m_permille = (l15 >= 0.0) ? (uint32_t)(l15 * 1000.0) : 0U;

  /* Parse board model from device tree if exposed by kernel */
  if (dt_model_path != nullptr) {
    FILE *mfp = fopen(dt_model_path, "r");
    if (mfp != nullptr) {
      size_t n = fread(ov.board_model, 1U, sizeof(ov.board_model) - 1U, mfp);
      ov.board_model[n] = '\0';
      for (size_t i = 0U; i < n; ++i) {
        if (ov.board_model[i] == '\n' || ov.board_model[i] == '\r') {
          ov.board_model[i] = '\0';
          break;
        }
      }
      fclose(mfp);
    }
  }

  *out_overview = ov;
  return SYSMON_OK;

error:
  if (fp != nullptr) {
    fclose(fp);
  }
  return SYSMON_ERR_IO;
}

static void sleep_ms(uint32_t ms) {
  struct timespec ts = {.tv_sec = (time_t)(ms / 1000U),
                        .tv_nsec = (long)((ms % 1000U) * 1000000UL)};
  nanosleep(&ts, nullptr);
}

int main(int argc, char *argv[]) {
  uint32_t iterations = 5U;
  uint32_t interval_ms = 1000U;

  if (argc >= 2) {
    char *endptr = nullptr;
    unsigned long val = strtoul(argv[1], &endptr, 10);
    if (endptr != argv[1] && *endptr == '\0' && val > 0UL && val <= 10000UL) {
      iterations = (uint32_t)val;
    }
  }
  if (argc >= 3) {
    char *endptr = nullptr;
    unsigned long val = strtoul(argv[2], &endptr, 10);
    if (endptr != argv[2] && *endptr == '\0' && val >= 50UL && val <= 60000UL) {
      interval_ms = (uint32_t)val;
    }
  }

  struct sigaction sa = {0};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  (void)sigaction(SIGINT, &sa, nullptr);
  (void)sigaction(SIGTERM, &sa, nullptr);

  cpu_ticks_t prev_cpu = {0};
  check_msg(sysmon_read_cpu_ticks("/proc/stat", &prev_cpu) == SYSMON_OK,
            "Failed initial baseline read of /proc/stat");

  log_info_fmt("Starting sysmon_c23 (%u samples, interval %u ms)...",
               iterations, interval_ms);

  for (uint32_t sample = 1U; sample <= iterations && g_keep_running; ++sample) {
    sleep_ms(interval_ms);

    cpu_ticks_t curr_cpu = {0};
    mem_stats_t mem = {0};
    sys_overview_t ov = {0};
    uint32_t cpu_permille = 0U;

    check_msg(sysmon_read_cpu_ticks("/proc/stat", &curr_cpu) == SYSMON_OK,
              "Failed to read /proc/stat");
    check_msg(sysmon_calc_cpu_usage_permille(&prev_cpu, &curr_cpu,
                                             &cpu_permille) == SYSMON_OK,
              "Failed to compute CPU utilization");
    check_msg(sysmon_read_meminfo("/proc/meminfo", &mem) == SYSMON_OK,
              "Failed to read /proc/meminfo");
    check_msg(sysmon_read_overview("/proc/uptime", "/proc/loadavg",
                                   "/proc/device-tree/model", &ov) == SYSMON_OK,
              "Failed to read system overview metrics");

    prev_cpu = curr_cpu;

    printf("[#%02u] Board: \"%s\" | Uptime: %.1fs | "
           "CPU: %u.%01u%% | RAM: %" PRIu64 "/%" PRIu64 " kB (%u.%01u%%) | "
           "Load(1m): %u.%03u\n",
           sample, ov.board_model, ov.uptime_sec, cpu_permille / 10U,
           cpu_permille % 10U, mem.mem_used_kb, mem.mem_total_kb,
           mem.mem_usage_permille / 10U, mem.mem_usage_permille % 10U,
           ov.load_1m_permille / 1000U, ov.load_1m_permille % 1000U);
    (void)fflush(stdout);
  }

  return EXIT_SUCCESS;

error:
  return EXIT_FAILURE;
}
