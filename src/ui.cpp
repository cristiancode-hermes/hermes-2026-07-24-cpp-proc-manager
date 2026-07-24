#include "procman/ui.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

#ifdef PROCMAN_USE_TUI
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>
using namespace ftxui;
#endif
using namespace std::chrono_literals;

namespace procman {

// ── Helper functions (shared by TUI and CLI) ──
static std::string fmt_mem(uint64_t kb) {
  if (kb > 1024 * 1024) return std::to_string(kb / (1024 * 1024)) + "G";
  if (kb > 1024) return std::to_string(kb / 1024) + "M";
  return std::to_string(kb) + "K";
}

static std::string fmt_pct(double pct) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%5.1f", pct);
  return buf;
}

// ── TUI implementation (only with FTXUI) ──
#ifdef PROCMAN_USE_TUI

static Decorator state_color(char state) {
  switch (state) {
    case 'R': return color(Color::Green);
    case 'S': return color(Color::Cyan);
    case 'D': return color(Color::Red);
    case 'Z': return color(Color::RedLight);
    case 'T': return color(Color::Yellow);
    default:  return color(Color::GrayDark);
  }
}

static Decorator pct_color(double pct) {
  if (pct > 50.0) return color(Color::Red);
  if (pct > 20.0) return color(Color::Yellow);
  return color(Color::Green);
}

ProcManagerTUI::ProcManagerTUI() {
  spdlog::set_level(spdlog::level::err); // suppress debug logging in TUI
}

ProcManagerTUI::~ProcManagerTUI() {
  running_ = false;
  if (refresh_thread_.joinable()) refresh_thread_.join();
}

void ProcManagerTUI::refresh_data() {
  // Called from refresh thread
  auto procs = reader_.read_all_processes();
  if (procs) {
    list_.set_processes(std::move(*procs));
  }
  auto sysinfo = reader_.read_system_info();
  if (sysinfo) sysinfo_ = *sysinfo;
}

void ProcManagerTUI::do_refresh() {
  while (running_) {
    std::this_thread::sleep_for(2000ms); // refresh every 2 seconds
    refresh_data();
    sort_cycle_++;
  }
}

