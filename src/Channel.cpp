/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: astein <astein@student.42lisboa.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/01 23:23:46 by anshovah          #+#    #+#             */
/*   Updated: 2024/05/13 21:45:23 by astein           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"


// Constructor
// -----------------------------------------------------------------------------
Channel::Channel(const std::string &name, const std::string &topic) : 
	_channelName(name),
	_topic(topic),
	_topicChange(""),
	_key(""),
	_limit(0),
	_inviteOnly(false),
	_topicProtected(true),
	_clients()
{
    Logger::log("Channel CREATED: " + _channelName);
	logChanel();
}

// Initialize the channel with the client that created it
// -----------------------------------------------------------------------------
// It could be called by anyone so the only way to kinf od protect it is to 
// check if the client list is empty
void 	Channel::iniChannel(Client *client)
{
	if (!_clients.empty())
		return ;
	client->addChannel(this);
	std::string msg;
	// SEND JOIN MESSAGE FOR THE CLIENT WAS ADDED
	msg =	":" + client->getUniqueName() + "!" + client->getUsername() + "@localhost" +" JOIN " + _channelName + " * :realname";
	client->sendMessage(msg);
    this->addClient(client, STATE_O);
    Logger::log("Channel INIT: " + _channelName);
	logChanel();
}

// Copy Constructor
// -----------------------------------------------------------------------------
Channel::Channel(const Channel &other) : 
	_channelName(other._channelName),
	_topic(other._topic),
	_topicChange(other._topicChange),
	_key(other._key),
	_limit(other._limit),
	_inviteOnly(other._inviteOnly),
	_topicProtected(other._topicProtected)
{
	Logger::log("Channel COPIED: " + _channelName);
	std::map<Client *, int>::const_iterator it;
	for(it = other._clients.begin(); it != other._clients.end(); ++it)
		_clients.insert(*it);
	logChanel();
}

// Destructor
// -----------------------------------------------------------------------------
Channel::~Channel()
{
	std::map<Client *, int>::const_iterator it;
	for(it = _clients.begin(); it != _clients.end(); ++it)
		it->first->removeChannel(this);
	_clients.clear();
	Logger::log("Channel DESTROYED: " + _channelName);
}

// Equal Overload (for list remove)
// -----------------------------------------------------------------------------
bool	Channel::operator==(const Channel &other) const
{
	return this->_channelName == other._channelName;
}

// So the Server can check if the Channel still has an operator
// -----------------------------------------------------------------------------
bool	Channel::isActive() const
{
	std::map<Client *, int>::const_iterator it;
	for(it = _clients.begin(); it != _clients.end(); ++it)
		if (it->second == STATE_O)
			return true;
	return false;
}

// Members & Operators funtionality
// -----------------------------------------------------------------------------
/*
	This takes a message which HAS to contain:
		- sender
		- key (optional)

	Checks if the channel accepst the client
		Checks flags and stuff
*/
void	Channel::joinChannel(Client *client, const std::string &pswd)
{
	if (!client)
		return ;

	// IF ALREADY IN CHANNEL
	if (getClientState(client) > STATE_I)
	{
		// SEND MESSAGE ALREADY IN CHANNEL
		client->sendMessage(ERR_USERONCHANNEL, client->getUniqueName() + " " +  _channelName + " :is already on channel");
		Logger::log("Client " + client->getUniqueName() + " is already in " + _channelName);
		return ;
	}
	
	// IS K FLAG?
	if (!_key.empty())
	{
		// CHECK IF PASWD IS PROVIDED AND CORRECT
		if (_key != pswd)
		{
			// 475
			return client->sendMessage(ERR_BADCHANNELKEY, _channelName + " :Cannot join channel (+k)");
		}
	}
	
	// IF I FLAG
	if (_inviteOnly)
	{
		// CHECK IF INVITED
		if (getClientState(client) < STATE_I)
		{
			client->sendMessage(ERR_INVITEONLYCHAN, _channelName + " :Cannot join channel (+i)");
			return ;
		}
	}
	
	// IF L FLAG
	if (_limit != 0)
	{
		// CHECK IF CHANNEL IS FULL
		int noOfClients = 0;
		std::map<Client *, int>::const_iterator it;
		for(it = _clients.begin(); it != _clients.end(); ++it)
			if (it->second > STATE_I)
				noOfClients++;
		if (noOfClients >= _limit)
			return client->sendMessage(ERR_CHANNELISFULL, _channelName + " :Cannot join channel (+l)");
	}

	// IF WE GOT HERE
	// JOIN CHANNEL

	// 1. MSG TO NEW CLIENT
	std::string msgToSend;
	msgToSend =	":" + client->getUniqueName() + "!" + client->getUsername() + "@localhost" +" JOIN " + _channelName + " * :realname";
	client->sendMessage(msgToSend);

	// 2. ADD CLIENT TO CHANNEL (which will send him the mode and topic, and names)
	client->addChannel(this);
	this->addClient(client, STATE_C);
	
	// 3. SEND JOIN MESSAGE TO EVERYONE ELSE
	this->sendMessageToClients(msgToSend, client);

	Logger::log("Client " + client->getUniqueName() + " joined " + _channelName);
}

