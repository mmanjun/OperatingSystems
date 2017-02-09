#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/delay.h>
#include<linux/kernel_stat.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/pci.h>
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
#define RED 0x5108
#define GREEN 0x5104
#define BLUE 0x5100
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
MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Mansi Pratz");

struct fifo_entry{
u32 command;
u32 value;
};

struct fifo{
u64 p_base;
struct fifo_entry *k_base;
u32 head;
u32 tail_cache;
}fifo;


struct kyouko3_struct
{ 
	struct pci_dev *pci_dev_saved;
	unsigned int *k_control_base;
	unsigned int *k_card_ram_base;
	unsigned int p_card_ram_base;
	unsigned int p_control_base;
        unsigned int graphics_on;
	unsigned int dma_mapped;
        unsigned int graphics_off;
        struct fifo fifo;
}kyouko3;

unsigned int K_READ_REG(unsigned int reg)
{
	unsigned int value;
	rmb();
	value= *(kyouko3.k_control_base+(reg>>2));
	return(value);
}
void K_WRITE_REG(unsigned int reg,unsigned int value)
{
	*(kyouko3.k_control_base+(reg>>2))=value;
}

int kyouko3_open(struct inode *inode, struct file *fp)
{
	unsigned long ram_size;
	ram_size=K_READ_REG(DEVICE_RAM);
	ram_size *= (1024*1024);
	kyouko3.k_control_base= ioremap(kyouko3.p_control_base,KYOUKO_CONTROL_SIZE);
	kyouko3.k_card_ram_base = ioremap(kyouko3.p_card_ram_base,DEVICE_RAM);
	kyouko3.fifo.k_base= pci_alloc_consistent(kyouko3.pci_dev_saved, 8192u, &kyouko3.fifo.p_base);
	K_WRITE_REG(FIFO_START,kyouko3.fifo.p_base);
	K_WRITE_REG(FIFO_END,kyouko3.fifo.p_base+8192u);
	kyouko3.fifo.head=0;
	kyouko3.fifo.tail_cache=0;
	//include late-set flags
	kyouko3.graphics_on=0;
	kyouko3.dma_mapped=0;
	printk(KERN_ALERT "Opened device\n");
	return 0;
}

void FIFO_WRITE(unsigned int reg, unsigned int value){
	kyouko3.fifo.k_base[kyouko3.fifo.head].command=reg;
	kyouko3.fifo.k_base[kyouko3.fifo.head].value=value;
	kyouko3.fifo.head++;
	if(kyouko3.fifo.head >=FIFO_ENTRIES){
	kyouko3.fifo.head=0;
	}
}
void fifo_flush()
{
	K_WRITE_REG(FIFO_HEAD, kyouko3.fifo.head);
	while(kyouko3.fifo.tail_cache!=kyouko3.fifo.head){
	kyouko3.fifo.tail_cache=K_READ_REG(FIFO_TAIL);
	schedule();
	}
}
void kyouko3_ioctl(struct file *fp, unsigned int cmd, unsigned long arg){
	float one = 1.0;
	float zero = 0.0;
	unsigned int one_us_int = *(unsigned int*)&one;
	unsigned int zero_zs_int = *(unsigned int*)&zero;
	switch(cmd){
                int ret;
		case FIFO_QUEUE:
				
			       ret = copy_from_user(kyouko3.fifo.k_base,(struct fifo_entry *)arg, sizeof(struct fifo_entry));
				FIFO_WRITE(kyouko3.fifo.k_base->command,kyouko3.fifo.k_base->value);
				break;                                    
		case FIFO_FLUSH:
				fifo_flush();
				break;
		case VMODE:
		if((int)(arg)==GRAPHICS_ON){
		K_WRITE_REG(FRAME_COLUMNS, 1024);
		K_WRITE_REG(FRAME_ROWS, 768);
		K_WRITE_REG(FRAME_ROWPITCH, 1024*4);
		K_WRITE_REG(FRAME_PIXELFORMAT, 0xf888);
		K_WRITE_REG(FRAME_STARTADDRESS, 0);
		K_WRITE_REG(ACCELERATION_BITMASK, 0X40000000);
		K_WRITE_REG(ENCODER_WIDTH, 1024);
		K_WRITE_REG(ENCODER_HEIGHT, 768);
		K_WRITE_REG(ENCODER_BITX, 0);
		K_WRITE_REG(ENCODER_BITY, 0);
		K_WRITE_REG(ENCODER_FRAME, 0);
		K_WRITE_REG(CONFIG_MODESET, 0);
		
		msleep(10);
		
		FIFO_WRITE(RED, zero_zs_int);
		FIFO_WRITE(GREEN, zero_zs_int);
		FIFO_WRITE(BLUE,  one_us_int) ;
		FIFO_WRITE(ALPHA, zero_zs_int);
		FIFO_WRITE(RASTER_CLEAR, 0x03);
		FIFO_WRITE(RASTER_FLUSH, 0x0);
	        fifo_flush();
		kyouko3.graphics_on=1;
		}
		else if((int)arg==GRAPHICS_OFF)
		{
		  FIFO_WRITE(RASTER_CLEAR, 0x03);
	 	  FIFO_WRITE(RASTER_FLUSH, 0x0);
		  K_WRITE_REG(ACCELERATION_BITMASK, 0X80000000);
		  K_WRITE_REG(CONFIG_MODESET,0);
		  fifo_flush();
		  kyouko3.graphics_on=0;
		}
	}
}

