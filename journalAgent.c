#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <signal.h>

#define JOURNALSUCCESS  0
#define JOURNALINIT     0
#define JOURNALADD      1
#define JOURNALSHOW     2
#define JOURNALDEL       3

#define ACLSET     0
#define ACLREPLACE 1
#define ACLMOVE    2

void iftacl_push_to_blob(char* blob, void *value, int size, int *offset)
{
    int i = *offset;
    int* ivalue = (int*) value;
    if(size == 4)
    {
        blob[i] = (*ivalue >> 24) & 0xFF;
        blob[i + 1] = (*ivalue >> 16) & 0xFF;
        blob[i + 2] = (*ivalue >> 8) & 0xFF;
        blob[i + 3] = (*ivalue) & 0xFF;
    }
    else if(size == 2)
    {
        blob[i] = (*ivalue >> 8) & 0xFF;
        blob[i + 1] = (*ivalue) & 0xFF;
    }
    else if(size == 1)
    {
        blob[i] = (*ivalue) & 0xFF;
    }
    *offset += size;
}

/* Big endian */
int read_bytes(char* value, int size, int* offset)
{
    int num = 0;
    int i;
    for(i = 0 ; i < size ; i++)
    {
        num = num << 8;
        num |= (value[*offset + i] & 0xff);
    }
    *offset += size;
    return num;
}

struct iftacl_aces
{
    char type;
    char flags;
    unsigned int access;
    char role;
    unsigned int uid;
};

struct entryIftacl {
    int serial;
    unsigned short int op; // setAcl, replaceAcl, move
    unsigned int user;
    int inumber;  //64 bits?
    int cnt;
    struct iftacl_aces *aces;
    struct entryIftacl *next;
};

struct journalIftacl {
    int serial;
    int pid;
    int vvid;
    int cnt;
    struct entryIftacl *es; // 4 + 2 + 4 + 4 + 4 + 11 * cnt;
};

struct journalIftacl *jouTable; 

int journal_init(int pid, int vvid, struct entryIftacl *es)
{
    printf("journal_init\n");
    struct journalIftacl *tmpTable = (struct journalIftacl*) malloc(sizeof(struct journalIftacl));
    tmpTable->serial = 0;
    tmpTable->pid = pid;
    tmpTable->vvid = vvid;
    if(es == NULL)
    {
        tmpTable->cnt = 0;
        tmpTable->es = NULL;
    }
    else
    {
        // init entries
    }
    jouTable = tmpTable;
    return JOURNALSUCCESS;
}

int dump_jouTable()
{
    printf("dump_jouTable\n");
    int i;
    int j;
    int fd;
    char path[512] = {0};
    int bufferLen = 0;
    char *buffer;
    int offset = 0;

    bufferLen = 4 + 4 + 4 + 4;
    if(jouTable->es != NULL)
    {
        for (i = 0 ; i < jouTable->cnt ; i++)
        {
            struct entryIftacl *tmp = jouTable->es;
            while(tmp != NULL)
            {
                bufferLen += (4 + 2 + 4 + 4 + 4 + 11 * jouTable->es->cnt);
                tmp = tmp->next;
            }
        }
    }
    buffer = (char*) malloc(sizeof(char) * bufferLen);
    iftacl_push_to_blob(buffer, &(jouTable->serial), 4, &offset); 
    iftacl_push_to_blob(buffer, &(jouTable->pid), 4, &offset); 
    iftacl_push_to_blob(buffer, &(jouTable->vvid), 4, &offset); 
    iftacl_push_to_blob(buffer, &(jouTable->cnt), 4, &offset); 
    printf("jouTable->cnt:%d\n", jouTable->cnt);
    if(jouTable->es != NULL)
    {
        struct entryIftacl *tmp = jouTable->es;
        while(tmp != NULL)
        {
            iftacl_push_to_blob(buffer, &(tmp->serial), 4, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->op), 2, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->user), 4, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->inumber), 4, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->cnt), 4, &offset); 
            printf("tmp->cnt:%d\n", tmp->cnt);
            for (j = 0 ; j < tmp->cnt ; j++)
            {
                iftacl_push_to_blob(buffer, &(tmp->aces[j].type), 1, &offset);
                iftacl_push_to_blob(buffer, &(tmp->aces[j].flags), 1, &offset);
                iftacl_push_to_blob(buffer, &(tmp->aces[j].access), 4, &offset);
                iftacl_push_to_blob(buffer, &(tmp->aces[j].role), 1, &offset);
                iftacl_push_to_blob(buffer, &(tmp->aces[j].uid), 4, &offset);
            }
            tmp = tmp->next;
        }
    }
    sprintf(path, "./%d", jouTable->vvid);
    fd = open(path,O_RDWR|O_CREAT|O_TRUNC); 
    write(fd, buffer, bufferLen);
    close(fd);
    free(buffer);
}

