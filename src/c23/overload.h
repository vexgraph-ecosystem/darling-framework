#ifndef DARLING_C23_OVERLOAD_H
#define DARLING_C23_OVERLOAD_H

// darling R4 — c23/overload.h
//
// Arity overloading for C23 — the OPERATION half of the constructor pattern.
// constructor.h names a TYPE's constructors (Thing_0, Thing_1, ...); this names
// an OPERATION's arities: one public name, one function per argument count,
// dispatched by the count alone.
//
//   // ops/add.h
//   Panel *Panel_add_2(Panel *parent, Panel *child);
//   Panel *Panel_add_3(Panel *parent, Panel *child, int index);
//   #define Panel_add(...) OVERLOAD_DISPATCH(Panel_add, __VA_ARGS__)
//
//   Panel_add(root, child);        // -> Panel_add_2(root, child)
//   Panel_add(root, child, 2);     // -> Panel_add_3(root, child, 2)
//
// The generated `_k` names are implementation; call sites spell the bare op.
// Arity GAPS are legal: declaring _2 and _3 but not _1 makes a 1-arg call fail
// to compile, exactly like a missing Java overload.
//
// Pure preprocessor: no code, no allocation, no runtime cost. Extend past 8
// args by growing OVERLOAD_PICK and the NAME##_k ladder in step.

#define OVERLOAD_PICK(_0, _1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

#define OVERLOAD_DISPATCH(NAME, ...)                                        \
    OVERLOAD_PICK(dummy __VA_OPT__(,) __VA_ARGS__,                          \
        NAME##_8, NAME##_7, NAME##_6, NAME##_5,                             \
        NAME##_4, NAME##_3, NAME##_2, NAME##_1, NAME##_0)(__VA_ARGS__)

#endif // DARLING_C23_OVERLOAD_H
