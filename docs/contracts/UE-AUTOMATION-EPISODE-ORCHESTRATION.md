# UE Automation Episode Orchestration

`run_ue_automation_episode.ps1` is a single-episode execution wrapper for committed Unreal product fixtures. It does not evaluate product behavior. Its authority ends at fixture identity, process completion, and artifact completeness.

## Guarantees

Before launching Unreal, the wrapper:

- resolves and records the exact source commit;
- rejects dirty source unless explicitly allowed for development;
- requires a versioned protocol whose status is `LOCKED`;
- hashes the ONNX model;
- refuses to start while another Unreal Editor process is active;
- keeps outputs inside the repository;
- creates a unique run root and writes an execution plan.

During execution, it:

- starts exactly one child PowerShell process;
- invokes `build.ps1` through a generated typed splat, avoiding string-to-Boolean conversion errors;
- polls the same process instead of relaunching after bridge timeouts;
- applies a real wall-clock timeout;
- preserves stdout, stderr, partial artifacts, and orchestration state after every failure.

After execution, `validate_ue_automation_run.py` requires:

- an exact matching automation test path;
- clean automation success with zero warnings and errors;
- a manifest from the expected source commit, protocol, variant, and repetition;
- `source_tree_dirty=false`;
- every required manifest artifact to exist and be nonempty.

A wrapper `PASS` is **not** a product verdict. It means the product fixture completed with a trustworthy identity and complete evidence. The versioned protocol evaluator must still determine behavioral `PASS` or `FAIL`.

## Dry-run example

```powershell
.\scripts\run_ue_automation_episode.ps1 `
  -TestName PhysAnim.Product.ScriptedLocomotion.Normal `
  -ProtocolPath product-gates\scripted-locomotion.v2.json `
  -Variant Normal `
  -Repetition 1 `
  -DryRun
```

## Execution example

```powershell
.\scripts\run_ue_automation_episode.ps1 `
  -TestName PhysAnim.Product.ScriptedLocomotion.Normal `
  -ProtocolPath product-gates\scripted-locomotion.v2.json `
  -Variant Normal `
  -Repetition 1 `
  -TimeoutSeconds 900
```

Use `-SkipCompile` only when the exact source commit has already been compiled. Use `-AllowDirty` only for development diagnostics; the manifest and orchestration record will retain that fact and the validator will not grant a fixture `PASS` to a dirty run.
