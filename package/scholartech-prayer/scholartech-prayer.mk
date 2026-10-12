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
endef

define SCHOLARTECH_PRAYER_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 0644 $(SCHOLARTECH_PRAYER_PKGDIR)/scholartech-prayer.service \
		$(TARGET_DIR)/usr/lib/systemd/system/scholartech-prayer.service
	mkdir -p $(TARGET_DIR)/etc/systemd/system/multi-user.target.wants
	ln -sf /usr/lib/systemd/system/scholartech-prayer.service \
		$(TARGET_DIR)/etc/systemd/system/multi-user.target.wants/scholartech-prayer.service
endef

$(eval $(generic-package))
