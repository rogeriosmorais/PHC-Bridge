from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

AUDIT_SCHEMA = "physanim-experiment-evidence-audit/v1"
PHASE_CONSTANTS = {
    "standing_hold": "StandingEndSeconds",
    "acceleration": "AccelerationEndSeconds",
    "cruise": "CruiseEndSeconds",
    "moving_turn": "MovingTurnEndSeconds",
    "deceleration": "DecelerationEndSeconds",
}


@dataclass(frozen=True)
class Issue:
    severity: str
    code: str
    message: str
    path: str | None = None
    experiment_id: str | None = None

    def as_dict(self) -> dict[str, Any]:
        value: dict[str, Any] = {
            "severity": self.severity,
            "code": self.code,
            "message": self.message,
        }
        if self.path is not None:
            value["path"] = self.path
        if self.experiment_id is not None:
            value["experiment_id"] = self.experiment_id
        return value


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def _repo_relative(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path)


def _extract_phase_ends(source: str) -> dict[str, float]:
    values: dict[str, float] = {}
    for phase, constant in PHASE_CONSTANTS.items():
        match = re.search(
            rf"\b{re.escape(constant)}\s*=\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+))\s*;",
            source,
        )
        if match:
            values[phase] = float(match.group(1))
    return values


def _protocol_phase_ends(protocol: dict[str, Any]) -> dict[str, float]:
    script = protocol.get("script")
    if not isinstance(script, dict):
        return {}
    values: dict[str, float] = {}
    for phase in PHASE_CONSTANTS:
        spec = script.get(phase)
        if isinstance(spec, dict) and isinstance(spec.get("end_sec"), (int, float)):
            values[phase] = float(spec["end_sec"])
    return values


def compare_scripted_schedule(
    protocol: dict[str, Any], source: str, tolerance: float = 1.0e-9
) -> dict[str, Any]:
    protocol_ends = _protocol_phase_ends(protocol)
    runtime_ends = _extract_phase_ends(source)
    compared = sorted(set(protocol_ends) & set(runtime_ends))
    mismatches = [
        phase
        for phase in compared
        if abs(protocol_ends[phase] - runtime_ends[phase]) > tolerance
    ]
    missing_from_runtime = sorted(set(protocol_ends) - set(runtime_ends))
    missing_from_protocol = sorted(set(runtime_ends) - set(protocol_ends))
    return {
        "status": "MATCH" if not mismatches and not missing_from_runtime else "MISMATCH",
        "protocol_phase_end_sec": protocol_ends,
        "runtime_phase_end_sec": runtime_ends,
        "mismatched_phases": mismatches,
        "missing_from_runtime": missing_from_runtime,
        "missing_from_protocol": missing_from_protocol,
    }


