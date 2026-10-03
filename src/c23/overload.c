#include "c23/overload.h"

#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Overload (c23/overload.c)
 * ============================================================================
 * The arity half of C's missing overloading, shared by every operation in the
 * properties layer. Where constructor.h names a TYPE's constructors
 * (Thing_0, Thing_1, ...), this names an OPERATION's arities: one public name,
 * one function per argument count, dispatched by the count alone.
 *
 * It is deliberately pure preprocessor — the macros expand at the call site,
 * so there is no function to call, no ABI, no allocation, and no runtime cost.
 * A missing arity (declaring _2 and _3 but not _1) is a compile error at the
 * call site, exactly like a missing Java overload. Nothing here owns memory or
 * touches the tree; it only selects which real function runs.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Overload (c23/overload.c)
 * ============================================================================
 * Arity overloading vocabulary. Header-only in practice — this translation
 * unit exists so the vocabulary is one module a reader can find, not so it can
 * hold behavior.
 *
 * STRUCT FIELDS: none — no owned state.
 *
 * MACROS (exported by c23/overload.h):
 * ----------------------------------------------------------------------------
 *   OVERLOAD_PICK(_0.._8, NAME, ...) : positional arity selector
 *   OVERLOAD_DISPATCH(NAME, ...)     : pick NAME##_<argc>(...) at the call site
 * ============================================================================
 */
