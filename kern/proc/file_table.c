#include <file_table.h>
#include <limits.h>
#include <synch.h>
#include <lib.h>
#include <kern/stat.h>
#include <kern/errno.h>

struct file_table * 
file_table_create(void) 
{	
	struct file_table * ft = NULL;
	ft = (struct file_table *)kmalloc(sizeof(struct file_table));

	if (ft != NULL) {
		memset(ft->files, 0, sizeof(ft->files));

		ft->lk = lock_create("file_table_lock");

		if (ft->lk == NULL) {
			kfree(ft);
			ft = NULL;
		}
	}

	return ft;
}

void 
file_table_destroy(struct file_table * ft) 
{
	KASSERT(ft != NULL);

	if (ft != NULL) {
		// TODO destroy each open file
		lock_destroy(ft->lk);
		kfree(ft);
	}
}

int 
file_table_add(struct file_table * ft, char * filename, int flags, int32_t * retval) 
{
	KASSERT(ft != NULL);
	KASSERT(filename != NULL);
	KASSERT(retval != NULL);

	if (ft == NULL || filename == NULL || retval == NULL) {
		return EINVAL;
	}

	lock_acquire(ft->lk);

	int fd = 0;

	while (fd < OPEN_MAX && ft->files[fd] != NULL) {
		++fd;
	}

	if (fd < 0 || fd >= OPEN_MAX) {
		lock_release(ft->lk);
		*retval = -1;
		return EMFILE;
	}

	ft->files[fd] = (struct file_handle *)kmalloc(sizeof(struct file_handle));
	if (ft->files[fd] == NULL) {
		lock_release(ft->lk);
		*retval = -1;
		return ENOMEM;
	}

	ft->files[fd]->f_vnode = NULL;
	ft->files[fd]->ref_count = 1;
	ft->files[fd]->offset = 0;
	ft->files[fd]->flags = flags;
	ft->files[fd]->lk = lock_create(filename);

	if (ft->files[fd]->lk == NULL) {
		kfree(ft->files[fd]);
		ft->files[fd] = NULL;
		lock_release(ft->lk);
		*retval = -1;
		return ENOMEM;
	}

	if (flags & O_APPEND == O_APPEND) {
		struct stat file_info;
		memset(&file_info, 0, sizeof(file_info));
		if (VOP_STAT(ft->files[fd]->f_vnode, &file_info)) {
			*retval = -1;
			return EIO;
		}
		ft->files[fd]->offset = st_size;
	}

	int result = vfs_open(filename, flags, 0664, &ft->files[fd]->f_vnode);

	if (result) {
		lock_destroy(ft->files[fd]->lk);
		kfree(ft->files[fd]);
		ft->files[fd] = NULL;
		lock_release(ft->lk);
		*retval = -1;
		return result;
	}

	KASSERT(ft->files[fd]->f_vnode != NULL);

	lock_release(ft->lk);

	*retval = fd; 
	return 0;
}
