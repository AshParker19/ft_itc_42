/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anshovah <anshovah@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/01 22:55:11 by astein            #+#    #+#             */
/*   Updated: 2024/05/04 06:39:35 by anshovah         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

// -----------------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------------
Server::Server(const std::string &port, const std::string &password) : _serverIP("localhost")
{
	// Initialize the list of allowed cmds
	_cmds["NICK"] = &Server::nick;
    _cmds["USER"] = &Server::user;
    _cmds["WHOIS"] = &Server::whois;
    _cmds["PRIVMSG"] = &Server::privmsg;
    _cmds["JOIN"] = &Server::join;
    _cmds["INVITE"] = &Server::invite;
    _cmds["TOPIC"] = &Server::topic;
    _cmds["MODE"] = &Server::mode;
    _cmds["KICK"] = &Server::kick;
    _cmds["PART"] = &Server::part;
	parseArgs(port, password);
}

Server::~Server()
{
	// TODO: Close all sockets
}

void Server::parseArgs(const std::string &port, const std::string &password)
{
	u_int16_t	portInt;
	std::istringstream iss(port);

	if (!(iss >> portInt))
    	throw ServerException("Invalid port number\n\t>>Port is not the IRC port (194) or in the range 1024-65535!");

	// 194 is the default port for IRC
	// https://en.wikipedia.org/wiki/Port_(computer_networking)
	// The ports up to 49151 are not as strictly controlled
	// The ports from 49152 to 65535 are called dynamic ports
	if (portInt != 194 && (portInt < 1024 || portInt > 65535))
		throw ServerException("Port is not the IRC port (194) or in the range 1024-65535!");
	_port = portInt;
	info ("port accepted: " + port, CLR_YLW);
	// TODO: Check if password is valid
	_password = password;
}

// -----------------------------------------------------------------------------
// Server Methods
// -----------------------------------------------------------------------------
void	Server::initNetwork()
{
	info("[START] Init network", CLR_YLW);

    // Create a master socket for the server
	// AF_INET:			to use IPv4 (AF = Adress Family) (AF_INET6 for IPv6)
	// SOCK_STREAM:		TCP two way connection using a stream of bytes
	// 0:				Protocol (0 = default aka TCP/IP)
	// https://pubs.opengroup.org/onlinepubs/009604499/functions/socket.html
    if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		throw ServerException("Socket creation failed:\n\t" +	std::string(strerror(errno)));

    
	// TODO: REVISE THIS
	// here we futher configure the socket
    // SOL_SOCKET:		Use SOL_SOCKET for general settings
	// SO_REUSEADDR:	Allow the socket to be reused immediately after it is closed
	// opt:				Option value set to 1; needs to be a void pointer
    int opt = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void*>(&opt), sizeof(opt)) < 0)
		throw ServerException("Setsockopt failed\n\t" +	std::string(strerror(errno)));

    // Specify the type of socket created
	// AF_INET:			to use IPv4 (AF = Adress Family) (AF_INET6 for IPv6)
	// INADDR_ANY:		accept any incoming messages from all interfaces of the host machine
	// htons(_port):	converts the port number to network byte order
    _address.sin_family 		= AF_INET;
    _address.sin_addr.s_addr	= INADDR_ANY;
    _address.sin_port 			= htons(_port);

	// TODO: SET THE PASSWORD

	// Use fcntl to set the socket to non-blocking
	// https://pubs.opengroup.org/onlinepubs/009695399/functions/fcntl.html
	if (fcntl(_socket, F_SETFL, O_NONBLOCK) < 0)
		throw ServerException("Fcntl failed\n\t" +	std::string(strerror(errno)));

	// Binds the socket to the previously defined address
	// https://pubs.opengroup.org/onlinepubs/009695399/functions/bind.html
    if (bind(_socket, (struct sockaddr *)&_address, sizeof(_address)) < 0)
		throw ServerException("Bind failed\n\t" +	std::string(strerror(errno)));

	// Listen for incoming connections
	// 3: The maximum length to which the queue of pending connections for sockfd may grow
	// https://pubs.opengroup.org/onlinepubs/009695399/functions/listen.html
	if (listen(_socket, 3) < 0)
		throw ServerException("Listen failed\n\t" +	std::string(strerror(errno)));

	info("[>DONE] Init network", CLR_GRN);
	info("Server IP: ", CLR_GRN);
	system("hostname -I | awk '{print $1}'");
}

void	Server::goOnline()
{
	info("[START] Go online", CLR_GRN);
	std::vector<pollfd> fds;
	while (_keepRunning)
	{
		fds = getFdsAsVector();
		info ("Waiting for connections ...", CLR_ORN);
		int pollReturn = poll(fds.data(), fds.size() , -1);
		info ("DONE Waiting for connections ...", CLR_ORN);
		if (pollReturn == -1)
		{
			if (!_keepRunning)
			{
				shutDown();
				return ;
			}
			throw ServerException("Poll failed\n\t" + std::string(strerror(errno)));
		}
		if (pollReturn == 0)
			continue;

		// Check for new connections
        if (fds[0].revents & POLLIN)
		{
			// https://pubs.opengroup.org/onlinepubs/009695399/functions/accept.html
			int	addrlen = sizeof(_address);
			int new_socket = accept(fds[0].fd, (struct sockaddr *)&_address, (socklen_t*)&addrlen);
            if (new_socket < 0)
				throw ServerException("Accept failed\n\t" + std::string(strerror(errno)));
			// Use fcntl to set the socket to non-blocking
			// https://pubs.opengroup.org/onlinepubs/009695399/functions/fcntl.html
			if (fcntl(new_socket, F_SETFL, O_NONBLOCK) < 0)
				throw ServerException("Fcntl failed\n\t" +	std::string(strerror(errno)));
			addClient(Client(new_socket));
        }

		// TODO: in client check for to long msgs
		// Read from clients
		char buffer[BUFFER_SIZE+1];	// +1 for the null terminator
		Client *cur_client;
		for (size_t i = 1; i < fds.size(); ++i)
		{
            if (fds[i].revents & POLLIN)
			{
				cur_client = getClientByFd(fds[i].fd);
				if (!cur_client)
					continue ; // should never happen by arcitechture
					
                int valread = read(fds[i].fd, buffer, BUFFER_SIZE);
                if (valread <= 0)
				{
					// Some read error happend
					// The server doesn't bother to much and just deletes this client
					info("Client" + cur_client->getUniqueName() + " disconnected", CLR_RED);
					Logger::log("Client" + cur_client->getUniqueName() + " disconnected");
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);		// erase the client fd from the fd vector
					removeClient(cur_client);		// erase the client from the client lisr
                    --i; // Adjust loop counter since we removed an element
                }
				else
				{
                    buffer[valread] = '\0';
					// Since the buffer could only be a part of a message we
					// 1. append it to the client buffer
					if (!cur_client->appendBuffer(buffer))
					{
						cur_client->sendMessage("Message was to long and will be deleted");
						// The message was to long
						// The full messages will be deleted and the client will be informed
					}
					// 2. get the full message(s) from the client buffer
					std::string fullMsg;
					while (!(fullMsg = cur_client->getFullMessage()).empty())
					{
						// 3. process the message(s)
						Logger::log("start processing message from " + cur_client->getUniqueName() + " -> " + fullMsg);
						processMessage(*cur_client, fullMsg);
					}
                }
            }
		}
	}	
	info("[>DONE] Go online", CLR_YLW);
}

void	Server::shutDown()
{
	info("[START] Shut down", CLR_YLW);
	broadcastMessage("!!!Server shutting down in 42 seconds!!!\r\n");
	sleep(1);
	// TODO: SLEEP is C! Use C++ sleep_for
	info("[>DONE] Shut down", CLR_GRN);
}

std::vector<pollfd>	Server::getFdsAsVector() const
{
	std::vector<pollfd> fds;
	pollfd fd;
	fd.fd = _socket;
	// POLLIN: There is data to read
	fd.events = POLLIN;
	fds.push_back(fd);
	for (std::list<Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		fd.fd = it->getSocketFd();
		fd.events = POLLIN;
		fds.push_back(fd);
	}
	if(fds.size() == 0)
		throw ServerException("No socket file descriptors found");
	return fds;
}

void	Server::broadcastMessage(const std::string &message) const
{
	info("[START] Broadcast message", CLR_YLW);
	std::string ircMessage = "TODO:!" + message + "TODO:!";
	
	// Send it to all clients
	for (std::list<Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		// TODO: Create the ircMessage in right format! aka adding the user's name
		it->sendMessage(ircMessage);
	}

	// Send it to all channels
	for (std::list<Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		it->sendMessage(ircMessage);
	}
	info("[>DONE] Broadcast message", CLR_GRN);
}

// -----------------------------------------------------------------------------
// Processing the Messages
// -----------------------------------------------------------------------------
void	Server::processMessage(Client &sender, const std::string &ircMessage)
{
	// Parse the IRC Message
	Message     msg(sender, ircMessage);
	
	//Execute IRC Message
	//	1. Check if CLIENT is loggedin
	if (!isLoggedIn(msg))
		return ;
	
	//	2. Execute normal commands
	//		3.1. Find the channel if there is  channelname in the msg
	msg.setChannel(getChannelByName(msg.getChannelName()));
	
	// 		3.2 Process the msg aka call the right function
	chooseCommand(msg);
}

bool	Server::isLoggedIn(Message &msg)
{
	//	1. Check if NICK is set
	if (msg.getSender().getUniqueName().empty())
	{
		if (msg.getCmd() == "NICK")
			nick(msg);
		else
			msg.getSender().sendMessage(ERR_NOTREGISTERED, ":You have not registered");
		return false;
	}

	//	2. Check if USER is set
	if (msg.getSender().getUsername().empty())
	{
		if (msg.getCmd() == "USER")
			user(msg);
		else
			msg.getSender().sendMessage(ERR_NOTREGISTERED, ":You have not registered");
		return false;
	}
	return true;
}

void	Server::chooseCommand(Message &msg)
{
	std::string cmd = msg.getCmd();
	if (cmd.empty())
		return ; // this should not happen by design

	for (std::map<std::string, CommandFunction>::iterator it = _cmds.begin(); it != _cmds.end(); ++it)
	{
		if (cmd == it->first)
		{
			(this->*(it->second))(msg);
			return ;
		}
	}
	// :10.11.3.6 421 anshovah_ PRIMSG :Unknown command
	msg.getSender().sendMessage(ERR_UNKNOWNCOMMAND, msg.getCmd() + " :Unknown command");
}

// /NICK
void	Server::nick(Message &message)
{	
	std::string oldNickname = message.getSender().getUniqueName();
	std::string newNickname = message.getArg(0);

	if (oldNickname.empty())
		oldNickname = newNickname;

	if (newNickname.empty())
		message.getSender().sendMessage(ERR_NONICKNAMEGIVEN, ":No nickname given");
	else if (!isNameAvailable(_clients, newNickname))
		message.getSender().sendMessage(ERR_NICKNAMEINUSE, newNickname + " :Nickname is already in use");
	else
	{
		message.getSender().setUniqueName(newNickname);
		std::string ircMessage = 
			":" + oldNickname + "!" +
			message.getSender().getUsername() +
			"@localhost NICK :" +
			newNickname;
		message.getSender().sendMessage(ircMessage);
	}
}

void	Server::user(Message &msg)
{
	std::string newUsername = msg.getArg(0);
	
	if (!msg.getArg(0).empty() && !msg.getArg(1).empty() &&
		!msg.getArg(2).empty() && !msg.getColon().empty())
	{
		msg.getSender().setUsername(msg.getArg(0));
		msg.getSender().setFullname(msg.getColon());
	}
	else
		msg.getSender().sendMessage(ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
}

void	Server::whois	(Message &message)
{
(void)message;
}


/* 
	receiver not found				ERR_NOSUCHNICK
		inform sender about it
	send message to receiver
 */
