#include "tcpserver.h"

#include <QMutex>

#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex serverOnFlag_mutex;
extern bool serverOnFlag;

extern QMutex newDataFlag_mutex;
extern bool newDataFlg;

TcpServer::TcpServer(QObject *parent)
    : QObject{parent}
{}

void TcpServer::runServer()
{
    SOCKET_STATUS status = UNINITALIZED;
    char buffer[512];
    WSADATA wsaData;
    int iResult;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL;
    struct addrinfo hints;


    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0)
    {
        std::cout << "WSAStartup failed with error: " << iResult << std::endl;
        emit serverClosed();
        return;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, "8080", &hints, &result);
    if ( iResult != 0 )
    {
        std::cout << "getaddrinfo failed with error: " << iResult << std::endl;
        WSACleanup();
        emit serverClosed();
        return;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        emit serverClosed();
        return;
    }


    unsigned long nonBlocking = 1; // 1 for non-blocking, 0 for blocking
    if (ioctlsocket(ListenSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        std::cout << "ioctlsocket failed with error: " << WSAGetLastError() << std::endl;
        closesocket(ListenSocket);
        freeaddrinfo(result);
        WSACleanup();
        emit serverClosed();
        return;
        // Handle error
    }

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        emit serverClosed();
        return;
    }


    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        std::cout << "listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(ListenSocket);
        WSACleanup();
        emit serverClosed();
        return;
    }

    SOCKADDR_IN clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (true) {
        ClientSocket = accept(ListenSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
        serverOnFlag_mutex.lock();
        if(!serverOnFlag)
        {
            status = CLOSE_SIGNAL_RECEIVED;
            serverOnFlag_mutex.unlock();
            break;
        }
        serverOnFlag_mutex.unlock();

        if (ClientSocket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // No incoming connection at the moment, continue checking or do other work
                // Consider using select() or WSAPoll() to efficiently wait for events
                // without busy-waiting.
                // Example: Sleep for a short duration to avoid high CPU usage
                Sleep(10);
                continue;
            } else {
                std::cout << "accept failed with error: " << error << std::endl;
                status = ACCEPT_ERROR;
                break;
            }
        } else {
            // A new client connection has been accepted
            std::cout << "Accepted new client connection." << std::endl;
            status = CLIENT_CONNECTED;
            break;
            // Handle the new clientSocket (e.g., create a new thread for it)
        }
    }

    // No longer need server socket
    closesocket(ListenSocket);



    if(status == CLIENT_CONNECTED)
    {
        bool local_newDataFlg = false;
        bool hdr_flg = false;
        bool clientStreamFlag = true;
        std::string rsp;
        std::string ack = "ACK!";
        std::string fin = "FIN!";
        std::string pause = "PAUSE";
        std::string play = "PLAY";
        ssize_t valread;
        while(true)
        {
            serverOnFlag_mutex.lock();
            if(!serverOnFlag)
            {
                serverOnFlag_mutex.unlock();
                break;
            }
            serverOnFlag_mutex.unlock();


            newDataFlag_mutex.lock();
            if(newDataFlg)
                local_newDataFlg = true;
            else
                local_newDataFlg = false;
            newDataFlag_mutex.unlock();

            if(local_newDataFlg)
            {

                if(!hdr_flg)
                {
                    char data[20];
                    frame_mutex.lock();
                    buildMatHeader(frame, data);
                    frame_mutex.unlock();
                    send(ClientSocket, data, 20, 0);

                     valread = recv(ClientSocket, buffer, 512,0);
                    buffer[valread] = '\0';

                    if(valread)
                        std::cout << "buffer:" << buffer << std::endl;

                    rsp = std::string(buffer);
                    if (substringCheck(rsp,ack))
                    {
                        hdr_flg = true;
                        buffer[0] = '\0';
                        buffer[1] = '\0';
                        rsp = "";
                    }
                }

                if(hdr_flg)
                {

                    if(clientStreamFlag)
                    {
                        frame_mutex.lock();
                        bool dataSent = false;
                        u_char * ptr = frame.data;
                        size_t dataToSend;
                        while(!dataSent)
                        {
                            dataToSend = (frame.dataend - ptr);
                            //std::cout << "dataToSend:" << dataToSend << std::endl;
                            if(dataToSend > 1500)
                                dataToSend = 1500;
                            else
                                dataSent = true;

                            send(ClientSocket, (char*)ptr, dataToSend, 0);
                            ptr += dataToSend;
                        }
                        frame_mutex.unlock();

                        std::cout << "FRAME SENT" << std::endl;
                    }


                    ssize_t valread;
                    while (true)
                    {
                        serverOnFlag_mutex.lock();
                        if(!serverOnFlag)
                        {
                            serverOnFlag_mutex.unlock();
                            break;
                        }
                        serverOnFlag_mutex.unlock();

                        valread = recv(ClientSocket, buffer, 512,0);
                        buffer[valread] = '\0';
                        rsp = std::string(buffer);
                        if(rsp != "" && rsp != "ACK!")
                            std::cout << "rsp:" << rsp << std::endl;

                        if(!valread)
                            continue;

                        if(substringCheck(rsp,fin))
                        {
                            serverOnFlag_mutex.lock();
                            serverOnFlag = false;
                            serverOnFlag_mutex.unlock();
                            break;
                        }
                        else if(substringCheck(rsp,pause))
                        {
                            clientStreamFlag = false;
                            buffer[0] = '\0';
                            rsp = "";
                            break;
                        }
                        else if(substringCheck(rsp,play))
                        {
                            clientStreamFlag = true;
                            buffer[0] = '\0';
                            rsp = "";
                            break;
                        }
                        else if(substringCheck(rsp,ack))
                        {
                            rsp = "";
                            break;
                        }
                        memset(buffer,0,512);

                    }
                }

                local_newDataFlg = false;
                newDataFlag_mutex.lock();
                newDataFlg = false;
                newDataFlag_mutex.unlock();
            }
        }

        newDataFlag_mutex.lock();
        newDataFlg = false;
        newDataFlag_mutex.unlock();
    }
    else
        printSocketStatus(status);

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    emit serverClosed();

}

