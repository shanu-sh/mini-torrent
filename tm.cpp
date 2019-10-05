#include<iostream>
#include<openssl/sha.h>


using namespace std;

int main()
{

    const unisgned char data[]="Original string";

    unsigned char hash[1000];

    SHA1(data,sizeof(data)-1,hash);

    cout<<hash<<"\n";
}