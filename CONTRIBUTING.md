# Contributing to darling-editor

All code in `darling-framework` must comply with the
[workspace constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a),
one real, Git-ignored workspace-root `../../../preferences.md`. There is no local
Darling lawbook in this checkout; that is an explicit reading gap, not an exemption.

R2 comprises Vexspoke CPU computation/behavior and Relational Engine
memory/storage/native C search. Migration is staged; existing Vexspoke
memory/container ABI and default allocator remain. Darling's include allowlist
stays Vexspoke + Graphvex + Hotcwap. R1 owns lifetimes/residency; Graphvex R3 owns
GPU shaders, dispatch and graphics composition. No direct engine dependency,
C/Rust atomic-layout compatibility or automatic schema migration is implied.

## Non-Negotiable Invariants
1. **Rule 1 (No Arrow Sugar)**: Always write `(*ptr).field`. Never use `->`.
2. **Rule 3 (One Class Per File)**: Each `.h`/`.c` pair contains exactly one class struct.
3. **Rule 10 (Two-Layer Access Cap)**: Maximum 2 member hops per expression (`(*a).b`).
4. **Rule 23 (Living Overview Blueprint)**: Implementation files begin with `;;OVERVIEW` detailing struct fields and function registry.
5. **Rule 24 (Symmetric Getters/Setters)**: Complete mutator and accessor pairs for every stored struct field.
