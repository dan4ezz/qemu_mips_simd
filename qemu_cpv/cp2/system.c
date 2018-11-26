#include <stdio.h>
#include <sys/stat.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <string.h>
#include <stdarg.h>
#include "system.h"
#include "machine.h"
#include "common.h"
#include "prockern.h"
#include "prockern_access.h"


// machine
extern Machine_T *k128cp2_machine;

// for parsing xml files
xmlDocPtr doc;


//
void system_check() {
	// check sizeof's
	if (sizeof(uint8_t)  != 1) sim_error("sizeof(uint8_t)!=1");
	if (sizeof(uint32_t) != 4) sim_error("sizeof(uint32_t)!=4");
	if (sizeof(uint64_t) != 8) sim_error("sizeof(uint64_t)!=8");
}


//
void sim_print_string (const char *str) {

	FILE *f;

	// FIXME implement option that enables clock printing
//	fprintf(stdout,"cp2 %llu %llu %s\n", kern.k64clock, kern.clockcount, str);

	// set output stream
	f = stdout;
	if (
		(k128cp2_machine != NULL) &&
		(k128cp2_machine -> outstream != NULL)
	) {
		f = k128cp2_machine -> outstream;
	}

	// print
	fprintf (f,"<libk128cp2 %s>\n", str);
	fflush (f);
}


//
void __attribute__((format (printf,1,2)))
sim_printf (const char fmt[], ...) {
	static char str[1000];
	va_list ap;
	va_start (ap, fmt);
	vsnprintf (str, sizeof(str)-1, fmt, ap);
	sim_print_string (str);
}


// sim error
void __attribute__((noreturn,format (printf,1,2)))
sim_error (const char fmt[], ...) {

	int l;
	static char errstr[100];
	va_list ap;

	l = sprintf (errstr, "Error: ");

	va_start (ap, fmt);
	vsnprintf (errstr+l, sizeof(errstr)-l, fmt, ap);
	sim_print_string (errstr);

	exit(1);
}


// sim warning
void __attribute__((format (printf,1,2)))
sim_warning (const char fmt[], ...) {

	int l;
	static char errstr[100];
	va_list ap;

	l = sprintf (errstr, "Warning: ");

	va_start (ap,fmt);
	vsnprintf (errstr+l, sizeof(errstr)-l, fmt, ap);
	sim_print_string (errstr);
}



/*
//
void set_host_endianity () {

	uint32_t x;

    ((uint8 *) &x)[0] = 0;
    ((uint8 *) &x)[1] = 1;
    ((uint8 *) &x)[2] = 2;
    ((uint8 *) &x)[3] = 3;

    if (x == 0x03020100) {
		// little endian
        host_bigendian = FALSE;
    } else if (x == 0x00010203) {
		// big endian
        host_bigendian = TRUE;
    } else {
		sim_error_internal ();
    }

}
*/


//
uint32_t swap_word(uint32_t w)
{
    return ((w & 0x0ff) << 24) | (((w >> 8) & 0x0ff) << 16) |
        (((w >> 16) & 0x0ff) << 8) | ((w >> 24) & 0x0ff);
}


//
uint64_t swap_dword(uint64_t w)
{   uint32_t wl, wr;
    wr = w;       wr = swap_word(wr);
    wl = w >> 32; wl = swap_word(wl);
    return (((uint64_t)wr) << 32) | wl;
}

//
xmlXPathObjectPtr
getnodeset (xmlDocPtr doc, xmlChar *xpath){

        xmlXPathContextPtr context;
        xmlXPathObjectPtr result;

        context = xmlXPathNewContext(doc);
        if (context == NULL) {
                return NULL;
        }
        result = xmlXPathEvalExpression(xpath, context);
        xmlXPathFreeContext(context);
        if (result == NULL) {
                return NULL;
        }
        if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
                xmlXPathFreeObject(result);
                return NULL;
        }
        return result;
}



//
void convert_hexstr_ui32 (uint32_t *dst, int size, char *hexstr) {

	int i, shift;

	i = 0;
	while (
		( *hexstr != '\0' ) &&
		(i<size/sizeof(uint32_t)) &&
		(sscanf(hexstr,"%8x%n", &(dst[i]), &shift) == 1)
	) {
		hexstr += shift;
		i ++;
	}
}



//
void convert_hexstr_ui64 (uint64_t *dst, int size, char *hexstr) {

	int i, shift;

	i = 0;
	while (
		( *hexstr != '\0' ) &&
		(i<size/sizeof(uint64_t)) &&
		(sscanf(hexstr,"%16" PRIx64 "%n", &(dst[i]), &shift) == 1)
	) {
		hexstr += shift;
		i ++;
	}
}




// get xml doc with string parsing
int open_xmldoc_on_string (char *str)
{
	doc = xmlParseMemory (str, strlen(str));

	if (doc == NULL ) {
		sim_warning ("cannot parse xml string");
		return 1;
	}

	return 0;
}


// get xml doc with file parsing
int open_xmldoc_on_file (char *filename)
{
	if (! file_exist (filename)) {
		sim_warning ("cannot open file %s", filename);
		return 1;
	}

	doc = xmlParseFile (filename);

	if (doc == NULL ) {
		sim_warning ("cannot parse xml file %s", filename);
		return 1;
	}

	return 0;
}

//
void close_xmldoc()
{
	xmlFreeDoc(doc);
}




