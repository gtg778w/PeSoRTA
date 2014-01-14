#include <stdlib.h>
#include <stdint.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sched.h>

#include "membound.h"

int32_t membound_mainloop(membound_t *membound_p)
{
    int32_t m;
    int32_t *cacheline_array= membound_p->cacheline_array;
    int32_t dword_per_cacheline = membound_p->dword_per_cacheline;
    int32_t graph_index     = membound_p->graph_index;
    int64_t loop_iterations = membound_p->loop_iterations;
    
    m = 1;
    do
    {
        m = cacheline_array[(dword_per_cacheline*m) + graph_index];
    }while(loop_iterations--);
    
    return (int)m;
}

static int32_t get_dword_per_cacheline( void )
{
    int32_t ret = 0;
    long int cacheline_size;
    FILE* filep;
    
    filep = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if(NULL == filep)
    {
        perror( "get_dword_per_cacheline: failed to open sysfs file for "
                "cacheline size");
        ret = -1;
        goto error0;
    }
    
    ret = fscanf(filep, "%li", &cacheline_size);
    if(1 != ret)
    {
        perror("get_dword_per_cacheline: fscanf failed");
        ret = -1;
        goto error1;
    }

    ret = (int32_t)(cacheline_size / sizeof(int32_t));

error1:
    fclose(filep);
error0:
    return ret;
}

void membound_free(membound_t *membound_p)
{
    void *mapped_region = membound_p->mapped_region;
    size_t mapped_region_size   = membound_p->mapped_region_size;
    int fd  = membound_p->fd;
    
    if(NULL != mapped_region)
    {
        munmap(mapped_region, mapped_region_size);
        close(fd);
        *membound_p = (struct membound_s){0};
        membound_p->fd = -1;
        membound_p->graph_index = -1;
    }
}

int membound_init(  membound_t  *membound_p,
                    char    *datafile_name,
                    int32_t graph_index,
                    int64_t loop_iterations)
{
    int32_t dword_per_cacheline;
    int fd;
    
    int32_t *cacheline0;
    size_t read_ret;
    
    size_t  mapped_region_size;
    void    *mapped_region;
    
    /*Get the number of int32_t's in a cacheline.*/
    dword_per_cacheline = get_dword_per_cacheline();
    if(-1 == dword_per_cacheline)
    {
        fprintf(stderr, "membound_init: get_dword_per_cacheline failed!\n");
        goto error0;
    }
    
    /*Open the input file.*/
    fd = open(datafile_name, O_RDONLY);
    if(-1 == fd)
    {
        fprintf(stderr, "membound_init: Failed to open the input file: \"%s\". ", 
                        datafile_name);
        perror("open failed");
        goto error0;
    }
    
    /*Allocate a single cache-line-sized space on the stack.*/
    cacheline0 = (int32_t*)malloc(sizeof(int32_t) * dword_per_cacheline);
    if(NULL == cacheline0)
    {
        perror( "membound_init: Failed to allocate a single cacheline-sized "
                "block of memory");
        goto error1;
    }
    
    /*Read the data-file header.*/
    read_ret =  read(fd, cacheline0, (sizeof(int32_t) * dword_per_cacheline)); 
    if(read_ret != (sizeof(int32_t) * dword_per_cacheline))
    {
        if(-1 == read_ret)
        {
            perror( "membound_init: Failed to read data file header. read"
                    " failed");
        }
        else
        {
            fprintf(stderr, "membound_init: read returned fewer bytes "
                            "than requested.\n");
        }
        goto error2;
    }
    
    /*Determine the size of the desired memory mapped region from the data-file 
    header*/
    mapped_region_size = cacheline0[graph_index];
    
    /*Map the data file into memory*/
    mapped_region = mmap(   NULL, mapped_region_size, 
                            PROT_READ, 
                            (MAP_PRIVATE | MAP_LOCKED | MAP_POPULATE),
                            fd, (off_t)0);
    if(MAP_FAILED == mapped_region)
    {
        perror("membound_init: mmap failed!");
        goto error2;
    }
    
    /*Free temporarily allocated memory*/
    free(cacheline0);
    
    /*Set the necessary entries in the membound_t structure*/
    membound_p->fd = fd;
    membound_p->mapped_region = mapped_region;
    membound_p->mapped_region_size = mapped_region_size;
    membound_p->cacheline_array = (int32_t*)mapped_region;
    membound_p->graph_index = graph_index;
    membound_p->dword_per_cacheline = dword_per_cacheline;
    membound_p->loop_iterations = loop_iterations;
    
    return 0;

    /*Undo all statefull operation in reverse order*/
error2:
    free(cacheline0);
error1:
    close(fd);
error0:
    *membound_p = (struct membound_s){0};
    membound_p->fd = -1;
    membound_p->graph_index = -1;
    return -1;
}

