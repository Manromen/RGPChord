/*
 ChordTypes.h
 Chord

 Created by Ralph-Gordon Paul on 28. April 2014.
 
 -------------------------------------------------------------------------------
 The MIT License (MIT)
 
 Copyright (c) 2013 Ralph-Gordon Paul. All rights reserved.
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
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
}

#endif /* defined(__RGP__Chord__ChordTypes__) */
