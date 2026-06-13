# Unified Logging Wrapper PRD

## 1. Vision and Overview
The project currently suffers from excessive log spam, with unformatted, noisy logs making it difficult to debug or monitor. The vision is to create a unified logging wrapper that guarantees clear, impactful logs across all components (C++, Python, etc.) while strictly preventing spam through rate limiting and deduplication.

## 2. Problem Definition
- Agents, tests, builds, and feature designs emit logs directly via `print()`, `UE_LOG`, and `logging.xxx`.
- Repeated function calls flood the logs with garbage/spam, obscuring valuable information.
- Lack of standardized log formatting makes logs hard to read and parse.

## 3. Product Objectives
- **Centralized Wrapper:** Provide a single logging interface (adapted for C++ and Python) that all repository code MUST use.
- **Spam Prevention:** Implement deduplication or rate-limiting so that identical or high-frequency logs are suppressed or summarized.
- **Clarity & Impact:** Ensure all logs are formatted clearly and impactfully.
- **Complete Migration:** Replace 100% of existing direct log emissions across the repository with the new wrapper.

## 4. Architecture Overview
- **Python Wrapper:** A custom logger module (e.g., `logger_wrapper.py`) wrapping Python's `logging` module with a deduplication/rate-limiting filter.
- **C++ Wrapper:** A custom UE5 macro/class (e.g., `FPhysAnimLogger`) wrapping `UE_LOG` with a static map to track log frequencies and suppress spam.
- **Integration:** All scripts, tests, and UE5 source files will include/import these wrappers instead of using standard logging.

## 5. Functional Requirements
- **FR1:** The wrapper must support standard log levels (Info, Warning, Error).
- **FR2:** The wrapper must track the frequency of identical log messages or log locations.
- **FR3:** The wrapper must suppress logs that exceed a defined threshold (e.g., more than X times per second) and optionally emit a "suppressed N times" summary.
- **FR4:** The wrapper must provide a clean, standardized output format.

## 6. Non-Functional Requirements
- **Performance:** The deduplication logic must have minimal overhead, particularly in the UE5 tick loop.
- **Ubiquity:** The wrapper must be used by every agent, test, build, and feature design in the repository.

## 7. Risk Analysis
- **Performance Hit:** Tracking log frequencies in hot paths (like UE5 tick) could introduce latency. *Mitigation: Use efficient hashing (e.g., file+line number) and simple time-based thresholds.*
- **Missing Logs:** Aggressive spam prevention might hide critical fast-occurring errors. *Mitigation: Never suppress Error level logs, or ensure summaries are always printed.*
- **Migration Effort:** Migrating hundreds of `UE_LOG` and `print` calls could cause regressions or merge conflicts. *Mitigation: Automate migration where possible and rely on TDD/tests to ensure correctness.*
