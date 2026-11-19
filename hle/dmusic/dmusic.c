/* SPDX-License-Identifier: MIT */
/*
 * DirectMusic, as far as the games use it.
 *
 * Cultures 1 only creates the DirectMusic object (IDirectMusic2, DirectX 6.1) to see that
 * DirectX 7 is installed, and releases it again. The C2 engine's music driver
 * (gedx8musicdrv.dll) uses the performance, the loader, segments, audio paths and the
 * composer of DirectX 8.
 *
 * For now the music is silent: the objects exist and answer as DirectMusic does when it
 * plays (a segment loads, plays and has a length), but no sound is made. Playing the
 * segments through a DLS synth is the rest of milestone M10.
 *
 * Objects are as in ddraw.c: a guest block with the vtable and the host object's index;
 * vtables hold reserved import addresses named after the methods ("IDirectMusicSegment8::
 * GetLength"), so a method without an implementation stops with its name.
 */
#include "dmusic.h"

#include <stdio.h>
#include <string.h>

#include "rc/log.h"
#include "rc/process.h"
#include "rc/vm.h"

#define COM "DMUSIC_COM"
#define S_OK 0u
#define S_FALSE 1u
#define E_NOTIMPL 0x80004001u
#define DMUS_E_NOT_FOUND 0x88781193u
#define DMUS_PPQ 768u

enum kind { K_FREE, K_MUSIC, K_PERFORMANCE, K_COMPOSER, K_LOADER, K_SEGMENT, K_STATE, K_PATH, K_KINDS };

static const char *const music_methods[] = {
    "QueryInterface", "AddRef", "Release", "EnumPort", "CreateMusicBuffer", "CreatePort",
    "EnumMasterClock", "GetMasterClock", "SetMasterClock", "Activate", "GetDefaultPort",
    "SetDirectSound", "SetExternalMasterClock",
};
static const char *const performance_methods[] = {
    "QueryInterface", "AddRef", "Release", "Init", "PlaySegment", "Stop", "GetSegmentState",
    "SetPrepareTime", "GetPrepareTime", "SetBumperLength", "GetBumperLength", "SendPMsg",
    "MusicToReferenceTime", "ReferenceToMusicTime", "IsPlaying", "GetTime", "AllocPMsg", "FreePMsg",
    "GetGraph", "SetGraph", "SetNotificationHandle", "GetNotificationPMsg", "AddNotificationType",
    "RemoveNotificationType", "AddPort", "RemovePort", "AssignPChannelBlock", "AssignPChannel",
    "PChannelInfo", "DownloadInstrument", "Invalidate", "GetParam", "SetParam", "GetGlobalParam",
    "SetGlobalParam", "GetLatencyTime", "GetQueueTime", "AdjustTime", "CloseDown", "GetResolvedTime",
    "MIDIToMusic", "MusicToMIDI", "TimeToRhythm", "RhythmToTime", "InitAudio", "PlaySegmentEx",
    "StopEx", "ClonePMsg", "CreateAudioPath", "CreateStandardAudioPath", "SetDefaultAudioPath",
    "GetDefaultAudioPath", "GetParamEx", "SetParamEx",
};
static const char *const composer_methods[] = {
    "QueryInterface", "AddRef", "Release", "ComposeSegmentFromTemplate", "ComposeSegmentFromShape",
    "ComposeTransition", "AutoTransition", "ComposeTemplateFromShape", "ChangeChordMap",
};
static const char *const loader_methods[] = {
    "QueryInterface", "AddRef", "Release", "GetObject", "SetObject", "SetSearchDirectory",
    "ScanDirectory", "CacheObject", "ReleaseObject", "ClearCache", "EnableCache", "EnumObject",
    "CollectGarbage", "ReleaseObjectByUnknown", "LoadObjectFromFile",
};
static const char *const segment_methods[] = {
    "QueryInterface", "AddRef", "Release", "GetLength", "SetLength", "GetRepeats", "SetRepeats",
    "GetDefaultResolution", "SetDefaultResolution", "GetTrack", "GetTrackGroup", "InsertTrack",
    "RemoveTrack", "InitPlay", "GetGraph", "SetGraph", "AddNotificationType", "RemoveNotificationType",
    "GetParam", "SetParam", "Clone", "SetStartPoint", "GetStartPoint", "SetLoopPoints", "GetLoopPoints",
    "SetPChannelsUsed", "SetTrackConfig", "GetAudioPathConfig", "Compose", "Download", "Unload",
};
static const char *const state_methods[] = {
    "QueryInterface", "AddRef", "Release", "GetRepeats", "GetSegment", "GetStartTime", "GetSeek",
    "GetStartPoint", "SetTrackConfig", "GetObjectInPath",
};
static const char *const path_methods[] = {
    "QueryInterface", "AddRef", "Release", "GetObjectInPath", "Activate", "SetVolume", "ConvertPChannel",
};

