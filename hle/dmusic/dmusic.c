/* SPDX-License-Identifier: MIT */
/*
 * DirectMusic, as far as the games use it.
 *
 * Cultures 1 only creates the DirectMusic object (IDirectMusic2, DirectX 6.1) to see that
 * DirectX 7 is installed, and releases it again. The object answers QueryInterface, AddRef
 * and Release; any other method stops with its name. The C2 engine's music driver
 * (gedx8musicdrv.dll) needs the loader, performance and segments: milestone M10.
 */
#include "dmusic.h"

#include <stdio.h>

#include "rc/process.h"
#include "rc/vm.h"

#define COM "DMUSIC_COM"
#define S_OK 0u
#define E_NOINTERFACE 0x80004002u

static const char *const methods[] = {
    "QueryInterface", "AddRef", "Release", "EnumPort", "CreateMusicBuffer", "CreatePort",
    "EnumMasterClock", "GetMasterClock", "SetMasterClock", "Activate", "GetDefaultPort",
    "SetDirectSound", "SetExternalMasterClock",
};

static uint32_t refs;

static uint32_t create(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    static uint32_t obj;
    if (!obj) {
        uint32_t n = sizeof methods / sizeof methods[0];
        uint32_t vt = rc_heap_alloc(4 * n, false);
        for (uint32_t i = 0; i < n; i++) {
            char buf[64];
            snprintf(buf, sizeof buf, "IDirectMusic::%s", methods[i]);
            rc_st32(vt + 4 * i, rc_import_address(COM, rc_str(rc_proc_str(buf))));
        }
        obj = rc_heap_alloc(4, false);
        rc_st32(obj, vt);
    }
    refs++;
    return obj;
}

static void m_QueryInterface(rc_cpu *c)
{
    refs++;
    rc_st32(RC_ARG(c, 2), RC_ARG(c, 0));
    RC_RETURN(c, S_OK, 3);
}
static void m_AddRef(rc_cpu *c) { RC_RETURN(c, ++refs, 1); }
static void m_Release(rc_cpu *c) { RC_RETURN(c, refs ? --refs : 0, 1); }

static const rc_import_def defs[] = {
    {COM, "IDirectMusic::QueryInterface", m_QueryInterface},
    {COM, "IDirectMusic::AddRef", m_AddRef},
    {COM, "IDirectMusic::Release", m_Release},
};

void rc_dmusic_init(void)
{
    rc_register_imports(defs, sizeof defs / sizeof defs[0]);
    rc_com_register("{636B9F10-0C7D-11D1-95B2-0020AFDC7421}", create); /* CLSID_DirectMusic */
}
