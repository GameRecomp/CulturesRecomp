/* SPDX-License-Identifier: MIT */
#include "rc/game.h"
#include "rc/log.h"
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
            "  --version        print the version\n"
            "  --help           show this help\n",
            g->id, g->title, g->id);
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
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(game, stdout);
            return RC_EXIT_OK;
        } else if (strcmp(a, "--version") == 0) {
            printf("cultures-%s %s (%s)\n", game->id, RC_VERSION, RC_GIT_HASH);
            return RC_EXIT_OK;
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

    if (!generated || generated->count == 0) {
        printf("%s: no lifted code in this build.\n"
               "Lift your copy of the game first; see docs/WORKFLOWS.md.\n",
               game->title);
        return RC_EXIT_NOT_LIFTED;
    }

    char exe_path[1024];
    const char *exe_name = NULL;
    for (const char *const *n = game->exe_names; *n; n++) {
        if (rc_vfs_resolve(game_dir, *n, exe_path, sizeof exe_path) == 0) {
            exe_name = *n;
            break;
        }
    }
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
