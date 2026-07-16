from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Sequence

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from scripts.evaluate_scripted_locomotion_protocol import (
    ProtocolLinkageError,
    evaluate_protocol_causal_metrics,
    validate_protocol_linkage,
)


class DeterminismAuditError(ValueError):
    pass


def _read_object(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except FileNotFoundError as exc:
        raise DeterminismAuditError(f"{label} does not exist: {path}") from exc
    except json.JSONDecodeError as exc:
        raise DeterminismAuditError(f"{label} is invalid JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise DeterminismAuditError(f"{label} must be a JSON object: {path}")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().lower()


def _sha256_locked_text(path: Path) -> str:
    normalized = path.read_bytes().replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    return hashlib.sha256(normalized).hexdigest().lower()


def _finite(value: object, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise DeterminismAuditError(f"{label} must be numeric")
    number = float(value)
    if not math.isfinite(number):
        raise DeterminismAuditError(f"{label} must be finite")
    return number


def _unique_values(records: Sequence[dict[str, Any]], field: str) -> list[Any]:
    values = {json.dumps(record.get(field), sort_keys=True) for record in records}
    return [json.loads(value) for value in sorted(values)]


def _campaign(protocol: dict[str, Any], campaign_name: str) -> dict[str, Any]:
    determinism = protocol.get("determinism")
    if not isinstance(determinism, dict):
        raise DeterminismAuditError("protocol does not declare a determinism contract")
    campaigns = determinism.get("campaigns")
    if not isinstance(campaigns, dict):
        raise DeterminismAuditError("protocol determinism.campaigns must be an object")
    campaign = campaigns.get(campaign_name)
    if not isinstance(campaign, dict):
        raise DeterminismAuditError(f"unknown determinism campaign: {campaign_name}")
    return campaign


def audit_campaign_records(
    protocol: dict[str, Any],
    campaign_name: str,
    records: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    campaign = _campaign(protocol, campaign_name)
    variant = campaign.get("variant")
    repetitions = campaign.get("repetitions")
    if not isinstance(variant, str) or not variant:
        raise DeterminismAuditError("campaign variant must be a nonempty string")
    if isinstance(repetitions, bool) or not isinstance(repetitions, int) or repetitions < 2:
        raise DeterminismAuditError("campaign repetitions must be an integer of at least two")

    issues: list[dict[str, Any]] = []
    behavioral_failures: list[dict[str, Any]] = []

    identities: list[tuple[str, int]] = []
    for index, record in enumerate(records):
        if not isinstance(record, dict):
            raise DeterminismAuditError(f"record {index} must be an object")
        record_variant = record.get("variant")
        repetition = record.get("repetition")
        if not isinstance(record_variant, str) or isinstance(repetition, bool) or not isinstance(repetition, int):
            issues.append({"code": "invalid_run_identity", "record": index})
            continue
        identities.append((record_variant, repetition))

    expected = {(variant, repetition) for repetition in range(1, repetitions + 1)}
    counts = Counter(identities)
    actual = set(identities)
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    duplicates = sorted(identity for identity, count in counts.items() if count > 1)
    if missing:
        issues.append({"code": "missing_repetitions", "runs": [list(value) for value in missing]})
    if unexpected:
        issues.append({"code": "unexpected_repetitions", "runs": [list(value) for value in unexpected]})
    if duplicates:
        issues.append({"code": "duplicate_repetitions", "runs": [list(value) for value in duplicates]})

    clean_required = campaign.get("require_clean_source") is True
    if clean_required:
        dirty = [record.get("repetition") for record in records if record.get("source_tree_dirty") is not False]
        if dirty:
            issues.append({"code": "dirty_source", "repetitions": dirty})

    internal_authority_fields = (
        ("source_commit", "fingerprint_source_commit", "fingerprint_source_commit_mismatch"),
        ("source_tree_dirty", "fingerprint_source_tree_dirty", "fingerprint_dirty_state_mismatch"),
        ("model_sha256", "fingerprint_model_sha256", "fingerprint_model_sha256_mismatch"),
        ("protocol_sha256", "fingerprint_protocol_sha256", "fingerprint_protocol_sha256_mismatch"),
    )
    for record in records:
        for manifest_field, fingerprint_field, issue_code in internal_authority_fields:
            left = record.get(manifest_field)
            right = record.get(fingerprint_field)
            if isinstance(left, str) and isinstance(right, str):
                matches = left.lower() == right.lower()
            else:
                matches = left == right
            if not matches:
                issues.append(
                    {
                        "code": issue_code,
                        "repetition": record.get("repetition"),
                        "manifest_value": left,
                        "fingerprint_value": right,
                    }
                )

    equality_fields = (
        ("require_same_source_commit", "source_commit", "source_commit_mismatch"),
        ("require_same_model_sha256", "model_sha256", "model_sha256_mismatch"),
        ("require_same_protocol_sha256", "protocol_sha256", "protocol_sha256_mismatch"),
        ("require_authority_digest_match", "authority_digest_sha256", "authority_digest_mismatch"),
    )
    equality_results: dict[str, Any] = {}
    for switch, field, issue_code in equality_fields:
        values = _unique_values(records, field)
        required = campaign.get(switch) is True
        passed = not required or (len(values) == 1 and values[0] not in (None, ""))
        equality_results[field] = {"required": required, "values": values, "passed": passed}
        if not passed:
            issues.append({"code": issue_code, "values": values})

    byte_results: dict[str, Any] = {}
    byte_artifacts = campaign.get("byte_identical_artifacts", [])
    if not isinstance(byte_artifacts, list) or not all(
        isinstance(value, str) and value for value in byte_artifacts
    ):
        raise DeterminismAuditError("byte_identical_artifacts must be an array of filenames")
    for artifact in byte_artifacts:
        hashes = [record.get("artifact_hashes", {}).get(artifact) for record in records]
        passed = len(set(hashes)) == 1 and hashes[0] not in (None, "") if hashes else False
        byte_results[artifact] = {"hashes": hashes, "passed": passed}
        if not passed:
            behavioral_failures.append({"code": "byte_artifact_mismatch", "artifact": artifact})

    causal_results: list[dict[str, Any]] = []
    for record in records:
        verdict = record.get("causal_verdict")
        passed = verdict == "PASS"
        causal_results.append(
            {"repetition": record.get("repetition"), "verdict": verdict, "passed": passed}
        )
        if not passed:
            behavioral_failures.append(
                {
                    "code": "causal_metric_contract_failed",
                    "repetition": record.get("repetition"),
                    "verdict": verdict,
                }
            )

    numeric_results: dict[str, Any] = {}
    numeric_contract = campaign.get("numeric_endpoints", {})
    if not isinstance(numeric_contract, dict):
        raise DeterminismAuditError("campaign numeric_endpoints must be an object")
    for endpoint, limits in numeric_contract.items():
        if not isinstance(endpoint, str) or not isinstance(limits, dict):
            raise DeterminismAuditError("numeric endpoint contracts must be named objects")
        values = [_finite(record.get("metrics", {}).get(endpoint), endpoint) for record in records]
        minimum = min(values) if values else math.nan
        maximum = max(values) if values else math.nan
        absolute_spread = maximum - minimum if values else math.nan
        scale = max(max(abs(value) for value in values), 1.0e-12) if values else math.nan
        relative_spread = absolute_spread / scale if values else math.nan
        maximum_absolute = _finite(
            limits.get("maximum_absolute_spread"),
            f"{endpoint}.maximum_absolute_spread",
        )
        maximum_relative = _finite(
            limits.get("maximum_relative_spread"),
            f"{endpoint}.maximum_relative_spread",
        )
        passed = absolute_spread <= maximum_absolute and relative_spread <= maximum_relative
        numeric_results[endpoint] = {
            "values": values,
            "minimum": minimum,
            "maximum": maximum,
            "absolute_spread": absolute_spread,
            "relative_spread": relative_spread,
            "maximum_absolute_spread": maximum_absolute,
            "maximum_relative_spread": maximum_relative,
            "passed": passed,
        }
        if not passed:
            behavioral_failures.append({"code": "numeric_spread_exceeded", "endpoint": endpoint})

    if issues:
        verdict = "INVALID"
    elif behavioral_failures:
        verdict = "FAIL"
    else:
        verdict = "PASS"
    return {
        "schema_version": "physanim-locomotion-determinism-audit/v1",
        "verdict": verdict,
        "campaign_name": campaign_name,
        "campaign": campaign,
        "expected_run_count": repetitions,
        "actual_run_count": len(records),
        "issues": issues,
        "behavioral_failures": behavioral_failures,
        "authority_equality": equality_results,
        "byte_determinism": byte_results,
        "causal_contracts": causal_results,
        "numeric_determinism": numeric_results,
    }


def load_run_record(run_root: Path | str) -> dict[str, Any]:
    run_root = Path(run_root).resolve()
    manifest_path = run_root / "manifest.json"
    fingerprint_path = run_root / "environment-fingerprint.json"
    manifest = _read_object(manifest_path, "run manifest")
    fingerprint = _read_object(fingerprint_path, "environment fingerprint")
    try:
        validate_protocol_linkage(manifest_path)
        causal = evaluate_protocol_causal_metrics(manifest_path)
    except (ProtocolLinkageError, ValueError) as exc:
        raise DeterminismAuditError(f"run linkage or causal evaluation failed at {run_root}: {exc}") from exc

    artifact_hashes: dict[str, str | None] = {}
    protocol_path = Path(str(manifest.get("protocol_path", ""))).resolve()
    if not protocol_path.is_file():
        raise DeterminismAuditError(f"manifest protocol path is missing: {protocol_path}")
    campaign_protocol = _read_object(protocol_path, "scripted-locomotion protocol")
    determinism = campaign_protocol.get("determinism", {})
    campaigns = determinism.get("campaigns", {}) if isinstance(determinism, dict) else {}
    requested_artifacts: set[str] = set()
    if isinstance(campaigns, dict):
        for value in campaigns.values():
            if isinstance(value, dict) and isinstance(value.get("byte_identical_artifacts"), list):
                requested_artifacts.update(
                    item for item in value["byte_identical_artifacts"] if isinstance(item, str)
                )
    for artifact in sorted(requested_artifacts):
        path = run_root / artifact
        artifact_hashes[artifact] = _sha256(path) if path.is_file() else None

    fingerprint_artifacts = fingerprint.get("artifacts")
    if not isinstance(fingerprint_artifacts, dict):
        raise DeterminismAuditError(f"environment fingerprint artifacts are malformed: {fingerprint_path}")
    source = fingerprint.get("source")
    if not isinstance(source, dict):
        raise DeterminismAuditError(f"environment fingerprint source is malformed: {fingerprint_path}")
    return {
        "run_root": str(run_root),
        "variant": manifest.get("variant"),
        "repetition": manifest.get("repetition"),
        "source_commit": manifest.get("source_commit"),
        "source_tree_dirty": manifest.get("source_tree_dirty"),
        "model_sha256": str(manifest.get("model_onnx_sha256", "")).lower(),
        "protocol_sha256": str(manifest.get("protocol_sha256", "")).lower(),
        "authority_digest_sha256": fingerprint.get("authority_digest_sha256"),
        "fingerprint_source_commit": source.get("commit"),
        "fingerprint_source_tree_dirty": source.get("tree_dirty"),
        "fingerprint_model_sha256": fingerprint_artifacts.get("model_onnx_sha256"),
        "fingerprint_protocol_sha256": fingerprint_artifacts.get("protocol_sha256"),
        "artifact_hashes": artifact_hashes,
        "causal_verdict": causal.get("verdict"),
        "metrics": causal.get("metrics", {}),
    }


def audit_run_roots(
    protocol_path: Path | str,
    campaign_name: str,
    run_roots: Sequence[Path | str],
) -> dict[str, Any]:
    protocol_path = Path(protocol_path).resolve()
    protocol = _read_object(protocol_path, "scripted-locomotion protocol")
    records: list[dict[str, Any]] = []
    load_errors: list[dict[str, str]] = []
    for run_root in run_roots:
        try:
            records.append(load_run_record(run_root))
        except DeterminismAuditError as exc:
            load_errors.append({"run_root": str(Path(run_root).resolve()), "error": str(exc)})
    if load_errors:
        return {
            "schema_version": "physanim-locomotion-determinism-audit/v1",
            "verdict": "INVALID",
            "campaign_name": campaign_name,
            "protocol_path": str(protocol_path),
            "protocol_sha256": _sha256_locked_text(protocol_path),
            "load_errors": load_errors,
        }
    report = audit_campaign_records(protocol, campaign_name, records)
    report["protocol_path"] = str(protocol_path)
    report["protocol_sha256"] = _sha256_locked_text(protocol_path)
    report["runs"] = records
    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit a preregistered scripted-locomotion determinism campaign."
    )
    parser.add_argument("--protocol", type=Path, required=True)
    parser.add_argument("--campaign", required=True)
    parser.add_argument("--run-root", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = audit_run_roots(args.protocol, args.campaign, args.run_root)
    except DeterminismAuditError as exc:
        report = {
            "schema_version": "physanim-locomotion-determinism-audit/v1",
            "verdict": "INVALID",
            "error": str(exc),
        }
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return {"PASS": 0, "FAIL": 2}.get(report.get("verdict"), 1)


if __name__ == "__main__":
    raise SystemExit(main())
