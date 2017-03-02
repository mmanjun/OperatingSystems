#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/delay.h>
#include<linux/kernel_stat.h>
#include<linux/fs.h>
#include<linux/sched.h>
#include<linux/cdev.h>
#include<linux/pci.h>
#include<linux/types.h>
#include<linux/mm.h>
#include<linux/mman.h>
#include "kyouko3.h"

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Bhavya Manasa Prateek Pratyush");

DECLARE_WAIT_QUEUE_HEAD(dma_snooze);

spinlock_t lock;

/* Function to map the driver */
int kyouko3_mmap(struct file*fp, struct vm_area_struct *vma);

/* Function to probe the device */
int kyouko3_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id);

/* Function to remove the driver */
int kyouko3_remove(struct pci_dev *pci_dev);

/* Function to release the driver */					
int kyouko3_release(struct inode *inode, struct file *fp);

/* IOCTL function for the driver */
void kyouko3_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);

/* Function to open the driver */
int kyouko3_open(struct inode *inode, struct file *fp);

/* Structures */

/* Driver structures */
struct pci_device_id kyouko3_dev_ids[]=
{
  {PCI_DEVICE(PCI_VENDOR_ID_CCORSI, PCI_DEVICE_ID_CCORSI_KYOUKO3)},
  {0}
};

struct pci_driver kyouko3_pci_drv=
{
  .name     = "whatever",
  .id_table = kyouko3_dev_ids,
  .probe    = kyouko3_probe,
  .remove   = kyouko3_remove
};

/* File operation structures */
struct file_operations kyouko3_fops = 
{
  .open           = kyouko3_open,
  .release        = kyouko3_release,
  .unlocked_ioctl = kyouko3_ioctl,
  .mmap           = kyouko3_mmap,
  .owner          = THIS_MODULE
};

/* Device empty structure */
struct cdev kyouko3_cdev;

/* Function to read registers */
unsigned int K_READ_REG(unsigned int reg)
{
  unsigned int value;
  rmb();
  value = *(kyouko3.k_control_base+(reg>>2));
  return(value);
}

/* Function to write register */
void K_WRITE_REG(unsigned int reg,unsigned int value)
{
  *(kyouko3.k_control_base+(reg>>2)) = value;
}

/* Function to open the driver */
int kyouko3_open(struct inode *inode, struct file *fp)
{
  unsigned long ram_size;
  ram_size   		  =  K_READ_REG(DEVICE_RAM);
  ram_size   		 *= (1024*1024);
  kyouko3.k_control_base  = ioremap(kyouko3.p_control_base,KYOUKO_CONTROL_SIZE);
  kyouko3.k_card_ram_base = ioremap(kyouko3.p_card_ram_base,DEVICE_RAM);
  kyouko3.fifo.k_base     = pci_alloc_consistent(kyouko3.pci_dev_saved, 124000, &kyouko3.fifo.p_base);

  K_WRITE_REG(FIFO_START,kyouko3.fifo.p_base);
  K_WRITE_REG(FIFO_END,kyouko3.fifo.p_base + 8192u);

  kyouko3.fifo.head	  = 0;
  kyouko3.fifo.tail_cache = 0;
  
  kyouko3.graphics_on     = 0;
  kyouko3.dma_mapped	  = 0;
  printk(KERN_ALERT "Opened device\n");
  return 0;
}

/* Function to write into FIFO */
void FIFO_WRITE(unsigned int reg, unsigned int value)
{
  kyouko3.fifo.k_base[kyouko3.fifo.head].command = reg;
  kyouko3.fifo.k_base[kyouko3.fifo.head].value	 = value;
  kyouko3.fifo.head++;
  if(kyouko3.fifo.head >= FIFO_ENTRIES)
  {
    kyouko3.fifo.head = 0;
  }
}

/* Function to flush the FIFO */
void fifo_flush()
{
  K_WRITE_REG(FIFO_HEAD, kyouko3.fifo.head);
  while(kyouko3.fifo.tail_cache != kyouko3.fifo.head)
  {
    kyouko3.fifo.tail_cache = K_READ_REG(FIFO_TAIL);
    schedule();
  }
}

