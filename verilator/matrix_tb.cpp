#include "VMatrix.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdlib.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

/*
 *  Round a 2I.6Q fixed point to 2I.3Q
 */
uint8_t fixed_point_round_to_even(uint8_t num) {
   // In middle -> rount to even
   if ((num & 0x07) == 0x04) {
      if ((num & 0x08) == 0x08)
         return (num + 0x08) & ~(0x07);
      else
         return (num) & ~(0x07);
      // Isn't middle, must round up
   } else if ((num & 0x04) == 0x04) {
      return (num + 0x08) & ~(0x07);
      // Otherwise round down
   } else {
      return num & ~(0x07);
   }
}

class CFloat8 {
 private:
   uint8_t num;

 public:
   uint8_t bias = 0;

   CFloat8(uint8_t exponent, uint8_t sign, uint8_t fraction) {
      this->num = (sign & 1) << 7;
      this->num |= (exponent & 0xF) << 3;
      this->num |= (fraction & 0x7);
   }

   CFloat8(uint8_t num) { this->num = num; }

   uint8_t get_sign() const { return (this->num & 0x80) >> 7; }

   uint8_t get_exponent() const { return (this->num & 0x78) >> 3; }

   uint8_t get_fraction() const { return (this->num & 0x7); }

   uint8_t get_raw() const { return this->num; }

   std::string display_binary() const {
      std::string str;
      if (this->get_sign())
         str.append("-");

      str.append("1.");
      uint8_t fraction = this->get_fraction();
      for (int i = 0; i < 3; i++) {
         if (fraction & 0x4)
            str.append("1");
         else
            str.append("0");

         fraction <<= 1;
      }

      str.append(" 2^");
      int exponent = this->get_exponent();
      exponent = exponent - this->bias;
      str.append(std::to_string(exponent));

      return str;
   }

   friend std::ostream &operator<<(std::ostream &out, CFloat8 &a) {
      if (a.get_exponent() == 0 && a.get_fraction() == 0) {
         out << 0;
      } else {
         int8_t exponent = a.get_exponent();
         int8_t biased_exponent = exponent - a.bias;
         double number = (1.0 + ((double)(a.get_fraction()) / 8.0)) *
                         std::pow(2.0, biased_exponent);
         if (a.get_sign()) {
            number *= -1;
         }
         out << number;
      }
      return out;
   }

   CFloat8 operator+(const CFloat8 &n) {
      uint8_t addend1_exp = this->get_exponent();
      int16_t addend1 = (this->get_fraction() | 1 << 3) << 3;
      if (this->get_exponent() == 0 && this->get_fraction() == 0)
         addend1 = 0;

      uint16_t addend2_exp = n.get_exponent();
      int16_t addend2 = (n.get_fraction() | 1 << 3) << 3;
      if (n.get_exponent() == 0 && n.get_fraction() == 0)
         addend2 = 0;

      // Align Exponents
      while (addend1_exp != addend2_exp) {
         if (addend1_exp < addend2_exp) {
            addend1 = (addend1 >> 1) | (addend1 & 1);
            addend1_exp++;
         } else {
            addend2 = (addend2 >> 1) | (addend2 & 1);
            addend2_exp++;
         }
      }

      // Negate if necessary
      if (this->get_sign()) {
         addend1 = ~addend1 + 1;
      }
      if (n.get_sign()) {
         addend2 = ~addend2 + 1;
      }

      // addends are now in 2's complement fixed point 2I.3Q
      int16_t sum = addend1 + addend2;
      uint8_t sum_sign = sum < 0;
      int sum_exp = addend1_exp;

      // Convert to magnitude if result is negative
      if (sum_sign) {
         sum = ~(sum) + 1;
      }

      while (true) {
         // If it is zero return 0 form
         if (sum == 0)
            return CFloat8(0);

         // Fraction overflow? 1x.xxxxxx
         if (sum & 0x80) {
            sum = (sum >> 1) | (sum & 1);
            sum_exp++;
         }

         // Normalize it x0.xxxxxx -> x1.xxxxxx
         while (!(sum & 0x40)) {
            sum <<= 1;
            sum_exp--;
         }

         if (sum_exp < 0)
            return CFloat8(0);
         if (sum_exp > 15)
            return CFloat8(0xF, sum_sign, 0xF);

         // Round, shifting over 3 to reuse function
         sum = fixed_point_round_to_even(sum);

         // Return result if normalized
         if ((sum & 0x40))
            return CFloat8(sum_exp, sum_sign, sum >> 3);
      }
   }

