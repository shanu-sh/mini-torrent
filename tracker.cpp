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
#include<signal.h>
#include<limits>
#include<unordered_map>
#include<stdio.h>

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
unordered_map<string,string> mapdata;

FILE *tr;

void mysignal_handler(int s)
{
    //cout<<"Signal caught";
    fclose(tr);
    exit(1);
}

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
        bool flag=false;

        string filename;
        string group_id;

        recv(cval,( void*)buffer,sizeof(buffer),0);

        stringstream ss(buffer);

        ss>>filename;
        ss>>group_id;
        cout<<"sending port\n";

        for(auto x:arr)
        {
            if(x.filename.compare(filename)==0 && x.group_id.compare(group_id)==0)
            {
                string temp=x.ip+" "+x.port;
                memset(buffer,'\0',BUFFSIZE);

                strcpy(buffer,temp.c_str());
                send(cval,(const void*)buffer,sizeof(buffer),0);

                flag=true;
                break;
            }
            
        }

        if(flag==false)
        {
            memset(buffer,'\0',BUFFSIZE);
            strcpy(buffer,"not_present ");
            send(cval,(const void*)buffer,sizeof(buffer),0);
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
        ss>>temp.group_id;

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

        char data[10000];
        string filedata=temp.filename+" "+temp.ip+" "+temp.port+" "+temp.group_id+" "+temp.hash+"\n";
        strcpy(data,filedata.c_str());

        fwrite(data,sizeof(char),strlen(data),tr);
        fflush(tr);

        cout<<arr[arr.size()-1].filename<<" "<<arr[arr.size()-1].group_id<<"\n";
        cout<<"Upload done\n";
    }

    else if(command==2)
    {
        //Creates a user
        char buffer[BUFFSIZE];
        recv(cval,( void*)buffer,sizeof(buffer),0);
        stringstream ss(buffer);

        string user_id;
        string password;

        ss>>user_id;
        ss>>password;

        if(mapdata.find(user_id)==mapdata.end())
        {
            mapdata[user_id]=password;
            strcpy(buffer,"user_created");
            send(cval,(void*)buffer,sizeof(buffer),0);
        }
        else
        {
            strcpy(buffer,"user_already_created");
            send(cval,(void*)buffer,sizeof(buffer),0);
        }
        
    }

    else if(command==3)
    {
        //Authenticates user
        char buffer[BUFFSIZE];
        recv(cval,( void*)buffer,sizeof(buffer),0);
        stringstream ss(buffer);

        string user_id;
        string password;

        ss>>user_id;
        ss>>password;

        if(mapdata.find(user_id)==mapdata.end())
        {
            strcpy(buffer,"User_not_present");
            send(cval,(void*)buffer,sizeof(buffer),0);
        }
        else
        {
            if(mapdata[user_id].compare(password)==0)
                strcpy(buffer,"logged_in");
            else
                strcpy(buffer,"User_not_present");
            
            send(cval,(void*)buffer,sizeof(buffer),0);
        }
    }
    close(cval);
}


void *acceptclient(void *arg)
{
    int sockid=socket(AF_INET,SOCK_STREAM,0);
    char buffer[BUFFSIZE];
    struct sockaddr_in addr;
    int len,cid;
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
}
int main()
{
    
    int len;
    tr=fopen("tracker_data.txt","r");

    if(tr==NULL)
    {
        cout<<"FIle not present\n";
    }
    char buffer1[200];
    char buffer2[200];
    char buffer3[200];
    char buffer4[200];
    char buffer5[200];
    cout<<"reading from file\n";
   
    string line;
    //getline(tr,line);

    while(fscanf(tr,"%s %s %s %s %s",buffer1,buffer2,buffer3,buffer4,buffer5)!=EOF)
    {
    cout<<"In while\n";
    cout<<buffer1<<"\n";
    cout<<buffer2<<"\n";

    line=string(buffer1)+string(buffer2);
    cout<<line<<"\n";

    struct trackerdata temp;
    temp.filename=string(buffer1);
    temp.ip=string(buffer2);
    temp.port=string(buffer3);
    temp.group_id=string(buffer4);
    temp.hash=string(buffer5);
    arr.push_back(temp);
    }
    fclose(tr);
    tr=fopen("tracker_data.txt","a");

    pthread_t id;
    pthread_create(&id,NULL,acceptclient,(void*)&len);
    pthread_detach(id);

    string command;
    while(1)
    {
        cout<<flush;
        
        cout<<"Enter command\n";
        cin>>command;
        if(command.compare("quit")==0)
        {
            fclose(tr);
            exit(1);
        }

    }
}