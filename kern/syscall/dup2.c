#include <syscall.h>
#include <types.h>
#include <current.h>
#include <limits.h>
#include <kern/errno.h>

int 
sys_dup2(int oldfd, int newfd, int32_t * retval) 
{
	KASSERT(curproc != NULL);
	KASSERT(retval != NULL);

	if (oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX) {
		*retval = -1;
		return EBADF;
	}

	if (oldfd == newfd) {
		*retval = newfd;
		return 0;
	}

	lock_acquire(curproc->ft_lock);

	if (curproc->files[oldfd] == NULL) {
		lock_release(curproc->ft_lock);
		*retval = -1;
		return EBADF;
	}

	// Close the already opened file
	if (curproc->files[newfd] != NULL) {
		struct lock * lk = curproc->files[newfd]->lk;
		lock_acquire(lk);

		--curproc->files[newfd]->ref_count;
		
		KASSERT(curproc->files[newfd]->ref_count >= 0);

		if (curproc->files[newfd]->ref_count == 0) {
			vfs_close(curproc->files[newfd]->f_vnode);
			curproc->files[newfd] = NULL;
			lock_release(lk);
			lock_destroy(lk);
		}
		else {
			curproc->files[newfd] = NULL;
			lock_release(lk);
		}
	}

	KASSERT(curproc->files[newfd] == NULL);

	lock_acquire(curproc->files[oldfd]->lk);

	curproc->files[newfd] = curproc->files[oldfd];
	++curproc->files[oldfd]->ref_count;

	lock_release(curproc->files[oldfd]->lk);
	lock_release(curproc->ft_lock);

	*retval = newfd;

	return 0;
}
