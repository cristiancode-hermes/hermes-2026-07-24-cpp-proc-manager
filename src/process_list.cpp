#include "procman/process_list.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>

namespace procman {

void ProcessList::set_processes(std::vector<ProcessInfo> procs) {
  processes_ = std::move(procs);
  apply_filter();
  apply_sort();
}

void ProcessList::set_filter(const std::string& filter) {
  filter_ = filter;
  std::transform(filter_.begin(), filter_.end(), filter_.begin(),
    [](unsigned char c) { return std::tolower(c); });
  apply_filter();
  apply_sort();
}

void ProcessList::set_sort(SortField field, SortDir dir) {
  sort_field_ = field;
  sort_dir_ = dir;
  apply_sort();
}

void ProcessList::toggle_sort(SortField field) {
  if (sort_field_ == field) {
    sort_dir_ = (sort_dir_ == SortDir::ASC) ? SortDir::DESC : SortDir::ASC;
  } else {
    sort_field_ = field;
    sort_dir_ = SortDir::DESC;
  }
  apply_sort();
}

void ProcessList::refresh() {
  apply_filter();
  apply_sort();
}

void ProcessList::apply_filter() {
  if (filter_.empty()) {
    view_ = processes_;
    return;
  }

  view_.clear();
  for (const auto& p : processes_) {
    auto name_lower = p.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
      [](unsigned char c) { return std::tolower(c); });
    if (name_lower.find(filter_) != std::string::npos) {
      view_.push_back(p);
      continue;
    }
    auto pid_str = std::to_string(p.pid);
    if (pid_str.find(filter_) != std::string::npos) {
      view_.push_back(p);
    }
  }
}

void ProcessList::apply_sort() {
  std::sort(view_.begin(), view_.end(), [this](const auto& a, const auto& b) {
    return compare(a, b, sort_field_, sort_dir_);
  });
}

bool ProcessList::compare(const ProcessInfo& a, const ProcessInfo& b, SortField f, SortDir d) {
  bool asc = (d == SortDir::ASC);
  switch (f) {
    case SortField::PID:
      return asc ? (a.pid < b.pid) : (a.pid > b.pid);
    case SortField::NAME:
      return asc ? (a.name < b.name) : (a.name > b.name);
    case SortField::CPU:
      return asc ? (a.cpu_pct < b.cpu_pct) : (a.cpu_pct > b.cpu_pct);
    case SortField::MEM:
      return asc ? (a.mem_pct < b.mem_pct) : (a.mem_pct > b.mem_pct);
    case SortField::STATE:
      return asc ? (a.state < b.state) : (a.state > b.state);
    case SortField::THREADS:
      return asc ? (a.num_threads < b.num_threads) : (a.num_threads > b.num_threads);
  }
  return false;
}

std::vector<std::pair<ProcessInfo, int>> ProcessList::build_tree() const {
  // Build a simple tree: parent PID → children
  // For each process, determine indentation level based on parent-child chain
  // This uses PPID; we'd need to read it but we don't store it currently.
  // Simplified: just return flat list with indentation 0
  std::vector<std::pair<ProcessInfo, int>> result;
  for (const auto& p : view_) {
    result.emplace_back(p, 0);
  }
  return result;
}

} // namespace procman
