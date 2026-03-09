# Stage 1 User Intervention Checklist

## Purpose

This checklist defines the work that must be done by the human user rather than an AI agent.

## Current Known Local Setup

- UE version being installed: `5.7.3`
- Planned install root: `E:\UE_5.7`
- Planned `UE5_PATH` value after install: `E:\UE_5.7\Engine`

## User-Owned Actions

| ID | When | User Action | Why AI Cannot Finish It Alone | Evidence Expected Back |
|---|---|---|---|---|
| S1-U-01 | Before Phase 0 execution | Install external tools and confirm paths (`conda`, UE5, any required SDK/runtime pieces) | installation happens outside the repo and may require local machine choices | version list and resolved paths, including `UE 5.7.3` at `E:\UE_5.7` if that install succeeds |
| S1-U-02 | Before Phase 0 execution | Accept licenses and obtain external datasets/repos such as AMASS, Mixamo assets, ProtoMotions | licenses, credentials, and downloads may require human acceptance | confirmation that data/repos exist at the expected locations |
| S1-U-03 | Phase 0 | Create the UE5 project scaffold and perform editor-only setup for Manny, Physics Asset, and Physics Control Component | editor workflows cannot be fully completed by docs alone in this repo state | screenshots, notes, or project paths |
| S1-U-04 | Phase 0 | Run editor-only or visually judged feasibility checks for G1 | visual quality and editor behavior require human observation | short pass/fail note plus screenshots or clips |
| S1-U-05 | Phase 1 | Run the G2 side-by-side quality judgment | the gate is explicitly human-evaluated | comparison video, screenshots, and final judgment |
| S1-U-06 | Phase 2 | Run the G3 demo evaluation with observers | observer feedback cannot be synthesized credibly by an agent | observer notes and go/no-go decision |
| S1-U-07 | Any phase | Resolve blocked tradeoffs that change scope, architecture, or timeline | these are product decisions, not implementation details | explicit decision note |

## When To Escalate Immediately

- a required tool cannot be installed with the expected version
- a dataset or repo cannot be obtained legally or practically
- a UE5 editor workflow differs materially from the plan
- a quality gate depends on subjective judgment that the user has not provided yet
- a planning decision would violate the locked architecture

## What AI Should Never Assume

- that local tool installation succeeded without evidence
- that a license was accepted
- that a dataset is present and valid
- that a visual gate passed without human confirmation
- that the user wants to expand scope past the Stage 1 proof-of-concept