static const struct {
    const char *name;
    const char *const *methods;
    uint32_t count;
} kinds[K_KINDS] = {
    [K_MUSIC] = {"IDirectMusic", music_methods, sizeof music_methods / sizeof *music_methods},
    [K_PERFORMANCE] = {"IDirectMusicPerformance8", performance_methods,
                       sizeof performance_methods / sizeof *performance_methods},
    [K_COMPOSER] = {"IDirectMusicComposer8", composer_methods, sizeof composer_methods / sizeof *composer_methods},
    [K_LOADER] = {"IDirectMusicLoader8", loader_methods, sizeof loader_methods / sizeof *loader_methods},
    [K_SEGMENT] = {"IDirectMusicSegment8", segment_methods, sizeof segment_methods / sizeof *segment_methods},
    [K_STATE] = {"IDirectMusicSegmentState8", state_methods, sizeof state_methods / sizeof *state_methods},
    [K_PATH] = {"IDirectMusicAudioPath8", path_methods, sizeof path_methods / sizeof *path_methods},
};

typedef struct object {
    enum kind kind;
    uint32_t refs;
    uint32_t guest;
    /* A segment: its length and repeats (music time); a state: its segment and start. */
    uint32_t length, repeats, segment;
    uint64_t start;
} object;

#define OBJECTS 512
static object objects[OBJECTS];
static uint32_t vtables[K_KINDS];

static uint32_t vtable(enum kind k)
{
    if (!vtables[k]) {
        uint32_t t = rc_heap_alloc(4 * kinds[k].count, false);
        for (uint32_t i = 0; i < kinds[k].count; i++) {
            char buf[96];
            snprintf(buf, sizeof buf, "%s::%s", kinds[k].name, kinds[k].methods[i]);
            rc_st32(t + 4 * i, rc_import_address(COM, rc_str(rc_proc_str(buf))));
        }
        vtables[k] = t;
    }
    return vtables[k];
}

static object *new_object(enum kind k)
{
    for (uint32_t i = 1; i < OBJECTS; i++) {
        if (objects[i].kind == K_FREE) {
            object *o = &objects[i];
            memset(o, 0, sizeof *o);
            o->kind = k;
            o->refs = 1;
            o->guest = rc_heap_alloc(8, false);
            rc_st32(o->guest, vtable(k));
            rc_st32(o->guest + 4, i);
            return o;
        }
    }
    rc_fatal("DirectMusic: out of objects");
}

static object *self(rc_cpu *c)
{
    uint32_t g = RC_ARG(c, 0);
    uint32_t i = g ? rc_ld32(g + 4) : 0;
    if (!i || i >= OBJECTS || objects[i].kind == K_FREE) {
        rc_fatal("DirectMusic: bad this pointer %#x", g);
    }
    return &objects[i];
}

static object *object_at(uint32_t g)
{
    uint32_t i = g ? rc_ld32(g + 4) : 0;
    return i && i < OBJECTS && objects[i].kind != K_FREE ? &objects[i] : NULL;
}

