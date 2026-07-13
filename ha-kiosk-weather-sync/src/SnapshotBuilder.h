#pragma once

#include <QJsonObject>
#include <QJsonValue>

/// Turns BomClient's two forecast responses (daily/hourly) plus InfluxClient's
/// hourly temperature history and current conditions (all from the local
/// Ecowitt station via Telegraf) into the one read-model snapshot
/// WeatherBridge serves to QML. Field shapes here are chosen to match
/// DashboardData.qml's existing mock literals exactly (hour/icon/temp,
/// day/icon/hi/lo, pre-formatted observation strings) so the QML side only
/// has to swap its data source, not its field names.
namespace SnapshotBuilder
{

/// Assembles the full snapshot object — hourlyForecast, forecast,
/// observations, sun, and weatherHistory — from BomClient's daily/hourly
/// forecasts and InfluxClient's current conditions/temperature history.
QJsonObject
build( const QJsonValue &daily, const QJsonValue &hourly, const QJsonValue &currentConditions, const QJsonValue &weatherHistory );

} // namespace SnapshotBuilder
