Smartcard Support for S905-- --
	----------------------To configure smartcard driver on your platform,
	you must following two steps below 1)
  Add configurations to your kernel config file using menuconfig, or you can add to.config.CONFIG_AML_SMARTCARD = y 2) Add dts configuration Samples below:

	smartcard
	{
		compatible = "amlogic,smartcard";
		irq_trigger_type = "GPIO_IRQ_LOW";

		reset_pin - gpios = <&gpio GPIOX_11 GPIO_ACTIVE_HIGH >;	//Reset pin
		detect_pin - gpios = <&gpio GPIOX_20 GPIO_ACTIVE_HIGH >;	//Detect pin
		enable_5v3v_pin - gpios = <&gpio GPIOX_10 GPIO_ACTIVE_HIGH >;	//5V3V pin, can be ignored

		interrupts = <0 69 4 >;	//smc irq

		smc0_clock_source = <0 >;	//Smc clock source, if change this, you must adjust clk and divider in smartcard.c
		smc0_irq = <69 >;		//smc irq
		smc0_det_invert = <0 >;	//0: high voltage on detect pin indicates card in.
		smc0_5v3v_level = <0 >;
		smc_need_enable_pin = "no";	//Ordinarily, smartcard controller needs a enable pin.
		if board
			controls smartcard directly, then we don 't need it.
	reset_level = <0>;					//0: pull low reset pin to reset card

	smc0_clk_pinmux_reg = <0x30>;
	smc0_clk_pinmux_bit = <0x80>;
	smc0_clk_oen_reg = <0x200f>;
	smc0_clk_out_reg = <0x2010>;
	smc0_clk_bit = <0x2000>;
	smc0_clk_oebit = <0x2000000>;
	smc0_clk_oubit = <0x1000000>;

	pinctrl-names = "default";
	pinctrl-0 = <&smc_pins>;
	resets = <&clock GCLK_IDX_SMART_CARD_MPEG_DOMAIN>;
	reset-names = "smartcard";

	status = "okay";
};

&pinmux {
smc_pins:smc_pins {									 //Set gpio to 7816-clk 7816-data mode.
			 amlogic,setmask = <4 0x000000c0>;       //Please refer to core-pin mux document
			 amlogic,clrmask = <3 0x60000280>;
			 amlogic,pins = "GPIOX_8","GPIOX_9";
		 };
}


Daniel.yan
yan.yan@amlogic.com





