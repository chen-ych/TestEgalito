#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char *dir;

    if(argc == 3) {
        dir = getcwd(NULL, SHRT_MAX);

        if(dir) {
            if(!chdir(argv[1])) {
                system(argv[2]);

                chdir(dir);
                free(dir);
            }
            else {
                fprintf(stderr, "presdir: Invalid directory: %s\n", argv[1]);
            }
        }
        else {
            fprintf(stderr, "presdir: Out of memory\n");
        }
    }
    else {
        fprintf(stderr, "usage: %s dir program.exe\n", argv[0]);
    }

    return 0;
}
