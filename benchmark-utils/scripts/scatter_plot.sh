#!/bin/bash

# It is assumed that the script is run from the repository's home folder.
# Expects path to csv file as first argument and the output file name (will be in pdf format) as second argument.

# ===============================================

BENCHMARKS_PATH="./benchmarks/no-measure/"
TEMP_FILE_SUFFIX="-temp.out"

MIN_TIME=0.002
MAX_TIME=3600
ZOOM_MAX_TIME=1

MEDUSA_COLUMN=4
OTHER_SIM_COLUMN=6
OTHER_SIM_NAME="SliQSim"
AXIS_LABEL="Runtime (s)"

# ===============================================

parse_bench_dir() {
    local temp_file_name=$dir_name$TEMP_FILE_SUFFIX

    awk -v maxt="$MAX_TIME" -v dir_name="$dir_name" \
        -v medusa_col="$MEDUSA_COLUMN" -v other_sim_col="$OTHER_SIM_COLUMN" \
        -F ',' \
    'index($1, dir_name) == 1 {
        if (!match($medusa_col, /[0-9]*\.?[0-9]+/)) {
            $medusa_col=maxt;
        };
        if (!match($other_sim_col, /[0-9]*\.?[0-9]+/)) {
            $other_sim_col=maxt;
        };
        print $other_sim_col, $medusa_col
    }' "$in_file_path" > "$temp_file_name"
}

plot() {
    local out_file=$1
    local min=$2
    local max=$3

    gnuplot -persist <<-EOFMarker
        set terminal pdf size 30cm,20cm enhanced background rgb 'white'
        set output "$out_file"
        
        set multiplot

        set origin 0,0
        set size 1,1
        set xlabel "$OTHER_SIM_NAME $AXIS_LABEL" font ",20"
        set ylabel "MEDUSA $AXIS_LABEL" font ",20"
        set logscale
        set grid

        set xrange ["$min":"$max"]
        set yrange ["$min":"$max"]
        set size square

        set xtics font ",20"
        set ytics font ",20"
        set key left top font ",20" box

        files = system("ls *$TEMP_FILE_SUFFIX")
        pt_types = "6 7 5 4 3 11 1 2 10"
        lc_colors = "9 4 3 8 9 5 8 2 6"
        plot_files = ''
        idx = 1
        do for [file in files] {
            title = system("echo " . file . " | sed 's/$TEMP_FILE_SUFFIX//'")
            pt = word(pt_types, idx)
            lc = word(lc_colors, idx)
            plot_files = plot_files . '"' . file . '" title "' . title . '" pt ' . pt . ' lc ' . lc . ' ps 1.5,'
            idx = idx + 1
        }
        # Remove trailing comma
        plot_files = substr(plot_files, 1, strlen(plot_files)-1)
        eval("plot " . plot_files)

        # Plot the diagonal
        plot x lc 7 title ""
EOFMarker
}

# ===============================================

if [ $# -ne 2 ]; then
    echo "Usage: $0 <results-file-path> <output-file-name>"
    exit 1
fi

in_file_path=$1
output_file_name=$2

output_file_extension="${output_file_name##*.}"
zoom_output_file_name="${output_file_name%.*}_z.$output_file_extension"

for dir in "$BENCHMARKS_PATH"*/; do
    dir_name=$(basename "$dir")
    dir_name=${dir_name#LP-}
    parse_bench_dir
done

plot $output_file_name $MIN_TIME $MAX_TIME
plot $zoom_output_file_name $MIN_TIME $ZOOM_MAX_TIME

rm -f *"$TEMP_FILE_SUFFIX"
