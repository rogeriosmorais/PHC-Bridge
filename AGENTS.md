# AGENTS.md

This is a Windows machine. Use PowerShell commands. Do not use Linux shell commands.

## Project

PHC-Bridge is a UE5 proof-of-concept bridge between an offline-trained PHC-family policy and Unreal Engine runtime systems.

Primary goal:
- drive a physics-based humanoid in UE5 from a neural policy

Secondary goals:
- keep the UE bridge small
- maximize reuse of UE5 built-ins
- preserve a clean separation between offline training and runtime inference

## Architecture Lock

Do not change this architecture unless explicitly asked for an architecture review.

`PoseSearch -> PHC Policy (NNE/ONNX) -> Physics Control Component -> Chaos Physics -> Renderer`

Interpretation:
- motion selection/search belongs to PoseSearch
- policy inference belongs to UE5 NNE with ONNX Runtime
- low-level actuation belongs to Physics Control
- simulation belongs to Chaos
- training belongs outside UE5

## Hard Rules

1. Prefer UE5 built-ins over custom systems.
2. Keep training and runtime separate.
3. No TensorRT dependency.
4. No custom Python pipeline for UE5 asset authoring.
5. Use TDD for deterministic logic.
6. Exploratory spikes must become deterministic tests before the work is complete.
7. Do not leave permanent fail-by-design or skip-by-design tests in the main suite.
8. Treat Manny/Quinn as the default runtime skeleton unless explicitly changed.
9. Keep commits small and atomic.
10. Build with `.\\scripts\\build.ps1`. This script should also be used to run any test passing -Test "Test Name" as a parameter right after the build.
11. If smoke tests were run, read logs with `python .\\scripts\\read_logs.py`.
12. DO NOT ATTEMPT MORE THAN 3 TESTS. If you reach 3 tests without success stop. It's very likely we need a change in focus/direction.

<!-- mcp-graph:start -->
# AGENTS.md — NewEngine-AgentB

### Codex-Specific Rules

- Root project instructions live in `AGENTS.md`; repo-scoped skills live in `.agents/skills/<skill>/SKILL.md`.
- Invoke skills explicitly with `$graph-implement`, `$graph-security`, or another installed skill name when a task needs that workflow.
- In Codex Plan Mode, use mcp-graph and skills for discovery and planning only. Do not edit files until the user asks for implementation outside Plan Mode.
- During implementation, use `apply_patch` for manual edits and preserve unrelated user changes in the worktree.
- Respect sandbox and approval prompts. If a required command fails because of sandbox/network restrictions, rerun it with an approval request.
- Do not spawn subagents unless the user explicitly asks for delegation or parallel agent work.

## mcp-graph — NewEngine-AgentB

Este projeto usa **mcp-graph** para gestão de execução via grafo persistente (SQLite).
Dados armazenados em `workflow-graph/graph.db` (local, gitignored).

### ⚠️ Regra de Execução OBRIGATÓRIA

**O mcp-graph é a fonte de verdade ABSOLUTA. Nenhuma implementação acontece fora do grafo.**

1. **Node deve existir** — antes de escrever QUALQUER código, o node correspondente DEVE existir no grafo
2. **Fluxo obrigatório** — `start_task → [implementar com TDD] → finish_task` (pipeline v8.0) ou `next → context(compact) → context(rag) → [TDD] → analyze(implement_done) → update_status` (granular) — SEM EXCEÇÕES
3. **Epic = estrutura primeiro** — criar Epic + tasks filhas + edges ANTES de implementar
4. **Status tracking** — `update_status → in_progress` ANTES de codar, `→ done` APÓS completar
5. **Validação** — usar `validate` (action: `ac`) após cada task para checar critérios de aceitação
6. **Zero trabalho não-rastreado** — se não tem node no grafo, CRIAR PRIMEIRO

> **Sem node no grafo = sem código escrito.**

### Fluxo de trabalho OBRIGATÓRIO

**Pipeline v8.0 (recomendado — 2 calls):**
```
start_task → [implementar com TDD] → finish_task
```

**Granular (6 calls — disponível para controle fino):**
```
next → context(compact) → context(rag) → [implementar com TDD] → analyze(implement_done) → update_status
```

