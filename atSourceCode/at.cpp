// Copyright (c) 2014 CIYAM Developers
//
// Distributed under the MIT/X11 software license, please refer to the file license.txt
// in the root project directory or http://www.opensource.org/licenses/mit-license.php.
#include <cstdlib>
#include <memory.h>

#ifndef _WIN32
#  include <stdint.h>
#else
#  ifdef _MSC_VER
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
#  endif
#endif

#include <map>
#include <set>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <stdexcept>

/* Basic Test Cases
(output value)
> code 0100000000b8220000000000003301000000000028
> run
8888

(testing loop)
> code 35020000000000330100000000001e00000000f228
> run
1
2
3
4
5
6
7
8
9
0

(explicit loop)
> code 010000000003000000000000000500000000330100000000001e00000000f428
> run
2
1
0
*/

using namespace std;

const int32_t c_code_page_bytes = 512;
const int32_t c_data_page_bytes = 512;

const int32_t c_call_stack_page_bytes = 256;
const int32_t c_user_stack_page_bytes = 256;

int32_t g_code_pages = 1;
int32_t g_data_pages = 1;

int32_t g_call_stack_pages = 1;
int32_t g_user_stack_pages = 1;

const int64_t c_default_balance = 100;

const int32_t c_max_to_multiply = 0x1fffffff;

int64_t g_val = 0;
int64_t g_val1 = 0;

int64_t g_balance = c_default_balance;

bool g_first_call = true;

int32_t g_increment_func = 0;

enum op_code
{
   e_op_code_NOP = 0x7f,
   e_op_code_SET_VAL = 0x01,
   e_op_code_SET_DAT = 0x02,
   e_op_code_CLR_DAT = 0x03,
   e_op_code_INC_DAT = 0x04,
   e_op_code_DEC_DAT = 0x05,
   e_op_code_ADD_DAT = 0x06,
   e_op_code_SUB_DAT = 0x07,
   e_op_code_MUL_DAT = 0x08,
   e_op_code_DIV_DAT = 0x09,
   e_op_code_BOR_DAT = 0x0a,
   e_op_code_AND_DAT = 0x0b,
   e_op_code_XOR_DAT = 0x0c,
   e_op_code_NOT_DAT = 0x0d,
   e_op_code_SET_IND = 0x0e,
   e_op_code_SET_IDX = 0x0f,
   e_op_code_PSH_DAT = 0x10,
   e_op_code_POP_DAT = 0x11,
   e_op_code_JMP_SUB = 0x12,
   e_op_code_RET_SUB = 0x13,
   e_op_code_IND_DAT = 0x14,
   e_op_code_IDX_DAT = 0x15,
   e_op_code_MOD_DAT = 0x16,
   e_op_code_SHL_DAT = 0x17,
   e_op_code_SHR_DAT = 0x18,
   e_op_code_JMP_ADR = 0x1a,
   e_op_code_BZR_DAT = 0x1b,
   e_op_code_BNZ_DAT = 0x1e,
   e_op_code_BGT_DAT = 0x1f,
   e_op_code_BLT_DAT = 0x20,
   e_op_code_BGE_DAT = 0x21,
   e_op_code_BLE_DAT = 0x22,
   e_op_code_BEQ_DAT = 0x23,
   e_op_code_BNE_DAT = 0x24,
   e_op_code_SLP_DAT = 0x25,
   e_op_code_FIZ_DAT = 0x26,
   e_op_code_STZ_DAT = 0x27,
   e_op_code_FIN_IMD = 0x28,
   e_op_code_STP_IMD = 0x29,
   e_op_code_SLP_IMD = 0x2a,
   e_op_code_ERR_ADR = 0x2b,
   e_op_code_SET_PCS = 0x30,
   e_op_code_EXT_FUN = 0x32,
   e_op_code_EXT_FUN_DAT = 0x33,
   e_op_code_EXT_FUN_DAT_2 = 0x34,
   e_op_code_EXT_FUN_RET = 0x35,
   e_op_code_EXT_FUN_RET_DAT = 0x36,
   e_op_code_EXT_FUN_RET_DAT_2 = 0x37,
};

struct machine_state
{
   machine_state( )
   {
      pce = pcs = 0;
      reset( );
   }

   void reset( )
   {
      pc = pcs;
      opc = 0;

      cs = 0;
      us = 0;

      steps = 0;

      a1 = 0;
      a2 = 0;
      a3 = 0;
      a4 = 0;

      b1 = 0;
      b2 = 0;
      b3 = 0;
      b4 = 0;

      jumps.clear( );

      stopped = false;
      finished = false;
   }

   bool stopped; // transient
   bool finished; // transient

   int32_t pc;
   int32_t pce;
   int32_t pcs;

   int32_t opc; // transient

   int32_t cs;
   int32_t us;

   int64_t a1;
   int64_t a2;
   int64_t a3;
   int64_t a4;

   int64_t b1;
   int64_t b2;
   int64_t b3;
   int64_t b4;

   int32_t steps;

   int32_t sleep_until;

   set< int32_t > jumps; // transient
};

struct function_data
{
   function_data( )
   {
      loop = false;
      offset = 0;
   }

   bool loop;
   size_t offset;

   vector< int64_t > data;
};

string decode_function_name( int16_t fun, int8_t op )
{
   ostringstream osstr;

   int8_t op_expected = 0;

   switch( fun )
   {
      case 0x0100:
      osstr << "Get_A1";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0101:
      osstr << "Get_A2";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0102:
      osstr << "Get_A3";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0103:
      osstr << "Get_A4";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0104:
      osstr << "Get_B1";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0105:
      osstr << "Get_B2";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0106:
      osstr << "Get_B3";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0107:
      osstr << "Get_B4";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0110:
      osstr << "Set_A1";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0111:
      osstr << "Set_A2";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0112:
      osstr << "Set_A3";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0113:
      osstr << "Set_A4";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0114:
      osstr << "Set_A1_A2";
      op_expected = e_op_code_EXT_FUN_DAT_2;
      break;

      case 0x0115:
      osstr << "Set_A3_A4";
      op_expected = e_op_code_EXT_FUN_DAT_2;
      break;

      case 0x0116:
      osstr << "Set_B1";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0117:
      osstr << "Set_B2";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0118:
      osstr << "Set_B3";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0119:
      osstr << "Set_B4";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x011a:
      osstr << "Set_B1_B2";
      op_expected = e_op_code_EXT_FUN_DAT_2;
      break;

      case 0x011b:
      osstr << "Set_B3_B4";
      op_expected = e_op_code_EXT_FUN_DAT_2;
      break;

      case 0x0120:
      osstr << "Clear_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0121:
      osstr << "Clear_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0122:
      osstr << "Clear_A_And_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0123:
      osstr << "Copy_A_From_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0124:
      osstr << "Copy_B_From_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0125:
      osstr << "Check_A_Is_Zero";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0126:
      osstr << "Check_B_Is_Zero";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0127:
      osstr << "Check_A_Equals_B";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0128:
      osstr << "Swap_A_and_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0129:
      osstr << "OR_A_with_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x012a:
      osstr << "OR_B_with_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x012b:
      osstr << "AND_A_with_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x012c:
      osstr << "AND_B_with_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x012d:
      osstr << "XOR_A_with_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x012e:
      osstr << "XOR_B_with_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0200:
      osstr << "MD5_A_To_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0201:
      osstr << "Check_MD5_A_With_B";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0202:
      osstr << "HASH160_A_To_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0203:
      osstr << "Check_HASH160_A_With_B";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0204:
      osstr << "SHA256_A_To_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0205:
      osstr << "Check_SHA256_A_With_B";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0300:
      osstr << "Get_Block_Timestamp";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0301:
      osstr << "Get_Creation_Timestamp";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0302:
      osstr << "Get_Last_Block_Timestamp";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0303:
      osstr << "Put_Last_Block_Hash_In_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0304:
      osstr << "A_To_Tx_After_Timestamp";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0305:
      osstr << "Get_Type_For_Tx_In_A";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0306:
      osstr << "Get_Amount_For_Tx_In_A";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0307:
      osstr << "Get_Timestamp_For_Tx_In_A";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0308:
      osstr << "Get_Random_Id_For_Tx_In_A";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0309:
      osstr << "Message_From_Tx_In_A_To_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x030a:
      osstr << "B_To_Address_Of_Tx_In_A";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x030b:
      osstr << "B_To_Address_Of_Creator";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0400:
      osstr << "Get_Current_Balance";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0401:
      osstr << "Get_Previous_Balance";
      op_expected = e_op_code_EXT_FUN_RET;
      break;

      case 0x0402:
      osstr << "Send_To_Address_In_B";
      op_expected = e_op_code_EXT_FUN_DAT;
      break;

      case 0x0403:
      osstr << "Send_All_To_Address_In_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0404:
      osstr << "Send_Old_To_Address_In_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0405:
      osstr << "Send_A_To_Address_In_B";
      op_expected = e_op_code_EXT_FUN;
      break;

      case 0x0406:
      osstr << "Add_Minutes_To_Timestamp";
      op_expected = e_op_code_EXT_FUN_RET_DAT_2;
      break;

      default:
      osstr << "0x" << hex << setw( 4 ) << setfill( '0' ) << fun;
   }

   if( op && op_expected && op != op_expected )
      osstr << " *** invalid op ***";

   return osstr.str( );
}

