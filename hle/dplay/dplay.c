/* SPDX-License-Identifier: MIT */
/*
 * DirectPlay 4 (dplayx, the ANSI interfaces): network games between recompiled builds on a LAN,
 * for Cultures 1's lifted DPlayDLL.dll.
 *
 * One service provider is offered, TCP/IP. Sessions are found by a UDP query (port 47624)
 * broadcast on the LAN and sent to this machine (two copies on one computer find each other)
 * and to $RC_DPLAY_HOST if set; the host answers with its session. Players connect to the host
 * over TCP; the host numbers the players, keeps the list, relays messages (to one player or to
 * all) and turns arrivals and departures into DirectPlay's system messages. Delivery is always
 * guaranteed and in order. Groups are accepted but have no members.
 *
 * COM objects are as in ddraw.c; one object answers for every IDirectPlay version.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "dplay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSESOCK closesocket
#define NONBLOCK(s) do { u_long one_ = 1; ioctlsocket((s), FIONBIO, &one_); } while (0)
#define WOULDBLOCK() (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#define NONBLOCK(s) fcntl((s), F_SETFL, fcntl((s), F_GETFL, 0) | O_NONBLOCK)
#define WOULDBLOCK() (errno == EAGAIN || errno == EWOULDBLOCK)
#endif

#include "kernel32/k32.h"
#include "rc/hle_modules.h"
#include "rc/log.h"
#include "rc/process.h"
#include "rc/vm.h"

#define COM "DPLAY_COM"
#define DP_OK 0u
#define DPERR_BUFFERTOOSMALL 0x8877001eu
#define DPERR_INVALIDPLAYER 0x88770096u
#define DPERR_NOMESSAGES 0x887700beu
#define DPERR_NOSESSIONS 0x887700dcu
#define DPERR_INVALIDPARAMS 0x80070057u
#define DPERR_GENERIC 0x80004005u
#define DPERR_UNSUPPORTED 0x80004001u

#define DISCOVERY_PORT 47624
#define DPSYS_CREATEPLAYERORGROUP 3u
#define DPSYS_DESTROYPLAYERORGROUP 5u
#define DPSYS_SESSIONLOST 0x31u
#define DPSYS_SETPLAYERORGROUPDATA 0x102u

/* Frames on the TCP connection: u32 length (of the type and payload), u8 type. */
enum { F_WELCOME = 1, F_CREATE_REQ, F_PLAYER_ID, F_PLAYER_NEW, F_PLAYER_GONE, F_DATA, F_DESTROY_REQ, F_PLAYER_DATA };

typedef struct player {
    bool used, local;
    uint32_t id, flags;
    char short_name[64], long_name[64];
    int conn; /* host: the connection that owns it (-1: the host itself) */
    uint32_t event; /* local: the event CreatePlayer was given, set when a message arrives */
    uint8_t *data; /* player data (SetPlayerData), shared with every machine */
    uint32_t data_size;
} player;

typedef struct msg {
    uint32_t from, to, size;
    uint8_t *data;
    struct msg *next;
} msg;

typedef struct conn {
    SOCKET s;
    uint8_t *in;
    uint32_t in_len, in_cap;
} conn;

#define PLAYERS 64
#define CONNS 16

static struct {
    bool open, host;
    uint8_t guid_instance[16], guid_app[16];
    uint32_t max_players, flags, user[4];
    char name[64];
    player players[PLAYERS];
    uint32_t next_id;
    msg *queue, *tail;
    SOCKET udp, listener;
    conn conns[CONNS];  /* host: its clients; client: conns[0] is the host */
    int nconns;
    uint32_t pending_id; /* client: a CreatePlayer waiting for its id */
} dp;

/* Sessions found by EnumSessions. */
#define FOUND 16
static struct {
    uint8_t guid_instance[16], guid_app[16];
    uint32_t max_players, cur_players, flags, user[4];
    char name[64];
    struct sockaddr_in addr; /* the host's TCP address */
} found[FOUND];
static int nfound;

static bool net_ready;
static uint32_t n_sent, n_received; /* game messages (RC_TRACE=dplay logs them now and then) */

/* Network waits use real time: the game's clock may be virtual (RC_CLOCK=virtual). */
static uint64_t real_ms(void)
{
#if defined(_WIN32)
    return GetTickCount64();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
#endif
}

static void real_sleep(uint32_t ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec t = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
#endif
}

static void net_init(void)
{
    if (net_ready) {
        return;
    }
#if defined(_WIN32)
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
#endif
    dp.udp = INVALID_SOCKET;
    dp.listener = INVALID_SOCKET;
    net_ready = true;
}

/* ---- the message queue ---- */

static void enqueue(uint32_t from, uint32_t to, const void *data, uint32_t size)
{
    msg *m = calloc(1, sizeof *m);
    if (!m) {
        return;
    }
    m->from = from;
    m->to = to;
    m->size = size;
    m->data = malloc(size ? size : 1);
    if (m->data && size) {
        memcpy(m->data, data, size);
    }
    if (dp.tail) {
        dp.tail->next = m;
    } else {
        dp.queue = m;
    }
    dp.tail = m;
    for (int i = 0; i < PLAYERS; i++) {
        player *p = &dp.players[i];
        if (p->used && p->local && p->event && (to == 0 || to == p->id)) {
            rc_k32_set_event(p->event);
        }
    }
}

/* A system message about a player (addressed to all local players). The names and the data
 * follow the message; Receive points the message's pointers at them.
 *   CREATE (48 bytes): type, player type, id, current players, data*, size, DPNAME at 24, parent, flags
 *   DESTROY (52 bytes): type, player type, id, local data*, size, remote data*, size, DPNAME at 28, ...
 *   SETDATA (20 bytes): type, player type, id, data*, size */
#define SYS_HEAD 64
static void sys_player(uint32_t type, const player *p)
{
    uint32_t size = SYS_HEAD + 128 + p->data_size;
    uint8_t *m = calloc(1, size);
    if (!m) {
        return;
    }
    uint32_t w[5] = {type, 1 /* DPPLAYERTYPE_PLAYER */, p->id, 0, 0};
    memcpy(m, w, sizeof w);
    memcpy(m + SYS_HEAD, p->short_name, 64);
    memcpy(m + SYS_HEAD + 64, p->long_name, 64);
    if (p->data_size) {
        memcpy(m + SYS_HEAD + 128, p->data, p->data_size);
    }
    enqueue(0, 0, m, size);
    free(m);
}

static void set_data(player *p, const uint8_t *data, uint32_t size)
{
    free(p->data);
    p->data = NULL;
    p->data_size = 0;
    if (size && (p->data = malloc(size)) != NULL) {
        memcpy(p->data, data, size);
        p->data_size = size;
    }
}

/* ---- sockets ---- */

static void send_frame(conn *k, uint8_t type, const void *data, uint32_t size)
{
    uint8_t head[5];
    uint32_t len = size + 1;
    memcpy(head, &len, 4);
    head[4] = type;
    send(k->s, (const char *)head, 5, 0);
    if (size) {
        send(k->s, (const char *)data, (int)size, 0);
    }
}

static void broadcast(uint8_t type, const void *data, uint32_t size, int except)
{
    for (int i = 0; i < dp.nconns; i++) {
        if (i != except && dp.conns[i].s != INVALID_SOCKET) {
            send_frame(&dp.conns[i], type, data, size);
        }
    }
}

static int nplayers(void)
{
    int n = 0;
    for (int i = 0; i < PLAYERS; i++) {
        n += dp.players[i].used;
    }
    return n;
}

static player *find_player(uint32_t id)
{
    for (int i = 0; i < PLAYERS; i++) {
        if (dp.players[i].used && dp.players[i].id == id) {
            return &dp.players[i];
        }
    }
    return NULL;
}

static void free_player(player *p)
{
    free(p->data);
    memset(p, 0, sizeof *p);
}

static player *add_player(uint32_t id, uint32_t flags, const char *sn, const char *ln, bool local, int conn_index)
{
    for (int i = 0; i < PLAYERS; i++) {
        if (!dp.players[i].used) {
            player *p = &dp.players[i];
            free_player(p);
            p->used = true;
            p->local = local;
            p->id = id;
            p->flags = flags;
            p->conn = conn_index;
            snprintf(p->short_name, sizeof p->short_name, "%s", sn ? sn : "");
            snprintf(p->long_name, sizeof p->long_name, "%s", ln ? ln : "");
            return p;
        }
    }
    return NULL;
}

