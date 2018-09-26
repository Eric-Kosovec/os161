#include <syscall.h>
#include <proc.h>
#include <current.h>
#include <types.h>
#include <limits.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/errno.h>

int 
sys_close(int fd, int32_t * retval) 
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

	lock_acquire(curproc->files[fd]->lk);

	--curproc->files[fd]->ref_count;

	KASSERT(curproc->files[fd]->ref_count >= 0);

	if (curproc->files[fd]->ref_count == 0) {
		if (curproc->files[fd]->f_vnode != NULL) {
			vfs_close(curproc->files[fd]->f_vnode);
		}

		struct lock * lk = curproc->files[fd]->lk;

		kfree(curproc->files[fd]);
		curproc->files[fd] = NULL;

		lock_release(lk);
		lock_release(curproc->ft_lock);
		lock_destroy(lk);

		*retval = 0;
		return 0;
	}

	lock_release(curproc->files[fd]->lk);
	lock_release(curproc->ft_lock);

	*retval = 0;
	return 0;
}
