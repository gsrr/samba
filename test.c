#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>

int num = 10;
int inum = 50;
int aclnum = 5;

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
    int cnt;
    struct acl *iacl;
};

struct inode {
    int num;
    int hist;
    int cnt;
    struct acl *iacl;
};

void create_journal(struct journal *jou, int num)
{
    int i;
    int j;
    int user = 100001;
    int base = 1;
    for (i = 0 ; i < num ; i++)
    {
        jou[i].serial_number = base;
        jou[i].inode_number = 1;
        jou[i].user = 10000 + i + 1;
	jou[i].cnt = aclnum;
        jou[i].iacl = (struct acl*)malloc(sizeof(struct acl) * aclnum);
        user += 1;
        base *= 2;
        for (j = 0 ; j < aclnum ; j++)
        {
           jou[i].iacl[j].user = 10000 + j + i;
           jou[i].iacl[j].perm = 3;
           jou[i].iacl[j].scope = 3;
           jou[i].iacl[j].inherit = 0;
        }
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
	nodes[i].cnt = 2 * aclnum;
        nodes[i].iacl = (struct acl*)malloc(sizeof(struct acl) * (aclnum * 2));
        struct acl *tacl = nodes[i].iacl;
        for (j = 0 ; j < aclnum ; j++)
        {
            tacl[j].user = 10000 + j;
            tacl[j].perm = 3;
            tacl[j].scope = 3;
            tacl[j].inherit = 0;
        }
        for (j = 0 ; j < aclnum ; j++)
        {
            tacl[j + aclnum].user = 10000 + j + aclnum;
            tacl[j + aclnum].perm = 3;
            tacl[j + aclnum].scope = 3;
            tacl[j + aclnum].inherit = 1;
        }
    }
}

int is_updated(int hist, struct journal *jou)
{
     int i;
     for (i = 0 ; i < num ; i++)
     {
        if(jou[i].serial_number & hist == 0)
        {
            return -1;
        }
     }
     return 0;
}

int check_acl(int user, struct inode *nodes, int inode_num)
{   
    int i;
    int cnt = nodes[inode_num - 1].cnt;
    struct acl *tacl = nodes[inode_num - 1].iacl;	
    int mask = 0;
    for(i = 0 ; i < cnt ; i++)
    {
       mask |= tacl[i].perm;     
    }
    if ((mask & 1) == 0)
        return -1;
    return 0;
}

struct acl* dupACL(struct acl *jacl, int jcnt)
{
        int i;
        int j;
        struct acl *dacl = (struct acl*)malloc(sizeof(struct acl) * jcnt);
        memcpy(dacl, jacl, sizeof(struct acl) * jcnt);
        return dacl;       
}   

int mergeCnt(struct acl *iacl, int icnt, struct acl *jacl, int jcnt)
{
   int mcnt = 0;
   int i;
   int j;
   for(i = 0 ; i < icnt ; i++)
   {
        if (iacl[i].inherit == 0)
        {
                mcnt += 1;
        }
   }
   for(j = 0 ; j < jcnt ; j++)
   {
        if(jacl[j].scope != 0)
        {
                mcnt += 1;
        }
   }
   return mcnt;     
}

struct acl* mergeACL(struct acl *iacl, int icnt, struct acl *jacl, int jcnt, int mcnt)
{
   int i;
   int j;
   int k = 0;
   struct acl *macl = (struct acl*)malloc(sizeof(struct acl) * mcnt);
   for(i = 0 ; i < icnt ; i++)
   {
        if (iacl[i].inherit == 0)
        {
                macl[k].user = iacl[i].user;
                macl[k].perm = iacl[i].perm;
                macl[k].scope = iacl[i].scope;
                macl[k].inherit = 0;
                k++;
        }
   }
   for(j = 0 ; j < jcnt ; j++)
   {
        if(jacl[j].scope != 0)
        {
                macl[k].user = jacl[j].user;
                macl[k].perm = jacl[j].perm;
                macl[k].scope = jacl[j].scope;
                macl[k].inherit = 1;
                k++;
        }
   }
   return macl;    
   
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

