//
//  COEN233Assignment1
//  server.c
//  Created by Yang Guo on 5/24/18.
//  Student ID：1425884

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef unsigned char uint8;
typedef unsigned short uint16;

//init constants
const uint16 PACKET_ID_START = 0XFFFF;
const uint16 PACKET_ID_END = 0XFFFF;

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
    char *payload;
    enum REJECT_CODE rejectCode;
};

//initialize send message from a packet
int initMsg (struct PACKET packet, uint8* sendMessage) {
    memcpy(sendMessage, &PACKET_ID_START, 2);
    memcpy(sendMessage + 2, &packet.clientId, 1);
    memcpy(sendMessage + 3, &packet.packetType, 2);
    int end = 0;  //size of the message
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

//decode the received message and set it be a packet
enum REJECT_CODE readMsg (uint8* receiveMessage, int messageLen, struct PACKET* packet, int* sequence) {
    memcpy(&packet->clientId, receiveMessage + 2, 1);
    memcpy(&packet->packetType, receiveMessage + 3, 2);
    switch (packet->packetType) {
        case DATA: {
            int expectedSeq = sequence[packet->clientId] + 1;
            memcpy(&packet->segmentNum, receiveMessage + 5, 1);
            memcpy(&packet->length, receiveMessage + 6, 1);
            
            // handle end of packet missing error
            if (memcmp(&PACKET_ID_END, receiveMessage + messageLen - 2, 2) != 0) {
                return PACKET_ID_END_MISSING;
            }
            
            // handle pack length error
            if (7 + packet->length + 2 != messageLen) {
                return LENGTH_MISMATCH;
            }
            
            // handle sequence error
            if (packet->segmentNum < expectedSeq) {
                return DUPLICATE_PACKET;
            } else if (packet->segmentNum > expectedSeq) {
                return OUT_OF_SEQUENCE;
            } else {
                // update current sequence number
                sequence[packet->clientId] = packet->segmentNum;
                if(sequence[packet->clientId] == 5)
                    sequence[packet->clientId] = 0;
            }
            
            char data[packet->length + 1];
            data[packet->length] = 0;    // end of string
            memcpy(data, receiveMessage + 7, packet->length);
            packet->payload = data;
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
    for (int i = 0; i < size; i++) {
        if (i > 0)
            printf(":");
        printf("%02X", *(string + i));
    }
    printf("\n");
}



int main() {
    
    struct sockaddr_in server;
    socklen_t addrSize;

    //create UDP socket
    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

    //configure settings
    bzero(&server, sizeof(server));
    server.sin_port = htons(7115);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;

    if(udpSocket == -1) {
        printf("ERROR: socket not created\n");
        exit(1);
    }
    if(bind(udpSocket, (struct sockaddr *) &server, sizeof(server)) == -1) {
        printf("ERROR: socket not binded\n");
        exit(1);
    }

    
    printf("======================================\n");
    printf("    Server is running....\n");
    printf("======================================\n");

    int sequence[10] = {0};
    int testTimes = 0;

    while(11) {
        
        if(testTimes == 10)
            break;
        else
            testTimes++;

        uint8 buffer[1024];
        addrSize = sizeof(server);

        fflush(stdout);
        // get client
        int recvSize = (int) recvfrom(udpSocket, buffer, 1024, 0, (struct sockaddr *) &server, &addrSize);
        if (recvSize == -1) {
            perror("RECVFROM ERROR");
            break;
        }

        //set received data packet
        struct PACKET receivedDataPack;
        enum REJECT_CODE rejectCode = readMsg(buffer, recvSize, &receivedDataPack, sequence);

        // make response packet as an ACK packet
        struct PACKET packet;
        packet.clientId = receivedDataPack.clientId;
        packet.packetType = ACK;
        packet.segmentNum = receivedDataPack.segmentNum;

        // check if there is reject error
        switch (rejectCode) {
            case OUT_OF_SEQUENCE:
                printf("\nWrong packet:\n");
                printf("ERROR: OUT OF SEQUENCE\n");
                break;
            case LENGTH_MISMATCH:
                printf("\nWrong packet:\n");
                printf("ERROR: LENGTH MISMATCH\n");
                break;
            case PACKET_ID_END_MISSING:
                printf("\nWrong packet:\n");
                printf("ERROR: END OF PACKET ID MISSING\n");
                break;
            case DUPLICATE_PACKET:
                printf("\nWrong packet:\n");
                printf("ERROR: DUPLICATE PACKET\n");
                break;
            case NO_ERROR:
                printf("\nReceived correct packet NO. %d \n", receivedDataPack.segmentNum);
                //printMsg(buffer, recvSize);
                break;
        }

        // If there is an error, set it as a REJECT packet
        if (rejectCode != NO_ERROR) {
            packet.packetType = REJECT;
            packet.rejectCode = rejectCode;
        }

        // make response packet to buffer and get the packet length
        int packLen = initMsg(packet, buffer);

        // send packet to client
        if (sendto(udpSocket, buffer, packLen, 0, (struct sockaddr *) &server, addrSize) == -1) {
            perror("SENDTO ERROR");
            break;
        }
        //printf("Sending response message...\n");
        //printMsg(buffer, packLen);

    }
    
    close(udpSocket);
    return 0;
}


