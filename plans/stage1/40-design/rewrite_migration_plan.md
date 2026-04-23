# Rewrite Migration Plan

## Purpose

This document defines what stays, what is wrapped, what is deleted, and what is shadow-run only during the continuous-balance rewrite.

## What Stays

- existing bridge startup and inference path
- existing code symbols where temporary compatibility is useful
- existing legacy handoff implementation as a compatibility subsystem during migration

## What Is Wrapped

- old handoff state-machine logic
- legacy labels used only for comparison or compatibility
- shell-heavy transition diagnostics that are still useful as secondary failure explanations

## What Is Being Deleted, Not Ported

- flip-centered success criteria
- handoff completion as the architectural center
- shell status as a success substitute
- any assumption that balance-critical topology changes are normal operation

## Shadow-Run Only

Run the new continuous-balance mode in parallel with the legacy mode until:

- instrumentation is trustworthy
- the authority matrix is enforced
- the new truth model is stable enough to explain failures honestly

Only then begin deleting old handoff logic.

## Recommended Order

1. truth model
2. authority matrix
3. instrumentation and acceptance
4. smallest always-simulated proximal prototype
5. shadow-run comparison with the legacy path
6. deletion of old handoff logic after trust is earned
