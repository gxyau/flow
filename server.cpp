//Example code: A simple server side code, which echos back the received message. 
//Handle multiple socket connections with select and fd_set on Linux 
#include <arpa/inet.h> //close 
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdio.h> 
#include <errno.h>
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> // strlen
#include <string>
#include <set>
#include <sys/socket.h> 
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <unistd.h> // close
#include <unordered_map>
#include <vector>
#include "message_specs.h"

using std::unordered_map;
using std::vector;

#define TRUE 1     
#define FALSE 0     
#define PORT 51717

// Threshold
int BUY_THRESHOLD = 20, SELL_THRESHOLD = 15;

// Users and orders
unordered_map<int, vector<uint64_t>*> userOrders;

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

        bool addOrder(Order *order) {
            switch (order->side) {
                case 'B': buyQty += order->qty;
                        if (buyQty - netPos > BUY_THRESHOLD) {
                            buyQty -= order->qty;
                            return false;
                        }
                        break;
                case 'S': sellQty += order->qty;
                        if (sellQty - netPos > SELL_THRESHOLD) {
                            sellQty -= order->qty;
                            return false;
                        }
                        break;
            }
            return true;
        }

        bool modifyOrder(Order *order, int newQty) {
            int delta = newQty - order->qty;
            std::cout << "Old quantity: " << order->qty << std::endl;
            std::cout << "Delta: " << delta << std::endl;
            switch (order->side) {
                case 'B': buyQty += delta;
                        if (buyQty - netPos > BUY_THRESHOLD) {
                            buyQty -= delta;
                            return false;
                        }
                        break;
                case 'S': sellQty += delta;
                        if (sellQty - netPos > SELL_THRESHOLD) {
                            sellQty -= delta;
                            return false;
                        }
                        break;
            }
            // todo change order inside map (x)
            order->qty = newQty;
            std::cout << "New quantity: " << order->qty << std::endl;
            return true;
        }

        void deleteOrder(Order *order) {
            auto it = orders.find(order->orderId);
            if (it != orders.end()) {
                std::cout << "b.qty, s.qty: " << buyQty << ", " << sellQty << std::endl;
                switch(order->side) {
                    case 'B': buyQty  -= order->qty;
                              break;
                    case 'S': sellQty -= order->qty;
                              break;
                }
                std::cout << "b.qty, s.qty: " << buyQty << ", " << sellQty << std::endl;
                orders.erase(it);
                delete(order);
            } else {
                std::cout << "RED ALERT!!!" << std::endl;
            }
        }

        void trade(int tradeQty, char side) {
            netPos += tradeQty * (side = 'S' ? 1 : -1);
        }
};

// Map of financial instruments
unordered_map<int, FinancialInstrument*> financialInstruments;

// Removing users and associated orders when disconnected
void removeUser(int sd) { // sd = socket descrip
    for (uint64_t orderId : *userOrders[sd]) {
        auto it = orders.find(orderId);
        if (it != orders.end()) {
            // Output to shell
            std::cout << "Deleting order" << std::endl;
            std::cout << "Order ID: " << it->second->orderId << std::endl;
            FinancialInstrument* fi = financialInstruments.find(it->second->financialInstrumentId)->second;
            // Deleting it
            fi->deleteOrder(it->second);
            std::cout << "b.qty, s.qty: " << fi->buyQty << ", " << fi->sellQty << std::endl;
        }
    }
    // Remove user
    auto it = userOrders.find(sd);
    delete(it->second);
    userOrders.erase(it);
    std::cout << std::endl;
    return;
}


