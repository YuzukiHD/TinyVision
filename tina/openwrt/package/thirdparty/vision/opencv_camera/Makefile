include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=opencv_camera
PKG_VERSION:=1.0
PKG_RELEASE:=1

define Package/$(PKG_NAME)
  SECTION:=OpenCV
  CATEGORY:=OpenCV
  TITLE:=opencv open camera and display image
  DEPENDS:=+opencv +libstdcpp
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/* $(PKG_BUILD_DIR)/
endef

TARGET_LDFLAGS+=-L$(STAGING_DIR)/usr/lib/

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS) $(EXTRA_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		TARGET="$(PKG_NAME)" \
		all
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) 	$(1)/usr/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))