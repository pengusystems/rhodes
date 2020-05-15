`timescale 1ns/1ps
module dummy_axis_ip_v1_0_M00_AXIS #(
        parameter integer C_M_AXIS_TDATA_WIDTH	= 32
    )   
	(
		input wire  M_AXIS_ACLK,
		input wire  M_AXIS_ARESETN,
		output wire  M_AXIS_TVALID,
		output wire [C_M_AXIS_TDATA_WIDTH-1 : 0] M_AXIS_TDATA,
		output wire [(C_M_AXIS_TDATA_WIDTH/8)-1 : 0] M_AXIS_TSTRB,
		output wire  M_AXIS_TLAST,
		input wire  M_AXIS_TREADY
	);

    reg [C_M_AXIS_TDATA_WIDTH-1:0] stream_data_out;
    reg stream_valid = 1'b0;
	always @(posedge M_AXIS_ACLK)                                             
    begin                                                                     
	   if (!M_AXIS_ARESETN) begin
	       stream_data_out <= 0;
	       stream_valid <= 1'b0;
	   end 
	   else begin
	       stream_valid <= 1'b1;
	       if (M_AXIS_TREADY) begin
	           stream_data_out <= stream_data_out + 1;
	       end
	   end
    end
    assign M_AXIS_TVALID = stream_valid;
    assign M_AXIS_TDATA = stream_data_out;
    assign M_AXIS_TLAST	= M_AXIS_TDATA[3:0] == 4'b1000;
    assign M_AXIS_TSTRB	= {(C_M_AXIS_TDATA_WIDTH/8){1'b1}};

endmodule
