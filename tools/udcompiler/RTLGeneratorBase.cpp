#include "RTLGeneratorBase.h"

bool RTLGeneratorBase::dumpRTL(const tCollRTLOps &collRTLOps) const
{
	tCollRTLOps::const_iterator it = collRTLOps.begin();
	tCollRTLOps::const_iterator itEnd = collRTLOps.end();

	for(; it != itEnd; it++)
	{
		const RTLOp &aRTLOp = *it;

		switch(aRTLOp.getType())
		{
			case RTLOp::OP_ADD:
				printf("OP_ADD\n");
				break;
			case RTLOp::OP_ASSIGN:
				printf("OP_ASSIGN\n");
				break;
			case RTLOp::OP_CALL:
				printf("OP_CALL\n");
				break;
			case RTLOp::OP_COMPARE:
				printf("OP_COMPARE\n");
				break;
			case RTLOp::OP_JUMP:
				printf("OP_JUMP\n");
				break;
			case RTLOp::OP_NOP:
				printf("OP_NOP\n");
				break;
			case RTLOp::OP_RETURN:
				printf("OP_RETURN\n");
				break;
			case RTLOp::OP_SUBTRACT:
				printf("OP_SUBTRACT\n");
				break;
			case RTLOp::OP_UNKNOWN:
				printf("OP_UNKNOWN\n");
				break;
			default:
				return false;
		}

		if(aRTLOp.getArg1().getType() != Argument::TYPE_UNKNOWN)
		{
			if(!dumpArg(aRTLOp.getArg1()))
			{
				return false;
			}
		}

		if(aRTLOp.getArg2().getType() != Argument::TYPE_UNKNOWN)
		{
			if(!dumpArg(aRTLOp.getArg2()))
			{
				return false;
			}
		}

		if(aRTLOp.getArg3().getType() != Argument::TYPE_UNKNOWN)
		{
			if(!dumpArg(aRTLOp.getArg3()))
			{
				return false;
			}
		}

		printf("\n");
	}

	return true;
}

bool RTLGeneratorBase::dumpArg(const Argument &aArg) const
{
	switch(aArg.getType())
	{
		case Argument::TYPE_MEMORY_LOC:
			printf("\t[TYPE_MEMORY_LOC]\n");
			break;
		case Argument::TYPE_REGISTER:
			printf("\t[TYPE_REGISTER] CPU_REG_%s\n", getRegisterName(aArg.getRegister(), aArg.getDataWidth()).c_str());
			break;
		case Argument::TYPE_UNKNOWN:
			printf("\t[TYPE_UNKNOWN]\n");
			break;
		case Argument::TYPE_VALUE:
			printf("\t[TYPE_VALUE]\n");
			break;
		default:
			return false;
	}

	return true;
}

std::string RTLGeneratorBase::getDataWidthSuffix(Argument::eDataWidth enDataWidth) const
{
	switch(enDataWidth)
	{
		case Argument::DATA_WIDTH_8_LW_LB:
			return "8_LW_LB";
		case Argument::DATA_WIDTH_8_LW_HB:
			return "8_LW_HB";
		case Argument::DATA_WIDTH_8_HW_LB:
			return "8_HW_LB";
		case Argument::DATA_WIDTH_8_HW_HB:
			return "8_HW_HB";
		case Argument::DATA_WIDTH_16_LW:
			return "16_LW";
		case Argument::DATA_WIDTH_16_HW:
			return "16_HW";
		case Argument::DATA_WIDTH_32:
			return "32";
		default:
			return "UNKNOWN";
	}
}
