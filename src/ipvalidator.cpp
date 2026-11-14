#include "ipvalidator.h"
#include <QHostAddress>
#include <QAbstractSocket>

bool IPValidator::isIPValidIPV4(const QString &address)
{
    QHostAddress ip;
    if (!ip.setAddress(address)) {
        return false;
    }

    return (ip.protocol() == QAbstractSocket::IPv4Protocol);
}
