#ifndef _IMC_UTILS_H_
#define _IMC_UTILS_H_

#include <json/json.h>

#define pci_cfg_address(bus, dev, func) (bus << 20) | (dev << 15) | (func << 12)
#define index(address, off) (address | off)/4

#define DCLK_PMON_UNIT_CTL_REG    0xF4
#define DCLK_PMON_UNIT_STATUS_REG 0xF8

#define DCLK_PMON_CTR0_LOW_REG  0xA0
#define DCLK_PMON_CTR0_HIGH_REG 0xA4
#define DCLK_PMON_CTR1_LOW_REG  0xA8
#define DCLK_PMON_CTR1_HIGH_REG 0xAC
#define DCLK_PMON_CTR2_LOW_REG  0xB0
#define DCLK_PMON_CTR2_HIGH_REG 0xB4
#define DCLK_PMON_CTR3_LOW_REG  0xB8
#define DCLK_PMON_CTR3_HIGH_REG 0xBC

#define DCLK_PMON_CTRCTL0_REG   0xD8
#define DCLK_PMON_CTRCTL1_REG   0xDC
#define DCLK_PMON_CTRCTL2_REG   0xE0
#define DCLK_PMON_CTRCTL3_REG   0xE4
#define U_MSR_PMON_GLOBAL_CTL   0x0700

#define IMC_PERF_EVENT(event, umask)		\
  ( (event)                                     \
    | (umask << 8)                              \
    | (0UL << 17) /* Clear counter */           \
    | (0UL << 18) /* Edge Detection. */		\
    | (0UL << 20) /* Overflow disable */	\
    | (1UL << 22) /* Enable. */			\
    | (0UL << 23) /* Invert */			\
    | (0x0UL << 24) /* Threshold */		\
    )

#define CAS_READS      IMC_PERF_EVENT(0x04, 0x03)
#define CAS_WRITES     IMC_PERF_EVENT(0x04, 0x0C)
#define ACT_COUNT      IMC_PERF_EVENT(0x01, 0x0B)
#define PRE_COUNT_ALL  IMC_PERF_EVENT(0x02, 0x03)
#define PRE_COUNT_MISS IMC_PERF_EVENT(0x02, 0x01)

int begin_intel_skx_imc();
int collect_intel_skx_imc(json_object *jarray);

#endif
