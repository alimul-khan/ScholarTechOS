#pragma once
/* The local settings page on port 80 (http://<screen>/ or http://prayer-<id>.local/): display_template's manage.html and
 * the small API it uses, as the Android app serves it (StosApi.java) — /api/config, /api/media, /media/<id>, /api/places,
 * /api/wifi — plus /api/auth. Changes need the screen's 9-digit passphrase (X-STOS-PIN), like prayerdisplay.online. */
void web_start(void);
