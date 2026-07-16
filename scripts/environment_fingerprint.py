from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
from pathlib import Path
from typing import Any

SCHEMA = "physanim-environment-fingerprint/v1"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sha256_locked_text_file(path: Path) -> str:
    normalized = path.read_bytes().replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    return hashlib.sha256(normalized).hexdigest()


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def _optional_hash(path: Path) -> str | None:
    return sha256_file(path) if path.is_file() else None


def _git(repo_root: Path, *args: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repo_root), *args],
        check=True,
        capture_output=True,
        text=True,
    )
    return completed.stdout.strip()


def _extract_device(report_path: Path | None) -> dict[str, Any]:
    if report_path and report_path.is_file():
        report = _read_json(report_path)
        devices = report.get("devices")
        if isinstance(devices, list) and devices and isinstance(devices[0], dict):
            source = devices[0]
            return {
                "device_name": source.get("deviceName"),
                "platform": source.get("platform"),
                "os_version": source.get("oSVersion"),
                "gpu": source.get("gPU"),
                "cpu": source.get("cPUModel"),
                "ram_gb": source.get("rAMInGB"),
                "render_mode": source.get("renderMode"),
                "rhi": source.get("rHI"),
                "source": "unreal_automation_report",
            }
    return {
        "device_name": platform.node(),
        "platform": platform.platform(),
        "os_version": platform.version(),
        "gpu": None,
        "cpu": platform.processor() or os.environ.get("PROCESSOR_IDENTIFIER"),
        "ram_gb": None,
        "render_mode": None,
        "rhi": None,
        "source": "host_python_fallback",
    }


def build_fingerprint(
    *,
    repo_root: Path,
    protocol_path: Path,
    model_path: Path,
    source_commit: str,
    source_tree_dirty: bool,
    automation_report_path: Path | None = None,
) -> dict[str, Any]:
    repo_root = repo_root.resolve()
    protocol_path = protocol_path.resolve()
    model_path = model_path.resolve()
    protocol = _read_json(protocol_path)

    uproject_path = repo_root / "PhysAnimUE5" / "PhysAnimUE5.uproject"
    plugin_path = (
        repo_root
        / "PhysAnimUE5"
        / "Plugins"
        / "PhysAnimPlugin"
        / "PhysAnimPlugin.uplugin"
    )
    engine_config_path = repo_root / "PhysAnimUE5" / "Config" / "DefaultEngine.ini"
    game_config_path = repo_root / "PhysAnimUE5" / "Config" / "DefaultGame.ini"
    package_lock_path = repo_root / "package-lock.json"
    uproject = _read_json(uproject_path) if uproject_path.is_file() else {}
    plugin = _read_json(plugin_path) if plugin_path.is_file() else {}

    script = protocol.get("script") if isinstance(protocol.get("script"), dict) else {}
    determinism = (
        protocol.get("determinism")
        if isinstance(protocol.get("determinism"), dict)
        else {}
    )
    artifact_hashes = {
        "protocol_sha256": sha256_locked_text_file(protocol_path),
        "model_onnx_sha256": sha256_file(model_path),
        "uproject_sha256": _optional_hash(uproject_path),
        "plugin_descriptor_sha256": _optional_hash(plugin_path),
        "default_engine_ini_sha256": _optional_hash(engine_config_path),
        "default_game_ini_sha256": _optional_hash(game_config_path),
        "package_lock_sha256": _optional_hash(package_lock_path),
    }
    authority_material = {
        "source_commit": source_commit,
        "source_tree_dirty": source_tree_dirty,
        "protocol_id": protocol.get("protocol_id"),
        "protocol_version": protocol.get("version"),
        "artifact_hashes": artifact_hashes,
        "fixed_delta_time_sec": script.get("fixed_delta_time_sec"),
        "determinism": determinism,
    }
    authority_digest = hashlib.sha256(
        json.dumps(
            authority_material,
            sort_keys=True,
            separators=(",", ":"),
            ensure_ascii=True,
        ).encode("utf-8")
    ).hexdigest()

    return {
        "schema_version": SCHEMA,
        "source": {
            "commit": source_commit,
            "tree_dirty": source_tree_dirty,
        },
        "protocol": {
            "path": str(protocol_path),
            "id": protocol.get("protocol_id"),
            "version": protocol.get("version"),
            "status": protocol.get("status"),
            "fixed_delta_time_sec": script.get("fixed_delta_time_sec"),
            "determinism": determinism,
        },
        "unreal": {
            "engine_association": uproject.get("EngineAssociation"),
            "plugin_version": plugin.get("VersionName"),
            "uproject_path": str(uproject_path),
        },
        "runtime": {
            "python_version": platform.python_version(),
            "python_implementation": platform.python_implementation(),
            "machine_architecture": platform.machine(),
        },
        "device": _extract_device(automation_report_path),
        "artifacts": artifact_hashes,
        "authority_digest_sha256": authority_digest,
        "determinism_levels": {
            "byte": "Exact hashes of selected raw evidence streams and authority artifacts.",
            "numerical": "Element-wise or metric equality within a preregistered tolerance.",
            "behavioral": "Identical protocol verdict and bounded endpoint variation.",
            "process_scope": "Must state same-process, cross-process, or cross-machine scope.",
        },
    }


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a machine-readable fingerprint for an authoritative UE run.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--protocol", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--source-commit")
    parser.add_argument("--source-tree-dirty", action="store_true")
    parser.add_argument("--automation-report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    repo_root = args.repo_root.resolve()
    source_commit = args.source_commit or _git(repo_root, "rev-parse", "HEAD")
    source_tree_dirty = args.source_tree_dirty
    if args.source_commit is None and not args.source_tree_dirty:
        source_tree_dirty = bool(_git(repo_root, "status", "--porcelain=v1"))
    protocol = args.protocol if args.protocol.is_absolute() else repo_root / args.protocol
    model = args.model if args.model.is_absolute() else repo_root / args.model
    report = args.automation_report
    if report and not report.is_absolute():
        report = repo_root / report
    result = build_fingerprint(
        repo_root=repo_root,
        protocol_path=protocol,
        model_path=model,
        source_commit=source_commit,
        source_tree_dirty=source_tree_dirty,
        automation_report_path=report,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
