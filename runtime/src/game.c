/* SPDX-License-Identifier: MIT */
#include "rc/game.h"
#include "rc/hle.h"
#include "rc/log.h"
#include "rc/mem.h"
#include "rc/pe.h"
#include "rc/vfs.h"
#include "rc_version.h"

#include <stdio.h>
#include <string.h>

static void usage(const rc_game_info *g, FILE *f) {
    fprintf(f,
            "%s - static recompilation of %s\n"
            "\n"
            "usage: cultures-%s [--game-dir DIR] [--log LEVEL]\n"
            "\n"
            "  --game-dir DIR   directory of your installed copy (default: current directory)\n"
            "  --log LEVEL      error, warn, info, debug or trace (also $RC_LOG)\n"
            "  --check          load the game's exe, resolve its imports and report what the\n"
            "                   HLE layer still lacks (works without lifted code)\n"
            "  --version        print the version\n"
            "  --help           show this help\n",
            g->id, g->title, g->id);
}

/* Locate the main executable of the install; NULL if none of the names exists. */
static const char *find_exe(const rc_game_info *game, const char *dir, char *path, size_t size) {
    for (const char *const *n = game->exe_names; *n; n++) {
        if (rc_vfs_resolve(dir, *n, path, size) == 0) return *n;
    }
    return NULL;
}

static int check(const rc_game_info *game, const char *game_dir) {
    char path[1024], err[256];
    const char *name = find_exe(game, game_dir, path, sizeof path);
    if (!name) {
        fprintf(stderr, "%s not found in '%s' (use --game-dir)\n", game->exe_names[0], game_dir);
        return RC_EXIT_ERROR;
    }
    if (rc_mem_init() != 0) return RC_EXIT_ERROR;
    rc_pe_image img;
    if (rc_pe_load_file(path, &img, err, sizeof err) != 0) {
        fprintf(stderr, "%s: %s\n", path, err);
        return RC_EXIT_ERROR;
    }
    printf("%s: image 0x%08x+0x%x, entry 0x%08x, %u sections\n", name, img.image_base,
           img.size_of_image, img.entry, img.section_count);
    printf("imports: %u functions from %u DLLs, %u implemented, %u missing\n", img.imports,
           img.import_dlls, img.imports - img.imports_missing, img.imports_missing);

    /* Per-DLL summary over all thunks (one per distinct import). */
    size_t n = rc_hle_count();
    for (size_t i = 0; i < n; i++) {
        uint32_t t = RC_HLE_THUNK_BASE + (uint32_t)i * RC_HLE_THUNK_STRIDE;
        const char *label = rc_hle_thunk_is_imported(t) ? rc_hle_thunk_label(t) : NULL;
        const char *bang = label ? strchr(label, '!') : NULL;
        if (!bang) continue;
        size_t dll_len = (size_t)(bang - label);
        int seen = 0;
        for (size_t k = 0; k < i && !seen; k++) {
            uint32_t t2 = RC_HLE_THUNK_BASE + (uint32_t)k * RC_HLE_THUNK_STRIDE;
            const char *l2 = rc_hle_thunk_label(t2);
            seen = rc_hle_thunk_is_imported(t2) && l2 && strncmp(l2, label, dll_len + 1) == 0;
        }
        if (seen) continue;
        unsigned total = 0, done = 0;
        for (size_t k = i; k < n; k++) {
            uint32_t t2 = RC_HLE_THUNK_BASE + (uint32_t)k * RC_HLE_THUNK_STRIDE;
            const char *l2 = rc_hle_thunk_label(t2);
            if (rc_hle_thunk_is_imported(t2) && l2 && strncmp(l2, label, dll_len + 1) == 0) {
                total++;
                done += rc_hle_thunk_is_implemented(t2) ? 1u : 0u;
            }
        }
        printf("  %-20.*s %4u imported, %4u implemented\n", (int)dll_len, label, total, done);
    }
    if (rc_log_get_level() >= RC_LOG_DEBUG) {
        for (size_t i = 0; i < n; i++) {
            uint32_t t = RC_HLE_THUNK_BASE + (uint32_t)i * RC_HLE_THUNK_STRIDE;
            if (rc_hle_thunk_is_imported(t) && !rc_hle_thunk_is_implemented(t))
                printf("  missing %s\n", rc_hle_thunk_label(t));
        }
    }
    return RC_EXIT_OK;
}

static int parse_level(const char *s, rc_log_level *out) {
    static const char *const names[] = {"error", "warn", "info", "debug", "trace"};
    for (int i = 0; i <= RC_LOG_TRACE; i++) {
        if (strcmp(s, names[i]) == 0) {
            *out = (rc_log_level)i;
            return 0;
        }
    }
    return -1;
}

int rc_game_main(const rc_game_info *game, const rc_generated_set *generated, int argc,
                 char **argv) {
    const char *game_dir = ".";
    int do_check = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(game, stdout);
            return RC_EXIT_OK;
        } else if (strcmp(a, "--version") == 0) {
            printf("cultures-%s %s (%s)\n", game->id, RC_VERSION, RC_GIT_HASH);
            return RC_EXIT_OK;
        } else if (strcmp(a, "--check") == 0) {
            do_check = 1;
        } else if (strcmp(a, "--game-dir") == 0 && i + 1 < argc) {
            game_dir = argv[++i];
        } else if (strcmp(a, "--log") == 0 && i + 1 < argc) {
            rc_log_level level;
            if (parse_level(argv[++i], &level) != 0) {
                fprintf(stderr, "unknown log level '%s'\n", argv[i]);
                return RC_EXIT_USAGE;
            }
            rc_log_set_level(level);
        } else {
            fprintf(stderr, "unknown argument '%s'\n\n", a);
            usage(game, stderr);
            return RC_EXIT_USAGE;
        }
    }

    if (do_check) return check(game, game_dir);

    if (!generated || generated->count == 0) {
        printf("%s: no lifted code in this build.\n"
               "Lift your copy of the game first; see docs/WORKFLOWS.md.\n",
               game->title);
        return RC_EXIT_NOT_LIFTED;
    }

    char exe_path[1024];
    const char *exe_name = find_exe(game, game_dir, exe_path, sizeof exe_path);
    if (!exe_name) {
        RC_ERROR("%s not found in '%s' (use --game-dir)", game->exe_names[0], game_dir);
        return RC_EXIT_ERROR;
    }
    RC_INFO("found %s", exe_path);

    /* M5: verify the hash against generated->binaries, map the PE, resolve imports through
     * rc_hle_resolve(), set up TEB/stack and call the entry point via rc_dispatch(). */
    RC_ERROR("starting lifted code is not implemented yet (milestone M5)");
    return RC_EXIT_ERROR;
}
