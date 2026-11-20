/* SPDX-License-Identifier: MIT */
/*
 * The entry point shared by every game shell (cmake/game_main.c.in):
 *
 *   cultures-<id> [--game-dir DIR] [--user-dir DIR] [--exe NAME] [--check] [-- GAME_ARGS...]
 *
 * It finds the installed game, hashes its executable and the DLLs next to it, picks the
 * lifted binaries whose hashes match (a shell links every build lifted for its game), and
 * runs the program with rc_run. Without --game-dir or RC_GAME_DIR, and without the game in
 * the working directory, it looks for orig/<id>/<variant>/ in the working directory, the
 * program's directory and the directories above them (as TropicoRecomp does), so a build runs
 * from any build directory and from a debugger.
 */
#include "rc/game.h"

#include "rc/hle.h"
#include "rc/hle_modules.h"
#include "rc/hostfs.h"
#include "rc/log.h"
#include "rc/platform.h"
#include "rc/process.h"
#include "rc/sha256.h"
#include "rc_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const rc_game_info *g, FILE *f)
{
    fprintf(f,
            "%s - static recompilation of %s\n"
            "\n"
            "usage: cultures-%s [--game-dir DIR] [--user-dir DIR] [--exe NAME] [--check] [-- GAME_ARGS...]\n"
            "\n"
            "  --game-dir DIR   your installed copy (default: $RC_GAME_DIR, else the current\n"
            "                   directory, else the lifted install in orig/%s/ above it or\n"
            "                   above the program)\n"
            "  --user-dir DIR   where the game writes saves and settings (default: the\n"
            "                   platform's per-user data directory)\n"
            "  --exe NAME       run another program of the install (Editor.exe, GameMp.exe)\n"
            "  --check          report which of the executable's imports the HLE layer still\n"
            "                   lacks (works without lifted code)\n"
            "  --version        print the version\n"
            "  --help           show this help\n"
            "\n"
            "RC_TRACE=imports logs every API call.\n",
            g->id, g->title, g->id, g->id);
}

/* The main executable of the install: its file name, NULL if there is none. */
static const char *find_exe(const rc_game_info *game, const char *dir, char *path)
{
    for (const char *const *n = game->exe_names; *n; n++) {
        if (rc_host_resolve(dir, *n, false, path) && rc_host_exists(path, NULL, NULL)) {
            return *n;
        }
    }
    return NULL;
}

/* SHA-256 of `name` in `dir`; false if it is not there. */
static bool hash_file(const char *dir, const char *name, char out[65])
{
    char path[RC_PATH_MAX];
    if (!rc_host_resolve(dir, name, false, path) || !rc_host_exists(path, NULL, NULL)) {
        return false;
    }
    uint32_t size = 0;
    uint8_t *data = rc_host_read(path, &size);
    if (!data) {
        return false;
    }
    rc_sha256_hex(data, size, out);
    free(data);
    return true;
}

static bool same_file_name(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a - 'A' + 'a') : *a;
        char y = *b >= 'A' && *b <= 'Z' ? (char)(*b - 'A' + 'a') : *b;
        if (x != y) {
            return false;
        }
    }
    return *a == *b;
}

/* Whether `generated` holds the executable `name` with SHA-256 `hash`. */
static bool exe_lifted(const rc_generated_set *generated, const char *name, const char *hash)
{
    for (size_t i = 0; generated && i < generated->count; i++) {
        const rc_generated_binary *b = generated->binaries[i];
        if (same_file_name(b->name, name) && strcmp(b->sha256, hash) == 0) {
            return true;
        }
    }
    return false;
}

#define MAX_VARIANTS 32

typedef struct variant_list {
    char names[MAX_VARIANTS][64];
    int count;
} variant_list;

static void add_variant(void *ctx, const char *name, bool is_dir, uint64_t size)
{
    (void)size;
    variant_list *l = ctx;
    if (is_dir && l->count < MAX_VARIANTS && strlen(name) < sizeof l->names[0]) {
        snprintf(l->names[l->count++], sizeof l->names[0], "%s", name);
    }
}

static int cmp_variant(const void *a, const void *b) { return strcmp(a, b); }

/* An install below orig/<id>/ in `start` or a directory above it: the first variant (by name)
 * whose executable was lifted into this program, else the first that has the game at all (so
 * the refusal names its hash). `out` holds RC_PATH_MAX. */
