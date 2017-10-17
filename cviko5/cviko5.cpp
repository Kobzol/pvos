#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/mman.h>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <memory>
#include <sys/stat.h>

struct data
{
    float cisla[10];
    float suma;
};

void fifo()
{
    int roura = open("/tmp/trubka", O_RDONLY | O_NONBLOCK);
    char buf[1024];

    fcntl(roura, F_SETFL, fcntl(roura, F_GETFL) & ~O_NONBLOCK);

    while (true)
    {
        ssize_t ret = read(roura, buf, sizeof(buf));
        if (ret > 0)
        {
            write(STDOUT_FILENO, buf, ret);
        }
        else if (ret < 0)
        {
            printf("fail: %s\n", strerror(errno));
            break;
        }
        else
        {
            printf("pipa nema vstup\n");
            usleep(10 * 1000);
        }
    }
}
void mmaped_file()
{
    const size_t size = 100;

    int data = open("data.dat", O_RDWR | O_CREAT, 0660);
    ftruncate(data, size);
    auto* mem = static_cast<char*>(::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, data, 0));
    strcpy(mem, "ahoj kamo\njak se mas?\n");

    if (msync(mem, size, MS_SYNC) != 0)
    {
        printf("error: %s\n", strerror(errno));
    }
    munmap(mem, size);

    close(data);
}
void systemv_shm()
{
    int id = shmget(0xCAFECAFE, 1024, IPC_CREAT | 0660);
    auto* mem = (char*) shmat(id, nullptr, 0);

    int pid = fork();
    if (pid == 0)
    {
        sleep(1);
        printf("%s", mem);

        exit(0);
    }

    strcpy(mem, "ahoj kamo\n");

    wait(nullptr);

    shmdt(mem);
    shmctl(id, IPC_RMID, nullptr);
}

#define TOTAL_SIZE (1UL * 1024UL * 1024UL * 1024UL)
#define PIPE_BUFFER_SIZE (65536L)

void uloha1_anonymous()
{
    int pipes[2];
    pipe(pipes);

    int pid = fork();
    if (pid == 0)
    {
        close(pipes[1]);

        size_t sum = 0;
        auto* buffer = new int[PIPE_BUFFER_SIZE / sizeof(int)];
        while (true)
        {
            ssize_t size = read(pipes[0], buffer, PIPE_BUFFER_SIZE);
            if (size < 1) break;

            for (size_t i = 0; i < size / sizeof(int); i++)
            {
                sum += buffer[i];
            }
        }
        delete[] buffer;

        printf("child: %ld\n", sum);

        exit(0);
    }

    close(pipes[0]);

    size_t sum = 0;
    srand((unsigned int) time(nullptr));

    size_t counter = 0;
    auto* buffer = new int[PIPE_BUFFER_SIZE / sizeof(int)];
    while (counter < TOTAL_SIZE)
    {
        for (size_t i = 0; i < PIPE_BUFFER_SIZE / sizeof(int); i++)
        {
            int generated = rand() % 65536;
            sum += generated;
            buffer[i] = generated;
            counter += sizeof(int);
        }

        write(pipes[1], buffer, PIPE_BUFFER_SIZE);
    }
    delete[] buffer;

    close(pipes[1]);

    wait(nullptr);

    printf("parent: %ld\n", sum);
}
void uloha1_mkfifo()
{
    const char* path = "/tmp/pvos";
    unlink(path);

    int fd = mkfifo(path, 0660);
    if (fd != 0)
    {
        printf("%s\n", strerror(errno));
        throw "couldn't create FIFO";
    }

    int pid = fork();
    if (pid == 0)
    {
        int pipe = open(path, O_RDONLY);

        size_t sum = 0;
        auto* buffer = new int[PIPE_BUFFER_SIZE / sizeof(int)];
        while (true)
        {
            ssize_t size = read(pipe, buffer, PIPE_BUFFER_SIZE);
            if (size < 1) break;

            for (size_t i = 0; i < size / sizeof(int); i++)
            {
                sum += buffer[i];
            }
        }
        delete[] buffer;

        printf("child: %ld\n", sum);

        exit(0);
    }

    int pipe = open(path, O_WRONLY);

    size_t sum = 0;
    srand((unsigned int) time(nullptr));

    size_t counter = 0;
    auto* buffer = new int[PIPE_BUFFER_SIZE / sizeof(int)];
    while (counter < TOTAL_SIZE)
    {
        for (size_t i = 0; i < PIPE_BUFFER_SIZE / sizeof(int); i++)
        {
            int generated = rand() % 65536;
            sum += generated;
            buffer[i] = generated;
            counter += sizeof(int);
        }

        write(pipe, buffer, PIPE_BUFFER_SIZE);
    }
    delete[] buffer;
    close(pipe);

    wait(nullptr);

    printf("parent: %ld\n", sum);

    unlink(path);
}
void uloha1_shmem()
{
    const size_t mmap_size = 10 * 1024 * 1024;

    auto* mem = static_cast<int*>(::mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_SHARED, -1, 0));
    int readReadyPipe[2];
    pipe(readReadyPipe);
    int writeReadyPipe[2];
    pipe(writeReadyPipe);

    int pid = fork();
    if (pid == 0)
    {
        close(readReadyPipe[1]);
        close(writeReadyPipe[0]);

        char buffer;
        size_t sum = 0;
        while (true)
        {
            ssize_t len = read(readReadyPipe[0], &buffer, 1);
            if (len < 1) break;

            for (size_t i = 0; i < mmap_size / sizeof(int); i++)
            {
                sum += mem[i];
            }

            write(writeReadyPipe[1], &buffer, 1);
        }

        printf("child: %ld\n", sum);

        exit(0);
    }

    size_t sum = 0;
    srand((unsigned int) time(nullptr));

    size_t counter = 0;
    char buffer = 0;
    while (counter < TOTAL_SIZE)
    {
        for (size_t i = 0; i < mmap_size / sizeof(int); i++)
        {
            int generated = rand() % 65536;
            sum += generated;
            mem[i] = generated;
            counter += sizeof(int);
        }

        write(readReadyPipe[1], &buffer, 1);
        read(writeReadyPipe[0], &buffer, 1);
    }

    close(readReadyPipe[1]);
    close(writeReadyPipe[0]);

    wait(nullptr);

    munmap(mem, mmap_size);

    printf("parent: %ld\n", sum);
}

