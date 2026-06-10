# graph-prd

Phase 0 — Transform a vague idea into a structured, import-ready PRD using 7 product methodologies: 5W2H, Jobs-to-be-Done, Pareto 80/20, MoSCoW, INVEST, Given-When-Then, and Risk Matrix.

## When to Use

- Starting from scratch with only a vague idea or feature concept
- Before the ANALYZE phase when no PRD document exists yet
- When you want a structured requirements elicitation process that covers all angles
- When you want a PRD that scores high on `analyze(mode:"prd_quality")`

## Mandatory Flow

```
5W2H → JTBD → Brainstorm+Pareto → MoSCoW → INVEST decomposition → Given-When-Then → Risk Matrix → Generate PRD.md → import_prd(dryRun) → $graph-analyze
```

Note: This skill is PRE-lifecycle. It does NOT call `set_phase`. The transition to ANALYZE happens via `$graph-analyze`.

## Workflow

### Step 1: Vision & Context (5W2H)

Ask the user the 7 structured questions — present them as a single block for the user to answer:

- **What** — What is the product/feature? What problem does it solve?
- **Why** — Why is this needed now? What's the business justification?
- **Who** — Who are the target users? Who are the stakeholders?
- **Where** — Where will this run? (platform, environment, infrastructure)
- **When** — What's the timeline? MVP deadline? Phased delivery?
- **How** — How will it work at a high level? Key technical approach?
- **How Much** — What are the resource constraints? Team size, budget, token limits?

After answers, synthesize a **Vision Statement** (2-3 sentences) and confirm with the user.

### Step 2: Jobs-to-be-Done (JTBD)

Based on 5W2H answers, extract user jobs:

- What "job" is the user hiring this product to do?
- What is the current workaround? (what gets "fired")
- What outcome defines success?

Format each job as: **"When [situation], I want to [motivation], so I can [expected outcome]."**

Present 2-5 JTBD statements for user review.

### Step 3: Feature Brainstorm & Pareto Analysis (80/20)

Ask the user to brainstorm ALL features (no filter, quantity over quality).

Then apply Pareto 80/20:
1. Rate each feature: **Value** (1-10) and **Effort** (1-10)
2. Calculate **Value/Effort ratio**
3. Present a sorted table
4. Highlight the **top 20%** by ratio — these are the Pareto winners

```
| Feature | Value | Effort | Ratio | Pareto? |
|---------|-------|--------|-------|---------|
| ...     | 9     | 3      | 3.0   | TOP 20% |
```

Ask user to confirm the shortlist.

### Step 4: MoSCoW Prioritization

Categorize ALL features from Step 3:

- **Must** — Cannot launch without (map to Priority 1-2)
- **Should** — Important but not blocking launch (Priority 3)
- **Could** — Nice to have, first to cut if constrained (Priority 4)
- **Won't** — Explicitly out of scope this version (excluded from PRD)

Present the categorization and ask user to adjust. Only Must + Should proceed to decomposition.

### Step 5: Epic & Story Decomposition (INVEST)

Group Must + Should features into **Epics** (become `##` headings in PRD). For each epic, decompose into **Tasks** (become `###` headings).

Validate each task against INVEST criteria:
- **I**ndependent — Can be developed without other tasks?
- **N**egotiable — Not over-specified? Room for implementation decisions?
- **V**aluable — Delivers user-facing value?
- **E**stimable — Can we size it? Assign `**Tamanho:** XS|S|M|L|XL`
- **S**mall — Completable in 1 sprint? If L/XL, decompose further
- **T**estable — Can we write acceptance criteria?

Add metadata to each task:
```
**Tamanho:** S
**Prioridade:** 2
**Tags:** auth, security
**Depende de:** Task 1.1
```

### Step 6: Acceptance Criteria (Given-When-Then)

For each task from Step 5, write testable acceptance criteria.

Primary format — **Given-When-Then**:
```
**Criterios de aceite:**
- GIVEN user is on login page WHEN enters valid credentials THEN receives JWT token
- GIVEN invalid password WHEN login attempted THEN shows error message
```

