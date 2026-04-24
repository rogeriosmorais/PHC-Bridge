# Reviews

Reviews are optional unless explicitly requested by the user.

This folder may store review notes, but review files are not mandatory workflow gates.

A task may proceed based on mechanical proof:
- required build passed
- required tests passed
- scope check passed
- forbidden files untouched
- task commit created

Use reviews only when they add signal:
- suspected scope violation
- suspected fake/stub implementation
- unclear task packet
- repeated failure
- architecture risk

Do not block implementation because a durable review report is missing.
