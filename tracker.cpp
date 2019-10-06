#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<iostream>
#include<vector>
#include<sstream>

#define BUFFSIZE 512

using namespace std;

struct trackerdata
{
    string ip;
    string port;
    string group_id;
    string hash;
    string filename;
};

vector<struct trackerdata> arr;

void *func(void * arg)
{
    int *cid=(int*)arg;
    int cval=*cid;
    int command;

    recv(cval,( void*)&command,sizeof(command),0);

    cout<<command<<"\n";

    if(command==0)
    {
        int nsockid;

        char buffer[BUFFSIZE];

        recv(cval,( void*)buffer,sizeof(buffer),0);
        cout<<"sending port\n";

        for(auto x:arr)
        {
            if(x.filename.compare(string(buffer))==0)
            {
                string temp=x.ip+" "+x.port;
                memset(buffer,'\0',BUFFSIZE);

                strcpy(buffer,temp.c_str());
                send(cval,(const void*)buffer,sizeof(buffer),0);

                break;
            }
            
        }
    }
    else if(command==1)
    {
        cout<<"Uploading a file\n";
        string filename;
        char buffer[BUFFSIZE];
        recv(cval,(void*)buffer,sizeof(buffer),0);
        cout<<buffer<<"\n";

        stringstream ss(buffer);

        struct trackerdata temp;
        ss>>temp.filename;
        ss>>temp.ip;
        ss>>temp.port;

        memset(buffer,'\0',BUFFSIZE);
        string hash="";
        int n;
        while((n=recv(cval,buffer,BUFFSIZE,0))>0 )
        {
            hash=hash+buffer;
            // cout<<n<<"\n";
            memset(buffer,'\0',BUFFSIZE);
        }

        cout<<"Recieved hash is "<<hash<<"\n";
        temp.hash=hash;
        arr.push_back(temp);

        cout<<arr[0].ip<<"\n";
        cout<<"Upload done\n";
    }
    close(cval);
}

int main()
{
    int sockid=socket(AF_INET,SOCK_STREAM,0);
    int len,file_size,n;
    FILE *fp;
    int cid;
    char buffer[BUFFSIZE];
    struct sockaddr_in addr;

    addr.sin_family=AF_INET;
    addr.sin_port=htons(2000);
    addr.sin_addr.s_addr=inet_addr("127.0.0.1");

    len=sizeof(addr);

    bind(sockid,(struct sockaddr*)&addr,sizeof(addr));

    pthread_t ids[BUFFSIZE];
    int count=0;

    cout<<"Before listen\n";
    listen(sockid,3);

    int command;
    while(1)
    {
        cid=accept(sockid,(struct sockaddr*)&addr,(socklen_t*)&len);

        pthread_create(&ids[count],NULL,func,(void*)&cid);
        pthread_detach(ids[count++]); 
    }
    fclose(fp);
}