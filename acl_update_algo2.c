#include <sys/stat.h>
#include <sys/xattr.h>
#include "ift_journal.h"

#define XATTR_IFTACL_NAME   "security.iftacl"
#define IFTUPDATEACL    0

#define IFT_SUBFOLDER 0x02
#define IFT_SUBFILE 0x01

#define IFT_NTACL_INHERIT               0x10
#define IFT_NO_PROPAGATE_INHERIT        0x04
#define IFT_NTACL_NOTFOLDER             0x08

struct iftacl_sd
{
    char version; // version 0x01
    unsigned short size; // size = 17 in version01
    char dirty; // dirty = 1 (iftacl has not been changed through samba library)
    unsigned int owner;
    unsigned int group;
    unsigned int dflags; // inherit flags for samba
    unsigned short size_ace; // size = 11 in version01
    unsigned int num_aces;
    struct iftacl_aces *aces;
};

struct inode {
    int owner;
    int group;
    char *name;
    int inumber;
    int mode;
    struct inode *prev;
    struct inode *next;
    struct iftacl_sd *iacl;
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

struct inode *nodes = NULL;

int iftacl_parser(char *value, int valueLen, struct iftacl_sd *iftsd)
{
    int i = 0;
    int offset = 0;

    if ((offset + 3) > valueLen)
    {
        return -999;
    }
    iftsd->version = read_bytes(value, 1, &offset);
    if(iftsd->version != 1)
    {
        return -999;
    }

    iftsd->size = read_bytes(value, 2, &offset); // size = 17 in version 01
    if(iftsd->size != 17 || (offset + 17) > valueLen)
    {
        return -999;
    }
    iftsd->dirty = read_bytes(value, 1, &offset);
    iftsd->owner = read_bytes(value, 4, &offset);
    iftsd->group = read_bytes(value, 4, &offset);
    iftsd->dflags = read_bytes(value, 2, &offset);
    iftsd->size_ace = read_bytes(value, 2, &offset); // size_ace = 11 in version 01
    iftsd->num_aces = read_bytes(value, 4, &offset);
    if(iftsd->size_ace != 11 || (offset + 11 * iftsd->num_aces) > valueLen)
    {
        return -999;
    }

    iftsd->aces = (struct iftacl_aces*)malloc(sizeof(struct iftacl_aces) * iftsd->num_aces);

    for( i = 0 ;i < iftsd->num_aces ; i++)
    {
        iftsd->aces[i].type = read_bytes(value, 1, &offset);
        iftsd->aces[i].flags = read_bytes(value, 1, &offset);
        iftsd->aces[i].access = read_bytes(value, 4, &offset);
        iftsd->aces[i].role = read_bytes(value, 1, &offset);
        iftsd->aces[i].uid = read_bytes(value, 4, &offset);
    }
    return 0;
}

int iftacl_get_len(struct iftacl_sd *iacl)
{
    return (3 + 17) + 11 * (iacl->num_aces);
}

int iftacl_inherit_folder(struct inode *node, struct iftacl_sd *iacl)
{
        int i = 0;
        iacl->dflags = 0x9c04;  // inherit
        iacl->owner = node->owner;
        iacl->group = node->group;

        for(i = iacl->num_aces - 1 ; i > -1 ; i--)
        {
                //creator owner = 4, creator group = 5, everyone = 3
                char af = iacl->aces[i].flags;
                if((af & IFT_SUBFILE) == 0 && (af & IFT_SUBFOLDER) == 0)
                {
                        iacl->aces[i].role = 101; /*Not inherit*/
                        continue;
                }

                if((af & IFT_NO_PROPAGATE_INHERIT) == 0)
                {
                        iacl->aces[i].flags |= IFT_NTACL_NOTFOLDER; // cancel this folder
                        if((af & IFT_SUBFOLDER) != 0)
                        {
                                iacl->aces[i].flags &= ~(IFT_NTACL_NOTFOLDER); // add this folder
                        }

                        iacl->aces[i].flags |= IFT_NTACL_INHERIT;
                        if(iacl->aces[i].role == 4) // creator owner
                        {
                                iacl->aces[i].uid = iacl->owner;
                                iacl->aces[i].flags |= IFT_NTACL_NOTFOLDER;
                        }
                        else if(iacl->aces[i].role == 5) //creator group
                        {
                                iacl->aces[i].uid = iacl->group;
                                iacl->aces[i].flags |= IFT_NTACL_NOTFOLDER;
                        }
                }
                else
                {
                        if((af & IFT_SUBFOLDER) == 0)
                        {
                                iacl->aces[i].role = 101; /*Not inherit*/
                                continue;
                        }
                        /*
                         * add this folder and inherit flag/
                         * remove subfolder, files.
                         * (same as the case of file)
                         */
                        iacl->aces[i].flags = IFT_NTACL_INHERIT;
                        if(iacl->aces[i].role == 4) // creator owner
                        {
                                iacl->aces[i].uid = iacl->owner;
                                iacl->aces[i].role = 1;
                        }
                        else if(iacl->aces[i].role == 5) //creator group
                        {
                                iacl->aces[i].uid = iacl->group;
                                iacl->aces[i].role = 2;
                        }
                }
        }
        return 0;
}

int iftacl_inherit_file(struct inode *node, struct iftacl_sd *iacl)
{
        int i = 0;
        iacl->dflags = 0x9c04;  // inherit
        iacl->owner = node->owner;
        iacl->group = node->group;
        for(i = iacl->num_aces - 1 ; i > -1 ; i--)
        {
                char af = iacl->aces[i].flags;
                if((af & IFT_SUBFILE) == 0)
                {
                        iacl->aces[i].role = 101; /*Not inherit*/
                }
                else
                {
                        iacl->aces[i].flags = IFT_NTACL_INHERIT;
                        if(iacl->aces[i].role == 4) // creator owner
                        {
                                iacl->aces[i].uid = iacl->owner;
                                iacl->aces[i].role = 1;
                        }
                        else if(iacl->aces[i].role == 5) //creator group
                        {
                                iacl->aces[i].uid = iacl->group;
                                iacl->aces[i].role = 2;
                        }
                }
        }
        return 0;
}

struct iftacl_sd* load_iftacl_from_parent(struct inode *node)
{
    struct iftacl_sd *iacl;
    struct iftacl_sd *pacl = node->prev->iacl;
    int plen = iftacl_get_len(pacl);
    printf("\tsd size%d, ace size:%d\n", sizeof(struct iftacl_sd), sizeof(struct iftacl_aces));
    iacl = (struct iftacl_sd*) malloc(sizeof(struct iftacl_sd));
    iacl->aces = (struct iftacl_aces*) malloc(sizeof(char) * (11 * pacl->num_aces));
    memcpy(iacl, pacl, sizeof(struct iftacl_sd));
    memcpy(iacl->aces, pacl->aces, sizeof(struct iftacl_aces) * pacl->num_aces);
    if(S_ISDIR(node->mode))
    {
        iftacl_inherit_folder(node, iacl);
    }
    else
    {
        iftacl_inherit_file(node, iacl);
    }
    return iacl;
}

struct iftacl_sd* load_iftacl(char *path, struct inode *node)
{
    int len;
    char *value;
    struct iftacl_sd *iftsd;
    len = getxattr(path, XATTR_IFTACL_NAME, NULL, 0);
    if(len == -1)
    {
        return load_iftacl_from_parent(node);
    }
    printf("xattr len:%d\n", len);
    value = (char*)malloc(sizeof(char) * len);
    len = getxattr(path, XATTR_IFTACL_NAME, value, len);
    if(len == -1)
    {
        return load_iftacl_from_parent(node);
    }
    iftsd = (struct iftacl_sd*)malloc(sizeof(struct iftacl_sd));
    iftacl_parser(value, len, iftsd);
    return iftsd;
}

void dump_iftacl(struct iftacl_sd *iftacl)
{
    int i;
    printf("version : %d\n", iftacl->version);
    printf("size : %d\n", iftacl->size);
    printf("dirty : %d\n", iftacl->dirty);
    printf("owner : %d\n", iftacl->owner);
    printf("group : %d\n", iftacl->group);
    printf("dflags : %d\n", iftacl->dflags);
    printf("size_ace : %d\n", iftacl->size_ace);
    printf("num_aces : %d\n", iftacl->num_aces);

    for(i = 0 ; i < iftacl->num_aces ; i++)
    {
        printf("type : %d\n", iftacl->aces[i].type);
        printf("flags : %d\n", iftacl->aces[i].flags);
        printf("access : %d\n", iftacl->aces[i].access);
        printf("role : %d\n", iftacl->aces[i].role);
        printf("uid : %d\n", iftacl->aces[i].uid);
    }
}

void insert_inode(struct inode *node)
{
    if(nodes == NULL)
    {
        node->prev = NULL;
        nodes = node;
        return;
    }
    struct inode *tmp = nodes;

    while(tmp->next != NULL)
    {
        tmp = tmp->next;
    }
    node->prev = tmp;
    tmp->next = node;
    return;
}

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
        node->owner = fileStat.st_uid;
        node->group = fileStat.st_gid;
        node->inumber = fileStat.st_ino;
        node->mode = fileStat.st_mode;
        node->next = NULL;
        insert_inode(node);
        node->iacl = load_iftacl(tpath, node);
        dump_iftacl(node->iacl);
        printf("\n");
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

