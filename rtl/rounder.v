`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/09/2026 08:28:05 PM
// Design Name: 
// Module Name: rounder
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module rounder(
    input sign_in,
    input [5:0] exp_in,
    input [2:0] m_in,
    input round_bit,
    input sticky_bit,
    output [7:0] c_out
    );
    
    wire low_bit = m_in[0];
    
    //round up if round&sticky, or round&low is odd (round to even)
    wire round = round_bit & (sticky_bit | low_bit);
    wire [3:0] round_m = m_in + round;

    //exponent stuffy
    wire [5:0] new_exp = exp_in + round_m[3]; 
    
    //saturation
    wire sat = (new_exp > 15) ? 1'b1 : 1'b0;
    
    assign c_out = (exp_in == 6'd0) ? 8'h00 :
                   sat ? {sign_in, 7'h7F} : {sign_in, new_exp[3:0], round_m[2:0]};
    
    
    
endmodule
