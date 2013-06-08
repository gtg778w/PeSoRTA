#! /bin/csh -x

    #The root directory of all the PeSoRTA benchmarks
    set PeSoRTA_root="../"

    #The name of the application being used for the tests
    set application="ffmpeg"

    #The directory of the application containing the relevant config directory 
    #and data directory
    set appdir="${PeSoRTA_root}/${application}"

    
    set bindir="~/Desktop/bin"
    
    set bin="${bindir}/${application}_timing"

    set results_root="${PeSoRTA_root}/data/timing/${application}"

    mkdir -p ${results_root}

    set configs=("dec.deadline.flv" "dec.deadline.mp4" "dec.deadline.webm" "dec.factory.flv" "dec.factory.mp4" "dec.factory.webm" "dec.highway.flv" "dec.highway.mp4" "dec.highway.webm" "dec.sintel.flv" "dec.sintel.mp4" "dec.sintel.webm" "enc.bridge_close.y4m.flv" "enc.bridge_close.y4m.mp4" "enc.bridge_close.y4m.webm" "enc.deadline.y4m.flv" "enc.deadline.y4m.mp4" "enc.deadline.y4m.webm" "enc.highway.y4m.flv" "enc.highway.y4m.mp4" "enc.highway.y4m.webm" "enc.mad900.y4m.flv" "enc.mad900.y4m.mp4" "enc.mad900.y4m.webm")

    set repetitions=20

    foreach config (${configs})
        set config_file="${appdir}/config/${config}.config"

        set resultdir="${results_root}/${config}"
    
        mkdir -p ${resultdir}
    
        set csv_prefix="timing.${application}.${config}"

        echo "********** ********** ********** **********"
    
        echo
        echo "application="${application}
        echo "config="${config}
        echo

        #loop through the repetitions of the experiment
        foreach r (`seq 1 1 ${repetitions}`)
            set csv_name="${resultdir}/${csv_prefix}.${r}.csv"
        
            echo 
            echo ${csv_name}":"            
            echo "    running:      ${bin}"
            echo "    exp. dir:     ${appdir}"
            echo "    config file:  ${config_file}"
            echo "    results file: ${csv_name}"
            echo
            #run the experiment
            
            ${bin} -r -R ${appdir} -C ${config_file} -L ${csv_name}
            
        end
        
        echo "********** ********** ********** **********"
    end

