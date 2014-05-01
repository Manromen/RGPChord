/*
 ChordNode.cpp
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

#include <rgp/ChordNode.h>
#include <rgp/Log.h>

#include <sstream>
#include <unistd.h>
#include <cstring>
#include <memory>

// network
#include <arpa/inet.h>
#include <netdb.h>

using namespace rgp;

#pragma mark - Constructor / Destructor

ChordNode::ChordNode (ChordId node, std::string ip, uint16_t port, std::shared_ptr<Chord> chord)
: _nodeID(node), _ipAddress(ip), _port(port), _chord(chord)
{
    
}

ChordNode::~ChordNode ()
{
    // stop request handler
    _stopRequestHandlerThread = true;
    try {
        // wait till request handler finished
        _requestHandlerThread.join();
        
        // catched if requestHandlerThread is not running (or not joinable)
    } catch (...) {}
    
    // disconnect sendSocket if connected
    if (_sendSocket != -1) {
        close(_sendSocket);
        _sendSocket = -1;
    }
}

#pragma mark - Public

// returns this node as struct ChordHeaderNode
ChordHeaderNode ChordNode::chordNode () const
{
    ChordHeaderNode node {0, 0, 0};
    
    node.nodeId = htons(_nodeID);
    node.ip = htonl(inet_addr(_ipAddress.c_str()));
    node.port = htons(_port);
    
    return node;
}

bool ChordNode::isAlive ()
{
    // if we have an open send connection - check if remote node is alive
    if (_sendSocket > 0) {
        
        _sendSocket_mutex.lock(); // we use send socket to send data and receive the response
        try {
            // Hearbeat
            sendRequest(ChordMessageTypeHeartbeat, nullptr, 0);
            
        } catch (ChordConnectionException &exception) {
            // error
            Log::sharedLog()->error(std::string("ChordNode::isAlive(): coulnd't send request: ") += exception.what());
            
            close(_sendSocket);
            _sendSocket = -1;
        }
        
        // receive heartbeat answer
        if (_sendSocket > 0) {
            
            try {
                
                std::shared_ptr<ChordMessageType> type;
                std::shared_ptr<ssize_t> dataSize;
                
                std::shared_ptr<uint8_t> data = recvResponse(type, dataSize);
                
                // success -> node is alive
                if (*type == ChordMessageTypeHeartbeatReply) {
                    _sendSocket_mutex.unlock();
                    return true;
                }
                
            } catch (ChordConnectionException &exception) {
                // error
                Log::sharedLog()->error(std::string("ChordNode::isAlive(): coulnd't receive request: ") += exception.what());
                
                close(_sendSocket);
                _sendSocket = -1;
            }
        }
        _sendSocket_mutex.unlock();
    }
    
    // if receive connection is alive - we don't need a hearbeat for this
    if (_receiveSocket > 0) {
        return true;
    }
    
    return false;
}

ChordConnectionStatus ChordNode::establishSendConnection ()
{
    // check required properties
    if (_ipAddress.compare("") == 0 || _port == 0) {
        return ChordConnectionStatusConnectingFailed;
    }
    
    _sendSocket_mutex.lock();
    
    // check if already connected
    if (_sendSocket != -1) {
        _sendSocket_mutex.unlock();
        return ChordConnectionStatusAlreadyConnected;
    }
    
    // create socket
    _sendSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    // create sockaddr
    struct sockaddr_in addr4client;
    memset(&addr4client, 0, sizeof(addr4client)); // fill struct with zeros
#ifndef __linux__
    addr4client.sin_len = sizeof(addr4client);
#endif
    addr4client.sin_family = AF_INET;
    addr4client.sin_port = htons(_port);
    
    if((addr4client.sin_addr.s_addr = inet_addr(_ipAddress.c_str())) == (unsigned long)INADDR_NONE)
    {
        // ERROR: INADDR_NONE - try with hostname
        struct hostent *hostp = gethostbyname(_ipAddress.c_str());
        if (hostp == NULL) {
            Log::sharedLog()->errorWithErrno("ChordNode::establishSendConnection():hostent failed - cannot use given host address ", errno);
            close(_sendSocket);
            _sendSocket = -1;
            _sendSocket_mutex.unlock();
            return ChordConnectionStatusConnectingFailed; // we cannot connect
        } else {
            memcpy(&addr4client.sin_addr, hostp->h_addr, sizeof(addr4client.sin_addr));
        }
    }
    
    // connect
    if (connect(_sendSocket, (struct sockaddr*)&addr4client, sizeof(struct sockaddr_in)) != 0) {
        Log::sharedLog()->errorWithErrno("ChordNode::establishSendConnection():connect(): ", errno);
        close(_sendSocket);
        _sendSocket = -1;
        _sendSocket_mutex.unlock();
        return ChordConnectionStatusConnectingFailed; // we cannot connect
    }
    
    // identify ourself
    try {
        sendRequest(ChordMessageTypeIdentify, NULL, 0);
    } catch (ChordConnectionException &exception) {
        // send failed
        Log::sharedLog()->error(std::string("ChordNode::establishSendConnection():identify ") += exception.what());
        close(_sendSocket);
        _sendSocket = -1;
        _sendSocket_mutex.unlock();
        return ChordConnectionStatusConnectingFailed; // we cannot connect
    }
    
    _sendSocket_mutex.unlock();
    return ChordConnectionStatusSuccessfullyConnected;
}

// send connection can be close
// (f.e. we have a new successor and don't need to keep the connection alive anymore)
void ChordNode::closeSendConnection ()
{
    _sendSocket_mutex.lock();
    if (_sendSocket > 0) {
        
        close(_sendSocket);
        _sendSocket = -1;
    }
    _sendSocket_mutex.unlock();
}

// tell's remote node that i'm his predecessor
// this method is used by stablilization
// returns node that the remote node returns
ChordHeaderNode ChordNode::getPredecessorFromRemoteNode (std::shared_ptr<ChordNode> ownNode)
{
    ChordHeaderNode pred = ownNode->chordNode();
    _sendSocket_mutex.lock();
    // update predecessor with own node
    try {
        std::shared_ptr<uint8_t> sendData {new uint8_t[sizeof(ChordHeaderNode)], std::default_delete<uint8_t[]>()};
        memcpy(sendData.get(), &pred, sizeof(ChordHeaderNode));
        
        sendRequest(ChordMessageTypeUpdatePredecessor, sendData, sizeof(ChordHeaderNode));
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::getPredecessorFromRemoteNode():sendRequest(): ") += exception.what());
        _sendSocket_mutex.unlock();
        throw ChordConnectionException { "couldn't send request" };
    }
    
    // receive the answer
    std::shared_ptr<ChordMessageType> responseType;
    std::shared_ptr<uint8_t> data;
    std::shared_ptr<ssize_t> dataSize { 0 };
    
    try {
        data = recvResponse(responseType, dataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::getPredecessorFromRemoteNode():recvResponse(): ") += exception.what());;
        _sendSocket_mutex.unlock();
        throw ChordConnectionException { "couldn't receive response" };
    }
    _sendSocket_mutex.unlock();
    
    // check for available data
    if (*dataSize == sizeof(ChordHeaderNode)) {
        
        ChordHeaderNode receivedNode { 0 };
        if (!data) {
            Log::sharedLog()->error("responseData == nullptr");
        } else {
            memcpy(&receivedNode, &data, sizeof(ChordHeaderNode));
        }
        return receivedNode;
        
    } else {
        Log::sharedLog()->error("answer contains unexpected data size");
        throw ChordConnectionException { "answer contains unexpected data size" };
    }
}

// search for a key (or a node)
ChordHeaderNode ChordNode::searchForKey (ChordId key)
{
    ChordId searchKey { htons(key) }; // convert key to network byte order
    
    std::shared_ptr<uint8_t> searchData { new uint8_t[sizeof(ChordId)], std::default_delete<uint8_t[]>() };
    memcpy(&searchData, &searchKey, sizeof(ChordId));
    
    // send search
    _sendSocket_mutex.lock();
    try {
        sendRequest(ChordMessageTypeSearch, searchData, sizeof(ChordId));
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::searchForKey():sendRequest(): ") += exception.what());
    }
    
    // receive result
    std::shared_ptr<ChordMessageType> responseType;
    std::shared_ptr<uint8_t> responseData;
    std::shared_ptr<ssize_t> responseDataSize { 0 };
    
    try {
        responseData = recvResponse(responseType, responseDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::searchForKey():recvResponse(): ") += exception.what());
    }
    _sendSocket_mutex.unlock();
    
    switch (*responseType) {
        case ChordMessageTypeSearchNodeResponse:
        {
            // check for available data
            if (*responseDataSize == sizeof(ChordHeaderNode)) {
                
                ChordHeaderNode receivedNode { 0 };
                if (!responseData) {
                    Log::sharedLog()->error("responseData == nullptr");
                } else {
                    memcpy(&receivedNode, &responseData, sizeof(ChordHeaderNode));
                }
                
                return receivedNode;
                
            } else {
                Log::sharedLog()->error("answer contains unexpected data size");
                throw ChordConnectionException { "answer contains unexpected data size" };
            }
            break;
        }
            
        default:
        {
            Log::sharedLog()->error(std::string("received unexpected answer type: ") += std::to_string(*responseType));
            throw ChordConnectionException { "received unexpected answer: " };
            break;
        }
    }
}

// receive data for key - nullptr if data not found
std::shared_ptr<uint8_t> ChordNode::requestDataForKey (ChordId key)
{
    ChordId dataKey { htons(key) }; // convert key to network byte order
    
    std::shared_ptr<uint8_t> requestData { new uint8_t[sizeof(ChordId)], std::default_delete<uint8_t[]>() };
    memcpy(&requestData, &dataKey, sizeof(ChordId));
    
    // send search
    _sendSocket_mutex.lock();
    try {
        sendRequest(ChordMessageTypeDataRequest, requestData, sizeof(ChordId));
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::requestDataForKey():sendRequest(): ") += exception.what());
    }
    
    // receive the data
    std::shared_ptr<ChordMessageType> responseType;
    std::shared_ptr<uint8_t> responseData;
    std::shared_ptr<ssize_t> responseDataSize { 0 };
    
    try {
        responseData = recvResponse(responseType, responseDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::requestDataForKey():recvResponse(): ") += exception.what());
    }
    _sendSocket_mutex.unlock();
    
    switch (*responseType) {
        case ChordMessageTypeDataAnswer:
        {
            // check for available data
            if (responseDataSize > 0) {
                
                if (!responseData) {
                    Log::sharedLog()->error("ChordNode::requestDataForKey(): responseData == nullptr");
                } else {
                    
                    return responseData;
                }
                
            } else {
                Log::sharedLog()->error("answer contains no data");
                throw ChordConnectionException { "answer contains no data" };
            }
            break;
        }
            
        case ChordMessageTypeDataNotFound:
        {
            Log::sharedLog()->error("ChordNode::requestDataForKey(): received data not found from remote node");
            break;
        }
            
        default:
        {
            Log::sharedLog()->error(std::string("received unexpected answer type: ") += std::to_string(*responseType));
            throw ChordConnectionException { "received unexpected answer: " };
            break;
        }
    }
    
    return nullptr;
}

// sends the data to the remote node to add it there locally
// returns true on success
bool ChordNode::addData (std::shared_ptr<ChordData> data)
{
    // protect the send socket
    _sendSocket_mutex.lock();
    
    std::shared_ptr<uint8_t> binaryData { (*data).serializedData() };
    
    // get data size from binary
    uint8_t binaryDataSizeNBO[4] { 0 };
    memcpy(&binaryDataSizeNBO, &binaryData, 4 * sizeof(uint8_t));
    uint32_t binaryDataSize { ntohl(static_cast<uint32_t>(*binaryDataSizeNBO)) };
    
    // send the data
    try {
        sendRequest(ChordMessageTypeDataAdd, binaryData, binaryDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::addData():sendRequest(): ") += exception.what());
    }
    
    std::shared_ptr<ChordMessageType> responseType;
    std::shared_ptr<uint8_t> responseData;
    std::shared_ptr<ssize_t> responseDataSize { 0 };
    
    // receive answer
    try {
        responseData = recvResponse(responseType, responseDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::addData():recvResponse(): ") += exception.what());
    }
    
    // we are done with the send socket
    _sendSocket_mutex.unlock();
    
    if (*responseType == ChordMessageTypeDataAddSuccess) {
        return true;
    }
    
    return false;
}

// returns a describing string of the node
std::string ChordNode::description () const
{
    std::stringstream description;
    
    description << "{ Node ID: " << _nodeID << " IP: " << _ipAddress << " Port: " << _port << " }";
    
    return description.str();
}

#pragma mark - Private

void ChordNode::setReceiveSocket (int socket)
{
    if (_receiveSocket != -1) {
        Log::sharedLog()->error(std::string("warning receivesocket for node: ")
                                += std::to_string(_nodeID)
                                += " was already set");
    }
    
    // stop request handler if already started
    _stopRequestHandlerThread = true;
    try {
        // wait till request handler finished
        _requestHandlerThread.join();
        
        // called if requestHandlerThread is not running (or not joinable)
    } catch (...) {}
    
    _stopRequestHandlerThread = false;
    _receiveSocket = socket;
    
    // start request handler
    if (!_requestHandlerThread.joinable()) { // only if it's not already running
        _requestHandlerThread = std::thread(&ChordNode::handleRequests, this);
    }
}

void ChordNode::handleRequests ()
{
    RGPLOGV("ChordNode handleRequest");
    
    // receive request
    ChordHeader requestHeader { {0, 0, 0}, ChordMessageTypeIdentify, 0 };
    
    std::shared_ptr<uint8_t> data { nullptr };
    ssize_t readBytes { 0 };
    
    while (!_stopRequestHandlerThread) {
        
        data.reset();
        
        // wait for incomming data
        if ((readBytes = recv(_receiveSocket, &requestHeader, sizeof(ChordHeader), 0)) <= 0) {
            if (readBytes == 0) {
                RGPLOGV("Remote Node closed connection");
                break;
            }
            Log::sharedLog()->errorWithErrno("ChordNode::handleRequests():recv1: ", errno);
            break;
        }
        
        // error check
        if (readBytes != sizeof(ChordHeader)) {
            Log::sharedLog()->error("don't received enough data ... something bad happened");
            continue;
        }
        
        // check for available data
        if (ntohl(requestHeader.dataSize) > 0) {
            
            // alloc space for the data
            uint32_t dataSize = ntohl(requestHeader.dataSize);
            data = std::shared_ptr<uint8_t>(new uint8_t[dataSize], std::default_delete<uint8_t[]>());
            
            // receive the data
            readBytes = recv(_receiveSocket, data.get(), ntohl(requestHeader.dataSize), 0);
            
            if (readBytes == 0) {
                RGPLOGV("Remote Node closed connection");
                break;
            }
            
            // error check
            if (readBytes < 0) {
                Log::sharedLog()->errorWithErrno("ChordNode::handleRequests():recv2: ", errno);
                continue;
            }
            
            if (readBytes != static_cast<ssize_t>(ntohl(requestHeader.dataSize))) {
                Log::sharedLog()->error((std::string("data size: ")
                                         += std::to_string(ntohl(requestHeader.dataSize))
                                         += " readBytes: ")
                                        += std::to_string(readBytes));
                Log::sharedLog()->error("recv(): don't received the expected data size");
                break;
            }
        }
        
        // check message type and react appropriate
        switch (requestHeader.type)
        {
                
            case ChordMessageTypeHeartbeat:
            {
                RGPLOGV(std::string("received Heartbeat message from: ") += std::to_string(_nodeID));
                // answer with heartbeat reply
                sendResponse(ChordMessageTypeHeartbeatReply, nullptr, 0);
                
                break;
            }
                
            case ChordMessageTypeSearch:
            {
                RGPLOGV("received Search message");
                
                // error check
                if (!data) {
                    Log::sharedLog()->error("received search without data ...");
                    break;
                }
                
                ChordId key = ntohl(*data);
                
                // search the key (checks local / sends search)
                ChordHeaderNode node = _chord->searchForKey(shared_from_this(), key);
                
                // create understandable response format
                std::shared_ptr<uint8_t> nodeData(new uint8_t[sizeof(ChordHeaderNode)], std::default_delete<uint8_t[]>());
                memcpy(nodeData.get(), &node, sizeof(ChordHeaderNode));
                
                // send response
                try {
                    
                    sendResponse(ChordMessageTypeSearchNodeResponse, nodeData, sizeof(ChordHeaderNode));
                    
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeUpdatePredecessor:
            {
                RGPLOGV(std::string("received Update Predecessor message from: ") += std::to_string(_nodeID));
                
                // Error checking
                if (!data) {
                    Log::sharedLog()->error("received update predecessor without data ...");
                    break;
                }
                
                if (ntohl(requestHeader.dataSize) != sizeof(ChordHeaderNode)) {
                    Log::sharedLog()->error("received update predecessor with unexpected data size ...");
                    break;
                }
                
                // apply new predecessor
                ChordHeaderNode node;
                memcpy(&node, data.get(), sizeof(ChordHeaderNode));
                ChordHeaderNode newPredecessor = _chord->updatePredecessor(node);
                
                // create understandable response format
                std::shared_ptr<uint8_t> nodeData(new uint8_t[sizeof(ChordHeaderNode)], std::default_delete<uint8_t[]>());
                memcpy(nodeData.get(), &newPredecessor, sizeof(ChordHeaderNode));
                
                // send answer
                try {
                    sendResponse(ChordMessageTypePredecessor, nodeData, sizeof(ChordHeaderNode));
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeDataAdd:
            {
                // someone wants to add data to us
                RGPLOGV("received add data message");
                
                // Error checking
                if (!data) {
                    Log::sharedLog()->error("received add data without data ...");
                    
                    // send answer
                    try {
                        sendResponse(ChordMessageTypeDataAddFailed, nullptr, 0);
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                    
                    break;
                }
                
                bool added = _chord->addDataToHashMap(data);
                
                try {
                    if (added) {
                        // send success answer
                        sendResponse(ChordMessageTypeDataAddSuccess, nullptr, 0);
                    } else {
                        // send failed answer
                        sendResponse(ChordMessageTypeDataAddFailed, nullptr, 0);
                    }
                    
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeDataRequest:
            {
                RGPLOGV("received data request message");
                
                if (!data) {
                    Log::sharedLog()->error("received data request without data ...");
                    break;
                }
                
                ChordId key = ntohl(*data);
                
                // search for the data
                std::shared_ptr<uint8_t> data = _chord->getDataWithKey(key);
                
                if (data) {
                    
                    uint32_t dataSize { ntohl(*data.get()) };
                    
                    // send response
                    try {
                        sendResponse(ChordMessageTypeDataAnswer, data, dataSize);
                        
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                    
                } else {
                    // send response
                    try {
                        sendResponse(ChordMessageTypeDataNotFound, nullptr, 0);
                        
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                }
                break;
            }
                
            default:
            {
                Log::sharedLog()->error(std::string("received unknown message type: ") += std::to_string(requestHeader.type));
                break;
            }
        }
    }
    
    RGPLOGV("close handlingThread");
    
    close(_receiveSocket);
    _receiveSocket = -1;
}

// sends response to remote node
// throws ChordConnectionException on error
void ChordNode::sendResponse (ChordMessageType type, std::shared_ptr<uint8_t> data, ssize_t dataSize)
{
    // data that will be send later (ChordHeader + the data)
    uint8_t *message = new uint8_t[sizeof(ChordHeader) + dataSize];
    
    // header
    ChordHeader header = _chord->createChordHeader(type);
    header.dataSize = htonl(dataSize);
    
    // copy header to message
    memcpy(message, &header, sizeof(ChordHeader));
    
    if (dataSize > 0) {
        // copy data to message
        memcpy((message + sizeof(ChordHeader)), data.get(), dataSize);
    }
    
    // send message
    ssize_t bytesSend { 0 };
    if ((bytesSend = send(_receiveSocket, message, sizeof(ChordHeader) + dataSize, 0)) <= 0) {
        
        // memory management
        delete [] message; message = nullptr;
        
        // check if connection was closed
        if (bytesSend == 0) {
            Log::sharedLog()->error("Remote Node closed connection");
            throw ChordConnectionException { "Remote Node closed connection" };
        }
        
        // check if send failed
        if (bytesSend == -1) {
            Log::sharedLog()->errorWithErrno("ChordNode::sendResponse():send() ", errno);
            throw ChordConnectionException { "Error sending data to remote node" };
        }
    }
    
    // memory management
    if (message != nullptr) {
        delete [] message; message = nullptr;
    }
}

// sends request to remote node
// throws ChordConnectionException on error
void ChordNode::sendRequest (ChordMessageType type, std::shared_ptr<uint8_t> data, ssize_t dataSize)
{
    // data that will be send later (ChordHeader + the data)
    uint8_t *message = new uint8_t[sizeof(ChordHeader) + dataSize];
    
    // header
    ChordHeader header = _chord->createChordHeader(type);
    header.dataSize = htonl(dataSize);
    
    // copy header to message
    memcpy(message, &header, sizeof(ChordHeader));
    
    if (dataSize > 0) {
        // copy data to message
        memcpy((message + sizeof(ChordHeader)), data.get(), dataSize);
    }
    
    // send message
    ssize_t bytesSend { 0 };
    if ((bytesSend = send(_sendSocket, message, sizeof(ChordHeader) + dataSize, 0)) <= 0) {
        
        delete [] message;
        message = nullptr;
        
        // check if connection was closed
        if (bytesSend == 0) {
            Log::sharedLog()->error("Remote Node closed connection");
            close(_sendSocket);
            _sendSocket = -1;
            throw ChordConnectionException { "Remote Node closed connection" };
        }
        
        // check if send failed
        if (bytesSend == -1) {
            Log::sharedLog()->errorWithErrno("ChordNode::sendRequest():send() ", errno);
            throw ChordConnectionException { "Error sending data to remote node" };
        }
    }
    
    // memory management
    if (message != nullptr) {
        delete [] message;
    }
}

// receives response from remote node
// throws ChordConnectionException on error
std::shared_ptr<uint8_t> ChordNode::recvResponse (std::shared_ptr<ChordMessageType> type, std::shared_ptr<ssize_t> dataSize)
{
    // receive answer
    ssize_t readBytes { 0 };
    ChordHeader responseHeader;
    
    if ((readBytes = recv(_sendSocket, &responseHeader, sizeof(ChordHeader), 0)) <= 0) {
        
        if (readBytes == 0) { // connection was closed
            Log::sharedLog()->error(std::string("Node with id: ") += std::to_string(_nodeID) += " closed the connection");
            close(_sendSocket);
            _sendSocket = -1;
            throw ChordConnectionException { "Node closed the connection" };
        }
        
        // error
        Log::sharedLog()->errorWithErrno("ChordNode::recvResponse():recv1() ", errno);
        throw ChordConnectionException { strerror(errno) };
    }
    
    // error check
    if (readBytes != sizeof(ChordHeader)) {
        Log::sharedLog()->error("don't received enough data ... something bad happened");
        throw ChordConnectionException { "don't received enough data ... something bad happened" };
    }
    
    *type = static_cast<ChordMessageType>(responseHeader.type);
    *dataSize = ntohl(responseHeader.dataSize);
    
    if (dataSize > 0) {
        std::shared_ptr<uint8_t> data(new uint8_t[*dataSize], std::default_delete<uint8_t[]>());
        
        // receive the data
        if ((readBytes = recv(_sendSocket, &data, *dataSize, 0)) <= 0) {
            if (readBytes == 0) {
                Log::sharedLog()->error(std::string("Node with id: ") += std::to_string(_nodeID) += " closed the connection");
                close(_sendSocket);
                _sendSocket = -1;
                throw ChordConnectionException { "Node closed the connection" };
            }
            Log::sharedLog()->errorWithErrno("ChordNode::recvResponse():recv2() ", errno);
            throw ChordConnectionException { strerror(errno) };
        }
        
        // error check
        if (readBytes != *dataSize) {
            Log::sharedLog()->error("don't received enough data ... something bad happened");
            throw ChordConnectionException { "don't received enough data ... something bad happened" };
        }
        
        return data;
    }
    
    return nullptr;
}