int ProcManagerTUI::run() {
  // Initial data load
  refresh_data();

  auto screen = ScreenInteractive::Fullscreen();

  // State
  std::string search_text;
  bool show_help = false;
  bool show_details = false;
  int selected_idx = 0;
  int scroll_offset = 0;

  // ── Input for search ──
  auto search_input = Input(&search_text, "filter (name or PID)");
  search_input |= CatchEvent([&](const Event& e) {
    if (e == Event::Return) {
      list_.set_filter(search_text);
      return true;
    }
    return false;
  });

  // ── Main renderer ──
  auto renderer = Renderer([&] {
    const auto& procs = list_.view();
    auto sys = sysinfo_;

    // Header: system info
    auto header = hbox(Elements{
      text("  proc-manager ") | bold | color(Color::Cyan),
      separator(),
      text(" CPU:" + std::to_string(sys.num_cpus) + " core") | color(Color::GrayDark),
      separator(),
      text(" RAM: " + fmt_mem(sys.available_ram_kb) + "/" + fmt_mem(sys.total_ram_kb)) | color(Color::GrayDark),
      separator(),
      text("Up: " + std::to_string(static_cast<int>(sys.uptime_seconds / 3600)) + "h") | color(Color::GrayDark),
      filler(),
      text("Sort: ") | color(Color::GrayDark),
      [&]() -> Element {
        switch (list_.sort_field()) {
          case SortField::PID: return text("PID") | bold;
          case SortField::NAME: return text("NAME") | bold;
          case SortField::CPU: return text("CPU") | bold | color(Color::Yellow);
          case SortField::MEM: return text("MEM") | bold | color(Color::Yellow);
          case SortField::STATE: return text("STATE") | bold;
          case SortField::THREADS: return text("THR") | bold;
        }
        return text("");
      }(),
      text(list_.sort_dir() == SortDir::DESC ? " ▼" : " ▲") | color(Color::GrayDark),
      separator(),
      text("Procs: ") | color(Color::GrayDark),
      text(std::to_string(procs.size())) | bold,
      separator(),
      text("F1=help  /:filter  q:quit") | color(Color::GrayDark),
    }) | border;

    // Column headers
    auto col_headers = hbox(Elements{
      text("  PID     ") | bold,
      text("NAME                          ") | bold,
      text("CPU%   ") | bold,
      text("MEM%   ") | bold,
      text("STATE ") | bold,
      text("THR ") | bold,
      text("RSS      ") | bold,
      text("CMDLINE") | bold,
    }) | color(Color::GrayDark);

    // Process rows
    Elements rows;
    int display_count = std::min(static_cast<int>(procs.size()), 100);
    for (int i = scroll_offset; i < scroll_offset + display_count && i < static_cast<int>(procs.size()); ++i) {
      const auto& p = procs[i];
      auto selected = (i == selected_idx);

      // Truncate name to 30 chars
      auto name_display = p.name.substr(0, 30);
      auto cmdline_display = p.cmdline.substr(0, 50);
      if (cmdline_display.size() < p.cmdline.size()) cmdline_display += "...";

      auto row = hbox(Elements{
        text("  " + std::to_string(p.pid) + "  ") | (selected ? bold : Element{}),
        text(name_display + std::string(30 - name_display.size(), ' ')),
        text(fmt_pct(p.cpu_pct) + "%") | pct_color(p.cpu_pct) | size(WIDTH, EQUAL, 7),
        text(fmt_pct(p.mem_pct) + "%") | pct_color(p.mem_pct) | size(WIDTH, EQUAL, 7),
        text("  " + p.state + std::string(3 - p.state.size(), ' ')) | state_color(p.state_char) | size(WIDTH, EQUAL, 6),
        text("  " + std::to_string(p.num_threads) + " ") | size(WIDTH, EQUAL, 4),
        text(fmt_mem(p.vm_rss_kb) + "     ").substr(0, 8) | size(WIDTH, EQUAL, 8),
        text(cmdline_display),
      });

      if (selected) {
        row = row | inverted;
      }
      rows.push_back(row);
    }

    // Scroll indicators
    if (static_cast<int>(procs.size()) > display_count) {
      rows.push_back(text("  ... " + std::to_string(procs.size() - display_count - scroll_offset) + " more processes") | color(Color::GrayDark));
    }

    auto content = vbox(std::move(rows));

    // Help overlay
    if (show_help) {
      auto help_box = window(
        text(" Help ") | bold | color(Color::Cyan),
        vbox(Elements{
          text("  ↑/↓       Navigate processes"),
          text("  /          Search/filter by name or PID"),
          text("  Enter      Apply filter"),
          text("  s          Sort by CPU"),
          text("  m          Sort by memory"),
          text("  p          Sort by PID"),
          text("  n          Sort by name"),
          text("  t          Sort by threads"),
          text("  Esc/Ctrl+C Clear filter / quit"),
          text("  q          Quit"),
          text("  F1         Toggle this help"),
          separator(),
          text("  Source: /proc filesystem"),
          text("  Refresh: every 2 seconds"),
        })
      ) | border | size(WIDTH, GREATER_THAN, 40);

      return header | vbox(Elements{help_box, col_headers, content}) | border;
    }

    return header | vbox(Elements{col_headers, separator(), content}) | border;
  });

  // ── Event handling ──
  renderer |= CatchEvent([&](const Event& e) {
    if (e == Event::F1 || (e == Event::Character('h') && show_help)) {
      show_help = !show_help;
      return true;
    }
    if (e == Event::Character('q')) {
      running_ = false;
      screen.ExitLoopClosure()();
      return true;
    }
    if (e == Event::Escape) {
      search_text.clear();
      list_.set_filter("");
      return true;
    }
    if (e == Event::Character('/')) {
      show_help = false;
      return true;
    }
    if (e == Event::Character('s')) {
      list_.toggle_sort(SortField::CPU);
      return true;
    }
    if (e == Event::Character('m')) {
      list_.toggle_sort(SortField::MEM);
      return true;
    }
    if (e == Event::Character('p')) {
      list_.toggle_sort(SortField::PID);
      return true;
    }
    if (e == Event::Character('n')) {
      list_.toggle_sort(SortField::NAME);
      return true;
    }
    if (e == Event::Character('t')) {
      list_.toggle_sort(SortField::THREADS);
      return true;
    }
    if (e == Event::ArrowUp || e == Event::Character('k')) {
      selected_idx = std::max(0, selected_idx - 1);
      if (selected_idx < scroll_offset) scroll_offset = selected_idx;
      return true;
    }
    if (e == Event::ArrowDown || e == Event::Character('j')) {
      selected_idx = std::min(static_cast<int>(list_.view().size()) - 1, selected_idx + 1);
      if (selected_idx >= scroll_offset + display_count) {
        // Increase threshold
      }
      return true;
    }
    return false;
  });

  // ── Search component ──
  auto search_container = Container::Horizontal({search_input});

  // ── Start refresh thread ──
  refresh_thread_ = std::thread([this] { do_refresh(); });

  // ── Main loop ──
  screen.Loop(renderer);
  running_ = false;

  return 0;
}

