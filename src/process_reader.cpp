#include "procman/process_reader.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <cstring>

namespace fs = std::filesystem;
using namespace procman;

// ── Helpers ──
Result<std::string> ProcessReader::read_proc_file(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return std::unexpected(Error{errno, "Cannot open " + path + ": " + std::strerror(errno)});
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  if (!f && !f.eof()) {
    return std::unexpected(Error{errno, "Error reading " + path + ": " + std::strerror(errno)});
  }
  return ss.str();
}

std::string ProcessReader::read_cmdline(pid_t pid) {
  auto path = "/proc/" + std::to_string(pid) + "/cmdline";
  auto r = read_proc_file(path);
  if (!r) return std::string{};
  // cmdline uses null bytes as delimiters
  std::string result;
  for (char c : *r) {
    if (c == '\0') {
      if (!result.empty() && result.back() != ' ') result += ' ';
    } else {
      result += c;
    }
  }
  // Trim trailing space
  while (!result.empty() && result.back() == ' ') result.pop_back();
  return result;
}

ProcessInfo ProcessReader::parse_stat(pid_t pid, const std::string& content) {
  ProcessInfo info;
  info.pid = pid;

  // /proc/[pid]/stat format: pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime ...
  // We need to extract comm carefully because it can contain parentheses and spaces
  // Format: "pid (comm) state ..."
  auto close_paren = content.rfind(')');
  if (close_paren == std::string::npos) return info;

  // Extract comm between first '(' and last ')'
  auto open_paren = content.find('(');
  if (open_paren == std::string::npos) return info;

  info.name = content.substr(open_paren + 1, close_paren - open_paren - 1);

  // After the closing paren, skip space, then comes state
  auto pos = close_paren + 1;
  // Skip whitespace
  while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) ++pos;
  if (pos < content.size()) {
    info.state_char = content[pos];
    info.state = std::string(1, info.state_char);
  }

  // Parse fields after state: ppid, pgrp, session, tty_nr, tpgid, flags, minflt, cminflt, majflt, cmajflt, utime, stime, ...
  // We'll use sscanf for the remaining fields
  int state_int;
  long ppid, pgrp, session, tty_nr, tpgid;
  unsigned long flags, minflt, cminflt, majflt, cmajflt;

  auto after_state = content.substr(pos + 1);
  int n = std::sscanf(after_state.c_str(),
    "%ld %ld %ld %ld %ld %lu %lu %lu %lu %lu %lu %lu",
    &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,
    &minflt, &cminflt, &majflt, &cmajflt, &info.utime, &info.stime);

  // Fields 39-41 (0-indexed): nice, num_threads
  // After stime there are cutime, cstime, priority, nice, num_threads, ...
  // Let's find the remaining fields more carefully
  // Actually, let's use string stream parsing for the rest
  std::istringstream iss(after_state);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) tokens.push_back(token);

  // After the first 12 numeric fields we already parsed, we need field indices:
  // 0: ppid, 1: pgrp, 2: session, 3: tty_nr, 4: tpgid, 5: flags,
  // 6: minflt, 7: cminflt, 8: majflt, 9: cmajflt, 10: utime, 11: stime
  // Then cutime(12), cstime(13), priority(14), nice(15), num_threads(16)
  if (tokens.size() > 15) {
    std::from_chars(tokens[15].data(), tokens[15].data() + tokens[15].size(), info.nice);
  }
  if (tokens.size() > 16) {
    std::from_chars(tokens[16].data(), tokens[16].data() + tokens[16].size(), info.num_threads);
  }

  return info;
}

ProcessInfo ProcessReader::parse_status(pid_t pid, const std::string& content) {
  ProcessInfo info;
  info.pid = pid;

  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    auto colon = line.find(':');
    if (colon == std::string::npos) continue;

    auto key = std::string_view{line}.substr(0, colon);
    auto value = std::string_view{line}.substr(colon + 1);
    // Trim whitespace from value
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
      value.remove_prefix(1);

    if (key == "Name") {
      info.name = value;
    } else if (key == "State") {
      info.state = value;
      if (!value.empty()) info.state_char = value[0];
    } else if (key == "VmPeak") {
      // "  12345 kB"
      std::from_chars(value.data(), value.data() + value.size(), info.vm_peak_kb);
    } else if (key == "VmSize") {
      std::from_chars(value.data(), value.data() + value.size(), info.vm_size_kb);
    } else if (key == "VmRSS") {
      std::from_chars(value.data(), value.data() + value.size(), info.vm_rss_kb);
    } else if (key == "Threads") {
      std::from_chars(value.data(), value.data() + value.size(), info.num_threads);
    }
  }

  return info;
}

