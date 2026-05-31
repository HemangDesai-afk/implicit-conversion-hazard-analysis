#!/usr/bin/env bash
# Run the analyzer on a directory without a compile_commands.json
# Usage: ./run_no_db.sh <dir> <output_base_name>

set -euo pipefail

DIR="$1"
BASE_NAME="$2"
ANALYZER="./build/implicit-conversion-hazard"
THRESHOLD=50

echo "=== Analyzing $DIR ==="
FILES=$(find "$DIR" -name "*.c" -o -name "*.cpp" -o -name "*.cc" | sort)
COUNT=$(echo "$FILES" | wc -l)
echo "Files found: $COUNT"

echo "$FILES" | xargs "$ANALYZER" \
    --risk-threshold "$THRESHOLD" \
    --markdown \
    --extra-arg=-w \
    -- \
    >> "${BASE_NAME}_dashboard.md"

echo "$FILES" | xargs "$ANALYZER" \
    --risk-threshold "$THRESHOLD" \
    --json \
    --extra-arg=-w \
    -- \
    >> "${BASE_NAME}_findings.json"

echo "Done. Results saved to ${BASE_NAME}_dashboard.md and ${BASE_NAME}_findings.json"
