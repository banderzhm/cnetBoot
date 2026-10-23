# Security Policy

## Reporting a vulnerability

Please do not disclose security vulnerabilities in a public issue. Use GitHub's private
security-advisory reporting flow for this repository and include:

- the affected commit or release;
- a minimal reproduction;
- the expected and observed security boundary;
- any known mitigation.

Reports involving authentication, authorization, tenant isolation, secret handling, protocol
parsing, or memory safety are treated as security issues. Maintainers will acknowledge a complete
report, assess its severity, and coordinate disclosure after a fix is available.

## Supported versions

Until the first stable release, only the latest commit on `main` receives security fixes.
