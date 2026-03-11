# Architecture & Engineering Review: NewEngine

Based on a thorough review of `DESIGN.md`, `RESEARCH.md`, `ENGINEERING_PLAN.md`, and `STAGE1_PLAN.md`, the concept is incredibly ambitious and positions the engine perfectly for next-gen character simulation. 

However, there are a few **major flaws and critical risks** in the current Stage 1 plan that contradict the ultimate goals of the architecture. Below are the identified issues and concrete action items to resolve them.

---

## 1. The Idea Fast-Path (Sim-to-Sim Gap)

**The Flaw:** Assuming PD gains in Unreal Engine's Chaos Physics can simply be "hand-tuned" to match IsaacLab/IsaacGym's internal physics.
- **Why it's a risk:** IsaacLab and Chaos use fundamentally different solvers, joint constraint formulations, and integration steps. The PHC policy learned to walk based on the exact micro-forces of Isaac's simulation. Transferring this policy and expecting it to balance purely by tweaking UE5's `PhysicsControlComponent` spring/damper gains is mathematically unlikely to succeed. The character will likely fall over immediately.
- **Impact:** Stage 1 might stall completely at Gate 1 if the Sim-to-Sim gap cannot be bridged.

**Action Items / Solutions:**
*   **System ID Pipeline:** Instead of manual tuning, create a small script that records a sequence of forced joint rotations in both Chaos and IsaacLab. Use an optimizer (e.g., simple gradient descent or grid search) to find the Chaos PD gains that minimize the trajectory differences.
*   **Domain Randomization in Training:** If the policy must be robust to solver differences, introduce heavy domain randomization to the PD gains, masses, and friction coefficients *during* the IsaacLab/ProtoMotions training phase so the network doesn't overfit to Isaac's exact solver math.
*   **Residual Policy Fine-Tuning Setup:** Prepare a workflow to fine-tune the policy inside UE5 (even if it's off-policy using collected trajectories) if basic tuning fails.

---

## 2. Design Limitations (CPU-GPU-CPU Bottleneck)

**The Flaw:** In Stage 1, the pipeline requires constant PCI-e bus transfers that violate the "bypassing sequential bottlenecks" philosophy of the project.
- **Why it's a risk:** The loop is: `PoseSearch (CPU) -> NNE Inference (GPU) -> Chaos Physics (CPU)`. This requires marshaling data from the CPU to the GPU for the neural network, then pulling the resulting 69 float targets *back* from the GPU to the CPU every single frame to feed the `PhysicsControlComponent`.
- **Impact:** This round-trip sync will introduce severe latency and overhead. Stage 1's performance budget allows 0.4ms for inference and 1.0ms for Chaos. The PCI-e sync overhead alone may exceed this, causing frame drops.

**Action Items / Solutions:**
*   **Early Profiling (Phase 0):** Build a dummy `PhysAnimPlugin` immediately that just sends 131 floats to NNE and reads 69 floats back every frame at 60Hz. Profile the raw PCI-e readback latency before writing any actual control logic.
*   **Batched Inference:** While Stage 1 only has 2 characters, ensure the NNE call is batched for both characters simultaneously to minimize the number of API calls and dispatch overhead.
*   **CPU Inference Fallback:** Since the ONNX model is tiny (~1M parameters), test running inference on the CPU via ONNX Runtime `CPUExecutionProvider`. For only two characters, avoiding the PCI-e round trip entirely might actually be faster than GPU inference.

---

## 3. Engineering: Control Loop Desync

**The Flaw:** Running the policy at Render frequency (60Hz) while running physics at Substep frequency (120-240Hz).
- **Why it's a risk:** The engineering plan explicitly notes: *"PHC inference runs once per render frame; torque targets are held constant across substeps."* If the ragdoll receives new joint targets at 60Hz but Chaos evaluates physics at 240Hz, the joints will receive "stair-step" inputs.
- **Impact:** This introduces high-frequency jitter (robotic snapping) exactly at the start of each render frame. For PD controllers simulating organic muscle, inputs need to be interpolated smoothly across physics substeps, otherwise the character will vibrate violently.

**Action Items / Solutions:**
*   **Target Interpolation:** Modify the `PhysAnimPlugin` so it calculates the delta between the *previous* frame's NN output targets and the *current* frame's targets. During the Chaos physics substepping callback, linearly interpolate the targets being sent to the `PhysicsControlComponent` across the sub-ticks.
*   **Low-Pass Filtering:** Apply a simple EMA (Exponential Moving Average) filter to the NN outputs before applying them as control targets to smooth out any sudden jerks.

---

## 4. Retargeting Reality Check

**The Flaw:** Static 1:1 mapping table from the 24-joint SMPL skeleton to the 70-bone UE5 Mannequin.
- **Why it's a risk:** SMPL's simplification of the spine and shoulders is notorious. The UE5 Mannequin has clavicle bones, twist bones, and multi-segment spine chains. If PoseSearch drives the unmapped bones but the NN drives the mapped bones without a IK/retargeting solver layer to resolve the constraints between them, you will get severe visual artifacts (collapsing shoulders, twisted elbows). 
- **Impact:** Stage 1 gate holds "Does physics-driven motion look better?" to a high visual bar. Bad retargeting will make the outcome look worse than standard animation.

**Action Items / Solutions:**
*   **Use UE5 IK Rig for Retargeting:** Instead of a simple C++ static dictionary mapping, utilize UE5's built-in `IKRetargeter`. Pass the NN outputs into a virtual skeleton matching the SMPL hierarchy, and let the robust IK Rig solve the translation to the Manny skeleton, including handling twist bones and clavicles.
*   **Masked Blending:** For bones not predicted by the NN (fingers, facial, extra spine segments), use UE5's `BlendSpace` or `CopyBone` nodes to securely lock them to the underlying PoseSearch animation before the physics simulation takes over.
