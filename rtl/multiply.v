`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/09/2026 07:24:28 PM
// Design Name: 
// Module Name: multiply
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


module multiply(
    input [7:0] a,
    input [7:0] b,
    input [5:0] bias,
    output sign_out,
    output [5:0] exp_out,
    output [2:0] m_out, //3 mantissa bit to kepep
    output round_bit, //4th fraction bit
    output sticky_bit //or of all lower bits
    );
    
    wire sign_a = a[7];
    wire sign_b = b[7];
    wire [3:0] exp_a = a[6:3];
    wire [3:0] exp_b = b[6:3];
    wire [3:0] f_a = {1'b1, a[2:0]}; //both are normal so 1 is msb
    wire [3:0] f_b = {1'b1, b[2:0]}; //both are normal so 1 is msb
    
    //check for zero in this module
    wire is_zero = (a == 8'h00) || (a == 8'h80) || (b == 8'h00) || (b == 8'h80);
    
    //get sign bit
    assign sign_out = is_zero ? 1'b0 : (sign_a ^ sign_b);
    
    //multiply them johns
    wire [7:0] product = f_a * f_b;
    wire shift = product[7]; //product[7:6].product[5:0]
    
    assign m_out = is_zero ? 3'd0 : shift ? product[6:4] : product[5:3];
    assign round_bit = is_zero ? 3'd0 : shift ? product[3] : product[2];
    assign sticky_bit = is_zero ? 3'd0 : shift ? (product[2]||product[1]||product[0]) :
                                (product[1]||product[0]);
    
    //get exponant exponent
    assign exp_out = is_zero ? 6'd0 : (shift + exp_a + exp_b - bias);
    
endmodule
