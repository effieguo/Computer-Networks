//
//  coen233 Assignment1
//  client.c
//
//  Created by Yang Guo on 5/24/18.
//  Student ID: 1425884

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef unsigned char uint8;
typedef unsigned short uint16;

//initial constants
const uint16 PACKET_ID_START = 0XFFFF;
const uint16 PACKET_ID_END = 0XFFFF;
//test data info
uint8 TEST_DATA[] = "Test data!!!";

struct sockaddr_in server;
socklen_t addrSize;
int clientSocket;

//Initial array for storing the message receive from server
uint8 buffer[1024];

enum PACKET_TYPE {
    DATA = 0XFFF1,
    ACK = 0XFFF2,
    REJECT = 0XFFF3
};

enum REJECT_CODE {
    OUT_OF_SEQUENCE = 0XFFF4,
    LENGTH_MISMATCH = 0XFFF5,
    PACKET_ID_END_MISSING = 0XFFF6,
    DUPLICATE_PACKET = 0XFFF7,
    NO_ERROR = 0XFFF8
};

struct PACKET {
    uint8 clientId;
    enum PACKET_TYPE packetType;
    uint8 segmentNum;
    uint8 length;
    uint8 *payload;
    enum REJECT_CODE rejectCode;
};

//build send message by getting a packet
int initMsg (struct PACKET packet, uint8* sendMessage) {
    memcpy(sendMessage, &PACKET_ID_START, 2);
    memcpy(sendMessage + 2, &packet.clientId, 1);
    memcpy(sendMessage + 3, &packet.packetType, 2);
    int end = 0;
    switch (packet.packetType) {
        case DATA:
            memcpy(sendMessage + 5, &packet.segmentNum, 1);
            memcpy(sendMessage + 6, &packet.length, 1);
            memcpy(sendMessage + 7, packet.payload, packet.length);
            end = 7 + packet.length;
            break;
        case ACK:
            memcpy(sendMessage + 5, &packet.segmentNum, 1);
            end = 5 + 1;
            break;
        case REJECT:
            memcpy(sendMessage + 5, &packet.rejectCode, 2);
            memcpy(sendMessage + 7, &packet.segmentNum, 1);
            end = 7 + 1;
    }
    memcpy(sendMessage + end, &PACKET_ID_END, 2);
    return end + 2;
}

//
enum REJECT_CODE readMsg (uint8* receiveMessage, long messageLen, struct PACKET* packet, int* sequence) {
    memcpy(&packet->clientId, receiveMessage + 2, 1);
    memcpy(&packet->packetType, receiveMessage + 3, 2);
    switch (packet->packetType) {
        case DATA: {
            int expectedSeq = sequence[packet->clientId] + 1;
            memcpy(&packet->segmentNum, receiveMessage + 5, 1);
            memcpy(&packet->length, receiveMessage + 6, 1);
            
            // handle missing end of packet error
            if (memcmp(&PACKET_ID_END, receiveMessage + messageLen - 2, 2) != 0) {
                return PACKET_ID_END_MISSING;
            }
            
            // handle pack length error.
            if (7 + packet->length + 2 != messageLen) {
                return LENGTH_MISMATCH;
            }
            
            // handle pack sequence error.
            if (packet->segmentNum < expectedSeq) {
                return DUPLICATE_PACKET;
            } else if (packet->segmentNum > expectedSeq) {
                return OUT_OF_SEQUENCE;
            } else {
                // update current sequence number.
                sequence[packet->clientId] = packet->segmentNum;
            }
            
            memcpy(&packet->payload, receiveMessage + 7, packet->length);
            break;
        }
        case ACK:
            memcpy(&packet->segmentNum, receiveMessage + 5, 1);
            break;
        case REJECT:
            memcpy(&packet->rejectCode, receiveMessage + 5, 2);
            memcpy(&packet->segmentNum, receiveMessage + 7, 1);
    }
    return NO_ERROR;
}

//print message byte by byte
void printMsg (uint8 *string, int size) {
    //print receive data
    for (int i = 0; i < size; i++) {
        if (i > 0)
            printf(":");
        printf("%02X", *(string + i));
    }
    
    printf("\n");
}

