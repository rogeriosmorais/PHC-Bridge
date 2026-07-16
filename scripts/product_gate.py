from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence


VERIFIER_VERSION = "2.0.0"
REPO_ROOT = Path(__file__).resolve().parents[1]
CONTRACT_PATH = REPO_ROOT / "product-gates" / "standing-v0.v2.json"

# This digest is deliberately duplicated in executable code. Changing a locked
# contract is therefore a code change, not a data-only threshold adjustment.
EXPECTED_CONTRACT_SHA256 = "ce3e094c6e1731eb26222bb17c70dd82db76561c43f1fec3daed518c140cf32d"

UNTRUSTED_RESULT_FIELDS = {
    "artifact_pass",
    "controller_stability_passed",
    "physical_continuity_validator_passed",
    "product_success",
    "strict_verdict",
    "thresholds",
}


class ProductGateConfigurationError(RuntimeError):
    pass


@dataclass(frozen=True)
class ProductGateResult:
    passed: bool
    failures: tuple[str, ...]
    checks: tuple[str, ...]
    report: dict[str, Any]


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _reject_duplicate_keys(pairs: Sequence[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _load_json_bytes(data: bytes, label: str) -> Mapping[str, Any]:
    try:
        value = json.loads(data.decode("utf-8"), object_pairs_hook=_reject_duplicate_keys)
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
        raise ProductGateConfigurationError(f"invalid {label} JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise ProductGateConfigurationError(f"{label} must be a JSON object")
    return value


def _load_locked_contract() -> tuple[Mapping[str, Any], bytes, str]:
    contract_bytes = CONTRACT_PATH.read_bytes()
    digest = _sha256(contract_bytes)
    if digest != EXPECTED_CONTRACT_SHA256:
        raise ProductGateConfigurationError(
            "locked product-gate contract digest mismatch; append a new version instead of editing v2"
        )
    contract = _load_json_bytes(contract_bytes, "contract")
    if contract.get("status") != "LOCKED":
        raise ProductGateConfigurationError("product-gate contract is not LOCKED")
    if contract.get("schema_version") != "physanim-product-gate-contract/v2":
        raise ProductGateConfigurationError("unsupported product-gate contract schema")
    return contract, contract_bytes, digest


def _is_finite_number(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(value)


def _parse_timestamp(value: Any) -> datetime | None:
    if not isinstance(value, str) or not value:
        return None
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError:
        return None
    if parsed.tzinfo is None:
        return None
    return parsed.astimezone(timezone.utc)


def _evaluate_criterion(criterion: Mapping[str, Any], evidence: Mapping[str, Any]) -> tuple[bool, str]:
    fact = criterion.get("fact")
    operator = criterion.get("operator")
    expected = criterion.get("value")
    if not isinstance(fact, str) or not isinstance(operator, str):
        raise ProductGateConfigurationError("criterion requires string fact and operator")
    if fact not in evidence:
        return False, f"missing required fact: {fact}"

    actual = evidence[fact]
    if operator == "equals":
        passed = type(actual) is type(expected) and actual == expected
        return passed, f"{fact} must equal {expected!r}"
    if operator == "nonempty":
        passed = isinstance(actual, str) and bool(actual.strip())
        return passed, f"{fact} must be a non-empty string"
    if operator == "timestamp":
        passed = _parse_timestamp(actual) is not None
        return passed, f"{fact} must be an ISO-8601 timestamp with timezone"
    if operator == "commit":
        passed = isinstance(actual, str) and re.fullmatch(r"[0-9a-fA-F]{40}", actual) is not None
        return passed, f"{fact} must be a full 40-character commit hash"
    if operator == "one_of":
        if not isinstance(expected, list):
            raise ProductGateConfigurationError(f"one_of criterion for {fact} requires a list")
        passed = actual in expected and all(type(actual) is type(item) for item in expected)
        return passed, f"{fact} must be one of {expected!r}"
    if operator in {"minimum", "maximum", "positive"}:
        if not _is_finite_number(actual):
            return False, f"{fact} must be finite"
        if actual < 0:
            return False, f"{fact} must be non-negative"
        if operator == "minimum":
            passed = actual >= expected
            return passed, f"{fact} must be >= {expected}"
        if operator == "maximum":
            passed = actual <= expected
            return passed, f"{fact} must be <= {expected}"
        passed = actual > 0
        return passed, f"{fact} must be > 0"
    if operator == "range":
        minimum = criterion.get("minimum")
        maximum = criterion.get("maximum")
        exclusive_minimum = criterion.get("exclusive_minimum", False)
        if (
            not _is_finite_number(minimum)
            or not _is_finite_number(maximum)
            or minimum > maximum
            or not isinstance(exclusive_minimum, bool)
        ):
            raise ProductGateConfigurationError(
                f"range criterion for {fact} requires valid minimum and maximum"
            )
        if not _is_finite_number(actual):
            return False, f"{fact} must be finite"
        if actual < 0:
            return False, f"{fact} must be non-negative"
        lower_passed = actual > minimum if exclusive_minimum else actual >= minimum
        passed = lower_passed and actual <= maximum
        if exclusive_minimum:
            return passed, f"{fact} must be > {minimum} and <= {maximum}"
        return passed, f"{fact} must be between {minimum} and {maximum}"
    raise ProductGateConfigurationError(f"unsupported criterion operator: {operator}")


def _evaluate_invariant(invariant: Mapping[str, Any], evidence: Mapping[str, Any]) -> tuple[bool, str]:
    left = invariant.get("left")
    right = invariant.get("right")
    operator = invariant.get("operator")
    if not isinstance(left, str) or not isinstance(right, str):
        raise ProductGateConfigurationError("invariant requires string left and right facts")
    if left not in evidence or right not in evidence:
        return False, f"invariant facts must exist: {left}, {right}"
    left_value = evidence[left]
    right_value = evidence[right]
    if not _is_finite_number(left_value) or not _is_finite_number(right_value):
        return False, f"invariant facts must be finite: {left}, {right}"
    if operator == "less_than_or_equal":
        return left_value <= right_value, f"{left} must be <= {right}"
    raise ProductGateConfigurationError(f"unsupported invariant operator: {operator}")


def evaluate_product_gate(
    evidence_path: Path,
    *,
    now: datetime | None = None,
    git_revision: str,
) -> ProductGateResult:
    contract, _, contract_digest = _load_locked_contract()
    evidence_bytes = evidence_path.read_bytes()
    evidence = _load_json_bytes(evidence_bytes, "evidence")
    evaluated_at = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
    failures: list[str] = []
    checks: list[str] = []

    if not re.fullmatch(r"[0-9a-fA-F]{40}", git_revision):
        raise ProductGateConfigurationError("git revision must be a full 40-character commit hash")

    source_commit = evidence.get("source_commit")
    if isinstance(source_commit, str) and source_commit.lower() != git_revision.lower():
        failures.append("source_commit must match git revision")
        checks.append("source_commit must match git revision")

    criteria = contract.get("criteria")
    if not isinstance(criteria, list) or not criteria:
        raise ProductGateConfigurationError("locked contract has no criteria")
    for criterion in criteria:
        if not isinstance(criterion, dict):
            raise ProductGateConfigurationError("contract criterion must be an object")
        passed, message = _evaluate_criterion(criterion, evidence)
        checks.append(message)
        if not passed:
            failures.append(message)

    invariants = contract.get("invariants")
    if not isinstance(invariants, list):
        raise ProductGateConfigurationError("locked contract invariants must be a list")
    for invariant in invariants:
        if not isinstance(invariant, dict):
            raise ProductGateConfigurationError("contract invariant must be an object")
        passed, message = _evaluate_invariant(invariant, evidence)
        checks.append(message)
        if not passed:
            failures.append(message)

    captured_at = _parse_timestamp(evidence.get("captured_at_utc"))
    if captured_at is not None:
        age_seconds = (evaluated_at - captured_at).total_seconds()
        max_age = contract.get("max_evidence_age_seconds")
        if not _is_finite_number(max_age) or max_age <= 0:
            raise ProductGateConfigurationError("contract max_evidence_age_seconds must be positive")
        checks.append(f"evidence age must be between 0 and {max_age} seconds")
        if age_seconds < -5:
            failures.append("evidence timestamp is in the future")
        elif age_seconds > max_age:
            failures.append(
                f"evidence is stale: age {age_seconds:.1f}s exceeds {max_age}s"
            )

    failures = list(dict.fromkeys(failures))
    passed = not failures
    ignored_fields = sorted(UNTRUSTED_RESULT_FIELDS.intersection(evidence))
    report: dict[str, Any] = {
        "schema_version": "physanim-local-gate-diagnostic/v2",
        "authority": "LOCAL_DIAGNOSTIC_ONLY",
        "verifier_version": VERIFIER_VERSION,
        "gate_id": contract["gate_id"],
        "gate_version": contract["gate_version"],
        "contract_sha256": contract_digest,
        "evidence_sha256": _sha256(evidence_bytes),
        "git_revision": git_revision.lower(),
        "attempt_uuid": evidence.get("attempt_uuid"),
        "evaluated_at_utc": evaluated_at.isoformat().replace("+00:00", "Z"),
        "diagnostic_outcome": (
            "ALL_CRITERIA_OBSERVED" if passed else "CRITERIA_NOT_OBSERVED"
        ),
        "failures": failures,
        "ignored_untrusted_result_fields": ignored_fields,
    }
    return ProductGateResult(passed, tuple(failures), tuple(checks), report)


def _git_revision(repo_root: Path) -> str:
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=False,
    )
    revision = completed.stdout.strip()
    if completed.returncode != 0 or not revision:
        raise ProductGateConfigurationError("unable to resolve git revision")
    return revision


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run a non-authoritative local diagnostic against standing-v0. "
            "Only the protected external oracle can issue product receipts."
        )
    )
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        result = evaluate_product_gate(
            args.evidence.resolve(),
            git_revision=_git_revision(args.repo_root.resolve()),
        )
    except (OSError, ProductGateConfigurationError) as exc:
        print(f"PRODUCT GATE ERROR: {exc}", file=sys.stderr)
        return 2

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(
        json.dumps(result.report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"LOCAL DIAGNOSTIC: {result.report['diagnostic_outcome']}")
    for failure in result.failures:
        print(f"- {failure}")
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
