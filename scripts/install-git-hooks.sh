#!/usr/bin/env bash
# Point this repo at scripts/git-hooks/ (run once per clone).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

chmod +x scripts/git-hooks/pre-commit

git config core.hooksPath scripts/git-hooks

echo "Installed git hooks (core.hooksPath=scripts/git-hooks)"
echo "  pre-commit: removes oauth_config.local.js, runs pebble build when SDK is available"
echo ""
echo "  SKIP_PEBBLE_BUILD=1 git commit   # skip build"
echo "  git commit --no-verify           # skip entire hook"
