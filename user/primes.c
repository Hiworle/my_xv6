
#include "kernel/types.h"
#include "user.h"

/**
 * 读取输入的数据，进行筛选
 * 如果筛选后还剩数据，就通过管道传入下一个线程
 *
 * 输入：管道读端 p[0]
 */
void prime(int in)
{
    int n, m;
    int created = 0;
    int p[2];

    read(in, &n, sizeof(int));
    printf("prime %d\n", n); // 第一个数据必定为素数

    // 读入数据
    while (read(in, &m, sizeof(int)))
    {
        if (!created)
        { // 创建管道和子线程
            pipe(p);
            created = 1;
            if (fork() == 0)
            {
                close(p[1]);
                prime(p[0]);
                close(p[0]);
            }
            else
            {
                close(p[0]);
            }
        }

        if (m % n != 0)
        {
            write(p[1], &m, sizeof(int));
        }
    }

    close(in);
    close(p[1]);
}

int main(int argc, char *argv[])
{
    int n;
    if (argc != 2)
    { // 默认值为 35
        n = 35;
    }
    else
    { // 接受自定义的 int 值，但是不能太大
        n = atoi(argv[1]);
    }

    int p[2];
    pipe(p);
    int pid = fork();
    if (pid > 0)
    {
        close(p[0]);
        for (int i = 2; i <= n; i++)
        {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
    }
    else if (pid == 0)
    {
        close(p[1]);
        prime(p[0]);
        close(p[0]);
    }
    else
    {
        printf("fork() failed\n");
        exit(1);
    }

    wait(0); // 等待子进程结束再退出
    exit(0);
}