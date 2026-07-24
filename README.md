# proc-manager — Interactive Linux Process Manager

Monitor de procesos interactivo para Linux con interfaz TUI (FTXUI) y modo CLI. Lee información detallada de procesos desde `/proc/` con actualización en tiempo real.

## Características

- **Interfaz TUI interactiva** con navegación por teclado
- **Listado completo** de procesos con PID, nombre, CPU%, MEM%, estado, hilos
- **Ordenamiento** por CPU, memoria, PID, nombre, estado o número de hilos
- **Filtro** por nombre o PID (presiona `/` y escribe)
- **Información del sistema**: núcleos de CPU, RAM total/disponible, uptime
- **Modo CLI** para scripts: salida en tabla o JSON
- **Actualización en tiempo real** cada 2 segundos
- **Colores por estado/uso**: procesos en ejecución (verde), dormidos (cyan), zombies (rojo), etc.

## Requisitos

- Linux (lectura de `/proc`)
- CMake ≥ 3.28
- Compilador C++23 (GCC 14+, Clang 18+)
- Opcional: FTXUI (se descarga automáticamente vía FetchContent)

## Compilación

```bash
# Debug (con tests, sanitizers)
cmake --preset debug
cmake --build --preset debug

# Release (optimizado, LTO)
cmake --preset release
cmake --build --preset release

# CLI only (sin dependencia FTXUI)
cmake --preset cli-only
cmake --build --preset cli-only
```

## Uso

### TUI interactivo (por defecto)

```bash
./build/debug/proc-manager
```

Teclas:
- `↑/↓` o `j/k` — Navegar procesos
- `/` — Buscar/filtrar (Enter para aplicar, Escape para limpiar)
- `s` — Ordenar por CPU
- `m` — Ordenar por memoria
- `p` — Ordenar por PID
- `n` — Ordenar por nombre
- `t` — Ordenar por hilos
- `q` — Salir
- `F1` — Ayuda

### CLI (no interactivo)

```bash
# Tabla de procesos (top 10 por uso de memoria)
./build/cli/proc-manager --cli -n 10

# Salida JSON
./build/cli/proc-manager --cli --json
```

## Tests

```bash
cd build/debug && ctest --output-on-failure
```

## Estructura

```
include/procman/
├── types.hpp           # Tipos: Error, Result, ProcessInfo, SystemInfo
├── process_reader.hpp  # Lector de /proc/[pid]/*
├── process_list.hpp    # Contenedor con ordenamiento y filtro
└── ui.hpp              # Interfaz TUI (FTXUI) y modo CLI

src/
├── main.cpp            # Entry point
├── process_reader.cpp  # Implementación de lectura /proc
├── process_list.cpp    # Ordenamiento y filtrado
└── ui.cpp              # TUI y CLI

tests/
└── test_main.cpp       # Tests unitarios y de integración
```

## Licencia

MIT
