#include "filesys/filesys.h"
#include "threads/thread.h"

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}


/* Added for project 4 */
bool
filesys_cd (const char* name)
{
  struct dir *new_dir = NULL;
  char* name_cpy = (char*) malloc ( (strlen(name)+1) * sizeof(char));
  strlcpy(name_cpy, name, strlen(name) + 1);
   
  // TODO: relative path, cwd
  struct dir *curr = dir_open_root();
  char *token_;
  char* rest_str;
  for (token_ = strtok_r(name_cpy, "/", &rest_str); token_ != NULL; token_ = strtok_r(NULL, "/", &rest_str))
  {
    struct inode *dir_inode = NULL;

    if(! dir_lookup(curr, token_, &dir_inode)) {
      dir_close(curr);
      return NULL;
    }
             
    struct dir *next = dir_open(dir_inode);
    if(next == NULL) {
      dir_close(curr);
      return NULL;
    }
    
    dir_close(curr);
    curr = next;
  }
  new_dir = curr;
  free(name_cpy);


  if(new_dir == NULL)
    return false;
  
  if(thread_current() -> cur_dir != NULL) 
    dir_close(thread_current() -> cur_dir);
  
  thread_current()-> cur_dir = new_dir;
  
  return true;
}
/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (bool is_dir, const char *name, off_t initial_size) 
{
  if(strlen(name) ==0)
    return false;
  block_sector_t inode_sector = 0;
  
  char directory[ strlen(name) ];
  char file_name[ strlen(name) ];

  char *name_cpy = (char*) malloc( sizeof(char) * (strlen(name) + 1) );
  memcpy (name_cpy, name, sizeof(char) * (strlen(name) + 1));
  //relative path handling : start with ./

  // absolute path handling
  char *dir_str = directory;
  if(strlen(name) > 0 && name[0] == '/') {
    if(dir_str) 
      *dir_str++ = '/';
  }
       
  // tokenize
  char *token, *p, *last_token = "";
  for (token = strtok_r(name_cpy, "/", &p); token != NULL; token = strtok_r(NULL, "/", &p))
  {
    // append last_token into directory
    int tl = strlen (last_token);
    if (dir_str && tl > 0) {
      memcpy (dir_str, last_token, sizeof(char) * tl);
      dir_str[tl] = '/';
      dir_str += tl + 1;
    }
    last_token = token;
    //printf("%s----\n", last_token);
  }
         
  if(dir_str) 
    *dir_str = '\0';
  memcpy (file_name, last_token, sizeof(char) * (strlen(last_token) + 1));
  free (name_cpy);
// -----------
  struct dir *dir;
  name_cpy = (char*) malloc ( (strlen(name)+1) * sizeof(char));
  //printf("%s name ----\n", name);
  strlcpy(name_cpy, name, strlen(name) + 1);
   
  struct dir *curr = dir_open_root();
  
  /* /user/bin */ 
  if( (strlen(name) != 0) && (name[0] == '/') )   //ABSOLUTE PATH
  {
    char *token_;
    char* rest_str;
    for (token_ = strtok_r(name_cpy, "/", &rest_str); token_ != NULL; token_ = strtok_r(NULL, "/", &rest_str))
    {
      /* this should be last filename */
    if(strlen(rest_str) == 0)
    {
      //printf("this is the last token\n");
      break;
    }
    struct inode *inode = NULL;
    if(! dir_lookup(curr, token_, &inode)) {
      dir_close(curr);
      return NULL; // such directory not exist
    }
             
    struct dir *next = dir_open(inode);
    if(next == NULL) {
      dir_close(curr);
      return NULL;
    }
    dir_close(curr);
    curr = next;
    }
    //printf("filename : %s\,", file_name);
    dir = curr;
    free(name_cpy);
  }
  else  //RELATIVE PATH
  {
    if(thread_current() -> cur_dir != NULL)
      curr = thread_current() -> cur_dir;
    char *token_;
    char* rest_str;
    for (token_ = strtok_r(name_cpy, "/", &rest_str); token_ != NULL; token_ = strtok_r(NULL, "/", &rest_str))
    {
      if(is_dir)
      {
      }
      /* this should be last filename */
      if(strlen(rest_str) == 0)
      {
        //printf("this is the last token\n");
        break;
      }
      struct inode *inode = NULL;
      if(! dir_lookup(curr, token_, &inode)) {
        dir_close(curr);
        return NULL; // such directory not exist
      }
             
      struct dir *next = dir_open(inode);
      if(next == NULL) {
        dir_close(curr);
        return NULL;
      }
      dir_close(curr);
      curr = next;
    }
    //printf("filename : %s\,", file_name);
    dir = curr;
    free(name_cpy);
  }
  
  //--------------  
  bool success;
  if(is_dir) // CREATE a directory by mkdir
  {
    //printf("------MAKING DIRECTORY ---- %s\n", name);
  }

  if(1) // CREATE a file
  {
    success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (is_dir,inode_sector, initial_size)
                  && dir_add (dir, file_name, inode_sector));
    if (!success && inode_sector != 0) 
      free_map_release (inode_sector, 1);
    
    struct inode* inode;
    if(!dir_lookup(dir, file_name, &inode))
    {
      //printf("%s is not created!\n", file_name);
    }
    
    dir_close (dir);
    if(!success)
          ;
  }

 /* 
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (false,inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
*/
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
 
  //Added from here
  char *name_cpy = (char*) malloc( sizeof(char) * (strlen(name) + 1) );
  memcpy (name_cpy, name, sizeof(char) * (strlen(name) + 1));
  struct dir *curr = dir_open_root();

  if( (strlen(name) != 0) && (name[0] == '/') )   //ABSOLUTE PATH
  {
    char *token_;
    char* rest_str;
    for (token_ = strtok_r(name_cpy, "/", &rest_str); token_ != NULL; token_ = strtok_r(NULL, "/", &rest_str))
    {
      /* this should be last filename */
    if(strlen(rest_str) == 0)
    {
      //printf("this is the last token\n");
      break;
    }
    struct inode *inode = NULL;
    if(! dir_lookup(curr, token_, &inode)) {
      dir_close(curr);
      return NULL; // such directory not exist
    }
             
    struct dir *next = dir_open(inode);
    if(next == NULL) {
      dir_close(curr);
      return NULL;
    }
    dir_close(curr);
    curr = next;
    }
    //printf("filename : %s\,", file_name);
    dir = curr;
    free(name_cpy);
  }
  else  //RELATIVE PATH
  {
    if(thread_current() -> cur_dir != NULL)
    {
      curr = thread_current() -> cur_dir;
    }
    char *token_;
    char* rest_str;
    for (token_ = strtok_r(name_cpy, "/", &rest_str); token_ != NULL; token_ = strtok_r(NULL, "/", &rest_str))
    {
      /* this should be last filename */
      if(strlen(rest_str) == 0)
      {
        //printf("this is the last token\n");
        break;
      }
      struct inode *inode = NULL;
      if(! dir_lookup(curr, token_, &inode)) {
        dir_close(curr);
        return NULL; // such directory not exist
      }
             
      struct dir *next = dir_open(inode);
      if(next == NULL) {
        dir_close(curr);
        return NULL;
      }
      dir_close(curr);
      curr = next;
    }
    //printf("filename : %s\,", file_name);
    dir = curr;
    free(name_cpy);
  }
  
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);
  if(inode == NULL)
  {
    //printf("couldn't find inode \n");
  }
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
