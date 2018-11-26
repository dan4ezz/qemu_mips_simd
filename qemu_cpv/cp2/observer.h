#ifndef __observer_h__
#define __observer_h__

#include "common.h"

// section type
#if NUMBER_OF_EXESECT != 4
#error "Number of sections must be 4"
#endif
typedef enum {
	REGID_SECT_0 = 0,
	REGID_SECT_1 = 1,
	REGID_SECT_2 = 2,
	REGID_SECT_3 = 3,
	REGID_SECT_NONE
} sectid_t;

// event sources
typedef enum {
	EVENT_SRC_FPR,
	EVENT_SRC_FPR_BYPASS,
	EVENT_SRC_FCCR,
	EVENT_SRC_FCSR,
	EVENT_SRC_CTRLREG,
	EVENT_SRC_GPR,
	EVENT_SRC_ADDRAN,
	EVENT_SRC_ADDRAN_BP, // AN register bypass
	EVENT_SRC_ADDRNN,
	EVENT_SRC_ADDRMN,
	EVENT_SRC_IREG,
	EVENT_SRC_LMEM,
	EVENT_SRC_IRAM,
	EVENT_SRC_FIFO,
	EVENT_SRC_SYSCTRL,
	EVENT_SRC_K64          // k64 interface (read/write interface registers)
} event_src_t;

// event types
typedef enum {
	EVENT_TYPE_READ,
	EVENT_TYPE_WRITE,
	EVENT_TYPE_RQSTART,
	EVENT_TYPE_RQFINISH,
	EVENT_TYPE_READ_BYPASS
} event_type_t;


// event descriptor
typedef struct {
	uint64_t     clck;
	event_src_t  src;
	event_type_t type;
	sectid_t     sect;
	uint32_t     addr;
	uint64_t     data;
	uint64_t     mask;
	int          ldc2flag; // for k64 interface
	bool_t       mod;
} event_t;


// function headers
void observer_newevent    (event_t *eventptr);
void send_newevent (
	uint64_t     clck,
	event_src_t  src,
	event_type_t type,
	int          sect,
	uint32_t     addr,
	uint64_t     data,
	bool_t       mod
);

// events buffer methods
void events_buffer_init ();
void eventsbuffer_timestep     ();
void eventsbuffer_new_entry    (event_t *new_event, int delay);
void eventsbuffer_dump_current ();
void eventsbuffer_dump_event   (event_t *event_ptr);

//
void observer_init        ();
void observer_step_start  (uint64_t new_step_number);
void observer_step_finish ();
void observer_newevent_delayed (int delay);



#endif // __observer_h__
