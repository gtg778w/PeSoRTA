#ifndef PeSoRTA_INCLUDE
#define PeSoRTA_INCLUDE

    char *workload_name(void);
    /*
        - a simple function that returns the name and any description of the workload as a static string
    */

    int workload_init(char *configfile, void **state_p, long *job_count_p);
    /*
        - initialize the workload based on the contents of the configuration file.
        - ideally all memory that needs to be allocated should be allocated at this stage
        - the main program may mlock all memory after this point 
    */
    
    int perform_job(void *state);
    /*
        - perform the next job of the workload
        - this function should ideally be pure computation
        - this function or any sucessively called function should not (ideally):
            - allocate extra memory including too much stack memory
            - perform I/O operations, especially if they can block
            - perform synchronization operations if they can block
            - sleep
        - mainly, this function should not do anything that can cause the task to block.
    */
    
    int workload_uninit(void *state);
    /*
        - uninitialize the workload
        - deallocate any allocated memory
        - close any files
    */

#endif
