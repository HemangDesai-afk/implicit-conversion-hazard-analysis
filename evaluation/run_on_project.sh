#!/usr/bin/env bash
# Run the Implicit Conversion Hazard Analyzer on a target project
#
# Usage: ./run_on_project.sh <project_dir> [--threshold N]
#
# Prerequisites:
#   - The target project must have a compile_commands.json
#   - The analyzer binary must be built
#
# Example:
#   ./run_on_project.sh /path/to/sqlite --threshold 50

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYZER="${SCRIPT_DIR}/../build/implicit-conversion-hazard"
OUTPUT_DIR="${SCRIPT_DIR}/output"

THRESHOLD=50
PROJECT_DIR=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --threshold)
            THRESHOLD="$2"
            shift 2
            ;;
        *)
            PROJECT_DIR="$1"
            shift
            ;;
    esac
done

if [[ -z "$PROJECT_DIR" ]]; then
    echo "Usage: $0 <project_dir> [--threshold N]"
    exit 1
fi

if [[ ! -f "$ANALYZER" ]]; then
    echo "Error: Analyzer binary not found at $ANALYZER"
    echo "Run ../scripts/build.sh first."
    exit 1
fi

if [[ ! -f "$PROJECT_DIR/compile_commands.json" ]]; then
    echo "Warning: compile_commands.json not found in $PROJECT_DIR"
    echo "Falling back to directory scan (using default C/C++ flags)..."
    echo ""
fi

mkdir -p "$OUTPUT_DIR"
PROJECT_NAME=$(basename "$PROJECT_DIR")
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_DIR="${SCRIPT_DIR}/output"
MARKDOWN_FILE="${OUTPUT_DIR}/${PROJECT_NAME}_${TIMESTAMP}.md"
JSON_FILE="${OUTPUT_DIR}/${PROJECT_NAME}_${TIMESTAMP}.json"

echo "=== Running Implicit Conversion Hazard Analyzer ==="
echo "Project:     $PROJECT_DIR"
echo "Threshold:   $THRESHOLD"
echo "Markdown:    $MARKDOWN_FILE"
echo "=================================================="

# Run on all source files listed in compile_commands.json
# We use -p to specify the build directory
cd "$PROJECT_DIR"

# Get list of source files
SOURCE_FILES=""
if [[ -f "compile_commands.json" ]]; then
    SOURCE_FILES=$(python3 -c "
import json, sys
try:
    with open('compile_commands.json') as f:
        db = json.load(f)
    files = set()
    for entry in db:
        f = entry['file']
        if f.endswith(('.c', '.cpp', '.cc', '.cxx', '.h', '.hpp')):
            files.add(f)
    for f in sorted(files):
        print(f)
except Exception:
    pass
" 2>/dev/null || echo "")
fi

if [[ -z "$SOURCE_FILES" ]]; then
    SOURCE_FILES=$(find . -maxdepth 4 -name "*.c" -o -name "*.cpp" -o -name "*.cc" | sed 's|^./||' | sort)
fi

if [[ -z "$SOURCE_FILES" ]]; then
    echo "No source files found in compile_commands.json"
    exit 1
fi

FILE_COUNT=$(echo "$SOURCE_FILES" | wc -l)
echo "Files to analyze: $FILE_COUNT"
echo ""

# Run the analyzer on all files in a single invocation to get a single unified report
echo "$SOURCE_FILES" | xargs "$ANALYZER" \
    -p "$PROJECT_DIR" \
    --risk-threshold "$THRESHOLD" \
    --markdown \
    -- \
    >> "$MARKDOWN_FILE" 2>&1 || true

# Also generate JSON output for correlation scripts
echo "$SOURCE_FILES" | xargs "$ANALYZER" \
    -p "$PROJECT_DIR" \
    --risk-threshold "$THRESHOLD" \
    --json \
    -- \
    >> "$JSON_FILE" 2>/dev/null || true

echo ""
echo "=== Analysis Complete ==="
echo "Markdown Dashboard: $MARKDOWN_FILE"
echo "JSON Results:       $JSON_FILE"
echo ""

# Quick summary
echo "=== Quick Summary ==="
grep -c "CRITICAL" "$MARKDOWN_FILE" 2>/dev/null && echo "  critical findings" || echo "  0 critical findings"
grep -c "HIGH" "$MARKDOWN_FILE" 2>/dev/null && echo "  high findings" || echo "  0 high findings"
grep -c "MEDIUM" "$MARKDOWN_FILE" 2>/dev/null && echo "  medium findings" || echo "  0 medium findings"
