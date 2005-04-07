#include "register.h"
#include "common.h"

tRegister decodeRegister(unsigned uRawData)
{
	switch(uRawData)
	{
		case 0x00:
			return R_ZERO;
		case 0x01:
			return R_AT;
		case 0x02:
			return R_V0;
		case 0x03:
			return R_V1;
		case 0x04:
			return R_A0;
		case 0x05:
			return R_A1;
		case 0x06:
			return R_A2;
		case 0x07:
			return R_A3;
		case 0x08:
			return R_T0;
		case 0x09:
			return R_T1;
		case 0x0A:
			return R_T2;
		case 0x0B:
			return R_T3;
		case 0x0C:
			return R_T4;
		case 0x0D:
			return R_T5;
		case 0x0E:
			return R_T6;
		case 0x0F:
			return R_T7;
		case 0x10:
			return R_S0;
		case 0x11:
			return R_S1;
		case 0x12:
			return R_S2;
		case 0x13:
			return R_S3;
		case 0x14:
			return R_S4;
		case 0x15:
			return R_S5;
		case 0x16:
			return R_S6;
		case 0x17:
			return R_S7;
		case 0x18:
			return R_T8;
		case 0x19:
			return R_T9;
		case 0x1C:
			return R_GP;
		case 0x1D:
			return R_SP;
		case 0x1E:
			return R_FP;
		case 0x1F:
			return R_RA;
		default:
			M_ASSERT(false);
			return R_UNKNOWN;
	}
}

const char *getRegName(tRegister eRegister)
{
	switch(eRegister)
	{
		case R_ZERO:
			return "0";
		case R_A0:
			return "a0";
		case R_A1:
			return "a1";
		case R_A2:
			return "a2";
		case R_A3:
			return "a3";
		case R_AT:
			return "at";
		case R_FP:
			return "fp";
		case R_GP:
			return "gp";
		case R_RA:
			return "ra";
		case R_S0:
			return "s0";
		case R_S1:
			return "s1";
		case R_S2:
			return "s2";
		case R_S3:
			return "s3";
		case R_S4:
			return "s4";
		case R_S5:
			return "s5";
		case R_S6:
			return "s6";
		case R_S7:
			return "s7";
		case R_SP:
			return "sp";
		case R_T0:
			return "t0";
		case R_T1:
			return "t1";
		case R_T2:
			return "t2";
		case R_T3:
			return "t3";
		case R_T4:
			return "t4";
		case R_T5:
			return "t5";
		case R_T6:
			return "t6";
		case R_T7:
			return "t7";
		case R_T8:
			return "t8";
		case R_T9:
			return "t9";
		case R_V0:
			return "v0";
		case R_V1:
			return "v1";
		case R_UNKNOWN:
			M_ASSERT(false);
			return "<UNKN>";
		default:
			M_ASSERT(false);
			return "<DEF>";
	}
}