static bool find_up(const rc_game_info *game, const rc_generated_set *generated, const char *start,
                    char *out)
{
    char base[RC_PATH_MAX - 128]; /* room for the suffixes */
    snprintf(base, sizeof base, "%s", start);
    for (int up = 0; up < 8; up++) {
        char orig[RC_PATH_MAX];
        bool is_dir = false;
        snprintf(orig, sizeof orig, "%s%sorig/%s", base, base[strlen(base) - 1] == '/' ? "" : "/",
                 game->id);
        if (rc_host_exists(orig, &is_dir, NULL) && is_dir) {
            static variant_list l;
            l.count = 0;
            rc_host_list(orig, add_variant, &l);
            qsort(l.names, (size_t)l.count, sizeof l.names[0], cmp_variant);
            bool have_any = false;
            for (int i = 0; i < l.count; i++) {
                char dir[RC_PATH_MAX], path[RC_PATH_MAX], hash[65];
                snprintf(dir, sizeof dir, "%.900s/%s", orig, l.names[i]);
                const char *exe = find_exe(game, dir, path);
                if (!exe) {
                    continue;
                }
                if (hash_file(dir, exe, hash) && exe_lifted(generated, exe, hash)) {
                    snprintf(out, RC_PATH_MAX, "%s", dir);
                    return true;
                }
                if (!have_any) {
                    snprintf(out, RC_PATH_MAX, "%s", dir);
                    have_any = true;
                }
            }
            if (have_any) {
                return true;
            }
        }
        size_t len = strlen(base);
        snprintf(base + len, sizeof base - len, "%s..", base[len - 1] == '/' ? "" : "/");
    }
    return false;
}

static uint32_t rd16(const uint8_t *b, uint32_t at) { return (uint32_t)b[at] | ((uint32_t)b[at + 1] << 8); }
static uint32_t rd32(const uint8_t *b, uint32_t at) { return rd16(b, at) | (rd16(b, at + 2) << 16); }

/* File offset of `rva`, or 0. */
static uint32_t rva_offset(const uint8_t *f, uint32_t size, uint32_t rva)
{
    uint32_t pe = rd32(f, 0x3c);
    uint32_t nsec = rd16(f, pe + 6), opt = pe + 24, secs = opt + rd16(f, pe + 20);
    for (uint32_t i = 0; i < nsec && secs + 40 * i + 40 <= size; i++) {
        uint32_t h = secs + 40 * i;
        uint32_t va = rd32(f, h + 12), raw = rd32(f, h + 16), off = rd32(f, h + 20);
        if (rva >= va && rva < va + raw && off + (rva - va) < size) {
            return off + (rva - va);
        }
    }
    return 0;
}

/* --check: every import of the executable, per DLL, implemented or not. */
static int check(const rc_game_info *game, const char *game_dir, const char *exe_name)
{
    char path[RC_PATH_MAX];
    const char *name = exe_name;
    if (name && !(rc_host_resolve(game_dir, name, false, path) && rc_host_exists(path, NULL, NULL))) {
        name = NULL;
    } else if (!name) {
        name = find_exe(game, game_dir, path);
    }
    if (!name) {
        fprintf(stderr, "%s not found in '%s' (use --game-dir)\n", exe_name ? exe_name : game->exe_names[0], game_dir);
        return RC_EXIT_ERROR;
    }
    uint32_t size = 0;
    uint8_t *f = rc_host_read(path, &size);
    if (!f || size < 0x40 || rd32(f, 0x3c) + 0xf8 > size) {
        fprintf(stderr, "%s: not a PE file\n", path);
        free(f);
        return RC_EXIT_ERROR;
    }
    rc_hle_all();
    uint32_t opt = rd32(f, 0x3c) + 24;
    uint32_t dir = rva_offset(f, size, rd32(f, opt + 96 + 8));
    unsigned total = 0, done = 0;
    for (uint32_t d = dir; dir && d + 20 <= size; d += 20) {
        uint32_t oft = rd32(f, d), dname = rd32(f, d + 12), ft = rd32(f, d + 16);
        if (!oft && !dname && !ft) {
            break;
        }
        const char *dll = (const char *)f + rva_offset(f, size, dname);
        uint32_t look = rva_offset(f, size, oft ? oft : ft);
        unsigned n = 0, have = 0;
        for (uint32_t j = look; look && j + 4 <= size; j += 4) {
            uint32_t v = rd32(f, j);
            if (!v) {
                break;
            }
            char ord[16];
            const char *fn = ord;
            if (v & 0x80000000u) {
                snprintf(ord, sizeof ord, "#%u", v & 0xffff);
            } else {
                fn = (const char *)f + rva_offset(f, size, v) + 2;
            }
            bool ok = rc_import_implemented(dll, fn);
            n++;
            have += ok;
            if (!ok && rc_trace("check")) {
                printf("  missing %s!%s\n", dll, fn);
            }
        }
        printf("  %-20s %4u imported, %4u implemented\n", dll, n, have);
        total += n;
        done += have;
    }
    printf("%s: %u imports, %u implemented, %u missing (RC_TRACE=check lists them)\n", name, total,
           done, total - done);
    free(f);
    return RC_EXIT_OK;
}

static const rc_game_info *the_game;

static void setup(void)
{
    if (!rc_plat_init(the_game->title)) {
        rc_fatal("no display");
    }
    rc_hle_all();
}

static void show_fatal(const char *message) { rc_plat_error(the_game->title, message); }

