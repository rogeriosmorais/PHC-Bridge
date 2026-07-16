# Causal Locomotion Metric Endpoints

`locomotion_causal_metrics.py` defines protocol-independent, interpretable measurements for scripted shell locomotion. It intentionally does not assign product thresholds or combine the endpoints into a weighted score.

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

The classifications `STATUE`, `REVERSED`, `LATERAL_DIVERGENCE`, and `FORWARD_TRACKING` are diagnostics. Protocol v2 should preregister numerical gates around the raw endpoints rather than treating the classification alone as acceptance.