//
void xmldoc_load_lmem (uint32_t *lmem, int size, char *path) {

	xmlChar *nodeval;
	xmlNodeSetPtr nodeset;
	xmlXPathObjectPtr result;

	// find node
	result = getnodeset (doc, (xmlChar*) path);

	// get value and fill memory
	if (result != NULL) {
		nodeset = result->nodesetval;
		nodeval = xmlNodeListGetString (doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
		convert_hexstr_ui32 (lmem, size, (char*)nodeval);
		xmlFree(nodeval);
		xmlXPathFreeObject (result);
	} else {
		sim_warning ("tag %s not found", path);
	}

	// cleanup
	xmlCleanupParser();
}


//
void xmldoc_load_iram (uint64_t *iram, int size, char *path) {

	xmlChar *nodeval;
	xmlNodeSetPtr nodeset;
	xmlXPathObjectPtr result;

	// find node
	result = getnodeset (doc, (xmlChar*) path);

	// get value and fill memory
	if (result != NULL) {
		nodeset = result->nodesetval;
		nodeval = xmlNodeListGetString (doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
		convert_hexstr_ui64 (iram, size, (char*)nodeval);
		xmlFree(nodeval);
		xmlXPathFreeObject (result);
	} else {
		sim_warning ("tag %s not found", path);
	}

	// cleanup
	xmlCleanupParser();
}


//
bool_t __attribute__((format (printf,2,3)))
get_str_tagvalue_from_xmlfile(char* str, const char *pathfmt, ...)
{
	static char pathstr[1000];
	xmlChar *strval;
	xmlXPathObjectPtr result;
	va_list ap;

	// make path string
	va_start(ap,pathfmt);
	vsnprintf(pathstr,sizeof(pathstr)-1,pathfmt,ap);

	// find node
	result = getnodeset (doc, (xmlChar*) pathstr);

	// get value
	if (result != NULL) {
		strval = xmlNodeListGetString(doc, result->nodesetval->nodeTab[0]->xmlChildrenNode, 1);
		strcpy (str, (char*) strval);
		xmlFree(strval);
		xmlXPathFreeObject (result);
		return TRUE;
	} else {
		sim_warning ("tag %s not found", pathstr);
		return FALSE;
	}

	// cleanup
	xmlCleanupParser();
}


//
bool_t get_ui64_tagvalue_from_xmlfile(char *path, uint64_t *data) {

	xmlChar *strval;
	xmlXPathObjectPtr result;

	// find node
	result = getnodeset (doc, (xmlChar*) path);

	// get value
	if (result != NULL) {
		strval = xmlNodeListGetString(doc, result->nodesetval->nodeTab[0]->xmlChildrenNode, 1);
		*data = strtoull((char*) strval,NULL,16);
		xmlFree(strval);
		xmlXPathFreeObject (result);
		return TRUE;
	} else {
		sim_warning ("tag %s not found", path);
		return FALSE;
	}

	// cleanup
	xmlCleanupParser();
}


//
void xmldoc_load_regs () {

	int sec_num,reg_num;
	uint64_t data;
	char path[100];

	// for all sections
	for(sec_num=0; sec_num<NUMBER_OF_EXESECT; sec_num++) {

		// load fccr registers
		sprintf(path,"//state//section%d//fccr", sec_num);
		if (get_ui64_tagvalue_from_xmlfile(path, &data)) rawwrite_fccr (sec_num,data,~0);

		// load fpr registers
		for(reg_num=0; reg_num<FPR_SIZE; reg_num++) {
			sprintf(path,"//state//section%d//fpr%02d", sec_num, reg_num);
			if (get_ui64_tagvalue_from_xmlfile(path, &data)) rawwrite_fpr (sec_num, reg_num, (fpr_t) {.ui64 = data});
        }

	}

	// load gpr registers
	for(reg_num=0; reg_num<GPR_SIZE; reg_num++) {
		sprintf(path,"//state//gpr%02d", reg_num);
		if (get_ui64_tagvalue_from_xmlfile(path, &data)) rawwrite_gpr(reg_num,data);
    }

	// load address registers
	for(reg_num=0; reg_num<ADDRREG_SIZE; reg_num++) {
		sprintf(path,"//state//an%02d", reg_num);
		if (get_ui64_tagvalue_from_xmlfile(path, &data)) rawwrite_addran(reg_num,data);
		sprintf(path,"//state//nn%02d", reg_num);
		if (get_ui64_tagvalue_from_xmlfile(path, &data)) rawwrite_addrnn(reg_num,data);
		sprintf(path,"//state//mn%02d", reg_num);
		if (get_ui64_tagvalue_from_xmlfile(path, &data)) rawwrite_addrmn(reg_num,data);
    }


	// load control registers
	if (get_ui64_tagvalue_from_xmlfile("//state//pc",      &data)) reg_pc          = data;
	if (get_ui64_tagvalue_from_xmlfile("//state//status",  &data)) reg_status.ui32 = data;
	if (get_ui64_tagvalue_from_xmlfile("//state//control", &data)) reg_control.ui32= data;
	if (get_ui64_tagvalue_from_xmlfile("//state//comm",    &data)) reg_comm        = data;
	if (get_ui64_tagvalue_from_xmlfile("//state//psp",     &data)) {reg_psp        = data; kern.psp_cur=reg_psp;}
	if (get_ui64_tagvalue_from_xmlfile("//state//lc",      &data)) reg_lc          = data;
	if (get_ui64_tagvalue_from_xmlfile("//state//la",      &data)) reg_la.ui32     = data;
	if (get_ui64_tagvalue_from_xmlfile("//state//lsp",     &data)) {reg_lsp        = data; kern.lsp_cur=reg_lsp;}

	//
	xmlCleanupParser();
}


// Check whether a file exist.
bool_t file_exist (char *filename) {

	struct stat statbuf;

	if (stat (filename, &statbuf) == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