int kyouko3_release(struct inode *inode, struct file *fp)
{
	kyouko3_ioctl(fp, VMODE, GRAPHICS_OFF);
	fifo_flush();
	pci_free_consistent(kyouko3.pci_dev_saved,8192u,kyouko3.fifo.k_base,kyouko3.fifo.p_base);
	//do_munmap();
	iounmap(kyouko3.p_control_base);
	iounmap(kyouko3.p_card_ram_base);
	printk(KERN_ALERT "BUUH BYE\n");
	return 0;
}

int kyouko3_mmap(struct file*fp, struct vm_area_struct *vma)
{
int ret;
switch(vma->vm_pgoff<< PAGE_SHIFT)
{
case 0:
ret = io_remap_pfn_range(vma,vma->vm_start, kyouko3.p_control_base>>PAGE_SHIFT,vma->vm_end - vma->vm_start,vma->vm_page_prot);
break;

case 0x8000000:
ret = io_remap_pfn_range(vma,vma->vm_start, kyouko3.p_card_ram_base>>PAGE_SHIFT,vma->vm_end - vma->vm_start,vma->vm_page_prot);
break;

return ret;
}
}

struct file_operations kyouko3_fops = {
 .open= kyouko3_open,
 .release= kyouko3_release,
 .unlocked_ioctl=kyouko3_ioctl,
 .mmap= kyouko3_mmap,
 .owner= THIS_MODULE
};

struct cdev kyouko3_cdev;

int kyouko3_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id)
{
       	int enable_int_value;  
	kyouko3.pci_dev_saved=pci_dev;
	kyouko3.p_control_base=pci_resource_start(pci_dev,1);
	kyouko3.p_card_ram_base= pci_resource_start(pci_dev,2);
	enable_int_value=pci_enable_device(pci_dev);  
	pci_set_master(pci_dev);
	printk(KERN_ALERT "This is probe function\n");
	//pci_dev=irq;
	return 0;
}


int kyouko3_remove(struct pci_dev *pci_dev)
{
	pci_disable_device(pci_dev);
	return 0;
}

struct pci_device_id kyouko3_dev_ids[]={
	{PCI_DEVICE(PCI_VENDOR_ID_CCORSI, PCI_DEVICE_ID_CCORSI_KYOUKO3)},
	{0}
};


struct pci_driver kyouko3_pci_drv={
	.name="whatever",
	.id_table= kyouko3_dev_ids,
	.probe= kyouko3_probe,
	.remove= kyouko3_remove
};

int my_init_function(void)
{
	int register_int_value;
	cdev_init(&kyouko3_cdev,&kyouko3_fops);
	cdev_add(&kyouko3_cdev,MKDEV(500,127),1);
	register_int_value=pci_register_driver(&kyouko3_pci_drv);
	printk(KERN_ALERT "This is init function\n");
	return 0;
}
module_init(my_init_function);
void my_exit_function(void)
{
	pci_unregister_driver(&kyouko3_pci_drv);
	cdev_del(&kyouko3_cdev);
	printk(KERN_ALERT "This is exit function\n");
	return;
}
module_exit(my_exit_function);

