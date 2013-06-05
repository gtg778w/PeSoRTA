#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

/*appends */
char* PeSoRTA_strappend(char* dest, char *src)
{
    char* ret = NULL;

    int dest_len;
    int src_len;
    int ret_len;
    
    
    if(NULL == dest)
    {
        dest_len = 0;
    }
    else
    {
        dest_len = strlen(dest);
    }
    
    src_len = strlen(src) ;
    ret_len = dest_len + src_len + 1;
    
    ret = (char*)realloc(dest, ret_len*sizeof(char));
    if(NULL != ret)
    {
        strcpy(&(ret[dest_len]), src);
    }
    
    return ret;
}

char* PeSoRTA_strtriml(char* src)
{
    char* ret = src-1;
    char next;
    int keepgoing = 1;
    
    do
    {
        ret++;
        next = *ret;
        keepgoing = keepgoing && (next != '\0');
        keepgoing = keepgoing && isspace((int)next);
    }while(keepgoing);
    
    return ret;
}

void PeSoRTA_strtrimr(char* src)
{
    char* ret = src + strlen(src);
    char next;
    int keepgoing = 1;
    
    do
    {
        ret--;
        next = *ret;
        keepgoing = keepgoing && (ret != src);
        keepgoing = keepgoing && isspace((int)next);
    }while(keepgoing);
    
    if((ret == src) && isspace((int)next))
    {
        *ret = '\0';
    }
    else
    {
        *(ret+1) = '\0';
    }
}

char* PeSoRTA_stralloccpy(char* src)
{
    char *ret = NULL;
    int src_len;
    
    src_len = strlen(src);
    ret = malloc(src_len + 1);
    if(NULL != ret)
    {
        strcpy(ret, src);
    }
    
    return ret;
}

char* PeSoRTA_strallocprintf(const char *format, ...)
{
    char *ret;
    char buffer[2];
    va_list ap;
    int size;
    
    va_start(ap, format);
    size = vsnprintf(buffer, 2, format, ap);
    ret = (char*)malloc(sizeof(char)*size);
    if(NULL == ret)
    {
        goto exit0;
    }
    va_start(ap, format);
    size = vsnprintf(ret, size, format, ap);
exit0:
    return ret;
}

char* PeSoRTA_strappendprintf(char * dest, const char *format, ...)
{
    va_list ap;

    char buffer[2];
    int size;

    int dest_len;

    char *ret;
        
    dest_len = strlen(dest);
    
    va_start(ap, format);
    size = vsnprintf(buffer, 2, format, ap);

    ret = (char*)realloc(dest, sizeof(char)*(size+dest_len+1));
    if(NULL == ret)
    {
        goto exit0;
    }

    va_start(ap, format);
    size = vsnprintf(&(ret[dest_len]), size, format, ap);
exit0:
    return ret;
}

int PeSoRTA_strappendprintf_safe(char **dest_p, const char *format, ...)
{
    int ret = 0;
    
    va_list ap;

    char buffer[2];
    int size;

    char *src;
    int src_len;

    char *dest;
    
    src = *dest_p;
    src_len = (src != NULL)? strlen(src) : 0;
    
    va_start(ap, format);
    size = vsnprintf(buffer, 2, format, ap);

    dest = (char*)realloc(src, sizeof(char)*(size+src_len+1));
    if(NULL == dest)
    {
        ret = -1;
        goto exit0;
    }

    va_start(ap, format);
    size = vsnprintf(&(dest[src_len]), (size+1), format, ap);
    
    *dest_p = dest;
exit0:
    return ret;
}

#ifdef TEST_PESORTA_STRING

/*
compile with the following:
 gcc -Wall -DTEST_PESORTA_STRING PeSoRTA_string.c -o test_PeSoRTA_string
*/

#include <stdio.h>

