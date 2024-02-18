include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=xplayerdemo
PKG_VERSION:=1
PKG_RELEASE:=1


PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SUBMENU:=Multimedia
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=use xplayerdemo in tina_multimedia
  DEPENDS:=+libcedarx
endef

define Package/$(PKG_NAME)/description
	CedarX2.8 xplayerdemo
endef

define Build/Prepare

endef

define Build/Compile
endef


define Package/$(PKG_NAME)/install
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
