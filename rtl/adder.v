`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/09/2026 09:09:36 PM
// Design Name: 
// Module Name: adder
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


module adder(
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
    wire [5:0] exp_a = {2'd0, a[6:3]};
    wire [5:0] exp_b = {2'd0, b[6:3]};
    
    wire a_zero = (a == 8'h00 || a == 8'h80);
    wire b_zero = (b == 8'h00 || b == 8'h80);
    
    wire [6:0] f_a = a_zero ? 7'd0 : {1'b1, a[2:0], 3'd0}; //bottom 3 bits to catch shifted bits
    wire [6:0] f_b = b_zero ? 7'd0 : {1'b1, b[2:0], 3'd0}; //guard, round, other one i forgot name
    
//    reg [3:0] shift_val;
    reg [6:0] shift_a, shift_b;
    reg [5:0] shift_exp;
    reg sign;
    reg shift;
    
    reg [7:0] sum;
    
    //shifting exponents and gettin shifted fractions
    always @(*) begin
       if (exp_a < exp_b) begin
          shift_a = f_a >> (exp_b - exp_a);
          shift_b = f_b;
          shift_exp = exp_b;        
       end else begin //handles both b<a and b=a     
          shift_b = f_b >> (exp_a - exp_b);//will shift 0 if same
          shift_exp = exp_a;
          shift_a = f_a;         
       end
    end

    //adding with magnitude rule or wahtever stupiod shit
    always @(*) begin
       if (sign_a == sign_b) begin
          sign = sign_a;
          sum = shift_a + shift_b;
       end else if (shift_b > shift_a) begin
          sum = shift_b - shift_a;
          sign = sign_b;
       end else if (shift_a > shift_b) begin
          sum = shift_a - shift_b;
          sign = sign_a;
       end else begin //if same but opposite sign = 0
          sum = 8'd0;
          sign = 1'b0;
       end
    end

    //normalize
    reg [5:0] norm_exp;
    reg [7:0] norm_sum;
    
    //7 is overflow, 6 is imlplicit 1, [5:3] fraction, [2:0] guard, round, STICKY
    always @(*) begin
       casez (sum)
          8'd0: begin
               norm_exp = 0;
               norm_sum = 0;
          end
          8'b00000001: begin
               norm_sum = sum << 6;
               norm_exp = shift_exp - 6;
          end
          8'b0000001?: begin
               norm_sum = sum << 5;
               norm_exp = shift_exp - 5;
          end
          8'b000001??: begin
               norm_sum = sum << 4;
               norm_exp = shift_exp - 4;
          end
          8'b00001???: begin
               norm_sum = sum << 3;
               norm_exp = shift_exp - 3;
          end
          8'b0001????: begin
               norm_sum = sum << 2;
               norm_exp = shift_exp - 2;
          end
          8'b001?????: begin
               norm_sum = sum << 1;
               norm_exp = shift_exp - 1;
          end
          8'b01??????: begin
               norm_sum = sum;
               norm_exp = shift_exp;
          end
          8'b1???????: begin
               norm_sum = sum >> 1;
               norm_exp = shift_exp + 1;
          end
       endcase
    
    end
    
    assign sign_out = sign;
    
    assign exp_out = norm_exp;
    assign m_out = norm_sum[5:3]; //3 mantissa bit to kepep
    assign round_bit = norm_sum[2];
    assign sticky_bit = (norm_sum[1]||norm_sum[0]);
    
//    assign m_out = shift ? sum[6:4] : sum[5:3];
//    assign round_bit = shift ? sum[3] : sum[2];
//    assign sticky_bit = shift ? (sum[2]||sum[1]||sum[0]) :
//                                (sum[1]||sum[0]);
    
endmodule
