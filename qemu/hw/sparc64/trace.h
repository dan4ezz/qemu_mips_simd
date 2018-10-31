/* This file is autogenerated by tracetool, do not edit. */

#ifndef TRACE_HW_SPARC64_GENERATED_TRACERS_H
#define TRACE_HW_SPARC64_GENERATED_TRACERS_H

#include "qemu-common.h"
#include "trace/control.h"

extern TraceEvent _TRACE_EBUS_ISA_IRQ_HANDLER_EVENT;
extern TraceEvent _TRACE_SUN4U_IOMMU_MEM_READ_EVENT;
extern TraceEvent _TRACE_SUN4U_IOMMU_MEM_WRITE_EVENT;
extern TraceEvent _TRACE_SUN4U_IOMMU_TRANSLATE_EVENT;
extern uint16_t _TRACE_EBUS_ISA_IRQ_HANDLER_DSTATE;
extern uint16_t _TRACE_SUN4U_IOMMU_MEM_READ_DSTATE;
extern uint16_t _TRACE_SUN4U_IOMMU_MEM_WRITE_DSTATE;
extern uint16_t _TRACE_SUN4U_IOMMU_TRANSLATE_DSTATE;
#define TRACE_EBUS_ISA_IRQ_HANDLER_ENABLED 1
#define TRACE_SUN4U_IOMMU_MEM_READ_ENABLED 1
#define TRACE_SUN4U_IOMMU_MEM_WRITE_ENABLED 1
#define TRACE_SUN4U_IOMMU_TRANSLATE_ENABLED 1
#include "qemu/log.h"


#define TRACE_EBUS_ISA_IRQ_HANDLER_BACKEND_DSTATE() ( \
    trace_event_get_state_dynamic_by_id(TRACE_EBUS_ISA_IRQ_HANDLER) || \
    false)

static inline void _nocheck__trace_ebus_isa_irq_handler(int n, int level)
{
    if (trace_event_get_state(TRACE_EBUS_ISA_IRQ_HANDLER)) {
        struct timeval _now;
        gettimeofday(&_now, NULL);
        qemu_log_mask(LOG_TRACE,
                      "%d@%zd.%06zd:ebus_isa_irq_handler " "Set ISA IRQ %d level %d" "\n",
                      getpid(),
                      (size_t)_now.tv_sec, (size_t)_now.tv_usec
                      , n, level);
    }
}

static inline void trace_ebus_isa_irq_handler(int n, int level)
{
    if (true) {
        _nocheck__trace_ebus_isa_irq_handler(n, level);
    }
}

#define TRACE_SUN4U_IOMMU_MEM_READ_BACKEND_DSTATE() ( \
    trace_event_get_state_dynamic_by_id(TRACE_SUN4U_IOMMU_MEM_READ) || \
    false)

static inline void _nocheck__trace_sun4u_iommu_mem_read(uint64_t addr, uint64_t val, int size)
{
    if (trace_event_get_state(TRACE_SUN4U_IOMMU_MEM_READ)) {
        struct timeval _now;
        gettimeofday(&_now, NULL);
        qemu_log_mask(LOG_TRACE,
                      "%d@%zd.%06zd:sun4u_iommu_mem_read " "addr: 0x%"PRIx64" val: 0x%"PRIx64" size: %d" "\n",
                      getpid(),
                      (size_t)_now.tv_sec, (size_t)_now.tv_usec
                      , addr, val, size);
    }
}

static inline void trace_sun4u_iommu_mem_read(uint64_t addr, uint64_t val, int size)
{
    if (true) {
        _nocheck__trace_sun4u_iommu_mem_read(addr, val, size);
    }
}

#define TRACE_SUN4U_IOMMU_MEM_WRITE_BACKEND_DSTATE() ( \
    trace_event_get_state_dynamic_by_id(TRACE_SUN4U_IOMMU_MEM_WRITE) || \
    false)

static inline void _nocheck__trace_sun4u_iommu_mem_write(uint64_t addr, uint64_t val, int size)
{
    if (trace_event_get_state(TRACE_SUN4U_IOMMU_MEM_WRITE)) {
        struct timeval _now;
        gettimeofday(&_now, NULL);
        qemu_log_mask(LOG_TRACE,
                      "%d@%zd.%06zd:sun4u_iommu_mem_write " "addr: 0x%"PRIx64" val: 0x%"PRIx64" size: %d" "\n",
                      getpid(),
                      (size_t)_now.tv_sec, (size_t)_now.tv_usec
                      , addr, val, size);
    }
}

static inline void trace_sun4u_iommu_mem_write(uint64_t addr, uint64_t val, int size)
{
    if (true) {
        _nocheck__trace_sun4u_iommu_mem_write(addr, val, size);
    }
}

#define TRACE_SUN4U_IOMMU_TRANSLATE_BACKEND_DSTATE() ( \
    trace_event_get_state_dynamic_by_id(TRACE_SUN4U_IOMMU_TRANSLATE) || \
    false)

static inline void _nocheck__trace_sun4u_iommu_translate(uint64_t addr, uint64_t trans_addr, uint64_t tte)
{
    if (trace_event_get_state(TRACE_SUN4U_IOMMU_TRANSLATE)) {
        struct timeval _now;
        gettimeofday(&_now, NULL);
        qemu_log_mask(LOG_TRACE,
                      "%d@%zd.%06zd:sun4u_iommu_translate " "xlate 0x%"PRIx64" => pa 0x%"PRIx64" tte: 0x%"PRIx64 "\n",
                      getpid(),
                      (size_t)_now.tv_sec, (size_t)_now.tv_usec
                      , addr, trans_addr, tte);
    }
}

static inline void trace_sun4u_iommu_translate(uint64_t addr, uint64_t trans_addr, uint64_t tte)
{
    if (true) {
        _nocheck__trace_sun4u_iommu_translate(addr, trans_addr, tte);
    }
}
#endif /* TRACE_HW_SPARC64_GENERATED_TRACERS_H */
