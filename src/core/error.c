#include "error.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define PER_LINE 0x10

void hexdump(void *memory, size_t length)
{
    if (!memory)
    {
        ERROR("Hexdump: Got null pointer");
        return;
    }

    if (!length)
    {
        ERROR("Hexdump: Got invalid length data structure");
        return;
    }

    printf("*** Hexdump Debug Utility ***\n");
    printf("Printing data of length %zu from %p address\n", length, memory);

    unsigned char *ascii = calloc(PER_LINE + 1, sizeof(unsigned char));

    const unsigned char *buffer = (const unsigned char *)memory;

    size_t i;
    if (length > 5000)
    {
        printf("Temporarily set limit of 5000 on length\n");
        length = 5000;
    }
    for (i = 0; i < length; i++)
    {
        if ((i % PER_LINE) == 0) printf(" %s\n +%08zX: ", ascii, i);

        printf(" %02X ", *(buffer + i));

        if (*(buffer + i) < 0x20 || *(buffer + i) > 0x7E)
            ascii[i % PER_LINE] = '.';
        else
            ascii[i % PER_LINE] = *(buffer + i);

        ascii[(i % PER_LINE) + 1] = '\0';
    }

    while ((i % PER_LINE) != 0)
    {
        printf("    ");
        i++;
    }

    printf(" %s\n", ascii);

    free(ascii);
}
