`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/09/2026 06:35:22 PM
// Design Name: 
// Module Name: mac
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


module mac(
    input clk,
    input reset,
    input [7:0] a_in,
    input [7:0] b_in,
    input [5:0] bias,
    output reg [7:0] a_out,
    output reg [7:0] b_out,
    output [7:0] c_out
    );
    
    reg [7:0] ACC = 8'h00;//initialize to 0
    wire [7:0] ACC_next;
    
    wire [7:0] p;
    
    wire m_sign, m_round, m_sticky;
    wire [5:0] m_exp;
    wire [2:0] m_mant;
    
    multiply mult(
     .a(a_in),
     .b(b_in),
     .bias(bias),
     .sign_out(m_sign),
     .exp_out(m_exp),
     .m_out(m_mant), //3 mantissa bit to kepep
     .round_bit(m_round), //4th fraction bit
     .sticky_bit(m_sticky) //or of all lower bits
    );   
     
     
    rounder m_rounder(
     .sign_in(m_sign),
     .exp_in(m_exp),
     .m_in(m_mant),
     .round_bit(m_round),
     .sticky_bit(m_sticky),
     .c_out(p)
    );
    
    wire a_sign, a_round, a_sticky;
    wire [5:0] a_exp;
    wire [2:0] a_mant;
    adder add(
     .a(ACC),
     .b(p),
     .bias(bias),
     .sign_out(a_sign),
     .exp_out(a_exp),
     .m_out(a_mant), //3 mantissa bit to kepep
     .round_bit(a_round), //4th fraction bit
     .sticky_bit(a_sticky) //or of all lower bits
    );
    
    rounder a_rounder(
     .sign_in(a_sign),
     .exp_in(a_exp),
     .m_in(a_mant),
     .round_bit(a_round),
     .sticky_bit(a_sticky),
     .c_out(ACC_next)
    );

    always @(posedge clk) begin
       if (reset) begin
          ACC <= 0;
          a_out <= 0;
          b_out <= 0;
       end else begin
          ACC <= ACC_next;
          a_out <= a_in;
          b_out <= b_in;
       end
    end
    
    assign c_out = ACC;
    
    
endmodule
