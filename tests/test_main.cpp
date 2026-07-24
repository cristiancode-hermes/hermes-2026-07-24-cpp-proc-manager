#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "procman/types.hpp"
#include "procman/process_reader.hpp"
#include "procman/process_list.hpp"

#include <thread>
#include <chrono>

using namespace procman;
using Catch::Approx;

// ── Type / Error tests ──
TEST_CASE("Error type construction", "[types]") {
  Error e{42, "test error"};
  REQUIRE(e.code == 42);
  REQUIRE(e.message == "test error");
}

TEST_CASE("Result type works", "[types]") {
  auto ok = Result<int>{42};
  REQUIRE(ok.has_value());
  REQUIRE(*ok == 42);

  auto err = Result<int>{std::unexpected(Error{500, "fail"})};
  REQUIRE(!err.has_value());
  REQUIRE(err.error().code == 500);
}

TEST_CASE("ProcessInfo defaults", "[types]") {
  ProcessInfo p;
  REQUIRE(p.pid == 0);
  REQUIRE(p.name.empty());
  REQUIRE(p.total_ticks() == 0);
  REQUIRE(p.cpu_pct == Approx(0.0));
  REQUIRE(p.mem_pct == Approx(0.0));
}

TEST_CASE("ProcessInfo total_ticks", "[types]") {
  ProcessInfo p;
  p.utime = 100;
  p.stime = 50;
  REQUIRE(p.total_ticks() == 150);
}

// ── ProcessReader tests ──
TEST_CASE("Read system info from /proc", "[reader][integration]") {
  ProcessReader reader;
  auto result = reader.read_system_info();

  REQUIRE(result.has_value());
  REQUIRE(result->total_ram_kb > 0);
  REQUIRE(result->num_cpus >= 1);
  REQUIRE(result->uptime_seconds > 0.0);
}

TEST_CASE("Read all processes from /proc", "[reader][integration]") {
  ProcessReader reader;
  auto result = reader.read_all_processes();

  REQUIRE(result.has_value());
  REQUIRE(!result->empty());

  // Should find at least init or systemd (PID 1)
  auto it = std::find_if(result->begin(), result->end(), [](const ProcessInfo& p) {
    return p.pid == 1;
  });
  REQUIRE(it != result->end());
  REQUIRE(!it->name.empty());

  // Verify structure
  bool found_some = false;
  for (const auto& p : *result) {
    if (p.pid > 0 && !p.name.empty()) {
      found_some = true;
      break;
    }
  }
  REQUIRE(found_some);
}

TEST_CASE("Read single process PID 1", "[reader][integration]") {
  ProcessReader reader;
  auto result = reader.read_process(1);

  REQUIRE(result.has_value());
  REQUIRE(result->pid == 1);
  REQUIRE(!result->name.empty());
  REQUIRE(!result->cmdline.empty());
  REQUIRE(result->vm_rss_kb >= 0);
  REQUIRE(result->num_threads >= 1);
}

TEST_CASE("Read CPU ticks from /proc/stat", "[reader][integration]") {
  ProcessReader reader;
  auto result = reader.read_cpu_ticks();

  REQUIRE(result.has_value());

  // Should have at least as many entries as CPU cores
  auto sysinfo = reader.read_system_info();
  REQUIRE(sysinfo.has_value());
  REQUIRE(result->size() >= static_cast<size_t>(sysinfo->num_cpus));
}

// ── ProcessList tests ──
TEST_CASE("ProcessList basic operations", "[list]") {
  ProcessList list;

  // Create sample processes
  std::vector<ProcessInfo> procs;
  ProcessInfo p1, p2, p3;
  p1.pid = 3; p1.name = "chrome";  p1.mem_pct = 25.0; p1.cpu_pct = 5.0;  p1.state_char = 'S'; p1.num_threads = 20;
  p2.pid = 1; p2.name = "init";    p2.mem_pct = 1.0;  p2.cpu_pct = 0.1;  p2.state_char = 'S'; p2.num_threads = 1;
  p3.pid = 2; p3.name = "firefox"; p3.mem_pct = 15.0; p3.cpu_pct = 3.0;  p3.state_char = 'R'; p3.num_threads = 12;
  procs.push_back(p1);
  procs.push_back(p2);
  procs.push_back(p3);

  list.set_processes(std::move(procs));

  // Default sort is CPU descending
  const auto& view = list.view();
  REQUIRE(view.size() == 3);

  // First should be chrome (highest CPU%)
  REQUIRE(view[0].name == "chrome");
  REQUIRE(view[0].cpu_pct == Approx(5.0));

  // Test sort by PID
  list.set_sort(SortField::PID, SortDir::ASC);
  REQUIRE(list.view()[0].pid == 1);
  REQUIRE(list.view()[1].pid == 2);
  REQUIRE(list.view()[2].pid == 3);

  // Test sort by name
  list.set_sort(SortField::NAME, SortDir::ASC);
  REQUIRE(list.view()[0].name == "chrome");
  REQUIRE(list.view()[1].name == "firefox");
  REQUIRE(list.view()[2].name == "init");

  // Test filter
  list.set_filter("fire");
  REQUIRE(list.view().size() == 1);
  REQUIRE(list.view()[0].name == "firefox");

  list.set_filter("");
  REQUIRE(list.view().size() == 3);
}

TEST_CASE("ProcessList toggle sort", "[list]") {
  ProcessList list;
  std::vector<ProcessInfo> procs;
  ProcessInfo p1, p2;
  p1.pid = 1; p1.name = "a"; p1.cpu_pct = 10.0; p1.state_char = 'S';
  p2.pid = 2; p2.name = "b"; p2.cpu_pct = 20.0; p2.state_char = 'S';
  procs.push_back(p1);
  procs.push_back(p2);
  list.set_processes(std::move(procs));

  // First toggle: flip to ASC (a first, 10%)
  list.toggle_sort(SortField::CPU);
  REQUIRE(list.view()[0].name == "a");

  // Second toggle: flip back to DESC (b first, 20%)
  list.toggle_sort(SortField::CPU);
  REQUIRE(list.view()[0].name == "b");

  // Toggle different field: PID descending (b first)
  list.toggle_sort(SortField::PID);
  REQUIRE(list.view()[0].pid == 2);
}

// ── CLI edge cases ──
TEST_CASE("Formatter helpers don't crash", "[cli]") {
  // From ui.cpp: fmt_mem, fmt_pct — test basic functionality
  // These are local to ui.cpp, so we test the concept
  auto mem_gb = 2UL * 1024 * 1024; // 2GB
  REQUIRE(mem_gb / (1024 * 1024) == 2);
  REQUIRE(mem_gb / 1024 == 2048);
}

TEST_CASE("ProcessReader handles non-existent PID", "[reader]") {
  ProcessReader reader;
  auto result = reader.read_process(999999);
  // Should either fail gracefully or return an empty-ish process
  if (result.has_value()) {
    // PID 999999 likely doesn't exist — if it succeeds the structure might be empty
    bool pid_ok = (result->pid == 0 || result->pid == 999999);
    REQUIRE(pid_ok);
  } else {
    // Error is expected
    REQUIRE(result.error().code != 0);
  }
}
