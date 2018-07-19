//
//  coen233 Assignment2
//  client.c
//
//  Created by Yang Guo on 5/29/18.
//  Student ID: 1425884
//
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <zconf.h>


typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

//init constants
const uint16 PACKET_ID_START = 0XFFFF;
const uint16 PACKET_ID_END = 0XFFFF;
const uint16 ACC_PER = 0XFFF8;
const uint8 TECH_2G = '2';
const uint8 TECH_3G = '3';
const uint8 TECH_4G = '4';
const uint8 TECH_5G = '5';


struct Subscriber {
    uint32 subscriberNum;
    uint8 technology;
    uint8 paymentStatus;
};

struct DataPack {
    uint8 clientID;
    uint8 segmentNum;
    uint8 length;
    struct Subscriber subscriber;
};

struct sockaddr_in server;
socklen_t addressSize;
int clientSocket;

//Initial array for storing the message receive from server
uint8 receiveMessage[1024];

//print the sending or receiving message
void printMsg(unsigned char *string, int size) {
    
    if (size == 14 && string[3] != 0XF8) {
        printf("Received respond from server:\n");
        if (string[3] == 0XF9) {
            printf("Not paid!\n");
        }

        if (string[3] == 0XFA) {
            printf("Not exist!\n");
        }

        if (string[3] == 0XFB) {
            printf("Access OK!\n");
        }
    }

    //print receive acc_per
    for (int i = 0; i < size; i++) {
        if (i > 0)
            printf(":");
        printf("%02X", *(string + i));
    }

    printf("\n");
}

//Initial massage into sending form
void initMessage(uint8 *string, struct DataPack packet) {
    memcpy(string, &PACKET_ID_START, 2);
    memcpy(string + 2, &packet.clientID, 1);
    memcpy(string + 3, &ACC_PER, 2);
    memcpy(string + 5, &packet.segmentNum, 1);
    memcpy(string + 6, &packet.length, 1);
    memcpy(string + 7, &packet.subscriber.technology, 1);
    memcpy(string + 8, &packet.subscriber.subscriberNum, 4);
    memcpy(string + 12, &PACKET_ID_END, 2);

}


//send message to server and receive message from server
void sendAndReceive(uint8 clientId, uint8 segNum, uint32 subscriberNum, uint8 tech) {
    uint8 payloadLength = 5;
    struct DataPack packet = {
        .clientID = clientId,
        .segmentNum = segNum,
        .subscriber.technology = tech,
        .subscriber.subscriberNum = subscriberNum,
        .length = payloadLength
    };
    
    int size = 14;
    uint8 sendMsg[size];

    initMessage(sendMsg, packet);
    
    //Send message to server
    sendto(clientSocket, sendMsg, size, 0, (struct sockaddr *) &server, addressSize);
    
    //print send message
    printf("Sending packet to server...\n");
    printMsg(sendMsg, size);
    printf("\n");
    
    struct timeval timer;
    timer.tv_sec = 3;      // set timeout 3 sec
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    
    //set receive size and sending message count
    int recvSize = -1;
    int count = 3;

    while (count > 0) {
        //receive message from server
        recvSize = (int) recvfrom(clientSocket, receiveMessage, 1024, 0, (struct sockaddr *) &server, &addressSize);
        if(recvSize < 0)
            perror("RECVFROM ERROR");
        else
            break;
        //ever time count--
        count--;
    }

    //after 3 times, if recvSize still = -1, server does not respond
    if (recvSize < 0) {
        printf("Server does not respond!!!\n\n");
        exit(1);
    } else {
        printMsg(receiveMessage, recvSize);
    }
    printf("\n");

}

int main() {

    //Create UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    //Configure settings
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(6666);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");


    //Initialize size variable
    addressSize = sizeof server;
    

    //No.1 unpaid packet!
    printf("\n");
    printf("No.1 unpaid packet!\n");
    sendAndReceive(1, 1, 4086668821, TECH_3G);

    //No.2 not exist packet with no such subscriber number!
    printf("No.2 not exist packet(subscriber number is not found)!\n");
    sendAndReceive(1, 1, 4087777777, TECH_4G);

    //No.3 not exist packet with technology not match!
    printf("No.3 not exist packet(technology does not match)!\n");
    sendAndReceive(1, 1, 4085546805, TECH_5G);

    //No.4 correct packet!
    printf("No.4 correct request packet!\n");
    sendAndReceive(1, 1, 4086808821, TECH_2G);
    
    //No.5 no response!
    printf("No.5 no response!\n");
    sendAndReceive(1, 1, 4086808821, TECH_2G);

    //add more test case here!!!

    close(clientSocket);
    return 0;
}
