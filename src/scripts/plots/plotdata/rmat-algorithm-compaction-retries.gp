call "common.gnuplot" "3.3in,3.2in"

# set terminal pdf
# set output "batch-insertion-no-rewrite-id.pdf"
set output "`echo $OUT`"

mp_startx=0.10
mp_starty=0.06
mp_height=0.83
mp_rowgap=0.14
mp_colgap=0.03
mp_width=0.8

eval mpSetup(1, 1)

set key top right
# set key spacing 1.25
# set key at 12,40
# unset key

eval mpNext
set xrange [1:65536]
set yrange [0:*]
set y2range [0:*]
set logscale x
unset xlabel
set format y "%.0fM"
set format x "%.0f"
set ylabel 'Algorithm throughput (M edges/s)' offset -1,0
set y2label 'Retries' offset -3,0
set y2tics
set xlabel 'Size Tile Batch'
# set label '\textbf{Used Memory}' at 4, 2650
plot \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_column-first_0.dat' using ($1):($4) index 0 title "column-first" with linespoint ls 1, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_row-first_0.dat' using ($1):($4) index 0 title "row-first" with linespoint ls 2, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_hilbert_0.dat' using ($1):($4) index 0 title "hilbert" with linespoint ls 3, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_column-first_1.dat' using ($1):($4) index 0 title "column-first, dynamic" with linespoint ls 4, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_row-first_1.dat' using ($1):($4) index 0 title "row-first, dynamic" with linespoint ls 5, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_hilbert_1.dat' using ($1):($4) index 0 title "hilbert, dynamic" with linespoint ls 6, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_column-first_0.dat' using ($1):($6) axes x1y2 title "retries column-first" with linespoint ls 7, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_row-first_0.dat' using ($1):($6) axes x1y2 title "retries row-first" with linespoint ls 8, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_hilbert_0.dat' using ($1):($6) axes x1y2 title "retries hilbert" with linespoint ls 9, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_column-first_1.dat' using ($1):($6) axes x1y2 title "retries column-first, dynamic" with linespoint ls 10, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_row-first_1.dat' using ($1):($6) axes x1y2 title "retries row-first, dynamic" with linespoint ls 11, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_hilbert_1.dat' using ($1):($6) axes x1y2 title "retries hilbert, dynamic" with linespoint ls 12, \
