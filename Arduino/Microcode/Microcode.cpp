
#include "Microcode.h"

#include "Constants.h"

static const uint8_t STEP1 = 2; // first real step
static const uint8_t STEP2 = STEP1 + 1;
static const uint8_t STEP3 = STEP1 + 2;
static const uint8_t STEP4 = STEP1 + 3;
static const uint8_t STEP5 = STEP1 + 4;
static const uint8_t STEP6 = STEP1 + 5;

static const uint32_t INSTRUCTION_END     = _TR;
static const uint32_t READ_PROGRAM_MEMORY = PGM | BW_MEM;
static const uint32_t READ_MEMORY         = BW_MEM;

static const uint32_t SET_MAW_FROM_PC     = Register::PC().writeToBus() | _MAW;

uint32_t getImmediateMovControlWord(const Register &dest, uint8_t microtime, std::string& desc)
{
  if (dest == Register::PC())
  {
    desc = "jmpi Imm";
  }
  else
  {
    desc = "movi " + dest.toString() + ", " + "Imm";
  }


  switch (microtime)
  {
    // write next program address to memory address register
    case STEP1: return SET_MAW_FROM_PC;

    // read immediate value into destimation register
    case STEP2: return (dest == Register::PC() ? 0 : PCC) | READ_PROGRAM_MEMORY | dest.readFromBus() | INSTRUCTION_END;
  }

  return INSTRUCTION_END;
}

uint32_t getClearAllControlWord(uint8_t microtime, std::string& desc)
{
  desc = "clra";

  switch (microtime)
  {
    // clear accumulator
    case STEP1: return Register::PC().writeToBus() | ALU_A_AND_B | _ALW;

    // write to all general purpose registers
    case STEP2: return Register::Acc().writeToBus() |
                       Register::Ra().readFromBus() |
                       Register::Rb().readFromBus() |
                       Register::Rc().readFromBus() |
                       Register::Rd().readFromBus() |
                       Register::StP().readFromBus() |
                       INSTRUCTION_END;
  }

  return INSTRUCTION_END;
}

uint32_t getConditionalJumpControlWord(const EepromAddress &address, std::string& desc)
{
  static const uint8_t Carry = 0b000;
  static const uint8_t Zero = 0b001;
  static const uint8_t Oflow = 0b010;
  static const uint8_t Neg = 0b100;

  static const uint8_t NotCarry = (~Carry) & 0x7;
  static const uint8_t NotZero = (~Zero) & 0x7;
  static const uint8_t NotOflow = (~Oflow) & 0x7;
  static const uint8_t NotNeg = (~Neg) & 0x7;

  bool doJump = false;

  switch (address.opcode().srcReg())
  {
    case Carry:
      desc = "jc";
      doJump = address.isCarryFlagSet();
      break;

    case Zero:
      desc = "jz";
      doJump = address.isZeroFlagSet();
      break;

    case Oflow:
      desc = "jo";
      doJump = address.isOverflowFlagSet();
      break;

    case Neg:
      desc = "jn";
      doJump = address.isNegativeFlagSet();
      break;

    case NotCarry:
      desc = "jnc";
      doJump = !address.isCarryFlagSet();
      break;

    case NotZero:
      desc = "jnz";
      doJump = !address.isZeroFlagSet();
      break;

    case NotOflow:
      desc = "jno";
      doJump = !address.isOverflowFlagSet();
      break;

    case NotNeg:
      desc = "jnn";
      doJump = !address.isNegativeFlagSet();
      break;
  }

  if (doJump)
  {
    switch (address.microtime())
    {
      case STEP1: return SET_MAW_FROM_PC;
      case STEP2: return READ_PROGRAM_MEMORY | Register::PC().readFromBus() | INSTRUCTION_END;
    }
  }
  else
  {
    if (address.microtime() == STEP1)
    {
      return PCC;
    }
  }
  return INSTRUCTION_END;
}


uint32_t getMovControlWord(const EepromAddress& address, std::string& desc)
{
  Opcode opcode = address.opcode();
  Register dest = opcode.destReg();
  Register src = opcode.srcReg();

  if (dest == Register::Imm())
  {
    return getConditionalJumpControlWord(address, desc);
  }
  else if (src == Register::Imm())
  {
    if (dest == Register::Acc())
    {
      // clra: 00 110 111
      return getClearAllControlWord(address.microtime(), desc);
    }
    else
    {
      // movi: 00 dst 111
      return getImmediateMovControlWord(dest, address.microtime(), desc);
    }
  }
  else if (dest == Register::Acc())
  {
    if (src == Register::PC())
    {
      desc = "jmz";
      switch (address.microtime())
      {
        // set pc to 0
        case STEP1: return Register::PC().writeToBus() | ALU_A_AND_B | _ALW;
        case STEP2: return Register::Acc().writeToBus() | Register::PC().readFromBus() | INSTRUCTION_END;
      }
    }
    else if (src != dest)
    {
      desc = "tst " + src.toString();
      switch (address.microtime())
      {
        // set accumulator
        case STEP1: return src.writeToBus() | ALU_A_PLUS_B | _ALW | INSTRUCTION_END;
      }
    }
  }
  else if (src != dest) {
    if (dest == Register::PC())
    {
      desc = "jmp " + src.toString();
    }
    else
    {
      desc = opcode.describe();
    }

    switch (address.microtime())
    {
      // copy src -> dest
      case STEP1: return src.writeToBus() | dest.readFromBus() | INSTRUCTION_END;
    }
  }
  else if (dest == Register::PC())
  {
    desc = "hlt";
    // halt
    return HLT;
  }
  else if (dest == Register::Ra())
  {
    desc = "nop";
    // nop
  }

  return INSTRUCTION_END;
}

