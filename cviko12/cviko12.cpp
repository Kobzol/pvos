#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <poll.h>
#include "../cviko3/reader.h"

#define WRAP_ERROR(ret)\
    if ((ret) < 0)\
    {\
        fprintf(stderr, "Error: %s\n", strerror(errno));\
        exit(1);\
    }

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("device\n");
        return 1;
    }

    termios options{};

    int usb = open(argv[1], O_RDWR);
    WRAP_ERROR(usb);

    WRAP_ERROR(tcgetattr(usb, &options));
    cfmakeraw(&options);
    WRAP_ERROR(cfsetspeed(&options, B115200));
    WRAP_ERROR(tcsetattr(usb, TCSAFLUSH, &options));

    pollfd polls[2];
    polls[0].fd = STDIN_FILENO;
    polls[0].events = POLLIN;
    polls[1].fd = usb;
    polls[1].events = POLLIN | POLLHUP;

    char buffer[256];
    Reader reader;
    while (true)
    {
        int ret = poll(polls, 2, 1000);
        if (ret >= 0)
        {
            if (polls[0].revents & POLLIN)
            {
                ssize_t len = read(STDIN_FILENO, buffer, sizeof(buffer));
                WRAP_ERROR(len);
                WRAP_ERROR(write(usb, buffer, len));
            }

            if (polls[1].revents & POLLHUP) break;
            else if (polls[1].revents & POLLIN)
            {
                if (reader.readline(usb, buffer, sizeof(buffer)) < 0) break;
                fprintf(stderr, "%s", buffer);
            }
        }
        else break;
    }

    printf("Communication ended\n");

    WRAP_ERROR(close(usb));

    return 0;
}