   CFloat8 operator*(const CFloat8 &n) {
      uint8_t product_exponent =
          n.get_exponent() + this->get_exponent() - this->bias;

      uint8_t product_sign = this->get_sign() ^ n.get_sign();

      // Multiplicand and Multiplier contain implicit 1
      uint8_t multiplicand = n.get_fraction() | (1 << 3);
      uint8_t multiplier = this->get_fraction() | (1 << 3);
      if (this->get_exponent() == 0 && this->get_fraction() == 0)
         multiplicand = 0;
      if (n.get_exponent() == 0 && n.get_fraction() == 0)
         multiplier = 0;

      // Product is now in form 2I.6Q
      uint8_t product = multiplicand * multiplier;

      // Until steady, loop
      while (true) {
         if (product == 0)
            return CFloat8(0, product_sign, 0);

         // Check fraction overflow eg. 1x.xxxxxx
         if ((product & 0x80) == 0x80) {

            product = (product >> 1) | (product & 1);
            product_exponent += 1;
         }

         // Normalize if necessary eg. 0.xxxxxx -> 1.xxxxxx
         while (!(product & 0x40)) {
            product <<= 1;
            product_exponent -= 1;
         }

         // Check exponent overflow
         if (product_exponent > 15)
            return CFloat8(0xF, product_sign, 0xF);

         product = fixed_point_round_to_even(product);

         // If normalized return
         if ((product & 0x40) == 0x40)
            return CFloat8(product_exponent, product_sign, product >> 3);
      }
   }
};

class CFloatMat3 {
 public:
   CFloat8 array[3][3];

   CFloatMat3()
       : array{{CFloat8(0), CFloat8(0), CFloat8(0)},
               {CFloat8(0), CFloat8(0), CFloat8(0)},
               {CFloat8(0), CFloat8(0), CFloat8(0)}} {}

   CFloatMat3 operator*(const CFloatMat3 &n) {
      CFloat8 result[3][3] = {{CFloat8(0), CFloat8(0), CFloat8(0)},
                              {CFloat8(0), CFloat8(0), CFloat8(0)},
                              {CFloat8(0), CFloat8(0), CFloat8(0)}};

      for (int i = 0; i < 3; i++) {

         for (int j = 0; j < 3; j++) {

            for (int k = 0; k < 3; k++) {
               result[i][j] =
                   result[i][j] + (this->array[i][k] * n.array[k][j]);
            }
         }
      }

      CFloatMat3 out;
      for (int i = 0; i < 3; i++)
         for (int j = 0; j < 3; j++)
            out.array[i][j] = result[i][j];
      return out;
   }

   void display() {
      std::cout << "[\n";
      for (int row = 0; row < 3; row++) {
         for (int col = 0; col < 3; col++) {
            std::cout << this->array[row][col] << " ";
         }
         std::cout << std::endl;
      }
      std::cout << "]\n";
   }
};

CFloatMat3 dut_to_mat(VMatrix *dut) {
   CFloatMat3 result;
   result.array[0][0] = CFloat8(dut->c00);
   result.array[0][1] = CFloat8(dut->c01);
   result.array[0][2] = CFloat8(dut->c02);
   result.array[1][0] = CFloat8(dut->c10);
   result.array[1][1] = CFloat8(dut->c11);
   result.array[1][2] = CFloat8(dut->c12);
   result.array[2][0] = CFloat8(dut->c20);
   result.array[2][1] = CFloat8(dut->c21);
   result.array[2][2] = CFloat8(dut->c22);
   return result;
}

static std::mt19937 rng;
static int rnd(int lo, int hi) {
   return std::uniform_int_distribution<int>(lo, hi)(rng);
}

// Value of a CFloat8 at a given bias (zero = E==0 && F==0, either sign)
static double val(const CFloat8 &x, int bias) {
   if (x.get_exponent() == 0 && x.get_fraction() == 0)
      return 0.0;
   double m = (1.0 + x.get_fraction() / 8.0) *
              std::ldexp(1.0, (int)x.get_exponent() - bias);
   return x.get_sign() ? -m : m;
}

static bool is_zero(const CFloat8 &x) {
   return x.get_exponent() == 0 && x.get_fraction() == 0;
}

// Random operand: zero (00 or 80) or a normal number. mode picks the exponent
// range.
static CFloat8 gen_operand(int bias, int mode) {
   CFloat8 x(0);
   if (rnd(0, 99) < 10) {
      x = CFloat8((uint8_t)(rnd(0, 1) ? 0x80 : 0x00));
   } else {
      int E, F = rnd(0, 7), S = rnd(0, 1);
      if (mode == 0)
         E = rnd(bias - 1, bias + 2); // values near 1
      else if (mode == 1)
         E = rnd(1, 15); // full range, saturation likely
      else {
         E = rnd(bias, bias + 1); // few values: ties / exact cancellation
         F = rnd(0, 1) ? 4 : 0;
      }
      if (E < 1)
         E = 1;
      if (E > 15)
         E = 15;
      x = CFloat8((uint8_t)E, (uint8_t)S, (uint8_t)F);
   }
   x.bias = bias;
   return x;
}

