#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QTimer>

#include <functional>
#include <memory>

#include "CalendarClient.h"
#include "CommandServer.h"
#include "Config.h"
#include "DelegatedAuth.h"
#include "GoogleAuth.h"
#include "GoogleColorNames.h"
#include "SnapshotBuilder.h"
#include "SnapshotWriter.h"

namespace
{

// Resolves one config color value to a lowercase "#rrggbb": passes a
// literal hex straight through, otherwise looks the name up first in
// Google's per-event color names, then its per-calendar color names (see
// GoogleColorNames.h), using the live palettes fetched from
// /calendar/v3/colors. Returns empty and fills `warning` if none of that
// resolves — callers should log it clearly rather than silently guess.
QString resolveColor( const QString &raw, const QJsonObject &eventPalette, const QJsonObject &calendarPalette,
					  QString &warning )
{
	static const QRegularExpression hexPattern( QStringLiteral( "^#?[0-9a-fA-F]{6}$" ) );
	const QString trimmed = raw.trimmed();
	if ( hexPattern.match( trimmed ).hasMatch() )
		return ( trimmed.startsWith( '#' ) ? trimmed : ( "#" + trimmed ) ).toLower();

	const QString key = trimmed.toLower().remove( ' ' );
	const QString eventId = GoogleColorNames::eventColorIds().value( key );
	if ( !eventId.isEmpty() )
	{
		const QString hex = eventPalette.value( eventId ).toObject().value( "background" ).toString();
		if ( !hex.isEmpty() )
			return hex.toLower();
	}
	const QString calendarId = GoogleColorNames::calendarColorIds().value( key );
	if ( !calendarId.isEmpty() )
	{
		const QString hex = calendarPalette.value( calendarId ).toObject().value( "background" ).toString();
		if ( !hex.isEmpty() )
			return hex.toLower();
	}
	warning =
		QStringLiteral(
			"unknown color \"%1\" — use a literal hex instead (see GoogleColorNames.h for known names)" )
			.arg( raw );
	return QString();
}

// Fetches every configured calendar once, builds one snapshot, writes it,
// then invokes `onDone(hadError)`. A fresh SnapshotBuilder every call since
// "today"/"this weekend" are computed at construction time and must be
// re-evaluated each cycle.
void runSyncCycle( CalendarClient *client, const QVector<CalendarConfig> &calendars,
				   const QVector<PersonConfig> &people, const QHash<QString, QString> &eventColorIdToHex,
				   const QHash<QString, QString> &calendarFallbackHex, const QString &snapshotPath,
				   std::function<void( bool hadError )> onDone )
{
	auto builder = std::make_shared<SnapshotBuilder>( people, eventColorIdToHex, calendarFallbackHex );
	auto pending = std::make_shared<int>( calendars.size() );
	auto hadError = std::make_shared<bool>( false );

	// Today through +5 weeks covers today/this-weekend/upcoming without
	// paging through months of unrelated future events.
	const QDateTime windowStart( QDate::currentDate(), QTime( 0, 0 ) );
	const QDateTime windowEnd = windowStart.addDays( 35 );

	for ( const CalendarConfig &cal : calendars )
	{
		client->listEvents(
			cal.calendarId, windowStart, windowEnd,
			[builder, cal, pending, hadError, snapshotPath, onDone]( QJsonArray events, QString err ) {
				if ( !err.isEmpty() )
				{
					qWarning().noquote() << QStringLiteral( "failed to fetch events for calendar %1: %2" )
												.arg( cal.calendarId, err );
					*hadError = true;
				}
				else
				{
					builder->addEvents( cal.calendarId, events );
				}

				if ( --( *pending ) > 0 )
					return;

				// Which calendar a newly-created event (tapping an empty
				// timeline slot) lands on — Config::load guarantees at
				// least one calendar is configured, so this is never
				// empty. Households with more than one configured calendar
				// all get funneled onto the first regardless of which
				// person's column was tapped; there's no per-person
				// calendar ownership concept in this data model (see
				// SnapshotBuilder's class comment).
				QJsonObject snapshot = builder->build();
				snapshot["defaultCalendarId"] = cal.calendarId;

				QString writeError;
				if ( SnapshotWriter::write( snapshotPath, snapshot, writeError ) )
				{
					qInfo().noquote() << "snapshot written to" << snapshotPath;
				}
				else
				{
					qCritical().noquote() << "failed to write snapshot:" << writeError;
					*hadError = true;
				}
				onDone( *hadError );
			} );
	}
}

} // namespace

