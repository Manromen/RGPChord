/*
 ChordTypes.h
 Chord

 Created by Ralph-Gordon Paul on 28. April 2014.
 
 -------------------------------------------------------------------------------
 GNU Lesser General Public License Version 3, 29 June 2007
 
 Copyright (c) 2014 Ralph-Gordon Paul. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public License
 along with this library.
 -------------------------------------------------------------------------------
*/

#ifndef __RGP__Chord__ChordTypes__
#define __RGP__Chord__ChordTypes__

namespace rgp {
    
    typedef uint32_t ChordId;
    
    typedef struct {
        ChordId from;
        ChordId to;
    } ChordRange;
    
    // specify type of message
    typedef enum : uint8_t {
        
        // if someone connects he identifies himself with his first package
        ChordMessageTypeIdentify = 1,
        
        // checks if the node is still alive
        ChordMessageTypeHeartbeat,
        // answers to hearbeat (i'm still alive!)
        ChordMessageTypeHeartbeatReply,
        
        // search for a hash id
        ChordMessageTypeSearch,
        // answers search with another node (that is responsible for the key)
        ChordMessageTypeSearchNodeResponse,
        
        // requests data
        ChordMessageTypeDataRequest,
        // send data
        ChordMessageTypeDataAnswer,
        // requested data not found
        ChordMessageTypeDataNotFound,
        
        // add data
        ChordMessageTypeDataAdd,
        // answers: data wasn't added
        ChordMessageTypeDataAddFailed,
        // answers: data was added successfully
        ChordMessageTypeDataAddSuccess,
        
        // i'm your predecessor
        ChordMessageTypeUpdatePredecessor,
        // tell me your predecessor
        ChordMessageTypeTellPredecessor,
        // my predecessor is ... (answer to update/tell predecessor)
        ChordMessageTypePredecessor
    } ChordMessageType;
    
    // feedback of connect()
    typedef enum : uint8_t {
        ChordConnectionStatusSuccessfullyConnected = 1,
        ChordConnectionStatusConnectingFailed,
        ChordConnectionStatusAlreadyConnected
    } ChordConnectionStatus;
    
    // node data as struct
    // this struct should always contain network byte order (for consistency)
    typedef struct {
        // node id
        ChordId nodeId;
        // ip-address
        uint32_t ip;
        // listening port
        uint16_t port;
    } ChordHeaderNode;
    
    // every message begins with this header
    // this struct should always contain network byte order (for consistency)
    typedef struct {
        // senders node info
        ChordHeaderNode node;
        // message type (ChordMessageType)
        ChordMessageType type;
        // size of data that follows (0 if there is no data)
        uint32_t dataSize;
    } ChordHeader;
    
    // exceptions
    class ChordConnectionException {
        
    private:
        std::string reason;
        
    public:
        ChordConnectionException (std::string reason) : reason(reason) {}
        std::string what () { return reason; }
    };
}

#endif /* defined(__RGP__Chord__ChordTypes__) */
