#include <stdio.h>
#include "string.h"
#include "system.h"
#include "machine.h"
#include "observer.h"
#include "options.h"

// machine
extern Machine_T *k128cp2_machine;

// cycle counter
uint64_t step_number;

// events buffer
// 2D buffer
// 1st dimension: time shift
//   events_buffer_start = current time
// 2nd dimension: different events
#define EVENTS_BUFFER_TIMEDEPTH     20
#define EVENTS_BUFFER_EVENTS_MAXNUM 100
struct {
	event_t events[EVENTS_BUFFER_EVENTS_MAXNUM];
	int events_num;
} events_buffer[EVENTS_BUFFER_TIMEDEPTH];
int events_buffer_start;


//
void observer_init () {

	// init events buffer
	events_buffer_init ();
}

/*
//
void observer_newevent_delayed (int delay) {

	// place new event info into delayed events buffer
	eventsbuffer_new_entry (&observer_newevent_data, delay);
}
*/


// interface with event sources
// event data stored in observer_newevent_data
void observer_newevent (event_t *eventptr) {

	// place new event info in events buffer
	eventsbuffer_new_entry (eventptr, 0);
}


// observer interface
void send_newevent (
	uint64_t     clck,
	event_src_t  src,
	event_type_t type,
	int          sect,
	uint32_t     addr,
	uint64_t     data,
	bool_t       mod
) {

	event_t ev;

	// fill observer interface structure
	ev.clck  = clck;
	ev.src   = src;
	ev.type  = type;
	ev.sect  = sect;
	ev.addr  = addr;
	ev.data  = data;
	ev.mask     = 0;  // fake
	ev.ldc2flag = 0;  // fake
	ev.mod   = mod;

	// notify observer
    observer_newevent (&ev);
//	eventsbuffer_new_entry (&observer_newevent_data, 0);
}


// start of new cycle
void observer_step_start (uint64_t new_step_number) {

	// update cycle counter
	step_number = new_step_number;

	// flush events buffer
//	eventsbuffer_flush ();
}


// end of cycle => process collected info
void observer_step_finish () {

	// dump events of current cycle
	if (k128cp2_machine -> opt_dump_event_enable) {
		eventsbuffer_dump_current ();
	}

	// shift events buffer
	eventsbuffer_timestep ();

	// dump additional info
	#define fout (k128cp2_machine -> outstream)
//	prockern_dump_lmem_pipe     (fout);
	if (k128cp2_machine -> opt_dump_fpu)     prockern_dump_fpu     (fout);
	if (k128cp2_machine -> opt_dump_gpr)     prockern_dump_gpr     (fout);
	if (k128cp2_machine -> opt_dump_addrreg) prockern_dump_addrreg (fout);
//	if (k128cp2_machine -> opt_dump_ctrlreg) prockern_dump_ctrlreg (fout);
	#undef fout

}


// init events buffer
void events_buffer_init () {
	int i;
	events_buffer_start = 0;
	for (i=0; i<EVENTS_BUFFER_TIMEDEPTH; i++) {
		events_buffer[i].events_num = 0;
	}
}


// shift events buffer one step in time
void eventsbuffer_timestep () {

	// clear current entry
	events_buffer[events_buffer_start].events_num = 0;

	// step to next entry
	events_buffer_start = (events_buffer_start + 1) % EVENTS_BUFFER_TIMEDEPTH;
}


// place new event into events buffer
void eventsbuffer_new_entry (event_t *new_event, int delay) {

	int index;

	// delay overflow check
	if (delay >= EVENTS_BUFFER_TIMEDEPTH) {
		sim_error_internal();
		return;
	}

	// get buffer index (according to time)
	index = (events_buffer_start + delay) % EVENTS_BUFFER_TIMEDEPTH;

	// events number overflow check
#define evnum events_buffer[index].events_num
	if (evnum >= EVENTS_BUFFER_EVENTS_MAXNUM) {
		sim_error("Error: events buffer overflow. The most possible reason: k64 works with stalled cp2.\n");
		sim_error_internal();
		return;
	}

	// add new entry
	events_buffer[index].events[evnum] = *new_event;

	// increment events number
	evnum += 1;
#undef evnum

}


// dump current events (that corresponds to current time step)
void eventsbuffer_dump_current () {
	int i;
	for (i=0; i<events_buffer[events_buffer_start].events_num; i++) {
#define ev events_buffer[events_buffer_start].events[i]

		if (
			( ev.sect == REGID_SECT_NONE) ||
			((ev.sect == REGID_SECT_0) && k128cp2_machine->opt_dump_event_sect0) ||
			((ev.sect == REGID_SECT_1) && k128cp2_machine->opt_dump_event_sect1) ||
			((ev.sect == REGID_SECT_2) && k128cp2_machine->opt_dump_event_sect2) ||
			((ev.sect == REGID_SECT_3) && k128cp2_machine->opt_dump_event_sect3)
		) {
			eventsbuffer_dump_event (&ev);
		}

	}
}


