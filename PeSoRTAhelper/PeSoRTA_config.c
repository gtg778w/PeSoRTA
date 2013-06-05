#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PeSoRTA_helper.h"

int PeSoRTA_getconfigopt(FILE* filep, char *optstring, int *opt_p, char **optarg_p)
{
    int ret = -1;
    
    char *line = NULL;
    size_t line_length = 0;

    char *trimmed_line;
    size_t trimmed_length = 0;    
    
    char *opt_chrp = NULL;
    int opt = -1;
    
    /*loop until a non-whitespace line is found*/
    do
    {
        line_length = getline(&line, &line_length, filep);
        if(line_length == -1)
        {
            *opt_p = -1;
            *optarg_p = NULL;
            ret = -1;
            goto exit0;
        }
    
        trimmed_line = PeSoRTA_strtriml(line);
        trimmed_length = strlen(trimmed_line);
    }while(0 == trimmed_length);

    if('-' != trimmed_line[0])
    {
        *opt_p = -1;
        *optarg_p = line;
        ret = 0;
        goto exit0;
    }
    else
    {
        opt = (int)trimmed_line[1];
    }
    
    opt_chrp = strchr(optstring, opt);
    if((NULL == opt_chrp) || (':' == opt))
    {
        *opt_p = -1;
        *optarg_p = line;
        ret = 0;
        goto exit0;        
    }

    *opt_p = opt;
    *optarg_p = NULL;    
    if(':' == opt_chrp[1])
    {
        trimmed_line = PeSoRTA_strtriml(&trimmed_line[2]);
        trimmed_length = strlen(trimmed_line);
        if(trimmed_length > 0)
        {
            *optarg_p = PeSoRTA_stralloccpy(trimmed_line);
            if(NULL == *optarg_p)
            {
                ret = -1;
            }
            else
            {
                ret = 0;
                PeSoRTA_strtrimr(*optarg_p);
            }
        }
    }

    free(line);
exit0:
    return ret;
}

#ifdef TEST_PESORTA_CONFIG

/*
compile with the following:
 gcc -Wall -DTEST_PESORTA_STRING PeSoRTA_string.c -o test_PeSoRTA_string
*/

int main(int argc, char **argv)
{
    FILE* file;
    int ret = 0;
    
    char *optstring = "abcdA:B:C:D:";
    int opt;
    char *optarg;
        
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <config file>!\n", argv[0]);
        ret = -1;
        goto exit0;
    }

    file = fopen(argv[1], "r");
    if(NULL == file)
    {
        perror("fopen failed in main");
        ret = -1;
        goto exit0;
    }

    while(!feof(file))
    {
        ret = PeSoRTA_getconfigopt(file, optstring, &opt, &optarg);
        if(ret == -1)
        {
            perror("PeSoRTA_getconfigopt failed in main");
            goto exit1;
        }
        
        if(opt == -1)
        {
            printf("\tbad line: ");
        }
        else
        {
            printf("\toption (%c), argument: ", opt);
        }
        
        if(NULL != optarg)
        {
            printf("\"%s\"\n", optarg);
            free(optarg);
            optarg = NULL;
        }
        else
        {
            printf("\"\"\n");
        }
    }

exit1:
    fclose(file);
exit0:
    return ret;
}

#endif