// Helper to execute message orders
void createNewOrder(int sd, char* buffer, Header &header, OrderResponse &orderResponse) {
    NewOrder newOrder;
    std::memcpy(&newOrder, buffer, header.payloadSize);
    Order *order = new Order(newOrder.orderId, newOrder.listingId, newOrder.orderQuantity, newOrder.orderPrice, newOrder.side);
    if (!financialInstruments.count(newOrder.listingId)) {
        FinancialInstrument *fi = new FinancialInstrument();
        financialInstruments[newOrder.listingId] = fi;
    }
    bool res = financialInstruments[newOrder.listingId]->addOrder(order);
    // Check if this works
    orderResponse.messageType = OrderResponse::MESSAGE_TYPE;
    orderResponse.orderId = order->orderId;
    orderResponse.status = res ? OrderResponse::Status::ACCEPTED : OrderResponse::Status::REJECTED;
    if (res) {
        orders[order->orderId] = order;
        userOrders[sd]->push_back(order->orderId);
    }
    // Output to shell
    std::cout << "Creating new order" << std::endl;
    std::cout << "Order ID: " << order->orderId << std::endl;
    std::cout << "Listing ID: " << order->financialInstrumentId << std::endl;
    std::cout << "Quantity: " << order->qty << std::endl;
    std::cout << "Price: " << order->price << std::endl;
    std::cout << "Side: " << order->side << std::endl;
    std::cout << "Result: " << (res ? "ACCEPTED" : "REJECTED") << std::endl;
}
void deleteExistingOrder(char* buffer, Header &header) {
    DeleteOrder deleteOrder;
    std::memcpy(&deleteOrder, buffer, header.payloadSize);
    auto it = orders.find(deleteOrder.orderId);
    if (it == orders.end()) {
        std::cerr << "Order does not exists! No order(s) deleted." << std::endl;
        return;
    }
    Order *order = it->second;
    // Output to shell
    std::cout << "Deleting order" << std::endl;
    std::cout << "Order ID: " << order->orderId << std::endl;
    FinancialInstrument* fi = financialInstruments.find(order->financialInstrumentId)->second;
    // Deleting it
    fi->deleteOrder(order);
    return;
}
void modifyExistingOrder(char* buffer, Header &header, OrderResponse &orderResponse) {
    ModifyOrderQuantity modifyOrderQuantity;
    std::memcpy(&modifyOrderQuantity, buffer, header.payloadSize);
    //todo check for existence (x)
    auto it = orders.find(modifyOrderQuantity.orderId);
    if (it == orders.end()) {
        std::cerr << "Order does not exists! No modification.";
        return;
    }
    Order *order = it->second;
    // Output to shell, first part
    std::cout << "Modifying order" << std::endl;
    std::cout << "Order ID: " << order->orderId << std::endl;
    std::cout << "Old quantity: " << order->qty << std::endl;
    // Updating
    FinancialInstrument* fi = financialInstruments.find(order->financialInstrumentId)->second;
    bool res = fi->modifyOrder(order, modifyOrderQuantity.newQuantity);
    // Response message
    orderResponse.messageType = OrderResponse::MESSAGE_TYPE;
    orderResponse.orderId = order->orderId;
    orderResponse.status = res ? OrderResponse::Status::ACCEPTED : OrderResponse::Status::REJECTED;
    // Output to shell, second part
    std::cout << "New quantity: " << modifyOrderQuantity.newQuantity << std::endl;
    std::cout << "Result: " << (res ? "ACCEPTED" : "REJECTED") << std::endl;
    return;
}

void executeTrade(char *buffer, Header &header) {
    Trade trade;
    std::memcpy(&trade, buffer, header.payloadSize);
    auto it = orders.find(trade.tradeId);
    if (it == orders.end()) {
        std::cerr << "Order " << trade.tradeId << " not found, no transaction" << std::endl;
        return;
    }
    Order *order = orders.find(trade.tradeId)->second;
    FinancialInstrument fi = *financialInstruments.find(trade.listingId)->second;
    fi.trade(trade.tradeQuantity, order->side);
    // Output to shell
    std::cout << "Trade" << std::endl;
    std::cout << "Order ID: " << order->orderId << std::endl;
    std::cout << "Listing ID: " << order->financialInstrumentId << std::endl;
    std::cout << "Quantity: " << order->qty << std::endl;
    std::cout << "Price: " << order->price << std::endl;
    std::cout << "Side: " << order->side << std::endl;
    return;
}

