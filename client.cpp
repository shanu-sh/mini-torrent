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

#define BUFFSIZE 512
#define PACKETSIZE 512
#define CHUNKSIZE 524288

using namespace std;

struct client_data
{
    string filename;
    vector<int> chunks;
};

struct host_data
{
    string ip;
    int port;
    long filesize;
};

struct filetransfer_data
{
    long offset;
    int noofclients;
    string filename;
    string ip;
    int port;
    long filesize;
    int client;
    int chunkstoberead;
};

vector<client_data> arr;
pthread_mutex_t lock;

FILE *tr;

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

    char chunk[CHUNKSIZE];

    while( size>0&&(n=fread(chunk,sizeof(char),CHUNKSIZE,fp))>0)
    {
        //send(cid,buffer,n,0);

        //Compute hash value
        
        hash=computehash(chunk);

        cout<<"\nHash computed is ";
        cout<<hash<<"\n";
        memset(buffer,'\0',BUFFSIZE);

        strcpy(buffer,hash.c_str());
        //send(sockid,buffer,SHA_DIGEST_LENGTH*2,0);

        memset(buffer,'\0',BUFFSIZE);

        cdata.chunks.push_back(1);

        chunkcount++;
        size=size-n;
        memset(chunk,'\0',CHUNKSIZE);
    }

    arr.push_back(cdata);

    string fdata;

    fdata=cdata.filename+" ";
    strcpy(buffer,fdata.c_str());
    fwrite(buffer,sizeof(char),strlen(buffer),tr);
    fflush(tr);

    for(long i=0;i<cdata.chunks.size();i++)
    {
        //cout<<"i is "<<i<<"\n";

        if(i!=cdata.chunks.size()-1)
            fdata=to_string(cdata.chunks[i])+" ";
        else
            fdata=to_string(cdata.chunks[i])+"\n";

        strcpy(buffer,fdata.c_str());

        fwrite(buffer,sizeof(char),strlen(buffer),tr);
        fflush(tr);
    }

    cout<<"No of chunks created "<<chunkcount<<"\n";
    cout<<"Last chunk size is "<<n<<"\n";

    memset(buffer,'\0',sizeof(buffer));
    string t=to_string(n)+" "+to_string(chunkcount);
    strcpy(buffer,t.c_str());

    send(sockid,buffer,BUFFSIZE,0);
    
    fclose(fp);
    close(sockid);
}

void *requestforfilesinchunks(void * arg)
{

    pthread_mutex_lock(&lock);
    FILE * fpt;
    struct filetransfer_data *ptr=(struct filetransfer_data *)arg;

    struct filetransfer_data temp=*ptr;
    
    long i=temp.offset;
    int control,n;
    char buffer[BUFFSIZE];
    int noofclients=temp.noofclients;
    string filename=temp.filename;

    int port=temp.port;
    string ip=temp.ip;

    int sockid=socket(AF_INET,SOCK_STREAM,0);

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

    send(sockid, buffer, sizeof(buffer), 0);

    //Now send the file offset which you want to read
   
    strcpy(buffer,to_string(i).c_str());

    send(sockid, buffer,BUFFSIZE, 0);

    int datatoberead=temp.chunkstoberead;

    strcpy(buffer,to_string(datatoberead).c_str());
    send(sockid, buffer,BUFFSIZE, 0);

    //n=recv(sockid,buffer,BUFFSIZE,0);
    int packetsize=0;
    char packet[PACKETSIZE];
    fpt=fopen(filename.c_str(),"r+");
    rewind(fpt);
    fseek(fpt,i*CHUNKSIZE,SEEK_SET);

    
    while(packetsize<datatoberead && (n=recv(sockid,packet,PACKETSIZE,0))>0)
    {
        
        //cout<<"Value read in packet is "<<buffer<<" and csize is "<<packetsize<<"\n";
        packetsize=packetsize+n;
        
        fwrite(packet,sizeof(char),n,fpt);
        

    }
    fflush(fpt);

    cout<<"File written in client for offset "<<i<<"\n";
    close(sockid);
    fclose(fpt);
    pthread_mutex_unlock(&lock);
   
}

