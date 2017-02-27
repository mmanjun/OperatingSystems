# Device Driver-FIFO
Device driver for the Kyouko3 PCIe graphics card that resides in each of the virtual Linux machines on the corsi and epyon systems. The device driver is built as a kernel module and structured so that the card appears as a character device (major 500, minor 127) accessible through system calls open(), close(), mmap() and ioctl(). The driver delivers sufficient capability to the user level to enable the user to draw smooth-shaded triangles. Further, the driver provides this capability in two ways:
1. It enables drawing of single triangles directly, through the FIFO facility, using memory- mapped control registers, where the memory-mapping is invoked by a user-level system call to mmap().
2. It must enable drawing of large numbers of triangles indirectly, through the DMA facility, using driver-allocated DMA buffers that have been memory-mapped to user space.

The steps to be followed to run this software:

1. Create a symbolic link to the graphics card:
mknod /dev/kyouko3 c 500 127

2. Make the file:
make

3. Insert the module into the kernel:
/sbin/insmod mymod.ko

4. Run the user level code:
gcc user.c
./a.out

5. Remove the module from the kernel after seeing the output:
/sbin/rmmod mymod