void	Channel::inviteToChannel(Client *host, Client *guest)
{
	// IF HOST IS NOT IN CHANNEL
	if (getClientState(host) < STATE_C)
	{
		host->sendMessage(ERR_NOTONCHANNEL, _channelName + " :You're not on that channel");
		return ;
	}
	
	// IF GUEST ALREADY IN CHANNEL
	if (getClientState(guest) >= STATE_I)
	{
		host->sendMessage(ERR_USERONCHANNEL, guest->getUniqueName() + " " +  _channelName + " :is already on channel");
		return ;
	}
	
	// IF INVITE ONLY (I FLAG)
	if (_inviteOnly)
	{
		if (getClientState(host) < STATE_O)
		{
			host->sendMessage(ERR_CHANOPRIVSNEEDED, _channelName + " :You're not channel operator");
			return ;
		}
	}
	
	// Add guest to the invitation list
	this->addClient(guest, STATE_I);

	// SEND INVITE
	// host :Aurora.AfterNET.Org 341 astein astein__ #test3
	host->sendMessage(RPL_INVITING, guest->getUniqueName() + " " + _channelName);
	
	// guest :astein!alex@F456A.75198A.60D2B2.ADA236.IP INVITE astein__ #test3
	guest->sendMessage(":" + host->getUniqueName() + "!" + host->getUsername() +
		"@localhost" + " INVITE " + guest->getUniqueName() + " " + _channelName);

	Logger::log("Invite sent to " + guest->getUniqueName() + " by " + host->getUniqueName());
}

void	Channel::kickFromChannel(Client *kicker, Client *kicked, const std::string &reason)
{
	// IF KICKER IS NOT OPERATOR
	if (getClientState(kicker) < STATE_O)
	{
		kicker->sendMessage(ERR_CHANOPRIVSNEEDED, _channelName + " :You're not channel operator");
		return ;
	}
	
	// IF KICKED IS NOT IN CHANNEL
	if (getClientState(kicked) < STATE_C)
	{
		kicked->sendMessage(ERR_USERNOTINCHANNEL, kicked->getUniqueName() + " " + _channelName + " :They aren't on that channel");
		return ;
	}

	// FIRST SEND THE MSG THEN KICK SO THAT THE
	// sendMessageToClients() will INLUDE THE KICKED GUY
	// MSG:
	// :astein!alex@F456A.75198A.60D2B2.ADA236.IP KICK #test3 astein__ :astein
	std::string msg = ":" + kicker->getUniqueName() + "!" + kicker->getUsername() + "@localhost" +
		" KICK " + _channelName + " " + kicked->getUniqueName() + " :" + kicker->getUniqueName();
	if (!reason.empty())
		msg += " :" + reason;
	this->sendMessageToClients(msg);

	kicked->removeChannel(this);
	this->removeClient(kicked);
	Logger::log("Client " + kicked->getUniqueName() + " kicked from " + _channelName + " by " + kicker->getUniqueName());
}

