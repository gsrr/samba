#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/xattr.h>


#define NUM 10
#define INUM 50
#define ACLNUM 5

struct acl {
    int user;
    int perm;    // perm : 1 --> write, 2 --> read
    int scope;   // scope : 1 --> subfolder, 2 --> file
    int inherit;
};

struct journal {
    int serial_number;
    int inode_number;
    int user;
    int acl;
};

struct inode {
    int num;
    int hist;
    struct acl *iacl;
};

void create_journal(struct journal *jou, int num)
{
    int i;
    int user = 100001;
    int base = 1;
    for (i = 0 ; i < num ; i++)
    {
        jou[i].serial_number = base;
        jou[i].inode_number = i + 1;
        jou[i].user = 10000 + i + 1;
        jou[i].acl = i + 1;
        user += 1;
        base *= 2;
    }
    
}

void create_inode(struct inode *nodes, int num)
{
    int i;
    int j;
    for (i = 0 ; i < num ; i++)
    {
        nodes[i].num = i + 1;
        nodes[i].hist = 0;
        nodes[i].iacl = (struct acl*)malloc(sizeof(struct acl) * (ACLNUM * 2));
        for (j = 0 ; j < ACLNUM ; j++)
        {
            nodes[i].iacl[j].user = 10000 + j;
            nodes[i].iacl[j].perm = 3;
            nodes[i].iacl[j].scope = 3;
            nodes[i].iacl[j].inherit = 0;
        }
        for (j = 0 ; j < ACLNUM ; j++)
        {
            nodes[i].iacl[j].user = 10000 + j;
            nodes[i].iacl[j].perm = 3;
            nodes[i].iacl[j].scope = 3;
            nodes[i].iacl[j].inherit = 1;
        }
    }
}

int is_updated(int hist, struct journal *jou)
{
     int i;
     for (i = 0 ; i < NUM ; i++)
     {
        if(jou[i].serial_number & hist == 0)
        {
            return -1;
        }
     }
     return 0;
}

int check_acl(struct inode *nodes, int inode_num)
{   
    int i;
    for(i = 0 ; i < nodes[inode_num].acl; i++)
    {
        ;
    }
    return 0;
}

void acl_update(struct inode *nodes, struct journal *jou, int inode_num)
{
    int i;
    int j;
    int phist = 0;
    
    if(inode_num == 0)
    {
        return;
    }

    if (is_updated(nodes[inode_num - 1].hist, jou) == 0)
    {
        return;
    }
    acl_update(nodes, jou, inode_num - 1); // update parent first
    
    if (inode_num != 1)
    {
        phist = nodes[inode_num - 1 - 1].hist;
    }
    for (i = 0 ; i < NUM ; i++)
    {
        if(check_acl(nodes, inode_num) != 0)
        {
            continue;
        }

        if(jou[i].inode_number == inode_num)
        {
            nodes[inode_num - 1].acl += jou[i].acl;
            nodes[inode_num - 1].hist += jou[i].serial_number;
        }
        else
        {
            if ((jou[i].serial_number & phist) != 0)
            {
                nodes[inode_num - 1].acl += jou[i].acl;
                nodes[inode_num - 1].hist += jou[i].serial_number;
            }
        }

    }

}

int main(int argc,char *args[]) { 
    int fd;
    int i;
    int j;
    struct journal jou[NUM];
    struct inode nodes[INUM];
    

    create_journal(jou, NUM);

    for (i = 0 ; i < NUM; i++)
    {
        printf("journal : %d --> ", i);
        printf("serial=%d, inode=%d, user=%d, acl=%d", jou[i].serial_number, jou[i].inode_number, jou[i].user, jou[i].acl);
        printf("\n");
    }
    
    create_inode(nodes, INUM);

    for (i = 0 ; i < INUM; i++)
    {
        printf("inode : (num, acli, hist) = (%d, %d, %d)\n", nodes[i].num, nodes[i].acl, nodes[i].hist);
    }
    
   // acl_update(nodes, jou, INUM);
    
    printf("\n\n");
    for (i = 0 ; i < INUM; i++)
    {
        printf("inode : (num, acli, hist) = (%d, %d, %d)\n", nodes[i].num, nodes[i].acl, nodes[i].hist);
    }
    /*
    char path[512] = {0};
    for (i = 0 ; i < 50000 ; i++)
    {
        sprintf(path, "sub1/file_%d", i);
        fd = open(path,O_RDWR|O_CREAT|O_TRUNC); 
        if((fd < 0)){ 
            printf("Open Error!Check if the file is exist and you have the permission!\n"); 
            exit(1); 
        }  
        for (j = 0 ; j < 256 ; j++)
        {
            write(fd,"1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",1024);//这里需要用读取到的字符数,否则会出错,因为buff数组有可能未被全覆盖 
        }
        fsetxattr(fd, "user.test", "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111", 256, XATTR_CREATE);
        close(fd); 
    } 
    */
    exit(0); 

} 
