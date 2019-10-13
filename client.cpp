#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<iostream>
#include<sstream>
#include<vector>
#include<openssl/sha.h>

#define BUFFSIZE 65536
#define PACKETSIZE 512

using namespace std;

struct client_data
{
    string filename;
    vector<string> chunks;
};

struct host_data
{
    string ip;
    int port;
    long filesize;
};

struct filetransfer_data
{
    int offset;
    int noofclients;
    string filename;
    string ip;
    int port;
    long filesize;
    int client;
};

vector<client_data> arr;
pthread_mutex_t lock;

FILE *tr;

bool login_status=true;

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


    FILE *fp;
    fp=fopen(file.c_str(),"r");

    struct client_data cdata;
    cdata.filename=file;

    if(fp==NULL)
    {
        cout<<file;
        cout<<" not Present\n";
        return;
    }
    
    fseek(fp,0,SEEK_END);
    long size=ftell(fp);
    rewind(fp);

    filename=filename+" "+ip+" "+to_string(port)+" "+to_string(groupid)+" "+to_string(size);

    char data[BUFFSIZE];

    strcpy(data,filename.c_str());
    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

    //Sending the control info and file name to tracker

    send(sockid,(const void*)&control,sizeof(control),0);
    send(sockid,(const void*)data,sizeof(data),0);


    //Now we need to create hash and send to the tracker

    string hash;
    int chunkcount=0;
    //Reading file and calculating hash and sending to tracker

    while( size>0&&(n=fread(buffer,sizeof(char),BUFFSIZE,fp))>0)
    {
        //send(cid,buffer,n,0);

        //Compute hash value
        
        hash=computehash(buffer);

        cout<<"\nHash computed is ";
        cout<<hash<<"\n";
        memset(buffer,'\0',BUFFSIZE);

        strcpy(buffer,hash.c_str());
        //send(sockid,buffer,SHA_DIGEST_LENGTH*2,0);

        memset(buffer,'\0',BUFFSIZE);

        cdata.chunks.push_back(to_string(chunkcount));

        chunkcount++;
        size=size-n;

    }

    arr.push_back(cdata);

    string fdata;

    fdata=cdata.filename+" ";
    for(int i=0;i<cdata.chunks.size();i++)
    {
        if(i!=cdata.chunks.size()-1)
            fdata=fdata+cdata.chunks[i]+" ";
        else
            fdata=fdata+cdata.chunks[i]+"\n";
    }

    strcpy(buffer,fdata.c_str());

    fwrite(buffer,sizeof(char),strlen(buffer),tr);
    fflush(tr);

    cout<<"No of chunks created "<<chunkcount<<"\n";
    cout<<"Last chunk size is "<<n<<"\n";

    memset(buffer,'\0',sizeof(buffer));
    string t=to_string(n)+" "+to_string(chunkcount);
    strcpy(buffer,t.c_str());

    cout<<buffer<<"\n";
    send(sockid,buffer,BUFFSIZE,0);
    

    fclose(fp);

    close(sockid);
}

FILE * fp;

void *requestforfilesinchunks(void * arg)
{

    pthread_mutex_lock(&lock);
    struct filetransfer_data *ptr=(struct filetransfer_data *)arg;

    struct filetransfer_data temp=*ptr;

    cout<<"is offest "<<temp.offset<<"\n";

    int i=temp.offset;
    int control,n;
    char buffer[BUFFSIZE];
    int noofclients=temp.noofclients;
    string filename=temp.filename;

    int port=temp.port;
    string ip=temp.ip;

    int sockid=socket(AF_INET,SOCK_STREAM,0);
    cout<<"Socket created\n";

    if(sockid<0)
    {
        perror("Error in socket creation\n");
        exit(1);
    }

    struct sockaddr_in serveraddr;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(port);
    serveraddr.sin_addr.s_addr=inet_addr(ip.c_str());

    connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
    control=1;
    send(sockid,(const void*)&control,sizeof(control),0);

    memset(buffer,'\0',BUFFSIZE);
    strcpy(buffer,filename.c_str());

    cout<<"Sending file name "<<buffer<<"\n";
    send(sockid, buffer, sizeof(buffer), 0);

    //Now send the file offset which you want to read
   
    strcpy(buffer,to_string(i).c_str());
    cout<<"Offset send is "<<buffer<<"\n";

    send(sockid, buffer,BUFFSIZE, 0);

    cout<<"Name sent to client server\n";
    //recv(sockid, &filesize, sizeof(filesize), 0);

    
    cout<<"Waiting for server to send data\n";

    //n=recv(sockid,buffer,BUFFSIZE,0);
    int packetsize=0;
    char packet[PACKETSIZE];

    rewind(fp);
    fseek(fp,i*BUFFSIZE,SEEK_SET);

    while(packetsize<BUFFSIZE && (n=recv(sockid,packet,PACKETSIZE,0))>0)
    {
        
        cout<<"Value read in packet is "<<buffer<<" and csize is "<<packetsize<<"\n";
        packetsize=packetsize+n;
        
        fwrite(packet,sizeof(char),n,fp);
        fflush(fp);

        if(n<PACKETSIZE)
            break;
    }

    cout<<"File written in client for offset "<<i<<"\n";
    close(sockid);
    pthread_mutex_unlock(&lock);
   
}

