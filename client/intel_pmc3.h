#ifndef _INTEL_PMC3_H_
#define _INTEL_PMC3_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <json/json.h>
#include "cpuid.h"

/*! 
 \file intel_pmc3.h
 \author Todd Evans 
 \brief Counters for Intel Performance Monitoring Version 3

  \par Location of cpu info and monitoring register files:

  ex) Display cpuid and msr file for cpu 0:

      $ ls -l /dev/cpu/0
      total 0
      crw-------  1 root root 203, 0 Oct 28 18:47 cpuid
      crw-------  1 root root 202, 0 Oct 28 18:47 msr


  \par MSR address layout of registers:

  IA32_PMCx (CTRx) MSRs start at address 0C1H and occupy a contiguous block of MSR
  address space; the number of MSRs per logical processor is reported using
  CPUID.0AH:EAX[15:8].  

  IA32_PERFEVTSELx (CTLx) MSRs start at address 186H and occupy a contiguous block
  of MSR address space. Each performance event select register is paired with a
  corresponding performance counter in the 0C1H address block.
*/

#define IA32_CTR0 0xC1 /* CPUID.0AH: EAX[15:8] > 0 */
#define IA32_CTR1 0xC2 /* CPUID.0AH: EAX[15:8] > 1 */
#define IA32_CTR2 0xC3 /* CPUID.0AH: EAX[15:8] > 2 */
#define IA32_CTR3 0xC4 /* CPUID.0AH: EAX[15:8] > 3 */
#define IA32_CTR4 0xC5 /* CPUID.0AH: EAX[15:8] > 4 */
#define IA32_CTR5 0xC6 /* CPUID.0AH: EAX[15:8] > 5 */
#define IA32_CTR6 0xC7 /* CPUID.0AH: EAX[15:8] > 6 */
#define IA32_CTR7 0xC8 /* CPUID.0AH: EAX[15:8] > 7 */

#define IA32_CTL0 0x186 /* CPUID.0AH: EAX[15:8] > 0 */
#define IA32_CTL1 0x187 /* CPUID.0AH: EAX[15:8] > 1 */
#define IA32_CTL2 0x188 /* CPUID.0AH: EAX[15:8] > 2 */
#define IA32_CTL3 0x189 /* CPUID.0AH: EAX[15:8] > 3 */
#define IA32_CTL4 0x18A /* CPUID.0AH: EAX[15:8] > 4 */
#define IA32_CTL5 0x18B /* CPUID.0AH: EAX[15:8] > 5 */
#define IA32_CTL6 0x18C /* CPUID.0AH: EAX[15:8] > 6 */
#define IA32_CTL7 0x18D /* CPUID.0AH: EAX[15:8] > 7 */

/*! \name Fixed Counter Registers

  These counters always count the same events.
  @{
*/
#define IA32_FIXED_CTR_CTRL 0x38D //!< Fixed Counter Control Register
#define IA32_FIXED_CTR0     0x309 //!< Fixed Counter 0: Instructions Retired
#define IA32_FIXED_CTR1     0x30A //!< Fixed Counter 1: Core Clock Cycles
#define IA32_FIXED_CTR2     0x30B //!< Fixed Counter 2: Reference Clock Cycles
//@}

/*! \name Global Control Registers
  
  Controls for all registers.
  @{
*/
#define IA32_PERF_GLOBAL_STATUS   0x38E //!< indicates overflow 
#define IA32_PERF_GLOBAL_CTRL     0x38F //!< enables all fixed and configurable counters
#define IA32_PERF_GLOBAL_OVF_CTRL 0x390 //!< clears overflow indicators in GLOBAL_STATUS.
//@}

/* Schema Keys 
   All counter registers are 48 bits wide. 
*/

#define KEYS \
    X(CTL0, "C", ""), \
    X(CTL1, "C", ""), \
    X(CTL2, "C", ""), \
    X(CTL3, "C", ""), \
    X(CTL4, "C", ""), \
    X(CTL5, "C", ""), \
    X(CTL6, "C", ""), \
    X(CTL7, "C", ""),	   \
    X(CTR0, "E,W=48", ""), \
    X(CTR1, "E,W=48", ""), \
    X(CTR2, "E,W=48", ""), \
    X(CTR3, "E,W=48", ""), \
    X(CTR4, "E,W=48", ""), \
    X(CTR5, "E,W=48", ""), \
    X(CTR6, "E,W=48", ""), \
    X(CTR7, "E,W=48", ""),	 \
    X(FIXED_CTR0, "E,W=48", ""), \
    X(FIXED_CTR1, "E,W=48", ""), \
    X(FIXED_CTR2, "E,W=48", "")