/* A player record on the wire: id, flags, short and long name (64 bytes each), data size, data. */
#define REC 140
static uint8_t *pack_player(const player *p, uint32_t *len)
{
    uint8_t *out = malloc(REC + p->data_size);
    if (!out) {
        *len = 0;
        return NULL;
    }
    memcpy(out, &p->id, 4);
    memcpy(out + 4, &p->flags, 4);
    memcpy(out + 8, p->short_name, 64);
    memcpy(out + 72, p->long_name, 64);
    memcpy(out + 136, &p->data_size, 4);
    if (p->data_size) {
        memcpy(out + REC, p->data, p->data_size);
    }
    *len = REC + p->data_size;
    return out;
}

/* Read a record into `p` (a scratch player); returns its length, 0 if `n` is too short. */
static uint32_t unpack_player(const uint8_t *in, uint32_t n, player *p)
{
    uint32_t ds;
    if (n < REC) {
        return 0;
    }
    memcpy(&ds, in + 136, 4);
    if (n - REC < ds) {
        return 0;
    }
    memset(p, 0, sizeof *p);
    memcpy(&p->id, in, 4);
    memcpy(&p->flags, in + 4, 4);
    memcpy(p->short_name, in + 8, 64);
    memcpy(p->long_name, in + 72, 64);
    p->short_name[63] = p->long_name[63] = 0;
    p->data = (uint8_t *)(uintptr_t)(in + REC); /* borrowed */
    p->data_size = ds;
    return REC + ds;
}

/* Send a player's record to every connection but `except` (host) as `type`. */
static void broadcast_player(uint8_t type, const player *p, int except)
{
    uint32_t len;
    uint8_t *rec = pack_player(p, &len);
    if (rec) {
        broadcast(type, rec, len, except);
        free(rec);
    }
}

/* A copy of a scratch record as a player of this machine's list. */
static player *add_from(const player *r, bool local, int conn_index)
{
    player *np = add_player(r->id, r->flags, r->short_name, r->long_name, local, conn_index);
    if (np) {
        set_data(np, r->data, r->data_size);
    }
    return np;
}

/* Route a data message (host): to the host's own players and to the connections that own
 * the target (or to all but the sender). */
static void route(uint32_t from, uint32_t to, const uint8_t *data, uint32_t size, int from_conn)
{
    if (to == 0) {
        /* To everyone: the local players other than the sender get it. */
        for (int i = 0; i < PLAYERS; i++) {
            if (dp.players[i].used && dp.players[i].local && dp.players[i].id != from) {
                enqueue(from, 0, data, size);
                break;
            }
        }
    } else {
        player *p = find_player(to);
        if (p && p->local) {
            enqueue(from, to, data, size);
        }
    }
    if (!dp.host) {
        return;
    }
    uint8_t *f = malloc(size + 8);
    if (!f) {
        return;
    }
    memcpy(f, &from, 4);
    memcpy(f + 4, &to, 4);
    memcpy(f + 8, data, size);
    for (int i = 0; i < dp.nconns; i++) {
        if (i == from_conn || dp.conns[i].s == INVALID_SOCKET) {
            continue;
        }
        bool wanted = to == 0;
        for (int k = 0; !wanted && k < PLAYERS; k++) {
            wanted = dp.players[k].used && dp.players[k].id == to && dp.players[k].conn == i;
        }
        if (wanted) {
            send_frame(&dp.conns[i], F_DATA, f, size + 8);
        }
    }
    free(f);
}

static void handle_frame(int ci, uint8_t type, const uint8_t *p, uint32_t n)
{
    conn *k = &dp.conns[ci];
    player r;
    uint32_t id;
    switch (type) {
    case F_CREATE_REQ: /* host: a client's new player */
        if (dp.host && unpack_player(p, n, &r)) {
            r.id = dp.next_id++;
            player *np = add_from(&r, false, ci);
            if (np) {
                send_frame(k, F_PLAYER_ID, &np->id, 4);
                broadcast_player(F_PLAYER_NEW, np, ci);
                sys_player(DPSYS_CREATEPLAYERORGROUP, np);
            }
        }
        break;
    case F_DESTROY_REQ:
        if (dp.host && n >= 4) {
            memcpy(&id, p, 4);
            player *gone = find_player(id);
            if (gone) {
                sys_player(DPSYS_DESTROYPLAYERORGROUP, gone);
                free_player(gone);
                broadcast(F_PLAYER_GONE, &id, 4, ci);
            }
        }
        break;
    case F_WELCOME: /* client: the session's players */
        for (uint32_t o = 0, len; o < n && (len = unpack_player(p + o, n - o, &r)) != 0; o += len) {
            if (!find_player(r.id)) {
                add_from(&r, false, 0);
            }
        }
        break;
    case F_PLAYER_ID:
        if (n >= 4) {
            memcpy(&dp.pending_id, p, 4);
        }
        break;
    case F_PLAYER_NEW:
        if (unpack_player(p, n, &r) && !find_player(r.id)) {
            player *np = add_from(&r, false, 0);
            if (np) {
                sys_player(DPSYS_CREATEPLAYERORGROUP, np);
            }
        }
        break;
    case F_PLAYER_DATA: /* a player's new data: keep it, pass it on (host), tell the game */
        if (unpack_player(p, n, &r)) {
            player *q = find_player(r.id);
            if (q && !q->local) {
                set_data(q, r.data, r.data_size);
                if (dp.host) {
                    broadcast_player(F_PLAYER_DATA, q, ci);
                }
                sys_player(DPSYS_SETPLAYERORGROUPDATA, q);
            }
        }
        break;
    case F_PLAYER_GONE:
        if (n >= 4) {
            memcpy(&id, p, 4);
            player *gone = find_player(id);
            if (gone) {
                sys_player(DPSYS_DESTROYPLAYERORGROUP, gone);
                free_player(gone);
            }
        }
        break;
    case F_DATA:
        if (n >= 8) {
            uint32_t from, to;
            memcpy(&from, p, 4);
            memcpy(&to, p + 4, 4);
            route(from, to, p + 8, n - 8, ci);
        }
        break;
    default:
        break;
    }
}

static void drop_conn(int ci)
{
    conn *k = &dp.conns[ci];
    CLOSESOCK(k->s);
    k->s = INVALID_SOCKET;
    if (dp.host) {
        for (int i = 0; i < PLAYERS; i++) {
            player *p = &dp.players[i];
            if (p->used && !p->local && p->conn == ci) {
                sys_player(DPSYS_DESTROYPLAYERORGROUP, p);
                broadcast(F_PLAYER_GONE, &p->id, 4, ci);
                free_player(p);
            }
        }
    } else {
        uint32_t lost[1] = {DPSYS_SESSIONLOST};
        enqueue(0, 0, lost, 4);
        dp.open = false;
    }
}