map< int32_t, function_data > g_function_data;

int64_t get_function_data( int32_t func_num )
{
   if( func_num == g_increment_func )
   {
      if( g_first_call )
         g_first_call = false;
      else
      {
         for( map< int32_t, function_data >::iterator i = g_function_data.begin( ); i != g_function_data.end( ); ++i )
         {
            if( ++( i->second.offset ) >= i->second.data.size( ) )
            {
               if( i->second.loop )
                  i->second.offset = 0;
               else
                  --( i->second.offset );
            }
         }
      }
   }

   int64_t rc = g_function_data[ func_num ].data[ g_function_data[ func_num ].offset ];

   return rc;
}

int64_t func( int32_t func_num, machine_state& state )
{
   int64_t rc = 0;

   if( func_num == 1 )
      rc = g_val;
   else if( func_num == 2 ) // get a value
   {
      if( g_val == 9 )
         rc = g_val = 0;
      else
         rc = ++g_val;
   }
   else if( func_num == 3 ) // get size
      rc = 10;
   else if( func_num == 4 ) // get func num
      rc = func_num;
   else if( func_num == 25 || func_num == 32 ) // get balance (prior)
   {
      rc = g_balance;
      if( g_function_data.count( func_num ) )
      {
         cout << "(resetting function data)\n";
         g_first_call = true;
         for( map< int32_t, function_data >::iterator i = g_function_data.begin( ); i != g_function_data.end( ); ++i )
            i->second.offset = 0;
      }
   }
   else if( func_num == 0x0100 ) // Get_A1
      rc = state.a1;
   else if( func_num == 0x0101 ) // Get_A2
      rc = state.a2;
   else if( func_num == 0x0102 ) // Get_A3
      rc = state.a3;
   else if( func_num == 0x0103 ) // Get_A4
      rc = state.a4;
   else if( func_num == 0x0104 ) // Get_B1
      rc = state.b1;
   else if( func_num == 0x0105 ) // Get_B2
      rc = state.b2;
   else if( func_num == 0x0106 ) // Get_B3
      rc = state.b3;
   else if( func_num == 0x0107 ) // Get_B4
      rc = state.b4;
   else if( func_num == 0x0120 ) // Clear_A
   {
      state.a1 = 0;
      state.a2 = 0;
      state.a3 = 0;
      state.a4 = 0;
   }
   else if( func_num == 0x0121 ) // Clear_B
   {
      state.b1 = 0;
      state.b2 = 0;
      state.b3 = 0;
      state.b4 = 0;
   }
   else if( func_num == 0x0122 ) // Clear_A_And_B
   {
      state.a1 = state.b1 = 0;
      state.a2 = state.b2 = 0;
      state.a3 = state.b3 = 0;
      state.a4 = state.b4 = 0;
   }
   else if( func_num == 0x0123 ) // Copy_A_From_B
   {
      state.a1 = state.b1;
      state.a2 = state.b2;
      state.a3 = state.b3;
      state.a4 = state.b4;
   }
   else if( func_num == 0x0124 ) // Copy_B_From_A
   {
      state.b1 = state.a1;
      state.b2 = state.a2;
      state.b3 = state.a3;
      state.b4 = state.a4;
   }
   else if( func_num == 0x0125 ) // Check_A_Is_Zero
   {
      if( state.a1 == 0 && state.a2 == 0 && state.a3 == 0 && state.a4 == 0 )
         rc = true;
   }
   else if( func_num == 0x0126 ) // Check_B_Is_Zero
   {
      if( state.b1 == 0 && state.b2 == 0 && state.b3 == 0 && state.b4 == 0 )
         rc = true;
   }
   else if( func_num == 0x0127 ) // Check_A_Equals_B
   {
      if( state.a1 == state.b1 && state.a2 == state.b2 && state.a3 == state.b3 && state.a4 == state.b4 )
         rc = true;
   }
   else if( func_num == 0x0128 ) // Swap_A_and_B
   {
      int64_t tmp_a1 = state.a1;
      int64_t tmp_a2 = state.a2;
      int64_t tmp_a3 = state.a3;
      int64_t tmp_a4 = state.a4;

      state.a1 = state.b1;
      state.a2 = state.b2;
      state.a3 = state.b3;
      state.a4 = state.b4;

      state.b1 = tmp_a1;
      state.b2 = tmp_a2;
      state.b3 = tmp_a3;
      state.b4 = tmp_a4;
   }
   else if( func_num == 0x0129 ) // OR_A_with_B
   {
      state.a1 = state.a1 | state.b1;
      state.a2 = state.a2 | state.b2;
      state.a3 = state.a3 | state.b3;
      state.a4 = state.a4 | state.b4;
   }
   else if( func_num == 0x012a ) // OR_B_with_A
   {
      state.b1 = state.a1 | state.b1;
      state.b2 = state.a2 | state.b2;
      state.b3 = state.a3 | state.b3;
      state.b4 = state.a4 | state.b4;
   }
   else if( func_num == 0x012b ) // AND_A_with_B
   {
      state.a1 = state.a1 & state.b1;
      state.a2 = state.a2 & state.b2;
      state.a3 = state.a3 & state.b3;
      state.a4 = state.a4 & state.b4;
   }
   else if( func_num == 0x012c ) // AND_B_with_A
   {
      state.b1 = state.a1 & state.b1;
      state.b2 = state.a2 & state.b2;
      state.b3 = state.a3 & state.b3;
      state.b4 = state.a4 & state.b4;
   }
   else if( func_num == 0x012d ) // XOR_A_with_B
   {
      state.a1 = state.a1 ^ state.b1;
      state.a2 = state.a2 ^ state.b2;
      state.a3 = state.a3 ^ state.b3;
      state.a4 = state.a4 ^ state.b4;
   }
   else if( func_num == 0x012e ) // XOR_B_with_A
   {
      state.b1 = state.a1 ^ state.b1;
      state.b2 = state.a2 ^ state.b2;
      state.b3 = state.a3 ^ state.b3;
      state.b4 = state.a4 ^ state.b4;
   }
   else if( g_function_data.count( func_num ) )
      rc = get_function_data( func_num );

   if( func_num != 2 )
   {
      if( func_num < 0x100 )
         cout << "func: " << dec << func_num << " rc: " << hex << setw( 16 ) << setfill( '0' ) << rc << '\n';
      else
         cout << "func: " << decode_function_name( func_num, 0 )
          << " rc: 0x" << hex << setw( 16 ) << setfill( '0' ) << rc << '\n';
   }

   return rc;
}

int64_t func1( int32_t func_num, machine_state& state, int64_t value, int8_t* p_data = 0, int32_t dsize = 0 )
{
   int64_t rc = 0;

   if( func_num == 1 ) // echo
      cout << dec << value << '\n';
   else if( func_num == 2 )
      rc = value * 2; // double it
   else if( func_num == 3 )
      rc = value / 2; // halve it
   else if( func_num == 26 || func_num == 33 ) // pay balance (prior)
   {
      cout << "payout " << dec << g_balance << " to account: " << value << '\n';
      g_balance = 0;
   }
   else if( func_num == 0x0110 ) // Set_A1
      state.a1 = value;
   else if( func_num == 0x0111 ) // Set_A2
      state.a2 = value;
   else if( func_num == 0x0112 ) // Set_A3
      state.a3 = value;
   else if( func_num == 0x0113 ) // Set_A4
      state.a4 = value;
   else if( func_num == 0x0116 ) // Set_B1
      state.b1 = value;
   else if( func_num == 0x0117 ) // Set_B2
      state.b2 = value;
   else if( func_num == 0x0118 ) // Set_B3
      state.b3 = value;
   else if( func_num == 0x0119 ) // Set_B4
      state.b4 = value;
   else if( g_function_data.count( func_num ) )
      rc = get_function_data( func_num );

   if( func_num != 1 && func_num != 26 )
   {
      if( func_num < 0x100 )
         cout << "func1: " << dec << func_num << " with " << value
          << " rc: " << hex << setw( 16 ) << setfill( '0' ) << rc << '\n';
      else
         cout << "func1: " << decode_function_name( func_num, 0 )
          << " with " << value << " rc: 0x" << hex << setw( 16 ) << setfill( '0' ) << rc << '\n';
   }

   return rc;
}

