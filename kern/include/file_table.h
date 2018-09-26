#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

#include <synch.h>
#include <types.h>
#include <limits.h>
#include <vnode.h>

struct file_handle;

struct file_table {
	struct file_handle * files[OPEN_MAX];
	struct lock * lk;
};

struct file_handle {
	struct vnode * f_vnode;
	int ref_count;
	off_t offset;
	int flags;
	struct lock * lk;
};

struct file_table * file_table_create(void);
void file_table_destroy(struct file_table * ft);
int file_table_add(struct file_table * ft, char * filename, int flags, int32_t * retval);

#endif