#define HT_KEYS \
    X(CTL0, "C", ""), \
    X(CTL1, "C", ""), \
    X(CTL2, "C", ""), \
    X(CTL3, "C", ""), \
    X(CTR0, "C", ""), \
    X(CTR1, "C", ""), \
    X(CTR2, "C", ""), \
    X(CTR3, "C", ""), \
    X(FIXED_CTR0, "E,W=48", ""), \
    X(FIXED_CTR1, "E,W=48", ""), \
    X(FIXED_CTR2, "E,W=48", "")

#define KNL_KEYS		 \
  X(CTL0, "C", ""),		 \
    X(CTL1, "C", ""),		 \
    X(CTR0, "E,W=40", ""),	 \
    X(CTR1, "E,W=40", ""),	 \
    X(FIXED_CTR0, "E,W=40", ""), \
    X(FIXED_CTR1, "E,W=40", ""), \
    X(FIXED_CTR2, "E,W=40", "")

/*! \brief Event select */
// IA32_PERFEVTSELx MSR layout
//   [0, 7] Event Select
//   [8, 15] Unit Mask (UMASK)
//   16 USR
//   17 OS
//   18 E Edge_detect
//   19 PC Pin control
//   20 INT APIC interrupt enable
//   21 ANY Any thread (version 3)
//   22 EN Enable counters
//   23 INV Invert counter mask
//   [24, 31] Counter Mask (CMASK)
//   [32, 63] Reserved
#define PERF_EVENT(event, umask) \
  ( (event)			 \
    | (umask << 8)		 \
    | (1ULL << 16)		 \
    | (1ULL << 17)		 \
    | (0ULL << 21)		 \
    | (1ULL << 22)		 \
    )

/* NHM WTM */
#define MEM_UNCORE_RETIRED_REMOTE_DRAM PERF_EVENT(0x0F, 0x10) /* CHECKME */
#define MEM_UNCORE_RETIRED_LOCAL_DRAM  PERF_EVENT(0x0F, 0x20) /* CHECKME */
#define FP_COMP_OPS_EXE_X87            PERF_EVENT(0x10, 0x01)
#define MEM_LOAD_RETIRED_L1D_HIT       PERF_EVENT(0xCB, 0x01)
#define MEM_LOAD_RETIRED_L2_HIT        PERF_EVENT(0xCB, 0x02)
#define MEM_LOAD_RETIRED_L3_HIT        PERF_EVENT(0xCB, 0x0C)
#define MEM_LOAD_RETIRED_L3_MISS       PERF_EVENT(0xCB, 0x10) /* May be same as 0x0F/0x10 + 0x0F/0x20. */

/* SNB IVB */
#define DTLB_LOAD_MISSES_WALK_CYCLES   PERF_EVENT(0x08, 0x04)
#define FP_COMP_OPS_EXE_SSE_FP_PACKED  PERF_EVENT(0x10, 0x10)
#define FP_COMP_OPS_EXE_SSE_FP_SCALAR  PERF_EVENT(0x10, 0x80)
#define SSE_DOUBLE_SCALAR_PACKED       PERF_EVENT(0x10, 0x90)
#define SIMD_FP_256_PACKED_DOUBLE      PERF_EVENT(0x11, 0x02)

/* KNL */
#define MEM_UOPS_RETIRED_ALL_LOADS_KNL     PERF_EVENT(0x04, 0x40)
#define MEM_UOPS_RETIRED_L2_HIT_LOADS_KNL  PERF_EVENT(0x04, 0x02)

/* SKX CLX */
#define FP_ARITH_INST_RETIRED_SCALAR_DOUBLE      PERF_EVENT(0xC7, 0x01)
#define FP_ARITH_INST_RETIRED_SCALAR_SINGLE      PERF_EVENT(0xC7, 0x02)
#define FP_ARITH_INST_RETIRED_128B_PACKED_DOUBLE PERF_EVENT(0xC7, 0x04)
#define FP_ARITH_INST_RETIRED_128B_PACKED_SINGLE PERF_EVENT(0xC7, 0x08)
#define FP_ARITH_INST_RETIRED_256B_PACKED_DOUBLE PERF_EVENT(0xC7, 0x10)
#define FP_ARITH_INST_RETIRED_256B_PACKED_SINGLE PERF_EVENT(0xC7, 0x20)
#define FP_ARITH_INST_RETIRED_512B_PACKED_DOUBLE PERF_EVENT(0xC7, 0x40)
#define FP_ARITH_INST_RETIRED_512B_PACKED_SINGLE PERF_EVENT(0xC7, 0x80)

