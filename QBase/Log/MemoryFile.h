#ifndef BERT_MEMORYFILE_H
#define BERT_MEMORYFILE_H

#include <string>

class MemoryFile
{
public:
    MemoryFile();
   ~MemoryFile();

    bool        Open(const std::string& file, bool bAppend = true);
    bool        Open(const char* file, bool bAppend = true);
    void        Close();

    void        Write(const void* data, std::size_t len); //  WriteHook

    bool        IsOpen() const { return m_file != kInvalidFile; }
    std::size_t Offset() const { return m_offset; }

    static bool MakeDir(const char* pDir);

    static const int   kInvalidFile = -1;
    static char* const kInvalidAddr;

private:
    bool            _ExtendFileSize(std::size_t  size);
    bool            _AssureSpace(std::size_t  size);

    int				m_file;
    char*           m_pMemory;
    std::size_t     m_offset;
    std::size_t     m_size;
};

#endif

