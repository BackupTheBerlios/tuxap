#include "X86RTLGenerator.h"

bool X86RTLGenerator::generateRTL(const LoaderBase::tCollData &collData, tCollRTLOps &collRTLOps)
{
	unsigned uIdx = 0;

	while(uIdx < 52)
	//while(uIdx < collData.size())
	{
		x86_insn_t aInsn;
		uIdx += x86_disasm((unsigned char *)&collData[0], collData.size(), 0, uIdx, &aInsn);

		RTLOp aRTLOp;

		if(!parseInsn(aInsn, aRTLOp))
		{
			return false;
		}

		collRTLOps.push_back(aRTLOp);

		char buf[1024];
		x86_format_insn(&aInsn, buf, sizeof(buf), intel_syntax);
		printf("0x%08X: %s\n", uIdx, buf);
	}

	return true;
}

bool X86RTLGenerator::parseInsn(x86_insn_t &aInsn, RTLOp &aRTLOp)
{
	size_t uOpCount = x86_operand_count(&aInsn, op_explicit);
	unsigned uExpectedOpCount = 0xFFFFFFFF;

	switch(aInsn.type)
	{
		case insn_add:
			aRTLOp.setType(RTLOp::OP_ADD);
			uExpectedOpCount = 2;
			break;
		case insn_call:
			aRTLOp.setType(RTLOp::OP_CALL);
			break;	
		case insn_jcc:
			aRTLOp.setType(RTLOp::OP_JUMP);
			break;
		case insn_jmp:
			aRTLOp.setType(RTLOp::OP_JUMP);
			break;
		case insn_mov:
			aRTLOp.setType(RTLOp::OP_ASSIGN);
			break;
		case insn_return:
			aRTLOp.setType(RTLOp::OP_RETURN);
			break;
		case insn_sub:
			aRTLOp.setType(RTLOp::OP_SUBTRACT);
			break;
		case insn_test:
			aRTLOp.setType(RTLOp::OP_COMPARE);
			break;
		case insn_push:
		case insn_pop:
			aRTLOp.setType(RTLOp::OP_NOP);
			break;
		default:
			return false;
	}

	if((uOpCount >= 1) && (!parseArgument(x86_operand_1st(&aInsn), aRTLOp.getArg1())))
	{
		return false;
	}

	if((uOpCount >= 2) && (!parseArgument(x86_operand_2nd(&aInsn), aRTLOp.getArg2())))
	{
		return false;
	}

	if((uOpCount >= 3) && (!parseArgument(x86_operand_3rd(&aInsn), aRTLOp.getArg3())))
	{
		return false;
	}

	return true;
}

bool X86RTLGenerator::parseArgument(x86_op_t *pOp, Argument &aArgument)
{
	if(!pOp)
	{
		return false;
	}

	switch(pOp->type)
	{
		case op_register:
		{
			aArgument.setType(Argument::TYPE_REGISTER);
			std::string strRegisterName = pOp->data.reg.name;

			if(strRegisterName == "ah")
			{
				aArgument.setRegister((Argument::eRegister)REGISTER_X86_EAX);
				aArgument.setDataWidth(Argument::DATA_WIDTH_8_LW_HB);
			}
			else if(strRegisterName == "al")
			{
				aArgument.setRegister((Argument::eRegister)REGISTER_X86_EAX);
				aArgument.setDataWidth(Argument::DATA_WIDTH_8_LW_LB);
			}
			else if(strRegisterName == "ax")
			{
				aArgument.setRegister((Argument::eRegister)REGISTER_X86_EAX);
				aArgument.setDataWidth(Argument::DATA_WIDTH_16_LW);
			}
			else if(strRegisterName == "eax")
			{
				aArgument.setRegister((Argument::eRegister)REGISTER_X86_EAX);
				aArgument.setDataWidth(Argument::DATA_WIDTH_32);
			}
			else if(strRegisterName == "esi")
			{
				aArgument.setRegister((Argument::eRegister)REGISTER_X86_ESI);
				aArgument.setDataWidth(Argument::DATA_WIDTH_32);
			}
			else if(strRegisterName == "esp")
			{
				aArgument.setRegister((Argument::eRegister)REGISTER_X86_ESP);
				aArgument.setDataWidth(Argument::DATA_WIDTH_32);
			}
			else
				return false;
			break;
		}
		case op_immediate:
			aArgument.setType(Argument::TYPE_VALUE);

			switch(pOp->datatype)
			{
				case op_byte:
					aArgument.setValue(pOp->data.byte);
					aArgument.setDataWidth(Argument::DATA_WIDTH_8_LW_LB);
					break;
				default:
					return false;
			}
			break;
		case op_expression:
			aArgument.setType(Argument::TYPE_MEMORY_LOC);
			break;
		case op_offset:
			aArgument.setType(Argument::TYPE_MEMORY_LOC);
			break;
		case op_relative_far:
			aArgument.setType(Argument::TYPE_VALUE);
			break;
		case op_relative_near:
			aArgument.setType(Argument::TYPE_VALUE);
			break;
		default:
			return false;
	}

	return true;
}

std::string X86RTLGenerator::getRegisterName(Argument::eRegister enRegister, Argument::eDataWidth enDataWidth) const
{
	eRegisterX86 enRegisterX86 = (eRegisterX86)enRegister;

	switch(enRegisterX86)
	{
		case REGISTER_X86_EAX:
			return "EAX." + getDataWidthSuffix(enDataWidth);
		case REGISTER_X86_ESI:
			return "ESI." + getDataWidthSuffix(enDataWidth);
		case REGISTER_X86_ESP:
			return "ESP." + getDataWidthSuffix(enDataWidth);
		default:
			return "UNKNOWN";
	}
}