bool TcpServer::substringCheck(std::string a, std::string b)
{
    size_t pos = 0;

    if(a == b)
        return true;

    pos = a.find(b);

    if(pos != std::string::npos)
        return true;
    else
        return false;
}

void TcpServer::buildMatHeader(cv::Mat &src, char* data)
{

    int cols = src.cols;
    int rows = src.rows;
    size_t step = src.step[0];
    int type = src.type();

    data[0] = 'H';
    data[1] = 'D';
    data[2] = 'R';
    data[3] = ':';

    // std::cout << "cols: " << std::hex << cols << std::endl;
    // std::cout << "rows: " << std::hex << rows << std::endl;
    // std::cout << "step: " << std::hex << step << std::endl;
    // std::cout << "type: " << std::hex << type << std::endl;

    //columns little endian
    data[4] = (static_cast<uint8_t>(cols >> 24));
    data[5] = (static_cast<uint8_t>(cols >> 16));
    data[6] = (static_cast<uint8_t>(cols >> 8));
    data[7] = (static_cast<uint8_t>(cols));

    //rows little endian
    data[8] = (static_cast<uint8_t>(rows >> 24));
    data[9] = (static_cast<uint8_t>(rows >> 16));
    data[10] = (static_cast<uint8_t>(rows >> 8));
    data[11] = (static_cast<uint8_t>(rows));

    //step little endian
    data[12] = (static_cast<uint8_t>(step >> 24));
    data[13] = (static_cast<uint8_t>(step >> 16));
    data[14] = (static_cast<uint8_t>(step >> 8));
    data[15] = (static_cast<uint8_t>(step));

    //type little endian
    data[16] = (static_cast<uint8_t>(type >> 24));
    data[17] = (static_cast<uint8_t>(type >> 16));
    data[18] = (static_cast<uint8_t>(type >> 8));
    data[19] = (static_cast<uint8_t>(type));

}

void TcpServer::printSocketStatus(SOCKET_STATUS s)
{
    switch (s) {
    case CLIENT_CONNECTED:
        std::cout << "CLIENT_CONNECTED" << std::endl;
        break;
    case CLOSE_SIGNAL_RECEIVED:
        std::cout << "CLOSE_SIGNAL_RECEIVED" << std::endl;
        break;
    case UNINITALIZED:
        std::cout << "UNINITALIZED" << std::endl;
        break;
    case SELECT_ERROR:
        std::cout << "SELECT_ERROR" << std::endl;
        break;
    case ACCEPT_ERROR:
        std::cout << "ACCEPT_ERROR" << std::endl;
        break;

    default:
        std::cout << "UNKNOWN" << std::endl;
        break;
    }

}
