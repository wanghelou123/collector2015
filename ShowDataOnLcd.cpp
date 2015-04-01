#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

#define	CMD_CLEAR	1


int main(int argc, char *argv[])
{
	int fd = open("/dev/LCD1602", O_RDWR | O_NOCTTY | O_NDELAY );
	int cmd=0;
	int ret  = 0;
	char buffer[64];
	unsigned char tmp_buffer[64]={0};
	unsigned char line1[17]={0};
	unsigned char line2[17]={0};
	if(fd == -1) {
		perror("open");
		exit(-1);
	}
	while(1) {
		memset(buffer, 0, sizeof(buffer));
		memset(tmp_buffer, 0, sizeof(tmp_buffer));
		printf("Please input command\n 1:Read The LCD Screen conent\n 2:Write Content to the LCD\n 3: clear the LCD Screen\n");
		scanf("%d", &cmd);	
		printf("cmd:%d\n", cmd);

		switch(cmd){
			case 1:
				ret = read(fd, tmp_buffer, 32);
				printf("read %d bytes from LCD.\n", ret);
				memcpy(line1, tmp_buffer, 16);
				memcpy(line2, tmp_buffer+16, 16);
				printf("%s\n", line1);
				printf("%s\n", line2);

			break;

			case 2:
				printf("Please input you want to show content:\n");
				scanf("%s", buffer);
				write(fd, buffer, strlen(buffer));
			break;
			case 3:
				ret = ioctl(fd,  _IO('m',  CMD_CLEAR), 0);
				if(ret == -1 ) {
					perror("ioctl");
				}
			break;
		}

	}


	close(fd);

	return 0;
}
