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
#define RED 0x5018
#define GREEN 0x5014
#define BLUE 0x5010
#define ALPHA 0x501C
#define RASTER_CLEAR 0x3008
#define RASTER_FLUSH 0x3FFC
#define FIFO_HEAD 0x4010
#define FIFO_TAIL 0x4014
#define VMODE _IOW(0xCC, 0, unsigned long)
#define FIFO_QUEUE _IOWR(0xCC, 3, unsigned long)
#define FIFO_FLUSH _IO(0XCC, 4)
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0
#define RASTER_PRIMITIVE 0x3000
#define X_COORDINATE 0X5000
#define Y_COORDINATE 0X5004
#define Z_COORDINATE 0X5008
#define W_COORDINATE 0X500C
#define VERTEX_EMIT  0x3004

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
//U_WRITE_REG(i,0xff0000);
}
float zero = 0.0;
float one = 1.0;	
float x1 = 0.0;
float y1 = -0.5;
float x2= -0.5;
float y2 = 0.5;
float x3 = 0.5;
float y3 = 0.5;
float a= 1.0;
float b=0.0;
float c=0.0;
float d = 0.0;
float e = 1.0;
float f = 0.0;
float g = 0.0;
float h = 0.0;
float it = 1.0;
unsigned int zero_zs_int = *(unsigned int*)&zero;
unsigned int one_us_int = *(unsigned int*)&one;
unsigned int x1_int = *(unsigned int*)&x1;
unsigned int y1_int = *(unsigned int*)&y1;
unsigned int x2_int = *(unsigned int*)&x2;
unsigned int y2_int = *(unsigned int*)&y2;
unsigned int x3_int = *(unsigned int*)&x3;
unsigned int y3_int = *(unsigned int*)&y3;
unsigned int aa = *(unsigned int*)&a;
unsigned int bb = *(unsigned int*)&b;
unsigned int cc = *(unsigned int*)&c;
unsigned int dd = *(unsigned int*)&d;
unsigned int ee = *(unsigned int*)&e;
unsigned int ff = *(unsigned int*)&f;
unsigned int gg = *(unsigned int*)&g;
unsigned int hh = *(unsigned int*)&h;
unsigned int ii = *(unsigned int*)&it;
struct fifo_entry{
unsigned int command;
unsigned int value;
};
struct fifo_entry entry[24] = {{RASTER_PRIMITIVE,1}, 
			      {X_COORDINATE, x1_int},
			      {Y_COORDINATE, y1_int},
			      {Z_COORDINATE, zero_zs_int},
			      {W_COORDINATE, one_us_int},
			      {X_COORDINATE, x2_int},
			      {Y_COORDINATE, y2_int},
			      {Z_COORDINATE, zero_zs_int},
			      {X_COORDINATE, x3_int},
			      {Y_COORDINATE, y3_int},
			      {Z_COORDINATE, zero_zs_int},
                              {BLUE, aa},
			      {GREEN, bb},
			      {RED, cc},
                              {ALPHA, zero_zs_int},
			      {VERTEX_EMIT,0},
			      {RASTER_PRIMITIVE,0},
			      {RASTER_FLUSH,0},
			      {BLUE, dd},
			      {GREEN, ee},
			      {RED, ff},
                              {BLUE, gg},
			      {GREEN, hh},
			      {RED, ii},
                              };	
ioctl(fd, FIFO_QUEUE, &entry[0]);
ioctl(fd, FIFO_QUEUE, &entry[1]);
ioctl(fd, FIFO_QUEUE, &entry[2]);
ioctl(fd, FIFO_QUEUE, &entry[3]);
ioctl(fd, FIFO_QUEUE, &entry[4]);
ioctl(fd, FIFO_QUEUE, &entry[11]);
ioctl(fd, FIFO_QUEUE, &entry[12]);
ioctl(fd, FIFO_QUEUE, &entry[13]);
ioctl(fd, FIFO_QUEUE, &entry[14]);
ioctl(fd, FIFO_QUEUE, &entry[15]);
ioctl(fd, FIFO_QUEUE, &entry[5]);
ioctl(fd, FIFO_QUEUE, &entry[6]);
ioctl(fd, FIFO_QUEUE, &entry[7]);
ioctl(fd, FIFO_QUEUE, &entry[18]);
ioctl(fd, FIFO_QUEUE, &entry[19]);
ioctl(fd, FIFO_QUEUE, &entry[20]);
ioctl(fd, FIFO_QUEUE, &entry[15]);
ioctl(fd, FIFO_QUEUE, &entry[8]);
ioctl(fd, FIFO_QUEUE, &entry[9]);
ioctl(fd, FIFO_QUEUE, &entry[10]);
ioctl(fd, FIFO_QUEUE, &entry[21]);
ioctl(fd, FIFO_QUEUE, &entry[22]);
ioctl(fd, FIFO_QUEUE, &entry[23]);
ioctl(fd, FIFO_QUEUE, &entry[15]);
ioctl(fd, FIFO_QUEUE, &entry[16]);
ioctl(fd, FIFO_QUEUE, &entry[17]);
ioctl(fd,FIFO_FLUSH);
sleep(5);
ioctl(fd,VMODE,GRAPHICS_OFF);
close(fd);
return 0; 
}

