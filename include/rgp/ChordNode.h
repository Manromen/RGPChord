/*
 ChordNode.h
 Chord

 Created by Ralph-Gordon Paul on 13. July 2013.
 
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

#ifndef __RGP__Chord__ChordNode__
#define __RGP__Chord__ChordNode__

#include <iostream>

#include <rgp/Chord>

namespace rgp {
    
    // forward declaration
    class Chord;
    
    class ChordNode {
        
    public:
        // Constructor
        // Node ID, IP-Address, Port, associated Chord
        ChordNode (ChordHeaderNode, std::string, uint16_t,
                   std::shared_ptr<Chord>);
        // Destructor
        ~ChordNode ();
        
        // getter
        ChordId getNodeID () const { return this->_nodeID; }
        std::string getIPAddress () const { return this->_ipAddress; }
        int getPort () const { return this->_port; }
        int getReceiveSocket () const { return this->_receiveSocket; }
        int getSendSocket () const { return this->_sendSocket; }
        
        // setter
        void setSendSocket (int socket) { this->_sendSocket = socket; }
        // sets new receive socket and starts listening thread
        void setReceiveSocket (int socket);
        
        // returns this node as struct ChordHeaderNode
        ChordHeaderNode chordNode_t () const;
        
        // checks if connection is open
        bool isAlive ();
        // establish a connection to remote node (if not already connected)
        ChordConnectionStatus establishSendConnection ();
        
        // send connection can be close
        // (f.e. we have a new successor and don't need to keep the connection
        // alive anymore)
        void closeSendConnection ();
        
        // tell's remote node that i'm his predecessor
        // this method is used by stablilization
        // returns node that the remote node returns
        // throws ChordConnectionException on error
        ChordHeaderNode getPredecessorFromRemoteNode
        (std::shared_ptr<ChordNode> ownNode);
        
        // search for a key (or a node)
        // throws ChordConnectionException on error
        ChordHeaderNode searchForKey (ChordId key);
        
        // receive data for key
        // returns nullptr if data not found
        // throws ChordConnectionException on error
        std::shared_ptr<ChordData> requestDataForKey (ChordId key);
        
        // sends the data to the remote node to add it there locally
        // returns true on success
        bool addData (std::string *data);
        
        // returns a describing string of the instance (node id, ip, ...)
        std::string description () const;
        
    private:
        ChordId _nodeID { 0 };
        std::string _ipAddress { "" };
        uint16_t _port { 0 };
        // request from the other side are incomming here
        int _receiveSocket { -1 };
        // requests are send with this socket
        int _sendSocket { -1 };
        
        // make send socket thread-safe
        // (several threads will use this:
        // stabilize() thread
        // TUI thread (for search and add data)
        // receiveHandler of other nodes (for search from other nodes etc.)
        std::mutex _sendSocket_mutex;
        
        // thread to handle incomming requests
        std::thread _requestHandlerThread {};
        // determines if the requestHandlerThread should exit
        std::atomic<bool> _stopRequestHandlerThread { false };
        
        // associated Chord
        std::shared_ptr<Chord> _chord { nullptr };
        
        // handle incomming data (heartbeat, search, ...)
        void handleRequests ();
        
        // sends response to remote node
        // throws ChordConnectionException on error
        void sendResponse (ChordMessageType type, char *data, ssize_t dataSize);
        
        // sends request to remote node
        // throws ChordConnectionException on error
        void sendRequest (ChordMessageType type, char *data, ssize_t dataSize);
        
        // receives response from remote node
        // returns pointer to reveived data or nullptr if
        // there was no received data
        // throws ChordConnectionException on error
        char *recvResponse (ChordMessageType *type, ssize_t *dataSize);
    };
}

#endif /* defined(__RGP__Chord__ChordNode__) */
