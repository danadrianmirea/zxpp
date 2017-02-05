#include "z80.h"

int parseNextInstruction(uint8_t* location)
{
    std::vector<uint8_t> bytes;
    while (prefixes.find(*location) != prefixes.end())
    {
        bytes.push_back(*location);
        location++;
    }
    bytes.push_back(*location);
    
    if ( bytes.size() > 1 && bytes[1] == 0xED ) { bytes[0] = 0; }     // Ignore other prefixes before ED

    assert(bytes.size() <= 3);

    while (bytes.size() < 3) { bytes.insert(bytes.begin(), 0); }   // Pad with zeros

    // return opcode(bytes[0], bytes[1], bytes[2]);

    int pref1 = 0;
    switch (bytes[0])
    {
        case 0xDD: pref1 = 1; break;
        case 0xFD: pref1 = 2; break;
        case 0xED: pref1 = 3; break;
        case 0xCB: pref1 = 4; break;
        default: pref1 = 0; break;
    }
    int pref2 = 0;
    switch (bytes[1])
    {
        case 0xDD: pref2 = 1; break;
        case 0xFD: pref2 = 2; break;
        case 0xED: pref2 = 3; break;
        case 0xCB: pref2 = 4; break;
        default: pref2 = 0; break;
    }

    // Compute the index in array, more info in instructions_preprocess.py
    return ((pref1 > 0) ? pref1 + 4 : 0) * 256 + pref2 * 256 + bytes[2];
}

void Z80::init()
{
    m_registers.PC = 0;
    m_IFF1 = 0;
    m_IFF2 = 0;
    m_registers.SP = 0xFFFF;
    m_registers.IX.word = 0xFFFF;
    m_registers.IY.word = 0xFFFF;
    m_registers.IR.word = 0x00FF;
    m_registers.AF.word = 0xFFFF;
    m_registers.BC.word = 0xFFFF;
    m_registers.DE.word = 0xFFFF;
    m_registers.HL.word = 0xFFFF;
    m_registers.AFx.word = 0xFFFF;
    m_registers.BCx.word = 0xFFFF;
    m_registers.DEx.word = 0xFFFF;
    m_registers.HLx.word = 0xFFFF;

    m_instructionSet = z80InstructionSet();
}

Z80::Z80()
{
    init();
}

Z80Registers* Z80::getRegisters()
{
    return &m_registers;
}

Z80IOPorts* Z80::getIoPorts()
{
    return &m_ioPorts;
}

void Z80::setIFF1(bool b)
{
    m_IFF1 = b;
}

void Z80::setIFF2(bool b)
{
    m_IFF2 = b;
}

bool Z80::getIFF2()
{
    return m_IFF2;
}

void Z80::halt()
{
    m_isHalted = true;
}

int Z80::getInterruptMode()
{
    return m_interruptMode;
}

void Z80::setInterruptMode(int m)
{
    m_interruptMode = m;
}

int Z80::runInstruction(int instBytes, Spectrum48KMemory* m)
{
    
    if (m_registers.PC == 0x09F4 || 
    (m_registers.PC > 1660-30 && m_registers.PC < 1660+30) || 
    (m_registers.PC > 2079-30 && m_registers.PC < 2079+30) || 
    (m_registers.PC > 2100-30 && m_registers.PC < 2100+30) || 
    (m_registers.PC > 2601-30 && m_registers.PC < 2601+30) || 
    (m_registers.PC > 3716-30 && m_registers.PC < 3716+30))
    {
        int gsd = 3;
    }

    /*auto found = m_instructionSet.find(instBytes);
    if (found == m_instructionSet.end()) { instBytes = {0, 0, 0}; }  // NOP*/
    Instruction instruction = m_instructionSet[instBytes];

    std::vector<uint8_t> data;
    for (int i = 0; i < instruction.numDataBytes; i++)
    {
        data.push_back((*m).memory[m_registers.PC + i]);
    }

    m_registers.PC += instruction.numDataBytes;
    auto prevPC = m_registers.PC;

    instruction.execute(this, m, data);

    int cyclesTaken = instruction.cycles;
    if (m_registers.PC != prevPC)       // Jump was taken
    {
        cyclesTaken = instruction.cyclesOnJump;
    }


    return cyclesTaken;
}

void Z80::nextInstruction(Spectrum48KMemory* m)
{
    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    // printState();    

    int instruction = parseNextInstruction(&(m->begin())[m_registers.PC]);
    // int numBytes = ( std::get<0>(instruction) == 0 ) ? ( (std::get<1>(instruction) == 0) ? 1 : 2 ) : 3;
    int numBytes = ( instruction >= 5*256 ) ? 3 : ( instruction >= 256 ) ? 2 : 1;
    m_registers.PC += numBytes;
    int cycles = runInstruction(instruction, m);
    double timeTaken = (int)cycles * CLOCK_TIME;

    int i = 0;
    duration<double> timeSpan;
    do
    {
        i++;
        auto now = high_resolution_clock::now();
        timeSpan = duration_cast<duration<double>>(now - start);
    } while ( timeSpan.count() < timeTaken );
    int bb = i;

}

void Z80::printState()
{
    std::cout << std::hex;
    std::cout << "S = " << m_registers.AF.bytes.low.SF << " Z = " << m_registers.AF.bytes.low.ZF;
    std::cout << " P/O = " << m_registers.AF.bytes.low.PF << " N = " << m_registers.AF.bytes.low.NF;
    std::cout << " C = " << m_registers.AF.bytes.low.CF << " I = " << +m_registers.IR.bytes.high;
    std::cout << " R = " << +m_registers.IR.bytes.low << " IE = " << m_IFF1;
    std::cout << " IM = " << m_interruptMode << std::endl;

    std::cout << "A = " << +m_registers.AF.bytes.high << " BC = " << m_registers.BC.word;
    std::cout << " DE = " << m_registers.DE.word << " HL = " << m_registers.HL.word;
    std::cout << " SP = " << m_registers.SP << " PC = " << m_registers.PC << std::endl;

    std::cout << "Ax = " << +m_registers.AFx.bytes.high << " BCx = " << m_registers.BCx.word;
    std::cout << " DEx = " << m_registers.DEx.word << " HLx = " << m_registers.HLx.word;
    std::cout << " IX = " << m_registers.IX.word << " IY = " << m_registers.IY.word << std::endl;

}