#endif // PROCMAN_USE_TUI

// ── CLI mode (always available) ──
int run_cli(int argc, char** argv) {
  CLI::App app{"proc-manager — Interactive Linux Process Manager (FTXUI TUI + CLI mode)"};

  bool json_mode = false;
  int count = 3;
  bool no_header = false;

  app.add_flag("--json", json_mode, "Output as JSON lines");
  app.add_option("-n,--count", count, "Number of top processes to show")->default_val(3);
  app.add_flag("--no-header", no_header, "Do not print header row");

  // Strip --cli argument if present (handled by main)
  std::vector<const char*> filtered_argv;
  filtered_argv.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg != "--cli") {
      filtered_argv.push_back(argv[i]);
    }
  }
  int filtered_argc = static_cast<int>(filtered_argv.size());

  CLI11_PARSE(app, filtered_argc, filtered_argv.data());

  ProcessReader reader;
  auto procs = reader.read_all_processes();
  if (!procs) {
    std::cerr << "Error: " << procs.error().message << '\n';
    return 1;
  }

  auto sysinfo = reader.read_system_info();
  if (!sysinfo) {
    std::cerr << "Error reading system info: " << sysinfo.error().message << '\n';
    return 1;
  }

  // Sort by mem% descending
  std::sort(procs->begin(), procs->end(), [](const auto& a, const auto& b) {
    return a.mem_pct > b.mem_pct;
  });

  if (json_mode) {
    // JSON lines
    std::cout << "{\"timestamp\":" << std::time(nullptr) << ",\"num_cpus\":" << sysinfo->num_cpus
              << ",\"total_ram_kb\":" << sysinfo->total_ram_kb
              << ",\"available_ram_kb\":" << sysinfo->available_ram_kb
              << ",\"uptime_seconds\":" << sysinfo->uptime_seconds
              << "}\n";
    int limit = std::min(count, static_cast<int>(procs->size()));
    for (int i = 0; i < limit; ++i) {
      const auto& p = (*procs)[i];
      std::cout << "{\"pid\":" << p.pid
                << ",\"name\":\"" << p.name
                << "\",\"cmdline\":\"" << p.cmdline
                << "\",\"state\":\"" << p.state_char
                << "\",\"cpu_pct\":" << p.cpu_pct
                << ",\"mem_pct\":" << p.mem_pct
                << ",\"vm_rss_kb\":" << p.vm_rss_kb
                << ",\"num_threads\":" << p.num_threads
                << "}\n";
    }
    return 0;
  }

  // Table output
  if (!no_header) {
    std::println(std::cout, "{:>6} {:<30} {:>6} {:>6} {:<4} {:>4} {:>8}  {}",
                 "PID", "NAME", "CPU%", "MEM%", "STA", "THR", "RSS", "CMDLINE");
    std::println(std::cout, "{:-<80}", "");
  }

  int limit = std::min(count, static_cast<int>(procs->size()));
  for (int i = 0; i < limit; ++i) {
    const auto& p = (*procs)[i];
    std::println(std::cout, "{:>6} {:<30} {:>5.1f} {:>5.1f} {:<4} {:>4} {:>8}  {}",
                 p.pid, p.name.substr(0, 30),
                 p.cpu_pct, p.mem_pct,
                 std::string(1, p.state_char),
                 p.num_threads,
                 fmt_mem(p.vm_rss_kb),
                 p.cmdline.substr(0, 60));
  }

  return 0;
}

} // namespace procman
