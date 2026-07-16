from __future__ import annotations

import json
from pathlib import Path

from scripts.environment_fingerprint import (
    build_fingerprint,
    sha256_file,
    sha256_locked_text_file,
)


def _write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def test_fingerprint_hashes_authority_files_and_extracts_protocol_settings(tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    protocol = repo / "product-gates" / "scripted-locomotion.v2.json"
    model = repo / "PhysAnimUE5" / "Content" / "NNEModels" / "phc_policy.onnx"
    uproject = repo / "PhysAnimUE5" / "PhysAnimUE5.uproject"
    plugin = repo / "PhysAnimUE5" / "Plugins" / "PhysAnimPlugin" / "PhysAnimPlugin.uplugin"
    config = repo / "PhysAnimUE5" / "Config" / "DefaultEngine.ini"
    lock = repo / "package-lock.json"
    _write_json(
        protocol,
        {
            "status": "LOCKED",
            "protocol_id": "scripted-causal-locomotion",
            "version": 2,
            "script": {"fixed_delta_time_sec": 1.0 / 60.0},
            "determinism": {"seed": 7, "threading": "single"},
        },
    )
    model.parent.mkdir(parents=True, exist_ok=True)
    model.write_bytes(b"model")
    _write_json(uproject, {"EngineAssociation": "5.7"})
    _write_json(plugin, {"VersionName": "1.2.3"})
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text("[/Script/Engine.PhysicsSettings]\nbSubstepping=True\n", encoding="utf-8")
    _write_json(lock, {"lockfileVersion": 3})

    result = build_fingerprint(
        repo_root=repo,
        protocol_path=protocol,
        model_path=model,
        source_commit="a" * 40,
        source_tree_dirty=False,
    )

    assert result["schema_version"] == "physanim-environment-fingerprint/v1"
    assert result["source"]["commit"] == "a" * 40
    assert result["unreal"]["engine_association"] == "5.7"
    assert result["protocol"]["fixed_delta_time_sec"] == 1.0 / 60.0
    assert result["protocol"]["determinism"]["seed"] == 7
    assert result["artifacts"]["model_onnx_sha256"] == sha256_file(model)
    assert result["artifacts"]["protocol_sha256"] == sha256_locked_text_file(protocol)
    assert result["authority_digest_sha256"]


def test_locked_protocol_hash_is_line_ending_independent(tmp_path: Path) -> None:
    lf = tmp_path / "lf.json"
    crlf = tmp_path / "crlf.json"
    lf.write_bytes(b'{\n  "status": "LOCKED"\n}\n')
    crlf.write_bytes(b'{\r\n  "status": "LOCKED"\r\n}\r\n')

    assert sha256_locked_text_file(lf) == sha256_locked_text_file(crlf)
    assert sha256_file(lf) != sha256_file(crlf)


def test_automation_report_device_is_included(tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    protocol = repo / "protocol.json"
    model = repo / "model.onnx"
    _write_json(protocol, {"status": "LOCKED", "protocol_id": "x", "version": 1})
    model.write_bytes(b"model")
    report = repo / "index.json"
    _write_json(
        report,
        {
            "devices": [
                {
                    "deviceName": "machine",
                    "platform": "WindowsEditor",
                    "oSVersion": "Windows",
                    "gPU": "GPU",
                    "cPUModel": "CPU",
                    "rAMInGB": 32,
                    "renderMode": "SM6",
                }
            ]
        },
    )

    result = build_fingerprint(
        repo_root=repo,
        protocol_path=protocol,
        model_path=model,
        source_commit="b" * 40,
        source_tree_dirty=True,
        automation_report_path=report,
    )

    assert result["device"]["gpu"] == "GPU"
    assert result["device"]["cpu"] == "CPU"
    assert result["device"]["ram_gb"] == 32
    assert result["source"]["tree_dirty"] is True


def test_authority_digest_changes_when_model_changes(tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    protocol = repo / "protocol.json"
    model = repo / "model.onnx"
    _write_json(protocol, {"status": "LOCKED", "protocol_id": "x", "version": 1})
    model.write_bytes(b"one")
    first = build_fingerprint(
        repo_root=repo,
        protocol_path=protocol,
        model_path=model,
        source_commit="c" * 40,
        source_tree_dirty=False,
    )
    model.write_bytes(b"two")
    second = build_fingerprint(
        repo_root=repo,
        protocol_path=protocol,
        model_path=model,
        source_commit="c" * 40,
        source_tree_dirty=False,
    )

    assert first["authority_digest_sha256"] != second["authority_digest_sha256"]