### Lifecycle (9 fases)

1. **ANALYZE** — Criar PRD, definir requisitos (`import_prd`, `add_node`)
2. **DESIGN** — Arquitetura, decisões técnicas (`add_node`, `edge`, `analyze`)
3. **PLAN** — Sprint planning, decomposição (`plan_sprint`, `analyze`, `sync_stack_docs`)
4. **IMPLEMENT** — TDD Red→Green→Refactor (`next`, `context`, `update_status`, `analyze` — modes: implement_done, tdd_check, progress)
5. **VALIDATE** — Testes E2E, critérios de aceitação (`validate`, `metrics`)
6. **REVIEW** — Code review, blast radius (`export`, `metrics`)
7. **HANDOFF** — PR, documentação, entrega (`export`, `snapshot`)
8. **DEPLOY** — CI pipeline, release, post-release validation (`export`, `snapshot`, `analyze`)
9. **LISTENING** — Feedback, novo ciclo (`add_node`, `import_prd`)

### Phase Gates (Transições entre Fases)

Antes de mudar de fase, rodar o analyze mode correspondente:

| De → Para | Gate (analyze mode) | Pré-requisitos |
|-----------|---------------------|----------------|
| ANALYZE → DESIGN | — | ≥1 epic/requirement no grafo |
| DESIGN → PLAN | `design_ready` | ADRs, interfaces, coupling + harness ≥ 55 |
| PLAN → IMPLEMENT | — | `sync_stack_docs` + `plan_sprint` executados |
| IMPLEMENT → VALIDATE | `validate_ready` | ≥50% tasks done com AC testável |
| VALIDATE → REVIEW | `done_integrity` + `status_flow` | Todos checks passam |
| REVIEW → HANDOFF | `review_ready` | Export + blast radius ok |
| HANDOFF → DEPLOY | `handoff_ready` + `doc_completeness` | Snapshot + memories salvos |
| DEPLOY → LISTENING | `deploy_ready` + `release_check` | Release validado + harness ≥ 70 |

### Definition of Done (8 Checks)

Rodar `analyze(mode: "implement_done", nodeId)` antes de `update_status(done)`:

| # | Check | Severidade | O que verifica |
|---|-------|------------|----------------|
| 1 | `has_acceptance_criteria` | **required** | Task ou parent tem AC |
| 2 | `ac_quality_pass` | **required** | Score AC ≥ 60 (INVEST) |
| 3 | `no_unresolved_blockers` | **required** | Nenhum `depends_on` para node não-done |
| 4 | `status_flow_valid` | **required** | Passou por `in_progress` antes de `done` |
| 5 | `has_description` | recomendado | Descrição não-vazia |
| 6 | `not_oversized` | recomendado | Sem L/XL sem subtasks |
| 7 | `has_testable_ac` | recomendado | ≥1 AC testável |
| 8 | `has_test_files` | recomendado | testFiles preenchido |

### Definition of Ready (7 Checks — Gate ANALYZE → DESIGN)

Rodar `analyze(mode: "ready")` antes de avançar para DESIGN:

| # | Check | O que verifica |
|---|-------|----------------|
| 1 | `has_requirements` | ≥1 epic ou requirement no grafo |
| 2 | `has_acceptance_criteria` | Tasks ou AC nodes existem |
| 3 | `no_orphans` | Sem requirements ou tasks órfãos |
| 4 | `no_cycles` | Sem ciclos de dependência |
| 5 | `has_constraints` | ≥1 constraint node |
| 6 | `has_risks` | ≥1 risk node |
| 7 | `prd_quality_score` | Score PRD ≥ 60 |

### Princípios de Fluxo (Little's Law + Lean + TOC)

**WIP = 1** — Um agente deve ter no máximo 1 task `in_progress` de cada vez.
Lei de Little: `cycle_time = WIP / throughput`. Reduzir WIP reduz cycle time sem perder throughput.

**Pull, não Push** — Usar `next` para puxar a próxima task (pull system).
Nunca empurrar tasks para `in_progress` sem terminar a anterior.

**Gargalo primeiro (Theory of Constraints)** — Se VALIDATE tem tasks acumuladas,
parar de implementar e validar. Otimizar o gargalo, não produzir mais WIP.