/* Everything that arrived: discovery queries (host), new connections (host), frames. */
static void poll_net(void)
{
    if (!net_ready) {
        return;
    }
    uint8_t buf[2048];
    if (dp.host && dp.udp != INVALID_SOCKET) {
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        int n;
        while ((n = (int)recvfrom(dp.udp, (char *)buf, sizeof buf, 0, (struct sockaddr *)&from, &fl)) >= 20) {
            if (memcmp(buf, "CDPQ", 4) != 0) {
                continue;
            }
            uint8_t r[128] = {0};
            uint32_t cur = 0;
            for (int i = 0; i < PLAYERS; i++) {
                cur += dp.players[i].used;
            }
            uint16_t port = 0;
            struct sockaddr_in la;
            socklen_t ll = sizeof la;
            if (getsockname(dp.listener, (struct sockaddr *)&la, &ll) == 0) {
                port = ntohs(la.sin_port);
            }
            memcpy(r, "CDPR", 4);
            memcpy(r + 4, dp.guid_instance, 16);
            memcpy(r + 20, dp.guid_app, 16);
            memcpy(r + 36, &dp.max_players, 4);
            memcpy(r + 40, &cur, 4);
            memcpy(r + 44, dp.user, 16);
            memcpy(r + 60, &dp.flags, 4);
            memcpy(r + 64, &port, 2);
            memcpy(r + 66, dp.name, 62);
            sendto(dp.udp, (const char *)r, sizeof r, 0, (struct sockaddr *)&from, fl);
        }
    }
    if (dp.host && dp.listener != INVALID_SOCKET) {
        SOCKET s;
        while ((s = accept(dp.listener, NULL, NULL)) != INVALID_SOCKET) {
            int ci = dp.nconns < CONNS ? dp.nconns++ : -1;
            for (int i = 0; ci < 0 && i < dp.nconns; i++) {
                if (dp.conns[i].s == INVALID_SOCKET) {
                    ci = i;
                }
            }
            if (ci < 0) {
                CLOSESOCK(s);
                continue;
            }
            NONBLOCK(s);
            int one = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
            dp.conns[ci] = (conn){.s = s};
            /* Welcome: the players so far. */
            uint8_t *all = NULL;
            uint32_t n = 0;
            for (int i = 0; i < PLAYERS; i++) {
                uint32_t len;
                uint8_t *rec = dp.players[i].used ? pack_player(&dp.players[i], &len) : NULL;
                uint8_t *grown = rec ? realloc(all, n + len) : NULL;
                if (grown) {
                    all = grown;
                    memcpy(all + n, rec, len);
                    n += len;
                }
                free(rec);
            }
            send_frame(&dp.conns[ci], F_WELCOME, all, n);
            free(all);
        }
    }
    for (int ci = 0; ci < dp.nconns; ci++) {
        conn *k = &dp.conns[ci];
        if (k->s == INVALID_SOCKET) {
            continue;
        }
        for (;;) {
            int n = (int)recv(k->s, (char *)buf, sizeof buf, 0);
            if (n == 0 || (n < 0 && !WOULDBLOCK())) {
                drop_conn(ci);
                break;
            }
            if (n < 0) {
                break;
            }
            if (k->in_len + (uint32_t)n > k->in_cap) {
                k->in_cap = (k->in_len + (uint32_t)n) * 2;
                k->in = realloc(k->in, k->in_cap);
            }
            memcpy(k->in + k->in_len, buf, (size_t)n);
            k->in_len += (uint32_t)n;
            uint32_t at = 0;
            while (k->in_len - at >= 5) {
                uint32_t len;
                memcpy(&len, k->in + at, 4);
                if (len == 0 || k->in_len - at - 4 < len) {
                    break;
                }
                handle_frame(ci, k->in[at + 4], k->in + at + 5, len - 1);
                if (k->s == INVALID_SOCKET) {
                    break;
                }
                at += 4 + len;
            }
            memmove(k->in, k->in + at, k->in_len - at);
            k->in_len -= at;
        }
    }
}

/* ---- the object ---- */

static const char *const dp_methods[] = {
    "QueryInterface", "AddRef", "Release", "AddPlayerToGroup", "Close", "CreateGroup", "CreatePlayer",
    "DeletePlayerFromGroup", "DestroyGroup", "DestroyPlayer", "EnumGroupPlayers", "EnumGroups",
    "EnumPlayers", "EnumSessions", "GetCaps", "GetGroupData", "GetGroupName", "GetMessageCount",
    "GetPlayerAddress", "GetPlayerCaps", "GetPlayerData", "GetPlayerName", "GetSessionDesc",
    "Initialize", "Open", "Receive", "Send", "SetGroupData", "SetGroupName", "SetPlayerData",
    "SetPlayerName", "SetSessionDesc", "AddGroupToGroup", "CreateGroupInGroup", "DeleteGroupFromGroup",
    "EnumConnections", "EnumGroupsInGroup", "GetGroupConnectionSettings", "InitializeConnection",
    "SecureOpen", "SendChatMessage", "SetGroupConnectionSettings", "StartSession", "GetGroupFlags",
    "GetGroupParent", "GetPlayerAccount", "GetPlayerFlags", "GetGroupOwner", "SetGroupOwner", "SendEx",
    "GetMessageQueue", "CancelMessage", "CancelPriority",
};

static uint32_t refs;

static uint32_t create(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    static uint32_t vt, obj;
    if (!vt) {
        uint32_t n = sizeof dp_methods / sizeof *dp_methods;
        vt = rc_heap_alloc(4 * n, false);
        for (uint32_t i = 0; i < n; i++) {
            char buf[80];
            snprintf(buf, sizeof buf, "IDirectPlay4A::%s", dp_methods[i]);
            rc_st32(vt + 4 * i, rc_import_address(COM, rc_str(rc_proc_str(buf))));
        }
        obj = rc_heap_alloc(8, true);
        rc_st32(obj, vt);
    }
    net_init();
    refs++;
    return obj;
}

static void m_QueryInterface(rc_cpu *c)
{
    refs++;
    rc_st32(RC_ARG(c, 2), RC_ARG(c, 0));
    RC_RETURN(c, DP_OK, 3);
}
static void m_AddRef(rc_cpu *c) { RC_RETURN(c, ++refs, 1); }
static void m_Release(rc_cpu *c) { RC_RETURN(c, refs ? --refs : 0, 1); }


/* EnumConnections(this, app guid, callback, context, flags): TCP/IP. */
static void m_EnumConnections(rc_cpu *c)
{
    static const uint8_t sp_tcpip[16] = {0xe0, 0x5e, 0xe9, 0x36, 0x77, 0x85, 0xcf, 0x11, 0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82};
    static const uint8_t aid_sp[16] = {0xc0, 0x16, 0xd9, 0x07, 0xaf, 0xe0, 0xcf, 0x11, 0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e};
    uint32_t cb = RC_ARG(c, 2), ctx = RC_ARG(c, 3);
    uint32_t guid = rc_heap_alloc(16, false), addr = rc_heap_alloc(40, true), name = rc_heap_alloc(16, true);
    memcpy(rc_ptr(guid), sp_tcpip, 16);
    /* The connection: one DPADDRESS element, DPAID_ServiceProvider = TCP/IP. */
    memcpy(rc_ptr(addr), aid_sp, 16);
    rc_st32(addr + 16, 16);
    memcpy(rc_ptr(addr + 20), sp_tcpip, 16);
    rc_st32(name, 16);
    static uint32_t sp_name;
    if (!sp_name) {
        sp_name = rc_proc_str("Internet TCP/IP Connection For DirectPlay");
    }
    rc_st32(name + 8, sp_name);
    rc_st32(name + 12, rc_ld32(name + 8));
    uint32_t args[6] = {guid, addr, 36, name, 0, ctx};
    rc_call_guest(c, cb, 6, args);
    RC_RETURN(c, DP_OK, 5);
}

/* The host address a player typed (DPAID_INet in the connection), queried besides the LAN. */
static char typed_host[64];

/* InitializeConnection(this, connection, flags): a DPADDRESS, a chain of (guid, size, data). */
static void m_InitializeConnection(rc_cpu *c)
{
    static const uint8_t aid_inet[16] = {0xa0, 0x4d, 0xa5, 0xc4, 0xaf, 0xe0, 0xcf, 0x11, 0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e};
    uint32_t a = RC_ARG(c, 1);
    typed_host[0] = 0;
    for (int i = 0; a && i < 16; i++) {
        uint32_t n = rc_ld32(a + 16);
        if (n > 4096) {
            break;
        }
        if (memcmp(rc_ptr(a), aid_inet, 16) == 0 && n) {
            uint32_t k = n < sizeof typed_host ? n : sizeof typed_host - 1;
            memcpy(typed_host, rc_ptr(a + 20), k);
            typed_host[k] = 0;
            break;
        }
        if (memcmp(rc_ptr(a), aid_inet, 4) != 0 && rc_ld32(a) == 0 && n == 0) {
            break;
        }
        a += 20 + n;
    }
    RC_RETURN(c, DP_OK, 3);
}

/* Session discovery: queries go out from one socket; answers are collected whenever the game
 * asks (EnumSessions), so the asynchronous mode the games use never blocks. */
static SOCKET disc = INVALID_SOCKET;
static uint64_t disc_sent; /* real time of the last queries; 0: no enumeration running */

