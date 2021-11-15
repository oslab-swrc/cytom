CC=clang++

src_to_bc:
	$(CC) -c -emit-llvm -I../src/include -I../src/util/pci-ring-buffer/include -I../build/Release-x86_64/flatbuffer/include -flimit-debug-info -fno-rtti -O2 -pthread -std=gnu++14 $(input) -o $(output)

bc_to_exe:
	llc $(task_bc) -o $(task_s)
	$(CC) -o $(task_exe) -L../build/Release-x86_64/lib $(task_s) -lpthread -lperf-event -lutil -lring-buffer -lpci_ring_buffer -ltraversal -lboost_program_options -lrt
	$(RM) $(link_bc) $(task_bc) $(task_s)

%:
	@:

clean:
	$(RM) *.ll *.bc *.s *.o execute

