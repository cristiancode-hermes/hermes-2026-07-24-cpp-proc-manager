# Architecture: proc-manager

```
┌─────────────────────────────────────────────────────────┐
│                    proc-manager                          │
├─────────────────────────────────────────────────────────┤
│  main.cpp                                                │
│    ├── Modo TUI → ProcManagerTUI (FTXUI)                │
│    └── Modo CLI → run_cli() (CLI11)                     │
├─────────────────────────────────────────────────────────┤
│  Procesos      │  UI                                     │
│  (process_reader.cpp)  │  (ui.cpp)                       │
│  ┌─────────────────┐   │  ┌─────────────────────┐        │
│  │ /proc/[pid]/stat │   │  │ FTXUI:              │        │
│  │ /proc/[pid]/status│  │  │ - ScreenInteractive │        │
│  │ /proc/[pid]/cmdline│ │  │ - Renderer           │        │
│  │ /proc/stat       │   │  │ - CatchEvent         │        │
│  │ /proc/meminfo    │   │  │ - hbox/vbox layout   │        │
│  │ /proc/uptime     │   │  └─────────────────────┘        │
│  └─────────────────┘   │                                  │
│                        │  ┌─────────────────────┐         │
│  ┌─────────────────┐   │  │ CLI mode:            │        │
│  │ ProcessList      │   │  │ - formatted table    │        │
│  │ - sorting        │   │  │ - JSON output        │        │
│  │ - filtering      │   │  └─────────────────────┘         │
│  └─────────────────┘   │                                  │
└─────────────────────────────────────────────────────────┘
```

## Flujo de datos

1. **Inicialización**: `main.cpp` parsea argumentos, decide TUI o CLI
2. **TUI**: `ProcManagerTUI::run()` carga datos iniciales, lanza hilo de refresco, entra en `screen.Loop()`
3. **Refresco**: Cada 2s, el hilo llama `ProcessReader::read_all_processes()` → `ProcessList::set_processes()`
4. **Renderizado**: `Renderer` construye DOM FTXUI cada frame: header (info sistema) + tabla de procesos
5. **Eventos**: `CatchEvent` maneja teclas: navegación, ordenamiento, filtro, salida
6. **CLI**: `run_cli()` lee procesos una vez, ordena, imprime tabla o JSON

## Fuentes de datos (Linux /proc)

| Archivo | Contenido | Función |
|---------|-----------|---------|
| `/proc/[pid]/stat` | Estado, PID padre, CPU ticks, prioridad, hilos | `parse_stat()` |
| `/proc/[pid]/status` | Nombre, estado, VmPeak, VmSize, VmRSS, Threads | `parse_status()` |
| `/proc/[pid]/cmdline` | Línea de comandos completa (null-separada) | `read_cmdline()` |
| `/proc/stat` | CPU total y por núcleo, context switches | `read_cpu_ticks()` |
| `/proc/meminfo` | MemTotal, MemAvailable | `read_system_info()` |
| `/proc/uptime` | Tiempo de actividad del sistema | `read_system_info()` |
