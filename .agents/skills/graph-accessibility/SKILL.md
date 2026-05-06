---
name: graph-accessibility
description: Accessibility compliance audit using WCAG 2.2 AA standards, ARIA validation, screen reader testing, keyboard navigation, color contrast analysis, and i18n readiness
triggers:
  - graph-accessibility
version: 1.0.0
author: Diego Nogueira
date: 2026-04-04
---

# graph-accessibility

Accessibility compliance audit using WCAG 2.2 AA standards, ARIA validation, screen reader testing, keyboard navigation, color contrast analysis, and i18n readiness. Ensures UI components are usable by all users regardless of ability.

## When to Use

- Before DEPLOY phase for UI features
- When adding dashboard components
- During VALIDATE phase for user-facing changes
- Quarterly accessibility reviews
- When targeting WCAG compliance

## Mandatory Flow

```
WCAG checklist -> ARIA validation -> keyboard navigation -> color contrast -> screen reader test -> i18n readiness -> focus management -> report -> write_memory
```

## Workflow

### Step 1: WCAG 2.2 AA Checklist

Audit against the 4 WCAG principles (POUR):

| Principle | Category | What to Check |
|-----------|----------|---------------|
| **Perceivable** | Text alternatives | All images have `alt` text, icons have `aria-label`, decorative images use `alt=""` |
| **Perceivable** | Captions | Video/audio content has captions or transcripts |
| **Perceivable** | Adaptable | Content structure uses semantic HTML (`h1-h6`, `nav`, `main`, `aside`) |
| **Operable** | Keyboard accessible | All functionality available via keyboard alone |
| **Operable** | Enough time | No auto-advancing content without pause/stop controls |
| **Operable** | No seizure triggers | No flashing content >3 flashes/second |
| **Understandable** | Readable | Language declared (`lang` attribute), clear labels, consistent terminology |
| **Understandable** | Predictable | Navigation consistent across pages, no unexpected context changes |
| **Understandable** | Input assistance | Form errors identified, suggestions provided, labels associated |
| **Robust** | Compatible | Valid HTML, ARIA used correctly, works with assistive tech |

Score each principle: PASS, PARTIAL, FAIL. Record findings for failures.

### Step 2: ARIA Validation

Check all interactive elements have proper ARIA attributes:

| Element | Required ARIA | Common Mistakes |
|---------|--------------|-----------------|
| Buttons | `role="button"` (if not `<button>`) | Clickable `<div>` without role |
| Forms | `aria-label` or `<label>` association | Missing labels on inputs |
| Modals | `role="dialog"`, `aria-modal="true"` | No `aria-labelledby` |
| Tabs | `role="tablist"`, `role="tab"`, `role="tabpanel"` | Missing `aria-selected` |
| Navigation | `role="navigation"` or `<nav>` | No `aria-label` for multiple navs |
| Images | `alt` text or `aria-hidden="true"` for decorative | Empty alt on informative images |

Verify ARIA landmarks: `banner`, `navigation`, `main`, `contentinfo`. Flag:
- Missing labels on form inputs
- Clickable `<div>` elements without `role="button"`
- Images without `alt` text
- `aria-hidden="true"` on interactive elements

### Step 3: Keyboard Navigation

Verify all interactive elements are reachable via Tab key:

| Check | How to Verify | Pass Criteria |
|-------|---------------|---------------|
| Tab order | Press Tab through all interactive elements | Logical, top-to-bottom, left-to-right order |
| Focus indicators | Visual focus ring on focused elements | Visible, high-contrast focus outline (no `outline: none`) |
| Skip link | First Tab on page | "Skip to content" link present and functional |
| Keyboard traps | Tab through all components | Can always Tab out of any component |
| Escape key | Press Escape on modals/dropdowns | Closes overlay and returns focus |
| Arrow keys | Test within tab panels, menus, radio groups | Arrow navigation within compound widgets |

Test with Playwright where possible:
```
mcp__playwright__browser_press_key({ key: "Tab" })
mcp__playwright__browser_snapshot()
```

### Step 4: Color Contrast

Check all text meets WCAG AA contrast ratios:

| Element | Minimum Ratio | How to Check |
|---------|--------------|--------------|
| Normal text (<18pt) | 4.5:1 | Check foreground vs background color |
| Large text (>=18pt or >=14pt bold) | 3:1 | Check foreground vs background color |
| UI components | 3:1 | Borders, icons, focus indicators |
| Non-text contrast | 3:1 | Charts, graphs, interactive elements |

