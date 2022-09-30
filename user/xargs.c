#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"

int readn(int argc, char *argv[], int *n)
{
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n')
    {
        *n = atoi(&argv[1][2]);
        return 1;
    }
    *n = 1; // default
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("xargs needs at least one arguments!\n");
        exit(-1);
    }

    int n; // 额外的参数个数
    int cnt = 0;
    int hasN = readn(argc, argv, &n);
    int size = argc + n - hasN;
    char *buf[size], *line[n], **p;

    for (int i = 0; i < size - 1; i++)
    {
        buf[i] = (char *)malloc(MAXARG);
        if (i + 1 + hasN < argc)
            memmove(buf[i], argv[i + 1 + hasN], MAXARG);
    }

    p = buf + (argc - 1 - hasN);

    for (int i = 0; i < n; i++)
    {
        line[i] = (char *)malloc(MAXARG);
    }
    while (gets(line[cnt], MAXARG))
    {
        if (line[cnt][0] == 0)
            break;
        line[cnt][strlen(line[cnt]) - 1] = 0; // 此位置为换行符
        cnt++;

        if (cnt < n)
        {
            continue;
        }

        for (int i = 0; i < n; i++)
        {
            memmove(p[i], line[i], strlen(line[i]) + 1);
            memset(line[i], 0, MAXARG); // 清空
        }
        p[n] = 0;
        cnt = 0;

        int pid = fork();
        if (pid == 0)
        {
            // child
            if (exec(buf[0], buf) < 0)
                printf("xargs: exec() failed\n");
            exit(-1);
        }
        else if (pid > 0)
        {
            // parent
            wait(0);
        }
        else
        {
            printf("xargs: fork() failed\n");
            exit(-1);
        }
    }

    for (int i = 0; i < size - 1; i++)
    {
        free(buf[i]);
    }
    for (int i = 0; i < n; i++)
    {
        free(line[i]);
    }
    exit(0);
}