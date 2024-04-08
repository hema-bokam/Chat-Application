/**
 * @assignment1
 * @author  Team Members <ubitname@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <string.h>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <netdb.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>

#include "../include/global.h"
#include "../include/logger.h"
#include <set>

// #include "logger.cpp"


using namespace std;

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
#define BACKLOG 5
#define STDIN_FD 0
#define TRUE 1
#define CMD_SIZE 100
#define BUFFER_SIZE 256


//we referred to beej guide for socket programming core functionalities
//reference: https://beej.us/guide/bgnet/html/split/index.html

struct buffer_message_info{
	string source_ip;
	string destination_ip;
	string message;
};

//to store all the required information related to the client
struct client_details{
	string ip;
	int port_num;
	string host_name;
	bool login_state;
	//below field is useful for sending data to the client 
	int socket_fd; //it will help to identify the client socket
	//set to store blocked clients
    set<string> blocked_clients_set; //stores ip address of blocked clients
	int msgs_recv = 0;
	int msgs_sent = 0;
	map<string, vector<buffer_message_info>> buffer_messages_map;
};

bool is_valid_ip_address(const string& ip);  //checks if the given IP address is in valid format or not.
bool is_valid_port(const string& port);  //checks if the given port is a valid number or not

class Server{
	public: 
		int server_port_num;
	    sockaddr_in server_address;
		int server_socket;
		vector<client_details> clients; //stores all information of connected clients
		//char ip_buffer[INET_ADDRSTRLEN];
		char* server_ip_address;

		Server(int port_num){
			server_port_num = port_num;
			int max_sd;
			//declare file descriptor
			fd_set readfds;
			char command[100];  //to store input command
			//create TCP socket
			server_socket = socket(AF_INET, SOCK_STREAM, 0);
			if (server_socket == -1) {
				//there is some error in creating socket
				return;
			}

			// Bind the socket to an address and the given port number
			memset(&server_address, 0, sizeof(server_address));   
			server_address.sin_family = AF_INET;
			server_address.sin_addr.s_addr = htonl(INADDR_ANY);
			server_address.sin_port = htons(server_port_num);

			if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
				cerr << "Binding failed" << endl;
				return;
			}
			// Listen for incoming connections
			if (listen(server_socket, BACKLOG) < 0) {
				std::cerr << "Error listening" << std::endl;
				return;
			}

			char server_hostname[NI_MAXHOST];
			gethostname(server_hostname, NI_MAXHOST);
			if (gethostname(server_hostname, sizeof(server_hostname)) == -1) {
				cerr << "Failed to get local hostname." << endl;
				return; 
			}

			struct hostent *host_info = gethostbyname(server_hostname);
			if (host_info == nullptr) {
				cerr << "Failed to get host info for " << server_hostname << endl;
				return; 
			}
			server_ip_address = inet_ntoa(*(struct in_addr *)host_info->h_addr_list[0]);
			//cout<<"server hostname: "<<server_hostname << endl;
			//cout << "server ip address: "<<server_ip_address<<endl;
			int addressLen = sizeof(server_address);
			
			while(true) {
				
				FD_ZERO(&readfds);
				//add master socket to set
				FD_SET(server_socket, &readfds);
				max_sd = server_socket;

				for (auto& client : clients) {
					FD_SET(client.socket_fd, &readfds);
					if (client.socket_fd > max_sd) {
						max_sd = client.socket_fd;
					}
				}
				FD_SET(STDIN_FILENO, &readfds);
				if (STDIN_FILENO > max_sd) {
					max_sd = STDIN_FILENO; // Ensure max_sd tracks the highest file descriptor
				}
				// reference: https://beej.us/guide/bgnet/html/split/slightly-advanced-techniques.html#select
				//waiting for an activity on one of the sockets, timeout is NULL.
				int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
				if((activity < 0) && (errno!=EINTR)) {
					cout << "select error";
				}
				//Accepts incoming requests
				if(FD_ISSET(server_socket, &readfds)) {
					int client_socket;
					sockaddr_in client_address;
            	    socklen_t client_address_size = sizeof(client_address);
					if((client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_size)) < 0){
						//error accepting incoming request
						exit(0);
					}
					char client_hostname[NI_MAXHOST];
					int res = getnameinfo((struct sockaddr*)&client_address, sizeof(client_address),client_hostname, NI_MAXHOST, NULL, 0, 0);
					//check if loggedin client is trying to login again
					// bool isClientExist = false;
					// for(auto &client : clients){
					// 	if(client.ip == inet_ntoa(client_address.sin_addr)){
					// 		isClientExist = true;
					// 		client.socket_fd = client_socket;
					// 		client.login_state = true;
					// 		break;
					// 	}
					// }
					// if(!isClientExist){
					// 	//cout << "New client"<<endl;
					// 	if (res == 0) {
					// 		//cout << "New client"<<endl;
					// 		//store client information
					// 		client_details client;
					// 		client.host_name = client_hostname;
					// 		client.ip = inet_ntoa(client_address.sin_addr);
					// 		client.port_num = ntohs(client_address.sin_port);
					// 		client.socket_fd = client_socket;	
					// 		client.login_state = true;
					// 		//store in the vector. It will help retrive all connected clients to the given server
					// 		clients.push_back(client);
					// 	}
					// }
					if (res == 0) {
						//store client information
						client_details client;
						client.host_name = client_hostname;
						client.ip = inet_ntoa(client_address.sin_addr);
						client.port_num = ntohs(client_address.sin_port);
						client.socket_fd = client_socket;	
						client.login_state = true;
						//store in the vector. It will help retrive all connected clients to the given server
						clients.push_back(client);
					}
				}
				// Handle client messages
				for (auto &client : clients) {
					if(client.socket_fd >= 0){
						if (FD_ISSET(client.socket_fd, &readfds)) {
							handleClientMessage(client.socket_fd);
							// if (!handleClientMessage(client.socket_fd)) {
							// 	// If handleClientMessage returns false, it means the client disconnected or logged out or exited
							// 	close(client.socket_fd); //close the client socket
							// 	client.socket_fd = -1; // Mark as closed
							// }
						}
					}
				}
				//remove clients that are marked as closed.
				// clients.erase(std::remove_if(clients.begin(), clients.end(), 
				// 							[](const client_details& c) { return c.socket_fd == -1; }), clients.end());
				//handle input operations
				if(FD_ISSET(STDIN_FILENO, &readfds)){
						memset(&command, 0, sizeof(command));
						ssize_t readBytes = read(STDIN_FILENO, command, sizeof(command) - 1); // read command
						if (readBytes > 0) {
							if (command[readBytes - 1] == '\n') {
								command[readBytes - 1] = '\0';
							}
						}
						if(strcmp(command, "PORT") == 0){
							print_server_port();
						}else if(strcmp(command, "IP") == 0){
							print_server_ip();
						}else if(strcmp(command, "AUTHOR") == 0){
							print_server_author();
						}else if(strcmp(command, "LIST") == 0){
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							print_connected_clients();
							cse4589_print_and_log("[%s:END]\n", command);
						}else if(strncmp(command, "BLOCKED", 7) == 0){
							stringstream ss(command);
							string command_str;
							vector<string> tokens;

							// Tokenize the input
							while (ss >> command) {
								tokens.push_back(command);
							}
							// cout << "command is: "<<command_str<<endl;
							// cout << "Blocked ip: "<<ip_client<<endl;
							try{
								if((tokens.size() < 2) || !is_valid_ip_address(tokens[1]) || !check_ip_exists(tokens[1])){
									throw invalid_argument("IP is invalid");
								}
								//cout <<"Valid ip"<<endl;
								print_blocked_clients(tokens[1]);
							}catch(const invalid_argument& exception){
								cse4589_print_and_log("[%s:ERROR]\n", tokens[0].c_str());
								cse4589_print_and_log("[%s:END]\n", tokens[0].c_str());
							}
						}
				}
			}
		}
		bool check_ip_exists(string ip){
			for(auto &client : clients){
                if(client.ip == ip){
                    return true;
                }
            }
			return false;
		}
		string serialize_clients_data(string command){
			stringstream ss;
			ss << command << " ";
			for (const auto& client : clients) {
				//if(client.login_state){
					ss << client.ip << "," 
					<< client.port_num << ","
					<< client.host_name << ","
					<< (client.login_state ? "true" : "false") << ","
					<< (client.socket_fd) <<  ";";
				//}
			}
			//cout << "Serialized client info string: "<<ss.str()<<endl;
			return ss.str();
		}

		//It will handle 2 client messages, i.e., LOGOUT and SEND_CLIENTS_INFO
		//If clients sends "SEND_CLIENTS_INFO" message we are directly sending required information from this method
		//If client send "LOGOUT" message to the server, then we return boolean value. We will handle this case in server loop
		void handleClientMessage(int client_socket) {
			char buffer[1024];
			memset(buffer, 0, sizeof(buffer));
			int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
			if (bytes_received <= 0) {
				return;
			}
			//cout << "Buffer message before: "<<buffer<<endl;
			char* token = strtok(buffer, " ");
			char command_str[100];
			if (token != nullptr) {
                strcpy(command_str, token);
			}
			//cout << "Command is: "<<command_str<<endl;
			//cout << "Buffer message: "<<buffer<<endl;
			// Check if the message is a logout request
			if(strcmp(command_str, "LOGIN") == 0){
				//cout << "Inside handle client login "<<endl;
				string data_to_transfer = serialize_clients_data(command_str);
				//cout << "After serialization data: "<<data_to_transfer<<endl;
				uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
				send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
				send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); // Then send the data
				// for(auto &client : clients){
				// 	if(client.socket_fd == client_socket){
				// 		//check if there are any buffered messages for the client
				// 		if(client.buffer_messages_map.find(client.ip) != client.buffer_messages_map.end()){
				// 			for(auto &buffer_message : client.buffer_messages_map[client.ip]){
				// 				char command[] = "RELAYED";									
				// 				cse4589_print_and_log("[%s:SUCCESS]\n", command);
				// 				cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", buffer_message.source_ip.c_str(), buffer_message.destination_ip.c_str(), buffer_message.message.c_str());
				// 				cse4589_print_and_log("[%s:END]\n", command);
				// 				string data_to_transfer = "SEND " + buffer_message.source_ip + " " + buffer_message.message;
				// 				// cout << "After serialization data: "<<data_to_transfer<<endl;
				// 				uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
				// 				send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
				// 				send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); // Then send the data
				// 			}
				// 			client.buffer_messages_map.erase(client.ip);
				// 		}
				// 	}
				//}
				return;
			}else if(strcmp(command_str, "EXIT") == 0){
				for(auto &client : clients){
					if(client.socket_fd == client_socket){
						client.socket_fd = -1;
						break;
					}
				}
				//cout << "before exit clients size: "<<clients.size()<<endl;
				clients.erase(std::remove_if(clients.begin(), clients.end(), 
											[](const client_details& c) { return c.socket_fd == -1; }), clients.end());
				//cout << "After exit clients size: "<<clients.size()<<endl;
				return; // Indicates that the client wants to exit
			}else if(strcmp(command_str, "LOGOUT") == 0){
				//cout << "Inside handle client logout"<<endl;
				for(auto &client : clients){
					if(client.socket_fd == client_socket){
						client.login_state = false;
						// close(client_socket);
						// client.socket_fd = -1;
						// client_socket = -1;
						return;
					}
				}
			}else if(strcmp(command_str, "REFRESH") == 0){
				string data_to_transfer = serialize_clients_data(command_str);
				//cout << "After serialization data: "<<data_to_transfer<<endl;
				uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
				send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
				send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); // Then send the data
			
			}else if(strcmp(command_str, "SEND") == 0){
				//cout << "Inside handle client: send"<<endl;
               // char* token =  strtok(buffer, " "); 
                char source_ip[INET_ADDRSTRLEN];
                char destination_ip[INET_ADDRSTRLEN];
                char message[300];
				token = strtok(nullptr, " ");
                if (token != nullptr) {
                    strcpy(source_ip, token);
                    token = strtok(nullptr, " ");
                    if(token != nullptr){
                        strcpy(destination_ip, token);
                        token = strtok(nullptr, "");
                        if(token != nullptr){
                            strcpy(message, token);
                        }
                    }
                }
				// cout << "message: "<<message<<endl;
				// cout <<"source ip: "<<source_ip<<endl;
				// cout<<"destination ip: "<<destination_ip<<endl;

				bool isDestinationIpExist = false;
            	//try to send message to the corresponding client
				auto sender_client = std::find_if(clients.begin(), clients.end(), [client_socket](const client_details& client) {
					return client.socket_fd == client_socket;
				});
				auto receiver_client = std::find_if(clients.begin(), clients.end(), [destination_ip](const client_details& client) {
					return client.ip == destination_ip && client.login_state;
				});
				if(receiver_client == clients.end()){
					return;
				}
				// if(sender_client != clients.end()){
				// 	send error
				// }
				bool isBlocked = false;
				
				if(receiver_client->blocked_clients_set.find(source_ip) != receiver_client->blocked_clients_set.end()){
					isBlocked = true;
                }
				if(!isBlocked){
					if(receiver_client->login_state){
						char command[] = "RELAYED";
						cse4589_print_and_log("[%s:SUCCESS]\n", command);
						cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", source_ip, destination_ip, message);
						cse4589_print_and_log("[%s:END]\n", command);
						string data_to_transfer = "SEND " + string(source_ip) + " " + message;
						// cout << "After serialization data: "<<data_to_transfer<<endl;
						uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
						send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
						send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); // Then send the data
					}
					else{
						buffer_message_info buffer_message;
						buffer_message.source_ip = source_ip;
						buffer_message.destination_ip = destination_ip;
						buffer_message.message = message;
						receiver_client->buffer_messages_map[destination_ip].push_back(buffer_message);
						//cout << "Buffered messages size: "<<client.buffer_messages_map[destination_ip].size()<<endl;
					}
                }
            }else if((strcmp(command_str, "BLOCK") == 0) || (strcmp(command_str, "UNBLOCK") == 0) || (strcmp(command_str, "BLOCKED") == 0)){
                char client_ip[INET_ADDRSTRLEN];
				token = strtok(nullptr, "");
                if (token != nullptr) {
                    strcpy(client_ip, token);
				}
				//cout << "Client ip to block: "<<client_ip <<endl;
				auto sender_client = std::find_if(clients.begin(), clients.end(), [client_socket](const client_details& client) {
					return client.socket_fd == client_socket;
				});
				string data_to_transfer;
				if(strcmp(command_str, "BLOCK") == 0){
					if(sender_client->blocked_clients_set.find(client_ip) != sender_client->blocked_clients_set.end()){
						//sender_client->socket_fd == client_socket
						//already blocked
						data_to_transfer = "BLOCK ERROR";
						//send error message		
					}
					else{
						sender_client->blocked_clients_set.insert(client_ip);
						data_to_transfer = "BLOCK SUCCESS";
					}
				}
				else if(strcmp(command_str, "UNBLOCK") == 0){
					if(sender_client->blocked_clients_set.find(client_ip) == sender_client->blocked_clients_set.end()){
						//sender_client->socket_fd == client_socket
						//already blocked
						data_to_transfer = "UNBLOCK ERROR";
						//send error message		
					}
					else{
						sender_client->blocked_clients_set.erase(client_ip);
						data_to_transfer = "UNBLOCK SUCCESS";
					}
				}
				//cout << "Client socket: "<<client_socket << " Block client socket: "<<  sender_client->socket_fd <<endl;
				//cout << "data to transfer for block: "<<data_to_transfer<<endl;
				uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
				send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
				send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); 
				return;
			}
			return;
		}

		void print_blocked_clients(string ip_client){
			char command_str[] = "BLOCKED";
			auto client = std::find_if(clients.begin(), clients.end(), [&ip_client](const client_details& client) {
				return client.ip == ip_client;
			});		
			vector<client_details> blockedClients;
			for (const auto& blocked_ip : client->blocked_clients_set) { // Iterate over blocked IPs
				for (const auto& client : clients) { // Iterate over all clients
					if (client.ip == blocked_ip) { // If client's IP is in the blocked set
						blockedClients.push_back(client); // Store the client's details
						break; 
					}
				}
			}
			std::sort(blockedClients.begin(), blockedClients.end(), [](const client_details& c1, const client_details& c2) {
				return c1.port_num < c2.port_num;
			});
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			//display list of blocked clients
			for (int i=0; i<blockedClients.size(); i++) {
				cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i+1, blockedClients[i].host_name.c_str(), blockedClients[i].ip.c_str(), blockedClients[i].port_num);
			}
			cse4589_print_and_log("[%s:END]\n", command_str);
			return;
		}

		void print_server_author(){
			char command_str[] = "AUTHOR";
			string MY_TEAM_NAME = "hemaboka-srachako";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", MY_TEAM_NAME.c_str());
			cse4589_print_and_log("[%s:END]\n", command_str);
		}

		void print_server_port(){
			char command_str[] = "PORT";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("PORT:%d\n", server_port_num);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}

		void print_server_ip(){
			// int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    		// if (udp_socket < 0) {
        	// 	std::cerr << "Failed to create socket" << std::endl;
    		// }
			// // Google's public DNS server address (8.8.8.8) 
			// struct sockaddr_in external_address;
			// memset(&external_address, 0, sizeof(external_address));
			// external_address.sin_family = AF_INET;
			// external_address.sin_addr.s_addr = inet_addr("8.8.8.8");
			// external_address.sin_port = htons(53);
			// // Connect to the public endpoint
			// int err = connect(udp_socket, (const struct sockaddr*)&external_address, sizeof(external_address));
			// if (err < 0) {
			// 	cerr << "Failed to connect" <<endl;
			// 	close(udp_socket);
			// 	return;
			// }
			// // Get the local endpoint/IP address of the socket
			// struct sockaddr_in local_address;
			// socklen_t addr_len = sizeof(local_address);
			// err = getsockname(udp_socket, (struct sockaddr*)&local_address, &addr_len);
			// if (err < 0) {
			// 	cerr << "Failed : local address" << endl;
			// 	close(udp_socket);
			// 	return;
			// }
			// // Convert the IP to a string and close the socket
			// if (inet_ntop(AF_INET, &local_address.sin_addr, ip_buffer, sizeof(ip_buffer)) == NULL) {
			// 	cerr << "Failed to convert IP address" << endl;
			// 	close(udp_socket);
			// 	return;  
			// }
			// close(udp_socket);
			char command_str[] = "IP";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("IP:%s\n", server_ip_address);
			cse4589_print_and_log("[%s:END]\n", command_str);
			return;
		}

		void print_connected_clients(){
			//sort clients data based on port number in ascending order
			std::sort(clients.begin(), clients.end(), [](const client_details& c1, const client_details& c2) {
        	return c1.port_num < c2.port_num;
			});
			for (int i=0; i<clients.size(); i++) {
				cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i+1, clients[i].host_name.c_str(), clients[i].ip.c_str(), clients[i].port_num);
			}
		}
};

class Client{
		int client_port_num, fd_max;
		struct sockaddr_in client_address;
		int client_socket;
		bool isLoggedIn = false;  //login state of a client
		socklen_t client_address_Len = sizeof(client_address);
		char ip_buffer[INET_ADDRSTRLEN];
		vector<client_details> clients;
		char *ip_addr;
	public:
		Client(int port_num){
			//cout << "Inside client constructor"<<endl;
			client_port_num = port_num;
			client_socket = -1;
			struct hostent *host_entry;
			char hostname[1024];
			if (gethostname(hostname, sizeof(hostname)) == -1){
				//cout << "error"<<endl;
				cerr << "Cannot retrieve host name: " << hostname << endl;
			}
			if ((host_entry = gethostbyname(hostname)) == NULL){
				//cout <<"error"<<endl;
				cerr << "Cannot retrieve host entry" << endl;
		    }
			ip_addr = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
			// cout << "Host Name: " << hostname << " IP Address: " << ip_addr << endl;
			char command[300];
			fd_set readfds;
			while(true){
				FD_ZERO(&readfds);
				FD_SET(STDIN_FILENO, &readfds);
				if(client_socket >= 0){
					FD_SET(client_socket, &readfds); // client_socket is your connected socket
					fd_max = (STDIN_FILENO > client_socket) ? STDIN_FILENO : client_socket;
				}else fd_max = STDIN_FILENO;

				if (select(fd_max + 1, &readfds, NULL, NULL, NULL) == -1) {
					exit(4);
				}
				//handle input operations
				if(FD_ISSET(STDIN_FILENO, &readfds)){
					bzero(&command, sizeof(command));
					read(STDIN_FILENO, command, sizeof(command) - 1); 
					command[strlen(command)-1]='\0';
					process_commands(command);
				}
				if(client_socket >= 0){
					if(FD_ISSET(client_socket, &readfds)){
						receive_messages_from_server();
					}
				}
			}
		}

		void process_commands(string command_str){
			stringstream ss(command_str);
			string command;
			vector<string> tokens;

			// Tokenize the input
			while (ss >> command) {
				tokens.push_back(command);
			}

			if (tokens.empty()) return;

			if(tokens[0] == "EXIT"){
				exit_request();
				exit(0);
			}else if(tokens[0] == "IP"){
				print_client_ip();
			}else if(tokens[0] == "PORT"){
				print_client_port();
			}else if(tokens[0] == "AUTHOR"){
				print_client_author();
			}else if(tokens[0] == "REFRESH"){
				refresh();
			}else if(tokens[0] == "LIST"){ 
				//	cout << "Inside list command"<<endl;
				print_client_list();
			}else if(tokens[0] == "LOGIN"){
				try{
					if(isLoggedIn || tokens.size() < 3) throw invalid_argument("Missing server ip or port num");
					if(!is_valid_ip_address(tokens[1])){
						//throw exception when IP address is in invalid format
						throw invalid_argument("Invalid IP address");
					}
					if(!is_valid_port(tokens[2])){
						//throw exception when port number is invalid
						throw invalid_argument("Invalid port number");
					}
					//send login request to the server
					//cout << "before calling login method"<<endl;
					login(tokens[1], atoi(tokens[2].c_str()));
					//cout << "after calling login method"<<endl;
				}catch(const invalid_argument& exception){
					cse4589_print_and_log("[%s:ERROR]\n", tokens[0].c_str());
					cse4589_print_and_log("[%s:END]\n", tokens[0].c_str());
				}
			}else if(tokens[0] == "SEND"){
				std::string message;
				for(size_t i = 2; i < tokens.size(); ++i) {
					if(i != 2) message += " "; 
					message += tokens[i];
				}
				try{
					if(!is_valid_ip_address(tokens[1])){
						//throw exception when IP address is in invalid format
						throw invalid_argument("Invalid IP address");
					}
					//send message to the corresponding client
					send_message(tokens[1], message);
					
				}catch(const invalid_argument& exception){
					cse4589_print_and_log("[%s:ERROR]\n",tokens[0].c_str());
					cse4589_print_and_log("[%s:END]\n", tokens[0].c_str());
				}
				//cout << "Message to send: "<< message << endl;
			}else if(tokens[0] == "LOGOUT"){
				//cout << "inside command client logout"<<endl;
				logout();
			}else if((tokens[0] == "BLOCK") || (tokens[0] == "UNBLOCK")){
				//cout << "command is: "<<tokens[0]<<endl;
				try{
					if(tokens.size() < 2) throw invalid_argument("Missing ip address to block");
					if(!is_valid_ip_address(tokens[1])){
					//throw exception when IP address is in invalid format
					throw invalid_argument("Invalid IP address");
					}
					//cout << "Given ip: "<<block_client_ip<<endl;
					block_or_unblock_client(tokens[1], tokens[0].c_str());
				}catch(const invalid_argument& exception){
					cse4589_print_and_log("[%s:ERROR]\n", tokens[0].c_str());
					cse4589_print_and_log("[%s:END]\n", tokens[0].c_str());
				}
			}
		}

		void receive_messages_from_server(){
		//	cout << "Inside receive messages from server"<<endl;
			
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(client_socket, &readfds);

			// Set timeout to 0, making select non-blocking
			struct timeval tv;
			tv.tv_sec = 0;  // 0 seconds
			tv.tv_usec = 0; // 0 microseconds

			if (select(client_socket + 1, &readfds, NULL, NULL, &tv) < 0) {
				cerr << "select error" << endl;
				return;
			}

			if (FD_ISSET(client_socket, &readfds)) {
				// Data is available to read
				uint32_t data_length;
				// Make sure to check recv return value for errors or closed connection
				if (recv(client_socket, &data_length, sizeof(data_length), 0) > 0) {
					data_length = ntohl(data_length);
					char data[1024];
					memset(data, 0, sizeof(data));
					if (recv(client_socket, data, data_length, 0) > 0) {
						char* token =  strtok(data, " "); 
						char command_str[100];
						char content[1024];
						if (token != nullptr) {		
							strcpy(command_str, token);
							token = strtok(nullptr, ""); 
							if (token != nullptr) {
								strcpy(content, token);
							}
						}
						//cout << "Command is: "<<command_str<<endl;
						//cout << "Content is: "<<content <<endl;
						if(strcmp(command_str, "REFRESH") == 0){
							clients = deserialize_clients_data(content);
						}else if(strcmp(command_str, "LOGIN") == 0){
							//cout << "Inside receive msgs from server"<<endl;
							clients = deserialize_clients_data(content);
						}else if(strcmp(command_str, "SEND") == 0){
							//cout << "Inside send response"<<endl;
							//cout << "message is: "<<content<<endl;
                            token = strtok(content, " ");
                            if(token != nullptr){
                                char source_ip[INET_ADDRSTRLEN];
                                strcpy(source_ip, token);
                                token = strtok(nullptr, "");
                                if(token != nullptr){
                                    char message[300];
									strcpy(message, token);
									char command[] = "RECEIVED";
									cse4589_print_and_log("[%s:SUCCESS]\n", command);
                                    cse4589_print_and_log("msg from:%s\n[msg]:%s\n", source_ip, message);
									cse4589_print_and_log("[%s:END]\n", command);
                                }
                            }
						}else if((strcmp(command_str, "BLOCK") == 0) || (strcmp(command_str, "UNBLOCK") == 0)){
							//cout <<"Content: "<<content<<endl;;
							if(strcmp(content, "ERROR") == 0){
								cse4589_print_and_log("[%s:ERROR]\n", command_str);
								cse4589_print_and_log("[%s:END]\n", command_str);
							}else{
								cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
								cse4589_print_and_log("[%s:END]\n", command_str);
							}
							return;
						}
					}
				}
			}
			return;
		}

		void login(string server_ip, int server_port){
			char command_str[] = "LOGIN";
			struct sockaddr_in server_addr;
			string ip_address;
			char hostname[1024];
			gethostname(hostname, 1024);
			struct hostent *ht;
			(ht = gethostbyname(hostname));
			if ( ht == NULL)
			{
				//hostname error
			}
			struct in_addr **addr_list = (struct in_addr **)ht->h_addr_list;
			for (int i = 0; addr_list[i] != NULL; ++i)
			{
				ip_address = inet_ntoa(*addr_list[i]);
			}
			// Create socket
			client_socket = socket(AF_INET, SOCK_STREAM, 0);
			if (client_socket == -1) {
				cerr << "Error creating socket" << endl;
				return;
			}
			// Bind the socket to an address and port
			memset(&client_address, 0, sizeof(client_address));
			client_address.sin_family = AF_INET;
			//client_address.sin_addr.s_addr = INADDR_ANY;
			client_address.sin_addr = *((struct in_addr *)ht->h_addr);
			client_address.sin_port = htons(client_port_num);
			if (bind(client_socket, (struct sockaddr *)&client_address, sizeof(client_address)) < 0)
			{
				close(client_socket);
			}

			// Set up server address structure
			memset(&server_addr, 0, sizeof(server_addr));
			server_addr.sin_family = AF_INET;
			server_addr.sin_addr.s_addr = INADDR_ANY;
			server_addr.sin_port = htons(server_port);

			//Convert IPv4 addresses from text to binary form
			if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
				cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
				close(client_socket);
				return;
			}
			// Connect to server
			int res = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
			if (res < 0) {
				//cout << "Error in connecting with server" << endl;
				cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
				close(client_socket);
				return;
			}
			string clients_in_server = "";
			int result = send(client_socket, "LOGIN", strlen("LOGIN") + 1, 0);
			if(result < 0){
				 close(client_socket);
				 //isLoggedIn = false;
				 return;
			}		
			isLoggedIn = true;
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("[%s:END]\n", command_str);
			return;
		}

		void print_client_author(){
			char command_str[] = "AUTHOR";
			char MY_TEAM_NAME[] = "hemaboka-srachako";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", MY_TEAM_NAME);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}

		void print_client_port(){
			char command_str[] = "PORT";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("PORT:%d\n", client_port_num);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}

		void print_client_ip(){
			// int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    		// if (udp_socket < 0) {
        	// 	std::cerr << "Failed to create socket" << std::endl;
    		// }
			// struct sockaddr_in external_address;
			// memset(&external_address, 0, sizeof(external_address));
			// external_address.sin_family = AF_INET;
			// external_address.sin_addr.s_addr = inet_addr("8.8.8.8");  //Google's IP
			// external_address.sin_port = htons(53);
			// // Connect to the public endpoint
			// int err = connect(udp_socket, (const struct sockaddr*)&external_address, sizeof(external_address));
			// if (err < 0) {
			// 	std::cerr << "Failed to connect" << std::endl;
			// 	close(udp_socket);
			// 	return;
			// }
			// // Get the local IP address of the socket
			// struct sockaddr_in local_address;
			// socklen_t addr_len = sizeof(local_address);
			// err = getsockname(udp_socket, (struct sockaddr*)&local_address, &addr_len);
			// if (err < 0) {
			// 	cerr << "Failed : local address" << endl;
			// 	close(udp_socket);
			// 	return;
			// }
			// // Convert the IP to a string and close the socket
			// if (inet_ntop(AF_INET, &local_address.sin_addr, ip_buffer, sizeof(ip_buffer)) == NULL) {
			// 	cerr << "Failed to convert IP address" << endl;
			// 	close(udp_socket);
			// 	return;
			// }
			char command_str[] = "IP";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("IP:%s\n", ip_addr);
			cse4589_print_and_log("[%s:END]\n", command_str);
			//close(udp_socket);
			return;
		}

		void print_client_list(){
			//cout << "Inside client list"<<endl;
			char command_str[] = "LIST";
			if(!isLoggedIn){
				cse4589_print_and_log("[%s:ERROR]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
				return;
			}
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			print_soted_client_list();
			cse4589_print_and_log("[%s:END]\n", command_str);
		}
		void print_soted_client_list(){
			std::sort(clients.begin(), clients.end(), [](const client_details& c1, const client_details& c2) {
        	return c1.port_num < c2.port_num;
			});
			for (int i=0; i<clients.size(); i++) {
				cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i+1, clients[i].host_name.c_str(), clients[i].ip.c_str(), clients[i].port_num);
			}
		}

		vector<client_details> deserialize_clients_data(const string& clients_data){
			vector<client_details> clients;
			stringstream data_stream(clients_data);
			string data;
			while (getline(data_stream, data, ';')) {
				if (data.empty()) continue; // skip empty records
				stringstream recordStream(data);
				string field;
				vector<string> fields;
				while (getline(recordStream, field, ',')) {
					fields.push_back(field);
				}
				if (fields.size() == 5) { //ensure there are exactly 5 fields, Because client_details have only 5 fields
					client_details client;
					client.ip = fields[0];
					client.port_num = stoi(fields[1]);
					client.host_name = fields[2];
					client.login_state = (fields[3] == "true");
					client.socket_fd = stoi(fields[4]);

					clients.push_back(client);
				}
			}
    		return clients;
		}

		void refresh(){
			char command_str[] = "REFRESH";
			if(!isLoggedIn){
				cse4589_print_and_log("[%s:ERROR]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
				return;
			}
			int result = send(client_socket, "REFRESH", strlen("REFRESH") + 1, 0);
			if(result < 0){
				 //close(client_socket);
				 return;
			}
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}
		
		void exit_request(){
			if(client_socket >= 0){
				const char* exit_msg = "EXIT";
				send(client_socket, exit_msg, strlen(exit_msg), 0);
			} 
			char command_str[] = "EXIT";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}

		//send message to the client
		void send_message(string destination_ip, string message){
			char command_str[] = "SEND";
			if(!isLoggedIn){
				cse4589_print_and_log("[%s:ERROR]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
				return;
			}
			//check if the destination ip address is there in clients list, if not send error message
			bool isDestinationIpExist = check_ip_exists(destination_ip);
            if(!isDestinationIpExist){
                cse4589_print_and_log("[%s:ERROR]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
                return;
            }
			//send message to the server
            string message_to_send = "SEND " + string(ip_buffer) + " " + destination_ip + " " + message;
			//cout << "Message sending to server: "<<message_to_send<<endl;
            int result = send(client_socket, message_to_send.c_str(), message_to_send.length(), 0);
			//cout << "After sending message to server"<<endl;
            if(result < 0){
                cse4589_print_and_log("[%s:ERROR]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
                return;
            }
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}

		//clients wants to logout
		void logout(){
			//cout << "Inside logout method" <<endl;
			char command_str[100] = "LOGOUT";
			if(!isLoggedIn){
				cse4589_print_and_log("[%s:ERROR]\n", command_str);
				cse4589_print_and_log("[%s:END]\n", command_str);
				return;
			}
			string data_to_transfer = "LOGOUT";
			uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
			send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
			send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); // Then send the data
			// close(client_socket);
			// client_socket = -1;
			isLoggedIn = false;
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("[%s:END]\n", command_str);
		}
		
		void block_or_unblock_client(string block_client_ip, const char *command){
			if(!isLoggedIn){
				cse4589_print_and_log("[%s:ERROR]\n", command);
				cse4589_print_and_log("[%s:END]\n", command);
				return;
			}
			bool isIpExist = check_ip_exists(block_client_ip);
            if(!isIpExist){
                cse4589_print_and_log("[%s:ERROR]\n", command);
				cse4589_print_and_log("[%s:END]\n", command);
                return;
            }
			string message_to_send = string(command) + " "+ block_client_ip;
            send(client_socket, message_to_send.c_str(), message_to_send.length(), 0);
		}

		bool check_ip_exists(string ip){
			for(auto &client : clients){
                if(client.ip == ip){
                    return true;
                }
            }
			return false;
		}

};

int main(int argc, char** argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/* Clear LOGFILE*/
    fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	if(argc != 3){
		cout << "Invalid input! Please enter the valid input" << endl;
	}
	else{
		int port_num = atoi(argv[2]);
		if(strcmp(argv[1], "s") == 0){
			Server server(port_num); 
		}
		else if(strcmp(argv[1], "c") == 0){
			Client c(port_num);
		}
		else{
			cout << "Invalid input! Please enter the valid input" << endl;
		}
	}
	return 0;
}

