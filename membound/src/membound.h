#ifndef MEMBOUND_INCLUDE
#define MEMBOUND_INCLUDE

    typedef struct membound_s
    {
        int     fd;
        void*   mapped_region;
        size_t  mapped_region_size;
        int32_t *cacheline_array;
        int32_t graph_index;
        int32_t dword_per_cacheline;
        int64_t loop_iterations;
    } membound_t;

    int membound_init(  membound_t  *membound_p,
                        char    *datafile_name,
                        int32_t graph_index,
                        int64_t loop_iterations);
    void membound_free(membound_t *membound_p);
    int32_t membound_mainloop(membound_t *membound_p);
    

#endif


