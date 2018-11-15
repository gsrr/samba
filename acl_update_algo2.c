#include <sys/stat.h>
#include "ift_journal.h"

#define IFTUPDATEACL    0


struct inode {
    char *name;
    int inumber;
    struct inode *prev;
    struct inode *next;
    struct iftacl_aces *aces;
};

void ift_update_acl(char *path)
{
    printf("update acl:%s\n", path);
    int i;
    struct entryIftacl *e = jouTable->es;
    while(e != NULL)
    {
        printf("inode number:%d\n", e->inumber);
        e = e->next;
    }
}

void load_iftacl(char *path)
{
    ;
}
'
void init_path(char *path)
{
    printf("init_path : %s\n", path);
    char tpath[512] = {0};
    char *rest = path;
    char *token;
    while(token = strtok_r(rest, "/", &rest))
    {
        struct inode *node = (struct inode*)malloc(sizeof(struct inode));
        node->name = (char*)malloc(sizeof(char) * (strlen(token) + 1));
        strncpy(node->name, token, strlen(token));
        node->name[strlen(token)] = '\0';

        struct stat fileStat;
        sprintf(tpath, "%s/%s", tpath, token);
        printf("tpath:%s\n", tpath);
        stat(tpath,&fileStat);
        printf("inode number:%d\n", fileStat.st_ino);
        node->inumber = fileStat.st_ino;
        node->aces = load_iftacl(tpath);
    }
}

int main(int argc, char *argv[])
{
    if (atoi(argv[1]) == IFTUPDATEACL)
    {
        init_path(argv[3]);
        read_jou_table(argv[2]);
        ift_update_acl(argv[3]);
    }
    return 0;
}
