# Causal Locomotion Metric Endpoints

`locomotion_causal_metrics.py` defines protocol-independent, interpretable measurements for scripted shell locomotion. The library itself does not hide the endpoints inside a weighted score. Immutable protocol files assign thresholds explicitly.

The endpoint set separates:

- shell path and net route progress;
- physical-root path length;
- physical-root progress projected onto the shell route;
- lateral physical-root displacement;
- root-to-shell progress ratio;
- tracking-error area under the curve, time average, final value, and maximum;
- forbidden assistance observations;
- exact bundle membership, including omissions, duplicates, and unexpected runs.

Synthetic adversarial tests cover:

- a moving shell with a stationary physical body;
- physical-root motion opposite the route;
- large lateral motion with little forward progress;
- healthy forward tracking;
- forbidden CharacterMovement or shell-helper assistance;
- malformed time ordering;
- cherry-picked bundles with missing, duplicated, or unexpected runs.

The classifications `STATUE`, `REVERSED`, `LATERAL_DIVERGENCE`, and `FORWARD_TRACKING` remain diagnostics rather than a substitute for the raw measurements.

## Protocol authority

`product-gates/scripted-locomotion.v3.json` preregisters the causal contract for the `Normal` variant. It requires forward tracking and independently gates projected route progress, root-to-shell progress ratio, lateral displacement, time-average tracking error, final tracking error, maximum tracking error, and forbidden assistance.

`evaluate_scripted_locomotion_protocol.py --evaluate-causal` validates protocol linkage before applying those gates. Locked v1 and v2 evidence remains readable and returns `NOT_APPLICABLE`; their meaning is not retroactively changed.
