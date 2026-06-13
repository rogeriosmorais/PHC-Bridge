from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


SUCCESS_VERDICTS = {
    "PASS",
    "PASSED",
    "SUCCESS",
    "SUCCEEDED",
    "COMPLETED",
    "COMPLETE",
    "PRODUCT SUCCESS",
    "PRODUCT_SUCCESS",
    "PRODUCT_SUCCESS_CANDIDATE",
    "PRODUCT SUCCESS CANDIDATE",
}

FAILURE_VERDICTS = {
    "BLOCKED",
    "FAILED",
    "FAIL",
    "FAILED_VALIDATION",
    "INCONCLUSIVE",
}

DIAGNOSTIC_VERDICTS = {
    "DIAGNOSTIC",
}

ACTIVE_SEGMENT_STATES = {
    "ACTIVE",
    "COMPLETED",
    "DONE",
    "PASSED",
    "SUCCESS",
    "SUCCEEDED",
}

LOG_VERDICT_RE = re.compile(
    r"\b(?:RESULT|VERDICT|STRICT\s*VERDICT|STRICT_VERDICT|STRICTVERDICT)\b"
    r"\s*[:=]\s*\{?\s*"
    r"(PASS|PASSED|FAIL|FAILED|BLOCKED|SUCCESS|SUCCEEDED|CONTRADICTORY|MISSING(?:[_ ]EVIDENCE)?)\b",
    re.IGNORECASE,
)

