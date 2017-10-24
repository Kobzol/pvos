#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <cstdio>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <wait.h>
#include <cstdlib>
#include <unordered_map>
#include <iostream>

void semaphore_systemv()
{
    int sem = semget(0xCAFECAFE, 1, IPC_CREAT | 0660);
    semctl(sem, 0, SETVAL, 1);

    fork();

    sembuf up{0, 1, 0};
    sembuf down{0, -1, 0};
    int a, b;

    while (true)
    {
        semop(sem, &down, 1);
        printf("(%d): zadej dve cisla\n", getpid());
        scanf("%d", &a);
        scanf("%d", &b);
        semop(sem, &up, 1);

        printf("(%d): mam %d %d\n", getpid(), a, b);
    }

    semctl(sem, 0, IPC_RMID);
}
void semaphore_posix()
{
    const char* semname = "/sem";
    sem_unlink(semname);
    sem_t* sem = sem_open(semname, O_CREAT | O_RDWR, 0660, 1);

    fork();

    int a, b;
    while (true)
    {
        sem_wait(sem);
        printf("(%d): zadej dve cisla\n", getpid());
        scanf("%d", &a);
        scanf("%d", &b);
        sem_post(sem);

        if (a == 0) break;

        printf("(%d): mam %d %d\n", getpid(), a, b);
    }

    sem_close(sem);
    sem_unlink(semname);
}
void semaphore_posix2()
{
    void* addr = mmap(nullptr, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    auto* sem = (sem_t*) addr;
    sem_init(sem, 1, 1);

    fork();

    int a, b;
    while (true)
    {
        sem_wait(sem);
        printf("(%d): zadej dve cisla\n", getpid());
        scanf("%d", &a);
        scanf("%d", &b);
        sem_post(sem);

        if (a == 0) break;

        printf("(%d): mam %d %d\n", getpid(), a, b);
    }

    sem_destroy(sem);
    munmap(addr, sizeof(sem_t));
}

#define TRANSPORT_SIZE (128)
#define CHILDREN_COUNT 10
struct Transport
{
    int index;
    int data[TRANSPORT_SIZE];
};

void uloha1()
{
    int semPut = semget(0xCAFECAFE, 1, IPC_CREAT | 0666);
    semctl(semPut, 0, SETVAL, 1);

    int semGet = semget(0xCAFECAFF, 1, IPC_CREAT | 0666);
    semctl(semGet, 0, SETVAL, 0);

    sembuf up{0, 1, 0};
    sembuf down{0, -1, 0};

    auto* transport = (Transport*)
            ::mmap(nullptr, sizeof(Transport), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    transport->index = 0;

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            int total = 0;
            srand(getpid());
            while (true)
            {
                semop(semPut, &down, 1);
                //printf("%d putting, count %d\n", getpid(), transport->index);
                int count = transport->index;
                transport->data[count++] = getpid();
                transport->index++;
                total++;
                //printf("%d total: %d\n", getpid(), total);

                semop(semGet, &up, 1);
                if (count < TRANSPORT_SIZE)
                {
                    semop(semPut, &up, 1);
                }

                //usleep(1000 * (rand() % 500));
            }

            exit(0);
        }
    }

    sembuf downmax{0, -TRANSPORT_SIZE, 0};
    std::unordered_map<int, int> children;

    while (true)
    {
        semop(semGet, &downmax, 1);
        printf("parent extracting\n");
        transport->index = 0;
        for (int i = 0; i < TRANSPORT_SIZE; i++)
        {
            int pid = transport->data[i];
            if (children.find(pid) == children.end()) children.insert({pid, 0});
            children[pid]++;
        }

        usleep(1000 * (rand() % 500));

        for (auto& child: children)
        {
            printf("%d: %d\n", child.first, child.second);
        }
        semop(semPut, &up, 1);
    }

    while (waitpid(-1, nullptr, 0) >= 0);
}
void uloha2()
{
    int semPut = semget(0xCAFECAFE, 1, IPC_CREAT | 0666);
    semctl(semPut, 0, SETVAL, 1);

    int semGet = semget(0xCAFECAFF, 1, IPC_CREAT | 0666);
    semctl(semGet, 0, SETVAL, TRANSPORT_SIZE);

    sembuf up{0, 1, 0};
    sembuf down{0, -1, 0};

    auto* transport = (Transport*)
            ::mmap(nullptr, sizeof(Transport), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    transport->index = 0;

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            srand(getpid());
            while (true)
            {
                semop(semPut, &down, 1);
                printf("%d putting, count %d\n", getpid(), transport->index);
                int count = transport->index;
                transport->data[count++] = getpid();
                transport->index++;

                semop(semGet, &down, 1);
                if (count < TRANSPORT_SIZE)
                {
                    semop(semPut, &up, 1);
                }

                usleep(1000 * (rand() % 500));
            }

            exit(0);
        }
    }

    sembuf downmax{0, 0, 0};
    std::unordered_map<int, int> children;

    while (true)
    {
        semop(semGet, &downmax, 1);
        semctl(semGet, 0, SETVAL, TRANSPORT_SIZE);
        printf("parent extracting\n");
        transport->index = 0;
        for (int i = 0; i < TRANSPORT_SIZE; i++)
        {
            int pid = transport->data[i];
            if (children.find(pid) == children.end()) children.insert({pid, 0});
            children[pid]++;
        }

        for (auto& child: children)
        {
            printf("%d: %d\n", child.first, child.second);
        }
        semop(semPut, &up, 1);
    }

    while (waitpid(-1, nullptr, 0) >= 0);
}
void uloha3()
{
    auto* semPut = (sem_t*) mmap(nullptr, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(semPut, 1, 1);

    auto* semGet = (sem_t*) mmap(nullptr, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(semGet, 1, 1);

    auto* transport = (Transport*)
            ::mmap(nullptr, sizeof(Transport), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    transport->index = 0;

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            srand(getpid());
            while (true)
            {
                sem_wait(semPut);
                printf("%d putting, count %d\n", getpid(), transport->index);
                int count = transport->index;
                transport->data[count++] = getpid();
                transport->index++;

                sem_post(semGet);
                if (count < TRANSPORT_SIZE)
                {
                    sem_post(semPut);
                }

                usleep(1000 * (rand() % 800));
            }

            exit(0);
        }
    }

    std::unordered_map<int, int> children;

    while (true)
    {
        sem_wait(semGet);
        if (transport->index == TRANSPORT_SIZE)
        {
            printf("parent extracting\n");
            transport->index = 0;
            for (int i = 0; i < TRANSPORT_SIZE; i++)
            {
                int pid = transport->data[i];
                if (children.find(pid) == children.end()) children.insert({pid, 0});
                children[pid]++;
            }

            for (auto& child: children)
            {
                printf("%d: %d\n", child.first, child.second);
            }
            sem_post(semPut);
        }
    }

    while (waitpid(-1, nullptr, 0) >= 0);
}

int main()
{
    //uloha1();
    //uloha2();
    uloha3();

    return 0;
}