void	Channel::partChannel(Client *client, const std::string &reason)
{
	if (!client)
		return ;

	// PREVENT CLEINT FROM LEAVING LOBBY CHANNEL
	if (_channelName == "#lobby")
	{
		client->sendMessage(ERR_UNKNOWNCOMMAND, _channelName + " :You can't leave the Lobby channel!");
		return ;
	}
	
	// IF CLIENT IS NOT IN CHANNEL
	if (getClientState(client) < STATE_C)
	{
		client->sendMessage(ERR_NOTONCHANNEL, _channelName + " :You're not on that channel");
		return ;
	}

	// FIRST SEND THE MSG THEN PART SO THAT THE
	// sendMessageToClients() will INLUDE THE LEAVING GUY
	// MSG:
	// :astein_!alex@F456A.75198A.60D2B2.ADA236.IP PART #test :Leaving
	std::string r = "Leaving";
	if(!reason.empty())
		r = reason;
	std::string msg = ":" + client->getUniqueName() + "!" + client->getUsername() + "@localhost" +
		" PART " + _channelName + " :" + r;
	this->sendMessageToClients(msg);

	client->removeChannel(this);
	this->removeClient(client);
	Logger::log("Client " + client->getUniqueName() + " left " + _channelName);
}

// Modes & Topic funtionality
// -----------------------------------------------------------------------------
void	Channel::topicOfChannel(Client *sender, const std::string &topic)
{
	// IF SENDER IS NOT IN CHANNEL
	if (getClientState(sender) < STATE_C)
	{
		sender->sendMessage(ERR_NOTONCHANNEL, _channelName + " :You're not on that channel");
		return ;
	}
	
	// IF NO TOPIC IS PROVIDED
	if (topic.empty())
	{
		// SEND TOPIC MESSAGE
		sendTopicMessage(sender);
		return ;
	}
	
	// IF TOPIC IS PROTECTED (FLAG T)
	if (_topicProtected)
	{
		if (getClientState(sender) < STATE_O)
		{
			sender->sendMessage(ERR_CHANOPRIVSNEEDED, _channelName + " :You're not channel operator");
			return ;
		}
	}
	// CHANGE TOPIC
	_topic = topic;

	// GET THE TOPIC CHANGED STRING
	std::time_t currentTime = std::time(0);
	// Convert time_t to string using stringstream unix timestamp
	std::stringstream ss;
	ss << currentTime;
	_topicChange = sender->getUniqueName() + "!" + 
			sender->getUsername() + "@localhost" +
			" " + ss.str();
	// SEND TOPIC MESSAGE
	std::string confirmMsg = ":" +
		sender->getUniqueName() + "!" +
		sender->getUsername() + "@localhost" +
		" TOPIC " + _channelName + " :" + _topic;

	sendMessageToClients(confirmMsg);

	Logger::log("Topic changed to: " + _topic + " by " + sender->getUniqueName());
	Logger::log("Topic change message: " + _topicChange);
}

