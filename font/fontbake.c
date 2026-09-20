// fontbake — headless baked-font installer CLI.
//
// Bakes OS fonts into ~/anti/fonts without a window or GPU: the atlas and
// dictionary are pure CPU data, texture uploads happen later at runtime.
//   fontbake               install missing + stale (normal setup / refresh)
//   fontbake --force        rebake every installed family
//   fontbake --list         print installed families + source paths
//   fontbake --verify Fam   reload a baked entry, probe glyphs (no GPU)
//   fontbake --emoji        cascade check: U+1F33B via Helvetica -> color page
//   fontbake Fam [Fam...]   bake the named families only

#include <stdio.h>
#include <string.h>

#include "font/font.h"
#include "font/font_bake.h"
#include "io/vexhome.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Fontbake
 * ============================================================================
 * Headless baked-font installer CLI (main harness, no struct): bakes OS
 * fonts into the VexHome fonts store without a window or GPU — the atlas and
 * dictionary are pure CPU data and texture uploads happen later at runtime.
 * Commands cover install missing + stale (normal setup/refresh), --force
 * rebake, --list, --verify Fam (reload a baked entry and probe glyphs with
 * no GPU), --emoji cascade check, and named-family bakes.
 *
 * The harness is procedural: it drives the FontBake_* install API with a
 * progress callback, verifies glyph presence/absence through Font_getGlyph,
 * and ensures the home directory exists via VexHome_ensure before any store
 * access.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Fontbake (main/fontbake.c)
 * LEVEL: L3 — Module Code (headless bake CLI main harness)
 * ============================================================================
 * headless baked-font installer CLI.
 *
 * STRUCT FIELDS: none — procedural (CLI harness, no struct).
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - onProgress(family, done, total, user)
 *   - printf(%s\n", done, total, family)
 *   - fflush(stdout)
 *   - cmdList(void)
 *   - for(i++)
 *   - cmdVerify(family)
 *   - Font_free(f)
 *   - cmdEmoji(void)
 *   - main(argc, argv)
 *   - VexHome_ensure()
 * ============================================================================
 */


static void onProgress(const char *family, size_t done, size_t total, void *user) {
    (void)user;
    printf("[install %zu/%zu] %s\n", done, total, family);
    fflush(stdout);
}

static int cmdList(void) {    static char families[2048][256];
    static char paths[2048][1024];
    int n = FontBake_listInstalled(families, paths, 2048);
    printf("installed: %d (store: %s)\n", n, FontBake_storeDir());
    for (int i = 0; i < n; i++) {
        bool baked = FontBake_hasBaked(families[i]);
        bool stale = baked && FontBake_isStale(families[i]);
        printf("%3d  [%c] %s\n", i + 1, !baked ? ' ' : (stale ? '*' : 'x'), families[i]);
    }
    printf("[x] baked  [*] stale  [ ] missing\n");
    return 0;
}

static int cmdVerify(const char *family) {    Font *f = Font_openBaked(family);
    if (!f) {
        printf("verify failed: cannot open baked '%s'\n", family);
        return 1;
    }
    GlyphMetrics gm;
    bool a = Font_getGlyph(f, (uint32_t)'A', 64.0f, &gm);
    // U+0378 is unassigned: must stay absent (and must return fast, not spin).
    bool missing = !Font_getGlyph(f, 0x0378u, 64.0f, NULL);
    printf("verify '%s': glyphs=%zu pages=%zu baked=%d 'A'[%s %.1fx%.1f pg%d] U+0378-absent[%s]\n",
           family, Font_glyphCount(f), Font_pageCount(f), Font_isBaked(f),
           a ? "ok" : "MISS", a ? gm.width : 0.0, a ? gm.height : 0.0,
           a ? gm.page : -1, missing ? "ok" : "FAIL");
    Font_free(f);
    return (a && missing) ? 0 : 1;
}

// Cascade check: sunflower is not in Helvetica, so it must resolve through
// the platform color path (Apple Color Emoji) with color=1.
static int cmdEmoji(void) {
    Font *f = Font_loadSystem("Helvetica");
    if (!f) {
        printf("emoji test: no Helvetica\n");
        return 1;
    }
    GlyphMetrics gm;
    bool ok = Font_getGlyph(f, 0x1F33Bu, 64.0f, &gm);
    printf("emoji test: ok=%d color=%d page=%d %.1fx%.1f adv=%.1f pages=%zu\n",
           ok, ok ? gm.color : -1, ok ? gm.page : -1,
           ok ? gm.width : 0.0, ok ? gm.height : 0.0,
           ok ? gm.advance : 0.0, Font_pageCount(f));
    bool pass = ok && gm.color == 1 && gm.width > 0.0f && gm.height > 0.0f;
    Font_free(f);
    return pass ? 0 : 1;
}

int main(int argc, char **argv) {
    VexHome_ensure();
    if (argc >= 2 && strcmp(argv[1], "--list") == 0)
        return cmdList();
    if (argc >= 3 && strcmp(argv[1], "--verify") == 0)
        return cmdVerify(argv[2]);
    if (argc >= 2 && strcmp(argv[1], "--emoji") == 0)
        return cmdEmoji();
    if (argc >= 2 && strcmp(argv[1], "--force") == 0) {
        FontBakeInstallReport r = FontBake_installAll(true, onProgress, NULL);
        printf("done: %zu new, %zu rebaked, %zu fresh, %zu failed / %zu\n",
               r.bakedNew, r.rebaked, r.alreadyFresh, r.failed, r.total);
        return r.failed > 0 ? 1 : 0;
    }
    if (argc >= 2 && argv[1][0] != '-') {
        int rc = 0;
        for (int i = 1; i < argc; i++) {
            if (!FontBake_bakeOne(argv[i]) || !FontBake_hasBaked(argv[i])) {
                printf("bake failed: %s\n", argv[i]);
                rc = 1;
            }
        }
        return rc;
    }
    FontBakeInstallReport r = FontBake_installAll(false, onProgress, NULL);
    printf("done: %zu new, %zu rebaked, %zu fresh, %zu failed / %zu\n",
           r.bakedNew, r.rebaked, r.alreadyFresh, r.failed, r.total);
    return r.failed > 0 ? 1 : 0;
}
