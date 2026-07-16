from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

SCHEMA = "physanim-ue-automation-validation/v1"
DEFAULT_REQUIRED_ARTIFACT_FIELDS = (
    "physics_samples",
    "policy_samples",
    "scenario_summary",
    "policy_input_snapshot",
    "render_capture",
)


@dataclass(frozen=True)
class Issue:
    code: str
    message: str
    path: str | None = None

    def as_dict(self) -> dict[str, str]:
        value = {"code": self.code, "message": self.message}
        if self.path:
            value["path"] = self.path
        return value


def _read_json(path: Path, label: str, issues: list[Issue]) -> dict[str, Any] | None:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except OSError as exc:
        issues.append(Issue(f"missing_{label}", f"Cannot read {label}: {exc}", str(path)))
        return None
    except json.JSONDecodeError as exc:
        issues.append(Issue(f"invalid_{label}", f"Invalid {label} JSON: {exc}", str(path)))
        return None
    if not isinstance(value, dict):
        issues.append(Issue(f"invalid_{label}", f"{label} must be a JSON object", str(path)))
        return None
    return value


def _normalize(path: Path) -> str:
    return str(path.resolve()).replace("\\", "/").casefold()


def _resolve_artifact(run_root: Path, value: object) -> Path | None:
    if not isinstance(value, str) or not value:
        return None
    candidate = Path(value)
    return candidate if candidate.is_absolute() else run_root / candidate


def _find_report_index(report_root: Path) -> Path:
    direct = report_root / "index.json"
    if direct.exists():
        return direct
    candidates = sorted(report_root.rglob("index.json")) if report_root.exists() else []
    return candidates[0] if candidates else direct


def validate_run(
    *,
    run_root: Path,
    report_root: Path,
    expected_test: str,
    expected_source_commit: str,
    expected_protocol: Path,
    expected_variant: str,
    expected_repetition: int,
    required_artifact_fields: tuple[str, ...] = DEFAULT_REQUIRED_ARTIFACT_FIELDS,
) -> dict[str, Any]:
    run_root = run_root.resolve()
    report_root = report_root.resolve()
    expected_protocol = expected_protocol.resolve()
    issues: list[Issue] = []
    checks = {
        "automation_report_present": False,
        "automation_success": False,
        "manifest_present": False,
        "manifest_identity_matches": False,
        "source_clean": False,
        "required_artifacts_present": False,
    }

    report_path = _find_report_index(report_root)
    report = _read_json(report_path, "automation_report", issues)
    if report is not None:
        checks["automation_report_present"] = True
        tests = report.get("tests")
        matching = []
        if isinstance(tests, list):
            matching = [
                test
                for test in tests
                if isinstance(test, dict) and test.get("fullTestPath") == expected_test
            ]
        if not matching:
            issues.append(
                Issue(
                    "expected_test_missing",
                    f"Automation report does not contain exact test {expected_test!r}",
                    str(report_path),
                )
            )
        else:
            test = matching[0]
            counters_clean = (
                int(report.get("failed", 0)) == 0
                and int(report.get("inProcess", 0)) == 0
                and int(report.get("notRun", 0)) == 0
                and int(report.get("succeeded", 0)) >= 1
            )
            test_clean = (
                test.get("state") == "Success"
                and int(test.get("errors", 0)) == 0
                and int(test.get("warnings", 0)) == 0
            )
            checks["automation_success"] = counters_clean and test_clean
            if not checks["automation_success"]:
                issues.append(
                    Issue(
                        "automation_not_successful",
                        "Automation report or exact test is not a clean success",
                        str(report_path),
                    )
                )

    manifest_path = run_root / "manifest.json"
    manifest = _read_json(manifest_path, "manifest", issues)
    if manifest is not None:
        checks["manifest_present"] = True
        identity_ok = True
        if manifest.get("source_commit") != expected_source_commit:
            identity_ok = False
            issues.append(
                Issue(
                    "source_commit_mismatch",
                    f"Expected source {expected_source_commit}, found {manifest.get('source_commit')!r}",
                    str(manifest_path),
                )
            )
        if manifest.get("variant") != expected_variant:
            identity_ok = False
            issues.append(
                Issue(
                    "variant_mismatch",
                    f"Expected variant {expected_variant!r}, found {manifest.get('variant')!r}",
                    str(manifest_path),
                )
            )
        try:
            repetition_matches = int(manifest.get("repetition")) == int(expected_repetition)
        except (TypeError, ValueError):
            repetition_matches = False
        if not repetition_matches:
            identity_ok = False
            issues.append(
                Issue(
                    "repetition_mismatch",
                    f"Expected repetition {expected_repetition}, found {manifest.get('repetition')!r}",
                    str(manifest_path),
                )
            )
        manifest_protocol = manifest.get("protocol_path")
        if not isinstance(manifest_protocol, str) or _normalize(Path(manifest_protocol)) != _normalize(expected_protocol):
            identity_ok = False
            issues.append(
                Issue(
                    "protocol_mismatch",
                    f"Expected protocol {expected_protocol}, found {manifest_protocol!r}",
                    str(manifest_path),
                )
            )
        checks["manifest_identity_matches"] = identity_ok

        checks["source_clean"] = manifest.get("source_tree_dirty") is False
        if not checks["source_clean"]:
            issues.append(Issue("dirty_source", "Manifest declares a dirty source tree", str(manifest_path)))

        all_artifacts = True
        for field in required_artifact_fields:
            artifact = _resolve_artifact(run_root, manifest.get(field))
            if artifact is None or not artifact.exists() or not artifact.is_file() or artifact.stat().st_size == 0:
                all_artifacts = False
                issues.append(
                    Issue(
                        "missing_artifact",
                        f"Required manifest artifact {field!r} is missing or empty: {manifest.get(field)!r}",
                        str(manifest_path),
                    )
                )
        checks["required_artifacts_present"] = all_artifacts

    verdict = "PASS" if all(checks.values()) and not issues else "INVALID"
    return {
        "schema_version": SCHEMA,
        "verdict": verdict,
        "run_root": str(run_root),
        "report_root": str(report_root),
        "expected": {
            "test": expected_test,
            "source_commit": expected_source_commit,
            "protocol": str(expected_protocol),
            "variant": expected_variant,
            "repetition": expected_repetition,
        },
        "checks": checks,
        "issues": [issue.as_dict() for issue in issues],
    }


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate one completed Unreal automation product-fixture run.")
    parser.add_argument("--run-root", type=Path, required=True)
    parser.add_argument("--report-root", type=Path, required=True)
    parser.add_argument("--expected-test", required=True)
    parser.add_argument("--expected-source-commit", required=True)
    parser.add_argument("--expected-protocol", type=Path, required=True)
    parser.add_argument("--expected-variant", required=True)
    parser.add_argument("--expected-repetition", type=int, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--required-artifact-field", action="append", dest="artifact_fields")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    fields = tuple(args.artifact_fields) if args.artifact_fields else DEFAULT_REQUIRED_ARTIFACT_FIELDS
    result = validate_run(
        run_root=args.run_root,
        report_root=args.report_root,
        expected_test=args.expected_test,
        expected_source_commit=args.expected_source_commit,
        expected_protocol=args.expected_protocol,
        expected_variant=args.expected_variant,
        expected_repetition=args.expected_repetition,
        required_artifact_fields=fields,
    )
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        sys.stdout.write(payload)
    return 0 if result["verdict"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
