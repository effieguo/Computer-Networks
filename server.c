//
//  coen233 Assignment2
//  server.c
//
//  Created by Yang Guo on 5/28/18.
//  Student ID: 1425884
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

//initialize constants
const uint16 PACKET_ID_START = 0XFFFF;
const uint16 PACKET_ID_END = 0XFFFF;

const uint16 ACC_PER = 0XFFF8;
const uint16 NOT_PAID = 0XFFF9;
const uint16 NOT_EXIST = 0XFFFA;
const uint16 ACCESS_OK = 0XFFFB;

//set file location
const uint8 fileName[] = "/Users/effieguo/Downloads/Computer Networks/COEN233_Assignment2/server/Verfication_Database.txt";

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

//read file and put data into subscriberData
//return the number of subscriber
int readFile(struct Subscriber subscriberData[], const char *fileName) {
    printf("===============================================\n");
    printf("  Reading data from Verfication Database...\n");
    printf("===============================================\n");
    FILE *file;
    size_t readNum = 0;
    char *line = NULL;
    uint8 numbers[11];
    uint8 data[18];
    
    file = fopen(fileName, "r");
    
    if (file == NULL) {
        printf("Cannot open the file!\n");
    }
    // skip the title line
    getline(&line, &readNum, file);
    
    //index of the subscriber
    int count = 0;
    printf("Subscriber Number\tTechnology\tPaid\n");
    while ((getline(&line, &readNum, file)) != -1) {
        memcpy(data, line, 17);
        data[17] = '\0';
        int j = 0;
        for (int i = 0; i < 12; i++) {
            //avoid the '-' char
            if (data[i] != '-') {
                numbers[j] = data[i];
                j++;
            }
        }
        
        numbers[10] = '\0';  //set end of string
        subscriberData[count].subscriberNum = (uint32) atol((const char*) numbers);
        subscriberData[count].technology =  data[14];
        subscriberData[count].paymentStatus = data[16];
    printf("%u\t\t0%c\t\t%c\n",subscriberData[count].subscriberNum,subscriberData[count].technology,subscriberData[count].paymentStatus);
        count++;
    }
    fclose(file);
    
    return count;
}

//read Message and get DataPack
struct DataPack readMsg(uint8 *recvBuffer) {
    struct DataPack dataPack;
    struct Subscriber recvSubscriber;
    memcpy(&dataPack.clientID, recvBuffer + 2, 1);
    memcpy(&dataPack.segmentNum, recvBuffer + 5, 1);
    memcpy(&dataPack.length, recvBuffer + 6, 1);
    memcpy(&recvSubscriber.technology, recvBuffer + 7, 1);
    memcpy(&recvSubscriber.subscriberNum, recvBuffer + 8, 4);
    dataPack.subscriber = recvSubscriber;
    return dataPack;
}

//Initialize respond message
void initMessage(struct DataPack dataPack, uint8 *respondMsg, uint16 respondType){
    
    memcpy(respondMsg, &PACKET_ID_START, 2);
    memcpy(respondMsg + 2, &dataPack.clientID, 1);
    if(respondType == 1){
        memcpy(respondMsg + 3, &NOT_PAID, 2);
    }
    
    if(respondType == 2){
        memcpy(respondMsg + 3, &NOT_EXIST, 2);
    }
    
    if(respondType == 0){
        memcpy(respondMsg + 3, &ACCESS_OK, 2);
    }
    memcpy(respondMsg + 5, &dataPack.segmentNum, 1);
    memcpy(respondMsg + 6, &dataPack.length, 1);
    memcpy(respondMsg + 7, &dataPack.subscriber.technology, 1);
    memcpy(respondMsg + 8, &dataPack.subscriber.subscriberNum, 4);

    memcpy(respondMsg + 12, &PACKET_ID_END, 2);
    printf("\n");
}

int main() {
    
    struct Subscriber subscriberData[10];
    
    //read file and init subscriberData
    int count = readFile(subscriberData, (const char*) fileName);
    
    printf("========================================\n");
    printf("     Server is running...\n");
    printf("========================================\n");
    
    struct sockaddr_in server;
    socklen_t addrSize;
    
    //Create UDP socket
    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    
    //Configure settings in address struct
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(6666);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    //Bind socket with address struct
    bind(udpSocket, (struct sockaddr *) &server, sizeof(server));
    
    //Initialize size variable
    addrSize = sizeof server;
    
    int testTimes = 0;
    
    //while loop for receive packet from client
    while (1) {
        
        if(testTimes == 4)
            break;
        else
            testTimes++;
        
        //receive any incoming UDP datagram.
        uint8 recvbuffer[1024];
        memset(recvbuffer, 0, 1024);
        int recvSize = (int) recvfrom(udpSocket, recvbuffer, 1024, 0, (struct sockaddr *) &server, &addrSize);
        
        struct DataPack packet = readMsg(recvbuffer);
        
        if (recvSize > 0) {
            //process permission request packet
            printf("Recevied one packet from client %d...\n", packet.clientID);
            for (int i = 0; i < count; i++) {
                //if subscriber number is found
                if (packet.subscriber.subscriberNum == subscriberData[i].subscriberNum) {
                    //if technology matches
                    if (subscriberData[i].technology == packet.subscriber.technology) {
                        //if paid
                        if (subscriberData[i].paymentStatus == '1') {
                            printf("Subscriber number is found!\nTechnology matches!\nPaid!\n");
                            uint8 respondMessage[14];
                            initMessage(packet, respondMessage, 0);
                            sendto(udpSocket, respondMessage, 14, 0, (struct sockaddr *) &server, addrSize);
                            break;
                            
                        } else {
                            printf("Subscriber number is found!\nTechnology matches!\nNot paid!\n");
                            uint8 respondMessage[14];
                            initMessage(packet, respondMessage, 1);
                            sendto(udpSocket, respondMessage, 14, 0, (struct sockaddr *) &server, addrSize);
                            break;
                        }
                    } else {
                        printf("Subscriber number is found!\nThe technology does not match!\n");
                        unsigned char respondMessage[14];
                        initMessage(packet, respondMessage, 2);
                        sendto(udpSocket, respondMessage, 14, 0, (struct sockaddr *) &server, addrSize);
                        break;
                    }
                } else if (i == 2) {
                    printf("Subscriber number is not found!\n");
                    int respondSize = 14;
                    uint8 respondMessage[respondSize];
                    initMessage(packet, respondMessage, 2);
                    sendto(udpSocket, respondMessage, 14, 0, (struct sockaddr *) &server, addrSize);
                }
            }
            
        }
    }
    
    close(udpSocket);
    return 0;
}

