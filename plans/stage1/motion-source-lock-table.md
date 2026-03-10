# Stage 1 Motion Source Lock Table

## Purpose

This file upgrades the motion-source plan into exact clip-level decisions once the real datasets are present.

Do not claim the Stage 1 motion set is fully sourced until every required row is either locked or explicitly removed.

## Status Meanings

- `planned`: source family decided, exact clip not yet locked
- `locked`: exact source clip or file is chosen
- `replaced`: original motion replaced with an approved alternative
- `removed`: motion explicitly removed from scope
- `blocked`: no acceptable source found yet

## Locomotion Core

| Motion | Final Source Family | Exact Clip / File | Conversion Needed? | Pretrained / Fine-Tune / Both | Status | Notes |
|---|---|---|---|---|---|---|
| idle / relaxed stand | pretrained + AMASS |  | no | pretrained | planned | |
| walk forward | pretrained + AMASS |  | no | pretrained | planned | |
| jog / run forward | pretrained + AMASS |  | no | pretrained | planned | |
| start moving | AMASS |  | no | both | planned | |
| stop moving | AMASS |  | no | both | planned | |
| turn left in place | AMASS |  | no | both | planned | |
| turn right in place | AMASS |  | no | both | planned | |
| short pivot while moving | AMASS |  | no | both | planned | |
| strafe left | AMASS or approved replacement |  | maybe | fine-tune | planned | |
| strafe right | AMASS or approved replacement |  | maybe | fine-tune | planned | |
| short recovery / rebalance step | AMASS or approved replacement |  | maybe | fine-tune | planned | |

## Lock Rule

Before G2, every motion used in the comparison sequence must be `locked`.
