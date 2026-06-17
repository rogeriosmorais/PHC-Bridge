# PRD: Parameterized Dumbbell Load Test Simulation

## Visao Geral
**What:** A parameterized, headless automation test in Unreal Engine that simulates attaching a dumbbell of varying masses to the character's hand during the Stage 2A balance proof.
**Why:** To provide empirical, undeniable proof of the AI's physical resistance (muscular response) to external forces, validating the PD controller's interaction with the neural policy without relying on subjective visual inspection.
**Who:** AI engineers and automated CI/CD pipelines.
**Where:** Inside the `PhysAnimStandingProof` functional test suite, running in the `Lvl_ThirdPerson` map.
**When:** Immediate implementation during the ANALYZE -> HANDOFF cycle.
**How:** Using `IMPLEMENT_COMPLEX_AUTOMATION_TEST` to spawn a `AStaticMeshActor` dumbbell, attach it via a `UPhysicsConstraintComponent` to the character's hand (`hand_r`), and loop through a predefined set of mass overrides (0kg, 5kg, 10kg, 20kg).
**How Much:** Minimal scope, leveraging existing Stage 2A durable artifact and truth arbitration telemetry.

## Epic: Automated Load Testing Framework

### Task: Implement Complex Automation Test Scaffold
Create the parameterized test scaffold in `PhysAnimStandingProof.FunctionalTests.cpp`.
**Tamanho:** S
**Prioridade:** 1
**Criterios de aceite:**
- Test runner accepts a list of string parameters representing dumbbell weights in kg (e.g., "0", "5", "15").
- The test correctly locates the `ACharacter` in the loaded map.

### Task: Implement Dumbbell Spawn and Attachment Logic
Implement a latent command to dynamically spawn and attach the physics-simulated dumbbell to the character.
**Tamanho:** M
**Prioridade:** 1
**Depende de:** Implement Complex Automation Test Scaffold
**Criterios de aceite:**
- GIVEN the test is initializing, WHEN the latent command runs, THEN a static mesh actor is spawned and set to simulate physics.
- The mass of the static mesh actor is overridden based on the test parameter.
- A `UPhysicsConstraintComponent` is created, linking the dumbbell to the character's `hand_r` bone.

### Task: Execute and Collect Load Artifacts
Run the simulation and ensure the `ThighNetWork` and `ActionMagnitudeVariance` are captured for each weight variant.
**Tamanho:** S
**Prioridade:** 1
**Depende de:** Implement Dumbbell Spawn and Attachment Logic
**Criterios de aceite:**
- GIVEN the test is executing, WHEN the 3.35s duration completes, THEN a terminal JSON artifact is emitted.
- The terminal artifact contains the `ThighNetWork` value corresponding to the specific dumbbell load.

## Riscos
### Risk: Unreal Physics Constraint Instability
At very high masses (e.g., 30kg+), the `UPhysicsConstraintComponent` may become violently unstable and cause the character to explode before the AI can react.
**Probabilidade:** Alta. **Impacto:** Medio.
**Mitigacao:** Cap the maximum test weight at 20kg or increase constraint solver iteration counts.

## Restricoes
### Constraint: Headless Execution Standard
The test must run entirely headless without requiring human visual intervention or the creation of new blueprint assets.
### Constraint: Leverage Existing Telemetry
The test must rely exclusively on the existing `FPhysAnimRunArtifactSnapshot` and `collect_evidence.py` pipeline for verification; no new bespoke telemetry structures should be created for this specific load test.