void	Server::privmsg(Message &message)
{
	// NO RECIPIENT
	std::string receiverNickname = message.getArg(0);

	if (receiverNickname.empty())
	{
		message.getSender().sendMessage(ERR_NORECIPIENT, ":No recipient given (PRIVMSG)");
		return ;
	}
	
	// NO TEXT TO SEND
	if (message.getColon().empty())
	{
		message.getSender().sendMessage(ERR_NOTEXTTOSEND, ":No text to send");
		return ;
	}

	// NO SUCH NICK 401
	message.setReceiver(getInstanceByName(_clients, receiverNickname));
	if (!message.getReceiver())
	{
		message.getSender().sendMessage(ERR_NOSUCHNICK, receiverNickname + " :No such nick/channel");
		return ;
	}
	
	// :anshovah_!anshovah@F456A.75198A.60D2B2.ADA236.IP PRIVMSG astein :joao is lazyao
	std::string ircMessage = 
		":" + message.getSender().getUniqueName() + "!" +
		message.getSender().getUsername() +
		"@localhost PRIVMSG " +
		receiverNickname +
		" :" +
		message.getColon();
	message.getReceiver()->sendMessage(ircMessage);
}

void	Server::join(Message &message)
{
	(void)message;
	// if (!channel)
	// 	channel = createNewChannel(message);
	// if (!channel)
	// {
	// 	info ("STRANGE CASE: COULD NOT FIND NOR CREATE CHANNEL", CLR_RED);
	// 	message.getSender().sendMessage("Could not create channel"); 
	// 	return ;
	// }
	// message.getSender().joinChannel(channel);
	// channel->addClient(&(message.getSender()));
	// info(message.getSender().getUniqueName() + " JOINED Channel " + channel->getUniqueName(), CLR_GRN);
	// //TODO: send messages to the channel that the client has joined
}

