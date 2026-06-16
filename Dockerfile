# Iron Dome / RadarSim — reproducible build & headless run on any Linux host with Docker.
#
# Default (tests + console sim):
#   docker build -t airdefence .
#   docker run --rm airdefence
#
# GUI (Linux host with X11 only):
#   xhost +local:docker
#   docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix airdefence gui

FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    libgl1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxi-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxext-dev \
    libxfixes-dev \
    libxrender-dev \
    libxcb1-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libasound2-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRADAR_BUILD_GRAPHICS=ON \
    && cmake --build build -j"$(nproc)"

# Run full test suite at build time — image only tagged if tests pass.
RUN cd build && ./test_unit && ./test_integration && ./test_system

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1 \
    libx11-6 \
    libxrandr2 \
    libxi6 \
    libxcursor1 \
    libxinerama1 \
    libxext6 \
    libxfixes3 \
    libxrender1 \
    libxcb1 \
    libxkbcommon0 \
    libwayland-client0 \
    libasound2t64 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/test_unit /src/build/test_integration /src/build/test_system ./
COPY --from=builder /src/build/air_defense_debug ./
COPY --from=builder /src/build/air_defense_game ./
COPY --from=builder /src/build/radar_console ./
COPY --from=builder /src/build/radar_sim ./

COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["test"]
