// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#include <cstring>
#include <iostream>
#include "message_specs.h"
#define PORT 51717
using std::cin;
using std::cout;
using std::endl;

int main(int argc, char const *argv[]) { 
	int sock = 0, valread; 
	struct sockaddr_in serv_addr; 
	//char *hello = "Hello from client"; 
	char buffer[1024] = {0}; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
		printf("\n Socket creation error \n"); 
		return -1; 
	} 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(PORT); 
	
	// Convert IPv4 and IPv6 addresses from text to binary form 
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) { 
		printf("\nInvalid address/ Address not supported \n"); 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		printf("\nConnection Failed \n"); 
		return -1; 
	}
    // Get message
    while (true) {
    Header header;
    NewOrder newOrder;
    DeleteOrder deleteOrder;
    ModifyOrderQuantity modifyOrderQuantity;
    Trade trade;
    uint16_t messageType = 0;
    uint64_t listingId, orderId, tradeId, orderQuantity, orderPrice, newQuantity, tradeQuantity, tradePrice;
    char side, *message;
    cout << "Insert message type: " << endl;
    cin  >> messageType;
    cout << "Message type: " << messageType << endl;
    switch (messageType) {
        case 1: { cin >> listingId;
                cin >> orderId;
                cin >> orderQuantity;
                cin >> orderPrice;
                cin >> side;
                newOrder.messageType   = 1;
                newOrder.listingId     = listingId;
                newOrder.orderId       = orderId;
                newOrder.orderQuantity = orderQuantity;
                newOrder.orderPrice    = orderPrice;
                newOrder.side          = side;
                // Define header
                header.version        = 0;
                header.payloadSize    = 35;
                header.sequenceNumber = 0;
                header.timestamp      = 1;
                // Defining message
                message = new char[51];
                std::memcpy(message, &header, 16);
                std::memcpy(message+16, &newOrder, 35);
                break;
                }
        case 2: { uint64_t orderId;
                cin >> orderId;
                deleteOrder.messageType = 2;
                deleteOrder.orderId     = orderId;
                // Define header
                header.version        = 0;
                header.payloadSize    = 10;
                header.sequenceNumber = 0;
                header.timestamp      = 1;
                // Defining message
                message = new char[26];
                std::memcpy(message, &header, 16);
                std::memcpy(message+16, &deleteOrder, 10);
                break;
                }
        case 3: { uint64_t orderId, newQuantity;
                cin >> orderId;
                cin >> newQuantity;
                modifyOrderQuantity.messageType = 3;
                modifyOrderQuantity.orderId     = orderId;
                modifyOrderQuantity.newQuantity = newQuantity;
                // Define header
                message = new char[34];
                header.version        = 0;
                header.payloadSize    = 18;
                header.sequenceNumber = 0;
                header.timestamp      = 1;
                // Defining message
                std::memcpy(message, &header, 16);
                std::memcpy(message+16, &modifyOrderQuantity, 18);
                break;
                }
        case 4: {
                    cin >> listingId;
                    cin >> tradeId;
                    cin >> tradeQuantity;
                    cin >> tradePrice;
                    message = new char[50];
                    trade.messageType = 4;
                    trade.listingId = listingId;
                    trade.tradeId = tradeId;
                    trade.tradeQuantity = tradeQuantity;
                    trade.tradePrice = tradePrice;
                    header.version = 0;
                    header.payloadSize = 34;
                    header.sequenceNumber = 0;
                    header.timestamp = 1;
                    std::memcpy(message, &header, 16);
                    std::memcpy(message+16, &trade, 34);
                    break;
                }
        default: {
                     std::cout << "Oooooooofffffff" << std::endl;
                     return 0;
                 }
    }
    /*
    Header header;
    header.version        = 0;
    header.payloadSize    = 35;
    header.sequenceNumber = 0;
    header.timestamp      = 1;
    NewOrder payload;
    payload.messageType   = 1;
    payload.listingId     = 1;
    payload.orderId       = 1;
    payload.orderQuantity = 10;
    payload.orderPrice    = 10;
    payload.side          = 'B';
    char *message = new char[51];
    std::memcpy(message, &header, 16);
    std::memcpy(message+16, &payload, 35);
    */
    // Sending message
    for (int i = 0; i < header.payloadSize+16; ++i) printf("%02x ", message[i] & 0xff);
    cout << endl;
    send(sock, message, header.payloadSize+16, 0);
	//send(sock , hello , strlen(hello) , 0 ); 
	cout << "Payload size: " << header.payloadSize << endl; 
    }
	return 0; 
} 

