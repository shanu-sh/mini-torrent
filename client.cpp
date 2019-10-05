#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<iostream>
#include<sstream>

#define BUFFSIZE 512

using namespace std;

struct host_data
{
    string ip;
    int port;
};

void send2tracker(string tracker_ip,int tracker_port,string ip,int port,string filename)
{
    int sockid,n;
    int control=1;
    char buffer[BUFFSIZE];

    sockid=socket(AF_INET,SOCK_STREAM,0);

    if(sockid<0)
    {
        perror("Error in socket creation\n");
        exit(1);
    }

    struct sockaddr_in serveraddr;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(tracker_port);
    serveraddr.sin_addr.s_addr=inet_addr(tracker_ip.c_str());

    filename=filename+" "+ip+" "+to_string(port)+" ";

    char data[BUFFSIZE];

    strcpy(data,filename.c_str());
    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    send(sockid,(const void*)&control,sizeof(control),0);
    send(sockid,(const void*)data,sizeof(data),0);

    close(sockid);
}

void recvfromtracker(string ip,int port)
{
    int control=0;
    string filename;
    int sockid,n,cid,len;
    char buffer[BUFFSIZE];

    sockid=socket(AF_INET,SOCK_STREAM,0);

    if(sockid<0)
    {
        perror("Error in socket creation\n");
        exit(1);
    }

    cin>>filename;

    char data[BUFFSIZE];
    strcpy(data,filename.c_str());

    struct sockaddr_in serveraddr;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(port);
    serveraddr.sin_addr.s_addr=inet_addr(ip.c_str());

    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    //Sends the control information to client
    send(sockid,(const void*)&control,sizeof(control),0);
    send(sockid,(const void *)data,sizeof(data),0);

    //Wait  for server to communicate back

    memset(data,'\0',BUFFSIZE);
    recv(sockid,data,BUFFSIZE,0);
    
    stringstream ss(data);

    int dport;
    string dip,dp;
    ss>>dip;
    ss>>dp;
    dport=stoi(dp);
    cout<<dport<<"\n";
    cout<<dip<<"\n";
    close(sockid);

    //Now connect to client to download file (client to client communication)

    sockid=socket(AF_INET,SOCK_STREAM,0);
    cout<<"Socket created\n";
    if(sockid<0)
    {
        perror("Error in socket creation\n");
        exit(1);
    }

    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(dport);
    serveraddr.sin_addr.s_addr=inet_addr(dip.c_str());

    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    long filesize;
    recv(sockid, &filesize, sizeof(filesize), 0);

   
    FILE *fp=fopen("temp.jpg","wb");
    
    while(filesize>0 && (n=recv(sockid,buffer,BUFFSIZE,0))>0 )
    {
        fwrite(buffer,sizeof(char),n,fp);
        // cout<<n<<"\n";
        memset(buffer,'\0',BUFFSIZE);
        filesize=filesize-n;
        // cout<<filesize<<"\n";
    }
    // cout<<"outer "<<filesize<<"\n";
    close(sockid);
    fclose(fp);
}

void *funcd(void * arg)
{
    int sockid=socket(AF_INET,SOCK_STREAM,0);
    int len;
    int cid;
    int n;

    struct host_data *data=(struct host_data*)arg;
    char buffer[BUFFSIZE];
    struct sockaddr_in addr;

    addr.sin_family=AF_INET;
    addr.sin_port=htons(data->port);
    addr.sin_addr.s_addr=inet_addr(data->ip.c_str());

    len=sizeof(addr);

    bind(sockid,(struct sockaddr*)&addr,sizeof(addr));

    listen(sockid,3);

    while(1)
    {
        cid=accept(sockid,(struct sockaddr*)&addr,(socklen_t*)&len);

        //First recevive file name

        //recv(cid,buffer,sizeof(buffer),0);


        //cout<<buffer<<"\n";

        FILE *fp;
        fp=fopen("pic.jpg","rb");
        fseek(fp,0,SEEK_END);

        long size=ftell(fp);
        rewind(fp);

        send(cid,&size,sizeof(size),0);

        while((n=fread(buffer,sizeof(char),BUFFSIZE,fp))>0 && size>0)
        {
            send(cid,buffer,n,0);
            memset(buffer,'\0',BUFFSIZE);
            size=size-n;
        }

        fclose(fp);
    }
}

int main(int argc,char **argv)
{
    if(argc<3)
    {
        cout<<"Please enter three arguments\n";
        return 1;
    }

    string ip=argv[1];
    int port=stoi(argv[2]);
    string cmd;

    struct host_data data;

    data.ip=ip;
    data.port=port;

    pthread_t id;
    pthread_create(&id,NULL,funcd,(void*)&data);
    pthread_detach(id);

    string filename;

    string tracker_ip="127.0.0.1";
    int tracker_port=2000;

    while(1)
    {
        cin>>cmd;

        if(cmd.compare("download")==0)
        {
            recvfromtracker(tracker_ip,tracker_port);
        }

        if(cmd.compare("upload")==0)
        {
            cin>>filename;
            send2tracker(tracker_ip,tracker_port,ip,port,filename);
        }
    }   
}