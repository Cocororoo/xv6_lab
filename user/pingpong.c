#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]){
    if(argc != 1){
        printf("Too many args!\n");
        exit(-1);
    }

    int pa[2];
    int pb[2];
    int ret;
    ret = pipe(pa) || pipe(pb); 
    int pid;

    if (ret == 0)
    {
        if ((pid = fork()) == -1) {
            printf("Fork error!");
            exit(-1);
        }

        if (pid == 0) { 
            /* 子进程 */
            close(pb[1]); // 关闭pb管道写端
            close(pa[0]); // 关闭pa管道读端

            char buf[5];
            if (read(pb[0], buf, 4) > 0) {
                buf[4] = '\0';
                printf("%d: received %s\n", getpid(), buf);

                // 发送pong给父进程
                write(pa[1], "pong", 4);
            }

            close(pa[1]); // 写入完成，关闭写端
            close(pb[0]); // 读取完成，关闭读端
        } else {
            /*父进程*/
            close(pa[1]); // 关闭pa管道写端
            close(pb[0]); // 关闭pb管道读端

            // 发送ping给子进程
            write(pb[1], "ping", 4);

            char buf[5];
            if (read(pa[0], buf, 4) > 0) {
                buf[4] = '\0';
                printf("%d: received %s\n", getpid(), buf);
            }

            close(pa[0]); // 写入完成，关闭写端
            close(pb[1]); // 读取完成，关闭读端
        }
    } else {
        printf("Pipe Error!");
        exit(-1);
    }

    exit(0);
}