int64_t func2( int32_t func_num, machine_state& state, int64_t value1, int64_t value2, int8_t* p_data = 0, int32_t dsize = 0 )
{
   int64_t rc = 0;

   if( func_num == 2 )
      rc = value1 * value2; // multiply values
   else if( func_num == 3 )
      rc = value1 / value2; // divide values
   else if( func_num == 4 )
      rc = value1 + value2; // sum values
   else if( func_num == 31 ) // send amount to address
   {
      if( value1 > g_balance )
         value1 = g_balance;

      cout << "payout " << dec << value1 << " to account: " << hex << setw( 8 ) << setfill( '0' ) << value2 << '\n';
      g_balance -= value1;
   }
   else if( func_num == 0x0114 ) // Set_A1_A2
   {
      state.a1 = value1;
      state.a2 = value2;
   }
   else if( func_num == 0x0115 ) // Set_A3_A4
   {
      state.a3 = value1;
      state.a4 = value2;
   }
   else if( func_num == 0x011a ) // Set_B1_B2
   {
      state.b1 = value1;
      state.b2 = value2;
   }
   else if( func_num == 0x011b ) // Set_B3_B4
   {
      state.b3 = value1;
      state.b4 = value2;
   }
   else if( g_function_data.count( func_num ) )
      rc = get_function_data( func_num );

   if( func_num != 31 )
   {
      if( func_num < 0x100 )
         cout << "func2: " << dec << func_num << " with " << value1
          << " and " << value2 << " rc: " << hex << setw( 16 ) << setfill( '0' ) << rc << '\n';
      else
         cout << "func2: " << decode_function_name( func_num, 0 ) << " with " << value1
          << " and " << value2 << " rc: 0x" << hex << setw( 16 ) << setfill( '0' ) << rc << '\n';
   }

   return rc;
}

int get_fun( int8_t* p_code, int32_t csize, const machine_state& state, int16_t& fun )
{
   if( state.pc + ( int32_t )sizeof( int16_t ) >= csize )
      return -1;
   else
   {
      fun = *( int16_t* )( p_code + state.pc + 1 );

      return 0;
   }
}

int get_addr( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int32_t& addr, bool is_code = false )
{
   if( state.pc + ( int32_t )sizeof( int32_t ) >= csize )
      return -1;
   else
   {
      addr = *( int32_t* )( p_code + state.pc + 1 );

      if( addr < 0 || addr > c_max_to_multiply || ( is_code && addr >= csize ) )
         return -1;
      else if( !is_code && ( ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize ) )
         return -1;
      else
         return 0;
   }
}

