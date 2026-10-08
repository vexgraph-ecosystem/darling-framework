# Contributing to darling-editor

All code in `darling-framework` must comply with the
[workspace constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a),
one real, Git-ignored workspace-root `../../../preferences.md`. There is no local
Darling lawbook in this checkout; that is an explicit reading gap, not an exemption.

R2 comprises Vexspoke CPU computation/behavior and Relational Engine
memory/storage/native C search. Production native IO/NIO now comes from RE by
default, preserving C allocation semantics, not rewritten into Rust. Broader
collection migration remains staged. Darling may borrow RE + Vexspoke + Graphvex
and Hotcwap. R1 owns lifetimes/residency; Graphvex R3 owns GPU shaders, dispatch and
composition. No C/Rust atomic-layout compatibility or schema migration is implied.

## Non-Negotiable Invariants
- **Semantic Consistency Law (Reference form)**: Always write `(*ptr).field`, never arrow access.
- **Single Class Per File Law (Java Law)**: Each `.h`/`.c` pair contains exactly one class.
- **Semantic Consistency Law (Access depth)**: Maximum two member hops per expression.
- **Living Documentation Law**: Keep implementation blueprints current.
- **Single Class Per File Law (Java Law)**: Complete symmetric field accessors.
