#!/bin/bash
export LC_ALL=C.UTF-8

# It is assumed that the script is run from the repository's home folder.

#####################################################################################
# Constants:

# Output files
FILE_OUT_MEASURE="measure.csv"
FILE_OUT="no-measure.csv"

# Exec settings
MEDUSA_EXEC="./MEDUSA"
SLIQSIM_EXEC="../SliQSim-no-out/SliQSim"
SLIQSIM_EXEC_MSR="../SliQSim/SliQSim"

MEDUSA_BASE_OPT="-i"
MEDUSA_SYMB_OPT="-s"
MEDUSA_MEASURE_OPT="-m"
SLIQSIM_BASE_OPT="--print_info"
SLIQSIM_OPT="--type 1"
SLIQSIM_MEASURE_OPT="--type 0 --shots 1024"

# Benchmark directories
BENCH_NO_MEASURE="./benchmarks/no-measure/"
BENCH_MEASURE="./benchmarks/measure/"

# Measurement settings
REPS=1
TIMEOUT="timeout 1h"   # for no timeout: ""

# Output settings
SEP=","
NO_RUN="---"
ERROR="Error"
TO="TO"

#####################################################################################
# Functions:

# Runs a single circuit with the MEDUSA executable with the specified simulation type. Saves the results in the given variables.
sim_file_medusa() {
    local symb_opt="$1"
    local var_time_name="$2"
    local var_mem_name="$3"
    local var_fail_name="$4"
    
    local fail="${!var_fail_name}"
    local time_sum=0.0
    local mem_sum=0.0
    local output
    local time_cur
    local mem_cur

    if [[ $fail = false ]]; then
        for i in $(eval echo "{1..$REPS}"); do
            output=$($TIMEOUT $MEDUSA_EXEC $medusa_run_opt $symb_opt <$file 2>&1 | grep -v "^ *'")
            if [[ -z $output ]]; then
                time_avg=$TO
                mem_avg=$TO
                fail=true
                break;
            elif grep -q "Time" <<< "$output"; then
                time_cur=$(echo "$output" | grep -oP '(?<=Time=)[0-9.eE+-]+' | awk '{printf "%.4f", $1}')
                mem_cur=$(echo "$output" | grep -oP '(?<=Peak Memory Usage=)[0-9]+' | awk '{printf "%.2f", $1 / 1024}')

                time_sum=$(echo "scale=4; $time_sum + $time_cur" | bc)
                mem_sum=$(echo "scale=2; $mem_sum + $mem_cur" | bc)
            else
                time_avg=$ERROR
                mem_avg=$ERROR
                fail=true
                break;
            fi
        done

        if [[ $fail = false ]]; then
            time_avg=$(echo "scale=4; $time_sum / $REPS.0" | bc)
            mem_avg=$(echo "scale=2; $mem_sum / $REPS.0" | bc)
        fi
    else
        time_avg=$NO_RUN
        mem_avg=$NO_RUN
    fi

    eval "$var_time_name"=$time_avg
    eval "$var_mem_name"=$mem_avg
    eval "$var_fail_name"=$fail
}

# Runs a single circuit with the SliQSim executable.
sim_file_sliqsim() {
    sliqsim_time_sum=0.0
    sliqsim_mem_sum=0.0

    if [[ $sliqsim_fail = false ]]; then
        for i in $(eval echo "{1..$REPS}"); do
            sliqsim_output=$($TIMEOUT $sliqsim_run_exec --sim_qasm $file $sliqsim_run_opt 2>&1 | \
                             grep -v '^{ "counts"')
            if [[ -z $sliqsim_output ]]; then
                sliqsim_time_avg=$TO
                sliqsim_mem_avg=$TO
                sliqsim_fail=true
                break;
            elif grep -q "Runtime" <<< "$sliqsim_output"; then
                sliqsim_time_cur=$(echo "$sliqsim_output" | grep -oP '(?<=Runtime: )[0-9.]+' | awk '{printf "%.4f", $1}')
                sliqsim_mem_cur=$(echo "$sliqsim_output" | grep -oP '(?<=Peak memory usage: )[0-9]+' | awk '{printf "%.2f", $1 / (1024 * 1024)}')

                sliqsim_time_sum=$(echo "scale=4; $sliqsim_time_sum + $sliqsim_time_cur" | bc)
                sliqsim_mem_sum=$(echo "scale=2; $sliqsim_mem_sum + $sliqsim_mem_cur" | bc)
            else
                sliqsim_time_avg=$ERROR
                sliqsim_mem_avg=$ERROR
                sliqsim_fail=true
                break;
            fi
        done

        if [[ $sliqsim_fail = false ]]; then
            sliqsim_time_avg=$(echo "scale=4; $sliqsim_time_sum / $REPS.0" | bc)
            sliqsim_mem_avg=$(echo "scale=2; $sliqsim_mem_sum / $REPS.0" | bc)
        fi
    else
        sliqsim_time_avg=$NO_RUN
        sliqsim_mem_avg=$NO_RUN
    fi
}

