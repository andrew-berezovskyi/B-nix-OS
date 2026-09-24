################################################################################
#
# bnix-shell
#
################################################################################

BNIX_SHELL_VERSION = 1.0.0
BNIX_SHELL_SITE = $(BR2_EXTERNAL_BNIX_PATH)/package/bnix-shell/src
BNIX_SHELL_SITE_METHOD = local

define BNIX_SHELL_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror \
		-o $(@D)/bnix-shell $(@D)/bnix-shell.c
endef

define BNIX_SHELL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bnix-shell $(TARGET_DIR)/usr/bin/bnix-shell
endef

$(eval $(generic-package))
