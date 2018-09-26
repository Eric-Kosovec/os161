#include <syscall.h>
#include <current.h>
#include <types.h>
#include <proc.h>
#include <uio.h>
#include <vfs.h>
#include <kern/errno.h>

int 
sys___getcwd(userptr_t buf, size_t buflen, int32_t * retval) 
{
	KASSERT(retval != NULL);
	KASSERT(curproc != NULL);

	struct vnode * cwd_vnode = NULL;

	int result = vfs_getcurdir(&cwd_vnode);

	if (result) {
		*retval = -1;
		return result;
	}

	if (cwd_vnode == NULL) {
		*retval = -1;
		return ENOENT;
	}

	spinlock_acquire(&curproc->p_lock);
	


	spinlock_release(&curproc->p_lock);

	return 0;
}
