call "common.gnuplot" "3.3in,3.2in"

# set terminal pdf
# set output "batch-insertion-no-rewrite-id.pdf"
set output "`echo $OUT`"

mp_startx=0.10
mp_starty=0.06
mp_height=0.83
mp_rowgap=0.14
mp_colgap=0.03

eval mpSetup(1, 1)

set key top right
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
set ylabel 'Algorithm throughput (M edges/s)' offset -1,0
set xlabel 'Size Tile Batch'
# set label '\textbf{Used Memory}' at 4, 2650
plot \
    'algorithm/algorithm-performance_pagerank_rmat-22_65536_0_column-first_0.dat' using ($1):($4) index 0 title "rmat-22, column-first" with linespoint ls 1, \
    'algorithm/algorithm-performance_pagerank_rmat-22_65536_0_row-first_0.dat' using ($1):($4) index 0 title "rmat-22, row-first" with linespoint ls 2, \
    'algorithm/algorithm-performance_pagerank_rmat-22_65536_0_hilbert_0.dat' using ($1):($4) index 0 title "rmat-22, hilbert" with linespoint ls 3, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_column-first_0.dat' using ($1):($4) index 0 title "rmat-24, column-first" with linespoint ls 4, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_row-first_0.dat' using ($1):($4) index 0 title "rmat-24, row-first" with linespoint ls 5, \
    'algorithm/algorithm-performance_pagerank_rmat-24_65536_0_hilbert_0.dat' using ($1):($4) index 0 title "rmat-24, hilbert" with linespoint ls 6, \
    'algorithm/algorithm-performance_pagerank_rmat-27_65536_0_column-first_0.dat' using ($1):($4) index 0 title "rmat-27, column-first" with linespoint ls 7, \
    'algorithm/algorithm-performance_pagerank_rmat-27_65536_0_row-first_0.dat' using ($1):($4) index 0 title "rmat-27, row-first" with linespoint ls 8, \
    'algorithm/algorithm-performance_pagerank_rmat-27_65536_0_hilbert_0.dat' using ($1):($4) index 0 title "rmat-27, hilbert" with linespoint ls 9, \