static void disc_query(void)
{
    if (disc == INVALID_SOCKET) {
        disc = socket(AF_INET, SOCK_DGRAM, 0);
        if (disc == INVALID_SOCKET) {
            return;
        }
        int one = 1;
        setsockopt(disc, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof one);
        NONBLOCK(disc);
    }
    uint8_t q[20] = {0};
    memcpy(q, "CDPQ", 4);
    const char *targets[4] = {"255.255.255.255", "127.0.0.1", getenv("RC_DPLAY_HOST"), typed_host};
    for (int i = 0; i < 4; i++) {
        if (!targets[i] || !*targets[i]) {
            continue;
        }
        struct sockaddr_in to = {0};
        to.sin_family = AF_INET;
        to.sin_port = htons(DISCOVERY_PORT);
        if (inet_pton(AF_INET, targets[i], &to.sin_addr) != 1) {
            struct addrinfo hints = {0}, *res = NULL;
            hints.ai_family = AF_INET;
            if (getaddrinfo(targets[i], NULL, &hints, &res) != 0 || !res) {
                continue;
            }
            to.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
            freeaddrinfo(res);
        }
        sendto(disc, (const char *)q, sizeof q, 0, (struct sockaddr *)&to, sizeof to);
    }
    disc_sent = real_ms();
}

/* Take the answers that arrived (a session already known is updated). */
static void disc_collect(void)
{
    if (disc == INVALID_SOCKET) {
        return;
    }
    uint8_t r[256];
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    int n;
    while ((n = (int)recvfrom(disc, (char *)r, sizeof r, 0, (struct sockaddr *)&from, &fl)) >= 128) {
        if (memcmp(r, "CDPR", 4) != 0) {
            continue;
        }
        int i = 0;
        while (i < nfound && memcmp(found[i].guid_instance, r + 4, 16) != 0) {
            i++;
        }
        if (i == nfound) {
            if (nfound == FOUND) {
                continue;
            }
            nfound++;
        }
        uint16_t port;
        memcpy(found[i].guid_instance, r + 4, 16);
        memcpy(found[i].guid_app, r + 20, 16);
        memcpy(&found[i].max_players, r + 36, 4);
        memcpy(&found[i].cur_players, r + 40, 4);
        memcpy(found[i].user, r + 44, 16);
        memcpy(&found[i].flags, r + 60, 4);
        memcpy(&port, r + 64, 2);
        memcpy(found[i].name, r + 66, 62);
        found[i].name[62] = 0;
        found[i].addr = from;
        found[i].addr.sin_port = htons(port);
        fl = sizeof from;
    }
}

static void put_desc(uint32_t d, const uint8_t inst[16], const uint8_t app[16], uint32_t maxp, uint32_t cur,
                     uint32_t flags, const uint32_t user[4], const char *name)
{
    memset(rc_ptr(d), 0, 80);
    rc_st32(d, 80);
    rc_st32(d + 4, flags);
    memcpy(rc_ptr(d + 8), inst, 16);
    memcpy(rc_ptr(d + 24), app, 16);
    rc_st32(d + 40, maxp);
    rc_st32(d + 44, cur);
    /* The name follows the description (callers leave room for it). */
    uint32_t ln = (uint32_t)strnlen(name, 63);
    memcpy(rc_ptr(d + 80), name, ln);
    rc_st8(d + 80 + ln, 0);
    rc_st32(d + 48, d + 80);
    for (int i = 0; i < 4; i++) {
        rc_st32(d + 64 + 4 * (uint32_t)i, user[i]);
    }
}

/* EnumSessions(this, desc, timeout, callback, context, flags). Synchronous: ask, wait the
 * timeout (at most 3 s), report. DPENUMSESSIONS_ASYNC (0x10): report what is known and keep
 * asking in the background; with DPENUMSESSIONS_RETURNSTATUS (0x80) the first call answers
 * DPERR_CONNECTING while the first answers are on their way, as DirectPlay does. */
#define DPERR_CONNECTING 0x8877015eu
static void m_EnumSessions(rc_cpu *c)
{
    uint32_t timeout = RC_ARG(c, 2), cb = RC_ARG(c, 3), ctx = RC_ARG(c, 4), flags = RC_ARG(c, 5);
    net_init();
    if (flags & 0x10) {
        if (!disc_sent) {
            nfound = 0;
            disc_query();
            if (flags & 0x80) {
                RC_RETURN(c, DPERR_CONNECTING, 6);
            }
        } else if (real_ms() - disc_sent > 1000) {
            disc_query();
        }
        disc_collect();
    } else {
        nfound = 0;
        disc_query();
        uint64_t until = real_ms() + (timeout && timeout < 3000 ? timeout : 1000);
        do {
            real_sleep(10);
            disc_collect();
        } while (real_ms() < until);
        disc_sent = 0;
    }
    if (rc_trace("dplay")) {
        rc_log("[dplay] EnumSessions flags 0x%x: %d sessions", flags, nfound);
    }
    uint32_t d = rc_heap_alloc(80 + 64, true), t = rc_heap_alloc(4, true);
    for (int i = 0; i < nfound; i++) {
        put_desc(d, found[i].guid_instance, found[i].guid_app, found[i].max_players, found[i].cur_players,
                 found[i].flags, found[i].user, found[i].name);
        rc_st32(t, timeout);
        uint32_t args[4] = {d, t, 0, ctx};
        if (!rc_call_guest(c, cb, 4, args)) {
            break;
        }
    }
    uint32_t args[4] = {0, t, 1 /* DPESC_TIMEDOUT */, ctx};
    rc_call_guest(c, cb, 4, args);
    rc_heap_free(d);
    rc_heap_free(t);
    RC_RETURN(c, DP_OK, 6);
}

/* Enumeration stops when a session is opened or closed. */
static void disc_stop(void)
{
    if (disc != INVALID_SOCKET) {
        CLOSESOCK(disc);
        disc = INVALID_SOCKET;
    }
    disc_sent = 0;
}

/* Open(this, desc, flags): DPOPEN_CREATE (2) hosts, DPOPEN_JOIN (1) joins a found session. */
static uint32_t open_session(uint32_t desc, uint32_t flags)
{
    net_init();
    disc_collect();
    disc_stop();
    if (flags & 2) {
        dp.host = true;
        for (int i = 0; i < 16; i++) {
            dp.guid_instance[i] = (uint8_t)(rand() ^ (int)(real_ms() >> i));
        }
        memcpy(dp.guid_app, rc_ptr(desc + 24), 16);
        dp.max_players = rc_ld32(desc + 40);
        dp.flags = rc_ld32(desc + 4);
        for (int i = 0; i < 4; i++) {
            dp.user[i] = rc_ld32(desc + 64 + 4 * (uint32_t)i);
        }
        const char *n = rc_str(rc_ld32(desc + 48));
        snprintf(dp.name, sizeof dp.name, "%s", n ? n : "Game");
        dp.next_id = 0x100;
        dp.udp = socket(AF_INET, SOCK_DGRAM, 0);
        int one = 1;
        setsockopt(dp.udp, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
        struct sockaddr_in a = {0};
        a.sin_family = AF_INET;
        a.sin_port = htons(DISCOVERY_PORT);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(dp.udp, (struct sockaddr *)&a, sizeof a) != 0) {
            rc_log("[dplay] discovery port %d is taken: others will not find this game", DISCOVERY_PORT);
        }
        NONBLOCK(dp.udp);
        dp.listener = socket(AF_INET, SOCK_STREAM, 0);
        a.sin_port = 0;
        bind(dp.listener, (struct sockaddr *)&a, sizeof a);
        listen(dp.listener, 8);
        NONBLOCK(dp.listener);
        dp.open = true;
        return DP_OK;
    }
    for (int i = 0; i < nfound; i++) {
        if (memcmp(found[i].guid_instance, rc_ptr(desc + 8), 16) != 0) {
            continue;
        }
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(s, (struct sockaddr *)&found[i].addr, sizeof found[i].addr) != 0) {
            CLOSESOCK(s);
            return DPERR_NOSESSIONS;
        }
        NONBLOCK(s);
        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
        dp.conns[0] = (conn){.s = s};
        dp.nconns = 1;
        dp.host = false;
        memcpy(dp.guid_instance, found[i].guid_instance, 16);
        memcpy(dp.guid_app, found[i].guid_app, 16);
        dp.max_players = found[i].max_players;
        dp.flags = found[i].flags;
        memcpy(dp.user, found[i].user, 16);
        snprintf(dp.name, sizeof dp.name, "%s", found[i].name);
        dp.open = true;
        /* The welcome with the players so far. */
        uint64_t until = real_ms() + 2000;
        while (real_ms() < until) {
            poll_net();
            real_sleep(10);
            bool any = false;
            for (int k = 0; k < PLAYERS; k++) {
                any |= dp.players[k].used;
            }
            if (any) {
                break;
            }
        }
        return DP_OK;
    }
    return DPERR_NOSESSIONS;
}

