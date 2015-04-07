#include<stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include<linux/ppdev.h>
#include<stdlib.h>
main()
{
  int fd;
	unsigned char r;
	unsigned int inp;
	do {
		printf("Enter 1 to set, 0 to reset:\n");
		scanf("%u",&inp);
	}while(inp>1);		
	fd=open("/dev/parport0",O_RDWR);
	if(fd==-1)
	{
		perror("open");
		exit(0);
	}
	if(ioctl(fd,PPCLAIM) !=0)
	{
		perror("ioctl");
		close(fd);
		exit(0);
	}	
	if(ioctl(fd,PPRCONTROL,&r)!=0)
	{
		perror("ioctl");
		if(ioctl(fd,PPRELEASE) !=0)
			perror("ioctl");
		close(fd);
		exit(0);
	}
	printf("data = %x\n",r);
	r &= ~(0x01); //STROBE is first bit of control
	r |= (!inp);
	if(ioctl(fd, PPWCONTROL, &r) !=0)
		perror("ioctl");
	if(ioctl(fd,PPRDATA,&r)!=0)
        {
                perror("ioctl");
                if(ioctl(fd,PPRELEASE) !=0)
                        perror("ioctl");
                close(fd);
                exit(0);
        }
	r &= ~(1<<4); //D4 pin
	r |= (inp << 4);	
	if(ioctl(fd, PPWDATA, &r) !=0)
		perror("ioctl");
	if(ioctl(fd,PPRELEASE) !=0)
		perror("ioctl");
	close(fd);
}
