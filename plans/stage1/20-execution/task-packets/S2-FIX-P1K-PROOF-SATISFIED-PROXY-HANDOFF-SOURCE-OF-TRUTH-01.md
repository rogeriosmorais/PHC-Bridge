Task ID
S2-FIX-P1K-PROOF-SATISFIED-PROXY-HANDOFF-SOURCE-OF-TRUTH-01

Allowed files
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.StartStop.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationState.h
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationState.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTermination.h
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTermination.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationState.Tests.cpp
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp
plans/stage1/20-execution/task-packets/S2-FIX-P1K-PROOF-SATISFIED-PROXY-HANDOFF-SOURCE-OF-TRUTH-01.md
plans/stage1/20-execution/evidence/S2-FIX-P1K-PROOF-SATISFIED-PROXY-HANDOFF-SOURCE-OF-TRUTH-01.md
plans/stage1/20-execution/execution-log.md

Forbidden files
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp
PhysAnimRuntimeAdapter.*
PhysAnimRuntimeOrchestrator.*
PhysAnimRuntimeTerminationPipeline.*
PhysAnimValidators.*
PhysAnimSupportTruth.*
PhysAnimFailureArbitration.*
assets
ONNX files
PoseSearch tuning files
mass tuning files
PhysicsControl redesign files

Required work
1. Supersede the blocked proxy-timing portion of:
   S2-FIX-P1J-ACTIVATION-PATH-PROXY-TIMING-SURFACE-WIDEN-01

2. Fix only this current symptom:
   PhysAnim.ActivationPath.Wiring.ProofSatisfied
   fails during proof-satisfied activation with:
   ENTRY_DENIED reason=PROXY_OUTSIDE_HULL

3. This packet explicitly owns the proxy handoff source-of-truth boundary:
   - startup/reset timing
   - proof-complete -> standing-entry transition
   - startup proxy terminal recording
   - standing-entry proxy denial edge
   - post-standing proxy enforcement arming

4. Do not work on:
   - proof-disabled safe-deny path
   - proof gate truth criteria
   - support truth generation
   - validators
   - runtime adapter
   - runtime orchestrator
   - termination pipeline
   - failure arbitration
   - locomotion chain
   - tuning

5. Required proxy timing rule:
   - startup begins with proxy handoff disarmed
   - proof completion does not arm proxy handoff
   - leaving WaitingForPoseSearch does not arm proxy handoff
   - entering BalanceEntry_Settle does not arm proxy handoff
   - standing entry in progress may record PROXY_OUTSIDE_HULL but must not deny entry from it
   - proxy handoff arms only after standing entry is accepted
   - after proxy handoff is armed, fresh PROXY_OUTSIDE_HULL is enforceable

6. In PhysAnimComponent.StartStop.cpp:
   - reset proxy handoff arming state on start
   - reset proxy handoff arming state on stop/restart
   - clear stale startup proxy-entry denial state
   - do not clear audit evidence that a startup proxy terminal was deferred

7. In PhysAnimRuntimeTerminationState.* and PhysAnimRuntimeTermination.*:
   - add or refine only narrow state needed to distinguish:
     - recorded startup proxy terminal
     - deferred startup proxy terminal
     - enforceable post-handoff proxy terminal
   - do not redesign termination routing
   - do not erase terminal evidence
   - do not convert deferred proxy evidence into success

8. In PhysAnimComponent.cpp / PhysAnimComponent.Core.cpp / PhysAnimComponent.Balance.cpp:
   - locate the ENTRY_DENIED reason=PROXY_OUTSIDE_HULL path
   - prevent only startup-deferred proxy terminal evidence from denying standing entry
   - do not suppress non-proxy entry denials
   - do not suppress fresh post-handoff PROXY_OUTSIDE_HULL
   - arm proxy handoff immediately after standing entry acceptance
   - ensure stale startup proxy samples cannot be reused as fresh standing-active failures

9. In the activation wiring tests:
   - ProofSatisfied must assert:
     - fresh proof completed
     - standing entry was attempted
     - standing entry was not denied by startup-deferred PROXY_OUTSIDE_HULL
     - expected activation/standing path is reached
   - ProofNotSatisfied must still assert WaitingForPoseSearch is externally observable before fail-stop
   - neither test may accept early FailStopped as success

10. Add or extend regression coverage:
   - ProofSatisfied no longer fails with ENTRY_DENIED reason=PROXY_OUTSIDE_HULL
   - ProofSatisfied reaches expected activation/standing path
   - startup proxy terminal is recorded as deferred
   - startup-deferred proxy terminal cannot deny standing entry
   - proxy handoff starts disarmed after reset
   - proxy handoff arms only after standing entry acceptance
   - fresh post-handoff PROXY_OUTSIDE_HULL is enforced
   - StandingProof.NegativeSupport still fails with ActivationSupportFailure

11. Required logs:
   [PhysAnim] Proxy handoff reset state=<...>
   [PhysAnim] Startup proxy terminal recorded reason=PROXY_OUTSIDE_HULL state=<...>
   [PhysAnim] Startup proxy terminal deferred for standing entry
   [PhysAnim] Standing entry accepted proxy handoff armed state=<...>
   [PhysAnim] Proxy outside hull enforced after handoff armed

12. Evidence must include:
   - before case: ProofSatisfied failed with ENTRY_DENIED reason=PROXY_OUTSIDE_HULL
   - after case: ProofSatisfied passes
   - after case: ProofNotSatisfied still observes WaitingForPoseSearch before fail-stop
   - log excerpt showing startup proxy terminal recorded/deferred
   - log excerpt showing standing entry accepted before proxy handoff arms
   - log excerpt showing fresh post-handoff PROXY_OUTSIDE_HULL remains enforceable
   - confirmation that StandingProof.NegativeSupport still fails with ActivationSupportFailure

13. Update execution-log.md.

Tests
PowerShell
.\scripts\build.ps1
.\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring
.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofSatisfiedProxyHandoffSourceOfTruth
.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ActivationPathProxyTimingSurfaceWiden
.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProxyHandoffArmingTimingReset
.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofCompleteStandingEntryProxyTiming
.\scripts\build.ps1 -Test PhysAnim.StandingProof.Live
.\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport
.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-FIX-P1K-PROOF-SATISFIED-PROXY-HANDOFF-SOURCE-OF-TRUTH-01.md -WorkingTree -AllowExecutionLog -AllowEvidence
.\scripts\check_workflow_state.ps1 -Mode status -Strict

Stop conditions
1. Fix requires changing locomotion files.
2. Fix requires changing runtime adapter, support truth, validators, failure arbitration, orchestrator, or termination pipeline.
3. Fix requires tuning values.
4. ProofSatisfied still fails with ENTRY_DENIED reason=PROXY_OUTSIDE_HULL.
5. ProofSatisfied passes without fresh proof completion.
6. Startup-deferred PROXY_OUTSIDE_HULL is erased instead of recorded.
7. Startup-deferred PROXY_OUTSIDE_HULL still denies standing entry.
8. Proxy handoff is armed before standing entry acceptance.
9. PROXY_OUTSIDE_HULL is globally suppressed.
10. Fresh post-handoff PROXY_OUTSIDE_HULL is not enforced.
11. Non-proxy terminal reasons are deferred during standing entry.
12. ProofNotSatisfied no longer observes WaitingForPoseSearch before fail-stop.
13. StandingProof.Live regresses.
14. StandingProof.NegativeSupport no longer fails with ActivationSupportFailure.
15. JSON/audit artifact disagrees with component state.
