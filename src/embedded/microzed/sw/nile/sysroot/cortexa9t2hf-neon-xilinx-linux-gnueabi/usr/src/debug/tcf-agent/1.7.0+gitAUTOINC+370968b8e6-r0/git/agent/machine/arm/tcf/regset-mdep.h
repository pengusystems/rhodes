/*******************************************************************************
 * Copyright (c) 2013-2018 Stanislav Yakovlev and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 * The Eclipse Public License is available at
 * http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 * You may elect to redistribute this code under either of these licenses.
 *
 * Contributors:
 *     Stanislav Yakovlev - initial API and implementation
 * Emmanuel Touron (Wind River) - initial HW Breakpoint support
 *******************************************************************************/

/* offset to be applied to the PC after a software trap */
#define TRAP_OFFSET 0

#if defined(__linux__)

#include <elf.h>

#define MDEP_UseREGSET

#ifndef NT_ARM_VFP
#define NT_ARM_VFP 0x400
#endif

#ifndef PTRACE_GETREGSET
#  define PTRACE_GETREGSET 0x4204
#endif

#ifndef PTRACE_SETREGSET
#  define PTRACE_SETREGSET 0x4205
#endif

struct regset_gp {
    uint32_t regs[16];
    uint32_t cpsr;
    uint32_t orig_r0;
};

struct regset_fp {
    uint64_t fpregs[32];
    uint32_t fpscr;
};

#define REGSET_GP NT_PRSTATUS
#define REGSET_FP NT_ARM_VFP

#endif