**Eliminar desperdício (Lean/Toyota):**
- Overproduction: não implementar features não planejadas
- Waiting: não deixar tasks blocked sem ação
- Overprocessing: usar `context()` (73% menos tokens) em vez de `export()`
- Defects: TDD Red→Green→Refactor elimina retrabalho

**Métricas de fluxo (usar com `metrics` e `analyze(progress)`):**
- Cycle time = `done_timestamp - in_progress_timestamp` por task
- Lead time = `done_timestamp - created_at` por task
- Throughput = tasks done / dias
- Flow efficiency = tempo ativo / lead time total (target > 40%)

### Princípios XP Anti-Vibe-Coding

- **TDD obrigatório** — Teste antes do código. Sem teste = sem implementação.
- **Anti-one-shot** — Nunca gere sistemas inteiros em um prompt. Decomponha em tasks atômicas.
- **Decomposição atômica** — Cada task deve ser completável em ≤2h.
- **Code detachment** — Se a IA errou, explique o erro via prompt. Nunca edite manualmente.
- **CLAUDE.md como spec evolutiva** — Documente padrões e decisões aqui.

## Execução de Testes — Gates Hierárquicos

Ver `.claude/rules/tests.md` para referência completa e scripts.

| Gate | Comando | Trigger | Target |
|------|---------|---------|--------|
| Task | `npm run test:blast` | `finish_task` (cada task) | <60s |
| Épico | `npm run test:node` | `epicPromotion.readyToPromote: true` | ~3 min |
| PR | `npm test` | Antes de `git push` | varia |

**Blast obrigatório no finish_task. Node obrigatório no epic gate. Full obrigatório pré-PR.**

### Spec-Driven Development (spec-kit)

6 ferramentas adicionais para desenvolvimento guiado por especificações:

| Tool | Ação | Descrição |
|------|------|-----------|
| `constitution` | create, update, list, check | Princípios governantes do projeto — indexados no RAG, validados em quality gates. `check` valida nodes contra princípios |
| `plugin` | install, remove, enable, disable, list, info | Extensões dinâmicas (SQLite). Plugins alteram behavior de tools sem modificar código |
| `preset` | list, apply, show, create | Presets de workflow que alteram gates, WIP limits, e prerequisites |
| `spec` | generate, validate, list_templates | Templates de spec por fase (ANALYZE, DESIGN, PLAN, IMPLEMENT) |
| `spec_sync` | sync, status, history, link | Specs como documentos vivos — versionamento + sync bidirecional. Links: derived_from, implements, validates |
| `agent_format` | generate, list_formats, list_agents | Gera instruções para 6+ AI agents (markdown, TOML, skill.md, JSON) |

#### Presets disponíveis
| Preset | Quando usar | O que muda |
|--------|-------------|------------|
| `default` | Projetos normais | Gates advisory, WIP=1, prerequisites advisory |
| `strict-tdd` | Projetos críticos | Gates strict, TDD obrigatório, prerequisites strict, harness >= 70 |
| `agile-light` | Prototipagem rápida | Gates off, WIP=3, sem prerequisites |
| `enterprise` | Compliance/audit | Gates strict, security_scan obrigatório, doc_completeness required |

**Fluxo recomendado:**
1. `constitution create` — definir princípios do projeto
2. `preset apply` — escolher workflow (strict-tdd, agile-light, enterprise)
3. `spec generate` — gerar spec a partir de template
4. `spec validate` — validar spec contra template
5. `spec_sync link` — conectar spec com nodes do grafo

## Harness Engineering — Agent Readiness Score

### O que é
Métrica composta (0-100) que mede quão preparado o código está para geração/manutenção por agentes AI.
Quanto maior o score, menor o risco de alucinação e retrabalho.

### 8 Dimensões

| Dimensão | Peso | O que mede |
|----------|------|------------|
| Type Coverage | 25% | % arquivos sem `any` |
| Test Coverage | 25% | Módulos com arquivo de teste correspondente |
| Architecture Fitness | 15% | Deps direction, circular deps, barrel integrity |
| Docs Coverage | 10% | CLAUDE.md, README, rules/, docs/ |
| Naming Clarity | 10% | Nomes descritivos (sem data/result/temp/val genéricos) |
| Error Handling | 5% | Typed errors, sem catch vazio, sem console.error |
| Context Density | 5% | JSDoc em exports (contexto para agentes) |
| Provenance Coverage | 5% | Proporção de nodes com receipt de origem (source_file) |

