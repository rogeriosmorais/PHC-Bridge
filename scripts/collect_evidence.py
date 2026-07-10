from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


FINAL_DIAGNOSTIC = "DIAGNOSTIC"
FINAL_BLOCKED = "BLOCKED"
FINAL_CONTRADICTORY = "CONTRADICTORY"
FINAL_INSUFFICIENT = "INSUFFICIENT_EVIDENCE"

PRODUCER_OBSERVED_CLASSIFICATION = "DIAGNOSTIC_ALL_SIGNALS_OBSERVED"

ACTIVE_SEGMENT_STATES = {
    "ACTIVE",
    "COMPLETED",
    "DONE",
    "PASSED",
    "SUCCESS",
    "SUCCEEDED",
}

ORDERED_SEGMENTS = (
    "PoseSearch",
    "PhcPolicy",
    "PhysicsControl",
    "Chaos",
    "RendererFacingMotion",
)

LOG_VERDICT_RE = re.compile(
    r"\b(?:RESULT|VERDICT|STRICT\s*VERDICT|STRICT_VERDICT|STRICTVERDICT)\b"
    r"\s*[:=]\s*\{?\s*"
    r"(PASS|PASSED|FAIL|FAILED|BLOCKED|SUCCESS|SUCCEEDED|CONTRADICTORY|"
    r"INSUFFICIENT_EVIDENCE|MISSING(?:[_ ]EVIDENCE)?)\b",
    re.IGNORECASE,
)

ATTEMPT_CAPTURE_RE = re.compile(
    r"\bPhysAnimProof:\s*AttemptCapture\s+"
    r"uuid=(?P<uuid>\S+)\s+"
    r"capture=(?P<capture>COMPLETED|TERMINATED)\s+"
    r"active_standing_duration=(?P<duration>[+-]?(?:\d+(?:\.\d*)?|\.\d+))\s+"
    r"terminal_reason=(?P<reason>[A-Za-z0-9_]+)\b",
    re.IGNORECASE,
)

ADDITIONAL_REQUIRED_TERMINAL_FIELDS = (
    "emitter_attempt_uuid",
    "terminal_frame_artifact_captured",
    "standing_window_sample_count",
    "standing_window_max_delta_sec",
)

NONEMPTY_TERMINAL_FIELDS = {
    "schema_version",
    "attempt_uuid",
    "emitter_attempt_uuid",
    "attempt_nonce",
    "captured_at_utc",
    "source_commit",
    "final_runtime_outcome",
    "terminal_reason_name",
}


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


