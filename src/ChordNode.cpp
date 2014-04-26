/*
 ChordNode.cpp
 Chord

 Created by Ralph-Gordon Paul on 13.06.13.
 
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
#include <rgp/Chord.h>
#include <rgp/Log.h>

#include <sstream>

// standard c
#include <unistd.h>
#include <cstring>

// network
#include <arpa/inet.h>
#include <netdb.h>

using namespace rgp;

ChordNode::ChordNode (uint16_t id, std::string ip, uint16_t port, Chord *chord)
: nodeID(id), ipAddress(ip), port(port), chord(chord)
{
}

ChordNode::~ChordNode()
{
    // stop request handler
    stopRequestHandlerThread = true;
    try {
        // wait till request handler finished
        requestHandlerThread.join();
    } catch (...) {} // called if requestHandlerThread is not running (or not joinable)
    
    // disconnect sendSocket if connected
    if (this->sendSocket != -1) {
        close(this->sendSocket);
        this->sendSocket = -1;
    }
}

#pragma mark - Private

void ChordNode::setReceiveSocket(int socket)
{
    if (receiveSocket != -1) {
        Log::sharedLog()->error(std::string("warning receivesocket for node: ") += std::to_string(nodeID) += " was already set");
    }
    
    // stop request handler if already started
    stopRequestHandlerThread = true;
    try {
        // wait till request handler finished
        requestHandlerThread.join();
    } catch (...) {} // called if requestHandlerThread is not running (or not joinable)
    
    stopRequestHandlerThread = false;
    receiveSocket = socket;
    
    // start request handler
    if (!this->requestHandlerThread.joinable()) { // only if it's not already running
        this->requestHandlerThread = std::thread(&ChordNode::handleRequests, this);
    }
}

void ChordNode::handleRequests()
{
    Log::sharedLog()->printv("ChordNode handleRequest");
    // receive request
    ChordHeader_t requestHeader { {0, 0, 0}, 0, 0 };
    
    char *data { nullptr };
    ssize_t readBytes { 0 };
    
    while (!stopRequestHandlerThread) {
        
        // wait for incomming data
        if ((readBytes = recv(this->receiveSocket, &requestHeader, sizeof(ChordHeader_t), 0)) <= 0) {
            if (readBytes == 0) {
                Log::sharedLog()->printv("Remote Node closed connection");
                break;
            }
            Log::sharedLog()->errorWithErrno("ChordNode::handleRequests():recv1: ", errno);
            break;
        }
        
        // error check
        if (readBytes != sizeof(ChordHeader_t)) {
            Log::sharedLog()->error("don't received enough data ... something bad happened");
            continue;
        }
        
        // check for available data
        if (ntohl(requestHeader.dataSize) > 0) {
            
            // alloc space for the data
            uint32_t dataSize = ntohl(requestHeader.dataSize);
            data = new char[dataSize];
            
            // receive the data
            readBytes = recv(this->receiveSocket, data, ntohl(requestHeader.dataSize), 0);
            
            if (readBytes == 0) {
                Log::sharedLog()->printv("Remote Node closed connection");
                break;
            }
            
            // error check
            if (readBytes < 0) {
                Log::sharedLog()->errorWithErrno("ChordNode::handleRequests():recv2: ", errno);
                continue;
            }
            
            if (readBytes != static_cast<ssize_t>(ntohl(requestHeader.dataSize))) {
                Log::sharedLog()->error((std::string("data size: ") += std::to_string(ntohl(requestHeader.dataSize))
                                               += " readBytes: ") += std::to_string(readBytes));
                Log::sharedLog()->error("recv(): don't received the expected data size");
                break;
            }
        }
        
        // check message type and react appropriate
        switch (requestHeader.type)
        {
                
            case ChordMessageTypeHeartbeat:
            {
                Log::sharedLog()->printv(std::string("received Heartbeat message from: ") += std::to_string(nodeID));
                // answer with heartbeat reply
                sendResponse(ChordMessageTypeHeartbeatReply, nullptr, 0);

                break;
            }
                
            case ChordMessageTypeSearch:
            {
                Log::sharedLog()->printv("received Search message");
                
                // error check
                if (data == nullptr) {
                    Log::sharedLog()->error("received search without data ...");
                    break;
                }
                
                DataID_t *key = reinterpret_cast<DataID_t *>(data);
                
                // search the key (checks local / sends search)
                ChordNode_t node = chord->searchForKey(this, ntohs(*key));
                
                // send response
                try {
                    
                    sendResponse(ChordMessageTypeSearchNodeResponse, reinterpret_cast<char *>(&node), sizeof(ChordNode_t));
                    
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeUpdatePredecessor:
            {
                Log::sharedLog()->printv(std::string("received Update Predecessor message from: ") += std::to_string(nodeID));
                
                // Error checking
                if (data == nullptr) {
                    Log::sharedLog()->error("received update predecessor without data ...");
                    break;
                }
                
                if (ntohl(requestHeader.dataSize) != sizeof(ChordNode_t)) {
                    Log::sharedLog()->error("received update predecessor with unexpected data size ...");
                    break;
                }
                
                // apply new predecessor
                ChordNode_t *node = reinterpret_cast<ChordNode_t *>(data);
                ChordNode_t newPredecessor = chord->updatePredecessor(*node);
                
                // send answer
                try {
                    sendResponse(ChordMessageTypePredecessor, reinterpret_cast<char *>(&newPredecessor), sizeof(ChordNode_t));
                } catch (ChordConnectionException &exception) {
                    Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                }
                
                break;
            }
                
            case ChordMessageTypeDataAdd:
            {
                // someone wants to add data to us
                Log::sharedLog()->printv("received add data message");
                
                // Error checking
                if (data == nullptr) {
                    Log::sharedLog()->error("received add data without data ...");
                    
                    // send answer
                    try {
                        sendResponse(ChordMessageTypeDataAddFailed, nullptr, 0);
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                    
                    break;
                }
                
                bool added = chord->addDataToHashMap(data);
                
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
                Log::sharedLog()->printv("received data request message");
                
                if (data == nullptr) {
                    Log::sharedLog()->error("received data request without data ...");
                    break;
                }
                
                DataID_t *key = reinterpret_cast<DataID_t *>(data);
                
                // search for the data
                ChordData *data = chord->getDataWithKey(ntohs(*key));
                
                if (data != nullptr) {
                    
                    // copy data to char[] to be able to send the data
                    ssize_t dataSize = strlen(data->getData().c_str());
                    dataSize++; // terminal symbol
                    char *dataString = new char[dataSize];
                    memcpy(dataString, data->getData().c_str(), dataSize);
                    
                    // send response
                    try {
                        sendResponse(ChordMessageTypeDataAnswer, dataString, dataSize);
                        
                    } catch (ChordConnectionException &exception) {
                        Log::sharedLog()->error(std::string("Error sending response: ") += exception.what());
                    }
                    
                    delete [] dataString;
                    
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
        
        // memory management
        if (data != nullptr) {
            delete [] data; data = nullptr;
        }
    }
    
    // memory management
    if (data != nullptr) {
        delete [] data; data = nullptr;
    }
    
    Log::sharedLog()->printv("close handlingThread");
    
    close(this->receiveSocket);
    this->receiveSocket = -1;
}

// sends response to remote node
// throws ChordConnectionException on error
void ChordNode::sendResponse(ChordMessageType type, char *data, ssize_t dataSize)
{
    // data that will be send later (ChordHeader + the data)
    char *message = new char[sizeof(ChordHeader_t) + dataSize];
    
    // header
    ChordHeader_t *header = chord->createChordHeader(type);
    header->dataSize = htonl(dataSize);
    
    // copy header to message
    memcpy(message, header, sizeof(ChordHeader_t));
    
    if (dataSize > 0) {
        // copy data to message
        memcpy((message + sizeof(ChordHeader_t)), data, dataSize);
    }
    
    // memory management
    delete header; header = nullptr;
    
    // send message
    ssize_t bytesSend { 0 };
    if ((bytesSend = send(this->receiveSocket, message, sizeof(ChordHeader_t) + dataSize, 0)) <= 0) {
        
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
void ChordNode::sendRequest(ChordMessageType type, char *data, ssize_t dataSize)
{
    // data that will be send later (ChordHeader + the data)
    char *message = (char *)malloc(sizeof(ChordHeader_t) + dataSize);
    
    // header
    ChordHeader_t *header = chord->createChordHeader(type);
    header->dataSize = htonl(dataSize);
    
    // copy header to message
    memcpy(message, header, sizeof(ChordHeader_t));
    
    if (dataSize > 0) {
        // copy data to message
        memcpy((message + sizeof(ChordHeader_t)), data, dataSize);
    }
    
    // memory management
    delete header; header = nullptr;
    
    // send message
    ssize_t bytesSend { 0 };
    if ((bytesSend = send(this->sendSocket, message, sizeof(ChordHeader_t) + dataSize, 0)) <= 0) {
        
        free(message);
        message = NULL;
        
        // check if connection was closed
        if (bytesSend == 0) {
            Log::sharedLog()->error("Remote Node closed connection");
            close(this->sendSocket);
            this->sendSocket = -1;
            throw ChordConnectionException { "Remote Node closed connection" };
        }
        
        // check if send failed
        if (bytesSend == -1) {
            Log::sharedLog()->errorWithErrno("ChordNode::sendRequest():send() ", errno);
            throw ChordConnectionException { "Error sending data to remote node" };
        }
    }
    
    // memory management
    if (message != NULL) {
        free(message);
    }
}

// receives response from remote node
// throws ChordConnectionException on error
char *ChordNode::recvResponse(ChordMessageType *type, ssize_t *dataSize)
{
    // receive answer
    ssize_t readBytes { 0 };
    ChordHeader_t responseHeader;
    
    if ((readBytes = recv(this->sendSocket, &responseHeader, sizeof(ChordHeader_t), 0)) <= 0) {
        
        if (readBytes == 0) { // connection was closed
            Log::sharedLog()->error(std::string("Node with id: ") += std::to_string(this->nodeID) += " closed the connection");
            close(this->sendSocket);
            this->sendSocket = -1;
            throw ChordConnectionException { "Node closed the connection" };
        }
        
        // error
        Log::sharedLog()->errorWithErrno("ChordNode::recvResponse():recv1() ", errno);
        throw ChordConnectionException { strerror(errno) };
    }
    
    // error check
    if (readBytes != sizeof(ChordHeader_t)) {
        Log::sharedLog()->error("don't received enough data ... something bad happened");
        throw ChordConnectionException { "don't received enough data ... something bad happened" };
    }
    
    *type = static_cast<ChordMessageType>(responseHeader.type);
    *dataSize = ntohl(responseHeader.dataSize);
    
    if (*dataSize > 0) {
        char *data = new char[*dataSize];
        
        // receive the data
        if ((readBytes = recv(this->sendSocket, data, *dataSize, 0)) <= 0) {
            if (readBytes == 0) {
                Log::sharedLog()->error(std::string("Node with id: ") += std::to_string(this->nodeID) += " closed the connection");
                close(this->sendSocket);
                this->sendSocket = -1;
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

#pragma mark - Public

// returns this node as struct ChordNode_t
ChordNode_t ChordNode::chordNode_t() const
{
    ChordNode_t node {0, 0, 0};
    
    node.nodeID = htons(nodeID);
    node.ip = htonl(inet_addr(ipAddress.c_str()));
    node.port = htons(port);
    
    return node;
}

bool ChordNode::isAlive()
{
    // if we have an open send connection - check if remote node is alive
    if (this->sendSocket > 0) {
        
        sendSocket_mutex.lock(); // we use send socket to send data and receive the response
        try {
            // Hearbeat
            sendRequest(ChordMessageTypeHeartbeat, nullptr, 0);
            
        } catch (ChordConnectionException &exception) {
            // error
            Log::sharedLog()->error(std::string("ChordNode::isAlive(): coulnd't send request: ") += exception.what());
            
            close(this->sendSocket);
            this->sendSocket = -1;
        }
        
        // receive heartbeat answer
        if (this->sendSocket > 0) {
            
            try {
                
                ChordMessageType type;
                ssize_t dataSize;
                
                char *data = recvResponse(&type, &dataSize);
                
                // memory management -> there shouldn't be received data
                if (data != nullptr) {
                    delete [] data;
                }
                
                // success -> node is alive
                if (type == ChordMessageTypeHeartbeatReply) {
                    sendSocket_mutex.unlock();
                    return true;
                }
                
            } catch (ChordConnectionException &exception) {
                // error
                Log::sharedLog()->error(std::string("ChordNode::isAlive(): coulnd't receive request: ") += exception.what());
                
                close(this->sendSocket);
                this->sendSocket = -1;
            }
        }
        sendSocket_mutex.unlock();
    }
    
    // if receive connection is alive - we don't need a hearbeat for this
    if (this->receiveSocket > 0) {
        return true;
    }
    
    return false;
}

ChordConnectionStatus ChordNode::establishSendConnection()
{
    // check required properties
    if (ipAddress.compare("") == 0 || port == 0) {
        return ChordConnectionStatusConnectingFailed;
    }
    
    sendSocket_mutex.lock();
    
    // check if already connected
    if (this->sendSocket != -1) {
        sendSocket_mutex.unlock();
        return ChordConnectionStatusAlreadyConnected;
    }
    
    // create socket
    this->sendSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    // create sockaddr
    struct sockaddr_in addr4client;
    memset(&addr4client, 0, sizeof(addr4client)); // fill struct with zeros
#ifndef __linux__
    addr4client.sin_len = sizeof(addr4client);
#endif
    addr4client.sin_family = AF_INET;
    addr4client.sin_port = htons(port);
    
    if((addr4client.sin_addr.s_addr = inet_addr(ipAddress.c_str())) == (unsigned long)INADDR_NONE)
    {
        // ERROR: INADDR_NONE - try with hostname
        struct hostent *hostp = gethostbyname(ipAddress.c_str());
        if (hostp == NULL) {
            Log::sharedLog()->errorWithErrno("ChordNode::establishSendConnection():hostent failed - cannot use given host address ", errno);
            close(this->sendSocket);
            this->sendSocket = -1;
            sendSocket_mutex.unlock();
            return ChordConnectionStatusConnectingFailed; // we cannot connect
        } else {
            memcpy(&addr4client.sin_addr, hostp->h_addr, sizeof(addr4client.sin_addr));
        }
    }
    
    // connect
    if (connect(this->sendSocket, (struct sockaddr*)&addr4client, sizeof(struct sockaddr_in)) != 0) {
        Log::sharedLog()->errorWithErrno("ChordNode::establishSendConnection():connect(): ", errno);
        close(this->sendSocket);
        this->sendSocket = -1;
        sendSocket_mutex.unlock();
        return ChordConnectionStatusConnectingFailed; // we cannot connect
    }
    
    // identify ourself
    try {
        sendRequest(ChordMessageTypeIdentify, NULL, 0);
    } catch (ChordConnectionException &exception) {
        // send failed
        Log::sharedLog()->error(std::string("ChordNode::establishSendConnection():identify ") += exception.what());
        close(this->sendSocket);
        this->sendSocket = -1;
        sendSocket_mutex.unlock();
        return ChordConnectionStatusConnectingFailed; // we cannot connect
    }
    
    sendSocket_mutex.unlock();
    return ChordConnectionStatusSuccessfullyConnected;
}

// send connection can be close
// (f.e. we have a new successor and don't need to keep the connection alive anymore)
void ChordNode::closeSendConnection()
{
    sendSocket_mutex.lock();
    if (sendSocket > 0) {
        
        close(sendSocket);
        sendSocket = -1;
    }
    sendSocket_mutex.unlock();
}

// tell's remote node that i'm his predecessor
// this method is used by stablilization
// returns node that the remote node returns
ChordNode_t ChordNode::getPredecessorFromRemoteNode(ChordNode *ownNode)
{
    ChordNode_t pred = ownNode->chordNode_t();
    sendSocket_mutex.lock();
    // update predecessor with own node
    try {
        sendRequest(ChordMessageTypeUpdatePredecessor, reinterpret_cast<char *>(&pred), sizeof(ChordNode_t));
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::getPredecessorFromRemoteNode():sendRequest(): ") += exception.what());
        sendSocket_mutex.unlock();
        throw ChordConnectionException { "couldn't send request" };
    }
    
    // receive the answer
    ChordMessageType responseType;
    void *data { nullptr };
    ssize_t dataSize { 0 };
    
    try {
        data = recvResponse(&responseType, &dataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::getPredecessorFromRemoteNode():recvResponse(): ") += exception.what());;
        sendSocket_mutex.unlock();
        throw ChordConnectionException { "couldn't receive response" };
    }
    sendSocket_mutex.unlock();
    
    // check for available data
    if (dataSize == sizeof(ChordNode_t)) {
        
        ChordNode_t *tempNode { nullptr };
        ChordNode_t receivedNode {0, 0, 0};
        if (data == nullptr) {
            Log::sharedLog()->error("responseData == nullptr");
        } else {
            // copy to temp to be able to free the memory and return copy by reference
            tempNode = reinterpret_cast<ChordNode_t *>(data);
            receivedNode = *tempNode;
            
            delete tempNode; tempNode = nullptr; data = nullptr;
        }
        return receivedNode;
        
    } else {
        Log::sharedLog()->error("answer contains unexpected data size");
        throw ChordConnectionException { "answer contains unexpected data size" };
    }
}

// search for a key (or a node)
ChordNode_t ChordNode::searchForKey(DataID_t key)
{
    DataID_t searchKey { htons(key) }; // convert key to network byte order
    
    // send search
    sendSocket_mutex.lock();
    try {
        sendRequest(ChordMessageTypeSearch, reinterpret_cast<char *>(&searchKey), sizeof(DataID_t));
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::searchForKey():sendRequest(): ") += exception.what());
    }
    
    // receive result
    ChordMessageType responseType;
    void *responseData { nullptr };
    ssize_t responseDataSize { 0 };
    
    try {
        responseData = recvResponse(&responseType, &responseDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::searchForKey():recvResponse(): ") += exception.what());
    }
    sendSocket_mutex.unlock();
    
    switch (responseType) {
        case ChordMessageTypeSearchNodeResponse:
        {
            // check for available data
            if (responseDataSize == sizeof(ChordNode_t)) {
                
                ChordNode_t *tempNode { nullptr };
                ChordNode_t receivedNode {0, 0, 0};
                if (responseData == nullptr) {
                    Log::sharedLog()->error("responseData == nullptr");
                } else {
                    // copy to temp to be able to free the memory and return copy by reference
                    tempNode = reinterpret_cast<ChordNode_t *>(responseData);
                    receivedNode = *tempNode;
                    
                    delete tempNode; tempNode = nullptr;
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
            Log::sharedLog()->error(std::string("received unexpected answer type: ") += std::to_string(responseType));
            throw ChordConnectionException { "received unexpected answer: " };
            break;
        }
    }
}

// receive data for key - nullptr if data not found
ChordData *ChordNode::requestDataForKey(DataID_t key)
{
    DataID_t dataKey { htons(key) }; // convert key to network byte order
    
    // send search
    sendSocket_mutex.lock();
    try {
        sendRequest(ChordMessageTypeDataRequest, reinterpret_cast<char *>(&dataKey), sizeof(DataID_t));
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::requestDataForKey():sendRequest(): ") += exception.what());
    }
    
    // receive the data
    ChordMessageType responseType;
    char *responseData { nullptr };
    ssize_t responseDataSize { 0 };
    
    try {
        responseData = recvResponse(&responseType, &responseDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::requestDataForKey():recvResponse(): ") += exception.what());
    }
    sendSocket_mutex.unlock();
    
    switch (responseType) {
        case ChordMessageTypeDataAnswer:
        {
            // check for available data
            if (responseDataSize > 0) {
                
                if (responseData == nullptr) {
                    Log::sharedLog()->error("ChordNode::requestDataForKey(): responseData == nullptr");
                } else {
                    // create ChordData and return it
                    std::string dataString { responseData };
                    DataID_t dataHash { static_cast<DataID_t>(std::hash<std::string>()(dataString) % chord->highestID()) };
                    ChordData *data { new ChordData { dataHash, dataString } };
                    
                    // memory management
                    delete [] responseData; responseData = nullptr;
                    
                    return data;
                }
                
            } else {
                Log::sharedLog()->error("answer contains no data");
                throw ChordConnectionException { "answer contains no data" };
            }
            break;
        }
            
        case ChordMessageTypeDataNotFound:
        {
            // memory management
            if (responseData != nullptr) {
                delete [] responseData; responseData = nullptr;
            }
            
            Log::sharedLog()->error("ChordNode::requestDataForKey(): received data not found from remote node");
            break;
        }
            
        default:
        {
            // memory management
            if (responseData != nullptr) {
                delete [] responseData; responseData = nullptr;
            }
            
            Log::sharedLog()->error(std::string("received unexpected answer type: ") += std::to_string(responseType));
            throw ChordConnectionException { "received unexpected answer: " };
            break;
        }
    }
    
    // memory management
    if (responseData != nullptr) {
        delete [] responseData; responseData = nullptr;
    }
    
    return nullptr;
}

// sends the data to the remote node to add it there locally
// returns true on success
bool ChordNode::addData(std::string *data)
{
    // protect the send socket
    sendSocket_mutex.lock();
    
    // convert data to char array to be able to send it
    ssize_t dataSize = strlen(data->c_str());
    dataSize++; // terminal symbol
    char *dataString = new char [dataSize];
    memcpy(dataString, data->c_str(), dataSize);
    
    // send the data
    try {
        sendRequest(ChordMessageTypeDataAdd, dataString, dataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::addData():sendRequest(): ") += exception.what());
    }
    
    delete [] dataString;
    
    ChordMessageType responseType;
    char *responseData { nullptr };
    ssize_t responseDataSize { 0 };
    
    // receive answer
    try {
        responseData = recvResponse(&responseType, &responseDataSize);
    } catch (ChordConnectionException &exception) {
        Log::sharedLog()->error(std::string("ChordNode::addData():recvResponse(): ") += exception.what());
    }
    
    // we are done with the send socket
    sendSocket_mutex.unlock();
    
    // memory management
    if (responseData != nullptr) {
        delete [] responseData;
    }
    
    if (responseType == ChordMessageTypeDataAddSuccess) {
        return true;
    }
    
    return false;
}

// returns a describing string of the node
std::string ChordNode::description() const
{
    std::stringstream description;
    
    description << "{ Node ID: " << nodeID << " IP: " << ipAddress << " Port: " << port << " }";
    
    return description.str();
}