Alternative format — **checklist**:
```
**Criterios de aceite:**
- [ ] User can enter email and password
- [ ] System validates credentials against database
- [ ] JWT token returned with 1h expiry
```

Target: at least 2-3 AC per task. Every AC must be **testable** (concrete values, observable outcomes).

### Step 7: Risk & Constraint Analysis

**Risks** — Apply Risk Matrix (Probability x Impact):

```
| Risk | Probability | Impact | Severity | Mitigation |
|------|------------|--------|----------|------------|
| ...  | Alta       | Alto   | Critical | ...        |
```

Identify at least 2 risks. For each High/Critical risk, define a mitigation strategy.

**Constraints** — List technical, business, and regulatory constraints:
- Stack constraints (e.g., "SQLite only, no external infra")
- Performance constraints (e.g., "compression < 50ms")
- Compatibility constraints (e.g., "backward compatible, zero breaking changes")

Target: at least 2 constraints.

### Step 8: PRD Generation

Assemble the structured markdown file following the parser-compatible format:

```markdown
# PRD: <Title>

## Visao Geral
<Vision statement from Step 1>
<JTBD statements from Step 2>

## Fase 1 — <Phase Name>

### Epic: <Name>
<Description from Step 5>

#### Task N.M: <Title>
<Description>
**Tamanho:** S
**Prioridade:** 2
**Tags:** tag1, tag2
**Depende de:** Task X.Y
**Criterios de aceite:**
- GIVEN x WHEN y THEN z

## Riscos
### Risk: <Name>
<Description>. Probabilidade: Alta. Impacto: Alto.
Mitigacao: <strategy>

## Restricoes
### Constraint: <Name>
<Description>
```

**Critical format rules** (parser contract):
- `##` headings = Epics (parser maps level 2 to epics)
- `###` or `####` headings = Tasks (parser maps level 3-4 to tasks/subtasks)
- `**Criterios de aceite:**` label required for AC detection
- `**Tamanho:**`, `**Prioridade:**`, `**Depende de:**` for metadata extraction
- Risk/Constraint sections use keywords the classifier recognizes

Save to `docs/_internal/prd/<kebab-case-name>.md`.

### Step 9: Quality Validation (Dry Run)

Preview the PRD import without persisting:

```
Tool: mcp__mcp-graph__import_prd (filePath: "docs/_internal/prd/<name>.md", dryRun: true)
```

Review the preview:
- Epics detected correctly?
- Tasks have AC?
- Risks and constraints found?
- Dependencies inferred?

If issues found, iterate on the PRD file and re-run dry-run.

### Step 10: Transition

Present summary and guide user to the next phase:

```
PRD ready at docs/_internal/prd/<name>.md
Next step: Run $graph-analyze to import this PRD and begin the 9-phase lifecycle.
```

## Output Format

```
Phase: PRD (Phase 0 — Pre-lifecycle)
Methodologies: 5W2H, JTBD, Pareto 80/20, MoSCoW, INVEST, GWT, Risk Matrix
File: docs/_internal/prd/<name>.md
Epics: N defined
Tasks: M with acceptance criteria
Risks: K identified
Constraints: J defined
Dry-run: import_prd preview — X nodes extracted
Quality: prd_quality target score ≥ 70/100
Next: $graph-analyze
```

## Anti-Patterns

- Do NOT skip methodology steps — each feeds the next (5W2H → JTBD → Pareto → MoSCoW → INVEST → GWT → Risks)
- Do NOT write code or make architecture decisions — that belongs to the DESIGN phase
- Do NOT call `import_prd` without `dryRun: true` first — validate before committing to the graph
- Do NOT call `set_phase` — this skill is pre-lifecycle, transition happens via `$graph-analyze`
- Do NOT generate the PRD without user confirmation at each step — this is an interactive process
- Do NOT use `##` headings for tasks — the parser maps `##` to epics and `###`/`####` to tasks
- Do NOT skip risk/constraint sections — they account for 30% of the prd_quality score
- Do NOT omit `**Criterios de aceite:**` labels — the parser needs them to detect acceptance criteria
- Do NOT produce vague AC like "system works correctly" — use concrete Given-When-Then with measurable outcomes


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.
