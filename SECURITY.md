# Security & Integrity Policy

Porth-IO is designed for mission-critical infrastructure, including **High-Frequency Trading (HFT)** and **National Power Grids**. Security is handled with the same rigor as performance.

---

## 🛡️ Our Security Model

* **Kernel Bypass:** By moving logic to user-space, we reduce the attack surface of the Linux kernel.
* **Memory Safety:** We utilize RAII and modern C++ abstractions to mitigate buffer overflows, but we acknowledge the inherent risks of direct hardware memory mapping.
* **Supply Chain Integrity:** We generate a Software Bill of Materials (SBOM) for every release to ensure all dependencies are auditable.

---

## 🛑 Reporting a Vulnerability

**Please do not open public issues for security vulnerabilities.**

To report a sensitive issue:

1. **Email:** harridavies03@gmail.com
2. **Encryption:** (GPG Key Coming Soon)
3. **Process:** We aim to acknowledge all reports within 48 hours and provide a fix or mitigation within 7 days for critical vulnerabilities.

---

## 📜 Vulnerability Disclosure

We follow a **coordinated disclosure model**. We will work with you to ensure a fix is ready before the vulnerability is made public, protecting the sovereign hardware cluster.

---

**Trust through Transparency. Security through Determinism.**