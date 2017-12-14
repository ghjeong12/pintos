#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/init.h"
#include "userprog/process.h"
#include <kernel/console.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"

#include "filesys/inode.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define valid_fd(fd, f, res) if(fd < 0 || fd >= FD_MAX) {f->eax = res; break;}
static void syscall_handler (struct intr_frame *f);

static tid_t exec(void *cmd_line);
static int write (int fd, void *buffer, unsigned size);
static int wait(int  pid);
static int read (int fd, void *buffer, unsigned size);
static bool create (void *file, unsigned initial_size);
static bool remove (void *file);
static int open (void *file);
static int filesize (int fd);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);

void valid_address_check(void* addr);
void valid_stack_check(struct intr_frame* intr_f, int num);

bool mkdir(char* dir_name);
//void valid_fd(struct intr_frame* f, int res, int fd); 

/*
static struct process_file{
  struct file *file;
  int fd;
  struct list_elem elem;
};

static int process_add_file(struct file *f);
static struct file*process_get_file(int fd);
  */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_type=*(int *)(f->esp);
  int pid;
  int exit_code;
  int fd;
  char* dir_name; 
  switch(syscall_type)
  {
  case SYS_HALT:
    shutdown_power_off();
    break;
  
  case SYS_EXIT:
    if ((void *)((int *)f->esp + 1) >= PHYS_BASE)
      exit (-1);
    else
      exit (*((int *)f->esp + 1));
    break;
  
  case SYS_EXEC:
    valid_stack_check(f,1);
    valid_address_check(*((int **)f->esp + 1));
    f->eax = exec (*((int **)f->esp + 1));
    break;
  
  case SYS_WAIT:
    valid_stack_check(f,1);
    pid = *((int*)f->esp + 1);
    exit_code = wait(pid);
    f->eax = exit_code;
    break;
  
  case SYS_CREATE:
    valid_stack_check(f, 2);
    valid_address_check(*((int **)f->esp + 1));
    if ((void*)*((int **)f->esp + 1) == NULL)
      exit(-1);
    f->eax = (uint32_t) create (*((int **)f->esp + 1), *((int *)f->esp + 2));
    break;
  
  case SYS_REMOVE:
    valid_stack_check(f, 1);
    valid_address_check(*((int **)f->esp + 1));
    f->eax = (uint32_t)remove (*((int **)f->esp + 1));
    break;
  
  case SYS_OPEN:
    valid_stack_check(f, 1);
    valid_address_check(*((int **)f->esp + 1));
    f->eax = open (*((int **)f->esp + 1));
    break;
  
  case SYS_FILESIZE:
    valid_stack_check(f, 1);
    //valid_fd(*((int*)f->esp+1),f, -1);
    valid_fd(*((int *)f->esp + 1),f, -1);
    f->eax = filesize (*((int *)f->esp + 1));
    break;
  
  case SYS_READ:
    valid_stack_check(f, 3);
    valid_address_check (*((int **)f->esp + 2));
    valid_fd(*((int *)f->esp + 1),f, -1);
    f->eax = read (*((int *)f->esp + 1), *((int **)f->esp + 2), *((int *)f->esp + 3));
    break;

  case SYS_WRITE:
    valid_stack_check(f, 3);
    valid_address_check (*((int **)f->esp + 2));
    valid_fd(*((int *)f->esp + 1), f,-1);
    f->eax = write (*((int *)f->esp + 1), *((int **)f->esp + 2), *((int *)f->esp + 3));
    break;
  
  case SYS_SEEK:
    valid_stack_check(f, 2);
    valid_address_check (*((int **)f->esp + 1));
    valid_fd(*((int *)f->esp + 1),f,0);
    seek (*((int *)f->esp + 1), *((int *)f->esp + 2));
    break;

  case SYS_TELL:
    valid_stack_check(f, 1);
    valid_fd(*((int *)f->esp + 1),f,0);
    f->eax = tell (*((int *)f->esp + 1));
    break;
 
  case SYS_CLOSE:
    valid_stack_check(f, 1);
    valid_fd(*((int *)f->esp + 1),f,0);
    fd = *((int*)f->esp+1);
    if (thread_current()->fd_list[fd] != NULL){
      file_close (thread_current()->fd_list[fd]);
      thread_current()->fd_list[fd]=NULL;
    }
    break;
  // ADDED FOR PROJECT 4
  case SYS_MKDIR:
    dir_name = (char*) *((int*)f->esp+1);
    f->eax = filesys_create(true, dir_name, 0);
    break;
  
  case SYS_CHDIR:
    dir_name = (char*) *((int*)f->esp+1);
    f->eax = filesys_cd(dir_name);
    break;
  } // END OF SWITCH
}
void
exit (int exit_code_)
{
  int i;
  struct thread *cur = thread_current();
  
  cur->exit_code = exit_code_;
  cur->end = true;

  for (i=0; i < cur->num_files; i++){
    if (cur->fd_list[i] != NULL){
      file_close(cur->fd_list[i]);
      cur->fd_list[i] = NULL;
    }
  }
  
  printf("%s: exit(%d)\n", cur->process_name, exit_code_);

  //now you can wake the parent
  sema_up (&cur->sema_for_exit);
  
  //wait for kill
  sema_down (&cur->sema_for_kill);

  file_allow_write (cur->open_file); 
  file_close (cur->open_file); 
  thread_exit();
}