### Grades

| Grade | Score | Significado |
|-------|-------|-------------|
| A | >= 85 | Excelente — baixo risco de alucinação |
| B | >= 70 | Bom — deploy permitido |
| C | >= 55 | Razoável — precisa melhorar |
| D | < 55 | Crítico — alto risco de alucinação |

### Comandos

- `analyze(mode: "harness_scan")` — Scan completo, salva resultado em knowledge store
- `analyze(mode: "harness_trend")` — Evolução do score (últimos 10 snapshots)
- `analyze(mode: "harness_advice")` — Sugestões de melhoria por dimensão < 70
- `analyze(mode: "harness_remediate")` — Deterministic Remediation Engine: file-level violations → actionable fix suggestions sorted by priority. Zero AI, 16 rules, suppression store for false-positives
- `npm run harness:scan` — CLI local (human-readable output)

### Workflow Diário por Fase

| Fase | O que muda com Harness |
|------|------------------------|
| ANALYZE | Rodar harness_scan para baseline inicial |
| DESIGN | Gate: score >= 55 (C) para avançar para PLAN |
| PLAN | Sprint health mostra harness delta; tasks que melhoram dimensões fracas ganham prioridade via harnessBonus |
| IMPLEMENT | start_task mostra harnessWarning se score < 70; finish_task detecta regressão > 5pts e retorna ruleSuggestions |
| VALIDATE | Gate: sem regressão > 10pts |
| REVIEW | Gate: score >= 55 (C) |
| HANDOFF | Gate: score >= 55 (C) recomendado |
| DEPLOY | Gate MAIS RÍGIDO: score >= 70 (B) obrigatório para release |
| LISTENING | Score salvo como baseline pós-deploy para próximo ciclo |

### Security

Security NÃO é dimensão do harness — é quality gate paralelo (`security_scanner`).
Harness mede "agent readiness" (tipos, testes, docs). Security mede "code correctness" (vulnerabilidades, secrets).
Ambos são visíveis no lifecycle block de cada tool response.

### Issue Pattern Tracker (Steering Loop)

finish_task grava padrões recorrentes de falha DoD. Ao atingir 3 ocorrências,
auto-sugere regras em `.claude/rules/`. Padrões rastreados:
- `missing_ac` — Task sem acceptance criteria
- `status_skip` — Pulo de status (ex: backlog → done)
- `orphan_node` — Node sem parent
- `circular_dep` — Dependência circular
- `oversized_task` — Task L/XL sem subtasks
- `missing_description` — Descrição vazia
- `missing_estimate` — Sem xpSize ou estimateMinutes

### Memory ≠ Estado Atual

Memory files são **snapshots point-in-time**, não estado live. Contagens de progresso ("X/Y done", "% complete") ficam stale rapidamente.

**Antes de planejar baseado em memories:**
1. Grep pelo arquivo/função — se existe com implementação real, o memory é stale
2. **Código vence memory** — se memory diz "X não existe" mas código mostra que sim, confiar no código
3. Contagens numéricas > 48h = possivelmente stale — verificar antes de usar

> **Nunca confiar em contagens de progresso de memories. Sempre verificar no código antes de planejar.**

> **Referências detalhadas on-demand:** Use `help` tool para consultar: `tools`, `analyze_modes`, `skills`, `cli`, `knowledge`, `workflow`, `gates`, `dod`, `dor`, `prerequisites`, `workflows`, `flow`, `quality_metrics`, `tdd`, `pipeline`, `antipatterns`, `harness`, `dream`, `siebel`, `davinci`, `translate`, `journey`, `teamtask`, `snapshot`, `graph_health`.

### Codex Skills

Essential mcp-graph workflows are installed as repo-scoped Codex skills in `.agents/skills`.
Use `$graph-implement` for tracked TDD implementation, `$graph-tests` for test strategy,
`$graph-security` for security review, and `$ui-ux-pro-max` for UI/UX work.
<!-- mcp-graph:end -->