def _git_object_exists(root: Path, revision: str) -> bool:
    if not revision or not re.fullmatch(r"[0-9a-fA-F]{7,40}", revision):
        return False
    completed = subprocess.run(
        ["git", "cat-file", "-e", f"{revision}^{{commit}}"],
        cwd=root,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return completed.returncode == 0


def _iter_json_files(path: Path) -> Iterable[Path]:
    if not path.exists():
        return []
    return sorted(p for p in path.rglob("*.json") if p.is_file())


def _experiment_lineage_key(
    experiments_root: Path, path: Path, experiment_id: str
) -> tuple[str, int | None]:
    try:
        relative = path.relative_to(experiments_root)
        stage = relative.parts[0].casefold() if len(relative.parts) > 1 else "unknown"
    except ValueError:
        stage = "unknown"

    token_source = f"{path.name} {experiment_id}"
    experiment_match = re.search(
        r"(?i)(?:^|[._-])e(\d+)(?=[._-]|\s|$)", token_source
    )
    attempt_match = re.search(
        r"(?i)(?:^|[._-])attempt(\d+)(?=[._-]|\s|$)", token_source
    )
    if experiment_match:
        base_key = f"{stage}/e{int(experiment_match.group(1))}"
    else:
        normalized_id = re.sub(r"[^a-z0-9]+", "-", experiment_id.casefold()).strip("-")
        base_key = f"{stage}/id/{normalized_id}"
    attempt = int(attempt_match.group(1)) if attempt_match else None
    return base_key, attempt


def _collect_experiments(
    root: Path, issues: list[Issue], check_git: bool
) -> dict[str, dict[str, Any]]:
    experiments_root = root / "experiments"
    entries_by_base: dict[str, list[dict[str, Any]]] = {}
    for path in _iter_json_files(experiments_root):
        try:
            value = _read_json(path)
        except (OSError, json.JSONDecodeError, ValueError) as exc:
            issues.append(
                Issue("error", "invalid_experiment_json", str(exc), _repo_relative(root, path))
            )
            continue
        experiment_id = value.get("experiment_id")
        if not isinstance(experiment_id, str) or not experiment_id:
            continue
        base_key, attempt = _experiment_lineage_key(experiments_root, path, experiment_id)
        entry = {
            "attempt": attempt,
            "experiment_id": experiment_id,
            "is_preregistration": path.name.endswith(".preregister.json"),
            "path": _repo_relative(root, path),
            "value": value,
        }
        entries_by_base.setdefault(base_key, []).append(entry)

        if check_git:
            lineage_key = base_key if attempt is None else f"{base_key}/attempt{attempt}"
            for field in ("baseline_commit", "behavior_commit", "evidence_commit"):
                revision = value.get(field)
                if revision is not None and (
                    not isinstance(revision, str) or not _git_object_exists(root, revision)
                ):
                    issues.append(
                        Issue(
                            "error",
                            "missing_commit",
                            f"{field} does not resolve to a commit: {revision!r}",
                            entry["path"],
                            lineage_key,
                        )
                    )

    records: dict[str, dict[str, Any]] = {}
    for base_key, entries in sorted(entries_by_base.items()):
        prereg_by_attempt: dict[int | None, list[dict[str, Any]]] = {}
        result_by_attempt: dict[int | None, list[dict[str, Any]]] = {}
        for entry in entries:
            target = prereg_by_attempt if entry["is_preregistration"] else result_by_attempt
            target.setdefault(entry["attempt"], []).append(entry)

        # Historical attempt-1 files commonly reused the base preregistration while
        # later attempts gained explicit attempt-specific preregistrations.
        if (
            None in prereg_by_attempt
            and None not in result_by_attempt
            and 1 in result_by_attempt
            and 1 not in prereg_by_attempt
        ):
            prereg_by_attempt[1] = prereg_by_attempt.pop(None)

        attempts = sorted(
            set(prereg_by_attempt) | set(result_by_attempt),
            key=lambda value: (-1 if value is None else value),
        )
        for attempt in attempts:
            lineage_key = base_key if attempt is None else f"{base_key}/attempt{attempt}"
            preregistrations = prereg_by_attempt.get(attempt, [])
            results = result_by_attempt.get(attempt, [])
            source_ids = sorted(
                {
                    entry["experiment_id"]
                    for entry in preregistrations + results
                }
            )
            record = {
                "preregistrations": sorted(entry["path"] for entry in preregistrations),
                "results": sorted(entry["path"] for entry in results),
                "source_experiment_ids": source_ids,
                "has_preregistration": bool(preregistrations),
                "has_result": bool(results),
            }
            records[lineage_key] = record

            if record["has_preregistration"] and not record["has_result"]:
                issues.append(
                    Issue(
                        "error",
                        "experiment_missing_result",
                        "Preregistration has no recorded result.",
                        record["preregistrations"][0],
                        lineage_key,
                    )
                )
            if record["has_result"] and not record["has_preregistration"]:
                issues.append(
                    Issue(
                        "warning",
                        "experiment_missing_preregistration",
                        "Result has no matching preregistration; retained as legacy evidence rather than a current blocking error.",
                        record["results"][0],
                        lineage_key,
                    )
                )
    return records


def _resolve_manifest_path(root: Path, manifest_dir: Path, value: object) -> Path | None:
    if not isinstance(value, str) or not value:
        return None
    candidate = Path(value)
    if candidate.is_absolute():
        return candidate
    local = manifest_dir / candidate
    if local.exists():
        return local
    return root / candidate


def _collect_manifests(root: Path, issues: list[Issue]) -> list[dict[str, Any]]:
    manifests: list[dict[str, Any]] = []
    test_root = root / "test-results"
    if not test_root.exists():
        return manifests
    for path in sorted(test_root.rglob("manifest.json")):
        relative = _repo_relative(root, path)
        try:
            value = _read_json(path)
        except (OSError, json.JSONDecodeError, ValueError) as exc:
            issues.append(Issue("error", "invalid_manifest_json", str(exc), relative))
            continue
        manifests.append({"path": relative, "variant": value.get("variant")})
        if value.get("source_tree_dirty") is True:
            issues.append(
                Issue("error", "dirty_source_run", "Run declares a dirty source tree.", relative)
            )

        protocol_path = _resolve_manifest_path(root, path.parent, value.get("protocol_path"))
        if protocol_path is None or not protocol_path.exists():
            issues.append(
                Issue(
                    "error",
                    "missing_protocol",
                    f"Manifest protocol does not exist: {value.get('protocol_path')!r}",
                    relative,
                )
            )

        artifact_fields = (
            "physics_samples",
            "policy_samples",
            "scenario_summary",
            "policy_input_snapshot",
            "first_active_conditioned_actions",
            "render_capture",
        )
        for field in artifact_fields:
            artifact = value.get(field)
            if artifact is None:
                continue
            artifact_path = _resolve_manifest_path(root, path.parent, artifact)
            if artifact_path is None or not artifact_path.exists():
                issues.append(
                    Issue(
                        "error",
                        "missing_run_artifact",
                        f"Manifest field {field} points to a missing artifact: {artifact!r}",
                        relative,
                    )
                )
    return manifests


def _check_locked_scripted_schedule(root: Path, issues: list[Issue]) -> dict[str, Any] | None:
    protocol_path = root / "product-gates" / "scripted-locomotion.v1.json"
    source_path = (
        root
        / "PhysAnimUE5"
        / "Plugins"
        / "PhysAnimPlugin"
        / "Source"
        / "PhysAnimPlugin"
        / "Private"
        / "PhysAnimCausalStanding.Tests.cpp"
    )
    if not protocol_path.exists() or not source_path.exists():
        return None
    try:
        comparison = compare_scripted_schedule(
            _read_json(protocol_path), source_path.read_text(encoding="utf-8-sig")
        )
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        issues.append(
            Issue("error", "schedule_audit_failed", str(exc), _repo_relative(root, protocol_path))
        )
        return None
    if comparison["status"] != "MATCH":
        issues.append(
            Issue(
                "error",
                "locked_protocol_schedule_mismatch",
                "Runtime scripted phase boundaries differ from locked protocol v1: "
                + ", ".join(comparison["mismatched_phases"]),
                _repo_relative(root, source_path),
            )
        )
    return comparison


def audit_repository(root: Path, check_git: bool = True) -> dict[str, Any]:
    root = root.resolve()
    issues: list[Issue] = []
    experiments = _collect_experiments(root, issues, check_git)
    manifests = _collect_manifests(root, issues)
    schedule = _check_locked_scripted_schedule(root, issues)
    issue_values = [issue.as_dict() for issue in issues]
    errors = sum(issue["severity"] == "error" for issue in issue_values)
    warnings = sum(issue["severity"] == "warning" for issue in issue_values)
    return {
        "schema_version": AUDIT_SCHEMA,
        "root": str(root),
        "summary": {
            "experiment_count": len(experiments),
            "manifest_count": len(manifests),
            "error_count": errors,
            "warning_count": warnings,
            "status": "PASS" if errors == 0 else "FAIL",
        },
        "experiments": experiments,
        "manifests": manifests,
        "scripted_schedule": schedule,
        "issues": issue_values,
    }


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit experiment preregistrations, results, manifests, artifacts, and protocol linkage."
    )
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path)
    parser.add_argument("--no-git", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    result = audit_repository(args.root, check_git=not args.no_git)
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        sys.stdout.write(payload)
    return 0 if result["summary"]["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
