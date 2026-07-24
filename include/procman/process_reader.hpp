#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace procman {

// ── ProcessReader: reads /proc/[pid]/* and system info ──
class ProcessReader {
public:
  ProcessReader() = default;
  ~ProcessReader() = default;

  ProcessReader(const ProcessReader&) = delete;
  ProcessReader& operator=(const ProcessReader&) = delete;
  ProcessReader(ProcessReader&&) noexcept = default;
  ProcessReader& operator=(ProcessReader&&) noexcept = default;

  /// Read all processes from /proc
  Result<std::vector<ProcessInfo>> read_all_processes();

  /// Read system info from /proc/meminfo and /proc/stat
  Result<SystemInfo> read_system_info();

  /// Read detailed info for a single PID
  Result<ProcessInfo> read_process(pid_t pid);

  /// Read CPU ticks for each core + total (for delta calculations)
  Result<std::vector<uint64_t>> read_cpu_ticks();

  /// Update CPU% for processes using previous tick snapshot
  void update_cpu_pct(std::vector<ProcessInfo>& procs);

private:
  static Result<std::string> read_proc_file(const std::string& path);
  static ProcessInfo parse_stat(pid_t pid, const std::string& content);
  static ProcessInfo parse_status(pid_t pid, const std::string& content);
  static std::string read_cmdline(pid_t pid);
  static int count_pids_in_proc();

  // Previous CPU ticks for delta calculation
  std::vector<uint64_t> prev_cpu_ticks_;
  bool has_prev_{false};
};

} // namespace procman
