//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_API_API_TYPES_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_API_API_TYPES_HPP

#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <string>
#include <string_view>

#include "business_types.hpp"
#include "error.hpp"

// This file contains type definitions for HTTP and websocket API objects.
// Types for incoming requests are owning, since they're used after parsing,
// and match exactly the types and field names in the API.
// Types for responses and outgoing events are non-owning and lightweight,
// since they are only used as intermediate types for serialization.
// They do not have the exact same field names and types as the actual
// API messages, but instead contain enough information to produce them.
// This saves copies.

namespace chat {

//
// Incoming messages (HTTP requests and websocket client events)
//

// The request for POST /create-account
struct create_account_request
{
    // Username of the user to create
    std::string username;

    // Email (login identifier) of the new user.
    std::string email;

    // Password to use.
    std::string password;

    // Parses a request from a JSON string
    static result<create_account_request> from_json(std::string_view from);
};

// The request for POST /login
struct login_request
{
    // Email (login identifier) of the user to authenticate.
    std::string email;

    // Password to use.
    std::string password;

    // Parses a request from a JSON string
    static result<login_request> from_json(std::string_view from);
};

// A message as sent by the client
struct client_message
{
    std::string content;
};

// Sent by the client as a request to broadcast messages to other clients
// in a room
struct client_messages_event
{
    std::string roomId;
    std::vector<client_message> messages;
};

// Sent by the client to request more history for a certain room.
struct request_room_history_event
{
    std::string roomId;

    // ID of the earliest-in-time message that the client has.
    // This is a pagination mechanism.
    std::string firstMessageId;
};

// A variant that can represent any event that may be received from the client,
// or an error_code, if the client sent an invalid message
using any_client_event = boost::variant2::variant<
    error_code,  // Invalid, used to report errors
    client_messages_event,
    request_room_history_event>;

// Parses a message received from the websocket client into a variant
// holding any of the valid client-side events.
any_client_event parse_client_event(std::string_view from);

//
// Outgoing messages (HTTP responses and server events)
//

// Used within api_error, as a way to communicate specific error conditions
// to the client.
enum class api_error_id
{
    // generic, when there is not a more specific error ID
    bad_request = 0,

    // A login attempt failed (e.g. bad username or password)
    login_failed,

    // Failure during account creation, the selected email already exists
    email_exists,

    // Failure during account creation, the selected username already exists
    username_exists,
};

// A REST API error. Used within HTTP error responses.
struct api_error
{
    // An identifier for the error that occurred.
    api_error_id error_id;

    // A human-readable explanation of the error.
    std::string_view error_message;

    // Serializes the object as a JSON string.
    std::string to_json() const;
};

// Sent to the client when it connects
struct hello_event
{
    // The current authenticated user
    const user& me;

    // The list of chat rooms, with some message history
    boost::span<const room> rooms;

    // A user_id -> username map, to resolve user IDs into usernames
    const username_map& usernames;

    // Serializes the object as a JSON string.
    std::string to_json() const;
};

// Broadcast by the server to all clients in a room to signal that some messages arrived
struct server_messages_event
{
    // The room ID
    std::string_view room_id;

    // The user that sent the messages
    const user& sending_user;

    // The actual messages. All messages must have this->user_id == sending_user.id
    boost::span<const message> messages;

    // Serializes the object as a JSON string.
    std::string to_json() const;
};

// Sent to the client as a response to a request_room_history_event
struct room_history_event
{
    // The room ID
    std::string_view room_id;

    // The actual messages
    const message_batch& history;

    // A user_id -> username map, to resolve user IDs into usernames
    const username_map& usernames;

    // Serializes the object as a JSON string.
    std::string to_json() const;
};

}  // namespace chat

#endif