int get_addrs( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int32_t& addr1, int32_t& addr2 )
{
   if( state.pc + ( int32_t )sizeof( int32_t ) + ( int32_t )sizeof( int32_t ) >= csize )
      return -1;
   else
   {
      addr1 = *( int32_t* )( p_code + state.pc + 1 );
      addr2 = *( int32_t* )( p_code + state.pc + 1 + sizeof( int32_t ) );

      if( addr1 < 0 || addr1 > c_max_to_multiply
       || addr2 < 0 || addr2 > c_max_to_multiply
       || ( addr1 * 8 ) < 0 || ( addr2 * 8 ) < 0
       || ( addr1 * 8 ) + ( int32_t )sizeof( int64_t ) > dsize
       || ( addr2 * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
         return -1;
      else
         return 0;
   }
}

int get_addr_off( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int32_t& addr, int8_t& off )
{
   if( state.pc + ( int32_t )sizeof( int32_t ) + ( int32_t )sizeof( int8_t ) >= csize )
      return -1;
   else
   {
      addr = *( int32_t* )( p_code + state.pc + 1 );
      off = *( int8_t* )( p_code + state.pc + 1 + sizeof( int32_t ) );

      if( addr < 0 || addr > c_max_to_multiply || ( addr * 8 ) < 0
       || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize || state.pc + off >= csize )
         return -1;
      else
         return 0;
   }
}

int get_addrs_off( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int32_t& addr1, int32_t& addr2, int8_t& off )
{
   if( state.pc + ( int32_t )sizeof( int32_t ) + ( int32_t )sizeof( int32_t ) + ( int32_t )sizeof( int8_t ) >= csize )
      return -1;
   else
   {
      addr1 = *( int32_t* )( p_code + state.pc + 1 );
      addr2 = *( int32_t* )( p_code + state.pc + 1 + sizeof( int32_t ) );
      off = *( int8_t* )( p_code + state.pc + 1 + sizeof( int32_t ) + sizeof( int32_t ) );

      if( addr1 < 0 || addr1 < c_max_to_multiply
       || addr2 < 0 || addr2 < c_max_to_multiply
       || ( addr1 * 8 ) < 0 || ( addr2 * 8 ) < 0
       || ( addr1 * 8 ) + ( int32_t )sizeof( int64_t ) > dsize
       || ( addr2 * 8 ) + ( int32_t )sizeof( int64_t ) > dsize || state.pc + off >= csize )
         return -1;
      else
         return 0;
   }
}

int get_fun_addr( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int16_t& fun, int32_t& addr )
{
   if( state.pc + ( int32_t )sizeof( int16_t ) + ( int32_t )sizeof( int32_t ) >= csize )
      return -1;
   else
   {
      fun = *( int16_t* )( p_code + state.pc + 1 );
      addr = *( int32_t* )( p_code + state.pc + 1 + sizeof( int16_t ) );

      if( addr < 0 || addr > c_max_to_multiply
       || ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
         return -1;
      else
         return 0;
   }
}

int get_fun_addrs( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int16_t& fun, int32_t& addr1, int32_t& addr2 )
{
   if( state.pc + ( int32_t )sizeof( int16_t )
    + ( int32_t )sizeof( int32_t ) + ( int32_t )sizeof( int32_t ) >= csize )
      return -1;
   else
   {
      fun = *( int16_t* )( p_code + state.pc + 1 );
      addr1 = *( int32_t* )( p_code + state.pc + 1 + sizeof( int16_t ) );
      addr2 = *( int32_t* )( p_code + state.pc + 1 + sizeof( int16_t ) + sizeof( int32_t ) );

      if( addr1 < 0 || addr1 > c_max_to_multiply
       || addr2 < 0 || addr2 > c_max_to_multiply
       || ( addr1 * 8 ) < 0 || ( addr2 * 8 ) < 0
       || ( addr1 * 8 ) + ( int32_t )sizeof( int64_t ) > dsize
       || ( addr2 * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
         return -1;
      else
         return 0;
   }
}

int get_addr_val( int8_t* p_code,
 int32_t csize, int32_t dsize, const machine_state& state, int32_t& addr, int64_t& val )
{
   if( state.pc + ( int32_t )sizeof( int32_t ) + ( int32_t )sizeof( int64_t ) >= csize )
      return -1;
   else
   {
      addr = *( int32_t* )( p_code + state.pc + 1 );
      val = *( int64_t* )( p_code + state.pc + 1 + ( int32_t )sizeof( int32_t ) );

      if( addr < 0 || addr > c_max_to_multiply
       || ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
         return -1;
      else
         return 0;
   }
}

int process_op( int8_t* p_code, int32_t csize, int8_t* p_data, int32_t dsize,
 int32_t cssize, int32_t ussize, bool disassemble, bool determine_jumps, machine_state& state )
{
   int rc = 0;

   bool invalid = false;
   bool had_overflow = false;

   if( csize < 1 || state.pc >= csize )
      return 0;

   if( determine_jumps )
      state.jumps.insert( state.pc );

   int8_t op = p_code[ state.pc ];

   if( op && disassemble && !determine_jumps )
   {
      cout << hex << setw( 8 ) << setfill( '0' ) << state.pc;
      if( state.pc == state.opc )
         cout << "* ";
      else
         cout << "  ";
   }

   if( op == e_op_code_NOP )
   {
      if( disassemble )
      {
         if( !determine_jumps )
            cout << "NOP\n";
         while( true )
         {
            ++rc;
            if( state.pc + rc >= csize || p_code[ state.pc + rc ] != e_op_code_NOP )
               break;
         }
      }
      else while( true )
      {
         ++rc;
         ++state.pc;
         if( state.pc >= csize || p_code[ state.pc ] != e_op_code_NOP )
            break;
      }
   }
   else if( op == e_op_code_SET_VAL )
   {
      int32_t addr;
      int64_t val;
      rc = get_addr_val( p_code, csize, dsize, state, addr, val );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int64_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "SET @" << hex << setw( 8 ) << setfill( '0' )
                << addr << " #" << setw( 16 ) << setfill( '0' ) << val << '\n';
         }
         else
         {
            state.pc += rc;
            *( int64_t* )( p_data + ( addr * 8 ) ) = val;
         }
      }
   }
   else if( op == e_op_code_SET_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "SET @" << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 8 ) << setfill( '0' ) << addr2 << '\n';
         }
         else
         {
            state.pc += rc;
            *( int64_t* )( p_data + ( addr1 * 8 ) ) = *( int64_t* )( p_data + ( addr2 * 8 ) );
         }
      }
   }
   else if( op == e_op_code_CLR_DAT )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "CLR @" << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
         }
         else
         {
            state.pc += rc;
            *( int64_t* )( p_data + ( addr * 8 ) ) = 0;
         }
      }
   }
   else if( op == e_op_code_INC_DAT || op == e_op_code_DEC_DAT || op == e_op_code_NOT_DAT )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( op == e_op_code_INC_DAT )
                  cout << "INC @";
               else if( op == e_op_code_DEC_DAT )
                  cout << "DEC @";
               else
                  cout << "NOT @";

               cout << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
            }
         }
         else
         {
            state.pc += rc;

            if( op == e_op_code_INC_DAT )
               ++*( int64_t* )( p_data + ( addr * 8 ) );
            else if( op == e_op_code_DEC_DAT )
               --*( int64_t* )( p_data + ( addr * 8 ) );
            else
               *( int64_t* )( p_data + ( addr * 8 ) ) = ~*( int64_t* )( p_data + ( addr * 8 ) );
         }
      }
   }
   else if( op == e_op_code_ADD_DAT || op == e_op_code_SUB_DAT
    || op == e_op_code_MUL_DAT || op == e_op_code_DIV_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( op == e_op_code_ADD_DAT )
                  cout << "ADD @";
               else if( op == e_op_code_SUB_DAT )
                  cout << "SUB @";
               else if( op == e_op_code_MUL_DAT )
                  cout << "MUL @";
               else
                  cout << "DIV @";

               cout << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 8 ) << setfill( '0' ) << addr2 << '\n';
            }
         }
         else
         {
            int64_t val = *( int64_t* )( p_data + ( addr2 * 8 ) );

            if( op == e_op_code_DIV_DAT && val == 0 )
               rc = -2;
            else
            {
               state.pc += rc;

               if( op == e_op_code_ADD_DAT )
                  *( int64_t* )( p_data + ( addr1 * 8 ) ) += *( int64_t* )( p_data + ( addr2 * 8 ) );
               else if( op == e_op_code_SUB_DAT )
                  *( int64_t* )( p_data + ( addr1 * 8 ) ) -= *( int64_t* )( p_data + ( addr2 * 8 ) );
               else if( op == e_op_code_MUL_DAT )
                  *( int64_t* )( p_data + ( addr1 * 8 ) ) *= *( int64_t* )( p_data + ( addr2 * 8 ) );
               else
                  *( int64_t* )( p_data + ( addr1 * 8 ) ) /= *( int64_t* )( p_data + ( addr2 * 8 ) );
            }
         }
      }
   }
   else if( op == e_op_code_BOR_DAT
    || op == e_op_code_AND_DAT || op == e_op_code_XOR_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( op == e_op_code_BOR_DAT )
                  cout << "BOR @";
               else if( op == e_op_code_AND_DAT )
                  cout << "AND @";
               else
                  cout << "XOR @";

               cout << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 8 ) << setfill( '0' ) << addr2 << '\n';
            }
         }
         else
         {
            state.pc += rc;
            int64_t val = *( int64_t* )( p_data + ( addr2 * 8 ) );

            if( op == e_op_code_BOR_DAT )
               *( int64_t* )( p_data + ( addr1 * 8 ) ) |= val;
            else if( op == e_op_code_AND_DAT )
               *( int64_t* )( p_data + ( addr1 * 8 ) ) &= val;
            else
               *( int64_t* )( p_data + ( addr1 * 8 ) ) ^= val;
         }
      }
   }
   else if( op == e_op_code_SET_IND )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "SET @" << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $($" << setw( 8 ) << setfill( '0' ) << addr2 << ")\n";
         }
         else
         {
            int64_t addr = *( int64_t* )( p_data + ( addr2 * 8 ) );

            if( addr < 0 || addr > c_max_to_multiply
             || ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
               rc = -1;
            else
            {
               state.pc += rc;
              *( int64_t* )( p_data + ( addr1 * 8 ) ) = *( int64_t* )( p_data + ( addr * 8 ) );
            }
         }
      }
   }
   else if( op == e_op_code_SET_IDX )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      int32_t size = sizeof( int32_t ) + sizeof( int32_t );

      if( rc == 0 || disassemble )
      {
         int32_t addr3;
         rc = get_addr( p_code + size, csize, dsize, state, addr3 );

         if( rc == 0 || disassemble )
         {
            rc = 1 + size + sizeof( int32_t );

            if( disassemble )
            {
               if( !determine_jumps )
                  cout << "SET @" << hex << setw( 8 ) << setfill( '0' )
                   << addr1 << " $($" << setw( 8 ) << setfill( '0' ) << addr2
                   << "+$" << setw( 8 ) << setfill( '0' ) << addr3 << ")\n";
            }
            else
            {
               int64_t base = *( int64_t* )( p_data + ( addr2 * 8 ) );
               int64_t offs = *( int64_t* )( p_data + ( addr3 * 8 ) );

               int64_t addr = base + offs;

               if( addr < 0 || addr > c_max_to_multiply
                || ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
                  rc = -1;
               else
               {
                  state.pc += rc;
                 *( int64_t* )( p_data + ( addr1 * 8 ) ) = *( int64_t* )( p_data + ( addr * 8 ) );
               }
            }
         }
      }
   }
   else if( op == e_op_code_PSH_DAT || op == e_op_code_POP_DAT )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( op == e_op_code_PSH_DAT )
                  cout << "PSH $";
               else
                  cout << "POP @";

               cout << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
            }
         }
         else if( ( op == e_op_code_PSH_DAT && state.us == ( ussize / 8 ) )
          || ( op == e_op_code_POP_DAT && state.us == 0 ) )
            rc = -1;
         else
         {
            state.pc += rc;
            if( op == e_op_code_PSH_DAT )
               *( int64_t* )( p_data + dsize + cssize + ussize
                - ( ++state.us * 8 ) ) = *( int64_t* )( p_data + ( addr * 8 ) );
            else
               *( int64_t* )( p_data + ( addr * 8 ) )
                = *( int64_t* )( p_data + dsize + cssize + ussize - ( state.us-- * 8 ) );
         }
      }
   }
   else if( op == e_op_code_JMP_SUB )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr, true );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "JSR :" << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
         }
         else
         {
            if( state.cs == ( cssize / 8 ) )
               rc = -1;
            else if( state.jumps.count( addr ) )
            {
               *( int64_t* )( p_data + dsize + cssize - ( ++state.cs * 8 ) ) = state.pc + rc;
               state.pc = addr;
            }
            else
               rc = -2;
         }
      }
   }
   else if( op == e_op_code_RET_SUB )
   {
      rc = 1;

      if( disassemble )
      {
         if( !determine_jumps )
            cout << "RET\n";
      }
      else
      {
         if( state.cs == 0 )
            rc = -1;
         else
         {
            int64_t val = *( int64_t* )( p_data + dsize + cssize - ( state.cs-- * 8 ) );
            int32_t addr = ( int32_t )val;
            if( state.jumps.count( addr ) )
               state.pc = addr;
            else
               rc = -2;
         }
      }
   }
   else if( op == e_op_code_IND_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "SET @($" << hex << setw( 8 ) << setfill( '0' )
                << addr1 << ") $" << setw( 8 ) << setfill( '0' ) << addr2 << "\n";
         }
         else
         {
            int64_t addr = *( int64_t* )( p_data + ( addr1 * 8 ) );

            if( addr < 0 || addr > c_max_to_multiply
             || ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
               rc = -1;
            else
            {
               state.pc += rc;
              *( int64_t* )( p_data + ( addr * 8 ) ) = *( int64_t* )( p_data + ( addr2 * 8 ) );
            }
         }
      }
   }
   else if( op == e_op_code_IDX_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      int32_t size = sizeof( int32_t ) + sizeof( int32_t );

      if( rc == 0 || disassemble )
      {
         int32_t addr3;
         rc = get_addr( p_code + size, csize, dsize, state, addr3 );

         if( rc == 0 || disassemble )
         {
            rc = 1 + size + sizeof( int32_t );

            if( disassemble )
            {
               if( !determine_jumps )
                  cout << "SET @($" << hex << setw( 8 ) << setfill( '0' )
                   << addr1 << "+$" << setw( 8 ) << setfill( '0' ) << addr2
                   << ") $" << setw( 8 ) << setfill( '0' ) << addr3 << "\n";
            }
            else
            {
               int64_t base = *( int64_t* )( p_data + ( addr1 * 8 ) );
               int64_t offs = *( int64_t* )( p_data + ( addr2 * 8 ) );

               int64_t addr = base + offs;

               if( addr < 0 || addr > c_max_to_multiply
                || ( addr * 8 ) < 0 || ( addr * 8 ) + ( int32_t )sizeof( int64_t ) > dsize )
                  rc = -1;
               else
               {
                  state.pc += rc;
                 *( int64_t* )( p_data + ( addr * 8 ) ) = *( int64_t* )( p_data + ( addr3 * 8 ) );
               }
            }
         }
      }
   }
   else if( op == e_op_code_MOD_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               cout << "MOD @" << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 16 ) << setfill( '0' ) << addr2 << '\n';
            }
         }
         else
         {
            state.pc += rc;
            int64_t val = *( int64_t* )( p_data + ( addr2 * 8 ) );

            *( int64_t* )( p_data + ( addr1 * 8 ) ) %= val;
         }
      }
   }
   else if( op == e_op_code_SHL_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               cout << "SHL @" << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 16 ) << setfill( '0' ) << addr2 << '\n';
            }
         }
         else
         {
            state.pc += rc;
            int64_t val = *( int64_t* )( p_data + ( addr2 * 8 ) );

            *( int64_t* )( p_data + ( addr1 * 8 ) ) <<= val;
         }
      }
   }
   else if( op == e_op_code_SHR_DAT )
   {
      int32_t addr1, addr2;
      rc = get_addrs( p_code, csize, dsize, state, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               cout << "SHR @" << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 16 ) << setfill( '0' ) << addr2 << '\n';
            }
         }
         else
         {
            state.pc += rc;
            int64_t val = *( int64_t* )( p_data + ( addr2 * 8 ) );

            *( int64_t* )( p_data + ( addr1 * 8 ) ) >>= val;
         }
      }
   }
   else if( op == e_op_code_JMP_ADR )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr, true );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "JMP :" << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
         }
         else if( state.jumps.count( addr ) )
            state.pc = addr;
         else
            rc = -2;
      }
   }
   else if( op == e_op_code_BZR_DAT || op == e_op_code_BNZ_DAT )
   {
      int8_t off;
      int32_t addr;
      rc = get_addr_off( p_code, csize, dsize, state, addr, off );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int8_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( op == e_op_code_BZR_DAT )
                  cout << "BZR $";
               else
                  cout << "BNZ $";

               cout << hex << setw( 8 ) << setfill( '0' )
                << addr << " :" << setw( 8 ) << setfill( '0' ) << ( state.pc + off ) << '\n';
            }
         }
         else
         {
            int64_t val = *( int64_t* )( p_data + ( addr * 8 ) );

            if( ( op == e_op_code_BZR_DAT && val == 0 )
             || ( op == e_op_code_BNZ_DAT && val != 0 ) )
            {
               if( state.jumps.count( state.pc + off ) )
                  state.pc += off;
               else
                  rc = -2;
            }
            else
               state.pc += rc;
         }
      }
   }
   else if( op == e_op_code_BGT_DAT || op == e_op_code_BLT_DAT
    || op == e_op_code_BGE_DAT || op == e_op_code_BLE_DAT
    || op == e_op_code_BEQ_DAT || op == e_op_code_BNE_DAT )
   {
      int8_t off;
      int32_t addr1, addr2;
      rc = get_addrs_off( p_code, csize, dsize, state, addr1, addr2, off );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t ) + sizeof( int32_t ) + sizeof( int8_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( op == e_op_code_BGT_DAT )
                  cout << "BGT $";
               else if( op == e_op_code_BLT_DAT )
                  cout << "BLT $";
               else if( op == e_op_code_BGE_DAT )
                  cout << "BGE $";
               else if( op == e_op_code_BLE_DAT )
                  cout << "BLE $";
               else if( op == e_op_code_BEQ_DAT )
                  cout << "BEQ $";
               else
                  cout << "BNE $";

               cout << hex << setw( 8 ) << setfill( '0' )
                << addr1 << " $" << setw( 8 ) << setfill( '0' )
                << addr2 << " :" << setw( 8 ) << setfill( '0' ) << ( state.pc + off ) << '\n';
            }
         }
         else
         {
            int64_t val1 = *( int64_t* )( p_data + ( addr1 * 8 ) );
            int64_t val2 = *( int64_t* )( p_data + ( addr2 * 8 ) );

            if( ( op == e_op_code_BGT_DAT && val1 > val2 )
             || ( op == e_op_code_BLT_DAT && val1 < val2 )
             || ( op == e_op_code_BGE_DAT && val1 >= val2 )
             || ( op == e_op_code_BLE_DAT && val1 <= val2 )
             || ( op == e_op_code_BEQ_DAT && val1 == val2 )
             || ( op == e_op_code_BNE_DAT && val1 != val2 ) )
            {
               if( state.jumps.count( state.pc + off ) )
                  state.pc += off;
               else
                  rc = -2;
            }
            else
               state.pc += rc;
         }
      }
   }
   else if( op == e_op_code_SLP_DAT )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr, true );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "SLP @" << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
         }
         else
         {
            // NOTE: The "sleep_until" state value would be set to the current block + $addr.

            state.pc += rc;
         }
      }
   }
   else if( op == e_op_code_FIZ_DAT || op == e_op_code_STZ_DAT )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr );

      if( rc == 0 || disassemble )
      {
         if( disassemble )
         {
            rc = 1 + sizeof( int32_t );

            if( !determine_jumps )
            {
               if( op == e_op_code_FIZ_DAT )
                  cout << "FIZ $";
               else
                  cout << "STZ $";

               cout << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
            }
         }
         else
         {
            if( *( int64_t* )( p_data + ( addr * 8 ) ) == 0 )
            {
               if( op == e_op_code_STZ_DAT )
               {
                  state.pc += rc;
                  state.stopped = true;
               }   
               else
               {
                  state.pc = state.pcs;
                  state.finished = true;
               }
            }
            else
            {
               rc = 1 + sizeof( int32_t );
               state.pc += rc;
            }
         }
      }
   }
   else if( op == e_op_code_FIN_IMD || op == e_op_code_STP_IMD )
   {
      if( disassemble )
      {
         rc = 1;

         if( !determine_jumps )
         {
            if( op == e_op_code_FIN_IMD )
               cout << "FIN\n";
            else
               cout << "STP\n";
         }
      }
      else if( op == e_op_code_STP_IMD )
      {
         state.pc += rc;
         state.stopped = true;
      }
      else
      {
         state.pc = state.pcs;
         state.finished = true;
      }
   }
   else if( op == e_op_code_SLP_IMD )
   {
      if( rc == 0 || disassemble )
      {
         rc = 1;

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "SLP\n";
         }
         else
         {
            // NOTE: The "sleep_until" state value would be set to the current block + 1.

            state.pc += rc;
         }
      }
   }
   else if( op == e_op_code_ERR_ADR )
   {
      int32_t addr;
      rc = get_addr( p_code, csize, dsize, state, addr, true );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
               cout << "ERR :" << hex << setw( 8 ) << setfill( '0' ) << addr << '\n';
         }
         else if( state.jumps.count( addr ) )
            state.pce = addr;
         else
            rc = -3;
      }
   }
   else if( op == e_op_code_SET_PCS )
   {
      rc = 1;

      if( disassemble )
      {
         if( !determine_jumps )
            cout << "PCS\n";
      }
      else
      {
         state.pc += rc;
         state.pcs = state.pc;
      }
   }
   else if( op == e_op_code_EXT_FUN )
   {
      int16_t fun;
      rc = get_fun( p_code, csize, state, fun );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int16_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( fun < 0x100 )
                  cout << "FUN " << dec << fun << "\n";
               else
                  cout << "FUN " << decode_function_name( fun, op ) << "\n";
            }
         }
         else
         {
            state.pc += rc;
            func( fun, state );
         }
      }
   }
   else if( op == e_op_code_EXT_FUN_DAT )
   {
      int16_t fun;
      int32_t addr;
      rc = get_fun_addr( p_code, csize, dsize, state, fun, addr );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int16_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( fun < 0x100 )
                  cout << "FUN " << dec << fun << " $" << hex << setw( 8 ) << setfill( '0' ) << addr << "\n";
               else
                  cout << "FUN " << decode_function_name( fun, op )
                   << " $" << hex << setw( 8 ) << setfill( '0' ) << addr << "\n";
            }
         }
         else
         {
            state.pc += rc;
            int64_t val = *( int64_t* )( p_data + ( addr * 8 ) );

            func1( fun, state, val, p_data, dsize );
         }
      }
   }
   else if( op == e_op_code_EXT_FUN_DAT_2 )
   {
      int16_t fun;
      int32_t addr1, addr2;
      rc = get_fun_addrs( p_code, csize, dsize, state, fun, addr1, addr2 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int16_t ) + sizeof( int32_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( fun < 0x100 )
                  cout << "FUN " << dec << fun << " $" << hex << setw( 8 )
                   << setfill( '0' ) << addr1 << " $" << setw( 8 ) << setfill( '0' ) << addr2 << "\n";
               else
                  cout << "FUN " << decode_function_name( fun, op ) << " $" << hex << setw( 8 )
                   << setfill( '0' ) << addr1 << " $" << setw( 8 ) << setfill( '0' ) << addr2 << "\n";
            }
         }
         else
         {
            state.pc += rc;
            int64_t val1 = *( int64_t* )( p_data + ( addr1 * 8 ) );
            int64_t val2 = *( int64_t* )( p_data + ( addr2 * 8 ) );

            func2( fun, state, val1, val2, p_data, dsize );
         }
      }
   }
   else if( op == e_op_code_EXT_FUN_RET )
   {
      int16_t fun;
      int32_t addr;
      rc = get_fun_addr( p_code, csize, dsize, state, fun, addr );

      if( rc == 0 || disassemble )
      {
         rc = 1 + sizeof( int16_t ) + sizeof( int32_t );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( fun < 0x100 )
                  cout << "FUN @" << hex << setw( 8 ) << setfill( '0' ) << addr << ' ' << dec << fun << '\n';
               else
                  cout << "FUN @" << hex << setw( 8 ) << setfill( '0' ) << addr
                   << " " << decode_function_name( fun, op ) << '\n';
            }
         }
         else
         {
            state.pc += rc;
            *( int64_t* )( p_data + ( addr * 8 ) ) = func( fun, state );
         }
      }
   }
   else if( op == e_op_code_EXT_FUN_RET_DAT || op == e_op_code_EXT_FUN_RET_DAT_2 )
   {
      int16_t fun;
      int32_t addr1, addr2;
      rc = get_fun_addrs( p_code, csize, dsize, state, fun, addr1, addr2 );

      int32_t size = sizeof( int16_t ) + sizeof( int32_t ) + sizeof( int32_t );

      int32_t addr3;
      if( ( rc == 0 || disassemble ) && op == e_op_code_EXT_FUN_RET_DAT_2 )
         rc = get_addr( p_code + size, csize, dsize, state, addr3 );

      if( rc == 0 || disassemble )
      {
         rc = 1 + size + ( op == e_op_code_EXT_FUN_RET_DAT_2 ? sizeof( int32_t ) : 0 );

         if( disassemble )
         {
            if( !determine_jumps )
            {
               if( fun < 0x100 )
                  cout << "FUN @" << hex << setw( 8 ) << setfill( '0' ) << addr1
                   << ' ' << dec << fun << " $" << setw( 8 ) << setfill( '0' ) << addr2;
               else
                  cout << "FUN @" << hex << setw( 8 ) << setfill( '0' ) << addr1 << " "
                   << decode_function_name( fun, op ) << " $" << setw( 8 ) << setfill( '0' ) << addr2;

               if( op == e_op_code_EXT_FUN_RET_DAT_2 )
                  cout << " $" << setw( 8 ) << setfill( '0' ) << addr3;

               cout << "\n";
            }
         }
         else
         {
            state.pc += rc;
            int64_t val = *( int64_t* )( p_data + ( addr2 * 8 ) );

            if( op != e_op_code_EXT_FUN_RET_DAT_2 )
               *( int64_t* )( p_data + ( addr1 * 8 ) ) = func1( fun, state, val, p_data, dsize );
            else
            {
               int64_t val2 = *( int64_t* )( p_data + ( addr3 * 8 ) );
               *( int64_t* )( p_data + ( addr1 * 8 ) ) = func2( fun, state, val, val2, p_data, dsize );
            }
         }
      }
   }
   else
   {
      if( !disassemble )
         rc = -2;
   }

   if( rc == -1 && state.pce )
   {
      rc = 0;
      state.pc = state.pce;
   }

   if( rc == -1 && disassemble && !determine_jumps )
      cout << "\n(overflow)\n";

   if( rc == -2 && disassemble && !determine_jumps )
      cout << "\n(invalid op)\n";

   if( rc >= 0 )
      ++state.steps;

   return rc;
}