irqreturn_t dma_interupt_handler(int irq, void *dev_id, struct pt_regs *regs)
{
// printk(KERN_ALERT "from interrupt handler \n");
  unsigned int iflags;
  unsigned long flags=0;

   iflags = K_READ_REG(INTERUPT_STATUS);
   K_WRITE_REG(INTERUPT_STATUS, iflags & 0xf);

   if ((iflags & 0x02) == 0) 
   {
     return IRQ_NONE;
   }
   printk(KERN_ALERT, "KERNEL >>>>>> dma_interrupt_handler");
   kyouko3.drain = (kyouko3.drain + 1) % DMA_NUM_BUFS;
   spin_lock_irqsave(&lock, flags);
   if(kyouko3.isQueueFull == 1)
   {
     wake_up_interruptible(&dma_snooze);
     kyouko3.isQueueFull = 0;
   }
   spin_unlock_irqrestore(&lock, flags);
   
   if(kyouko3.fill != kyouko3.drain)
   {
     FIFO_WRITE(BUFFERA_ADDRESS, dma_buffer_array[kyouko3.drain].dma_handle);
     FIFO_WRITE(BUFFERA_CONFIG, dma_buffer_array[kyouko3.drain].dma_buffer_size);
     K_WRITE_REG(FIFO_HEAD, kyouko3.fifo.head);
   }
   return IRQ_HANDLED;
}

void dma_initiate_transfer(unsigned int arg_count)
{
  unsigned int flags;

  dma_buffer_array[kyouko3.fill].dma_buffer_size=arg_count; 
  
  printk(KERN_ALERT "FILL %d\n ", kyouko3.fill);
  spin_lock_irqsave(&lock, flags);
  if(kyouko3.fill == kyouko3.drain)
  {
     printk(KERN_ALERT "from initiate transfer fill == drain \n");

    spin_unlock_irqrestore(&lock, flags);
    kyouko3.fill = (kyouko3.fill + 1) % 8;
    FIFO_WRITE(BUFFERA_ADDRESS, dma_buffer_array[kyouko3.drain].dma_handle);
    FIFO_WRITE(BUFFERA_CONFIG, dma_buffer_array[kyouko3.drain].dma_buffer_size);
    fifo_flush();
    return;
  }
  
  kyouko3.fill = (kyouko3.fill + 1) % 8;
  spin_unlock_irqrestore(&lock, flags);

  if(kyouko3.fill == kyouko3.drain)
  {
     kyouko3.isQueueFull = 1;
	// TODO find restore after wait or before
     wait_event_interruptible(dma_snooze, kyouko3.fill != kyouko3.drain);
     return;
  }

  //local_irq_restore(flags);

  return;
}

int dma_bind_function(struct file *fp, unsigned long arg)
{
 printk(KERN_ALERT "from bind dma \n");

unsigned long  vm_mmap_address;
int index = 0;
int ret = 0 ;

for(index ; index < 8 ; index++)
{
  kyouko3.vmmapindex = index;
  dma_buffer_array[index].dma_k_base= pci_alloc_consistent(kyouko3.pci_dev_saved, DMA_BUFFER_SIZE, &dma_buffer_array[index].dma_handle);
  if(IS_ERR_VALUE(dma_buffer_array[index].dma_handle))
  {
    printk(KERN_ALERT "ERROR IN BIND DMA HANDEL \n");
  }

  vm_mmap_address = vm_mmap(fp,0,DMA_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, 0x40000000 );
  if(IS_ERR_VALUE(vm_mmap_address))
  {
    printk(KERN_ALERT "ERROR IN VM_MMAP");
  }

  dma_buffer_array[index].dma_p_base = vm_mmap_address;
}

kyouko3.vmmapindex = 0;

ret = copy_to_user((unsigned long __user *)arg,&dma_buffer_array[0].dma_p_base,sizeof(unsigned long));
if(ret)
{
    printk(KERN_ALERT "ERROR IN COPY TO USER \n");
}

ret = pci_enable_msi(kyouko3.pci_dev_saved);
 printk(KERN_ALERT "pci enable val %d \n", ret);

if (ret) 
{
    printk("MSI capability structure configuration failed \n");
    return ret;
}

ret = request_irq(kyouko3.pci_dev_saved->irq, (irq_handler_t)dma_interupt_handler, IRQF_SHARED,
		  "kyouku3 dma interupt handler", &kyouko3);
printk(KERN_ALERT "KERNEL >>> after request irq");
if (ret) 
{
    pci_disable_msi(kyouko3.pci_dev_saved);
}

K_WRITE_REG(INPT_SET, 0x02);
return ret;
}

void unbind_dma(void)
{
   int i = 0;
   //wait_event_interruptible(dma_snooze, 1);

   for (i = 0; i < 8; i++) 
   {
     vm_munmap(dma_buffer_array[i].dma_p_base, DMA_BUFFER_SIZE);
     pci_free_consistent(kyouko3.pci_dev_saved, DMA_BUFFER_SIZE, dma_buffer_array[i].dma_k_base, dma_buffer_array[i].dma_handle);
   }
     K_WRITE_REG(INPT_SET, 0x0);
     free_irq(kyouko3.pci_dev_saved->irq, &kyouko3);
     pci_disable_msi(kyouko3.pci_dev_saved);
  
}

