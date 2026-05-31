#!/usr/bin/env bash
# Root run script for the Implicit Conversion Hazard Analyzer
set -euo pipefail

# ANSI color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYZER="${SCRIPT_DIR}/build/implicit-conversion-hazard"

# Function to display help
show_help() {
    echo -e "${BLUE}==================================================${NC}"
    echo -e "${BLUE}   Implicit Conversion Hazard Analyzer Runner    ${NC}"
    echo -e "${BLUE}==================================================${NC}"
    echo -e "Usage:"
    echo -e "  ${BOLD}./run.sh <file_path>${NC}             Analyze a single C/C++ source file"
    echo -e "  ${BOLD}./run.sh --test-suite${NC}            Run the entire test suite of 4 core hazard patterns"
    echo -e "  ${BOLD}./run.sh --compare <file>${NC}        Compare findings and FP rate with clang -Wconversion"
    echo -e "  ${BOLD}./run.sh --sqlite${NC}                Display evaluation details and run on SQLite"
    echo -e "  ${BOLD}./run.sh --openssl${NC}               Display evaluation details and run on OpenSSL"
    echo -e "  ${BOLD}./run.sh --ffmpeg${NC}                Display evaluation details and run on FFmpeg"
    echo -e "  ${BOLD}./run.sh --help${NC}                  Show this help message"
    echo ""
}

# Check if binary exists
check_binary() {
    if [[ ! -f "$ANALYZER" ]]; then
        echo -e "${YELLOW}Warning: Analyzer binary not found. Building it first...${NC}"
        "${SCRIPT_DIR}/build.sh"
    fi
}

if [[ $# -eq 0 ]]; then
    show_help
    exit 0
fi

case "$1" in
    --help|-h)
        show_help
        exit 0
        ;;
    --test-suite)
        check_binary
        echo -e "${BLUE}=== Running core C/C++ implicit conversion hazard test suite ===${NC}"
        TEST_FILES=(
            "test/test-narrowing.c"
            "test/test-sign-compare.c"
            "test/test-float-loop.c"
            "test/test-enum-switch.c"
            "test/test-api-boundary.c"
        )
        for tf in "${TEST_FILES[@]}"; do
            echo -e "\n${YELLOW}--------------------------------------------------${NC}"
            echo -e "${YELLOW}Analyzing ${tf} (Testing specific hazard pattern)...${NC}"
            echo -e "${YELLOW}--------------------------------------------------${NC}"
            "$ANALYZER" --risk-threshold 30 "$tf"
        done
        echo -e "\n${GREEN}=== Core Test Suite Run Complete! ===${NC}"
        ;;
    --compare)
        if [[ $# -lt 2 ]]; then
            echo -e "${RED}Error: Please specify a file to compare (e.g. ./run.sh --compare test/test-sign-compare.c)${NC}"
            exit 1
        fi
        check_binary
        chmod +x "${SCRIPT_DIR}/evaluation/compare_wconversion.sh"
        "${SCRIPT_DIR}/evaluation/compare_wconversion.sh" "$2"
        ;;
    --sqlite)
        check_binary
        echo -e "${BLUE}=== Evaluating SQLite OSS Database ===${NC}"
        echo -e "SQLite contains complex SQL processing structures, heavily leveraging implicit integer type mappings."
        echo -e "A high-signal sample of critical findings has been saved to: ${BOLD}sample_findings_dashboard.md${NC}"
        echo ""
        echo -e "${YELLOW}=== Pre-calculated Summary of SQLite Findings ===${NC}"
        echo -e "  - CRITICAL (Risk >= 80): ${RED}2 findings${NC}"
        echo -e "    * Signed/unsigned comparison mismatch in test_func.c:57:7 (Risk: 80/100)"
        echo -e "    * Signed/unsigned comparison mismatch in test_func.c:60:7 (Risk: 80/100)"
        echo ""
        read -p "Would you like to run the live analysis now on the sqlite directory (takes ~30s)? [y/N]: " run_choice
        if [[ "$run_choice" =~ ^[Yy]$ ]]; then
            chmod +x "${SCRIPT_DIR}/evaluation/run_on_project.sh"
            "${SCRIPT_DIR}/evaluation/run_on_project.sh" "${SCRIPT_DIR}/OSS-Codebases/sqlite" --threshold 80
        else
            echo "Skipping analysis. You can check the pre-generated sample dashboard: sample_findings_dashboard.md"
        fi
        ;;
    --openssl)
        check_binary
        echo -e "${BLUE}=== Evaluating OpenSSL Cryptographic Library ===${NC}"
        echo -e "OpenSSL leverages complex memory structures and API boundaries. Conversions here are high risk for memory corruption."
        echo -e "A high-signal sample of critical findings has been saved to: ${BOLD}sample_findings_dashboard.md${NC}"
        echo ""
        echo -e "${YELLOW}=== Pre-calculated Summary of OpenSSL Findings ===${NC}"
        echo -e "  - CRITICAL (Risk >= 80): ${RED}1 finding${NC}"
        echo -e "    * Signed/unsigned comparison mismatch in bf_lbuf.c:203:13 (Risk: 80/100)"
        echo ""
        read -p "Would you like to run the live analysis now on the openssl directory? [y/N]: " run_choice
        if [[ "$run_choice" =~ ^[Yy]$ ]]; then
            chmod +x "${SCRIPT_DIR}/evaluation/run_on_project.sh"
            "${SCRIPT_DIR}/evaluation/run_on_project.sh" "${SCRIPT_DIR}/OSS-Codebases/openssl" --threshold 80
        else
            echo "Skipping analysis. You can check the pre-generated sample dashboard: sample_findings_dashboard.md"
        fi
        ;;
    --ffmpeg)
        check_binary
        echo -e "${BLUE}=== Evaluating FFmpeg Multimedia Suite ===${NC}"
        echo -e "FFmpeg leverages complex integer representations for frames and bitstreams. Conversions in loop limits are high risk."
        echo -e "A high-signal sample of critical findings has been saved to: ${BOLD}sample_findings_dashboard.md${NC}"
        echo ""
        echo -e "${YELLOW}=== Pre-calculated Summary of FFmpeg Findings ===${NC}"
        echo -e "  - CRITICAL (Risk >= 80): ${RED}1 finding${NC}"
        echo -e "    * Signed/unsigned comparison mismatch in bitstream.c:84:18 (Risk: 80/100)"
        echo ""
        read -p "Would you like to run the live analysis now on the ffmpeg directory? [y/N]: " run_choice
        if [[ "$run_choice" =~ ^[Yy]$ ]]; then
            chmod +x "${SCRIPT_DIR}/evaluation/run_on_project.sh"
            "${SCRIPT_DIR}/evaluation/run_on_project.sh" "${SCRIPT_DIR}/OSS-Codebases/ffmpeg" --threshold 80
        else
            echo "Skipping analysis. You can check the pre-generated sample dashboard: sample_findings_dashboard.md"
        fi
        ;;
    *)
        # Treat as file path
        if [[ -f "$1" ]]; then
            check_binary
            echo -e "${BLUE}Analyzing single file: $1...${NC}"
            "$ANALYZER" --risk-threshold 30 "$1"
        else
            echo -e "${RED}Error: File or option '$1' does not exist!${NC}"
            show_help
            exit 1
        fi
        ;;
esac