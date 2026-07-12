The humanoid is not currently getting a fair standing attempt. The runtime rejects or corrupts the experiment before the PHC policy controls a fully simulated body.


  What Is Actually Broken


  1. Phase 1 falsely calls kinematic animation motion “physical instability.” The neck failed at only 5.06 deg/s, barely above an unrelated root threshold.
  2. World-space rotations are written into a parent-relative Physics Control API.
  3. Evidence mode changes whether policy targets are dispatched. Instrumentation is affecting behavior.
  4. Per-tick control tuning was accidentally removed. Root-on and settle gain ramps are therefore not applied.
  5. The policy permanently controls only eight lower-body targets. Spine and upper body outputs are discarded.
  6. Policy activation produces a recorded 178.4° target discontinuity.
  7. Intended topology is reported as actual topology without reading Physics Control or Chaos state.
  8. The pelvis tilt metric uses Manny’s local Z axis incorrectly, making an upright pelvis appear roughly 91° tilted.
  9. The original PHC action-to-PD transformation is not present authoritatively in this checkout. Existing documents contradict each other about PI * action versus normalized PD-
     range mapping.


  This is largely self-inflicted runtime complexity, not evidence that humanoid balance is beyond the model or engine.


  Implementation Plan


  1. Make observation truthful
      - Replace the Phase 1 upper-body velocity gate with actual movement-type, raw-simulation, reset, relative-pose, and control-readback checks.
      - Apply the existing sustained-violation grace period correctly.
      - Calibrate pelvis tilt against Manny’s captured neutral pelvis orientation
	  - Record actual state, never inferred intended state.


  2. Repair the target contract
      - Separate captured world transforms from parent-relative Physics Control targets.
      - Use explicit parent-relative targets throughout standing.
      - Seed every target from the current physical pose before enabling controls.
      - Preserve that seed while policy influence ramps, preventing the first-frame jump.
      - Make evidence flags completely observational.


  3. Replace the activation complexity
      - Reduce the 3,186-line transition machinery to preparation, atomic full-simulation activation, policy blend, and standing.
      - Enable every required Chaos body together.
      - Disable CharacterMovement, capsule assistance, shell correction, and kinematic rescue.
      - Make one function the sole owner of body movement types and gain publication.
      - Restore per-tick Physics Control tuning and verify its actual readback.

  4. Validate the physical plant independently
     Run the same full-simulation topology through this fixed ladder:
      - controls off
      - damping only
      - fixed neutral target
      - zero policy actions through the real action path
      - real ONNX policy


     Stop at the first failing layer. If controls-off explodes, repair Physics Asset masses, inertia, collisions, penetration, or constraints. If neutral hold fails, repair target
     spaces, actuator symmetry, and gains. Only evaluate the model after those pass.


  5. Restore full policy authority
      - Dispatch lower limbs and spine first, followed by neck/head and arms.
      - Remove the permanent lower-limb-only filter.
      - Verify output ordering, handedness, neutral reference, target range, readback, and action responsiveness.
      - Do not tune product thresholds during this work.


  6. Run the immutable product ladder
      - First require one development Normal run to reach the complete ten-second standing window.
      - Then require three clean Normal repetitions.
      - Finally run ZeroActions, DropControlDispatch, and ForcedSupportLoss.
      - Product success remains the existing perturbation recovery and causal comparison, not merely entering BalanceActive_Standing.


  Hard Limitation
  You selected the existing ONNX only. Therefore there is no guaranteed successful outcome.


  If the physical plant and control pipeline pass their independent tests but the existing ONNX still cannot outperform ZeroActions or dropped dispatch, the honest conclusion is
  that this artifact is incompatible or incapable. Without its authoritative actuator preprocessing data, checkpoint, or permission to replace it, further threshold changes would
  be another fake success.


  The first implementation slice should fix target spaces, evidence-independent dispatch, real Phase 1 validation, and per-tick tuning together. Until those are fixed, gain tuning
  and judgments about model capability are scientifically meaningless.