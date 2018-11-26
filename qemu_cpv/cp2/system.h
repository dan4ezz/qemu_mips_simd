#ifndef __system_h__
#define __system_h__

#include "common.h"

void system_check ();
void sim_print_string (const char *str);
void __attribute__((format (printf,1,2)))          sim_printf     (const char fmt[], ...);
void __attribute__((noreturn,format (printf,1,2))) sim_error      (const char fmt[], ...);
void __attribute__((format (printf,1,2)))          sim_warning    (const char fmt[], ...);

bool_t file_exist (char *filename);

int  open_xmldoc_on_string (char *str);
int  open_xmldoc_on_file   (char *filename);
void close_xmldoc          ();

bool_t __attribute__((format (printf,2,3)))
get_str_tagvalue_from_xmlfile(char* str, const char *pathfmt, ...);

#define sim_assert(_cond) if (!(_cond)) sim_error ("error at %s:%d",__FILE__,__LINE__);

#endif // __system_h__

