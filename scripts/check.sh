#!/bin/bash
# 1. Auto-format everything
echo "🎨 Formatting code..."
find include examples tests -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" | xargs clang-format -i

# 2. Run the local CI
echo "🚀 Running Local CI (act)..."
# act -j verify-and-audit
act -j verify-and-audit --artifact-server-path /tmp/artifacts