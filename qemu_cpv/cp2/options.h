#ifndef __options_h__
#define __options_h__

// options types
typedef enum {
	OPT_TYPE_INT=0,
	OPT_TYPE_STR
} opt_type_t;


// typedef for one option
typedef struct {
	char       *name;
	opt_type_t  type;
	int         valnum;
	char       *valstr;
} option_t;


// function headers
void load_options              ();
int  load_options_from_xmlfile (char *config_filename);
int  load_options_from_string  (char *str);
void process_options ();

#endif // __options_h__
