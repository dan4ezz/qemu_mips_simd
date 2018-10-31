/* This file is autogenerated by tracetool, do not edit. */

#include "qemu/osdep.h"
#include "trace.h"

uint16_t _TRACE_SDHCI_SET_INSERTED_DSTATE;
uint16_t _TRACE_SDHCI_SEND_COMMAND_DSTATE;
uint16_t _TRACE_SDHCI_ERROR_DSTATE;
uint16_t _TRACE_SDHCI_RESPONSE4_DSTATE;
uint16_t _TRACE_SDHCI_RESPONSE16_DSTATE;
uint16_t _TRACE_SDHCI_END_TRANSFER_DSTATE;
uint16_t _TRACE_SDHCI_ADMA_DSTATE;
uint16_t _TRACE_SDHCI_ADMA_LOOP_DSTATE;
uint16_t _TRACE_SDHCI_ADMA_TRANSFER_COMPLETED_DSTATE;
uint16_t _TRACE_SDHCI_ACCESS_DSTATE;
uint16_t _TRACE_SDHCI_READ_DATAPORT_DSTATE;
uint16_t _TRACE_SDHCI_WRITE_DATAPORT_DSTATE;
uint16_t _TRACE_MILKYMIST_MEMCARD_MEMORY_READ_DSTATE;
uint16_t _TRACE_MILKYMIST_MEMCARD_MEMORY_WRITE_DSTATE;
uint16_t _TRACE_PXA2XX_MMCI_READ_DSTATE;
uint16_t _TRACE_PXA2XX_MMCI_WRITE_DSTATE;
TraceEvent _TRACE_SDHCI_SET_INSERTED_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_set_inserted",
    .sstate = TRACE_SDHCI_SET_INSERTED_ENABLED,
    .dstate = &_TRACE_SDHCI_SET_INSERTED_DSTATE 
};
TraceEvent _TRACE_SDHCI_SEND_COMMAND_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_send_command",
    .sstate = TRACE_SDHCI_SEND_COMMAND_ENABLED,
    .dstate = &_TRACE_SDHCI_SEND_COMMAND_DSTATE 
};
TraceEvent _TRACE_SDHCI_ERROR_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_error",
    .sstate = TRACE_SDHCI_ERROR_ENABLED,
    .dstate = &_TRACE_SDHCI_ERROR_DSTATE 
};
TraceEvent _TRACE_SDHCI_RESPONSE4_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_response4",
    .sstate = TRACE_SDHCI_RESPONSE4_ENABLED,
    .dstate = &_TRACE_SDHCI_RESPONSE4_DSTATE 
};
TraceEvent _TRACE_SDHCI_RESPONSE16_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_response16",
    .sstate = TRACE_SDHCI_RESPONSE16_ENABLED,
    .dstate = &_TRACE_SDHCI_RESPONSE16_DSTATE 
};
TraceEvent _TRACE_SDHCI_END_TRANSFER_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_end_transfer",
    .sstate = TRACE_SDHCI_END_TRANSFER_ENABLED,
    .dstate = &_TRACE_SDHCI_END_TRANSFER_DSTATE 
};
TraceEvent _TRACE_SDHCI_ADMA_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_adma",
    .sstate = TRACE_SDHCI_ADMA_ENABLED,
    .dstate = &_TRACE_SDHCI_ADMA_DSTATE 
};
TraceEvent _TRACE_SDHCI_ADMA_LOOP_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_adma_loop",
    .sstate = TRACE_SDHCI_ADMA_LOOP_ENABLED,
    .dstate = &_TRACE_SDHCI_ADMA_LOOP_DSTATE 
};
TraceEvent _TRACE_SDHCI_ADMA_TRANSFER_COMPLETED_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_adma_transfer_completed",
    .sstate = TRACE_SDHCI_ADMA_TRANSFER_COMPLETED_ENABLED,
    .dstate = &_TRACE_SDHCI_ADMA_TRANSFER_COMPLETED_DSTATE 
};
TraceEvent _TRACE_SDHCI_ACCESS_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_access",
    .sstate = TRACE_SDHCI_ACCESS_ENABLED,
    .dstate = &_TRACE_SDHCI_ACCESS_DSTATE 
};
TraceEvent _TRACE_SDHCI_READ_DATAPORT_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_read_dataport",
    .sstate = TRACE_SDHCI_READ_DATAPORT_ENABLED,
    .dstate = &_TRACE_SDHCI_READ_DATAPORT_DSTATE 
};
TraceEvent _TRACE_SDHCI_WRITE_DATAPORT_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "sdhci_write_dataport",
    .sstate = TRACE_SDHCI_WRITE_DATAPORT_ENABLED,
    .dstate = &_TRACE_SDHCI_WRITE_DATAPORT_DSTATE 
};
TraceEvent _TRACE_MILKYMIST_MEMCARD_MEMORY_READ_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "milkymist_memcard_memory_read",
    .sstate = TRACE_MILKYMIST_MEMCARD_MEMORY_READ_ENABLED,
    .dstate = &_TRACE_MILKYMIST_MEMCARD_MEMORY_READ_DSTATE 
};
TraceEvent _TRACE_MILKYMIST_MEMCARD_MEMORY_WRITE_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "milkymist_memcard_memory_write",
    .sstate = TRACE_MILKYMIST_MEMCARD_MEMORY_WRITE_ENABLED,
    .dstate = &_TRACE_MILKYMIST_MEMCARD_MEMORY_WRITE_DSTATE 
};
TraceEvent _TRACE_PXA2XX_MMCI_READ_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "pxa2xx_mmci_read",
    .sstate = TRACE_PXA2XX_MMCI_READ_ENABLED,
    .dstate = &_TRACE_PXA2XX_MMCI_READ_DSTATE 
};
TraceEvent _TRACE_PXA2XX_MMCI_WRITE_EVENT = {
    .id = 0,
    .vcpu_id = TRACE_VCPU_EVENT_NONE,
    .name = "pxa2xx_mmci_write",
    .sstate = TRACE_PXA2XX_MMCI_WRITE_ENABLED,
    .dstate = &_TRACE_PXA2XX_MMCI_WRITE_DSTATE 
};
TraceEvent *hw_sd_trace_events[] = {
    &_TRACE_SDHCI_SET_INSERTED_EVENT,
    &_TRACE_SDHCI_SEND_COMMAND_EVENT,
    &_TRACE_SDHCI_ERROR_EVENT,
    &_TRACE_SDHCI_RESPONSE4_EVENT,
    &_TRACE_SDHCI_RESPONSE16_EVENT,
    &_TRACE_SDHCI_END_TRANSFER_EVENT,
    &_TRACE_SDHCI_ADMA_EVENT,
    &_TRACE_SDHCI_ADMA_LOOP_EVENT,
    &_TRACE_SDHCI_ADMA_TRANSFER_COMPLETED_EVENT,
    &_TRACE_SDHCI_ACCESS_EVENT,
    &_TRACE_SDHCI_READ_DATAPORT_EVENT,
    &_TRACE_SDHCI_WRITE_DATAPORT_EVENT,
    &_TRACE_MILKYMIST_MEMCARD_MEMORY_READ_EVENT,
    &_TRACE_MILKYMIST_MEMCARD_MEMORY_WRITE_EVENT,
    &_TRACE_PXA2XX_MMCI_READ_EVENT,
    &_TRACE_PXA2XX_MMCI_WRITE_EVENT,
  NULL,
};

static void trace_hw_sd_register_events(void)
{
    trace_event_register_group(hw_sd_trace_events);
}
trace_init(trace_hw_sd_register_events)
