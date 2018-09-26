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
sys_write(int fd, const_userptr_t buf, size_t buflen, int32_t * retval) 
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

	if ((curproc->files[fd]->flags & O_WRONLY) != O_WRONLY && (curproc->files[fd]->flags & O_RDWR) != O_RDWR) {
		lock_release(curproc->ft_lock);
		*retval = -1;
		return EBADF;
	}

	struct uio write_uio;
	struct iovec write_iovec;

	write_iovec.iov_ubase = (userptr_t)buf;
	write_iovec.iov_len = buflen;

	write_uio.uio_iov = &write_iovec;
	write_uio.uio_iovcnt = 1;
	write_uio.uio_resid = buflen;
	write_uio.uio_segflg = UIO_USERSPACE;
	write_uio.uio_rw = UIO_WRITE;

	spinlock_acquire(&curproc->p_lock);
	KASSERT(curproc->p_addrspace != NULL);
	write_uio.uio_space = curproc->p_addrspace;
	spinlock_release(&curproc->p_lock);

	lock_acquire(curproc->files[fd]->lk);
	lock_release(curproc->ft_lock);

	write_uio.uio_offset = curproc->files[fd]->offset;

	size_t amount_written = write_uio.uio_resid;

	int result = VOP_WRITE(curproc->files[fd]->f_vnode, &write_uio);

	amount_written -= write_uio.uio_resid;

	curproc->files[fd]->offset = write_uio.uio_offset;

	lock_release(curproc->files[fd]->lk);

	if (result) {
		*retval = -1;
		return result;
	}

	*retval = amount_written;
	return 0;
}