void	Channel::modeOfChannel(Client *sender, const std::string &flag, const std::string &value, Server *server)
{
	// IF FLAG IS NOT PROVIDED
	if (flag.empty())
	{
		// 	:Aurora.AfterNET.Org 324 ash2223 #test +tinrc 
		sender->sendMessage(RPL_CHANNELMODEIS, _channelName + " " + getChannelFlags());
		return ;
	}

	// IF SENDER IS NOT IN CHANNEL
	if (getClientState(sender) < STATE_C)
	{
		sender->sendMessage(ERR_NOTONCHANNEL, _channelName + " :You're not on that channel");
		return ;
	}
	
	std::string mode = "@";
	char sign = '+';
	
	if (flag.size() == 1)
	{
		mode[0] = flag[0];
	}
	else if (flag.size() == 2)
	{
		sign = flag[0];
		mode[0] = flag[1];
	}

	// MODE IS WRONG IF
	// 	- SIGN IS NOT + OR -
	// 	- FLAG IS NOT 1 OR 2 CHAR LONG
	// 	- FLAG IS NOT ONE OFF THE FOLLOWING: itkol
	if ((sign != '+' && sign != '-') || (flag.size() > 2) || (mode.find_first_not_of("itkol") != std::string::npos))
	{
		sender->sendMessage(ERR_UNKNOWNMODE, sender->getUniqueName() + " " + flag + " :is unknown mode char to me");
		return ;
	}

	// CHECK IS OPERATOR
	if (getClientState(sender) < STATE_O)
	{
		sender->sendMessage(ERR_CHANOPRIVSNEEDED, _channelName + " :You're not channel operator");
		return ;
	}

	switch (mode[0])
	{
		// invite-only mode
		case 'i':
		{
			if (_inviteOnly != (sign == '+'))
			{
				_inviteOnly = !_inviteOnly;
				sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
					" MODE " + _channelName + " " + sign + mode);
			}
			break;
		}
		// topic change restriction 
		case 't':
		{
			if (_topicProtected != (sign == '+'))
			{
				_topicProtected = !_topicProtected;
				sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
					" MODE " + _channelName + " " + sign + mode);
			}
			break;
		}
		case 'k':
		{
			// IF KEY IS NOT PROVIDED
			if (value.empty())
			{
				sender->sendMessage(ERR_NEEDMOREPARAMS, "MODE " + mode + " :Not enough parameters");
				break ;
			}

			// IF WE WANT TO SET ...
			if (sign == '+')
			{
				// ... AND KEY WAS SET BEFORE ...
				if (!_key.empty())
				{
					// ... INFORM SENDER 
					sender->sendMessage(ERR_KEYSET, _channelName + " :Channel key already set");
				}
				// ...AND KEY WAS EMPTY BEFORE...
				else
				{
					// ... SET IT
					_key = value;
					// :ash2223!anshovah@F456A.75198A.60D2B2.ADA236.IP MODE #test +k try
					sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
						" MODE " + _channelName + " " + sign + mode + " " + value);
				}
			}
			// IF WE WANT TO REMOVE KEYWORD ...
			else
			{
				// ... IF KEYWORD WAS SET BEFORE AND ...
				if (!_key.empty())
				{
					// ... IF VALUE IS CORRECT KEY
					if (value == _key) // remove channel keyword
					{
						// :ash2223!anshovah@F456A.75198A.60D2B2.ADA236.IP MODE #test -k try
						_key = "";
						sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
						" MODE " + _channelName + " " + sign + mode + " " + value);
					}
					// ... IF VALUE IS NOT CORRECT KEY
					else
					{
						// :luna.AfterNET.Org 467 ash2223 #test :Channel key already set
						sender->sendMessage(ERR_KEYSET, _channelName + " :Channel key already set");
						break ;
					}
				}
				// ... IF IT WAS NOT SET BEFORE --> IGNORE IT
				break ;
			}
			break ;
		}
		case 'l':
		{
			if (sign == '-')
			{
				// IF FLAG WAS SET
				if (_limit != 0)
				{
					// UNSET IT AND INFORM
					_limit = 0;
					sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
						" MODE " + _channelName + " -l");
				}
				break ;
			}
			//IF SIGN IS +
			else
			{
				if(value.empty())
				{
					sender->sendMessage(ERR_NEEDMOREPARAMS, "MODE +l :Not enough parameters");
					break ;
				}

				// CHECK IF VALUE IS A POSITIVE SIGNED INTEGER
				if (intNoOverflow(value))
				{
					int newLimit = std::atoi(value.c_str());
					if (_limit == newLimit)
						break ;
					if (newLimit <= 0)
					{
						sender->sendMessage(ERR_NEEDMOREPARAMS, "MODE +l :Not enough parameters");
						break ;
					}
					_limit = newLimit;
					sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
						" MODE " + _channelName + " +l " + to_string(_limit));
				}
				break ;
			}
			break ;
		}
		case 'o':
		{
			// IF TARGET IS NOT PROVIDED
			if(value.empty())
				break ;
			// IF TARGET IS DOESN'T EXIST
			Client *target = server->getClientByNick(value);
			if (!target)
			{
				sender->sendMessage(ERR_NOSUCHNICK, value + " :No such nick/channel");
				break ;
			}
			// IF TARGET IS NOT IN CHANNEL
			if (getClientState(target) < STATE_C)
			{
				sender->sendMessage(ERR_USERNOTINCHANNEL, value + " " + _channelName + " :They aren't on that channel");
				break ;
			}

			// IF TARGET IS NOT AN OPERATOR YET AND WE WANT TO MAKE HIM ONE...
			// OR IF TARGET IS AN OPERATOR AND WE WANT TO REMOVE HIM
			if ((getClientState(target) == STATE_O) != (sign == '+'))
			{
				// ... MAKE HIM ONE OR REMOVE HIM AND INFORM
				addClient(target, sign == '+' ? STATE_O : STATE_C);
				sendMessageToClients(":" + sender->getUniqueName() + "!" + sender->getUsername() + "@localhost" +
					" MODE " + _channelName + " " + sign + "o " + value);
			}
			break ;
		}
	}
}

