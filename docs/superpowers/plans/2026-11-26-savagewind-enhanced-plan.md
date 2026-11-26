# Execution Plan: Savage Wind Recomp — Faithful Enhanced Build

Execute inline using `superpowers:executing-plans` on branch `enhanced-faithful` (from `upstream/test`).

- **Task 1:** Clone fork, configure `upstream`, save spec/plan, verify baseline `make -j8`, `i18n_check.py`, `test_game_sites.py`, `test_layout.py`.
- **Task 2:** Wave 1 — Integrate direct fix PRs (`#1141`, `#1156`, `#1157`, `#1151`, `#1164`, `#1142`, `#1160`) and verify.
- **Task 3:** Wave 2 — Integrate benchmark & faithful video/UI PRs (`#1143`, `#1159`, `#1161`, `#1162`, `#1163`) and verify.
- **Task 4:** Wave 3 — Audit Vulkan refactor stack (`#1140`, `#1145`–`#1149`), drafts (`#559`, `#648`), and active forks (`Ninjune`, `Dathinel`, `Fayberr`, `Zarigis`).
- **Task 5:** Wave 4 — Investigate and fix 2–3 open upstream visual/logic/crash bugs with TDD.
- **Task 6:** Full verification, Windows portability audit, fresh whole-branch code review, and final report.
