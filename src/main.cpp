#include "procman/ui.hpp"
#include "procman/process_reader.hpp"

#include <iostream>
#include <string_view>

using namespace procman;

int main(int argc, char** argv) {
  // Simple manual arg parsing (CLI11 not needed for just TUI vs CLI mode)
  bool use_cli = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg == "--help" || arg == "-h") {
      std::println(std::cout, "Usage: proc-manager [--cli] [--json] [-n N]");
      std::println(std::cout, "  --cli       Non-interactive CLI mode (default: TUI)");
      std::println(std::cout, "  --json      JSON output (implies --cli)");
      std::println(std::cout, "  -n, --count N  Show top N processes (CLI mode, default: 3)");
      return 0;
    }
    if (arg == "--cli") { use_cli = true; }
    if (arg == "--json") { use_cli = true; }
    if (arg == "-n" || arg == "--count") { use_cli = true; }
  }

  if (use_cli) {
    return run_cli(argc, argv);
  }

#ifdef PROCMAN_USE_TUI
  // ── TUI mode ──
  ProcManagerTUI app_tui;
  return app_tui.run();
#else
  // TUI not built — fall back to CLI
  std::cerr << "TUI mode not available (compiled without FTXUI). Falling back to CLI.\n";
  return run_cli(argc, argv);
#endif
}