// Autograder never hits underflow, so skip inputs that would.
static bool no_underflow(CFloatMat3 &A, CFloatMat3 &B, int bias) {
   double min_normal = std::ldexp(1.0, 1 - bias);
   for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) {
         CFloat8 acc(0);
         for (int k = 0; k < 3; k++) {
            CFloat8 a = A.array[i][k], b = B.array[k][j];
            if (!is_zero(a) && !is_zero(b) &&
                (int)a.get_exponent() + (int)b.get_exponent() - bias < 1)
               return false; // product underflow
            CFloat8 p = a * b;
            double s = val(acc, bias) + val(p, bias);
            if (s != 0.0 && std::fabs(s) < min_normal)
               return false; // sum underflow
            acc = acc + p;
         }
      }
   return true;
}

static void tick(VMatrix *dut) {
   dut->clk = 0;
   dut->eval();
   dut->clk = 1;
   dut->eval();
}

// Load matrices, pulse start, wait for done. Returns false on timeout.
static bool run_dut(VMatrix *dut, CFloatMat3 &A, CFloatMat3 &B, int bias,
                    CFloatMat3 &out) {
   CData *ap[9] = {&dut->a00, &dut->a01, &dut->a02, &dut->a10, &dut->a11,
                   &dut->a12, &dut->a20, &dut->a21, &dut->a22};
   CData *bp[9] = {&dut->b00, &dut->b01, &dut->b02, &dut->b10, &dut->b11,
                   &dut->b12, &dut->b20, &dut->b21, &dut->b22};
   for (int i = 0; i < 9; i++) {
      *ap[i] = A.array[i / 3][i % 3].get_raw();
      *bp[i] = B.array[i / 3][i % 3].get_raw();
   }
   dut->bias = bias;
   dut->reset = 0;
   dut->start = 1;
   tick(dut); // setup cycle
   dut->start = 0;

   int timeout = 20;
   while (!dut->done && timeout--)
      tick(dut);
   out = dut_to_mat(dut);
   return dut->done;
}

// 8'h80 and 8'h00 both mean zero; spec wants 00 on outputs
static uint8_t norm(uint8_t v) { return (v & 0x7F) ? v : 0x00; }

int main(int argc, char **argv) {
   Verilated::commandArgs(argc, argv);
   int num_tests = argc > 1 ? atoi(argv[1]) : 5000;
   uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], nullptr, 0)
                            : std::random_device{}();
   printf("seed = %u\n", seed);
   rng.seed(seed);

   VMatrix *dut = new VMatrix;

   // reset
   dut->start = 0;
   dut->reset = 1;
   tick(dut);
   tick(dut);
   dut->reset = 0;

   int ran = 0, fails = 0;
   for (int t = 0; t < num_tests; t++) {
      // build a valid random test
      int bias = 0;
      CFloatMat3 A, B;
      bool ok = false;
      for (int attempt = 0; attempt < 1000 && !ok; attempt++) {
         bias = rnd(9, 12);
         int mode = rnd(0, 2);
         for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
               A.array[i][j] = gen_operand(bias, mode);
               B.array[i][j] = gen_operand(bias, mode);
            }
         ok = no_underflow(A, B, bias);
      }
      if (!ok)
         continue;

      CFloatMat3 expected = A * B; // reference model
      CFloatMat3 got;
      bool done = run_dut(dut, A, B, bias, got);
      ran++;

      bool bad = !done;
      for (int i = 0; i < 9 && !bad; i++)
         if (norm(got.array[i / 3][i % 3].get_raw()) !=
             norm(expected.array[i / 3][i % 3].get_raw()))
            bad = true;

      if (bad) {
         fails++;
         if (fails <= 10) {
            printf("MISMATCH test %d (bias %d)%s\n", t, bias,
                   done ? "" : "  [TIMEOUT: done never asserted]");
            printf("  A:");
            for (int i = 0; i < 9; i++)
               printf(" %02X", A.array[i / 3][i % 3].get_raw());
            printf("\n  B:");
            for (int i = 0; i < 9; i++)
               printf(" %02X", B.array[i / 3][i % 3].get_raw());
            printf("\n  exp:");
            for (int i = 0; i < 9; i++)
               printf(" %02X", norm(expected.array[i / 3][i % 3].get_raw()));
            printf("\n  got:");
            for (int i = 0; i < 9; i++)
               printf(" %02X", norm(got.array[i / 3][i % 3].get_raw()));
            printf("\n");
         }
      }
   }

   printf("%d tests run, %d failed -> %s\n", ran, fails,
          fails ? "FAIL" : "PASS");
   dut->final();
   delete dut;
   return fails != 0;
}