void insert_jou_table(struct journalIftacl *tmpTable, struct entryIftacl *e)
{
    printf("insert jou table\n");
    if(tmpTable->es == NULL)
    {
        tmpTable->es = e;
        return;
    }
    else
    {
        struct entryIftacl *tmp = tmpTable->es;
        while(tmp->next != NULL)
        {
            tmp = tmp->next;
        }
        tmp->next = e;
        return;
    }
}

void read_jou_entry(struct journalIftacl *tmpTable, char *buffer, int *offset)
{
    int i;
    int j;
    if(tmpTable->cnt == 0)
    {
        return;
    }

    printf("entry->cnt:%d\n", tmpTable->cnt);
    for(i = 0 ; i < tmpTable->cnt ; i++)
    {
        struct entryIftacl *tes  = (struct entryIftacl*)malloc(sizeof(struct entryIftacl));
        tes->serial = read_bytes(buffer, 4, offset);        
        tes->op = read_bytes(buffer, 2, offset);        
        tes->user = read_bytes(buffer, 4, offset);        
        tes->inumber = read_bytes(buffer, 4, offset);        
        tes->cnt = read_bytes(buffer, 4, offset);        
        tes->aces = (struct iftacl_aces*)malloc(sizeof(struct iftacl_aces) * (tes->cnt));
        printf("tes->cnt:%d\n", tes->cnt);
        for(j = 0 ; j < tes->cnt ; j++)
        {
            tes->aces[j].type = read_bytes(buffer, 1, offset);
            tes->aces[j].flags = read_bytes(buffer, 1, offset);
            tes->aces[j].access = read_bytes(buffer, 4, offset);
            tes->aces[j].role = read_bytes(buffer, 1, offset);
            tes->aces[j].uid = read_bytes(buffer, 4, offset);
        }
        tes->next = NULL;
        insert_jou_table(tmpTable, tes);
    }
}

void read_jou_table(char *path)
{
    int i;
    int offset = 0;
    char buffer[8192] = {0};
    int fd = open(path,O_RDWR);
    int ret = read (fd, &buffer, 8192);
    printf("jou table size:%d\n", ret);
    struct journalIftacl *tmpTable = (struct journalIftacl*)malloc(sizeof(struct journalIftacl));
    tmpTable->serial = read_bytes(buffer, 4, &offset);
    tmpTable->pid = read_bytes(buffer, 4, &offset);
    tmpTable->vvid = read_bytes(buffer, 4, &offset);
    tmpTable->cnt = read_bytes(buffer, 4, &offset);
    tmpTable->es = NULL;
    
    read_jou_entry(tmpTable, buffer, &offset);
    jouTable = tmpTable;
    close(fd);
}

