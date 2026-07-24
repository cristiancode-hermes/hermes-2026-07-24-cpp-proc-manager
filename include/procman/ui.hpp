#pragma once

#include "process_list.hpp"
#include "process_reader.hpp"

#include <atomic>
#include <thread>

namespace procman {

// ── ProcManagerTUI: FTXUI-based interactive process manager ──
class ProcManagerTUI {
public:
  ProcManagerTUI();
  ~ProcManagerTUI();

  ProcManagerTUI(const ProcManagerTUI&) = delete;
  ProcManagerTUI& operator=(const ProcManagerTUI&) = delete;
  ProcManagerTUI(ProcManagerTUI&&) noexcept = delete;
  ProcManagerTUI& operator=(ProcManagerTUI&&) noexcept = delete;

  int run();

private:
  void refresh_data();
  void do_refresh();

  ProcessReader reader_;
  ProcessList list_;
  SystemInfo sysinfo_;

  std::atomic<bool> running_{true};
  std::thread refresh_thread_;

  int sort_cycle_{0};
};

// ── CLI mode: simple text output ──
int run_cli(int argc, char** argv);

} // namespace procman
