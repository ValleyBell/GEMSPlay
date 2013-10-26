void VBLINT(void);
void DACME(void);
void gems_init(void);
void gems_loop(void);
void STARTSEQ(UINT8 SeqNum);

void SetROMFile(const UINT8* ROMPtr);
void SetDataFiles(const UINT8* PatPtr, const UINT8* EnvPtr,
				  const UINT8* SeqPtr, const UINT8* SmpPtr);
const UINT8* GetDataPtr(UINT8 DataType);
void WriteFIFOCommand(UINT8 CmdByte);
void FinishCommandFIFO(void);
