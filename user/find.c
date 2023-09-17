#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

char *trim(char *str) {
    char *end = str + strlen(str) - 1;
    while (end > str && *end == ' ') end--;
    *(end + 1) = '\0';
    return str;
}

void find(char *path, char *fname) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_FILE:
      // printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
      // printf("path = %s, fmtpath = %s, fname = %s====>strcmp = %d\n", path, fmtname(path), fname, strcmp(fmtname(path), fname));
      if (strcmp(trim(fmtname(path)), fname) == 0)
      {
        printf("%s\n", path);
      }
      
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }
        if (strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
          find(buf, fname);  // 递归搜索子目录
        }
      }
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    printf("Missing args!!!\n");
  } else if (argc == 2) {
    find(".", argv[1]);
  } else if (argc == 3) {
    find(argv[1], argv[2]);
  } else {
    printf("Too many args!!!\n");
    exit(-1);
  } 
  
  exit(0);
}