void dump_state( const machine_state& state )
{
   cout << "pc: " << hex << setw( 8 ) << setfill( '0' ) << state.pc << '\n';

   cout << "cs: " << dec << state.cs << '\n';
   cout << "us: " << dec << state.us << '\n';

   cout << "pce: " << hex << setw( 8 ) << setfill( '0' ) << state.pcs << '\n';
   cout << "pcs: " << hex << setw( 8 ) << setfill( '0' ) << state.pcs << '\n';

   cout << "steps: " << dec << state.steps << '\n';

   cout << "a1: " << hex << setw( 16 ) << setfill( '0' ) << state.a1 << '\n';
   cout << "a2: " << hex << setw( 16 ) << setfill( '0' ) << state.a2 << '\n';
   cout << "a3: " << hex << setw( 16 ) << setfill( '0' ) << state.a3 << '\n';
   cout << "a4: " << hex << setw( 16 ) << setfill( '0' ) << state.a4 << '\n';

   cout << "b1: " << hex << setw( 16 ) << setfill( '0' ) << state.b1 << '\n';
   cout << "b2: " << hex << setw( 16 ) << setfill( '0' ) << state.b2 << '\n';
   cout << "b3: " << hex << setw( 16 ) << setfill( '0' ) << state.b3 << '\n';
   cout << "b4: " << hex << setw( 16 ) << setfill( '0' ) << state.b4 << '\n';
}