void	Server::invite(Message &message)
{
	(void)message;

	// // INVITE <client> #<channel>
	// // if (!findInList(message.getArg1(), _clients))
	// // {
	// // 	// info(":No such nick", CLR_RED);
	// // 	// TODO: no suck nick
	// // 	return;
	// // }
	// // if (!findInList(message.getChannelName(), _channels))
	// // {
	// // 	// TODO: no such channel
	// // 	return ;
	// // }
	// message.getReceiver()->getInvited(channel);
	// // TODO: send a message that a client was invited to a channel
}

void	Server::topic(Message &message)
{
	(void)message;
	// TOPIC #<channelName> :<topic>
	
}

void	Server::mode(Message &message)
{
	(void)message;
	// MODE #<channelName> flag
}

void	Server::kick(Message &message)
{
	(void)message;

	// // KICK #<channelName> <client>
	// // if (!findInList(message.getArg1(), _clients))
	// // {
	// // 	// info(":No such nick", CLR_RED);
	// // 	// TODO: no suck nick
	// // 	return;
	// // }
	// // if (!findInList(message.getChannelName(), _channels))
	// // {
	// // 	// TODO: no such channel
	// // 	return ;
	// // }
	// message.getSender().getKicked(channel);
}

void	Server::part(Message &message)
{
	(void)message;

	// message.getSender().leaveChannel(channel);
	// channel->removeClient(&(message.getSender()));
	// if (!channel->isActive())
	// 	_channels.remove(*channel);
}

