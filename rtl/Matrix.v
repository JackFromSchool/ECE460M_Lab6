`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/09/2026 07:19:42 PM
// Design Name: 
// Module Name: systolic_array
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


module Matrix(
   input wire clk,
   input wire reset,
   input wire start,
   input wire [5:0] bias,
   input wire [7:0] a00, a01, a02, a10, a11, a12, a20, a21, a22,
   input wire [7:0] b00, b01, b02, b10, b11, b12, b20, b21, b22,
   output reg busy,
   output reg done,
   output wire [7:0] c00, c01, c02, c10, c11, c12, c20, c21, c22
   );
   
   //locked values of bias, a and b matrix so chnage wont affect them
   reg [5:0] r_bias;
   reg [7:0] r_a00, r_a01, r_a02, r_a10, r_a11, r_a12, r_a20, r_a21, r_a22;
   reg [7:0] r_b00, r_b01, r_b02, r_b10, r_b11, r_b12, r_b20, r_b21, r_b22;
   
   reg [7:0] a_out00, a_out10, a_out20;
   wire [7:0] a_out01, a_out11, a_out12;
   wire [7:0] a_out02, a_out21, a_out22;
   
   reg [7:0] b_out00, b_out01, b_out02;
   wire [7:0] b_out10, b_out11, b_out12;
   wire [7:0] b_out20, b_out21, b_out22;
   
   //wires for cout
   wire [7:0] c_out00, c_out01, c_out02, c_out10, c_out11, c_out12, c_out20, c_out21, c_out22;
   
   wire clear = reset | (!busy & start);
   
    mac mac00(
    .clk(clk),
    .reset(clear),
    .a_in(a_out00),
    .b_in(b_out00),
    .bias(r_bias),
    .a_out(a_out01),
    .b_out(b_out10),
    .c_out(c_out00)
    );
    
    mac mac01(
    .clk(clk),
    .reset(clear),
    .a_in(a_out01),
    .b_in(b_out01),
    .bias(r_bias),
    .a_out(a_out02),
    .b_out(b_out11),
    .c_out(c_out01)
    );
    
    mac mac02(
    .clk(clk),
    .reset(clear),
    .a_in(a_out02),
    .b_in(b_out02),
    .bias(r_bias),
    .a_out(),
    .b_out(b_out12),
    .c_out(c_out02)
    );
    
    mac mac10(
    .clk(clk),
    .reset(clear),
    .a_in(a_out10),
    .b_in(b_out10),
    .bias(r_bias),
    .a_out(a_out11),
    .b_out(b_out20),
    .c_out(c_out10)
    );
    
    mac mac11(
    .clk(clk),
    .reset(clear),
    .a_in(a_out11),
    .b_in(b_out11),
    .bias(r_bias),
    .a_out(a_out12),
    .b_out(b_out21),
    .c_out(c_out11)
    );
    
    mac mac12(
    .clk(clk),
    .reset(clear),
    .a_in(a_out12),
    .b_in(b_out12),
    .bias(r_bias),
    .a_out(),
    .b_out(b_out22),
    .c_out(c_out12)
    );
    
    mac mac20(
    .clk(clk),
    .reset(clear),
    .a_in(a_out20),
    .b_in(b_out20),
    .bias(r_bias),
    .a_out(a_out21),
    .b_out(),
    .c_out(c_out20)
    );
    
    mac mac21(
    .clk(clk),
    .reset(clear),
    .a_in(a_out21),
    .b_in(b_out21),
    .bias(r_bias),
    .a_out(a_out22),
    .b_out(),
    .c_out(c_out21)
    );
    
    mac mac22(
    .clk(clk),
    .reset(clear),
    .a_in(a_out22),
    .b_in(b_out22),
    .bias(r_bias),
    .a_out(),
    .b_out(),
    .c_out(c_out22)
    );
        
    reg [2:0] cycle = 3'd0;
    
    always @(posedge clk) begin
       if (reset) begin
          busy <= 0;
          done <= 0;
          cycle <= 3'd0;
          a_out00 <= 8'd0; b_out00 <= 8'd0;
          a_out10 <= 8'd0; b_out01 <= 8'd0;
          a_out20 <= 8'd0; b_out02 <= 8'd0;
          
       end else begin
          if (!busy & start) begin
             r_bias <= bias;
             busy <= 1;
             cycle <= 3'd0;
             done <= 0;
             
             //latch values
             r_a00 <= a00; r_a01 <= a01; r_a02 <= a02; r_a10 <= a10; r_a11 <= a11; r_a12 <= a12; r_a20 <= a20; r_a21 <= a21; r_a22 <= a22;
             r_b00 <= b00; r_b01 <= b01; r_b02 <= b02; r_b10 <= b10; r_b11 <= b11; r_b12 <= b12; r_b20 <= b20; r_b21 <= b21; r_b22 <= b22;
             
             //set flow wwires to cycle 1 bc of start
             a_out00 <= a00; b_out00 <= b00;
             a_out10 <= 8'd0; b_out01 <= 8'd0;
             a_out20 <= 8'd0; b_out02 <= 8'd0;
          end else if (busy) begin
             cycle <= cycle + 1;
             
             case (cycle) //cycle updates every clk
                3'd0: begin
                   a_out00 <= r_a01; b_out00 <= r_b10;
                   a_out10 <= r_a10; b_out01 <= r_b01;
                   a_out20 <= 8'd0; b_out02 <= 8'd0;
                end
                3'd1: begin
                   a_out00 <= r_a02; b_out00 <= r_b20;
                   a_out10 <= r_a11; b_out01 <= r_b11;
                   a_out20 <= r_a20; b_out02 <= r_b02;
                end
                3'd2: begin
                   a_out00 <= 8'd0; b_out00 <= 8'd0;
                   a_out10 <= r_a12; b_out01 <= r_b21;
                   a_out20 <= r_a21; b_out02 <= r_b12;
                end
                
                3'd3: begin
                   a_out00 <= 8'd0; b_out00 <= 8'd0;
                   a_out10 <= 8'd0; b_out01 <= 8'd0;
                   a_out20 <= r_a22; b_out02 <= r_b22;
                end
                
                3'd4: begin
                   a_out00 <= 8'd0; b_out00 <= 8'd0;
                   a_out10 <= 8'd0; b_out01 <= 8'd0;
                   a_out20 <= 8'd0; b_out02 <= 8'd0;
                end
                
                3'd5: begin                
                   a_out00 <= 8'd0; b_out00 <= 8'd0;
                   a_out10 <= 8'd0; b_out01 <= 8'd0;
                   a_out20 <= 8'd0; b_out02 <= 8'd0;
                end
                
                3'd6: begin    
                   busy <= 0;       
                   done <= 1;        
                   a_out00 <= 8'd0; b_out00 <= 8'd0; 
                   a_out10 <= 8'd0; b_out01 <= 8'd0;
                   a_out20 <= 8'd0; b_out02 <= 8'd0;
                end
                default: begin
                   done <= 0;
                   a_out00 <= 8'd0; b_out00 <= 8'd0;
                   a_out10 <= 8'd0; b_out01 <= 8'd0;
                   a_out20 <= 8'd0; b_out02 <= 8'd0;
                end
             endcase
          end
             
       end
    end
    
    assign c00 = done ? c_out00 : 8'd0;
    assign c01 = done ? c_out01 : 8'd0;
    assign c02 = done ? c_out02 : 8'd0;
    assign c10 = done ? c_out10 : 8'd0;
    assign c11 = done ? c_out11 : 8'd0;
    assign c12 = done ? c_out12 : 8'd0;
    assign c20 = done ? c_out20 : 8'd0;
    assign c21 = done ? c_out21 : 8'd0;
    assign c22 = done ? c_out22 : 8'd0;
    
endmodule
