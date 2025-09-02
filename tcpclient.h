#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    void setPort(std::string str)
    {
        m_port = str;
    }
    void setNetworkAddress(std::string str)
    {
        m_ipAddress = str;
    }
    void freeData()
    {
        if(m_data != NULL)
            delete[] m_data;
    }


public slots:
    void runClient();

signals:
    void clientClosed();
    void updateFrame();
    void ctrlMessageSent(bool x);

private:
    bool substringCheck(std::string a, std::string b, size_t *idx);
    std::string m_ipAddress;
    std::string m_port;
    char * m_data = NULL;
};

#endif // TCPCLIENT_H