void getfilenames(string tracker_ip,int tracker_port)
{

    int sockid=socket(AF_INET,SOCK_STREAM,0);
    int control=4;

    int noofiles;
    char buffer[BUFFSIZE];

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

    recv(sockid,(void*)&noofiles,sizeof(noofiles),0);

    cout<<"The files present in tracker is \n";

    for(int i=0;i<noofiles;i++)
    {
        recv(sockid,(void*)buffer,sizeof(buffer),0);
    }
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
        cout<<"Host received is "<<hdata.ip<<" "<<hdata.port<<" "<<hdata.filesize<<" \n";
        memset(buffer,'\0',BUFFSIZE);
    }

    cout<<"No of hosts present is "<<hosts.size()<<" \n";
    close(sockid);

    //Now connect to client server to get the total count of chunk present

    vector<int> chunkdata;

    vector<vector<int> > chunksinfo;


    for(int i=0;i<hosts.size();i++)
    {
        vector<int> bitsinfo;

        sockid=socket(AF_INET,SOCK_STREAM,0);
        if(sockid<0)
        {
            perror("Error in socket creation\n");
            exit(1);
        }

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

        int chunkcount;
        string temp;

        stringstream ss(buffer);
        ss>>temp;
        chunkcount=stoi(temp);
        chunkdata.push_back(chunkcount);

        int bit;
        for(long i=0;i<chunkcount;i++)
        {
            recv(sockid,buffer,BUFFSIZE,0);
            stringstream ss1(buffer);
            ss1>>bit;

            if(bit)
                bitsinfo.push_back(1);
            else
                bitsinfo.push_back(0);
        }

        chunksinfo.push_back(bitsinfo);
        close(sockid);
    }

    // for(int i=0;i<hosts.size();i++)
    // {
    //     cout<<hosts[i].port<<" is host "<<chunkdata[i]<<" \n";

    //     for(auto x:chunksinfo[i])
    //         cout<<x;
    //     cout<<"\n";
    // }

    filesize=hosts[0].filesize;

    //First Write logic of how to get which part from which client

    int totlch=chunkdata[0]-1;
    int noofclients=hosts.size();

    FILE * fpt;
    
    fpt=fopen(filename.c_str(),"w");
    cout<<"File opened\n";

    for(long i=0;i<filesize;i++)
        fputc('\0',fpt);
    
    fflush(fpt);
    fclose(fpt);
   
    pthread_t ids[CHUNKSIZE];
    long count=0;
    
    vector<filetransfer_data> farr;

    vector<int> datafetched(totlch+1,0);

    for(long i=0;i<=totlch;i++)
    {
        struct filetransfer_data *temp=new struct filetransfer_data;

        long client=i%noofclients;

        bool flag=true;
        if(chunksinfo[client][i]!=1)
        {
            flag=false;

            cout<<"Chunk "<<i<<" Not present in destined client\n";
            for(long j=0;j<noofclients;j++)
            {
                if(chunksinfo[j][i]==1)
                {
                    cout<<"chunk "<<i<<" not present in "<<client<<" But present in "<<j<<"\n";
                    client=j;
                    flag=true;
                    break;
                }
            }
        }
        if(flag==false)
        {
            cout<<"Chunk "<<i<<" not present in any client\n";
            continue;
        }

        datafetched[i]=1;
        
        temp->offset=i;
        temp->noofclients=noofclients;
        temp->filename=filename;
        temp->port=hosts[client].port;
        temp->ip=hosts[client].ip;
        temp->client=client;        
        
        if(filesize>CHUNKSIZE)
            temp->chunkstoberead=CHUNKSIZE;
        else
            temp->chunkstoberead=filesize;

        filesize=filesize-CHUNKSIZE;
        pthread_create(&ids[count],NULL,requestforfilesinchunks,(void*)temp);
        pthread_detach(ids[count++]); 
    }
}

void *transferfiles(void *arg)
{
    int *a=(int*)arg;
    int cid=*a;

    int command,n;
    
    char buffer[BUFFSIZE];

    recv(cid,( void*)&command,sizeof(command),0);

    if(command==0)
    {
        //Send the chunk count to the requesting client
        //received the filename

        recv(cid,buffer,sizeof(buffer),0);

        cout<<"file name is "<<buffer<<"\n";
        string filename(buffer);
        for(auto x:arr)
        {
            if(x.filename.compare(filename)==0)
            {
                string sdata=to_string(x.chunks.size());
                strcpy(buffer,sdata.c_str());
                send(cid,buffer,sizeof(buffer),0);

                for(auto y:x.chunks)
                {
                    strcpy(buffer,to_string(y).c_str());
                    send(cid,buffer,sizeof(buffer),0);
                }
                break;
            }
        }

    }

    else if(command==1)
    {
        long offset;
        string temp;

        //For the filename start sending the file
        cout<<"Starting transfer of file\n";

        memset(buffer,'\0',BUFFSIZE);

        recv(cid,buffer,BUFFSIZE,0);

        FILE *fp;
        fp=fopen(buffer,"r");

        //get the offset
        memset(buffer,'\0',BUFFSIZE);
        recv(cid,buffer,BUFFSIZE,0);
        stringstream ss(buffer);
        ss>>offset;

        int datatoberead;
        memset(buffer,'\0',BUFFSIZE);
        recv(cid,buffer,BUFFSIZE,0);
        stringstream ss1(buffer);
        ss1>>datatoberead;

        rewind(fp);
        fseek(fp,offset*CHUNKSIZE,SEEK_SET);

        int packetsize=0;
        char packet[PACKETSIZE];

        memset(packet,'\0',PACKETSIZE);
        while( packetsize<datatoberead&&(n=fread(packet,sizeof(char),PACKETSIZE,fp))>0)
        {
            send(cid,packet,n,0);
            packetsize=packetsize+n;
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

    pthread_t ids[CHUNKSIZE];
    int count=0;

    while(1)
    {
        cid=accept(sockid,(struct sockaddr*)&addr,(socklen_t*)&len);
        pthread_create(&ids[count],NULL,transferfiles,(void*)&cid);
        pthread_detach(ids[count++]); 

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

    char buffer[CHUNKSIZE];

    string user_id,password;
    struct host_data data;

    tr=fopen("client_data.txt","r");
    if(tr!=NULL)
    {
        while(fscanf(tr,"%[^\n]\n",buffer)!=EOF)
        {
            //cout<<buffer<<"\n";
            stringstream ss(buffer);

            struct client_data temp;
            int t;

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

        if(cmd.compare("list_files")==0)
        {
            if(login_status==false)
                cout<<"Please login first\n";
            else
            {
                getfilenames(tracker_ip,tracker_port);
            }
            
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