#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cstdio>
#include <cstdlib>
#include <wait.h>
#include <mqueue.h>
#include <sys/mman.h>
#include <unordered_map>
#include <cstring>
#include "message_queue.h"

struct message
{
    long type;

    struct
    {
        union
        {
            int integer;
            float real;
        };
    } data;
};

#define TRANSPORT_SIZE (128)
#define CHILDREN_COUNT 10
#define TYPE_PUT 1
#define TYPE_GET 2
struct Transport
{
    int data[TRANSPORT_SIZE];
};

struct TransportMsg
{
    long type;
    int index;
};

void msg_systemv()
{
    int id = msgget(0xCAFECAFE, IPC_CREAT | 0660);

    if (fork() == 0)
    {
        srand((unsigned int) getpid());
        message msg{};

        for (int i = 0; i < 20; i++)
        {
            msg.type = rand() % 2 + 1;
            if (msg.type == 1)
            {
                msg.data.integer = rand();
            }
            else msg.data.real = rand() / (float) rand();
            msgsnd(id, &msg, sizeof(msg.data), 0);
            printf("send\n");
            usleep(1000 * 500 % rand());
        }

        msg.type = 3;
        msgsnd(id, &msg, sizeof(msg.data), 0);

        exit(0);
    }

    message msg{};
    while (true)
    {
        msgrcv(id, &msg, sizeof(msg.data), 0, 0);
        if (msg.type == 1)
        {
            printf("%d\n", msg.data.integer);
        }
        else if (msg.type == 2)
        {
            printf("%f\n", msg.data.real);
        }
        else break;
    }

    while (waitpid(-1, nullptr, 0) > 0);

    msgctl(id, IPC_RMID, nullptr);
}
void msg_posix()
{
    const char* name = "/CAFECAFE3";
    mq_attr attr{};
    attr.mq_msgsize = sizeof(message);
    attr.mq_maxmsg = 10;
    attr.mq_curmsgs = 0;
    mqd_t id = mq_open(name, O_CREAT | O_RDWR, 0660, &attr);

    if (fork() == 0)
    {
        srand((unsigned int) getpid());
        message msg{};

        for (int i = 0; i < 20; i++)
        {
            msg.type = rand() % 2 + 1;
            if (msg.type == 1)
            {
                msg.data.integer = rand();
            }
            else msg.data.real = rand() / (float) rand();
            mq_send(id, reinterpret_cast<const char *>(&msg), sizeof(message), 1);
            usleep(1000 * 500 % rand());
        }

        msg.type = 3;
        mq_send(id, reinterpret_cast<const char *>(&msg), sizeof(message), 1);

        exit(0);
    }

    message msg{};
    while (true)
    {
        unsigned int prio;
        mq_receive(id, reinterpret_cast<char *>(&msg), sizeof(message), &prio);
        if (msg.type == 1)
        {
            printf("%d\n", msg.data.integer);
        }
        else if (msg.type == 2)
        {
            printf("%f\n", msg.data.real);
        }
        else break;
    }

    while (waitpid(-1, nullptr, 0) > 0);

    mq_close(id);
    mq_unlink(name);
}

void uloha1_systemv()
{
    auto* transport = (Transport*) mmap(nullptr, sizeof(Transport),
                                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    int putQueue = msgget(0xCAFECAFE, IPC_CREAT | 0660);
    TransportMsg initMsg{};
    initMsg.type = TYPE_PUT;
    initMsg.index = 0;
    msgsnd(putQueue, &initMsg, sizeof(initMsg.index), 0);

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            srand(getpid());
            TransportMsg msg{};

            while (true)
            {
                msgrcv(putQueue, &msg, sizeof(msg.index), TYPE_PUT, 0);
                printf("%d putting, count %d\n", getpid(), msg.index);
                int count = msg.index;
                transport->data[count++] = getpid();
                msg.index++;

                if (count < TRANSPORT_SIZE)
                {
                    msg.type = TYPE_PUT;
                }
                else msg.type = TYPE_GET;
                msgsnd(putQueue, &msg, sizeof(msg.index), 0);

                usleep(1000 * (rand() % 800));
            }

            exit(0);
        }
    }

    std::unordered_map<int, int> children;
    TransportMsg msg{};

    while (true)
    {
        msgrcv(putQueue, &msg, sizeof(msg.index), TYPE_GET, 0);
        printf("parent extracting\n");
        msg.index = 0;
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

        msg.type = TYPE_PUT;
        msgsnd(putQueue, &msg, sizeof(msg.index), 0);
    }

    while (waitpid(-1, nullptr, 0) >= 0);
}
void uloha1_posix()
{
    auto* transport = (Transport*) mmap(nullptr, sizeof(Transport),
                                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    mq_attr attr{};
    attr.mq_msgsize = sizeof(TransportMsg);
    attr.mq_maxmsg = 10;
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;

    mq_unlink("/pvos_put");
    mq_unlink("/pvos_get");
    mqd_t putQueue = mq_open("/pvos_put", O_CREAT | O_RDWR, 0666, &attr);
    mqd_t getQueue = mq_open("/pvos_get", O_CREAT | O_RDWR, 0666, &attr);

    TransportMsg initMsg{};
    initMsg.index = 0;
    mq_send(putQueue, reinterpret_cast<const char *>(&initMsg), sizeof(TransportMsg), 1);

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            srand(getpid());
            TransportMsg msg{};
            unsigned int prio;

            while (true)
            {
                mq_receive(putQueue, reinterpret_cast<char *>(&msg), sizeof(TransportMsg), &prio);
                printf("%d putting, count %d\n", getpid(), msg.index);
                int count = msg.index;
                transport->data[count++] = getpid();
                msg.index++;

                if (count < TRANSPORT_SIZE)
                {
                    int ret = mq_send(putQueue, reinterpret_cast<const char *>(&msg), sizeof(TransportMsg), 1);
                }
                else mq_send(getQueue, reinterpret_cast<const char *>(&msg), sizeof(TransportMsg), 1);

                usleep(1000 * (rand() % 800));
            }

            exit(0);
        }
    }

    std::unordered_map<int, int> children;
    TransportMsg msg{};
    unsigned int prio;

    while (true)
    {
        mq_receive(getQueue, reinterpret_cast<char *>(&msg), sizeof(TransportMsg), &prio);
        printf("parent extracting\n");
        msg.index = 0;
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

        mq_send(putQueue, reinterpret_cast<const char *>(&msg), sizeof(TransportMsg), 1);
    }

    while (waitpid(-1, nullptr, 0) >= 0);
}
void uloha2()
{
    MessageQueue<TransportMsg> queue(0xCAFECAFE, 128, 8);

    for (int i = 0; i < 4; i++)
    {
        if (fork() == 0)
        {
            while (true)
            {
                srand(getpid());
                TransportMsg buffer{};
                int type = 1;
                buffer.type = type;

                printf("%d waiting for type %ld\n", getpid(), buffer.type);
                queue.get(&buffer);
                printf("%d got type %d, value %d\n", getpid(), buffer.type, buffer.index);
                if (type != 0 && type != buffer.type)
                {
                    printf("ERROR\n");
                    exit(1);
                }
            }
        }
    }

    int counter = 0;
    while (true)
    {
        TransportMsg msg{};
        msg.type = 1;
        msg.index = counter++;
        printf("putting\n");
        queue.put(&msg);
        usleep(1000 * (rand() % 200));
    }

    while (waitpid(-1, nullptr, 0) >= 0);
}

int main()
{
    //uloha1_systemv();
    //uloha1_posix();

    uloha2();

    return 0;
}
