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

set key bottom right
# set key spacing 1.25
# set key at 12,40
# unset key

eval mpNext
set xrange [1:65536]
set yrange [0:*]
set y2range [0:*]
set logscale x
set format y "%.0fM"
set format x "%.0f"
set ylabel 'Insertion throughput (edges/s)' offset -1,0
set y2label 'Compactions' offset -3,0
set y2tics
set xlabel 'Size Insertion Batch'
# set label '\textbf{Used Memory}' at 4, 2650
plot \
    'insertion_batch/insertion-batch_pagerank_twitter-full_512_0_hilbert_0.dat' using ($1):($4/1000000) index 0 title "twitter, no dynamic compaction" with linespoint ls 1, \
    'insertion_batch/insertion-batch_pagerank_twitter-full_512_0_hilbert_1.dat' using ($1):($4/1000000) index 0 title "twitter, dynamic compaction" with linespoint ls 2, \
    'insertion_batch/insertion-batch_pagerank_twitter-full_512_0_hilbert_1.dat' using ($1):($6) axes x1y2 title "twitter, Count compactions" with linespoint ls 7, \
    'insertion_batch/insertion-batch_pagerank_uk2007_512_0_hilbert_0.dat' using ($1):($4/1000000) index 0 title "uk2007-05, no dynamic compaction" with linespoint ls 3, \
    'insertion_batch/insertion-batch_pagerank_uk2007_512_0_hilbert_1.dat' using ($1):($4/1000000) index 0 title "uk2007-05, dynamic compaction" with linespoint ls 4, \
    'insertion_batch/insertion-batch_pagerank_uk2007_512_0_hilbert_1.dat' using ($1):($6) axes x1y2 title "uk2007-05, Count compactions" with linespoint ls 9, \
    'insertion_batch/insertion-batch_pagerank_twitter-small_512_0_hilbert_0.dat' using ($1):($4/1000000) index 0 title "twitter-small, no dynamic compaction" with linespoint ls 5, \
    'insertion_batch/insertion-batch_pagerank_twitter-small_512_0_hilbert_1.dat' using ($1):($4/1000000) index 0 title "twitter-small, dynamic compaction" with linespoint ls 6, \
    'insertion_batch/insertion-batch_pagerank_twitter-small_512_0_hilbert_1.dat' using ($1):($6) axes x1y2 title "twitter-small, Count compactions" with linespoint ls 8, \
