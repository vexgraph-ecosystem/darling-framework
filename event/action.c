#include "event/action.h"

#include "darling/panel/panel.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "nio/mem.h"
#include "oop/type.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ActionEvent
 * ============================================================================
 * Transient semantic action message for button presses, menu picks, and
 * dialog confirms: a source panel, a numeric action id, and an owned command
 * string (e.g. "menu:file:open"). Not a node and never attachable — central
 * wiring delivers it through the Panel tree, where consume() short-circuits
 * the bubble walk.
 *
 * The event is arena-allocated through Memory_alloc with the
 * TYPE_ACTION_EVENT_SINGLETON type id; the command string is owned by the
 * event and released by ActionEvent_free, which also returns the event to
 * the arena. nanos is a plain settable timestamp (clock wiring is behavior
 * phase).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ActionEvent (plain struct, no Panel base)
 * LEVEL: L2 — Behavior (transient semantic action message)
 * ============================================================================
 * Transient semantic action message for button presses, menu picks, and
 * dialog confirms: source panel, numeric id, and an owned command string.
 * Not a node, never attachable; central wiring delivers it through the
 * Panel tree.
 *
 * STRUCT FIELDS (Mirroring event/action.h):
 * ----------------------------------------------------------------------------
 *   Panel *source;      // Panel originating the action; nullptr = none yet
 *   int32_t actionId;   // Numeric action id
 *   char *command;      // Owned command string (e.g. "menu:file:open"); nullptr = none
 *   bool consumed;      // True = stop the bubble walk
 *   uint64_t nanos;     // Plain settable timestamp (clock wiring is behavior phase)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ActionEvent_0(void)
 *   - ActionEvent_2(source, actionId)
 *
 * Core Functions:
 *   - ActionEvent_consume(ev)
 *   - ActionEvent_free(ev)
 *
 * Setters:
 *   - ActionEvent_setSource(ev, source)
 *   - ActionEvent_setActionId(ev, actionId)
 *   - ActionEvent_setCommand(ev, command)
 *   - ActionEvent_setNanos(ev, nanos)
 *
 * Getters:
 *   - ActionEvent_getSource(ev)
 *   - ActionEvent_getActionId(ev)
 *   - ActionEvent_getCommand(ev)
 *   - ActionEvent_getNanos(ev)
 *   - ActionEvent_isConsumed(ev)
 * ============================================================================
 */

// CONSTRUCTORS
// ============================================================================

ActionEvent *ActionEvent_0(void) {
    ActionEvent *ev = (ActionEvent*) Memory_alloc(TYPE_ACTION_EVENT_SINGLETON, sizeof(ActionEvent));
    if (!ev)
        return nullptr;
    (*ev).source = nullptr;
    (*ev).actionId = 0;
    (*ev).command = nullptr;
    (*ev).consumed = false;
    (*ev).nanos = 0u;
    return ev;
}

ActionEvent *ActionEvent_2(Panel *source, int32_t actionId) {
    ActionEvent *ev = ActionEvent_0();
    if (ev) {
        (*ev).source = source;
        (*ev).actionId = actionId;
    }
    return ev;
}

// CORE FUNCTIONS
// ============================================================================

void ActionEvent_consume(ActionEvent *ev) {
    if (!ev)
        return;
    (*ev).consumed = true;
}

void ActionEvent_free(ActionEvent *ev) {
    if (!ev)
        return;
    if ((*ev).command)
        Memory_free((*ev).command);
    (*ev).command = nullptr;
    Memory_free(ev);
}

// SETTERS
// ============================================================================

void ActionEvent_setSource(ActionEvent *ev, Panel *source) {
    if (!ev)
        return;
    (*ev).source = source;
}

void ActionEvent_setActionId(ActionEvent *ev, int32_t actionId) {
    if (!ev)
        return;
    (*ev).actionId = actionId;
}

void ActionEvent_setCommand(ActionEvent *ev, const char *command) {
    if (!ev)
        return;
    if ((*ev).command)
        Memory_free((*ev).command);
    (*ev).command = nullptr;
    if (command) {
        size_t len = strlen(command) + 1;
        (*ev).command = (char*) Memory_alloc(TYPE_ARRAY, len);
        if ((*ev).command)
            strcpy((*ev).command, command);
    }
}

void ActionEvent_setNanos(ActionEvent *ev, uint64_t nanos) {
    if (!ev)
        return;
    (*ev).nanos = nanos;
}

// GETTERS
// ============================================================================

Panel *ActionEvent_getSource(const ActionEvent *ev) {
    return ev ? (*ev).source : nullptr;
}

int32_t ActionEvent_getActionId(const ActionEvent *ev) {
    return ev ? (*ev).actionId : 0;
}

const char *ActionEvent_getCommand(const ActionEvent *ev) {
    return ev ? (*ev).command : nullptr;
}

uint64_t ActionEvent_getNanos(const ActionEvent *ev) {
    return ev ? (*ev).nanos : 0u;
}

bool ActionEvent_isConsumed(const ActionEvent *ev) {
    return ev ? (*ev).consumed : false;
}
