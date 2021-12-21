# SPDX-License-Identifier: GPL-2.0-only

# Begin modified by hailong.chen for task 9551005 on 2020-08-05
ifdef CONFIG_TCT_INIT_SPEED_UP
# End modified by hailong.chen for task 9551005 on 2020-08-05
obj-$(CONFIG_QPNP_QG)			+= qpnp-qg.o pmic-voter.o qg-util.o qg-soc.o qg-sdam.o qg-battery-profile.o qg-profile-lib.o fg-alg.o
endif

obj-$(CONFIG_TCT_OTTAWA_CHG_PATCH)	+= p9415.o

# Begin added for task 10831333 on 2021-03-11
obj-$(CONFIG_TCT_CHICAGO_CHG_PATCH)	+= p9415-v2.o
# End added for task 10831333 on 2021-03-11

obj-$(CONFIG_QPNP_SMB5)			+= step-chg-jeita.o battery.o qpnp-smb5.o smb5-lib.o pmic-voter.o storm-watch.o schgm-flash.o
obj-$(CONFIG_SMB1390_CHARGE_PUMP_PSY)	+= smb1390-charger-psy.o pmic-voter.o
obj-$(CONFIG_SMB1355_SLAVE_CHARGER)	+= smb1355-charger.o pmic-voter.o
obj-$(CONFIG_SMB1351_USB_CHARGER)	+= smb1351-charger.o pmic-voter.o battery.o
obj-$(CONFIG_QPNP_SMB2)			+= step-chg-jeita.o battery.o qpnp-smb2.o smb-lib.o pmic-voter.o storm-watch.o
obj-$(CONFIG_SMB138X_CHARGER)		+= step-chg-jeita.o smb138x-charger.o smb-lib.o pmic-voter.o storm-watch.o battery.o
obj-$(CONFIG_QPNP_FG_GEN3)		+= qpnp-fg-gen3.o fg-memif.o fg-util.o
obj-$(CONFIG_QPNP_QNOVO5)		+= qpnp-qnovo5.o battery.o pmic-voter.o
obj-$(CONFIG_QPNP_FG_GEN4)		+= qpnp-fg-gen4.o fg-memif.o fg-util.o fg-alg.o pmic-voter.o
# Begin modified by hailong.chen for task 9551005 on 2020-08-05
ifndef CONFIG_TCT_INIT_SPEED_UP
# End modified by hailong.chen for task 9551005 on 2020-08-05
obj-$(CONFIG_QPNP_QG)			+= qpnp-qg.o pmic-voter.o qg-util.o qg-soc.o qg-sdam.o qg-battery-profile.o qg-profile-lib.o fg-alg.o
endif
obj-$(CONFIG_HL6111R)			+= hl6111r.o
obj-$(CONFIG_SMB1398_CHARGER)		+= smb1398-charger.o pmic-voter.o
obj-$(CONFIG_QPNP_SMBLITE)		+= step-chg-jeita.o battery.o qpnp-smblite.o smblite-lib.o pmic-voter.o storm-watch.o schgm-flashlite.o
# Begin added by hailong.chen for task 9656602 on 2020-09-09
obj-$(CONFIG_TCT_IRVINE_CHG_COMMON)		+= tct-customized-fr.o
# End added by hailong.chen for task 9656602 on 2020-09-09