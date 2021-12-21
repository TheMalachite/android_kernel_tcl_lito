#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include <linux/seq_file.h>
#include <linux/sched/task.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include "ion.h"

#define NR_HEAP_TYPE_MAX (16)

extern int is_dma_buf_file_ext(struct file *file);
extern bool is_ion_dma_buf(struct dma_buf* dmabuf);
extern struct ion_device* get_ion_device(void);

struct ion_fd_data{
  	struct task_struct *tsk;
  	struct seq_file *seq;
	size_t ionsize;
};

struct ion_buffer* ion_file_to_ionbuffer(struct file* file){
	struct dma_buf *dbuf = NULL;

	if(!file || !is_dma_buf_file_ext(file) )
		return NULL;

	dbuf = file->private_data;
	if(!dbuf)
		return NULL;

	if(is_ion_dma_buf(dbuf))
		return dbuf->priv;

	return NULL;
}

static void dump_heap_ion_info(struct seq_file *s,size_t *heapsize){
	struct ion_heap *heap;
	struct ion_device *dev = get_ion_device();
	int type = 0;

	seq_printf(s,"%-16.s%-10s%-10s%-10s\n", 
				"heapname","type", "id","size");
	plist_for_each_entry(heap, &dev->heaps, node) {
		type = heap->type;
		if(type < 0 || type >= NR_HEAP_TYPE_MAX)
			type = NR_HEAP_TYPE_MAX-1;

		seq_printf(s,"%-16.s%-10d%-10d%-10d\n", 
			(type == NR_HEAP_TYPE_MAX-1)?"others":heap->name,heap->type, heap->id,heapsize[type]);
	}
}

static int ion_info_cb(const void *data,
			      struct file *file, unsigned int fd)
{
	struct ion_buffer *ibuf = NULL;
	struct ion_fd_data* fdata = (struct ion_fd_data*)data;
	struct seq_file* s = fdata->seq;

	ibuf = ion_file_to_ionbuffer(file);

	if(!ibuf)
		return 0;

	fdata->ionsize += ibuf->size;

	seq_printf(s,"%-16.s%-10d%-10d%-10d%-10d%-10d%-10s\n", 
				fdata->tsk->comm,fdata->tsk->pid,fdata->tsk->tgid,
				ibuf->size,ibuf->kmap_cnt,ibuf->heap->type,ibuf->heap->name);

	return 0;
}



static void dump_process_ion_info(struct seq_file *s)
{
	struct task_struct *tsk = NULL;
	int ret;
	struct ion_fd_data fdata;

	seq_printf(s,"-----------------------------------------------------\n");
	seq_printf(s,"Process ion details:\n");
	
	fdata.seq = s;
	seq_printf(s,"%-16.s%-10s%-10s%-10s%-10s%-10s%-10s\n", 
				"taskname","pid","tgid", "size","mapcount","heaptype","heapname");
	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		task_lock(tsk);
		fdata.tsk = tsk;
		fdata.ionsize = 0;
		ret = iterate_fd(tsk->files, 0, ion_info_cb, &fdata);
		if (fdata.ionsize){
			seq_printf(s,"%-16.s%-10d%-10d%u%s\n",
				tsk->comm, tsk->pid, tsk->tgid, fdata.ionsize,"(total)");
			seq_printf(s,"-----------------------------------------------------\n");
		}
		task_unlock(tsk);
	}
	rcu_read_unlock();
}


static int ion_monitor_show(struct seq_file *s, void *data)
{
	struct rb_node *n = NULL;
	struct ion_buffer* ibuf;
	size_t total_size = 0;
	size_t system_heap_size = 0;
	size_t system_contig_size = 0;
	size_t carveout_size = 0;
	size_t chunk_size = 0;
	size_t dma_size = 0;
	size_t custom_size = 0;
	size_t others_size = 0;
	size_t heapsize[NR_HEAP_TYPE_MAX]={0};
	int type = 0;
	struct ion_device *dev = get_ion_device();

	if (!dev)
		return -EINVAL;

	mutex_lock(&dev->buffer_lock);

	for(n=rb_first(&dev->buffers); n!=NULL ; n=rb_next(n)){
		ibuf = rb_entry(n , struct ion_buffer , node);

		if (ibuf->heap->type == ION_HEAP_TYPE_SYSTEM)
  			system_heap_size += ibuf->size;
  		else if (ibuf->heap->type == ION_HEAP_TYPE_SYSTEM_CONTIG)
  			system_contig_size += ibuf->size;
		else if (ibuf->heap->type == ION_HEAP_TYPE_CARVEOUT)
  			carveout_size += ibuf->size;
		else if (ibuf->heap->type == ION_HEAP_TYPE_CHUNK)
  			chunk_size += ibuf->size;
		else if (ibuf->heap->type == ION_HEAP_TYPE_DMA)
  			dma_size += ibuf->size;
		else if (ibuf->heap->type == ION_HEAP_TYPE_CUSTOM)
  			custom_size += ibuf->size;
		else
			others_size += ibuf->size;

		type = ibuf->heap->type;

		if(type < 0 || type >= NR_HEAP_TYPE_MAX)
			type = NR_HEAP_TYPE_MAX-1;

		heapsize[type] += ibuf->size;

		total_size += ibuf->size;
	}

	mutex_unlock(&dev->buffer_lock);
	seq_printf(s,"-----------------------------------------------------\n");
	seq_printf(s,"%16.s %16zu\n","total size",total_size);
	seq_printf(s,"%16.s %16zu\n","system size",system_heap_size);
	seq_printf(s,"%16.s %16zu\n","sys contig size",system_contig_size);
	seq_printf(s,"%16.s %16zu\n","carveout size",carveout_size);
	seq_printf(s,"%16.s %16zu\n","chunk size",chunk_size);
	seq_printf(s,"%16.s %16zu\n","dma size",dma_size);
	seq_printf(s,"%16.s %16zu\n","custom size",custom_size);
	seq_printf(s,"%16.s %16zu\n","others size",others_size);
	seq_printf(s,"-----------------------------------------------------\n");

	dump_heap_ion_info(s,heapsize);
	dump_process_ion_info(s);

	return 0;
}

static int ion_monitor_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_monitor_show, NULL);
}

static const struct file_operations ion_monitor_file_operations = {
	.owner = THIS_MODULE,
	.open = ion_monitor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init ion_monitor_init(void)
{
	proc_create("ion_monitor", S_IRUGO, NULL,
			&ion_monitor_file_operations);
	return 0;
}

module_init(ion_monitor_init);
MODULE_LICENSE("GPL");

