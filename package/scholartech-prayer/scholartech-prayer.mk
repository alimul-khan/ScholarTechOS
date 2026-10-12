################################################################################
#
# scholartech-prayer
#
################################################################################

SCHOLARTECH_PRAYER_VERSION = $(shell cat $(BR2_EXTERNAL_SCHOLARTECH_PATH)/apps/prayer/VERSION)
SCHOLARTECH_PRAYER_SITE = $(BR2_EXTERNAL_SCHOLARTECH_PATH)/apps/prayer
SCHOLARTECH_PRAYER_SITE_METHOD = local
SCHOLARTECH_PRAYER_LICENSE = Proprietary, MIT (vendor/adhan.js), OFL-1.1 (fonts)
SCHOLARTECH_PRAYER_LICENSE_FILES = vendor/ADHAN-LICENSE fonts/LICENSE.txt

define SCHOLARTECH_PRAYER_INSTALL_TARGET_CMDS
	rm -rf $(TARGET_DIR)/opt/scholartech/prayer
	mkdir -p $(TARGET_DIR)/opt/scholartech/prayer
	cd $(@D) && cp -a VERSION *.html serve.py css js fonts img samples vendor \
		$(TARGET_DIR)/opt/scholartech/prayer/
	$(INSTALL) -D -m 0644 $(SCHOLARTECH_PRAYER_PKGDIR)/app.conf \
		$(TARGET_DIR)/etc/scholartech/app.conf
	$(INSTALL) -D -m 0755 $(SCHOLARTECH_PRAYER_PKGDIR)/prayer-serve \
		$(TARGET_DIR)/opt/scholartech/prayer-serve
	$(INSTALL) -D -m 0755 $(SCHOLARTECH_PRAYER_PKGDIR)/prayer-update \
		$(TARGET_DIR)/opt/scholartech/prayer-update
	$(SCHOLARTECH_PRAYER_INSTALL_DEPLOY_KEY)
endef

# Read-only deploy key for app updates. Never in git: the build reads it from secrets/
# (CI writes it there from the PRAYER_DEPLOY_KEY secret). Without it, updates are off.
SCHOLARTECH_PRAYER_DEPLOY_KEY = $(BR2_EXTERNAL_SCHOLARTECH_PATH)/secrets/prayer-deploy-key
ifneq ($(wildcard $(SCHOLARTECH_PRAYER_DEPLOY_KEY)),)
define SCHOLARTECH_PRAYER_INSTALL_DEPLOY_KEY
	$(INSTALL) -D -m 0600 $(SCHOLARTECH_PRAYER_DEPLOY_KEY) \
		$(TARGET_DIR)/etc/scholartech/prayer-deploy-key
endef
endif

define SCHOLARTECH_PRAYER_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(SCHOLARTECH_PRAYER_PKGDIR)/scholartech-prayer.service \
		$(TARGET_DIR)/usr/lib/systemd/system/scholartech-prayer.service
	mkdir -p $(TARGET_DIR)/etc/systemd/system/multi-user.target.wants
	ln -sf /usr/lib/systemd/system/scholartech-prayer.service \
		$(TARGET_DIR)/etc/systemd/system/multi-user.target.wants/scholartech-prayer.service
	$(INSTALL) -D -m 0644 $(SCHOLARTECH_PRAYER_PKGDIR)/scholartech-prayer-update.service \
		$(TARGET_DIR)/usr/lib/systemd/system/scholartech-prayer-update.service
	$(INSTALL) -D -m 0644 $(SCHOLARTECH_PRAYER_PKGDIR)/scholartech-prayer-update.timer \
		$(TARGET_DIR)/usr/lib/systemd/system/scholartech-prayer-update.timer
	mkdir -p $(TARGET_DIR)/etc/systemd/system/timers.target.wants
	ln -sf /usr/lib/systemd/system/scholartech-prayer-update.timer \
		$(TARGET_DIR)/etc/systemd/system/timers.target.wants/scholartech-prayer-update.timer
endef

$(eval $(generic-package))
