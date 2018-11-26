/*!
 * Опции.
 *
 * Значения опций читаются из xml-файла
 * функцией load_options_from_xmlfile (),
 * сохраняются в таблице options_table.
 * Часть опций кэшируются в k128cp2_machine
 * для быстрого доступа во время работы эмулятора.
 *
 */

#include <string.h>
#include "system.h"
#include "options.h"
#include "machine.h"

// set/release global pointer to cp2 structure
#define SET_GLOBAL_POINTER     \
    if (cp2ptr != NULL) { \
        k128cp2_machine=cp2ptr; \
    } else { \
        sim_error ("null cp2 pointer"); \
    }
#define RELEASE_GLOBAL_POINTER k128cp2_machine=NULL;

// machine
extern Machine_T *k128cp2_machine;

//! Options table
static option_t options_table[] = {
	{ "dump//inst"              , OPT_TYPE_INT, 0, NULL},
	{ "dump//inststop"          , OPT_TYPE_INT, 0, NULL},
	{ "dump//fpu"               , OPT_TYPE_INT, 0, NULL},
	{ "dump//fpu_float"         , OPT_TYPE_INT, 0, NULL},
	{ "dump//gpr"               , OPT_TYPE_INT, 0, NULL},
	{ "dump//addrreg"           , OPT_TYPE_INT, 0, NULL},
	{ "dump//ctrlreg"           , OPT_TYPE_INT, 0, NULL},
	{ "dump//lstack"            , OPT_TYPE_INT, 0, NULL},
	{ "dump//pstack"            , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//enable"     , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//float"      , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//sect0"      , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//sect1"      , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//sect2"      , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//sect3"      , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//iram"       , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//k64"        , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//fcsr"       , OPT_TYPE_INT, 0, NULL},
	{ "dump//event//ctrlreg"    , OPT_TYPE_INT, 0, NULL},
	{ "dump//stop//regs"        , OPT_TYPE_INT, 0, NULL},
	{ "dump//stop//lmem//sect0" , OPT_TYPE_INT, 0, NULL},
	{ "dump//stop//lmem//sect1" , OPT_TYPE_INT, 0, NULL},
	{ "dump//stop//lmem//sect2" , OPT_TYPE_INT, 0, NULL},
	{ "dump//stop//lmem//sect3" , OPT_TYPE_INT, 0, NULL},
	{ "savestate//enable"       , OPT_TYPE_INT, 0, NULL},
	{ "savestate//onrun"        , OPT_TYPE_INT, 0, NULL},
	{ "savestate//onstop"       , OPT_TYPE_INT, 0, NULL},
	{ "savestate//message"      , OPT_TYPE_INT, 0, NULL},
	{ "savestate//fileprefix"   , OPT_TYPE_STR, 0, NULL},
	{ "output//tofile"          , OPT_TYPE_INT, 0, NULL},
	{ "output//filename"        , OPT_TYPE_STR, 0, NULL},
	{ NULL, 0, 0, NULL}
};


void update_options_cache_from_table ();

// set option value
void opt_set_value(option_t *opt, const char *newval)
{
	char *tailptr;
	uint64_t data;

	switch (opt->type) {
		case OPT_TYPE_INT:
			data = strtoull(newval, &tailptr, 10);
			if (tailptr != newval) {
				opt->valnum = data;
			} else {
				sim_error ("expecting number value for option '%s'", opt->name);
			}
			break;
		case OPT_TYPE_STR:
			if (opt->valstr != NULL) free(opt->valstr);
			opt->valstr = strdup(newval);
			break;
		default: sim_error_internal();
	}
}

	typedef union Option1 {
		char*  str;
		int num;
	} OptionValue;


// set option value
static void opt_get_value_by_optptr (const option_t *optptr, option_t *val)
{
	val->type = optptr->type;
	switch (optptr->type) {
		case OPT_TYPE_INT:
			val->valnum = optptr->valnum;
			break;
		case OPT_TYPE_STR:
			val->valstr = optptr->valstr;
			break;
		default: sim_error_internal();
	}
}


// load options from xmlfile
int load_options_from_string (char *str)
{
	// open xml doc
	if (open_xmldoc_on_string (str) != 0 ) {
		sim_warning ("cannot read options from string");
		return 1;
	}

	// load options
	load_options ();

	// close xml doc
	close_xmldoc ();

	// return
	return 0;
}


// load options from stream
int load_options_from_xmlfile (char *config_filename)
{
	// xml file initialization
	if( open_xmldoc_on_file (config_filename) != 0 ) {
		sim_warning ("cannot load options from xml file %s", config_filename);
		return 1;
	}

	// load options
	load_options ();

	// close xml file
	close_xmldoc ();

	// return
	return 0;
}


// load options
// provided that xml doc has been already opened
// (see load_options_from_string() or load_options_from_file() )
void load_options ()
{
	int i;
	static char strval[1000];

	// fill options_table
	i = 0;
#define opt (options_table[i])
	while (opt.name != NULL) {
		if(get_str_tagvalue_from_xmlfile(strval,"//options//%s",opt.name)) {
			opt_set_value(&opt, strval);
		}
#undef opt
		i++;
	}

	// update options cache in machine
	update_options_cache_from_table ();

	// process options
	process_options ();
}


//
option_t* opt_get_optptr_by_name(const char *name)
{

	int i;

	i = 0;

#define opt (options_table[i])
	while (
		(opt.name != NULL) &&
		(strcmp(opt.name, name) != 0)
	) {
		i++;
	}

	if (opt.name == NULL) {
		sim_warning ("cannot find option '%s'", name);
		return NULL;
	} else {
		return (&opt);
	}
#undef opt
}


//
static bool_t opt_get_value_by_name (const char *name, option_t *val)
{
	option_t *optptr;

	// find option in options_table
	optptr = opt_get_optptr_by_name(name);
	if (optptr == NULL) return FALSE;

	// get value
	opt_get_value_by_optptr (optptr, val);

	return TRUE;
}


//
void update_options_cache_from_table ()
{
	option_t val;
	val.valnum = 0;
	val.valstr = NULL;

	if (opt_get_value_by_name ("dump//inst"              ,  &val)) k128cp2_machine->opt_dump_inst      = val.valnum;
	if (opt_get_value_by_name ("dump//inststop"          ,  &val)) k128cp2_machine->opt_dump_inststop  = val.valnum;
	if (opt_get_value_by_name ("dump//fpu"               ,  &val)) k128cp2_machine->opt_dump_fpu       = val.valnum;
	if (opt_get_value_by_name ("dump//fpu_float"         ,  &val)) k128cp2_machine->opt_dump_fpu_float = val.valnum;
	if (opt_get_value_by_name ("dump//gpr"               ,  &val)) k128cp2_machine->opt_dump_gpr       = val.valnum;
	if (opt_get_value_by_name ("dump//addrreg"           ,  &val)) k128cp2_machine->opt_dump_addrreg   = val.valnum;
	if (opt_get_value_by_name ("dump//ctrlreg"           ,  &val)) k128cp2_machine->opt_dump_ctrlreg   = val.valnum;
	if (opt_get_value_by_name ("dump//lstack"            ,  &val)) k128cp2_machine->opt_dump_lstack    = val.valnum;
	if (opt_get_value_by_name ("dump//pstack"            ,  &val)) k128cp2_machine->opt_dump_pstack    = val.valnum;
	if (opt_get_value_by_name ("dump//event//enable"     ,  &val)) k128cp2_machine->opt_dump_event_enable  = val.valnum;
	if (opt_get_value_by_name ("dump//event//float"      ,  &val)) k128cp2_machine->opt_dump_event_float   = val.valnum;
	if (opt_get_value_by_name ("dump//event//sect0"      ,  &val)) k128cp2_machine->opt_dump_event_sect0   = val.valnum;
	if (opt_get_value_by_name ("dump//event//sect1"      ,  &val)) k128cp2_machine->opt_dump_event_sect1   = val.valnum;
	if (opt_get_value_by_name ("dump//event//sect2"      ,  &val)) k128cp2_machine->opt_dump_event_sect2   = val.valnum;
	if (opt_get_value_by_name ("dump//event//sect3"      ,  &val)) k128cp2_machine->opt_dump_event_sect3   = val.valnum;
	if (opt_get_value_by_name ("dump//event//iram"       ,  &val)) k128cp2_machine->opt_dump_event_iram    = val.valnum;
	if (opt_get_value_by_name ("dump//event//k64"        ,  &val)) k128cp2_machine->opt_dump_event_k64     = val.valnum;
	if (opt_get_value_by_name ("dump//event//fcsr"       ,  &val)) k128cp2_machine->opt_dump_event_fcsr    = val.valnum;
	if (opt_get_value_by_name ("dump//event//ctrlreg"    ,  &val)) k128cp2_machine->opt_dump_event_ctrlreg = val.valnum;
	if (opt_get_value_by_name ("dump//stop//regs"        ,  &val)) k128cp2_machine->opt_haltdump_regs    = val.valnum;
	if (opt_get_value_by_name ("dump//stop//lmem//sect0" ,  &val)) k128cp2_machine->opt_haltdump_lmem_sect0  = val.valnum;
	if (opt_get_value_by_name ("dump//stop//lmem//sect1" ,  &val)) k128cp2_machine->opt_haltdump_lmem_sect1  = val.valnum;
	if (opt_get_value_by_name ("dump//stop//lmem//sect2" ,  &val)) k128cp2_machine->opt_haltdump_lmem_sect2  = val.valnum;
	if (opt_get_value_by_name ("dump//stop//lmem//sect3" ,  &val)) k128cp2_machine->opt_haltdump_lmem_sect3  = val.valnum;
	if (opt_get_value_by_name ("savestate//enable"       ,  &val)) k128cp2_machine->opt_savestate_enable      = val.valnum;
	if (opt_get_value_by_name ("savestate//onrun"        ,  &val)) k128cp2_machine->opt_savestate_onrun       = val.valnum;
	if (opt_get_value_by_name ("savestate//onstop"       ,  &val)) k128cp2_machine->opt_savestate_onstop      = val.valnum;
	if (opt_get_value_by_name ("savestate//message"      ,  &val)) k128cp2_machine->opt_savestate_message     = val.valnum;
	if (opt_get_value_by_name ("savestate//fileprefix"   ,  &val)) k128cp2_machine->opt_savestate_fileprefix  = val.valstr;
}

//! Process options
void process_options () {

	option_t val;
	val.valnum = 0;
	val.valstr = NULL;

	// set output file
	opt_get_value_by_name ("output//tofile", &val);
	if (val.valnum == 0) {
		set_output_file (NULL);
	} else {
		opt_get_value_by_name ("output//filename", &val);
		set_output_file (val.valstr);
	}
}

//
void libk128cp2_load_option (void *cp2ptr, const char* opt_name, const char *newval)
{
	SET_GLOBAL_POINTER;

	option_t *opt;

	opt = opt_get_optptr_by_name(opt_name);
	opt_set_value(opt, newval);

	update_options_cache_from_table ();

//	process_options ();

	RELEASE_GLOBAL_POINTER;
}
