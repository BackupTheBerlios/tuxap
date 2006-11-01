#ifndef X86RTLGENERATOR_H
#define X86RTLGENERATOR_H

#ifndef RTLGENERATORBASE_H
#include "RTLGeneratorBase.h"
#endif

extern "C" {
#include "libdisasm_0.21-pre2/libdisasm/libdis.h"
}

class X86RTLGenerator : public RTLGeneratorBase
{
public:
	enum eRegisterX86
	{
		REGISTER_X86_UNKNOWN = Argument::REGISTER_UNKNOWN,

		REGISTER_X86_EAX = Argument::REGISTER_R001,
		REGISTER_X86_ESI = Argument::REGISTER_R002,
		REGISTER_X86_ESP = Argument::REGISTER_R003
	};

	bool generateRTL(const LoaderBase::tCollData &collData, tCollRTLOps &collRTLOps);

private:
	bool parseInsn(x86_insn_t &aInsn, RTLOp &aRTLOp);
	bool parseArgument(x86_op_t *pOp, Argument &aArgument);
	std::string getRegisterName(Argument::eRegister enRegister, Argument::eDataWidth enDataWidth) const;
};

#endif