uint32_t getLodControlWord(const EepromAddress& address, std::string& desc)
{
  Opcode opcode = address.opcode();
  Register dest = opcode.destReg();
  Register src = opcode.srcReg();

  if (dest == Register::StPi())
  {
    // peek
    if (src < Register::StP())
    {
      desc = "peek " + src.toString();
      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _MAW;
        case STEP2: return src.readFromBus() | BW_MEM | INSTRUCTION_END;
      }
    }
    else if (src == Register::StP()) // mem to lcd command
    {      
      desc = "lcc mem";
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC;
        case STEP2: return PCC | BW_MEM | PGM | _MAW;
        case STEP3: return BW_MEM | LCD_COMMAND | LCD | INSTRUCTION_END;
      }
    }
    else if (src == Register::PC()) // mem to lcd data
    {
      desc = "lcd mem";
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC;
        case STEP2: return PCC | BW_MEM | PGM | _MAW;
        case STEP3: return BW_MEM | _ALW | ALU_A_PLUS_B;
        case STEP4: return LCD_DATA | LCD | Register::Acc().writeToBus() | INSTRUCTION_END;
      }
    }
    else if (src == Register::StPi()) // pgm mem to lcd command
    {
      desc = "lcc pgm";
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC;
        case STEP2: return PCC | BW_MEM | PGM | _MAW;
        case STEP3: return BW_MEM | PGM | _ALW | ALU_A_PLUS_B;
        case STEP4: return LCD_COMMAND | LCD | Register::Acc().writeToBus() | INSTRUCTION_END;
      }
    }
    else if (src == Register::Imm()) // pgm mem to lcd command
    {
      desc = "lcd pgm";
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC;
        case STEP2: return PCC | BW_MEM | PGM | _MAW;
        case STEP3: return BW_MEM | PGM | LCD_DATA | LCD | INSTRUCTION_END;
      }
    }
  }
  else if (src == Register::StPi())
  {
    // pop / ret
    if (dest == Register::PC())
    {
      desc = "ret";

      switch (address.microtime())
      {
        case STEP1: return Register::Acc().writeToBus() | Register::PC().readFromBus(); // temporarily store Acc
        case STEP2: return Register::StP().writeToBus() | _ALW | ALC | ALU_A_PLUS_B | _MAW;
        case STEP3: return Register::StP().readFromBus() | BW_ALU;
        case STEP4: return Register::PC().writeToBus() | _ALW | ALU_A_PLUS_B; // restore Acc
        case STEP5: return dest.readFromBus() | BW_MEM | INSTRUCTION_END;
      }

    }
    else if (dest != Register::Imm())
    {
      desc = "pop " + dest.toString();
      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _ALW | ALC | ALU_A_PLUS_B | _MAW;
        case STEP2: return Register::StP().readFromBus() | BW_ALU;
        case STEP3: return dest.readFromBus() | BW_MEM | INSTRUCTION_END;
      }
    }
    else
    {
      desc = "lcc imm";
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC; 
        case STEP2: return PCC | BW_MEM | PGM | _ALW | ALU_A_PLUS_B;
        case STEP3: return LCD_COMMAND | LCD | Register::Acc().writeToBus() | INSTRUCTION_END;
      }
    }
  }
  else if (src == Register::Imm())
  {
    // load data from an immediate address to register
    if (dest != src)
    {
      desc = opcode.describe() + " (" + dest.toString() + " = *Imm)";
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC;
        case STEP2: return PCC | BW_MEM | PGM | _MAW;
        case STEP3: return BW_MEM | dest.readFromBus() | INSTRUCTION_END;
      }
    }
    else
    {
      desc = "lcd imm";      
      switch (address.microtime())
      {
        case STEP1: return _MAW | BW_PC;
        case STEP2: return PCC | BW_MEM | PGM | LCD_DATA | LCD | INSTRUCTION_END;
      }
    }
  }
  else if (dest == Register::Imm())
  {
    // clear a single register
    desc = "clr " + src.toString();
    switch (address.microtime())
    {
      case STEP1: return Register::PC().writeToBus() | ALU_A_AND_B | _ALW;
      case STEP2: return Register::Acc().writeToBus() | src.readFromBus() | INSTRUCTION_END;
    }
  }
  else
  {
    // load data from address in src to register dest
    desc = opcode.describe() + " (" + dest.toString() + " = " + (src == Register::Rc() ? "PGM*" : "*") + src.toString() + ")";
    switch (address.microtime())
    {
      case STEP1: return _MAW | src.writeToBus();
      case STEP2: return ((src == Register::Rc()) ? PGM : 0) | BW_MEM | dest.readFromBus() | INSTRUCTION_END;
    }
  }

  return _TR;
}

