#pragma once

#include "types.hpp"
#include <vector>
#include <string>
#include <functional>
#include <algorithm>

namespace procman {

// ── Sort order ──
enum class SortField { PID, NAME, CPU, MEM, STATE, THREADS };
enum class SortDir { ASC, DESC };

// ── Filter mode ──
enum class ViewMode { LIST, TREE };

// ── ProcessList: container with sorting and filtering ──
class ProcessList {
public:
  ProcessList() = default;

  void set_processes(std::vector<ProcessInfo> procs);
  void set_filter(const std::string& filter);
  void set_sort(SortField field, SortDir dir = SortDir::DESC);
  void toggle_sort(SortField field);
  void refresh();

  [[nodiscard]] const std::vector<ProcessInfo>& view() const { return view_; }
  [[nodiscard]] const std::vector<ProcessInfo>& all() const { return processes_; }
  [[nodiscard]] SortField sort_field() const { return sort_field_; }
  [[nodiscard]] SortDir sort_dir() const { return sort_dir_; }
  [[nodiscard]] const std::string& filter() const { return filter_; }

  // Calculate tree: return list with indentation levels based on parent-child
  // relationships (simplified: uses /proc/[pid]/stat PPID field)
  [[nodiscard]] std::vector<std::pair<ProcessInfo, int>> build_tree() const;

private:
  std::vector<ProcessInfo> processes_;
  std::vector<ProcessInfo> view_;
  std::string filter_;
  SortField sort_field_{SortField::CPU};
  SortDir sort_dir_{SortDir::DESC};

  void apply_filter();
  void apply_sort();
  static bool compare(const ProcessInfo& a, const ProcessInfo& b, SortField f, SortDir d);
};

} // namespace procman
