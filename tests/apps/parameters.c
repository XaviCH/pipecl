#include "parameters.h"

#include <stdio.h>

size_t test_get_width()
{
    #ifndef WIDTH
    #define WIDTH 1920
    #endif
    
    return WIDTH; 
}

size_t test_get_height()
{
    #ifndef HEIGHT
    #define HEIGHT 1080
    #endif
    
    return HEIGHT; 
}

size_t test_get_size()
{
    #ifndef SIZE
    #define SIZE 1
    #endif
    
    return SIZE; 
}

int test_is_offline_rendering()
{
    #ifndef OFFLINE
    #define OFFLINE 1
    #endif
    
    return OFFLINE;
}

static void __attribute__((constructor)) setup()
{
    printf("TEST PARAMETERS: width=%ld, height=%ld, size=%ld, offline=%d\n\n", 
        test_get_width(),
        test_get_height(),
        test_get_size(),
        test_is_offline_rendering()
    );

}