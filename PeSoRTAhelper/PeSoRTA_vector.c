#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

int PeSoRTA_vector_writeCSVF(char* fileName, int32_t input_size, double* data)
{
    FILE* filep;
    int ret = 0;
    int32_t i = 0;
    
    filep = fopen(fileName, "w");
    if(NULL == filep)
    {
        perror("ERROR: PeSoRTA_vector_writeCSVF) fopen failed ");
        ret = -1;
        goto exit0;
    }
    
    for(i = 0; i < input_size; i++)
    {
        ret = fprintf(filep, "%lf\n", data[i]);
        if(ret < 0)
        {
            perror("ERROR: PeSoRTA_vector_writeCSVF) fprintf failed ");
            ret = -1;
            goto exit1;
        }
    }

exit1:
    fclose(filep);
exit0:
    return ret;
}

int PeSoRTA_vector_readCSVF(char* fileName, int32_t *input_size_p, double* *data_p)
{
    FILE* filep;

	double* input_data = NULL;
	int32_t page_size;
	int32_t buffer_size = 0;
	int32_t array_size = 0;
    int32_t i = 0;
	
	char *lineBuffer = NULL;
	size_t lineBuffer_size = 0;
	ssize_t getline_ret = 0;
	
	int ret = 0;

    page_size = getpagesize();


    filep = fopen(fileName, "r");
    if(NULL == filep)
    {
        perror("ERROR: PeSoRTA_vector_readCSVF) fopen failed");
        goto exit0;
    }

    do
    {
        if((array_size - 1) <= i)
        {
            buffer_size += page_size;
            array_size = buffer_size/sizeof(double);
	        input_data = (double*)realloc(input_data, buffer_size);
	        if(NULL == input_data)
	        {
	            perror("ERROR: PeSoRTA_vector_readCSVF) malloc failed");
	            goto exit3;
	        }       
        }
        
        getline_ret = getline(&lineBuffer, &lineBuffer_size, filep);
        if(getline_ret > 0)
        {
            /*lineBuffer_size should be > 0*/
            lineBuffer[lineBuffer_size - 1] = '\0';
            errno = 0;
            ret = sscanf(lineBuffer, "%lf", &(input_data[i]));
            if((ret != 1))
            {
                if(errno != 0)
                {
                    perror("ERROR: PeSoRTA_vector_readCSVF) sscanf failed");
                    goto exit3;
                }
                else
                {
                    input_data[i] = 0.0;
                }
            }
            i++;
        }
        
    }while(getline_ret >= 0);

    if(feof(filep) != 0)
    {
        if(lineBuffer_size > 0)
        {
            free(lineBuffer);
        }

        fclose(filep);

        *input_size_p = i;
        *data_p = input_data;
        return 0;
    }

/*exit4:*/
    /*else*/
    perror("ERROR: PeSoRTA_vector_readCSVF) getline failed");
exit3:
    if(lineBuffer_size > 0)
    {
        free(lineBuffer);
    }
/*exit2:*/
    free(input_data);
/*exit1:*/
    fclose(filep);
exit0:
    return -1;
}

#ifdef TEST_Vector

int main(void)
{
    int ret;
    double vector_out[1000];
    double *vector_in;
    int32_t vector_in_size;
    int32_t i;
    
    for(i = 0; i < 1000; i++)
    {
        vector_out[i] = (double)i;
    }
    
    ret = PeSoRTA_vector_writeCSVF("TEST_Vector.csv", 1000, vector_out);
    if(ret < 0)
    {
        fprintf(stderr, "PeSoRTA_vector_writeCSVF failed in main!\n");
        return -1;
    }
    
    ret = PeSoRTA_vector_readCSVF("TEST_Vector.csv", &vector_in_size, &vector_in);
    if(ret < 0)
    {
        fprintf(stderr, "PeSoRTA_vector_readCSVF failed in main!\n");
        return -1;
    }
    
    if(1000 != vector_in_size)
    {
        printf("TEST_Vector: FAIL\n");
        printf("\t written vector of length %i\n", 1000);
        printf("\t read vector of length %i\n\n", vector_in_size);
    }
    
    for(i = 0; i < 1000; i++)
    {
        if(vector_in[i] != vector_out[i])
        {
            printf("TEST_Vector: FAIL\n");
            printf("\t mismatch\n");
            printf("\t vector_out[%i] = %lf\n", i, vector_out[i]);
            printf("\t vector_out[%i] = %lf\n\n", i, vector_in[i]);
            free(vector_in);
            return -1;
        }
    }
    
    printf("TEST_Vector: SUCCESS!\n\n");
    free(vector_in);
    return 0;
}

#endif

