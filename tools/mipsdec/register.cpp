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
		case 0x10:
			return R_S0;
		case 0x11:
			return R_S1;
		case 0x12:
			return R_S2;
		case 0x13:
			return R_S3;
		case 0x1C:
			return R_GP;
		case 0x1D:
			return R_SP;
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
		case R_AT_DSB:
			return "at_DSB";
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
		case R_SP:
			return "sp";
		case R_V0:
			return "v0";
		case R_V0_DSB:
			return "v0_DSB";
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

tRegister getDSBRegister(tRegister aRegister)
{
	switch(aRegister)
	{
		case R_AT:
			return R_AT_DSB;
		case R_V0:
			return R_V0_DSB;
		default:
			M_ASSERT(false);
			return R_UNKNOWN;
	}
}