// Simple List Management
// -----------------------------------------------------------------------------
// IF CLIENT ALREADY EXISTS, IT WILL UPDATE THE STATUS
// THIS WILL ALSO SEND THE RPL_TOPIC and RPL_NAMREPLY MSG
void	Channel::addClient		(Client *client, int status)
{
	_clients[client] = status;
	if (status > STATE_I)
	{
		sendTopicMessage(client);
		sendNamesMessage(client);
	}
	Logger::log("Channel " + _channelName + " added/changed client: " + client->getUniqueName() + " to status " + to_string(status));
	logChanel();
}

void	Channel::removeClient	(Client *client)
{
	_clients.erase(client);
}

// Channel Broadcast Message
// -----------------------------------------------------------------------------
// If sender is provided, it will not send the message to the sender
void	Channel::sendMessageToClients(const std::string &ircMessage, Client *sender) const
{
    std::map<Client *, int>::const_iterator it;
	for(it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (sender && *it->first == *sender)
			continue ;
		// IF CLIENT IS NOT IN THE CHANNEL SKIP IT (INVITED CLIENTS)
		if (it->second < STATE_C)
			continue ;
		it->first->sendMessage(ircMessage);
	}
	std::string logMsg ="Channel " + _channelName + " sent message to all clients";
	if(sender)
		logMsg += " except " + sender->getUniqueName();
	logMsg += ": " + ircMessage;
	Logger::log(logMsg);
}

void	Channel::sendWhoMessage(Client *receiver) const
{
	std::map<Client *, int>::const_iterator it;
	
	if(_clients.empty())
		return ;
	for(it = _clients.begin(); it != _clients.end(); ++it)
	{
		// >> :Aurora.AfterNET.Org 352 astein #birdsandbees anshovah F456A.75198A.60D2B2.ADA236.IP *.afternet.org astein H@xz :0 realname
		std::string flags = "H";
		if (it->second == STATE_O)
			flags += "@";
		else if (it->second == STATE_I)
			flags += "*";
		// flags += "xz";
		receiver->sendMessage(RPL_WHOREPLY, _channelName + " " + it->first->getUsername() +
			" " + it->first->getHostname() + " * " + it->first->getUniqueName() + " "  +
			flags + " :0 " + it->first->getFullname());
	}
	receiver->sendMessage(RPL_ENDOFWHO, _channelName + " :End of /WHO list.");
	Logger::log("Channel " + _channelName + " sent WHO message to " + receiver->getUniqueName());
}