static void m_Open(rc_cpu *c) { RC_RETURN(c, open_session(RC_ARG(c, 1), RC_ARG(c, 2)), 3); }

/* SecureOpen(this, desc, flags, security, credentials): no security here. */
static void m_SecureOpen(rc_cpu *c) { RC_RETURN(c, open_session(RC_ARG(c, 1), RC_ARG(c, 2)), 5); }

static void names_from(uint32_t dpname, char sn[64], char ln[64])
{
    sn[0] = ln[0] = 0;
    if (!dpname) {
        return;
    }
    const char *s = rc_str(rc_ld32(dpname + 8)), *l = rc_str(rc_ld32(dpname + 12));
    snprintf(sn, 64, "%s", s ? s : "");
    snprintf(ln, 64, "%s", l ? l : "");
}

/* CreatePlayer(this, id*, name, event, data, size, flags) */
static void m_CreatePlayer(rc_cpu *c)
{
    player r = {.flags = RC_ARG(c, 6)};
    uint32_t event = RC_ARG(c, 3);
    names_from(RC_ARG(c, 2), r.short_name, r.long_name);
    r.data = RC_ARG(c, 5) ? rc_ptr(RC_ARG(c, 4)) : NULL;
    r.data_size = r.data ? RC_ARG(c, 5) : 0;
    if (dp.host) {
        r.id = dp.next_id++;
        player *p = add_from(&r, true, -1);
        if (p) {
            p->event = event;
            broadcast_player(F_PLAYER_NEW, p, -1);
        }
    } else if (dp.nconns) {
        uint32_t len;
        uint8_t *rec = pack_player(&r, &len);
        dp.pending_id = 0;
        if (rec) {
            send_frame(&dp.conns[0], F_CREATE_REQ, rec, len);
            free(rec);
        }
        uint64_t until = real_ms() + 3000;
        while (!dp.pending_id && real_ms() < until && dp.conns[0].s != INVALID_SOCKET) {
            poll_net();
            real_sleep(5);
        }
        if (!dp.pending_id) {
            RC_RETURN(c, DPERR_GENERIC, 7);
        }
        r.id = dp.pending_id;
        player *p = add_from(&r, true, 0);
        if (p) {
            p->event = event;
            if (dp.queue) {
                rc_k32_set_event(event);
            }
        }
    } else {
        RC_RETURN(c, DPERR_GENERIC, 7);
    }
    rc_st32(RC_ARG(c, 1), r.id);
    RC_RETURN(c, DP_OK, 7);
}

static void m_DestroyPlayer(rc_cpu *c)
{
    uint32_t id = RC_ARG(c, 1);
    player *p = find_player(id);
    if (!p) {
        RC_RETURN(c, DPERR_INVALIDPLAYER, 2);
    }
    free_player(p);
    if (dp.host) {
        broadcast(F_PLAYER_GONE, &id, 4, -1);
    } else if (dp.nconns) {
        send_frame(&dp.conns[0], F_DESTROY_REQ, &id, 4);
    }
    RC_RETURN(c, DP_OK, 2);
}

/* SetPlayerData(this, id, data, size, flags): kept here and sent to every machine. */
static void m_SetPlayerData(rc_cpu *c)
{
    player *p = find_player(RC_ARG(c, 1));
    if (!p) {
        RC_RETURN(c, DPERR_INVALIDPLAYER, 5);
    }
    uint32_t size = RC_ARG(c, 3);
    set_data(p, size ? rc_ptr(RC_ARG(c, 2)) : NULL, size);
    if (!(RC_ARG(c, 4) & 1)) { /* not DPSET_LOCAL */
        uint32_t len;
        uint8_t *rec = pack_player(p, &len);
        if (rec) {
            broadcast(F_PLAYER_DATA, rec, len, -1);
            free(rec);
        }
    }
    RC_RETURN(c, DP_OK, 5);
}

/* GetPlayerData(this, id, data, size*, flags) */
static void m_GetPlayerData(rc_cpu *c)
{
    player *p = find_player(RC_ARG(c, 1));
    if (!p) {
        RC_RETURN(c, DPERR_INVALIDPLAYER, 5);
    }
    uint32_t data = RC_ARG(c, 2), psize = RC_ARG(c, 3), cap = rc_ld32(psize);
    rc_st32(psize, p->data_size);
    if (p->data_size && (!data || cap < p->data_size)) {
        RC_RETURN(c, DPERR_BUFFERTOOSMALL, 5);
    }
    if (p->data_size) {
        memcpy(rc_ptr(data), p->data, p->data_size);
    }
    RC_RETURN(c, DP_OK, 5);
}

static void close_all(void)
{
    for (int i = 0; i < dp.nconns; i++) {
        if (dp.conns[i].s != INVALID_SOCKET) {
            CLOSESOCK(dp.conns[i].s);
        }
        free(dp.conns[i].in);
    }
    if (dp.udp != INVALID_SOCKET) {
        CLOSESOCK(dp.udp);
    }
    if (dp.listener != INVALID_SOCKET) {
        CLOSESOCK(dp.listener);
    }
    for (int i = 0; i < PLAYERS; i++) {
        free_player(&dp.players[i]);
    }
    while (dp.queue) {
        msg *m = dp.queue;
        dp.queue = m->next;
        free(m->data);
        free(m);
    }
    memset(&dp, 0, sizeof dp);
    dp.udp = dp.listener = INVALID_SOCKET;
}

static void m_Close(rc_cpu *c)
{
    disc_stop();
    close_all();
    RC_RETURN(c, DP_OK, 1);
}

/* Send(this, from, to, flags, data, size) and SendEx(..., priority, timeout, context, id*) */
static void send_msg(uint32_t from, uint32_t to, uint32_t data, uint32_t size)
{
    if (++n_sent % 500 == 0 && rc_trace("dplay")) {
        rc_log("[dplay] %u messages sent, %u received", n_sent, n_received);
    }
    const uint8_t *d = rc_ptr(data);
    if (dp.host) {
        route(from, to, d, size, -1);
        return;
    }
    if (to == 0 || !find_player(to) || !find_player(to)->local) {
        if (dp.nconns && dp.conns[0].s != INVALID_SOCKET) {
            uint8_t *f = malloc(size + 8);
            if (f) {
                memcpy(f, &from, 4);
                memcpy(f + 4, &to, 4);
                memcpy(f + 8, d, size);
                send_frame(&dp.conns[0], F_DATA, f, size + 8);
                free(f);
            }
        }
    }
    if (to != 0) {
        player *p = find_player(to);
        if (p && p->local) {
            enqueue(from, to, d, size);
        }
    } else {
        /* To all: the other local players get it too (not the sender itself). */
        for (int i = 0; i < PLAYERS; i++) {
            if (dp.players[i].used && dp.players[i].local && dp.players[i].id != from) {
                enqueue(from, dp.players[i].id, d, size);
                break;
            }
        }
    }
}

static void m_Send(rc_cpu *c)
{
    send_msg(RC_ARG(c, 1), RC_ARG(c, 2), RC_ARG(c, 4), RC_ARG(c, 5));
    RC_RETURN(c, DP_OK, 6);
}

static void m_SendEx(rc_cpu *c)
{
    send_msg(RC_ARG(c, 1), RC_ARG(c, 2), RC_ARG(c, 4), RC_ARG(c, 5));
    if (RC_ARG(c, 9)) {
        rc_st32(RC_ARG(c, 9), 0);
    }
    RC_RETURN(c, DP_OK, 10);
}

