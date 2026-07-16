from __future__ import annotations

import fnmatch
import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any, Mapping, Sequence


REPO_ROOT = Path(__file__).resolve().parents[1]
POLICY_PATH = REPO_ROOT / "product-gates" / "impact-policy.v1.json"
EXPECTED_POLICY_SHA256 = "f0e2e3ef0ea9118ebe29869646bdf445f78db0f4861f0aea0fbe4f7adc746d1b"


class ProductImpactConfigurationError(RuntimeError):
    pass


def canonicalize_locked_policy_bytes(policy_bytes: bytes) -> bytes:
    """Normalize only platform line endings before verifying a locked text policy."""
    return policy_bytes.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


@dataclass(frozen=True)
class ProductImpactResult:
    changed_paths: tuple[str, ...]
    product_paths: tuple[str, ...]
    non_product_paths: tuple[str, ...]
    unknown_paths: tuple[str, ...]
    product_impacting: bool
    implementation_completion_allowed: bool
    may_claim_product_success_without_receipt: bool
    policy_sha256: str


def _reject_duplicate_keys(pairs: Sequence[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ProductImpactConfigurationError(f"duplicate policy key: {key}")
        result[key] = value
    return result


def _load_policy() -> tuple[Mapping[str, Any], str]:
    policy_bytes = canonicalize_locked_policy_bytes(POLICY_PATH.read_bytes())
    digest = hashlib.sha256(policy_bytes).hexdigest()
    if digest != EXPECTED_POLICY_SHA256:
        raise ProductImpactConfigurationError(
            "locked product-impact policy digest mismatch; append a new version"
        )
    try:
        policy = json.loads(
            policy_bytes.decode("utf-8"), object_pairs_hook=_reject_duplicate_keys
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProductImpactConfigurationError(f"invalid product-impact policy: {exc}") from exc
    if not isinstance(policy, dict):
        raise ProductImpactConfigurationError("product-impact policy must be an object")
    if policy.get("schema_version") != "physanim-product-impact-policy/v1":
        raise ProductImpactConfigurationError("unsupported product-impact policy schema")
    if policy.get("status") != "LOCKED":
        raise ProductImpactConfigurationError("product-impact policy is not locked")
    if policy.get("unknown_path_policy") != "PRODUCT_IMPACTING":
        raise ProductImpactConfigurationError("unknown paths must fail closed")
    for field in ("product_patterns", "non_product_patterns"):
        patterns = policy.get(field)
        if not isinstance(patterns, list) or not patterns:
            raise ProductImpactConfigurationError(f"{field} must be a non-empty list")
        if any(not isinstance(pattern, str) or not pattern for pattern in patterns):
            raise ProductImpactConfigurationError(f"{field} contains an invalid pattern")
    return policy, digest


def _normalize_path(path: str) -> str:
    if not isinstance(path, str) or not path.strip():
        raise ProductImpactConfigurationError("changed paths must be non-empty strings")
    normalized = path.replace("\\", "/").strip()
    if normalized.startswith("/") or re.match(r"^[A-Za-z]:/", normalized):
        raise ProductImpactConfigurationError(f"absolute changed path is forbidden: {path}")
    pure = PurePosixPath(normalized)
    if ".." in pure.parts:
        raise ProductImpactConfigurationError(f"changed path escapes the repository: {path}")
    canonical = pure.as_posix()
    if canonical in {"", "."}:
        raise ProductImpactConfigurationError("changed path resolves to the repository root")
    return canonical


def _matches(path: str, patterns: Sequence[str]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def classify_product_impact(paths: Sequence[str]) -> ProductImpactResult:
    policy, policy_digest = _load_policy()
    changed_paths = tuple(dict.fromkeys(_normalize_path(path) for path in paths))
    product_patterns = policy["product_patterns"]
    non_product_patterns = policy["non_product_patterns"]
    product_paths: list[str] = []
    non_product_paths: list[str] = []
    unknown_paths: list[str] = []

    for path in changed_paths:
        if _matches(path, product_patterns):
            product_paths.append(path)
        elif _matches(path, non_product_patterns):
            non_product_paths.append(path)
        else:
            unknown_paths.append(path)

    product_impacting = bool(product_paths or unknown_paths)
    return ProductImpactResult(
        changed_paths=changed_paths,
        product_paths=tuple(product_paths),
        non_product_paths=tuple(non_product_paths),
        unknown_paths=tuple(unknown_paths),
        product_impacting=product_impacting,
        implementation_completion_allowed=not product_impacting,
        may_claim_product_success_without_receipt=False,
        policy_sha256=policy_digest,
    )