# Expects the file path to the test as the first argument and if it is symbolic circuit as the second argument
run_benchmark_file() {
    local file=$1
    local is_loop=$2

    benchmark_name=$(echo "$file" | cut -d'/' -f4-)

    sim_file_medusa $MEDUSA_SYMB_OPT "medusa_symb_time_avg" "medusa_symb_mem_avg" "medusa_symb_fail"
    # Change to file without loops for normal simulation if in loop directory
    if [[ $is_loop = true ]]; then
        directory=$(dirname "$file")
        filename=$(basename "$file")
        new_filename="NL_$filename"

        if [ -f "$directory/$new_filename" ]; then
            file="$directory/$new_filename"
        elif [[ $sliqsim_fail = false ]]; then
            # SliQSim won't parse the circuit
            echo "Missing file $directory/$new_filename" >&2
        fi
    fi
    sim_file_medusa "" "medusa_time_avg" "medusa_mem_avg" "medusa_fail"
    sim_file_sliqsim

    printf "%s$SEP%s$SEP%s$SEP%s$SEP%s$SEP%s$SEP%s\n" "${benchmark_name%.qasm}" "$medusa_symb_time_avg" "$medusa_symb_mem_avg" \
           "$medusa_time_avg" "$medusa_mem_avg" "$sliqsim_time_avg" "$sliqsim_mem_avg" \
           >> "$file_out"
}

run_benchmarks() {
    # Init
    if [[ $is_measure = true ]]; then
        benchmarks_fd=$BENCH_MEASURE
        medusa_run_opt="$MEDUSA_BASE_OPT $MEDUSA_MEASURE_OPT"
        sliqsim_run_opt="$SLIQSIM_BASE_OPT $SLIQSIM_MEASURE_OPT"
        sliqsim_run_exec=$SLIQSIM_EXEC_MSR
        file_out=$FILE_OUT_MEASURE
    else
        benchmarks_fd=$BENCH_NO_MEASURE
        medusa_run_opt=$MEDUSA_BASE_OPT
        sliqsim_run_opt="$SLIQSIM_BASE_OPT $SLIQSIM_OPT"
        sliqsim_run_exec=$SLIQSIM_EXEC
        file_out=$FILE_OUT
    fi

    printf "%s$SEP%s$SEP%s$SEP%s$SEP%s$SEP%s$SEP%s\n" "Circuit" "MEDUSA symbolic t" "MEDUSA symbolic mem" "MEDUSA t" \
           "MEDUSA mem" "SliQSim t" "SliQSim mem" \
           >> "$file_out"

    for folder in "$benchmarks_fd"*/; do
        folder_name=$(basename "$folder")

        # Initialize the flags for crashes and exceeding the timeout
        medusa_symb_fail=false
        medusa_fail=false
        sliqsim_fail=false

        if [[ ! "$folder_name" =~ ^LP- ]]; then
            # Non loop circuits
            for file in "$folder"*.qasm; do
                medusa_symb_fail=true # So symbolic simulation is not run
                if [ -f "$file" ]; then
                    run_benchmark_file $file false
                fi
                # Random and RevLib - nonlinear progression in simulation time -> wait for TO on every benchmark (restart flags)
                if [[ ( "$folder_name" = "Random" ) || ( "$folder_name" = "RevLib" ) || ( "$folder_name" = "Feynman" ) ]]; then
                    medusa_symb_fail=false
                    medusa_fail=false
                    sliqsim_fail=false
                fi
                if [[ ( $medusa_symb_fail = true ) && ( $medusa_fail = true ) && ( $sliqsim_fail = true ) ]]; then
                    break;
                fi
            done
        else
            # Circuits with a loop variant
            for file in "$folder"[0-9]*.qasm; do
                # To avoid an invalid iteration when no file matches the criteria
                if [ -f "$file" ]; then
                    run_benchmark_file $file true
                fi
                if [[ ( $medusa_symb_fail = true ) && ( $medusa_fail = true ) && ( $sliqsim_fail = true ) ]]; then
                    break;
                fi
            done
        fi
    done
}

# Removes 'LP-' from benchmark names and correctly reorders the given csv file.
format_csv() {
    local file=$1
    local tmp_file="tmp.csv"

    sed -i '/^LP-/ s/^LP-//' "$file"
    { head -n1 "$file"; tail -n+2 "$file" | sort -t, -k1,1; } > "$tmp_file"
    mv "$tmp_file" "$file"
}

#####################################################################################
# Output:
is_measure=false # global so it can be read from all the functions
run_benchmarks
format_csv $FILE_OUT

is_measure=true
run_benchmarks
format_csv $FILE_OUT_MEASURE