int main(int argc , char *argv[]) { 
	int opt = TRUE; 
	int master_socket , addrlen , new_socket , activity, i , valread , sd; 
	int max_sd; 
	struct sockaddr_in address;
    std::set<int> client_socket;

    // Reading threshold, if any
    if (argc >= 3) BUY_THRESHOLD = std::atoi(argv[2]);
    if (argc >= 4) SELL_THRESHOLD = std::atoi(argv[3]);
    std::cout << "B, S: " << BUY_THRESHOLD << ", " << SELL_THRESHOLD << std::endl;
		
	char buffer[1024]; //data buffer of 1K 
		
	//set of socket descriptors 
	fd_set readfds; 
		
	//create a master socket 
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) { 
		perror("socket failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	//set master socket to allow multiple connections , 
	//this is just a good habit, it will work without this 
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) { 
		perror("setsockopt"); 
		exit(EXIT_FAILURE); 
	} 
	
	//type of socket created 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( PORT ); 
		
	//bind the socket to localhost port 8888 
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) { 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	printf("Listener on port %d \n", PORT); 
		
	//try to specify maximum of 3 pending connections for the master socket 
	if (listen(master_socket, 3) < 0) { 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 
		
	//accept the incoming connection 
	addrlen = sizeof(address); 
	puts("Waiting for connections ..."); 
		
	while(TRUE) { 
		//clear the socket set 
		FD_ZERO(&readfds); 
	
		//add master socket to set 
		FD_SET(master_socket, &readfds); 
		max_sd = master_socket; 
			
		//add child sockets to set 
		for ( auto it = client_socket.begin(); it != client_socket.end(); it++ ) { 
			//socket descriptor 
			sd = *it; 
				
			//if valid socket descriptor then add to read list 
			if(sd > 0) FD_SET( sd , &readfds); 
				
			//highest file descriptor number, need it for the select function 
			if(sd > max_sd) max_sd = sd; 
		} 
	
		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely 
		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL); 
	
		if ((activity < 0) && (errno!=EINTR)) { 
			printf("select error"); 
		} 
			
		//If something happened on the master socket , 
		//then its an incoming connection 
		if (FD_ISSET(master_socket, &readfds)) { 
			if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
				perror("accept"); 
				exit(EXIT_FAILURE); 
			} 
			
			//inform user of socket number - used in send and receive commands 
			printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port)); 

			//add new socket to array of sockets 
			client_socket.insert(new_socket);
            userOrders[new_socket] = new vector<uint64_t> ();
		} 
			

        std::vector<int> to_erase;
		//else its some IO operation on some other socket 
		for (auto it = client_socket.begin(); it != client_socket.end(); it++) { 
			sd = *it; 
				
			if (FD_ISSET( sd , &readfds)) { 
				//Check if it was for closing , and also read the 
				//incoming message
				if ((valread = read( sd , buffer, 16)) <= 0) { 
					//Somebody disconnected , get his details and print 
					getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen); 
					printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr) , ntohs(address.sin_port)); 
					
                    // HANDLE DISCONNECTED USER
                    removeUser(sd);
                    /*
                    for (uint64_t orderId : *userOrders[sd]) {
                        auto it = orders.find(orderId);
                        if (it != orders.end()) {
                            // Output to shell
                            std::cout << "Deleting order" << std::endl;
                            std::cout << "Order ID: " << it->second->orderId << std::endl;
                            FinancialInstrument* fi = financialInstruments.find(it->second->financialInstrumentId)->second;
                            // Deleting it
                            fi->deleteOrder(it->second);
                            std::cout << "b.qty, s.qty: " << fi->buyQty << ", " << fi->sellQty << std::endl;
                        }
                    }
                    // Remove user
                    auto it = userOrders.find(sd);
                    delete(it->second);
                    userOrders.erase(it);
                    std::cout << std::endl;
                    */

					//Close the socket and mark as 0 in list for reuse 
					close( sd ); 
					to_erase.push_back(sd);
				} else { 
                    Header header;
                    std::cout << valread << " bytes read" << std::endl;
                    std::memcpy(&header, buffer, 16);
                    valread = read(sd, buffer, header.payloadSize);
                    // Get message type
                    uint16_t *messageType = (uint16_t*) buffer;
                    std::cout << "Message type: " << *messageType << std::endl;
                    // Initiate order and response
                    // TODO: move the variables into switch
                    Order *order;
                    OrderResponse orderResponse;
                    bool res, reply = false;
                    switch(*messageType) { // Switch between message types
                        case 1: {
                            reply = true;
                            createNewOrder(sd, buffer, header, orderResponse);
                            /*
                            NewOrder newOrder;
                            std::memcpy(&newOrder, buffer, header.payloadSize);
                            order = new Order(newOrder.orderId, newOrder.listingId, newOrder.orderQuantity, newOrder.orderPrice, newOrder.side);
                            if (!financialInstruments.count(newOrder.listingId)) {
                                FinancialInstrument *fi = new FinancialInstrument();
                                financialInstruments[newOrder.listingId] = fi;
                            }
                            std::cout << "Current Buy Quantity: " << financialInstruments[newOrder.listingId]->buyQty << std::endl;
                            std::cout << "Current Sell Quantity: " << financialInstruments[newOrder.listingId]->sellQty << std::endl;
                            res = financialInstruments[newOrder.listingId]->addOrder(order);
                            // Check if this works
                            orderResponse.messageType = OrderResponse::MESSAGE_TYPE;
                            orderResponse.orderId = order->orderId;
                            orderResponse.status = res ? OrderResponse::Status::ACCEPTED : OrderResponse::Status::REJECTED;
                            reply = true; // Will send response
                            if (res) {
                                orders[order->orderId] = order;
                                userOrders[sd]->push_back(order->orderId);
                            }
                            // Output to shell
                            std::cout << "Creating new order" << std::endl;
                            std::cout << "Order ID: " << order->orderId << std::endl;
                            std::cout << "Listing ID: " << order->financialInstrumentId << std::endl;
                            std::cout << "Quantity: " << order->qty << std::endl;
                            std::cout << "Price: " << order->price << std::endl;
                            std::cout << "Side: " << order->side << std::endl;
                            std::cout << "Result: " << (res ? "ACCEPTED" : "REJECTED") << std::endl;
                            */
                            break;
                            } 
                        case 2: {
                            deleteExistingOrder(buffer, header);
                                    /*
                            DeleteOrder deleteOrder;
                            std::memcpy(&deleteOrder, buffer, header.payloadSize);
                            auto it = orders.find(deleteOrder.orderId);
                            if (it == orders.end()) {
                                std::cerr << "Order does not exists! No order(s) deleted." << std::endl;
                                break;
                            }
                            order = it->second;
                            // Output to shell
                            std::cout << "Deleting order" << std::endl;
                            std::cout << "Order ID: " << order->orderId << std::endl;
                            FinancialInstrument* fi = financialInstruments.find(order->financialInstrumentId)->second;
                            // Deleting it
                            fi->deleteOrder(order);
                            */
                            break; 
                            }
                        case 3: { 
                            reply = true;
                            modifyExistingOrder(buffer, header, orderResponse);
                            /*
                            ModifyOrderQuantity modifyOrderQuantity;
                            std::memcpy(&modifyOrderQuantity, buffer, header.payloadSize);
                            //todo check for existence (x)
                            auto it = orders.find(modifyOrderQuantity.orderId);
                            if (it == orders.end()) {
                                std::cerr << "Order does not exists! No modification.";
                                break;
                            }
                            order = it->second;
                            // Output to shell, first part
                            std::cout << "Modifying order" << std::endl;
                            std::cout << "Order ID: " << order->orderId << std::endl;
                            std::cout << "Old quantity: " << order->qty << std::endl;
                            // Updating
                            FinancialInstrument* fi = financialInstruments.find(order->financialInstrumentId)->second;
                            res = fi->modifyOrder(order, modifyOrderQuantity.newQuantity);
                            // Response message
                            orderResponse.messageType = OrderResponse::MESSAGE_TYPE;
                            orderResponse.orderId = order->orderId;
                            orderResponse.status = res ? OrderResponse::Status::ACCEPTED : OrderResponse::Status::REJECTED;
                            reply = true; // Will send response
                            // Output to shell, second part
                            std::cout << "New quantity: " << modifyOrderQuantity.newQuantity << std::endl;
                            std::cout << "Result: " << (res ? "ACCEPTED" : "REJECTED") << std::endl;
                            */
                            break; 
                        }
                        case 4: {
                            executeTrade(buffer, header);
                            /*
                            Trade trade;
                            std::memcpy(&trade, buffer, header.payloadSize);
                            auto it = orders.find(trade.tradeId);
                            if (it == orders.end()) {
                                std::cerr << "Order " << trade.tradeId << " not found, no transaction" << std::endl;
                                break;
                            }
                            order = orders.find(trade.tradeId)->second;
                            FinancialInstrument fi = *financialInstruments.find(trade.listingId)->second;
                            fi.trade(trade.tradeQuantity, order->side);
                            // Output to shell
                            std::cout << "Trade" << std::endl;
                            std::cout << "Order ID: " << order->orderId << std::endl;
                            std::cout << "Listing ID: " << order->financialInstrumentId << std::endl;
                            std::cout << "Quantity: " << order->qty << std::endl;
                            std::cout << "Price: " << order->price << std::endl;
                            std::cout << "Side: " << order->side << std::endl;
                            break;
                            */
                            break;
                        }
                }
                // Making response message, if needed
                if (reply) {
                    char *message = new char[28];
                    header.payloadSize = 28;
                    header.sequenceNumber += 1;
                    header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::system_clock::now().time_since_epoch()).count();
                    std::memcpy(message, &header, 16);
                    std::memcpy(message+16, &orderResponse, 12);
                    send(sd, message, 28, 0);
                }
            //send(new_socket , hello , strlen(hello) , 0 ); 
            std::cout << std::endl;
            } 
			} 
		}
        for(int i : to_erase) {
            client_socket.erase(i);
        } 
	} 
		
	return 0; 
} 
