#include <syscall.h>
#include <synch.h>
#include <current.h>
#include <proc.h>
#include <vfs.h>
#include <limits.h>
#include <uio.h>
#include <vnode.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/iovec.h>

int 
sys_read(int fd, userptr_t buf, size_t buflen, int32_t * retval) 
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

	if ((curproc->files[fd]->flags & O_WRONLY) == O_WRONLY) {
		lock_release(curproc->ft_lock);
		*retval = -1;
		return EBADF;
	}

	struct uio read_uio;
	struct iovec read_iovec;

	read_iovec.iov_ubase = buf;
	read_iovec.iov_len = buflen;

	read_uio.uio_iov = &read_iovec;
	read_uio.uio_iovcnt = 1;
	read_uio.uio_resid = buflen;
	read_uio.uio_segflg = UIO_USERSPACE;
	read_uio.uio_rw = UIO_READ;

	spinlock_acquire(&curproc->p_lock);
	KASSERT(curproc->p_addrspace != NULL);
	read_uio.uio_space = curproc->p_addrspace;
	spinlock_release(&curproc->p_lock);

	lock_acquire(curproc->files[fd]->lk);
	lock_release(curproc->ft_lock);

	read_uio.uio_offset = curproc->files[fd]->offset;

	size_t amount_read = read_uio.uio_resid;

	int result = VOP_READ(curproc->files[fd]->f_vnode, &read_uio);

	amount_read -= read_uio.uio_resid;

	curproc->files[fd]->offset = read_uio.uio_offset;

	lock_release(curproc->files[fd]->lk);

	if (result) {
		*retval = -1;
		return result;
	}

	*retval = amount_read;
	return 0;
}
