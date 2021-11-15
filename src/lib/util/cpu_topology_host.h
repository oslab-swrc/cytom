// SPDX-License-Identifier: MIT

enum {
    NUM_SOCKET = 1,
    NUM_PHYSICAL_CPU_PER_SOCKET = 2,
    SMT_LEVEL = 2,
};

const int OS_CPU_ID[NUM_SOCKET][NUM_PHYSICAL_CPU_PER_SOCKET][SMT_LEVEL] = {
    { /* socket id: 0 */
        { /* physical cpu id: 0 */
          0, 2,     },
        { /* physical cpu id: 1 */
          1, 3,     },
    },
};