void dump_bytes( int8_t* p_bytes, int num )
{
   for( int i = 0; i < num; i += 16 )
   {
      cout << hex << setw( 8 ) << setfill( '0' ) << i << ' ';

      for( int j = 0; j < 16; j++ )
      {
         int val = ( unsigned char )p_bytes[ i + j ];

         cout << ' ' << hex << setw( 2 ) << setfill( '0' ) << val;
      }

      cout << '\n';
   }
}

void list_code( machine_state& state, int8_t* p_code, int32_t csize,
 int8_t* p_data, int32_t dsize, int32_t cssize, int32_t ussize, bool determine_jumps = false )
{
   int32_t opc = state.pc;
   int32_t osteps = state.steps;

   state.pc = 0;
   state.opc = opc;

   while( true )
   {
      int rc = process_op( p_code, csize, p_data, dsize, cssize, ussize, true, determine_jumps, state );

      if( rc <= 0 )
         break;

      state.pc += rc;
   }

   state.steps = osteps;
   state.pc = opc;
}

bool check_has_balance( )
{
   if( g_balance == 0 )
   {
      cout << "(stopped - zero balance)\n";
      return false;
   }
   else
      return true;
}

void reset_machine( machine_state& state,
 int8_t* p_code, int32_t csize, int8_t* p_data, int32_t dsize, int32_t cssize, int32_t ussize )
{
   state.reset( );
   list_code( state, p_code, csize, p_data, dsize, cssize, ussize, true );

   memset( p_data, 0, dsize + cssize + ussize );

   g_first_call = true;

   for( map< int32_t, function_data >::iterator i = g_function_data.begin( ); i != g_function_data.end( ); ++i )
      i->second.offset = 0;
}