/* Receive(this, from*, to*, flags, data, size*) */
static void m_Receive(rc_cpu *c)
{
    poll_net();
    uint32_t pfrom = RC_ARG(c, 1), pto = RC_ARG(c, 2), flags = RC_ARG(c, 3), data = RC_ARG(c, 4), psize = RC_ARG(c, 5);
    msg *m = dp.queue, *prev = NULL;
    for (; m; prev = m, m = m->next) {
        if ((flags & 2) && pto && m->to && m->to != rc_ld32(pto)) { /* DPRECEIVE_TOPLAYER */
            continue;
        }
        if ((flags & 4) && pfrom && m->from != rc_ld32(pfrom)) { /* DPRECEIVE_FROMPLAYER */
            continue;
        }
        break;
    }
    if (!m) {
        RC_RETURN(c, DPERR_NOMESSAGES, 6);
    }
    uint32_t cap = rc_ld32(psize);
    rc_st32(psize, m->size);
    if (!data || cap < m->size) {
        RC_RETURN(c, DPERR_BUFFERTOOSMALL, 6);
    }
    memcpy(rc_ptr(data), m->data, m->size);
    if (m->from == 0 && m->size >= SYS_HEAD + 128) {
        /* Point the system message at the names and the data that follow it (see sys_player). */
        uint32_t type = rc_ld32(data), names = data + SYS_HEAD, pdata = data + SYS_HEAD + 128;
        uint32_t dsize = m->size - SYS_HEAD - 128;
        if (type == DPSYS_CREATEPLAYERORGROUP) {
            rc_st32(data + 12, (uint32_t)nplayers());
            rc_st32(data + 16, dsize ? pdata : 0);
            rc_st32(data + 20, dsize);
            rc_st32(data + 24, 16);
            rc_st32(data + 32, names);
            rc_st32(data + 36, names + 64);
        } else if (type == DPSYS_DESTROYPLAYERORGROUP) {
            rc_st32(data + 20, dsize ? pdata : 0);
            rc_st32(data + 24, dsize);
            rc_st32(data + 28, 16);
            rc_st32(data + 36, names);
            rc_st32(data + 40, names + 64);
        } else if (type == DPSYS_SETPLAYERORGROUPDATA) {
            rc_st32(data + 12, dsize ? pdata : 0);
            rc_st32(data + 16, dsize);
        }
    }
    n_received += m->from != 0;
    rc_st32(pfrom, m->from);
    if (pto) {
        rc_st32(pto, m->to);
    }
    if (!(flags & 8)) { /* not DPRECEIVE_PEEK */
        if (prev) {
            prev->next = m->next;
        } else {
            dp.queue = m->next;
        }
        if (dp.tail == m) {
            dp.tail = prev;
        }
        free(m->data);
        free(m);
    }
    RC_RETURN(c, DP_OK, 6);
}

/* GetMessageCount(this, id, count*) */
static void m_GetMessageCount(rc_cpu *c)
{
    poll_net();
    uint32_t n = 0;
    for (msg *m = dp.queue; m; m = m->next) {
        n++;
    }
    rc_st32(RC_ARG(c, 2), n);
    RC_RETURN(c, DP_OK, 3);
}

/* GetMessageQueue(this, from, to, flags, messages*, bytes*) */
static void m_GetMessageQueue(rc_cpu *c)
{
    poll_net();
    /* DPMESSAGEQUEUE_SEND (1): sends leave at once, that queue is empty. RECEIVE (2): ours. */
    uint32_t n = 0, bytes = 0, to = RC_ARG(c, 2);
    for (msg *m = (RC_ARG(c, 3) & 2) ? dp.queue : NULL; m; m = m->next) {
        if (!to || !m->to || m->to == to) {
            n++;
            bytes += m->size;
        }
    }
    if (RC_ARG(c, 4)) {
        rc_st32(RC_ARG(c, 4), n);
    }
    if (RC_ARG(c, 5)) {
        rc_st32(RC_ARG(c, 5), bytes);
    }
    RC_RETURN(c, DP_OK, 6);
}

/* EnumPlayers(this, guid, callback, context, flags) */
static void m_EnumPlayers(rc_cpu *c)
{
    poll_net();
    /* Filter: DPENUMPLAYERS_LOCAL (8) or _REMOTE (2) only; the callback's flags say which a
     * player is (games find their own player that way), plus server player and spectator. */
    uint32_t cb = RC_ARG(c, 2), ctx = RC_ARG(c, 3), want = RC_ARG(c, 4);
    uint32_t name = rc_heap_alloc(16 + 128, true);
    for (int i = 0; i < PLAYERS; i++) {
        player *p = &dp.players[i];
        if (!p->used || ((want & 8) && !p->local) || ((want & 2) && p->local)) {
            continue;
        }
        rc_st32(name, 16);
        memcpy(rc_ptr(name + 16), p->short_name, 64);
        memcpy(rc_ptr(name + 80), p->long_name, 64);
        rc_st32(name + 8, name + 16);
        rc_st32(name + 12, name + 80);
        uint32_t flags = (p->flags & 0x300u) | (p->local ? 8u : 2u);
        uint32_t args[5] = {p->id, 1 /* DPPLAYERTYPE_PLAYER */, name, flags, ctx};
        if (!rc_call_guest(c, cb, 5, args)) {
            break;
        }
    }
    rc_heap_free(name);
    RC_RETURN(c, DP_OK, 5);
}

/* GetPlayerName(this, id, data, size*): a DPNAME and its strings. */
static void m_GetPlayerName(rc_cpu *c)
{
    player *p = find_player(RC_ARG(c, 1));
    if (!p) {
        RC_RETURN(c, DPERR_INVALIDPLAYER, 4);
    }
    uint32_t data = RC_ARG(c, 2), psize = RC_ARG(c, 3);
    uint32_t ls = (uint32_t)strlen(p->short_name) + 1, ll = (uint32_t)strlen(p->long_name) + 1;
    uint32_t need = 16 + ls + ll, cap = rc_ld32(psize);
    rc_st32(psize, need);
    if (!data || cap < need) {
        RC_RETURN(c, DPERR_BUFFERTOOSMALL, 4);
    }
    rc_st32(data, 16);
    rc_st32(data + 4, 0);
    memcpy(rc_ptr(data + 16), p->short_name, ls);
    memcpy(rc_ptr(data + 16 + ls), p->long_name, ll);
    rc_st32(data + 8, data + 16);
    rc_st32(data + 12, data + 16 + ls);
    RC_RETURN(c, DP_OK, 4);
}

/* GetSessionDesc(this, data, size*) */
static void m_GetSessionDesc(rc_cpu *c)
{
    uint32_t data = RC_ARG(c, 1), psize = RC_ARG(c, 2);
    uint32_t ln = (uint32_t)strlen(dp.name) + 1, need = 80 + ln, cap = rc_ld32(psize);
    rc_st32(psize, need);
    if (!data || cap < need) {
        RC_RETURN(c, DPERR_BUFFERTOOSMALL, 3);
    }
    uint32_t cur = 0;
    for (int i = 0; i < PLAYERS; i++) {
        cur += dp.players[i].used;
    }
    put_desc(data, dp.guid_instance, dp.guid_app, dp.max_players, cur, dp.flags, dp.user, dp.name);
    RC_RETURN(c, DP_OK, 3);
}

/* SetSessionDesc(this, desc, flags): the host's own copy (clients keep theirs). */
static void m_SetSessionDesc(rc_cpu *c)
{
    uint32_t d = RC_ARG(c, 1);
    dp.max_players = rc_ld32(d + 40);
    dp.flags = rc_ld32(d + 4);
    for (int i = 0; i < 4; i++) {
        dp.user[i] = rc_ld32(d + 64 + 4 * (uint32_t)i);
    }
    RC_RETURN(c, DP_OK, 3);
}

