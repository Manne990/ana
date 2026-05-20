#include "ana.h"

#include <stdio.h>
#include <string.h>

static void print_usage(void)
{
    printf("ana-convert %s\n", ANA_VERSION_STRING);
    printf("Usage: ana-convert --version\n");
    printf("\n");
    printf("Asset conversion is planned for ANA 0.1, but is not implemented yet.\n");
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", ANA_VERSION_STRING);
        return 0;
    }

    print_usage();
    return 0;
}

