#include <asm/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include "joystick.h"

struct js_event
{
	__u32 time;  /* event timestamp in milliseconds */
	__s16 value; /* value */
	__u8 type;   /* event type */
	__u8 number; /* axis/button number */
};

#define JS_EVENT_BUTTON 0x01 /* button pressed/released */
#define JS_EVENT_AXIS 0x02   /* joystick moved */
#define JS_EVENT_INIT 0x80   /* initial state of device */

int set_interface_attribs(int fd, int speed, int parity)
{
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	if (tcgetattr(fd, &tty) != 0)
	{
		std::cout << "error from tcgetattr " << errno;
		return -1;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK; // disable break processing
	tty.c_lflag = 0;		// no signaling chars, no echo,
							// no canonical processing
	tty.c_oflag = 0;		// no remapping, no delays
	tty.c_cc[VMIN] = 0;		// read doesn't block
	tty.c_cc[VTIME] = 5;	// 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
									   // enable reading
	tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		std::cout << "error from tcsetattr " << errno;
		return -1;
	}
	return 0;
}

int main()
{
	std::cout << "opening USB" << std::endl;
	int fdc = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_SYNC);
	set_interface_attribs(fdc, B115200, 0);

	ppm_file ppm(fdc);
	//ppm_stream ppm(std::cout);

	sleep(2);

	std::cout << "opening joystick (serial)" << std::endl;

	joystick joy("./mappings.ini", "./user_trims.ini", ppm);

	// open joystick
	//int fd = open("/dev/input/js0", O_RDONLY);

	//js_event e;
	std::string block;
    char buffer[64];
	const int blockSize = 7;

	int axis;
	int button;
	int value;


	// :1234A2:1234

	while (1)
	{
		// Read from serial if available
		while (read(fdc, buffer, sizeof buffer) > 0)
		{
			// buffer can contain:
			// Ax:vvvv where x is axis and v is a number from 0000 to 1024
			// Bx:vvvv where x is the button and v is either 0000 or 0001
			block += std::string(buffer);

			// Strip the start of the block if an incomplete message exists
			while (block.size() > 0 && (block[0] != 'A' && block[0] != 'B')) {
				// Incorrect start
				block = block.substr(1);
			}
			
			// Maybe now?
			if (block.length() >= blockSize) {
				if (block[0] == 'A')
				{
					// Axis message
					axis = std::stoi(block.substr(1,1));
					value = std::stoi(block.substr(3,4));
					joy.put_axis(axis, (float)value);
				}
				if (block[0] == 'B')
				{
					// Button message
					button = std::stoi(block.substr(1,1));
					value = std::stoi(block.substr(3,4));
					joy.put_button(button, value != 0);
				}

				// We are finished with this block
				block = block.substr(7);
			}
		}
	}

	// read from joystick
	// while(read(fd, &e, sizeof(e)) > 0)
	// {
	// 	e.type = e.type &~JS_EVENT_INIT;

	// 	if (e.type == JS_EVENT_AXIS)
	// 	{
	// 		joy.put_axis(e.number, (float)e.value / 32768.0f);
	// 	}
	// 	else if (e.type == JS_EVENT_BUTTON)
	// 	{
	// 		joy.put_button(e.number, e.value != 0);
	// 	}
	// 	else
	// 	{
	// 		std::cout << "unknown type " << (int)e.type;
	// 	}
	// }
	return 0;
}
