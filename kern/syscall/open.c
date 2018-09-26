#include <types.h>
#include <copyinout.h>
#include <limits.h>
#include <synch.h>
#include <current.h>
#include <proc.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <syscall.h>
#include <file_table.h>

static bool check_flags(int flags);

int 
sys_open(const_userptr_t filename, int flags, int32_t * retval)
{
	KASSERT(retval != NULL);
	KASSERT(curproc != NULL);

	char safe_filename[PATH_MAX + 1];
	memset(safe_filename, 0, sizeof(safe_filename));

	size_t filename_length = 0;

	int result = copyinstr(filename, safe_filename, PATH_MAX, &filename_length);

	if (result) {
		*retval = -1;
		return result;
	}

	if (!check_flags(flags)) {
		*retval = -1;
		return EINVAL;
	}

	result = file_table_add(curproc->file_table, safe_filename, flags, retval);

	if (result) {
		*retval = -1;
		return result;
	}

	return 0;
}

static bool 
check_flags(int flags) 
{
	if (flags < 0 || flags > 64) {
		return false;
	}
	if ((flags & O_WRONLY) == O_WRONLY && (flags & O_RDWR) == O_RDWR) {
		return false;
	}
	return true;
}
