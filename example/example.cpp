/*
 example.cpp
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

#include <iostream>
#include <rgp/Chord.h>
#include <rgp/Log.h>

#include <sstream>
#include <arpa/inet.h>

using namespace rgp;

// prints the usage information (like --help)
void printUsage(const char *);

// checks if the given string is an ip-address string
bool isIPString(std::string);

// check if a string is an integer (used to check if port is an integer)
bool isInteger(std::string);

// the textual user interface
void tui(Chord &);

int main(int argc, const char * argv[])
{
    // all possible parameters
    std::string ipAddress {""};
    std::string port {""};
    std::string c_ipAddress {""};
    std::string c_port {""};
    
    bool isDaemonProcess { false };
    
    // read parameters
    for (int i=1; i < argc; i++) {
        std::string parameter { argv[i] };
        if (parameter == "-ip") {
            
            if (i < argc) {
                ipAddress = argv[++i];
            } else {
                // missing parameter
                printUsage(argv[0]);
                exit(EXIT_FAILURE);
            }
            
        } else if (parameter == "-port") {
            
            if (i < argc) {
                port = argv[++i];
            } else {
                // missing parameter
                printUsage(argv[0]);
                exit(EXIT_FAILURE);
            }
            
        } else if (parameter == "-cip") {
            
            if (i < argc) {
                c_ipAddress = argv[++i];
            } else {
                // missing parameter
                printUsage(argv[0]);
                exit(EXIT_FAILURE);
            }
            
        } else if (parameter == "-cport") {
            
            if (i < argc) {
                c_port = argv[++i];
            } else {
                // missing parameter
                printUsage(argv[0]);
                exit(EXIT_FAILURE);
            }
            
        } else if (parameter == "-v") { // verbose
            
            // set log level
            Log::sharedLog()->logLevel = 1;
            
        } else if (parameter == "-daemon") {
            
            isDaemonProcess = true;
            
        } else {
            
            // unknown input
            printUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    // check if required arguments are given
    if (ipAddress == "" || port == "") {
        
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // check ip address format
    if (!isIPString(ipAddress)) {
        Log::sharedLog()->print( "-ip: unvalid ip address" );
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // check c_ip address format
    if (c_ipAddress != "" ) {
        if (!isIPString(c_ipAddress)) {
            Log::sharedLog()->print( "-cip: unvalid ip address" );
            printUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    // check port address format
    if (!isInteger(port)) {
        Log::sharedLog()->print( "-port: invalid number or not a number at all" );
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // check c_ip address format
    if (c_port != "" ) {
        if (!isInteger(c_port)) {
            Log::sharedLog()->print( "-cport: invalid number or not a number at all" );
            printUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    // init the chord system
    Chord *chord { nullptr };
    if (c_ipAddress != "" && c_port != "") {
        Log::sharedLog()->printv("connect to existing dht");
        // connect to existing dht
        chord = new Chord { ipAddress, static_cast<uint16_t>(stoi(port)), c_ipAddress, static_cast<uint16_t>(stoi(c_port)) };
        
    } else {
        Log::sharedLog()->printv("create new dht");
        // create new dht
        chord = new Chord { ipAddress, static_cast<uint16_t>(stoi(port)) };
    }
    
    // start textual user interface
    if (isDaemonProcess) {
        chord->join();
    } else {
        tui(*chord);
    }
    
    // free memory
    delete chord; chord = nullptr;
    
    return EXIT_SUCCESS;
}

// check if given string is correct ipv4 address format
bool isIPString(std::string ip)
{
    struct in_addr pin;
    int result = inet_aton(ip.c_str(), &pin);
    
    if (result == 1) {
        return true;
    }
    
    return false;
}

// check if a string is an integer
bool isInteger(std::string number)
{
    // check for integer with converting the string to int
    try {
        stoi(number);
        
    } catch (...) { // if there is an error, it isn't a integer
        return false;
    }
    
    return true;
}

// print usage information to the output (if usage was wrong)
void printUsage(const char *programName)
{
    std::stringstream output { "Usage: " };
    output << programName
    <<
    " -ip ipAddress -port port [-cip ipAddress -cport port]\n" \
    "--------------------------------------------------------------\n" \
    "-ip: your IP-Adress\n" \
    "-port: your Port\n" \
    "-cip: IP-Adress of an existing node\n" \
    "-cport: Port of an existing node\n" \
    "-v: verbose mode (enables debug output)" \
    "-daemon: disables interaction (TUI)" \
    "--------------------------------------------------------------";
    Log::sharedLog()->print( output.str() );
}

/*
 1. Befehl:     put value       Speichert die Zeichenkette value im Chord-System.
 Ausgabe:       hash            Hash-Wert unter dem die Daten gespeichert wurden.
                node            Knoten (ID, IP und Port) auf dem Daten gespeichert wurden.
 
 2. Befehl:     get hash        Sucht die Daten (Zeichenkette) zu dem gegebenen Hashwert.
 Ausgabe:       value           Zeichenkette des gesuchten Hash-W ertes.
                node            Knoten (ID, IP und Port) von dem Daten gelesen wurden.
 
 3. Befehl:     list            Gibt alle lokal gespeicherten Daten mit Hashwerten aus.
 Ausgabe:       values          Die Daten.
*/

void tui(Chord &chord)
{
    std::string command { "" };
    
    while ( true ) {
        
        // read input into buffer array
        char buffer[1500];
        std::cin.getline(buffer, 1500);
        
        // create string from buffer
        command = std::string { buffer };
        
        if (command.compare(0, 4, "put ") == 0) { // PUT Command
            
            // check if there is a given value
            if (command.size() <= 4) {
                Log::sharedLog()->print("command needs a value");
                continue;
            }
            
            // get value from command
            std::string value = command.substr(4, command.npos);
            chord.addData(value);
            
        } else if (command.compare(0, 4, "get ") == 0) { // GET Command
            
            // check if there is a given hash
            if (command.size() <= 4) {
                Log::sharedLog()->print("command needs a hash");
                continue;
            }
            
            // get hash from command
            int hash = stoi(command.substr(4, command.npos));
            chord.printDataWithHash(hash);
            
        } else if (command.compare(0, 4, "list") == 0) { // LIST Command
            
            chord.printAllLocalData();
            
        } else if (command.compare(0, 6, "status") == 0) { // STATUS Command
            
            chord.printStatus();
            
        } else if (command.compare(0, 4, "exit") == 0 || command.compare(0, 4, "quit") == 0) { // Exit Programm
            
            // TODO: disconnect nicely from dht -> transfer data to successor
            break; // exit the while (true) command handling loop
            
        } else { // Unknown Command
            
            // print a usage information
            Log::sharedLog()->print( "\n" \
            "Unknown Command. Please use:\n" \
            "put value      save value into the dht\n" \
            "get hash       search for hash inside dht\n" \
            "list           list local stored values\n" \
            "status         print some info\n" \
            "exit or quit   exit programm\n" \
            "\n");
        }
    }
}

