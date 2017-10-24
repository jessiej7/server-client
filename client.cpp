//
//  client.cpp
//  pa22
//
//  Created by Jia Zhao on 3/11/17.
//  Copyright © 2017 Jia Zhao. All rights reserved.
//
#include <sys/time.h>
#include <time.h>
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
    struct addrinfo *client_s, *client_r, *rp;
    struct sockaddr_storage their_addr;
    socklen_t their_addr_len;
    char pack_buf[BUF_SIZE];
    int sfd_s;
    int sfd_r;

    //additional variable
    int expect_seq=0;
    int receive_seq=-1;
    int nread;
    int pack_len;
    int SB=0, NS=0;
    int packet_size;

    //set up socket struct
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family=AF_UNSPEC;			// allow IPv4 or IPv6
    hints.ai_socktype=SOCK_DGRAM;		// use datagram socket
    hints.ai_protocol=0;				// any protocol
  
    /* getaddreinfo() returns a list of address structure */
    //get ip address and port number
    //connect socket from emulator to client
    getaddrinfo(argv[1], argv[2], &hints, &client_s);
    for (rp=client_s; rp != NULL; rp=rp->ai_next) {
        sfd_s=socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sfd_s == -1) continue;	//ignore failure request
        if(connect(sfd_s, rp->ai_addr, rp->ai_addrlen) != -1) break;	//success

        close(sfd_s);
    }
    
    freeaddrinfo(client_s);		//address nolonger needed
    
    //set up socket struct
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family=AF_UNSPEC;			// allow IPv4 or IPv6
    hints.ai_socktype=SOCK_DGRAM;		// use datagram socket
    hints.ai_protocol=0;				// any protocol
    
    //get port number
    //bind socket to emulator
    getaddrinfo(NULL, argv[3], &hints, &client_r);
    for (rp=client_r; rp != NULL; rp=rp->ai_next) {
        sfd_r=socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sfd_r == -1) continue;
        if(bind(sfd_r, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sfd_r);
    }

    freeaddrinfo(client_r);		//address nolonger needed
    
    
    //set up output files
    FILE *file_input, *file_seqNum, *file_ackNum;
    file_input=fopen(argv[4],"r+");
    file_seqNum=fopen("seqnum.log", "w+");
    file_ackNum=fopen("ack.log", "w+");
    

    //input chunk
    char stream[BUF_SIZE];
    char end_str[BUF_SIZE];
   
    //set end flag to ture when send out an eot packet
    bool end_flag=false;

    char pack_data_array[8][BUF_SIZE];
    
    //str_num tracks end of file
    //size tracks actual transmitted characters 
    int str_num=0;
    int size=0;
    
    //has to set a globle packet
    packet receive_packet(0,0,0,NULL);

    //set up time interpreter
    time_t start_t, end_t;
    double diff_t;
    bool time_flag=true;
    double cost_time;

    //another option
    //setsockopt(sfd_r, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv))
    
    while (1) {
        //window is not full
        while (NS < SB+7 && !end_flag) {
            //chunck input file into 30 char per data segment
            str_num=fread(stream, 1, 30, file_input);
            if(str_num != 0){
                size=str_num+1;
                stream[size-1]='\0';
                
                //prepare ack packet with data
                packet send_packet(1,NS%8,size-1,stream);
                pack_data_array[NS%8][0]='\0';
                send_packet.serialize(pack_data_array[NS%8]);
                
                //write packet to socket
                packet_size=strlen(pack_data_array[NS%8]);
                
                if(write(sfd_s, pack_data_array[NS%8], packet_size) != packet_size) exit(EXIT_FAILURE);
                fprintf(file_seqNum, "%d\n", NS%8);
                NS++;
                time_flag=true;
            }
            //eof, send ack with packet.type == 3
            else{
                packet send_packet(3,NS%8,0,NULL);
                send_packet.serialize(end_str);
                //packet_size=strlen(pack_data_array[NS%8]);
                //if(write(sfd_s, pack_data_array[NS%8], packet_size) != packet_size) exit(EXIT_FAILURE);
                end_flag=true;
            }
        }
        
        //get difftime for packet receiving, prepare for timeout
        if(time_flag){
            time(&start_t);
            time_flag=false;
        }
        int nread = read(sfd_r, pack_buf, BUF_SIZE);
        time(&end_t);
        cost_time = difftime(end_t, start_t);

        //set timer to 2s
        if (cost_time > 2 && SB != NS) {
            printf("TIMEOUT\n");
            time_flag=true;
            //when timeout, re-send all received acked packets
            for (int i=SB; i<NS; i++) {
                packet_size=strlen(pack_data_array[i%8]);
                if(write(sfd_s, pack_data_array[i%8], packet_size) != packet_size) exit(EXIT_FAILURE);
                fprintf(file_seqNum, "%d\n", i%8);
            }
            continue;
        }
        
        if(nread == -1) continue;
        
        //nread=read(sfd_r, pack_buf, BUF_SIZE);
        //packet receive_packet(0,0,0,NULL);


        //testing

        receive_packet.deserialize(pack_buf);
        fprintf(file_ackNum, "%d\n", receive_packet.getSeqNum());
        if ( SB%8 != NS)
        {           
            SB++;
            time_flag=true;
        }

        if(end_flag && SB == NS) break;
        
    }
    
    //prepare and send eot packet 
    packet_size=strlen(end_str);
    //write packet to socket
    if(write(sfd_s, end_str, packet_size) != packet_size) exit(EXIT_FAILURE);
    fprintf(file_seqNum, "%d\n", NS%8);

    //receive eot ack from server
    nread=read(sfd_r, pack_buf, BUF_SIZE);
    //read packet information
    receive_packet.deserialize(pack_buf);
    //print packet info on screen
    receive_packet.printContents();
    //write sequence number to file_ackNum
    fprintf(file_ackNum, "%d\n", receive_packet.getSeqNum());
    
    
    close(sfd_s);
    close(sfd_r);
    fclose(file_input);
    fclose(file_seqNum);
    fclose(file_ackNum);
    
    exit(EXIT_SUCCESS);
}





























