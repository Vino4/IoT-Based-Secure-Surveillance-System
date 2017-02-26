/* 
IBSSS Client Handler 

This file defines the ibsss client handler class and functionlity

Authors:
Matt Almenshad | Andrew Gao | Jenny Horn 
*/
#include "ibsss_client_handler.hh"
#include "ibsss_op_codes.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <vector>
#include <thread>
#include <sqlite3.h> 
#include <ibsss_database.h> 
#include <ibsss_session_token.h> 
#include <ibsss_mutex.hh>
#include <algorithm>
#include <ibsss_error.hh>

/*
Client_Handle::killSession()
**Thread Safe**
- Closes TCP connection
- Removes connection from connection manager
- Deletes client handle from memory
Arguments:
	none
Returns:
	none
*/
void Client_Handle::killSession(){
	
	IBSSS_KILL_SESSION_MUTEX.lock();

	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS)
		std::cout 
		<< "\n\n{[=-....Terminating Session....-=]}\n        	Target: [" << getSessionToken() << "]" 
		<< std::endl;	

	close(client_descriptor);
	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS)
		std::cout << "Closed Connection: [" << getSessionToken() << "]" << std::endl;	

	connections->erase(std::remove(connections->begin(),connections->end(),this));
	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS)
		std::cout << "Removed [" << getSessionToken() << "] from connection manager" << std::endl;	

	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS){
		std::cout << "Clients Online: ";
		for(std::vector<Client_Handle*>::iterator it = connections->begin(); it != connections->end(); ++it) {
			std::cout << "[" << (*it)->getSessionToken() << "]";
		}	
		std::cout << std::endl;
	}

	delete this;	
	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS)
		std::cout << "Cleared session memorey" << std::endl;	
	IBSSS_KILL_SESSION_MUTEX.unlock();
}

/*
Client_Handle::processConnection()

Reads messages from the clients and responds by calling the appropriate handler functions.
Functions may respond by doing the following:
	- Processing data (possibly from the ibsss database)
	- Sending back responses
	- Managing session states (possibly other sessions)
	
Arguments:
	none
Returns:
	none
*/
void Client_Handle::processConnection(Client_Handle * client_handle){
	
	int client_descriptor = client_handle->getClientDescriptor();
	unsigned char message_buffer[256];
	unsigned int length = 0;
	int read_status, write_status;
	for (;;){	
		if ((read_status = read(client_descriptor, &length, 4)) < 0)
	            ibsssError("failed to read");
		
		if ((read_status = read(client_descriptor, message_buffer, length)) < 0)
	            ibsssError("failed to read");
		if (IBSSS_TRACE_READ_WRITE_STATUS)
			std::cout << "Read Status: " << read_status << std::endl;
		
		if (read_status == 0){
			break;
		}
	
		message_buffer[length] = '\0';		
	
		std::cout << "[" << client_handle->getSessionToken() << "]["
		<< length << "]: "
		<< message_buffer
		<< std::endl;
	
		if ((write_status = write(client_descriptor, "Fuck You!", 9)) < 0)
	            ibsssError("failed to write");

		if (IBSSS_TRACE_READ_WRITE_STATUS)
			std::cout << "Write Status: " << write_status << std::endl;
		
		if (write_status == 0){
			break;
		}
	}
	client_handle->killSession();
}

/*
Client_Handle::getSessionToken()
	
Arguments:
	none
Returns:
	string session token with IBSSS_SESSION_TOKEN_LENGTH length
*/
std::string Client_Handle::getSessionToken(){
	return this->session_token;
}

/*
Client_Handle::getThreadHandle()
	
Arguments:
	none
Returns:
	thread pointer to the thread running the connection processing function	
*/
std::thread * Client_Handle::getThreadHandle(){
	return thread_handle;
}

/*
Client_Handle::setClientDescriptor()
      
Sets client descriptor.
The descritor will be used to read and write messages to the client through a TCP socket.

Arguments:
     int client descriptor 
Returns:
	none
*/
void Client_Handle::setClientDescriptor(int descriptor){
	client_descriptor = descriptor;
}

/*
Client_Handle::getClientDescriptor()
	
Returns client decritpor which can be used to read to, write from and manage a TCP socket.

Arguments:
	none
Returns:
	int client descriptor	
*/
int Client_Handle::getClientDescriptor(){
	return client_descriptor;
}

/*
Client_Handle::Client_Handle()

Initializes a client handle by:
	- Setting all strings to '\0'
	- Setting all status, descriptor and other integeral variables to -1 or 0 where needed

Arguments:
	none
Returns:
	none
*/
Client_Handle::Client_Handle(){
	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS)
		std::cout << "\n\n{[=-....Creating a new session....-=]}" << std::endl;
	session_token = "\0";
	username = "\0";
	ID = "\0";
	client_descriptor = -1;
	session_status = -1;
}

/*
Client_Handle::generateToken()

Creates a randomized string from the characters defined in char IBSSS_SESSION_TOKEN_ALLOWED_CHARACTERS[]
The maximum index for the array must be defined as IBSSS_SESSION_TOKEN_CHARACTER_POOL_SIZE
The token size length must be globally defined as IBSSS_SESSION_TOKEN_LENGTH

Arguments:
	none
Returns:
	none
*/
void Client_Handle::generateToken(){

	session_token.resize(IBSSS_SESSION_TOKEN_LENGTH+1, '\0');
	
	for(int i = 0; i < IBSSS_SESSION_TOKEN_LENGTH; i++)
		session_token[i] = IBSSS_SESSION_TOKEN_ALLOWED_CHARACTERS[rand()%IBSSS_SESSION_TOKEN_CHARACTER_POOL_SIZE];

	if (IBSSS_DEBUG_MESSAGES && IBSSS_TRACE_SESSIONS)
		std::cout << "Created new session token: " << session_token << std::endl;

}

/*
Client_Handle::initClientSession()

Initialize the client session by:
	- Setting the appropriate session variables
	- Generating session token
	- Starting session thread

Arguments:
	- std::vector<Client_Handle*> * connection_manager, a pointer to a vector referencing all client handles
	- int descriptor, socket descriptor to be used for writing to, reading from and managing a connection
Returns:
	none
*/
void Client_Handle::initClientSession(std::vector<Client_Handle*> * connections_reference, int descriptor){
	connections = connections_reference;
	client_descriptor = descriptor;
	generateToken();
	thread_handle = new std::thread(&Client_Handle::processConnection, this, this);
}
