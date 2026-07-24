#pragma once

#include <expected>
#include <string>
#include <cstdint>

namespace procman {

// ── Error type ──
struct Error {
  int code{0};
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

// ── Process info (from /proc/[pid]/status + stat + cmdline) ──
struct ProcessInfo {
  pid_t pid{0};
  std::string name;           // comm (truncated to 15 chars from /proc/[pid]/comm)
  std::string cmdline;        // full command line
  std::string state;          // R, S, D, Z, T, etc.
  char state_char{'?'};

  // From /proc/[pid]/stat
  uint64_t utime{0};          // user mode ticks
  uint64_t stime{0};          // kernel mode ticks
  int64_t nice{0};
  long num_threads{0};

  // From /proc/[pid]/status
  uint64_t vm_rss_kb{0};      // RSS in kB
  uint64_t vm_size_kb{0};     // total VM size in kB
  uint64_t vm_peak_kb{0};     // peak VM size

  // Calculated
  double cpu_pct{0.0};        // CPU usage % (needs two samples)
  double mem_pct{0.0};        // memory usage % (RSS / total RAM)

  // Derived
  uint64_t total_ticks() const noexcept { return utime + stime; }
};

// ── System info ──
struct SystemInfo {
  uint64_t total_ram_kb{0};
  uint64_t available_ram_kb{0};
  unsigned num_cpus{0};
  double uptime_seconds{0.0};
};

} // namespace procman
