#include<fcntl.h>
#include<stdio.h>
#include<errno.h>
#include<math.h>
#include<sys/mman.h>
#include<linux/ioctl.h>
#define KYOUKO3_CONTROL_SIZE (65536)
#define Device_RAM (0x0020)
#define KYOUKO3_FRAMEBUFFER_SIZE (1024 * 764 *4)
#define PCI_VENDOR_ID_CCORSI 0x1234
#define PCI_DEVICE_ID_CCORSI_KYOUKO3 0x1113
#define KYOUKO_CONTROL_SIZE 65536
#define DEVICE_RAM 0x0020
#define PAGESHIFT 2
#define FIFO_START 0x1020
#define FIFO_END 0x1024
#define FIFO_ENTRIES 1024
#define FRAME_COLUMNS 0X8000
#define FRAME_ROWS 0X8004
#define FRAME_ROWPITCH 0X8008
#define FRAME_PIXELFORMAT 0X800C
#define FRAME_STARTADDRESS 0X8010
#define ENCODER_WIDTH 0X9000
#define ENCODER_HEIGHT 0X9004
#define ENCODER_BITX 0X9008
#define ENCODER_BITY 0X900C
#define ENCODER_FRAME 0X9010
#define ACCELERATION_BITMASK 0X1010
#define CONFIG_MODESET 0X1008
#define RED 0x5100
#define GREEN 0x5104
#define BLUE 0x5108
#define ALPHA 0x510C
#define RASTER_CLEAR 0x3008
#define RASTER_FLUSH 0x3FFC
#define FIFO_HEAD 0x4010
#define FIFO_TAIL 0x4014
#define VMODE _IOW(0xCC, 0, unsigned long)
#define FIFO_QUEUE _IOWR(0xCC, 3, unsigned long)
#define FIFO_FLUSH _IO(0XCC, 4)
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0


struct u_kyouko_device {
unsigned int *u_control_base;
unsigned int *u_frame_buffer;
}kyouko3;

void U_WRITE_REG(unsigned int rgister, unsigned int value)
{
*(kyouko3.u_frame_buffer+(rgister))=value;
}

unsigned int U_READ_REG(unsigned int rgister)
{
return(*(kyouko3.u_control_base+(rgister>>2)));
}

int main(void)
{
int fd;
int result;
int i =0;

fd= open("/dev/kyouko3", O_RDWR);
//mmap frame buffer
kyouko3.u_frame_buffer=mmap(0,KYOUKO3_FRAMEBUFFER_SIZE, PROT_READ|PROT_WRITE,MAP_SHARED,fd,0x8000000);
kyouko3.u_control_base=mmap(0,KYOUKO3_CONTROL_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
result= U_READ_REG(Device_RAM);
printf("Ram size in MB is: %d\n",result);
ioctl(fd,VMODE,GRAPHICS_ON);
for(i=200*1024;i<201*1024;i++)
{
U_WRITE_REG(i,0xff0000);
}
struct fifo_entry{
unsigned int command;
unsigned int value;
};
struct fifo_entry entry = {0x3FFC, 0};
ioctl(fd,FIFO_QUEUE,&entry);
ioctl(fd,FIFO_FLUSH);
sleep(5);
ioctl(fd,VMODE,GRAPHICS_OFF);
close(fd);
return 0; 
}

