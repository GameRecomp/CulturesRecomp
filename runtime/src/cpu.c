/* SPDX-License-Identifier: MIT */
#include "rc/cpu.h"

#include <string.h>

void rc_cpu_init(rc_cpu *c, uint32_t esp) {
    memset(c, 0, sizeof *c);
    c->esp = esp;
    c->fpu_control = 0x027f;
    c->fpu_tag = 0xffff;
}
