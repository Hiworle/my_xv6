#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int dad2son_fd[2];
    int son2dad_fd[2];
    int ret1 = pipe(dad2son_fd);
    int ret2 = pipe(son2dad_fd);
    if (ret1 < 0 || ret2 < 0)
    {
        printf("pipe() failed\n");
        exit(1);
    }

    int pid = fork();
    if (pid > 0)
    {
        /* 父进程 */
        // send "ping"
        char *send = "ping";
        close(dad2son_fd[0]);
        write(dad2son_fd[1], send, 5);
        close(dad2son_fd[1]);

        // receive "pong"
        char received[5];
        close(son2dad_fd[1]);
        int r_len = read(son2dad_fd[0], received, 5);
        if (r_len > 0)
        {
            received[r_len] = '\0';
            printf("%d: received %s\n", getpid(), received);
        }
        close(son2dad_fd[0]);
    }
    else if (pid == 0)
    {
        /* 子进程 */
        // receive "ping"
        char received[5];
        close(dad2son_fd[1]);
        int r_len = read(dad2son_fd[0], received, 5);
        if (r_len > 0)
        {
            received[r_len] = '\0';
            printf("%d: received %s\n", getpid(), received);
        }
        close(dad2son_fd[0]);

        // send "pong"
        char *send = "pong";
        close(son2dad_fd[0]);
        write(son2dad_fd[1], send, 5);
        close(son2dad_fd[1]);
    }
    else
    {
        printf("fork() failed\n");
        exit(1);
    }

    exit(0);
}