    for (i = 0 ; i < num ; i++)
    {
        if(check_acl(jou[i].user, nodes, inode_num) != 0)
        {
            continue;
        }

        if(jou[i].inode_number == inode_num)
        {
            struct acl *dacl = dupACL(jou[i].iacl, jou[i].cnt);
            free(nodes[inode_num - 1].iacl);
            nodes[inode_num - 1].cnt = jou[i].cnt;
            nodes[inode_num - 1].iacl = dacl;
            nodes[inode_num - 1].hist += jou[i].serial_number;
        }
        else
        {
            if ((jou[i].serial_number & phist) != 0)
            {
                int mcnt = mergeCnt(nodes[inode_num - 1].iacl, nodes[inode_num - 1].cnt, jou[i].iacl, jou[i].cnt);
                struct acl *macl = mergeACL(nodes[inode_num - 1].iacl, nodes[inode_num - 1].cnt, jou[i].iacl, jou[i].cnt, mcnt);
                free(nodes[inode_num - 1].iacl);
                nodes[inode_num - 1].cnt = mcnt;
                nodes[inode_num - 1].iacl = macl;
                nodes[inode_num - 1].hist += jou[i].serial_number;
            }
        }

    }

}

void clear_hist_all(struct inode *nodes)
{
    int i;
    for(i = 0 ; i < inum ; i++)
    {
        nodes[i].hist = 0;
    }
}

void clear_hist(struct inode *nodes)
{
    nodes[inum - 1].hist = 0;
}

int main(int argc,char *argv[]) { 
    float total_time = 0;
    int fd;
    int i;
    int j;
    struct journal jou[num];
    struct inode nodes[inum];
    struct timeval st, et;

    int debug = atoi(argv[1]);
    int mode = atoi(argv[2]);
    int files = atoi(argv[3]);
    int enableACL = atoi(argv[4]);
    int enableSET = atoi(argv[5]);
    num = atoi(argv[6]);
    inum = atoi(argv[7]);
    aclnum = atoi(argv[8]);

    printf("args: debug=%d, mode=%d, files=%d, acl=%d, setxattr=%d, num=%d, inum=%d, aclnum=%d\n", debug, mode, files, enableACL, enableSET, num, inum, aclnum);
    create_journal(jou, num);
    create_inode(nodes, inum);
    
    if(debug == 1)
    {
        for (i = 0 ; i < num; i++)
        {
            printf("journal : %d --> ", i);
            printf("serial=%d, inode=%d, user=%d\n", jou[i].serial_number, jou[i].inode_number, jou[i].user);
            for (j = 0 ; j < aclnum ; j++)
            {
                printf("jou's acl : (index, user, perm, scope, inherit) = (%d, %d, %d, %d, %d)\n", j + 1, jou[i].iacl[j].user, jou[i].iacl[j].perm, jou[i].iacl[j].scope, jou[i].iacl[j].inherit);
            }
            printf("\n");
        }

        for (i = 0 ; i < inum; i++)
        {
            printf("inode : (num,  hist) = (%d, %d)\n", nodes[i].num, nodes[i].hist);
            for (j = 0 ; j < 2*aclnum ; j++)
            {
                printf("acl : (index, user, perm, scope, inherit) = (%d, %d, %d, %d, %d)\n", j + 1, nodes[i].iacl[j].user, nodes[i].iacl[j].perm, nodes[i].iacl[j].scope, nodes[i].iacl[j].inherit);
            }
        }
    }
    /*
        acl_update(nodes, jou, inum);
    printf("after\n\n");
    for (i = 0 ; i < inum; i++)
    {
        printf("inode : (num,  hist) = (%d, %d)\n", nodes[i].num, nodes[i].hist);
        for (j = 0 ; j < nodes[i].cnt ; j++)
        {
            printf("acl : (index, user, perm, scope, inherit) = (%d, %d, %d, %d, %d)\n", j + 1, nodes[i].iacl[j].user, nodes[i].iacl[j].perm, nodes[i].iacl[j].scope, nodes[i].iacl[j].inherit);
        }
    }
*/
    char path[512] = {0};
    for (i = 0 ; i < files ; i++)
    {
        sprintf(path, "sub1/file_%d", i);
        fd = open(path,O_RDWR|O_CREAT|O_TRUNC); 
        if((fd < 0)){ 
            printf("Open Error!Check if the file is exist and you have the permission!\n"); 
            exit(1); 
        }  
        if(mode == 1)
            clear_hist_all(nodes);
        else
            clear_hist(nodes);
        if(enableACL == 1)
        {
            
            gettimeofday(&st,NULL);
            acl_update(nodes, jou, inum);
            gettimeofday(&et,NULL);
            int elapsed = ((et.tv_sec - st.tv_sec) * 1000000) + (et.tv_usec - st.tv_usec);
            //printf("elapsed:%d\n", elapsed);
            total_time += elapsed;
        }
        /*
        for (j = 0 ; j < 256 ; j++)
        {
            write(fd,"1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",1024); 
        }
        */
        if(enableSET == 1)
        {
            fsetxattr(fd, "user.test", "1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111", 256, XATTR_CREATE);
        }
        close(fd); 
    } 
    printf("Elapse:%f\n", total_time/1000000);
    exit(0); 

} 