//
char* event_type_str(event_src_t type) {
	switch(type) {
		case EVENT_TYPE_READ        : return "READ   "; break;
		case EVENT_TYPE_WRITE       : return "WRITE  "; break;
		case EVENT_TYPE_RQSTART     : return "RQSTR"; break;
		case EVENT_TYPE_RQFINISH    : return "RQFIN"; break;
		case EVENT_TYPE_READ_BYPASS : return "READ_BP"; break;
		default                     : return "???  "; break;
	}
}


//
char* event_src_str(event_src_t src) {
	switch(src) {
		case EVENT_SRC_FPR           :  return "FPR    "; break;
		case EVENT_SRC_FPR_BYPASS    :  return "FPR_BP "; break;
		case EVENT_SRC_FCCR          :  return "FCCR   "; break;
		case EVENT_SRC_FCSR          :  return "FCSR   "; break;
		case EVENT_SRC_CTRLREG       :  return "CTRLREG"; break;
		case EVENT_SRC_GPR           :  return "GPR    "; break;
		case EVENT_SRC_ADDRAN        :  return "AN     "; break;
		case EVENT_SRC_ADDRAN_BP     :  return "AN_BP  "; break;
		case EVENT_SRC_ADDRNN        :  return "NN     "; break;
		case EVENT_SRC_ADDRMN        :  return "MN     "; break;
		case EVENT_SRC_LMEM          :  return "LMEM   "; break;
		case EVENT_SRC_IREG          :  return "IREG   "; break;
		case EVENT_SRC_IRAM          :  return "IRAM   "; break;
		case EVENT_SRC_FIFO          :  return "FIFO   "; break;
		case EVENT_SRC_SYSCTRL       :  return "SYSCTRL"; break;
		case EVENT_SRC_K64           :  return "K64    "; break;
		default                      :  return "???    "; break;
	}
}

//
void eventsbuffer_dump_event (event_t *event_ptr) {

	uint32_t datahi,datalo;
	static char str[200]      ;
	static char idstr[100]    ;
	static char clckstr[100]  ;
	static char sectstr[100]  ;
	static char srcstr[100]   ;
	static char typestr[100]  ;
	static char addrstr[100]  ;
	static char valstr[100]   ;
	static char srck64str[100];

	// option dump_event_iram/k64 support
	if (
		((event_ptr->src == EVENT_SRC_IRAM   ) && (!k128cp2_machine->opt_dump_event_iram   )) ||
		((event_ptr->src == EVENT_SRC_K64    ) && (!k128cp2_machine->opt_dump_event_k64    )) ||
		((event_ptr->src == EVENT_SRC_FCSR   ) && (!k128cp2_machine->opt_dump_event_fcsr   )) ||
		((event_ptr->src == EVENT_SRC_CTRLREG) && (!k128cp2_machine->opt_dump_event_ctrlreg))
	) {
		return;
	}

	if( event_ptr->mod == 0 )
		return;

	// flush strings
	str[0] = clckstr[0] = idstr[0] = sectstr[0] = srcstr[0] = typestr[0] = addrstr[0] = valstr[0] = srck64str[0] = 0;

	// fill clock
	sprintf (clckstr, "clck=%" PRId64, (uint64_t) kern.k64clock);

	// fill idstr
	sprintf (idstr, "id=%" PRId64, event_ptr->clck);

	// fill sectstr
	if (event_ptr->sect < NUMBER_OF_EXESECT) {
		sprintf (sectstr, "sect=%d", (int) event_ptr->sect);
	}

	// fill srcstr
	sprintf (srcstr, "src=%s", event_src_str (event_ptr->src));

	// fill typestr
	sprintf (typestr, "type=%s", event_type_str(event_ptr->type));

	// fill addrstr
	sprintf (addrstr, "addr/num=%08x", (int) event_ptr->addr);

	// fill valstr
	sprintf (valstr, "val=");
	// print data
	if (
		k128cp2_machine->opt_dump_fpu_float &&
		( (event_ptr->src == EVENT_SRC_FPR) ||
		  (event_ptr->src == EVENT_SRC_FPR_BYPASS) ||
		  (event_ptr->src == EVENT_SRC_LMEM)
		)
	) {
		datalo = event_ptr->data & bits64(31,0);
		datahi = event_ptr->data >> 32;
		sprintf (valstr+strlen(valstr), "(%10.1f, %-10.1f)",
			hex2float(datahi), hex2float(datalo)
		);
	} else {
		sprintf (valstr+strlen(valstr), "%016" PRIx64, event_ptr->data);
	}

	// for k64 interface print mask and ldc2flag
	if (
		(event_ptr->src  == EVENT_SRC_K64) &&
		(event_ptr->type == EVENT_TYPE_WRITE)
	) {
		sprintf (srck64str, "mask=%016" PRIx64 " ldc2flag=%d",
			event_ptr->mask, event_ptr->ldc2flag
		);
	}

	// compose final string
	snprintf (str, sizeof(str), "dumpevent: %s %s %s %s %s %s %s %s",
		clckstr,
		idstr,
		sectstr,
		srcstr,
		typestr,
		addrstr,
		valstr,
		srck64str
	);

	// output
	sim_print_string (str);

}

