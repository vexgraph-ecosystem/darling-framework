# Contributions & Engineering Manifesto (darling)

This project is a strictly solo development process conducted in tight pair-programming partnership with an AI coding assistant.

It serves as an architectural manifesto for a **retained-mode UI and 2D/3D presentation toolkit** built entirely in pure C23, featuring zero steady-state heap allocations, native macOS `CALayer`/`IOSurface` compositing, and Vulkan scene rendering.

---

## 1. The AI-First Architecture Manifesto & Boilerplate Defense

This codebase strictly enforces the verbose, explicit boilerplate required across the `vexgraph` ecosystem:
- Strict prohibition of arrow syntax (`p->field` is banned; only explicit `(*p).field` is permitted).
- Single Class Per File (the Java Law: one public `typedef struct` per `.h`/`.c` pair).
- Arity-overloaded explicit constructor dispatch macros (`Class_0()`, `Class_1()`).
- Complete, symmetric getters and setters for all struct fields (Java-library standard).
- Strict dest-last parameter ordering `(a, b, dest)`.
- Two-layer member access cap (`(*layer1).layer2` maximum).
- Exhaustive `;;OVERVIEW` blueprints mirrored at the top of every implementation file.

### Why the Boilerplate Exists
This boilerplate is **not** an accident, nor is it a misunderstanding of idiomatic C. It is an intentional, machine-verifiable scaffold built specifically for **AI-Human Pair Systems Programming**:
1. **Machine Comprehension**: Eliminating `->` and restricting each file to a single class struct allows an AI coding agent to inspect, refactor, and reason over UI components with zero ambiguity and bounded context overhead.
2. **Predictable Object Model**: Symmetric getters and setters guarantee that neither human nor AI ever pierces struct internals across module seams.
3. **AI-Maintained Rigor**: Writing dozens of boilerplate mutators and accessors is effortless for an AI agent. The human designer directs the visual aesthetics, layout trees, and event choreography, while the AI guarantees structural and memory completeness.

---

## 2. Sanity Warning for External Contributors

> [!WARNING]
> **SANITY NOTICE FOR EXTERNAL CONTRIBUTORS**
> This repository is not designed for traditional C conveniences, casual hacking, or stylistic shortcuts. It is an unapologetic, machine-verifiable manifesto of AI-augmented systems architecture.
>
> **If you do not approve of this architecture or cannot find peace with this philosophy, consider leaving this repository for your own sanity.**
>
> We do not accept Pull Requests, issues, or unsolicited stylistic refactors attempting to re-introduce `->`, combine multiple widgets into one file, or bypass explicit getters/setters. Upstream is maintained exclusively by the author and the AI agent.

---

## 3. Supreme Living Document: `preferences.md` & Repo-Local Preferences

All architectural rules and style invariants are governed by the central constitution:

- **[preferences.md](https://github.com/vexgraph-dev/vexspoke/blob/main/preferences.md)** (tracked in `vexspoke`, accessible locally at `../../preferences.md`)
- **[darling-framework-preferences.md](darling-framework-preferences.md)** (repo-local mirror binding darling-framework)

Whenever preferences or conventions evolve, `preferences.md` and `darling-framework-preferences.md` are updated and committed locally in the same cycle (the Living Preferences Law / Zero Drift).

---

## 4. `darling` Architectural Invariants

| Invariant | Specification |
| :--- | :--- |
| **Living Darling Docs** | [`_docs/darling.md`](_docs/darling.md) is a load-bearing blueprint (1699+ lines). Any addition, removal, or change to a widget's fields, constructors, or methods must update `_docs/darling.md` in the exact same commit (per the Living Darling Docs Law). |
| **Symmetric Getter/Setter Completeness** | Every state-bearing struct field provides explicit `Class_get*` and `Class_set*` functions. No manual member piercing (per the Symmetric Getter/Setter Completeness Law). |
| **Sub-Part Field Segregation** | Widgets owning sub-objects (carets, scrollbars) expose them only via `Class_part_*` verbs. Struct fields are segregated under explicit part banners (per the Sub-Part Field Segregation Law). |
| **Two-Layer Compositing Split** | Scene panels render to the Vulkan swapchain background; content panels composite individual `IOSurface`-backed `CALayer`s via the WindowServer bridge. |
| **Zero Steady-State Allocation** | One arena carved from the OS. Zero `malloc`/`calloc` calls during layout passes, paint runs, or cursor hit-tests. |