int main( )
{
   auto_ptr< int8_t > ap_code( new int8_t[ g_code_pages * c_code_page_bytes ] );
   auto_ptr< int8_t > ap_data( new int8_t[ g_data_pages * c_data_page_bytes
    + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes ] );

   memset( ap_code.get( ), 0, g_code_pages * c_code_page_bytes );
   memset( ap_data.get( ), 0, g_data_pages * c_data_page_bytes
    + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes );

   machine_state state;
   set< int32_t > break_points;

   string cmd, next;
   while( cout << "\n> ", getline( cin, next ) )
   {
      if( next.empty( ) )
         continue;

      string::size_type pos = next.find( ' ' );
      if( pos == string::npos )
      {
         cmd = next;
         next.erase( );
      }
      else
      {
         cmd = next.substr( 0, pos );
         next.erase( 0, pos + 1 );
      }

      string arg_1, arg_2, arg_3;

      pos = next.find( ' ' );
      arg_1 = next.substr( 0, pos );
      if( pos != string::npos )
      {
         arg_2 = next.substr( pos + 1 );

         pos = arg_2.find( ' ' );
         if( pos != string::npos )
         {
            arg_3 = arg_2.substr( pos + 1 );
            arg_2.erase( pos );
         }
      }

      if( cmd == "?" || cmd == "help" )
      {
         cout << "commands:\n";
         cout << "---------\n";
         cout << "code <hex byte values> [<[0x]offset>]\n";
         cout << "data <hex byte values> [<[0x]offset>]\n";
         cout << "run\n";
         cout << "cont\n";
         cout << "dump {code|data|stacks}\n";
         cout << "list\n";
         cout << "load <file>\n";
         cout << "save <file>\n";
         cout << "size [{code|data|call|user} [<pages>]]\n";
         cout << "step [<num_steps>]\n";
         cout << "break <[0x]value>\n";
         cout << "reset\n";
         cout << "state\n";
         cout << "balance [<amount>]\n";
         cout << "function <[+]#> [<[0x]value1[,[0x]value2[,...]]>] [loop]\n";
         cout << "functions\n";
         cout << "help\n";
         cout << "exit" << endl;
      }
      else if( ( cmd == "code" || cmd == "data" ) && arg_1.size( ) && ( arg_1.size( ) % 2 == 0 ) )
      {
         int32_t offset = 0;
         if( !arg_2.empty( ) )
         {
            if( arg_2.size( ) > 2 && arg_2.find( "0x" ) == 0 )
            {
               istringstream isstr( arg_2.substr( 2 ) );
               isstr >> hex >> offset;
            }
            else
               offset = atoi( arg_2.c_str( ) );
         }

         int8_t* p_c = cmd == "code" ? ap_code.get( ) : ap_data.get( ) + offset;

         if( arg_2.empty( ) )
         {
            if( cmd == "code" )
               memset( p_c, 0, g_code_pages * c_code_page_bytes );
            else
               memset( p_c, 0, g_data_pages * c_data_page_bytes );
         }

         for( size_t i = 0; i < arg_1.size( ); i += 2 )
         {
            unsigned int value;
            istringstream isstr( arg_1.substr( i, 2 ) );
            isstr >> hex >> value;

            *( p_c + ( i / 2 ) ) = ( int8_t )value;
         }

         if( cmd == "code" )
            reset_machine( state,
             ap_code.get( ), g_code_pages * c_code_page_bytes,
             ap_data.get( ), g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes, g_user_stack_pages * c_user_stack_page_bytes );
      }
      else if( cmd == "run" || cmd == "cont" )
      {
         if( cmd == "run" )
            reset_machine( state, ap_code.get( ),
             g_code_pages * c_code_page_bytes, ap_data.get( ), g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes, g_user_stack_pages * c_user_stack_page_bytes );

         while( true )
         {
            if( !check_has_balance( ) )
               break;

            int rc = process_op(
             ap_code.get( ), g_code_pages * c_code_page_bytes,
             ap_data.get( ), g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes,
             g_user_stack_pages * c_user_stack_page_bytes, false, false, state );

            if( !check_has_balance( ) )
               break;

            --g_balance;

            if( rc >= 0 )
            {
               if( state.stopped )
               {
                  cout << "(stopped)\n";
                  cout << "total steps: " << dec << state.steps << '\n';

                  state.stopped = false;
                  break;
               }
               else if( state.finished )
               {
                  cout << "(finished)\n";
                  cout << "total steps: " << dec << state.steps << '\n';

                  break;
               }

               if( break_points.count( state.pc ) )
               {
                  cout << "(break point)\n";
                  break;
               }
            }
            else
            {
               if( rc == -1 )
                  cout << "error: overflow\n";
               else if( rc == -2 )
                  cout << "error: invalid code\n";
               else
                  cout << "unexpected error\n";

               break;
            }
         }
      }
      else if( cmd == "dump" && ( next == "code" || next == "data" || next == "stacks" ) )
      {
         if( next == "code" )
            dump_bytes( ap_code.get( ), g_code_pages * c_code_page_bytes );
         else if( next == "data" )
            dump_bytes( ap_data.get( ), g_data_pages * c_data_page_bytes );
         else
            dump_bytes( ap_data.get( ) + g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes );
      }
      else if( cmd == "list" )
         list_code( state,
          ap_code.get( ), g_code_pages * c_code_page_bytes,
          ap_data.get( ), g_data_pages * c_data_page_bytes,
          g_call_stack_pages * c_call_stack_page_bytes, g_user_stack_pages * c_user_stack_page_bytes );
      else if( cmd == "load" && !arg_1.empty( ) )
      {
         ifstream inpf( arg_1.c_str( ), ios::in | ios::binary );

         if( !inpf )
            cout << "error: unable to open '" << arg_1 << "' for input" << endl;
         else
         {
            inpf.read( ( char* )&g_val, sizeof( g_val ) );
            inpf.read( ( char* )&g_val1, sizeof( g_val1 ) );
            inpf.read( ( char* )&g_balance, sizeof( g_balance ) );
            inpf.read( ( char* )&g_first_call, sizeof( g_first_call ) );
            inpf.read( ( char* )&g_increment_func, sizeof( g_increment_func ) );

            inpf.read( ( char* )&state.pc, sizeof( state.pc ) );
            inpf.read( ( char* )&state.cs, sizeof( state.cs ) );
            inpf.read( ( char* )&state.us, sizeof( state.us ) );
            inpf.read( ( char* )&state.pce, sizeof( state.pce ) );
            inpf.read( ( char* )&state.pcs, sizeof( state.pcs ) );
            inpf.read( ( char* )&state.steps, sizeof( state.steps ) );

            inpf.read( ( char* )&state.a1, sizeof( state.a1 ) );
            inpf.read( ( char* )&state.a2, sizeof( state.a2 ) );
            inpf.read( ( char* )&state.a3, sizeof( state.a3 ) );
            inpf.read( ( char* )&state.a4, sizeof( state.a4 ) );

            inpf.read( ( char* )&state.b1, sizeof( state.b1 ) );
            inpf.read( ( char* )&state.b2, sizeof( state.b2 ) );
            inpf.read( ( char* )&state.b3, sizeof( state.b3 ) );
            inpf.read( ( char* )&state.b4, sizeof( state.b4 ) );

            inpf.read( ( char* )&g_code_pages, sizeof( g_code_pages ) );
            ap_code.reset( new int8_t[ g_code_pages * c_code_page_bytes ] );

            inpf.read( ( char* )ap_code.get( ), g_code_pages * c_code_page_bytes );

            inpf.read( ( char* )&g_data_pages, sizeof( g_data_pages ) );
            inpf.read( ( char* )&g_call_stack_pages, sizeof( g_call_stack_pages ) );
            inpf.read( ( char* )&g_user_stack_pages, sizeof( g_user_stack_pages ) );

            ap_data.reset( new int8_t[ g_data_pages * c_data_page_bytes
            + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes ] );

            inpf.read( ( char* )ap_data.get( ), g_data_pages * c_data_page_bytes
             + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes );

            g_function_data.clear( );

            size_t size;
            inpf.read( ( char* )&size, sizeof( size_t ) );

            for( size_t i = 0; i < size; i++ )
            {
               int32_t func;
               inpf.read( ( char* )&func, sizeof( int32_t ) );

               bool loop;
               inpf.read( ( char* )&loop, sizeof( bool ) );

               size_t offset;
               inpf.read( ( char* )&offset, sizeof( size_t ) );

               g_function_data[ func ].loop = loop;
               g_function_data[ func ].offset = offset;

               size_t dsize;
               inpf.read( ( char* )&dsize, sizeof( size_t ) );

               for( size_t j = 0; j < dsize; j++ )
               {
                  int64_t next;
                  inpf.read( ( char* )&next, sizeof( int64_t ) );

                  g_function_data[ func ].data.push_back( next );
               }
            }

            inpf.close( );

            list_code( state,
             ap_code.get( ), g_code_pages * c_code_page_bytes,
             ap_data.get( ), g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes, g_user_stack_pages * c_user_stack_page_bytes, true );
         }
      }
      else if( cmd == "save" && !arg_1.empty( ) )
      {
         ofstream outf( arg_1.c_str( ), ios::out | ios::binary );

         if( !outf )
            cout << "error: unable to open '" << arg_1 << "' for output" << endl;
         else
         {
            outf.write( ( const char* )&g_val, sizeof( g_val ) );
            outf.write( ( const char* )&g_val1, sizeof( g_val1 ) );
            outf.write( ( const char* )&g_balance, sizeof( g_balance ) );
            outf.write( ( const char* )&g_first_call, sizeof( g_first_call ) );
            outf.write( ( const char* )&g_increment_func, sizeof( g_increment_func ) );

            outf.write( ( const char* )&state.pc, sizeof( state.pc ) );
            outf.write( ( const char* )&state.cs, sizeof( state.cs ) );
            outf.write( ( const char* )&state.us, sizeof( state.us ) );
            outf.write( ( const char* )&state.pce, sizeof( state.pce ) );
            outf.write( ( const char* )&state.pcs, sizeof( state.pcs ) );
            outf.write( ( const char* )&state.steps, sizeof( state.steps ) );

            outf.write( ( const char* )&state.a1, sizeof( state.a1 ) );
            outf.write( ( const char* )&state.a2, sizeof( state.a2 ) );
            outf.write( ( const char* )&state.a3, sizeof( state.a3 ) );
            outf.write( ( const char* )&state.a4, sizeof( state.a4 ) );

            outf.write( ( const char* )&state.b1, sizeof( state.b1 ) );
            outf.write( ( const char* )&state.b2, sizeof( state.b2 ) );
            outf.write( ( const char* )&state.b3, sizeof( state.b3 ) );
            outf.write( ( const char* )&state.b4, sizeof( state.b4 ) );

            outf.write( ( const char* )&g_code_pages, sizeof( g_code_pages ) );
            outf.write( ( const char* )ap_code.get( ), g_code_pages * c_code_page_bytes );

            outf.write( ( const char* )&g_data_pages, sizeof( g_data_pages ) );
            outf.write( ( const char* )&g_call_stack_pages, sizeof( g_call_stack_pages ) );
            outf.write( ( const char* )&g_user_stack_pages, sizeof( g_user_stack_pages ) );

            outf.write( ( const char* )ap_data.get( ), g_data_pages * c_data_page_bytes
             + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes );

            size_t size = g_function_data.size( );
            outf.write( ( const char* )&size, sizeof( size_t ) );

            for( map< int32_t, function_data >::iterator i = g_function_data.begin( ); i != g_function_data.end( ); ++i )
            {
               outf.write( ( const char* )&i->first, sizeof( int32_t ) );
               outf.write( ( const char* )&i->second.loop, sizeof( bool ) );
               outf.write( ( const char* )&i->second.offset, sizeof( size_t ) );

               size_t dsize = i->second.data.size( );
               outf.write( ( const char* )&dsize, sizeof( size_t ) );

               for( size_t j = 0; j < dsize; j++ )
                  outf.write( ( const char* )&i->second.data[ j ], sizeof( int64_t ) );
            }

            outf.close( );
         }
      }
      else if( cmd == "size" )
      {
         if( arg_1.empty( ) || arg_2.empty( ) )
         {
            if( arg_1.empty( ) || arg_1 == "code" )
               cout << "code (" << g_code_pages << " * "
                << c_code_page_bytes << ") = " << ( g_code_pages * c_code_page_bytes ) << " bytes\n";

            if( arg_1.empty( ) || arg_1 == "data" )
               cout << "data (" << g_data_pages << " * "
                << c_data_page_bytes << ") = " << ( g_data_pages * c_data_page_bytes ) << " bytes\n";

            if( arg_1.empty( ) || arg_1 == "call" )
               cout << "call (" << g_call_stack_pages << " * "
                << c_call_stack_page_bytes << ") = " << ( g_call_stack_pages * c_call_stack_page_bytes ) << " bytes\n";

            if( arg_1.empty( ) || arg_1 == "user" )
               cout << "user (" << g_user_stack_pages << " * "
                << c_user_stack_page_bytes << ") = " << ( g_user_stack_pages * c_user_stack_page_bytes ) << " bytes\n";
         }
         else
         {
            int pages = atoi( arg_2.c_str( ) );
            if( pages <= 0 )
               throw runtime_error( "invalid # pages value: " + arg_2 );

            if( arg_1 == "code" )
            {
               g_code_pages = pages;

               ap_code.reset( new int8_t[ g_code_pages * c_code_page_bytes ] );

               memset( ap_code.get( ), 0, g_code_pages * c_code_page_bytes );
            }
            else
            {
               if( arg_1 == "data" )
                  g_data_pages = pages;
               else if( arg_1 == "call" )
                  g_call_stack_pages = pages;
               else if( arg_1 == "user" )
                  g_user_stack_pages = pages;
               else
                  throw runtime_error( "unexpected arg_1 '" + arg_1 + "' for 'size' command" );

               ap_data.reset( new int8_t[ g_data_pages * c_data_page_bytes
               + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes ] );

               memset( ap_data.get( ), 0, g_data_pages * c_data_page_bytes
                + g_call_stack_pages * c_call_stack_page_bytes + g_user_stack_pages * c_user_stack_page_bytes );
            }
         }
      }
      else if( cmd == "step" )
      {
         int32_t steps = 0;
         int32_t num_steps = 0;

         if( !arg_1.empty( ) )
            num_steps = atoi( arg_1.c_str( ) );

         if( state.finished )
            reset_machine( state,
             ap_code.get( ), g_code_pages * c_code_page_bytes,
             ap_data.get( ), g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes, g_user_stack_pages * c_user_stack_page_bytes );

         while( true )
         {
            if( !check_has_balance( ) )
               break;

            int rc = process_op(
             ap_code.get( ), g_code_pages * c_code_page_bytes,
             ap_data.get( ), g_data_pages * c_data_page_bytes,
             g_call_stack_pages * c_call_stack_page_bytes,
             g_user_stack_pages * c_user_stack_page_bytes, false, false, state );

            if( !check_has_balance( ) )
               break;

            --g_balance;

            if( rc >= 0 )
            {
               ++steps;
               if( state.stopped || state.finished || num_steps && steps >= num_steps )
               {
                  if( state.stopped )
                     cout << "(stopped)\n";
                  else if( state.finished )
                     cout << "(finished)\n";

                  break;
               }
               else if( !num_steps )
                  break;
            }
            else
            {
               if( rc == -1 )
                  cout << "error: overflow\n";
               else if( rc == -2 )
                  cout << "error: invalid code\n";
               else
                  cout << "unexpected error\n";

               break;
            }
         }
      }
      else if( cmd == "break" )
      {
         if( arg_1.empty( ) )
         {
            for( set< int32_t >::iterator i = break_points.begin( ); i != break_points.end( ); ++i )
               cout << *i << '\n';
         }
         else
         {
            int32_t bp;
            if( arg_1.size( ) > 2 && arg_1.find( "0x" ) == 0 )
            {
               istringstream isstr( arg_1.substr( 2 ) );
               isstr >> hex >> bp;
            }
            else
               bp = atoi( arg_1.c_str( ) );

            if( break_points.count( bp ) )
               break_points.erase( bp );
            else
               break_points.insert( bp );
         }
      }
      else if( cmd == "reset" )
         reset_machine( state,
          ap_code.get( ), g_code_pages * c_code_page_bytes,
          ap_data.get( ), g_data_pages * c_data_page_bytes,
          g_call_stack_pages * c_call_stack_page_bytes, g_user_stack_pages * c_user_stack_page_bytes );
      else if( cmd == "state" )
         dump_state( state );
      else if( cmd == "balance" )
      {
         if( arg_1.empty( ) )
            cout << dec << g_balance << '\n';
         else
            g_balance = atoi( arg_1.c_str( ) );
      }
      else if( cmd == "function" && !arg_1.empty( ) )
      {
         bool is_increment_func = false;
         if( arg_1[ 0 ] == '+' )
         {
            is_increment_func = true;
            arg_1.erase( 0, 1 );
         }

         int32_t func = atoi( arg_1.c_str( ) );

         if( is_increment_func )
            g_increment_func = func;

         if( ( !is_increment_func || !arg_2.empty( ) ) && g_function_data.count( func ) )
            g_function_data.erase( func );

         if( !arg_2.empty( ) )
         {
            while( true )
            {
               string::size_type pos = arg_2.find( ',' );

               string next( arg_2.substr( 0, pos ) );

               int64_t val;
               if( next.size( ) > 2 && next.find( "0x" ) == 0 )
               {
                  istringstream isstr( next );
                  isstr >> hex >> val;
               }
               else
                  val = atoi( next.c_str( ) );

               g_function_data[ func ].data.push_back( val );

               if( pos == string::npos )
                  break;

               arg_2.erase( 0, pos + 1 );
            }

            if( arg_3 == "true" )
               g_function_data[ func ].loop = true;
         }
      }
      else if( cmd == "functions" )
      {
         for( map< int32_t, function_data >::iterator i = g_function_data.begin( ); i != g_function_data.end( ); ++i )
         {
            if( i->first == g_increment_func )
               cout << '+';
            else
               cout << ' ';

            cout << dec << setw( 3 ) << setfill( '0' ) << i->first;

            for( size_t j = 0; j < i->second.data.size( ); j++ )
               cout << ( j == 0 ? ' ' : ',' ) << "0x" << hex << setw( 16 ) << i->second.data[ j ];

            if( i->second.loop )
               cout << " true\n";
            else
               cout << " false\n";
         }
      }
      else if( cmd == "quit" || cmd == "exit" )
         break;
      else
         cout << "invalid command: " << cmd << endl;
   }
}

