# Pose Search Locomotion Corpus Audit

The dedicated Unreal automation test `PhysAnim.Development.PoseSearchCorpusAudit` audited all 16 directional unarmed locomotion sequences: eight walk clips and eight jog clips.

Results:

- All 16 assets loaded.
- Every clip's root-motion direction matched its locked authored-data direction with dot product greater than 0.999.
- Authored forward is animation-data `+Y`; authored left is `+X`.
- All clips had less than 2 degrees of unintended full-clip yaw.
- Walk speeds were approximately 300 cm/s.
- Jog speeds ranged from approximately 500 to 600 cm/s and exceeded their matching walk clips by more than 10 percent.
- The standard Manny mesh offset remains `-90 degrees`, which maps animation-data `+Y` to actor/world forward `+X`.

The machine-readable evidence is `pose-search-corpus-audit.v1.json`. The automation report recorded one success, zero warnings, and zero errors.
