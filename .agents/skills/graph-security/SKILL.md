---
name: graph-security
description: Cross-cutting security audit using OWASP Top 10, STRIDE threat modeling, dependency audit, and secrets scanning
triggers:
  - graph-security
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-security

Cross-cutting security audit using OWASP Top 10, STRIDE threat modeling, dependency audit, and secrets scanning. Identifies vulnerabilities across dependencies, source code, input validation, and file operations.

## When to Use

- After IMPLEMENT phase or before REVIEW phase
- When security review is needed for sensitive features
- Before deploying features that handle user input, authentication, or file I/O
- The user says "security audit", "security review", or "check vulnerabilities"

## Mandatory Flow

```
npm audit → OWASP Top 10 checklist → secrets scan → STRIDE model → input validation → path traversal → report → write_memory
```

## Workflow

### Step 1: Dependency Audit

Run dependency vulnerability scan:
```bash
npm audit --audit-level=high
```

Analyze output:
- Count CVEs by severity (critical, high, moderate, low)
- Check for known vulnerabilities in direct and transitive dependencies
- Flag packages with no maintained fix available
- Recommend `npm audit fix` or manual resolution for each finding

### Step 2: OWASP Top 10 Scan

Interactive checklist covering all OWASP Top 10 (2021) categories. For each, review the codebase:

| # | Category | What to Check |
|---|----------|---------------|
| A01 | Broken Access Control | Route guards, authorization checks, RBAC enforcement |
| A02 | Cryptographic Failures | Hardcoded secrets, weak hashing, plaintext storage |
| A03 | Injection | SQL injection (parameterized queries?), command injection, template injection |
| A04 | Insecure Design | Threat modeling gaps, missing rate limiting, no abuse cases |
| A05 | Security Misconfiguration | Default credentials, verbose errors in production, open CORS |
| A06 | Vulnerable Components | Outdated dependencies, known CVEs (from Step 1) |
| A07 | Auth Failures | Weak passwords, missing MFA, session fixation, token expiry |
| A08 | Software Integrity | Unsigned dependencies, missing SRI, no integrity checks on imports |
| A09 | Logging Failures | Missing security event logs, PII in logs, no audit trail |
| A10 | SSRF | Unvalidated URLs, internal network access from user input |

Mark each as: PASS, FAIL, N/A. Record findings for failures.

### Step 3: Secrets Scan

Grep for API keys, tokens, passwords, and private keys in source code using these patterns:

- `/[A-Z0-9]{20,}/` — potential API keys or tokens
- `/password\s*=/` — hardcoded passwords
- `/secret\s*[:=]/` — hardcoded secrets
- `/-----BEGIN.*KEY-----/` — private keys
- `/Bearer\s+[A-Za-z0-9\-._~+\/]+=*/` — bearer tokens
- `/AKIA[0-9A-Z]{16}/` — AWS access keys

Exclude: `node_modules/`, `dist/`, `*.lock`, `*.test.ts` (test fixtures only if clearly mock data).

Flag any finding in committed source code as Critical or High.

### Step 4: STRIDE Threat Model

For each major component (API routes, MCP tools, CLI commands, store layer, file operations), evaluate:

| Threat | Question | Mitigation Check |
|--------|----------|-----------------|
| **S**poofing | Can an attacker impersonate a user or component? | Auth tokens, session validation |
| **T**ampering | Can data be modified in transit or at rest? | Input validation, checksums, parameterized queries |
| **R**epudiation | Can actions be denied without audit trail? | Logging, audit events |
| **I**nformation Disclosure | Can sensitive data leak? | Error messages, log sanitization, file permissions |
| **D**enial of Service | Can the system be overwhelmed? | Rate limiting, resource bounds, timeout handling |
| **E**levation of Privilege | Can a user gain unauthorized access? | Least privilege, role checks, input sanitization |

### Step 5: Input Validation Gate

Verify all external input boundaries use proper validation:

- All MCP tool params validated via Zod schemas (`src/mcp/tools/`)
- All API routes use `validateBody`/`validateQuery` middleware (`src/api/`)
- CLI args validated via Commander.js + Zod (`src/cli/`)
- No raw `JSON.parse()` without schema validation on untrusted input
- No `eval()`, `Function()` constructor, or `vm.runInNewContext()` on user input

### Step 6: Path Traversal Gate

Verify all file operations are protected against path traversal:

- `safeReadFileSync` used for all user-controlled file reads
- `assertPathInsideProject` used to validate file paths stay within project boundaries
- No raw `fs.readFile`, `fs.readFileSync`, `fs.writeFile` on user-provided paths
- No `..` traversal possible in file path parameters
- Symlink resolution checked before file access

### Step 7: Security Report

Generate report with findings organized by severity:

```
Tool: mcp__mcp-graph__write_memory
Params:
  title: "Security Audit — <date>"
  content: "<findings summary with severity counts, OWASP compliance, STRIDE coverage>"
  tags: ["security", "audit", "owasp", "stride"]
```

## Output Format

```
Phase: SECURITY AUDIT
Dependency Audit: N critical, N high, N moderate, N low
OWASP Compliance: X/10 passed (Y%)
Secrets Scan: N findings (N critical, N high)
STRIDE Coverage: X/6 threats modeled per component
Input Validation: N/M boundaries validated
Path Traversal: N/M file operations protected
Total Findings: N critical, N high, N medium, N low, N info
Recommendations: N action items

Saved to memory: "Security Audit — <date>"
```

## Anti-Patterns

- Do NOT skip dependency audit — known CVEs are the lowest-hanging fruit
- Do NOT ignore medium-severity findings — they compound into critical exposure
- Do NOT hardcode credentials even for tests — use environment variables or test fixtures
- Do NOT disable eslint-plugin-security rules — fix the underlying code
- Do NOT skip STRIDE for new features — threat modeling prevents design-level vulnerabilities
- Do NOT use `eval()` or `Function()` constructor — always use safe alternatives
- Do NOT trust user input without Zod validation — validate at every boundary


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.