/* Music time: DMUS_PPQ ticks a quarter note at 120 beats a minute, from the clock. */
static uint32_t music_now(void) { return (uint32_t)(rc_ticks_ms() * DMUS_PPQ * 2 / 1000); }

static void put(uint32_t p, uint32_t v)
{
    if (p) {
        rc_st32(p, v);
    }
}

/* ---- every interface ---- */

static void m_QueryInterface(rc_cpu *c)
{
    object *o = self(c);
    o->refs++;
    put(RC_ARG(c, 2), o->guest);
    RC_RETURN(c, S_OK, 3);
}
static void m_AddRef(rc_cpu *c) { RC_RETURN(c, ++self(c)->refs, 1); }
/* Objects stay (the guest may hold pointers it has released); the count is what it reports. */
static void m_Release(rc_cpu *c)
{
    object *o = self(c);
    RC_RETURN(c, o->refs ? --o->refs : 0, 1);
}
static void ok1(rc_cpu *c) { RC_RETURN(c, S_OK, 1); }
static void ok2(rc_cpu *c) { RC_RETURN(c, S_OK, 2); }
static void ok3(rc_cpu *c) { RC_RETURN(c, S_OK, 3); }
static void ok4(rc_cpu *c) { RC_RETURN(c, S_OK, 4); }
static void ok5(rc_cpu *c) { RC_RETURN(c, S_OK, 5); }
static void ok6(rc_cpu *c) { RC_RETURN(c, S_OK, 6); }

/* ---- the performance ---- */

/* InitAudio(this, music*, dsound*, hwnd, default path type, pchannels, flags, params) */
static void p_InitAudio(rc_cpu *c)
{
    if (RC_ARG(c, 1)) {
        object *m = new_object(K_MUSIC);
        rc_st32(RC_ARG(c, 1), m->guest);
    }
    if (RC_ARG(c, 2)) {
        rc_st32(RC_ARG(c, 2), 0); /* no DirectSound object to hand out */
    }
    RC_RETURN(c, S_OK, 8);
}

/* PlaySegmentEx(this, source, name, transition, flags, start (64-bit), state*, from, path) */
static void p_PlaySegmentEx(rc_cpu *c)
{
    object *seg = object_at(RC_ARG(c, 1));
    uint32_t out = RC_ARG(c, 7);
    if (out) {
        object *st = new_object(K_STATE);
        st->segment = seg ? RC_ARG(c, 1) : 0;
        st->start = music_now();
        rc_st32(out, st->guest);
    }
    RC_RETURN(c, S_OK, 10);
}

/* PlaySegment(this, segment, flags, start (64-bit), state*) */
static void p_PlaySegment(rc_cpu *c)
{
    uint32_t out = RC_ARG(c, 5);
    if (out) {
        object *st = new_object(K_STATE);
        st->segment = RC_ARG(c, 1);
        st->start = music_now();
        rc_st32(out, st->guest);
    }
    RC_RETURN(c, S_OK, 6);
}

/* IsPlaying(this, segment, state): nothing sounds, so nothing is playing. */
static void p_IsPlaying(rc_cpu *c) { RC_RETURN(c, S_FALSE, 3); }

/* GetTime(this, reference time*, music time*) */
static void p_GetTime(rc_cpu *c)
{
    uint64_t ref = rc_ticks_ms() * 10000u;
    if (RC_ARG(c, 1)) {
        rc_st32(RC_ARG(c, 1), (uint32_t)ref);
        rc_st32(RC_ARG(c, 1) + 4, (uint32_t)(ref >> 32));
    }
    put(RC_ARG(c, 2), music_now());
    RC_RETURN(c, S_OK, 3);
}

/* CreateStandardAudioPath(this, type, pchannels, activate, path*) */
static void p_CreateStandardAudioPath(rc_cpu *c)
{
    put(RC_ARG(c, 4), new_object(K_PATH)->guest);
    RC_RETURN(c, S_OK, 5);
}

