//////////////////////////////////////////////////////////////////////
//                             University of California, Riverside
//
//
//
//                             Copyright 2020
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Ian-Chin Wang, Chi-An Wu
//
//   Description:
//     Skeleton of NPHeap Pseudo Device
//
////////////////////////////////////////////////////////////////////////

#include "npheap.h"

#include <asm/processor.h>
#include <asm/segment.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
static DEFINE_MUTEX(mlock);
extern struct miscdevice npheap_dev;
/**
 * struct node - The node storing basic information
 * @id: The key for the node, based on the offset
 * @size: The size of the allocated memory
 * @data: The actual data.
 */
struct node {
	__u64 id;
	__u64 size;
	void *data;
	struct node* next;
};

/**
 * init_node() - Initalize the node
 * @_node: The targeted node
 * @id: The id of the node
 */
void init_node(struct node* _node, __u64 id)
{
	_node->next = NULL;
	_node->id = id;
	_node->size = 0;
	_node->data = NULL;
}

/**
 * alloc_node() - Allocate page or user space memory to a node
 * @_node: The targeted node
 * @size: The size of the desired memory space in 64 bits
 *
 * Return:
 * * 0	- Succeed
 * *-1	- Memory allocation failed
 */
int alloc_node(struct node* _node, __u64 size)
{
	if (_node->size > 0) {
		printk(KERN_ERR "Error: Reallocate memory for offset=%llu\n", _node->id);
		return -1;
	}
#ifdef ALLOC_PAGE
	_node->data = alloc_pages(GFP_USER, get_order(size));
#elif defined(KMALLOC)
	_node->data = kzalloc(size, GFP_USER);
#else
	_node->data = (void*)vmalloc(size);
#endif
	if (!_node->data) {
		printk(KERN_ERR "Failed allocate %llu bytes of memory for offset=%llu\n", _node->size, _node->id);
		return -1;
	}
	_node->size = size;
	return 0;
}


struct node *head = NULL;

/**
 * get_node() - Return the node relevent to an offset
 * @_offset: The offset
 *
 * Return:
 * The targeted node. NULL if not found
 */
struct node* get_node(pgoff_t _offset)
{
	struct node *cur = head;
	struct node *prev = NULL;
	__u64 offset = _offset >> PAGE_SHIFT;
	printk(KERN_INFO "get_node offset=%llu\n", offset);

	while (cur) {
		if (cur->id == offset) {
			return cur;
		} 
		else if (cur->id > offset) {
			struct node *new_node  = kmalloc(sizeof(struct node), GFP_KERNEL);
			init_node(new_node, offset);
			new_node->next = cur;
			/*If insert from head*/
			if (cur == head)
				head = new_node;
			else
				prev->next = new_node;
			return new_node;
		}
		prev = cur;
		cur = cur->next;
	}

	if (!cur) {
		cur = kmalloc(sizeof(struct node), GFP_KERNEL);
		init_node(cur, offset);
		if (!head)
			head = cur;
		else
			prev->next = cur;
		return cur;
	}
	return NULL;
}


/**
 * delete_node() - Delete the node relevent to an offset
 * @_offset: The offset
 *
 * Return:
 * * 0	- Succeed
 * *-1	- Node not found
 */
int delete_node(pgoff_t _offset)
{
	struct node *cur = head;
	struct node *pre = NULL;
	__u64 offset = _offset >> PAGE_SHIFT;
	while(cur){
		if(cur->id == offset){
			if(cur == head){
				head = cur->next;
			}
			else{			
				pre->next = cur->next;
			}
#ifdef ALLOC_PAGE
			free_page(cur->data, get_order(cur->size));
#elif defined(KMALLOC)
			kfree(cur->data);
#else
			vfree(cur->data);
#endif
			kfree(cur);
			return 0;
		}
		/* The node to delete not found*/
		if(cur->id > offset)
			return -1;
		pre = cur;
		cur = cur->next;
	}
	return -1;
}


void npheap_open(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "MWR: Simple VMA open, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff);
}

static struct vm_operations_struct npheap_ops = {
	.open = npheap_open,
};