/* GetCaps(this, DPCAPS*, flags) */
static void m_GetCaps(rc_cpu *c)
{
    uint32_t p = RC_ARG(c, 1);
    memset(rc_ptr(p + 4), 0, 36);
    rc_st32(p + 4, 0x4 | 0x40);  /* DPCAPS_GUARANTEEDOPTIMIZED | DPCAPS_ASYNCSUPPORTED */
    rc_st32(p + 8, 65536);       /* max buffer size */
    rc_st32(p + 16, 8);          /* max players */
    rc_st32(p + 20, 1000000);    /* hundred baud */
    rc_st32(p + 24, 20);         /* latency, ms */
    rc_st32(p + 28, 8);          /* max local players */
    rc_st32(p + 36, 5000);       /* timeout, ms */
    if (dp.host) {
        rc_st32(p + 4, rc_ld32(p + 4) | 0x2); /* DPCAPS_ISHOST */
    }
    RC_RETURN(c, DP_OK, 3);
}

static void m_GetPlayerCaps(rc_cpu *c)
{
    uint32_t args1 = RC_ARG(c, 2);
    memset(rc_ptr(args1 + 4), 0, 36);
    rc_st32(args1 + 8, 65536);
    rc_st32(args1 + 24, 20);
    RC_RETURN(c, DP_OK, 4);
}

static void m_GetPlayerFlags(rc_cpu *c)
{
    player *p = find_player(RC_ARG(c, 1));
    if (!p) {
        RC_RETURN(c, DPERR_INVALIDPLAYER, 3);
    }
    rc_st32(RC_ARG(c, 2), (p->flags & 0x300u) | (p->local ? 0x8u /* DPPLAYER_LOCAL */ : 0));
    RC_RETURN(c, DP_OK, 3);
}

/* Player data, group calls: accepted, nothing kept. */
/* GetPlayerAddress(this, id, data, size*): the service provider and the player's IP address. */
static void m_GetPlayerAddress(rc_cpu *c)
{
    static const uint8_t aid_sp[16] = {0xc0, 0x16, 0xd9, 0x07, 0xaf, 0xe0, 0xcf, 0x11, 0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e};
    static const uint8_t sp_tcpip[16] = {0xe0, 0x5e, 0xe9, 0x36, 0x77, 0x85, 0xcf, 0x11, 0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82};
    static const uint8_t aid_inet[16] = {0xa0, 0x4d, 0xa5, 0xc4, 0xaf, 0xe0, 0xcf, 0x11, 0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e};
    player *p = find_player(RC_ARG(c, 1));
    if (!p) {
        RC_RETURN(c, DPERR_INVALIDPLAYER, 4);
    }
    char ip[32] = "127.0.0.1";
    if (dp.host && !p->local && p->conn >= 0 && dp.conns[p->conn].s != INVALID_SOCKET) {
        struct sockaddr_in a;
        socklen_t al = sizeof a;
        if (getpeername(dp.conns[p->conn].s, (struct sockaddr *)&a, &al) == 0) {
            inet_ntop(AF_INET, &a.sin_addr, ip, sizeof ip);
        }
    }
    uint32_t il = (uint32_t)strlen(ip) + 1;
    uint32_t data = RC_ARG(c, 2), psize = RC_ARG(c, 3), need = 36 + 20 + il, cap = rc_ld32(psize);
    rc_st32(psize, need);
    if (!data || cap < need) {
        RC_RETURN(c, DPERR_BUFFERTOOSMALL, 4);
    }
    memcpy(rc_ptr(data), aid_sp, 16);
    rc_st32(data + 16, 16);
    memcpy(rc_ptr(data + 20), sp_tcpip, 16);
    memcpy(rc_ptr(data + 36), aid_inet, 16);
    rc_st32(data + 52, il);
    memcpy(rc_ptr(data + 56), ip, il);
    RC_RETURN(c, DP_OK, 4);
}

static void ok2(rc_cpu *c) { RC_RETURN(c, DP_OK, 2); }
static void ok3(rc_cpu *c) { RC_RETURN(c, DP_OK, 3); }
static void ok4(rc_cpu *c) { RC_RETURN(c, DP_OK, 4); }
static void ok5(rc_cpu *c) { RC_RETURN(c, DP_OK, 5); }
static void ok6(rc_cpu *c) { RC_RETURN(c, DP_OK, 6); }
static void m_CreateGroup(rc_cpu *c)
{
    rc_st32(RC_ARG(c, 1), dp.next_id++ | 0x80000000u);
    RC_RETURN(c, DP_OK, 6);
}

/* ---- IDirectPlayLobby3A: addresses only; the game is never started by a lobby. ---- */

#define LCOM "DPLAYLOBBY_COM"
#define DPERR_NOTLOBBIED 0x8877042eu

static const char *const lobby_methods[] = {
    "QueryInterface", "AddRef", "Release", "Connect", "CreateAddress", "EnumAddress", "EnumAddressTypes",
    "EnumLocalApplications", "GetConnectionSettings", "ReceiveLobbyMessage", "RunApplication",
    "SendLobbyMessage", "SetConnectionSettings", "SetLobbyMessageEvent", "CreateCompoundAddress", "ConnectEx",
    "RegisterApplication", "UnregisterApplication", "WaitForConnectionSettings",
};

static uint32_t lobby_refs;

static uint32_t create_lobby(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    static uint32_t vt, obj;
    lobby_refs++;
    if (!vt) {
        uint32_t n = sizeof lobby_methods / sizeof *lobby_methods;
        vt = rc_heap_alloc(4 * n, false);
        for (uint32_t i = 0; i < n; i++) {
            char buf[80];
            snprintf(buf, sizeof buf, "IDirectPlayLobby3A::%s", lobby_methods[i]);
            rc_st32(vt + 4 * i, rc_import_address(LCOM, rc_str(rc_proc_str(buf))));
        }
        obj = rc_heap_alloc(8, true);
        rc_st32(obj, vt);
    }
    return obj;
}

/* Write `n` address elements (guid, size, data) into the guest buffer if it fits. */
static uint32_t put_address(uint32_t out, uint32_t psize, uint32_t n, const uint32_t *guids, const uint32_t *sizes,
                            const uint32_t *datas)
{
    uint32_t need = 0;
    for (uint32_t i = 0; i < n; i++) {
        need += 20 + sizes[i];
    }
    uint32_t cap = rc_ld32(psize);
    rc_st32(psize, need);
    if (!out || cap < need) {
        return DPERR_BUFFERTOOSMALL;
    }
    for (uint32_t i = 0; i < n; i++) {
        memmove(rc_ptr(out), rc_ptr(guids[i]), 16);
        rc_st32(out + 16, sizes[i]);
        if (sizes[i]) {
            memmove(rc_ptr(out + 20), rc_ptr(datas[i]), sizes[i]);
        }
        out += 20 + sizes[i];
    }
    return DP_OK;
}

/* CreateAddress(this, sp guid, data type, data, size, address, size*) */
static void l_CreateAddress(rc_cpu *c)
{
    static uint32_t aid_sp;
    if (!aid_sp) {
        static const uint8_t g[16] = {0xc0, 0x16, 0xd9, 0x07, 0xaf, 0xe0, 0xcf, 0x11, 0x9c, 0x4e, 0x00, 0xa0, 0xc9, 0x05, 0x42, 0x5e};
        aid_sp = rc_heap_alloc(16, false);
        memcpy(rc_ptr(aid_sp), g, 16);
    }
    uint32_t guids[2] = {aid_sp, RC_ARG(c, 2)}, sizes[2] = {16, RC_ARG(c, 4)}, datas[2] = {RC_ARG(c, 1), RC_ARG(c, 3)};
    RC_RETURN(c, put_address(RC_ARG(c, 5), RC_ARG(c, 6), 2, guids, sizes, datas), 7);
}

/* CreateCompoundAddress(this, elements, count, address, size*): elements of guid, size, data*. */
static void l_CreateCompoundAddress(rc_cpu *c)
{
    uint32_t el = RC_ARG(c, 1), n = RC_ARG(c, 2);
    uint32_t guids[16], sizes[16], datas[16];
    if (n > 16) {
        RC_RETURN(c, DPERR_INVALIDPARAMS, 5);
    }
    for (uint32_t i = 0; i < n; i++) {
        guids[i] = el + 24 * i;
        sizes[i] = rc_ld32(el + 24 * i + 16);
        datas[i] = rc_ld32(el + 24 * i + 20);
    }
    RC_RETURN(c, put_address(RC_ARG(c, 3), RC_ARG(c, 4), n, guids, sizes, datas), 5);
}

