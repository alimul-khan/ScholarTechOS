# Optional kiosk (not in the default image)

Files for a fullscreen web kiosk (cage + Cog/WebKit). Not enabled by default.
To add it back later: enable BR2_PACKAGE_WPEWEBKIT, BR2_PACKAGE_COG, BR2_PACKAGE_CAGE,
BR2_PACKAGE_LIBINPUT in the config fragment, copy these files into the overlay
(kiosk.service -> /etc/systemd/system, kiosk.conf -> /etc/scholartech,
index.html -> /usr/share/scholartech/www) and enable the service.
WebKit adds roughly 4 hours to a cloud build.
