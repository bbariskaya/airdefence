#!/bin/sh
set -e
cd /app

case "${1:-test}" in
  test)
    echo "=== test_unit ===" && ./test_unit
    echo "=== test_integration ===" && ./test_integration
    echo "=== test_system ===" && ./test_system
    ;;
  debug)
    exec ./air_defense_debug
    ;;
  gui)
    exec ./air_defense_game
    ;;
  radar)
    exec ./radar_sim
    ;;
  console)
    exec ./radar_console
    ;;
  *)
    exec "$@"
    ;;
esac