/* CreateAudioPath(this, config, activate, path*) */
static void p_CreateAudioPath(rc_cpu *c)
{
    put(RC_ARG(c, 3), new_object(K_PATH)->guest);
    RC_RETURN(c, S_OK, 4);
}

/* GetDefaultAudioPath(this, path*) */
static void p_GetDefaultAudioPath(rc_cpu *c)
{
    static object *def;
    if (!def) {
        def = new_object(K_PATH);
    }
    def->refs++;
    put(RC_ARG(c, 1), def->guest);
    RC_RETURN(c, S_OK, 2);
}

/* GetSegmentState(this, state*, time) */
static void p_GetSegmentState(rc_cpu *c)
{
    put(RC_ARG(c, 1), 0);
    RC_RETURN(c, DMUS_E_NOT_FOUND, 3);
}

/* GetParam/GetGlobalParam: nothing is known. */
static void p_GetParam(rc_cpu *c) { RC_RETURN(c, DMUS_E_NOT_FOUND, 7); }
static void p_GetGlobalParam(rc_cpu *c) { RC_RETURN(c, DMUS_E_NOT_FOUND, 4); }

/* ---- the loader ---- */

/* GetObject(this, DMUS_OBJECTDESC*, iid, object*) and LoadObjectFromFile(this, class, iid,
 * file, object*): a silent segment of four bars. */
static void l_GetObject(rc_cpu *c)
{
    object *s = new_object(K_SEGMENT);
    s->length = 16 * DMUS_PPQ;
    put(RC_ARG(c, 3), s->guest);
    RC_RETURN(c, S_OK, 4);
}

static void l_LoadObjectFromFile(rc_cpu *c)
{
    object *s = new_object(K_SEGMENT);
    s->length = 16 * DMUS_PPQ;
    put(RC_ARG(c, 4), s->guest);
    RC_RETURN(c, S_OK, 5);
}

/* ---- segments and their states ---- */

static void s_GetLength(rc_cpu *c)
{
    put(RC_ARG(c, 1), self(c)->length);
    RC_RETURN(c, S_OK, 2);
}
static void s_SetLength(rc_cpu *c)
{
    self(c)->length = RC_ARG(c, 1);
    RC_RETURN(c, S_OK, 2);
}
static void s_GetRepeats(rc_cpu *c)
{
    put(RC_ARG(c, 1), self(c)->repeats);
    RC_RETURN(c, S_OK, 2);
}
static void s_SetRepeats(rc_cpu *c)
{
    self(c)->repeats = RC_ARG(c, 1);
    RC_RETURN(c, S_OK, 2);
}
/* Clone(this, start, end, segment*) */
static void s_Clone(rc_cpu *c)
{
    object *o = self(c);
    object *n = new_object(K_SEGMENT);
    n->length = o->length;
    n->repeats = o->repeats;
    put(RC_ARG(c, 3), n->guest);
    RC_RETURN(c, S_OK, 4);
}
/* GetParam(this, guid, group, index, time, next*, data): nothing in a silent segment. */
static void s_GetParam(rc_cpu *c) { RC_RETURN(c, DMUS_E_NOT_FOUND, 7); }

/* GetSegment(this, segment*) */
static void st_GetSegment(rc_cpu *c)
{
    object *o = self(c);
    put(RC_ARG(c, 1), o->segment);
    RC_RETURN(c, o->segment ? S_OK : DMUS_E_NOT_FOUND, 2);
}
static void st_GetStartTime(rc_cpu *c)
{
    put(RC_ARG(c, 1), (uint32_t)self(c)->start);
    RC_RETURN(c, S_OK, 2);
}

/* GetAudioPathConfig(this, config*): segments carry none here; the standard path plays them. */
static void s_GetAudioPathConfig(rc_cpu *c)
{
    put(RC_ARG(c, 1), 0);
    RC_RETURN(c, 0x88781213u /* DMUS_E_NO_AUDIOPATH_CONFIG */, 2);
}

