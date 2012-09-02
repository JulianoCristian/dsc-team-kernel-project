/* arch/arm/mach-msm/include/mach/memory.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

/* physical offset of RAM */
#if defined(CONFIG_PHYS_OFFSET)
#define PHYS_OFFSET UL(CONFIG_PHYS_OFFSET)
#define RESET_VECTOR UL(0x00000000)
#endif

#define MAX_PHYSMEM_BITS 32
#define SECTION_SIZE_BITS 28

/* Certain configurations of MSM7x30 have multiple memory banks.
*  One or more of these banks can contain holes in the memory map as well.
*  These macros define appropriate conversion routines between the physical
*  and virtual address domains for supporting these configurations using
*  SPARSEMEM and a 3G/1G VM split.
*/

#if defined(CONFIG_ARCH_MSM7X30)

#define EBI0_PHYS_OFFSET PHYS_OFFSET
#define EBI0_PAGE_OFFSET PAGE_OFFSET
#define EBI0_SIZE 0x10000000

#define EBI1_PHYS_OFFSET 0x40000000
#define EBI1_PAGE_OFFSET (EBI0_PAGE_OFFSET + EBI0_SIZE)

#if (defined(CONFIG_SPARSEMEM) && defined(CONFIG_VMSPLIT_3G))

#define __phys_to_virt(phys)				\
	((phys) >= EBI1_PHYS_OFFSET ?			\
	(phys) - EBI1_PHYS_OFFSET + EBI1_PAGE_OFFSET :	\
	(phys) - EBI0_PHYS_OFFSET + EBI0_PAGE_OFFSET)

#define __virt_to_phys(virt)				\
	((virt) >= EBI1_PAGE_OFFSET ?			\
	(virt) - EBI1_PAGE_OFFSET + EBI1_PHYS_OFFSET :	\
	(virt) - EBI0_PAGE_OFFSET + EBI0_PHYS_OFFSET)

#endif

#endif

#define HAS_ARCH_IO_REMAP_PFN_RANGE

#define CONSISTENT_DMA_SIZE (14*SZ_1M)

#ifndef __ASSEMBLY__
void *alloc_bootmem_aligned(unsigned long size, unsigned long alignment);
unsigned long allocate_contiguous_ebi_nomap(unsigned long, unsigned long);
void clean_and_invalidate_caches(unsigned long, unsigned long, unsigned long);
void clean_caches(unsigned long, unsigned long, unsigned long);
void invalidate_caches(unsigned long, unsigned long, unsigned long);
int platform_physical_remove_pages(unsigned long, unsigned long);
int platform_physical_active_pages(unsigned long, unsigned long);
int platform_physical_low_power_pages(unsigned long, unsigned long);

#ifdef CONFIG_ARCH_MSM_ARM11
void write_to_strongly_ordered_memory(void);
void map_page_strongly_ordered(void);


#include <asm/mach-types.h>

#if defined(CONFIG_ARCH_MSM7227)
#define arch_barrier_extra() do \
	{ \
		write_to_strongly_ordered_memory(); \
	} while (0)
#else
#define arch_barrier_extra() do {} while (0)
#endif

#ifdef CONFIG_CACHE_L2X0
extern void l2x0_cache_sync(void);
extern void l2x0_cache_flush_all(void);
#define finish_arch_switch(prev)     do { l2x0_cache_sync(); } while (0)
#endif

#endif
#endif

#if defined CONFIG_ARCH_MSM_SCORPION || defined CONFIG_ARCH_MSM_SCORPIONMP
#define arch_has_speculative_dfetch()	1
#else
#define arch_has_speculative_dfetch()  0
#endif

#endif

/* these correspond to values known by the modem */
#define MEMORY_DEEP_POWERDOWN	0
#define MEMORY_SELF_REFRESH	1
#define MEMORY_ACTIVE		2

#define NPA_MEMORY_NODE_NAME	"/mem/apps/ddr_dpd"