/* EnumAddress(this, callback, address, size, context): callback(guid, size, data, context). */
static void l_EnumAddress(rc_cpu *c)
{
    uint32_t cb = RC_ARG(c, 1), a = RC_ARG(c, 2), size = RC_ARG(c, 3), ctx = RC_ARG(c, 4);
    for (uint32_t at = 0; at + 20 <= size;) {
        uint32_t n = rc_ld32(a + at + 16);
        uint32_t args[4] = {a + at, n, a + at + 20, ctx};
        if (!rc_call_guest(c, cb, 4, args)) {
            break;
        }
        at += 20 + n;
    }
    RC_RETURN(c, DP_OK, 5);
}

static void l_QueryInterface(rc_cpu *c)
{
    lobby_refs++;
    rc_st32(RC_ARG(c, 2), RC_ARG(c, 0));
    RC_RETURN(c, DP_OK, 3);
}
/* DPlayDLL takes a non-zero Release result for an error: the count must be real. */
static void l_AddRef(rc_cpu *c) { RC_RETURN(c, ++lobby_refs, 1); }
static void l_Release(rc_cpu *c) { RC_RETURN(c, lobby_refs ? --lobby_refs : 0, 1); }
static void l_GetConnectionSettings(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 4); }
static void l_SetLobbyMessageEvent(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 4); }
static void l_ReceiveLobbyMessage(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 6); }
static void l_EnumLocalApplications(rc_cpu *c) { RC_RETURN(c, DP_OK, 4); }
static void l_ok3(rc_cpu *c) { RC_RETURN(c, DP_OK, 3); }
static void l_ok5(rc_cpu *c) { RC_RETURN(c, DP_OK, 5); }
static void l_Connect(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 4); }
static void l_ConnectEx(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 5); }
static void l_SetConnectionSettings(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 4); }
static void l_SendLobbyMessage(rc_cpu *c) { RC_RETURN(c, DPERR_NOTLOBBIED, 5); }
static void l_RunApplication(rc_cpu *c) { RC_RETURN(c, DPERR_UNSUPPORTED, 5); }
static void l_WaitForConnectionSettings(rc_cpu *c) { RC_RETURN(c, DP_OK, 2); }

static const rc_import_def defs[] = {
    {LCOM, "IDirectPlayLobby3A::QueryInterface", l_QueryInterface},
    {LCOM, "IDirectPlayLobby3A::AddRef", l_AddRef},
    {LCOM, "IDirectPlayLobby3A::Release", l_Release},
    {LCOM, "IDirectPlayLobby3A::CreateAddress", l_CreateAddress},
    {LCOM, "IDirectPlayLobby3A::CreateCompoundAddress", l_CreateCompoundAddress},
    {LCOM, "IDirectPlayLobby3A::EnumAddress", l_EnumAddress},
    {LCOM, "IDirectPlayLobby3A::GetConnectionSettings", l_GetConnectionSettings},
    {LCOM, "IDirectPlayLobby3A::SetLobbyMessageEvent", l_SetLobbyMessageEvent},
    {LCOM, "IDirectPlayLobby3A::ReceiveLobbyMessage", l_ReceiveLobbyMessage},
    {LCOM, "IDirectPlayLobby3A::EnumLocalApplications", l_EnumLocalApplications},
    {LCOM, "IDirectPlayLobby3A::RegisterApplication", l_ok3},
    {LCOM, "IDirectPlayLobby3A::UnregisterApplication", l_ok3},
    {LCOM, "IDirectPlayLobby3A::EnumAddressTypes", l_ok5},
    {LCOM, "IDirectPlayLobby3A::Connect", l_Connect},
    {LCOM, "IDirectPlayLobby3A::ConnectEx", l_ConnectEx},
    {LCOM, "IDirectPlayLobby3A::SetConnectionSettings", l_SetConnectionSettings},
    {LCOM, "IDirectPlayLobby3A::SendLobbyMessage", l_SendLobbyMessage},
    {LCOM, "IDirectPlayLobby3A::RunApplication", l_RunApplication},
    {LCOM, "IDirectPlayLobby3A::WaitForConnectionSettings", l_WaitForConnectionSettings},
    {COM, "IDirectPlay4A::QueryInterface", m_QueryInterface},
    {COM, "IDirectPlay4A::AddRef", m_AddRef},
    {COM, "IDirectPlay4A::Release", m_Release},
    {COM, "IDirectPlay4A::EnumConnections", m_EnumConnections},
    {COM, "IDirectPlay4A::InitializeConnection", m_InitializeConnection},
    {COM, "IDirectPlay4A::EnumSessions", m_EnumSessions},
    {COM, "IDirectPlay4A::Open", m_Open},
    {COM, "IDirectPlay4A::SecureOpen", m_SecureOpen},
    {COM, "IDirectPlay4A::Close", m_Close},
    {COM, "IDirectPlay4A::CreatePlayer", m_CreatePlayer},
    {COM, "IDirectPlay4A::DestroyPlayer", m_DestroyPlayer},
    {COM, "IDirectPlay4A::Send", m_Send},
    {COM, "IDirectPlay4A::SendEx", m_SendEx},
    {COM, "IDirectPlay4A::Receive", m_Receive},
    {COM, "IDirectPlay4A::GetMessageCount", m_GetMessageCount},
    {COM, "IDirectPlay4A::GetMessageQueue", m_GetMessageQueue},
    {COM, "IDirectPlay4A::EnumPlayers", m_EnumPlayers},
    {COM, "IDirectPlay4A::GetPlayerName", m_GetPlayerName},
    {COM, "IDirectPlay4A::GetSessionDesc", m_GetSessionDesc},
    {COM, "IDirectPlay4A::SetSessionDesc", m_SetSessionDesc},
    {COM, "IDirectPlay4A::GetCaps", m_GetCaps},
    {COM, "IDirectPlay4A::GetPlayerCaps", m_GetPlayerCaps},
    {COM, "IDirectPlay4A::GetPlayerFlags", m_GetPlayerFlags},
    {COM, "IDirectPlay4A::GetPlayerData", m_GetPlayerData},
    {COM, "IDirectPlay4A::SetPlayerData", m_SetPlayerData},
    {COM, "IDirectPlay4A::SetPlayerName", ok4},
    {COM, "IDirectPlay4A::CreateGroup", m_CreateGroup},
    {COM, "IDirectPlay4A::DestroyGroup", ok2},
    {COM, "IDirectPlay4A::AddPlayerToGroup", ok3},
    {COM, "IDirectPlay4A::DeletePlayerFromGroup", ok3},
    {COM, "IDirectPlay4A::EnumGroups", ok5},
    {COM, "IDirectPlay4A::EnumGroupPlayers", ok6},
    {COM, "IDirectPlay4A::StartSession", ok3},
    {COM, "IDirectPlay4A::CancelMessage", ok3},
    {COM, "IDirectPlay4A::CancelPriority", ok4},
    {COM, "IDirectPlay4A::Initialize", ok2},
    {COM, "IDirectPlay4A::GetPlayerAddress", m_GetPlayerAddress},
    {COM, "IDirectPlay4A::SetGroupData", ok5},
    {COM, "IDirectPlay4A::SetGroupName", ok4},
    {COM, "IDirectPlay4A::SendChatMessage", ok5},
    {COM, "IDirectPlay4A::EnumGroupsInGroup", ok6},
};

/* The network is serviced at every message pump too, so a host answers joins and relays
 * messages while its game is busy elsewhere. */
static void pump(rc_cpu *c)
{
    (void)c;
    if (dp.open) {
        poll_net();
    }
}

void rc_dplay_init(void)
{
    rc_add_pump_hook(pump);
    rc_register_imports(defs, sizeof defs / sizeof defs[0]);
    rc_com_register("{D1EB6D20-8923-11D0-9D97-00A0C90A43CB}", create);       /* CLSID_DirectPlay */
    rc_com_register("{2FE8F810-B2A5-11D0-A787-0000F803ABFC}", create_lobby); /* CLSID_DirectPlayLobby */
}