int main(void)
{
    int ret;

    char *main_string = "Main String";
    char *lspace = " \n\t   Main String";
    char *rspace = "Main String \n\t   ";
    char *allspace = " \n\t   ";
    char *nullstring = "";
    char *new_string, *temp_string;
    
    printf("\n*** TEST_PESORTA_STRING ***\n");
    
    /********************************************************/
    /****                    alloccpy                    ****/
    /********************************************************/
    printf("\ntesting \"PeSoRTA_stralloccpy\":\n");
    
    printf("\t1)\toriginal string: \"%s\"\n", main_string);
    new_string = PeSoRTA_stralloccpy(main_string);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t  \tcopied string: \"%s\"\n", new_string);
    free(new_string);
    
    printf("\t2)\toriginal string: \"%s\"\n", lspace);
    new_string = PeSoRTA_stralloccpy(lspace);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t  \tcopied string: \"%s\"\n", new_string);    
    free(new_string);
    
    printf("\t3)\toriginal string: \"%s\"\n", rspace);
    new_string = PeSoRTA_stralloccpy(rspace);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t  \tcopied string: \"%s\"\n", new_string);    
    free(new_string);
    
    printf("\t4)\toriginal string: \"%s\"\n", allspace);
    new_string = PeSoRTA_stralloccpy(allspace);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;    
    }
    printf("\t  \tcopied string: \"%s\"\n", new_string);
    free(new_string);
    
    printf("\t5)\toriginal string: \"%s\"\n", nullstring);
    new_string = PeSoRTA_stralloccpy(nullstring);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;    
    }
    printf("\t  \tcopied string: \"%s\"\n", new_string);
    free(new_string);
    
    /********************************************************/
    /********************************************************/
    
    /********************************************************/
    /****                    strtriml                    ****/
    /********************************************************/
    printf("\ntesting \"PeSoRTA_strtriml\":\n");
    
    printf("\t1)\toriginal string: \"%s\"\n", main_string);
    new_string = PeSoRTA_strtriml(main_string);
    printf("\t  \tltrimed string: \"%s\"\n", new_string);
    
    printf("\t2)\toriginal string: \"%s\"\n", lspace);
    new_string = PeSoRTA_strtriml(lspace);
    printf("\t  \tltrimed string: \"%s\"\n", new_string);
    
    printf("\t3)\toriginal string: \"%s\"\n", rspace);
    new_string = PeSoRTA_strtriml(rspace);
    printf("\t  \tltrimed string: \"%s\"\n", new_string);
    
    printf("\t4)\toriginal string: \"%s\"\n", allspace);
    new_string = PeSoRTA_strtriml(allspace);
    printf("\t  \tltrimed string: \"%s\"\n", new_string);
    
    printf("\t5)\toriginal string: \"%s\"\n", nullstring);
    new_string = PeSoRTA_strtriml(nullstring);
    printf("\t  \tltrimed string: \"%s\"\n", new_string);
    /********************************************************/
    /********************************************************/    
    
    /********************************************************/
    /****                    strtrimr                    ****/
    /********************************************************/
    
    printf("\ntesting \"PeSoRTA_strtrimr\":\n");
    
    new_string = PeSoRTA_stralloccpy(main_string);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t1)\tcopied string: \"%s\"\n", new_string);
    PeSoRTA_strtrimr(new_string);
    printf("\t  \trtrimed string: \"%s\"\n", new_string);
    
    new_string = PeSoRTA_stralloccpy(lspace);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t2)\tcopied string: \"%s\"\n", new_string);
    PeSoRTA_strtrimr(new_string);
    printf("\t  \trtrimed string: \"%s\"\n", new_string);
    
    new_string = PeSoRTA_stralloccpy(rspace);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t3)\tcopied string: \"%s\"\n", new_string);
    PeSoRTA_strtrimr(new_string);
    printf("\t  \trtrimed string: \"%s\"\n", new_string);
    
    new_string = PeSoRTA_stralloccpy(allspace);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t4)\tcopied string: \"%s\"\n", new_string);
    PeSoRTA_strtrimr(new_string);
    printf("\t  \trtrimed string: \"%s\"\n", new_string);
    
    new_string = PeSoRTA_stralloccpy(nullstring);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\t5)\tcopied string: \"%s\"\n", new_string);
    PeSoRTA_strtrimr(new_string);
    printf("\t  \trtrimed string: \"%s\"\n", new_string);
    
    /********************************************************/
    /********************************************************/

    /********************************************************/
    /****                   strappend                    ****/
    /********************************************************/
    
    printf("\ntesting \"PeSoRTA_strappend\":\n");
    
    printf("\toriginal string: \"%s\"\n", main_string);
    new_string = PeSoRTA_stralloccpy(main_string);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\tcopied string: \"%s\"\n", new_string);
    printf("\tappendable string: \"%s\"\n", " appended!");
   
    temp_string = PeSoRTA_strappend(new_string, " appended!");
    if(NULL == temp_string)
    {
        printf("\t\"PeSoRTA_strappend\" returned NULL!\n");
        free(new_string);
        return -1;
    }
    else
    {
        new_string = temp_string;
    }

    printf("\tappended string: \"%s\"\n", new_string);    
    
    free(new_string);
    
    /********************************************************/
    /********************************************************/
    
    /********************************************************/
    /****                strallocprintf                    **/
    /********************************************************/
    printf("\ntesting \"PeSoRTA_strallocprintf\":\n");
        
    new_string = PeSoRTA_strallocprintf("%i) string = %s, %f\n", 
        2, "string", 1.68);
    printf("\t%s\n", new_string);
    free(new_string);
    
    /********************************************************/
    /********************************************************/

    /********************************************************/
    /****               strappendprintf                    **/
    /********************************************************/
    printf("\ntesting \"PeSoRTA_strappendprintf\":\n");
        
    printf("\toriginal string: \"%s\"\n", main_string);
    new_string = PeSoRTA_stralloccpy(main_string);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\tcopied string: \"%s\"\n", new_string);

    new_string = PeSoRTA_strappendprintf(new_string,
        "%i) string = %s, %f", 
        2, "string", 1.68);
    printf("\tappended string: \"%s\"\n", new_string);
    free(new_string);
    
    /********************************************************/
    /********************************************************/

    /********************************************************/
    /****            strappendprintf_safe                  **/
    /********************************************************/
    printf("\ntesting \"PeSoRTA_strappendprintf_safe\":\n");
        
    printf("\toriginal string: \"%s\"\n", main_string);
    new_string = PeSoRTA_stralloccpy(main_string);
    if(NULL == new_string)
    {
        printf("\t\"PeSoRTA_stralloccpy\" returned NULL!\n");
        return -1;
    }
    printf("\tcopied string: \"%s\"\n", new_string);

    ret = PeSoRTA_strappendprintf_safe(&(new_string),
        "%i) string = %s, %f", 
        2, "string", 1.68);
    if(ret == 0)
    {
        printf("\tappended string: \"%s\"\n", new_string);
        free(new_string);
    }
    else
    {
        printf("\t\"PeSoRTA_strappendprintf_safe\" "
                "returned %i!\n", ret);
        free(new_string);
        return -1;
    }
    
    /********************************************************/
    /********************************************************/

    printf("\n***       END TEST      ***\n\n");
    return 0;

}

#endif

