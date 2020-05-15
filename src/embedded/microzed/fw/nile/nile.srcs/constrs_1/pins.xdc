set_property PACKAGE_PIN G19 [get_ports {GPIO_0_0_tri_io[0]}];  # BTN1 on carrier (EMIO GPIO)
set_property PACKAGE_PIN J20 [get_ports {gpio_rtl_0_tri_io[0]}];  # BTN3 on carrier (AXI GPIO)
set_property PACKAGE_PIN U19 [get_ports {gpio_rtl_0_tri_io[1]}];  # LED4 on carrier (AXI GPIO)

set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_0_tri_io[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_rtl_0_tri_io[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_rtl_0_tri_io[1]}]
