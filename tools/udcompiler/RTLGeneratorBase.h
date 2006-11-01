#ifndef RTLGENERATORBASE_H
#define RTLGENERATORBASE_H

#ifndef LOADERBASE_H
#include "LoaderBase.h"
#endif

#include <list>

class RTLGeneratorBase
{
public:
	class Argument
	{
	public:
		enum eRegister
		{
			REGISTER_UNKNOWN,

			REGISTER_R001,
			REGISTER_R002,
			REGISTER_R003,
			REGISTER_R004
		};

		enum eType
		{
			TYPE_UNKNOWN,

			TYPE_REGISTER,
			TYPE_VALUE,
			TYPE_MEMORY_LOC
		};

		enum eDataWidth
		{
			DATA_WIDTH_UNKNOWN,

			DATA_WIDTH_8_LW_LB,
			DATA_WIDTH_8_LW_HB,
			DATA_WIDTH_8_HW_LB,
			DATA_WIDTH_8_HW_HB,
			DATA_WIDTH_16_LW,
			DATA_WIDTH_16_HW,
			DATA_WIDTH_32
		};

		Argument() :
			m_enType(TYPE_UNKNOWN),
			m_enRegister(REGISTER_UNKNOWN),
			m_enDataWidth(DATA_WIDTH_UNKNOWN),
			m_uValue(0x0BADC0DE)
		{}

		eType getType() const { return m_enType; }
		void setType(eType enType) { m_enType = enType; }

		eRegister getRegister() const { return m_enRegister; }
		void setRegister(eRegister enRegister) { m_enRegister = enRegister; }

		eDataWidth getDataWidth() const { return m_enDataWidth; }
		void setDataWidth(eDataWidth enDataWidth) { m_enDataWidth = enDataWidth; }

		unsigned getValue() const { return m_uValue; }
		void setValue(unsigned uValue) { m_uValue = uValue; }

	private:
		eType m_enType;
		eRegister m_enRegister;
		eDataWidth m_enDataWidth;
		unsigned m_uValue;
	};

	class RTLOp
	{
	public:
		enum eType
		{
			OP_UNKNOWN,

			OP_ADD,
			OP_ASSIGN,
			OP_CALL,
			OP_COMPARE,
			OP_JUMP,
			OP_NOP,
			OP_RETURN,
			OP_SUBTRACT
		};

		RTLOp() :
		  m_enType(OP_UNKNOWN)
		{}

		eType getType() const { return m_enType; }
		void setType(eType enType) { m_enType = enType; }

		Argument &getArg1() { return m_aArg1; }
		Argument &getArg2() { return m_aArg2; }
		Argument &getArg3() { return m_aArg3; }
		const Argument &getArg1() const { return m_aArg1; }
		const Argument &getArg2() const { return m_aArg2; }
		const Argument &getArg3() const { return m_aArg3; }

	private:
		eType m_enType;
		Argument m_aArg1;
		Argument m_aArg2;
		Argument m_aArg3;
	};

	typedef std::list<RTLOp> tCollRTLOps;

	virtual bool generateRTL(const LoaderBase::tCollData &collData, tCollRTLOps &collRTLOps) = 0;
	bool dumpRTL(const tCollRTLOps &collRTLOps) const;

protected:
	virtual std::string getRegisterName(Argument::eRegister enRegister, Argument::eDataWidth enDataWidth) const = 0;
	std::string getDataWidthSuffix(Argument::eDataWidth enDataWidth) const;

private:
	bool dumpArg(const Argument &aArg) const;
};

#endif