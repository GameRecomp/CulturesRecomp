/* SPDX-License-Identifier: MIT */
#include "rc/cpu.h"

#include <string.h>

void rc_cpu_init(rc_cpu *c, uint32_t esp) {
    memset(c, 0, sizeof *c);
    c->esp = esp;
    c->eflags = 0x202; /* IF set, reserved bit 1 */
    c->fpu.cw = 0x027f;
    c->fpu.tag = 0xffff;
}
