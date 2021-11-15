// SPDX-License-Identifier: MIT

#include "core/jobs.h"

void dummy_func(char * args[], evolving_graphs::Jobs * jobs, uint8_t idx) {
  return;
}

int main(int argc, char** argv) {

  evolving_graphs::Jobs jobs(argc, argv);
  
  jobs.add_job(dummy_func);

  jobs.launch_edge_readers();

  jobs.join_all();

  return 0;
}