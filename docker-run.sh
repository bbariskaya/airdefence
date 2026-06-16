#!/bin/bash
# Easy Docker commands for airdefence.
set -e
export DOCKER_BUILDKIT=1
ROOT="$(cd "$(dirname "$0")" && pwd)"
IMAGE=airdefence:latest

run_docker() {
  sg docker -c "DOCKER_BUILDKIT=1 docker $*"
}

gui_docker_args() {
  local disp="${DISPLAY:-:0}"
  GUI_EXTRA=(
    -e "DISPLAY=$disp"
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw
  )
  if [[ -n "${XAUTHORITY:-}" && -f "$XAUTHORITY" ]]; then
    GUI_EXTRA+=(-v "$XAUTHORITY:/tmp/.Xauthority:ro" -e XAUTHORITY=/tmp/.Xauthority)
  fi
  # Allow container to connect to host X11 / XWayland (Ubuntu Wayland)
  if command -v xhost >/dev/null 2>&1; then
    xhost +local:docker >/dev/null 2>&1 || xhost +si:localuser:docker >/dev/null 2>&1 || true
  fi
}

case "${1:-}" in
  test)
    run_docker run --rm "$IMAGE" test
    ;;
  debug)
    run_docker run --rm "$IMAGE" debug
    ;;
  gui)
    if [[ ! -e /tmp/.X11-unix ]]; then
      echo "No X11 socket — are you in a desktop session?"
      echo "For the game window, run natively instead:"
      echo "  $ROOT/build/air_defense_game"
      exit 1
    fi
    gui_docker_args
    echo "Starting Iron Dome GUI in Docker (DISPLAY=${DISPLAY:-:0})..."
    run_docker run --rm "${GUI_EXTRA[@]}" "$IMAGE" gui
    ;;
  radar)
    gui_docker_args
    run_docker run --rm "${GUI_EXTRA[@]}" "$IMAGE" radar
    ;;
  console)
    run_docker run --rm "$IMAGE" console
    ;;
  build)
    shift
    run_docker build -t airdefence "$ROOT" "$@"
    ;;
  run)
    shift
    if [[ $# -eq 0 ]]; then
      echo "Usage: ./docker-run.sh {test|debug|gui|build}"
      echo "  test   — all tests"
      echo "  debug  — headless sim"
      echo "  gui    — game window (needs desktop + xhost)"
      exit 1
    fi
    run_docker run "$@"
    ;;
  *)
    if [[ $# -eq 0 ]]; then
      echo "Usage: ./docker-run.sh {test|debug|gui|build}"
      exit 1
    fi
    run_docker "$@"
    ;;
esac
