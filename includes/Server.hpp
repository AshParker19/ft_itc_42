/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: astein <astein@student.42lisboa.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/01 22:55:18 by astein            #+#    #+#             */
/*   Updated: 2024/05/02 16:02:33 by astein           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>      // For input and output
#include <exception>     // Standard exceptions
#include <list>          // For list container
#include <string>        // For string operations
#include <sstream>       // For string streams
#include <cstdlib>       // General purpose utilities: memory management, program utilities, string conversions, random numbers, etc.
#include <cstring>       // For string manipulation and memory functions
#include <cstdio>        // For C-style input-output like perror
#include <fcntl.h>       // For file control options
#include <unistd.h>      // Provides access to the POSIX operating system API
#include <sys/socket.h>  // For socket operations
#include <netinet/in.h>  // For internet addresses
#include <arpa/inet.h>   // For internet operations
#include <sys/select.h>  // For select function
#include <signal.h>      // For signal handling
#include <errno.h>
#include <poll.h>

#include "utils.hpp"
#include "Client.hpp"
#include "Channel.hpp"

// Standart exception class for Server
class ServerException : public std::exception
{
	public:
		ServerException(const std::string &message);
		~ServerException() 			throw();
		const char	*what() const	throw();

	private:
		std::string _message;
};

class Server
{
    public:
        Server(const std::string &port, const std::string &password)
			throw(ServerException);
        ~Server();

		// Public Member Functions
        void                    initNetwork()
			throw(ServerException);
        void                    goOnline();
        void                    shutDown();

		// Signal handling for exit
    	static void 					setupSignalHandling();
    	static void 					sigIntHandler(int sig);

    private:
        Server(); // No default constructor
        
		// Private Member Functions
        void                    parseArgs(const std::string &port, const std::string &password)
			throw(ServerException);
        // void                    addClient(Client *client);
        // void                    removeClient(Client *client);
        // void                    addChannel(Channel &channel);
        // void                    removeChannel(Channel *channel);
		void 					dealWithChannelMsg(Client *client, const std::string &ircMessage);

        void                    broadcastMessage(const std::string &message) const;
        
		// Attributes
		static volatile sig_atomic_t 	_keepRunning;
		int                     		_socket;
        u_int16_t		           		_port;
        std::string             		_password;
        std::list<Client>       		_clients;
        std::list<Channel>      		_channels;

		
};
	
#endif