/* GetObjectInPath(this, pchannel, stage, buffer, guid, index, iid, object*): nothing inside. */
static void path_GetObjectInPath(rc_cpu *c)
{
    put(RC_ARG(c, 7), 0);
    RC_RETURN(c, DMUS_E_NOT_FOUND, 8);
}

/* A value out of a call that has nothing to say: 0 into the pointer at argument 1. */
static void zero2(rc_cpu *c)
{
    put(RC_ARG(c, 1), 0);
    RC_RETURN(c, S_OK, 2);
}
static void zero3(rc_cpu *c)
{
    put(RC_ARG(c, 1), 0);
    put(RC_ARG(c, 2), 0);
    RC_RETURN(c, S_OK, 3);
}

/* Time conversions at 120 beats a minute: music ticks <-> 100 ns reference time. */
static void p_MusicToReferenceTime(rc_cpu *c) /* (this, mt, rt*) */
{
    uint64_t rt = (uint64_t)RC_ARG(c, 1) * 10000000u / (DMUS_PPQ * 2);
    put(RC_ARG(c, 2), (uint32_t)rt);
    put(RC_ARG(c, 2) ? RC_ARG(c, 2) + 4 : 0, (uint32_t)(rt >> 32));
    RC_RETURN(c, S_OK, 3);
}
static void p_ReferenceToMusicTime(rc_cpu *c) /* (this, rt (64), mt*) */
{
    uint64_t rt = (uint64_t)RC_ARG(c, 1) | (uint64_t)RC_ARG(c, 2) << 32;
    put(RC_ARG(c, 3), (uint32_t)(rt * (DMUS_PPQ * 2) / 10000000u));
    RC_RETURN(c, S_OK, 4);
}
static void p_GetResolvedTime(rc_cpu *c) /* (this, rt (64), resolved*, flags) */
{
    put(RC_ARG(c, 3), RC_ARG(c, 1));
    put(RC_ARG(c, 3) ? RC_ARG(c, 3) + 4 : 0, RC_ARG(c, 2));
    RC_RETURN(c, S_OK, 5);
}
static void p_GetLatencyTime(rc_cpu *c) /* (this, rt*) */
{
    uint64_t ref = rc_ticks_ms() * 10000u;
    put(RC_ARG(c, 1), (uint32_t)ref);
    put(RC_ARG(c, 1) ? RC_ARG(c, 1) + 4 : 0, (uint32_t)(ref >> 32));
    RC_RETURN(c, S_OK, 2);
}

/* ---- the composer ---- */

/* ComposeTransition(this, from, to, measure, command, flags, chordmap, transition*) and
 * AutoTransition(this, performance, to, command, flags, chordmap, transition*, to state*,
 * transition state*): no transition, the next segment plays. */
static void c_ComposeTransition(rc_cpu *c)
{
    put(RC_ARG(c, 7), 0);
    RC_RETURN(c, S_OK, 8);
}
static void c_AutoTransition(rc_cpu *c)
{
    put(RC_ARG(c, 6), 0);
    put(RC_ARG(c, 7), 0);
    put(RC_ARG(c, 8), 0);
    RC_RETURN(c, S_OK, 9);
}

/* ---- creation ---- */

static uint32_t create_music(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    return new_object(K_MUSIC)->guest;
}
static uint32_t create_performance(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    return new_object(K_PERFORMANCE)->guest;
}
static uint32_t create_composer(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    return new_object(K_COMPOSER)->guest;
}
static uint32_t create_loader(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    return new_object(K_LOADER)->guest;
}
static uint32_t create_segment(rc_cpu *c, uint32_t iid)
{
    (void)c;
    (void)iid;
    object *s = new_object(K_SEGMENT);
    s->length = 16 * DMUS_PPQ;
    return s->guest;
}

