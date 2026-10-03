#include "event/gesture.h"

#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/draft.h"
#include "annotation/incomplete.h"

;;DRAFT
;;INCOMPLETE
;;DEFINITION
/**
 * Gesture events — reserved R4 operation/bridge home, not an implementation.
 *
 * Gesture-recognition policy over borrowed scroll/pinch/pan payloads and owner-driven timing.
 *
 * This draft currently owns no storage or borrowed resources and executes no
 * behavior. Before implementation, specify lifetime/ownership, failure results,
 * constructor arities and applicable property/event contracts. Rendering,
 * platform resources and scheduling stay with their existing R1/R3 owners.
 * Widget behavior belongs in its class; shared operations belong in properties/.
 * No blanket event opt-in, fabricated success result or duplicate driver is
 * introduced by this scaffold. The draft marker stays until real lab evidence
 * supports the implemented scope; appearance approval belongs to the user.
 */

;;OVERVIEW
/**
 * MODULE: Gesture events (event/gesture.c)
 * STATUS: draft scaffold; zero implemented runtime capabilities.
 * STORAGE: none; this is a procedural operation/bridge placeholder.
 * PUBLIC API: none added; no callable functions declared or defined.
 * PRIVATE HELPERS: none defined.
 * IMPLEMENT NEXT: Gesture-recognition policy over borrowed scroll/pinch/pan payloads and owner-driven timing.
 * LAB PROOF NOW: source/header compilation only, never runtime readiness.
 * LAB PROOF LATER: constructors, ownership/teardown, failure/boundary cases,
 * property routing and captured-pixel/input oracles where applicable.
 */
