#ifndef PeSoRTA_HELPER_INCLUDE
#define PeSoRTA_HELPER_INCLUDE

#include <stdio.h>

/*** PeSoRTA_string ***/
char* PeSoRTA_strappend(char* dest, char *src);
char* PeSoRTA_strtriml(char* src);
void  PeSoRTA_strtrimr(char* src);
char* PeSoRTA_stralloccpy(char* src);
char* PeSoRTA_strallocprintf(const char *format, ...);
char* PeSoRTA_strappendprintf(char * dest, const char *format, ...);
int PeSoRTA_strappendprintf_safe(char **dest_p, const char *format, ...);

/*** PeSoRTA_config ***/
int   PeSoRTA_getconfigopt(FILE* filep, char *optstring, int *opt_p, char **optarg_p);

/*** PeSoRTA_vector ***/
int PeSoRTA_vector_writeCSVF(char* fileName, int32_t input_size, double* data);
int PeSoRTA_vector_readCSVF(char* fileName, int32_t *input_size_p, double* *data_p);

#endif
