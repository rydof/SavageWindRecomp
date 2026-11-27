# Design Specification: Savage Wind Recomp — Faithful Enhanced Build

**Date:** 2026-11-26  
**Workspace:** `<repo>`
**Fork (`origin`):** `https://github.com/rydof/SavageWindRecomp`  
**Upstream (`upstream`):** `https://github.com/Revivedx/SavageWindRecomp` (base branch: `upstream/test`)

## 1. Intent & Why
Build a cleaner, more complete local version of **Savage Wind Recomp** (Switch BotW v1.9.0 + both DLCs) that stays **faithful to the original game**: integrating the built-in benchmark (`#1163`), visual/logic/input/crash fixes, and non-gameplay-altering video/UI improvements from open PRs and forks, plus fixing additional open upstream visual/logic/crash bugs directly.

## 2. Goals, Non-Goals & Anti-Goals
- **Goals:**
  1. Clone `rydof/SavageWindRecomp`, add `upstream` (`Revivedx/SavageWindRecomp`), and branch `enhanced-faithful` from `upstream/test`.
  2. **Phase 1 — Curated Open PRs:** Audit and integrate in verified waves:
     - *Visual/logic/input/crash fixes:* `#1141`, `#1156`, `#1157`, `#1151`, `#1164`, `#1142`, `#1160`.
     - *Benchmark & faithful video/UI options:* `#1143`, `#1159`, `#1161`, `#1162`, `#1163`.
     - *Selectively audit & include if sound:* Dathinel's 6-PR Vulkan render-context cleanup stack (`#1140`, `#1145`–`#1149`) and drafts `#559`, `#648`.
  3. **Phase 2 — Fork Harvest:** Audit unmerged bugfix branches on active forks (`Ninjune/SavageWindRecomp`, `Dathinel`, `Fayberr`, `Zarigis`) and cherry-pick verified fixes not already in `upstream/test`.
  4. **Phase 3 — Original Engineering Fixes:** Investigate and fix 2–3 reproducible open upstream visual/logic/crash bugs with TDD.
  5. **Build & Verify:** Full Linux build (`make -j8`), subsystem tests, `i18n_check.py`, and Windows portability audit.
- **Non-Goals:**
  - Online/co-op multiplayer (`#294`) — excluded.
  - New gameplay-altering cheats or balance changes.
  - Non-PC ports (PS5, ARM/Mali).
  - Pushing to `origin` or `upstream` without explicit user consent.
- **Anti-Goals:**
  - Never commit Nintendo files, keys, firmware, extracted assets, `generated/` code, saves, or local paths.
  - Never modify `<switch-files>` or real saves/settings in `~/.local/share/savagewind/`.
  - Never break Windows compatibility with unguarded POSIX-only code in shared runtime paths.