uint32_t getStoControlWord(const EepromAddress& address, std::string& desc)
{
  Opcode opcode = address.opcode();
  Register dest = opcode.destReg();
  Register src = opcode.srcReg();

  if (dest == Register::StPi())
  {
    if (src == Register::Imm())
    {
      // push immediate
      desc = "pushi <= Imm";
      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _ALW | ALU_A_MINUS_B;
        case STEP2: return _StPW | BW_ALU;
        case STEP3: return Register::PC().writeToBus() | _MAW;
        case STEP4: return PCC | PGM | BW_MEM | _ALW | ALU_A_PLUS_B;
        case STEP5: return Register::StP().writeToBus() | _MAW;
        case STEP6: return _MW | Register::Acc().writeToBus() | INSTRUCTION_END;
      }
    }
    else if (src == Register::PC())
    {
      // call (address in Rc)
      desc = "call Rc";
      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _ALW | ALU_A_MINUS_B;
        case STEP2: return _StPW | BW_ALU | _MAW;
        case STEP3: return Register::PC().writeToBus() | _MW;
        case STEP4: return Register::Rc().writeToBus() | Register::PC().readFromBus() | INSTRUCTION_END;
      }
    }
    else
    {
      // push
      desc = "push <= " + src.toString();
      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _ALW | ALU_A_MINUS_B;
        case STEP2: return _StPW | BW_ALU | _MAW;
        case STEP3: return src.writeToBus() | _MW | INSTRUCTION_END;
      }
    }
  }
  else if (dest == Register::Imm())
  {
    if (src == Register::PC())
    {
      // call immediate
      desc = "calli";
      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _ALW | ALU_A_MINUS_B;
        case STEP2: return _StPW | BW_ALU | _MAW;
        case STEP3: return src.writeToBus() | _ALW | ALU_A_PLUS_B | ALC;
        case STEP4: return BW_ALU | _MW;
        case STEP5: return src.writeToBus() | _MAW;
        case STEP6: return PGM | BW_MEM | src.readFromBus() | INSTRUCTION_END;
      }
    }
    else
    {
      // store immediate value to immediate address
      if (src == Register::Imm())
      {
        desc = "stoi (PGM*Imm2 = Imm1)";
        switch (address.microtime())
        {
          case STEP1: return Register::PC().writeToBus() | _MAW;
          case STEP2: return PCC | PGM | BW_MEM | _MAW | _ALW | ALU_A_PLUS_B; // store value in ALU
          case STEP3: return Register::PC().writeToBus() | _MAW;
          case STEP4: return PCC | PGM | BW_MEM | _MAW;
          case STEP5: return _MW | PGM | BW_ALU | INSTRUCTION_END;
        }
      }
      // store value in src to immediate address
      else
      {
        desc = "stoi " + src.toString() + " (*Imm = " + src.toString() + ")";
        switch (address.microtime())
        {
          case STEP1: return Register::PC().writeToBus() | _MAW;
          case STEP2: return PCC | PGM | BW_MEM | _MAW;
          case STEP3: return _MW | src.writeToBus() | INSTRUCTION_END;
        }
      }
    }
  }
  else if (src == Register::StPi())
  {
    // pop / ret
    if (dest == Register::PC())
    {
      desc = "ret";

      switch (address.microtime())
      {
        case STEP1: return Register::Acc().writeToBus() | Register::PC().readFromBus(); // temporarity store Acc
        case STEP2: return Register::StP().writeToBus() | _ALW | ALC | ALU_A_PLUS_B | _MAW;
        case STEP3: return Register::StP().readFromBus() | BW_ALU;
        case STEP4: return Register::PC().writeToBus() | _ALW | ALU_A_PLUS_B; // restore Acc
        case STEP5: return dest.readFromBus() | BW_MEM | INSTRUCTION_END;
      }

    }
    else
    {
      desc = "pop => " + dest.toString();

      switch (address.microtime())
      {
        case STEP1: return Register::StP().writeToBus() | _ALW | ALC | ALU_A_PLUS_B | _MAW;
        case STEP2: return Register::StP().readFromBus() | BW_ALU;
        case STEP3: return dest.readFromBus() | BW_MEM | INSTRUCTION_END;
      }
    }
  }
  else
  {
    desc  = opcode.describe() + " (" + (dest == Register::Rc() ? "PGM" : "") + "*" + dest.toString() + " = " + src.toString() + ")";

    // store value in src to address in dest
    switch (address.microtime())
    {
      case STEP1: return dest.writeToBus() | _MAW;
      case STEP2: return (dest == Register::Rc() ? PGM : 0) | src.writeToBus() | _MW | INSTRUCTION_END;
    }
  }
  return _TR;
}

