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

#include "../include/global.h"
#include "../include/logger.h"

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

//to store all the required information related to the client
struct client_details{
	string ip;
	int port_num;
	string host_name;
	bool login_state;
	//below field is useful for sending data to the client 
	int socket_fd; //it will help to identify the client socket
};

bool is_valid_ip_address(char* address);   //checks if the given IP address is in valid format or not.
bool is_valid_port(char* port);  //checks if the given port is a valid number or not

class Server{
	public: 
		int server_port_num;
	    sockaddr_in server_address;
		int server_socket;
		vector<client_details> clients; //stores all information of connected clients
		char ip_buffer[INET_ADDRSTRLEN];

		Server(int port_num){
			server_port_num = port_num;
			int client_socket, activity, max_sd;
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
				std::cerr << "Failed to get local hostname." << endl;
				return; 
			}
			struct hostent *host_info = gethostbyname(server_hostname);
			if (host_info == nullptr) {
				std::cerr << "Failed to get host info for " << server_hostname << endl;
				return; 
			}
			string server_ip_address = inet_ntoa(*(struct in_addr *)host_info->h_addr_list[0]);
			// cout<<"server hostname: "<<hostname << endl;
			// cout << "server ip address: "<<server_ip_address<<endl;
			int addressLen = sizeof(server_address);
			sockaddr_in client_address;
            socklen_t client_address_size = sizeof(client_address);
			//memset(&client_address, 0, sizeof(client_address));
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
				//waiting for an activity on one of the sockets, timeout is NULL.
				activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
				if((activity < 0) && (errno!=EINTR)) {
					std::cout << "select error";
				}
				//Accepts incoming requests
				if(FD_ISSET(server_socket, &readfds)) {
					if((client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_size)) < 0){
						//error accepting incoming request
						exit(0);
					}
					char client_hostname[NI_MAXHOST];
					int res = getnameinfo((struct sockaddr*)&client_address, sizeof(client_address),client_hostname, NI_MAXHOST, NULL, 0, 0);
					if (res == 0) {
						//store client information
						client_details client;
						client.host_name = client_hostname;
						client.ip = inet_ntoa(client_address.sin_addr);
						client.port_num = ntohs(client_address.sin_port);
						client.socket_fd = client_socket;	
						//store in the vector. It will help retrive all connected clients to the given server
						clients.push_back(client);
					}
				}
				// Handle client messages
				for (auto &client : clients) {
					if (FD_ISSET(client.socket_fd, &readfds)) {
						if (!handleClientMessage(client.socket_fd)) {
							// If handleClientMessage returns false, it means the client disconnected or logged out or exited
							close(client.socket_fd); //close the client socket
							client.socket_fd = -1; // Mark as closed
						}
					}
				}
				//remove clients that are marked as closed.
				clients.erase(std::remove_if(clients.begin(), clients.end(), 
											[](const client_details& c) { return c.socket_fd == -1; }), clients.end());
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
						}
				}
			}
		}

		string serialize_clients_data(){
			stringstream ss;
			for (const auto& client : clients) {
				ss << client.ip << "," 
				<< client.port_num << ","
				<< client.host_name << ","
				<< (client.login_state ? "true" : "false") << ","
				<< (client.socket_fd) <<  ";";
			}
			//cout << "Serialized client info string: "<<ss.str()<<endl;
			return ss.str();
		}

		//It will handle 2 client messages, i.e., LOGOUT and SEND_CLIENTS_INFO
		//If clients sends "SEND_CLIENTS_INFO" message we are directly sending required information from this method
		//If client send "LOGOUT" message to the server, then we return boolean value. We will handle this case in server loop
		bool handleClientMessage(int client_socket) {
			char buffer[1024];
			memset(buffer, 0, sizeof(buffer));
			int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
			if (bytes_received <= 0) {
				//return false;
			}
			//cout << "Buffer message: "<<buffer<<endl;
			// Check if the message is a logout request
			if (strcmp(buffer, "LOGOUT") == 0) {
				return false; // Indicates that the client wants to log out
			}else if(strcmp(buffer, "SEND_CLIENTS_INFO") == 0){
				string data_to_transfer = serialize_clients_data();
				//cout << "After serialization data: "<<data_to_transfer<<endl;
				uint32_t data_length = htonl(data_to_transfer.length()); // Ensure network byte order
				send(client_socket, &data_length, sizeof(data_length), 0); // Send the length first
				send(client_socket, data_to_transfer.c_str(), data_to_transfer.length(), 0); // Then send the data
				return true;
			}
			return true; // Indicates that the client is still connected
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
			int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    		if (udp_socket < 0) {
        		std::cerr << "Failed to create socket" << std::endl;
    		}
			// Google's public DNS server address (8.8.8.8) 
			struct sockaddr_in external_address;
			memset(&external_address, 0, sizeof(external_address));
			external_address.sin_family = AF_INET;
			external_address.sin_addr.s_addr = inet_addr("8.8.8.8");
			external_address.sin_port = htons(53);
			// Connect to the public endpoint
			int err = connect(udp_socket, (const struct sockaddr*)&external_address, sizeof(external_address));
			if (err < 0) {
				cerr << "Failed to connect" <<endl;
				close(udp_socket);
				return;
			}
			// Get the local endpoint/IP address of the socket
			struct sockaddr_in local_address;
			socklen_t addr_len = sizeof(local_address);
			err = getsockname(udp_socket, (struct sockaddr*)&local_address, &addr_len);
			if (err < 0) {
				cerr << "Failed : local address" << endl;
				close(udp_socket);
				return;
			}
			// Convert the IP to a string and close the socket
			if (inet_ntop(AF_INET, &local_address.sin_addr, ip_buffer, sizeof(ip_buffer)) == NULL) {
				cerr << "Failed to convert IP address" << endl;
				close(udp_socket);
				return;  
			}
			close(udp_socket);
			char command_str[] = "IP";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("IP:%s\n", ip_buffer);
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
		int client_port_num;
		struct sockaddr_in client_address;
		int client_socket;
		bool isLoggedIn = false;  //login state of a client
		socklen_t client_address_Len = sizeof(client_address);
		char ip_buffer[INET_ADDRSTRLEN];
		vector<client_details> clients;
	public:
		Client(int port_num){
			client_port_num = port_num;
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
			int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    		if (udp_socket < 0) {
        		std::cerr << "Failed to create socket" << std::endl;
    		}
			struct sockaddr_in external_address;
			memset(&external_address, 0, sizeof(external_address));
			external_address.sin_family = AF_INET;
			external_address.sin_addr.s_addr = inet_addr("8.8.8.8");  //Google's IP
			external_address.sin_port = htons(53);
			// Connect to the public endpoint
			int err = connect(udp_socket, (const struct sockaddr*)&external_address, sizeof(external_address));
			if (err < 0) {
				std::cerr << "Failed to connect" << std::endl;
				close(udp_socket);
				return;
			}
			// Get the local IP address of the socket
			struct sockaddr_in local_address;
			socklen_t addr_len = sizeof(local_address);
			err = getsockname(udp_socket, (struct sockaddr*)&local_address, &addr_len);
			if (err < 0) {
				cerr << "Failed : local address" << endl;
				close(udp_socket);
				return;
			}
			// Convert the IP to a string and close the socket
			if (inet_ntop(AF_INET, &local_address.sin_addr, ip_buffer, sizeof(ip_buffer)) == NULL) {
				cerr << "Failed to convert IP address" << endl;
				close(udp_socket);
				return;
			}
			char command_str[] = "IP";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("IP:%s\n", ip_buffer);
			cse4589_print_and_log("[%s:END]\n", command_str);
			close(udp_socket);
			return;
		}

		void print_client_list(){
			char command_str[] = "LIST";
			if(!isLoggedIn){
				cse4589_print_and_log("[%s:ERROR]\n", command_str);
				return;
			}
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			print_soted_client_list();
		}
		void print_soted_client_list(){
			std::sort(clients.begin(), clients.end(), [](const client_details& c1, const client_details& c2) {
        	return c1.port_num < c2.port_num;
			});
			for (int i=0; i<clients.size(); i++) {
				cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i+1, clients[i].host_name.c_str(), clients[i].ip.c_str(), clients[i].port_num);
			}
		}

		void login(string server_ip, int server_port){
			struct sockaddr_in server_addr;
			string ip_address;
			char hostname[1024];
			if (gethostname(hostname, 1024) < 0)
			{
				// cout<<"gethostname error"<<endl;
			}
			struct hostent *ht;
			if ((ht = gethostbyname(hostname)) == NULL)
			{
				//cout<<"hostname error" << endl;
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
				cse4589_print_and_log("[LOGIN:ERROR]\n");
				close(client_socket);
				return;
			}
			// Connect to server
			int res = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
			if (res < 0) {
				//cout << "Error in connecting with server" << endl;
				cse4589_print_and_log("[LOGIN:ERROR]\n");
				close(client_socket);
				return;
			}
			string clients_in_server = "";
			int result = send(client_socket, "SEND_CLIENTS_INFO", strlen("SEND_CLIENTS_INFO") + 1, 0);
			if(result < 0){
				 close(client_socket);
				 //isLoggedIn = false;
				 return;
			}
			//receive data from server
			uint32_t data_length;
			//from the server we are sending data length first and then data, so read the data length first then read the data
			recv(client_socket, &data_length, sizeof(data_length), 0); // read the length first
			data_length = ntohl(data_length); 
			std::string data;
			data.clear(); 
			data = std::string(data_length, '\0');
			recv(client_socket, &data[0], data_length, 0); //read exactly data_length bytes
			//cout << "Data received from server: " << data << endl;
			clients = deserialize_clients_data(data);   //deserialize the clients data and store them in clients vector
			cse4589_print_and_log("[LOGIN:SUCCESS]\n");
			isLoggedIn = true;
			return;
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
				return;
			}
			int result = send(client_socket, "SEND_CLIENTS_INFO", strlen("SEND_CLIENTS_INFO") + 1, 0);
			if(result < 0){
				 //close(client_socket);
				 return;
			}
			uint32_t data_length;
			recv(client_socket, &data_length, sizeof(data_length), 0); //read the length first
			data_length = ntohl(data_length); 
			std::string data;
			data.clear(); 
			data = std::string(data_length, '\0');
			recv(client_socket, &data[0], data_length, 0); //read exactly data_length bytes
			//cout << "Data received from server: " << data << endl;
			clients = deserialize_clients_data(data);
			//cout << "After deserialization clients size: "<<clients.size()<<endl;
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			//print_sorted_client_list();
		}
		
		void exit(){
			if(client_socket >= 0){
				const char* logout_msg = "LOGOUT";
				send(client_socket, logout_msg, strlen(logout_msg), 0);
			} 
			char command_str[] = "EXIT";
			cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
			cse4589_print_and_log("[%s:END]\n", command_str);
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
			char command[300];
			while(true){
				bzero(&command, sizeof(command));
				read(STDIN_FILENO, command, sizeof(command) - 1); 
				command[strlen(command)-1]='\0';
				if(strcmp(command, "EXIT") == 0){
					c.exit();
					exit(0);
				}else if(strcmp(command, "IP") == 0){
					c.print_client_ip();
				}else if(strcmp(command, "PORT") == 0){
					c.print_client_port();
				}else if(strcmp(command, "AUTHOR") == 0){
					c.print_client_author();
				}else if(strcmp(command, "REFRESH") == 0){
					c.refresh();
					cse4589_print_and_log("[%s:END]\n", command);
				}else if(strcmp(command, "LIST") == 0){ 
					c.print_client_list();
					cse4589_print_and_log("[%s:END]\n", command);
				}else if(strncmp(command, "LOGIN", 5) == 0){
					char* token =  strtok(command, " "); 
					char command_str[100];
					char server_ip_address[100];
					char port[6];
					if (token != nullptr) {
						strcpy(command_str, token);
						token = strtok(nullptr, " "); 
						if (token != nullptr) {
							strcpy(server_ip_address, token);
							token = strtok(nullptr, " ");
							if (token != nullptr) {
								strcpy(port, token);
							}
						}
					}
					try{
						if(!is_valid_ip_address(server_ip_address)){
							//throw exception when IP address is in invalid format
							throw invalid_argument("Invalid IP address");
						}
						if(!is_valid_port(port)){
							//throw exception when port number is invalid
							throw invalid_argument("Invalid port number");
						}
						//send login request to the server
						c.login(server_ip_address, atoi(port));
						cse4589_print_and_log("[%s:END]\n", command);
					}catch(const invalid_argument& exception){
						cse4589_print_and_log("[%s:ERROR]\n", command_str);
						cse4589_print_and_log("[%s:END]\n", command_str);
					}
				}
			}
		}
		else{
			cout << "Invalid input! Please enter the valid input" << endl;
		}
	}
	return 0;
}

