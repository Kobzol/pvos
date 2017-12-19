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
    explicit Reader(int fd): fd(fd)
    {

    }
    virtual ~Reader() = default;

    ssize_t readline(char* buffer, size_t max_length)
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

            return (ssize_t) std::min(max_length, count + 1); // +1 because of null termination
        }
        else
        {
            int read_count = sizeof(this->line_buf) - this->line_index;
            auto len = this->read(this->line_buf + this->line_index, static_cast<size_t>(read_count));
            if (len > 0)
            {
                this->line_index += len;
                this->line_buf[this->line_index] = '\0';

                return this->readline(buffer, max_length);
            }
            else return len;
        }
    }

    virtual void dispose()
    {

    }

    virtual ssize_t read(char* buffer, size_t size)
    {
        return ::read(this->fd, buffer, size);
    }
    virtual ssize_t write(const char* buffer, size_t size)
    {
        return ::write(this->fd, buffer, size);
    }

    ssize_t tm_to_ms(timeval* tm)
    {
        return tm->tv_sec * 1000 + tm->tv_usec / 1000;
    }

    char line_buf[1024] = { 0 };
    size_t line_index = 0;
    int fd;
};

class SSLReader: public Reader
{
public:
    SSLReader(int fd, SSL* ctx): Reader(fd), ctx(ctx)
    {

    }

    ssize_t read(char* buffer, size_t size) override
    {
        ssize_t ret = SSL_read(this->ctx, buffer, size);
        if (ret == -1 && errno != EAGAIN) { ERR_print_errors_fp(stderr); exit(2); }
        return ret;
    }
    ssize_t write(const char* buffer, size_t size) override
    {
        ssize_t ret = SSL_write(this->ctx, buffer, size);
        if (ret == -1 && errno != EAGAIN) { ERR_print_errors_fp(stderr); exit(2); }
        return ret;
    }

    void dispose() override
    {
        SSL_free(this->ctx);
    }

    SSL* ctx = nullptr;
};
