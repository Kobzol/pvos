#pragma once

#include <sys/sem.h>

#define MUTEX 0
#define SIZE 1

template <typename T>
class MessageQueue
{
public:
    MessageQueue(int id, int messageCount, int typeCount)
            : messageCount(messageCount), typeCount(typeCount)
    {
        this->messageMemory = (T*) mmap(nullptr, sizeof(T) * messageCount,
                                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        this->index = (int*) mmap(nullptr, sizeof(int),
                                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        this->semaphores = semget(id, typeCount + 2, IPC_CREAT | 0666);

        semctl(this->semaphores, MUTEX, SETVAL, 1);
        semctl(this->semaphores, SIZE, SETVAL, 0);
        for (int i = 2; i < typeCount + 2; i++)
        {
            semctl(this->semaphores, i, SETVAL, 0);
        }
    }
    ~MessageQueue()
    {
        munmap(this->messageMemory, sizeof(T) * this->messageCount);

        for (int i = 0; i < this->typeCount + 2; i++)
        {
            semctl(this->semaphores, i, IPC_RMID);
        }
    }

    MessageQueue(const MessageQueue& other) = delete;
    MessageQueue(const MessageQueue&& other) = delete;
    MessageQueue operator=(const MessageQueue& other) = delete;

    int put(T* buffer)
    {
        int type = buffer->type;
        this->down(MUTEX);

        if (*this->index >= this->messageCount)
        {
            this->up(MUTEX);
            return -1;
        }

        memcpy(this->messageMemory + *this->index, buffer, sizeof(T));
        *this->index = *this->index + 1;

        this->up(SIZE);
        this->up(type + 1);
        this->up(MUTEX);

        return 0;
    }
    int get(T* buffer)
    {
        int type = buffer->type;
        if (type != 0)
        {
            this->down(type + 1);
        }

        this->down(SIZE);
        this->down(MUTEX);

        int found = false;
        for (int i = 0; i < *this->index; i++)
        {
            T* item = this->messageMemory + i;
            if (type == 0 || item->type == type)
            {
                memcpy(buffer, item, sizeof(T));
                memmove(item, item + 1, sizeof(T) * (*this->index - i - 1));
                *this->index = *this->index - 1;
                break;
            }
        }

        if (type == 0)
        {
            this->down(buffer->type + 1);
        }

        this->up(MUTEX);

        return 0;
    }

private:
    void down(int index)
    {
        sembuf buf{};
        buf.sem_op = -1;
        buf.sem_num = index;
        buf.sem_flg = 0;
        int ret = semop(this->semaphores, &buf, 1);
        if (ret < 0) printf("%s\n", strerror(errno));
    }
    void up(int index)
    {
        sembuf buf{};
        buf.sem_op = 1;
        buf.sem_num = index;
        buf.sem_flg = 0;
        int ret = semop(this->semaphores, &buf, 1);
        if (ret < 0) printf("%s\n", strerror(errno));
    }

    int messageCount;
    int typeCount;

    T* messageMemory;
    int* index;

    int semaphores;
};