// acl = "op:user:inumber:cnt:aces", 0:0:1087985:2:0;0;2032031;1;100001:0;0;1179785;3;0
void add_jou_entry(char *acl)
{
    printf("add journal entry\n");
    struct entryIftacl *tentry = (struct entryIftacl*)malloc(sizeof(struct entryIftacl)); 
    int cnt = 0;
    tentry->serial = jouTable->serial; // critical section
    jouTable->serial += 1;
    char *rest = acl;
    char *token;
    while(token = strtok_r(rest, ":", &rest))
    {
        if(cnt == 0)
        {
            tentry->op = atoi(token);
        }
        else if(cnt == 1)
        {
            tentry->user = atoi(token);
        }
        else if(cnt == 2)
        {
            tentry->inumber = atoi(token);
        }
        else if(cnt == 3)
        {
            tentry->cnt = atoi(token);
            tentry->aces = (struct iftacl_aces*)malloc(sizeof(struct entryIftacl) * (tentry->cnt));
        }
        if(cnt > 3)
        {
            printf("token : %s\n", token); 
            char *rest2 = token;
            char *acetoken;
            int cnt2 = 0;
            while(acetoken = strtok_r(rest2, ";", &rest2))
            {
                printf("\tacetoken: %s\n", acetoken);
                int index = cnt - 4;
                if(cnt2 == 0)
                {
                    tentry->aces[index].type = atoi(acetoken); 
                }
                if(cnt2 == 1)
                {
                    tentry->aces[index].flags = atoi(acetoken); 
                }
                if(cnt2 == 2)
                {
                    tentry->aces[index].access = atoi(acetoken); 
                }
                if(cnt2 == 3)
                {
                    tentry->aces[index].role = atoi(acetoken); 
                }
                if(cnt2 == 4)
                {
                    tentry->aces[index].uid = atoi(acetoken); 
                }
                cnt2 += 1;
            }
        }
        cnt += 1;
    }
    tentry->next = NULL;
    if(jouTable->es == NULL)
    {
        jouTable->es = tentry;
    }
    else
    {
        printf("append tentry\n");
        struct entryIftacl *tes = jouTable->es;
        while(tes->next != NULL)
        {
            tes = tes->next;
        }
        printf("tes.cnt:%d\n", tes->cnt);
        printf("tentry.cnt:%d\n", tentry->cnt);
        tes->next = tentry;
    }
    jouTable->cnt += 1;
}

void show_jou_table()
{
    int j;
    printf("show journal table\n");
    printf("serial:%d\n", jouTable->serial);
    printf("pid:%d\n", jouTable->pid);
    printf("vvid:%d\n", jouTable->vvid);
    printf("cnt:%d\n", jouTable->cnt);

    struct entryIftacl *e = jouTable->es;
    while(e != NULL)
    {
        printf("\tserial:%d\n", e->serial);
        printf("\top:%d\n", e->op);
        printf("\tuser:%d\n", e->user);
        printf("\tinumber:%d\n", e->inumber);
        printf("\tace cnt:%d\n", e->cnt);
        
        for (j = 0 ; j < e->cnt ; j++)
        {
            printf("\t\ttype:%d\n", e->aces[j].type);
            printf("\t\tflag:%d\n", e->aces[j].flags);
            printf("\t\taccess:%d\n", e->aces[j].access);
            printf("\t\trole:%d\n", e->aces[j].role);
            printf("\t\tuid:%d\n\n", e->aces[j].uid);
            
        }
        e = e -> next;
    }
    
}

void send_signal()
{
    printf("send signal to %d\n", jouTable->pid);
    kill(jouTable->pid, SIGUSR1);
}

void del_jou_entry(int serial)
{
    printf("del_jou_entry\n");
    struct entryIftacl *p = NULL;
    struct entryIftacl *tmp = jouTable->es;
    if(tmp == NULL)
    {
        return;
    }

    while(tmp != NULL)
    {
        printf("tmp serial, serial = %d, %d\n", tmp->serial, serial);
        if(tmp->serial == serial)
        {
            break;
        }
        p = tmp;
        tmp = tmp->next;
    }
    if(tmp == NULL)
    {
        printf("Cant find entry\n");
        return;
    }
    if(p != NULL)
    {
        p->next = tmp->next;
    }
    else
    {
        jouTable->es = tmp->next;
    }
    free(tmp->aces);
    free(tmp);
    jouTable->cnt -= 1;
    if(jouTable->cnt == 0)
    {
        jouTable->es = NULL;
    }
}

int main(int argc, char *argv[])
{
    if (atoi(argv[1]) == JOURNALINIT)
    {
        int pid = atoi(argv[2]);
        int vvid = atoi(argv[3]);
        int ret = journal_init(pid, vvid, NULL);
        if (ret != JOURNALSUCCESS)
        {
            printf("init failed");
        }
        dump_jouTable();
    }    
    else if(atoi(argv[1]) == JOURNALADD)
    {
        read_jou_table(argv[2]);
        add_jou_entry(argv[3]);
        dump_jouTable();
        send_signal();
    }    
    else if(atoi(argv[1]) == JOURNALSHOW)
    {
        read_jou_table(argv[2]);
       show_jou_table();
    }
    else if(atoi(argv[1]) == JOURNALDEL)
    {
       read_jou_table(argv[2]);
       del_jou_entry(atoi(argv[3]));
       show_jou_table();
       dump_jouTable();
    }

}