int npheap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	printk(KERN_WARNING "mmap called, start = %lu, end = %lu, offset=%lu\n", vma->vm_start, vma->vm_end, vma->vm_pgoff);
	unsigned long len = vma->vm_end - vma->vm_start;
	struct node* target_node = get_node(vma->vm_pgoff << PAGE_SHIFT);
	if (target_node->size == 0) {
		if (alloc_node(target_node, len) != 0)
			return ~EINVAL;
	}
	printk(KERN_INFO "npheap_mmap size=%llu\n", target_node->size);

#ifdef ALLOC_PAGE
	int ret = remap_pfn_range(vma, vma->vm_start, page_to_pfn(target_node->data), len, vma->vm_page_prot);
#elif defined(KMALLOC)
	int ret = remap_pfn_range(vma, vma->vm_start, virt_to_phys(target_node->data)>>PAGE_SHIFT , len, vma->vm_page_prot);
#else
	unsigned long vm_start = vma->vm_start;
	void* virt_addr = target_node->data;
	int ret;
	while (len > 0) {
		ret = remap_pfn_range(vma, vm_start, vmalloc_to_pfn(virt_addr), PAGE_SIZE, vma->vm_page_prot);
		if (ret != 0) {
			printk(KERN_ERR "Error remap_pfn_range\n");
			return ~EAGAIN;
		}
		len -= PAGE_SIZE;
		vm_start += PAGE_SIZE;
		virt_addr += PAGE_SIZE;
	}
#endif

	vma->vm_private_data = filp->private_data;
	if (ret != 0) {
		printk(KERN_ERR "Error remap_pfn_range\n");
		return ~EAGAIN;
	}
	vma->vm_ops = &npheap_ops;
	printk(KERN_INFO "remap_pfn_range OK\n");
	npheap_open(vma);
    return 0;
}

int npheap_init(void)
{
    int ret;
    if ((ret = misc_register(&npheap_dev)))
        printk(KERN_ERR "Unable to register \"npheap\" misc device\n");
    else
        printk(KERN_ERR "\"npheap\" misc device installed\n");
    return ret;
}

void npheap_exit(void)
{
    misc_deregister(&npheap_dev);
}


// If exist, return the data.
long npheap_lock(struct npheap_cmd __user *user_cmd)
{
	mutex_lock(&mlock);
    return 0;
}     

long npheap_unlock(struct npheap_cmd __user *user_cmd)
{
	mutex_unlock(&mlock);
    return 0;
}

long npheap_getsize(struct npheap_cmd __user *user_cmd)
{
	struct npheap_cmd *cmd = kmalloc(sizeof(struct npheap_cmd), GFP_KERNEL);
	if (access_ok(struct npheap_cmd, user_cmd, sizeof(struct npheap_cmd))) {
		copy_from_user(cmd, user_cmd, sizeof(struct npheap_cmd));
		struct node* target_node = get_node(cmd->offset);
		printk(KERN_INFO "npheap_getsize offset%llu size=%llu\n", cmd->offset, target_node->size);
		long ret = (long)target_node->size;
		kfree(cmd);
		return (ret > 0)? ret : 0;
	}
    return 0;
}

long npheap_delete(struct npheap_cmd __user *user_cmd)
{
	struct npheap_cmd *cmd = kmalloc(sizeof(struct npheap_cmd), GFP_KERNEL);
	if (access_ok(struct npheap_cmd, user_cmd, sizeof(struct npheap_cmd))) {
		copy_from_user(cmd, user_cmd, sizeof(struct npheap_cmd));
		printk(KERN_INFO "npheap_delete offset=%llu\n", cmd->offset);
		delete_node(cmd->offset);
		kfree(cmd);
	}
    return 0;
}

long npheap_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case NPHEAP_IOCTL_LOCK:
        return npheap_lock((void __user *) arg);
    case NPHEAP_IOCTL_UNLOCK:
        return npheap_unlock((void __user *) arg);
    case NPHEAP_IOCTL_GETSIZE:
        return npheap_getsize((void __user *) arg);
    case NPHEAP_IOCTL_DELETE:
        return npheap_delete((void __user *) arg);
    default:
        return -ENOTTY;
    }
}
