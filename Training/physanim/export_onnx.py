from __future__ import annotations

import argparse
import json
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import numpy as np
import onnx
import onnxruntime as ort
import torch
from torch import nn


REPO_ROOT = Path(__file__).resolve().parents[2]
TRAINING_ROOT = REPO_ROOT / "Training"
PROTOMOTIONS_ROOT = TRAINING_ROOT / "ProtoMotions"

DEFAULT_CHECKPOINT_PATH = (
    PROTOMOTIONS_ROOT / "data" / "pretrained_models" / "motion_tracker" / "smpl" / "last.ckpt"
)
DEFAULT_OUTPUT_PATH = TRAINING_ROOT / "output" / "phc_policy.onnx"
DEFAULT_UE_IMPORT_PATH = TRAINING_ROOT.parent / "PhysAnimUE5" / "Content" / "NNEModels" / "phc_policy.onnx"


def _ensure_protomotions_imports() -> None:
    if str(PROTOMOTIONS_ROOT) not in sys.path:
        sys.path.insert(0, str(PROTOMOTIONS_ROOT))

    from protomotions.utils import config_utils  # noqa: F401


def _infer_config_path(checkpoint_path: Path) -> Path:
    direct = checkpoint_path.parent / "config.yaml"
    if direct.exists():
        return direct

    parent = checkpoint_path.parent.parent / "config.yaml"
    if parent.exists():
        return parent

    raise FileNotFoundError(f"Could not find config.yaml for checkpoint {checkpoint_path}")


@dataclass(frozen=True)
class Stage1PolicySpec:
    checkpoint_path: Path
    config_path: Path
    output_path: Path
    ue_import_path: Path
    input_names: tuple[str, ...]
    input_dims: dict[str, int]
    output_name: str
    output_dim: int
    opset_fallbacks: tuple[int, ...] = (17, 16, 15)

    @classmethod
    def default(
        cls,
        checkpoint_path: Path | None = None,
        config_path: Path | None = None,
        output_path: Path | None = None,
        ue_import_path: Path | None = None,
    ) -> "Stage1PolicySpec":
        checkpoint_path = Path(checkpoint_path or DEFAULT_CHECKPOINT_PATH)
        config_path = Path(config_path or _infer_config_path(checkpoint_path))
        output_path = Path(output_path or DEFAULT_OUTPUT_PATH)
        ue_import_path = Path(ue_import_path or DEFAULT_UE_IMPORT_PATH)
        return cls(
            checkpoint_path=checkpoint_path,
            config_path=config_path,
            output_path=output_path,
            ue_import_path=ue_import_path,
            input_names=("self_obs", "mimic_target_poses", "terrain"),
            input_dims={
                "self_obs": 358,
                "mimic_target_poses": 6495,
                "terrain": 256,
            },
            output_name="actions",
            output_dim=69,
        )

    def make_dummy_tensor(self, input_name: str, batch_size: int = 1) -> torch.Tensor:
        return torch.zeros((batch_size, self.input_dims[input_name]), dtype=torch.float32)

    def make_dummy_inputs(self, batch_size: int = 1) -> tuple[torch.Tensor, ...]:
        return tuple(self.make_dummy_tensor(name, batch_size=batch_size) for name in self.input_names)

    def make_random_inputs(self, seed: int, batch_size: int = 1) -> dict[str, np.ndarray]:
        rng = np.random.default_rng(seed)
        return {
            name: rng.standard_normal((batch_size, dim)).astype(np.float32)
            for name, dim in self.input_dims.items()
        }


class Stage1PolicyWrapper(nn.Module):
    def __init__(self, model: nn.Module):
        super().__init__()
        self.model = model
        self.actor = model._actor
        self.obs_mlp = self.actor.mu.input_models["obs_mlp"]
        self.mimic_target_poses_mlp = self.actor.mu.input_models["mimic_target_poses"]
        self.terrain_mlp = self.actor.mu.input_models["terrain"]
        self.transformer = self.actor.mu.seqTransEncoder
        self.output_model = self.actor.mu.output_model
        self.num_future_steps = 15
        self.num_obs_per_target_pose = 433

    def forward(
        self,
        self_obs: torch.Tensor,
        mimic_target_poses: torch.Tensor,
        terrain: torch.Tensor,
    ) -> torch.Tensor:
        obs_token = self.obs_mlp({"self_obs": self_obs}).unsqueeze(1)

        batch_size = self_obs.shape[0]
        mimic_tokens = mimic_target_poses.reshape(
            batch_size * self.num_future_steps,
            self.num_obs_per_target_pose,
        )
        mimic_tokens = self.mimic_target_poses_mlp(
            {"mimic_target_poses": mimic_tokens}
        ).reshape(batch_size, self.num_future_steps, -1)

        terrain_token = self.terrain_mlp({"terrain": terrain}).unsqueeze(1)

        cat_obs = torch.cat([obs_token, mimic_tokens, terrain_token], dim=1)
        cat_obs = cat_obs.permute(1, 0, 2).contiguous()
        encoded = self.transformer(cat_obs)[0]
        mu = self.output_model(encoded)
        return torch.tanh(mu)


@dataclass
class LoadedStage1Policy:
    spec: Stage1PolicySpec
    model: nn.Module
    wrapper: Stage1PolicyWrapper


@dataclass(frozen=True)
class ParityResult:
    output_name: str
    max_abs_diff: float
    mean_abs_diff: float


