#include "ift_journal.h"

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
    int i;
    int j;
    int k;
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
                bufferLen += (4 + 4 + 2 + 4 + 4 + 4 + 11 * jouTable->es->cnt);         
                printf("dump table : tmp->path :%s\n", tmp->path);
                if(tmp->path != NULL)
                {
                    bufferLen += 4; // length;
                    bufferLen += strlen(tmp->path);
                }
                else
                {
                    bufferLen += 4; // length = 0;
                }
                tmp = tmp->next;
            }
        }
    }
    buffer = (char*) malloc(sizeof(char) * bufferLen);
    iftacl_push_to_blob(buffer, &(jouTable->serial), 4, &offset); 
    iftacl_push_to_blob(buffer, &(jouTable->pid), 4, &offset); 
    iftacl_push_to_blob(buffer, &(jouTable->vvid), 4, &offset); 
    iftacl_push_to_blob(buffer, &(jouTable->cnt), 4, &offset); 
    if(jouTable->es != NULL)
    {
        struct entryIftacl *tmp = jouTable->es;
        int length;
        while(tmp != NULL)
        {
            if(tmp->path == NULL)
            {
                length = 0;
            }
            else
            {
                length = strlen(tmp->path);
            }
            iftacl_push_to_blob(buffer, &(tmp->serial), 4, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->aid), 4, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->op), 2, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->user), 4, &offset); 
            iftacl_push_to_blob(buffer, &(tmp->inumber), 4, &offset); 
            iftacl_push_to_blob(buffer, &length , 4, &offset); 
            printf("length:%d\n", length);
            for(k = 0 ; k < length ; k++)
            {
                iftacl_push_to_blob(buffer, &(tmp->path[k]), 1, &offset); 
            }
            iftacl_push_to_blob(buffer, &(tmp->cnt), 4, &offset); 
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


// acl = "aid:op:user:inumber:cnt:aces", 0:0:0:1087985:2:0;0;2032031;1;100001:0;0;1179785;3;0
void add_jou_entry(char *acl)
{
    struct entryIftacl *tentry = (struct entryIftacl*)malloc(sizeof(struct entryIftacl)); 
    int cnt = 0;
    tentry->serial = jouTable->serial; // critical section
    jouTable->serial += 1;
    char *rest = acl;
    char *token;
    while(token = strtok_r(rest, ":", &rest))
    {
        printf("token:%s\n", token);
        if(cnt == 0)
        {
            tentry->aid = atoi(token);
        }
        if(cnt == 1)
        {
            tentry->op = atoi(token);
        }
        else if(cnt == 2)
        {
            tentry->user = atoi(token);
        }
        else if(cnt == 3)
        {
            tentry->inumber = atoi(token);
        }
        else if(cnt == 4)
        {
            int length = strlen(token);
            if(length == 1)
            {
                tentry->path = NULL;
            }
            else
            {
                tentry->path = (char*)malloc(sizeof(char) * (length + 1));
                strncpy(tentry->path, token, length);
                tentry->path[length] = '\0';
            }
        }
        else if(cnt == 5)
        {
            tentry->cnt = atoi(token);
            tentry->aces = (struct iftacl_aces*)malloc(sizeof(struct entryIftacl) * (tentry->cnt));
        }
        if(cnt > 5)
        {
            printf("token : %s\n", token); 
            char *rest2 = token;
            char *acetoken;
            int cnt2 = 0;
            while(acetoken = strtok_r(rest2, ";", &rest2))
            {
                printf("\tacetoken: %s\n", acetoken);
                int index = cnt - 6;
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
        struct entryIftacl *tes = jouTable->es;
        while(tes->next != NULL)
        {
            tes = tes->next;
        }
        tes->next = tentry;
    }
    jouTable->cnt += 1;
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
