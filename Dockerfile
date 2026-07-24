# Stage 1: Build
FROM gcc:14-bookworm AS builder
RUN apt-get update && apt-get install -y cmake pkg-config
WORKDIR /src
COPY CMakeLists.txt CMakePresets.json ./
COPY include/ src/ tests/ ./
RUN cmake --preset release && cmake --build --preset release

# Stage 2: Runtime (~25 MB)
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/release/proc-manager /usr/local/bin/proc-manager
ENTRYPOINT ["proc-manager"]