void	Channel::sendTopicMessage(Client *receiver) const
{
	if (_topic.empty())
	{
		receiver->sendMessage(RPL_NOTOPIC, _channelName + " :No topic is set");
		Logger::log("Channel " + _channelName + " sent NO TOPIC message to " + receiver->getUniqueName());
		return ;
	}

	receiver->sendMessage(RPL_TOPIC, _channelName + " :" + _topic);
	receiver->sendMessage(RPL_TOPICADDITIONAL, _channelName + " " + _topicChange);
	Logger::log("Channel " + _channelName + " sent TOPIC message to " + receiver->getUniqueName());
}

void	Channel::sendNamesMessage(Client *receiver) const
{
	std::string names = getClientList();
	receiver->sendMessage(RPL_NAMREPLY, "= " + _channelName + " :" + names);
	receiver->sendMessage(RPL_ENDOFNAMES, _channelName + " :End of /NAMES list.");
	Logger::log("Channel " + _channelName + " sent NAMES message to " + receiver->getUniqueName());
}

// Getters and Setters
// -----------------------------------------------------------------------------
const std::string	&Channel::getUniqueName() const
{
    return _channelName;
}

const std::string Channel::getClientList() const
{
	std::string users = "";

	std::map<Client *, int>::const_iterator it;
	for(it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second > STATE_I)
		{
			if (it->second == STATE_O)
				users += "@";
			users += it->first->getUniqueName();
			users += " ";
		}
	}
	Logger::log("Created user (clients & operators) list for channel " + _channelName + ": " + users);
	return users;
}

// LOG
// -----------------------------------------------------------------------------
void Channel::logChanel() const
{
	std::ostringstream header, values;
	
	// Log headers
	header  << std::left
			<< "| " << std::setw(15) << "NAME"
			<< "| " << std::setw(15) << "TOPIC"
			<< "| " << std::setw(15) << "TOPIC CHANGE"
			<< "| " << std::setw(15) << "KEY"
			<< "| " << std::setw(15) << "LIMIT"
			<< "| " << std::setw(15) << "INVITE ONLY"
			<< "| " << std::setw(15) << "TOPIC PROTECTED"
			<< "| " << std::setw(15) << "CLIENTS";

	// Log values
	values  << std::left
			<< "| " << std::setw(15) << (_channelName.length() > 14 ? _channelName.substr(0, 14) + "." : _channelName.empty() ? "(NULL)" : _channelName)
			<< "| " << std::setw(15) << (_topic.length() > 14 ? _topic.substr(0, 14) + "." : _topic.empty() ? "(NULL)" : _topic)
			<< "| " << std::setw(15) << (_topicChange.length() > 14 ? _topicChange.substr(0, 14) + "." : _topicChange.empty() ? "(NULL)" : _topicChange)
			<< "| " << std::setw(15) << (_key.length() > 14 ? _key.substr(0, 14) + "." : _key.empty() ? "(NULL)" : _key)
			<< "| " << std::setw(15) << (_limit == 0 ? "(unset)" : to_string(_limit))
			<< "| " << std::setw(15) << (_inviteOnly ? "Yes" : "No")
			<< "| " << std::setw(15) << (_topicProtected ? "Yes" : "No")
			<< "| " << getClientList();


        // Simulate logging (You can replace this with actual log calls)
	Logger::log("\n=> START CHANNEL ====================================================================================================================");
    Logger::log(header.str());
    Logger::log(values.str());
	Logger::log("=> END CHANNEL ======================================================================================================================\n");
}

// Private Methods
// -----------------------------------------------------------------------------

// Checks the state of a client
// -----------------------------------------------------------------------------
// 	-1: Client not in the list
// 	 0: Client is only invited
// 	 1: Client is in the list
// 	 2: Client is an operator
int	Channel::getClientState(const Client *client) const
{
	if (_clients.empty() || !client)
		return -1;
	std::map<Client *, int>::const_iterator it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
		// Check if the client is in the list
		if (*client == *it->first)
			return it->second;
	return -1;
}

std::string			Channel::getChannelFlags()
{
	std::string flags = "+";

	if (_topicProtected)
		flags += "t";
	if (_inviteOnly)
		flags += "i";
	if (_limit != 0)
		flags += "l";
	if (!_key.empty())
		flags += "k";
	return (flags);
}