def load_stage1_policy(
    checkpoint_path: Path | None = None,
    config_path: Path | None = None,
    output_path: Path | None = None,
    ue_import_path: Path | None = None,
) -> LoadedStage1Policy:
    _ensure_protomotions_imports()

    from hydra.utils import instantiate
    from omegaconf import OmegaConf

    spec = Stage1PolicySpec.default(
        checkpoint_path=checkpoint_path,
        config_path=config_path,
        output_path=output_path,
        ue_import_path=ue_import_path,
    )
    config = OmegaConf.load(spec.config_path)
    model = instantiate(config.agent.config.model)

    checkpoint = torch.load(spec.checkpoint_path, map_location="cpu")
    model_state = checkpoint["model"]
    model.load_state_dict(model_state, strict=True)
    model.eval()

    wrapper = Stage1PolicyWrapper(model)
    wrapper.eval()
    return LoadedStage1Policy(spec=spec, model=model, wrapper=wrapper)


def export_stage1_policy_to_onnx(
    loaded: LoadedStage1Policy,
    output_path: Path | None = None,
    opset: int | None = None,
) -> int:
    output_path = Path(output_path or loaded.spec.output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    candidate_opsets: Iterable[int]
    if opset is None:
        candidate_opsets = loaded.spec.opset_fallbacks
    else:
        candidate_opsets = (opset,)

    dummy_inputs = loaded.spec.make_dummy_inputs(batch_size=1)
    last_error: Exception | None = None

    for candidate in candidate_opsets:
        try:
            torch.onnx.export(
                loaded.wrapper,
                dummy_inputs,
                output_path,
                export_params=True,
                do_constant_folding=True,
                input_names=list(loaded.spec.input_names),
                output_names=[loaded.spec.output_name],
                opset_version=candidate,
            )
            onnx_model = onnx.load(output_path)
            onnx.checker.check_model(onnx_model)
            return candidate
        except Exception as exc:  # pragma: no cover - exercised only on fallback failure
            last_error = exc
            if output_path.exists():
                output_path.unlink()

    raise RuntimeError(f"Failed to export ONNX with opsets {tuple(candidate_opsets)}") from last_error


def compare_pytorch_and_onnx(
    loaded: LoadedStage1Policy,
    output_path: Path,
    seed: int = 0,
) -> ParityResult:
    input_arrays = loaded.spec.make_random_inputs(seed=seed)
    input_tensors = {
        name: torch.from_numpy(array)
        for name, array in input_arrays.items()
    }

    with torch.no_grad():
        pytorch_output = loaded.wrapper(
            input_tensors["self_obs"],
            input_tensors["mimic_target_poses"],
            input_tensors["terrain"],
        ).detach().cpu().numpy()

    session = ort.InferenceSession(
        str(output_path),
        providers=["CPUExecutionProvider"],
    )
    onnx_output = session.run([loaded.spec.output_name], input_arrays)[0]

    abs_diff = np.abs(pytorch_output - onnx_output)
    return ParityResult(
        output_name=loaded.spec.output_name,
        max_abs_diff=float(abs_diff.max()),
        mean_abs_diff=float(abs_diff.mean()),
    )


def copy_onnx_to_ue_import_path(
    onnx_path: Path,
    ue_import_path: Path | None = None,
) -> Path:
    target_path = Path(ue_import_path or DEFAULT_UE_IMPORT_PATH)
    target_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(onnx_path, target_path)
    return target_path


def write_export_report(
    report_path: Path,
    spec: Stage1PolicySpec,
    accepted_opset: int,
    parity: ParityResult | None,
) -> None:
    report = {
        "checkpoint_path": str(spec.checkpoint_path),
        "config_path": str(spec.config_path),
        "onnx_path": str(spec.output_path),
        "ue_import_path": str(spec.ue_import_path),
        "accepted_opset": accepted_opset,
        "input_dims": spec.input_dims,
        "output_name": spec.output_name,
        "output_dim": spec.output_dim,
        "parity": None
        if parity is None
        else {
            "output_name": parity.output_name,
            "max_abs_diff": parity.max_abs_diff,
            "mean_abs_diff": parity.mean_abs_diff,
        },
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export the Stage 1 PHC policy to ONNX.")
    parser.add_argument("--checkpoint", type=Path, default=DEFAULT_CHECKPOINT_PATH)
    parser.add_argument("--config", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT_PATH)
    parser.add_argument("--ue-output", type=Path, default=DEFAULT_UE_IMPORT_PATH)
    parser.add_argument("--opset", type=int, default=None)
    parser.add_argument("--skip-parity", action="store_true")
    parser.add_argument("--copy-to-ue", action="store_true")
    parser.add_argument("--report", type=Path, default=None)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    loaded = load_stage1_policy(
        checkpoint_path=args.checkpoint,
        config_path=args.config,
        output_path=args.output,
        ue_import_path=args.ue_output,
    )
    accepted_opset = export_stage1_policy_to_onnx(
        loaded,
        output_path=args.output,
        opset=args.opset,
    )

    parity = None if args.skip_parity else compare_pytorch_and_onnx(loaded, args.output, seed=7)

    if args.copy_to_ue:
        copy_onnx_to_ue_import_path(args.output, args.ue_output)

    report_path = args.report or args.output.with_suffix(".export.json")
    write_export_report(report_path, loaded.spec, accepted_opset, parity)

    print(f"Exported ONNX to {args.output}")
    print(f"Accepted opset: {accepted_opset}")
    if parity is not None:
        print(f"Parity max_abs_diff={parity.max_abs_diff:.8f}")
        print(f"Parity mean_abs_diff={parity.mean_abs_diff:.8f}")
    if args.copy_to_ue:
        print(f"Copied ONNX to {args.ue_output}")
    print(f"Report written to {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
