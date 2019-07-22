#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>

#include <vfs.h>
#include <kern/fcntl.h>
#include <limits.h>


#include "opt-A2.h"


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

// Termiantes the process that called it and leaves a 'message' for the parent (why it died and a number code)
// ******NOTE: THIS FUNCTION IS CALLED BY THE CHILD*****
void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  // (void)exitcode;

  #if OPT_A2
    // Code here
    // Child has a pointer to the parent -- find out if the parent exists
    // Parent hsa an array structure containing the childProcessData structures
      // It is in here that we modify the values of "int exitCode" and "bool exitStatus"
    // Then KILL the child :(((
    bool hasParent = false;
    bool childDead = false;
    if (curproc->parentProc != NULL) hasParent = true;
    if (hasParent) {
        // We're modifying the parent's data structure, we need to obtain its lock
        lock_acquire(curproc->parentProc->processLock);
        for (unsigned int i = 0 ; i < array_num(curproc->parentProc->children) ; i ++) {
          struct childProcessData *childData = array_get(curproc->parentProc->children, i);
          if (childData->pid == curproc->pid) {
            childData->exitCode = exitcode;
            childData->exitStatus = true;
            curproc->isAlive = false;
            childDead = true;
            break;
          }
        }
        lock_release(curproc->parentProc->processLock);
        if (childDead) {
          // Call CV signal to wake up the parent waiting on the child to die
          cv_signal(curproc->deathCV, curproc->parentProc->processLock);
        }
    } else {
      lock_acquire(curproc->processLock);
      for (unsigned int i = 0 ; i < array_num(curproc->children) ; i ++) {
        struct childProcessData *childData = array_get(curproc->children, i);
        childData->childProcess->parentProc = NULL;
      }

      lock_release(curproc->processLock);
    }
  #else
    (void)exitcode;
  #endif

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
// Returns the process id of the current process
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
    *retval = curproc->pid;
    return(0);
  #else
    *retval = 1;
    return(0);
  #endif
}

/* stub handler for waitpid() system call                */
// Puts the parent process to sleep until the child process is terminated
  // If the child process is already terminated, parent will not sleep and retrive the return values (exit status and exit code)
  // If not terminated, parent process needs to sleep
  // First need to check if this pid belongs to one of your children (parent->child relationship important)
  // If the pid isnt one of your children --> return ERROR
  // Otherwise, need to check if that child process is terminated
      // option1: can add param to proc.h to keep track of death/alive status
      // need to implement a lock for each process
  // wait for child to terminate now
      // option1: can use a condition variable (each process has its own condition variable)
          // the parent would fall asleep on the childs condition variable (you call cv_wait on the child)
          // so that when when the child calls exit -- it should signal for the parent to wake up
          // once you wake up, the child is now terminated and you are ready to get the childs exit code and exit status (must be left behind by child for parent when terminated)
          // the exit code and exit status should combine to one single value for waitpid
  //
int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  #if OPT_A2
  /* for now, just pretend the exitstatus is 0 */
  bool foundChild = false;

  // Check if the pid given is actually your child
  lock_acquire(curproc->processLock);
  for (unsigned int i = 0 ; i < array_num(curproc->children); i ++) {
    struct childProcessData *childData = array_get(curproc->children, i);
    if (childData->pid == pid) {
      foundChild = true;
      // Found the child in the parent's children array
      // Check if child is alive/dead -- the childProcessData struct should have a pointer to the child proc
        // If alive - call cv wait on child's condition variable
        // If dead - get the return values (exit status and code)
      while (!childData->exitStatus) {
        cv_wait(childData->childProcess->deathCV, curproc->processLock);
      }
      exitstatus = _MKWAIT_EXIT(childData->exitCode);
      break;
    }
  }
  lock_release(curproc->processLock);

  // return ERRROR
  if (!foundChild) {
    *retval = -1;
    return (ESRCH);
  }

  #else
  exitstatus = 0;
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


