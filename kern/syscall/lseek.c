#include <syscall.h>
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <limits.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/seek.h>
#include <kern/stat.h>

int 
sys_lseek(int fd, off_t pos, int whence, int64_t * retval) 
{
	KASSERT(retval != NULL);
	KASSERT(curproc != NULL);

	if (fd < 0 || fd >= OPEN_MAX) {
		*retval = -1;
		return EBADF;
	}

	lock_acquire(curproc->ft_lock);

	if (curproc->files[fd] == NULL) {
		lock_release(curproc->ft_lock);
		*retval = -1;
		return EBADF;
	}

	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
		lock_release(curproc->ft_lock);
		*retval = -1;
		return EINVAL;
	}

	off_t seek_to = pos;

	lock_acquire(curproc->files[fd]->lk);
	lock_release(curproc->ft_lock);

	if (!VOP_ISSEEKABLE(curproc->files[fd]->f_vnode)) {
		lock_release(curproc->files[fd]->lk);
		*retval = -1;
		return ESPIPE;
	}

	if (whence == SEEK_END) {
		struct stat file_info;
		memset(&file_info, 0, sizeof(file_info));
		if (VOP_STAT(curproc->files[fd]->f_vnode, &file_info)) {
			lock_release(curproc->files[fd]->lk);
			*retval = -1;
			return ESPIPE;
		}
		seek_to = pos + file_info.st_size;
	}

	else if (whence == SEEK_CUR) {
		seek_to = pos + curproc->files[fd]->offset;
	}

	if (seek_to < 0) {
		lock_release(curproc->files[fd]->lk);
		*retval = -1;
		return EINVAL;
	}

	curproc->files[fd]->offset = seek_to;

	lock_release(curproc->files[fd]->lk);

	*retval = seek_to;

	return 0;
}
