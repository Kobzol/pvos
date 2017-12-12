#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cerrno>
#include <cstring>

// 0 - write, 1 - read

#define CHECK(ret)\
	if (ret < 0)\
	{\
		fprintf(stderr, "%d: %s\n", __LINE__, strerror(errno));\
	}\

struct rgb
{
	rgb() = default;
	rgb(uint8_t r, uint8_t g, uint8_t b): r(r), g(g), b(b) {}
	uint8_t r = 0, g = 0, b = 0;
};

void i2cWrite(int fd, uint8_t address)
{
	CHECK(ioctl(fd, I2C_SLAVE, address));
}
uint8_t readRegister(int fd, int address)
{
	uint8_t buf = address;
	CHECK(write(fd, &buf, sizeof(buf)));

	int ret = read(fd, &buf, sizeof(buf));
	CHECK(ret);

	return buf;
}
void writeRegister(int fd, uint8_t address, uint8_t value)
{
	uint8_t buf[2] = { address, value };

	CHECK(write(fd, buf, sizeof(buf)));
}
void setLed(int fd, int x, int y, rgb color)
{
	i2cWrite(fd, 0x46);
	uint8_t address_r = 24 * x + y;
	uint8_t address_g = address_r + 8;
	uint8_t address_b = address_r + 16;

	uint8_t cmd[3][2] = {
			{ address_r, color.r },
			{ address_g, color.g },
			{ address_b, color.b }
	};
	for (int i = 0; i < 3; i++)
	{
		CHECK(write(fd, cmd + i, 2));
	}
}

int main()
{
	int fd = open("/dev/i2c-2", O_RDWR);
	CHECK(fd);

	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
		{
			setLed(fd, i, j, rgb(0, 0, 0));
		}
	}

	int addr = 0x5f; // temperature
	i2cWrite(fd, addr);

	writeRegister(fd, 0x20, 0x85);
	while (true)
	{
		i2cWrite(fd, addr);
		int status = readRegister(fd, 0x27);
		if (status & 0x1)
		{
			uint16_t low = readRegister(fd, 0x2A);
			uint16_t high = readRegister(fd, 0x2B);
			uint16_t value = ((int) high << 8) | low;
			if (high & 0x80)
			{
				value = ~(uint16_t)value + 1;
			}
			printf("%d\n", value);

			uint16_t t0degc = readRegister(fd, 0x32);
			uint16_t t1degc = readRegister(fd, 0x33);

			uint8_t t01msb = readRegister(fd, 0x35);
			t0degc |= (t01msb & 0b00000011) << 8;
			t1degc |= (t01msb & 0b00001100) << 6;
			t0degc /= 8;
			t1degc /= 8;

			uint8_t t0low = readRegister(fd, 0x3C);
			uint8_t t0high = readRegister(fd, 0x3D);
			int16_t t0_out = (t0high << 8) | t0low;
			if (t0high & 0x80)
			{
				t0_out = (~t0_out) + 1;
			}

			uint8_t t1low = readRegister(fd, 0x3E);
			uint8_t t1high = readRegister(fd, 0x3F);
			int16_t t1_out = (t1high << 8) | t1low;
			if (t1high & 0x80)
			{
				t1_out = (~t1_out) + 1;
			}

			printf("T0 degc: %d\n", t0degc);
			printf("T1 degc: %d\n", t1degc);
			printf("T0 out: %d\n", t0_out);
			printf("T1 out: %d\n", t1_out);

			float a = t1degc - t0degc;
			float b = value - t0_out;
			float c = t1_out - t0_out;

			float temperature = ((a * b) / c) + t0degc;
			printf("%f\n", temperature);

			float r = (temperature - 28.0f) / (6.0f);
			int count = 8 * r;

			for (int i = 0; i < 8; i++)
			{
				setLed(fd, 0, i, rgb(0, 0, 0));
			}
			for (int i = 0; i < count; i++)
			{
				setLed(fd, 0, i, rgb(60, 0, 0));
			}

			sleep(1);
		}
	}

	return 0;
}