void recvfromtracker(string ip,int port)
{
    int control=0;
    string filename;
    int group_id;
    int sockid,n,cid,len;
    char buffer[BUFFSIZE];

    int dport;
    string dip,dp;

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

    //Sends the control information to tracker
    send(sockid,(const void*)&control,sizeof(control),0);
    send(sockid,(const void *)data,sizeof(data),0);

    //Wait  for server to communicate back

    memset(data,'\0',BUFFSIZE);
    //recv(sockid,data,BUFFSIZE,0);
    
    vector<host_data> hosts;

    long filesize;

    memset(buffer,'\0',BUFFSIZE);
    while((n=recv(sockid,buffer,BUFFSIZE,0))>0)
    {
        cout<<buffer<<"\n";
        stringstream ss(buffer);

        struct host_data hdata;
        ss>>dip;

        if(dip.compare("not_present")==0)
        {
            cout<<"File not uploaded in tracker\n";
            return;
        }

        ss>>dp;
        ss>>filesize;

        hdata.ip=dip;
        dport=stoi(dp);

        hdata.port=dport;
        hdata.filesize=filesize;
        hosts.push_back(hdata);
        cout<<"Host received is "<<hdata.ip<<" "<<hdata.port<<" "<<hdata.filesize<<" ";
        memset(buffer,'\0',BUFFSIZE);
    }

    cout<<"No of hosts present is "<<hosts.size()<<" \n";
    close(sockid);

    //Now connect to client server to get the total count of chunk present

    vector<int> chunkdata;
    for(int i=0;i<hosts.size();i++)
    {
        sockid=socket(AF_INET,SOCK_STREAM,0);
        cout<<"Socket created\n";
        if(sockid<0)
        {
            perror("Error in socket creation\n");
            exit(1);
        }

        cout<<"ip is "<<hosts[i].ip<<" "<<hosts[i].port<<" \n";
        serveraddr.sin_family=AF_INET;
        serveraddr.sin_port=htons(hosts[i].port);
        serveraddr.sin_addr.s_addr=inet_addr(hosts[i].ip.c_str());

        connect(sockid,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

        //Sends the control information to client server to get the no of chunks
        control=0;
        send(sockid,(const void*)&control,sizeof(control),0);
        strcpy(buffer,filename.c_str());
        send(sockid, buffer, sizeof(buffer), 0);
        recv(sockid,buffer,BUFFSIZE,0);

        cout<<buffer<<"\n";
        int chunkcount;
        string temp;

        stringstream ss(buffer);
        ss>>temp;
        chunkcount=stoi(temp);
        cout<<"Chunk present is "<<chunkcount<<"\n";
        chunkdata.push_back(chunkcount);
        close(sockid);
    }

    for(int i=0;i<hosts.size();i++)
    {
        cout<<hosts[i].port<<" is host "<<chunkdata[i]<<" \n";
        
    }

    filesize=hosts[0].filesize;

    //First Write logic of how to get which part from which client
    cout<<"File size is "<<filesize<<"\n";

    int totlch=chunkdata[0]-1;
    int noofclients=hosts.size();

    int part=totlch/noofclients;
    

    //creating file with temp values
    char buffer1[filesize]={'\0'};
    fp=fopen(filename.c_str(),"w+");
    fwrite(buffer1,sizeof(char),filesize,fp);
    fflush(fp);
    //fclose(fp);
   
    cout<<"Total chunks is "<<totlch<<" no of cients "<<noofclients<<" part "<<part<<" \n";
    pthread_t ids[BUFFSIZE];

    int count=0;
    
    vector<filetransfer_data> farr;
    for(int i=0;i<=totlch;i++)
    {
        struct filetransfer_data *temp=new struct filetransfer_data;

        int client=i%noofclients;

        temp->offset=i;
        temp->noofclients=noofclients;
        temp->filename=filename;
        temp->port=hosts[client].port;
        temp->ip=hosts[client].ip;
        temp->client=client;

        cout<<"\n"<<" offset is "<<temp->offset<<" port is "<<temp->port<<"\n";

        
        pthread_create(&ids[count],NULL,requestforfilesinchunks,(void*)temp);
        pthread_detach(ids[count++]); 
    }
}

void transferfiles(int cid)
{
    int command,n;
    
    char buffer[BUFFSIZE];

    recv(cid,( void*)&command,sizeof(command),0);

    if(command==0)
    {
        //Send the chunk count to the requesting client
        cout<<"Sending chunk size\n";

        //received the filename
        recv(cid,buffer,sizeof(buffer),0);

        cout<<"file name is "<<buffer<<"\n";
        string filename(buffer);
        cout<<buffer<<"\n";

        for(auto x:arr)
        {
            if(x.filename.compare(filename)==0)
            {
                string sdata=to_string(x.chunks.size());
                cout<<sdata<<"\n";
                strcpy(buffer,sdata.c_str());
                send(cid,buffer,sizeof(buffer),0);
                break;
            }
        }

    }

    else if(command==1)
    {
        int offset;
        string temp;

        //For the filename start sending the file
        cout<<"Starting transfer of file\n";

        memset(buffer,'\0',BUFFSIZE);

        recv(cid,buffer,BUFFSIZE,0);
        //cout<<buffer<<"\n";

        FILE *fp;
        fp=fopen(buffer,"r");

        //get the offset
        memset(buffer,'\0',BUFFSIZE);
        recv(cid,buffer,BUFFSIZE,0);
        stringstream ss(buffer);
        ss>>offset;

        cout<<"Offset received is "<<offset<<"\n";

        rewind(fp);
        fseek(fp,offset*BUFFSIZE,SEEK_SET);

        //Now you have to recieve the offset

        // n=fread(buffer,sizeof(char),BUFFSIZE,fp);
        // cout<<"Value read from file is "<<buffer<<endl;

        //send(cid,buffer,n,0);
            
        int packetsize=BUFFSIZE;
        char packet[PACKETSIZE];

        memset(packet,'\0',PACKETSIZE);
        cout<<"Packet size is "<<packetsize<<"\n";

        while( packetsize>0&&(n=fread(packet,sizeof(char),PACKETSIZE,fp))>0)
        {
            send(cid,packet,n,0);
            cout<<"Value read from file is "<<packet<<endl;
            packetsize=packetsize-n;
            memset(packet,'\0',PACKETSIZE);
        }
        cout<<"Value sent\n";
        fclose(fp);
    }
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

    pthread_t ids[BUFFSIZE];
    int count=0;

    while(1)
    {
        cid=accept(sockid,(struct sockaddr*)&addr,(socklen_t*)&len);

        //First recevive file name
        transferfiles(cid);
        
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

    char buffer[BUFFSIZE];

    string user_id,password;
    struct host_data data;

    tr=fopen("client_data.txt","r");
    if(tr!=NULL)
    {
        while(fscanf(tr,"%[^\n]\n",buffer)!=EOF)
        {
            cout<<buffer<<"\n";
            stringstream ss(buffer);

            struct client_data temp;
            string t;

            ss>>temp.filename;
            while(ss>>t)
                temp.chunks.push_back(t);

            arr.push_back(temp);
        }
        fclose(tr);
    }

    tr=fopen("client_data.txt","a");

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