TERMINAL_PROOF_PASS_LOG_RE = re.compile(
    r"\bPHYSANIMPROOF\b.*\bATTEMPTRESULT\b.*\bVERDICT\s*=\s*PASS\b",
    re.IGNORECASE,
)


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def normalize_text(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def normalize_upper(value: Any) -> str:
    return normalize_text(value).upper().replace("-", "_")


def truthy(value: Any) -> bool:
    return bool(value)


def latest_matching_file(
    directory: Path,
    pattern: str,
    attempt_uuid: Optional[str] = None,
) -> Optional[Path]:
    if not directory.exists():
        return None
    matches = list(directory.glob(pattern))
    if not matches:
        return None
    if attempt_uuid is not None:
        filtered_matches = []
        for path in matches:
            payload = load_json_file(path)
            if not payload:
                continue
            if normalize_text(payload.get("attempt_uuid")) == attempt_uuid:
                filtered_matches.append(path)
        matches = filtered_matches
        if not matches:
            return None
    return max(matches, key=lambda path: (path.stat().st_mtime, path.name))


def load_json_file(path: Optional[Path]) -> Optional[Dict[str, Any]]:
    if path is None or not path.exists():
        return None
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if isinstance(payload, dict):
        return payload
    return {"value": payload}


def read_text_lines(path: Optional[Path]) -> List[str]:
    if path is None or not path.exists():
        return []
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return [line.rstrip("\n") for line in handle]


def collect_log_claims(lines: Iterable[str]) -> List[str]:
    claims = []
    for line in lines:
        if LOG_VERDICT_RE.search(line):
            claims.append(line.strip())
    return claims


def filter_lines_for_attempt(lines: Iterable[str], attempt_uuid: Optional[str]) -> List[str]:
    requested_attempt_uuid = normalize_text(attempt_uuid)
    if not requested_attempt_uuid:
        return list(lines)
    return [line for line in lines if requested_attempt_uuid in line]


def format_key_value(name: str, value: Any) -> str:
    if isinstance(value, bool):
        rendered = "True" if value else "False"
    elif value is None:
        rendered = "None"
    else:
        rendered = str(value)
    return f"{name}={rendered}"


def summarize_terminal(terminal: Dict[str, Any]) -> List[str]:
    fields = [
        "attempt_uuid",
        "physical_continuity_validator_passed",
        "terminal_frame_artifact_captured",
        "terminal_reason_name",
        "terminal_reason",
        "pose_search_selected_animation_name",
        "policy_model_loaded",
    ]
    return [format_key_value(field, terminal.get(field)) for field in fields if field in terminal]


def summarize_summary(summary: Dict[str, Any]) -> List[str]:
    fields = [
        "attempt_uuid",
        "strict_verdict",
        "terminal_reason",
        "terminal_reason_name",
        "missing_evidence",
    ]
    lines = [format_key_value(field, summary.get(field)) for field in fields if field in summary]
    quality_flags = summary.get("quality_flags")
    if isinstance(quality_flags, dict):
        for key in (
            "assistance_truth_clean",
            "continuity_truth_clean",
            "support_truth_clean",
            "simulation_truth_clean",
            "terminal_failure",
            "artifact_log_contradiction",
            "missing_evidence",
        ):
            if key in quality_flags:
                lines.append(format_key_value(f"quality_flags.{key}", quality_flags.get(key)))
    return lines


def summarize_log(path: Optional[Path], lines: Sequence[str]) -> List[str]:
    if path is None:
        return ["no log file found"]
    claims = collect_log_claims(lines)
    if not claims:
        return [f"{path.name} | no verdict-like log lines found"]
    preview = claims[:5]
    result = [f"{path.name} | {len(claims)} claim-like line(s)"]
    result.extend(format_log_claim(claim) for claim in preview)
    return result


def format_log_claim(line: str) -> str:
    if TERMINAL_PROOF_PASS_LOG_RE.search(line):
        return f"terminal proof evidence: {line}"
    return line


def first_blocking_segment(summary: Optional[Dict[str, Any]]) -> str:
    if not summary:
        return "No evidence summary available."
    segments = summary.get("segments")
    if not isinstance(segments, list) or not segments:
        return "No segment data available."
    for segment in segments:
        if not isinstance(segment, dict):
            continue
        state = normalize_upper(segment.get("state"))
        if state in ACTIVE_SEGMENT_STATES:
            continue
        parts = [
            format_key_value("segment_name", segment.get("segment_name")),
            format_key_value("state", segment.get("state")),
        ]
        if segment.get("missing_required_fields"):
            parts.append(format_key_value("missing_required_fields", segment.get("missing_required_fields")))
        if segment.get("diagnostic_notes"):
            parts.append(format_key_value("diagnostic_notes", segment.get("diagnostic_notes")))
        return ", ".join(parts)
    return "No blocking segment found."


def summary_supports_success(summary: Optional[Dict[str, Any]]) -> bool:
    if not summary:
        return False
    verdict = normalize_upper(summary.get("strict_verdict"))
    if verdict not in SUCCESS_VERDICTS:
        return False
    quality_flags = summary.get("quality_flags")
    if isinstance(quality_flags, dict):
        if truthy(quality_flags.get("missing_evidence")):
            return False
        if truthy(quality_flags.get("terminal_failure")):
            return False
        if truthy(quality_flags.get("artifact_log_contradiction")):
            return False
    if truthy(summary.get("missing_evidence")):
        return False
    return True


def terminal_supports_success(terminal: Optional[Dict[str, Any]]) -> bool:
    if not terminal:
        return False
    if not truthy(terminal.get("physical_continuity_validator_passed")):
        return False
    if "terminal_frame_artifact_captured" in terminal and not truthy(terminal.get("terminal_frame_artifact_captured")):
        return False
    return True


def artifact_verdict(summary: Optional[Dict[str, Any]], terminal: Optional[Dict[str, Any]]) -> str:
    if summary is None or terminal is None:
        return "MISSING EVIDENCE"
    if truthy(summary.get("missing_evidence")):
        return "MISSING EVIDENCE"
    summary_verdict = normalize_upper(summary.get("strict_verdict"))
    if summary_verdict in FAILURE_VERDICTS:
        return summary_verdict.replace("_", " ")
    if summary_verdict in DIAGNOSTIC_VERDICTS:
        return summary_verdict
    if summary_supports_success(summary) and terminal_supports_success(terminal):
        return "PRODUCT SUCCESS"
    return "BLOCKED"


def has_log_pass_claim(log_claims: Sequence[str]) -> bool:
    for line in log_claims:
        if TERMINAL_PROOF_PASS_LOG_RE.search(line):
            continue
        upper = line.upper()
        if "PASS" in upper or "PASSED" in upper or "SUCCESS" in upper:
            return True
    return False


def has_log_fail_claim(log_claims: Sequence[str]) -> bool:
    for line in log_claims:
        upper = line.upper()
        if "FAIL" in upper or "FAILED" in upper or "BLOCKED" in upper:
            return True
    return False


def build_report(repo_root: Path, attempt_uuid: Optional[str] = None) -> List[str]:
    saved_root = repo_root / "PhysAnimUE5" / "Saved"
    proof_dir = saved_root / "PhysAnim" / "ProofArtifacts"
    summary_dir = saved_root / "PhysAnim" / "EvidenceSummaries"
    log_dir = saved_root / "Logs"

    requested_attempt_uuid = normalize_text(attempt_uuid) or None

    terminal_path = latest_matching_file(proof_dir, "*_terminal.json", requested_attempt_uuid)
    summary_path = latest_matching_file(summary_dir, "*_evidence_summary.json", requested_attempt_uuid)
    log_path = latest_matching_file(log_dir, "*.log")

    terminal = load_json_file(terminal_path)
    summary = load_json_file(summary_path)
    log_lines = filter_lines_for_attempt(read_text_lines(log_path), requested_attempt_uuid)
    log_claims = collect_log_claims(log_lines)

    missing_items = []
    if terminal_path is None:
        if requested_attempt_uuid is None:
            missing_items.append("terminal artifact")
        else:
            missing_items.append(f"terminal artifact for attempt_uuid={requested_attempt_uuid}")
    if summary_path is None:
        if requested_attempt_uuid is None:
            missing_items.append("evidence summary")
        else:
            missing_items.append(f"evidence summary for attempt_uuid={requested_attempt_uuid}")

    explicit_missing = truthy(summary.get("missing_evidence")) if summary else False
    if explicit_missing and "evidence summary" not in missing_items:
        missing_items.append("evidence summary flagged missing_evidence=true")

    summary_success = summary_supports_success(summary)
    terminal_success = terminal_supports_success(terminal)

    contradiction_items = []
    if log_path is not None and has_log_pass_claim(log_claims) and not (summary_success and terminal_success):
        contradiction_items.append("log claims PASS/PASSED/SUCCESS, but artifacts do not support product success")
    if log_path is not None and has_log_fail_claim(log_claims) and summary_success and terminal_success:
        contradiction_items.append("log claims failure, but artifacts support product success")
    if summary and terminal:
        if normalize_upper(summary.get("strict_verdict")) in SUCCESS_VERDICTS and not terminal_success:
            contradiction_items.append("summary strict verdict implies success, but terminal artifact does not support success")
        if normalize_upper(summary.get("strict_verdict")) in FAILURE_VERDICTS and terminal_success and has_log_pass_claim(log_claims):
            contradiction_items.append("log PASS contradicts blocked summary verdict")

    actual_lines: List[str] = []
    if terminal_path is not None and terminal is not None:
        actual_lines.append(f"{terminal_path.name} | " + " | ".join(summarize_terminal(terminal)))
    else:
        actual_lines.append("terminal artifact missing")
    if summary_path is not None and summary is not None:
        actual_lines.append(f"{summary_path.name} | " + " | ".join(summarize_summary(summary)))
    else:
        actual_lines.append("evidence summary missing")

    weak_lines: List[str] = []
    if log_path is not None:
        weak_lines.extend(summarize_log(log_path, log_lines))
    else:
        weak_lines.append("no log file found")
    if not summary_success:
        weak_lines.append("summary strict verdict does not support product success")
    if terminal is not None and not terminal_success:
        weak_lines.append("terminal artifact does not satisfy the success support check")

    missing_lines: List[str] = []
    if missing_items:
        for item in missing_items:
            missing_lines.append(f"- {item}")
    else:
        missing_lines.append("- none")
    if explicit_missing:
        missing_lines.append("- summary marked missing_evidence=true, so it cannot pass")

    segment_lines = []
    if summary:
        segments = summary.get("segments")
        if isinstance(segments, list):
            for segment in segments:
                if isinstance(segment, dict):
                    name = segment.get("segment_name")
                    state = segment.get("state")
                    segment_lines.append(f"- {name}: {state}")
    if not segment_lines:
        segment_lines = ["- No segment data available."]

    if missing_items:
        verdict = "MISSING EVIDENCE"
    elif contradiction_items:
        verdict = "CONTRADICTORY"
    else:
        verdict = artifact_verdict(summary, terminal)

    contradiction_lines = [f"- {line}" for line in contradiction_items] if contradiction_items else ["- none"]
    lines = [
        "Actual Evidence",
        *[f"- {line}" for line in actual_lines],
        "",
        "Weak Evidence",
        *[f"- {line}" for line in weak_lines],
        "",
        "Contradictions",
        *contradiction_lines,
        "",
        "Missing Evidence",
        *missing_lines,
        "",
        "Segment Status",
        *segment_lines,
        "",
        "Verdict",
        f"- {verdict}",
    ]
    return lines


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Collect repository evidence from saved UE artifacts.")
    parser.add_argument(
        "--repo-root",
        default=None,
        help="Repository root. Defaults to the parent directory of this script.",
    )
    parser.add_argument(
        "--attempt-uuid",
        default=None,
        help="Restrict evidence collection to a single attempt UUID.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve() if args.repo_root else repo_root_from_script()
    report = build_report(repo_root, attempt_uuid=args.attempt_uuid)
    print("\n".join(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