#define EACH(m, fn)                                                                                \
    {COM, "IDirectMusic::" m, fn}, {COM, "IDirectMusicPerformance8::" m, fn},                     \
        {COM, "IDirectMusicComposer8::" m, fn}, {COM, "IDirectMusicLoader8::" m, fn},             \
        {COM, "IDirectMusicSegment8::" m, fn}, {COM, "IDirectMusicSegmentState8::" m, fn},        \
        {COM, "IDirectMusicAudioPath8::" m, fn}

static const rc_import_def defs[] = {
    EACH("QueryInterface", m_QueryInterface),
    EACH("AddRef", m_AddRef),
    EACH("Release", m_Release),
    {COM, "IDirectMusic::Activate", ok2},
    {COM, "IDirectMusic::SetDirectSound", ok3},
    /* performance */
    {COM, "IDirectMusicPerformance8::Init", ok4},
    {COM, "IDirectMusicPerformance8::InitAudio", p_InitAudio},
    {COM, "IDirectMusicPerformance8::PlaySegment", p_PlaySegment},
    {COM, "IDirectMusicPerformance8::PlaySegmentEx", p_PlaySegmentEx},
    {COM, "IDirectMusicPerformance8::Stop", ok5},
    {COM, "IDirectMusicPerformance8::StopEx", ok5}, /* this, object, time (64-bit), flags */
    {COM, "IDirectMusicPerformance8::IsPlaying", p_IsPlaying},
    {COM, "IDirectMusicPerformance8::GetTime", p_GetTime},
    {COM, "IDirectMusicPerformance8::GetSegmentState", p_GetSegmentState},
    {COM, "IDirectMusicPerformance8::SetPrepareTime", ok2},
    {COM, "IDirectMusicPerformance8::SetBumperLength", ok2},
    {COM, "IDirectMusicPerformance8::SetNotificationHandle", ok4}, /* this, event, minimum (64-bit) */
    {COM, "IDirectMusicPerformance8::AddNotificationType", ok2},
    {COM, "IDirectMusicPerformance8::RemoveNotificationType", ok2},
    {COM, "IDirectMusicPerformance8::GetParam", p_GetParam},
    {COM, "IDirectMusicPerformance8::SetParam", ok6},
    {COM, "IDirectMusicPerformance8::GetGlobalParam", p_GetGlobalParam},
    {COM, "IDirectMusicPerformance8::SetGlobalParam", ok4},
    {COM, "IDirectMusicPerformance8::Invalidate", ok3},
    {COM, "IDirectMusicPerformance8::CloseDown", ok1},
    {COM, "IDirectMusicPerformance8::CreateAudioPath", p_CreateAudioPath},
    {COM, "IDirectMusicPerformance8::CreateStandardAudioPath", p_CreateStandardAudioPath},
    {COM, "IDirectMusicPerformance8::SetDefaultAudioPath", ok2},
    {COM, "IDirectMusicPerformance8::GetDefaultAudioPath", p_GetDefaultAudioPath},
    /* loader */
    {COM, "IDirectMusicLoader8::GetObject", l_GetObject},
    {COM, "IDirectMusicLoader8::LoadObjectFromFile", l_LoadObjectFromFile},
    {COM, "IDirectMusicLoader8::SetObject", ok2},
    {COM, "IDirectMusicLoader8::SetSearchDirectory", ok4},
    {COM, "IDirectMusicLoader8::ScanDirectory", ok4},
    {COM, "IDirectMusicLoader8::CacheObject", ok2},
    {COM, "IDirectMusicLoader8::ReleaseObject", ok2},
    {COM, "IDirectMusicLoader8::ReleaseObjectByUnknown", ok2},
    {COM, "IDirectMusicLoader8::ClearCache", ok2},
    {COM, "IDirectMusicLoader8::EnableCache", ok3},
    {COM, "IDirectMusicLoader8::CollectGarbage", ok1},
    /* segments, their states, audio paths */
    {COM, "IDirectMusicSegment8::GetLength", s_GetLength},
    {COM, "IDirectMusicSegment8::SetLength", s_SetLength},
    {COM, "IDirectMusicSegment8::GetRepeats", s_GetRepeats},
    {COM, "IDirectMusicSegment8::SetRepeats", s_SetRepeats},
    {COM, "IDirectMusicSegment8::Clone", s_Clone},
    {COM, "IDirectMusicSegment8::GetParam", s_GetParam},
    {COM, "IDirectMusicSegment8::SetParam", ok6},
    {COM, "IDirectMusicSegment8::SetStartPoint", ok2},
    {COM, "IDirectMusicSegment8::SetLoopPoints", ok3},
    {COM, "IDirectMusicSegment8::Download", ok2},
    {COM, "IDirectMusicSegment8::Unload", ok2},
    {COM, "IDirectMusicSegment8::AddNotificationType", ok2},
    {COM, "IDirectMusicSegment8::RemoveNotificationType", ok2},
    {COM, "IDirectMusicSegmentState8::GetSegment", st_GetSegment},
    {COM, "IDirectMusicSegmentState8::GetStartTime", st_GetStartTime},
    {COM, "IDirectMusicAudioPath8::Activate", ok2},
    {COM, "IDirectMusicAudioPath8::GetObjectInPath", path_GetObjectInPath},
    {COM, "IDirectMusicSegmentState8::GetObjectInPath", path_GetObjectInPath},
    {COM, "IDirectMusicSegment8::GetAudioPathConfig", s_GetAudioPathConfig},
    {COM, "IDirectMusicSegment8::GetStartPoint", zero2},
    {COM, "IDirectMusicSegment8::GetLoopPoints", zero3},
    {COM, "IDirectMusicSegment8::SetPChannelsUsed", ok3},
    {COM, "IDirectMusicSegment8::GetDefaultResolution", zero2},
    {COM, "IDirectMusicSegment8::SetDefaultResolution", ok2},
    {COM, "IDirectMusicSegmentState8::GetSeek", zero2},
    {COM, "IDirectMusicSegmentState8::GetRepeats", zero2},
    {COM, "IDirectMusicSegmentState8::GetStartPoint", zero2},
    {COM, "IDirectMusicPerformance8::MusicToReferenceTime", p_MusicToReferenceTime},
    {COM, "IDirectMusicPerformance8::ReferenceToMusicTime", p_ReferenceToMusicTime},
    {COM, "IDirectMusicPerformance8::GetResolvedTime", p_GetResolvedTime},
    {COM, "IDirectMusicPerformance8::GetLatencyTime", p_GetLatencyTime},
    {COM, "IDirectMusicPerformance8::GetQueueTime", p_GetLatencyTime},
    {COM, "IDirectMusicPerformance8::GetPrepareTime", zero2},
    {COM, "IDirectMusicPerformance8::GetBumperLength", zero2},
    {COM, "IDirectMusicAudioPath8::SetVolume", ok3},
    /* composer */
    {COM, "IDirectMusicComposer8::ComposeTransition", c_ComposeTransition},
    {COM, "IDirectMusicComposer8::AutoTransition", c_AutoTransition},
};

void rc_dmusic_init(void)
{
    rc_register_imports(defs, sizeof defs / sizeof defs[0]);
    rc_com_register("{636B9F10-0C7D-11D1-95B2-0020AFDC7421}", create_music);       /* DirectMusic */
    rc_com_register("{D2AC2881-B39B-11D1-8704-00600893B1BD}", create_performance); /* Performance */
    rc_com_register("{D2AC2890-B39B-11D1-8704-00600893B1BD}", create_composer);    /* Composer */
    rc_com_register("{D2AC2892-B39B-11D1-8704-00600893B1BD}", create_loader);      /* Loader */
    rc_com_register("{D2AC2882-B39B-11D1-8704-00600893B1BD}", create_segment);     /* Segment */
}
