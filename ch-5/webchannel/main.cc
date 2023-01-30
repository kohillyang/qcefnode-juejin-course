#include <QtWebSockets/qwebsocketserver>
#include <QtWebSockets/qwebsocket.h>
#include <QObject>
#include <QtCore/QDebug>
#include <QWebChannelAbstractTransport>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QWebChannel>
class WebSocketTransport : public QWebChannelAbstractTransport {
    Q_OBJECT
  public:
    explicit WebSocketTransport(QWebSocket *socket) : m_socket(socket) {
        connect(socket, &QWebSocket::textMessageReceived, this,
                &WebSocketTransport::textMessageReceived);
        connect(socket, &QWebSocket::disconnected, this, &WebSocketTransport::deleteLater);
    }
    virtual ~WebSocketTransport() { m_socket->deleteLater(); }

    void sendMessage(const QJsonObject &message) override {
        QJsonDocument doc(message);
        m_socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    };

  private:
    void WebSocketTransport::textMessageReceived(const QString &messageData) {
        QJsonParseError error;
        QJsonDocument message = QJsonDocument::fromJson(messageData.toUtf8(), &error);
        if (error.error) {
            qWarning() << "Failed to parse text message as JSON object:" << messageData
                       << "Error is:" << error.errorString();
            return;
        } else if (!message.isObject()) {
            qWarning() << "Received JSON message that is not an object: " << messageData;
            return;
        }
        this->messageReceived(message.object(), this);
    }

  private:
    QWebSocket *m_socket;
};
class ServerObject : public QObject {
    Q_OBJECT
  public:
    Q_INVOKABLE int addOne(const int &p) { return p + 1; }
    Q_INVOKABLE void triggerSignal() { return this->testSignal(); }
  Q_SIGNALS:
    void testSignal();
};
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QWebSocketServer server("", QWebSocketServer::NonSecureMode);
    const int port = 3333;
    QWebChannel channel;
    QObject::connect(&server, &QWebSocketServer::newConnection, [&]() {
        QWebSocket *socket = server.nextPendingConnection();
        channel.connectTo(new WebSocketTransport(socket));
    });
    ServerObject serverObject;
    channel.registerObject("obj", &serverObject);
    server.listen(QHostAddress::Any, port);
    return app.exec();
}
#include "main.moc"