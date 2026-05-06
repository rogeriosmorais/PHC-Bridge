---
name: graph-api-design
description: API governance and design audit using OpenAPI/Swagger spec generation, REST maturity model, contract validation, and breaking change detection
triggers:
  - graph-api-design
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-api-design

API governance and design audit using OpenAPI/Swagger spec generation, REST maturity model, contract validation, and breaking change detection. Ensures consistent naming, validated contracts, backward compatibility, and comprehensive documentation across all API surfaces.

## When to Use

- Before REVIEW when APIs change
- When adding new endpoints or MCP tools
- During DESIGN for API-first development
- Before major releases

## Mandatory Flow

```
endpoint inventory --> naming conventions --> contract validation --> breaking changes --> versioning --> documentation --> report --> write_memory
```

## Workflow

### Step 1: Endpoint Inventory

Catalog all API routes (`src/api/routes/`) and MCP tools (`src/mcp/tools/`). Count endpoints per resource. Verify RESTful naming: pluralized nouns for resources, HTTP verbs for actions. Flag non-RESTful patterns.

- List all Express Router files and extract route definitions (GET, POST, PUT, DELETE, PATCH)
- List all MCP tool registrations via `server.tool()` calls
- Group endpoints by resource (e.g., `/nodes`, `/edges`, `/knowledge`)
- Count total endpoints per router and per HTTP method
- Flag routes using verbs in the URL path (e.g., `/getNodes` instead of `GET /nodes`)

### Step 2: Naming Convention Audit

Check route naming consistency: kebab-case paths, consistent pluralization, no verbs in URLs (use HTTP methods instead). For MCP tools: snake_case names, consistent parameter naming. Compare against existing patterns.

- REST routes: verify kebab-case (`/code-graph`, not `/codeGraph`)
- REST routes: verify pluralized resource nouns (`/nodes`, not `/node`)
- REST routes: verify no action verbs in paths — use HTTP methods instead
- MCP tools: verify snake_case naming (`import_prd`, not `importPrd`)
- MCP tools: verify consistent parameter naming across related tools (e.g., `nodeId` everywhere, not mixed `node_id`/`nodeId`)
- Score: compliant endpoints / total endpoints = naming compliance %

### Step 3: Contract Validation

Verify all endpoints have Zod schema validation on input (`validateBody`/`validateQuery` middleware). Check all MCP tools have `z.string()`/`z.number()` params. Flag endpoints accepting unvalidated input. Verify response shapes are consistent.

- Check each API route for `validateBody()` or `validateQuery()` middleware usage
- Check each MCP tool for Zod schema definitions on all parameters
- Flag any `req.body` or `req.query` access without prior validation middleware
- Flag any MCP tool parameter without a Zod type definition
- Verify response shapes use consistent patterns (e.g., `{ data, meta }` or `{ result }`)
- Score: validated endpoints / total endpoints = validation coverage %

### Step 4: Breaking Change Detection

Compare current API surface with previous version (git diff on route files and MCP tool schemas). Detect: removed endpoints, renamed parameters, changed response shapes, tightened validation. Flag breaking changes that need migration path.

- Run `git diff HEAD~10..HEAD -- src/api/routes/ src/mcp/tools/` to detect recent changes
- Detect removed route handlers or MCP tool registrations
- Detect renamed parameters (old name gone, new name added)
- Detect changed response shapes (fields removed or type-changed)
- Detect tightened validation (previously optional now required)
- Each breaking change must have a documented migration path or deprecation period

### Step 5: Versioning & Deprecation

Check for deprecated endpoints/tools (comments, `@deprecated` markers). Verify deprecated items have migration path documented. Check for version headers or URL versioning patterns.

- Grep for `@deprecated`, `deprecated`, `DEPRECATED` in route and tool files
- Verify each deprecated item has a comment indicating the replacement
- Check if deprecated tools still function (grace period) or are fully removed
- Verify deprecated MCP tools are listed in the deprecation registry (`src/mcp/tools/`)
- Check for API version indicators (URL prefix, header, query param)

### Step 6: Documentation Check

Verify API routes have JSDoc comments. Check MCP tools have description strings in `server.tool()` registration. Flag undocumented public endpoints. Verify parameter descriptions exist.

- Check each route handler file for JSDoc comments on exported functions
- Check each MCP tool for a `description` string in its registration
- Check MCP tool parameters for description strings
- Verify `docs/reference/MCP-TOOLS-REFERENCE.md` is up to date with current tool list
- Verify `docs/reference/REST-API-REFERENCE.md` is up to date with current route list
- Flag any public endpoint without documentation as undocumented

### Step 7: API Report

Generate the full audit report. Score 0-100 per dimension (naming, validation, breaking changes, deprecation, docs). List of endpoints by compliance status. Save via `mcp__mcp-graph__write_memory`.

```
Tool: mcp__mcp-graph__write_memory
Params:
  title: "API Design Audit — <date>"
  content: "<findings summary with scores per dimension, breaking changes, undocumented endpoints>"
  tags: ["api", "audit", "design", "governance"]
```

## Output Format

```
Phase: API DESIGN AUDIT
Total Endpoints: <N> REST + <N> MCP tools
Naming Compliance: <N>%
Validation Coverage: <N>%
Breaking Changes: <N> detected
Undocumented Endpoints: <N>
Deprecated Items: <N> (with migration: <N>, without: <N>)
Overall Grade: <A-F>
Recommendations: <top 3 actions>

Saved to memory: "API Design Audit — <date>"
```

## Anti-Patterns

- Do NOT add endpoints without Zod validation — every input boundary must be validated
- Do NOT rename parameters without deprecation period — consumers depend on the current contract
- Do NOT remove endpoints without migration path — provide alternatives before removal
- Do NOT use verbs in REST URLs — use HTTP methods (GET, POST, PUT, DELETE) instead
- Do NOT skip API documentation — it is the contract between producer and consumer
- Do NOT break backward compatibility without major version bump — semver is mandatory


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.
