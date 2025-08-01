# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel serial device drivers.
#

obj-$(CONFIG_SERIAL_CORE) += serial_base.o
serial_base-y := serial_core.o serial_base_bus.o serial_ctrl.o serial_port.o

obj-$(CONFIG_SERIAL_EARLYCON) += earlycon.o
obj-$(CONFIG_SERIAL_EARLYCON_SEMIHOST) += earlycon-semihost.o
obj-$(CONFIG_SERIAL_EARLYCON_RISCV_SBI) += earlycon-riscv-sbi.o

# These Sparc drivers have to appear before others such as 8250
# which share ttySx minor node space.  Otherwise console device
# names change and other unplesantries.
obj-$(CONFIG_SERIAL_SUNCORE) += suncore.o
obj-$(CONFIG_SERIAL_SUNHV) += sunhv.o
obj-$(CONFIG_SERIAL_SUNZILOG) += sunzilog.o
obj-$(CONFIG_SERIAL_SUNSU) += sunsu.o
obj-$(CONFIG_SERIAL_SUNSAB) += sunsab.o

obj-$(CONFIG_SERIAL_21285) += 21285.o

# Now bring in any enabled 8250/16450/16550 type drivers.
obj-y += 8250/

obj-$(CONFIG_SERIAL_ALTERA_JTAGUART)	+= altera_jtaguart.o
obj-$(CONFIG_SERIAL_ALTERA_UART)	+= altera_uart.o
obj-$(CONFIG_SERIAL_AMBA_PL010)		+= amba-pl010.o
obj-$(CONFIG_SERIAL_AMBA_PL011)		+= amba-pl011.o
obj-$(CONFIG_SERIAL_GRLIB_GAISLER_APBUART) += apbuart.o
obj-$(CONFIG_SERIAL_AR933X)		+= ar933x_uart.o
obj-$(CONFIG_SERIAL_ARC)		+= arc_uart.o
obj-$(CONFIG_SERIAL_ATMEL)		+= atmel_serial.o
obj-$(CONFIG_SERIAL_BCM63XX)		+= bcm63xx_uart.o
obj-$(CONFIG_SERIAL_CLPS711X)		+= clps711x.o
obj-$(CONFIG_SERIAL_CPM)		+= cpm_uart.o
obj-$(CONFIG_SERIAL_CONEXANT_DIGICOLOR)	+= digicolor-usart.o
obj-$(CONFIG_SERIAL_DZ)			+= dz.o
obj-$(CONFIG_SERIAL_ESP32)		+= esp32_uart.o
obj-$(CONFIG_SERIAL_ESP32_ACM)		+= esp32_acm.o
obj-$(CONFIG_SERIAL_FSL_LINFLEXUART)	+= fsl_linflexuart.o
obj-$(CONFIG_SERIAL_FSL_LPUART)		+= fsl_lpuart.o
obj-$(CONFIG_SERIAL_ICOM)		+= icom.o
obj-$(CONFIG_SERIAL_IMX)		+= imx.o
obj-$(CONFIG_SERIAL_IMX_EARLYCON)	+= imx_earlycon.o
obj-$(CONFIG_SERIAL_IP22_ZILOG)		+= ip22zilog.o
obj-$(CONFIG_SERIAL_JSM)		+= jsm/
obj-$(CONFIG_SERIAL_LANTIQ)		+= lantiq.o
obj-$(CONFIG_SERIAL_LITEUART)		+= liteuart.o
obj-$(CONFIG_SERIAL_HS_LPC32XX)		+= lpc32xx_hs.o
obj-$(CONFIG_SERIAL_MAX3100)		+= max3100.o
obj-$(CONFIG_SERIAL_MAX310X)		+= max310x.o
obj-$(CONFIG_SERIAL_MCF)		+= mcf.o
obj-$(CONFIG_SERIAL_MEN_Z135)		+= men_z135_uart.o
obj-$(CONFIG_SERIAL_MILBEAUT_USIO)	+= milbeaut_usio.o
obj-$(CONFIG_SERIAL_MESON)		+= meson_uart.o
obj-$(CONFIG_SERIAL_MPC52xx)		+= mpc52xx_uart.o
obj-$(CONFIG_SERIAL_MPS2_UART)		+= mps2-uart.o
obj-$(CONFIG_SERIAL_MSM)		+= msm_serial.o
obj-$(CONFIG_SERIAL_MUX)		+= mux.o
obj-$(CONFIG_SERIAL_MVEBU_UART)		+= mvebu-uart.o
obj-$(CONFIG_SERIAL_MXS_AUART)		+= mxs-auart.o
obj-$(CONFIG_SERIAL_OMAP)		+= omap-serial.o
obj-$(CONFIG_SERIAL_OWL)		+= owl-uart.o
obj-$(CONFIG_SERIAL_PCH_UART)		+= pch_uart.o
obj-$(CONFIG_SERIAL_PIC32)		+= pic32_uart.o
obj-$(CONFIG_SERIAL_PXA_NON8250)	+= pxa.o
obj-$(CONFIG_SERIAL_PMACZILOG)		+= pmac_zilog.o
obj-$(CONFIG_SERIAL_QCOM_GENI)		+= qcom_geni_serial.o
obj-$(CONFIG_SERIAL_QE)			+= ucc_uart.o
obj-$(CONFIG_SERIAL_RDA)		+= rda-uart.o
obj-$(CONFIG_SERIAL_RP2)		+= rp2.o
obj-$(CONFIG_SERIAL_RSCI)		+= rsci.o
obj-$(CONFIG_SERIAL_SA1100)		+= sa1100.o
obj-$(CONFIG_SERIAL_SAMSUNG)		+= samsung_tty.o
obj-$(CONFIG_SERIAL_SB1250_DUART)	+= sb1250-duart.o
obj-$(CONFIG_SERIAL_SCCNXP)		+= sccnxp.o
obj-$(CONFIG_SERIAL_SC16IS7XX)		+= sc16is7xx.o
obj-$(CONFIG_SERIAL_SC16IS7XX_SPI)	+= sc16is7xx_spi.o
obj-$(CONFIG_SERIAL_SC16IS7XX_I2C)	+= sc16is7xx_i2c.o
obj-$(CONFIG_SERIAL_SH_SCI)		+= sh-sci.o
obj-$(CONFIG_SERIAL_SIFIVE)		+= sifive.o
obj-$(CONFIG_SERIAL_SPRD)		+= sprd_serial.o
obj-$(CONFIG_SERIAL_ST_ASC)		+= st-asc.o
obj-$(CONFIG_SERIAL_STM32)		+= stm32-usart.o
obj-$(CONFIG_SERIAL_SUNPLUS)		+= sunplus-uart.o
obj-$(CONFIG_SERIAL_TEGRA)		+= serial-tegra.o
obj-$(CONFIG_SERIAL_TEGRA_TCU)		+= tegra-tcu.o
obj-$(CONFIG_SERIAL_TEGRA_UTC)		+= tegra-utc.o
obj-$(CONFIG_SERIAL_TIMBERDALE)		+= timbuart.o
obj-$(CONFIG_SERIAL_TXX9)		+= serial_txx9.o
obj-$(CONFIG_SERIAL_UARTLITE)		+= uartlite.o
obj-$(CONFIG_SERIAL_VT8500)		+= vt8500_serial.o
obj-$(CONFIG_SERIAL_XILINX_PS_UART)	+= xilinx_uartps.o
obj-$(CONFIG_SERIAL_ZS)			+= zs.o

# GPIOLIB helpers for modem control lines
obj-$(CONFIG_SERIAL_MCTRL_GPIO)	+= serial_mctrl_gpio.o

obj-$(CONFIG_KGDB_SERIAL_CONSOLE) += kgdboc.o
obj-$(CONFIG_SERIAL_NUVOTON_MA35D1) += ma35d1_serial.o
