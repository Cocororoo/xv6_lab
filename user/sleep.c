#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Sleep needs one arguments!\n");
        exit(-1);
    }
    int ticks = atoi(argv[1]);
    sleep(ticks);
    printf("(Nothing happens for a litte while)\n");
    exit(0);
}