#include "tcpclient.h"

#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#include <QMutex>
#include <QThread>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex clientOnFlag_mutex;
extern bool clientOnFlag;

extern QMutex clientCtrlFlag_mutex;
extern bool clientCtrlFlag;


TcpClient::TcpClient(QObject *parent)
    : QObject{parent}
{}

void TcpClient::runClient()
{
    std::cout << "IP ADDRESS: " << m_ipAddress << std::endl;
    std::cout << "PORT: " << m_port << std::endl;

    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
        *ptr = NULL,
        hints;
    char buffer[512];
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed with error: " << iResult << std::endl;
        emit clientClosed();
        return;
    }

    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(m_ipAddress.c_str(), m_port.c_str(), &hints, &result);
    if ( iResult != 0 ) {
        std::cout << "getaddrinfo failed with error: " << iResult << std::endl;
        emit clientClosed();
        WSACleanup();
        return;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
                               ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
            emit clientClosed();
            WSACleanup();
            return;
        }

        // Connect to server.
        iResult = ::connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        std::cout << "Unable to connect to server!" << std::endl;
        emit clientClosed();
        WSACleanup();
        return;
    }


    int rows, cols, type;
    size_t step, idx;
    ssize_t valread;
    std::string ack = "ACK!";
    std::string fin = "FIN!";
    std::string pause = "PAUSE";
    std::string play = "PLAY";
    std::string hdr = "HDR:";
    std::string rsp;

    while(true)
    {
        clientOnFlag_mutex.lock();
        if(!clientOnFlag)
        {
            clientOnFlag_mutex.unlock();
            break;
        }
        clientOnFlag_mutex.unlock();


        valread = recv(ConnectSocket, buffer, 512,0);

        rsp = std::string((char*)buffer);



        if(substringCheck(rsp,hdr,&idx))
        {
            rsp = "";
            break;
        }
        else
        {
            memset(buffer, 0, sizeof(buffer));
            rsp = "";
        }
    }


    if((idx + 20) > 511)
    {
        std::cout << "HDR FRAME CLIPPED" << std::endl;
        emit clientClosed();
        return;
    }


    cols = static_cast<int>((buffer[idx + 4] << 24) | (buffer[idx + 5] << 16) | (buffer[idx + 6] << 8) | (unsigned char)buffer[7]);
    rows = static_cast<int>((buffer[idx + 8] << 24) | (buffer[idx + 9] << 16) | (buffer[idx + 10] << 8) |  (unsigned char)buffer[11]);
    step = static_cast<int>((buffer[idx + 12] << 24) | (buffer[idx + 13] << 16) | (buffer[idx + 14] << 8) | (unsigned char)buffer[15]);
    type = static_cast<int>((buffer[idx + 16] << 24) | (buffer[idx + 17] << 16) | (buffer[idx + 18] << 8) | (unsigned char)buffer[19]);


    std::cout << "cols: " << std::hex << cols << std::endl;
    std::cout << "rows: " << std::hex << rows << std::endl;
    std::cout << "step: " << std::hex << step << std::endl;
    std::cout << "type: " << std::hex << type << std::endl;





    size_t frameSize = (cols * rows * 3);

    std::cout << "s: " << std::dec << frameSize << std::endl;


    send(ConnectSocket, ack.c_str(), ack.size(), 0);

    // while(true)
    // {

    //     clientOnFlag_mutex.lock();
    //     if(!clientOnFlag)
    //     {
    //         clientOnFlag_mutex.unlock();
    //         break;
    //     }
    //     clientOnFlag_mutex.unlock();


    //     send(ConnectSocket, ack.c_str(), ack.size(), 0);

    //     valread = recv(ConnectSocket, buffer, 512,0);

    //     rsp = std::string((char*)buffer);

    //     if(substringCheck(rsp,ack,&idx))
    //         break;

    // }


    if(m_data != NULL)
        delete[] m_data;
    m_data = new char[frameSize];
    bool local_flg = false;
    bool stream_flg = true;

    while(true)
    {
        if(stream_flg)
        {
            char* ptr = m_data;
            size_t count = 0;
            while(count < frameSize)
            {
                valread = recv(ConnectSocket, ptr, 1500 , 0);

                ptr += valread;
                count += valread;

            }

            frame_mutex.lock();
            frame = cv::Mat(rows,cols,type,m_data,step);
            frame_mutex.unlock();
            emit updateFrame();

            clientOnFlag_mutex.lock();
            clientCtrlFlag_mutex.lock();
            if(!clientCtrlFlag && clientOnFlag)
                send(ConnectSocket, ack.c_str(), ack.size(), 0);
            clientCtrlFlag_mutex.unlock();
            clientOnFlag_mutex.unlock();
        }

        clientCtrlFlag_mutex.lock();
        if(clientCtrlFlag)
        {

            if(stream_flg)
            {
                stream_flg = false;
                send(ConnectSocket, pause.c_str(), pause.size(), 0);

            }
            else
            {
                stream_flg = true;
                send(ConnectSocket, play.c_str(), play.size(), 0);
            }
            emit ctrlMessageSent(stream_flg);
            clientCtrlFlag = false;
        }
        clientCtrlFlag_mutex.unlock();

        clientOnFlag_mutex.lock();
        if(!clientOnFlag)
        {
            send(ConnectSocket, fin.c_str(), fin.size(), 0);
            local_flg = true;
        }
        clientOnFlag_mutex.unlock();

        if(local_flg)
            break;


    }

    closesocket(ConnectSocket);
    WSACleanup();


    emit clientClosed();


}

bool TcpClient::substringCheck(std::string a, std::string b, size_t *idx)
{
    size_t pos = 0;

    if (a == b)
    {
        *idx = 0;
        return true;
    }
    pos = a.find(b);
    *idx = pos;
    if(pos != std::string::npos)
        return true;
    else
        return false;
}