bool is_valid_ip_address(const string& ip_address) {
	//cout << "Inside ip validation: " << ip_address << endl;
    vector<char> address(ip_address.begin(), ip_address.end());
    address.push_back('\0'); // Ensure null-termination
     int num, dots = 0;
    char* ptr;
    ptr = strtok(address.data(), ".");
    if (ptr == nullptr) {
        return false;
    }
    while (ptr) {
        if (!isdigit(*ptr)) return false; // Ensure all characters are digits
        num = atoi(ptr); // Convert segment to integer
        if (num >= 0 && num <= 255) {
            ptr = strtok(nullptr, ".");
            if (ptr != nullptr) dots++; // Count the dots
        } else {
            return false; // Segment is not a valid number
        }
    }
    if (dots != 3) { // IPv4 address must have three dots (which means 4 segments)
        return false;
    }
    return true;
}


bool is_valid_port(const string& port) {
    if (port.empty()) {
        return false;
    }
    char *endptr;
    long port_num = strtol(port.c_str(), &endptr, 10);
	//check if port is not in valid format
    if (*endptr != '\0' || port_num < 0 || port_num > 65535) {
        return false;
    }

    // Check if the port string starts with a non-digit character
    if (!isdigit(port[0])) {
        return false;
    }

    return true;
}