bool is_valid_ip_address(char* ip_address) {
	char* address = new char[strlen(ip_address) + 1];
	address = strcpy(address, ip_address);
    int num, dots = 0;
    char* ptr;
    if (address == nullptr) {
        return false;
    }
    ptr = strtok(address, ".");
    if (ptr == nullptr) {
        return false;
    }
    while (ptr) {
        // check if the current segment is a valid number
        if (!isdigit(*ptr)) return false; 
        num = atoi(ptr); // convert segment to integer
        if (num >= 0 && num <= 255) {
            // valid segment, move to the next one
            ptr = strtok(nullptr, ".");
            if (ptr != nullptr) dots++; // count the dots
        } else {
            // segment is not a valid number
            return false;
        }
    }
    //IPv4 address must have three dots which means 4 segments
    if (dots != 3) {
        return false;
    }
	address = nullptr;
    return true;
}

bool is_valid_port(char *port) {
    if (port == nullptr || *port == '\0') {
        return false;
    }
    char *endptr;
    long port_num = strtol(port, &endptr, 10); 
    // check for conversion errors and validate the range
    if (*endptr != '\0' || port_num < 0 || port_num > 65535) {
        return false; 
    }
    // check if the port string starts with a non-digit character
    if (!isdigit(*port)) {
        return false;
    }
    return true;
}
