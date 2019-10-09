#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<iostream>
#include<sstream>
#include<openssl/sha.h>

#define BUFFSIZE 524288

using namespace std;

struct host_data
{
    string ip;
    int port;
};

bool login_status=false;

string computehash(string str)
{
    unsigned char temp[SHA_DIGEST_LENGTH];
    char buf[SHA_DIGEST_LENGTH*2];
 
    string hash="";

    memset(buf,'\0', SHA_DIGEST_LENGTH*2);
    memset(temp,'\0', SHA_DIGEST_LENGTH);
 
    SHA1((unsigned char *)str.c_str(), strlen(str.c_str())-1, temp);
 
    for (int i=0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf((char*)&(buf[i]), "%x", temp[i]);
        hash=hash+buf[i];
    }

    return hash;
}

void send2tracker(string tracker_ip,int tracker_port,string ip,int port,string filename,int groupid)
{
    int sockid,n;
    int control=1;
    char buffer[BUFFSIZE];

    string file=filename;

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

    filename=filename+" "+ip+" "+to_string(port)+" "+to_string(groupid)+" ";

    char data[BUFFSIZE];

    strcpy(data,filename.c_str());
    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    //Sending the control info and file name to tracker

    send(sockid,(const void*)&control,sizeof(control),0);
    send(sockid,(const void*)data,sizeof(data),0);


    //Now we need to create hash and send to the tracker

    FILE *fp;
    fp=fopen(file.c_str(),"rb");
    if(fp==NULL)
    {
        cout<<file;
        cout<<"FIle not Present\n";
        return;
    }
    
    fseek(fp,0,SEEK_END);

    long size=ftell(fp);
    rewind(fp);

    string hash;
    int chunkcount=0;
    //Reading file and calculating hash and sending to tracker

    while((n=fread(buffer,sizeof(char),BUFFSIZE,fp))>0 && size>0)
    {
        //send(cid,buffer,n,0);

        //Compute hash value
        
        hash=computehash(buffer);

        cout<<"\nHash computed is ";
        cout<<hash<<"\n";
        memset(buffer,'\0',BUFFSIZE);

        strcpy(buffer,hash.c_str());
        send(sockid,buffer,SHA_DIGEST_LENGTH*2,0);

        memset(buffer,'\0',BUFFSIZE);

        chunkcount++;
        size=size-n;
    }

    cout<<"No of chunks created "<<chunkcount<<"\n";
    fclose(fp);

    close(sockid);
}

void recvfromtracker(string ip,int port)
{
    int control=0;
    string filename;
    int group_id;
    int sockid,n,cid,len;
    char buffer[BUFFSIZE];

    sockid=socket(AF_INET,SOCK_STREAM,0);

    if(sockid<0)
    {
        perror("Error in socket creation\n");
        exit(1);
    }

    cin>>filename;
    cin>>group_id;

    char data[BUFFSIZE];

    string tempdata=filename+" "+to_string(group_id)+" ";
    strcpy(data,tempdata.c_str());

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

    if(dip.compare("not_present")==0)
    {
        cout<<"File not uploaded in tracker\n";
        return;
    }
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

    //sending filename to the other client

    strcpy(buffer,filename.c_str());
    send(sockid, buffer, sizeof(buffer), 0);


    recv(sockid, &filesize, sizeof(filesize), 0);

   
    FILE *fp=fopen(filename.c_str(),"wb");
    
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

        recv(cid,buffer,sizeof(buffer),0);
        cout<<buffer<<"\n";

        FILE *fp;
        fp=fopen(buffer,"rb");
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

void create_user(string tracker_ip,int tracker_port,string user_id,string password)
{
    int sockid,n;
    int control=2;
    char buffer[BUFFSIZE];
    string data;

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

    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    send(sockid,(const void*)&control,sizeof(control),0);

    data=user_id+" "+password;
    strcpy(buffer,data.c_str());

    send(sockid,(const void*)buffer,sizeof(buffer),0);

    recv(sockid,(void *)buffer,sizeof(buffer),0);

    stringstream ss(buffer);

    data="";
    ss>>data;

    if(data.compare("user_created")==0)
        cout<<"User created successfully\n";
    else
        cout<<"User creation unsuccessful\n";

}

bool authenticate(string tracker_ip,int tracker_port,string user_id,string password)
{
    int sockid,n;
    int control=3;
    char buffer[BUFFSIZE];
    string data;

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

    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    send(sockid,(const void*)&control,sizeof(control),0);

    data=user_id+" "+password;
    strcpy(buffer,data.c_str());
    send(sockid,(const void*)buffer,sizeof(buffer),0);

    recv(sockid,(void *)buffer,sizeof(buffer),0);

    stringstream ss(buffer);

    data="";
    ss>>data;

    if(data.compare("logged_in")==0)
        return true;
    else
        return false;

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

    string user_id,password;

    struct host_data data;

    data.ip=ip;
    data.port=port;

    pthread_t id;
    pthread_create(&id,NULL,funcd,(void*)&data);
    pthread_detach(id);

    string filename;
    int groupid;
    string tracker_ip="127.0.0.1";
    int tracker_port=2000;

    while(1)
    {
        cin>>cmd;

        if(cmd.compare("download")==0)
        {
            if(login_status)
                recvfromtracker(tracker_ip,tracker_port);
            else
            {
                cout<<"Please log in \n";
            }
            
        }

        if(cmd.compare("upload")==0)
        {
            if(login_status)
            {
                cin>>filename;
                cin>>groupid;
                send2tracker(tracker_ip,tracker_port,ip,port,filename,groupid);
            }
            else
            {
                cout<<"Please log in \n";
            }
            
        }

        if(cmd.compare("create_user")==0)
        {          
            cout<<"Enter user name\n";
            cin>>user_id;

            cout<<"Enter pasword\n";
            cin>>password;

            create_user(tracker_ip,tracker_port,user_id,password);
        }

        if(cmd.compare("login")==0)
        {
            cout<<"Enter user name\n";
            cin>>user_id;

            cout<<"Enter pasword\n";
            cin>>password;

            if(authenticate(tracker_ip,tracker_port,user_id,password))
            {
                login_status=true;
                cout<<"Logged in\n";
            }
            else
                cout<<"Please check your user_id and password\n";
        }

        if(cmd.compare("logout")==0)
        {
            if(login_status==false)
                cout<<"Please login first\n";
            else
            {
                login_status=false;
                cout<<"Logged out\n";
            }
            
        }

    }   
}