// Creates a new process that is a copy of the parent processor -- has it's own PID and set to be the child of the caller
// The return value in the child is 0 and the return value in the parent is the PID of the child
// Create a process structure (because we're a new process) -- use the function 'proc_create_runprogram()'
       // will create and populate process structure with everything but a thread+address space, process ID, parent-child relationship
       // can also return an error code -- if returns NULL, then there is insufficient memory (needs to be handled and fork needs to return the proper error code)
   // create a new address space and copy the contents of the parents address space to that of the child (child inherits code, heap, stack, etc.)
       // use the 'as_copy()' function -- creates a new address space and copies the old address space into the new one
       // can also return an error code -- usually because there was no memory :(
       // no need to malloc space for the address space (handled by 'as_copy()') -- just need to create a pointer and pass it into 'as_copy()'
   // attach the newly created address space to the child process
       // looking at the function 'curproc_setas()'' it will explain how to safely set the address space of a process -- copy this code (don't actually use it though)
   // after it's been established that all of the above is possible, create a PID for the child
       // DO IT YOURSELF
       // 'proc.h' -- process structure doesn't have a PID field (need to add)
       // also need to ensure that no processes have the same PID, a PID of 0, or a negative PID value
       // the PID is going to be accessible by more than 1 process at the same time (need to have a mutex to protect it)
           // can initialize a lock in the 'proc_bootstrap' function (make sure its the last thing to create in the function)
   // initiate the parent-child relationship
       // DO IT YOURSELF
       // option1: the process has a pointer to its parent process and a dynamic array of pointers or PIDs of children (need to have a process table in the OS for lookups based on the PID to find the children)
   // create a thread for the child process and tell the thread where to execute from (point it to the program counter)
       // take the saved trapframe of the parent process and put it onto the child process threads kernel stack
           // make a copy of the parents trapframe on the heap (use 'mem_copy()') and pass the heap address of the copy into the child process threads ('thread_fork()')
           // the child process can put it onto its stack and delete the version thats on the heap (free it)
       // call 'thread_fork()' -- make sure the second parameter here is the address of the child process
       // since thread_fork takes in a function pointer (entry point address for the sequence of instructions by the thread) -- this should be
           // since our new process will start executing in kernel mode, we need to pass in a kernel function (enter_forked_process)
           // function needs to take the trapframe from the parent process and put it onto the child processes thread stack (kernel stack of the child thread)
           // put things on the stack: heap variables vs local variables
           // take the trap frame of the parent and make a copy of it on the heap (use 'memcpy') -- and pass the heap address of the copy into thread_fork (free it after)
int
sys_fork(struct trapframe *tf, pid_t *retval) {
  #if OPT_A2

  // Create the process structure
  struct proc *childProcess = proc_create_runprogram(curproc->p_name);
  if (childProcess == NULL) {
    return ENOMEM;
  }

  // spinlock_acquire(&childProcess->p_lock);
  int asCopyCode = as_copy(curproc->p_addrspace, &childProcess->p_addrspace);
  // spinlock_release(&childProcess->p_lock);
  if (asCopyCode) {
    proc_destroy(childProcess);
    return ENOMEM;
  }

  // Create a PID for the child
  pid_t createPidCode = create_pid();
  if (createPidCode == 0) {
    proc_destroy(childProcess);
    return ENPROC;
  }
  childProcess->pid = createPidCode;

  // Initiate the child-parent relationship
  childProcess->parentProc = curproc;
  int addChildCode = proc_add_child(curproc, childProcess);
  if (addChildCode) {
    proc_destroy(childProcess);
    return addChildCode;
  }

  // Create a new thread for the child process
  struct trapframe *temp = kmalloc(sizeof(struct trapframe));
  memcpy(temp, tf, sizeof(struct trapframe));
  int errorCode = thread_fork(curproc->p_name, childProcess, (void *)&enter_forked_process, temp, 0);
  if (errorCode) {
    proc_destroy(childProcess);
    return errorCode;
  }

  *retval = childProcess->pid;
  return 0;
  #endif
}


// Counts the number of arguments and copies them into the kernel
// Copy the program path into the kernel
    // Open the program file using vfs_open(prog_name ....)
    // Create new address space, set process to the new address space, and activate
    // Using the opened program file, load the program image using load_elf
