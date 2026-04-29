#ifndef TESTS_GLSC2_PARAMETERS_H
#define TESTS_GLSC2_PARAMETERS_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t  test_get_width();
size_t  test_get_height();
size_t  test_get_size();
int     test_is_offline_rendering();

#ifdef __cplusplus
}
#endif

#endif // TESTS_GLSC2_PARAMETERS_H