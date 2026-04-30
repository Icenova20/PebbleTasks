#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Rebuilding and restarting PebbleTasks OAuth server..."
docker compose up -d --build
echo "Compose deploy complete."

read -r -p "Prune unused Docker images now? [y/N]: " prune_choice
case "${prune_choice:-}" in
  y|Y|yes|YES)
    echo "Pruning unused images..."
    docker image prune -f
    echo "Done."
    ;;
  *)
    echo "Skipping image prune."
    ;;
esac