/* HSW and later  */
#define DTLB_LOAD_MISSES_MISS_CAUSES_A_WALK PERF_EVENT(0x08, 0x01)
#define DTLB_LOAD_MISSES_WALK_COMPLETED_2M_4M PERF_EVENT(0x08, 0x04)
#define L1D_REPLACEMENT                PERF_EVENT(0x51, 0x01) 
#define RESOURCE_STALLS_ANY            PERF_EVENT(0xA2, 0x01) 
#define MEM_UOPS_RETIRED_ALL_LOADS     PERF_EVENT(0xD0, 0x81) // CTR0-3 Only
#define MEM_UOPS_RETIRED_ALL_STORES    PERF_EVENT(0xD0, 0x82) // CTR0-3 Only
#define MEM_LOAD_UOPS_RETIRED_L1_HIT   PERF_EVENT(0xD1, 0x01) // CTR0-3 Only
#define MEM_LOAD_UOPS_RETIRED_L2_HIT   PERF_EVENT(0xD1, 0x02) // CTR0-3 Only
#define MEM_LOAD_UOPS_RETIRED_LLC_HIT  PERF_EVENT(0xD1, 0x04) // CTR0-3 Only
#define MEM_LOAD_UOPS_RETIRED_LLC_MISS PERF_EVENT(0xD1, 0x20) // CTR0-3 Only
#define MEM_LOAD_UOPS_RETIRED_HIT_LFB  PERF_EVENT(0xD1, 0x40) // CTR0-3 Only
#define MEM_LOAD_UOPS_RETIRED_L1_MISS  PERF_EVENT(0xD1, 0x08)
#define MEM_LOAD_UOPS_RETIRED_L2_MISS  PERF_EVENT(0xD1, 0x10)
#define MEM_LOAD_UOPS_RETIRED_L3_MISS  PERF_EVENT(0xD1, 0x20)
#define MEM_LOAD_UOPS_RETIRED_HIT_LFB  PERF_EVENT(0xD1, 0x40)
#define L2_LINES_IN_ALL                PERF_EVENT(0xF1, 0x07)

static uint64_t skx_events[] = {
  FP_ARITH_INST_RETIRED_SCALAR_DOUBLE,
  FP_ARITH_INST_RETIRED_128B_PACKED_DOUBLE,
  FP_ARITH_INST_RETIRED_256B_PACKED_DOUBLE,
  FP_ARITH_INST_RETIRED_512B_PACKED_DOUBLE,
  FP_ARITH_INST_RETIRED_SCALAR_SINGLE,
  FP_ARITH_INST_RETIRED_128B_PACKED_SINGLE,
  FP_ARITH_INST_RETIRED_256B_PACKED_SINGLE,
  FP_ARITH_INST_RETIRED_512B_PACKED_SINGLE,
};
static char *skx_events_str[] = {
  "scalar_double",
  "128b_double",
  "256b_double",
  "512b_double",
  "scalar_single",
  "128b_single",
  "256b_single",
  "512b_single",
};

static  uint64_t knl_events[] = {
  MEM_UOPS_RETIRED_ALL_LOADS_KNL,
  MEM_UOPS_RETIRED_L2_HIT_LOADS_KNL,
};
static  uint64_t hsw_events[] = {
  MEM_UOPS_RETIRED_ALL_LOADS,
  MEM_LOAD_UOPS_RETIRED_L1_HIT,
  MEM_LOAD_UOPS_RETIRED_L2_HIT,
  L1D_REPLACEMENT,
  MEM_LOAD_UOPS_RETIRED_LLC_HIT,
  DTLB_LOAD_MISSES_MISS_CAUSES_A_WALK,
  RESOURCE_STALLS_ANY,
  L2_LINES_IN_ALL,
};
static  uint64_t snb_events[] = {
  MEM_UOPS_RETIRED_ALL_LOADS,
  MEM_LOAD_UOPS_RETIRED_L1_HIT,
  MEM_LOAD_UOPS_RETIRED_L2_HIT,
  MEM_LOAD_UOPS_RETIRED_LLC_HIT,
  L1D_REPLACEMENT,
  FP_COMP_OPS_EXE_SSE_FP_SCALAR,
  FP_COMP_OPS_EXE_SSE_FP_PACKED,
  SIMD_FP_256_PACKED_DOUBLE,
};
static  uint64_t nhm_events[] = {
  MEM_UNCORE_RETIRED_REMOTE_DRAM,
  MEM_UNCORE_RETIRED_LOCAL_DRAM,
  FP_COMP_OPS_EXE_SSE_FP_PACKED,
  FP_COMP_OPS_EXE_SSE_FP_SCALAR
};

//! Generate bitmask of n 1s
#define BIT_MASK(n) (~( ((~0ULL) << ((n)-1)) << 1 ))

int begin_intel_pmc3();
int collect_intel_pmc3();

#endif
