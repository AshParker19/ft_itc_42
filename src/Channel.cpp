/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: anshovah <anshovah@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/05/01 23:23:46 by anshovah          #+#    #+#             */
/*   Updated: 2024/05/05 04:08:43 by anshovah         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

// Constructor
Channel::Channel(const std::string &name, Client &client) : 
	_name(name), _topic(""), _key(""), _limit(0), _inviteOnly(false), _topicProtected(false)
{
    Logger::log("Channel created: " + _name);
    client.addChannel(this);
    _clients.push_back(&client);
	_operators.push_back(&client);

	// SEND JOIN MESSAGE FOR THE CLIENT THAT CREATED THE CHANNEL
	client.sendMessage(":" + client.getUniqueName() + 
		"!" + client.getUsername() + "@localhost" + // + msg.getSender().getHost()
		" JOIN " + _name + " * :realname");
}

// Destructor
Channel::~Channel()
{
    std::list<Client *>::iterator it = _clients.begin();

    while (it != _clients.end())
    {
        (*it)->removeChannel(this);
        it++;
    }
    _clients.clear();
}

// Equal Overload for list remove
bool Channel::operator==(const Channel &other) const
{
	return this->_name == other._name;
}

void Channel::sendMessageToClients(const std::string &ircMessage) const
{
    std::list<Client *>::const_iterator it = _clients.begin();

    while (it != _clients.end())
    {
        // if ((*it)->getUniqueName() != ircMessage.substr(1, ircMessage.find(' ', 1) - 1))
        //TODO: not send a message to itself
        (*it)->sendMessage(ircMessage);
        it++;
    }
}

/*
	This takes a message which HAS to contain:
		- sender
		- key (optional)

	Checks if the channel accepst the client
		Checks flags and stuff
*/
void Channel::addClient(Message &msg)
{
	// IF ALREADY IN CHANNEL
	if (isClientInChannel(msg.getSender()))
	{
		// SEND MESSAGE ALREADY IN CHANNEL |
		msg.getSender().sendMessage(ERR_USERONCHANNEL,
			msg.getSender().getUniqueName() + " " +  msg.getChannelName() + " :is already on channel");
		return ;
	}
	
	// IS K FLAG?
	if (!_key.empty())
	{
		// CHECK IF PASWD IS PROVIDED AND CORRECT
		if (_key != msg.getArg(0))
		{
			// 475
			return msg.getSender().sendMessage(ERR_BADCHANNELKEY, _name + " :Cannot join channel (+k)");
		}
	}
	// IF I FLAG
	if (_inviteOnly)
	{
		// CHECK IF INVITED
		// TODO: check if this sender is invited
		msg.getSender().sendMessage(ERR_INVITEONLYCHAN, _name + " :Cannot join channel (+i)");
		return ;
	}

	// IF L FLAG
	if (_limit != 0)
	{
		// CHECK IF CHANNEL IS FULL
		if (_clients.size() != 0 && _clients.size() >= _limit)
			return msg.getSender().sendMessage(ERR_CHANNELISFULL, msg.getChannelName() + " :Cannot join channel (+l)");
	}

	// IF WE GOT HERE
	// JOIN CHANNEL
    _clients.push_back(&msg.getSender());
	msg.getSender().addChannel(this);

	// CREATE MSG
	std::string msgToSend =
		":" + msg.getSender().getUniqueName() + 
		"!" + msg.getSender().getUsername() + "@localhost" + // + msg.getSender().getHost()
		" JOIN " + _name + " * :realname";

	// SEND JOIN MESSAGE
	msg.getSender().sendMessage(msgToSend);
	
	// INFORM ALL CHANNEL MEMBERS
	// >> :ash222!anshovah@F456A.75198A.60D2B2.ADA236.IP JOIN #qweqwe * :realname
	this->sendMessageToClients(msgToSend);	
}

void Channel::addOperator(Message &msg)
{
	(void)msg;
	// _operators.push_back(client);
}

// // SETTERS
// void	Channel::setTopic(const std::string &param)
// {
// 	_topic = param;
// }

// void	Channel::setKey(const std::string &param)
// {
// 	_key = param;
// }

// void	Channel::setUserLimit(const size_t param)
// {
// 	_limit = param;
// }

// void	Channel::setInviteOnly(const bool param)
// {
// 	_inviteOnly = param;
// }

// void	Channel::setTopicProtected(const bool param)
// {
// 	_topicProtected = param;
// }

// // GETTERS
// const std::string		&Channel::getTopic() const
// {
// 	return _topic;
// }

// const std::string		&Channel::getKey() const
// {
// 	return _key;
// }

// size_t 			Channel::getUserLimit() const
// {
// 	return _limit;
// }

// bool 				Channel::getInviteOnly() const
// {
// 	return _inviteOnly;
// }

// bool 				Channel::getTopicProtected() const
// {
// 	return _topicProtected;
// }


void	Channel::removeClient(Client &client)
{
	(void)client;
    // TODO: test check if one op can kick another op
    // _clients.remove(client);
    // _operators.remove(client); // TODO: test if this fails if the client isnt in the list
	// TODO: send a message to the channel?
}

const std::string &Channel::getUniqueName() const
{
    return _name;
}



bool	Channel::isActive() const
{
	if(_operators.size() == 0)
		return false;
	if(_clients.size() == 0)
		return false;
	return true;
}


// PRIVATE METHODS
// -----------------------------------------------------------------------------
bool	Channel::isClientInChannel(const Client &client) const
{
	if (_clients.empty())
		return false;
	std::list<Client *>::const_iterator it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (&client == *it)
			return true;
	}
	return false;
}

// i t k o l
void	Channel::manageMode(Message &msg)
{

}