void printResponse(struct PACKET receivedPack) {
    // handle four reject
    switch (receivedPack.packetType) {
        case ACK:
            printf("Received ACK from server for packet with segment %d\n", receivedPack.segmentNum);
            break;
        case REJECT:
            printf("Received REJECT from server for packet with segment %d\n", receivedPack.segmentNum);
            switch (receivedPack.rejectCode) {
                case OUT_OF_SEQUENCE:
                    fprintf(stderr, "REJECT CODE: OUT OF SEQUENCE\n");
                    break;
                case DUPLICATE_PACKET:
                    fprintf(stderr, "REJECT CODE: DUPLICATE PACKET\n");
                    break;
                case LENGTH_MISMATCH:
                    fprintf(stderr, "REJECT CODE: LENGTH MISMATCH\n");
                    break; // fatal error, exit.
                case PACKET_ID_END_MISSING:
                    fprintf(stderr, "REJECT CODE: END OF PACKET ID MISSING\n");
                    break; // fatal error, exit.
                case NO_ERROR:
                    break;
            }
        case DATA:
            break;
    }
}

//send message to server and receive message from server
void sendAndReceive(uint8* message, uint8 clientId, uint8 num, int type) {
    uint8 payloadLength = strlen((const char *)message);
    struct PACKET dataPack;
    dataPack.clientId = clientId;
    dataPack.packetType = DATA;
    dataPack.segmentNum = num;
    dataPack.payload = message;
    dataPack.length = payloadLength;

    int size = initMsg(dataPack, buffer);
    
    //packet with error: DON'T HAVE the END PACKET ID!
    if (type == 2) {
        size = size - 2;
    }
    
    //packet with error: LENGTH NOT MATCH!
    if(type == 1){
        buffer[6] = 0; //set the length be 0
    }
    
    //set receive size and sending message count
    long recvSize = -1;
    int count = 3;
    
    //send message to server
    sendto(clientSocket, buffer, size, 0, (struct sockaddr *) &server, sizeof(server));
    
    //show send message
    printf("\nSending packet with segment %d to server...\n", dataPack.segmentNum);
    printMsg(buffer, size);
    printf("\n");
    
    struct timeval timer;
    timer.tv_sec = 3;      // set timeout 3 sec
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer));
    
    while (count > 0) {
        //receive message from server
        recvSize = recvfrom(clientSocket, buffer, 1024, 0,(struct sockaddr *) &server, &addrSize);
        if(recvSize < 0)
            perror("RECVFROM ERROR");
        else
            break;
        //ever time count--
        count--;
    }
    
    //after 3 times, if recvSize still equals to -1, show server does not respond
    if (recvSize < 0) {
        printf("Server does not respond!! \n\n");
        exit(1);
    } else {
        // get packet information
        struct PACKET receivedPack;
        readMsg(buffer, recvSize, &receivedPack, NULL);
        
        printResponse(receivedPack);
    }
    printf("\n");
    
}



int main() {
    //Create UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket == -1) {
        perror("ERROR");
        exit(1);
    }
    
    //configure settings
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(7115);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    
    //send five correct packets
    printf("======================================\n");
    printf("    Send five correct packets...\n");
    printf("======================================\n");

    for (int i=0; i<5; i++) {
        sendAndReceive(TEST_DATA, 1, i+1, 0);
    }
    
    //send another five packets include one correct packet and four with error
    printf("======================================\n");
    printf("    Send another five packets...\n");
    printf("======================================\n");
    
    //No.1 correct packet!
    printf("No.1 correct packet!\n");
    sendAndReceive(TEST_DATA,1,1,0);
    
    //No.2 packet with error: NOT IN SEQUENCE!
    printf("No.2 packet with error: not in sequence!\n");
    sendAndReceive(TEST_DATA,1,3,0);
    
    //No.3 packet with error: LENGTH NOT MATCH!
    printf("No.3 packet with error: length not match!\n");
    sendAndReceive(TEST_DATA,1,2,1);
    
    //No.4 packet with error: DON'T HAVE the END PACKET ID!
    printf("No.4 packet with error: don't have the end of packet ID!\n");
    sendAndReceive(TEST_DATA,1,2,2);
    
    //No.5 packet with error: DUPLICATED PACKET!
    printf("No.5 packet with error: duplicated packet!\n");
    sendAndReceive(TEST_DATA,1,1,0);
    
    //No.5 packet with error: No response!
    printf("No.5 packet with error: No response!\n");
    sendAndReceive(TEST_DATA,1,2,0);
    
    close(clientSocket);
    return 0;
}


