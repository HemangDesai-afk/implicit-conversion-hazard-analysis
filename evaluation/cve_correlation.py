#!/usr/bin/env python3
"""
CVE Correlation Script for Implicit Conversion Hazard Analyzer

Cross-references analyzer findings against known CVEs caused by
implicit conversions in the analyzed project.

Usage:
    python3 cve_correlation.py <findings_json> <cve_database.json>

The CVE database is a JSON file with known CVEs:
{
    "cves": [
        {
            "id": "CVE-2015-7036",
            "project": "sqlite",
            "description": "Integer overflow in sqlite3MallocSize",
            "pattern": "integer overflow|truncation|narrowing",
            "affected_file": "src/malloc.c",
            "line_hint": 200,
            "type": "integer_truncation"
        }
    ]
}
"""

import json
import sys
import re
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Finding:
    file: str
    line: int
    source_type: str
    target_type: str
    conversion_kind: str
    risk_score: int
    risk_level: str
    context: str
    fix_suggestion: str


@dataclass
class CVE:
    id: str
    project: str
    description: str
    pattern: str
    affected_file: str
    line_hint: Optional[int]
    type: str


@dataclass
class CorrelationResult:
    cve: CVE
    matched_finding: Optional[Finding] = None
    match_score: float = 0.0
    match_reason: str = ""


def load_findings(json_path: str) -> list[Finding]:
    with open(json_path) as f:
        data = json.load(f)
    return [Finding(**item) for item in data]


def load_cves(json_path: str) -> list[CVE]:
    with open(json_path) as f:
        data = json.load(f)
    return [CVE(**item) for item in data["cves"]]


def file_matches(finding_file: str, cve_file: str) -> bool:
    """Check if a finding file path matches a CVE file path."""
    # Normalize paths
    f_parts = Path(finding_file).parts
    c_parts = Path(cve_file).parts

    # Check if the CVE file is a substring of the finding file
    # (finding may have full path, CVE may have relative path)
    f_normalized = "/".join(f_parts[-len(c_parts):]) if len(f_parts) >= len(c_parts) else ""
    c_normalized = "/".join(c_parts)

    return f_normalized == c_normalized or cve_file in finding_file or finding_file.endswith(cve_file)


def line_proximity(finding_line: int, cve_line_hint: Optional[int], tolerance: int = 50) -> float:
    """Score how close the finding line is to the CVE line hint."""
    if cve_line_hint is None:
        return 0.5  # neutral if no hint
    distance = abs(finding_line - cve_line_hint)
    if distance == 0:
        return 1.0
    if distance <= tolerance:
        return 1.0 - (distance / tolerance)
    return 0.0


def pattern_matches(finding_context: str, cve_pattern: str) -> bool:
    """Check if the CVE pattern matches the finding context."""
    pattern = re.compile(cve_pattern, re.IGNORECASE)
    return bool(pattern.search(finding_context))


def correlate(findings: list[Finding], cves: list[CVE]) -> list[CorrelationResult]:
    """Find correlations between CVEs and analyzer findings."""
    results = []

    for cve in cves:
        best_match: Optional[Finding] = None
        best_score = 0.0
        best_reason = ""

        for finding in findings:
            score = 0.0
            reasons = []

            # File match (required for strong correlation)
            file_match = file_matches(finding.file, cve.affected_file)
            if file_match:
                score += 0.4
                reasons.append("file_match")

            # Line proximity
            line_score = line_proximity(finding.line, cve.line_hint)
            score += line_score * 0.3
            if line_score > 0.3:
                reasons.append(f"line_proximity({line_score:.2f})")

            # Pattern match in context
            if pattern_matches(finding.context, cve.pattern):
                score += 0.2
                reasons.append("pattern_match")

            # Conversion type match
            if cve.type == "integer_truncation" and "Integral" in finding.conversion_kind:
                score += 0.1
                reasons.append("type_match:truncation")
            elif cve.type == "sign_mismatch" and "sign" in finding.context.lower():
                score += 0.1
                reasons.append("type_match:sign")
            elif cve.type == "float_to_int" and "FloatingToIntegral" in finding.conversion_kind:
                score += 0.1
                reasons.append("type_match:float_to_int")

            # Risk score bonus (higher risk findings more likely to be real CVEs)
            score += (finding.risk_score / 100.0) * 0.1

            if score > best_score and file_match:
                best_score = score
                best_match = finding
                best_reason = ", ".join(reasons)

        results.append(CorrelationResult(
            cve=cve,
            matched_finding=best_match,
            match_score=best_score,
            match_reason=best_reason
        ))

    return results


def print_report(results: list[CorrelationResult], findings: list[Finding]):
    """Print the correlation report."""
    print("=" * 60)
    print("  CVE Correlation Report")
    print("=" * 60)
    print()

    matched = [r for r in results if r.matched_finding is not None and r.match_score >= 0.5]
    partial = [r for r in results if r.matched_finding is not None and 0.3 <= r.match_score < 0.5]
    missed = [r for r in results if r.matched_finding is None or r.match_score < 0.3]

    print(f"Total CVEs analyzed:     {len(results)}")
    print(f"Matched (score >= 0.5):  {len(matched)}")
    print(f"Partial (0.3-0.5):       {len(partial)}")
    print(f"Missed (< 0.3):          {len(missed)}")
    print()

    if matched:
        print("-" * 60)
        print("  MATCHED CVEs")
        print("-" * 60)
        for r in matched:
            print(f"  {r.cve.id}: {r.cve.description}")
            print(f"    Score:  {r.match_score:.2f}")
            print(f"    Reason: {r.match_reason}")
            f = r.matched_finding
            print(f"    Found:  {f.file}:{f.line} ({f.source_type} -> {f.target_type})")
            print()

    if partial:
        print("-" * 60)
        print("  PARTIAL MATCHES")
        print("-" * 60)
        for r in partial:
            print(f"  {r.cve.id}: {r.cve.description}")
            print(f"    Score:  {r.match_score:.2f}")
            print(f"    Reason: {r.match_reason}")
            print()

    if missed:
        print("-" * 60)
        print("  MISSED CVEs (not detected or low confidence)")
        print("-" * 60)
        for r in missed:
            print(f"  {r.cve.id}: {r.cve.description}")
            print(f"    Score:  {r.match_score:.2f}")
            print(f"    Pattern: {r.cve.pattern}")
            print(f"    File:    {r.cve.affected_file}")
            print()

    # Summary metrics
    if results:
        recall = len(matched) / len(results)
        print(f"Recall (matched/total): {recall:.1%}")


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 cve_correlation.py <findings.json> <cve_database.json>")
        print()
        print("Example:")
        print("  python3 cve_correlation.py output/sqlite_findings.json cves/sqlite_cves.json")
        sys.exit(1)

    findings_file = sys.argv[1]
    cves_file = sys.argv[2]

    findings = load_findings(findings_file)
    cves = load_cves(cves_file)

    results = correlate(findings, cves)
    print_report(results, findings)


if __name__ == "__main__":
    main()