/* IOCTL function for the driver */
void kyouko3_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
  float one = 1.0;
  float zero = 0.0;
  float minus= -1.0;
  unsigned int one_us_int = *(unsigned int*)&one;
  unsigned int zero_zs_int = *(unsigned int*)&zero;
  unsigned int minus_one_int = *(unsigned int*)&minus;
  int ret;
   unsigned int arg_count; 
  unsigned long *argp = (unsigned long *)arg;


 switch(cmd)
  {
       case FIFO_QUEUE:
	             ret = copy_from_user(kyouko3.fifo.k_base,(struct fifo_entry *)arg, sizeof(struct fifo_entry));
		     FIFO_WRITE(kyouko3.fifo.k_base->command,kyouko3.fifo.k_base->value);
     		     break;                                    
    case FIFO_FLUSH:
		     fifo_flush();
		     break;
    case VMODE:
		if((int)(arg)==GRAPHICS_ON)
		{
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
		
		  FIFO_WRITE(RED, one_us_int);
		  FIFO_WRITE(GREEN, one_us_int);
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
		break;

    case START_DMA:
                   // printk(KERN_ALERT "start DMA \n");	
		    ret = copy_from_user(&arg_count, (unsigned long __user *)arg, sizeof(unsigned int));
		   //  printk(KERN_ALERT "copy from user count %d \n", arg_count);	
		    if(!ret)
		    {
			dma_initiate_transfer(arg_count);
		    }
		    copy_to_user((unsigned long __user *)arg,&dma_buffer_array[kyouko3.fill].dma_p_base,sizeof(unsigned long));
		    break;

    case BIND_DMA:
		  dma_bind_function(fp, arg); 
                  break;
    case UNBIND_DMA:
		  unbind_dma();
   		  break;

 	 }
}

/* Function to release the driver */	
int kyouko3_release(struct inode *inode, struct file *fp)
{
  kyouko3_ioctl(fp, VMODE, GRAPHICS_OFF);
  fifo_flush();
  pci_free_consistent(kyouko3.pci_dev_saved,8192u,kyouko3.fifo.k_base,kyouko3.fifo.p_base);
  //do_munmap();
  iounmap(kyouko3.p_control_base);
  iounmap(kyouko3.p_card_ram_base);
  printk(KERN_ALERT "BUUH BYE\n");
}

/* Function to memory map the driver */
int kyouko3_mmap(struct file*fp, struct vm_area_struct *vma)
{
 // printk(KERN_ALERT "hello from kyouko3_mmap \n");
 /*
  if(current->fsuid!=0)
  {
  exit(0);
  }*/
  int ret;
  switch(vma->vm_pgoff<< PAGE_SHIFT)
  {
    case 0:
            ret = io_remap_pfn_range(vma,vma->vm_start, kyouko3.p_control_base>>PAGE_SHIFT,vma->vm_end - vma->vm_start,vma->vm_page_prot);
            break;

    case 0x8000000:
            ret = io_remap_pfn_range(vma,vma->vm_start, kyouko3.p_card_ram_base>>PAGE_SHIFT,vma->vm_end - vma->vm_start,vma->vm_page_prot);
	    break;
    case 0x40000000:
   // default:
	//    printk(KERN_ALERT "kyouko_3_mmap default called:wq \n");
	    ret = io_remap_pfn_range(vma,vma->vm_start, dma_buffer_array[kyouko3.vmmapindex].dma_handle>>PAGE_SHIFT,vma->vm_end - vma->vm_start,vma->vm_page_prot);
	  //  printk(KERN_ALERT "kyouko_3_mmap default called:end \n");
  	    break;
   }

   return ret;
}

/* Function to probe the device */
int kyouko3_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id)
{
	int enable_int_value;  
	kyouko3.pci_dev_saved=pci_dev;
	kyouko3.p_control_base=pci_resource_start(pci_dev,1);
	kyouko3.p_card_ram_base= pci_resource_start(pci_dev,2);
	enable_int_value=pci_enable_device(pci_dev);  
	pci_set_master(pci_dev);
	//printk(KERN_ALERT "This is probe function\n");
	//pci_dev=irq;
	return 0;
}

/* Function to remove the driver */
int kyouko3_remove(struct pci_dev *pci_dev)
{
	pci_disable_device(pci_dev);
	return 0;
}

/* Initialization function */
int my_init_function(void)
{
	int register_int_value;
	cdev_init(&kyouko3_cdev,&kyouko3_fops);
	cdev_add(&kyouko3_cdev,MKDEV(500,127),1);
	register_int_value=pci_register_driver(&kyouko3_pci_drv);
	printk(KERN_ALERT "This is init function\n");
	return 0;
}


/* Exit function */
void my_exit_function(void)
{
	pci_unregister_driver(&kyouko3_pci_drv);
	cdev_del(&kyouko3_cdev);
	printk(KERN_ALERT "This is exit function\n");
	return;
}

module_init(my_init_function);
module_exit(my_exit_function);
