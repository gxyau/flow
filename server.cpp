// Server side C/C++ program to demonstrate Socket programming 
#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <cstring>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include "message_specs.h"
#define PORT 8080 
using std::unordered_map;

// Threshold
int BUY_THRESHOLD = 20, SELL_THRESHOLD = 15;


struct Order {
        uint64_t orderId, financialInstrumentId, qty, price;
        char side;
        Order() : orderId(), financialInstrumentId(), qty(), price(), side() {}
        Order(int oId, int fiId, int q, int p, char s) : orderId(oId), financialInstrumentId(fiId), qty(q), price(p), side(s) {}
};

// Map of orders
unordered_map<int, Order*> orders;

class FinancialInstrument {
    public:
        int netPos = 0, buyQty = 0, sellQty = 0;
        
        int getHypotheticalBuy() const { 
            return std::max(buyQty, netPos + buyQty);
        }

        int getHypotheticalSell() const {
            return std::max(sellQty, sellQty - netPos);
        }

        bool addOrder(Order &order) {
            switch (order.side) {
                case 'B': buyQty += order.qty;
                        if (buyQty - netPos > BUY_THRESHOLD) {
                            buyQty -= order.qty;
                            return false;
                        }
                        break;
                case 'S': sellQty += order.qty;
                        if (sellQty - netPos > SELL_THRESHOLD) {
                            sellQty -= order.qty;
                            return false;
                        }
                        break;
            }
            orders[order.orderId] = &order;
            return true;
        }

        bool modifyOrder(Order &order, int newQty) {
            switch (order.side) {
                case 'B': if (newQty - order.qty + buyQty > BUY_THRESHOLD) return false;
                        buyQty   += newQty - order.qty;
                        order.qty = newQty;
                        break;
                case 'S': if (newQty - order.qty + sellQty > SELL_THRESHOLD) return false;
                        sellQty   += newQty - order.qty;
                        order.qty  = newQty;
                        break;
            }
            // todo change order inside map (x)
            orders[order.orderId]->qty = newQty;
            return true;
        }

        void deleteOrder(Order &order) {
            auto it = orders.find(order.orderId);
            if (it != orders.end()) {
                switch(order.side) {
                    case 'B': buyQty  -= order.qty;
                    case 'S': sellQty -= order.qty;
                }
                orders.erase(it);
            }
        }

        void trade(int tradeQty, char side) {
            netPos += tradeQty * (side = 'S' ? 1 : -1);
        }
};

// Map of financial instruments
unordered_map<int, FinancialInstrument*> financialInstruments;

int main(int argc, char const *argv[]) { 
	int server_fd, new_socket, valread; 
	struct sockaddr_in address; 
	int opt = 1; 
	int addrlen = sizeof(address); 
	char buffer[1024] = {0}; 
	//char *hello = "Hello from server"; 
	
	// Creating socket file descriptor 
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
		perror("socket failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	// Forcefully attaching socket to the port 8080 
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
												&opt, sizeof(opt))) { 
		perror("setsockopt"); 
		exit(EXIT_FAILURE); 
	} 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( PORT ); 
	
	// Forcefully attaching socket to the port 8080 
	if (bind(server_fd, (struct sockaddr *)&address, 
								sizeof(address))<0) { 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	if (listen(server_fd, 3) < 0) { 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
					(socklen_t*)&addrlen))<0) { 
		perror("accept"); 
		exit(EXIT_FAILURE); 
	} 
    // Read header
    while (true) {
        Header header;
	    valread = read( new_socket , buffer, 16);
        if (!valread) continue;
            std::cout << valread << " bytes read" << std::endl;
        std::memcpy(&header, buffer, 16);
        valread = read(new_socket, buffer, header.payloadSize);
        // Get message type
        uint16_t *messageType = (uint16_t*) buffer;
        // Initiate order and response
        // TODO: move the variables into switch
        Order order;
        OrderResponse orderResponse;
        bool res, reply = false;
        switch(*messageType) { // Switch between message types
            case 1: {
                        NewOrder newOrder;
                        std::memcpy(&newOrder, buffer, header.payloadSize);
                        order = Order(newOrder.orderId, newOrder.listingId, newOrder.orderQuantity, newOrder.orderPrice, newOrder.side);
                        if (!financialInstruments.count(newOrder.listingId)) {
                            FinancialInstrument *fi = new FinancialInstrument();
                            financialInstruments[newOrder.listingId] = fi;
                        }
                        res = financialInstruments[newOrder.listingId]->addOrder(order);
                        // Check if this works
                        orderResponse.messageType = OrderResponse::MESSAGE_TYPE;
                        orderResponse.orderId = order.orderId;
                        orderResponse.status = res ? OrderResponse::Status::ACCEPTED : OrderResponse::Status::REJECTED;
                        reply = true; // Will send response
                        break;
                    }
            case 2: {
                        DeleteOrder deleteOrder;
                        std::memcpy(&deleteOrder, buffer, header.payloadSize);
                        order = *orders.find(deleteOrder.orderId)->second;
                        FinancialInstrument fi = *financialInstruments.find(order.financialInstrumentId)->second;
                        fi.deleteOrder(order);
                        break; 
                    }
            case 3: { 
                        ModifyOrderQuantity modifyOrderQuantity;
                        std::memcpy(&modifyOrderQuantity, buffer, header.payloadSize);
                        //todo check for existence (x)
                        auto it = orders.find(modifyOrderQuantity.orderId);
                        if (it == orders.end()) break;
                        order = *it->second;
                        FinancialInstrument fi = *financialInstruments.find(order.financialInstrumentId)->second;
                        res = fi.modifyOrder(order, modifyOrderQuantity.newQuantity);
                        // Response message
                        orderResponse.messageType = OrderResponse::MESSAGE_TYPE;
                        orderResponse.orderId = order.orderId;
                        orderResponse.status = res ? OrderResponse::Status::ACCEPTED : OrderResponse::Status::REJECTED;
                        reply = true; // Will send response
                        break; 
                    }
            case 4: {
                        Trade trade;
                        std::memcpy(&trade, buffer, header.payloadSize);
                        order = *orders.find(trade.tradeId)->second;
                        FinancialInstrument fi = *financialInstruments.find(trade.listingId)->second;
                        fi.trade(trade.tradeQuantity, order.side);
                        break;
                    }
        }
        std::cout << "Financial instruments is empty: " << financialInstruments.empty() << std::endl;
        std::cout << "Orders is empty: " << orders.empty() << std::endl;
	    printf("%s\n",buffer );
        // Making response message, if needed
        if (reply) {
            std::cout << "Choo choo MF" << std::endl;
            char *message = new char[28];
            header.payloadSize = 28;
            header.sequenceNumber += 1;
            header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::system_clock::now().time_since_epoch()).count();
            std::memcpy(message, &header, 16);
            std::memcpy(message+16, &orderResponse, 12);
            send(new_socket, message, 28, 0);
        }
    	//send(new_socket , hello , strlen(hello) , 0 ); 
	    printf("Hello message sent\n"); 
    }
	return 0; 
} 

