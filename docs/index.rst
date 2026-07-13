ha-tab kiosk
============

A Qt6 kiosk tablet: two independent daemons poll external data sources
(Google Calendar, BOM weather + a local InfluxDB) and each write a JSON
snapshot to disk; the kiosk app (:doc:`ha-kiosk`) reads both snapshots and
renders the dashboard. Only the calendar side has a write path — touch
gestures in the app send intent-named commands
(:doc:`calendar-sync-client`'s wire protocol) over a Unix domain socket to
:doc:`ha-kiosk-google-calendar-sync`, which is the only component that talks
to Google. :doc:`kiosk-log` is a small shared library all three binaries
install at startup for consistent stderr/file logging.

.. toctree::
   :maxdepth: 2

   kiosk-log
   calendar-sync-client
   ha-kiosk-weather-sync
   ha-kiosk-google-calendar-sync
   ha-kiosk
