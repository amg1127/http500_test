#include <QBuffer>
#include <QCoreApplication>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

class HttpServerObject : public QObject {
    Q_OBJECT
private:
    QTcpServer* httpServerSocket;
public:
    HttpServerObject (QObject* parent = Q_NULLPTR):
        QObject (parent),
        httpServerSocket (new QTcpServer (this)) {
        QObject::connect (this->httpServerSocket, &QTcpServer::newConnection, this, &HttpServerObject::newConnection);
        // Listen a random port and serve a HTTP server there
        if (! this->httpServerSocket->listen (QHostAddress::LocalHost)) {
            qFatal ("QTcpServer::listen(): %s", this->httpServerSocket->errorString().toLatin1().data());
        }
    }
    inline quint16 serverPort () {
        return (this->httpServerSocket->serverPort ());
    }
public slots:
    void newConnection () {
        QTcpSocket* client = this->httpServerSocket->nextPendingConnection ();
        QByteArray line;
        qulonglong contentLength = 0;

        // Read request headers
        do {
            while (! client->canReadLine ()) {
                client->waitForReadyRead ();
            }
            line = client->readLine().trimmed();
            qDebug ("C: %s", line.data());
            if (! qstrnicmp (line.data(), "Content-Length:", 15)) {
                contentLength = line.mid(15).trimmed().toULongLong ();
            }
        } while (! line.isEmpty());

        // Read request body if it is present
        if (contentLength) {
            qDebug ("C: (%llu additional bytes from request body)", contentLength);
            while (contentLength) {
                quint64 bytesAvailable = client->bytesAvailable ();
                if (bytesAvailable > contentLength) {
                    bytesAvailable = contentLength;
                }
                if (! bytesAvailable) {
                    client->waitForReadyRead ();
                    continue;
                }
                contentLength -= client->read(bytesAvailable).count();
            }
        }

        QThread::sleep (2);
        // Now answer the request
        line = "HTTP/1.1 500 Internal Server Error\r\n";
        client->write (line); client->flush (); qDebug ("S: %s", line.trimmed().data ());
        line = "X-HTTP-Reason: testing\r\n";
        client->write (line); client->flush (); qDebug ("S: %s", line.trimmed().data ());
        line = "Content-Type: text/plain; charset=utf8\r\n";
        client->write (line); client->flush (); qDebug ("S: %s", line.trimmed().data ());
        line = "Content-Length: 0\r\n";
        client->write (line); client->flush (); qDebug ("S: %s", line.trimmed().data ());
        line = "\r\n";
        client->write (line); client->flush (); qDebug ("S: %s", line.trimmed().data ());

        client->close ();
        client->deleteLater ();
    }
};

class HttpClientObject : public QObject {
    Q_OBJECT
private:
    quint16 _serverPort;
    QNetworkAccessManager* networkManager;
    QByteArray postData;
    QBuffer postDataBuffer;
public:
    HttpClientObject (QObject* parent = Q_NULLPTR):
        QObject (parent),
        networkManager (new QNetworkAccessManager (this)),
        postData ("animal=buffalo&plant=fern&fungus=mushroom"),
        postDataBuffer (&postData, this) {
    }
    void setServerPort (quint16 serverPort) {
        this->_serverPort = serverPort;
    }
public slots:
    void downloadProgress (qint64 bytesReceived, qint64 bytesTotal) {
        qDebug ("QNetworkReply::downloadProgress %lld / %lld", bytesReceived, bytesTotal);
    }
    void uploadProgress (qint64 bytesSent, qint64 bytesTotal) {
        qDebug ("QNetworkReply::uploadProgress %lld / %lld", bytesSent, bytesTotal);
    }
    void finished () {
        qDebug ("QNetworkReply::finished");
        this->postDataBuffer.close ();
        QTimer::singleShot (2000, this, &HttpClientObject::newRequest);
    }
    void newRequest () {
        QNetworkRequest httpRequest;
        httpRequest.setUrl (QStringLiteral("http://localhost:%1/test/").arg(this->_serverPort));
        httpRequest.setHeader (QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        httpRequest.setHeader (QNetworkRequest::ContentLengthHeader, this->postData.count ());
        if (! this->postDataBuffer.open (QIODevice::ReadOnly)) {
            qFatal ("QBuffer::open(): %s", this->postDataBuffer.errorString().toLatin1().data());
        }
        QNetworkReply* httpReply = this->networkManager->sendCustomRequest (httpRequest, "POST", &(this->postDataBuffer));
        QObject::connect (httpReply, &QNetworkReply::downloadProgress, this, &HttpClientObject::downloadProgress);
        QObject::connect (httpReply, &QNetworkReply::uploadProgress, this, &HttpClientObject::uploadProgress);
        QObject::connect (httpReply, &QNetworkReply::finished, this, &HttpClientObject::finished);
        QObject::connect (httpReply, &QNetworkReply::finished, httpReply, &QObject::deleteLater);
        qDebug ("--");
        qDebug ("HttpClientObject::newRequest");
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // Create the HTTP server
    HttpServerObject httpServer;
    quint16 port = httpServer.serverPort ();
    qDebug ("HTTP server is listening the TCP port %u...", port);

    // Move the server to a separate thread
    QThread serverThread;
    httpServer.moveToThread (&serverThread);
    serverThread.start ();

    // Create the HTTP client
    HttpClientObject httpClient;
    httpClient.setServerPort (port);

    // Let the endless loop start
    httpClient.newRequest ();
    return a.exec();
}

#include "main.moc"