uint32_t getAluControlWord(const EepromAddress& address, std::string& desc)
{
  AluOpcode &opcode((AluOpcode&)address.opcode());
  Register reg = opcode.aluReg();
  AluMode mode = opcode.aluMode();

  // inc/dec
  if (mode == AluMode::INC_A())
  {
    bool dec = opcode.useCarry();

    desc = (dec ? "dec " : "inc ") + reg.toString();

    switch (address.microtime())
    {
      case STEP1: return reg.writeToBus() | (dec ? ALU_A_MINUS_B : (ALU_A_PLUS_B | ALC)) | _ALW;
      case STEP2: return reg.readFromBus() | Register::Acc().writeToBus() | INSTRUCTION_END;
    }    
  }
  else if (mode == AluMode::A_PLUS_B())
  {
    desc = opcode.describe();
    
    switch (address.microtime())
    {
      case STEP1: return reg.writeToBus() | ALB | (mode << ALU_OFFSET) | _ALW | ((opcode.useCarry() && address.isCarryFlagSet()) ? ALC : 0) ;
      case STEP2: return reg.readFromBus() | Register::Acc().writeToBus() | INSTRUCTION_END;
    }
  }
  else if (mode == AluMode::A_MINUS_B() || mode == AluMode::B_MINUS_A())
  {
    desc = opcode.describe();

    switch (address.microtime())
    {
      case STEP1: return reg.writeToBus() | ALB | (mode << ALU_OFFSET) | _ALW | ((opcode.useCarry() && address.isCarryFlagSet()) ? 0 : ALC);
      case STEP2: return reg.readFromBus() | Register::Acc().writeToBus() | INSTRUCTION_END;
    }
  }
  else if (opcode.useCarry())
  {
    // cmp Rb, reg
    if (mode == AluMode::A_OR_B())
    {
      desc = "cmp Rb, " + reg.toString();

      mode = AluMode::B_MINUS_A();
    }
    // cmp reg, Rb
    else if (mode == AluMode::A_AND_B())
    {
      desc = "cmp " + reg.toString() + ", Rb";

      mode = AluMode::A_MINUS_B();
    }
    else if (mode == AluMode::A_XOR_B()) // lcd command
    {
      desc = "lcc " + reg.toString();
      switch (address.microtime())
      {
        case STEP1: return LCD_COMMAND | LCD | reg.writeToBus() | INSTRUCTION_END;
      }
    }
    else if (mode == AluMode::NOT_A()) // lcd data
    {
      desc = "lcd " + reg.toString();
      switch (address.microtime())
      {
        case STEP1: return LCD_DATA | LCD | reg.writeToBus() | INSTRUCTION_END;
      }
    }

    switch (address.microtime())
    {
      case STEP1: return reg.writeToBus() | ALB | ALC | (mode << ALU_OFFSET) | _ALW | INSTRUCTION_END;
    }
  }
  else
  {
    desc = opcode.describe();

    if (mode == AluMode::NOT_A()) mode = AluMode::B_MINUS_A();

    //and, or, xor, not
    switch (address.microtime())
    {
      case STEP1: return reg.writeToBus() | ALB | (mode << ALU_OFFSET) | _ALW;
      case STEP2: return reg.readFromBus() | Register::Acc().writeToBus() | INSTRUCTION_END;
    }
  }


  return _TR;
}


extern uint32_t getControlWord(const EepromAddress &address, std::string &desc)
{
  // first steps are always to retrieve the next instruction
  // from memory and place into the instruction register
  switch (address.microtime())
  {
    case 0:
      return BW_PC | _MAW;

    case 1:
      return READ_PROGRAM_MEMORY | _IRW | PCC;

    default:
      break;
  }

  switch (address.opcode().group())
  {
    case OpcodeGroup::MOV_BITS:
      return getMovControlWord(address, desc);

    case OpcodeGroup::LOD_BITS:
      return getLodControlWord(address, desc);

    case OpcodeGroup::STO_BITS:
      return getStoControlWord(address, desc);

    case OpcodeGroup::ALU_BITS:
      return getAluControlWord(address, desc);

    default:
      return 0;
  }
}