int main( int argc, char *argv[] )
{
	QCoreApplication app( argc, argv );
	QCoreApplication::setApplicationName( "ha-kiosk-google-calendar-sync" );

	QCommandLineParser parser;
	parser.addOption( { "config", "Path to daemon config JSON.", "path" } );
	parser.addOption(
		{ "once", "Run a single sync cycle and exit, instead of polling forever (for testing)." } );
	parser.process( app );

	const QString configPath = parser.value( "config" );
	if ( configPath.isEmpty() )
	{
		qCritical() << "usage: ha-kiosk-google-calendar-sync --config <path> [--once]";
		return 1;
	}

	Config config;
	QString error;
	if ( !Config::load( configPath, config, error ) )
	{
		qCritical() << "config error:" << error;
		return 1;
	}

	// Guards against two instances racing over the same socket/snapshot —
	// lives next to snapshotPath (normally /run/kiosk, an FHS runtime
	// directory) rather than a hardcoded path, so a config pointed
	// elsewhere (e.g. for testing) still gets its own lock.
	const QString runtimeDir = QFileInfo( config.snapshotPath ).absolutePath();
	QDir().mkpath( runtimeDir );
	QLockFile lockFile( runtimeDir + "/ha-kiosk-google-calendar-sync.lock" );
	if ( !lockFile.tryLock() )
	{
		qCritical().noquote() << "another instance is already running (lock held at" << lockFile.fileName()
							  << ")";
		return 1;
	}

	auto auth = new GoogleAuth( config.serviceAccountKeyPath, &app );
	if ( !auth->init( error ) )
	{
		qCritical() << "auth error:" << error;
		return 1;
	}

	// Only constructed when configured (Config::load already verified all
	// three fields are set together or not at all) — CalendarClient treats
	// a null delegatedAuth as "no fallback, just report delegation_required
	// as a failure", same behavior as before this existed.
	DelegatedAuth *delegatedAuth = nullptr;
	if ( !config.oauthClientId.isEmpty() )
	{
		delegatedAuth =
			new DelegatedAuth( config.oauthClientId, config.oauthClientSecret, config.userTokenPath, &app );
		QObject::connect( delegatedAuth, &DelegatedAuth::authorizationPending, &app,
						  []( QString verificationUrl, QString userCode, int expiresInSecs ) {
							  qWarning().noquote() << QStringLiteral(
								  "Google Calendar needs one-time authorization to invite attendees: visit "
								  "%1 and enter code %2 (expires in %3 min)" )
														.arg( verificationUrl, userCode )
														.arg( expiresInSecs / 60 );
						  } );
	}

	auto client = new CalendarClient( auth, delegatedAuth, &app );
	const bool once = parser.isSet( "once" );
	const QVector<CalendarConfig> calendars = config.calendars;
	const QVector<PersonConfig> configuredPeople = config.people;
	const QString snapshotPath = config.snapshotPath;
	const int pollIntervalMs = qMax( 1, config.pollIntervalSeconds ) * 1000;

	auto pollTimer = std::make_shared<QTimer>();
	auto commandServer = std::make_shared<CommandServer>( client, config.people, delegatedAuth );

	// Color resolution needs one network round trip (the live /colors
	// palettes) before anything else can run, so the rest of startup —
	// resolving every configured color, building the id/calendar -> hex
	// lookup tables SnapshotBuilder needs, then wiring the poll timer,
	// command socket, and first sync cycle — all happens inside this
	// callback rather than linearly in main().
	client->fetchColorDefinitions(
		[=]( QJsonObject calendarPalette, QJsonObject eventPalette, QString colorsError ) {
			if ( !colorsError.isEmpty() )
			{
				qCritical().noquote() << "failed to fetch color definitions:" << colorsError;
				QTimer::singleShot( 0, qApp, []() { QCoreApplication::exit( 1 ); } );
				return;
			}

			QHash<QString, QString> eventColorIdToHex;
			for ( auto it = eventPalette.constBegin(); it != eventPalette.constEnd(); ++it )
				eventColorIdToHex.insert( it.key(),
										  it.value().toObject().value( "background" ).toString().toLower() );

			QVector<PersonConfig> resolvedPeople;
			qInfo().noquote() << "resolved person colors:";
			for ( const PersonConfig &p : configuredPeople )
			{
				QString warning;
				const QString hex = resolveColor( p.color, eventPalette, calendarPalette, warning );
				if ( !warning.isEmpty() )
				{
					qWarning().noquote()
						<< QStringLiteral(
							   "  %1: %2 — this person's events will fall to \"other\" until fixed" )
							   .arg( p.person, warning );
					continue;
				}
				qInfo().noquote() << QStringLiteral( "  %1 = \"%2\" -> %3 (tagged via %4)" )
										  .arg( p.person, p.color, hex, p.emails.join( ", " ) );
				resolvedPeople.append( { p.person, hex, p.emails } );
			}

			QHash<QString, QString> calendarFallbackHex;
			qInfo().noquote() << "resolved calendar fallback colors:";
			for ( const CalendarConfig &cal : calendars )
			{
				if ( cal.color.isEmpty() )
					continue;
				QString warning;
				const QString hex = resolveColor( cal.color, eventPalette, calendarPalette, warning );
				if ( !warning.isEmpty() )
				{
					qWarning().noquote() << QStringLiteral( "  %1: %2" ).arg( cal.calendarId, warning );
					continue;
				}
				qInfo().noquote()
					<< QStringLiteral( "  %1 = \"%2\" -> %3" ).arg( cal.calendarId, cal.color, hex );
				calendarFallbackHex.insert( cal.calendarId, hex );
			}

			auto cycle = [client, calendars, resolvedPeople, eventColorIdToHex, calendarFallbackHex,
						  snapshotPath, once]() {
				runSyncCycle( client, calendars, resolvedPeople, eventColorIdToHex, calendarFallbackHex,
							  snapshotPath, [once]( bool hadError ) {
								  if ( once )
									  QTimer::singleShot( 0, qApp, [hadError]() {
										  QCoreApplication::exit( hadError ? 1 : 0 );
									  } );
							  } );
			};

			if ( !once )
			{
				pollTimer->setInterval( pollIntervalMs );
				QObject::connect( pollTimer.get(), &QTimer::timeout, cycle );
				pollTimer->start();
				qInfo().noquote() << "polling every" << config.pollIntervalSeconds << "seconds";

				if ( config.socketPath.isEmpty() )
				{
					qWarning() << "no socketPath configured — command socket disabled, read-only mode";
				}
				else
				{
					QString socketError;
					if ( !commandServer->listen( config.socketPath, socketError ) )
					{
						qCritical().noquote() << "failed to listen on command socket:" << socketError;
						QTimer::singleShot( 0, qApp, []() { QCoreApplication::exit( 1 ); } );
						return;
					}
					// Refresh the snapshot right after a successful write
					// instead of waiting for the next poll tick, so the read
					// model catches up to what was just changed.
					QObject::connect( commandServer.get(), &CommandServer::writeSucceeded, cycle );
					qInfo().noquote() << "listening for commands on" << config.socketPath;
				}
			}

			cycle();
		} );

	const int result = app.exec();

	// Explicit, ordered teardown before QCoreApplication's own destructor
	// runs: GoogleAuth/DelegatedAuth/CalendarClient own QNetworkAccessManager
	// as a direct member, and letting Qt's parent-child cleanup destroy them
	// in whatever order it likes (after real socket/SSL activity) produces a
	// harmless but noisy "invalid nullptr parameter" connect warning.
	commandServer.reset();
	delete client;
	delete delegatedAuth;
	delete auth;

	return result;
}
