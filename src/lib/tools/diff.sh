#!/bin/sh

set -e

cd ../../../build/RelWithDebInfo-x86_64/lib/tools;
for graph in livejournal orkut; do
    ./sort_results \
        --input-results /home/steffen/research/kaleidoscope/results-storage/final_results_sssp_${graph}_0.00 \
        --output-results /home/steffen/research/kaleidoscope/results-storage/sorted_final_results_sssp_${graph}_0.00 \
        --sort-ascending 1 \
        --algorithm sssp
    for delta in 0.0025 0.005 0.01 0.02 0.04 0.08 0.16 0.32 0.64; do
        ./sort_results \
            --input-results /home/steffen/research/kaleidoscope/results-storage/final_results_sssp_${graph}_${delta} \
            --output-results /home/steffen/research/kaleidoscope/results-storage/sorted_final_results_sssp_${graph}_${delta} \
            --sort-ascending 1 \
            --algorithm sssp
        ./results_diff \
            --result-1 /home/steffen/research/kaleidoscope/results-storage/sorted_final_results_sssp_${graph}_0.00 \
            --result-2 /home/steffen/research/kaleidoscope/results-storage/sorted_final_results_sssp_${graph}_$delta \
            --output-file /home/steffen/research/kaleidoscope/results-storage/comparison_sssp_${graph}_0.00_$delta \
            --count-comparisons $1
    done
done

cd ../../../../src/lib/tools;
for graph in livejournal orkut; do
    echo $graph
    for delta in 0.0025 0.005 0.01 0.02 0.04 0.08 0.16 0.32 0.64; do
        ./kendall-tau-computation.py \
            --input /home/steffen/research/kaleidoscope/results-storage/comparison_sssp_${graph}_0.00_$delta \
            --delta $delta
    done
done