int rc_game_main(const rc_game_info *game, const rc_generated_set *generated, int argc, char **argv)
{
    the_game = game;
    const char *game_dir = getenv("RC_GAME_DIR");
    const char *user_dir = NULL;
    const char *exe_name = NULL;
    bool do_check = false;
    static char args[1024];
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(game, stdout);
            return RC_EXIT_OK;
        } else if (strcmp(a, "--version") == 0) {
            printf("cultures-%s %s (%s)\n", game->id, RC_VERSION, RC_GIT_HASH);
            return RC_EXIT_OK;
        } else if (strcmp(a, "--check") == 0) {
            do_check = true;
        } else if (strcmp(a, "--game-dir") == 0 && i + 1 < argc) {
            game_dir = argv[++i];
        } else if (strcmp(a, "--user-dir") == 0 && i + 1 < argc) {
            user_dir = argv[++i];
        } else if (strcmp(a, "--exe") == 0 && i + 1 < argc) {
            exe_name = argv[++i];
        } else if (strcmp(a, "--") == 0) {
            for (int j = i + 1; j < argc; j++) {
                size_t used = strlen(args);
                snprintf(args + used, sizeof args - used, "%s%s", used ? " " : "", argv[j]);
            }
            break;
        } else {
            fprintf(stderr, "unknown argument '%s'\n\n", a);
            usage(game, stderr);
            return RC_EXIT_USAGE;
        }
    }
    if (!game_dir || !*game_dir) {
        static char found[RC_PATH_MAX];
        char path[RC_PATH_MAX];
        const char *exe_dir = rc_plat_base_dir();
        game_dir = ".";
        if (!find_exe(game, ".", path) &&
            (find_up(game, generated, ".", found) || (exe_dir && find_up(game, generated, exe_dir, found)))) {
            game_dir = found;
            rc_log("game in %s", game_dir);
        }
    }
    if (do_check) {
        return check(game, game_dir, exe_name);
    }
    if (!generated || generated->count == 0) {
        printf("%s: no lifted code in this build.\n"
               "Lift your copy of the game first; see docs/WORKFLOWS.md.\n",
               game->title);
        return RC_EXIT_NOT_LIFTED;
    }

    char path[RC_PATH_MAX];
    const char *exe = exe_name;
    if (exe && !(rc_host_resolve(game_dir, exe, false, path) && rc_host_exists(path, NULL, NULL))) {
        exe = NULL;
    } else if (!exe) {
        exe = find_exe(game, game_dir, path);
    }
    if (!exe) {
        fprintf(stderr, "%s not found in '%s' (use --game-dir)\n", exe_name ? exe_name : game->exe_names[0], game_dir);
        return RC_EXIT_ERROR;
    }
    /* The lifted binaries whose hashes match the files of this install. */
    static rc_lifted_binary lifted[16];
    uint32_t nlifted = 0;
    bool have_exe = false;
    char exe_hash[65] = "";
    hash_file(game_dir, exe, exe_hash);
    for (size_t i = 0; i < generated->count && nlifted < 16; i++) {
        const rc_generated_binary *b = generated->binaries[i];
        /* Of the executables only the one being run (a game may have lifted its
         * multiplayer or editor build too); every lifted DLL whose hash matches. */
        size_t len = strlen(b->name);
        bool is_exe = len > 4 && (strcmp(b->name + len - 4, ".exe") == 0 || strcmp(b->name + len - 4, ".EXE") == 0);
        if (is_exe && !same_file_name(b->name, exe)) {
            continue;
        }
        char h[65];
        if (!hash_file(game_dir, b->name, h) || strcmp(h, b->sha256) != 0) {
            continue;
        }
        lifted[nlifted].file = b->name;
        lifted[nlifted].base = b->image_base;
        lifted[nlifted].table = b->table;
        lifted[nlifted].count = b->count;
        nlifted++;
        have_exe |= strcmp(h, exe_hash) == 0;
        rc_log("lifted %s (%s) at %#010x", b->name, b->variant, b->image_base);
    }
    if (!have_exe) {
        fprintf(stderr,
                "%s in '%s' (SHA-256 %s) is not among the builds lifted into this program.\n"
                "Lift it with `recomp lift` and build again; see docs/WORKFLOWS.md.\n",
                exe, game_dir, exe_hash);
        return RC_EXIT_NOT_LIFTED;
    }

    static char user[RC_PATH_MAX];
    if (!user_dir) {
        const char *u = rc_plat_user_dir("culturesrecomp", game->id);
        snprintf(user, sizeof user, "%s", u ? u : "user");
        user_dir = user;
    }
    if (!rc_host_exists(user_dir, NULL, NULL)) {
        rc_host_mkdir(user_dir);
    }

    rc_set_fatal_hook(show_fatal);
    static rc_config cfg;
    cfg.id = game->id;
    cfg.game_dir = game_dir;
    cfg.user_dir = user_dir;
    cfg.exe = exe;
    cfg.guest_dir = game->guest_dir;
    cfg.args = args;
    cfg.setup = setup;
    cfg.lifted = lifted;
    cfg.nlifted = nlifted;
    return rc_run(&cfg);
}
