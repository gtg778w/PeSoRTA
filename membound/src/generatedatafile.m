
    arg_list = argv();    
    argc = length(arg_list);
    if argc > 0
        if argc > 1
            error('This script only supports upto a single argument');
        end
        outfile_name = arg_list{1};
    else
        outfile_name = './membound_input.dat';
    end

    sys_cache_dir   = '/sys/devices/system/cpu/cpu0/cache/';
    cache_dir_list  = readdir(sys_cache_dir);
    cache_dir_count = length(cache_dir_list);

    cacheline_size = [];
    cache_size = [];
    cache_sets = [];
    cache_associativity = [];
    for d_i = 1:cache_dir_count
    
        cache_dir = sprintf('%s/%s/', sys_cache_dir, cache_dir_list{d_i});

        %check if it is a directory
        if ~ isdir(cache_dir)
            %its not a directory. move onto the next entry
            continue;
        end
        
        %check if it is the current directory or parent directory
        if( strcmp(cache_dir_list{d_i}, '.') || strcmp(cache_dir_list{d_i}, '..'))
            %its the '.' or '..' directory. move on to the next rnt
            continue;
        end        
        
        cache_type_file = sprintf('%s/type', cache_dir);
        fid = fopen(cache_type_file);
        [val, count] = fread(fid, inf, 'char');
        val = char(val)';
        if( strncmp(val, 'Instruction', 11) )
            %only interested in caches that store data like data caches and unified caches
            continue;
        end
        
        cache_line_file = sprintf('%s/coherency_line_size', cache_dir);
        cache_size_file = sprintf('%s/size', cache_dir);
        cache_sets_file = sprintf('%s/number_of_sets', cache_dir);
        cache_associativity_file = sprintf('%s/ways_of_associativity', cache_dir);
        
        local_cacheline_size=   csvread(cache_line_file);
        local_cache_size    =   csvread(cache_size_file) * 1024;
        local_cache_sets    =   csvread(cache_sets_file);
        local_cache_associtivity = csvread(cache_associativity_file);
        
        cacheline_size  = [cacheline_size, local_cacheline_size];
        cache_size      = [cache_size, local_cache_size];
        cache_sets      = [cache_sets, local_cache_sets];
        cache_associativity = [cache_associativity, local_cache_associtivity];
    end

    if any(cacheline_size ~= cacheline_size(1))
        error('cache line sizes are not same same accross the caches!');
    end
    cacheline_size = cacheline_size(1);

    %sort the caches by size
    [cache_size, sorted_i] = sort(cache_size);
    cache_sets = cache_sets(sorted_i);
    cache_associativity = cache_associativity(sorted_i);

    cache_count = min((length(cache_size)+2), cacheline_size / 4);
    
    cache_start_index   = (length(cache_size)+2) - cache_count + 1;
    cache_sets          = cache_sets(cache_start_index:end);
    cache_associativity = cache_associativity(cache_start_index:end);
    cache_size          = cache_size(cache_start_index:end);
    
    cache_sets      = [1, cache_sets, cache_sets(end)*8];
    cache_associativity = [1, cache_associativity, cache_associativity(end), cache_associativity(end)];
    cache_size      = [cacheline_size, cache_size, cache_size(end)*8];
    cache_lines     = cache_size / cacheline_size;
    
    indices = zeros([cache_lines(end), (cacheline_size / 4)]);
    
    c_i = 1;
    indices(2, c_i) = 1;
    indices(1, c_i) = cache_size(1);
    
    for c_i = 2 : cache_count;
        if cache_lines(c_i) > 1
            [dummy, sequence] = sort(rand([cache_lines(c_i)-2, 1]));
            sequence = sequence + 1;
        else
            sequence = [];
        end
        indices([1; sequence], c_i) = [sequence; 1];
        
        m = 1;
        c = 0;
        do
            m = indices(m, c_i);
            c = c + 1;
        until(m == 1);
        
        if( c ~= (cache_lines(c_i)-1) )
            error('The algorithm failed!');
        end
        
        indices([1; sequence]+1, c_i) = [sequence; 1];
        indices(1, c_i) = cache_size(c_i);
    end
    
    indices = (indices')(1:end);
    
    [fid, msg] = fopen(outfile_name, 'w');
    if -1 == fid
        error("failed to open \"%s\": %s", outfile_name, msg);
    end
    fwrite(fid, indices, 'int32');
    fclose(fid);
    
