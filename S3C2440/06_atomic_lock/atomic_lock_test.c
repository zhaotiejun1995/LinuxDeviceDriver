#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


int fd;

void signal_fun(int signum)
{
    unsigned char key_val;
    read(fd, &key_val, 1);
    printf("key_val: 0x%x\n", key_val);
}

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret;
    int oflags;
//    signal(SIGIO, signal_fun);
    
	fd = open("/dev/atomic_lock", O_RDWR | O_NONBLOCK);
	if (fd < 0)
	{
		printf("can't open!\n");
		return -1;
	}

//	fcntl(fd, F_SETOWN, getpid()); //告诉内核
//	oflags = fcntl(fd, F_GETFL);
//	fcntl(fd, F_SETFL, oflags | FASYNC);//改变fasync，会调用驱动中的fasync函数指针对应的函数。

	while (1)
	{   
        read(fd, &key_val, 1);
        printf("key_val: 0x%x\n", key_val);
	    sleep(5);
	}
	return 0;
}


