#!/usr/bin/env bash
# Root build script for the Implicit Conversion Hazard Analyzer
set -euo pipefail

# ANSI color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}==================================================${NC}"
echo -e "${BLUE}   Implicit Conversion Hazard Analyzer Builder   ${NC}"
echo -e "${BLUE}==================================================${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -f "${SCRIPT_DIR}/scripts/build.sh" ]]; then
    echo -e "${RED}Error: scripts/build.sh not found!${NC}"
    exit 1
fi

chmod +x "${SCRIPT_DIR}/scripts/build.sh"
"${SCRIPT_DIR}/scripts/build.sh"

if [[ -f "${SCRIPT_DIR}/build/implicit-conversion-hazard" ]]; then
    echo -e "${GREEN}Build succeeded! Binary is at:${NC} build/implicit-conversion-hazard"
else
    echo -e "${RED}Build completed, but binary was not found in build/ directory.${NC}"
    exit 1
fi
