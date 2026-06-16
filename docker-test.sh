#!/bin/bash
# Build (if needed) and test in Docker.
# First build: ~3–8 min (downloads Raylib + compiles everything).
# Later runs: ~5–30 s if nothing changed (Docker cache).
set -e
cd "$(dirname "$0")"

REBUILD=0
for arg in "$@"; do
  case "$arg" in
    --rebuild|-r) REBUILD=1 ;;
  esac
done

if [[ "$REBUILD" -eq 0 ]] && ./docker-run.sh image inspect airdefence:latest &>/dev/null; then
  echo "Image airdefence:latest already exists — skipping rebuild."
  echo "  (Use ./docker-test.sh --rebuild to force a full rebuild)"
else
  echo "Building Docker image (step 5 compiles C++ + Raylib — this takes a few minutes)..."
  time ./docker-run.sh build -t airdefence .
fi

echo ""
echo "Running tests in container..."
./docker-run.sh run --rm airdefence test
echo ""
echo "Done."
echo "  Game (native, fastest):  build/air_defense_game"
echo "  Headless in Docker:      ./docker-run.sh run --rm airdefence debug"
echo "  GUI in Docker:           ./docker-run.sh run --rm -e DISPLAY=\$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix airdefence gui"