// Need to copy the arguments into the new address space -- copy the arguments (array and the strings) onto the user stack (as_define_stack)
// Delete the old address space
// Call enter_new_process with address to the arguments on the stack, the stack pointer (from as_define_stack), and the program entry point (from vfs_open)
int
sys_execv(const char *program, char **args) {
  #if OPT_A2
  if (program == NULL) return ENOENT;
  if (strlen(program) == 0) return ENOENT;





  // Counts the number of arguments and copies them into the kernel
  int numArguments = 0;
  while (args[numArguments] != NULL) {
    numArguments ++;
  }


  // Copy to kernel
  size_t argArraySize = (numArguments + 1) * sizeof(char *);    // remember to include the NULL terminator
  char **kernelArgs = kmalloc(argArraySize);
  if (kernelArgs == NULL) {
    return ENOMEM;
  }

  for (int n = 0 ; n < numArguments ; n ++) {
    size_t argSize = (strlen(args[n]) + 1) * sizeof(char);
    kernelArgs[n] = kmalloc(argSize);     // A
    int errorCode = copyinstr((const_userptr_t) args[n], kernelArgs[n], ARG_MAX, NULL);
    if (errorCode) {
      // TODO: find out what to return here
      return errorCode;
    }
  }
  kernelArgs[numArguments] = NULL;






  // Copy the program path into the kernel -- how do i do this ???
      // The program path is one of the arguments to execv (const char *program)
      // NOTE: for arguments that are pointers in syscalls (like execv) the actual address is unimportant
          // since we're changing address spaces -- instead we copy the value of what the pointer is pointing at
      // NOTE: we do this because the program path passed in is a pointer to string in user level space address space
          // to implement execv we'll need to purge the old address space and replace it with a new one and when we
          // do this, any pointers to the old address space will be invalidated -- to prevent this we have to copy
          // the string into the kernel before you destroy the user space


  // When copying from/to userspace
      // Use copyin/copyout for fixed size variables (integers, arrays, etc.)
      // Use copyinstr/copyoutstr when copying NULL terminated strings
  // int copyin(const_userptr_t usersrc, void *dest, size_t len);
  // int copyout(const void *src, userptr_t userdest, size_t len);
  // int copyinstr(const_userptr_t usersrc, char *dest, size_t len, size_t *got);
  // int copyoutstr(const char *src, userptr_t userdest, size_t len, size_t *got);
  size_t programLength = (strlen(program) + 1) * sizeof(char);    // remember to include the NULL terminator
  char *kernelSpace = kmalloc(programLength);
  if (kernelSpace == NULL) {
    return ENOMEM;
  }
  int errorCode = copyin((const_userptr_t) program, (void *) kernelSpace, programLength);
  if (errorCode) {
    // TODO: find out the error to return here
    return errorCode;
  }





  // Open the program file using vfs_open(prog_name ....)
  // Create new address space, set process to the new address space, and activate
  // Using the opened program file, load the program image using load_elf
  struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;


	/* Open the file. */
	result = vfs_open(kernelSpace, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	// KASSERT(curproc_getas() == NULL);
  // curproc should not have an address space
  	if(curproc->p_addrspace != NULL) {
  		as = curproc->p_addrspace;
  		as_destroy(as);
  		curproc->p_addrspace = NULL;
  }


	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}
	/* Switch to it and activate it. */
  // TODO: keep track of the old address so we can delete it later
  struct addrspace *oldAddressSpace = curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);


	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}


  // Need to copy the arguments into the new address space -- copy the arguments
      // (array and the strings) onto the user stack (as_define_stack)
  // NOTE: to pass arguments to a user program, we have to load the arguments into the programs
      // address space and create the argv array in the user programs address space
      // argv is an array of pointers to the actual argument strings
  // NOTE: when you load data into an address space we must make sure its properly aligned
      // 4 byte items (character pointers) must start at an address that is divisible by 4 --> USE ROUNDUP
  // NOTE: the stack pointer should always start at an address 8-byte aligned (divisble by 8)

  // When copying from/to userspace
      // Use copyin/copyout for fixed size variables (integers, arrays, etc.)
      // Use copyinstr/copyoutstr when copying NULL terminated strings
  // int copyin(const_userptr_t usersrc, void *dest, size_t len);
  // int copyout(const void *src, userptr_t userdest, size_t len);
  // int copyinstr(const_userptr_t usersrc, char *dest, size_t len, size_t *got);
  // int copyoutstr(const char *src, userptr_t userdest, size_t len, size_t *got);

  // Create the argv array in the user programs address space (array of pointers to the actual argument strings)
  if (numArguments > 0) {
    vaddr_t argv [numArguments + 1];
    // Copy the argument strings onto the user stack
    for (int n = (numArguments - 1) ; n >= 0 ; n --) {
      // NOTE: when you load data into an address space we must make sure its properly aligned
          // 4 byte items (character pointers) must start at an address that is divisible by 4 --> USE ROUNDUP
      size_t argSize = ROUNDUP(strlen(kernelArgs[n]) + 1, 4);


      // NOTE: the stack pointer should always start at an address 8-byte aligned (divisble by 8)
      stackptr -= (argSize * sizeof(char));

      // Copy the stack array into the user stack
      int errorCode = copyoutstr(kernelArgs[n], (userptr_t) stackptr, argSize, NULL);
      if (errorCode) {
        // TODO: find the error code to return here
        return errorCode;
      }
      argv[n] = stackptr;
    }
    argv[numArguments] = 0;

    // Copy the argument pointers onto the user stack (use the address here)
    for (int n = numArguments ; n >= 0 ; n --) {
      stackptr -= sizeof(vaddr_t);
      int errorCode = copyout(&argv[n], (userptr_t) stackptr, sizeof(vaddr_t));
      if (errorCode) {
        // TODO: find the error code to return here
        return errorCode;
      }
    }
  }









  // Delete old address space
  as_destroy(oldAddressSpace);

  // Call enter_new_process with address to the arguments on the stack, the stack pointer
      // (from as_define_stack), and the program entry point (from vfs_open)
	enter_new_process(numArguments /*argc*/, (userptr_t) stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

  #endif
}
