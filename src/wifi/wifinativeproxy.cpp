/**
 ** This file is part of the WiFi project.
 ** Copyright 2019 张作深 <zhangzuoshen@hangsheng.com.cn>.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "wifinativeproxy_p.h"

#include <private/qobject_p.h>

#include "wifidbus_p.h"
#include "station_interface.h"

class WiFiNativeProxyPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(WiFiNativeProxy)
public:
    WiFiNativeProxyPrivate();
    ~WiFiNativeProxyPrivate();

    void onConnectionInfoChanged(const QString &info);
    void onScanResultFound(const QString &bss);
    void onScanResultLost(const QString &bss);
    void onScanResultUpdated(const QString &bss);
    void onWifiStateChanged(bool enabled);

    void onServiceRegistered(const QString &service);
    void onServiceUnregistered(const QString &service);
    void processServiced(bool serviced);

public:
    wifi::native::Station *m_station = NULL;
    bool m_isServiced = false;

    bool m_isEnabled = false;
    WiFiInfo m_info;
    WiFiScanResultList m_scanResults;
    WiFiNetworkList m_networks;
};

WiFiNativeProxyPrivate::WiFiNativeProxyPrivate() : QObjectPrivate()
{
}

WiFiNativeProxyPrivate::~WiFiNativeProxyPrivate()
{
}

void WiFiNativeProxyPrivate::onConnectionInfoChanged(const QString &info)
{
    Q_Q(WiFiNativeProxy);

    WiFiInfo revInfo = WiFiInfo::fromJson(info.toUtf8());
    if(m_info != revInfo) {
        m_info = revInfo;
        Q_EMIT q->connectionInfoChanged();
    }
}

void WiFiNativeProxyPrivate::onScanResultFound(const QString &bss)
{
    Q_Q(WiFiNativeProxy);

    WiFiScanResult revBSS = WiFiScanResult::fromJson(bss.toUtf8());
    if(!m_scanResults.contains(revBSS)) {
        m_scanResults << revBSS;
        Q_EMIT q->scanResultsChanged();
    }
}

void WiFiNativeProxyPrivate::onScanResultLost(const QString &bss)
{
    Q_Q(WiFiNativeProxy);

    WiFiScanResult revBSS = WiFiScanResult::fromJson(bss.toUtf8());
    if(m_scanResults.contains(revBSS)) {
        m_scanResults.removeAll(revBSS);
        Q_EMIT q->scanResultsChanged();
    }
}

void WiFiNativeProxyPrivate::onScanResultUpdated(const QString &bss)
{
    Q_Q(WiFiNativeProxy);

    WiFiScanResult revBSS = WiFiScanResult::fromJson(bss.toUtf8());
    int index = m_scanResults.indexOf(revBSS);
    if(index >= 0 && index < m_scanResults.length()) {
        m_scanResults.replace(index, revBSS);
        Q_EMIT q->scanResultsChanged();
    }
}

void WiFiNativeProxyPrivate::onWifiStateChanged(bool enabled)
{
    Q_Q(WiFiNativeProxy);

    if(m_isEnabled != enabled) {
        m_isEnabled = enabled;
        Q_EMIT q->isWiFiEnabledChanged();
    }
}

void WiFiNativeProxyPrivate::onServiceRegistered(const QString &service)
{
    Q_Q(WiFiNativeProxy);

    qDebug() << Q_FUNC_INFO << service;
    processServiced(true);
    //    q->setWiFiEnabled(true);
}

void WiFiNativeProxyPrivate::onServiceUnregistered(const QString &service)
{
    Q_Q(WiFiNativeProxy);

    qDebug() << Q_FUNC_INFO << service;
    processServiced(false);
}

void WiFiNativeProxyPrivate::processServiced(bool serviced)
{
    Q_Q(WiFiNativeProxy);

    if(m_isServiced == serviced) {
        return;
    }
    m_isServiced = serviced;
    if(m_isServiced) {
        m_isEnabled = m_station->isWiFiEnabled();
        Q_EMIT q->isWiFiEnabledChanged();
        m_info = WiFiInfo::fromJson(m_station->connectionInfo().toUtf8());
        Q_EMIT q->connectionInfoChanged();
        m_scanResults = WiFiScanResultList::fromJson(m_station->scanResults().toUtf8());
        Q_EMIT q->scanResultsChanged();
        m_networks = WiFiNetworkList::fromJson(m_station->networks().toUtf8());
        Q_EMIT q->networksChanged();
    } else {
        q->setWiFiEnabled(false);
    }
}

WiFiNativeProxy::WiFiNativeProxy(QObject *parent)
    : QObject(*(new WiFiNativeProxyPrivate), parent)
{
    Q_D(WiFiNativeProxy);

    d->m_station = new wifi::native::Station(WiFiDBus::serviceName,
            WiFiDBus::stationPath, WiFiDBus::connection());
    QObjectPrivate::connect(d->m_station,
                            &WifiNativeStationInterface::ConnectionInfoChanged,
                            d, &WiFiNativeProxyPrivate::onConnectionInfoChanged);
    QObjectPrivate::connect(d->m_station,
                            &WifiNativeStationInterface::ScanResultFound,
                            d, &WiFiNativeProxyPrivate::onScanResultFound);
    QObjectPrivate::connect(d->m_station,
                            &WifiNativeStationInterface::ScanResultLost,
                            d, &WiFiNativeProxyPrivate::onScanResultLost);
    QObjectPrivate::connect(d->m_station,
                            &WifiNativeStationInterface::ScanResultUpdated,
                            d, &WiFiNativeProxyPrivate::onScanResultUpdated);
    QObjectPrivate::connect(d->m_station,
                            &WifiNativeStationInterface::WifiStateChanged,
                            d, &WiFiNativeProxyPrivate::onWifiStateChanged);

    QDBusServiceWatcher *wather = new QDBusServiceWatcher(WiFiDBus::serviceName,
            WiFiDBus::connection(), QDBusServiceWatcher::WatchForOwnerChange, this);
    QObjectPrivate::connect(wather, &QDBusServiceWatcher::serviceRegistered, d,
                            &WiFiNativeProxyPrivate::onServiceRegistered);
    QObjectPrivate::connect(wather, &QDBusServiceWatcher::serviceUnregistered, d,
                            &WiFiNativeProxyPrivate::onServiceUnregistered);

    QDBusReply<bool> reply =
                    WiFiDBus::connection().interface()->isServiceRegistered(
                                    WiFiDBus::serviceName);
    d->processServiced(reply.value());
}

bool WiFiNativeProxy::isWiFiEnabled() const
{
    Q_D(const WiFiNativeProxy);
    return d->m_isEnabled;
}

void WiFiNativeProxy::setWiFiEnabled(bool enabled)
{
    Q_D(WiFiNativeProxy);

    if(d->m_isEnabled == enabled) {
        return;
    }
    d->m_isEnabled = enabled;
    emit isWiFiEnabledChanged();

    if(d->m_isServiced) {
        d->m_station->setWiFiEnabled(d->m_isEnabled);
    }
}

WiFiInfo WiFiNativeProxy::connectionInfo() const
{
    Q_D(const WiFiNativeProxy);
    return d->m_info;
}

WiFiScanResultList WiFiNativeProxy::scanResults() const
{
    Q_D(const WiFiNativeProxy);
    return d->m_scanResults;
}

WiFiNetworkList WiFiNativeProxy::networks() const
{
    Q_D(const WiFiNativeProxy);
    return d->m_networks;
}

void WiFiNativeProxy::addNetwork(const WiFiNetwork &network)
{
    Q_D(WiFiNativeProxy);

    if(d->m_isServiced) {
        d->m_station->AddNetwork(QString::fromUtf8(network.toJson()));
    }
}