Verify information is not conveyed by color alone:
- Error states use icons + color (not just red text)
- Chart data uses patterns + color (not just different colors)
- Links have underline or other non-color indicator

Check dark mode contrast if applicable. Tools: axe-core, Lighthouse accessibility audit.

### Step 5: Screen Reader Testing

Test critical user flows with screen reader (VoiceOver on Mac, NVDA on Windows):

| Check | What to Verify |
|-------|----------------|
| Page title | Announced correctly on page load |
| Headings | Hierarchy logical (h1 -> h2 -> h3, no skipped levels) |
| Form labels | Read correctly when input is focused |
| Error messages | Announced when validation fails (`aria-live` regions) |
| Dynamic content | Updates announced via `aria-live="polite"` or `aria-live="assertive"` |
| Tables | Headers associated with cells (`<th>` with `scope`) |
| Lists | Announced as lists with item count |

Use Playwright for automated checks where possible:
```
mcp__playwright__browser_snapshot()  // Check accessibility tree
mcp__playwright__browser_evaluate({ expression: "document.title" })
```

### Step 6: i18n Readiness

Check for internationalization readiness:

| Check | What to Verify | Why It Matters |
|-------|----------------|----------------|
| No hardcoded strings | UI text extracted to i18n files or constants | Translation requires extractable strings |
| Text direction | RTL support (`dir="rtl"`) available | Arabic, Hebrew users |
| Date/number format | Locale-aware formatting (`Intl.DateTimeFormat`) | US vs EU date formats |
| No string concatenation | Use template strings or i18n interpolation | Word order varies by language |
| Content expansion | UI handles 30% longer text (German expansion) | Layout breaks with longer translations |
| Pluralization | Plural rules handled (not just "s" suffix) | Languages have complex plural rules |

### Step 7: Focus Management

Verify focus management for dynamic content:

| Scenario | Expected Behavior | How to Test |
|----------|-------------------|-------------|
| Modal opens | Focus moves to modal (first focusable element or title) | Open modal, check `document.activeElement` |
| Modal closes | Focus returns to trigger element | Close modal, check focus position |
| Form error | Focus moves to first error message | Submit invalid form, check focus |
| Route change | Focus moves to new content or page title | Navigate, check focus position |
| Toast/notification | Does NOT steal focus (use `aria-live` instead) | Trigger notification, verify focus unchanged |
| Dropdown open | Focus moves to first option | Open dropdown, check focus |
| Content load | Newly loaded content receives focus appropriately | Load content, verify announcement |

### Step 8: Accessibility Report

Generate comprehensive accessibility report:

```
Tool: mcp__mcp-graph__write_memory
Params:
  title: "Accessibility Audit — <date>"
  content: "<WCAG scores, ARIA compliance, keyboard, contrast, screen reader, i18n, focus management>"
  tags: ["accessibility", "audit", "wcag", "a11y"]
```

## Output Format

```
Phase: ACCESSIBILITY AUDIT
WCAG Principle Scores:
  Perceivable: PASS/PARTIAL/FAIL
  Operable: PASS/PARTIAL/FAIL
  Understandable: PASS/PARTIAL/FAIL
  Robust: PASS/PARTIAL/FAIL
ARIA Compliance: N%
Keyboard Navigation Score: N/10
Color Contrast Compliance: N%
Screen Reader Pass Rate: N%
i18n Readiness: N%
Critical Issues: N
Focus Management Score: N/10
Overall Grade: A/B/C/D/F

Saved to memory: "Accessibility Audit — <date>"
```

## Anti-Patterns

- Do NOT treat accessibility as optional -- it is a legal requirement in many jurisdictions
- Do NOT rely only on automated tools -- manual testing catches 50%+ of issues
- Do NOT use `aria-hidden` on interactive elements
- Do NOT remove focus outlines without providing a visible replacement
- Do NOT use color as the only indicator of state or meaning
- Do NOT skip screen reader testing -- it reveals the real user experience
- Do NOT hardcode strings in UI components -- they break i18n


## Codex Notes

- In Codex Plan Mode, use this skill for planning only and do not mutate files.
- During implementation, follow the project `AGENTS.md` rules and use `apply_patch` for manual edits.
