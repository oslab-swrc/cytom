// SPDX-License-Identifier: MIT

#pragma once

#define set_active(__array, __vertex) set_bool_array(__array, __vertex, true)
#define set_inactive(__array, __vertex) set_bool_array(__array, __vertex, false)

#define set_active_atomically(__array, __vertex) set_bool_array_atomically(__array, __vertex, true)
#define set_inactive_atomically(__array, __vertex) set_bool_array_atomically(__array, __vertex, false)

#define is_active(__array, __vertex) (eval_bool_array(__array, __vertex) == 1)
#define is_inactive(__array, __vertex) (eval_bool_array(__array, __vertex) == 0)
