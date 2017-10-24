//
//  main.cpp
//  pa22
//
//  Created by Jia Zhao on 3/11/17.
//  Copyright © 2017 Jia Zhao. All rights reserved.
//
//This is the server implementation for GBN protocol
//datagram uses UDP socket for transmission

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "packet.h"
#include "packet.cpp"

#define BUF_SIZE 1024
#define MAX 10000

using namespace std;

int main(int argc, const char * argv[]) {
    //initial set up
    struct addrinfo hints;
    struct addrinfo *server_r, *server_s, *rp;
    struct sockaddr_storage their_addr;
    socklen_t their_addr_len;
    char pack_buf[BUF_SIZE];
    int sfd_r;
    int sfd_s;
    
    //adding variable
    int expect_seq=0;
    int receive_seq=-1;
    int nread;
    int packet_size;

    /* obtain address from matching host port */
    //connect socket from emulator to server 
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family=AF_UNSPEC;      // allow IPv4 or IPv6
    hints.ai_socktype=SOCK_DGRAM;   // use datagram socket
    hints.ai_protocol=0;            // any protocol
    
    /* getaddreinfo() returns a list of address structure */
    //get IP address and port number
    //connect socket
    getaddrinfo(argv[1], argv[3], &hints, &server_s);
    for (rp=server_s; rp != NULL; rp=rp->ai_next) {
        sfd_s=socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sfd_s == -1) continue;
        //if(bind(sfd_s, rp->ai_addr, rp->ai_addrlen) == 0) break;
        if(connect(sfd_s, rp->ai_addr, rp->ai_addrlen) != -1) break;    // success

        close(sfd_s);
    }
    
    freeaddrinfo(server_s);     // address no longer needed

    //bind server socket to emulator
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family=AF_UNSPEC;          // allow IPv4 or IPv6
    hints.ai_socktype=SOCK_DGRAM;       // use datagram socket
    hints.ai_protocol=0;                // any protocol
    
    //get port number(use localadress)
    //bind socket to emulator
    getaddrinfo(NULL, argv[2], &hints, &server_r);
    for (rp=server_r; rp != NULL; rp=rp->ai_next) {
        sfd_r=socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sfd_r == -1) continue;       // ignore failure request
        if(bind(sfd_r, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sfd_r);
    }
    
    freeaddrinfo(server_r);
    

    
    //set up files for server
    FILE *file_input, *file_server_output;
    file_input=fopen(argv[4],"w+");
    file_server_output=fopen("arrival.log", "w+");
    
    //put data into packet.data()
    char data[BUF_SIZE];
    while (1) {
        //get packet info from socket
        their_addr_len=sizeof(struct sockaddr_storage);
        //int nread=read(sfd_r, pack_buf, BUF_SIZE);
        if(nread=recvfrom(sfd_r, pack_buf, BUF_SIZE, 0, (struct sockaddr*)&their_addr, &their_addr_len) == -1) continue;

        //somehow it has to be set up to ensure server conmunication
        //get a numeric host name and service name for a socket address
        
        char host[NI_MAXHOST], service[NI_MAXSERV];
        getnameinfo((struct sockaddr *)&their_addr, their_addr_len, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
        
        packet receive_pack(0,0,0,data);
        //abstract packet info
        receive_pack.deserialize(pack_buf);
        //add terminate char
        //data[receive_pack.getLength()]='\0';
        //write seq to output file
        fprintf(file_server_output, "%d\n",receive_pack.getSeqNum());
        

        //check receive ack == expect seq? 
        if (receive_pack.getSeqNum() == expect_seq) {
            receive_seq=expect_seq;
            
            //get packet.type == 3, end of the file
            if (receive_pack.getType() == 3) {
                //send eot ack packet.type==2
                packet send_pack(2,expect_seq,0,NULL);
                //add terminate char
                pack_buf[0]='\0';
                //form ack packet
                send_pack.serialize(pack_buf);
                //write packet to socket
                packet_size=strlen(pack_buf);
                if(write(sfd_s, pack_buf, packet_size) != packet_size) exit(EXIT_FAILURE);
                expect_seq=(expect_seq+1)%8;
                break;
            }
            //get packet.type == 1, write info to file_input
            //increase sequence number by 1
            else{
                fprintf(file_input, "%s\n", receive_pack.getData() );
                expect_seq=(expect_seq+1)%8;
            }
            
        }

        //send ack packet back to socket
        packet send_pack(0,receive_seq,0,NULL);
        pack_buf[0]='\0';
        send_pack.serialize(pack_buf);
        packet_size=strlen(pack_buf);
        //write ack to socket
        if(write(sfd_s, pack_buf, packet_size) != packet_size) exit(EXIT_FAILURE);
        
        
    }
    close(sfd_r);
    close(sfd_s);
    fclose(file_server_output);
    fclose(file_input);
    exit(EXIT_SUCCESS);
}



