// -----------------------------------------------------------------------------
// Client Methods
// -----------------------------------------------------------------------------
void	Server::addClient(Client client)
{
	_clients.push_back(client);
}

void	Server::removeClient(Client *client)
{
	if (!client)
		return;
	_clients.remove(*client);
	// TODO: CHECK if this calls the client destructor
	// There also the username should be freed again!
	// Print an info that the user was deleted!
}

Client	*Server::getClientByFd(int fd)
{
	for (std::list<Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->getSocketFd() == fd)
			return &(*it);
	}
	return NULL;
}

Client	*Server::getClientByNick(const std::string &nickname)
{
	for (std::list<Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->getUniqueName() == nickname)
			return &(*it);
	}
	return NULL;
}

// -----------------------------------------------------------------------------
// Channel Methods
// -----------------------------------------------------------------------------
void	Server::addChannel(Channel &channel)
{
	(void)channel;
	//TODO:
}

void	Server::removeChannel(Channel *channel)
{
	(void)channel;
	//TODO:
}

Channel	*Server::getChannelByName(const std::string &channelName)
{
	if (channelName.empty())
		return NULL;

	// If we have a channel name find
	// Try to find the channel
	for (std::list<Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		if (it->getUniqueName() == channelName)
			return &(*it);
	}
	return NULL;
}

Channel	*Server::createNewChannel(Message &msg)
{
	if (getChannelByName(msg.getChannelName()))
		return NULL;
	// Couldn't find the channel -> create it
	_channels.push_back(Channel(msg.getChannelName(), &(msg.getSender())));
	return &(_channels.back());
}

// -----------------------------------------------------------------------------
// Static Signal handling (for exit with CTRL C)
// -----------------------------------------------------------------------------
volatile sig_atomic_t	Server::_keepRunning = 1;

void	Server::setupSignalHandling()
{
    signal(SIGINT, Server::sigIntHandler);
}

void	Server::sigIntHandler(int sig)
{
	if(sig != SIGINT)
		return;
    _keepRunning = 0;  // Set flag to false to break the loop
    std::cout << CLR_RED << "Shutdown signal received, terminating server..." << CLR_RST << std::endl;
}

// -----------------------------------------------------------------------------
// Standard exception class for server
// -----------------------------------------------------------------------------
ServerException::ServerException(const std::string &message) : _message(message)
{
	// Nothing to do
}

ServerException::~ServerException() throw()
{
	// Nothing to do
}

const char *ServerException::what(void) const throw()
{
    return (_message.c_str());
}