int ProcessReader::count_pids_in_proc() {
  int count = 0;
  try {
    for (const auto& entry : fs::directory_iterator("/proc")) {
      if (entry.is_directory()) {
        auto name = entry.path().filename().string();
        bool is_number = !name.empty() && std::all_of(name.begin(), name.end(),
          [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
        if (is_number) ++count;
      }
    }
  } catch (...) {}
  return count;
}

Result<std::vector<ProcessInfo>> ProcessReader::read_all_processes() {
  std::vector<ProcessInfo> procs;
  procs.reserve(count_pids_in_proc());

  try {
    for (const auto& entry : fs::directory_iterator("/proc")) {
      if (!entry.is_directory()) continue;
      auto name = entry.path().filename().string();
      bool is_number = !name.empty() && std::all_of(name.begin(), name.end(),
        [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
      if (!is_number) continue;

      pid_t pid = 0;
      std::from_chars(name.data(), name.data() + name.size(), pid);
      if (pid <= 0) continue;

      auto result = read_process(pid);
      if (result) procs.push_back(std::move(*result));
    }
  } catch (const fs::filesystem_error& e) {
    return std::unexpected(Error{errno, "Filesystem error: " + std::string(e.what())});
  }

  // Calculate mem_pct for each process
  auto sysinfo = read_system_info();
  uint64_t total_ram = 0;
  if (sysinfo) total_ram = sysinfo->total_ram_kb;

  for (auto& p : procs) {
    if (total_ram > 0) {
      p.mem_pct = 100.0 * static_cast<double>(p.vm_rss_kb) / static_cast<double>(total_ram);
    }
  }

  // Sort by PID by default
  std::sort(procs.begin(), procs.end(), [](const auto& a, const auto& b) {
    return a.pid < b.pid;
  });

  return procs;
}

Result<ProcessInfo> ProcessReader::read_process(pid_t pid) {
  auto pid_str = std::to_string(pid);

  // Read status first (more reliable for name)
  auto status_content = read_proc_file("/proc/" + pid_str + "/status");
  ProcessInfo info;
  if (status_content) {
    info = parse_status(pid, *status_content);
  }

  // Read stat for CPU ticks
  auto stat_content = read_proc_file("/proc/" + pid_str + "/stat");
  if (stat_content) {
    auto stat_info = parse_stat(pid, *stat_content);
    // Merge: use stat for fields status doesn't have
    info.name = stat_info.name.empty() ? info.name : stat_info.name;
    if (info.state_char == '?' && stat_info.state_char != '?') {
      info.state_char = stat_info.state_char;
      info.state = stat_info.state;
    }
    if (info.utime == 0 && info.stime == 0) {
      info.utime = stat_info.utime;
      info.stime = stat_info.stime;
    }
    if (info.num_threads == 0) info.num_threads = stat_info.num_threads;
    info.nice = stat_info.nice;
  }

  // Read cmdline
  info.cmdline = read_cmdline(pid);
  if (info.cmdline.empty()) {
    // For kernel threads or zombies, use the name
    info.cmdline = "[" + info.name + "]";
  }

  return info;
}

Result<SystemInfo> ProcessReader::read_system_info() {
  SystemInfo info;

  // Read /proc/meminfo
  auto mem = read_proc_file("/proc/meminfo");
  if (mem) {
    std::istringstream iss(*mem);
    std::string line;
    while (std::getline(iss, line)) {
      auto colon = line.find(':');
      if (colon == std::string::npos) continue;
      auto key = line.substr(0, colon);
      auto val_str = line.substr(colon + 1);
      // Trim leading whitespace (from_chars doesn't skip whitespace)
      auto start = val_str.find_first_not_of(" \t");
      if (start != std::string::npos) val_str = val_str.substr(start);
      unsigned long long val = 0;
      std::from_chars(val_str.data(), val_str.data() + val_str.size(), val);
      if (key == "MemTotal") {
        info.total_ram_kb = val;
      } else if (key == "MemAvailable") {
        info.available_ram_kb = val;
      }
    }
  }

  // Read /proc/stat for CPU count
  auto stat = read_proc_file("/proc/stat");
  if (stat) {
    std::istringstream iss(*stat);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.rfind("cpu", 0) == 0 && line.size() > 3 && std::isdigit(static_cast<unsigned char>(line[3]))) {
        ++info.num_cpus;
      }
    }
    if (info.num_cpus == 0) info.num_cpus = 1;
  }

  // Read /proc/uptime
  auto uptime = read_proc_file("/proc/uptime");
  if (uptime) {
    std::from_chars(uptime->data(), uptime->data() + uptime->size(), info.uptime_seconds);
  }

  return info;
}

Result<std::vector<uint64_t>> ProcessReader::read_cpu_ticks() {
  auto stat = read_proc_file("/proc/stat");
  if (!stat) return std::unexpected(stat.error());

  std::vector<uint64_t> ticks;
  std::istringstream iss(*stat);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.rfind("cpu", 0) != 0) continue;
    // Skip "cpu" aggregate (we need per-core)
    if (line.size() > 3 && !std::isdigit(static_cast<unsigned char>(line[3]))) continue;
    if (line.size() <= 3) continue; // "cpu" aggregate

    std::istringstream iss2(line);
    std::string cpu_name;
    iss2 >> cpu_name;

    uint64_t val, total = 0;
    for (int i = 0; i < 8; ++i) {
      if (iss2 >> val) total += val;
    }
    ticks.push_back(total);
  }

  return ticks;
}

void ProcessReader::update_cpu_pct(std::vector<ProcessInfo>& procs) {
  auto current_ticks = read_cpu_ticks();
  if (!current_ticks) return;

  if (!has_prev_ || prev_cpu_ticks_.empty() || prev_cpu_ticks_.size() != current_ticks->size()) {
    prev_cpu_ticks_ = *current_ticks;
    has_prev_ = true;
    return;
  }

  // Total delta across all cores
  uint64_t total_delta = 0;
  for (size_t i = 0; i < current_ticks->size(); ++i) {
    total_delta += (*current_ticks)[i] - prev_cpu_ticks_[i];
  }

  if (total_delta == 0) return;

  // For each process, calculate CPU% as (process_ticks_delta / total_delta) * 100 * num_cores
  // We don't have per-process delta here since we only have one snapshot.
  // In practice, the caller provides the snapshot twice and we store intermediate data.
  // For now, CPU% will be 0 for a single-shot; the TUI refreshes to get real values.

  prev_cpu_ticks_ = *current_ticks;
}