void uloha2_check_obj(const data& d)
{
    float sum = 0.0f;
    for (int i = 0; i < 10; i++)
    {
        sum += d.cisla[i];
    }

    if (sum != d.suma)
    {
        throw "NOT VALID";
    }
}
void uloha2_alter_obj(data& obj)
{
    int index = rand() % 10;
    obj.cisla[index] += (rand() % 256);

    float sum = 0.0f;
    for (int i = 0; i < 10; i++)
    {
        sum += obj.cisla[i];
    }

    obj.suma = sum;
}
void uloha2()
{
    data d = { 0 };
    const char* path = "pvos.dat";
    {
        int fd = open(path, O_CREAT | O_WRONLY, 0777);
        write(fd, &d, sizeof(data));
        close(fd);
    }

    for (int i = 0; i < 10; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            srand(getpid() + time(nullptr) * i);

            data obj = { 0 };
            int fd = open(path, O_RDWR);
            ftruncate(fd, sizeof(data));

            if (rand() % 2 == 0)
            {
                ssize_t len = read(fd, &obj, sizeof(data));
                if (len < 1)
                {
                    throw "ERROR READING";
                }

                printf("Loaded sum: %f\n", obj.suma);
                uloha2_check_obj(obj);
                uloha2_alter_obj(obj);

                lseek(fd, SEEK_SET, 0);
                len = write(fd, &obj, sizeof(data));
                if (len != sizeof(data))
                {
                    throw "ERROR WRITING";
                }

                printf("Child %d used file, sum: %f\n", getpid(), obj.suma);
            }
            else
            {
                auto* mem = static_cast<char*>(::mmap(nullptr, sizeof(data),
                                                      PROT_READ | PROT_WRITE,
                                                      MAP_SHARED, fd, 0));
                data* ptr = (data*) mem;
                printf("Loaded sum: %f\n", ptr->suma);

                uloha2_check_obj(*ptr);
                uloha2_alter_obj(*ptr);

                msync(mem, sizeof(data), MS_SYNC);
                //munmap(mem, sizeof(data));

                printf("Child %d used mmap, sum: %f\n", getpid(), ptr->suma);
            }

            close(fd);

            usleep(1000 * (rand() % 512));
            exit(0);
        }

        wait(nullptr);
    }
}

int main()
{
    //printf("%d\n", sizeof(data));
    //uloha1_anonymous(); // 64 KiB, 1 GiB, 5.92 s
    //uloha1_mkfifo(); // 64 KiB, 1 GiB, 5.61 s
    //uloha1_shmem(); // 64 KiB, 1 GiB, 2.37 s
    uloha2();

    return 0;
}