def get_float(data: Dict[str, Any], key: str) -> Optional[float]:
    value = data.get(key)
    if isinstance(value, bool) or value is None:
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def load_json_file(path: Optional[Path]) -> Optional[Dict[str, Any]]:
    if path is None or not path.exists():
        return None
    try:
        with path.open("r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return None
    return payload if isinstance(payload, dict) else None


def latest_matching_file(
    directory: Path,
    pattern: str,
    attempt_uuid: Optional[str] = None,
) -> Optional[Path]:
    if not directory.exists():
        return None

    matches = list(directory.glob(pattern))
    requested_attempt_uuid = normalize_text(attempt_uuid)
    if requested_attempt_uuid:
        matches = [
            path
            for path in matches
            if normalize_text((load_json_file(path) or {}).get("attempt_uuid"))
            == requested_attempt_uuid
        ]

    if not matches:
        return None
    return max(matches, key=lambda path: (path.stat().st_mtime, path.name))


def read_text_lines(path: Optional[Path]) -> List[str]:
    if path is None or not path.exists():
        return []
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return [line.rstrip("\n") for line in handle]


def filter_lines_for_attempt(lines: Iterable[str], attempt_uuid: Optional[str]) -> List[str]:
    requested_attempt_uuid = normalize_text(attempt_uuid)
    if not requested_attempt_uuid:
        return list(lines)
    return [line for line in lines if requested_attempt_uuid in line]


def collect_log_claims(lines: Iterable[str]) -> List[str]:
    return [
        line.strip()
        for line in lines
        if ATTEMPT_CAPTURE_RE.search(line) or LOG_VERDICT_RE.search(line)
    ]


def load_required_terminal_fields() -> Tuple[List[str], Optional[str]]:
    contract_path = repo_root_from_script() / "product-gates" / "standing-v0.v1.json"
    contract = load_json_file(contract_path)
    if contract is None:
        return [], f"locked product contract unavailable: {contract_path}"

    criteria = contract.get("criteria")
    if not isinstance(criteria, list):
        return [], f"locked product contract has no criteria list: {contract_path}"

    fields: List[str] = []
    for criterion in criteria:
        if not isinstance(criterion, dict):
            continue
        fact = normalize_text(criterion.get("fact"))
        if fact and fact not in fields:
            fields.append(fact)
    for fact in ADDITIONAL_REQUIRED_TERMINAL_FIELDS:
        if fact not in fields:
            fields.append(fact)
    return fields, None


def missing_terminal_fields(terminal: Dict[str, Any], required_fields: Sequence[str]) -> List[str]:
    missing: List[str] = []
    for field in required_fields:
        if field not in terminal or terminal.get(field) is None:
            missing.append(field)
        elif field in NONEMPTY_TERMINAL_FIELDS and not normalize_text(terminal.get(field)):
            missing.append(field)
    return missing


def missing_summary_structure(summary: Dict[str, Any]) -> List[str]:
    missing: List[str] = []
    if not normalize_text(summary.get("attempt_uuid")):
        missing.append("attempt_uuid")
    if "diagnostic_classification" not in summary:
        missing.append("diagnostic_classification")
    if not isinstance(summary.get("quality_flags"), dict):
        missing.append("quality_flags")

    segments = summary.get("segments")
    if not isinstance(segments, list):
        missing.append("segments")
    else:
        segment_names = {
            normalize_text(segment.get("segment_name"))
            for segment in segments
            if isinstance(segment, dict)
        }
        for segment_name in ORDERED_SEGMENTS:
            if segment_name not in segment_names:
                missing.append(f"segments.{segment_name}")
    return missing


def format_key_value(name: str, value: Any) -> str:
    if isinstance(value, bool):
        rendered = "True" if value else "False"
    elif value is None:
        rendered = "None"
    else:
        rendered = str(value)
    return f"{name}={rendered}"


def summarize_terminal(terminal: Dict[str, Any]) -> List[str]:
    fields = (
        "attempt_uuid",
        "emitter_attempt_uuid",
        "schema_version",
        "final_runtime_outcome",
        "terminal_frame_artifact_captured",
        "terminal_reason_name",
        "terminal_reason",
        "balance_active_standing_continuous_sec",
        "standing_window_sample_count",
        "standing_window_max_delta_sec",
        "setup_override_count",
    )
    return [format_key_value(field, terminal.get(field)) for field in fields if field in terminal]


def summarize_stability_metrics(terminal: Dict[str, Any]) -> List[str]:
    metrics = (
        ("Hold Duration", "hold_duration_sec"),
        ("Standing Window", "balance_active_standing_continuous_sec"),
        ("Standing Samples", "standing_window_sample_count"),
        ("Standing Max Delta", "standing_window_max_delta_sec"),
        ("Simulating Bodies", "runtime_simulating_body_count"),
        ("Max Root Tilt", "max_root_tilt_deg"),
        ("Peak Angular Speed", "peak_angular_speed_deg_per_sec"),
        ("Support Churn", "support_churn_hz"),
        ("Proxy Drift", "proxy_outside_hull_duration_ms"),
        ("Terminal Reason", "terminal_reason_name"),
        ("Policy Inference Success", "policy_inference_success_count"),
        ("Thigh Net Work", "thigh_net_work"),
    )
    return [f"{label}={terminal.get(key)}" for label, key in metrics if key in terminal]


def summarize_summary(summary: Dict[str, Any]) -> List[str]:
    fields = (
        "attempt_uuid",
        "diagnostic_classification",
        "terminal_reason",
        "terminal_reason_name",
        "missing_evidence",
    )
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
        return [f"{path.name} | no AttemptCapture or verdict-like lines found"]
    result = [f"{path.name} | {len(claims)} factual/claim line(s)"]
    result.extend(claims[:5])
    return result


def terminal_has_runtime_failure(terminal: Dict[str, Any]) -> bool:
    terminal_reason = terminal.get("terminal_reason")
    terminal_reason_name = normalize_upper(terminal.get("terminal_reason_name"))
    final_outcome = normalize_text(terminal.get("final_runtime_outcome"))
    return (
        terminal_reason not in (0, "0")
        or terminal_reason_name not in {"NONE", "NULLPTR"}
        or final_outcome != "BalanceActive_Standing"
        or terminal.get("terminal_frame_artifact_captured") is not True
    )


def validate_attempt_capture(
    log_lines: Sequence[str],
    attempt_uuid: str,
    terminal: Dict[str, Any],
) -> Tuple[List[str], List[str]]:
    captures = []
    for line in log_lines:
        match = ATTEMPT_CAPTURE_RE.search(line)
        if match and normalize_text(match.group("uuid")) == attempt_uuid:
            captures.append(match)

    missing: List[str] = []
    contradictions: List[str] = []
    if not captures:
        missing.append(f"production AttemptCapture log for attempt_uuid={attempt_uuid}")
        return missing, contradictions

    if len(captures) > 1:
        contradictions.append(f"multiple production AttemptCapture lines found for attempt_uuid={attempt_uuid}")
        return missing, contradictions

    capture = captures[0]
    runtime_failed = terminal_has_runtime_failure(terminal)
    expected_capture = "TERMINATED" if runtime_failed else "COMPLETED"
    actual_capture = normalize_upper(capture.group("capture"))
    if actual_capture != expected_capture:
        contradictions.append(
            f"log reports capture={actual_capture} but terminal facts require capture={expected_capture}"
        )

    artifact_reason = normalize_upper(terminal.get("terminal_reason_name"))
    log_reason = normalize_upper(capture.group("reason"))
    if log_reason != artifact_reason:
        contradictions.append(
            f"log reports terminal_reason={log_reason} but artifact reports terminal_reason={artifact_reason}"
        )

    artifact_duration = get_float(terminal, "balance_active_standing_continuous_sec")
    log_duration = float(capture.group("duration"))
    if artifact_duration is not None and not math.isclose(log_duration, artifact_duration, abs_tol=0.001):
        contradictions.append(
            "log active_standing_duration="
            f"{log_duration:.3f} but artifact balance_active_standing_continuous_sec={artifact_duration:.3f}"
        )
    return missing, contradictions


def build_report_result(
    repo_root: Path,
    attempt_uuid: Optional[str] = None,
) -> Tuple[List[str], str]:
    test_results_dir = repo_root / "test-results"
    proof_dir = test_results_dir / "proof-artifacts"
    summary_dir = test_results_dir / "evidence-summaries"
    saved_log_dir = repo_root / "PhysAnimUE5" / "Saved" / "Logs"
    test_log_dir = test_results_dir / "logs"

    requested_attempt_uuid = normalize_text(attempt_uuid) or None
    terminal_path = latest_matching_file(proof_dir, "*_terminal.json", requested_attempt_uuid)
    terminal = load_json_file(terminal_path)
    resolved_attempt_uuid = requested_attempt_uuid
    if not resolved_attempt_uuid and terminal is not None:
        resolved_attempt_uuid = normalize_text(terminal.get("attempt_uuid")) or None

    # A summary is selected only after the terminal attempt is fixed. Independent
    # "latest" selection would merge two experiments into a synthetic result.
    summary_path = (
        latest_matching_file(summary_dir, "*_evidence_summary.json", resolved_attempt_uuid)
        if resolved_attempt_uuid
        else None
    )
    summary = load_json_file(summary_path)

    log_path: Optional[Path] = None
    if resolved_attempt_uuid:
        attempt_log = test_log_dir / f"{resolved_attempt_uuid}.log"
        if attempt_log.exists():
            log_path = attempt_log
    if log_path is None:
        log_path = latest_matching_file(test_log_dir, "*.log")
    if log_path is None:
        log_path = latest_matching_file(saved_log_dir, "*.log")

    log_lines = filter_lines_for_attempt(read_text_lines(log_path), resolved_attempt_uuid)

    missing_items: List[str] = []
    contradiction_items: List[str] = []

    if terminal_path is None or terminal is None:
        suffix = f" for attempt_uuid={requested_attempt_uuid}" if requested_attempt_uuid else ""
        missing_items.append(f"terminal artifact{suffix}")

    if summary_path is None or summary is None:
        suffix = f" for attempt_uuid={resolved_attempt_uuid}" if resolved_attempt_uuid else ""
        missing_items.append(f"evidence summary{suffix}")

    required_fields, contract_error = load_required_terminal_fields()
    if contract_error:
        missing_items.append(contract_error)

    if terminal is not None:
        for field in missing_terminal_fields(terminal, required_fields):
            missing_items.append(f"missing mandatory terminal field: {field}")

        artifact_attempt_uuid = normalize_text(terminal.get("attempt_uuid"))
        emitter_attempt_uuid = normalize_text(terminal.get("emitter_attempt_uuid"))
        if artifact_attempt_uuid and emitter_attempt_uuid and artifact_attempt_uuid != emitter_attempt_uuid:
            contradiction_items.append(
                f"artifact attempt_uuid={artifact_attempt_uuid} but emitter_attempt_uuid={emitter_attempt_uuid}"
            )
        if requested_attempt_uuid and artifact_attempt_uuid != requested_attempt_uuid:
            contradiction_items.append(
                f"requested attempt_uuid={requested_attempt_uuid} but artifact reports {artifact_attempt_uuid}"
            )

    if summary is not None:
        for field in missing_summary_structure(summary):
            missing_items.append(f"missing mandatory summary field: {field}")
        if truthy(summary.get("missing_evidence")):
            missing_items.append("evidence summary flagged missing_evidence=true")
        quality_flags = summary.get("quality_flags")
        if isinstance(quality_flags, dict) and truthy(quality_flags.get("missing_evidence")):
            missing_items.append("evidence summary quality_flags.missing_evidence=true")

    if terminal is not None and summary is not None:
        terminal_uuid = normalize_text(terminal.get("attempt_uuid"))
        summary_uuid = normalize_text(summary.get("attempt_uuid"))
        if terminal_uuid and summary_uuid and terminal_uuid != summary_uuid:
            contradiction_items.append(
                f"terminal attempt_uuid={terminal_uuid} but summary attempt_uuid={summary_uuid}"
            )

        producer_classification = normalize_upper(summary.get("diagnostic_classification"))
        if producer_classification == PRODUCER_OBSERVED_CLASSIFICATION and terminal_has_runtime_failure(terminal):
            contradiction_items.append(
                "producer summary claims all signals observed but terminal facts report runtime failure"
            )

        quality_flags = summary.get("quality_flags")
        if isinstance(quality_flags, dict) and "terminal_failure" in quality_flags:
            claimed_failure = truthy(quality_flags.get("terminal_failure"))
            factual_failure = terminal_has_runtime_failure(terminal)
            if claimed_failure != factual_failure:
                contradiction_items.append(
                    "summary quality_flags.terminal_failure disagrees with terminal facts"
                )

    if terminal is not None and resolved_attempt_uuid:
        capture_missing, capture_contradictions = validate_attempt_capture(
            log_lines,
            resolved_attempt_uuid,
            terminal,
        )
        missing_items.extend(capture_missing)
        contradiction_items.extend(capture_contradictions)

    # Preserve order while keeping the report readable when two checks find the same gap.
    missing_items = list(dict.fromkeys(missing_items))
    contradiction_items = list(dict.fromkeys(contradiction_items))

    if contradiction_items:
        classification = FINAL_CONTRADICTORY
    elif missing_items:
        classification = FINAL_INSUFFICIENT
    elif terminal is not None and terminal_has_runtime_failure(terminal):
        classification = FINAL_BLOCKED
    else:
        classification = FINAL_DIAGNOSTIC

    actual_lines: List[str] = []
    if terminal_path is not None and terminal is not None:
        actual_lines.append(f"{terminal_path.name} | " + " | ".join(summarize_terminal(terminal)))
    else:
        actual_lines.append("terminal artifact missing")
    if summary_path is not None and summary is not None:
        actual_lines.append(f"{summary_path.name} | " + " | ".join(summarize_summary(summary)))
    else:
        actual_lines.append("evidence summary missing")

    weak_lines = summarize_log(log_path, log_lines)
    if summary is not None:
        producer_classification = normalize_text(summary.get("diagnostic_classification"))
        if producer_classification:
            weak_lines.append(
                "producer diagnostic_classification is reported for context and is not used as this collector's classification"
            )

    segment_lines: List[str] = []
    blocking_segment: Optional[str] = None
    if summary is not None and isinstance(summary.get("segments"), list):
        segment_map = {
            normalize_text(segment.get("segment_name")): normalize_text(segment.get("state"))
            for segment in summary["segments"]
            if isinstance(segment, dict)
        }
        for name in ORDERED_SEGMENTS:
            if name not in segment_map:
                continue
            state = segment_map[name]
            if normalize_upper(state) not in ACTIVE_SEGMENT_STATES:
                segment_lines.append(f"- segment_name={name}, state={state}")
                if blocking_segment is None:
                    blocking_segment = name
    if not segment_lines:
        segment_lines = [
            "- All reported segments are Active."
            if summary is not None and summary.get("segments")
            else "- No segment data available."
        ]

    blocker_line = (
        f"Next Blocking Segment: segment_name={blocking_segment}"
        if blocking_segment
        else "No blocking segment identified."
    )

    lines = [
        "Authority",
        "- LOCAL_DIAGNOSTIC_ONLY; this report cannot satisfy the external product gate.",
        "- Runtime facts and external oracle/receipt must be authored by separate trust domains.",
        "",
        "Actual Evidence",
        *[f"- {line}" for line in actual_lines],
        "",
        "Stability Metrics",
        *[f"- {line}" for line in (summarize_stability_metrics(terminal) if terminal else ["no terminal artifact found"])],
        "",
        "Weak Evidence",
        *[f"- {line}" for line in weak_lines],
        "",
        "Contradictions",
        *([f"- {line}" for line in contradiction_items] if contradiction_items else ["- none"]),
        "",
        "Missing Evidence",
        *([f"- {line}" for line in missing_items] if missing_items else ["- none"]),
        "",
        "Segment Status",
        *segment_lines,
        blocker_line,
        "",
        "Local Diagnostic Classification",
        f"- {classification}",
    ]
    return lines, classification


def build_report(repo_root: Path, attempt_uuid: Optional[str] = None) -> List[str]:
    report, _ = build_report_result(repo_root, attempt_uuid)
    return report


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
    report, classification = build_report_result(repo_root, attempt_uuid=args.attempt_uuid)
    print("\n".join(report))
    return 0 if classification == FINAL_DIAGNOSTIC else 1


if __name__ == "__main__":
    raise SystemExit(main())