static tid_t
exec(void *cmd_line)
{
  return process_execute((char*)cmd_line);
}
static int
wait(int pid)
{
  return process_wait(pid);
}

static bool
create (void *file_name, unsigned initial_size)
{
  bool success;
  if(file_name == NULL)
    exit(-1);
  success = filesys_create (false, (char*)file_name, initial_size);
  return success;
}

static bool
remove (void *file_name)
{
  bool success = filesys_remove ((char*)file_name);
  return success;
}

static int
open (void *file_name)
{
  struct file *file;
  if (file_name == NULL)
      exit(-1);
  file = filesys_open ((char*)file_name);
  if (file == NULL){
    return -1;
  }
  else
  {
    struct thread *cur = thread_current();
    if (cur->num_files >= FD_MAX){
      return -1;
    }
    (cur->fd_list)[(cur->num_files)++] = file;
    return cur->num_files - 1;
  }
}

static int
filesize (int fd)
{
    struct thread *cur = thread_current();

    if (cur->fd_list[fd] == NULL){
      return 0;
    }
    else{
      return file_length (cur->fd_list[fd]);
    }
}


static void
seek (int fd, unsigned position)
{
  struct thread* cur = thread_current();
  if (cur->fd_list[fd] != NULL)
    file_seek (cur->fd_list[fd], position);
}

static unsigned
tell (int fd)
{
  struct thread *t = thread_current();
  if (t->fd_list[fd] == NULL){
    return 0;
  }
  else{
    return file_tell (t->fd_list[fd]);
  }
}

static int
write (int fd, void *buffer, unsigned size)
{
  char *buffer_charptr = (char*)buffer;
  //when writing to the console!
  if (fd == 1){
    putbuf(buffer_charptr, size);
    return size;
  }
  else{
    struct thread *cur = thread_current();
    if (cur->fd_list[fd] == NULL){
      return -1;
    }
    else{
      return file_write (cur->fd_list[fd], buffer, size); 
    }
  }
  //return ;
}

static int
read (int fd, void *buffer, unsigned size)
{
  int i;
  char *buffer_charptr = (char*)buffer;

  if (fd == 0){
    for (i=0; i<size; i++)
      buffer_charptr[i] = input_getc();
  }
  else{
    if (thread_current()->fd_list[fd] == NULL)
      return -1;
    else
      return file_read (thread_current()->fd_list[fd], buffer, size);
  }
  return size;
}

/*
static int process_add_file(struct file *f){
  struct process_file *pf=malloc(sizeof(struct process_file));
  pf->file = f;
  pf->fd = thread_current()->fd_num;
  thread_current()->fd_num++;
  list_push_back(&thread_current()->file_list,&pf->elem);
  return pf->fd;

}


static struct file*process_get_file(int fd){
  struct thread *t = thread_current();
  struct list_elem *e;
  for(e =list_begin(&t->fd_list);e!=list_end(&t->fd_list);e=list_next(e)){
     struct process_file *pf = list_entry(e,struct process_file, elem);
     if(fd==pf->fd){
          return pf->file;
     }
  }
  return NULL;
}
*/
void valid_address_check(void* addr)
{
  if(addr >= PHYS_BASE)
    exit(-1);
}
void valid_stack_check(struct intr_frame* intr_f, int num)
{
  valid_address_check((int*)(intr_f->esp) + num);
}
/*
void valid_fd(struct intr_frame* f, int res, int fd)
{
  if(fd < 0 || fd > FD_MAX)
  {
    f->eax = res;
    return ;
  }
}
*/
