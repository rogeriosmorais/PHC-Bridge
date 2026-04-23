# Continuous Balance Architecture

## Purpose

This document defines the high-level architecture of the PhysAnim bridge. It establishes the structural body sets and the scope of the rewrite.

**Executable plant rules, truth thresholds, and terminal behaviors are owned by their respective contracts.**

## Rewrite Scope (V0 - V3)

- **V0 (Current)**: Proximal standing on feet; 120Hz truth / 30Hz reporting; Manny/Quinn default.
- **V1**: Full-body COM truth; expanded support set (hands/knees); dynamic pose transitions.
- **V2**: Locomotion-integrated balance; recovery policy activation.
- **V3**: General-purpose humanoid balance with arbitrary contact.

## Structural Body Sets

### 1. Balance-Critical Chain
The architectural center of the humanoid:
- `pelvis`, `spine_01`, `spine_02`, `spine_03`, `thigh_l`, `thigh_r`
- **Ownership**: Must be continuously simulated. Any bone removal or renaming triggers `activation_topology_change`.

### 2. Support Set
Bodies that provide valid, plantar support for the balance hull:
- `foot_l`, `foot_r`, `ball_l`, `ball_r`
- **Truth Role**: Every qualifying contact on these bodies contributes to the support hull.

### 3. Contamination Monitor Set
Bodies whose world contact is forbidden during the attempt:
- `calf_l`, `calf_r`
- **Truth Role**: World contact triggers terminal `activation_authority_conflict` as defined in [authority_matrix.md](authority_matrix.md). These bodies must remain clear of all world geometry to maintain the "Honest Standing" proof.

### 4. Upper-Body & Excluded Sets
- All bodies not in the critical or support sets.
- **V0 Rule**: Collision must be disabled to ensure isolation.

## Concepts

### 1. Standing-Reference (High Level)
The target stance is derived from authored animation data, not live poses. It provides the "Ideal" against which achievement is measured.

### 2. Support Model (High Level)
The bridge uses a planar projection model where stability is defined by the relationship between a proximal proxy (pseudo-COM) and a footprint (support hull) formed by active contacts.

### 3. Truth Sets vs. Effect Domain
- **Truth Sets**: The bodies whose physical state defines "Success" or "Failure".
- **Effect Domain**: The entire skeletal mesh and world context where interactions may materially contaminate the truth sets.

## Non-Goals
- Real-time full-body COM solving in `V0`.
- Arbitrary non-plantar support (e.g., sitting, leaning) in `V0`.
- Networked movement prediction during activation in `V0`.
- Sophisticated recovery/fall logic in `V0`.
