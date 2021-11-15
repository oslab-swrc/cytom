call "common.gnuplot" "3.3in,3.2in"

# set terminal pdf
# set output "batch-insertion-rewrite-id.pdf"
set output "`echo $OUT`"

mp_startx=0.10
mp_starty=0.06
mp_height=0.83
mp_rowgap=0.14
mp_colgap=0.03

eval mpSetup(1, 1)

set key bottom right
# set key spacing 1.25
# set key at 12,40
# unset key

eval mpNext
set xrange [1:65536]
set yrange [0:*]
set logscale x
unset xlabel
set format y "%.0fM"
set format x "%.0f"
set ylabel 'Insertion throughput (edges/s)' offset -1,0
set xlabel 'Size Insertion Batch'
# set label '\textbf{Used Memory}' at 4, 2650
plot \
    'insertion_batch/insertion-batch_pagerank_rmat-16_512_1_hilbert_0.dat' using ($1):($4/1000000) index 0 title "rmat-16, no dynamic compaction" with linespoint ls 1, \
    'insertion_batch/insertion-batch_pagerank_rmat-16_512_1_hilbert_1.dat' using ($1):($4/1000000) index 0 title "rmat-16, dynamic compaction" with linespoint ls 2, \
    'insertion_batch/insertion-batch_pagerank_rmat-22_512_1_hilbert_1.dat' using ($1):($4/1000000) index 0 title "rmat-22, no dynamic compaction" with linespoint ls 3, \
    'insertion_batch/insertion-batch_pagerank_rmat-22_512_1_hilbert_1.dat' using ($1):($4/1000000) index 0 title "rmat-22, dynamic compaction" with linespoint ls 4, \
    'insertion_batch/insertion-batch_pagerank_rmat-24_512_1_hilbert_1.dat' using ($1):($4/1000000) index 0 title "rmat-24, no dynamic compaction" with linespoint ls 5, \
    'insertion_batch/insertion-batch_pagerank_rmat-24_512_1_hilbert_1.dat' using ($1):($4/1000000) index 0 title "rmat-24, dynamic compaction" with linespoint ls 6
