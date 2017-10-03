#pragma once

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/time.h>

class Reader
{
public:
    int readline(int fd, char* buffer, size_t max_length)
    {
        char* pos = strchr(this->line_buf, '\n');
        if (pos != nullptr)
        {
            size_t count = (pos - this->line_buf) + 1;    // +1 to skip over newline

            memcpy(buffer, this->line_buf, std::min(count, max_length - 1));
            buffer[count] = '\0';

            if (count < line_index)
            {
                memcpy(this->line_buf, this->line_buf + count, this->line_index - count);
                this->line_index = this->line_index - count;
            }
            else
            {
                this->line_buf[0] = '\0';
                this->line_index = 0;
            }

            return (int) std::min(max_length, count + 1); // +1 because of null termination
        }
        else
        {
            int read_count = sizeof(this->line_buf) - this->line_index;
            auto len = static_cast<int>(read(fd, this->line_buf + this->line_index, static_cast<size_t>(read_count)));

            if (len > 0)
            {
                this->line_index += len;
                this->line_buf[this->line_index] = '\0';

                return this->readline(fd, buffer, max_length);
            }
            else return 0;
        }
    }
    int readlineSelect(int fd, char* buffer, size_t max_length, ssize_t timeout_ms)
    {
        printf("%d ms left\n", timeout_ms);

        char* pos = strchr(this->line_buf, '\n');
        if (pos != nullptr)
        {
            size_t count = (pos - this->line_buf) + 1;    // +1 to skip over newline

            memcpy(buffer, this->line_buf, std::min(count, max_length - 1));
            buffer[count] = '\0';

            if (count < line_index)
            {
                memcpy(this->line_buf, this->line_buf + count, this->line_index - count);
                this->line_index = this->line_index - count;
            }
            else
            {
                this->line_buf[0] = '\0';
                this->line_index = 0;
            }

            return (int) std::min(max_length, count + 1); // +1 because of null termination
        }
        else
        {
            fd_set readset{};
            FD_ZERO(&readset);
            FD_SET(fd, &readset);

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = timeout_ms * 1000;

            int ret = select(fd + 1, &readset, nullptr, nullptr, &timeout);
            if (ret > 0 && FD_ISSET(fd, &readset))
            {
                int read_count = sizeof(this->line_buf) - this->line_index;
                auto len = read(fd, this->line_buf + this->line_index, static_cast<size_t>(read_count));

                if (len > 0)
                {
                    this->line_index += len;
                    this->line_buf[this->line_index] = '\0';

                    return this->readlineSelect(fd, buffer, max_length, this->tm_to_ms(&timeout));
                }
                else return 0;
            }
            else if (ret == 0)
            {
                throw "Timeout!";
            }
        }
    }
    int readlinePoll(int fd, char* buffer, size_t max_length, ssize_t timeout_ms)
    {
        printf("%d ms left\n", timeout_ms);

        char* pos = strchr(this->line_buf, '\n');
        if (pos != nullptr)
        {
            size_t count = (pos - this->line_buf) + 1;    // +1 to skip over newline

            memcpy(buffer, this->line_buf, std::min(count, max_length - 1));
            buffer[count] = '\0';

            if (count < line_index)
            {
                memcpy(this->line_buf, this->line_buf + count, this->line_index - count);
                this->line_index = this->line_index - count;
            }
            else
            {
                this->line_buf[0] = '\0';
                this->line_index = 0;
            }

            return (int) std::min(max_length, count + 1); // +1 because of null termination
        }
        else
        {
            struct pollfd polls[1];
            polls[0].fd = fd;
            polls[0].events = POLLIN;

            timeval time{};
            gettimeofday(&time, nullptr);

            int ret = poll(polls, 1, timeout_ms);
            if (ret > 0 && (polls[0].revents & POLLIN) != 0)
            {
                int read_count = sizeof(this->line_buf) - this->line_index;
                auto len = read(fd, this->line_buf + this->line_index, static_cast<size_t>(read_count));

                if (len > 0)
                {
                    this->line_index += len;
                    this->line_buf[this->line_index] = '\0';

                    timeval currtime{};
                    gettimeofday(&currtime, nullptr);

                    timeval result{};
                    timersub(&currtime, &time, &result);

                    int left = timeout_ms - this->tm_to_ms(&result);

                    if (left < 0) throw "Timeout";

                    return this->readlinePoll(fd, buffer, max_length, left);
                }
                else return 0;
            }
            else if (ret == 0)
            {
                throw "Timeout!";
            }
        }
    }
    int readlineNonblock(int fd, char* buffer, size_t max_length, ssize_t timeout_ms)
    {
        if (timeout_ms < 0)
        {
            throw "Timeout";
        }

        printf("%d ms left\n", timeout_ms);

        char* pos = strchr(this->line_buf, '\n');
        if (pos != nullptr)
        {
            size_t count = (pos - this->line_buf) + 1;    // +1 to skip over newline

            memcpy(buffer, this->line_buf, std::min(count, max_length - 1));
            buffer[count] = '\0';

            if (count < line_index)
            {
                memcpy(this->line_buf, this->line_buf + count, this->line_index - count);
                this->line_index = this->line_index - count;
            }
            else
            {
                this->line_buf[0] = '\0';
                this->line_index = 0;
            }

            return (int) std::min(max_length, count + 1); // +1 because of null termination
        }
        else
        {
            int read_count = sizeof(this->line_buf) - this->line_index;

            timeval time{};
            gettimeofday(&time, nullptr);

            auto len = read(fd, this->line_buf + this->line_index, static_cast<size_t>(read_count));
            if (len < 0 && errno == EAGAIN)
            {
                usleep(10 * 1000);

                timeval currtime{};
                gettimeofday(&currtime, nullptr);

                timeval result{};
                timersub(&currtime, &time, &result);

                return this->readlineNonblock(fd, buffer, max_length, timeout_ms - this->tm_to_ms(&result));
            }
            else if (len > 0)
            {
                this->line_index += len;
                this->line_buf[this->line_index] = '\0';

                timeval currtime{};
                gettimeofday(&currtime, nullptr);

                timeval result{};
                timersub(&currtime, &time, &result);

                return this->readlineNonblock(fd, buffer, max_length, timeout_ms - this->tm_to_ms(&result));
            }
            else return 0;
        }
    }

private:
    ssize_t tm_to_ms(timeval* tm)
    {
        return tm->tv_sec * 1000 + tm->tv_usec / 1000;
    }

    char line_buf[1024] = { 0 };
    size_t line_index = 0;
};
